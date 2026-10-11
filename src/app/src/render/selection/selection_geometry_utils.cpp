#include "render/selection/selection_geometry_utils.hpp"

#include "sed/math_utils.hpp"

#include <array>
#include <algorithm>
#include <cmath>

namespace sed::render
{
namespace
{

constexpr double kSegmentBoundsEpsilon = 1.0e-4;
constexpr double kOrientationEpsilon = 1.0e-5;

bool pointWithinSegmentBounds(const QPointF& a, const QPointF& b, const QPointF& point)
{
    return point.x() >= std::min(a.x(), b.x()) - kSegmentBoundsEpsilon
        && point.x() <= std::max(a.x(), b.x()) + kSegmentBoundsEpsilon
        && point.y() >= std::min(a.y(), b.y()) - kSegmentBoundsEpsilon
        && point.y() <= std::max(a.y(), b.y()) + kSegmentBoundsEpsilon;
}

} // namespace

double screenPointOrientation(const QPointF& p, const QPointF& q, const QPointF& r)
{
    return (q.x() - p.x()) * (r.y() - p.y()) - (q.y() - p.y()) * (r.x() - p.x());
}

bool screenSegmentsIntersect(
    const QPointF& p1,
    const QPointF& p2,
    const QPointF& q1,
    const QPointF& q2)
{
    const double o1 = screenPointOrientation(p1, p2, q1);
    const double o2 = screenPointOrientation(p1, p2, q2);
    const double o3 = screenPointOrientation(q1, q2, p1);
    const double o4 = screenPointOrientation(q1, q2, p2);

    const bool proper =
        ((o1 > kOrientationEpsilon && o2 < -kOrientationEpsilon)
            || (o1 < -kOrientationEpsilon && o2 > kOrientationEpsilon))
        && ((o3 > kOrientationEpsilon && o4 < -kOrientationEpsilon)
            || (o3 < -kOrientationEpsilon && o4 > kOrientationEpsilon));
    if (proper)
    {
        return true;
    }

    // Exact-edge cases are common when a selection box lands on projected grid
    // or wireframe pixels, so test collinear endpoints explicitly.
    if (std::abs(o1) <= kOrientationEpsilon && pointWithinSegmentBounds(p1, p2, q1)) return true;
    if (std::abs(o2) <= kOrientationEpsilon && pointWithinSegmentBounds(p1, p2, q2)) return true;
    if (std::abs(o3) <= kOrientationEpsilon && pointWithinSegmentBounds(q1, q2, p1)) return true;
    if (std::abs(o4) <= kOrientationEpsilon && pointWithinSegmentBounds(q1, q2, p2)) return true;
    return false;
}

bool screenSegmentIntersectsRect(const QPointF& a, const QPointF& b, const QRectF& box)
{
    if (box.contains(a) || box.contains(b))
    {
        return true;
    }

    const double segMinX = std::min(a.x(), b.x());
    const double segMaxX = std::max(a.x(), b.x());
    const double segMinY = std::min(a.y(), b.y());
    const double segMaxY = std::max(a.y(), b.y());
    if (segMaxX < box.left() || segMinX > box.right() || segMaxY < box.top() || segMinY > box.bottom())
    {
        return false;
    }

    const QPointF r0(box.left(), box.top());
    const QPointF r1(box.right(), box.top());
    const QPointF r2(box.right(), box.bottom());
    const QPointF r3(box.left(), box.bottom());
    return screenSegmentsIntersect(a, b, r0, r1)
        || screenSegmentsIntersect(a, b, r1, r2)
        || screenSegmentsIntersect(a, b, r2, r3)
        || screenSegmentsIntersect(a, b, r3, r0);
}

std::optional<ScreenSegmentRectSample> screenSegmentRectMidpointSample(
    const QPointF& a,
    const QPointF& b,
    const QRectF& box)
{
    if (!std::isfinite(a.x()) || !std::isfinite(a.y())
        || !std::isfinite(b.x()) || !std::isfinite(b.y()))
    {
        return std::nullopt;
    }

    const QRectF normalized = box.normalized();
    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();
    double enter = 0.0;
    double leave = 1.0;

    const auto clipBoundary = [&enter, &leave](
        const double direction,
        const double distance) -> bool
    {
        if (std::abs(direction) <= sed::math::kTinyEpsilon)
        {
            return distance >= -kSegmentBoundsEpsilon;
        }

        const double parameter = distance / direction;
        if (direction < 0.0)
        {
            enter = std::max(enter, parameter);
        }
        else
        {
            leave = std::min(leave, parameter);
        }
        return enter <= leave + kSegmentBoundsEpsilon;
    };

    if (!clipBoundary(-dx, a.x() - normalized.left())
        || !clipBoundary(dx, normalized.right() - a.x())
        || !clipBoundary(-dy, a.y() - normalized.top())
        || !clipBoundary(dy, normalized.bottom() - a.y()))
    {
        return std::nullopt;
    }

    enter = std::clamp(enter, 0.0, 1.0);
    leave = std::clamp(leave, 0.0, 1.0);
    if (enter > leave + kSegmentBoundsEpsilon)
    {
        return std::nullopt;
    }

    const double parameter = (enter + leave) * 0.5;
    return ScreenSegmentRectSample{
        QPointF(a.x() + dx * parameter, a.y() + dy * parameter),
        parameter };
}

bool screenPointInPolygon(const QPointF& point, const std::vector<QPointF>& polygon)
{
    if (polygon.size() < 3)
    {
        return false;
    }

    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++)
    {
        const QPointF& pi = polygon[i];
        const QPointF& pj = polygon[j];

        // Treat boundary hits as selected; this keeps face selection stable
        // when the drag box aligns exactly with an edge.
        if (std::abs(screenPointOrientation(pj, pi, point)) <= kOrientationEpsilon
            && pointWithinSegmentBounds(pj, pi, point))
        {
            return true;
        }

        const bool crosses = ((pi.y() > point.y()) != (pj.y() > point.y()));
        if (!crosses)
        {
            continue;
        }

        const double denom = pj.y() - pi.y();
        if (std::abs(denom) <= sed::math::kTinyEpsilon)
        {
            continue;
        }

        const double xAtY = pi.x() + (point.y() - pi.y()) * (pj.x() - pi.x()) / denom;
        if (point.x() < xAtY)
        {
            inside = !inside;
        }
    }

    return inside;
}

bool screenPolygonIntersectsRect(const std::vector<QPointF>& polygon, const QRectF& box)
{
    if (polygon.empty())
    {
        return false;
    }

    // 1. Any polygon vertex inside the box is an overlap.
    for (const QPointF& point : polygon)
    {
        if (box.contains(point))
        {
            return true;
        }
    }

    // 2. Any polygon edge crossing the box is an overlap.
    if (polygon.size() >= 2)
    {
        for (std::size_t i = 0; i < polygon.size(); ++i)
        {
            const QPointF& a = polygon[i];
            const QPointF& b = polygon[(i + 1) % polygon.size()];
            if (screenSegmentIntersectsRect(a, b, box))
            {
                return true;
            }
        }
    }

    // 3. The box can be fully enclosed by the polygon without crossing edges.
    if (polygon.size() >= 3)
    {
        const std::array<QPointF, 4> corners{
            QPointF(box.left(), box.top()),
            QPointF(box.right(), box.top()),
            QPointF(box.right(), box.bottom()),
            QPointF(box.left(), box.bottom()),
        };
        for (const QPointF& corner : corners)
        {
            if (screenPointInPolygon(corner, polygon))
            {
                return true;
            }
        }
    }

    return false;
}

} // namespace sed::render
