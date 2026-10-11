#include "test_common.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <QByteArray>
#include <QCryptographicHash>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QDataStream>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryDir>

#include "sed/assets/sed.hpp"
#include "sed/assets/jkl.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/parse_frame.hpp"
#include "sed/level/cleave.hpp"
#include "sed/level/build.hpp"
#include "sed/level/thing.hpp"
#include "ui/project/project_startup_utils.hpp"
#include "ui/recovery/document_recovery_utils.hpp"
#include "ui/recovery/partial_level_salvage.hpp"

namespace sed::ui::recovery::detail
{

std::size_t maximumRecoveryDirectoryEntriesScannedForTesting() noexcept;

std::array<quint64, 4> exerciseNativeFallbackBudgetForTesting(
    quint64 inputBytes,
    std::span<const quint64> requests);

bool decodeRecoveryHistoryWithRetainedBudgetForTesting(
    const QByteArray& payload,
    ProjectType projectType,
    const Level& currentLevel,
    quint64 maximumRetainedLevelBytes,
    quint64 maximumRetainedLevelItems,
    QString* error);

bool decodeRecoveryHistoryWithReconstructionBudgetForTesting(
    const QByteArray& payload,
    ProjectType projectType,
    const Level& currentLevel,
    quint64 maximumReconstructionBytes,
    QString* error);

bool validateGeneratedRecoveryHistoryWithRetainedBudgetForTesting(
    const QByteArray& payload,
    ProjectType projectType,
    const Level& currentLevel,
    std::size_t expectedUndoEntries,
    std::size_t expectedRedoEntries,
    quint64 maximumRetainedLevelBytes,
    quint64 maximumRetainedLevelItems,
    QString* error);

std::optional<QByteArray> readRecoveryBytesBoundedForTesting(
    qint64 expectedSize,
    const std::function<qint64(char*, qint64)>& reader,
    QString* error);

std::optional<RecoveryRecord> readRecoveryRecordWithNativeDecodeCountForTesting(
    const std::filesystem::path& recoveryPath,
    std::uint64_t* nativeDocumentDecodeCount,
    QString* error);

std::optional<RecoveredDocument>
readAndLoadRecoveryDocumentWithNativeDecodeCountForTesting(
    const std::filesystem::path& recoveryPath,
    std::uint64_t* nativeDocumentDecodeCount,
    QString* error);

std::vector<qint64> canonicalHistoryBaseReferencesForTesting(
    const Level& currentLevel);

} // namespace sed::ui::recovery::detail

namespace sed::app_tests
{

namespace
{

constexpr qsizetype kOuterChunkHeaderBytes = 68;

// Keep the long-standing recovery unit-test payload intentionally small.  It
// is an opaque deterministic payload used by the publication/recovery tests;
// changing it would broaden the fixture contract of those tests.
bool exactRecoveryText(const QString& actual, const QString& expected)
{
    if (actual == expected) return true;
    std::cerr << "Recovery string mismatch\nexpected: " << expected.toStdString()
              << "\nactual: " << actual.toStdString() << '\n';
    return false;
}

QByteArray editorFixture()
{
    QCborMap map{};
    map.insert(QStringLiteral("version"), 1);
    map.insert(QStringLiteral("selection"), QCborMap{});
    map.insert(QStringLiteral("padding"), QByteArray(256, 'x'));
    return ui::recovery::serializeCanonicalCbor(QCborValue(map));
}

// This separate deterministic EDIT payload is used only by the native app
// HIST golden. The app test verifies it at the native SED/recovery boundary
// (exact bytes survive repair); it does not claim to exercise the private
// MainWindow production validator.
QByteArray appHistoryEditorFixture()
{
    // This is deliberately opaque to the native codec.  It carries a stable
    // marker and a pretend unsaved COG buffer so the FEC golden proves exact
    // EDIT-byte preservation without duplicating MainWindow's private EDIT
    // schema in a lower-level recovery test.
    QCborMap root{};
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("fixture"),
        QStringLiteral("native-history-fec-boundary"));
    root.insert(QStringLiteral("unsavedCogSource"),
        QByteArrayLiteral("symbols startup\nreturn;\nend"));
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

ui::recovery::WriteRequest makeRequest(
    const std::filesystem::path& path,
    const std::shared_ptr<ui::recovery::PublicationGate>& gate,
    const std::uint64_t generation,
    const ProjectType kind = ProjectType::MOTS,
    const int sectorCount = 2,
    const bool withHistory = false)
{
    ui::recovery::WriteRequest request{};
    request.recoveryPath = path;
    request.publicationGate = gate;
    request.generation = generation;
    request.metadata.identity = QStringLiteral(
        "untitled:document-recovery-test-session");
    request.metadata.sessionId = QStringLiteral("document-recovery-test-session");
    request.metadata.displayName = QStringLiteral("Document Recovery Test");
    request.metadata.timestamp = QDateTime::currentMSecsSinceEpoch();
    request.metadata.sourceModifiedAtMs = -1;
    request.metadata.undoSignature = generation * 17;
    request.metadata.requiresUserRecovery = true;
    request.metadata.projectType = kind;
    request.editorStatePayload = editorFixture();

    editor::DocumentState state{};
    // Build the same internally coherent Level that New Project would.  The
    // unknown-project recovery fixtures deliberately destroy HEAD and then
    // reconstruct it from a user choice; starting from those same defaults
    // keeps the recovered authoritative Level byte-identical to HIST's base.
    ui::project::applyNewProjectTypeDefaults(state.level, kind);
    state.level.masterCMP = "dflt.cmp";
    for (int i = 0; i < sectorCount; ++i)
    {
        Sector sector{};
        sector.id = i + 1;
        sector.flags = 0x100 + i;
        sector.ambient = ColorF{ 0.17f, 0.29f, 0.43f, 0.61f };
        state.level.sectors.push_back(std::move(sector));
    }
    request.document.clear(std::move(state));
    if (withHistory)
    {
        request.document.pushUndo("history fixture");
    }
    return request;
}

bool writeBytes(const std::filesystem::path& path, const QByteArray& bytes)
{
    QFile file(QString::fromStdWString(path.wstring()));
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(bytes) == bytes.size();
}

QByteArray descriptorOnlyRecoveryBytes(
    const QString& sessionId,
    const qint64 timestamp,
    const std::function<void(QJsonObject&)>& mutate = {})
{
    QJsonObject metadata{};
    metadata.insert(QStringLiteral("version"), 1);
    metadata.insert(QStringLiteral("identity"),
        QStringLiteral("untitled:%1").arg(sessionId));
    metadata.insert(QStringLiteral("sessionId"), sessionId);
    metadata.insert(QStringLiteral("sourcePath"), QString{});
    metadata.insert(QStringLiteral("displayName"), QStringLiteral("Flood fixture"));
    metadata.insert(QStringLiteral("timestamp"), QString::number(timestamp));
    metadata.insert(QStringLiteral("sourceModifiedAtMs"), QStringLiteral("-1"));
    metadata.insert(QStringLiteral("undoSignature"), QStringLiteral("0"));
    metadata.insert(QStringLiteral("requiresUserRecovery"), true);
    metadata.insert(QStringLiteral("projectType"), static_cast<int>(ProjectType::MOTS));
    metadata.insert(QStringLiteral("levelSha256"), QString(64, QLatin1Char('0')));
    metadata.insert(QStringLiteral("documentSha256"), QString(64, QLatin1Char('0')));
    metadata.insert(QStringLiteral("payloadFormat"),
        QStringLiteral("sed-v1-document-recovery"));
    if (mutate) mutate(metadata);
    const QByteArray header = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    return QByteArrayLiteral("sed_recovery_v1\n")
        + QByteArray::number(header.size()) + '\n'
        + QCryptographicHash::hash(header, QCryptographicHash::Sha256).toHex()
        + '\n' + header;
}

bool setFileModificationTime(
    const std::filesystem::path& path,
    const qint64 modifiedAtMs)
{
    QFile file(QString::fromStdWString(path.wstring()));
    return file.open(QIODevice::ReadWrite)
        && file.setFileTime(
            QDateTime::fromMSecsSinceEpoch(modifiedAtMs, Qt::UTC),
            QFileDevice::FileModificationTime);
}

bool readBytes(const std::filesystem::path& path, QByteArray* out)
{
    QFile file(QString::fromStdWString(path.wstring()));
    if (!out || !file.open(QIODevice::ReadOnly)) return false;
    *out = file.readAll();
    return true;
}

bool rewritePrimaryRecoveryMetadata(
    const std::filesystem::path& path,
    const std::function<void(QJsonObject&)>& mutate)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes) || !mutate) return false;
    const qsizetype magicEnd = bytes.indexOf('\n');
    const qsizetype sizeEnd = bytes.indexOf('\n', magicEnd + 1);
    const qsizetype hashEnd = bytes.indexOf('\n', sizeEnd + 1);
    if (magicEnd < 0 || sizeEnd < 0 || hashEnd < 0) return false;
    bool sizeOk = false;
    const qsizetype oldHeaderSize = bytes.mid(
        magicEnd + 1, sizeEnd - magicEnd - 1).toLongLong(&sizeOk);
    const qsizetype oldHeaderOffset = hashEnd + 1;
    if (!sizeOk || oldHeaderSize <= 0
        || oldHeaderSize > bytes.size() - oldHeaderOffset)
    {
        return false;
    }
    QJsonParseError parseError{};
    QJsonDocument document = QJsonDocument::fromJson(
        bytes.mid(oldHeaderOffset, oldHeaderSize), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        return false;
    }
    QJsonObject object = document.object();
    mutate(object);
    const QByteArray header = QJsonDocument(object).toJson(QJsonDocument::Compact);
    const QByteArray prefix = QByteArrayLiteral("sed_recovery_v1\n")
        + QByteArray::number(header.size()) + '\n'
        + QCryptographicHash::hash(header, QCryptographicHash::Sha256).toHex()
        + '\n';
    return writeBytes(path, prefix + header
        + bytes.mid(oldHeaderOffset + oldHeaderSize));
}

bool damagePrimaryRecoveryMetadata(const std::filesystem::path& path)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes)) return false;
    qsizetype cursor = 0;
    for (int line = 0; line < 3; ++line)
    {
        cursor = bytes.indexOf('\n', cursor);
        if (cursor < 0) return false;
        ++cursor;
    }
    if (cursor >= bytes.size()) return false;
    bytes[cursor] = static_cast<char>(bytes.at(cursor) ^ 0x20);
    return writeBytes(path, bytes);
}

bool uppercasePrimaryRecoveryHash(const std::filesystem::path& path)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes)) return false;
    const qsizetype magicEnd = bytes.indexOf('\n');
    const qsizetype sizeEnd = bytes.indexOf('\n', magicEnd + 1);
    const qsizetype hashEnd = bytes.indexOf('\n', sizeEnd + 1);
    if (magicEnd < 0 || sizeEnd < 0 || hashEnd < 0
        || hashEnd - sizeEnd - 1 != 64)
    {
        return false;
    }
    bool changed = false;
    for (qsizetype cursor = sizeEnd + 1; cursor < hashEnd; ++cursor)
    {
        const char value = bytes.at(cursor);
        if (value >= 'a' && value <= 'f')
        {
            bytes[cursor] = static_cast<char>(value - ('a' - 'A'));
            changed = true;
        }
    }
    return changed && writeBytes(path, bytes);
}

struct OuterChunk
{
    QByteArray type{};
    quint32 flags{};
    quint64 storedSize{};
    quint64 rawSize{};
    quint32 crc{};
    QByteArray sha256{};
    qsizetype payloadOffset{};
};

// Application-owned SED V1 HIST fixture.  The sed_core vectors exercise the
// opaque native FEC boundary; this fixture additionally carries the actual
// editor HIST CBOR and EDIT payload used by the recovery application.
struct AppSedSectionLayout final
{
    qsizetype descriptorOffset{};
    qsizetype payloadOffset{};
    quint64 storedSize{};
    quint64 logicalStoredSize{};
    quint32 dataStreams{};
    quint32 parityStreams{};
    quint32 chunkPayloadSize{};
    quint32 stripeCount{};
    quint32 codedOffset{};
};

std::optional<AppSedSectionLayout> findAppSedSection(
    const QByteArray& bytes,
    const QByteArray& requestedType)
{
    constexpr qsizetype headerBytes = 28;
    constexpr qsizetype descriptorBytes = 100;
    constexpr qsizetype trailerBytes = 116;
    if (bytes.size() < headerBytes || requestedType.size() != 4)
    {
        return std::nullopt;
    }

    QDataStream header(bytes);
    header.setByteOrder(QDataStream::LittleEndian);
    char magic[4]{};
    quint32 version{}, project{}, sectionCount{};
    qint64 timestamp{};
    quint32 checksum{};
    header.readRawData(magic, 4);
    header >> version >> project >> sectionCount >> timestamp >> checksum;
    (void)version;
    (void)project;
    (void)timestamp;
    (void)checksum;
    if (header.status() != QDataStream::Ok
        || QByteArray(magic, 4) != QByteArray("sed\0", 4)
        || sectionCount == 0 || sectionCount > 1024)
    {
        return std::nullopt;
    }

    qsizetype cursor = headerBytes;
    for (quint32 index = 0; index < sectionCount; ++index)
    {
        if (cursor < 0 || cursor > bytes.size()
            || descriptorBytes > bytes.size() - cursor)
        {
            return std::nullopt;
        }
        const QByteArray descriptor = bytes.mid(cursor, descriptorBytes);
        const QByteArray type = descriptor.mid(8, 4);
        QDataStream fields(descriptor);
        fields.setByteOrder(QDataStream::LittleEndian);
        fields.skipRawData(28);
        quint64 storedSize{}, logicalStoredSize{};
        quint32 dataStreams{}, parityStreams{}, chunkPayloadSize{}, stripeCount{}, codedOffset{};
        fields >> storedSize >> logicalStoredSize >> dataStreams
               >> parityStreams >> chunkPayloadSize >> stripeCount >> codedOffset;
        if (fields.status() != QDataStream::Ok
            || storedSize > static_cast<quint64>(bytes.size() - cursor - descriptorBytes)
            || descriptorBytes + static_cast<qsizetype>(storedSize)
                + trailerBytes > bytes.size() - cursor)
        {
            return std::nullopt;
        }
        if (type == requestedType)
        {
            return AppSedSectionLayout{
                cursor,
                cursor + descriptorBytes,
                storedSize,
                logicalStoredSize,
                dataStreams,
                parityStreams,
                chunkPayloadSize,
                stripeCount,
                codedOffset };
        }
        cursor += descriptorBytes + static_cast<qsizetype>(storedSize)
            + trailerBytes;
    }
    return std::nullopt;
}

QByteArray damageAppHistorySlots(
    QByteArray bytes,
    const AppSedSectionLayout& section,
    const std::size_t damageCount)
{
    constexpr qsizetype slotBytes = 4096;
    constexpr qsizetype slotPayloadBytes = slotBytes - 4;
    if (damageCount == 0 || damageCount > section.dataStreams + section.parityStreams
        || section.parityStreams != 2 || section.chunkPayloadSize != slotPayloadBytes
        || section.stripeCount == 0)
    {
        return {};
    }
    for (std::size_t stripe = 0; stripe < section.stripeCount; ++stripe)
    {
        for (std::size_t stream = 0; stream < damageCount; ++stream)
        {
            // Deliberately corrupt the checked payload, not its CRC field.
            // The native decoder must discover the erasure from the on-wire
            // CRC and reconstruct it independently in every physical stripe.
            const qsizetype offset = section.payloadOffset
                + static_cast<qsizetype>(section.codedOffset)
                + static_cast<qsizetype>(
                    (stream * section.stripeCount + stripe) * slotBytes)
                + static_cast<qsizetype>(7 + stream);
            if (offset < 0 || offset >= bytes.size()) return {};
            bytes[offset] = static_cast<char>(bytes.at(offset)
                ^ static_cast<char>(0x4d + stream + stripe));
        }
    }
    return bytes;
}

std::vector<std::byte> appSedBytes(const QByteArray& bytes)
{
    const auto* const data = reinterpret_cast<const std::byte*>(bytes.constData());
    return std::vector<std::byte>(data, data + bytes.size());
}

const assets::SedOptionalSection* findAppOptionalSection(
    const std::vector<assets::SedOptionalSection>& sections,
    const std::array<char, 4>& type)
{
    const auto found = std::find_if(sections.begin(), sections.end(),
        [&type](const assets::SedOptionalSection& section)
        {
            return section.type == type;
        });
    return found == sections.end() ? nullptr : &*found;
}

std::optional<QByteArray> readNativeOptionalPayload(
    const QByteArray& nativePayload,
    const std::array<char, 4>& type)
{
    try
    {
        const assets::SedFile nativeFile = assets::loadSedFileBytes(std::as_bytes(
            std::span(nativePayload.constData(),
                static_cast<std::size_t>(nativePayload.size()))));
        const assets::SedOptionalSection* const section = findAppOptionalSection(
            nativeFile.optionalSections, type);
        if ( section == nullptr )
        {
            return std::nullopt;
        }
        return QByteArray(
            reinterpret_cast<const char*>(section->payload.data()),
            static_cast<qsizetype>(section->payload.size()));
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::filesystem::path appHistoryGoldenPath()
{
    return std::filesystem::path(__FILE__).parent_path()
        / "assets" / "sed_v1_hist_document.sed";
}

bool writeAppHistoryGoldenFiles(
    const QByteArray& base,
    const AppSedSectionLayout& history)
{
    const std::filesystem::path basePath = appHistoryGoldenPath();
    std::error_code error{};
    std::filesystem::create_directories(basePath.parent_path(), error);
    if (error) return false;
    if (!writeBytes(basePath, base)) return false;
    for (std::size_t count = 1; count <= 3; ++count)
    {
        const QByteArray damaged = damageAppHistorySlots(base, history, count);
        if (damaged.isEmpty()) return false;
        const std::filesystem::path path = basePath.parent_path()
            / (QStringLiteral("sed_v1_hist_document_%1_damage.sed")
                .arg(static_cast<qulonglong>(count))).toStdString();
        if (!writeBytes(path, damaged)) return false;
    }
    return true;
}

std::optional<OuterChunk> findOuterChunk(const QByteArray& bytes, const QByteArray& type)
{
    const qsizetype typeOffset = bytes.indexOf(type);
    if (typeOffset < 8 || typeOffset + 4 + (kOuterChunkHeaderBytes - 12) > bytes.size())
        return std::nullopt;
    const qsizetype headerOffset = typeOffset - 8;
    QDataStream stream(bytes.mid(headerOffset, kOuterChunkHeaderBytes));
    stream.setByteOrder(QDataStream::LittleEndian);
    char magic[8]{}, chunkType[4]{};
    OuterChunk chunk{};
    stream.readRawData(magic, 8);
    stream.readRawData(chunkType, 4);
    stream >> chunk.flags >> chunk.storedSize >> chunk.rawSize;
    char sha[32]{};
    stream >> chunk.crc;
    stream.readRawData(sha, 32);
    chunk.sha256 = QByteArray(sha, 32);
    chunk.type = QByteArray(chunkType, 4);
    chunk.payloadOffset = headerOffset + kOuterChunkHeaderBytes;
    if (stream.status() != QDataStream::Ok
        || QByteArray(magic, 8) != QByteArrayLiteral("sedchn1\n")
        || chunk.type != type
        || chunk.storedSize > static_cast<quint64>(bytes.size() - chunk.payloadOffset))
        return std::nullopt;
    return chunk;
}

// Independent RFC 4648 bit-index oracle for content-dependent filename fields.
// The family token itself is a fixed Python hashlib/base64 vector below.
QString fixedBase32(const QByteArray& bytes)
{
    constexpr char alphabet[] = "abcdefghijklmnopqrstuvwxyz234567";
    QString result;
    for (qsizetype bit = 0; bit < bytes.size() * 8; bit += 5)
    {
        unsigned value = 0;
        for (qsizetype offset = 0; offset < 5; ++offset)
        {
            value <<= 1;
            const qsizetype index = bit + offset;
            if (index < bytes.size() * 8)
                value |= (static_cast<unsigned char>(bytes.at(index / 8)) >> (7 - index % 8)) & 1u;
        }
        result += QLatin1Char(alphabet[value]);
    }
    return result;
}

QByteArray fixtureDocumentDigest(const QByteArray& container)
{
    const auto chunk = findOuterChunk(container, QByteArrayLiteral("docm"));
    if (!chunk || chunk->storedSize != chunk->rawSize) return {};
    return QCryptographicHash::hash(container.mid(chunk->payloadOffset,
        static_cast<qsizetype>(chunk->storedSize)), QCryptographicHash::Sha256);
}

std::vector<OuterChunk> findOuterChunks(const QByteArray& bytes, const QByteArray& type)
{
    std::vector<OuterChunk> chunks{};
    qsizetype cursor = 0;
    while ((cursor = bytes.indexOf(QByteArrayLiteral("sedchn1\n"), cursor)) >= 0)
    {
        if (cursor + kOuterChunkHeaderBytes > bytes.size()) break;
        QDataStream stream(bytes.mid(cursor, kOuterChunkHeaderBytes));
        stream.setByteOrder(QDataStream::LittleEndian);
        char magic[8]{}, chunkType[4]{}, sha[32]{};
        OuterChunk chunk{};
        quint32 flags{}, crc{};
        quint64 storedSize{}, rawSize{};
        stream.readRawData(magic, 8);
        stream.readRawData(chunkType, 4);
        stream >> flags >> storedSize >> rawSize >> crc;
        stream.readRawData(sha, 32);
        chunk.type = QByteArray(chunkType, 4);
        chunk.flags = flags;
        chunk.storedSize = storedSize;
        chunk.rawSize = rawSize;
        chunk.crc = crc;
        chunk.sha256 = QByteArray(sha, 32);
        chunk.payloadOffset = cursor + kOuterChunkHeaderBytes;
        if (stream.status() == QDataStream::Ok
            && QByteArray(magic, 8) == QByteArrayLiteral("sedchn1\n")
            && chunk.type == type
            && storedSize <= static_cast<quint64>(bytes.size() - chunk.payloadOffset))
        {
            chunks.push_back(chunk);
            cursor = chunk.payloadOffset + static_cast<qsizetype>(storedSize);
        }
        else
        {
            ++cursor;
        }
    }
    return chunks;
}

bool appendOuterChunkCopy(
    const std::filesystem::path& destination,
    const std::filesystem::path& source,
    const QByteArray& type)
{
    QByteArray destinationBytes{}, sourceBytes{};
    if (!readBytes(destination, &destinationBytes)
        || !readBytes(source, &sourceBytes)) return false;
    const auto sourceChunk = findOuterChunk(sourceBytes, type);
    if (!sourceChunk) return false;
    const qsizetype sourceHeader = sourceChunk->payloadOffset - kOuterChunkHeaderBytes;
    const qsizetype sourceSize = kOuterChunkHeaderBytes
        + static_cast<qsizetype>(sourceChunk->storedSize);
    if (sourceHeader < 0 || sourceHeader + sourceSize > sourceBytes.size()) return false;
    destinationBytes.append(sourceBytes.mid(sourceHeader, sourceSize));
    return writeBytes(destination, destinationBytes);
}

bool prependInvalidDocumentChunk(const std::filesystem::path& path)
{
    QByteArray original{};
    if (!readBytes(path, &original)) return false;
    QByteArray decoy{};
    QDataStream stream(&decoy, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("sedchn1\n", 8);
    stream.writeRawData("docm", 4);
    // Canonical native framing with a bounded one-byte payload. The payload
    // is intentionally not a SED document, so strict and partial native
    // decoding must reject it without changing the parser's accepted state.
    stream << quint32{4} << quint64{1} << quint64{1} << quint32{0};
    const QByteArray zeroHash(32, '\0');
    stream.writeRawData(zeroHash.constData(), zeroHash.size());
    if (stream.status() != QDataStream::Ok
        || decoy.size() != kOuterChunkHeaderBytes)
    {
        return false;
    }
    decoy.append('x');
    return writeBytes(path, decoy + original);
}

bool replaceOuterChunkCopy(
    const std::filesystem::path& destination,
    const std::filesystem::path& source,
    const QByteArray& type)
{
    QByteArray destinationBytes{}, sourceBytes{};
    if (!readBytes(destination, &destinationBytes)
        || !readBytes(source, &sourceBytes)) return false;
    const auto destinationChunk = findOuterChunk(destinationBytes, type);
    const auto sourceChunk = findOuterChunk(sourceBytes, type);
    if (!destinationChunk || !sourceChunk) return false;
    const qsizetype destinationHeader = destinationChunk->payloadOffset - kOuterChunkHeaderBytes;
    const qsizetype destinationSize = kOuterChunkHeaderBytes
        + static_cast<qsizetype>(destinationChunk->storedSize);
    const qsizetype sourceHeader = sourceChunk->payloadOffset - kOuterChunkHeaderBytes;
    const qsizetype sourceSize = kOuterChunkHeaderBytes
        + static_cast<qsizetype>(sourceChunk->storedSize);
    if (destinationHeader < 0 || sourceHeader < 0
        || destinationHeader + destinationSize > destinationBytes.size()
        || sourceHeader + sourceSize > sourceBytes.size()) return false;
    destinationBytes.replace(destinationHeader, destinationSize,
        sourceBytes.mid(sourceHeader, sourceSize));
    return writeBytes(destination, destinationBytes);
}

quint32 testCrc32(const QByteArray& bytes)
{
    quint32 crc = 0xffff'ffffu;
    for (const unsigned char byte : bytes)
    {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc >> 1u) ^ (0xedb8'8320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

void writeLe32(QByteArray& bytes, const qsizetype offset, const quint32 value)
{
    for (int byte = 0; byte < 4; ++byte)
    {
        bytes[offset + byte] = static_cast<char>((value >> (8 * byte)) & 0xffu);
    }
}

void writeLe64(QByteArray& bytes, const qsizetype offset, const quint64 value)
{
    for (int byte = 0; byte < 8; ++byte)
    {
        bytes[offset + byte] = static_cast<char>((value >> (8 * byte)) & 0xffu);
    }
}

unsigned char testGfMultiply(unsigned char lhs, unsigned char rhs)
{
    unsigned char result{};
    while (rhs != 0)
    {
        if ((rhs & 1u) != 0)
        {
            result ^= lhs;
        }
        rhs >>= 1u;
        const bool highBit = (lhs & 0x80u) != 0;
        lhs = static_cast<unsigned char>(lhs << 1u);
        if (highBit)
        {
            lhs ^= 0x1du;
        }
    }
    return result;
}

unsigned char testGfCoefficient(const quint32 shard)
{
    unsigned char result{ 1 };
    for (quint32 index = 0; index < shard; ++index)
    {
        result = testGfMultiply(result, 2);
    }
    return result;
}

QByteArray makeProtectedMetaChunkForTest(const QByteArray& raw)
{
    if (raw.isEmpty()) return {};
    const QByteArray compressed = qCompress(raw, 6);
    if (compressed.isEmpty()) return {};
    const quint32 dataShards = std::min<quint32>(
        16u, std::max<quint32>(1u, static_cast<quint32>(compressed.size())));
    const quint32 shardSize = static_cast<quint32>(
        (compressed.size() + dataShards - 1) / dataShards);
    std::vector<QByteArray> shards(
        static_cast<std::size_t>(dataShards + 2u),
        QByteArray(static_cast<qsizetype>(shardSize), '\0'));
    for (quint32 index = 0; index < dataShards; ++index)
    {
        const qsizetype offset = static_cast<qsizetype>(index) * shardSize;
        const qsizetype count = std::max<qsizetype>(0,
            std::min<qsizetype>(shardSize, compressed.size() - offset));
        if (count > 0)
        {
            std::copy_n(compressed.constData() + offset, count,
                shards[static_cast<std::size_t>(index)].data());
        }
    }
    for (quint32 index = 0; index < dataShards; ++index)
    {
        const QByteArray& data = shards[static_cast<std::size_t>(index)];
        const unsigned char coefficient = testGfCoefficient(index);
        for (quint32 byte = 0; byte < shardSize; ++byte)
        {
            const unsigned char value = static_cast<unsigned char>(data.at(byte));
            QByteArray& parityP = shards[static_cast<std::size_t>(dataShards)];
            QByteArray& parityQ = shards[static_cast<std::size_t>(dataShards + 1u)];
            parityP[byte] = static_cast<char>(
                static_cast<unsigned char>(parityP.at(byte)) ^ value);
            parityQ[byte] = static_cast<char>(
                static_cast<unsigned char>(parityQ.at(byte))
                ^ testGfMultiply(coefficient, value));
        }
    }

    const QByteArray rawHash = QCryptographicHash::hash(
        raw, QCryptographicHash::Sha256);
    QByteArray table{};
    QDataStream tableStream(&table, QIODevice::WriteOnly);
    tableStream.setByteOrder(QDataStream::LittleEndian);
    tableStream.writeRawData("sedfec1\n", 8);
    tableStream << quint32{1} << quint32{1}
                << static_cast<quint64>(raw.size());
    tableStream.writeRawData(rawHash.constData(), rawHash.size());
    tableStream << static_cast<quint64>(raw.size())
                << static_cast<quint64>(compressed.size())
                << dataShards << shardSize;
    tableStream.writeRawData(rawHash.constData(), rawHash.size());
    for (const QByteArray& shard : shards)
    {
        tableStream << testCrc32(shard);
    }
    if (tableStream.status() != QDataStream::Ok) return {};

    QByteArray protectedPayload{};
    QDataStream prefix(&protectedPayload, QIODevice::WriteOnly);
    prefix.setByteOrder(QDataStream::LittleEndian);
    prefix << static_cast<quint64>(table.size());
    protectedPayload.append(table);
    QDataStream primaryCrc(&protectedPayload, QIODevice::Append);
    primaryCrc.setByteOrder(QDataStream::LittleEndian);
    primaryCrc << testCrc32(table);
    for (const QByteArray& shard : shards)
    {
        protectedPayload.append(shard);
    }
    protectedPayload.append(table);
    QDataStream footer(&protectedPayload, QIODevice::Append);
    footer.setByteOrder(QDataStream::LittleEndian);
    footer << testCrc32(table) << static_cast<quint64>(table.size());
    footer.writeRawData("fecend1\n", 8);
    if (prefix.status() != QDataStream::Ok
        || primaryCrc.status() != QDataStream::Ok
        || footer.status() != QDataStream::Ok)
    {
        return {};
    }

    QByteArray chunk{};
    QDataStream chunkHeader(&chunk, QIODevice::WriteOnly);
    chunkHeader.setByteOrder(QDataStream::LittleEndian);
    chunkHeader.writeRawData("sedchn1\n", 8);
    chunkHeader.writeRawData("meta", 4);
    chunkHeader << quint32{2}
                << static_cast<quint64>(protectedPayload.size())
                << static_cast<quint64>(raw.size())
                << testCrc32(protectedPayload);
    chunkHeader.writeRawData(rawHash.constData(), rawHash.size());
    if (chunkHeader.status() != QDataStream::Ok
        || chunk.size() != kOuterChunkHeaderBytes)
    {
        return {};
    }
    chunk.append(protectedPayload);
    return chunk;
}

bool rewriteRecoveryMetadataCopies(
    const std::filesystem::path& path,
    const std::function<void(QJsonObject&)>& mutate)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes) || !mutate) return false;
    const qsizetype magicEnd = bytes.indexOf('\n');
    const qsizetype sizeEnd = bytes.indexOf('\n', magicEnd + 1);
    const qsizetype hashEnd = bytes.indexOf('\n', sizeEnd + 1);
    if (magicEnd < 0 || sizeEnd < 0 || hashEnd < 0) return false;
    bool sizeOk = false;
    const qsizetype oldHeaderSize = bytes.mid(
        magicEnd + 1, sizeEnd - magicEnd - 1).toLongLong(&sizeOk);
    const qsizetype oldHeaderOffset = hashEnd + 1;
    if (!sizeOk || oldHeaderSize <= 0
        || oldHeaderSize > bytes.size() - oldHeaderOffset)
    {
        return false;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(
        bytes.mid(oldHeaderOffset, oldHeaderSize), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        return false;
    }
    QJsonObject metadata = document.object();
    mutate(metadata);
    const QByteArray header = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    const QByteArray protectedChunk = makeProtectedMetaChunkForTest(header);
    if (protectedChunk.isEmpty()) return false;

    QByteArray chunks = bytes.mid(oldHeaderOffset + oldHeaderSize);
    const auto oldMeta = findOuterChunk(chunks, QByteArrayLiteral("meta"));
    if (!oldMeta) return false;
    const qsizetype oldMetaHeaderOffset = oldMeta->payloadOffset
        - kOuterChunkHeaderBytes;
    const qsizetype oldMetaSize = kOuterChunkHeaderBytes
        + static_cast<qsizetype>(oldMeta->storedSize);
    if (oldMetaHeaderOffset < 0
        || oldMetaHeaderOffset + oldMetaSize > chunks.size())
    {
        return false;
    }
    chunks.replace(oldMetaHeaderOffset, oldMetaSize, protectedChunk);

    const QByteArray prefix = QByteArrayLiteral("sed_recovery_v1\n")
        + QByteArray::number(header.size()) + '\n'
        + QCryptographicHash::hash(header, QCryptographicHash::Sha256).toHex()
        + '\n';
    return writeBytes(path, prefix + header + chunks);
}

bool makeCrossBoundMetaTables(const std::filesystem::path& path)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes)) return false;
    const auto meta = findOuterChunk(bytes, QByteArrayLiteral("meta"));
    if (!meta || meta->flags != 2u || meta->storedSize < 12) return false;

    const qsizetype payloadOffset = meta->payloadOffset;
    QDataStream prefix(bytes.mid(payloadOffset, 8));
    prefix.setByteOrder(QDataStream::LittleEndian);
    quint64 originalTableSize{};
    prefix >> originalTableSize;
    if (prefix.status() != QDataStream::Ok || originalTableSize < 120
        || originalTableSize > meta->storedSize - 12) return false;
    const QByteArray originalTable = bytes.mid(
        payloadOffset + 8, static_cast<qsizetype>(originalTableSize));
    if (originalTable.mid(0, 8) != QByteArrayLiteral("sedfec1\n")) return false;

    QDataStream descriptor(originalTable.mid(56));
    descriptor.setByteOrder(QDataStream::LittleEndian);
    quint64 rawSize{}, storedSize{};
    quint32 dataShards{}, shardSize{};
    descriptor >> rawSize >> storedSize >> dataShards >> shardSize;
    if (descriptor.status() != QDataStream::Ok || rawSize == 0 || storedSize == 0
        || dataShards == 0 || dataShards > 16 || shardSize == 0) return false;
    const qsizetype originalDescriptorSize = 56
        + static_cast<qsizetype>(dataShards + 2u) * 4;
    if (56 + originalDescriptorSize != originalTable.size()) return false;

    const qsizetype originalShardBytes = static_cast<qsizetype>(
        dataShards + 2u) * static_cast<qsizetype>(shardSize);
    const qsizetype originalDataOffset = payloadOffset + 8
        + static_cast<qsizetype>(originalTableSize) + 4;
    if (originalDataOffset < 0 || originalShardBytes <= 0
        || originalShardBytes > bytes.size() - originalDataOffset) return false;
    const QByteArray originalShards = bytes.mid(
        originalDataOffset, originalShardBytes);
    QByteArray compressedMetadata{};
    for (quint32 shard = 0; shard < dataShards; ++shard)
    {
        compressedMetadata.append(originalShards.mid(
            static_cast<qsizetype>(shard) * shardSize, shardSize));
    }
    compressedMetadata.truncate(static_cast<qsizetype>(storedSize));
    const QByteArray metadata = qUncompress(compressedMetadata);
    if (metadata.size() != static_cast<qsizetype>(rawSize)) return false;

    const QByteArray suffix(" ", 1);
    const QByteArray suffixStored = qCompress(suffix, 6);
    if (suffixStored.isEmpty()) return false;
    const quint32 suffixDataShards = std::min<quint32>(
        16u, std::max<quint32>(1u, static_cast<quint32>(suffixStored.size())));
    const quint32 suffixShardSize = static_cast<quint32>(
        (suffixStored.size() + suffixDataShards - 1) / suffixDataShards);
    std::vector<QByteArray> suffixShards(
        static_cast<std::size_t>(suffixDataShards + 2u),
        QByteArray(static_cast<qsizetype>(suffixShardSize), '\0'));
    for (quint32 shard = 0; shard < suffixDataShards; ++shard)
    {
        const qsizetype sourceOffset = static_cast<qsizetype>(shard) * suffixShardSize;
        const qsizetype count = std::max<qsizetype>(0,
            std::min<qsizetype>(suffixShardSize, suffixStored.size() - sourceOffset));
        if (count > 0)
        {
            std::copy_n(suffixStored.constData() + sourceOffset, count,
                suffixShards[static_cast<std::size_t>(shard)].data());
        }
    }
    QByteArray suffixShardBytes{};
    for (const QByteArray& shard : suffixShards) suffixShardBytes.append(shard);

    const QByteArray combined = metadata + suffix;
    const QByteArray combinedHash = QCryptographicHash::hash(
        combined, QCryptographicHash::Sha256);
    const QByteArray suffixHash = QCryptographicHash::hash(
        suffix, QCryptographicHash::Sha256);
    const auto appendTableHeader = [&](QByteArray& table, const quint32 count)
    {
        QDataStream stream(&table, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream.writeRawData("sedfec1\n", 8);
        stream << quint32{1} << count
               << static_cast<quint64>(combined.size());
        stream.writeRawData(combinedHash.constData(), combinedHash.size());
        return stream.status() == QDataStream::Ok;
    };
    const auto appendDescriptor = [](QByteArray& table,
        const QByteArray& raw, const QByteArray& stored,
        const quint32 shardCount, const quint32 bytesPerShard,
        const std::vector<quint32>& crcs)
    {
        QDataStream stream(&table, QIODevice::Append);
        stream.setByteOrder(QDataStream::LittleEndian);
        const QByteArray hash = QCryptographicHash::hash(
            raw, QCryptographicHash::Sha256);
        stream << static_cast<quint64>(raw.size())
               << static_cast<quint64>(stored.size())
               << shardCount << bytesPerShard;
        stream.writeRawData(hash.constData(), hash.size());
        for (const quint32 crc : crcs) stream << crc;
        return stream.status() == QDataStream::Ok;
    };

    QByteArray primaryTable{};
    if (!appendTableHeader(primaryTable, 2)) return false;
    primaryTable.append(originalTable.mid(56));
    std::vector<quint32> suffixCrcs{};
    suffixCrcs.reserve(suffixShards.size());
    for (const QByteArray& shard : suffixShards)
    {
        suffixCrcs.push_back(testCrc32(shard));
    }
    if (!appendDescriptor(primaryTable, suffix, suffixStored,
            suffixDataShards, suffixShardSize, suffixCrcs)) return false;

    const QByteArray combinedStored = qCompress(combined, 6);
    if (combinedStored.isEmpty()) return false;
    const quint32 mirrorDataShards = std::min<quint32>(
        16u, std::max<quint32>(1u, static_cast<quint32>(combinedStored.size())));
    const quint32 mirrorShardSize = static_cast<quint32>(
        (combinedStored.size() + mirrorDataShards - 1) / mirrorDataShards);
    std::vector<quint32> falseCrcs(
        static_cast<std::size_t>(mirrorDataShards + 2u), 0x13579bdfu);
    QByteArray mirrorTable{};
    if (!appendTableHeader(mirrorTable, 1)
        || !appendDescriptor(mirrorTable, combined, combinedStored,
            mirrorDataShards, mirrorShardSize, falseCrcs)
        || mirrorTable.size() >= primaryTable.size()) return false;

    QByteArray protectedPayload{};
    QDataStream leading(&protectedPayload, QIODevice::WriteOnly);
    leading.setByteOrder(QDataStream::LittleEndian);
    leading << static_cast<quint64>(primaryTable.size());
    protectedPayload.append(primaryTable);
    QDataStream primaryCrc(&protectedPayload, QIODevice::Append);
    primaryCrc.setByteOrder(QDataStream::LittleEndian);
    primaryCrc << testCrc32(primaryTable);
    protectedPayload.append(originalShards);
    protectedPayload.append(suffixShardBytes);
    protectedPayload.append(QByteArray(
        primaryTable.size() - mirrorTable.size(), '\0'));
    protectedPayload.append(mirrorTable);
    QDataStream footer(&protectedPayload, QIODevice::Append);
    footer.setByteOrder(QDataStream::LittleEndian);
    footer << testCrc32(mirrorTable)
           << static_cast<quint64>(mirrorTable.size());
    footer.writeRawData("fecend1\n", 8);
    if (leading.status() != QDataStream::Ok
        || primaryCrc.status() != QDataStream::Ok
        || footer.status() != QDataStream::Ok) return false;

    const qsizetype headerOffset = payloadOffset - kOuterChunkHeaderBytes;
    bytes.replace(payloadOffset, static_cast<qsizetype>(meta->storedSize),
        protectedPayload);
    writeLe64(bytes, headerOffset + 16,
        static_cast<quint64>(protectedPayload.size()));
    writeLe64(bytes, headerOffset + 24,
        static_cast<quint64>(combined.size()));
    writeLe32(bytes, headerOffset + 32, testCrc32(protectedPayload));
    const QByteArray combinedDigest = QCryptographicHash::hash(
        combined, QCryptographicHash::Sha256);
    std::copy_n(combinedDigest.constData(), combinedDigest.size(),
        bytes.data() + headerOffset + 36);
    return writeBytes(path, bytes);
}

bool appendWarningChunk(
    const std::filesystem::path& path,
    const QString& warning)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes)) return false;
    const QByteArray raw = warning.toUtf8();
    if (raw.isEmpty()) return false;
    QByteArray chunk{};
    QDataStream stream(&chunk, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("sedchn1\n", 8);
    stream.writeRawData("warn", 4);
    stream << quint32{0}
           << static_cast<quint64>(raw.size())
           << static_cast<quint64>(raw.size())
           << testCrc32(raw);
    const QByteArray digest = QCryptographicHash::hash(
        raw, QCryptographicHash::Sha256);
    stream.writeRawData(digest.constData(), digest.size());
    if (stream.status() != QDataStream::Ok || chunk.size() != kOuterChunkHeaderBytes)
        return false;
    chunk.append(raw);
    bytes.append(chunk);
    return writeBytes(path, bytes);
}

bool appendOversizedCompressedWarning(const std::filesystem::path& path)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes)) return false;
    QByteArray stored = qCompress(QByteArrayLiteral("x"), 9);
    if (stored.size() < 4
        || assets::SedMaximumFileBytes > std::numeric_limits<quint32>::max())
    {
        return false;
    }
    const quint32 declaredRawSize = static_cast<quint32>(
        assets::SedMaximumFileBytes);
    stored[0] = static_cast<char>((declaredRawSize >> 24u) & 0xffu);
    stored[1] = static_cast<char>((declaredRawSize >> 16u) & 0xffu);
    stored[2] = static_cast<char>((declaredRawSize >> 8u) & 0xffu);
    stored[3] = static_cast<char>(declaredRawSize & 0xffu);

    QByteArray chunk{};
    QDataStream stream(&chunk, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("sedchn1\n", 8);
    stream.writeRawData("warn", 4);
    stream << quint32{1}
           << static_cast<quint64>(stored.size())
           << static_cast<quint64>(declaredRawSize)
           << testCrc32(stored);
    const QByteArray zeroHash(32, '\0');
    stream.writeRawData(zeroHash.constData(), zeroHash.size());
    if (stream.status() != QDataStream::Ok
        || chunk.size() != kOuterChunkHeaderBytes)
    {
        return false;
    }
    chunk.append(stored);
    bytes.append(chunk);
    return writeBytes(path, bytes);
}

bool hideOuterDocumentTypeAndAppendMarkerFlood(
    const std::filesystem::path& path,
    const int markerCount)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes) || markerCount <= 0) return false;
    const auto docm = findOuterChunk(bytes, QByteArrayLiteral("docm"));
    if (!docm || docm->payloadOffset < kOuterChunkHeaderBytes) return false;
    const qsizetype headerOffset = docm->payloadOffset - kOuterChunkHeaderBytes;
    bytes.replace(headerOffset + 8, 4, QByteArrayLiteral("XOCM"));

    // Each marker has a plausible bounded outer DOCM header so it exercises
    // both the damaged-header literal scan and the later outer-boundary path,
    // but its one-byte payload can never become accepted native content.
    QByteArray marker{};
    QDataStream stream(&marker, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("sedchn1\n", 8);
    stream.writeRawData("docm", 4);
    stream << quint32{4} << quint64{1} << quint64{1} << quint32{0};
    const QByteArray zeroHash(32, '\0');
    stream.writeRawData(zeroHash.constData(), zeroHash.size());
    if (stream.status() != QDataStream::Ok
        || marker.size() != kOuterChunkHeaderBytes)
    {
        return false;
    }
    marker.append('x');
    for (int i = 0; i < markerCount; ++i)
    {
        bytes.append(marker);
    }
    return writeBytes(path, bytes);
}

bool writeNoFooterSectionAnchorFlood(
    const std::filesystem::path& path,
    const int markerCount)
{
    if (markerCount <= 0) return false;
    QByteArray bytes{};
    bytes.reserve(markerCount * 4096);
    const QByteArray anchor("sectv1\n\0", 8);
    const QByteArray padding(4088, 'x');
    for (int i = 0; i < markerCount; ++i)
    {
        bytes.append(anchor);
        bytes.append(padding);
    }
    return !bytes.contains(QByteArrayLiteral("sedend\r\n"))
        && writeBytes(path, bytes);
}

bool poisonPrimaryMetaTable(const std::filesystem::path& path)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes)) return false;
    const auto meta = findOuterChunk(bytes, QByteArrayLiteral("meta"));
    if (!meta || (meta->flags & 2u) == 0) return false;
    const qsizetype payload = meta->payloadOffset;
    QDataStream prefix(bytes.mid(payload, 8));
    prefix.setByteOrder(QDataStream::LittleEndian);
    quint64 tableSize{};
    prefix >> tableSize;
    if (prefix.status() != QDataStream::Ok || meta->storedSize < 12
        || tableSize < 120
        || tableSize > static_cast<quint64>(meta->storedSize - 12)) return false;
    const qsizetype tableOffset = payload + 8;
    const qsizetype crcOffset = tableOffset + static_cast<qsizetype>(tableSize);
    if (crcOffset + 4 > payload + static_cast<qsizetype>(meta->storedSize)) return false;
    // Keep the table schema-valid but poison one shard CRC. The mirrored
    // footer table remains intact and is the uniquely valid decode.
    const qsizetype firstShardCrc = tableOffset + 56 + 8 + 8 + 4 + 4 + 32;
    if (firstShardCrc + 4 > crcOffset) return false;
    bytes[firstShardCrc] = static_cast<char>(bytes.at(firstShardCrc) ^ 0x51);
    const QByteArray primaryTable = bytes.mid(tableOffset, static_cast<qsizetype>(tableSize));
    const quint32 crc = testCrc32(primaryTable);
    for (int byte = 0; byte < 4; ++byte)
    {
        bytes[crcOffset + byte] = static_cast<char>((crc >> (8 * byte)) & 0xffu);
    }
    return writeBytes(path, bytes);
}

bool makePrimaryMetaTableMultiUnit(const std::filesystem::path& path)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes)) return false;
    const auto meta = findOuterChunk(bytes, QByteArrayLiteral("meta"));
    if (!meta || (meta->flags & 2u) == 0 || meta->storedSize < 12) return false;
    const qsizetype payload = meta->payloadOffset;
    QDataStream prefix(bytes.mid(payload, 8));
    prefix.setByteOrder(QDataStream::LittleEndian);
    quint64 tableSize{};
    prefix >> tableSize;
    if (prefix.status() != QDataStream::Ok || tableSize < 56
        || tableSize > static_cast<quint64>(meta->storedSize - 12)) return false;
    const qsizetype tableOffset = payload + 8;
    const qsizetype crcOffset = tableOffset + static_cast<qsizetype>(tableSize);
    if (crcOffset + 4 > payload + static_cast<qsizetype>(meta->storedSize)) return false;
    // Keep the outer table CRC valid while declaring a second descriptor. The
    // descriptor bytes are intentionally absent, so this is a CRC-valid but
    // schema-invalid primary candidate; the intact footer mirror must win.
    for (int byte = 0; byte < 4; ++byte)
    {
        bytes[tableOffset + 12 + byte] = static_cast<char>((2u >> (8 * byte)) & 0xffu);
    }
    const QByteArray primaryTable = bytes.mid(tableOffset, static_cast<qsizetype>(tableSize));
    const quint32 crc = testCrc32(primaryTable);
    for (int byte = 0; byte < 4; ++byte)
    {
        bytes[crcOffset + byte] = static_cast<char>((crc >> (8 * byte)) & 0xffu);
    }
    return writeBytes(path, bytes);
}

bool damageMetaHeaderAndPrimaryPrefix(const std::filesystem::path& path)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes)) return false;
    const auto meta = findOuterChunk(bytes, QByteArrayLiteral("meta"));
    if (!meta || meta->payloadOffset < kOuterChunkHeaderBytes
        || meta->storedSize < 20) return false;
    const qsizetype headerOffset = meta->payloadOffset - kOuterChunkHeaderBytes;
    // Hide the outer chunk from ordinary scanning and damage the primary FEC
    // prefix. The footer mirror remains intact for exact-boundary recovery.
    bytes[headerOffset] = static_cast<char>(bytes.at(headerOffset) ^ 0x35);
    for (qsizetype i = 0; i < 8; ++i)
    {
        bytes[meta->payloadOffset + i] = static_cast<char>(
            bytes.at(meta->payloadOffset + i) ^ (0x51 + i));
    }
    return writeBytes(path, bytes);
}

bool damageMetaMirrorFooter(const std::filesystem::path& path)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes)) return false;
    const auto meta = findOuterChunk(bytes, QByteArrayLiteral("meta"));
    if (!meta || meta->storedSize < 8) return false;
    const qsizetype footerMagic = meta->payloadOffset
        + static_cast<qsizetype>(meta->storedSize) - 8;
    if (footerMagic < meta->payloadOffset) return false;
    bytes[footerMagic] = static_cast<char>(bytes.at(footerMagic) ^ 0x42);
    return writeBytes(path, bytes);
}

bool declareOversizedOuterDocument(const std::filesystem::path& path)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes)) return false;
    const auto docm = findOuterChunk(bytes, QByteArrayLiteral("docm"));
    if (!docm) return false;
    const qsizetype headerOffset = docm->payloadOffset - kOuterChunkHeaderBytes;
    constexpr quint64 declaredSize = 768ull * 1024ull * 1024ull;
    for (int byte = 0; byte < 8; ++byte)
    {
        bytes[headerOffset + 16 + byte] = static_cast<char>(
            (declaredSize >> (byte * 8)) & 0xffu);
    }
    return writeBytes(path, bytes);
}

bool destroyNativeSection(
    const std::filesystem::path& path,
    const QByteArray& nativeType)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes)) return false;
    const auto outer = findOuterChunk(bytes, QByteArrayLiteral("docm"));
    if (!outer || (outer->flags & 4u) == 0) return false;
    const QByteArray documentBytes = bytes.mid(
        outer->payloadOffset, static_cast<qsizetype>(outer->storedSize));
    std::vector<std::vector<std::byte>> fragments{};
    try
    {
        fragments = assets::splitSedFileSections(std::as_bytes(std::span(
            documentBytes.constData(), static_cast<std::size_t>(documentBytes.size()))));
    }
    catch (...)
    {
        return false;
    }
    qsizetype offset{};
    for (const std::vector<std::byte>& fragment : fragments)
    {
        const QByteArray fragmentBytes(
            reinterpret_cast<const char*>(fragment.data()),
            static_cast<qsizetype>(fragment.size()));
        // splitSedFileSections owns the native framing arithmetic. Every
        // semantic fragment starts with the eight-byte sectv1 anchor followed
        // by its exact four-byte type. Do not search for the type text: the
        // anchor itself contains "sect", which previously made the SECT
        // fixture destroy HEAD instead.
        if (fragmentBytes.size() >= 12
            && fragmentBytes.mid(8, 4) == nativeType)
        {
            const qsizetype absolute = outer->payloadOffset + offset;
            constexpr qsizetype descriptorBytes = 100;
            constexpr qsizetype slotBytes = 4096;
            constexpr qsizetype slotPayloadBytes = 4092;
            QDataStream descriptor(fragmentBytes);
            descriptor.setByteOrder(QDataStream::LittleEndian);
            descriptor.skipRawData(44);
            quint32 dataStreams{}, parityStreams{}, chunkPayloadBytes{}, stripeCount{}, codedOffset{};
            descriptor >> dataStreams >> parityStreams >> chunkPayloadBytes
                       >> stripeCount >> codedOffset;
            if (descriptor.status() != QDataStream::Ok || dataStreams == 0
                || parityStreams == 0 || stripeCount == 0
                || chunkPayloadBytes != slotPayloadBytes
                || parityStreams + 1u > dataStreams + parityStreams)
            {
                return false;
            }

            // Turn p+1 slots in the same horizontal stripe into independently
            // CRC-detected erasures.  This exceeds the section's repair
            // budget without destroying either checked descriptor, the
            // native header mirror/footer, or an adjacent semantic section.
            for (quint32 stream = 0; stream <= parityStreams; ++stream)
            {
                const quint64 slot = static_cast<quint64>(stream) * stripeCount;
                const quint64 local = static_cast<quint64>(descriptorBytes)
                    + codedOffset + slot * slotBytes;
                if (local + slotBytes > static_cast<quint64>(fragmentBytes.size()))
                {
                    return false;
                }
                const qsizetype damagedByte = absolute + static_cast<qsizetype>(local);
                bytes[damagedByte] = static_cast<char>(bytes.at(damagedByte)
                    ^ static_cast<char>(0x51u + stream));
            }
            return writeBytes(path, bytes);
        }
        offset += fragmentBytes.size();
    }
    return false;
}

bool damageOuterDocmHeader(const std::filesystem::path& path)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes)) return false;
    const qsizetype typeOffset = bytes.indexOf(QByteArrayLiteral("docm"));
    if (typeOffset < 8) return false;
    bytes[typeOffset - 8] = static_cast<char>(bytes.at(typeOffset - 8) ^ 0x22);
    return writeBytes(path, bytes);
}

bool damageOuterAndNativeHeaders(const std::filesystem::path& path)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes)) return false;
    const auto outer = findOuterChunk(bytes, QByteArrayLiteral("docm"));
    if (!outer || outer->storedSize < 28
        || outer->storedSize > static_cast<quint64>(bytes.size()
            - outer->payloadOffset))
    {
        return false;
    }
    // Destroy the native primary fixed header while leaving the checked
    // semantic section descriptors available for the corruption scanner.
    for (qsizetype i = 0; i < 28; ++i)
    {
        const qsizetype offset = outer->payloadOffset + i;
        bytes[offset] = static_cast<char>(bytes.at(offset) ^ (0x31 + i));
    }
    // Also damage the checked native mirror, forcing recovery to synthesize
    // framing from validated sectv1 descriptors.  The test intentionally
    // leaves outer meta intact so its project type is the trusted fallback.
    constexpr qsizetype nativeMirrorBytes = 4 + 4 + 28 + 4;
    constexpr qsizetype nativeFooterBytes = 8;
    const qsizetype payloadEnd = outer->payloadOffset
        + static_cast<qsizetype>(outer->storedSize);
    const qsizetype mirror = payloadEnd
        - nativeFooterBytes - nativeMirrorBytes;
    if (mirror < outer->payloadOffset + 28
        || mirror + nativeMirrorBytes + nativeFooterBytes != payloadEnd
        || bytes.mid(mirror, 4) != QByteArray("mhdr", 4))
    {
        return false;
    }
    for (qsizetype i = 0; i < nativeMirrorBytes; ++i)
    {
        const qsizetype offset = mirror + i;
        bytes[offset] = static_cast<char>(bytes.at(offset) ^ (0x71 + i));
    }
    // Finally damage the outer docm chunk magic so parseRecoveryChunks must
    // use its bounded damaged-envelope/native-semantic fallback rather than
    // the normal chunk header path.
    const qsizetype outerMagic = outer->payloadOffset - kOuterChunkHeaderBytes;
    bytes[outerMagic] = static_cast<char>(bytes.at(outerMagic) ^ 0x2d);
    return writeBytes(path, bytes);
}

bool damageNativePrimaryMagic(const std::filesystem::path& path)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes)) return false;
    const auto outer = findOuterChunk(bytes, QByteArrayLiteral("docm"));
    if (!outer || outer->storedSize < 28
        || outer->storedSize > static_cast<quint64>(bytes.size()
            - outer->payloadOffset))
    {
        return false;
    }
    const qsizetype offset = outer->payloadOffset;
    bytes[offset] = static_cast<char>(bytes.at(offset) ^ 0x4b);
    return writeBytes(path, bytes);
}

bool damageMetaShards(const std::filesystem::path& path, const int shardCount)
{
    QByteArray bytes{};
    if (!readBytes(path, &bytes) || shardCount <= 0) return false;
    const auto meta = findOuterChunk(bytes, QByteArrayLiteral("meta"));
    if (!meta || (meta->flags & 2u) == 0) return false;
    const qsizetype payload = meta->payloadOffset;
    QDataStream prefix(bytes.mid(payload, 8));
    prefix.setByteOrder(QDataStream::LittleEndian);
    quint64 tableSize{};
    prefix >> tableSize;
    const qsizetype fecDescriptor = payload + 8 + 8 + 4 + 4 + 8 + 32;
    QDataStream descriptor(bytes.mid(fecDescriptor, 24));
    descriptor.setByteOrder(QDataStream::LittleEndian);
    quint64 rawSize{}, storedSize{};
    quint32 dataShards{}, shardSize{};
    descriptor >> rawSize >> storedSize >> dataShards >> shardSize;
    (void)rawSize; (void)storedSize;
    if (prefix.status() != QDataStream::Ok || descriptor.status() != QDataStream::Ok
        || tableSize == 0 || shardSize == 0
        || dataShards < static_cast<quint32>(shardCount)) return false;
    const qsizetype dataOffset = payload + 8 + static_cast<qsizetype>(tableSize) + 4;
    for (int shard = 0; shard < shardCount; ++shard)
    {
        const qsizetype offset = dataOffset + static_cast<qsizetype>(shard) * shardSize;
        if (offset >= payload + static_cast<qsizetype>(meta->storedSize)) return false;
        bytes[offset] = static_cast<char>(bytes.at(offset) ^ (0x61 + shard));
    }
    return writeBytes(path, bytes);
}

QByteArray canonicalNestedLevel(const Level& level)
{
    const std::vector<std::byte> bytes = assets::saveToSedBytes(
        level, {}, assets::SedSerializationProfile::UnprotectedNested);
    return QByteArray(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<qsizetype>(bytes.size()));
}

QByteArray historyWithSelectionOverride(
    const QByteArray& payload,
    const QString& selectionKey,
    const QCborArray& selectionValue,
    const qsizetype manifestSelectionIndex = 2)
{
    QCborParserError parseError{};
    const QCborValue rootValue = QCborValue::fromCbor(payload, &parseError);
    if (parseError.error != QCborError::NoError || !rootValue.isMap())
    {
        return {};
    }
    QCborMap root = rootValue.toMap();
    QCborArray groups = root.value(QStringLiteral("undoGroups")).toArray();
    if (groups.isEmpty() || !groups.at(0).isArray())
    {
        return {};
    }
    QCborArray group = groups.at(0).toArray();
    if (group.isEmpty() || !group.at(0).isArray())
    {
        return {};
    }
    QCborArray manifest = group.at(0).toArray();
    if ((manifest.size() != 4 && manifest.size() != 5)
        || (manifestSelectionIndex != 2 && manifestSelectionIndex != 4)
        || (manifestSelectionIndex == 4 && manifest.size() != 5)
        || !manifest.at(manifestSelectionIndex).isMap())
    {
        return {};
    }
    QCborMap selection = manifest.at(manifestSelectionIndex).toMap();
    selection.insert(selectionKey, selectionValue);
    manifest[manifestSelectionIndex] = selection;
    group[0] = manifest;
    groups[0] = group;
    root.insert(QStringLiteral("undoGroups"), groups);
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

QByteArray historyWithDirtyOverride(
    const QByteArray& payload,
    const qint64 dirtyFlags)
{
    QCborParserError parseError{};
    const QCborValue rootValue = QCborValue::fromCbor(payload, &parseError);
    if (parseError.error != QCborError::NoError || !rootValue.isMap())
    {
        return {};
    }
    QCborMap root = rootValue.toMap();
    QCborArray groups = root.value(QStringLiteral("undoGroups")).toArray();
    if (groups.isEmpty() || !groups.at(0).isArray())
    {
        return {};
    }
    QCborArray group = groups.at(0).toArray();
    if (group.isEmpty() || !group.at(0).isArray())
    {
        return {};
    }
    QCborArray manifest = group.at(0).toArray();
    if ((manifest.size() != 4 && manifest.size() != 5)
        || !manifest.at(1).isInteger())
    {
        return {};
    }
    manifest[1] = QCborValue(dirtyFlags);
    group[0] = manifest;
    groups[0] = group;
    root.insert(QStringLiteral("undoGroups"), groups);
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

QByteArray historyWithNonCanonicalBaseReference(
    const QByteArray& payload,
    const qint64 canonicalReference,
    const qint64 duplicateIndex)
{
    if (canonicalReference >= 0 || duplicateIndex < 0) return {};
    QCborParserError parseError{};
    const QCborValue rootValue = QCborValue::fromCbor(payload, &parseError);
    if (parseError.error != QCborError::NoError || !rootValue.isMap())
    {
        return {};
    }
    QCborMap root = rootValue.toMap();
    for (const QString& key : { QStringLiteral("undoGroups"), QStringLiteral("redoGroups") })
    {
        QCborArray groups = root.value(key).toArray();
        for (qsizetype groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
        {
            QCborArray group = groups.at(groupIndex).toArray();
            for (qsizetype entryIndex = 0; entryIndex < group.size(); ++entryIndex)
            {
                QCborArray manifest = group.at(entryIndex).toArray();
                if ((manifest.size() != 4 && manifest.size() != 5)
                    || !manifest.at(3).isArray()) continue;
                QCborArray record = manifest.at(3).toArray();
                QCborArray* references = nullptr;
                QCborArray checkpointReferences{};
                if (record.size() == 3 && record.at(2).isArray())
                {
                    checkpointReferences = record.at(2).toArray();
                    references = &checkpointReferences;
                }
                else if (record.size() == 6 && record.at(5).isArray())
                {
                    checkpointReferences = record.at(5).toArray();
                    references = &checkpointReferences;
                }
                if (!references) continue;
                for (qsizetype referenceIndex = 0;
                     referenceIndex < references->size(); ++referenceIndex)
                {
                    if (!references->at(referenceIndex).isInteger()) continue;
                    const qint64 reference = references->at(referenceIndex).toInteger(
                        std::numeric_limits<qint64>::min());
                    if (reference >= 0 || reference == std::numeric_limits<qint64>::min())
                    {
                        continue;
                    }
                    if (reference != canonicalReference) continue;
                    (*references)[referenceIndex] = QCborValue(-1 - duplicateIndex);
                    if (record.size() == 3) record[2] = *references;
                    else record[5] = *references;
                    manifest[3] = record;
                    group[entryIndex] = manifest;
                    groups[groupIndex] = group;
                    root.insert(key, groups);
                    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
                }
            }
        }
    }
    return {};
}

QByteArray historyWithMalformedDelta(const QByteArray& payload)
{
    QCborParserError parseError{};
    const QCborValue rootValue = QCborValue::fromCbor(payload, &parseError);
    if (parseError.error != QCborError::NoError || !rootValue.isMap())
    {
        return {};
    }
    QCborMap root = rootValue.toMap();
    QCborArray groups = root.value(QStringLiteral("undoGroups")).toArray();
    for (qsizetype groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        QCborArray group = groups.at(groupIndex).toArray();
        for (qsizetype entryIndex = 0; entryIndex < group.size(); ++entryIndex)
        {
            QCborArray manifest = group.at(entryIndex).toArray();
            if ((manifest.size() != 4 && manifest.size() != 5)
                || !manifest.at(3).isArray())
            {
                continue;
            }
            QCborArray record = manifest.at(3).toArray();
            if (record.size() != 6 || !record.at(1).isInteger()
                || record.at(1).toInteger(-1) != 1)
            {
                continue;
            }
            // A prefix larger than the previous list is a schema-valid CBOR
            // integer but must be rejected by the strict delta decoder.
            record[2] = QCborValue(std::numeric_limits<qint64>::max());
            manifest[3] = record;
            group[entryIndex] = manifest;
            groups[groupIndex] = group;
            root.insert(QStringLiteral("undoGroups"), groups);
            return ui::recovery::serializeCanonicalCbor(QCborValue(root));
        }
    }
    return {};
}

QByteArray historyWithOversizedPoolFragment(const QByteArray& payload)
{
    QCborParserError parseError{};
    const QCborValue rootValue = QCborValue::fromCbor(payload, &parseError);
    if (parseError.error != QCborError::NoError || !rootValue.isMap()) return {};
    QCborMap root = rootValue.toMap();
    QCborArray pool = root.value(QStringLiteral("pool")).toArray();
    if (pool.isEmpty()) return {};
    pool[0] = QByteArray(8 * 1024 + 1, '\x7f');
    root.insert(QStringLiteral("pool"), pool);
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

QByteArray historyWithDuplicatePoolEntry(const QByteArray& payload)
{
    QCborParserError parseError{};
    const QCborValue rootValue = QCborValue::fromCbor(payload, &parseError);
    if (parseError.error != QCborError::NoError || !rootValue.isMap()) return {};
    QCborMap root = rootValue.toMap();
    QCborArray pool = root.value(QStringLiteral("pool")).toArray();
    if (pool.isEmpty()) return {};
    pool.append(pool.at(0));
    root.insert(QStringLiteral("pool"), pool);
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

QByteArray historyWithUnusedPoolEntry(const QByteArray& payload)
{
    QCborParserError parseError{};
    const QCborValue rootValue = QCborValue::fromCbor(payload, &parseError);
    if (parseError.error != QCborError::NoError || !rootValue.isMap()) return {};
    QCborMap root = rootValue.toMap();
    QCborArray pool = root.value(QStringLiteral("pool")).toArray();
    pool.append(QByteArrayLiteral("unused-history-fragment"));
    root.insert(QStringLiteral("pool"), pool);
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

QByteArray historyWithShortNonFinalGroup(const QByteArray& payload)
{
    QCborParserError parseError{};
    const QCborValue rootValue = QCborValue::fromCbor(payload, &parseError);
    if (parseError.error != QCborError::NoError || !rootValue.isMap()) return {};
    QCborMap root = rootValue.toMap();
    QCborArray groups = root.value(QStringLiteral("undoGroups")).toArray();
    if (groups.size() < 2 || !groups.at(0).isArray()) return {};
    QCborArray first = groups.at(0).toArray();
    if (first.size() != 32) return {};
    first.removeLast();
    groups[0] = first;
    root.insert(QStringLiteral("undoGroups"), groups);
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

QByteArray cborWithExcessiveDeclaredNodes()
{
    // A definite array declaring 3,000,000 children. It is intentionally
    // truncated: the streaming preflight must reject the declared allocation
    // pressure before QCborValue attempts to materialize any child nodes.
    QByteArray bytes{};
    bytes.append(static_cast<char>(0x9a)); // array, following u32 item count
    constexpr quint32 count = 3'000'000;
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        bytes.append(static_cast<char>((count >> shift) & 0xffu));
    }
    return bytes;
}

QByteArray cborWithExcessiveNesting()
{
    QByteArray bytes(66, static_cast<char>(0x81)); // one-item arrays
    bytes.append(static_cast<char>(0x00));          // innermost integer
    return bytes;
}

QByteArray cborWithExcessiveMaterialization()
{
    // Twenty thousand one-byte scalars have a tiny encoded form but require
    // a much larger QCborValue element graph. The aggregate preflight budget
    // must reject this before Qt allocates that graph.
    constexpr quint32 count = 20'000;
    QByteArray bytes{};
    bytes.reserve(5 + static_cast<qsizetype>(count));
    bytes.append(static_cast<char>(0x9a));
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        bytes.append(static_cast<char>((count >> shift) & 0xffu));
    }
    bytes.append(QByteArray(static_cast<qsizetype>(count), '\0'));
    return bytes;
}

QByteArray cborWithOversizedTextItem()
{
    // Definite UTF-8 text with a canonical u32 byte count.  It is deliberately
    // not a complete HIST map: the streaming guard must reject the declared
    // per-text allocation before the materializing parser reaches schema
    // validation.
    constexpr quint32 byteCount = 3u * 1024u * 1024u + 1u;
    QByteArray bytes{};
    bytes.reserve(5 + static_cast<qsizetype>(byteCount));
    bytes.append(static_cast<char>(0x7a)); // text, following u32 byte count
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        bytes.append(static_cast<char>((byteCount >> shift) & 0xffu));
    }
    const qsizetype payloadOffset = bytes.size();
    bytes.resize(payloadOffset + static_cast<qsizetype>(byteCount));
    std::fill(bytes.begin() + payloadOffset, bytes.end(), 'a');
    return bytes;
}

bool sameDocumentState(
    const editor::DocumentState& lhs,
    const editor::DocumentState& rhs)
{
    return canonicalNestedLevel(lhs.level) == canonicalNestedLevel(rhs.level)
        && ui::recovery::serializeSelection(lhs.selection)
            == ui::recovery::serializeSelection(rhs.selection);
}

editor::Document makeDeepHistoryDocument(
    const ProjectType kind,
    const int editCount,
    const int undoCount = 7,
    const bool duplicateBaseFragments = false,
    const std::size_t levelPaddingBytes = 0,
    const int sectorCount = 40,
    const int vertexCount = 96,
    const int thingCount = 80,
    const UvCoordinateSpace uvCoordinateSpace =
        UvCoordinateSpace::Normalized,
    const bool legacyFrameSpelling = false)
{
    editor::DocumentState state{};
    state.level.kind = kind;
    state.level.uvCoordinateSpace = uvCoordinateSpace;
    state.level.masterCMP = "history.cmp";
    state.level.colormaps = { "history.cmp", "bonus.cmp" };
    state.level.materials = { "stone.mat", "metal.mat" };
    state.level.layers = { "base", "details", "gameplay" };
    if (duplicateBaseFragments)
    {
        // A long uniform HEAD string deterministically produces repeated
        // fixed history blocks. It gives the malformed-reference test a real
        // duplicate base fragment whose non-first index must be rejected.
        state.level.lvisstring.assign(64 * 1024, 'v');
    }
    if (levelPaddingBytes != 0)
    {
        state.level.lvisstring.assign(levelPaddingBytes, 'p');
    }
    for (int sectorIndex = 0; sectorIndex < sectorCount; ++sectorIndex)
    {
        Sector sector{};
        sector.id = sectorIndex + 1;
        sector.num = sectorIndex;
        sector.flags = 0x100 + sectorIndex;
        sector.layer = sectorIndex % 3;
        sector.sound = "history-loop-" + std::to_string(sectorIndex) + ".wav";
        sector.vertices.reserve(static_cast<std::size_t>(vertexCount));
        for (int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
        {
            sector.vertices.push_back(Vertex{
                static_cast<double>(vertexIndex),
                static_cast<double>(sectorIndex),
                static_cast<double>((vertexIndex * 7 + sectorIndex) % 19),
                vertexIndex % 5 });
        }
        Surface surface{};
        surface.num = sectorIndex;
        surface.material = sectorIndex % 2 == 0 ? "stone.mat" : "metal.mat";
        surface.vertices.reserve(static_cast<std::size_t>(vertexCount));
        surface.texVertices.reserve(static_cast<std::size_t>(vertexCount));
        for (int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
        {
            surface.vertices.push_back(vertexIndex);
            surface.texVertices.push_back(TexVertex{
                static_cast<float>(vertexIndex) / 16.0f,
                static_cast<float>(sectorIndex) / 8.0f,
                ColorF{ 0.13f, 0.27f, 0.51f, 0.73f } });
        }
        sector.surfaces.push_back(std::move(surface));
        state.level.sectors.push_back(std::move(sector));
    }
    for (int thingIndex = 0; thingIndex < thingCount; ++thingIndex)
    {
        Thing thing{};
        thing.id = 1000 + thingIndex;
        thing.num = thingIndex;
        thing.sector = thingIndex % sectorCount;
        thing.templateName = "history-template";
        thing.name = "history-thing-" + std::to_string(thingIndex);
        thing.x = thingIndex * 0.25;
        thing.y = thingIndex * -0.5;
        thing.z = thingIndex * 0.75;
        thing.layer = thingIndex % 3;
        for (int frameIndex = 0; frameIndex < 3; ++frameIndex)
        {
            TplValue frame{};
            frame.name = "frame";
            frame.adjType = AdjType::frame;
            frame.setFrame(
                thing.x + frameIndex,
                thing.y - frameIndex,
                thing.z + frameIndex * 0.5,
                frameIndex * 5.0,
                frameIndex * 10.0,
                frameIndex * 15.0);
            if (legacyFrameSpelling)
            {
                // The checked-in V1 compatibility fixture authored fixed-six
                // Frame tokens. Pin those original bytes independently of the
                // current lossless default used by new editing operations.
                frame.stringValue = sed::format::frameValue(
                    thing.x + frameIndex, thing.y - frameIndex,
                    thing.z + frameIndex * 0.5, frameIndex * 5.0,
                    frameIndex * 10.0, frameIndex * 15.0, 6);
            }
            thing.values.push_back(std::move(frame));
        }
        TplValue nonFrame{};
        nonFrame.name = "health";
        nonFrame.type = ValueType::integer;
        nonFrame.intValue = 100;
        thing.values.push_back(std::move(nonFrame));
        state.level.things.push_back(std::move(thing));
    }
    ThingTemplate historyTemplate{};
    historyTemplate.name = "history-template";
    historyTemplate.desc = "Template bytes shared by every history state";
    state.level.templates.push_back(std::move(historyTemplate));
    for (int cogIndex = 0; cogIndex < 2; ++cogIndex)
    {
        Cog cog{};
        cog.id = 2001 + cogIndex;
        cog.num = cogIndex;
        cog.name = "history-" + std::to_string(cogIndex) + ".cog";
        state.level.cogs.push_back(std::move(cog));
    }
    for (int lightIndex = 0; lightIndex < 2; ++lightIndex)
    {
        SedLight light{};
        light.id = 3001 + lightIndex;
        light.position = TVector{
            1.0 + lightIndex, 2.0 + lightIndex, 3.0 + lightIndex };
        state.level.lights.push_back(light);
    }
    HNode node{};
    node.id = 4001;
    node.name = "history-node";
    state.level.h3doNodes.push_back(std::move(node));

    editor::Document document(std::move(state));
    for (int edit = 0; edit < editCount; ++edit)
    {
        editor::DocumentDirty dirty{};
        switch (edit % 4)
        {
        case 0: dirty = editor::DocumentDirty::LevelGeometry; break;
        case 1: dirty = editor::DocumentDirty::Things; break;
        case 2: dirty = editor::DocumentDirty::Lights; break;
        default: dirty = editor::DocumentDirty::Cogs; break;
        }
        document.pushUndo("history edit " + std::to_string(edit), dirty);
        document.selection().clear();
        document.selection().sectors.insert(edit % sectorCount);
        document.selection().sectors.insert((edit + 1) % sectorCount);
        document.selection().surfaces.insert(SurfaceRef{ edit % sectorCount, 0 });
        document.selection().surfaces.insert(SurfaceRef{ (edit + 1) % sectorCount, 0 });
        document.selection().surfaceVertices.insert(editor::SurfaceVertexRef{
            SurfaceRef{ edit % sectorCount, 0 }, edit % vertexCount });
        document.selection().surfaceVertices.insert(editor::SurfaceVertexRef{
            SurfaceRef{ (edit + 1) % sectorCount, 0 }, (edit + 1) % vertexCount });
        document.selection().things.insert(edit % thingCount);
        document.selection().things.insert((edit + 1) % thingCount);
        document.selection().frames.insert(level::FrameRef{ edit % thingCount, edit % 3 });
        document.selection().frames.insert(level::FrameRef{
            (edit + 1) % thingCount, (edit + 1) % 3 });
        document.selection().lights.insert(0);
        document.selection().lights.insert(1);
        document.selection().cogs.insert(0);
        document.selection().cogs.insert(1);
        switch (edit % 4)
        {
        case 0:
            document.level().sectors[static_cast<std::size_t>(edit % sectorCount)]
                .vertices[static_cast<std::size_t>(edit % vertexCount)].x += 0.125 + edit;
            break;
        case 1:
            document.level().things[static_cast<std::size_t>(edit % thingCount)].x += 1.25;
            break;
        case 2:
            document.level().lights[0].intensity += 0.5;
            document.level().sectors[0].ambient.r += 0.001f;
            break;
        default:
            document.level().cogs[0].name = "history-" + std::to_string(edit) + ".cog";
            break;
        }
        document.level().curId = 5000 + edit;
    }
    for (int redoEntry = 0; redoEntry < std::min(undoCount, editCount); ++redoEntry)
    {
        (void)document.undo();
    }
    return document;
}

bool verifyRestoredHistory(
    const editor::Document& source,
    const editor::DocumentRecoveryHistory& expected)
{
    // Native readers retain compact COW snapshots rather than an expanded
    // Level vector. Use the source's portable walk as the state oracle while
    // exercising the native compact restore object itself.
    const editor::DocumentRecoveryHistory portableExpected = expected.compact
        ? source.recoveryHistory() : expected;
    editor::Document undoDocument(source.current());
    undoDocument.restoreRecoveryHistory(source.current(), expected);
    if (undoDocument.undoDepth() != expected.undoEntryCount()
        || undoDocument.redoDepth() != expected.redoEntryCount())
    {
        return false;
    }
    for (const editor::DocumentRecoveryHistoryEntry& entry : portableExpected.undoEntries)
    {
        if (undoDocument.nextUndoLabel() != entry.label)
        {
            return false;
        }
        editor::DocumentDirty dirty{};
        if (!undoDocument.undo(&dirty)
            || static_cast<std::uint32_t>(dirty) != entry.dirtyFlags
            || !sameDocumentState(undoDocument.current(), entry.state))
        {
            return false;
        }
    }

    editor::Document redoDocument(source.current());
    redoDocument.restoreRecoveryHistory(source.current(), expected);
    for (const editor::DocumentRecoveryHistoryEntry& entry : portableExpected.redoEntries)
    {
        if (redoDocument.nextRedoLabel() != entry.label)
        {
            return false;
        }
        editor::DocumentDirty dirty{};
        if (!redoDocument.redo(&dirty)
            || static_cast<std::uint32_t>(dirty) != entry.dirtyFlags
            || !sameDocumentState(redoDocument.current(), entry.state))
        {
            return false;
        }
    }
    return true;
}

void runAppHistoryGoldenTests()
{
    // Keep this fixture deliberately small enough for a fast app test while
    // retaining a real multi-entry undo/redo timeline.  The current cursor is
    // between two redo entries, so successful repair proves both directions,
    // not merely that HIST can be parsed.
    const editor::Document source = makeDeepHistoryDocument(
        ProjectType::IJIM,
        6,
        2,
        false,
        0,
        4,
        12,
        4,
        UvCoordinateSpace::TexturePixels,
        true);
    const QByteArray editorPayload = appHistoryEditorFixture();
    const QByteArray historyPayload = ui::recovery::serializeEditorDocumentHistory(source);
    check(!historyPayload.isEmpty(),
        "app HIST golden: real editor-history CBOR payload is generated");

    const auto toNativeOptional = [](const std::array<char, 4>& type,
        const QByteArray& payload)
    {
        std::vector<std::byte> bytes{};
        bytes.reserve(static_cast<std::size_t>(payload.size()));
        for (const unsigned char value : payload)
            bytes.push_back(static_cast<std::byte>(value));
        return assets::SedOptionalSection{ type, std::move(bytes) };
    };
    const std::array optional{
        toNativeOptional({ 'e', 'd', 'i', 't' }, editorPayload),
        toNativeOptional({ 'h', 'i', 's', 't' }, historyPayload),
    };
    const std::vector<std::byte> generated = assets::saveToSedBytes(
        source.level(), optional, assets::SedSerializationProfile::ProtectedV1, 0);
    const QByteArray generatedBytes(
        reinterpret_cast<const char*>(generated.data()),
        static_cast<qsizetype>(generated.size()));
    QDataStream generatedHeader(generatedBytes);
    generatedHeader.setByteOrder(QDataStream::LittleEndian);
    char generatedMagic[4]{};
    quint32 generatedVersion{};
    generatedHeader.readRawData(generatedMagic, 4);
    generatedHeader >> generatedVersion;
    check(generatedHeader.status() == QDataStream::Ok
            && QByteArray(generatedMagic, 4) == QByteArray("sed\0", 4)
            && generatedVersion == assets::SedLegacyBinaryVersion,
        "app HIST golden: compatibility fixture remains native SED V1 with pixel-space UVs");

    const editor::Document normalizedSource = makeDeepHistoryDocument(
        ProjectType::IJIM, 3, 1);
    const QByteArray normalizedHistoryPayload =
        ui::recovery::serializeEditorDocumentHistory(normalizedSource);
    const std::array normalizedOptional{
        toNativeOptional({ 'e', 'd', 'i', 't' }, editorPayload),
        toNativeOptional({ 'h', 'i', 's', 't' }, normalizedHistoryPayload),
    };
    const std::vector<std::byte> normalizedBytes = assets::saveToSedBytes(
        normalizedSource.level(),
        normalizedOptional,
        assets::SedSerializationProfile::ProtectedV1,
        0);
    const bool normalizedHeaderIsV2 = normalizedBytes.size() >= 8u
        && normalizedBytes[4]
            == static_cast<std::byte>(assets::SedBinaryVersion & 0xffu)
        && normalizedBytes[5] == std::byte{ 0 }
        && normalizedBytes[6] == std::byte{ 0 }
        && normalizedBytes[7] == std::byte{ 0 };
    std::optional<assets::SedFile> normalizedLoaded{};
    try
    {
        normalizedLoaded = assets::loadSedFileBytes(normalizedBytes);
    }
    catch (...)
    {
        normalizedLoaded.reset();
    }
    const assets::SedOptionalSection* normalizedLoadedHistory =
        normalizedLoaded
        ? findAppOptionalSection(
            normalizedLoaded->optionalSections, { 'h', 'i', 's', 't' })
        : nullptr;
    const assets::SedOptionalSection* normalizedLoadedEdit =
        normalizedLoaded
        ? findAppOptionalSection(
            normalizedLoaded->optionalSections, { 'e', 'd', 'i', 't' })
        : nullptr;
    QString normalizedHistoryError{};
    const std::optional<editor::DocumentRecoveryHistory> normalizedHistory =
        normalizedLoaded && normalizedLoadedHistory
        ? ui::recovery::deserializeEditorDocumentHistory(
            QByteArray(
                reinterpret_cast<const char*>(
                    normalizedLoadedHistory->payload.data()),
                static_cast<qsizetype>(
                    normalizedLoadedHistory->payload.size())),
            ProjectType::IJIM,
            normalizedLoaded->level,
            &normalizedHistoryError)
        : std::nullopt;
    check(normalizedHeaderIsV2
            && normalizedLoaded
            && normalizedLoaded->level.uvCoordinateSpace
                == UvCoordinateSpace::Normalized
            && canonicalNestedLevel(normalizedLoaded->level)
                == canonicalNestedLevel(normalizedSource.level())
            && normalizedLoadedEdit
            && normalizedLoadedEdit->payload
                == std::vector<std::byte>(
                    reinterpret_cast<const std::byte*>(editorPayload.constData()),
                    reinterpret_cast<const std::byte*>(editorPayload.constData())
                        + editorPayload.size())
            && normalizedLoadedHistory
            && normalizedLoadedHistory->payload
                == std::vector<std::byte>(
                    reinterpret_cast<const std::byte*>(
                        normalizedHistoryPayload.constData()),
                    reinterpret_cast<const std::byte*>(
                        normalizedHistoryPayload.constData())
                        + normalizedHistoryPayload.size())
            && normalizedHistory
            && normalizedHistoryError.isEmpty()
            && verifyRestoredHistory(normalizedSource, *normalizedHistory),
        "app HIST native SED V2 strictly retains normalized Level data and usable undo/redo history");
    const auto historyLayout = findAppSedSection(
        generatedBytes, QByteArrayLiteral("hist"));
    const auto editLayout = findAppSedSection(
        generatedBytes, QByteArrayLiteral("edit"));
    check(historyLayout && editLayout
            && historyLayout->parityStreams == 2
            && historyLayout->stripeCount >= 1
            && historyLayout->dataStreams + historyLayout->parityStreams >= 3,
        "app HIST golden: checked on-wire HIST and EDIT descriptors expose p=2 and physical slots");
    if (!historyLayout || !editLayout) return;

    const std::filesystem::path basePath = appHistoryGoldenPath();
    if (qEnvironmentVariableIsSet("SED_WRITE_APP_HIST_GOLDEN"))
    {
        check(writeAppHistoryGoldenFiles(generatedBytes, *historyLayout),
            "app HIST golden: explicit regeneration writes base and damage vectors");
    }

    QByteArray golden{};
    check(readBytes(basePath, &golden) && !golden.isEmpty(),
        "app HIST golden: checked-in native SED fixture is present");
    if (golden.isEmpty()) return;
    check(golden == generatedBytes,
        "app HIST golden: checked-in bytes match the deterministic V1 app fixture");

    const auto goldenHistoryLayout = findAppSedSection(
        golden, QByteArrayLiteral("hist"));
    check(goldenHistoryLayout
            && goldenHistoryLayout->parityStreams == 2
            && goldenHistoryLayout->stripeCount == historyLayout->stripeCount
            && goldenHistoryLayout->dataStreams == historyLayout->dataStreams,
        "app HIST golden: checked-in HIST descriptor matches the generated physical layout");
    if (!goldenHistoryLayout) return;

    const std::vector<std::byte> goldenSed = appSedBytes(golden);
    assets::SedFile baseline{};
    try
    {
        baseline = assets::loadSedFileBytes(goldenSed);
    }
    catch (const std::exception& exception)
    {
        check(false, (std::string("app HIST golden: strict base load failed: ")
            + exception.what()).c_str());
        return;
    }
    const assets::SedOptionalSection* const baselineEdit = findAppOptionalSection(
        baseline.optionalSections, { 'e', 'd', 'i', 't' });
    const assets::SedOptionalSection* const baselineHistory = findAppOptionalSection(
        baseline.optionalSections, { 'h', 'i', 's', 't' });
    check(baselineEdit && baselineHistory
            && baselineEdit->payload == std::vector<std::byte>(
                reinterpret_cast<const std::byte*>(editorPayload.constData()),
                reinterpret_cast<const std::byte*>(editorPayload.constData())
                    + editorPayload.size())
            && baselineHistory->payload == std::vector<std::byte>(
                reinterpret_cast<const std::byte*>(historyPayload.constData()),
                reinterpret_cast<const std::byte*>(historyPayload.constData())
                    + historyPayload.size()),
        "app HIST golden: strict load retains exact EDIT and real HIST payloads");

    // Retain the generated vector while making the direct byte-identical
    // save/load/save assertion; do not point into a temporary container.
    const std::vector<std::byte> canonicalVector = assets::saveToSedBytes(
        baseline.level, baseline.optionalSections,
        assets::SedSerializationProfile::ProtectedV1, 0);
    const QByteArray canonical(
        reinterpret_cast<const char*>(canonicalVector.data()),
        static_cast<qsizetype>(canonicalVector.size()));
    check(canonical == golden,
        "app HIST golden: deterministic save-load-save bytes are identical");

    if (!baselineHistory || !baselineEdit) return;

    QString historyError{};
    const auto baselineDecodedHistory = ui::recovery::deserializeEditorDocumentHistory(
        QByteArray(reinterpret_cast<const char*>(baselineHistory->payload.data()),
            static_cast<qsizetype>(baselineHistory->payload.size())),
        ProjectType::IJIM, baseline.level, &historyError);
    check(baselineDecodedHistory && historyError.isEmpty()
            && verifyRestoredHistory(source, *baselineDecodedHistory),
        "app HIST golden: intact CBOR history restores usable undo and redo branches");

    for (std::size_t damageCount = 1; damageCount <= 2; ++damageCount)
    {
        const std::filesystem::path damagedPath = basePath.parent_path()
            / (QStringLiteral("sed_v1_hist_document_%1_damage.sed")
                .arg(static_cast<qulonglong>(damageCount))).toStdString();
        QByteArray damaged{};
        check(readBytes(damagedPath, &damaged) && !damaged.isEmpty(),
            "app HIST golden: checked-in in-budget damage vector is present");
        if (damaged.isEmpty()) continue;
        const QByteArray expectedDamage = damageAppHistorySlots(
            golden, *goldenHistoryLayout, damageCount);
        check(damaged == expectedDamage,
            "app HIST golden: damage vector uses the documented physical HIST slots");
        assets::SedFile repaired{};
        try
        {
            repaired = assets::loadSedFileBytes(appSedBytes(damaged));
        }
        catch (const std::exception& exception)
        {
            check(false, (std::string("app HIST golden: in-budget strict repair failed: ")
                + exception.what()).c_str());
            continue;
        }
        const assets::SedOptionalSection* const repairedEdit = findAppOptionalSection(
            repaired.optionalSections, { 'e', 'd', 'i', 't' });
        const assets::SedOptionalSection* const repairedHistory = findAppOptionalSection(
            repaired.optionalSections, { 'h', 'i', 's', 't' });
        check(repaired.repairedChunks == damageCount * goldenHistoryLayout->stripeCount
                && canonicalNestedLevel(repaired.level)
                    == canonicalNestedLevel(baseline.level)
                && repairedEdit && baselineEdit
                && repairedEdit->payload == baselineEdit->payload
                && repairedHistory && baselineHistory
                && repairedHistory->payload == baselineHistory->payload,
            (std::string("app HIST golden: ") + std::to_string(damageCount)
                + " damaged chunk(s) self-heal exact Level, EDIT, and HIST").c_str());

        QString repairedHistoryError{};
        const auto decoded = repairedHistory
            ? ui::recovery::deserializeEditorDocumentHistory(
                QByteArray(reinterpret_cast<const char*>(repairedHistory->payload.data()),
                    static_cast<qsizetype>(repairedHistory->payload.size())),
                ProjectType::IJIM, repaired.level, &repairedHistoryError)
            : std::optional<editor::DocumentRecoveryHistory>{};
        check(decoded && repairedHistoryError.isEmpty()
                && verifyRestoredHistory(source, *decoded),
            (std::string("app HIST golden: ") + std::to_string(damageCount)
                + " damaged chunk(s) retain usable undo/redo").c_str());
    }

    const std::filesystem::path overBudgetPath = basePath.parent_path()
        / "sed_v1_hist_document_3_damage.sed";
    QByteArray overBudget{};
    check(readBytes(overBudgetPath, &overBudget) && !overBudget.isEmpty(),
        "app HIST golden: checked-in over-budget damage vector is present");
    if (overBudget.isEmpty()) return;
    check(overBudget == damageAppHistorySlots(
            golden, *goldenHistoryLayout, 3),
        "app HIST golden: three-damage vector targets three distinct slots in every stripe");
    bool strictRejected = false;
    try
    {
        (void)assets::loadSedFileBytes(appSedBytes(overBudget));
    }
    catch (const std::exception&)
    {
        strictRejected = true;
    }
    const assets::SedRecoveryLoadResult partial = assets::recoverFromSedBytes(
        appSedBytes(overBudget));
    const assets::SedOptionalSection* const partialEdit = findAppOptionalSection(
        partial.optionalSections, { 'e', 'd', 'i', 't' });
    const assets::SedOptionalSection* const partialHistory = findAppOptionalSection(
        partial.optionalSections, { 'h', 'i', 's', 't' });
    const bool histMissing = std::find(
        partial.missingOptionalSections.begin(),
        partial.missingOptionalSections.end(), "hist")
        != partial.missingOptionalSections.end();
    check(strictRejected && partial.partialRecoveryFromCorruption
            && partial.missingRequiredSections.empty() && histMissing
            && partialHistory == nullptr
            && canonicalNestedLevel(partial.level)
                == canonicalNestedLevel(baseline.level)
            && partialEdit && baselineEdit
            && partialEdit->payload == baselineEdit->payload,
        "app HIST golden: three damaged chunks drop only HIST while Level and EDIT remain exact");
}

} // namespace

void runDocumentRecoveryTests()
{
    static_assert(editor::DocumentMaximumHistoryEntries == 1024);
    static_assert(ui::recovery::MaximumPersistedHistoryEntries == 100'000);
    static_assert(ui::recovery::MaximumPersistedHistoryEntries
        > editor::DocumentMaximumHistoryEntries);

    check(fixedBase32(QByteArray{}) == QString{}
            && fixedBase32(QByteArrayLiteral("f")) == QStringLiteral("my")
            && fixedBase32(QByteArrayLiteral("fo")) == QStringLiteral("mzxq")
            && fixedBase32(QByteArrayLiteral("foobar")) == QStringLiteral("mzxw6ytboi"),
        "document_recovery independent filename oracle agrees with fixed RFC 4648 vectors");
    runAppHistoryGoldenTests();

    {
        constexpr std::array<quint64, 4> crossPathRequests{
            1024, 512, 512, 1,
        };
        const auto trace =
            ui::recovery::detail::exerciseNativeFallbackBudgetForTesting(
                1024, crossPathRequests);
        check(trace == std::array<quint64, 4>{ 2048, 1, 0, 1 },
            "document_recovery shares one two-input native fallback byte budget across damaged-header, embedded, signature, anchor, and retry paths");
    }

    {
        QCborMap mixedLengthKeys{};
        mixedLengthKeys.insert(QStringLiteral("aa"), 2);
        mixedLengthKeys.insert(QStringLiteral("b"), 1);
        const QByteArray mixedLengthEncoding =
            ui::recovery::serializeCanonicalCbor(QCborValue(mixedLengthKeys));
        constexpr ui::recovery::RecoveryCborLimits mixedLengthLimits{
            1024,
            16,
            4,
            16,
            0,
            1024 * 1024,
        };
        QString mixedLengthError{};
        const QByteArray expectedEncoding = QByteArray::fromHex("a261620162616102");
        const auto mixedLengthDecoded = ui::recovery::deserializeCanonicalCbor(
            expectedEncoding, mixedLengthLimits, &mixedLengthError);
        check(mixedLengthDecoded && mixedLengthError.isEmpty()
                && mixedLengthEncoding == expectedEncoding
                && mixedLengthDecoded->toMap().size() == 2
                && mixedLengthDecoded->toMap().value(QStringLiteral("b")).toInteger() == 1
                && mixedLengthDecoded->toMap().value(QStringLiteral("aa")).toInteger() == 2,
            "document_recovery canonical writer and streaming preflight agree on encoded key-length then unsigned-byte map order");
    }

    {
        bool rejectedNonTextKey{};
        try
        {
            QCborMap map{};
            map.insert(QCborValue(7), QStringLiteral("value"));
            (void)ui::recovery::serializeCanonicalCbor(QCborValue(map));
        }
        catch (const std::runtime_error&)
        {
            rejectedNonTextKey = true;
        }

        QByteArray duplicateKeys{};
        duplicateKeys.append(static_cast<char>(0xa2));
        duplicateKeys.append(static_cast<char>(0x61));
        duplicateKeys.append('a');
        duplicateKeys.append(static_cast<char>(0x00));
        duplicateKeys.append(static_cast<char>(0x61));
        duplicateKeys.append('a');
        duplicateKeys.append(static_cast<char>(0x01));
        QCborParserError duplicateParseError{};
        const QCborValue duplicateValue = QCborValue::fromCbor(
            duplicateKeys, &duplicateParseError);
        bool rejectedDuplicateKey{};
        try
        {
            (void)ui::recovery::serializeCanonicalCbor(duplicateValue);
        }
        catch (const std::runtime_error&)
        {
            rejectedDuplicateKey = true;
        }
        check(rejectedNonTextKey && rejectedDuplicateKey
                && duplicateParseError.error == QCborError::NoError,
            "document_recovery canonical writer rejects non-text and duplicate map keys that its reader cannot admit");
    }

    {
        QByteArray source(2 * 1024 * 1024 + 123, '\0');
        for (qsizetype i = 0; i < source.size(); ++i)
        {
            source[i] = static_cast<char>((i * 29) & 0xff);
        }
        qint64 cursor{};
        qint64 requested{};
        int calls{};
        const auto reader = [&source, &cursor, &requested, &calls](
            char* const destination, const qint64 count)
        {
            ++calls;
            requested += count;
            if (cursor >= source.size()) return qint64{0};
            const qint64 available = static_cast<qint64>(source.size()) - cursor;
            const qint64 copied = std::min(count, available);
            std::copy_n(source.constData() + cursor,
                static_cast<std::size_t>(copied), destination);
            cursor += copied;
            return copied;
        };
        QString readError{};
        const auto read = ui::recovery::detail::readRecoveryBytesBoundedForTesting(
            source.size(), reader, &readError);
        check(read && *read == source && readError.isEmpty()
                && cursor == source.size() && requested == source.size() + 1
                && calls == 4,
            "document_recovery bounded reader fills one exact allocation in 1 MiB transfers and probes exactly one trailing byte");
    }

    {
        constexpr qint64 kExpectedBytes = 130000;
        QByteArray growingSource(static_cast<qsizetype>(kExpectedBytes + 100), 'g');
        qint64 cursor{};
        qint64 returned{};
        const auto reader = [&growingSource, &cursor, &returned](
            char* const destination, const qint64 count)
        {
            const qint64 available = static_cast<qint64>(growingSource.size()) - cursor;
            const qint64 copied = std::min(count, available);
            if (copied <= 0) return qint64{0};
            std::copy_n(growingSource.constData() + cursor,
                static_cast<std::size_t>(copied), destination);
            cursor += copied;
            returned += copied;
            return copied;
        };
        QString growthError{};
        const auto read = ui::recovery::detail::readRecoveryBytesBoundedForTesting(
            kExpectedBytes, reader, &growthError);
        check(!read && returned == kExpectedBytes + 1
                && exactRecoveryText(growthError, QStringLiteral("Recovery container grew while it was being read.")),
            "document_recovery bounded reader rejects concurrent growth after reading only admitted size plus one byte");
    }

    {
        constexpr qint64 kExpectedBytes = 100000;
        QByteArray shortSource(static_cast<qsizetype>(kExpectedBytes - 7), 's');
        qint64 cursor{};
        const auto reader = [&shortSource, &cursor](
            char* const destination, const qint64 count)
        {
            if (cursor >= shortSource.size()) return qint64{0};
            const qint64 available = static_cast<qint64>(shortSource.size()) - cursor;
            const qint64 copied = std::min(count, available);
            std::copy_n(shortSource.constData() + cursor,
                static_cast<std::size_t>(copied), destination);
            cursor += copied;
            return copied;
        };
        QString shortError{};
        const auto shortRead = ui::recovery::detail::readRecoveryBytesBoundedForTesting(
            kExpectedBytes, reader, &shortError);
        int errorCalls{};
        QString deviceError{};
        const auto failedRead = ui::recovery::detail::readRecoveryBytesBoundedForTesting(
            4096,
            [&errorCalls](char*, qint64)
            {
                ++errorCalls;
                return qint64{-1};
            },
            &deviceError);
        check(!shortRead && cursor == shortSource.size()
                && exactRecoveryText(shortError, QStringLiteral("Recovery container ended before its admitted size was read."))
                && !failedRead && errorCalls == 1
                && exactRecoveryText(deviceError, QStringLiteral("Recovery container read failed before its admitted size was read.")),
            "document_recovery bounded reader rejects short and device-error reads cleanly");
    }

    {
        int calls{};
        QString sizeError{};
        const auto oversized = ui::recovery::detail::readRecoveryBytesBoundedForTesting(
            static_cast<qint64>(assets::SedMaximumFileBytes + 1u),
            [&calls](char*, qint64)
            {
                ++calls;
                return qint64{0};
            },
            &sizeError);
        check(!oversized && calls == 0
                && exactRecoveryText(sizeError, QStringLiteral("Recovery container size is invalid.")),
            "document_recovery bounded reader rejects over-limit initial size before allocation or I/O");
    }

    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(), "document_recovery creates an isolated test directory");
    if (!temporaryDirectory.isValid()) return;

    QByteArray resilientSource(3 * 4096, '\0');
    for (qsizetype i = 0; i < resilientSource.size(); ++i)
    {
        resilientSource[i] = static_cast<char>((i * 37) & 0xff);
    }
    std::array<int, 3> resilientAttempts{};
    const ui::recovery::RecoveryRangeReader injectedReader = [
        &resilientSource, &resilientAttempts](
            const std::uint64_t offset,
            const std::span<std::byte> destination)
    {
        const std::size_t block = static_cast<std::size_t>(offset / 4096);
        ++resilientAttempts.at(block);
        if (block == 1)
        {
            return false;
        }
        std::copy_n(
            reinterpret_cast<const std::byte*>(resilientSource.constData())
                + static_cast<std::ptrdiff_t>(offset),
            destination.size(),
            destination.begin());
        return true;
    };
    QString resilientReadError{};
    const auto resilientRead = ui::recovery::readRecoveryContainerResilient(
        static_cast<std::uint64_t>(resilientSource.size()),
        injectedReader,
        4096,
        3,
        &resilientReadError);
    check(resilientRead && resilientReadError.isEmpty()
            && resilientRead->unreadableRanges
                == std::vector<ui::recovery::RecoveryUnreadableRange>{ { 4096, 4096 } }
            && resilientAttempts == std::array<int, 3>{ 1, 3, 1 }
            && resilientRead->bytes.left(4096) == resilientSource.left(4096)
            && resilientRead->bytes.mid(4096, 4096) == QByteArray(4096, '\0')
            && resilientRead->bytes.right(4096) == resilientSource.right(4096),
        "document_recovery retries unreadable 4 KiB ranges, zero-fills erasures, and continues reading later blocks");

    int oversizedReadAttempts{};
    QString oversizedReadError{};
    const auto oversizedRead = ui::recovery::readRecoveryContainerResilient(
        assets::SedMaximumFileBytes + 1u,
        [&oversizedReadAttempts](const std::uint64_t, const std::span<std::byte>)
        {
            ++oversizedReadAttempts;
            return true;
        },
        4096,
        3,
        &oversizedReadError);
    check(assets::SedMaximumFileBytes == 256ull * 1024ull * 1024ull
            && !oversizedRead && oversizedReadAttempts == 0
            && exactRecoveryText(oversizedReadError, QStringLiteral("Recovery resilient-read size or retry options are invalid.")),
        "document_recovery rejects containers above the shared practical 256 MiB cap before allocating or invoking the range reader");

    // Persistence must not inherit the smaller interactive undo limit. A DTO
    // just beyond the GUI cap must still serialize and decode losslessly; an
    // application that installs it in Document may then retain only the 1,024
    // entries nearest its cursor.
    editor::DocumentState persistenceState{};
    ui::project::applyNewProjectTypeDefaults(
        persistenceState.level, ProjectType::MOTS);
    persistenceState.level.masterCMP = "dflt.cmp";
    const Level persistenceLevel = persistenceState.level;
    editor::DocumentRecoveryHistory persistenceHistory{};
    constexpr std::size_t beyondGuiHistoryCount =
        editor::DocumentMaximumHistoryEntries + 1;
    persistenceHistory.undoEntries.reserve(beyondGuiHistoryCount);
    for (std::size_t index = 0; index < beyondGuiHistoryCount; ++index)
    {
        editor::DocumentRecoveryHistoryEntry entry{};
        entry.label = "portable history " + std::to_string(index);
        entry.dirtyFlags = static_cast<std::uint32_t>(editor::DocumentDirty::Things);
        entry.state = persistenceState;
        persistenceHistory.undoEntries.push_back(std::move(entry));
    }
    const QByteArray beyondGuiHistoryPayload =
        ui::recovery::serializeEditorDocumentHistory(
            persistenceLevel, persistenceHistory);
    QString beyondGuiHistoryError{};
    const auto beyondGuiHistoryDecoded =
        ui::recovery::deserializeEditorDocumentHistory(
            beyondGuiHistoryPayload,
            ProjectType::MOTS,
            persistenceLevel,
            &beyondGuiHistoryError);
    check(!beyondGuiHistoryPayload.isEmpty()
            && beyondGuiHistoryDecoded
            && beyondGuiHistoryError.isEmpty()
            && beyondGuiHistoryDecoded->undoEntries.size() == beyondGuiHistoryCount,
        "document_recovery backend reads and writes histories larger than the 1,024-entry GUI cap");

    for (const ProjectType kind : {
             ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM })
    {
        const editor::Document source = makeDeepHistoryDocument(kind, 12);
        const editor::DocumentRecoveryHistory expected = source.recoveryHistory();
        const QByteArray streamed = ui::recovery::serializeEditorDocumentHistory(source);
        editor::Document saveSource = source;
        for (Thing& thing : saveSource.level().things)
        {
            level::thingSyncNumFrames(thing);
        }
        const QByteArray saveOptimized =
            ui::recovery::serializeEditorDocumentHistoryForSave(saveSource);
        const QByteArray fromDto = ui::recovery::serializeEditorDocumentHistory(
            source.current().level, expected);
        QString historyError{};
        const auto decoded = ui::recovery::deserializeEditorDocumentHistory(
            streamed, kind, source.current().level, &historyError);
        check(!streamed.isEmpty() && streamed == fromDto,
            "document_recovery semantic history pool is deterministic for every project type");
        check(decoded && historyError.isEmpty()
                && decoded->undoEntries.size() == expected.undoEntries.size()
                && decoded->redoEntries.size() == expected.redoEntries.size()
                && verifyRestoredHistory(source, *decoded),
            "document_recovery restores exact labels, dirty flags, selections, Levels, Undo, and Redo for every project type");
        QString saveHistoryError{};
        const auto saveDecoded = ui::recovery::deserializeEditorDocumentHistory(
            saveOptimized, kind, saveSource.current().level, &saveHistoryError);
        const auto entriesMatch = [](const auto& actual, const auto& reference)
        {
            return actual.size() == reference.size()
                && std::equal(actual.begin(), actual.end(), reference.begin(),
                    [](const editor::DocumentRecoveryHistoryEntry& lhs,
                       const editor::DocumentRecoveryHistoryEntry& rhs)
                    {
                        return lhs.label == rhs.label
                            && lhs.dirtyFlags == rhs.dirtyFlags;
                    });
        };
        const bool saveHistoryValid = saveDecoded && saveHistoryError.isEmpty()
            && entriesMatch(saveDecoded->undoEntries, expected.undoEntries)
            && entriesMatch(saveDecoded->redoEntries, expected.redoEntries);
        std::string saveHistoryCheck =
            "document_recovery optimized manual-Save HIST remains fully decodable with the exact timeline for every project type";
        if (!saveHistoryValid)
        {
            saveHistoryCheck += ": ";
            saveHistoryCheck += saveHistoryError.toStdString();
            if (saveDecoded)
            {
                saveHistoryCheck += " decoded="
                    + std::to_string(saveDecoded->undoEntries.size()) + "/"
                    + std::to_string(saveDecoded->redoEntries.size())
                    + " expected=" + std::to_string(expected.undoEntries.size())
                    + "/" + std::to_string(expected.redoEntries.size());
            }
        }
        check(saveHistoryValid, saveHistoryCheck.c_str());
    }

    // Structural commands retain a compact post-operation selection for
    // History navigation.  The ordinary historical Undo state contains the
    // pre-cleave selection, so exercise the on-disk hint explicitly after the
    // live selection has changed and ensure only the resulting Sector is
    // restored as the navigation target.
    {
        editor::Document structural = makeDeepHistoryDocument(
            ProjectType::IJIM, 0, 0);
        const std::int32_t oldSector = 0;
        const std::int32_t newSector = static_cast<std::int32_t>(
            structural.level().sectors.size());
        structural.selection().clear();
        structural.selection().sectors.insert(oldSector);
        const bool applied = structural.applyUndoable(
            "Sector: Cleaved",
            editor::DocumentDirty::Structure
                | editor::DocumentDirty::LevelGeometry,
            [newSector](editor::DocumentState& state)
            {
                state.level.sectors.push_back(state.level.sectors.front());
                state.selection.clear();
                state.selection.sectors.insert(newSector);
                return true;
            });
        structural.selection().clear();
        structural.selection().sectors.insert(oldSector);

        const QByteArray payload =
            ui::recovery::serializeEditorDocumentHistory(structural);
        QString decodeError{};
        auto decoded = ui::recovery::deserializeEditorDocumentHistory(
            payload, ProjectType::IJIM, structural.current().level,
            &decodeError);
        editor::Document restored(structural.current());
        if (decoded)
        {
            restored.restoreRecoveryHistory(
                structural.current(), std::move(*decoded));
        }
        const auto transition = restored.transitionAtDepth(
            restored.undoDepth());
        QCborArray invalidSector{};
        invalidSector.append(99'999);
        const QByteArray malformedNavigation = historyWithSelectionOverride(
            payload, QStringLiteral("sectors"), invalidSector, 4);
        QString malformedError{};
        const auto malformedDecoded =
            ui::recovery::deserializeEditorDocumentHistory(
                malformedNavigation, ProjectType::IJIM,
                structural.current().level, &malformedError);
        check(applied && !payload.isEmpty() && decodeError.isEmpty()
                && transition
                && transition->afterSelection.sectors.size() == 1
                && transition->afterSelection.sectors.contains(newSector),
            "document_recovery round-trips the exact resulting Sector navigation target for structural history");
        check(!malformedNavigation.isEmpty() && !malformedDecoded
                && exactRecoveryText(malformedError, QStringLiteral("Recovery undo history navigation selection references missing or incompatible adjacent Level objects.")),
            "document_recovery rejects a structural History navigation target outside its exact adjacent post-operation Level");
    }

    // Preserve the unreleased V1 HIST fragment contract at its non-obvious
    // final boundary: splitGeneratedSedFileSections() attaches the 48-byte
    // checked header-mirror/footer to hnod, while HIST deliberately keeps the
    // last 64 bytes atomic (the final 16 hnod bytes plus that framing).
    {
        bool exactAtEveryHnodSize = true;
        std::array<std::size_t, 3> observedFrameSizes{};
        const std::array<std::size_t, 3> hnodCounts{ 0u, 1u, 3u };
        for (std::size_t fixture = 0; fixture < hnodCounts.size(); ++fixture)
        {
            editor::Document opaqueTailSource =
                makeDeepHistoryDocument(ProjectType::IJIM, 0, 0);
            opaqueTailSource.level().h3doNodes.clear();
            for (std::size_t node = 0; node < hnodCounts[fixture]; ++node)
            {
                HNode value{};
                value.id = static_cast<std::int32_t>(4'000u + node);
                value.name = "tail-node-" + std::to_string(node);
                opaqueTailSource.level().h3doNodes.push_back(std::move(value));
            }
            const assets::SedGeneratedUnprotectedDocument generated =
                assets::generateUnprotectedSedDocument(
                    opaqueTailSource.current().level);
            const auto semantic = assets::splitGeneratedSedFileSections(
                generated.bytes,
                assets::SedSerializationProfile::HistoryNested);
            const auto fragments =
                ui::recovery::detail::generatedHistoryFragmentsForTesting(
                    opaqueTailSource.current().level);
            QByteArray reconstructed{};
            for (const QByteArray& fragment : fragments)
                reconstructed.append(fragment);
            const auto& hnodFrame =
                *generated.requiredSections.exactFrames().back();
            observedFrameSizes[fixture] = hnodFrame.size();
            const QByteArray finalSemantic(
                reinterpret_cast<const char*>(semantic.back().data()),
                static_cast<qsizetype>(semantic.back().size()));
            const qsizetype suffixBytes = finalSemantic.size()
                - static_cast<qsizetype>(hnodFrame.size());
            exactAtEveryHnodSize = exactAtEveryHnodSize
                && suffixBytes == 48
                && !fragments.empty()
                && fragments.back().size() == 64
                && fragments.back() == finalSemantic.right(64)
                && reconstructed == QByteArray(
                    reinterpret_cast<const char*>(generated.bytes.data()),
                    static_cast<qsizetype>(generated.bytes.size()));
        }
        check(exactAtEveryHnodSize
                && observedFrameSizes[0] < observedFrameSizes[1]
                && observedFrameSizes[1] < observedFrameSizes[2],
            "document_recovery HIST preserves exact hnod plus 48-byte trailing framing across the legacy atomic 64-byte boundary");
    }

    {
        const editor::Document source =
            makeDeepHistoryDocument(ProjectType::JKDF2, 0, 0);
        editor::DocumentRecoveryHistory mismatched{};
        editor::DocumentRecoveryHistoryEntry entry{};
        entry.label = "mismatched project";
        entry.state = source.current();
        entry.state.level.kind = ProjectType::IJIM;
        mismatched.undoEntries.push_back(std::move(entry));
        bool rejected = false;
        try
        {
            (void)ui::recovery::serializeEditorDocumentHistory(
                source.current().level, mismatched);
        }
        catch (const std::exception& error)
        {
            rejected = exactRecoveryText(QString::fromUtf8(error.what()), QStringLiteral("Recovery history entry project type disagrees with its save-time Level base."));
        }
        check(rejected,
            "document_recovery HIST rejects a cross-project state before reusing base framing or frame owners");
    }

    // Byte-for-byte differential coverage for every live Snapshot variant.
    // The DTO writer deliberately has no transition-domain hint and therefore
    // regenerates complete nested Levels; equality proves the incremental
    // writer's exact generated-frame reuse cannot alter canonical HIST.
    for (const ProjectType kind : {
             ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM })
    {
        editor::Document variants = makeDeepHistoryDocument(kind, 0, 0);

        variants.pushUndo("full with narrow-looking flags",
            editor::DocumentDirty::Things | editor::DocumentDirty::Cogs);
        variants.level().lvisstring = "full snapshot changed";

        variants.pushUndo("things", editor::DocumentDirty::Things);
        variants.level().things.front().x += 1.0;

        variants.pushThingSubsetUndo("thing subset resource change",
            editor::DocumentDirty::Things, { 0 });
        variants.level().things.front().name += "-new-resource";

        variants.pushThingTailUndo("thing tail",
            editor::DocumentDirty::Structure | editor::DocumentDirty::Things);
        Thing appended = variants.level().things.front();
        appended.id += 100'000;
        appended.num = static_cast<std::int32_t>(variants.level().things.size());
        appended.name = "history-appended-resource";
        variants.level().things.push_back(std::move(appended));

        variants.pushUndo("cogs", editor::DocumentDirty::Cogs);
        ++variants.level().cogs.front().num;

        variants.pushUndo("lights", editor::DocumentDirty::Lights);
        variants.level().lights.front().range += 1.0;
        variants.level().sectors.front().ambient.r += 0.01f;

        variants.pushUndo("things lights",
            editor::DocumentDirty::Things | editor::DocumentDirty::Lights);
        variants.level().things.front().z += 2.0;
        variants.level().lights.front().intensity += 3.0;
        variants.level().sectors.front().ambient.g += 0.02f;

        variants.pushSectorSubsetUndo("sector subset",
            editor::DocumentDirty::LevelGeometry, { 0 });
        variants.level().sectors.front().vertices.front().x += 0.5;

        variants.pushSurfaceSubsetUndo("surface subset",
            editor::DocumentDirty::LevelGeometry, { 0 });
        variants.level().sectors.front().surfaces.front().extraLight.b += 0.03f;

        const bool redoA = variants.undo();
        const bool redoB = variants.undo();
        const editor::DocumentRecoveryHistory portable =
            variants.recoveryHistory();
        const QByteArray incremental =
            ui::recovery::serializeEditorDocumentHistory(variants);
        const QByteArray complete =
            ui::recovery::serializeEditorDocumentHistory(
                variants.current().level, portable);
        check(redoA && redoB && !incremental.isEmpty()
                && incremental == complete,
            "document_recovery incremental HIST writer is byte-identical for every Snapshot variant, project, resource change, and both stacks");
    }

    // Fixed-offset HIST blocks must localize a topology-preserving one-Sector
    // change.  The `sect` frame is intentionally much larger than one block;
    // comparing only the changed block(s) must not re-fragment the complete
    // frame, while decoding must still reproduce the exact canonical nested
    // Level bytes of the historical state.
    {
        editor::Document localized = makeDeepHistoryDocument(
            ProjectType::MOTS, 0, 0, false, 0, 12, 256, 8);
        localized.pushSectorSubsetUndo(
            "localized sector block", editor::DocumentDirty::LevelGeometry, { 0 });
        localized.level().sectors.front().vertices.front().x += 0.375;
        ui::recovery::detail::RecoveryHistoryEncodeStats stats{};
        QString error{};
        QByteArray payload{};
        try
        {
            payload = ui::recovery::detail::serializeEditorDocumentHistoryWithStatsForTesting(
                localized, &stats);
        }
        catch (const std::exception& exception)
        {
            error = QString::fromUtf8(exception.what());
        }
        const auto generated = assets::generateUnprotectedSedDocument(
            localized.current().level);
        const auto& sectFrame = generated.requiredSections.exactFrames()[2];
        bool sectorRecordSpansValid = false;
        if (sectFrame)
        {
            try
            {
                const auto spans = assets::detail::splitHistoryNestedSectorFrameRecords(
                    std::span<const std::byte>(*sectFrame));
                std::size_t cursor = 100u + 4u;
                sectorRecordSpansValid = true;
                for (const auto& span : spans)
                {
                    sectorRecordSpansValid = sectorRecordSpansValid
                        && span.begin == cursor
                        && span.end > span.begin
                        && span.end <= sectFrame->size();
                    cursor = span.end;
                }
                // The record spans stop before the checked trailer; the
                // frame prefix/count and trailer are intentionally retained
                // as bounded bridge fragments by the HIST writer.
                sectorRecordSpansValid = sectorRecordSpansValid
                    && cursor + 116u == sectFrame->size();
            }
            catch (const std::exception&)
            {
                sectorRecordSpansValid = false;
            }
        }
        const auto decoded = error.isEmpty()
            ? ui::recovery::deserializeEditorDocumentHistory(
                payload, ProjectType::MOTS, localized.current().level, &error)
            : std::optional<editor::DocumentRecoveryHistory>{};
        const auto expected = localized.recoveryHistory();
        const bool exactNested = decoded && !expected.undoEntries.empty()
            && canonicalNestedLevel(decoded->undoEntries.front().state.level)
                == canonicalNestedLevel(expected.undoEntries.front().state.level);
        check(error.isEmpty() && !payload.isEmpty() && sectFrame
                && sectorRecordSpansValid
                && stats.fragmentedBytesBySection[2] > 0
                && stats.directRecordReusedBytesBySection[2] > 0
                && stats.fragmentedBytesBySection[2]
                    < static_cast<std::uint64_t>(sectFrame->size())
                && exactNested,
            "document_recovery fixed HIST blocks localize a one-Sector sect change and reconstruct exact nested bytes");
    }

    // The splitter must also retain the empty-vector and one-record cases;
    // the latter is deliberately tiny while the localized fixture above
    // exercises many records and a record larger than one 8 KiB fragment.
    {
        const auto sectorRecordCount = [](const int sectorCount)
        {
            const editor::Document document = makeDeepHistoryDocument(
                ProjectType::MOTS, 0, 0, false, 0, sectorCount,
                sectorCount == 0 ? 0 : 1, 0);
            const auto generated = assets::generateUnprotectedSedDocument(
                document.current().level);
            const auto& frame = generated.requiredSections.exactFrames()[2];
            if (!frame) return std::size_t{ std::numeric_limits<std::size_t>::max() };
            try
            {
                return assets::detail::splitHistoryNestedSectorFrameRecords(
                    std::span<const std::byte>(*frame)).size();
            }
            catch (const std::exception&)
            {
                return std::size_t{ std::numeric_limits<std::size_t>::max() };
            }
        };
        check(sectorRecordCount(0) == 0 && sectorRecordCount(1) == 1,
            "document_recovery SECT record splitter handles empty and single-sector frames");

        // The bounded writer seam must reject before the core splitter
        // reserves/materializes a record vector.  A zero budget is a small
        // synthetic stand-in for a pathological frame with hundreds of
        // thousands of tiny sectors; the production limit is exercised by
        // the same maximumRecords argument without allocating that fixture.
        const editor::Document bounded = makeDeepHistoryDocument(
            ProjectType::MOTS, 0, 0, false, 0, 1, 1, 0);
        const auto boundedGenerated = assets::generateUnprotectedSedDocument(
            bounded.current().level);
        const auto& boundedFrame =
            boundedGenerated.requiredSections.exactFrames()[2];
        bool boundedRejectedBeforeMaterialization = false;
        if (boundedFrame)
        {
            try
            {
                (void)assets::detail::splitHistoryNestedSectorFrameRecords(
                    std::span<const std::byte>(*boundedFrame), 0);
            }
            catch (const std::length_error&)
            {
                boundedRejectedBeforeMaterialization = true;
            }
        }
        check(boundedRejectedBeforeMaterialization,
            "document_recovery bounded SECT splitter rejects over-budget record vectors early");
    }

    // A non-SECT edit must reuse the complete canonical SECT frame.  This is
    // both a no-op regression check for the record-oriented base layout and a
    // guard against accidentally repooling geometry when only Things change.
    {
        editor::Document thingOnly = makeDeepHistoryDocument(
            ProjectType::MOTS, 0, 0, false, 0, 12, 256, 8);
        thingOnly.pushUndo("Thing-only history edit", editor::DocumentDirty::Things);
        thingOnly.level().things.front().x += 0.5;
        ui::recovery::detail::RecoveryHistoryEncodeStats stats{};
        QString error{};
        const QByteArray payload =
            ui::recovery::detail::serializeEditorDocumentHistoryWithStatsForTesting(
                thingOnly, &stats);
        const auto decoded = ui::recovery::deserializeEditorDocumentHistory(
            payload, ProjectType::MOTS, thingOnly.current().level, &error);
        check(error.isEmpty() && decoded && !payload.isEmpty()
                && stats.fragmentedBytesBySection[2] == 0
                && stats.reusedFramesBySection[2] != 0,
            "document_recovery reuses the record-oriented SECT base for a Thing-only history edit");
    }

    // A size-changing geometry edit must resynchronize at a prior block
    // boundary instead of copying the entire shifted tail, while retaining
    // the same canonical bytes and complete state semantics.
    {
        editor::Document resized = makeDeepHistoryDocument(
            ProjectType::IJIM, 0, 0, false, 0, 4, 96, 4);
        resized.pushUndo(
            "resized geometry", editor::DocumentDirty::LevelGeometry
                | editor::DocumentDirty::Structure);
        Sector& sector = resized.level().sectors.front();
        for (int index = 0; index < 37; ++index)
        {
            sector.vertices.push_back(Vertex{
                100.0 + index, 200.0 + index, 300.0 + index, index % 5 });
        }
        QString error{};
        ui::recovery::detail::RecoveryHistoryEncodeStats resizeStats{};
        const QByteArray payload =
            ui::recovery::detail::serializeEditorDocumentHistoryWithStatsForTesting(
                resized, &resizeStats);
        const auto decoded = ui::recovery::deserializeEditorDocumentHistory(
            payload, ProjectType::IJIM, resized.current().level, &error);
        const auto expected = resized.recoveryHistory();
        const auto generated = assets::generateUnprotectedSedDocument(
            resized.current().level);
        const auto& resizedSectFrame = generated.requiredSections.exactFrames()[2];
        const bool resizedExact = decoded && !expected.undoEntries.empty()
            && canonicalNestedLevel(decoded->undoEntries.front().state.level)
                == canonicalNestedLevel(expected.undoEntries.front().state.level);
        check(!payload.isEmpty() && decoded && error.isEmpty()
                && !expected.undoEntries.empty()
                && resizedExact && resizedSectFrame
                && resizeStats.fragmentedBytesBySection[2] > 0
                && resizeStats.fragmentedBytesBySection[2]
                    < static_cast<std::uint64_t>(resizedSectFrame->size()),
            "document_recovery resynchronizes shifted sector tails while preserving exact size-changing geometry");
    }

    // Consecutive structural edits must carry the exact variable fragment
    // spans forward.  A fixed-block-only implementation would rescan/repool
    // the complete shifted `sect` frame on the second edit, and a same-size
    // edit after that would index the variable reference list as if it were
    // fixed.  This timeline exercises grow, shrink, grow, then a localized
    // same-size geometry edit in one direction of the undo walk.
    {
        editor::Document chained = makeDeepHistoryDocument(
            ProjectType::MOTS, 0, 0, false, 0, 12, 256, 8);
        auto appendVertices = [&chained](const int count, const double base)
        {
            for (int index = 0; index < count; ++index)
            {
                chained.level().sectors.front().vertices.push_back(Vertex{
                    base + index, base + index * 0.5, base - index, index % 7 });
            }
        };
        chained.pushUndo("chained grow one",
            editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure);
        appendVertices(19, 100.0);
        chained.pushUndo("chained grow two",
            editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure);
        appendVertices(23, 200.0);
        chained.pushUndo("chained shrink",
            editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure);
        for (int index = 0; index < 11; ++index)
        {
            chained.level().sectors.front().vertices.pop_back();
        }
        chained.pushUndo("chained same-size edit", editor::DocumentDirty::LevelGeometry);
        chained.level().sectors.front().vertices.front().x += 0.625;

        ui::recovery::detail::RecoveryHistoryEncodeStats chainedStats{};
        QString chainedError{};
        QByteArray chainedPayload{};
        try
        {
            chainedPayload =
                ui::recovery::detail::serializeEditorDocumentHistoryWithStatsForTesting(
                    chained, &chainedStats);
        }
        catch (const std::exception& exception)
        {
            chainedError = QString::fromUtf8(exception.what());
        }
        const auto chainedDecoded = chainedError.isEmpty()
            ? ui::recovery::deserializeEditorDocumentHistory(
                chainedPayload, ProjectType::MOTS, chained.current().level,
                &chainedError)
            : std::optional<editor::DocumentRecoveryHistory>{};
        const auto chainedGenerated = assets::generateUnprotectedSedDocument(
            chained.current().level);
        const auto& chainedSect = chainedGenerated.requiredSections.exactFrames()[2];
        const auto chainedExact = chainedDecoded
            && verifyRestoredHistory(chained, *chainedDecoded);
        const QByteArray chainedDto = chainedError.isEmpty()
            ? ui::recovery::serializeEditorDocumentHistory(
                chained.current().level, chained.recoveryHistory())
            : QByteArray{};
        check(chainedError.isEmpty() && chainedExact && !chainedPayload.isEmpty()
                && chainedPayload == chainedDto
                && chainedSect
                && chainedStats.fragmentedFramesBySection[2] >= 4
                && chainedStats.fragmentedBytesBySection[2]
                    < static_cast<std::uint64_t>(chainedSect->size()),
            "document_recovery chains shifted sector spans across grow/shrink and same-size edits without repooling the complete sect frame");
    }

    // A late-sector structural edit must preserve both sides of the splice:
    // the large same-offset prefix is carried by direct prior references and
    // the unchanged tail is carried by shifted prior spans.  Routing the
    // prefix through referenceFor() would report almost the complete frame as
    // fragmented even though only the late sector and checked trailer changed.
    {
        editor::Document lateSector = makeDeepHistoryDocument(
            ProjectType::MOTS, 0, 0, false, 0, 24, 256, 8);
        lateSector.pushUndo(
            "late-sector grow", editor::DocumentDirty::LevelGeometry
                | editor::DocumentDirty::Structure);
        Sector& sector = lateSector.level().sectors[
            lateSector.level().sectors.size() - 2u];
        for (int index = 0; index < 37; ++index)
        {
            sector.vertices.push_back(Vertex{
                500.0 + index, 600.0 + index, 700.0 + index, index % 5 });
        }
        ui::recovery::detail::RecoveryHistoryEncodeStats lateStats{};
        QString lateError{};
        const QByteArray latePayload =
            ui::recovery::detail::serializeEditorDocumentHistoryWithStatsForTesting(
                lateSector, &lateStats);
        const auto lateDecoded = ui::recovery::deserializeEditorDocumentHistory(
            latePayload, ProjectType::MOTS, lateSector.current().level,
            &lateError);
        const auto lateGenerated = assets::generateUnprotectedSedDocument(
            lateSector.current().level);
        const auto& lateSect = lateGenerated.requiredSections.exactFrames()[2];
        const bool lateExact = lateDecoded
            && verifyRestoredHistory(lateSector, *lateDecoded);
        const QByteArray lateDto = lateError.isEmpty()
            ? ui::recovery::serializeEditorDocumentHistory(
                lateSector.current().level, lateSector.recoveryHistory())
            : QByteArray{};
        check(lateError.isEmpty() && lateExact && lateSect
                && latePayload == lateDto
                && (lateStats.resynchronizedPrefixBytesBySection[2] > 64 * 1024
                    || lateStats.directRecordReusedBytesBySection[2] > 64 * 1024)
                && (lateStats.resynchronizedSuffixBytesBySection[2] > 64
                    || lateStats.directRecordReusedBytesBySection[2] > 64)
                && lateStats.fragmentedBytesBySection[2]
                    < static_cast<std::uint64_t>(lateSect->size()) / 4,
            "document_recovery reuses direct prefix and shifted suffix spans for a late-sector structural edit");
    }

    // Real edit-mode ordering regression.  Structural surface edits use full
    // snapshots while Thing edits use compact domain snapshots.  Exercise
    // both directions (Thing -> Surface -> Thing and Surface -> Thing ->
    // Surface) and leave a redo branch so the native HIST restore path proves
    // that mixed stacks are not silently discarded.
    for (const ProjectType kind : {
             ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM })
    {
        editor::Document mixed = makeDeepHistoryDocument(kind, 0, 0);
        auto applyThing = [&mixed](const std::string& label, const double x)
        {
            return mixed.applyUndoable(
                label, editor::DocumentDirty::Things,
                [x](editor::DocumentState& state)
                {
                    state.level.things.front().x = x;
                    state.selection.clear();
                    state.selection.things.insert(0);
                    return true;
                });
        };
        auto applySurface = [&mixed](const std::string& label, const std::string& material)
        {
            return mixed.applyUndoable(
                label,
                editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure,
                [material](editor::DocumentState& state)
                {
                    state.level.sectors.front().surfaces.front().material = material;
                    state.selection.clear();
                    state.selection.surfaces.insert(SurfaceRef{ 0, 0 });
                    return true;
                });
        };

        const bool firstThing = applyThing("Thing before surface", 10.0);
        const bool firstSurface = applySurface("Surface cleave", "mixed-surface-a.mat");
        const bool secondThing = applyThing("Thing between surfaces", 20.0);
        const bool secondSurface = applySurface("Surface multi-cut", "mixed-surface-b.mat");
        // Undo the final Surface edit. The current state is therefore a Thing
        // edit with a structural Surface entry on the redo side.
        const bool madeRedo = mixed.undo();
        const editor::DocumentRecoveryHistory expected = mixed.recoveryHistory();
        const QByteArray payload = ui::recovery::serializeEditorDocumentHistory(mixed);
        QString mixedError{};
        const auto decoded = ui::recovery::deserializeEditorDocumentHistory(
            payload, kind, mixed.current().level, &mixedError);
        check(firstThing && firstSurface && secondThing && secondSurface && madeRedo
                && decoded && mixedError.isEmpty()
                && decoded->undoEntries.size() == expected.undoEntries.size()
                && decoded->redoEntries.size() == expected.redoEntries.size()
                && verifyRestoredHistory(mixed, *decoded),
            "document_recovery preserves mixed Thing/Surface undo and redo ordering for every project type");
    }

    // Use the real cube/cleave implementation as a second fixture.  The
    // compact synthetic test above intentionally exercises only the codec;
    // this one keeps the topology and selections that a viewport cleave
    // actually produces.  In particular, leave the current state on a
    // structural Surface edit in both edit-mode orderings: this is the case
    // that previously caused native HIST to disappear on Save/Open.
    const auto makeCleaveFixture = [](const ProjectType kind)
    {
        editor::DocumentState state{};
        state.level.kind = kind;
        state.level.layers = { "Layer0" };
        state.level.header.version = kind == ProjectType::IJIM ? 3 : 1;
        Sector& sector = sed::level::levelCreateCube(
            state.level, 0.0, 0.0, 0.0, TVector::up, TVector::right);
        sector.layer = 0;
        Thing thing{};
        thing.id = 100;
        thing.num = 0;
        thing.layer = 0;
        thing.x = -1.0;
        state.level.things.push_back(std::move(thing));
        return editor::Document(std::move(state));
    };
    const auto applySurfaceCleave = [](editor::DocumentState& state)
    {
        std::int32_t sourceSurface = -1;
        for (std::size_t i = 0; i < state.level.sectors.front().surfaces.size(); ++i)
        {
            const Surface& surface = state.level.sectors.front().surfaces[i];
            bool lower = false;
            bool upper = false;
            for (const std::int32_t vertexIndex : surface.vertices)
            {
                const double x = state.level.sectors.front().vertices[
                    static_cast<std::size_t>(vertexIndex)].x;
                lower = lower || x < 0.5 - sed::math::kGeometryEpsilon;
                upper = upper || x > 0.5 + sed::math::kGeometryEpsilon;
            }
            if (lower && upper)
            {
                sourceSurface = static_cast<std::int32_t>(i);
                break;
            }
        }
        if (sourceSurface < 0
            || !sed::level::levelCleaveSurface(
                state.level, SurfaceRef{ 0, sourceSurface },
                TVector::right, 0.5, 0.0, 0.0))
        {
            return false;
        }
        state.selection.clear();
        state.selection.surfaces.insert(SurfaceRef{
            0, static_cast<std::int32_t>(state.level.sectors.front().surfaces.size() - 1) });
        return true;
    };
    for (const ProjectType kind : {
             ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM })
    {
        editor::Document thingThenSurface = makeCleaveFixture(kind);
        const bool thingA = thingThenSurface.applyUndoable(
            "Thing before cleave", editor::DocumentDirty::Things,
            [](editor::DocumentState& state)
            {
                state.level.things.front().x = 2.0;
                state.selection.clear();
                state.selection.things.insert(0);
                return true;
            });
        const bool surfaceA = thingThenSurface.applyUndoable(
            "Surface cleave", editor::DocumentDirty::LevelGeometry
                | editor::DocumentDirty::Structure,
            applySurfaceCleave);
        const bool thingB = thingThenSurface.applyUndoable(
            "Thing between cleaves", editor::DocumentDirty::Things,
            [](editor::DocumentState& state)
            {
                state.level.things.front().x = 3.0;
                state.selection.clear();
                state.selection.things.insert(0);
                return true;
            });
        const bool surfaceB = thingThenSurface.applyUndoable(
            "Surface after Thing", editor::DocumentDirty::LevelGeometry
                | editor::DocumentDirty::Structure,
            [](editor::DocumentState& state)
            {
                if (state.level.sectors.front().surfaces.empty()) return false;
                const std::int32_t output = static_cast<std::int32_t>(
                    state.level.sectors.front().surfaces.size() - 1);
                state.level.sectors.front().surfaces[static_cast<std::size_t>(output)].material =
                    "post-thing.mat";
                state.selection.clear();
                state.selection.surfaces.insert(SurfaceRef{ 0, output });
                return true;
            });
        const QByteArray payload = ui::recovery::serializeEditorDocumentHistory(
            thingThenSurface);
        QString error{};
        const auto decoded = ui::recovery::deserializeEditorDocumentHistory(
            payload, kind, thingThenSurface.current().level, &error);
        check(thingA && surfaceA && thingB && surfaceB
                && decoded && error.isEmpty()
                && decoded->undoEntries.size() == 4
                && decoded->redoEntries.empty()
                && verifyRestoredHistory(thingThenSurface, *decoded),
            "document_recovery preserves a model-valid Thing -> cleave -> Thing -> Surface-current history");

        editor::Document surfacesThenThing = makeCleaveFixture(kind);
        const bool reverseSurfaceA = surfacesThenThing.applyUndoable(
            "Initial surface cleave", editor::DocumentDirty::LevelGeometry
                | editor::DocumentDirty::Structure,
            [](editor::DocumentState& state)
            {
                for (std::size_t i = 0; i < state.level.sectors.front().surfaces.size(); ++i)
                {
                    const Surface& surface = state.level.sectors.front().surfaces[i];
                    bool lower = false;
                    bool upper = false;
                    for (const std::int32_t vertexIndex : surface.vertices)
                    {
                        const double x = state.level.sectors.front().vertices[
                            static_cast<std::size_t>(vertexIndex)].x;
                        lower = lower || x < 0.5 - sed::math::kGeometryEpsilon;
                        upper = upper || x > 0.5 + sed::math::kGeometryEpsilon;
                    }
                    if (lower && upper
                        && sed::level::levelCleaveSurface(
                            state.level, SurfaceRef{ 0, static_cast<std::int32_t>(i) },
                            TVector::right, 0.5, 0.0, 0.0))
                    {
                        state.selection.clear();
                        state.selection.surfaces.insert(SurfaceRef{
                            0, static_cast<std::int32_t>(state.level.sectors.front().surfaces.size() - 1) });
                        return true;
                    }
                }
                return false;
            });
        const bool reverseSurfaceB = surfacesThenThing.applyUndoable(
            "Second surface edit", editor::DocumentDirty::LevelGeometry
                | editor::DocumentDirty::Structure,
            [](editor::DocumentState& state)
            {
                const std::int32_t output = static_cast<std::int32_t>(
                    state.level.sectors.front().surfaces.size() - 1);
                state.level.sectors.front().surfaces[static_cast<std::size_t>(output)].material =
                    "second-surface.mat";
                state.selection.clear();
                state.selection.surfaces.insert(SurfaceRef{ 0, output });
                return true;
            });
        const bool reverseThing = surfacesThenThing.applyUndoable(
            "Thing between surfaces", editor::DocumentDirty::Things,
            [](editor::DocumentState& state)
            {
                state.level.things.front().x = 4.0;
                state.selection.clear();
                state.selection.things.insert(0);
                return true;
            });
        const bool reverseSurfaceC = surfacesThenThing.applyUndoable(
            "Final surface edit", editor::DocumentDirty::LevelGeometry
                | editor::DocumentDirty::Structure,
            [](editor::DocumentState& state)
            {
                const std::int32_t output = static_cast<std::int32_t>(
                    state.level.sectors.front().surfaces.size() - 1);
                state.level.sectors.front().surfaces[static_cast<std::size_t>(output)].material =
                    "final-surface.mat";
                state.selection.clear();
                state.selection.surfaces.insert(SurfaceRef{ 0, output });
                return true;
            });
        const QByteArray reversePayload = ui::recovery::serializeEditorDocumentHistory(
            surfacesThenThing);
        QString reverseError{};
        const auto reverseDecoded = ui::recovery::deserializeEditorDocumentHistory(
            reversePayload, kind, surfacesThenThing.current().level, &reverseError);
        check(reverseSurfaceA && reverseSurfaceB && reverseThing && reverseSurfaceC
                && reverseDecoded && reverseError.isEmpty()
                && reverseDecoded->undoEntries.size() == 4
                && reverseDecoded->redoEntries.empty()
                && verifyRestoredHistory(surfacesThenThing, *reverseDecoded),
            "document_recovery preserves a model-valid Surface -> Surface -> Thing -> Surface-current history");
    }

    // Large-base regression for the native consuming path.  Alternating
    // one-Thing and topology-preserving Sector operations must retain only
    // the changed values/owning Sectors after recovery; charging each
    // expanded Level as retained used to omit HIST once this exceeded a
    // realistic map's geometry footprint.
    editor::Document alternatingCompact = makeDeepHistoryDocument(
        ProjectType::MOTS, 0, 0, false, 0, 256, 128, 128);
    for (int edit = 0; edit < 24; ++edit)
    {
        const std::int32_t thingIndex = edit % 128;
        alternatingCompact.pushThingSubsetUndo(
            "Thing transform",
            editor::DocumentDirty::Things,
            std::vector<std::int32_t>{thingIndex});
        alternatingCompact.level().things[static_cast<std::size_t>(thingIndex)].x += 0.5;
        const std::int32_t sectorIndex = edit % 256;
        alternatingCompact.pushSectorSubsetUndo(
            "Sector transform",
            editor::DocumentDirty::LevelGeometry,
            std::vector<std::int32_t>{sectorIndex});
        alternatingCompact.level().sectors[static_cast<std::size_t>(sectorIndex)]
            .vertices.front().x += 0.25;
    }
    const editor::DocumentUndoSnapshotStats liveCompactStats =
        alternatingCompact.undoSnapshotStats();
    QString alternatingError{};
    const QByteArray alternatingPayload =
        ui::recovery::serializeEditorDocumentHistory(alternatingCompact);
    const auto alternatingRecovered =
        ui::recovery::deserializeEditorDocumentHistoryForRestore(
            alternatingPayload,
            ProjectType::MOTS,
            alternatingCompact.current().level,
            &alternatingError);
    const editor::DocumentUndoSnapshotStats recoveredCompactStats =
        alternatingRecovered && alternatingRecovered->compact
        ? [&]()
        {
            editor::Document restored(alternatingCompact.current());
            restored.restoreRecoveryHistory(
                alternatingCompact.current(), *alternatingRecovered);
            return restored.undoSnapshotStats();
        }()
        : editor::DocumentUndoSnapshotStats{};
    check(liveCompactStats.full == 0
            && liveCompactStats.thingSubsets == 24
            && liveCompactStats.sectorSubsets == 24,
        "document_recovery live mixed Thing/Sector timeline uses compact snapshots");
    check(!alternatingPayload.isEmpty()
            && alternatingRecovered
            && alternatingRecovered->compact
            && alternatingError.isEmpty()
            && recoveredCompactStats.full == 0
            && recoveredCompactStats.thingSubsets == 24
            && recoveredCompactStats.sectorSubsets == 24,
        "document_recovery imported mixed Thing/Sector timeline retains compact snapshot kinds");
    check(alternatingRecovered
            && verifyRestoredHistory(alternatingCompact, *alternatingRecovered),
        "document_recovery restores exact large mixed Thing/Sector undo/redo states");

    const editor::Document deepHistory = makeDeepHistoryDocument(ProjectType::MOTS, 72);
    const editor::DocumentRecoveryHistory expandedHistory = deepHistory.recoveryHistory();
    const QByteArray compactHistory = ui::recovery::serializeEditorDocumentHistory(
        deepHistory);
    QCborParserError compactSchemaError{};
    const QCborValue compactSchemaValue = QCborValue::fromCbor(
        compactHistory, &compactSchemaError);
    const QCborMap compactSchema = compactSchemaValue.toMap();
    const auto compactBaseFragments =
        ui::recovery::detail::generatedHistoryFragmentsForTesting(
            deepHistory.current().level);
    QString compactRoundTripError{};
    const auto compactRoundTrip =
        ui::recovery::deserializeEditorDocumentHistory(
            compactHistory, ProjectType::MOTS,
            deepHistory.current().level, &compactRoundTripError);
    check(compactSchemaError.error == QCborError::NoError
            && compactSchema.value(QStringLiteral("encoding")).toString()
                == QStringLiteral("sed-semantic-base-delta-v1"),
        "document_recovery uses the explicit V1 current-base checkpoint/delta schema");
    check(compactSchema.value(QStringLiteral("fragmentLayout")).toString()
                == QStringLiteral("sect-record-v1")
            && compactSchema.value(QStringLiteral("baseCount")).toInteger(-1)
                == static_cast<qint64>(compactBaseFragments.size())
            && compactRoundTrip && compactRoundTripError.isEmpty()
            && verifyRestoredHistory(deepHistory, *compactRoundTrip),
        "document_recovery SECT-record layout matches encoder base references and round-trips");

    // Layout identity is part of the persisted HIST contract.  A small old
    // seven-key payload remains a valid fixed-block V1 vector, while the
    // record-oriented payload must reject an unknown discriminator and must
    // not silently fall back to fixed base numbering.
    const editor::Document legacyFixedDocument = makeDeepHistoryDocument(
        ProjectType::MOTS, 0, 0, false, 0, 0, 0, 0);
    const QByteArray legacySource = ui::recovery::serializeEditorDocumentHistory(
        legacyFixedDocument);
    QCborMap legacyRoot = QCborValue::fromCbor(legacySource).toMap();
    legacyRoot.remove(QStringLiteral("fragmentLayout"));
    const QByteArray legacyFixedHistory = ui::recovery::serializeCanonicalCbor(
        QCborValue(legacyRoot));
    QString legacyFixedError{};
    const auto legacyFixedDecoded =
        ui::recovery::deserializeEditorDocumentHistory(
            legacyFixedHistory, ProjectType::MOTS,
            legacyFixedDocument.current().level, &legacyFixedError);
    check(!legacyFixedHistory.isEmpty() && legacyFixedDecoded
            && legacyFixedError.isEmpty(),
        "document_recovery loads the legacy seven-key fixed-block HIST vector");

    legacyRoot.insert(QStringLiteral("fragmentLayout"),
        QStringLiteral("fixed-v1"));
    const QByteArray explicitFixedHistory = ui::recovery::serializeCanonicalCbor(
        QCborValue(legacyRoot));
    QString explicitFixedError{};
    const auto explicitFixedDecoded =
        ui::recovery::deserializeEditorDocumentHistory(
            explicitFixedHistory, ProjectType::MOTS,
            legacyFixedDocument.current().level, &explicitFixedError);
    check(explicitFixedDecoded && explicitFixedError.isEmpty(),
        "document_recovery accepts the explicit fixed-v1 HIST layout discriminator");

    QCborMap malformedLayoutRoot = compactSchema;
    malformedLayoutRoot.insert(QStringLiteral("fragmentLayout"),
        QStringLiteral("unknown-layout-v1"));
    const QByteArray malformedLayoutHistory =
        ui::recovery::serializeCanonicalCbor(QCborValue(malformedLayoutRoot));
    QString malformedLayoutError{};
    const auto malformedLayoutDecoded =
        ui::recovery::deserializeEditorDocumentHistory(
            malformedLayoutHistory, ProjectType::MOTS,
            deepHistory.current().level, &malformedLayoutError);
    check(!malformedLayoutDecoded
            && exactRecoveryText(malformedLayoutError, QStringLiteral("Recovery undo history schema or fragment layout is unsupported.")),
        "document_recovery rejects an unknown HIST fragment-layout discriminator");

    QCborMap omittedLayoutRoot = compactSchema;
    omittedLayoutRoot.remove(QStringLiteral("fragmentLayout"));
    const QByteArray omittedLayoutHistory =
        ui::recovery::serializeCanonicalCbor(QCborValue(omittedLayoutRoot));
    QString omittedLayoutError{};
    const auto omittedLayoutDecoded =
        ui::recovery::deserializeEditorDocumentHistory(
            omittedLayoutHistory, ProjectType::MOTS,
            deepHistory.current().level, &omittedLayoutError);
    check(!omittedLayoutDecoded,
        "document_recovery does not reinterpret a new SECT-record HIST as fixed-block V1");

    // An empty timeline must not manufacture a Level snapshot or a pool. The
    // authoritative top-level Level remains the only geometry copy.
    const editor::Document emptyHistoryDocument =
        makeDeepHistoryDocument(ProjectType::MOTS, 0);
    const QByteArray emptyHistory = ui::recovery::serializeEditorDocumentHistory(
        emptyHistoryDocument);
    const QCborMap emptyHistoryRoot = QCborValue::fromCbor(emptyHistory).toMap();
    QString emptyHistoryError{};
    const auto emptyHistoryDecoded = ui::recovery::deserializeEditorDocumentHistory(
        emptyHistory, ProjectType::MOTS, emptyHistoryDocument.current().level,
        &emptyHistoryError);
    check(!emptyHistory.isEmpty()
            && emptyHistoryRoot.value(QStringLiteral("pool")).toArray().isEmpty()
            && emptyHistoryRoot.value(QStringLiteral("undoGroups")).toArray().isEmpty()
            && emptyHistoryRoot.value(QStringLiteral("redoGroups")).toArray().isEmpty()
            && emptyHistoryDecoded && emptyHistoryError.isEmpty()
            && emptyHistoryDecoded->undoEntries.empty()
            && emptyHistoryDecoded->redoEntries.empty(),
        "document_recovery empty HIST stores no Level/pool fragments and validates against the current base");

    // A one-edit timeline should store only changed fragments. Its first
    // record is a delta seeded from the current base; the unchanged prefix or
    // suffix is therefore represented implicitly by negative base references.
    // Keep the single edit on the undo stack. The helper's default undoCount
    // intentionally creates a redo branch, which would make this sparse
    // fixture's undoGroups empty and obscure the first-delta representation.
    const editor::Document sparseHistory = makeDeepHistoryDocument(
        ProjectType::MOTS, 1, 0);
    const QByteArray sparsePayload = ui::recovery::serializeEditorDocumentHistory(
        sparseHistory);
    const QCborMap sparseRoot = QCborValue::fromCbor(sparsePayload).toMap();
    const QCborArray sparseGroups = sparseRoot.value(QStringLiteral("undoGroups"))
        .toArray();
    const QCborArray sparseManifest = sparseGroups.isEmpty()
        ? QCborArray{} : sparseGroups.at(0).toArray().at(0).toArray();
    const QCborArray sparseRecord = sparseManifest.size() == 4
        ? sparseManifest.at(3).toArray() : QCborArray{};
    const qint64 sparseBaseCount = sparseRoot.value(QStringLiteral("baseCount"))
        .toInteger(-1);
    check(sparseBaseCount > 0
            && sparseRoot.value(QStringLiteral("pool")).toArray().size()
                < sparseBaseCount
            && sparseRecord.size() == 6
            && sparseRecord.at(1).toInteger(-1) == 1
            && (sparseRecord.at(2).toInteger(0) > 0
                || sparseRecord.at(3).toInteger(0) > 0),
        "document_recovery sparse HIST seeds its first delta from the current base without pooling unchanged fragments");

    // Binding the history to a different Level must fail before any state
    // reconstruction; the top-level Level is still independently usable.
    QCborMap mismatchedBaseRoot = sparseRoot;
    mismatchedBaseRoot.insert(QStringLiteral("baseCount"), sparseBaseCount + 1);
    const QByteArray mismatchedBasePayload = ui::recovery::serializeCanonicalCbor(
        QCborValue(mismatchedBaseRoot));
    QString mismatchedBaseError{};
    check(!ui::recovery::deserializeEditorDocumentHistory(
                mismatchedBasePayload, ProjectType::MOTS,
                sparseHistory.current().level, &mismatchedBaseError)
            && exactRecoveryText(mismatchedBaseError, QStringLiteral("Recovery undo history base digest or fragment count does not match the current Level.")),
        "document_recovery rejects HIST whose base digest/count does not match the top-level Level");

    const editor::Document longHistory =
        makeDeepHistoryDocument(ProjectType::MOTS, 40, 0);
    const QByteArray longHistoryPayload = ui::recovery::serializeEditorDocumentHistory(
        longHistory);
    const QCborMap longHistoryRoot = QCborValue::fromCbor(longHistoryPayload).toMap();
    bool checkpointUsesBaseReference = false;
    for (const QCborValue& groupValue : longHistoryRoot.value(
             QStringLiteral("undoGroups")).toArray())
    {
        for (const QCborValue& manifestValue : groupValue.toArray())
        {
            const QCborArray manifest = manifestValue.toArray();
            const QCborArray record = manifest.at(3).toArray();
            if (record.size() == 3 && record.at(1).toInteger(-1) == 0)
            {
                for (const QCborValue& reference : record.at(2).toArray())
                {
                    checkpointUsesBaseReference = checkpointUsesBaseReference
                        || reference.toInteger(0) <= -1;
                }
            }
        }
    }
    QString longHistoryError{};
    const auto decodedLongHistory = ui::recovery::deserializeEditorDocumentHistory(
        longHistoryPayload, ProjectType::MOTS, longHistory.current().level,
        &longHistoryError);
    check(checkpointUsesBaseReference && decodedLongHistory
            && longHistoryError.isEmpty()
            && verifyRestoredHistory(longHistory, *decodedLongHistory),
        "document_recovery accepts negative current-base references in periodic checkpoints and restores the full timeline");
    bool cursorTraversalRestored = false;
    if (decodedLongHistory)
    {
        // Use a separate source with both undo and redo entries so this is a
        // genuine arbitrary cursor, rather than the all-undo checkpoint-only
        // fixture above. Save the exact cursor state and depths, walk to the
        // oldest state, then replay exactly the original undo depth.
        const editor::Document cursorSource = makeDeepHistoryDocument(
            ProjectType::MOTS, 40, 7);
        const QByteArray cursorPayload =
            ui::recovery::serializeEditorDocumentHistory(cursorSource);
        QString cursorError{};
        const auto decodedCursorHistory =
            ui::recovery::deserializeEditorDocumentHistory(
                cursorPayload, ProjectType::MOTS,
                cursorSource.current().level, &cursorError);
        if (!decodedCursorHistory || !cursorError.isEmpty())
        {
            check(false,
                "document_recovery cursor fixture decodes with both undo and redo branches");
        }
        editor::Document cursor(cursorSource.current());
        if (decodedCursorHistory)
        {
            cursor.restoreRecoveryHistory(
                cursorSource.current(), *decodedCursorHistory);
        }
        const editor::DocumentState savedState = cursor.current();
        const std::size_t savedUndoDepth = cursor.undoDepth();
        const std::size_t savedRedoDepth = cursor.redoDepth();
        while (cursor.canUndo())
        {
            cursor.undo();
        }
        const bool reachedOldest = cursor.undoDepth() == 0
            && cursor.redoDepth() == savedUndoDepth + savedRedoDepth;
        for (std::size_t i = 0; i < savedUndoDepth && cursor.canRedo(); ++i)
        {
            cursor.redo();
        }
        cursorTraversalRestored = reachedOldest
            && cursor.undoDepth() == savedUndoDepth
            && cursor.redoDepth() == savedRedoDepth
            && sameDocumentState(cursor.current(), savedState);

        // Traverse the entire future branch as well, then return to the saved
        // cursor. This catches state-reference bugs that only occur after a
        // redo-chain checkpoint.
        while (cursor.canRedo())
        {
            cursor.redo();
        }
        const std::size_t newestUndoDepth = cursor.undoDepth();
        while (cursor.undoDepth() > savedUndoDepth && cursor.canUndo())
        {
            cursor.undo();
        }
        cursorTraversalRestored = cursorTraversalRestored
            && newestUndoDepth == savedUndoDepth + savedRedoDepth
            && cursor.undoDepth() == savedUndoDepth
            && cursor.redoDepth() == savedRedoDepth
            && sameDocumentState(cursor.current(), savedState);
    }
    check(cursorTraversalRestored,
        "document_recovery preserves an arbitrary undo cursor while traversing to the oldest and back to the current state");
    const auto inspectHistoryRecords = [&compactSchema](const QString& key,
        const std::size_t expectedEntries)
    {
        const QCborArray groups = compactSchema.value(key).toArray();
        std::size_t entryCount{};
        std::size_t checkpointCount{};
        std::size_t deltaCount{};
        bool cadenceValid = true;
        for (const QCborValue& groupValue : groups)
        {
            const QCborArray group = groupValue.toArray();
            for (const QCborValue& manifestValue : group)
            {
                const QCborArray manifest = manifestValue.toArray();
                const QCborArray record =
                    (manifest.size() == 4 || manifest.size() == 5)
                    ? manifest.at(3).toArray() : QCborArray{};
                const qint64 kind = record.size() >= 2
                    ? record.at(1).toInteger(-1) : -1;
                if (kind == 0)
                {
                    ++checkpointCount;
                }
                else if (kind == 1)
                {
                    ++deltaCount;
                }
                else
                {
                    cadenceValid = false;
                }
                if (entryCount != 0 && entryCount % 16 == 0 && kind != 0)
                {
                    cadenceValid = false;
                }
                ++entryCount;
            }
        }
        return entryCount == expectedEntries && cadenceValid
            && checkpointCount >= expectedEntries / 16
            && deltaCount > 0;
    };
    check(inspectHistoryRecords(QStringLiteral("undoGroups"),
            expandedHistory.undoEntries.size())
            && inspectHistoryRecords(QStringLiteral("redoGroups"),
                expandedHistory.redoEntries.size()),
        "document_recovery emits periodic full checkpoints and intervening reference-list deltas");
    quint64 independentLevelBytes{};
    const auto sumIndependentLevels = [&independentLevelBytes](
        const std::vector<editor::DocumentRecoveryHistoryEntry>& entries)
    {
        for (const editor::DocumentRecoveryHistoryEntry& entry : entries)
        {
            independentLevelBytes += static_cast<quint64>(
                canonicalNestedLevel(entry.state.level).size());
        }
    };
    sumIndependentLevels(expandedHistory.undoEntries);
    sumIndependentLevels(expandedHistory.redoEntries);
    check(independentLevelBytes > 0
            && static_cast<quint64>(compactHistory.size()) * 3 < independentLevelBytes,
        "document_recovery deep repetitive history is less than one third of independent complete Level snapshots");
    QString compactHistoryError{};
    const auto decodedDeepHistory = ui::recovery::deserializeEditorDocumentHistory(
        compactHistory, ProjectType::MOTS, deepHistory.current().level,
        &compactHistoryError);
    check(decodedDeepHistory && compactHistoryError.isEmpty()
            && verifyRestoredHistory(deepHistory, *decodedDeepHistory),
        "document_recovery deep pooled history preserves exact multi-selection membership and primary ordering");

    // Enough mixed-mode checkpoints exceed the former one-Level (128 MiB)
    // aggregate retained footprint, while the semantic pool remains small
    // because adjacent states share almost all geometry.  The bounded V1
    // history envelope must retain and reopen this timeline instead of
    // silently omitting HIST.
    const editor::Document realisticLargeHistory =
        makeDeepHistoryDocument(
            ProjectType::MOTS, 40, 0, false, 4u * 1024u * 1024u);
    const editor::DocumentRecoveryHistory realisticExpected =
        realisticLargeHistory.recoveryHistory();
    std::uint64_t realisticRetainedBytes{};
    std::uint64_t realisticRetainedItems{};
    for (const auto& entry : realisticExpected.undoEntries)
    {
        const assets::SedLevelDecodedFootprint footprint =
            assets::estimateSedLevelDecodedFootprint(entry.state.level);
        realisticRetainedBytes += footprint.ownedBytes;
        realisticRetainedItems += footprint.itemCount;
    }
    QString realisticLargeHistoryError{};
    const QByteArray realisticLargeHistoryPayload =
        ui::recovery::serializeEditorDocumentHistory(realisticLargeHistory);
    const auto decodedRealisticLargeHistory =
        ui::recovery::deserializeEditorDocumentHistory(
            realisticLargeHistoryPayload,
            ProjectType::MOTS,
            realisticLargeHistory.current().level,
            &realisticLargeHistoryError);
    QString realisticCompactHistoryError{};
    const auto decodedRealisticCompactHistory =
        ui::recovery::deserializeEditorDocumentHistoryForRestore(
            realisticLargeHistoryPayload,
            ProjectType::MOTS,
            realisticLargeHistory.current().level,
            &realisticCompactHistoryError);
    const editor::DocumentUndoSnapshotStats realisticCompactStats =
        decodedRealisticCompactHistory && decodedRealisticCompactHistory->compact
        ? [&]()
        {
            editor::Document restored(realisticLargeHistory.current());
            restored.restoreRecoveryHistory(
                realisticLargeHistory.current(), *decodedRealisticCompactHistory);
            return restored.undoSnapshotStats();
        }()
        : editor::DocumentUndoSnapshotStats{};
    check(realisticRetainedBytes > assets::SedMaximumDecodedObjectBytes
            && realisticRetainedItems < assets::SedMaximumDecodedItems
            && !realisticLargeHistoryPayload.isEmpty()
            && decodedRealisticLargeHistory
            && realisticLargeHistoryError.isEmpty()
            && decodedRealisticLargeHistory->undoEntries.size() == 40
            && decodedRealisticCompactHistory
            && decodedRealisticCompactHistory->compact
            && realisticCompactHistoryError.isEmpty()
            && realisticCompactStats.full == 0
            && realisticCompactStats.thingSubsets == 10
            && realisticCompactStats.sectorSubsets == 10
            && verifyRestoredHistory(
                realisticLargeHistory, *decodedRealisticCompactHistory)
            && verifyRestoredHistory(
                realisticLargeHistory, *decodedRealisticLargeHistory),
        "document_recovery keeps a large deduplicated mixed-mode timeline that previously exceeded retained/reconstruction budgets");

    // LightsSnapshot stores only SedLight values and baked ColorF arrays.
    // Keep a deliberately wide, low-vertex map so the old admission proxy
    // (a complete Sector plus Surface for every baked surface, with a full
    // TexVertex for every colour) crosses the former 512 MiB aggregate limit while
    // the actual compact snapshots remain comfortably below it.  This is
    // the concrete regression for the autosave failure where a valid HIST
    // stream was written but discarded on reopen by the over-conservative
    // retained-Level accounting.
    {
        constexpr int kBakedSectorCount = 12'000;
        constexpr int kLightEditCount = 90;
        editor::Document bakedLightTimeline = makeDeepHistoryDocument(
            ProjectType::IJIM, 0, 0, false, 0,
            kBakedSectorCount, 1, 1);
        for (int edit = 0; edit < kLightEditCount; ++edit)
        {
            bakedLightTimeline.pushUndo(
                "Baked light edit " + std::to_string(edit),
                editor::DocumentDirty::Lights);
            bakedLightTimeline.level().lights.front().intensity += 0.5;
            bakedLightTimeline.level().sectors.front().ambient.r += 0.001f;
        }
        const auto& bakedLevel = bakedLightTimeline.current().level;

        // Recreate the former proxy's shape once to document the old charge
        // without allocating one proxy per history entry.
        Level legacyLightProxy{};
        legacyLightProxy.kind = bakedLevel.kind;
        legacyLightProxy.lights = bakedLevel.lights;
        legacyLightProxy.sectors.reserve(bakedLevel.sectors.size());
        for (const Sector& sourceSector : bakedLevel.sectors)
        {
            Sector sector{};
            sector.ambient = sourceSector.ambient;
            sector.surfaces.reserve(sourceSector.surfaces.size());
            for (const Surface& sourceSurface : sourceSector.surfaces)
            {
                Surface surface{};
                surface.texVertices.resize(sourceSurface.texVertices.size());
                sector.surfaces.push_back(std::move(surface));
            }
            legacyLightProxy.sectors.push_back(std::move(sector));
        }
        const assets::SedLevelDecodedFootprint legacyLightFootprint =
            assets::estimateSedLevelDecodedFootprint(legacyLightProxy);
        const std::uint64_t legacyLightTimelineBytes =
            legacyLightFootprint.ownedBytes
            * static_cast<std::uint64_t>(kLightEditCount);

        QString bakedLightHistoryError{};
        const QByteArray bakedLightHistoryPayload =
            ui::recovery::serializeEditorDocumentHistory(bakedLightTimeline);
        const auto bakedLightCompactHistory =
            ui::recovery::deserializeEditorDocumentHistoryForRestore(
                bakedLightHistoryPayload,
                ProjectType::IJIM,
                bakedLevel,
                &bakedLightHistoryError);
        bool bakedLightUndoRedoExact = bakedLightCompactHistory
            && bakedLightCompactHistory->compact
            && bakedLightCompactHistory->undoEntryCount()
                == static_cast<std::size_t>(kLightEditCount);
        if (bakedLightUndoRedoExact)
        {
            const editor::DocumentState bakedState = bakedLightTimeline.current();
            editor::Document restored(bakedState);
            restored.restoreRecoveryHistory(bakedState, *bakedLightCompactHistory);
            for (int edit = kLightEditCount - 1; edit >= 0; --edit)
            {
                editor::DocumentDirty dirtyFlags{};
                bakedLightUndoRedoExact = bakedLightUndoRedoExact
                    && restored.undo(&dirtyFlags)
                    && dirtyFlags == editor::DocumentDirty::Lights
                    && std::abs(
                        restored.level().lights.front().intensity
                            - (10.0 + 0.5 * static_cast<double>(edit)))
                        < 1.0e-9;
            }
            for (int edit = 0; bakedLightUndoRedoExact && edit < kLightEditCount; ++edit)
            {
                editor::DocumentDirty dirtyFlags{};
                bakedLightUndoRedoExact = restored.redo(&dirtyFlags)
                    && dirtyFlags == editor::DocumentDirty::Lights;
            }
        }
        check(legacyLightTimelineBytes > 512ull * 1024ull * 1024ull
                && !bakedLightHistoryPayload.isEmpty()
                && bakedLightCompactHistory
                && bakedLightCompactHistory->compact
                && bakedLightHistoryError.isEmpty()
                && bakedLightUndoRedoExact,
            "document_recovery accepts compact baked-light history whose old Sector/TexVertex proxy would exceed the retained-Level budget");
    }

    // Exact warning regression: a large unchanged Level plus many compact
    // Thing edits used to be charged as one retained full Level per entry,
    // exceeding the former retained-history envelope and causing the writer
    // to publish Level+EDIT without HIST. The live stack and native reader
    // retain only Things for these records, so generation must remain admissible.
    editor::Document compactThingTimeline = makeDeepHistoryDocument(
        ProjectType::MOTS, 0, 0, false, 16u * 1024u * 1024u);
    quint64 legacyThingTimelineCharge{};
    for (int edit = 0; edit < 40; ++edit)
    {
        compactThingTimeline.pushUndo(
            "compact Thing edit " + std::to_string(edit),
            editor::DocumentDirty::Things);
        compactThingTimeline.level().things.front().x += 0.25;
        legacyThingTimelineCharge += assets::estimateSedLevelDecodedFootprint(
            compactThingTimeline.level()).ownedBytes;
    }
    QByteArray compactThingTimelinePayload{};
    QByteArray compactThingTimelineSavePayload{};
    ui::recovery::detail::RecoveryHistoryEncodeStats
        compactThingTimelineEncodeStats{};
    QString compactThingTimelineError{};
    try
    {
        compactThingTimelinePayload =
            ui::recovery::serializeEditorDocumentHistory(compactThingTimeline);
        compactThingTimelineSavePayload =
            ui::recovery::detail::serializeEditorDocumentHistoryForSaveWithStats(
                compactThingTimeline, &compactThingTimelineEncodeStats);
    }
    catch (const std::exception& e)
    {
        compactThingTimelineError = QString::fromUtf8(e.what());
    }
    QString restoredThingTimelineError{};
    const auto restoredThingTimeline =
        ui::recovery::deserializeEditorDocumentHistoryForRestore(
            compactThingTimelinePayload,
            ProjectType::MOTS,
            compactThingTimeline.current().level,
            &restoredThingTimelineError);
    const editor::DocumentUndoSnapshotStats restoredThingTimelineStats =
        restoredThingTimeline && restoredThingTimeline->compact
        ? [&]()
        {
            editor::Document restored(compactThingTimeline.current());
            restored.restoreRecoveryHistory(
                compactThingTimeline.current(), *restoredThingTimeline);
            return restored.undoSnapshotStats();
        }()
        : editor::DocumentUndoSnapshotStats{};
    const bool compactThingTimelineValid =
        legacyThingTimelineCharge > 512ull * 1024ull * 1024ull
            && !compactThingTimelinePayload.isEmpty()
            && !compactThingTimelineSavePayload.isEmpty()
            && compactThingTimelineError.isEmpty()
            && restoredThingTimeline
            && restoredThingTimeline->compact
            && restoredThingTimeline->undoEntryCount() == 40
            && restoredThingTimelineError.isEmpty()
            && restoredThingTimelineStats.full == 0
            && restoredThingTimelineStats.thingSubsets == 40
            && compactThingTimelineEncodeStats.reusedFramesBySection[2] == 40
            && compactThingTimelineEncodeStats.fragmentedFramesBySection[2] == 0
            && compactThingTimelineEncodeStats.fragmentedBytesBySection[2] == 0;
    const std::string compactThingDiagnostic =
        "document_recovery production restore retains a large deduplicated compact HIST"
        " (legacy=" + std::to_string(legacyThingTimelineCharge)
        + ", payload=" + std::to_string(compactThingTimelinePayload.size())
        + ", restored=" + std::to_string(restoredThingTimeline.has_value())
        + ", restoredError=" + restoredThingTimelineError.toStdString()
        + ", subsets=" + std::to_string(restoredThingTimelineStats.thingSubsets)
        + ")";
    check(compactThingTimelineValid, compactThingDiagnostic.c_str());

    // Retained MotS bake provenance forces every live Thing-subset snapshot
    // to retain a complete Level. The generated-HIST writer's compact
    // admission must charge that same storage before it publishes history.
    {
        constexpr std::size_t kProvenanceThingEditCount = 3;
        editor::Document provenanceThingTimeline = makeDeepHistoryDocument(
            ProjectType::MOTS, 0, 0, false, 16u * 1024u * 1024u);
        provenanceThingTimeline.level().motsArchLightingBakeBaselines.push_back(
            MotsArchLightingBakeBaseline{
                provenanceThingTimeline.level().things.front().id,
                "history-template.3do", -1, std::nullopt });
        for (std::size_t edit = 0; edit < kProvenanceThingEditCount; ++edit)
        {
            const std::int32_t thingIndex = static_cast<std::int32_t>(edit);
            provenanceThingTimeline.pushThingSubsetUndo(
                "provenance-backed Thing edit " + std::to_string(edit),
                editor::DocumentDirty::Things,
                std::vector<std::int32_t>{ thingIndex });
            provenanceThingTimeline.level().things[edit].x += 0.25;
        }
        const assets::SedLevelDecodedFootprint fullLevelFootprint =
            assets::estimateSedLevelDecodedFootprint(
                provenanceThingTimeline.current().level);
        const editor::DocumentUndoSnapshotStats liveSnapshotStats =
            provenanceThingTimeline.undoSnapshotStats();
        const QByteArray provenanceThingHistory =
            ui::recovery::serializeEditorDocumentHistory(
                provenanceThingTimeline);
        const quint64 reducedRetainedBudget =
            2u * fullLevelFootprint.ownedBytes;
        QString reducedBudgetError{};
        const bool generatedValidationRejectedReducedBudget =
            !ui::recovery::detail::validateGeneratedRecoveryHistoryWithRetainedBudgetForTesting(
                provenanceThingHistory,
                ProjectType::MOTS,
                provenanceThingTimeline.current().level,
                kProvenanceThingEditCount,
                0,
                reducedRetainedBudget,
                assets::SedMaximumDecodedItems,
                &reducedBudgetError);
        QString sufficientBudgetError{};
        const bool generatedValidationAcceptedSufficientBudget =
            ui::recovery::detail::validateGeneratedRecoveryHistoryWithRetainedBudgetForTesting(
                provenanceThingHistory,
                ProjectType::MOTS,
                provenanceThingTimeline.current().level,
                kProvenanceThingEditCount,
                0,
                4u * fullLevelFootprint.ownedBytes,
                assets::SedMaximumDecodedItems,
                &sufficientBudgetError);
        QString provenanceRestoreError{};
        const auto provenanceCompactHistory =
            ui::recovery::deserializeEditorDocumentHistoryForRestore(
                provenanceThingHistory,
                ProjectType::MOTS,
                provenanceThingTimeline.current().level,
                &provenanceRestoreError);
        bool provenanceUndoRedoExact = provenanceCompactHistory
            && provenanceCompactHistory->compact
            && provenanceCompactHistory->undoEntryCount()
                == kProvenanceThingEditCount
            && provenanceRestoreError.isEmpty();
        editor::DocumentUndoSnapshotStats restoredSnapshotStats{};
        if (provenanceUndoRedoExact)
        {
            editor::Document restored(provenanceThingTimeline.current());
            restored.restoreRecoveryHistory(
                provenanceThingTimeline.current(), *provenanceCompactHistory);
            restoredSnapshotStats = restored.undoSnapshotStats();
            provenanceUndoRedoExact = restoredSnapshotStats.full
                    == kProvenanceThingEditCount
                && verifyRestoredHistory(
                    provenanceThingTimeline, *provenanceCompactHistory);
        }
        check(liveSnapshotStats.full == kProvenanceThingEditCount
                && liveSnapshotStats.thingSubsets == 0
                && fullLevelFootprint.ownedBytes > 16u * 1024u * 1024u
                && !provenanceThingHistory.isEmpty()
                && generatedValidationRejectedReducedBudget
                && reducedBudgetError.contains(
                    QStringLiteral("cumulative retained Level object storage"))
                && generatedValidationAcceptedSufficientBudget
                && sufficientBudgetError.isEmpty()
                && provenanceCompactHistory
                && restoredSnapshotStats.full == kProvenanceThingEditCount
                && provenanceUndoRedoExact,
            "document_recovery generated HIST admission charges full MotS provenance snapshots and compact restore preserves exact Thing Undo/Redo");
    }

    // The shipped test.sed has tens of thousands of surfaces/vertices and
    // crosses the old two-million-item aggregate after only a few checkpoints.
    // Keep this fixture below the 32M history-item envelope while forcing
    // the legacy 2M check to reject it.
    const editor::Document highItemHistory = makeDeepHistoryDocument(
        ProjectType::MOTS, 32, 0, false, 0, 200, 256, 500);
    const editor::DocumentRecoveryHistory highItemExpected =
        highItemHistory.recoveryHistory();
    std::uint64_t highItemCount{};
    for (const auto& entry : highItemExpected.undoEntries)
    {
        highItemCount += assets::estimateSedLevelDecodedFootprint(
            entry.state.level).itemCount;
    }
    QString highItemHistoryError{};
    const QByteArray highItemHistoryPayload =
        ui::recovery::serializeEditorDocumentHistory(highItemHistory);
    const auto decodedHighItemHistory =
        ui::recovery::deserializeEditorDocumentHistory(
            highItemHistoryPayload,
            ProjectType::MOTS,
            highItemHistory.current().level,
            &highItemHistoryError);
    check(highItemCount > assets::SedMaximumDecodedItems
            && highItemCount < 32'000'000ull
            && !highItemHistoryPayload.isEmpty()
            && decodedHighItemHistory
            && highItemHistoryError.isEmpty()
            && decodedHighItemHistory->undoEntries.size() == 32,
        "document_recovery keeps a test.sed-scale high-item timeline under the bounded 32M history-item envelope");

    // Independent footprint for the first two fixed deep-history Undo states:
    // 12,014 owned items and 11,921 authored string bytes including terminators.
    // Recovery walks newest first; both states have a two-digit Cog filename.
    const quint64 expectedRetainedBytes = sizeof(Level) + 7 * sizeof(std::string)
        + 40 * sizeof(Sector) + 3840 * sizeof(Vertex) + 40 * sizeof(Surface)
        + 3840 * sizeof(std::int32_t) + 3840 * sizeof(TexVertex)
        + 80 * sizeof(Thing) + 320 * sizeof(TplValue) + sizeof(ThingTemplate)
        + 2 * sizeof(Cog) + 2 * sizeof(SedLight) + sizeof(HNode) + 11921;
    const auto retainedDiagnostic = [expectedRetainedBytes](quint64 byteLimit, quint64 itemLimit)
    {
        return QStringLiteral("Recovery undo history cumulative retained Level object storage exceeds its bounded byte/item budget after entry 2 (dirty=0x8, bytes=%1/%2, items=24028/%3).")
            .arg(2 * expectedRetainedBytes).arg(byteLimit).arg(itemLimit);
    };
    // Generated validation retains compact snapshots, unlike the materializing
    // reader above. Newest-first edits64..54 contain 3 one-Sector snapshots,
    // 3 two-Cog snapshots, 3 full baked-light snapshots and 2 one-Thing snapshots.
    // MotS Lights snapshots additionally retain all 80 Things and their 320
    // template values: 11 roots + 3*290 + 3*2 + 3*(3922 + 400) + 2*5 = 13863.
    // Strings including terminators: 11*2 empty root strings + 3*31 Sector
    // strings + 3*29 Cog strings + 2*143 Thing/value strings = 488 bytes.
    struct ExpectedBakedSurfaceStorage { std::vector<ColorF> colors; };
    struct ExpectedBakedSectorStorage
    {
        ColorF ambient;
        std::vector<ExpectedBakedSurfaceStorage> surfaces;
    };
    quint64 fullMotsThingSnapshotBytes = 80 * sizeof(Thing)
        + 320 * sizeof(TplValue);
    for (int thingIndex = 0; thingIndex < 80; ++thingIndex)
    {
        fullMotsThingSnapshotBytes += std::string("history-template").size() + 1u;
        fullMotsThingSnapshotBytes += std::string(
            "history-thing-" + std::to_string(thingIndex)).size() + 1u;
        for (int frameIndex = 0; frameIndex < 3; ++frameIndex)
        {
            const double x = thingIndex * 0.25 + frameIndex;
            const double y = thingIndex * -0.5 - frameIndex;
            const double z = thingIndex * 0.75 + frameIndex * 0.5;
            const double pitch = frameIndex * 5.0;
            const double yaw = frameIndex * 10.0;
            const double roll = frameIndex * 15.0;
            fullMotsThingSnapshotBytes += std::string("frame").size() + 1u
                + sed::format::frameValue(x, y, z, pitch, yaw, roll).size() + 1u;
        }
        // health has a six-byte name and an owned empty string value.
        fullMotsThingSnapshotBytes += std::string("health").size() + 2u;
    }
    const quint64 expectedCompactRetainedBytes = 11 * sizeof(Level)
        + 3 * (sizeof(Sector) + 96 * sizeof(Vertex) + sizeof(Surface)
            + 96 * sizeof(std::int32_t) + 96 * sizeof(TexVertex))
        + 3 * 2 * sizeof(Cog)
        + 3 * (2 * sizeof(SedLight) + 40 * sizeof(ExpectedBakedSectorStorage)
            + 40 * sizeof(ExpectedBakedSurfaceStorage) + 3840 * sizeof(ColorF)
            + fullMotsThingSnapshotBytes)
        + 2 * (sizeof(Thing) + 4 * sizeof(TplValue)) + 488;
    const QString compactRetainedDiagnostic = QStringLiteral(
        "Recovery undo history cumulative retained Level object storage exceeds its bounded byte/item budget after entry 11 (dirty=0x4, bytes=%1/%2, items=13863/12014).")
        .arg(expectedCompactRetainedBytes).arg(assets::SedMaximumDecodedObjectBytes);
    QString retainedLevelBudgetError{};
    QString generatedRetainedLevelBudgetError{};
    const assets::SedLevelDecodedFootprint firstRetainedLevel =
        expandedHistory.undoEntries.empty()
        ? assets::SedLevelDecodedFootprint{}
        : assets::estimateSedLevelDecodedFootprint(
            expandedHistory.undoEntries.front().state.level);
    const bool materializingReaderRejectedRetainedHistory =
        !ui::recovery::detail::decodeRecoveryHistoryWithRetainedBudgetForTesting(
                compactHistory,
                ProjectType::MOTS,
                deepHistory.current().level,
                assets::SedMaximumDecodedObjectBytes,
                firstRetainedLevel.itemCount,
                &retainedLevelBudgetError);
    QString retainedLevelByteBudgetError{};
    QString retainedLevelItemBudgetError{};
    const bool materializingReaderRejectedRetainedBytesOnly =
        !ui::recovery::detail::decodeRecoveryHistoryWithRetainedBudgetForTesting(
            compactHistory,
            ProjectType::MOTS,
            deepHistory.current().level,
            firstRetainedLevel.ownedBytes,
            assets::SedMaximumDecodedItems,
            &retainedLevelByteBudgetError);
    const bool materializingReaderRejectedRetainedItemsOnly =
        !ui::recovery::detail::decodeRecoveryHistoryWithRetainedBudgetForTesting(
            compactHistory,
            ProjectType::MOTS,
            deepHistory.current().level,
            std::numeric_limits<quint64>::max(),
            firstRetainedLevel.itemCount,
            &retainedLevelItemBudgetError);
    const bool nullOutputWriterValidationRejectedRetainedHistory =
        !ui::recovery::detail::validateGeneratedRecoveryHistoryWithRetainedBudgetForTesting(
                compactHistory,
                ProjectType::MOTS,
                deepHistory.current().level,
                expandedHistory.undoEntries.size(),
                expandedHistory.redoEntries.size(),
                assets::SedMaximumDecodedObjectBytes,
                firstRetainedLevel.itemCount,
                &generatedRetainedLevelBudgetError);
    check(firstRetainedLevel.itemCount == 12014 && firstRetainedLevel.ownedBytes == expectedRetainedBytes
            && materializingReaderRejectedRetainedHistory
            && materializingReaderRejectedRetainedBytesOnly
            && materializingReaderRejectedRetainedItemsOnly
            && nullOutputWriterValidationRejectedRetainedHistory
            && exactRecoveryText(retainedLevelBudgetError, retainedDiagnostic(assets::SedMaximumDecodedObjectBytes, 12014))
            && exactRecoveryText(generatedRetainedLevelBudgetError, compactRetainedDiagnostic),
        "document_recovery compact writer validation and materializing reader reject their independently counted retained storage budgets");
    check(exactRecoveryText(retainedLevelByteBudgetError, retainedDiagnostic(expectedRetainedBytes, assets::SedMaximumDecodedItems))
            && exactRecoveryText(retainedLevelItemBudgetError, retainedDiagnostic(std::numeric_limits<quint64>::max(), 12014)),
        "document_recovery reports retained-Level byte and item aggregate limits independently");

    const QCborMap canonicalHistoryRoot = QCborValue::fromCbor(compactHistory).toMap();
    QByteArray reorderedHistory{};
    reorderedHistory.append(static_cast<char>(
        0xa0u | static_cast<unsigned char>(canonicalHistoryRoot.size())));
    for (auto it = canonicalHistoryRoot.constEnd();
         it != canonicalHistoryRoot.constBegin();)
    {
        --it;
        reorderedHistory.append(ui::recovery::serializeCanonicalCbor(it.key()));
        reorderedHistory.append(ui::recovery::serializeCanonicalCbor(it.value()));
    }
    QString reorderedHistoryError{};
    const auto reorderedHistoryDecoded =
        ui::recovery::deserializeEditorDocumentHistory(
            reorderedHistory, ProjectType::MOTS, deepHistory.current().level,
            &reorderedHistoryError);
    check(reorderedHistory != compactHistory,
        "document_recovery reordered HIST regression fixture differs from canonical bytes");
    check(!reorderedHistoryDecoded,
        "document_recovery rejects reordered HIST map keys outside its deterministic wire encoding");
    check(exactRecoveryText(reorderedHistoryError, QStringLiteral("Recovery undo history canonical CBOR exceeds the bounded node/depth/string/materialization limits, map-key order, or single-value framing.")),
        "document_recovery reports reordered HIST map keys as noncanonical");

    QString oversizedHistoryError{};
    const bool oversizedDecoded =
        ui::recovery::detail::decodeRecoveryHistoryWithReconstructionBudgetForTesting(
            compactHistory, ProjectType::MOTS, deepHistory.current().level,
            1, &oversizedHistoryError);
    check(!compactHistory.isEmpty() && !oversizedDecoded
            && exactRecoveryText(oversizedHistoryError, QStringLiteral("Recovery undo history cumulative Level reconstruction exceeds its bounded work budget.")),
        "document_recovery rejects compact HIST manifests whose expanded Level work exceeds the cumulative budget");

    const QByteArray oversizedPoolFragment =
        historyWithOversizedPoolFragment(compactHistory);
    QString oversizedPoolError{};
    const auto oversizedPoolDecoded = ui::recovery::deserializeEditorDocumentHistory(
        oversizedPoolFragment, ProjectType::MOTS, deepHistory.current().level,
        &oversizedPoolError);
    check(!oversizedPoolFragment.isEmpty() && !oversizedPoolDecoded
            && exactRecoveryText(oversizedPoolError, QStringLiteral("Recovery undo history canonical CBOR exceeds the bounded node/depth/string/materialization limits, map-key order, or single-value framing.")),
        "document_recovery streaming-preflights HIST pool fragments larger than the V1 content-chunk bound before materialization");

    const QByteArray duplicatePoolHistory =
        historyWithDuplicatePoolEntry(compactHistory);
    QString duplicatePoolError{};
    check(!duplicatePoolHistory.isEmpty()
            && !ui::recovery::deserializeEditorDocumentHistory(
                duplicatePoolHistory, ProjectType::MOTS, deepHistory.current().level,
                &duplicatePoolError)
            && exactRecoveryText(duplicatePoolError, QStringLiteral("Recovery undo history fragment pool contains duplicate entries.")),
        "document_recovery rejects semantically duplicate HIST pool entries before Level reconstruction");

    const editor::Document geometryDirtyHistory =
        makeDeepHistoryDocument(ProjectType::MOTS, 1, 0);
    const QByteArray geometryDirtyPayload =
        ui::recovery::serializeEditorDocumentHistory(geometryDirtyHistory);
    const QByteArray forgedThingsDirtyPayload = historyWithDirtyOverride(
        geometryDirtyPayload,
        static_cast<qint64>(editor::DocumentDirty::Things));
    QString forgedThingsDirtyError{};
    check(!forgedThingsDirtyPayload.isEmpty()
            && !ui::recovery::deserializeEditorDocumentHistory(
                forgedThingsDirtyPayload, ProjectType::MOTS,
                geometryDirtyHistory.current().level, &forgedThingsDirtyError)
            && exactRecoveryText(forgedThingsDirtyError, QStringLiteral("Recovery undo history dirty flags do not describe the adjacent Level transition.")),
        "document_recovery rejects a forged Things dirty mask when the adjacent Level changed geometry");

    QString forgedThingsCompactError{};
    check(!forgedThingsDirtyPayload.isEmpty()
            && !ui::recovery::deserializeEditorDocumentHistoryForRestore(
                forgedThingsDirtyPayload, ProjectType::MOTS,
                geometryDirtyHistory.current().level,
                &forgedThingsCompactError)
            && exactRecoveryText(forgedThingsCompactError, QStringLiteral("Recovery undo history dirty flags do not describe the adjacent Level transition.")),
        "document_recovery compact HIST reader never trusts a forged Things dirty mask for frame reuse");

    const QByteArray forgedLightsDirtyPayload = historyWithDirtyOverride(
        geometryDirtyPayload,
        static_cast<qint64>(editor::DocumentDirty::Lights));
    QString forgedLightsDirtyError{};
    check(!forgedLightsDirtyPayload.isEmpty()
            && !ui::recovery::deserializeEditorDocumentHistory(
                forgedLightsDirtyPayload, ProjectType::MOTS,
                geometryDirtyHistory.current().level, &forgedLightsDirtyError)
            && exactRecoveryText(forgedLightsDirtyError, QStringLiteral("Recovery undo history dirty flags do not describe the adjacent Level transition.")),
        "document_recovery rejects a forged Lights dirty mask when the adjacent Level changed unrelated geometry");

    const QByteArray forgedCogsDirtyPayload = historyWithDirtyOverride(
        geometryDirtyPayload,
        static_cast<qint64>(editor::DocumentDirty::Cogs));
    QString forgedCogsDirtyError{};
    check(!forgedCogsDirtyPayload.isEmpty()
            && !ui::recovery::deserializeEditorDocumentHistory(
                forgedCogsDirtyPayload, ProjectType::MOTS,
                geometryDirtyHistory.current().level, &forgedCogsDirtyError)
            && exactRecoveryText(forgedCogsDirtyError, QStringLiteral("Recovery undo history dirty flags do not describe the adjacent Level transition.")),
        "document_recovery rejects a forged Cogs dirty mask when the adjacent Level changed unrelated geometry");

    constexpr qint64 structureDirty = static_cast<qint64>(
        editor::DocumentDirty::Structure);
    const QByteArray forgedThingsStructurePayload = historyWithDirtyOverride(
        geometryDirtyPayload,
        static_cast<qint64>(editor::DocumentDirty::Things) | structureDirty);
    QString forgedThingsStructureError{};
    const auto forgedThingsStructureDecoded =
        ui::recovery::deserializeEditorDocumentHistory(
            forgedThingsStructurePayload, ProjectType::MOTS,
            geometryDirtyHistory.current().level,
            &forgedThingsStructureError);
    check(!forgedThingsStructurePayload.isEmpty()
            && forgedThingsStructureDecoded
            && forgedThingsStructureError.isEmpty()
            && verifyRestoredHistory(
                geometryDirtyHistory, *forgedThingsStructureDecoded),
        "document_recovery treats Things|Structure as a full snapshot and restores unrelated geometry exactly");

    const QByteArray forgedLightsStructurePayload = historyWithDirtyOverride(
        geometryDirtyPayload,
        static_cast<qint64>(editor::DocumentDirty::Lights) | structureDirty);
    QString forgedLightsStructureError{};
    const auto forgedLightsStructureDecoded =
        ui::recovery::deserializeEditorDocumentHistory(
            forgedLightsStructurePayload, ProjectType::MOTS,
            geometryDirtyHistory.current().level,
            &forgedLightsStructureError);
    check(!forgedLightsStructurePayload.isEmpty()
            && forgedLightsStructureDecoded
            && forgedLightsStructureError.isEmpty()
            && verifyRestoredHistory(
                geometryDirtyHistory, *forgedLightsStructureDecoded),
        "document_recovery treats Lights|Structure as a full snapshot and restores unrelated geometry exactly");

    const editor::Document validThingsStructureHistory =
        makeDeepHistoryDocument(ProjectType::MOTS, 2, 0);
    const QByteArray validThingsStructurePayload = historyWithDirtyOverride(
        ui::recovery::serializeEditorDocumentHistory(validThingsStructureHistory),
        static_cast<qint64>(editor::DocumentDirty::Things) | structureDirty);
    QString validThingsStructureError{};
    const auto validThingsStructureDecoded =
        ui::recovery::deserializeEditorDocumentHistory(
            validThingsStructurePayload, ProjectType::MOTS,
            validThingsStructureHistory.current().level,
            &validThingsStructureError);
    check(validThingsStructureDecoded && validThingsStructureError.isEmpty(),
        "document_recovery accepts Things|Structure as an exact full-snapshot transition");

    const editor::Document validLightsStructureHistory =
        makeDeepHistoryDocument(ProjectType::MOTS, 3, 0);
    const QByteArray validLightsStructurePayload = historyWithDirtyOverride(
        ui::recovery::serializeEditorDocumentHistory(validLightsStructureHistory),
        static_cast<qint64>(editor::DocumentDirty::Lights) | structureDirty);
    QString validLightsStructureError{};
    const auto validLightsStructureDecoded =
        ui::recovery::deserializeEditorDocumentHistory(
            validLightsStructurePayload, ProjectType::MOTS,
            validLightsStructureHistory.current().level,
            &validLightsStructureError);
    check(validLightsStructureDecoded && validLightsStructureError.isEmpty(),
        "document_recovery accepts Lights|Structure as an exact full-snapshot transition");

    const editor::Document duplicateBaseHistory = makeDeepHistoryDocument(
        ProjectType::MOTS, 40, 0, true);
    const QByteArray duplicateBaseHistoryPayload =
        ui::recovery::serializeEditorDocumentHistory(duplicateBaseHistory);
    const std::vector<qint64> canonicalBaseReferences =
        ui::recovery::detail::canonicalHistoryBaseReferencesForTesting(
            duplicateBaseHistory.current().level);
    qint64 duplicateBaseIndex{-1};
    QByteArray nonCanonicalBaseReference{};
    for (std::size_t index = 0; index < canonicalBaseReferences.size(); ++index)
    {
        const qint64 directReference = -1 - static_cast<qint64>(index);
        if (canonicalBaseReferences[index] != directReference)
        {
            const QByteArray candidate = historyWithNonCanonicalBaseReference(
                duplicateBaseHistoryPayload,
                canonicalBaseReferences[index],
                static_cast<qint64>(index));
            // Some duplicated save-time fragments do not occur in any
            // historical checkpoint/delta. Select the first duplicate that
            // is actually referenced so this fixture always reaches the
            // decoder's canonical negative-reference check.
            if (!candidate.isEmpty())
            {
                duplicateBaseIndex = static_cast<qint64>(index);
                nonCanonicalBaseReference = candidate;
                break;
            }
        }
    }
    QString nonCanonicalBaseReferenceError{};
    const auto nonCanonicalDecoded = nonCanonicalBaseReference.isEmpty()
        ? std::optional<editor::DocumentRecoveryHistory>{}
        : ui::recovery::deserializeEditorDocumentHistory(
            nonCanonicalBaseReference, ProjectType::MOTS,
            duplicateBaseHistory.current().level,
            &nonCanonicalBaseReferenceError);
    check(duplicateBaseIndex >= 0
            && !nonCanonicalBaseReference.isEmpty()
            && !nonCanonicalDecoded
            && exactRecoveryText(nonCanonicalBaseReferenceError, QStringLiteral("Recovery undo history uses a noncanonical duplicate current-base reference.")),
        "document_recovery rejects a noncanonical negative current-base reference");

    const QByteArray unusedPoolHistory =
        historyWithUnusedPoolEntry(compactHistory);
    QString unusedPoolError{};
    check(!unusedPoolHistory.isEmpty()
            && !ui::recovery::deserializeEditorDocumentHistory(
                unusedPoolHistory, ProjectType::MOTS, deepHistory.current().level,
                &unusedPoolError)
            && exactRecoveryText(unusedPoolError, QStringLiteral("Recovery undo history fragment pool contains unused entries.")),
        "document_recovery rejects unused HIST pool entries instead of retaining redundant content");

    const QByteArray shortHistoryGroup =
        historyWithShortNonFinalGroup(compactHistory);
    QString shortHistoryGroupError{};
    check(!shortHistoryGroup.isEmpty()
            && !ui::recovery::deserializeEditorDocumentHistory(
                shortHistoryGroup, ProjectType::MOTS,
                deepHistory.current().level,
                &shortHistoryGroupError)
            && exactRecoveryText(shortHistoryGroupError, QStringLiteral("Recovery undo history group is non-canonical or too large.")),
        "document_recovery rejects non-final HIST groups that do not use the canonical 32-entry width");

    const QByteArray malformedDelta = historyWithMalformedDelta(compactHistory);
    QString malformedDeltaError{};
    check(!malformedDelta.isEmpty()
            && !ui::recovery::deserializeEditorDocumentHistory(
                malformedDelta, ProjectType::MOTS, deepHistory.current().level,
                &malformedDeltaError)
            && exactRecoveryText(malformedDeltaError, QStringLiteral("Recovery undo history delta splice bounds are invalid.")),
        "document_recovery rejects malformed versioned reference-list delta bounds");

    QString excessiveNodesError{};
    check(!ui::recovery::deserializeEditorDocumentHistory(
                cborWithExcessiveDeclaredNodes(),
                ProjectType::MOTS,
                deepHistory.current().level,
                &excessiveNodesError)
            && exactRecoveryText(excessiveNodesError, QStringLiteral("Recovery undo history canonical CBOR exceeds the bounded node/depth/string/materialization limits, map-key order, or single-value framing.")),
        "document_recovery preflights declared CBOR node pressure before materializing HIST");

    QString excessiveDepthError{};
    check(!ui::recovery::deserializeEditorDocumentHistory(
                cborWithExcessiveNesting(),
                ProjectType::MOTS,
                deepHistory.current().level,
                &excessiveDepthError)
            && exactRecoveryText(excessiveDepthError, QStringLiteral("Recovery undo history canonical CBOR exceeds the bounded node/depth/string/materialization limits, map-key order, or single-value framing.")),
        "document_recovery preflights excessive CBOR nesting before materializing HIST");

    QString excessiveMaterializationError{};
    check(!ui::recovery::deserializeEditorDocumentHistory(
                cborWithExcessiveMaterialization(),
                ProjectType::MOTS,
                deepHistory.current().level,
                &excessiveMaterializationError)
            && exactRecoveryText(excessiveMaterializationError, QStringLiteral("Recovery undo history canonical CBOR exceeds the bounded node/depth/string/materialization limits, map-key order, or single-value framing.")),
        "document_recovery preflights aggregate HIST object materialization before QCborValue allocation");

    QString oversizedTextError{};
    check(!ui::recovery::deserializeEditorDocumentHistory(
                cborWithOversizedTextItem(),
                ProjectType::MOTS,
                deepHistory.current().level,
                &oversizedTextError)
            && exactRecoveryText(oversizedTextError, QStringLiteral("Recovery undo history canonical CBOR exceeds the bounded node/depth/string/materialization limits, map-key order, or single-value framing.")),
        "document_recovery streaming-preflights oversized CBOR text before QString materialization");

    const auto scalarSelection = [](const qint64 value)
    {
        QCborArray array{};
        array.append(value);
        return array;
    };
    const auto tupleSelection = [](std::initializer_list<qint64> values)
    {
        QCborArray tuple{};
        for (const qint64 value : values) tuple.append(value);
        QCborArray array{};
        array.append(tuple);
        return array;
    };
    const auto rejectDanglingSelection = [&compactHistory, &deepHistory](
        const QString& key,
        const QCborArray& replacement,
        const char* message)
    {
        const QByteArray corrupted = historyWithSelectionOverride(
            compactHistory, key, replacement);
        QString selectionError{};
        const auto decoded = ui::recovery::deserializeEditorDocumentHistory(
            corrupted, ProjectType::MOTS, deepHistory.current().level,
            &selectionError);
        check(!corrupted.isEmpty() && !decoded
                && exactRecoveryText(selectionError, QStringLiteral("Recovery undo history selection references missing or incompatible Level objects.")),
            message);
    };
    rejectDanglingSelection(
        QStringLiteral("sectors"), scalarSelection(99'999),
        "document_recovery rejects a schema-valid HIST sector selection outside its reconstructed Level");
    rejectDanglingSelection(
        QStringLiteral("things"), scalarSelection(99'999),
        "document_recovery rejects a schema-valid HIST Thing selection outside its reconstructed Level");
    rejectDanglingSelection(
        QStringLiteral("lights"), scalarSelection(99'999),
        "document_recovery rejects a schema-valid HIST light selection outside its reconstructed Level");
    rejectDanglingSelection(
        QStringLiteral("cogs"), scalarSelection(99'999),
        "document_recovery rejects a schema-valid HIST COG selection outside its reconstructed Level");
    rejectDanglingSelection(
        QStringLiteral("surfaces"), tupleSelection({ 0, 99'999 }),
        "document_recovery rejects a schema-valid HIST surface selection outside its reconstructed Level");
    rejectDanglingSelection(
        QStringLiteral("vertices"), tupleSelection({ 0, 0, 99'999 }),
        "document_recovery rejects a schema-valid HIST surface-vertex selection outside its reconstructed Level");
    rejectDanglingSelection(
        QStringLiteral("frames"), tupleSelection({ 0, 99'999 }),
        "document_recovery rejects a schema-valid HIST frame value index outside its reconstructed Level");
    rejectDanglingSelection(
        QStringLiteral("frames"), tupleSelection({ 0, 3 }),
        "document_recovery rejects an in-range HIST frame reference whose Thing value is not a frame");

    QCborMap abandonedHistorySchema{};
    abandonedHistorySchema.insert(QStringLiteral("version"), 1);
    abandonedHistorySchema.insert(QStringLiteral("undo"), QCborArray{});
    abandonedHistorySchema.insert(QStringLiteral("redo"), QCborArray{});
    QString abandonedHistoryError{};
    check(!ui::recovery::deserializeEditorDocumentHistory(
                ui::recovery::serializeCanonicalCbor(QCborValue(abandonedHistorySchema)),
                ProjectType::MOTS,
                deepHistory.current().level,
                &abandonedHistoryError)
            && exactRecoveryText(abandonedHistoryError, QStringLiteral("Recovery undo history uses an earlier development schema that is unsupported.")),
        "document_recovery intentionally rejects the abandoned complete-Level HIST development schema");

    QCborParserError compactParseError{};
    QCborMap corruptHistoryRoot = QCborValue::fromCbor(
        compactHistory, &compactParseError).toMap();
    QCborArray corruptPool = corruptHistoryRoot.value(QStringLiteral("pool")).toArray();
    if (compactParseError.error == QCborError::NoError && !corruptPool.isEmpty())
    {
        for (qsizetype index = 0; index < corruptPool.size(); ++index)
        {
            QByteArray corruptFragment = corruptPool.at(index).toByteArray();
            constexpr qsizetype semanticPayloadOffset = 100;
            if (corruptFragment.startsWith(QByteArray("sectv1\n\0", 8))
                && corruptFragment.size() > semanticPayloadOffset)
            {
                // Pool item zero is only the nested fixed header, whose
                // checked tail mirror intentionally repairs a signature bit
                // flip.  Damage an authored semantic payload byte so the
                // nested section CRC32C must reject the reconstructed Level.
                corruptFragment[semanticPayloadOffset] = static_cast<char>(
                    corruptFragment.at(semanticPayloadOffset) ^ 0x5a);
                corruptPool[index] = corruptFragment;
                corruptHistoryRoot.insert(QStringLiteral("pool"), corruptPool);
                break;
            }
        }
    }
    QString corruptHistoryError{};
    const auto corruptedHistoryDecoded = ui::recovery::deserializeEditorDocumentHistory(
        ui::recovery::serializeCanonicalCbor(QCborValue(corruptHistoryRoot)),
        ProjectType::MOTS,
        deepHistory.current().level,
        &corruptHistoryError);
    check(!corruptedHistoryDecoded
            && !corruptHistoryError.isEmpty(),
        "document_recovery rejects a corrupted pooled Level fragment through native semantic validation");

    const std::filesystem::path root(temporaryDirectory.path().toStdWString());

    {
        const std::string geometry = R"JKL(SECTION: HEADER
VERSION 1
END
SECTION: GEORESOURCE
WORLD COLORMAPS 0
WORLD VERTICES 3
0: 0 0 0
1: 1 0 0
2: 0 1 0
WORLD TEXTURE VERTICES 1
0: 0 0
WORLD ADJOINS 0
WORLD SURFACES 1
0: -1 0x0 0x0 0 3 3 -1 0 3 0,0 1,0 2,0
END
SECTION: SECTORS
WORLD SECTORS 1
SECTOR 0
FLAGS 0
AMBIENT LIGHT 0
EXTRA LIGHT 0
VERTICES 3
0: 0
1: 1
2: 2
SURFACES 0 1
END
)JKL";
        const std::string cogRows = R"JKL(SECTION: COGS
WORLD COGS 3
0: defaults.cog
1: defaults.cog 7
2: delayed.cog 11
END
)JKL";
        const auto loadJkl = [](const std::string& text)
        {
            std::istringstream input(text);
            return assets::loadFromJKL(input);
        };
        const auto hasSurfaceMap = [](const Level& level)
        {
            return level.importedJklSurfaceRefs
                == std::vector<SurfaceRef>{ SurfaceRef{ 0, 0 } };
        };
        const auto hasPendingRows = [&hasSurfaceMap](
            const Level& level,
            const std::string_view editedRawValue)
        {
            return level.importedJklCogRowsPresent
                && hasSurfaceMap(level)
                && level.cogs.size() == 3u
                && level.cogs[0].name == "defaults.cog"
                && level.cogs[0].values.empty()
                && level.cogs[1].values.size() == 1u
                && level.cogs[1].values[0].name == "param0"
                && level.cogs[1].values[0].type == ValueType::str
                && level.cogs[1].values[0].stringValue == editedRawValue
                && level.cogs[2].values.size() == 1u
                && level.cogs[2].values[0].name == "param0"
                && level.cogs[2].values[0].stringValue == "11";
        };
        const auto hasTypedDefaults = [](const Level& level)
        {
            const auto hasTypedCog = [](const Cog& cog,
                                         const std::int32_t expectedCount)
            {
                return cog.values.size() == 4u
                    && cog.values[0].name == "count"
                    && cog.values[0].type == ValueType::integer
                    && cog.values[0].intValue == expectedCount
                    && cog.values[1].name == "target"
                    && cog.values[1].cogType == CogType::sec
                    && cog.values[1].type == ValueType::ptr
                    && cog.values[1].intValue == -1
                    && cog.values[2].name == "direction"
                    && cog.values[2].type == ValueType::vect
                    && cog.values[2].stringValue == "(0/0/0)"
                    && cog.values[3].name == "sound"
                    && cog.values[3].cogType == CogType::wav
                    && cog.values[3].type == ValueType::str
                    && cog.values[3].stringValue.empty();
            };
            return !level.importedJklCogRowsPresent
                && level.importedJklSurfaceRefs.empty()
                && level.cogs.size() == 3u
                && hasTypedCog(level.cogs[0], 0)
                && hasTypedCog(level.cogs[1], 8)
                && hasTypedCog(level.cogs[2], 11);
        };

        Level imported{};
        bool fixtureValid = false;
        try
        {
            imported = loadJkl(geometry + cogRows);
            fixtureValid = imported.importedJklCogRowsPresent
                && hasSurfaceMap(imported)
                && imported.cogs.size() == 3u
                && imported.cogs[0].values.empty()
                && imported.cogs[1].values.size() == 1u
                && imported.cogs[1].values[0].stringValue == "7"
                && imported.cogs[2].values.size() == 1u
                && imported.cogs[2].values[0].stringValue == "11";
        }
        catch (const std::exception&)
        {
            fixtureValid = false;
        }

        bool markerOnlyCogHistoryExact = false;
        if ( fixtureValid )
        {
            const auto sameCogRows = [](
                const std::vector<Cog>& lhs,
                const std::vector<Cog>& rhs)
            {
                if ( lhs.size() != rhs.size() ) return false;
                for ( std::size_t cogIndex = 0;
                    cogIndex < lhs.size(); ++cogIndex )
                {
                    const Cog& leftCog = lhs[cogIndex];
                    const Cog& rightCog = rhs[cogIndex];
                    if ( leftCog.id != rightCog.id
                        || leftCog.num != rightCog.num
                        || leftCog.name != rightCog.name
                        || leftCog.values.size() != rightCog.values.size() )
                    {
                        return false;
                    }
                    for ( std::size_t valueIndex = 0;
                        valueIndex < leftCog.values.size(); ++valueIndex )
                    {
                        const CogValue& leftValue = leftCog.values[valueIndex];
                        const CogValue& rightValue = rightCog.values[valueIndex];
                        if ( leftValue.name != rightValue.name
                            || leftValue.type != rightValue.type
                            || leftValue.intValue != rightValue.intValue
                            || leftValue.floatValue != rightValue.floatValue
                            || leftValue.stringValue != rightValue.stringValue
                            || leftValue.cogType != rightValue.cogType )
                        {
                            return false;
                        }
                    }
                }
                return true;
            };
            editor::DocumentState markerState{};
            markerState.level = imported;
            editor::Document markerOnlyHistory(std::move(markerState));
            markerOnlyHistory.pushUndo(
                "Change only imported COG marker",
                editor::DocumentDirty::Cogs);
            markerOnlyHistory.level().importedJklCogRowsPresent = false;
            const QByteArray markerOnlyPayload =
                ui::recovery::serializeEditorDocumentHistory(markerOnlyHistory);
            QString markerOnlyError{};
            auto markerOnlyDecoded =
                ui::recovery::deserializeEditorDocumentHistory(
                    markerOnlyPayload, ProjectType::JKDF2,
                    markerOnlyHistory.current().level, &markerOnlyError);
            if ( markerOnlyDecoded )
            {
                editor::Document restored(markerOnlyHistory.current());
                restored.restoreRecoveryHistory(
                    markerOnlyHistory.current(),
                    std::move(*markerOnlyDecoded));
                editor::DocumentDirty markerDirty{};
                const bool undoRestoresMarker = restored.undo(&markerDirty)
                    && markerDirty == editor::DocumentDirty::Cogs
                    && restored.level().importedJklCogRowsPresent
                    && hasSurfaceMap(restored.level())
                    && sameCogRows(restored.level().cogs, imported.cogs);
                const bool redoClearsOnlyMarker = restored.redo(&markerDirty)
                    && markerDirty == editor::DocumentDirty::Cogs
                    && !restored.level().importedJklCogRowsPresent
                    && hasSurfaceMap(restored.level())
                    && sameCogRows(restored.level().cogs, imported.cogs);
                markerOnlyCogHistoryExact = undoRestoresMarker
                    && redoClearsOnlyMarker;
            }
        }
        check(markerOnlyCogHistoryExact,
            "document_recovery persists and restores a marker-only COG transition through sparse HIST metadata and a compact CogsSnapshot");

        const std::filesystem::path rotationRoot = root / "pending-cog-rotation";
        const std::filesystem::path livePath = ui::recovery::recoveryFilePath(
            rotationRoot,
            QStringLiteral("untitled:document-recovery-test-session"),
            QStringLiteral("document-recovery-test-session"),
            QStringLiteral("Pending COG Import"),
            ProjectType::JKDF2);
        auto gate = std::make_shared<ui::recovery::PublicationGate>();
        gate->generation.store(1);
        auto request = makeRequest(
            livePath, gate, 1, ProjectType::JKDF2, 0, false);
        editor::DocumentState pendingState{};
        pendingState.level = imported;
        request.document.clear(std::move(pendingState));
        request.document.pushUndo(
            "Edit raw imported COG token", editor::DocumentDirty::Cogs);
        if (request.document.level().cogs.size() == 3u
            && request.document.level().cogs[1].values.size() == 1u)
        {
            request.document.level().cogs[1].values[0].stringValue = "8";
        }
        const std::filesystem::path resourceRoot = root / "pending-cog-resources";
        std::error_code resourceError{};
        std::filesystem::create_directories(resourceRoot / "cog", resourceError);
        std::ofstream defaultsScript(resourceRoot / "cog" / "defaults.cog");
        defaultsScript << "symbols\n"
            << "    int count=0\n"
            << "    sector target\n"
            << "    vector direction\n"
            << "    wav sound\n"
            << "end\n";
        defaultsScript.close();
        const bool defaultsScriptWritten = !defaultsScript.fail()
            && !defaultsScript.is_open();
        std::ofstream delayedScript(resourceRoot / "cog" / "delayed.cog");
        delayedScript << "symbols\n"
            << "    int count=0\n"
            << "    sector target\n"
            << "    vector direction\n"
            << "    wav sound\n"
            << "end\n";
        delayedScript.close();
        const bool delayedScriptWritten = !delayedScript.fail()
            && !delayedScript.is_open();
        assets::ResourceLocator availableResources{};
        availableResources.addRoot(resourceRoot);

        const bool pendingEditValid = fixtureValid
            && hasPendingRows(request.document.level(), "8")
            && !resourceError
            && defaultsScriptWritten && delayedScriptWritten;
        request.document.pushUndo(
            "Hydrate imported COG defaults", editor::DocumentDirty::Cogs);
        bool finalizedHistoryState = false;
        try
        {
            assets::finalizeJklImportResources(
                request.document.level(), availableResources);
            finalizedHistoryState = hasTypedDefaults(request.document.level());
        }
        catch (const std::exception&)
        {
            finalizedHistoryState = false;
        }
        const bool returnedToPending = request.document.undo()
            && hasPendingRows(request.document.level(), "8");
        const editor::Document expectedPending = request.document;
        const std::uint64_t expectedPendingUndo = expectedPending.undoDepth();
        const std::uint64_t expectedPendingRedo = expectedPending.redoDepth();
        const auto initialWrite = fixtureValid && pendingEditValid
                && finalizedHistoryState && returnedToPending
            ? ui::recovery::writeRecoverySnapshot(std::move(request))
            : ui::recovery::WriteResult{};
        const std::filesystem::path metadataFreePath =
            rotationRoot / "pending-cog-metadata-free.sedrecovery";
        auto metadataFreeGate = std::make_shared<ui::recovery::PublicationGate>();
        metadataFreeGate->generation.store(1);
        auto metadataFreeRequest = makeRequest(
            metadataFreePath, metadataFreeGate, 1, ProjectType::JKDF2, 0, false);
        Level metadataFreeLevel = expectedPending.current().level;
        metadataFreeLevel.importedJklCogRowsPresent = false;
        metadataFreeLevel.importedJklSurfaceRefs.clear();
        editor::DocumentState metadataFreeState{};
        metadataFreeState.level = metadataFreeLevel;
        metadataFreeRequest.document.clear(std::move(metadataFreeState));
        const auto metadataFreeWrite = fixtureValid
            ? ui::recovery::writeRecoverySnapshot(std::move(metadataFreeRequest))
            : ui::recovery::WriteResult{};
        const auto importedRecord = initialWrite.status
                == ui::recovery::WriteStatus::published
            ? ui::recovery::readRecoveryRecord(livePath)
            : std::optional<ui::recovery::RecoveryRecord>{};
        const std::vector<std::byte> requiredOnlyBytes = assets::saveToSedBytes(
            expectedPending.current().level, {},
            assets::SedSerializationProfile::UnprotectedNested);
        const QByteArray requiredOnlyPayload(
            reinterpret_cast<const char*>(requiredOnlyBytes.data()),
            static_cast<qsizetype>(requiredOnlyBytes.size()));
        const QByteArray requiredOnlyDigest = QCryptographicHash::hash(
            requiredOnlyPayload, QCryptographicHash::Sha256);
        Level markerOnlyHashLevel = metadataFreeLevel;
        markerOnlyHashLevel.cogs.clear();
        markerOnlyHashLevel.importedJklCogRowsPresent = true;
        markerOnlyHashLevel.importedJklSurfaceRefs.clear();
        Level absentMarkerHashLevel = markerOnlyHashLevel;
        absentMarkerHashLevel.importedJklCogRowsPresent = false;
        check(initialWrite.status == ui::recovery::WriteStatus::published
                && metadataFreeWrite.status == ui::recovery::WriteStatus::published
                && initialWrite.levelStateHash
                    == ui::recovery::authoredLevelStateHash(
                        expectedPending.current().level)
                && metadataFreeWrite.levelStateHash
                    == ui::recovery::authoredLevelStateHash(metadataFreeLevel)
                && metadataFreeWrite.levelStateHash == requiredOnlyDigest
                && initialWrite.levelStateHash != metadataFreeWrite.levelStateHash
                && ui::recovery::authoredLevelStateHash(markerOnlyHashLevel)
                    != ui::recovery::authoredLevelStateHash(absentMarkerHashLevel)
                && importedRecord
                && importedRecord->metadata.levelSha256 == requiredOnlyDigest,
            "document_recovery shares authored hashes between GUI baseline and worker, detects marker-only import changes, and keeps META bound to required Level bytes");
        const std::filesystem::path damagedPendingJimpPath =
            rotationRoot / "pending-cog-jimp-loss.sedrecovery";
        QByteArray pendingRecoveryBytes{};
        const bool pendingRecoveryCopied = readBytes(
                livePath, &pendingRecoveryBytes)
            && writeBytes(damagedPendingJimpPath, pendingRecoveryBytes);
        const bool pendingJimpDestroyed = pendingRecoveryCopied
            && destroyNativeSection(
                damagedPendingJimpPath, QByteArrayLiteral("jimp"));
        QString pendingJimpError{};
        const auto damagedPendingJimpRecord = pendingJimpDestroyed
            ? ui::recovery::readRecoveryRecord(
                damagedPendingJimpPath, &pendingJimpError)
            : std::optional<ui::recovery::RecoveryRecord>{};
        Level expectedAfterJimpLoss = expectedPending.current().level;
        expectedAfterJimpLoss.importedJklCogRowsPresent = false;
        expectedAfterJimpLoss.importedJklSurfaceRefs.clear();
        const bool optionalJimpLossKeepsRequiredLevel =
            damagedPendingJimpRecord
            && damagedPendingJimpRecord->decodedLevel
            && damagedPendingJimpRecord->partialRecovery
            && damagedPendingJimpRecord->corrupted
            && damagedPendingJimpRecord->missingSections.contains(
                QStringLiteral("jimp"))
            && !damagedPendingJimpRecord->missingSections.contains(
                QStringLiteral("cogs"))
            && !damagedPendingJimpRecord->missingSections.contains(
                QStringLiteral("sect"))
            && !damagedPendingJimpRecord->decodedLevel
                ->importedJklCogRowsPresent
            && damagedPendingJimpRecord->decodedLevel
                ->importedJklSurfaceRefs.empty()
            && assets::sedLevelsEqual(
                *damagedPendingJimpRecord->decodedLevel,
                expectedAfterJimpLoss)
            && ui::recovery::loadRecoveredLevel(
                *damagedPendingJimpRecord, &pendingJimpError).has_value();
        check(optionalJimpLossKeepsRequiredLevel,
            "document_recovery keeps the required Level and clears only import metadata after real DOCM jimp loss, without a required-Level binding mismatch");
        QString pendingReadError{};
        auto pendingRecord = initialWrite.status == ui::recovery::WriteStatus::published
            ? ui::recovery::readRecoveryRecord(livePath, &pendingReadError)
            : std::optional<ui::recovery::RecoveryRecord>{};
        const std::optional<QByteArray> writtenHistoryPayload = pendingRecord
            ? readNativeOptionalPayload(
                pendingRecord->levelPayload, { 'h', 'i', 's', 't' })
            : std::nullopt;
        bool historyImportExtensionValid = false;
        bool malformedHistoryImportRejected = false;
        if ( pendingRecord && pendingRecord->decodedLevel
            && writtenHistoryPayload && !writtenHistoryPayload->isEmpty() )
        {
            QCborParserError importHistoryParseError{};
            const QCborValue importHistoryValue = QCborValue::fromCbor(
                *writtenHistoryPayload, &importHistoryParseError);
            QCborMap importHistoryRoot = importHistoryValue.toMap();
            QCborMap importExtension = importHistoryRoot.value(
                QStringLiteral("jklCogImportV1")).toMap();
            const auto rejectsImportHistoryRoot = [&](const QCborMap& root)
            {
                QString error{};
                return !ui::recovery::deserializeEditorDocumentHistory(
                    ui::recovery::serializeCanonicalCbor(QCborValue(root)),
                    ProjectType::JKDF2,
                    *pendingRecord->decodedLevel,
                    &error)
                    && !error.isEmpty();
            };
            historyImportExtensionValid =
                importHistoryParseError.error == QCborError::NoError
                && importHistoryRoot.contains(
                    QStringLiteral("jklCogImportV1"))
                && importExtension.value(QStringLiteral("version"))
                    .toInteger(-1) == 1
                && importExtension.value(QStringLiteral("current"))
                    .isArray()
                && !importExtension.value(QStringLiteral("undo"))
                    .toArray().isEmpty();

            QCborMap wrongVersionExtension = importExtension;
            wrongVersionExtension.insert(QStringLiteral("version"), 2);
            QCborMap wrongVersionRoot = importHistoryRoot;
            wrongVersionRoot.insert(QStringLiteral(
                "jklCogImportV1"), wrongVersionExtension);
            const bool unsupportedVersionRejected =
                historyImportExtensionValid
                && rejectsImportHistoryRoot(wrongVersionRoot);

            QCborMap mismatchedCurrentExtension = importExtension;
            QCborArray currentImportState = mismatchedCurrentExtension.value(
                QStringLiteral("current")).toArray();
            if ( currentImportState.size() == 2 )
            {
                currentImportState[0] = false;
                mismatchedCurrentExtension.insert(QStringLiteral("current"),
                    currentImportState);
            }
            QCborMap mismatchedCurrentRoot = importHistoryRoot;
            mismatchedCurrentRoot.insert(QStringLiteral(
                "jklCogImportV1"), mismatchedCurrentExtension);
            const bool currentMismatchRejected = historyImportExtensionValid
                && rejectsImportHistoryRoot(mismatchedCurrentRoot);

            QCborMap duplicateIndexExtension = importExtension;
            QCborArray sparseUndo = duplicateIndexExtension.value(
                QStringLiteral("undo")).toArray();
            if ( !sparseUndo.isEmpty() )
            {
                sparseUndo.append(sparseUndo.at(0));
                duplicateIndexExtension.insert(QStringLiteral("undo"),
                    sparseUndo);
            }
            QCborMap duplicateIndexRoot = importHistoryRoot;
            duplicateIndexRoot.insert(QStringLiteral(
                "jklCogImportV1"), duplicateIndexExtension);
            const bool duplicateIndexRejected = historyImportExtensionValid
                && !importExtension.value(QStringLiteral("undo"))
                    .toArray().isEmpty()
                && rejectsImportHistoryRoot(duplicateIndexRoot);
            QCborMap omittedImportExtensionRoot = importHistoryRoot;
            omittedImportExtensionRoot.remove(QStringLiteral("jklCogImportV1"));
            const bool omittedImportExtensionRejected = historyImportExtensionValid
                && rejectsImportHistoryRoot(omittedImportExtensionRoot);
            QCborMap outOfRangeIndexExtension = importExtension;
            QCborArray outOfRangeUndo = outOfRangeIndexExtension.value(
                QStringLiteral("undo")).toArray();
            if ( !outOfRangeUndo.isEmpty() )
            {
                QCborArray entry = outOfRangeUndo.at(0).toArray();
                entry[0] = static_cast<qint64>(expectedPendingUndo);
                outOfRangeUndo[0] = entry;
                outOfRangeIndexExtension.insert(QStringLiteral("undo"),
                    outOfRangeUndo);
            }
            QCborMap outOfRangeIndexRoot = importHistoryRoot;
            outOfRangeIndexRoot.insert(QStringLiteral(
                "jklCogImportV1"), outOfRangeIndexExtension);
            const bool outOfRangeIndexRejected = historyImportExtensionValid
                && expectedPendingUndo > 0u
                && !importExtension.value(QStringLiteral("undo"))
                    .toArray().isEmpty()
                && rejectsImportHistoryRoot(outOfRangeIndexRoot);

            QCborMap invalidReferenceExtension = importExtension;
            QCborArray invalidReferenceUndo = invalidReferenceExtension.value(
                QStringLiteral("undo")).toArray();
            if ( !invalidReferenceUndo.isEmpty() )
            {
                QCborArray entry = invalidReferenceUndo.at(0).toArray();
                QCborArray references = entry.at(2).toArray();
                if ( !references.isEmpty() )
                {
                    QCborArray reference = references.at(0).toArray();
                    reference[1] = 99;
                    references[0] = reference;
                    entry[2] = references;
                    invalidReferenceUndo[0] = entry;
                    invalidReferenceExtension.insert(QStringLiteral("undo"),
                        invalidReferenceUndo);
                }
            }
            QCborMap invalidReferenceRoot = importHistoryRoot;
            invalidReferenceRoot.insert(QStringLiteral(
                "jklCogImportV1"), invalidReferenceExtension);
            const bool invalidReferenceRejected = historyImportExtensionValid
                && !importExtension.value(QStringLiteral("undo"))
                    .toArray().isEmpty()
                && rejectsImportHistoryRoot(invalidReferenceRoot);

            editor::Document multiImportHistory = expectedPending;
            multiImportHistory.pushUndo(
                "Second raw imported COG edit", editor::DocumentDirty::Cogs);
            if ( multiImportHistory.level().cogs.size() == 3u
                && multiImportHistory.level().cogs[1].values.size() == 1u )
            {
                multiImportHistory.level().cogs[1].values[0].stringValue = "9";
            }
            const QByteArray multiImportHistoryPayload =
                ui::recovery::serializeEditorDocumentHistory(multiImportHistory);
            const QCborValue multiImportHistoryValue = QCborValue::fromCbor(
                multiImportHistoryPayload);
            QCborMap multiImportHistoryRoot = multiImportHistoryValue.toMap();
            QCborMap multiImportExtension = multiImportHistoryRoot.value(
                QStringLiteral("jklCogImportV1")).toMap();
            QCborArray descendingUndo = multiImportExtension.value(
                QStringLiteral("undo")).toArray();
            bool nonascendingIndicesRejected = false;
            if ( descendingUndo.size() >= 2 )
            {
                const QCborValue first = descendingUndo.at(0);
                const QCborValue second = descendingUndo.at(1);
                descendingUndo[0] = second;
                descendingUndo[1] = first;
                multiImportExtension.insert(QStringLiteral("undo"),
                    descendingUndo);
                multiImportHistoryRoot.insert(QStringLiteral(
                    "jklCogImportV1"), multiImportExtension);
                QString error{};
                nonascendingIndicesRejected =
                    !ui::recovery::deserializeEditorDocumentHistory(
                        ui::recovery::serializeCanonicalCbor(
                            QCborValue(multiImportHistoryRoot)),
                        ProjectType::JKDF2,
                        multiImportHistory.current().level,
                        &error)
                    && !error.isEmpty();
            }
            malformedHistoryImportRejected = unsupportedVersionRejected
                && currentMismatchRejected && duplicateIndexRejected
                && outOfRangeIndexRejected && invalidReferenceRejected
                && omittedImportExtensionRejected
                && nonascendingIndicesRejected;
            if ( !historyImportExtensionValid || !malformedHistoryImportRejected )
            {
                std::cerr << "JKL COG HIST diagnostic: pendingRecord="
                    << static_cast<bool>(pendingRecord)
                    << " decodedLevel=" << static_cast<bool>(pendingRecord->decodedLevel)
                    << " deferredPayloadBytes=" << pendingRecord->historyPayload.size()
                    << " writtenPayloadBytes=" << writtenHistoryPayload->size()
                    << " parseError=" << static_cast<int>(importHistoryParseError.error)
                    << " rootHasExtension=" << importHistoryRoot.contains(
                        QStringLiteral("jklCogImportV1"))
                    << " extensionVersion=" << importExtension.value(
                        QStringLiteral("version")).toInteger(-1)
                    << " currentType=" << static_cast<int>(importExtension.value(
                        QStringLiteral("current")).type())
                    << " currentSize=" << importExtension.value(
                        QStringLiteral("current")).toArray().size()
                    << " undoRows=" << importExtension.value(
                        QStringLiteral("undo")).toArray().size()
                    << " redoRows=" << importExtension.value(
                        QStringLiteral("redo")).toArray().size()
                    << " unsupportedVersion=" << unsupportedVersionRejected
                    << " currentMismatch=" << currentMismatchRejected
                    << " duplicateIndex=" << duplicateIndexRejected
                    << " outOfRangeIndex=" << outOfRangeIndexRejected
                    << " invalidReference=" << invalidReferenceRejected
                    << " omittedExtension=" << omittedImportExtensionRejected
                    << " nonascending=" << nonascendingIndicesRejected
                    << " expectedUndo=" << expectedPendingUndo
                    << " expectedRedo=" << expectedPendingRedo
                    << '\n';
            }
        }
        check(historyImportExtensionValid && malformedHistoryImportRejected,
            "document_recovery writes the explicit current and sparse JKL COG HIST sidecar and rejects unsupported versions, current mismatch, invalid refs, and unordered or invalid sparse indices atomically");
        auto recoveredPending = pendingRecord
            ? ui::recovery::loadRecoveredDocument(
                std::move(*pendingRecord), &pendingReadError)
            : std::optional<ui::recovery::RecoveredDocument>{};
        const std::size_t recoveredUndoCount = recoveredPending
                && recoveredPending->history
            ? recoveredPending->history->undoEntryCount() : 0u;
        const std::size_t recoveredRedoCount = recoveredPending
                && recoveredPending->history
            ? recoveredPending->history->redoEntryCount() : 0u;
        const bool currentPendingExact = recoveredPending
            && hasPendingRows(recoveredPending->current.level, "8")
            && recoveredPending->history
            && recoveredUndoCount == 1u
            && recoveredRedoCount == 1u;
        bool recoveredHistoryExact = false;
        if (recoveredPending && recoveredPending->history)
        {
            editor::Document restored(recoveredPending->current);
            restored.restoreRecoveryHistory(
                recoveredPending->current,
                std::move(*recoveredPending->history));
            editor::DocumentDirty dirty{};
            const bool undoToRawSeven = restored.undo(&dirty)
                && dirty == editor::DocumentDirty::Cogs
                && hasPendingRows(restored.current().level, "7");
            const bool redoToRawEight = restored.redo(&dirty)
                && dirty == editor::DocumentDirty::Cogs
                && hasPendingRows(restored.current().level, "8");
            const bool redoToTypedDefaults = restored.redo(&dirty)
                && dirty == editor::DocumentDirty::Cogs
                && hasTypedDefaults(restored.current().level);
            const bool undoToRawEight = restored.undo(&dirty)
                && dirty == editor::DocumentDirty::Cogs
                && hasPendingRows(restored.current().level, "8");
            recoveredHistoryExact = undoToRawSeven && redoToRawEight
                && redoToTypedDefaults && undoToRawEight
                && restored.undoDepth() == expectedPendingUndo
                && restored.redoDepth() == expectedPendingRedo;
        }

        Level retriedAfterRecovery = recoveredPending
            ? recoveredPending->current.level : Level{};
        bool lateResourceRetryExact = false;
        try
        {
            if (recoveredPending)
            {
                assets::finalizeJklImportResources(
                    retriedAfterRecovery, availableResources);
                lateResourceRetryExact = hasTypedDefaults(retriedAfterRecovery);
            }
        }
        catch (const std::exception&)
        {
            lateResourceRetryExact = false;
        }
        check(initialWrite.status == ui::recovery::WriteStatus::published
                && initialWrite.warning.isEmpty()
                && currentPendingExact && recoveredHistoryExact
                && lateResourceRetryExact,
            "document_recovery preserves empty, partial, and raw JKL COG rows with their ordered surface map in current/Undo/Redo HIST, then hydrates exact defaults when resources appear");
        if ( !(initialWrite.status == ui::recovery::WriteStatus::published
                && initialWrite.warning.isEmpty()
                && currentPendingExact && recoveredHistoryExact
                && lateResourceRetryExact) )
        {
            std::cerr << "JKL COG restore diagnostic: writeStatus="
                << static_cast<int>(initialWrite.status)
                << " warning=" << initialWrite.warning.toStdString()
                << " pendingRecord=" << static_cast<bool>(pendingRecord)
                << " recoveredDocument=" << static_cast<bool>(recoveredPending)
                << " hasHistory=" << (recoveredPending
                    && recoveredPending->history.has_value())
                << " currentPendingExact=" << currentPendingExact
                << " recoveredHistoryExact=" << recoveredHistoryExact
                << " lateResourceRetryExact=" << lateResourceRetryExact
                << " historyUndo=" << (recoveredPending && recoveredPending->history
                    ? recoveredUndoCount : 0u)
                << " historyRedo=" << (recoveredPending && recoveredPending->history
                    ? recoveredRedoCount : 0u)
                << " currentRows=" << (recoveredPending
                    ? recoveredPending->current.level.cogs.size() : 0u)
                << " marker=" << (recoveredPending
                    && recoveredPending->current.level.importedJklCogRowsPresent)
                << " refs=" << (recoveredPending
                    ? recoveredPending->current.level.importedJklSurfaceRefs.size() : 0u)
                << " pendingError=" << pendingReadError.toStdString()
                << '\n';
        }

        gate->generation.store(2);
        editor::Document finalizedForRotation = expectedPending;
        const bool finalizedForRotationExact = finalizedForRotation.redo()
            && hasTypedDefaults(finalizedForRotation.current().level);
        auto rotationRequest = makeRequest(
            livePath, gate, 2, ProjectType::JKDF2, 0, false);
        rotationRequest.document = std::move(finalizedForRotation);
        rotationRequest.archivePreviousSnapshot = true;
        rotationRequest.archiveSourcePath = livePath;
        rotationRequest.maximumHistoricalSnapshots = 1;
        const auto rotationWrite = finalizedForRotationExact
            ? ui::recovery::writeRecoverySnapshot(std::move(rotationRequest))
            : ui::recovery::WriteResult{};
        const auto rotationHistory =
            ui::recovery::scanAutosaveHistory(rotationRoot);
        const auto archivedPending = rotationHistory.size() == 1u
            ? ui::recovery::readRecoveryRecord(
                rotationHistory.front().recoveryPath, &pendingReadError)
            : std::optional<ui::recovery::RecoveryRecord>{};
        const auto recoveredArchive = archivedPending
            ? ui::recovery::loadRecoveredDocument(
                std::move(*archivedPending), &pendingReadError)
            : std::optional<ui::recovery::RecoveredDocument>{};
        const auto rotatedLiveRecord = ui::recovery::readRecoveryRecord(
            livePath, &pendingReadError);
        const auto rotatedLive = rotatedLiveRecord
            ? ui::recovery::loadRecoveredDocument(
                std::move(*rotatedLiveRecord), &pendingReadError)
            : std::optional<ui::recovery::RecoveredDocument>{};
        check(rotationWrite.status == ui::recovery::WriteStatus::published
                && rotationWrite.warning.isEmpty()
                && rotationHistory.size() == 1u
                && recoveredArchive
                && hasPendingRows(recoveredArchive->current.level, "8")
                && recoveredArchive->history
                && recoveredArchive->history->undoEntryCount() == 1u
                && recoveredArchive->history->redoEntryCount() == 1u
                && rotatedLive && hasTypedDefaults(rotatedLive->current.level),
            "document_recovery autosave rotation archives the pending COG checkpoint with HIST metadata and publishes the later fully hydrated Level");

        Level withoutCogSection{};
        bool noCogFixtureValid = false;
        try
        {
            withoutCogSection = loadJkl(geometry);
            noCogFixtureValid = !withoutCogSection.importedJklCogRowsPresent
                && hasSurfaceMap(withoutCogSection)
                && withoutCogSection.cogs.empty();
        }
        catch (const std::exception&)
        {
            noCogFixtureValid = false;
        }
        const std::filesystem::path noCogPath = rotationRoot
            / "ordinary-no-cog.sedrecovery";
        auto noCogGate = std::make_shared<ui::recovery::PublicationGate>();
        noCogGate->generation.store(1);
        auto noCogRequest = makeRequest(
            noCogPath, noCogGate, 1, ProjectType::JKDF2, 0, false);
        editor::DocumentState noCogState{};
        noCogState.level = withoutCogSection;
        noCogRequest.document.clear(std::move(noCogState));
        const auto noCogWrite = noCogFixtureValid
            ? ui::recovery::writeRecoverySnapshot(std::move(noCogRequest))
            : ui::recovery::WriteResult{};
        const auto noCogRecord = noCogWrite.status
                == ui::recovery::WriteStatus::published
            ? ui::recovery::readRecoveryRecord(noCogPath, &pendingReadError)
            : std::optional<ui::recovery::RecoveryRecord>{};
        const std::optional<QByteArray> noCogHistoryPayload = noCogRecord
            ? readNativeOptionalPayload(
                noCogRecord->levelPayload, { 'h', 'i', 's', 't' })
            : std::nullopt;
        const std::optional<QByteArray> noCogJimpPayload = noCogRecord
            ? readNativeOptionalPayload(
                noCogRecord->levelPayload, { 'j', 'i', 'm', 'p' })
            : std::nullopt;
        QCborParserError noCogHistoryParseError{};
        const QCborValue noCogHistoryValue = noCogHistoryPayload
            ? QCborValue::fromCbor(
                *noCogHistoryPayload, &noCogHistoryParseError)
            : QCborValue{};
        const QCborMap noCogHistoryRoot = noCogHistoryValue.toMap();
        const bool noCogHistoryWireValid = noCogHistoryPayload
            && !noCogHistoryPayload->isEmpty()
            && noCogHistoryParseError.error == QCborError::NoError
            && noCogHistoryValue.isMap();
        const bool noCogHistoryEmpty = noCogRecord && noCogRecord->history
            && noCogRecord->history->undoEntryCount() == 0u
            && noCogRecord->history->redoEntryCount() == 0u;
        const bool noCogImportExtensionAbsent = noCogHistoryWireValid
            && !noCogHistoryRoot.contains(QStringLiteral("jklCogImportV1"));
        check(noCogWrite.status == ui::recovery::WriteStatus::published
                && noCogWrite.warning.isEmpty()
                && noCogRecord && noCogRecord->decodedLevel
                && noCogRecord->warnings.isEmpty()
                && !noCogRecord->corrupted && !noCogRecord->partialRecovery
                && noCogRecord->missingSections.isEmpty()
                && !noCogRecord->decodedLevel->importedJklCogRowsPresent
                && noCogRecord->decodedLevel->importedJklSurfaceRefs.empty()
                && noCogRecord->history
                && noCogHistoryEmpty && noCogHistoryWireValid
                && noCogImportExtensionAbsent && !noCogJimpPayload,
            "document_recovery accepts ordinary imported geometry with no COG rows and drops its unused JKL surface map without creating HIST failure or metadata churn");
        if ( !(noCogWrite.status == ui::recovery::WriteStatus::published
                && noCogWrite.warning.isEmpty()
                && noCogRecord && noCogRecord->decodedLevel
                && noCogRecord->warnings.isEmpty()
                && !noCogRecord->corrupted && !noCogRecord->partialRecovery
                && noCogRecord->missingSections.isEmpty()
                && !noCogRecord->decodedLevel->importedJklCogRowsPresent
                && noCogRecord->decodedLevel->importedJklSurfaceRefs.empty()
                && noCogRecord->history
                && noCogHistoryEmpty && noCogHistoryWireValid
                && noCogImportExtensionAbsent && !noCogJimpPayload) )
        {
            std::cerr << "ordinary no-COG recovery diagnostic: fixtureValid="
                << noCogFixtureValid
                << " writeStatus=" << static_cast<int>(noCogWrite.status)
                << " warning=" << noCogWrite.warning.toStdString()
                << " record=" << static_cast<bool>(noCogRecord)
                << " decodedLevel=" << (noCogRecord
                    && noCogRecord->decodedLevel)
                << " marker=" << (noCogRecord && noCogRecord->decodedLevel
                    && noCogRecord->decodedLevel->importedJklCogRowsPresent)
                << " refs=" << (noCogRecord && noCogRecord->decodedLevel
                    ? noCogRecord->decodedLevel->importedJklSurfaceRefs.size() : 0u)
                << " history=" << (noCogRecord
                    && noCogRecord->history.has_value())
                << " undo=" << (noCogRecord && noCogRecord->history
                    ? noCogRecord->history->undoEntryCount() : 0u)
                << " redo=" << (noCogRecord && noCogRecord->history
                    ? noCogRecord->history->redoEntryCount() : 0u)
                << " historyPayloadBytes=" << (noCogHistoryPayload
                    ? noCogHistoryPayload->size() : -1)
                << " historyParseError=" << static_cast<int>(
                    noCogHistoryParseError.error)
                << " historyWireValid=" << noCogHistoryWireValid
                << " importExtension=" << noCogHistoryRoot.contains(
                    QStringLiteral("jklCogImportV1"))
                << " jimpPresent=" << noCogJimpPayload.has_value()
                << " warnings=" << (noCogRecord
                    ? noCogRecord->warnings.join(QStringLiteral(" | "))
                        .toStdString() : std::string{})
                << " missingSections=" << (noCogRecord
                    ? noCogRecord->missingSections.join(QStringLiteral(","))
                        .toStdString() : std::string{})
                << " readError=" << pendingReadError.toStdString()
                << '\n';
        }
    }

    // Native MotS persistence intentionally filters known off-flag zero ARCH
    // bindings on a private snapshot and compacts retained table indices.
    // Recovery publication must validate against that canonical native Level
    // while leaving the live editor Level untouched.
    {
        const std::filesystem::path archFilterPath =
            root / "mots-arch-filtered-recovery.sedrecovery";
        auto archFilterGate = std::make_shared<ui::recovery::PublicationGate>();
        archFilterGate->generation.store(1);
        auto request = makeRequest(
            archFilterPath, archFilterGate, 1, ProjectType::MOTS, 1, false);

        const auto makeThing = [](
            const std::int32_t id,
            const std::int32_t num,
            const std::int32_t archIndex,
            const std::int32_t flagsValue)
        {
            Thing thing{};
            thing.id = id;
            thing.num = num;
            thing.sector = 0;
            thing.templateName = "arch_actor";
            thing.name = "arch_actor-" + std::to_string(num);
            thing.archLightingIndex = archIndex;
            TplValue flags{};
            flags.name = "thingflags";
            flags.type = ValueType::integer;
            flags.intValue = flagsValue;
            thing.values.push_back(std::move(flags));
            return thing;
        };
        const auto zeroRecord = [](const std::int32_t wireIndex)
        {
            MotsArchLightObject object{};
            object.wireIndex = wireIndex;
            object.meshes.push_back(MotsArchLightMesh{
                0, { MotsArchLightVertex{} } });
            return object;
        };

        Level& liveLevel = request.document.level();
        liveLevel.things.push_back(makeThing(700, 0, 0, 0));
        liveLevel.things.push_back(makeThing(701, 1, 1, 0x8));
        MotsArchLightObject retained{};
        retained.wireIndex = 1;
        retained.meshes.push_back(MotsArchLightMesh{
            0, { MotsArchLightVertex{ 0.25f, 0.5f, 0.75f, 1.0f } } });
        liveLevel.motsArchLighting = {
            zeroRecord(0), retained, zeroRecord(2) };
        liveLevel.lvisstring = "recovery preserves exact non-ARCH Level fields";
        const Level liveLevelBeforeWrite = liveLevel;
        const auto nativeBytes = assets::saveToSedBytes(liveLevelBeforeWrite);
        const assets::SedFile canonicalNative = assets::loadSedFileBytes(nativeBytes);

        const auto archFilterWrite = ui::recovery::writeRecoverySnapshot(request);
        QString archFilterError{};
        const auto archFilterRecord =
            archFilterWrite.status == ui::recovery::WriteStatus::published
            ? ui::recovery::readRecoveryRecord(archFilterPath, &archFilterError)
            : std::optional<ui::recovery::RecoveryRecord>{};
        const auto recoveredArchFilterLevel = archFilterRecord
            ? ui::recovery::loadRecoveredLevel(*archFilterRecord, &archFilterError)
            : std::optional<Level>{};
        check(archFilterWrite.status == ui::recovery::WriteStatus::published
                && archFilterWrite.warning.isEmpty()
                && archFilterRecord && !archFilterRecord->corrupted
                && recoveredArchFilterLevel
                && canonicalNative.level.motsArchLighting.size() == 1u
                && canonicalNative.level.things.size() == 2u
                && canonicalNative.level.things[0].archLightingIndex == -1
                && canonicalNative.level.things[1].archLightingIndex == 0
                && assets::sedLevelsEqual(
                    *recoveredArchFilterLevel, canonicalNative.level)
                && assets::sedLevelsEqual(
                    request.document.current().level, liveLevelBeforeWrite)
                && liveLevel.motsArchLighting.size() == 3u
                && liveLevel.things[0].archLightingIndex == 0
                && liveLevel.things[1].archLightingIndex == 1,
            "document_recovery validates the exact native-canonical MotS ARCH filter and index remap without mutating the live Level");
    }

    // Native DOCM canonicalization also applies to historical Level frames.
    // Keep a real Undo and Redo state across the dormant-zero filter and
    // retained-index compaction; recovery must not silently omit HIST when
    // the authored editor Levels contain the pre-canonical ARCH layout.
    {
        const std::filesystem::path archHistoryPath =
            root / "mots-arch-filtered-history-recovery.sedrecovery";
        auto archHistoryGate =
            std::make_shared<ui::recovery::PublicationGate>();
        archHistoryGate->generation.store(1);
        auto request = makeRequest(
            archHistoryPath, archHistoryGate, 1, ProjectType::MOTS, 1, false);
        request.document.level().lvisstring = "filtered ARCH history base";
        const editor::DocumentState initialState = request.document.current();

        const auto makeThing = [](
            const std::int32_t id,
            const std::int32_t num,
            const std::int32_t archIndex,
            const std::int32_t flagsValue)
        {
            Thing thing{};
            thing.id = id;
            thing.num = num;
            thing.sector = 0;
            thing.templateName = "arch_actor";
            thing.name = "arch_actor-" + std::to_string(num);
            thing.archLightingIndex = archIndex;
            TplValue flags{};
            flags.name = "thingflags";
            flags.type = ValueType::integer;
            flags.intValue = flagsValue;
            thing.values.push_back(std::move(flags));
            return thing;
        };
        const auto zeroRecord = [](const std::int32_t wireIndex)
        {
            MotsArchLightObject object{};
            object.wireIndex = wireIndex;
            object.meshes.push_back(MotsArchLightMesh{
                0, { MotsArchLightVertex{} } });
            return object;
        };

        request.document.pushUndo(
            "before filtered MotS ARCH setup", editor::DocumentDirty::Things);
        Level& liveLevel = request.document.level();
        liveLevel.things.push_back(makeThing(800, 0, 0, 0));
        liveLevel.things.push_back(makeThing(801, 1, 1, 0x8));
        MotsArchLightObject retained{};
        retained.wireIndex = 1;
        retained.meshes.push_back(MotsArchLightMesh{
            0, { MotsArchLightVertex{ 0.25f, 0.5f, 0.75f, 1.0f } } });
        liveLevel.motsArchLighting = {
            zeroRecord(0), retained, zeroRecord(2) };

        request.document.pushUndo(
            "before retained MotS ARCH edit", editor::DocumentDirty::Things);
        liveLevel.motsArchLighting[1].meshes[0].vertices[0].green += 0.125f;
        const editor::DocumentState expectedRedoRaw = request.document.current();
        const bool timelineMovedBack = request.document.undo();
        const editor::DocumentState expectedCurrentRaw = request.document.current();

        const auto canonicalNativeLevel = [](const Level& level)
        {
            return assets::loadSedFileBytes(assets::saveToSedBytes(level)).level;
        };
        editor::DocumentState expectedInitial = initialState;
        expectedInitial.level = canonicalNativeLevel(initialState.level);
        editor::DocumentState expectedCurrent = expectedCurrentRaw;
        expectedCurrent.level = canonicalNativeLevel(expectedCurrentRaw.level);
        editor::DocumentState expectedRedo = expectedRedoRaw;
        expectedRedo.level = canonicalNativeLevel(expectedRedoRaw.level);

        const auto archHistoryWrite = ui::recovery::writeRecoverySnapshot(request);
        QString archHistoryError{};
        auto archHistoryRecord =
            archHistoryWrite.status == ui::recovery::WriteStatus::published
            ? ui::recovery::readRecoveryRecord(archHistoryPath, &archHistoryError)
            : std::optional<ui::recovery::RecoveryRecord>{};
        const bool serializedHistoryPresent = archHistoryRecord
            && archHistoryRecord->history
            && archHistoryRecord->history->undoEntryCount() == 1
            && archHistoryRecord->history->redoEntryCount() == 1;
        auto recoveredArchHistory = archHistoryRecord
            ? ui::recovery::loadRecoveredDocument(
                std::move(*archHistoryRecord), &archHistoryError)
            : std::optional<ui::recovery::RecoveredDocument>{};

        bool recoveredUndoRedoExact = recoveredArchHistory
            && recoveredArchHistory->history
            && recoveredArchHistory->history->undoEntryCount() == 1
            && recoveredArchHistory->history->redoEntryCount() == 1
            && recoveredArchHistory->missingSections.isEmpty();
        if (recoveredUndoRedoExact)
        {
            editor::Document restored(recoveredArchHistory->current);
            restored.restoreRecoveryHistory(
                recoveredArchHistory->current, *recoveredArchHistory->history);
            recoveredUndoRedoExact = restored.undo()
                && sameDocumentState(restored.current(), expectedInitial)
                && restored.redo()
                && sameDocumentState(restored.current(), expectedCurrent)
                && restored.redo()
                && sameDocumentState(restored.current(), expectedRedo);
        }

        check(timelineMovedBack
                && archHistoryWrite.status == ui::recovery::WriteStatus::published
                && archHistoryWrite.warning.isEmpty()
                && serializedHistoryPresent
                && recoveredArchHistory
                && recoveredArchHistory->current.level.motsArchLighting.size() == 1u
                && recoveredArchHistory->current.level.things.size() == 2u
                && recoveredArchHistory->current.level.things[0].archLightingIndex == -1
                && recoveredArchHistory->current.level.things[1].archLightingIndex == 0
                && recoveredUndoRedoExact
                && archHistoryError.isEmpty(),
            "document_recovery retains and restores exact MotS Undo/Redo history through native ARCH filtering and index compaction");
    }

    // A MotS automatic 0x20 bake is a Lights-only undo transition even though
    // its generated ARCH records, owner references, and baseline provenance
    // are serialized in the adjacent Things frame. Recovery must retain HIST.
    {
        const auto archPath = root / "mots-arch-light-auto-bake.sedrecovery";
        auto archGate = std::make_shared<ui::recovery::PublicationGate>();
        archGate->generation.store(1);
        auto request = makeRequest(
            archPath, archGate, 1, ProjectType::MOTS, 1, false);
        Level& level = request.document.level();
        Thing owner{};
        owner.id = 700;
        owner.num = 0;
        owner.sector = 0;
        owner.templateName = "arch_actor";
        owner.name = "arch_actor";
        TplValue flags{};
        flags.name = "thingflags";
        flags.type = ValueType::integer;
        flags.intValue = 0x8;
        owner.values.push_back(flags);
        TplValue model{};
        model.name = "model3d";
        model.type = ValueType::str;
        model.adjType = AdjType::model3do;
        model.stringValue = "arch_actor.3do";
        owner.values.push_back(model);
        level.things.push_back(owner);
        SedLight light{};
        light.id = 701;
        level.lights.push_back(light);
        request.document.pushUndo(
            "MotS ARCH auto bake", editor::DocumentDirty::Lights);
        level.lights.front().flags |= LF_AffectArchThings;
        MotsArchLightObject generated{};
        generated.wireIndex = 0;
        generated.meshes.push_back(MotsArchLightMesh{
            0, { MotsArchLightVertex{ 0.25f, 0.5f, 0.75f, 1.0f } } });
        level.motsArchLighting.push_back(generated);
        level.things.front().archLightingIndex = 0;
        level.motsArchLightingBakeBaselines.push_back(
            MotsArchLightingBakeBaseline{
                owner.id, "arch_actor.3do", -1, std::nullopt });
        const auto archWrite = ui::recovery::writeRecoverySnapshot(request);
        QString archReadError{};
        const auto archRecord = archWrite.status == ui::recovery::WriteStatus::published
                && archWrite.warning.isEmpty()
            ? ui::recovery::readRecoveryRecord(archPath, &archReadError)
            : std::optional<ui::recovery::RecoveryRecord>{};
        const auto archivedLevel = archRecord
            ? ui::recovery::loadRecoveredLevel(*archRecord, &archReadError)
            : std::optional<Level>{};
        const bool archRecoveryAccepted =
            archWrite.status == ui::recovery::WriteStatus::published
            && archWrite.warning.isEmpty()
            && archRecord && archRecord->history
            && archRecord->history->undoEntryCount() == 1
            && archivedLevel
            && archivedLevel->motsArchLightingBakeBaselines.size() == 1
            && archivedLevel->things.size() == 1
            && archivedLevel->things.front().archLightingIndex == 0;
        if (!archRecoveryAccepted)
        {
            std::cerr << "MotS ARCH recovery diagnostic: status="
                      << static_cast<int>(archWrite.status)
                      << " warning=" << archWrite.warning.toStdString()
                      << " writeError=" << archWrite.error.toStdString()
                      << " readError=" << archReadError.toStdString()
                      << " record=" << static_cast<bool>(archRecord)
                      << " history=" << (archRecord && archRecord->history.has_value())
                      << " level=" << static_cast<bool>(archivedLevel) << "\n";
        }
        check(archRecoveryAccepted
                && archWrite.warning.isEmpty()
                && archRecord && archRecord->history
                && archRecord->history->undoEntryCount() == 1
                && archivedLevel
                && archivedLevel->motsArchLightingBakeBaselines.size() == 1
                && archivedLevel->things.size() == 1
                && archivedLevel->things.front().archLightingIndex == 0,
            "document_recovery retains HIST without the dirty-domain warning for an adjacent MotS 0x20 ARCH bake");

        const auto publishCheckpoint = [&request](
            const std::filesystem::path& recoveryPath,
            const std::uint64_t generation,
            const std::size_t expectedUndoEntries)
        {
            auto gate = std::make_shared<ui::recovery::PublicationGate>();
            gate->generation.store(generation);
            request.recoveryPath = recoveryPath;
            request.publicationGate = gate;
            request.generation = generation;
            request.metadata.undoSignature = generation * 17;
            request.metadata.timestamp = QDateTime::currentMSecsSinceEpoch();

            const auto write = ui::recovery::writeRecoverySnapshot(request);
            QString readError{};
            const auto record = write.status == ui::recovery::WriteStatus::published
                ? ui::recovery::readRecoveryRecord(recoveryPath, &readError)
                : std::optional<ui::recovery::RecoveryRecord>{};
            const editor::DocumentRecoveryHistory portableHistory =
                request.document.recoveryHistory();
            const bool portableTransitionAccepts =
                !portableHistory.undoEntries.empty()
                && assets::sedLevelsEqualIgnoringHistoryDomains(
                    portableHistory.undoEntries.front().state.level,
                    request.document.current().level,
                    false, true, false);
            const auto exactHistory = record && record->history
                && record->history->undoEntryCount() == expectedUndoEntries
                && verifyRestoredHistory(request.document, *record->history);
            bool undoRedoRoundTrip = false;
            if ( exactHistory )
            {
                editor::Document restored(request.document.current());
                restored.restoreRecoveryHistory(
                    restored.current(), *record->history);
                const QByteArray currentLevel =
                    canonicalNestedLevel(restored.current().level);
                editor::DocumentDirty dirty{};
                undoRedoRoundTrip = restored.undo(&dirty)
                    && dirty == editor::DocumentDirty::Lights
                    && restored.redo(&dirty)
                    && dirty == editor::DocumentDirty::Lights
                    && canonicalNestedLevel(restored.current().level)
                        == currentLevel;
            }
            const bool accepted =
                write.status == ui::recovery::WriteStatus::published
                && write.warning.isEmpty()
                && record && !record->corrupted && exactHistory
                && portableTransitionAccepts && undoRedoRoundTrip;
            if ( !accepted )
            {
                std::cerr << "Repeated MotS ARCH recovery diagnostic: warning="
                          << write.warning.toStdString()
                          << " writeError=" << write.error.toStdString()
                          << " readError=" << readError.toStdString()
                          << " history=" << (record && record->history.has_value())
                          << " portableTransition=" << portableTransitionAccepts
                          << "\n";
            }
            return accepted;
        };

        request.document.pushUndo(
            "Move existing MotS baked light", editor::DocumentDirty::Lights);
        request.document.level().lights.front().position.x += 0.25;
        request.document.level().motsArchLighting[0]
            .meshes[0].vertices[0].red += 0.125f;
        check(publishCheckpoint(
                root / "mots-arch-light-position-edit.sedrecovery", 2, 2),
            "document_recovery keeps two-entry HIST after moving an already baked MotS 0x20 light");

        request.document.pushUndo(
            "Recolor existing MotS baked light", editor::DocumentDirty::Lights);
        request.document.level().lights.front().color.r = 0.75f;
        request.document.level().motsArchLighting[0]
            .meshes[0].vertices[0].red += 0.25f;
        check(publishCheckpoint(
                root / "mots-arch-light-color-edit.sedrecovery", 3, 3),
            "document_recovery keeps three-entry HIST after consecutive position and color edits to an already baked MotS 0x20 light");
    }

    // HistoryNested uses the same native version in its fixed header and all
    // eight section descriptors. Adding or removing the first/last ARCH
    // record changes that version, so both stack directions must carry their
    // own canonical header prefix instead of reusing the current Level's.
    {
        const auto nestedVersion = [](const Level& level)
        {
            const std::vector<std::byte> bytes = assets::saveToSedBytes(
                level, {}, assets::SedSerializationProfile::HistoryNested);
            if (bytes.size() < 8u)
            {
                throw std::logic_error(
                    "HistoryNested test document is shorter than its version field.");
            }
            std::uint32_t version{};
            for (std::size_t byte = 0; byte < 4u; ++byte)
            {
                version |= static_cast<std::uint32_t>(
                    std::to_integer<std::uint8_t>(bytes[4u + byte]))
                    << (byte * 8u);
            }
            return version;
        };
        const auto verifyVersionTransition = [&root, &nestedVersion](
            const UvCoordinateSpace coordinateSpace,
            const std::uint32_t withoutArchVersion,
            const std::uint32_t withArchVersion,
            const std::string_view suffix)
        {
            std::string stage = "create fixture";
            try
            {
                auto gate = std::make_shared<ui::recovery::PublicationGate>();
                gate->generation.store(1);
                auto request = makeRequest(
                    root / ("mots-arch-version-" + std::string(suffix)
                        + ".sedrecovery"),
                    gate, 1, ProjectType::MOTS, 1, false);
                Level& initialLevel = request.document.level();
                initialLevel.uvCoordinateSpace = coordinateSpace;
                Thing owner{};
                owner.id = 800;
                owner.num = 0;
                owner.sector = 0;
                owner.templateName = "arch_actor";
                owner.name = "arch_actor";
                TplValue flags{};
                flags.name = "thingflags";
                flags.type = ValueType::integer;
                flags.intValue = 0x8;
                owner.values.push_back(flags);
                TplValue model{};
                model.name = "model3d";
                model.type = ValueType::str;
                model.adjType = AdjType::model3do;
                model.stringValue = "arch_actor.3do";
                owner.values.push_back(model);
                initialLevel.things.push_back(std::move(owner));
                SedLight light{};
                light.id = 801;
                initialLevel.lights.push_back(light);
                request.document.pushUndo(
                    "MotS ARCH version transition",
                    editor::DocumentDirty::Lights);
                Level& authoredLevel = request.document.level();
                if (authoredLevel.lights.size() != 1u
                    || authoredLevel.things.size() != 1u)
                {
                    return false;
                }
                authoredLevel.lights.front().flags |= LF_AffectArchThings;
                MotsArchLightObject record{};
                record.wireIndex = 0;
                record.meshes.push_back(MotsArchLightMesh{
                    0, { MotsArchLightVertex{ 0.25f, 0.5f, 0.75f, 1.0f } } });
                authoredLevel.motsArchLighting.push_back(record);
                authoredLevel.things.front().archLightingIndex = 0;
                authoredLevel.motsArchLightingBakeBaselines.push_back(
                    MotsArchLightingBakeBaseline{
                        800, "arch_actor.3do", -1, std::nullopt });

                stage = "encode/decode undo history";
                QString forwardError{};
                ui::recovery::detail::RecoveryHistoryEncodeStats forwardStats{};
                const QByteArray forwardPayload =
                    ui::recovery::detail::serializeEditorDocumentHistoryWithStatsForTesting(
                        request.document, &forwardStats);
                auto forwardHistory =
                    ui::recovery::deserializeEditorDocumentHistoryForRestore(
                        forwardPayload, ProjectType::MOTS,
                        request.document.current().level, &forwardError);
                if (!forwardHistory || !forwardError.isEmpty()
                    || nestedVersion(request.document.current().level)
                        != withArchVersion
                    || forwardStats.reusedFramesBySection[7] != 0
                    || forwardStats.fragmentedFramesBySection[7] == 0)
                {
                    std::cerr << "MotS ARCH version regression (" << suffix
                              << ") at " << stage << ": "
                              << forwardError.toStdString() << '\n';
                    return false;
                }

                stage = "undo to prior version";
                editor::Document reversed(request.document.current());
                reversed.restoreRecoveryHistory(
                    reversed.current(), std::move(*forwardHistory));
                if (!reversed.undo())
                {
                    return false;
                }
                const Level& reversedLevel = reversed.current().level;
                if (nestedVersion(reversedLevel) != withoutArchVersion
                    || !reversedLevel.motsArchLighting.empty()
                    || !reversedLevel.motsArchLightingBakeBaselines.empty()
                    || reversedLevel.lights.size() != 1u
                    || (reversedLevel.lights.front().flags
                        & LF_AffectArchThings) != 0)
                {
                    std::cerr << "MotS ARCH version regression (" << suffix
                              << ") at " << stage
                              << ": reconstructed prior Level did not match its native version.\n";
                    return false;
                }

                stage = "encode/decode redo history";
                QString reverseError{};
                ui::recovery::detail::RecoveryHistoryEncodeStats reverseStats{};
                const QByteArray reversePayload =
                    ui::recovery::detail::serializeEditorDocumentHistoryWithStatsForTesting(
                        reversed, &reverseStats);
                auto reverseHistory =
                    ui::recovery::deserializeEditorDocumentHistoryForRestore(
                        reversePayload, ProjectType::MOTS,
                        reversed.current().level, &reverseError);
                if (!reverseHistory || !reverseError.isEmpty()
                    || reverseStats.reusedFramesBySection[7] != 0
                    || reverseStats.fragmentedFramesBySection[7] == 0)
                {
                    std::cerr << "MotS ARCH version regression (" << suffix
                              << ") at " << stage << ": "
                              << reverseError.toStdString() << '\n';
                    return false;
                }

                stage = "redo to ARCH version";
                editor::Document replayed(reversed.current());
                replayed.restoreRecoveryHistory(
                    replayed.current(), std::move(*reverseHistory));
                if (!replayed.redo())
                {
                    return false;
                }
                const Level& replayedLevel = replayed.current().level;
                return nestedVersion(replayedLevel) == withArchVersion
                    && replayedLevel.motsArchLighting.size() == 1u
                    && replayedLevel.things.size() == 1u
                    && replayedLevel.things.front().archLightingIndex == 0
                    && replayedLevel.motsArchLightingBakeBaselines.size() == 1u
                    && replayedLevel.lights.size() == 1u
                    && (replayedLevel.lights.front().flags
                        & LF_AffectArchThings) != 0;
            }
            catch (const std::exception& exception)
            {
                std::cerr << "MotS ARCH version regression (" << suffix
                          << ") at " << stage << ": "
                          << exception.what() << '\n';
                return false;
            }
        };

        const bool normalizedVersionTransition = verifyVersionTransition(
            UvCoordinateSpace::Normalized,
            assets::SedBinaryVersion,
            assets::SedArchNormalizedBinaryVersion,
            "normalized");
        const bool pixelVersionTransition = verifyVersionTransition(
            UvCoordinateSpace::TexturePixels,
            assets::SedLegacyBinaryVersion,
            assets::SedArchPixelBinaryVersion,
            "pixel");
        check(normalizedVersionTransition && pixelVersionTransition,
            "document_recovery preserves both ARCH version transitions in undo and redo HIST states");
    }

    // Autosave and crash recovery intentionally share writeRecoverySnapshot
    // and the same .sedrecovery container. Exercise that production path with
    // the checked-in native SED and canonical JKL/NDY conformance vectors.
    struct RecoveryVector final
    {
        ProjectType kind{};
        const char* stem{};
        const char* extension{};
    };
    constexpr std::array<RecoveryVector, 3> recoveryVectors{{
        { ProjectType::JKDF2, "jkdf2", ".jkl" },
        { ProjectType::MOTS, "mots", ".jkl" },
        { ProjectType::IJIM, "ijim", ".ndy" },
    }};
    const std::filesystem::path vectorRoot =
        "src/core/tests/assets/fixtures/sed_vectors";
    for (const RecoveryVector& vector : recoveryVectors)
    {
        QByteArray nativeBytes{};
        QByteArray expectedText{};
        QByteArray expectedPartialText{};
        const bool vectorsRead = readBytes(
                vectorRoot / (std::string(vector.stem) + "_native.sed"),
                &nativeBytes)
            && readBytes(vectorRoot / (std::string(vector.stem)
                    + "_expected" + vector.extension), &expectedText)
            && readBytes(vectorRoot / (std::string(vector.stem)
                    + "_expected_partial" + vector.extension),
                &expectedPartialText);
        std::optional<Level> source{};
        try
        {
            if (vectorsRead)
            {
                source = assets::loadFromSedBytes(std::as_bytes(std::span(
                    nativeBytes.constData(),
                    static_cast<std::size_t>(nativeBytes.size()))));
            }
        }
        catch (...)
        {
            source.reset();
        }

        const std::filesystem::path recoveryPath = root
            / (std::string(vector.stem) + "-vector-autosave.sedrecovery");
        auto vectorGate = std::make_shared<ui::recovery::PublicationGate>();
        vectorGate->generation.store(1);
        auto request = makeRequest(recoveryPath, vectorGate, 1,
            vector.kind, 0, false);
        if (source)
        {
            editor::DocumentState state{};
            state.level = *source;
            request.document.clear(std::move(state));
            request.document.pushUndo("checked-in vector checkpoint");
        }
        const auto vectorWrite = source
            ? ui::recovery::writeRecoverySnapshot(std::move(request))
            : ui::recovery::WriteResult{};
        QString vectorError{};
        const auto record = vectorWrite.status
                == ui::recovery::WriteStatus::published
            ? ui::recovery::readRecoveryRecord(recoveryPath, &vectorError)
            : std::optional<ui::recovery::RecoveryRecord>{};
        const auto restored = record
            ? ui::recovery::loadRecoveredLevel(*record, &vectorError)
            : std::optional<Level>{};
        std::ostringstream restoredText{};
        if (restored) assets::saveToJKL(*restored, restoredText);
        check(vectorsRead && source && record && restored
                && !record->corrupted && record->history
                && restored->kind == vector.kind
                && assets::sedLevelsEqual(*source, *restored)
                && restoredText.str()
                    == std::string(expectedText.constData(),
                        static_cast<std::size_t>(expectedText.size())),
            "document_recovery autosave opens checked-in native SED and exports the exact project-specific JKL/NDY vector");

        const bool destroyed = record
            && destroyNativeSection(recoveryPath, QByteArrayLiteral("sect"));
        const auto partialRecord = destroyed
            ? ui::recovery::readRecoveryRecord(recoveryPath, &vectorError)
            : std::optional<ui::recovery::RecoveryRecord>{};
        const auto partialLevel = partialRecord
            ? ui::recovery::loadRecoveredLevel(*partialRecord, &vectorError)
            : std::optional<Level>{};
        std::ostringstream partialText{};
        if (partialLevel) assets::saveToJKL(*partialLevel, partialText);
        ui::recovery::PartialLevelSalvageResult partialArtifact{};
        if (partialRecord && partialLevel)
        {
            ui::recovery::PartialLevelSalvageContext context{};
            context.sourceRecoveryPath = recoveryPath;
            context.forensicRecoveryPath = recoveryPath;
            context.missingRequiredSections = {
                QStringLiteral("sect") };
            context.recoveryWarnings = partialRecord->warnings;
            context.artifactId = QStringLiteral("recovery-vector-%1")
                .arg(QString::fromLatin1(vector.stem));
            partialArtifact = ui::recovery::writePartialLevelSalvage(
                *partialLevel, context);
        }
        QByteArray partialArtifactText{};
        if (partialArtifact.wroteNativeText())
            (void)readBytes(partialArtifact.nativeTextPath,
                &partialArtifactText);
        check(partialRecord && partialLevel && partialRecord->partialRecovery
                && partialRecord->missingSections.contains(
                    QStringLiteral("sect"))
                && partialLevel->kind == vector.kind
                && partialText.str()
                    == std::string(expectedPartialText.constData(),
                        static_cast<std::size_t>(expectedPartialText.size()))
                && partialArtifact.wroteManifest()
                && partialArtifactText == expectedPartialText,
            "document_recovery beyond-parity autosave damage publishes the exact checked-in partial JKL/NDY salvage vector and manifest");
    }

    {
        QByteArray resourceBytes{};
        const bool resourceRead = readBytes(vectorRoot
                / "all_resource_categories.sed", &resourceBytes);
        std::optional<Level> resourceSource{};
        try
        {
            if (resourceRead)
            {
                resourceSource = assets::loadFromSedBytes(std::as_bytes(
                    std::span(resourceBytes.constData(),
                        static_cast<std::size_t>(resourceBytes.size()))));
            }
        }
        catch (...)
        {
            resourceSource.reset();
        }
        const std::filesystem::path resourceRecovery = root
            / "all-resource-categories-autosave.sedrecovery";
        auto resourceGate = std::make_shared<ui::recovery::PublicationGate>();
        resourceGate->generation.store(1);
        auto request = makeRequest(resourceRecovery, resourceGate, 1,
            ProjectType::IJIM, 0, false);
        if (resourceSource)
        {
            editor::DocumentState state{};
            state.level = *resourceSource;
            request.document.clear(std::move(state));
        }
        const auto resourceWrite = resourceSource
            ? ui::recovery::writeRecoverySnapshot(std::move(request))
            : ui::recovery::WriteResult{};
        QString resourceError{};
        const auto resourceRecord = resourceWrite.status
                == ui::recovery::WriteStatus::published
            ? ui::recovery::readRecoveryRecord(
                resourceRecovery, &resourceError)
            : std::optional<ui::recovery::RecoveryRecord>{};
        const auto resourceRestored = resourceRecord
            ? ui::recovery::loadRecoveredLevel(
                *resourceRecord, &resourceError)
            : std::optional<Level>{};
        check(resourceSource && resourceRecord && resourceRestored
                && assets::sedLevelsEqual(
                    *resourceSource, *resourceRestored),
            "document_recovery autosave preserves every categorized resource and cross-section local index from the checked-in SED vector");
    }

    const auto slotA = ui::recovery::recoveryFilePath(
        root, QStringLiteral("file:C:/levels/level.jkl"), QStringLiteral("session-a"),
        QStringLiteral("level.jkl"), ProjectType::JKDF2);
    const auto slotB = ui::recovery::recoveryFilePath(
        root, QStringLiteral("file:C:/levels/level.jkl"), QStringLiteral("session-b"),
        QStringLiteral("level.jkl"), ProjectType::JKDF2);
    const auto slotC = ui::recovery::recoveryFilePath(
        root, QStringLiteral("file:C:/levels/level.jkl"), QStringLiteral("session-a"),
        QStringLiteral("level.jkl"), ProjectType::MOTS);
    const auto slotOtherDirectory = ui::recovery::recoveryFilePath(
        root, QStringLiteral("file:C:/other/level.jkl"), QStringLiteral("session-a"),
        QStringLiteral("level.jkl"), ProjectType::JKDF2);
    const auto untitledA = ui::recovery::recoveryFilePath(
        root, QStringLiteral("untitled:one"), QStringLiteral("one"),
        QStringLiteral("Untitled"), ProjectType::JKDF2);
    const auto untitledB = ui::recovery::recoveryFilePath(
        root, QStringLiteral("untitled:two"), QStringLiteral("two"),
        QStringLiteral("Untitled"), ProjectType::JKDF2);
    check(slotA != slotB && slotA != slotC && slotA != slotOtherDirectory
            && untitledA != untitledB,
        "document_recovery keeps same basenames, directories, sessions, project types, and untitled sessions collision-safe");
    const QString slotAName = QFileInfo(QString::fromStdWString(
        slotA.filename().wstring())).fileName();
    const auto longDisplayNameSlot = ui::recovery::recoveryFilePath(
        root, QStringLiteral("file:C:/levels/very-long-level.jkl"),
        QStringLiteral("session-long"), QStringLiteral("part.one.")
            + QString(400, QLatin1Char('x')) + QStringLiteral(".jkl"),
        ProjectType::JKDF2);
    const QString longDisplayName = QFileInfo(QString::fromStdWString(
        longDisplayNameSlot.filename().wstring())).fileName();
    check(slotAName == QStringLiteral("level-jkdf2-recovery-zca5vzqulwd4cqjv4gta5nhf6e.sedrecovery")
            && slotAName.endsWith(QStringLiteral(".sedrecovery"))
            && slotAName.count(QLatin1Char('.')) == 1
            && QRegularExpression(
                QStringLiteral("^level-jkdf2-recovery-[a-z2-7]{26}\\.sedrecovery$"))
                .match(slotAName).hasMatch()
            && longDisplayName == QStringLiteral("part_one_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx-jkdf2-recovery-f4whdu74kqr7rernxy5ipsb74q.sedrecovery")
            && longDisplayName.count(QLatin1Char('.')) == 1,
        "document_recovery live filenames use one 128-bit family token, one extension, and a strict 128-character bound");

    // A successful publication belongs only to its session-qualified live
    // slot. The same source can be open in two SED processes; neither writer
    // may treat the other's complete checkpoint as stale merely because the
    // normalized document identity matches.
    const std::filesystem::path concurrentSource = root / "concurrent-level.jkl";
    const QString concurrentIdentity = ui::recovery::documentIdentity(
        concurrentSource, QString{});
    const auto concurrentSlotA = ui::recovery::recoveryFilePath(
        root, concurrentIdentity, QStringLiteral("concurrent-session-a"),
        QStringLiteral("concurrent-level.jkl"), ProjectType::JKDF2);
    const auto concurrentSlotB = ui::recovery::recoveryFilePath(
        root, concurrentIdentity, QStringLiteral("concurrent-session-b"),
        QStringLiteral("concurrent-level.jkl"), ProjectType::JKDF2);
    auto concurrentGateA = std::make_shared<ui::recovery::PublicationGate>();
    auto concurrentGateB = std::make_shared<ui::recovery::PublicationGate>();
    concurrentGateA->generation.store(1);
    concurrentGateB->generation.store(1);
    auto concurrentRequestA = makeRequest(
        concurrentSlotA, concurrentGateA, 1, ProjectType::JKDF2, 1, false);
    auto concurrentRequestB = makeRequest(
        concurrentSlotB, concurrentGateB, 1, ProjectType::JKDF2, 2, false);
    for (auto* request : { &concurrentRequestA, &concurrentRequestB })
    {
        request->metadata.identity = concurrentIdentity;
        request->metadata.sourcePath = QString::fromStdWString(
            concurrentSource.wstring());
        request->metadata.sourceModifiedAtMs = -1;
    }
    concurrentRequestA.metadata.sessionId =
        QStringLiteral("concurrent-session-a");
    concurrentRequestB.metadata.sessionId =
        QStringLiteral("concurrent-session-b");
    concurrentRequestB.metadata.requiresUserRecovery = false;
    const auto concurrentWriteA = ui::recovery::writeRecoverySnapshot(
        std::move(concurrentRequestA));
    const auto concurrentWriteB = ui::recovery::writeRecoverySnapshot(
        std::move(concurrentRequestB));
    const auto concurrentLive = ui::recovery::scanRecoveries(root);
    check(concurrentWriteA.status == ui::recovery::WriteStatus::published
            && concurrentWriteB.status == ui::recovery::WriteStatus::published
            && std::filesystem::is_regular_file(concurrentSlotA)
            && std::filesystem::is_regular_file(concurrentSlotB)
            && std::count_if(concurrentLive.begin(), concurrentLive.end(),
                [&concurrentIdentity](const ui::recovery::RecoveryDescriptor& value)
                {
                    return value.metadata.identity == concurrentIdentity;
                }) == 2,
        "document_recovery publication in one session never erases a concurrent live slot for the same source");
    check(std::filesystem::is_regular_file(concurrentSlotB)
            && std::any_of(concurrentLive.begin(), concurrentLive.end(),
                [&concurrentSlotB](const ui::recovery::RecoveryDescriptor& value)
                {
                    return value.recoveryPath == concurrentSlotB
                        && !value.metadata.requiresUserRecovery;
                }),
        "document_recovery discovery preserves another active session's clean hard-kill baseline");

    const std::filesystem::path path = root / "document.sedrecovery";
    auto gate = std::make_shared<ui::recovery::PublicationGate>();
    gate->generation.store(1);

    auto completeRequest = makeRequest(
        path, gate, 1, ProjectType::MOTS, 100, true);
    const QByteArray expectedLevel = canonicalNestedLevel(
        completeRequest.document.level());
    const auto write = ui::recovery::writeRecoverySnapshot(completeRequest);
    check(write.status == ui::recovery::WriteStatus::published,
        "document_recovery publishes a complete native V1 autosave");
    QByteArray bytes{};
    check(readBytes(path, &bytes)
            && bytes.startsWith(QByteArrayLiteral("sed_recovery_v1\n"))
            && bytes.contains(QByteArrayLiteral("sed-v1-document-recovery"))
            && bytes.contains(QByteArrayLiteral("sedchn1\n"))
            && bytes.contains(QByteArrayLiteral("sedfec1\n"))
            && bytes.contains(QByteArrayLiteral("fecend1\n"))
            && bytes.contains(QByteArrayLiteral("meta"))
            && bytes.contains(QByteArrayLiteral("docm")),
        "document_recovery uses lowercase V1 envelope, chunk, nested FEC, and payload identifiers");
    const auto offeredDescriptor = ui::recovery::readRecoveryDescriptor(path);
    check(offeredDescriptor && offeredDescriptor->metadata.requiresUserRecovery,
        "document_recovery persists the automatic-offer discriminator for authored work");

    const std::filesystem::path cleanRoot = root / "clean-baseline";
    const std::filesystem::path cleanPath = cleanRoot / "clean.sedrecovery";
    auto cleanGate = std::make_shared<ui::recovery::PublicationGate>();
    cleanGate->generation.store(1);
    auto cleanRequest = makeRequest(
        cleanPath, cleanGate, 1, ProjectType::JKDF2, 2, false);
    cleanRequest.metadata.requiresUserRecovery = false;
    const auto cleanWrite = ui::recovery::writeRecoverySnapshot(cleanRequest);
    cleanGate->generation.store(2);
    auto replaceCleanRequest = makeRequest(
        cleanPath, cleanGate, 2, ProjectType::JKDF2, 3, false);
    replaceCleanRequest.metadata.requiresUserRecovery = false;
    replaceCleanRequest.archivePreviousSnapshot = true;
    const auto replaceCleanWrite = ui::recovery::writeRecoverySnapshot(
        std::move(replaceCleanRequest));
    const auto cleanDescriptors = ui::recovery::scanRecoveries(cleanRoot);
    const auto cleanHistory = ui::recovery::scanAutosaveHistory(cleanRoot);
    check(cleanWrite.status == ui::recovery::WriteStatus::published
            && replaceCleanWrite.status == ui::recovery::WriteStatus::published
            && cleanDescriptors.size() == 1
            && cleanHistory.empty()
            && !cleanDescriptors.front().metadata.requiresUserRecovery,
        "document_recovery keeps a hard-kill clean baseline without misclassifying or archiving it as unsaved work");

    // Live and historical discovery each retain at most 256 descriptors.
    // Even 256 newer authored live slots must not consume the separate
    // history cap ahead of an older periodic autosave.
    const std::filesystem::path eligibilityRoot = root / "eligibility-priority";
    std::filesystem::create_directories(eligibilityRoot);
    QByteArray dirtyRecoveryBytes{};
    const bool floodSourceBytesRead = readBytes(path, &dirtyRecoveryBytes);
    bool liveFloodWritten = floodSourceBytesRead;
    for (int i = 0; liveFloodWritten && i < 256; ++i)
    {
        liveFloodWritten = writeBytes(
            eligibilityRoot / ("dirty-live-" + std::to_string(i)
                + ".sedrecovery"),
            dirtyRecoveryBytes);
    }
    const std::filesystem::path eligibleHistoryPath =
        eligibilityRoot / "eligible.autosave-1-test.sedautosave";
    const bool eligibleHistoryWritten = dirtyRecoveryBytes.size() > 0
        && writeBytes(eligibleHistoryPath, dirtyRecoveryBytes);
    const auto eligibilityScan = ui::recovery::scanAutosaveHistory(
        eligibilityRoot);
    check(liveFloodWritten && eligibleHistoryWritten
            && eligibilityScan.size() == 1
            && ui::recovery::recoveryPathsEqual(
                eligibilityScan.front().recoveryPath, eligibleHistoryPath)
            && eligibilityScan.front().metadata.requiresUserRecovery,
        "document_recovery scans periodic history independently of a full authored-live discovery budget");

    // A descriptor selected by a dialog/family scan is a fingerprint, not
    // permission to delete whatever another process later atomically installs
    // under the same canonical filename.
    const std::filesystem::path guardedDeleteRoot = root / "guarded-delete";
    const std::filesystem::path guardedDeletePath =
        guardedDeleteRoot / "guarded.sedrecovery";
    auto guardedDeleteGate = std::make_shared<ui::recovery::PublicationGate>();
    guardedDeleteGate->generation.store(1);
    auto guardedOldRequest = makeRequest(
        guardedDeletePath, guardedDeleteGate, 1, ProjectType::JKDF2, 1, false);
    const auto guardedOldWrite = ui::recovery::writeRecoverySnapshot(
        std::move(guardedOldRequest));
    const auto guardedOldDescriptor = ui::recovery::readRecoveryDescriptor(
        guardedDeletePath);
    guardedDeleteGate->generation.store(2);
    auto guardedNewRequest = makeRequest(
        guardedDeletePath, guardedDeleteGate, 2, ProjectType::JKDF2, 2, false);
    const auto guardedNewWrite = ui::recovery::writeRecoverySnapshot(
        std::move(guardedNewRequest));
    const auto guardedNewDescriptor = ui::recovery::readRecoveryDescriptor(
        guardedDeletePath);
    QString staleDeleteError{};
    const bool staleDelete = guardedOldDescriptor
        && ui::recovery::invalidateAndRemoveRecovery(
            nullptr, guardedDeletePath, &staleDeleteError,
            &guardedOldDescriptor->metadata);
    const auto guardedAfterStaleDelete = ui::recovery::readRecoveryDescriptor(
        guardedDeletePath);
    QString currentDeleteError{};
    const bool currentDelete = guardedNewDescriptor
        && ui::recovery::invalidateAndRemoveRecovery(
            nullptr, guardedDeletePath, &currentDeleteError,
            &guardedNewDescriptor->metadata);
    check(guardedOldWrite.status == ui::recovery::WriteStatus::published
            && guardedNewWrite.status == ui::recovery::WriteStatus::published
            && guardedOldDescriptor && guardedNewDescriptor
            && !staleDelete && !staleDeleteError.isEmpty()
            && guardedAfterStaleDelete
            && guardedAfterStaleDelete->metadata.documentSha256
                == guardedNewDescriptor->metadata.documentSha256
            && currentDelete && currentDeleteError.isEmpty()
            && !std::filesystem::exists(guardedDeletePath),
        "document_recovery compare-before-delete preserves a newer atomic replacement and removes only the authenticated selected snapshot");

    // Autosave History has the same compare-before-delete contract, but the
    // caller obtains its row from the bounded history scan and must opt into
    // the explicit-history removal reason.  Publish the replacement through
    // QSaveFile at the exact same path after the row was listed; this is the
    // real inter-process TOCTOU shape, with a fresh authenticated DOCM/META
    // fingerprint rather than a hand-edited descriptor.
    const std::filesystem::path historyToctouRoot = root / "history-toctou";
    const std::filesystem::path historyToctouPath =
        historyToctouRoot / "listed.sedautosave";
    auto historyToctouGate = std::make_shared<ui::recovery::PublicationGate>();
    historyToctouGate->generation.store(1);
    auto historyOldRequest = makeRequest(
        historyToctouPath, historyToctouGate, 1, ProjectType::MOTS, 1, false);
    const auto historyOldWrite = ui::recovery::writeRecoverySnapshot(
        std::move(historyOldRequest));
    const auto listedHistory = ui::recovery::scanAutosaveHistory(
        historyToctouRoot);
    historyToctouGate->generation.store(2);
    auto historyReplacementRequest = makeRequest(
        historyToctouPath, historyToctouGate, 2, ProjectType::MOTS, 2, false);
    const auto historyReplacementWrite = ui::recovery::writeRecoverySnapshot(
        std::move(historyReplacementRequest));
    const auto historyReplacementDescriptor = ui::recovery::readRecoveryDescriptor(
        historyToctouPath);
    QString historyStaleDeleteError{};
    const bool historyStaleDelete = listedHistory.size() == 1
        && ui::recovery::invalidateAndRemoveRecovery(
            nullptr, historyToctouPath, &historyStaleDeleteError,
            &listedHistory.front().metadata,
            ui::recovery::RecoveryRemovalReason::explicitHistoryDelete);
    const auto historyAfterStaleDelete = ui::recovery::readRecoveryDescriptor(
        historyToctouPath);
    QString historyCurrentDeleteError{};
    const bool historyCurrentDelete = historyReplacementDescriptor
        && ui::recovery::invalidateAndRemoveRecovery(
            nullptr, historyToctouPath, &historyCurrentDeleteError,
            &historyReplacementDescriptor->metadata,
            ui::recovery::RecoveryRemovalReason::explicitHistoryDelete);
    check(historyOldWrite.status == ui::recovery::WriteStatus::published
            && historyReplacementWrite.status == ui::recovery::WriteStatus::published
            && listedHistory.size() == 1 && historyReplacementDescriptor
            && listedHistory.front().metadata.documentSha256
                != historyReplacementDescriptor->metadata.documentSha256
            && !historyStaleDelete && !historyStaleDeleteError.isEmpty()
            && historyAfterStaleDelete
            && historyAfterStaleDelete->metadata.documentSha256
                == historyReplacementDescriptor->metadata.documentSha256
            && historyCurrentDelete && historyCurrentDeleteError.isEmpty()
            && !std::filesystem::exists(historyToctouPath),
        "document_recovery autosave-history explicit Delete preserves a valid same-path atomic replacement after a stale listed descriptor");

    const std::filesystem::path rotationRoot = root / "autosave-rotation-failures";
    const std::filesystem::path rotationPath = ui::recovery::recoveryFilePath(
        rotationRoot,
        QStringLiteral("untitled:document-recovery-test-session"),
        QStringLiteral("document-recovery-test-session"),
        QStringLiteral("Document Recovery Test"),
        ProjectType::MOTS);
    auto rotationGate = std::make_shared<ui::recovery::PublicationGate>();
    rotationGate->generation.store(1);
    auto rotationInitial = makeRequest(
        rotationPath, rotationGate, 1, ProjectType::MOTS, 1, false);
    const qint64 rotationInitialTimestamp = rotationInitial.metadata.timestamp;
    const auto rotationInitialWrite = ui::recovery::writeRecoverySnapshot(
        std::move(rotationInitial));
    QByteArray rotationInitialBytes{};
    const bool rotationInitialRead = readBytes(
        rotationPath, &rotationInitialBytes);

    rotationGate->generation.store(2);
    auto archiveFailureRequest = makeRequest(
        rotationPath, rotationGate, 2, ProjectType::MOTS, 2, false);
    archiveFailureRequest.archivePreviousSnapshot = true;
    archiveFailureRequest.maximumHistoricalSnapshots = 1;
    archiveFailureRequest.archivePublicationAllowedForTest = []() { return false; };
    const auto archiveFailure = ui::recovery::writeRecoverySnapshot(
        std::move(archiveFailureRequest));
    QByteArray afterArchiveFailureBytes{};
    const bool afterArchiveFailureRead = readBytes(
        rotationPath, &afterArchiveFailureBytes);
    const auto afterArchiveFailureHistory =
        ui::recovery::scanAutosaveHistory(rotationRoot);
    check(rotationInitialWrite.status == ui::recovery::WriteStatus::published
            && rotationInitialRead
            && archiveFailure.status == ui::recovery::WriteStatus::failed
            && afterArchiveFailureRead
            && afterArchiveFailureBytes == rotationInitialBytes
            && std::none_of(afterArchiveFailureHistory.begin(),
                afterArchiveFailureHistory.end(), [](const auto& descriptor)
                {
                    return descriptor.recoveryPath.extension() == ".sedautosave";
                }),
        "document_recovery archive-publication failure leaves the preceding live checkpoint byte-exact and unpruned");

    rotationGate->generation.store(3);
    auto liveFailureRequest = makeRequest(
        rotationPath, rotationGate, 3, ProjectType::MOTS, 3, false);
    liveFailureRequest.archivePreviousSnapshot = true;
    liveFailureRequest.maximumHistoricalSnapshots = 1;
    liveFailureRequest.liveCommitAllowedForTest = []() { return false; };
    const auto liveFailure = ui::recovery::writeRecoverySnapshot(
        std::move(liveFailureRequest));
    QByteArray afterLiveFailureBytes{};
    const bool afterLiveFailureRead = readBytes(
        rotationPath, &afterLiveFailureBytes);
    const auto afterLiveFailureHistory =
        ui::recovery::scanAutosaveHistory(rotationRoot);
    const std::size_t afterLiveFailureArchiveCount =
        static_cast<std::size_t>(std::count_if(
            afterLiveFailureHistory.begin(), afterLiveFailureHistory.end(),
            [](const auto& descriptor)
            {
                return descriptor.recoveryPath.extension() == ".sedautosave";
            }));
    const auto firstArchivePath = std::find_if(
        afterLiveFailureHistory.begin(), afterLiveFailureHistory.end(),
        [](const ui::recovery::RecoveryDescriptor& descriptor)
        {
            return descriptor.recoveryPath.extension() == ".sedautosave";
        });
    check(liveFailure.status == ui::recovery::WriteStatus::failed
            && afterLiveFailureRead
            && afterLiveFailureBytes == rotationInitialBytes
            && afterLiveFailureArchiveCount == 1,
        "document_recovery live-commit failure retains both the old live checkpoint and the pre-published archive duplicate");

    rotationGate->generation.store(4);
    auto repeatedLiveFailureRequest = makeRequest(
        rotationPath, rotationGate, 4, ProjectType::MOTS, 4, false);
    repeatedLiveFailureRequest.archivePreviousSnapshot = true;
    repeatedLiveFailureRequest.maximumHistoricalSnapshots = 1;
    repeatedLiveFailureRequest.liveCommitAllowedForTest = []() { return false; };
    const auto repeatedLiveFailure = ui::recovery::writeRecoverySnapshot(
        std::move(repeatedLiveFailureRequest));
    const auto afterRepeatedLiveFailureHistory =
        ui::recovery::scanAutosaveHistory(rotationRoot);
    const std::size_t afterRepeatedLiveFailureArchiveCount =
        static_cast<std::size_t>(std::count_if(
            afterRepeatedLiveFailureHistory.begin(),
            afterRepeatedLiveFailureHistory.end(),
            [](const auto& descriptor)
            {
                return descriptor.recoveryPath.extension() == ".sedautosave";
            }));
    const auto repeatedArchivePath = std::find_if(
        afterRepeatedLiveFailureHistory.begin(),
        afterRepeatedLiveFailureHistory.end(),
        [](const ui::recovery::RecoveryDescriptor& descriptor)
        {
            return descriptor.recoveryPath.extension() == ".sedautosave";
        });
    check(repeatedLiveFailure.status == ui::recovery::WriteStatus::failed
            && readBytes(rotationPath, &afterLiveFailureBytes)
            && afterLiveFailureBytes == rotationInitialBytes
            && afterRepeatedLiveFailureArchiveCount == 1
            && firstArchivePath != afterLiveFailureHistory.end()
            && repeatedArchivePath != afterRepeatedLiveFailureHistory.end()
            && firstArchivePath->recoveryPath
                == repeatedArchivePath->recoveryPath,
        "document_recovery repeated live-commit failure idempotently refreshes one archive instead of accumulating full duplicate snapshots");

    rotationGate->generation.store(5);
    auto rotationSuccessRequest = makeRequest(
        rotationPath, rotationGate, 5, ProjectType::MOTS, 5, false);
    rotationSuccessRequest.archivePreviousSnapshot = true;
    rotationSuccessRequest.maximumHistoricalSnapshots = 1;
    const auto rotationSuccess = ui::recovery::writeRecoverySnapshot(
        std::move(rotationSuccessRequest));
    QByteArray rotationSuccessBytes{};
    const auto rotationSuccessHistory =
        ui::recovery::scanAutosaveHistory(rotationRoot);
    const std::size_t rotationSuccessArchiveCount =
        static_cast<std::size_t>(std::count_if(
            rotationSuccessHistory.begin(), rotationSuccessHistory.end(),
            [](const auto& descriptor)
            {
                return descriptor.recoveryPath.extension() == ".sedautosave";
            }));
    check(rotationSuccess.status == ui::recovery::WriteStatus::published
            && readBytes(rotationPath, &rotationSuccessBytes)
            && rotationSuccessBytes != rotationInitialBytes
            && rotationSuccessArchiveCount == 1,
        "document_recovery successful three-phase rotation commits the new live checkpoint before pruning history to its bound");
    const auto rotationHistoryDescriptor = std::find_if(
        rotationSuccessHistory.begin(), rotationSuccessHistory.end(),
        [](const ui::recovery::RecoveryDescriptor& descriptor)
        {
            return descriptor.recoveryPath.extension() == ".sedautosave";
        });
    const QString rotationHistoryName = rotationHistoryDescriptor
        == rotationSuccessHistory.end()
        ? QString{}
        : QFileInfo(QString::fromStdWString(
            rotationHistoryDescriptor->recoveryPath.filename().wstring())).fileName();
    const QByteArray expectedRotationDigest = fixtureDocumentDigest(rotationInitialBytes);
    const QString expectedRotationName = QStringLiteral("document_recovery_test-mots-autosave-%1-z7rg4gx2uxswjdmm56unobeo5a-%2.sedautosave")
        .arg(QDateTime::fromMSecsSinceEpoch(rotationInitialTimestamp, Qt::UTC)
            .toString(QStringLiteral("yyyyMMdd'T'HHmmss'Z'")))
        .arg(fixedBase32(expectedRotationDigest.left(10)));
    check(expectedRotationDigest.size() == 32
            && exactRecoveryText(rotationHistoryName, expectedRotationName)
            && rotationHistoryName.endsWith(QStringLiteral(".sedautosave"))
            && rotationHistoryName.count(QLatin1Char('.')) == 1
            && QRegularExpression(QStringLiteral(
                    "^document_recovery_test-mots-autosave-"
                    "[0-9]{8}T[0-9]{6}Z-[a-z2-7]{26}-[a-z2-7]{16}"
                    "\\.sedautosave$"))
                .match(rotationHistoryName).hasMatch()
            && rotationHistoryName.size() <= 160,
        "document_recovery historical filenames use compact UTC time, one family token, one extension, and a strict 160-character bound");

    const std::filesystem::path retentionRoot = root / "autosave-retention";
    const std::filesystem::path retentionPath = ui::recovery::recoveryFilePath(
        retentionRoot,
        QStringLiteral("untitled:document-recovery-test-session"),
        QStringLiteral("document-recovery-test-session"),
        QStringLiteral("Document Recovery Test"),
        ProjectType::MOTS);
    auto retentionGate = std::make_shared<ui::recovery::PublicationGate>();
    bool retentionWritesOk = true;
    for (std::uint64_t generation = 1; generation <= 12; ++generation)
    {
        retentionGate->generation.store(generation);
        auto request = makeRequest(
            retentionPath, retentionGate, generation,
            ProjectType::MOTS, static_cast<int>(generation), false);
        request.metadata.requiresUserRecovery = true;
        request.archivePreviousSnapshot = generation > 1;
        request.maximumHistoricalSnapshots = 10;
        retentionWritesOk = retentionWritesOk
            && ui::recovery::writeRecoverySnapshot(std::move(request)).status
                == ui::recovery::WriteStatus::published;
    }
    const auto retainedHistory = ui::recovery::scanAutosaveHistory(retentionRoot);
    const auto retainedLive = ui::recovery::scanRecoveries(retentionRoot);
    const auto retainedFamily = retainedLive.empty()
        ? std::vector<ui::recovery::RecoveryDescriptor>{}
        : ui::recovery::scanRecoveryFamily(
            retentionRoot,
            retainedLive.front().metadata.identity,
            retainedLive.front().metadata.sessionId);
    const std::size_t historicalCount = static_cast<std::size_t>(std::count_if(
        retainedHistory.begin(), retainedHistory.end(),
        [](const ui::recovery::RecoveryDescriptor& descriptor)
        {
            return descriptor.recoveryPath.extension() == ".sedautosave";
        }));
    check(retentionWritesOk && historicalCount == 10
            && retainedHistory.size() == 10 && retainedLive.size() == 1
            && retainedFamily.size() == 11
            && std::all_of(retainedHistory.begin(), retainedHistory.end(),
                [](const ui::recovery::RecoveryDescriptor& descriptor)
                {
                    return QFileInfo(QString::fromStdWString(
                        descriptor.recoveryPath.wstring()))
                        .fileName().size() <= 255;
                })
            && std::all_of(retainedHistory.begin(), retainedHistory.end(),
                [](const ui::recovery::RecoveryDescriptor& descriptor)
                {
                    return descriptor.metadata.requiresUserRecovery;
                }),
        "document_recovery retains exactly ten full-DOCM historical autosaves while automatic scans see only the live slot");

    const std::filesystem::path oversizedMetadataPath =
        root / "oversized-meta.sedrecovery";
    auto oversizedMetadataGate = std::make_shared<ui::recovery::PublicationGate>();
    oversizedMetadataGate->generation.store(1);
    auto oversizedMetadataRequest = makeRequest(
        oversizedMetadataPath, oversizedMetadataGate, 1, ProjectType::MOTS, 2, false);
    oversizedMetadataRequest.metadata.displayName = QString(513, QChar('m'));
    const auto oversizedMetadataWrite = ui::recovery::writeRecoverySnapshot(
        std::move(oversizedMetadataRequest));
    check(oversizedMetadataWrite.status == ui::recovery::WriteStatus::failed
            && !std::filesystem::exists(oversizedMetadataPath),
        "document_recovery writer rejects a display name above its 512-character META field cap even below the JSON byte cap");

    const std::filesystem::path oversizedSessionDescriptorPath =
        root / "oversized-session-descriptor.sedrecovery";
    const std::filesystem::path oversizedDisplayDescriptorPath =
        root / "oversized-display-descriptor.sedrecovery";
    const QString oversizedSessionId(129, QChar('s'));
    const QByteArray oversizedSessionDescriptor = descriptorOnlyRecoveryBytes(
        QStringLiteral("reader-field-limit"),
        QDateTime::currentMSecsSinceEpoch(),
        [&oversizedSessionId](QJsonObject& metadata)
        {
            metadata.insert(QStringLiteral("sessionId"), oversizedSessionId);
            metadata.insert(QStringLiteral("identity"),
                QStringLiteral("untitled:%1").arg(oversizedSessionId));
        });
    const QByteArray oversizedDisplayDescriptor = descriptorOnlyRecoveryBytes(
        QStringLiteral("reader-display-limit"),
        QDateTime::currentMSecsSinceEpoch(),
        [](QJsonObject& metadata)
        {
            metadata.insert(
                QStringLiteral("displayName"), QString(513, QChar('d')));
        });
    QString oversizedSessionDescriptorError{};
    QString oversizedDisplayDescriptorError{};
    check(oversizedSessionDescriptor.size() < 1024 * 1024
            && oversizedDisplayDescriptor.size() < 1024 * 1024
            && writeBytes(
                oversizedSessionDescriptorPath, oversizedSessionDescriptor)
            && writeBytes(
                oversizedDisplayDescriptorPath, oversizedDisplayDescriptor)
            && !ui::recovery::readRecoveryDescriptor(
                oversizedSessionDescriptorPath,
                &oversizedSessionDescriptorError).has_value()
            && !ui::recovery::readRecoveryDescriptor(
                oversizedDisplayDescriptorPath,
                &oversizedDisplayDescriptorError).has_value(),
        "document_recovery descriptor reader rejects 129-character session and 513-character display fields before scan retention");

    const auto docm = findOuterChunk(bytes, QByteArrayLiteral("docm"));
    const auto meta = findOuterChunk(bytes, QByteArrayLiteral("meta"));
    check(findOuterChunks(bytes, QByteArrayLiteral("docm")).size() == 1
            && docm && meta && docm->flags == 4u && docm->storedSize == docm->rawSize
            && docm->crc == 0 && docm->sha256 == QByteArray(32, '\0'),
        "document_recovery stores exactly one native DOCM without duplicate outer CRC/SHA and a protected META");
    check(!bytes.contains(QByteArrayLiteral("LVL2")),
        "document_recovery has no abandoned LVL2 compatibility path");
    if (docm)
    {
        const QByteArray payload = bytes.mid(docm->payloadOffset,
            static_cast<qsizetype>(docm->storedSize));
        try
        {
            const assets::SedFile file = assets::loadSedFileBytes(std::as_bytes(std::span(
                payload.constData(), static_cast<std::size_t>(payload.size()))));
            check(file.optionalSections.size() == 2
                    && QByteArray(file.optionalSections[0].type.data(), 4) == QByteArrayLiteral("edit")
                    && QByteArray(file.optionalSections[1].type.data(), 4) == QByteArrayLiteral("hist"),
                "document_recovery DOCM contains required EDIT and HIST native sections");
        }
        catch (...)
        {
            check(false, "document_recovery DOCM is a strict native SED V1 file");
        }
    }

    QString error{};
    const auto record = ui::recovery::readRecoveryRecord(path, &error);
    const auto level = record ? ui::recovery::loadRecoveredLevel(*record, &error)
                              : std::optional<Level>{};
    check(record && level && record->decodedLevel
            && !record->corrupted && record->history
            && record->history->compact
            && record->documentTimestamp
                == completeRequest.metadata.timestamp
            && record->editorStatePayload == completeRequest.editorStatePayload
            && canonicalNestedLevel(*level) == expectedLevel
            && verifyRestoredHistory(completeRequest.document, *record->history)
            && level->kind == ProjectType::MOTS && level->sectors.size() == 100,
        "document_recovery round-trips the complete Level, EDIT payload, and Undo/Redo HIST state");

    const std::filesystem::path oversizedWarning =
        root / "oversized-compressed-warn.sedrecovery";
    QString oversizedWarningError{};
    const auto oversizedWarningRecord = writeBytes(oversizedWarning, bytes)
            && appendOversizedCompressedWarning(oversizedWarning)
        ? ui::recovery::readRecoveryRecord(oversizedWarning, &oversizedWarningError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    check(oversizedWarningRecord && oversizedWarningRecord->corrupted
            && oversizedWarningRecord->warnings.contains(QStringLiteral("Recovery chunk warn was skipped: type-specific stored/raw size limit is invalid")),
        "document_recovery rejects a CRC-valid compressed WARN with an oversized raw prefix before decompression");

    const std::filesystem::path duplicateWarnings =
        root / "duplicate-warning-chunks.sedrecovery";
    QString duplicateWarningsError{};
    const auto duplicateWarningsRecord = writeBytes(duplicateWarnings, bytes)
            && appendWarningChunk(duplicateWarnings,
                QStringLiteral("first canonical warning"))
            && appendWarningChunk(duplicateWarnings,
                QStringLiteral("second warning must not materialize"))
        ? ui::recovery::readRecoveryRecord(
            duplicateWarnings, &duplicateWarningsError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    const QStringList duplicateWarningText = duplicateWarningsRecord
        ? duplicateWarningsRecord->warnings : QStringList{};
    check(duplicateWarningsRecord && duplicateWarningsRecord->corrupted
            && duplicateWarningText.contains(
                QStringLiteral("first canonical warning"))
            && !duplicateWarningText.contains(
                QStringLiteral("second warning must not materialize"))
            && duplicateWarningText.contains(
                QStringLiteral("Recovery chunk warn was skipped: excess duplicate warning chunk")),
        "document_recovery materializes at most one canonical WARN chunk and reports excess duplicates without retaining their text");

    const std::filesystem::path duplicateDocm = root / "duplicate-identical-docm.sedrecovery";
    const bool duplicateAppended = writeBytes(duplicateDocm, bytes)
        && appendOuterChunkCopy(duplicateDocm, path, QByteArrayLiteral("docm"));
    QString duplicateError{};
    const auto duplicateRecord = duplicateAppended
        ? ui::recovery::readRecoveryRecord(duplicateDocm, &duplicateError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    check(duplicateAppended && duplicateRecord && duplicateRecord->corrupted
            && std::any_of(duplicateRecord->warnings.begin(), duplicateRecord->warnings.end(),
                [](const QString& warning)
                {
                    return warning == QStringLiteral("Duplicate valid DOCM matched the accepted bytes; the duplicate was ignored without another native decode.");
                }),
        "document_recovery accepts an identical duplicate DOCM only with an explicit corruption warning");

    const std::filesystem::path decoyBeforeValidDocm =
        root / "failed-decoy-before-valid-docm.sedrecovery";
    QString decoyBeforeValidError{};
    const auto decoyBeforeValidRecord = writeBytes(
            decoyBeforeValidDocm, bytes)
        && prependInvalidDocumentChunk(decoyBeforeValidDocm)
        ? ui::recovery::readRecoveryRecord(
            decoyBeforeValidDocm, &decoyBeforeValidError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    const auto decoyBeforeValidLevel = decoyBeforeValidRecord
        ? ui::recovery::loadRecoveredLevel(
            *decoyBeforeValidRecord, &decoyBeforeValidError)
        : std::optional<Level>{};
    check(decoyBeforeValidRecord
            && decoyBeforeValidRecord->metadata.projectType == ProjectType::MOTS
            && decoyBeforeValidLevel
            && decoyBeforeValidLevel->kind == ProjectType::MOTS
            && decoyBeforeValidRecord->history.has_value()
            && !decoyBeforeValidRecord->partialRecovery
            && decoyBeforeValidRecord->missingSections.isEmpty(),
        "document_recovery discards a failed DOCM decoy transaction before accepting the later valid document");

    const std::filesystem::path excessiveDocm =
        root / "excessive-duplicate-docm.sedrecovery";
    const bool excessiveDocmAppended = writeBytes(excessiveDocm, bytes)
        && appendOuterChunkCopy(excessiveDocm, path, QByteArrayLiteral("docm"))
        && appendOuterChunkCopy(excessiveDocm, path, QByteArrayLiteral("docm"));
    QString excessiveDocmError{};
    const auto excessiveDocmRecord = excessiveDocmAppended
        ? ui::recovery::readRecoveryRecord(excessiveDocm, &excessiveDocmError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    check(excessiveDocmRecord && excessiveDocmRecord->corrupted
            && excessiveDocmRecord->warnings.contains(QStringLiteral("Recovery chunk docm was skipped: excess duplicate native document chunk")),
        "document_recovery validates at most one redundant DOCM and skips further native decode amplification");

    const std::filesystem::path partialFirstDocm =
        root / "partial-first-then-strict-docm.sedrecovery";
    QString partialFirstError{};
    const bool partialFirstAppended = writeBytes(partialFirstDocm, bytes)
        && destroyNativeSection(partialFirstDocm, QByteArrayLiteral("hist"))
        && appendOuterChunkCopy(partialFirstDocm, path, QByteArrayLiteral("docm"));
    const auto partialFirstRecord = partialFirstAppended
        ? ui::recovery::readRecoveryRecord(partialFirstDocm, &partialFirstError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    check(partialFirstRecord && partialFirstRecord->corrupted
            && !partialFirstRecord->partialRecovery
            && partialFirstRecord->missingSections.isEmpty()
            && partialFirstRecord->lostSectionCount == 0
            && !partialFirstRecord->editorStatePayload.isEmpty()
            && partialFirstRecord->history.has_value(),
        "document_recovery lets a later strict DOCM supersede a partial-first duplicate without retaining stale loss state");

    const std::filesystem::path conflictingSource = root / "conflicting-source.sedrecovery";
    auto conflictingGate = std::make_shared<ui::recovery::PublicationGate>();
    conflictingGate->generation.store(1);
    const auto conflictingWrite = ui::recovery::writeRecoverySnapshot(
        makeRequest(conflictingSource, conflictingGate, 1, ProjectType::MOTS, 101, true));
    const std::filesystem::path conflictingDocm = root / "duplicate-conflicting-docm.sedrecovery";
    const bool conflictingAppended = conflictingWrite.status == ui::recovery::WriteStatus::published
        && writeBytes(conflictingDocm, bytes)
        && appendOuterChunkCopy(conflictingDocm, conflictingSource, QByteArrayLiteral("docm"));
    QString conflictingError{};
    const auto conflictingRecord = conflictingAppended
        ? ui::recovery::readRecoveryRecord(conflictingDocm, &conflictingError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    check(conflictingAppended && !conflictingRecord
            && exactRecoveryText(conflictingError, QStringLiteral("Conflicting valid DOCM payloads were found; recovery was rejected safely.")),
        "document_recovery rejects contradictory valid DOCM payloads instead of selecting one arbitrarily");

    const std::filesystem::path swappedDocm = root / "swapped-docm-binding.sedrecovery";
    writeBytes(swappedDocm, bytes);
    QString swappedError{};
    const auto swappedRecord = replaceOuterChunkCopy(
        swappedDocm, conflictingSource, QByteArrayLiteral("docm"))
        ? ui::recovery::readRecoveryRecord(swappedDocm, &swappedError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    check(!swappedRecord
            && exactRecoveryText(swappedError, QStringLiteral("Recovery metadata does not bind to the decoded native DOCM; source association was rejected safely.")),
        "document_recovery binds META to the exact native DOCM and rejects a same-project swapped payload");

    const std::filesystem::path poisonedMeta = root / "poisoned-primary-meta.sedrecovery";
    writeBytes(poisonedMeta, bytes);
    QString poisonedMetaError{};
    const auto poisonedMetaRecord = poisonPrimaryMetaTable(poisonedMeta)
        ? ui::recovery::readRecoveryRecord(poisonedMeta, &poisonedMetaError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    check(poisonedMetaRecord && poisonedMetaRecord->corrupted
            && poisonedMetaRecord->metadata.identity
                == QStringLiteral("untitled:document-recovery-test-session")
            && poisonedMetaRecord->repairedMetadataShardCount == 0,
        "document_recovery rejects a poisoned CRC-valid primary META table and uses its uniquely valid mirror");

    const std::filesystem::path multiUnitMeta = root / "multi-unit-primary-meta.sedrecovery";
    writeBytes(multiUnitMeta, bytes);
    QString multiUnitMetaError{};
    const auto multiUnitMetaRecord = makePrimaryMetaTableMultiUnit(multiUnitMeta)
        ? ui::recovery::readRecoveryRecord(multiUnitMeta, &multiUnitMetaError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    check(multiUnitMetaRecord && multiUnitMetaRecord->metadata.identity
            == QStringLiteral("untitled:document-recovery-test-session")
            && multiUnitMetaRecord->corrupted,
        "document_recovery ignores a CRC-valid schema-invalid multi-unit primary META and admits only the checked mirror");

    const std::filesystem::path crossBoundMeta =
        root / "cross-bound-meta-tables.sedrecovery";
    const bool crossBoundFixture = writeBytes(crossBoundMeta, bytes)
        && makeCrossBoundMetaTables(crossBoundMeta)
        && damagePrimaryRecoveryMetadata(crossBoundMeta);
    QString crossBoundError{};
    const auto crossBoundDescriptor = crossBoundFixture
        ? ui::recovery::readRecoveryDescriptor(crossBoundMeta, &crossBoundError)
        : std::optional<ui::recovery::RecoveryDescriptor>{};
    check(crossBoundFixture && !crossBoundDescriptor,
        "document_recovery requires a one-unit META table to independently decode the metadata it declares");

    const std::filesystem::path mirrorOnlyDamage = root / "damaged-meta-mirror-only.sedrecovery";
    writeBytes(mirrorOnlyDamage, bytes);
    QString mirrorOnlyError{};
    const auto mirrorOnlyRecord = damageMetaMirrorFooter(mirrorOnlyDamage)
        ? ui::recovery::readRecoveryRecord(mirrorOnlyDamage, &mirrorOnlyError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    check(mirrorOnlyRecord && mirrorOnlyRecord->corrupted
            && std::any_of(mirrorOnlyRecord->warnings.begin(), mirrorOnlyRecord->warnings.end(),
                [](const QString& warning)
                {
                    return warning == QStringLiteral("The primary META parity table was valid, but its checked mirror/footer was damaged.");
                }),
        "document_recovery reports a damaged META mirror/footer even when the primary table remains valid");

    const std::filesystem::path incoherentPath =
        root / "incoherent-association.sedrecovery";
    auto incoherentGate = std::make_shared<ui::recovery::PublicationGate>();
    incoherentGate->generation.store(1);
    auto incoherentRequest = makeRequest(
        incoherentPath, incoherentGate, 1, ProjectType::MOTS, 2, false);
    incoherentRequest.metadata.identity = QStringLiteral(
        "untitled:different-session");
    const auto incoherentWrite = ui::recovery::writeRecoverySnapshot(
        std::move(incoherentRequest));
    check(incoherentWrite.status == ui::recovery::WriteStatus::failed
            && !std::filesystem::exists(incoherentPath),
        "document_recovery writer rejects identity/source/session association mismatch before publication");

    const std::array<std::pair<QString, QJsonValue>, 10> metadataConflicts{
        std::pair{ QStringLiteral("identity"),
            QJsonValue(QStringLiteral("untitled:conflicting-session")) },
        std::pair{ QStringLiteral("sessionId"),
            QJsonValue(QStringLiteral("conflicting-session")) },
        std::pair{ QStringLiteral("sourcePath"),
            QJsonValue(QStringLiteral("C:/conflicting/source.jkl")) },
        std::pair{ QStringLiteral("displayName"),
            QJsonValue(QStringLiteral("Conflicting display name")) },
        std::pair{ QStringLiteral("timestamp"),
            QJsonValue(QStringLiteral("987654321")) },
        std::pair{ QStringLiteral("sourceModifiedAtMs"),
            QJsonValue(QStringLiteral("123456789")) },
        std::pair{ QStringLiteral("undoSignature"),
            QJsonValue(QStringLiteral("424242")) },
        std::pair{ QStringLiteral("projectType"),
            QJsonValue(static_cast<int>(ProjectType::IJIM)) },
        std::pair{ QStringLiteral("levelSha256"),
            QJsonValue(QString(64, QChar('a'))) },
        std::pair{ QStringLiteral("documentSha256"),
            QJsonValue(QString(64, QChar('b'))) },
    };
    bool everyMetadataConflictRejected = true;
    for (qsizetype index = 0;
         index < static_cast<qsizetype>(metadataConflicts.size());
         ++index)
    {
        const auto& [field, value] = metadataConflicts[static_cast<std::size_t>(index)];
        const std::filesystem::path conflictPath = root
            / QStringLiteral("metadata-conflict-%1.sedrecovery")
                .arg(index).toStdString();
        const bool fixtureWritten = writeBytes(conflictPath, bytes)
            && rewritePrimaryRecoveryMetadata(
                conflictPath,
                [&field, &value](QJsonObject& object)
                {
                    object.insert(field, value);
                });
        QString conflictError{};
        const auto descriptor = fixtureWritten
            ? ui::recovery::readRecoveryDescriptor(conflictPath, &conflictError)
            : std::optional<ui::recovery::RecoveryDescriptor>{};
        everyMetadataConflictRejected = everyMetadataConflictRejected
            && fixtureWritten && !descriptor
            && exactRecoveryText(conflictError, QStringLiteral("The valid primary and protected recovery metadata copies disagree; source association was rejected safely."));
    }
    check(everyMetadataConflictRejected,
        "document_recovery rejects valid primary/META disagreement in every source-association and recency field");

    const std::array<std::pair<
        QString, std::function<void(QJsonObject&)>>, 4> requiredMetadataStringCases{{
        { QStringLiteral("missing-source-path"),
            [](QJsonObject& object)
            {
                object.remove(QStringLiteral("sourcePath"));
            } },
        { QStringLiteral("wrong-source-path-type"),
            [](QJsonObject& object)
            {
                object.insert(QStringLiteral("sourcePath"), false);
            } },
        { QStringLiteral("missing-display-name"),
            [](QJsonObject& object)
            {
                object.remove(QStringLiteral("displayName"));
            } },
        { QStringLiteral("wrong-display-name-type"),
            [](QJsonObject& object)
            {
                object.insert(QStringLiteral("displayName"), 7);
            } },
    }};
    bool everyRequiredMetadataStringRejected = true;
    for (const auto& [suffix, mutate] : requiredMetadataStringCases)
    {
        const std::filesystem::path malformedPath = root
            / ("metadata-required-string-" + suffix.toStdString()
                + ".sedrecovery");
        const bool fixtureWritten = writeBytes(malformedPath, bytes)
            && rewriteRecoveryMetadataCopies(malformedPath, mutate);
        QString descriptorError{};
        const auto descriptor = fixtureWritten
            ? ui::recovery::readRecoveryDescriptor(
                malformedPath, &descriptorError)
            : std::optional<ui::recovery::RecoveryDescriptor>{};
        QString recordError{};
        const auto record = fixtureWritten
            ? ui::recovery::readRecoveryRecord(malformedPath, &recordError)
            : std::optional<ui::recovery::RecoveryRecord>{};
        everyRequiredMetadataStringRejected = everyRequiredMetadataStringRejected
            && fixtureWritten && !descriptor
            && record && record->corrupted
            && record->metadata.identity.startsWith(QStringLiteral("orphan:"))
            && record->warnings.contains(QStringLiteral(
                "All recovery metadata copies were unreadable. SED salvaged the checked Level as an Untitled session; source path and capture metadata were unavailable."));
    }
    check(everyRequiredMetadataStringRejected,
        "document_recovery rejects missing or non-string sourcePath/displayName in matching primary and protected META copies");

    const std::filesystem::path emptyUntitledStringsPath =
        root / "valid-empty-untitled-metadata-strings.sedrecovery";
    auto emptyUntitledStringsGate =
        std::make_shared<ui::recovery::PublicationGate>();
    emptyUntitledStringsGate->generation.store(1);
    auto emptyUntitledStringsRequest = makeRequest(
        emptyUntitledStringsPath, emptyUntitledStringsGate, 1,
        ProjectType::MOTS, 1, false);
    emptyUntitledStringsRequest.metadata.displayName.clear();
    const auto emptyUntitledStringsWrite = ui::recovery::writeRecoverySnapshot(
        emptyUntitledStringsRequest);
    QString emptyUntitledStringsError{};
    const auto emptyUntitledStringsRecord =
        emptyUntitledStringsWrite.status == ui::recovery::WriteStatus::published
        ? ui::recovery::readRecoveryRecord(
            emptyUntitledStringsPath, &emptyUntitledStringsError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    const auto emptyUntitledStringsDescriptor =
        ui::recovery::readRecoveryDescriptor(emptyUntitledStringsPath);
    check(emptyUntitledStringsWrite.status == ui::recovery::WriteStatus::published
            && emptyUntitledStringsRecord && !emptyUntitledStringsRecord->corrupted
            && emptyUntitledStringsRecord->metadata.identity
                == QStringLiteral("untitled:document-recovery-test-session")
            && emptyUntitledStringsRecord->metadata.sourcePath.isEmpty()
            && emptyUntitledStringsRecord->metadata.displayName
                == QStringLiteral("Untitled")
            && emptyUntitledStringsDescriptor
            && emptyUntitledStringsError.isEmpty(),
        "document_recovery accepts present empty sourcePath/displayName strings for an untitled session");

    const std::filesystem::path invalidSourceTimestampPath =
        root / "invalid-source-timestamp.sedrecovery";
    QString invalidSourceTimestampError{};
    const bool invalidSourceTimestampWritten =
        writeBytes(invalidSourceTimestampPath, bytes)
        && rewritePrimaryRecoveryMetadata(
            invalidSourceTimestampPath,
            [](QJsonObject& object)
            {
                object.insert(
                    QStringLiteral("sourceModifiedAtMs"),
                    QStringLiteral("-2"));
            })
        // Remove the valid protected metadata fallback so this fixture
        // exercises acceptance of the malformed primary object itself.
        && damageMetaShards(invalidSourceTimestampPath, 3);
    const auto invalidSourceTimestampDescriptor =
        invalidSourceTimestampWritten
        ? ui::recovery::readRecoveryDescriptor(
            invalidSourceTimestampPath, &invalidSourceTimestampError)
        : std::optional<ui::recovery::RecoveryDescriptor>{};
    check(invalidSourceTimestampWritten && !invalidSourceTimestampDescriptor,
        "document_recovery rejects source modification timestamps below the sole -1 unavailable sentinel");

    const std::filesystem::path legacyCaptureFieldPath =
        root / "legacy-capture-field.sedrecovery";
    QString legacyCaptureFieldError{};
    const bool legacyCaptureFieldWritten =
        writeBytes(legacyCaptureFieldPath, bytes)
        && rewritePrimaryRecoveryMetadata(
            legacyCaptureFieldPath,
            [](QJsonObject& object)
            {
                const QJsonValue timestamp = object.take(
                    QStringLiteral("timestamp"));
                object.insert(QStringLiteral("capturedAtMs"), timestamp);
            })
        && damageMetaShards(legacyCaptureFieldPath, 3);
    const auto legacyCaptureFieldDescriptor = legacyCaptureFieldWritten
        ? ui::recovery::readRecoveryDescriptor(
            legacyCaptureFieldPath, &legacyCaptureFieldError)
        : std::optional<ui::recovery::RecoveryDescriptor>{};
    check(legacyCaptureFieldWritten && !legacyCaptureFieldDescriptor,
        "document_recovery V1 requires timestamp and rejects the unreleased capturedAtMs field");

    const std::filesystem::path primaryDamaged =
        root / "descriptor-primary-damaged.sedrecovery";
    QString descriptorError{};
    check(writeBytes(primaryDamaged, bytes)
            && damagePrimaryRecoveryMetadata(primaryDamaged)
            && ui::recovery::readRecoveryDescriptor(
                primaryDamaged, &descriptorError).has_value(),
        "document_recovery metadata-only discovery uses protected META when the primary copy is damaged");

    const std::filesystem::path footerOnlyMeta =
        root / "footer-only-headerless-meta.sedrecovery";
    QString footerOnlyMetaError{};
    const auto footerOnlyMetaRecord = writeBytes(footerOnlyMeta, bytes)
        && damageMetaHeaderAndPrimaryPrefix(footerOnlyMeta)
        ? ui::recovery::readRecoveryRecord(footerOnlyMeta, &footerOnlyMetaError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    check(footerOnlyMetaRecord && footerOnlyMetaRecord->metadata.identity
            == QStringLiteral("untitled:document-recovery-test-session")
            && footerOnlyMetaRecord->corrupted,
        "document_recovery recovers headerless META from its exact checked footer/mirror boundary");

    const std::filesystem::path nonCanonicalPrimaryHash =
        root / "descriptor-uppercase-primary-hash.sedrecovery";
    descriptorError.clear();
    const bool hashUppercased = writeBytes(nonCanonicalPrimaryHash, bytes)
        && uppercasePrimaryRecoveryHash(nonCanonicalPrimaryHash);
    const auto nonCanonicalDescriptor = hashUppercased
        ? ui::recovery::readRecoveryDescriptor(
            nonCanonicalPrimaryHash, &descriptorError)
        : std::optional<ui::recovery::RecoveryDescriptor>{};
    const auto nonCanonicalRecord = hashUppercased
        ? ui::recovery::readRecoveryRecord(
            nonCanonicalPrimaryHash, &descriptorError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    check(nonCanonicalDescriptor && nonCanonicalRecord
            && nonCanonicalRecord->corrupted,
        "document_recovery rejects non-canonical uppercase primary digest text and recovers through protected META");

    const std::filesystem::path redundantDamaged =
        root / "descriptor-meta-damaged.sedrecovery";
    descriptorError.clear();
    check(writeBytes(redundantDamaged, bytes)
            && damageMetaShards(redundantDamaged, 3)
            && ui::recovery::readRecoveryDescriptor(
                redundantDamaged, &descriptorError).has_value(),
        "document_recovery metadata-only discovery uses the checked primary when META exceeds parity");

    const std::filesystem::path oversizedDocument =
        root / "descriptor-oversized-docm.sedrecovery";
    descriptorError.clear();
    check(writeBytes(oversizedDocument, bytes)
            && declareOversizedOuterDocument(oversizedDocument)
            && ui::recovery::readRecoveryDescriptor(
                oversizedDocument, &descriptorError).has_value(),
        "document_recovery descriptor discovery never materializes or decodes an oversized DOCM");

    const std::filesystem::path scanRoot = root / "metadata-scan";
    std::error_code scanDirectoryError{};
    std::filesystem::create_directory(scanRoot, scanDirectoryError);
    const std::filesystem::path scanValid = scanRoot / "valid.sedrecovery";
    const std::filesystem::path scanHuge = scanRoot / "huge-docm.sedrecovery";
    bool scanFixturesWritten = !scanDirectoryError
        && writeBytes(scanValid, bytes)
        && writeBytes(scanHuge, bytes)
        && declareOversizedOuterDocument(scanHuge);
    for (int index = 0; index < 128 && scanFixturesWritten; ++index)
    {
        QByteArray malformed = QByteArrayLiteral("sed_recovery_v1\n")
            + QByteArray::number(1024 * 1024) + QByteArrayLiteral("\n")
            + QByteArray(64, 'f') + QByteArrayLiteral("\n")
            + QByteArrayLiteral("truncated");
        scanFixturesWritten = writeBytes(
            scanRoot / QStringLiteral("malformed-%1.sedrecovery")
                .arg(index).toStdString(),
            malformed);
    }
    const auto scannedDescriptors = scanFixturesWritten
        ? ui::recovery::scanRecoveries(scanRoot)
        : std::vector<ui::recovery::RecoveryDescriptor>{};
    check(scanFixturesWritten && scannedDescriptors.size() == 2
            && std::any_of(scannedDescriptors.begin(), scannedDescriptors.end(),
                [&scanValid](const ui::recovery::RecoveryDescriptor& descriptor)
                {
                    return descriptor.recoveryPath == scanValid;
                })
            && std::any_of(scannedDescriptors.begin(), scannedDescriptors.end(),
                [&scanHuge](const ui::recovery::RecoveryDescriptor& descriptor)
                {
                    return descriptor.recoveryPath == scanHuge;
                }),
        "document_recovery isolates many malformed files and huge DOCM claims during bounded metadata-only scans");

    const std::filesystem::path futureDecoyRoot =
        root / "metadata-scan-future-decoys";
    std::error_code futureDecoyDirectoryError{};
    std::filesystem::create_directory(
        futureDecoyRoot, futureDecoyDirectoryError);
    const std::filesystem::path olderValidRecovery =
        futureDecoyRoot / "older-valid.sedrecovery";
    const qint64 decoyNow = QDateTime::currentMSecsSinceEpoch();
    const std::filesystem::path futureDecoyBase =
        root / "future-conflicting-meta-base.sedrecovery";
    auto futureDecoyGate = std::make_shared<ui::recovery::PublicationGate>();
    futureDecoyGate->generation.store(1);
    auto futureDecoyRequest = makeRequest(
        futureDecoyBase, futureDecoyGate, 1, ProjectType::MOTS, 3, false);
    futureDecoyRequest.metadata.timestamp = decoyNow + 120'000;
    const auto futureDecoyWrite = ui::recovery::writeRecoverySnapshot(
        std::move(futureDecoyRequest));
    QByteArray futureDecoyBytes{}, conflictingRecoveryBytes{};
    bool futureDecoyFixturesWritten = !futureDecoyDirectoryError
        && futureDecoyWrite.status == ui::recovery::WriteStatus::published
        && readBytes(futureDecoyBase, &futureDecoyBytes)
        && readBytes(conflictingSource, &conflictingRecoveryBytes)
        && writeBytes(olderValidRecovery, descriptorOnlyRecoveryBytes(
            QStringLiteral("older-valid"), decoyNow - 120'000))
        && setFileModificationTime(olderValidRecovery, decoyNow - 120'000);
    const auto conflictingMeta = findOuterChunk(
        conflictingRecoveryBytes, QByteArrayLiteral("meta"));
    if (futureDecoyFixturesWritten && conflictingMeta)
    {
        const qsizetype conflictingMetaHeader =
            conflictingMeta->payloadOffset - kOuterChunkHeaderBytes;
        const qsizetype conflictingMetaSize = kOuterChunkHeaderBytes
            + static_cast<qsizetype>(conflictingMeta->storedSize);
        futureDecoyBytes.append(conflictingRecoveryBytes.mid(
            conflictingMetaHeader, conflictingMetaSize));
    }
    else
    {
        futureDecoyFixturesWritten = false;
    }
    for (int index = 0; index < 256 && futureDecoyFixturesWritten; ++index)
    {
        const std::filesystem::path decoy = futureDecoyRoot
            / QStringLiteral("future-conflicting-meta-%1.sedrecovery")
                .arg(index, 4, 10, QLatin1Char('0')).toStdString();
        futureDecoyFixturesWritten = writeBytes(
                decoy, futureDecoyBytes)
            && setFileModificationTime(decoy, decoyNow + 120'000 + index);
    }
    const auto futureDecoyDescriptors = futureDecoyFixturesWritten
        ? ui::recovery::scanRecoveries(futureDecoyRoot)
        : std::vector<ui::recovery::RecoveryDescriptor>{};
    check(futureDecoyFixturesWritten && futureDecoyDescriptors.size() == 1
            && futureDecoyDescriptors.front().recoveryPath
                == olderValidRecovery,
        "document_recovery scans past validated DOCM extents and rejects 256 future-dated conflicting-META decoys before they can hide an older valid recovery");

    constexpr int kExpectedRecoveryScanCandidateLimit = 256;
    constexpr int kOlderInvalidDecoyCount = 16;
    const std::filesystem::path floodRoot = root / "metadata-scan-flood";
    std::error_code floodDirectoryError{};
    std::filesystem::create_directory(floodRoot, floodDirectoryError);
    const qint64 floodModifiedAtMs = QDateTime::currentMSecsSinceEpoch()
        - 60 * 60 * 1000;
    const qint64 floodTimestamp = floodModifiedAtMs;
    const std::filesystem::path floodNewest =
        floodRoot / "newest-valid.sedrecovery";
    bool floodFixturesWritten = !floodDirectoryError
        && writeBytes(floodNewest, descriptorOnlyRecoveryBytes(
            QStringLiteral("directory-flood-dirty"),
            floodTimestamp - 60'000))
        && setFileModificationTime(floodNewest, floodModifiedAtMs + 60'000);
    const QByteArray tiedValidDescriptor = descriptorOnlyRecoveryBytes(
        QStringLiteral("directory-flood-tied-valid"), floodTimestamp,
        [](QJsonObject& metadata)
        {
            metadata.insert(QStringLiteral("requiresUserRecovery"), false);
        });
    for (int index = 0;
         index < kExpectedRecoveryScanCandidateLimit && floodFixturesWritten;
         ++index)
    {
        const std::filesystem::path candidate = floodRoot
            / QStringLiteral("bulk-valid-%1.sedrecovery")
                .arg(index, 4, 10, QLatin1Char('0')).toStdString();
        floodFixturesWritten = writeBytes(candidate, tiedValidDescriptor)
            && setFileModificationTime(candidate, floodModifiedAtMs);
    }
    for (int index = 0;
         index < kOlderInvalidDecoyCount && floodFixturesWritten;
         ++index)
    {
        const std::filesystem::path decoy = floodRoot
            / QStringLiteral("older-invalid-%1.sedrecovery")
                .arg(index, 4, 10, QLatin1Char('0')).toStdString();
        floodFixturesWritten = writeBytes(
                decoy, QByteArrayLiteral("invalid recovery decoy"))
            && setFileModificationTime(decoy, floodModifiedAtMs - 60'000);
    }
    const auto floodedDescriptors = floodFixturesWritten
        ? ui::recovery::scanRecoveries(floodRoot)
        : std::vector<ui::recovery::RecoveryDescriptor>{};
    bool deterministicFloodSelection = floodedDescriptors.size()
            == kExpectedRecoveryScanCandidateLimit
        && floodedDescriptors.front().recoveryPath == floodNewest;
    for (int index = 0;
         index < kExpectedRecoveryScanCandidateLimit - 1
            && deterministicFloodSelection;
         ++index)
    {
        const std::filesystem::path expected = floodRoot
            / QStringLiteral("bulk-valid-%1.sedrecovery")
                .arg(index, 4, 10, QLatin1Char('0')).toStdString();
        deterministicFloodSelection = floodedDescriptors[
            static_cast<std::size_t>(index + 1)].recoveryPath == expected;
    }
    const std::filesystem::path firstExcludedFloodCandidate = floodRoot
        / QStringLiteral("bulk-valid-%1.sedrecovery")
            .arg(kExpectedRecoveryScanCandidateLimit - 1,
                4, 10, QLatin1Char('0')).toStdString();
    check(floodFixturesWritten && deterministicFloodSelection
            && std::none_of(floodedDescriptors.begin(), floodedDescriptors.end(),
                [&firstExcludedFloodCandidate](
                    const ui::recovery::RecoveryDescriptor& descriptor)
                {
                    return descriptor.recoveryPath
                        == firstExcludedFloodCandidate;
                }),
        "document_recovery caps a flooded directory at 256 valid descriptors, prioritizes older unsaved work over clean session baselines, and keeps deterministic identity/path ties");
    check(ui::recovery::detail::maximumRecoveryDirectoryEntriesScannedForTesting()
            == 4096,
        "document_recovery caps directory enumeration work independently of its 256 retained metadata candidates");

    const std::filesystem::path damagedMeta = root / "damaged-meta.sedrecovery";
    writeBytes(damagedMeta, bytes);
    check(damageMetaShards(damagedMeta, 2),
        "document_recovery damages two outer META shards");
    const auto repairedMeta = ui::recovery::readRecoveryRecord(damagedMeta, &error);
    check(repairedMeta && repairedMeta->metadata.identity
            == QStringLiteral("untitled:document-recovery-test-session")
            && repairedMeta->repairedMetadataShardCount >= 2,
        "document_recovery repairs META through its outer parity protection");

    const std::filesystem::path metadataLostSingleDecode =
        root / "metadata-lost-single-docm-decode.sedrecovery";
    std::uint64_t nativeDocumentDecodeCount = 0;
    QString metadataLostSingleDecodeError{};
    const bool metadataLostFixture = writeBytes(
            metadataLostSingleDecode, bytes)
        && damagePrimaryRecoveryMetadata(metadataLostSingleDecode)
        && damageMetaShards(metadataLostSingleDecode, 3);
    const auto metadataLostRecord = metadataLostFixture
        ? ui::recovery::detail::readRecoveryRecordWithNativeDecodeCountForTesting(
            metadataLostSingleDecode,
            &nativeDocumentDecodeCount,
            &metadataLostSingleDecodeError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    std::uint64_t nativeReadAndInstallDecodeCount = 0;
    QString nativeReadAndInstallError{};
    const auto metadataLostInstalled = metadataLostFixture
        ? ui::recovery::detail::
            readAndLoadRecoveryDocumentWithNativeDecodeCountForTesting(
                metadataLostSingleDecode,
                &nativeReadAndInstallDecodeCount,
                &nativeReadAndInstallError)
        : std::optional<ui::recovery::RecoveredDocument>{};
    check(metadataLostRecord
            && exactRecoveryText(metadataLostRecord->metadata.identity,
                QStringLiteral("orphan:") + QString::fromLatin1(fixtureDocumentDigest(bytes).toHex()))
            && metadataLostRecord->metadata.projectType == ProjectType::MOTS
            && metadataLostRecord->metadata.projectTypeTrust
                == ui::recovery::ProjectTypeTrust::native
            && metadataLostRecord->history.has_value()
            && metadataLostRecord->history->compact
            && metadataLostRecord->historyPayload.isEmpty()
            && metadataLostRecord->decodedLevel
            && nativeDocumentDecodeCount == 1,
        "document_recovery reuses one validated DOCM decode for metadata-loss synthesis and deferred HIST instead of decoding the native document repeatedly");
    check(metadataLostInstalled
            && metadataLostInstalled->history
            && metadataLostInstalled->history->compact
            && metadataLostInstalled->history->undoEntryCount() == 1
            && nativeReadAndInstallDecodeCount == 1
            && nativeReadAndInstallError.isEmpty(),
        "document_recovery moves the already validated authoritative Level and compact HIST into the live recovery document without a second native DOCM decode");

    gate->generation.store(2);
    const auto superseded = ui::recovery::writeRecoverySnapshot(
        makeRequest(root / "superseded.sedrecovery", gate, 1));
    check(superseded.status == ui::recovery::WriteStatus::superseded,
        "document_recovery prevents an obsolete generation from publishing");
    ui::recovery::RecoveryMetadata recency{};
    recency.sourcePath = QString::fromStdWString((root / "source.jkl").wstring());
    recency.timestamp = QDateTime::currentMSecsSinceEpoch();
    recency.sourceModifiedAtMs = 0;
    check(ui::recovery::isRecoveryNewerThanSource(recency)
            && ui::recovery::recoverySourceChanged(recency),
        "document_recovery reports unsaved snapshots whose source changed");

    gate->generation.store(1);
    auto noEditor = makeRequest(root / "missing-edit.sedrecovery", gate, 1);
    noEditor.editorStatePayload.clear();
    check(ui::recovery::writeRecoverySnapshot(std::move(noEditor)).status
            == ui::recovery::WriteStatus::failed,
        "document_recovery refuses an autosave without its required EDIT section");

    const std::filesystem::path omittedHistoryWithDamagedEditor =
        root / "omitted-history-damaged-editor.sedrecovery";
    auto omittedHistoryRequest = makeRequest(
        omittedHistoryWithDamagedEditor, gate, 1, ProjectType::MOTS, 2, false);
    omittedHistoryRequest.document.selection().things.insert(99'999);
    omittedHistoryRequest.document.pushUndo("invalid history fixture");
    const auto omittedHistoryWrite = ui::recovery::writeRecoverySnapshot(
        std::move(omittedHistoryRequest));
    QString omittedHistoryError{};
    const auto omittedHistoryRecord = omittedHistoryWrite.status
            == ui::recovery::WriteStatus::published
        && !omittedHistoryWrite.warning.isEmpty()
        && destroyNativeSection(
            omittedHistoryWithDamagedEditor, QByteArrayLiteral("edit"))
        ? ui::recovery::readRecoveryRecord(
            omittedHistoryWithDamagedEditor, &omittedHistoryError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    check(omittedHistoryRecord && omittedHistoryRecord->partialRecovery
            && omittedHistoryRecord->missingSections.contains(QStringLiteral("edit"))
            && !omittedHistoryRecord->missingSections.contains(QStringLiteral("hist")),
        "document_recovery does not infer physical HIST loss when an intentional omission accompanies unrelated EDIT damage");

    const std::filesystem::path damaged = root / "damaged-required.sedrecovery";
    writeBytes(damaged, bytes);
    check(destroyNativeSection(damaged, QByteArrayLiteral("sect")),
        "document_recovery destroys one native required section beyond parity");
    auto damagedRecord = ui::recovery::readRecoveryRecord(damaged, &error);
    check(damagedRecord && damagedRecord->partialRecovery
            && damagedRecord->lostSectionCount > 0
            && damagedRecord->missingSections.contains(QStringLiteral("sect"))
            && ui::recovery::loadRecoveredLevel(*damagedRecord, &error).has_value(),
        "document_recovery safely salvages healthy Level sections after native repair loss");
    const QByteArray damagedRequiredPayload = damagedRecord
        ? damagedRecord->levelPayload : QByteArray{};
    const auto damagedInstalled = damagedRecord
        ? ui::recovery::loadRecoveredDocument(std::move(*damagedRecord), &error)
        : std::optional<ui::recovery::RecoveredDocument>{};
    check(damagedInstalled && damagedRecord
            && !damagedRequiredPayload.isEmpty()
            && damagedRecord->levelPayload == damagedRequiredPayload,
        "document_recovery consuming install retains damaged required DOCM bytes for last-resort text salvage");

    const std::filesystem::path damagedOptional = root / "damaged-optional.sedrecovery";
    writeBytes(damagedOptional, bytes);
    check(destroyNativeSection(damagedOptional, QByteArrayLiteral("edit")),
        "document_recovery destroys optional EDIT beyond native parity");
    const auto optionalRecord = ui::recovery::readRecoveryRecord(damagedOptional, &error);
    check(optionalRecord && optionalRecord->partialRecovery
            && optionalRecord->editorStatePayload.isEmpty()
            && optionalRecord->missingSections.contains(QStringLiteral("edit"))
            && ui::recovery::loadRecoveredLevel(*optionalRecord, &error).has_value(),
        "document_recovery preserves Level while reporting lost optional state");

    const std::filesystem::path damagedHistory = root / "damaged-history.sedrecovery";
    writeBytes(damagedHistory, bytes);
    check(destroyNativeSection(damagedHistory, QByteArrayLiteral("hist")),
        "document_recovery destroys optional HIST beyond native parity");
    const auto historyRecord = ui::recovery::readRecoveryRecord(damagedHistory, &error);
    check(historyRecord && historyRecord->partialRecovery
            && !historyRecord->history
            && historyRecord->missingSections.contains(QStringLiteral("hist"))
            && ui::recovery::loadRecoveredLevel(*historyRecord, &error).has_value(),
        "document_recovery loses lower-priority history without losing the authoritative Level");

    const std::filesystem::path damagedHistoryWithWarn =
        root / "damaged-history-with-forged-warn.sedrecovery";
    writeBytes(damagedHistoryWithWarn, bytes);
    const auto historyWarnRecord = destroyNativeSection(
        damagedHistoryWithWarn, QByteArrayLiteral("hist"))
        && appendWarningChunk(
            damagedHistoryWithWarn,
            QStringLiteral("Recovery published Level+EDIT; HIST was omitted: forged"))
        ? ui::recovery::readRecoveryRecord(damagedHistoryWithWarn, &error)
        : std::optional<ui::recovery::RecoveryRecord>{};
    check(historyWarnRecord && historyWarnRecord->partialRecovery
            && historyWarnRecord->missingSections.contains(QStringLiteral("hist")),
        "document_recovery does not let an untrusted HIST omission warning conceal physically lost history");

    const std::filesystem::path damagedHeader = root / "damaged-docm-header.sedrecovery";
    writeBytes(damagedHeader, bytes);
    check(damageOuterDocmHeader(damagedHeader),
        "document_recovery damages the outer DOCM header fixture");
    const auto scanned = ui::recovery::readRecoveryRecord(damagedHeader, &error);
    check(scanned && scanned->corrupted && ui::recovery::loadRecoveredLevel(*scanned, &error),
        "document_recovery scans and semantically validates native SED when the DOCM envelope header is damaged");

    const std::filesystem::path markerFlood = root / "marker-flood.sedrecovery";
    writeBytes(markerFlood, bytes);
    check(destroyNativeSection(markerFlood, QByteArrayLiteral("sect"))
            && hideOuterDocumentTypeAndAppendMarkerFlood(markerFlood, 256),
        "document_recovery creates a partial native DOCM behind a flood of plausible outer markers");
    QString markerFloodError{};
    const auto markerFloodRecord = ui::recovery::readRecoveryRecord(
        markerFlood, &markerFloodError);
    check(markerFloodRecord && markerFloodRecord->partialRecovery
            && markerFloodRecord->missingSections.contains(QStringLiteral("sect"))
            && markerFloodRecord->warnings.contains(QStringLiteral("Recovery native-envelope fallback reached its bounded marker/candidate work budget; excess candidates were ignored.")),
        "document_recovery bounds marker candidates and cumulative native decode work while retaining checked section-anchor salvage");

    const std::filesystem::path noFooterAnchorFlood =
        root / "no-footer-anchor-flood.sedrecovery";
    QString noFooterAnchorFloodError{};
    check(writeNoFooterSectionAnchorFlood(noFooterAnchorFlood, 256)
            && !ui::recovery::readRecoveryRecord(
                noFooterAnchorFlood, &noFooterAnchorFloodError)
            && !noFooterAnchorFloodError.isEmpty(),
        "document_recovery scans a no-footer section-anchor flood with one bounded progressive footer pass");

    const std::filesystem::path repairedPrimary = root / "repaired-native-primary.sedrecovery";
    auto repairedPrimaryGate = std::make_shared<ui::recovery::PublicationGate>();
    repairedPrimaryGate->generation.store(1);
    const auto repairedPrimaryWrite = ui::recovery::writeRecoverySnapshot(
        makeRequest(repairedPrimary, repairedPrimaryGate, 1, ProjectType::MOTS, 5, false));
    check(repairedPrimaryWrite.status == ui::recovery::WriteStatus::published
            && damageNativePrimaryMagic(repairedPrimary),
        "document_recovery damages only the native primary magic while its checked mirror survives");
    const auto repairedPrimaryRecord = ui::recovery::readRecoveryRecord(
        repairedPrimary, &error);
    const auto repairedPrimaryLevel = repairedPrimaryRecord
        ? ui::recovery::loadRecoveredLevel(*repairedPrimaryRecord, &error)
        : std::optional<Level>{};
    check(repairedPrimaryRecord && repairedPrimaryRecord->corrupted
            && !repairedPrimaryRecord->partialRecovery
            && repairedPrimaryLevel && repairedPrimaryLevel->kind == ProjectType::MOTS,
        "document_recovery loads a complete native payload repaired from its mirror even without the sed magic gate");

    for (const ProjectType kind : { ProjectType::MOTS, ProjectType::IJIM })
    {
        const std::filesystem::path damagedFraming = root
            / (kind == ProjectType::MOTS
                ? "damaged-mots-native-framing.sedrecovery"
                : "damaged-ijim-native-framing.sedrecovery");
        auto framingGate = std::make_shared<ui::recovery::PublicationGate>();
        framingGate->generation.store(1);
        auto framingRequest = makeRequest(
            damagedFraming, framingGate, 1, kind, 6, false);
        const auto framingWrite = ui::recovery::writeRecoverySnapshot(
            framingRequest);
        QString framingError{};
        const auto framingBytes = [&]()
        {
            QByteArray value{};
            (void)readBytes(damagedFraming, &value);
            return value;
        }();
        check(framingWrite.status == ui::recovery::WriteStatus::published
                && !framingBytes.isEmpty()
                && destroyNativeSection(damagedFraming, QByteArrayLiteral("head"))
                && damageOuterAndNativeHeaders(damagedFraming),
            "document_recovery damages outer DOCM, native primary/mirror headers, and HEAD for project-kind fallback");
        const auto framingRecord = ui::recovery::readRecoveryRecord(
            damagedFraming, &framingError);
        const auto framingLevel = framingRecord
            ? ui::recovery::loadRecoveredLevel(*framingRecord, &framingError)
            : std::optional<Level>{};
        check(framingRecord && framingRecord->metadata.projectType == kind
                && framingRecord->metadata.projectTypeKnown()
            && framingRecord->documentTimestamp
                    == framingRequest.metadata.timestamp
                && framingLevel && framingLevel->kind == kind
                && framingLevel->header.version
                    == (kind == ProjectType::IJIM ? 3 : 1)
                && std::abs(framingLevel->header.gravity
                    - (kind == ProjectType::IJIM ? 2.0 : 4.0)) < 0.0001
                && framingRecord->missingSections.contains(QStringLiteral("head")),
            "document_recovery uses trusted META project type and safe HEAD defaults when HEAD and both native headers are lost");

        const std::filesystem::path nativeHeaderOnly = root
            / (kind == ProjectType::MOTS
                ? "native-mots-header-only.sedrecovery"
                : "native-ijim-header-only.sedrecovery");
        auto nativeHeaderGate = std::make_shared<ui::recovery::PublicationGate>();
        nativeHeaderGate->generation.store(1);
        const auto nativeHeaderWrite = ui::recovery::writeRecoverySnapshot(
            makeRequest(nativeHeaderOnly, nativeHeaderGate, 1, kind, 6, false));
        QString nativeHeaderError{};
        check(nativeHeaderWrite.status == ui::recovery::WriteStatus::published
                && destroyNativeSection(nativeHeaderOnly, QByteArrayLiteral("head"))
                && damagePrimaryRecoveryMetadata(nativeHeaderOnly)
                && damageMetaShards(nativeHeaderOnly, 3),
            "document_recovery damages META and HEAD while retaining checked native headers");
        const auto nativeHeaderRecord = ui::recovery::readRecoveryRecord(
            nativeHeaderOnly, &nativeHeaderError);
        const auto nativeHeaderLevel = nativeHeaderRecord
            ? ui::recovery::loadRecoveredLevel(*nativeHeaderRecord, &nativeHeaderError)
            : std::optional<Level>{};
        check(nativeHeaderRecord && nativeHeaderRecord->metadata.projectTypeKnown()
                && nativeHeaderRecord->metadata.projectTypeTrust
                    == ui::recovery::ProjectTypeTrust::native
                && nativeHeaderLevel && nativeHeaderLevel->kind == kind
                && nativeHeaderLevel->header.version
                    == (kind == ProjectType::IJIM ? 3 : 1)
                && std::abs(nativeHeaderLevel->header.gravity
                    - (kind == ProjectType::IJIM ? 2.0 : 4.0)) < 0.0001,
            "document_recovery trusts the surviving native fixed header and applies safe HEAD defaults when META and HEAD are lost");
        if (nativeHeaderRecord)
        {
            auto mismatched = *nativeHeaderRecord;
            mismatched.metadata.projectType = kind == ProjectType::MOTS
                ? ProjectType::IJIM : ProjectType::MOTS;
            mismatched.metadata.projectTypeTrust = ui::recovery::ProjectTypeTrust::user;
            QString mismatchError{};
            check(!ui::recovery::loadRecoveredLevel(mismatched, &mismatchError)
                    && exactRecoveryText(mismatchError, QStringLiteral("Recovery metadata and trusted native SED project type disagree.")),
                "document_recovery rejects a user/META project choice that disagrees with a trusted native header");
        }

        const std::filesystem::path unknownFraming = root
            / (kind == ProjectType::MOTS
                ? "unknown-mots-native-framing.sedrecovery"
                : "unknown-ijim-native-framing.sedrecovery");
        auto unknownGate = std::make_shared<ui::recovery::PublicationGate>();
        unknownGate->generation.store(1);
        const auto unknownWrite = ui::recovery::writeRecoverySnapshot(
            makeRequest(unknownFraming, unknownGate, 1, kind, 6, true));
        QString unknownError{};
        check(unknownWrite.status == ui::recovery::WriteStatus::published
                && destroyNativeSection(unknownFraming, QByteArrayLiteral("head"))
                && damageOuterAndNativeHeaders(unknownFraming)
                && damagePrimaryRecoveryMetadata(unknownFraming)
                && damageMetaShards(unknownFraming, 3),
            "document_recovery damages META plus outer/native headers and HEAD for unknown-project recovery");
        const auto unknownRecord = ui::recovery::readRecoveryRecord(
            unknownFraming, &unknownError);
        check(unknownRecord && !unknownRecord->metadata.projectTypeKnown()
                && unknownRecord->history == std::nullopt
                && !unknownRecord->historyPayload.isEmpty()
                && !ui::recovery::loadRecoveredLevel(*unknownRecord, &unknownError),
            "document_recovery never infers JKDF2 or decodes HIST when every project discriminator is lost");
        if (unknownRecord)
        {
            auto historyMismatchRecord = *unknownRecord;
            historyMismatchRecord.metadata.projectType = kind == ProjectType::MOTS
                ? ProjectType::IJIM : ProjectType::MOTS;
            historyMismatchRecord.metadata.projectTypeTrust =
                ui::recovery::ProjectTypeTrust::user;
            QString historyMismatchError{};
            const auto historyMismatchDocument =
                ui::recovery::loadRecoveredDocument(
                    historyMismatchRecord, &historyMismatchError);
            check(historyMismatchDocument
                    && !historyMismatchDocument->history
                    && historyMismatchDocument->missingSections.contains(
                        QStringLiteral("hist"))
                    && historyMismatchDocument->warnings.contains(QStringLiteral("Recovered HIST semantic validation failed: Recovery undo history base digest or fragment count does not match the current Level.. The Level was retained, undo/redo history was dropped, and a clean save is required.")),
                "document_recovery keeps the authoritative Level when user-selected project semantics invalidate optional HIST");

            auto selectedRecord = *unknownRecord;
            selectedRecord.metadata.projectType = kind;
            selectedRecord.metadata.projectTypeTrust =
                ui::recovery::ProjectTypeTrust::user;
            QString selectedError{};
            const auto selectedDocument = ui::recovery::loadRecoveredDocument(
                selectedRecord, &selectedError);
            check(selectedDocument && selectedDocument->current.level.kind == kind
                    && selectedDocument->current.level.header.version
                        == (kind == ProjectType::IJIM ? 3 : 1)
                    && std::abs(selectedDocument->current.level.header.gravity
                        - (kind == ProjectType::IJIM ? 2.0 : 4.0)) < 0.0001
                    && !selectedDocument->history
                    && selectedDocument->missingSections.contains(
                        QStringLiteral("hist"))
                    && selectedDocument->warnings.contains(QStringLiteral("Recovered HIST semantic validation failed: Recovery undo history base digest or fragment count does not match the current Level.. The Level was retained, undo/redo history was dropped, and a clean save is required.")),
                "document_recovery applies user-selected project type and safe HEAD defaults, then drops HIST whose exact current-Level base was irrecoverably changed");
        }
    }

    const std::filesystem::path truncated = root / "truncated.sedrecovery";
    writeBytes(truncated, bytes.left(std::max<qsizetype>(0, bytes.size() - 11)));
    const auto truncatedRecord = ui::recovery::readRecoveryRecord(truncated);
    check(!truncatedRecord || truncatedRecord->corrupted || truncatedRecord->partialRecovery,
        "document_recovery rejects or explicitly marks a truncated container instead of silently accepting it");

    const std::filesystem::path malformed = root / "malformed.sedrecovery";
    writeBytes(malformed, QByteArrayLiteral("sed_recovery_v1\n0\n\n{}"));
    check(!ui::recovery::readRecoveryRecord(malformed).has_value(),
        "document_recovery rejects malformed containers without a DOCM");
    QString cleanupError{};
    check(ui::recovery::invalidateAndRemoveRecovery(gate, path, &cleanupError)
            && cleanupError.isEmpty() && !std::filesystem::exists(path),
        "document_recovery invalidates the publication gate before cleanup");

    const std::filesystem::path historyGuardPath =
        root / "policy-guard.sedautosave";
    writeBytes(historyGuardPath, QByteArrayLiteral("history"));
    QString historyGuardError{};
    const bool accidentalHistoryDelete =
        ui::recovery::invalidateAndRemoveRecovery(
            nullptr, historyGuardPath, &historyGuardError);
    check(!accidentalHistoryDelete
            && !historyGuardError.isEmpty()
            && std::filesystem::exists(historyGuardPath),
        "document_recovery blocks accidental deletion of .sedautosave history at the low-level cleanup boundary");
    QString explicitHistoryDeleteError{};
    const bool explicitHistoryDelete =
        ui::recovery::invalidateAndRemoveRecovery(
            nullptr, historyGuardPath, &explicitHistoryDeleteError, nullptr,
            ui::recovery::RecoveryRemovalReason::explicitHistoryDelete);
    check(explicitHistoryDelete
            && explicitHistoryDeleteError.isEmpty()
            && !std::filesystem::exists(historyGuardPath),
        "document_recovery permits only the explicitly authorized history deletion reason");
    const std::filesystem::path liveReasonGuardPath =
        root / "policy-guard.sedrecovery";
    writeBytes(liveReasonGuardPath, QByteArrayLiteral("live"));
    QString liveReasonGuardError{};
    const bool accidentalLivePrune =
        ui::recovery::invalidateAndRemoveRecovery(
            nullptr, liveReasonGuardPath, &liveReasonGuardError, nullptr,
            ui::recovery::RecoveryRemovalReason::retentionPrune);
    check(!accidentalLivePrune
            && !liveReasonGuardError.isEmpty()
            && std::filesystem::exists(liveReasonGuardPath),
        "document_recovery rejects retention pruning when the target is not autosave history");
    std::error_code liveCleanupError{};
    std::filesystem::remove(liveReasonGuardPath, liveCleanupError);
}

} // namespace sed::app_tests
