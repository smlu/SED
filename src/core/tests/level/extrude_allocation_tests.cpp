#include "test_common.hpp"

#include "sed/assets/sed.hpp"
#include "sed/level/build.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <map>
#include <new>
#include <optional>
#include <set>
#include <string_view>
#include <vector>

namespace
{
thread_local std::optional<std::size_t> allocationsBeforeFailure;
thread_local bool allocationFailureFired = false;
std::size_t currentOrdinal = 0;

void* allocate(std::size_t bytes)
{
    if ( allocationsBeforeFailure )
    {
        if ( *allocationsBeforeFailure == 0 )
        {
            allocationsBeforeFailure.reset();
            allocationFailureFired = true;
            throw std::bad_alloc();
        }
        --*allocationsBeforeFailure;
    }
    if ( void* memory = std::malloc(bytes == 0 ? 1 : bytes) )
    {
        return memory;
    }
    throw std::bad_alloc();
}
} // namespace

void* operator new(std::size_t bytes)
{
    return allocate(bytes);
}
void* operator new[](std::size_t bytes)
{
    return allocate(bytes);
}
void operator delete(void* memory) noexcept
{
    std::free(memory);
}
void operator delete[](void* memory) noexcept
{
    std::free(memory);
}
void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}
void operator delete[](void* memory, std::size_t) noexcept
{
    std::free(memory);
}

namespace
{
using sed::level::SurfaceExtrudeError;
using sed::tests::check;

bool sameRuntimeState(const sed::Level& actual, const sed::Level& expected)
{
    if ( actual.curId != expected.curId || actual.sectors.size() != expected.sectors.size() )
    {
        return false;
    }
    const auto sameDouble = [](double a, double b)
    { return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b); };
    for ( std::size_t owner = 0; owner < actual.sectors.size(); ++owner )
    {
        const auto& a = actual.sectors[owner];
        const auto& b = expected.sectors[owner];
        if ( a.id != b.id || a.num != b.num || a.mark != b.mark || a.vertices.size() != b.vertices.size() ||
             a.surfaces.size() != b.surfaces.size() )
        {
            return false;
        }
        for ( std::size_t vertex = 0; vertex < a.vertices.size(); ++vertex )
        {
            if ( a.vertices[vertex].mark != b.vertices[vertex].mark )
            {
                return false;
            }
        }
        for ( std::size_t face = 0; face < a.surfaces.size(); ++face )
        {
            const auto& x = a.surfaces[face];
            const auto& y = b.surfaces[face];
            if ( x.num != y.num || x.mark != y.mark || x.d3dId != y.d3dId || x.nAdj != y.nAdj ||
                 x.nMat != y.nMat || !sameDouble(x.normal.x, y.normal.x) ||
                 !sameDouble(x.normal.y, y.normal.y) || !sameDouble(x.normal.z, y.normal.z) )
            {
                return false;
            }
        }
    }
    return true;
}

sed::Level fixture()
{
    sed::Level level;
    level.layers = {"Extrude allocation layer"};
    level.sectors.push_back(sed::tests::makeLitTestCubeSector(0, sed::ColorWhite));
    auto& sector = level.sectors[0];
    sector.sound = "extrude_allocation_preserves_owner_sound_metadata.wav";
    sector.mark = 101;
    for ( auto& vertex : sector.vertices )
    {
        vertex.mark = 201;
    }
    for ( auto& face : sector.surfaces )
    {
        face.material = "extrude_allocation_preserves_surface_material_metadata.mat";
        face.mark = 301;
        face.d3dId = 401;
        face.nAdj = 501;
        face.nMat = 601;
    }
    return level;
}

sed::Level roundedExpandFixture()
{
    sed::Level level;
    level.layers = {"Rounded extrusion allocation layer"};
    auto& sector = level.newSector();
    sector.sound = "rounded_extrusion_preserves_source_metadata.wav";
    const std::array<sed::TVector, 6> bottom{
        {{-1, -1, 0}, {-.2, -.99998, .00003}, {.1, -1.00001, -.000015}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}}};
    for ( const auto& point : bottom )
    {
        sector.newVertex(point.x, point.y, point.z);
    }
    for ( const auto& point : bottom )
    {
        sector.newVertex(point.x, point.y, 1);
    }
    const auto addFace = [&](std::vector<std::int32_t> indices)
    {
        sed::Surface face;
        face.vertices = std::move(indices);
        face.material = "rounded_extrusion_preserves_material.mat";
        sed::TVector area{};
        const auto& origin = sector.vertices[face.vertices[0]];
        for ( std::size_t i = 0; i < face.vertices.size(); ++i )
        {
            const auto& point = sector.vertices[face.vertices[i]];
            sed::TexVertex texture;
            texture.u = static_cast<float>(8 * point.x + 4 * point.y);
            texture.v = static_cast<float>(16 * point.z + 2 * point.x);
            face.texVertices.push_back(texture);
            if ( i > 0 && i + 1 < face.vertices.size() )
            {
                const auto& next = sector.vertices[face.vertices[i + 1]];
                const sed::TVector a{point.x - origin.x, point.y - origin.y, point.z - origin.z};
                const sed::TVector b{next.x - origin.x, next.y - origin.y, next.z - origin.z};
                area.x += a.y * b.z - a.z * b.y;
                area.y += a.z * b.x - a.x * b.z;
                area.z += a.x * b.y - a.y * b.x;
            }
        }
        const double length = std::hypot(area.x, area.y, area.z);
        face.normal = {area.x / length, area.y / length, area.z / length};
        sector.addSurface(std::move(face));
    };
    addFace({0, 1, 2, 3, 4, 5});
    addFace({11, 10, 9, 8, 7, 6});
    for ( std::int32_t i = 0; i < 6; ++i )
    {
        const std::int32_t next = (i + 1) % 6;
        addFace({next, i, i + 6, next + 6});
    }
    return level;
}

bool close(double a, double b)
{
    return std::abs(a - b) <= 1e-10;
}

bool validFaceVertices(const sed::Sector& sector, const sed::Surface& face)
{
    bool valid = face.vertices.size() >= 3;
    check(face.texVertices.size() == face.vertices.size(),
          "Extrude output texture vertices match the complete face boundary");
    for ( const auto& texture : face.texVertices )
    {
        check(std::isfinite(texture.u) && std::isfinite(texture.v),
              "Extrude output texture coordinates are finite");
    }
    std::set<std::int32_t> unique;
    for ( const auto index : face.vertices )
    {
        if ( index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size() )
        {
            valid = false;
            continue;
        }
        const auto& point = sector.vertices[index];
        valid = unique.insert(index).second && valid;
        valid = std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) && valid;
    }
    check(valid, "Extrude output face has at least three unique in-range finite vertices");
    return valid;
}

struct Point2
{
    double x{}, y{};
};
using Polygon2 = std::vector<Point2>;

double cross2(const Point2 a, const Point2 b, const Point2 p)
{
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

double signedArea2(const Polygon2& points)
{
    double area = 0;
    for ( std::size_t i = 0; i < points.size(); ++i )
    {
        const auto& a = points[i];
        const auto& b = points[(i + 1) % points.size()];
        area += a.x * b.y - a.y * b.x;
    }
    return area * 0.5;
}

bool convexPolygon(const Polygon2& polygon)
{
    const double area = signedArea2(polygon);
    if ( polygon.size() < 3 || !std::isfinite(area) || std::abs(area) <= 1e-10 )
    {
        return false;
    }
    const double direction = area > 0 ? 1 : -1;
    for ( std::size_t edge = 0; edge < polygon.size(); ++edge )
    {
        const auto a = polygon[edge];
        const auto b = polygon[(edge + 1) % polygon.size()];
        if ( a.x == b.x && a.y == b.y )
        {
            return false;
        }
        for ( const auto point : polygon )
        {
            if ( direction * cross2(a, b, point) < -1e-10 )
            {
                return false;
            }
        }
    }
    return true;
}

Polygon2 yzPolygon(const sed::Sector& sector, const sed::Surface& face)
{
    Polygon2 polygon;
    if ( !validFaceVertices(sector, face) )
    {
        return polygon;
    }
    for ( const auto index : face.vertices )
    {
        const auto& point = sector.vertices[index];
        polygon.push_back({point.y, point.z});
    }
    return polygon;
}

double faceArea(const sed::Sector& sector, const sed::Surface& face)
{
    return std::abs(signedArea2(yzPolygon(sector, face)));
}

// Independent 2D half-plane clipping: the fixture coordinates are exact
// quarters, so this does not share the production geometry precision helpers.
Polygon2 intersectConvex(Polygon2 polygon, const Polygon2& clip)
{
    const double orientation = signedArea2(clip) >= 0 ? 1 : -1;
    for ( std::size_t edge = 0; edge < clip.size() && !polygon.empty(); ++edge )
    {
        const auto a = clip[edge];
        const auto b = clip[(edge + 1) % clip.size()];
        Polygon2 next;
        Point2 previous = polygon.back();
        double previousSide = orientation * cross2(a, b, previous);
        for ( const auto point : polygon )
        {
            const double side = orientation * cross2(a, b, point);
            if ( (side >= 0) != (previousSide >= 0) )
            {
                const double t = previousSide / (previousSide - side);
                next.push_back(
                    {previous.x + t * (point.x - previous.x), previous.y + t * (point.y - previous.y)});
            }
            if ( side >= 0 )
            {
                next.push_back(point);
            }
            previous = point;
            previousSide = side;
        }
        polygon = std::move(next);
    }
    return polygon;
}

void verifySourceRim(const sed::Sector& sector)
{
    check(sector.surfaces.size() == 4, "Inset preview returns exactly four rim faces");
    for ( const auto& point : sector.vertices )
    {
        check(std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z),
              "Inset preview contains only finite vertices");
    }
    const Polygon2 hole{{-.75, -.75}, {.75, -.75}, {.75, .75}, {-.75, .75}};
    std::vector<Polygon2> polygons;
    double area = 0;
    for ( const auto& face : sector.surfaces )
    {
        auto polygon = yzPolygon(sector, face);
        if ( polygon.size() < 3 )
        {
            continue;
        }
        bool onSource = true;
        for ( const auto index : face.vertices )
        {
            const auto& point = sector.vertices[index];
            onSource = onSource && close(point.x, 1) && point.y >= -1 && point.y <= 1 && point.z >= -1 &&
                       point.z <= 1;
        }
        check(onSource, "Inset rim stays on source x=1 inside the fixed outer square");
        const double signedArea = signedArea2(polygon);
        check(convexPolygon(polygon), "Inset rim faces are nondegenerate convex planar polygons");
        check(close(std::abs(signedArea2(intersectConvex(polygon, hole))), 0),
              "Inset rim has no area inside the fixed central opening");
        for ( const auto& previous : polygons )
        {
            check(close(std::abs(signedArea2(intersectConvex(polygon, previous))), 0),
                  "Inset rim faces have disjoint interiors");
        }
        area += std::abs(signedArea);
        polygons.push_back(std::move(polygon));
    }
    // Containment, disjoint interiors, no hole overlap and exact remaining
    // area together establish complete coverage of the specified square rim.
    check(close(area, 1.75), "Inset rim covers exactly outer area 4 minus opening area 2.25");
}

void verifySector(const sed::Sector& sector, double span)
{
    double minX = 2, maxX = 0, minY = 2, maxY = -2, minZ = 2, maxZ = -2;
    bool finite = !sector.vertices.empty();
    for ( const auto& vertex : sector.vertices )
    {
        finite = finite && std::isfinite(vertex.x) && std::isfinite(vertex.y) && std::isfinite(vertex.z);
        minX = std::min(minX, vertex.x);
        maxX = std::max(maxX, vertex.x);
        minY = std::min(minY, vertex.y);
        maxY = std::max(maxY, vertex.y);
        minZ = std::min(minZ, vertex.z);
        maxZ = std::max(maxZ, vertex.z);
    }
    check(finite && close(minX, 1) && close(maxX, 1.5) && close(minY, -span / 2) && close(maxY, span / 2) &&
              close(minZ, -span / 2) && close(maxZ, span / 2),
          "Extrude success has finite vertices and analytically specified "
          "dimensions");
    check(!sector.surfaces.empty(), "Extruded solid has a nonempty boundary");
    std::map<std::pair<int, int>, int> edges;
    double signedVolume = 0;
    for ( const auto& face : sector.surfaces )
    {
        if ( !validFaceVertices(sector, face) )
        {
            continue;
        }
        const auto& origin = sector.vertices[face.vertices[0]];
        bool constantX = true, constantY = true, constantZ = true;
        sed::TVector area{};
        for ( std::size_t i = 0; i < face.vertices.size(); ++i )
        {
            const auto& point = sector.vertices[face.vertices[i]];
            constantX = constantX && close(point.x, origin.x);
            constantY = constantY && close(point.y, origin.y);
            constantZ = constantZ && close(point.z, origin.z);
            ++edges[{face.vertices[i], face.vertices[(i + 1) % face.vertices.size()]}];
            if ( i == 0 || i + 1 == face.vertices.size() )
            {
                continue;
            }
            const auto& next = sector.vertices[face.vertices[i + 1]];
            const sed::TVector u{point.x - origin.x, point.y - origin.y, point.z - origin.z};
            const sed::TVector v{next.x - origin.x, next.y - origin.y, next.z - origin.z};
            const sed::TVector cross{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
            area.x += cross.x;
            area.y += cross.y;
            area.z += cross.z;
            signedVolume += (origin.x * cross.x + origin.y * cross.y + origin.z * cross.z) / 6;
        }
        const bool onBoundary = (constantX && (close(origin.x, 1) || close(origin.x, 1.5))) ||
                                (constantY && (close(origin.y, -span / 2) || close(origin.y, span / 2))) ||
                                (constantZ && (close(origin.z, -span / 2) || close(origin.z, span / 2)));
        check(onBoundary, "Every Extrude face lies on an independently fixed prism "
                          "boundary plane");
        Polygon2 projected;
        for ( const auto index : face.vertices )
        {
            const auto& point = sector.vertices[index];
            projected.push_back(constantX   ? Point2{point.y, point.z}
                                : constantY ? Point2{point.x, point.z}
                                            : Point2{point.x, point.y});
        }
        check(convexPolygon(projected), "Every Extrude face has a convex nondegenerate boundary");
        const double inward = area.x * (1.25 - origin.x) - area.y * origin.y - area.z * origin.z;
        check(std::isfinite(inward) && inward > 0,
              "Every Extrude face has nondegenerate inward geometric winding");
        const double normalLength = std::hypot(face.normal.x, face.normal.y, face.normal.z);
        const double normalInward =
            face.normal.x * (1.25 - origin.x) - face.normal.y * origin.y - face.normal.z * origin.z;
        check(close(normalLength, 1) && normalInward > 0 && std::isfinite(normalInward) &&
                  area.x * face.normal.x + area.y * face.normal.y + area.z * face.normal.z > 0,
              "Extrude cached normals are finite unit vectors agreeing with inward "
              "faces");
        bool inwardPlane = true;
        for ( const auto& point : sector.vertices )
        {
            const double distance = face.normal.x * (point.x - origin.x) +
                                    face.normal.y * (point.y - origin.y) +
                                    face.normal.z * (point.z - origin.z);
            inwardPlane = inwardPlane && std::isfinite(distance) && distance >= -1e-10;
        }
        for ( const auto index : face.vertices )
        {
            const auto& point = sector.vertices[index];
            const double distance = face.normal.x * (point.x - origin.x) +
                                    face.normal.y * (point.y - origin.y) +
                                    face.normal.z * (point.z - origin.z);
            inwardPlane = inwardPlane && close(distance, 0);
        }
        check(inwardPlane, "Every cached face plane contains its boundary and "
                           "encloses all solid vertices");
    }
    for ( const auto& [edge, count] : edges )
    {
        check(count == 1 && edges.contains({edge.second, edge.first}) &&
                  edges.at({edge.second, edge.first}) == 1,
              "Extruded sector has exactly two opposite incidences per edge");
    }
    check(close(-signedVolume, 0.5 * span * span),
          "Extruded solid has the independently specified positive enclosed "
          "prism volume");
}
void verifyRoundedExpand(const sed::Level& level, const sed::Level& original)
{
    check(level.sectors.size() == 2, "Rounded expanded extrusion adds exactly one Sector");
    if ( level.sectors.size() != 2 )
    {
        return;
    }
    const auto samePoint = [](const sed::Vertex& a, const sed::Vertex& b)
    { return a.x == b.x && a.y == b.y && a.z == b.z; };
    const auto sameTexture = [](const sed::TexVertex& a, const sed::TexVertex& b)
    {
        return a.u == b.u && a.v == b.v && a.color == b.color;
    };
    const auto isExactAuthoredSourceContour = [&](const sed::Sector& sector,
        const sed::Surface& face)
    {
        const auto& authoredSector = original.sectors[0];
        const auto& authoredFace = authoredSector.surfaces[0];
        if ( face.vertices.size() != authoredFace.vertices.size()
            || face.texVertices.size() != face.vertices.size()
            || authoredFace.texVertices.size() != authoredFace.vertices.size()
            || face.vertices.size() < 3 )
        {
            return false;
        }
        for ( std::size_t start = 0; start < authoredFace.vertices.size(); ++start )
        {
            bool equal = true;
            for ( std::size_t corner = 0; corner < authoredFace.vertices.size(); ++corner )
            {
                const auto actualIndex = face.vertices[corner];
                const auto authoredIndex = authoredFace.vertices[(start + corner)
                    % authoredFace.vertices.size()];
                if ( actualIndex < 0 || authoredIndex < 0
                    || static_cast<std::size_t>(actualIndex) >= sector.vertices.size()
                    || static_cast<std::size_t>(authoredIndex) >= authoredSector.vertices.size() )
                {
                    equal = false;
                    break;
                }
                equal = equal
                    && samePoint(sector.vertices[static_cast<std::size_t>(actualIndex)],
                        authoredSector.vertices[static_cast<std::size_t>(authoredIndex)])
                    && sameTexture(face.texVertices[corner],
                        authoredFace.texVertices[(start + corner) % authoredFace.vertices.size()]);
                if ( !equal )
                {
                    break;
                }
            }
            if ( equal )
            {
                return true;
            }
        }
        return false;
    };
    {
        const auto& authoredSector = original.sectors[0];
        const auto& authoredFace = authoredSector.surfaces[0];
        check(isExactAuthoredSourceContour(authoredSector, authoredFace),
            "Rounded portal oracle accepts the exact authored source contour copy");

        auto cyclicRotation = authoredFace;
        std::rotate(cyclicRotation.vertices.begin(), cyclicRotation.vertices.begin() + 2,
            cyclicRotation.vertices.end());
        std::rotate(cyclicRotation.texVertices.begin(), cyclicRotation.texVertices.begin() + 2,
            cyclicRotation.texVertices.end());
        check(isExactAuthoredSourceContour(authoredSector, cyclicRotation),
            "Rounded portal oracle accepts a matching cyclic source rotation");

        auto oneUlpSector = authoredSector;
        oneUlpSector.vertices[static_cast<std::size_t>(authoredFace.vertices[0])].x =
            std::nextafter(oneUlpSector.vertices[static_cast<std::size_t>(authoredFace.vertices[0])].x,
                std::numeric_limits<double>::infinity());
        check(!isExactAuthoredSourceContour(oneUlpSector, authoredFace),
            "Rounded portal oracle rejects a one-ULP source-coordinate change");

        auto changedUv = authoredFace;
        changedUv.texVertices[0].u = std::nextafter(changedUv.texVertices[0].u,
            std::numeric_limits<float>::infinity());
        check(!isExactAuthoredSourceContour(authoredSector, changedUv),
            "Rounded portal oracle rejects a changed source UV");

        auto changedRgba = authoredFace;
        changedRgba.texVertices[0].color.r = std::nextafter(changedRgba.texVertices[0].color.r,
            -std::numeric_limits<float>::infinity());
        check(!isExactAuthoredSourceContour(authoredSector, changedRgba),
            "Rounded portal oracle rejects a changed source RGBA component");

        auto wrongOrder = authoredFace;
        std::swap(wrongOrder.vertices[1], wrongOrder.vertices[2]);
        std::swap(wrongOrder.texVertices[1], wrongOrder.texVertices[2]);
        check(!isExactAuthoredSourceContour(authoredSector, wrongOrder),
            "Rounded portal oracle rejects a differently ordered source contour");

        auto omittedCorner = authoredFace;
        omittedCorner.vertices.pop_back();
        omittedCorner.texVertices.pop_back();
        check(!isExactAuthoredSourceContour(authoredSector, omittedCorner),
            "Rounded portal oracle rejects an omitted source corner");

        auto repeatedCorner = authoredFace;
        repeatedCorner.vertices.back() = repeatedCorner.vertices.front();
        repeatedCorner.texVertices.back() = repeatedCorner.texVertices.front();
        check(!isExactAuthoredSourceContour(authoredSector, repeatedCorner),
            "Rounded portal oracle rejects a repeated source corner");

        auto invalidPositiveIndex = authoredFace;
        invalidPositiveIndex.vertices[0] = static_cast<std::int32_t>(authoredSector.vertices.size());
        check(!isExactAuthoredSourceContour(authoredSector, invalidPositiveIndex),
            "Rounded portal oracle rejects an out-of-range positive vertex index");

        auto invalidNegativeIndex = authoredFace;
        invalidNegativeIndex.vertices[0] = -1;
        check(!isExactAuthoredSourceContour(authoredSector, invalidNegativeIndex),
            "Rounded portal oracle rejects a negative vertex index");

        auto textureCountMismatch = authoredFace;
        textureCountMismatch.texVertices.pop_back();
        check(!isExactAuthoredSourceContour(authoredSector, textureCountMismatch),
            "Rounded portal oracle rejects a texture-count mismatch");
    }
    for ( const auto& point : original.sectors[0].vertices )
    {
        check(std::any_of(level.sectors[0].vertices.begin(), level.sectors[0].vertices.end(),
                          [&](const auto& actual) { return samePoint(point, actual); }),
              "Rounded expanded extrusion preserves all twelve exact source "
              "coordinates");
    }
    int directedPortals = 0;
    double portalArea = 0;
    using Point3 = std::array<double, 3>;
    std::map<std::pair<Point3, Point3>, int> portalEdges;
    for ( std::size_t owner = 0; owner < level.sectors.size(); ++owner )
    {
        const auto& sector = level.sectors[owner];
        std::map<std::pair<int, int>, int> edges;
        if ( owner == 1 )
        {
            for ( const auto& point : sector.vertices )
            {
                check(std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) &&
                          point.x >= -1.1001 && point.x <= 1.1001 && point.y >= -1.10011 &&
                          point.y <= 1.1001 && point.z >= -.2001 && point.z <= .0001,
                      "Rounded expanded vertices stay inside the independently bounded "
                      "expanded prism");
            }
        }
        double volume = 0;
        for ( std::size_t faceIndex = 0; faceIndex < sector.surfaces.size(); ++faceIndex )
        {
            const auto& face = sector.surfaces[faceIndex];
            if ( !validFaceVertices(sector, face) )
            {
                continue;
            }
            const auto& origin = sector.vertices[face.vertices[0]];
            sed::TVector area{};
            for ( std::size_t i = 0; i < face.vertices.size(); ++i )
            {
                const auto& point = sector.vertices[face.vertices[i]];
                const auto& next = sector.vertices[face.vertices[(i + 1) % face.vertices.size()]];
                ++edges[{face.vertices[i], face.vertices[(i + 1) % face.vertices.size()]}];
                if ( i > 0 && i + 1 < face.vertices.size() )
                {
                    const sed::TVector a{point.x - origin.x, point.y - origin.y, point.z - origin.z};
                    const sed::TVector b{next.x - origin.x, next.y - origin.y, next.z - origin.z};
                    const sed::TVector c{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
                    area = area + c;
                    volume -= (origin.x * c.x + origin.y * c.y + origin.z * c.z) / 6;
                }
                if ( owner == 0 && face.texVertices.size() == face.vertices.size() )
                {
                    for ( const auto& oldFace : original.sectors[0].surfaces )
                    {
                        for ( std::size_t oldCorner = 0; oldCorner < oldFace.vertices.size(); ++oldCorner )
                        {
                            if ( samePoint(point, original.sectors[0].vertices[oldFace.vertices[oldCorner]]) )
                            {
                                check(face.texVertices[i].u == oldFace.texVertices[oldCorner].u &&
                                          face.texVertices[i].v == oldFace.texVertices[oldCorner].v,
                                      "Rounded source corners retain their exact independently "
                                      "seeded UV values");
                            }
                        }
                    }
                }
                if ( owner == 0 && face.adjoin )
                {
                    portalArea += (point.x * next.y - point.y * next.x) / 2;
                    ++portalEdges[{{point.x, point.y, point.z}, {next.x, next.y, next.z}}];
                }
            }
            const double normalLength = std::hypot(face.normal.x, face.normal.y, face.normal.z);
            check(close(normalLength, 1) && area.dot(face.normal) > 0,
                  "Rounded expanded faces have finite unit caches agreeing with "
                  "nondegenerate winding");
            if ( !face.adjoin )
            {
                continue;
            }
            if ( owner == 0 )
            {
                Polygon2 projected;
                for ( const auto index : face.vertices )
                {
                    const auto& point = sector.vertices[index];
                    projected.push_back({point.x, point.y});
                }
                check(signedArea2(projected) > 0
                        && (isExactAuthoredSourceContour(sector, face)
                            || convexPolygon(projected)),
                      "Rounded portal cells have positive footprints; authored reflex contours stay exact and new cells are convex");
            }
            ++directedPortals;
            const auto target = *face.adjoin;
            const bool valid =
                target.sector >= 0 && static_cast<std::size_t>(target.sector) < level.sectors.size() &&
                target.surface >= 0 &&
                static_cast<std::size_t>(target.surface) < level.sectors[target.sector].surfaces.size();
            check(valid, "Rounded expanded portal target is in range");
            if ( !valid )
            {
                continue;
            }
            const auto& other = level.sectors[target.sector];
            const auto& mirror = other.surfaces[target.surface];
            check(mirror.adjoin ==
                      std::optional<sed::SurfaceRef>{{static_cast<int>(owner), static_cast<int>(faceIndex)}},
                  "Rounded expanded portals have exact reciprocal references");
            if ( !validFaceVertices(other, mirror) )
            {
                continue;
            }
            bool matches = false;
            if ( face.vertices.size() == mirror.vertices.size() )
            {
                for ( std::size_t start = 0; start < mirror.vertices.size(); ++start )
                {
                    bool equal = true;
                    for ( std::size_t i = 0; i < face.vertices.size(); ++i )
                    {
                        equal =
                            equal &&
                            samePoint(sector.vertices[face.vertices[i]],
                                      other.vertices[mirror.vertices[(start + mirror.vertices.size() - i) %
                                                                     mirror.vertices.size()]]);
                    }
                    matches = matches || equal;
                }
            }
            check(matches, "Rounded expanded portal boundaries are exact reversed "
                           "cyclic coordinates");
        }
        for ( const auto& [edge, count] : edges )
        {
            check(count == 1 && edges.contains({edge.second, edge.first}) &&
                      edges.at({edge.second, edge.first}) == 1,
                  "Rounded expanded owners remain closed with exactly opposite edge "
                  "incidences");
        }
        // XY footprint is 3.999995; bottom heights are within 3e-5 of zero.
        // Expanded rectangle is 2.2 by 2.2, length .2. A 1e-4 plane/bounds
        // envelope contributes at most 4.84e-4 + .2*8.8e-4 to its volume.
        const double expected = owner == 0 ? 3.999995 : .968;
        const double bound = owner == 0 ? 4 * 3e-5 : .00066;
        check(std::isfinite(volume) && volume > 0 && std::abs(volume - expected) <= bound,
              "Rounded expanded owners preserve independent positive analytic "
              "volume bounds");
    }
    check(directedPortals >= 2 && directedPortals % 2 == 0,
          "Rounded expanded extrusion joins complete reciprocal portal cells");
    check(close(portalArea, 3.999995), "Rounded expanded portal covers the fixed analytic XY area");
    std::array<std::vector<std::pair<double, double>>, 6> intervals;
    const auto& originalFace = original.sectors[0].surfaces[0];
    // Exact internal cell edges cancel; subdivided outer edges cover each
    // independently authored 3D source segment once, without gaps or overlap.
    for ( const auto& [edge, count] : portalEdges )
    {
        const auto reverse = portalEdges.find({edge.second, edge.first});
        const int opposite = reverse == portalEdges.end() ? 0 : reverse->second;
        check(count == 1 && opposite <= 1, "Portal cells use every directed geometric edge at most once");
        if ( count == opposite )
        {
            continue;
        }
        bool matched = false;
        for ( std::size_t i = 0; i < 6; ++i )
        {
            const auto& a = original.sectors[0].vertices[originalFace.vertices[i]];
            const auto& b = original.sectors[0].vertices[originalFace.vertices[(i + 1) % 6]];
            const Point3 start{a.x, a.y, a.z}, delta{b.x - a.x, b.y - a.y, b.z - a.z};
            const auto axis =
                static_cast<std::size_t>(std::max_element(delta.begin(), delta.end(), [](double x, double y)
                                                          { return std::abs(x) < std::abs(y); }) -
                                         delta.begin());
            const double t0 = (edge.first[axis] - start[axis]) / delta[axis];
            const double t1 = (edge.second[axis] - start[axis]) / delta[axis];
            bool onSegment = t0 >= 0 && t1 <= 1 && t1 > t0;
            for ( std::size_t j = 0; j < 3; ++j )
            {
                const double error = 32 * std::numeric_limits<double>::epsilon() *
                                     (std::abs(start[j]) + std::abs(delta[j]) + 1);
                onSegment = onSegment && std::abs(edge.first[j] - (start[j] + t0 * delta[j])) <= error &&
                            std::abs(edge.second[j] - (start[j] + t1 * delta[j])) <= error;
            }
            if ( onSegment )
            {
                check(!matched, "Remaining portal edge belongs to one authored outline segment");
                intervals[i].emplace_back(t0, t1);
                matched = true;
            }
        }
        check(matched, "Every uncancelled portal edge follows a fixed authored 3D outline segment");
    }
    for ( auto& coverage : intervals )
    {
        std::sort(coverage.begin(), coverage.end());
        double end = 0;
        for ( const auto& interval : coverage )
        {
            check(interval.first == end, "Portal boundary covers its source edge without gaps or overlaps");
            end = interval.second;
        }
        check(end == 1, "Portal boundary covers each of the six authored edges completely");
    }
}
} // namespace

int main(int argc, char** argv)
{
    if ( argc != 2 )
    {
        return 2;
    }
    const std::string_view name = argv[1];
    const std::array<std::string_view, 8> names{"wire",   "solid",  "bounds", "source-cut",
                                                "commit", "expand", "inset",  "rounded-expand"};
    const auto found = std::find(names.begin(), names.end(), name);
    if ( found == names.end() )
    {
        return 2;
    }
    const auto mode = static_cast<int>(found - names.begin());
    std::set_terminate(
        []
        {
            std::fprintf(stderr, "Extrude unexpectedly terminated at allocation %zu\n", currentOrdinal);
            std::_Exit(86);
        });
    bool reachedSuccess = false;
    std::size_t rejected = 0;
    for ( std::size_t ordinal = 0; ordinal < 8192; ++ordinal )
    {
        sed::Level level = mode == 7 ? roundedExpandFixture() : fixture();
        sed::Cog cog;
        for ( const bool pairText : {false, true} )
        {
            sed::CogValue value;
            value.type = sed::ValueType::ptr;
            value.cogType = sed::CogType::surface;
            value.intValue = 0;
            value.stringValue = pairText ? "                                0 0" : "";
            cog.values.push_back(std::move(value));
        }
        level.cogs.push_back(std::move(cog));
        const sed::Level original = level;
        const auto bytes = sed::assets::saveToSedBytes(level);
        sed::level::SurfaceExtrudeReport report{SurfaceExtrudeError::InvalidResult, {71, 72}};
        std::optional<sed::level::SurfaceExtrudePreview> wire;
        std::optional<sed::level::SurfaceExtrudeExpandBounds> bounds;
        std::optional<sed::Sector> solid;
        bool accepted = false, caught = false;
        currentOrdinal = ordinal;
        allocationFailureFired = false;
        allocationsBeforeFailure = ordinal;
        try
        {
            switch ( mode )
            {
            case 0:
                wire = sed::level::levelBuildSurfaceExtrudePreview(level, {0, 0}, .5, 0, &report);
                accepted = wire.has_value();
                break;
            case 1:
                solid = sed::level::levelBuildSurfaceExtrudeSectorPreview(level, {0, 0}, .5, 0, &report);
                accepted = solid.has_value();
                break;
            case 2:
                bounds = sed::level::levelSurfaceExtrudeExpandBounds(level, {0, 0}, &report);
                accepted = bounds.has_value();
                break;
            case 3:
                solid = sed::level::levelBuildSurfaceExtrudeSourceCutPreview(level, {0, 0}, -.25, &report);
                accepted = solid.has_value();
                break;
            default:
                accepted = sed::level::levelExtrudeSurface(level, {0, 0}, mode == 7 ? .2 : .5,
                                                           mode == 7   ? .1
                                                           : mode == 5 ? .25
                                                           : mode == 6 ? -.25
                                                                       : 0,
                                                           &report);
                break;
            }
        }
        catch ( ... )
        {
            caught = true;
        }
        allocationsBeforeFailure.reset();
        check(!caught, "Public Extrude API never leaks an exception");
        if ( allocationFailureFired )
        {
            ++rejected;
            check(!accepted && !wire && !solid && !bounds,
                  "Extrude allocation failure returns empty outputs and false");
            check(report.error == SurfaceExtrudeError::AllocationFailure &&
                      report.surface == sed::SurfaceRef{0, 0},
                  "Extrude allocation failure resets both report OUT fields");
        }
        if ( allocationFailureFired || mode < 4 )
        {
            check(sed::assets::sedLevelsEqual(level, original) && sameRuntimeState(level, original),
                  "Extrude failure and previews preserve independent authored and "
                  "runtime state");
            bool serializedUnchanged = false;
            try
            {
                serializedUnchanged = sed::assets::saveToSedBytes(level) == bytes;
            }
            catch ( const std::exception& error )
            {
                std::fprintf(stderr, "Extrude rollback cannot serialize: %s\n", error.what());
            }
            check(serializedUnchanged, "Extrude rollback also preserves complete native bytes");
        }
        if ( !allocationFailureFired )
        {
            check(accepted && report.error == SurfaceExtrudeError::None &&
                      report.surface == sed::SurfaceRef{0, 0},
                  "Extrude sweep reaches success with exact report OUT fields");
            if ( accepted )
            {
                if ( mode == 0 )
                {
                    const std::array<sed::TVector, 4> inner{{{1, 1, -1}, {1, 1, 1}, {1, -1, 1}, {1, -1, -1}}};
                    const std::array<sed::TVector, 4> outer{
                        {{1.5, -1, -1}, {1.5, -1, 1}, {1.5, 1, 1}, {1.5, 1, -1}}};
                    check(wire->innerLoop.size() == 4 && wire->outerLoop.size() == 4,
                          "Wire preview returns exactly four corners per loop");
                    for ( std::size_t i = 0;
                          i < 4 && i < wire->innerLoop.size() && i < wire->outerLoop.size(); ++i )
                    {
                        const auto& a = wire->innerLoop[i];
                        const auto& b = wire->outerLoop[i];
                        check(close(a.x, inner[i].x) && close(a.y, inner[i].y) && close(a.z, inner[i].z) &&
                                  close(b.x, outer[i].x) && close(b.y, outer[i].y) && close(b.z, outer[i].z),
                              "Wire preview corners match fixed coordinate vectors");
                    }
                }
                else if ( mode == 1 )
                {
                    verifySector(*solid, 2);
                }
                else if ( mode == 2 )
                {
                    check(close(bounds->negativeCollapseLimit, -1) && close(bounds->minSpan, 2),
                          "Expand bounds match both analytic output values");
                }
                else if ( mode == 3 )
                {
                    verifySourceRim(*solid);
                }
                else if ( mode == 7 )
                {
                    verifyRoundedExpand(level, original);
                }
                else
                {
                    check(level.sectors.size() == 2, "Committed extrusion creates exactly one new sector");
                    if ( level.sectors.size() == 2 )
                    {
                        verifySector(level.sectors[1], mode == 5 ? 2.5 : mode == 6 ? 1.5 : 2);
                    }
                    int links = 0;
                    for ( std::size_t s = 0; s < level.sectors.size(); ++s )
                    {
                        for ( std::size_t f = 0; f < level.sectors[s].surfaces.size(); ++f )
                        {
                            const auto& face = level.sectors[s].surfaces[f];
                            if ( !face.adjoin )
                            {
                                continue;
                            }
                            ++links;
                            const auto ref = *face.adjoin;
                            const bool validTarget =
                                ref.sector >= 0 &&
                                static_cast<std::size_t>(ref.sector) < level.sectors.size() &&
                                ref.surface >= 0 &&
                                static_cast<std::size_t>(ref.surface) <
                                    level.sectors[ref.sector].surfaces.size();
                            check(validTarget, "Committed portal target indices are in range");
                            if ( !validTarget )
                            {
                                continue;
                            }
                            const auto& targetSector = level.sectors[ref.sector];
                            const auto& targetFace = targetSector.surfaces[ref.surface];
                            check(targetFace.adjoin == std::optional<sed::SurfaceRef>{{static_cast<int>(s),
                                                                                       static_cast<int>(f)}},
                                  "Committed extrusion has reciprocal portal references");
                            if ( !validFaceVertices(level.sectors[s], face) ||
                                 !validFaceVertices(targetSector, targetFace) )
                            {
                                continue;
                            }
                            bool matched = false;
                            if ( face.vertices.size() == targetFace.vertices.size() )
                            {
                                const auto count = face.vertices.size();
                                for ( std::size_t start = 0; start < count && !matched; ++start )
                                {
                                    matched = true;
                                    for ( std::size_t corner = 0; corner < count; ++corner )
                                    {
                                        const auto& a = level.sectors[s].vertices[face.vertices[corner]];
                                        const auto& b =
                                            targetSector.vertices
                                                [targetFace.vertices[(start + count - corner) % count]];
                                        matched = matched && a.x == b.x && a.y == b.y && a.z == b.z;
                                    }
                                }
                            }
                            check(matched, "Committed portal boundaries match in reverse "
                                           "cyclic coordinate order");
                            check(close(faceArea(level.sectors[s], face), mode == 6 ? 2.25 : 4.0),
                                  "Committed portal area matches the fixed source and "
                                  "rectangle intersection");
                        }
                    }
                    check(links == 2, "Committed extrusion creates exactly one paired portal");
                }
            }
            reachedSuccess = true;
            break;
        }
    }
    check(rejected > 0 && reachedSuccess, "Extrude allocation sweep covers failures and success");
    return sed::tests::failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
