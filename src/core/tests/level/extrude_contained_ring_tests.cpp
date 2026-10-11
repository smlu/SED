#include "test_common.hpp"

#include "../../src/level/extrude_contained_ring.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <vector>

namespace sed::tests
{

namespace
{

using sed::TPoint2;
using sed::level::detail::ContainedRingContour;
using sed::level::detail::ContainedRingPoint;
using sed::level::detail::ContainedRingResult;
using sed::level::detail::ContainedRingTriangle;
using sed::level::detail::ContainedRingVertex;

ContainedRingPoint point(const double x, const double y, const std::size_t index)
{
    return ContainedRingPoint{ TPoint2{ x, y }, index };
}

std::vector<ContainedRingPoint> rectangle()
{
    return {
        point(-3.0, -2.0, 100),
        point(3.0, -2.0, 110),
        point(3.0, 2.0, 120),
        point(-3.0, 2.0, 130),
    };
}

std::vector<ContainedRingPoint> sourceSquare()
{
    return {
        point(-1.0, -1.0, 10),
        point(1.0, -1.0, 20),
        point(1.0, 1.0, 30),
        point(-1.0, 1.0, 40),
    };
}

ContainedRingResult analyticSquareRing()
{
    const auto triangle = [](const ContainedRingVertex first,
        const ContainedRingVertex second, const ContainedRingVertex third)
    {
        return ContainedRingTriangle{
            std::array<ContainedRingVertex, 3>{ first, second, third } };
    };
    return ContainedRingResult{
        {
            triangle({ ContainedRingContour::rectangle, 100 },
                { ContainedRingContour::rectangle, 110 },
                { ContainedRingContour::source, 20 }),
            triangle({ ContainedRingContour::rectangle, 100 },
                { ContainedRingContour::source, 20 },
                { ContainedRingContour::source, 10 }),
            triangle({ ContainedRingContour::rectangle, 110 },
                { ContainedRingContour::rectangle, 120 },
                { ContainedRingContour::source, 30 }),
            triangle({ ContainedRingContour::rectangle, 110 },
                { ContainedRingContour::source, 30 },
                { ContainedRingContour::source, 20 }),
            triangle({ ContainedRingContour::rectangle, 120 },
                { ContainedRingContour::rectangle, 130 },
                { ContainedRingContour::source, 40 }),
            triangle({ ContainedRingContour::rectangle, 120 },
                { ContainedRingContour::source, 40 },
                { ContainedRingContour::source, 30 }),
            triangle({ ContainedRingContour::rectangle, 130 },
                { ContainedRingContour::rectangle, 100 },
                { ContainedRingContour::source, 10 }),
            triangle({ ContainedRingContour::rectangle, 130 },
                { ContainedRingContour::source, 10 },
                { ContainedRingContour::source, 40 }),
        },
        20,
        110,
    };
}

long double cross(
    const TPoint2& first,
    const TPoint2& second,
    const TPoint2& third)
{
    return (static_cast<long double>(second.x) - first.x)
        * (static_cast<long double>(third.y) - first.y)
        - (static_cast<long double>(second.y) - first.y)
        * (static_cast<long double>(third.x) - first.x);
}

long double signedArea(const std::vector<ContainedRingPoint>& polygon)
{
    if ( polygon.size() < 3 )
    {
        return 0.0L;
    }
    // Translate the fan to the first vertex before taking products. This is
    // exact for the fixed dyadic fixtures and keeps the oracle meaningful at
    // the large translated origin used below.
    const auto& anchor = polygon.front().position;
    long double area{};
    for (std::size_t index = 1; index + 1 < polygon.size(); ++index)
    {
        area += cross(anchor, polygon[index].position,
            polygon[index + 1].position);
    }
    return area / 2.0L;
}

const ContainedRingPoint* findPoint(
    const std::vector<ContainedRingPoint>& points,
    const std::size_t identity)
{
    for (const auto& point : points)
    {
        if ( point.index == identity )
        {
            return &point;
        }
    }
    return nullptr;
}

const ContainedRingPoint* pointForVertex(
    const std::vector<ContainedRingPoint>& source,
    const std::vector<ContainedRingPoint>& rectangle,
    const ContainedRingVertex vertex)
{
    if ( vertex.contour == ContainedRingContour::source )
    {
        return findPoint(source, vertex.index);
    }
    if ( vertex.contour == ContainedRingContour::rectangle )
    {
        return findPoint(rectangle, vertex.index);
    }
    return nullptr;
}

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

    friend bool operator==(const EdgeKey& lhs, const EdgeKey& rhs) noexcept
    {
        return lhs.first == rhs.first && lhs.second == rhs.second;
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

bool samePoint(const TPoint2& first, const TPoint2& second) noexcept
{
    return first.x == second.x && first.y == second.y;
}

int sign(const long double value) noexcept
{
    return value < 0.0L ? -1 : value > 0.0L ? 1 : 0;
}

bool onSegment(
    const TPoint2& first,
    const TPoint2& second,
    const TPoint2& point)
{
    if ( cross(first, second, point) != 0.0L )
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
    const TPoint2& fourth)
{
    const int firstTurn = sign(cross(first, second, third));
    const int secondTurn = sign(cross(first, second, fourth));
    const int thirdTurn = sign(cross(third, fourth, first));
    const int fourthTurn = sign(cross(third, fourth, second));
    if ( firstTurn != secondTurn && thirdTurn != fourthTurn
        && firstTurn != 0 && secondTurn != 0
        && thirdTurn != 0 && fourthTurn != 0 )
    {
        return true;
    }
    return (firstTurn == 0 && onSegment(first, second, third))
        || (secondTurn == 0 && onSegment(first, second, fourth))
        || (thirdTurn == 0 && onSegment(third, fourth, first))
        || (fourthTurn == 0 && onSegment(third, fourth, second));
}

bool properIntersection(
    const TPoint2& first,
    const TPoint2& second,
    const TPoint2& third,
    const TPoint2& fourth)
{
    return sign(cross(first, second, third))
            * sign(cross(first, second, fourth)) < 0
        && sign(cross(third, fourth, first))
            * sign(cross(third, fourth, second)) < 0;
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

bool sameVertexSet(
    const std::array<ContainedRingVertex, 3>& first,
    const std::array<ContainedRingVertex, 3>& second) noexcept
{
    for (const auto firstVertex : first)
    {
        if ( std::find(second.begin(), second.end(), firstVertex) == second.end() )
        {
            return false;
        }
    }
    for (const auto secondVertex : second)
    {
        if ( std::find(first.begin(), first.end(), secondVertex) == first.end() )
        {
            return false;
        }
    }
    return true;
}

bool triangleContainsStrict(
    const TPoint2& first,
    const TPoint2& second,
    const TPoint2& third,
    const TPoint2& point)
{
    const int firstTurn = sign(cross(first, second, point));
    const int secondTurn = sign(cross(second, third, point));
    const int thirdTurn = sign(cross(third, first, point));
    return firstTurn != 0 && firstTurn == secondTurn && firstTurn == thirdTurn;
}

bool pointStrictlyInsideConvexContour(
    const std::vector<ContainedRingPoint>& contour,
    const TPoint2& point)
{
    const bool counterClockwise = signedArea(contour) > 0.0L;
    for (std::size_t index = 0; index < contour.size(); ++index)
    {
        const auto& first = contour[index].position;
        const auto& second = contour[(index + 1) % contour.size()].position;
        const long double turn = cross(first, second, point);
        if ( counterClockwise ? turn <= 0.0L : turn >= 0.0L )
        {
            return false;
        }
    }
    return true;
}

std::optional<int> boundaryDirection(
    const EdgeKey& edge,
    const ContainedRingContour contour,
    const std::vector<ContainedRingPoint>& points,
    const int direction)
{
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        const auto next = static_cast<std::size_t>(
            (static_cast<std::ptrdiff_t>(index) + direction
                + static_cast<std::ptrdiff_t>(points.size()))
            % static_cast<std::ptrdiff_t>(points.size()));
        const ContainedRingVertex first{ contour, points[index].index };
        const ContainedRingVertex second{ contour, points[next].index };
        if ( edgeKey(first, second).first == first )
        {
            if ( edge == edgeKey(first, second) )
            {
                return 0;
            }
        }
        else if ( edge == edgeKey(first, second) )
        {
            return 1;
        }
    }
    return std::nullopt;
}

bool triangleReferencesAreValid(
    const std::vector<ContainedRingPoint>& source,
    const std::vector<ContainedRingPoint>& rectangle,
    const ContainedRingTriangle& triangle)
{
    for (const auto vertex : triangle.vertices)
    {
        if ( vertex.contour != ContainedRingContour::source
            && vertex.contour != ContainedRingContour::rectangle )
        {
            return false;
        }
        if ( pointForVertex(source, rectangle, vertex) == nullptr )
        {
            return false;
        }
    }
    return triangle.vertices[0] != triangle.vertices[1]
        && triangle.vertices[0] != triangle.vertices[2]
        && triangle.vertices[1] != triangle.vertices[2];
}

bool triangleEdgesStayOutsideHole(
    const std::vector<ContainedRingPoint>& source,
    const std::vector<ContainedRingPoint>& rectangle,
    const ContainedRingTriangle& triangle)
{
    std::array<TPoint2, 3> trianglePoints{};
    for (std::size_t index = 0; index < 3; ++index)
    {
        const auto* point = pointForVertex(
            source, rectangle, triangle.vertices[index]);
        if ( point == nullptr )
        {
            return false;
        }
        trianglePoints[index] = point->position;
    }

    for (std::size_t edge = 0; edge < 3; ++edge)
    {
        const auto& first = trianglePoints[edge];
        const auto& second = trianglePoints[(edge + 1) % 3];
        for (std::size_t sourceEdge = 0; sourceEdge < source.size(); ++sourceEdge)
        {
            const auto& third = source[sourceEdge].position;
            const auto& fourth = source[(sourceEdge + 1) % source.size()].position;
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

    for (const auto& sourcePoint : source)
    {
        if ( triangleContainsStrict(
                trianglePoints[0], trianglePoints[1], trianglePoints[2],
                sourcePoint.position) )
        {
            return false;
        }
    }
    return true;
}

bool trianglesDoNotOverlap(
    const std::vector<ContainedRingPoint>& source,
    const std::vector<ContainedRingPoint>& rectangle,
    const ContainedRingTriangle& first,
    const ContainedRingTriangle& second)
{
    std::array<TPoint2, 3> firstPoints{};
    std::array<TPoint2, 3> secondPoints{};
    for (std::size_t index = 0; index < 3; ++index)
    {
        const auto* firstPoint = pointForVertex(
            source, rectangle, first.vertices[index]);
        const auto* secondPoint = pointForVertex(
            source, rectangle, second.vertices[index]);
        if ( firstPoint == nullptr || secondPoint == nullptr )
        {
            return false;
        }
        firstPoints[index] = firstPoint->position;
        secondPoints[index] = secondPoint->position;
    }
    if ( sameVertexSet(first.vertices, second.vertices) )
    {
        return false;
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
            if ( sameGeometricSegment(firstStart, firstEnd, secondStart, secondEnd) )
            {
                continue;
            }
            if ( !(samePoint(firstStart, secondStart)
                    || samePoint(firstStart, secondEnd)
                    || samePoint(firstEnd, secondStart)
                    || samePoint(firstEnd, secondEnd)) )
            {
                return false;
            }
        }
    }

    if ( triangleContainsStrict(
            firstPoints[0], firstPoints[1], firstPoints[2], secondPoints[0])
        || triangleContainsStrict(
            firstPoints[0], firstPoints[1], firstPoints[2], secondPoints[1])
        || triangleContainsStrict(
            firstPoints[0], firstPoints[1], firstPoints[2], secondPoints[2])
        || triangleContainsStrict(
            secondPoints[0], secondPoints[1], secondPoints[2], firstPoints[0])
        || triangleContainsStrict(
            secondPoints[0], secondPoints[1], secondPoints[2], firstPoints[1])
        || triangleContainsStrict(
            secondPoints[0], secondPoints[1], secondPoints[2], firstPoints[2]) )
    {
        return false;
    }
    return true;
}

bool ringResultIsValid(
    const std::vector<ContainedRingPoint>& source,
    const std::vector<ContainedRingPoint>& rectangle,
    const ContainedRingResult& result,
    const long double expectedArea,
    const bool exactArea = true)
{
    if ( result.triangles.size() < 3 || source.size() < 3 || rectangle.size() < 3 )
    {
        return false;
    }
    for (std::size_t first = 0; first < source.size(); ++first)
    {
        if ( !std::isfinite(source[first].position.x)
            || !std::isfinite(source[first].position.y) )
        {
            return false;
        }
        for (std::size_t second = first + 1; second < source.size(); ++second)
        {
            if ( source[first].index == source[second].index
                || samePoint(source[first].position, source[second].position) )
            {
                return false;
            }
        }
    }
    for (std::size_t first = 0; first < rectangle.size(); ++first)
    {
        if ( !std::isfinite(rectangle[first].position.x)
            || !std::isfinite(rectangle[first].position.y) )
        {
            return false;
        }
        for (std::size_t second = first + 1; second < rectangle.size(); ++second)
        {
            if ( rectangle[first].index == rectangle[second].index
                || samePoint(rectangle[first].position, rectangle[second].position) )
            {
                return false;
            }
        }
    }

    const auto outerArea = signedArea(rectangle);
    if ( outerArea == 0.0L )
    {
        return false;
    }
    const int outerWinding = outerArea > 0.0L ? 1 : -1;
    const int sourceWinding = signedArea(source) > 0.0L ? 1 : -1;
    const int sourceDirection = sourceWinding == outerWinding ? -1 : 1;
    std::map<EdgeKey, std::array<int, 2>> counts;
    long double area{};
    for (const auto& triangle : result.triangles)
    {
        if ( !triangleReferencesAreValid(source, rectangle, triangle) )
        {
            return false;
        }
        const auto* first = pointForVertex(source, rectangle, triangle.vertices[0]);
        const auto* second = pointForVertex(source, rectangle, triangle.vertices[1]);
        const auto* third = pointForVertex(source, rectangle, triangle.vertices[2]);
        const long double triangleCross = cross(
            first->position, second->position, third->position);
        if ( (outerArea > 0.0L && triangleCross <= 0.0L)
            || (outerArea < 0.0L && triangleCross >= 0.0L) )
        {
            return false;
        }
        if ( !triangleEdgesStayOutsideHole(source, rectangle, triangle) )
        {
            return false;
        }
        area += triangleCross / 2.0L;
        for (std::size_t edge = 0; edge < 3; ++edge)
        {
            const auto first = triangle.vertices[edge];
            const auto second = triangle.vertices[(edge + 1) % 3];
            const auto key = edgeKey(first, second);
            auto& directions = counts[key];
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
    const long double expectedSignedArea = outerArea > 0.0L
        ? expectedArea
        : -expectedArea;
    // The rounded fixture's decimal literals are intentionally non-dyadic.
    // Its fixed decimal area therefore includes input conversion error. The
    // bounded comparison covers the three cross-product terms and their
    // accumulation; dyadic fixtures continue to require exact equality.
    const long double areaError = exactArea
        ? 0.0L
        : 64.0L * std::numeric_limits<double>::epsilon()
            * std::max(1.0L, std::abs(expectedSignedArea));
    if ( exactArea
        ? area != expectedSignedArea
        : std::abs(area - expectedSignedArea) > areaError )
    {
        return false;
    }

    for (std::size_t index = 0; index < source.size(); ++index)
    {
        const EdgeKey edge{
            { ContainedRingContour::source, source[index].index },
            { ContainedRingContour::source,
                source[(index + 1) % source.size()].index } };
        const auto key = edgeKey(edge.first, edge.second);
        const auto found = counts.find(key);
        const auto expectedDirection = boundaryDirection(
            key, ContainedRingContour::source, source, sourceDirection);
        if ( found == counts.end() || !expectedDirection
            || found->second[*expectedDirection] != 1
            || found->second[1 - *expectedDirection] != 0 )
        {
            return false;
        }
    }
    for (std::size_t index = 0; index < rectangle.size(); ++index)
    {
        const EdgeKey edge{
            { ContainedRingContour::rectangle, rectangle[index].index },
            { ContainedRingContour::rectangle,
                rectangle[(index + 1) % rectangle.size()].index } };
        const auto key = edgeKey(edge.first, edge.second);
        const auto found = counts.find(key);
        const auto expectedDirection = boundaryDirection(
            key, ContainedRingContour::rectangle, rectangle, 1);
        if ( found == counts.end() || !expectedDirection
            || found->second[*expectedDirection] != 1
            || found->second[1 - *expectedDirection] != 0 )
        {
            return false;
        }
    }
    for (const auto& [edge, directions] : counts)
    {
        const auto sourceBoundary = boundaryDirection(
            edge, ContainedRingContour::source, source, sourceDirection);
        const auto rectangleBoundary = boundaryDirection(
            edge, ContainedRingContour::rectangle, rectangle, 1);
        const auto expectedDirection = sourceBoundary
            ? sourceBoundary : rectangleBoundary;
        if ( expectedDirection )
        {
            if ( directions[*expectedDirection] != 1
                || directions[1 - *expectedDirection] != 0 )
            {
                return false;
            }
        }
        else if ( directions[0] != 1 || directions[1] != 1 )
        {
            return false;
        }
    }

    for (std::size_t first = 0; first < result.triangles.size(); ++first)
    {
        for (std::size_t second = first + 1; second < result.triangles.size(); ++second)
        {
            if ( !trianglesDoNotOverlap(
                    source, rectangle, result.triangles[first], result.triangles[second]) )
            {
                return false;
            }
        }
    }
    return true;
}

bool bridgeIsVisible(
    const std::vector<ContainedRingPoint>& source,
    const std::vector<ContainedRingPoint>& rectangle,
    const ContainedRingResult& result)
{
    const auto* sourcePoint = findPoint(source, result.sourceBridge);
    const auto* rectanglePoint = findPoint(rectangle, result.rectangleBridge);
    if ( sourcePoint == nullptr || rectanglePoint == nullptr )
    {
        return false;
    }
    if ( !pointStrictlyInsideConvexContour(rectangle, sourcePoint->position) )
    {
        return false;
    }
    for (std::size_t edge = 0; edge < source.size(); ++edge)
    {
        const auto& first = source[edge].position;
        const auto& second = source[(edge + 1) % source.size()].position;
        if ( !segmentsIntersect(
                sourcePoint->position, rectanglePoint->position, first, second) )
        {
            continue;
        }
        if ( samePoint(sourcePoint->position, first)
            || samePoint(sourcePoint->position, second) )
        {
            continue;
        }
        return false;
    }
    for (std::size_t edge = 0; edge < rectangle.size(); ++edge)
    {
        const auto& first = rectangle[edge].position;
        const auto& second = rectangle[(edge + 1) % rectangle.size()].position;
        if ( !segmentsIntersect(
                sourcePoint->position, rectanglePoint->position, first, second) )
        {
            continue;
        }
        if ( samePoint(rectanglePoint->position, first)
            || samePoint(rectanglePoint->position, second) )
        {
            continue;
        }
        return false;
    }
    return true;
}

void validAnnulusCases()
{
    const auto source = sourceSquare();
    const auto outer = rectangle();
    ContainedRingResult result;
    const bool accepted = sed::level::detail::buildContainedExtrudeRing(
        source, outer, result);
    check(accepted, "Contained ring accepts a strictly contained square hole");
    if ( !accepted )
    {
        return;
    }

    check(result.triangles.size() == 8,
        "Contained square ring has the fixed eight-triangle triangulation");
    check(result.sourceBridge == 20,
        "Contained ring selects the fixed lowest rightmost source identity");
    check(result.rectangleBridge == 110,
        "Contained ring selects the fixed lowest rightmost rectangle identity");
    check(bridgeIsVisible(source, outer, result),
        "Contained ring bridge is visible without crossing the source hole");
    check(ringResultIsValid(source, outer, result, 20.0L),
        "Contained square ring has fixed area, winding, closure, and coverage");
}

void concaveSourceCase()
{
    const std::vector<ContainedRingPoint> source{
        point(-1.5, -1.0, 10),
        point(1.5, -1.0, 20),
        point(1.5, 1.0, 30),
        point(0.0, 0.0, 40),
        point(-1.5, 1.0, 50),
    };
    const auto outer = rectangle();
    ContainedRingResult result;
    const bool accepted = sed::level::detail::buildContainedExtrudeRing(
        source, outer, result);
    check(accepted, "Contained ring accepts a simple concave legacy source contour");
    if ( accepted )
    {
        check(result.triangles.size() == 9,
            "Contained concave ring has the fixed nine-triangle triangulation");
        check(result.sourceBridge == 20 && result.rectangleBridge == 110,
            "Contained concave ring keeps the fixed bridge identities");
        check(bridgeIsVisible(source, outer, result),
            "Contained concave ring bridge remains visible");
        check(ringResultIsValid(source, outer, result, 19.5L),
            "Contained concave ring preserves fixed area and all contour edges");
    }
}

void windingAndStableIdentityCases()
{
    for (const bool reverseSource : {false, true})
    {
        for (const bool reverseRectangle : {false, true})
        {
            auto source = sourceSquare();
            auto outer = rectangle();
            std::rotate(source.begin(), source.begin() + 2, source.end());
            std::rotate(outer.begin(), outer.begin() + 2, outer.end());
            if ( reverseSource )
            {
                std::reverse(source.begin(), source.end());
            }
            if ( reverseRectangle )
            {
                std::reverse(outer.begin(), outer.end());
            }
            ContainedRingResult result;
            check(sed::level::detail::buildContainedExtrudeRing(source, outer, result),
                "Contained ring accepts every independent contour winding combination");
            check(result.sourceBridge == 20 && result.rectangleBridge == 110,
                "Contained ring bridge selection is independent of input start and winding");
            check(ringResultIsValid(source, outer, result, 20.0L),
                "Contained ring follows the supplied outer winding with fixed area");
        }
    }
}

void scaleAndTranslationCases()
{
    const std::array<double, 2> scales{0.5, 2.0};
    const std::array<long double, 2> expectedAreas{5.0L, 80.0L};
    for (std::size_t caseIndex = 0; caseIndex < scales.size(); ++caseIndex)
    {
        const double scale = scales[caseIndex];
        auto source = sourceSquare();
        auto outer = rectangle();
        for (auto& point : source)
        {
            point.position.x = std::fma(point.position.x, scale, 1048576.0);
            point.position.y = std::fma(point.position.y, scale, -2097152.0);
        }
        for (auto& point : outer)
        {
            point.position.x = std::fma(point.position.x, scale, 1048576.0);
            point.position.y = std::fma(point.position.y, scale, -2097152.0);
        }
        ContainedRingResult result;
        check(sed::level::detail::buildContainedExtrudeRing(source, outer, result),
            "Contained ring accepts translated and dyadically scaled contours");
        check(result.sourceBridge == 20 && result.rectangleBridge == 110,
            "Contained ring keeps stable bridge identities after scaling and translation");
        check(ringResultIsValid(source, outer, result, expectedAreas[caseIndex]),
            "Contained ring preserves translated scaled area and topology");
    }
}

void extremeScaleCases()
{
    const std::array<double, 2> scales{
        std::ldexp(1.0, -600), std::ldexp(1.0, 600) };
    for (std::size_t caseIndex = 0; caseIndex < scales.size(); ++caseIndex)
    {
        auto source = sourceSquare();
        auto outer = rectangle();
        for (auto& point : source)
        {
            point.position.x *= scales[caseIndex];
            point.position.y *= scales[caseIndex];
        }
        for (auto& point : outer)
        {
            point.position.x *= scales[caseIndex];
            point.position.y *= scales[caseIndex];
        }
        ContainedRingResult result;
        check(sed::level::detail::buildContainedExtrudeRing(source, outer, result),
            "Contained ring accepts genuinely tiny and large dyadic contours");
        // The scaled coordinates deliberately underflow/overflow ordinary
        // area products on double-only hosts. Validate the returned topology
        // and stable identities against the fixed unscaled geometry instead.
        check(result.sourceBridge == 20 && result.rectangleBridge == 110,
            "Contained ring keeps stable bridge identities at extreme scales");
        check(ringResultIsValid(sourceSquare(), rectangle(), result, 20.0L),
            "Contained ring keeps exact topology at extreme scales");
    }
}

void forwardCollinearAndBacktrackingCases()
{
    const std::vector<ContainedRingPoint> subdividedSource{
        point(-1.0, -1.0, 10),
        point(0.0, -1.0, 15),
        point(1.0, -1.0, 20),
        point(1.0, 1.0, 30),
        point(-1.0, 1.0, 40),
    };
    ContainedRingResult result;
    check(sed::level::detail::buildContainedExtrudeRing(
            subdividedSource, rectangle(), result),
        "Contained ring preserves a forward collinear source subdivision");
    check(ringResultIsValid(subdividedSource, rectangle(), result, 20.0L),
        "Contained ring retains every forward collinear boundary edge");

    const std::vector<ContainedRingPoint> backtrackingSource{
        point(-1.0, -1.0, 10),
        point(1.0, -1.0, 20),
        point(0.0, -1.0, 30),
        point(1.0, 1.0, 40),
        point(-1.0, 1.0, 50),
    };
    check(!sed::level::detail::buildContainedExtrudeRing(
            backtrackingSource, rectangle(), result),
        "Contained ring rejects a collinear source backtrack");
}

void nonDyadicObliqueCase()
{
    const std::vector<ContainedRingPoint> source{
        point(-1.25, -1.10, 10),
        point(1.60, -1.05, 20),
        point(1.35, 0.90, 30),
        point(0.20, 0.35, 40),
        point(-1.40, 1.20, 50),
    };
    const std::vector<ContainedRingPoint> outer{
        point(-3.30, -2.10, 100),
        point(3.90, -1.80, 110),
        point(3.50, 3.30, 120),
        point(-3.70, 3.00, 130),
    };
    ContainedRingResult result;
    const bool accepted = sed::level::detail::buildContainedExtrudeRing(
        source, outer, result);
    check(accepted,
        "Contained ring accepts a non-dyadic concave contour in an oblique rectangle");
    if ( accepted )
    {
        check(result.triangles.size() == 9,
            "Contained oblique ring uses one triangle per contour edge pair");
        check(result.sourceBridge == 20 && result.rectangleBridge == 110,
            "Contained oblique ring keeps deterministic stable bridge identities");
        check(ringResultIsValid(source, outer, result, 31.84375L, false),
            "Contained oblique ring preserves its fixed area within input-rounding budget");
    }
}

void outputPreservationCase()
{
    const auto source = sourceSquare();
    const auto outer = rectangle();
    ContainedRingResult result;
    result.sourceBridge = 71;
    result.rectangleBridge = 73;
    result.triangles = { ContainedRingTriangle{
        std::array<ContainedRingVertex, 3>{
            ContainedRingVertex{ ContainedRingContour::rectangle, 910 },
            ContainedRingVertex{ ContainedRingContour::source, 920 },
            ContainedRingVertex{ ContainedRingContour::rectangle, 930 } } } };
    const ContainedRingResult before = result;
    const std::vector<ContainedRingPoint> outside{
        point(-1.0, -1.0, 10),
        point(4.0, -1.0, 20),
        point(1.0, 1.0, 30),
    };
    check(!sed::level::detail::buildContainedExtrudeRing(
            outside, outer, result),
        "Contained ring rejects a source contour outside the rectangle");
    check(result.triangles == before.triangles
            && result.sourceBridge == before.sourceBridge
            && result.rectangleBridge == before.rectangleBridge,
        "Contained ring failure preserves every output parameter");

    const std::vector<ContainedRingPoint> tooSmall{
        point(-1.0, -1.0, 10),
        point(1.0, -1.0, 20),
    };
    check(!sed::level::detail::buildContainedExtrudeRing(
            tooSmall, outer, result),
        "Contained ring rejects a source contour with fewer than three corners");
    check(result.triangles == before.triangles
            && result.sourceBridge == before.sourceBridge
            && result.rectangleBridge == before.rectangleBridge,
        "Contained ring malformed input preserves every output parameter");
}

void malformedContourCases()
{
    const auto source = sourceSquare();
    ContainedRingResult result;

    auto nonConvexOuter = rectangle();
    nonConvexOuter.insert(nonConvexOuter.begin() + 2, point(0.0, 0.0, 140));
    check(!sed::level::detail::buildContainedExtrudeRing(
            source, nonConvexOuter, result),
        "Contained ring rejects a non-convex outer contour");

    const std::vector<ContainedRingPoint> crossed{
        point(-1.5, -1.0, 10),
        point(1.5, 1.0, 20),
        point(-1.5, 1.0, 30),
        point(1.5, -1.0, 40),
    };
    check(!sed::level::detail::buildContainedExtrudeRing(
            crossed, rectangle(), result),
        "Contained ring rejects a self-crossing source contour");

    auto duplicate = source;
    duplicate[2].position = duplicate[1].position;
    check(!sed::level::detail::buildContainedExtrudeRing(
            duplicate, rectangle(), result),
        "Contained ring rejects an exact duplicate source corner");

    auto duplicateIdentity = source;
    duplicateIdentity[2].index = duplicateIdentity[1].index;
    check(!sed::level::detail::buildContainedExtrudeRing(
            duplicateIdentity, rectangle(), result),
        "Contained ring rejects duplicate stable contour identities");

    auto nonFinite = source;
    nonFinite[1].position.x = std::numeric_limits<double>::quiet_NaN();
    check(!sed::level::detail::buildContainedExtrudeRing(
            nonFinite, rectangle(), result),
        "Contained ring rejects a non-finite source corner");

    auto boundary = source;
    boundary[2].position = TPoint2{ 3.0, 1.0 };
    check(!sed::level::detail::buildContainedExtrudeRing(
            boundary, rectangle(), result),
        "Contained ring rejects a source corner on the outer boundary");
}

void resultOracleNegativeCases()
{
    const auto source = sourceSquare();
    const auto outer = rectangle();
    const ContainedRingResult valid = analyticSquareRing();
    check(ringResultIsValid(source, outer, valid, 20.0L),
        "Independent ring oracle accepts the fixed analytic annulus vector");

    auto missing = valid;
    missing.triangles.pop_back();
    check(!ringResultIsValid(source, outer, missing, 20.0L),
        "Independent ring oracle rejects a missing triangle");

    auto duplicate = valid;
    duplicate.triangles.push_back(duplicate.triangles.front());
    check(!ringResultIsValid(source, outer, duplicate, 20.0L),
        "Independent ring oracle rejects a duplicated triangle");

    auto inverted = valid;
    std::swap(inverted.triangles.front().vertices[1],
        inverted.triangles.front().vertices[2]);
    check(!ringResultIsValid(source, outer, inverted, 20.0L),
        "Independent ring oracle rejects an inverted triangle");

    auto holeCovering = valid;
    holeCovering.triangles.front().vertices = {
        ContainedRingVertex{ ContainedRingContour::rectangle, 100 },
        ContainedRingVertex{ ContainedRingContour::rectangle, 110 },
        ContainedRingVertex{ ContainedRingContour::rectangle, 120 } };
    check(!ringResultIsValid(source, outer, holeCovering, 20.0L),
        "Independent ring oracle rejects a triangle covering the source hole");

    auto crossingDiagonal = valid;
    crossingDiagonal.triangles.front().vertices = {
        ContainedRingVertex{ ContainedRingContour::rectangle, 100 },
        ContainedRingVertex{ ContainedRingContour::rectangle, 110 },
        ContainedRingVertex{ ContainedRingContour::source, 30 } };
    check(properIntersection(
            TPoint2{ -3.0, -2.0 }, TPoint2{ 1.0, 1.0 },
            TPoint2{ -1.0, -1.0 }, TPoint2{ -1.0, 1.0 }),
        "Fixed crossing control contains a proper source-hole intersection");
    check(!ringResultIsValid(source, outer, crossingDiagonal, 20.0L),
        "Independent ring oracle rejects a crossing diagonal");

    auto invalidReference = valid;
    invalidReference.triangles.front().vertices[0] =
        ContainedRingVertex{ ContainedRingContour::source, 9999 };
    check(!ringResultIsValid(source, outer, invalidReference, 20.0L),
        "Independent ring oracle rejects an invalid contour reference");

    auto invalidContour = valid;
    invalidContour.triangles.front().vertices[0].contour =
        static_cast<ContainedRingContour>(0xffu);
    check(!ringResultIsValid(source, outer, invalidContour, 20.0L),
        "Independent ring oracle rejects an invalid contour kind");
}

} // namespace

void runExtrudeContainedRingTests()
{
    validAnnulusCases();
    concaveSourceCase();
    windingAndStableIdentityCases();
    scaleAndTranslationCases();
    extremeScaleCases();
    forwardCollinearAndBacktrackingCases();
    nonDyadicObliqueCase();
    outputPreservationCase();
    malformedContourCases();
    resultOracleNegativeCases();
}

} // namespace sed::tests
