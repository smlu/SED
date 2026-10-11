#pragma once

#include "ui/uv/uv_island_geometry_utils.hpp"

#include <cstdint>
#include <vector>

#include <QSize>
#include <QVector3D>

namespace sed::ui::uv
{

enum class SmartUvMarginMethod : std::uint8_t
{
    scaled,
    add,
    fraction,
};

enum class SmartUvRotationMethod : std::uint8_t
{
    axisAligned,
    axisAlignedHorizontal,
    axisAlignedVertical,
    geometryWorldVertical,
};

/** User-facing Smart UV Project controls with an architecture-safe SED default. */
struct SmartUvProjectOptions
{
    double angleLimitDeg{ 66.0 };
    SmartUvMarginMethod marginMethod{ SmartUvMarginMethod::scaled };
    SmartUvRotationMethod rotationMethod{ SmartUvRotationMethod::geometryWorldVertical };
    double islandMargin{ 0.0 };
    double areaWeight{ 0.0 };
    bool correctAspect{ true };
    bool scaleToBounds{ false };
    /** Uniform multiplier applied to each connected island after packing. */
    double islandScale{ 1.0 };
};

/** Optional diagnostics used by focused tests and future UI reporting. */
struct SmartUvProjectReport
{
    std::size_t projectionGroupCount{};
    std::size_t packedIslandCount{};
    std::size_t ignoredFaceCount{};
    std::vector<QVector3D> projectionNormals{};
    /** Projection-normal index per input island; -1 marks a degenerate ignored face. */
    std::vector<int> faceProjectionGroups{};
};

/**
 * Blender-style Smart UV Project for the supplied surface islands.
 *
 * Projection-vector discovery follows Blender's area-sorted normal clustering:
 * faces near each seed by half the angle limit contribute to an optionally
 * area-weighted projection normal, remaining faces seed the most angularly
 * distinct groups, and every face is assigned to its closest projection
 * normal. Shared-edge faces assigned to the same projection remain one packed
 * island. Packing is deterministic and uses a compact shelf approximation;
 * it intentionally does not embed Blender's internal polygon packer.
 *
 * The operation is atomic: on failure, `islands` and `report` are unchanged.
 */
bool smartUvProject(
    const Level& level,
    std::vector<UvIslandState>& islands,
    const SmartUvProjectOptions& options,
    const QSize& textureSize,
    SmartUvProjectReport* report = nullptr);

/**
 * Estimate the multiplier that makes a unit-scale projection retain the
 * selected surfaces' current UV edge scale. Corresponding non-degenerate
 * edges are compared with a median, so disconnected island offsets and one
 * malformed edge cannot dominate the result.
 */
[[nodiscard]] double estimateSmartUvIslandScale(
    const std::vector<UvIslandState>& current,
    const std::vector<UvIslandState>& projected) noexcept;

} // namespace sed::ui::uv
