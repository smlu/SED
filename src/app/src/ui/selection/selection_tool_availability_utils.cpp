#include "ui/selection/selection_tool_availability_utils.hpp"

#include <algorithm>

namespace sed::ui::selection
{
namespace
{

bool validSectorIndex(const std::int32_t sector) noexcept
{
    return sector >= 0;
}

bool validSurfaceRef(const SurfaceRef ref) noexcept
{
    return ref.sector >= 0 && ref.surface >= 0;
}

bool validSurfaceVertexRef(const editor::SurfaceVertexRef ref) noexcept
{
    return validSurfaceRef(ref.surface) && ref.localVertex >= 0;
}

bool hasNoNonSectorSelection(const editor::Selection& selection) noexcept
{
    return selection.surfaces.empty()
        && selection.surfaceVertices.empty()
        && selection.things.empty()
        && selection.frames.empty()
        && selection.lights.empty()
        && selection.cogs.empty();
}

bool hasNoNonSurfaceSelection(const editor::Selection& selection) noexcept
{
    return selection.sectors.empty()
        && selection.things.empty()
        && selection.frames.empty()
        && selection.lights.empty()
        && selection.cogs.empty();
}

} // namespace

bool canCleaveSectorSelection(const editor::Selection& selection) noexcept
{
    return !selection.sectors.empty()
        && hasNoNonSectorSelection(selection)
        && std::ranges::all_of(selection.sectors, validSectorIndex);
}

bool canJoinSectorSelection(const editor::Selection& selection) noexcept
{
    return selection.sectors.size() >= 2
        && hasNoNonSectorSelection(selection)
        && std::ranges::all_of(selection.sectors, validSectorIndex);
}

bool canMergeSectorSelection(const editor::Selection& selection) noexcept
{
    return selection.sectors.size() >= 2
        && hasNoNonSectorSelection(selection)
        && std::ranges::all_of(selection.sectors, validSectorIndex);
}

bool canCleaveSurfaceSelection(const editor::Selection& selection) noexcept
{
    return !selection.surfaces.empty()
        && selection.surfaceVertices.empty()
        && hasNoNonSurfaceSelection(selection)
        && std::ranges::all_of(selection.surfaces, validSurfaceRef);
}

bool canCleaveSurfaceEdgeSelection(const editor::Selection& selection) noexcept
{
    const bool hasSurfaceContext = !selection.surfaces.empty() || !selection.surfaceVertices.empty();
    return hasSurfaceContext
        && hasNoNonSurfaceSelection(selection)
        && std::ranges::all_of(selection.surfaces, validSurfaceRef)
        && std::ranges::all_of(selection.surfaceVertices, validSurfaceVertexRef);
}

void reconcileSurfaceEdgeOwnerSelection(
    editor::Selection& selection,
    const std::unordered_set<SurfaceRef,
        editor::SurfaceRefHash,
        editor::SurfaceRefEq>& edgeOwners,
    const bool replaceFaceContext)
{
    if (replaceFaceContext)
    {
        selection.surfaces.clear();
    }
    selection.surfaces.insert(edgeOwners.begin(), edgeOwners.end());
    selection.surfaceVertices.clear();
}

} // namespace sed::ui::selection
