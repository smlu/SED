#include "extrude_contained_ring.hpp"

#include "exact_orientation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace sed::level::detail
{

namespace
{

struct EdgeKey
{
    ContainedRingVertex first{};
    ContainedRingVertex second{};

    friend bool operator<(const EdgeKey& lhs, const EdgeKey& rhs) noexcept
    {
        if ( lhs.first.contour != rhs.first.contour )
        {
            return lhs.first.contour < rhs.first.contour;
        }
        if ( lhs.first.index != rhs.first.index )
        {
            return lhs.first.index < rhs.first.index;
        }
        if ( lhs.second.contour != rhs.second.contour )
        {
            return lhs.second.contour < rhs.second.contour;
        }
        return lhs.second.index < rhs.second.index;
    }
};

EdgeKey edgeKey(ContainedRingVertex first, ContainedRingVertex second)
{
    if ( EdgeKey{ second, first } < EdgeKey{ first, second } )
    {
        return { second, first };
    }
    return { first, second };
}

struct IndexedPoint
{
    TPoint2 position{};
    ContainedRingVertex vertex{};
};

struct ContourInfo
{
    std::vector<IndexedPoint> points;
    int winding{};
};

struct PathNode
{
    IndexedPoint point{};
};

using Direction = std::int32_t;

bool finite(const TPoint2& point) noexcept
{
    return std::isfinite(point.x) && std::isfinite(point.y);
}

std::optional<int> orientation(
    const TPoint2& first,
    const TPoint2& second,
    const TPoint2& third) noexcept
{
    return exactOrientationSign(
        first.x, first.y, second.x, second.y, third.x, third.y);
}

bool samePoint(const TPoint2& first, const TPoint2& second) noexcept
{
    return first.x == second.x && first.y == second.y;
}

bool onSegment(
    const TPoint2& first,
    const TPoint2& second,
    const TPoint2& point) noexcept
{
    const auto turn = orientation(first, second, point);
    if ( !turn || *turn != 0 )
    {
        return false;
    }
    return point.x >= std::min(first.x, second.x)
        && point.x <= std::max(first.x, second.x)
        && point.y >= std::min(first.y, second.y)
        && point.y <= std::max(first.y, second.y);
}

bool segmentsIntersect(
    const TPoint2& first,
    const TPoint2& second,
    const TPoint2& third,
    const TPoint2& fourth) noexcept
{
    const auto firstTurn = orientation(first, second, third);
    const auto secondTurn = orientation(first, second, fourth);
    const auto thirdTurn = orientation(third, fourth, first);
    const auto fourthTurn = orientation(third, fourth, second);
    if ( !firstTurn || !secondTurn || !thirdTurn || !fourthTurn )
    {
        return false;
    }
    if ( *firstTurn != *secondTurn && *thirdTurn != *fourthTurn
        && *firstTurn != 0 && *secondTurn != 0
        && *thirdTurn != 0 && *fourthTurn != 0 )
    {
        return true;
    }
    return (*firstTurn == 0 && onSegment(first, second, third))
        || (*secondTurn == 0 && onSegment(first, second, fourth))
        || (*thirdTurn == 0 && onSegment(third, fourth, first))
        || (*fourthTurn == 0 && onSegment(third, fourth, second));
}

bool properIntersection(
    const TPoint2& first,
    const TPoint2& second,
    const TPoint2& third,
    const TPoint2& fourth) noexcept
{
    const auto firstTurn = orientation(first, second, third);
    const auto secondTurn = orientation(first, second, fourth);
    const auto thirdTurn = orientation(third, fourth, first);
    const auto fourthTurn = orientation(third, fourth, second);
    return firstTurn && secondTurn && thirdTurn && fourthTurn
        && (*firstTurn) * (*secondTurn) < 0
        && (*thirdTurn) * (*fourthTurn) < 0;
}

int contourWinding(const std::vector<IndexedPoint>& points) noexcept
{
    if ( points.size() < 3 )
    {
        return 0;
    }
    double minimumY = std::numeric_limits<double>::infinity();
    for (const auto& point : points)
    {
        minimumY = std::min(minimumY, point.position.y);
    }
    std::optional<std::size_t> candidate;
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        if ( points[index].position.y != minimumY )
        {
            continue;
        }
        if ( !candidate
            || points[index].position.x < points[*candidate].position.x
            || (points[index].position.x == points[*candidate].position.x
                && points[index].vertex.index < points[*candidate].vertex.index) )
        {
            candidate = index;
        }
    }
    if ( candidate )
    {
        const auto& pointsAt = [&](const std::size_t index) -> const TPoint2&
        {
            return points[index % points.size()].position;
        };
        const auto turn = orientation(
            pointsAt(*candidate + points.size() - 1),
            pointsAt(*candidate),
            pointsAt(*candidate + 1));
        if ( turn && *turn != 0 )
        {
            return *turn;
        }
    }
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        const auto turn = orientation(
            points[(index + points.size() - 1) % points.size()].position,
            points[index].position,
            points[(index + 1) % points.size()].position);
        if ( turn && *turn != 0 )
        {
            return *turn;
        }
    }
    return 0;
}

bool uniqueContourIdentities(const std::vector<IndexedPoint>& points) noexcept
{
    for (std::size_t first = 0; first < points.size(); ++first)
    {
        for (std::size_t second = first + 1; second < points.size(); ++second)
        {
            if ( points[first].vertex.index == points[second].vertex.index )
            {
                return false;
            }
        }
    }
    return true;
}

bool simpleContour(const std::vector<IndexedPoint>& points) noexcept
{
    if ( points.size() < 3 || !uniqueContourIdentities(points) )
    {
        return false;
    }
    for (const auto& point : points)
    {
        if ( !finite(point.position) )
        {
            return false;
        }
    }
    for (std::size_t first = 0; first < points.size(); ++first)
    {
        for (std::size_t second = first + 1; second < points.size(); ++second)
        {
            if ( samePoint(points[first].position, points[second].position) )
            {
                return false;
            }
        }
    }
    for (std::size_t firstEdge = 0; firstEdge < points.size(); ++firstEdge)
    {
        const auto& first = points[firstEdge].position;
        const auto& second = points[(firstEdge + 1) % points.size()].position;
        if ( samePoint(first, second) )
        {
            return false;
        }
        const auto turn = orientation(
            first,
            second,
            points[(firstEdge + 2) % points.size()].position);
        if ( !turn )
        {
            return false;
        }
        if ( *turn == 0
            && !onSegment(
                first,
                points[(firstEdge + 2) % points.size()].position,
                second) )
        {
            return false;
        }
        for (std::size_t secondEdge = firstEdge + 1; secondEdge < points.size(); ++secondEdge)
        {
            const auto& third = points[secondEdge].position;
            const auto& fourth = points[(secondEdge + 1) % points.size()].position;
            const bool adjacent = secondEdge == firstEdge + 1
                || (firstEdge == 0 && secondEdge + 1 == points.size());
            if ( adjacent )
            {
                continue;
            }
            if ( segmentsIntersect(first, second, third, fourth) )
            {
                return false;
            }
        }
    }
    return contourWinding(points) != 0;
}

bool convexContour(const std::vector<IndexedPoint>& points, const int winding) noexcept
{
    if ( !simpleContour(points) || winding == 0 )
    {
        return false;
    }
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        const auto& first = points[(index + points.size() - 1) % points.size()].position;
        const auto& second = points[index].position;
        const auto& third = points[(index + 1) % points.size()].position;
        const auto turn = orientation(first, second, third);
        if ( !turn || *turn != winding )
        {
            return false;
        }
    }
    return true;
}

bool pointStrictlyInsideConvex(
    const std::vector<IndexedPoint>& contour,
    const TPoint2& point,
    const int winding)
{
    for (std::size_t index = 0; index < contour.size(); ++index)
    {
        const auto turn = orientation(
            contour[index].position,
            contour[(index + 1) % contour.size()].position,
            point);
        if ( !turn || *turn != winding )
        {
            return false;
        }
    }
    return true;
}

bool strictlyContained(
    const std::vector<IndexedPoint>& source,
    const std::vector<IndexedPoint>& rectangle)
{
    const int rectangleWinding = contourWinding(rectangle);
    if ( rectangleWinding == 0 )
    {
        return false;
    }
    for (const auto& point : source)
    {
        if ( !pointStrictlyInsideConvex(rectangle, point.position, rectangleWinding) )
        {
            return false;
        }
    }
    return true;
}

bool bridgeVisible(
    const std::vector<IndexedPoint>& source,
    const std::vector<IndexedPoint>& rectangle,
    const std::size_t sourceIndex,
    const std::size_t rectangleIndex)
{
    const auto& first = source[sourceIndex].position;
    const auto& second = rectangle[rectangleIndex].position;
    for (std::size_t edge = 0; edge < source.size(); ++edge)
    {
        const auto next = (edge + 1) % source.size();
        const auto& third = source[edge].position;
        const auto& fourth = source[next].position;
        if ( !segmentsIntersect(first, second, third, fourth) )
        {
            continue;
        }
        if ( samePoint(first, third) || samePoint(first, fourth) )
        {
            if ( onSegment(first, second, third) && onSegment(first, second, fourth) )
            {
                return false;
            }
            continue;
        }
        return false;
    }
    for (std::size_t edge = 0; edge < rectangle.size(); ++edge)
    {
        const auto next = (edge + 1) % rectangle.size();
        const auto& third = rectangle[edge].position;
        const auto& fourth = rectangle[next].position;
        if ( !segmentsIntersect(first, second, third, fourth) )
        {
            continue;
        }
        if ( samePoint(second, third) || samePoint(second, fourth) )
        {
            continue;
        }
        return false;
    }
    return true;
}

bool rightmostFirst(
    const IndexedPoint& first,
    const IndexedPoint& second) noexcept
{
    if ( first.position.x != second.position.x )
    {
        return first.position.x > second.position.x;
    }
    if ( first.position.y != second.position.y )
    {
        return first.position.y < second.position.y;
    }
    return first.vertex.index < second.vertex.index;
}

std::vector<std::size_t> bridgeOrder(const std::vector<IndexedPoint>& points)
{
    std::vector<std::size_t> order;
    order.reserve(points.size());
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        order.push_back(index);
    }
    std::sort(order.begin(), order.end(), [&](const std::size_t first,
        const std::size_t second)
    {
        return rightmostFirst(points[first], points[second]);
    });
    return order;
}

std::vector<PathNode> makeBridgePath(
    const std::vector<IndexedPoint>& source,
    const std::vector<IndexedPoint>& rectangle,
    const std::size_t sourceBridge,
    const std::size_t rectangleBridge,
    const int sourceWinding,
    const int rectangleWinding)
{
    // The supplied rectangle order is the output winding contract. Only the
    // source hole is reversed when it has the same winding as the rectangle.
    const bool sourceForward = sourceWinding != rectangleWinding;
    std::vector<PathNode> path;
    path.reserve(source.size() + rectangle.size() + 2);
    const auto cyclicIndex = [](const std::size_t start,
        const std::size_t count, const std::size_t size, const bool forward)
    {
        const auto offset = count % size;
        if ( forward )
        {
            return (start + offset) % size;
        }
        return start >= offset ? start - offset : size - (offset - start);
    };
    const auto append = [&](const IndexedPoint& point)
    {
        path.push_back(PathNode{ point });
    };
    append(rectangle[rectangleBridge]);
    for (std::size_t count = 1; count < rectangle.size(); ++count)
    {
        const auto index = cyclicIndex(
            rectangleBridge, count, rectangle.size(), true);
        append(rectangle[index]);
    }
    append(rectangle[rectangleBridge]);
    append(source[sourceBridge]);
    for (std::size_t count = 1; count < source.size(); ++count)
    {
        const auto index = cyclicIndex(
            sourceBridge, count, source.size(), sourceForward);
        append(source[index]);
    }
    append(source[sourceBridge]);
    return path;
}

bool pointStrictlyInsideTriangle(
    const TPoint2& first,
    const TPoint2& second,
    const TPoint2& third,
    const TPoint2& point) noexcept
{
    const auto firstTurn = orientation(first, second, point);
    const auto secondTurn = orientation(second, third, point);
    const auto thirdTurn = orientation(third, first, point);
    if ( !firstTurn || !secondTurn || !thirdTurn
        || *firstTurn == 0 || *secondTurn == 0 || *thirdTurn == 0 )
    {
        return false;
    }
    return (*firstTurn == *secondTurn) && (*firstTurn == *thirdTurn);
}

bool diagonalIsClear(
    const std::vector<PathNode>& path,
    const std::vector<std::size_t>& active,
    const std::size_t previous,
    const std::size_t next)
{
    const auto& first = path[previous].point.position;
    const auto& second = path[next].point.position;
    for (std::size_t edge = 0; edge < active.size(); ++edge)
    {
        const auto edgeFirst = active[edge];
        const auto edgeSecond = active[(edge + 1) % active.size()];
        if ( edgeFirst == previous || edgeFirst == next
            || edgeSecond == previous || edgeSecond == next )
        {
            continue;
        }
        const auto& third = path[edgeFirst].point.position;
        const auto& fourth = path[edgeSecond].point.position;
        if ( !segmentsIntersect(first, second, third, fourth) )
        {
            continue;
        }
        if ( properIntersection(first, second, third, fourth) )
        {
            return false;
        }
        if ( samePoint(first, third) || samePoint(first, fourth)
            || samePoint(second, third) || samePoint(second, fourth) )
        {
            continue;
        }
        return false;
    }
    return true;
}

bool earIsValid(
    const std::vector<PathNode>& path,
    const std::vector<std::size_t>& active,
    const std::size_t position,
    const int winding)
{
    const auto previous = active[(position + active.size() - 1) % active.size()];
    const auto current = active[position];
    const auto next = active[(position + 1) % active.size()];
    const auto& first = path[previous].point.position;
    const auto& second = path[current].point.position;
    const auto& third = path[next].point.position;
    const auto turn = orientation(first, second, third);
    if ( !turn || *turn != winding )
    {
        return false;
    }
    if ( !diagonalIsClear(path, active, previous, next) )
    {
        return false;
    }
    for (const auto index : active)
    {
        if ( index == previous || index == current || index == next )
        {
            continue;
        }
        const auto& point = path[index].point.position;
        // A diagonal or an ear boundary must not jump over a distinct active
        // contour point. Exact-coordinate aliases are the repeated bridge
        // endpoints used by the weakly-simple path and are permitted.
        if ( samePoint(point, first) || samePoint(point, second)
            || samePoint(point, third) )
        {
            continue;
        }
        if ( onSegment(first, second, point)
            || onSegment(second, third, point)
            || onSegment(third, first, point)
            || pointStrictlyInsideTriangle(first, second, third, point) )
        {
            return false;
        }
    }
    return true;
}

bool triangulatePath(
    const std::vector<PathNode>& path,
    const int winding,
    std::vector<ContainedRingTriangle>& triangles)
{
    if ( path.size() < 3 )
    {
        return false;
    }
    std::vector<std::size_t> active;
    active.reserve(path.size());
    for (std::size_t index = 0; index < path.size(); ++index)
    {
        active.push_back(index);
    }
    while ( active.size() > 3 )
    {
        bool clipped = false;
        for (std::size_t position = 0; position < active.size(); ++position)
        {
            if ( !earIsValid(path, active, position, winding) )
            {
                continue;
            }
            const auto previous = active[(position + active.size() - 1) % active.size()];
            const auto current = active[position];
            const auto next = active[(position + 1) % active.size()];
            triangles.push_back(ContainedRingTriangle{
                { path[previous].point.vertex,
                    path[current].point.vertex,
                    path[next].point.vertex } });
            active.erase(active.begin() + static_cast<std::ptrdiff_t>(position));
            clipped = true;
            break;
        }
        if ( !clipped )
        {
            return false;
        }
    }
    const auto first = active[0];
    const auto second = active[1];
    const auto third = active[2];
    const auto turn = orientation(
        path[first].point.position,
        path[second].point.position,
        path[third].point.position);
    if ( !turn || *turn != winding )
    {
        return false;
    }
    triangles.push_back(ContainedRingTriangle{
        { path[first].point.vertex,
            path[second].point.vertex,
            path[third].point.vertex } });
    return true;
}

const IndexedPoint* findPoint(
    const ContourInfo& source,
    const ContourInfo& rectangle,
    const ContainedRingVertex vertex) noexcept
{
    const std::vector<IndexedPoint>* points = nullptr;
    if ( vertex.contour == ContainedRingContour::source )
    {
        points = &source.points;
    }
    else if ( vertex.contour == ContainedRingContour::rectangle )
    {
        points = &rectangle.points;
    }
    else
    {
        return nullptr;
    }
    for (const auto& point : *points)
    {
        if ( point.vertex.index == vertex.index )
        {
            return &point;
        }
    }
    return nullptr;
}

bool sameGeometricSegment(
    const TPoint2& first,
    const TPoint2& second,
    const TPoint2& third,
    const TPoint2& fourth) noexcept
{
    return (samePoint(first, third) && samePoint(second, fourth))
        || (samePoint(first, fourth) && samePoint(second, third));
}

bool triangleOutsideSource(
    const ContourInfo& source,
    const ContourInfo& rectangle,
    const ContainedRingTriangle& triangle)
{
    std::array<TPoint2, 3> points{};
    for (std::size_t index = 0; index < 3; ++index)
    {
        const auto* point = findPoint(source, rectangle, triangle.vertices[index]);
        if ( point == nullptr )
        {
            return false;
        }
        points[index] = point->position;
    }
    for (const auto& sourcePoint : source.points)
    {
        if ( pointStrictlyInsideTriangle(points[0], points[1], points[2],
                sourcePoint.position) )
        {
            return false;
        }
    }
    for (std::size_t edge = 0; edge < 3; ++edge)
    {
        const auto& first = points[edge];
        const auto& second = points[(edge + 1) % 3];
        for (std::size_t sourceEdge = 0; sourceEdge < source.points.size(); ++sourceEdge)
        {
            const auto& third = source.points[sourceEdge].position;
            const auto& fourth = source.points[(sourceEdge + 1) % source.points.size()].position;
            if ( !segmentsIntersect(first, second, third, fourth) )
            {
                continue;
            }
            if ( sameGeometricSegment(first, second, third, fourth)
                || samePoint(first, third) || samePoint(first, fourth)
                || samePoint(second, third) || samePoint(second, fourth) )
            {
                continue;
            }
            return false;
        }
    }
    return true;
}

bool trianglesDoNotOverlap(
    const ContourInfo& source,
    const ContourInfo& rectangle,
    const ContainedRingTriangle& first,
    const ContainedRingTriangle& second)
{
    std::array<TPoint2, 3> firstPoints{};
    std::array<TPoint2, 3> secondPoints{};
    for (std::size_t index = 0; index < 3; ++index)
    {
        const auto* firstPoint = findPoint(source, rectangle, first.vertices[index]);
        const auto* secondPoint = findPoint(source, rectangle, second.vertices[index]);
        if ( firstPoint == nullptr || secondPoint == nullptr )
        {
            return false;
        }
        firstPoints[index] = firstPoint->position;
        secondPoints[index] = secondPoint->position;
    }
    for (std::size_t firstEdge = 0; firstEdge < 3; ++firstEdge)
    {
        const auto& firstStart = firstPoints[firstEdge];
        const auto& firstEnd = firstPoints[(firstEdge + 1) % 3];
        for (std::size_t secondEdge = 0; secondEdge < 3; ++secondEdge)
        {
            const auto& secondStart = secondPoints[secondEdge];
            const auto& secondEnd = secondPoints[(secondEdge + 1) % 3];
            if ( !segmentsIntersect(firstStart, firstEnd, secondStart, secondEnd) )
            {
                continue;
            }
            if ( properIntersection(firstStart, firstEnd, secondStart, secondEnd) )
            {
                return false;
            }
            if ( sameGeometricSegment(firstStart, firstEnd, secondStart, secondEnd)
                || samePoint(firstStart, secondStart)
                || samePoint(firstStart, secondEnd)
                || samePoint(firstEnd, secondStart)
                || samePoint(firstEnd, secondEnd) )
            {
                continue;
            }
            return false;
        }
    }
    for (const auto& point : secondPoints)
    {
        if ( pointStrictlyInsideTriangle(
                firstPoints[0], firstPoints[1], firstPoints[2], point) )
        {
            return false;
        }
    }
    for (const auto& point : firstPoints)
    {
        if ( pointStrictlyInsideTriangle(
                secondPoints[0], secondPoints[1], secondPoints[2], point) )
        {
            return false;
        }
    }
    return true;
}

bool resultIsValid(
    const ContourInfo& source,
    const ContourInfo& rectangle,
    const std::vector<ContainedRingTriangle>& triangles,
    const int winding)
{
    if ( triangles.empty() )
    {
        return false;
    }
    std::map<EdgeKey, std::array<int, 2>> incidence;
    for (const auto& triangle : triangles)
    {
        std::array<const IndexedPoint*, 3> points{};
        for (std::size_t index = 0; index < 3; ++index)
        {
            points[index] = findPoint(source, rectangle, triangle.vertices[index]);
            if ( points[index] == nullptr )
            {
                return false;
            }
        }
        const auto turn = orientation(
            points[0]->position, points[1]->position, points[2]->position);
        if ( !turn || *turn != winding )
        {
            return false;
        }
        if ( !triangleOutsideSource(source, rectangle, triangle) )
        {
            return false;
        }
        for (std::size_t edge = 0; edge < 3; ++edge)
        {
            const auto first = triangle.vertices[edge];
            const auto second = triangle.vertices[(edge + 1) % 3];
            const auto key = edgeKey(first, second);
            auto& directions = incidence[key];
            if ( first == key.first )
            {
                ++directions[0];
            }
            else
            {
                ++directions[1];
            }
        }
    }
    std::map<EdgeKey, int> boundary;
    const auto addBoundary = [&](const std::vector<IndexedPoint>& points,
        const int direction)
    {
        for (std::size_t index = 0; index < points.size(); ++index)
        {
            const auto next = direction > 0
                ? (index + 1 == points.size() ? 0 : index + 1)
                : (index == 0 ? points.size() - 1 : index - 1);
            const auto first = points[index].vertex;
            const auto second = points[next].vertex;
            const auto key = edgeKey(first, second);
            boundary[key] = first == key.first ? 0 : 1;
        }
    };
    const int rectangleDirection = 1;
    const int sourceDirection = source.winding == rectangle.winding ? -1 : 1;
    addBoundary(rectangle.points, rectangleDirection);
    addBoundary(source.points, sourceDirection);
    for (const auto& [edge, directions] : incidence)
    {
        const auto boundaryDirection = boundary.find(edge);
        const int total = directions[0] + directions[1];
        if ( boundaryDirection != boundary.end() )
        {
            if ( total != 1 || directions[boundaryDirection->second] != 1
                || directions[1 - boundaryDirection->second] != 0 )
            {
                return false;
            }
        }
        else if ( total != 2 || directions[0] != 1 || directions[1] != 1 )
        {
            return false;
        }
    }
    for (const auto& [edge, unused] : boundary)
    {
        (void)unused;
        if ( incidence.find(edge) == incidence.end() )
        {
            return false;
        }
    }
    for (std::size_t first = 0; first < triangles.size(); ++first)
    {
        for (std::size_t second = first + 1; second < triangles.size(); ++second)
        {
            if ( !trianglesDoNotOverlap(source, rectangle,
                    triangles[first], triangles[second]) )
            {
                return false;
            }
        }
    }
    return true;
}

} // namespace

bool buildContainedExtrudeRing(
    const std::span<const ContainedRingPoint> source,
    const std::span<const ContainedRingPoint> rectangle,
    ContainedRingResult& output)
{
    constexpr auto maximumDirection =
        static_cast<std::size_t>(std::numeric_limits<Direction>::max());
    // Keep the bounded path length below the signed range used by the
    // surrounding level-editing APIs. Reject before any narrowing.
    if ( source.size() > maximumDirection - 2
        || rectangle.size() > maximumDirection - 2
        || source.size() > maximumDirection - 2 - rectangle.size() )
    {
        return false;
    }
    ContourInfo sourceContour;
    sourceContour.points.reserve(source.size());
    for (const auto& point : source)
    {
        sourceContour.points.push_back(IndexedPoint{
            point.position,
            { ContainedRingContour::source, point.index } });
    }
    ContourInfo rectangleContour;
    rectangleContour.points.reserve(rectangle.size());
    for (const auto& point : rectangle)
    {
        rectangleContour.points.push_back(IndexedPoint{
            point.position,
            { ContainedRingContour::rectangle, point.index } });
    }
    sourceContour.winding = contourWinding(sourceContour.points);
    rectangleContour.winding = contourWinding(rectangleContour.points);
    if ( !simpleContour(sourceContour.points)
        || sourceContour.winding == 0
        || !convexContour(rectangleContour.points, rectangleContour.winding)
        || !strictlyContained(sourceContour.points, rectangleContour.points) )
    {
        return false;
    }

    const auto sourceOrder = bridgeOrder(sourceContour.points);
    const auto rectangleOrder = bridgeOrder(rectangleContour.points);
    std::optional<std::pair<std::size_t, std::size_t>> bridge;
    for (const auto sourceIndex : sourceOrder)
    {
        for (const auto rectangleIndex : rectangleOrder)
        {
            if ( !bridgeVisible(sourceContour.points, rectangleContour.points,
                    sourceIndex, rectangleIndex) )
            {
                continue;
            }
            bridge = std::pair{ sourceIndex, rectangleIndex };
            break;
        }
        if ( bridge )
        {
            break;
        }
    }
    if ( !bridge )
    {
        return false;
    }

    auto path = makeBridgePath(
        sourceContour.points, rectangleContour.points,
        bridge->first, bridge->second,
        sourceContour.winding, rectangleContour.winding);
    const int winding = rectangleContour.winding;
    std::vector<ContainedRingTriangle> triangles;
    triangles.reserve(path.size() - 2);
    if ( !triangulatePath(path, winding, triangles)
        || !resultIsValid(sourceContour, rectangleContour, triangles, winding) )
    {
        return false;
    }

    ContainedRingResult candidate;
    candidate.triangles = std::move(triangles);
    candidate.sourceBridge = sourceContour.points[bridge->first].vertex.index;
    candidate.rectangleBridge = rectangleContour.points[bridge->second].vertex.index;
    output = std::move(candidate);
    return true;
}

} // namespace sed::level::detail
