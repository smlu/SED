#include "ui/recovery/document_recovery_utils.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <limits>
#include <new>
#include <numeric>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

#include <QCryptographicHash>
#include <QBuffer>
#include <QCborArray>
#include <QCborMap>
#include <QCborStreamReader>
#include <QCborStreamWriter>
#include <QCborValue>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QScopeGuard>
#include <QUuid>

#include "sed/assets/sed.hpp"
#include "sed/level/thing.hpp"
#include "sed/text.hpp"
#include "ui/project/project_startup_utils.hpp"

namespace sed::ui::recovery
{
namespace
{

constexpr auto kRecoveryMagicV1 = "sed_recovery_v1\n";
constexpr auto kChunkMagic = "sedchn1\n";
constexpr auto kFecTableMagic = "sedfec1\n";
constexpr auto kFecFooterMagic = "fecend1\n";
constexpr qint64 kMaximumHeaderBytes = 1024 * 1024;
// Descriptor discovery is deliberately much tighter than full recovery
// loading.  A canonical META is generated from the <= 1 MiB primary JSON and
// its one protected section is comfortably below this bound even for
// incompressible input.  Keeping the bound independent of DOCM prevents a
// directory scan from allocating attacker-sized recovery payloads.
constexpr quint64 kMaximumMetadataProtectedBytes = 4ull * 1024ull * 1024ull;
constexpr qsizetype kMaximumMetadataSessionIdCharacters = 128;
constexpr qsizetype kMaximumMetadataDisplayNameCharacters = 512;
constexpr qsizetype kMaximumMetadataSourcePathCharacters = 32'768;
constexpr qsizetype kMaximumMetadataIdentityCharacters =
    kMaximumMetadataSourcePathCharacters + 16;
constexpr qint64 kMaximumMetadataScanBytes = kMaximumHeaderBytes
    + static_cast<qint64>(kMaximumMetadataProtectedBytes) + 64 * 1024;
// No outer chunk may exceed the complete native/recovery file ceiling. More
// restrictive type-specific limits below are checked before payload copying or
// qUncompress so a tiny stored value cannot request a large expansion.
constexpr quint64 kMaximumChunkBytes = assets::SedMaximumFileBytes;
constexpr quint64 kMaximumWarningBytes = 1ull * 1024ull * 1024ull;
// Full recovery validation stages the complete container before native
// decoding. Share the native complete-file ceiling rather than admitting a
// multi-gigabyte allocation merely because individual chunk arithmetic is
// representable.
constexpr quint64 kMaximumContainerBytes = assets::SedMaximumFileBytes;
constexpr int kMaximumOuterBoundaryCandidates = 64;
constexpr int kMaximumOuterBoundaryMarkerProbes = 8192;
constexpr int kMaximumDamagedDocumentHeaderCandidates = 64;
constexpr int kMaximumSedSignatureFallbackCandidates = 64;
constexpr int kMaximumSectionAnchorCandidates = 128;
constexpr std::size_t kMaximumRecoveryScanCandidates = 256;
constexpr std::size_t kMaximumRecoveryDirectoryEntriesScanned = 4096;
// One supported family contains one live slot plus at most ten history
// points. Leave ample room for failed-prune duplicates without allowing a
// hostile exact-digest filename flood to multiply 4 MiB META validation by
// the general directory-entry ceiling.
constexpr std::size_t kMaximumRecoveryFamilyEntriesScanned = 64;
constexpr quint64 kNativeFallbackWorkMultiplier = 2;
constexpr int kMaximumNativeFallbackCandidates =
    kMaximumDamagedDocumentHeaderCandidates
    + kMaximumSedSignatureFallbackCandidates
    + kMaximumSectionAnchorCandidates + 1;
constexpr quint32 kChunkCompressed = 1u;
constexpr quint32 kChunkProtected = 2u;
// `docm` is already a self-describing native SED document whose semantic
// sections own integrity and recovery. This flag makes the intentionally zero
// outer CRC/SHA fields unambiguous without changing the scan-friendly header.
constexpr quint32 kChunkNativeSed = 4u;
constexpr int kChunkHeaderBytes = 8 + 4 + 4 + 8 + 8 + 4 + 32;
constexpr quint32 kFecDataShards = 16;
constexpr quint32 kFecParityShards = 2;
constexpr quint64 kRecoveryCborMaterializationInputMultiplier = 32;
constexpr quint64 kRecoveryCborMaterializationMinimumBytes = 1ull * 1024ull * 1024ull;
constexpr quint64 kRecoveryCborNodeMaterializationBytes = 64;

// Scoped test instrumentation counts native outer-DOCM decode attempts. It is
// thread-local so parallel tests and production recovery workers cannot
// interfere, and the public recovery API has no persistent mutable hook.
thread_local std::uint64_t* gNativeDocumentDecodeCounterForTesting = nullptr;

void noteNativeDocumentDecodeForTesting() noexcept
{
    if (gNativeDocumentDecodeCounterForTesting)
    {
        ++*gNativeDocumentDecodeCounterForTesting;
    }
}

class ScopedNativeDocumentDecodeCounter final
{
public:
    explicit ScopedNativeDocumentDecodeCounter(std::uint64_t* counter) noexcept
        : previous_(gNativeDocumentDecodeCounterForTesting)
    {
        gNativeDocumentDecodeCounterForTesting = counter;
    }

    ~ScopedNativeDocumentDecodeCounter()
    {
        gNativeDocumentDecodeCounterForTesting = previous_;
    }

    ScopedNativeDocumentDecodeCounter(
        const ScopedNativeDocumentDecodeCounter&) = delete;
    ScopedNativeDocumentDecodeCounter& operator=(
        const ScopedNativeDocumentDecodeCounter&) = delete;

private:
    std::uint64_t* previous_{};
};

struct NativeFallbackBudget
{
    quint64 remainingBytes{};
    int remainingCandidates{};
    quint64 chargedBytes{};
    quint64 rejectedBytes{};
    bool exhausted{};

    bool tryConsume(const quint64 bytes)
    {
        if (bytes == 0 || remainingCandidates <= 0 || bytes > remainingBytes)
        {
            rejectedBytes = bytes > std::numeric_limits<quint64>::max()
                    - rejectedBytes
                ? std::numeric_limits<quint64>::max()
                : rejectedBytes + bytes;
            exhausted = true;
            return false;
        }
        --remainingCandidates;
        remainingBytes -= bytes;
        chargedBytes += bytes;
        return true;
    }
};

NativeFallbackBudget makeNativeFallbackBudget(const quint64 inputBytes)
{
    const quint64 maximumBytes = inputBytes
        > std::numeric_limits<quint64>::max() / kNativeFallbackWorkMultiplier
        ? std::numeric_limits<quint64>::max()
        : inputBytes * kNativeFallbackWorkMultiplier;
    return NativeFallbackBudget{
        maximumBytes,
        kMaximumNativeFallbackCandidates,
    };
}

bool preflightRecoveryCbor(
    const QByteArray& payload,
    const RecoveryCborLimits& limits)
{
    if (payload.isEmpty() || limits.maximumInputBytes == 0
        || limits.maximumNodes == 0 || limits.maximumDepth <= 0
        || limits.maximumMaterializedBytes == 0
        || static_cast<quint64>(payload.size()) > limits.maximumInputBytes)
    {
        return false;
    }

    QCborStreamReader reader(payload);
    quint64 nodeCount{};
    const quint64 inputBytes = static_cast<quint64>(payload.size());
    const quint64 scaledMaterializationBytes = inputBytes
        > std::numeric_limits<quint64>::max()
            / kRecoveryCborMaterializationInputMultiplier
        ? std::numeric_limits<quint64>::max()
        : inputBytes * kRecoveryCborMaterializationInputMultiplier;
    const quint64 materializationBudget = std::min(
        limits.maximumMaterializedBytes,
        std::max(kRecoveryCborMaterializationMinimumBytes,
            scaledMaterializationBytes));
    // Canonical verification emits one input-sized QByteArray while the
    // decoded graph is still live. Account for it before visiting any node.
    quint64 materializedBytes = inputBytes;
    const auto chargeMaterialization = [&](const quint64 bytes)
    {
        if (materializedBytes > materializationBudget
            || bytes > materializationBudget - materializedBytes)
        {
            return false;
        }
        materializedBytes += bytes;
        return true;
    };
    if (materializedBytes > materializationBudget) return false;
    const auto visit = [&](auto&& self, const int depth) -> bool
    {
        if (depth > limits.maximumDepth || nodeCount >= limits.maximumNodes)
        {
            return false;
        }
        ++nodeCount;
        if (!chargeMaterialization(kRecoveryCborNodeMaterializationBytes))
        {
            return false;
        }

        if (reader.isContainer())
        {
            if (!reader.isLengthKnown()) return false;
            const bool isMap = reader.isMap();
            const quint64 itemCount = reader.length();
            quint64 childCount = itemCount;
            if (isMap)
            {
                if (childCount > limits.maximumNodes / 2u) return false;
                childCount *= 2u;
            }
            if (childCount > limits.maximumNodes - nodeCount
                || depth == limits.maximumDepth
                || !reader.enterContainer())
            {
                return false;
            }
            if (isMap)
            {
                QByteArrayView previousKey{};
                bool havePreviousKey{};
                for (quint64 pair = 0; pair < itemCount; ++pair)
                {
                    // Recovery schemas use bounded text keys exclusively.
                    // Capturing their already-bounded raw encodings lets us
                    // enforce canonical map order without materializing or
                    // cloning the decoded graph.
                    if (!reader.hasNext() || !reader.isString()) return false;
                    const qsizetype keyStart = reader.currentOffset();
                    if (!self(self, depth + 1)) return false;
                    const qsizetype keyEnd = reader.currentOffset();
                    if (keyStart < 0 || keyEnd <= keyStart
                        || keyEnd > payload.size())
                    {
                        return false;
                    }
                    const QByteArrayView key(
                        payload.constData() + keyStart, keyEnd - keyStart);
                    const bool orderedAfterPrevious = !havePreviousKey
                        || previousKey.size() < key.size()
                        || (previousKey.size() == key.size()
                            && std::lexicographical_compare(
                                previousKey.begin(), previousKey.end(),
                                key.begin(), key.end(),
                                [](const char lhs, const char rhs)
                                {
                                    return static_cast<unsigned char>(lhs)
                                        < static_cast<unsigned char>(rhs);
                                }));
                    if (!orderedAfterPrevious) return false;
                    previousKey = key;
                    havePreviousKey = true;

                    if (!reader.hasNext() || !self(self, depth + 1)) return false;
                }
                return reader.lastError() == QCborError::NoError
                    && !reader.hasNext() && reader.leaveContainer();
            }
            while (reader.lastError() == QCborError::NoError && reader.hasNext())
            {
                if (!self(self, depth + 1)) return false;
            }
            return reader.lastError() == QCborError::NoError
                && reader.leaveContainer();
        }

        if (reader.isString() || reader.isByteArray())
        {
            if (!reader.isLengthKnown()
                || reader.length() > static_cast<quint64>(payload.size())
                || (reader.isString()
                    && reader.length() > limits.maximumTextBytes)
                || (reader.isByteArray()
                    && reader.length() > limits.maximumByteStringBytes))
            {
                return false;
            }
            const quint64 encodedBytes = reader.length();
            const quint64 decodedBytes = reader.isString()
                ? (encodedBytes > std::numeric_limits<quint64>::max() / 2u
                    ? std::numeric_limits<quint64>::max()
                    : encodedBytes * 2u)
                : encodedBytes;
            if (!chargeMaterialization(decodedBytes)) return false;
            std::array<char, 4096> scratch{};
            QCborStreamReader::StringResult<qsizetype> result{};
            do
            {
                result = reader.readStringChunk(
                    scratch.data(), static_cast<qsizetype>(scratch.size()));
            }
            while (result.status == QCborStreamReader::Ok);
            return result.status == QCborStreamReader::EndOfString
                && reader.lastError() == QCborError::NoError;
        }
        if (reader.isTag() || reader.isInvalid()) return false;
        return reader.next(limits.maximumDepth);
    };

    return reader.hasNext() && visit(visit, 0)
        && reader.lastError() == QCborError::NoError
        && reader.containerDepth() == 0 && !reader.hasNext()
        && reader.currentOffset() == payload.size();
}

constexpr QCborValue::EncodingOptions kRecoveryCborEncodingOptions =
    QCborValue::SortKeysInMaps;

void writeCanonicalRecoveryCbor(
    QCborStreamWriter& writer,
    const QCborValue& value)
{
    if (value.isArray())
    {
        const QCborArray array = value.toArray();
        writer.startArray(static_cast<quint64>(array.size()));
        for (const QCborValue& child : array)
        {
            writeCanonicalRecoveryCbor(writer, child);
        }
        (void)writer.endArray();
        return;
    }
    if (value.isMap())
    {
        const QCborMap map = value.toMap();
        std::vector<QCborMap::ConstIterator> entries{};
        entries.reserve(static_cast<std::size_t>(map.size()));
        for (auto it = map.constBegin(); it != map.constEnd(); ++it)
        {
            if (!it.key().isString())
            {
                throw std::runtime_error(
                    "Recovery CBOR maps require text keys.");
            }
            entries.push_back(it);
        }
        std::stable_sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs)
        {
            return lhs.key().compare(rhs.key()) < 0;
        });
        for (std::size_t index = 1; index < entries.size(); ++index)
        {
            if (entries[index - 1u].key().compare(entries[index].key()) == 0)
            {
                throw std::runtime_error(
                    "Recovery CBOR maps require unique keys.");
            }
        }

        writer.startMap(static_cast<quint64>(entries.size()));
        for (const auto& entry : entries)
        {
            writeCanonicalRecoveryCbor(writer, entry.key());
            writeCanonicalRecoveryCbor(writer, entry.value());
        }
        (void)writer.endMap();
        return;
    }

    if (value.isTag() || value.isInvalid())
    {
        throw std::runtime_error(
            "Recovery CBOR does not permit tags or invalid values.");
    }

    // Keep the encoding policy explicit for scalar values. Containers are
    // streamed above so nested maps can be sorted without cloning the graph.
    value.toCbor(writer, kRecoveryCborEncodingOptions);
}

bool recoveryChunkSizesWithinAdmissionLimits(
    const QByteArray& type,
    const quint64 storedSize,
    const quint64 rawSize)
{
    if (type == QByteArrayLiteral("meta"))
    {
        return storedSize <= kMaximumMetadataProtectedBytes
            && rawSize <= static_cast<quint64>(kMaximumHeaderBytes);
    }
    if (type == QByteArrayLiteral("docm"))
    {
        return storedSize == rawSize && rawSize <= assets::SedMaximumFileBytes;
    }
    if (type == QByteArrayLiteral("warn"))
    {
        return storedSize <= kMaximumWarningBytes
            && rawSize <= kMaximumWarningBytes;
    }
    return false;
}

QString normalizedMetadataSourcePath(const QString& sourcePath)
{
    if (sourcePath.trimmed().isEmpty()) return {};
    return normalizedSourcePath(
        std::filesystem::path(sourcePath.toStdWString()));
}

bool recoveryMetadataFieldsWithinLimits(const RecoveryMetadata& metadata)
{
    const QString identity = metadata.identity.trimmed();
    const QString sessionId = metadata.sessionId.trimmed();
    const QString displayName = metadata.displayName.trimmed();
    if (identity.size() > kMaximumMetadataIdentityCharacters
        || sessionId.size() > kMaximumMetadataSessionIdCharacters
        || metadata.sourcePath.size() > kMaximumMetadataSourcePathCharacters
        || displayName.size() > kMaximumMetadataDisplayNameCharacters)
    {
        return false;
    }

    // Empty display names are normalized on read. Bound that derived value as
    // well so a maximum-length synthetic source path cannot retain another
    // unbounded QString in every descriptor discovered by scanRecoveries.
    const QString effectiveDisplayName = displayName.isEmpty()
        ? (metadata.sourcePath.isEmpty()
            ? QStringLiteral("Untitled")
            : QFileInfo(metadata.sourcePath).fileName())
        : displayName;
    return effectiveDisplayName.size() <= kMaximumMetadataDisplayNameCharacters;
}

bool recoveryMetadataEquivalent(
    const RecoveryMetadata& lhs,
    const RecoveryMetadata& rhs)
{
    return lhs.identity == rhs.identity
        && lhs.sessionId == rhs.sessionId
        && normalizedMetadataSourcePath(lhs.sourcePath)
            == normalizedMetadataSourcePath(rhs.sourcePath)
        && lhs.displayName == rhs.displayName
        && lhs.timestamp == rhs.timestamp
        && lhs.sourceModifiedAtMs == rhs.sourceModifiedAtMs
        && lhs.undoSignature == rhs.undoSignature
        && lhs.requiresUserRecovery == rhs.requiresUserRecovery
        && lhs.projectType == rhs.projectType
        && lhs.levelSha256 == rhs.levelSha256
        && lhs.documentSha256 == rhs.documentSha256;
}

bool recoveryMetadataAssociationCoherent(const RecoveryMetadata& metadata)
{
    if (!recoveryMetadataFieldsWithinLimits(metadata)) return false;
    const QString sessionId = metadata.sessionId.trimmed();
    if (sessionId.isEmpty()) return false;
    const QString normalizedSource = normalizedMetadataSourcePath(
        metadata.sourcePath);
    if (normalizedSource.size() > kMaximumMetadataIdentityCharacters - 5)
    {
        return false;
    }
    const QString expectedIdentity = normalizedSource.isEmpty()
        ? QStringLiteral("untitled:%1").arg(sessionId)
        : QStringLiteral("file:%1").arg(normalizedSource);
    return metadata.identity == expectedIdentity;
}

QJsonObject metadataToJson(const RecoveryMetadata& metadata, const int version)
{
    QJsonObject object{};
    object.insert(QStringLiteral("version"), version);
    object.insert(QStringLiteral("identity"), metadata.identity);
    object.insert(QStringLiteral("sessionId"), metadata.sessionId);
    object.insert(QStringLiteral("sourcePath"), metadata.sourcePath);
    object.insert(QStringLiteral("displayName"), metadata.displayName);
    object.insert(QStringLiteral("timestamp"), QString::number(metadata.timestamp));
    object.insert(QStringLiteral("sourceModifiedAtMs"), QString::number(metadata.sourceModifiedAtMs));
    object.insert(QStringLiteral("undoSignature"), QString::number(metadata.undoSignature));
    object.insert(QStringLiteral("requiresUserRecovery"), metadata.requiresUserRecovery);
    object.insert(QStringLiteral("projectType"), static_cast<int>(metadata.projectType));
    object.insert(QStringLiteral("levelSha256"), QString::fromLatin1(metadata.levelSha256.toHex()));
    object.insert(QStringLiteral("documentSha256"), QString::fromLatin1(metadata.documentSha256.toHex()));
    object.insert(QStringLiteral("payloadFormat"),
        QStringLiteral("sed-v1-document-recovery"));
    return object;
}

std::optional<RecoveryMetadata> metadataFromJson(
    const QByteArray& json,
    int* outVersion,
    QString* error)
{
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        if (error)
        {
            *error = QStringLiteral("Invalid recovery metadata: %1").arg(parseError.errorString());
        }
        return std::nullopt;
    }

    const QJsonObject object = document.object();
    const int version = object.value(QStringLiteral("version")).toInt();
    const QString payloadFormat = object.value(QStringLiteral("payloadFormat")).toString();
    if (version != 1
        || payloadFormat != QStringLiteral("sed-v1-document-recovery"))
    {
        if (error)
        {
            *error = QStringLiteral("Unsupported recovery file version or payload format.");
        }
        return std::nullopt;
    }

    if (outVersion)
    {
        *outVersion = version;
    }

    RecoveryMetadata metadata{};
    metadata.identity = object.value(QStringLiteral("identity")).toString().trimmed();
    metadata.sessionId = object.value(QStringLiteral("sessionId")).toString().trimmed();
    const QJsonValue sourcePathValue = object.value(QStringLiteral("sourcePath"));
    const QJsonValue displayNameValue = object.value(QStringLiteral("displayName"));
    if (!sourcePathValue.isString() || !displayNameValue.isString())
    {
        if (error)
        {
            *error = QStringLiteral("Recovery metadata is incomplete or malformed.");
        }
        return std::nullopt;
    }
    metadata.sourcePath = sourcePathValue.toString();
    metadata.displayName = displayNameValue.toString().trimmed();
    if (!recoveryMetadataFieldsWithinLimits(metadata))
    {
        if (error)
        {
            *error = QStringLiteral(
                "Recovery metadata identity, session, source path, or display name exceeds its bounded field limit.");
        }
        return std::nullopt;
    }

    bool timestampOk = false;
    metadata.timestamp = object.value(QStringLiteral("timestamp")).toString().toLongLong(&timestampOk);
    bool sourceModifiedOk = false;
    metadata.sourceModifiedAtMs = object.value(QStringLiteral("sourceModifiedAtMs")).toString().toLongLong(&sourceModifiedOk);
    bool signatureOk = false;
    metadata.undoSignature = object.value(QStringLiteral("undoSignature")).toString().toULongLong(&signatureOk);
    const QJsonValue requiresUserRecovery =
        object.value(QStringLiteral("requiresUserRecovery"));
    const int projectType = object.value(QStringLiteral("projectType")).toInt(-1);
    const QString levelSha256Hex = object.value(QStringLiteral("levelSha256")).toString();
    const QString documentSha256Hex = object.value(QStringLiteral("documentSha256")).toString();
    const QByteArray levelSha256 = QByteArray::fromHex(levelSha256Hex.toLatin1());
    const QByteArray documentSha256 = QByteArray::fromHex(documentSha256Hex.toLatin1());
    const auto canonicalHex = [](const QString& encoded, const QByteArray& decoded)
    {
        return encoded.size() == 64
            && encoded == QString::fromLatin1(decoded.toHex());
    };

    if (metadata.identity.isEmpty() || !timestampOk || metadata.timestamp <= 0
        || !sourceModifiedOk || metadata.sourceModifiedAtMs < -1
        || !signatureOk || !requiresUserRecovery.isBool()
        || projectType < static_cast<int>(ProjectType::JKDF2)
        || projectType > static_cast<int>(ProjectType::IJIM)
        || levelSha256.size() != QCryptographicHash::hashLength(QCryptographicHash::Sha256)
        || documentSha256.size() != QCryptographicHash::hashLength(QCryptographicHash::Sha256)
        || !canonicalHex(levelSha256Hex, levelSha256)
        || !canonicalHex(documentSha256Hex, documentSha256))
    {
        if (error)
        {
            *error = QStringLiteral("Recovery metadata is incomplete or malformed.");
        }
        return std::nullopt;
    }

    metadata.projectType = static_cast<ProjectType>(projectType);
    metadata.requiresUserRecovery = requiresUserRecovery.toBool();
    metadata.projectTypeTrust = ProjectTypeTrust::metadata;
    metadata.levelSha256 = levelSha256;
    metadata.documentSha256 = documentSha256;
    if (metadata.displayName.isEmpty())
    {
        metadata.displayName = metadata.sourcePath.isEmpty()
            ? QStringLiteral("Untitled")
            : QFileInfo(metadata.sourcePath).fileName();
    }
    return metadata;
}

QByteArray serializeLevelToSed(
    const Level& level,
    const std::span<const assets::SedOptionalSection> optionalSections = {},
    const assets::SedSerializationProfile profile = assets::SedSerializationProfile::ProtectedV1,
    const std::int64_t timestamp = 0)
{
    const std::vector<std::byte> bytes = assets::saveToSedBytes(
        level, optionalSections, profile, timestamp);
    return QByteArray(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<qsizetype>(bytes.size()));
}

bool hasRecoverySedSignature(const QByteArray& payload)
{
    return assets::hasSedBinarySignature(std::as_bytes(std::span(
        payload.constData(), static_cast<std::size_t>(payload.size()))));
}

std::optional<Level> deserializeLevelFromSed(
    const QByteArray& payload,
    const ProjectType projectType,
    QString* error,
    const bool allowPartial = false,
    const assets::SedSerializationProfile profile =
        assets::SedSerializationProfile::ProtectedV1)
{
    if (allowPartial && profile != assets::SedSerializationProfile::ProtectedV1)
    {
        if (error)
        {
            *error = QStringLiteral("Partial recovery is only valid for protected top-level SED data.");
        }
        return std::nullopt;
    }
    try
    {
        const auto bytes = std::as_bytes(std::span(
            payload.constData(), static_cast<std::size_t>(payload.size())));
        Level level{};
        if (allowPartial)
        {
            // Keep the structured recovery result intact until project-kind
            // validation.  A damaged primary/mirror header can leave the
            // native recovery scanner with its conservative JKDF2 framing;
            // when the required HEAD unit is explicitly missing, the outer
            // recovery META is the trusted project discriminator.  A
            // surviving HEAD remains authoritative and decodeSedBytes already
            // rejects any HEAD/header disagreement before this point.
            assets::SedRecoveryLoadResult recovered =
                assets::recoverFromSedBytes(bytes);
            const bool headMissing = std::find(
                recovered.missingSections.begin(), recovered.missingSections.end(),
                std::string("head")) != recovered.missingSections.end();
            if (headMissing)
            {
                ProjectType resolvedProjectType = projectType;
                if (recovered.projectTypeTrusted && recovered.projectType)
                {
                    if (*recovered.projectType != projectType)
                    {
                        if (error)
                        {
                            *error = QStringLiteral(
                                "Recovery metadata and trusted native SED project type disagree.");
                        }
                        return std::nullopt;
                    }
                    resolvedProjectType = *recovered.projectType;
                }
                // HEAD was lost as one checked semantic domain. The caller
                // has established the authoritative project from native
                // framing, META, or an explicit user choice; replace only the
                // irrecoverable project/header fields with safe defaults.
                ui::project::applyNewProjectTypeDefaults(
                    recovered.level, resolvedProjectType);
            }
            level = std::move(recovered.level);
        }
        else
        {
            if (profile == assets::SedSerializationProfile::HistoryNested)
            {
                level = assets::loadHistoryNestedSedLevelBytes(bytes);
            }
            else if (profile
                == assets::SedSerializationProfile::UnprotectedNested)
            {
                level = assets::loadNestedSedLevelBytes(bytes);
            }
            else
            {
                level = assets::loadFromSedBytes(bytes);
            }
        }
        if (level.kind != projectType)
        {
            if (error)
            {
                *error = QStringLiteral("Recovery metadata and binary SED project type disagree.");
            }
            return std::nullopt;
        }
        return level;
    }
    catch (const std::bad_alloc&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        if (error)
        {
            *error = QString::fromUtf8(e.what());
        }
    }
    catch (...)
    {
        if (error)
        {
            *error = QStringLiteral("Unknown recovery parser failure.");
        }
    }
    return std::nullopt;
}

template <typename Set, typename Project>
QCborArray sortedSelectionArray(const Set& source, Project project)
{
    using Value = decltype(project(*source.begin()));
    std::vector<Value> values{};
    values.reserve(source.size());
    for (const auto& item : source)
    {
        values.push_back(project(item));
    }
    std::sort(values.begin(), values.end());

    QCborArray array{};
    for (const auto& value : values)
    {
        if constexpr (std::is_integral_v<Value>)
        {
            array.append(static_cast<qint64>(value));
        }
        else
        {
            QCborArray tuple{};
            for (const auto component : value)
            {
                tuple.append(static_cast<qint64>(component));
            }
            array.append(tuple);
        }
    }
    return array;
}

QCborMap selectionToCbor(const editor::Selection& selection)
{
    QCborMap map{};
    map.insert(QStringLiteral("sectors"), sortedSelectionArray(
        selection.sectors,
        [](const std::int32_t value) { return value; }));
    map.insert(QStringLiteral("things"), sortedSelectionArray(
        selection.things,
        [](const std::int32_t value) { return value; }));
    map.insert(QStringLiteral("lights"), sortedSelectionArray(
        selection.lights,
        [](const std::int32_t value) { return value; }));
    map.insert(QStringLiteral("cogs"), sortedSelectionArray(
        selection.cogs,
        [](const std::int32_t value) { return value; }));
    map.insert(QStringLiteral("surfaces"), sortedSelectionArray(
        selection.surfaces,
        [](const SurfaceRef& ref)
        {
            return std::array<std::int32_t, 2>{ ref.sector, ref.surface };
        }));
    map.insert(QStringLiteral("vertices"), sortedSelectionArray(
        selection.surfaceVertices,
        [](const editor::SurfaceVertexRef& ref)
        {
            return std::array<std::int32_t, 3>{
                ref.surface.sector, ref.surface.surface, ref.localVertex };
        }));
    map.insert(QStringLiteral("frames"), sortedSelectionArray(
        selection.frames,
        [](const level::FrameRef& ref)
        {
            return std::array<std::int32_t, 2>{ ref.thing, ref.value };
        }));
    return map;
}

bool cborIntegerArray(
    const QCborValue& value,
    std::vector<std::int32_t>* out,
    const qsizetype maximum = 10'000'000)
{
    if (!out || !value.isArray())
    {
        return false;
    }
    const QCborArray array = value.toArray();
    if (array.size() > maximum)
    {
        return false;
    }
    out->clear();
    out->reserve(static_cast<std::size_t>(array.size()));
    std::int32_t previous{};
    bool havePrevious = false;
    for (const QCborValue& item : array)
    {
        if (!item.isInteger())
        {
            return false;
        }
        const qint64 integer = item.toInteger();
        if (integer < std::numeric_limits<std::int32_t>::min()
            || integer > std::numeric_limits<std::int32_t>::max())
        {
            return false;
        }
        const auto converted = static_cast<std::int32_t>(integer);
        // selectionToCbor emits sorted sets.  Reject duplicates and unsorted
        // arrays instead of silently normalizing malformed input through a
        // std::set during decode.
        if (havePrevious && converted <= previous)
        {
            return false;
        }
        previous = converted;
        havePrevious = true;
        out->push_back(converted);
    }
    return true;
}

std::optional<editor::Selection> selectionFromCbor(const QCborValue& value)
{
    if (!value.isMap())
    {
        return std::nullopt;
    }
    const QCborMap map = value.toMap();
    // Selection is a fixed part of the V1 manifest.  Rejecting unknown keys
    // keeps the CBOR representation canonical and prevents an apparently
    // valid history entry from smuggling unvalidated editor data through the
    // nested Level integrity boundary.
    if (map.size() != 7)
    {
        return std::nullopt;
    }
    editor::Selection selection{};
    std::vector<std::int32_t> integers{};
    const auto readSet = [&](const QString& key, auto& target) -> bool
    {
        if (!cborIntegerArray(map.value(key), &integers))
        {
            return false;
        }
        target.insert(integers.begin(), integers.end());
        return true;
    };
    if (!readSet(QStringLiteral("sectors"), selection.sectors)
        || !readSet(QStringLiteral("things"), selection.things)
        || !readSet(QStringLiteral("lights"), selection.lights)
        || !readSet(QStringLiteral("cogs"), selection.cogs))
    {
        return std::nullopt;
    }

    const auto readTuples = [&](const QString& key, const int tupleSize, auto append) -> bool
    {
        const QCborValue tupleValue = map.value(key);
        if (!tupleValue.isArray())
        {
            return false;
        }
        const QCborArray tuples = tupleValue.toArray();
        if (tuples.size() > 10'000'000)
        {
            return false;
        }
        std::array<std::int32_t, 3> previous{};
        bool havePrevious = false;
        for (const QCborValue& item : tuples)
        {
            if (!item.isArray())
            {
                return false;
            }
            const QCborArray tuple = item.toArray();
            if (tuple.size() != tupleSize)
            {
                return false;
            }
            std::array<std::int32_t, 3> values{};
            for (int i = 0; i < tupleSize; ++i)
            {
                if (!tuple.at(i).isInteger())
                {
                    return false;
                }
                const qint64 integer = tuple.at(i).toInteger();
                if (integer < std::numeric_limits<std::int32_t>::min()
                    || integer > std::numeric_limits<std::int32_t>::max())
                {
                    return false;
                }
                values[static_cast<std::size_t>(i)] = static_cast<std::int32_t>(integer);
            }
            if (havePrevious)
            {
                bool greater = false;
                for (int i = 0; i < tupleSize; ++i)
                {
                    if (values[static_cast<std::size_t>(i)]
                        != previous[static_cast<std::size_t>(i)])
                    {
                        greater = values[static_cast<std::size_t>(i)]
                            > previous[static_cast<std::size_t>(i)];
                        break;
                    }
                }
                if (!greater)
                {
                    return false;
                }
            }
            std::copy_n(values.begin(), tupleSize, previous.begin());
            havePrevious = true;
            append(values);
        }
        return true;
    };

    if (!readTuples(QStringLiteral("surfaces"), 2, [&](const auto& tuple)
        {
            selection.surfaces.insert(SurfaceRef{ tuple[0], tuple[1] });
        })
        || !readTuples(QStringLiteral("vertices"), 3, [&](const auto& tuple)
        {
            selection.surfaceVertices.insert(editor::SurfaceVertexRef{
                SurfaceRef{ tuple[0], tuple[1] }, tuple[2] });
        })
        || !readTuples(QStringLiteral("frames"), 2, [&](const auto& tuple)
        {
            selection.frames.insert(level::FrameRef{ tuple[0], tuple[1] });
        }))
    {
        return std::nullopt;
    }
    return selection;
}

bool selectionReferencesLevel(
    const editor::Selection& selection,
    const Level& level) noexcept
{
    const auto validIndex = [](const std::int32_t index, const std::size_t count)
    {
        return index >= 0 && static_cast<std::size_t>(index) < count;
    };
    const auto allIndicesValid = [&](const auto& indices, const std::size_t count)
    {
        return std::all_of(indices.begin(), indices.end(),
            [&](const std::int32_t index) { return validIndex(index, count); });
    };
    const auto validSurface = [&](const SurfaceRef ref)
    {
        return validIndex(ref.sector, level.sectors.size())
            && validIndex(ref.surface,
                level.sectors[static_cast<std::size_t>(ref.sector)].surfaces.size());
    };

    if (!allIndicesValid(selection.sectors, level.sectors.size())
        || !allIndicesValid(selection.things, level.things.size())
        || !allIndicesValid(selection.lights, level.lights.size())
        || !allIndicesValid(selection.cogs, level.cogs.size())
        || !std::all_of(
            selection.surfaces.begin(), selection.surfaces.end(), validSurface))
    {
        return false;
    }

    const bool verticesValid = std::all_of(
        selection.surfaceVertices.begin(), selection.surfaceVertices.end(),
        [&](const editor::SurfaceVertexRef& ref)
        {
            return validSurface(ref.surface)
                && validIndex(ref.localVertex,
                    level.sectors[static_cast<std::size_t>(ref.surface.sector)]
                        .surfaces[static_cast<std::size_t>(ref.surface.surface)].vertices.size());
        });
    const bool framesValid = std::all_of(
        selection.frames.begin(), selection.frames.end(),
        [&](const level::FrameRef& ref)
        {
            if (!validIndex(ref.thing, level.things.size()))
            {
                return false;
            }
            const Thing& thing = level.things[static_cast<std::size_t>(ref.thing)];
            if (!validIndex(ref.value, thing.values.size()))
            {
                return false;
            }
            const TplValue& value = thing.values[static_cast<std::size_t>(ref.value)];
            return value.type == ValueType::frame
                && text::iequalsAscii(value.name, "frame");
        });
    return verticesValid && framesValid;
}

constexpr qsizetype kHistoryEntriesPerGroup = 32;
constexpr qsizetype kMaximumHistoryEntries = static_cast<qsizetype>(
    MaximumPersistedHistoryEntries);
constexpr qsizetype kMaximumHistoryPoolItems = 1'000'000;
constexpr qsizetype kMaximumHistoryFragmentsPerState = 300'000;
constexpr qsizetype kMaximumHistoryLabelCharacters = 1024 * 1024;
// A QString code unit needs at most three bytes in canonical UTF-8.  Every V1
// text value is either a fixed schema token/key or an entry label, so this is
// also a safe pre-materialization ceiling for an individual CBOR text item.
constexpr quint64 kMaximumHistoryTextBytes =
    3ull * static_cast<quint64>(kMaximumHistoryLabelCharacters);
constexpr qsizetype kHistoryOpaqueTailFragmentBytes = 64;
// HistoryNested's checked section descriptor is 100 bytes. Its expanded-size
// and CRC fields change whenever a section is structurally edited, so prefix
// matching begins after this header while the header stays in the bridge.
constexpr qsizetype kHistorySectionHeaderBytes = 100;
constexpr qsizetype kHistorySectionTrailerBytes = 116;
// HIST uses fixed offsets for its base/fallback layout rather than a variable
// boundary detector. Fixed offsets make an adjacent frame cheap to compare:
// an unchanged block can reuse its previous reference without hashing or
// interning it, while a changed block is the only byte range that enters the
// pool. Size-changing frames may carry writer-side shifted spans, but every
// span remains bounded by the same native history maximum so decoder
// preflight and the writer share one safety limit.
constexpr qsizetype kHistoryChunkMaximumBytes = 8 * 1024;
// A shifted-tail candidate is useful only when it has a meaningful exact
// suffix.  The complete tail is still byte-checked before any old reference
// is reused; this threshold merely avoids doing resynchronization work for a
// tiny coincidental match.
constexpr qsizetype kHistoryResynchronizationMinimumSuffixBytes = 64;
// HIST is an optional workspace stream.  Parse it only after this bound so
// QCborValue cannot first materialize an arbitrarily large attacker-controlled
// tree. The limit is intentionally below the 128 MiB native section ceiling:
// real editor histories are compact (the Level bytes are pooled/delta encoded),
// while a 64 MiB CBOR document already permits a very large undo stack.
static_assert(assets::SedMaximumHistorySectionBytes
    <= static_cast<std::uint64_t>(std::numeric_limits<qsizetype>::max()));
constexpr qsizetype kMaximumHistoryPayloadBytes = static_cast<qsizetype>(
    assets::SedMaximumHistorySectionBytes);
constexpr quint64 kMaximumHistoryCborNodes = 2'000'000;
constexpr int kMaximumHistoryCborDepth = 64;
constexpr std::size_t kMaximumJklCogImportStates =
    MaximumPersistedHistoryEntries;
constexpr std::uint64_t kMaximumJklCogImportReferences =
    assets::SedMaximumDecodedItems;
// HIST manifests are compact references, so a small input can otherwise ask
// the decoder to materialize the same large Level tens of thousands of times.
// Bound cumulative reconstruction work by both the encoded input and a hard
// process-wide ceiling. The floor preserves legitimate deep-history files
// whose highly deduplicated CBOR is only a few hundred KiB.
constexpr quint64 kMaximumHistoryReconstructionBytesAbsolute = 256ull * 1024ull * 1024ull;
// A decoded Level is still capped at SedMaximumDecodedObjectBytes.  Native
// history, however, intentionally keeps several structural checkpoints so
// undo can cross edit-mode boundaries.  The former aggregate default reused
// the one-Level cap and dropped otherwise valid test.sed-scale timelines.
// Keep a separate, explicit optional-history envelope; it is finite and is
// still far below any unbounded file/allocation allowance.
// The supplied 129-entry IJIM autosave retains about 518 MiB after exact
// compact reconstruction.  Keep a finite 768 MiB aggregate ceiling so that
// legitimate structural checkpoints survive, while the stricter 128 MiB
// per-Level, entry-count, CBOR, and reconstruction caps remain unchanged.
constexpr quint64 kMaximumHistoryRetainedLevelBytes = 768ull * 1024ull * 1024ull;
// Baked-light snapshots contain one compact ColorF item per texture vertex.
// A valid large IJIM timeline can therefore cross many millions of logical
// items while remaining below the independently enforced retained-storage
// ceiling.  The separate 32-million-item limit prevents unbounded logical
// collection growth without rejecting the validated real-world timeline.
constexpr quint64 kMaximumHistoryRetainedLevelItems = 32'000'000ull;

struct SparseJklCogImportState
{
    std::size_t index{};
    assets::SedJklCogImportState state{};
};

struct JklCogImportHistory
{
    assets::SedJklCogImportState current{};
    std::vector<SparseJklCogImportState> undo{};
    std::vector<SparseJklCogImportState> redo{};
};

assets::SedJklCogImportState representedJklCogImportState(
    const Level& level)
{
    if ( !assets::hasRepresentedJklCogImportState(level) )
    {
        return {};
    }
    return assets::SedJklCogImportState{
        level.importedJklCogRowsPresent,
        level.importedJklSurfaceRefs };
}

void addJklCogImportStateDigest(
    QCryptographicHash& hash,
    const assets::SedJklCogImportState& state)
{
    if ( !state.cogRowsPresent && state.surfaceRefs.empty() ) return;
    if ( state.surfaceRefs.size()
        > std::numeric_limits<std::uint32_t>::max() )
    {
        throw std::runtime_error(
            "JKL COG import state exceeds the canonical digest limit.");
    }

    // Preserve historical digests for ordinary Levels. Represented JKL COG
    // provenance gets a conditional, domain-separated little-endian suffix;
    // this binds marker-only and ordered-map changes without changing the
    // required HistoryNested frames or the empty-state V1 digest.
    constexpr char domain[] = "SED-JKL-COG-IMPORT-V1";
    hash.addData(domain, static_cast<qsizetype>(sizeof(domain) - 1u));
    const char marker = state.cogRowsPresent ? '\1' : '\0';
    hash.addData(&marker, 1);
    const auto addU32 = [&hash](const std::uint32_t value)
    {
        const std::array<char, 4> bytes{
            static_cast<char>(value & 0xffu),
            static_cast<char>((value >> 8u) & 0xffu),
            static_cast<char>((value >> 16u) & 0xffu),
            static_cast<char>((value >> 24u) & 0xffu),
        };
        hash.addData(bytes.data(), static_cast<qsizetype>(bytes.size()));
    };
    addU32(static_cast<std::uint32_t>(state.surfaceRefs.size()));
    for ( const SurfaceRef& reference : state.surfaceRefs )
    {
        addU32(static_cast<std::uint32_t>(reference.sector));
        addU32(static_cast<std::uint32_t>(reference.surface));
    }
}

void addJklCogImportStateDigest(
    QCryptographicHash& hash,
    const Level& level)
{
    addJklCogImportStateDigest(hash,
        representedJklCogImportState(level));
}

QByteArray canonicalLevelDigest(
    const QByteArray& canonicalLevelBytes,
    const Level& level)
{
    if ( canonicalLevelBytes.isEmpty() ) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(canonicalLevelBytes);
    addJklCogImportStateDigest(hash, level);
    return hash.result();
}

bool sameJklCogImportState(
    const assets::SedJklCogImportState& lhs,
    const assets::SedJklCogImportState& rhs) noexcept
{
    return lhs.cogRowsPresent == rhs.cogRowsPresent
        && lhs.surfaceRefs == rhs.surfaceRefs;
}

QCborArray jklCogImportStateToCbor(
    const assets::SedJklCogImportState& state)
{
    QCborArray encoded{};
    encoded.append(state.cogRowsPresent);
    QCborArray references{};
    for ( const SurfaceRef& reference : state.surfaceRefs )
    {
        QCborArray pair{};
        pair.append(reference.sector);
        pair.append(reference.surface);
        references.append(pair);
    }
    encoded.append(references);
    return encoded;
}

QCborArray sparseJklCogImportStatesToCbor(
    const std::vector<SparseJklCogImportState>& states)
{
    QCborArray encoded{};
    for ( const SparseJklCogImportState& entry : states )
    {
        QCborArray state{};
        state.append(static_cast<qint64>(entry.index));
        state.append(entry.state.cogRowsPresent);
        QCborArray references{};
        for ( const SurfaceRef& reference : entry.state.surfaceRefs )
        {
            QCborArray pair{};
            pair.append(reference.sector);
            pair.append(reference.surface);
            references.append(pair);
        }
        state.append(references);
        encoded.append(state);
    }
    return encoded;
}

bool decodeJklCogImportStateParts(
    const QCborValue& markerValue,
    const QCborValue& referencesValue,
    const bool requireMeaningful,
    std::uint64_t* const aggregateReferenceCount,
    assets::SedJklCogImportState* const state,
    QString* const error)
{
    const auto fail = [error](const QString& message)
    {
        if ( error ) *error = message;
        return false;
    };
    if ( !markerValue.isBool() || !referencesValue.isArray() )
    {
        return fail(QStringLiteral(
            "Recovery undo history JKL COG import state is malformed."));
    }
    const QCborArray references = referencesValue.toArray();
    const std::size_t referenceCount = static_cast<std::size_t>(
        references.size());
    if ( referenceCount > kMaximumJklCogImportReferences
        || *aggregateReferenceCount
            > kMaximumJklCogImportReferences - referenceCount )
    {
        return fail(QStringLiteral(
            "Recovery undo history JKL COG import references exceed the bounded item limit."));
    }
    const bool rowsPresent = markerValue.toBool();
    if ( requireMeaningful && !rowsPresent && references.isEmpty() )
    {
        return fail(QStringLiteral(
            "Recovery undo history contains an unrepresented sparse JKL COG import state."));
    }
    for ( const QCborValue& referenceValue : references )
    {
        if ( !referenceValue.isArray() )
        {
            return fail(QStringLiteral(
                "Recovery undo history JKL COG import reference is malformed."));
        }
        const QCborArray pair = referenceValue.toArray();
        if ( pair.size() != 2 || !pair.at(0).isInteger()
            || !pair.at(1).isInteger() )
        {
            return fail(QStringLiteral(
                "Recovery undo history JKL COG import reference is malformed."));
        }
        const qint64 sector = pair.at(0).toInteger(
            std::numeric_limits<qint64>::min());
        const qint64 surface = pair.at(1).toInteger(
            std::numeric_limits<qint64>::min());
        if ( sector < std::numeric_limits<std::int32_t>::min()
            || sector > std::numeric_limits<std::int32_t>::max()
            || surface < std::numeric_limits<std::int32_t>::min()
            || surface > std::numeric_limits<std::int32_t>::max() )
        {
            return fail(QStringLiteral(
                "Recovery undo history JKL COG import reference exceeds the supported integer range."));
        }
    }

    assets::SedJklCogImportState decoded{};
    decoded.cogRowsPresent = rowsPresent;
    decoded.surfaceRefs.reserve(referenceCount);
    for ( const QCborValue& referenceValue : references )
    {
        const QCborArray pair = referenceValue.toArray();
        decoded.surfaceRefs.push_back(SurfaceRef{
            static_cast<std::int32_t>(pair.at(0).toInteger()),
            static_cast<std::int32_t>(pair.at(1).toInteger()) });
    }
    *aggregateReferenceCount += referenceCount;
    *state = std::move(decoded);
    return true;
}

bool decodeSparseJklCogImportStates(
    const QCborValue& value,
    std::vector<SparseJklCogImportState>* const states,
    std::uint64_t* const aggregateReferenceCount,
    QString* const error)
{
    const auto fail = [error](const QString& message)
    {
        if ( error ) *error = message;
        return false;
    };
    if ( !value.isArray() )
    {
        return fail(QStringLiteral(
            "Recovery undo history JKL COG import stack is malformed."));
    }
    const QCborArray entries = value.toArray();
    if ( static_cast<std::size_t>(entries.size()) > kMaximumJklCogImportStates )
    {
        return fail(QStringLiteral(
            "Recovery undo history JKL COG import stack exceeds the entry limit."));
    }
    std::size_t previousIndex{};
    bool hasPrevious{};
    states->reserve(static_cast<std::size_t>(entries.size()));
    for ( const QCborValue& entryValue : entries )
    {
        if ( !entryValue.isArray() )
        {
            return fail(QStringLiteral(
                "Recovery undo history JKL COG import stack entry is malformed."));
        }
        const QCborArray entry = entryValue.toArray();
        if ( entry.size() != 3 || !entry.at(0).isInteger() )
        {
            return fail(QStringLiteral(
                "Recovery undo history JKL COG import stack entry is malformed."));
        }
        const qint64 encodedIndex = entry.at(0).toInteger(-1);
        if ( encodedIndex < 0
            || encodedIndex >= static_cast<qint64>(kMaximumJklCogImportStates)
            || (hasPrevious
                && static_cast<std::size_t>(encodedIndex) <= previousIndex) )
        {
            return fail(QStringLiteral(
                "Recovery undo history JKL COG import stack indices are not ascending and unique."));
        }
        SparseJklCogImportState decoded{};
        decoded.index = static_cast<std::size_t>(encodedIndex);
        if ( !decodeJklCogImportStateParts(
                entry.at(1), entry.at(2), true,
                aggregateReferenceCount, &decoded.state, error) )
        {
            return false;
        }
        states->push_back(std::move(decoded));
        previousIndex = static_cast<std::size_t>(encodedIndex);
        hasPrevious = true;
    }
    return true;
}

bool decodeJklCogImportHistory(
    const QCborValue& value,
    JklCogImportHistory* const history,
    QString* const error)
{
    const auto fail = [error](const QString& message)
    {
        if ( error ) *error = message;
        return false;
    };
    if ( !value.isMap() )
    {
        return fail(QStringLiteral(
            "Recovery undo history JKL COG import extension is malformed."));
    }
    const QCborMap extension = value.toMap();
    if ( extension.size() != 4
        || !extension.contains(QStringLiteral("current"))
        || !extension.contains(QStringLiteral("undo"))
        || !extension.contains(QStringLiteral("redo"))
        || !extension.value(QStringLiteral("version")).isInteger()
        || extension.value(QStringLiteral("version")).toInteger(-1) != 1 )
    {
        return fail(QStringLiteral(
            "Recovery undo history JKL COG import extension version or fields are unsupported."));
    }
    std::uint64_t aggregateReferenceCount{};
    const QCborValue currentStateValue =
        extension.value(QStringLiteral("current"));
    if ( !currentStateValue.isArray()
        || currentStateValue.toArray().size() != 2 )
    {
        return fail(QStringLiteral(
            "Recovery undo history JKL COG current state is malformed."));
    }
    const QCborArray currentState = currentStateValue.toArray();
    if ( !decodeJklCogImportStateParts(
            currentState.at(0), currentState.at(1),
            false, &aggregateReferenceCount, &history->current, error) )
    {
        return false;
    }
    if ( !decodeSparseJklCogImportStates(
            extension.value(QStringLiteral("undo")), &history->undo,
            &aggregateReferenceCount, error)
        || !decodeSparseJklCogImportStates(
            extension.value(QStringLiteral("redo")), &history->redo,
            &aggregateReferenceCount, error) )
    {
        return false;
    }
    const bool meaningfulCurrent = history->current.cogRowsPresent
        || !history->current.surfaceRefs.empty();
    if ( !meaningfulCurrent && history->undo.empty() && history->redo.empty() )
    {
        return fail(QStringLiteral(
            "Recovery undo history contains an empty JKL COG import extension."));
    }
    return true;
}

bool installJklCogImportState(
    Level& level,
    assets::SedJklCogImportState state,
    const bool requireMeaningful,
    QString* const error)
{
    level.importedJklCogRowsPresent = state.cogRowsPresent;
    level.importedJklSurfaceRefs = std::move(state.surfaceRefs);
    if ( !assets::hasRepresentedJklCogImportState(level) )
    {
        if ( requireMeaningful )
        {
            if ( error )
            {
                *error = QStringLiteral(
                    "Recovery undo history contains an unrepresented sparse JKL COG import state.");
            }
            return false;
        }
        level.importedJklCogRowsPresent = false;
        level.importedJklSurfaceRefs.clear();
        return true;
    }
    for ( const SurfaceRef& reference : level.importedJklSurfaceRefs )
    {
        if ( reference.sector < 0
            || static_cast<std::size_t>(reference.sector) >= level.sectors.size() )
        {
            if ( error )
            {
                *error = QStringLiteral(
                    "Recovery undo history JKL COG import sector reference is outside its Level.");
            }
            return false;
        }
        const Sector& sector = level.sectors[
            static_cast<std::size_t>(reference.sector)];
        if ( reference.surface < 0
            || static_cast<std::size_t>(reference.surface)
                >= sector.surfaces.size() )
        {
            if ( error )
            {
                *error = QStringLiteral(
                    "Recovery undo history JKL COG import Surface reference is outside its Level.");
            }
            return false;
        }
    }
    return true;
}
constexpr quint64 kMaximumHistoryPoolBytes =
    assets::SedMaximumHistorySectionBytes;
constexpr quint64 kHistoryReconstructionInputMultiplier = 256ull;
constexpr quint64 kHistoryReconstructionMinimumBytes = 32ull * 1024ull * 1024ull;
// A semantic Level is reconstructed from content-addressed fragments.  The
// old guard charged the complete expanded Level for every entry, even when
// 99% of those bytes were the same current-base fragments.  That made a
// legitimate mixed-mode history fail solely because it crossed an edit-mode
// boundary.  Charge each fragment's bytes once, then add a bounded per-entry
// floor so a hostile manifest cannot obtain unlimited CPU work by replaying a
// tiny reference list.  Per-state and per-file limits remain unchanged.
constexpr quint64 kHistoryPerStateReconstructionCharge = 256ull * 1024ull;
// The V1 history stream is deliberately a schema of its own.  The native
// Level sections remain the source of truth for every historical state; this
// layer only deduplicates their immutable semantic fragments and records
// bounded, versioned splice deltas between consecutive reference lists.
constexpr auto kHistoryEncoding = "sed-semantic-base-delta-v1";
constexpr auto kHistoryFragmentLayoutKey = "fragmentLayout";
constexpr auto kHistoryFixedFragmentLayout = "fixed-v1";
constexpr auto kHistorySectorRecordFragmentLayout = "sect-record-v1";
constexpr qsizetype kHistoryCheckpointPeriod = 16;
constexpr qint64 kHistoryRecordVersion = 1;
constexpr qint64 kHistoryRecordCheckpoint = 0;
constexpr qint64 kHistoryRecordDelta = 1;
// References in HIST share one namespace.  Non-negative values address the
// content pool; negative values address the canonical save-time Level base as
// `-1 - baseIndex`.  The base bytes are never copied into HIST.
using HistoryReference = qint64;

// The first released development HIST payload used fixed 8192-byte fragments
// and has no discriminator field.  New payloads carry the explicit layout
// marker so a reader never silently interprets old reference numbers with the
// record-oriented SECT base partition.
enum class HistoryFragmentLayout : std::uint8_t
{
    FixedBlocksV1,
    SectorRecordsV1,
};

class HistorySectorLayoutLimitExceeded final : public std::runtime_error
{
public:
    HistorySectorLayoutLimitExceeded()
        : std::runtime_error(
            "Recovery history SECT record layout exceeds the per-state fragment limit.")
    {
    }
};

quint64 historyReconstructionBudget(const qsizetype inputBytes)
{
    const quint64 encoded = inputBytes > 0
        ? static_cast<quint64>(inputBytes) : 1ull;
    const quint64 scaled = encoded > std::numeric_limits<quint64>::max()
        / kHistoryReconstructionInputMultiplier
        ? kMaximumHistoryReconstructionBytesAbsolute
        : encoded * kHistoryReconstructionInputMultiplier;
    return std::min(
        kMaximumHistoryReconstructionBytesAbsolute,
        std::max(kHistoryReconstructionMinimumBytes, scaled));
}

std::size_t historyPoolHash(const QByteArray& bytes) noexcept
{
    return std::hash<std::string_view>{}(std::string_view(
        bytes.constData(), static_cast<std::size_t>(bytes.size())));
}

// Forward declaration: the writer-side SECT splitter is defined below the
// fixed-fragment helper, but the persisted-layout-aware base decoder needs it
// while walking the semantic sections here.
std::vector<QByteArray> sectorHistoryFragmentBytes(
    const QByteArray& frame,
    std::size_t maximumFragments);

template <typename Append>
void forEachHistoryFixedBlock(const QByteArray& bytes, Append append)
{
    for (qsizetype offset{}; offset < bytes.size();)
    {
        const qsizetype length = std::min(
            kHistoryChunkMaximumBytes, bytes.size() - offset);
        append(bytes.mid(offset, length));
        offset += length;
    }
}

std::vector<QByteArray> historyContentFragments(
    const std::span<const std::byte> nestedBytes,
    const HistoryFragmentLayout layout = HistoryFragmentLayout::SectorRecordsV1)
{
    // nestedBytes was emitted immediately above by saveToSedBytes(). Avoid
    // decoding its complete Level again merely to recover writer-owned
    // framing boundaries; untrusted file/recovery input continues to use the
    // strict splitSedFileSections() path.
    const auto semanticFragments = assets::splitGeneratedSedFileSections(
        std::span<const std::byte>(nestedBytes),
        assets::SedSerializationProfile::HistoryNested);
    if (semanticFragments.empty())
    {
        throw std::runtime_error("Recovery history Level has an invalid semantic fragment count.");
    }

    std::vector<QByteArray> fragments{};
    for (std::size_t i = 0; i < semanticFragments.size(); ++i)
    {
        const auto& fragment = semanticFragments[i];
        const QByteArray bytes(
            reinterpret_cast<const char*>(fragment.data()),
            static_cast<qsizetype>(fragment.size()));
        const auto appendFragment = [&fragments](const QByteArray& value)
        {
            if (value.isEmpty()
                || static_cast<quint64>(value.size()) > kMaximumChunkBytes)
            {
                throw std::runtime_error("Recovery history fragment size is outside the safety limit.");
            }
            if (fragments.size()
                >= static_cast<std::size_t>(kMaximumHistoryFragmentsPerState))
            {
                throw HistorySectorLayoutLimitExceeded{};
            }
            fragments.push_back(value);
        };
        // splitSedFileSections deliberately appends opaque final framing to
        // its last semantic fragment. Isolating the fixed tail keeps native
        // framing independently deduplicable while preserving exact replay.
        if (i + 1u == semanticFragments.size()
            && bytes.size() > kHistoryOpaqueTailFragmentBytes)
        {
            forEachHistoryFixedBlock(
                bytes.left(bytes.size() - kHistoryOpaqueTailFragmentBytes),
                appendFragment);
            appendFragment(bytes.right(kHistoryOpaqueTailFragmentBytes));
        }
        // splitGeneratedSedFileSections[0] is the outer header prefix;
        // required frames therefore begin at index 1 and SECT is index 3.
        else if (layout == HistoryFragmentLayout::SectorRecordsV1 && i == 3u)
        {
            const std::size_t remainingFragments =
                fragments.size() >= static_cast<std::size_t>(
                    kMaximumHistoryFragmentsPerState)
                ? 0
                : static_cast<std::size_t>(kMaximumHistoryFragmentsPerState)
                    - fragments.size();
            for (const QByteArray& value : sectorHistoryFragmentBytes(
                     bytes, remainingFragments))
            {
                appendFragment(value);
            }
        }
        else
        {
            forEachHistoryFixedBlock(bytes, appendFragment);
        }
    }
    return fragments;
}

struct GeneratedHistoryFragmentLayout
{
    std::vector<QByteArray> fragments{};
    std::vector<QByteArray> prefixFragments{};
    std::array<std::vector<QByteArray>, 8> frameFragments{};
    QByteArray trailingFraming{};
};

struct HistoryFragmentSpan
{
    qsizetype offset{};
    qsizetype length{};
};

std::vector<HistoryFragmentSpan> fixedHistoryFragmentSpans(
    const qsizetype fullSize, const bool finalHnodFrame)
{
    if (fullSize <= 0)
    {
        return {};
    }
    const bool hasAtomicTail = finalHnodFrame
        && fullSize > kHistoryOpaqueTailFragmentBytes;
    const qsizetype bodySize = hasAtomicTail
        ? fullSize - kHistoryOpaqueTailFragmentBytes : fullSize;
    std::vector<HistoryFragmentSpan> spans{};
    for (qsizetype offset{}; offset < bodySize;)
    {
        const qsizetype length = std::min(
            kHistoryChunkMaximumBytes, bodySize - offset);
        spans.push_back(HistoryFragmentSpan{ offset, length });
        offset += length;
    }
    if (hasAtomicTail)
    {
        spans.push_back(HistoryFragmentSpan{
            bodySize, kHistoryOpaqueTailFragmentBytes });
    }
    return spans;
}

// The SECT wire payload is a counted sequence of self-contained Sector
// records.  Keep the checked descriptor/count and trailer as small bridge
// fragments, then split each record only when it exceeds the common 8 KiB
// history fragment limit.  This is still just a concatenation of the exact
// nested frame bytes: the reader does not need a new schema or record table.
struct SectorHistoryRecordFragmentGroup
{
    std::size_t fragmentBegin{};
    std::size_t fragmentEnd{};
    std::size_t byteBegin{};
    std::size_t byteEnd{};
};

struct SectorHistoryFragmentLayout
{
    std::vector<HistoryFragmentSpan> spans{};
    std::vector<SectorHistoryRecordFragmentGroup> records{};
};

SectorHistoryFragmentLayout sectorHistoryFragmentLayout(
    const std::span<const std::byte> frame,
    const std::size_t maximumFragments =
        static_cast<std::size_t>(kMaximumHistoryFragmentsPerState))
{
    std::vector<assets::detail::SedHistorySectionRecordSpan> records{};
    try
    {
        // A record can occupy more than one history fragment, and the
        // descriptor/count bridge also consumes fragments, so this is only a
        // conservative early allocation bound.  appendRange below applies
        // the exact total-fragment budget.
        records = assets::detail::splitHistoryNestedSectorFrameRecords(
            frame, maximumFragments);
    }
    catch (const std::length_error&)
    {
        throw HistorySectorLayoutLimitExceeded{};
    }
    SectorHistoryFragmentLayout result{};
    const auto appendRange = [&result, maximumFragments](
        const qsizetype begin, const qsizetype end)
    {
        if (begin < 0 || end < begin)
        {
            throw std::logic_error("History SECT fragment range is invalid.");
        }
        const std::size_t first = result.spans.size();
        for (qsizetype offset = begin; offset < end;)
        {
            if (result.spans.size() >= maximumFragments)
            {
                throw HistorySectorLayoutLimitExceeded{};
            }
            const qsizetype length = std::min(
                kHistoryChunkMaximumBytes, end - offset);
            result.spans.push_back(HistoryFragmentSpan{ offset, length });
            offset += length;
        }
        return std::pair<std::size_t, std::size_t>{
            first, result.spans.size() };
    };
    qsizetype cursor = 0;
    for (const assets::detail::SedHistorySectionRecordSpan& record : records)
    {
        const qsizetype begin = static_cast<qsizetype>(record.begin);
        const qsizetype end = static_cast<qsizetype>(record.end);
        // The first record follows the checked frame descriptor and the
        // counted-vector prefix.  Keep that prefix as an ordinary bridge;
        // only subsequent records must start at the preceding record's end.
        if (begin < cursor || end <= begin
            || static_cast<std::size_t>(end) > frame.size())
        {
            throw std::logic_error(
                "History SECT record spans are not contiguous.");
        }
        (void)appendRange(cursor, begin);
        const auto [fragmentBegin, fragmentEnd] = appendRange(begin, end);
        result.records.push_back(SectorHistoryRecordFragmentGroup{
            fragmentBegin, fragmentEnd,
            static_cast<std::size_t>(begin), static_cast<std::size_t>(end) });
        cursor = end;
    }
    appendRange(cursor, static_cast<qsizetype>(frame.size()));
    return result;
}

std::vector<HistoryFragmentSpan> sectorHistoryFragmentSpans(
    const std::span<const std::byte> frame,
    const std::size_t maximumFragments =
        static_cast<std::size_t>(kMaximumHistoryFragmentsPerState))
{
    return sectorHistoryFragmentLayout(frame, maximumFragments).spans;
}

std::vector<QByteArray> sectorHistoryFragmentBytes(
    const QByteArray& frame,
    const std::size_t maximumFragments =
        static_cast<std::size_t>(kMaximumHistoryFragmentsPerState))
{
    const auto spans = sectorHistoryFragmentSpans(
        std::as_bytes(std::span(frame.constData(),
            static_cast<std::size_t>(frame.size()))), maximumFragments);
    std::vector<QByteArray> fragments{};
    fragments.reserve(spans.size());
    for (const HistoryFragmentSpan& span : spans)
    {
        fragments.push_back(frame.mid(span.offset, span.length));
    }
    return fragments;
}

std::optional<qsizetype> historySectionTrailerStart(
    const char* const data, const qsizetype size)
{
    // `sedmir1\n` is the canonical section-trailer marker. Require the
    // complete fixed trailer to end at the frame boundary; a payload string
    // that happens to contain the marker must not become a false anchor.
    constexpr std::string_view marker = "sedmir1\n";
    if (!data || size < kHistorySectionTrailerBytes)
    {
        return std::nullopt;
    }
    const qsizetype offset = size - kHistorySectionTrailerBytes;
    if (std::memcmp(
            data + offset, marker.data(), marker.size()) == 0)
    {
        return offset;
    }
    return std::nullopt;
}

GeneratedHistoryFragmentLayout generatedHistoryFragmentLayout(
    const assets::SedGeneratedUnprotectedDocument& generated,
    const HistoryFragmentLayout layout)
{
    if (generated.bytes.empty())
    {
        throw std::logic_error(
            "Recovery history base fragment layout requires materialized generated bytes.");
    }
    const auto semanticFragments = assets::splitGeneratedSedFileSections(
        std::span<const std::byte>(generated.bytes),
        assets::SedSerializationProfile::HistoryNested);
    const auto& frames = generated.requiredSections.exactFrames();
    if (semanticFragments.size() != frames.size() + 1u
        || generated.canonicalHeader != semanticFragments.front())
    {
        throw std::logic_error(
            "Generated recovery history has an invalid required-section layout.");
    }

    GeneratedHistoryFragmentLayout result{};
    const auto appendGeneratedFragment = [&result](
        const QByteArray& fragment,
        std::vector<QByteArray>& destination)
    {
        if (fragment.isEmpty()
            || static_cast<quint64>(fragment.size()) > kMaximumChunkBytes)
        {
            throw std::logic_error(
                "Generated recovery history fragment is outside the safety limit.");
        }
        if (result.fragments.size()
            >= static_cast<std::size_t>(kMaximumHistoryFragmentsPerState))
        {
            throw HistorySectorLayoutLimitExceeded{};
        }
        destination.push_back(fragment);
        result.fragments.push_back(fragment);
    };
    const auto chunk = [&appendGeneratedFragment](const QByteArray& bytes,
                                 std::vector<QByteArray>& destination)
    {
        forEachHistoryFixedBlock(bytes,
            [&appendGeneratedFragment, &destination](const QByteArray& fragment)
            {
                appendGeneratedFragment(fragment, destination);
            });
    };
    const auto appendAtomic = [&appendGeneratedFragment](const QByteArray& fragment,
                                        std::vector<QByteArray>& destination)
    {
        appendGeneratedFragment(fragment, destination);
    };

    chunk(QByteArray(
        reinterpret_cast<const char*>(semanticFragments.front().data()),
        static_cast<qsizetype>(semanticFragments.front().size())),
        result.prefixFragments);
    for (std::size_t index = 0; index < frames.size(); ++index)
    {
        if (!frames[index])
        {
            throw std::logic_error(
                "Generated recovery history required-section frame is missing.");
        }
        const auto& semantic = semanticFragments[index + 1u];
        const auto& frame = *frames[index];
        if (semantic.size() < frame.size()
            || !std::equal(frame.begin(), frame.end(), semantic.begin())
            || (index + 1u != frames.size() && semantic.size() != frame.size()))
        {
            throw std::logic_error(
                "Generated recovery history frame boundaries disagree.");
        }
        QByteArray unit(
            reinterpret_cast<const char*>(semantic.data()),
            static_cast<qsizetype>(semantic.size()));
        if (index + 1u == frames.size())
        {
            result.trailingFraming = unit.mid(static_cast<qsizetype>(frame.size()));
            if (result.trailingFraming.isEmpty())
            {
                throw std::logic_error(
                    "Generated recovery history trailing framing is missing.");
            }
        }
        if (index + 1u == frames.size()
            && unit.size() > kHistoryOpaqueTailFragmentBytes)
        {
            chunk(unit.left(unit.size() - kHistoryOpaqueTailFragmentBytes),
                result.frameFragments[index]);
            appendAtomic(unit.right(kHistoryOpaqueTailFragmentBytes),
                result.frameFragments[index]);
        }
        else if (layout == HistoryFragmentLayout::SectorRecordsV1
            && index == 2u)
        {
            // SECT is the dominant geometry payload in large histories. Its
            // records are self-delimiting, so pooling those records directly
            // preserves unchanged sectors even when a neighboring sector's
            // size shifts all following byte offsets.
            const std::size_t remainingFragments =
                result.fragments.size() >= static_cast<std::size_t>(
                    kMaximumHistoryFragmentsPerState)
                ? 0
                : static_cast<std::size_t>(kMaximumHistoryFragmentsPerState)
                    - result.fragments.size();
            for (const QByteArray& fragment : sectorHistoryFragmentBytes(
                     unit, remainingFragments))
            {
                appendAtomic(fragment, result.frameFragments[index]);
            }
        }
        else
        {
            chunk(unit, result.frameFragments[index]);
        }
    }
    const QByteArray canonicalTrailingFraming(
        reinterpret_cast<const char*>(generated.canonicalTrailingFraming.data()),
        static_cast<qsizetype>(generated.canonicalTrailingFraming.size()));
    if (canonicalTrailingFraming.isEmpty()
        || result.trailingFraming != canonicalTrailingFraming)
    {
        throw std::logic_error(
            "Generated recovery history has an invalid canonical trailing frame.");
    }
    return result;
}

std::vector<HistoryReference> canonicalBaseReferences(
    const std::vector<QByteArray>& baseFragments)
{
    std::vector<HistoryReference> references{};
    references.reserve(baseFragments.size());
    std::unordered_map<std::size_t, std::vector<qsizetype>> buckets{};
    for (qsizetype index = 0; index < static_cast<qsizetype>(baseFragments.size()); ++index)
    {
        const QByteArray& fragment = baseFragments[static_cast<std::size_t>(index)];
        const std::size_t hash = historyPoolHash(fragment);
        HistoryReference canonical = index;
        auto& bucket = buckets[hash];
        for (const qsizetype candidate : bucket)
        {
            if (baseFragments[static_cast<std::size_t>(candidate)] == fragment)
            {
                canonical = candidate;
                break;
            }
        }
        bucket.push_back(index);
        references.push_back(-1 - canonical);
    }
    return references;
}

struct WorkspaceHistoryPosition
{
    editor::DocumentRecoveryHistoryStack stack{};
    std::size_t index{};
};

struct WorkspaceHistoryBindingDigest
{
    WorkspaceHistoryPosition position{};
    QByteArray beforeSha256{};
    QByteArray afterSha256{};
    QCborValue cleaveAttachment{};
    QCborValue layerVisualsAttachment{};
};

class RecoveryHistoryEncoder
{
public:
    explicit RecoveryHistoryEncoder(
        const Level& baseLevel,
        const HistoryFragmentLayout requestedLayout =
            HistoryFragmentLayout::SectorRecordsV1,
        std::vector<WorkspaceHistoryPosition> bindingRequests = {})
        : fragmentLayout_(requestedLayout),
          bindingRequests_(std::move(bindingRequests))
    {
        bindingDigests_.reserve(bindingRequests_.size());
        setBase(baseLevel);
    }

    RecoveryHistoryEncoder() = default;

    void setBase(const Level& baseLevel)
    {
        assets::SedGeneratedUnprotectedDocument generated =
            assets::generateUnprotectedSedDocument(baseLevel);
        const std::vector<std::byte>& nestedBytes = generated.bytes;
        if (generated.canonicalHeader.empty())
        {
            throw std::logic_error(
                "Recovery history current Level has no canonical SED header.");
        }
        baseHeader_ = QByteArray(
            reinterpret_cast<const char*>(generated.canonicalHeader.data()),
            static_cast<qsizetype>(generated.canonicalHeader.size()));
        if (nestedBytes.empty()
            || nestedBytes.size() > static_cast<std::size_t>(
                kMaximumHistoryReconstructionBytesAbsolute))
        {
            throw std::runtime_error(
                "Recovery history current Level exceeds the native reconstruction safety limit.");
        }
        QCryptographicHash baseHash(QCryptographicHash::Sha256);
        baseHash.addData(reinterpret_cast<const char*>(nestedBytes.data()),
            static_cast<qsizetype>(nestedBytes.size()));
        addJklCogImportStateDigest(baseHash, baseLevel);
        baseDigest_ = baseHash.result();
        baseProjectType_ = baseLevel.kind;
        GeneratedHistoryFragmentLayout layout{};
        if (fragmentLayout_ == HistoryFragmentLayout::SectorRecordsV1)
        {
            // A pathological Level with many tiny Sectors can make the
            // record-oriented manifest larger than the bounded per-state
            // reference budget even though fixed blocks would fit. The
            // bounded splitter and append helper throw before materializing
            // an unbounded vector; restart with the explicit legacy layout
            // before any HIST bytes are published.
            try
            {
                layout = generatedHistoryFragmentLayout(
                    generated, HistoryFragmentLayout::SectorRecordsV1);
            }
            catch (const HistorySectorLayoutLimitExceeded&)
            {
                fragmentLayout_ = HistoryFragmentLayout::FixedBlocksV1;
                layout = generatedHistoryFragmentLayout(
                    generated, HistoryFragmentLayout::FixedBlocksV1);
            }
        }
        else
        {
            layout = generatedHistoryFragmentLayout(
                generated, HistoryFragmentLayout::FixedBlocksV1);
        }
        baseFragments_ = std::move(layout.fragments);
        currentJklCogImportState_ = representedJklCogImportState(baseLevel);
        jklCogImportReferenceCount_ = static_cast<std::uint64_t>(
            currentJklCogImportState_.surfaceRefs.size());
        undoJklCogImportStates_.clear();
        redoJklCogImportStates_.clear();
        if (baseFragments_.empty()
            || baseFragments_.size() > static_cast<std::size_t>(
                kMaximumHistoryFragmentsPerState))
        {
            throw std::runtime_error(
                "Recovery history current Level has an invalid fragment count.");
        }
        baseReferences_ = canonicalBaseReferences(baseFragments_);
        chargedBaseFragments_.assign(baseFragments_.size(), false);
        chargedPoolFragments_.clear();
        baseByHash_.clear();
        for (qsizetype index = 0; index < static_cast<qsizetype>(baseFragments_.size()); ++index)
        {
            baseByHash_[historyPoolHash(baseFragments_[static_cast<std::size_t>(index)])]
                .push_back(index);
        }
        prefixReferences_.reserve(layout.prefixFragments.size());
        std::size_t referenceOffset{};
        for (std::size_t index = 0; index < layout.prefixFragments.size(); ++index)
        {
            prefixReferences_.push_back(baseReferences_[referenceOffset++]);
        }
        std::array<FrameState, 8> baseFrameStates{};
        const auto& baseFrames = generated.requiredSections.exactFrames();
        trailingFraming_ = std::move(layout.trailingFraming);
        for (std::size_t section = 0; section < baseFrameStates.size(); ++section)
        {
            FrameState& frameState = baseFrameStates[section];
            frameState.owner = baseFrames[section];
            frameState.references.reserve(layout.frameFragments[section].size());
            for (std::size_t index = 0;
                 index < layout.frameFragments[section].size(); ++index)
            {
                frameState.references.push_back(baseReferences_[referenceOffset++]);
            }
            const qsizetype fullFrameSize = static_cast<qsizetype>(
                frameState.owner->size()
                + (section + 1u == baseFrames.size()
                    ? trailingFraming_.size() : 0));
            frameState.spans = fragmentLayout_ == HistoryFragmentLayout::SectorRecordsV1
                && section == 2u
                ? sectorHistoryFragmentSpans(
                    std::span<const std::byte>(*frameState.owner))
                : fixedHistoryFragmentSpans(
                    fullFrameSize, section + 1u == baseFrames.size());
            if (frameState.spans.size() != frameState.references.size())
            {
                throw std::logic_error(
                    "Generated recovery history base fragment spans are inconsistent.");
            }
        }
        if (referenceOffset != baseReferences_.size())
        {
            throw std::logic_error(
                "Generated recovery history base fragment layout is inconsistent.");
        }
        undoState_ = StackState{
            baseReferences_, 0, baseFrameStates, trailingFraming_ };
        redoState_ = StackState{
            baseReferences_, 0, std::move(baseFrameStates), trailingFraming_ };
        undoRequiredSections_ = generated.requiredSections;
        redoRequiredSections_ = std::move(generated.requiredSections);
        baseConfigured_ = true;
    }

    void add(
        const editor::DocumentRecoveryHistoryStack stack,
        const std::string_view label,
        const std::uint32_t dirtyFlags,
        const editor::DocumentState& state,
        const assets::SedHistoryTransitionToken* const transition = nullptr,
        const assets::SedRequiredSection additionalSections =
            assets::SedRequiredSection::None,
        const editor::Selection* const explicitNavigationSelection = nullptr)
    {
        if (entryCount_ >= kMaximumHistoryEntries
            || label.size() > static_cast<std::size_t>(kMaximumHistoryLabelCharacters))
        {
            throw std::runtime_error("Recovery history entry count or label size exceeds the safety limit.");
        }

        if (!baseConfigured_)
        {
            throw std::runtime_error(
                "Recovery history requires the save-time current Level as its delta base.");
        }
        const std::size_t historyIndex = static_cast<std::size_t>(
            stack == editor::DocumentRecoveryHistoryStack::Undo
                ? undoState_.stateCount : redoState_.stateCount);
        std::optional<SparseJklCogImportState> preparedJklCogImportState{};
        if ( assets::hasRepresentedJklCogImportState(state.level) )
        {
            const std::uint64_t referenceCount = static_cast<std::uint64_t>(
                state.level.importedJklSurfaceRefs.size());
            if ( referenceCount > kMaximumJklCogImportReferences
                || jklCogImportReferenceCount_
                    > kMaximumJklCogImportReferences - referenceCount )
            {
                throw std::runtime_error(
                    "Recovery history JKL COG import references exceed the bounded item limit.");
            }
            preparedJklCogImportState.emplace(SparseJklCogImportState{
                historyIndex, assets::SedJklCogImportState{
                    state.level.importedJklCogRowsPresent,
                    state.level.importedJklSurfaceRefs } });
        }
        const auto requestedAt = [this, stack](const std::size_t index)
        {
            return std::any_of(bindingRequests_.begin(), bindingRequests_.end(),
                [stack, index](const WorkspaceHistoryPosition& request)
                {
                    return request.stack == stack && request.index == index;
                });
        };
        const bool hashCurrentState = requestedAt(historyIndex);
        const bool hashForNextBinding = historyIndex
                < std::numeric_limits<std::size_t>::max()
            && requestedAt(historyIndex + 1u);
        if (state.level.kind != baseProjectType_)
        {
            throw std::runtime_error(
                "Recovery history entry project type disagrees with its save-time Level base.");
        }
        // Selection is application-owned HIST data rather than part of the
        // core Level frame. Validate it directly before the fast writer path
        // skips an immediate full semantic replay of every generated state.
        // A stale index must omit optional HIST, never publish a timeline that
        // will be rejected only after the user needs recovery.
        if (!selectionReferencesLevel(state.selection, state.level))
        {
            throw std::runtime_error(
                "Recovery history selection references a missing or incompatible Level object.");
        }
        const editor::Selection* const navigationSelection =
            explicitNavigationSelection
                ? explicitNavigationSelection
                : (transition ? transition->navigationSelection() : nullptr);
        // A Redo history state is itself the operation's post-state, so its
        // compact navigation hint can be checked immediately.  Undo hints
        // refer to the adjacent preceding state and are validated against
        // that exact Level by the decoder; live transition tokens are
        // unforgeable and originate from a selection captured after apply().
        if (navigationSelection
            && stack == editor::DocumentRecoveryHistoryStack::Redo
            && !selectionReferencesLevel(*navigationSelection, state.level))
        {
            throw std::runtime_error(
                "Recovery history navigation selection references a missing or incompatible Level object.");
        }

        // The native Level codec remains the only Level schema on disk. The
        // concrete compact Snapshot says which exact generated section frames
        // can be shared with this stack's preceding state. Resource catalogs
        // are still rebuilt exactly, and the core writer broadens invalidation
        // when a category-local index changes.
        const assets::SedGeneratedUnprotectedRequiredSections& previous =
            stack == editor::DocumentRecoveryHistoryStack::Undo
            ? undoRequiredSections_ : redoRequiredSections_;
        assets::SedGeneratedUnprotectedDocument generated = transition
            ? assets::detail::generateHistoryNestedTransitionDocument(
                state.level, *transition, &previous, false,
                additionalSections)
            : assets::generateUnprotectedSedDocument(state.level, false);
        if (generated.byteSize == 0
            || generated.byteSize > static_cast<std::size_t>(
                kMaximumHistoryReconstructionBytesAbsolute))
        {
            throw std::runtime_error(
                "Recovery history Level exceeds the native reconstruction safety limit.");
        }
        StackState& stackState = stack == editor::DocumentRecoveryHistoryStack::Undo
            ? undoState_ : redoState_;
        const std::size_t poolStart = pool_.size();
        bool poolCommitted = false;
        const auto poolRollback = qScopeGuard([this, poolStart, &poolCommitted]
        {
            if (!poolCommitted) rollbackPoolTo(poolStart);
        });
        if (generated.canonicalHeader.empty())
        {
            throw std::logic_error(
                "Generated recovery history state has no canonical SED header.");
        }
        const QByteArray generatedHeader(
            reinterpret_cast<const char*>(generated.canonicalHeader.data()),
            static_cast<qsizetype>(generated.canonicalHeader.size()));
        if (generated.canonicalTrailingFraming.empty())
        {
            throw std::logic_error(
                "Generated recovery history state has no canonical trailing frame.");
        }
        QByteArray nestedDigest{};
        std::optional<WorkspaceHistoryBindingDigest> preparedBinding{};
        if (hashCurrentState || hashForNextBinding)
        {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            const auto addHashBytes = [&hash](
                const std::byte* const data, const std::size_t size)
            {
                if (size > static_cast<std::size_t>(
                        std::numeric_limits<qsizetype>::max()))
                {
                    throw std::runtime_error(
                        "Workspace history Level digest exceeds the hash input limit.");
                }
                hash.addData(reinterpret_cast<const char*>(data),
                    static_cast<qsizetype>(size));
            };
            addHashBytes(generated.canonicalHeader.data(),
                generated.canonicalHeader.size());
            for (const auto& frame : generated.requiredSections.exactFrames())
            {
                if (!frame)
                {
                    throw std::logic_error(
                        "Workspace history Level digest has a missing required frame.");
                }
                addHashBytes(frame->data(), frame->size());
            }
            addHashBytes(generated.canonicalTrailingFraming.data(),
                generated.canonicalTrailingFraming.size());
            addJklCogImportStateDigest(hash, state.level);
            nestedDigest = hash.result();

            if (hashCurrentState)
            {
                const QByteArray& adjacentDigest =
                    stack == editor::DocumentRecoveryHistoryStack::Undo
                    ? (historyIndex == 0u ? baseDigest_ : lastUndoStateDigest_)
                    : (historyIndex == 0u ? baseDigest_ : lastRedoStateDigest_);
                if (adjacentDigest.size()
                    != QCryptographicHash::hashLength(QCryptographicHash::Sha256))
                {
                    throw std::logic_error(
                        "Workspace history binding is missing its adjacent Level digest.");
                }
                WorkspaceHistoryBindingDigest binding{};
                binding.position = WorkspaceHistoryPosition{ stack, historyIndex };
                if (stack == editor::DocumentRecoveryHistoryStack::Undo)
                {
                    binding.beforeSha256 = nestedDigest;
                    binding.afterSha256 = adjacentDigest;
                }
                else
                {
                    binding.beforeSha256 = adjacentDigest;
                    binding.afterSha256 = nestedDigest;
                }
                preparedBinding = std::move(binding);
            }
        }
        QByteArray preparedLastStateDigest = hashForNextBinding
            ? nestedDigest : QByteArray{};
        const QByteArray currentTrailingFraming(
            reinterpret_cast<const char*>(generated.canonicalTrailingFraming.data()),
            static_cast<qsizetype>(generated.canonicalTrailingFraming.size()));
        if (currentTrailingFraming.size() != trailingFraming_.size())
        {
            throw std::logic_error(
                "Generated recovery history trailing frame has an incompatible size.");
        }
        std::vector<HistoryReference> currentPrefixReferences = prefixReferences_;
        if (generatedHeader != baseHeader_)
        {
            currentPrefixReferences.clear();
            forEachHistoryFixedBlock(generatedHeader,
                [this, &currentPrefixReferences](const QByteArray& fragment)
                {
                    currentPrefixReferences.push_back(referenceFor(fragment));
                });
            if (currentPrefixReferences.size() != prefixReferences_.size())
            {
                throw std::logic_error(
                    "Generated recovery history header has an incompatible fragment layout.");
            }
        }
        std::vector<HistoryReference> references{};
        references.reserve(stackState.previous.size());
        references.insert(references.end(), currentPrefixReferences.begin(),
            currentPrefixReferences.end());
        std::array<FrameState, 8> nextFrames{};
        const QByteArray& priorTrailingFraming = stackState.trailingFraming;
        const auto& frames = generated.requiredSections.exactFrames();
        for (std::size_t section = 0; section < frames.size(); ++section)
        {
            if (!frames[section])
            {
                throw std::logic_error(
                    "Generated recovery history required-section frame is missing.");
            }
            FrameState& next = nextFrames[section];
            next.owner = frames[section];
            const FrameState& prior = stackState.frames[section];
            // A transition generator may materialize a fresh immutable owner
            // even when its bytes are identical to the preceding frame (the
            // DTO writer does this for every state).  Treat byte identity as
            // the canonical identity as well as shared-owner identity.  This
            // keeps the optimized transition walk byte-for-byte identical to
            // the portable DTO writer and, more importantly, preserves the
            // existing variable span layout instead of needlessly
            // re-fragmenting an unchanged frame.
            const bool finalHnodFrame = section + 1u == frames.size();
            const bool priorFrameBytesEqual = prior.owner
                && prior.owner->size() == next.owner->size()
                && std::equal(prior.owner->begin(), prior.owner->end(),
                    next.owner->begin())
                && (!finalHnodFrame
                    || priorTrailingFraming == currentTrailingFraming);
            const qsizetype nextFullSize = static_cast<qsizetype>(
                next.owner->size()
                + (finalHnodFrame ? currentTrailingFraming.size() : 0));
            const auto spansCoverFrame = [](
                const std::vector<HistoryFragmentSpan>& spans,
                const std::size_t referenceCount,
                const qsizetype fullSize)
            {
                if (spans.size() != referenceCount || fullSize < 0)
                {
                    return false;
                }
                qsizetype expectedOffset{};
                for (const HistoryFragmentSpan& span : spans)
                {
                    if (span.offset != expectedOffset
                        || span.length <= 0
                        || span.length > kHistoryChunkMaximumBytes
                        || span.offset > fullSize - span.length)
                    {
                        return false;
                    }
                    expectedOffset += span.length;
                }
                return expectedOffset == fullSize;
            };
            const bool priorFrameOwnerAndFramingEqual =
                prior.owner == next.owner
                && (!finalHnodFrame
                    || priorTrailingFraming == currentTrailingFraming);
            if (priorFrameOwnerAndFramingEqual || priorFrameBytesEqual)
            {
                next.references = prior.references;
                next.spans = prior.spans;
                ++reusedFrameCount_;
                ++reusedFramesBySection_[section];
            }
            else
            {
                // Large non-final frames already live in immutable core-owned
                // storage. Compare them in place and materialize QByteArray
                // only for blocks that actually enter the pool. HNOD is the
                // sole exception because its native frame and fixed trailing
                // document framing share one atomic final block.
                QByteArray finalFrameBytes{};
                if (finalHnodFrame)
                {
                    finalFrameBytes = QByteArray(
                        reinterpret_cast<const char*>(next.owner->data()),
                        static_cast<qsizetype>(next.owner->size()));
                    finalFrameBytes.append(currentTrailingFraming);
                }
                const char* const currentData = finalHnodFrame
                    ? finalFrameBytes.constData()
                    : reinterpret_cast<const char*>(next.owner->data());
                const qsizetype currentSize = finalHnodFrame
                    ? finalFrameBytes.size()
                    : static_cast<qsizetype>(next.owner->size());

                // The last 64 bytes of HNOD plus its checked trailing framing
                // are one atomic fragment by V1 contract.  All preceding
                // bytes use the same fixed offsets in the writer and reader.
                const bool hasAtomicTail = finalHnodFrame
                    && currentSize > kHistoryOpaqueTailFragmentBytes;
                const qsizetype bodySize = hasAtomicTail
                    ? currentSize - kHistoryOpaqueTailFragmentBytes
                    : currentSize;
                const qsizetype bodyBlocks = bodySize == 0
                    ? 0
                    : (bodySize + kHistoryChunkMaximumBytes - 1)
                        / kHistoryChunkMaximumBytes;

                const qsizetype priorOwnerSize = prior.owner
                    ? static_cast<qsizetype>(prior.owner->size()) : 0;
                const qsizetype priorFullSize = prior.owner
                    ? priorOwnerSize
                        + (finalHnodFrame
                            ? static_cast<qsizetype>(priorTrailingFraming.size()) : 0)
                    : 0;
                const bool priorHasAtomicTail = finalHnodFrame
                    && priorFullSize > kHistoryOpaqueTailFragmentBytes;
                const qsizetype priorBodySize = priorHasAtomicTail
                    ? priorFullSize - kHistoryOpaqueTailFragmentBytes
                    : priorFullSize;
                const qsizetype priorBodyBlocks = priorBodySize == 0
                    ? 0
                    : (priorBodySize + kHistoryChunkMaximumBytes - 1)
                        / kHistoryChunkMaximumBytes;
                const qsizetype priorReferenceCount = priorBodyBlocks
                    + (priorHasAtomicTail ? 1 : 0);
                const bool priorFixedLayoutMatches = prior.owner
                    && prior.references.size() == static_cast<std::size_t>(
                        priorReferenceCount);

                // Compare a range without materializing a second copy of the
                // previous frame.  This is deliberately a byte comparison,
                // not a hash comparison: equal bytes are the canonical
                // identity, and an old reference can therefore be reused
                // without hashing or touching the fragment pool.
                const auto previousRangeEquals =
                    [currentData, &prior, priorOwnerSize, priorFullSize,
                     finalHnodFrame, &priorTrailingFraming](const qsizetype offset,
                                     const qsizetype length)
                {
                    if (!prior.owner || offset < 0 || length <= 0
                        || offset > priorFullSize - length)
                    {
                        return false;
                    }
                    const qsizetype ownerAvailable = std::max<qsizetype>(
                        0, priorOwnerSize - offset);
                    const qsizetype ownerLength = std::min(length, ownerAvailable);
                    if (ownerLength != 0
                        && std::memcmp(
                            prior.owner->data() + offset,
                            currentData + offset,
                            static_cast<std::size_t>(ownerLength)) != 0)
                    {
                        return false;
                    }
                    const qsizetype framingOffset = offset + ownerLength;
                    const qsizetype framingLength = length - ownerLength;
                    if (framingLength == 0)
                    {
                        return true;
                    }
                    if (!finalHnodFrame
                        || framingOffset < priorOwnerSize
                        || framingLength > priorTrailingFraming.size()
                        || framingOffset - priorOwnerSize
                            > priorTrailingFraming.size() - framingLength)
                    {
                        return false;
                    }
                    return std::memcmp(
                        priorTrailingFraming.constData()
                            + (framingOffset - priorOwnerSize),
                        currentData + offset + ownerLength,
                        static_cast<std::size_t>(framingLength)) == 0;
                };

                const auto currentFragment = [currentData](
                    const qsizetype offset, const qsizetype length)
                {
                    return QByteArray(currentData + offset, length);
                };

                next.references.reserve(static_cast<std::size_t>(
                    bodyBlocks + (hasAtomicTail ? 1 : 0)));
                bool fragmented = false;
                const auto appendFragment =
                    [this, &next, &fragmented, section](
                        const QByteArray& fragment)
                    {
                        if (fragment.isEmpty()
                            || static_cast<quint64>(fragment.size())
                                > kMaximumChunkBytes)
                        {
                            throw std::runtime_error(
                                "Recovery history fragment size is outside the safety limit.");
                        }
                        fragmented = true;
                        fragmentedFrameBytes_ +=
                            static_cast<quint64>(fragment.size());
                        fragmentedBytesBySection_[section] +=
                            static_cast<quint64>(fragment.size());
                        next.references.push_back(referenceFor(fragment));
                    };

                if (fragmentLayout_ == HistoryFragmentLayout::SectorRecordsV1
                    && section == 2u)
                {
                    // Pool complete SECT records (and only split an unusually
                    // large record into bounded pieces).  referenceFor()
                    // performs exact hash-bucket equality against both the
                    // save-time base and all prior states, so an unchanged
                    // record is reused even when a preceding record changed
                    // its byte length and shifted its physical offset.
                    const SectorHistoryFragmentLayout currentLayout =
                        sectorHistoryFragmentLayout(
                            std::span<const std::byte>(*next.owner));
                    if (currentLayout.spans.size()
                        > static_cast<std::size_t>(kMaximumHistoryFragmentsPerState))
                    {
                        throw HistorySectorLayoutLimitExceeded{};
                    }
                    next.spans = currentLayout.spans;
                    next.references.reserve(next.spans.size());
                    const char* const data = reinterpret_cast<const char*>(
                        next.owner->data());
                    std::optional<SectorHistoryFragmentLayout> priorLayout{};
                    if (prior.owner)
                    {
                        priorLayout = sectorHistoryFragmentLayout(
                            std::span<const std::byte>(*prior.owner));
                    }
                    const bool priorLayoutValid = priorLayout.has_value()
                        && prior.spans.size() == prior.references.size()
                        && priorLayout->spans.size() == prior.spans.size()
                        && std::equal(
                            priorLayout->spans.begin(), priorLayout->spans.end(),
                            prior.spans.begin(),
                            [](const HistoryFragmentSpan& lhs,
                               const HistoryFragmentSpan& rhs)
                            {
                                return lhs.offset == rhs.offset
                                    && lhs.length == rhs.length;
                            });
                    const auto appendRecordFragment =
                        [this, &next, &fragmented, section](
                            const QByteArray& fragment,
                            const bool isRecordFragment)
                    {
                        if (fragment.isEmpty()
                            || static_cast<quint64>(fragment.size())
                                > kMaximumChunkBytes)
                        {
                            throw std::runtime_error(
                                "Recovery history SECT fragment is outside the safety limit.");
                        }
                        const std::size_t poolSizeBefore = pool_.size();
                        next.references.push_back(referenceFor(fragment));
                        if (pool_.size() != poolSizeBefore)
                        {
                            fragmented = true;
                            fragmentedFrameBytes_ +=
                                static_cast<quint64>(fragment.size());
                            fragmentedBytesBySection_[section] +=
                                static_cast<quint64>(fragment.size());
                        }
                        else if (isRecordFragment)
                        {
                            recordReusedBytesBySection_[section] +=
                                static_cast<quint64>(fragment.size());
                        }
                    };
                    const auto appendPoolRange =
                        [&appendRecordFragment, &currentLayout, data](
                            const std::size_t begin,
                            const std::size_t end,
                            const bool isRecordFragment)
                    {
                        for (std::size_t index = begin; index < end; ++index)
                        {
                            const HistoryFragmentSpan& span =
                                currentLayout.spans[index];
                            appendRecordFragment(QByteArray(
                                data + span.offset, span.length),
                                isRecordFragment);
                        }
                    };
                    std::size_t currentFragmentCursor = 0;
                    for (std::size_t recordIndex = 0;
                         recordIndex < currentLayout.records.size();
                         ++recordIndex)
                    {
                        const SectorHistoryRecordFragmentGroup& currentRecord =
                            currentLayout.records[recordIndex];
                        appendPoolRange(
                            currentFragmentCursor, currentRecord.fragmentBegin,
                            false);
                        bool directReuse = priorLayoutValid
                            && recordIndex < priorLayout->records.size();
                        if (directReuse)
                        {
                            const SectorHistoryRecordFragmentGroup& priorRecord =
                                priorLayout->records[recordIndex];
                            directReuse = currentRecord.byteEnd
                                    - currentRecord.byteBegin
                                == priorRecord.byteEnd - priorRecord.byteBegin
                                && currentRecord.fragmentEnd
                                    - currentRecord.fragmentBegin
                                == priorRecord.fragmentEnd
                                    - priorRecord.fragmentBegin;
                            if (directReuse)
                            {
                                for (std::size_t fragmentIndex =
                                         currentRecord.fragmentBegin;
                                     fragmentIndex < currentRecord.fragmentEnd;
                                     ++fragmentIndex)
                                {
                                    const std::size_t priorIndex =
                                        priorRecord.fragmentBegin
                                        + (fragmentIndex
                                            - currentRecord.fragmentBegin);
                                    const HistoryFragmentSpan& currentSpan =
                                        currentLayout.spans[fragmentIndex];
                                    const HistoryFragmentSpan& priorSpan =
                                        priorLayout->spans[priorIndex];
                                    if (currentSpan.length != priorSpan.length
                                        || std::memcmp(
                                            next.owner->data()
                                                + currentSpan.offset,
                                            prior.owner->data()
                                                + priorSpan.offset,
                                            static_cast<std::size_t>(
                                                currentSpan.length)) != 0)
                                    {
                                        directReuse = false;
                                        break;
                                    }
                                }
                            }
                            if (directReuse)
                            {
                                for (std::size_t fragmentIndex =
                                         currentRecord.fragmentBegin;
                                     fragmentIndex < currentRecord.fragmentEnd;
                                     ++fragmentIndex)
                                {
                                    const std::size_t priorIndex =
                                        priorRecord.fragmentBegin
                                        + (fragmentIndex
                                            - currentRecord.fragmentBegin);
                                    next.references.push_back(
                                        prior.references[priorIndex]);
                                }
                                const quint64 bytes = static_cast<quint64>(
                                    currentRecord.byteEnd
                                        - currentRecord.byteBegin);
                                recordReusedBytesBySection_[section] += bytes;
                                directRecordReusedBytesBySection_[section] += bytes;
                            }
                        }
                        if (!directReuse)
                        {
                            appendPoolRange(
                                currentRecord.fragmentBegin,
                                currentRecord.fragmentEnd, true);
                        }
                        currentFragmentCursor = currentRecord.fragmentEnd;
                    }
                    appendPoolRange(
                        currentFragmentCursor, currentLayout.spans.size(), false);
                    if (currentLayout.spans.size() != next.references.size())
                    {
                        throw std::logic_error(
                            "Generated recovery history SECT spans disagree with references.");
                    }
                    if (fragmented)
                    {
                        ++fragmentedFrameCount_;
                        ++fragmentedFramesBySection_[section];
                    }
                }
                else
                {
                struct ResynchronizedTailPart
                {
                    qsizetype currentOffset{};
                    qsizetype length{};
                    qsizetype priorReferenceIndex{};
                };

                // A structural edit can change the size of a semantic frame
                // and shift every following fixed block.  In that case the
                // old same-offset loop would re-materialize the whole tail.
                // Find a deterministic exact suffix, end the newly emitted
                // bridge at the next existing prior block boundary, and then
                // reuse the old references for the shifted tail.  The bridge
                // is intentionally still split at <=8 KiB, so this cannot
                // create an oversized fragment and remains bounded by the
                // existing writer limits.
                std::vector<ResynchronizedTailPart> resynchronizedTail{};
                std::size_t resynchronizedPrefixSpanStart{};
                std::size_t resynchronizedPrefixSpanCount{};
                qsizetype resynchronizedCurrentPrefixStart{};
                qsizetype resynchronizedCurrentPrefixEnd{};
                qsizetype resynchronizedCurrentTailStart{};
                qsizetype resynchronizedCurrentReuseEnd{};
                qsizetype resynchronizedCurrentBodyEnd{};
                bool useResynchronization = false;
                const auto currentTrailerStart = historySectionTrailerStart(
                    reinterpret_cast<const char*>(next.owner->data()),
                    static_cast<qsizetype>(next.owner->size()));
                const auto priorTrailerStart = prior.owner
                    ? historySectionTrailerStart(
                        reinterpret_cast<const char*>(prior.owner->data()),
                        priorOwnerSize)
                    : std::optional<qsizetype>{};
                bool priorSpansValid = prior.owner
                    && prior.spans.size() == prior.references.size();
                if (priorSpansValid)
                {
                    qsizetype expectedOffset{};
                    for (const HistoryFragmentSpan& span : prior.spans)
                    {
                        if (span.offset != expectedOffset
                            || span.length <= 0
                            || span.length > kHistoryChunkMaximumBytes
                            || span.offset > priorFullSize - span.length)
                        {
                            priorSpansValid = false;
                            break;
                        }
                        expectedOffset += span.length;
                    }
                    priorSpansValid = priorSpansValid
                        && expectedOffset == priorFullSize;
                }
                if (priorSpansValid && currentTrailerStart && priorTrailerStart
                    && *currentTrailerStart > 0 && *priorTrailerStart > 0)
                {
                    // Exclude each section descriptor and trailer from
                    // matching. Both repeat the changed payload size/checksum,
                    // so treating either as part of the body would hide the
                    // large unchanged prefix/suffix after every structural
                    // edit. The descriptor remains part of the emitted
                    // bridge; payload bytes after it can still reuse direct
                    // prior spans once a complete span boundary is reached.
                    const qsizetype currentBodyEnd = *currentTrailerStart;
                    const qsizetype priorBodyEnd = *priorTrailerStart;
                    const char* const priorData =
                        reinterpret_cast<const char*>(prior.owner->data());
                    const qsizetype comparable = std::min(
                        currentBodyEnd, priorBodyEnd);
                    qsizetype commonPrefix = std::min(
                        comparable, kHistorySectionHeaderBytes);
                    while (commonPrefix + kHistoryChunkMaximumBytes <= comparable
                        && std::memcmp(
                            currentData + commonPrefix,
                            priorData + commonPrefix,
                            static_cast<std::size_t>(kHistoryChunkMaximumBytes)) == 0)
                    {
                        commonPrefix += kHistoryChunkMaximumBytes;
                    }
                    while (commonPrefix < comparable
                        && currentData[commonPrefix] == priorData[commonPrefix])
                    {
                        ++commonPrefix;
                    }

                    qsizetype commonSuffix{};
                    while (commonSuffix + kHistoryChunkMaximumBytes
                                <= comparable - commonPrefix
                        && std::memcmp(
                            currentData + currentBodyEnd - commonSuffix
                                - kHistoryChunkMaximumBytes,
                            priorData + priorBodyEnd - commonSuffix
                                - kHistoryChunkMaximumBytes,
                            static_cast<std::size_t>(kHistoryChunkMaximumBytes)) == 0)
                    {
                        commonSuffix += kHistoryChunkMaximumBytes;
                    }
                    while (commonSuffix < comparable - commonPrefix
                        && currentData[currentBodyEnd - commonSuffix - 1]
                            == priorData[priorBodyEnd - commonSuffix - 1])
                    {
                        ++commonSuffix;
                    }

                    if (commonSuffix >= kHistoryResynchronizationMinimumSuffixBytes)
                    {
                        const qsizetype priorChangedEnd = priorBodyEnd - commonSuffix;
                        std::size_t priorTailSpanIndex = prior.spans.size();
                        for (std::size_t index = 0; index < prior.spans.size(); ++index)
                        {
                            const HistoryFragmentSpan& span = prior.spans[index];
                            if (span.offset >= priorChangedEnd
                                && span.offset < priorBodyEnd
                                && span.offset <= priorBodyEnd - span.length)
                            {
                                priorTailSpanIndex = index;
                                break;
                            }
                        }
                        if (priorTailSpanIndex < prior.spans.size())
                        {
                            const qsizetype priorTailStart = prior.spans[
                                priorTailSpanIndex].offset;
                            const qint64 sizeDelta = static_cast<qint64>(currentBodyEnd)
                                - static_cast<qint64>(priorBodyEnd);
                            const qint64 candidateCurrentTail =
                                static_cast<qint64>(priorTailStart) + sizeDelta;
                            const qsizetype currentChangedEnd =
                                currentBodyEnd - commonSuffix;
                            if (candidateCurrentTail >= currentChangedEnd
                                && candidateCurrentTail >= 0
                                && candidateCurrentTail <= currentBodyEnd)
                            {
                                // Keep the unchanged prefix in its original
                                // references too.  The bridge must begin at
                                // the first prior span that crosses into the
                                // changed region; routing it through
                                // referenceFor() would allocate/hash and
                                // re-discover every unchanged prefix block on
                                // a late-sector structural edit.
                                std::size_t prefixSpanStart{};
                                while (prefixSpanStart < priorTailSpanIndex
                                    && prior.spans[prefixSpanStart].offset
                                        < kHistorySectionHeaderBytes)
                                {
                                    ++prefixSpanStart;
                                }
                                const qsizetype prefixStart = prefixSpanStart
                                    < priorTailSpanIndex
                                    ? prior.spans[prefixSpanStart].offset : 0;
                                qsizetype prefixEnd = prefixStart;
                                std::size_t prefixSpanCount{};
                                for (std::size_t index = prefixSpanStart;
                                     index < priorTailSpanIndex; ++index)
                                {
                                    const HistoryFragmentSpan& span =
                                        prior.spans[index];
                                    if (span.offset != prefixEnd
                                        || span.offset >= priorBodyEnd
                                        || span.offset > priorBodyEnd - span.length
                                        || span.offset + span.length > commonPrefix)
                                    {
                                        break;
                                    }
                                    prefixEnd += span.length;
                                    ++prefixSpanCount;
                                }
                                qsizetype currentOffset =
                                    static_cast<qsizetype>(candidateCurrentTail);
                                for (std::size_t index = priorTailSpanIndex;
                                     index < prior.spans.size(); ++index)
                                {
                                    const HistoryFragmentSpan& span = prior.spans[index];
                                    if (span.offset >= priorBodyEnd)
                                    {
                                        // The section trailer (and, for the
                                        // final HNOD, the outer framing) is
                                        // intentionally emitted as a fresh
                                        // atomic tail below.
                                        break;
                                    }
                                    if (span.offset < priorTailStart)
                                    {
                                        resynchronizedTail.clear();
                                        break;
                                    }
                                    if (span.offset > priorBodyEnd - span.length)
                                    {
                                        // The fixed base layout may have one
                                        // final fragment crossing from the
                                        // payload into the section trailer.
                                        // Reuse its body prefix only through a
                                        // newly emitted bridge below.
                                        break;
                                    }
                                    if (currentOffset > currentBodyEnd - span.length
                                        || std::memcmp(
                                            currentData + currentOffset,
                                            priorData + span.offset,
                                            static_cast<std::size_t>(span.length)) != 0)
                                    {
                                        resynchronizedTail.clear();
                                        break;
                                    }
                                    resynchronizedTail.push_back(
                                        ResynchronizedTailPart{
                                            currentOffset, span.length,
                                            static_cast<qsizetype>(index) });
                                    currentOffset += span.length;
                                }
                                if (!resynchronizedTail.empty()
                                    && currentOffset <= currentBodyEnd
                                    && currentBodyEnd - currentOffset
                                        <= kHistoryChunkMaximumBytes
                                    && currentSize - currentBodyEnd
                                        <= kHistoryChunkMaximumBytes)
                                {
                                    resynchronizedPrefixSpanCount =
                                        prefixSpanCount;
                                    resynchronizedPrefixSpanStart =
                                        prefixSpanStart;
                                    resynchronizedCurrentPrefixStart =
                                        prefixSpanCount != 0 ? prefixStart : 0;
                                    resynchronizedCurrentPrefixEnd =
                                        prefixSpanCount != 0 ? prefixEnd : 0;
                                    resynchronizedCurrentTailStart =
                                        static_cast<qsizetype>(candidateCurrentTail);
                                    resynchronizedCurrentReuseEnd = currentOffset;
                                    resynchronizedCurrentBodyEnd = currentBodyEnd;
                                    resynchronizedPrefixBytesBySection_[section] +=
                                        static_cast<quint64>(
                                            prefixSpanCount != 0
                                            ? prefixEnd - prefixStart : 0);
                                    for (const ResynchronizedTailPart& part :
                                         resynchronizedTail)
                                    {
                                        resynchronizedSuffixBytesBySection_[section] +=
                                            static_cast<quint64>(part.length);
                                    }
                                    useResynchronization = true;
                                }
                            }
                        }
                    }
                }

                next.references.reserve(static_cast<std::size_t>(
                    bodyBlocks + (hasAtomicTail ? 1 : 0)
                        + resynchronizedTail.size()
                        + resynchronizedPrefixSpanCount));
                if (useResynchronization)
                {
                    // The bytes before the first complete reusable span
                    // include the changed section descriptor and/or a
                    // boundary-crossing block, so retain them as the bridge.
                    for (qsizetype offset{};
                         offset < resynchronizedCurrentPrefixStart;)
                    {
                        const qsizetype length = std::min(
                            kHistoryChunkMaximumBytes,
                            resynchronizedCurrentPrefixStart - offset);
                        appendFragment(currentFragment(offset, length));
                        offset += length;
                    }
                    for (std::size_t index = 0;
                         index < resynchronizedPrefixSpanCount; ++index)
                    {
                        next.references.push_back(prior.references[
                            resynchronizedPrefixSpanStart + index]);
                    }
                    // The bridge begins after the maximal exact same-offset
                    // prefix. Only the changed region (and any partial
                    // boundary span) is materialized and hashed.
                    for (qsizetype offset = resynchronizedCurrentPrefixEnd;
                         offset < resynchronizedCurrentTailStart;)
                    {
                        const qsizetype length = std::min(
                            kHistoryChunkMaximumBytes,
                            resynchronizedCurrentTailStart - offset);
                        appendFragment(currentFragment(offset, length));
                        offset += length;
                    }
                    for (const ResynchronizedTailPart& part : resynchronizedTail)
                    {
                        next.references.push_back(prior.references[
                            static_cast<std::size_t>(part.priorReferenceIndex)]);
                    }
                    if (resynchronizedCurrentReuseEnd
                        < resynchronizedCurrentBodyEnd)
                    {
                        appendFragment(currentFragment(
                            resynchronizedCurrentReuseEnd,
                            resynchronizedCurrentBodyEnd
                                - resynchronizedCurrentReuseEnd));
                    }
                    appendFragment(currentFragment(
                        resynchronizedCurrentBodyEnd,
                        currentSize - resynchronizedCurrentBodyEnd));
                    next.spans.reserve(next.references.size());
                    for (qsizetype offset{};
                         offset < resynchronizedCurrentPrefixStart;)
                    {
                        const qsizetype length = std::min(
                            kHistoryChunkMaximumBytes,
                            resynchronizedCurrentPrefixStart - offset);
                        next.spans.push_back(HistoryFragmentSpan{ offset, length });
                        offset += length;
                    }
                    for (std::size_t index = 0;
                         index < resynchronizedPrefixSpanCount; ++index)
                    {
                        next.spans.push_back(prior.spans[
                            resynchronizedPrefixSpanStart + index]);
                    }
                    for (qsizetype offset = resynchronizedCurrentPrefixEnd;
                         offset < resynchronizedCurrentTailStart;)
                    {
                        const qsizetype length = std::min(
                            kHistoryChunkMaximumBytes,
                            resynchronizedCurrentTailStart - offset);
                        next.spans.push_back(HistoryFragmentSpan{ offset, length });
                        offset += length;
                    }
                    for (const ResynchronizedTailPart& part : resynchronizedTail)
                    {
                        next.spans.push_back(HistoryFragmentSpan{
                            part.currentOffset, part.length });
                    }
                    if (resynchronizedCurrentReuseEnd
                        < resynchronizedCurrentBodyEnd)
                    {
                        next.spans.push_back(HistoryFragmentSpan{
                            resynchronizedCurrentReuseEnd,
                            resynchronizedCurrentBodyEnd
                                - resynchronizedCurrentReuseEnd });
                    }
                    next.spans.push_back(HistoryFragmentSpan{
                        resynchronizedCurrentBodyEnd,
                        currentSize - resynchronizedCurrentBodyEnd });
                }
                else
                {
                    for (qsizetype block = 0; block < bodyBlocks; ++block)
                    {
                        const qsizetype offset = block * kHistoryChunkMaximumBytes;
                        const qsizetype length = std::min(
                            kHistoryChunkMaximumBytes, bodySize - offset);
                        const qsizetype priorLength = offset < priorBodySize
                            ? std::min(kHistoryChunkMaximumBytes,
                                priorBodySize - offset) : 0;
                        if (priorFixedLayoutMatches
                            && block < static_cast<qsizetype>(prior.references.size())
                            && priorLength == length
                            && previousRangeEquals(offset, length))
                        {
                            next.references.push_back(prior.references[
                                static_cast<std::size_t>(block)]);
                        }
                        else
                        {
                            appendFragment(currentFragment(offset, length));
                        }
                    }
                    if (hasAtomicTail)
                    {
                        const qsizetype offset = bodySize;
                        const qsizetype priorTailIndex = priorBodyBlocks;
                        if (priorFixedLayoutMatches
                            && priorFullSize == currentSize
                            && priorTailIndex
                                < static_cast<qsizetype>(prior.references.size())
                            && previousRangeEquals(offset,
                                kHistoryOpaqueTailFragmentBytes))
                        {
                            next.references.push_back(prior.references[
                                static_cast<std::size_t>(priorTailIndex)]);
                        }
                        else
                        {
                            appendFragment(currentFragment(
                                offset, kHistoryOpaqueTailFragmentBytes));
                        }
                    }
                    next.spans = fixedHistoryFragmentSpans(
                        currentSize, finalHnodFrame);
                    if (next.spans.size() != next.references.size())
                    {
                        throw std::logic_error(
                            "Generated recovery history frame spans disagree with references.");
                    }
                }
                if (fragmented)
                {
                    ++fragmentedFrameCount_;
                    ++fragmentedFramesBySection_[section];
                }
                }
            }
            if (!spansCoverFrame(
                    next.spans, next.references.size(), nextFullSize))
            {
                throw std::logic_error(
                    "Generated recovery history frame spans are not contiguous, bounded, or complete.");
            }
            references.insert(references.end(), next.references.begin(),
                next.references.end());
        }
        if (references.empty())
        {
            throw std::runtime_error("Recovery history Level has an invalid fragment count.");
        }
        if (references.size()
            > static_cast<std::size_t>(kMaximumHistoryFragmentsPerState))
        {
            if (fragmentLayout_ == HistoryFragmentLayout::SectorRecordsV1)
            {
                throw HistorySectorLayoutLimitExceeded{};
            }
            throw std::runtime_error(
                "Recovery history Level has an invalid fixed fragment count.");
        }

        const quint64 stateBytes = static_cast<quint64>(generated.byteSize);
        // Most history states share the unchanged current-base fragments.
        // Charge each fragment only on first use, with a bounded per-state
        // floor so a hostile manifest cannot replay a tiny reference list
        // without consuming reconstruction budget.
        std::vector<bool> nextChargedBase = chargedBaseFragments_;
        std::vector<bool> nextChargedPool = chargedPoolFragments_;
        quint64 uniqueFragmentBytes{};
        for (const HistoryReference reference : references)
        {
            if (reference < 0)
            {
                const qint64 baseIndex = -1 - reference;
                if (baseIndex >= 0
                    && static_cast<std::size_t>(baseIndex) < chargedBaseFragments_.size()
                    && !nextChargedBase[static_cast<std::size_t>(baseIndex)])
                {
                    nextChargedBase[static_cast<std::size_t>(baseIndex)] = true;
                    uniqueFragmentBytes += static_cast<quint64>(
                        baseFragments_[static_cast<std::size_t>(baseIndex)].size());
                }
            }
            else if (static_cast<std::size_t>(reference) < nextChargedPool.size()
                && !nextChargedPool[static_cast<std::size_t>(reference)])
            {
                nextChargedPool[static_cast<std::size_t>(reference)] = true;
                uniqueFragmentBytes += static_cast<quint64>(
                    pool_[static_cast<std::size_t>(reference)].size());
            }
        }
        const quint64 perStateCharge = std::min(
            stateBytes, kHistoryPerStateReconstructionCharge);
        if (uniqueFragmentBytes > std::numeric_limits<quint64>::max()
            - perStateCharge)
        {
            throw std::runtime_error(
                "Recovery history cumulative Level reconstruction exceeds its bounded work budget.");
        }
        const quint64 workCharge = uniqueFragmentBytes + perStateCharge;
        if (workCharge > kMaximumHistoryReconstructionBytesAbsolute
            || reconstructedBytes_ > kMaximumHistoryReconstructionBytesAbsolute - workCharge)
        {
            throw std::runtime_error(
                "Recovery history cumulative Level reconstruction exceeds its bounded work budget.");
        }
        const QCborArray record = makeRecord(stackState, references);
        QCborArray manifest{};
        manifest.append(QString::fromUtf8(label.data(), static_cast<qsizetype>(label.size())));
        manifest.append(static_cast<qint64>(dirtyFlags));
        manifest.append(selectionToCbor(state.selection));
        manifest.append(record);
        // Omit the optional trailing field entirely when no exact structural
        // target exists.  This keeps ordinary V1 timelines byte-identical and
        // leaves one canonical spelling for the absent case.
        if (navigationSelection)
        {
            manifest.append(selectionToCbor(*navigationSelection));
        }

        QCborArray& group = stack == editor::DocumentRecoveryHistoryStack::Undo
            ? pendingUndo_ : pendingRedo_;
        QCborArray& groups = stack == editor::DocumentRecoveryHistoryStack::Undo
            ? undoGroups_ : redoGroups_;
        group.append(manifest);
        if (group.size() == kHistoryEntriesPerGroup)
        {
            flushGroup(group, groups);
        }

        if ( preparedJklCogImportState )
        {
            std::vector<SparseJklCogImportState>& importStates =
                stack == editor::DocumentRecoveryHistoryStack::Undo
                ? undoJklCogImportStates_ : redoJklCogImportStates_;
            const std::uint64_t referenceCount = static_cast<std::uint64_t>(
                preparedJklCogImportState->state.surfaceRefs.size());
            importStates.push_back(std::move(*preparedJklCogImportState));
            jklCogImportReferenceCount_ += referenceCount;
        }

        // Everything that can allocate or reject this entry has succeeded.
        // Commit the directional frame/reference cache and work-accounting
        // state with noexcept moves. If any earlier operation threw, the scope
        // guard removed newly interned fragments and the prior stack remains
        // a complete canonical state for a caller-side retry.
        ++entryCount_;
        stackState.previous = std::move(references);
        ++stackState.stateCount;
        stackState.frames = std::move(nextFrames);
        stackState.trailingFraming = currentTrailingFraming;
        chargedBaseFragments_ = std::move(nextChargedBase);
        chargedPoolFragments_ = std::move(nextChargedPool);
        reconstructedBytes_ += workCharge;

        // Do not advance either directional cache until every allocation,
        // bound check, fragment reference, and manifest update above has
        // succeeded. A failed entry aborts serialization with the prior cache
        // still describing the last committed state.
        if (stack == editor::DocumentRecoveryHistoryStack::Undo)
        {
            undoRequiredSections_ = std::move(generated.requiredSections);
        }
        else
        {
            redoRequiredSections_ = std::move(generated.requiredSections);
        }
        poolCommitted = true;
        // Binding state participates in the same failure-atomic commit as
        // the stack's frame/reference cache. In particular, the sector-layout
        // retry must not inherit a digest from an entry that later failed to
        // allocate or validate its manifest.
        if (preparedBinding)
        {
            bindingDigests_.push_back(std::move(*preparedBinding));
        }
        QByteArray& lastStateDigest =
            stack == editor::DocumentRecoveryHistoryStack::Undo
                ? lastUndoStateDigest_ : lastRedoStateDigest_;
        lastStateDigest = std::move(preparedLastStateDigest);
    }

    QByteArray finish()
    {
        if (bindingDigests_.size() != bindingRequests_.size())
        {
            throw std::runtime_error(
                "Workspace history binding refers to a missing HIST transition.");
        }
        flushGroup(pendingUndo_, undoGroups_);
        flushGroup(pendingRedo_, redoGroups_);

        QCborArray encodedPool{};
        for (const QByteArray& fragment : pool_)
        {
            encodedPool.append(fragment);
        }

        // This is the current unreleased V1 HIST schema. The decoder also
        // retains the previous seven-key fixed-block shape for compatibility;
        // the explicit layout field prevents either shape from being guessed.
        // The former {version, undo:[complete Level bytes], redo:[...]} shape
        // is intentionally not migrated or accepted.
        QCborMap root{};
        root.insert(QStringLiteral("version"), 1);
        root.insert(QStringLiteral("encoding"), QString::fromLatin1(kHistoryEncoding));
        root.insert(QString::fromLatin1(kHistoryFragmentLayoutKey),
            QString::fromLatin1(fragmentLayout_ == HistoryFragmentLayout::SectorRecordsV1
                ? kHistorySectorRecordFragmentLayout
                : kHistoryFixedFragmentLayout));
        root.insert(QStringLiteral("baseDigest"), baseDigest_);
        root.insert(QStringLiteral("baseCount"),
            static_cast<qint64>(baseFragments_.size()));
        root.insert(QStringLiteral("pool"), encodedPool);
        root.insert(QStringLiteral("undoGroups"), undoGroups_);
        root.insert(QStringLiteral("redoGroups"), redoGroups_);
        const bool hasJklCogImportMetadata =
            currentJklCogImportState_.cogRowsPresent
            || !currentJklCogImportState_.surfaceRefs.empty()
            || !undoJklCogImportStates_.empty()
            || !redoJklCogImportStates_.empty();
        if ( hasJklCogImportMetadata )
        {
            QCborMap importHistory{};
            importHistory.insert(QStringLiteral("version"), 1);
            importHistory.insert(QStringLiteral("current"),
                jklCogImportStateToCbor(currentJklCogImportState_));
            importHistory.insert(QStringLiteral("undo"),
                sparseJklCogImportStatesToCbor(undoJklCogImportStates_));
            importHistory.insert(QStringLiteral("redo"),
                sparseJklCogImportStatesToCbor(redoJklCogImportStates_));
            root.insert(QStringLiteral("jklCogImportV1"), importHistory);
        }
        const QByteArray encoded = serializeCanonicalCbor(QCborValue(root));
        if (encoded.isEmpty()
            || encoded.size() > kMaximumHistoryPayloadBytes)
        {
            throw std::runtime_error(
                "Recovery history CBOR exceeds the bounded input safety limit.");
        }
        if (reconstructedBytes_ > historyReconstructionBudget(encoded.size()))
        {
            throw std::runtime_error(
                "Recovery history cumulative Level reconstruction exceeds the encoded-input work budget.");
        }
        return encoded;
    }

    const std::vector<WorkspaceHistoryBindingDigest>& bindingDigests() const noexcept
    {
        return bindingDigests_;
    }

    [[nodiscard]] detail::RecoveryHistoryEncodeStats stats() const noexcept
    {
        return detail::RecoveryHistoryEncodeStats{
            reusedFrameCount_, fragmentedFrameCount_, fragmentedFrameBytes_,
            reusedFramesBySection_, fragmentedFramesBySection_,
            fragmentedBytesBySection_, resynchronizedPrefixBytesBySection_,
            resynchronizedSuffixBytesBySection_, recordReusedBytesBySection_,
            directRecordReusedBytesBySection_ };
    }

private:
    struct FrameState
    {
        std::shared_ptr<const std::vector<std::byte>> owner{};
        std::vector<HistoryReference> references{};
        std::vector<HistoryFragmentSpan> spans{};
    };

    struct StackState
    {
        std::vector<HistoryReference> previous{};
        qsizetype stateCount{};
        std::array<FrameState, 8> frames{};
        QByteArray trailingFraming{};
    };

    static QCborArray referenceArray(const std::span<const HistoryReference> references)
    {
        QCborArray array{};
        for (const HistoryReference reference : references)
        {
            array.append(static_cast<qint64>(reference));
        }
        return array;
    }

    static QCborArray checkpointRecord(const std::span<const HistoryReference> references)
    {
        QCborArray record{};
        record.append(kHistoryRecordVersion);
        record.append(kHistoryRecordCheckpoint);
        record.append(referenceArray(references));
        return record;
    }

    static QCborArray deltaRecord(
        const std::span<const HistoryReference> previous,
        const std::span<const HistoryReference> references)
    {
        qsizetype prefix{};
        while (prefix < static_cast<qsizetype>(previous.size())
            && prefix < static_cast<qsizetype>(references.size())
            && previous[static_cast<std::size_t>(prefix)]
                == references[static_cast<std::size_t>(prefix)])
        {
            ++prefix;
        }

        qsizetype suffix{};
        while (suffix < static_cast<qsizetype>(previous.size()) - prefix
            && suffix < static_cast<qsizetype>(references.size()) - prefix
            && previous[previous.size() - 1u - static_cast<std::size_t>(suffix)]
                == references[references.size() - 1u - static_cast<std::size_t>(suffix)])
        {
            ++suffix;
        }

        const qsizetype eraseCount = static_cast<qsizetype>(previous.size())
            - prefix - suffix;
        const qsizetype insertEnd = static_cast<qsizetype>(references.size())
            - suffix;
        QCborArray inserted{};
        for (qsizetype index = prefix; index < insertEnd; ++index)
        {
            inserted.append(static_cast<qint64>(
                references[static_cast<std::size_t>(index)]));
        }

        QCborArray record{};
        record.append(kHistoryRecordVersion);
        record.append(kHistoryRecordDelta);
        record.append(static_cast<qint64>(prefix));
        record.append(static_cast<qint64>(suffix));
        record.append(static_cast<qint64>(eraseCount));
        record.append(inserted);
        return record;
    }

    QCborArray makeRecord(
        const StackState& stackState,
        const std::vector<HistoryReference>& references)
    {
        const QCborArray full = checkpointRecord(references);
        const bool checkpointRequired = stackState.stateCount != 0
            && stackState.stateCount % kHistoryCheckpointPeriod == 0;
        if (stackState.stateCount == 0)
        {
            const QCborArray delta = deltaRecord(stackState.previous, references);
            if (serializeCanonicalCbor(QCborValue(delta)).size()
                < serializeCanonicalCbor(QCborValue(full)).size())
            {
                return delta;
            }
            return full;
        }
        if (checkpointRequired)
        {
            return full;
        }

        const QCborArray delta = deltaRecord(stackState.previous, references);
        // Comparing the canonical CBOR records (rather than an estimate based
        // on integer widths) makes the checkpoint/delta choice deterministic
        // and gives the decoder a precise canonicality rule to enforce.
        if (serializeCanonicalCbor(QCborValue(delta)).size()
            >= serializeCanonicalCbor(QCborValue(full)).size())
        {
            return full;
        }
        return delta;
    }

    HistoryReference referenceFor(const QByteArray& fragment)
    {
        const std::size_t hash = historyPoolHash(fragment);
        const auto baseIt = baseByHash_.find(hash);
        if (baseIt != baseByHash_.end())
        {
            for (const qsizetype index : baseIt->second)
            {
                if (baseFragments_[static_cast<std::size_t>(index)] == fragment)
                {
                    return -1 - static_cast<HistoryReference>(index);
                }
            }
        }
        return intern(fragment);
    }

    const QByteArray& resolveReference(const HistoryReference reference) const
    {
        if (reference < 0)
        {
            const HistoryReference index = -1 - reference;
            if (index < 0 || index >= static_cast<HistoryReference>(baseFragments_.size()))
            {
                throw std::runtime_error("Recovery history base reference is out of range.");
            }
            return baseFragments_[static_cast<std::size_t>(index)];
        }
        if (reference >= static_cast<HistoryReference>(pool_.size()))
        {
            throw std::runtime_error("Recovery history pool reference is out of range.");
        }
        return pool_[static_cast<std::size_t>(reference)];
    }

    HistoryReference intern(const QByteArray& fragment)
    {
        const std::size_t hash = historyPoolHash(fragment);
        auto& bucket = poolByHash_[hash];
        for (const qsizetype index : bucket)
        {
            if (pool_[static_cast<std::size_t>(index)] == fragment)
            {
                return index;
            }
        }
        if (pool_.size() >= static_cast<std::size_t>(kMaximumHistoryPoolItems))
        {
            throw std::runtime_error("Recovery history fragment pool exceeds the safety limit.");
        }
        const quint64 fragmentBytes = static_cast<quint64>(fragment.size());
        if (fragmentBytes > kMaximumHistoryPoolBytes
            || poolBytes_ > kMaximumHistoryPoolBytes - fragmentBytes)
        {
            throw std::runtime_error(
                "Recovery history cumulative fragment pool exceeds the safety limit.");
        }
        const HistoryReference index = static_cast<HistoryReference>(pool_.size());
        pool_.push_back(fragment);
        try
        {
            chargedPoolFragments_.push_back(false);
            try
            {
                bucket.push_back(index);
            }
            catch (...)
            {
                chargedPoolFragments_.pop_back();
                throw;
            }
        }
        catch (...)
        {
            pool_.pop_back();
            throw;
        }
        poolBytes_ += fragmentBytes;
        return index;
    }

    void rollbackPoolTo(const std::size_t retained) noexcept
    {
        while (pool_.size() > retained)
        {
            const std::size_t index = pool_.size() - 1u;
            const QByteArray& fragment = pool_.back();
            const std::size_t hash = historyPoolHash(fragment);
            const auto found = poolByHash_.find(hash);
            if (found != poolByHash_.end())
            {
                auto& bucket = found->second;
                if (!bucket.empty()
                    && bucket.back() == static_cast<qsizetype>(index))
                {
                    bucket.pop_back();
                }
                if (bucket.empty()) poolByHash_.erase(found);
            }
            poolBytes_ -= static_cast<quint64>(fragment.size());
            pool_.pop_back();
            chargedPoolFragments_.pop_back();
        }
    }

    static void flushGroup(QCborArray& pending, QCborArray& destination)
    {
        if (pending.isEmpty())
        {
            return;
        }
        destination.append(pending);
        pending = QCborArray{};
    }

    std::vector<QByteArray> pool_{};
    std::unordered_map<std::size_t, std::vector<qsizetype>> poolByHash_{};
    std::vector<QByteArray> baseFragments_{};
    std::vector<HistoryReference> baseReferences_{};
    std::vector<HistoryReference> prefixReferences_{};
    QByteArray baseHeader_{};
    QByteArray trailingFraming_{};
    std::unordered_map<std::size_t, std::vector<qsizetype>> baseByHash_{};
    QByteArray baseDigest_{};
    ProjectType baseProjectType_{ ProjectType::JKDF2 };
    assets::SedJklCogImportState currentJklCogImportState_{};
    std::vector<SparseJklCogImportState> undoJklCogImportStates_{};
    std::vector<SparseJklCogImportState> redoJklCogImportStates_{};
    std::uint64_t jklCogImportReferenceCount_{};
    bool baseConfigured_{};
    QCborArray undoGroups_{};
    QCborArray redoGroups_{};
    QCborArray pendingUndo_{};
    QCborArray pendingRedo_{};
    StackState undoState_{};
    StackState redoState_{};
    assets::SedGeneratedUnprotectedRequiredSections undoRequiredSections_{};
    assets::SedGeneratedUnprotectedRequiredSections redoRequiredSections_{};
    qsizetype entryCount_{};
    quint64 poolBytes_{};
    quint64 reconstructedBytes_{};
    std::vector<bool> chargedBaseFragments_{};
    std::vector<bool> chargedPoolFragments_{};
    HistoryFragmentLayout fragmentLayout_{
        HistoryFragmentLayout::SectorRecordsV1};
    quint64 reusedFrameCount_{};
    quint64 fragmentedFrameCount_{};
    quint64 fragmentedFrameBytes_{};
    std::array<quint64, 8> reusedFramesBySection_{};
    std::array<quint64, 8> fragmentedFramesBySection_{};
    std::array<quint64, 8> fragmentedBytesBySection_{};
    std::array<quint64, 8> resynchronizedPrefixBytesBySection_{};
    std::array<quint64, 8> resynchronizedSuffixBytesBySection_{};
    std::array<quint64, 8> recordReusedBytesBySection_{};
    std::array<quint64, 8> directRecordReusedBytesBySection_{};
    std::vector<WorkspaceHistoryPosition> bindingRequests_{};
    std::vector<WorkspaceHistoryBindingDigest> bindingDigests_{};
    QByteArray lastUndoStateDigest_{};
    QByteArray lastRedoStateDigest_{};
};

struct RecoveryHistoryDecodeStats
{
    std::size_t undoEntries{};
    std::size_t redoEntries{};
};

bool decodeRecoveryHistory(
    const QByteArray& payload,
    ProjectType projectType,
    const Level* currentLevel,
    editor::DocumentRecoveryHistory* history,
    RecoveryHistoryDecodeStats* stats,
    QString* error,
    quint64 maximumRetainedLevelBytes,
    quint64 maximumRetainedLevelItems,
    quint64 maximumReconstructionBytes,
    bool compactOutput,
    const std::vector<WorkspaceHistoryBindingDigest>* expectedWorkspaceBindings = nullptr,
    bool* workspaceBindingsValid = nullptr,
    WorkspaceHistoryAttachmentLevelValidator attachmentValidator = nullptr);

bool preflightRecoveryHistoryCbor(const QByteArray& payload, QString* error);

QByteArray validateGeneratedRecoveryHistoryEnvelope(
    QByteArray payload,
    const std::size_t expectedUndoEntries,
    const std::size_t expectedRedoEntries)
{
    QString preflightError{};
    if (!preflightRecoveryHistoryCbor(payload, &preflightError))
    {
        throw std::runtime_error(preflightError.isEmpty()
            ? "Generated recovery history failed bounded canonical framing validation."
            : preflightError.toStdString());
    }

    QCborParserError parseError{};
    const QCborValue rootValue = QCborValue::fromCbor(payload, &parseError);
    if (parseError.error != QCborError::NoError || !rootValue.isMap()
        || rootValue.toCbor(QCborValue::NoTransformation) != payload)
    {
        throw std::runtime_error(
            "Generated recovery history failed canonical CBOR validation.");
    }

    const QCborMap root = rootValue.toMap();
    const auto countEntries = [](const QCborValue& value)
        -> std::optional<std::size_t>
    {
        if (!value.isArray())
        {
            return std::nullopt;
        }
        std::size_t count{};
        for (const QCborValue& groupValue : value.toArray())
        {
            if (!groupValue.isArray())
            {
                return std::nullopt;
            }
            const qsizetype groupSize = groupValue.toArray().size();
            if (groupSize <= 0
                || groupSize > kHistoryEntriesPerGroup
                || count > MaximumPersistedHistoryEntries
                    - static_cast<std::size_t>(groupSize))
            {
                return std::nullopt;
            }
            count += static_cast<std::size_t>(groupSize);
        }
        return count;
    };
    const auto undoCount = countEntries(
        root.value(QStringLiteral("undoGroups")));
    const auto redoCount = countEntries(
        root.value(QStringLiteral("redoGroups")));
    if (!undoCount || !redoCount
        || *undoCount != expectedUndoEntries
        || *redoCount != expectedRedoEntries
        || *undoCount > MaximumPersistedHistoryEntries - *redoCount)
    {
        throw std::runtime_error(
            "Generated recovery history entry framing exceeds the persistence contract.");
    }
    return payload;
}

QByteArray validateGeneratedRecoveryHistory(
    QByteArray payload,
    const ProjectType projectType,
    const Level& currentLevel,
    const std::size_t expectedUndoEntries,
    const std::size_t expectedRedoEntries,
    const quint64 maximumRetainedLevelBytes =
        kMaximumHistoryRetainedLevelBytes,
    const quint64 maximumRetainedLevelItems = kMaximumHistoryRetainedLevelItems)
{
    RecoveryHistoryDecodeStats stats{};
    editor::DocumentRecoveryHistory compactHistory{};
    QString error{};
    if (!decodeRecoveryHistory(
            payload,
            projectType,
            &currentLevel,
            &compactHistory,
            &stats,
            &error,
            maximumRetainedLevelBytes,
            maximumRetainedLevelItems,
            kMaximumHistoryReconstructionBytesAbsolute,
            true)
        || stats.undoEntries != expectedUndoEntries
        || stats.redoEntries != expectedRedoEntries)
    {
        throw std::runtime_error(error.isEmpty()
            ? "Generated recovery history failed semantic round-trip validation."
            : error.toStdString());
    }
    return payload;
}

QByteArray serializeRecoveryHistoryWithLayout(
    const Level& currentLevel,
    const editor::DocumentRecoveryHistory& history,
    const HistoryFragmentLayout fragmentLayout)
{
    RecoveryHistoryEncoder encoder(currentLevel, fragmentLayout);
    const Level* undoPostLevel = &currentLevel;
    for (const editor::DocumentRecoveryHistoryEntry& entry : history.undoEntries)
    {
        if (entry.navigationSelection
            && !selectionReferencesLevel(*entry.navigationSelection, *undoPostLevel))
        {
            throw std::runtime_error(
                "Recovery undo history navigation selection references a missing or incompatible adjacent Level object.");
        }
        encoder.add(editor::DocumentRecoveryHistoryStack::Undo,
            entry.label, entry.dirtyFlags, entry.state, nullptr,
            assets::SedRequiredSection::None,
            entry.navigationSelection ? &*entry.navigationSelection : nullptr);
        undoPostLevel = &entry.state.level;
    }
    for (const editor::DocumentRecoveryHistoryEntry& entry : history.redoEntries)
    {
        if (entry.navigationSelection
            && !selectionReferencesLevel(
                *entry.navigationSelection, entry.state.level))
        {
            throw std::runtime_error(
                "Recovery redo history navigation selection references a missing or incompatible adjacent Level object.");
        }
        encoder.add(editor::DocumentRecoveryHistoryStack::Redo,
            entry.label, entry.dirtyFlags, entry.state, nullptr,
            assets::SedRequiredSection::None,
            entry.navigationSelection ? &*entry.navigationSelection : nullptr);
    }
    const ProjectType projectType = currentLevel.kind;
    return validateGeneratedRecoveryHistory(
        encoder.finish(), projectType, currentLevel,
        history.undoEntries.size(), history.redoEntries.size());
}

QByteArray serializeRecoveryHistory(
    const Level& currentLevel,
    const editor::DocumentRecoveryHistory& history)
{
    try
    {
        return serializeRecoveryHistoryWithLayout(
            currentLevel, history, HistoryFragmentLayout::SectorRecordsV1);
    }
    catch (const HistorySectorLayoutLimitExceeded&)
    {
        return serializeRecoveryHistoryWithLayout(
            currentLevel, history, HistoryFragmentLayout::FixedBlocksV1);
    }
}

void normalizeHistoricalThingFrameState(editor::DocumentState& state)
{
    struct SelectedFrame
    {
        std::int32_t thing{};
        std::size_t ordinal{};
    };
    std::vector<SelectedFrame> selectedFrames{};
    selectedFrames.reserve(state.selection.frames.size());
    for (const level::FrameRef& ref : state.selection.frames)
    {
        if (ref.thing < 0 || ref.value < 0
            || static_cast<std::size_t>(ref.thing) >= state.level.things.size())
        {
            throw std::runtime_error(
                "Historical frame selection references a missing Thing/value.");
        }
        const Thing& thing = state.level.things[static_cast<std::size_t>(ref.thing)];
        if (static_cast<std::size_t>(ref.value) >= thing.values.size())
        {
            throw std::runtime_error(
                "Historical frame selection references a missing Thing/value.");
        }
        const TplValue& selected = thing.values[static_cast<std::size_t>(ref.value)];
        if (selected.type != ValueType::frame
            || !text::iequalsAscii(selected.name, "frame"))
        {
            throw std::runtime_error(
                "Historical frame selection does not reference a frame value.");
        }
        std::size_t ordinal{};
        for (std::size_t index = 0;
             index < static_cast<std::size_t>(ref.value);
             ++index)
        {
            const TplValue& value = thing.values[index];
            if (value.type == ValueType::frame
                && text::iequalsAscii(value.name, "frame"))
            {
                ++ordinal;
            }
        }
        selectedFrames.push_back(SelectedFrame{ ref.thing, ordinal });
    }

    for (Thing& thing : state.level.things)
    {
        ::sed::level::thingSyncNumFrames(thing);
    }

    state.selection.frames.clear();
    for (const SelectedFrame& selected : selectedFrames)
    {
        const Thing& thing = state.level.things[
            static_cast<std::size_t>(selected.thing)];
        std::size_t ordinal{};
        std::optional<std::int32_t> valueIndex{};
        for (std::size_t index = 0; index < thing.values.size(); ++index)
        {
            const TplValue& value = thing.values[index];
            if (value.type != ValueType::frame
                || !text::iequalsAscii(value.name, "frame"))
            {
                continue;
            }
            if (ordinal == selected.ordinal)
            {
                if (index > static_cast<std::size_t>(
                        std::numeric_limits<std::int32_t>::max()))
                {
                    throw std::runtime_error(
                        "Historical frame value index exceeds the supported range.");
                }
                valueIndex = static_cast<std::int32_t>(index);
                break;
            }
            ++ordinal;
        }
        if (!valueIndex)
        {
            throw std::runtime_error(
                "Historical frame selection could not be remapped after numframes normalization.");
        }
        state.selection.frames.insert(level::FrameRef{
            selected.thing, *valueIndex });
    }
}

bool historicalThingFrameStateNeedsNormalization(const Thing& thing) noexcept
{
    std::size_t frameCount{};
    std::size_t numFramesCount{};
    std::size_t firstFrame = thing.values.size();
    std::size_t numFramesIndex = thing.values.size();
    const TplValue* numFrames{};
    for (std::size_t index = 0; index < thing.values.size(); ++index)
    {
        const TplValue& value = thing.values[index];
        if (value.type == ValueType::frame
            && text::iequalsAscii(value.name, "frame"))
        {
            if (firstFrame == thing.values.size()) firstFrame = index;
            ++frameCount;
        }
        if (text::iequalsAscii(value.name, "numframes"))
        {
            if (!numFrames) numFrames = &value;
            numFramesIndex = index;
            ++numFramesCount;
        }
    }
    if (frameCount == 0)
    {
        return numFramesCount != 0;
    }
    return numFramesCount != 1
        || numFramesIndex + 1 != firstFrame
        || !numFrames
        || numFrames->name != "numframes"
        || numFrames->type != ValueType::integer
        || numFrames->intValue != static_cast<std::int32_t>(frameCount)
        || numFrames->floatValue != 0.0
        || !numFrames->stringValue.empty();
}

QByteArray serializeRecoveryHistoryWithLayout(
    const editor::Document& document,
    const bool normalizeHistoricalThingFrameCounts,
    const bool semanticRoundTripValidation,
    detail::RecoveryHistoryEncodeStats* const encodeStats,
    const HistoryFragmentLayout fragmentLayout,
    const std::vector<WorkspaceHistoryPosition>* const bindingRequests = nullptr,
    std::vector<WorkspaceHistoryBindingDigest>* const bindingDigests = nullptr)
{
    RecoveryHistoryEncoder encoder(
        document.current().level,
        fragmentLayout,
        bindingRequests ? *bindingRequests
                        : std::vector<WorkspaceHistoryPosition>{});
    const bool visited = document.visitRecoveryHistoryTransitions(
        [&encoder, normalizeHistoricalThingFrameCounts](
            const editor::DocumentRecoveryHistoryStack stack,
            const std::string_view label,
            const std::uint32_t dirtyFlags,
            const editor::DocumentHistoryTransitionDomain,
            const assets::SedHistoryTransitionToken& transition,
            editor::DocumentState& state)
        {
            assets::SedRequiredSection additionalSections =
                assets::SedRequiredSection::None;
            if (normalizeHistoricalThingFrameCounts)
            {
                // Manual SED Save canonicalizes the top-level Thing frame
                // counts. Apply the same derived-field normalization to each
                // state in this temporary history walk, without retaining a
                // second expanded history vector or mutating the live document.
                const bool needsNormalization = std::any_of(
                    state.level.things.begin(), state.level.things.end(),
                    historicalThingFrameStateNeedsNormalization);
                if (needsNormalization)
                {
                    // This derived manual-Save edit changes the serialized
                    // Thing values even when the underlying compact transition
                    // is a COG/light/geometry-only record.
                    additionalSections |= assets::SedRequiredSection::Things;
                    // Normalization changes value indices. Apply it to a
                    // Things+selection copy for this encoded state only: the
                    // temporary history walker must remain byte-exact for the
                    // next compact COG/light/geometry swap, whose snapshot
                    // does not own the Thing vector.
                    editor::DocumentState normalized{};
                    normalized.level.things = state.level.things;
                    normalized.selection = state.selection;
                    normalizeHistoricalThingFrameState(normalized);
                    state.level.things.swap(normalized.level.things);
                    std::swap(state.selection, normalized.selection);
                    try
                    {
                        encoder.add(stack, label, dirtyFlags, state,
                            &transition, additionalSections);
                    }
                    catch (...)
                    {
                        state.level.things.swap(normalized.level.things);
                        std::swap(state.selection, normalized.selection);
                        throw;
                    }
                    state.level.things.swap(normalized.level.things);
                    std::swap(state.selection, normalized.selection);
                    return true;
                }
            }
            encoder.add(stack, label, dirtyFlags, state, &transition,
                additionalSections);
            return true;
        });
    if (!visited)
    {
        throw std::runtime_error("Recovery history walk terminated unexpectedly.");
    }
    QByteArray payload = encoder.finish();
    if (bindingDigests)
    {
        *bindingDigests = encoder.bindingDigests();
    }
    if (encodeStats) *encodeStats = encoder.stats();
    if (!semanticRoundTripValidation)
    {
        // Manual Save writes the authoritative current Level independently of
        // optional HIST. The encoder has just consumed the already-valid live
        // compact timeline; repeat the bounded/canonical envelope and exact
        // count checks, but avoid deserializing every historical Level a
        // second time at the 70-80% save stall. The full semantic decoder is
        // still exercised on Open and by the ordinary/recovery serializer.
        return validateGeneratedRecoveryHistoryEnvelope(
            std::move(payload), document.undoDepth(), document.redoDepth());
    }
    return validateGeneratedRecoveryHistory(
        std::move(payload), document.level().kind, document.current().level,
        document.undoDepth(), document.redoDepth());
}

QByteArray serializeRecoveryHistory(
    const editor::Document& document,
    const bool normalizeHistoricalThingFrameCounts,
    const bool semanticRoundTripValidation,
    detail::RecoveryHistoryEncodeStats* const encodeStats = nullptr,
    const std::vector<WorkspaceHistoryPosition>* const bindingRequests = nullptr,
    std::vector<WorkspaceHistoryBindingDigest>* const bindingDigests = nullptr)
{
    try
    {
        return serializeRecoveryHistoryWithLayout(
            document, normalizeHistoricalThingFrameCounts,
            semanticRoundTripValidation, encodeStats,
            HistoryFragmentLayout::SectorRecordsV1,
            bindingRequests, bindingDigests);
    }
    catch (const HistorySectorLayoutLimitExceeded&)
    {
        return serializeRecoveryHistoryWithLayout(
            document, normalizeHistoricalThingFrameCounts,
            semanticRoundTripValidation, encodeStats,
            HistoryFragmentLayout::FixedBlocksV1,
            bindingRequests, bindingDigests);
}
}

QByteArray serializeRecoveryHistory(const editor::Document& document)
{
    return serializeRecoveryHistory(document, false, true, nullptr);
}

bool preflightRecoveryHistoryCbor(const QByteArray& payload, QString* error)
{
    const auto fail = [error](const QString& message)
    {
        if (error) *error = message;
        return false;
    };
    constexpr RecoveryCborLimits limits{
        assets::SedMaximumHistorySectionBytes,
        kMaximumHistoryCborNodes,
        kMaximumHistoryCborDepth,
        kMaximumHistoryTextBytes,
        static_cast<quint64>(kHistoryChunkMaximumBytes),
        assets::SedMaximumDecodedObjectBytes,
    };
    if (!preflightRecoveryCbor(payload, limits))
    {
        return fail(QStringLiteral(
            "Recovery undo history canonical CBOR exceeds the bounded node/depth/string/materialization limits, map-key order, or single-value framing."));
    }
    return true;
}

bool dirtyFlagsCanRepresentLevelTransition(
    const Level& from,
    const Level& to,
    const std::uint32_t dirtyFlags)
{
    constexpr std::uint32_t things =
        static_cast<std::uint32_t>(editor::DocumentDirty::Things);
    constexpr std::uint32_t lights =
        static_cast<std::uint32_t>(editor::DocumentDirty::Lights);
    constexpr std::uint32_t cogs =
        static_cast<std::uint32_t>(editor::DocumentDirty::Cogs);
    const bool partialThings = (dirtyFlags & things) != 0;
    const bool partialLights = (dirtyFlags & lights) != 0;
    if (dirtyFlags == cogs)
    {
        return assets::sedLevelsEqualIgnoringHistoryDomains(
            from, to, false, false, true);
    }
    if (dirtyFlags != 0 && (dirtyFlags & ~(things | lights)) == 0
        && (partialThings || partialLights))
    {
        return assets::sedLevelsEqualIgnoringHistoryDomains(
            from, to, partialThings, partialLights, false);
    }
    else
    {
        // This mirrors DocumentUndoHistory's conservative fallback. Zero,
        // Structure (alone or combined), All, mixed COG/domain masks, and
        // masks containing any other full-domain bit create a FullSnapshot.
        // Pure LevelGeometry is topology-preserving for transform records;
        // compact import separately verifies its changed Sector subset.
        return true;
    }
}

bool sameHistoryBox(const TBox& lhs, const TBox& rhs) noexcept
{
    return lhs.x1 == rhs.x1 && lhs.y1 == rhs.y1 && lhs.z1 == rhs.z1
        && lhs.x2 == rhs.x2 && lhs.y2 == rhs.y2 && lhs.z2 == rhs.z2;
}

bool sameHistoryVertex(const Vertex& lhs, const Vertex& rhs) noexcept
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z
        && lhs.mark == rhs.mark;
}

bool sameHistoryTexVertex(const TexVertex& lhs, const TexVertex& rhs) noexcept
{
    return lhs.u == rhs.u && lhs.v == rhs.v && lhs.color == rhs.color;
}

bool sameHistorySurface(const Surface& lhs, const Surface& rhs) noexcept
{
    return lhs.num == rhs.num
        && lhs.mark == rhs.mark
        && lhs.d3dId == rhs.d3dId
        && lhs.surfaceFlags == rhs.surfaceFlags
        && lhs.faceFlags == rhs.faceFlags
        && lhs.geo == rhs.geo
        && lhs.light == rhs.light
        && lhs.tex == rhs.tex
        && lhs.extraLight == rhs.extraLight
        && lhs.adjoinFlags == rhs.adjoinFlags
        && lhs.adjoin == rhs.adjoin
        && lhs.material == rhs.material
        && lhs.nAdj == rhs.nAdj
        && lhs.nMat == rhs.nMat
        && lhs.uScale == rhs.uScale
        && lhs.vScale == rhs.vScale
        && lhs.normal == rhs.normal
        && lhs.vertices == rhs.vertices
        && lhs.texVertices.size() == rhs.texVertices.size()
        && std::equal(lhs.texVertices.begin(), lhs.texVertices.end(),
            rhs.texVertices.begin(), sameHistoryTexVertex);
}

bool sameHistorySector(const Sector& lhs, const Sector& rhs) noexcept
{
    if (lhs.id != rhs.id
        || lhs.num != rhs.num
        || lhs.flags != rhs.flags
        || lhs.ambient != rhs.ambient
        || lhs.extraLight != rhs.extraLight
        || lhs.pointLight.color != rhs.pointLight.color
        || lhs.pointLight.position != rhs.pointLight.position
        || lhs.pointLight.minRange != rhs.pointLight.minRange
        || lhs.pointLight.maxRange != rhs.pointLight.maxRange
        || lhs.colormap != rhs.colormap
        || lhs.tint != rhs.tint
        || lhs.sound != rhs.sound
        || lhs.soundVolume != rhs.soundVolume
        || lhs.thrust != rhs.thrust
        || !sameHistoryBox(lhs.collideBox, rhs.collideBox)
        || lhs.layer != rhs.layer
        || lhs.mark != rhs.mark
        || lhs.vertices.size() != rhs.vertices.size()
        || lhs.surfaces.size() != rhs.surfaces.size())
    {
        return false;
    }
    if (!std::equal(lhs.vertices.begin(), lhs.vertices.end(),
            rhs.vertices.begin(), sameHistoryVertex))
    {
        return false;
    }
    return std::equal(lhs.surfaces.begin(), lhs.surfaces.end(),
        rhs.surfaces.begin(), sameHistorySurface);
}

bool sameHistoryTplValue(const TplValue& lhs, const TplValue& rhs) noexcept
{
    return lhs.name == rhs.name
        && lhs.type == rhs.type
        && lhs.intValue == rhs.intValue
        && lhs.floatValue == rhs.floatValue
        && lhs.stringValue == rhs.stringValue
        && lhs.adjType == rhs.adjType;
}

bool sameHistoryThing(const Thing& lhs, const Thing& rhs) noexcept
{
    return lhs.id == rhs.id
        && lhs.num == rhs.num
        && lhs.sector == rhs.sector
        && lhs.templateName == rhs.templateName
        && lhs.name == rhs.name
        && lhs.x == rhs.x
        && lhs.y == rhs.y
        && lhs.z == rhs.z
        && lhs.pch == rhs.pch
        && lhs.yaw == rhs.yaw
        && lhs.rol == rhs.rol
        && lhs.values.size() == rhs.values.size()
        && std::equal(lhs.values.begin(), lhs.values.end(),
            rhs.values.begin(), sameHistoryTplValue)
        && lhs.layer == rhs.layer
        && sameHistoryBox(lhs.bbox, rhs.bbox);
}

/**
 * Returns the exact owning Sector subset changed by a topology-preserving
 * LevelGeometry transition. A null optional means that another domain also
 * changed or that sector topology changed, so a full snapshot is required.
 */
std::optional<std::vector<std::int32_t>> historyGeometrySubset(
    const Level& from,
    const Level& to)
{
    if (!assets::sedLevelsEqualIgnoringSectors(from, to)
        || from.sectors.size() != to.sectors.size())
    {
        return std::nullopt;
    }

    std::vector<std::int32_t> changed{};
    changed.reserve(to.sectors.size());
    for (std::size_t index = 0; index < to.sectors.size(); ++index)
    {
        if (!sameHistorySector(from.sectors[index], to.sectors[index]))
        {
            if (index > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
            {
                return std::nullopt;
            }
            changed.push_back(static_cast<std::int32_t>(index));
        }
    }
    return changed;
}

/** Returns changed Thing indices when every other Level domain is identical. */
std::optional<std::vector<std::int32_t>> historyThingSubset(
    const Level& from,
    const Level& to)
{
    if (!assets::sedLevelsEqualIgnoringHistoryDomains(
            from, to, true, false, false)
        || from.things.size() != to.things.size())
    {
        return std::nullopt;
    }

    std::vector<std::int32_t> changed{};
    changed.reserve(to.things.size());
    for (std::size_t index = 0; index < to.things.size(); ++index)
    {
        if (!sameHistoryThing(from.things[index], to.things[index]))
        {
            if (index > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
            {
                return std::nullopt;
            }
            changed.push_back(static_cast<std::int32_t>(index));
        }
    }
    return changed;
}

bool decodeRecoveryHistory(
    const QByteArray& payload,
    const ProjectType projectType,
    const Level* currentLevel,
    editor::DocumentRecoveryHistory* history,
    RecoveryHistoryDecodeStats* stats,
    QString* error,
    const quint64 maximumRetainedLevelBytes,
    const quint64 maximumRetainedLevelItems,
    const quint64 maximumReconstructionBytes,
    const bool compactOutput,
    const std::vector<WorkspaceHistoryBindingDigest>* const expectedWorkspaceBindings,
    bool* const workspaceBindingsValid,
    const WorkspaceHistoryAttachmentLevelValidator attachmentValidator)
{
    const auto fail = [error](const QString& message)
    {
        if (error) *error = message;
        return false;
    };
    if (error) error->clear();
    if (workspaceBindingsValid)
    {
        *workspaceBindingsValid = false;
    }

    // Check the raw CBOR envelope before invoking Qt's parser.  The parser
    // allocates a complete object graph before schema validation, so this
    // guard is deliberately the first operation after the error reset.
    if (payload.isEmpty()
        || payload.size() > kMaximumHistoryPayloadBytes)
    {
        return fail(QStringLiteral(
            "Recovery undo history payload exceeds the bounded input safety limit."));
    }
    if (!preflightRecoveryHistoryCbor(payload, error))
    {
        return false;
    }

    QCborParserError parseError{};
    const QCborValue rootValue = QCborValue::fromCbor(payload, &parseError);
    if (parseError.error != QCborError::NoError || !rootValue.isMap())
    {
        return fail(QStringLiteral("Recovery undo history CBOR is malformed."));
    }
    // Qt's convenience parser can successfully decode the first value of a
    // stream that has trailing bytes. The allocation-free preflight above has
    // already bounded the object graph; re-encoding now catches non-canonical
    // encodings before any schema-specific collection is copied or expanded.
    if (rootValue.toCbor(QCborValue::NoTransformation) != payload)
    {
        return fail(QStringLiteral(
            "Recovery undo history CBOR is not a canonical single value."));
    }
    const QCborMap root = rootValue.toMap();
    // Reject the abandoned development-only shape explicitly.  It stored
    // complete Levels under `undo`/`redo` and was never part of the compact
    // base/delta V1 contract; keeping a distinct diagnostic also prevents a
    // future schema extension from accidentally reinterpreting it.
    if (root.size() == 3
        && root.value(QStringLiteral("version")).isInteger()
        && root.value(QStringLiteral("version")).toInteger() == 1
        && root.value(QStringLiteral("undo")).isArray()
        && root.value(QStringLiteral("redo")).isArray())
    {
        return fail(QStringLiteral(
            "Recovery undo history uses an earlier development schema that is unsupported."));
    }
    const QCborValue version = root.value(QStringLiteral("version"));
    const QCborValue encoding = root.value(QStringLiteral("encoding"));
    const QCborValue fragmentLayoutValue = root.value(
        QString::fromLatin1(kHistoryFragmentLayoutKey));
    const QCborValue baseDigestValue = root.value(QStringLiteral("baseDigest"));
    const QCborValue baseCountValue = root.value(QStringLiteral("baseCount"));
    const QCborValue poolValue = root.value(QStringLiteral("pool"));
    const QCborValue undoGroups = root.value(QStringLiteral("undoGroups"));
    const QCborValue redoGroups = root.value(QStringLiteral("redoGroups"));
    const bool hasJklCogImportHistory =
        root.contains(QStringLiteral("jklCogImportV1"));
    const bool legacyFixedLayout = !hasJklCogImportHistory && root.size() == 7
        && fragmentLayoutValue.type() == QCborValue::Undefined;
    const qsizetype explicitRootSize = hasJklCogImportHistory ? 9 : 8;
    const bool explicitFixedLayout = root.size() == explicitRootSize
        && fragmentLayoutValue.isString()
        && fragmentLayoutValue.toString()
            == QString::fromLatin1(kHistoryFixedFragmentLayout);
    const bool explicitSectorLayout = root.size() == explicitRootSize
        && fragmentLayoutValue.isString()
        && fragmentLayoutValue.toString()
            == QString::fromLatin1(kHistorySectorRecordFragmentLayout);
    if ((!legacyFixedLayout && !explicitFixedLayout && !explicitSectorLayout)
        || !version.isInteger() || version.toInteger() != 1
        || !encoding.isString()
        || encoding.toString() != QString::fromLatin1(kHistoryEncoding)
        || !baseDigestValue.isByteArray()
        || baseDigestValue.toByteArray().size()
            != QCryptographicHash::hashLength(QCryptographicHash::Sha256)
        || !baseCountValue.isInteger()
        || !poolValue.isArray() || !undoGroups.isArray() || !redoGroups.isArray())
    {
        return fail(QStringLiteral(
            "Recovery undo history schema or fragment layout is unsupported."));
    }
    const HistoryFragmentLayout fragmentLayout = explicitSectorLayout
        ? HistoryFragmentLayout::SectorRecordsV1
        : HistoryFragmentLayout::FixedBlocksV1;

    std::optional<JklCogImportHistory> jklCogImportHistory{};
    if ( hasJklCogImportHistory )
    {
        if ( !currentLevel )
        {
            return fail(QStringLiteral(
                "Recovery undo history JKL COG import metadata requires the authoritative current Level."));
        }
        JklCogImportHistory decodedImportHistory{};
        QString importHistoryError{};
        if ( !decodeJklCogImportHistory(
                root.value(QStringLiteral("jklCogImportV1")),
                &decodedImportHistory, &importHistoryError) )
        {
            return fail(importHistoryError.isEmpty()
                ? QStringLiteral(
                    "Recovery undo history JKL COG import extension is malformed.")
                : importHistoryError);
        }
        if ( !sameJklCogImportState(
                decodedImportHistory.current,
                representedJklCogImportState(*currentLevel)) )
        {
            return fail(QStringLiteral(
                "Recovery undo history JKL COG import current state does not match the authoritative Level."));
        }
        jklCogImportHistory.emplace(std::move(decodedImportHistory));
    }
    else if ( currentLevel
        && assets::hasRepresentedJklCogImportState(*currentLevel) )
    {
        return fail(QStringLiteral(
            "Recovery undo history omits represented current JKL COG import metadata."));
    }

    const qint64 encodedBaseCount = baseCountValue.toInteger(-1);
    if (encodedBaseCount < 0
        || encodedBaseCount > kMaximumHistoryFragmentsPerState)
    {
        return fail(QStringLiteral("Recovery undo history base fragment count is invalid."));
    }
    std::vector<QByteArray> baseFragments{};
    QByteArray expectedBaseDigest{};
    std::optional<assets::SedNestedHistoryDecoder> baseIncrementalDecoder{};
    if (currentLevel)
    {
        try
        {
            if (compactOutput && history)
            {
                baseIncrementalDecoder.emplace(*currentLevel);
            }
            const std::span<const std::byte> currentBytes = baseIncrementalDecoder
                ? baseIncrementalDecoder->currentBytes()
                : std::span<const std::byte>{};
            const std::vector<std::byte> fallbackCurrentBytes = baseIncrementalDecoder
                ? std::vector<std::byte>{}
                : assets::saveToSedBytes(
                    *currentLevel, {},
                    assets::SedSerializationProfile::HistoryNested);
            const std::span<const std::byte> effectiveCurrentBytes =
                baseIncrementalDecoder
                ? currentBytes
                : std::span<const std::byte>(fallbackCurrentBytes);
            baseFragments = historyContentFragments(
                effectiveCurrentBytes, fragmentLayout);
            QCryptographicHash baseHash(QCryptographicHash::Sha256);
            baseHash.addData(reinterpret_cast<const char*>(
                    effectiveCurrentBytes.data()),
                static_cast<qsizetype>(effectiveCurrentBytes.size()));
            if ( jklCogImportHistory )
            {
                addJklCogImportStateDigest(baseHash,
                    jklCogImportHistory->current);
            }
            expectedBaseDigest = baseHash.result();
        }
        catch (const std::exception& exception)
        {
            return fail(QStringLiteral("Recovery undo history current Level could not be canonicalized: %1")
                .arg(QString::fromUtf8(exception.what())));
        }
        if (baseFragments.size() != static_cast<std::size_t>(encodedBaseCount)
            || expectedBaseDigest != baseDigestValue.toByteArray())
        {
            return fail(QStringLiteral(
                "Recovery undo history base digest or fragment count does not match the current Level."));
        }
    }
    else if (encodedBaseCount != 0)
    {
        return fail(QStringLiteral(
            "Recovery undo history requires the authoritative current Level for base validation."));
    }
    const std::vector<HistoryReference> baseReferences = canonicalBaseReferences(baseFragments);

    const QCborArray encodedPool = poolValue.toArray();
    if (encodedPool.size() > kMaximumHistoryPoolItems)
    {
        return fail(QStringLiteral("Recovery undo history pool exceeds the safety limit."));
    }
    std::vector<QByteArray> pool{};
    pool.reserve(static_cast<std::size_t>(encodedPool.size()));
    std::unordered_map<std::size_t, std::vector<qsizetype>> baseByHash{};
    for (qsizetype index = 0; index < static_cast<qsizetype>(baseFragments.size()); ++index)
    {
        baseByHash[historyPoolHash(baseFragments[static_cast<std::size_t>(index)])]
            .push_back(index);
    }
    quint64 pooledBytes{};
    for (const QCborValue& value : encodedPool)
    {
        if (!value.isByteArray())
        {
            return fail(QStringLiteral("Recovery undo history pool entry is malformed."));
        }
        const QByteArray fragment = value.toByteArray();
        if (fragment.isEmpty()
            || static_cast<quint64>(fragment.size())
                > static_cast<quint64>(kHistoryChunkMaximumBytes)
            || pooledBytes > kMaximumHistoryPoolBytes
            || static_cast<quint64>(fragment.size())
                > kMaximumHistoryPoolBytes - pooledBytes)
        {
            return fail(QStringLiteral(
                "Recovery undo history pool entry or cumulative pool size is invalid."));
        }
        const auto baseIt = baseByHash.find(historyPoolHash(fragment));
        if (baseIt != baseByHash.end())
        {
            for (const qsizetype baseIndex : baseIt->second)
            {
                if (baseFragments[static_cast<std::size_t>(baseIndex)] == fragment)
                {
                    return fail(QStringLiteral(
                        "Recovery undo history pool redundantly stores a current-base fragment."));
                }
            }
        }
        pooledBytes += static_cast<quint64>(fragment.size());
        pool.push_back(fragment);
    }
    // The encoder interns every fragment. Reject redundant pool encodings
    // before reconstructing any Level so a hostile HIST cannot consume its
    // node/materialization allowance with semantically duplicate byte arrays.
    std::vector<std::size_t> poolOrder(pool.size());
    std::iota(poolOrder.begin(), poolOrder.end(), std::size_t{});
    std::sort(poolOrder.begin(), poolOrder.end(), [&pool](
        const std::size_t lhs, const std::size_t rhs)
        {
            return pool[lhs] < pool[rhs];
        });
    for (std::size_t index = 1; index < poolOrder.size(); ++index)
    {
        if (pool[poolOrder[index - 1u]] == pool[poolOrder[index]])
        {
            return fail(QStringLiteral(
                "Recovery undo history fragment pool contains duplicate entries."));
        }
    }
    poolOrder.clear();
    poolOrder.shrink_to_fit();

    if (history)
    {
        history->undoEntries.clear();
        history->redoEntries.clear();
        history->compact.reset();
        if (compactOutput)
        {
            history->compact = std::make_shared<editor::DocumentUndoHistory>();
        }
    }
    RecoveryHistoryDecodeStats localStats{};
    std::size_t totalDecodedCount{};
    const quint64 reconstructionBudget = std::min(
        historyReconstructionBudget(payload.size()),
        maximumReconstructionBytes);
    quint64 reconstructedBytes{};
    assets::SedLevelDecodedFootprint retainedLevelFootprint{};
    bool workspaceBindingHashesValid = expectedWorkspaceBindings != nullptr;
    std::size_t workspaceBindingsMatched{};
    std::vector<bool> referencedPoolEntries(pool.size(), false);
    std::vector<bool> chargedPoolEntries(pool.size(), false);
    std::vector<bool> chargedBaseEntries(baseFragments.size(), false);
    qsizetype nextFirstUsePoolIndex{};
    QString referenceResolutionError{};
    const auto resolveReference = [&] (const HistoryReference reference)
        -> const QByteArray*
    {
        if (reference >= 0)
        {
            if (reference >= static_cast<HistoryReference>(pool.size())) return nullptr;
            return &pool[static_cast<std::size_t>(reference)];
        }
        if (reference == std::numeric_limits<qint64>::min()) return nullptr;
        const HistoryReference index = -1 - reference;
        if (index < 0 || index >= static_cast<HistoryReference>(baseFragments.size()))
        {
            return nullptr;
        }
        // The encoder canonicalizes duplicate base fragments to the first
        // occurrence. Accepting a later equal index would create multiple
        // byte-identical spellings for the same state and would let a crafted
        // HIST bypass the deterministic first-use/reference invariants.
        if (baseReferences[static_cast<std::size_t>(index)] != reference)
        {
            referenceResolutionError = QStringLiteral(
                "Recovery undo history uses a noncanonical duplicate current-base reference.");
            return nullptr;
        }
        return &baseFragments[static_cast<std::size_t>(index)];
    };
    const auto workspaceBindingAt = [expectedWorkspaceBindings](
        const editor::DocumentRecoveryHistoryStack stack,
        const std::size_t index) -> const WorkspaceHistoryBindingDigest*
    {
        if (!expectedWorkspaceBindings)
        {
            return nullptr;
        }
        const auto found = std::find_if(
            expectedWorkspaceBindings->begin(), expectedWorkspaceBindings->end(),
            [stack, index](const WorkspaceHistoryBindingDigest& binding)
            {
                return binding.position.stack == stack
                    && binding.position.index == index;
            });
        return found == expectedWorkspaceBindings->end() ? nullptr : &*found;
    };
    const auto decodeGroups = [&](
        const QCborArray& groups,
        const editor::DocumentRecoveryHistoryStack stack) -> bool
    {
        auto* output = history && !compactOutput
            ? (stack == editor::DocumentRecoveryHistoryStack::Undo
                ? &history->undoEntries : &history->redoEntries)
            : nullptr;
        std::size_t& decodedCount = stack == editor::DocumentRecoveryHistoryStack::Undo
            ? localStats.undoEntries : localStats.redoEntries;
        std::vector<SparseJklCogImportState>* importStateEntries =
            jklCogImportHistory
            ? (stack == editor::DocumentRecoveryHistoryStack::Undo
                ? &jklCogImportHistory->undo : &jklCogImportHistory->redo)
            : nullptr;
        std::size_t importStateCursor{};
        std::vector<HistoryReference> previousReferences = baseReferences;
        std::optional<Level> previousLevel{};
        std::optional<assets::SedNestedHistoryDecoder> incrementalDecoder =
            baseIncrementalDecoder;
        QByteArray previousBindingStateDigest = baseDigestValue.toByteArray();
        qsizetype groupIndex{};
        for (const QCborValue& groupValue : groups)
        {
            if (!groupValue.isArray())
            {
                return fail(QStringLiteral("Recovery undo history group is malformed."));
            }
            const QCborArray entries = groupValue.toArray();
            if (entries.isEmpty() || entries.size() > kHistoryEntriesPerGroup
                || (groupIndex + 1 < groups.size()
                    && entries.size() != kHistoryEntriesPerGroup)
                || decodedCount > static_cast<std::size_t>(
                    kMaximumHistoryEntries - entries.size())
                || totalDecodedCount > static_cast<std::size_t>(
                    kMaximumHistoryEntries - entries.size()))
            {
                return fail(QStringLiteral("Recovery undo history group is non-canonical or too large."));
            }
            for (const QCborValue& entryValue : entries)
            {
                if (!entryValue.isArray())
                {
                    return fail(QStringLiteral("Recovery undo history manifest is malformed."));
                }
                const QCborArray manifest = entryValue.toArray();
                if ((manifest.size() != 4 && manifest.size() != 5)
                    || !manifest.at(0).isString()
                    || !manifest.at(1).isInteger() || !manifest.at(2).isMap()
                    || !manifest.at(3).isArray()
                    || (manifest.size() == 5 && !manifest.at(4).isMap()))
                {
                    return fail(QStringLiteral("Recovery undo history manifest is incomplete."));
                }
                const QString label = manifest.at(0).toString();
                const qint64 dirty = manifest.at(1).toInteger(-1);
                const auto selection = selectionFromCbor(manifest.at(2));
                const QCborArray record = manifest.at(3).toArray();
                std::optional<editor::Selection> navigationSelection{};
                if (manifest.size() == 5)
                {
                    navigationSelection = selectionFromCbor(manifest.at(4));
                }
                if (label.size() > kMaximumHistoryLabelCharacters || dirty < 0
                    || dirty > std::numeric_limits<std::uint32_t>::max()
                    || !selection
                    || (manifest.size() == 5 && !navigationSelection)
                    || record.size() < 2
                    || !record.at(0).isInteger() || !record.at(1).isInteger()
                    || record.at(0).toInteger(-1) != kHistoryRecordVersion)
                {
                    return fail(QStringLiteral("Recovery undo history manifest values are invalid."));
                }

                const qint64 recordKind = record.at(1).toInteger(-1);
                std::vector<HistoryReference> references{};
                const auto readReferenceArray = [&](const QCborValue& value) -> bool
                {
                    if (!value.isArray())
                    {
                        return false;
                    }
                    const QCborArray values = value.toArray();
                    if (values.isEmpty()
                        || values.size() > kMaximumHistoryFragmentsPerState)
                    {
                        return false;
                    }
                    references.clear();
                    references.reserve(static_cast<std::size_t>(values.size()));
                    for (const QCborValue& reference : values)
                    {
                        if (!reference.isInteger())
                        {
                            return false;
                        }
                        const qint64 index = reference.toInteger(
                            std::numeric_limits<qint64>::min());
                        if (!resolveReference(index))
                        {
                            return false;
                        }
                        references.push_back(static_cast<HistoryReference>(index));
                    }
                    return true;
                };

                const auto buildCheckpointRecord = [&]()
                {
                    QCborArray canonical{};
                    canonical.append(kHistoryRecordVersion);
                    canonical.append(kHistoryRecordCheckpoint);
                    QCborArray values{};
                    for (const HistoryReference index : references)
                    {
                        values.append(static_cast<qint64>(index));
                    }
                    canonical.append(values);
                    return canonical;
                };
                const auto buildDeltaRecord = [
                    &previousReferences,
                    &references](qsizetype* prefixOut,
                        qsizetype* suffixOut,
                        qsizetype* eraseOut,
                        QCborArray* insertedOut)
                {
                    qsizetype prefix{};
                    while (prefix < static_cast<qsizetype>(previousReferences.size())
                        && prefix < static_cast<qsizetype>(references.size())
                        && previousReferences[static_cast<std::size_t>(prefix)]
                            == references[static_cast<std::size_t>(prefix)])
                    {
                        ++prefix;
                    }
                    qsizetype suffix{};
                    while (suffix < static_cast<qsizetype>(previousReferences.size()) - prefix
                        && suffix < static_cast<qsizetype>(references.size()) - prefix
                        && previousReferences[previousReferences.size() - 1u
                            - static_cast<std::size_t>(suffix)]
                            == references[references.size() - 1u
                                - static_cast<std::size_t>(suffix)])
                    {
                        ++suffix;
                    }
                    const qsizetype eraseCount = static_cast<qsizetype>(
                        previousReferences.size()) - prefix - suffix;
                    const qsizetype insertEnd = static_cast<qsizetype>(
                        references.size()) - suffix;
                    QCborArray inserted{};
                    for (qsizetype index = prefix; index < insertEnd; ++index)
                    {
                        inserted.append(static_cast<qint64>(
                            references[static_cast<std::size_t>(index)]));
                    }
                    *prefixOut = prefix;
                    *suffixOut = suffix;
                    *eraseOut = eraseCount;
                    *insertedOut = inserted;
                };

                const qsizetype stateIndex = static_cast<qsizetype>(decodedCount);
                const std::size_t workspaceIndex = decodedCount;
                assets::SedJklCogImportState importState{};
                bool hasExplicitImportState{};
                if ( importStateEntries
                    && importStateCursor < importStateEntries->size() )
                {
                    SparseJklCogImportState& candidate =
                        (*importStateEntries)[importStateCursor];
                    if ( candidate.index < workspaceIndex )
                    {
                        return fail(QStringLiteral(
                            "Recovery undo history JKL COG import state index was not consumed in order."));
                    }
                    if ( candidate.index == workspaceIndex )
                    {
                        importState = std::move(candidate.state);
                        ++importStateCursor;
                        hasExplicitImportState = true;
                    }
                }
                const WorkspaceHistoryBindingDigest* const workspaceBinding =
                    workspaceBindingAt(stack, workspaceIndex);
                const bool workspaceHashNeededForNext = workspaceIndex
                        < std::numeric_limits<std::size_t>::max()
                    && workspaceBindingAt(stack, workspaceIndex + 1u) != nullptr;
                const bool checkpointRequired = stateIndex != 0
                    && stateIndex % kHistoryCheckpointPeriod == 0;
                if (recordKind == kHistoryRecordCheckpoint)
                {
                    if (record.size() != 3
                        || !readReferenceArray(record.at(2)))
                    {
                        return fail(referenceResolutionError.isEmpty()
                            ? QStringLiteral(
                                "Recovery undo history checkpoint record is malformed.")
                            : referenceResolutionError);
                    }
                    const QCborArray canonical = buildCheckpointRecord();
                    if (canonical != record)
                    {
                        return fail(QStringLiteral(
                            "Recovery undo history checkpoint record is non-canonical."));
                    }
                    if (!checkpointRequired && !previousReferences.empty())
                    {
                        qsizetype prefix{}, suffix{}, eraseCount{};
                        QCborArray inserted{};
                        buildDeltaRecord(&prefix, &suffix, &eraseCount, &inserted);
                        QCborArray delta{};
                        delta.append(kHistoryRecordVersion);
                        delta.append(kHistoryRecordDelta);
                        delta.append(static_cast<qint64>(prefix));
                        delta.append(static_cast<qint64>(suffix));
                        delta.append(static_cast<qint64>(eraseCount));
                        delta.append(inserted);
                        if (serializeCanonicalCbor(QCborValue(delta)).size()
                            < serializeCanonicalCbor(QCborValue(record)).size())
                        {
                            return fail(QStringLiteral(
                                "Recovery undo history checkpoint was used when a smaller delta was canonical."));
                        }
                    }
                }
                else if (recordKind == kHistoryRecordDelta)
                {
                    if (record.size() != 6 || previousReferences.empty()
                        || checkpointRequired
                        || !record.at(2).isInteger()
                        || !record.at(3).isInteger()
                        || !record.at(4).isInteger()
                        || !record.at(5).isArray())
                    {
                        return fail(QStringLiteral(
                            "Recovery undo history delta record is malformed or out of sequence."));
                    }
                    const qint64 prefixValue = record.at(2).toInteger(-1);
                    const qint64 suffixValue = record.at(3).toInteger(-1);
                    const qint64 eraseValue = record.at(4).toInteger(-1);
                    const QCborArray inserted = record.at(5).toArray();
                    if (prefixValue < 0 || suffixValue < 0 || eraseValue < 0
                        || prefixValue > static_cast<qint64>(previousReferences.size())
                        || suffixValue > static_cast<qint64>(previousReferences.size())
                            - prefixValue
                        || eraseValue > static_cast<qint64>(previousReferences.size())
                            - prefixValue - suffixValue
                        || inserted.size() > kMaximumHistoryFragmentsPerState)
                    {
                        return fail(QStringLiteral(
                            "Recovery undo history delta splice bounds are invalid."));
                    }
                    references.clear();
                    references.reserve(static_cast<std::size_t>(
                        prefixValue + inserted.size() + suffixValue));
                    references.insert(references.end(), previousReferences.begin(),
                        previousReferences.begin() + static_cast<std::ptrdiff_t>(prefixValue));
                    for (const QCborValue& value : inserted)
                    {
                        if (!value.isInteger())
                        {
                            return fail(QStringLiteral(
                                "Recovery undo history delta insertion is malformed."));
                        }
                        const qint64 index = value.toInteger(
                            std::numeric_limits<qint64>::min());
                        if (!resolveReference(index))
                        {
                            return fail(referenceResolutionError.isEmpty()
                                ? QStringLiteral(
                                    "Recovery undo history delta insertion is out of range.")
                                : referenceResolutionError);
                        }
                        references.push_back(static_cast<HistoryReference>(index));
                    }
                    references.insert(references.end(),
                        previousReferences.end() - static_cast<std::ptrdiff_t>(suffixValue),
                        previousReferences.end());
                    if (references.empty()
                        || references.size() > static_cast<std::size_t>(
                            kMaximumHistoryFragmentsPerState))
                    {
                        return fail(QStringLiteral(
                            "Recovery undo history delta reconstructs an invalid reference list."));
                    }

                    qsizetype expectedPrefix{}, expectedSuffix{}, expectedErase{};
                    QCborArray expectedInserted{};
                    buildDeltaRecord(&expectedPrefix, &expectedSuffix,
                        &expectedErase, &expectedInserted);
                    QCborArray canonical{};
                    canonical.append(kHistoryRecordVersion);
                    canonical.append(kHistoryRecordDelta);
                    canonical.append(static_cast<qint64>(expectedPrefix));
                    canonical.append(static_cast<qint64>(expectedSuffix));
                    canonical.append(static_cast<qint64>(expectedErase));
                    canonical.append(expectedInserted);
                    if (canonical != record)
                    {
                        return fail(QStringLiteral(
                            "Recovery undo history delta splice is not the canonical longest-prefix/suffix delta."));
                    }
                    const QCborArray full = [&]
                    {
                        QCborArray value{};
                        value.append(kHistoryRecordVersion);
                        value.append(kHistoryRecordCheckpoint);
                        QCborArray refs{};
                        for (const HistoryReference index : references)
                        {
                            refs.append(static_cast<qint64>(index));
                        }
                        value.append(refs);
                        return value;
                    }();
                    if (serializeCanonicalCbor(QCborValue(record)).size()
                        >= serializeCanonicalCbor(QCborValue(full)).size())
                    {
                        return fail(QStringLiteral(
                            "Recovery undo history delta was not smaller than a checkpoint."));
                    }
                }
                else
                {
                    return fail(QStringLiteral(
                        "Recovery undo history record kind is unsupported."));
                }

                // Pool order is canonical first-use order. This both makes
                // HIST deterministic and ensures every retained fragment is
                // necessary; unreferenced or reordered entries are rejected
                // instead of consuming memory without representing history.
                for (const HistoryReference index : references)
                {
                    if (index < 0)
                    {
                        continue;
                    }
                    if (!referencedPoolEntries[static_cast<std::size_t>(index)])
                    {
                        if (index != nextFirstUsePoolIndex)
                        {
                            return fail(QStringLiteral(
                                "Recovery undo history pool is not in canonical first-use order."));
                        }
                        referencedPoolEntries[static_cast<std::size_t>(index)] = true;
                        ++nextFirstUsePoolIndex;
                    }
                }

                quint64 stateBytes{};
                for (const HistoryReference index : references)
                {
                    const QByteArray* fragment = resolveReference(index);
                    if (!fragment)
                    {
                        return fail(referenceResolutionError.isEmpty()
                            ? QStringLiteral(
                                "Recovery undo history reference is out of range.")
                            : referenceResolutionError);
                    }
                    const quint64 fragmentBytes = static_cast<quint64>(fragment->size());
                    if (stateBytes > kMaximumChunkBytes - fragmentBytes)
                    {
                        return fail(QStringLiteral(
                            "Recovery undo history reconstructed Level is too large."));
                    }
                    stateBytes += fragmentBytes;
                }
                quint64 uniqueFragmentBytes{};
                for (const HistoryReference index : references)
                {
                    if (index < 0)
                    {
                        const qint64 baseIndex = -1 - index;
                        if (baseIndex >= 0
                            && static_cast<std::size_t>(baseIndex) < chargedBaseEntries.size()
                            && !chargedBaseEntries[static_cast<std::size_t>(baseIndex)])
                        {
                            chargedBaseEntries[static_cast<std::size_t>(baseIndex)] = true;
                            uniqueFragmentBytes += static_cast<quint64>(
                                baseFragments[static_cast<std::size_t>(baseIndex)].size());
                        }
                    }
                    else if (static_cast<std::size_t>(index) < chargedPoolEntries.size()
                        && !chargedPoolEntries[static_cast<std::size_t>(index)])
                    {
                        chargedPoolEntries[static_cast<std::size_t>(index)] = true;
                        uniqueFragmentBytes += static_cast<quint64>(
                            pool[static_cast<std::size_t>(index)].size());
                    }
                }
                const quint64 perStateCharge = std::min(
                    stateBytes, kHistoryPerStateReconstructionCharge);
                if (uniqueFragmentBytes > std::numeric_limits<quint64>::max()
                    - perStateCharge)
                {
                    return fail(QStringLiteral(
                        "Recovery undo history cumulative Level reconstruction exceeds its bounded work budget."));
                }
                const quint64 workCharge = uniqueFragmentBytes + perStateCharge;
                if (workCharge > reconstructionBudget
                    || reconstructedBytes > reconstructionBudget - workCharge)
                {
                    return fail(QStringLiteral(
                        "Recovery undo history cumulative Level reconstruction exceeds its bounded work budget."));
                }
                // Charge unique pooled/base bytes before reserve/append and
                // native decoding. Unchanged geometry is not charged once per
                // history entry, while the per-state floor still bounds
                // repeated manifests.
                reconstructedBytes += workCharge;
                QByteArray nested{};
                std::vector<std::byte> incrementalNested{};
                if (incrementalDecoder)
                {
                    incrementalNested.reserve(static_cast<std::size_t>(stateBytes));
                }
                else
                {
                    nested.reserve(static_cast<qsizetype>(stateBytes));
                }
                for (const HistoryReference index : references)
                {
                    const QByteArray* const fragment = resolveReference(index);
                    if (!fragment)
                    {
                        return fail(referenceResolutionError.isEmpty()
                            ? QStringLiteral(
                                "Recovery undo history reference is out of range.")
                            : referenceResolutionError);
                    }
                    if (incrementalDecoder)
                    {
                        const auto bytes = std::as_bytes(std::span(
                            fragment->constData(),
                            static_cast<std::size_t>(fragment->size())));
                        incrementalNested.insert(
                            incrementalNested.end(), bytes.begin(), bytes.end());
                    }
                    else
                    {
                        nested.append(*fragment);
                    }
                }
                QByteArray currentBindingStateDigest{};
                if (expectedWorkspaceBindings
                    && (workspaceBinding || workspaceHashNeededForNext))
                {
                    QCryptographicHash hash(QCryptographicHash::Sha256);
                    if (incrementalDecoder)
                    {
                        hash.addData(reinterpret_cast<const char*>(
                                incrementalNested.data()),
                            static_cast<qsizetype>(incrementalNested.size()));
                    }
                    else
                    {
                        hash.addData(nested);
                    }
                    addJklCogImportStateDigest(hash, importState);
                    currentBindingStateDigest = hash.result();
                }
                QString levelError{};
                std::optional<Level> level{};
                const Level* decodedLevel{};
                const Level* const precedingLevel = incrementalDecoder
                    ? &incrementalDecoder->currentLevel() : nullptr;
                if (incrementalDecoder)
                {
                    // Undo manifests describe the state before an operation,
                    // while their navigation hint targets the adjacent
                    // post-operation Level currently held by the directional
                    // decoder. Validate before advance() replaces that Level.
                    if (navigationSelection
                        && stack == editor::DocumentRecoveryHistoryStack::Undo
                        && !selectionReferencesLevel(
                            *navigationSelection,
                            incrementalDecoder->currentLevel()))
                    {
                        return fail(QStringLiteral(
                            "Recovery undo history navigation selection references missing or incompatible adjacent Level objects."));
                    }
                    if (workspaceBinding)
                    {
                        QString attachmentError{};
                        const bool redoStack = stack
                            == editor::DocumentRecoveryHistoryStack::Redo;
                        // Redo's pre-Apply side is the decoder's current Level;
                        // validate its exact surface/target identities before
                        // advance replaces that Level. Undo's post-Apply layer
                        // rows have the same pre-advance lifetime.
                        if (!attachmentValidator
                            || (redoStack
                                && !attachmentValidator(
                                    workspaceBinding->cleaveAttachment,
                                    workspaceBinding->layerVisualsAttachment,
                                    precedingLevel,
                                    nullptr,
                                    &attachmentError))
                            || (!redoStack
                                && !attachmentValidator(
                                    QCborValue{},
                                    workspaceBinding->layerVisualsAttachment,
                                    nullptr,
                                    precedingLevel,
                                    &attachmentError)))
                        {
                            workspaceBindingHashesValid = false;
                        }
                    }
                    try
                    {
                        incrementalDecoder->advance(
                            std::move(incrementalNested),
                            std::move(importState));
                        decodedLevel = &incrementalDecoder->currentLevel();
                    }
                    catch (const std::bad_alloc&)
                    {
                        throw;
                    }
                    catch (const std::exception& exception)
                    {
                        levelError = QString::fromUtf8(exception.what());
                    }
                }
                else
                {
                    level = deserializeLevelFromSed(
                        nested,
                        projectType,
                        &levelError,
                        false,
                        assets::SedSerializationProfile::HistoryNested);
                    if ( level )
                    {
                        if ( !installJklCogImportState(
                                *level, std::move(importState),
                                hasExplicitImportState, &levelError) )
                        {
                            level.reset();
                        }
                        else
                        {
                            decodedLevel = &*level;
                        }
                    }
                }
                if (!decodedLevel)
                {
                    return fail(levelError.isEmpty()
                        ? QStringLiteral("Recovery undo history Level is invalid.")
                        : levelError);
                }
                if (decodedLevel->kind != projectType)
                {
                    return fail(QStringLiteral(
                        "Recovery metadata and binary SED project type disagree."));
                }
                if ( incrementalDecoder
                    && (assets::hasRepresentedJklCogImportState(*decodedLevel)
                        != hasExplicitImportState) )
                {
                    return fail(QStringLiteral(
                        "Recovery undo history JKL COG import state is not represented by its reconstructed Level."));
                }
                if (!selectionReferencesLevel(*selection, *decodedLevel))
                {
                    return fail(QStringLiteral(
                        "Recovery undo history selection references missing or incompatible Level objects."));
                }
                // Materializing readers already retain the preceding state in
                // the output vector; referencing it avoids a second full
                // Level copy. The writer-validation path has no output, so it
                // keeps one bounded adjacent copy instead.
                const Level* transitionFrom = incrementalDecoder
                    ? nullptr
                    : (output
                        ? (output->empty()
                            ? currentLevel : &output->back().state.level)
                        : (previousLevel ? &*previousLevel : currentLevel));
                if (workspaceBinding)
                {
                    QString attachmentError{};
                    const bool undoStack = stack
                        == editor::DocumentRecoveryHistoryStack::Undo;
                    const bool valid = attachmentValidator
                        && (incrementalDecoder
                        ? (undoStack
                            ? attachmentValidator(
                                workspaceBinding->cleaveAttachment,
                                workspaceBinding->layerVisualsAttachment,
                                decodedLevel,
                                nullptr,
                                &attachmentError)
                            : attachmentValidator(
                                QCborValue{},
                                workspaceBinding->layerVisualsAttachment,
                                nullptr,
                                decodedLevel,
                                &attachmentError))
                        : attachmentValidator(
                            workspaceBinding->cleaveAttachment,
                            workspaceBinding->layerVisualsAttachment,
                            undoStack ? decodedLevel : transitionFrom,
                            undoStack ? transitionFrom : decodedLevel,
                            &attachmentError));
                    if (!valid)
                    {
                        workspaceBindingHashesValid = false;
                    }
                }
                if (workspaceBinding)
                {
                    const QByteArray& expectedBefore =
                        stack == editor::DocumentRecoveryHistoryStack::Undo
                        ? currentBindingStateDigest
                        : previousBindingStateDigest;
                    const QByteArray& expectedAfter =
                        stack == editor::DocumentRecoveryHistoryStack::Undo
                        ? previousBindingStateDigest
                        : currentBindingStateDigest;
                    if (expectedBefore.size() != 32 || expectedAfter.size() != 32
                        || workspaceBinding->beforeSha256 != expectedBefore
                        || workspaceBinding->afterSha256 != expectedAfter)
                    {
                        workspaceBindingHashesValid = false;
                    }
                    ++workspaceBindingsMatched;
                }
                previousBindingStateDigest = workspaceHashNeededForNext
                    ? currentBindingStateDigest : QByteArray{};
                const Level* const navigationLevel =
                    stack == editor::DocumentRecoveryHistoryStack::Redo
                        ? decodedLevel
                        : transitionFrom;
                if (navigationSelection && navigationLevel
                    && !selectionReferencesLevel(
                        *navigationSelection, *navigationLevel))
                {
                    return fail(QStringLiteral(
                        "Recovery undo history navigation selection references missing or incompatible adjacent Level objects."));
                }
                const auto incrementalDirtyFlagsCanRepresentTransition = [&]()
                {
                    constexpr std::uint32_t things =
                        static_cast<std::uint32_t>(editor::DocumentDirty::Things);
                    constexpr std::uint32_t lights =
                        static_cast<std::uint32_t>(editor::DocumentDirty::Lights);
                    constexpr std::uint32_t cogs =
                        static_cast<std::uint32_t>(editor::DocumentDirty::Cogs);
                    const std::uint32_t flags = static_cast<std::uint32_t>(dirty);
                    if (flags == cogs)
                    {
                        return incrementalDecoder->
                            lastTransitionEqualsIgnoringHistoryDomains(
                                false, false, true);
                    }
                    const bool partialThings = (flags & things) != 0;
                    const bool partialLights = (flags & lights) != 0;
                    if (flags != 0 && (flags & ~(things | lights)) == 0
                        && (partialThings || partialLights))
                    {
                        return incrementalDecoder->
                            lastTransitionEqualsIgnoringHistoryDomains(
                                partialThings, partialLights, false);
                    }
                    return true;
                };
                if ((incrementalDecoder
                        && !incrementalDirtyFlagsCanRepresentTransition())
                    || (transitionFrom
                        && !dirtyFlagsCanRepresentLevelTransition(
                            *transitionFrom, *decodedLevel,
                            static_cast<std::uint32_t>(dirty))))
                {
                    return fail(QStringLiteral(
                        "Recovery undo history dirty flags do not describe the adjacent Level transition."));
                }
                // Native readers immediately convert each expanded state to
                // the same compact representation used by the live undo
                // stack. Charge only the domains that representation retains;
                // the old whole-Level-per-entry charge rejected ordinary
                // Thing edits merely because unchanged geometry was large.
                const auto compactFootprint = [](
                    const Level& source,
                    const std::uint32_t flags,
                    const std::vector<std::int32_t>* geometrySectorIndices,
                    const std::vector<std::int32_t>* thingIndices)
                {
                    // LightsSnapshot/ThingsLightsSnapshot retain only the
                    // baked colour payload, not a copy of every Sector and
                    // Surface object.  Keep layout-equivalent local types so
                    // the admission estimate charges the same vector element
                    // widths as DocumentUndoHistory.  The previous proxy
                    // resized Surface::texVertices, charging u/v plus colour
                    // (and every Surface's authored metadata) for a snapshot
                    // that stores only one ColorF per tex vertex.  On a level
                    // with many baked vertices this false charge could make a
                    // valid HIST stream exceed the aggregate budget and be
                    // dropped on reopen.
                    struct BakedSurfaceStorage
                    {
                        std::vector<sed::ColorF> texVertexColors{};
                    };
                    struct BakedSectorStorage
                    {
                        sed::ColorF ambient{};
                        std::vector<BakedSurfaceStorage> surfaces{};
                    };
                    const auto bit = [flags](const editor::DocumentDirty value)
                    {
                        return (flags & static_cast<std::uint32_t>(value)) != 0;
                    };
                    constexpr std::uint32_t partialMask =
                        static_cast<std::uint32_t>(editor::DocumentDirty::Things)
                        | static_cast<std::uint32_t>(editor::DocumentDirty::Lights)
                        | static_cast<std::uint32_t>(editor::DocumentDirty::Cogs)
                        | static_cast<std::uint32_t>(editor::DocumentDirty::LevelGeometry);
                    const bool lights = bit(editor::DocumentDirty::Lights);
                    // MotS LightsSnapshot is the combined Things+Lights
                    // snapshot because 0x20 baked output owns ARCH refs,
                    // records, and baseline provenance as well as lights.
                    const bool things = bit(editor::DocumentDirty::Things)
                        || (lights && source.kind == ProjectType::MOTS);
                    const bool cogs = bit(editor::DocumentDirty::Cogs);
                    const bool geometry = flags == static_cast<std::uint32_t>(
                        editor::DocumentDirty::LevelGeometry);
                    const bool thingDomain = flags == static_cast<std::uint32_t>(
                        editor::DocumentDirty::Things);
                    const bool full = flags == 0
                        || bit(editor::DocumentDirty::Structure)
                        || (flags & ~partialMask) != 0
                        || (cogs && (things || lights))
                        || ((flags & static_cast<std::uint32_t>(
                                editor::DocumentDirty::LevelGeometry)) != 0
                            && !geometry)
                        || (geometry && !geometrySectorIndices)
                        || (thingDomain && !thingIndices)
                        // Document::makeThingSubsetSnapshot escalates to a
                        // FullSnapshot while MotS bake provenance is present,
                        // because a compact row cannot restore that table.
                        // Admission must charge the same complete Level the
                        // in-memory history actually retains.
                        || (thingDomain
                            && !source.motsArchLightingBakeBaselines.empty());
                    if (full)
                    {
                        return assets::estimateSedLevelDecodedFootprint(source);
                    }

                    // Build a domain-only proxy so the existing bounded,
                    // overflow-checked estimator remains the single source of
                    // truth for strings, values, and nested collection items.
                    Level retained{};
                    retained.kind = source.kind;
                    retained.curId = source.curId;
                    assets::SedLevelDecodedFootprint bakedFootprint{};
                    if (geometry)
                    {
                        retained.sectors.reserve(geometrySectorIndices->size());
                        for (const std::int32_t index : *geometrySectorIndices)
                        {
                            if (index >= 0 && static_cast<std::size_t>(index)
                                    < source.sectors.size())
                            {
                                retained.sectors.push_back(source.sectors[
                                    static_cast<std::size_t>(index)]);
                            }
                        }
                    }
                    if (things)
                    {
                        if (thingIndices)
                        {
                            retained.things.reserve(thingIndices->size());
                            for (const std::int32_t index : *thingIndices)
                            {
                                if (index >= 0 && static_cast<std::size_t>(index)
                                        < source.things.size())
                                {
                                    retained.things.push_back(source.things[
                                        static_cast<std::size_t>(index)]);
                                }
                            }
                        }
                        else
                        {
                            retained.things = source.things;
                        }
                    }
                    if ( cogs )
                    {
                        retained.cogs = source.cogs;
                        retained.importedJklCogRowsPresent =
                            source.importedJklCogRowsPresent;
                        retained.importedJklSurfaceRefs =
                            source.importedJklSurfaceRefs;
                    }
                    if (lights)
                    {
                        retained.lights = source.lights;
                        if (source.kind == ProjectType::MOTS)
                        {
                            retained.motsArchLighting = source.motsArchLighting;
                            retained.motsArchLightingBakeBaselines =
                                source.motsArchLightingBakeBaselines;
                        }
                        // The compact light snapshot has no Level::Sector
                        // objects. Add its small baked-lighting graph below
                        // instead of constructing a geometry-shaped proxy.
                        const auto addBaked =
                            [&bakedFootprint](const std::uint64_t count,
                                const std::size_t elementBytes)
                        {
                            if (count > 0
                                && static_cast<std::uint64_t>(elementBytes)
                                    > std::numeric_limits<std::uint64_t>::max() / count)
                            {
                                throw std::runtime_error(
                                    "Recovery undo history baked-lighting footprint overflowed.");
                            }
                            const assets::SedLevelDecodedFootprint addition{
                                count * static_cast<std::uint64_t>(elementBytes),
                                count };
                            if (!assets::tryAccumulateSedLevelDecodedFootprint(
                                    bakedFootprint, addition))
                            {
                                throw std::runtime_error(
                                    "Recovery undo history baked-lighting footprint overflowed.");
                            }
                        };
                        addBaked(static_cast<std::uint64_t>(source.sectors.size()),
                            sizeof(BakedSectorStorage));
                        for (const Sector& sourceSector : source.sectors)
                        {
                            addBaked(static_cast<std::uint64_t>(sourceSector.surfaces.size()),
                                sizeof(BakedSurfaceStorage));
                            for (const Surface& sourceSurface : sourceSector.surfaces)
                            {
                                // A live Lights snapshot retains only one
                                // ColorF for every texture vertex.
                                addBaked(static_cast<std::uint64_t>(
                                    sourceSurface.texVertices.size()),
                                    sizeof(sed::ColorF));
                            }
                        }
                    }
                    assets::SedLevelDecodedFootprint retainedFootprint =
                        assets::estimateSedLevelDecodedFootprint(retained);
                    if (!assets::tryAccumulateSedLevelDecodedFootprint(
                            retainedFootprint, bakedFootprint))
                    {
                        throw std::runtime_error(
                            "Recovery undo history baked-lighting footprint overflowed.");
                    }
                    return retainedFootprint;
                };
                std::optional<std::vector<std::int32_t>> geometrySubset{};
                std::optional<std::vector<std::int32_t>> thingSubset{};
                if (compactOutput
                    && static_cast<std::uint32_t>(dirty)
                        == static_cast<std::uint32_t>(editor::DocumentDirty::LevelGeometry))
                {
                    geometrySubset = incrementalDecoder
                        ? incrementalDecoder->lastChangedSectorIndices()
                        : (transitionFrom
                            ? historyGeometrySubset(*transitionFrom, *decodedLevel)
                            : std::nullopt);
                }
                if (compactOutput
                    && static_cast<std::uint32_t>(dirty)
                        == static_cast<std::uint32_t>(editor::DocumentDirty::Things))
                {
                    thingSubset = incrementalDecoder
                        ? incrementalDecoder->lastChangedThingIndices()
                        : (transitionFrom
                            ? historyThingSubset(*transitionFrom, *decodedLevel)
                            : std::nullopt);
                }
                const assets::SedLevelDecodedFootprint levelFootprint =
                    compactOutput
                    ? compactFootprint(
                        *decodedLevel,
                        static_cast<std::uint32_t>(dirty),
                        geometrySubset ? &*geometrySubset : nullptr,
                        thingSubset ? &*thingSubset : nullptr)
                    : assets::estimateSedLevelDecodedFootprint(*decodedLevel);
                assets::SedLevelDecodedFootprint nextRetained =
                    retainedLevelFootprint;
                if (!assets::tryAccumulateSedLevelDecodedFootprint(
                        nextRetained, levelFootprint)
                    || nextRetained.ownedBytes > maximumRetainedLevelBytes
                    || nextRetained.itemCount > maximumRetainedLevelItems)
                {
                    return fail(QStringLiteral(
                        "Recovery undo history cumulative retained Level object storage exceeds its bounded byte/item budget "
                        "after entry %1 (dirty=0x%2, bytes=%3/%4, items=%5/%6).")
                            .arg(totalDecodedCount + 1)
                            .arg(static_cast<qulonglong>(
                                static_cast<std::uint32_t>(dirty)), 0, 16)
                            .arg(static_cast<qulonglong>(nextRetained.ownedBytes))
                            .arg(static_cast<qulonglong>(maximumRetainedLevelBytes))
                            .arg(static_cast<qulonglong>(nextRetained.itemCount))
                            .arg(static_cast<qulonglong>(maximumRetainedLevelItems)));
                }
                retainedLevelFootprint = nextRetained;
                if (compactOutput && history && history->compact)
                {
                    history->compact->appendCompactRecoveryState(
                        stack,
                        label.toStdString(),
                        static_cast<std::uint32_t>(dirty),
                        *decodedLevel,
                        *selection,
                        geometrySubset,
                        thingSubset,
                        std::nullopt,
                        std::move(navigationSelection));
                    // The incremental decoder retains a failure-atomic patch
                    // of only the preceding changed domains. The legacy
                    // nonincremental validation path still needs one adjacent
                    // expanded Level for its pairwise transition checks.
                    if (!incrementalDecoder)
                    {
                        previousLevel = std::move(*level);
                    }
                }
                else if (!output)
                {
                    // Keep one adjacent state for the null-output writer
                    // validation path. Move rather than copy; materializing
                    // readers use the output vector above and do not
                    // duplicate this Level.
                    previousLevel = std::move(*level);
                }
                else
                {
                    editor::DocumentRecoveryHistoryEntry entry{};
                    entry.label = label.toStdString();
                    entry.dirtyFlags = static_cast<std::uint32_t>(dirty);
                    entry.state.level = std::move(*level);
                    entry.state.selection = *selection;
                    entry.navigationSelection = std::move(navigationSelection);
                    output->push_back(std::move(entry));
                }
                previousReferences = references;
                ++decodedCount;
                ++totalDecodedCount;
            }
            ++groupIndex;
        }
        if ( importStateEntries
            && importStateCursor != importStateEntries->size() )
        {
            return fail(QStringLiteral(
                "Recovery undo history JKL COG import state index is outside its stack."));
        }
        return true;
    };

    if (!decodeGroups(undoGroups.toArray(), editor::DocumentRecoveryHistoryStack::Undo)
        || !decodeGroups(redoGroups.toArray(), editor::DocumentRecoveryHistoryStack::Redo))
    {
        if (history)
        {
            history->undoEntries.clear();
            history->redoEntries.clear();
        }
        return false;
    }
    if (history && history->compact)
    {
        history->compact->finishCompactRecoveryImport();
    }
    if (nextFirstUsePoolIndex != static_cast<qsizetype>(pool.size()))
    {
        if (history)
        {
            history->undoEntries.clear();
            history->redoEntries.clear();
        }
        return fail(QStringLiteral(
            "Recovery undo history fragment pool contains unused entries."));
    }
    if (stats) *stats = localStats;
    if (workspaceBindingsValid)
    {
        *workspaceBindingsValid = expectedWorkspaceBindings
            && workspaceBindingHashesValid
            && workspaceBindingsMatched == expectedWorkspaceBindings->size();
    }
    return true;
}

std::optional<editor::DocumentRecoveryHistory> deserializeRecoveryHistory(
    const QByteArray& payload,
    const ProjectType projectType,
    const Level* currentLevel,
    QString* error,
    const bool compactOutput,
    const std::vector<WorkspaceHistoryBindingDigest>* const expectedWorkspaceBindings = nullptr,
    bool* const workspaceBindingsValid = nullptr,
    const WorkspaceHistoryAttachmentLevelValidator attachmentValidator = nullptr)
{
    editor::DocumentRecoveryHistory history{};
    if (!decodeRecoveryHistory(payload, projectType, currentLevel,
            &history, nullptr, error,
            kMaximumHistoryRetainedLevelBytes,
            kMaximumHistoryRetainedLevelItems,
            kMaximumHistoryReconstructionBytesAbsolute,
            compactOutput,
            expectedWorkspaceBindings,
            workspaceBindingsValid,
            attachmentValidator))
    {
        return std::nullopt;
    }
    return history;
}

quint32 recoveryCrc32(const QByteArray& bytes)
{
    quint32 crc = 0xFFFF'FFFFu;
    for (const unsigned char byte : bytes)
    {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc >> 1u) ^ (0xEDB8'8320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

bool compressedSizePrefixMatches(const QByteArray& bytes, const quint64 expectedRawSize)
{
    if (bytes.size() < 4 || expectedRawSize > std::numeric_limits<quint32>::max())
    {
        return false;
    }
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.constData());
    const quint32 declared = (static_cast<quint32>(data[0]) << 24u)
        | (static_cast<quint32>(data[1]) << 16u)
        | (static_cast<quint32>(data[2]) << 8u)
        | static_cast<quint32>(data[3]);
    return declared == expectedRawSize;
}

// The outer V1 META unit uses a compact systematic Reed-Solomon-style P/Q code
// over GF(256). CRC-32 identifies erasures and P/Q can reconstruct any two
// missing shards without duplicating the native DOCM document.
struct Gf256Tables
{
    std::array<unsigned char, 512> exp{};
    std::array<unsigned char, 256> log{};

    Gf256Tables()
    {
        unsigned int value = 1;
        for (int i = 0; i < 255; ++i)
        {
            exp[static_cast<std::size_t>(i)] = static_cast<unsigned char>(value);
            log[static_cast<std::size_t>(value)] = static_cast<unsigned char>(i);
            value <<= 1u;
            if ((value & 0x100u) != 0)
            {
                value ^= 0x11du;
            }
        }
        for (int i = 255; i < 512; ++i)
        {
            exp[static_cast<std::size_t>(i)] = exp[static_cast<std::size_t>(i - 255)];
        }
    }
};

const Gf256Tables& gf256Tables()
{
    static const Gf256Tables tables{};
    return tables;
}

unsigned char gfMul(const unsigned char lhs, const unsigned char rhs)
{
    if (lhs == 0 || rhs == 0)
    {
        return 0;
    }
    const auto& tables = gf256Tables();
    return tables.exp[static_cast<std::size_t>(tables.log[lhs] + tables.log[rhs])];
}

unsigned char gfDiv(const unsigned char lhs, const unsigned char rhs)
{
    if (lhs == 0)
    {
        return 0;
    }
    if (rhs == 0)
    {
        return 0;
    }
    const auto& tables = gf256Tables();
    int exponent = static_cast<int>(tables.log[lhs]) - static_cast<int>(tables.log[rhs]);
    if (exponent < 0)
    {
        exponent += 255;
    }
    return tables.exp[static_cast<std::size_t>(exponent)];
}

unsigned char gfCoefficient(const std::size_t shard)
{
    return gf256Tables().exp[shard % 255u];
}

struct FecSectionDescriptor
{
    quint64 rawSize{};
    quint64 storedSize{};
    quint32 dataShards{};
    quint32 shardSize{};
    QByteArray rawHash{};
    std::vector<quint32> shardCrcs{};
};

QByteArray serializeFecTable(
    const QByteArray& completeLevel,
    const std::vector<FecSectionDescriptor>& descriptors)
{
    if (completeLevel.isEmpty()
        || static_cast<quint64>(completeLevel.size()) > kMaximumChunkBytes
        || descriptors.size() != 1)
    {
        throw std::runtime_error("Recovery parity table input is out of range.");
    }
    QByteArray table{};
    QDataStream stream(&table, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData(kFecTableMagic, 8);
    stream << quint32{1}
           << static_cast<quint32>(descriptors.size())
           << static_cast<quint64>(completeLevel.size());
    const QByteArray completeHash = QCryptographicHash::hash(
        completeLevel, QCryptographicHash::Sha256);
    stream.writeRawData(completeHash.constData(), completeHash.size());
    for (const FecSectionDescriptor& descriptor : descriptors)
    {
        stream << descriptor.rawSize << descriptor.storedSize
               << descriptor.dataShards << descriptor.shardSize;
        stream.writeRawData(descriptor.rawHash.constData(), descriptor.rawHash.size());
        for (const quint32 crc : descriptor.shardCrcs)
        {
            stream << crc;
        }
    }
    if (stream.status() != QDataStream::Ok || table.isEmpty()
        || static_cast<quint64>(table.size()) > kMaximumChunkBytes)
    {
        throw std::runtime_error("Recovery parity table encoding failed.");
    }
    return table;
}

bool parseFecTable(
    const QByteArray& table,
    quint64* totalRawSize,
    QByteArray* totalHash,
    std::vector<FecSectionDescriptor>* descriptors)
{
    if (!totalRawSize || !totalHash || !descriptors)
    {
        return false;
    }
    QDataStream stream(table);
    stream.setByteOrder(QDataStream::LittleEndian);
    char magic[8]{};
    quint32 version{};
    quint32 count{};
    stream.readRawData(magic, 8);
    stream >> version >> count >> *totalRawSize;
    char hash[32]{};
    stream.readRawData(hash, 32);
    if (stream.status() != QDataStream::Ok
        || QByteArray(magic, 8) != QByteArray(kFecTableMagic, 8)
        // Recovery V1 protects META as one semantic unit. Reject an
        // impossible count before reserve so a checksum-resealed tiny table
        // cannot request a large descriptor allocation.
        || version != 1 || count != 1
        || *totalRawSize == 0 || *totalRawSize > kMaximumChunkBytes)
    {
        return false;
    }
    *totalHash = QByteArray(hash, 32);
    descriptors->clear();
    descriptors->reserve(count);
    quint64 summedRaw{};
    for (quint32 i = 0; i < count; ++i)
    {
        FecSectionDescriptor descriptor{};
        stream >> descriptor.rawSize >> descriptor.storedSize
               >> descriptor.dataShards >> descriptor.shardSize;
        char rawHash[32]{};
        stream.readRawData(rawHash, 32);
        descriptor.rawHash = QByteArray(rawHash, 32);
        if (stream.status() != QDataStream::Ok
            || descriptor.rawSize == 0 || descriptor.rawSize > kMaximumChunkBytes
            || descriptor.storedSize == 0 || descriptor.storedSize > kMaximumChunkBytes
            || descriptor.dataShards == 0 || descriptor.dataShards > kFecDataShards
            || descriptor.shardSize == 0
            || descriptor.shardSize
                != (descriptor.storedSize + descriptor.dataShards - 1)
                    / descriptor.dataShards)
        {
            return false;
        }
        descriptor.shardCrcs.resize(
            static_cast<std::size_t>(descriptor.dataShards + kFecParityShards));
        for (quint32& crc : descriptor.shardCrcs)
        {
            stream >> crc;
        }
        if (stream.status() != QDataStream::Ok
            || summedRaw > kMaximumChunkBytes - descriptor.rawSize)
        {
            return false;
        }
        summedRaw += descriptor.rawSize;
        descriptors->push_back(std::move(descriptor));
    }
    return stream.atEnd() && summedRaw == *totalRawSize;
}

QByteArray makeProtectedPayload(const QByteArray& payload)
{
    if (payload.isEmpty() || static_cast<quint64>(payload.size()) > kMaximumChunkBytes)
    {
        throw std::runtime_error("Recovery payload size is out of range.");
    }
    const std::vector<QByteArray> rawSections{ payload };
    std::vector<FecSectionDescriptor> descriptors{};
    std::vector<std::vector<QByteArray>> encodedSections{};
    descriptors.reserve(rawSections.size());
    encodedSections.reserve(rawSections.size());

    for (const QByteArray& raw : rawSections)
    {
        if (raw.isEmpty() || static_cast<quint64>(raw.size()) > kMaximumChunkBytes)
        {
            throw std::runtime_error("Recovery section size is out of range.");
        }
        const QByteArray stored = qCompress(raw, 6);
        if (stored.isEmpty() || static_cast<quint64>(stored.size()) > kMaximumChunkBytes)
        {
            throw std::runtime_error("Recovery section compression failed.");
        }
        const quint32 dataCount = std::min<quint32>(
            kFecDataShards,
            std::max<quint32>(1, static_cast<quint32>(stored.size())));
        const quint32 shardSize = static_cast<quint32>(
            (stored.size() + dataCount - 1) / dataCount);
        std::vector<QByteArray> shards(
            static_cast<std::size_t>(dataCount + kFecParityShards),
            QByteArray(static_cast<qsizetype>(shardSize), '\0'));
        for (quint32 i = 0; i < dataCount; ++i)
        {
            const qsizetype offset = static_cast<qsizetype>(i) * shardSize;
            const qsizetype available = std::max<qsizetype>(0,
                std::min<qsizetype>(shardSize, stored.size() - offset));
            if (available > 0)
            {
                std::copy_n(
                    stored.constData() + offset,
                    available,
                    shards[static_cast<std::size_t>(i)].data());
            }
        }
        QByteArray& parityP = shards[static_cast<std::size_t>(dataCount)];
        QByteArray& parityQ = shards[static_cast<std::size_t>(dataCount + 1)];
        for (quint32 i = 0; i < dataCount; ++i)
        {
            const QByteArray& data = shards[static_cast<std::size_t>(i)];
            const unsigned char coefficient = gfCoefficient(i);
            for (quint32 byte = 0; byte < shardSize; ++byte)
            {
                const unsigned char value = static_cast<unsigned char>(data.at(byte));
                parityP[byte] = static_cast<char>(
                    static_cast<unsigned char>(parityP.at(byte)) ^ value);
                parityQ[byte] = static_cast<char>(
                    static_cast<unsigned char>(parityQ.at(byte)) ^ gfMul(coefficient, value));
            }
        }

        FecSectionDescriptor descriptor{};
        descriptor.rawSize = static_cast<quint64>(raw.size());
        descriptor.storedSize = static_cast<quint64>(stored.size());
        descriptor.dataShards = dataCount;
        descriptor.shardSize = shardSize;
        descriptor.rawHash = QCryptographicHash::hash(raw, QCryptographicHash::Sha256);
        descriptor.shardCrcs.reserve(shards.size());
        for (const QByteArray& shard : shards)
        {
            descriptor.shardCrcs.push_back(recoveryCrc32(shard));
        }
        descriptors.push_back(std::move(descriptor));
        encodedSections.push_back(std::move(shards));
    }

    const QByteArray table = serializeFecTable(payload, descriptors);
    quint64 encodedSize = 8 + static_cast<quint64>(table.size()) + 4
        + static_cast<quint64>(table.size()) + 4 + 8 + 8;
    if (encodedSize > kMaximumChunkBytes)
    {
        throw std::runtime_error("Protected recovery payload exceeds the container limit.");
    }
    for (const FecSectionDescriptor& descriptor : descriptors)
    {
        const quint64 shardCount = descriptor.dataShards + kFecParityShards;
        if (descriptor.shardSize > 0
            && shardCount > (kMaximumChunkBytes - encodedSize) / descriptor.shardSize)
        {
            throw std::runtime_error("Protected recovery payload exceeds the container limit.");
        }
        encodedSize += shardCount * descriptor.shardSize;
    }
    if (encodedSize > kMaximumChunkBytes)
    {
        throw std::runtime_error("Protected recovery payload exceeds the container limit.");
    }
    QByteArray result{};
    QDataStream prefix(&result, QIODevice::WriteOnly);
    prefix.setByteOrder(QDataStream::LittleEndian);
    prefix << static_cast<quint64>(table.size());
    result.append(table);
    const quint32 tableCrc = recoveryCrc32(table);
    QDataStream tableCrcStream(&result, QIODevice::Append);
    tableCrcStream.setByteOrder(QDataStream::LittleEndian);
    tableCrcStream << tableCrc;
    for (const auto& section : encodedSections)
    {
        for (const QByteArray& shard : section)
        {
            result.append(shard);
        }
    }
    result.append(table);
    QDataStream footer(&result, QIODevice::Append);
    footer.setByteOrder(QDataStream::LittleEndian);
    footer << tableCrc << static_cast<quint64>(table.size());
    footer.writeRawData(kFecFooterMagic, 8);
    if (prefix.status() != QDataStream::Ok
        || tableCrcStream.status() != QDataStream::Ok
        || footer.status() != QDataStream::Ok
        || static_cast<quint64>(result.size()) != encodedSize)
    {
        throw std::runtime_error("Protected recovery payload encoding failed.");
    }
    return result;
}

struct DecodedProtectedPayload
{
    QByteArray level{};
    QStringList warnings{};
    std::uint32_t repairedShardCount{};
    std::uint32_t lostSectionCount{};
    bool corrupted{};
};

struct ProtectedMetadataTable
{
    quint64 totalRawSize{};
    QByteArray totalHash{};
    std::vector<FecSectionDescriptor> descriptors{};
};

std::optional<DecodedProtectedPayload> decodeProtectedPayloadImpl(
    const QByteArray& payload,
    QString* error,
    const QByteArray& forcedTable = {},
    const bool forcedFooterTable = false)
{
    constexpr qsizetype footerBytes = 4 + 8 + 8;
    if (payload.size() <= footerBytes + 12
        || static_cast<quint64>(payload.size()) > kMaximumMetadataProtectedBytes)
    {
        if (error)
        {
            *error = QStringLiteral(
                "Protected recovery META is truncated or exceeds its 4 MiB stored limit.");
        }
        return std::nullopt;
    }

    quint64 tableSize{};
    QByteArray table{};
    bool usedFooterTable = forcedFooterTable;
    if (!forcedTable.isEmpty())
    {
        table = forcedTable;
        tableSize = static_cast<quint64>(table.size());
    }
    else
    {
        QDataStream stream(payload.left(8));
        stream.setByteOrder(QDataStream::LittleEndian);
        stream >> tableSize;
        if (stream.status() == QDataStream::Ok && tableSize > 0
            && tableSize <= static_cast<quint64>(payload.size() - 12))
        {
            table = payload.mid(8, static_cast<qsizetype>(tableSize));
            QDataStream crcStream(payload.mid(8 + static_cast<qsizetype>(tableSize), 4));
            crcStream.setByteOrder(QDataStream::LittleEndian);
            quint32 expected{};
            crcStream >> expected;
            if (crcStream.status() != QDataStream::Ok || recoveryCrc32(table) != expected)
            {
                table.clear();
            }
        }
    }
    if (table.isEmpty())
    {
        QDataStream footer(payload.right(footerBytes));
        footer.setByteOrder(QDataStream::LittleEndian);
        quint32 expected{};
        char magic[8]{};
        footer >> expected >> tableSize;
        footer.readRawData(magic, 8);
        if (footer.status() != QDataStream::Ok
            || QByteArray(magic, 8) != QByteArray(kFecFooterMagic, 8)
            || tableSize == 0
            || tableSize > static_cast<quint64>(payload.size() - footerBytes))
        {
            if (error) *error = QStringLiteral("Both protected-level descriptor tables are damaged.");
            return std::nullopt;
        }
        const qsizetype tableOffset = payload.size() - footerBytes
            - static_cast<qsizetype>(tableSize);
        table = payload.mid(tableOffset, static_cast<qsizetype>(tableSize));
        if (recoveryCrc32(table) != expected)
        {
            if (error) *error = QStringLiteral("Both protected-level descriptor tables failed CRC-32.");
            return std::nullopt;
        }
        usedFooterTable = true;
    }

    quint64 totalRawSize{};
    QByteArray totalHash{};
    std::vector<FecSectionDescriptor> descriptors{};
    if (!parseFecTable(table, &totalRawSize, &totalHash, &descriptors)
        || totalRawSize > static_cast<quint64>(kMaximumHeaderBytes))
    {
        if (error)
        {
            *error = QStringLiteral(
                "Protected recovery META descriptor table is invalid or exceeds its 1 MiB raw limit.");
        }
        return std::nullopt;
    }

    const qsizetype dataOffset = 8 + static_cast<qsizetype>(tableSize) + 4;
    qsizetype cursor = dataOffset;
    DecodedProtectedPayload decoded{};
    decoded.corrupted = usedFooterTable;
    if (usedFooterTable)
    {
        decoded.warnings.push_back(QStringLiteral(
            "The primary recovery parity table was damaged; its checked mirror was used."));
    }

    for (std::size_t sectionIndex = 0; sectionIndex < descriptors.size(); ++sectionIndex)
    {
        const FecSectionDescriptor& descriptor = descriptors[sectionIndex];
        const std::size_t totalShards = static_cast<std::size_t>(
            descriptor.dataShards + kFecParityShards);
        const quint64 sectionBytes64 = static_cast<quint64>(totalShards) * descriptor.shardSize;
        if (sectionBytes64 > static_cast<quint64>(std::numeric_limits<qsizetype>::max())
            || cursor < 0
            || sectionBytes64 > static_cast<quint64>(payload.size() - cursor))
        {
            if (error) *error = QStringLiteral("Protected level shard data is truncated.");
            return std::nullopt;
        }
        std::vector<QByteArray> shards{};
        std::vector<bool> valid{};
        shards.reserve(totalShards);
        valid.reserve(totalShards);
        for (std::size_t i = 0; i < totalShards; ++i)
        {
            QByteArray shard = payload.mid(cursor, descriptor.shardSize);
            cursor += descriptor.shardSize;
            valid.push_back(recoveryCrc32(shard) == descriptor.shardCrcs[i]);
            shards.push_back(std::move(shard));
        }

        const std::size_t damagedParityCount =
            static_cast<std::size_t>(!valid[descriptor.dataShards])
            + static_cast<std::size_t>(!valid[descriptor.dataShards + 1]);
        if (damagedParityCount > 0)
        {
            decoded.corrupted = true;
            decoded.warnings.push_back(QStringLiteral(
                "Recovery section %1 has %2 damaged parity shard(s); intact data remains usable.")
                .arg(sectionIndex)
                .arg(damagedParityCount));
        }

        std::vector<std::size_t> missingData{};
        for (std::size_t i = 0; i < descriptor.dataShards; ++i)
        {
            if (!valid[i]) missingData.push_back(i);
        }
        const std::size_t parityPIndex = descriptor.dataShards;
        const std::size_t parityQIndex = descriptor.dataShards + 1;
        bool repaired = true;
        if (missingData.size() == 1)
        {
            const std::size_t missing = missingData.front();
            QByteArray recovered(descriptor.shardSize, '\0');
            if (valid[parityPIndex])
            {
                recovered = shards[parityPIndex];
                for (std::size_t i = 0; i < descriptor.dataShards; ++i)
                {
                    if (i == missing) continue;
                    for (quint32 byte = 0; byte < descriptor.shardSize; ++byte)
                    {
                        recovered[byte] = static_cast<char>(
                            static_cast<unsigned char>(recovered.at(byte))
                            ^ static_cast<unsigned char>(shards[i].at(byte)));
                    }
                }
            }
            else if (valid[parityQIndex])
            {
                recovered = shards[parityQIndex];
                for (std::size_t i = 0; i < descriptor.dataShards; ++i)
                {
                    if (i == missing) continue;
                    const unsigned char coefficient = gfCoefficient(i);
                    for (quint32 byte = 0; byte < descriptor.shardSize; ++byte)
                    {
                        recovered[byte] = static_cast<char>(
                            static_cast<unsigned char>(recovered.at(byte))
                            ^ gfMul(coefficient, static_cast<unsigned char>(shards[i].at(byte))));
                    }
                }
                const unsigned char divisor = gfCoefficient(missing);
                for (quint32 byte = 0; byte < descriptor.shardSize; ++byte)
                {
                    recovered[byte] = static_cast<char>(
                        gfDiv(static_cast<unsigned char>(recovered.at(byte)), divisor));
                }
            }
            else
            {
                repaired = false;
            }
            if (repaired && recoveryCrc32(recovered) == descriptor.shardCrcs[missing])
            {
                shards[missing] = std::move(recovered);
                valid[missing] = true;
                ++decoded.repairedShardCount;
            }
            else
            {
                repaired = false;
            }
        }
        else if (missingData.size() == 2 && valid[parityPIndex] && valid[parityQIndex])
        {
            const std::size_t first = missingData[0];
            const std::size_t second = missingData[1];
            QByteArray p = shards[parityPIndex];
            QByteArray q = shards[parityQIndex];
            for (std::size_t i = 0; i < descriptor.dataShards; ++i)
            {
                if (i == first || i == second) continue;
                const unsigned char coefficient = gfCoefficient(i);
                for (quint32 byte = 0; byte < descriptor.shardSize; ++byte)
                {
                    const unsigned char value = static_cast<unsigned char>(shards[i].at(byte));
                    p[byte] = static_cast<char>(static_cast<unsigned char>(p.at(byte)) ^ value);
                    q[byte] = static_cast<char>(static_cast<unsigned char>(q.at(byte))
                        ^ gfMul(coefficient, value));
                }
            }
            QByteArray firstShard(descriptor.shardSize, '\0');
            QByteArray secondShard(descriptor.shardSize, '\0');
            const unsigned char firstCoefficient = gfCoefficient(first);
            const unsigned char secondCoefficient = gfCoefficient(second);
            const unsigned char divisor = firstCoefficient ^ secondCoefficient;
            for (quint32 byte = 0; byte < descriptor.shardSize; ++byte)
            {
                const unsigned char pValue = static_cast<unsigned char>(p.at(byte));
                const unsigned char qValue = static_cast<unsigned char>(q.at(byte));
                const unsigned char firstValue = gfDiv(
                    qValue ^ gfMul(secondCoefficient, pValue), divisor);
                firstShard[byte] = static_cast<char>(firstValue);
                secondShard[byte] = static_cast<char>(pValue ^ firstValue);
            }
            repaired = recoveryCrc32(firstShard) == descriptor.shardCrcs[first]
                && recoveryCrc32(secondShard) == descriptor.shardCrcs[second];
            if (repaired)
            {
                shards[first] = std::move(firstShard);
                shards[second] = std::move(secondShard);
                valid[first] = valid[second] = true;
                decoded.repairedShardCount += 2;
            }
        }
        else if (!missingData.empty())
        {
            repaired = false;
        }

        QByteArray raw{};
        if (repaired)
        {
            QByteArray stored{};
            stored.reserve(static_cast<qsizetype>(descriptor.storedSize));
            for (std::size_t i = 0; i < descriptor.dataShards; ++i)
            {
                stored.append(shards[i]);
            }
            stored.truncate(static_cast<qsizetype>(descriptor.storedSize));
            if (compressedSizePrefixMatches(stored, descriptor.rawSize))
            {
                raw = qUncompress(stored);
            }
            if (raw.size() != static_cast<qsizetype>(descriptor.rawSize)
                || QCryptographicHash::hash(raw, QCryptographicHash::Sha256)
                    != descriptor.rawHash)
            {
                raw.clear();
                repaired = false;
            }
        }

        if (!repaired)
        {
            decoded.corrupted = true;
            ++decoded.lostSectionCount;
            decoded.warnings.push_back(QStringLiteral(
                "Recovery section %1 had more damage than its two-shard parity could repair and was omitted.")
                .arg(sectionIndex));
            continue;
        }
        decoded.level.append(raw);
    }

    const qsizetype expectedMirrorOffset = payload.size() - footerBytes
        - static_cast<qsizetype>(tableSize);
    if (cursor != expectedMirrorOffset)
    {
        if (error) *error = QStringLiteral(
            "Protected recovery payload has an unexpected mirror/table boundary.");
        return std::nullopt;
    }

    if (decoded.repairedShardCount > 0)
    {
        decoded.corrupted = true;
        decoded.warnings.push_back(QStringLiteral(
            "Reed-Solomon parity repaired %1 damaged recovery shard(s).")
            .arg(decoded.repairedShardCount));
    }
    if (decoded.lostSectionCount == 0
        && (decoded.level.size() != static_cast<qsizetype>(totalRawSize)
            || QCryptographicHash::hash(decoded.level, QCryptographicHash::Sha256) != totalHash))
    {
        if (error) *error = QStringLiteral("Protected level failed its complete SHA-256 check.");
        return std::nullopt;
    }
    if (decoded.level.isEmpty())
    {
        if (error) *error = QStringLiteral("No healthy level sections remained.");
        return std::nullopt;
    }
    return decoded;
}

std::optional<DecodedProtectedPayload> decodeProtectedPayload(
    const QByteArray& payload,
    QString* error)
{
    constexpr qsizetype footerBytes = 4 + 8 + 8;
    if (payload.size() <= footerBytes + 12)
    {
        if (error) *error = QStringLiteral("Protected recovery payload is truncated.");
        return std::nullopt;
    }

    const auto primaryTable = [&]() -> QByteArray
    {
        QDataStream prefix(payload.left(8));
        prefix.setByteOrder(QDataStream::LittleEndian);
        quint64 tableSize{};
        prefix >> tableSize;
        if (prefix.status() != QDataStream::Ok || tableSize == 0
            || tableSize > static_cast<quint64>(payload.size() - 12))
        {
            return {};
        }
        const QByteArray table = payload.mid(8, static_cast<qsizetype>(tableSize));
        QDataStream crcStream(payload.mid(8 + static_cast<qsizetype>(tableSize), 4));
        crcStream.setByteOrder(QDataStream::LittleEndian);
        quint32 expected{};
        crcStream >> expected;
        return crcStream.status() == QDataStream::Ok
            && recoveryCrc32(table) == expected ? table : QByteArray{};
    };
    const auto footerTable = [&]() -> QByteArray
    {
        QDataStream footer(payload.right(footerBytes));
        footer.setByteOrder(QDataStream::LittleEndian);
        quint32 expected{};
        quint64 tableSize{};
        char magic[8]{};
        footer >> expected >> tableSize;
        footer.readRawData(magic, 8);
        if (footer.status() != QDataStream::Ok
            || QByteArray(magic, 8) != QByteArray(kFecFooterMagic, 8)
            || tableSize == 0
            || tableSize > static_cast<quint64>(payload.size() - footerBytes))
        {
            return {};
        }
        const QByteArray table = payload.mid(
            payload.size() - footerBytes - static_cast<qsizetype>(tableSize),
            static_cast<qsizetype>(tableSize));
        return recoveryCrc32(table) == expected ? table : QByteArray{};
    };

    const QByteArray primary = primaryTable();
    const QByteArray mirror = footerTable();
    if (primary.isEmpty() && mirror.isEmpty())
    {
        return decodeProtectedPayloadImpl(payload, error);
    }
    if (primary.isEmpty())
    {
        return decodeProtectedPayloadImpl(payload, error, mirror, true);
    }
    if (mirror.isEmpty())
    {
        auto result = decodeProtectedPayloadImpl(payload, error, primary, false);
        if (result)
        {
            result->corrupted = true;
            result->warnings.push_back(QStringLiteral(
                "The primary META parity table was valid, but its checked mirror/footer was damaged."));
        }
        return result;
    }
    if (primary == mirror)
    {
        return decodeProtectedPayloadImpl(payload, error, primary, false);
    }

    // Both tables are CRC-valid but disagree. Decode both independently and
    // trust only a candidate that also validates every shard CRC and the
    // expanded section SHA/total hash. Never silently prefer the primary.
    const auto primaryDecoded = decodeProtectedPayloadImpl(payload, nullptr, primary, false);
    const auto mirrorDecoded = decodeProtectedPayloadImpl(payload, nullptr, mirror, true);
    const auto usable = [](const std::optional<DecodedProtectedPayload>& candidate)
    {
        return candidate && candidate->lostSectionCount == 0 && !candidate->level.isEmpty();
    };
    const bool primaryUsable = usable(primaryDecoded);
    const bool mirrorUsable = usable(mirrorDecoded);
    if (primaryUsable && !mirrorUsable)
    {
        auto result = primaryDecoded;
        result->corrupted = true;
        result->warnings.push_back(QStringLiteral(
            "The mirrored META parity tables disagree; the independently checked primary table was uniquely valid."));
        return result;
    }
    if (!primaryUsable && mirrorUsable)
    {
        auto result = mirrorDecoded;
        result->corrupted = true;
        result->warnings.push_back(QStringLiteral(
            "The mirrored META parity tables disagree; the independently checked mirror table was uniquely valid."));
        return result;
    }
    if (primaryUsable && mirrorUsable && primaryDecoded->level == mirrorDecoded->level)
    {
        auto result = primaryDecoded;
        result->corrupted = true;
        result->warnings.push_back(QStringLiteral(
            "Both CRC-valid META parity tables disagreed but independently reconstructed the same payload."));
        return result;
    }
    if (error)
    {
        *error = QStringLiteral(
            "Mirrored META parity tables disagree and no unique independently checked decode exists.");
    }
    return std::nullopt;
}

struct ProtectedParityTableCandidate
{
    QByteArray table{};
    bool footerTable{};
};

std::vector<ProtectedParityTableCandidate> validProtectedParityTables(
    const QByteArray& payload)
{
    constexpr qsizetype footerBytes = 4 + 8 + 8;
    std::vector<ProtectedParityTableCandidate> tables{};
    if (payload.size() <= footerBytes + 12) return tables;

    QDataStream prefix(payload.left(8));
    prefix.setByteOrder(QDataStream::LittleEndian);
    quint64 primarySize{};
    prefix >> primarySize;
    if (prefix.status() == QDataStream::Ok && primarySize > 0
        && primarySize <= static_cast<quint64>(payload.size() - 12))
    {
        const QByteArray table = payload.mid(8, static_cast<qsizetype>(primarySize));
        QDataStream crcStream(payload.mid(
            8 + static_cast<qsizetype>(primarySize), 4));
        crcStream.setByteOrder(QDataStream::LittleEndian);
        quint32 expected{};
        crcStream >> expected;
        if (crcStream.status() == QDataStream::Ok
            && recoveryCrc32(table) == expected)
        {
            tables.push_back(ProtectedParityTableCandidate{ table, false });
        }
    }

    QDataStream footer(payload.right(footerBytes));
    footer.setByteOrder(QDataStream::LittleEndian);
    quint32 expected{};
    quint64 mirrorSize{};
    char magic[8]{};
    footer >> expected >> mirrorSize;
    footer.readRawData(magic, 8);
    if (footer.status() == QDataStream::Ok
        && QByteArray(magic, 8) == QByteArray(kFecFooterMagic, 8)
        && mirrorSize > 0
        && mirrorSize <= static_cast<quint64>(payload.size() - footerBytes))
    {
        const QByteArray table = payload.mid(
            payload.size() - footerBytes - static_cast<qsizetype>(mirrorSize),
            static_cast<qsizetype>(mirrorSize));
        if (recoveryCrc32(table) == expected)
        {
            tables.push_back(ProtectedParityTableCandidate{ table, true });
        }
    }
    return tables;
}

struct ProtectedMetadataAdmission
{
    DecodedProtectedPayload decoded{};
    RecoveryMetadata metadata{};
};

std::optional<ProtectedMetadataAdmission> admitProtectedMetadataDecoded(
    const QByteArray& stored,
    const DecodedProtectedPayload& decoded,
    const std::optional<quint64>& expectedRawSize,
    const QByteArray& expectedSha256,
    QString* error)
{
    const auto fail = [error](const QString& message)
    {
        if (error) *error = message;
        return std::optional<ProtectedMetadataAdmission>{};
    };
    if (stored.isEmpty()
        || static_cast<quint64>(stored.size()) > kMaximumMetadataProtectedBytes)
    {
        return fail(QStringLiteral(
            "Protected recovery META exceeds the 4 MiB admission limit."));
    }

    if (decoded.lostSectionCount != 0 || decoded.level.isEmpty())
    {
        return fail(QStringLiteral(
            "Protected recovery META has lost FEC sections and cannot be used."));
    }
    if (static_cast<quint64>(decoded.level.size())
            > static_cast<quint64>(kMaximumHeaderBytes))
    {
        return fail(QStringLiteral(
            "Protected recovery META JSON exceeds the 1 MiB admission limit."));
    }
    if (expectedRawSize && *expectedRawSize
            != static_cast<quint64>(decoded.level.size()))
    {
        return fail(QStringLiteral(
            "Protected recovery META raw-size binding failed."));
    }
    if (!expectedSha256.isEmpty()
        && (expectedSha256.size() != QCryptographicHash::hashLength(
                QCryptographicHash::Sha256)
            || QCryptographicHash::hash(decoded.level, QCryptographicHash::Sha256)
                != expectedSha256))
    {
        return fail(QStringLiteral(
            "Protected recovery META SHA-256 binding failed."));
    }

    // A valid parity table can still be a malicious, CRC-consistent table with
    // multiple logical units. Only a single descriptor whose complete table
    // and section hashes match the independently decoded payload may admit
    // metadata. A damaged alternate mirror is allowed when the decoder has
    // uniquely selected the surviving canonical table.
    bool canonicalTableFound = false;
    for (const ProtectedParityTableCandidate& candidate
         : validProtectedParityTables(stored))
    {
        quint64 totalRawSize{};
        QByteArray totalHash{};
        std::vector<FecSectionDescriptor> descriptors{};
        if (!parseFecTable(candidate.table, &totalRawSize, &totalHash, &descriptors)
            || descriptors.size() != 1
            || totalRawSize != static_cast<quint64>(decoded.level.size())
            || totalRawSize > static_cast<quint64>(kMaximumHeaderBytes)
            || totalHash != QCryptographicHash::hash(
                decoded.level, QCryptographicHash::Sha256))
        {
            continue;
        }
        const FecSectionDescriptor& descriptor = descriptors.front();
        const quint32 expectedDataShards = std::min<quint32>(
            kFecDataShards,
            std::max<quint32>(1, static_cast<quint32>(descriptor.storedSize)));
        if (descriptor.rawSize != totalRawSize
            || descriptor.rawSize > static_cast<quint64>(kMaximumHeaderBytes)
            || descriptor.storedSize == 0
            || descriptor.storedSize > kMaximumMetadataProtectedBytes
            || descriptor.dataShards != expectedDataShards
            || descriptor.rawHash != totalHash)
        {
            continue;
        }
        // Validate provenance: a table is admissible only if independently
        // decoding the payload with that exact primary/footer table produces
        // the metadata that the arbitration decoder returned. Merely matching
        // total size/SHA declarations is insufficient when a crafted
        // multi-descriptor table and a valid mirror cross-bind their claims.
        const auto candidateDecoded = decodeProtectedPayloadImpl(
            stored, nullptr, candidate.table, candidate.footerTable);
        if (!candidateDecoded || candidateDecoded->lostSectionCount != 0
            || candidateDecoded->level != decoded.level)
        {
            continue;
        }
        canonicalTableFound = true;
        break;
    }
    if (!canonicalTableFound)
    {
        return fail(QStringLiteral(
            "Protected recovery META has no single complete CRC-checked FEC unit."));
    }

    int metadataVersion{};
    QString metadataError{};
    const auto metadata = metadataFromJson(
        decoded.level, &metadataVersion, &metadataError);
    if (!metadata || metadataVersion != 1)
    {
        return fail(metadataError.isEmpty()
            ? QStringLiteral("Protected recovery META JSON is not V1.")
            : metadataError);
    }
    if (!recoveryMetadataAssociationCoherent(*metadata))
    {
        return fail(QStringLiteral(
            "Protected recovery META source association is incoherent."));
    }

    return ProtectedMetadataAdmission{ decoded, *metadata };
}

std::optional<ProtectedMetadataAdmission> admitProtectedMetadata(
    const QByteArray& stored,
    const std::optional<quint64>& expectedRawSize,
    const QByteArray& expectedSha256,
    QString* error)
{
    if (stored.isEmpty()
        || static_cast<quint64>(stored.size()) > kMaximumMetadataProtectedBytes)
    {
        if (error)
        {
            *error = QStringLiteral(
                "Protected recovery META exceeds the 4 MiB admission limit.");
        }
        return std::nullopt;
    }
    QString decodeError{};
    const auto decoded = decodeProtectedPayload(stored, &decodeError);
    if (!decoded)
    {
        if (error)
        {
            *error = decodeError.isEmpty()
                ? QStringLiteral("Protected recovery META failed parity validation.")
                : decodeError;
        }
        return std::nullopt;
    }
    return admitProtectedMetadataDecoded(
        stored, *decoded, expectedRawSize, expectedSha256, error);
}

struct EncodedChunk
{
    std::array<char, 4> type{};
    QByteArray raw{};
    QByteArray stored{};
    quint32 flags{};
    quint32 crc{};
    QByteArray sha256{};
};

EncodedChunk makeChunk(const char (&type)[5], QByteArray raw)
{
    if (raw.isEmpty() || static_cast<quint64>(raw.size()) > kMaximumWarningBytes)
    {
        throw std::runtime_error("Recovery warn payload size is out of range.");
    }
    EncodedChunk chunk{};
    std::copy_n(type, 4, chunk.type.begin());
    chunk.raw = std::move(raw);
    const QByteArray compressed = qCompress(chunk.raw, 6);
    if (!compressed.isEmpty() && compressed.size() < chunk.raw.size())
    {
        chunk.stored = compressed;
        chunk.flags = kChunkCompressed;
    }
    else
    {
        // WARN is intentionally compressed only when that makes the payload
        // smaller. DOCM uses makeNativeDocumentChunk below and is never
        // compressed or redundantly hashed by the outer envelope.
        chunk.stored = chunk.raw;
        chunk.flags = 0;
    }
    if (chunk.stored.isEmpty()
        || static_cast<quint64>(chunk.stored.size()) > kMaximumWarningBytes)
    {
        throw std::runtime_error("Recovery WARN encoding failed.");
    }
    chunk.crc = recoveryCrc32(chunk.stored);
    chunk.sha256 = QCryptographicHash::hash(chunk.raw, QCryptographicHash::Sha256);
    return chunk;
}

EncodedChunk makeNativeDocumentChunk(QByteArray raw)
{
    if (raw.isEmpty() || static_cast<quint64>(raw.size()) > kMaximumChunkBytes)
    {
        throw std::runtime_error("Recovery raw chunk payload size is out of range.");
    }
    EncodedChunk chunk{};
    std::copy_n("docm", 4, chunk.type.begin());
    chunk.raw = std::move(raw);
    chunk.stored = chunk.raw;
    chunk.flags = kChunkNativeSed;
    chunk.crc = 0;
    chunk.sha256 = QByteArray(
        QCryptographicHash::hashLength(QCryptographicHash::Sha256), '\0');
    return chunk;
}

EncodedChunk makeProtectedChunk(
    const char (&type)[5],
    QByteArray raw)
{
    if (type[0] == 'm' && type[1] == 'e' && type[2] == 't' && type[3] == 'a'
        && static_cast<quint64>(raw.size())
            > static_cast<quint64>(kMaximumHeaderBytes))
    {
        throw std::runtime_error(
            "Protected recovery meta JSON exceeds the 1 MiB writer limit.");
    }
    EncodedChunk chunk{};
    std::copy_n(type, 4, chunk.type.begin());
    chunk.raw = std::move(raw);
    chunk.stored = makeProtectedPayload(chunk.raw);
    chunk.flags = kChunkProtected;
    chunk.crc = recoveryCrc32(chunk.stored);
    chunk.sha256 = QCryptographicHash::hash(chunk.raw, QCryptographicHash::Sha256);
    return chunk;
}

bool encodedChunkSelfCheck(const EncodedChunk& chunk, QString* error)
{
    const auto fail = [error](const QString& message)
    {
        if (error) *error = message;
        return false;
    };
    if (chunk.raw.isEmpty() || chunk.stored.isEmpty()
        || static_cast<quint64>(chunk.raw.size()) > kMaximumChunkBytes
        || static_cast<quint64>(chunk.stored.size()) > kMaximumChunkBytes)
    {
        return fail(QStringLiteral("generated chunk size verification failed"));
    }
    if (chunk.flags == kChunkNativeSed)
    {
        if (chunk.stored != chunk.raw || chunk.crc != 0
            || chunk.sha256 != QByteArray(
                QCryptographicHash::hashLength(QCryptographicHash::Sha256), '\0')
            || !hasRecoverySedSignature(chunk.raw))
        {
            return fail(QStringLiteral("generated native DOCM envelope is non-canonical"));
        }
        // writeRecoverySnapshot already required an exact semantic native SED
        // decode/re-encode before constructing this envelope. Repeating that
        // full pass here would only duplicate work; this check owns framing.
        return true;
    }
    if (recoveryCrc32(chunk.stored) != chunk.crc
        || QCryptographicHash::hash(chunk.raw, QCryptographicHash::Sha256) != chunk.sha256)
    {
        return fail(QStringLiteral("generated chunk checksum verification failed"));
    }
    if (chunk.flags == kChunkProtected)
    {
        QString decodeError{};
        const auto decoded = decodeProtectedPayload(chunk.stored, &decodeError);
        if (!decoded || decoded->corrupted || decoded->lostSectionCount != 0
            || decoded->level != chunk.raw)
        {
            return fail(decodeError.isEmpty()
                ? QStringLiteral("generated protected chunk did not round-trip exactly")
                : decodeError);
        }
        return true;
    }
    if (chunk.flags == kChunkCompressed)
    {
        if (!compressedSizePrefixMatches(chunk.stored, chunk.raw.size())
            || qUncompress(chunk.stored) != chunk.raw)
        {
            return fail(QStringLiteral("generated compressed chunk did not round-trip exactly"));
        }
        return true;
    }
    if (chunk.flags == 0)
    {
        if (chunk.stored != chunk.raw)
        {
            return fail(QStringLiteral("generated raw chunk did not preserve its payload"));
        }
        return true;
    }
    return fail(QStringLiteral("generated chunk uses unsupported flags"));
}

QByteArray chunkHeader(const EncodedChunk& chunk)
{
    QByteArray header{};
    header.reserve(kChunkHeaderBytes);
    QDataStream stream(&header, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData(kChunkMagic, 8);
    stream.writeRawData(chunk.type.data(), 4);
    stream << chunk.flags
           << static_cast<quint64>(chunk.stored.size())
           << static_cast<quint64>(chunk.raw.size())
           << chunk.crc;
    stream.writeRawData(chunk.sha256.constData(), chunk.sha256.size());
    if (stream.status() != QDataStream::Ok || header.size() != kChunkHeaderBytes)
    {
        throw std::runtime_error("Recovery chunk header encoding failed.");
    }
    return header;
}

struct ParsedRecoveryFile
{
    RecoveryMetadata metadata{};
    std::int64_t documentTimestamp{};
    QByteArray payload{};
    std::shared_ptr<Level> decodedLevel{};
    QByteArray editorStatePayload{};
    QByteArray deferredHistoryPayload{};
    std::optional<editor::DocumentRecoveryHistory> history{};
    WorkspaceHistoryState workspaceHistory{};
    QStringList warnings{};
    bool metadataChunkDecoded{};
    bool metadataCopiesConflict{};
    bool partialRecovery{};
    bool corrupted{};
    std::uint32_t repairedNativeChunkCount{};
    std::uint32_t repairedMetadataShardCount{};
    QStringList missingSections{};
    std::vector<RecoveryUnreadableRange> unreadableRanges{};
    std::uint32_t lostSectionCount{};
    std::uint32_t unidentifiedMissingSectionCount{};
    QByteArray canonicalLevelSha256{};
    QByteArray canonicalDocumentSha256{};
    std::optional<ProjectType> nativeProjectType{};
    bool nativeProjectTypeTrusted{};
    bool documentCopiesConflict{};
    bool documentBindingMismatch{};
};

void markMissingSection(ParsedRecoveryFile& parsed, const QString& section)
{
    parsed.partialRecovery = true;
    if (!parsed.missingSections.contains(section))
    {
        parsed.missingSections.push_back(section);
        ++parsed.lostSectionCount;
    }
}

void verifyRecoveryDocumentBinding(ParsedRecoveryFile& parsed)
{
    if (parsed.metadata.identity.isEmpty() || parsed.payload.isEmpty()) return;

    const QByteArray expectedLevel = parsed.metadata.levelSha256;
    const QByteArray expectedDocument = parsed.metadata.documentSha256;
    // Orphan salvage intentionally has no trustworthy metadata digest. It is
    // still surfaced as corrupted/detached, but must not be rejected merely
    // because its source association was already lost.
    if (expectedLevel.isEmpty() && expectedDocument.isEmpty()) return;

    if (parsed.canonicalLevelSha256.isEmpty())
    {
        parsed.corrupted = true;
        parsed.partialRecovery = true;
        parsed.warnings.push_back(QStringLiteral(
            "The recovered Level could not be canonically re-encoded; metadata binding was not verifiable."));
        return;
    }

    if (!expectedLevel.isEmpty()
        && parsed.canonicalLevelSha256 != expectedLevel)
    {
        parsed.documentBindingMismatch = true;
        parsed.corrupted = true;
        parsed.warnings.push_back(QStringLiteral(
            "Recovery metadata does not match the canonical required Level; the native payload may have been transplanted."));
        return;
    }

    if (!parsed.canonicalDocumentSha256.isEmpty() && !expectedDocument.isEmpty())
    {
        if (parsed.canonicalDocumentSha256 != expectedDocument)
        {
            parsed.documentBindingMismatch = true;
            parsed.corrupted = true;
            parsed.warnings.push_back(QStringLiteral(
                "Recovery metadata does not match the canonical native DOCM; the snapshot may contain a conflicting editor-state payload."));
        }
    }
    else if (!expectedDocument.isEmpty())
    {
        parsed.corrupted = true;
        parsed.warnings.push_back(QStringLiteral(
            "Native repair or optional editor-section loss prevented checking the complete DOCM binding, but the required Level binding passed."));
    }
}

qint64 seekNextSequence(
    QIODevice& file,
    const QByteArray& needle,
    qint64 start,
    const qint64 end)
{
    constexpr qint64 blockSize = 64 * 1024;
    if (needle.isEmpty() || start < 0 || end <= start) return -1;
    QByteArray overlap{};
    qint64 cursor = std::max<qint64>(0, start);
    while (cursor < end && file.seek(cursor))
    {
        const qint64 remaining = end - cursor;
        const QByteArray block = file.read(std::min(blockSize, remaining));
        if (block.isEmpty()) return -1;
        const QByteArray search = overlap + block;
        const qsizetype found = search.indexOf(needle);
        if (found >= 0)
        {
            const qint64 absolute = cursor - overlap.size() + found;
            file.seek(absolute);
            return absolute;
        }
        overlap = search.right(needle.size() - 1);
        cursor += block.size();
    }
    return -1;
}

qint64 seekNextFecTableMagic(QIODevice& file, const qint64 start)
{
    return seekNextSequence(
        file,
        QByteArray(kFecTableMagic, 8),
        start,
        std::min(file.size(), kMaximumMetadataScanBytes));
}

template <typename AcceptPayload>
std::optional<DecodedProtectedPayload> recoverProtectedPayloadWithoutChunkHeader(
    QIODevice& file,
    AcceptPayload acceptPayload)
{
    // The protected payload owns a complete checked descriptor table and total
    // complete hash. This provides an independent route when the small outer
    // header is the only damaged part of an otherwise healthy checkpoint.
    qint64 searchFrom = 0;
    int candidates = 0;
    while (candidates++ < 64)
    {
        const qint64 tableMagic = seekNextFecTableMagic(file, searchFrom);
        if (tableMagic < 8) break;
        searchFrom = tableMagic + 8;
        const qint64 payloadStart = tableMagic - 8;
        if (!file.seek(payloadStart)) continue;
        QDataStream prefix(file.read(8));
        prefix.setByteOrder(QDataStream::LittleEndian);
        quint64 tableSize{};
        prefix >> tableSize;
        if (prefix.status() != QDataStream::Ok || tableSize == 0
            || tableSize > static_cast<quint64>(kMaximumHeaderBytes)
            || tableSize > static_cast<quint64>(file.size() - tableMagic))
        {
            continue;
        }
        const QByteArray table = file.read(static_cast<qint64>(tableSize));
        QDataStream crcStream(file.read(4));
        crcStream.setByteOrder(QDataStream::LittleEndian);
        quint32 expectedTableCrc{};
        crcStream >> expectedTableCrc;
        quint64 totalRawSize{};
        QByteArray totalHash{};
        std::vector<FecSectionDescriptor> descriptors{};
        if (table.size() != static_cast<qint64>(tableSize)
            || crcStream.status() != QDataStream::Ok
            || recoveryCrc32(table) != expectedTableCrc
            || !parseFecTable(table, &totalRawSize, &totalHash, &descriptors)
            || totalRawSize > static_cast<quint64>(kMaximumHeaderBytes)
            || descriptors.size() != 1)
        {
            continue;
        }

        quint64 shardBytes{};
        bool sizeValid = true;
        for (const FecSectionDescriptor& descriptor : descriptors)
        {
            const quint64 shardCount = descriptor.dataShards + kFecParityShards;
            if (descriptor.shardSize > 0
                && shardCount > (kMaximumChunkBytes - shardBytes) / descriptor.shardSize)
            {
                sizeValid = false;
                break;
            }
            shardBytes += shardCount * descriptor.shardSize;
        }
        constexpr quint64 footerBytes = 4 + 8 + 8;
        const quint64 fixedBytes = 8 + tableSize + 4 + tableSize + footerBytes;
        if (!sizeValid || fixedBytes > kMaximumMetadataProtectedBytes
            || shardBytes > kMaximumMetadataProtectedBytes - fixedBytes)
        {
            continue;
        }
        const quint64 payloadSize = fixedBytes + shardBytes;
        if (payloadSize > static_cast<quint64>(file.size() - payloadStart)
            || !file.seek(payloadStart))
        {
            continue;
        }
        const QByteArray payload = file.read(static_cast<qint64>(payloadSize));
        const auto decoded = decodeProtectedPayload(payload, nullptr);
        if (decoded && acceptPayload(payload, *decoded))
        {
            return decoded;
        }
    }

    // If the outer META chunk header and its primary FEC prefix/table were
    // damaged, the checked mirror footer still gives an exact V1 payload
    // boundary. Reconstruct that boundary from the footer table and its shard
    // geometry instead of accepting an arbitrary byte span around a magic
    // sequence. The canonical admission callback remains the final authority.
    searchFrom = 0;
    candidates = 0;
    while (candidates++ < 64)
    {
        const qint64 footerMagic = seekNextSequence(
            file, QByteArray(kFecFooterMagic, 8), searchFrom,
            std::min(file.size(), kMaximumMetadataScanBytes));
        if (footerMagic < 12) break;
        searchFrom = footerMagic + 8;
        const qint64 footerStart = footerMagic - 12;
        if (!file.seek(footerStart)) continue;
        QDataStream footer(file.read(12));
        footer.setByteOrder(QDataStream::LittleEndian);
        quint32 expectedTableCrc{};
        quint64 tableSize{};
        footer >> expectedTableCrc >> tableSize;
        if (footer.status() != QDataStream::Ok || tableSize == 0
            || tableSize > static_cast<quint64>(kMaximumHeaderBytes)
            || tableSize > static_cast<quint64>(footerMagic - 12))
        {
            continue;
        }
        // The footer contributes its CRC and size fields before fecend1: the
        // mirror table starts 12 bytes before the magic, not immediately
        // before it.
        const qint64 mirrorOffset = footerMagic - 12
            - static_cast<qint64>(tableSize);
        if (mirrorOffset < 0 || !file.seek(mirrorOffset)) continue;
        const QByteArray mirror = file.read(static_cast<qint64>(tableSize));
        if (mirror.size() != static_cast<qint64>(tableSize)
            || recoveryCrc32(mirror) != expectedTableCrc)
        {
            continue;
        }

        quint64 totalRawSize{};
        QByteArray totalHash{};
        std::vector<FecSectionDescriptor> descriptors{};
        if (!parseFecTable(mirror, &totalRawSize, &totalHash, &descriptors))
        {
            continue;
        }
        quint64 shardBytes{};
        bool sizeValid = true;
        for (const FecSectionDescriptor& descriptor : descriptors)
        {
            const quint64 shardCount = descriptor.dataShards + kFecParityShards;
            if (descriptor.shardSize == 0
                || shardBytes > kMaximumMetadataProtectedBytes
                || shardCount > (kMaximumMetadataProtectedBytes - shardBytes)
                    / descriptor.shardSize)
            {
                sizeValid = false;
                break;
            }
            shardBytes += shardCount * descriptor.shardSize;
        }
        constexpr quint64 footerBytes = 4 + 8 + 8;
        const quint64 fixedBytes = 8 + tableSize + 4 + tableSize + footerBytes;
        if (!sizeValid || fixedBytes > kMaximumMetadataProtectedBytes
            || shardBytes > kMaximumMetadataProtectedBytes - fixedBytes)
        {
            continue;
        }
        const qint64 payloadStart = mirrorOffset
            - static_cast<qint64>(shardBytes)
            - 4 - static_cast<qint64>(tableSize) - 8;
        const quint64 payloadSize = fixedBytes + shardBytes;
        if (payloadStart < 0
            || payloadSize > static_cast<quint64>(std::numeric_limits<qint64>::max())
            || payloadStart > file.size()
            || payloadSize > static_cast<quint64>(file.size() - payloadStart)
            || payloadStart + static_cast<qint64>(payloadSize) != footerMagic + 8
            || !file.seek(payloadStart))
        {
            continue;
        }
        const QByteArray payload = file.read(static_cast<qint64>(payloadSize));
        const auto decoded = decodeProtectedPayload(payload, nullptr);
        if (decoded && acceptPayload(payload, *decoded))
        {
            return decoded;
        }
    }
    return std::nullopt;
}

std::optional<DecodedProtectedPayload> recoverMetadataWithoutChunkHeader(QIODevice& file)
{
    return recoverProtectedPayloadWithoutChunkHeader(file,
        [](const QByteArray& stored, const DecodedProtectedPayload& decoded)
        {
            // Headerless discovery uses the exact same admission rules as the
            // normal META path. The decoded value is passed in so the scanner
            // does not perform a second parity reconstruction.
            return admitProtectedMetadataDecoded(
                stored, decoded, std::nullopt, {}, nullptr).has_value();
        });
}

struct RecoveryMetadataScan
{
    std::optional<RecoveryMetadata> metadata{};
    bool primaryValid{};
    bool redundantRecoveredWithoutHeader{};
    bool fatal{};
    QString error{};
};

std::optional<RecoveryMetadata> readPrimaryRecoveryMetadata(QIODevice& file)
{
    if (!file.seek(0)) return std::nullopt;
    const auto readBoundedLine = [&file](const qint64 maximumBytes)
        -> std::optional<QByteArray>
    {
        const QByteArray line = file.readLine(maximumBytes + 1);
        if (line.isEmpty() || line.size() > maximumBytes
            || !line.endsWith('\n'))
        {
            return std::nullopt;
        }
        return line;
    };

    const auto magic = readBoundedLine(64);
    const auto sizeLine = readBoundedLine(32);
    const auto hashLine = readBoundedLine(96);
    if (!magic || *magic != QByteArray(kRecoveryMagicV1)
        || !sizeLine || !hashLine)
    {
        return std::nullopt;
    }

    bool sizeOk = false;
    const qint64 headerSize = sizeLine->trimmed().toLongLong(&sizeOk);
    const QByteArray encodedHash = hashLine->trimmed();
    const QByteArray expectedHash = QByteArray::fromHex(encodedHash);
    if (!sizeOk || headerSize <= 0 || headerSize > kMaximumHeaderBytes
        || encodedHash.size() != 64 || expectedHash.size() != 32
        || encodedHash != expectedHash.toHex()
        || headerSize > file.size() - file.pos())
    {
        return std::nullopt;
    }

    const QByteArray header = file.read(headerSize);
    int metadataVersion{};
    if (header.size() != headerSize
        || QCryptographicHash::hash(header, QCryptographicHash::Sha256)
            != expectedHash)
    {
        return std::nullopt;
    }
    const auto metadata = metadataFromJson(header, &metadataVersion, nullptr);
    return metadata && metadataVersion == 1 ? metadata : std::nullopt;
}

std::optional<RecoveryMetadata> readOuterRecoveryMetadataChunk(
    QIODevice& file,
    bool* copiesConflict)
{
    if (copiesConflict) *copiesConflict = false;
    qint64 searchFrom = 0;
    qint64 remainingScanBytes = kMaximumMetadataScanBytes;
    int candidates = 0;
    std::optional<RecoveryMetadata> result{};
    while (searchFrom < file.size() && remainingScanBytes > 0
        && candidates++ < kMaximumOuterBoundaryCandidates)
    {
        const qint64 available = file.size() - searchFrom;
        const qint64 scanBytes = std::min(available, remainingScanBytes);
        const qint64 scanEnd = searchFrom + scanBytes;
        const qint64 chunkStart = seekNextSequence(
            file, QByteArray(kChunkMagic, 8), searchFrom, scanEnd);
        if (chunkStart < 0 || chunkStart > file.size() - kChunkHeaderBytes)
        {
            break;
        }
        // Charge only bytes actually searched. A validated DOCM extent below
        // is skipped with seek arithmetic and therefore does not consume the
        // metadata-discovery I/O budget.
        remainingScanBytes -= std::max<qint64>(1, chunkStart - searchFrom + 1);
        searchFrom = chunkStart + 1;
        if (!file.seek(chunkStart)) continue;
        const QByteArray header = file.read(kChunkHeaderBytes);
        if (header.size() != kChunkHeaderBytes) continue;

        QDataStream stream(header);
        stream.setByteOrder(QDataStream::LittleEndian);
        char magic[8]{}, typeChars[4]{}, hashChars[32]{};
        quint32 flags{}, expectedCrc{};
        quint64 storedSize{}, rawSize{};
        stream.readRawData(magic, 8);
        stream.readRawData(typeChars, 4);
        stream >> flags >> storedSize >> rawSize >> expectedCrc;
        stream.readRawData(hashChars, 32);
        const QByteArray type(typeChars, 4);
        const qint64 payloadOffset = chunkStart + kChunkHeaderBytes;
        if (stream.status() != QDataStream::Ok
            || QByteArray(magic, 8) != QByteArray(kChunkMagic, 8))
        {
            continue;
        }
        if (type == QByteArrayLiteral("docm"))
        {
            // Never search inside the potentially huge native document for an
            // accidental outer marker. A canonical native envelope gives us a
            // bounded, range-checked extent that can be skipped without
            // materializing or decoding DOCM. Continue after it so a second
            // valid-but-conflicting META cannot hide behind the document from
            // descriptor-only validation.
            const bool canonicalNativeEnvelope = flags == kChunkNativeSed
                && storedSize == rawSize && storedSize != 0
                && storedSize <= assets::SedMaximumFileBytes
                && expectedCrc == 0
                && QByteArray(hashChars, 32) == QByteArray(32, '\0')
                && storedSize
                    <= static_cast<quint64>(file.size() - payloadOffset);
            if (!canonicalNativeEnvelope)
            {
                // Its declared extent is not trustworthy, so do not scan its
                // bytes as outer framing. Primary metadata can still be used,
                // but full recovery validation will reject a malformed DOCM.
                break;
            }
            searchFrom = payloadOffset + static_cast<qint64>(storedSize);
            continue;
        }
        if (type != QByteArrayLiteral("meta") || flags != kChunkProtected
            || storedSize == 0 || rawSize == 0
            || storedSize > kMaximumMetadataProtectedBytes
            || rawSize > static_cast<quint64>(kMaximumHeaderBytes)
            || storedSize > static_cast<quint64>(file.size() - payloadOffset))
        {
            continue;
        }
        if (!file.seek(payloadOffset)) continue;
        const QByteArray stored = file.read(static_cast<qint64>(storedSize));
        if (stored.size() != static_cast<qint64>(storedSize)) continue;
        searchFrom = payloadOffset + static_cast<qint64>(storedSize);

        QString admissionError{};
        const auto admission = admitProtectedMetadata(
            stored,
            std::optional<quint64>(rawSize),
            QByteArray(hashChars, 32),
            &admissionError);
        if (!admission) continue;
        const RecoveryMetadata& metadata = admission->metadata;
        if (result && !recoveryMetadataEquivalent(*result, metadata))
        {
            if (copiesConflict) *copiesConflict = true;
            return std::nullopt;
        }
        result = metadata;
        (void)expectedCrc; // Inner checked data remains authoritative on CRC damage.
    }
    return result;
}

RecoveryMetadataScan scanRecoveryMetadataCopies(QIODevice& file)
{
    RecoveryMetadataScan scan{};
    const std::optional<RecoveryMetadata> primary =
        readPrimaryRecoveryMetadata(file);
    scan.primaryValid = primary.has_value();

    bool redundantCopiesConflict = false;
    std::optional<RecoveryMetadata> redundant =
        readOuterRecoveryMetadataChunk(file, &redundantCopiesConflict);
    if (redundantCopiesConflict)
    {
        scan.fatal = true;
        scan.error = QStringLiteral(
            "Multiple valid protected recovery metadata copies disagree.");
        return scan;
    }
    if (!redundant && file.seek(0))
    {
        const auto decoded = recoverMetadataWithoutChunkHeader(file);
        int version{};
        redundant = decoded
            ? metadataFromJson(decoded->level, &version, nullptr)
            : std::nullopt;
        if (redundant && version == 1)
        {
            scan.redundantRecoveredWithoutHeader = true;
        }
        else
        {
            redundant.reset();
        }
    }

    if (primary && redundant
        && !recoveryMetadataEquivalent(*primary, *redundant))
    {
        scan.fatal = true;
        scan.error = QStringLiteral(
            "The valid primary and protected recovery metadata copies disagree; source association was rejected safely.");
        return scan;
    }
    scan.metadata = primary ? primary : redundant;
    if (scan.metadata
        && !recoveryMetadataAssociationCoherent(*scan.metadata))
    {
        scan.metadata.reset();
        scan.fatal = true;
        scan.error = QStringLiteral(
            "Recovery metadata identity does not match its source path/session association.");
    }
    return scan;
}

void appendChunkWarning(ParsedRecoveryFile& parsed, const QByteArray& type, const QString& detail)
{
    parsed.corrupted = true;
    parsed.warnings.push_back(QStringLiteral("Recovery chunk %1 was skipped: %2")
        .arg(QString::fromLatin1(type), detail));
}

bool synthesizeMetadataFromDecodedDocument(
    ParsedRecoveryFile& parsed,
    const qint64 fallbackModifiedAtMs,
    QString* error)
{
    if (parsed.payload.isEmpty())
    {
        if (error)
        {
            *error = QStringLiteral(
                "Recovery metadata synthesis requires an already validated native document.");
        }
        return false;
    }

    RecoveryMetadata metadata{};
    metadata.identity = QStringLiteral("orphan:%1").arg(QString::fromLatin1(
        QCryptographicHash::hash(
            parsed.payload, QCryptographicHash::Sha256).toHex()));
    metadata.sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    metadata.displayName = QStringLiteral("Recovered damaged session");
    metadata.timestamp = fallbackModifiedAtMs > 0
        ? fallbackModifiedAtMs
        : QDateTime::currentMSecsSinceEpoch();
    metadata.sourceModifiedAtMs = -1;
    metadata.undoSignature = 0;
    if (parsed.nativeProjectType)
    {
        metadata.projectType = *parsed.nativeProjectType;
    }
    metadata.projectTypeTrust = parsed.nativeProjectTypeTrusted
        ? ProjectTypeTrust::native
        : ProjectTypeTrust::unknown;
    metadata.documentSha256 = QCryptographicHash::hash(
        parsed.payload, QCryptographicHash::Sha256);
    metadata.levelSha256 = parsed.canonicalLevelSha256;
    parsed.metadata = std::move(metadata);
    return true;
}

void decodeDeferredHistoryWhenProjectKnown(ParsedRecoveryFile& parsed)
{
    // HIST references are relative to the authoritative top-level Level. The
    // parser intentionally retains the opaque payload here and defers
    // semantic decoding until loadRecoveredDocument has loaded that Level;
    // validating against a second reconstructed copy would double memory and
    // could bind history to a different duplicate DOCM candidate.
    Q_UNUSED(parsed);
}

bool parseRecoveryChunks(
    QIODevice& file,
    ParsedRecoveryFile& parsed,
    NativeFallbackBudget& nativeFallbackBudget,
    const WorkspaceHistoryAttachmentLevelValidator attachmentValidator)
{
    if (!file.seek(0)) return false;
    // Full parsing is allowed only after readRecoveryFileBytes has bounded and
    // staged the immutable container. Requiring that QBuffer removes a dormant
    // future-call seam where an arbitrary QIODevice could reach readAll()
    // without the 256 MiB admission and growth probe.
    const auto* buffer = qobject_cast<QBuffer*>(&file);
    if (!buffer) return false;
    const QByteArray& bytes = buffer->buffer();
    if (bytes.isEmpty()) return false;
    const quint64 inputBytes = static_cast<quint64>(bytes.size());

    bool foundDocument = false;
    int metadataChunkCandidates = 0;
    int documentChunkCandidates = 0;
    bool warningChunkSeen = false;
    qsizetype cursor = 0;
    int chunkCount = 0;
    const auto decodeDocument = [&](const QByteArray& payload,
                                    const bool allowPartialRecovery = true,
                                    const std::size_t minimumRecoveredRequiredSections = 0u) -> bool
    {
        // Native candidate probing must be transactional. A damaged/decoy
        // DOCM can pass the outer framing checks and then fail during native
        // parsing; do not let its project type, optional sections, losses,
        // counters, hashes, or warnings contaminate the next candidate. Keep
        // an owned staging record and publish it only after strict or partial
        // semantic validation succeeds.
        ParsedRecoveryFile& destination = parsed;
        ParsedRecoveryFile candidate = destination;
        ParsedRecoveryFile& parsed = candidate;
        try
        {
            // DOCM is already a known, bounded native payload.  Do not reject
            // it merely because the native primary magic was damaged: the
            // strict loader can use its checked mirror, and the corruption-
            // only path can resynchronize from checked section anchors when
            // both fixed-header copies are gone.
            noteNativeDocumentDecodeForTesting();
            assets::SedFile document = assets::loadSedFileBytes(std::as_bytes(std::span(
                payload.constData(), static_cast<std::size_t>(payload.size()))));
            parsed.documentTimestamp = document.timestamp;
            parsed.nativeProjectType = document.level.kind;
            parsed.nativeProjectTypeTrusted = true;
            parsed.repairedNativeChunkCount += document.repairedChunks;
            parsed.corrupted = parsed.corrupted || document.hadCorruption;
            for (const std::string& section : document.repairedSections)
            {
                parsed.warnings.push_back(QStringLiteral("Native SED V1 repaired section %1.")
                    .arg(QString::fromStdString(section)));
            }
            for (const assets::SedOptionalSection& optional : document.optionalSections)
            {
                const QByteArray value(reinterpret_cast<const char*>(optional.payload.data()),
                    static_cast<qsizetype>(optional.payload.size()));
                const QByteArray type(optional.type.data(), 4);
                if (type == QByteArrayLiteral("edit")) parsed.editorStatePayload = value;
                else if (type == QByteArrayLiteral("hist")) parsed.deferredHistoryPayload = value;
            }
            const bool hasNativeHistoryPayload =
                !parsed.deferredHistoryPayload.isEmpty();
            if (hasNativeHistoryPayload)
            {
                QString historyError{};
                QString workspaceWarning{};
                QByteArray validatedEditorState{};
                parsed.history = deserializeEditorDocumentHistoryForRestoreWithWorkspaceBindings(
                    parsed.deferredHistoryPayload,
                    document.level.kind,
                    document.level,
                    parsed.editorStatePayload,
                    &validatedEditorState,
                    &historyError,
                    &workspaceWarning,
                    &parsed.workspaceHistory,
                    attachmentValidator);
                if (!validatedEditorState.isEmpty())
                {
                    parsed.editorStatePayload = std::move(validatedEditorState);
                }
                if (!workspaceWarning.isEmpty())
                {
                    parsed.warnings.push_back(workspaceWarning);
                }
                if (!parsed.history)
                {
                    markMissingSection(parsed, QStringLiteral("hist"));
                    parsed.warnings.push_back(QStringLiteral(
                        "Native SED HIST semantic validation failed; the authoritative Level was retained and history was dropped: %1")
                        .arg(historyError.isEmpty()
                            ? QStringLiteral("unknown schema error")
                            : historyError));
                    parsed.deferredHistoryPayload.clear();
                }
                else
                {
                    // The compact decoded timeline is now authoritative. Do
                    // not retain a second complete encoded HIST buffer for a
                    // project type that is already known.
                    parsed.deferredHistoryPayload.clear();
                }
            }
            if (!hasNativeHistoryPayload
                && !parsed.editorStatePayload.isEmpty())
            {
                QString workspaceWarning{};
                QByteArray withoutAttachments =
                    omitEditorWorkspaceHistoryAttachments(
                        parsed.editorStatePayload, &workspaceWarning);
                if (!withoutAttachments.isEmpty())
                {
                    parsed.editorStatePayload = std::move(withoutAttachments);
                }
                parsed.workspaceHistory = {};
                if (!workspaceWarning.isEmpty()
                    && !parsed.warnings.contains(workspaceWarning))
                {
                    parsed.warnings.push_back(workspaceWarning);
                }
            }
            const QByteArray canonicalLevel = serializeLevelToSed(
                document.level, {}, assets::SedSerializationProfile::UnprotectedNested);
            if (canonicalLevel.isEmpty()) return false;
            parsed.canonicalLevelSha256 = QCryptographicHash::hash(
                canonicalLevel, QCryptographicHash::Sha256);
            // A strictly valid unmodified DOCM is already the canonical bytes
            // published by the writer. If native FEC repaired the input, keep
            // the complete-document digest unavailable rather than retaining
            // another full serialization solely for hashing.
            parsed.canonicalDocumentSha256 = document.hadCorruption
                ? QByteArray{}
                : QCryptographicHash::hash(payload, QCryptographicHash::Sha256);
            parsed.decodedLevel = std::make_shared<Level>(
                std::move(document.level));
            destination = std::move(candidate);
            foundDocument = true;
            return true;
        }
        catch (const std::bad_alloc&)
        {
            throw;
        }
        catch (const std::exception&)
        {
            if (!allowPartialRecovery) return false;
            // Strict decoding may have populated nativeProjectType, optional
            // sections, repair counters, or hashes before a late validation
            // exception. Start partial salvage from the untouched outer
            // baseline rather than leaking that failed candidate's state.
            candidate = destination;
            try
            {
                noteNativeDocumentDecodeForTesting();
                assets::SedRecoveryLoadResult document = assets::recoverFromSedBytes(
                    std::as_bytes(std::span(payload.constData(),
                        static_cast<std::size_t>(payload.size()))));
                parsed.documentTimestamp = document.timestamp;
                parsed.nativeProjectType = document.projectType;
                parsed.nativeProjectTypeTrusted = document.projectTypeTrusted
                    && document.projectType.has_value();
                const std::size_t recoveredRequiredSections = 8u
                    - std::min<std::size_t>(
                        8u, document.missingRequiredSections.size());
                if (recoveredRequiredSections
                    < minimumRecoveredRequiredSections)
                {
                    return false;
                }
                parsed.repairedNativeChunkCount += document.repairedChunks;
                parsed.corrupted = true;
                parsed.partialRecovery = document.partialRecoveryFromCorruption
                    || !document.missingSections.empty()
                    || document.unidentifiedMissingSections != 0;
                if (document.unidentifiedMissingSections != 0)
                {
                    parsed.unidentifiedMissingSectionCount +=
                        document.unidentifiedMissingSections;
                    parsed.lostSectionCount += document.unidentifiedMissingSections;
                    parsed.warnings.push_back(QStringLiteral(
                        "Native SED V1 could not identify %1 damaged semantic section(s); the recovery is detached from the source file.")
                        .arg(document.unidentifiedMissingSections));
                }
                parsed.warnings.push_back(QStringLiteral(
                    "Native SED V1 recovery loaded the healthy sections of DOCM; %1 section(s) were lost.")
                    .arg(document.missingSections.size()));
                for (const std::string& section : document.missingSections)
                {
                    const QString sectionName = QString::fromStdString(section);
                    markMissingSection(parsed, sectionName);
                    parsed.warnings.push_back(QStringLiteral("Native SED V1 lost section %1.")
                        .arg(sectionName));
                }
                for (const assets::SedOptionalSection& optional : document.optionalSections)
                {
                    const QByteArray value(reinterpret_cast<const char*>(optional.payload.data()),
                        static_cast<qsizetype>(optional.payload.size()));
                    const QByteArray type(optional.type.data(), 4);
                    if (type == QByteArrayLiteral("edit")) parsed.editorStatePayload = value;
                    else if (type == QByteArrayLiteral("hist")) parsed.deferredHistoryPayload = value;
                }
                const bool hasRecoveredHistoryPayload =
                    !parsed.deferredHistoryPayload.isEmpty();
                if (hasRecoveredHistoryPayload
                    && document.projectTypeTrusted
                    && document.projectType)
                {
                    QString historyError{};
                    QString workspaceWarning{};
                    QByteArray validatedEditorState{};
                    parsed.history = deserializeEditorDocumentHistoryForRestoreWithWorkspaceBindings(
                        parsed.deferredHistoryPayload,
                        *document.projectType,
                        document.level,
                        parsed.editorStatePayload,
                        &validatedEditorState,
                        &historyError,
                        &workspaceWarning,
                        &parsed.workspaceHistory,
                        attachmentValidator);
                    if (!validatedEditorState.isEmpty())
                    {
                        parsed.editorStatePayload = std::move(validatedEditorState);
                    }
                    if (!workspaceWarning.isEmpty())
                    {
                        parsed.warnings.push_back(workspaceWarning);
                    }
                    if (!parsed.history)
                    {
                        markMissingSection(parsed, QStringLiteral("hist"));
                        parsed.warnings.push_back(QStringLiteral(
                            "Recovered SED HIST semantic validation failed; the authoritative Level was retained and history was dropped: %1")
                            .arg(historyError.isEmpty()
                                ? QStringLiteral("unknown schema error")
                                : historyError));
                        parsed.deferredHistoryPayload.clear();
                    }
                    else
                    {
                        parsed.deferredHistoryPayload.clear();
                    }
                }
                if (!hasRecoveredHistoryPayload
                    && !parsed.editorStatePayload.isEmpty())
                {
                    QString workspaceWarning{};
                    QByteArray withoutAttachments =
                        omitEditorWorkspaceHistoryAttachments(
                            parsed.editorStatePayload, &workspaceWarning);
                    if (!withoutAttachments.isEmpty())
                    {
                        parsed.editorStatePayload = std::move(withoutAttachments);
                    }
                    parsed.workspaceHistory = {};
                    if (!workspaceWarning.isEmpty()
                        && !parsed.warnings.contains(workspaceWarning))
                    {
                        parsed.warnings.push_back(workspaceWarning);
                    }
                }
                if (document.missingRequiredSections.empty())
                {
                    try
                    {
                        const QByteArray canonicalLevel = serializeLevelToSed(
                            document.level, {}, assets::SedSerializationProfile::UnprotectedNested);
                        if (canonicalLevel.isEmpty())
                        {
                            throw std::runtime_error(
                                "Recovered Level canonical serialization is empty.");
                        }
                        parsed.canonicalLevelSha256 = QCryptographicHash::hash(
                            canonicalLevel, QCryptographicHash::Sha256);
                        // Do not synthesize a complete-document digest from a
                        // repaired or partial EDIT/HIST set. The required Level
                        // remains authoritative while full binding is
                        // intentionally unavailable.
                        parsed.canonicalDocumentSha256.clear();
                    }
                    catch (const std::exception&)
                    {
                        parsed.canonicalLevelSha256.clear();
                        parsed.canonicalDocumentSha256.clear();
                    }
                }
                parsed.decodedLevel = std::make_shared<Level>(
                    std::move(document.level));
                destination = std::move(candidate);
                foundDocument = true;
                return true;
            }
            catch (const std::bad_alloc&)
            {
                throw;
            }
            catch (const std::exception&)
            {
                return false;
            }
        }
    };
    // Failed fallback probes only need a read-only view into the staged
    // container. Avoid allocating and copying a potentially hundreds-of-MiB
    // QByteArray for every candidate; retain one owned copy only after native
    // semantic validation accepts the range.
    const auto decodeDocumentRange = [&](const qsizetype offset,
                                         const qsizetype size,
                                         const bool allowPartialRecovery = true,
                                         const std::size_t minimumRecoveredRequiredSections = 1u) -> bool
    {
        if (offset < 0 || size <= 0 || offset > bytes.size()
            || size > bytes.size() - offset)
        {
            return false;
        }
        const QByteArray view = QByteArray::fromRawData(
            bytes.constData() + offset, size);
        if (!decodeDocument(
                view, allowPartialRecovery, minimumRecoveredRequiredSections))
        {
            return false;
        }
        parsed.payload = QByteArray(view.constData(), view.size());
        return true;
    };
    const auto canonicalStrictDocument = [](const QByteArray& payload)
        -> std::optional<std::array<QByteArray, 2>>
    {
        try
        {
            noteNativeDocumentDecodeForTesting();
            const assets::SedFile document = assets::loadSedFileBytes(std::as_bytes(std::span(
                payload.constData(), static_cast<std::size_t>(payload.size()))));
            const QByteArray level = serializeLevelToSed(
                document.level, {}, assets::SedSerializationProfile::UnprotectedNested);
            if (level.isEmpty()) return std::nullopt;
            return std::array<QByteArray, 2>{
                QCryptographicHash::hash(level, QCryptographicHash::Sha256),
                document.hadCorruption
                    ? QByteArray{}
                    : QCryptographicHash::hash(payload, QCryptographicHash::Sha256),
            };
        }
        catch (const std::bad_alloc&)
        {
            throw;
        }
        catch (const std::exception&)
        {
            return std::nullopt;
        }
    };

    while (cursor < bytes.size())
    {
        const qsizetype chunkStart = bytes.indexOf(QByteArray(kChunkMagic, 8), cursor);
        if (chunkStart < 0) break;
        if (++chunkCount > 64)
        {
            parsed.corrupted = true;
            parsed.warnings.push_back(QStringLiteral(
                "Recovery container exceeded the safe chunk-count limit."));
            break;
        }
        if (chunkStart + kChunkHeaderBytes > bytes.size()) break;
        const QByteArray header = bytes.mid(chunkStart, kChunkHeaderBytes);
        QDataStream stream(header);
        stream.setByteOrder(QDataStream::LittleEndian);
        char magic[8]{}, typeChars[4]{}, hashChars[32]{};
        quint32 flags{}, expectedCrc{};
        quint64 storedSize{}, rawSize{};
        stream.readRawData(magic, 8); stream.readRawData(typeChars, 4);
        stream >> flags >> storedSize >> rawSize >> expectedCrc;
        stream.readRawData(hashChars, 32);
        const QByteArray type(typeChars, 4);
        const qsizetype payloadOffset = chunkStart + kChunkHeaderBytes;
        if (stream.status() != QDataStream::Ok
            || (flags & ~(kChunkCompressed | kChunkProtected | kChunkNativeSed)) != 0
            || ((flags & kChunkCompressed) && (flags & kChunkProtected))
            || ((flags & kChunkNativeSed)
                && (flags & (kChunkCompressed | kChunkProtected)))
            || storedSize == 0 || rawSize == 0
            || storedSize > kMaximumChunkBytes || rawSize > kMaximumChunkBytes
            || storedSize > static_cast<quint64>(bytes.size() - payloadOffset))
        {
            appendChunkWarning(parsed, type, QStringLiteral("header or size is invalid"));
            cursor = chunkStart + 1;
            continue;
        }
        cursor = payloadOffset + static_cast<qsizetype>(storedSize);
        if (type != QByteArrayLiteral("meta") && type != QByteArrayLiteral("docm")
            && type != QByteArrayLiteral("warn"))
        {
            appendChunkWarning(parsed, type, QStringLiteral("unsupported recovery chunk type"));
            continue;
        }
        if ((type == QByteArrayLiteral("meta") && flags != kChunkProtected)
            || (type == QByteArrayLiteral("docm") && flags != kChunkNativeSed)
            || (type == QByteArrayLiteral("warn")
                && (flags & ~kChunkCompressed) != 0))
        {
            appendChunkWarning(parsed, type, QStringLiteral("non-canonical chunk flags"));
            continue;
        }
        if (!recoveryChunkSizesWithinAdmissionLimits(type, storedSize, rawSize))
        {
            appendChunkWarning(parsed, type, QStringLiteral(
                "type-specific stored/raw size limit is invalid"));
            continue;
        }
        // A canonical writer emits exactly one META, DOCM, and WARN chunk.
        // Keep one redundant semantic candidate for META/DOCM corruption
        // diagnosis, but never let a hostile container multiply parity/native
        // decoding or retain dozens of MiB-sized warning strings.
        if (type == QByteArrayLiteral("meta")
            && ++metadataChunkCandidates > 2)
        {
            appendChunkWarning(parsed, type, QStringLiteral(
                "excess duplicate metadata chunk"));
            continue;
        }
        if (type == QByteArrayLiteral("docm")
            && ++documentChunkCandidates > 2)
        {
            appendChunkWarning(parsed, type, QStringLiteral(
                "excess duplicate native document chunk"));
            continue;
        }
        if (type == QByteArrayLiteral("warn"))
        {
            if (warningChunkSeen)
            {
                appendChunkWarning(parsed, type, QStringLiteral(
                    "excess duplicate warning chunk"));
                continue;
            }
            warningChunkSeen = true;
        }

        const bool nativeDocument = (flags & kChunkNativeSed) != 0;
        // Canonical DOCM can approach the complete 256 MiB file ceiling and
        // already owns native checksums/FEC. Keep a read-only view into the
        // staged container until native semantic validation accepts it; META
        // and WARN retain their ordinary owned/copying paths.
        const QByteArray stored = nativeDocument
            ? QByteArray::fromRawData(bytes.constData() + payloadOffset,
                static_cast<qsizetype>(storedSize))
            : bytes.mid(payloadOffset, static_cast<qsizetype>(storedSize));
        QByteArray raw{};
        std::optional<RecoveryMetadata> admittedMetadata{};
        if ((flags & kChunkProtected) != 0)
        {
            QString protectedError{};
            const auto admission = type == QByteArrayLiteral("meta")
                ? admitProtectedMetadata(
                    stored,
                    std::optional<quint64>(rawSize),
                    QByteArray(hashChars, 32),
                    &protectedError)
                : std::optional<ProtectedMetadataAdmission>{};
            if (!admission)
            {
                appendChunkWarning(parsed, type, protectedError.isEmpty()
                    ? QStringLiteral("only META may use outer parity protection") : protectedError);
                continue;
            }
            raw = admission->decoded.level;
            admittedMetadata = admission->metadata;
            parsed.repairedMetadataShardCount += admission->decoded.repairedShardCount;
            parsed.corrupted = parsed.corrupted || admission->decoded.corrupted;
            parsed.warnings.append(admission->decoded.warnings);
        }
        else
        {
            if (!nativeDocument && recoveryCrc32(stored) != expectedCrc)
            {
                parsed.corrupted = true;
                parsed.warnings.push_back(QStringLiteral(
                    "Recovery chunk %1 had outer CRC damage; native payload checks decide salvage.")
                    .arg(QString::fromLatin1(type)));
                if (type != QByteArrayLiteral("docm")) continue;
            }
            if ((flags & kChunkCompressed) != 0)
            {
                if (!compressedSizePrefixMatches(stored, rawSize))
                {
                    appendChunkWarning(parsed, type, QStringLiteral("compressed size prefix is invalid"));
                    continue;
                }
                raw = qUncompress(stored);
            }
            else raw = stored;
        }
        if (raw.size() != static_cast<qsizetype>(rawSize)
            || (!nativeDocument
                && QCryptographicHash::hash(raw, QCryptographicHash::Sha256)
                    != QByteArray(hashChars, 32))
            || (nativeDocument
                && (expectedCrc != 0
                    || QByteArray(hashChars, 32) != QByteArray(32, '\0'))))
        {
            // A protected META may have a damaged outer CRC while its inner
            // descriptors repair it; native DOCM is checked below.
            if ((flags & kChunkProtected) == 0 && type != QByteArrayLiteral("docm"))
            {
                appendChunkWarning(parsed, type, QStringLiteral("size or SHA-256 check failed"));
                continue;
            }
            parsed.corrupted = true;
        }
        if ((flags & kChunkProtected) != 0 && recoveryCrc32(stored) != expectedCrc)
        {
            parsed.corrupted = true;
            parsed.warnings.push_back(QStringLiteral(
                "Recovery META had outer CRC damage; checked parity data was used."));
        }

        if (type == QByteArrayLiteral("meta"))
        {
            if (admittedMetadata)
            {
                parsed.metadataChunkDecoded = true;
                if (parsed.metadata.identity.isEmpty()) parsed.metadata = *admittedMetadata;
                else if (!recoveryMetadataEquivalent(parsed.metadata, *admittedMetadata))
                {
                    parsed.metadataCopiesConflict = true;
                    appendChunkWarning(parsed, type, QStringLiteral("metadata copies disagree"));
                }
            }
            else appendChunkWarning(parsed, type, QStringLiteral("metadata is invalid"));
        }
        else if (type == QByteArrayLiteral("docm"))
        {
            if (foundDocument)
            {
                if (raw == parsed.payload)
                {
                    parsed.corrupted = true;
                    parsed.warnings.push_back(QStringLiteral(
                        "Duplicate valid DOCM matched the accepted bytes; the duplicate was ignored without another native decode."));
                    continue;
                }
                if (!nativeFallbackBudget.tryConsume(storedSize))
                {
                    appendChunkWarning(parsed, type, QStringLiteral(
                        "duplicate native document exceeded the shared validation-work budget"));
                    continue;
                }
                const auto duplicate = canonicalStrictDocument(raw);
                if (!duplicate)
                {
                    appendChunkWarning(parsed, type, QStringLiteral(
                        "duplicate DOCM is not a valid native SED document"));
                }
                else if (!parsed.canonicalDocumentSha256.isEmpty()
                    && duplicate->at(1) == parsed.canonicalDocumentSha256)
                {
                    parsed.corrupted = true;
                    parsed.warnings.push_back(QStringLiteral(
                        "Duplicate valid DOCM matched the canonical document; the duplicate was ignored."));
                }
                else if (!parsed.canonicalLevelSha256.isEmpty()
                    && duplicate->at(0) == parsed.canonicalLevelSha256
                    && parsed.canonicalDocumentSha256.isEmpty())
                {
                    // A damaged DOCM encountered first may have yielded a
                    // checked Level-only salvage. A later strict DOCM with the
                    // same authoritative Level is the stronger complete
                    // candidate and must supersede that partial envelope so
                    // EDIT/HIST are not silently lost. Preserve the earlier
                    // repair diagnostics and mark the container corrupted.
                    ParsedRecoveryFile previous = parsed;
                    parsed.editorStatePayload.clear();
                    parsed.deferredHistoryPayload.clear();
                    parsed.history.reset();
                    parsed.missingSections.clear();
                    parsed.lostSectionCount = 0;
                    parsed.unidentifiedMissingSectionCount = 0;
                    parsed.partialRecovery = false;
                    parsed.canonicalLevelSha256.clear();
                    parsed.canonicalDocumentSha256.clear();
                    if (!nativeFallbackBudget.tryConsume(storedSize))
                    {
                        parsed = std::move(previous);
                        parsed.documentCopiesConflict = true;
                        parsed.corrupted = true;
                        parsed.warnings.push_back(QStringLiteral(
                            "A later matching DOCM could not be installed within the shared validation-work budget; recovery was rejected safely."));
                    }
                    else if (decodeDocumentRange(payloadOffset,
                            static_cast<qsizetype>(storedSize), false))
                    {
                        parsed.corrupted = true;
                        parsed.warnings.push_back(QStringLiteral(
                            "A later valid DOCM matched the recovered Level and superseded the earlier partial DOCM."));
                    }
                    else
                    {
                        parsed = std::move(previous);
                        parsed.documentCopiesConflict = true;
                        parsed.corrupted = true;
                        parsed.warnings.push_back(QStringLiteral(
                            "A later DOCM matched the recovered Level but failed strict replacement validation."));
                    }
                }
                else
                {
                    parsed.documentCopiesConflict = true;
                    parsed.corrupted = true;
                    parsed.warnings.push_back(QStringLiteral(
                        "Conflicting valid DOCM payloads were found; recovery was rejected safely."));
                }
            }
            else if (!decodeDocumentRange(payloadOffset,
                         static_cast<qsizetype>(storedSize)))
            {
                appendChunkWarning(parsed, type, QStringLiteral("native SED V1 payload is invalid"));
            }
        }
        else
        {
            const QString warning = QString::fromUtf8(raw).trimmed();
            if (!warning.isEmpty())
            {
                parsed.warnings.push_back(warning);
            }
        }
    }

    // A damaged outer magic does not erase the rest of the fixed DOCM
    // envelope. Recover that exact, size-bounded native payload before trying
    // signature/anchor resynchronization. Keeping the native payload prefix is
    // important when its fixed header is also damaged: the native recovery
    // scanner then sees the checked top-level section stream in its original
    // framing instead of mistaking raw HistoryNested HIST sections for a
    // second document.
    if (!foundDocument)
    {
        qsizetype typeSearch = 0;
        int damagedHeaderCandidateCount = 0;
        bool damagedHeaderFallbackBudgetExhausted = false;
        while (!foundDocument
            && damagedHeaderCandidateCount < kMaximumDamagedDocumentHeaderCandidates)
        {
            const qsizetype typeOffset = bytes.indexOf(
                QByteArrayLiteral("docm"), typeSearch);
            if (typeOffset < 0) break;
            typeSearch = typeOffset + 4;
            if (typeOffset < 8) continue;
            const qsizetype headerOffset = typeOffset - 8;
            if (headerOffset + kChunkHeaderBytes > bytes.size()) continue;
            ++damagedHeaderCandidateCount;

            QDataStream stream(bytes.mid(headerOffset, kChunkHeaderBytes));
            stream.setByteOrder(QDataStream::LittleEndian);
            char damagedMagic[8]{}, typeChars[4]{}, hashChars[32]{};
            quint32 flags{}, expectedCrc{};
            quint64 storedSize{}, rawSize{};
            stream.readRawData(damagedMagic, 8);
            stream.readRawData(typeChars, 4);
            stream >> flags >> storedSize >> rawSize >> expectedCrc;
            stream.readRawData(hashChars, 32);
            const qsizetype payloadOffset = headerOffset + kChunkHeaderBytes;
            if (stream.status() != QDataStream::Ok
                || QByteArray(typeChars, 4) != QByteArrayLiteral("docm")
                || flags != kChunkNativeSed
                || storedSize == 0 || storedSize != rawSize
                || storedSize > kMaximumChunkBytes
                || expectedCrc != 0
                || QByteArray(hashChars, 32) != QByteArray(32, '\0')
                || storedSize > static_cast<quint64>(bytes.size() - payloadOffset))
            {
                continue;
            }
            if (!nativeFallbackBudget.tryConsume(storedSize))
            {
                damagedHeaderFallbackBudgetExhausted = true;
                continue;
            }
            if (decodeDocumentRange(
                    payloadOffset, static_cast<qsizetype>(storedSize), true))
            {
                parsed.corrupted = true;
                parsed.warnings.push_back(QStringLiteral(
                    "The outer DOCM magic was damaged; its canonical bounded envelope and native semantic checks recovered the Level."));
            }
        }
        if (damagedHeaderCandidateCount
                == kMaximumDamagedDocumentHeaderCandidates
            && bytes.indexOf(QByteArrayLiteral("docm"), typeSearch) >= 0)
        {
            damagedHeaderFallbackBudgetExhausted = true;
        }

        const auto containerSpan = std::as_bytes(std::span(
            bytes.constData(), static_cast<std::size_t>(bytes.size())));
        if (!foundDocument)
        {
            // The native embedded-document search can validate candidates
            // across the complete container. Charge that full scan once, then
            // charge the selected range again before installing it.
            if (!nativeFallbackBudget.tryConsume(inputBytes))
            {
                damagedHeaderFallbackBudgetExhausted = true;
            }
            else if (const auto embedded = assets::findEmbeddedSedDocument(containerSpan))
            {
                if (!nativeFallbackBudget.tryConsume(embedded->size))
                {
                    damagedHeaderFallbackBudgetExhausted = true;
                }
                else if (decodeDocumentRange(
                        static_cast<qsizetype>(embedded->offset),
                        static_cast<qsizetype>(embedded->size), false))
                {
                    parsed.corrupted = true;
                    parsed.warnings.push_back(QStringLiteral(
                        "The outer DOCM header was damaged; native SED V1 framing and semantic validation recovered it."));
                }
            }
        }

        const qsizetype sedStart = foundDocument
            ? -1 : bytes.indexOf(QByteArray("sed\0", 4));
        std::vector<qsizetype> validatedOuterBoundaries{};
        bool outerBoundaryScanBudgetExhausted = false;
        if (!foundDocument)
        {
            validatedOuterBoundaries.reserve(kMaximumOuterBoundaryCandidates);
            qsizetype markerSearch = sedStart >= 0 ? sedStart + 4 : 0;
            int markerProbes{};
            while (markerProbes < kMaximumOuterBoundaryMarkerProbes
                && validatedOuterBoundaries.size()
                    < static_cast<std::size_t>(kMaximumOuterBoundaryCandidates))
            {
                const qsizetype probe = bytes.indexOf(
                    QByteArray(kChunkMagic, 8), markerSearch);
                if (probe < 0) break;
                markerSearch = probe + 1;
                ++markerProbes;
                if (probe > bytes.size() - kChunkHeaderBytes) continue;

                const QByteArray header = QByteArray::fromRawData(
                    bytes.constData() + probe, kChunkHeaderBytes);
                QDataStream stream(header);
                stream.setByteOrder(QDataStream::LittleEndian);
                char magic[8]{}, typeChars[4]{}, hashChars[32]{};
                quint32 flags{}, expectedCrc{};
                quint64 storedSize{}, rawSize{};
                stream.readRawData(magic, 8);
                stream.readRawData(typeChars, 4);
                stream >> flags >> storedSize >> rawSize >> expectedCrc;
                stream.readRawData(hashChars, 32);
                const QByteArray type(typeChars, 4);
                const qsizetype payloadOffset = probe + kChunkHeaderBytes;
                const bool knownType = type == QByteArrayLiteral("meta")
                    || type == QByteArrayLiteral("docm")
                    || type == QByteArrayLiteral("warn");
                const bool flagsValid = (flags
                    & ~(kChunkCompressed | kChunkProtected | kChunkNativeSed)) == 0
                    && !((flags & kChunkCompressed) && (flags & kChunkProtected))
                    && !((flags & kChunkNativeSed)
                        && (flags & (kChunkCompressed | kChunkProtected)));
                if (stream.status() == QDataStream::Ok
                    && QByteArray(magic, 8) == QByteArray(kChunkMagic, 8)
                    && knownType && flagsValid && storedSize > 0 && rawSize > 0
                    && storedSize <= kMaximumChunkBytes
                    && rawSize <= kMaximumChunkBytes
                    && recoveryChunkSizesWithinAdmissionLimits(
                        type, storedSize, rawSize)
                    && storedSize <= static_cast<quint64>(
                        bytes.size() - payloadOffset))
                {
                    validatedOuterBoundaries.push_back(probe);
                }
            }
            if ((markerProbes == kMaximumOuterBoundaryMarkerProbes
                    || validatedOuterBoundaries.size()
                        == static_cast<std::size_t>(kMaximumOuterBoundaryCandidates))
                && bytes.indexOf(QByteArray(kChunkMagic, 8), markerSearch) >= 0)
            {
                outerBoundaryScanBudgetExhausted = true;
            }
        }
        // findEmbeddedSedDocument handled complete candidates. The bounded
        // signature-to-outer-boundary fallback exists only for corruption-
        // aware partial native loading when no strictly framed DOCM remains.
        bool signatureFallbackWorkBudgetExhausted = false;
        if (!foundDocument && sedStart >= 0)
        {
            int candidateCount{};
            const auto tryCandidate = [&](const qsizetype end)
            {
                if (end <= sedStart) return false;
                const quint64 candidateBytes = static_cast<quint64>(end - sedStart);
                if (candidateCount >= kMaximumSedSignatureFallbackCandidates
                    || !nativeFallbackBudget.tryConsume(candidateBytes))
                {
                    signatureFallbackWorkBudgetExhausted = true;
                    return false;
                }
                ++candidateCount;
                return decodeDocumentRange(
                    sedStart, static_cast<qsizetype>(candidateBytes), true);
            };

            if (!tryCandidate(bytes.size()))
            {
                for (auto end = validatedOuterBoundaries.rbegin();
                     end != validatedOuterBoundaries.rend() && !foundDocument
                        && !signatureFallbackWorkBudgetExhausted;
                     ++end)
                {
                    if (*end > sedStart)
                    {
                        (void)tryCandidate(*end);
                    }
                }
            }
        }

        // If the outer DOCM header and the native primary header are both
        // damaged, no `sed\0` signature remains for the preceding fallback.
        // Checked sectv1 descriptors are still safe resynchronization points;
        // ask the native corruption-only loader to validate each bounded
        // candidate. The anchors are visited in file order, so the first
        // accepted suffix retains the greatest possible checked prefix. We
        // never accept an arbitrary byte prefix or a candidate with fewer
        // than two required sections, which prevents a random payload
        // fragment from becoming a false document. Each candidate is decoded
        // only once under the one shared native-fallback byte budget.
        bool sectionAnchorFallbackBudgetExhausted = false;
        if (!foundDocument)
        {
            const QByteArray sectionAnchor("sectv1\n\0", 8);
            const QByteArray nativeFooter("sedend\r\n", 8);
            // Discover native footer positions once with a progressive scan.
            // Candidate probes below perform only logarithmic lookups; they must
            // never rescan a near-cap container backwards for every anchor.
            std::vector<qsizetype> nativeFooterOffsets{};
            nativeFooterOffsets.reserve(kMaximumOuterBoundaryMarkerProbes);
            qsizetype footerSearch{};
            while (nativeFooterOffsets.size()
                < static_cast<std::size_t>(kMaximumOuterBoundaryMarkerProbes))
            {
                const qsizetype footer = bytes.indexOf(nativeFooter, footerSearch);
                if (footer < 0) break;
                nativeFooterOffsets.push_back(footer);
                footerSearch = footer + 1;
            }
            if (nativeFooterOffsets.size()
                    == static_cast<std::size_t>(kMaximumOuterBoundaryMarkerProbes)
                && bytes.indexOf(nativeFooter, footerSearch) >= 0)
            {
                sectionAnchorFallbackBudgetExhausted = true;
            }
            int candidateCount{};
            qsizetype search = 0;
            while (!foundDocument
                && candidateCount < kMaximumSectionAnchorCandidates)
            {
                const qsizetype candidate = bytes.indexOf(sectionAnchor, search);
                if (candidate < 0) break;
                search = candidate + sectionAnchor.size();
                ++candidateCount;
                // Only a fully bounded, canonical outer chunk header is a
                // valid stopping point.  A raw sedchn1 byte sequence inside a
                // section payload is not allowed to truncate the candidate.
                const auto boundary = std::lower_bound(
                    validatedOuterBoundaries.begin(),
                    validatedOuterBoundaries.end(), search);
                qsizetype candidateEnd = boundary == validatedOuterBoundaries.end()
                    ? bytes.size() : *boundary;
                // If an outer trailing chunk is absent, stop at the last
                // native footer before EOF instead of retaining unrelated
                // warning/transport bytes in the DOCM candidate.  Native
                // recovery still validates every checked section; a missing
                // footer intentionally leaves the EOF bound unchanged.
                auto footer = std::lower_bound(
                    nativeFooterOffsets.begin(), nativeFooterOffsets.end(), candidateEnd);
                if (footer != nativeFooterOffsets.begin())
                {
                    --footer;
                    if (*footer > candidate
                        && *footer + nativeFooter.size() <= candidateEnd)
                    {
                        candidateEnd = *footer + nativeFooter.size();
                    }
                }
                if (candidateEnd <= candidate
                    || candidateEnd - candidate > static_cast<qsizetype>(kMaximumChunkBytes))
                {
                    continue;
                }
                const quint64 candidateBytes = static_cast<quint64>(
                    candidateEnd - candidate);
                if (!nativeFallbackBudget.tryConsume(candidateBytes))
                {
                    sectionAnchorFallbackBudgetExhausted = true;
                    continue;
                }
                if (decodeDocumentRange(
                        candidate,
                        candidateEnd - candidate,
                        true,
                        2u))
                {
                    parsed.corrupted = true;
                    parsed.warnings.push_back(QStringLiteral(
                        "The outer DOCM header and native primary header were damaged; checked SED section anchors recovered the Level."));
                }
            }
            if (candidateCount == kMaximumSectionAnchorCandidates
                && bytes.indexOf(sectionAnchor, search) >= 0)
            {
                sectionAnchorFallbackBudgetExhausted = true;
            }
        }
    if (damagedHeaderFallbackBudgetExhausted
            || outerBoundaryScanBudgetExhausted
            || signatureFallbackWorkBudgetExhausted
            || sectionAnchorFallbackBudgetExhausted
            || nativeFallbackBudget.exhausted)
        {
            parsed.corrupted = true;
            parsed.warnings.push_back(QStringLiteral(
                "Recovery native-envelope fallback reached its bounded marker/candidate work budget; excess candidates were ignored."));
        }
    }

    // When both checked native header copies are lost, sed_core synthesizes a
    // zero creation timestamp because section data cannot establish it. The
    // protected recovery capture time is the creation instant of DOCM and can
    // restore that hint without storing a duplicate metadata field.
    if (parsed.canonicalDocumentSha256.isEmpty()
        && parsed.documentTimestamp == 0
        && parsed.metadata.timestamp > 0)
    {
        parsed.documentTimestamp = parsed.metadata.timestamp;
    }

    verifyRecoveryDocumentBinding(parsed);
    decodeDeferredHistoryWhenProjectKnown(parsed);
    if (parsed.metadataCopiesConflict || parsed.documentCopiesConflict
        || parsed.documentBindingMismatch || !foundDocument) return false;
    if (parsed.editorStatePayload.isEmpty())
    {
        markMissingSection(parsed, QStringLiteral("edit"));
        parsed.warnings.push_back(QStringLiteral("DOCM EDIT optional section was lost."));
    }
    // Absence of an optional HIST section is not itself evidence of physical
    // loss: writers may intentionally omit it. Native corruption recovery and
    // semantic HIST decoding already mark a named HIST loss when one is
    // actually observed; never synthesize one from unrelated partial state.
    return true;
}

std::optional<ParsedRecoveryFile> recoverFromRedundantChunks(
    QIODevice& file,
    const qint64 fallbackModifiedAtMs,
    NativeFallbackBudget& nativeFallbackBudget,
    QString* error,
    const WorkspaceHistoryAttachmentLevelValidator attachmentValidator)
{
    ParsedRecoveryFile recovered{};
    bool synthesizedMetadata = false;
    const auto synthesizeMetadataFromLevel = [&](const QByteArray&)
    {
        synthesizedMetadata = synthesizeMetadataFromDecodedDocument(
            recovered, fallbackModifiedAtMs, error);
        return synthesizedMetadata;
    };
    std::optional<DecodedProtectedPayload> independentlyDecodedMetadata{};
    if (file.seek(0))
    {
        independentlyDecodedMetadata = recoverMetadataWithoutChunkHeader(file);
        if (independentlyDecodedMetadata)
        {
            int version{};
            const auto metadata = metadataFromJson(
                independentlyDecodedMetadata->level, &version, nullptr);
            if (metadata && version == 1)
            {
                // Seed the project type before HIST decoding. If META's outer
                // header is damaged, history must not be decoded using the
                // default JKDF2 project type.
                recovered.metadata = *metadata;
            }
        }
    }
    const bool chunksParsed = file.seek(0)
        && parseRecoveryChunks(
            file, recovered, nativeFallbackBudget, attachmentValidator);
    if (recovered.metadataCopiesConflict || recovered.documentCopiesConflict
        || recovered.documentBindingMismatch)
    {
        if (error)
        {
            *error = recovered.documentCopiesConflict
                ? QStringLiteral("Conflicting valid DOCM payloads were found; recovery was rejected safely.")
                : QStringLiteral("Recovery metadata does not bind to the decoded native DOCM; source association was rejected safely.");
        }
        return std::nullopt;
    }
    if (!chunksParsed)
    {
        if (error)
        {
            *error = QStringLiteral(
                "Recovery metadata is damaged and no complete redundant metadata/level pair remains.");
        }
        return std::nullopt;
    }
    if (recovered.metadata.identity.isEmpty())
    {
        const auto decodedMetadata = recoverMetadataWithoutChunkHeader(file);
        int version{};
        const auto metadata = decodedMetadata
            ? metadataFromJson(decodedMetadata->level, &version, nullptr)
            : std::nullopt;
        if (!metadata || version != 1)
        {
            if (!synthesizeMetadataFromLevel(recovered.payload))
            {
                if (error && error->isEmpty())
                {
                    *error = QStringLiteral(
                        "Recovery metadata is damaged and no complete redundant metadata/level pair remains.");
                }
                return std::nullopt;
            }
        }
        else
        {
            recovered.metadata = *metadata;
            recovered.corrupted = true;
            recovered.repairedMetadataShardCount += decodedMetadata->repairedShardCount;
            recovered.warnings.append(decodedMetadata->warnings);
            recovered.warnings.push_back(QStringLiteral(
                "The outer metadata chunk header was damaged; SED recovered its checked protected payload directly."));
        }
    }
    else if (independentlyDecodedMetadata && !recovered.metadataChunkDecoded)
    {
        recovered.corrupted = true;
        recovered.repairedMetadataShardCount += independentlyDecodedMetadata->repairedShardCount;
        recovered.warnings.append(independentlyDecodedMetadata->warnings);
        recovered.warnings.push_back(QStringLiteral(
            "The outer metadata chunk header was damaged; SED recovered its checked protected payload directly."));
    }
    decodeDeferredHistoryWhenProjectKnown(recovered);
    if (synthesizedMetadata)
    {
        recovered.corrupted = true;
        if (recovered.metadata.projectTypeKnown())
        {
            recovered.warnings.push_back(QStringLiteral(
                "All recovery metadata copies were unreadable. SED salvaged the checked Level as an Untitled session; source path and capture metadata were unavailable."));
        }
        else
        {
            recovered.warnings.push_back(QStringLiteral(
                "All recovery metadata and native project headers were unreadable. SED salvaged the checked Level data, but the project type is unknown; choose JKDF2, MotS, or IJIM before loading it."));
        }
    }
    else
    {
        recovered.warnings.push_back(QStringLiteral(
            "The primary recovery header was damaged; SED reconstructed it from the redundant metadata chunk."));
    }
    recovered.corrupted = true;
    return recovered;
}

std::optional<ParsedRecoveryFile> parseRecoveryFile(
    QIODevice& file,
    const qint64 fallbackModifiedAtMs,
    const bool readPayload,
    QString* error,
    const WorkspaceHistoryAttachmentLevelValidator attachmentValidator = nullptr)
{
    if (file.size() <= 0
        || static_cast<quint64>(file.size()) > kMaximumContainerBytes)
    {
        if (error) *error = QStringLiteral("Recovery container size is invalid.");
        return std::nullopt;
    }
    NativeFallbackBudget nativeFallbackBudget = makeNativeFallbackBudget(
        static_cast<quint64>(file.size()));

    const RecoveryMetadataScan metadataScan = scanRecoveryMetadataCopies(file);
    if (metadataScan.fatal)
    {
        if (error) *error = metadataScan.error;
        return std::nullopt;
    }
    ParsedRecoveryFile parsed{};
    if (metadataScan.metadata)
    {
        parsed.metadata = *metadataScan.metadata;
    }
    parsed.corrupted = !metadataScan.primaryValid
        || metadataScan.redundantRecoveredWithoutHeader;
    if (metadataScan.redundantRecoveredWithoutHeader)
    {
        parsed.warnings.push_back(QStringLiteral(
            "The outer metadata chunk header was damaged; SED recovered its checked protected payload directly."));
    }

    if (!readPayload)
    {
        if (parsed.metadata.identity.isEmpty())
        {
            if (error)
            {
                *error = QStringLiteral(
                    "No bounded valid primary or protected recovery metadata copy was found.");
            }
            return std::nullopt;
        }
        return parsed;
    }

    const bool chunksParsed = parseRecoveryChunks(
        file, parsed, nativeFallbackBudget, attachmentValidator);
    if (parsed.metadataCopiesConflict || parsed.documentCopiesConflict
        || parsed.documentBindingMismatch)
    {
        if (error)
        {
            *error = parsed.documentCopiesConflict
                ? QStringLiteral("Conflicting valid DOCM payloads were found; recovery was rejected safely.")
                : QStringLiteral("Recovery metadata does not bind to the decoded native DOCM; source association was rejected safely.");
        }
        return std::nullopt;
    }
    if (chunksParsed && parsed.metadata.identity.isEmpty())
    {
        // DOCM has already passed native semantic validation. Reuse its
        // retained payload, canonical Level digest, and captured project-kind
        // result instead of discarding `parsed`, decoding DOCM again in the
        // redundant path, and decoding it a third time merely to infer orphan
        // metadata.
        if (!synthesizeMetadataFromDecodedDocument(
                parsed, fallbackModifiedAtMs, error))
        {
            return std::nullopt;
        }
        decodeDeferredHistoryWhenProjectKnown(parsed);
        parsed.corrupted = true;
        if (parsed.metadata.projectTypeKnown())
        {
            parsed.warnings.push_back(QStringLiteral(
                "All recovery metadata copies were unreadable. SED salvaged the checked Level as an Untitled session; source path and capture metadata were unavailable."));
        }
        else
        {
            parsed.warnings.push_back(QStringLiteral(
                "All recovery metadata and native project headers were unreadable. SED salvaged the checked Level data, but the project type is unknown; choose JKDF2, MotS, or IJIM before loading it."));
        }
        return parsed;
    }
    if (!chunksParsed)
    {
        // The ordinary envelope may be the damaged part. Re-scan the checked
        // META payload independently and, as a last resort, synthesize orphan
        // metadata from a self-validating native DOCM so intact user work is
        // not hidden merely because discovery metadata was lost.
        return recoverFromRedundantChunks(
            file, fallbackModifiedAtMs, nativeFallbackBudget, error,
            attachmentValidator);
    }
    return parsed;
}

QString pathToQString(const std::filesystem::path& path)
{
    return QString::fromStdWString(path.wstring());
}

struct ParsedWorkspaceHistory
{
    bool present{};
    QCborMap root{};
    QCborMap history{};
    std::vector<WorkspaceHistoryPosition> positions{};
    std::vector<WorkspaceHistoryBindingDigest> expectations{};
};

std::optional<ParsedWorkspaceHistory> parseWorkspaceHistoryForBinding(
    const QByteArray& editorStatePayload,
    const std::size_t undoCount,
    const std::size_t redoCount,
    QString* error)
{
    if (error)
    {
        error->clear();
    }
    constexpr RecoveryCborLimits limits{
        assets::SedMaximumSemanticSectionBytes,
        2'000'000,
        64,
        3ull * 64ull * 1024ull * 1024ull,
        assets::SedMaximumSemanticSectionBytes,
        assets::SedMaximumDecodedObjectBytes,
    };
    QString parseError{};
    const std::optional<QCborValue> parsed = deserializeCanonicalCbor(
        editorStatePayload, limits, &parseError);
    if (!parsed || !parsed->isMap())
    {
        if (error)
        {
            *error = QStringLiteral(
                "The EDIT workspace attachment envelope is invalid: %1")
                .arg(parseError);
        }
        return std::nullopt;
    }

    ParsedWorkspaceHistory result{};
    result.root = parsed->toMap();
    const QCborValue historyValue = result.root.value(
        QStringLiteral("workspaceHistory"));
    if (historyValue.isUndefined())
    {
        return result;
    }
    if (!historyValue.isMap())
    {
        if (error)
        {
            *error = QStringLiteral("The EDIT workspaceHistory value is not a map.");
        }
        return std::nullopt;
    }
    result.present = true;
    result.history = historyValue.toMap();
    const QCborValue version = result.history.value(QStringLiteral("version"));
    const QCborValue encodedUndoCount = result.history.value(
        QStringLiteral("undoCount"));
    const QCborValue encodedRedoCount = result.history.value(
        QStringLiteral("redoCount"));
    const QCborValue entriesValue = result.history.value(QStringLiteral("entries"));
    if (!version.isInteger() || version.toInteger() != 1
        || !encodedUndoCount.isInteger()
        || encodedUndoCount.toInteger(-1) != static_cast<qint64>(undoCount)
        || !encodedRedoCount.isInteger()
        || encodedRedoCount.toInteger(-1) != static_cast<qint64>(redoCount)
        || !entriesValue.isArray())
    {
        if (error)
        {
            *error = QStringLiteral(
                "The EDIT workspaceHistory version or exact HIST counts are invalid.");
        }
        return std::nullopt;
    }
    const QCborArray entries = entriesValue.toArray();
    if (entries.isEmpty() || entries.size() > 64)
    {
        if (error)
        {
            *error = QStringLiteral(
                "The EDIT workspaceHistory attachment count exceeds its safety limit.");
        }
        return std::nullopt;
    }
    result.positions.reserve(static_cast<std::size_t>(entries.size()));
    result.expectations.reserve(static_cast<std::size_t>(entries.size()));
    for (const QCborValue& entryValue : entries)
    {
        if (!entryValue.isMap())
        {
            if (error) *error = QStringLiteral(
                "An EDIT workspaceHistory attachment is malformed.");
            return std::nullopt;
        }
        const QCborMap entry = entryValue.toMap();
        const QCborValue stackValue = entry.value(QStringLiteral("stack"));
        const QCborValue indexValue = entry.value(QStringLiteral("index"));
        const QCborValue beforeDigest = entry.value(
            QStringLiteral("beforeSha256"));
        const QCborValue afterDigest = entry.value(
            QStringLiteral("afterSha256"));
        const QCborValue cleaveValue = entry.value(QStringLiteral("cleave"));
        const QCborValue layerValue = entry.value(QStringLiteral("layerVisuals"));
        if (!stackValue.isInteger() || !indexValue.isInteger()
            || !beforeDigest.isByteArray() || !afterDigest.isByteArray()
            || beforeDigest.toByteArray().size() != 32
            || afterDigest.toByteArray().size() != 32
            || (cleaveValue.isUndefined() && layerValue.isUndefined())
            || (!cleaveValue.isUndefined() && !cleaveValue.isMap())
            || (!layerValue.isUndefined() && !layerValue.isMap()))
        {
            if (error) *error = QStringLiteral(
                "An EDIT workspaceHistory attachment has invalid fields.");
            return std::nullopt;
        }
        if (cleaveValue.isMap())
        {
            const QCborMap cleave = cleaveValue.toMap();
            const QCborValue workspace = cleave.value(QStringLiteral("workspace"));
            const QCborValue selection = cleave.value(
                QStringLiteral("documentSelection"));
            const QCborValue active = cleave.value(QStringLiteral("active"));
            const QCborValue mode = cleave.value(QStringLiteral("mode"));
            if (!workspace.isMap() || !selection.isByteArray()
                || !active.isArray() || active.toArray().size() != 7
                || !mode.isArray() || mode.toArray().size() != 3)
            {
                if (error) *error = QStringLiteral(
                    "An EDIT workspaceHistory Cleave attachment is incomplete.");
                return std::nullopt;
            }
            for (const QCborValue& value : active.toArray())
            {
                if (!value.isInteger()
                    || value.toInteger() < std::numeric_limits<std::int32_t>::min()
                    || value.toInteger() > std::numeric_limits<std::int32_t>::max())
                {
                    if (error) *error = QStringLiteral(
                        "An EDIT workspaceHistory active item is malformed.");
                    return std::nullopt;
                }
            }
            for (const QCborValue& value : mode.toArray())
            {
                if (!value.isInteger())
                {
                    if (error) *error = QStringLiteral(
                        "An EDIT workspaceHistory editor mode is malformed.");
                    return std::nullopt;
                }
            }
        }
        if (layerValue.isMap())
        {
            const QCborMap layerVisuals = layerValue.toMap();
            if (!layerVisuals.value(QStringLiteral("before")).isMap()
                || !layerVisuals.value(QStringLiteral("after")).isMap())
            {
                if (error) *error = QStringLiteral(
                    "An EDIT workspaceHistory layer attachment is incomplete.");
                return std::nullopt;
            }
        }
        const qint64 stack = stackValue.toInteger(-1);
        const qint64 index = indexValue.toInteger(-1);
        const std::size_t stackCount = stack == 0
            ? undoCount : (stack == 1 ? redoCount : 0u);
        if (stack < 0 || stack > 1 || index < 0
            || static_cast<std::uint64_t>(index) >= stackCount)
        {
            if (error) *error = QStringLiteral(
                "An EDIT workspaceHistory attachment targets an invalid HIST position.");
            return std::nullopt;
        }
        const WorkspaceHistoryPosition position{
            stack == 0 ? editor::DocumentRecoveryHistoryStack::Undo
                       : editor::DocumentRecoveryHistoryStack::Redo,
            static_cast<std::size_t>(index),
        };
        if (std::any_of(result.positions.begin(), result.positions.end(),
                [&position](const WorkspaceHistoryPosition& previous)
                {
                    return previous.stack == position.stack
                        && previous.index == position.index;
                }))
        {
            if (error) *error = QStringLiteral(
                "EDIT workspaceHistory contains a duplicate HIST position.");
            return std::nullopt;
        }
        result.positions.push_back(position);
        WorkspaceHistoryBindingDigest expectation{
            position,
            beforeDigest.toByteArray(),
            afterDigest.toByteArray(),
        };
        if (cleaveValue.isMap())
        {
            expectation.cleaveAttachment = cleaveValue;
        }
        if (layerValue.isMap())
        {
            expectation.layerVisualsAttachment = layerValue;
        }
        result.expectations.push_back(std::move(expectation));
    }

    const QCborValue pendingValue = result.history.value(
        QStringLiteral("pendingRedo"));
    if (!pendingValue.isUndefined())
    {
        if (!pendingValue.isMap())
        {
            if (error) *error = QStringLiteral(
                "The EDIT workspaceHistory pendingRedo value is malformed.");
            return std::nullopt;
        }
        const QCborMap pending = pendingValue.toMap();
        const QCborValue stack = pending.value(QStringLiteral("stack"));
        const QCborValue index = pending.value(QStringLiteral("index"));
        if (!stack.isInteger() || stack.toInteger(-1) != 1
            || !index.isInteger() || index.toInteger(-1) < 0
            || static_cast<std::uint64_t>(index.toInteger()) >= redoCount)
        {
            if (error) *error = QStringLiteral(
                "The EDIT workspaceHistory pendingRedo position is invalid.");
            return std::nullopt;
        }
        const std::size_t pendingIndex = static_cast<std::size_t>(
            index.toInteger());
        const bool hasCleaveAttachment = std::any_of(
            entries.begin(), entries.end(),
            [pendingIndex](const QCborValue& value)
            {
                if (!value.isMap()) return false;
                const QCborMap entry = value.toMap();
                return entry.value(QStringLiteral("stack")).toInteger(-1) == 1
                    && entry.value(QStringLiteral("index")).toInteger(-1)
                        == static_cast<qint64>(pendingIndex)
                    && entry.value(QStringLiteral("cleave")).isMap();
            });
        if (!hasCleaveAttachment)
        {
            if (error) *error = QStringLiteral(
                "The EDIT workspaceHistory pendingRedo has no matching Cleave attachment.");
            return std::nullopt;
        }
    }
    return result;
}

std::optional<std::array<std::size_t, 2>> checkedRecoveryHistoryStackCounts(
    const QByteArray& payload,
    QString* error)
{
    if (error) error->clear();
    if (!preflightRecoveryHistoryCbor(payload, error))
    {
        return std::nullopt;
    }
    QCborParserError parseError{};
    const QCborValue rootValue = QCborValue::fromCbor(payload, &parseError);
    if (parseError.error != QCborError::NoError || !rootValue.isMap()
        || rootValue.toCbor(QCborValue::NoTransformation) != payload)
    {
        if (error) *error = QStringLiteral(
            "HIST does not contain a canonical single CBOR map.");
        return std::nullopt;
    }
    const QCborMap root = rootValue.toMap();
    const QCborValue undoValue = root.value(QStringLiteral("undoGroups"));
    const QCborValue redoValue = root.value(QStringLiteral("redoGroups"));
    if (!undoValue.isArray() || !redoValue.isArray())
    {
        if (error) *error = QStringLiteral(
            "HIST undo/redo group arrays are missing.");
        return std::nullopt;
    }
    const auto countEntries = [](const QCborArray& groups,
                                 std::size_t* count) noexcept
    {
        if (!count)
        {
            return false;
        }
        std::size_t total{};
        for (qsizetype groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
        {
            const QCborValue groupValue = groups.at(groupIndex);
            if (!groupValue.isArray())
            {
                return false;
            }
            const qsizetype groupSize = groupValue.toArray().size();
            if (groupSize <= 0 || groupSize > kHistoryEntriesPerGroup
                || static_cast<std::size_t>(groupSize)
                    > MaximumPersistedHistoryEntries - total)
            {
                return false;
            }
            total += static_cast<std::size_t>(groupSize);
        }
        *count = total;
        return true;
    };
    std::array<std::size_t, 2> counts{};
    if (!countEntries(undoValue.toArray(), &counts[0])
        || !countEntries(redoValue.toArray(), &counts[1]))
    {
        if (error) *error = QStringLiteral(
            "HIST stack counts exceed the persisted history limit.");
        return std::nullopt;
    }
    if (counts[1] > MaximumPersistedHistoryEntries - counts[0])
    {
        if (error) *error = QStringLiteral(
            "HIST combined undo/redo count exceeds the persisted history limit.");
        return std::nullopt;
    }
    return counts;
}

QByteArray omitWorkspaceHistoryFromPayload(
    const QByteArray& editorStatePayload,
    QString* error)
{
    ParsedWorkspaceHistory parsed{};
    constexpr RecoveryCborLimits limits{
        assets::SedMaximumSemanticSectionBytes,
        2'000'000,
        64,
        3ull * 64ull * 1024ull * 1024ull,
        assets::SedMaximumSemanticSectionBytes,
        assets::SedMaximumDecodedObjectBytes,
    };
    QString parseError{};
    const std::optional<QCborValue> value = deserializeCanonicalCbor(
        editorStatePayload, limits, &parseError);
    if (!value || !value->isMap())
    {
        if (error) *error = parseError.isEmpty()
            ? QStringLiteral("The EDIT payload is not a CBOR map.")
            : parseError;
        return {};
    }
    parsed.root = value->toMap();
    if (!parsed.root.contains(QStringLiteral("workspaceHistory")))
    {
        if (error) error->clear();
        return editorStatePayload;
    }
    parsed.root.remove(QStringLiteral("workspaceHistory"));
    if (error) error->clear();
    return serializeCanonicalCbor(QCborValue(parsed.root));
}

} // namespace

QByteArray authoredLevelStateHash(const Level& level)
{
    try
    {
        Level authoredLevel = level;
        authoredLevel.lvisstring.clear();
        const QByteArray canonicalAuthoredLevel = serializeLevelToSed(
            authoredLevel, {}, assets::SedSerializationProfile::UnprotectedNested);
        return canonicalLevelDigest(canonicalAuthoredLevel, authoredLevel);
    }
    catch (...)
    {
        return {};
    }
}

QByteArray serializeCanonicalCbor(const QCborValue& value)
{
    QByteArray encoded{};
    QCborStreamWriter writer(&encoded);
    writeCanonicalRecoveryCbor(writer, value);
    return encoded;
}

std::optional<QCborValue> deserializeCanonicalCbor(
    const QByteArray& payload,
    const RecoveryCborLimits& limits,
    QString* error)
{
    if (error) error->clear();
    if (!preflightRecoveryCbor(payload, limits))
    {
        if (error)
        {
            *error = QStringLiteral(
                "bounded canonical CBOR preflight failed (input, node/depth/string/materialization limits, text map-key order, or single-value framing)");
        }
        return std::nullopt;
    }

    QCborParserError parseError{};
    const QCborValue value = QCborValue::fromCbor(payload, &parseError);
    if (parseError.error != QCborError::NoError)
    {
        if (error) *error = QStringLiteral("bounded CBOR value is malformed");
        return std::nullopt;
    }
    if (value.toCbor(QCborValue::NoTransformation) != payload)
    {
        if (error) *error = QStringLiteral("CBOR value is not deterministic canonical encoding");
        return std::nullopt;
    }
    return value;
}

QString newSessionId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QByteArray serializeEditorDocumentHistory(
    const editor::Document& document)
{
    return serializeRecoveryHistory(document);
}

QByteArray serializeEditorDocumentHistoryForSave(
    const editor::Document& document)
{
    return serializeRecoveryHistory(document, true, false, nullptr);
}

namespace
{

QByteArray patchWorkspaceHistoryBindingDigests(
    const ParsedWorkspaceHistory& parsed,
    const std::vector<WorkspaceHistoryBindingDigest>& digests,
    QString* error)
{
    if (error) error->clear();
    if (!parsed.present)
    {
        if (error) *error = QStringLiteral(
            "EDIT has no workspaceHistory attachment map to bind.");
        return {};
    }
    const QCborArray entries = parsed.history.value(
        QStringLiteral("entries")).toArray();
    if (entries.size() != static_cast<qsizetype>(digests.size()))
    {
        if (error) *error = QStringLiteral(
            "EDIT workspaceHistory entries do not match the requested HIST bindings.");
        return {};
    }
    QCborArray patchedEntries{};
    for (const QCborValue& entryValue : entries)
    {
        if (!entryValue.isMap())
        {
            if (error) *error = QStringLiteral(
                "An EDIT workspaceHistory attachment is malformed.");
            return {};
        }
        QCborMap entry = entryValue.toMap();
        const qint64 stack = entry.value(QStringLiteral("stack")).toInteger(-1);
        const qint64 index = entry.value(QStringLiteral("index")).toInteger(-1);
        const auto digest = std::find_if(
            digests.begin(), digests.end(), [stack, index](
                const WorkspaceHistoryBindingDigest& value)
            {
                const qint64 expectedStack =
                    value.position.stack
                        == editor::DocumentRecoveryHistoryStack::Undo ? 0 : 1;
                return stack == expectedStack
                    && index == static_cast<qint64>(value.position.index);
            });
        if (digest == digests.end())
        {
            if (error) *error = QStringLiteral(
                "EDIT workspaceHistory refers to an unencoded HIST transition.");
            return {};
        }
        entry.insert(QStringLiteral("beforeSha256"), digest->beforeSha256);
        entry.insert(QStringLiteral("afterSha256"), digest->afterSha256);
        patchedEntries.append(std::move(entry));
    }
    QCborMap patchedHistory = parsed.history;
    patchedHistory.insert(QStringLiteral("entries"), patchedEntries);
    QCborMap root = parsed.root;
    root.insert(QStringLiteral("workspaceHistory"), patchedHistory);
    return serializeCanonicalCbor(QCborValue(root));
}

QByteArray serializeEditorHistoryWithWorkspaceBindings(
    const editor::Document& document,
    const QByteArray& editorStatePayload,
    QByteArray* boundEditorStatePayload,
    const bool normalizeHistoricalThingFrameCounts,
    QString* warning)
{
    if (warning) warning->clear();
    if (!boundEditorStatePayload)
    {
        throw std::invalid_argument(
            "Workspace-bound HIST serialization requires an EDIT output.");
    }
    boundEditorStatePayload->clear();
    QString parseError{};
    const std::optional<ParsedWorkspaceHistory> parsed =
        parseWorkspaceHistoryForBinding(
            editorStatePayload, document.undoDepth(), document.redoDepth(),
            &parseError);
    if (!parsed)
    {
        QString omitError{};
        *boundEditorStatePayload = omitWorkspaceHistoryFromPayload(
            editorStatePayload, &omitError);
        if (boundEditorStatePayload->isEmpty())
        {
            throw std::runtime_error(
                (omitError.isEmpty() ? parseError : omitError).toStdString());
        }
        if (warning)
        {
            *warning = QStringLiteral(
                "Historical Cleave/Layer workspace attachments were dropped: %1")
                    .arg(parseError);
        }
        return serializeRecoveryHistory(document,
            normalizeHistoricalThingFrameCounts,
            !normalizeHistoricalThingFrameCounts, nullptr);
    }
    if (!parsed->present)
    {
        *boundEditorStatePayload = editorStatePayload;
        return serializeRecoveryHistory(document,
            normalizeHistoricalThingFrameCounts,
            !normalizeHistoricalThingFrameCounts, nullptr);
    }

    std::vector<WorkspaceHistoryBindingDigest> digests{};
    const QByteArray history = serializeRecoveryHistory(
        document,
        normalizeHistoricalThingFrameCounts,
        !normalizeHistoricalThingFrameCounts,
        nullptr,
        &parsed->positions,
        &digests);
    QString bindingError{};
    QByteArray patched = patchWorkspaceHistoryBindingDigests(
        *parsed, digests, &bindingError);
    if (patched.isEmpty())
    {
        QString omitError{};
        patched = omitWorkspaceHistoryFromPayload(editorStatePayload, &omitError);
        if (patched.isEmpty())
        {
            throw std::runtime_error(bindingError.toStdString());
        }
        if (warning)
        {
            *warning = QStringLiteral(
                "Historical Cleave/Layer workspace attachments were dropped: %1")
                    .arg(bindingError);
        }
    }
    *boundEditorStatePayload = std::move(patched);
    return history;
}

} // namespace

QByteArray serializeEditorDocumentHistoryWithWorkspaceBindings(
    const editor::Document& document,
    const QByteArray& editorStatePayload,
    QByteArray* const boundEditorStatePayload,
    QString* const warning)
{
    return serializeEditorHistoryWithWorkspaceBindings(
        document, editorStatePayload, boundEditorStatePayload, false, warning);
}

QByteArray serializeEditorDocumentHistoryForSaveWithWorkspaceBindings(
    const editor::Document& document,
    const QByteArray& editorStatePayload,
    QByteArray* const boundEditorStatePayload,
    QString* const warning)
{
    return serializeEditorHistoryWithWorkspaceBindings(
        document, editorStatePayload, boundEditorStatePayload, true, warning);
}

QByteArray omitEditorWorkspaceHistoryAttachments(
    const QByteArray& editorStatePayload,
    QString* const warning)
{
    QString error{};
    QByteArray result = omitWorkspaceHistoryFromPayload(
        editorStatePayload, &error);
    if (warning)
    {
        *warning = error.isEmpty()
            ? QStringLiteral(
                "Historical Cleave/Layer workspace attachments were omitted because HIST is unavailable.")
            : error;
    }
    return result;
}

QByteArray detail::serializeEditorDocumentHistoryForSaveWithStats(
    const editor::Document& document,
    RecoveryHistoryEncodeStats* const stats)
{
    return serializeRecoveryHistory(document, true, false, stats);
}

QByteArray detail::serializeEditorDocumentHistoryWithStatsForTesting(
    const editor::Document& document,
    RecoveryHistoryEncodeStats* const stats)
{
    return serializeRecoveryHistory(document, false, true, stats);
}

std::vector<QByteArray> detail::generatedHistoryFragmentsForTesting(
    const Level& level)
{
    const assets::SedGeneratedUnprotectedDocument generated =
        assets::generateUnprotectedSedDocument(level);
    return historyContentFragments(
        generated.bytes, HistoryFragmentLayout::SectorRecordsV1);
}

QByteArray serializeEditorDocumentHistory(
    const Level& currentLevel,
    const editor::DocumentRecoveryHistory& history)
{
    return serializeRecoveryHistory(currentLevel, history);
}

std::optional<editor::DocumentRecoveryHistory> deserializeEditorDocumentHistory(
    const QByteArray& payload,
    const ProjectType projectType,
    const Level& currentLevel,
    QString* error)
{
    // This public inspection helper intentionally returns the portable
    // expanded DTO. Native .sed/.sedrecovery readers use the compact path.
    return deserializeRecoveryHistory(
        payload, projectType, &currentLevel, error, false);
}

std::optional<editor::DocumentRecoveryHistory>
deserializeEditorDocumentHistoryForRestore(
    const QByteArray& payload,
    const ProjectType projectType,
    const Level& currentLevel,
    QString* error)
{
    return deserializeRecoveryHistory(
        payload, projectType, &currentLevel, error, true);
}

std::optional<editor::DocumentRecoveryHistory>
deserializeEditorDocumentHistoryForRestoreWithWorkspaceBindings(
    const QByteArray& historyPayload,
    const ProjectType projectType,
    const Level& currentLevel,
    const QByteArray& editorStatePayload,
    QByteArray* const validatedEditorStatePayload,
    QString* const historyError,
    QString* const workspaceWarning,
    WorkspaceHistoryState* const workspaceHistory,
    const WorkspaceHistoryAttachmentLevelValidator attachmentValidator)
{
    if (historyError) historyError->clear();
    if (workspaceWarning) workspaceWarning->clear();
    if (workspaceHistory) *workspaceHistory = {};
    if (!validatedEditorStatePayload)
    {
        if (historyError) *historyError = QStringLiteral(
            "Workspace-bound HIST restore requires an EDIT output buffer.");
        return std::nullopt;
    }
    *validatedEditorStatePayload = editorStatePayload;

    constexpr RecoveryCborLimits editorLimits{
        assets::SedMaximumSemanticSectionBytes,
        2'000'000,
        64,
        3ull * 64ull * 1024ull * 1024ull,
        assets::SedMaximumSemanticSectionBytes,
        assets::SedMaximumDecodedObjectBytes,
    };
    QString editorParseError{};
    const std::optional<QCborValue> editorValue = deserializeCanonicalCbor(
        editorStatePayload, editorLimits, &editorParseError);
    const bool hasWorkspaceHistory = editorValue && editorValue->isMap()
        && editorValue->toMap().contains(QStringLiteral("workspaceHistory"));
    if (!hasWorkspaceHistory)
    {
        return deserializeRecoveryHistory(
            historyPayload, projectType, &currentLevel, historyError, true);
    }

    const auto dropWorkspaceHistory = [
        &editorStatePayload, validatedEditorStatePayload, workspaceWarning](
            const QString& reason)
    {
        QString omitError{};
        QByteArray sanitized = omitWorkspaceHistoryFromPayload(
            editorStatePayload, &omitError);
        if (!sanitized.isEmpty())
        {
            *validatedEditorStatePayload = std::move(sanitized);
        }
        if (workspaceWarning)
        {
            *workspaceWarning = QStringLiteral(
                "Historical Cleave/Layer workspace attachments were dropped: %1")
                    .arg(reason.isEmpty() ? omitError : reason);
        }
    };

    QString countError{};
    const std::optional<std::array<std::size_t, 2>> counts =
        checkedRecoveryHistoryStackCounts(historyPayload, &countError);
    if (!counts)
    {
        dropWorkspaceHistory(countError);
        return deserializeRecoveryHistory(
            historyPayload, projectType, &currentLevel, historyError, true);
    }

    QString attachmentError{};
    const std::optional<ParsedWorkspaceHistory> parsedAttachments =
        parseWorkspaceHistoryForBinding(
            editorStatePayload, (*counts)[0], (*counts)[1], &attachmentError);
    if (!parsedAttachments || !parsedAttachments->present)
    {
        dropWorkspaceHistory(attachmentError.isEmpty()
            ? QStringLiteral("the attachment map is malformed")
            : attachmentError);
        return deserializeRecoveryHistory(
            historyPayload, projectType, &currentLevel, historyError, true);
    }

    bool bindingsValid = false;
    std::optional<editor::DocumentRecoveryHistory> history =
        deserializeRecoveryHistory(
            historyPayload,
            projectType,
            &currentLevel,
            historyError,
            true,
            &parsedAttachments->expectations,
            &bindingsValid,
            attachmentValidator);
    if (!history)
    {
        dropWorkspaceHistory(QStringLiteral(
            "HIST could not be checked against the attached transitions"));
        return std::nullopt;
    }
    if (!bindingsValid)
    {
        dropWorkspaceHistory(!attachmentValidator
            ? QStringLiteral(
                "the application-level historical-reference validator was unavailable")
            : QStringLiteral(
                "the HIST positions, adjacent Level hashes, or historical references did not match"));
        return history;
    }

    if (workspaceHistory)
    {
        workspaceHistory->attachments.reserve(
            parsedAttachments->expectations.size());
        for (const WorkspaceHistoryBindingDigest& expectation
             : parsedAttachments->expectations)
        {
            WorkspaceHistoryAttachment attachment{};
            attachment.stack = expectation.position.stack;
            attachment.index = expectation.position.index;
            attachment.cleave = expectation.cleaveAttachment;
            attachment.layerVisuals = expectation.layerVisualsAttachment;
            workspaceHistory->attachments.push_back(std::move(attachment));
        }
        const QCborValue pendingValue = parsedAttachments->history.value(
            QStringLiteral("pendingRedo"));
        if (pendingValue.isMap())
        {
            const qint64 pendingIndex = pendingValue.toMap().value(
                QStringLiteral("index")).toInteger(-1);
            if (pendingIndex >= 0)
            {
                workspaceHistory->pendingRedoIndex =
                    static_cast<std::size_t>(pendingIndex);
            }
        }
    }
    return history;
}

QString normalizedSourcePath(const std::filesystem::path& sourcePath)
{
    if (sourcePath.empty())
    {
        return {};
    }

    QString normalized = QDir::cleanPath(QFileInfo(pathToQString(sourcePath)).absoluteFilePath());
#ifdef Q_OS_WIN
    normalized = normalized.toCaseFolded();
#endif
    return normalized;
}

bool recoveryPathsEqual(
    const std::filesystem::path& lhs,
    const std::filesystem::path& rhs)
{
    if (lhs.empty() || rhs.empty()) return lhs.empty() && rhs.empty();
    QString left = QDir::cleanPath(QFileInfo(pathToQString(lhs)).absoluteFilePath());
    QString right = QDir::cleanPath(QFileInfo(pathToQString(rhs)).absoluteFilePath());
#ifdef Q_OS_WIN
    left = left.toCaseFolded();
    right = right.toCaseFolded();
#endif
    return left == right;
}

bool isAutosaveHistoryPath(const std::filesystem::path& path) noexcept
{
    try
    {
        return QFileInfo(pathToQString(path)).fileName().endsWith(
            QStringLiteral(".sedautosave"), Qt::CaseInsensitive);
    }
    catch (...)
    {
        // A path that cannot be classified is never admitted to a destructive
        // history operation. The low-level delete boundary treats it as a
        // live/unknown path and therefore remains conservative.
        return false;
    }
}

bool isLiveRecoveryPath(const std::filesystem::path& path) noexcept
{
    try
    {
        return QFileInfo(pathToQString(path)).fileName().endsWith(
            QStringLiteral(".sedrecovery"), Qt::CaseInsensitive);
    }
    catch (...)
    {
        return false;
    }
}

QString documentIdentity(const std::filesystem::path& sourcePath, const QString& sessionId)
{
    const QString normalized = normalizedSourcePath(sourcePath);
    if (!normalized.isEmpty())
    {
        return QStringLiteral("file:%1").arg(normalized);
    }

    const QString safeSessionId = sessionId.trimmed().isEmpty() ? newSessionId() : sessionId.trimmed();
    return QStringLiteral("untitled:%1").arg(safeSessionId);
}

QString recoveryProjectTag(const ProjectType projectType)
{
    switch (projectType)
    {
    case ProjectType::JKDF2:
        return QStringLiteral("jkdf2");
    case ProjectType::MOTS:
        return QStringLiteral("mots");
    case ProjectType::IJIM:
        return QStringLiteral("ijim");
    default:
        return QStringLiteral("unknown");
    }
}

// Encode bytes as lowercase base32. Lowercase alphabet symbols are deliberate:
// Windows filenames are case-insensitive, so a case-sensitive alphabet would
// silently make distinct filename tokens equivalent.
QByteArray recoveryBase32Encode(const QByteArray& bytes)
{
    static constexpr char kBase32Alphabet[] =
        "abcdefghijklmnopqrstuvwxyz234567";
    QByteArray token{};
    token.reserve((bytes.size() * 8 + 4) / 5);
    std::uint32_t buffer{};
    int bits{};
    for (const unsigned char byte : bytes)
    {
        buffer = (buffer << 8u) | byte;
        bits += 8;
        while (bits >= 5)
        {
            bits -= 5;
            token.append(kBase32Alphabet[(buffer >> bits) & 0x1fu]);
        }
    }
    if (bits > 0)
    {
        token.append(kBase32Alphabet[(buffer << (5 - bits)) & 0x1fu]);
    }
    return token;
}

// A filename is only a discovery hint, so use one 128-bit family token rather
// than two long independent digests. Length-prefixing the UTF-8 fields keeps
// the identity/session pair unambiguous before hashing. The full strings stay
// authenticated in META and are always checked after a filename match.
QByteArray recoveryFamilyToken(
    const QString& identity,
    const QString& sessionId)
{
    const QByteArray identityBytes = identity.trimmed().toUtf8();
    const QByteArray sessionBytes = sessionId.trimmed().toUtf8();
    QByteArray material = QByteArrayLiteral("sed-recovery-family-v1");
    const auto appendField = [&material](const QByteArray& field)
    {
        std::uint64_t length = static_cast<std::uint64_t>(field.size());
        for (int byte = 0; byte < 8; ++byte)
        {
            material.append(static_cast<char>(length & 0xffu));
            length >>= 8u;
        }
        material.append(field);
    };
    appendField(identityBytes);
    appendField(sessionBytes);
    // 128 bits encode to 26 lowercase Base32 characters (the final four
    // padding bits are deterministic zeroes).
    return recoveryBase32Encode(
        QCryptographicHash::hash(material, QCryptographicHash::Sha256)
            .left(16));
}

QString sanitizeRecoveryBasename(QString displayName)
{
    QString basename = QFileInfo(displayName.trimmed()).completeBaseName().trimmed();
    if (basename.isEmpty())
    {
        basename = QStringLiteral("untitled");
    }

    QString sanitized{};
    sanitized.reserve(std::min<qsizetype>(basename.size(), 48));
    for (const QChar character : basename)
    {
        const ushort code = character.unicode();
        const bool asciiLetter = (code >= 'a' && code <= 'z')
            || (code >= 'A' && code <= 'Z');
        const bool asciiDigit = code >= '0' && code <= '9';
        if (asciiLetter || asciiDigit || character == QLatin1Char('-')
            || character == QLatin1Char('_'))
        {
            sanitized += character.toLower();
        }
        else if (sanitized.isEmpty() || sanitized.back() != QLatin1Char('_'))
        {
            sanitized += QLatin1Char('_');
        }
        if (sanitized.size() >= 48)
        {
            break;
        }
    }
    while (sanitized.endsWith(QLatin1Char('.'))
        || sanitized.endsWith(QLatin1Char(' ')))
    {
        sanitized.chop(1);
    }
    if (sanitized.isEmpty() || sanitized == QStringLiteral(".")
        || sanitized == QStringLiteral(".."))
    {
        sanitized = QStringLiteral("untitled");
    }

    const QString folded = sanitized.toCaseFolded();
    static const std::array<QString, 22> reservedNames{
        QStringLiteral("con"), QStringLiteral("prn"), QStringLiteral("aux"),
        QStringLiteral("nul"), QStringLiteral("com1"), QStringLiteral("com2"),
        QStringLiteral("com3"), QStringLiteral("com4"), QStringLiteral("com5"),
        QStringLiteral("com6"), QStringLiteral("com7"), QStringLiteral("com8"),
        QStringLiteral("com9"), QStringLiteral("lpt1"), QStringLiteral("lpt2"),
        QStringLiteral("lpt3"), QStringLiteral("lpt4"), QStringLiteral("lpt5"),
        QStringLiteral("lpt6"), QStringLiteral("lpt7"), QStringLiteral("lpt8"),
        QStringLiteral("lpt9") };
    if (std::any_of(reservedNames.begin(), reservedNames.end(),
            [&folded](const QString& reserved) { return folded == reserved; }))
    {
        sanitized.prepend(QLatin1Char('_'));
    }
    return sanitized;
}

std::filesystem::path recoveryFilePath(
    const std::filesystem::path& recoveryDirectory,
    const QString& identity,
    const QString& sessionId,
    const QString& displayName,
    const ProjectType projectType)
{
    const QString effectiveSessionId = sessionId.trimmed().isEmpty()
        ? newSessionId()
        : sessionId.trimmed();
    const QByteArray familyToken = recoveryFamilyToken(
        identity.trimmed(), effectiveSessionId);
    const QString filename = QStringLiteral("%1-%2-recovery-%3.sedrecovery")
        .arg(sanitizeRecoveryBasename(displayName), recoveryProjectTag(projectType),
            QString::fromLatin1(familyToken));
    return recoveryDirectory / filename.toStdString();
}

std::filesystem::path recoveryFilePath(
    const std::filesystem::path& recoveryDirectory,
    const std::filesystem::path& sourcePath,
    const QString& sessionId,
    const QString& displayName,
    const ProjectType projectType)
{
    return recoveryFilePath(
        recoveryDirectory, documentIdentity(sourcePath, sessionId),
        sessionId, displayName, projectType);
}

qint64 sourceModifiedAtMs(const std::filesystem::path& sourcePath)
{
    if (sourcePath.empty())
    {
        return -1;
    }
    const QFileInfo info(pathToQString(sourcePath));
    return info.exists() ? info.lastModified().toMSecsSinceEpoch() : -1;
}

std::optional<RecoveryContainerBytes> readRecoveryContainerResilient(
    const std::uint64_t fileSize,
    const RecoveryRangeReader& reader,
    const std::uint32_t blockBytes,
    const std::uint32_t maximumAttempts,
    QString* error)
{
    if (error) error->clear();
    if (!reader || fileSize == 0 || fileSize > kMaximumContainerBytes
        || fileSize > static_cast<std::uint64_t>(
            std::numeric_limits<qsizetype>::max())
        || blockBytes < 512 || blockBytes > 1024u * 1024u
        || maximumAttempts == 0 || maximumAttempts > 16)
    {
        if (error)
        {
            *error = QStringLiteral("Recovery resilient-read size or retry options are invalid.");
        }
        return std::nullopt;
    }

    RecoveryContainerBytes result{};
    result.bytes = QByteArray(static_cast<qsizetype>(fileSize), '\0');
    for (std::uint64_t offset = 0; offset < fileSize;)
    {
        const std::uint64_t length = std::min<std::uint64_t>(blockBytes, fileSize - offset);
        auto destination = std::span<std::byte>(
            reinterpret_cast<std::byte*>(result.bytes.data())
                + static_cast<std::ptrdiff_t>(offset),
            static_cast<std::size_t>(length));
        bool complete = false;
        for (std::uint32_t attempt = 0; attempt < maximumAttempts; ++attempt)
        {
            if (reader(offset, destination))
            {
                complete = true;
                break;
            }
        }
        if (!complete)
        {
            std::fill(destination.begin(), destination.end(), std::byte{});
            if (!result.unreadableRanges.empty()
                && result.unreadableRanges.back().offset
                    + result.unreadableRanges.back().size == offset)
            {
                result.unreadableRanges.back().size += length;
            }
            else
            {
                result.unreadableRanges.push_back(RecoveryUnreadableRange{ offset, length });
            }
        }
        offset += length;
    }
    return result;
}

namespace
{

// The destination is already admitted and allocated to its exact bounded
// size. Read it in larger contiguous runs to reduce QFile dispatch/syscall
// overhead without increasing peak memory.
constexpr qint64 kRecoverySequentialReadChunkBytes = 1024 * 1024;

enum class BoundedRecoveryReadStatus
{
    success,
    shortRead,
    deviceError,
    grew,
};

struct BoundedRecoveryRead final
{
    BoundedRecoveryReadStatus status{BoundedRecoveryReadStatus::deviceError};
    QByteArray bytes{};
};

using RecoverySequentialReader = std::function<qint64(char*, qint64)>;

BoundedRecoveryRead readBoundedRecoveryBytes(
    const qint64 expectedSize,
    const RecoverySequentialReader& reader)
{
    BoundedRecoveryRead result{};
    if (!reader || expectedSize <= 0
        || static_cast<quint64>(expectedSize) > kMaximumContainerBytes
        || expectedSize > std::numeric_limits<qsizetype>::max())
    {
        return result;
    }

    result.bytes.resize(static_cast<qsizetype>(expectedSize));
    qint64 filled{};
    while (filled < expectedSize)
    {
        const qint64 request = std::min(
            kRecoverySequentialReadChunkBytes, expectedSize - filled);
        const qint64 count = reader(result.bytes.data() + filled, request);
        if (count < 0 || count > request)
        {
            result.status = BoundedRecoveryReadStatus::deviceError;
            result.bytes = QByteArray{};
            return result;
        }
        if (count == 0)
        {
            result.status = BoundedRecoveryReadStatus::shortRead;
            result.bytes = QByteArray{};
            return result;
        }
        filled += count;
    }

    // Probe exactly one byte beyond the admitted initial size. This detects a
    // concurrently appended file without allowing readAll() to materialize an
    // attacker-controlled tail or a second over-limit allocation.
    char extra{};
    const qint64 extraCount = reader(&extra, 1);
    if (extraCount < 0 || extraCount > 1)
    {
        result.status = BoundedRecoveryReadStatus::deviceError;
        result.bytes = QByteArray{};
        return result;
    }
    if (extraCount != 0)
    {
        result.status = BoundedRecoveryReadStatus::grew;
        result.bytes = QByteArray{};
        return result;
    }
    result.status = BoundedRecoveryReadStatus::success;
    return result;
}

QString boundedRecoveryReadError(const BoundedRecoveryReadStatus status)
{
    switch (status)
    {
    case BoundedRecoveryReadStatus::shortRead:
        return QStringLiteral(
            "Recovery container ended before its admitted size was read.");
    case BoundedRecoveryReadStatus::grew:
        return QStringLiteral(
            "Recovery container grew while it was being read.");
    case BoundedRecoveryReadStatus::deviceError:
        return QStringLiteral(
            "Recovery container read failed before its admitted size was read.");
    case BoundedRecoveryReadStatus::success:
        break;
    }
    return {};
}

} // namespace

namespace detail
{

std::size_t maximumRecoveryDirectoryEntriesScannedForTesting() noexcept
{
    return kMaximumRecoveryDirectoryEntriesScanned;
}

std::array<quint64, 4> exerciseNativeFallbackBudgetForTesting(
    const quint64 inputBytes,
    const std::span<const quint64> requests)
{
    NativeFallbackBudget budget = makeNativeFallbackBudget(inputBytes);
    for (const quint64 request : requests)
    {
        (void)budget.tryConsume(request);
    }
    return {
        budget.chargedBytes,
        budget.rejectedBytes,
        budget.remainingBytes,
        budget.exhausted ? 1ull : 0ull,
    };
}

bool decodeRecoveryHistoryWithRetainedBudgetForTesting(
    const QByteArray& payload,
    const ProjectType projectType,
    const Level& currentLevel,
    const quint64 maximumRetainedLevelBytes,
    const quint64 maximumRetainedLevelItems,
    QString* error)
{
    editor::DocumentRecoveryHistory history{};
    return decodeRecoveryHistory(
        payload,
        projectType,
        &currentLevel,
        &history,
        nullptr,
        error,
        maximumRetainedLevelBytes,
        maximumRetainedLevelItems,
        kMaximumHistoryReconstructionBytesAbsolute,
        false);
}

bool decodeRecoveryHistoryWithReconstructionBudgetForTesting(
    const QByteArray& payload,
    const ProjectType projectType,
    const Level& currentLevel,
    const quint64 maximumReconstructionBytes,
    QString* error)
{
    return decodeRecoveryHistory(
        payload,
        projectType,
        &currentLevel,
        nullptr,
        nullptr,
        error,
        assets::SedMaximumDecodedObjectBytes,
        assets::SedMaximumDecodedItems,
        maximumReconstructionBytes,
        false);
}

bool validateGeneratedRecoveryHistoryWithRetainedBudgetForTesting(
    const QByteArray& payload,
    const ProjectType projectType,
    const Level& currentLevel,
    const std::size_t expectedUndoEntries,
    const std::size_t expectedRedoEntries,
    const quint64 maximumRetainedLevelBytes,
    const quint64 maximumRetainedLevelItems,
    QString* error)
{
    if (error) error->clear();
    try
    {
        (void)validateGeneratedRecoveryHistory(
            payload,
            projectType,
            currentLevel,
            expectedUndoEntries,
            expectedRedoEntries,
            maximumRetainedLevelBytes,
            maximumRetainedLevelItems);
        return true;
    }
    catch (const std::exception& exception)
    {
        if (error) *error = QString::fromUtf8(exception.what());
        return false;
    }
}

// Link-only deterministic seam for the race/read-limit tests. Production and
// tests use the same bounded allocator and expected-size-plus-one-byte logic;
// it is intentionally absent from the public recovery header.
std::optional<QByteArray> readRecoveryBytesBoundedForTesting(
    const qint64 expectedSize,
    const std::function<qint64(char*, qint64)>& reader,
    QString* error)
{
    if (error) error->clear();
    if (!reader || expectedSize <= 0
        || static_cast<quint64>(expectedSize) > kMaximumContainerBytes
        || expectedSize > std::numeric_limits<qsizetype>::max())
    {
        if (error) *error = QStringLiteral("Recovery container size is invalid.");
        return std::nullopt;
    }
    BoundedRecoveryRead result = readBoundedRecoveryBytes(expectedSize, reader);
    if (result.status != BoundedRecoveryReadStatus::success)
    {
        if (error) *error = boundedRecoveryReadError(result.status);
        return std::nullopt;
    }
    return std::move(result.bytes);
}

std::optional<RecoveryRecord> readRecoveryRecordWithNativeDecodeCountForTesting(
    const std::filesystem::path& recoveryPath,
    std::uint64_t* nativeDocumentDecodeCount,
    QString* error)
{
    if (nativeDocumentDecodeCount)
    {
        *nativeDocumentDecodeCount = 0;
    }
    ScopedNativeDocumentDecodeCounter counter(nativeDocumentDecodeCount);
    return readRecoveryRecord(recoveryPath, error);
}

std::optional<RecoveredDocument>
readAndLoadRecoveryDocumentWithNativeDecodeCountForTesting(
    const std::filesystem::path& recoveryPath,
    std::uint64_t* nativeDocumentDecodeCount,
    QString* error)
{
    if (nativeDocumentDecodeCount)
    {
        *nativeDocumentDecodeCount = 0;
    }
    ScopedNativeDocumentDecodeCounter counter(nativeDocumentDecodeCount);
    std::optional<RecoveryRecord> record = readRecoveryRecord(
        recoveryPath, error);
    if (!record || !record->metadata.projectTypeKnown())
    {
        return std::nullopt;
    }
    return loadRecoveredDocument(std::move(*record), error);
}

std::vector<qint64> canonicalHistoryBaseReferencesForTesting(
    const Level& currentLevel)
{
    try
    {
        const std::vector<std::byte> nestedBytes = assets::saveToSedBytes(
            currentLevel, {}, assets::SedSerializationProfile::HistoryNested);
        return canonicalBaseReferences(historyContentFragments(
            nestedBytes, HistoryFragmentLayout::SectorRecordsV1));
    }
    catch (const std::exception&)
    {
        return {};
    }
}

} // namespace detail

static std::optional<RecoveryContainerBytes> readRecoveryFileBytes(
    QFile& file,
    const QString& path,
    QString* error)
{
    const qint64 signedSize = file.size();
    if (signedSize <= 0
        || static_cast<quint64>(signedSize) > kMaximumContainerBytes)
    {
        if (error) *error = QStringLiteral("Recovery container size is invalid.");
        return std::nullopt;
    }

    BoundedRecoveryRead ordinaryRead{};
    if (file.seek(0))
    {
        ordinaryRead = readBoundedRecoveryBytes(signedSize,
            [&file](char* const destination, const qint64 count)
            {
                return file.read(destination, count);
            });
        if (ordinaryRead.status == BoundedRecoveryReadStatus::success
            && file.error() == QFileDevice::NoError)
        {
            // The one-byte probe catches an append visible through this open
            // handle. Re-stat as well so an append/truncate racing immediately
            // after EOF cannot make a stale-size prefix look canonical.
            if (file.size() != signedSize)
            {
                if (error) *error = QStringLiteral(
                    "Recovery container size changed while it was being read.");
                return std::nullopt;
            }
            RecoveryContainerBytes ordinary{};
            ordinary.bytes = std::move(ordinaryRead.bytes);
            return ordinary;
        }
    }

    if (ordinaryRead.status == BoundedRecoveryReadStatus::grew
        || ordinaryRead.status == BoundedRecoveryReadStatus::shortRead)
    {
        if (error) *error = boundedRecoveryReadError(ordinaryRead.status);
        return std::nullopt;
    }
    // A successful helper result accompanied by a late QFile error can still
    // own the full admitted allocation. Release it before allocating the
    // resilient destination so a near-cap retry never transiently holds two
    // complete container buffers.
    ordinaryRead.bytes = QByteArray{};

    // QFile can remain in an error state after a device read failure. Retry
    // each independent block through a separately reopenable handle so one
    // bad range does not prevent later DOCM stripes/sections from being read.
    QFile rangeFile(path);
    bool rangeSizeChanged = false;
    const RecoveryRangeReader rangeReader = [&rangeFile, &rangeSizeChanged, signedSize](
        const std::uint64_t offset,
        const std::span<std::byte> destination)
    {
        if (!rangeFile.isOpen() && !rangeFile.open(QIODevice::ReadOnly))
        {
            return false;
        }
        if (rangeFile.size() != signedSize)
        {
            rangeSizeChanged = true;
            rangeFile.close();
            return false;
        }
        if (!rangeFile.seek(static_cast<qint64>(offset)))
        {
            rangeFile.close();
            return false;
        }
        qsizetype filled{};
        while (filled < static_cast<qsizetype>(destination.size()))
        {
            const qint64 count = rangeFile.read(
                reinterpret_cast<char*>(destination.data()) + filled,
                static_cast<qint64>(destination.size()) - filled);
            if (count <= 0)
            {
                rangeFile.close();
                return false;
            }
            filled += static_cast<qsizetype>(count);
        }
        if (rangeFile.size() != signedSize)
        {
            rangeSizeChanged = true;
            rangeFile.close();
            return false;
        }
        return true;
    };
    std::optional<RecoveryContainerBytes> recovered = readRecoveryContainerResilient(
        static_cast<std::uint64_t>(signedSize), rangeReader, 4096, 3, error);
    if (rangeSizeChanged || QFileInfo(path).size() != signedSize)
    {
        if (error) *error = QStringLiteral(
            "Recovery container size changed during resilient reading.");
        return std::nullopt;
    }
    return recovered;
}

namespace
{

bool writeRecoveryBytesAtomically(
    const QByteArray& bytes,
    const std::filesystem::path& destination,
    QString* error)
{
    QSaveFile output(pathToQString(destination));
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly))
    {
        if (error) *error = output.errorString();
        return false;
    }
    qint64 offset{};
    while (offset < bytes.size())
    {
        const qint64 written = output.write(
            bytes.constData() + offset, bytes.size() - offset);
        if (written <= 0)
        {
            if (error) *error = output.errorString();
            output.cancelWriting();
            return false;
        }
        offset += written;
    }
    if (!output.commit())
    {
        if (error) *error = output.errorString();
        return false;
    }
    return true;
}

struct PendingHistoricalSnapshot
{
    QByteArray bytes{};
    RecoveryMetadata metadata{};
    std::filesystem::path sourcePath{};
};

std::optional<PendingHistoricalSnapshot> capturePreviousRecoverySnapshot(
    const WriteRequest& request,
    QString* warning)
{
    const std::filesystem::path sourcePath = request.archiveSourcePath.empty()
        ? request.recoveryPath
        : request.archiveSourcePath;
    if (!request.archivePreviousSnapshot
        || request.maximumHistoricalSnapshots == 0
        || !QFileInfo::exists(pathToQString(sourcePath)))
    {
        return std::nullopt;
    }

    QString descriptorError{};
    const std::optional<RecoveryDescriptor> previous = readRecoveryDescriptor(
        sourcePath, &descriptorError);
    if (!previous)
    {
        if (warning) *warning = QStringLiteral(
            "Could not retain the preceding autosave in history: %1")
            .arg(descriptorError);
        return std::nullopt;
    }
    // A clean live baseline exists only to close the hard-kill window. It is
    // never useful as a historical restore point and would create the exact
    // misleading "unsaved changes" entries this archive is designed to avoid.
    if (!previous->metadata.requiresUserRecovery)
    {
        return std::nullopt;
    }

    QFile input(pathToQString(sourcePath));
    const qint64 admittedSize = QFileInfo(input).size();
    if (admittedSize <= 0
        || admittedSize > static_cast<qint64>(assets::SedMaximumFileBytes)
        || !input.open(QIODevice::ReadOnly))
    {
        if (warning) *warning = QStringLiteral(
            "Could not retain the preceding autosave in history: %1")
            .arg(input.errorString());
        return std::nullopt;
    }
    BoundedRecoveryRead captured = readBoundedRecoveryBytes(
        admittedSize,
        [&input](char* const destination, const qint64 count)
        {
            return input.read(destination, count);
        });
    if (captured.status != BoundedRecoveryReadStatus::success
        || input.error() != QFileDevice::NoError
        || input.size() != admittedSize)
    {
        if (warning) *warning = QStringLiteral(
            "Could not retain the preceding autosave because it changed while being captured.");
        return std::nullopt;
    }
    return PendingHistoricalSnapshot{
        std::move(captured.bytes), previous->metadata, sourcePath };
}

std::optional<std::filesystem::path> publishHistoricalSnapshot(
    const PendingHistoricalSnapshot& previous,
    QString* error)
{
    const std::filesystem::path& sourcePath = previous.sourcePath;

    // Rotation is archive-first. If the following live QSaveFile commit is
    // transiently denied, the GUI retries the same obligation. Name that
    // archive from the authenticated old snapshot instead of a random UUID:
    // every retry atomically replaces the same history object rather than
    // allocating another up-to-256 MiB duplicate until the disk is full.
    // documentSha256 binds the complete canonical DOCM; timestamp separates
    // otherwise identical document images intentionally captured at distinct
    // historical points in this editing session. Keep one 128-bit combined
    // identity/session token in the archive name; full association remains
    // authenticated by META after discovery.
    // The display basename comes from protected META rather than the live
    // recovery path, avoiding names such as `level-<hash>.autosave-...` that
    // were both unreadable and contained a second dot-delimited suffix.
    // Keep an 80-bit document fingerprint so retries map to one deterministic
    // archive while genuinely different checkpoints in the same second still
    // get separate history names.
    const QByteArray familyToken = recoveryFamilyToken(
        previous.metadata.identity, previous.metadata.sessionId);
    const QString documentFingerprint = QString::fromLatin1(
        recoveryBase32Encode(
            previous.metadata.documentSha256.left(10)));
    const QString timestamp = QDateTime::fromMSecsSinceEpoch(
        previous.metadata.timestamp, Qt::UTC)
        .toString(QStringLiteral("yyyyMMdd'T'HHmmss'Z'"));
    const QString archiveName = QStringLiteral(
        "%1-%2-autosave-%3-%4-%5.sedautosave")
        .arg(sanitizeRecoveryBasename(previous.metadata.displayName))
        .arg(recoveryProjectTag(previous.metadata.projectType))
        .arg(timestamp)
        .arg(QString::fromLatin1(familyToken))
        .arg(documentFingerprint);
    const std::filesystem::path archivePath = sourcePath.parent_path()
        / archiveName.toStdWString();
    QString copyError{};
    if (!writeRecoveryBytesAtomically(previous.bytes, archivePath, &copyError))
    {
        if (error) *error = QStringLiteral(
            "Could not retain the preceding autosave in history: %1")
            .arg(copyError);
        return std::nullopt;
    }

    return archivePath;
}

QString pruneHistoricalSnapshots(
    const PendingHistoricalSnapshot& previous,
    const std::size_t maximumHistoricalSnapshots)
{
    const std::filesystem::path& sourcePath = previous.sourcePath;
    std::vector<RecoveryDescriptor> sameIdentity = scanRecoveryFamily(
        sourcePath.parent_path(), previous.metadata.identity,
        previous.metadata.sessionId);
    std::erase_if(sameIdentity,
        [](const RecoveryDescriptor& descriptor)
        {
            return !QFileInfo(pathToQString(descriptor.recoveryPath))
                .fileName().endsWith(
                    QStringLiteral(".sedautosave"), Qt::CaseInsensitive);
        });
    std::sort(sameIdentity.begin(), sameIdentity.end(),
        [](const RecoveryDescriptor& lhs, const RecoveryDescriptor& rhs)
        {
            return lhs.metadata.timestamp > rhs.metadata.timestamp;
        });
    QStringList pruneFailures{};
    for (std::size_t i = maximumHistoricalSnapshots;
         i < sameIdentity.size(); ++i)
    {
        QString removalError{};
        if (!invalidateAndRemoveRecovery(
                nullptr, sameIdentity[i].recoveryPath, &removalError,
                &sameIdentity[i].metadata,
                RecoveryRemovalReason::retentionPrune))
        {
            pruneFailures.push_back(removalError.isEmpty()
                ? QFileInfo(pathToQString(sameIdentity[i].recoveryPath)).fileName()
                : removalError);
        }
    }
    return pruneFailures.isEmpty()
        ? QString{}
        : QStringLiteral("Could not prune old autosaves: %1")
            .arg(pruneFailures.join(QStringLiteral(", ")));
}

} // namespace

WriteResult writeRecoverySnapshot(WriteRequest request)
{
    WriteResult result{};
    result.recoveryPath = request.recoveryPath;

    try
    {
        if (!request.publicationGate
            || request.publicationGate->generation.load(std::memory_order_acquire) != request.generation)
        {
            result.status = WriteStatus::superseded;
            return result;
        }

        if (request.recoveryPath.empty())
        {
            result.error = QStringLiteral("Recovery destination path is empty.");
            return result;
        }
        // Writers run from a live Document, whose Level kind is always the
        // authoritative project discriminator.  Normalize legacy callers
        // that leave the new trust field at its storage default; readers must
        // never make this same inference from a damaged on-disk Level.
        if (!request.metadata.projectTypeKnown())
        {
            request.metadata.projectType = request.document.level().kind;
            request.metadata.projectTypeTrust = ProjectTypeTrust::metadata;
        }
        if (!recoveryMetadataAssociationCoherent(request.metadata)
            || request.document.level().kind != request.metadata.projectType)
        {
            result.error = QStringLiteral(
                "Recovery metadata does not match the document project type or source association.");
            return result;
        }

        // A recovery checkpoint always publishes the complete native Level
        // and EDIT state. HIST is a lower-priority optimization: if its
        // encoder or semantic verifier fails, publish the authoritative core
        // checkpoint without it and make the degradation visible in both the
        // WARN chunk and the worker result.
        if (request.editorStatePayload.isEmpty()
            || static_cast<quint64>(request.editorStatePayload.size())
                > assets::SedMaximumSemanticSectionBytes)
        {
            result.error = QStringLiteral(
                "Recovery editor/COG state is required and exceeds the supported safety limit.");
            return result;
        }
        constexpr RecoveryCborLimits editorLimits{
            assets::SedMaximumSemanticSectionBytes,
            2'000'000,
            64,
            3ull * 64ull * 1024ull * 1024ull,
            assets::SedMaximumSemanticSectionBytes,
            assets::SedMaximumDecodedObjectBytes,
        };
        QString editorCborError{};
        const std::optional<QCborValue> editorState = deserializeCanonicalCbor(
            request.editorStatePayload, editorLimits, &editorCborError);
        if (!editorState || !editorState->isMap())
        {
            result.error = QStringLiteral(
                "Recovery editor/COG state is not a deterministic bounded CBOR map: %1.")
                    .arg(editorCborError);
            return result;
        }

        QByteArray historyPayload{};
        QByteArray editorStatePayload = request.editorStatePayload;
        bool includeHistory = true;
        const auto historyWarning = [&result](const QString& detail)
        {
            result.warning = QStringLiteral(
                "Recovery published Level+EDIT; HIST was omitted: %1").arg(detail);
        };
        try
        {
            // Every generated nested state already passed the complete core
            // Level writer preflight, exact section framing, and the bounded
            // canonical HIST envelope checks. Do not immediately replay the
            // entire optional timeline on this background publication path:
            // Open/recovery performs the same strict semantic decode before
            // HIST can be installed.  Keep this admission check on the
            // worker as well: publishing a stream which the reader would
            // immediately reject creates a misleading autosave that appears
            // complete until the next restart.  The compact decoder retains
            // only the concrete snapshot domains (including the baked-light
            // colour graph), so this is bounded by the same V1 budgets rather
            // than by a second expanded history vector.
            QByteArray boundEditorState{};
            QString attachmentWarning{};
            historyPayload = serializeEditorDocumentHistoryWithWorkspaceBindings(
                request.document,
                editorStatePayload,
                &boundEditorState,
                &attachmentWarning);
            editorStatePayload = std::move(boundEditorState);
            if (!attachmentWarning.isEmpty())
            {
                result.warning = attachmentWarning;
            }
            if (historyPayload.isEmpty()
                || historyPayload.size() > kMaximumHistoryPayloadBytes)
            {
                includeHistory = false;
                historyWarning(QStringLiteral("encoded history exceeded the supported safety limit"));
            }
            else
            {
                historyPayload = validateGeneratedRecoveryHistory(
                    std::move(historyPayload),
                    request.document.level().kind,
                    request.document.current().level,
                    request.document.undoDepth(),
                    request.document.redoDepth());
            }
        }
        catch (const std::exception& e)
        {
            includeHistory = false;
            historyWarning(QString::fromUtf8(e.what()));
        }

        if (!includeHistory)
        {
            QString attachmentWarning{};
            QByteArray withoutAttachments =
                omitEditorWorkspaceHistoryAttachments(
                    request.editorStatePayload, &attachmentWarning);
            if (withoutAttachments.isEmpty())
            {
                result.error = attachmentWarning.isEmpty()
                    ? QStringLiteral(
                        "Recovery could not preserve the required EDIT payload after HIST degradation.")
                    : attachmentWarning;
                return result;
            }
            editorStatePayload = std::move(withoutAttachments);
            if (!attachmentWarning.isEmpty()
                && !result.warning.contains(attachmentWarning))
            {
                result.warning += QStringLiteral(" ") + attachmentWarning;
            }
        }

        std::vector<assets::SedOptionalSection> optionalSections{};
        optionalSections.push_back(assets::SedOptionalSection{
            { 'e', 'd', 'i', 't' },
            std::vector<std::byte>(
                reinterpret_cast<const std::byte*>(editorStatePayload.constData()),
                reinterpret_cast<const std::byte*>(editorStatePayload.constData())
                    + editorStatePayload.size()) });
        if (includeHistory)
        {
            optionalSections.push_back(assets::SedOptionalSection{
                { 'h', 'i', 's', 't' },
                std::vector<std::byte>(
                    reinterpret_cast<const std::byte*>(historyPayload.constData()),
                    reinterpret_cast<const std::byte*>(historyPayload.constData())
                        + historyPayload.size()) });
        }

        const std::int64_t documentTimestamp = request.metadata.timestamp;
        QByteArray documentPayload = serializeLevelToSed(
            request.document.level(), optionalSections,
            assets::SedSerializationProfile::ProtectedV1,
            documentTimestamp);
        if (documentPayload.isEmpty()
            || static_cast<quint64>(documentPayload.size()) > kMaximumChunkBytes)
        {
            result.error = QStringLiteral(
                "Binary SED recovery serializer produced an invalid DOCM payload size.");
            return result;
        }
        QByteArray canonicalLevelSha256{};
        try
        {
            assets::SedFile roundTrip = assets::loadSedFileBytes(std::as_bytes(std::span(
                documentPayload.constData(), static_cast<std::size_t>(documentPayload.size()))));
            bool optionalSectionsMatch =
                roundTrip.optionalSections.size() == optionalSections.size();
            if (optionalSectionsMatch)
            {
                for (std::size_t i = 0; i < optionalSections.size(); ++i)
                {
                    const assets::SedOptionalSection& expected = optionalSections[i];
                    const assets::SedOptionalSection& actual = roundTrip.optionalSections[i];
                    if (actual.type != expected.type || actual.payload != expected.payload)
                    {
                        optionalSectionsMatch = false;
                        break;
                    }
                }
            }
            const QByteArray canonicalLevelPayload = serializeLevelToSed(
                roundTrip.level, {}, assets::SedSerializationProfile::UnprotectedNested);
            if (roundTrip.hadCorruption
                || roundTrip.level.kind != request.metadata.projectType
                || roundTrip.timestamp != documentTimestamp
                || !optionalSectionsMatch
                || !assets::sedLevelNativeSerializationMatches(
                    request.document.level(), roundTrip.level)
                || canonicalLevelPayload.isEmpty())
            {
                result.error = QStringLiteral(
                    "Binary SED DOCM failed strict project, field-complete Level, or optional-section validation.");
                return result;
            }
            // Layer visibility is editor workspace state carried by EDIT. It
            // is serialized in the authoritative Level for faithful restore,
            // but must not make an authored document dirty. Compute the same
            // authored digest used by Save/Open baselines on this worker.
            roundTrip.level.lvisstring.clear();
            canonicalLevelSha256 = QCryptographicHash::hash(
                canonicalLevelPayload, QCryptographicHash::Sha256);
            result.levelStateHash = authoredLevelStateHash(roundTrip.level);
            if (result.levelStateHash.isEmpty())
            {
                result.error = QStringLiteral(
                    "Binary SED DOCM authored-Level digest serialization failed.");
                return result;
            }
        }
        catch (const std::exception& e)
        {
            result.error = QStringLiteral("Binary SED DOCM validation failed: %1")
                .arg(QString::fromUtf8(e.what()));
            return result;
        }

        request.metadata.levelSha256 = canonicalLevelSha256;
        request.metadata.requiresUserRecovery =
            request.metadata.requiresUserRecovery
            || (!request.savedLevelStateHash.isEmpty()
                && request.savedLevelStateHash != result.levelStateHash);
        request.metadata.documentSha256 = QCryptographicHash::hash(
            documentPayload, QCryptographicHash::Sha256);
        const QByteArray header = QJsonDocument(metadataToJson(request.metadata, 1))
            .toJson(QJsonDocument::Compact);
        int verifiedMetadataVersion{};
        QString metadataError{};
        const std::optional<RecoveryMetadata> verifiedMetadata = metadataFromJson(
            header, &verifiedMetadataVersion, &metadataError);
        if (!verifiedMetadata || verifiedMetadataVersion != 1
            || !recoveryMetadataAssociationCoherent(*verifiedMetadata)
            || verifiedMetadata->levelSha256 != request.metadata.levelSha256
            || verifiedMetadata->documentSha256 != request.metadata.documentSha256)
        {
            result.error = metadataError.isEmpty()
                ? QStringLiteral("Recovery metadata digest binding failed self-verification.")
                : metadataError;
            return result;
        }

        // META is the only outer protected payload. DOCM is deliberately raw:
        // native SED V1 already protects every semantic section and has a
        // mirrored fixed header, so wrapping it again would obscure repair.
        EncodedChunk metadataChunk = makeProtectedChunk("meta", header);
        EncodedChunk documentChunk = makeNativeDocumentChunk(
            std::move(documentPayload));
        std::optional<EncodedChunk> warningChunk{};
        if (!result.warning.isEmpty())
        {
            warningChunk = makeChunk("warn", result.warning.toUtf8());
        }

        // Verify every fully encoded chunk before opening QSaveFile. This is
        // deliberately worker-side: an encoder/FEC regression must fail while
        // the previous checkpoint is still authoritative, never after its
        // atomic replacement has begun.
        QString selfCheckError{};
        const auto checked = [&selfCheckError](const EncodedChunk& chunk)
        {
            return encodedChunkSelfCheck(chunk, &selfCheckError);
        };
        if (!checked(metadataChunk) || !checked(documentChunk)
            || (warningChunk && !checked(*warningChunk)))
        {
            result.error = QStringLiteral(
                "Recovery encoder self-check failed before publication: %1")
                .arg(selfCheckError);
            return result;
        }

        if (request.publicationGate->generation.load(std::memory_order_acquire) != request.generation)
        {
            result.status = WriteStatus::superseded;
            return result;
        }

        // Capture the old complete bytes before staging replacement. It is
        // published under the gate immediately before live commit; pruning is
        // delayed until both durable outputs exist.
        QString historicalCaptureWarning{};
        const std::optional<PendingHistoricalSnapshot> historicalSnapshot =
            capturePreviousRecoverySnapshot(request, &historicalCaptureWarning);
        if (!historicalCaptureWarning.isEmpty())
        {
            // Rotation was explicitly requested and the preceding durable
            // bytes could not be captured safely. Keep that live checkpoint
            // authoritative instead of replacing the only known-good point.
            result.error = historicalCaptureWarning;
            return result;
        }

        const std::filesystem::path parent = request.recoveryPath.parent_path();
        if (!QDir().mkpath(pathToQString(parent)))
        {
            result.error = QStringLiteral("Could not create recovery directory: %1")
                .arg(QDir::toNativeSeparators(pathToQString(parent)));
            return result;
        }

        const QByteArray headerHash = QCryptographicHash::hash(
            header, QCryptographicHash::Sha256).toHex();
        QSaveFile file(pathToQString(request.recoveryPath));
        // A direct-write fallback could destroy the previous valid recovery if
        // the process or machine stops mid-write. Fail safely instead.
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly))
        {
            result.error = QStringLiteral("Could not stage recovery snapshot: %1").arg(file.errorString());
            return result;
        }

        const QByteArray prefix = QByteArray(kRecoveryMagicV1)
            + QByteArray::number(header.size()) + '\n'
            + headerHash + '\n';
        quint64 containerSize = static_cast<quint64>(prefix.size() + header.size());
        const auto addChunkSize = [&containerSize](const EncodedChunk& chunk)
        {
            const quint64 encodedSize = kChunkHeaderBytes
                + static_cast<quint64>(chunk.stored.size());
            if (containerSize > kMaximumContainerBytes - encodedSize)
            {
                return false;
            }
            containerSize += encodedSize;
            return true;
        };
        if (!addChunkSize(metadataChunk) || !addChunkSize(documentChunk)
            || (warningChunk && !addChunkSize(*warningChunk)))
        {
            result.error = QStringLiteral("Recovery container exceeds the supported safety limit.");
            file.cancelWriting();
            return result;
        }
        const auto writeAll = [&file](const char* data, const qint64 size) -> bool
        {
            qint64 offset{};
            while (offset < size)
            {
                const qint64 written = file.write(data + offset, size - offset);
                if (written <= 0) return false;
                offset += written;
            }
            return true;
        };
        const auto writeChunk = [&](const EncodedChunk& chunk) -> bool
        {
            const QByteArray encodedHeader = chunkHeader(chunk);
            return writeAll(encodedHeader.constData(), encodedHeader.size())
                && writeAll(chunk.stored.constData(), chunk.stored.size());
        };
        if (!writeAll(prefix.constData(), prefix.size())
            || !writeAll(header.constData(), header.size())
            || !writeChunk(metadataChunk)
            || !writeChunk(documentChunk)
            || (warningChunk && !writeChunk(*warningChunk)))
        {
            result.error = QStringLiteral("Could not write recovery snapshot: %1").arg(file.errorString());
            file.cancelWriting();
            return result;
        }

        {
            std::lock_guard<std::mutex> publishLock(
                request.publicationGate->publishMutex);
            if (request.publicationGate->generation.load(
                    std::memory_order_acquire) != request.generation)
            {
                file.cancelWriting();
                result.status = WriteStatus::superseded;
                return result;
            }

            // Three-phase rotation: publish the checked previous bytes first,
            // but do not prune yet. A crash/failure before the live commit can
            // then leave only a harmless duplicate, never a window where the
            // previous live point exists solely in RAM.
            if (historicalSnapshot)
            {
                QString archiveError{};
#ifdef SED_APP_RECOVERY_TEST_ACCESS
                if (request.archivePublicationAllowedForTest
                    && !request.archivePublicationAllowedForTest())
                {
                    file.cancelWriting();
                    result.error = QStringLiteral(
                        "Test-injected historical autosave publication failure.");
                    return result;
                }
#endif
                if (!publishHistoricalSnapshot(
                        *historicalSnapshot, &archiveError))
                {
                    file.cancelWriting();
                    result.error = archiveError;
                    return result;
                }
            }

#ifdef SED_APP_RECOVERY_TEST_ACCESS
            if (request.liveCommitAllowedForTest
                && !request.liveCommitAllowedForTest())
            {
                file.cancelWriting();
                result.error = QStringLiteral(
                    "Test-injected live recovery commit failure.");
                return result;
            }
#endif
            if (!file.commit())
            {
                result.error = QStringLiteral(
                    "Could not atomically publish recovery snapshot: %1")
                    .arg(file.errorString());
                return result;
            }
        }

        // Retention maintenance does not participate in the live publication
        // transaction. Run its bounded, metadata-only family scan after
        // releasing the gate so a directory full of malformed entries cannot
        // block Save, shutdown, or explicit invalidation behind the writer's
        // publication mutex.
        QString archiveWarning{};
        if (historicalSnapshot)
        {
            const QString publicationWarning = pruneHistoricalSnapshots(
                *historicalSnapshot, request.maximumHistoricalSnapshots);
            if (!publicationWarning.isEmpty())
            {
                archiveWarning = archiveWarning.isEmpty()
                    ? publicationWarning
                    : archiveWarning + QStringLiteral(" ") + publicationWarning;
            }
        }
        if (!archiveWarning.isEmpty())
        {
            result.warning = result.warning.isEmpty()
                ? archiveWarning
                : result.warning + QStringLiteral(" ") + archiveWarning;
        }

        result.status = WriteStatus::published;
        return result;
    }
    catch (const std::exception& e)
    {
        result.error = QString::fromUtf8(e.what());
        return result;
    }
    catch (...)
    {
        result.error = QStringLiteral("Unknown recovery writer failure.");
        return result;
    }
}

std::optional<RecoveryRecord> readRecoveryRecord(
    const std::filesystem::path& recoveryPath,
    QString* error,
    const WorkspaceHistoryAttachmentLevelValidator attachmentValidator)
{
    try
    {
    const QString path = pathToQString(recoveryPath);
    const qint64 fallbackModifiedAtMs = QFileInfo(path).lastModified().toMSecsSinceEpoch();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error)
        {
            *error = QStringLiteral("Could not open recovery snapshot: %1").arg(file.errorString());
        }
        return std::nullopt;
    }

    const auto container = readRecoveryFileBytes(file, path, error);
    if (!container)
    {
        return std::nullopt;
    }
    QBuffer buffer{};
    buffer.setData(container->bytes);
    if (!buffer.open(QIODevice::ReadOnly))
    {
        if (error) *error = QStringLiteral("Could not stage recovery bytes for validation.");
        return std::nullopt;
    }
    std::optional<ParsedRecoveryFile> parsed = parseRecoveryFile(
        buffer, fallbackModifiedAtMs, true, error, attachmentValidator);
    if (!parsed)
    {
        return std::nullopt;
    }
    if (!container->unreadableRanges.empty())
    {
        parsed->corrupted = true;
        parsed->unreadableRanges = container->unreadableRanges;
        for (const RecoveryUnreadableRange& range : container->unreadableRanges)
        {
            parsed->warnings.push_back(QStringLiteral(
                "Recovery storage range [%1, %2) remained unreadable after three attempts; it was treated as a zero-filled erasure.")
                .arg(range.offset)
                .arg(range.offset + range.size));
        }
    }

    RecoveryRecord record{};
    record.metadata = parsed->metadata;
    record.documentTimestamp = parsed->documentTimestamp;
    record.levelPayload = parsed->payload;
    record.decodedLevel = std::move(parsed->decodedLevel);
    record.nativeProjectType = parsed->nativeProjectType;
    record.nativeProjectTypeTrusted = parsed->nativeProjectTypeTrusted;
    record.editorStatePayload = parsed->editorStatePayload;
    record.historyPayload = parsed->deferredHistoryPayload;
    record.history = std::move(parsed->history);
    record.workspaceHistory = std::move(parsed->workspaceHistory);
    record.warnings = parsed->warnings;
    record.partialRecovery = parsed->partialRecovery;
    record.corrupted = parsed->corrupted;
    record.repairedNativeChunkCount = parsed->repairedNativeChunkCount;
    record.repairedMetadataShardCount = parsed->repairedMetadataShardCount;
    record.missingSections = parsed->missingSections;
    record.unreadableRanges = parsed->unreadableRanges;
    record.lostSectionCount = parsed->lostSectionCount;
    record.unidentifiedMissingSectionCount =
        parsed->unidentifiedMissingSectionCount;
    record.recoveryPath = recoveryPath;
    return record;
    }
    catch (const std::bad_alloc&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        if (error) *error = QStringLiteral("Recovery validation failed safely: %1")
            .arg(QString::fromUtf8(e.what()));
        return std::nullopt;
    }
    catch (...)
    {
        if (error) *error = QStringLiteral("Recovery validation failed safely after an unknown error.");
        return std::nullopt;
    }
}

std::optional<RecoveryDescriptor> readRecoveryDescriptor(
    const std::filesystem::path& recoveryPath,
    QString* error)
{
    try
    {
    const QString path = pathToQString(recoveryPath);
    const qint64 fallbackModifiedAtMs = QFileInfo(path).lastModified().toMSecsSinceEpoch();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error)
        {
            *error = QStringLiteral("Could not open recovery snapshot: %1").arg(file.errorString());
        }
        return std::nullopt;
    }

    // Discovery is metadata-only. Do not stage the full file or decode DOCM,
    // EDIT, or HIST while walking a recovery directory.
    const std::optional<ParsedRecoveryFile> parsed = parseRecoveryFile(
        file, fallbackModifiedAtMs, false, error);
    if (!parsed)
    {
        return std::nullopt;
    }
    return RecoveryDescriptor{ parsed->metadata, recoveryPath };
    }
    catch (const std::bad_alloc&)
    {
        if (error)
        {
            *error = QStringLiteral(
                "Recovery metadata validation exceeded its bounded memory budget.");
        }
        return std::nullopt;
    }
    catch (const std::exception& e)
    {
        if (error) *error = QStringLiteral("Recovery metadata validation failed safely: %1")
            .arg(QString::fromUtf8(e.what()));
        return std::nullopt;
    }
    catch (...)
    {
        if (error) *error = QStringLiteral(
            "Recovery metadata validation failed safely after an unknown error.");
        return std::nullopt;
    }
}

namespace
{

enum class RecoveryScanKind
{
    live,
    historical,
};

std::vector<RecoveryDescriptor> scanRecoveryFiles(
    const std::filesystem::path& recoveryDirectory,
    const RecoveryScanKind kind)
{
    const auto descriptorComesFirst = [](
        const RecoveryDescriptor& lhs,
        const RecoveryDescriptor& rhs)
    {
        // A malformed/legacy clean entry must never consume a retained slot
        // ahead of recoverable authored work in either bounded scan.
        if (lhs.metadata.requiresUserRecovery
            != rhs.metadata.requiresUserRecovery)
        {
            return lhs.metadata.requiresUserRecovery;
        }
        if (lhs.metadata.timestamp != rhs.metadata.timestamp)
        {
            return lhs.metadata.timestamp > rhs.metadata.timestamp;
        }
        const int identityOrder = QString::compare(
            lhs.metadata.identity, rhs.metadata.identity, Qt::CaseSensitive);
        if (identityOrder != 0)
        {
            return identityOrder < 0;
        }
        return QString::compare(
            QDir::cleanPath(pathToQString(lhs.recoveryPath)),
            QDir::cleanPath(pathToQString(rhs.recoveryPath)),
            Qt::CaseSensitive) < 0;
    };

    // QDir::entryInfoList materializes every matching directory entry before
    // discovery can reject any of them. Stream at most the bounded entry count,
    // validate each eligible file under the per-file metadata limits, and only
    // then let it consume one of the 256 retained slots. Filesystem mtimes are
    // untrusted discovery hints: a malformed future-dated decoy must not hide
    // an older valid checkpoint. descriptorComesFirst makes the heap root the
    // least-preferred retained descriptor.
    std::vector<RecoveryDescriptor> recoveries{};
    recoveries.reserve(kMaximumRecoveryScanCandidates);
    const QString suffix = kind == RecoveryScanKind::live
        ? QStringLiteral(".sedrecovery")
        : QStringLiteral(".sedautosave");
    // Apply the format-owned lowercase suffix before walking. Unrelated files
    // in a polluted recovery directory cannot consume the 4096-entry work
    // budget or hide a valid autosave merely by sorting ahead of it.
    QDirIterator files(
        pathToQString(recoveryDirectory),
        QStringList{ QStringLiteral("*") + suffix },
        QDir::Files | QDir::Hidden | QDir::System,
        QDirIterator::NoIteratorFlags);
    std::size_t scannedEntries{};
    while (files.hasNext()
        && scannedEntries < kMaximumRecoveryDirectoryEntriesScanned)
    {
        files.next();
        ++scannedEntries;
        const QFileInfo file = files.fileInfo();
        if (!file.isFile() || file.isSymLink() || !file.isReadable()
            || !file.fileName().endsWith(suffix, Qt::CaseSensitive)
            || file.fileName().contains(
                QStringLiteral(".corrupted-"), Qt::CaseInsensitive))
        {
            continue;
        }

        std::optional<RecoveryDescriptor> descriptor{};
        try
        {
            descriptor = readRecoveryDescriptor(
                std::filesystem::path(file.absoluteFilePath().toStdWString()));
        }
        catch (...)
        {
            // One malformed or allocation-hostile file must never hide the
            // remaining valid snapshots in this bounded directory scan.
            continue;
        }
        if (!descriptor) continue;

        if (recoveries.size() < kMaximumRecoveryScanCandidates)
        {
            recoveries.push_back(std::move(*descriptor));
            std::push_heap(
                recoveries.begin(), recoveries.end(), descriptorComesFirst);
        }
        else if (descriptorComesFirst(*descriptor, recoveries.front()))
        {
            std::pop_heap(
                recoveries.begin(), recoveries.end(), descriptorComesFirst);
            recoveries.back() = std::move(*descriptor);
            std::push_heap(
                recoveries.begin(), recoveries.end(), descriptorComesFirst);
        }
    }
    std::sort(recoveries.begin(), recoveries.end(), descriptorComesFirst);
    return recoveries;
}

} // namespace

std::vector<RecoveryDescriptor> scanRecoveries(
    const std::filesystem::path& recoveryDirectory)
{
    return scanRecoveryFiles(recoveryDirectory, RecoveryScanKind::live);
}

std::vector<RecoveryDescriptor> scanAutosaveHistory(
    const std::filesystem::path& recoveryDirectory)
{
    return scanRecoveryFiles(recoveryDirectory, RecoveryScanKind::historical);
}

std::vector<RecoveryDescriptor> scanRecoveryFamily(
    const std::filesystem::path& recoveryDirectory,
    const QString& identity,
    const QString& sessionId)
{
    const QString normalizedIdentity = identity.trimmed();
    const QString normalizedSessionId = sessionId.trimmed();
    if (normalizedIdentity.isEmpty() || normalizedSessionId.isEmpty())
    {
        return {};
    }

    const QString familyToken = QString::fromLatin1(
        recoveryFamilyToken(normalizedIdentity, normalizedSessionId));
    const QString legacyIdentityDigest = QString::fromLatin1(
        QCryptographicHash::hash(
            normalizedIdentity.toUtf8(), QCryptographicHash::Sha256).toHex());
    const QString legacySessionDigest = QString::fromLatin1(
        QCryptographicHash::hash(
            normalizedSessionId.toUtf8(), QCryptographicHash::Sha256).toHex());
    const QStringList nameFilters{
        QStringLiteral("*-%1.sedrecovery")
            .arg(familyToken),
        QStringLiteral("*-%1-*.sedautosave")
            .arg(familyToken),
        // V1 files created before the compact reader-friendly naming scheme
        // remain discoverable and removable. Their historical archive names
        // did not include identity/session tokens, so only the live pattern
        // can be narrowed safely here; history browsing still discovers
        // those files through scanAutosaveHistory().
        QStringLiteral("*-%1-%2.sedrecovery")
            .arg(legacyIdentityDigest, legacySessionDigest),
    };

    std::vector<RecoveryDescriptor> descriptors{};
    descriptors.reserve(16);
    QDirIterator files(
        pathToQString(recoveryDirectory),
        nameFilters,
        QDir::Files | QDir::Hidden | QDir::System,
        QDirIterator::NoIteratorFlags);
    std::size_t scannedEntries{};
    while (files.hasNext()
        && scannedEntries < kMaximumRecoveryFamilyEntriesScanned)
    {
        files.next();
        ++scannedEntries;
        const QFileInfo file = files.fileInfo();
        if (file.isSymLink() || !file.isReadable()
            || file.fileName().contains(
                QStringLiteral(".corrupted-"), Qt::CaseInsensitive))
        {
            continue;
        }
        const auto descriptor = readRecoveryDescriptor(
            std::filesystem::path(file.absoluteFilePath().toStdWString()));
        if (descriptor
            && descriptor->metadata.identity == normalizedIdentity
            && descriptor->metadata.sessionId == normalizedSessionId)
        {
            descriptors.push_back(*descriptor);
        }
    }
    std::sort(descriptors.begin(), descriptors.end(),
        [](const RecoveryDescriptor& lhs, const RecoveryDescriptor& rhs)
        {
            return lhs.metadata.timestamp > rhs.metadata.timestamp;
        });
    return descriptors;
}

bool isRecoveryNewerThanSource(const RecoveryMetadata& metadata)
{
    if (metadata.sourcePath.trimmed().isEmpty())
    {
        return true;
    }

    const QFileInfo source(metadata.sourcePath);
    if (!source.exists())
    {
        return true;
    }

    // Applying recovery never writes the source. Do not hide a newer snapshot
    // merely because the file changed independently; the UI reports that
    // conflict and keeps the recovered document explicitly unsaved.
    return metadata.timestamp > source.lastModified().toMSecsSinceEpoch();
}

bool recoverySourceChanged(const RecoveryMetadata& metadata)
{
    if (metadata.sourcePath.trimmed().isEmpty())
    {
        return false;
    }
    const QFileInfo source(metadata.sourcePath);
    const qint64 currentModifiedAtMs = source.exists()
        ? source.lastModified().toMSecsSinceEpoch()
        : -1;
    return currentModifiedAtMs != metadata.sourceModifiedAtMs;
}

namespace
{

bool validateDecodedRecoveryProject(
    const RecoveryRecord& record,
    const Level& level,
    QString* error)
{
    if (!record.metadata.projectTypeKnown())
    {
        if (error)
        {
            *error = QStringLiteral(
                "Recovered project type is unknown. Choose JKDF2, MotS, or IJIM before loading the Level.");
        }
        return false;
    }

    const bool headMissing = record.missingSections.contains(
        QStringLiteral("head"));
    if (headMissing)
    {
        // A trusted surviving native header remains authoritative even when
        // HEAD was lost. Only genuinely unknown framing permits the explicit
        // user/META project choice to supply safe HEAD defaults.
        if (record.nativeProjectTypeTrusted
            && record.nativeProjectType
            && *record.nativeProjectType != record.metadata.projectType)
        {
            if (error)
            {
                *error = QStringLiteral(
                    "Recovery metadata and trusted native SED project type disagree.");
            }
            return false;
        }
        return true;
    }

    const ProjectType nativeType = record.nativeProjectType.value_or(level.kind);
    if ((record.nativeProjectTypeTrusted
            && nativeType != record.metadata.projectType)
        || level.kind != record.metadata.projectType)
    {
        if (error)
        {
            *error = QStringLiteral(
                "Recovery metadata and binary SED project type disagree.");
        }
        return false;
    }
    return true;
}

void applyDecodedRecoveryDefaults(
    const RecoveryRecord& record,
    Level& level)
{
    if (record.missingSections.contains(QStringLiteral("head")))
    {
        // HEAD is one atomic required semantic domain. The checked native
        // project type, protected META, or explicit user choice has already
        // been validated above; replace only the irrecoverable project/header
        // fields exactly as the legacy second-decode path did.
        ui::project::applyNewProjectTypeDefaults(
            level, record.metadata.projectType);
    }
}

std::optional<Level> copyDecodedRecoveryLevel(
    const RecoveryRecord& record,
    QString* error)
{
    if (!record.decodedLevel)
    {
        return std::nullopt;
    }
    if (!validateDecodedRecoveryProject(record, *record.decodedLevel, error))
    {
        return std::nullopt;
    }
    Level level = *record.decodedLevel;
    applyDecodedRecoveryDefaults(record, level);
    return level;
}

std::optional<Level> takeDecodedRecoveryLevel(
    RecoveryRecord& record,
    QString* error)
{
    if (!record.decodedLevel)
    {
        return std::nullopt;
    }
    if (!validateDecodedRecoveryProject(record, *record.decodedLevel, error))
    {
        return std::nullopt;
    }

    // The validated object graph replaces DOCM for an ordinary consuming
    // install. Required or unidentified Level loss is different: the UI
    // schedules a last-resort text salvage from the damaged native bytes only
    // after this function has assembled the recovered document. Retain DOCM
    // until that worker captures it; optional EDIT/HIST-only loss does not
    // need the raw Level envelope.
    const auto isRequiredLevelSection = [](const QString& section)
    {
        return section == QStringLiteral("head")
            || section == QStringLiteral("rsrc")
            || section == QStringLiteral("sect")
            || section == QStringLiteral("thng")
            || section == QStringLiteral("tmpl")
            || section == QStringLiteral("cogs")
            || section == QStringLiteral("lite")
            || section == QStringLiteral("hnod");
    };
    const bool requiresTextSalvage =
        record.unidentifiedMissingSectionCount != 0
        || std::any_of(record.missingSections.begin(),
            record.missingSections.end(), isRequiredLevelSection);
    if (!requiresTextSalvage)
    {
        record.levelPayload.clear();
    }

    // RecoveryRecord remains copyable for inspection/tests. Move the decoded
    // graph only for the normal unique consuming transaction; otherwise copy
    // so another record cannot observe a moved-from Level.
    const bool uniquelyOwned = record.decodedLevel.use_count() == 1;
    Level level = uniquelyOwned
        ? std::move(*record.decodedLevel)
        : *record.decodedLevel;
    if (uniquelyOwned)
    {
        record.decodedLevel.reset();
    }
    applyDecodedRecoveryDefaults(record, level);
    return level;
}

} // namespace

std::optional<Level> loadRecoveredLevel(const RecoveryRecord& record, QString* error)
{
    try
    {
        if (!record.metadata.projectTypeKnown())
        {
            if (error)
            {
                *error = QStringLiteral(
                    "Recovered project type is unknown. Choose JKDF2, MotS, or IJIM before loading the Level.");
            }
            return std::nullopt;
        }
        if (record.decodedLevel)
        {
            return copyDecodedRecoveryLevel(record, error);
        }
        noteNativeDocumentDecodeForTesting();
        return deserializeLevelFromSed(
            record.levelPayload,
            record.metadata.projectType,
            error,
            record.partialRecovery);
    }
    catch (const std::bad_alloc&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        if (error) *error = QStringLiteral("Recovered level parsing failed safely: %1")
            .arg(QString::fromUtf8(e.what()));
        return std::nullopt;
    }
    catch (...)
    {
        if (error) *error = QStringLiteral("Recovered level parsing failed safely after an unknown error.");
        return std::nullopt;
    }
}

static std::optional<RecoveredDocument> loadRecoveredDocumentImpl(
    const RecoveryRecord& record,
    RecoveryRecord* consumingRecord,
    std::optional<editor::DocumentRecoveryHistory> decodedHistory,
    QString* error,
    const WorkspaceHistoryAttachmentLevelValidator attachmentValidator)
{
    try
    {
        std::optional<Level> level = consumingRecord
            && consumingRecord->decodedLevel
            ? takeDecodedRecoveryLevel(*consumingRecord, error)
            : loadRecoveredLevel(record, error);
        if (!level)
        {
            return std::nullopt;
        }

        RecoveredDocument recovered{};
        recovered.current.level = std::move(*level);
        recovered.documentTimestamp = record.documentTimestamp;
        recovered.history = std::move(decodedHistory);
        recovered.editorStatePayload = record.editorStatePayload;
        recovered.workspaceHistory = record.workspaceHistory;
        recovered.warnings = record.warnings;
        recovered.missingSections = record.missingSections;
        recovered.unreadableRanges = record.unreadableRanges;
        if (!recovered.history && !record.historyPayload.isEmpty())
        {
            if (!record.metadata.projectTypeKnown())
            {
                if (error)
                {
                    *error = QStringLiteral(
                        "Recovered project type is unknown. Choose JKDF2, MotS, or IJIM before decoding undo history.");
                }
                return std::nullopt;
            }
            QString historyError{};
            QString workspaceWarning{};
            QByteArray validatedEditorState{};
            recovered.history =
                deserializeEditorDocumentHistoryForRestoreWithWorkspaceBindings(
                    record.historyPayload,
                    record.metadata.projectType,
                    recovered.current.level,
                    recovered.editorStatePayload,
                    &validatedEditorState,
                    &historyError,
                    &workspaceWarning,
                    &recovered.workspaceHistory,
                    attachmentValidator);
            if (!validatedEditorState.isEmpty())
            {
                recovered.editorStatePayload = std::move(validatedEditorState);
            }
            if (!workspaceWarning.isEmpty())
            {
                recovered.warnings.push_back(workspaceWarning);
            }
            if (!recovered.history)
            {
                // HIST is optional workspace state. A user-selected project
                // type may prove that the preserved raw history is
                // semantically invalid for that project, but it must not veto
                // the authoritative Level.
                if (!recovered.missingSections.contains(
                        QStringLiteral("hist")))
                {
                    recovered.missingSections.push_back(
                        QStringLiteral("hist"));
                }
                recovered.warnings.push_back(QStringLiteral(
                    "Recovered HIST semantic validation failed: %1. The Level was retained, undo/redo history was dropped, and a clean save is required.")
                    .arg(historyError.isEmpty()
                        ? QStringLiteral("unknown schema error")
                        : historyError));
            }
        }
        return recovered;
    }
    catch (const std::bad_alloc&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        if (error) *error = QStringLiteral("Recovered document assembly failed safely: %1")
            .arg(QString::fromUtf8(e.what()));
        return std::nullopt;
    }
    catch (...)
    {
        if (error) *error = QStringLiteral(
            "Recovered document assembly failed safely after an unknown error.");
        return std::nullopt;
    }
}

std::optional<RecoveredDocument> loadRecoveredDocument(
    const RecoveryRecord& record,
    QString* error,
    const WorkspaceHistoryAttachmentLevelValidator attachmentValidator)
{
    return loadRecoveredDocumentImpl(
        record, nullptr, record.history, error, attachmentValidator);
}

std::optional<RecoveredDocument> loadRecoveredDocument(
    RecoveryRecord&& record,
    QString* error,
    const WorkspaceHistoryAttachmentLevelValidator attachmentValidator)
{
    return loadRecoveredDocumentImpl(
        record, &record, std::move(record.history), error, attachmentValidator);
}

QByteArray serializeSelection(const editor::Selection& selection)
{
    return serializeCanonicalCbor(QCborValue(selectionToCbor(selection)));
}

std::optional<editor::Selection> deserializeSelection(
    const QByteArray& payload,
    QString* error)
{
    constexpr RecoveryCborLimits limits{
        assets::SedMaximumSemanticSectionBytes,
        2'000'000,
        64,
        4'096,
        0,
        assets::SedMaximumDecodedObjectBytes,
    };
    QString cborError{};
    const std::optional<QCborValue> value = deserializeCanonicalCbor(
        payload, limits, &cborError);
    const std::optional<editor::Selection> selection = value
        ? selectionFromCbor(*value)
        : std::nullopt;
    if (!selection && error)
    {
        *error = cborError.isEmpty()
            ? QStringLiteral("Recovery selection CBOR is malformed.")
            : QStringLiteral("Recovery selection %1.").arg(cborError);
    }
    return selection;
}

bool invalidateAndRemoveRecovery(
    const std::shared_ptr<PublicationGate>& publicationGate,
    const std::filesystem::path& recoveryPath,
    QString* error,
    const RecoveryMetadata* expectedMetadata,
    const RecoveryRemovalReason reason)
{
    if (error) error->clear();
    if (recoveryPath.empty()) return true;

    // History is a user-facing timeline, not part of live-session cleanup.
    // Enforce that distinction at the final destructive boundary as well as
    // in MainWindow's family-retirement helpers. This protects against a
    // future caller accidentally routing a restore/close/save cleanup through
    // the generic compare-before-delete function. Only the explicit History
    // dialog action and post-publication retention pruning may remove a
    // `.sedautosave`; conversely those reasons may not delete a live slot.
    const bool historical = isAutosaveHistoryPath(recoveryPath);
    if (historical && reason == RecoveryRemovalReason::liveRetirement)
    {
        if (error)
        {
            *error = QStringLiteral(
                "Autosave history is durable user data and may only be deleted from Autosave History or by retention pruning.");
        }
        return false;
    }
    if (!historical
        && (reason == RecoveryRemovalReason::explicitHistoryDelete
            || reason == RecoveryRemovalReason::retentionPrune))
    {
        if (error)
        {
            *error = QStringLiteral(
                "The requested history deletion did not target a .sedautosave entry.");
        }
        return false;
    }

    std::unique_lock<std::mutex> publishLock{};
    if (publicationGate)
    {
        publicationGate->generation.fetch_add(1, std::memory_order_acq_rel);
        publishLock = std::unique_lock<std::mutex>(publicationGate->publishMutex);
    }

    // Recovery paths are primarily produced by Qt directory iteration. On
    // Windows those are normalized with forward separators, while an
    // std::filesystem path assembled from the same directory can contain a
    // mixture of forward and native separators. MSVC's filesystem removal can
    // report `false` with no error for that spelling even though QFile still
    // resolves and opens the entry. Returning success in that case left a
    // discarded checkpoint discoverable. Use the same Qt path layer as the
    // scanner and writer, and verify disappearance before reporting success.
    const QString qtPath = QDir::cleanPath(pathToQString(recoveryPath));
    if (!QFileInfo::exists(qtPath)) return true;

    // Never append an operation token to an already near-limit recovery
    // component. Live names carry a combined family token and autosaves add
    // their capture fingerprint, so a naive `.deleting-<uuid>` suffix can
    // exceed the portable 255-code-unit component limit and make safe pruning
    // silently impossible. Keep operation siblings collision-safe and below
    // a conservative 240-unit ceiling.
    const QFileInfo originalInfo(qtPath);
    const auto uniqueSiblingPath = [&originalInfo](
        const QString& marker, const bool retainRecoverySuffix)
    {
        const QString suffix = retainRecoverySuffix
            && !originalInfo.suffix().isEmpty()
            ? QStringLiteral(".") + originalInfo.suffix()
            : QStringLiteral(".seddelete");
        const QString tail = QStringLiteral(".%1-%2%3")
            .arg(marker,
                QUuid::createUuid().toString(QUuid::WithoutBraces),
                suffix);
        constexpr qsizetype kMaximumOperationComponentCharacters = 240;
        QString base = originalInfo.completeBaseName();
        base.truncate(std::max<qsizetype>(1,
            kMaximumOperationComponentCharacters - tail.size()));
        if (base.isEmpty()) base = QStringLiteral("snapshot");
        return originalInfo.dir().filePath(base + tail);
    };

    if (expectedMetadata)
    {
        // A cached history/family row can outlive another process's atomic
        // replacement of the same canonical filename. Move the exact current
        // directory entry aside first, then authenticate that object before
        // deleting it. A mismatching/newer replacement is restored (or kept
        // under a visible recovery suffix if another writer already recreated
        // the canonical path), so compare/delete has no TOCTOU data-loss gap.
        const QString quarantine = uniqueSiblingPath(
            QStringLiteral("deleting"), false);
        if (!QFile::rename(qtPath, quarantine))
        {
            if (error)
            {
                *error = QStringLiteral(
                    "Could not isolate the selected recovery snapshot before deletion: %1")
                    .arg(qtPath);
            }
            return false;
        }

        QString descriptorError{};
        const auto isolated = readRecoveryDescriptor(
            std::filesystem::path(quarantine.toStdWString()),
            &descriptorError);
        if (!isolated
            || !recoveryMetadataEquivalent(
                isolated->metadata, *expectedMetadata))
        {
            QString restoredPath = qtPath;
            if (QFileInfo::exists(restoredPath))
            {
                restoredPath = uniqueSiblingPath(
                    QStringLiteral("preserved"), true);
            }
            const bool restored = QFile::rename(quarantine, restoredPath);
            if (error)
            {
                *error = restored
                    ? QStringLiteral(
                        "The recovery snapshot changed after it was listed; the newer/different file was preserved at %1.")
                        .arg(QDir::toNativeSeparators(restoredPath))
                    : QStringLiteral(
                        "The recovery snapshot changed after it was listed and could not be restored from %1; it was not deleted. %2")
                        .arg(QDir::toNativeSeparators(quarantine),
                            descriptorError);
            }
            return false;
        }
        if (!QFile::remove(quarantine) || QFileInfo::exists(quarantine))
        {
            QString retainedPath = qtPath;
            if (QFileInfo::exists(retainedPath))
            {
                retainedPath = uniqueSiblingPath(
                    QStringLiteral("preserved"), true);
            }
            (void)QFile::rename(quarantine, retainedPath);
            if (error)
            {
                *error = QStringLiteral(
                    "Could not remove the isolated recovery snapshot; its bytes were retained at %1.")
                    .arg(QDir::toNativeSeparators(
                        QFileInfo::exists(retainedPath)
                            ? retainedPath : quarantine));
            }
            return false;
        }
        return true;
    }

    if (!QFile::remove(qtPath) || QFileInfo::exists(qtPath))
    {
        if (error)
        {
            *error = QStringLiteral("Could not remove recovery snapshot: %1")
                .arg(qtPath);
        }
        return false;
    }
    return true;
}

} // namespace sed::ui::recovery
