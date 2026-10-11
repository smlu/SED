#include "test_common.hpp"

#include "sed/level/geometry.hpp"
#include "sed/level/build.hpp"
#include "sed/level/cleave.hpp"
#include "sed/level/merge.hpp"
#include "sed/level/uv.hpp"
#include "sed/editor/uv_promotion.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sed::tests
{

namespace
{

bool sameFloatBits(const float lhs, const float rhs) noexcept
{
    return std::bit_cast<std::uint32_t>(lhs)
        == std::bit_cast<std::uint32_t>(rhs);
}

bool sameColorBits(const sed::ColorF& lhs, const sed::ColorF& rhs) noexcept
{
    return sameFloatBits(lhs.r, rhs.r)
        && sameFloatBits(lhs.g, rhs.g)
        && sameFloatBits(lhs.b, rhs.b)
        && sameFloatBits(lhs.a, rhs.a);
}

bool sameTexVertexBits(
    const sed::TexVertex& lhs,
    const sed::TexVertex& rhs) noexcept
{
    return sameFloatBits(lhs.u, rhs.u)
        && sameFloatBits(lhs.v, rhs.v)
        && sameColorBits(lhs.color, rhs.color);
}

bool sameSurfaceUvBits(
    const sed::Surface& lhs,
    const sed::Surface& rhs) noexcept
{
    if (!sameFloatBits(lhs.uScale, rhs.uScale)
        || !sameFloatBits(lhs.vScale, rhs.vScale)
        || lhs.texVertices.size() != rhs.texVertices.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < lhs.texVertices.size(); ++index)
    {
        if (!sameTexVertexBits(
                lhs.texVertices[index], rhs.texVertices[index]))
        {
            return false;
        }
    }
    return true;
}

bool sameSectorUvBits(
    const sed::Sector& lhs,
    const sed::Sector& rhs) noexcept
{
    if (lhs.surfaces.size() != rhs.surfaces.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < lhs.surfaces.size(); ++index)
    {
        if (!sameSurfaceUvBits(lhs.surfaces[index], rhs.surfaces[index]))
        {
            return false;
        }
    }
    return true;
}

bool sameLevelUvBits(
    const sed::Level& lhs,
    const sed::Level& rhs) noexcept
{
    if (lhs.uvCoordinateSpace != rhs.uvCoordinateSpace
        || lhs.sectors.size() != rhs.sectors.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < lhs.sectors.size(); ++index)
    {
        if (!sameSectorUvBits(lhs.sectors[index], rhs.sectors[index]))
        {
            return false;
        }
    }
    return true;
}

bool closeSurfaceUv(
    const sed::Surface& lhs,
    const sed::Surface& rhs,
    const double epsilon = kRotationEpsilon) noexcept
{
    if ( !close(lhs.uScale, rhs.uScale, epsilon)
        || !close(lhs.vScale, rhs.vScale, epsilon)
        || lhs.faceFlags != rhs.faceFlags
        || lhs.texVertices.size() != rhs.texVertices.size() )
    {
        return false;
    }

    for ( std::size_t index = 0; index < lhs.texVertices.size(); ++index )
    {
        const sed::TexVertex& lhsVertex = lhs.texVertices[index];
        const sed::TexVertex& rhsVertex = rhs.texVertices[index];
        if ( !close(lhsVertex.u, rhsVertex.u, epsilon)
            || !close(lhsVertex.v, rhsVertex.v, epsilon)
            || !close(lhsVertex.color.r, rhsVertex.color.r, epsilon)
            || !close(lhsVertex.color.g, rhsVertex.color.g, epsilon)
            || !close(lhsVertex.color.b, rhsVertex.color.b, epsilon)
            || !close(lhsVertex.color.a, rhsVertex.color.a, epsilon) )
        {
            return false;
        }
    }
    return true;
}

bool closeLevelGeometryAndUvs(
    const sed::Level& lhs,
    const sed::Level& rhs,
    const double epsilon = kRotationEpsilon) noexcept
{
    if (lhs.uvCoordinateSpace != rhs.uvCoordinateSpace
        || lhs.sectors.size() != rhs.sectors.size())
    {
        return false;
    }

    for (std::size_t sectorIndex = 0;
         sectorIndex < lhs.sectors.size();
         ++sectorIndex)
    {
        const sed::Sector& lhsSector = lhs.sectors[sectorIndex];
        const sed::Sector& rhsSector = rhs.sectors[sectorIndex];
        if (lhsSector.vertices.size() != rhsSector.vertices.size()
            || lhsSector.surfaces.size() != rhsSector.surfaces.size())
        {
            return false;
        }

        for (std::size_t vertexIndex = 0;
             vertexIndex < lhsSector.vertices.size();
             ++vertexIndex)
        {
            const sed::Vertex& lhsVertex = lhsSector.vertices[vertexIndex];
            const sed::Vertex& rhsVertex = rhsSector.vertices[vertexIndex];
            if (!close(lhsVertex.x, rhsVertex.x, epsilon)
                || !close(lhsVertex.y, rhsVertex.y, epsilon)
                || !close(lhsVertex.z, rhsVertex.z, epsilon)
                || lhsVertex.mark != rhsVertex.mark)
            {
                return false;
            }
        }

        for (std::size_t surfaceIndex = 0;
             surfaceIndex < lhsSector.surfaces.size();
             ++surfaceIndex)
        {
            const sed::Surface& lhsSurface =
                lhsSector.surfaces[surfaceIndex];
            const sed::Surface& rhsSurface =
                rhsSector.surfaces[surfaceIndex];
            if (lhsSurface.vertices != rhsSurface.vertices
                || lhsSurface.adjoin != rhsSurface.adjoin
                || lhsSurface.material != rhsSurface.material
                || lhsSurface.surfaceFlags != rhsSurface.surfaceFlags
                || lhsSurface.faceFlags != rhsSurface.faceFlags
                || !close(lhsSurface.normal.x, rhsSurface.normal.x, epsilon)
                || !close(lhsSurface.normal.y, rhsSurface.normal.y, epsilon)
                || !close(lhsSurface.normal.z, rhsSurface.normal.z, epsilon)
                || !closeSurfaceUv(lhsSurface, rhsSurface, epsilon))
            {
                return false;
            }
        }
    }
    return true;
}

} // namespace

void runLevelUvTests()
{
    {
        constexpr double textureWidth = 128.0;
        constexpr double textureHeight = 64.0;
        const auto dimensions = [](const sed::Surface&)
            -> std::optional<sed::level::UvTextureDimensions>
        {
            return sed::level::UvTextureDimensions{
                textureWidth,
                textureHeight };
        };

        sed::Level normalized{};
        normalized.kind = sed::ProjectType::JKDF2;
        normalized.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        sed::Sector& sourceSector = sed::level::levelCreateCube(
            normalized,
            0.0,
            0.0,
            0.0,
            sed::TVector::up,
            sed::TVector::right);
        for (std::size_t surfaceIndex = 0;
             surfaceIndex < sourceSector.surfaces.size();
             ++surfaceIndex)
        {
            sed::Surface& surface = sourceSector.surfaces[surfaceIndex];
            surface.material = "pipeline.mat";
            surface.uScale = 0.03125f;
            surface.vScale = 0.09375f;
            for (std::size_t corner = 0;
                 corner < surface.texVertices.size();
                 ++corner)
            {
                surface.texVertices[corner].u = -1.25f
                    + static_cast<float>(surfaceIndex) * 0.5f
                    + static_cast<float>(corner) * 1.125f;
                surface.texVertices[corner].v = 2.75f
                    - static_cast<float>(surfaceIndex) * 0.375f
                    - static_cast<float>(corner) * 0.875f;
            }
        }

        sed::Level pixels = normalized;
        const sed::level::LevelUvConversionResult toPixels =
            sed::level::convertLevelUvCoordinateSpace(
                pixels,
                sed::UvCoordinateSpace::TexturePixels,
                dimensions);
        const bool normalizedCleaved = sed::level::levelCleaveSector(
            normalized, 0, sed::TVector::right, 0.5, 0.0, 0.0);
        const bool pixelsCleaved = sed::level::levelCleaveSector(
            pixels, 0, sed::TVector::right, 0.5, 0.0, 0.0);
        const std::optional<std::int32_t> normalizedMerged =
            normalizedCleaved
            ? sed::level::levelMergeSectors(normalized, 0, 1)
            : std::nullopt;
        const std::optional<std::int32_t> pixelsMerged = pixelsCleaved
            ? sed::level::levelMergeSectors(pixels, 0, 1)
            : std::nullopt;

        const auto firstSolidSurface = [](const sed::Level& level)
            -> std::optional<sed::SurfaceRef>
        {
            for (std::size_t sectorIndex = 0;
                 sectorIndex < level.sectors.size();
                 ++sectorIndex)
            {
                const sed::Sector& sector = level.sectors[sectorIndex];
                for (std::size_t surfaceIndex = 0;
                     surfaceIndex < sector.surfaces.size();
                     ++surfaceIndex)
                {
                    if (!sector.surfaces[surfaceIndex].adjoin)
                    {
                        return sed::SurfaceRef{
                            static_cast<std::int32_t>(sectorIndex),
                            static_cast<std::int32_t>(surfaceIndex) };
                    }
                }
            }
            return std::nullopt;
        };
        const std::optional<sed::SurfaceRef> normalizedExtrudeSurface =
            firstSolidSurface(normalized);
        const std::optional<sed::SurfaceRef> pixelsExtrudeSurface =
            firstSolidSurface(pixels);
        const bool normalizedExtruded = normalizedExtrudeSurface
            && sed::level::levelExtrudeSurface(
                normalized, *normalizedExtrudeSurface, 0.75);
        const bool pixelsExtruded = pixelsExtrudeSurface
            && sed::level::levelExtrudeSurface(
                pixels, *pixelsExtrudeSurface, 0.75);
        const sed::level::LevelUvConversionResult backToNormalized =
            sed::level::convertLevelUvCoordinateSpace(
                pixels,
                sed::UvCoordinateSpace::Normalized,
                dimensions);

        check(static_cast<bool>(toPixels)
                && normalizedCleaved && pixelsCleaved
                && normalizedMerged && pixelsMerged
                && normalizedExtrudeSurface == pixelsExtrudeSurface
                && normalizedExtruded && pixelsExtruded
                && static_cast<bool>(backToNormalized)
                && closeLevelGeometryAndUvs(normalized, pixels),
            "UV edit pipeline keeps cleave, reciprocal-sector merge, and extrusion geometry plus signed multi-tile mapping equivalent across normalized and pixel compatibility spaces");
    }

    {
        auto makePixelLevel = [](const std::string& material,
                                  const float uOffset) -> sed::Level
        {
            sed::Level level{};
            level.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;
            sed::Sector sector{};
            sector.vertices = {
                { 0.0, 0.0, 0.0, 0 },
                { 1.0, 0.0, 0.0, 1 },
                { 0.0, 1.0, 0.0, 2 },
            };
            sed::Surface surface{};
            surface.material = material;
            surface.vertices = { 0, 1, 2 };
            surface.texVertices = {
                { uOffset, 16.0f, sed::ColorRed },
                { uOffset + 64.0f, 16.0f, sed::ColorGreen },
                { uOffset, 48.0f, sed::ColorBlue },
            };
            surface.uScale = 32.0f;
            surface.vScale = 16.0f;
            sector.surfaces.push_back(std::move(surface));
            level.sectors.push_back(std::move(sector));
            return level;
        };

        const sed::level::UvTextureDimensionsResolver dimensions =
            [](const sed::Surface& surface)
                -> std::optional<sed::level::UvTextureDimensions>
            {
                if (surface.material == "known.mat")
                {
                    return sed::level::UvTextureDimensions{ 128.0, 64.0 };
                }
                return std::nullopt;
            };

        sed::Level current = makePixelLevel("known.mat", 32.0f);
        const sed::Level originalCurrent = current;
        sed::editor::DocumentRecoveryHistory history{};
        sed::editor::DocumentRecoveryHistoryEntry earlier{};
        earlier.label = "earlier pixels";
        earlier.dirtyFlags = static_cast<std::uint32_t>(
            sed::editor::DocumentDirty::Structure);
        earlier.state.level = makePixelLevel("known.mat", 16.0f);
        earlier.state.selection.sectors.insert(0);
        earlier.navigationSelection.emplace();
        earlier.navigationSelection->surfaces.insert({ 0, 0 });
        history.undoEntries.push_back(std::move(earlier));
        history.undoEntries.push_back({
            "oldest pixels",
            static_cast<std::uint32_t>(sed::editor::DocumentDirty::Structure),
            { makePixelLevel("known.mat", 0.0f), {} },
            std::nullopt,
        });
        sed::editor::DocumentRecoveryHistoryEntry later{};
        later.label = "later pixels";
        later.dirtyFlags = static_cast<std::uint32_t>(
            sed::editor::DocumentDirty::Structure);
        later.state.level = makePixelLevel("known.mat", 64.0f);
        later.state.selection.surfaces.insert({ 0, 0 });
        history.redoEntries.push_back(std::move(later));

        const sed::editor::DocumentUvConversionResult converted =
            sed::editor::convertDocumentUvCoordinateSpace(
                current,
                history,
                sed::UvCoordinateSpace::Normalized,
                dimensions);
        sed::Level reverseCurrent = current;
        sed::editor::DocumentRecoveryHistory reverseHistory = history;
        const sed::editor::DocumentUvConversionResult reversed =
            sed::editor::convertDocumentUvCoordinateSpace(
                reverseCurrent,
                reverseHistory,
                sed::UvCoordinateSpace::TexturePixels,
                dimensions);
        sed::editor::Document restored({ current, {} });
        restored.restoreRecoveryHistory(
            restored.current(), std::move(history));
        const bool labelsPreserved = restored.undoLabels()
                == std::vector<std::string>{
                    "oldest pixels", "earlier pixels" }
            && restored.redoLabels()
                == std::vector<std::string>{ "later pixels" };
        const bool firstUndoConverted = restored.undo()
            && restored.level().uvCoordinateSpace
                == sed::UvCoordinateSpace::Normalized
            && close(restored.level().sectors[0].surfaces[0].texVertices[0].u,
                16.0 / 128.0)
            && close(restored.level().sectors[0].surfaces[0].uScale,
                32.0 / 128.0)
            && restored.selection().sectors.contains(0);
        const bool secondUndoConverted = restored.undo()
            && close(restored.level().sectors[0].surfaces[0].texVertices[0].u,
                0.0);
        const bool firstCurrentRestored = restored.redo()
            && close(restored.level().sectors[0].surfaces[0].texVertices[0].u,
                16.0 / 128.0);
        const bool currentRestored = restored.redo()
            && close(restored.level().sectors[0].surfaces[0].texVertices[0].u,
                32.0 / 128.0);
        const bool redoConverted = restored.redo()
            && restored.level().uvCoordinateSpace
                == sed::UvCoordinateSpace::Normalized
            && close(restored.level().sectors[0].surfaces[0].texVertices[0].u,
                64.0 / 128.0)
            && close(restored.level().sectors[0].surfaces[0].vScale,
                16.0 / 64.0);
        sed::editor::Document reverseRestored({ reverseCurrent, {} });
        reverseRestored.restoreRecoveryHistory(
            reverseRestored.current(), std::move(reverseHistory));
        const bool reverseUndoUsesPixels = reverseRestored.undo()
            && reverseRestored.level().uvCoordinateSpace
                == sed::UvCoordinateSpace::TexturePixels
            && close(reverseRestored.level().sectors[0]
                    .surfaces[0].texVertices[0].u,
                16.0)
            && reverseRestored.selection().sectors.contains(0);
        check(converted && reversed
                && sameLevelUvBits(reverseCurrent, originalCurrent)
                && labelsPreserved
                && firstUndoConverted && secondUndoConverted
                && firstCurrentRestored && currentRestored && redoConverted
                && reverseUndoUsesPixels,
            "document_uv_promotion converts the current Level plus ordered undo/redo histories in both directions while preserving labels, selections, and exact mappings");

        sed::Level failedCurrent = makePixelLevel("known.mat", 32.0f);
        sed::editor::DocumentRecoveryHistory failedHistory{};
        failedHistory.undoEntries.push_back({
            "missing historical MAT",
            static_cast<std::uint32_t>(sed::editor::DocumentDirty::Structure),
            { makePixelLevel("missing.mat", 16.0f), {} },
            std::nullopt,
        });
        const sed::Level failedCurrentBefore = failedCurrent;
        const sed::editor::DocumentRecoveryHistory failedHistoryBefore =
            failedHistory;
        const sed::editor::DocumentUvConversionResult rejected =
            sed::editor::convertDocumentUvCoordinateSpace(
                failedCurrent,
                failedHistory,
                sed::UvCoordinateSpace::Normalized,
                dimensions);
        check(!rejected
                && rejected.failedHistoryStack
                    == sed::editor::DocumentRecoveryHistoryStack::Undo
                && rejected.failedHistoryIndex == 0
                && sameLevelUvBits(failedCurrent, failedCurrentBefore)
                && failedHistory.undoEntries.size()
                    == failedHistoryBefore.undoEntries.size()
                && sameLevelUvBits(
                    failedHistory.undoEntries[0].state.level,
                    failedHistoryBefore.undoEntries[0].state.level),
            "document_uv_promotion leaves the current Level and complete history unchanged when only a historical MAT dimension is unavailable");

        sed::Level failedRedoCurrent = makePixelLevel("known.mat", 32.0f);
        sed::editor::DocumentRecoveryHistory failedRedoHistory{};
        failedRedoHistory.redoEntries.push_back({
            "known redo state",
            static_cast<std::uint32_t>(sed::editor::DocumentDirty::Structure),
            { makePixelLevel("known.mat", 48.0f), {} },
            std::nullopt,
        });
        failedRedoHistory.redoEntries.push_back({
            "missing redo MAT",
            static_cast<std::uint32_t>(sed::editor::DocumentDirty::Structure),
            { makePixelLevel("missing.mat", 80.0f), {} },
            std::nullopt,
        });
        const sed::Level failedRedoCurrentBefore = failedRedoCurrent;
        const sed::editor::DocumentRecoveryHistory failedRedoHistoryBefore =
            failedRedoHistory;
        const sed::editor::DocumentUvConversionResult rejectedRedo =
            sed::editor::convertDocumentUvCoordinateSpace(
                failedRedoCurrent,
                failedRedoHistory,
                sed::UvCoordinateSpace::Normalized,
                dimensions);
        check(!rejectedRedo
                && rejectedRedo.failedHistoryStack
                    == sed::editor::DocumentRecoveryHistoryStack::Redo
                && rejectedRedo.failedHistoryIndex == 1
                && rejectedRedo.failedMaterial == "missing.mat"
                && sameLevelUvBits(
                    failedRedoCurrent, failedRedoCurrentBefore)
                && failedRedoHistory.redoEntries.size()
                    == failedRedoHistoryBefore.redoEntries.size()
                && sameLevelUvBits(
                    failedRedoHistory.redoEntries[0].state.level,
                    failedRedoHistoryBefore.redoEntries[0].state.level)
                && sameLevelUvBits(
                    failedRedoHistory.redoEntries[1].state.level,
                    failedRedoHistoryBefore.redoEntries[1].state.level),
            "document_uv_promotion reports the exact redo transition and leaves the whole document atomic when a later redo MAT is unavailable");

        sed::Level invalidDestinationCurrent =
            makePixelLevel("known.mat", 32.0f);
        sed::editor::DocumentRecoveryHistory invalidDestinationHistory{};
        invalidDestinationHistory.undoEntries.push_back({
            "valid history",
            static_cast<std::uint32_t>(sed::editor::DocumentDirty::Structure),
            { makePixelLevel("known.mat", 16.0f), {} },
            std::nullopt,
        });
        const sed::Level invalidDestinationCurrentBefore =
            invalidDestinationCurrent;
        const sed::editor::DocumentRecoveryHistory invalidDestinationHistoryBefore =
            invalidDestinationHistory;
        const sed::editor::DocumentUvConversionResult invalidDestination =
            sed::editor::convertDocumentUvCoordinateSpace(
                invalidDestinationCurrent,
                invalidDestinationHistory,
                static_cast<sed::UvCoordinateSpace>(99),
                dimensions);
        check(!invalidDestination
                && !invalidDestination.failedHistoryStack
                && invalidDestination.failedHistoryIndex == 0
                && !invalidDestination.error.empty()
                && sameLevelUvBits(
                    invalidDestinationCurrent,
                    invalidDestinationCurrentBefore)
                && invalidDestinationHistory.undoEntries.size()
                    == invalidDestinationHistoryBefore.undoEntries.size()
                && sameLevelUvBits(
                    invalidDestinationHistory.undoEntries[0].state.level,
                    invalidDestinationHistoryBefore.undoEntries[0].state.level),
            "document_uv_promotion rejects an unknown destination before touching current or historical UV state");

        sed::Level compactCurrent = makePixelLevel("known.mat", 48.0f);
        sed::editor::Document compactSource({ compactCurrent, {} });
        compactSource.level().things.resize(64);
        compactSource.level().things[0].x = 1.0;
        compactSource.pushThingSubsetUndo(
            "compact thing subset",
            sed::editor::DocumentDirty::Things,
            { 0 });
        compactSource.level().things[0].x = 3.0;
        compactSource.pushThingTailUndo(
            "compact thing tail",
            sed::editor::DocumentDirty::Structure
                | sed::editor::DocumentDirty::Things);
        sed::Thing& appendedThing = compactSource.level().newThing();
        appendedThing.name = "appended";
        compactSource.selection().things = { 64 };
        const bool compactTailTouched = compactSource.touchLatestUndoRecord();
        compactSource.pushSurfaceSubsetUndo(
            "compact surface edit",
            sed::editor::DocumentDirty::LevelGeometry,
            { 0 });
        compactSource.level().sectors[0].surfaces[0].texVertices[0].u = 80.0f;
        compactCurrent = compactSource.level();
        sed::editor::DocumentRecoveryHistory compactHistory{};
        compactHistory.compact = std::make_shared<
            sed::editor::DocumentUndoHistory>(compactSource.current());
        compactSource.visitRecoveryHistoryTransitions(
            [&compactHistory](
                const sed::editor::DocumentRecoveryHistoryStack stack,
                const std::string_view label,
                const std::uint32_t dirtyFlags,
                const sed::editor::DocumentHistoryTransitionDomain,
                const sed::assets::SedHistoryTransitionToken& transition,
                sed::editor::DocumentState& state)
            {
                std::optional<std::vector<std::int32_t>> sectors{};
                if (transition.sectorSubset())
                {
                    sectors.emplace(
                        transition.sectorIndices().begin(),
                        transition.sectorIndices().end());
                }
                std::optional<std::vector<std::int32_t>> things{};
                if ( transition.thingSubset() )
                {
                    things.emplace(
                        transition.thingIndices().begin(),
                        transition.thingIndices().end());
                }
                std::optional<std::pair<std::size_t, std::size_t>> thingTail{};
                if ( transition.thingTail() )
                {
                    thingTail.emplace(
                        transition.thingTailBaseSize(),
                        transition.thingTailAffectedCount());
                }
                compactHistory.compact->appendCompactRecoveryState(
                    stack,
                    std::string(label),
                    dirtyFlags,
                    state.level,
                    state.selection,
                    std::move(sectors),
                    std::move(things),
                    std::move(thingTail),
                    transition.navigationSelection()
                        ? std::optional<sed::editor::Selection>{
                            *transition.navigationSelection() }
                        : std::nullopt,
                    transition.surfaceSubset());
                return true;
            });
        compactHistory.compact->finishCompactRecoveryImport();
        const sed::editor::DocumentUndoSnapshotStats beforeCompactStats =
            compactHistory.compact->snapshotStats();
        const sed::editor::DocumentUvConversionResult compactConverted =
            sed::editor::convertDocumentUvCoordinateSpace(
                compactCurrent,
                compactHistory,
                sed::UvCoordinateSpace::Normalized,
                dimensions);
        const sed::editor::DocumentUndoSnapshotStats afterCompactStats =
            compactHistory.compact->snapshotStats();
        sed::editor::Document compactRestored({ compactCurrent, {} });
        compactRestored.restoreRecoveryHistory(
            compactRestored.current(), std::move(compactHistory));
        const bool compactSurfaceUndoConverted = compactRestored.undo()
            && compactRestored.level().uvCoordinateSpace
                == sed::UvCoordinateSpace::Normalized
            && close(compactRestored.level().sectors[0]
                    .surfaces[0].texVertices[0].u,
                48.0 / 128.0);
        const bool compactTailUndoPreserved = compactRestored.undo()
            && compactRestored.level().things.size() == 64;
        const bool compactSubsetUndoPreserved = compactRestored.undo()
            && close(compactRestored.level().things[0].x, 1.0);
        const bool compactSubsetRedoPreserved = compactRestored.redo()
            && close(compactRestored.level().things[0].x, 3.0);
        const bool compactTailRedoPreserved = compactRestored.redo()
            && compactRestored.level().things.size() == 65
            && compactRestored.level().things.back().name == "appended"
            && compactRestored.selection().things
                == std::unordered_set<std::int32_t>{ 64 };
        const bool compactSurfaceRedoPreserved = compactRestored.redo()
            && close(compactRestored.level().sectors[0]
                    .surfaces[0].texVertices[0].u,
                80.0 / 128.0);
        const auto compactStatsPreserved = [](const auto& stats)
        {
            return stats.full == 0
                && stats.things == 0
                && stats.thingSubsets == 1
                && stats.thingTails == 1
                && stats.surfaceSubsets == 1;
        };
        check(compactTailTouched
                && compactStatsPreserved(beforeCompactStats)
                && compactConverted
                && compactStatsPreserved(afterCompactStats)
                && compactSurfaceUndoConverted
                && compactTailUndoPreserved
                && compactSubsetUndoPreserved
                && compactSubsetRedoPreserved
                && compactTailRedoPreserved
                && compactSurfaceRedoPreserved,
            "document_uv_promotion retains bounded Thing subset, mixed Structure/Things tail, and geometry snapshots with exact undo/redo semantics");
    }

    {
        sed::Level level{};
        level.ppunit = 160.0;
        check(close(sed::level::levelPixelPerUnit(level), 160.0),
            "levelPixelPerUnit keeps a finite positive authored value");

        const std::array invalidPixelsPerUnit{
            0.0,
            -1.0,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(),
            -std::numeric_limits<double>::infinity(),
        };
        bool allInvalidValuesUseDefault = true;
        for (const double value : invalidPixelsPerUnit)
        {
            level.ppunit = value;
            allInvalidValuesUseDefault = allInvalidValuesUseDefault
                && close(sed::level::levelPixelPerUnit(level), 320.0);
        }
        check(allInvalidValuesUseDefault,
            "levelPixelPerUnit maps every non-positive and non-finite value to the classic default");
    }

    {
        const sed::TexVertex pixels{
            -32.0f, 112.0f, sed::ColorF{ 0.1f, 0.2f, 0.3f, 0.4f } };
        const auto normalized = sed::level::convertUvCoordinate(
            pixels,
            sed::UvCoordinateSpace::TexturePixels,
            sed::UvCoordinateSpace::Normalized,
            sed::level::UvTextureDimensions{ 128.0, 64.0 });
        const auto restored = normalized
            ? sed::level::convertUvCoordinate(
                *normalized,
                sed::UvCoordinateSpace::Normalized,
                sed::UvCoordinateSpace::TexturePixels,
                sed::level::UvTextureDimensions{ 128.0, 64.0 })
            : std::nullopt;
        check(normalized.has_value()
                && close(normalized->u, -0.25f)
                && close(normalized->v, 1.75f)
                && normalized->color == pixels.color,
            "UV conversion: signed multi-tile pixel values normalize per axis and preserve color");
        check(restored.has_value()
                && close(restored->u, pixels.u)
                && close(restored->v, pixels.v)
                && restored->color == pixels.color,
            "UV conversion: normalized values restore exact pixel coordinates");

        check(!sed::level::convertUvCoordinate(
                pixels,
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                sed::level::UvTextureDimensions{ 0.0, 64.0 }),
            "UV conversion: zero-width texture dimensions reject");
        check(!sed::level::convertUvCoordinate(
                pixels,
                static_cast<sed::UvCoordinateSpace>(99),
                sed::UvCoordinateSpace::Normalized,
                sed::level::UvTextureDimensions{ 128.0, 64.0 }),
            "UV conversion: unknown source coordinate space rejects");

        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();
        const double dimensionNan =
            std::numeric_limits<double>::quiet_NaN();
        const double dimensionInfinity =
            std::numeric_limits<double>::infinity();
        const double dimensionMinimum =
            std::numeric_limits<double>::min();
        const float maximum = std::numeric_limits<float>::max();
        struct InvalidCoordinateCase
        {
            const char* name{};
            sed::TexVertex coordinate{};
            sed::UvCoordinateSpace source{};
            sed::UvCoordinateSpace destination{};
            sed::level::UvTextureDimensions dimensions{};
        };
        const std::array invalidCases{
            InvalidCoordinateCase{
                "unknown destination", pixels,
                sed::UvCoordinateSpace::TexturePixels,
                static_cast<sed::UvCoordinateSpace>(99),
                { 128.0, 64.0 } },
            InvalidCoordinateCase{
                "NaN U", { nan, 1.0f, sed::ColorRed },
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                { 128.0, 64.0 } },
            InvalidCoordinateCase{
                "positive-infinite U", { infinity, 1.0f, sed::ColorRed },
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                { 128.0, 64.0 } },
            InvalidCoordinateCase{
                "negative-infinite U", { -infinity, 1.0f, sed::ColorRed },
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                { 128.0, 64.0 } },
            InvalidCoordinateCase{
                "NaN V", { 1.0f, nan, sed::ColorRed },
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                { 128.0, 64.0 } },
            InvalidCoordinateCase{
                "positive-infinite V", { 1.0f, infinity, sed::ColorRed },
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                { 128.0, 64.0 } },
            InvalidCoordinateCase{
                "negative-infinite V", { 1.0f, -infinity, sed::ColorRed },
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                { 128.0, 64.0 } },
            InvalidCoordinateCase{
                "zero width", pixels,
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                { 0.0, 64.0 } },
            InvalidCoordinateCase{
                "negative width", pixels,
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                { -128.0, 64.0 } },
            InvalidCoordinateCase{
                "NaN width", pixels,
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                { dimensionNan, 64.0 } },
            InvalidCoordinateCase{
                "infinite width", pixels,
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                { dimensionInfinity, 64.0 } },
            InvalidCoordinateCase{
                "zero height", pixels,
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                { 128.0, 0.0 } },
            InvalidCoordinateCase{
                "negative height", pixels,
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                { 128.0, -64.0 } },
            InvalidCoordinateCase{
                "NaN height", pixels,
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                { 128.0, dimensionNan } },
            InvalidCoordinateCase{
                "infinite height", pixels,
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                { 128.0, dimensionInfinity } },
            InvalidCoordinateCase{
                "positive U overflow", { maximum, 1.0f, sed::ColorRed },
                sed::UvCoordinateSpace::Normalized,
                sed::UvCoordinateSpace::TexturePixels,
                { 2.0, 2.0 } },
            InvalidCoordinateCase{
                "negative U overflow", { -maximum, 1.0f, sed::ColorRed },
                sed::UvCoordinateSpace::Normalized,
                sed::UvCoordinateSpace::TexturePixels,
                { 2.0, 2.0 } },
            InvalidCoordinateCase{
                "positive V overflow", { 1.0f, maximum, sed::ColorRed },
                sed::UvCoordinateSpace::Normalized,
                sed::UvCoordinateSpace::TexturePixels,
                { 2.0, 2.0 } },
            InvalidCoordinateCase{
                "negative V overflow", { 1.0f, -maximum, sed::ColorRed },
                sed::UvCoordinateSpace::Normalized,
                sed::UvCoordinateSpace::TexturePixels,
                { 2.0, 2.0 } },
            InvalidCoordinateCase{
                "U division overflow", { 1.0f, 1.0f, sed::ColorRed },
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                { dimensionMinimum, 1.0 } },
            InvalidCoordinateCase{
                "V division overflow", { 1.0f, 1.0f, sed::ColorRed },
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                { 1.0, dimensionMinimum } },
        };

        for (const InvalidCoordinateCase& testCase : invalidCases)
        {
            const std::optional<sed::TexVertex> result =
                sed::level::convertUvCoordinate(
                    testCase.coordinate,
                    testCase.source,
                    testCase.destination,
                    testCase.dimensions);
            const std::string message =
                "UV conversion malformed matrix rejects "
                + std::string(testCase.name);
            check(!result, message.c_str());
        }

        const sed::TexVertex sameSpace{
            -0.0f,
            4.25f,
            sed::ColorF{ -0.0f, 0.25f, 0.75f, 1.0f } };
        const std::optional<sed::TexVertex> sameSpaceResult =
            sed::level::convertUvCoordinate(
                sameSpace,
                sed::UvCoordinateSpace::Normalized,
                sed::UvCoordinateSpace::Normalized,
                sed::level::UvTextureDimensions{
                    dimensionNan, -dimensionInfinity });
        check(sameSpaceResult
                && sameTexVertexBits(*sameSpaceResult, sameSpace),
            "UV conversion: a finite same-space value is a bit-exact no-op without valid texture dimensions");

        check(!sed::level::convertUvCoordinate(
                sed::TexVertex{ nan, 1.0f, sed::ColorWhite },
                sed::UvCoordinateSpace::Normalized,
                sed::UvCoordinateSpace::Normalized,
                sed::level::UvTextureDimensions{ 1.0, 1.0 }),
            "UV conversion: same-space no-op still rejects non-finite coordinates");
    }

    {
        enum class FailureKind
        {
            invalidSource,
            invalidDestination,
            invalidDimensions,
            nonFiniteUScale,
            nonFiniteVScale,
            positiveUScaleOverflow,
            negativeVScaleOverflow,
            uScaleDivisionOverflow,
            nonFiniteUCoordinate,
            nonFiniteVCoordinate,
            positiveUCoordinateOverflow,
            negativeVCoordinateOverflow,
            uCoordinateDivisionOverflow,
        };
        struct FailureCase
        {
            const char* name{};
            FailureKind kind{};
            std::string_view expectedError{};
            std::optional<sed::SurfaceRef> expectedSurface{};
            std::size_t expectedResolverCalls{};
        };
        const std::array failureCases{
            FailureCase{
                "invalid source tag",
                FailureKind::invalidSource,
                "unknown UV coordinate space",
                std::nullopt,
                0u },
            FailureCase{
                "invalid destination tag",
                FailureKind::invalidDestination,
                "unknown UV coordinate space",
                std::nullopt,
                0u },
            FailureCase{
                "invalid later dimensions",
                FailureKind::invalidDimensions,
                "dimensions are missing or invalid",
                sed::SurfaceRef{ 0, 1 },
                2u },
            FailureCase{
                "non-finite U scale",
                FailureKind::nonFiniteUScale,
                "UV scale cannot be represented",
                sed::SurfaceRef{ 0, 1 },
                2u },
            FailureCase{
                "non-finite V scale",
                FailureKind::nonFiniteVScale,
                "UV scale cannot be represented",
                sed::SurfaceRef{ 0, 1 },
                2u },
            FailureCase{
                "positive U scale overflow",
                FailureKind::positiveUScaleOverflow,
                "UV scale cannot be represented",
                sed::SurfaceRef{ 0, 1 },
                2u },
            FailureCase{
                "negative V scale overflow",
                FailureKind::negativeVScaleOverflow,
                "UV scale cannot be represented",
                sed::SurfaceRef{ 0, 1 },
                2u },
            FailureCase{
                "U scale division overflow",
                FailureKind::uScaleDivisionOverflow,
                "UV scale cannot be represented",
                sed::SurfaceRef{ 0, 1 },
                2u },
            FailureCase{
                "non-finite U coordinate",
                FailureKind::nonFiniteUCoordinate,
                "texture coordinate cannot be represented",
                sed::SurfaceRef{ 0, 1 },
                2u },
            FailureCase{
                "non-finite V coordinate",
                FailureKind::nonFiniteVCoordinate,
                "texture coordinate cannot be represented",
                sed::SurfaceRef{ 0, 1 },
                2u },
            FailureCase{
                "positive U coordinate overflow",
                FailureKind::positiveUCoordinateOverflow,
                "texture coordinate cannot be represented",
                sed::SurfaceRef{ 0, 1 },
                2u },
            FailureCase{
                "negative V coordinate overflow",
                FailureKind::negativeVCoordinateOverflow,
                "texture coordinate cannot be represented",
                sed::SurfaceRef{ 0, 1 },
                2u },
            FailureCase{
                "U coordinate division overflow",
                FailureKind::uCoordinateDivisionOverflow,
                "texture coordinate cannot be represented",
                sed::SurfaceRef{ 0, 1 },
                2u },
        };

        for (const FailureCase& testCase : failureCases)
        {
            sed::Level level{};
            level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
            level.sectors.resize(1);
            level.sectors[0].surfaces.resize(2);
            level.sectors[0].surfaces[0].material = "first.mat";
            level.sectors[0].surfaces[0].uScale = -0.375f;
            level.sectors[0].surfaces[0].vScale = 2.625f;
            level.sectors[0].surfaces[0].texVertices = {
                { -1.25f, 2.5f,
                    sed::ColorF{ -0.0f, 0.25f, 0.5f, 0.75f } },
            };
            level.sectors[0].surfaces[0].vertices = { 0 };
            level.sectors[0].surfaces[1].material = "second.mat";
            level.sectors[0].surfaces[1].uScale = 0.5f;
            level.sectors[0].surfaces[1].vScale = -0.75f;
            level.sectors[0].surfaces[1].texVertices = {
                { 3.25f, -4.5f,
                    sed::ColorF{ 0.875f, -0.0f, 0.125f, 1.0f } },
            };
            level.sectors[0].surfaces[1].vertices = { 0 };

            sed::UvCoordinateSpace destination =
                sed::UvCoordinateSpace::TexturePixels;
            switch (testCase.kind)
            {
            case FailureKind::invalidSource:
                level.uvCoordinateSpace =
                    static_cast<sed::UvCoordinateSpace>(99);
                break;
            case FailureKind::invalidDestination:
                destination = static_cast<sed::UvCoordinateSpace>(99);
                break;
            case FailureKind::invalidDimensions:
                break;
            case FailureKind::nonFiniteUScale:
                level.sectors[0].surfaces[1].uScale =
                    std::numeric_limits<float>::quiet_NaN();
                break;
            case FailureKind::nonFiniteVScale:
                level.sectors[0].surfaces[1].vScale =
                    std::numeric_limits<float>::infinity();
                break;
            case FailureKind::positiveUScaleOverflow:
                level.sectors[0].surfaces[1].uScale =
                    std::numeric_limits<float>::max();
                break;
            case FailureKind::negativeVScaleOverflow:
                level.sectors[0].surfaces[1].vScale =
                    -std::numeric_limits<float>::max();
                break;
            case FailureKind::uScaleDivisionOverflow:
                level.uvCoordinateSpace =
                    sed::UvCoordinateSpace::TexturePixels;
                destination = sed::UvCoordinateSpace::Normalized;
                break;
            case FailureKind::nonFiniteUCoordinate:
                level.sectors[0].surfaces[1].texVertices[0].u =
                    std::numeric_limits<float>::quiet_NaN();
                break;
            case FailureKind::nonFiniteVCoordinate:
                level.sectors[0].surfaces[1].texVertices[0].v =
                    std::numeric_limits<float>::infinity();
                break;
            case FailureKind::positiveUCoordinateOverflow:
                level.sectors[0].surfaces[1].texVertices[0].u =
                    std::numeric_limits<float>::max();
                break;
            case FailureKind::negativeVCoordinateOverflow:
                level.sectors[0].surfaces[1].texVertices[0].v =
                    -std::numeric_limits<float>::max();
                break;
            case FailureKind::uCoordinateDivisionOverflow:
                level.uvCoordinateSpace =
                    sed::UvCoordinateSpace::TexturePixels;
                destination = sed::UvCoordinateSpace::Normalized;
                level.sectors[0].surfaces[1].uScale = 0.0f;
                level.sectors[0].surfaces[1].texVertices[0].u = 1.0f;
                break;
            }

            const sed::Level baseline = level;
            std::size_t resolverCalls = 0;
            const sed::level::LevelUvConversionResult result =
                sed::level::convertLevelUvCoordinateSpace(
                    level,
                    destination,
                    [&](const sed::Surface& surface)
                        -> std::optional<sed::level::UvTextureDimensions>
                    {
                        ++resolverCalls;
                        if (testCase.kind == FailureKind::invalidDimensions
                            && surface.material == "second.mat")
                        {
                            return sed::level::UvTextureDimensions{
                                -2.0, 3.0 };
                        }
                        if ((testCase.kind
                                    == FailureKind::uScaleDivisionOverflow
                                || testCase.kind
                                    == FailureKind::uCoordinateDivisionOverflow)
                            && surface.material == "second.mat")
                        {
                            return sed::level::UvTextureDimensions{
                                std::numeric_limits<double>::min(),
                                3.0 };
                        }
                        return sed::level::UvTextureDimensions{ 2.0, 3.0 };
                    });

            const bool exactFailure = !result
                && result.failedSurface == testCase.expectedSurface
                && result.error.find(testCase.expectedError)
                    != std::string::npos
                && resolverCalls == testCase.expectedResolverCalls
                && sameLevelUvBits(level, baseline);
            const std::string message =
                "Level UV conversion failure matrix is exact and atomic for "
                + std::string(testCase.name);
            check(exactFailure, message.c_str());
        }

        sed::Level noResolverLevel{};
        noResolverLevel.uvCoordinateSpace =
            sed::UvCoordinateSpace::TexturePixels;
        noResolverLevel.sectors.resize(1);
        noResolverLevel.sectors[0].surfaces.resize(1);
        noResolverLevel.sectors[0].surfaces[0].material = "no-resolver.mat";
        noResolverLevel.sectors[0].surfaces[0].uScale = -0.0f;
        noResolverLevel.sectors[0].surfaces[0].vScale = 2.0f;
        noResolverLevel.sectors[0].surfaces[0].texVertices = {
            { -4.0f, 8.0f,
                sed::ColorF{ -0.0f, 0.25f, 0.5f, 0.75f } },
        };
        noResolverLevel.sectors[0].surfaces[0].vertices = { 0 };
        const sed::Level noResolverBaseline = noResolverLevel;
        const sed::level::UvTextureDimensionsResolver noResolver{};
        const sed::level::LevelUvConversionResult noResolverResult =
            sed::level::convertLevelUvCoordinateSpace(
                noResolverLevel,
                sed::UvCoordinateSpace::Normalized,
                noResolver);
        check(!noResolverResult
                && noResolverResult.failedSurface
                    == sed::SurfaceRef{ 0, 0 }
                && noResolverResult.error.find("dimensions")
                    != std::string::npos
                && sameLevelUvBits(
                    noResolverLevel, noResolverBaseline),
            "Level UV conversion: an empty resolver reports the first exact surface and preserves every UV bit");

        for (const std::string_view material : { "", "  ", "NoNe" })
        {
            sed::Level candidate{};
            candidate.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;
            candidate.sectors.resize(1);
            candidate.sectors[0].vertices = {
                { 0.0, 0.0, 0.0 },
                { 1.0, 0.0, 0.0 },
                { 0.0, 1.0, 0.0 },
            };
            sed::Surface& surface = candidate.sectors[0].surfaces.emplace_back();
            surface.material = std::string(material);
            surface.nMat = 99;
            surface.vertices = { 0, 1, 2 };
            surface.texVertices = {
                { -4.0f, 8.0f, sed::ColorRed },
                { 1.5f, -2.0f, sed::ColorGreen },
                { 3.0f, 6.0f, sed::ColorBlue },
            };
            surface.uScale = -0.0f;
            surface.vScale = 2.0f;
            const sed::Level original = candidate;
            std::size_t resolverCalls = 0;
            const auto resolver = [&resolverCalls](const sed::Surface&)
                -> std::optional<sed::level::UvTextureDimensions>
            {
                ++resolverCalls;
                return sed::level::UvTextureDimensions{ 2.0, 3.0 };
            };
            const sed::level::LevelUvConversionResult forward =
                sed::level::convertLevelUvCoordinateSpace(
                    candidate,
                    sed::UvCoordinateSpace::Normalized,
                    resolver);
            sed::Level normalizedExpected = original;
            normalizedExpected.uvCoordinateSpace =
                sed::UvCoordinateSpace::Normalized;
            const bool forwardExact = forward
                && !forward.failedSurface
                && forward.error.empty()
                && candidate.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && sameLevelUvBits(candidate, normalizedExpected);
            const sed::level::LevelUvConversionResult reverse =
                sed::level::convertLevelUvCoordinateSpace(
                    candidate,
                    sed::UvCoordinateSpace::TexturePixels,
                    resolver);
            check(forwardExact
                    && reverse && !reverse.failedSurface
                    && reverse.error.empty()
                    && resolverCalls == 0u
                    && sameLevelUvBits(candidate, original),
                (std::string("Level UV no-material passthrough round-trips '")
                    + std::string(material) + "'" ).c_str());
        }

        sed::Sector noMaterialSector{};
        sed::Surface& noMaterialSurface = noMaterialSector.surfaces.emplace_back();
        noMaterialSurface.material = " NoNe ";
        noMaterialSurface.nMat = 99;
        noMaterialSurface.vertices = { 0 };
        noMaterialSurface.texVertices = {
            { 7.0f, -3.0f, sed::ColorYellow },
        };
        noMaterialSurface.uScale = 1.25f;
        noMaterialSurface.vScale = -0.75f;
        const sed::Sector originalNoMaterialSector = noMaterialSector;
        std::size_t sectorResolverCalls = 0;
        const auto sectorResolver = [&sectorResolverCalls](const sed::Surface&)
            -> std::optional<sed::level::UvTextureDimensions>
        {
            ++sectorResolverCalls;
            return sed::level::UvTextureDimensions{ 2.0, 3.0 };
        };
        const sed::level::LevelUvConversionResult sectorForward =
            sed::level::convertSectorUvCoordinateSpace(
                noMaterialSector,
                4,
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                sectorResolver);
        const bool sectorForwardExact = sectorForward
            && !sectorForward.failedSurface
            && sectorForward.error.empty()
            && sameSectorUvBits(noMaterialSector, originalNoMaterialSector);
        const sed::level::LevelUvConversionResult sectorReverse =
            sed::level::convertSectorUvCoordinateSpace(
                noMaterialSector,
                4,
                sed::UvCoordinateSpace::Normalized,
                sed::UvCoordinateSpace::TexturePixels,
                sectorResolver);
        check(sectorForwardExact
                && sectorReverse && !sectorReverse.failedSurface
                && sectorReverse.error.empty()
                && sectorResolverCalls == 0u
                && sameSectorUvBits(noMaterialSector, originalNoMaterialSector),
            "Sector UV no-material passthrough round-trips without resolver lookup");

        for (const std::string_view failure : {
                 "count",
                 "coordinate",
                 "scale" })
        {
            sed::Level malformed{};
            malformed.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;
            malformed.sectors.resize(1);
            sed::Surface& surface = malformed.sectors[0].surfaces.emplace_back();
            surface.material = " none ";
            surface.nMat = 99;
            surface.vertices = { 0, 1, 2 };
            surface.texVertices = {
                { 1.0f, 2.0f, sed::ColorWhite },
                { 3.0f, 4.0f, sed::ColorWhite },
                { 5.0f, 6.0f, sed::ColorWhite },
            };
            surface.uScale = 1.0f;
            surface.vScale = 1.0f;
            if (failure == "count")
            {
                surface.texVertices.pop_back();
            }
            else if (failure == "coordinate")
            {
                surface.texVertices[1].u = std::numeric_limits<float>::quiet_NaN();
            }
            else
            {
                surface.uScale = std::numeric_limits<float>::infinity();
            }
            const sed::Level baseline = malformed;
            const sed::level::LevelUvConversionResult result =
                sed::level::convertLevelUvCoordinateSpace(
                    malformed,
                    sed::UvCoordinateSpace::Normalized,
                    {});
            const std::string expected = failure == "count"
                ? "Texture-coordinate count does not match the polygon corner count."
                : failure == "coordinate"
                    ? "A texture coordinate cannot be represented in the destination space."
                    : "A UV scale cannot be represented in the destination space.";
            check(!result
                    && result.failedSurface == sed::SurfaceRef{ 0, 0 }
                    && result.error == expected
                    && sameLevelUvBits(malformed, baseline),
                (std::string("Level UV no-material exact failure: ")
                    + std::string(failure)).c_str());
        }

        sed::Level emptyLevel{};
        emptyLevel.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;
        emptyLevel.sectors.clear();
        std::size_t emptyLevelResolverCalls = 0;
        const sed::level::LevelUvConversionResult emptyLevelResult =
            sed::level::convertLevelUvCoordinateSpace(
                emptyLevel,
                sed::UvCoordinateSpace::Normalized,
                [&emptyLevelResolverCalls](const sed::Surface&)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    ++emptyLevelResolverCalls;
                    return std::nullopt;
                });
        check(emptyLevelResult
                && emptyLevelResolverCalls == 0u
                && emptyLevel.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized,
            "Level UV conversion: an empty Level changes only its coordinate-space tag without resource lookup");

        sed::Sector emptySector{};
        std::size_t emptySectorResolverCalls = 0;
        const sed::level::LevelUvConversionResult emptySectorResult =
            sed::level::convertSectorUvCoordinateSpace(
                emptySector,
                7,
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                [&emptySectorResolverCalls](const sed::Surface&)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    ++emptySectorResolverCalls;
                    return std::nullopt;
                });
        check(emptySectorResult && emptySectorResolverCalls == 0u,
            "Sector UV conversion: an empty Sector succeeds without resource lookup");
    }

    {
        sed::Level level{};
        level.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;
        level.sectors.resize(1);
        level.sectors[0].surfaces.resize(2);
        level.sectors[0].surfaces[0].material = "wide.mat";
        level.sectors[0].surfaces[0].uScale = 64.0f;
        level.sectors[0].surfaces[0].vScale = 32.0f;
        level.sectors[0].surfaces[0].texVertices = {
            { -32.0f, 112.0f, sed::ColorWhite },
            { 320.0f, -16.0f, sed::ColorWhite },
        };
        level.sectors[0].surfaces[0].vertices = { 0, 1 };
        level.sectors[0].surfaces[1].material = "missing.mat";
        level.sectors[0].surfaces[1].uScale = 5.0f;
        level.sectors[0].surfaces[1].vScale = 10.0f;
        level.sectors[0].surfaces[1].texVertices = {
            { 10.0f, 20.0f, sed::ColorWhite },
        };
        level.sectors[0].surfaces[1].vertices = { 0 };
        const sed::Level baseline = level;

        const sed::level::LevelUvConversionResult missingResult =
            sed::level::convertLevelUvCoordinateSpace(
                level,
                sed::UvCoordinateSpace::Normalized,
                [](const sed::Surface& surface)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    if (surface.material == "wide.mat")
                    {
                        return sed::level::UvTextureDimensions{ 128.0, 64.0 };
                    }
                    return std::nullopt;
                });
        check(!missingResult
                && missingResult.failedSurface
                    == sed::SurfaceRef{ 0, 1 }
                && level.uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels
                && level.sectors[0].surfaces[0].texVertices[0].u
                    == baseline.sectors[0].surfaces[0].texVertices[0].u
                && level.sectors[0].surfaces[0].uScale
                    == baseline.sectors[0].surfaces[0].uScale
                && level.sectors[0].surfaces[1].texVertices[0].v
                    == baseline.sectors[0].surfaces[1].texVertices[0].v
                && level.sectors[0].surfaces[1].vScale
                    == baseline.sectors[0].surfaces[1].vScale,
            "Level UV conversion: missing MAT dimensions reject coordinates and UV scales atomically with the exact surface");

        const sed::level::LevelUvConversionResult converted =
            sed::level::convertLevelUvCoordinateSpace(
                level,
                sed::UvCoordinateSpace::Normalized,
                [](const sed::Surface& surface)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    return surface.material == "wide.mat"
                        ? std::optional<sed::level::UvTextureDimensions>{
                            { 128.0, 64.0 } }
                        : std::optional<sed::level::UvTextureDimensions>{
                            { 10.0, 20.0 } };
                });
        check(converted
                && level.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && close(level.sectors[0].surfaces[0].texVertices[0].u,
                    -0.25f)
                && close(level.sectors[0].surfaces[0].texVertices[0].v,
                    1.75f)
                && close(level.sectors[0].surfaces[0].uScale, 0.5f)
                && close(level.sectors[0].surfaces[0].vScale, 0.5f)
                && close(level.sectors[0].surfaces[1].texVertices[0].u,
                    1.0f)
                && close(level.sectors[0].surfaces[1].texVertices[0].v,
                    1.0f)
                && close(level.sectors[0].surfaces[1].uScale, 0.5f)
                && close(level.sectors[0].surfaces[1].vScale, 0.5f),
            "Level UV conversion: every surface coordinate and UV scale converts per axis in one transaction");

        const sed::level::LevelUvConversionResult restored =
            sed::level::convertLevelUvCoordinateSpace(
                level,
                sed::UvCoordinateSpace::TexturePixels,
                [](const sed::Surface& surface)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    return surface.material == "wide.mat"
                        ? std::optional<sed::level::UvTextureDimensions>{
                            { 128.0, 64.0 } }
                        : std::optional<sed::level::UvTextureDimensions>{
                            { 10.0, 20.0 } };
                });
        check(restored
                && level.uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels
                && close(level.sectors[0].surfaces[0].uScale, 64.0f)
                && close(level.sectors[0].surfaces[0].vScale, 32.0f)
                && close(level.sectors[0].surfaces[1].uScale, 5.0f)
                && close(level.sectors[0].surfaces[1].vScale, 10.0f),
            "Level UV conversion: normalized UV scales restore their exact pixel-space density");

        const sed::Level pixelBaseline = level;
        std::size_t noOpResolverCalls = 0;
        const sed::level::LevelUvConversionResult noOp =
            sed::level::convertLevelUvCoordinateSpace(
                level,
                sed::UvCoordinateSpace::TexturePixels,
                [&noOpResolverCalls](const sed::Surface&)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    ++noOpResolverCalls;
                    return std::nullopt;
                });
        check(noOp
                && noOpResolverCalls == 0u
                && sameLevelUvBits(level, pixelBaseline),
            "Level UV conversion: matching coordinate space is an exact no-op without resource lookup");
    }

    {
        sed::Level level{};
        level.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;
        level.sectors.resize(1);
        level.sectors[0].surfaces.resize(1);
        sed::Surface& emptySurface = level.sectors[0].surfaces[0];
        emptySurface.material = "empty.mat";
        emptySurface.uScale = 64.0f;
        emptySurface.vScale = -32.0f;

        std::size_t resolverCalls = 0;
        const sed::level::LevelUvConversionResult converted =
            sed::level::convertLevelUvCoordinateSpace(
                level,
                sed::UvCoordinateSpace::Normalized,
                [&resolverCalls](const sed::Surface&)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    ++resolverCalls;
                    return sed::level::UvTextureDimensions{ 128.0, 64.0 };
                });
        check(converted && resolverCalls == 1u
                && level.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && emptySurface.texVertices.empty()
                && close(emptySurface.uScale, 0.5f)
                && close(emptySurface.vScale, -0.5f),
            "Level UV conversion: an empty coordinate array still converts its per-axis UV scales and Level tag");

        const sed::Level normalizedBaseline = level;
        const sed::level::LevelUvConversionResult rejected =
            sed::level::convertLevelUvCoordinateSpace(
                level,
                sed::UvCoordinateSpace::TexturePixels,
                [](const sed::Surface&)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    return std::nullopt;
                });
        check(!rejected
                && rejected.failedSurface == sed::SurfaceRef{ 0, 0 }
                && level.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && level.sectors[0].surfaces[0].texVertices.empty()
                && level.sectors[0].surfaces[0].uScale
                    == normalizedBaseline.sectors[0].surfaces[0].uScale
                && level.sectors[0].surfaces[0].vScale
                    == normalizedBaseline.sectors[0].surfaces[0].vScale,
            "Level UV conversion: missing dimensions leave an empty-UV surface and its scale metadata unchanged atomically");
    }

    {
        struct CardinalityCase
        {
            const char* name{};
            std::size_t textureCoordinateCount{};
        };
        const std::array cardinalityCases{
            CardinalityCase{ "missing corner coordinate", 2u },
            CardinalityCase{ "excess corner coordinate", 4u },
        };

        for (const sed::UvCoordinateSpace source : {
                 sed::UvCoordinateSpace::TexturePixels,
                 sed::UvCoordinateSpace::Normalized })
        {
            for (const bool sameSpace : { false, true })
            {
                const sed::UvCoordinateSpace destination = sameSpace
                    ? source
                    : (source == sed::UvCoordinateSpace::TexturePixels
                        ? sed::UvCoordinateSpace::Normalized
                        : sed::UvCoordinateSpace::TexturePixels);
                for (const CardinalityCase& testCase : cardinalityCases)
                {
                    sed::Level level{};
                    level.uvCoordinateSpace = source;
                    level.sectors.resize(1);
                    level.sectors[0].surfaces.resize(2);
                    for (sed::Surface& surface : level.sectors[0].surfaces)
                    {
                        surface.material = "cardinality.mat";
                        surface.vertices = { 0, 1, 2 };
                        surface.texVertices = {
                            { -0.0f, 2.5f, sed::ColorRed },
                            { 1.25f, -3.5f, sed::ColorGreen },
                            { 4.75f, 8.5f, sed::ColorBlue },
                        };
                        surface.uScale = -0.25f;
                        surface.vScale = 0.75f;
                    }
                    level.sectors[0].surfaces[1].texVertices.resize(
                        testCase.textureCoordinateCount,
                        sed::TexVertex{ 9.0f, -11.0f, sed::ColorWhite });

                    const sed::Level baseline = level;
                    std::size_t levelResolverCalls = 0;
                    const sed::level::LevelUvConversionResult levelResult =
                        sed::level::convertLevelUvCoordinateSpace(
                            level,
                            destination,
                            [&levelResolverCalls](const sed::Surface&)
                                -> std::optional<
                                    sed::level::UvTextureDimensions>
                            {
                                ++levelResolverCalls;
                                return sed::level::UvTextureDimensions{
                                    128.0,
                                    32.0,
                                };
                            });
                    const std::string levelMessage =
                        "Level UV conversion rejects "
                        + std::string(testCase.name)
                        + (sameSpace ? " in same space" : " across spaces");
                    check(!levelResult
                            && levelResult.failedSurface
                                == sed::SurfaceRef{ 0, 1 }
                            && levelResult.error.find("corner count")
                                != std::string::npos
                            && levelResolverCalls == (sameSpace ? 0u : 1u)
                            && sameLevelUvBits(level, baseline),
                        levelMessage.c_str());

                    sed::Sector sector = baseline.sectors[0];
                    const sed::Sector sectorBaseline = sector;
                    std::size_t sectorResolverCalls = 0;
                    const sed::level::LevelUvConversionResult sectorResult =
                        sed::level::convertSectorUvCoordinateSpace(
                            sector,
                            7,
                            source,
                            destination,
                            [&sectorResolverCalls](const sed::Surface&)
                                -> std::optional<
                                    sed::level::UvTextureDimensions>
                            {
                                ++sectorResolverCalls;
                                return sed::level::UvTextureDimensions{
                                    128.0,
                                    32.0,
                                };
                            });
                    const std::string sectorMessage =
                        "Sector UV conversion rejects "
                        + std::string(testCase.name)
                        + (sameSpace ? " in same space" : " across spaces");
                    check(!sectorResult
                            && sectorResult.failedSurface
                                == sed::SurfaceRef{ 7, 1 }
                            && sectorResult.error.find("corner count")
                                != std::string::npos
                            && sectorResolverCalls == (sameSpace ? 0u : 1u)
                            && sameSectorUvBits(sector, sectorBaseline),
                        sectorMessage.c_str());
                }
            }
        }
    }

    {
        sed::Sector sector{};
        sector.surfaces.resize(2);
        sector.surfaces[0].material = "wide.mat";
        sector.surfaces[0].uScale = 64.0f;
        sector.surfaces[0].vScale = 32.0f;
        sector.surfaces[0].texVertices = {
            { -32.0f, 112.0f, sed::ColorWhite },
        };
        sector.surfaces[0].vertices = { 0 };
        sector.surfaces[1].material = "missing.mat";
        sector.surfaces[1].texVertices = {
            { 20.0f, 40.0f, sed::ColorWhite },
        };
        sector.surfaces[1].vertices = { 0 };
        const sed::Sector baseline = sector;

        std::size_t rejectedResolverCalls = 0;
        const sed::level::LevelUvConversionResult rejected =
            sed::level::convertSectorUvCoordinateSpace(
                sector,
                7,
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                [&rejectedResolverCalls](const sed::Surface& surface)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    ++rejectedResolverCalls;
                    if (surface.material == "wide.mat")
                    {
                        return sed::level::UvTextureDimensions{
                            128.0, 64.0 };
                    }
                    return std::nullopt;
                });
        check(!rejected
                && rejected.failedSurface == sed::SurfaceRef{ 7, 1 }
                && rejectedResolverCalls == 2u
                && sameSectorUvBits(sector, baseline),
            "Sector UV conversion: a missing later MAT leaves all generated surfaces unchanged");

        struct TagCase
        {
            const char* name{};
            sed::UvCoordinateSpace source{};
            sed::UvCoordinateSpace destination{};
            bool succeeds{};
        };
        const std::array tagCases{
            TagCase{
                "invalid source",
                static_cast<sed::UvCoordinateSpace>(99),
                sed::UvCoordinateSpace::Normalized,
                false },
            TagCase{
                "invalid destination",
                sed::UvCoordinateSpace::TexturePixels,
                static_cast<sed::UvCoordinateSpace>(99),
                false },
            TagCase{
                "matching coordinate spaces",
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::TexturePixels,
                true },
        };
        for (const TagCase& testCase : tagCases)
        {
            sed::Sector candidate = baseline;
            std::size_t resolverCalls = 0;
            const sed::level::LevelUvConversionResult result =
                sed::level::convertSectorUvCoordinateSpace(
                    candidate,
                    7,
                    testCase.source,
                    testCase.destination,
                    [&resolverCalls](const sed::Surface&)
                        -> std::optional<sed::level::UvTextureDimensions>
                    {
                        ++resolverCalls;
                        return sed::level::UvTextureDimensions{
                            128.0, 64.0 };
                    });
            const bool exactResult = static_cast<bool>(result)
                    == testCase.succeeds
                && !result.failedSurface
                && resolverCalls == 0u
                && sameSectorUvBits(candidate, baseline);
            const std::string message =
                "Sector UV conversion tag matrix performs no lookup and leaves data exact for "
                + std::string(testCase.name);
            check(exactResult, message.c_str());
        }

        const sed::level::LevelUvConversionResult converted =
            sed::level::convertSectorUvCoordinateSpace(
                sector,
                7,
                sed::UvCoordinateSpace::TexturePixels,
                sed::UvCoordinateSpace::Normalized,
                [](const sed::Surface& surface)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    return surface.material == "wide.mat"
                        ? std::optional<sed::level::UvTextureDimensions>{
                            { 128.0, 64.0 } }
                        : std::optional<sed::level::UvTextureDimensions>{
                            { 40.0, 80.0 } };
                });
        check(converted
                && close(sector.surfaces[0].texVertices[0].u, -0.25f)
                && close(sector.surfaces[0].texVertices[0].v, 1.75f)
                && close(sector.surfaces[0].uScale, 0.5f)
                && close(sector.surfaces[0].vScale, 0.5f)
                && close(sector.surfaces[1].texVertices[0].u, 0.5f)
                && close(sector.surfaces[1].texVertices[0].v, 0.5f),
            "Sector UV conversion: generated pixel UVs and scales normalize independently per material axis");
    }

    {
        enum class InvalidSameSpaceValue
        {
            uCoordinate,
            vCoordinate,
            uScale,
            vScale,
        };
        struct SameSpaceFailureCase
        {
            const char* name{};
            InvalidSameSpaceValue value{};
        };
        const std::array failureCases{
            SameSpaceFailureCase{
                "non-finite U coordinate",
                InvalidSameSpaceValue::uCoordinate },
            SameSpaceFailureCase{
                "non-finite V coordinate",
                InvalidSameSpaceValue::vCoordinate },
            SameSpaceFailureCase{
                "non-finite U scale",
                InvalidSameSpaceValue::uScale },
            SameSpaceFailureCase{
                "non-finite V scale",
                InvalidSameSpaceValue::vScale },
        };

        for (const sed::UvCoordinateSpace space : {
                 sed::UvCoordinateSpace::TexturePixels,
                 sed::UvCoordinateSpace::Normalized })
        {
            for (const SameSpaceFailureCase& testCase : failureCases)
            {
                sed::Level level{};
                level.uvCoordinateSpace = space;
                level.sectors.resize(1);
                level.sectors[0].surfaces.resize(2);
                for (sed::Surface& surface : level.sectors[0].surfaces)
                {
                    surface.uScale = 0.25f;
                    surface.vScale = -0.5f;
                    surface.texVertices = {
                        { -1.25f, 2.5f, sed::ColorWhite },
                    };
                    surface.vertices = { 0 };
                }

                sed::Surface& malformed = level.sectors[0].surfaces[1];
                const float infinity =
                    std::numeric_limits<float>::infinity();
                switch (testCase.value)
                {
                case InvalidSameSpaceValue::uCoordinate:
                    malformed.texVertices[0].u = infinity;
                    break;
                case InvalidSameSpaceValue::vCoordinate:
                    malformed.texVertices[0].v = -infinity;
                    break;
                case InvalidSameSpaceValue::uScale:
                    malformed.uScale = infinity;
                    break;
                case InvalidSameSpaceValue::vScale:
                    malformed.vScale = -infinity;
                    break;
                }

                const sed::Level baseline = level;
                std::size_t levelResolverCalls = 0;
                const sed::level::LevelUvConversionResult levelResult =
                    sed::level::convertLevelUvCoordinateSpace(
                        level,
                        space,
                        [&levelResolverCalls](const sed::Surface&)
                            -> std::optional<sed::level::UvTextureDimensions>
                        {
                            ++levelResolverCalls;
                            return std::nullopt;
                        });
                const std::string levelMessage =
                    "Level UV same-space validation rejects atomically in "
                    + std::string(
                        space == sed::UvCoordinateSpace::TexturePixels
                            ? "pixel space: "
                            : "normalized space: ")
                    + testCase.name;
                check(!levelResult
                        && levelResult.failedSurface
                            == sed::SurfaceRef{ 0, 1 }
                        && levelResolverCalls == 0u
                        && sameLevelUvBits(level, baseline),
                    levelMessage.c_str());

                sed::Sector sector = baseline.sectors[0];
                const sed::Sector sectorBaseline = sector;
                std::size_t sectorResolverCalls = 0;
                const sed::level::LevelUvConversionResult sectorResult =
                    sed::level::convertSectorUvCoordinateSpace(
                        sector,
                        7,
                        space,
                        space,
                        [&sectorResolverCalls](const sed::Surface&)
                            -> std::optional<sed::level::UvTextureDimensions>
                        {
                            ++sectorResolverCalls;
                            return std::nullopt;
                        });
                const std::string sectorMessage =
                    "Sector UV same-space validation rejects atomically in "
                    + std::string(
                        space == sed::UvCoordinateSpace::TexturePixels
                            ? "pixel space: "
                            : "normalized space: ")
                    + testCase.name;
                check(!sectorResult
                        && sectorResult.failedSurface
                            == sed::SurfaceRef{ 7, 1 }
                        && sectorResolverCalls == 0u
                        && sameSectorUvBits(sector, sectorBaseline),
                    sectorMessage.c_str());
            }
        }
    }

    {
        sed::Sector sector{};
        sector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        sed::Surface surface{};
        surface.vertices = { 0, 1, 2 };
        surface.texVertices = {
            { 0.0f, 0.0f, sed::ColorWhite },
            { 0.0f, 0.0f, sed::ColorWhite },
            { 0.0f, 0.0f, sed::ColorWhite },
        };
        surface.uScale = 1.0f;
        surface.vScale = 1.0f;
        surface.normal = sed::TVector{ 0.0, 0.0, 1.0 };

        sed::level::surfaceCalcUV(320.0, sector, surface, 0);

        check(close(surface.texVertices[0].u, 0.0f), "calcUV: tv0.u");
        check(close(surface.texVertices[0].v, 0.0f), "calcUV: tv0.v");
        check(close(surface.texVertices[1].u, 320.0f), "calcUV: tv1.u");
        check(close(surface.texVertices[1].v, 0.0f), "calcUV: tv1.v");
        check(close(surface.texVertices[2].u, 0.0f), "calcUV: tv2.u");
        check(close(surface.texVertices[2].v, -320.0f), "calcUV: tv2.v");
    }

    {
        // The low-level arrangement routines are coordinate-space agnostic:
        // normalized UV scale metadata turns the legacy pixels-per-unit
        // factor into normalized texture coordinates without a hidden MAT
        // lookup. Exercise anisotropic dimensions and signed/multi-tile
        // anchors so a later pixel-space assumption cannot pass unnoticed.
        sed::Sector sector{};
        sector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 2.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        sed::Surface surface{};
        surface.vertices = { 0, 1, 2 };
        surface.texVertices = {
            { -0.25f, 1.75f, sed::ColorRed },
            { 0.0f, 0.0f, sed::ColorGreen },
            { 0.0f, 0.0f, sed::ColorBlue },
        };
        surface.uScale = 1.0f / 128.0f;
        surface.vScale = 1.0f / 64.0f;
        surface.normal = sed::TVector{ 0.0, 0.0, 1.0 };

        sed::level::surfaceCalcUV(320.0, sector, surface, 0);

        check(close(surface.texVertices[0].u, -0.25f)
                && close(surface.texVertices[0].v, 1.75f)
                && surface.texVertices[0].color == sed::ColorRed,
            "calcUV.normalized: signed multi-tile anchor and baked color remain exact");
        check(close(surface.texVertices[1].u, 4.75f)
                && close(surface.texVertices[1].v, 1.75f),
            "calcUV.normalized: U arrangement uses normalized anisotropic scale");
        check(close(surface.texVertices[2].u, -0.25f)
                && close(surface.texVertices[2].v, -3.25f),
            "calcUV.normalized: V arrangement uses normalized anisotropic scale");

        surface.uScale = 99.0f;
        surface.vScale = 99.0f;
        check(sed::level::surfaceFindUVScales(320.0, sector, surface),
            "findUVScales.normalized: recovery reports success");
        check(close(surface.uScale, 1.0 / 128.0)
                && close(surface.vScale, 1.0 / 64.0),
            "findUVScales.normalized: recovers per-axis normalized scale from arranged coordinates");
    }

    {
        sed::Sector sector{};
        sector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        sed::Surface surface{};
        surface.vertices = { 0, 1, 2 };
        surface.texVertices = {
            { 0.0f, 0.0f, sed::ColorWhite },
            { 320.0f, 0.0f, sed::ColorWhite },
            { 0.0f, 320.0f, sed::ColorWhite },
        };
        surface.normal = sed::TVector{ 0.0, 0.0, 1.0 };

        sed::TVector un{};
        sed::TVector vn{};
        const bool ok = sed::level::surfaceCalcUVNormals(sector, surface, un, vn);
        check(ok, "calcUVNormals: ok");
        check(closeVec(un, sed::TVector{ 1.0, 0.0, 0.0 }, kRotationEpsilon), "calcUVNormals: un");
        check(closeVec(vn, sed::TVector{ 0.0, 1.0, 0.0 }, kRotationEpsilon), "calcUVNormals: vn");
    }

    {
        sed::Sector sector{};
        sector.vertices = {
            { 5.0, 10.0, 2.0, 0 },
            { 7.0, 10.0, 2.0, 0 },
            { 5.0, 12.0, 4.0, 0 },
        };

        sed::Surface surface{};
        surface.vertices = { 0, 1, 2 };
        surface.texVertices = {
            { 0.0f, 0.0f, sed::ColorWhite },
            { 320.0f, 0.0f, sed::ColorWhite },
            { 0.0f, 320.0f, sed::ColorWhite },
        };
        surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);

        sed::TVector un{};
        sed::TVector vn{};
        const bool ok = sed::level::surfaceCalcUVNormals(sector, surface, un, vn);
        constexpr double kInvSqrt2 = 0.70710678118654752440;
        check(ok, "calcUVNormals.translatedSloped: ok");
        check(closeVec(un, sed::TVector{ 1.0, 0.0, 0.0 }, kRotationEpsilon), "calcUVNormals.translatedSloped: un");
        check(closeVec(vn, sed::TVector{ 0.0, kInvSqrt2, kInvSqrt2 }, kRotationEpsilon), "calcUVNormals.translatedSloped: vn");
    }

    {
        // These helpers are public format-boundary building blocks. They must
        // fail closed even when a caller reaches them before the higher-level
        // Level admission validator.
        sed::Sector sector{};
        sector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        sed::Surface surface{};
        surface.vertices = { 0, 1, 2 };
        surface.texVertices = {
            { 0.0f, 0.0f, sed::ColorRed },
            { 1.0f, 0.0f, sed::ColorGreen },
            { 0.0f, 1.0f, sed::ColorBlue },
        };
        surface.uScale = 7.0f;
        surface.vScale = 8.0f;
        surface.normal = sed::TVector{ 0.0, 0.0, 1.0 };

        surface.texVertices[1].u =
            std::numeric_limits<float>::quiet_NaN();
        sed::TVector uAxis{ 9.0, 8.0, 7.0 };
        sed::TVector vAxis{ 6.0, 5.0, 4.0 };
        check(!sed::level::surfaceCalcUVNormals(
                    sector, surface, uAxis, vAxis)
                && uAxis == sed::TVector::zero
                && vAxis == sed::TVector::zero,
            "calcUVNormals.nonFiniteUv rejects and clears both output axes");

        check(!sed::level::surfaceFindUVScales(320.0, sector, surface)
                && sameFloatBits(surface.uScale, 7.0f)
                && sameFloatBits(surface.vScale, 8.0f),
            "findUVScales.nonFiniteUv preserves both scale values atomically");

        surface.texVertices[1].u = 1.0f;
        check(!sed::level::surfaceFindUVScales(
                  std::numeric_limits<double>::quiet_NaN(), sector, surface)
                && sameFloatBits(surface.uScale, 7.0f)
                && sameFloatBits(surface.vScale, 8.0f),
            "findUVScales.nonFinitePixelsPerUnit preserves both scale values atomically");

        surface.texVertices[1].u =
            std::numeric_limits<float>::max();
        sector.vertices[1].x = 0.02;
        check(!sed::level::surfaceFindUVScales(1.0, sector, surface)
                && sameFloatBits(surface.uScale, 7.0f)
                && sameFloatBits(surface.vScale, 8.0f),
            "findUVScales.finiteDoubleOverflow rejects and preserves both scale values atomically");

        surface.texVertices = {
            { 0.0f, 0.0f, sed::ColorRed },
            { 1.0f, 0.0f, sed::ColorGreen },
            { 0.0f, 1.0f, sed::ColorBlue },
        };
        sector.vertices[1].x = 2.0;
        surface.uScale = 7.0f;
        surface.vScale = 8.0f;
        check(!sed::level::surfaceFindUVScales(
                  std::numeric_limits<double>::max(), sector, surface)
                && sameFloatBits(surface.uScale, 7.0f)
                && sameFloatBits(surface.vScale, 8.0f),
            "findUVScales.projectedDeltaOverflow rejects and preserves both scale values atomically");

        const float maximum = std::numeric_limits<float>::max();
        sector.vertices[1].x = 2.0;
        sector.vertices[2].y = 2.0;
        surface.texVertices = {
            { -maximum, -maximum, sed::ColorRed },
            { maximum, -maximum, sed::ColorGreen },
            { -maximum, maximum, sed::ColorBlue },
        };
        surface.uScale = 7.0f;
        surface.vScale = 8.0f;
        check(sed::level::surfaceFindUVScales(1.0, sector, surface)
                && sameFloatBits(surface.uScale, maximum)
                && sameFloatBits(surface.vScale, maximum),
            "findUVScales.extremeFiniteUvDelta recovers representable scales without overflowing the subtraction");
    }

    {
        sed::Sector sector{};
        sector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        sed::Surface surface{};
        surface.vertices = { 0, 1, 2 };
        surface.texVertices = {
            { 0.0f, 0.0f, sed::ColorWhite },
            { 320.0f, 0.0f, sed::ColorWhite },
            { 0.0f, 320.0f, sed::ColorWhite },
        };
        surface.normal = sed::TVector{ 0.0, 0.0, 1.0 };

        check(!sed::level::surfaceIsTextureFlipped(sector, surface), "isTextureFlipped.false");

        surface.texVertices[2].v = -320.0f;
        check(sed::level::surfaceIsTextureFlipped(sector, surface), "isTextureFlipped.true");

        const float maximum = std::numeric_limits<float>::max();
        surface.texVertices = {
            { maximum, maximum * 0.25f, sed::ColorWhite },
            { 0.0f, 0.0f, sed::ColorWhite },
            { maximum * 0.25f, maximum, sed::ColorWhite },
        };
        check(sed::level::surfaceIsTextureFlipped(sector, surface),
            "isTextureFlipped.extremeFinite evaluates orientation without float-product overflow");

        surface.texVertices.resize(2);
        check(!sed::level::surfaceIsTextureFlipped(sector, surface), "isTextureFlipped.missingTexVertex.false");
    }

    {
        sed::Sector sector{};
        sector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 2.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        sed::Surface surface{};
        surface.vertices = { 0, 1, 2 };
        surface.texVertices = {
            { 0.0f, 0.0f, sed::ColorWhite },
            { 640.0f, 0.0f, sed::ColorWhite },
            { 0.0f, 160.0f, sed::ColorWhite },
        };
        surface.uScale = 99.0f;
        surface.vScale = 99.0f;
        surface.normal = sed::TVector{ 0.0, 0.0, 1.0 };

        check(sed::level::surfaceFindUVScales(320.0, sector, surface),
            "findUVScales.valid succeeds");
        check(close(surface.uScale, 1.0), "findUVScales.uScale");
        check(close(surface.vScale, 0.5), "findUVScales.vScale");

        surface.texVertices.resize(2);
        surface.uScale = 7.0f;
        surface.vScale = 8.0f;
        check(!sed::level::surfaceFindUVScales(320.0, sector, surface),
            "findUVScales.missingTexVertex: incomplete tuples report failure");
        check(close(surface.uScale, 7.0), "findUVScales.missingTexVertex.keepU");
        check(close(surface.vScale, 8.0), "findUVScales.missingTexVertex.keepV");
    }

    {
        sed::Sector sector{};
        sector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        sed::Surface surface{};
        surface.vertices = { 0, 1 };
        surface.texVertices = {
            { 0.0f, 0.0f, sed::ColorWhite },
            { 0.0f, 0.0f, sed::ColorWhite },
        };
        surface.uScale = 1.0f;
        surface.vScale = 1.0f;
        surface.faceFlags = sed::FF_SF_FLIP;

        const sed::TVector un{ 1.0, 0.0, 0.0 };
        const sed::TVector vn{ 0.0, 1.0, 0.0 };

        sed::level::surfaceArrangeTextureBy(320.0, sector, surface, un, vn, 0.0, 0.0, 0.0, 0.0f, 0.0f);
        check(close(surface.texVertices[1].v, -320.0f), "arrangeTextureBy: FF_SF_FLIP flips V");
    }

    {
        sed::Sector sector{};
        sector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        sed::Surface surface{};
        surface.vertices = { 0, 1, 2 };
        surface.texVertices = {
            { -0.25f, 1.75f, sed::ColorRed },
            { 2.0f, 3.0f, sed::ColorGreen },
            { 4.0f, 5.0f, sed::ColorBlue },
        };
        surface.uScale = std::numeric_limits<float>::max();
        surface.vScale = 1.0f;
        const sed::Surface before = surface;

        sed::level::surfaceArrangeTextureBy(
            std::numeric_limits<double>::max(),
            sector,
            surface,
            sed::TVector{ 1.0, 0.0, 0.0 },
            sed::TVector{ 0.0, 1.0, 0.0 },
            0.0,
            0.0,
            0.0,
            -0.25f,
            1.75f);
        check(sameSurfaceUvBits(surface, before),
            "arrangeTextureBy.lateOverflow preserves every UV, color, and scale atomically");
    }

    {
        // Two adjacent faces can start their local vertex loops on different
        // world-space axes. Auto Align must recover each face's authored UV
        // frame instead of forcing U onto local edge zero, or it rotates the
        // wall and breaks the already-continuous shared edge.
        sed::Sector sector{};
        sector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 1.0, 1.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
            { 1.0, 1.0, 1.0, 0 },
            { 1.0, 0.0, 1.0, 0 },
        };

        sed::Surface floor{};
        floor.vertices = { 0, 1, 2, 3 };
        floor.texVertices = {
            { 0.0f, 0.0f, sed::ColorWhite },
            { 1.0f, 0.0f, sed::ColorWhite },
            { 1.0f, -1.0f, sed::ColorWhite },
            { 0.0f, -1.0f, sed::ColorWhite },
        };
        floor.uScale = 1.0f;
        floor.vScale = 1.0f;
        floor.normal = sed::level::surfaceCalcNormalNewell(sector, floor);

        sed::Surface wall{};
        wall.vertices = { 1, 2, 4, 5 };
        wall.texVertices = {
            floor.texVertices[1],
            floor.texVertices[2],
            { 0.0f, -1.0f, sed::ColorWhite },
            { 0.125f, 0.375f, sed::ColorWhite },
        };
        wall.uScale = 1.0f;
        wall.vScale = 1.0f;
        wall.faceFlags = sed::FF_SF_FLIP;
        wall.normal = sed::TVector{ 0.999, 0.001, 0.0 };

        check(sed::level::surfaceAutoAlignUV(1.0, sector, wall),
              "autoAlignUV.normalizedAdjacentDifferentFirstEdge: succeeds");
        check(close(wall.texVertices[0].u, floor.texVertices[1].u)
                  && close(wall.texVertices[0].v, floor.texVertices[1].v)
                  && close(wall.texVertices[1].u, floor.texVertices[2].u)
                  && close(wall.texVertices[1].v, floor.texVertices[2].v),
              "autoAlignUV.normalizedAdjacentDifferentFirstEdge: preserves both shared-edge UV endpoints");
        check(close(wall.texVertices[2].u, 0.0f)
                  && close(wall.texVertices[2].v, -1.0f)
                  && close(wall.texVertices[3].u, 0.0f)
                  && close(wall.texVertices[3].v, 0.0f),
              "autoAlignUV.normalizedAdjacentDifferentFirstEdge: repairs the polygon in its authored UV frame");
        check(closeVec(wall.normal, sed::TVector{ 1.0, 0.0, 0.0 }, kRotationEpsilon),
              "autoAlignUV.normalizedAdjacentDifferentFirstEdge: refreshes the geometric normal before alignment");

        wall.texVertices.pop_back();
        const sed::Surface invalidBefore = wall;
        check(!sed::level::surfaceAutoAlignUV(1.0, sector, wall)
                  && wall.normal.x == invalidBefore.normal.x
                  && wall.normal.y == invalidBefore.normal.y
                  && wall.normal.z == invalidBefore.normal.z
                  && wall.texVertices.size() == invalidBefore.texVertices.size()
                  && wall.texVertices[0].u == invalidBefore.texVertices[0].u
                  && wall.texVertices[0].v == invalidBefore.texVertices[0].v,
              "autoAlignUV.invalid: rejects incomplete UV geometry without synthesizing coordinates");
    }

    {
        constexpr double textureWidth = 128.0;
        constexpr double textureHeight = 32.0;
        const auto makeFixture = []()
        {
            sed::Level level{};
            level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
            level.ppunit = 320.0;
            sed::Sector& sector = level.newSector();
            sector.vertices = {
                { 0.0, 0.0, 0.0, 0 },
                { 2.0, 0.0, 0.0, 0 },
                { 2.0, 1.0, 0.0, 0 },
                { 0.0, 1.0, 0.0, 0 },
            };

            sed::Surface surface{};
            surface.nMat = 0;
            surface.vertices = { 0, 1, 2, 3 };
            surface.texVertices = {
                { -0.25f, 1.75f, sed::ColorRed },
                { 0.0f, 0.0f, sed::ColorGreen },
                { 0.0f, 0.0f, sed::ColorBlue },
                { 0.0f, 0.0f, sed::ColorWhite },
            };
            surface.uScale = 1.0f / 128.0f;
            surface.vScale = 1.0f / 32.0f;
            surface.faceFlags = sed::FF_SF_FLIP;
            surface.normal = sed::TVector{ 0.0, 0.0, 1.0 };
            sector.surfaces.push_back(surface);
            return level;
        };
        const auto dimensions = [](const sed::Surface&)
            -> std::optional<sed::level::UvTextureDimensions>
        {
            return sed::level::UvTextureDimensions{
                textureWidth,
                textureHeight };
        };

        sed::Level normalized = makeFixture();
        sed::Level pixels = normalized;
        const sed::level::LevelUvConversionResult toPixels =
            sed::level::convertLevelUvCoordinateSpace(
                pixels,
                sed::UvCoordinateSpace::TexturePixels,
                dimensions);
        const bool normalizedAligned = sed::level::surfaceAutoAlignUV(
            normalized.ppunit,
            normalized.sectors[0],
            normalized.sectors[0].surfaces[0]);
        const bool pixelsAligned = sed::level::surfaceAutoAlignUV(
            pixels.ppunit,
            pixels.sectors[0],
            pixels.sectors[0].surfaces[0]);
        const sed::level::LevelUvConversionResult backToNormalized =
            sed::level::convertLevelUvCoordinateSpace(
                pixels,
                sed::UvCoordinateSpace::Normalized,
                dimensions);
        check(static_cast<bool>(toPixels)
                && normalizedAligned
                && pixelsAligned
                && static_cast<bool>(backToNormalized)
                && closeSurfaceUv(
                    normalized.sectors[0].surfaces[0],
                    pixels.sectors[0].surfaces[0]),
            "autoAlignUV.crossSpace produces equivalent signed multi-tile UVs, scales, flags, and colors with anisotropic texture dimensions");
    }

    {
        constexpr double textureWidth = 257.0;
        constexpr double textureHeight = 73.0;
        const auto dimensions = [](const sed::Surface&)
            -> std::optional<sed::level::UvTextureDimensions>
        {
            return sed::level::UvTextureDimensions{
                textureWidth,
                textureHeight };
        };

        sed::Level normalized{};
        normalized.uvCoordinateSpace =
            sed::UvCoordinateSpace::Normalized;
        normalized.ppunit = 319.75;
        sed::Sector& sector = normalized.newSector();
        sector.vertices = {
            { 0.137, -0.281, 0.0, 0 },
            { 1.371, 0.064, 0.0, 0 },
            { -0.217, 1.293, 0.0, 0 },
        };

        sed::Surface surface{};
        surface.vertices = { 0, 1, 2 };
        surface.texVertices = {
            { 0.123456f, -0.654321f, sed::ColorRed },
            { 0.742193f, -0.431257f, sed::ColorGreen },
            { 0.071923f, 0.217381f, sed::ColorBlue },
        };
        surface.uScale = 0.731f / static_cast<float>(textureWidth);
        surface.vScale = -1.213f / static_cast<float>(textureHeight);
        surface.normal = sed::TVector{ 0.0, 0.0, 1.0 };
        sector.surfaces.push_back(surface);

        sed::Level pixels = normalized;
        const sed::level::LevelUvConversionResult toPixels =
            sed::level::convertLevelUvCoordinateSpace(
                pixels,
                sed::UvCoordinateSpace::TexturePixels,
                dimensions);

        const sed::TVector uAxis{ 0.6, 0.8, 0.0 };
        const sed::TVector vAxis{ -0.8, 0.6, 0.0 };
        const auto arrange = [&](sed::Level& level)
        {
            sed::Sector& arrangedSector = level.sectors[0];
            sed::Surface& arrangedSurface = arrangedSector.surfaces[0];
            const sed::Vertex& anchor = arrangedSector.vertices[0];
            const sed::TexVertex anchorUv = arrangedSurface.texVertices[0];
            sed::level::surfaceArrangeTextureBy(
                level.ppunit,
                arrangedSector,
                arrangedSurface,
                uAxis,
                vAxis,
                anchor.x,
                anchor.y,
                anchor.z,
                anchorUv.u,
                anchorUv.v);
        };
        arrange(normalized);
        arrange(pixels);
        const sed::level::LevelUvConversionResult backToNormalized =
            sed::level::convertLevelUvCoordinateSpace(
                pixels,
                sed::UvCoordinateSpace::Normalized,
                dimensions);

        check(static_cast<bool>(toPixels)
                && static_cast<bool>(backToNormalized)
                && closeSurfaceUv(
                    normalized.sectors[0].surfaces[0],
                    pixels.sectors[0].surfaces[0]),
            "arrangeTexture.crossSpace keeps non-round geometry and UV reprojection unit-neutral with anisotropic dimensions");
    }

    {
        sed::Level level{};
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        level.ppunit = 1.0;
        (void)level.newSector();
        (void)level.newSector();

        constexpr std::int32_t sourceIndex = 0;
        constexpr std::int32_t targetIndex = 1;

        sed::Sector& sourceSector = level.sectors[sourceIndex];
        sed::Sector& targetSector = level.sectors[targetIndex];

        sourceSector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        targetSector.vertices = sourceSector.vertices;

        sed::Surface sourceSurface{};
        sourceSurface.vertices    = { 0, 1, 2 };
        sourceSurface.texVertices = std::vector<sed::TexVertex>(sourceSurface.vertices.size(), sed::TexVertex{ 0.0f, 0.0f, sed::ColorWhite });
        sourceSurface.uScale      = 2.0f;
        sourceSurface.vScale      = 3.0f;
        sourceSurface.faceFlags   = sed::FF_SF_FLIP;
        sourceSurface.normal      = sed::TVector{ 0.0, 0.0, 1.0 };
        sed::level::surfaceCalcUV(1.0, sourceSector, sourceSurface, 0);
        (void)sourceSector.addSurface(std::move(sourceSurface));

        sed::Surface targetSurface{};
        targetSurface.vertices    = { 0, 1, 2 };
        targetSurface.texVertices = {
            { 0.10f, 0.20f, sed::ColorWhite },
            { 0.30f, 0.40f, sed::ColorWhite },
            { 0.50f, 0.60f, sed::ColorWhite },
        };
        targetSurface.uScale      = 1.0f;
        targetSurface.vScale      = 1.0f;
        targetSurface.faceFlags   = 0u;
        targetSurface.normal      = sed::TVector{ 0.0, 0.0, 1.0 };
        (void)targetSector.addSurface(std::move(targetSurface));

        const bool stitched = sed::level::levelStitchSurfaces(level, sed::SurfaceRef{ sourceIndex, 0 }, sed::SurfaceRef{ targetIndex, 0 });
        check(stitched
                  && level.uvCoordinateSpace
                      == sed::UvCoordinateSpace::Normalized,
              "stitchSurfaces.normalized: succeeds without changing the Level UV coordinate space");

        const sed::Surface& src = level.sectors[sourceIndex].surfaces[0];
        const sed::Surface& dst = level.sectors[targetIndex].surfaces[0];

        check(dst.uScale == src.uScale, "stitchSurfaces: uScale");
        check(dst.vScale == src.vScale, "stitchSurfaces: vScale");
        check((dst.faceFlags & sed::FF_SF_FLIP) == (src.faceFlags & sed::FF_SF_FLIP), "stitchSurfaces: FF_SF_FLIP");

        check(dst.vertices.size() == src.vertices.size(), "stitchSurfaces: vertex count");
        check(dst.texVertices.size() == src.texVertices.size(), "stitchSurfaces: tex vertex count");

        if (dst.texVertices.size() == src.texVertices.size())
        {
            check(close(dst.texVertices[0].u, src.texVertices[0].u), "stitchSurfaces: common U");
            check(close(dst.texVertices[0].v, src.texVertices[0].v), "stitchSurfaces: common V");

            check(!close(dst.texVertices[1].u, 0.30f) || !close(dst.texVertices[1].v, 0.40f),
                  "stitchSurfaces.normalized: recomputes target UVs in normalized units");

            for (const sed::TexVertex& tv : dst.texVertices)
            {
                check(std::isfinite(tv.u) && std::isfinite(tv.v), "stitchSurfaces: finite UV");
            }
        }
    }

    {
        constexpr double textureWidth = 80.0;
        constexpr double textureHeight = 48.0;
        const auto makeFixture = []()
        {
            sed::Level level{};
            level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
            level.ppunit = 320.0;
            (void)level.newSector();
            (void)level.newSector();
            sed::Sector& sourceSector = level.sectors[0];
            sed::Sector& targetSector = level.sectors[1];
            sourceSector.vertices = {
                { 0.0, 0.0, 0.0, 0 },
                { 2.0, 0.0, 0.0, 0 },
                { 0.0, 2.0, 0.0, 0 },
            };
            targetSector.vertices = sourceSector.vertices;

            sed::Surface source{};
            source.nMat = 0;
            source.vertices = { 0, 1, 2 };
            source.texVertices = {
                { -1.25f, 2.5f, sed::ColorRed },
                { 2.75f, 2.0f, sed::ColorGreen },
                { -0.75f, -3.5f, sed::ColorBlue },
            };
            source.uScale = -0.025f;
            source.vScale = 0.0625f;
            source.faceFlags = sed::FF_SF_FLIP;
            source.normal = sed::TVector{ 0.0, 0.0, 1.0 };
            sourceSector.surfaces.push_back(source);

            sed::Surface target{};
            target.nMat = 0;
            target.vertices = { 0, 1, 2 };
            target.texVertices = {
                { 0.125f, 0.25f, sed::ColorWhite },
                { 0.375f, 0.5f, sed::ColorF{ 0.2f, 0.3f, 0.4f, 0.5f } },
                { 0.625f, 0.75f, sed::ColorF{ 0.6f, 0.7f, 0.8f, 0.9f } },
            };
            target.uScale = 1.0f;
            target.vScale = 1.0f;
            target.normal = sed::TVector{ 0.0, 0.0, 1.0 };
            targetSector.surfaces.push_back(target);
            return level;
        };
        const auto dimensions = [](const sed::Surface&)
            -> std::optional<sed::level::UvTextureDimensions>
        {
            return sed::level::UvTextureDimensions{
                textureWidth,
                textureHeight };
        };

        sed::Level normalized = makeFixture();
        sed::Level pixels = normalized;
        const sed::level::LevelUvConversionResult toPixels =
            sed::level::convertLevelUvCoordinateSpace(
                pixels,
                sed::UvCoordinateSpace::TexturePixels,
                dimensions);
        const bool normalizedStitched = sed::level::levelStitchSurfaces(
            normalized,
            sed::SurfaceRef{ 0, 0 },
            sed::SurfaceRef{ 1, 0 });
        const bool pixelsStitched = sed::level::levelStitchSurfaces(
            pixels,
            sed::SurfaceRef{ 0, 0 },
            sed::SurfaceRef{ 1, 0 });
        const sed::level::LevelUvConversionResult backToNormalized =
            sed::level::convertLevelUvCoordinateSpace(
                pixels,
                sed::UvCoordinateSpace::Normalized,
                dimensions);
        check(static_cast<bool>(toPixels),
            "stitchSurfaces.crossSpace converts the normalized fixture to anisotropic pixel units");
        check(normalizedStitched,
            "stitchSurfaces.crossSpace accepts the normalized fixture");
        check(pixelsStitched,
            "stitchSurfaces.crossSpace accepts the equivalent pixel fixture");
        check(static_cast<bool>(backToNormalized)
                && normalized.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized,
            "stitchSurfaces.crossSpace restores the pixel result to the canonical normalized tag");
        check(closeSurfaceUv(
                normalized.sectors[0].surfaces[0],
                pixels.sectors[0].surfaces[0]),
            "stitchSurfaces.crossSpace leaves the source UV state equivalent across anisotropic units");
        check(closeSurfaceUv(
                normalized.sectors[1].surfaces[0],
                pixels.sectors[1].surfaces[0],
                1.0e-4),
            "stitchSurfaces.crossSpace produces equivalent target UVs, scales, flags, and colors for one anisotropic material");
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.sectors.emplace_back();
        sector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 1.0, 1.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
            { 1.0, 0.0, 1.0, 0 },
            { 0.0, 0.0, 1.0, 0 },
            { 10.0, 0.0, 0.0, 0 },
            { 11.0, 0.0, 0.0, 0 },
            { 11.0, 1.0, 0.0, 0 },
            { 10.0, 1.0, 0.0, 0 },
        };
        sector.surfaces.reserve(8);

        const auto sameStitchState = [](const sed::Surface& lhs, const sed::Surface& rhs)
        {
            if (lhs.uScale != rhs.uScale
                || lhs.vScale != rhs.vScale
                || lhs.faceFlags != rhs.faceFlags
                || lhs.texVertices.size() != rhs.texVertices.size())
            {
                return false;
            }

            for (std::size_t i = 0; i < lhs.texVertices.size(); ++i)
            {
                if (lhs.texVertices[i].u != rhs.texVertices[i].u
                    || lhs.texVertices[i].v != rhs.texVertices[i].v)
                {
                    return false;
                }
            }
            return true;
        };

        sed::Surface& source = sector.surfaces.emplace_back();
        source.vertices = { 0, 1, 2, 3 };
        source.texVertices = {
            { 0.0f, 0.0f, sed::ColorWhite },
            { 480.0f, 0.0f, sed::ColorWhite },
            { 480.0f, -240.0f, sed::ColorWhite },
            { 0.0f, -240.0f, sed::ColorWhite },
        };
        source.uScale = 1.5f;
        source.vScale = 0.75f;
        source.faceFlags = sed::FF_SF_FLIP;
        source.normal = sed::TVector{ 0.0, 0.0, 1.0 };

        sed::Surface& perpendicular = sector.surfaces.emplace_back();
        perpendicular.vertices = { 1, 0, 5, 4 };
        perpendicular.texVertices = std::vector<sed::TexVertex>(
            perpendicular.vertices.size(),
            sed::TexVertex{ 11.0f, 22.0f, sed::ColorWhite });
        perpendicular.uScale = 1.0f;
        perpendicular.vScale = 1.0f;
        perpendicular.normal = sed::TVector{ 0.0, 1.0, 0.0 };

        check(sed::level::levelStitchSurfaces(level, { 0, 0 }, { 0, 1 }),
              "stitchSurfaces.nonCoplanar: shared-edge target succeeds");
        const sed::Surface& stitchedPerpendicular = sector.surfaces[1];
        check(close(stitchedPerpendicular.texVertices[0].u, source.texVertices[1].u)
                  && close(stitchedPerpendicular.texVertices[0].v, source.texVertices[1].v)
                  && close(stitchedPerpendicular.texVertices[1].u, source.texVertices[0].u)
                  && close(stitchedPerpendicular.texVertices[1].v, source.texVertices[0].v),
              "stitchSurfaces.nonCoplanar: both shared-edge endpoints remain continuous");
        check(stitchedPerpendicular.uScale == source.uScale
                  && stitchedPerpendicular.vScale == source.vScale
                  && (stitchedPerpendicular.faceFlags & sed::FF_SF_FLIP) != 0,
              "stitchSurfaces.nonCoplanar: source scale and flip propagate");
        for (const sed::TexVertex& tv : stitchedPerpendicular.texVertices)
        {
            check(std::isfinite(tv.u) && std::isfinite(tv.v),
                  "stitchSurfaces.nonCoplanar: projected UV remains finite");
        }

        sed::Surface& disconnected = sector.surfaces.emplace_back();
        disconnected.vertices = { 6, 7, 8, 9 };
        disconnected.texVertices = {
            { 42.0f, 84.0f, sed::ColorWhite },
            { 1.0f, 2.0f, sed::ColorWhite },
            { 3.0f, 4.0f, sed::ColorWhite },
            { 5.0f, 6.0f, sed::ColorWhite },
        };
        disconnected.uScale = 4.0f;
        disconnected.vScale = 5.0f;
        disconnected.normal = sed::TVector{ 0.0, 0.0, 1.0 };

        check(sed::level::levelStitchSurfaces(level, { 0, 0 }, { 0, 2 }),
              "stitchSurfaces.disconnected: legacy fallback succeeds without a common vertex");
        const sed::Surface& stitchedDisconnected = sector.surfaces[2];
        check(close(stitchedDisconnected.texVertices[0].u, 42.0f)
                  && close(stitchedDisconnected.texVertices[0].v, 84.0f),
              "stitchSurfaces.disconnected: target vertex zero remains the fallback UV anchor");
        check(stitchedDisconnected.uScale == source.uScale
                  && stitchedDisconnected.vScale == source.vScale
                  && (stitchedDisconnected.faceFlags & sed::FF_SF_FLIP) != 0,
              "stitchSurfaces.disconnected: source basis state still propagates");

        sed::Surface malformed = disconnected;
        malformed.vertices[3] = 99;
        malformed.uScale = 7.0f;
        malformed.vScale = 8.0f;
        malformed.faceFlags = 0u;
        const std::vector<sed::TexVertex> malformedUvs = malformed.texVertices;
        sector.surfaces.push_back(malformed);
        check(!sed::level::levelStitchSurfaces(level, { 0, 0 }, { 0, 3 }),
              "stitchSurfaces.invalidTarget: invalid geometry is rejected");
        const sed::Surface& rejectedMalformed = sector.surfaces[3];
        bool malformedUnchanged = rejectedMalformed.texVertices.size() == malformedUvs.size();
        for (std::size_t i = 0; i < rejectedMalformed.texVertices.size() && i < malformedUvs.size(); ++i)
        {
            malformedUnchanged = malformedUnchanged
                && close(rejectedMalformed.texVertices[i].u, malformedUvs[i].u)
                && close(rejectedMalformed.texVertices[i].v, malformedUvs[i].v);
        }
        check(malformedUnchanged
                  && close(rejectedMalformed.uScale, 7.0f)
                  && close(rejectedMalformed.vScale, 8.0f)
                  && rejectedMalformed.faceFlags == 0u,
              "stitchSurfaces.invalidTarget: rejection is atomic");

        sed::Surface mismatched = disconnected;
        mismatched.texVertices.pop_back();
        sector.surfaces.push_back(mismatched);
        const std::vector<sed::TexVertex> mismatchedUvs = sector.surfaces[4].texVertices;
        check(!sed::level::levelStitchSurfaces(level, { 0, 0 }, { 0, 4 })
                  && sector.surfaces[4].texVertices.size() == mismatchedUvs.size(),
              "stitchSurfaces.invalidTarget: mismatched texture-vertex count is rejected unchanged");
        check(!sed::level::levelStitchSurfaces(level, { 0, 0 }, { 0, 0 }),
              "stitchSurfaces.self: self-stitch is a no-op failure");

        const sed::Surface finiteNormalTargetBefore = sector.surfaces[2];
        const sed::TVector sourceNormalBefore = source.normal;
        source.normal.x = std::numeric_limits<double>::quiet_NaN();
        check(!sed::level::levelStitchSurfaces(level, { 0, 0 }, { 0, 2 })
                  && sameStitchState(sector.surfaces[2], finiteNormalTargetBefore),
              "stitchSurfaces.nonFiniteNormal: rejects a non-finite source normal atomically");
        source.normal = sourceNormalBefore;

        sed::Surface nonFinitePosition = disconnected;
        nonFinitePosition.vertices[0] = static_cast<std::int32_t>(sector.vertices.size());
        sector.vertices.push_back(sed::Vertex{
            std::numeric_limits<double>::quiet_NaN(),
            0.0,
            0.0,
            0,
        });
        sector.surfaces.push_back(nonFinitePosition);
        const sed::Surface nonFinitePositionBefore = sector.surfaces[5];
        check(!sed::level::levelStitchSurfaces(level, { 0, 0 }, { 0, 5 })
                  && sameStitchState(sector.surfaces[5], nonFinitePositionBefore),
              "stitchSurfaces.nonFinitePosition: rejects a non-finite target vertex atomically");

        const sed::Surface overflowTargetBefore = sector.surfaces[2];
        const float sourceUScaleBefore = source.uScale;
        source.uScale = std::numeric_limits<float>::max();
        check(!sed::level::levelStitchSurfaces(level, { 0, 0 }, { 0, 2 })
                  && sameStitchState(sector.surfaces[2], overflowTargetBefore),
              "stitchSurfaces.nonFiniteOutput: rejects overflowed candidate UVs before publication");
        source.uScale = sourceUScaleBefore;

        // A saved face normal can be normalized yet contain a small rounded
        // component along the real hinge. Normal-cross-only transport then
        // drifts along an oblique shared edge. The shared edge must remain the
        // authoritative seam for banked surfaces.
        const sed::TexVertex bankedSourceUv0 = source.texVertices[0];
        const sed::TexVertex bankedSourceUv1 = source.texVertices[1];
        const std::int32_t bankedVertex0 = static_cast<std::int32_t>(sector.vertices.size());
        sector.vertices.push_back({ 0.0, -1.0, 1.0, 0 });
        const std::int32_t bankedVertex1 = static_cast<std::int32_t>(sector.vertices.size());
        sector.vertices.push_back({ 1.0, -1.0, 1.0, 0 });

        sed::Surface banked{};
        banked.vertices = { 1, 0, bankedVertex0, bankedVertex1 };
        banked.texVertices = std::vector<sed::TexVertex>(
            banked.vertices.size(),
            sed::TexVertex{ 17.0f, 29.0f, sed::ColorWhite });
        banked.uScale = 1.0f;
        banked.vScale = 1.0f;
        banked.normal = sed::TVector{ 0.002, 0.7071067811865475, 0.7071067811865475 };
        banked.normal.normalize();
        const std::int32_t bankedSurfaceIndex = static_cast<std::int32_t>(sector.surfaces.size());
        sector.surfaces.push_back(std::move(banked));

        check(sed::level::levelStitchSurfaces(level, { 0, 0 }, { 0, bankedSurfaceIndex }),
              "stitchSurfaces.bankedRoundedNormal: shared-edge target succeeds");
        const sed::Surface& stitchedBanked = sector.surfaces[static_cast<std::size_t>(bankedSurfaceIndex)];
        check(close(stitchedBanked.texVertices[0].u, bankedSourceUv1.u)
                  && close(stitchedBanked.texVertices[0].v, bankedSourceUv1.v)
                  && close(stitchedBanked.texVertices[1].u, bankedSourceUv0.u)
                  && close(stitchedBanked.texVertices[1].v, bankedSourceUv0.v),
              "stitchSurfaces.bankedRoundedNormal: both hinge endpoints remain exactly aligned");
    }
}

} // namespace sed::tests
