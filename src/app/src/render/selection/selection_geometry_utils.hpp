#pragma once

#include <QPointF>
#include <QRectF>

#include <optional>
#include <vector>

namespace sed::render
{

/**
 * Signed area test used by screen-space selection geometry.
 *
 * Positive and negative values indicate opposite winding around the segment
 * from p to q; values near zero mean the points are collinear.
 */
double screenPointOrientation(const QPointF& p, const QPointF& q, const QPointF& r);

/**
 * Returns true when two screen-space line segments touch or cross.
 *
 * Collinear overlaps and endpoint touches count as intersections, matching how
 * drag-select boxes are expected to catch wireframe edges.
 */
bool screenSegmentsIntersect(
    const QPointF& p1,
    const QPointF& p2,
    const QPointF& q1,
    const QPointF& q2);

/**
 * Returns true when a screen-space segment touches, crosses, or starts inside
 * the rectangle.
 */
bool screenSegmentIntersectsRect(const QPointF& a, const QPointF& b, const QRectF& box);

/**
 * One stable sample from the part of a screen-space segment inside a rectangle.
 *
 * The returned parameter is in [0, 1] along a -> b and lets callers interpolate
 * per-vertex values such as window depth at exactly the sampled screen point.
 * A midpoint of the clipped interval avoids testing an edge outside the marquee
 * merely because its un-clipped midpoint lies elsewhere.
 */
struct ScreenSegmentRectSample
{
    QPointF point{};
    double parameter{};
};

[[nodiscard]] std::optional<ScreenSegmentRectSample>
screenSegmentRectMidpointSample(
    const QPointF& a,
    const QPointF& b,
    const QRectF& box);

/**
 * Point-in-polygon test for projected surface polygons.
 *
 * Points on an edge are treated as inside so lasso/box selection remains
 * forgiving on exact boundaries.
 */
bool screenPointInPolygon(const QPointF& point, const std::vector<QPointF>& polygon);

/**
 * Returns true when a projected polygon overlaps the selection rectangle.
 *
 * The test covers all three useful cases: a polygon vertex inside the box, a
 * polygon edge crossing the box, or the entire box lying inside the polygon.
 */
bool screenPolygonIntersectsRect(const std::vector<QPointF>& polygon, const QRectF& box);

} // namespace sed::render
