#include "ui/viewport/cleave_shape_utils.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <utility>

namespace sed::ui::viewport
{
namespace
{

constexpr double kMinimumDimension = 0.005;
constexpr double kMaximumDimension = 10000.0;
constexpr int kMinimumCount = 3;
constexpr int kMaximumCount = 64;

bool validDimension(const double value)
{
    return std::isfinite(value)
        && value >= kMinimumDimension
        && value <= kMaximumDimension;
}

bool validCount(const int value)
{
    return value >= kMinimumCount && value <= kMaximumCount;
}

CleaveShapePoint2D point(const double x, const double y)
{
    return { x, y };
}

double signedArea(const std::vector<CleaveShapePoint2D>& vertices)
{
    if ( vertices.size() < 3 )
    {
        return 0.0;
    }

    double twiceArea = 0.0;
    for ( std::size_t index = 0; index < vertices.size(); ++index )
    {
        const CleaveShapePoint2D& current = vertices[index];
        const CleaveShapePoint2D& next = vertices[(index + 1) % vertices.size()];
        twiceArea += current.x * next.y - current.y * next.x;
    }
    return 0.5 * twiceArea;
}

void addConvexCell(CleaveShapePlan& plan)
{
    plan.convexCells.push_back({ plan.perimeter });
    plan.convexClosedPerimeter = true;
}

CleaveShapePlan makeRectangle(const double width, const double height)
{
    const double halfWidth = width * 0.5;
    const double halfHeight = height * 0.5;

    CleaveShapePlan plan{};
    plan.perimeter = {
        point(-halfWidth, -halfHeight),
        point(halfWidth, -halfHeight),
        point(halfWidth, halfHeight),
        point(-halfWidth, halfHeight),
    };
    addConvexCell(plan);
    return plan;
}

std::optional<CleaveShapePlan> makeRegularPolygon(const CleaveShapeSpec& spec)
{
    if ( !validCount(spec.segments) )
    {
        return std::nullopt;
    }

    const double radius = spec.width * 0.5;
    CleaveShapePlan plan{};
    plan.perimeter.reserve(static_cast<std::size_t>(spec.segments));
    for ( int index = 0; index < spec.segments; ++index )
    {
        const double angle = std::numbers::pi_v<double> * 0.5
            + 2.0 * std::numbers::pi_v<double> * static_cast<double>(index)
                / static_cast<double>(spec.segments);
        plan.perimeter.push_back(point(
            radius * std::cos(angle),
            radius * std::sin(angle)));
    }
    addConvexCell(plan);
    return plan;
}

std::optional<CleaveShapePlan> makeArch(
    const CleaveShapeSpec& spec,
    const double height)
{
    if ( !validCount(spec.segments) )
    {
        return std::nullopt;
    }

    const double halfWidth = spec.width * 0.5;
    const double halfHeight = height * 0.5;
    CleaveShapePlan plan{};
    plan.perimeter.reserve(static_cast<std::size_t>(spec.segments + 3));
    plan.perimeter.push_back(point(-halfWidth, -halfHeight));
    plan.perimeter.push_back(point(halfWidth, -halfHeight));
    plan.perimeter.push_back(point(halfWidth, 0.0));

    // The top half ellipse joins the vertical jambs at the spring line.
    for ( int index = 1; index <= spec.segments; ++index )
    {
        const double angle = std::numbers::pi_v<double>
            * static_cast<double>(index) / static_cast<double>(spec.segments);
        plan.perimeter.push_back(point(
            halfWidth * std::cos(angle),
            halfHeight * std::sin(angle)));
    }

    addConvexCell(plan);
    return plan;
}

std::optional<CleaveShapePlan> makeStar(
    const CleaveShapeSpec& spec,
    const double height)
{
    if ( !validCount(spec.points) )
    {
        return std::nullopt;
    }

    const std::size_t pointCount = static_cast<std::size_t>(spec.points);
    const double radiusX = spec.width * 0.5;
    const double radiusY = height * 0.5;
    constexpr double kInnerRadiusRatio = 0.45;

    CleaveShapePlan plan{};
    plan.convexClosedPerimeter = false;
    plan.perimeter.reserve(pointCount * 2);
    for ( std::size_t index = 0; index < pointCount * 2; ++index )
    {
        const double angle = std::numbers::pi_v<double> * 0.5
            + std::numbers::pi_v<double> * static_cast<double>(index)
                / static_cast<double>(pointCount);
        const double radius = index % 2 == 0 ? 1.0 : kInnerRadiusRatio;
        plan.perimeter.push_back(point(
            radiusX * radius * std::cos(angle),
            radiusY * radius * std::sin(angle)));
    }

    plan.decompositionStrokes.reserve(pointCount);
    CleaveShapeCell2D centerCell{};
    centerCell.vertices.reserve(pointCount);
    for ( std::size_t index = 0; index < pointCount; ++index )
    {
        const std::size_t firstValley = index * 2 + 1;
        const std::size_t nextValley = ((index + 1) % pointCount) * 2 + 1;
        const CleaveShapePoint2D first = plan.perimeter[firstValley];
        const CleaveShapePoint2D second = plan.perimeter[nextValley];
        plan.decompositionStrokes.push_back({ first, second });
        centerCell.vertices.push_back(first);
    }
    plan.convexCells.push_back(std::move(centerCell));

    for ( std::size_t index = 0; index < pointCount; ++index )
    {
        const std::size_t tip = index * 2;
        const std::size_t previousValley = (tip + pointCount * 2 - 1)
            % (pointCount * 2);
        const std::size_t nextValley = (tip + 1) % (pointCount * 2);
        plan.convexCells.push_back({ {
            plan.perimeter[previousValley],
            plan.perimeter[tip],
            plan.perimeter[nextValley],
        } });
    }

    return plan;
}

bool isValidPlan(const CleaveShapePlan& plan)
{
    if ( plan.perimeter.size() < 3 || plan.convexCells.empty() )
    {
        return false;
    }

    for ( const CleaveShapePoint2D& vertex : plan.perimeter )
    {
        if ( !std::isfinite(vertex.x) || !std::isfinite(vertex.y) )
        {
            return false;
        }
    }
    for ( std::size_t index = 0; index < plan.perimeter.size(); ++index )
    {
        const CleaveShapePoint2D& first = plan.perimeter[index];
        const CleaveShapePoint2D& second = plan.perimeter[
            (index + 1) % plan.perimeter.size()];
        const double edgeX = second.x - first.x;
        const double edgeY = second.y - first.y;
        if ( !std::isfinite(edgeX * edgeX + edgeY * edgeY)
            || edgeX * edgeX + edgeY * edgeY <= 0.0 )
        {
            return false;
        }
    }
    const double perimeterArea = signedArea(plan.perimeter);
    if ( !std::isfinite(perimeterArea) || perimeterArea <= 0.0 )
    {
        return false;
    }

    double cellArea = 0.0;
    for ( const CleaveShapeStroke2D& stroke : plan.decompositionStrokes )
    {
        const double deltaX = stroke.second.x - stroke.first.x;
        const double deltaY = stroke.second.y - stroke.first.y;
        if ( !std::isfinite(stroke.first.x) || !std::isfinite(stroke.first.y)
            || !std::isfinite(stroke.second.x) || !std::isfinite(stroke.second.y)
            || !std::isfinite(deltaX * deltaX + deltaY * deltaY)
            || deltaX * deltaX + deltaY * deltaY <= 0.0 )
        {
            return false;
        }
    }
    for ( const CleaveShapeCell2D& cell : plan.convexCells )
    {
        if ( cell.vertices.size() < 3 )
        {
            return false;
        }
        for ( const CleaveShapePoint2D& vertex : cell.vertices )
        {
            if ( !std::isfinite(vertex.x) || !std::isfinite(vertex.y) )
            {
                return false;
            }
        }
        const double area = signedArea(cell.vertices);
        if ( !std::isfinite(area) || area <= 0.0 )
        {
            return false;
        }
        cellArea += area;

        for ( std::size_t index = 0; index < cell.vertices.size(); ++index )
        {
            const CleaveShapePoint2D& first = cell.vertices[index];
            const CleaveShapePoint2D& second = cell.vertices[
                (index + 1) % cell.vertices.size()];
            const CleaveShapePoint2D& third = cell.vertices[
                (index + 2) % cell.vertices.size()];
            const double firstX = second.x - first.x;
            const double firstY = second.y - first.y;
            const double secondX = third.x - second.x;
            const double secondY = third.y - second.y;
            const double positiveProduct = firstX * secondY;
            const double negativeProduct = firstY * secondX;
            const double turn = positiveProduct - negativeProduct;
            const double turnRoundoff = 64.0
                * std::numeric_limits<double>::epsilon()
                * (std::abs(positiveProduct) + std::abs(negativeProduct));
            const double edgeLengthSquared = firstX * firstX + firstY * firstY;
            if ( !std::isfinite(turn) || !std::isfinite(turnRoundoff)
                || !std::isfinite(edgeLengthSquared) || edgeLengthSquared <= 0.0
                || turn <= turnRoundoff )
            {
                return false;
            }
        }
    }

    const double areaTolerance = 1024.0
        * std::numeric_limits<double>::epsilon()
        * std::max(perimeterArea, cellArea);
    return std::isfinite(cellArea) && std::isfinite(areaTolerance)
        && std::abs(cellArea - perimeterArea) <= areaTolerance;
}

} // namespace

std::optional<CleaveShapePlan> makeCleaveShapePlan(const CleaveShapeSpec& spec)
{
    if ( !validDimension(spec.width) || !validDimension(spec.height) )
    {
        return std::nullopt;
    }

    const double linkedHeight = spec.dimensionsLinked ? spec.width : spec.height;
    std::optional<CleaveShapePlan> plan;
    switch ( spec.kind )
    {
        case CleaveShapeKind::Square:
            plan = makeRectangle(spec.width, spec.width);
            break;

        case CleaveShapeKind::Rectangle:
            plan = makeRectangle(spec.width, linkedHeight);
            break;

        case CleaveShapeKind::RegularPolygon:
            // Width is the circumcircle diameter; a regular polygon always
            // keeps equal X/Y radii even if stale unlinked settings remain.
            plan = makeRegularPolygon(spec);
            break;

        case CleaveShapeKind::Arch:
            plan = makeArch(spec, linkedHeight);
            break;

        case CleaveShapeKind::Star:
            plan = makeStar(spec, linkedHeight);
            break;

        default:
            return std::nullopt;
    }

    if ( !plan || !isValidPlan(*plan) )
    {
        return std::nullopt;
    }
    return plan;
}

} // namespace sed::ui::viewport
