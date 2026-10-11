#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "sed/geometry.hpp"
#include "sed/level.hpp"

namespace sed::level
{

struct EdgeRef
{
    std::int32_t sector{ -1 };
    std::int32_t surface{ -1 };
    std::int32_t edge{ -1 };
};

struct FrameRef
{
    std::int32_t thing{ -1 };
    std::int32_t value{ -1 };

    constexpr bool operator==(const FrameRef&) const noexcept = default;
};

/** One transformed sector vertex used to evaluate an immutable UV plan. */
struct ConnectedSurfaceUvVertexPosition
{
    VertexRef vertex{};
    TVector position{};
};

/** Immutable source mapping for one surface affected by shared moved vertices. */
struct ConnectedSurfaceUvPlanSurface
{
    SurfaceRef surface{};
    std::vector<VertexRef> vertices{};
    std::vector<TVector> sourcePositions{};
    std::vector<TexVertex> sourceTexVertices{};
    std::vector<bool> movedCorners{};
    std::size_t anchorCorner{};
    TVector sourceNormal{};
    TVector uGradient{};
    TVector vGradient{};
};

/**
 * Captured transform baseline. The plan contains no material dependency and
 * therefore works in either normalized or legacy texture-pixel UV space.
 */
struct ConnectedSurfaceUvPlan
{
    bool valid{};
    UvCoordinateSpace coordinateSpace{UvCoordinateSpace::Normalized};
    std::vector<VertexRef> movedVertices{};
    std::vector<TVector> sourceMovedPositions{};
    std::vector<ConnectedSurfaceUvPlanSurface> surfaces{};
};

enum class ConnectedSurfaceUvTransformKind : std::int32_t
{
    identity,
    translate,
    rotate,
    scale,
    rotateThenTranslate,
};

/**
 * Double-precision transform description shared by transient UV preview and
 * the corresponding committed geometry transform.
 */
struct ConnectedSurfaceUvTransform
{
    ConnectedSurfaceUvTransformKind kind{ConnectedSurfaceUvTransformKind::identity};
    TVector translation{};
    TVector rotationAxis{};
    TVector pivot{};
    double angleDegrees{};
    ScaleData scale{};
};

/** One non-authoritative or committed surface UV replacement. */
struct ConnectedSurfaceUvSurface
{
    SurfaceRef surface{};
    std::vector<std::int32_t> vertices{};
    std::vector<TexVertex> texVertices{};
};

/** Failure-atomic result shared by transform preview and final commit. */
struct ConnectedSurfaceUvEvaluation
{
    bool valid{};
    UvCoordinateSpace coordinateSpace{UvCoordinateSpace::Normalized};
    std::vector<ConnectedSurfaceUvSurface> surfaces{};
};

/**
 * Captures every face touched by only part of the moved vertex set. When
 * includeFullyMovedSurfaces is true, complete faces are captured as well so
 * Scale can retain the source texture density. Complete Move/Rotate faces are
 * intentionally omitted and retain their authored UVs bit-exactly.
 */
[[nodiscard]] ConnectedSurfaceUvPlan captureConnectedSurfaceUvPlan(const Level& level,
                                                                   std::span<const VertexRef> movedVertices,
                                                                   bool includeFullyMovedSurfaces);

/** Evaluates a captured plan without mutating its source Level. */
[[nodiscard]] ConnectedSurfaceUvEvaluation
evaluateConnectedSurfaceUvPlan(const ConnectedSurfaceUvPlan& plan,
                               std::span<const ConnectedSurfaceUvVertexPosition> transformedVertices);

/** Evaluates a plan with the exact double-precision transform used at commit. */
[[nodiscard]] ConnectedSurfaceUvEvaluation evaluateConnectedSurfaceUvPlan(const ConnectedSurfaceUvPlan& plan,
                                                                          const ConnectedSurfaceUvTransform& transform);

/** Applies the shared preview/commit transform to one source position. */
[[nodiscard]] TVector transformConnectedSurfaceUvPosition(const TVector& source,
                                                          const ConnectedSurfaceUvTransform& transform) noexcept;

/** Evaluates a plan from the current positions in a candidate Level. */
[[nodiscard]] ConnectedSurfaceUvEvaluation evaluateConnectedSurfaceUvPlan(const ConnectedSurfaceUvPlan& plan,
                                                                          const Level& transformedLevel);

/** Validates every target before publishing any evaluated UV coordinates. */
bool applyConnectedSurfaceUvEvaluation(Level& level, const ConnectedSurfaceUvEvaluation& evaluation) noexcept;

enum class Axis : std::int32_t
{
    x,
    y,
    z,
};

void levelClearVertexMarks(Level& level) noexcept;
/** Returns false when the recalc workset cannot be prepared; rejection leaves the Level untouched. */
[[nodiscard]] bool levelRecalcSectorChecked(Level& level, std::int32_t sectorIndex) noexcept;
void levelRecalcSector(Level& level, std::int32_t sectorIndex) noexcept;

/** Checked variants prepare all allocating scratch state before changing Level data. */
[[nodiscard]] bool levelTranslateSectorsChecked(
    Level& level, std::span<const std::int32_t> sectorIndices, double dx, double dy, double dz) noexcept;
[[nodiscard]] bool levelTranslateSurfacesChecked(
    Level& level, std::span<const SurfaceRef> surfaceIndices, double dx, double dy, double dz) noexcept;
[[nodiscard]] bool levelTranslateEdgesChecked(
    Level& level, std::span<const EdgeRef> edgeIndices, double dx, double dy, double dz) noexcept;
[[nodiscard]] bool levelTranslateVerticesChecked(
    Level& level, std::span<const VertexRef> vertexIndices, double dx, double dy, double dz) noexcept;

void levelTranslateSectors(Level& level, std::span<const std::int32_t> sectorIndices, double dx, double dy, double dz) noexcept;
void levelTranslateSurfaces(Level& level, std::span<const SurfaceRef> surfaceIndices, double dx, double dy, double dz) noexcept;
void levelTranslateEdges(Level& level, std::span<const EdgeRef> edgeIndices, double dx, double dy, double dz) noexcept;
void levelTranslateVertices(Level& level, std::span<const VertexRef> vertexIndices, double dx, double dy, double dz) noexcept;

// Selected Thing/Frame transforms stage their complete batch before publication.
// Allocation failure propagates with the Level unchanged; nonfinite generated
// poses reject the batch without mutation. Duplicate refs apply sequentially.
// Returns true only for an authored numeric pose/sector change. False leaves
// the Level unchanged, including noncanonical Frame tokens on a no-op.
bool levelTranslateThings(
    Level& level,
    std::span<const std::int32_t> thingIndices,
    double dx,
    double dy,
    double dz,
    bool moveFrames,
    bool updateSector = true);
bool levelTranslateFrames(Level& level, std::span<const FrameRef> frameIndices, double dx, double dy, double dz);
void levelTranslateLights(Level& level, std::span<const std::int32_t> lightIndices, double dx, double dy, double dz) noexcept;

void rotatePYR(double& pitch, double& yaw, double& roll, const TVector& axis, double angleDeg) noexcept;

[[nodiscard]] bool levelRotateSectorsChecked(
    Level& level,
    std::span<const std::int32_t> sectorIndices,
    const TVector& axis,
    double cx,
    double cy,
    double cz,
    double angleDeg) noexcept;
[[nodiscard]] bool levelRotateSurfacesChecked(
    Level& level,
    std::span<const SurfaceRef> surfaceIndices,
    const TVector& axis,
    double cx,
    double cy,
    double cz,
    double angleDeg) noexcept;
[[nodiscard]] bool levelRotateEdgesChecked(
    Level& level,
    std::span<const EdgeRef> edgeIndices,
    const TVector& axis,
    double cx,
    double cy,
    double cz,
    double angleDeg) noexcept;

void levelRotateSectors(Level& level, std::span<const std::int32_t> sectorIndices, const TVector& axis, double cx, double cy, double cz, double angleDeg) noexcept;
void levelRotateSurfaces(Level& level, std::span<const SurfaceRef> surfaceIndices, const TVector& axis, double cx, double cy, double cz, double angleDeg) noexcept;
void levelRotateEdges(Level& level, std::span<const EdgeRef> edgeIndices, const TVector& axis, double cx, double cy, double cz, double angleDeg) noexcept;
void levelRotateThings(Level& level, std::span<const std::int32_t> thingIndices, const TVector& axis, double cx, double cy, double cz, double angleDeg, bool rotatePYRValues) noexcept;
void levelRotateLights(Level& level, std::span<const std::int32_t> lightIndices, const TVector& axis, double cx, double cy, double cz, double angleDeg) noexcept;
bool levelRotateFrames(Level& level, std::span<const FrameRef> frameIndices, const TVector& axis, double cx, double cy, double cz, double angleDeg, bool rotatePYRValues);

// Malformed/cyclic ancestry, non-finite input, or arithmetic overflow leaves all coordinates unchanged.
void levelOffsetByNode(const Level& level, std::int32_t nodeIndex, double& x, double& y, double& z) noexcept;
void levelUnoffsetByNode(const Level& level, std::int32_t nodeIndex, double& x, double& y, double& z) noexcept;

void levelScaleSectors(Level& level, std::span<const std::int32_t> sectorIndices, const ScaleData& sd, bool scaleTextures) noexcept;
void levelScaleSurfaces(Level& level, std::span<const SurfaceRef> surfaceIndices, const ScaleData& sd, bool scaleTextures) noexcept;
void levelScaleThings(Level& level, std::span<const std::int32_t> thingIndices, const ScaleData& sd) noexcept;
void levelScaleLights(Level& level, std::span<const std::int32_t> lightIndices, const ScaleData& sd) noexcept;
bool levelScaleFrames(Level& level, std::span<const FrameRef> frameIndices, const ScaleData& sd);

[[nodiscard]] bool levelScaleSectorsChecked(
    Level& level, std::span<const std::int32_t> sectorIndices, const ScaleData& sd, bool scaleTextures) noexcept;
[[nodiscard]] bool levelScaleSurfacesChecked(
    Level& level, std::span<const SurfaceRef> surfaceIndices, const ScaleData& sd, bool scaleTextures) noexcept;

void levelFlipThings(Level& level, std::span<const std::int32_t> thingIndices, double cx, double cy, double cz, Axis axis) noexcept;
void levelFlipThingsOverPlane(Level& level, std::span<const std::int32_t> thingIndices, const TVector& planeNormal, double cx, double cy, double cz) noexcept;

void levelFlipLightsOverPlane(Level& level, std::span<const std::int32_t> lightIndices, const TVector& planeNormal, double cx, double cy, double cz) noexcept;
bool levelFlipFramesOverPlane(Level& level, std::span<const FrameRef> frameIndices, const TVector& planeNormal, double cx, double cy, double cz);
void levelFlipSurfaces(Level& level, std::span<const SurfaceRef> surfaceIndices) noexcept;
[[nodiscard]] bool levelFlipSectorsChecked(
    Level& level,
    std::span<const std::int32_t> sectorIndices,
    double cx,
    double cy,
    double cz,
    Axis axis) noexcept;
[[nodiscard]] bool levelFlipSectorsOverPlaneChecked(
    Level& level,
    std::span<const std::int32_t> sectorIndices,
    const TVector& planeNormal,
    double px,
    double py,
    double pz) noexcept;
void levelFlipSectors(Level& level, std::span<const std::int32_t> sectorIndices, double cx, double cy, double cz, Axis axis) noexcept;
void levelFlipSectorsOverPlane(Level& level, std::span<const std::int32_t> sectorIndices, const TVector& planeNormal, double px, double py, double pz) noexcept;

std::int32_t levelDuplicateSector(
    Level& level,
    std::int32_t sectorIndex,
    double cx,
    double cy,
    double cz,
    const TVector& nx,
    const TVector& ny,
    const TVector& nz,
    double dx,
    double dy,
    double dz) noexcept;

} // namespace sed::level
