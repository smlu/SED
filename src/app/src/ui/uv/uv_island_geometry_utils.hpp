#pragma once

#include "sed/level.hpp"

#include <optional>
#include <vector>

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>

namespace sed::ui::uv
{

/** Editable UV polygon plus its display transform and surface identity. */
struct UvIslandState
{
    SurfaceRef ref{};
    QString label{};
    QColor color{};
    QString material{};
    QSize textureSize{ 1, 1 };
    UvCoordinateSpace storedCoordinateSpace{UvCoordinateSpace::Normalized};
    std::vector<QPointF> uv{};
    std::vector<QPointF> unwrapUv{};
    QPointF displayOffset{};
    qreal displayMirrorAxisX{ 0.0 };
    bool displayMirrorX{ false };
    bool selected{ true };
};

struct UvViewOrientation
{
    bool mirrorX{};
    qreal rotationRadians{};
};

/** One directed UV polygon edge used by translation-only edge snapping. */
struct UvSnapEdge
{
    QPointF start{};
    QPointF end{};
};

/** Geometry inputs for one UV translation snap, gathered across every island. */
struct UvSnapGeometry
{
    std::vector<QPointF> movingPoints{};
    std::vector<QPointF> targetPoints{};
    std::vector<QPointF> movingSurfacePoints{};
    std::vector<QPointF> targetSurfacePoints{};
    std::vector<UvSnapEdge> movingEdges{};
    std::vector<UvSnapEdge> targetEdges{};
};

/** Returns the axis-aligned bounds of a point set, or an invalid rect when empty. */
QRectF uvPointBounds(const std::vector<QPointF>& points);

/** Snaps one coordinate to the configured UV grid; invalid inputs are returned unchanged. */
qreal uvSnapCoordinateToGrid(qreal value, qreal gridStep) noexcept;

/** Snaps both coordinates to the configured UV grid. */
QPointF uvSnapPointToGrid(const QPointF& point, qreal gridStep) noexcept;

/**
 * Snaps to the visible main UV grid, or to its four subdivisions when the
 * optional sub-grid is enabled. Invalid input keeps the original point.
 */
QPointF uvSnapPointToVisibleGrid(
    const QPointF& point,
    qreal mainGridStep,
    bool subGridEnabled) noexcept;

/** Coordinate counterpart to uvSnapPointToVisibleGrid(). */
qreal uvSnapCoordinateToVisibleGrid(
    qreal value,
    qreal mainGridStep,
    bool subGridEnabled) noexcept;

/**
 * Returns the shortest translation from any reference point to the enabled
 * visible lattice. For valid non-empty input this is unconditional, including
 * an exact zero delta when the reference already lies on the grid.
 */
std::optional<QPointF> uvNearestVisibleGridSnapDelta(
    const std::vector<QPointF>& referencePoints,
    qreal mainGridStep,
    bool subGridEnabled) noexcept;

/**
 * Collects moving and unselected target geometry across all UV islands.
 * An island with any moving component contributes no target geometry, which
 * prevents a partial component edit from snapping back onto its own owner.
 * Surface points are centroids; a moving centroid is emitted only when the
 * complete island translates rigidly.
 */
UvSnapGeometry uvCollectSnapGeometry(
    const std::vector<UvIslandState>& islands,
    const std::vector<std::vector<bool>>& movingVertexMasks,
    const std::vector<std::vector<bool>>& selectedEdgeMasks,
    bool collectEdges);

/**
 * Finds the nearest exact point-to-point translation within maxDistance.
 * A zero delta is a successful snap and deliberately prevents a later grid
 * fallback from pulling an already coincident vertex away from its target.
 */
std::optional<QPointF> uvNearestPointSnapDelta(
    const std::vector<QPointF>& movingPoints,
    const std::vector<QPointF>& targetPoints,
    qreal maxDistance) noexcept;

/**
 * Finds the nearest translation which overlays a moving edge on a parallel
 * target edge. Both endpoint orders are considered, so reversed polygon
 * winding does not change the result. Candidate directions must be parallel
 * within five degrees, and edges whose post-translation endpoint residual
 * exceeds maxEndpointResidual are rejected.
 */
std::optional<QPointF> uvNearestEdgeSnapDelta(
    const std::vector<UvSnapEdge>& movingEdges,
    const std::vector<UvSnapEdge>& targetEdges,
    qreal maxDistance,
    qreal maxEndpointResidual) noexcept;

/** Returns the editable UV bounds for a single island. */
QRectF islandBounds(const UvIslandState& island);

/** Computes signed polygon area; clockwise polygons return negative area. */
qreal polygonSignedArea(const std::vector<QPointF>& points);

/** Resolves the reflection needed to match two stable polygon windings. */
std::optional<bool> uvViewMirrorXForWinding(qreal storedArea, qreal projectedArea);

/** Converts a UV coordinate through the whole-canvas view transform. */
QPointF uvViewScreenFromUv(
    const QPointF& uv,
    const QPointF& pan,
    qreal zoom,
    bool mirrorX,
    qreal rotationRadians = 0.0);

/** Inverse of uvViewScreenFromUv(). */
QPointF uvViewUvFromScreen(
    const QPointF& screen,
    const QPointF& pan,
    qreal zoom,
    bool mirrorX,
    qreal rotationRadians = 0.0);

/**
 * Resolves one rigid whole-canvas orientation from corresponding active-face
 * stored UV and viewport-projected vertices. The longest stable stored edge chooses
 * rotation after the winding-derived reflection is applied.
 */
std::optional<UvViewOrientation> uvViewOrientationForActiveSurface(
    const std::vector<UvIslandState>& islands,
    std::optional<SurfaceRef> activeSurface,
    const std::vector<QPointF>& projectedVertices);

/**
 * Resolves the one whole-canvas View Face mirror from the active surface.
 * Other selected islands never vote independently. Missing/unstable active
 * projection data preserves previousMirrorX so edge-on and behind-camera
 * transitions cannot make a multi-surface edit flicker.
 */
bool uvViewMirrorXForActiveSurface(
    const std::vector<UvIslandState>& islands,
    std::optional<SurfaceRef> activeSurface,
    std::optional<qreal> projectedArea,
    bool previousMirrorX);

/** Computes pan so one UV coordinate remains under a chosen screen point. */
QPointF uvViewPanForAnchor(
    const QPointF& uv,
    const QPointF& screen,
    qreal zoom,
    bool mirrorX,
    qreal rotationRadians = 0.0);

/** Computes polygon centroid, falling back to point-cloud center for degenerate polygons. */
QPointF polygonCentroid(const std::vector<QPointF>& points);

/** Returns the editable UV centroid for a single island. */
QPointF islandCentroid(const UvIslandState& island);

/** Returns the display frame center, falling back to centroid when bounds are invalid. */
QPointF islandFrameCenter(const UvIslandState& island);

/** Converts stored texture-space UV coordinates into the current displayed island coordinates. */
QPointF islandDisplayFromStoredUv(const UvIslandState& island, QPointF uv);

/** Converts displayed island coordinates back into stored texture-space UV coordinates. */
QPointF islandStoredFromDisplayUv(const UvIslandState& island, QPointF uv);

/** Compares display polygons after restoring the exact float document representation. */
bool uvDisplayPointsStoreSameCoordinates(
    const UvIslandState& lhs, const std::vector<QPointF>& lhsPoints,
    const UvIslandState& rhs, const std::vector<QPointF>& rhsPoints);

/** Compares two island lists by surface identity and stored UV coordinates. */
bool uvIslandStatesStoreSameUv(const std::vector<UvIslandState>& lhs, const std::vector<UvIslandState>& rhs);

/** Returns the union bounds for all islands, optionally ignoring unselected islands. */
QRectF islandsBounds(const std::vector<UvIslandState>& islands, bool selectedOnly);

/** Returns the arithmetic center of a point cloud, or the origin when empty. */
QPointF uvPointCloudCenter(const std::vector<QPointF>& points);

/** Translates the editable UV polygon without changing display bookkeeping. */
void translateIsland(UvIslandState& island, const QPointF& delta);

/** Translates editable/display UV data and records the display offset. */
void translateIslandDisplay(UvIslandState& island, const QPointF& delta);

/** Rotates the editable UV polygon around its centroid. */
void rotateIsland(UvIslandState& island, qreal degrees);

/** Scales the editable UV polygon around its centroid. */
void scaleIsland(UvIslandState& island, qreal sx, qreal sy);

/**
 * Rotates stored surface UVs around their center. Normalized coordinates are
 * rotated directly; legacy pixel coordinates are first made aspect-neutral
 * using the texture dimensions. The edit is failure-atomic. A disengaged
 * result reports invalid input; an engaged false result reports a no-op.
 */
std::optional<bool> uvRotateTexVerticesAroundCenter(
    std::vector<TexVertex>& texVertices,
    UvCoordinateSpace coordinateSpace,
    const QSize& textureSize,
    double degrees) noexcept;

/**
 * Scales stored surface UVs around their center, failure-atomically. A
 * disengaged result reports invalid input; an engaged false reports a no-op.
 */
std::optional<bool> uvScaleTexVerticesAroundCenter(
    std::vector<TexVertex>& texVertices,
    double scaleU,
    double scaleV) noexcept;

/** Packs islands into deterministic rows, optionally changing only selected islands. */
void packIslands(std::vector<UvIslandState>& islands, bool selectedOnly);

/** Clamps texture dimensions so normalization never divides by zero. */
QSize sanitizeUvTextureSize(const QSize& size);

/** Converts pixel UVs to normalized stored UVs using a sanitized texture size. */
QPointF normalizeStoredUv(const QPointF& uv, const QSize& textureSize);

/** Converts normalized stored UVs back to pixel UVs using a sanitized texture size. */
QPointF denormalizeStoredUv(const QPointF& uv, const QSize& textureSize);

} // namespace sed::ui::uv
