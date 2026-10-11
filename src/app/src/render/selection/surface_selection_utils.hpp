#pragma once

#include "sed/level.hpp"

#include <QMatrix4x4>
#include <QPointF>
#include <QVector3D>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace sed::render
{

/**
 * Bounds used by the selected-surface normal overlay preferences.  These are
 * scale factors (rather than raw world units) so the marker remains readable
 * on both tiny and very large level geometry.
 */
inline constexpr float kSurfaceNormalLengthScaleMin = 0.10f;
inline constexpr float kSurfaceNormalLengthScaleMax = 4.00f;
inline constexpr float kSurfaceNormalArrowheadScaleMin = 0.10f;
inline constexpr float kSurfaceNormalArrowheadScaleMax = 3.00f;

float clampedSurfaceNormalLengthScale(float value) noexcept;
float clampedSurfaceNormalArrowheadScale(float value) noexcept;

/** Returns the planar polygon area centroid, with a robust legacy fallback. */
std::optional<QVector3D> surfaceFaceCenter(
    const Sector& sector,
    const Surface& surface) noexcept;

/** Returns the stored unit normal, or a Newell fallback for valid geometry. */
std::optional<QVector3D> surfaceDisplayNormal(
    const Sector& sector,
    const Surface& surface) noexcept;

/**
 * Returns a unit arrowhead side vector whose normal/side plane faces the
 * camera. Camera-right/up are deterministic fallbacks for end-on views.
 */
std::optional<QVector3D> surfaceNormalArrowheadSide(
    QVector3D surfaceNormal,
    QVector3D directionToCamera,
    QVector3D cameraRight,
    QVector3D cameraUp) noexcept;

/**
 * Selection-aware surface pivot modes used by transform gizmos. Face normals
 * intentionally use surfaceFaceCenter(): a normal belongs to its face even
 * while an edge/vertex submode gives the transform gizmo a hit-aware pivot.
 */
enum class SurfaceSelectionPivotMode : std::uint8_t
{
    Face,
    Edge,
    Vertex,
};

/**
 * Resolves the same pivot used by the surface transform gizmo.
 *
 * `selection` is consumed in the gizmo's stable order (primary surface,
 * selected vertex surfaces, then selected faces); invalid references are
 * ignored.  Face mode returns a single face's planar area centroid or
 * the selection AABB centre for multiple faces.  Edge/Vertex mode
 * returns the nearest edge midpoint/vertex on `activeSurface` when the last
 * hit and hit surface are provided, otherwise it falls back to the AABB
 * centre.  A missing/invalid selection returns no value.
 */
std::optional<QVector3D> surfaceSelectionPivot(
    const Level& level,
    std::span<const SurfaceRef> selection,
    const std::optional<SurfaceRef>& activeSurface,
    SurfaceSelectionPivotMode mode,
    const std::optional<SurfaceRef>& hitSurface = std::nullopt,
    const std::optional<QVector3D>& hitPosition = std::nullopt) noexcept;

/** Authoritative double pivot used when committing transforms. */
std::optional<TVector> surfaceSelectionDocumentPivot(
    const Level& level,
    std::span<const SurfaceRef> selection,
    const std::optional<SurfaceRef>& activeSurface,
    SurfaceSelectionPivotMode mode,
    const std::optional<SurfaceRef>& hitSurface = std::nullopt,
    const std::optional<TVector>& hitPosition = std::nullopt) noexcept;

/**
 * Projects a world-space point through the supplied MVP matrix into viewport
 * coordinates.
 *
 * Returns false for points behind the camera or on a non-projectable clip W.
 */
bool projectWorldPointToScreen(
    const QVector3D& point,
    const QMatrix4x4& mvp,
    int viewportWidth,
    int viewportHeight,
    QPointF& outPos);

/**
 * Projects valid vertices from a level surface into screen space.
 *
 * Invalid vertex indices, non-finite coordinates, and points behind the camera
 * are skipped so callers can use the returned polygon directly for overlap
 * checks.
 */
std::vector<QPointF> projectSurfacePolygonToScreen(
    const Sector& sector,
    const Surface& surface,
    const QMatrix4x4& mvp,
    int viewportWidth,
    int viewportHeight);

/**
 * Returns true when a single-sided surface should be box-selectable from the
 * current camera position.
 *
 * Double-sided surfaces always pass. Missing or degenerate normals fall back to
 * a normal computed from the first valid three surface vertices.
 */
bool surfaceFacesSelectionCamera(
    const Sector& sector,
    const Surface& surface,
    const QVector3D& eye,
    const QVector3D& target,
    bool orthographicCamera);

} // namespace sed::render
