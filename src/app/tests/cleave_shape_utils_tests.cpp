#include "test_common.hpp"

#include "ui/viewport/cleave_shape_utils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace sed::app_tests
{
namespace
{

using ui::viewport::CleaveShapeCell2D;
using ui::viewport::CleaveShapeKind;
using ui::viewport::CleaveShapePlan;
using ui::viewport::CleaveShapePoint2D;
using ui::viewport::CleaveShapeSpec;

double signedArea(const std::vector<CleaveShapePoint2D>& points)
{
    if ( points.size() < 3 )
    {
        return 0.0;
    }

    double twiceArea = 0.0;
    for ( std::size_t index = 0; index < points.size(); ++index )
    {
        const CleaveShapePoint2D& current = points[index];
        const CleaveShapePoint2D& next = points[(index + 1) % points.size()];
        twiceArea += current.x * next.y - current.y * next.x;
    }
    return 0.5 * twiceArea;
}

bool finitePoint(const CleaveShapePoint2D& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y);
}

bool isConvexCell(const CleaveShapeCell2D& cell)
{
    if ( cell.vertices.size() < 3 || signedArea(cell.vertices) <= 0.0 )
    {
        return false;
    }

    for ( std::size_t index = 0; index < cell.vertices.size(); ++index )
    {
        const CleaveShapePoint2D& a = cell.vertices[index];
        const CleaveShapePoint2D& b = cell.vertices[(index + 1) % cell.vertices.size()];
        const CleaveShapePoint2D& c = cell.vertices[(index + 2) % cell.vertices.size()];
        const double turn = (b.x - a.x) * (c.y - b.y)
            - (b.y - a.y) * (c.x - b.x);
        if ( !std::isfinite(turn) || turn < -1.0e-10 )
        {
            return false;
        }
    }
    return true;
}

bool pointInsideOrOnPolygon(
    const CleaveShapePoint2D& point,
    const std::vector<CleaveShapePoint2D>& polygon)
{
    bool inside = false;
    for ( std::size_t index = 0; index < polygon.size(); ++index )
    {
        const CleaveShapePoint2D& first = polygon[index];
        const CleaveShapePoint2D& second = polygon[(index + 1) % polygon.size()];
        const double edgeX = second.x - first.x;
        const double edgeY = second.y - first.y;
        const double pointX = point.x - first.x;
        const double pointY = point.y - first.y;
        const double cross = edgeX * pointY - edgeY * pointX;
        const double edgeLengthSquared = edgeX * edgeX + edgeY * edgeY;
        const double dot = pointX * edgeX + pointY * edgeY;
        if ( std::abs(cross) <= 1.0e-10 * std::max(1.0, edgeLengthSquared)
            && dot >= -1.0e-10
            && dot <= edgeLengthSquared + 1.0e-10 )
        {
            return true;
        }

        const bool straddles = (first.y > point.y) != (second.y > point.y);
        if ( straddles )
        {
            const double crossingX = first.x
                + (point.y - first.y) * edgeX / edgeY;
            if ( point.x < crossingX )
            {
                inside = !inside;
            }
        }
    }
    return inside;
}

double orientation(
    const CleaveShapePoint2D& first,
    const CleaveShapePoint2D& second,
    const CleaveShapePoint2D& third)
{
    return (second.x - first.x) * (third.y - first.y)
        - (second.y - first.y) * (third.x - first.x);
}

bool properSegmentsCross(
    const CleaveShapePoint2D& firstA,
    const CleaveShapePoint2D& secondA,
    const CleaveShapePoint2D& firstB,
    const CleaveShapePoint2D& secondB)
{
    const double firstSide = orientation(firstA, secondA, firstB);
    const double secondSide = orientation(firstA, secondA, secondB);
    const double thirdSide = orientation(firstB, secondB, firstA);
    const double fourthSide = orientation(firstB, secondB, secondA);
    return ((firstSide > 1.0e-10 && secondSide < -1.0e-10)
            || (firstSide < -1.0e-10 && secondSide > 1.0e-10))
        && ((thirdSide > 1.0e-10 && fourthSide < -1.0e-10)
            || (thirdSide < -1.0e-10 && fourthSide > 1.0e-10));
}

bool samePlan(const CleaveShapePlan& lhs, const CleaveShapePlan& rhs)
{
    if ( lhs.convexClosedPerimeter != rhs.convexClosedPerimeter
        || lhs.perimeter.size() != rhs.perimeter.size()
        || lhs.decompositionStrokes.size() != rhs.decompositionStrokes.size()
        || lhs.convexCells.size() != rhs.convexCells.size() )
    {
        return false;
    }

    const auto samePoint = [](const CleaveShapePoint2D& first,
                              const CleaveShapePoint2D& second)
    {
        return first.x == second.x && first.y == second.y;
    };
    for ( std::size_t index = 0; index < lhs.perimeter.size(); ++index )
    {
        if ( !samePoint(lhs.perimeter[index], rhs.perimeter[index]) )
        {
            return false;
        }
    }
    for ( std::size_t index = 0; index < lhs.decompositionStrokes.size(); ++index )
    {
        if ( !samePoint(lhs.decompositionStrokes[index].first,
                       rhs.decompositionStrokes[index].first)
            || !samePoint(lhs.decompositionStrokes[index].second,
                          rhs.decompositionStrokes[index].second) )
        {
            return false;
        }
    }
    for ( std::size_t cellIndex = 0; cellIndex < lhs.convexCells.size(); ++cellIndex )
    {
        const auto& left = lhs.convexCells[cellIndex].vertices;
        const auto& right = rhs.convexCells[cellIndex].vertices;
        if ( left.size() != right.size() )
        {
            return false;
        }
        for ( std::size_t pointIndex = 0; pointIndex < left.size(); ++pointIndex )
        {
            if ( !samePoint(left[pointIndex], right[pointIndex]) )
            {
                return false;
            }
        }
    }
    return true;
}

void checkConvexPartition(const CleaveShapePlan& plan, const char* message)
{
    bool valid = !plan.perimeter.empty() && !plan.convexCells.empty();
    double cellArea = 0.0;
    for ( const CleaveShapePoint2D& point : plan.perimeter )
    {
        valid = valid && finitePoint(point);
    }
    for ( const CleaveShapeCell2D& cell : plan.convexCells )
    {
        valid = valid && isConvexCell(cell);
        cellArea += signedArea(cell.vertices);
    }
    valid = valid && signedArea(plan.perimeter) > 0.0
        && std::abs(cellArea - signedArea(plan.perimeter))
            <= 1.0e-9 * std::max(1.0, signedArea(plan.perimeter));
    check(valid, message);
}

} // namespace

void runCleaveShapeUtilsTests()
{
    const auto square = ui::viewport::makeCleaveShapePlan(CleaveShapeSpec{
        CleaveShapeKind::Square, 2.0, 1.0, false, 16, 5 });
    check(square.has_value(),
        "square Cleave shape produces a centered local plan");
    if ( square )
    {
        check(square->convexClosedPerimeter
                && square->perimeter.size() == 4
                && square->decompositionStrokes.empty()
                && square->convexCells.size() == 1,
            "square uses one convex perimeter and one extrusion cell");
        checkConvexPartition(*square,
            "square plan has a positive CCW perimeter and valid convex cell");
        double minX = std::numeric_limits<double>::infinity();
        double maxX = -std::numeric_limits<double>::infinity();
        double minY = std::numeric_limits<double>::infinity();
        double maxY = -std::numeric_limits<double>::infinity();
        for ( const CleaveShapePoint2D& point : square->perimeter )
        {
            minX = std::min(minX, point.x);
            maxX = std::max(maxX, point.x);
            minY = std::min(minY, point.y);
            maxY = std::max(maxY, point.y);
        }
        check(minX == -1.0 && maxX == 1.0 && minY == -1.0 && maxY == 1.0,
            "square remains centered and uses equal linked dimensions");
    }

    const CleaveShapeSpec starSpec{
        CleaveShapeKind::Star, 4.0, 2.0, false, 16, 5 };
    const auto star = ui::viewport::makeCleaveShapePlan(starSpec);
    check(star.has_value(),
        "star Cleave shape produces a decomposed local plan");
    if ( star )
    {
        check(!star->convexClosedPerimeter
                && star->perimeter.size() == 10
                && star->decompositionStrokes.size() == 5
                && star->convexCells.size() == 6,
            "five-point star retains its perimeter and adds a center polygon plus five tips");
        checkConvexPartition(*star,
            "star decomposition partitions its full outline into convex extrusion cells");
        const bool hasCentralPentagon = std::any_of(
            star->convexCells.begin(), star->convexCells.end(),
            [](const CleaveShapeCell2D& cell)
            {
                return cell.vertices.size() == 5;
            });
        const std::size_t tipTriangleCount = static_cast<std::size_t>(std::count_if(
            star->convexCells.begin(), star->convexCells.end(),
            [](const CleaveShapeCell2D& cell)
            {
                return cell.vertices.size() == 3;
            }));
        check(hasCentralPentagon && tipTriangleCount == 5,
            "star extrusion cells include its central polygon and all point triangles");
        const bool cellsRemainInsideOutline = std::all_of(
            star->convexCells.begin(), star->convexCells.end(),
            [&star](const CleaveShapeCell2D& cell)
            {
                return std::all_of(cell.vertices.begin(), cell.vertices.end(),
                    [&star](const CleaveShapePoint2D& vertex)
                    {
                        return pointInsideOrOnPolygon(vertex, star->perimeter);
                    });
            });
        const bool decompositionRemainsInsideOutline = std::all_of(
            star->decompositionStrokes.begin(), star->decompositionStrokes.end(),
            [&star](const ui::viewport::CleaveShapeStroke2D& stroke)
            {
                for ( const double fraction : { 0.25, 0.5, 0.75 } )
                {
                    const CleaveShapePoint2D sample{
                        stroke.first.x + (stroke.second.x - stroke.first.x) * fraction,
                        stroke.first.y + (stroke.second.y - stroke.first.y) * fraction,
                    };
                    if ( !pointInsideOrOnPolygon(sample, star->perimeter) )
                    {
                        return false;
                    }
                }
                return true;
            });
        bool chordsDoNotCross = true;
        for ( std::size_t firstIndex = 0;
            firstIndex < star->decompositionStrokes.size(); ++firstIndex )
        {
            const auto& first = star->decompositionStrokes[firstIndex];
            for ( std::size_t secondIndex = firstIndex + 1;
                secondIndex < star->decompositionStrokes.size(); ++secondIndex )
            {
                if ( secondIndex == firstIndex + 1
                    || (firstIndex == 0
                        && secondIndex + 1 == star->decompositionStrokes.size()) )
                {
                    continue;
                }
                const auto& second = star->decompositionStrokes[secondIndex];
                chordsDoNotCross = chordsDoNotCross
                    && !properSegmentsCross(
                        first.first, first.second, second.first, second.second);
            }
        }
        check(cellsRemainInsideOutline && decompositionRemainsInsideOutline
                && chordsDoNotCross,
            "star convex cells and ordered decomposition chords stay inside the silhouette without crossing");
        const bool allStrokesHaveLength = std::all_of(
            star->decompositionStrokes.begin(), star->decompositionStrokes.end(),
            [](const ui::viewport::CleaveShapeStroke2D& stroke)
            {
                const double deltaX = stroke.second.x - stroke.first.x;
                const double deltaY = stroke.second.y - stroke.first.y;
                return std::isfinite(deltaX * deltaX + deltaY * deltaY)
                    && deltaX * deltaX + deltaY * deltaY > 0.0;
            });
        check(allStrokesHaveLength,
            "star decomposition contains no zero-length cuts");
        const auto repeatedStar = ui::viewport::makeCleaveShapePlan(starSpec);
        check(repeatedStar && samePlan(*star, *repeatedStar),
            "star outline and decomposition ordering are deterministic");
    }

    const auto triangleStar = ui::viewport::makeCleaveShapePlan(CleaveShapeSpec{
        CleaveShapeKind::Star, 2.0, 2.0, true, 16, 3 });
    check(triangleStar && triangleStar->perimeter.size() == 6
            && triangleStar->decompositionStrokes.size() == 3
            && triangleStar->convexCells.size() == 4,
        "three-point star produces one center cell and three tip cells");
    if ( triangleStar )
    {
        checkConvexPartition(*triangleStar,
            "three-point star decomposition keeps every extrusion cell convex");
        bool hasStrictReflexValleys = true;
        for ( std::size_t index = 1; index < triangleStar->perimeter.size(); index += 2 )
        {
            const CleaveShapePoint2D& previous = triangleStar->perimeter[
                (index + triangleStar->perimeter.size() - 1)
                    % triangleStar->perimeter.size()];
            const CleaveShapePoint2D& current = triangleStar->perimeter[index];
            const CleaveShapePoint2D& next = triangleStar->perimeter[
                (index + 1) % triangleStar->perimeter.size()];
            hasStrictReflexValleys = hasStrictReflexValleys
                && orientation(previous, current, next) < 0.0;
        }
        check(hasStrictReflexValleys,
            "three-point star valleys remain strictly reflex rather than collapsing to a triangle");
    }

    const auto linkedRectangle = ui::viewport::makeCleaveShapePlan(CleaveShapeSpec{
        CleaveShapeKind::Rectangle, 4.0, 2.0, true, 16, 5 });
    const auto rectangle = ui::viewport::makeCleaveShapePlan(CleaveShapeSpec{
        CleaveShapeKind::Rectangle, 4.0, 2.0, false, 16, 5 });
    check(linkedRectangle && rectangle,
        "rectangle plans accept linked and independent positive dimensions");
    if ( linkedRectangle && rectangle )
    {
        const auto bounds = [](const CleaveShapePlan& plan)
        {
            double minX = std::numeric_limits<double>::infinity();
            double maxX = -std::numeric_limits<double>::infinity();
            double minY = std::numeric_limits<double>::infinity();
            double maxY = -std::numeric_limits<double>::infinity();
            for ( const CleaveShapePoint2D& point : plan.perimeter )
            {
                minX = std::min(minX, point.x);
                maxX = std::max(maxX, point.x);
                minY = std::min(minY, point.y);
                maxY = std::max(maxY, point.y);
            }
            return std::array<double, 4>{ minX, maxX, minY, maxY };
        };
        const auto linkedBounds = bounds(*linkedRectangle);
        const auto freeBounds = bounds(*rectangle);
        check(linkedBounds[0] == -2.0 && linkedBounds[1] == 2.0
                && linkedBounds[2] == -2.0 && linkedBounds[3] == 2.0,
            "linked rectangle dimensions follow the entered width");
        check(freeBounds[0] == -2.0 && freeBounds[1] == 2.0
                && freeBounds[2] == -1.0 && freeBounds[3] == 1.0,
            "unlinked rectangle preserves separate width and height");
    }

    const auto polygon = ui::viewport::makeCleaveShapePlan(CleaveShapeSpec{
        CleaveShapeKind::RegularPolygon, 2.0, 2.0, true, 7, 5 });
    check(polygon && polygon->perimeter.size() == 7
            && polygon->convexClosedPerimeter
            && polygon->convexCells.size() == 1,
        "regular N-gon supports seven sides as one convex cell");
    if ( polygon )
    {
        checkConvexPartition(*polygon,
            "regular N-gon plan remains suitable for convex-sector extrusion");
    }

    const auto arch = ui::viewport::makeCleaveShapePlan(CleaveShapeSpec{
        CleaveShapeKind::Arch, 4.0, 3.0, false, 12, 5 });
    check(arch && arch->convexClosedPerimeter
            && arch->perimeter.size() == 15
            && arch->decompositionStrokes.empty()
            && arch->convexCells.size() == 1,
        "filled segmented arch has one convex perimeter and no extra cuts");
    if ( arch )
    {
        checkConvexPartition(*arch,
            "filled arch remains a valid convex extrusion cell");
    }

    const auto slenderRectangle = ui::viewport::makeCleaveShapePlan(CleaveShapeSpec{
        CleaveShapeKind::Rectangle, 0.005, 10000.0, false, 16, 5 });
    check(slenderRectangle && slenderRectangle->perimeter.size() == 4,
        "shape dimensions accept their supported lower and upper boundaries");
    if ( slenderRectangle )
    {
        checkConvexPartition(*slenderRectangle,
            "a slender rectangle still has a finite positive convex cell");
        check(std::abs(signedArea(slenderRectangle->perimeter) - 50.0) < 1.0e-9,
            "slender dimensions preserve the requested area");
    }

    const auto maximumPolygon = ui::viewport::makeCleaveShapePlan(CleaveShapeSpec{
        CleaveShapeKind::RegularPolygon, 10000.0, 10000.0, true, 64, 5 });
    const auto maximumStar = ui::viewport::makeCleaveShapePlan(CleaveShapeSpec{
        CleaveShapeKind::Star, 10000.0, 10000.0, true, 16, 64 });
    check(maximumPolygon && maximumPolygon->perimeter.size() == 64
            && maximumStar && maximumStar->perimeter.size() == 128
            && maximumStar->decompositionStrokes.size() == 64,
        "maximum supported polygon and star counts produce bounded plans");
    if ( maximumPolygon && maximumStar )
    {
        checkConvexPartition(*maximumPolygon,
            "maximum regular polygon count remains convex");
        checkConvexPartition(*maximumStar,
            "maximum star point count retains a finite convex partition");
    }

    const std::vector<CleaveShapeSpec> invalidSpecs{
        { CleaveShapeKind::Square, 0.0, 1.0, true, 16, 5 },
        { CleaveShapeKind::Rectangle, 1.0,
            std::numeric_limits<double>::infinity(), false, 16, 5 },
        { CleaveShapeKind::RegularPolygon, 2.0, 2.0, true, 2, 5 },
        { CleaveShapeKind::Arch, 2.0, 2.0, false, 2, 5 },
        { CleaveShapeKind::Star, 2.0, 2.0, false, 16, 2 },
        { CleaveShapeKind::Star, 2.0, 2.0, false, 16, 65 },
        { CleaveShapeKind::Rectangle, 0.004, 1.0, false, 16, 5 },
        { CleaveShapeKind::Rectangle, 1.0, 10000.1, false, 16, 5 },
        { static_cast<CleaveShapeKind>(255), 1.0, 1.0, true, 16, 5 },
    };
    const bool invalidRejected = std::all_of(
        invalidSpecs.begin(), invalidSpecs.end(),
        [](const CleaveShapeSpec& spec)
        {
            return !ui::viewport::makeCleaveShapePlan(spec).has_value();
        });
    check(invalidRejected,
        "shape plans reject zero/non-finite sizes and out-of-range counts");
}

} // namespace sed::app_tests
