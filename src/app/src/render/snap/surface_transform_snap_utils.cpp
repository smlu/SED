#include "render/snap/surface_transform_snap_utils.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <tuple>

namespace sed::render
{
namespace
{

bool finiteVector(const QVector3D &value) noexcept
{
    return std::isfinite(value.x()) && std::isfinite(value.y()) && std::isfinite(value.z());
}

std::optional<QVector3D> normalized(const QVector3D &value) noexcept
{
    if ( !finiteVector(value) )
    {
        return std::nullopt;
    }
    const float lengthSquared = value.lengthSquared();
    if ( !std::isfinite(lengthSquared) || lengthSquared <= 1.0e-12f )
    {
        return std::nullopt;
    }
    return value / std::sqrt(lengthSquared);
}

std::optional<QVector3D> normalizedDocumentDirection(TVector value) noexcept
{
    const double length = std::hypot(value.x, value.y, value.z);
    if (!std::isfinite(length) || length == 0.0) return std::nullopt;
    value = TVector{value.x / length, value.y / length, value.z / length};
    return QVector3D(static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z));
}

TVector preciseDirectionCross(const QVector3D& a, const QVector3D& b) noexcept
{
    return TVector{static_cast<double>(a.y())*b.z()-static_cast<double>(a.z())*b.y(),
        static_cast<double>(a.z())*b.x()-static_cast<double>(a.x())*b.z(),
        static_cast<double>(a.x())*b.y()-static_cast<double>(a.y())*b.x()};
}

double preciseDirectionDot(const QVector3D& a, const QVector3D& b) noexcept
{
    return static_cast<double>(a.x())*b.x()+static_cast<double>(a.y())*b.y()+static_cast<double>(a.z())*b.z();
}

QVector3D constrainedCorrection(const QVector3D &correction, const QVector3D &constraintAxis) noexcept
{
    const std::optional<QVector3D> axis = normalized(constraintAxis);
    return axis ? *axis * QVector3D::dotProduct(correction, *axis) : correction;
}

} // namespace

std::int64_t surfaceSnapBroadPhaseCellCoordinate(const float value, const float cellSize) noexcept
{
    if ( !std::isfinite(value) || !std::isfinite(cellSize) || cellSize <= 0.0f )
    {
        return 0;
    }

    const double cell = std::floor(static_cast<double>(value) / static_cast<double>(cellSize));
    constexpr double minimum = static_cast<double>(std::numeric_limits<std::int64_t>::min());
    constexpr double maximum = static_cast<double>(std::numeric_limits<std::int64_t>::max());
    if ( cell <= minimum )
    {
        return std::numeric_limits<std::int64_t>::min();
    }
    if ( cell >= maximum )
    {
        return std::numeric_limits<std::int64_t>::max();
    }
    return static_cast<std::int64_t>(cell);
}

bool nearbyMoveOrientationAllowed(const SurfaceMoveSnapElement element, const bool rotateToNearbyEnabled,
                                  const QVector3D &constraintAxis) noexcept
{
    if ( !rotateToNearbyEnabled || !finiteVector(constraintAxis) )
    {
        return false;
    }
    if ( element == SurfaceMoveSnapElement::Face )
    {
        return true;
    }
    if ( element != SurfaceMoveSnapElement::Edge || !finiteVector(constraintAxis) )
    {
        return false;
    }
    return constraintAxis.lengthSquared() <= 1.0e-10f;
}

std::optional<QVector3D> constrainMoveSnapTranslation(const QVector3D &rawTranslation,
                                                      const QVector3D &snappedTranslation, const float maximumResidual,
                                                      const QVector3D &constraintAxis)
{
    if ( !finiteVector(rawTranslation) || !finiteVector(snappedTranslation) || !finiteVector(constraintAxis) ||
         !std::isfinite(maximumResidual) || maximumResidual < 0.0f )
    {
        return std::nullopt;
    }

    const std::optional<QVector3D> axis = normalized(constraintAxis);
    if ( !axis )
    {
        return snappedTranslation;
    }

    const QVector3D correction = snappedTranslation - rawTranslation;
    const QVector3D constrained = *axis * QVector3D::dotProduct(correction, *axis);
    const QVector3D residual = correction - constrained;
    const float residualSquared = residual.lengthSquared();
    const float maximumResidualSquared = maximumResidual * maximumResidual;
    if ( !std::isfinite(residualSquared) || !std::isfinite(maximumResidualSquared) ||
         residualSquared > maximumResidualSquared )
    {
        return std::nullopt;
    }
    return rawTranslation + constrained;
}

std::optional<QVector3D> snapVertexMoveTranslation(const QVector3D& movingPoint, const QVector3D& rawTranslation,
    const std::span<const SurfaceSnapVertex> stationaryVertices, const float captureDistance, const QVector3D& constraintAxis)
{
    return snapVertexMoveTranslation(SurfaceSnapVertex{movingPoint}, rawTranslation,
        stationaryVertices, captureDistance, constraintAxis);
}

std::optional<QVector3D> snapVertexMoveTranslation(const SurfaceSnapVertex& movingVertex, const QVector3D& rawTranslation,
    const std::span<const SurfaceSnapVertex> stationaryVertices, const float captureDistance,
    const QVector3D& constraintAxis, TVector* documentTranslation)
{
    return snapVertexMoveTranslation(movingVertex, TVector{rawTranslation.x(), rawTranslation.y(), rawTranslation.z()},
        stationaryVertices, captureDistance, constraintAxis, documentTranslation);
}

std::optional<QVector3D> snapVertexMoveTranslation(const SurfaceSnapVertex& movingVertex, const TVector& rawTranslation,
    const std::span<const SurfaceSnapVertex> stationaryVertices, const float captureDistance, const QVector3D& constraintAxis, TVector* documentTranslation)
{
    const TVector moving = movingVertex.documentPoint.value_or(
        TVector{movingVertex.point.x(), movingVertex.point.y(), movingVertex.point.z()});
    if (!std::isfinite(moving.x) || !std::isfinite(moving.y) || !std::isfinite(moving.z)
        || !std::isfinite(rawTranslation.x) || !std::isfinite(rawTranslation.y) || !std::isfinite(rawTranslation.z) || !finiteVector(constraintAxis)
        || !std::isfinite(captureDistance) || captureDistance <= 0.0f)
    {
        return std::nullopt;
    }
    const TVector raw = rawTranslation;
    const auto axis = normalized(constraintAxis);
    const double captureSquared = static_cast<double>(captureDistance) * captureDistance;
    double bestResidualSquared = std::numeric_limits<double>::infinity();
    double bestCorrectionSquared = std::numeric_limits<double>::infinity();
    auto bestIdentity = std::tuple{std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::max()};
    std::optional<QVector3D> best{};
    for (const SurfaceSnapVertex& target : stationaryVertices)
    {
        const TVector destination = target.documentPoint.value_or(TVector{target.point.x(), target.point.y(), target.point.z()});
        const TVector displacement = destination - moving;
        const TVector requested = displacement - raw;
        TVector correction = requested;
        if (axis)
        {
            TVector direction{axis->x(), axis->y(), axis->z()};
            direction = direction * (1.0 / std::hypot(direction.x, direction.y, direction.z));
            correction = direction * requested.Dot(direction);
        }
        const double residualSquared = (requested - correction).LengthSqr();
        const double correctionSquared = correction.LengthSqr();
        if (!std::isfinite(residualSquared) || !std::isfinite(correctionSquared)
            || residualSquared > captureSquared || correctionSquared > captureSquared)
        {
            continue;
        }
        const auto identity = std::tuple{target.sector, target.vertex};
        const bool better = !axis ? correctionSquared < bestCorrectionSquared
                || (correctionSquared == bestCorrectionSquared && identity < bestIdentity)
            : residualSquared < bestResidualSquared
                || (residualSquared == bestResidualSquared && (correctionSquared < bestCorrectionSquared
                    || (correctionSquared == bestCorrectionSquared && identity < bestIdentity)));
        if (better)
        {
            const TVector result = axis ? raw + correction : displacement;
            const QVector3D presented(static_cast<float>(result.x), static_cast<float>(result.y), static_cast<float>(result.z));
            if (finiteVector(presented))
            {
                bestResidualSquared = residualSquared;
                bestCorrectionSquared = correctionSquared;
                bestIdentity = identity;
                best = presented;
                if (documentTranslation)
                {
                    *documentTranslation = result;
                }
            }
        }
    }
    return best;
}

std::optional<QVector3D> snapEdgeMoveTranslation(const SurfaceSnapEdge &movingEdge, const QVector3D &rawTranslation,
                                                 const std::span<const SurfaceSnapEdge> stationaryEdges,
                                                 const float captureDistance, const float minimumParallelCosine,
                                                 const QVector3D &constraintAxis)
{
    const std::optional<SurfaceEdgeMoveSnapResult> result = snapEdgeMoveTransform(
        movingEdge, rawTranslation, stationaryEdges, captureDistance, minimumParallelCosine, constraintAxis);
    return result ? std::optional<QVector3D>{result->translation} : std::nullopt;
}

std::optional<SurfaceEdgeMoveSnapResult>
snapEdgeMoveTransform(const SurfaceSnapEdge& movingEdge, const QVector3D& rawTranslation,
    const std::span<const SurfaceSnapEdge> stationaryEdges, const float captureDistance,
    const float minimumParallelCosine, const QVector3D& constraintAxis)
{
    return snapEdgeMoveTransform(movingEdge, TVector{rawTranslation.x(), rawTranslation.y(), rawTranslation.z()},
        stationaryEdges, captureDistance, minimumParallelCosine, constraintAxis);
}

std::optional<SurfaceEdgeMoveSnapResult>
snapEdgeMoveTransform(const SurfaceSnapEdge &movingEdge, const TVector &rawTranslation,
                      const std::span<const SurfaceSnapEdge> stationaryEdges, const float captureDistance,
                      const float minimumParallelCosine, const QVector3D &constraintAxis)
{
    if ( !finiteVector(movingEdge.first) || !finiteVector(movingEdge.second) || (!std::isfinite(rawTranslation.x) || !std::isfinite(rawTranslation.y) || !std::isfinite(rawTranslation.z)) ||
         !finiteVector(constraintAxis) || !std::isfinite(captureDistance) || captureDistance <= 0.0f )
    {
        return std::nullopt;
    }
    const TVector movingFirst = movingEdge.documentFirst.value_or(TVector{movingEdge.first.x(), movingEdge.first.y(), movingEdge.first.z()});
    const TVector movingSecond = movingEdge.documentSecond.value_or(TVector{movingEdge.second.x(), movingEdge.second.y(), movingEdge.second.z()});
    const TVector movingDifference = movingSecond - movingFirst;
    const std::optional<QVector3D> movingDirection = normalizedDocumentDirection(movingDifference);
    if ( !movingDirection )
    {
        return std::nullopt;
    }

    const TVector movingMidpoint = movingFirst + (movingSecond - movingFirst) * 0.5;
    const QVector3D rotationPivot(static_cast<float>(movingMidpoint.x), static_cast<float>(movingMidpoint.y), static_cast<float>(movingMidpoint.z));
    const double captureSquared = static_cast<double>(captureDistance) * captureDistance;
    const float safeParallelCosine =
        std::clamp(std::isfinite(minimumParallelCosine) ? minimumParallelCosine : 0.995f, 0.0f, 1.0f);
    double bestResidualSquared = std::numeric_limits<float>::infinity();
    double bestCorrectionSquared = std::numeric_limits<float>::infinity();
    auto bestIdentity = std::tuple{std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::max(),
                                   std::numeric_limits<std::int32_t>::max()};
    std::optional<SurfaceEdgeMoveSnapResult> best{};
    const bool freeMove = !normalized(constraintAxis).has_value();
    for ( const SurfaceSnapEdge &target : stationaryEdges )
    {
        if ( !finiteVector(target.first) || !finiteVector(target.second) )
        {
            continue;
        }
        const TVector targetFirst = target.documentFirst.value_or(TVector{target.first.x(), target.first.y(), target.first.z()});
        const TVector targetSecond = target.documentSecond.value_or(TVector{target.second.x(), target.second.y(), target.second.z()});
        const TVector targetDifference = targetSecond - targetFirst;
        const std::optional<QVector3D> targetDirection = normalizedDocumentDirection(targetDifference);
        if ( !targetDirection )
        {
            continue;
        }

        QVector3D alignedTargetDirection = *targetDirection;
        float alignment = QVector3D::dotProduct(*movingDirection, alignedTargetDirection);
        if ( alignment < 0.0f )
        {
            alignedTargetDirection = -alignedTargetDirection;
            alignment = -alignment;
        }
        if ( alignment < safeParallelCosine )
        {
            continue;
        }

        const TVector displacement = (targetFirst - movingFirst) * 0.5 + (targetSecond - movingSecond) * 0.5;
        const TVector requested = displacement - rawTranslation;
        TVector correction = requested;
        if (const auto axis = normalized(constraintAxis))
        {
            TVector direction{axis->x(), axis->y(), axis->z()};
            direction = direction * (1.0 / std::hypot(direction.x, direction.y, direction.z));
            correction = direction * requested.Dot(direction);
        }
        const TVector residual = requested - correction;
        const double residualSquared = residual.Dot(residual);
        const double correctionSquared = correction.Dot(correction);
        if ( !std::isfinite(residualSquared) || !std::isfinite(correctionSquared) || residualSquared > captureSquared ||
             correctionSquared > captureSquared )
        {
            continue;
        }
        const auto identity = std::tuple{target.sector, std::min(target.firstVertex, target.secondVertex),
                                         std::max(target.firstVertex, target.secondVertex)};
        const bool better = freeMove ? correctionSquared < bestCorrectionSquared ||
                                           (correctionSquared == bestCorrectionSquared && identity < bestIdentity)
                                     : residualSquared < bestResidualSquared ||
                                           (residualSquared == bestResidualSquared &&
                                            (correctionSquared < bestCorrectionSquared ||
                                             (correctionSquared == bestCorrectionSquared && identity < bestIdentity)));
        if ( better )
        {
            SurfaceEdgeMoveSnapResult candidate{};
            candidate.documentTranslation = freeMove ? displacement : rawTranslation + correction;
            candidate.translation = QVector3D(static_cast<float>(candidate.documentTranslation.x),
                static_cast<float>(candidate.documentTranslation.y), static_cast<float>(candidate.documentTranslation.z));
            candidate.rotationPivot = rotationPivot;
            candidate.documentRotationPivot = movingFirst + (movingSecond - movingFirst) * 0.5;

            // Axis handles are positional constraints. Free movement may also
            // finish the small orientation correction implied by "same oriented".
            if ( freeMove )
            {
                const TVector rotationAxis = preciseDirectionCross(*movingDirection, alignedTargetDirection);
                const std::optional<QVector3D> unitAxis = normalizedDocumentDirection(rotationAxis);
                if ( unitAxis )
                {
                    candidate.rotationAxis = *unitAxis;
                    candidate.rotationAngleDegrees =
                        static_cast<float>(std::atan2(std::hypot(rotationAxis.x, rotationAxis.y, rotationAxis.z),
                            preciseDirectionDot(*movingDirection, alignedTargetDirection)) * 180.0 / std::numbers::pi);
                }
            }
            if (!finiteVector(candidate.translation) || !finiteVector(candidate.rotationPivot)
                || !finiteVector(candidate.rotationAxis) || !std::isfinite(candidate.rotationAngleDegrees))
            {
                continue;
            }
            bestResidualSquared = residualSquared;
            bestCorrectionSquared = correctionSquared;
            bestIdentity = identity;
            best = candidate;
        }
    }
    return best;
}

std::optional<SurfaceMoveOrientationSnapResult> snapSurfaceMoveOrientation(const QVector3D &movingNormal,
                                                                           const QVector3D &stationaryNormal,
                                                                           const float maximumCorrectionDegrees)
{
    const std::optional<QVector3D> moving = normalized(movingNormal);
    const std::optional<QVector3D> stationary = normalized(stationaryNormal);
    if ( !moving || !stationary || !std::isfinite(maximumCorrectionDegrees) || maximumCorrectionDegrees < 0.0f )
    {
        return std::nullopt;
    }

    const float sameFacingAlignment = QVector3D::dotProduct(*moving, *stationary);
    const float opposedAlignment = -sameFacingAlignment;
    // Surface translation has already acquired the target spatially. Finish
    // whichever parallel orientation is closest to the authored moving normal.
    // Exact quarter-turn ties retain the conventional opposed/back-to-back
    // choice as a deterministic fallback.
    const QVector3D target = opposedAlignment >= sameFacingAlignment ? -*stationary : *stationary;
    const TVector cross = preciseDirectionCross(*moving, target);
    const float angleDegrees = static_cast<float>(std::atan2(std::hypot(cross.x, cross.y, cross.z),
        preciseDirectionDot(*moving, target)) * 180.0 / std::numbers::pi);
    if ( !std::isfinite(angleDegrees) || angleDegrees > maximumCorrectionDegrees || angleDegrees == 0.0f )
    {
        return std::nullopt;
    }

    const std::optional<QVector3D> axis = normalizedDocumentDirection(cross);
    if ( !axis )
    {
        return std::nullopt;
    }
    return SurfaceMoveOrientationSnapResult{*axis, angleDegrees};
}

} // namespace sed::render
