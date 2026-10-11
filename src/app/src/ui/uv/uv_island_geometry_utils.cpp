#include "ui/uv/uv_island_geometry_utils.hpp"

#include "sed/math_utils.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cmath>
#include <limits>
#include <numbers>

namespace sed::ui::uv
{

QRectF uvPointBounds(const std::vector<QPointF>& points)
{
    if (points.empty())
    {
        return {};
    }

    qreal minX = points.front().x();
    qreal minY = points.front().y();
    qreal maxX = minX;
    qreal maxY = minY;
    for (const QPointF& p : points)
    {
        minX = std::min(minX, p.x());
        minY = std::min(minY, p.y());
        maxX = std::max(maxX, p.x());
        maxY = std::max(maxY, p.y());
    }
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
}

qreal uvSnapCoordinateToGrid(const qreal value, const qreal gridStep) noexcept
{
    if (!std::isfinite(value) || !std::isfinite(gridStep) || gridStep <= 0.0)
    {
        return value;
    }

    const qreal cell = value / gridStep;
    if (!std::isfinite(cell))
    {
        return value;
    }

    const qreal snapped = std::round(cell) * gridStep;
    return std::isfinite(snapped) ? snapped : value;
}

QPointF uvSnapPointToGrid(const QPointF& point, const qreal gridStep) noexcept
{
    return QPointF(
        uvSnapCoordinateToGrid(point.x(), gridStep),
        uvSnapCoordinateToGrid(point.y(), gridStep));
}

QPointF uvSnapPointToVisibleGrid(
    const QPointF& point,
    const qreal mainGridStep,
    const bool subGridEnabled) noexcept
{
    const QPointF main = uvSnapPointToGrid(point, mainGridStep);
    if (!subGridEnabled || !std::isfinite(mainGridStep) || mainGridStep <= 0.0)
    {
        return main;
    }

    const QPointF sub = uvSnapPointToGrid(point, mainGridStep / 4.0);
    const qreal mainDistance = std::hypot(main.x() - point.x(), main.y() - point.y());
    const qreal subDistance = std::hypot(sub.x() - point.x(), sub.y() - point.y());
    // Preserve a main-grid point on an exact tie. This keeps the primary
    // lattice authoritative while still exposing all visible subdivisions.
    return subDistance < mainDistance ? sub : main;
}

qreal uvSnapCoordinateToVisibleGrid(
    const qreal value,
    const qreal mainGridStep,
    const bool subGridEnabled) noexcept
{
    const qreal main = uvSnapCoordinateToGrid(value, mainGridStep);
    if (!subGridEnabled || !std::isfinite(mainGridStep) || mainGridStep <= 0.0)
    {
        return main;
    }

    const qreal sub = uvSnapCoordinateToGrid(value, mainGridStep / 4.0);
    return std::abs(sub - value) < std::abs(main - value) ? sub : main;
}

std::optional<QPointF> uvNearestVisibleGridSnapDelta(
    const std::vector<QPointF>& referencePoints,
    const qreal mainGridStep,
    const bool subGridEnabled) noexcept
{
    if (referencePoints.empty()
        || !std::isfinite(mainGridStep)
        || mainGridStep <= 0.0)
    {
        return std::nullopt;
    }

    std::optional<QPointF> best{};
    qreal bestDistance = std::numeric_limits<qreal>::infinity();
    for (const QPointF& point : referencePoints)
    {
        if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
        {
            continue;
        }
        const QPointF delta = uvSnapPointToVisibleGrid(
            point, mainGridStep, subGridEnabled) - point;
        const qreal distance = std::hypot(delta.x(), delta.y());
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = delta;
        }
    }
    return best;
}

UvSnapGeometry uvCollectSnapGeometry(
    const std::vector<UvIslandState>& islands,
    const std::vector<std::vector<bool>>& movingVertexMasks,
    const std::vector<std::vector<bool>>& selectedEdgeMasks,
    const bool collectEdges)
{
    UvSnapGeometry result{};
    for (std::size_t islandIndex = 0; islandIndex < islands.size(); ++islandIndex)
    {
        const UvIslandState& island = islands[islandIndex];
        const std::vector<bool>* movingMask = islandIndex < movingVertexMasks.size()
            ? &movingVertexMasks[islandIndex]
            : nullptr;
        const std::vector<bool>* edgeMask = islandIndex < selectedEdgeMasks.size()
            ? &selectedEdgeMasks[islandIndex]
            : nullptr;

        const bool islandMoves = movingMask
            && std::any_of(movingMask->begin(), movingMask->end(),
                [](const bool selected) { return selected; });
        const bool completeIslandMoves = islandMoves
            && movingMask->size() >= island.uv.size()
            && std::all_of(
                movingMask->begin(),
                movingMask->begin() + static_cast<std::ptrdiff_t>(island.uv.size()),
                [](const bool selected) { return selected; });

        for (std::size_t vertexIndex = 0; vertexIndex < island.uv.size(); ++vertexIndex)
        {
            const bool moving = movingMask
                && vertexIndex < movingMask->size()
                && (*movingMask)[vertexIndex];
            if (moving)
            {
                result.movingPoints.push_back(island.uv[vertexIndex]);
            }
            else if (!islandMoves)
            {
                result.targetPoints.push_back(island.uv[vertexIndex]);
            }
        }
        if (!island.uv.empty())
        {
            if (completeIslandMoves)
            {
                result.movingSurfacePoints.push_back(islandCentroid(island));
            }
            else if (!islandMoves)
            {
                result.targetSurfacePoints.push_back(islandCentroid(island));
            }
        }

        if (!collectEdges || island.uv.size() < 2u)
        {
            continue;
        }
        for (std::size_t edgeIndex = 0; edgeIndex < island.uv.size(); ++edgeIndex)
        {
            const std::size_t next = (edgeIndex + 1u) % island.uv.size();
            const UvSnapEdge edge{ island.uv[edgeIndex], island.uv[next] };
            const bool selected = edgeMask
                && edgeIndex < edgeMask->size()
                && (*edgeMask)[edgeIndex];
            if (selected)
            {
                result.movingEdges.push_back(edge);
                continue;
            }

            if (!islandMoves)
            {
                result.targetEdges.push_back(edge);
            }
        }
    }
    return result;
}

std::optional<QPointF> uvNearestPointSnapDelta(
    const std::vector<QPointF>& movingPoints,
    const std::vector<QPointF>& targetPoints,
    const qreal maxDistance) noexcept
{
    if (!std::isfinite(maxDistance) || maxDistance < 0.0)
    {
        return std::nullopt;
    }

    std::optional<QPointF> best{};
    qreal bestDistance = std::numeric_limits<qreal>::infinity();
    for (const QPointF& moving : movingPoints)
    {
        if (!std::isfinite(moving.x()) || !std::isfinite(moving.y()))
        {
            continue;
        }
        for (const QPointF& target : targetPoints)
        {
            if (!std::isfinite(target.x()) || !std::isfinite(target.y()))
            {
                continue;
            }
            const QPointF delta = target - moving;
            const qreal distance = std::hypot(delta.x(), delta.y());
            if (distance <= maxDistance && distance < bestDistance)
            {
                bestDistance = distance;
                best = delta;
            }
        }
    }
    return best;
}

std::optional<QPointF> uvNearestEdgeSnapDelta(
    const std::vector<UvSnapEdge>& movingEdges,
    const std::vector<UvSnapEdge>& targetEdges,
    const qreal maxDistance,
    const qreal maxEndpointResidual) noexcept
{
    if (!std::isfinite(maxDistance) || maxDistance < 0.0
        || !std::isfinite(maxEndpointResidual) || maxEndpointResidual < 0.0)
    {
        return std::nullopt;
    }

    std::optional<QPointF> best{};
    qreal bestScore = std::numeric_limits<qreal>::infinity();
    for (const UvSnapEdge& moving : movingEdges)
    {
        const QPointF movingVector = moving.end - moving.start;
        const qreal movingLength = std::hypot(movingVector.x(), movingVector.y());
        if (!std::isfinite(movingLength) || movingLength <= 1.0e-12)
        {
            continue;
        }
        for (const UvSnapEdge& target : targetEdges)
        {
            const QPointF targetVector = target.end - target.start;
            const qreal targetLength = std::hypot(targetVector.x(), targetVector.y());
            if (!std::isfinite(targetLength) || targetLength <= 1.0e-12)
            {
                continue;
            }

            // Edge snapping is an overlay operation, not merely endpoint
            // proximity. Reject different orientations explicitly so very
            // short perpendicular edges cannot pass a pixel-derived endpoint
            // residual tolerance. Absolute cosine keeps reversed winding
            // eligible while limiting acquisition to five degrees.
            constexpr qreal kMinimumParallelCosine = 0.9961946980917455;
            const qreal orientationCosine = std::abs(
                QPointF::dotProduct(movingVector, targetVector)
                / (movingLength * targetLength));
            if (!std::isfinite(orientationCosine)
                || orientationCosine < kMinimumParallelCosine)
            {
                continue;
            }

            const auto considerOrder = [&](const QPointF& targetStart,
                                            const QPointF& targetEnd)
            {
                const QPointF startDelta = targetStart - moving.start;
                const QPointF endDelta = targetEnd - moving.end;
                const QPointF delta = (startDelta + endDelta) * 0.5;
                const qreal translation = std::hypot(delta.x(), delta.y());
                const qreal startResidual = std::hypot(
                    (moving.start.x() + delta.x()) - targetStart.x(),
                    (moving.start.y() + delta.y()) - targetStart.y());
                const qreal endResidual = std::hypot(
                    (moving.end.x() + delta.x()) - targetEnd.x(),
                    (moving.end.y() + delta.y()) - targetEnd.y());
                const qreal residual = std::max(startResidual, endResidual);
                if (translation > maxDistance || residual > maxEndpointResidual)
                {
                    return;
                }

                // Translation dominates acquisition. Residual breaks ties in
                // favor of the edge pair which truly overlays most closely.
                const qreal score = translation + residual * 2.0;
                if (score < bestScore)
                {
                    bestScore = score;
                    best = delta;
                }
            };

            considerOrder(target.start, target.end);
            considerOrder(target.end, target.start);
        }
    }
    return best;
}

QRectF islandBounds(const UvIslandState& island)
{
    return uvPointBounds(island.uv);
}

qreal polygonSignedArea(const std::vector<QPointF>& points)
{
    if (points.size() < 3)
    {
        return 0.0;
    }

    qreal twiceArea = 0.0;
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const QPointF& a = points[i];
        const QPointF& b = points[(i + 1u) % points.size()];
        twiceArea += (a.x() * b.y()) - (b.x() * a.y());
    }
    return twiceArea * 0.5;
}

std::optional<bool> uvViewMirrorXForWinding(
    const qreal storedArea,
    const qreal projectedArea)
{
    constexpr qreal kStableArea = 1.0e-9;
    if (!std::isfinite(storedArea)
        || !std::isfinite(projectedArea)
        || std::abs(storedArea) <= kStableArea
        || std::abs(projectedArea) <= kStableArea)
    {
        return std::nullopt;
    }
    return std::signbit(storedArea) != std::signbit(projectedArea);
}

QPointF uvViewScreenFromUv(
    const QPointF& uv,
    const QPointF& pan,
    const qreal zoom,
    const bool mirrorX,
    const qreal rotationRadians)
{
    const qreal safeZoom = std::max<qreal>(std::abs(zoom), 1.0e-9);
    const qreal xSign = mirrorX ? -1.0 : 1.0;
    const qreal cs = std::cos(rotationRadians);
    const qreal sn = std::sin(rotationRadians);
    const qreal orientedX = uv.x() * xSign;
    return QPointF(
        pan.x() + (orientedX * cs - uv.y() * sn) * safeZoom,
        pan.y() + (orientedX * sn + uv.y() * cs) * safeZoom);
}

QPointF uvViewUvFromScreen(
    const QPointF& screen,
    const QPointF& pan,
    const qreal zoom,
    const bool mirrorX,
    const qreal rotationRadians)
{
    const qreal safeZoom = std::max<qreal>(std::abs(zoom), 1.0e-9);
    const qreal xSign = mirrorX ? -1.0 : 1.0;
    const qreal cs = std::cos(rotationRadians);
    const qreal sn = std::sin(rotationRadians);
    const qreal screenX = (screen.x() - pan.x()) / safeZoom;
    const qreal screenY = (screen.y() - pan.y()) / safeZoom;
    return QPointF(
        (screenX * cs + screenY * sn) * xSign,
        -screenX * sn + screenY * cs);
}

std::optional<UvViewOrientation> uvViewOrientationForActiveSurface(
    const std::vector<UvIslandState>& islands,
    const std::optional<SurfaceRef> activeSurface,
    const std::vector<QPointF>& projectedVertices)
{
    if (!activeSurface || projectedVertices.size() < 3u)
    {
        return std::nullopt;
    }

    const auto activeIt = std::find_if(
        islands.begin(),
        islands.end(),
        [&activeSurface](const UvIslandState& island)
        {
            return island.ref.sector == activeSurface->sector
                && island.ref.surface == activeSurface->surface;
        });
    if (activeIt == islands.end() || activeIt->uv.size() != projectedVertices.size())
    {
        return std::nullopt;
    }

    std::vector<QPointF> storedUv{};
    storedUv.reserve(activeIt->uv.size());
    for (const QPointF& uv : activeIt->uv)
    {
        storedUv.push_back(islandStoredFromDisplayUv(*activeIt, uv));
    }

    const std::optional<bool> mirror = uvViewMirrorXForWinding(
        polygonSignedArea(storedUv),
        polygonSignedArea(projectedVertices));
    if (!mirror)
    {
        return std::nullopt;
    }

    constexpr qreal kStableEdgeLengthSquared = 1.0e-12;
    std::optional<std::size_t> bestEdge{};
    qreal bestScore{};
    for (std::size_t i = 0; i < storedUv.size(); ++i)
    {
        const QPointF storedEdge = storedUv[(i + 1u) % storedUv.size()] - storedUv[i];
        const QPointF projectedEdge = projectedVertices[(i + 1u) % projectedVertices.size()] - projectedVertices[i];
        const qreal storedLengthSquared = QPointF::dotProduct(storedEdge, storedEdge);
        const qreal projectedLengthSquared = QPointF::dotProduct(projectedEdge, projectedEdge);
        if (!std::isfinite(storedLengthSquared) || !std::isfinite(projectedLengthSquared)
            || storedLengthSquared <= kStableEdgeLengthSquared
            || projectedLengthSquared <= kStableEdgeLengthSquared)
        {
            continue;
        }

        // Choose from stored UV geometry only so camera foreshortening cannot
        // switch the orientation basis while the user orbits the same face.
        // Equal-length ties deliberately retain the first polygon edge.
        const qreal score = storedLengthSquared;
        if (!bestEdge || score > bestScore)
        {
            bestEdge = i;
            bestScore = score;
        }
    }
    if (!bestEdge)
    {
        return std::nullopt;
    }

    QPointF storedEdge = storedUv[(*bestEdge + 1u) % storedUv.size()] - storedUv[*bestEdge];
    if (*mirror)
    {
        storedEdge.setX(-storedEdge.x());
    }
    const QPointF projectedEdge =
        projectedVertices[(*bestEdge + 1u) % projectedVertices.size()]
        - projectedVertices[*bestEdge];
    const qreal storedAngle = std::atan2(storedEdge.y(), storedEdge.x());
    const qreal projectedAngle = std::atan2(projectedEdge.y(), projectedEdge.x());
    const qreal rotation = std::remainder(
        projectedAngle - storedAngle,
        qreal{2.0} * std::numbers::pi_v<qreal>);
    if (!std::isfinite(rotation))
    {
        return std::nullopt;
    }
    return UvViewOrientation{ *mirror, rotation };
}

bool uvViewMirrorXForActiveSurface(
    const std::vector<UvIslandState>& islands,
    const std::optional<SurfaceRef> activeSurface,
    const std::optional<qreal> projectedArea,
    const bool previousMirrorX)
{
    if (!activeSurface || !projectedArea)
    {
        return previousMirrorX;
    }

    const auto activeIt = std::find_if(
        islands.begin(),
        islands.end(),
        [&activeSurface](const UvIslandState& island)
        {
            return island.ref.sector == activeSurface->sector
                && island.ref.surface == activeSurface->surface;
        });
    if (activeIt == islands.end())
    {
        return previousMirrorX;
    }

    std::vector<QPointF> storedUv{};
    storedUv.reserve(activeIt->uv.size());
    for (const QPointF& uv : activeIt->uv)
    {
        storedUv.push_back(islandStoredFromDisplayUv(*activeIt, uv));
    }

    const std::optional<bool> decision = uvViewMirrorXForWinding(
        polygonSignedArea(storedUv),
        *projectedArea);
    return decision.value_or(previousMirrorX);
}

QPointF uvViewPanForAnchor(
    const QPointF& uv,
    const QPointF& screen,
    const qreal zoom,
    const bool mirrorX,
    const qreal rotationRadians)
{
    return screen - uvViewScreenFromUv(
        uv,
        QPointF(),
        zoom,
        mirrorX,
        rotationRadians);
}

QPointF polygonCentroid(const std::vector<QPointF>& points)
{
    if (points.empty())
    {
        return {};
    }

    // Step 1: degenerate polygons have no stable signed-area centroid, so use point-cloud center.
    const qreal area = polygonSignedArea(points);
    if (std::abs(area) <= 1.0e-9)
    {
        return uvPointCloudCenter(points);
    }

    // Step 2: accumulate the standard area-weighted polygon centroid.
    qreal cx = 0.0;
    qreal cy = 0.0;
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const QPointF& a = points[i];
        const QPointF& b = points[(i + 1u) % points.size()];
        const qreal cross = (a.x() * b.y()) - (b.x() * a.y());
        cx += (a.x() + b.x()) * cross;
        cy += (a.y() + b.y()) * cross;
    }

    const qreal factor = 1.0 / (6.0 * area);
    return QPointF(cx * factor, cy * factor);
}

QPointF islandCentroid(const UvIslandState& island)
{
    return polygonCentroid(island.uv);
}

QPointF islandFrameCenter(const UvIslandState& island)
{
    const QRectF bounds = islandBounds(island);
    return bounds.isValid() ? bounds.center() : islandCentroid(island);
}

QPointF islandDisplayFromStoredUv(const UvIslandState& island, QPointF uv)
{
    if (island.displayMirrorX)
    {
        uv.setX((2.0 * island.displayMirrorAxisX) - uv.x());
    }
    return uv + island.displayOffset;
}

QPointF islandStoredFromDisplayUv(const UvIslandState& island, QPointF uv)
{
    uv -= island.displayOffset;
    if (island.displayMirrorX)
    {
        uv.setX((2.0 * island.displayMirrorAxisX) - uv.x());
    }
    return uv;
}

bool uvDisplayPointsStoreSameCoordinates(
    const UvIslandState& lhs, const std::vector<QPointF>& lhsPoints,
    const UvIslandState& rhs, const std::vector<QPointF>& rhsPoints)
{
    if (lhsPoints.size() != rhsPoints.size() || lhs.storedCoordinateSpace != rhs.storedCoordinateSpace
        || (lhs.storedCoordinateSpace != UvCoordinateSpace::Normalized
            && lhs.storedCoordinateSpace != UvCoordinateSpace::TexturePixels)
        || (lhs.storedCoordinateSpace == UvCoordinateSpace::TexturePixels
            && (lhs.textureSize != rhs.textureSize || !lhs.textureSize.isValid())))
    {
        return false;
    }
    const auto documentPoint = [](const UvIslandState& island, const QPointF& point)
    {
        const QPointF stored = islandStoredFromDisplayUv(island, point);
        return island.storedCoordinateSpace == UvCoordinateSpace::TexturePixels
            ? denormalizeStoredUv(stored, island.textureSize) : stored;
    };
    const auto sameFloat = [](const double a, const double b)
    {
        if (!std::isfinite(a) || !std::isfinite(b)
            || std::abs(a) > std::numeric_limits<float>::max()
            || std::abs(b) > std::numeric_limits<float>::max())
        {
            return false;
        }
        return std::bit_cast<std::uint32_t>(static_cast<float>(a))
            == std::bit_cast<std::uint32_t>(static_cast<float>(b));
    };
    for (std::size_t i = 0; i < lhsPoints.size(); ++i)
    {
        const QPointF a = documentPoint(lhs, lhsPoints[i]);
        const QPointF b = documentPoint(rhs, rhsPoints[i]);
        if (!sameFloat(a.x(), b.x()) || !sameFloat(a.y(), b.y()))
        {
            return false;
        }
    }
    return true;
}

bool uvIslandStatesStoreSameUv(const std::vector<UvIslandState>& lhs, const std::vector<UvIslandState>& rhs)
{
    if ( lhs.size() != rhs.size() )
    {
        return false;
    }

    // Do not discard small authored edits using a display tolerance. The final
    // document writer rounds to stored floats and decides whether an edit remains.
    for ( std::size_t islandIndex = 0; islandIndex < lhs.size(); ++islandIndex )
    {
        const UvIslandState& a = lhs[islandIndex];
        const UvIslandState& b = rhs[islandIndex];
        if ( a.ref.sector != b.ref.sector
            || a.ref.surface != b.ref.surface
            || a.uv.size() != b.uv.size() )
        {
            return false;
        }

        for ( std::size_t vertexIndex = 0; vertexIndex < a.uv.size(); ++vertexIndex )
        {
            const QPointF auv = islandStoredFromDisplayUv(a, a.uv[vertexIndex]);
            const QPointF buv = islandStoredFromDisplayUv(b, b.uv[vertexIndex]);
            if ( !std::isfinite(auv.x())
                || !std::isfinite(auv.y())
                || !std::isfinite(buv.x())
                || !std::isfinite(buv.y())
                || auv.x() != buv.x()
                || auv.y() != buv.y() )
            {
                return false;
            }
        }
    }

    return true;
}

QRectF islandsBounds(const std::vector<UvIslandState>& islands, const bool selectedOnly)
{
    QRectF bounds{};
    bool have = false;
    for (const UvIslandState& island : islands)
    {
        if (selectedOnly && !island.selected)
        {
            continue;
        }

        const QRectF b = islandBounds(island);
        if (!b.isValid() || b.isEmpty())
        {
            continue;
        }

        bounds = have ? bounds.united(b) : b;
        have = true;
    }

    return have ? bounds : QRectF();
}

QPointF uvPointCloudCenter(const std::vector<QPointF>& points)
{
    if (points.empty())
    {
        return {};
    }

    qreal sumX = 0.0;
    qreal sumY = 0.0;
    for (const QPointF& point : points)
    {
        sumX += point.x();
        sumY += point.y();
    }

    const qreal invCount = 1.0 / static_cast<qreal>(points.size());
    return QPointF(sumX * invCount, sumY * invCount);
}

void translateIsland(UvIslandState& island, const QPointF& delta)
{
    for (QPointF& p : island.uv)
    {
        p += delta;
    }
}

void translateIslandDisplay(UvIslandState& island, const QPointF& delta)
{
    if (std::abs(delta.x()) <= sed::math::kTinyEpsilon && std::abs(delta.y()) <= sed::math::kTinyEpsilon)
    {
        return;
    }

    translateIsland(island, delta);
    for (QPointF& p : island.unwrapUv)
    {
        p += delta;
    }
    island.displayOffset += delta;
}

void rotateIsland(UvIslandState& island, const qreal degrees)
{
    if (island.uv.empty())
    {
        return;
    }

    const QPointF c = islandCentroid(island);
    const qreal radians = degrees * (std::numbers::pi_v<qreal> / 180.0);
    const qreal cs = std::cos(radians);
    const qreal sn = std::sin(radians);
    for (QPointF& p : island.uv)
    {
        const QPointF d = p - c;
        p = QPointF(c.x() + d.x() * cs - d.y() * sn, c.y() + d.x() * sn + d.y() * cs);
    }
}

void scaleIsland(UvIslandState& island, const qreal sx, const qreal sy)
{
    if (island.uv.empty())
    {
        return;
    }

    const QPointF c = islandCentroid(island);
    for (QPointF& p : island.uv)
    {
        p = QPointF(c.x() + (p.x() - c.x()) * sx, c.y() + (p.y() - c.y()) * sy);
    }
}

namespace
{

std::optional<float> checkedTexCoordinate(const double value) noexcept
{
    constexpr double maximum = static_cast<double>(
        std::numeric_limits<float>::max());
    if (!std::isfinite(value) || value < -maximum || value > maximum)
    {
        return std::nullopt;
    }
    return static_cast<float>(value);
}

bool texCoordinateChanged(const float before, const float after) noexcept
{
    // These are the exact values that will be stored in the document. Do not
    // use a display-space tolerance here: small but representable UV edits
    // must still publish and enter Undo history.
    return before != after;
}

} // namespace

std::optional<bool> uvRotateTexVerticesAroundCenter(
    std::vector<TexVertex>& texVertices,
    const UvCoordinateSpace coordinateSpace,
    const QSize& textureSize,
    const double degrees) noexcept
{
    if (texVertices.empty() || !std::isfinite(degrees))
    {
        return std::nullopt;
    }

    const bool normalized = coordinateSpace == UvCoordinateSpace::Normalized;
    const bool pixels = coordinateSpace == UvCoordinateSpace::TexturePixels;
    if (!normalized && !pixels)
    {
        return std::nullopt;
    }
    if (pixels
        && (!textureSize.isValid()
            || textureSize.width() <= 0
            || textureSize.height() <= 0))
    {
        return std::nullopt;
    }

    const double width = pixels
        ? static_cast<double>(textureSize.width())
        : 1.0;
    const double height = pixels
        ? static_cast<double>(textureSize.height())
        : 1.0;
    double centerU = 0.0;
    double centerV = 0.0;
    for (const TexVertex& texVertex : texVertices)
    {
        if (!std::isfinite(texVertex.u) || !std::isfinite(texVertex.v))
        {
            return std::nullopt;
        }
        centerU += static_cast<double>(texVertex.u) / width;
        centerV += static_cast<double>(texVertex.v) / height;
    }
    centerU /= static_cast<double>(texVertices.size());
    centerV /= static_cast<double>(texVertices.size());
    if (!std::isfinite(centerU) || !std::isfinite(centerV))
    {
        return std::nullopt;
    }

    const double radians = degrees
        * (std::numbers::pi_v<double> / 180.0);
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    if (!std::isfinite(cosine) || !std::isfinite(sine))
    {
        return std::nullopt;
    }

    std::vector<TexVertex> next = texVertices;
    bool changed = false;
    for (TexVertex& texVertex : next)
    {
        const double u = static_cast<double>(texVertex.u) / width - centerU;
        const double v = static_cast<double>(texVertex.v) / height - centerV;
        const std::optional<float> nextU = checkedTexCoordinate(
            (centerU + u * cosine - v * sine) * width);
        const std::optional<float> nextV = checkedTexCoordinate(
            (centerV + u * sine + v * cosine) * height);
        if (!nextU || !nextV)
        {
            return std::nullopt;
        }
        changed = changed
            || texCoordinateChanged(texVertex.u, *nextU)
            || texCoordinateChanged(texVertex.v, *nextV);
        texVertex.u = *nextU;
        texVertex.v = *nextV;
    }

    if (changed)
    {
        texVertices = std::move(next);
    }
    return changed;
}

std::optional<bool> uvScaleTexVerticesAroundCenter(
    std::vector<TexVertex>& texVertices,
    const double scaleU,
    const double scaleV) noexcept
{
    if (texVertices.empty()
        || !std::isfinite(scaleU)
        || !std::isfinite(scaleV))
    {
        return std::nullopt;
    }

    double centerU = 0.0;
    double centerV = 0.0;
    for (const TexVertex& texVertex : texVertices)
    {
        if (!std::isfinite(texVertex.u) || !std::isfinite(texVertex.v))
        {
            return std::nullopt;
        }
        centerU += static_cast<double>(texVertex.u);
        centerV += static_cast<double>(texVertex.v);
    }
    centerU /= static_cast<double>(texVertices.size());
    centerV /= static_cast<double>(texVertices.size());
    if (!std::isfinite(centerU) || !std::isfinite(centerV))
    {
        return std::nullopt;
    }

    std::vector<TexVertex> next = texVertices;
    bool changed = false;
    for (TexVertex& texVertex : next)
    {
        const std::optional<float> nextU = checkedTexCoordinate(
            centerU
            + (static_cast<double>(texVertex.u) - centerU) * scaleU);
        const std::optional<float> nextV = checkedTexCoordinate(
            centerV
            + (static_cast<double>(texVertex.v) - centerV) * scaleV);
        if (!nextU || !nextV)
        {
            return std::nullopt;
        }
        changed = changed
            || texCoordinateChanged(texVertex.u, *nextU)
            || texCoordinateChanged(texVertex.v, *nextV);
        texVertex.u = *nextU;
        texVertex.v = *nextV;
    }

    if (changed)
    {
        texVertices = std::move(next);
    }
    return changed;
}

void packIslands(std::vector<UvIslandState>& islands, const bool selectedOnly)
{
    qreal cursorX = 0.0;
    qreal cursorY = 0.0;
    qreal rowHeight = 0.0;
    constexpr qreal kPadding = 0.5;
    constexpr qreal kMaxRowWidth = 12.0;

    for (UvIslandState& island : islands)
    {
        if (selectedOnly && !island.selected)
        {
            continue;
        }

        const QRectF b = islandBounds(island);
        if (!b.isValid())
        {
            continue;
        }

        if (cursorX > 0.0 && (cursorX + b.width()) > kMaxRowWidth)
        {
            cursorX = 0.0;
            cursorY += rowHeight + kPadding;
            rowHeight = 0.0;
        }

        const QPointF delta(cursorX - b.left(), -cursorY - b.top());
        translateIsland(island, delta);

        cursorX += b.width() + kPadding;
        rowHeight = std::max(rowHeight, b.height());
    }
}

QSize sanitizeUvTextureSize(const QSize& size)
{
    return QSize(std::max(1, size.width()), std::max(1, size.height()));
}

QPointF normalizeStoredUv(const QPointF& uv, const QSize& textureSize)
{
    const QSize safeSize = sanitizeUvTextureSize(textureSize);
    return QPointF(
        uv.x() / static_cast<qreal>(safeSize.width()),
        uv.y() / static_cast<qreal>(safeSize.height()));
}

QPointF denormalizeStoredUv(const QPointF& uv, const QSize& textureSize)
{
    const QSize safeSize = sanitizeUvTextureSize(textureSize);
    return QPointF(
        uv.x() * static_cast<qreal>(safeSize.width()),
        uv.y() * static_cast<qreal>(safeSize.height()));
}

} // namespace sed::ui::uv
