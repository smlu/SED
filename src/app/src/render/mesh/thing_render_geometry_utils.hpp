#pragma once

#include <array>
#include <optional>
#include <string_view>
#include <vector>

#include "sed/assets/3do.hpp"
#include "sed/assets/tpl.hpp"
#include "sed/level.hpp"

namespace sed::render
{

/**
 * Camera motion changes sprite vertices only for original game-preview type-0
 * billboards. Editor sprites and type-2 sprites keep authored world bases.
 */
constexpr bool spriteGeometryDirtyOnViewChange(
    const bool gamePreview,
    const bool hasType0Billboards) noexcept
{
    return gamePreview && hasType0Billboards;
}

/** One point in a wireframe line list generated for viewport thing rendering. */
struct ThingWirePoint
{
    double x{};
    double y{};
    double z{};
    float pickId{};
};

/** One textured triangle vertex for the thing icon fallback cube. */
struct ThingCubeFaceVertex
{
    TVector position{};
    float u{};
    float v{};
};

/** Ordered local cube corners shared by icon-cube faces and fallback wire cubes. */
std::array<TVector, 8> localThingCubeCorners(double halfSize) noexcept;

/** Returns the eight box corners transformed into world space. */
std::array<TVector, 8> transformedThingBoxCorners(const TMatrix43& xf, const ThingBox& box) noexcept;

/** Returns the eight fallback cube corners transformed into world space. */
std::array<TVector, 8> transformedThingCubeCorners(const TMatrix43& xf, double halfSize) noexcept;

/**
 * Resolves the direct template box used by the legacy thing renderer.
 *
 * Local level templates win over the external template database. The function
 * intentionally does not walk parents or fall back from a zero template bbox;
 * that preserves the previous `gl_view_mesh.cpp` behavior.
 */
ThingBox resolveThingRenderBox(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing) noexcept;

/**
 * Resolves the world-vertex index used to match a level wire edge to its
 * supporting rendered triangle. `boundaryEdgeIndex` identifies the ordered
 * edge from that surface corner to the next corner, including the closing
 * edge from the last corner to the first.
 */
std::optional<std::size_t> wireframeSurfacePlaneReferenceVertexIndex(
    const Sector& sector,
    const Surface& surface,
    std::size_t boundaryEdgeIndex) noexcept;

/** Builds the 12-edge line-list points for a transformed thing bbox. */
std::array<ThingWirePoint, 24> makeThingBoxWirePoints(
    const TMatrix43& xf,
    const ThingBox& box,
    float pickId) noexcept;

/** Builds the 12-edge line-list points for a transformed fallback cube. */
std::array<ThingWirePoint, 24> makeThingCubeWirePoints(
    const TMatrix43& xf,
    double halfSize,
    float pickId) noexcept;

/** Builds the 12 textured triangles used by the editor icon fallback cube. */
std::array<ThingCubeFaceVertex, 36> makeThingCubeFaceVertices(
    const TMatrix43& xf,
    double halfSize) noexcept;

/**
 * Appends all valid 3DO face edges as transformed wireframe points.
 *
 * Invalid vertex indices are skipped per edge, matching the old viewport
 * lambdas while keeping the traversal reusable for normal and selection draws.
 */
void appendModelWireframeWirePoints(
    std::vector<ThingWirePoint>& out,
    const assets::Model3do& model,
    const TMatrix43& xf,
    float pickId);

} // namespace sed::render
