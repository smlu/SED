#include "ui/recovery/architectural_lighting_workspace_utils.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_set>
#include <utility>

#include <QCborArray>
#include <QCborMap>

namespace sed::ui::recovery
{
namespace
{

// The decoded DTO, temporary uniqueness table, CBOR values and hash buckets
// coexist during admission/encoding. Use a deliberately conservative
// per-identity estimate so an attacker cannot turn a small EDIT payload into
// an unbounded collection of individually small allocations.
constexpr std::size_t kMaximumOwnedIdentityBytes = 16u * 1024u * 1024u;
constexpr std::size_t kEstimatedIdentityStorageBytes = 192u;
constexpr std::size_t kMaximumIdentityCount = 1'000'000u;
constexpr std::size_t kMaximumIdentityCountByBytes =
    kMaximumOwnedIdentityBytes / kEstimatedIdentityStorageBytes;

void setError(QString* const error, const QString& message)
{
    if ( error )
    {
        *error = message;
    }
}

bool isIntegerInInt32Range(const QCborValue& value, std::int32_t* result)
{
    if ( !value.isInteger() )
    {
        return false;
    }

    const qint64 integer = value.toInteger();
    if ( integer < 0
        || integer > std::numeric_limits<std::int32_t>::max() )
    {
        return false;
    }

    *result = static_cast<std::int32_t>(integer);
    return true;
}

bool isAuthoringMode(const qint64 value)
{
    return value == static_cast<qint64>(
            level::MotsArchLightingVertexMode::Mono)
        || value == static_cast<qint64>(
            level::MotsArchLightingVertexMode::Rgb);
}

bool isTargetMode(const qint64 value)
{
    return value == static_cast<qint64>(
            GlView::ArchitecturalLightingTargetMode::Owner)
        || value == static_cast<qint64>(
            GlView::ArchitecturalLightingTargetMode::Vertices);
}

} // namespace

bool architecturalLightingWorkspaceToCbor(
    const GlView::RecoveryState::ArchitecturalLightingState& state,
    QCborValue* const output,
    QString* const error)
{
    if ( !output )
    {
        setError(error, QStringLiteral("The MotS Arch workspace output is missing."));
        return false;
    }

    if ( !isAuthoringMode(static_cast<qint64>(state.authoringMode))
        || !isTargetMode(static_cast<qint64>(state.targetMode)) )
    {
        setError(error, QStringLiteral(
            "The MotS Arch workspace contains an invalid authoring or target mode."));
        return false;
    }

    const std::size_t count = state.selectedVertices.size();
    constexpr std::size_t kMaximumAdmittedCount =
        kMaximumIdentityCount < kMaximumIdentityCountByBytes
        ? kMaximumIdentityCount
        : kMaximumIdentityCountByBytes;
    if ( count > kMaximumAdmittedCount )
    {
        setError(error, QStringLiteral(
            "The MotS Arch workspace exceeds the 16 MiB selection budget."));
        return false;
    }

    std::unordered_set<GlView::ArchitecturalLightingVertexIdentity,
        GlView::ArchitecturalLightingVertexIdentityHash> uniqueIdentities{};
    uniqueIdentities.reserve(count);
    for ( const GlView::ArchitecturalLightingVertexIdentity& identity
          : state.selectedVertices )
    {
        if ( identity.thingIndex < 0 || identity.lodIndex != 0
            || identity.meshIndex < 0 || identity.vertexIndex < 0 )
        {
            setError(error, QStringLiteral(
                "The MotS Arch workspace contains an invalid vertex identity."));
            return false;
        }
        if ( !uniqueIdentities.insert(identity).second )
        {
            setError(error, QStringLiteral(
                "The MotS Arch workspace contains a duplicate vertex identity."));
            return false;
        }
    }

    QCborArray selectedVertices{};
    for ( const GlView::ArchitecturalLightingVertexIdentity& identity
          : state.selectedVertices )
    {
        QCborArray encodedIdentity{};
        encodedIdentity.append(identity.thingIndex);
        encodedIdentity.append(identity.lodIndex);
        encodedIdentity.append(identity.meshIndex);
        encodedIdentity.append(identity.vertexIndex);
        selectedVertices.append(encodedIdentity);
    }

    QCborMap encoded{};
    encoded.insert(QStringLiteral("version"), 1);
    encoded.insert(
        QStringLiteral("authoringMode"),
        static_cast<int>(state.authoringMode));
    encoded.insert(
        QStringLiteral("targetMode"),
        static_cast<int>(state.targetMode));
    encoded.insert(QStringLiteral("selectedVertices"), selectedVertices);
    encoded.insert(
        QStringLiteral("implicitOwnerExpansion"),
        state.implicitOwnerExpansion);
    *output = QCborValue(encoded);
    if ( error )
    {
        error->clear();
    }
    return true;
}

bool architecturalLightingWorkspaceFromCbor(
    const QCborValue& value,
    const Level& level,
    GlView::RecoveryState::ArchitecturalLightingState* const state,
    QString* const error)
{
    if ( !state )
    {
        setError(error, QStringLiteral("The MotS Arch workspace output is missing."));
        return false;
    }
    if ( !value.isMap() )
    {
        setError(error, QStringLiteral("The MotS Arch workspace must be a map."));
        return false;
    }

    const QCborMap encoded = value.toMap();
    if ( !encoded.contains(QStringLiteral("version"))
        || !encoded.contains(QStringLiteral("authoringMode"))
        || !encoded.contains(QStringLiteral("targetMode"))
        || !encoded.contains(QStringLiteral("selectedVertices"))
        || !encoded.contains(QStringLiteral("implicitOwnerExpansion")) )
    {
        setError(error, QStringLiteral("The MotS Arch workspace fields are incomplete or unknown."));
        return false;
    }

    const QCborValue version = encoded.value(QStringLiteral("version"));
    const QCborValue authoringMode = encoded.value(QStringLiteral("authoringMode"));
    const QCborValue targetMode = encoded.value(QStringLiteral("targetMode"));
    const QCborValue selectedVerticesValue = encoded.value(
        QStringLiteral("selectedVertices"));
    const QCborValue implicitOwnerExpansion = encoded.value(
        QStringLiteral("implicitOwnerExpansion"));
    if ( !version.isInteger()
        || version.toInteger() != 1
        || !authoringMode.isInteger()
        || !isAuthoringMode(authoringMode.toInteger())
        || !targetMode.isInteger()
        || !isTargetMode(targetMode.toInteger())
        || !selectedVerticesValue.isArray()
        || !implicitOwnerExpansion.isBool() )
    {
        setError(error, QStringLiteral("The MotS Arch workspace contains an invalid version, mode, or field type."));
        return false;
    }

    const QCborArray selectedVertices = selectedVerticesValue.toArray();
    const std::size_t count = static_cast<std::size_t>(selectedVertices.size());
    constexpr std::size_t kMaximumAdmittedCount =
        kMaximumIdentityCount < kMaximumIdentityCountByBytes
        ? kMaximumIdentityCount
        : kMaximumIdentityCountByBytes;
    if ( count > kMaximumAdmittedCount )
    {
        setError(error, QStringLiteral(
            "The MotS Arch workspace exceeds the 16 MiB selection budget."));
        return false;
    }

    GlView::RecoveryState::ArchitecturalLightingState parsed{};
    parsed.authoringMode = static_cast<level::MotsArchLightingVertexMode>(
        authoringMode.toInteger());
    parsed.targetMode = static_cast<GlView::ArchitecturalLightingTargetMode>(
        targetMode.toInteger());
    parsed.implicitOwnerExpansion = implicitOwnerExpansion.toBool();
    parsed.selectedVertices.reserve(count);

    std::unordered_set<GlView::ArchitecturalLightingVertexIdentity,
        GlView::ArchitecturalLightingVertexIdentityHash> uniqueIdentities{};
    uniqueIdentities.reserve(count);
    for ( const QCborValue& encodedIdentityValue : selectedVertices )
    {
        if ( !encodedIdentityValue.isArray() )
        {
            setError(error, QStringLiteral("A MotS Arch vertex identity is not an array."));
            return false;
        }

        const QCborArray encodedIdentity = encodedIdentityValue.toArray();
        if ( encodedIdentity.size() != 4 )
        {
            setError(error, QStringLiteral("A MotS Arch vertex identity must contain four indices."));
            return false;
        }

        GlView::ArchitecturalLightingVertexIdentity identity{};
        if ( !isIntegerInInt32Range(encodedIdentity.at(0), &identity.thingIndex)
            || !isIntegerInInt32Range(encodedIdentity.at(1), &identity.lodIndex)
            || !isIntegerInInt32Range(encodedIdentity.at(2), &identity.meshIndex)
            || !isIntegerInInt32Range(encodedIdentity.at(3), &identity.vertexIndex) )
        {
            setError(error, QStringLiteral("A MotS Arch vertex identity contains an invalid index."));
            return false;
        }
        if ( identity.lodIndex != 0
            || static_cast<std::size_t>(identity.thingIndex) >= level.things.size() )
        {
            setError(error, QStringLiteral("A MotS Arch vertex identity refers to an unavailable Thing or LOD."));
            return false;
        }
        if ( !uniqueIdentities.insert(identity).second )
        {
            setError(error, QStringLiteral("The MotS Arch workspace contains a duplicate vertex identity."));
            return false;
        }
        parsed.selectedVertices.push_back(identity);
    }

    *state = std::move(parsed);
    if ( error )
    {
        error->clear();
    }
    return true;
}

} // namespace sed::ui::recovery
