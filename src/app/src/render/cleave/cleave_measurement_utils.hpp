#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <tuple>
#include <vector>

#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include "render/cleave/cleave_geometry_utils.hpp"
#include "ui/components/ui_decimal_format.hpp"

namespace sed::render
{

enum class CleaveSnapTargetKind : std::uint8_t
{
    Vertex,
    EdgeMidpoint,
    SurfaceCenter,
    StagedEndpoint,
};

struct CleaveSnapCandidate
{
    TVector position{};
    QPointF widgetPosition{};
    CleaveSnapTargetKind kind{CleaveSnapTargetKind::Vertex};
    // Populated for EdgeMidpoint so the viewport can identify the source edge
    // instead of showing an ambiguous point marker.
    TVector edgeFirst{};
    TVector edgeSecond{};
};

struct CleaveRigidEndpointSnap
{
    std::int32_t endpoint{};
    CleaveSnapCandidate target{};
};

enum class CleavePointDragConstraint : std::uint8_t
{
    Free,
    Angle,
    Geometry,
    Grid,
};

/**
 * Resolves the modifier policy for a newly authored point versus an existing
 * Multi-cut endpoint. New lines retain their configured default angle step;
 * edited endpoints move freely unless the user explicitly requests a snap.
 */
[[nodiscard]] constexpr CleavePointDragConstraint cleavePointDragConstraint(
    const bool editingEndpoint,
    const bool shiftHeld,
    const bool controlHeld,
    const bool gridSnapEnabled = false,
    const bool angleStepEnabled = false) noexcept
{
    if (editingEndpoint && shiftHeld && controlHeld)
    {
        return CleavePointDragConstraint::Angle;
    }
    if (shiftHeld)
    {
        return CleavePointDragConstraint::Geometry;
    }
    const bool gridSnap = gridSnapEnabled != controlHeld;
    if (editingEndpoint && angleStepEnabled && !controlHeld && !gridSnap)
    {
        return CleavePointDragConstraint::Angle;
    }
    if (gridSnap)
    {
        return CleavePointDragConstraint::Grid;
    }
    return editingEndpoint
        ? CleavePointDragConstraint::Free
        : CleavePointDragConstraint::Angle;
}

/**
 * Chooses the closest target to either endpoint of a rigidly translated line.
 * Endpoint zero and candidate order are stable tie-breakers. The caller owns
 * world projection and applies target.position - movedEndpoint as one rigid
 * translation, preserving the line's angle and length.
 */
inline std::optional<CleaveRigidEndpointSnap> selectNearestCleaveRigidEndpointSnap(
    const QPointF& first,
    const QPointF& second,
    const std::vector<CleaveSnapCandidate>& candidates,
    const double tolerancePx,
    const std::optional<std::int32_t> onlyEndpoint = std::nullopt) noexcept
{
    if (!std::isfinite(tolerancePx) || tolerancePx <= 0.0
        || (onlyEndpoint && (*onlyEndpoint < 0 || *onlyEndpoint > 1)))
    {
        return std::nullopt;
    }

    const QPointF endpoints[2]{first, second};
    double bestDistanceSquared = std::numeric_limits<double>::infinity();
    bool bestIsStagedEndpoint = false;
    std::optional<CleaveRigidEndpointSnap> best{};
    for (std::int32_t endpoint = 0; endpoint < 2; ++endpoint)
    {
        if (onlyEndpoint && endpoint != *onlyEndpoint)
        {
            continue;
        }
        if (!std::isfinite(endpoints[endpoint].x())
            || !std::isfinite(endpoints[endpoint].y()))
        {
            continue;
        }
        for (const CleaveSnapCandidate& candidate : candidates)
        {
            if (!std::isfinite(candidate.widgetPosition.x())
                || !std::isfinite(candidate.widgetPosition.y()))
            {
                continue;
            }
            const QPointF delta = candidate.widgetPosition - endpoints[endpoint];
            const double distanceSquared = delta.x() * delta.x() + delta.y() * delta.y();
            // Staged endpoints are small authored handles and represent an
            // explicit topology operation, so give them both a larger reach
            // and priority over broad geometry markers inside that reach.
            // Distance remains the tie-breaker within each target class.
            const bool isStagedEndpoint = candidate.kind
                == CleaveSnapTargetKind::StagedEndpoint;
            const double candidateTolerance = candidate.kind
                    == CleaveSnapTargetKind::StagedEndpoint
                ? tolerancePx + 12.0
                : tolerancePx;
            if (distanceSquared <= candidateTolerance * candidateTolerance
                && (!best
                    || (isStagedEndpoint && !bestIsStagedEndpoint)
                    || (isStagedEndpoint == bestIsStagedEndpoint
                        && distanceSquared < bestDistanceSquared)))
            {
                bestDistanceSquared = distanceSquared;
                bestIsStagedEndpoint = isStagedEndpoint;
                best = CleaveRigidEndpointSnap{endpoint, candidate};
            }
        }
    }
    return best;
}

struct CleaveScreenMarkerSegment
{
    QPointF first{};
    QPointF second{};
};

/**
 * Builds the compact screen-space tick used to identify an edge-midpoint
 * snap. The tick is centered on the source edge and perpendicular to it, so
 * it identifies the midpoint without painting the complete edge as though it
 * were selected geometry.
 */
inline std::optional<CleaveScreenMarkerSegment> cleaveEdgeMidpointMarkerSegment(
    const QPointF& edgeFirst,
    const QPointF& edgeSecond,
    const double halfLengthPx = 7.0) noexcept
{
    if (!std::isfinite(edgeFirst.x()) || !std::isfinite(edgeFirst.y())
        || !std::isfinite(edgeSecond.x()) || !std::isfinite(edgeSecond.y())
        || !std::isfinite(halfLengthPx) || halfLengthPx <= 0.0)
    {
        return std::nullopt;
    }

    const QPointF edge = edgeSecond - edgeFirst;
    const double edgeLength = std::hypot(edge.x(), edge.y());
    if (edgeLength <= 1.0e-6)
    {
        return std::nullopt;
    }

    const QPointF midpoint = (edgeFirst + edgeSecond) * 0.5;
    const QPointF normal(-edge.y() / edgeLength, edge.x() / edgeLength);
    return CleaveScreenMarkerSegment{
        midpoint - normal * halfLengthPx,
        midpoint + normal * halfLengthPx,
    };
}

/**
 * Selects the nearest projected cleave construction point inside a pixel
 * acquisition radius. Candidate order is a stable tie-breaker, which lets the
 * caller retain vertex/endpoint priority when points overlap exactly.
 */
inline std::optional<CleaveSnapCandidate> selectNearestCleaveSnapCandidate(
    const QPointF& pointer,
    const std::vector<CleaveSnapCandidate>& candidates,
    const double tolerancePx) noexcept
{
    if (!std::isfinite(tolerancePx) || tolerancePx <= 0.0)
    {
        return std::nullopt;
    }

    double bestDistanceSquared = std::numeric_limits<double>::infinity();
    bool bestIsStagedEndpoint = false;
    std::optional<CleaveSnapCandidate> best{};
    for (const CleaveSnapCandidate& candidate : candidates)
    {
        if (!std::isfinite(candidate.widgetPosition.x())
            || !std::isfinite(candidate.widgetPosition.y()))
        {
            continue;
        }
        const QPointF delta = candidate.widgetPosition - pointer;
        const double distanceSquared = delta.x() * delta.x() + delta.y() * delta.y();
        // Prefer staged endpoints once they enter their acquisition radius.
        // They are visually smaller than geometry guides and snapping to one
        // also expresses the user's intent to reconnect cleave topology.
        const bool isStagedEndpoint = candidate.kind
            == CleaveSnapTargetKind::StagedEndpoint;
        const double candidateTolerance = candidate.kind
                == CleaveSnapTargetKind::StagedEndpoint
            ? tolerancePx + 12.0
            : tolerancePx;
        if (distanceSquared <= candidateTolerance * candidateTolerance
            && (!best
                || (isStagedEndpoint && !bestIsStagedEndpoint)
                || (isStagedEndpoint == bestIsStagedEndpoint
                    && distanceSquared < bestDistanceSquared)))
        {
            bestDistanceSquared = distanceSquared;
            bestIsStagedEndpoint = isStagedEndpoint;
            best = candidate;
        }
    }
    return best;
}

struct CleavePlanePolygonEdge
{
    TVector first{};
    TVector second{};
};

/**
 * Finds the non-degenerate plane-polygon edge with the smallest true 3D
 * point-to-segment distance to the authored camera. This is deliberately a
 * world-space choice: orbiting after authoring cannot make the cleave handle
 * jump to another side of its plane.
 */
inline std::optional<CleavePlanePolygonEdge> nearestCleavePlanePolygonEdge(
    const std::vector<std::vector<TVector>>& polygons,
    const TVector& cameraPosition) noexcept
{
    double bestDistanceSquared = std::numeric_limits<double>::infinity();
    std::optional<CleavePlanePolygonEdge> best{};
    for (const std::vector<TVector>& polygon : polygons)
    {
        if (polygon.size() < 2)
        {
            continue;
        }
        for (std::size_t edge = 0; edge < polygon.size(); ++edge)
        {
            const TVector& first = polygon[edge];
            const TVector& second = polygon[(edge + 1) % polygon.size()];
            const TVector segment = second - first;
            const double lengthSquared = segment.lengthSqr();
            if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0)
            {
                continue;
            }
            const double t = std::clamp(
                cleaveDot(cameraPosition - first, segment) / lengthSquared,
                0.0,
                1.0);
            const double distanceSquared = (cameraPosition - (first + segment * t)).lengthSqr();
            if (std::isfinite(distanceSquared) && distanceSquared < bestDistanceSquared)
            {
                bestDistanceSquared = distanceSquared;
                best = CleavePlanePolygonEdge{first, second};
            }
        }
    }
    return best;
}

inline bool cleavePointerMotionIsDrag(
    const QPoint& delta,
    const int clickTolerancePx = 2) noexcept
{
    return delta.manhattanLength() > std::max(0, clickTolerancePx);
}

enum class CleaveMeasureUnit : std::uint8_t
{
    Bare,
    Meters,
    JKU,
    IJU,
    // Kept last so the persisted numeric values of the original four units
    // remain stable across the compact Units-dropdown migration.
    None,
};

[[nodiscard]] inline QString cleaveLineNumberText(const std::size_t zeroBasedIndex)
{
    return QString::number(static_cast<qulonglong>(zeroBasedIndex) + 1ULL);
}

inline QString compactCleaveMeasurementNumber(const double value, const int decimals)
{
    const QLocale& locale = ui::uiNumericLocale();
    QString text = ui::formatUiNumber(value, 'f', decimals);
    bool parsed = false;
    const double roundedValue = ui::parseUiNumber(text, &parsed);
    if ( std::isfinite(value) && value != 0.0 && parsed && roundedValue == 0.0 )
    {
        return ui::formatUiNumber(value, 'g', 6);
    }
    const QString decimalPoint = locale.decimalPoint();
    const QString zero = locale.zeroDigit();
    while ( text.contains(decimalPoint) && text.endsWith(zero) )
    {
        text.chop(zero.size());
    }
    if ( text.endsWith(decimalPoint) )
    {
        text.chop(decimalPoint.size());
    }
    return text == locale.negativeSign() + zero ? zero : text;
}

inline QString cleaveTransformHudDisplayText(const QString& canonicalText)
{
    if ( ui::uiNumericLocale().decimalPoint() == QStringLiteral(".") )
    {
        return canonicalText;
    }
    const bool rotate = canonicalText.startsWith(QStringLiteral("Rotate "))
        && canonicalText.endsWith(QChar(0x00b0));
    const bool scale = canonicalText.startsWith(QStringLiteral("Scale "))
        && canonicalText.endsWith(QChar(0x00d7));
    if ( !rotate && !scale )
    {
        return canonicalText;
    }
    const qsizetype prefixLength = rotate ? 7 : 6;
    const QString number = canonicalText.sliced(prefixLength,
        canonicalText.size() - prefixLength - 1);
    return canonicalText.first(prefixLength)
        + ui::localizeUiNumericText(number) + canonicalText.last(1);
}

inline QString cleaveLengthText(const double bareLength, const CleaveMeasureUnit unit)
{
    double value = std::isfinite(bareLength) ? std::max(0.0, bareLength) : 0.0;
    QString suffix{};
    switch (unit)
    {
        case CleaveMeasureUnit::None:
            return {};
        case CleaveMeasureUnit::Bare:
            break;
        case CleaveMeasureUnit::Meters:
            value *= 10.0;
            suffix = QStringLiteral(" m");
            break;
        case CleaveMeasureUnit::JKU:
            value *= 10.0;
            suffix = QStringLiteral(" JKU");
            break;
        case CleaveMeasureUnit::IJU:
            value *= 10.0;
            suffix = QStringLiteral(" IJU");
            break;
    }
    return compactCleaveMeasurementNumber(value, 3) + suffix;
}

inline double cleaveStableAngleDegrees(
    TVector direction,
    TVector reference,
    TVector planeNormal) noexcept
{
    if (!cleaveNormalize(direction) || !cleaveNormalize(reference)
        || !cleaveNormalize(planeNormal))
    {
        return 0.0;
    }
    const double x = std::clamp(
        static_cast<double>(cleaveDot(reference, direction)),
        -1.0,
        1.0);
    const double y = static_cast<double>(cleaveDot(
        planeNormal,
        cleaveCross(reference, direction)));
    constexpr double kRadiansToDegrees = 57.295779513082320876;
    double angle = std::atan2(y, x) * kRadiansToDegrees;
    if (angle < 0.0)
    {
        angle += 360.0;
    }
    return angle;
}

inline double cleaveAngleBetweenDegrees(
    TVector firstDirection,
    TVector secondDirection) noexcept
{
    if (!cleaveNormalize(firstDirection) || !cleaveNormalize(secondDirection))
    {
        return 0.0;
    }
    constexpr double kRadiansToDegrees = 57.295779513082320876;
    return std::acos(std::clamp(
        static_cast<double>(cleaveDot(firstDirection, secondDirection)),
        -1.0,
        1.0)) * kRadiansToDegrees;
}

struct CleaveJointAngle
{
    TVector joint{};
    TVector firstOther{};
    TVector secondOther{};
    std::size_t firstLine{};
    std::size_t secondLine{};
    double degrees{};
};

struct CleaveJointGuideCache
{
    std::vector<CleaveJointAngle> joints{};
    std::vector<std::uint8_t> linesWithJoint{};
    std::uint64_t authoredRevision{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t previewRevision{std::numeric_limits<std::uint64_t>::max()};
};

[[nodiscard]] inline bool cleaveJointGuideRebuildFitsBudget(
    const std::size_t lineCount,
    const std::size_t retainedBytes,
    const std::size_t budgetBytes) noexcept
{
    struct EndpointScratch
    {
        TVector point{};
        TVector other{};
        std::size_t line{};
    };
    constexpr std::size_t bytesPerLine = 2u * sizeof(EndpointScratch)
        + sizeof(CleaveJointAngle) + sizeof(std::uint8_t);
    return retainedBytes <= budgetBytes
        && lineCount <= (budgetBytes - retainedBytes) / bytesPerLine;
}

template <typename Line>
[[nodiscard]] std::vector<CleaveJointAngle> makeCleaveJointAngleMeasurements(
    const std::span<const Line> lines)
{
    struct Endpoint
    {
        TVector point{};
        TVector other{};
        std::size_t line{};
    };
    std::vector<Endpoint> endpoints{};
    if ( lines.size() > endpoints.max_size() / 2u )
    {
        return {};
    }
    endpoints.reserve(lines.size() * 2u);
    for ( std::size_t index = 0; index < lines.size(); ++index )
    {
        const auto& line = lines[index];
        if ( !cleaveFinitePoint(line.first) || !cleaveFinitePoint(line.second)
            || cleaveSameHandle(line.first, line.second) )
        {
            continue;
        }
        endpoints.push_back({line.first, line.second, index});
        endpoints.push_back({line.second, line.first, index});
    }
    std::sort(endpoints.begin(), endpoints.end(), [](const Endpoint& first, const Endpoint& second)
    {
        return std::tie(first.point.x, first.point.y, first.point.z, first.line)
            < std::tie(second.point.x, second.point.y, second.point.z, second.line);
    });
    std::vector<CleaveJointAngle> joints{};
    joints.reserve(endpoints.size() / 2u);
    for ( std::size_t begin = 0; begin < endpoints.size(); )
    {
        std::size_t end = begin + 1u;
        while ( end < endpoints.size() && cleaveSameHandle(endpoints[begin].point, endpoints[end].point) )
        {
            ++end;
        }
        if ( end - begin == 2u && endpoints[begin].line != endpoints[begin + 1u].line )
        {
            const auto& first = endpoints[begin];
            const auto& second = endpoints[begin + 1u];
            TVector firstDirection = first.other - first.point;
            TVector secondDirection = second.other - second.point;
            if ( cleaveNormalize(firstDirection) && cleaveNormalize(secondDirection) )
            {
                joints.push_back({first.point, first.other, second.other, first.line, second.line,
                    cleaveAngleBetweenDegrees(firstDirection, secondDirection)});
            }
        }
        begin = end;
    }
    return joints;
}

[[nodiscard]] inline std::optional<QRectF> cleaveTransformHudBadgeRect(
    const QRectF& viewport,
    const QPointF& anchor,
    const QSizeF& textSize)
{
    if ( !std::isfinite(viewport.x()) || !std::isfinite(viewport.y())
        || !std::isfinite(viewport.width()) || !std::isfinite(viewport.height())
        || !std::isfinite(anchor.x()) || !std::isfinite(anchor.y())
        || !std::isfinite(textSize.width()) || !std::isfinite(textSize.height())
        || viewport.width() <= 16.0 || viewport.height() <= 16.0
        || textSize.width() <= 0.0 || textSize.height() <= 0.0 )
    {
        return std::nullopt;
    }
    constexpr qreal margin = 8.0;
    const QRectF available = viewport.adjusted(margin, margin, -margin, -margin);
    const qreal badgeWidth = std::min(available.width(), textSize.width() + 24.0);
    const qreal badgeHeight = std::min(available.height(), textSize.height() + 12.0);
    return QRectF(
        std::clamp(anchor.x() - badgeWidth / 2.0,
            available.left(), available.right() - badgeWidth),
        std::clamp(anchor.y() + 24.0,
            available.top(), available.bottom() - badgeHeight),
        badgeWidth, badgeHeight);
}

inline double cleaveScreenAngleDegrees(const QPointF& first, const QPointF& second) noexcept
{
    constexpr double kRadiansToDegrees = 57.295779513082320876;
    double angle = std::atan2(
        -(second.y() - first.y()),
        second.x() - first.x()) * kRadiansToDegrees;
    if (angle < 0.0)
    {
        angle += 360.0;
    }
    return angle;
}

// Snaps a direction inside the supplied construction plane while retaining
// its authored length. A non-positive step is the explicit "free angle"
// state and deliberately returns the original direction unchanged.
inline TVector cleaveSnapDirectionToAngleStep(
    const TVector& direction,
    TVector reference,
    TVector planeNormal,
    const double stepDegrees) noexcept
{
    if (!std::isfinite(stepDegrees) || stepDegrees <= 0.0
        || !cleaveFinitePoint(direction) || direction.lengthSqr() <= 0.0
        || !cleaveNormalize(planeNormal) || !cleaveFinitePoint(reference))
    {
        return direction;
    }

    const double length = direction.length();
    reference = reference - planeNormal * cleaveDot(reference, planeNormal);
    if (!cleaveNormalize(reference))
    {
        return direction;
    }

    TVector projectedDirection = direction
        - planeNormal * cleaveDot(direction, planeNormal);
    if (!cleaveNormalize(projectedDirection))
    {
        return direction;
    }

    TVector tangent = cleaveCross(planeNormal, reference);
    if (!cleaveNormalize(tangent))
    {
        return direction;
    }

    constexpr double kRadiansToDegrees = 57.295779513082320876;
    constexpr double kDegreesToRadians = 0.017453292519943295769;
    const double angleDegrees = std::atan2(
        static_cast<double>(cleaveDot(projectedDirection, tangent)),
        static_cast<double>(cleaveDot(projectedDirection, reference)))
        * kRadiansToDegrees;
    const double snappedRadians = std::round(angleDegrees / stepDegrees)
        * stepDegrees * kDegreesToRadians;
    return (reference * std::cos(snappedRadians)
        + tangent * std::sin(snappedRadians)) * length;
}

} // namespace sed::render
