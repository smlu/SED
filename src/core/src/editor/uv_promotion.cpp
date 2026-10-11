#include "sed/editor/uv_promotion.hpp"

#include <memory>
#include <utility>
#include <vector>

namespace sed::editor
{

namespace
{

std::string materialForFailure(
    const Level& level,
    const std::optional<SurfaceRef> failedSurface)
{
    if ( !failedSurface || failedSurface->sector < 0
        || failedSurface->surface < 0
        || static_cast<std::size_t>(failedSurface->sector)
            >= level.sectors.size() )
    {
        return {};
    }
    const Sector& sector = level.sectors[static_cast<std::size_t>(
        failedSurface->sector)];
    if ( static_cast<std::size_t>(failedSurface->surface)
        >= sector.surfaces.size() )
    {
        return {};
    }
    return sector.surfaces[static_cast<std::size_t>(
        failedSurface->surface)].material;
}

} // namespace

DocumentUvConversionResult convertDocumentUvCoordinateSpace(
    Level& current,
    DocumentRecoveryHistory& history,
    const UvCoordinateSpace destination,
    const level::UvTextureDimensionsResolver& resolveDimensions)
{
    Level convertedCurrent = current;
    const level::LevelUvConversionResult currentResult =
        level::convertLevelUvCoordinateSpace(
            convertedCurrent, destination, resolveDimensions);
    if ( !currentResult )
    {
        return {
            currentResult.failedSurface,
            std::nullopt,
            0,
            materialForFailure(convertedCurrent, currentResult.failedSurface),
            currentResult.error,
        };
    }

    Document source({ current, {} });
    source.restoreRecoveryHistory(source.current(), history);

    DocumentRecoveryHistory convertedHistory{};
    convertedHistory.compact = std::make_shared<DocumentUndoHistory>();
    std::size_t undoIndex{};
    std::size_t redoIndex{};
    DocumentUvConversionResult failure{};
    const bool convertedAll = source.visitRecoveryHistoryTransitions(
        [&](const DocumentRecoveryHistoryStack stack,
            const std::string_view label,
            const std::uint32_t dirtyFlags,
            const DocumentHistoryTransitionDomain,
            const assets::SedHistoryTransitionToken& transition,
            DocumentState& state)
        {
            std::size_t& index = stack == DocumentRecoveryHistoryStack::Undo
                ? undoIndex : redoIndex;
            const level::LevelUvConversionResult stateResult =
                level::convertLevelUvCoordinateSpace(
                    state.level, destination, resolveDimensions);
            if ( !stateResult )
            {
                failure.failedSurface = stateResult.failedSurface;
                failure.failedHistoryStack = stack;
                failure.failedHistoryIndex = index;
                failure.failedMaterial = materialForFailure(
                    state.level, stateResult.failedSurface);
                failure.error = stateResult.error;
                return false;
            }

            std::optional<std::vector<std::int32_t>> geometrySectorIndices{};
            if ( transition.sectorSubset() )
            {
                geometrySectorIndices.emplace(
                    transition.sectorIndices().begin(),
                    transition.sectorIndices().end());
            }
            std::optional<std::vector<std::int32_t>> thingIndices{};
            if ( transition.thingSubset() )
            {
                thingIndices.emplace(
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
            std::optional<Selection> navigationSelection{};
            if ( const Selection* const navigation =
                    transition.navigationSelection() )
            {
                navigationSelection = *navigation;
            }
            convertedHistory.compact->appendCompactRecoveryState(
                stack,
                std::string(label),
                dirtyFlags,
                state.level,
                state.selection,
                std::move(geometrySectorIndices),
                std::move(thingIndices),
                std::move(thingTail),
                std::move(navigationSelection),
                transition.surfaceSubset());
            ++index;
            return true;
        });
    if ( !convertedAll )
    {
        if ( failure.error.empty() )
        {
            failure.error = "Undo/redo history could not be visited completely.";
        }
        return failure;
    }

    convertedHistory.compact->finishCompactRecoveryImport();
    current = std::move(convertedCurrent);
    history = std::move(convertedHistory);
    return {};
}

} // namespace sed::editor
