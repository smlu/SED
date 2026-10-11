#include "test_common.hpp"

#include "sed/level/geometry.hpp"
#include "../../src/level/geometry_precision.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <numbers>
#include <vector>

namespace sed::tests
{

void runLevelGeometryTests()
{
    {
        for ( const bool reverse : {false,true} )
        {
            for ( const bool offsetPlane : {false,true} )
            {
                double x = 17.0;
                double y = 18.0;
                double z = 19.0;
                const double firstX = reverse ? 2.0 : 0.0;
                const double secondX = reverse ? 0.0 : 2.0;
                const bool accepted = offsetPlane
                    ? level::planeLineIntersection({1,0,0}, 1e-10, firstX,2,0,secondX,2,0,x,y,z)
                    : level::planeLineIntersection({1,0,0}, 1e-10,2,0,firstX,2,0,secondX,2,0,x,y,z);
                check(accepted && x == 1e-10 && y == 2.0 && z == 0.0,
                    "geometry.planeLineIntersection: both plane forms and edge directions retain exact near-endpoint OUT coordinates");
            }
        }
        for ( const bool nonfinite : {false,true} )
        {
            for ( const bool offsetPlane : {false,true} )
            {
                double x = 17.0;
                double y = 18.0;
                double z = 19.0;
                const double firstX = nonfinite ? std::numeric_limits<double>::quiet_NaN() : 1.0;
                const bool accepted = offsetPlane
                    ? level::planeLineIntersection({1,0,0}, 1e-10, firstX,0,0,1,2,0,x,y,z)
                    : level::planeLineIntersection({1,0,0}, 1e-10,2,0,firstX,0,0,1,2,0,x,y,z);
                check(!accepted && x == 17.0 && y == 18.0 && z == 19.0,
                    "geometry.planeLineIntersection: parallel and nonfinite input preserve every OUT sentinel in both forms");
            }
        }
    }
    {
        const auto tetrahedron = [](const double origin)
        {
            Sector sector;
            sector.vertices = {{origin,origin,origin}, {origin+1,origin+1,origin},
                {origin,origin+1,origin}, {origin,origin,origin+1}};
            for ( const auto& loop : {std::vector<std::int32_t>{0,1,2}, {0,3,1}, {1,3,2}, {2,3,0}} )
            {
                Surface face;
                face.vertices = loop;
                face.texVertices.resize(loop.size());
                sector.surfaces.push_back(std::move(face));
            }
            return sector;
        };
        const auto rawClosed = [](const Sector& sector)
        {
            std::map<std::pair<std::int32_t,std::int32_t>, std::pair<int,int>> edges;
            for ( const auto& face : sector.surfaces )
            {
                for ( std::size_t index = 0; index < face.vertices.size(); ++index )
                {
                    const auto a = face.vertices[index], b = face.vertices[(index + 1) % face.vertices.size()];
                    auto& edge = edges[std::minmax(a,b)];
                    ++edge.first;
                    edge.second += a < b ? 1 : -1;
                }
            }
            return !edges.empty() && std::all_of(edges.begin(), edges.end(),
                [](const auto& edge) { return edge.second.first == 2 && edge.second.second == 0; });
        };
        for ( const double origin : {0.0,10.0} )
        {
            Sector sector = tetrahedron(origin);
            // The point is unused by every face. Its distance from the diagonal
            // is inside one directed projection's arithmetic bound and outside
            // the reverse projection's bound. It cannot open a closed body.
            const double delta = (2.0 * origin + 2.0) * 64.0 * std::numeric_limits<double>::epsilon();
            sector.vertices.push_back({origin+0.75,origin+0.75+delta,origin});
            for ( int permutation = 0; permutation < 3; ++permutation )
            {
                check(rawClosed(sector) && level::precision::closedBoundary(sector),
                    "geometry.closedBoundary: unused near-edge point cannot make balanced tetrahedron depend on directed subdivision or ordering");
                if ( permutation == 0 )
                {
                    for ( auto& face : sector.surfaces )
                    {
                        std::reverse(face.vertices.begin(), face.vertices.end());
                    }
                    std::reverse(sector.surfaces.begin(), sector.surfaces.end());
                }
                else
                {
                    std::reverse(sector.vertices.begin(), sector.vertices.end());
                    for ( auto& face : sector.surfaces )
                    {
                        for ( auto& index : face.vertices )
                        {
                            index = static_cast<std::int32_t>(sector.vertices.size()) - 1 - index;
                        }
                    }
                }
            }
        }
        {
            // A connected closed prism with a deep top-face indentation. The
            // near-edge point is used by three real faces, not an unused pool
            // entry, so this also requires symmetric shared-edge subdivision.
            Sector sector;
            sector.vertices = {{10,10,10}, {11,11,10}, {10,11,10},
                {10,10,11}, {11,11,11}, {10,11,11},
                {10.75,10.75+1408.0*std::numeric_limits<double>::epsilon(),10.0+32.0*std::numeric_limits<double>::epsilon()}};
            for ( const auto& loop : {std::vector<std::int32_t>{0,1,2}, {3,5,6}, {5,4,6}, {4,3,6},
                    {1,0,3,4}, {2,1,4,5}, {0,2,5,3}} )
            {
                Surface face;
                face.vertices = loop;
                face.texVertices.resize(loop.size());
                sector.surfaces.push_back(std::move(face));
            }
            check(rawClosed(sector) && level::precision::closedBoundary(sector),
                "geometry.closedBoundary: a used near-edge corner preserves closed incidence through shared canonical subdivision");
        }
        for ( const bool realGap : {false,true} )
        {
            Sector sector = tetrahedron(10.0);
            const double aliasX = realGap ? 10.0 + 1.0e-7 : std::nextafter(10.0,11.0);
            sector.vertices.push_back({aliasX,10.0,10.0});
            sector.surfaces[1].vertices[0] = 4;
            check(!rawClosed(sector) && level::precision::closedBoundary(sector) == !realGap,
                "geometry.closedBoundary: canonical coincident endpoint closes while a genuine positional gap remains open");
        }
        auto nonmanifold = tetrahedron(0.0);
        nonmanifold.surfaces.push_back(nonmanifold.surfaces.front());
        check(!rawClosed(nonmanifold) && !level::precision::closedBoundary(nonmanifold),
            "geometry.closedBoundary: duplicate face remains nonmanifold after canonical edge subdivision");
        auto malformed = tetrahedron(0.0);
        malformed.surfaces[0].vertices[0] = 99;
        check(!level::precision::closedBoundary(malformed),
            "geometry.closedBoundary: invalid corner index rejects before edge subdivision");
    }
    // Imported text coordinates are rounded independently. This analytic cube
    // has one corner displaced by two decimal coordinate quanta, producing a
    // top-face residual of warp/(2*sqrt(1+warp*warp/8)) at its other corners.
    for ( const double offset : {0.0, 10000.0} )
    {
        for ( const double warp : {0.00002, 0.01} )
        {
            Sector cube = makeLitTestCubeSector(0.0, ColorWhite);
            cube.vertices[6].z += warp;
            cube.surfaces[4].vertices = {7, 6, 5, 4};
            const double norm = std::sqrt(1.0 + warp * warp / 8.0);
            cube.surfaces[4].normal = {warp / (4.0 * norm), warp / (4.0 * norm), -1.0 / norm};
            for ( auto& vertex : cube.vertices )
            {
                vertex.x += offset;
                vertex.y += offset;
                vertex.z += offset;
            }
            const auto& face = cube.surfaces[4];
            check(!level::surfaceIsPlanar(cube, face) && !level::sectorIsConvex(cube),
                "legacy diagnostics: strict edit predicates retain rejection of actual decimal face warp");
            check(level::surfaceIsPlanarForDiagnostics(cube, face) == (warp == 0.00002)
                    && level::sectorIsConvexForDiagnostics(cube) == (warp == 0.00002),
                "legacy diagnostics: decimal quantization is accepted while independently larger warp is rejected");

            Sector mirror = cube;
            mirror.surfaces[4].vertices = {6, 7, 4, 5};
            mirror.surfaces[4].normal = -face.normal;
            check(level::surfaceBoundariesMatchForDiagnostics(cube, face, mirror, mirror.surfaces[4]),
                "legacy diagnostics: identical reversed warped portal loops match independently of first corner and plane warp");
            Sector subdivided = mirror;
            subdivided.surfaces[4].vertices.clear();
            for ( std::size_t corner = 0; corner < 4; ++corner )
            {
                const auto a = mirror.surfaces[4].vertices[corner];
                const auto b = mirror.surfaces[4].vertices[(corner + 1) % 4];
                const auto& first = mirror.vertices[a];
                const auto& last = mirror.vertices[b];
                subdivided.surfaces[4].vertices.push_back(a);
                subdivided.surfaces[4].vertices.push_back(static_cast<std::int32_t>(subdivided.vertices.size()));
                subdivided.vertices.push_back({(first.x + last.x) / 2, (first.y + last.y) / 2,
                    (first.z + last.z) / 2, 0});
            }
            check(level::surfaceBoundariesMatchForDiagnostics(cube, face, subdivided, subdivided.surfaces[4]),
                "legacy diagnostics: every portal edge may contain a redundant collinear subdivision");
            for ( auto& vertex : mirror.vertices )
            {
                vertex.z += 0.000001;
            }
            check(!level::surfaceBoundariesMatchForDiagnostics(cube, face, mirror, mirror.surfaces[4]),
                "legacy diagnostics: even a 1e-6 world-unit portal gap remains a mismatch");
            mirror = cube;
            mirror.surfaces[4].vertices = {7, 5, 6, 4};
            mirror.surfaces[4].normal = -face.normal;
            check(!level::surfaceBoundariesMatchForDiagnostics(cube, face, mirror, mirror.surfaces[4]),
                "legacy diagnostics: sharing the same vertices in a crossed order is not portal equivalence");
        }
    }
    {
        // Both loops describe the same opposed rounded boundary. The middle
        // top corner is within the point correspondence bound on both sides,
        // but only the first traversal is within the independent on-segment
        // reduction bound. Matching must compare the ordered corners before
        // simplifying either loop; otherwise the result depends on traversal
        // order and a valid rounded portal is rejected.
        Sector first;
        first.vertices = {{1, 1, 1}, {2, 1.0 + 2.5e-14, 1}, {3, 1, 1},
            {3, 2, 1}, {1, 2, 1}};
        Surface firstFace;
        firstFace.vertices = {0, 1, 2, 3, 4};
        firstFace.texVertices.resize(firstFace.vertices.size());
        firstFace.normal = level::surfaceCalcNormalNewell(first, firstFace);
        first.surfaces.push_back(firstFace);

        Sector second;
        second.vertices = {{1, 1, 1}, {2, 1.0 + 3.2e-14, 1}, {3, 1, 1},
            {3, 2, 1}, {1, 2, 1}};
        Surface secondFace;
        secondFace.vertices = {4, 3, 2, 1, 0};
        secondFace.texVertices.resize(secondFace.vertices.size());
        secondFace.normal = level::surfaceCalcNormalNewell(second, secondFace);
        second.surfaces.push_back(secondFace);
        check(level::precision::surfaceBoundariesMatchChecked(first, first.surfaces.front(),
                  second, second.surfaces.front()),
            "legacy diagnostics: opposed rounded loops retain a shallow bend when independent simplification would disagree");
    }
    {
        // A one-ULP height at a unit coordinate is representable even though
        // the ordinary point-comparison allowance considers it nearby. The
        // topology proof must retain that edge rather than silently weld it.
        const double oneUlp = std::nextafter(1.0, 2.0) - 1.0;
        Sector thin;
        thin.vertices = {{0, 1, 0}, {1, 1, 0}, {1, 1 + oneUlp, 0}};
        Surface thinFace;
        thinFace.vertices = {0, 1, 2};
        thinFace.normal = {0, 0, 1};
        thinFace.texVertices.resize(thinFace.vertices.size());
        thin.surfaces.push_back(thinFace);

        Sector thinReverse = thin;
        thinReverse.surfaces.front().vertices = {2, 1, 0};
        thinReverse.surfaces.front().normal = {0, 0, -1};
        check(level::precision::surfaceBoundariesMatchChecked(thin, thin.surfaces.front(),
                  thinReverse, thinReverse.surfaces.front()),
            "legacy diagnostics: opposed reversed loops retain a representable one-ULP thin triangle");

        Sector collinear;
        collinear.vertices = {{0, 1, 0}, {1, 1, 0}, {2, 1, 0}};
        Surface collinearFace;
        collinearFace.vertices = {0, 1, 2};
        collinearFace.normal = {0, 0, 1};
        collinear.surfaces.push_back(collinearFace);
        Sector collinearReverse = collinear;
        collinearReverse.surfaces.front().vertices = {2, 1, 0};
        collinearReverse.surfaces.front().normal = {0, 0, -1};
        check(!level::precision::surfaceBoundariesMatchChecked(collinear,
                  collinear.surfaces.front(), collinearReverse, collinearReverse.surfaces.front()),
            "legacy diagnostics: opposed unit cached normals cannot admit an exactly collinear boundary");
    }
    for ( const double dent : {0.00001, 0.00005, 0.001} )
    {
        for ( const double normalScale : {1.0, 1e-200, 1e200} )
        {
            Sector sector;
            sector.vertices = {{0, 0, 0, 0}, {2, 0, 0, 0}, {2, 1, 0, 0},
                {1, 1 - dent, 0, 0}, {0, 1, 0, 0}};
            Surface face;
            face.vertices = {0, 1, 2, 3, 4};
            face.normal = {0, 0, normalScale};
            // Central reflex turn is exactly -2*dent in square world units.
            check(level::surfaceIsConvexForDiagnostics(sector, face) == (dent <= 0.00005),
                "legacy diagnostics: legacy turn boundary is inclusive and independent of cached normal scale");
            std::reverse(face.vertices.begin(), face.vertices.end());
            face.normal = -face.normal;
            check(level::surfaceIsConvexForDiagnostics(sector, face) == (dent <= 0.00005),
                "legacy diagnostics: reversing both winding and normal preserves the reflex-turn classification");
        }
    }
    for ( const double distance : {0.001, 0.001001} )
    {
        for ( const double normalScale : {1.0, 1e-200, 1e200} )
        {
            Sector sector;
            sector.vertices = {{0, 0, 0, 0}, {1, 0, 0, 0}, {1, 1, distance, 0}, {0, 1, 0, 0}};
            Surface face;
            face.vertices = {0, 1, 2, 3};
            face.normal = {0, 0, normalScale};
            check(level::surfaceIsPlanarForDiagnostics(sector, face) == (distance == 0.001),
                "legacy diagnostics: world-distance planarity boundary is inclusive and normal-scale independent");
        }
    }
    const auto invalidateDiagnosticFace = [](Sector& sector, Surface& face, int malformed)
    {
        if ( malformed == 0 )
        {
            face.vertices.back() = -1;
        }
        if ( malformed == 1 )
        {
            face.vertices.back() = static_cast<std::int32_t>(sector.vertices.size());
        }
        if ( malformed == 2 )
        {
            sector.vertices[face.vertices.back()].x = std::numeric_limits<double>::quiet_NaN();
        }
        if ( malformed == 3 )
        {
            face.normal.y = std::numeric_limits<double>::infinity();
        }
        if ( malformed == 4 )
        {
            face.normal = {};
        }
        if ( malformed == 5 )
        {
            face.vertices.clear();
        }
        if ( malformed == 6 )
        {
            face.vertices.resize(2);
        }
        if ( malformed == 7 )
        {
            face.vertices = {0, 0, 0, 0};
        }
        if ( malformed == 8 )
        {
            face.normal = -face.normal;
        }
    };
    Sector diagnosticGood = makeLitTestCubeSector(0, ColorWhite);
    Sector diagnosticMirror = makeLitTestCubeSector(2, ColorWhite);
    check(level::surfaceBoundariesMatchForDiagnostics(diagnosticGood, diagnosticGood.surfaces[0],
              diagnosticMirror, diagnosticMirror.surfaces[1]),
        "legacy diagnostics: independently valid opposed pair establishes malformed-pair control");
    for ( int malformed = 0; malformed < 9; ++malformed )
    {
        Sector sector = diagnosticGood;
        auto& face = sector.surfaces[0];
        invalidateDiagnosticFace(sector, face, malformed);
        check(!level::surfaceIsPlanarForDiagnostics(sector, face)
                && !level::surfaceIsConvexForDiagnostics(sector, face)
                && !level::sectorIsConvexForDiagnostics(sector),
            "legacy diagnostics: malformed indices, nonfinite data, empty and collapsed faces fail closed");
        for ( const bool invalidateFirst : {false, true} )
        {
            Sector first = diagnosticGood;
            Sector second = diagnosticMirror;
            Sector& invalid = invalidateFirst ? first : second;
            invalidateDiagnosticFace(invalid, invalid.surfaces[invalidateFirst ? 0 : 1], malformed);
            check(!level::surfaceBoundariesMatchForDiagnostics(first, first.surfaces[0], second, second.surfaces[1]),
                "legacy diagnostics: corruption of either side rejects an otherwise valid opposed portal pair");
        }
    }
    for ( const double invalid : {std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()} )
    for ( std::size_t component = 0; component < 12; ++component )
    {
        // Plane z=0, with finite crossing endpoints (1,2,3) and (5,6,-3).
        std::array<double, 12> input{{0, 0, 1, 0, 0, 0, 1, 2, 3, 5, 6, -3}};
        input[component] = invalid;
        double x = 71, y = 72, z = 73;
        const bool accepted = sed::level::planeLineIntersection(
            sed::TVector{input[0], input[1], input[2]}, input[3], input[4], input[5],
            input[6], input[7], input[8], input[9], input[10], input[11], x, y, z);
        check(!accepted, "geometry contract: point-plane intersection rejects each nonfinite input component");
        check(x == 71 && y == 72 && z == 73,
            "geometry contract: point-plane intersection rejection preserves all output sentinels");
    }
    for ( const double invalid : {std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()} )
    for ( std::size_t component = 0; component < 10; ++component )
    {
        std::array<double, 10> input{{0, 0, 1, 0, 1, 2, 3, 5, 6, -3}};
        input[component] = invalid;
        double x = 71, y = 72, z = 73;
        const bool accepted = sed::level::planeLineIntersection(
            sed::TVector{input[0], input[1], input[2]}, input[3],
            input[4], input[5], input[6], input[7], input[8], input[9], x, y, z);
        check(!accepted, "geometry contract: offset-plane intersection rejects each nonfinite input component");
        check(x == 71 && y == 72 && z == 73,
            "geometry contract: offset-plane intersection rejection preserves all output sentinels");
    }
    {
        double x = 71, y = 72, z = 73;
        check(sed::level::planeLineIntersection(sed::TVector{0, 0, 2}, 0.0,
                  1, 2, 3, 5, 6, -3, x, y, z) && x == 3 && y == 4 && z == 0,
            "geometry contract: non-unit plane success returns the fixed analytic intersection");
        x = 71; y = 72; z = 73;
        check(!sed::level::planeLineIntersection(sed::TVector::zero, 0.0,
                  1, 2, 3, 5, 6, -3, x, y, z) && x == 71 && y == 72 && z == 73,
            "geometry contract: zero normal rejects without output mutation");
    }
    for ( const double scale : {1.0, 1e-200, 1e200} )
    {
        double x = 71, y = 72, z = 73;
        check(sed::level::planeLineIntersection(sed::TVector{scale, 0, 0}, scale,
                  0, 2, 3, 2, 6, 7, x, y, z) && x == 1 && y == 4 && z == 5,
            "geometry contract: scaled normal and offset retain the fixed intersection");
        x = 71; y = 72; z = 73;
        check(sed::level::planeLineIntersection(sed::TVector{scale, 0, 0}, 1, 0, 0,
                  0, 2, 3, 2, 6, 7, x, y, z) && x == 1 && y == 4 && z == 5,
            "geometry contract: scaled point-plane normal retains the fixed intersection");
    }
    {
        const double largest = std::numeric_limits<double>::max();
        double x = 71, y = 72, z = 73;
        check(sed::level::planeLineIntersection(sed::TVector{1, 0, 0}, 0.0,
                  largest, 2, 3, -largest, 6, 7, x, y, z) && x == 0 && y == 4 && z == 5,
            "geometry contract: opposite maximum finite distances interpolate without overflow");
        x = 71; y = 72; z = 73;
        check(!sed::level::planeLineIntersection(sed::TVector{1, 0, 0}, 0.0,
                  1, largest, 0, 2, 0, 0, x, y, z) && x == 71 && y == 72 && z == 73,
            "geometry contract: unrepresentable extrapolation rejects without partial output mutation");
    }
    for (const double width : {1.0, 100.0, 10000.0})
    for (const double offset : {0.0, 1000000.0})
    {
        const sed::TVector a{0.6,0.8,0.0}, b{-0.48,0.36,0.8}, h{0.64,-0.48,0.6};
        const double depth=0.7, height=0.005;
        sed::Sector sector{};
        for (int mask=0; mask<8; ++mask)
        {
            const sed::TVector point=sed::TVector{offset,offset,offset}+a*((mask&1)?width:0.0)
                + b*((mask&2)?depth:0.0) + h*((mask&4)?height:0.0);
            sector.newVertex(point.x,point.y,point.z);
        }
        const std::array<std::array<std::int32_t,4>,6> loops{{
            {{0,1,3,2}},{{4,6,7,5}},{{0,2,6,4}},{{1,5,7,3}},{{0,4,5,1}},{{2,3,7,6}}}};
        const std::array<sed::TVector,6> normals{{h,-h,a,-a,b,-b}};
        std::map<std::pair<int,int>,std::pair<int,int>> edges;
        long double volumeSix=0.0L;
        bool planes=true;
        for (std::size_t faceIndex=0; faceIndex<loops.size(); ++faceIndex)
        {
            sed::Surface face{};
            face.vertices.assign(loops[faceIndex].begin(),loops[faceIndex].end());
            face.texVertices.resize(4);
            face.normal=sed::level::surfaceCalcNormalNewell(sector,face);
            const auto& expectedNormal=normals[faceIndex];
            const auto& anchor=sector.vertices[face.vertices[0]];
            for (const auto index : face.vertices)
            {
                const auto& point=sector.vertices[index];
                const double allowance=64.0*std::numeric_limits<double>::epsilon()
                    *(std::abs(expectedNormal.x)*(std::abs(point.x)+std::abs(anchor.x))
                        +std::abs(expectedNormal.y)*(std::abs(point.y)+std::abs(anchor.y))
                        +std::abs(expectedNormal.z)*(std::abs(point.z)+std::abs(anchor.z)));
                const long double expectedDistance=static_cast<long double>(expectedNormal.x)*(point.x-anchor.x)
                    +static_cast<long double>(expectedNormal.y)*(point.y-anchor.y)
                    +static_cast<long double>(expectedNormal.z)*(point.z-anchor.z);
                planes=planes && std::abs(expectedDistance)<=allowance;
            }
            for (std::size_t edge=0; edge<4; ++edge)
            {
                const int first=face.vertices[edge], second=face.vertices[(edge+1)%4];
                auto& incidence=edges[{std::min(first,second),std::max(first,second)}];
                ++incidence.first; incidence.second+=first<second?1:-1;
            }
            for (std::size_t corner=1; corner+1<4; ++corner)
            {
                const auto& p=sector.vertices[face.vertices[corner]];
                const auto& q=sector.vertices[face.vertices[corner+1]];
                const long double ax=anchor.x-offset,ay=anchor.y-offset,az=anchor.z-offset;
                const long double px=p.x-offset,py=p.y-offset,pz=p.z-offset;
                const long double qx=q.x-offset,qy=q.y-offset,qz=q.z-offset;
                volumeSix+=ax*(py*qz-pz*qy)+ay*(pz*qx-px*qz)+az*(px*qy-py*qx);
            }
            sector.surfaces.push_back(face);
        }
        bool closed=true;
        for (const auto& [edge,incidence] : edges) closed=closed && incidence.first==2 && incidence.second==0;
        // At the distant origin, endpoint subtraction quantizes the .005
        // thickness by a few 1e-10 units. Keep the original local oracle and
        // allow only that relative volume quantization for translated inputs.
        const double volumeRelativeError=offset==0.0?1e-8:1e-7;
        check(planes && closed && std::abs(std::abs(volumeSix)/6.0L-width*depth*height)<width*depth*height*volumeRelativeError,
            "thin rotated prism has independently planar closed geometry and analytic volume");
        bool calculatedPlanes=true;
        for (const auto& face : sector.surfaces)
            calculatedPlanes=calculatedPlanes && sed::level::surfaceIsPlanar(sector,face);
        check(calculatedPlanes && sed::level::sectorIsConvex(sector),
            "thin rotated prism calculated normals preserve supporting planes without a float tolerance");
        sed::Sector portalOwner=sector;
        sed::Surface portal=sector.surfaces[4];
        std::reverse(portal.vertices.begin(),portal.vertices.end());
        std::rotate(portal.vertices.begin(),portal.vertices.begin()+1,portal.vertices.end());
        portal.normal=sed::level::surfaceCalcNormalNewell(portalOwner,portal);
        check(sed::level::surfaceDoOverlap(sector,sector.surfaces[4],portalOwner,portal),
            "thin rotated coincident portals remain compatible across different first corners");
        for (auto& vertex : portalOwner.vertices)
        {
            vertex.x+=b.x*1e-6; vertex.y+=b.y*1e-6; vertex.z+=b.z*1e-6;
        }
        check(!sed::level::surfaceDoOverlap(sector,sector.surfaces[4],portalOwner,portal),
            "thin rotated portal plane uncertainty never admits a real boundary gap");
        portalOwner=sector;
        portalOwner.vertices[portal.vertices[0]].z+=1e-6;
        check(!sed::level::surfaceDoOverlap(sector,sector.surfaces[4],portalOwner,portal),
            "thin rotated portal plane uncertainty never admits a tilted boundary");
        sed::Sector boundary=sector;
        boundary.surfaces={sector.surfaces[4]};
        const auto& anchor=boundary.vertices[boundary.surfaces[0].vertices[0]];
        const sed::TVector far=sed::TVector{anchor.x,anchor.y,anchor.z}+a*(width*10.0)+h*height;
        boundary.newVertex(far.x,far.y,far.z);
        check(sed::level::sectorIsConvex(boundary),
            "thin rotated plane permits analytically coplanar extrapolation with input-conditioned normals");
        auto& query=boundary.vertices.back();
        query.x-=b.x; query.y-=b.y; query.z-=b.z;
        check(!sed::level::sectorIsConvex(boundary),
            "thin rotated plane rejects a real outward query beyond arithmetic uncertainty");
    }
    {
        sed::Sector sector{};
        sector.vertices={{0,0,0,0},{1,0,0,0},{0,1,0,0},{1000000,1000000,-1e-9,0}};
        sed::Surface face{}; face.vertices={0,1,2}; face.normal={0,0,1};
        sector.surfaces={face};
        check(!sed::level::sectorIsConvex(sector),
            "axis-aligned plane keeps exact zero tangent uncertainty at distant queries");
        sector.vertices.back().z=1.0;
        sector.surfaces[0].normal={0,0,-1};
        check(!sed::level::sectorIsConvex(sector),"plane uncertainty never admits a reversed cached normal");
        sector.surfaces[0].normal={0.01,0,1};
        check(!sed::level::sectorIsConvex(sector),"plane uncertainty never trusts an unrelated cached normal");
    }
    {
        sed::Sector sector{};
        sector.vertices={{0,0,0,0},{3,0,0,0},{3,1,0,0},{1,1,0,0},{1,3,0,0},{0,3,0,0}};
        sed::Surface face{}; face.vertices={0,1,2,3,4,5};
        const auto normal=sed::level::surfaceCalcNormalNewell(sector,face);
        check(normal.x==0.0 && normal.y==0.0 && normal.z==1.0,
            "Newell area normal retains signed concave polygon winding");
    }
    for (const auto space : {sed::UvCoordinateSpace::Normalized, sed::UvCoordinateSpace::TexturePixels})
    for (const double origin : {0.0, 1000000.0})
    for (const float uSign : {-1.0f, 1.0f})
    for (const float vSign : {-1.0f, 1.0f})
    for (const bool flip : {false, true})
    {
        sed::Level level{};
        level.uvCoordinateSpace = space;
        level.ppunit = space == sed::UvCoordinateSpace::Normalized ? 1.0 : 32.0;
        auto& owner = level.newSector();
        owner.vertices = {{origin,origin,0,0},{origin+1,origin,0,0},
            {origin+1,origin+1,0,0},{origin,origin+1,0,0},
            {origin+0.5,origin,0,0},{origin+1,origin+0.5,0,0},
            {origin+0.5,origin+1,0,0},{origin,origin+0.5,0,0}};
        sed::Surface source{};
        source.vertices = {0,3,2,1}; source.normal = {0,0,-1};
        source.uScale = uSign; source.vScale = vSign;
        source.faceFlags = flip ? sed::FF_SF_FLIP : 0;
        for (const auto index : source.vertices)
        {
            const auto& vertex = owner.vertices[index];
            source.texVertices.push_back({static_cast<float>((vertex.x-origin)*level.ppunit),
                static_cast<float>((vertex.y-origin)*level.ppunit), sed::ColorRed});
        }
        sed::Surface target{};
        target.vertices = {0,4,1,5,2,6,3,7}; target.normal = {0,0,1};
        target.texVertices.assign(8, sed::TexVertex{7,8,sed::ColorGreen});
        owner.surfaces = {source,target};
        check(sed::level::levelStitchSurfaces(level,{0,0},{0,1}),
            "stitch mirrored portal accepts both UV spaces, signed scales and flip metadata");
        const auto& result = level.sectors[0].surfaces[1];
        bool exact = result.uScale == uSign && result.vScale == vSign && result.faceFlags == source.faceFlags;
        for (std::size_t corner=0; corner<result.vertices.size(); ++corner)
        {
            const auto& vertex = level.sectors[0].vertices[result.vertices[corner]];
            exact = exact && result.texVertices[corner].u == static_cast<float>((vertex.x-origin)*level.ppunit)
                && result.texVertices[corner].v == static_cast<float>((vertex.y-origin)*level.ppunit)
                && result.texVertices[corner].color == sed::ColorGreen;
        }
        check(exact, "stitch mirrored subdivided portal preserves analytic UVs at every corner and target lighting");
    }
    {
        sed::Level level{};
        level.ppunit = 1.0;
        auto& owner = level.newSector();
        owner.vertices = {{0,0,0,0},{1,0,0,0},{2,0,0,0},{0,1,0,0}};
        sed::Surface source{};
        source.vertices={0,1,2}; source.normal={0,0,1};
        source.texVertices={{0,0,sed::ColorRed},{1,0,sed::ColorRed},{2,0,sed::ColorRed}};
        sed::Surface target=source;
        target.vertices={0,1,3}; target.texVertices.assign(3,sed::TexVertex{7,8,sed::ColorGreen});
        owner.surfaces={source,target};
        check(!sed::level::levelStitchSurfaces(level,{0,0},{0,1}),
            "stitch rejects exactly collinear projection geometry");
        bool unchanged=true;
        for (const auto& uv : level.sectors[0].surfaces[1].texVertices)
            unchanged=unchanged && uv.u==7 && uv.v==8 && uv.color==sed::ColorGreen;
        check(unchanged,"stitch zero-area rejection leaves every target tuple unchanged");
        // A collinear leading run is legal when a later corner spans area.
        level.sectors[0].surfaces[0].vertices.push_back(3);
        level.sectors[0].surfaces[0].texVertices.push_back({0,1,sed::ColorRed});
        check(sed::level::levelStitchSurfaces(level,{0,0},{0,1}),
            "stitch searches beyond a collinear leading run for a valid triangle");
    }
    {
        sed::Level level{};
        level.ppunit=1.0;
        auto& owner=level.newSector();
        owner.vertices={{0,0,0,0},{1,0,0,0},{1,1,0,0},{0,1,0,0},
            {3,0,0,0},{4,0,0,0},{4,1,0,0},{3,1,0,0}};
        sed::Surface source{};
        source.vertices={0,1,2,3}; source.normal={0,0,1};
        source.texVertices={{0,0,sed::ColorRed},{1,0.5f,sed::ColorRed},
            {1,1.5f,sed::ColorRed},{0,1,sed::ColorRed}};
        sed::Surface target=source;
        target.vertices={4,5,6,7}; target.texVertices.assign(4,sed::TexVertex{7,8,sed::ColorGreen});
        owner.surfaces={source,target};
        check(sed::level::levelStitchSurfaces(level,{0,0},{0,1}),"stitch retains supported sheared-source fallback");
        // Legacy Stitch transports an orthogonal unit-density frame. It does
        // not promise a complete affine shear copy to disconnected targets.
        // The source's constant-V direction is (2,-1); its perpendicular is
        // (1,2), both normalized independently of authored UV extent.
        bool density=true;
        const double divisor=std::sqrt(5.0);
        const auto& result=level.sectors[0].surfaces[1];
        for (std::size_t corner=0; corner<4; ++corner)
        {
            const auto& vertex=level.sectors[0].vertices[result.vertices[corner]];
            const double x=vertex.x-3.0, y=vertex.y;
            density=density && std::abs(result.texVertices[corner].u-(7.0+(2*x-y)/divisor))<1e-6
                && std::abs(result.texVertices[corner].v-(8.0+(x+2*y)/divisor))<1e-6;
        }
        check(density,"stitch sheared source retains orthogonal density policy and disconnected UV anchor");
    }
    for ( const double origin : {0.0, 10000.0, 1000000.0} )
    {
        for ( const double length : {1.0, 0.01} )
        {
            for ( const double width : {0.001, 0.00001, 0.000001} )
            {
                sed::Level level{};
                level.ppunit = 1.0;
                level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
                auto& sector = level.newSector();
                for ( const auto point : {std::pair{0.0, 0.0}, std::pair{length, 0.0},
                        std::pair{length, width}, std::pair{0.0, width},
                        std::pair{length, 2.0 * width}, std::pair{0.0, 2.0 * width}} )
                {
                    sector.newVertex(origin + point.first, origin + point.second, origin);
                }
                sed::Surface source{};
                source.vertices = {0, 1, 2, 3};
                source.normal = {0, 0, 1};
                source.texVertices = {{0.0f, 0.0f, sed::ColorRed},
                    {static_cast<float>(length), 0.0f, sed::ColorGreen},
                    {static_cast<float>(length), 1.0f, sed::ColorBlue},
                    {0.0f, 1.0f, sed::ColorWhite}};
                sed::Surface target = source;
                target.vertices = {3, 2, 4, 5};
                for ( auto& uv : target.texVertices )
                {
                    uv.u = 7.0f;
                    uv.v = 8.0f;
                }
                sector.addSurface(source);
                sector.addSurface(target);
                check(sed::level::levelStitchSurfaces(level, {0, 0}, {0, 1}),
                    "stitch precision: translated thin adjacent surfaces remain stitchable");
                // Only seam endpoints are the oracle: Pascal Stitch derives
                // nonshared density from the retained textureScale fields.
                for ( std::size_t endpoint = 0; endpoint < 2; ++endpoint )
                {
                    const auto& expected = source.texVertices[3 - endpoint];
                    const auto& actual = sector.surfaces[1].texVertices[endpoint];
                    check(std::abs(actual.u - expected.u) < 1.0e-6
                            && std::abs(actual.v - expected.v) < 1.0e-6,
                        "stitch precision: exact shared edge wins over nearby distinct thin-face edges");
                }
                for ( std::size_t corner = 0; corner < source.texVertices.size(); ++corner )
                {
                    const auto& actual = sector.surfaces[0].texVertices[corner];
                    const auto& expected = source.texVertices[corner];
                    check(actual.u == expected.u && actual.v == expected.v && actual.color == expected.color,
                        "stitch precision: source corner data remains exact");
                }
            }
        }
    }
    {
        sed::TBox box{};
        sed::level::setBox(box, 5.0, -1.0, 2.0, 3.0, 0.0, -2.0);

        check(close(box.x1, -1.0), "level::setBox.3d.x1");
        check(close(box.x2, 5.0), "level::setBox.3d.x2");
        check(close(box.y1, 2.0), "level::setBox.3d.y1");
        check(close(box.y2, 3.0), "level::setBox.3d.y2");
        check(close(box.z1, -2.0), "level::setBox.3d.z1");
        check(close(box.z2, 0.0), "level::setBox.3d.z2");
    }

    {
        sed::TBox a{};
        sed::TBox b{};

        sed::level::setBox(a, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0);
        sed::level::setBox(b, 2.0, 3.0, 0.0, 1.0, 0.0, 1.0);
        check(!sed::level::doBoxesIntersect(a, b), "level::doBoxesIntersect.3d.false");

        sed::level::setBox(b, 1.0, 2.0, -1.0, 0.5, 0.5, 2.0);
        check(sed::level::doBoxesIntersect(a, b), "level::doBoxesIntersect.3d.true");
    }

    {
        sed::TBox box{};
        sed::level::setBox(box, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0);

        check(sed::level::isPointInBox(box, 0.0, 0.0, 0.0), "level::isPointInBox.3d.origin");
        check(sed::level::isPointInBox(box, 1.0 + sed::CloseEnough * 0.5, 0.0, 0.0), "level::isPointInBox.3d.epsilon.in");
        check(!sed::level::isPointInBox(box, 1.0 + sed::CloseEnough * 2.0, 0.0, 0.0), "level::isPointInBox.3d.epsilon.out");
    }

    {
        check(sed::level::nextIndex(0, 3) == 1, "level::nextIndex.0");
        check(sed::level::nextIndex(2, 3) == 0, "level::nextIndex.wrap");
        check(sed::level::prevIndex(0, 3) == 2, "level::prevIndex.wrap");
        check(sed::level::prevIndex(2, 3) == 1, "level::prevIndex.2");
    }

    {
        check(close(sed::level::getAngle(0.0, 0.0, 1.0, 0.0, 0.0, 1.0), std::numbers::pi_v<double> * 0.5), "level::getAngle.pi2");
        check(close(sed::level::getAngle(0.0, 0.0, 1.0, 0.0, 0.0, -1.0), std::numbers::pi_v<double> * 1.5), "level::getAngle.3pi2");
    }

    {
        check(sed::level::doIntersect(0.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0, 0.0), "level::doIntersect.true");
        check(!sed::level::doIntersect(0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0), "level::doIntersect.false");
    }

    {
        check(sed::level::isPointWithinLine(0.5, 0.5, 0.5, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0), "level::isPointWithinLine.true");
        check(!sed::level::isPointWithinLine(2.0, 0.5, 0.5, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0), "level::isPointWithinLine.false");
    }

    {
        check(sed::level::isOnLine(0.0, 0.0, 0.0, 2.0, 2.0, 2.0, 1.0, 1.0, 1.0), "level::isOnLine.true");
        check(!sed::level::isOnLine(0.0, 0.0, 0.0, 2.0, 2.0, 2.0, 1.0, 1.0, 1.1), "level::isOnLine.false");

        check(!sed::level::isOnLine(1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0), "level::isOnLine.degenerate");
        check(sed::level::isOnLine(1, 0, 0, 1, 2, 2, 1, 1, 1),
            "level::isOnLine fixed YZ midpoint lies on line");
        check(!sed::level::isOnLine(1, 0, 0, 1, 2, 2, 1.01, 1, 1),
            "level::isOnLine fixed YZ offset lies off line");
        check(sed::level::isOnLine(0, 0, 0, -0.00006103515625, 2, 0.00006103515625,
                  -0.00006103515625, 2, 0.00006103515625),
            "level::isOnLine Y-dominant line includes its exact finite endpoint");
        check(!sed::level::isOnLine(0, 0, 0, -0.00006103515625, 2, 0.00006103515625,
                  0.00006103515625, 2, 0.00006103515625),
            "level::isOnLine Y-dominant line rejects displaced endpoint");
    }

    {
        sed::TBox2 box{};
        sed::level::setBox(box, 5.0, -1.0, 2.0, 3.0);

        check(close(box.x1, -1.0), "level::setBox.2d.x1");
        check(close(box.x2, 5.0), "level::setBox.2d.x2");
        check(close(box.y1, 2.0), "level::setBox.2d.y1");
        check(close(box.y2, 3.0), "level::setBox.2d.y2");
    }

    {
        sed::TBox2 a{};
        sed::TBox2 b{};

        sed::level::setBox(a, 0.0, 1.0, 0.0, 1.0);
        sed::level::setBox(b, 2.0, 3.0, 0.0, 1.0);
        check(!sed::level::doBoxesIntersect(a, b), "level::doBoxesIntersect.2d.false");

        sed::level::setBox(b, 1.0, 2.0, -1.0, 0.5);
        check(sed::level::doBoxesIntersect(a, b), "level::doBoxesIntersect.2d.true");
    }

    {
        sed::TBox2 box{};
        sed::level::setBox(box, 0.0, 1.0, 0.0, 1.0);

        check(sed::level::isPointInBox(box, 0.0, 0.0), "level::isPointInBox.2d.origin");
        check(sed::level::isPointInBox(box, 1.0 + sed::CloseEnough * 0.5, 0.0), "level::isPointInBox.2d.epsilon.in");
        check(!sed::level::isPointInBox(box, 1.0 + sed::CloseEnough * 2.0, 0.0), "level::isPointInBox.2d.epsilon.out");
    }

    {
        sed::Sector sec{};
        sec.addVertex(0.0, 0.0, 0.0);
        sec.addVertex(2.0, 0.0, 0.0);
        sec.addVertex(0.0, 4.0, 0.0);

        const sed::TBox bbox = sed::level::sectorCalcBBox(sec);
        check(close(bbox.x1, 0.0) && close(bbox.x2, 2.0), "level::computeBBox.x");
        check(close(bbox.y1, 0.0) && close(bbox.y2, 4.0), "level::computeBBox.y");
        check(close(bbox.z1, 0.0) && close(bbox.z2, 0.0), "level::computeBBox.z");

        const sed::TVector center = sed::level::sectorCalcCenter(sec);
        check(closeVec(center, sed::TVector{ 2.0 / 3.0, 4.0 / 3.0, 0.0 }), "level::computeCenter");

        const sed::level::BoundingSphere sphere = sed::level::sectorCalcBoundingSphere(sec);
        check(closeVec(sphere.center, center), "level::computeBoundingSphere.center");
        check(sphere.radius > 0.0, "level::computeBoundingSphere.radius");

        const std::optional<sed::PointLight> pointLight =
            sed::level::sectorMakeDefaultIjimPointLight(sec);
        const double outerRange = std::sqrt(20.0);
        check(pointLight.has_value()
                && pointLight->color == sed::ColorWhite
                && closeVec(pointLight->position, center)
                && close(pointLight->minRange, outerRange)
                && close(pointLight->maxRange, outerRange * 0.25),
            "level::sectorMakeDefaultIjimPointLight uses centroid, AABB diagonal, and 4:1 falloff");

        sed::Sector degenerate{};
        degenerate.addVertex(1.0, 2.0, 3.0);
        degenerate.addVertex(1.0, 2.0, 3.0);
        check(!sed::level::sectorMakeDefaultIjimPointLight(degenerate),
            "level::sectorMakeDefaultIjimPointLight rejects degenerate geometry");

        sed::Sector nonFinite = sec;
        nonFinite.vertices.front().x = std::numeric_limits<double>::infinity();
        check(!sed::level::sectorMakeDefaultIjimPointLight(nonFinite),
            "level::sectorMakeDefaultIjimPointLight rejects non-finite geometry");
    }

    {
        sed::Sector sec{};
        sec.addVertex(0.0, 0.0, 0.0);
        sec.addVertex(1.0, 0.0, 0.0);
        sec.addVertex(0.0, 1.0, 0.0);

        sed::Surface surf{};
        surf.vertices = { 0, 1, 2 };

        surf.normal = sed::level::surfaceCalcNormalNewell(sec, surf);

        const sed::TBox bbox = sed::level::surfaceCalcBBox(sec, surf);
        check(close(bbox.x1, 0.0) && close(bbox.x2, 1.0), "level::computeSurfaceBBox.x");
        check(close(bbox.y1, 0.0) && close(bbox.y2, 1.0), "level::computeSurfaceBBox.y");

        check(closeVec(surf.normal, sed::TVector::up, kRotationEpsilon), "surfaceCalcNormalNewell");

        check(sed::level::surfaceIsPointOn(sec, surf, 0.25, 0.25, 0.0), "level::isPointOnSurface.true");
        check(!sed::level::surfaceIsPointOn(sec, surf, 2.0, 2.0, 0.0), "level::isPointOnSurface.false");
        check(close(sed::level::surfaceDistToPlaneAbs(sec, surf, 0.0, 0.0, 2.0), 2.0), "level::distToPlaneAbs");
    }

    {
        sed::Surface floor{};
        floor.surfaceFlags = 0u;
        floor.normal = sed::TVector::up;
        sed::level::surfaceCheckIfFloor(floor);
        check((floor.surfaceFlags & sed::SF_Floor) != 0, "surfaceCheckIfFloor.up.floor");
        check((floor.surfaceFlags & sed::SF_Collision) != 0, "surfaceCheckIfFloor.up.collision");

        sed::Surface threshold{};
        threshold.surfaceFlags = sed::SF_Floor | sed::SF_Collision;
        threshold.normal = sed::TVector{ 0.0, 0.0, std::cos(std::numbers::pi_v<double> / 4.0) };
        sed::level::surfaceCheckIfFloor(threshold);
        check((threshold.surfaceFlags & sed::SF_Floor) == 0, "surfaceCheckIfFloor.threshold.notFloor");
        check((threshold.surfaceFlags & sed::SF_Collision) != 0, "surfaceCheckIfFloor.threshold.keepsCollision");
    }

    {
        sed::Sector sec{};
        sec.addVertex(0.0, 0.0, 0.0);
        sec.addVertex(2.0, 0.0, 0.0);
        sec.addVertex(2.0, 1.0, 1.0);
        sec.addVertex(0.0, 1.0, 1.0);

        sed::Surface surf{};
        surf.vertices = { 0, 1, 2, 3 };

        const double invSqrt2 = 1.0 / std::sqrt(2.0);
        const sed::TVector normal = sed::level::surfaceCalcNormalNewell(sec, surf);
        check(closeVec(normal, sed::TVector{ 0.0, -invSqrt2, invSqrt2 }, kRotationEpsilon), "surfaceCalcNormalNewell.slopedQuad");

        sed::Surface reversed = surf;
        std::reverse(reversed.vertices.begin(), reversed.vertices.end());
        const sed::TVector reversedNormal = sed::level::surfaceCalcNormalNewell(sec, reversed);
        check(closeVec(reversedNormal, sed::TVector{ 0.0, invSqrt2, -invSqrt2 }, kRotationEpsilon), "surfaceCalcNormalNewell.reversed");

        sed::Surface invalid = surf;
        invalid.vertices[2] = 99;
        check(closeVec(sed::level::surfaceCalcNormalNewell(sec, invalid), sed::TVector::zero, kExactEpsilon), "surfaceCalcNormalNewell.invalidIndex");

        sed::Surface degenerate{};
        degenerate.vertices = { 0, 1, 1 };
        check(closeVec(sed::level::surfaceCalcNormalNewell(sec, degenerate), sed::TVector::zero, kExactEpsilon), "surfaceCalcNormalNewell.degenerate");
    }

    {
        sed::Sector sec{};
        sec.addVertex(0.0, 0.0, 0.0);
        sec.addVertex(1.0, 0.0, 0.0);
        sec.addVertex(1.0, 1.0, 0.0);
        sec.addVertex(0.0, 1.0, 0.0);

        sed::Surface surf{};
        surf.vertices = { 0, 1, 2, 3 };
        surf.normal = sed::level::surfaceCalcNormalNewell(sec, surf);

        check(sed::level::surfaceIsPlanar(sec, surf), "level::isSurfacePlanar.true");
        check(sed::level::surfaceIsConvex(sec, surf), "level::isSurfaceConvex.square");
    }

    {
        sed::Sector sec{};
        sec.addVertex(0.0, 0.0, 0.0);
        sec.addVertex(1.0, 0.0, 0.0);
        sec.addVertex(1.0, 1.0, 0.0);
        sec.addVertex(0.0, 1.0, 1.0);

        sed::Surface surf{};
        surf.vertices = { 0, 1, 2, 3 };
        surf.normal = sed::level::surfaceCalcNormalNewell(sec, surf);

        check(!sed::level::surfaceIsPlanar(sec, surf), "level::isSurfacePlanar.false");
    }

    {
        sed::Sector sec{};
        sec.addVertex(0.0, 0.0, 0.0);
        sec.addVertex(2.0, 0.0, 0.0);
        sec.addVertex(2.0, 2.0, 0.0);
        sec.addVertex(1.0, 1.0, 0.0);
        sec.addVertex(0.0, 2.0, 0.0);

        sed::Surface surf{};
        surf.vertices = { 0, 1, 2, 3, 4 };
        surf.normal = sed::level::surfaceCalcNormalNewell(sec, surf);

        check(sed::level::surfaceIsPlanar(sec, surf), "level::isSurfacePlanar.concave");
        check(!sed::level::surfaceIsConvex(sec, surf), "level::isSurfaceConvex.concave");
    }

    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;

        check(sed::level::planeLineIntersection(sed::TVector::up, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, -1.0, x, y, z),
              "level::planeLineIntersection.true");
        check(closeVec(sed::TVector{ x, y, z }, sed::TVector{ 0.0, 0.0, 0.0 }), "level::planeLineIntersection.point");

        x = 7.0;
        y = 8.0;
        z = 9.0;
        check(!sed::level::planeLineIntersection(sed::TVector::up, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 1.0, x, y, z),
              "level::planeLineIntersection.pointPlane.parallel.false");
        check(closeVec(sed::TVector{ x, y, z }, sed::TVector{ 7.0, 8.0, 9.0 }), "level::planeLineIntersection.pointPlane.falseKeepsOutput");
    }

    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;

        check(sed::level::planeLineIntersection(sed::TVector::up, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, -1.0, x, y, z),
              "level::planeLineIntersection.pointPlane.true");

        check(sed::level::planeLineIntersection(sed::TVector::up, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, -1.0, x, y, z),
              "level::planeLineIntersection.D.true");
        check(closeVec(sed::TVector{ x, y, z }, sed::TVector{ 0.0, 0.0, 0.0 }), "level::planeLineIntersection.D.point");

        x = 4.0;
        y = 5.0;
        z = 6.0;
        check(!sed::level::planeLineIntersection(sed::TVector::up, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 1.0, x, y, z),
              "level::planeLineIntersection.D.parallel.false");
        check(closeVec(sed::TVector{ x, y, z }, sed::TVector{ 4.0, 5.0, 6.0 }), "level::planeLineIntersection.D.falseKeepsOutput");
    }

    {
        sed::Level level{};
        (void)level.newSector();

        sed::Sector& sector = level.sectors[0];
        sector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 1.0, 1.0, 0.0, 0 },
            { 0.0, 1.0, 0.5, 0 },
        };

        sed::Surface surface{};
        surface.vertices    = { 0, 1, 2, 3 };
        surface.texVertices = {
            { 3.25f, -7.5f, sed::ColorF{ 0.1f, 0.2f, 0.3f, 0.4f } },
            { 17.0f, 4.75f, sed::ColorF{ 0.2f, 0.3f, 0.4f, 0.5f } },
            { -2.5f, 33.0f, sed::ColorF{ 0.3f, 0.4f, 0.5f, 0.6f } },
            { 91.25f, -18.5f, sed::ColorF{ 0.4f, 0.5f, 0.6f, 0.7f } },
        };
        surface.normal      = sed::TVector::up;
        const std::vector<sed::TexVertex> authoredTexVertices = surface.texVertices;
        sector.surfaces.push_back(surface);

        const bool ok = sed::level::levelFlattenSurface(level, sed::SurfaceRef{ 0, 0 });
        check(ok, "level::flattenSurface.true");
        check(close(level.sectors[0].vertices[3].z, 0.0, kExactEpsilon), "level::flattenSurface.v3.z");
        check(sed::level::surfaceIsPlanar(level.sectors[0], level.sectors[0].surfaces[0]), "level::flattenSurface.planar");
        const sed::Surface& flattened = level.sectors[0].surfaces[0];
        check(
            flattened.texVertices.size() == authoredTexVertices.size(),
            "level::flattenSurface.uvPreservation.count");
        for (std::size_t index = 0;
             index < flattened.texVertices.size() && index < authoredTexVertices.size();
             ++index)
        {
            check(
                flattened.texVertices[index].u == authoredTexVertices[index].u
                    && flattened.texVertices[index].v == authoredTexVertices[index].v
                    && flattened.texVertices[index].color == authoredTexVertices[index].color,
                "level::flattenSurface.uvPreservation.authoredCornerExact");
        }
    }

    {
        sed::Level level{};
        (void)level.newSector();

        sed::Sector& sector = level.sectors[0];
        sector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 1.0, 1.0, 0.0, 0 },
            { 0.0, 1.0, 5.0, 0 },
        };

        sed::Surface surface{};
        surface.vertices    = { 0, 1, 2, 3 };
        surface.texVertices = std::vector<sed::TexVertex>(surface.vertices.size());
        surface.normal      = sed::TVector::up;
        sector.surfaces.push_back(surface);

        const bool ok = sed::level::levelFlattenSurface(level, sed::SurfaceRef{ 0, 0 });
        check(!ok, "level::flattenSurface.false");
        check(close(level.sectors[0].vertices[3].z, 5.0, kExactEpsilon), "level::flattenSurface.no_move");
    }

    {
        sed::Level level{};
        constexpr int kSurfaceCount = 128;
        for (int i = 0; i < kSurfaceCount; ++i)
        {
            sed::Sector& sector = level.newSector();
            const double centerX = static_cast<double>(i) * 2.0;
            sector.vertices = {
                { centerX - 0.5, -0.5, 0.0, 0 },
                { centerX + 0.5, -0.5, 0.0, 1 },
                { centerX + 0.5,  0.5, 0.0, 2 },
                { centerX - 0.5,  0.5, 0.0, 3 },
            };
            sed::Surface surface{};
            surface.vertices = { 0, 1, 2, 3 };
            surface.normal = sed::TVector::up;
            sector.surfaces.push_back(std::move(surface));
        }

        sed::level::SurfacePickIndex index{};
        index.rebuild(level);
        const sed::TVector origin{ 84.0, 0.0, 1.0 };
        const sed::TVector direction{ 0.0, 0.0, -1.0 };
        const std::vector<sed::level::SurfacePickHit> exhaustive =
            sed::level::levelPickSurfaces(level, origin, direction, 2.0);
        const std::vector<sed::level::SurfacePickHit> accelerated =
            index.pickSurfaces(level, origin, direction, 2.0);
        check(accelerated.size() == exhaustive.size()
                && accelerated.size() == 1u
                && accelerated.front().surface.sector == exhaustive.front().surface.sector
                && accelerated.front().surface.surface == exhaustive.front().surface.surface
                && close(accelerated.front().distance, exhaustive.front().distance, kExactEpsilon),
            "level::surfacePickIndex preserves exact hit results");
        check(index.surfaceCount() == static_cast<std::size_t>(kSurfaceCount)
                && index.lastCandidateCount() < index.surfaceCount() / 4u,
            "level::surfacePickIndex rejects distant surface candidates");

        const std::vector<sed::level::SurfacePickHit> miss = index.pickSurfaces(
            level,
            sed::TVector{ -10.0, 0.0, 1.0 },
            direction,
            2.0);
        check(miss.empty() && index.lastCandidateCount() == 0u,
            "level::surfacePickIndex preserves misses without exact candidates");

        sed::level::SurfacePickIndex filteredIndex{};
        filteredIndex.rebuild(level, [](const std::int32_t sectorIndex)
            {
                return sectorIndex == 7 || sectorIndex == 42;
            });
        check(filteredIndex.surfaceCount() == 2u,
            "level::surfacePickIndex filtered rebuild stores only eligible sectors");
        const std::vector<sed::level::SurfacePickHit> filteredHit =
            filteredIndex.pickSurfaces(level, origin, direction, 2.0);
        check(filteredHit.size() == 1u
                && filteredHit.front().surface.sector == 42,
            "level::surfacePickIndex filtered rebuild preserves eligible exact hits");
        const std::vector<sed::level::SurfacePickHit> filteredMiss =
            filteredIndex.pickSurfaces(
                level,
                sed::TVector{ 16.0, 0.0, 1.0 },
                direction,
                2.0);
        check(filteredMiss.empty(),
            "level::surfacePickIndex filtered rebuild rejects excluded sector geometry");

        sed::Level tinyLevel{};
        sed::Sector& tinySector = tinyLevel.newSector();
        tinySector.vertices = {
            { 0.0,   0.0,   0.0, 0 },
            { 0.001, 0.0,   0.0, 1 },
            { 0.001, 0.001, 0.0, 2 },
            { 0.0,   0.001, 0.0, 3 },
        };
        sed::Surface tinySurface{};
        tinySurface.vertices = { 0, 1, 2, 3 };
        tinySurface.normal = sed::TVector::up;
        tinySector.surfaces.push_back(std::move(tinySurface));

        sed::level::SurfacePickIndex tinyIndex{};
        tinyIndex.rebuild(tinyLevel);
        const sed::TVector toleranceOrigin{ -0.05, 0.0005, 1.0 };
        const std::vector<sed::level::SurfacePickHit> tinyExhaustive =
            sed::level::levelPickSurfaces(tinyLevel, toleranceOrigin, direction, 2.0);
        const std::vector<sed::level::SurfacePickHit> tinyAccelerated =
            tinyIndex.pickSurfaces(tinyLevel, toleranceOrigin, direction, 2.0);
        check(tinyExhaustive.size() == 1u
                && tinyAccelerated.size() == tinyExhaustive.size()
                && close(
                    tinyAccelerated.front().distance,
                    tinyExhaustive.front().distance,
                    kExactEpsilon),
            "level::surfacePickIndex preserves legacy tolerance outside tiny raw bounds");

        // The legacy picker uses one plane through vertex zero, but evaluates
        // edge half-spaces using every (possibly non-planar) vertex. Those edge
        // lines correspond to projected vertices and can extend beyond the raw
        // vertex AABB, so the broad phase must account for that displacement.
        sed::Level nonPlanarLevel{};
        sed::Sector& nonPlanarSector = nonPlanarLevel.newSector();
        nonPlanarSector.vertices = {
            { 0.0, 0.0,  0.0, 0 },
            { 0.4, 0.0, -1.6, 1 },
            { 0.4, 1.0, -1.6, 2 },
            { 0.0, 1.0,  0.0, 3 },
        };
        sed::Surface nonPlanarSurface{};
        nonPlanarSurface.vertices = { 0, 1, 2, 3 };
        nonPlanarSurface.normal = sed::TVector{ 1.0, 0.0, 1.0 };
        nonPlanarSector.surfaces.push_back(std::move(nonPlanarSurface));

        sed::level::SurfacePickIndex nonPlanarIndex{};
        nonPlanarIndex.rebuild(nonPlanarLevel);
        const sed::TVector nonPlanarOrigin{ 0.8, 0.5, 0.2 };
        const std::vector<sed::level::SurfacePickHit> nonPlanarExhaustive =
            sed::level::levelPickSurfaces(nonPlanarLevel, nonPlanarOrigin, direction, 2.0);
        const std::vector<sed::level::SurfacePickHit> nonPlanarAccelerated =
            nonPlanarIndex.pickSurfaces(nonPlanarLevel, nonPlanarOrigin, direction, 2.0);
        check(nonPlanarExhaustive.size() == 1u
                && nonPlanarAccelerated.size() == nonPlanarExhaustive.size()
                && close(
                    nonPlanarAccelerated.front().distance,
                    nonPlanarExhaustive.front().distance,
                    kExactEpsilon),
            "level::surfacePickIndex preserves non-planar projected edge bounds");

        // Opposite edges may be almost parallel without creating an
        // ill-conditioned corner.  The broad phase must measure only the
        // consecutive supporting edges that actually meet at polygon
        // vertices.  This fixture also checks the legacy reversed-winding
        // acceptance, the CloseEnough tolerance halo, and a distant ray.
        constexpr std::int32_t kNearParallelSurfaceCount = 32;
        constexpr std::int32_t kNearParallelTarget = 19;
        constexpr double kNearParallelSpacing = 4.0;
        constexpr double kNearParallelEdgeDelta = 1.0e-7;
        const auto makeNearParallelSector = [=](const bool reverse)
        {
            sed::Sector sector{};
            const double centerX = 0.0;
            sector.vertices = {
                { centerX - 0.75, -0.50, 0.0, 0 },
                { centerX + 0.75, -0.50 + kNearParallelEdgeDelta, 0.0, 1 },
                { centerX + 1.00, -0.25, 0.0, 2 },
                { centerX + 1.00,  0.25, 0.0, 3 },
                { centerX + 0.75,  0.50, 0.0, 4 },
                { centerX - 0.75,  0.50, 0.0, 5 },
                { centerX - 1.00,  0.25, 0.0, 6 },
                { centerX - 1.00, -0.25, 0.0, 7 },
            };
            sed::Surface surface{};
            surface.normal = sed::TVector::up;
            surface.vertices = { 0, 1, 2, 3, 4, 5, 6, 7 };
            if (reverse)
            {
                std::reverse(surface.vertices.begin(), surface.vertices.end());
            }
            sector.surfaces.push_back(std::move(surface));
            return sector;
        };

        sed::Level nearParallelLevel{};
        nearParallelLevel.sectors.reserve(kNearParallelSurfaceCount);
        for (std::int32_t index = 0; index < kNearParallelSurfaceCount; ++index)
        {
            sed::Sector sector = makeNearParallelSector(false);
            const double offset = static_cast<double>(index) * kNearParallelSpacing;
            for (sed::Vertex& vertex : sector.vertices)
            {
                vertex.x += offset;
            }
            nearParallelLevel.sectors.push_back(std::move(sector));
        }

        sed::level::SurfacePickIndex nearParallelIndex{};
        nearParallelIndex.rebuild(nearParallelLevel);
        const double targetX = static_cast<double>(kNearParallelTarget) * kNearParallelSpacing;
        const sed::TVector down{ 0.0, 0.0, -1.0 };
        const auto checkNearParallelQuery = [&](const sed::Level& level,
                                                sed::level::SurfacePickIndex& index,
                                                const sed::TVector& origin,
                                                const std::size_t expectedHitCount,
                                                const char* const parityMessage,
                                                const char* const candidateMessage)
        {
            const std::vector<sed::level::SurfacePickHit> exhaustive =
                sed::level::levelPickSurfaces(level, origin, down, 2000.0);
            const std::vector<sed::level::SurfacePickHit> accelerated =
                index.pickSurfaces(level, origin, down, 2000.0);
            const bool sameHits = accelerated.size() == exhaustive.size()
                && accelerated.size() == expectedHitCount
                && std::equal(
                    accelerated.begin(),
                    accelerated.end(),
                    exhaustive.begin(),
                    [](const sed::level::SurfacePickHit& first,
                       const sed::level::SurfacePickHit& second)
                    {
                        return first.surface == second.surface
                            && closeVec(first.position, second.position, kExactEpsilon)
                            && close(first.distance, second.distance, kExactEpsilon);
                    });
            check(sameHits, parityMessage);
            check(index.lastCandidateCount() == expectedHitCount, candidateMessage);
        };

        checkNearParallelQuery(
            nearParallelLevel,
            nearParallelIndex,
            sed::TVector{ targetX, 0.0, 1.0 },
            1u,
            "level::surfacePickIndex preserves near-parallel exact hit parity",
            "level::surfacePickIndex bounds near-parallel candidates to the target surface");
        checkNearParallelQuery(
            nearParallelLevel,
            nearParallelIndex,
            sed::TVector{ targetX + 1.0 + sed::CloseEnough * 0.5, 0.0, 1.0 },
            1u,
            "level::surfacePickIndex preserves the CloseEnough tolerance halo",
            "level::surfacePickIndex includes the tolerance halo without distant candidates");
        checkNearParallelQuery(
            nearParallelLevel,
            nearParallelIndex,
            sed::TVector{ targetX, 0.0, 1000.0 },
            1u,
            "level::surfacePickIndex preserves far-origin exact hit parity",
            "level::surfacePickIndex bounds far-origin candidates to the target surface");

        sed::Level reversedNearParallelLevel{};
        reversedNearParallelLevel.sectors.reserve(kNearParallelSurfaceCount);
        for (std::int32_t index = 0; index < kNearParallelSurfaceCount; ++index)
        {
            sed::Sector sector = makeNearParallelSector(true);
            const double offset = static_cast<double>(index) * kNearParallelSpacing;
            for (sed::Vertex& vertex : sector.vertices)
            {
                vertex.x += offset;
            }
            reversedNearParallelLevel.sectors.push_back(std::move(sector));
        }

        sed::level::SurfacePickIndex reversedNearParallelIndex{};
        reversedNearParallelIndex.rebuild(reversedNearParallelLevel);
        checkNearParallelQuery(
            reversedNearParallelLevel,
            reversedNearParallelIndex,
            sed::TVector{ targetX, 0.0, 1.0 },
            1u,
            "level::surfacePickIndex preserves reversed-winding exact hit parity",
            "level::surfacePickIndex bounds reversed-winding candidates to the target surface");

        // A dense rounded grid keeps the almost-parallel midpoint corners in
        // separate, disjoint surfaces. The broad phase must use a certified
        // bound for each face rather than putting every such face on its
        // exhaustive always-test list. Check exact hit fields for both
        // windings at the center, an interior offset, and a gap miss.
        constexpr int kRoundedGridRows = 24;
        constexpr int kRoundedGridColumns = 24;
        constexpr double kRoundedGridSpacing = 3.0;
        constexpr double kRoundedGridEdgeDelta = 1.0e-7;
        constexpr double kRoundedGridMidpointOffset = 1.0e-6;
        const auto makeRoundedNearParallelSector = [&](const bool reverse)
        {
            std::array<sed::TVector, 8> corners{
                sed::TVector{ -0.75, -0.50, 0.0 },
                sed::TVector{  0.75, -0.50, 0.0 },
                sed::TVector{  1.00, -0.25, 0.0 },
                sed::TVector{  1.00,  0.25, 0.0 },
                sed::TVector{  0.75,  0.50, 0.0 },
                sed::TVector{ -0.75,  0.50, 0.0 },
                sed::TVector{ -1.00,  0.25, 0.0 },
                sed::TVector{ -1.00, -0.25, 0.0 },
            };
            corners[1].y += kRoundedGridEdgeDelta;

            std::vector<sed::TVector> polygon{};
            polygon.reserve(corners.size() * 2u);
            for (std::size_t index = 0; index < corners.size(); ++index)
            {
                const sed::TVector& first = corners[index];
                const sed::TVector& second = corners[(index + 1u) % corners.size()];
                polygon.push_back(first);
                sed::TVector midpoint = (first + second) * 0.5;
                const double edgeX = second.x - first.x;
                const double edgeY = second.y - first.y;
                const double edgeLength = std::hypot(edgeX, edgeY);
                midpoint.x += kRoundedGridMidpointOffset * edgeY / edgeLength;
                midpoint.y -= kRoundedGridMidpointOffset * edgeX / edgeLength;
                polygon.push_back(midpoint);
            }

            sed::Sector sector{};
            sector.vertices.reserve(polygon.size());
            for (std::size_t index = 0; index < polygon.size(); ++index)
            {
                const sed::TVector& point = polygon[index];
                sector.vertices.push_back(sed::Vertex{
                    point.x,
                    point.y,
                    point.z,
                    static_cast<std::int32_t>(index),
                });
            }
            sed::Surface surface{};
            surface.normal = sed::TVector::up;
            surface.vertices.reserve(polygon.size());
            for (std::size_t index = 0; index < polygon.size(); ++index)
            {
                surface.vertices.push_back(static_cast<std::int32_t>(index));
            }
            if (reverse)
            {
                std::reverse(surface.vertices.begin(), surface.vertices.end());
            }
            sector.surfaces.push_back(std::move(surface));
            return sector;
        };
        const auto makeRoundedNearParallelLevel = [&](const bool reverse)
        {
            sed::Level level{};
            level.sectors.reserve(static_cast<std::size_t>(
                kRoundedGridRows * kRoundedGridColumns));
            for (int row = 0; row < kRoundedGridRows; ++row)
            {
                for (int column = 0; column < kRoundedGridColumns; ++column)
                {
                    sed::Sector sector = makeRoundedNearParallelSector(reverse);
                    const double offsetX = static_cast<double>(column)
                        * kRoundedGridSpacing;
                    const double offsetY = static_cast<double>(row)
                        * kRoundedGridSpacing;
                    for (sed::Vertex& vertex : sector.vertices)
                    {
                        vertex.x += offsetX;
                        vertex.y += offsetY;
                    }
                    level.sectors.push_back(std::move(sector));
                }
            }
            return level;
        };
        const auto checkRoundedNearParallelQuery = [=](
            const sed::Level& level,
            sed::level::SurfacePickIndex& index,
            const sed::TVector& origin,
            const std::size_t expectedHitCount,
            const std::size_t expectedCandidateCount,
            const char* const parityMessage,
            const char* const candidateMessage)
        {
            const std::vector<sed::level::SurfacePickHit> exhaustive =
                sed::level::levelPickSurfaces(level, origin, down, 4.0);
            const std::vector<sed::level::SurfacePickHit> accelerated =
                index.pickSurfaces(level, origin, down, 4.0);
            const bool sameHits = accelerated.size() == exhaustive.size()
                && accelerated.size() == expectedHitCount
                && std::equal(
                    accelerated.begin(),
                    accelerated.end(),
                    exhaustive.begin(),
                    [](const sed::level::SurfacePickHit& first,
                       const sed::level::SurfacePickHit& second)
                    {
                        return first.surface == second.surface
                            && closeVec(first.position, second.position, kExactEpsilon)
                            && close(first.distance, second.distance, kExactEpsilon);
                    });
            check(sameHits, parityMessage);
            check(index.lastCandidateCount() == expectedCandidateCount, candidateMessage);
        };
        const auto checkRoundedNearParallelLevel = [&](const bool reverse,
                                                        const char* const windingLabel)
        {
            sed::Level level = makeRoundedNearParallelLevel(reverse);
            sed::level::SurfacePickIndex index{};
            index.rebuild(level);
            constexpr int kTargetRow = 8;
            constexpr int kTargetColumn = 12;
            const double centerX = static_cast<double>(kTargetColumn)
                * kRoundedGridSpacing;
            const double centerY = static_cast<double>(kTargetRow)
                * kRoundedGridSpacing;
            const sed::TVector centerOrigin{ centerX, centerY, 2.0 };
            const sed::TVector interiorOrigin{ centerX + 0.5, centerY + 0.125, 2.0 };
            const sed::TVector gapOrigin{ centerX + 1.4, centerY, 2.0 };
            checkRoundedNearParallelQuery(
                level,
                index,
                centerOrigin,
                1u,
                1u,
                windingLabel,
                "level::surfacePickIndex bounds dense rounded center to one candidate");
            checkRoundedNearParallelQuery(
                level,
                index,
                interiorOrigin,
                1u,
                1u,
                windingLabel,
                "level::surfacePickIndex bounds dense rounded interior to one candidate");
            checkRoundedNearParallelQuery(
                level,
                index,
                gapOrigin,
                0u,
                0u,
                "level::surfacePickIndex preserves dense rounded gap miss parity",
                "level::surfacePickIndex rejects dense rounded gap candidates");
        };
        checkRoundedNearParallelLevel(
            false,
            "level::surfacePickIndex preserves dense rounded forward exact hit fields");
        checkRoundedNearParallelLevel(
            true,
            "level::surfacePickIndex preserves dense rounded reversed exact hit fields");

        // A non-unit cached normal scales the legacy edge tolerance.  The
        // accepted point below is outside the raw x bounds by more than
        // CloseEnough, so a normalized (rather than per-edge) tolerance would
        // lose the exact hit in the broad phase.  Forward and reversed loops
        // exercise the positive and negative all-edge branches respectively.
        const auto makeScaledNearParallelSector = [&](const bool reverse)
        {
            sed::Sector sector = makeNearParallelSector(reverse);
            sector.surfaces.front().normal = sed::TVector{ 0.0, 0.0, 0.1 };
            return sector;
        };
        constexpr double kScaledNormal = 0.1;
        constexpr double kAcceptedOutsideOffset = 0.001;
        sed::Level scaledNearParallelLevel{};
        scaledNearParallelLevel.sectors.push_back(makeScaledNearParallelSector(false));
        sed::level::SurfacePickIndex scaledNearParallelIndex{};
        scaledNearParallelIndex.rebuild(scaledNearParallelLevel);
        checkNearParallelQuery(
            scaledNearParallelLevel,
            scaledNearParallelIndex,
            sed::TVector{ -1.0 - kAcceptedOutsideOffset, 0.0, 1.0 },
            1u,
            "level::surfacePickIndex preserves non-unit-normal positive-branch parity",
            "level::surfacePickIndex scales positive-branch edge tolerance by its normal");

        sed::Level reversedScaledNearParallelLevel{};
        reversedScaledNearParallelLevel.sectors.push_back(makeScaledNearParallelSector(true));
        sed::level::SurfacePickIndex reversedScaledNearParallelIndex{};
        reversedScaledNearParallelIndex.rebuild(reversedScaledNearParallelLevel);
        checkNearParallelQuery(
            reversedScaledNearParallelLevel,
            reversedScaledNearParallelIndex,
            sed::TVector{ -1.0 - kAcceptedOutsideOffset, 0.0, 1.0 },
            1u,
            "level::surfacePickIndex preserves non-unit-normal negative-branch parity",
            "level::surfacePickIndex scales negative-branch edge tolerance by its normal");

        // A rounded, mixed-side contour cannot use the supporting-polygon
        // shortcut. It must stay in the conservative fallback while retaining
        // the exact picker's result.
        sed::Level mixedSideLevel{};
        sed::Sector& mixedSideSector = mixedSideLevel.newSector();
        mixedSideSector.vertices = {
            { -1.0, 0.0, 0.0, 0 },
            {  0.0, -1.0e-6, 0.0, 1 },
            {  1.0, 0.0, 0.0, 2 },
            {  1.0, 2.0, 0.0, 3 },
            {  0.0, 2.0, 0.0, 4 },
            {  0.0, 1.0, 0.0, 5 },
            { -1.0, 1.0, 0.0, 6 },
        };
        sed::Surface mixedSideSurface{};
        mixedSideSurface.vertices = { 0, 1, 2, 3, 4, 5, 6 };
        mixedSideSurface.normal = sed::TVector{ 0.0, 0.0, kScaledNormal };
        mixedSideSector.surfaces.push_back(std::move(mixedSideSurface));
        sed::level::SurfacePickIndex mixedSideIndex{};
        mixedSideIndex.rebuild(mixedSideLevel);
        const sed::TVector mixedSideOrigin{ 0.5, 0.5, 1.0 };
        const std::vector<sed::level::SurfacePickHit> mixedSideExhaustive =
            sed::level::levelPickSurfaces(mixedSideLevel, mixedSideOrigin, down, 2.0);
        const std::vector<sed::level::SurfacePickHit> mixedSideAccelerated =
            mixedSideIndex.pickSurfaces(mixedSideLevel, mixedSideOrigin, down, 2.0);
        check(mixedSideExhaustive.size() == mixedSideAccelerated.size()
                && mixedSideExhaustive.size() == 1u
                && mixedSideExhaustive.front().surface == mixedSideAccelerated.front().surface
                && closeVec(
                    mixedSideExhaustive.front().position,
                    mixedSideAccelerated.front().position,
                    kExactEpsilon)
                && close(
                    mixedSideExhaustive.front().distance,
                    mixedSideAccelerated.front().distance,
                    kExactEpsilon),
            "level::surfacePickIndex preserves rounded mixed-side exact parity");
        check(mixedSideIndex.lastCandidateCount() == 1u,
            "level::surfacePickIndex keeps rounded mixed-side fallback candidate");

        const sed::TVector mixedSideRejectedOrigin{ 0.5, 1.5, 1.0 };
        const std::vector<sed::level::SurfacePickHit> mixedSideRejectedExhaustive =
            sed::level::levelPickSurfaces(
                mixedSideLevel,
                mixedSideRejectedOrigin,
                down,
                2.0);
        const std::vector<sed::level::SurfacePickHit> mixedSideRejectedAccelerated =
            mixedSideIndex.pickSurfaces(
                mixedSideLevel,
                mixedSideRejectedOrigin,
                down,
                2.0);
        check(mixedSideRejectedExhaustive.empty()
                && mixedSideRejectedAccelerated.empty(),
            "level::surfacePickIndex preserves mixed-side rejection outside its kernel");

        // A collinear/degenerate face remains in the exhaustive path, but it
        // must retain exact picker parity and never make malformed geometry
        // poison the index's surrounding entries.
        sed::Level degenerateLevel{};
        sed::Sector& degenerateSector = degenerateLevel.newSector();
        degenerateSector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 1 },
            { 2.0, 0.0, 0.0, 2 },
        };
        sed::Surface degenerateSurface{};
        degenerateSurface.normal = sed::TVector::up;
        degenerateSurface.vertices = { 0, 1, 2 };
        degenerateSector.surfaces.push_back(std::move(degenerateSurface));
        sed::level::SurfacePickIndex degenerateIndex{};
        degenerateIndex.rebuild(degenerateLevel);
        checkNearParallelQuery(
            degenerateLevel,
            degenerateIndex,
            sed::TVector{ 0.0, 0.0, 1.0 },
            1u,
            "level::surfacePickIndex preserves degenerate exact hit parity",
            "level::surfacePickIndex keeps a degenerate face in its bounded fallback");
    }

    {
        const sed::Sector sec = makeTestCubeSector(0.0);
        sed::Level level{};
        level.sectors.push_back(sec);

        check(sed::level::sectorContainsPoint(sec, 0.0, 0.0, 0.0), "level::isInSector.inside");
        check(sed::level::sectorContainsPoint(sec, 1.0, 0.0, 0.0), "level::isInSector.on_surface");
        check(!sed::level::sectorContainsPoint(sec, 2.0, 0.0, 0.0), "level::isInSector.outside");
        check(sed::level::sectorContainsPointNoSides(sec, 0.0, 0.0, 0.0),
            "level::sectorContainsPointNoSides accepts a strict interior point");
        check(!sed::level::sectorContainsPointNoSides(sec, 1.0, 0.0, 0.0)
                && !sed::level::sectorContainsPointNoSides(sec, 1.0, 1.0, 0.0)
                && !sed::level::sectorContainsPointNoSides(sec, 1.0, 1.0, 1.0),
            "level::sectorContainsPointNoSides excludes face, edge, and vertex boundaries");
        check(sed::level::sectorContainsPointNoSides(sec, 0.998, 0.0, 0.0)
                && !sed::level::sectorContainsPointNoSides(sec, 0.9995, 0.0, 0.0),
            "level::sectorContainsPointNoSides applies its documented strict-interior tolerance");
        check(!sed::level::sectorContainsPointNoSides(
                    sec,
                    std::numeric_limits<double>::quiet_NaN(),
                    0.0,
                    0.0)
                && !sed::level::sectorContainsPointNoSides(
                    sec,
                    0.0,
                    std::numeric_limits<double>::infinity(),
                    0.0)
                && !sed::level::sectorContainsPointNoSides(
                    sec,
                    0.0,
                    0.0,
                    -std::numeric_limits<double>::infinity())
                && sed::level::sectorContainsPointNoSides(
                    sed::Sector{},
                    0.0,
                    0.0,
                    0.0),
            "level::sectorContainsPointNoSides rejects non-finite coordinates and preserves empty-sector compatibility");

        sed::Sector emptySurfaceSector = sec;
        for ( sed::Surface& surface : emptySurfaceSector.surfaces )
        {
            surface.vertices.clear();
        }

        sed::Sector negativeVertexSector = sec;
        negativeVertexSector.surfaces.front().vertices.front() = -1;

        sed::Sector highVertexSector = sec;
        highVertexSector.surfaces.front().vertices.front() =
            static_cast<std::int32_t>(highVertexSector.vertices.size());

        sed::Sector nonFiniteVertexSector = sec;
        nonFiniteVertexSector.vertices.front().x =
            std::numeric_limits<double>::quiet_NaN();

        sed::Sector nonFiniteNormalSector = sec;
        nonFiniteNormalSector.surfaces.front().normal.x =
            std::numeric_limits<double>::infinity();

        check(sed::level::sectorContainsPoint(emptySurfaceSector, 0.0, 0.0, 0.0)
                && sed::level::sectorContainsPointNoSides(
                    emptySurfaceSector,
                    0.0,
                    0.0,
                    0.0)
                && !sed::level::sectorContainsPoint(
                    negativeVertexSector,
                    0.0,
                    0.0,
                    0.0)
                && !sed::level::sectorContainsPoint(
                    highVertexSector,
                    0.0,
                    0.0,
                    0.0)
                && !sed::level::sectorContainsPoint(
                    nonFiniteVertexSector,
                    0.0,
                    0.0,
                    0.0)
                && !sed::level::sectorContainsPoint(
                    nonFiniteNormalSector,
                    0.0,
                    0.0,
                    0.0),
            "level::sectorContainsPoint preserves empty-plane compatibility but rejects malformed or non-finite planes");
        check(sed::level::levelFindSectorForXYZ(level, 0.0, 0.0, 0.0) == 0, "level::findSectorForXYZ.inside");
        check(sed::level::levelFindSectorForXYZ(level, 2.0, 0.0, 0.0) == -1, "level::findSectorForXYZ.outside");

        const sed::Sector secFar = makeTestCubeSector(3.0);
        check(!sed::level::sectorDoOverlap(sec, secFar), "level::doSectorsOverlap.false");

        const sed::Sector secNear = makeTestCubeSector(0.5);
        check(sed::level::sectorDoOverlap(sec, secNear), "level::doSectorsOverlap.true");

        check(sed::level::sectorIsConvex(sec), "level::sectorIsConvex.true");

        sed::Sector flipped = sec;
        std::reverse(flipped.surfaces[0].vertices.begin(), flipped.surfaces[0].vertices.end());
        flipped.surfaces[0].normal = sed::level::surfaceCalcNormalNewell(flipped, flipped.surfaces[0]);
        check(!sed::level::sectorIsConvex(flipped), "level::sectorIsConvex.false");
    }

    {
        sed::Level level{};
        constexpr std::int32_t kSectorCount = 512;
        constexpr std::int32_t kTargetSector = 321;
        for (std::int32_t i = 0; i < kSectorCount; ++i)
        {
            level.sectors.push_back(makeTestCubeSector(
                static_cast<double>(i) * 4.0));
        }

        sed::level::SectorContainmentIndex index{};
        index.rebuild(level);
        const double targetX = static_cast<double>(kTargetSector) * 4.0;
        check(index.matchesLevel(level)
                && index.sectorCount() == static_cast<std::size_t>(kSectorCount)
                && index.findContainingSector(level, targetX, 0.0, 0.0) == kTargetSector,
            "level::sectorContainmentIndex preserves exact containment");
        check(index.lastCandidateCount() < 32u,
            "level::sectorContainmentIndex rejects distant sectors before exact tests");
        check(index.findContainingSector(
                    level,
                    targetX,
                    0.0,
                    0.0,
                    kTargetSector) == kTargetSector
                && index.lastCandidateCount() == 1u,
            "level::sectorContainmentIndex retains preferred-sector fast path");
        check(index.findContainingSector(level, -20.0, 0.0, 0.0) == -1
                && index.lastCandidateCount() == 0u,
            "level::sectorContainmentIndex preserves misses without exact tests");

        // A same-sized document must not be mistaken for the document that
        // supplied the index.  Comparing only sector counts would reuse stale
        // bounds during a staged level/recovery restore and could assign the
        // camera (and its sector sound) to the wrong sector.
        sed::Level sameSizeDifferentLevel{};
        sameSizeDifferentLevel.sectors.reserve(kSectorCount);
        for (std::int32_t i = 0; i < kSectorCount; ++i)
        {
            sameSizeDifferentLevel.sectors.push_back(makeTestCubeSector(
                10000.0 + static_cast<double>(i) * 4.0));
        }
        check(sameSizeDifferentLevel.sectors.size() == level.sectors.size()
                && !index.matchesLevel(sameSizeDifferentLevel),
            "level::sectorContainmentIndex rejects same-size foreign level");

        sed::Level overlap{};
        overlap.sectors.push_back(makeTestCubeSector(0.0));
        overlap.sectors.push_back(makeTestCubeSector(0.5));
        index.rebuild(overlap);
        check(index.findContainingSector(overlap, 0.5, 0.0, 0.0) == 0
                && index.findContainingSector(overlap, 0.5, 0.0, 0.0, 1) == 1,
            "level::sectorContainmentIndex preserves deterministic and preferred overlap ownership");

        sed::Level toleranceBoundary{};
        toleranceBoundary.sectors.push_back(makeTestCubeSector(0.0));
        index.rebuild(toleranceBoundary);
        const double beyondFaceInsideTolerance = 5.0e-6;
        const double beyondFaceOutsideTolerance = 1.1e-5;
        const std::int32_t highFaceGlobal = sed::level::levelFindSectorForXYZ(
            toleranceBoundary,
            1.0 + beyondFaceInsideTolerance,
            0.0,
            0.0);
        const std::int32_t lowFaceGlobal = sed::level::levelFindSectorForXYZ(
            toleranceBoundary,
            -1.0 - beyondFaceInsideTolerance,
            0.0,
            0.0);
        check(highFaceGlobal == 0
                && index.findContainingSector(
                    toleranceBoundary,
                    1.0 + beyondFaceInsideTolerance,
                    0.0,
                    0.0) == highFaceGlobal
                && lowFaceGlobal == 0
                && index.findContainingSector(
                    toleranceBoundary,
                    -1.0 - beyondFaceInsideTolerance,
                    0.0,
                    0.0) == lowFaceGlobal,
            "level::sectorContainmentIndex matches global containment within face tolerance on both X bounds");
        const std::int32_t highOutsideGlobal = sed::level::levelFindSectorForXYZ(
            toleranceBoundary,
            1.0 + beyondFaceOutsideTolerance,
            0.0,
            0.0);
        const std::int32_t lowOutsideGlobal = sed::level::levelFindSectorForXYZ(
            toleranceBoundary,
            -1.0 - beyondFaceOutsideTolerance,
            0.0,
            0.0);
        check(highOutsideGlobal == -1
                && index.findContainingSector(
                    toleranceBoundary,
                    1.0 + beyondFaceOutsideTolerance,
                    0.0,
                    0.0) == highOutsideGlobal
                && lowOutsideGlobal == -1
                && index.findContainingSector(
                    toleranceBoundary,
                    -1.0 - beyondFaceOutsideTolerance,
                    0.0,
                    0.0) == lowOutsideGlobal,
            "level::sectorContainmentIndex matches global misses beyond face tolerance on both X bounds");
    }
}

} // namespace sed::tests
