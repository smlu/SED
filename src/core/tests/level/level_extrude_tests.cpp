#include "test_common.hpp"

#include "sed/assets/sed.hpp"
#include "sed/level/build.hpp"
#include "sed/level/cleave.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level.hpp"
#include "sed/editor/document.hpp"
#include "../../src/level/geometry_precision.hpp"

#include <cstddef>
#include <iostream>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace sed::tests
{

namespace
{

bool sameTextureVertex(
    const sed::TexVertex& lhs,
    const sed::TexVertex& rhs) noexcept
{
    return lhs.u == rhs.u
        && lhs.v == rhs.v
        && lhs.color == rhs.color;
}

bool sameTextureVertices(
    const std::vector<sed::TexVertex>& lhs,
    const std::vector<sed::TexVertex>& rhs) noexcept
{
    if ( lhs.size() != rhs.size() )
    {
        return false;
    }

    for ( std::size_t index = 0; index < lhs.size(); ++index )
    {
        if ( !sameTextureVertex(lhs[index], rhs[index]) )
        {
            return false;
        }
    }
    return true;
}

struct ForcePartitionProbe
{
    int calls{};
    std::size_t firstPortalCount{};
    std::size_t secondPortalCount{};
};

bool rejectFastPathThenAcceptPartition(sed::Level& level, const sed::SurfaceRef original,
    const sed::SurfaceRef generated, void* const opaque)
{
    auto* const probe = static_cast<ForcePartitionProbe*>(opaque);
    if ( !probe || original.sector < 0 || generated.sector < 0
        || static_cast<std::size_t>(original.sector) >= level.sectors.size()
        || static_cast<std::size_t>(generated.sector) >= level.sectors.size() )
    {
        return false;
    }
    ++probe->calls;
    const auto& source = level.sectors[static_cast<std::size_t>(original.sector)];
    std::size_t portalCount = 0;
    for ( const auto& face : source.surfaces )
    {
        if ( face.adjoin && face.adjoin->sector == generated.sector )
        {
            ++portalCount;
        }
    }
    if ( probe->calls == 1 )
    {
        probe->firstPortalCount = portalCount;
        return false;
    }
    if ( probe->calls == 2 )
    {
        probe->secondPortalCount = portalCount;
        return portalCount >= 2;
    }
    return false;
}

// A portal boundary can acquire an additional vertex where a cleave plane
// intersects an already quantized edge. Each extra point is valid only when
// it is a forward subdivision of the other boundary's current edge. Keep this
// correspondence check independent from the production portal matcher so the
// test still rejects gaps, backtracking, duplicate coordinates, and same-side
// winding.
bool opposedLoopsMatchAllowingSubdivisions(
    std::vector<sed::TVector> first,
    std::vector<sed::TVector> second)
{
    const auto sameCoordinate = [](const double lhs, const double rhs)
    {
        return std::isfinite(lhs) && std::isfinite(rhs)
            && std::abs(lhs - rhs) <= 16 * std::numeric_limits<double>::epsilon()
                * std::max(std::abs(lhs), std::abs(rhs));
    };
    const auto validBoundary = [](const std::vector<sed::TVector>& points)
    {
        if ( points.size() < 3 )
        {
            return false;
        }
        for ( std::size_t index = 0; index < points.size(); ++index )
        {
            const auto& point = points[index];
            if ( !std::isfinite(point.x) || !std::isfinite(point.y)
                || !std::isfinite(point.z) )
            {
                return false;
            }
            for ( std::size_t other = index + 1; other < points.size(); ++other )
            {
                const auto& duplicate = points[other];
                if ( point.x == duplicate.x && point.y == duplicate.y
                    && point.z == duplicate.z )
                {
                    return false;
                }
            }
        }
        // Reject an exact out-and-back boundary while retaining every
        // representably distinct shallow bend. Approximate collinearity is
        // handled only when proving that a point is a forward subdivision of
        // the other loop's current edge.
        for ( std::size_t corner = 0; corner < points.size(); ++corner )
        {
            const auto& start = points[(corner + points.size() - 1) % points.size()];
            const auto& middle = points[corner];
            const auto& end = points[(corner + 1) % points.size()];
            const std::array<long double, 3> startPoint{{start.x, start.y, start.z}};
            const std::array<long double, 3> middlePoint{{middle.x, middle.y, middle.z}};
            const std::array<long double, 3> endPoint{{end.x, end.y, end.z}};
            std::array<long double, 3> delta{};
            std::size_t dominant = 0;
            for ( std::size_t axis = 0; axis < 3; ++axis )
            {
                delta[axis] = endPoint[axis] - startPoint[axis];
                if ( std::abs(delta[axis]) > std::abs(delta[dominant]) )
                {
                    dominant = axis;
                }
            }
            if ( delta[dominant] == 0.0L )
            {
                return false;
            }
            const long double amount = (middlePoint[dominant] - startPoint[dominant])
                / delta[dominant];
            bool collinear = true;
            for ( std::size_t axis = 0; axis < 3; ++axis )
            {
                collinear = collinear
                    && (middlePoint[axis] - startPoint[axis])
                        - amount * delta[axis] == 0.0L;
            }
            if ( collinear && (amount <= 0.0L || amount >= 1.0L) )
            {
                return false;
            }
        }
        return true;
    };
    const auto pointOnSegment = [](const sed::TVector& point,
        const sed::TVector& start, const sed::TVector& end)
    {
        const std::array<long double, 3> a{{start.x, start.y, start.z}};
        const std::array<long double, 3> b{{end.x, end.y, end.z}};
        const std::array<long double, 3> p{{point.x, point.y, point.z}};
        std::array<long double, 3> delta{};
        std::size_t dominant = 0;
        for ( std::size_t axis = 0; axis < 3; ++axis )
        {
            delta[axis] = b[axis] - a[axis];
            if ( std::abs(delta[axis]) > std::abs(delta[dominant]) )
            {
                dominant = axis;
            }
        }
        if ( delta[dominant] == 0.0L )
        {
            return false;
        }
        const long double amount = (p[dominant] - a[dominant]) / delta[dominant];
        if ( !(amount > 0.0L && amount < 1.0L) )
        {
            return false;
        }
        for ( std::size_t axis = 0; axis < 3; ++axis )
        {
            const long double residual = (p[axis] - a[axis]) - amount * delta[axis];
            const long double scale = std::max({std::abs(a[axis]), std::abs(b[axis]),
                std::abs(p[axis])});
            const long double allowance = 16 * std::numeric_limits<double>::epsilon() * scale;
            if ( std::abs(residual) > allowance )
            {
                return false;
            }
        }
        return true;
    };
    if ( !validBoundary(first) || !validBoundary(second) )
    {
        return false;
    }

    // Walk both loops in opposite directions and skip only a corner proved to
    // be a forward subdivision of the other loop's current edge. Shared
    // authored corners are compared before any subdivision is skipped, so a
    // shallow but representable bend cannot disappear because each traversal
    // simplified its own copy in a different order.
    const auto matchAt = [&](const std::size_t secondStart)
    {
        if ( !sameCoordinate(first.front().x, second[secondStart].x)
            || !sameCoordinate(first.front().y, second[secondStart].y)
            || !sameCoordinate(first.front().z, second[secondStart].z) )
        {
            return false;
        }
        std::map<std::pair<std::size_t, std::size_t>, std::size_t> explored;
        const auto walk = [&](auto&& self, const std::size_t consumedFirst,
            const std::size_t consumedSecond, const std::size_t matchedCorners) -> bool
        {
            if ( consumedFirst == first.size() && consumedSecond == second.size() )
            {
                return matchedCorners >= 3;
            }
            const auto state = std::pair{consumedFirst, consumedSecond};
            const auto previous = explored.find(state);
            if ( previous != explored.end() && previous->second >= matchedCorners )
            {
                return false;
            }
            explored[state] = matchedCorners;

            const bool firstRemaining = consumedFirst < first.size();
            const bool secondRemaining = consumedSecond < second.size();
            const auto firstIndex = consumedFirst % first.size();
            const auto secondIndex = (secondStart + second.size()
                - (consumedSecond % second.size())) % second.size();
            const auto previousFirstIndex =
                (consumedFirst + first.size() - 1) % first.size();
            const auto previousSecondIndex = (secondStart + second.size()
                - ((consumedSecond + second.size() - 1) % second.size())) % second.size();
            if ( firstRemaining && secondRemaining )
            {
                const auto& firstPoint = first[firstIndex];
                const auto& secondPoint = second[secondIndex];
                if ( sameCoordinate(firstPoint.x, secondPoint.x)
                    && sameCoordinate(firstPoint.y, secondPoint.y)
                    && sameCoordinate(firstPoint.z, secondPoint.z)
                    && self(self, consumedFirst + 1, consumedSecond + 1, matchedCorners + 1) )
                {
                    return true;
                }
            }
            const auto& previousFirst = first[previousFirstIndex];
            const auto& previousSecond = second[previousSecondIndex];
            const auto& firstPoint = first[firstIndex];
            const auto& secondPoint = second[secondIndex];
            const bool secondIsSubdivision = secondRemaining
                && pointOnSegment(secondPoint, previousFirst,
                    firstRemaining ? firstPoint : first.front());
            const bool firstIsSubdivision = firstRemaining
                && pointOnSegment(firstPoint, previousSecond,
                    secondRemaining ? secondPoint : second[secondStart]);
            if ( secondIsSubdivision && self(self, consumedFirst, consumedSecond + 1,
                    matchedCorners) )
            {
                return true;
            }
            if ( firstIsSubdivision && self(self, consumedFirst + 1, consumedSecond,
                    matchedCorners) )
            {
                return true;
            }
            return false;
        };
        return walk(walk, 1, 1, 1);
    };
    for ( std::size_t start = 0; start < second.size(); ++start )
    {
        if ( matchAt(start) )
        {
            return true;
        }
    }
    return false;
}

} // namespace

void runLevelExtrudeTests()
{
    {
        const std::vector<sed::TVector> square{{0, 0, 0}, {1, 0, 0},
            {1, 1, 0}, {0, 1, 0}};
        const std::vector<sed::TVector> subdivided{{0, 0, 0}, {0, 1, 0},
            {1, 1, 0}, {1, 0.5, 0}, {1, 0, 0}};
        auto cyclic = subdivided;
        std::rotate(cyclic.begin(), cyclic.begin() + 2, cyclic.end());
        check(opposedLoopsMatchAllowingSubdivisions(square, subdivided)
                && opposedLoopsMatchAllowingSubdivisions(square, cyclic),
            "Extrude portal oracle: fixed forward subdivision and reversed cyclic start match exactly");
        const double oneUlpAboveOne = std::nextafter(1.0, 2.0);
        const std::vector<sed::TVector> nearCollinear{{0, 1, 0},
            {.5, oneUlpAboveOne, 0}, {1, 1, 0}, {1, 0, 0}, {0, 0, 0}};
        auto nearCollinearReversed = nearCollinear;
        std::reverse(nearCollinearReversed.begin(), nearCollinearReversed.end());
        const std::vector<sed::TVector> doublySubdivided{{0, 0, 0}, {.5, 0, 0},
            {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
        check(opposedLoopsMatchAllowingSubdivisions(nearCollinear, nearCollinearReversed)
                && opposedLoopsMatchAllowingSubdivisions(doublySubdivided, subdivided),
            "Extrude portal oracle: a representably distinct shallow bend and subdivisions on both loops are retained");

        auto displaced = subdivided;
        displaced[3].x = 1.000001;
        auto backtrack = subdivided;
        backtrack[3].y = 1.25;
        auto duplicate = subdivided;
        duplicate[3] = duplicate[2];
        check(!opposedLoopsMatchAllowingSubdivisions(square, displaced)
                && !opposedLoopsMatchAllowingSubdivisions(square, backtrack)
                && !opposedLoopsMatchAllowingSubdivisions(square, duplicate),
            "Extrude portal oracle: fixed displaced, backtracking and duplicate-coordinate boundaries fail closed");
        auto exactDuplicate = square;
        exactDuplicate[2] = exactDuplicate[1];
        auto exactDuplicateReversed = exactDuplicate;
        std::reverse(exactDuplicateReversed.begin(), exactDuplicateReversed.end());
        auto backtrackReversed = backtrack;
        std::reverse(backtrackReversed.begin(), backtrackReversed.end());
        check(!opposedLoopsMatchAllowingSubdivisions(exactDuplicate, square)
                && !opposedLoopsMatchAllowingSubdivisions(square, exactDuplicate)
                && !opposedLoopsMatchAllowingSubdivisions(backtrack, square)
                && !opposedLoopsMatchAllowingSubdivisions(square, backtrack)
                && !opposedLoopsMatchAllowingSubdivisions(exactDuplicate, exactDuplicateReversed)
                && !opposedLoopsMatchAllowingSubdivisions(exactDuplicateReversed, exactDuplicate)
                && !opposedLoopsMatchAllowingSubdivisions(backtrack, backtrackReversed)
                && !opposedLoopsMatchAllowingSubdivisions(backtrackReversed, backtrack),
            "Extrude portal oracle: malformed first and second boundaries cannot bypass exact reciprocal matching");

        check(!opposedLoopsMatchAllowingSubdivisions(square, square),
            "Extrude portal oracle: same-facing winding is never accepted as reciprocal");
        auto nan = subdivided;
        nan[2].x = std::numeric_limits<double>::quiet_NaN();
        auto nanReversed = nan;
        std::reverse(nanReversed.begin(), nanReversed.end());
        auto infinity = subdivided;
        infinity[2].x = std::numeric_limits<double>::infinity();
        auto infinityReversed = infinity;
        std::reverse(infinityReversed.begin(), infinityReversed.end());
        const std::vector<sed::TVector> shortLoop{{0, 0, 0}, {1, 0, 0}};
        auto shortLoopReversed = shortLoop;
        std::reverse(shortLoopReversed.begin(), shortLoopReversed.end());
        check(!opposedLoopsMatchAllowingSubdivisions(square, nan)
                && !opposedLoopsMatchAllowingSubdivisions(square, infinity)
                && !opposedLoopsMatchAllowingSubdivisions(nan, square)
                && !opposedLoopsMatchAllowingSubdivisions(infinity, square)
                && !opposedLoopsMatchAllowingSubdivisions(nan, nanReversed)
                && !opposedLoopsMatchAllowingSubdivisions(infinity, infinityReversed)
                && !opposedLoopsMatchAllowingSubdivisions(shortLoop, shortLoop),
            "Extrude portal oracle: nonfinite and fewer-than-three-corner boundaries fail closed");
        check(!opposedLoopsMatchAllowingSubdivisions(shortLoop, square)
                && !opposedLoopsMatchAllowingSubdivisions(square, shortLoop)
                && !opposedLoopsMatchAllowingSubdivisions(shortLoop, shortLoopReversed),
            "Extrude portal oracle: a short boundary fails regardless of which side supplies it");
    }
    {
        // Independent decimal-quantized oblique prism. Its original projected
        // right-edge corners retain a small bend; they must never be welded.
        Level input;
        auto& owner = input.newSector();
        constexpr double origin = 3.0;
        constexpr double slope = 0.09123456;
        constexpr double warp = 1e-6;
        const std::array<TVector, 7> polygon{{{.5,0,-.91}, {.5,0,-.53}, {.5,0,-.14},
            {.5,0,.64}, {.5,0,.983}, {0,0,.983}, {0,0,-.91}}};
        for ( int layer = 0; layer < 2; ++layer )
        {
            for ( const auto& point : polygon )
            {
                const double y = std::round(slope * point.z * 1e7) / 1e7;
                owner.newVertex(origin + point.x, origin + y + (point.x == 0 ? warp : 0) - layer,
                    origin + point.z);
            }
        }
        const auto append = [&](std::vector<std::int32_t> corners)
        {
            Surface face;
            face.vertices = std::move(corners);
            for ( const auto index : face.vertices )
            {
                TexVertex tuple;
                tuple.u = static_cast<float>(index % 7) * .25f;
                tuple.v = -tuple.u;
                tuple.color = ColorF{.25f,.5f,.75f,1.0f};
                face.texVertices.push_back(tuple);
            }
            face.normal = level::surfaceCalcNormalNewell(owner, face);
            face.num = static_cast<std::int32_t>(owner.surfaces.size());
            owner.addSurface(face);
        };
        append({0,1,2,3,4,5,6});
        append({13,12,11,10,9,8,7});
        for ( int edge = 0; edge < 7; ++edge )
        {
            append({(edge + 1) % 7, edge, edge + 7, (edge + 1) % 7 + 7});
        }
        Cog cog;
        CogValue value;
        value.type = ValueType::ptr;
        value.cogType = CogType::surface;
        value.intValue = 1;
        value.stringValue = "0 1";
        cog.values.push_back(value);
        input.cogs.push_back(cog);
        const auto before = assets::saveToSedBytes(input);
        level::SurfaceExtrudeReport wireReport, solidReport, commitReport;
        const auto wire = level::levelBuildSurfaceExtrudePreview(input, {0,0}, .2, .1, &wireReport);
        const auto solid = level::levelBuildSurfaceExtrudeSectorPreview(input, {0,0}, .2, .1, &solidReport);
        check(wire && solid && wireReport.error == level::SurfaceExtrudeError::None
                && solidReport.error == level::SurfaceExtrudeError::None
                && wireReport.surface == SurfaceRef{0,0} && solidReport.surface == SurfaceRef{0,0}
                && assets::saveToSedBytes(input) == before,
            "extrude.genericFallback: both previews accept quantized convex source with exact success OUT and no mutation");
        Level output = input;
        const bool accepted = level::levelExtrudeSurface(output, {0,0}, .2, .1, &commitReport);
        check(accepted && commitReport.error == level::SurfaceExtrudeError::None
                && commitReport.surface == SurfaceRef{0,0} && output.sectors.size() == 2,
            "extrude.genericFallback: commit accepts the same quantized source and exact report");
        if ( accepted && output.sectors.size() == 2 )
        {
            bool originals = true;
            const auto& source = output.sectors[0];
            for ( const auto& vertex : input.sectors[0].vertices )
            {
                originals = originals && std::any_of(source.vertices.begin(), source.vertices.end(),
                    [&](const auto& actual) { return actual.x == vertex.x && actual.y == vertex.y && actual.z == vertex.z; });
            }
            bool tuples = true;
            for ( std::size_t corner = 0; corner < 7; ++corner )
            {
                bool found = false;
                for ( std::size_t faceIndex = 0; faceIndex < source.surfaces.size(); ++faceIndex )
                {
                    if ( faceIndex != 0 && faceIndex < 9 )
                    {
                        continue;
                    }
                    const auto& face = source.surfaces[faceIndex];
                    for ( std::size_t index = 0; index < face.vertices.size() && index < face.texVertices.size(); ++index )
                    {
                        const auto& actual = source.vertices[face.vertices[index]];
                        const auto& expected = input.sectors[0].vertices[corner];
                        found = found || (actual.x == expected.x && actual.y == expected.y && actual.z == expected.z
                            && sameTextureVertex(face.texVertices[index], input.sectors[0].surfaces[0].texVertices[corner]));
                    }
                }
                tuples = tuples && found;
            }
            check(originals && tuples, "extrude.genericFallback: every authored coordinate and source UV/RGBA tuple survives exactly");
            bool paired = true;
            std::size_t portals = 0;
            double area = 0;
            for ( const auto& face : source.surfaces )
            {
                if ( !face.adjoin )
                {
                    continue;
                }
                ++portals;
                const auto& ref = *face.adjoin;
                if ( ref.sector != 1 || ref.surface < 0 || std::size_t(ref.surface) >= output.sectors[1].surfaces.size()
                    || face.vertices.size() < 3 )
                {
                    paired = false;
                    continue;
                }
                const auto& partner = output.sectors[1].surfaces[ref.surface];
                bool reverse = false;
                if ( partner.vertices.size() == face.vertices.size() )
                {
                    for ( std::size_t start = 0; start < partner.vertices.size(); ++start )
                    {
                        bool match = true;
                        for ( std::size_t index = 0; index < face.vertices.size(); ++index )
                        {
                            const auto& a = source.vertices[face.vertices[index]];
                            const auto& b = output.sectors[1].vertices[partner.vertices[(start + partner.vertices.size() - index) % partner.vertices.size()]];
                            match = match && a.x == b.x && a.y == b.y && a.z == b.z;
                        }
                        reverse = reverse || match;
                    }
                }
                paired = paired && reverse && partner.adjoin && partner.adjoin->sector == 0
                    && partner.adjoin->surface == face.num;
                const auto& anchor = source.vertices[face.vertices[0]];
                double twiceArea = 0;
                for ( std::size_t index = 1; index + 1 < face.vertices.size(); ++index )
                {
                    const auto& a = source.vertices[face.vertices[index]];
                    const auto& b = source.vertices[face.vertices[index + 1]];
                    twiceArea += (a.x - anchor.x) * (b.z - anchor.z) - (a.z - anchor.z) * (b.x - anchor.x);
                }
                area += std::abs(twiceArea) * .5;
            }
            check(paired && portals > 0 && std::abs(area - .9465) < 1e-12,
                "extrude.genericFallback: exact reciprocal portals cover the full fixed0.9465 XZ footprint");
            check(output.cogs.size() == 1 && output.cogs[0].values.size() == 1
                    && output.cogs[0].values[0].intValue == 1 && output.cogs[0].values[0].stringValue == "0 1",
                "extrude.genericFallback: unrelated COG target and full authored pair string remain exact");
            if ( solid )
            {
                // Rendering hides the inner cap; compare exact geometry pools.
                check(solid->vertices.size() == output.sectors[1].vertices.size()
                        && std::equal(solid->vertices.begin(), solid->vertices.end(), output.sectors[1].vertices.begin(),
                            [](const auto& a, const auto& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }),
                    "extrude.genericFallback: solid preview and committed generated coordinates agree exactly");
            }
        }
    }

    for ( const bool reverse : {false, true} )
    {
        Level level;
        auto& sector = level.newSector();
        sector.vertices = {{0,0,0}, {2,0,0}, {2,2,0}, {0,2,0}};
        Surface face;
        face.vertices = reverse ? std::vector<std::int32_t>{1,0,3,2} : std::vector<std::int32_t>{0,1,2,3};
        face.normal = reverse ? TVector{0,0,-1} : TVector{0,0,1};
        for ( const auto index : face.vertices )
        {
            const bool right = index == 1 || index == 2;
            TexVertex tuple;
            tuple.u = right ? 1e-12f : 1.0f;
            tuple.v = right ? -1e-12f : -1.0f;
            tuple.color = right ? ColorF{1e-12f,2e-12f,3e-12f,1e-12f} : ColorF{1.0f,0.5f,0.25f,1.0f};
            face.texVertices.push_back(tuple);
        }
        sector.addSurface(face);
        const Level before = level;
        const bool accepted = sed::level::levelCleaveEdge(level, {0,0}, 0, {1,0,0}, 1.9999999999, 0, 0);
        check(accepted && level.sectors.size() == 1 && level.sectors[0].surfaces.size() == 1,
            "Edge UV endpoint: both edge orientations accept the same independently fixed interior plane");
        if ( !accepted || level.sectors.size() != 1 || level.sectors[0].surfaces.size() != 1 )
        {
            continue;
        }
        const auto& output = level.sectors[0];
        const auto& result = output.surfaces[0];
        bool found = false;
        bool exact = result.vertices.size() == 5 && result.texVertices.size() == 5;
        bool originals = output.vertices.size() >= 4;
        for ( std::size_t index = 0; originals && index < 4; ++index )
        {
            const auto& original = before.sectors[0].vertices[index];
            const auto& current = output.vertices[index];
            originals = original.x == current.x && original.y == current.y && original.z == current.z;
        }
        if ( result.vertices.size() == result.texVertices.size() )
        {
            for ( std::size_t corner = 0; corner < result.vertices.size(); ++corner )
            {
                const auto index = result.vertices[corner];
                if ( index < 0 || static_cast<std::size_t>(index) >= output.vertices.size() )
                {
                    exact = false;
                    continue;
                }
                const auto& point = output.vertices[index];
                const auto& tuple = result.texVertices[corner];
                const auto originalCorner = std::find(face.vertices.begin(), face.vertices.end(), index);
                if ( originalCorner != face.vertices.end() )
                {
                    originals = originals && sameTextureVertex(tuple,
                        face.texVertices[static_cast<std::size_t>(originalCorner - face.vertices.begin())]);
                }
                if ( point.x == 1.9999999999 && point.y == 0.0 && point.z == 0.0 )
                {
                    found = true;
                    // Correctly rounded float vectors for endpoint values and
                    // the fixed binary-double cut coordinate above. No result
                    // helper supplies the oracle or chooses its orientation.
                    exact = exact && tuple.u == 5.10000028e-11f && tuple.v == -5.10000028e-11f
                        && tuple.color == ColorF{5.10000028e-11f,2.70000012e-11f,1.55000006e-11f,5.10000028e-11f};
                }
            }
        }
        check(found && exact,
            "Edge UV endpoint: fixed near-zero U/V/RGBA vectors remain identical under reversed edge orientation");
        check(originals,
            "Edge UV endpoint: successful insertion preserves every original coordinate and corner tuple exactly");
    }
    {
        const double largest = std::numeric_limits<double>::max();
        const double smallest = std::numeric_limits<double>::denorm_min();
        struct CachedNormalCase
        {
            TVector normal;
            level::SurfaceExtrudeError expected;
        };
        const std::array<CachedNormalCase, 6> cases{{
            {{0.6, 0.8, 0.0}, level::SurfaceExtrudeError::None},
            {{largest * 0.75, largest, 0.0}, level::SurfaceExtrudeError::None},
            {{smallest * 3.0, smallest * 4.0, 0.0}, level::SurfaceExtrudeError::None},
            {{0.0, 0.0, 0.0}, level::SurfaceExtrudeError::None},
            {{std::numeric_limits<double>::quiet_NaN(), 0.8, 0.0}, level::SurfaceExtrudeError::InvalidSourceTopology},
            {{-0.6, -0.8, 0.0}, level::SurfaceExtrudeError::NonPlanarSource}}};
        for ( std::size_t caseIndex = 0; caseIndex < cases.size(); ++caseIndex )
        {
            Level original;
            auto& sector = original.newSector();
            // Independently authored two-unit square: U=(-.8,.6,0),
            // V=(0,0,1), inward unit normal=(.6,.8,0).
            sector.vertices = {{0,0,0}, {-1.6,1.2,0}, {-1.6,1.2,2}, {0,0,2}};
            Surface face;
            face.vertices = {0,1,2,3};
            face.texVertices.resize(4);
            face.normal = cases[caseIndex].normal;
            sector.addSurface(face);
            const std::array<TVector, 4> expectedInner{{
                {0.0, 0.0, 2.0}, {-1.6, 1.2, 2.0}, {-1.6, 1.2, 0.0}, {0.0, 0.0, 0.0}}};
            const std::array<TVector, 4> expectedOuter{{
                {-0.15, -0.2, 0.0}, {-1.75, 1.0, 0.0}, {-1.75, 1.0, 2.0}, {-0.15, -0.2, 2.0}}};
            const auto closePosition = [](const auto& actual, const TVector& expected)
            {
                return std::abs(actual.x - expected.x) <= 2.0e-14
                    && std::abs(actual.y - expected.y) <= 2.0e-14
                    && std::abs(actual.z - expected.z) <= 2.0e-14;
            };
            const auto sourceRimArea = [](const Sector& clipped)
            {
                constexpr TVector sourceNormal{0.6, 0.8, 0.0};
                double area = 0.0;
                bool valid = true;
                for ( const auto& rim : clipped.surfaces )
                {
                    valid = valid && !rim.adjoin && rim.vertices.size() >= 3
                        && rim.vertices.size() == rim.texVertices.size();
                    if ( !valid )
                    {
                        break;
                    }
                    for ( const auto index : rim.vertices )
                    {
                        valid = valid && index >= 0
                            && static_cast<std::size_t>(index) < clipped.vertices.size();
                    }
                    if ( !valid )
                    {
                        break;
                    }
                    const auto& anchor = clipped.vertices[static_cast<std::size_t>(rim.vertices.front())];
                    double twiceArea = 0.0;
                    for ( std::size_t corner = 1; corner + 1 < rim.vertices.size(); ++corner )
                    {
                        const auto& first = clipped.vertices[static_cast<std::size_t>(rim.vertices[corner])];
                        const auto& second = clipped.vertices[static_cast<std::size_t>(rim.vertices[corner + 1])];
                        const TVector a{first.x - anchor.x, first.y - anchor.y, first.z - anchor.z};
                        const TVector b{second.x - anchor.x, second.y - anchor.y, second.z - anchor.z};
                        twiceArea += sourceNormal.dot(a.cross(b));
                    }
                    area += std::abs(twiceArea) * 0.5;
                }
                return std::pair{valid, area};
            };
            for ( int operation = 0; operation < 5; ++operation )
            {
                Level candidate = original;
                level::SurfaceExtrudeReport report{level::SurfaceExtrudeError::AllocationFailure, {-8,-9}};
                bool accepted = false;
                if ( operation == 0 )
                {
                    const auto result = level::levelBuildSurfaceExtrudePreview(candidate, {0,0}, 0.25, 0.0, &report);
                    accepted = result.has_value();
                    if ( result )
                    {
                        bool exactConstruction = result->innerLoop.size() == 4 && result->outerLoop.size() == 4;
                        for ( const auto& point : result->outerLoop )
                        {
                            exactConstruction = exactConstruction && std::any_of(original.sectors[0].vertices.begin(),
                                original.sectors[0].vertices.end(), [&](const Vertex& source)
                                {
                                    return std::abs(point.x - (source.x - 0.15)) <= 2e-15
                                        && std::abs(point.y - (source.y - 0.2)) <= 2e-15 && point.z == source.z;
                                });
                        }
                        check(exactConstruction,
                            "Extrude cache magnitude: geometric winding independently fixes all outer positions");
                        bool fixedLoops = result->innerLoop.size() == expectedInner.size()
                            && result->outerLoop.size() == expectedOuter.size();
                        for ( std::size_t corner = 0; fixedLoops && corner < expectedInner.size(); ++corner )
                        {
                            fixedLoops = closePosition(result->innerLoop[corner], expectedInner[corner])
                                && closePosition(result->outerLoop[corner], expectedOuter[corner]);
                        }
                        check(fixedLoops,
                            "Extrude cache magnitude: wire preview retains fixed inner and translated outer vectors");
                    }
                }
                else if ( operation == 1 )
                {
                    const auto result = level::levelBuildSurfaceExtrudeSectorPreview(
                        candidate, {0,0}, 0.25, 0.0, &report);
                    accepted = result.has_value();
                    if ( result )
                    {
                        bool fixedSolid = result->vertices.size() == expectedInner.size() + expectedOuter.size()
                            && result->surfaces.size() == 6;
                        for ( std::size_t corner = 0; fixedSolid && corner < expectedInner.size(); ++corner )
                        {
                            fixedSolid = closePosition(result->vertices[corner], expectedInner[corner])
                                && closePosition(result->vertices[expectedInner.size() + corner], expectedOuter[corner]);
                        }
                        check(fixedSolid,
                            "Extrude cache magnitude: solid preview publishes fixed cap vectors and six closed faces");
                    }
                }
                else if ( operation == 2 )
                {
                    const auto result = level::levelSurfaceExtrudeExpandBounds(candidate, {0,0}, &report);
                    accepted = result.has_value();
                    if ( result )
                    {
                        check(std::abs(result->negativeCollapseLimit + 1.0) <= 2.0e-14
                                && std::abs(result->minSpan - 2.0) <= 2.0e-14,
                            "Extrude cache magnitude: expand bounds retain fixed two-unit span and collapse limit");
                    }
                }
                else if ( operation == 3 )
                {
                    const auto result = level::levelBuildSurfaceExtrudeSourceCutPreview(
                        candidate, {0,0}, -0.1, &report);
                    accepted = result.has_value();
                    if ( result )
                    {
                        const auto [valid, area] = sourceRimArea(*result);
                        check(valid && result->surfaces.size() == 4
                                && std::abs(area - 0.76) <= 2.0e-12,
                            "Extrude cache magnitude: source-cut preview retains four disjoint faces and fixed 0.76 rim area");
                    }
                }
                else
                {
                    accepted = level::levelExtrudeSurface(candidate, {0,0}, 0.25, 0.0, &report);
                    if ( accepted )
                    {
                        bool fixedCommit = candidate.sectors.size() == 2
                            && candidate.sectors[1].vertices.size() == expectedInner.size() + expectedOuter.size()
                            && candidate.sectors[1].surfaces.size() == 6;
                        for ( std::size_t corner = 0; fixedCommit && corner < expectedInner.size(); ++corner )
                        {
                            fixedCommit = closePosition(candidate.sectors[1].vertices[corner], expectedInner[corner])
                                && closePosition(candidate.sectors[1].vertices[expectedInner.size() + corner], expectedOuter[corner]);
                        }
                        check(fixedCommit,
                            "Extrude cache magnitude: committed solid retains the preview's fixed cap vectors");
                    }
                }
                const bool expectedAccepted = cases[caseIndex].expected == level::SurfaceExtrudeError::None;
                if ( accepted != expectedAccepted || report.error != cases[caseIndex].expected )
                {
                    std::cerr << "Extrude cache normal case=" << caseIndex << " operation=" << operation
                        << " accepted=" << accepted << " error=" << static_cast<int>(report.error) << '\n';
                }
                check(accepted == expectedAccepted && report.error == cases[caseIndex].expected
                        && report.surface == SurfaceRef{0,0},
                    "Extrude cache magnitude: all five API return/report contracts accept finite same-facing scales and reject invalid directions");
                if ( cases[caseIndex].normal.x == 0.0
                    && cases[caseIndex].normal.y == 0.0
                    && cases[caseIndex].normal.z == 0.0 )
                {
                    const auto& sourceNormal = candidate.sectors[0].surfaces[0].normal;
                    check(sourceNormal.x == 0.0 && sourceNormal.y == 0.0 && sourceNormal.z == 0.0,
                        "Extrude zero cache: all five APIs preserve the authored unset source normal exactly");
                }
                if ( operation != 4 || !accepted )
                {
                    check(assets::sedLevelsEqual(candidate, original),
                        "Extrude cache magnitude: previews and rejected commits preserve complete input including malformed cache bits");
                }
            }
        }
    }
    {
        const auto roundedFixture = []
        {
            sed::Level level;
            level.layers = {"Original"};
            level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
            sed::Sector source;
            source.num = 0;
            source.id = 10;
            source.vertices = {{0,0,0}, {1,0,0}, {1,1,0.00005}, {0,1,0},
                {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}};
            const std::array<std::vector<std::int32_t>, 6> loops{{
                {0,1,2,3}, {4,7,6,5}, {0,3,7,4}, {1,5,6,2}, {0,4,5,1}, {3,2,6,7}}};
            const std::array<sed::TVector, 6> normals{{{0,0,1}, {0,0,-1}, {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}}};
            for ( std::size_t index = 0; index < loops.size(); ++index )
            {
                sed::Surface face;
                face.num = static_cast<std::int32_t>(index);
                face.vertices = loops[index];
                face.normal = normals[index];
                face.geo = 4;
                face.surfaceFlags = sed::SF_Collision;
                face.texVertices.resize(4);
                for ( std::size_t corner = 0; corner < 4; ++corner )
                {
                    face.texVertices[corner].u = static_cast<float>(corner) / 4.0f;
                    face.texVertices[corner].v = static_cast<float>(index) / 8.0f;
                }
                source.surfaces.push_back(std::move(face));
            }
            auto neighbor = source;
            neighbor.num = 1;
            neighbor.id = 11;
            for ( auto& vertex : neighbor.vertices )
            {
                vertex.z = vertex.z < 0.5 ? 1.0 : 2.0;
            }
            source.surfaces[1].adjoin = sed::SurfaceRef{1,0};
            neighbor.surfaces[0].adjoin = sed::SurfaceRef{0,1};
            level.sectors = {source, neighbor};
            level.curId = 12;
            return level;
        };
        const auto closed = [](const sed::Sector& sector)
        {
            std::map<std::pair<std::int32_t, std::int32_t>, std::pair<int, int>> incidence;
            for ( const auto& face : sector.surfaces )
            {
                for ( std::size_t index = 0; index < face.vertices.size(); ++index )
                {
                    const auto a = face.vertices[index];
                    const auto b = face.vertices[(index + 1) % face.vertices.size()];
                    if ( a == b || a < 0 || b < 0 || static_cast<std::size_t>(a) >= sector.vertices.size()
                        || static_cast<std::size_t>(b) >= sector.vertices.size() )
                    {
                        return false;
                    }
                    auto& edge = incidence[{std::min(a,b), std::max(a,b)}];
                    ++edge.first;
                    edge.second += a < b ? 1 : -1;
                }
            }
            return !incidence.empty() && std::all_of(incidence.begin(), incidence.end(),
                [](const auto& edge) { return edge.second.first == 2 && edge.second.second == 0; });
        };
        for ( const double expand : {0.0, 0.1, -0.1} )
        {
            auto level = roundedFixture();
            const auto baseline = level;
            const auto before = sed::assets::saveToSedBytes(level);
            sed::level::SurfaceExtrudeReport report{sed::level::SurfaceExtrudeError::InvalidResult, {-9,-9}};
            const auto wire = sed::level::levelBuildSurfaceExtrudePreview(level, {0,0}, 0.1, expand, &report);
            check(wire && report.error == sed::level::SurfaceExtrudeError::None
                    && report.surface == sed::SurfaceRef{0,0},
                "extrude.rounded: plain and signed Expand wire previews accept inherited source quantization with complete report");
            const auto bounds = sed::level::levelSurfaceExtrudeExpandBounds(level, {0,0}, &report);
            // The fitted plane projects the warped unit quad to a span of
            // 1.0000000009375; allow the rounding of its normalization/dot sums.
            constexpr double fittedSpan = 1.0000000009375;
            constexpr double spanError = 8.0 * std::numeric_limits<double>::epsilon();
            check(bounds && std::abs(bounds->minSpan - fittedSpan) <= spanError
                    && bounds->negativeCollapseLimit == -0.5 * bounds->minSpan
                    && report.error == sed::level::SurfaceExtrudeError::None && report.surface == sed::SurfaceRef{0,0},
                "extrude.rounded: bounds return analytically projected dimensions and complete success OUT");
            const auto solid = sed::level::levelBuildSurfaceExtrudeSectorPreview(level, {0,0}, 0.1, expand, &report);
            check(solid && closed(*solid) && report.error == sed::level::SurfaceExtrudeError::None,
                "extrude.rounded: solid preview remains independently closed for plain and signed Expand");
            check(sed::assets::saveToSedBytes(level) == before,
                "extrude.rounded: all preview paths preserve complete source and valid neighbor");
            std::optional<sed::Sector> cut;
            if ( expand < 0.0 )
            {
                cut = sed::level::levelBuildSurfaceExtrudeSourceCutPreview(level, {0,0}, expand, &report);
                check(cut && !cut->surfaces.empty() && report.error == sed::level::SurfaceExtrudeError::None,
                    "extrude.rounded: negative Expand exposes matching inherited source-cut preview");
            }
            const bool applied = sed::level::levelExtrudeSurface(level, {0,0}, 0.1, expand, &report);
            check(applied && level.sectors.size() >= 3 && report.error == sed::level::SurfaceExtrudeError::None
                    && report.surface == sed::SurfaceRef{0,0},
                "extrude.rounded: plain positive and negative Expand publish successfully with exact success report");
            if ( !applied )
            {
                std::cerr << "RoundedExtrude fixture=axis origin=0 expand=" << expand
                    << " error=" << static_cast<int>(report.error) << '\n';
                continue;
            }
            if ( cut )
            {
                bool sameRims = true;
                for ( const auto& rim : cut->surfaces )
                {
                    bool found = false;
                    for ( const auto& face : level.sectors[0].surfaces )
                    {
                        if ( face.adjoin || face.vertices.size() != rim.vertices.size() )
                        {
                            continue;
                        }
                        bool same = true;
                        for ( std::size_t corner = 0; same && corner < rim.vertices.size(); ++corner )
                        {
                            const auto& a = cut->vertices[rim.vertices[corner]];
                            const auto& b = level.sectors[0].vertices[face.vertices[corner]];
                            same = a.x == b.x && a.y == b.y && a.z == b.z
                                && sameTextureVertex(rim.texVertices[corner], face.texVertices[corner]);
                        }
                        found = found || same;
                    }
                    sameRims = sameRims && found;
                }
                check(sameRims, "extrude.rounded: source-cut preview matches every retained committed rim corner and UV exactly");
            }
            if ( wire )
            {
                const auto& pool = level.sectors[2].vertices;
                bool exactWire = pool.size() >= wire->innerLoop.size() + wire->outerLoop.size();
                for ( std::size_t index = 0; exactWire && index < wire->innerLoop.size(); ++index )
                {
                    const auto& inner = pool[index];
                    const auto& outer = pool[wire->innerLoop.size() + index];
                    exactWire = inner.x == wire->innerLoop[index].x && inner.y == wire->innerLoop[index].y
                        && inner.z == wire->innerLoop[index].z && outer.x == wire->outerLoop[index].x
                        && outer.y == wire->outerLoop[index].y && outer.z == wire->outerLoop[index].z;
                }
                check(exactWire, "extrude.rounded: wire endpoints exactly match the committed initial generated vertex pool");
            }
            sed::Level originalNeighbor;
            originalNeighbor.sectors = {baseline.sectors[1]};
            sed::Level actualNeighbor;
            actualNeighbor.sectors = {level.sectors[1]};
            check(sed::assets::sedLevelsEqual(originalNeighbor, actualNeighbor),
                "extrude.rounded: valid unrelated portal neighbor retains complete authored geometry and metadata");
            bool retained = true;
            for ( const auto& original : baseline.sectors[0].vertices )
            {
                retained = retained && std::any_of(level.sectors[0].vertices.begin(), level.sectors[0].vertices.end(),
                    [&](const auto& vertex) { return vertex.x == original.x && vertex.y == original.y && vertex.z == original.z; });
            }
            check(retained, "extrude.rounded: original authored source coordinates are retained exactly without flattening or welding");
            bool retainedUv = true;
            const auto& originalFace = baseline.sectors[0].surfaces[0];
            for ( std::size_t corner = 0; corner < originalFace.vertices.size(); ++corner )
            {
                const auto& point = baseline.sectors[0].vertices[originalFace.vertices[corner]];
                bool found = false;
                for ( std::size_t index = 0; index < level.sectors[0].surfaces.size(); ++index )
                {
                    if ( index != 0 && index < baseline.sectors[0].surfaces.size() )
                    {
                        continue;
                    }
                    const auto& face = level.sectors[0].surfaces[index];
                    for ( std::size_t current = 0; current < face.vertices.size(); ++current )
                    {
                        const auto& actual = level.sectors[0].vertices[face.vertices[current]];
                        found = found || (actual.x == point.x && actual.y == point.y && actual.z == point.z
                            && sameTextureVertex(face.texVertices[current], originalFace.texVertices[corner]));
                    }
                }
                retainedUv = retainedUv && found;
            }
            check(retainedUv, "extrude.rounded: source split fragments retain original corner UV and baked-light values exactly");

            bool valid = true;
            for ( std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex )
            {
                const auto& sector = level.sectors[sectorIndex];
                valid = valid && closed(sector);
                for ( std::size_t faceIndex = 0; faceIndex < sector.surfaces.size(); ++faceIndex )
                {
                    const auto& face = sector.surfaces[faceIndex];
                    if ( !face.adjoin )
                    {
                        continue;
                    }
                    const auto mirror = *face.adjoin;
                    if ( mirror.sector < 0 || static_cast<std::size_t>(mirror.sector) >= level.sectors.size()
                        || mirror.surface < 0 || static_cast<std::size_t>(mirror.surface) >= level.sectors[mirror.sector].surfaces.size() )
                    {
                        valid = false;
                        continue;
                    }
                    const auto& otherSector = level.sectors[mirror.sector];
                    const auto& other = otherSector.surfaces[mirror.surface];
                    valid = valid && other.adjoin == std::optional<sed::SurfaceRef>{{static_cast<std::int32_t>(sectorIndex), static_cast<std::int32_t>(faceIndex)}};
                    bool reversed = face.vertices.size() == other.vertices.size();
                    bool matchedRotation = false;
                    for ( std::size_t start = 0; reversed && start < other.vertices.size(); ++start )
                    {
                        bool match = true;
                        for ( std::size_t corner = 0; match && corner < face.vertices.size(); ++corner )
                        {
                            const auto& point = sector.vertices[face.vertices[corner]];
                            const auto& candidate = otherSector.vertices[other.vertices[
                                (start + other.vertices.size() - corner) % other.vertices.size()]];
                            match = point.x == candidate.x && point.y == candidate.y && point.z == candidate.z;
                        }
                        matchedRotation = matchedRotation || match;
                    }
                    valid = valid && reversed && matchedRotation;
                }
            }
            check(valid, "extrude.rounded: every original and generated solid stays closed with exact reciprocal portal corners");
            if ( solid )
            {
                bool previewCorners = true;
                for ( const auto& point : solid->vertices )
                {
                    bool found = false;
                    for ( std::size_t index = baseline.sectors.size(); index < level.sectors.size(); ++index )
                    {
                        found = found || std::any_of(level.sectors[index].vertices.begin(), level.sectors[index].vertices.end(),
                            [&](const auto& candidate) { return point.x == candidate.x && point.y == candidate.y && point.z == candidate.z; });
                    }
                    previewCorners = previewCorners && found;
                }
                check(previewCorners, "extrude.rounded: all solid-preview corners occur exactly in the accepted generated geometry");
            }
            {
                const auto& generated = level.sectors[2];
                bool strictSides = true;
                for ( std::size_t index = 2; index < (expand == 0.0 ? baseline.sectors[0].surfaces[0].vertices.size() + 2 : 6); ++index )
                {
                    strictSides = strictSides && sed::level::surfaceIsPlanar(generated, generated.surfaces[index])
                        && sed::level::surfaceIsConvex(generated, generated.surfaces[index]);
                }
                check(strictSides, "extrude.rounded: all three modes retain strict planarity and convexity on original generated side indices");
            }
        }
        {
            // Independent rational orthonormal frame; no production matrix or
            // normal calculator supplies the expected authoring geometry.
            const sed::TVector u{0.6,0.8,0};
            const sed::TVector v{0,0,1};
            const sed::TVector n{0.8,-0.6,0};
            const std::array<std::array<double,2>,6> polygon{{{0,0},{2,0},{2,1},{1.5,2},{0.5,2},{0,1}}};
            for ( const double origin : {0.0, 10000.0} )
            for ( const double expand : {0.0, 0.1, -0.1} )
            {
                sed::Level level;
                level.layers = {"Original"};
                auto& sector = level.newSector();
                for ( int cap = 0; cap < 2; ++cap )
                {
                    for ( std::size_t corner = 0; corner < polygon.size(); ++corner )
                    {
                        const double height = cap == 1 ? 1.0 : corner == 3 ? 0.00005 : 0.0;
                        const auto point = sed::TVector{origin,origin,origin}
                            + u.scaled(polygon[corner][0]) + v.scaled(polygon[corner][1]) + n.scaled(height);
                        sector.vertices.push_back({point.x,point.y,point.z});
                    }
                }
                const auto addFace = [&](std::vector<std::int32_t> indices, const sed::TVector normal)
                {
                    sed::Surface face;
                    face.vertices = std::move(indices);
                    face.texVertices.resize(face.vertices.size());
                    face.normal = normal;
                    sector.addSurface(std::move(face));
                };
                addFace({0,1,2,3,4,5}, n);
                addFace({11,10,9,8,7,6}, -n);
                for ( std::int32_t corner = 0; corner < 6; ++corner )
                {
                    const auto next = (corner + 1) % 6;
                    const double dx = polygon[next][0] - polygon[corner][0];
                    const double dy = polygon[next][1] - polygon[corner][1];
                    const double length = std::hypot(dx,dy);
                    addFace({next,corner,corner+6,next+6}, u.scaled(-dy/length) + v.scaled(dx/length));
                }
                const auto before = sed::assets::saveToSedBytes(level);
                sed::level::SurfaceExtrudeReport report;
                const auto wire = sed::level::levelBuildSurfaceExtrudePreview(level, {0,0}, 0.1, expand, &report);
                const auto solid = sed::level::levelBuildSurfaceExtrudeSectorPreview(level, {0,0}, 0.1, expand, &report);
                check(wire && solid && closed(*solid) && report.error == sed::level::SurfaceExtrudeError::None
                        && sed::assets::saveToSedBytes(level) == before,
                    "extrude.rounded: six-corner oblique translated sources support immutable plain and signed Expand previews");
                const bool applied = sed::level::levelExtrudeSurface(level, {0,0}, 0.1, expand, &report);
                check(applied && level.sectors.size() == 2 && closed(level.sectors[0]) && closed(level.sectors[1])
                        && report.error == sed::level::SurfaceExtrudeError::None && report.surface == sed::SurfaceRef{0,0},
                    "extrude.rounded: six-corner oblique original-like source commits all Expand directions at large origins");
                if ( !applied )
                {
                    std::cerr << "RoundedExtrude fixture=oblique origin=" << origin << " expand=" << expand
                        << " error=" << static_cast<int>(report.error) << '\n';
                }
                if ( applied && solid )
                {
                    bool exact = solid->vertices.size() == level.sectors[1].vertices.size();
                    for ( std::size_t index = 0; exact && index < solid->vertices.size(); ++index )
                    {
                        const auto& a = solid->vertices[index];
                        const auto& b = level.sectors[1].vertices[index];
                        exact = a.x == b.x && a.y == b.y && a.z == b.z;
                    }
                    const auto& generated = level.sectors[1];
                    exact = exact && solid->surfaces.size() == generated.surfaces.size();
                    for ( std::size_t index = 0; exact && index < generated.surfaces.size(); ++index )
                    {
                        const auto& a = solid->surfaces[index];
                        const auto& b = generated.surfaces[index];
                        exact = a.vertices == b.vertices && sameTextureVertices(a.texVertices, b.texVertices)
                            && a.material == b.material && a.surfaceFlags == b.surfaceFlags
                            && a.normal.x == b.normal.x && a.normal.y == b.normal.y && a.normal.z == b.normal.z;
                    }
                    check(exact, "extrude.rounded: oblique solid preview and commit share exact vertices face partitions and UV RGBA corners");
                    if ( expand < 0.0 && generated.vertices.size() > 10 )
                    {
                        // Fixed boundary of the left rectangular side after
                        // its inherited inner-edge kink. Integrate this ordered
                        // five-corner polygon independently of ear partitioning.
                        const std::array<std::int32_t, 5> boundary{{1,10,0,7,6}};
                        const auto localPoint = [&](const std::int32_t index)
                        {
                            const auto& point = generated.vertices[index];
                            return sed::TVector{point.x - origin, point.y - origin, point.z - origin};
                        };
                        const auto polygonIntegrals = [&](const auto& indices)
                        {
                            sed::TVector twiceArea{};
                            double sixVolume = 0.0;
                            const auto first = localPoint(indices[0]);
                            for ( std::size_t index = 1; index + 1 < indices.size(); ++index )
                            {
                                const auto b = localPoint(indices[index]);
                                const auto c = localPoint(indices[index + 1]);
                                twiceArea = twiceArea + (b - first).cross(c - first);
                                sixVolume += first.dot(b.cross(c));
                            }
                            return std::pair{twiceArea, sixVolume};
                        };
                        const auto expected = polygonIntegrals(boundary);
                        sed::TVector actualArea{};
                        double actualVolume = 0.0;
                        std::size_t pieces = 0;
                        bool strictPieces = true;
                        std::map<std::int32_t, sed::TexVertex> cornerValues;
                        for ( const auto& part : generated.surfaces )
                        {
                            if ( part.vertices.size() < 3 || part.texVertices.size() != part.vertices.size()
                                || !std::all_of(part.vertices.begin(), part.vertices.end(), [&](const auto vertex)
                                    { return vertex >= 0 && static_cast<std::size_t>(vertex) < generated.vertices.size(); }) )
                            {
                                strictPieces = false;
                                continue;
                            }
                            if ( !std::all_of(part.vertices.begin(), part.vertices.end(), [&](const auto vertex)
                                { return std::find(boundary.begin(), boundary.end(), vertex) != boundary.end(); }) )
                            {
                                continue;
                            }
                            ++pieces;
                            strictPieces = strictPieces && part.vertices.size() == 3
                                && sed::level::surfaceIsPlanar(generated, part)
                                && sed::level::surfaceIsConvex(generated, part);
                            const auto integrals = polygonIntegrals(part.vertices);
                            actualArea = actualArea + integrals.first;
                            actualVolume += integrals.second;
                            for ( std::size_t corner = 0; corner < part.vertices.size(); ++corner )
                            {
                                const auto [stored, inserted] = cornerValues.emplace(part.vertices[corner], part.texVertices[corner]);
                                strictPieces = strictPieces && (inserted || sameTextureVertex(stored->second, part.texVertices[corner]));
                            }
                        }
                        // Independent prepartition texture frame: the side's
                        // first edge runs z=1.9 to z=0.1 at320 pixels/unit,
                        // the inherited corner lies at z=1.2, and extrusion
                        // depth0.1 follows negative V. Normal fitting changes
                        // these dimensions only at second order in the warp.
                        struct ExpectedCornerUv
                        {
                            std::int32_t vertex;
                            float u;
                            float v;
                        };
                        constexpr std::array<ExpectedCornerUv, 5> expectedUvs{{
                            {1,0.0f,0.0f}, {10,224.0f,0.0f}, {0,576.0f,0.0f},
                            {7,576.0f,-32.0f}, {6,0.0f,-32.0f}}};
                        bool fixedUvs = true;
                        const double positionError = 128.0 * std::numeric_limits<double>::epsilon()
                            * (1.0 + std::abs(origin)) * 320.0;
                        for ( const auto& expectedUv : expectedUvs )
                        {
                            const auto found = cornerValues.find(expectedUv.vertex);
                            if ( found == cornerValues.end() )
                            {
                                fixedUvs = false;
                                continue;
                            }
                            const auto& actualUv = found->second;
                            // Bound stored float rounding plus the local frame's
                            // double operations and translated input quantization.
                            const auto uvError = [&](const float expectedValue)
                            {
                                return 4.0 * std::numeric_limits<float>::epsilon() * std::abs(expectedValue)
                                    + positionError;
                            };
                            fixedUvs = fixedUvs && std::abs(actualUv.u - expectedUv.u) <= uvError(expectedUv.u)
                                && std::abs(actualUv.v - expectedUv.v) <= uvError(expectedUv.v)
                                && actualUv.color.r == 1.0f && actualUv.color.g == 1.0f
                                && actualUv.color.b == 1.0f && actualUv.color.a == 1.0f;
                        }
                        check(fixedUvs,
                            "extrude.partition: fixed independent side UV vectors and white RGBA survive partition at both origins");
                        // Local products span at most a few units. This bound
                        // also includes stored-coordinate rounding at origin.
                        const double error = 256.0 * std::numeric_limits<double>::epsilon() * (1.0 + std::abs(origin));
                        check(pieces == 3 && cornerValues.size() == 5 && strictPieces
                                && std::abs(actualArea.x - expected.first.x) <= error
                                && std::abs(actualArea.y - expected.first.y) <= error
                                && std::abs(actualArea.z - expected.first.z) <= error
                                && std::abs(actualVolume - expected.second) <= error,
                            "extrude.partition: three strict triangles preserve five-corner coverage signed volume and exact shared UV RGBA");
                    }
                }
            }
        }
        for ( const double expand : {0.0, 0.1, -0.1} )
        {
            auto level = roundedFixture();
            level.sectors[0].vertices[2].z = 0.0;
            level.sectors[1].surfaces[0].adjoin.reset();
            const auto before = level;
            sed::level::SurfaceExtrudeReport report;
            const auto verify = [&](auto call)
            {
                report = {sed::level::SurfaceExtrudeError::None, {-8,-9}};
                check(!call() && report.error == sed::level::SurfaceExtrudeError::InvalidAdjoin
                        && report.surface == sed::SurfaceRef{0,0} && sed::assets::sedLevelsEqual(level, before),
                    "extrude.previewParity: planar wire solid and commit reject broken reciprocal source links with identical report and no mutation");
            };
            verify([&] { return bool(sed::level::levelBuildSurfaceExtrudePreview(level, {0,0}, 0.1, expand, &report)); });
            verify([&] { return bool(sed::level::levelBuildSurfaceExtrudeSectorPreview(level, {0,0}, 0.1, expand, &report)); });
            verify([&] { return sed::level::levelExtrudeSurface(level, {0,0}, 0.1, expand, &report); });
        }
        {
            auto level = roundedFixture();
            level.sectors[0].vertices[2].z = 0.0;
            // The reciprocal portal remains exact at z=1; only the remote
            // neighbor's opposite cap is materially warped.
            level.sectors[1].vertices[6].z += 0.25;
            const auto before = level;
            sed::level::SurfaceExtrudeReport report;
            const auto verify = [&](auto call)
            {
                report = {sed::level::SurfaceExtrudeError::None, {-7,-8}};
                check(!call() && report.error == sed::level::SurfaceExtrudeError::InvalidAdjoin
                        && report.surface == sed::SurfaceRef{0,0} && sed::assets::sedLevelsEqual(level, before),
                    "extrude.neighborPreflight: complete read-only owner admission rejects a malformed remote face equally in wire solid and commit");
            };
            verify([&] { return bool(sed::level::levelBuildSurfaceExtrudePreview(level, {0,0}, 0.1, 0.1, &report)); });
            verify([&] { return bool(sed::level::levelBuildSurfaceExtrudeSectorPreview(level, {0,0}, 0.1, 0.1, &report)); });
            verify([&] { return sed::level::levelExtrudeSurface(level, {0,0}, 0.1, 0.1, &report); });
        }
        for ( const bool reflex : {false, true} )
        {
            sed::Level level;
            auto& sector = level.newSector();
            sector.vertices = reflex
                ? std::vector<sed::Vertex>{{0,0,0},{1,0,0},{1,0.5,0},{0.99999,0.5,0},{1,1,0},{0,1,0}}
                : std::vector<sed::Vertex>{{0,0,0},{2,0,0},{0,2,0}};
            sed::Surface face;
            face.normal = {0,0,1};
            for ( std::size_t index = 0; index < sector.vertices.size(); ++index )
            {
                face.vertices.push_back(static_cast<std::int32_t>(index));
                face.texVertices.push_back({});
            }
            sector.addSurface(face);
            const auto before = level;
            sed::level::SurfaceExtrudeReport report;
            const auto cut = sed::level::levelBuildSurfaceExtrudeSourceCutPreview(level, {0,0}, reflex ? -0.1 : -0.25, &report);
            check(cut && report.error == sed::level::SurfaceExtrudeError::None && report.surface == sed::SurfaceRef{0,0}
                    && sed::assets::sedLevelsEqual(level, before),
                "extrude.sourceCutFootprint: triangle and bounded-reflex source return truthful immutable visible rims");
            if ( cut )
            {
                double area = 0.0;
                bool valid = !cut->surfaces.empty();
                for ( const auto& rim : cut->surfaces )
                {
                    if ( rim.vertices.size() < 3 || rim.vertices.size() != rim.texVertices.size()
                        || !std::all_of(rim.vertices.begin(), rim.vertices.end(), [&](const auto index)
                            { return index >= 0 && static_cast<std::size_t>(index) < cut->vertices.size(); }) )
                    {
                        valid = false;
                        continue;
                    }
                    valid = valid && !rim.adjoin;
                    double twiceArea = 0.0;
                    for ( std::size_t index = 0; index < rim.vertices.size(); ++index )
                    {
                        const auto& a = cut->vertices[rim.vertices[index]];
                        const auto& b = cut->vertices[rim.vertices[(index + 1) % rim.vertices.size()]];
                        valid = valid && a.z == 0.0;
                        twiceArea += a.x * b.y - a.y * b.x;
                    }
                    valid = valid && twiceArea > 0.0;
                    area += 0.5 * twiceArea;
                }
                // Triangle:2 - (1.5*1.5/2). Reflex square:(1-.0000025)-.8*.8.
                const double expected = reflex ? 0.3599975 : 0.875;
                check(valid && std::abs(area - expected) <= 64.0 * std::numeric_limits<double>::epsilon(),
                    "extrude.sourceCutFootprint: visible pieces cover the independent original-minus-opening area without hidden portal fragments");
            }
        }
        for ( const double skew : {0.0, 5.0e-10} )
        {
            sed::Level level;
            auto& sector = level.newSector();
            sector.vertices = {{0,0,0}, {2,0,0}, {2-skew,2,0}, {0,2,0}};
            sed::Surface face;
            face.vertices = {0,1,2,3};
            face.texVertices.resize(4);
            face.normal = {0,0,1};
            sector.addSurface(face);
            const auto before = level;
            sed::level::SurfaceExtrudeReport report;
            const auto cut = sed::level::levelBuildSurfaceExtrudeSourceCutPreview(level, {0,0}, -0.25, &report);
            check(cut && report.surface == sed::SurfaceRef{0,0}
                    && report.error == sed::level::SurfaceExtrudeError::None
                    && sed::assets::sedLevelsEqual(level, before),
                "extrude.sourceCutPrecision: rectangle and subnanometer trapezoid retain their actual boundary in a truthful rim preview");
            if ( cut )
            {
                bool exactBoundary = false;
                bool contained = true;
                double area = 0.0;
                for ( const auto& rim : cut->surfaces )
                {
                    if ( rim.vertices.size() < 3 || !std::all_of(rim.vertices.begin(), rim.vertices.end(), [&](const auto index)
                        { return index >= 0 && static_cast<std::size_t>(index) < cut->vertices.size(); }) )
                    {
                        contained = false;
                        continue;
                    }
                    for ( std::size_t corner = 0; corner < rim.vertices.size(); ++corner )
                    {
                        const auto& a = cut->vertices[rim.vertices[corner]];
                        const auto& b = cut->vertices[rim.vertices[(corner + 1) % rim.vertices.size()]];
                        exactBoundary = exactBoundary || (a.x == 2.0 - skew && a.y == 2.0 && a.z == 0.0);
                        contained = contained && a.x <= 2.0 - 0.5 * skew * a.y + 16.0 * std::numeric_limits<double>::epsilon();
                        area += 0.5 * (a.x * b.y - a.y * b.x);
                    }
                }
                check(exactBoundary && contained && std::abs(area - (1.75 - skew)) <= 64.0 * std::numeric_limits<double>::epsilon(),
                    "extrude.sourceCutPrecision: exact authored skew corner and independent trapezoid-minus-opening area survive preview");
            }
            if ( skew == 0.0 )
            {
                const auto fineCut = sed::level::levelBuildSurfaceExtrudeSourceCutPreview(level, {0,0}, -1.0e-10, &report);
                check(fineCut && fineCut->surfaces.size() == 4 && report.error == sed::level::SurfaceExtrudeError::None
                        && report.surface == sed::SurfaceRef{0,0} && sed::assets::sedLevelsEqual(level, before),
                    "extrude.sourceCutPrecision: a representable tiny negative margin creates an exact visible inset preview");
            }
        }
        for ( const double edge : {1.0e-10, 1.0e-20} )
        {
            sed::Level level;
            auto& sector = level.newSector();
            sector.vertices = {{0,0,0}, {edge,0,0}, {edge,edge,0}, {0,edge,0}};
            sed::Surface face;
            face.vertices = {0,1,2,3};
            face.texVertices.resize(4);
            face.normal = {0,0,1};
            sector.addSurface(face);
            sed::level::SurfaceExtrudeReport report;
            const auto bounds = sed::level::levelSurfaceExtrudeExpandBounds(level, {0,0}, &report);
            check(bounds && bounds->minSpan == edge && bounds->negativeCollapseLimit == -0.5 * edge
                    && report.error == sed::level::SurfaceExtrudeError::None && report.surface == sed::SurfaceRef{0,0},
                "extrude.expandBoundsPrecision: nonzero microscopic geometry retains exact scale-covariant bounds");
        }
        for ( const double expand : {0.0, 1.0e-10, -1.0e-10} )
        {
            sed::Level level;
            auto& sector = level.newSector();
            sector.vertices = {{0,0,0}, {2,0,0}, {0,2,0}};
            sed::Surface face;
            face.vertices = {0,1,2};
            face.texVertices.resize(3);
            face.texVertices[0].u = 0.0f;
            face.texVertices[0].v = 0.0f;
            face.texVertices[1].u = 0.5f;
            face.texVertices[1].v = 0.0f;
            face.texVertices[2].u = 0.0f;
            face.texVertices[2].v = 1.0f;
            for ( auto& tuple : face.texVertices )
            {
                tuple.color = {0.25f, 0.5f, 0.75f, 1.0f};
            }
            face.normal = {0,0,1};
            sector.addSurface(face);
            const auto before = level;
            sed::level::SurfaceExtrudeReport report;
            const auto wire = sed::level::levelBuildSurfaceExtrudePreview(level, {0,0}, 0.125, expand, &report);
            const bool rectangular = expand != 0.0;
            check(wire && wire->innerLoop.size() == (rectangular ? 4 : 3)
                    && report.error == sed::level::SurfaceExtrudeError::None && report.surface == sed::SurfaceRef{0,0}
                    && sed::assets::sedLevelsEqual(level, before),
                "extrude.tinyExpand: every exact nonzero value selects legacy rectangle while zero retains the triangle");
            const auto solid = sed::level::levelBuildSurfaceExtrudeSectorPreview(level, {0,0}, 0.125, expand, &report);
            const bool applied = sed::level::levelExtrudeSurface(level, {0,0}, 0.125, expand, &report);
            bool exact = applied && solid && wire && report.error == sed::level::SurfaceExtrudeError::None
                && report.surface == sed::SurfaceRef{0,0};
            if ( exact && rectangular )
            {
                const std::array<sed::TVector, 4> inner{{{-expand,-expand,0}, {-expand,2+expand,0},
                    {2+expand,2+expand,0}, {2+expand,-expand,0}}};
                exact = wire->innerLoop.size() == inner.size();
                for ( std::size_t corner = 0; exact && corner < inner.size(); ++corner )
                {
                    const auto& actual = wire->innerLoop[corner];
                    exact = actual.x == inner[corner].x && actual.y == inner[corner].y && actual.z == inner[corner].z;
                }
            }
            if ( exact )
            {
                exact = solid->vertices.size() == level.sectors[1].vertices.size();
                for ( std::size_t index = 0; exact && index < solid->vertices.size(); ++index )
                {
                    const auto& a = solid->vertices[index];
                    const auto& b = level.sectors[1].vertices[index];
                    exact = a.x == b.x && a.y == b.y && a.z == b.z;
                }
            }
            check(exact,
                "extrude.tinyExpand: fixed signed rectangle margin is preserved exactly through wire solid and commit");
            if ( applied && expand < 0.0 )
            {
                check(level.sectors.size() == 2,
                    "extrude.tinyInset: accepted transaction contains source and generated sectors");
                if ( level.sectors.size() != 2 )
                {
                    continue;
                }
                const auto& source = level.sectors[0];
                bool originalsUnchanged = source.vertices.size() >= 3;
                for ( std::size_t index = 0; originalsUnchanged && index < 3; ++index )
                {
                    const auto& old = before.sectors[0].vertices[index];
                    const auto& current = source.vertices[index];
                    originalsUnchanged = old.x == current.x && old.y == current.y && old.z == current.z;
                }
                for ( const auto& part : source.surfaces )
                {
                    originalsUnchanged = originalsUnchanged && part.vertices.size() == part.texVertices.size();
                    if ( part.vertices.size() != part.texVertices.size() )
                    {
                        continue;
                    }
                    for ( std::size_t corner = 0; corner < part.vertices.size(); ++corner )
                    {
                        const auto index = part.vertices[corner];
                        if ( index >= 0 && index < 3 )
                        {
                            originalsUnchanged = originalsUnchanged && sameTextureVertex(part.texVertices[corner],
                                before.sectors[0].surfaces[0].texVertices[static_cast<std::size_t>(index)]);
                        }
                    }
                }
                check(originalsUnchanged,
                    "extrude.tinyInset: original source coordinates and surviving UV/RGBA tuples remain exact");
                struct FixedCorner
                {
                    TVector position;
                    float u;
                    float v;
                };
                const std::array<FixedCorner, 3> expected{{
                    {{1e-10,1e-10,0}, 2.5e-11f,5e-11f},
                    {{1e-10,1.9999999999,0}, 2.5e-11f,1.0f},
                    {{1.9999999999,1e-10,0}, 0.5f,5e-11f}}};
                for ( const auto& wanted : expected )
                {
                    bool found = false;
                    bool correct = true;
                    for ( const auto& part : source.surfaces )
                    {
                        if ( !part.adjoin || part.adjoin->sector != 1 || part.vertices.size() != part.texVertices.size() )
                        {
                            continue;
                        }
                        for ( std::size_t corner = 0; corner < part.vertices.size(); ++corner )
                        {
                            const auto index = part.vertices[corner];
                            if ( index < 0 || static_cast<std::size_t>(index) >= source.vertices.size() )
                            {
                                correct = false;
                                continue;
                            }
                            const auto& point = source.vertices[index];
                            if ( point.x != wanted.position.x || point.y != wanted.position.y || point.z != wanted.position.z )
                            {
                                continue;
                            }
                            found = true;
                            const auto& tuple = part.texVertices[corner];
                            // Four float roundoffs cover the two edge interpolation
                            // stages and final float storage; the near-zero vectors
                            // still distinguish a lost tiny UV from a valid result.
                            constexpr float rounding = 4.0f * std::numeric_limits<float>::epsilon();
                            correct = correct && std::abs(tuple.u - wanted.u) <= rounding * std::abs(wanted.u)
                                && std::abs(tuple.v - wanted.v) <= rounding * std::abs(wanted.v)
                                && tuple.color == ColorF{0.25f,0.5f,0.75f,1.0f};
                        }
                    }
                    check(found && correct,
                        "extrude.tinyInset: fixed portal corner UV and RGBA vectors survive rectangle-junction canonicalization");
                }
                bool sidePlanes = level.sectors.size() == 2 && level.sectors[1].surfaces.size() >= 6;
                if ( sidePlanes )
                {
                    const auto& generated = level.sectors[1];
                    for ( std::size_t side = 2; side < 6; ++side )
                    {
                        const auto& part = generated.surfaces[side];
                        sidePlanes = sidePlanes && part.vertices.size() >= 3
                            && sed::level::surfaceIsPlanar(generated, part) && sed::level::surfaceIsConvex(generated, part);
                        for ( const auto index : part.vertices )
                        {
                            if ( index < 0 || static_cast<std::size_t>(index) >= generated.vertices.size() )
                            {
                                sidePlanes = false;
                                continue;
                            }
                            const auto& point = generated.vertices[index];
                            sidePlanes = sidePlanes && (side == 2 ? point.y == 1e-10 : side == 3 ? point.x == 1e-10
                                : side == 4 ? point.y == 1.9999999999 : point.x == 1.9999999999);
                        }
                    }
                }
                check(sidePlanes,
                    "extrude.tinyInset: all new side vertices stay exactly on the four independent rectangle planes");
            }
        }
        for ( const double origin : {0.0, 10000.0} )
        {
            // The tiny inward corner is inherited from a rounded input loop.
            // Its prism must preserve that exact footprint, never flatten or
            // enlarge it while producing independently planar side quads.
            sed::Level level;
            auto& source = level.newSector();
            const std::array<std::array<double, 2>, 6> footprint{{
                {0,0}, {1,0}, {1,0.5}, {0.99999,0.5}, {1,1}, {0,1}}};
            sed::Surface face;
            face.normal = {0,0,1};
            for ( const auto& point : footprint )
            {
                face.vertices.push_back(source.newVertex(origin + point[0], origin + point[1], origin));
                face.texVertices.push_back({});
            }
            source.addSurface(std::move(face));
            const auto before = level;
            sed::level::SurfaceExtrudeReport report;
            const auto preview = sed::level::levelBuildSurfaceExtrudeSectorPreview(level, {0,0}, 0.125, 0.0, &report);
            check(preview && report.error == sed::level::SurfaceExtrudeError::None
                    && report.surface == sed::SurfaceRef{0,0} && sed::assets::sedLevelsEqual(level, before),
                "extrude.inheritedFootprint: bounded original reflex corner permits immutable plain solid preview");
            const bool applied = sed::level::levelExtrudeSurface(level, {0,0}, 0.125, 0.0, &report);
            check(applied && report.error == sed::level::SurfaceExtrudeError::None
                    && report.surface == sed::SurfaceRef{0,0} && level.sectors.size() == 2,
                "extrude.inheritedFootprint: exact plain prism publishes without rejecting inherited corner rounding");
            if ( applied )
            {
                const auto& generated = level.sectors[1];
                bool exact = generated.vertices.size() == 2 * footprint.size() && closed(generated);
                for ( std::size_t index = 0; exact && index < footprint.size(); ++index )
                {
                    const auto& inner = generated.vertices[footprint.size() - 1 - index];
                    const auto& outer = generated.vertices[footprint.size() + index];
                    const auto& original = before.sectors[0].vertices[index];
                    exact = inner.x == original.x && inner.y == original.y && inner.z == original.z
                        && outer.x == original.x && outer.y == original.y && outer.z == origin - 0.125;
                }
                for ( std::size_t index = 2; exact && index < generated.surfaces.size(); ++index )
                {
                    exact = sed::level::surfaceIsPlanar(generated, generated.surfaces[index])
                        && sed::level::surfaceIsConvex(generated, generated.surfaces[index]);
                }
                check(exact,
                    "extrude.inheritedFootprint: all corners retain exact XY footprint and fixed Z translation with strict new side faces");
            }
        }
        {
            using Error = sed::level::SurfaceExtrudeError;
            for ( const int mode : {0,1,2,3,4} )
            {
                auto level = roundedFixture();
                sed::SurfaceRef ref{0,0};
                if ( mode == 0 )
                {
                    ref = {99,4};
                }
                else if ( mode == 1 )
                {
                    ref = {-1,0};
                }
                else if ( mode == 2 )
                {
                    level.sectors[0].surfaces[0].texVertices[0].u = std::numeric_limits<float>::quiet_NaN();
                }
                else if ( mode == 3 )
                {
                    level.sectors[0].surfaces[0].normal.x = std::numeric_limits<double>::infinity();
                }
                else
                {
                    level.sectors[0].surfaces[0].adjoin = sed::SurfaceRef{1,0};
                }
                const Error expected = mode < 2 ? Error::InvalidSurface
                    : mode == 4 ? Error::AlreadyAdjoined : Error::InvalidSourceTopology;
                const auto before = level;
                const auto verify = [&](auto call)
                {
                    sed::level::SurfaceExtrudeReport report{Error::InvalidAdjoin, {-7,-8}};
                    const bool accepted = call(&report);
                    check(!accepted && report.error == expected && report.surface == ref,
                        "extrude.report: every public entry rejects malformed source and replaces the complete OUT sentinel");
                };
                verify([&](auto* out) { return bool(sed::level::levelBuildSurfaceExtrudePreview(level,ref,0.1,0.0,out)); });
                verify([&](auto* out) { return bool(sed::level::levelBuildSurfaceExtrudeSectorPreview(level,ref,0.1,0.0,out)); });
                verify([&](auto* out) { return bool(sed::level::levelBuildSurfaceExtrudeSourceCutPreview(level,ref,-0.1,out)); });
                verify([&](auto* out) { return bool(sed::level::levelSurfaceExtrudeExpandBounds(level,ref,out)); });
                verify([&](auto* out) { return sed::level::levelExtrudeSurface(level,ref,0.1,0.0,out); });
                // The native writer rejects nonfinite scalars. Its direct
                // semantic comparator checks every scalar's IEEE bits without
                // attempting to serialize malformed input.
                check(sed::assets::sedLevelsEqual(level, before),
                    "extrude.report: malformed-source rejection from all APIs preserves the exact Level");
            }
            for ( const double invalid : {0.0, -0.1, std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::quiet_NaN()} )
            {
                auto level = roundedFixture();
                const auto before = sed::assets::saveToSedBytes(level);
                sed::level::SurfaceExtrudeReport report;
                check(!sed::level::levelBuildSurfaceExtrudePreview(level,{0,0},invalid,0.0,&report)
                        && report.error == Error::InvalidLength && report.surface == sed::SurfaceRef{0,0},
                    "extrude.report: wire rejects zero negative and nonfinite lengths with exact OUT");
                check(!sed::level::levelBuildSurfaceExtrudeSectorPreview(level,{0,0},invalid,0.0,&report)
                        && report.error == Error::InvalidLength && report.surface == sed::SurfaceRef{0,0},
                    "extrude.report: solid rejects zero negative and nonfinite lengths with exact OUT");
                check(!sed::level::levelExtrudeSurface(level,{0,0},invalid,0.0,&report)
                        && report.error == Error::InvalidLength && report.surface == sed::SurfaceRef{0,0}
                        && sed::assets::saveToSedBytes(level) == before,
                    "extrude.report: commit rejects invalid length without mutation and returns exact OUT");
            }
            for ( const double invalid : {std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::quiet_NaN(), -1.0} )
            {
                auto level = roundedFixture();
                const auto before = sed::assets::saveToSedBytes(level);
                const Error expected = std::isfinite(invalid) ? Error::CollapsedExpand : Error::InvalidExpand;
                const auto verify = [&](auto call)
                {
                    sed::level::SurfaceExtrudeReport report{Error::InvalidAdjoin, {-7,-8}};
                    check(!call(&report) && report.error == expected && report.surface == sed::SurfaceRef{0,0},
                        "extrude.report: nonfinite or collapsed Expand returns its exact distinct reason and source");
                };
                verify([&](auto* out) { return bool(sed::level::levelBuildSurfaceExtrudePreview(level,{0,0},0.1,invalid,out)); });
                verify([&](auto* out) { return bool(sed::level::levelBuildSurfaceExtrudeSectorPreview(level,{0,0},0.1,invalid,out)); });
                verify([&](auto* out) { return bool(sed::level::levelBuildSurfaceExtrudeSourceCutPreview(level,{0,0},invalid,out)); });
                verify([&](auto* out) { return sed::level::levelExtrudeSurface(level,{0,0},0.1,invalid,out); });
                check(sed::assets::saveToSedBytes(level) == before,
                    "extrude.report: all invalid Expand paths preserve complete input geometry and metadata");
            }
        }
        {
            auto thin = roundedFixture();
            for ( auto& sector : thin.sectors )
            {
                for ( auto& point : sector.vertices )
                {
                    point.x *= 2.0;
                    point.y *= 0.000001;
                }
            }
            thin.sectors[0].vertices[2].z = 0.0000001;
            const auto before = sed::assets::saveToSedBytes(thin);
            sed::level::SurfaceExtrudeReport report;
            check(!sed::level::levelExtrudeSurface(thin, {0,0}, 0.1, 0.0, &report)
                    && report.error == sed::level::SurfaceExtrudeError::NonPlanarSource
                    && report.surface == sed::SurfaceRef{0,0} && sed::assets::saveToSedBytes(thin) == before,
                "extrude.rounded: long thin material warp cannot borrow an allowance from the long axis");
        }
        for ( const double scale : {0.0001, 0.000001} )
        {
            auto tiny = roundedFixture();
            tiny.sectors[0].vertices[2].z = 0.0;
            for ( auto& sector : tiny.sectors )
            {
                for ( auto& point : sector.vertices )
                {
                    point.x *= scale;
                    point.y *= scale;
                    point.z *= scale;
                }
            }
            sed::level::SurfaceExtrudeReport report;
            check(sed::level::levelExtrudeSurface(tiny, {0,0}, 0.1 * scale, 0.0, &report)
                    && report.error == sed::level::SurfaceExtrudeError::None && report.surface == sed::SurfaceRef{0,0},
                "extrude.rounded: scale-covariant malformed admission keeps genuinely planar tiny positive controls editable");
        }
        for ( const double scale : {1.0, 0.0001, 0.000001} )
        for ( const int invalid : {0,1,2} )
        {
            auto level = roundedFixture();
            if ( invalid == 0 )
            {
                level.sectors[0].vertices[2].z = 0.01;
            }
            else if ( invalid == 1 )
            {
                level.sectors[0].vertices[2].x = 0.1;
                level.sectors[0].vertices[2].y = 0.1;
                level.sectors[0].vertices[2].z = 0.0;
            }
            else
            {
                level.sectors[0].surfaces[0].vertices[1] = 99;
            }
            for ( auto& sector : level.sectors )
            {
                for ( auto& point : sector.vertices )
                {
                    point.x *= scale;
                    point.y *= scale;
                    point.z *= scale;
                }
            }
            const auto before = sed::assets::saveToSedBytes(level);
            sed::level::SurfaceExtrudeReport report;
            check(!sed::level::levelExtrudeSurface(level, {0,0}, 0.1 * scale, 0.0, &report)
                    && report.error == (invalid == 0 ? sed::level::SurfaceExtrudeError::NonPlanarSource
                        : invalid == 1 ? sed::level::SurfaceExtrudeError::NonConvexSource
                        : sed::level::SurfaceExtrudeError::InvalidSourceTopology)
                    && report.surface == sed::SurfaceRef{0,0}
                    && sed::assets::saveToSedBytes(level) == before,
                "extrude.rounded: genuine warp concavity and malformed indices reject atomically with exact reason and target");
        }
    }
    for (const double origin : {0.0, 1000000.0})
    {
        const auto fixture = [&]()
        {
            sed::Level level{};
            level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
            auto& sector = level.newSector();
            sector.vertices = {{origin,origin,origin,0},{origin+0.01,origin,origin,0},
                {origin+0.01,origin+0.01,origin,0},{origin,origin+0.01,origin,0}};
            sed::Surface face{}; face.vertices = {0,1,2,3}; face.texVertices.resize(4);
            face.normal = {0,0,1}; sector.addSurface(std::move(face));
            return level;
        };
        const auto closedVolume = [](const sed::Sector& sector)
        {
            std::map<std::pair<std::int32_t,std::int32_t>, std::pair<int,int>> edges;
            long double volume = 0.0L;
            const auto& anchor = sector.vertices.front();
            const auto point = [&](std::int32_t index)
            {
                const auto& vertex = sector.vertices[static_cast<std::size_t>(index)];
                return std::array<long double,3>{static_cast<long double>(vertex.x)-anchor.x,
                    static_cast<long double>(vertex.y)-anchor.y, static_cast<long double>(vertex.z)-anchor.z};
            };
            for (const auto& face : sector.surfaces)
            {
                for (std::size_t edge = 0; edge < face.vertices.size(); ++edge)
                {
                    const auto a = face.vertices[edge], b = face.vertices[(edge+1)%face.vertices.size()];
                    auto& incidence = edges[{std::min(a,b),std::max(a,b)}];
                    ++incidence.first; incidence.second += a < b ? 1 : -1;
                }
                for (std::size_t corner = 1; corner+1 < face.vertices.size(); ++corner)
                {
                    const auto a = point(face.vertices[0]), b = point(face.vertices[corner]), c = point(face.vertices[corner+1]);
                    volume += a[0]*(b[1]*c[2]-b[2]*c[1]) + a[1]*(b[2]*c[0]-b[0]*c[2]) + a[2]*(b[0]*c[1]-b[1]*c[0]);
                }
            }
            for (const auto& [edge, incidence] : edges)
                if (incidence.first != 2 || incidence.second != 0) return -1.0L;
            return std::abs(volume)/6.0L;
        };
        for (const double length : {1.2e-7, 0.000099, 0.0001, 0.000101, 0.01})
        {
            sed::editor::Document document{};
            document.level() = fixture();
            const auto before = sed::assets::saveToSedBytes(document.level());
            const auto wire = sed::level::levelBuildSurfaceExtrudePreview(document.level(), {0,0}, length, 0.0);
            const auto solid = sed::level::levelBuildSurfaceExtrudeSectorPreview(document.level(), {0,0}, length, 0.0);
            const double side = (origin + 0.01) - origin;
            const double thickness = origin - (origin - length);
            const long double expectedVolume = static_cast<long double>(side)*side*thickness;
            check(wire && solid && solid->vertices.size() == 8 && solid->surfaces.size() == 6,
                "extrude.precision: wire and solid preview retain all corners below and around the old weld radius");
            if (solid && solid->vertices.size() == 8)
                check(std::abs(closedVolume(*solid)-expectedVolume) <= expectedVolume*1.0e-10L,
                    "extrude.precision: solid preview is independently closed with analytic nonzero volume");
            const bool applied = document.applyUndoable("Extrude", sed::editor::DocumentDirty::LevelGeometry,
                [&](sed::editor::DocumentState& state) { return sed::level::levelExtrudeSurface(state.level, {0,0}, length); });
            check(applied && document.level().sectors.size() == 2,
                "extrude.precision: valid small extrusion commits");
            if (!applied || document.level().sectors.size() != 2) continue;
            const auto& output = document.level().sectors[1];
            check(output.vertices.size() == 8 && std::abs(closedVolume(output)-expectedVolume) <= expectedVolume*1.0e-10L,
                "extrude.precision: committed output is closed and preserves physical volume");
            bool exactPlanes = true;
            for (const auto& vertex : output.vertices)
                exactPlanes = exactPlanes && (vertex.z == origin || vertex.z == origin-length);
            check(exactPlanes, "extrude.precision: every output corner remains on one analytic extrusion plane");
            const auto after = sed::assets::saveToSedBytes(document.level());
            check(document.undo() && sed::assets::saveToSedBytes(document.level()) == before
                    && document.redo() && sed::assets::saveToSedBytes(document.level()) == after,
                "extrude.precision: exact native Undo and Redo preserve tiny geometry");
        }
        for (const double length : {0.0, -1.0e-7, std::numeric_limits<double>::infinity()})
        {
            auto level = fixture();
            const auto before = sed::assets::saveToSedBytes(level);
            check(!sed::level::levelBuildSurfaceExtrudePreview(level,{0,0},length,0.0)
                    && !sed::level::levelBuildSurfaceExtrudeSectorPreview(level,{0,0},length,0.0)
                    && !sed::level::levelExtrudeSurface(level,{0,0},length)
                    && sed::assets::saveToSedBytes(level) == before,
                "extrude.precision: zero, inward or nonfinite distances cannot publish degenerate output");
        }
        {
            const auto level = fixture();
            const auto patch = sed::level::levelBuildSurfaceExtrudeSourceCutPreview(level, {0,0}, -1.2e-7);
            check(patch && patch->surfaces.size() == 4,
                "extrude.precision: tiny inset preview preserves four distinct visible rim fragments");
            if (patch)
            {
                const double innerMin = origin + 1.2e-7;
                const double innerMax = origin + 0.01 - 1.2e-7;
                const std::array<sed::TVector, 4> innerCorners{{
                    {innerMin,innerMin,origin}, {innerMax,innerMin,origin},
                    {innerMax,innerMax,origin}, {innerMin,innerMax,origin}}};
                const double coordinateError = 8.0 * std::numeric_limits<double>::epsilon() * std::max(std::abs(origin), 0.01);
                bool allInnerCorners = true;
                for ( const auto& expected : innerCorners )
                {
                    bool found = false;
                    for ( const auto& face : patch->surfaces )
                    {
                        for ( const auto index : face.vertices )
                        {
                            if ( index >= 0 && static_cast<std::size_t>(index) < patch->vertices.size() )
                            {
                                const auto& point = patch->vertices[index];
                                found = found || (std::abs(point.x - expected.x) <= coordinateError
                                    && std::abs(point.y - expected.y) <= coordinateError && point.z == expected.z);
                            }
                        }
                    }
                    allInnerCorners = allInnerCorners && found;
                }
                check(allInnerCorners,
                    "extrude.precision: four analytic inset corners remain distinct within translated double arithmetic error");
                for (const auto& face : patch->surfaces)
                {
                    const auto& a = patch->vertices[static_cast<std::size_t>(face.vertices[0])];
                    long double twiceArea = 0.0L;
                    for (std::size_t corner = 1; corner+1 < face.vertices.size(); ++corner)
                    {
                        const auto& b = patch->vertices[static_cast<std::size_t>(face.vertices[corner])];
                        const auto& c = patch->vertices[static_cast<std::size_t>(face.vertices[corner+1])];
                        twiceArea += (static_cast<long double>(b.x)-a.x)*(static_cast<long double>(c.y)-a.y)
                            -(static_cast<long double>(b.y)-a.y)*(static_cast<long double>(c.x)-a.x);
                    }
                    check(std::abs(twiceArea) > 0.0L,
                        "extrude.precision: every inset preview rim has independently nonzero physical area");
                }
            }
        }
        {
            sed::Level cubeLevel{};
            constexpr double side = 0.000099;
            const auto& cube = sed::level::levelCreateCube(cubeLevel, origin, origin, origin,
                sed::TVector::up, {side,0,0});
            const double actualSide = (origin + side)-origin;
            const long double volume = static_cast<long double>(actualSide)*actualSide*actualSide;
            check(cube.vertices.size() == 8 && cube.surfaces.size() == 6
                    && std::abs(closedVolume(cube)-volume) <= volume*1.0e-10L,
                "cube.precision: generated cube below old weld radius remains a closed analytic solid");
        }
        {
            sed::Level level{};
            const auto& source = sed::level::levelCreateCube(level, origin, origin, origin,
                sed::TVector::up, {0.01,0,0});
            const long double sourceVolume = closedVolume(source);
            check(sed::level::levelExtrudeSurface(level, {0,0}, 1.2e-7),
                "extrude.precision: tiny extrusion accepts a complete convex source sector");
            if (level.sectors.size() == 2)
            {
                check(closedVolume(level.sectors[0]) == sourceVolume
                        && closedVolume(level.sectors[1]) > 0.0L,
                    "extrude.precision: both original and output retain independently closed positive volumes");
                const auto portal = level.sectors[0].surfaces[0].adjoin;
                const bool validPortal = portal && portal->sector == 1 && portal->surface >= 0
                    && static_cast<std::size_t>(portal->surface) < level.sectors[1].surfaces.size();
                const auto* mirror = validPortal ? &level.sectors[1].surfaces[static_cast<std::size_t>(portal->surface)] : nullptr;
                check(mirror && mirror->adjoin == std::optional<sed::SurfaceRef>{sed::SurfaceRef{0,0}}
                        && sed::level::surfaceDoOverlap(level.sectors[0], level.sectors[0].surfaces[0],
                            level.sectors[static_cast<std::size_t>(portal->sector)], *mirror),
                    "extrude.precision: original and tiny output portal remain reciprocal and coincident");
            }
        }
    }
    {
        sed::Level level{};
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        sed::Sector& sector = level.newSector();
        sector.addVertex(0.0, 0.0, 0.0);
        sector.addVertex(1.0, 0.0, 0.0);
        sector.addVertex(1.0, 1.0, 0.0);
        sector.addVertex(0.0, 1.0, 0.0);
        sed::Surface surface{};
        surface.vertices = { 0, 1, 2, 3 };
        surface.texVertices.resize(4);
        surface.uScale = 1.0f / 128.0f;
        surface.vScale = 1.0f / 64.0f;
        surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);
        sector.addSurface(std::move(surface));
        level.uvCoordinateSpace = static_cast<sed::UvCoordinateSpace>(99);

        const std::size_t sectorCountBefore = level.sectors.size();
        const std::vector<sed::TexVertex> textureVerticesBefore =
            level.sectors[0].surfaces[0].texVertices;
        const auto preview = sed::level::levelBuildSurfaceExtrudePreview(
            level,
            sed::SurfaceRef{ 0, 0 },
            1.0,
            0.0);
        const auto sectorPreview =
            sed::level::levelBuildSurfaceExtrudeSectorPreview(
                level,
                sed::SurfaceRef{ 0, 0 },
                1.0,
                0.0);
        const auto sourceCutPreview =
            sed::level::levelBuildSurfaceExtrudeSourceCutPreview(
                level,
                sed::SurfaceRef{ 0, 0 },
                -0.1);
        const bool result = sed::level::levelExtrudeSurface(
            level, sed::SurfaceRef{ 0, 0 }, 1.0);
        check(!preview
                && !sectorPreview
                && !sourceCutPreview
                && !result
                && level.sectors.size() == sectorCountBefore
                && level.uvCoordinateSpace
                    == static_cast<sed::UvCoordinateSpace>(99)
                && sameTextureVertices(
                    level.sectors[0].surfaces[0].texVertices,
                    textureVerticesBefore),
            "extrudeSurface.invalidUvSpace withholds every preview and rejects an unknown Level UV tag atomically");
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;

        sed::Sector& sector = level.newSector();
        sector.addVertex(0.0, 0.0, 0.0);
        sector.addVertex(1.0, 0.0, 0.0);
        sector.addVertex(1.0, 1.0, 0.0);
        sector.addVertex(0.0, 1.0, 0.0);

        sed::Surface surface{};
        surface.material    = "test.mat";
        surface.geo         = static_cast<std::int32_t>(sed::TGeoMode::Texture);
        surface.vertices    = { 0, 1, 2, 3 };
        surface.texVertices = std::vector<sed::TexVertex>(surface.vertices.size());
        surface.uScale      = 1.0f / 128.0f;
        surface.vScale      = 1.0f / 64.0f;

        sector.addSurface(std::move(surface));

        sed::level::surfaceRecalcNewAll(sed::level::levelPixelPerUnit(level), sector, sector.surfaces[0], 0);
        sector.surfaces[0].texVertices = {
            { -2.25f, 3.5f, sed::ColorF{ 0.1f, 0.2f, 0.3f, 0.4f } },
            { 5.75f, -7.25f, sed::ColorF{ 0.2f, 0.3f, 0.4f, 0.5f } },
            { 11.5f, 13.75f, sed::ColorF{ 0.3f, 0.4f, 0.5f, 0.6f } },
            { -17.0f, 19.25f, sed::ColorF{ 0.4f, 0.5f, 0.6f, 0.7f } },
        };
        const std::vector<sed::TexVertex> sourceTextureVertices =
            sector.surfaces[0].texVertices;

        const bool ok = sed::level::levelExtrudeSurface(level, sed::SurfaceRef{ 0, 0 }, 1.0);
        check(ok, "extrudeSurface: returns true");
        check(level.uvCoordinateSpace == sed::UvCoordinateSpace::Normalized,
            "extrudeSurface.normalized: preserves the canonical Level UV tag");

        check(level.sectors.size() == 2, "extrudeSurface: sector count");

        const sed::Surface& orig = level.sectors[0].surfaces[0];
        check(sameTextureVertices(orig.texVertices, sourceTextureVertices)
                && orig.uScale == 1.0f / 128.0f
                && orig.vScale == 1.0f / 64.0f,
            "extrudeSurface.normalized: source signed multi-tile UVs, colors, and anisotropic scales remain exact");
        check(orig.adjoin.has_value(), "extrudeSurface: orig.adjoin");
        if (orig.adjoin)
        {
            check(orig.adjoin->sector == 1, "extrudeSurface: orig.adjoin.sector");
            check(orig.adjoin->surface == 0, "extrudeSurface: orig.adjoin.surface");
        }

        check(orig.surfaceFlags == 0x0, "extrudeSurface: orig.surfaceFlags reset");
        check(orig.faceFlags == 0x0u, "extrudeSurface: orig.faceFlags reset");
        check(orig.geo == 0, "extrudeSurface: orig.geo reset");

        check((orig.adjoinFlags & sed::SAF_Visible) != 0, "extrudeSurface: orig.SAF_Visible");
        check((orig.adjoinFlags & sed::SAF_Move) != 0, "extrudeSurface: orig.SAF_Move");
        check((orig.adjoinFlags & sed::SAF_AllowSoundPass) != 0, "extrudeSurface: orig.SAF_AllowSoundPass");

        const sed::Sector& extruded = level.sectors[1];
        check(extruded.vertices.size() == 8, "extrudeSurface: new vertices.size");
        check(extruded.surfaces.size() == 6, "extrudeSurface: new surfaces.size");

        auto vx = [&](const std::size_t i) -> sed::TVector
        {
            const sed::Vertex& v = extruded.vertices[i];
            return sed::TVector{ v.x, v.y, v.z };
        };

        check(closeVec(vx(0), sed::TVector{ 0.0, 1.0, 0.0 }), "extrudeSurface: v0");
        check(closeVec(vx(1), sed::TVector{ 1.0, 1.0, 0.0 }), "extrudeSurface: v1");
        check(closeVec(vx(2), sed::TVector{ 1.0, 0.0, 0.0 }), "extrudeSurface: v2");
        check(closeVec(vx(3), sed::TVector{ 0.0, 0.0, 0.0 }), "extrudeSurface: v3");

        check(closeVec(vx(4), sed::TVector{ 0.0, 0.0, -1.0 }), "extrudeSurface: v4");
        check(closeVec(vx(5), sed::TVector{ 1.0, 0.0, -1.0 }), "extrudeSurface: v5");
        check(closeVec(vx(6), sed::TVector{ 1.0, 1.0, -1.0 }), "extrudeSurface: v6");
        check(closeVec(vx(7), sed::TVector{ 0.0, 1.0, -1.0 }), "extrudeSurface: v7");

        if (extruded.surfaces.size() >= 2)
        {
            const sed::Surface& asurf = extruded.surfaces[0];
            const sed::Surface& osurf = extruded.surfaces[1];

            check(asurf.adjoin.has_value(), "extrudeSurface: asurf.adjoin");
            if (asurf.adjoin)
            {
                check(asurf.adjoin->sector == 0, "extrudeSurface: asurf.adjoin.sector");
                check(asurf.adjoin->surface == 0, "extrudeSurface: asurf.adjoin.surface");
            }

            check(asurf.vertices.size() == 4, "extrudeSurface: asurf.vertices.size");
            check(asurf.vertices.size() == asurf.texVertices.size(), "extrudeSurface: asurf.texVertices.size");
            if (asurf.vertices.size() == 4)
            {
                check(asurf.vertices[0] == 0, "extrudeSurface: asurf.v0");
                check(asurf.vertices[1] == 1, "extrudeSurface: asurf.v1");
                check(asurf.vertices[2] == 2, "extrudeSurface: asurf.v2");
                check(asurf.vertices[3] == 3, "extrudeSurface: asurf.v3");
            }

            check(asurf.surfaceFlags == 0x0, "extrudeSurface: asurf.surfaceFlags reset");
            check(asurf.faceFlags == 0x0u, "extrudeSurface: asurf.faceFlags reset");
            check(asurf.geo == 0, "extrudeSurface: asurf.geo reset");

            check(closeVec(asurf.normal, sed::TVector::down, kRotationEpsilon), "extrudeSurface: asurf.normal");
            check(asurf.material == "test.mat", "extrudeSurface: asurf.material");

            check(!osurf.adjoin.has_value(), "extrudeSurface: osurf.adjoin");
            check(osurf.vertices.size() == 4, "extrudeSurface: osurf.vertices.size");
            if (osurf.vertices.size() == 4)
            {
                check(osurf.vertices[0] == 4, "extrudeSurface: osurf.v0");
                check(osurf.vertices[1] == 5, "extrudeSurface: osurf.v1");
                check(osurf.vertices[2] == 6, "extrudeSurface: osurf.v2");
                check(osurf.vertices[3] == 7, "extrudeSurface: osurf.v3");
            }

            check(closeVec(osurf.normal, sed::TVector::up, kRotationEpsilon), "extrudeSurface: osurf.normal");
            check((osurf.surfaceFlags & sed::SF_Floor) != 0, "extrudeSurface: osurf.SF_Floor");
            check((osurf.surfaceFlags & sed::SF_Collision) != 0, "extrudeSurface: osurf.SF_Collision");
            check(osurf.material == "test.mat", "extrudeSurface: osurf.material");
        }

        for (std::size_t i = 0; i < extruded.surfaces.size(); ++i)
        {
            const sed::Surface& s = extruded.surfaces[i];
            check(s.vertices.size() == s.texVertices.size(), "extrudeSurface: texVertex count match");
            check(close(s.uScale, 1.0 / 128.0)
                    && close(s.vScale, 1.0 / 64.0),
                "extrudeSurface.normalized: every generated face retains per-axis normalized texture scale");
            for (const sed::TexVertex& texVertex : s.texVertices)
            {
                check(std::isfinite(texVertex.u)
                        && std::isfinite(texVertex.v),
                    "extrudeSurface.normalized: every generated UV is finite");
            }
        }
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;
        level.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;

        sed::Sector& sector = level.newSector();
        sector.addVertex(0.0, 0.0, 0.0);
        sector.addVertex(2.0, 0.0, 0.0);
        sector.addVertex(2.0, 2.0, 0.0);
        sector.addVertex(0.0, 2.0, 0.0);

        sed::Surface surface{};
        surface.material = "legacy.mat";
        surface.geo = static_cast<std::int32_t>(sed::TGeoMode::Texture);
        surface.vertices = { 0, 1, 2, 3 };
        surface.texVertices = {
            { -96.0f, 224.0f, sed::ColorRed },
            { 288.0f, -160.0f, sed::ColorGreen },
            { 544.0f, 352.0f, sed::ColorBlue },
            { -224.0f, 480.0f, sed::ColorWhite },
        };
        surface.uScale = -16.0f;
        surface.vScale = 32.0f;
        surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);
        sector.addSurface(surface);

        const bool ok = sed::level::levelExtrudeSurface(
            level, sed::SurfaceRef{ 0, 0 }, 1.0);
        check(ok,
            "extrudeSurface.pixelCompatibility: explicit legacy Level extrudes");
        check(level.uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels
                && level.sectors.size() == 2
                && sameTextureVertices(
                    level.sectors[0].surfaces[0].texVertices,
                    surface.texVertices)
                && level.sectors[0].surfaces[0].uScale == surface.uScale
                && level.sectors[0].surfaces[0].vScale == surface.vScale,
            "extrudeSurface.pixelCompatibility: source texture state and pixel-space tag stay exact");

        bool generatedTextureStateValid = level.sectors.size() == 2;
        if ( generatedTextureStateValid )
        {
            for ( const sed::Surface& generated : level.sectors[1].surfaces )
            {
                generatedTextureStateValid = generatedTextureStateValid
                    && generated.vertices.size()
                        == generated.texVertices.size()
                    && generated.uScale == surface.uScale
                    && generated.vScale == surface.vScale;
                for ( const sed::TexVertex& texVertex : generated.texVertices )
                {
                    generatedTextureStateValid = generatedTextureStateValid
                        && std::isfinite(texVertex.u)
                        && std::isfinite(texVertex.v);
                }
            }
        }
        check(generatedTextureStateValid,
            "extrudeSurface.pixelCompatibility: generated faces remain complete and use source pixel-scale metadata");
    }

    {
        constexpr float textureWidth = 128.0f;
        constexpr float textureHeight = 64.0f;

        sed::Level normalized{};
        normalized.clear();
        normalized.kind = sed::ProjectType::JKDF2;
        normalized.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;

        sed::Sector& sector = normalized.newSector();
        sector.addVertex(-1.5, -0.75, 0.0);
        sector.addVertex(2.5, -0.75, 0.0);
        sector.addVertex(2.5, 1.25, 0.0);
        sector.addVertex(-1.5, 1.25, 0.0);

        sed::Surface surface{};
        surface.material = "anisotropic.mat";
        surface.geo = static_cast<std::int32_t>(sed::TGeoMode::Texture);
        surface.vertices = { 0, 1, 2, 3 };
        surface.texVertices = {
            { -2.25f, 3.5f, sed::ColorRed },
            { 4.75f, -1.25f, sed::ColorGreen },
            { 7.5f, 8.75f, sed::ColorBlue },
            { -5.0f, 11.25f, sed::ColorWhite },
        };
        surface.uScale = -0.125f;
        surface.vScale = 0.375f;
        surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);
        sector.addSurface(surface);

        sed::Level pixels = normalized;
        pixels.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;
        sed::Surface& pixelSource = pixels.sectors[0].surfaces[0];
        pixelSource.uScale *= textureWidth;
        pixelSource.vScale *= textureHeight;
        for (sed::TexVertex& texVertex : pixelSource.texVertices)
        {
            texVertex.u *= textureWidth;
            texVertex.v *= textureHeight;
        }

        const sed::Level normalizedSource = normalized;
        const sed::Level pixelSourceLevel = pixels;
        const bool normalizedOk = sed::level::levelExtrudeSurface(
            normalized, sed::SurfaceRef{ 0, 0 }, 1.5);
        const bool pixelsOk = sed::level::levelExtrudeSurface(
            pixels, sed::SurfaceRef{ 0, 0 }, 1.5);
        check(normalizedOk && pixelsOk,
            "extrudeSurface.coordinateSpaces: normalized and pixel-space anisotropic fixtures both extrude");
        check(normalized.sectors.size() == 2
                && pixels.sectors.size() == 2,
            "extrudeSurface.coordinateSpaces: both operations generate exactly one sector");
        check(sameTextureVertices(
                    normalized.sectors[0].surfaces[0].texVertices,
                    normalizedSource.sectors[0].surfaces[0].texVertices)
                && normalized.sectors[0].surfaces[0].uScale
                    == normalizedSource.sectors[0].surfaces[0].uScale
                && normalized.sectors[0].surfaces[0].vScale
                    == normalizedSource.sectors[0].surfaces[0].vScale
                && sameTextureVertices(
                    pixels.sectors[0].surfaces[0].texVertices,
                    pixelSourceLevel.sectors[0].surfaces[0].texVertices)
                && pixels.sectors[0].surfaces[0].uScale
                    == pixelSourceLevel.sectors[0].surfaces[0].uScale
                && pixels.sectors[0].surfaces[0].vScale
                    == pixelSourceLevel.sectors[0].surfaces[0].vScale,
            "extrudeSurface.coordinateSpaces: signed multi-tile source corners and anisotropic scales stay exact");

        bool allGeneratedFacesEquivalent = normalized.sectors.size() == 2
            && pixels.sectors.size() == 2
            && normalized.sectors[1].surfaces.size()
                == pixels.sectors[1].surfaces.size();
        if (allGeneratedFacesEquivalent)
        {
            const sed::Sector& normalizedExtrusion = normalized.sectors[1];
            const sed::Sector& pixelExtrusion = pixels.sectors[1];
            for (std::size_t surfaceIndex = 0;
                 surfaceIndex < normalizedExtrusion.surfaces.size();
                 ++surfaceIndex)
            {
                const sed::Surface& normalizedSurface =
                    normalizedExtrusion.surfaces[surfaceIndex];
                const sed::Surface& pixelSurface =
                    pixelExtrusion.surfaces[surfaceIndex];
                allGeneratedFacesEquivalent = allGeneratedFacesEquivalent
                    && normalizedSurface.vertices == pixelSurface.vertices
                    && normalizedSurface.texVertices.size()
                        == pixelSurface.texVertices.size()
                    && close(
                        normalizedSurface.uScale,
                        pixelSurface.uScale / textureWidth)
                    && close(
                        normalizedSurface.vScale,
                        pixelSurface.vScale / textureHeight);

                for (std::size_t cornerIndex = 0;
                     allGeneratedFacesEquivalent
                     && cornerIndex < normalizedSurface.texVertices.size();
                     ++cornerIndex)
                {
                    const sed::TexVertex& normalizedCorner =
                        normalizedSurface.texVertices[cornerIndex];
                    const sed::TexVertex& pixelCorner =
                        pixelSurface.texVertices[cornerIndex];
                    allGeneratedFacesEquivalent = close(
                            normalizedCorner.u,
                            pixelCorner.u / textureWidth)
                        && close(
                            normalizedCorner.v,
                            pixelCorner.v / textureHeight)
                        && normalizedCorner.color == pixelCorner.color;
                }
            }
        }
        check(allGeneratedFacesEquivalent,
            "extrudeSurface.coordinateSpaces: every generated face, corner, color, and per-axis scale is equivalent");
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;

        sed::Sector& sector = level.newSector();
        sector.addVertex(0.0, 0.0, 0.0);
        sector.addVertex(2.0, 0.0, 0.0);
        sector.addVertex(2.0, 2.0, 0.0);
        sector.addVertex(0.0, 2.0, 0.0);

        sed::Surface surface{};
        surface.material = "late-failure.mat";
        surface.geo = static_cast<std::int32_t>(sed::TGeoMode::Texture);
        surface.vertices = { 0, 1, 2, 3 };
        surface.texVertices = {
            { -1.0f, 2.0f, sed::ColorRed },
            { 3.0f, 2.0f, sed::ColorGreen },
            { 3.0f, 5.0f, sed::ColorBlue },
            { -1.0f, 5.0f, sed::ColorWhite },
        };
        surface.uScale = 0.25f;
        surface.vScale = 0.5f;
        surface.normal = sed::TVector{ 0.0, 0.0, 1.0e-6 };
        sector.addSurface(surface);

        const sed::Level baseline = level;
        sed::level::SurfaceExtrudeReport report;
        const auto wire = sed::level::levelBuildSurfaceExtrudePreview(level, {0,0}, 1.0, 0.25, &report);
        check(wire && report.error == sed::level::SurfaceExtrudeError::None && report.surface == sed::SurfaceRef{0,0}
                && sed::assets::sedLevelsEqual(level, baseline),
            "extrudeSurface.scaledCache: finite same-facing normal scale permits immutable wire preview with complete OUT");
        const auto solid = sed::level::levelBuildSurfaceExtrudeSectorPreview(level, {0,0}, 1.0, 0.25, &report);
        check(solid && report.error == sed::level::SurfaceExtrudeError::None && report.surface == sed::SurfaceRef{0,0}
                && sed::assets::sedLevelsEqual(level, baseline),
            "extrudeSurface.scaledCache: solid preview accepts the same geometry without changing authored source data");
        const bool applied = sed::level::levelExtrudeSurface(level, {0,0}, 1.0, 0.25, &report);
        check(applied && report.error == sed::level::SurfaceExtrudeError::None && report.surface == sed::SurfaceRef{0,0},
            "extrudeSurface.scaledCache: expanded commit uses geometric winding instead of rejecting normal magnitude");
        if ( applied && wire && solid )
        {
            const std::array<sed::TVector, 4> inner{{{-0.25,-0.25,0}, {-0.25,2.25,0}, {2.25,2.25,0}, {2.25,-0.25,0}}};
            const std::array<sed::TVector, 4> outer{{{2.25,-0.25,-1}, {2.25,2.25,-1}, {-0.25,2.25,-1}, {-0.25,-0.25,-1}}};
            bool exact = wire->innerLoop.size() == 4 && wire->outerLoop.size() == 4
                && level.sectors.size() == 2 && solid->surfaces.size() == 10
                && solid->vertices.size() == level.sectors[1].vertices.size();
            for ( std::size_t index = 0; exact && index < 4; ++index )
            {
                exact = wire->innerLoop[index].x == inner[index].x && wire->innerLoop[index].y == inner[index].y
                    && wire->innerLoop[index].z == inner[index].z && wire->outerLoop[index].x == outer[index].x
                    && wire->outerLoop[index].y == outer[index].y && wire->outerLoop[index].z == outer[index].z;
            }
            for ( std::size_t index = 0; exact && index < solid->vertices.size(); ++index )
            {
                const auto& a = solid->vertices[index];
                const auto& b = level.sectors[1].vertices[index];
                exact = a.x == b.x && a.y == b.y && a.z == b.z;
            }
            const auto& retained = level.sectors[0].surfaces[0];
            exact = exact && retained.material == "late-failure.mat" && retained.uScale == 0.25f && retained.vScale == 0.5f
                && retained.vertices == surface.vertices && sameTextureVertices(retained.texVertices, surface.texVertices);
            check(exact,
                "extrudeSurface.scaledCache: fixed rectangular geometry and original complete UV RGBA scales survive preview and publication");
        }
    }

    {
        sed::Level level{};
        level.clear();
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;

        sed::Sector& sector = level.newSector();
        sector.addVertex(0.0, 0.0, 0.0);
        sector.addVertex(1.0, 0.0, 0.0);
        sector.addVertex(1.0, 1.0, 0.0);
        sector.addVertex(0.0, 1.0, 0.0);

        sed::Surface surface{};
        surface.vertices = { 0, 1, 2, 3 };
        surface.texVertices = {
            { -0.25f, 0.5f, sed::ColorRed },
            { 1.75f, -0.5f, sed::ColorGreen },
            { 2.25f, 1.5f, sed::ColorBlue },
        };
        surface.uScale = -0.125f;
        surface.vScale = 0.25f;
        surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);
        sector.addSurface(surface);

        const bool previewRejected =
            !sed::level::levelBuildSurfaceExtrudeSectorPreview(
                level, sed::SurfaceRef{ 0, 0 }, 1.0, 0.0);
        sed::Level commitLevel = level;
        const bool commitRejected = !sed::level::levelExtrudeSurface(
            commitLevel, sed::SurfaceRef{ 0, 0 }, 1.0);
        check(previewRejected && commitRejected,
            "extrudeSurface.incompleteUv: preview and commit reject an incomplete source texture-corner array");
        check(level.sectors.size() == 1
                && level.sectors[0].vertices.size() == 4
                && level.sectors[0].surfaces.size() == 1
                && level.sectors[0].surfaces[0].vertices
                    == surface.vertices
                && sameTextureVertices(
                    level.sectors[0].surfaces[0].texVertices,
                    surface.texVertices)
                && level.sectors[0].surfaces[0].uScale == surface.uScale
                && level.sectors[0].surfaces[0].vScale == surface.vScale,
            "extrudeSurface.incompleteUv: preview rejection leaves geometry and authored texture state unchanged");
        check(commitLevel.sectors.size() == 1
                && commitLevel.sectors[0].vertices.size() == 4
                && commitLevel.sectors[0].surfaces.size() == 1
                && commitLevel.sectors[0].surfaces[0].vertices
                    == surface.vertices
                && sameTextureVertices(
                    commitLevel.sectors[0].surfaces[0].texVertices,
                    surface.texVertices)
                && commitLevel.sectors[0].surfaces[0].uScale
                    == surface.uScale
                && commitLevel.sectors[0].surfaces[0].vScale
                    == surface.vScale,
            "extrudeSurface.incompleteUv: commit rejection is atomic");
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;

        sed::Sector& sector = level.newSector();
        sector.addVertex(0.0, 0.0, 0.0);
        sector.addVertex(2.0, 0.0, 0.0);
        sector.addVertex(2.0, 2.0, 0.0);
        sector.addVertex(0.0, 2.0, 0.0);

        sed::Surface surface{};
        surface.material    = "test.mat";
        surface.geo         = static_cast<std::int32_t>(sed::TGeoMode::Texture);
        surface.vertices    = { 0, 1, 2, 3 };
        surface.texVertices = std::vector<sed::TexVertex>(surface.vertices.size());
        surface.uScale      = 1.0f;
        surface.vScale      = 1.0f;
        sector.addSurface(std::move(surface));
        sed::level::surfaceRecalcNewAll(sed::level::levelPixelPerUnit(level), sector, sector.surfaces[0], 0);

        const std::optional<sed::level::SurfaceExtrudePreview> preview =
            sed::level::levelBuildSurfaceExtrudePreview(level, sed::SurfaceRef{ 0, 0 }, 2.0, 1.0);
        check(preview.has_value(), "extrudeSurface.expand: preview returns geometry");
        if (preview && preview->outerLoop.size() == 4)
        {
            check(closeVec(preview->innerLoop[0], sed::TVector{ -1.0, -1.0, 0.0 }), "extrudeSurface.expand: preview inner v0");
            check(closeVec(preview->innerLoop[2], sed::TVector{ 3.0, 3.0, 0.0 }), "extrudeSurface.expand: preview inner v2");
            check(closeVec(preview->outerLoop[0], sed::TVector{ 3.0, -1.0, -2.0 }), "extrudeSurface.expand: preview outer v0");
            check(closeVec(preview->outerLoop[2], sed::TVector{ -1.0, 3.0, -2.0 }), "extrudeSurface.expand: preview outer v2");
        }

        const std::optional<sed::level::SurfaceExtrudeExpandBounds> expandBounds =
            sed::level::levelSurfaceExtrudeExpandBounds(level, sed::SurfaceRef{ 0, 0 });
        check(expandBounds.has_value(), "extrudeSurface.expandBounds: returns bounds");
        if (expandBounds)
        {
            check(close(expandBounds->negativeCollapseLimit, -1.0),
                "extrudeSurface.expandBounds: negative collapse limit");
            check(close(expandBounds->minSpan, 2.0),
                "extrudeSurface.expandBounds: min span");
        }

        const std::optional<sed::Sector> previewSector =
            sed::level::levelBuildSurfaceExtrudeSectorPreview(level, sed::SurfaceRef{ 0, 0 }, 2.0, 1.0);
        check(previewSector.has_value(), "extrudeSurface.expand: sector preview returns geometry");
        check(level.sectors.size() == 1, "extrudeSurface.expand: sector preview does not mutate level");
        if (previewSector)
        {
            check(previewSector->surfaces.size() == 10,
                "extrudeSurface.expand: sector preview contains four sides outer cap and five inner cap fragments");
            double visibleRimArea = 0.0;
            double hiddenPortalArea = 0.0;
            for ( const auto& face : previewSector->surfaces )
            {
                bool sourcePlane = true;
                double twiceArea = 0.0;
                for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
                {
                    const auto& a = previewSector->vertices[face.vertices[corner]];
                    const auto& b = previewSector->vertices[face.vertices[(corner + 1) % face.vertices.size()]];
                    sourcePlane = sourcePlane && a.z == 0.0;
                    twiceArea += a.x * b.y - a.y * b.x;
                }
                if ( sourcePlane )
                {
                    const double area = 0.5 * std::abs(twiceArea);
                    if ( face.geo == static_cast<std::int32_t>(sed::TGeoMode::NotDrawn) )
                    {
                        hiddenPortalArea += area;
                    }
                    else
                    {
                        visibleRimArea += area;
                    }
                }
            }
            check(visibleRimArea == 12.0 && hiddenPortalArea == 4.0,
                "extrudeSurface.expand: preview shows twelve square units of rim and hides only the four-unit portal");
            check(!previewSector->surfaces.empty()
                    && previewSector->surfaces[0].geo == static_cast<std::int32_t>(sed::TGeoMode::NotDrawn),
                "extrudeSurface.expand: sector preview hides source adjoin face");
            check(previewSector->surfaces.size() > 1
                    && previewSector->surfaces[1].geo == static_cast<std::int32_t>(sed::TGeoMode::Texture),
                "extrudeSurface.expand: sector preview keeps outer face rendered");
        }

        check(!sed::level::levelBuildSurfaceExtrudePreview(level, sed::SurfaceRef{ 0, 0 }, 2.0, -2.0),
            "extrudeSurface.expand: collapsed negative expand rejected");

        const bool ok = sed::level::levelExtrudeSurface(level, sed::SurfaceRef{ 0, 0 }, 2.0, 1.0);
        check(ok, "extrudeSurface.expand: returns true");
        check(!sed::level::levelSurfaceExtrudeExpandBounds(level, sed::SurfaceRef{ 0, 0 }),
            "extrudeSurface.expandBounds: rejects adjoined source surface");
        check(level.sectors.size() == 2, "extrudeSurface.expand: sector count");
        if (level.sectors.size() == 2 && level.sectors[1].vertices.size() >= 7)
        {
            const sed::Sector& extruded = level.sectors[1];
            const sed::Vertex& v4 = extruded.vertices[4];
            const sed::Vertex& v6 = extruded.vertices[6];
            check(closeVec(sed::TVector{ v4.x, v4.y, v4.z }, sed::TVector{ 3.0, -1.0, -2.0 }),
                "extrudeSurface.expand: committed v4");
            check(closeVec(sed::TVector{ v6.x, v6.y, v6.z }, sed::TVector{ -1.0, 3.0, -2.0 }),
                "extrudeSurface.expand: committed v6");
        }
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;

        sed::Sector& sector = level.newSector();
        sector.addVertex(0.0, 0.0, 0.0);
        sector.addVertex(2.0, 0.0, 0.0);
        sector.addVertex(2.0, 2.0, 0.0);
        sector.addVertex(0.0, 2.0, 0.0);

        sed::Surface surface{};
        surface.material    = "test.mat";
        surface.geo         = static_cast<std::int32_t>(sed::TGeoMode::Texture);
        surface.vertices    = { 0, 1, 2, 3 };
        surface.texVertices = std::vector<sed::TexVertex>(surface.vertices.size());
        sector.addSurface(std::move(surface));
        sed::level::surfaceRecalcNewAll(sed::level::levelPixelPerUnit(level), sector, sector.surfaces[0], 0);

        const std::optional<sed::level::SurfaceExtrudePreview> preview =
            sed::level::levelBuildSurfaceExtrudePreview(level, sed::SurfaceRef{ 0, 0 }, 1.0, -0.5);
        check(preview.has_value(), "extrudeSurface.negativeExpand: preview returns inset geometry");
        if (preview && preview->innerLoop.size() == 4 && preview->outerLoop.size() == 4)
        {
            check(closeVec(preview->innerLoop[0], sed::TVector{ 0.5, 0.5, 0.0 }),
                "extrudeSurface.negativeExpand: preview inner v0");
            check(closeVec(preview->innerLoop[2], sed::TVector{ 1.5, 1.5, 0.0 }),
                "extrudeSurface.negativeExpand: preview inner v2");
        }

        const std::optional<sed::Sector> sourceCutPreview =
            sed::level::levelBuildSurfaceExtrudeSourceCutPreview(level, sed::SurfaceRef{ 0, 0 }, -0.5);
        check(sourceCutPreview.has_value(), "extrudeSurface.negativeExpand: source cut preview returns geometry");
        check(level.sectors.size() == 1, "extrudeSurface.negativeExpand: source cut preview does not mutate level");
        if (sourceCutPreview)
        {
            const sed::Sector& cut = *sourceCutPreview;
            check(cut.surfaces.size() == 4, "extrudeSurface.negativeExpand: source cut surface count");
            const std::array<sed::TVector, 8> expectedCorners{{
                {0,0,0}, {2,0,0}, {2,2,0}, {0,2,0},
                {0.5,0.5,0}, {1.5,0.5,0}, {1.5,1.5,0}, {0.5,1.5,0}}};
            bool exactCorners = true;
            double visibleArea = 0.0;
            for ( const auto& expected : expectedCorners )
            {
                bool found = false;
                for ( const auto& rim : cut.surfaces )
                {
                    for ( const auto index : rim.vertices )
                    {
                        if ( index >= 0 && static_cast<std::size_t>(index) < cut.vertices.size() )
                        {
                            const auto& point = cut.vertices[index];
                            found = found || (point.x == expected.x && point.y == expected.y && point.z == expected.z);
                        }
                    }
                }
                exactCorners = exactCorners && found;
            }
            for ( const auto& rim : cut.surfaces )
            {
                if ( rim.vertices.size() < 3 || !std::all_of(rim.vertices.begin(), rim.vertices.end(), [&](const auto index)
                    { return index >= 0 && static_cast<std::size_t>(index) < cut.vertices.size(); }) )
                {
                    exactCorners = false;
                    continue;
                }
                for ( std::size_t corner = 0; corner < rim.vertices.size(); ++corner )
                {
                    const auto& a = cut.vertices[rim.vertices[corner]];
                    const auto& b = cut.vertices[rim.vertices[(corner + 1) % rim.vertices.size()]];
                    visibleArea += 0.5 * (a.x * b.y - a.y * b.x);
                }
            }
            check(exactCorners && visibleArea == 3.0,
                "extrudeSurface.negativeExpand: exact original and opening corners bound three square units of visible source rim");

            for (const sed::Surface& cutSurface : cut.surfaces)
            {
                check(cutSurface.material == "test.mat", "extrudeSurface.negativeExpand: source cut material");
                check(cutSurface.geo == static_cast<std::int32_t>(sed::TGeoMode::Texture),
                    "extrudeSurface.negativeExpand: source cut visible");
                check(closeVec(cutSurface.normal, sed::TVector::up, kRotationEpsilon),
                    "extrudeSurface.negativeExpand: source cut normal");
                check(cutSurface.vertices.size() == cutSurface.texVertices.size(),
                    "extrudeSurface.negativeExpand: source cut texVertex count");
            }
        }

        check(!sed::level::levelBuildSurfaceExtrudeSourceCutPreview(level, sed::SurfaceRef{ 0, 0 }, 0.5),
            "extrudeSurface.negativeExpand: source cut rejects positive expand");
        check(!sed::level::levelBuildSurfaceExtrudeSourceCutPreview(level, sed::SurfaceRef{ 0, 0 }, -1.0),
            "extrudeSurface.negativeExpand: source cut rejects collapsed inset");

        const bool ok = sed::level::levelExtrudeSurface(level, sed::SurfaceRef{ 0, 0 }, 1.0, -0.5);
        check(ok, "extrudeSurface.negativeExpand: returns true");

        bool foundExtrudableGeneratedSurface = false;
        for (std::int32_t sectorIndex = 1; sectorIndex < static_cast<std::int32_t>(level.sectors.size()); ++sectorIndex)
        {
            const sed::Sector& generatedSector = level.sectors[static_cast<std::size_t>(sectorIndex)];
            for (std::int32_t surfaceIndex = 0; surfaceIndex < static_cast<std::int32_t>(generatedSector.surfaces.size()); ++surfaceIndex)
            {
                if (generatedSector.surfaces[static_cast<std::size_t>(surfaceIndex)].adjoin)
                {
                    continue;
                }

                if (sed::level::levelBuildSurfaceExtrudeSectorPreview(level, sed::SurfaceRef{ sectorIndex, surfaceIndex }, 0.5, 0.0))
                {
                    foundExtrudableGeneratedSurface = true;
                    break;
                }
            }
            if (foundExtrudableGeneratedSurface)
            {
                break;
            }
        }
        check(foundExtrudableGeneratedSurface, "extrudeSurface.negativeExpand: generated sector keeps extrudable faces");
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;

        sed::Sector& sector = level.newSector();
        sector.addVertex(0.0, 0.0, 0.0);
        sector.addVertex(2.0, 0.0, 0.0);
        sector.addVertex(0.5, 1.0, 0.0);

        sed::Surface surface{};
        surface.material    = "test.mat";
        surface.geo         = static_cast<std::int32_t>(sed::TGeoMode::Texture);
        surface.vertices    = { 0, 1, 2 };
        surface.texVertices = std::vector<sed::TexVertex>(surface.vertices.size());
        sector.addSurface(std::move(surface));
        sed::level::surfaceRecalcNewAll(sed::level::levelPixelPerUnit(level), sector, sector.surfaces[0], 0);

        const auto preview = sed::level::levelBuildSurfaceExtrudeSourceCutPreview(level, sed::SurfaceRef{0,0}, -0.25);
        check(preview && !preview->surfaces.empty(),
            "extrudeSurface.negativeExpand: source cut exposes actual retained fragments of a non-rectangular source");
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;
        level.masterCMP = "master.cmp";

        sed::Sector& sector = level.newSector();
        sector.colormap = "sector.cmp";
        sector.addVertex(0.0, 0.0, 0.0);
        sector.addVertex(1.0, 0.0, 0.0);
        sector.addVertex(1.0, 1.0, 0.0);
        sector.addVertex(0.0, 1.0, 0.0);

        sed::Surface surface{};
        surface.material    = "test.mat";
        surface.geo         = static_cast<std::int32_t>(sed::TGeoMode::Texture);
        surface.vertices    = { 0, 1, 2, 3 };
        surface.texVertices = std::vector<sed::TexVertex>(surface.vertices.size());
        sector.addSurface(std::move(surface));
        sed::level::surfaceRecalcNewAll(sed::level::levelPixelPerUnit(level), sector, sector.surfaces[0], 0);

        const bool ok = sed::level::levelExtrudeSurface(level, sed::SurfaceRef{ 0, 0 }, 1.0);
        check(ok, "extrudeSurface.colormap: jkdf explicit cmp returns true");
        check(level.sectors.size() == 2 && level.sectors[1].colormap == "sector.cmp",
            "extrudeSurface.colormap: jkdf explicit sector cmp copied");
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::MOTS;
        level.masterCMP = "mots_master.cmp";

        sed::Sector& sector = level.newSector();
        sector.addVertex(0.0, 0.0, 0.0);
        sector.addVertex(1.0, 0.0, 0.0);
        sector.addVertex(1.0, 1.0, 0.0);
        sector.addVertex(0.0, 1.0, 0.0);

        sed::Surface surface{};
        surface.material    = "test.mat";
        surface.geo         = static_cast<std::int32_t>(sed::TGeoMode::Texture);
        surface.vertices    = { 0, 1, 2, 3 };
        surface.texVertices = std::vector<sed::TexVertex>(surface.vertices.size());
        sector.addSurface(std::move(surface));
        sed::level::surfaceRecalcNewAll(sed::level::levelPixelPerUnit(level), sector, sector.surfaces[0], 0);

        const std::optional<sed::Sector> previewSector =
            sed::level::levelBuildSurfaceExtrudeSectorPreview(level, sed::SurfaceRef{ 0, 0 }, 1.0, 0.0);
        check(previewSector && previewSector->colormap == "mots_master.cmp",
            "extrudeSurface.colormap: mots preview falls back to master cmp");

        const bool ok = sed::level::levelExtrudeSurface(level, sed::SurfaceRef{ 0, 0 }, 1.0);
        check(ok, "extrudeSurface.colormap: mots fallback returns true");
        check(level.sectors.size() == 2 && level.sectors[1].colormap == "mots_master.cmp",
            "extrudeSurface.colormap: mots committed sector falls back to master cmp");
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::IJIM;
        level.masterCMP = "ijim_ignored.cmp";

        sed::Sector& sector = level.newSector();
        sector.addVertex(0.0, 0.0, 0.0);
        sector.addVertex(1.0, 0.0, 0.0);
        sector.addVertex(1.0, 1.0, 0.0);
        sector.addVertex(0.0, 1.0, 0.0);

        sed::Surface surface{};
        surface.material    = "test.mat";
        surface.geo         = static_cast<std::int32_t>(sed::TGeoMode::Texture);
        surface.vertices    = { 0, 1, 2, 3 };
        surface.texVertices = std::vector<sed::TexVertex>(surface.vertices.size());
        sector.addSurface(std::move(surface));
        sed::level::surfaceRecalcNewAll(sed::level::levelPixelPerUnit(level), sector, sector.surfaces[0], 0);

        const bool ok = sed::level::levelExtrudeSurface(level, sed::SurfaceRef{ 0, 0 }, 1.0);
        check(ok, "extrudeSurface.colormap: ijim returns true");
        check(level.sectors.size() == 2 && level.sectors[1].colormap.empty(),
            "extrudeSurface.colormap: ijim keeps empty sector colormap");
    }
    {
        // Fixed source-side arrangement regression. The exact CYN70 geometry
        // exercises the partitioned source-fragment transaction. The source
        // is a complete convex prism; the public builder supplies the
        // four-corner expanded boundary used by the real extrusion path.
        constexpr std::array<sed::TVector, 7> sourcePoints{{
            {0.055004999999999998, 1.439978, 4.1109738299999998},
            {0.27091398999999999, 1.4054319900000001, 4.1109738299999998},
            {0.26666799000000002, 1.4061110000000001, 3.8109738800000001},
            {0.265847, 1.4062429700000001, 3.7529370800000001},
            {0.055004999999999998, 1.439978, 3.7529370800000001},
            {0.055004999999999998, 1.439978, 3.8109738800000001},
            {0.055004999999999998, 1.439978, 3.9609739799999999}}};
        const std::array<sed::TexVertex, 7> kGradientTuples{{
            {0.0f, 0.0f, sed::ColorRed}, {0.125f, 0.25f, sed::ColorGreen},
            {0.25f, 0.5f, sed::ColorBlue}, {0.375f, 0.75f, sed::ColorWhite},
            {0.5f, 1.0f, sed::ColorRed}, {0.625f, 1.25f, sed::ColorGreen},
            {0.75f, 1.5f, sed::ColorBlue}}};
        constexpr sed::ColorF kSeamFreeColor{0.125f, 0.375f, 0.625f, 0.875f};
        constexpr std::int32_t kInitialUnrelatedGlobalSurface = 9;
        // The public seam-free extrusion and the manually assembled forced
        // candidate have different retained-face counts. This is the fixed
        // COG target for the manually assembled candidate below.
        constexpr std::int32_t kExpectedUnrelatedGlobalSurfaceAfterCleanup = 16;
        const auto makeFixture = [&](const bool seamFree)
        {
            sed::Level level;
            level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
            auto& source = level.newSector();
            for ( const auto& point : sourcePoints )
            {
                source.newVertex(point.x, point.y, point.z);
            }
            sed::Surface sourceFace;
            sourceFace.num = 0;
            sourceFace.mark = 1701;
            sourceFace.material = "source-fragment.mat";
            sourceFace.surfaceFlags = sed::SF_Collision;
            sourceFace.vertices = {0, 1, 2, 3, 4, 5, 6};
            sourceFace.texVertices.assign(kGradientTuples.begin(), kGradientTuples.end());
            if ( seamFree )
            {
                for ( auto& tuple : sourceFace.texVertices )
                {
                    tuple.color = kSeamFreeColor;
                }
            }
            sourceFace.uScale = 0.125f;
            sourceFace.vScale = -0.25f;
            sourceFace.normal = sed::level::surfaceCalcNormalNewell(source, sourceFace);
            source.addSurface(sourceFace);

            for ( const auto& point : sourcePoints )
            {
                source.newVertex(point.x, point.y + 0.2, point.z);
            }
            for ( std::size_t index = 0; index < source.vertices.size(); ++index )
            {
                source.vertices[index].mark = static_cast<std::int32_t>(1700 + index);
            }
            const auto appendSourceFace = [&](std::vector<std::int32_t> vertices)
            {
                sed::Surface face;
                face.num = static_cast<std::int32_t>(source.surfaces.size());
                face.material = "source-fragment.mat";
                face.surfaceFlags = sed::SF_Collision;
                face.vertices = std::move(vertices);
                face.texVertices.resize(face.vertices.size());
                face.normal = sed::level::surfaceCalcNormalNewell(source, face);
                source.addSurface(std::move(face));
            };
            appendSourceFace({13, 12, 11, 10, 9, 8, 7});
            for ( std::size_t edge = 0; edge < sourcePoints.size(); ++edge )
            {
                const auto next = (edge + 1) % sourcePoints.size();
                appendSourceFace({static_cast<std::int32_t>(next), static_cast<std::int32_t>(edge),
                    static_cast<std::int32_t>(edge + sourcePoints.size()),
                    static_cast<std::int32_t>(next + sourcePoints.size())});
            }

            auto& unrelated = level.newSector();
            unrelated.newVertex(10.0, 0.0, 0.0);
            unrelated.newVertex(11.0, 0.0, 0.0);
            unrelated.newVertex(11.0, 0.0, 1.0);
            unrelated.newVertex(10.0, 0.0, 1.0);
            sed::Surface unrelatedFace;
            unrelatedFace.num = 0;
            unrelatedFace.vertices = {0, 1, 2, 3};
            unrelatedFace.texVertices.resize(unrelatedFace.vertices.size());
            unrelatedFace.normal = sed::level::surfaceCalcNormalNewell(unrelated, unrelatedFace);
            unrelated.addSurface(std::move(unrelatedFace));

            sed::Cog cog;
            sed::CogValue cogValue;
            cogValue.type = sed::ValueType::ptr;
            cogValue.cogType = sed::CogType::surface;
            cogValue.intValue = 0;
            cogValue.stringValue = "0 0";
            cog.values.push_back(cogValue);
            sed::CogValue laterCogValue = cogValue;
            laterCogValue.intValue = kInitialUnrelatedGlobalSurface;
            laterCogValue.stringValue = "1 0";
            cog.values.push_back(std::move(laterCogValue));
            level.cogs.push_back(cog);
            return level;
        };
        const auto hasAuthoredTuple = [&](const sed::Sector& sector, const std::size_t corner,
            const sed::TexVertex& expected)
        {
            for ( const auto& face : sector.surfaces )
            {
                for ( std::size_t faceCorner = 0; faceCorner < face.vertices.size(); ++faceCorner )
                {
                    if ( faceCorner >= face.texVertices.size() || face.vertices[faceCorner] < 0
                        || static_cast<std::size_t>(face.vertices[faceCorner]) >= sector.vertices.size() )
                    {
                        continue;
                    }
                    const auto& point = sector.vertices[static_cast<std::size_t>(face.vertices[faceCorner])];
                    if ( point.x == sourcePoints[corner].x && point.y == sourcePoints[corner].y
                        && point.z == sourcePoints[corner].z
                        && sameTextureVertex(face.texVertices[faceCorner], expected) )
                    {
                        return true;
                    }
                }
            }
            return false;
        };

        sed::Level level = makeFixture(false);

        const auto untouchedAuthoredNormal = level.sectors[0].surfaces[1].normal;
        const auto before = level;
        const auto beforeBytes = sed::assets::saveToSedBytes(level);
        sed::level::SurfaceExtrudeReport report;
        const bool accepted = sed::level::levelExtrudeSurface(level, {0, 0}, 0.2, 0.1, &report);
        check(accepted && report.error == sed::level::SurfaceExtrudeError::None
                && report.surface == sed::SurfaceRef{0, 0} && level.sectors.size() == 3,
            "extrude.sourceFragments: public expanded extrusion accepts the fixed exact-alias crossing");
        if ( accepted && report.error == sed::level::SurfaceExtrudeError::None
            && report.surface == sed::SurfaceRef{0, 0} && level.sectors.size() == 3 )
        {
            const auto& result = level.sectors[0];
            bool validFragments = result.surfaces.size() > 1;
            for ( const auto& face : result.surfaces )
            {
                bool validFace = face.vertices.size() >= 3
                    && face.vertices.size() == face.texVertices.size();
                for ( std::size_t corner = 0; validFace && corner < face.vertices.size(); ++corner )
                {
                    const auto firstIndex = face.vertices[corner];
                    const auto next = (corner + 1) % face.vertices.size();
                    const auto secondIndex = face.vertices[next];
                    if ( firstIndex < 0 || secondIndex < 0
                        || static_cast<std::size_t>(firstIndex) >= result.vertices.size()
                        || static_cast<std::size_t>(secondIndex) >= result.vertices.size() )
                    {
                        validFace = false;
                        continue;
                    }
                    const auto& first = result.vertices[static_cast<std::size_t>(firstIndex)];
                    const auto& second = result.vertices[static_cast<std::size_t>(secondIndex)];
                    validFace = first.x != second.x || first.y != second.y || first.z != second.z;
                }
                validFragments = validFragments && validFace;
            }
            check(validFragments,
                "extrude.sourceFragments: retained source fragments have complete nonzero edges");

            const std::size_t originalVertexCount = sourcePoints.size() * 2;
            bool originalPool = result.vertices.size() >= originalVertexCount;
            for ( std::size_t index = 0; originalPool && index < originalVertexCount; ++index )
            {
                const auto& point = result.vertices[index];
                const auto& expected = sourcePoints[index % sourcePoints.size()];
                const double expectedY = index < sourcePoints.size() ? expected.y : expected.y + 0.2;
                originalPool = point.x == expected.x && point.y == expectedY && point.z == expected.z
                    && point.mark == static_cast<std::int32_t>(1700 + index);
            }
            check(originalPool,
                "extrude.sourceFragments: authored source vertex pool remains exact");
            bool authoredTuples = true;
            for ( std::size_t corner = 0; authoredTuples && corner < kGradientTuples.size(); ++corner )
            {
                authoredTuples = hasAuthoredTuple(result, corner, kGradientTuples[corner]);
            }
            check(authoredTuples,
                "extrude.sourceFragments: accepted gradient path preserves every authored UV/RGBA tuple");
            if ( !result.surfaces.empty() )
            {
                const auto& originalSurface = result.surfaces.front();
                check(originalSurface.material == "source-fragment.mat"
                        && originalSurface.mark == 1701
                        && originalSurface.uScale == 0.125f
                        && originalSurface.vScale == -0.25f,
                    "extrude.sourceFragments: authored source material, mark, and scales remain exact");
                check(result.surfaces.size() > 1
                        && result.surfaces[1].normal.x == untouchedAuthoredNormal.x
                        && result.surfaces[1].normal.y == untouchedAuthoredNormal.y
                        && result.surfaces[1].normal.z == untouchedAuthoredNormal.z,
                    "extrude.sourceFragments: untouched authored normal cache remains exact");
                check(originalSurface.surfaceFlags == 0
                        && originalSurface.faceFlags == 0
                        && originalSurface.geo == 0
                        && originalSurface.adjoinFlags
                            == (sed::SAF_Visible | sed::SAF_Move | sed::SAF_AllowSoundPass),
                    "extrude.sourceFragments: paired source portal metadata is reset by adjoin conversion");
            }
        }
        else if ( !accepted )
        {
            check(sed::assets::saveToSedBytes(level) == beforeBytes
                    && sed::assets::sedLevelsEqual(level, before),
                "extrude.sourceFragments: rejected source-fragment transaction restores every authored field");
        }

        sed::Level seamFree = makeFixture(true);
        const auto seamFreeUntouchedNormal = seamFree.sectors[0].surfaces[1].normal;
        const auto seamFreeBefore = seamFree;
        const auto seamFreeBeforeBytes = sed::assets::saveToSedBytes(seamFree);
        sed::level::SurfaceExtrudeReport seamFreeReport;
        const bool seamFreeAccepted = sed::level::levelExtrudeSurface(
            seamFree, {0, 0}, 0.2, 0.1, &seamFreeReport);
        check(seamFreeAccepted && seamFreeReport.error == sed::level::SurfaceExtrudeError::None
                && seamFreeReport.surface == sed::SurfaceRef{0, 0}
                && seamFree.sectors.size() == 3,
            "extrude.sourceFragments: constant nontrivial RGBA allows the public extrusion path");
        if ( seamFreeAccepted && seamFreeReport.error == sed::level::SurfaceExtrudeError::None
            && seamFreeReport.surface == sed::SurfaceRef{0, 0} && seamFree.sectors.size() == 3 )
        {
            const auto& result = seamFree.sectors[0];
            bool nonzeroEdges = true;
            for ( const auto& face : result.surfaces )
            {
                nonzeroEdges = nonzeroEdges && face.vertices.size() >= 3
                    && face.vertices.size() == face.texVertices.size();
                for ( std::size_t corner = 0; nonzeroEdges && corner < face.vertices.size(); ++corner )
                {
                    const auto next = (corner + 1) % face.vertices.size();
                    if ( face.vertices[corner] < 0 || face.vertices[next] < 0
                        || static_cast<std::size_t>(face.vertices[corner]) >= result.vertices.size()
                        || static_cast<std::size_t>(face.vertices[next]) >= result.vertices.size() )
                    {
                        nonzeroEdges = false;
                        continue;
                    }
                    const auto first = result.vertices[static_cast<std::size_t>(face.vertices[corner])];
                    const auto second = result.vertices[static_cast<std::size_t>(face.vertices[next])];
                    nonzeroEdges = first.x != second.x || first.y != second.y || first.z != second.z;
                }
            }
            check(nonzeroEdges,
                "extrude.sourceFragments: seam-free cleanup retains only complete source fragments");

            bool authoredPool = result.vertices.size() >= sourcePoints.size() * 2;
            for ( std::size_t index = 0; authoredPool && index < sourcePoints.size() * 2; ++index )
            {
                const auto& point = result.vertices[index];
                const auto& expected = sourcePoints[index % sourcePoints.size()];
                const double expectedY = index < sourcePoints.size() ? expected.y : expected.y + 0.2;
                authoredPool = point.x == expected.x && point.y == expectedY && point.z == expected.z
                    && point.mark == static_cast<std::int32_t>(1700 + index);
            }
            check(authoredPool,
                "extrude.sourceFragments: seam-free cleanup preserves authored IDs, coordinates, and marks");
            bool seamFreeTuples = true;
            for ( std::size_t corner = 0; seamFreeTuples && corner < kGradientTuples.size(); ++corner )
            {
                const auto expected = sed::TexVertex{kGradientTuples[corner].u,
                    kGradientTuples[corner].v, kSeamFreeColor};
                seamFreeTuples = hasAuthoredTuple(result, corner, expected);
            }
            check(seamFreeTuples,
                "extrude.sourceFragments: seam-free path preserves every authored UV/RGBA tuple");

            const bool hasUntouchedFace = result.surfaces.size() > 1;
            check(hasUntouchedFace && result.surfaces[1].normal.x == seamFreeUntouchedNormal.x
                    && result.surfaces[1].normal.y == seamFreeUntouchedNormal.y
                    && result.surfaces[1].normal.z == seamFreeUntouchedNormal.z,
                "extrude.sourceFragments: untouched authored normal cache remains exact");
            if ( !result.surfaces.empty() )
            {
                const auto& sourceFaceAfter = result.surfaces.front();
                check(sourceFaceAfter.material == "source-fragment.mat"
                        && sourceFaceAfter.mark == 1701
                        && sourceFaceAfter.uScale == 0.125f
                        && sourceFaceAfter.vScale == -0.25f
                        && sourceFaceAfter.surfaceFlags == 0
                        && sourceFaceAfter.faceFlags == 0
                        && sourceFaceAfter.geo == 0
                        && sourceFaceAfter.adjoinFlags
                            == (sed::SAF_Visible | sed::SAF_Move | sed::SAF_AllowSoundPass),
                    "extrude.sourceFragments: seam-free source portal metadata remains intentional");
            }
        }
        else if ( !seamFreeAccepted )
        {
            check(sed::assets::saveToSedBytes(seamFree) == seamFreeBeforeBytes
                    && sed::assets::sedLevelsEqual(seamFree, seamFreeBefore),
                "extrude.sourceFragments: seam-free failure restores every authored field");
        }

        // The public builder may accept a contained expansion through its
        // first candidate. Exercise the source-fragment cleanup through the
        // checked connection callback as well, rejecting that first candidate
        // so the partitioned retry is the only path that can publish.
        {
            sed::Level partitioned = makeFixture(true);
            const std::size_t sourceFaceCountBefore = partitioned.sectors[0].surfaces.size();
            sed::level::SurfaceExtrudeReport previewReport;
            const auto preview = sed::level::levelBuildSurfaceExtrudePreview(
                partitioned, {0, 0}, 0.2, 0.1, &previewReport);
            check(preview && previewReport.error == sed::level::SurfaceExtrudeError::None
                    && preview->innerLoop.size() == 4 && preview->outerLoop.size() == 4,
                "extrude.sourceFragments: seam-free fixture supplies a checked-connection preview");
            if ( preview && previewReport.error == sed::level::SurfaceExtrudeError::None
                && preview->innerLoop.size() == 4 && preview->outerLoop.size() == 4 )
            {
                sed::Sector generated;
                generated.num = 2;
                const auto appendLoop = [&](const std::vector<sed::TVector>& loop)
                {
                    std::vector<std::int32_t> indices;
                    indices.reserve(loop.size());
                    for ( const auto& point : loop )
                    {
                        indices.push_back(generated.newVertex(point.x, point.y, point.z));
                    }
                    return indices;
                };
                const auto inner = appendLoop(preview->innerLoop);
                const auto outer = appendLoop(preview->outerLoop);
                const auto appendFace = [&](std::vector<std::int32_t> corners)
                {
                    sed::Surface face;
                    face.num = static_cast<std::int32_t>(generated.surfaces.size());
                    face.material = "source-fragment.mat";
                    face.surfaceFlags = sed::SF_Collision;
                    face.vertices = std::move(corners);
                    face.texVertices.resize(face.vertices.size(), sed::TexVertex{0.0f, 0.0f, kSeamFreeColor});
                    face.normal = sed::level::surfaceCalcNormalNewell(generated, face);
                    generated.addSurface(std::move(face));
                };
                appendFace(inner);
                appendFace(outer);
                for ( std::size_t edge = 0; edge < inner.size(); ++edge )
                {
                    const auto next = (edge + 1) % inner.size();
                    const auto outerEdge = outer.size() - 1 - edge;
                    const auto outerNext = outer.size() - 1 - next;
                    appendFace({inner[next], inner[edge], outer[outerEdge], outer[outerNext]});
                }
                partitioned.sectors.push_back(std::move(generated));
                const auto beforePartitioned = partitioned;
                const auto beforePartitionedBytes = sed::assets::saveToSedBytes(partitioned);

                ForcePartitionProbe probe;
                const bool accepted = sed::level::precision::connectExtrudeSurfacesChecked(
                    partitioned, {0, 0}, {2, 0}, nullptr,
                    rejectFastPathThenAcceptPartition, &probe);
                const bool acceptedOutput = accepted && partitioned.sectors.size() == 3
                    && !partitioned.sectors[0].surfaces.empty()
                    && !partitioned.sectors[2].surfaces.empty();
                const bool forcedPartition = accepted && probe.calls == 2
                    && probe.firstPortalCount == 1 && probe.secondPortalCount == 8
                    && partitioned.sectors.size() == 3;
                std::size_t sourcePortalFaces = 0;
                std::size_t sourceNonPortalFaces = 0;
                if ( partitioned.sectors.size() == 3 )
                {
                    for ( const auto& face : partitioned.sectors[0].surfaces )
                    {
                        if ( face.adjoin && face.adjoin->sector == 2 )
                        {
                            ++sourcePortalFaces;
                        }
                        else
                        {
                            ++sourceNonPortalFaces;
                        }
                    }
                }
                const bool canonicalSourceLayout = forcedPartition
                    && partitioned.sectors[0].surfaces.size() == 16
                    && partitioned.sectors[0].surfaces.size()
                        == sourceFaceCountBefore - 1 + probe.secondPortalCount
                    && sourcePortalFaces == probe.secondPortalCount
                    && sourceNonPortalFaces == sourceFaceCountBefore - 1;
                check(forcedPartition,
                    "extrude.sourceFragments: checked validator forces the partitioned cleanup path");
                check(canonicalSourceLayout,
                    "extrude.sourceFragments: canonical source layout keeps eight authored faces and eight unique portal fragments");
                const auto exactOpposedBoundary = [](const sed::Sector& firstSector,
                    const sed::Surface& firstFace, const sed::Sector& secondSector,
                    const sed::Surface& secondFace)
                {
                    if ( firstFace.vertices.size() < 3
                        || firstFace.vertices.size() != secondFace.vertices.size() )
                    {
                        return false;
                    }
                    const auto count = firstFace.vertices.size();
                    for ( std::size_t start = 0; start < count; ++start )
                    {
                        bool matches = true;
                        for ( std::size_t corner = 0; corner < count; ++corner )
                        {
                            const auto firstIndex = firstFace.vertices[corner];
                            const auto secondIndex = secondFace.vertices[
                                (start + count - 1 - corner) % count];
                            if ( firstIndex < 0 || secondIndex < 0
                                || static_cast<std::size_t>(firstIndex) >= firstSector.vertices.size()
                                || static_cast<std::size_t>(secondIndex) >= secondSector.vertices.size() )
                            {
                                matches = false;
                                break;
                            }
                            const auto& first = firstSector.vertices[static_cast<std::size_t>(firstIndex)];
                            const auto& second = secondSector.vertices[static_cast<std::size_t>(secondIndex)];
                            if ( first.x != second.x || first.y != second.y || first.z != second.z )
                            {
                                matches = false;
                                break;
                            }
                        }
                        if ( matches )
                        {
                            return true;
                        }
                    }
                    return false;
                };
                const auto directedEdgesClose = [](const sed::Sector& sector)
                {
                    std::map<std::pair<std::int32_t, std::int32_t>, std::pair<int, int>> incidence;
                    for ( const auto& face : sector.surfaces )
                    {
                        if ( face.vertices.size() < 3 )
                        {
                            return false;
                        }
                        for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
                        {
                            const auto first = face.vertices[corner];
                            const auto second = face.vertices[(corner + 1) % face.vertices.size()];
                            if ( first < 0 || second < 0
                                || static_cast<std::size_t>(first) >= sector.vertices.size()
                                || static_cast<std::size_t>(second) >= sector.vertices.size()
                                || first == second )
                            {
                                return false;
                            }
                            auto& edge = incidence[{std::min(first, second), std::max(first, second)}];
                            ++edge.first;
                            edge.second += first < second ? 1 : -1;
                        }
                    }
                    return !incidence.empty() && std::all_of(incidence.begin(), incidence.end(),
                        [](const auto& edge)
                        {
                            return edge.second.first == 2 && edge.second.second == 0;
                        });
                };
                bool reciprocalSourcePortals = acceptedOutput;
                std::size_t reciprocalSourcePortalCount = 0;
                if ( acceptedOutput )
                {
                    const auto& sourceResult = partitioned.sectors[0];
                    const auto& generatedResult = partitioned.sectors[2];
                    for ( std::size_t sourceFaceIndex = 0;
                        sourceFaceIndex < sourceResult.surfaces.size(); ++sourceFaceIndex )
                    {
                        const auto& sourceFace = sourceResult.surfaces[sourceFaceIndex];
                        if ( !sourceFace.adjoin || sourceFace.adjoin->sector != 2 )
                        {
                            continue;
                        }
                        ++reciprocalSourcePortalCount;
                        const auto generatedFaceIndex = sourceFace.adjoin->surface;
                        if ( generatedFaceIndex < 0
                            || static_cast<std::size_t>(generatedFaceIndex)
                                >= generatedResult.surfaces.size() )
                        {
                            reciprocalSourcePortals = false;
                            continue;
                        }
                        const auto& generatedFace = generatedResult.surfaces[
                            static_cast<std::size_t>(generatedFaceIndex)];
                        reciprocalSourcePortals = reciprocalSourcePortals
                            && generatedFace.adjoin
                            && generatedFace.adjoin->sector == 0
                            && generatedFace.adjoin->surface
                                == static_cast<std::int32_t>(sourceFaceIndex)
                            && exactOpposedBoundary(sourceResult, sourceFace,
                                generatedResult, generatedFace);
                    }
                }
                check(acceptedOutput && reciprocalSourcePortals
                        && reciprocalSourcePortalCount == probe.secondPortalCount,
                    "extrude.sourceFragments: accepted source portal fragments have exact reciprocal reverse-cyclic boundaries");
                check(acceptedOutput && directedEdgesClose(partitioned.sectors[0])
                        && directedEdgesClose(partitioned.sectors[2]),
                    "extrude.sourceFragments: accepted source and generated sectors retain closed directed-edge incidence");
                if ( acceptedOutput )
                {
                    const auto& result = partitioned.sectors[0];
                    bool authoredPool = result.vertices.size() >= sourcePoints.size() * 2;
                    for ( std::size_t index = 0; authoredPool && index < sourcePoints.size() * 2; ++index )
                    {
                        const auto& point = result.vertices[index];
                        const auto& expected = sourcePoints[index % sourcePoints.size()];
                        const double expectedY = index < sourcePoints.size() ? expected.y : expected.y + 0.2;
                        authoredPool = point.x == expected.x && point.y == expectedY && point.z == expected.z
                            && point.mark == static_cast<std::int32_t>(1700 + index);
                    }
                    check(authoredPool,
                        "extrude.sourceFragments: forced partition preserves authored IDs and marks");

                    bool sourceTuples = true;
                    for ( std::size_t corner = 0; sourceTuples && corner < kGradientTuples.size(); ++corner )
                    {
                        const auto expected = sed::TexVertex{kGradientTuples[corner].u,
                            kGradientTuples[corner].v, kSeamFreeColor};
                        sourceTuples = hasAuthoredTuple(result, corner, expected);
                    }
                    check(sourceTuples,
                        "extrude.sourceFragments: forced partition preserves retained source UV/RGBA tuples");

                    const bool untouchedNormal = result.surfaces.size() > 1
                        && result.surfaces[1].normal.x == seamFreeUntouchedNormal.x
                        && result.surfaces[1].normal.y == seamFreeUntouchedNormal.y
                        && result.surfaces[1].normal.z == seamFreeUntouchedNormal.z;
                    check(untouchedNormal,
                        "extrude.sourceFragments: forced partition preserves the untouched normal cache");

                    bool hasArtificialOneUlp = false;
                    if ( result.vertices.size() > sourcePoints.size() * 2 )
                    {
                        const auto& authored = result.vertices[6];
                        const double nextUp = std::nextafter(authored.x, std::numeric_limits<double>::infinity());
                        const double nextDown = std::nextafter(authored.x, -std::numeric_limits<double>::infinity());
                        for ( std::size_t index = sourcePoints.size() * 2; index < result.vertices.size(); ++index )
                        {
                            const auto& point = result.vertices[index];
                            hasArtificialOneUlp = hasArtificialOneUlp
                                || (point.y == authored.y && point.z == authored.z
                                    && (point.x == nextUp || point.x == nextDown));
                        }
                    }
                    check(!hasArtificialOneUlp,
                        "extrude.sourceFragments: canonical support identity does not publish an artificial one-ULP duplicate");

                    check(partitioned.cogs.size() == 1 && partitioned.cogs[0].values.size() == 2
                            && partitioned.cogs[0].values[0].intValue == 0
                            && partitioned.cogs[0].values[0].stringValue == "0 0"
                            && partitioned.cogs[0].values[1].intValue
                                == kExpectedUnrelatedGlobalSurfaceAfterCleanup
                            && partitioned.cogs[0].values[1].stringValue.empty(),
                        "extrude.sourceFragments: forced partition remaps the unrelated COG to fixed global 16");
                }
                else if ( !accepted )
                {
                    check(sed::assets::saveToSedBytes(partitioned) == beforePartitionedBytes
                            && sed::assets::sedLevelsEqual(partitioned, beforePartitioned),
                        "extrude.sourceFragments: failed forced partition leaves the fixture unchanged");
                }
            }
        }
    }
    {
        // CYN73:7 contains an authored edge with three exactly collinear
        // points. The contained extrusion path must preserve that exact
        // incidence while keeping every published face nondegenerate.
        constexpr std::array<sed::TVector, 6> sourcePoints{{
            {-0.241228, 1.48737502, 4.06097412},
            {-0.241228, 1.48737502, 3.9609739799999999},
            {-0.241228, 1.48737502, 3.7529370800000001},
            {-0.27282599000000002, 1.2898869500000001, 3.7529370800000001},
            {-0.27282599000000002, 1.2898869500000001, 4.06097412},
            {-0.24912699999999999, 1.4380029400000001, 4.06097412}}};
        constexpr std::array<sed::TVector, 6> backPoints{{
            {-0.441228, 1.48737502, 4.06097412},
            {-0.441228, 1.48737502, 3.9609739799999999},
            {-0.441228, 1.48737502, 3.7529370800000001},
            {-0.47282599000000002, 1.2898869500000001, 3.7529370800000001},
            {-0.47282599000000002, 1.2898869500000001, 4.06097412},
            {-0.44912699999999999, 1.4380029400000001, 4.06097412}}};
        constexpr std::array<sed::TexVertex, 6> sourceTuples{{
            {0.121616311f, -0.12500006f, sed::ColorWhite},
            {0.121616311f, -0.250000298f, sed::ColorWhite},
            {0.121616311f, -0.510046542f, sed::ColorWhite},
            {0.121616311f, -0.625f, sed::ColorWhite},
            {0.121616311f, -0.375f, sed::ColorWhite},
            {0.121616311f, -0.125f, sed::ColorWhite}}};
        sed::Level level;
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        auto& source = level.newSector();
        for ( const auto& point : sourcePoints )
        {
            source.newVertex(point.x, point.y, point.z);
        }
        sed::Surface sourceFace;
        sourceFace.num = 0;
        sourceFace.material = "cyn73-collinear.mat";
        sourceFace.surfaceFlags = sed::SF_Collision;
        sourceFace.vertices = {0, 1, 2, 3, 4, 5};
        sourceFace.texVertices.assign(sourceTuples.begin(), sourceTuples.end());
        sourceFace.normal = sed::level::surfaceCalcNormalNewell(source, sourceFace);
        source.addSurface(sourceFace);

        for ( const auto& point : backPoints )
        {
            source.newVertex(point.x, point.y, point.z);
        }
        const auto appendFace = [&](std::vector<std::int32_t> vertices,
            std::vector<sed::TexVertex> texVertices = {})
        {
            sed::Surface face;
            face.num = static_cast<std::int32_t>(source.surfaces.size());
            face.material = "cyn73-collinear.mat";
            face.surfaceFlags = sed::SF_Collision;
            face.vertices = std::move(vertices);
            if ( texVertices.empty() )
            {
                texVertices.resize(face.vertices.size());
            }
            face.texVertices = std::move(texVertices);
            face.normal = sed::level::surfaceCalcNormalNewell(source, face);
            source.addSurface(std::move(face));
        };
        appendFace({11, 10, 9, 8, 7, 6});
        for ( std::size_t edge = 0; edge < sourcePoints.size(); ++edge )
        {
            const auto next = (edge + 1) % sourcePoints.size();
            appendFace({static_cast<std::int32_t>(next), static_cast<std::int32_t>(edge),
                static_cast<std::int32_t>(edge + sourcePoints.size()),
                static_cast<std::int32_t>(next + sourcePoints.size())});
        }

        const auto independentArea = [](const sed::Sector& sector, const sed::Surface& face)
        {
            std::array<long double, 3> area{};
            if ( face.vertices.size() < 3 )
            {
                return area;
            }
            const auto validIndex = [&](const std::int32_t index)
            {
                return index >= 0 && static_cast<std::size_t>(index) < sector.vertices.size();
            };
            if ( !validIndex(face.vertices.front()) )
            {
                return area;
            }
            const auto& anchor = sector.vertices[static_cast<std::size_t>(face.vertices.front())];
            for ( std::size_t corner = 1; corner + 1 < face.vertices.size(); ++corner )
            {
                if ( !validIndex(face.vertices[corner]) || !validIndex(face.vertices[corner + 1]) )
                {
                    return std::array<long double, 3>{};
                }
                const auto& first = sector.vertices[static_cast<std::size_t>(face.vertices[corner])];
                const auto& second = sector.vertices[static_cast<std::size_t>(face.vertices[corner + 1])];
                const long double ax = static_cast<long double>(first.x) - anchor.x;
                const long double ay = static_cast<long double>(first.y) - anchor.y;
                const long double az = static_cast<long double>(first.z) - anchor.z;
                const long double bx = static_cast<long double>(second.x) - anchor.x;
                const long double by = static_cast<long double>(second.y) - anchor.y;
                const long double bz = static_cast<long double>(second.z) - anchor.z;
                area[0] += ay * bz - az * by;
                area[1] += az * bx - ax * bz;
                area[2] += ax * by - ay * bx;
            }
            return area;
        };
        const auto areaNonzero = [&](const sed::Sector& sector, const sed::Surface& face)
        {
            const auto area = independentArea(sector, face);
            return std::isfinite(area[0]) && std::isfinite(area[1]) && std::isfinite(area[2])
                && (area[0] != 0.0L || area[1] != 0.0L || area[2] != 0.0L);
        };
        const auto sourceArea = independentArea(source, source.surfaces.front());
        const auto backArea = independentArea(source, source.surfaces[1]);
        const long double sourceWinding = sourceArea[0] * sourceFace.normal.x
            + sourceArea[1] * sourceFace.normal.y + sourceArea[2] * sourceFace.normal.z;
        const long double backWinding = backArea[0] * sourceFace.normal.x
            + backArea[1] * sourceFace.normal.y + backArea[2] * sourceFace.normal.z;
        bool sourceFacesNonzero = true;
        for ( const auto& face : source.surfaces )
        {
            sourceFacesNonzero = sourceFacesNonzero && areaNonzero(source, face);
        }
        check(sourceFacesNonzero && sourceWinding > 0.0L && backWinding < 0.0L,
            "extrude.collinearCyn73: fixed source prism has independent nonzero area and opposing cap winding");

        const auto before = level;
        const auto beforeBytes = sed::assets::saveToSedBytes(level);
        sed::level::SurfaceExtrudeReport report;
        const bool accepted = sed::level::levelExtrudeSurface(level, {0, 0}, 0.2, 0.1, &report);
        check(accepted && report.error == sed::level::SurfaceExtrudeError::None
                && report.surface == sed::SurfaceRef{0, 0} && level.sectors.size() == 2,
            "extrude.collinearCyn73: exact collinear source prism accepts contained extrusion");
        if ( accepted && report.error == sed::level::SurfaceExtrudeError::None
            && report.surface == sed::SurfaceRef{0, 0} && level.sectors.size() == 2 )
        {
            const auto& result = level.sectors[0];
            bool nonzeroFaces = true;
            for ( const auto& sector : level.sectors )
            {
                for ( const auto& face : sector.surfaces )
                {
                    nonzeroFaces = nonzeroFaces && face.vertices.size() >= 3
                        && face.vertices.size() == face.texVertices.size()
                        && areaNonzero(sector, face);
                }
            }
            check(nonzeroFaces,
                "extrude.collinearCyn73: accepted source and generated faces have nonzero independent area");

            bool sourcePool = result.vertices.size() >= sourcePoints.size() * 2;
            for ( std::size_t index = 0; sourcePool && index < sourcePoints.size() * 2; ++index )
            {
                const auto& point = result.vertices[index];
                const auto& expected = index < sourcePoints.size()
                    ? sourcePoints[index] : backPoints[index - sourcePoints.size()];
                sourcePool = point.x == expected.x && point.y == expected.y && point.z == expected.z;
            }
            check(sourcePool,
                "extrude.collinearCyn73: authored source vertex pool remains exact");

            bool sourceTuplesPresent = true;
            for ( std::size_t corner = 0; sourceTuplesPresent && corner < sourcePoints.size(); ++corner )
            {
                bool found = false;
                for ( const auto& face : result.surfaces )
                {
                    for ( std::size_t faceCorner = 0; faceCorner < face.vertices.size(); ++faceCorner )
                    {
                        const auto index = face.vertices[faceCorner];
                        if ( faceCorner >= face.texVertices.size() || index < 0
                            || static_cast<std::size_t>(index) >= result.vertices.size() )
                        {
                            continue;
                        }
                        const auto& point = result.vertices[static_cast<std::size_t>(index)];
                        found = found || (point.x == sourcePoints[corner].x
                            && point.y == sourcePoints[corner].y && point.z == sourcePoints[corner].z
                            && sameTextureVertex(face.texVertices[faceCorner], sourceTuples[corner]));
                    }
                }
                sourceTuplesPresent = found;
            }
            check(sourceTuplesPresent,
                "extrude.collinearCyn73: every authored collinear corner retains its fixed UV tuple");

            bool reciprocal = true;
            std::size_t portalCount = 0;
            for ( std::size_t sectorIndex = 0; reciprocal && sectorIndex < level.sectors.size(); ++sectorIndex )
            {
                const auto& sector = level.sectors[sectorIndex];
                for ( std::size_t faceIndex = 0; reciprocal && faceIndex < sector.surfaces.size(); ++faceIndex )
                {
                    const auto& face = sector.surfaces[faceIndex];
                    if ( !face.adjoin )
                    {
                        continue;
                    }
                    ++portalCount;
                    const auto otherSector = face.adjoin->sector;
                    const auto otherFace = face.adjoin->surface;
                    const bool validOther = otherSector >= 0
                        && static_cast<std::size_t>(otherSector) < level.sectors.size()
                        && otherFace >= 0
                        && static_cast<std::size_t>(otherFace)
                            < level.sectors[static_cast<std::size_t>(otherSector)].surfaces.size()
                        && level.sectors[static_cast<std::size_t>(otherSector)]
                            .surfaces[static_cast<std::size_t>(otherFace)].adjoin
                            == sed::SurfaceRef{static_cast<std::int32_t>(sectorIndex),
                                static_cast<std::int32_t>(faceIndex)};
                    if ( !validOther )
                    {
                        reciprocal = false;
                        continue;
                    }
                    const auto& otherSectorRef = level.sectors[static_cast<std::size_t>(otherSector)];
                    const auto& otherFaceRef = otherSectorRef.surfaces[static_cast<std::size_t>(otherFace)];
                    bool exactBoundary = face.vertices.size() == face.texVertices.size()
                        && otherFaceRef.vertices.size() == otherFaceRef.texVertices.size()
                        && face.vertices.size() == otherFaceRef.vertices.size();
                    for ( std::size_t corner = 0; exactBoundary && corner < face.vertices.size(); ++corner )
                    {
                        const auto firstIndex = face.vertices[corner];
                        const auto secondIndex = otherFaceRef.vertices[face.vertices.size() - 1 - corner];
                        if ( firstIndex < 0 || secondIndex < 0
                            || static_cast<std::size_t>(firstIndex) >= sector.vertices.size()
                            || static_cast<std::size_t>(secondIndex) >= otherSectorRef.vertices.size() )
                        {
                            exactBoundary = false;
                            continue;
                        }
                        const auto& firstPoint = sector.vertices[static_cast<std::size_t>(firstIndex)];
                        const auto& secondPoint = otherSectorRef.vertices[static_cast<std::size_t>(secondIndex)];
                        exactBoundary = firstPoint.x == secondPoint.x
                            && firstPoint.y == secondPoint.y && firstPoint.z == secondPoint.z;
                    }
                    reciprocal = exactBoundary;
                }
            }
            check(reciprocal && portalCount > 0,
                "extrude.collinearCyn73: accepted source and generated portals remain nonzero and reciprocal");
        }
        else if ( !accepted )
        {
            check(sed::assets::saveToSedBytes(level) == beforeBytes
                    && sed::assets::sedLevelsEqual(level, before),
                "extrude.collinearCyn73: rejected extrusion restores the exact authored prism");
        }
    }
    {
        // PYR10:9 contains a ten-corner source face whose inset expansion
        // previously stopped at side four with five corners left to ear-clip.
        // Keep the logged binary64 corners in a small closed prism so the
        // failure is independent of the surrounding game level. The source
        // attributes and every accepted result are checked by local oracles;
        // no production convexity or area predicate is reused here.
        constexpr std::array<sed::TVector, 10> sourcePoints{{
            {1.0656249499999999, -0.87, 0.44999999000000002},
            {1.07650399, -0.89320898000000004, 0.39197799999999999},
            {1.09375, -0.93000000999999999, 0.30000000999999998},
            {1.09375, -0.87774700000000005, 0.30000000999999998},
            {1.09375, -0.85225998999999997, 0.30000000999999998},
            {1.09375, -0.68602401000000002, 0.30000000999999998},
            {1.06960297, -0.68602401000000002, 0.42878199},
            {1.0656249499999999, -0.70486897000000004, 0.44999999000000002},
            {1.05833697, -0.73939001999999998, 0.48886900999999999},
            {1.05833697, -0.85445201000000004, 0.48886900999999999}}};
        constexpr double layerOffset = 0.2;
        constexpr std::array<sed::TexVertex, 10> sourceTuples{{
            {0.125f, -0.375f, {0.13f, 0.27f, 0.41f, 1.0f}},
            {0.25f, -0.25f, {0.23f, 0.37f, 0.51f, 0.9f}},
            {0.375f, -0.125f, {0.33f, 0.47f, 0.61f, 0.8f}},
            {0.5f, 0.0f, {0.43f, 0.57f, 0.71f, 0.7f}},
            {0.625f, 0.125f, {0.53f, 0.67f, 0.81f, 0.6f}},
            {0.75f, 0.25f, {0.63f, 0.77f, 0.91f, 0.5f}},
            {0.875f, 0.375f, {0.73f, 0.87f, 0.21f, 0.4f}},
            {1.0f, 0.5f, {0.83f, 0.17f, 0.31f, 0.3f}},
            {1.125f, 0.625f, {0.93f, 0.27f, 0.41f, 0.2f}},
            {1.25f, 0.75f, {0.03f, 0.37f, 0.51f, 0.1f}}}};
        const auto makeFixture = [&]
        {
            sed::Level level;
            level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
            auto& source = level.newSector();
            source.mark = 701;
            for ( std::size_t index = 0; index < sourcePoints.size(); ++index )
            {
                const auto vertex = source.newVertex(sourcePoints[index].x, sourcePoints[index].y,
                    sourcePoints[index].z);
                source.vertices[static_cast<std::size_t>(vertex)].mark =
                    static_cast<std::int32_t>(4100 + index);
            }
            sed::Surface sourceFace;
            sourceFace.num = 0;
            sourceFace.mark = 5100;
            sourceFace.material = "pyr10-inset.mat";
            sourceFace.surfaceFlags = sed::SF_Collision;
            sourceFace.vertices.resize(sourcePoints.size());
            for ( std::size_t index = 0; index < sourcePoints.size(); ++index )
            {
                sourceFace.vertices[index] = static_cast<std::int32_t>(index);
            }
            sourceFace.texVertices.assign(sourceTuples.begin(), sourceTuples.end());
            sourceFace.normal = sed::level::surfaceCalcNormalNewell(source, sourceFace);
            source.addSurface(std::move(sourceFace));

            for ( std::size_t index = 0; index < sourcePoints.size(); ++index )
            {
                const auto& point = sourcePoints[index];
                const auto vertex = source.newVertex(point.x + layerOffset, point.y, point.z);
                source.vertices[static_cast<std::size_t>(vertex)].mark =
                    static_cast<std::int32_t>(4200 + index);
            }
            const auto appendFace = [&](std::vector<std::int32_t> vertices,
                const std::size_t tupleSeed)
            {
                sed::Surface face;
                face.num = static_cast<std::int32_t>(source.surfaces.size());
                face.mark = static_cast<std::int32_t>(5200 + face.num);
                face.material = "pyr10-inset.mat";
                face.surfaceFlags = sed::SF_Collision;
                face.vertices = std::move(vertices);
                face.texVertices.resize(face.vertices.size());
                for ( std::size_t index = 0; index < face.texVertices.size(); ++index )
                {
                    face.texVertices[index] = sourceTuples[(tupleSeed + index) % sourceTuples.size()];
                }
                face.normal = sed::level::surfaceCalcNormalNewell(source, face);
                source.addSurface(std::move(face));
            };
            std::vector<std::int32_t> back;
            back.reserve(sourcePoints.size());
            for ( std::size_t index = 0; index < sourcePoints.size(); ++index )
            {
                back.push_back(static_cast<std::int32_t>(2 * sourcePoints.size() - 1 - index));
            }
            appendFace(std::move(back), 0);
            for ( std::size_t edge = 0; edge < sourcePoints.size(); ++edge )
            {
                const auto next = (edge + 1) % sourcePoints.size();
                appendFace({static_cast<std::int32_t>(next), static_cast<std::int32_t>(edge),
                    static_cast<std::int32_t>(edge + sourcePoints.size()),
                    static_cast<std::int32_t>(next + sourcePoints.size())}, edge);
            }
            return level;
        };
        const auto independentArea = [](const sed::Sector& sector, const sed::Surface& face)
            -> std::array<long double, 3>
        {
            std::array<long double, 3> area{};
            if ( face.vertices.size() < 3 )
            {
                return area;
            }
            const auto valid = [&](const std::int32_t index)
            {
                return index >= 0 && static_cast<std::size_t>(index) < sector.vertices.size();
            };
            if ( !valid(face.vertices.front()) )
            {
                return area;
            }
            const auto& anchor = sector.vertices[static_cast<std::size_t>(face.vertices.front())];
            for ( std::size_t corner = 1; corner + 1 < face.vertices.size(); ++corner )
            {
                if ( !valid(face.vertices[corner]) || !valid(face.vertices[corner + 1]) )
                {
                    return {};
                }
                const auto& first = sector.vertices[static_cast<std::size_t>(face.vertices[corner])];
                const auto& second = sector.vertices[static_cast<std::size_t>(face.vertices[corner + 1])];
                const long double ax = static_cast<long double>(first.x) - anchor.x;
                const long double ay = static_cast<long double>(first.y) - anchor.y;
                const long double az = static_cast<long double>(first.z) - anchor.z;
                const long double bx = static_cast<long double>(second.x) - anchor.x;
                const long double by = static_cast<long double>(second.y) - anchor.y;
                const long double bz = static_cast<long double>(second.z) - anchor.z;
                area[0] += ay * bz - az * by;
                area[1] += az * bx - ax * bz;
                area[2] += ax * by - ay * bx;
            }
            return area;
        };
        const auto areaNonzero = [&](const sed::Sector& sector, const sed::Surface& face)
        {
            const auto area = independentArea(sector, face);
            return std::isfinite(area[0]) && std::isfinite(area[1]) && std::isfinite(area[2])
                && (area[0] != 0.0L || area[1] != 0.0L || area[2] != 0.0L);
        };
        const auto allFacesValid = [&](const sed::Level& level)
        {
            if ( level.sectors.empty() )
            {
                return false;
            }
            for ( const auto& sector : level.sectors )
            {
                for ( const auto& face : sector.surfaces )
                {
                    if ( face.vertices.size() < 3 || face.vertices.size() != face.texVertices.size()
                        || !areaNonzero(sector, face) )
                    {
                        return false;
                    }
                }
            }
            return true;
        };
        const auto closedIncidence = [](const sed::Level& level)
        {
            for ( const auto& sector : level.sectors )
            {
                std::map<std::pair<std::int32_t, std::int32_t>, std::pair<int, int>> edges;
                for ( const auto& face : sector.surfaces )
                {
                    if ( face.vertices.size() < 3 )
                    {
                        return false;
                    }
                    for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
                    {
                        const auto first = face.vertices[corner];
                        const auto second = face.vertices[(corner + 1) % face.vertices.size()];
                        if ( first < 0 || second < 0 || first == second
                            || static_cast<std::size_t>(first) >= sector.vertices.size()
                            || static_cast<std::size_t>(second) >= sector.vertices.size() )
                        {
                            return false;
                        }
                        const auto key = std::pair{std::min(first, second), std::max(first, second)};
                        auto& incidence = edges[key];
                        ++incidence.first;
                        incidence.second += first < second ? 1 : -1;
                    }
                }
                for ( const auto& [edge, incidence] : edges )
                {
                    (void)edge;
                    if ( incidence.first != 2 || incidence.second != 0 )
                    {
                        return false;
                    }
                }
            }
            return true;
        };
        const auto reciprocalPortals = [](const sed::Level& level)
        {
            std::size_t portalCount = 0;
            for ( std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex )
            {
                const auto& sector = level.sectors[sectorIndex];
                for ( std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex )
                {
                    const auto& face = sector.surfaces[surfaceIndex];
                    if ( !face.adjoin )
                    {
                        continue;
                    }
                    ++portalCount;
                    const auto target = *face.adjoin;
                    if ( target.sector < 0 || target.surface < 0
                        || static_cast<std::size_t>(target.sector) >= level.sectors.size()
                        || static_cast<std::size_t>(target.surface)
                            >= level.sectors[static_cast<std::size_t>(target.sector)].surfaces.size() )
                    {
                        return false;
                    }
                    const auto& otherSector = level.sectors[static_cast<std::size_t>(target.sector)];
                    const auto& otherFace = otherSector.surfaces[static_cast<std::size_t>(target.surface)];
                    if ( !otherFace.adjoin
                        || *otherFace.adjoin != sed::SurfaceRef{
                            static_cast<std::int32_t>(sectorIndex), static_cast<std::int32_t>(surfaceIndex)}
                        || face.vertices.size() != otherFace.vertices.size()
                        || face.vertices.size() != face.texVertices.size()
                        || otherFace.vertices.size() != otherFace.texVertices.size() )
                    {
                        return false;
                    }
                    bool matchedRotation = false;
                    for ( std::size_t start = 0;
                        !matchedRotation && start < otherFace.vertices.size(); ++start )
                    {
                        bool exactBoundary = true;
                        for ( std::size_t corner = 0;
                            exactBoundary && corner < face.vertices.size(); ++corner )
                        {
                            const auto first = face.vertices[corner];
                            const auto second = otherFace.vertices[
                                (start + otherFace.vertices.size() - corner - 1)
                                    % otherFace.vertices.size()];
                            if ( first < 0 || second < 0
                                || static_cast<std::size_t>(first) >= sector.vertices.size()
                                || static_cast<std::size_t>(second) >= otherSector.vertices.size() )
                            {
                                exactBoundary = false;
                                continue;
                            }
                            const auto& firstPoint = sector.vertices[static_cast<std::size_t>(first)];
                            const auto& secondPoint = otherSector.vertices[static_cast<std::size_t>(second)];
                            exactBoundary = firstPoint.x == secondPoint.x
                                && firstPoint.y == secondPoint.y
                                && firstPoint.z == secondPoint.z;
                        }
                        matchedRotation = exactBoundary;
                    }
                    if ( !matchedRotation )
                    {
                        return false;
                    }
                }
            }
            return portalCount > 0;
        };
        const auto sourceAttributes = [&](const sed::Level& level)
        {
            if ( level.sectors.empty() || level.sectors.front().vertices.size() < sourcePoints.size() * 2 )
            {
                return false;
            }
            const auto& sector = level.sectors.front();
            for ( std::size_t index = 0; index < sourcePoints.size() * 2; ++index )
            {
                const auto& expected = sourcePoints[index % sourcePoints.size()];
                const auto& point = sector.vertices[index];
                const double expectedX = index < sourcePoints.size()
                    ? expected.x : expected.x + layerOffset;
                if ( point.x != expectedX || point.y != expected.y || point.z != expected.z
                    || point.mark != static_cast<std::int32_t>(index < sourcePoints.size()
                        ? 4100 + index : 4200 + index - sourcePoints.size()) )
                {
                    return false;
                }
            }
            for ( std::size_t corner = 0; corner < sourceTuples.size(); ++corner )
            {
                bool found = false;
                for ( const auto& face : sector.surfaces )
                {
                    for ( std::size_t faceCorner = 0; faceCorner < face.vertices.size(); ++faceCorner )
                    {
                        if ( faceCorner >= face.texVertices.size() )
                        {
                            continue;
                        }
                        const auto index = face.vertices[faceCorner];
                        if ( index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size() )
                        {
                            continue;
                        }
                        const auto& point = sector.vertices[static_cast<std::size_t>(index)];
                        found = found || (point.x == sourcePoints[corner].x
                            && point.y == sourcePoints[corner].y && point.z == sourcePoints[corner].z
                            && sameTextureVertex(face.texVertices[faceCorner], sourceTuples[corner]));
                    }
                }
                if ( !found )
                {
                    return false;
                }
            }
            return true;
        };

        auto level = makeFixture();
        const auto& source = level.sectors.front();
        const auto sourceArea = independentArea(source, source.surfaces[0]);
        const auto backArea = independentArea(source, source.surfaces[1]);
        const auto& sourceNormal = source.surfaces[0].normal;
        const long double sourceWinding = sourceArea[0] * sourceNormal.x
            + sourceArea[1] * sourceNormal.y + sourceArea[2] * sourceNormal.z;
        const long double backWinding = backArea[0] * sourceNormal.x
            + backArea[1] * sourceNormal.y + backArea[2] * sourceNormal.z;
        check(allFacesValid(level) && std::isfinite(sourceWinding)
                && std::isfinite(backWinding) && sourceWinding > 0.0L && backWinding < 0.0L,
            "extrude.pyr10Inset: exact logged source prism has independent nonzero faces and opposing caps");

        sed::level::SurfaceExtrudeReport report;
        const bool accepted = sed::level::levelExtrudeSurface(level, {0, 0}, 0.1, -0.01, &report);
        check(accepted && report.error == sed::level::SurfaceExtrudeError::None
                && report.surface == sed::SurfaceRef{0, 0} && level.sectors.size() == 2,
            "extrude.pyr10Inset: inset extrusion accepts the exact ten-corner source prism");
        if ( accepted && report.error == sed::level::SurfaceExtrudeError::None
            && report.surface == sed::SurfaceRef{0, 0} && level.sectors.size() == 2 )
        {
            check(allFacesValid(level),
                "extrude.pyr10Inset: every published face has finite nonzero independent area and complete tuples");
            check(closedIncidence(level),
                "extrude.pyr10Inset: every sector has exact two-sided closed edge incidence");
            check(reciprocalPortals(level),
                "extrude.pyr10Inset: generated portals are nonempty, reciprocal, and exactly opposed");
            check(sourceAttributes(level),
                "extrude.pyr10Inset: authored coordinates, marks, and fixed UV/RGBA tuples survive");
        }

        auto rejected = makeFixture();
        const auto beforeBytes = sed::assets::saveToSedBytes(rejected);
        sed::level::SurfaceExtrudeReport rejectedReport{
            sed::level::SurfaceExtrudeError::InvalidResult, {-9, -9}};
        const bool rejectedAccepted = sed::level::levelExtrudeSurface(rejected, {0, 0}, 0.0, -0.01,
            &rejectedReport);
        check(!rejectedAccepted && rejectedReport.error == sed::level::SurfaceExtrudeError::InvalidLength
                && rejectedReport.surface == sed::SurfaceRef{0, 0}
                && sed::assets::saveToSedBytes(rejected) == beforeBytes
                && sed::assets::sedLevelsEqual(rejected, makeFixture()),
            "extrude.pyr10Inset: invalid length reports the requested surface and rolls back byte-exactly");
    }
    {
        // Keep a small, maintained version of the external quantized-prism
        // sweep in the core gate. The ignored probe has 3 slopes * 3
        // translated origins * 4 inherited-bend sizes * 2 signed Expand
        // values. This matrix keeps those exact fixed inputs and makes every
        // accepted result prove its portal footprint independently in XZ.
        const auto makeQuantizedFixture = [](const double origin, const double slope, const double warp)
        {
            sed::Level level;
            level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
            auto& owner = level.newSector();
            constexpr std::array<std::int32_t, 14> vertexMarks{{0, 7, -11, 23, 42, -37, 101,
                -203, 307, -401, 509, -613, 719, -827}};
            constexpr std::array<sed::ColorF, 7> cornerColors{{
                {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 0.9f},
                {0.0f, 0.0f, 1.0f, 0.8f}, {1.0f, 1.0f, 0.0f, 0.7f},
                {1.0f, 0.0f, 1.0f, 0.6f}, {0.0f, 1.0f, 1.0f, 0.5f},
                {0.25f, 0.5f, 0.75f, 1.0f}}};
            const std::array<sed::TVector, 7> polygon{{
                {.5, 0, -.91}, {.5, 0, -.53}, {.5, 0, -.14}, {.5, 0, .64},
                {.5, 0, .983}, {0, 0, .983}, {0, 0, -.91}}};
            for ( int layer = 0; layer < 2; ++layer )
            {
                for ( const auto& point : polygon )
                {
                    const double y = std::round(slope * point.z * 1.0e7) / 1.0e7;
                    const auto vertex = owner.newVertex(origin + point.x,
                        origin + y + (point.x == 0.0 ? warp : 0.0) - layer,
                        origin + point.z);
                    owner.vertices[static_cast<std::size_t>(vertex)].mark =
                        vertexMarks[static_cast<std::size_t>(vertex)];
                }
            }
            const auto append = [&](std::vector<std::int32_t> corners)
            {
                sed::Surface face;
                // Give the selected source a stable identity so the
                // preservation oracle cannot satisfy a corner from an
                // untouched neighbour carrying the same UV tuple.
                face.material = owner.surfaces.empty() ? "selected-source.mat" : "neighbour.mat";
                face.vertices = std::move(corners);
                face.texVertices.reserve(face.vertices.size());
                for ( const auto index : face.vertices )
                {
                    sed::TexVertex tuple;
                    tuple.u = static_cast<float>(index % 7) * .25f;
                    tuple.v = -tuple.u;
                    tuple.color = cornerColors[static_cast<std::size_t>(index % 7)];
                    face.texVertices.push_back(tuple);
                }
                face.normal = sed::level::surfaceCalcNormalNewell(owner, face);
                face.num = static_cast<std::int32_t>(owner.surfaces.size());
                owner.addSurface(std::move(face));
            };
            append({0, 1, 2, 3, 4, 5, 6});
            append({13, 12, 11, 10, 9, 8, 7});
            for ( int edge = 0; edge < 7; ++edge )
            {
                append({(edge + 1) % 7, edge, edge + 7, (edge + 1) % 7 + 7});
            }
            sed::Cog cog;
            sed::CogValue value;
            value.type = sed::ValueType::ptr;
            value.cogType = sed::CogType::surface;
            value.intValue = 1;
            value.stringValue = "0 1";
            cog.values.push_back(std::move(value));
            level.cogs.push_back(std::move(cog));
            return level;
        };
        const auto xzArea = [](const sed::Sector& source, const bool portalsOnly)
        {
            double area = 0.0;
            std::size_t faceCount = 0;
            for ( const auto& face : source.surfaces )
            {
                if ( portalsOnly != static_cast<bool>(face.adjoin) )
                {
                    continue;
                }
                ++faceCount;
                if ( face.vertices.size() < 3 )
                {
                    return std::pair{std::size_t{}, -1.0};
                }
                for ( const auto index : face.vertices )
                {
                    if ( index < 0 || static_cast<std::size_t>(index) >= source.vertices.size() )
                    {
                        return std::pair{std::size_t{}, -1.0};
                    }
                }
                const auto& anchor = source.vertices[static_cast<std::size_t>(face.vertices.front())];
                double twiceArea = 0.0;
                for ( std::size_t corner = 1; corner + 1 < face.vertices.size(); ++corner )
                {
                    const auto& first = source.vertices[static_cast<std::size_t>(face.vertices[corner])];
                    const auto& second = source.vertices[static_cast<std::size_t>(face.vertices[corner + 1])];
                    twiceArea += (first.x - anchor.x) * (second.z - anchor.z)
                        - (first.z - anchor.z) * (second.x - anchor.x);
                }
                area += std::abs(twiceArea) * .5;
            }
            return std::pair{faceCount, area};
        };
        const auto preservesSourceCorners = [](const sed::Sector& actual, const sed::Sector& expected)
        {
            if ( expected.surfaces.empty() )
            {
                return false;
            }
            const auto& expectedFace = expected.surfaces.front();
            if ( expectedFace.vertices.size() != expectedFace.texVertices.size() )
            {
                return false;
            }
            for ( std::size_t corner = 0; corner < expectedFace.vertices.size(); ++corner )
            {
                const auto expectedIndex = expectedFace.vertices[corner];
                if ( expectedIndex < 0 || static_cast<std::size_t>(expectedIndex) >= expected.vertices.size() )
                {
                    return false;
                }
                const auto& expectedPoint = expected.vertices[static_cast<std::size_t>(expectedIndex)];
                bool found = false;
                for ( const auto& face : actual.surfaces )
                {
                    if ( face.material != expectedFace.material
                        || face.vertices.size() != face.texVertices.size() )
                    {
                        continue;
                    }
                    for ( std::size_t current = 0; current < face.vertices.size(); ++current )
                    {
                        const auto index = face.vertices[current];
                        if ( index < 0 || static_cast<std::size_t>(index) >= actual.vertices.size() )
                        {
                            continue;
                        }
                        const auto& point = actual.vertices[static_cast<std::size_t>(index)];
                        found = found || (point.x == expectedPoint.x && point.y == expectedPoint.y
                            && point.z == expectedPoint.z
                            && sameTextureVertex(face.texVertices[current], expectedFace.texVertices[corner]));
                    }
                }
                if ( !found )
                {
                    return false;
                }
            }
            return true;
        };
        const auto sourceVertexPoolIsUnchanged = [](const sed::Sector& actual,
            const sed::Sector& expected)
        {
            if ( actual.vertices.size() < expected.vertices.size() )
            {
                return false;
            }
            for ( std::size_t index = 0; index < expected.vertices.size(); ++index )
            {
                const auto& current = actual.vertices[index];
                const auto& original = expected.vertices[index];
                if ( current.x != original.x || current.y != original.y
                    || current.z != original.z || current.mark != original.mark )
                {
                    return false;
                }
            }
            return true;
        };
        const auto inheritedCapWithinSourceEnvelope = [](const sed::Sector& actual,
            const sed::Sector& expected)
        {
            if ( expected.surfaces.empty() || expected.surfaces.front().vertices.empty() )
            {
                return false;
            }
            const auto& expectedFace = expected.surfaces.front();
            const double normalLength = std::hypot(expectedFace.normal.x,
                expectedFace.normal.y, expectedFace.normal.z);
            if ( !std::isfinite(normalLength) || !(normalLength > 0.0) )
            {
                return false;
            }
            const sed::TVector sourceNormal{expectedFace.normal.x / normalLength,
                expectedFace.normal.y / normalLength, expectedFace.normal.z / normalLength};
            const auto anchorIndex = expectedFace.vertices.front();
            if ( anchorIndex < 0 || static_cast<std::size_t>(anchorIndex) >= expected.vertices.size() )
            {
                return false;
            }
            const auto& anchor = expected.vertices[static_cast<std::size_t>(anchorIndex)];
            double minimumX = std::numeric_limits<double>::infinity();
            double maximumX = -std::numeric_limits<double>::infinity();
            double minimumY = std::numeric_limits<double>::infinity();
            double maximumY = -std::numeric_limits<double>::infinity();
            double minimumZ = std::numeric_limits<double>::infinity();
            double maximumZ = -std::numeric_limits<double>::infinity();
            double minimumSupport = std::numeric_limits<double>::infinity();
            double maximumSupport = -std::numeric_limits<double>::infinity();
            for ( const auto index : expectedFace.vertices )
            {
                if ( index < 0 || static_cast<std::size_t>(index) >= expected.vertices.size() )
                {
                    return false;
                }
                const auto& point = expected.vertices[static_cast<std::size_t>(index)];
                minimumX = std::min(minimumX, point.x);
                maximumX = std::max(maximumX, point.x);
                minimumY = std::min(minimumY, point.y);
                maximumY = std::max(maximumY, point.y);
                minimumZ = std::min(minimumZ, point.z);
                maximumZ = std::max(maximumZ, point.z);
                const double support = sourceNormal.dot(
                    sed::TVector{point.x - anchor.x, point.y - anchor.y, point.z - anchor.z});
                if ( !std::isfinite(support) )
                {
                    return false;
                }
                minimumSupport = std::min(minimumSupport, support);
                maximumSupport = std::max(maximumSupport, support);
            }
            constexpr double envelopeError = 1.0e-10;
            bool foundPortal = false;
            for ( const auto& face : actual.surfaces )
            {
                if ( !face.adjoin || face.material != expectedFace.material )
                {
                    continue;
                }
                foundPortal = true;
                for ( const auto index : face.vertices )
                {
                    if ( index < 0 || static_cast<std::size_t>(index) >= actual.vertices.size() )
                    {
                        return false;
                    }
                    const auto& point = actual.vertices[static_cast<std::size_t>(index)];
                    const double support = sourceNormal.dot(
                        sed::TVector{point.x - anchor.x, point.y - anchor.y, point.z - anchor.z});
                    const double supportError = envelopeError
                        * (1.0 + std::max({std::abs(support), std::abs(minimumSupport),
                            std::abs(maximumSupport)}));
                    if ( point.x < minimumX - envelopeError || point.x > maximumX + envelopeError
                        || point.y < minimumY - envelopeError || point.y > maximumY + envelopeError
                        || point.z < minimumZ - envelopeError || point.z > maximumZ + envelopeError
                        || !std::isfinite(support)
                        || support < minimumSupport - supportError
                        || support > maximumSupport + supportError )
                    {
                        return false;
                    }
                }
            }
            return foundPortal;
        };
        const auto reciprocalPortals = [](const sed::Level& level)
        {
            if ( level.sectors.size() != 2 )
            {
                return false;
            }
            const auto& source = level.sectors[0];
            const auto& generated = level.sectors[1];
            std::size_t portalCount = 0;
            for ( std::size_t faceIndex = 0; faceIndex < source.surfaces.size(); ++faceIndex )
            {
                const auto& face = source.surfaces[faceIndex];
                if ( !face.adjoin )
                {
                    continue;
                }
                ++portalCount;
                if ( face.adjoin->sector != 1 || face.adjoin->surface < 0
                    || static_cast<std::size_t>(face.adjoin->surface) >= generated.surfaces.size() )
                {
                    return false;
                }
                const auto& partner = generated.surfaces[static_cast<std::size_t>(face.adjoin->surface)];
                if ( !partner.adjoin || partner.adjoin->sector != 0 || partner.adjoin->surface != face.num
                    || face.vertices.size() < 3 || partner.vertices.size() < 3 )
                {
                    return false;
                }
                std::vector<sed::TVector> sourceBoundary;
                sourceBoundary.reserve(face.vertices.size());
                for ( const auto index : face.vertices )
                {
                    if ( index < 0 || static_cast<std::size_t>(index) >= source.vertices.size() )
                    {
                        return false;
                    }
                    const auto& point = source.vertices[static_cast<std::size_t>(index)];
                    sourceBoundary.push_back({point.x, point.y, point.z});
                }
                std::vector<sed::TVector> generatedBoundary;
                generatedBoundary.reserve(partner.vertices.size());
                for ( const auto index : partner.vertices )
                {
                    if ( index < 0 || static_cast<std::size_t>(index) >= generated.vertices.size() )
                    {
                        return false;
                    }
                    const auto& point = generated.vertices[static_cast<std::size_t>(index)];
                    generatedBoundary.push_back({point.x, point.y, point.z});
                }
                if ( !opposedLoopsMatchAllowingSubdivisions(
                        std::move(sourceBoundary), std::move(generatedBoundary)) )
                {
                    return false;
                }
            }
            return portalCount > 0;
        };
        const auto generatedGeometryIsClosed = [](const sed::Sector& sector,
            const std::size_t originalFaceCount, const sed::Sector& source)
        {
            if ( sector.vertices.size() < 8 || sector.surfaces.size() < originalFaceCount
                || originalFaceCount < 2 || source.surfaces.empty()
                || source.surfaces.front().vertices.empty() )
            {
                return false;
            }
            struct EdgeIncidence
            {
                int count{};
                int winding{};
                std::size_t firstFace{};
                std::size_t secondFace{};
            };
            std::map<std::pair<std::int32_t, std::int32_t>, EdgeIncidence> incidence;
            for ( std::size_t faceIndex = 0; faceIndex < sector.surfaces.size(); ++faceIndex )
            {
                const auto& face = sector.surfaces[faceIndex];
                if ( face.vertices.size() < 3 || face.vertices.size() != face.texVertices.size() )
                {
                    return false;
                }
                if ( !std::isfinite(face.normal.x) || !std::isfinite(face.normal.y)
                    || !std::isfinite(face.normal.z) || !std::isfinite(face.uScale)
                    || !std::isfinite(face.vScale) )
                {
                    return false;
                }
                for ( const auto& tuple : face.texVertices )
                {
                    if ( !std::isfinite(tuple.u) || !std::isfinite(tuple.v) )
                    {
                        return false;
                    }
                }
                for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
                {
                    const auto index = face.vertices[corner];
                    const auto next = face.vertices[(corner + 1) % face.vertices.size()];
                    if ( index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size() )
                    {
                        return false;
                    }
                    if ( next < 0 || static_cast<std::size_t>(next) >= sector.vertices.size() || index == next )
                    {
                        return false;
                    }
                    const auto& point = sector.vertices[static_cast<std::size_t>(index)];
                    const auto& nextPoint = sector.vertices[static_cast<std::size_t>(next)];
                    if ( !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)
                        || !std::isfinite(nextPoint.x) || !std::isfinite(nextPoint.y)
                        || !std::isfinite(nextPoint.z)
                        || (point.x == nextPoint.x && point.y == nextPoint.y && point.z == nextPoint.z) )
                    {
                        return false;
                    }
                    auto& edge = incidence[{std::min(index, next), std::max(index, next)}];
                    if ( edge.count == 0 )
                    {
                        edge.firstFace = faceIndex;
                    }
                    else if ( edge.count == 1 )
                    {
                        edge.secondFace = faceIndex;
                    }
                    ++edge.count;
                    edge.winding += index < next ? 1 : -1;
                }
            }
            const bool closed = !incidence.empty() && std::all_of(incidence.begin(), incidence.end(),
                [](const auto& edge)
                {
                    const auto& value = edge.second;
                    return value.count == 2 && value.winding == 0
                        && value.firstFace != value.secondFace;
                });
            if ( !closed )
            {
                return false;
            }
            const auto& sourceFace = source.surfaces.front();
            const double sourceNormalLength = std::hypot(sourceFace.normal.x,
                sourceFace.normal.y, sourceFace.normal.z);
            if ( !std::isfinite(sourceNormalLength) || !(sourceNormalLength > 0.0) )
            {
                return false;
            }
            const sed::TVector sourceNormal{sourceFace.normal.x / sourceNormalLength,
                sourceFace.normal.y / sourceNormalLength, sourceFace.normal.z / sourceNormalLength};
            const auto sourceAnchorIndex = sourceFace.vertices.front();
            if ( sourceAnchorIndex < 0 || static_cast<std::size_t>(sourceAnchorIndex) >= source.vertices.size() )
            {
                return false;
            }
            const auto& sourceAnchor = source.vertices[static_cast<std::size_t>(sourceAnchorIndex)];
            double minimumSupport = std::numeric_limits<double>::infinity();
            double maximumSupport = -std::numeric_limits<double>::infinity();
            for ( const auto index : sourceFace.vertices )
            {
                if ( index < 0 || static_cast<std::size_t>(index) >= source.vertices.size() )
                {
                    return false;
                }
                const auto& point = source.vertices[static_cast<std::size_t>(index)];
                const double support = sourceNormal.dot(
                    sed::TVector{point.x - sourceAnchor.x, point.y - sourceAnchor.y,
                        point.z - sourceAnchor.z});
                if ( !std::isfinite(support) )
                {
                    return false;
                }
                minimumSupport = std::min(minimumSupport, support);
                maximumSupport = std::max(maximumSupport, support);
            }
            const auto withinSourceSupport = [&](const auto& face)
            {
                for ( const auto index : face.vertices )
                {
                    if ( index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size() )
                    {
                        return false;
                    }
                    const auto& point = sector.vertices[static_cast<std::size_t>(index)];
                    const double support = sourceNormal.dot(
                        sed::TVector{point.x - sourceAnchor.x, point.y - sourceAnchor.y,
                            point.z - sourceAnchor.z});
                    const double supportError = 1.0e-10
                        * (1.0 + std::max({std::abs(support), std::abs(minimumSupport),
                            std::abs(maximumSupport)}));
                    if ( !std::isfinite(support)
                        || support < minimumSupport - supportError
                        || support > maximumSupport + supportError )
                    {
                        return false;
                    }
                }
                return true;
            };
            // The outer cap and the original generated side faces are
                // newly constructed and must be strict. Inherited cap
                // fragments can preserve the source's rounded bends, so
                // they are covered by the source correspondence/portal
                // checks instead of an unrelated planar/convex oracle.
            return sed::level::surfaceIsPlanar(sector, sector.surfaces[1])
                && sed::level::surfaceIsConvex(sector, sector.surfaces[1])
                && std::all_of(sector.surfaces.begin() + 2,
                    sector.surfaces.begin() + static_cast<std::ptrdiff_t>(originalFaceCount),
                    [&](const auto& face)
                    {
                        return sed::level::surfaceIsPlanar(sector, face)
                            && sed::level::surfaceIsConvex(sector, face);
                    })
                && std::all_of(sector.surfaces.begin() + static_cast<std::ptrdiff_t>(originalFaceCount),
                    sector.surfaces.end(),
                    [&](const auto& face)
                    {
                        return withinSourceSupport(face) || (sed::level::surfaceIsPlanar(sector, face)
                            && sed::level::surfaceIsConvex(sector, face));
                    });
        };

        // Distinct indices do not make a zero-length edge valid. Keep this
        // negative vector independent from production admission so a future
        // test edit cannot accidentally make an index-only closure proof pass.
        auto exactZeroEdge = makeQuantizedFixture(0.0, 0.0, 0.0);
        auto& exactZeroSector = exactZeroEdge.sectors.front();
        check(generatedGeometryIsClosed(exactZeroSector, 6, exactZeroSector),
            "extrude.syntheticQuantized72: independent closure oracle accepts the planar baseline");
        const auto duplicateIndex = static_cast<std::int32_t>(exactZeroSector.vertices.size());
        exactZeroSector.vertices.push_back(exactZeroSector.vertices.front());
        exactZeroSector.surfaces.front().vertices[1] = duplicateIndex;
        check(!generatedGeometryIsClosed(exactZeroSector, 6, exactZeroSector),
            "extrude.syntheticQuantized72: independent closure oracle rejects distinct indices at identical coordinates");

        constexpr std::array<double, 3> slopes{{0.09123456, 0.13765432, 0.27182818}};
        constexpr std::array<double, 3> origins{{0.0, 3.0, 100.0}};
        constexpr std::array<double, 4> warps{{0.0, 1.0e-7, 3.0e-7, 1.0e-6}};
        constexpr std::array<double, 2> expands{{0.1, -0.01}};
        std::size_t caseIndex = 0;
        for ( const double slope : slopes )
        {
            for ( const double origin : origins )
            {
                for ( const double warp : warps )
                {
                    for ( const double expand : expands )
                    {
                        auto level = makeQuantizedFixture(origin, slope, warp);
                        const auto before = level;
                        const auto beforeBytes = sed::assets::saveToSedBytes(level);
                        sed::level::SurfaceExtrudeReport report{
                            sed::level::SurfaceExtrudeError::AllocationFailure, {-7, -8}};
                        const bool accepted = sed::level::levelExtrudeSurface(level, {0, 0}, .2, expand, &report);
                        if ( !accepted || report.error != sed::level::SurfaceExtrudeError::None )
                        {
                            std::cerr << "Quantized Extrude case=" << caseIndex
                                << " slope=" << slope << " origin=" << origin
                                << " warp=" << warp << " expand=" << expand
                                << " accepted=" << accepted
                                << " error=" << static_cast<int>(report.error) << '\n';
                        }
                        check(accepted && report.error == sed::level::SurfaceExtrudeError::None
                                && report.surface == sed::SurfaceRef{0, 0},
                            "extrude.syntheticQuantized72: every fixed source and signed Expand case returns success OUT");
                        if ( accepted && level.sectors.size() == 2 )
                        {
                            const auto [portalCount, area] = xzArea(level.sectors[0], true);
                            const bool sourceCorners = preservesSourceCorners(level.sectors[0], before.sectors[0]);
                            const bool sourceVertexPool = sourceVertexPoolIsUnchanged(
                                level.sectors[0], before.sectors[0]);
                            const bool sourceEnvelope = inheritedCapWithinSourceEnvelope(
                                level.sectors[0], before.sectors[0]);
                            const bool generated = generatedGeometryIsClosed(level.sectors[1], 6, before.sectors[0]);
                            bool footprint = portalCount > 0 && std::abs(area - 0.9465) <= 1.0e-10;
                            if ( expand < 0.0 )
                            {
                                sed::level::SurfaceExtrudeReport cutReport{
                                    sed::level::SurfaceExtrudeError::AllocationFailure, {-7, -8}};
                                const auto cut = sed::level::levelBuildSurfaceExtrudeSourceCutPreview(
                                    before, {0, 0}, expand, &cutReport);
                                const auto [rimCount, rimArea] = cut
                                    ? xzArea(*cut, false) : std::pair<std::size_t, double>{0, -1.0};
                                footprint = cut && cutReport.error == sed::level::SurfaceExtrudeError::None
                                    && cutReport.surface == sed::SurfaceRef{0, 0} && rimCount > 0
                                    && std::abs(area + rimArea - 0.9465) <= 1.0e-10;
                            }
                            const bool portals = reciprocalPortals(level);
                            const bool cog = level.cogs.size() == 1 && level.cogs[0].values.size() == 1
                                && level.cogs[0].values[0].intValue == 1
                                && level.cogs[0].values[0].stringValue == "0 1";
                            if ( !sourceCorners || !sourceVertexPool || !sourceEnvelope || !generated
                                || !portals || !footprint || !cog )
                            {
                                std::cerr << "Quantized Extrude oracle case=" << caseIndex
                                    << " sourceCorners=" << sourceCorners
                                    << " sourceVertexPool=" << sourceVertexPool
                                    << " sourceEnvelope=" << sourceEnvelope
                                    << " generated=" << generated
                                    << " portals=" << portals
                                    << " footprint=" << footprint
                                    << " cog=" << cog << '\n';
                            }
                            check(sourceCorners && sourceVertexPool && sourceEnvelope && generated
                                    && portals && footprint,
                                "extrude.syntheticQuantized72: generated solid keeps fixed corners, closed reciprocal portals, and conserved 0.9465 XZ coverage");
                            check(cog,
                                "extrude.syntheticQuantized72: fixed COG integer and pair text survive publication");
                        }
                        else if ( accepted )
                        {
                            check(false,
                                "extrude.syntheticQuantized72: accepted commit publishes exactly one generated sector");
                        }
                        else
                        {
                            check(sed::assets::saveToSedBytes(level) == beforeBytes,
                                "extrude.syntheticQuantized72: a rejected case rolls back every authored byte");
                        }
                        ++caseIndex;
                    }
                }
            }
        }
        check(caseIndex == 72,
            "extrude.syntheticQuantized72: fixed slope origin bend and signed Expand matrix contains exactly 72 cases");

        // Exercise the five public entry points on representative accepted
        // members of the same matrix. The full matrix above is a commit
        // regression; these controls pin each preview/report return contract
        // to the same authored source family.
        constexpr std::array<std::array<double, 3>, 3> apiCases{{
            {{0.09123456, 3.0, 1.0e-6}},
            {{0.13765432, 100.0, 1.0e-6}},
            {{0.27182818, 3.0, 3.0e-7}}}};
        for ( const auto& values : apiCases )
        {
            const auto source = makeQuantizedFixture(values[1], values[0], values[2]);
            const auto before = sed::assets::saveToSedBytes(source);
            sed::level::SurfaceExtrudeReport report{
                sed::level::SurfaceExtrudeError::AllocationFailure, {-7, -8}};
            auto wire = sed::level::levelBuildSurfaceExtrudePreview(source, {0, 0}, .2, .1, &report);
            check(wire && wire->innerLoop.size() == 4 && wire->outerLoop.size() == 4
                    && report.error == sed::level::SurfaceExtrudeError::None
                    && report.surface == sed::SurfaceRef{0, 0}
                    && sed::assets::saveToSedBytes(source) == before,
                "extrude.syntheticQuantized72: wire preview returns fixed four-corner loops and preserves input");
            auto solid = sed::level::levelBuildSurfaceExtrudeSectorPreview(source, {0, 0}, .2, .1, &report);
            check(solid && generatedGeometryIsClosed(*solid, 6, source.sectors.front())
                    && report.error == sed::level::SurfaceExtrudeError::None
                    && report.surface == sed::SurfaceRef{0, 0}
                    && sed::assets::saveToSedBytes(source) == before,
                "extrude.syntheticQuantized72: solid preview returns finite complete geometry and preserves input");
            auto bounds = sed::level::levelSurfaceExtrudeExpandBounds(source, {0, 0}, &report);
            check(bounds && bounds->minSpan > 0.0 && bounds->negativeCollapseLimit < 0.0
                    && report.error == sed::level::SurfaceExtrudeError::None
                    && report.surface == sed::SurfaceRef{0, 0}
                    && sed::assets::saveToSedBytes(source) == before,
                "extrude.syntheticQuantized72: bounds returns a finite positive span and complete success OUT");
            auto cut = sed::level::levelBuildSurfaceExtrudeSourceCutPreview(source, {0, 0}, -0.01, &report);
            bool validCut = cut && !cut->surfaces.empty();
            if ( validCut )
            {
                for ( const auto& face : cut->surfaces )
                {
                    validCut = validCut && !face.adjoin && face.vertices.size() >= 3
                        && face.vertices.size() == face.texVertices.size();
                    for ( const auto index : face.vertices )
                    {
                        validCut = validCut && index >= 0
                            && static_cast<std::size_t>(index) < cut->vertices.size();
                    }
                    for ( const auto& tuple : face.texVertices )
                    {
                        validCut = validCut && std::isfinite(tuple.u) && std::isfinite(tuple.v);
                    }
                }
            }
            check(validCut && report.error == sed::level::SurfaceExtrudeError::None
                    && report.surface == sed::SurfaceRef{0, 0}
                    && sed::assets::saveToSedBytes(source) == before,
                "extrude.syntheticQuantized72: negative source-cut preview returns visible unadjoined fragments and preserves input");
            auto committed = source;
            const bool accepted = sed::level::levelExtrudeSurface(committed, {0, 0}, .2, .1, &report);
            const auto [portalCount, area] = accepted && !committed.sectors.empty()
                ? xzArea(committed.sectors[0], true)
                : std::pair<std::size_t, double>{0, -1.0};
            check(accepted && report.error == sed::level::SurfaceExtrudeError::None
                    && report.surface == sed::SurfaceRef{0, 0} && committed.sectors.size() == 2
                    && reciprocalPortals(committed)
                    && portalCount > 0 && std::abs(area - 0.9465) <= 1.0e-10,
                "extrude.syntheticQuantized72: commit returns the same fixed reciprocal 0.9465 XZ footprint");
        }
    }
}

} // namespace sed::tests
