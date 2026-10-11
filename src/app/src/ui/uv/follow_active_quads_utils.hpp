#pragma once

#include "ui/uv/uv_island_geometry_utils.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace sed::ui::uv
{

enum class FollowActiveQuadsMode : std::uint8_t
{
    even,
    length,
    lengthAverage,
};

enum class FollowActiveQuadsError : std::uint8_t
{
    none,
    noActiveFace,
    activeFaceNotSelected,
    activeFaceNotQuad,
    invalidActiveUv,
    noConnectedQuads,
    invalidGeometry,
};

struct FollowActiveQuadsReport
{
    FollowActiveQuadsError error{FollowActiveQuadsError::none};
    std::size_t visitedFaceCount{};
    std::size_t updatedFaceCount{};
    std::size_t ignoredNonQuadCount{};
    std::size_t ambiguousEdgeCount{};
};

/** Human-readable error for UI feedback. Empty when the report succeeded. */
[[nodiscard]] const char *followActiveQuadsErrorMessage(FollowActiveQuadsError error) noexcept;

/**
 * Extend the active quad's UVs through selected, world-edge-connected quads.
 *
 * The active face is preserved exactly. Traversal is deterministic breadth
 * first; a branch or cycle is assigned by the first stable face/edge path.
 * Edges with more than one compatible selected neighbor are not traversed.
 * Length Average measures complete quad edge loops in the level, including
 * unselected quads, as Blender does. Since SED has no mesh seam metadata, a
 * discontinuous current UV edge does not act as an implicit seam.
 *
 * The operation is atomic: `islands` is unchanged on failure. When supplied,
 * `report` receives the precise failure reason for UI feedback.
 */
bool followActiveQuads(const Level &level, std::vector<UvIslandState> &islands, const std::vector<bool> &selectedFaces,
                       std::optional<SurfaceRef> activeFace, FollowActiveQuadsMode mode,
                       FollowActiveQuadsReport *report = nullptr);

} // namespace sed::ui::uv
