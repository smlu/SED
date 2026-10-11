#pragma once

#include "sed/geometry.hpp"

#include <cstdint>
#include <optional>
#include <span>

#include <QVector3D>

namespace sed::render {

struct SurfaceSnapVertex {
  QVector3D point{};
  std::int32_t sector{-1};
  std::int32_t vertex{-1};
  std::optional<TVector> documentPoint{};
};

struct SurfaceSnapEdge {
  QVector3D first{};
  QVector3D second{};
  std::int32_t sector{-1};
  std::int32_t firstVertex{-1};
  std::int32_t secondVertex{-1};
  std::optional<TVector> documentFirst{};
  std::optional<TVector> documentSecond{};
};

struct SurfaceEdgeMoveSnapResult {
  QVector3D translation{};
  TVector documentTranslation{};
  QVector3D rotationAxis{};
  QVector3D rotationPivot{};
  TVector documentRotationPivot{};
  float rotationAngleDegrees{};
};

struct SurfaceMoveOrientationSnapResult {
  QVector3D rotationAxis{};
  float rotationAngleDegrees{};
};

enum class SurfaceMoveSnapElement : std::uint8_t {
  Vertex,
  Edge,
  Face,
};

enum class SurfaceMoveSnapSolver : std::uint8_t {
  None,
  Vertex,
  Edge,
  Face,
};

/**
 * Maps one world coordinate to the nearby-surface broad-phase lattice.
 * Extreme but finite editor values clamp instead of invoking an out-of-range
 * floating-to-integer conversion.
 */
[[nodiscard]] std::int64_t surfaceSnapBroadPhaseCellCoordinate(
    float value,
    float cellSize) noexcept;

/** Select an entity-specific solver without falling through to Face logic. */
[[nodiscard]] constexpr SurfaceMoveSnapSolver resolveSurfaceMoveSnapSolver(
    const SurfaceMoveSnapElement element,
    const bool activeElementAvailable) noexcept {
  switch (element) {
  case SurfaceMoveSnapElement::Vertex:
    return activeElementAvailable ? SurfaceMoveSnapSolver::Vertex
                                  : SurfaceMoveSnapSolver::None;
  case SurfaceMoveSnapElement::Edge:
    return activeElementAvailable ? SurfaceMoveSnapSolver::Edge
                                  : SurfaceMoveSnapSolver::None;
  case SurfaceMoveSnapElement::Face:
    return SurfaceMoveSnapSolver::Face;
  }
  return SurfaceMoveSnapSolver::None;
}

/**
 * Returns whether Nearby Snap may add an orientation correction for the
 * current Surface element and move constraint. Faces can orient during free
 * or axis movement; Edges orient only during free movement; Vertices never
 * orient.
 */
[[nodiscard]] bool nearbyMoveOrientationAllowed(
    SurfaceMoveSnapElement element,
    bool rotateToNearbyEnabled,
    const QVector3D &constraintAxis = {}) noexcept;

/**
 * Preserve an axis-handle constraint when a higher-level snap solver proposes
 * an unconstrained translation. Free movement returns the proposed snap
 * unchanged. Axis movement projects only the snap correction onto that axis
 * and rejects targets whose remaining off-axis error exceeds the caller's
 * capture tolerance.
 */
[[nodiscard]] std::optional<QVector3D> constrainMoveSnapTranslation(
    const QVector3D &rawTranslation, const QVector3D &snappedTranslation,
    float maximumResidual, const QVector3D &constraintAxis = {});

/** Snap one moved vertex to the nearest eligible stationary vertex. */
[[nodiscard]] std::optional<QVector3D> snapVertexMoveTranslation(
    const QVector3D &movingPoint, const QVector3D &rawTranslation,
    std::span<const SurfaceSnapVertex> stationaryVertices,
    float captureDistance, const QVector3D &constraintAxis = {});

/** Same solver with the moving vertex's authoritative document coordinate. */
[[nodiscard]] std::optional<QVector3D> snapVertexMoveTranslation(
    const SurfaceSnapVertex& movingVertex, const QVector3D& rawTranslation,
    std::span<const SurfaceSnapVertex> stationaryVertices,
    float captureDistance, const QVector3D& constraintAxis = {},
    TVector* documentTranslation = nullptr);

/** Keeps the pointer displacement exact before candidate ranking and axis projection. */
[[nodiscard]] std::optional<QVector3D> snapVertexMoveTranslation(
    const SurfaceSnapVertex& movingVertex, const TVector& rawTranslation,
    std::span<const SurfaceSnapVertex> stationaryVertices,
    float captureDistance, const QVector3D& constraintAxis = {},
    TVector* documentTranslation = nullptr);

/**
 * Snap one moved edge to a nearby parallel edge. Free movement aligns edge
 * midpoints exactly; an axis handle applies only the correction available on
 * that axis and accepts it only while the residual remains within capture.
 */
[[nodiscard]] std::optional<QVector3D> snapEdgeMoveTranslation(
    const SurfaceSnapEdge &movingEdge, const QVector3D &rawTranslation,
    std::span<const SurfaceSnapEdge> stationaryEdges, float captureDistance,
    float minimumParallelCosine = 0.995f, const QVector3D &constraintAxis = {});

/**
 * Snap a moved edge to the nearest similarly oriented stationary edge.  In
 * free-move mode the small remaining orientation difference is resolved as a
 * rotation around the moving midpoint, followed by an exact midpoint
 * translation.  Axis-constrained moves preserve the handle constraint and do
 * not rotate because that would move the edge away from the selected axis.
 */
[[nodiscard]] std::optional<SurfaceEdgeMoveSnapResult>
snapEdgeMoveTransform(
    const SurfaceSnapEdge &movingEdge, const QVector3D &rawTranslation,
    std::span<const SurfaceSnapEdge> stationaryEdges, float captureDistance,
    float minimumParallelCosine = 0.995f,
    const QVector3D &constraintAxis = {});

[[nodiscard]] std::optional<SurfaceEdgeMoveSnapResult> snapEdgeMoveTransform(
    const SurfaceSnapEdge& movingEdge, const TVector& rawTranslation,
    std::span<const SurfaceSnapEdge> stationaryEdges, float captureDistance,
    float minimumParallelCosine = 0.995f, const QVector3D& constraintAxis = {});

/**
 * Resolve the normal correction used while moving a Face near a suitable
 * stationary surface. The closer of the stationary normal and its opposite
 * is used, so the common opposed/back-to-back case stays stable while an
 * already same-facing Face is never flipped by 180 degrees. With a 90-degree
 * limit every possible parallel orientation remains attainable.
 */
[[nodiscard]] std::optional<SurfaceMoveOrientationSnapResult>
snapSurfaceMoveOrientation(const QVector3D &movingNormal,
                           const QVector3D &stationaryNormal,
                           float maximumCorrectionDegrees = 90.0f);

} // namespace sed::render
