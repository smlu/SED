#include "test_common.hpp"

#include "sed/level/geometry.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace sed::tests
{

void runLevelPickingTests()
{
    for ( const double directionScale : {1.0, 1e-200, 1e200} )
    {
        sed::Level level{};
        auto& sector = level.newSector();
        sector.newVertex(3, 0.5, 0);
        double x = 71, y = 72, z = 73, distance = 74;
        check(sed::level::getNearestVertexToLine(sector.vertices, 0, 0, 0,
                  {directionScale, 0, 0}, 1, x, y, z, distance)
                && x == 3 && y == 0.5 && z == 0 && distance == 0.25,
            "geometry contract: scaled finite nearest direction retains every analytic result");
        sed::TVector nearest{71, 72, 73};
        check(sed::level::levelGetNearestWorldVertexToLine(level, 0, 0, 0,
                  {directionScale, 0, 0}, 1, nearest) && nearest == sed::TVector{3, 0.5, 0},
            "geometry contract: world nearest direction scaling retains exact result");
    }
    for ( const double invalid : {std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()} )
    for ( std::size_t slot = 0; slot < 7; ++slot )
    {
        const std::array<sed::Vertex, 1> vertices{{{3, 0.5, 0, 0}}};
        std::array<double, 7> input{{0, 0, 0, 1, 0, 0, 1}};
        input[slot] = invalid;
        double x = 71, y = 72, z = 73, distance = 74;
        check(!sed::level::getNearestVertexToLine(vertices, input[0], input[1], input[2],
                  {input[3], input[4], input[5]}, input[6], x, y, z, distance)
                && x == 71 && y == 72 && z == 73 && distance == 74,
            "geometry contract: every nonfinite nearest query input rejects with all outputs unchanged");
    }
    {
        const std::array<sed::Vertex, 1> vertices{{{3, 0.5, 0, 0}}};
        double x = 71, y = 72, z = 73, distance = 74;
        check(!sed::level::getNearestVertexToLine(vertices, 0, 0, 0, {1, 0, 0},
                  -1, x, y, z, distance) && x == 71 && y == 72 && z == 73 && distance == 74,
            "geometry contract: negative nearest radius rejects without output mutation");
    }
    for ( const int invalidCorner : {0, 1, 2, 3} )
    {
        sed::Sector sector{};
        sector.vertices = {{0, 0, 0, 0}, {1, 0, 0, 0}, {0, 1, 0, 0}};
        sed::Surface face{};
        face.vertices = {0, 1, 2};
        face.normal = {0, 0, 1};
        if ( invalidCorner == 0 )
        {
            face.vertices[2] = 99;
        }
        else
        {
            const std::array<double, 3> values{{std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}};
            sector.vertices[2].y = values[static_cast<std::size_t>(invalidCorner - 1)];
        }
        sed::Thing thing{};
        thing.bbox.z1 = -2;
        sed::TVector position{71, 72, 73};
        double pitch = 74, yaw = 75, roll = 76;
        check(!sed::level::thingCalcPoseOnSurface(thing, sector, face, position, pitch, yaw, roll)
                && position == sed::TVector{71, 72, 73} && pitch == 74 && yaw == 75 && roll == 76,
            "geometry contract: pose rejects a malformed late corner without publishing partial marker mean");
    }
    {
        sed::Sector sector{};
        sector.vertices = {{0, 0, 0, 0}, {1, 0, 0, 0}, {2, 1, 0, 0}};
        sed::Surface face{};
        face.vertices = {0, 1, 2};
        face.normal = {0, 0, 1};
        sed::TVector xAxis{71, 72, 73}, yAxis{74, 75, 76};
        double minx = 77, miny = 78, maxx = 79, maxy = 80;
        check(sed::level::surfaceCalcAxesAndBounds(sector, face, xAxis, yAxis, minx, miny, maxx, maxy)
                && xAxis == sed::TVector{1, 0, 0} && yAxis == sed::TVector{0, 1, 0}
                && minx == 0 && miny == 0 && maxx == 2 && maxy == 1,
            "geometry contract: projected bounds include the origin corner and return all fixed axes/extents");
    }
    for ( const int invalidCase : {0, 1, 2, 3} )
    {
        sed::Sector sector{};
        sector.vertices = {{0, 0, 0, 0}, {1, 0, 0, 0}, {2, 1, 0, 0}};
        sed::Surface face{};
        face.vertices = {0, 1, 2};
        face.normal = {0, 0, 1};
        if ( invalidCase == 0 )
        {
            face.vertices.clear();
        }
        else if ( invalidCase == 1 )
        {
            face.vertices[2] = 99;
        }
        else if ( invalidCase == 2 )
        {
            sector.vertices[2].y = std::numeric_limits<double>::quiet_NaN();
        }
        else
        {
            face.normal.z = std::numeric_limits<double>::infinity();
        }
        sed::TVector xAxis{71, 72, 73}, yAxis{74, 75, 76};
        double minx = 77, miny = 78, maxx = 79, maxy = 80;
        check(!sed::level::surfaceCalcAxesAndBounds(sector, face, xAxis, yAxis, minx, miny, maxx, maxy),
            "geometry contract: projected bounds reject missing/invalid/nonfinite inputs");
        check(xAxis == sed::TVector{71, 72, 73} && yAxis == sed::TVector{74, 75, 76}
                && minx == 77 && miny == 78 && maxx == 79 && maxy == 80,
            "geometry contract: rejected projected bounds preserve every output sentinel");
    }
    // Fixed numeric vectors distinguish boundary inclusion and every output.
    for ( const double radius : {0.0, 0.5} )
    {
        sed::Level level{};
        auto& sector = level.newSector();
        sector.newVertex(3.0, radius, 0.0);
        double x = 71.0, y = 72.0, z = 73.0, distance = 74.0;
        check(sed::level::getNearestVertexToLine(sector.vertices, 0, 0, 0,
                  sed::TVector{2, 0, 0}, radius, x, y, z, distance)
                && x == 3.0 && y == radius && z == 0.0 && distance == radius * radius,
            "geometry contract: local nearest search includes exact radius and sets all outputs");
        sed::TVector nearest{71, 72, 73};
        check(sed::level::levelGetNearestWorldVertexToLine(level, 0, 0, 0,
                  sed::TVector{2, 0, 0}, radius, nearest)
                && nearest.x == 3.0 && nearest.y == radius && nearest.z == 0.0,
            "geometry contract: world nearest search includes the same exact-radius hit");
    }
    {
        const std::array<sed::Vertex, 1> vertices{{{3, 4, 5, 0}}};
        double x = 71, y = 72, z = 73, distance = 74;
        check(!sed::level::getNearestVertexToLine(vertices, 0, 0, 0,
                  sed::TVector::zero, 2, x, y, z, distance)
                && x == 71 && y == 72 && z == 73 && distance == 74,
            "geometry contract: zero-direction nearest rejection preserves all output sentinels");
        check(!sed::level::getNearestVertexToLine(vertices, 0, 0, 0,
                  sed::TVector::right, 2, x, y, z, distance)
                && x == 71 && y == 72 && z == 73 && distance == 4,
            "geometry contract: valid nearest miss retains coordinates and returns the search-radius square");
        sed::Level level{};
        level.newSector().vertices.assign(vertices.begin(), vertices.end());
        sed::TVector nearest{71, 72, 73};
        check(!sed::level::levelGetNearestWorldVertexToLine(level, 8, 9, 10,
                  sed::TVector::right, 0.5, nearest)
                && nearest.x == 8 && nearest.y == 9 && nearest.z == 10,
            "geometry contract: world nearest miss explicitly returns the supplied line origin");
    }
    for ( const double scale : {1.0, 1e-6, 1e-7} )
    for ( const bool subdivided : {false, true} )
    {
        sed::Sector sector{};
        // Trapezoid = rectangle area 2 plus right triangle area 1:
        // exact centroid (14/9, 4/9), independently of collinear subdivision.
        sector.vertices = {{0, 0, 0, 0}, {4 * scale, 0, 0, 0},
            {2 * scale, scale, 0, 0}, {0, scale, 0, 0}, {scale, 0, 0, 0}};
        sed::Surface face{};
        face.vertices = subdivided ? std::vector<std::int32_t>{0, 4, 1, 2, 3}
                                   : std::vector<std::int32_t>{0, 1, 2, 3};
        face.normal = {0, 0, 1};
        sed::TVector center{71, 72, 73};
        check(sed::level::surfaceCalcCenter(sector, face, center)
                && std::abs(center.x - (14.0 / 9.0) * scale) < scale * 1e-13
                && std::abs(center.y - (4.0 / 9.0) * scale) < scale * 1e-13 && center.z == 0.0,
            "geometry contract: area centroid is scale invariant for asymmetric and subdivided faces");
    }
    for ( const int failure : {0, 1, 2, 3} )
    {
        sed::Sector sector{};
        sector.vertices = {{0, 0, 0, 0}, {1, 0, 0, 0}, {0, 1, 0, 0}};
        sed::Surface face{};
        face.vertices = {0, 1, 2};
        face.normal = {0, 0, 1};
        sed::Thing thing{};
        thing.bbox.z1 = -2.0;
        if ( failure == 0 )
        {
            sector.vertices[1] = sector.vertices[0];
        }
        else if ( failure == 1 )
        {
            face.normal = sed::TVector::zero;
        }
        else if ( failure == 2 )
        {
            thing.bbox.z1 = std::numeric_limits<double>::quiet_NaN();
        }
        else
        {
            face.vertices[0] = 99;
        }
        sed::TVector position{71, 72, 73};
        double pitch = 74, yaw = 75, roll = 76;
        check(!sed::level::thingCalcPoseOnSurface(thing, sector, face, position, pitch, yaw, roll),
            "geometry contract: invalid pose geometry or support bounds reject explicitly");
        check(position.x == 71 && position.y == 72 && position.z == 73
                && pitch == 74 && yaw == 75 && roll == 76,
            "geometry contract: rejected surface pose preserves every output argument");
    }
    {
        const std::array<sed::Vertex, 3> vertices{
            sed::Vertex{ 5.0, 2.0, 0.0, 0 },
            sed::Vertex{ 1.0, 0.5, 0.0, 0 },
            sed::Vertex{ 0.0, 3.0, 0.0, 0 },
        };

        double vx             = 0.0;
        double vy             = 0.0;
        double vz             = 0.0;
        double minDistanceSqr = 0.0;

        const bool found = sed::level::getNearestVertexToLine(
            vertices,
            0.0,
            0.0,
            0.0,
            sed::TVector{ 10.0, 0.0, 0.0 },
            1.0,
            vx,
            vy,
            vz,
            minDistanceSqr);

        check(found, "getNearestVertexToLine.found");
        check(close(vx, 1.0), "getNearestVertexToLine.vx");
        check(close(vy, 0.5), "getNearestVertexToLine.vy");
        check(close(vz, 0.0), "getNearestVertexToLine.vz");
        check(close(minDistanceSqr, 0.25), "getNearestVertexToLine.minDistanceSqr");
    }

    {
        const std::array<sed::Vertex, 1> vertices{
            sed::Vertex{ 1.0, 0.0, 0.0, 0 },
        };

        double vx = 0.0;
        double vy = 0.0;
        double vz = 0.0;

        const bool found = sed::level::getNearestVertexToLine(vertices, 0.0, 0.0, 0.0, sed::TVector::zero, 1.0, vx, vy, vz);
        check(!found, "getNearestVertexToLine.zeroDirection.false");
    }

    {
        sed::Level level{};
        level.clear();

        sed::Sector& s0 = level.newSector();
        s0.addVertex(5.0, 2.0, 0.0);
        s0.addVertex(1.0, 0.5, 0.0);

        sed::Sector& s1 = level.newSector();
        s1.addVertex(0.2, 0.3, 0.0);
        s1.addVertex(0.1, 0.1, 0.0);

        sed::TVector nearest{};
        const bool found = sed::level::levelGetNearestWorldVertexToLine(level, 0.0, 0.0, 0.0, sed::TVector::right, 1.0, nearest);
        check(found, "levelGetNearestWorldVertexToLine.found");
        check(closeVec(nearest, sed::TVector{ 0.1, 0.1, 0.0 }, kGeometryEpsilon), "levelGetNearestWorldVertexToLine.nearest");

        sed::TVector none{};
        const bool foundNone = sed::level::levelGetNearestWorldVertexToLine(level, 0.0, 0.0, 0.0, sed::TVector::right, 0.05, none);
        check(!foundNone, "levelGetNearestWorldVertexToLine.none.false");
        check(closeVec(none, sed::TVector::zero, kExactEpsilon), "levelGetNearestWorldVertexToLine.none.vertexIsLinePoint");
    }

    {
        sed::Sector sector{};
        sector.vertices = {
            sed::Vertex{ 0.0, 0.0, 0.0, 0 },
            sed::Vertex{ 1.0, 0.0, 0.0, 0 },
            sed::Vertex{ 0.0, 1.0, 0.0, 0 },
        };

        sed::Surface surf{};
        surf.normal = sed::TVector{ 0.0, 0.0, 1.0 };
        surf.vertices = { 0, 1, 2 };

        check(sed::level::surfaceIsCrossed(sector, surf, 0.25, 0.25, 1.0, 0.25, 0.25, -1.0), "isSurfaceCrossed.inside.true");
        check(!sed::level::surfaceIsCrossed(sector, surf, 2.0, 2.0, 1.0, 2.0, 2.0, -1.0), "isSurfaceCrossed.outside.false");
        check(!sed::level::surfaceIsCrossed(sector, surf, 0.25, 0.25, 1.0, 0.25, 0.25, 2.0), "isSurfaceCrossed.sameSide.false");
        check(sed::level::surfaceIsCrossed(sector, surf, 0.10, 0.10, 0.0, 0.20, 0.10, 0.0), "isSurfaceCrossed.inPlane.true");
        check(sed::level::surfaceIsCrossed(sector, surf, 0.25, 0.25, 0.0, 0.25, 0.25, 2.0), "isSurfaceCrossed.endpointOnPlane.true");

        sed::Surface emptySurf{};
        emptySurf.normal = sed::TVector{ 0.0, 0.0, 1.0 };
        check(!sed::level::surfaceIsCrossed(sector, emptySurf, 0.0, 0.0, 1.0, 0.0, 0.0, -1.0), "isSurfaceCrossed.emptySurface.false");
    }

    {
        sed::Sector sector{};
        sector.vertices = {
            sed::Vertex{ 0.0, 0.0, 0.0, 0 },
            sed::Vertex{ 1.0, 0.0, 0.0, 0 },
            sed::Vertex{ 0.0, 1.0, 0.0, 0 },
        };

        sed::Surface surf{};
        surf.normal = sed::TVector{ 0.0, 0.0, 1.0 };
        surf.vertices = { 0, 1, 2 };

        sed::TVector xAxis{};
        sed::TVector yAxis{};
        double minx = 0.0;
        double miny = 0.0;
        double maxx = 0.0;
        double maxy = 0.0;
        const bool axesOk = sed::level::surfaceCalcAxesAndBounds(sector, surf, xAxis, yAxis, minx, miny, maxx, maxy);

        check(axesOk, "calcSurfaceAxesAndBounds.ok");
        check(closeVec(xAxis, sed::TVector{ 1.0, 0.0, 0.0 }, kGeometryEpsilon), "calcSurfaceAxesAndBounds.xAxis");
        check(closeVec(yAxis, sed::TVector{ 0.0, 1.0, 0.0 }, kGeometryEpsilon), "calcSurfaceAxesAndBounds.yAxis");
        check(close(minx, 0.0), "calcSurfaceAxesAndBounds.minx");
        check(close(maxx, 1.0), "calcSurfaceAxesAndBounds.maxx");
        check(close(miny, 0.0), "calcSurfaceAxesAndBounds.miny");
        check(close(maxy, 1.0), "calcSurfaceAxesAndBounds.maxy");

        sed::TVector center{};
        const bool centerOk = sed::level::surfaceCalcCenter(sector, surf, center);
        check(centerOk, "calcSurfaceCenter.ok");
        check(closeVec(center, sed::TVector{ 1.0 / 3.0, 1.0 / 3.0, 0.0 }, kGeometryEpsilon), "calcSurfaceCenter.areaCentroid");

        sed::Sector redundantRectangle{};
        redundantRectangle.vertices = {
            sed::Vertex{ 0.0, 0.0, 0.0, 0 },
            sed::Vertex{ 1.0, 0.0, 0.0, 0 },
            sed::Vertex{ 1.5, 0.0, 0.0, 0 },
            sed::Vertex{ 2.0, 0.0, 0.0, 0 },
            sed::Vertex{ 2.0, 1.0, 0.0, 0 },
            sed::Vertex{ 0.0, 1.0, 0.0, 0 },
        };
        sed::Surface redundantRectangleSurface{};
        redundantRectangleSurface.vertices = { 0, 1, 2, 3, 4, 5 };
        redundantRectangleSurface.normal = sed::level::surfaceCalcNormalNewell(
            redundantRectangle, redundantRectangleSurface);
        sed::TVector redundantRectangleCenter{};
        check(sed::level::surfaceCalcCenter(
                  redundantRectangle, redundantRectangleSurface,
                  redundantRectangleCenter)
                && closeVec(redundantRectangleCenter,
                    sed::TVector{ 1.0, 0.5, 0.0 }, kGeometryEpsilon),
            "calcSurfaceCenter.redundantCollinearVerticesDoNotBiasAreaCentroid");

        sed::Sector slopedSector{};
        slopedSector.vertices = {
            sed::Vertex{ 5.0, 10.0, 2.0, 0 },
            sed::Vertex{ 7.0, 10.0, 2.0, 0 },
            sed::Vertex{ 7.0, 12.0, 4.0, 0 },
            sed::Vertex{ 5.0, 12.0, 4.0, 0 },
        };

        sed::Surface slopedSurface{};
        slopedSurface.vertices = { 0, 1, 2, 3 };
        slopedSurface.normal = sed::level::surfaceCalcNormalNewell(slopedSector, slopedSurface);

        sed::TVector slopedCenter{};
        const bool slopedCenterOk = sed::level::surfaceCalcCenter(slopedSector, slopedSurface, slopedCenter);
        check(slopedCenterOk, "calcSurfaceCenter.slopedTranslated.ok");
        check(closeVec(slopedCenter, sed::TVector{ 6.0, 11.0, 3.0 }, kGeometryEpsilon), "calcSurfaceCenter.slopedTranslated.center");

        sed::Thing thing{};
        thing.bbox.z1 = -1.0f;

        sed::TVector position{};
        double pitch = 0.0;
        double yaw = 0.0;
        double roll = 0.0;

        const bool poseOk = sed::level::thingCalcPoseOnSurface(thing, sector, surf, position, pitch, yaw, roll);
        check(poseOk, "getThingPoseOnSurface.ok");
        check(closeVec(position, sed::TVector{ 1.0 / 3.0, 1.0 / 3.0, 1.0 }, kGeometryEpsilon), "getThingPoseOnSurface.positionAtAreaCentroid");

        const sed::TVector expectedZ = sed::TVector{ 0.0, 0.0, 1.0 };
        const sed::TVector expectedY = sed::TVector{ 1.0, 0.0, 0.0 };
        const sed::TVector expectedX = expectedY.Cross(expectedZ);

        sed::TVector rx = sed::TVector::right;
        sed::TVector ry = sed::TVector::forward;
        sed::TVector rz = sed::TVector::up;
        sed::rotateVector(rx, pitch, yaw, roll);
        sed::rotateVector(ry, pitch, yaw, roll);
        sed::rotateVector(rz, pitch, yaw, roll);

        check(closeVec(rx, expectedX, kRotationEpsilon), "getThingPoseOnSurface.basis.x");
        check(closeVec(ry, expectedY, kRotationEpsilon), "getThingPoseOnSurface.basis.y");
        check(closeVec(rz, expectedZ, kRotationEpsilon), "getThingPoseOnSurface.basis.z");
    }

    {
        sed::Sector sector{};
        sector.vertices = {
            sed::Vertex{ 0.0, 0.0, 0.0, 0 },
            sed::Vertex{ 0.0, 0.0, 0.0, 0 },
            sed::Vertex{ 1.0, 0.0, 0.0, 0 },
        };

        sed::Surface shortSurface{};
        shortSurface.normal = sed::TVector{ 0.0, 0.0, 1.0 };
        shortSurface.vertices = { 0 };

        sed::TVector xAxis{};
        sed::TVector yAxis{};
        double minx = 0.0;
        double miny = 0.0;
        double maxx = 0.0;
        double maxy = 0.0;
        check(!sed::level::surfaceCalcAxesAndBounds(sector, shortSurface, xAxis, yAxis, minx, miny, maxx, maxy),
              "calcSurfaceAxesAndBounds.shortSurface.false");

        sed::Surface duplicateEdgeSurface{};
        duplicateEdgeSurface.normal = sed::TVector{ 0.0, 0.0, 1.0 };
        duplicateEdgeSurface.vertices = { 0, 1, 2 };

        sed::Thing thing{};
        sed::TVector position{};
        double pitch = 0.0;
        double yaw = 0.0;
        double roll = 0.0;
        check(!sed::level::thingCalcPoseOnSurface(thing, sector, duplicateEdgeSurface, position, pitch, yaw, roll),
              "getThingPoseOnSurface.zeroFirstEdge.false");
    }

    {
        sed::Level level{};
        level.clear();

        sed::Sector& sector = level.newSector();
        (void)sector.addVertex(0.0, 0.0, 0.0);
        (void)sector.addVertex(1.0, 0.0, 0.0);
        (void)sector.addVertex(0.0, 1.0, 0.0);

        sed::Surface surface{};
        surface.normal = sed::TVector::zero;
        surface.vertices = { 0, 1, 2 };
        (void)sector.addSurface(std::move(surface));

        const auto hit = sed::level::levelPickSurface(level, sed::TVector{ 0.25, 0.25, 1.0 }, sed::TVector{ 0.0, 0.0, -1.0 }, 10.0);
        check(hit.has_value(), "pickSurface.hit");

        if (hit)
        {
            check(hit->surface.sector == 0, "pickSurface.sector");
            check(hit->surface.surface == 0, "pickSurface.surface");
            check(close(hit->distance, 1.0), "pickSurface.distance");
            check(close(hit->position.x, 0.25), "pickSurface.position.x");
            check(close(hit->position.y, 0.25), "pickSurface.position.y");
            check(close(hit->position.z, 0.0), "pickSurface.position.z");
        }

        const auto miss = sed::level::levelPickSurface(level, sed::TVector{ 2.0, 2.0, 1.0 }, sed::TVector{ 0.0, 0.0, -1.0 }, 10.0);
        check(!miss.has_value(), "pickSurface.miss");
    }

    {
        sed::Level level{};
        level.clear();

        sed::Sector& sector = level.newSector();
        (void)sector.addVertex(0.0, 0.0, 0.0);
        (void)sector.addVertex(1.0, 0.0, 0.0);
        (void)sector.addVertex(0.0, 1.0, 0.0);
        (void)sector.addVertex(0.0, 0.0, -2.0);
        (void)sector.addVertex(1.0, 0.0, -2.0);
        (void)sector.addVertex(0.0, 1.0, -2.0);

        sed::Surface nearSurface{};
        nearSurface.normal = sed::TVector::zero;
        nearSurface.vertices = { 0, 1, 2 };
        (void)sector.addSurface(std::move(nearSurface));

        sed::Surface farSurface{};
        farSurface.normal = sed::TVector::zero;
        farSurface.vertices = { 3, 4, 5 };
        (void)sector.addSurface(std::move(farSurface));

        const std::vector<sed::level::SurfacePickHit> hits =
            sed::level::levelPickSurfaces(level, sed::TVector{ 0.25, 0.25, 1.0 }, sed::TVector{ 0.0, 0.0, -2.0 }, 10.0);

        check(hits.size() == 2, "pickSurfaces.sorted.count");
        if (hits.size() == 2)
        {
            check(hits[0].surface.surface == 0, "pickSurfaces.sorted.first");
            check(hits[1].surface.surface == 1, "pickSurfaces.sorted.second");
            check(close(hits[0].distance, 1.0), "pickSurfaces.sorted.firstDistance");
            check(close(hits[1].distance, 3.0), "pickSurfaces.sorted.secondDistance");
        }

        const std::vector<sed::level::SurfacePickHit> zeroDirectionHits =
            sed::level::levelPickSurfaces(level, sed::TVector{ 0.25, 0.25, 1.0 }, sed::TVector::zero, 10.0);
        check(zeroDirectionHits.empty(), "pickSurfaces.zeroDirection.empty");
        check(!sed::level::levelPickSurface(level, sed::TVector{ 0.25, 0.25, 1.0 }, sed::TVector::zero, 10.0).has_value(),
              "pickSurface.zeroDirection.none");
    }

    {
        // Picking from just inside / immediately in front of a solid sector
        // must still return the nearest face.  This is the edge case where a
        // depth-ordered viewport pick can otherwise appear to skip the front
        // sector and expose a face behind it.
        sed::Level level{};
        sed::Sector& sector = level.newSector();
        sector.vertices = {
            { 0.0, 0.0, -0.001, 0 },
            { 1.0, 0.0, -0.001, 1 },
            { 1.0, 1.0, -0.001, 2 },
            { 0.0, 1.0, -0.001, 3 },
            { 0.0, 0.0, -1.0, 4 },
            { 1.0, 0.0, -1.0, 5 },
            { 1.0, 1.0, -1.0, 6 },
            { 0.0, 1.0, -1.0, 7 },
        };
        sed::Surface nearFace{};
        nearFace.vertices = { 0, 1, 2, 3 };
        nearFace.normal = sed::TVector::zero;
        (void)sector.addSurface(std::move(nearFace));
        sed::Surface farFace{};
        farFace.vertices = { 4, 5, 6, 7 };
        farFace.normal = sed::TVector::zero;
        (void)sector.addSurface(std::move(farFace));

        sed::level::SurfacePickIndex index{};
        index.rebuild(level);
        const std::vector<sed::level::SurfacePickHit> hits = index.pickSurfaces(
            level,
            sed::TVector{ 0.5, 0.5, 0.0 },
            sed::TVector{ 0.0, 0.0, -1.0 },
            10.0);
        check(hits.size() == 2u
                && hits.front().surface.surface == 0
                && close(hits.front().distance, 0.001, 1e-9),
            "surfacePickIndex keeps a very-near front face ahead of an occluded face");

        const std::vector<sed::level::SurfacePickHit> insideHits = index.pickSurfaces(
            level,
            sed::TVector{ 0.5, 0.5, 0.0001 },
            sed::TVector{ 0.0, 0.0, -1.0 },
            10.0);
        check(!insideHits.empty()
                && insideHits.front().surface.surface == 0
                && insideHits.front().distance > 0.0,
            "surfacePickIndex remains front-to-back when the camera is inside the sector volume");
    }
}

} // namespace sed::tests
