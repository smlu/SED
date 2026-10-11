#pragma once

#include "sed/level.hpp"
#include "sed/level/transform.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace sed::render
{

struct GeometryTransformPreviewCorner
{
    SurfaceRef surface{};
    std::int32_t vertex{-1};
    TVector sourcePoint{};

    bool operator==(const GeometryTransformPreviewCorner&) const = default;
};

struct GeometryTransformPreviewVertex
{
    std::int32_t sector{-1};
    std::int32_t vertex{-1};
    TVector sourcePoint{};

    bool operator==(const GeometryTransformPreviewVertex&) const = default;
};

struct GeometryTransformPreviewPlan
{
    std::vector<SurfaceRef> wholeSurfaces{};
    std::vector<GeometryTransformPreviewCorner> partialCorners{};
    std::vector<GeometryTransformPreviewVertex> movedVertices{};

    [[nodiscard]] bool empty() const noexcept
    {
        return wholeSurfaces.empty()
            && partialCorners.empty()
            && movedVertices.empty();
    }
};

/**
 * Mirrors levelTranslate/Rotate/ScaleSurfaces without mutating the Level.
 * Complete selected and portal-mate faces are emitted once, while every
 * neighboring face that shares only part of the moved vertex set receives
 * exact per-source-vertex entries.
 */
[[nodiscard]] GeometryTransformPreviewPlan buildSurfaceTransformPreviewPlan(
    const Level& level,
    std::span<const SurfaceRef> selectedSurfaces);

/** Mirrors the geometry affected by a Sector translate/rotate/scale commit. */
[[nodiscard]] GeometryTransformPreviewPlan buildSectorTransformPreviewPlan(
    const Level& level,
    std::span<const std::int32_t> selectedSectors);

/** Captures UV correction from the same exact vertex set used by the shader preview. */
[[nodiscard]] level::ConnectedSurfaceUvPlan buildConnectedSurfaceUvPreviewPlan(
    const Level& level,
    const GeometryTransformPreviewPlan& geometryPlan,
    bool includeFullyMovedSurfaces);

/** Captures the union used by numeric transforms with mixed selection kinds. */
[[nodiscard]] level::ConnectedSurfaceUvPlan buildConnectedSurfaceUvTransformPlan(
    const Level& level,
    std::span<const std::int32_t> selectedSectors,
    std::span<const SurfaceRef> selectedSurfaces,
    bool includeFullyMovedSurfaces);

/**
 * Checks whether a scale preserves the source geometry admission rules for
 * the sectors affected by the committed transform. Existing nonconvex sectors
 * remain editable; strict solid convexity is required only for convex sources.
 */
[[nodiscard]] bool preservesAffectedScaleGeometry(
    const Level& source,
    const Level& candidate,
    std::span<const std::int32_t> sectorIndices,
    bool allowReversedWinding) noexcept;

/**
 * Negative preview IDs encode one source-point-filtered surface occurrence.
 * Bitwise complement remains defined for every non-negative signed pick ID,
 * including INT_MAX, and maps the point-buffer ID zero to -1.
 */
[[nodiscard]] constexpr int encodeFilteredTransformPreviewPickId(
    const int pickId) noexcept
{
    return ~pickId;
}

[[nodiscard]] constexpr bool transformPreviewPickIdIsFiltered(
    const int encodedPickId) noexcept
{
    return encodedPickId < 0;
}

[[nodiscard]] constexpr int decodeTransformPreviewPickId(
    const int encodedPickId) noexcept
{
    return transformPreviewPickIdIsFiltered(encodedPickId)
        ? ~encodedPickId
        : encodedPickId;
}

} // namespace sed::render
