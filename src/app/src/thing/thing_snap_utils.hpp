#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <QVector3D>

#include "sed/assets/resource_locator.hpp"
#include "sed/assets/tpl.hpp"
#include "sed/level.hpp"
#include "sed/level/geometry.hpp"

namespace sed::thing
{

/** Minimum and maximum coordinates of a ThingBox on each local axis. */
struct BoxMinMax
{
    std::array<double, 3> min{};
    std::array<double, 3> max{};
};

/** Axis-aligned bounds for already-transformed Qt vectors. */
struct VectorBounds3
{
    QVector3D min{};
    QVector3D max{};
    bool valid{};
};

/** Axis-aligned bounds for local TVector points. */
struct LocalBounds3
{
    std::array<double, 3> min{};
    std::array<double, 3> max{};
    bool valid{};
};

/** Best transform adjustment found by a snap solver. */
struct TransformSnapCandidate
{
    QVector3D translation{};
    std::optional<TVector> documentTranslation{};
    QVector3D rotateAxis{};
    QVector3D rotatePivot{};
    float rotateAngleDeg{};
    double score{std::numeric_limits<double>::infinity()};
};

/**
 * Local connector used by thing-to-thing snapping.
 *
 * Rails/tracks expose a center connector plus a pair of rail points. Generic
 * models can ignore railA/railB and use position/tangent as a side connector.
 */
struct SnapConnector
{
    TVector position{};
    TVector tangent{TVector::forward};
    TVector railA{};
    TVector railB{};
};

/** Candidate connector with a geometric quality score. */
struct ScoredSnapConnector
{
    SnapConnector connector{};
    double score{};
};

/** Candidate transform that joins a moving rail pair to an anchor rail pair. */
struct RailPairSnapCandidate
{
    TVector origin{};
    TVector pyr{};
    TMatrix43 rotation{};
    TVector targetRailA{};
    TVector targetRailB{};
    TVector movingRailA{};
    TVector movingRailB{};
    double pairResidualSq{};
    double tangentPenalty{};
    double tangentDot{};
};

/** True when all transform coordinates/angles on a thing are finite. */
bool finiteThingTransform(const Thing& thing) noexcept;

/**
 * Returns a copy of an owning Thing posed at a serialized path frame.
 *
 * Keeping the template/resource identity on the copy lets frame tools reuse
 * the same rendered bounds, connector, surface-contact, and grid-contact
 * calculations as ordinary Thing placement and movement.
 */
std::optional<Thing> thingAtFramePose(const Thing& owner, const TplValue& frameValue);

/**
 * Combines highlighted and active thing indices into a unique, valid move set.
 *
 * The active thing is appended last, preserving the existing editor behavior
 * where highlighted things are primary unless no highlighted thing is present.
 */
std::vector<std::int32_t> uniqueValidThingIndices(
    const std::vector<std::int32_t>& highlightedIndices,
    std::int32_t activeThingIndex,
    std::size_t thingCount);

/**
 * Combines highlighted and active thing indices for editor selections stored
 * as sets.
 */
std::vector<std::int32_t> uniqueValidThingIndices(
    const std::unordered_set<std::int32_t>& highlightedIndices,
    std::int32_t activeThingIndex,
    std::size_t thingCount);

/** True when the supplied thing index belongs to a move set. */
bool containsThingIndex(const std::vector<std::int32_t>& thingIndices, std::int32_t thingIndex);

/** Returns min/max arrays for a ThingBox. */
BoxMinMax boxMinMax(const ThingBox& box);

/** Returns the local center of each ThingBox axis from min/max arrays. */
std::array<double, 3> boxCenter(const BoxMinMax& bounds);

/** Reads one axis from a TVector. Axis 0/1/2 maps to x/y/z. */
double vectorComponent(const TVector& v, int axis);

/** Writes one axis into a TVector. Axis 0/1/2 maps to x/y/z. */
void setVectorComponent(TVector& v, int axis, double value);

/** Distance from a scalar value to a closed interval, or zero when inside. */
double outsideDistance(double value, double minValue, double maxValue);

/** Distance between two closed intervals, or zero when they overlap. */
float intervalOutsideDistance(float minA, float maxA, float minB, float maxB);
double intervalOutsideDistance(double minA, double maxA, double minB, double maxB);

/** Expands transformed bounds to include a point. */
void addPointToBounds(VectorBounds3& bounds, const QVector3D& point);

/** Expands local bounds to include a point. */
void addPointToBounds(LocalBounds3& bounds, const TVector& point);

/** Reads one axis from a QVector3D. Axis 0/1/2 maps to x/y/z. */
float vectorComponent(const QVector3D& v, int axis);

/** Returns a world unit axis. Axis 0/1/2 maps to x/y/z. */
QVector3D worldAxisVector(int axis);

/**
 * Builds bounds by asking a caller-provided function for each thing's world
 * corners under a candidate transform.
 */
VectorBounds3 transformedThingCornerBounds(
    const std::vector<std::int32_t>& thingIndices,
    const QVector3D& translation,
    float yawDeltaDeg,
    const std::function<std::vector<QVector3D>(std::int32_t, const QVector3D&, float)>& worldCornersForThing);

/** Returns one of a matrix's local basis vectors. Axis 0/1/2 maps to r/l/u. */
TVector matrixAxisVector(const TMatrix43& matrix, int axis);

/** Flattens a vector to XY and normalizes it, returning nullopt for zero length. */
std::optional<TVector> flattenNormalize(TVector v);

/** Signed yaw delta in degrees needed to rotate one flattened vector into another. */
double yawDeltaDeg(const TVector& from, const TVector& to);

/** True when a connector contains a meaningful rail pair. */
bool connectorHasRailPair(const SnapConnector& connector) noexcept;

/**
 * Computes the snap probe distance used for connector reach checks.
 *
 * Generic side connectors may snap when either the connector or cursor probe is
 * close to the target. Rail/track joins must require the actual moving connector
 * to be close, otherwise dragging near a target can prematurely snap and hold a
 * visible gap between rail joints.
 */
double connectorSnapProbeDistanceSq(
    double connectorDistanceSq,
    double cursorDistanceSq,
    bool requireConnectorProximity) noexcept;

/**
 * Computes the world-space acquisition reach for connector snapping.
 *
 * Rail/track pieces need a wider capture radius than generic side snapping
 * because their useful endpoints are often far from the drag pivot or cursor.
 * The final snap still aligns the actual rail endpoints exactly.
 */
double connectorSnapReach(
    double unitsPerPixel,
    bool railPairSnap,
    double baseRadius = 0.16) noexcept;

/** Projection clustering tolerance for scanning candidate rail side planes. */
double railTrackProjectionTolerance(double projectionExtent) noexcept;

/** Maximum allowed thickness of a scanned side along its projection axis. */
double railTrackMaxSideAxisSpan(double projectionExtent) noexcept;

/** Maximum valid spacing between the two rail strands on a candidate side. */
double railTrackMaxRailPairSpan(double modelExtent) noexcept;

/** Lateral search pad used to decide which neighboring points sit beside a candidate side. */
double railTrackSideLateralPad(double railPairSpan) noexcept;

/** Expected rail-pair spacing used by connector candidate scoring. */
double railTrackIdealRailPairSpan(double modelExtent) noexcept;

/** Scores a scanned side connector; lower scores are better. */
double railTrackSideCandidateScore(
    double axisSpan,
    double railPairSpan,
    double modelExtent,
    int sidePointCount) noexcept;

/**
 * Sorts scored rail/track connector candidates and returns the best distinct connectors.
 *
 * Model scanning may find the same side connector from several nearby projection
 * slices. This helper centralizes the score ordering, duplicate rejection, and
 * maximum exported connector count so the scanner can stay focused on geometry
 * detection instead of result grooming.
 */
std::vector<SnapConnector> selectBestDistinctRailTrackConnectors(
    std::vector<ScoredSnapConnector> candidates);

/**
 * Builds rail-pair snap candidates for two connectors.
 *
 * Both rail endpoints are considered, including swapped rail ordering on the
 * moving and anchor connector. Invalid zero-width gauges or tangents return no
 * candidates. When rotation is enabled, the moving connector tangent is aligned
 * first unless that would exceed the configured maximum; that candidate then
 * remains translation-only. Exact endpoint fits are preferred; compatible mixed-gauge track pieces
 * may fall back to a centerline join so shipped short/curved rail assets can be
 * chained without visible gaps. The caller still owns cursor/probe/origin
 * scoring.
 */
std::vector<RailPairSnapCandidate> buildRailPairSnapCandidates(
    const SnapConnector& movingConnector,
    const SnapConnector& anchorConnector,
    const TVector& basePyr,
    const TMatrix43& baseRotation,
    const TMatrix43& anchorTransform,
    const TVector& desiredMovingTangent,
    bool rotateToAlign,
    double minimumTangentDot,
    double maximumRotationDegrees = 180.0,
    bool viewportFloatYaw = false);

/** Returns the eight local-space corners of a thing box. */
std::array<TVector, 8> thingBoxCorners(const ThingBox& box);

/** Appends local-space face centers of a thing box. */
void appendThingBoxFaceCenters(std::vector<TVector>& points, const ThingBox& box);

/** Rotates a Qt vector around a center in the XY plane, preserving Z. */
QVector3D rotatePointZ(const QVector3D& point, const QVector3D& center, float angleDeg);

/**
 * Transforms local box points for a thing under a candidate translation/yaw.
 *
 * Used by both surface-stick probing and thing-to-thing snap bounds. Optional
 * face centers are included for surface contact support points.
 */
/** Double authoring counterpart; pivot, translation and result share the caller's coordinate frame. */
std::vector<TVector> transformThingBoxDocumentPoints(
    const Thing& thing, const ThingBox& box, const TVector& pivot,
    const TVector& translation, double yawDeltaDeg, bool includeFaceCenters);

std::vector<QVector3D> transformThingBoxPoints(
    const Thing& thing,
    const ThingBox& box,
    const QVector3D& pivot,
    const QVector3D& translation,
    float yawDeltaDeg,
    bool includeFaceCenters);

/**
 * Resolves a thing's local box and returns transformed world-space corners.
 */
std::vector<QVector3D> resolveThingWorldCorners(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    std::int32_t thingIndex,
    const QVector3D& pivot,
    const QVector3D& translation,
    float yawDeltaDeg,
    double fallbackHalfSize);

/**
 * Builds transformed bounds for several level things using resolved local boxes.
 */
VectorBounds3 transformedLevelThingCornerBounds(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    const std::vector<std::int32_t>& thingIndices,
    const QVector3D& pivot,
    const QVector3D& translation,
    float yawDeltaDeg,
    double fallbackHalfSize);

/** Resolves only the model 3DO box for a placed thing. */
std::optional<ThingBox> resolveThingModelBox(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    const Thing& thing);

/** Returns a surface normal for a pick hit, recalculating legacy zero normals. */
std::optional<TVector> surfaceNormalForHit(const Level& level, const sed::level::SurfacePickHit& hit);

/** True when an adjoined surface should be skipped by placement/stick picking. */
bool isPassThroughPlacementSurface(const Level& level, const sed::level::SurfacePickHit& hit);

/**
 * Resolves the editor-local visual/template bounds for a placed thing.
 *
 * Resolution order intentionally matches the previous viewport code:
 * model resource, sprite resource, placed thing bbox, inherited template bbox,
 * then a non-visual editor cube fallback.
 */
std::optional<ThingBox> resolveThingLocalBox(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    const Thing& thing,
    double fallbackHalfSize);

/**
 * Resolves the visual support points for a Thing pose in world space.
 *
 * The returned corners and face centres are suitable for grid-plane and
 * surface-contact tests. If no resource/template bounds can be resolved, the
 * Thing origin remains a deterministic fallback.
 */
/** Authored support points before any absolute float conversion. */
std::vector<TVector> resolveThingDocumentSupportPoints(
    const Level& level, const assets::Templates* templates,
    const assets::ResourceLocator* resources, const Thing& posedThing,
    double fallbackHalfSize);

std::vector<QVector3D> resolveThingWorldSupportPoints(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    const Thing& posedThing,
    double fallbackHalfSize);

/**
 * Extracts track/rail endpoint connectors from model vertices.
 *
 * The algorithm samples side candidates across XY directions and keeps the
 * clearest rail-pair endpoints. It is intentionally data-driven because legacy
 * templates do not carry explicit connector metadata.
 */
std::vector<SnapConnector> extractRailTrackConnectorsFromModelText(std::string_view modelText);

/**
 * Resolves rail/track connectors for a model resource when the model/template
 * names look like rail or track assets. Non rail-like names return an empty list.
 */
std::vector<SnapConnector> resolveRailTrackConnectors(
    const assets::ResourceLocator& resources,
    std::string_view modelName,
    std::string_view thingName);

/**
 * Resolves rail/track connectors for a placed thing.
 */
std::vector<SnapConnector> resolveThingSnapConnectors(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    const Thing& thing);

} // namespace sed::thing
