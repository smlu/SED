#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "sed/assets/3do.hpp"
#include "sed/assets/tpl.hpp"
#include "render/mesh/render_mesh_utils.hpp"
#include "sed/level.hpp"

namespace sed::render::thing_overlay
{

enum class CollisionShape : std::uint8_t
{
    none,
    sphere,
    sphereTree,
    face,
};

struct CollisionDescription
{
    CollisionShape shape{ CollisionShape::none };
    double radius{};
    bool hasRadius{};
    std::optional<double> broadPhaseRadius{};
};

struct Bounds3d
{
    TVector min{};
    TVector max{};
};

/** Resolve the effective inherited `collide`, `size`, and MotS `treesize` values. */
CollisionDescription resolveCollisionDescription(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing) noexcept;

/** Compare the exact effective values that determine collision overlay geometry. */
constexpr bool sameCollisionDescription(
    const CollisionDescription& first,
    const CollisionDescription& second) noexcept
{
    return first.shape == second.shape
        && first.hasRadius == second.hasRadius
        && (!first.hasRadius || first.radius == second.radius)
        && first.broadPhaseRadius == second.broadPhaseRadius;
}

/** Radius-only changes between cached sphere outlines keep their line topology. */
constexpr bool canUpdateCollisionSphereInPlace(
    const CollisionDescription& previous,
    const CollisionDescription& current) noexcept
{
    const auto isSphere = [](const CollisionShape shape) noexcept
    {
        return shape == CollisionShape::sphere;
    };
    return previous.hasRadius
        && current.hasRadius
        && isSphere(previous.shape)
        && isSphere(current.shape);
}

/** Three great-circle rings represented as independent line endpoint pairs. */
std::vector<TVector> makeSphereWirePoints(double radius, std::size_t segments = 24);

inline constexpr double kDefaultMotsTreeCollisionPreviewMeshRadiusFactor = 0.75;

/**
 * Append MotS collide=2's static hierarchy-mesh spheres in world space. Each
 * usable hierarchy node contributes one sphere with its effective authored
 * mesh radius multiplied by `meshRadiusFactor`. Nodes with MotS hierarchy flag
 * 0x2 use the nearest unflagged ancestor mesh's authored radius, matching the
 * engine's draw-time radius substitution. The default factor (0.75) is the
 * engine narrow-phase multiplier; callers may adjust it for editor preview.
 * The returned count excludes malformed nodes and meshes.
 */
std::size_t appendSphereTreeWirePoints(
    std::vector<TVector>& out,
    const assets::Model3do& model,
    const TMatrix43& thingTransform,
    double meshRadiusFactor = kDefaultMotsTreeCollisionPreviewMeshRadiusFactor);

/** Keep only finite, float-representable complete line segments. */
std::vector<TVector> finiteWireSegments(const std::vector<TVector>& points);

/** Bounds of all finite local 3DO vertices, or empty when no usable vertex exists. */
std::optional<Bounds3d> modelLocalVertexBounds(const assets::Model3do& model) noexcept;

/** World-axis-aligned bounds of a rendered sprite quad, or empty for invalid corners. */
std::optional<Bounds3d> spriteQuadWorldBounds(
    const std::array<TVector, 4>& corners) noexcept;

/**
 * Conservative radius enclosing a camera-facing sprite at any orbit. The
 * returned radius includes its local offset and half-diagonal; callers can
 * draw a world-axis-aligned cube of this half-extent around the Thing origin.
 */
std::optional<double> spriteBillboardConservativeRadius(
    double width,
    double height,
    const std::array<double, 3>& offset) noexcept;

/** Twelve transformed AABB edges represented as independent line endpoint pairs. */
std::array<TVector, 24> makeBoundsWirePoints(
    const Bounds3d& bounds,
    const TMatrix43& transform) noexcept;

/** Append the transformed unique model face edges, skipping malformed indices. */
void appendModelFaceWirePoints(
    std::vector<TVector>& out,
    const assets::Model3do& model,
    const TMatrix43& transform);

struct ThingOverlayGeometryPlan
{
    bool collisionShapes{};
    bool visualBounds{};
};

/**
 * Build both overlay classes while either view option is enabled. Keeping the
 * pair together lets users toggle between them without another rebuild, while
 * a Thing rebuild with both hidden avoids loading models just to regenerate
 * invisible wire geometry.
 */
constexpr ThingOverlayGeometryPlan thingOverlayGeometryPlan(
    const bool collisionShapesEnabled,
    const bool visualBoundsEnabled) noexcept
{
    const bool buildBoth = collisionShapesEnabled || visualBoundsEnabled;
    return ThingOverlayGeometryPlan{ buildBoth, buildBoth };
}

/**
 * Resolve composed draw ranges for the active and multi-selected Things.
 * Invalid owners, pick IDs, ranges, and offsets are omitted. The output is
 * ordered by Thing index so overlapping highlights remain deterministic.
 */
std::vector<DrawRange> selectedThingOverlayRanges(
    const std::unordered_set<std::int32_t>& selectedThingIndices,
    std::int32_t activeThingIndex,
    const std::vector<std::uint32_t>& thingPickIds,
    const std::unordered_map<int, std::vector<DrawRange>>& rangesByPickId,
    int composedVertexOffset);

/** All three visibility gates are required for either Thing overlay. */
constexpr bool shouldDrawThingOverlay(
    const bool showThings,
    const bool layerVisible,
    const bool thingVisible) noexcept
{
    return showThings && layerVisible && thingVisible;
}

/** Selection emphasis must not bypass the corresponding user-visible overlay toggle. */
constexpr bool shouldEmphasizeThingOverlay(
    const bool overlayEnabled,
    const bool hasSelectedRanges) noexcept
{
    return overlayEnabled && hasSelectedRanges;
}

} // namespace sed::render::thing_overlay
