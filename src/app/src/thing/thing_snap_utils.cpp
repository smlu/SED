#include "thing/thing_snap_utils.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include "sed/assets/3do.hpp"
#include "sed/level/thing.hpp"
#include "sed/math_utils.hpp"
#include "sed/text.hpp"
#include "thing/thing_resource_utils.hpp"
#include "ui/components/bounded_two_queue_cache.hpp"
#include "ui/components/preview_resource_revision_utils.hpp"

namespace
{

void appendUniqueValidThingIndex(
    std::vector<std::int32_t>& indices,
    const std::int32_t idx,
    const std::size_t thingCount)
{
    if (idx < 0 || static_cast<std::size_t>(idx) >= thingCount)
    {
        return;
    }
    if (std::find(indices.begin(), indices.end(), idx) == indices.end())
    {
        indices.push_back(idx);
    }
}

/** Minimum directional agreement for accepting a non-exact rail-gauge join. */
constexpr double kCompatibleRailGaugeDot = 0.82;

/**
 * Maximum summed squared rail endpoint residual for centerline fallback joins.
 *
 * Some shipped track models have slightly different local rail widths even
 * though their centerline/tangent endpoints are valid joins. Exact endpoint
 * candidates are still preferred; this tolerance only keeps those mixed-size
 * pieces usable once their gauge directions agree.
 */
constexpr double kCompatibleRailPairResidualSq = 0.0065;

/** Maximum rail/track connectors exported from one scanned model. */
constexpr std::size_t kMaxRailTrackSnapConnectors = 8;

/** Nearby scanned connector candidates are treated as the same model side. */
constexpr double kRailTrackConnectorDuplicateDistance = 0.045;

/** Duplicate candidates must point in nearly the same tangent direction. */
constexpr double kRailTrackConnectorDuplicateTangentDot = 0.70;

/** Candidate side planes shorter than this are too small to be useful rails. */
constexpr double kRailTrackMinProjectionExtent = 0.012;
constexpr int kRailTrackScanAngleStepDeg = 3;
constexpr int kRailTrackScanMaxAngleDeg = 180;
constexpr std::size_t kRailTrackMinModelPoints = 8;
constexpr double kRailTrackMinModelExtent = 0.075;
constexpr int kRailTrackMinSidePointCount = 4;

/** Projection clustering tolerance is proportional to scanned model span. */
constexpr double kRailTrackProjectionToleranceScale = 0.055;
constexpr double kRailTrackProjectionToleranceMin = 0.008;
constexpr double kRailTrackProjectionToleranceMax = 0.026;

/** Candidate sides must be thin along the scanned axis. */
constexpr double kRailTrackMaxSideAxisSpanMin = 0.018;
constexpr double kRailTrackMaxSideAxisSpanScale = 0.10;

/** Valid rail-pair gauge span limits for accepting a side connector. */
constexpr double kRailTrackMinRailPairSpan = 0.010;
constexpr double kRailTrackMaxRailPairSpanMin = 0.120;
constexpr double kRailTrackMaxRailPairSpanMax = 0.180;
constexpr double kRailTrackMaxRailPairSpanScale = 0.75;
constexpr int kRailTrackMinPointsPerRailStrand = 2;

/** Neighbor search padding around a side's rail-pair gauge. */
constexpr double kRailTrackSideLateralPadScale = 0.35;
constexpr double kRailTrackSideLateralPadMin = 0.010;
constexpr double kRailTrackSideLateralPadMax = 0.045;

/** Side score weights: low axis thickness, expected gauge width, many points. */
constexpr double kRailTrackSideScoreAxisSpanWeight = 4.0;
constexpr double kRailTrackIdealRailPairSpanScale = 0.28;
constexpr double kRailTrackIdealRailPairSpanMin = 0.060;
constexpr double kRailTrackIdealRailPairSpanMax = 0.140;
constexpr double kRailTrackSideScoreGaugeWeight = 0.15;
constexpr double kRailTrackSideScorePointReward = 0.0005;
constexpr double kRailTrackLocalCandidateReplaceDistance = 0.035;
constexpr double kRailTrackLocalCandidateReplaceTangentDot = 0.75;

/** Returns the center point between two local or world rail endpoints. */
sed::TVector railMidpoint(const sed::TVector& a, const sed::TVector& b)
{
    return (a + b).Scaled(0.5);
}

/** Normalizes a rail direction vector, rejecting zero-length gauges. */
std::optional<sed::TVector> normalizedRailVector(sed::TVector v)
{
    if (v.Normalize() == 0.0)
    {
        return std::nullopt;
    }
    return v;
}

/** Measures summed squared endpoint error after transforming a moving rail pair. */
double transformedRailPairResidualSq(
    const sed::TVector& origin,
    const sed::TMatrix43& rotation,
    const sed::TVector& movingRailA,
    const sed::TVector& movingRailB,
    const sed::TVector& targetRailA,
    const sed::TVector& targetRailB)
{
    const sed::TVector snappedRailA = origin + rotation.TransformVector(movingRailA);
    const sed::TVector snappedRailB = origin + rotation.TransformVector(movingRailB);
    return snappedRailA.DistanceSqr(targetRailA) + snappedRailB.DistanceSqr(targetRailB);
}

/** True when a moving rail gauge and target gauge point in compatible directions. */
bool railGaugeDirectionsAreCompatible(
    const sed::TMatrix43& movingRotation,
    const sed::TVector& movingRailA,
    const sed::TVector& movingRailB,
    const sed::TVector& targetRailA,
    const sed::TVector& targetRailB)
{
    const std::optional<sed::TVector> movingGauge =
        normalizedRailVector(movingRotation.TransformVector(movingRailB - movingRailA));
    const std::optional<sed::TVector> targetGauge =
        normalizedRailVector(targetRailB - targetRailA);
    return movingGauge
        && targetGauge
        && movingGauge->Dot(*targetGauge) >= kCompatibleRailGaugeDot;
}

/** Computes the origin that aligns moving and target rail centerlines. */
sed::TVector railCenterlineOrigin(
    const sed::TMatrix43& movingRotation,
    const sed::TVector& movingRailA,
    const sed::TVector& movingRailB,
    const sed::TVector& targetRailA,
    const sed::TVector& targetRailB)
{
    return railMidpoint(targetRailA, targetRailB)
        - movingRotation.TransformVector(railMidpoint(movingRailA, movingRailB));
}

} // namespace

namespace sed::thing
{

bool finiteThingTransform(const Thing& thing) noexcept
{
    return std::isfinite(thing.x)
        && std::isfinite(thing.y)
        && std::isfinite(thing.z)
        && std::isfinite(thing.pch)
        && std::isfinite(thing.yaw)
        && std::isfinite(thing.rol);
}

std::optional<Thing> thingAtFramePose(const Thing& owner, const TplValue& frameValue)
{
    if (frameValue.type != ValueType::frame
        || !sed::text::iequalsAscii(frameValue.name, "frame"))
    {
        return std::nullopt;
    }

    double x{};
    double y{};
    double z{};
    double pch{};
    double yaw{};
    double rol{};
    if (!frameValue.tryGetFrame(x, y, z, pch, yaw, rol))
    {
        return std::nullopt;
    }

    Thing posed = owner;
    posed.x = x;
    posed.y = y;
    posed.z = z;
    posed.pch = pch;
    posed.yaw = yaw;
    posed.rol = rol;
    if (!finiteThingTransform(posed))
    {
        return std::nullopt;
    }
    return posed;
}

std::vector<std::int32_t> uniqueValidThingIndices(
    const std::vector<std::int32_t>& highlightedIndices,
    const std::int32_t activeThingIndex,
    const std::size_t thingCount)
{
    std::vector<std::int32_t> indices{};
    indices.reserve(highlightedIndices.size() + 1);

    for (const std::int32_t idx : highlightedIndices)
    {
        appendUniqueValidThingIndex(indices, idx, thingCount);
    }
    appendUniqueValidThingIndex(indices, activeThingIndex, thingCount);
    return indices;
}

std::vector<std::int32_t> uniqueValidThingIndices(
    const std::unordered_set<std::int32_t>& highlightedIndices,
    const std::int32_t activeThingIndex,
    const std::size_t thingCount)
{
    std::vector<std::int32_t> indices{};
    indices.reserve(highlightedIndices.size() + 1);

    for (const std::int32_t idx : highlightedIndices)
    {
        appendUniqueValidThingIndex(indices, idx, thingCount);
    }
    appendUniqueValidThingIndex(indices, activeThingIndex, thingCount);
    return indices;
}

bool containsThingIndex(const std::vector<std::int32_t>& thingIndices, const std::int32_t thingIndex)
{
    return std::find(thingIndices.begin(), thingIndices.end(), thingIndex) != thingIndices.end();
}

BoxMinMax boxMinMax(const ThingBox& box)
{
    return BoxMinMax{
        std::array<double, 3>{
            std::min(box.x1, box.x2),
            std::min(box.y1, box.y2),
            std::min(box.z1, box.z2),
        },
        std::array<double, 3>{
            std::max(box.x1, box.x2),
            std::max(box.y1, box.y2),
            std::max(box.z1, box.z2),
        }
    };
}

std::array<double, 3> boxCenter(const BoxMinMax& bounds)
{
    return {
        (bounds.min[0] + bounds.max[0]) * 0.5,
        (bounds.min[1] + bounds.max[1]) * 0.5,
        (bounds.min[2] + bounds.max[2]) * 0.5,
    };
}

double vectorComponent(const TVector& v, const int axis)
{
    switch (axis)
    {
        case 0:  return v.x;
        case 1:  return v.y;
        default: return v.z;
    }
}

void setVectorComponent(TVector& v, const int axis, const double value)
{
    switch (axis)
    {
        case 0:  v.x = value; break;
        case 1:  v.y = value; break;
        default: v.z = value; break;
    }
}

double outsideDistance(const double value, const double minValue, const double maxValue)
{
    if (value < minValue)
    {
        return minValue - value;
    }
    if (value > maxValue)
    {
        return value - maxValue;
    }
    return 0.0;
}

float intervalOutsideDistance(const float minA, const float maxA, const float minB, const float maxB)
{
    if (maxA < minB)
    {
        return minB - maxA;
    }
    if (maxB < minA)
    {
        return minA - maxB;
    }
    return 0.0f;
}

double intervalOutsideDistance(const double minA, const double maxA, const double minB, const double maxB)
{
    if (maxA < minB) return minB - maxA;
    if (maxB < minA) return minA - maxB;
    return 0.0;
}

void addPointToBounds(VectorBounds3& bounds, const QVector3D& point)
{
    if (!bounds.valid)
    {
        bounds.min = point;
        bounds.max = point;
        bounds.valid = true;
        return;
    }

    bounds.min.setX(std::min(bounds.min.x(), point.x()));
    bounds.min.setY(std::min(bounds.min.y(), point.y()));
    bounds.min.setZ(std::min(bounds.min.z(), point.z()));
    bounds.max.setX(std::max(bounds.max.x(), point.x()));
    bounds.max.setY(std::max(bounds.max.y(), point.y()));
    bounds.max.setZ(std::max(bounds.max.z(), point.z()));
}

void addPointToBounds(LocalBounds3& bounds, const TVector& point)
{
    const std::array<double, 3> p{ point.x, point.y, point.z };
    if (!bounds.valid)
    {
        bounds.min = p;
        bounds.max = p;
        bounds.valid = true;
        return;
    }

    for (int axis = 0; axis < 3; ++axis)
    {
        bounds.min[static_cast<std::size_t>(axis)] =
            std::min(bounds.min[static_cast<std::size_t>(axis)], p[static_cast<std::size_t>(axis)]);
        bounds.max[static_cast<std::size_t>(axis)] =
            std::max(bounds.max[static_cast<std::size_t>(axis)], p[static_cast<std::size_t>(axis)]);
    }
}

float vectorComponent(const QVector3D& v, const int axis)
{
    switch (axis)
    {
        case 0:  return v.x();
        case 1:  return v.y();
        default: return v.z();
    }
}

QVector3D worldAxisVector(const int axis)
{
    switch (axis)
    {
        case 0:  return QVector3D(1.0f, 0.0f, 0.0f);
        case 1:  return QVector3D(0.0f, 1.0f, 0.0f);
        default: return QVector3D(0.0f, 0.0f, 1.0f);
    }
}

VectorBounds3 transformedThingCornerBounds(
    const std::vector<std::int32_t>& thingIndices,
    const QVector3D& translation,
    const float yawDeltaDeg,
    const std::function<std::vector<QVector3D>(std::int32_t, const QVector3D&, float)>& worldCornersForThing)
{
    VectorBounds3 out{};
    for (const std::int32_t idx : thingIndices)
    {
        for (const QVector3D& corner : worldCornersForThing(idx, translation, yawDeltaDeg))
        {
            addPointToBounds(out, corner);
        }
    }
    return out;
}

TVector matrixAxisVector(const TMatrix43& matrix, const int axis)
{
    switch (axis)
    {
        case 0:  return matrix.rvec;
        case 1:  return matrix.lvec;
        default: return matrix.uvec;
    }
}

std::optional<TVector> flattenNormalize(TVector v)
{
    v.z = 0.0;
    if (v.Normalize() == 0.0)
    {
        return std::nullopt;
    }
    return v;
}

double yawDeltaDeg(const TVector& from, const TVector& to)
{
    const double cross = from.x * to.y - from.y * to.x;
    const double dot = std::clamp(from.x * to.x + from.y * to.y, -1.0, 1.0);
    return std::atan2(cross, dot) * 180.0 / std::numbers::pi;
}

bool connectorHasRailPair(const SnapConnector& connector) noexcept
{
    return connector.railA.DistanceSqr(connector.railB) > 1.0e-8;
}

double connectorSnapProbeDistanceSq(
    const double connectorDistanceSq,
    const double cursorDistanceSq,
    const bool requireConnectorProximity) noexcept
{
    if (requireConnectorProximity)
    {
        return connectorDistanceSq;
    }
    return std::min(connectorDistanceSq, cursorDistanceSq * 0.45);
}

double connectorSnapReach(
    const double unitsPerPixel,
    const bool railPairSnap,
    const double baseRadius) noexcept
{
    const double pixelScale = std::isfinite(unitsPerPixel)
        ? std::max(0.0, unitsPerPixel)
        : 0.0;
    const double radius = std::clamp(
        std::isfinite(baseRadius) ? baseRadius : 0.16,
        0.000001,
        1000.0);
    if (railPairSnap)
    {
        return std::clamp(
            std::max(radius * 1.125, pixelScale * 64.0),
            radius,
            radius * 2.625);
    }

    return std::clamp(
        std::max(radius * 0.375, pixelScale * 32.0),
        radius * 0.34375,
        radius * 1.375);
}

double railTrackProjectionTolerance(const double projectionExtent) noexcept
{
    return std::clamp(
        projectionExtent * kRailTrackProjectionToleranceScale,
        kRailTrackProjectionToleranceMin,
        kRailTrackProjectionToleranceMax);
}

double railTrackMaxSideAxisSpan(const double projectionExtent) noexcept
{
    return std::max(
        kRailTrackMaxSideAxisSpanMin,
        projectionExtent * kRailTrackMaxSideAxisSpanScale);
}

double railTrackMaxRailPairSpan(const double modelExtent) noexcept
{
    return std::max(
        kRailTrackMaxRailPairSpanMin,
        std::min(kRailTrackMaxRailPairSpanMax, modelExtent * kRailTrackMaxRailPairSpanScale));
}

double railTrackSideLateralPad(const double railPairSpan) noexcept
{
    return std::clamp(
        railPairSpan * kRailTrackSideLateralPadScale,
        kRailTrackSideLateralPadMin,
        kRailTrackSideLateralPadMax);
}

double railTrackIdealRailPairSpan(const double modelExtent) noexcept
{
    return std::clamp(
        modelExtent * kRailTrackIdealRailPairSpanScale,
        kRailTrackIdealRailPairSpanMin,
        kRailTrackIdealRailPairSpanMax);
}

double railTrackSideCandidateScore(
    const double axisSpan,
    const double railPairSpan,
    const double modelExtent,
    const int sidePointCount) noexcept
{
    return axisSpan * kRailTrackSideScoreAxisSpanWeight
        + std::abs(railPairSpan - railTrackIdealRailPairSpan(modelExtent)) * kRailTrackSideScoreGaugeWeight
        - static_cast<double>(sidePointCount) * kRailTrackSideScorePointReward;
}

std::vector<SnapConnector> selectBestDistinctRailTrackConnectors(
    std::vector<ScoredSnapConnector> candidates)
{
    std::sort(candidates.begin(), candidates.end(), [](const ScoredSnapConnector& a, const ScoredSnapConnector& b)
        {
            return a.score < b.score;
        });

    std::vector<SnapConnector> result{};
    result.reserve(std::min<std::size_t>(candidates.size(), kMaxRailTrackSnapConnectors));

    // Walk candidates from best to worst, rejecting only nearby connectors that
    // point in nearly the same direction. Opposite sides can share space in
    // curved models, so tangent agreement is part of the duplicate test.
    for (const ScoredSnapConnector& candidate : candidates)
    {
        bool duplicate = false;
        for (const SnapConnector& existing : result)
        {
            if (existing.position.Distance(candidate.connector.position) <= kRailTrackConnectorDuplicateDistance
                && existing.tangent.Dot(candidate.connector.tangent) >= kRailTrackConnectorDuplicateTangentDot)
            {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
        {
            continue;
        }

        result.push_back(candidate.connector);
        if (result.size() >= kMaxRailTrackSnapConnectors)
        {
            break;
        }
    }

    return result;
}

std::vector<RailPairSnapCandidate> buildRailPairSnapCandidates(
    const SnapConnector& movingConnector,
    const SnapConnector& anchorConnector,
    const TVector& basePyr,
    const TMatrix43& baseRotation,
    const TMatrix43& anchorTransform,
    const TVector& desiredMovingTangent,
    const bool rotateToAlign,
    const double minimumTangentDot,
    const double maximumRotationDegrees,
    const bool viewportFloatYaw)
{
    if (!connectorHasRailPair(movingConnector) || !connectorHasRailPair(anchorConnector))
    {
        return {};
    }

    const std::optional<TVector> desiredTangent =
        rotateToAlign ? flattenNormalize(desiredMovingTangent) : std::optional<TVector>{};
    if (rotateToAlign && !desiredTangent)
    {
        return {};
    }

    const TVector anchorRailA = anchorTransform.TransformPoint(anchorConnector.railA);
    const TVector anchorRailB = anchorTransform.TransformPoint(anchorConnector.railB);
    const std::array<std::pair<TVector, TVector>, 2> anchorRailPairs = {
        std::pair<TVector, TVector>{ anchorRailA, anchorRailB },
        std::pair<TVector, TVector>{ anchorRailB, anchorRailA },
    };
    const std::array<std::pair<TVector, TVector>, 2> movingRailPairs = {
        std::pair<TVector, TVector>{ movingConnector.railA, movingConnector.railB },
        std::pair<TVector, TVector>{ movingConnector.railB, movingConnector.railA },
    };

    std::vector<RailPairSnapCandidate> candidates{};
    candidates.reserve(anchorRailPairs.size() * movingRailPairs.size());

    for (const auto& [targetRailA, targetRailB] : anchorRailPairs)
    {
        for (const auto& [movingRailA, movingRailB] : movingRailPairs)
        {
            TVector newPyr = basePyr;
            TMatrix43 newRot = baseRotation;
            double tangentPenalty = 0.0;
            double tangentDot = 1.0;

            bool rotatedToAlign = false;
            if (rotateToAlign)
            {
                std::optional<TVector> movingTangentWorld =
                    flattenNormalize(newRot.TransformVector(movingConnector.tangent));
                if (!movingTangentWorld)
                {
                    continue;
                }

                double yawDelta = yawDeltaDeg(*movingTangentWorld, *desiredTangent);
                if (viewportFloatYaw)
                {
                    yawDelta = static_cast<float>(yawDelta);
                }
                if (std::abs(yawDelta)
                    <= std::clamp(maximumRotationDegrees, 0.0, 180.0)
                        + 1.0e-8)
                {
                    newPyr.yaw += yawDelta;
                    newRot = TMatrix43::Create(TVector::zero, newPyr);
                    rotatedToAlign = true;
                }

                if (rotatedToAlign)
                {
                    movingTangentWorld = flattenNormalize(
                        newRot.TransformVector(movingConnector.tangent));
                    if (!movingTangentWorld)
                    {
                        continue;
                    }

                    tangentDot = std::clamp(
                        movingTangentWorld->Dot(*desiredTangent), -1.0, 1.0);
                    if (tangentDot < minimumTangentDot)
                    {
                        continue;
                    }
                    tangentPenalty = (1.0 - tangentDot)
                        * (1.0 - tangentDot) * 0.025;
                }
            }
            if (!rotatedToAlign)
            {
                const TVector currentGauge = baseRotation.TransformVector(movingRailB - movingRailA);
                const TVector targetGauge = targetRailB - targetRailA;
                tangentPenalty = currentGauge.DistanceSqr(targetGauge) * 2.0;
            }

            TVector newOrigin = targetRailA - newRot.TransformVector(movingRailA);
            double pairResidualSq = transformedRailPairResidualSq(
                newOrigin,
                newRot,
                movingRailA,
                movingRailB,
                targetRailA,
                targetRailB);

            if (!std::isfinite(pairResidualSq)
                || !std::isfinite(tangentPenalty))
            {
                continue;
            }

            if (pairResidualSq > sed::math::kGeometryEpsilon)
            {
                if (!railGaugeDirectionsAreCompatible(
                        newRot,
                        movingRailA,
                        movingRailB,
                        targetRailA,
                        targetRailB))
                {
                    continue;
                }

                newOrigin = railCenterlineOrigin(
                    newRot,
                    movingRailA,
                    movingRailB,
                    targetRailA,
                    targetRailB);
                pairResidualSq = transformedRailPairResidualSq(
                    newOrigin,
                    newRot,
                    movingRailA,
                    movingRailB,
                    targetRailA,
                    targetRailB);
                if (!std::isfinite(pairResidualSq)
                    || pairResidualSq > kCompatibleRailPairResidualSq)
                {
                    continue;
                }
            }

            RailPairSnapCandidate candidate{};
            candidate.origin = newOrigin;
            candidate.pyr = newPyr;
            candidate.rotation = newRot;
            candidate.targetRailA = targetRailA;
            candidate.targetRailB = targetRailB;
            candidate.movingRailA = movingRailA;
            candidate.movingRailB = movingRailB;
            candidate.pairResidualSq = pairResidualSq;
            candidate.tangentPenalty = tangentPenalty;
            candidate.tangentDot = tangentDot;
            candidates.push_back(candidate);
        }
    }

    return candidates;
}

std::array<TVector, 8> thingBoxCorners(const ThingBox& box)
{
    const double x1 = std::min(box.x1, box.x2);
    const double x2 = std::max(box.x1, box.x2);
    const double y1 = std::min(box.y1, box.y2);
    const double y2 = std::max(box.y1, box.y2);
    const double z1 = std::min(box.z1, box.z2);
    const double z2 = std::max(box.z1, box.z2);
    return std::array<TVector, 8>{
        TVector{ x1, y1, z1 },
        TVector{ x2, y1, z1 },
        TVector{ x2, y2, z1 },
        TVector{ x1, y2, z1 },
        TVector{ x1, y1, z2 },
        TVector{ x2, y1, z2 },
        TVector{ x2, y2, z2 },
        TVector{ x1, y2, z2 },
    };
}

void appendThingBoxFaceCenters(std::vector<TVector>& points, const ThingBox& box)
{
    const double x1 = std::min(box.x1, box.x2);
    const double x2 = std::max(box.x1, box.x2);
    const double y1 = std::min(box.y1, box.y2);
    const double y2 = std::max(box.y1, box.y2);
    const double z1 = std::min(box.z1, box.z2);
    const double z2 = std::max(box.z1, box.z2);
    const double cx = (x1 + x2) * 0.5;
    const double cy = (y1 + y2) * 0.5;
    const double cz = (z1 + z2) * 0.5;
    points.push_back(TVector{ x1, cy, cz });
    points.push_back(TVector{ x2, cy, cz });
    points.push_back(TVector{ cx, y1, cz });
    points.push_back(TVector{ cx, y2, cz });
    points.push_back(TVector{ cx, cy, z1 });
    points.push_back(TVector{ cx, cy, z2 });
}

QVector3D rotatePointZ(const QVector3D& point, const QVector3D& center, const float angleDeg)
{
    if (angleDeg == 0.0f)
    {
        return point;
    }

    const float a = angleDeg * std::numbers::pi_v<float> / 180.0f;
    const float cs = std::cos(a);
    const float sn = std::sin(a);
    const QVector3D d = point - center;
    return QVector3D(
        center.x() + d.x() * cs - d.y() * sn,
        center.y() + d.x() * sn + d.y() * cs,
        point.z());
}

std::vector<TVector> transformThingBoxDocumentPoints(
    const Thing& thing, const ThingBox& box, const TVector& pivot,
    const TVector& translation, const double yawDeltaDeg, const bool includeFaceCenters)
{
    TVector pyr{};
    pyr.pitch = thing.pch;
    pyr.yaw = thing.yaw + yawDeltaDeg;
    pyr.roll = thing.rol;
    const double angle = yawDeltaDeg * std::numbers::pi / 180.0;
    const double sine = std::sin(angle);
    const double cosine = std::cos(angle);
    const TVector relative{thing.x - pivot.x, thing.y - pivot.y, thing.z - pivot.z};
    const TVector origin = pivot + translation + TVector{
        relative.x * cosine - relative.y * sine,
        relative.x * sine + relative.y * cosine, relative.z};
    const TMatrix43 xf = TMatrix43::Create(origin, pyr);
    const auto corners = thingBoxCorners(box);
    std::vector<TVector> points(corners.begin(), corners.end());
    if (includeFaceCenters) appendThingBoxFaceCenters(points, box);
    for (TVector& point : points) point = xf.TransformPoint(point);
    return points;
}

std::vector<QVector3D> transformThingBoxPoints(
    const Thing& thing,
    const ThingBox& box,
    const QVector3D& pivot,
    const QVector3D& translation,
    const float yawDeltaDeg,
    const bool includeFaceCenters)
{
    std::vector<QVector3D> out{};

    TVector pyr{};
    pyr.pitch = thing.pch;
    pyr.yaw = thing.yaw + static_cast<double>(yawDeltaDeg);
    pyr.roll = thing.rol;

    const QVector3D rotatedOrigin = rotatePointZ(
        QVector3D(static_cast<float>(thing.x), static_cast<float>(thing.y), static_cast<float>(thing.z)),
        pivot,
        yawDeltaDeg);
    const TVector origin{
        static_cast<double>(rotatedOrigin.x() + translation.x()),
        static_cast<double>(rotatedOrigin.y() + translation.y()),
        static_cast<double>(rotatedOrigin.z() + translation.z())
    };
    const TMatrix43 xf = TMatrix43::Create(origin, pyr);

    std::vector<TVector> localPoints{};
    const std::array<TVector, 8> corners = thingBoxCorners(box);
    localPoints.reserve(includeFaceCenters ? 14 : 8);
    localPoints.insert(localPoints.end(), corners.begin(), corners.end());
    if (includeFaceCenters)
    {
        appendThingBoxFaceCenters(localPoints, box);
    }

    out.reserve(localPoints.size());
    for (const TVector& point : localPoints)
    {
        const TVector transformed = xf.TransformPoint(point);
        out.emplace_back(
            static_cast<float>(transformed.x),
            static_cast<float>(transformed.y),
            static_cast<float>(transformed.z));
    }
    return out;
}

std::vector<QVector3D> resolveThingWorldCorners(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    const std::int32_t thingIndex,
    const QVector3D& pivot,
    const QVector3D& translation,
    const float yawDeltaDeg,
    const double fallbackHalfSize)
{
    if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= level.things.size())
    {
        return {};
    }

    const Thing& thing = level.things[static_cast<std::size_t>(thingIndex)];
    const std::optional<ThingBox> box = resolveThingLocalBox(
        level,
        templates,
        resources,
        thing,
        fallbackHalfSize);
    if (!box)
    {
        return {};
    }

    return transformThingBoxPoints(thing, *box, pivot, translation, yawDeltaDeg, false);
}

VectorBounds3 transformedLevelThingCornerBounds(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    const std::vector<std::int32_t>& thingIndices,
    const QVector3D& pivot,
    const QVector3D& translation,
    const float yawDeltaDeg,
    const double fallbackHalfSize)
{
    VectorBounds3 out{};
    for (const std::int32_t thingIndex : thingIndices)
    {
        for (const QVector3D& corner : resolveThingWorldCorners(
                 level,
                 templates,
                 resources,
                 thingIndex,
                 pivot,
                 translation,
                 yawDeltaDeg,
                 fallbackHalfSize))
        {
            addPointToBounds(out, corner);
        }
    }
    return out;
}

std::optional<ThingBox> resolveThingModelBox(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    const Thing& thing)
{
    if (!resources)
    {
        return std::nullopt;
    }
    return loadModel3doBox(*resources, thingModel3doName(level, templates, thing));
}

std::optional<TVector> surfaceNormalForHit(const Level& level, const sed::level::SurfacePickHit& hit)
{
    if (hit.surface.sector < 0
        || static_cast<std::size_t>(hit.surface.sector) >= level.sectors.size())
    {
        return std::nullopt;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(hit.surface.sector)];
    if (hit.surface.surface < 0
        || static_cast<std::size_t>(hit.surface.surface) >= sector.surfaces.size())
    {
        return std::nullopt;
    }

    const Surface& surface = sector.surfaces[static_cast<std::size_t>(hit.surface.surface)];
    TVector normal = surface.normal;
    if (normal.IsZero())
    {
        normal = sed::level::surfaceCalcNormalNewell(sector, surface);
    }
    if (normal.Normalize() == 0.0)
    {
        return std::nullopt;
    }
    return normal;
}

bool isPassThroughPlacementSurface(const Level& level, const sed::level::SurfacePickHit& hit)
{
    if (hit.surface.sector < 0
        || static_cast<std::size_t>(hit.surface.sector) >= level.sectors.size())
    {
        return false;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(hit.surface.sector)];
    if (hit.surface.surface < 0
        || static_cast<std::size_t>(hit.surface.surface) >= sector.surfaces.size())
    {
        return false;
    }

    const Surface& surface = sector.surfaces[static_cast<std::size_t>(hit.surface.surface)];
    if (!surface.adjoin)
    {
        return false;
    }

    const bool noCollision = (surface.surfaceFlags & SF_Collision) == 0;
    const bool notSolidGeo = surface.geo < static_cast<std::int32_t>(TGeoMode::Solid);
    const std::string material = sed::text::lowerAscii(sed::text::trimView(surface.material));
    const bool noMaterial = material.empty() || material == "-" || material == "none";
    return noCollision && (notSolidGeo || noMaterial);
}

std::optional<ThingBox> resolveThingLocalBox(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    const Thing& thing,
    const double fallbackHalfSize)
{
    if (resources)
    {
        if (const std::optional<ThingBox> box =
                loadModel3doBox(*resources, thingModel3doName(level, templates, thing)))
        {
            return box;
        }

        if (const std::optional<ThingBox> box =
                loadSpriteBox(*resources, tplValueString(sed::level::thingFindValue(level, templates, thing, "sprite"))))
        {
            return box;
        }
    }

    if (!thing.bbox.IsZero())
    {
        return thing.bbox;
    }

    std::string current = thingTemplateKey(thing);
    for (int depth = 0; depth < 256 && !current.empty(); ++depth)
    {
        const ThingTemplate* tpl = nullptr;
        for (const ThingTemplate& localTpl : level.templates)
        {
            if (sed::text::iequalsAscii(localTpl.name, current))
            {
                tpl = &localTpl;
                break;
            }
        }
        if (!tpl && templates)
        {
            tpl = templates->find(current);
        }
        if (!tpl)
        {
            break;
        }

        if (!tpl->bbox.IsZero())
        {
            return tpl->bbox;
        }
        if (tpl->parent.empty() || sed::text::iequalsAscii(tpl->parent, "none"))
        {
            break;
        }
        current = tpl->parent;
    }

    ThingBox fallback{};
    fallback.Set(
        -fallbackHalfSize,
        -fallbackHalfSize,
        -fallbackHalfSize,
        fallbackHalfSize,
        fallbackHalfSize,
        fallbackHalfSize);
    return fallback;
}

std::vector<TVector> resolveThingDocumentSupportPoints(
    const Level& level, const assets::Templates* templates,
    const assets::ResourceLocator* resources, const Thing& posedThing,
    const double fallbackHalfSize)
{
    if (!finiteThingTransform(posedThing)) return {};
    const TVector origin{posedThing.x, posedThing.y, posedThing.z};
    if (const auto box = resolveThingLocalBox(level, templates, resources, posedThing, fallbackHalfSize))
        return transformThingBoxDocumentPoints(posedThing, *box, origin, TVector{}, 0.0, true);
    return {origin};
}

std::vector<QVector3D> resolveThingWorldSupportPoints(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    const Thing& posedThing,
    const double fallbackHalfSize)
{
    const QVector3D origin(
        static_cast<float>(posedThing.x),
        static_cast<float>(posedThing.y),
        static_cast<float>(posedThing.z));
    if (!finiteThingTransform(posedThing))
    {
        return {};
    }

    if (const std::optional<ThingBox> box = resolveThingLocalBox(
            level, templates, resources, posedThing, fallbackHalfSize))
    {
        return transformThingBoxPoints(
            posedThing, *box, origin, QVector3D{}, 0.0f, true);
    }

    return { origin };
}

std::vector<SnapConnector> extractRailTrackConnectorsFromModelText(const std::string_view modelText)
{
    std::vector<ScoredSnapConnector> candidates{};
    try
    {
        std::istringstream iss{std::string(modelText)};
        const assets::Model3do model = assets::loadFrom3DO(iss);
        std::vector<TVector> points{};
        std::vector<std::pair<int, int>> edges{};
        for (const assets::Model3doMesh& mesh : model.meshes)
        {
            const int meshPointBase = static_cast<int>(points.size());
            for (const assets::Model3doVertex& vertex : mesh.vertices)
            {
                if (std::isfinite(vertex.x) && std::isfinite(vertex.y) && std::isfinite(vertex.z))
                {
                    points.push_back(TVector{ vertex.x, vertex.y, vertex.z });
                }
            }
            if (points.size() == static_cast<std::size_t>(meshPointBase) + mesh.vertices.size())
            {
                for (const assets::Model3doFace& face : mesh.faces)
                {
                    if (face.vertices.size() < 2)
                    {
                        continue;
                    }
                    for (std::size_t i = 0; i < face.vertices.size(); ++i)
                    {
                        const std::int32_t a = face.vertices[i];
                        const std::int32_t b = face.vertices[(i + 1) % face.vertices.size()];
                        if (a < 0 || b < 0
                            || static_cast<std::size_t>(a) >= mesh.vertices.size()
                            || static_cast<std::size_t>(b) >= mesh.vertices.size())
                        {
                            continue;
                        }
                        edges.emplace_back(meshPointBase + static_cast<int>(a), meshPointBase + static_cast<int>(b));
                    }
                }
            }
        }

        if (points.size() < kRailTrackMinModelPoints)
        {
            return {};
        }

        double minX = points.front().x;
        double maxX = points.front().x;
        double minY = points.front().y;
        double maxY = points.front().y;
        for (const TVector& point : points)
        {
            minX = std::min(minX, point.x);
            maxX = std::max(maxX, point.x);
            minY = std::min(minY, point.y);
            maxY = std::max(maxY, point.y);
        }

        const double modelExtent = std::max(maxX - minX, maxY - minY);
        if (modelExtent < kRailTrackMinModelExtent)
        {
            return {};
        }

        struct RailSum
        {
            int count{};
            double x{};
            double y{};
            double z{};
        };

        struct SidePoint
        {
            int index{};
            TVector point{};
            double pperp{};
        };

        auto addSideCandidate =
            [&](const TVector axis, const double sideProj, const double minProj, const double maxProj)
        {
            const double extent = maxProj - minProj;
            if (extent < kRailTrackMinProjectionExtent)
            {
                return;
            }

            const double eps = railTrackProjectionTolerance(extent);
            const TVector perp{ -axis.y, axis.x, 0.0 };

            int count = 0;
            double minAxis = std::numeric_limits<double>::infinity();
            double maxAxis = -std::numeric_limits<double>::infinity();
            double minPerp = std::numeric_limits<double>::infinity();
            double maxPerp = -std::numeric_limits<double>::infinity();
            std::vector<SidePoint> sidePoints{};
            for (int pointIndex = 0; pointIndex < static_cast<int>(points.size()); ++pointIndex)
            {
                const TVector& point = points[static_cast<std::size_t>(pointIndex)];
                const double proj = point.x * axis.x + point.y * axis.y;
                if (std::abs(proj - sideProj) > eps)
                {
                    continue;
                }

                const double pperp = point.x * perp.x + point.y * perp.y;
                ++count;
                minAxis = std::min(minAxis, proj);
                maxAxis = std::max(maxAxis, proj);
                minPerp = std::min(minPerp, pperp);
                maxPerp = std::max(maxPerp, pperp);
                sidePoints.push_back(SidePoint{ pointIndex, point, pperp });
            }

            if (count < kRailTrackMinSidePointCount)
            {
                return;
            }

            const double axisSpan = maxAxis - minAxis;
            const double perpSpan = maxPerp - minPerp;
            if (axisSpan > railTrackMaxSideAxisSpan(extent)
                || perpSpan < kRailTrackMinRailPairSpan
                || perpSpan > railTrackMaxRailPairSpan(modelExtent))
            {
                return;
            }

            const double lateralPad = railTrackSideLateralPad(perpSpan);
            double nearestNegative = std::numeric_limits<double>::infinity();
            double nearestPositive = std::numeric_limits<double>::infinity();
            for (const TVector& point : points)
            {
                const double proj = point.x * axis.x + point.y * axis.y;
                const double delta = proj - sideProj;
                if (std::abs(delta) <= eps)
                {
                    continue;
                }

                const double pperp = point.x * perp.x + point.y * perp.y;
                if (pperp < minPerp - lateralPad || pperp > maxPerp + lateralPad)
                {
                    continue;
                }

                if (delta < 0.0)
                {
                    const double d = -delta;
                    nearestNegative = std::min(nearestNegative, d);
                }
                else
                {
                    nearestPositive = std::min(nearestPositive, delta);
                }
            }

            const bool hasNegativeSide = std::isfinite(nearestNegative);
            const bool hasPositiveSide = std::isfinite(nearestPositive);
            if (hasNegativeSide == hasPositiveSide)
            {
                return;
            }

            RailSum railA{};
            RailSum railB{};
            const double splitPerp = (minPerp + maxPerp) * 0.5;
            std::vector<unsigned char> sideRail(points.size(), 0);
            for (const SidePoint& sidePoint : sidePoints)
            {
                RailSum& rail = sidePoint.pperp <= splitPerp ? railA : railB;
                sideRail[static_cast<std::size_t>(sidePoint.index)] = sidePoint.pperp <= splitPerp ? 1 : 2;
                ++rail.count;
                rail.x += sidePoint.point.x;
                rail.y += sidePoint.point.y;
                rail.z += sidePoint.point.z;
            }
            if (railA.count < kRailTrackMinPointsPerRailStrand
                || railB.count < kRailTrackMinPointsPerRailStrand)
            {
                return;
            }

            for (const auto& [a, b] : edges)
            {
                const unsigned char sideA = sideRail[static_cast<std::size_t>(a)];
                const unsigned char sideB = sideRail[static_cast<std::size_t>(b)];
                if (sideA != 0 && sideB != 0 && sideA != sideB)
                {
                    return;
                }
            }

            SnapConnector connector{};
            connector.railA = TVector{
                railA.x / static_cast<double>(railA.count),
                railA.y / static_cast<double>(railA.count),
                railA.z / static_cast<double>(railA.count)
            };
            connector.railB = TVector{
                railB.x / static_cast<double>(railB.count),
                railB.y / static_cast<double>(railB.count),
                railB.z / static_cast<double>(railB.count)
            };
            connector.position = (connector.railA + connector.railB).Scaled(0.5);
            connector.tangent = hasNegativeSide ? axis : -axis;
            if (connector.tangent.Normalize() == 0.0)
            {
                return;
            }

            ScoredSnapConnector candidate{};
            candidate.connector = connector;
            candidate.score = railTrackSideCandidateScore(axisSpan, perpSpan, modelExtent, count);

            for (ScoredSnapConnector& existing : candidates)
            {
                const double dist = existing.connector.position.Distance(connector.position);
                const double dot = existing.connector.tangent.Dot(connector.tangent);
                if (dist <= kRailTrackLocalCandidateReplaceDistance
                    && dot >= kRailTrackLocalCandidateReplaceTangentDot)
                {
                    if (candidate.score < existing.score)
                    {
                        existing = candidate;
                    }
                    return;
                }
            }
            candidates.push_back(candidate);
        };

        for (int deg = 0; deg < kRailTrackScanMaxAngleDeg; deg += kRailTrackScanAngleStepDeg)
        {
            const double a = (static_cast<double>(deg) * std::numbers::pi) / 180.0;
            const TVector axis{ std::cos(a), std::sin(a), 0.0 };
            double minProj = std::numeric_limits<double>::infinity();
            double maxProj = -std::numeric_limits<double>::infinity();
            std::vector<double> projections{};
            projections.reserve(points.size());
            for (const TVector& point : points)
            {
                const double proj = point.x * axis.x + point.y * axis.y;
                minProj = std::min(minProj, proj);
                maxProj = std::max(maxProj, proj);
                projections.push_back(proj);
            }

            const double extent = maxProj - minProj;
            if (extent < kRailTrackMinProjectionExtent)
            {
                continue;
            }

            const double eps = railTrackProjectionTolerance(extent);
            std::sort(projections.begin(), projections.end());
            for (std::size_t i = 0; i < projections.size();)
            {
                std::size_t j = i + 1;
                double sum = projections[i];
                while (j < projections.size() && projections[j] - projections[i] <= eps)
                {
                    sum += projections[j];
                    ++j;
                }

                const double sideProj = sum / static_cast<double>(j - i);
                addSideCandidate(axis, sideProj, minProj, maxProj);
                i = j;
            }
        }
    }
    catch (...)
    {
        return {};
    }

    return selectBestDistinctRailTrackConnectors(std::move(candidates));
}

std::vector<SnapConnector> resolveRailTrackConnectors(
    const assets::ResourceLocator& resources,
    const std::string_view modelName,
    const std::string_view thingName)
{
    const std::string normalizedModelName = normalizedModel3doName(modelName);
    if (normalizedModelName.empty()
        || !isRailOrTrackThingKey(normalizedModelName, thingName))
    {
        return {};
    }

    static ui::detail::BoundedTwoQueueCache<std::vector<SnapConnector>> cache(512u,
        [](const std::vector<SnapConnector>&) { return 1u; });
    const std::string modelKey = sed::text::upperAscii(normalizedModelName) + "|"
        + ui::detail::previewResourceRevisionToken(&resources, normalizedModelName, ".3do");
    std::vector<SnapConnector> result{};
    if (cache.find(modelKey, result))
    {
        return result;
    }
    try
    {
        if (const std::optional<std::string> text = resources.tryReadTextWithExt(normalizedModelName, ".3do"))
            result = extractRailTrackConnectorsFromModelText(*text);
    }
    catch (...)
    {
        result.clear();
    }

    cache.insertOrAssign(modelKey, result);
    return result;
}

std::vector<SnapConnector> resolveThingSnapConnectors(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    const Thing& thing)
{
    return resources
        ? resolveRailTrackConnectors(
            *resources,
            thingModel3doName(level, templates, thing),
            thingTemplateKey(thing))
        : std::vector<SnapConnector>{};
}

} // namespace sed::thing
