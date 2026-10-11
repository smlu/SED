#include "sed/level/geometry.hpp"
#include "sed/level/cleave.hpp"
#include "sed/math_utils.hpp"
#include "geometry_precision.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <new>
#include <numbers>
#include <vector>

namespace sed::level
{

double getAngle(const double x, const double y, const double xa, const double ya, const double xb, const double yb) noexcept
{
    const double ax = xa - x;
    const double ay = ya - y;
    const double bx = xb - x;
    const double by = yb - y;

    const double denom = (std::sqrt(ax * ax + ay * ay) * std::sqrt(bx * bx + by * by));
    if (denom == 0.0)
    {
        return 0.0;
    }

    double d = (ax * bx + ay * by) / denom;
    d = std::clamp(d, -1.0, 1.0);

    double angle = std::acos(d);
    if (isAbove180(x, y, xa, ya, xb, yb))
    {
        angle = 2.0 * std::numbers::pi_v<double> - angle;
    }
    return angle;
}

bool doIntersect(
    const double x11,
    const double y11,
    const double x12,
    const double y12,
    const double x21,
    const double y21,
    const double x22,
    const double y22) noexcept
{
    const double dy1 = y12 - y11;
    const double dy2 = y22 - y21;
    const double dx1 = x12 - x11;
    const double dx2 = x22 - x21;

    const double D = dy1 * dx2 - dx1 * dy2;
    if (std::abs(D) < 0.0001)
    {
        return false;
    }

    const double b = (dx1 * (y21 - y11) + dy1 * (x11 - x21)) / D;
    const double y = y21 + dy2 * b;
    const double x = x21 + dx2 * b;

    bool ok = true;
    ok = ok && (y >= std::min(y11, y12)) && (y <= std::max(y11, y12));
    ok = ok && (y >= std::min(y21, y22)) && (y <= std::max(y21, y22));

    ok = ok && (x >= std::min(x11, x12)) && (x <= std::max(x11, x12));
    ok = ok && (x >= std::min(x21, x22)) && (x <= std::max(x21, x22));
    return ok;
}

namespace
{

bool closeEnoughLineValue(const double a, const double b) noexcept
{
    return std::abs(b - a) < CloseEnough;
}

TVector vertexPosition(const Vertex& vertex) noexcept
{
    return TVector{ vertex.x, vertex.y, vertex.z };
}

} // namespace

bool isOnLine(
    const double x1,
    const double y1,
    const double z1,
    const double x2,
    const double y2,
    const double z2,
    const double x,
    const double y,
    const double z) noexcept
{
    const double dx = x2 - x1;
    const double dy = y2 - y1;
    const double dz = z2 - z1;

    double nx = 0.0;
    double ny = 0.0;
    double nz = 0.0;

    if (!closeEnoughLineValue(dz, 0.0))
    {
        nx = x1 + (z - z1) * dx / dz;
        ny = y1 + (z - z1) * dy / dz;
        nz = z;
    }
    else if (!closeEnoughLineValue(dx, 0.0))
    {
        nx = x;
        ny = y1 + (x - x1) * dy / dx;
        nz = z1 + (x - x1) * dz / dx;
    }
    else
    {
        if (closeEnoughLineValue(dy, 0.0))
        {
            return false;
        }

        nx = x1 + (y - y1) * dx / dy;
        ny = y;
        nz = z1 + (y - y1) * dz / dy;
    }

    return closeEnoughLineValue(nx, x) && closeEnoughLineValue(ny, y) && closeEnoughLineValue(nz, z);
}

namespace
{

constexpr double kSectorContainmentPlaneTolerance = 1.0e-5;
// The bounds test is only a broad phase; keep it at the exact plane tolerance
// with a small arithmetic cushion so it cannot reject points the predicate accepts.
constexpr double kSectorContainmentBoundsPadding =
    kSectorContainmentPlaneTolerance + 1.0e-9;

bool isInSectorImpl(const Sector& sector, const double x, const double y, const double z, const bool onSides) noexcept
{
    if ( !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) )
    {
        return false;
    }

    for ( const Surface& surf : sector.surfaces )
    {
        if ( surf.vertices.empty() )
        {
            continue;
        }

        const std::int32_t v0Index = surf.vertices[0];
        if ( v0Index < 0 || static_cast<std::size_t>(v0Index) >= sector.vertices.size() )
        {
            return false;
        }

        const Vertex& v0 = sector.vertices[static_cast<std::size_t>(v0Index)];
        const double dist = surf.normal.dot(x - v0.x, y - v0.y, z - v0.z);
        if ( !std::isfinite(dist) )
        {
            return false;
        }
        if ( onSides )
        {
            if ( dist < -kSectorContainmentPlaneTolerance )
            {
                return false;
            }
        }
        else
        {
            if ( dist <= 0.001 )
            {
                return false;
            }
        }
    }

    // Preserve the legacy vacuous-inside result for a sector without usable
    // planes. sectorFindCollideBox relies on that compatibility behavior when
    // canonicalizing old empty IJIM sectors. Invalid or non-finite planes
    // still fail closed above.
    return true;
}

bool sectorFaceInsideNoSides(
    const Sector& sector,
    const double x1,
    const double y1,
    const double z1,
    const double x2,
    const double y2,
    const double z2,
    const double x3,
    const double y3,
    const double z3,
    const double x4,
    const double y4,
    const double z4) noexcept
{
    return isInSectorImpl(sector, x1, y1, z1, false)
        && isInSectorImpl(sector, x2, y2, z2, false)
        && isInSectorImpl(sector, x3, y3, z3, false)
        && isInSectorImpl(sector, x4, y4, z4, false);
}

} // namespace

bool sectorContainsPoint(const Sector& sector, const double x, const double y, const double z) noexcept
{
    return isInSectorImpl(sector, x, y, z, true);
}

bool sectorContainsPointNoSides(const Sector& sector, const double x, const double y, const double z) noexcept
{
    return isInSectorImpl(sector, x, y, z, false);
}

bool sectorFindCollideBox(const Sector& sector, const TBox& bbox, const double cx, const double cy, const double cz, TBox& collideBox) noexcept
{
    constexpr int kSteps = 25;

    bool bx1 = true;
    bool bx2 = true;
    bool by1 = true;
    bool by2 = true;
    bool bz1 = true;
    bool bz2 = true;

    const double xminus = (bbox.x1 - cx) / static_cast<double>(kSteps);
    const double xplus = (bbox.x2 - cx) / static_cast<double>(kSteps);
    const double yminus = (bbox.y1 - cy) / static_cast<double>(kSteps);
    const double yplus = (bbox.y2 - cy) / static_cast<double>(kSteps);
    const double zminus = (bbox.z1 - cz) / static_cast<double>(kSteps);
    const double zplus = (bbox.z2 - cz) / static_cast<double>(kSteps);

    double x1 = cx + xminus;
    double x2 = cx + xplus;
    double y1 = cy + yminus;
    double y2 = cy + yplus;
    double z1 = cz + zminus;
    double z2 = cz + zplus;

    if (!sectorFaceInsideNoSides(sector, x1, y1, z1, x1, y1, z2, x1, y2, z2, x1, y2, z1))
    {
        return false;
    }
    if (!sectorFaceInsideNoSides(sector, x2, y1, z1, x2, y1, z2, x2, y2, z2, x2, y2, z1))
    {
        return false;
    }

    for (int i = 0; i < kSteps; ++i)
    {
        if (bx1)
        {
            x1 += xminus;
            if (!sectorFaceInsideNoSides(sector, x1, y1, z1, x1, y1, z2, x1, y2, z2, x1, y2, z1))
            {
                x1 -= xminus;
                bx1 = false;
            }
        }

        if (bx2)
        {
            x2 += xplus;
            if (!sectorFaceInsideNoSides(sector, x2, y1, z1, x2, y1, z2, x2, y2, z2, x2, y2, z1))
            {
                x2 -= xplus;
                bx2 = false;
            }
        }

        if (by1)
        {
            y1 += yminus;
            if (!sectorFaceInsideNoSides(sector, x1, y1, z1, x1, y1, z2, x2, y1, z2, x2, y1, z1))
            {
                y1 -= yminus;
                by1 = false;
            }
        }

        if (by2)
        {
            y2 += yplus;
            if (!sectorFaceInsideNoSides(sector, x1, y2, z1, x1, y2, z2, x2, y2, z2, x2, y2, z1))
            {
                y2 -= yplus;
                by2 = false;
            }
        }

        if (bz1)
        {
            z1 += zminus;
            if (!sectorFaceInsideNoSides(sector, x1, y1, z1, x1, y2, z1, x2, y2, z1, x2, y1, z1))
            {
                z1 -= zminus;
                bz1 = false;
            }
        }

        if (bz2)
        {
            z2 += zplus;
            if (!sectorFaceInsideNoSides(sector, x1, y1, z2, x1, y2, z2, x2, y2, z2, x2, y1, z2))
            {
                z2 -= zplus;
                bz2 = false;
            }
        }

        if (!(bx1 || bx2 || by1 || by2 || bz1 || bz2))
        {
            break;
        }
    }

    collideBox.x1 = x1;
    collideBox.x2 = x2;
    collideBox.y1 = y1;
    collideBox.y2 = y2;
    collideBox.z1 = z1;
    collideBox.z2 = z2;
    return true;
}

namespace
{

bool sectorSurfacesSeparateOther(const Sector& source, const Sector& other) noexcept
{
    // Step 1: each source surface must have at least one vertex of the other
    // sector on its positive side. Otherwise that surface separates the two
    // convex volumes and the sectors do not overlap.
    for (const Surface& surf : source.surfaces)
    {
        if (surf.vertices.empty())
        {
            continue;
        }

        const std::int32_t v0Index = surf.vertices[0];
        if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= source.vertices.size())
        {
            return false;
        }

        const Vertex& v0 = source.vertices[static_cast<std::size_t>(v0Index)];

        bool onRight = false;
        for (const Vertex& v : other.vertices)
        {
            const double d = surf.normal.dot(vertexPosition(v) - vertexPosition(v0));
            if (d > 0.01)
            {
                onRight = true;
                break;
            }
        }

        if (!onRight)
        {
            return false;
        }
    }

    return true;
}

} // namespace

bool sectorDoOverlap(const Sector& sector1, const Sector& sector2) noexcept
{
    // Step 1: test separation from both sectors because either volume can
    // provide the separating surface.
    return sectorSurfacesSeparateOther(sector1, sector2) && sectorSurfacesSeparateOther(sector2, sector1);
}

namespace
{

bool isValidSurfaceRef(const Level& level, const SurfaceRef ref) noexcept
{
    if (ref.sector < 0 || ref.surface < 0)
    {
        return false;
    }

    const std::size_t sectorIndex = static_cast<std::size_t>(ref.sector);
    if (sectorIndex >= level.sectors.size())
    {
        return false;
    }

    const Sector& sector = level.sectors[sectorIndex];
    const std::size_t surfaceIndex = static_cast<std::size_t>(ref.surface);
    return surfaceIndex < sector.surfaces.size();
}

bool collectImportantVertices(const Sector& sector, const Surface& surface, std::vector<TVector>& out)
{
    out.clear();

    const std::int32_t count = static_cast<std::int32_t>(surface.vertices.size());
    if (count <= 0)
    {
        return true;
    }

    out.reserve(surface.vertices.size());

    for (std::int32_t i = 0; i < count; ++i)
    {
        const std::int32_t prev = prevIndex(i, count);
        const std::int32_t next = nextIndex(i, count);

        const std::int32_t v0Index = surface.vertices[static_cast<std::size_t>(prev)];
        const std::int32_t v1Index = surface.vertices[static_cast<std::size_t>(i)];
        const std::int32_t v2Index = surface.vertices[static_cast<std::size_t>(next)];

        if (v0Index < 0 || v1Index < 0 || v2Index < 0)
        {
            return false;
        }
        if (static_cast<std::size_t>(v0Index) >= sector.vertices.size()
            || static_cast<std::size_t>(v1Index) >= sector.vertices.size()
            || static_cast<std::size_t>(v2Index) >= sector.vertices.size())
        {
            return false;
        }

        const Vertex& v0 = sector.vertices[static_cast<std::size_t>(v0Index)];
        const Vertex& v1 = sector.vertices[static_cast<std::size_t>(v1Index)];
        const Vertex& v2 = sector.vertices[static_cast<std::size_t>(v2Index)];

        TVector e1 = vertexPosition(v1) - vertexPosition(v0);

        TVector e2 = vertexPosition(v2) - vertexPosition(v1);

        if (e1.normalize() == 0.0)
        {
            continue;
        }

        e2.normalize();
        if (e1 == e2)
        {
            continue;
        }

        out.push_back(vertexPosition(v1));
    }

    return true;
}

bool isCloseVertex(const TVector& a, const TVector& b) noexcept
{
    return SameValue(a.x, b.x, CloseEnough) && SameValue(a.y, b.y, CloseEnough) && SameValue(a.z, b.z, CloseEnough);
}

namespace
{

struct ProjectedVertex2
{
    double x{};
    double y{};
};

bool buildProjectedBasis(const std::vector<TVector>& polygon, TVector& axisU, TVector& axisV, TVector& normal) noexcept
{
    if (polygon.size() < 3)
    {
        return false;
    }

    for (std::size_t i = 2; i < polygon.size(); ++i)
    {
        const TVector e0 = polygon[i - 1] - polygon[0];
        const TVector e1 = polygon[i] - polygon[0];
        normal = e0.cross(e1);
        if (normal.normalize() != 0.0)
        {
            break;
        }
    }

    if (normal.length() == 0.0)
    {
        return false;
    }

    TVector refAxis = (std::abs(normal.z) < 0.9)
        ? TVector::Create(0.0, 0.0, 1.0)
        : TVector::Create(1.0, 0.0, 0.0);

    axisU = refAxis.cross(normal);
    if (axisU.normalize() == 0.0)
    {
        refAxis = TVector::Create(0.0, 1.0, 0.0);
        axisU = refAxis.cross(normal);
        if (axisU.normalize() == 0.0)
        {
            return false;
        }
    }

    axisV = normal.cross(axisU);
    return axisV.normalize() != 0.0;
}

std::vector<ProjectedVertex2> projectPolygon(
    const std::vector<TVector>& polygon,
    const TVector& axisU,
    const TVector& axisV)
{
    std::vector<ProjectedVertex2> projected{};
    projected.reserve(polygon.size());

    for (const TVector& v : polygon)
    {
        projected.push_back(ProjectedVertex2{ v.dot(axisU), v.dot(axisV) });
    }

    return projected;
}

double polygonArea2(const std::vector<ProjectedVertex2>& polygon) noexcept
{
    if (polygon.size() < 3)
    {
        return 0.0;
    }

    double area2 = 0.0;
    for (std::size_t i = 0; i < polygon.size(); ++i)
    {
        const ProjectedVertex2& a = polygon[i];
        const ProjectedVertex2& b = polygon[(i + 1) % polygon.size()];
        area2 += a.x * b.y - b.x * a.y;
    }
    return area2;
}

bool pointOnSegment2(
    const ProjectedVertex2& p,
    const ProjectedVertex2& a,
    const ProjectedVertex2& b) noexcept
{
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double cross = (p.x - a.x) * dy - (p.y - a.y) * dx;
    if (std::abs(cross) > 0.001)
    {
        return false;
    }

    const double minX = std::min(a.x, b.x) - 0.001;
    const double maxX = std::max(a.x, b.x) + 0.001;
    const double minY = std::min(a.y, b.y) - 0.001;
    const double maxY = std::max(a.y, b.y) + 0.001;
    return p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY;
}

bool pointInPolygonOrBoundary(
    const ProjectedVertex2& p,
    const std::vector<ProjectedVertex2>& polygon) noexcept
{
    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++)
    {
        const ProjectedVertex2& a = polygon[j];
        const ProjectedVertex2& b = polygon[i];

        if (pointOnSegment2(p, a, b))
        {
            return true;
        }

        const bool intersects = ((a.y > p.y) != (b.y > p.y))
            && (p.x < (b.x - a.x) * (p.y - a.y) / ((b.y - a.y) + sed::math::kTinyEpsilon) + a.x);
        if (intersects)
        {
            inside = !inside;
        }
    }

    return inside;
}

bool polygonsEquivalentProjected(const std::vector<TVector>& a, const std::vector<TVector>& b)
{
    TVector axisU{};
    TVector axisV{};
    TVector normal{};
    if (!buildProjectedBasis(a, axisU, axisV, normal))
    {
        return false;
    }

    const std::vector<ProjectedVertex2> polyA = projectPolygon(a, axisU, axisV);
    const std::vector<ProjectedVertex2> polyB = projectPolygon(b, axisU, axisV);
    if (polyA.size() < 3 || polyB.size() < 3)
    {
        return false;
    }

    const double areaA = std::abs(polygonArea2(polyA)) * 0.5;
    const double areaB = std::abs(polygonArea2(polyB)) * 0.5;
    const double areaTol = std::max(0.001, std::max(areaA, areaB) * 1.0e-4);
    if (std::abs(areaA - areaB) > areaTol)
    {
        return false;
    }

    for (const ProjectedVertex2& p : polyA)
    {
        if (!pointInPolygonOrBoundary(p, polyB))
        {
            return false;
        }
    }

    for (const ProjectedVertex2& p : polyB)
    {
        if (!pointInPolygonOrBoundary(p, polyA))
        {
            return false;
        }
    }

    return true;
}

bool cyclicVertexMatch(
    const std::vector<TVector>& a,
    const std::vector<TVector>& b,
    const int direction) noexcept
{
    if (a.size() != b.size() || a.empty())
    {
        return false;
    }

    const TVector& first = a.front();
    for (std::size_t start = 0; start < b.size(); ++start)
    {
        if (!isCloseVertex(first, b[start]))
        {
            continue;
        }

        bool ok = true;
        for (std::size_t i = 1; i < a.size(); ++i)
        {
            const std::size_t idx =
                (start + static_cast<std::size_t>((direction > 0 ? i : (b.size() - (i % b.size()))))) % b.size();
            if (!isCloseVertex(a[i], b[idx]))
            {
                ok = false;
                break;
            }
        }

        if (ok)
        {
            return true;
        }
    }

    return false;
}

} // namespace

bool surfacesMayNeedConnectFallback(
    const Sector& srcSector,
    const Surface& srcSurface,
    const Sector& dstSector,
    const Surface& dstSurface) noexcept
{
    if (srcSurface.vertices.empty() || dstSurface.vertices.empty())
    {
        return false;
    }

    TVector srcNormal = srcSurface.normal;
    TVector dstNormal = dstSurface.normal;
    if (srcNormal.length() == 0.0)
    {
        srcNormal = surfaceCalcNormalNewell(srcSector, srcSurface);
    }
    if (dstNormal.length() == 0.0)
    {
        dstNormal = surfaceCalcNormalNewell(dstSector, dstSurface);
    }

    if (srcNormal.normalize() == 0.0 || dstNormal.normalize() == 0.0)
    {
        return false;
    }

    if (srcNormal.dot(dstNormal) > -0.995)
    {
        return false;
    }

    const std::int32_t srcV0Index = srcSurface.vertices.front();
    const std::int32_t dstV0Index = dstSurface.vertices.front();
    if (srcV0Index < 0 || dstV0Index < 0
        || static_cast<std::size_t>(srcV0Index) >= srcSector.vertices.size()
        || static_cast<std::size_t>(dstV0Index) >= dstSector.vertices.size())
    {
        return false;
    }

    const Vertex& srcV0 = srcSector.vertices[static_cast<std::size_t>(srcV0Index)];
    const Vertex& dstV0 = dstSector.vertices[static_cast<std::size_t>(dstV0Index)];
    const double planeDist = std::abs(srcNormal.dot(vertexPosition(dstV0) - vertexPosition(srcV0)));
    if (planeDist > 0.02)
    {
        return false;
    }

    return doBoxesIntersect(surfaceCalcBBox(srcSector, srcSurface), surfaceCalcBBox(dstSector, dstSurface));
}

bool resolveSurfaceNormal(const Sector& sector, const Surface& surface, TVector& normal) noexcept
{
    normal = surfaceCalcNormalNewell(sector, surface);
    return precision::finite(normal) && normal.normalize() != 0.0;
}

bool surfaceBoundaryMatches(
    const Sector& source, const Surface& face,
    const Sector& target, const Surface& boundary) noexcept
{
    if ( face.vertices.size() < 3 || boundary.vertices.size() < 3 )
    {
        return false;
    }
    for ( const auto index : face.vertices )
    {
        if ( index < 0 || static_cast<std::size_t>(index) >= source.vertices.size() )
        {
            return false;
        }
        const TVector point = precision::position(source.vertices[index]);
        bool matched = false;
        for ( std::size_t edge = 0; edge < boundary.vertices.size(); ++edge )
        {
            const auto first = boundary.vertices[edge];
            const auto last = boundary.vertices[(edge + 1) % boundary.vertices.size()];
            if ( first < 0 || last < 0 || static_cast<std::size_t>(first) >= target.vertices.size()
                || static_cast<std::size_t>(last) >= target.vertices.size() )
            {
                return false;
            }
            double parameter{};
            if ( precision::onSegment(point, precision::position(target.vertices[first]),
                    precision::position(target.vertices[last]), parameter) )
            {
                matched = true;
                break;
            }
        }
        if ( !matched )
        {
            return false;
        }
    }
    return true;
}

} // namespace

void surfaceSetAdjoinParams(const ProjectType projectType, Surface& surface) noexcept
{
    surface.surfaceFlags = 0;
    surface.faceFlags    = 0;
    surface.geo          = 0;

    surface.adjoinFlags = SAF_Visible | SAF_Move;
    if (projectType != ProjectType::IJIM)
    {
        surface.adjoinFlags |= SAF_AllowSoundPass;
    }
}

void surfaceClearAdjoinParams(Surface& surface) noexcept
{
    surface.surfaceFlags = SF_Collision;
    surface.faceFlags    = 0;
    surface.geo          = 4;

    if (surface.material.empty())
    {
        surface.material = "dflt.mat";
    }
}

void surfaceCopyAdjoinParams(const Surface& src, Surface& dst)
{
    // Complete the only allocating copy before publishing any destination field.
    std::string material = src.material;
    dst.surfaceFlags = src.surfaceFlags;
    dst.faceFlags    = src.faceFlags;
    dst.geo          = src.geo;
    dst.adjoinFlags  = src.adjoinFlags;
    dst.material.swap(material);
}

bool levelSysAdjoinSurfaces(Level& level, const SurfaceRef surf1, const SurfaceRef surf2, const bool reset) noexcept
{
    if (!isValidSurfaceRef(level, surf1) || !isValidSurfaceRef(level, surf2))
    {
        return false;
    }

    Surface& a = level.sectors[static_cast<std::size_t>(surf1.sector)].surfaces[static_cast<std::size_t>(surf1.surface)];
    Surface& b = level.sectors[static_cast<std::size_t>(surf2.sector)].surfaces[static_cast<std::size_t>(surf2.surface)];

    a.adjoin = surf2;
    b.adjoin = surf1;
    a.nAdj = -1;
    b.nAdj = -1;

    if (reset)
    {
        surfaceSetAdjoinParams(level.kind, a);
        surfaceSetAdjoinParams(level.kind, b);
    }

    return true;
}

bool surfaceDoOverlap(const Sector& sector1, const Surface& surface1, const Sector& sector2, const Surface& surface2) noexcept
{
    try
    {
        return precision::surfaceDoOverlapChecked(sector1, surface1, sector2, surface2);
    }
    catch ( const std::bad_alloc& )
    {
        return false;
    }
}

bool precision::surfaceDoOverlapChecked(const Sector& sector1, const Surface& surface1,
    const Sector& sector2, const Surface& surface2)
{
    if (surface1.vertices.empty() || surface2.vertices.empty())
    {
        return false;
    }

    // Step 1: resolve usable normals for both surfaces. Zero cached normals
    // are recalculated from geometry before the overlap test gives up.
    TVector normal1{};
    TVector normal2{};
    if (!resolveSurfaceNormal(sector1, surface1, normal1) || !resolveSurfaceNormal(sector2, surface2, normal2))
    {
        return false;
    }

    // Step 2: adjoined surfaces must face each other.
    if (normal1.dot(normal2) > -0.995)
    {
        return false;
    }

    // Proximity can locate a candidate, but publishing a portal requires
    // coincident 3D boundaries. Projection must never discard a real gap or
    // tilt. Mutual segment matching retains redundant collinear corners.
    if (!surfaceBoundaryMatches(sector1, surface1, sector2, surface2)
        || !surfaceBoundaryMatches(sector2, surface2, sector1, surface1))
    {
        return false;
    }
    if ( !sourceSurfaceIsPlanar(sector1, surface1) || !sourceSurfaceIsConvex(sector1, surface1)
        || !sourceSurfaceIsPlanar(sector2, surface2) || !sourceSurfaceIsConvex(sector2, surface2) )
    {
        return false;
    }

    // Step 3: compare the two planes before doing heavier polygon work.
    const std::int32_t v0Index = surface1.vertices.front();
    const std::int32_t v1Index = surface2.vertices.front();
    if (v0Index < 0 || v1Index < 0
        || static_cast<std::size_t>(v0Index) >= sector1.vertices.size()
        || static_cast<std::size_t>(v1Index) >= sector2.vertices.size())
    {
        return false;
    }

    const Vertex& v0 = sector1.vertices[static_cast<std::size_t>(v0Index)];
    const Vertex& v1 = sector2.vertices[static_cast<std::size_t>(v1Index)];
    const double planeDist = std::abs(normal1.dot(vertexPosition(v1) - vertexPosition(v0)));
    const auto planeContext=precision::surfacePlaneError(sector1,surface1,normal1);
    const double allowance=planeContext.at(normal1,vertexPosition(v1),vertexPosition(v0));
    if (!std::isfinite(planeDist) || allowance<0.0 || planeDist>allowance)
    {
        // Matching rounded boundaries need not have the same residual at their
        // first corner. Preserve the exact ordered 3D boundary contract; do not
        // flatten the faces or reinterpret a proximity match as an adjoin.
        Surface first = surface1;
        Surface second = surface2;
        first.normal = normal1;
        second.normal = normal2;
        return surfaceBoundariesMatchChecked(sector1, first, sector2, second);
    }

    // Step 4: reduce both polygons to important vertices, skipping redundant
    // collinear runs so exact and projected matching are stable across cuts.
    std::vector<TVector> vxs0{};
    std::vector<TVector> vxs1{};

    if (!collectImportantVertices(sector1, surface1, vxs0))
    {
        return false;
    }
    if (!collectImportantVertices(sector2, surface2, vxs1))
    {
        return false;
    }

    if (vxs0.empty())
    {
        return false;
    }

    // Step 5: prefer exact cyclic equality, then fall back to projected polygon
    // equivalence for surfaces with different but geometrically equivalent edge
    // layouts.
    if (cyclicVertexMatch(vxs0, vxs1, -1) || cyclicVertexMatch(vxs0, vxs1, +1))
    {
        return true;
    }

    return polygonsEquivalentProjected(vxs0, vxs1);
}

bool precision::makeAdjoinFromSectorChecked(Level& level, const SurfaceRef surface, const std::int32_t startSector, const bool reset)
{
    if (!isValidSurfaceRef(level, surface))
    {
        return false;
    }

    Sector& srcSector = level.sectors[static_cast<std::size_t>(surface.sector)];
    Surface& srcSurf = srcSector.surfaces[static_cast<std::size_t>(surface.surface)];
    if (srcSurf.adjoin)
    {
        return false;
    }

    const std::int32_t start = std::max<std::int32_t>(0, startSector);
    for (std::int32_t s = start; s < static_cast<std::int32_t>(level.sectors.size()); ++s)
    {
        Sector& sector = level.sectors[static_cast<std::size_t>(s)];
        for (std::int32_t sf = 0; sf < static_cast<std::int32_t>(sector.surfaces.size()); ++sf)
        {
            if (s == surface.sector)
            {
                continue;
            }

            const SurfaceRef otherRef{ s, sf };
            Surface& other = sector.surfaces[static_cast<std::size_t>(sf)];

            if (precision::surfaceDoOverlapChecked(srcSector, srcSurf, sector, other))
            {
                return levelSysAdjoinSurfaces(level, surface, otherRef, reset);
            }
        }
    }

    // Fallback: if a candidate is coplanar/back-to-back and roughly overlaps, try the heavier surface
    // connect path on a temporary level. This lets auto-adjoin cut the larger face when exact vertex parity
    // is missing.
    for (std::int32_t s = start; s < static_cast<std::int32_t>(level.sectors.size()); ++s)
    {
        if (s == surface.sector)
        {
            continue;
        }

        const Sector& sector = level.sectors[static_cast<std::size_t>(s)];
        for (std::int32_t sf = 0; sf < static_cast<std::int32_t>(sector.surfaces.size()); ++sf)
        {
            const SurfaceRef otherRef{ s, sf };
            const Surface& other = sector.surfaces[static_cast<std::size_t>(sf)];

            if (other.adjoin)
            {
                continue;
            }

            if (!surfacesMayNeedConnectFallback(srcSector, srcSurf, sector, other))
            {
                continue;
            }

            Level candidate = level;
            if (!precision::connectSurfacesChecked(candidate, surface, otherRef))
            {
                continue;
            }

            level = std::move(candidate);
            return true;
        }
    }

    return false;
}

bool levelMakeAdjoinFromSector(Level& level, const SurfaceRef surface,
    const std::int32_t startSector, const bool reset) noexcept
{
    try
    {
        return precision::makeAdjoinFromSectorChecked(level, surface, startSector, reset);
    }
    catch (...)
    {
        return false;
    }
}

bool levelMakeAdjoin(Level& level, const SurfaceRef surface, const bool reset) noexcept
{
    return levelMakeAdjoinFromSector(level, surface, 0, reset);
}

bool levelUnadjoinSurface(Level& level, const SurfaceRef surface) noexcept
{
    if (!isValidSurfaceRef(level, surface))
    {
        return false;
    }

    Surface& surf = level.sectors[static_cast<std::size_t>(surface.sector)].surfaces[static_cast<std::size_t>(surface.surface)];
    if (!surf.adjoin)
    {
        return false;
    }

    const SurfaceRef otherRef = *surf.adjoin;
    if (!isValidSurfaceRef(level, otherRef))
    {
        surfaceClearAdjoinParams(surf);
        surf.adjoin.reset();
        surf.nAdj = -1;
        return true;
    }

    Surface& other = level.sectors[static_cast<std::size_t>(otherRef.sector)].surfaces[static_cast<std::size_t>(otherRef.surface)];
    if (!other.adjoin || other.adjoin->sector != surface.sector || other.adjoin->surface != surface.surface)
    {
        surfaceClearAdjoinParams(surf);
        surf.adjoin.reset();
        surf.nAdj = -1;
        return true;
    }

    surfaceClearAdjoinParams(surf);
    surfaceClearAdjoinParams(other);

    surfaceCheckIfFloor(surf);
    surfaceCheckIfFloor(other);

    other.adjoin.reset();
    surf.adjoin.reset();
    other.nAdj = -1;
    surf.nAdj = -1;
    return true;
}

bool levelUnadjoinSectors(Level& level, const std::int32_t sector, const std::int32_t otherSector) noexcept
{
    if (!isValidSurfaceRef(level, SurfaceRef{ sector, 0 }) || otherSector < 0)
    {
        return false;
    }

    Sector& sec = level.sectors[static_cast<std::size_t>(sector)];

    bool result = false;
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(sec.surfaces.size()); ++i)
    {
        const Surface& surf = sec.surfaces[static_cast<std::size_t>(i)];
        if (!surf.adjoin)
        {
            continue;
        }
        if (surf.adjoin->sector == otherSector
            && levelUnadjoinSurface(level, SurfaceRef{ sector, i }))
        {
            result = true;
        }
    }

    return result;
}

bool levelAdjoinSectors(Level& level, const std::int32_t sector, const std::int32_t otherSector, const bool reset) noexcept
{
    if (!isValidSurfaceRef(level, SurfaceRef{ sector, 0 }) || !isValidSurfaceRef(level, SurfaceRef{ otherSector, 0 }))
    {
        return false;
    }

    Sector& sec = level.sectors[static_cast<std::size_t>(sector)];
    Sector& sec1 = level.sectors[static_cast<std::size_t>(otherSector)];

    bool result = false;
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(sec.surfaces.size()); ++i)
    {
        Surface& surf = sec.surfaces[static_cast<std::size_t>(i)];
        if (surf.adjoin)
        {
            continue;
        }

        for (std::int32_t j = 0; j < static_cast<std::int32_t>(sec1.surfaces.size()); ++j)
        {
            Surface& surf1 = sec1.surfaces[static_cast<std::size_t>(j)];
            if (surf1.adjoin)
            {
                continue;
            }

            if (surfaceDoOverlap(sec, surf, sec1, surf1))
            {
                (void)levelSysAdjoinSurfaces(level, SurfaceRef{ sector, i }, SurfaceRef{ otherSector, j }, reset);
                result = true;
            }
        }
    }

    return result;
}

bool surfaceIsPointOn(const Sector& sector, const Surface& surface, const double x, const double y, const double z) noexcept
{
    const std::size_t count = surface.vertices.size();
    if (count < 3)
    {
        return false;
    }

    const TVector point{ x, y, z };

    for (std::size_t i = 0; i < count; ++i)
    {
        const std::int32_t v1Index = surface.vertices[i];
        const std::int32_t v2Index = surface.vertices[(i + 1) % count];
        if (v1Index < 0 || v2Index < 0 || static_cast<std::size_t>(v1Index) >= sector.vertices.size() || static_cast<std::size_t>(v2Index) >= sector.vertices.size())
        {
            return false;
        }

        const Vertex& v1 = sector.vertices[static_cast<std::size_t>(v1Index)];
        const Vertex& v2 = sector.vertices[static_cast<std::size_t>(v2Index)];

        const TVector edge = vertexPosition(v2) - vertexPosition(v1);
        const TVector vct = surface.normal.cross(edge);
        const double dist = vct.dot(point - vertexPosition(v1));
        if (dist < -CloseEnough)
        {
            return false;
        }
    }

    return true;
}

bool surfaceIsPlanar(const Sector& sector, const Surface& surface) noexcept
{
    const precision::SurfacePlaneError errorContext=precision::surfacePlaneError(sector,surface);
    if (!errorContext.valid) return false;
    if (surface.vertices.size() < 3)
    {
        return false;
    }

    if (surface.normal.isZero())
    {
        return false;
    }

    const std::int32_t v0Index = surface.vertices[0];
    if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= sector.vertices.size())
    {
        return false;
    }

    const Vertex& v0 = sector.vertices[static_cast<std::size_t>(v0Index)];

    for (const std::int32_t vIndex : surface.vertices)
    {
        if (vIndex < 0 || static_cast<std::size_t>(vIndex) >= sector.vertices.size())
        {
            return false;
        }

        const Vertex& v = sector.vertices[static_cast<std::size_t>(vIndex)];
        const double distance = surface.normal.dot(vertexPosition(v) - vertexPosition(v0));
        const double allowance=errorContext.at(surface.normal,vertexPosition(v),vertexPosition(v0));
        if (!std::isfinite(distance) || allowance<0.0 || std::abs(distance)>allowance)
        {
            return false;
        }
    }

    return true;
}

namespace
{
// Pascal's consistency checks admitted coordinate quantization at these
// physical scales. They are diagnostic limits, never editing weld distances.
constexpr double kDiagnosticPlaneDistance = 0.001;
constexpr double kDiagnosticCornerTurn = 0.0001;

bool diagnosticSurfaceNormal(const Sector& sector, const Surface& surface, TVector& normal) noexcept
{
    if ( surface.vertices.size() < 3 || !precision::finite(surface.normal) )
    {
        return false;
    }
    for ( const auto index : surface.vertices )
    {
        if ( index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size()
            || !precision::finite(vertexPosition(sector.vertices[static_cast<std::size_t>(index)])) )
        {
            return false;
        }
    }
    const double scale = std::max({std::abs(surface.normal.x), std::abs(surface.normal.y),
        std::abs(surface.normal.z)});
    if ( !(scale > 0.0) )
    {
        return false;
    }
    normal = {surface.normal.x / scale, surface.normal.y / scale, surface.normal.z / scale};
    if ( normal.normalize() == 0.0 )
    {
        return false;
    }
    const TVector geometric = surfaceCalcNormalNewell(sector, surface);
    return precision::finite(geometric) && normal.dot(geometric) > 0.0;
}

bool exactPoint(const TVector& first, const TVector& second) noexcept
{
    return precision::finite(first) && precision::finite(second) && first.x == second.x
        && first.y == second.y && first.z == second.z;
}

std::vector<TVector> diagnosticBoundary(const Sector& sector, const Surface& surface)
{
    std::vector<TVector> points;
    points.reserve(surface.vertices.size());
    for ( const auto index : surface.vertices )
    {
        const TVector point = vertexPosition(sector.vertices[static_cast<std::size_t>(index)]);
        if ( points.empty() || !precision::samePoint(point, points.back()) )
        {
            points.push_back(point);
        }
    }
    if ( points.size() > 1 && precision::samePoint(points.front(), points.back()) )
    {
        points.pop_back();
    }
    // Preserve traversal order. Vertex-set or projected overlap alone would
    // incorrectly equate a valid loop with a crossed permutation of its corners.
    for ( std::size_t corner = 0; points.size() >= 3 && corner < points.size(); )
    {
        const auto count = points.size();
        double parameter{};
        if ( precision::onSegment(points[corner], points[(corner + count - 1) % count],
                points[(corner + 1) % count], parameter) )
        {
            points.erase(points.begin() + static_cast<std::ptrdiff_t>(corner));
            corner = 0;
        }
        else
        {
            ++corner;
        }
    }
    return points;
}

std::vector<TVector> diagnosticOrderedBoundary(const Sector& sector, const Surface& surface)
{
    std::vector<TVector> points;
    points.reserve(surface.vertices.size());
    for ( const auto index : surface.vertices )
    {
        points.push_back(vertexPosition(sector.vertices[static_cast<std::size_t>(index)]));
    }
    // A repeated closing coordinate is syntax for a closed loop. Preserve
    // every merely nearby coordinate: this matcher must not weld a
    // representable thin edge while checking topology.
    if ( points.size() > 1 && exactPoint(points.front(), points.back()) )
    {
        points.pop_back();
    }
    return points;
}

bool diagnosticBoundaryHasValidTopology(const Sector& sector, const Surface& surface,
    const std::vector<TVector>& points) noexcept
{
    if ( points.size() < 3 )
    {
        return false;
    }
    for ( std::size_t first = 0; first < points.size(); ++first )
    {
        if ( !precision::finite(points[first]) )
        {
            return false;
        }
        for ( std::size_t second = first + 1; second < points.size(); ++second )
        {
            if ( exactPoint(points[first], points[second]) )
            {
                return false;
            }
        }
    }
    for ( std::size_t corner = 0; corner < points.size(); ++corner )
    {
        const TVector& start = points[(corner + points.size() - 1) % points.size()];
        const TVector& middle = points[corner];
        const TVector& end = points[(corner + 1) % points.size()];
        const std::array<long double, 3> a{{start.x, start.y, start.z}};
        const std::array<long double, 3> b{{middle.x, middle.y, middle.z}};
        const std::array<long double, 3> c{{end.x, end.y, end.z}};
        const std::array<long double, 3> firstEdge{{b[0] - a[0], b[1] - a[1], b[2] - a[2]}};
        const std::array<long double, 3> secondEdge{{c[0] - b[0], c[1] - b[1], c[2] - b[2]}};
        const std::array<long double, 3> cross{{
            firstEdge[1] * secondEdge[2] - firstEdge[2] * secondEdge[1],
            firstEdge[2] * secondEdge[0] - firstEdge[0] * secondEdge[2],
            firstEdge[0] * secondEdge[1] - firstEdge[1] * secondEdge[0]}};
        if ( cross[0] != 0.0L || cross[1] != 0.0L || cross[2] != 0.0L )
        {
            continue;
        }
        std::array<long double, 3> span{{c[0] - a[0], c[1] - a[1], c[2] - a[2]}};
        std::size_t dominant = 0;
        for ( std::size_t axis = 1; axis < span.size(); ++axis )
        {
            if ( std::abs(span[axis]) > std::abs(span[dominant]) )
            {
                dominant = axis;
            }
        }
        if ( span[dominant] == 0.0L )
        {
            return false;
        }
        const long double parameter = (b[dominant] - a[dominant]) / span[dominant];
        if ( parameter <= 0.0L || parameter >= 1.0L )
        {
            return false;
        }
    }
    const precision::SurfaceArea area = precision::surfaceArea(sector, surface);
    if ( !area.valid )
    {
        return false;
    }
    const long double magnitude = std::hypot(area.value[0], area.value[1], area.value[2]);
    const long double arithmeticUncertainty =
        std::hypot(area.arithmeticError[0], area.arithmeticError[1], area.arithmeticError[2]);
    // The input coordinates are already representable document values. The
    // full SurfaceArea error also models a possible perturbation of those
    // values and can therefore erase a real sub-ULP boundary when the face is
    // translated far from the origin. Arithmetic error is the relevant guard
    // for this exact topology proof; exact zero remains rejected.
    return std::isfinite(magnitude) && std::isfinite(arithmeticUncertainty)
        && magnitude > arithmeticUncertainty;
}
}

bool surfaceIsPlanarForDiagnostics(const Sector& sector, const Surface& surface) noexcept
{
    TVector normal;
    if ( !diagnosticSurfaceNormal(sector, surface, normal) )
    {
        return false;
    }
    const TVector anchor = vertexPosition(sector.vertices[surface.vertices.front()]);
    for ( const auto index : surface.vertices )
    {
        const TVector point = vertexPosition(sector.vertices[index]);
        const double distance = normal.dot(point - anchor);
        const double allowance = kDiagnosticPlaneDistance + precision::planeError(normal, point, anchor);
        if ( !std::isfinite(distance) || !std::isfinite(allowance) || std::abs(distance) > allowance )
        {
            return false;
        }
    }
    return true;
}

bool surfaceIsConvexForDiagnostics(const Sector& sector, const Surface& surface) noexcept
{
    TVector normal;
    if ( !diagnosticSurfaceNormal(sector, surface, normal) )
    {
        return false;
    }
    const auto count = surface.vertices.size();
    for ( std::size_t corner = 0; corner < count; ++corner )
    {
        const TVector first = vertexPosition(sector.vertices[surface.vertices[corner]]);
        const TVector center = vertexPosition(sector.vertices[surface.vertices[(corner + 1) % count]]);
        const TVector last = vertexPosition(sector.vertices[surface.vertices[(corner + 2) % count]]);
        const double turn = (last - center).cross(first - center).dot(normal);
        const double allowance = kDiagnosticCornerTurn + precision::cornerTurnError(first, center, last, normal);
        if ( !std::isfinite(turn) || !std::isfinite(allowance) || turn < -allowance )
        {
            return false;
        }
    }
    return true;
}

bool sectorIsConvexForDiagnostics(const Sector& sector) noexcept
{
    if ( sector.vertices.empty() || sector.surfaces.empty() )
    {
        return false;
    }
    for ( const auto& vertex : sector.vertices )
    {
        if ( !precision::finite(vertexPosition(vertex)) )
        {
            return false;
        }
    }
    for ( const auto& surface : sector.surfaces )
    {
        TVector normal;
        if ( !diagnosticSurfaceNormal(sector, surface, normal) )
        {
            return false;
        }
        const TVector anchor = vertexPosition(sector.vertices[surface.vertices.front()]);
        for ( const auto& vertex : sector.vertices )
        {
            const TVector point = vertexPosition(vertex);
            const double distance = normal.dot(point - anchor);
            const double allowance = kDiagnosticPlaneDistance + precision::planeError(normal, point, anchor);
            if ( !std::isfinite(distance) || !std::isfinite(allowance) || distance < -allowance )
            {
                return false;
            }
        }
    }
    return true;
}

namespace
{
// Source admission is capped at 0.3% of intrinsic feature width, as well as
// the legacy physical limit. Width is 2*area/perimeter, so a long thin face
// cannot hide material warp behind its long axis. No descendant can spend
// this admission limit: operations retain the measured original residuals.
constexpr double kSourceRelativeRounding = 0.003;

struct SourceFaceFrame
{
    TVector normal;
    TVector anchor;
    double width{};
};

std::optional<SourceFaceFrame> sourceFaceFrame(const Sector& sector, const Surface& surface) noexcept
{
    if ( surface.vertices.size() < 3 || !precision::finite(surface.normal) )
    {
        return std::nullopt;
    }
    for ( const auto index : surface.vertices )
    {
        if ( index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size()
            || !precision::finite(vertexPosition(sector.vertices[index])) )
        {
            return std::nullopt;
        }
    }
    SourceFaceFrame frame;
    frame.normal = surfaceCalcNormalNewell(sector, surface);
    if ( !precision::finite(frame.normal) || frame.normal.isZero() )
    {
        return std::nullopt;
    }
    if ( !surface.normal.isZero() )
    {
        const double normalScale = std::max({std::abs(surface.normal.x), std::abs(surface.normal.y),
            std::abs(surface.normal.z)});
        const TVector cached{surface.normal.x / normalScale, surface.normal.y / normalScale,
            surface.normal.z / normalScale};
        if ( cached.dot(frame.normal) <= 0.0 )
        {
            return std::nullopt;
        }
    }
    frame.anchor = vertexPosition(sector.vertices[surface.vertices.front()]);
    double scale = 0;
    for ( const auto index : surface.vertices )
    {
        const TVector delta = vertexPosition(sector.vertices[index]) - frame.anchor;
        scale = std::max({scale, std::abs(delta.x), std::abs(delta.y), std::abs(delta.z)});
    }
    if ( !std::isfinite(scale) || !(scale > 0.0) )
    {
        return std::nullopt;
    }
    long double twiceArea = 0;
    long double perimeter = 0;
    for ( std::size_t corner = 0; corner < surface.vertices.size(); ++corner )
    {
        const auto next = (corner + 1) % surface.vertices.size();
        const TVector deltaA = vertexPosition(sector.vertices[surface.vertices[corner]]) - frame.anchor;
        const TVector deltaB = vertexPosition(sector.vertices[surface.vertices[next]]) - frame.anchor;
        const TVector a{deltaA.x / scale, deltaA.y / scale, deltaA.z / scale};
        const TVector b{deltaB.x / scale, deltaB.y / scale, deltaB.z / scale};
        twiceArea += a.cross(b).dot(frame.normal);
        const TVector edge = b - a;
        perimeter += std::hypot(edge.x, edge.y, edge.z);
    }
    if ( !(perimeter > 0.0L) || !(twiceArea > 0.0L) )
    {
        return std::nullopt;
    }
    frame.width = static_cast<double>(twiceArea / perimeter * scale);
    return std::isfinite(frame.width) && frame.width > 0.0 ? std::optional{frame} : std::nullopt;
}

bool projectedSegmentsCross(const TVector& a, const TVector& b, const TVector& c, const TVector& d,
    const TVector& normal) noexcept
{
    const auto sign = [&](const TVector& p, const TVector& q, const TVector& r)
    {
        const double turn = (q - p).cross(r - p).dot(normal);
        const double error = precision::cornerTurnError(p, q, r, normal);
        if ( !std::isfinite(turn) || !std::isfinite(error) )
        {
            return 2;
        }
        return turn > error ? 1 : turn < -error ? -1 : 0;
    };
    const int abc = sign(a, b, c), abd = sign(a, b, d);
    const int cda = sign(c, d, a), cdb = sign(c, d, b);
    if ( abc == 2 || abd == 2 || cda == 2 || cdb == 2 )
    {
        return true;
    }
    if ( abc * abd < 0 && cda * cdb < 0 )
    {
        return true;
    }
    const auto onProjectedSegment = [&](const TVector& point, const TVector& first, const TVector& last)
    {
        const TVector projected = point - normal * normal.dot(point - first);
        const TVector projectedLast = last - normal * normal.dot(last - first);
        double amount{};
        return precision::onSegment(projected, first, projectedLast, amount);
    };
    return (abc == 0 && onProjectedSegment(c, a, b)) || (abd == 0 && onProjectedSegment(d, a, b))
        || (cda == 0 && onProjectedSegment(a, c, d)) || (cdb == 0 && onProjectedSegment(b, c, d));
}
}

bool precision::sourceSurfaceIsPlanar(const Sector& sector, const Surface& surface) noexcept
{
    const auto frame = sourceFaceFrame(sector, surface);
    if ( !frame )
    {
        return false;
    }
    const double limit = std::min(kDiagnosticPlaneDistance, kSourceRelativeRounding * frame->width);
    for ( const auto index : surface.vertices )
    {
        const TVector point = vertexPosition(sector.vertices[index]);
        const double distance = frame->normal.dot(point - frame->anchor);
        const double error = planeError(frame->normal, point, frame->anchor);
        if ( !std::isfinite(distance) || !std::isfinite(error) || std::abs(distance) > limit + error )
        {
            return false;
        }
    }
    return true;
}

bool precision::surfaceHasSimpleBoundary(const Sector& sector, const Surface& surface) noexcept
{
    const auto frame = sourceFaceFrame(sector, surface);
    if ( !frame )
    {
        return false;
    }
    const auto count = surface.vertices.size();
    const auto point = [&](const std::size_t corner)
    {
        return vertexPosition(sector.vertices[surface.vertices[corner % count]]);
    };
    for ( std::size_t corner = 0; corner < count; ++corner )
    {
        const TVector a = point(corner), b = point(corner + 1), c = point(corner + 2);
        if ( samePoint(a, b) || samePoint(b, c) || samePoint(a, c) )
        {
            return false;
        }
        const double turn = (b - a).cross(c - b).dot(frame->normal);
        const double error = cornerTurnError(a, b, c, frame->normal);
        if ( !std::isfinite(turn) || !std::isfinite(error) )
        {
            return false;
        }
        // Collinear boundary subdivisions may continue forward, never reverse.
        if ( std::abs(turn) <= error && (b - a).dot(c - b) < 0.0 )
        {
            return false;
        }
        for ( std::size_t other = corner + 2; other < count; ++other )
        {
            if ( (other + 1) % count != corner
                && projectedSegmentsCross(a, b, point(other), point(other + 1), frame->normal) )
            {
                return false;
            }
        }
    }
    return true;
}

bool precision::sourceSurfaceIsConvex(const Sector& sector, const Surface& surface) noexcept
{
    const auto frame = sourceFaceFrame(sector, surface);
    if ( !frame || !surfaceHasSimpleBoundary(sector, surface) )
    {
        return false;
    }
    const auto count = surface.vertices.size();
    const auto point = [&](const std::size_t corner)
    {
        return vertexPosition(sector.vertices[surface.vertices[corner % count]]);
    };
    for ( std::size_t corner = 0; corner < count; ++corner )
    {
        const TVector a = point(corner), b = point(corner + 1), c = point(corner + 2);
        const TVector chord = c - a;
        const double chordLength = std::hypot(chord.x, chord.y, chord.z);
        const double turn = (b - a).cross(c - b).dot(frame->normal);
        const double error = cornerTurnError(a, b, c, frame->normal);
        const double limit = std::min(kDiagnosticCornerTurn,
            kSourceRelativeRounding * frame->width * chordLength);
        if ( !std::isfinite(chordLength) || !std::isfinite(turn) || !std::isfinite(error)
            || !std::isfinite(limit) || turn < -limit - error )
        {
            return false;
        }
    }
    return true;
}

bool precision::sourceSectorIsConvex(const Sector& sector) noexcept
{
    if ( sector.vertices.empty() || sector.surfaces.empty() )
    {
        return false;
    }
    for ( const auto& vertex : sector.vertices )
    {
        if ( !finite(vertexPosition(vertex)) )
        {
            return false;
        }
    }
    for ( const auto& surface : sector.surfaces )
    {
        const auto frame = sourceFaceFrame(sector, surface);
        if ( !frame )
        {
            return false;
        }
        double minimum = 0, maximum = 0;
        for ( const auto& vertex : sector.vertices )
        {
            const double distance = frame->normal.dot(vertexPosition(vertex) - frame->anchor);
            if ( !std::isfinite(distance) )
            {
                return false;
            }
            minimum = std::min(minimum, distance);
            maximum = std::max(maximum, distance);
        }
        const double thickness = maximum - minimum;
        if ( !std::isfinite(thickness) || !(thickness > 0.0) )
        {
            return false;
        }
        const double limit = std::min(kDiagnosticPlaneDistance, kSourceRelativeRounding * thickness);
        for ( const auto& vertex : sector.vertices )
        {
            const TVector point = vertexPosition(vertex);
            const double distance = frame->normal.dot(point - frame->anchor);
            const double error = planeError(frame->normal, point, frame->anchor);
            if ( !std::isfinite(error) || distance < -limit - error )
            {
                return false;
            }
        }
    }
    return true;
}

bool precision::surfaceBoundariesMatchChecked(const Sector& firstSector, const Surface& first,
    const Sector& secondSector, const Surface& second)
{
    TVector firstNormal, secondNormal;
    if ( !diagnosticSurfaceNormal(firstSector, first, firstNormal)
        || !diagnosticSurfaceNormal(secondSector, second, secondNormal)
        || firstNormal.dot(secondNormal) > -0.995 )
    {
        return false;
    }
    const auto exactFirst = diagnosticOrderedBoundary(firstSector, first);
    const auto exactSecond = diagnosticOrderedBoundary(secondSector, second);
    if ( diagnosticBoundaryHasValidTopology(firstSector, first, exactFirst)
        && diagnosticBoundaryHasValidTopology(secondSector, second, exactSecond)
        && exactFirst.size() == exactSecond.size() )
    {
        for ( std::size_t start = 0; start < exactSecond.size(); ++start )
        {
            bool matches = true;
            for ( std::size_t corner = 0; corner < exactFirst.size(); ++corner )
            {
                if ( !precision::samePoint(exactFirst[corner],
                        exactSecond[(start + exactSecond.size() - corner) % exactSecond.size()]) )
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
    }
    const auto a = diagnosticBoundary(firstSector, first);
    const auto b = diagnosticBoundary(secondSector, second);
    if ( a.size() < 3 || a.size() != b.size() )
    {
        return false;
    }
    for ( std::size_t start = 0; start < b.size(); ++start )
    {
        bool matches = true;
        for ( std::size_t corner = 0; corner < a.size(); ++corner )
        {
            if ( !precision::samePoint(a[corner], b[(start + b.size() - corner) % b.size()]) )
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
}

bool surfaceBoundariesMatchForDiagnostics(const Sector& firstSector, const Surface& first,
    const Sector& secondSector, const Surface& second) noexcept
{
    try
    {
        return precision::surfaceBoundariesMatchChecked(firstSector, first, secondSector, second);
    }
    catch (...)
    {
        return false;
    }
}

namespace
{

constexpr double kFlattenPlaneEpsilon = 0.001;
constexpr double kFlattenDirectionEpsilon = 0.01;
constexpr double kFlattenMaxMoveSqr = 1.0;

bool surfaceUsesVertex(const Surface& surface, const std::int32_t vertexIndex) noexcept
{
    return std::find(surface.vertices.begin(), surface.vertices.end(), vertexIndex) != surface.vertices.end();
}

void findAdjacentSurfacesForVertex(
    const Sector& sector,
    const Surface& flattenedSurface,
    const std::int32_t vertexIndex,
    const Surface*& first,
    const Surface*& second) noexcept
{
    first = nullptr;
    second = nullptr;

    // Step 1: find up to two neighboring surfaces that share this vertex.
    // Those normals define the line or plane along which the vertex can move.
    for (const Surface& surface : sector.surfaces)
    {
        if (&surface == &flattenedSurface || !surfaceUsesVertex(surface, vertexIndex))
        {
            continue;
        }

        if (first == nullptr)
        {
            first = &surface;
            continue;
        }

        second = &surface;
        return;
    }
}

void calcFlattenDirectionForVertex(
    const Sector& sector,
    const Surface& flattenedSurface,
    const std::int32_t vertexIndex,
    TVector& out) noexcept
{
    const Surface* first = nullptr;
    const Surface* second = nullptr;
    findAdjacentSurfacesForVertex(sector, flattenedSurface, vertexIndex, first, second);

    // Step 1: isolated vertices slide along the flattened surface normal.
    if (first == nullptr && second == nullptr)
    {
        out = flattenedSurface.normal;
        return;
    }

    // Step 2: a single neighbor constrains movement to the neighbor plane.
    // Cross twice to get a vector lying on that neighbor while still moving
    // toward the flattened plane when possible.
    if (first != nullptr && second == nullptr)
    {
        const TVector crossNormal = flattenedSurface.normal.cross(first->normal);
        out = crossNormal.cross(first->normal);
        if (out.length() < kFlattenDirectionEpsilon)
        {
            out = first->normal;
        }
        return;
    }

    if (first == nullptr || second == nullptr)
    {
        out = flattenedSurface.normal;
        return;
    }

    // Step 3: two neighbors constrain movement to their plane intersection.
    out = first->normal.cross(second->normal);
    if (out.length() < kFlattenDirectionEpsilon)
    {
        const TVector crossNormal = flattenedSurface.normal.cross(first->normal);
        out = crossNormal.cross(first->normal);
        if (out.length() < kFlattenDirectionEpsilon)
        {
            out = second->normal;
        }
    }
}

bool moveVertexToPlaneAlongDirection(Surface& surface, const double planeD, Vertex& vertex, const TVector& direction) noexcept
{
    double ax = 0.0;
    double ay = 0.0;
    double az = 0.0;

    // Step 1: intersect the movement ray against the desired surface plane.
    if (!planeLineIntersection(
            surface.normal,
            planeD,
            vertex.x,
            vertex.y,
            vertex.z,
            vertex.x + direction.x,
            vertex.y + direction.y,
            vertex.z + direction.z,
            ax,
            ay,
            az))
    {
        return false;
    }

    // Step 2: preserve legacy behavior by refusing large jumps.
    const double dx = vertex.x - ax;
    const double dy = vertex.y - ay;
    const double dz = vertex.z - az;
    const double distSqr = dx * dx + dy * dy + dz * dz;
    if (distSqr >= kFlattenMaxMoveSqr)
    {
        return false;
    }

    vertex.x = ax;
    vertex.y = ay;
    vertex.z = az;
    return true;
}

} // namespace

bool levelFlattenSurface(Level& level, const SurfaceRef ref) noexcept
{
    if (!isValidSurfaceRef(level, ref))
    {
        return false;
    }

    Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
    Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];

    if (surface.vertices.empty())
    {
        return false;
    }
    if (surface.normal.isZero())
    {
        return false;
    }

    const std::int32_t v0Index = surface.vertices[0];
    if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= sector.vertices.size())
    {
        return false;
    }

    const Vertex& v0 = sector.vertices[static_cast<std::size_t>(v0Index)];
    const double d = surface.normal.dot(v0.x, v0.y, v0.z);

    bool result = true;

    // Step 1: move only vertices that drifted off the target plane.
    for (const std::int32_t vertexIndex : surface.vertices)
    {
        if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
        {
            return false;
        }

        Vertex& vx = sector.vertices[static_cast<std::size_t>(vertexIndex)];
        const double dist = std::abs(d - surface.normal.dot(vx.x, vx.y, vx.z));
        if (dist <= kFlattenPlaneEpsilon)
        {
            continue;
        }

        TVector vec{};
        calcFlattenDirectionForVertex(sector, surface, vertexIndex, vec);
        if (!moveVertexToPlaneAlongDirection(surface, d, vx, vec))
        {
            result = false;
        }
    }

    // Step 2: flattening moves geometry but does not edit the UV island.
    // Refresh only geometry-derived data for every affected surface.
    for (Surface& s : sector.surfaces)
    {
        s.normal = surfaceCalcNormalNewell(sector, s);
    }

    return result;
}

bool surfaceIsConvex(const Sector& sector, const Surface& surface) noexcept
{
    const std::size_t count = surface.vertices.size();
    if (count < 3)
    {
        return false;
    }

    for (std::size_t i = 0; i < count; ++i)
    {
        const std::int32_t i1 = surface.vertices[i];
        const std::int32_t i2 = surface.vertices[(i + 1) % count];
        const std::int32_t i3 = surface.vertices[(i + 2) % count];

        if (i1 < 0 || i2 < 0 || i3 < 0)
        {
            return false;
        }
        if (static_cast<std::size_t>(i1) >= sector.vertices.size()
            || static_cast<std::size_t>(i2) >= sector.vertices.size()
            || static_cast<std::size_t>(i3) >= sector.vertices.size())
        {
            return false;
        }

        const Vertex& v1 = sector.vertices[static_cast<std::size_t>(i1)];
        const Vertex& v2 = sector.vertices[static_cast<std::size_t>(i2)];
        const Vertex& v3 = sector.vertices[static_cast<std::size_t>(i3)];

        const TVector a = vertexPosition(v3) - vertexPosition(v2);
        const TVector b = vertexPosition(v1) - vertexPosition(v2);
        const TVector c = a.cross(b);

        const double turn = c.dot(surface.normal);
        if (!std::isfinite(turn) || turn < -precision::cornerTurnError(
                vertexPosition(v1), vertexPosition(v2), vertexPosition(v3), surface.normal))
        {
            return false;
        }
    }

    return true;
}

bool sectorIsConvex(const Sector& sector) noexcept
{
    for (const Surface& surface : sector.surfaces)
    {
        if (surface.vertices.empty())
        {
            continue;
        }

        const precision::SurfacePlaneError errorContext=precision::surfacePlaneError(sector,surface);
        if (!errorContext.valid || surface.normal.dot(errorContext.geometricNormal)<=0.0) return false;

        const std::int32_t v0Index = surface.vertices[0];
        if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= sector.vertices.size())
        {
            return false;
        }

        const Vertex& v0 = sector.vertices[static_cast<std::size_t>(v0Index)];

        for (const Vertex& v : sector.vertices)
        {
            const double d = surface.normal.dot(vertexPosition(v) - vertexPosition(v0));
            const double allowance=errorContext.at(surface.normal,vertexPosition(v),vertexPosition(v0));
            if (!std::isfinite(d) || allowance<0.0 || d < -allowance)
            {
                return false;
            }
        }
    }

    return true;
}

namespace
{

constexpr double kSurfaceCrossPlaneEpsilon = 0.0001;

const Vertex* surfaceVertexAt(const Sector& sector, const Surface& surface, std::size_t surfaceVertexIndex) noexcept;

bool finiteDirection(const TVector& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool normalizeFiniteDirection(TVector& value) noexcept
{
    if ( !finiteDirection(value) )
    {
        return false;
    }
    const double scale = std::max({std::abs(value.x), std::abs(value.y), std::abs(value.z)});
    if ( scale == 0.0 )
    {
        return false;
    }
    value = {value.x / scale, value.y / scale, value.z / scale};
    return value.normalize() > 0.0;
}

bool interpolateLineBySignedDistances(
    const double dist1,
    const double dist2,
    const double x1,
    const double y1,
    const double z1,
    const double x2,
    const double y2,
    const double z2,
    double& x,
    double& y,
    double& z) noexcept
{
    if ( !std::isfinite(dist1) || !std::isfinite(dist2) || dist1 == dist2
        || !finiteDirection({x1, y1, z1}) || !finiteDirection({x2, y2, z2}) )
    {
        return false;
    }

    // Interpolate from the endpoint nearest the plane. Forming a parameter
    // near one and subtracting almost an entire edge loses fine cut coordinates.
    // The choice depends on signed distances, so reversing an edge retains the
    // same small parameter. Scale if finite distances overflow subtraction.
    const bool fromEnd = std::abs(dist2) < std::abs(dist1);
    const double firstDistance = fromEnd ? dist2 : dist1;
    const double secondDistance = fromEnd ? dist1 : dist2;
    const double denominator = firstDistance - secondDistance;
    double k = 0.0;
    if ( std::isfinite(denominator) )
    {
        k = firstDistance / denominator;
    }
    else
    {
        const double scale = std::max(std::abs(dist1), std::abs(dist2));
        const double first = firstDistance / scale;
        const double second = secondDistance / scale;
        k = first / (first - second);
    }
    if ( !std::isfinite(k) )
    {
        return false;
    }
    const auto coordinate = [k, fromEnd](double first, double second)
    {
        if ( fromEnd )
        {
            std::swap(first, second);
        }
        const double delta = second - first;
        const double result = first + k * delta;
        return std::isfinite(delta) && std::isfinite(result) ? result : std::lerp(first, second, k);
    };
    const TVector result{coordinate(x1, x2), coordinate(y1, y2), coordinate(z1, z2)};
    if ( !finiteDirection(result) )
    {
        return false;
    }
    x = result.x;
    y = result.y;
    z = result.z;
    return true;
}

} // namespace

bool planeLineIntersection(
    const TVector& normal,
    const double px,
    const double py,
    const double pz,
    const double x1,
    const double y1,
    const double z1,
    const double x2,
    const double y2,
    const double z2,
    double& x,
    double& y,
    double& z) noexcept
{
    if ( !finiteDirection(normal) || !finiteDirection({px, py, pz}) )
    {
        return false;
    }
    // Step 1: calculate endpoint distances from the point-defined plane.
    const double dist1 = normal.dot(x1 - px, y1 - py, z1 - pz);
    const double dist2 = normal.dot(x2 - px, y2 - py, z2 - pz);

    // Step 2: reuse the common signed-distance interpolation.
    return interpolateLineBySignedDistances(dist1, dist2, x1, y1, z1, x2, y2, z2, x, y, z);
}

bool planeLineIntersection(
    const TVector& normal,
    const double d,
    const double x1,
    const double y1,
    const double z1,
    const double x2,
    const double y2,
    const double z2,
    double& x,
    double& y,
    double& z) noexcept
{
    if ( !finiteDirection(normal) || !std::isfinite(d) )
    {
        return false;
    }
    // Step 1: calculate endpoint distances from the normal-dot-position plane.
    const double dist1 = normal.dot(x1, y1, z1) - d;
    const double dist2 = normal.dot(x2, y2, z2) - d;

    // Step 2: reuse the common signed-distance interpolation.
    return interpolateLineBySignedDistances(dist1, dist2, x1, y1, z1, x2, y2, z2, x, y, z);
}

bool surfaceIsCrossed(
    const Sector& sector,
    const Surface& surface,
    const double x1,
    const double y1,
    const double z1,
    const double x2,
    const double y2,
    const double z2) noexcept
{
    const Vertex* v0 = surfaceVertexAt(sector, surface, 0);
    if (v0 == nullptr)
    {
        return false;
    }

    // Step 1: classify each endpoint against the surface plane.
    const double dist1 = surface.normal.dot(x1 - v0->x, y1 - v0->y, z1 - v0->z);
    const double dist2 = surface.normal.dot(x2 - v0->x, y2 - v0->y, z2 - v0->z);

    // Step 2: endpoints strictly on the same side cannot cross.
    if ((dist1 > kSurfaceCrossPlaneEpsilon) && (dist2 > kSurfaceCrossPlaneEpsilon))
    {
        return false;
    }
    if ((dist1 < -kSurfaceCrossPlaneEpsilon) && (dist2 < -kSurfaceCrossPlaneEpsilon))
    {
        return false;
    }

    // Step 3: preserve legacy behavior: any segment already in the plane is
    // considered crossed without checking polygon containment.
    if ((std::abs(dist1) <= kSurfaceCrossPlaneEpsilon) && (std::abs(dist2) <= kSurfaceCrossPlaneEpsilon))
    {
        return true;
    }

    // Step 4: find the plane hit and test whether it lies inside the surface.
    double px = 0.0;
    double py = 0.0;
    double pz = 0.0;
    if (!interpolateLineBySignedDistances(dist1, dist2, x1, y1, z1, x2, y2, z2, px, py, pz))
    {
        return false;
    }

    return surfaceIsPointOn(sector, surface, px, py, pz);
}

bool getNearestVertexToLine(
    const std::span<const Vertex> vertices,
    const double lx,
    const double ly,
    const double lz,
    const TVector& lineDirection,
    const double radius,
    double& vx,
    double& vy,
    double& vz,
    double& minDistanceSqr) noexcept
{
    TVector dir = lineDirection;
    if ( !finiteDirection({lx, ly, lz}) || !std::isfinite(radius) || radius < 0.0
        || !normalizeFiniteDirection(dir) )
    {
        return false;
    }

    bool found = false;
    minDistanceSqr = sqr(radius);

    for (const Vertex& v : vertices)
    {
        TVector lv{};
        VectorSub3(v.x, v.y, v.z, lx, ly, lz, lv.x, lv.y, lv.z);

        const TVector p = lv - lv.projectOnToNormal(dir);
        const double dist = p.lengthSqr();
        if (dist <= minDistanceSqr)
        {
            minDistanceSqr = dist;
            vx = v.x;
            vy = v.y;
            vz = v.z;
            found = true;
        }
    }

    return found;
}

bool getNearestVertexToLine(
    const std::span<const Vertex> vertices,
    const double lx,
    const double ly,
    const double lz,
    const TVector& lineDirection,
    const double radius,
    double& vx,
    double& vy,
    double& vz) noexcept
{
    double distSqr = 0.0;
    return getNearestVertexToLine(vertices, lx, ly, lz, lineDirection, radius, vx, vy, vz, distSqr);
}

bool levelGetNearestWorldVertexToLine(
    const Level& level,
    const double lx,
    const double ly,
    const double lz,
    const TVector& lineDirection,
    const double radius,
    TVector& vertex) noexcept
{
    double vx = lx;
    double vy = ly;
    double vz = lz;

    bool found = false;
    double minDistSqr = sqr(radius);

    for (const Sector& sector : level.sectors)
    {
        double tx{};
        double ty{};
        double tz{};
        double distSqr{};

        if (getNearestVertexToLine(sector.vertices, lx, ly, lz, lineDirection, radius, tx, ty, tz, distSqr))
        {
            if ( !found || distSqr < minDistSqr )
            {
                minDistSqr = distSqr;
                vx = tx;
                vy = ty;
                vz = tz;
                found = true;
            }
        }
    }

    vertex.setCoords(vx, vy, vz);
    return found;
}

namespace
{

std::optional<TVector> surfacePickNormal(const Sector& sector, const Surface& surface) noexcept
{
    // Step 1: prefer the cached surface normal, but lazily recover from geometry
    // when callers pass surfaces that have not been recalculated yet.
    TVector normal = surface.normal;
    if (normal.isZero())
    {
        normal = surfaceCalcNormalNewell(sector, surface);
    }

    if (normal.isZero())
    {
        return std::nullopt;
    }
    return normal;
}

bool surfaceContainsPickPoint(
    const Sector& sector,
    const Surface& surface,
    const TVector& normal,
    const double px,
    const double py,
    const double pz) noexcept
{
    bool hasNegative = false;
    bool hasPositive = false;

    // Step 1: classify the point against every oriented edge of the polygon.
    // The legacy picker accepted either winding direction, so only mixed signs
    // mean the point is outside.
    const TVector point{ px, py, pz };
    const std::size_t count = surface.vertices.size();
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::int32_t v1Index = surface.vertices[i];
        const std::int32_t v2Index = surface.vertices[(i + 1) % count];
        if (v1Index < 0 || v2Index < 0
            || static_cast<std::size_t>(v1Index) >= sector.vertices.size()
            || static_cast<std::size_t>(v2Index) >= sector.vertices.size())
        {
            return false;
        }

        const Vertex& v1 = sector.vertices[static_cast<std::size_t>(v1Index)];
        const Vertex& v2 = sector.vertices[static_cast<std::size_t>(v2Index)];

        const TVector edge = vertexPosition(v2) - vertexPosition(v1);
        const TVector edgePlane = normal.cross(edge);
        const double dist = edgePlane.dot(point - vertexPosition(v1));
        if (dist < -CloseEnough)
        {
            hasNegative = true;
        }
        else if (dist > CloseEnough)
        {
            hasPositive = true;
        }

        if (hasNegative && hasPositive)
        {
            return false;
        }
    }

    return true;
}

std::optional<SurfacePickHit> intersectSurfaceForPick(
    const Sector& sector,
    const Surface& surface,
    const std::size_t sectorIndex,
    const std::size_t surfaceIndex,
    const TVector& rayOrigin,
    const TVector& rayDirection,
    const double maxDistance) noexcept
{
    if (surface.vertices.size() < 3)
    {
        return std::nullopt;
    }

    // Step 1: resolve a usable plane for the surface.
    const std::optional<TVector> normal = surfacePickNormal(sector, surface);
    if (!normal)
    {
        return std::nullopt;
    }

    const std::int32_t v0Index = surface.vertices[0];
    if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= sector.vertices.size())
    {
        return std::nullopt;
    }

    const Vertex& v0 = sector.vertices[static_cast<std::size_t>(v0Index)];
    const double d = normal->dot(v0.x, v0.y, v0.z);

    // Step 2: intersect the ray against the surface plane.
    const double denom = normal->dot(rayDirection);
    if (sed::math::isZero(denom))
    {
        return std::nullopt;
    }

    const double t = (d - normal->dot(rayOrigin.x, rayOrigin.y, rayOrigin.z)) / denom;
    if (t < 0.0 || t > maxDistance)
    {
        return std::nullopt;
    }

    const double px = rayOrigin.x + rayDirection.x * t;
    const double py = rayOrigin.y + rayDirection.y * t;
    const double pz = rayOrigin.z + rayDirection.z * t;

    // Step 3: reject plane hits that land outside the polygon boundary.
    if (!surfaceContainsPickPoint(sector, surface, *normal, px, py, pz))
    {
        return std::nullopt;
    }

    SurfacePickHit hit{};
    hit.surface.sector = static_cast<std::int32_t>(sectorIndex);
    hit.surface.surface = static_cast<std::int32_t>(surfaceIndex);
    hit.position = TVector{ px, py, pz };
    hit.distance = t;
    return hit;
}

} // namespace

namespace
{

TBox surfacePickUnionBounds(const TBox& a, const TBox& b) noexcept
{
    TBox out{};
    out.x1 = std::min(a.x1, b.x1);
    out.y1 = std::min(a.y1, b.y1);
    out.z1 = std::min(a.z1, b.z1);
    out.x2 = std::max(a.x2, b.x2);
    out.y2 = std::max(a.y2, b.y2);
    out.z2 = std::max(a.z2, b.z2);
    return out;
}

double surfacePickAxisValue(const TVector& value, const int axis) noexcept
{
    if (axis == 0)
    {
        return value.x;
    }
    if (axis == 1)
    {
        return value.y;
    }
    return value.z;
}

std::optional<double> surfacePickConservativeBoundsPadding(
    const Sector& sector,
    const Surface& surface) noexcept
{
    const std::optional<TVector> normal = surfacePickNormal(sector, surface);
    if (!normal || surface.vertices.size() < 3)
    {
        return std::nullopt;
    }

    const double normalMagnitude = normal->length();
    if (!std::isfinite(normalMagnitude)
        || normalMagnitude <= std::numeric_limits<double>::epsilon())
    {
        return std::nullopt;
    }

    const std::int32_t v0Index = surface.vertices.front();
    if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= sector.vertices.size())
    {
        return std::nullopt;
    }
    const TVector planeOrigin =
        vertexPosition(sector.vertices[static_cast<std::size_t>(v0Index)]);

    // The exact picker intersects the plane through vertex zero, while edge
    // tests accept the orthogonal projections of every other vertex onto that
    // plane. Include the projection displacement in the raw-vertex bounds so
    // malformed/non-planar surfaces cannot be rejected by the broad phase.
    double maxProjectionDistance = 0.0;
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double minZ = std::numeric_limits<double>::max();
    double maxX = -std::numeric_limits<double>::max();
    double maxY = -std::numeric_limits<double>::max();
    double maxZ = -std::numeric_limits<double>::max();
    for (const std::int32_t vertexIndex : surface.vertices)
    {
        if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
        {
            return std::nullopt;
        }
        const TVector vertex =
            vertexPosition(sector.vertices[static_cast<std::size_t>(vertexIndex)]);
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z))
        {
            return std::nullopt;
        }
        minX = std::min(minX, vertex.x);
        minY = std::min(minY, vertex.y);
        minZ = std::min(minZ, vertex.z);
        maxX = std::max(maxX, vertex.x);
        maxY = std::max(maxY, vertex.y);
        maxZ = std::max(maxZ, vertex.z);
        const double projectionDistance =
            std::abs(normal->dot(vertex - planeOrigin)) / normalMagnitude;
        if (!std::isfinite(projectionDistance))
        {
            return std::nullopt;
        }
        maxProjectionDistance = std::max(maxProjectionDistance, projectionDistance);
    }

    std::vector<TVector> edgePlaneDirections{};
    std::vector<TVector> edgePlaneOrigins{};
    std::vector<double> edgeToleranceDistances{};
    edgePlaneDirections.reserve(surface.vertices.size());
    edgePlaneOrigins.reserve(surface.vertices.size());
    edgeToleranceDistances.reserve(surface.vertices.size());
    double maxOffsetDistance = 0.0;
    int polygonSide = 0;
    bool supportingPolygon = true;
    for (std::size_t i = 0; i < surface.vertices.size(); ++i)
    {
        const std::int32_t v1Index = surface.vertices[i];
        const std::int32_t v2Index = surface.vertices[(i + 1) % surface.vertices.size()];
        if (v1Index < 0 || v2Index < 0
            || static_cast<std::size_t>(v1Index) >= sector.vertices.size()
            || static_cast<std::size_t>(v2Index) >= sector.vertices.size())
        {
            return std::nullopt;
        }

        const TVector edge =
            vertexPosition(sector.vertices[static_cast<std::size_t>(v2Index)])
            - vertexPosition(sector.vertices[static_cast<std::size_t>(v1Index)]);
        const TVector edgePlaneCoefficient = normal->cross(edge);
        TVector edgePlane = edgePlaneCoefficient;
        const double magnitude = edgePlane.normalize();
        if (!std::isfinite(magnitude) || magnitude <= std::numeric_limits<double>::epsilon())
        {
            return std::nullopt;
        }

        // The supporting-convex path below requires every ordered edge to be
        // a supporting line of the projected polygon. Other valid faces may
        // still receive a finite bound from the dual half-plane certificates;
        // malformed or numerically uncertain faces remain exhaustive.
        int edgeSide = 0;
        const TVector edgeOrigin =
            vertexPosition(sector.vertices[static_cast<std::size_t>(v1Index)]);
        for (std::size_t j = 0; j < surface.vertices.size(); ++j)
        {
            if (j == i || j == (i + 1) % surface.vertices.size())
            {
                continue;
            }
            const std::int32_t testIndex = surface.vertices[j];
            const TVector testVertex =
                vertexPosition(sector.vertices[static_cast<std::size_t>(testIndex)]);
            const double sideDistance = edgePlane.dot(testVertex - edgeOrigin);
            if (!std::isfinite(sideDistance))
            {
                return std::nullopt;
            }
            const int side = sideDistance > 0.0 ? 1 : (sideDistance < 0.0 ? -1 : 0);
            if (side == 0)
            {
                continue;
            }
            if (edgeSide != 0 && edgeSide != side)
            {
                supportingPolygon = false;
            }
            edgeSide = side;
        }
        if (edgeSide == 0 || (polygonSide != 0 && polygonSide != edgeSide))
        {
            supportingPolygon = false;
        }
        if (edgeSide != 0 && polygonSide == 0)
        {
            polygonSide = edgeSide;
        }
        else if (edgeSide != 0 && polygonSide != edgeSide)
        {
            supportingPolygon = false;
        }

        const double edgeTolerance = std::nextafter(
            CloseEnough / magnitude,
            std::numeric_limits<double>::infinity());
        if (!std::isfinite(edgeTolerance))
        {
            return std::nullopt;
        }
        maxOffsetDistance = std::max(maxOffsetDistance, edgeTolerance);
        edgePlaneDirections.push_back(edgePlane);
        edgePlaneOrigins.push_back(edgeOrigin);
        edgeToleranceDistances.push_back(edgeTolerance);
    }

    // A supporting convex face can use the actual consecutive corners for a
    // quick bound.  Opposite edge planes are deliberately excluded: they can
    // be almost parallel without meeting at a polygon corner.
    constexpr double kSineFloor = 1e-12;
    const auto adjacentPadding = [&]() noexcept -> std::optional<double>
    {
        double minNonParallelSine = std::numeric_limits<double>::max();
        bool haveNonParallelPair = false;
        for (std::size_t i = 0; i < edgePlaneDirections.size(); ++i)
        {
            const std::size_t next = (i + 1) % edgePlaneDirections.size();
            const TVector cross = edgePlaneDirections[i].cross(edgePlaneDirections[next]);
            const double sine = cross.length();
            if (!std::isfinite(sine))
            {
                return std::nullopt;
            }
            if (sine <= kSineFloor)
            {
                // Exact parallel runs are collinear contour subdivisions. A
                // nonzero cross at this floor is a real sharp corner, not a
                // safe pair to omit from the displacement bound.
                if (cross.x != 0.0 || cross.y != 0.0 || cross.z != 0.0)
                {
                    return std::nullopt;
                }
                continue;
            }
            minNonParallelSine = std::min(minNonParallelSine, sine);
            haveNonParallelPair = true;
        }
        if (!haveNonParallelPair)
        {
            return std::nullopt;
        }

        const double padding = std::max(
            CloseEnough,
            maxProjectionDistance + (maxOffsetDistance * 2.0) / minNonParallelSine);
        if (!std::isfinite(padding) || padding > 1.0)
        {
            return std::nullopt;
        }
        return padding;
    };

    if (supportingPolygon)
    {
        if (const std::optional<double> padding = adjacentPadding())
        {
            return padding;
        }
    }

    // Faces with a mixed supporting-side pattern, or with a genuinely sharp
    // consecutive corner, can still receive a finite broad-phase bound when
    // the two all-sign half-plane branches admit conservative dual
    // certificates. For branch b, every edge satisfies
    // b * d_i(point) >= -edgeTolerance_i, where each edge tolerance is
    // CloseEnough divided by that edge plane's magnitude. The dual witnesses
    // below bound the coordinate objectives directly, so an irrelevant
    // near-parallel contour subdivision cannot inflate or invalidate the
    // bound.
    constexpr double kArithmeticSlack = 128.0 * std::numeric_limits<double>::epsilon();
    const auto vectorL1 = [](const TVector& value) noexcept
    {
        return std::abs(value.x) + std::abs(value.y) + std::abs(value.z);
    };
    struct DualCertificate
    {
        double baseBound = 0.0;
        double residualBound = 0.0;
        bool valid = false;
    };

    // A near-parallel pair can be an irrelevant contour subdivision while
    // another well-conditioned pair still proves every coordinate bound. For
    // each objective q, a nonnegative combination
    //
    //   q = lambda_i * (branch * edge_i)
    //       + lambda_j * (branch * edge_j) + mu * normal
    //
    // is a dual certificate for q dot point. Its right hand side is bounded
    // by the corresponding two edge inequalities and the surface plane. A
    // certificate is used only when the pair determinant and both nonnegative
    // coefficients are separated from their arithmetic error; otherwise the
    // existing exhaustive fallback remains responsible for the face.
    const double normalSquared = normal->dot(*normal);
    const std::array<TVector, 3> coordinateAxes{
        TVector{ 1.0, 0.0, 0.0 },
        TVector{ 0.0, 1.0, 0.0 },
        TVector{ 0.0, 0.0, 1.0 },
    };
    const std::array<double, 3> rawCoordinateMins{ minX, minY, minZ };
    const std::array<double, 3> rawCoordinateMaxs{ maxX, maxY, maxZ };
    const double planeConstant = normal->dot(planeOrigin);

    const auto dualWitness = [&](const int branch,
                                 const TVector& objective,
                                 DualCertificate& certificate) noexcept
    {
        if (!std::isfinite(normalSquared) || normalSquared <= 0.0
            || !std::isfinite(planeConstant))
        {
            return false;
        }

        const double normalCoefficientForObjective =
            objective.dot(*normal) / normalSquared;
        const TVector planarObjective =
            objective - *normal * normalCoefficientForObjective;
        if (!std::isfinite(normalCoefficientForObjective)
            || !std::isfinite(planarObjective.x)
            || !std::isfinite(planarObjective.y)
            || !std::isfinite(planarObjective.z))
        {
            return false;
        }

        DualCertificate bestCertificate{};
        const auto considerCertificate = [&](const double firstCoefficient,
                                             const double secondCoefficient,
                                             const TVector& firstDirection,
                                             const TVector& secondDirection,
                                             const double firstConstant,
                                             const double secondConstant,
                                             const double firstCoefficientError,
                                             const double secondCoefficientError,
                                             const double coefficientError,
                                             DualCertificate& bestCertificate) noexcept
        {
            if (!std::isfinite(firstCoefficient)
                || !std::isfinite(secondCoefficient)
                || !std::isfinite(firstCoefficientError)
                || !std::isfinite(secondCoefficientError)
                || !std::isfinite(coefficientError)
                || firstCoefficient < firstCoefficientError
                || secondCoefficient < secondCoefficientError)
            {
                return;
            }

            const TVector combination = firstDirection * firstCoefficient
                + secondDirection * secondCoefficient;
            const TVector normalRemainder = objective - combination;
            const double normalCoefficient = normalRemainder.dot(*normal)
                / normalSquared;
            const TVector planarRemainder =
                normalRemainder - *normal * normalCoefficient;
            const double residualMagnitude = vectorL1(planarRemainder);
            const double residualScale = std::max({
                1.0,
                vectorL1(objective),
                std::abs(firstCoefficient) * vectorL1(firstDirection),
                std::abs(secondCoefficient) * vectorL1(secondDirection),
                std::abs(normalCoefficient) * vectorL1(*normal),
            });
            const double residualError = kArithmeticSlack * residualScale
                + firstCoefficientError * vectorL1(firstDirection)
                + secondCoefficientError * vectorL1(secondDirection)
                + coefficientError * vectorL1(*normal);
            if (!std::isfinite(normalCoefficient)
                || !std::isfinite(residualMagnitude)
                || !std::isfinite(residualError)
                || residualMagnitude > residualError)
            {
                return;
            }

            const double firstTerm = firstCoefficient * firstConstant;
            const double secondTerm = secondCoefficient * secondConstant;
            const double normalTerm = normalCoefficient * planeConstant;
            const double bound = firstTerm + secondTerm + normalTerm;
            const double boundScale = std::max({
                1.0,
                std::abs(firstTerm),
                std::abs(secondTerm),
                std::abs(normalTerm),
                std::abs(bound),
            });
            const double boundError = kArithmeticSlack * boundScale
                + firstCoefficientError * std::abs(firstConstant)
                + secondCoefficientError * std::abs(secondConstant)
                + coefficientError * std::abs(planeConstant);
            const double safeBound = bound - boundError;
            const double safeResidual = residualMagnitude + residualError;
            if (std::isfinite(safeBound) && std::isfinite(safeResidual)
                && (!bestCertificate.valid
                    || safeResidual < bestCertificate.residualBound
                    || (safeResidual == bestCertificate.residualBound
                        && safeBound > bestCertificate.baseBound)))
            {
                bestCertificate.baseBound = safeBound;
                bestCertificate.residualBound = safeResidual;
                bestCertificate.valid = true;
            }
        };

        // A coordinate objective can lie exactly on one edge-plane normal.
        // Keep this one-edge certificate so axis-aligned faces do not depend
        // on a second edge whose computed coefficient is merely zero.
        for (std::size_t i = 0; i < edgePlaneDirections.size(); ++i)
        {
            const TVector direction = edgePlaneDirections[i]
                * static_cast<double>(branch);
            const double coefficient = planarObjective.dot(direction);
            const double coefficientScale = std::max({
                1.0,
                vectorL1(planarObjective),
                vectorL1(direction),
            });
            const double coefficientError = kArithmeticSlack * coefficientScale;
            const TVector planarRemainder = planarObjective - direction * coefficient;
            const double residualMagnitude = vectorL1(planarRemainder);
            const double residualError = kArithmeticSlack * coefficientScale
                + coefficientError * vectorL1(direction);
            if (!std::isfinite(coefficient)
                || !std::isfinite(coefficientError)
                || !std::isfinite(residualMagnitude)
                || !std::isfinite(residualError)
                || coefficient < coefficientError
                || residualMagnitude > residualError)
            {
                continue;
            }

            const TVector normalRemainder = objective - direction * coefficient;
            const double normalCoefficient = normalRemainder.dot(*normal)
                / normalSquared;
            const double firstTerm = coefficient * (
                static_cast<double>(branch) * edgePlaneDirections[i].dot(
                    edgePlaneOrigins[i])
                - edgeToleranceDistances[i]);
            const double normalTerm = normalCoefficient * planeConstant;
            const double bound = firstTerm + normalTerm;
            const double boundScale = std::max({
                1.0,
                std::abs(firstTerm),
                std::abs(normalTerm),
                std::abs(bound),
            });
            const double boundError = kArithmeticSlack * boundScale
                + coefficientError * std::abs(
                    static_cast<double>(branch) * edgePlaneDirections[i].dot(
                        edgePlaneOrigins[i]))
                + kArithmeticSlack * std::abs(normalCoefficient * planeConstant);
            const double safeBound = bound - boundError;
            const double safeResidual = residualMagnitude + residualError
                + kArithmeticSlack * std::max(
                    1.0,
                    std::abs(normalCoefficient) * vectorL1(*normal));
            if (std::isfinite(normalCoefficient) && std::isfinite(safeBound)
                && std::isfinite(safeResidual)
                && (!bestCertificate.valid
                    || safeResidual < bestCertificate.residualBound
                    || (safeResidual == bestCertificate.residualBound
                        && safeBound > bestCertificate.baseBound)))
            {
                bestCertificate.baseBound = safeBound;
                bestCertificate.residualBound = safeResidual;
                bestCertificate.valid = true;
            }
        }

        // If the objective is parallel to the surface normal, the plane alone
        // is a complete certificate. Exact zero components are intentional:
        // an approximately parallel objective must still find a tangent
        // witness or take the exhaustive path.
        if (planarObjective.x == 0.0 && planarObjective.y == 0.0
            && planarObjective.z == 0.0)
        {
            const double normalBound = normalCoefficientForObjective * planeConstant;
            const double safeBound = normalBound - kArithmeticSlack * std::max(
                1.0,
                std::abs(normalBound));
            if (std::isfinite(safeBound)
                && (!bestCertificate.valid
                    || bestCertificate.residualBound > kArithmeticSlack
                    || safeBound > bestCertificate.baseBound))
            {
                bestCertificate.baseBound = safeBound;
                bestCertificate.residualBound = kArithmeticSlack;
                bestCertificate.valid = true;
            }
        }

        for (std::size_t i = 0; i < edgePlaneDirections.size(); ++i)
        {
            for (std::size_t j = i + 1; j < edgePlaneDirections.size(); ++j)
            {
                const TVector firstDirection = edgePlaneDirections[i]
                    * static_cast<double>(branch);
                const TVector secondDirection = edgePlaneDirections[j]
                    * static_cast<double>(branch);
                const TVector pairCross = firstDirection.cross(secondDirection);
                const double determinant = pairCross.dot(*normal);
                const double determinantScale = vectorL1(firstDirection)
                    * vectorL1(secondDirection) * vectorL1(*normal);
                if (!std::isfinite(determinant)
                    || !std::isfinite(determinantScale))
                {
                    continue;
                }
                const double determinantError = kArithmeticSlack * determinantScale;
                const double determinantLowerBound =
                    std::abs(determinant) - determinantError;
                if (!std::isfinite(determinantError)
                    || !std::isfinite(determinantLowerBound)
                    || determinantLowerBound <= 0.0)
                {
                    // This pair is not a proof failure. It is simply an
                    // unusable witness; another pair may bound this axis.
                    continue;
                }

                const double firstNumerator = planarObjective.cross(secondDirection)
                    .dot(*normal);
                const double secondNumerator = firstDirection.cross(planarObjective)
                    .dot(*normal);
                const double firstNumeratorScale = vectorL1(planarObjective)
                    * vectorL1(secondDirection) * vectorL1(*normal);
                const double secondNumeratorScale = vectorL1(firstDirection)
                    * vectorL1(planarObjective) * vectorL1(*normal);
                if (!std::isfinite(firstNumerator)
                    || !std::isfinite(secondNumerator)
                    || !std::isfinite(firstNumeratorScale)
                    || !std::isfinite(secondNumeratorScale))
                {
                    continue;
                }

                const double firstCoefficient = firstNumerator / determinant;
                const double secondCoefficient = secondNumerator / determinant;
                const double firstCoefficientError =
                    (kArithmeticSlack * std::max(1.0, firstNumeratorScale)
                        + std::abs(firstCoefficient) * determinantError)
                    / determinantLowerBound
                    + kArithmeticSlack * std::max(1.0, std::abs(firstCoefficient));
                const double secondCoefficientError =
                    (kArithmeticSlack * std::max(1.0, secondNumeratorScale)
                        + std::abs(secondCoefficient) * determinantError)
                    / determinantLowerBound
                    + kArithmeticSlack * std::max(1.0, std::abs(secondCoefficient));
                const double firstConstant = static_cast<double>(branch)
                    * edgePlaneDirections[i].dot(edgePlaneOrigins[i])
                    - edgeToleranceDistances[i];
                const double secondConstant = static_cast<double>(branch)
                    * edgePlaneDirections[j].dot(edgePlaneOrigins[j])
                    - edgeToleranceDistances[j];
                if (!std::isfinite(firstCoefficientError)
                    || !std::isfinite(secondCoefficientError)
                    || !std::isfinite(firstConstant)
                    || !std::isfinite(secondConstant))
                {
                    continue;
                }

                const double normalCoefficient =
                    (objective
                        - firstDirection * firstCoefficient
                        - secondDirection * secondCoefficient)
                        .dot(*normal)
                    / normalSquared;
                const double normalCoefficientError = kArithmeticSlack
                    * std::max({
                        1.0,
                        std::abs(normalCoefficient),
                        std::abs(firstCoefficient),
                        std::abs(secondCoefficient),
                    });
                considerCertificate(
                    firstCoefficient,
                    secondCoefficient,
                    firstDirection,
                    secondDirection,
                    firstConstant,
                    secondConstant,
                    firstCoefficientError,
                    secondCoefficientError,
                    normalCoefficientError,
                    bestCertificate);
            }
        }

        if (!bestCertificate.valid
            || !std::isfinite(bestCertificate.baseBound)
            || !std::isfinite(bestCertificate.residualBound))
        {
            return false;
        }
        certificate = bestCertificate;
        return true;
    };

    std::array<double, 3> dualLowerBounds{
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
    };
    std::array<double, 3> dualUpperBounds{
        -std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max(),
    };
    bool dualBoundsCertified = true;
    for (const int branch : { -1, 1 })
    {
        std::array<DualCertificate, 6> branchCertificates{};
        for (std::size_t axis = 0; axis < coordinateAxes.size(); ++axis)
        {
            for (const double objectiveSign : { -1.0, 1.0 })
            {
                const TVector objective = coordinateAxes[axis] * objectiveSign;
                DualCertificate& certificate = branchCertificates[
                    axis * 2u + (objectiveSign > 0.0 ? 1u : 0u)];
                if (!dualWitness(branch, objective, certificate))
                {
                    dualBoundsCertified = false;
                    break;
                }
            }
            if (!dualBoundsCertified)
            {
                break;
            }
        }
        if (!dualBoundsCertified)
        {
            break;
        }

        // Every signed coordinate certificate has the form
        // q dot p >= base - residual * ||p||inf. Bootstrap a finite common
        // infinity-norm bound before applying the residual to each axis. This
        // avoids assuming that the raw vertex box already contains p.
        double bootstrapA = 0.0;
        double bootstrapE = 0.0;
        for (const DualCertificate& certificate : branchCertificates)
        {
            if (!certificate.valid
                || !std::isfinite(certificate.baseBound)
                || !std::isfinite(certificate.residualBound))
            {
                dualBoundsCertified = false;
                break;
            }
            bootstrapA = std::max(bootstrapA, -certificate.baseBound);
            bootstrapE = std::max(bootstrapE, certificate.residualBound);
        }
        if (!dualBoundsCertified || !std::isfinite(bootstrapA)
            || !std::isfinite(bootstrapE) || bootstrapE >= 1.0)
        {
            dualBoundsCertified = false;
            break;
        }
        const double bootstrapM = std::nextafter(
            bootstrapA / (1.0 - bootstrapE),
            std::numeric_limits<double>::infinity());
        if (!std::isfinite(bootstrapM))
        {
            dualBoundsCertified = false;
            break;
        }

        std::array<double, 3> branchLowerBounds{};
        std::array<double, 3> branchUpperBounds{};
        for (std::size_t axis = 0; axis < coordinateAxes.size(); ++axis)
        {
            const DualCertificate& negativeCertificate =
                branchCertificates[axis * 2u];
            const DualCertificate& positiveCertificate =
                branchCertificates[axis * 2u + 1u];
            const double positiveLower = positiveCertificate.baseBound
                - positiveCertificate.residualBound * bootstrapM;
            const double negativeLower = negativeCertificate.baseBound
                - negativeCertificate.residualBound * bootstrapM;
            branchLowerBounds[axis] = positiveLower;
            branchUpperBounds[axis] = -negativeLower;
            if (!std::isfinite(branchLowerBounds[axis])
                || !std::isfinite(branchUpperBounds[axis]))
            {
                dualBoundsCertified = false;
                break;
            }
            // The all-sign branches form a union. Preserve the broadest
            // bound when accumulating the current branch with the prior one.
            dualLowerBounds[axis] = std::min(
                dualLowerBounds[axis], branchLowerBounds[axis]);
            dualUpperBounds[axis] = std::max(
                dualUpperBounds[axis], branchUpperBounds[axis]);
        }
        if (!dualBoundsCertified)
        {
            break;
        }
    }

    if (dualBoundsCertified)
    {
        double dualPadding = maxProjectionDistance;
        for (std::size_t axis = 0; axis < coordinateAxes.size(); ++axis)
        {
            dualPadding = std::max({
                dualPadding,
                rawCoordinateMins[axis] - dualLowerBounds[axis],
                dualUpperBounds[axis] - rawCoordinateMaxs[axis],
            });
        }
        if (std::isfinite(dualPadding) && dualPadding <= 1.0)
        {
            return std::max(CloseEnough, dualPadding);
        }
    }

    // The dual certificates above are the only finite shortcut for faces that
    // do not satisfy the supporting-convex fast path. If any certificate is
    // unavailable or numerically uncertain, leave the face in the exact
    // picker always-test list.
    return std::nullopt;
}

bool surfacePickRayIntersectsBounds(
    const TBox& bounds,
    const TVector& origin,
    const TVector& direction,
    const double maxDistance) noexcept
{
    double tMin = 0.0;
    double tMax = maxDistance;
    const std::array<double, 3> origins{ origin.x, origin.y, origin.z };
    const std::array<double, 3> directions{ direction.x, direction.y, direction.z };
    const std::array<double, 3> mins{
        bounds.x1 - CloseEnough,
        bounds.y1 - CloseEnough,
        bounds.z1 - CloseEnough,
    };
    const std::array<double, 3> maxs{
        bounds.x2 + CloseEnough,
        bounds.y2 + CloseEnough,
        bounds.z2 + CloseEnough,
    };

    for (int axis = 0; axis < 3; ++axis)
    {
        const double ray = directions[static_cast<std::size_t>(axis)];
        const double start = origins[static_cast<std::size_t>(axis)];
        if (sed::math::isZero(ray))
        {
            if (start < mins[static_cast<std::size_t>(axis)]
                || start > maxs[static_cast<std::size_t>(axis)])
            {
                return false;
            }
            continue;
        }

        const double inv = 1.0 / ray;
        double nearT = (mins[static_cast<std::size_t>(axis)] - start) * inv;
        double farT = (maxs[static_cast<std::size_t>(axis)] - start) * inv;
        if (nearT > farT)
        {
            std::swap(nearT, farT);
        }
        tMin = std::max(tMin, nearT);
        tMax = std::min(tMax, farT);
        if (tMin > tMax)
        {
            return false;
        }
    }
    return tMax >= 0.0 && tMin <= maxDistance;
}

} // namespace

void SurfacePickIndex::rebuild(const Level& level)
{
    rebuild(level, {});
}

void SurfacePickIndex::rebuild(
    const Level& level,
    const std::function<bool(std::int32_t)>& includeSector)
{
    entries_.clear();
    alwaysTestEntries_.clear();
    order_.clear();
    nodes_.clear();
    lastCandidateCount_ = 0;

    std::size_t surfaceCount = 0;
    for (const Sector& sector : level.sectors)
    {
        surfaceCount += sector.surfaces.size();
    }
    entries_.reserve(surfaceCount);

    for (std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex)
    {
        if (includeSector
            && !includeSector(static_cast<std::int32_t>(sectorIndex)))
        {
            continue;
        }
        const Sector& sector = level.sectors[sectorIndex];
        for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
        {
            const Surface& surface = sector.surfaces[surfaceIndex];
            double minX = std::numeric_limits<double>::max();
            double minY = std::numeric_limits<double>::max();
            double minZ = std::numeric_limits<double>::max();
            double maxX = -std::numeric_limits<double>::max();
            double maxY = -std::numeric_limits<double>::max();
            double maxZ = -std::numeric_limits<double>::max();
            bool haveVertex = false;
            for (const std::int32_t vertexIndex : surface.vertices)
            {
                if (vertexIndex < 0
                    || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
                {
                    continue;
                }
                const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
                minX = std::min(minX, vertex.x);
                minY = std::min(minY, vertex.y);
                minZ = std::min(minZ, vertex.z);
                maxX = std::max(maxX, vertex.x);
                maxY = std::max(maxY, vertex.y);
                maxZ = std::max(maxZ, vertex.z);
                haveVertex = true;
            }
            if (!haveVertex)
            {
                continue;
            }

            Entry entry{};
            entry.surface.sector = static_cast<std::int32_t>(sectorIndex);
            entry.surface.surface = static_cast<std::int32_t>(surfaceIndex);
            const std::optional<double> padding =
                surfacePickConservativeBoundsPadding(sector, surface);
            entry.alwaysTest = !padding.has_value();
            const double pad = padding.value_or(0.0);
            entry.bounds.Set(
                minX - pad,
                minY - pad,
                minZ - pad,
                maxX + pad,
                maxY + pad,
                maxZ + pad);
            entry.center = TVector{
                (minX + maxX) * 0.5,
                (minY + maxY) * 0.5,
                (minZ + maxZ) * 0.5,
            };
            entries_.push_back(std::move(entry));
        }
    }

    order_.reserve(entries_.size());
    alwaysTestEntries_.reserve(entries_.size());
    for (std::size_t i = 0; i < entries_.size(); ++i)
    {
        if (entries_[i].alwaysTest)
        {
            alwaysTestEntries_.push_back(static_cast<std::uint32_t>(i));
        }
        else
        {
            order_.push_back(static_cast<std::uint32_t>(i));
        }
    }
    if (!order_.empty())
    {
        nodes_.reserve(order_.size() * 2u);
        (void)buildNode(0u, static_cast<std::uint32_t>(order_.size()));
    }
}

std::uint32_t SurfacePickIndex::buildNode(
    const std::uint32_t first,
    const std::uint32_t count)
{
    const std::uint32_t nodeIndex = static_cast<std::uint32_t>(nodes_.size());
    nodes_.push_back(Node{});

    TBox bounds = entries_[order_[first]].bounds;
    for (std::uint32_t i = 1; i < count; ++i)
    {
        bounds = surfacePickUnionBounds(bounds, entries_[order_[first + i]].bounds);
    }

    constexpr std::uint32_t kLeafSize = 8u;
    if (count <= kLeafSize)
    {
        Node& node = nodes_[nodeIndex];
        node.bounds = bounds;
        node.first = first;
        node.count = count;
        return nodeIndex;
    }

    const std::array<double, 3> extents{
        bounds.x2 - bounds.x1,
        bounds.y2 - bounds.y1,
        bounds.z2 - bounds.z1,
    };
    int splitAxis = 0;
    if (extents[1] > extents[splitAxis])
    {
        splitAxis = 1;
    }
    if (extents[2] > extents[splitAxis])
    {
        splitAxis = 2;
    }

    const std::uint32_t leftCount = count / 2u;
    auto begin = order_.begin() + static_cast<std::ptrdiff_t>(first);
    auto middle = begin + static_cast<std::ptrdiff_t>(leftCount);
    auto end = begin + static_cast<std::ptrdiff_t>(count);
    std::nth_element(begin, middle, end,
        [this, splitAxis](const std::uint32_t lhs, const std::uint32_t rhs)
        {
            return surfacePickAxisValue(entries_[lhs].center, splitAxis)
                < surfacePickAxisValue(entries_[rhs].center, splitAxis);
        });

    const std::uint32_t left = buildNode(first, leftCount);
    const std::uint32_t right = buildNode(first + leftCount, count - leftCount);
    Node& node = nodes_[nodeIndex];
    node.bounds = bounds;
    node.left = left;
    node.right = right;
    return nodeIndex;
}

std::vector<SurfacePickHit> SurfacePickIndex::pickSurfaces(
    const Level& level,
    const TVector& rayOrigin,
    const TVector& rayDirection,
    const double maxDistance) const noexcept
{
    TVector direction = rayDirection;
    if (direction.normalize() == 0.0 || maxDistance < 0.0)
    {
        lastCandidateCount_ = 0;
        return {};
    }

    std::vector<std::uint32_t> stack{};
    std::vector<std::uint32_t> candidates = alwaysTestEntries_;
    stack.reserve(64);
    candidates.reserve(alwaysTestEntries_.size() + 32u);
    if (!nodes_.empty())
    {
        stack.push_back(0u);
    }
    constexpr std::uint32_t kInvalidNode = std::numeric_limits<std::uint32_t>::max();
    while (!stack.empty())
    {
        const std::uint32_t nodeIndex = stack.back();
        stack.pop_back();
        if (nodeIndex >= nodes_.size())
        {
            continue;
        }
        const Node& node = nodes_[nodeIndex];
        if (!surfacePickRayIntersectsBounds(
                node.bounds, rayOrigin, direction, maxDistance))
        {
            continue;
        }

        if (node.count > 0u)
        {
            for (std::uint32_t i = 0; i < node.count; ++i)
            {
                const std::uint32_t entryIndex = order_[node.first + i];
                if (surfacePickRayIntersectsBounds(
                        entries_[entryIndex].bounds,
                        rayOrigin,
                        direction,
                        maxDistance))
                {
                    candidates.push_back(entryIndex);
                }
            }
            continue;
        }

        if (node.right != kInvalidNode)
        {
            stack.push_back(node.right);
        }
        if (node.left != kInvalidNode)
        {
            stack.push_back(node.left);
        }
    }

    std::sort(candidates.begin(), candidates.end(),
        [this](const std::uint32_t lhs, const std::uint32_t rhs)
        {
            const SurfaceRef& a = entries_[lhs].surface;
            const SurfaceRef& b = entries_[rhs].surface;
            if (a.sector != b.sector)
            {
                return a.sector < b.sector;
            }
            return a.surface < b.surface;
        });
    lastCandidateCount_ = candidates.size();

    std::vector<SurfacePickHit> hits{};
    hits.reserve(candidates.size());
    for (const std::uint32_t entryIndex : candidates)
    {
        const SurfaceRef& ref = entries_[entryIndex].surface;
        if (ref.sector < 0 || static_cast<std::size_t>(ref.sector) >= level.sectors.size())
        {
            continue;
        }
        const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
        if (ref.surface < 0 || static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
        {
            continue;
        }
        if (std::optional<SurfacePickHit> hit = intersectSurfaceForPick(
                sector,
                sector.surfaces[static_cast<std::size_t>(ref.surface)],
                static_cast<std::size_t>(ref.sector),
                static_cast<std::size_t>(ref.surface),
                rayOrigin,
                direction,
                maxDistance))
        {
            hits.push_back(*hit);
        }
    }

    std::sort(hits.begin(), hits.end(), [](const SurfacePickHit& a, const SurfacePickHit& b)
        {
            return a.distance < b.distance;
        });
    return hits;
}

void SectorContainmentIndex::rebuild(const Level& level)
{
    entries_.clear();
    order_.clear();
    nodes_.clear();
    sourceLevel_ = &level;
    sourceSectorCount_ = level.sectors.size();
    lastCandidateCount_ = 0;

    entries_.reserve(level.sectors.size());
    for (std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex)
    {
        const Sector& sector = level.sectors[sectorIndex];
        if (sector.vertices.empty())
        {
            continue;
        }
        Entry entry{};
        entry.sector = static_cast<std::int32_t>(sectorIndex);
        entry.bounds = sectorCalcBBox(sector);
        entry.center = TVector{
            (entry.bounds.x1 + entry.bounds.x2) * 0.5,
            (entry.bounds.y1 + entry.bounds.y2) * 0.5,
            (entry.bounds.z1 + entry.bounds.z2) * 0.5,
        };
        entries_.push_back(entry);
    }

    order_.resize(entries_.size());
    std::iota(order_.begin(), order_.end(), std::uint32_t{0});
    if (!order_.empty())
    {
        nodes_.reserve(order_.size() * 2u);
        (void)buildNode(0u, static_cast<std::uint32_t>(order_.size()));
    }
}

std::uint32_t SectorContainmentIndex::buildNode(
    const std::uint32_t first,
    const std::uint32_t count)
{
    const std::uint32_t nodeIndex = static_cast<std::uint32_t>(nodes_.size());
    nodes_.push_back(Node{});

    TBox bounds = entries_[order_[first]].bounds;
    for (std::uint32_t i = 1; i < count; ++i)
    {
        bounds = surfacePickUnionBounds(
            bounds,
            entries_[order_[first + i]].bounds);
    }

    constexpr std::uint32_t kLeafSize = 8u;
    if (count <= kLeafSize)
    {
        Node& node = nodes_[nodeIndex];
        node.bounds = bounds;
        node.first = first;
        node.count = count;
        return nodeIndex;
    }

    const std::array<double, 3> extents{
        bounds.x2 - bounds.x1,
        bounds.y2 - bounds.y1,
        bounds.z2 - bounds.z1,
    };
    int splitAxis = 0;
    if (extents[1] > extents[splitAxis])
    {
        splitAxis = 1;
    }
    if (extents[2] > extents[splitAxis])
    {
        splitAxis = 2;
    }

    const std::uint32_t leftCount = count / 2u;
    auto begin = order_.begin() + static_cast<std::ptrdiff_t>(first);
    auto middle = begin + static_cast<std::ptrdiff_t>(leftCount);
    auto end = begin + static_cast<std::ptrdiff_t>(count);
    std::nth_element(begin, middle, end,
        [this, splitAxis](const std::uint32_t lhs, const std::uint32_t rhs)
        {
            return surfacePickAxisValue(entries_[lhs].center, splitAxis)
                < surfacePickAxisValue(entries_[rhs].center, splitAxis);
        });

    const std::uint32_t left = buildNode(first, leftCount);
    const std::uint32_t right = buildNode(
        first + leftCount,
        count - leftCount);
    Node& node = nodes_[nodeIndex];
    node.bounds = bounds;
    node.left = left;
    node.right = right;
    return nodeIndex;
}

std::int32_t SectorContainmentIndex::findContainingSector(
    const Level& level,
    const double x,
    const double y,
    const double z,
    const std::int32_t preferredSector) const noexcept
{
    lastCandidateCount_ = 0;
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
    {
        return -1;
    }
    if (preferredSector >= 0
        && static_cast<std::size_t>(preferredSector) < level.sectors.size()
        && sectorContainsPoint(
            level.sectors[static_cast<std::size_t>(preferredSector)],
            x,
            y,
            z))
    {
        lastCandidateCount_ = 1;
        return preferredSector;
    }

    const auto boundsContainPoint = [=](const TBox& bounds) noexcept
    {
        return x >= bounds.x1 - kSectorContainmentBoundsPadding
            && x <= bounds.x2 + kSectorContainmentBoundsPadding
            && y >= bounds.y1 - kSectorContainmentBoundsPadding
            && y <= bounds.y2 + kSectorContainmentBoundsPadding
            && z >= bounds.z1 - kSectorContainmentBoundsPadding
            && z <= bounds.z2 + kSectorContainmentBoundsPadding;
    };

    constexpr std::uint32_t kInvalidNode =
        std::numeric_limits<std::uint32_t>::max();
    std::int32_t best = -1;
    const auto visit = [&](const auto& self, const std::uint32_t nodeIndex) noexcept -> void
    {
        if (nodeIndex >= nodes_.size())
        {
            return;
        }
        const Node& node = nodes_[nodeIndex];
        if (!boundsContainPoint(node.bounds))
        {
            return;
        }
        if (node.count > 0u)
        {
            for (std::uint32_t i = 0; i < node.count; ++i)
            {
                const Entry& entry = entries_[order_[node.first + i]];
                if (entry.sector == preferredSector
                    || !boundsContainPoint(entry.bounds))
                {
                    continue;
                }
                ++lastCandidateCount_;
                if ((best < 0 || entry.sector < best)
                    && entry.sector >= 0
                    && static_cast<std::size_t>(entry.sector) < level.sectors.size()
                    && sectorContainsPoint(
                        level.sectors[static_cast<std::size_t>(entry.sector)],
                        x,
                        y,
                        z))
                {
                    best = entry.sector;
                }
            }
            return;
        }
        if (node.left != kInvalidNode)
        {
            self(self, node.left);
        }
        if (node.right != kInvalidNode)
        {
            self(self, node.right);
        }
    };
    if (!nodes_.empty())
    {
        visit(visit, 0u);
    }
    return best;
}

std::vector<SurfacePickHit> levelPickSurfaces(
    const Level& level,
    const TVector& rayOrigin,
    const TVector& rayDirection,
    const double maxDistance) noexcept
{
    TVector dir = rayDirection;
    if (dir.normalize() == 0.0)
    {
        return {};
    }

    std::vector<SurfacePickHit> hits{};

    // Step 1: collect every valid sector/surface intersection using the shared
    // picker helper so list and best-hit queries stay behaviorally identical.
    for (std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex)
    {
        const Sector& sector = level.sectors[sectorIndex];
        for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
        {
            if (std::optional<SurfacePickHit> hit =
                    intersectSurfaceForPick(sector, sector.surfaces[surfaceIndex], sectorIndex, surfaceIndex, rayOrigin, dir, maxDistance))
            {
                hits.push_back(*hit);
            }
        }
    }

    // Step 2: callers expect front-to-back ordering for all-hit picking.
    std::sort(hits.begin(), hits.end(), [](const SurfacePickHit& a, const SurfacePickHit& b)
        {
            return a.distance < b.distance;
        });
    return hits;
}

std::optional<SurfacePickHit> levelPickSurface(
    const Level& level,
    const TVector& rayOrigin,
    const TVector& rayDirection,
    const double maxDistance) noexcept
{
    TVector dir = rayDirection;
    if (dir.normalize() == 0.0)
    {
        return std::nullopt;
    }

    std::optional<SurfacePickHit> best{};

    // Step 1: scan every pickable surface and retain only the nearest hit.
    for (std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex)
    {
        const Sector& sector = level.sectors[sectorIndex];
        for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
        {
            const std::optional<SurfacePickHit> hit =
                intersectSurfaceForPick(sector, sector.surfaces[surfaceIndex], sectorIndex, surfaceIndex, rayOrigin, dir, maxDistance);
            if (hit && (!best || hit->distance < best->distance))
            {
                best = *hit;
            }
        }
    }

    return best;
}

double surfaceDistToPlaneAbs(const Sector& sector, const Surface& surface, const double x, const double y, const double z) noexcept
{
    if (surface.vertices.empty())
    {
        return 0.0;
    }

    const std::int32_t v0Index = surface.vertices[0];
    if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= sector.vertices.size())
    {
        return 0.0;
    }

    const Vertex& v0 = sector.vertices[static_cast<std::size_t>(v0Index)];
    const double dist = surface.normal.dot(x - v0.x, y - v0.y, z - v0.z);
    return std::abs(dist);
}

std::int32_t levelFindSectorForXYZ(const Level& level, const double x, const double y, const double z) noexcept
{
    for (std::size_t i = 0; i < level.sectors.size(); ++i)
    {
        if (sectorContainsPoint(level.sectors[i], x, y, z))
        {
            return static_cast<std::int32_t>(i);
        }
    }

    return -1;
}

std::int32_t levelFindSectorForThing(const Level& level, const Thing& thing) noexcept
{
    return levelFindSectorForXYZ(level, thing.x, thing.y, thing.z);
}

TBox sectorCalcBBox(const Sector& sector) noexcept
{
    TBox box{};
    if (sector.vertices.empty())
    {
        return box;
    }

    box.x1 = box.x2 = sector.vertices[0].x;
    box.y1 = box.y2 = sector.vertices[0].y;
    box.z1 = box.z2 = sector.vertices[0].z;

    for (const Vertex& v : sector.vertices)
    {
        box.x1 = std::min(box.x1, v.x);
        box.x2 = std::max(box.x2, v.x);
        box.y1 = std::min(box.y1, v.y);
        box.y2 = std::max(box.y2, v.y);
        box.z1 = std::min(box.z1, v.z);
        box.z2 = std::max(box.z2, v.z);
    }

    return box;
}

TVector sectorCalcCenter(const Sector& sector) noexcept
{
    if (sector.vertices.empty())
    {
        return TVector::zero;
    }

    double xSum = 0.0;
    double ySum = 0.0;
    double zSum = 0.0;

    for (const Vertex& v : sector.vertices)
    {
        xSum += v.x;
        ySum += v.y;
        zSum += v.z;
    }

    const double count = static_cast<double>(sector.vertices.size());
    return TVector::Create(xSum / count, ySum / count, zSum / count);
}

BoundingSphere sectorCalcBoundingSphere(const Sector& sector) noexcept
{
    BoundingSphere out{};
    out.center = sectorCalcCenter(sector);
    out.radius = 0.0;

    for (const Vertex& v : sector.vertices)
    {
        const double dx = v.x - out.center.x;
        const double dy = v.y - out.center.y;
        const double dz = v.z - out.center.z;
        const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        out.radius = std::max(out.radius, dist);
    }

    return out;
}

std::optional<PointLight> sectorMakeDefaultIjimPointLight(const Sector& sector) noexcept
{
    if (sector.vertices.empty())
    {
        return std::nullopt;
    }

    const TVector center = sectorCalcCenter(sector);
    if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z))
    {
        return std::nullopt;
    }

    const TBox bounds = sectorCalcBBox(sector);
    const double outerRange = std::hypot(
        bounds.x2 - bounds.x1,
        bounds.y2 - bounds.y1,
        bounds.z2 - bounds.z1);
    if (!std::isfinite(outerRange) || outerRange <= 0.0)
    {
        return std::nullopt;
    }

    PointLight light{};
    light.color = ColorWhite;
    light.position = center;
    light.minRange = outerRange;
    light.maxRange = outerRange * 0.25;
    return light;
}

TBox surfaceCalcBBox(const Sector& sector, const Surface& surface) noexcept
{
    TBox box{};

    bool any = false;
    for (const std::int32_t index : surface.vertices)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size())
        {
            continue;
        }

        const Vertex& v = sector.vertices[static_cast<std::size_t>(index)];
        if (!any)
        {
            box.x1 = box.x2 = v.x;
            box.y1 = box.y2 = v.y;
            box.z1 = box.z2 = v.z;
            any = true;
            continue;
        }

        box.x1 = std::min(box.x1, v.x);
        box.x2 = std::max(box.x2, v.x);
        box.y1 = std::min(box.y1, v.y);
        box.y2 = std::max(box.y2, v.y);
        box.z1 = std::min(box.z1, v.z);
        box.z2 = std::max(box.z2, v.z);
    }

    return box;
}

namespace
{

const Vertex* surfaceVertexAt(const Sector& sector, const Surface& surface, const std::size_t surfaceVertexIndex) noexcept
{
    if (surfaceVertexIndex >= surface.vertices.size())
    {
        return nullptr;
    }

    const std::int32_t vertexIndex = surface.vertices[surfaceVertexIndex];
    if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
    {
        return nullptr;
    }

    return &sector.vertices[static_cast<std::size_t>(vertexIndex)];
}

} // namespace

TVector surfaceCalcNormalNewell(const Sector& sector, const Surface& surface) noexcept
{
    const precision::SurfaceArea area = precision::surfaceArea(sector, surface, false);
    if (!area.valid)
    {
        return TVector::zero;
    }
    const long double scale = std::max({std::abs(area.value[0]), std::abs(area.value[1]), std::abs(area.value[2])});
    if (!(scale > 0.0L) || !std::isfinite(scale))
    {
        return TVector::zero;
    }
    TVector normal{static_cast<double>(area.value[0] / scale), static_cast<double>(area.value[1] / scale), static_cast<double>(area.value[2] / scale)};
    normal.normalize();
    return normal;
}

namespace
{

bool calculateArrangedUv(
    const float reference,
    const double projectedOffset,
    const double pixelsPerUnit,
    const float scale,
    float& result) noexcept
{
    const double value = static_cast<double>(reference)
        + projectedOffset * pixelsPerUnit * static_cast<double>(scale);
    constexpr double maxFloat =
        static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(value) || value < -maxFloat || value > maxFloat)
    {
        return false;
    }

    result = static_cast<float>(value);
    return std::isfinite(result);
}

constexpr double kUvTriMinDistanceSqr = 1.0e-4;
constexpr double kUvTriMinAxisDistance = 1.0e-4;

double vertexDistanceSqr(const Vertex& a, const Vertex& b) noexcept
{
    return (vertexPosition(b) - vertexPosition(a)).lengthSqr();
}

bool chooseSecondUvTriVertex(
    const Sector& sector,
    const Surface& surface,
    const std::int32_t nv1,
    const Vertex& v0,
    const std::int32_t vertexCount,
    std::int32_t& nv2) noexcept
{
    std::int32_t n = nextIndex(nv1, vertexCount);
    std::int32_t remainingCandidates = vertexCount - 1;

    double farthestDist = -1.0;
    std::int32_t farthestIndex = -1;

    // Step 1: prefer the first vertex far enough from nv1 to form a stable
    // edge, while remembering the farthest valid fallback to preserve legacy
    // behavior on nearly duplicated polygons.
    while (remainingCandidates > 0)
    {
        if (const Vertex* v1 = surfaceVertexAt(sector, surface, static_cast<std::size_t>(n)))
        {
            const double dist = vertexDistanceSqr(v0, *v1);
            if (dist > farthestDist)
            {
                farthestDist = dist;
                farthestIndex = n;
            }

            if (dist > kUvTriMinDistanceSqr)
            {
                nv2 = n;
                return true;
            }
        }

        n = nextIndex(n, vertexCount);
        --remainingCandidates;
    }

    if (farthestIndex == -1)
    {
        return false;
    }

    nv2 = farthestIndex;
    return true;
}

bool buildUvTriSearchAxis(const Vertex& v0, const Vertex& v1, const TVector& normal, TVector& axis) noexcept
{
    // Step 1: build a vector perpendicular to the anchor edge and surface
    // normal. Third-vertex candidates are measured against this axis so the
    // selected triangle spans area in texture space.
    const TVector edge = vertexPosition(v1) - vertexPosition(v0);
    axis = edge.cross(normal);
    return axis.normalize() != 0.0;
}

bool chooseThirdUvTriVertex(
    const Sector& sector,
    const Surface& surface,
    const std::int32_t nv1,
    const std::int32_t nv2,
    const Vertex& v1,
    const TVector& axis,
    const std::int32_t vertexCount,
    std::int32_t& nv3) noexcept
{
    std::int32_t n = nextIndex(nv2, vertexCount);

    double farthestDist = -1.0;
    std::int32_t farthestIndex = -1;

    // Step 1: choose a vertex that is meaningfully off the nv1->nv2 line. When
    // none exceeds the threshold, use the farthest valid point as a fallback.
    for (std::int32_t i = 0; i < vertexCount; ++i)
    {
        if (n == nv1 || n == nv2)
        {
            n = nextIndex(n, vertexCount);
            continue;
        }

        if (const Vertex* v2 = surfaceVertexAt(sector, surface, static_cast<std::size_t>(n)))
        {
            const double dist = std::abs(axis.dot(vertexPosition(*v2) - vertexPosition(v1)));

            if (dist > farthestDist)
            {
                farthestDist = dist;
                farthestIndex = n;
            }

            if (dist > kUvTriMinAxisDistance)
            {
                nv3 = n;
                return true;
            }
        }

        n = nextIndex(n, vertexCount);
    }

    if (farthestIndex == -1 || !(farthestDist > 0.0) || !std::isfinite(farthestDist))
    {
        return false;
    }

    nv3 = farthestIndex;
    return true;
}

bool findTri(const Sector& sector, const Surface& surface, const std::int32_t nv1, std::int32_t& nv2, std::int32_t& nv3) noexcept
{
    if (surface.vertices.size() < 3)
    {
        return false;
    }

    if (nv1 < 0 || static_cast<std::size_t>(nv1) >= surface.vertices.size())
    {
        return false;
    }

    const Vertex* v0 = surfaceVertexAt(sector, surface, static_cast<std::size_t>(nv1));
    if (v0 == nullptr)
    {
        return false;
    }

    // Step 1: select a second vertex that gives the UV basis a nonzero edge.
    const std::int32_t vertexCount = static_cast<std::int32_t>(surface.vertices.size());
    if (!chooseSecondUvTriVertex(
            sector, surface, nv1, *v0, vertexCount, nv2))
    {
        return false;
    }

    const Vertex* v1 = surfaceVertexAt(sector, surface, static_cast<std::size_t>(nv2));
    if (v1 == nullptr)
    {
        return false;
    }

    // Step 2: build the axis used to detect a third vertex off that edge.
    TVector axis{};
    if (!buildUvTriSearchAxis(*v0, *v1, surface.normal, axis))
    {
        return false;
    }

    // Step 3: select a third vertex that gives the triangle area.
    return chooseThirdUvTriVertex(
        sector, surface, nv1, nv2, *v1, axis, vertexCount, nv3);
}

} // namespace

bool surfaceIsTextureFlipped(const Sector& sector, const Surface& surface) noexcept
{
    std::int32_t nv1 = 0;
    std::int32_t nv2 = 0;
    std::int32_t nv3 = 0;
    if (!findTri(sector, surface, nv1, nv2, nv3))
    {
        return false;
    }

    if (nv1 < 0 || nv2 < 0 || nv3 < 0)
    {
        return false;
    }

    const std::size_t count = surface.texVertices.size();
    if (static_cast<std::size_t>(nv1) >= count || static_cast<std::size_t>(nv2) >= count || static_cast<std::size_t>(nv3) >= count)
    {
        return false;
    }

    const TexVertex& tv1 = surface.texVertices[static_cast<std::size_t>(nv1)];
    const TexVertex& tv2 = surface.texVertices[static_cast<std::size_t>(nv2)];
    const TexVertex& tv3 = surface.texVertices[static_cast<std::size_t>(nv3)];
    if (!std::isfinite(tv1.u) || !std::isfinite(tv1.v)
        || !std::isfinite(tv2.u) || !std::isfinite(tv2.v)
        || !std::isfinite(tv3.u) || !std::isfinite(tv3.v))
    {
        return false;
    }

    const double lhs =
        (static_cast<double>(tv1.u) - static_cast<double>(tv2.u))
        * (static_cast<double>(tv3.v) - static_cast<double>(tv2.v));
    const double rhs =
        (static_cast<double>(tv3.u) - static_cast<double>(tv2.u))
        * (static_cast<double>(tv1.v) - static_cast<double>(tv2.v));
    return std::isfinite(lhs) && std::isfinite(rhs) && lhs > rhs;
}

double levelPixelPerUnit(const Level& level) noexcept
{
    if (!std::isfinite(level.ppunit) || level.ppunit <= 0.0)
    {
        return 320.0;
    }
    return level.ppunit;
}

void surfaceCalcDefaultUVNormals(const Sector& sector, const Surface& surface, const std::int32_t orgVertex, TVector& un, TVector& vn) noexcept
{
    un = TVector::zero;
    vn = TVector::zero;

    const std::int32_t count = static_cast<std::int32_t>(surface.vertices.size());
    if (count < 2)
    {
        return;
    }

    if (orgVertex < 0 || orgVertex >= count)
    {
        return;
    }

    const std::int32_t v1Index = surface.vertices[static_cast<std::size_t>(orgVertex)];
    const std::int32_t v2Index = surface.vertices[static_cast<std::size_t>(nextIndex(orgVertex, count))];
    if (v1Index < 0 || v2Index < 0 || static_cast<std::size_t>(v1Index) >= sector.vertices.size()
        || static_cast<std::size_t>(v2Index) >= sector.vertices.size())
    {
        return;
    }

    const Vertex& v1 = sector.vertices[static_cast<std::size_t>(v1Index)];
    const Vertex& v2 = sector.vertices[static_cast<std::size_t>(v2Index)];

    un = vertexPosition(v2) - vertexPosition(v1);
    un.normalize();

    vn = un.cross(surface.normal);
}

namespace
{

bool surfaceHasVertexAndTexIndex(const Surface& surface, const std::int32_t index) noexcept
{
    return index >= 0
        && static_cast<std::size_t>(index) < surface.vertices.size()
        && static_cast<std::size_t>(index) < surface.texVertices.size();
}

bool loadSurfaceUvTri(
    const Sector& sector,
    const Surface& surface,
    const std::int32_t nv1,
    const std::int32_t nv2,
    const std::int32_t nv3,
    const Vertex*& v1,
    const Vertex*& v2,
    const Vertex*& v3,
    const TexVertex*& tv1,
    const TexVertex*& tv2,
    const TexVertex*& tv3) noexcept
{
    if (!surfaceHasVertexAndTexIndex(surface, nv1)
        || !surfaceHasVertexAndTexIndex(surface, nv2)
        || !surfaceHasVertexAndTexIndex(surface, nv3))
    {
        return false;
    }

    v1 = surfaceVertexAt(sector, surface, static_cast<std::size_t>(nv1));
    v2 = surfaceVertexAt(sector, surface, static_cast<std::size_t>(nv2));
    v3 = surfaceVertexAt(sector, surface, static_cast<std::size_t>(nv3));
    if (v1 == nullptr || v2 == nullptr || v3 == nullptr)
    {
        return false;
    }

    tv1 = &surface.texVertices[static_cast<std::size_t>(nv1)];
    tv2 = &surface.texVertices[static_cast<std::size_t>(nv2)];
    tv3 = &surface.texVertices[static_cast<std::size_t>(nv3)];
    return true;
}

void calcUvAxesFromTri(
    const Vertex& v1,
    const TexVertex& tv1,
    const Vertex& v2,
    const TexVertex& tv2,
    const Vertex& v3,
    const TexVertex& tv3,
    const TVector& surfaceNormal,
    TVector& un,
    TVector& vn) noexcept
{
    const TVector a = vertexPosition(v1) - vertexPosition(v2);
    const TVector b = vertexPosition(v3) - vertexPosition(v2);

    const double du1 = static_cast<double>(tv1.u) - static_cast<double>(tv2.u);
    const double dv1 = static_cast<double>(tv1.v) - static_cast<double>(tv2.v);

    const double du2 = static_cast<double>(tv3.u) - static_cast<double>(tv2.u);
    const double dv2 = static_cast<double>(tv3.v) - static_cast<double>(tv2.v);

    // Step 1: solve the two edge vectors against their UV deltas to derive
    // world-space U/V axes.
    un = a.scaled(dv2) - b.scaled(dv1);
    vn = a.scaled(du2) - b.scaled(du1);

    // Step 2: preserve the winding corrections from the legacy implementation.
    if (dv2 * du1 < dv1 * du2)
    {
        un = -un;
    }
    if (du2 * dv1 < du1 * dv2)
    {
        vn = -vn;
    }

    // Step 3: if texture deltas produce a degenerate U axis, fall back to the
    // geometric edge before deriving V from the surface normal.
    if (un.normalize() == 0.0)
    {
        un = a;
        un.normalize();
    }

    vn = surfaceNormal.cross(un);
}

} // namespace

bool surfaceCalcUVNormalsFrom(const Sector& sector, const Surface& surface, const std::int32_t nv1, TVector& un, TVector& vn) noexcept
{
    un = TVector::zero;
    vn = TVector::zero;

    if (surface.vertices.size() < 3
        || surface.vertices.size() != surface.texVertices.size()
        || !std::isfinite(surface.normal.x)
        || !std::isfinite(surface.normal.y)
        || !std::isfinite(surface.normal.z))
    {
        return false;
    }
    for (std::size_t index = 0; index < surface.vertices.size(); ++index)
    {
        const std::int32_t vertexIndex = surface.vertices[index];
        const TexVertex& texVertex = surface.texVertices[index];
        if (vertexIndex < 0
            || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size()
            || !std::isfinite(texVertex.u)
            || !std::isfinite(texVertex.v))
        {
            return false;
        }

        const Vertex& vertex =
            sector.vertices[static_cast<std::size_t>(vertexIndex)];
        if (!std::isfinite(vertex.x)
            || !std::isfinite(vertex.y)
            || !std::isfinite(vertex.z))
        {
            return false;
        }
    }

    // Step 1: find a stable triangle on the surface to use as the UV basis.
    std::int32_t nv2 = 0;
    std::int32_t nv3 = 0;
    if (!findTri(sector, surface, nv1, nv2, nv3))
    {
        return false;
    }

    // Step 2: fetch matching geometry and texture vertices for that triangle.
    const Vertex* v1 = nullptr;
    const Vertex* v2 = nullptr;
    const Vertex* v3 = nullptr;
    const TexVertex* tv1 = nullptr;
    const TexVertex* tv2 = nullptr;
    const TexVertex* tv3 = nullptr;
    if (!loadSurfaceUvTri(sector, surface, nv1, nv2, nv3, v1, v2, v3, tv1, tv2, tv3))
    {
        return false;
    }

    // Step 3: derive world-space U/V axes from triangle geometry and UV deltas.
    calcUvAxesFromTri(*v1, *tv1, *v2, *tv2, *v3, *tv3, surface.normal, un, vn);
    const auto isFiniteNonZeroAxis = [](const TVector& axis) noexcept
    {
        const double lengthSqr = axis.lengthSqr();
        return std::isfinite(axis.x)
            && std::isfinite(axis.y)
            && std::isfinite(axis.z)
            && lengthSqr > 0.0
            && std::isfinite(lengthSqr);
    };
    if (!isFiniteNonZeroAxis(un) || !isFiniteNonZeroAxis(vn))
    {
        un = TVector::zero;
        vn = TVector::zero;
        return false;
    }
    return true;
}

bool surfaceCalcUVNormals(const Sector& sector, const Surface& surface, TVector& un, TVector& vn) noexcept
{
    return surfaceCalcUVNormalsFrom(sector, surface, 0, un, vn);
}

namespace
{

bool surfaceArrangeTextureByChecked(
    const double pixelsPerUnit,
    const Sector& sector,
    Surface& surface,
    const TVector& un,
    const TVector& vn,
    const double refx,
    const double refy,
    const double refz,
    const float refu,
    const float refv) noexcept
{
    if (surface.vertices.size() != surface.texVertices.size())
    {
        return false;
    }

    const auto calculateCoordinates = [&](const std::size_t index,
                                          float& u,
                                          float& vCoordinate)
    {
        const std::int32_t vi = surface.vertices[index];
        if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
        {
            return false;
        }

        const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vi)];
        const TVector offset =
            vertexPosition(vertex) - TVector{ refx, refy, refz };
        const TVector vAxis = (surface.faceFlags & FF_SF_FLIP) != 0 ? -vn : vn;
        return calculateArrangedUv(
                   refu,
                   un.dot(offset),
                   pixelsPerUnit,
                   surface.uScale,
                   u)
            && calculateArrangedUv(
                refv,
                vAxis.dot(offset),
                pixelsPerUnit,
                surface.vScale,
                vCoordinate);
    };

    for (std::size_t i = 0; i < surface.vertices.size(); ++i)
    {
        float u{};
        float v{};
        if (!calculateCoordinates(i, u, v))
        {
            return false;
        }
    }

    for (std::size_t i = 0; i < surface.vertices.size(); ++i)
    {
        float u{};
        float v{};
        if (!calculateCoordinates(i, u, v))
        {
            return false;
        }
        surface.texVertices[i].u = u;
        surface.texVertices[i].v = v;
    }
    return true;
}

bool surfaceArrangeTextureChecked(
    const double pixelsPerUnit,
    const Sector& sector,
    Surface& surface,
    const std::int32_t orgVertex,
    const TVector& un,
    const TVector& vn) noexcept
{
    if (surface.vertices.size() != surface.texVertices.size())
    {
        return false;
    }

    if (orgVertex < 0 || static_cast<std::size_t>(orgVertex) >= surface.vertices.size())
    {
        return false;
    }

    const std::int32_t vi = surface.vertices[static_cast<std::size_t>(orgVertex)];
    if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
    {
        return false;
    }

    const Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
    const TexVertex& tv = surface.texVertices[static_cast<std::size_t>(orgVertex)];
    return surfaceArrangeTextureByChecked(
        pixelsPerUnit,
        sector,
        surface,
        un,
        vn,
        v.x,
        v.y,
        v.z,
        tv.u,
        tv.v);
}

} // namespace

void surfaceArrangeTextureBy(
    const double pixelsPerUnit,
    const Sector& sector,
    Surface& surface,
    const TVector& un,
    const TVector& vn,
    const double refx,
    const double refy,
    const double refz,
    const float refu,
    const float refv) noexcept
{
    (void)surfaceArrangeTextureByChecked(
        pixelsPerUnit,
        sector,
        surface,
        un,
        vn,
        refx,
        refy,
        refz,
        refu,
        refv);
}

void surfaceArrangeTexture(
    const double pixelsPerUnit,
    const Sector& sector,
    Surface& surface,
    const std::int32_t orgVertex,
    const TVector& un,
    const TVector& vn) noexcept
{
    (void)surfaceArrangeTextureChecked(
        pixelsPerUnit, sector, surface, orgVertex, un, vn);
}

namespace
{

struct SharedSurfaceVertex
{
    std::int32_t sourceVertex{};
    std::int32_t targetVertex{};
};

struct SharedSurfaceEdge
{
    std::int32_t sourceVertex0{};
    std::int32_t sourceVertex1{};
    std::int32_t targetVertex0{};
    std::int32_t targetVertex1{};
    TVector tangent{};
};

bool verticesSharePosition(const Vertex& a, const Vertex& b) noexcept
{
    return precision::samePoint(precision::position(a), precision::position(b));
}

std::optional<SharedSurfaceEdge> findSharedSurfaceEdgeByPosition(
    const Sector& sourceSector,
    const Surface& sourceSurface,
    const Sector& targetSector,
    const Surface& targetSurface) noexcept
{
    if (sourceSurface.vertices.size() < 2 || targetSurface.vertices.size() < 2)
    {
        return std::nullopt;
    }

    for (std::size_t sourceIndex0 = 0; sourceIndex0 < sourceSurface.vertices.size(); ++sourceIndex0)
    {
        const std::size_t sourceIndex1 = (sourceIndex0 + 1u) % sourceSurface.vertices.size();
        const Vertex* source0 = surfaceVertexAt(sourceSector, sourceSurface, sourceIndex0);
        const Vertex* source1 = surfaceVertexAt(sourceSector, sourceSurface, sourceIndex1);
        if (source0 == nullptr || source1 == nullptr)
        {
            continue;
        }

        TVector tangent = vertexPosition(*source1) - vertexPosition(*source0);
        if (tangent.normalize() == 0.0)
        {
            continue;
        }

        for (std::size_t targetIndex0 = 0; targetIndex0 < targetSurface.vertices.size(); ++targetIndex0)
        {
            const std::size_t targetIndex1 = (targetIndex0 + 1u) % targetSurface.vertices.size();
            const Vertex* target0 = surfaceVertexAt(targetSector, targetSurface, targetIndex0);
            const Vertex* target1 = surfaceVertexAt(targetSector, targetSurface, targetIndex1);
            if (target0 == nullptr || target1 == nullptr)
            {
                continue;
            }

            if (verticesSharePosition(*source0, *target0)
                && verticesSharePosition(*source1, *target1))
            {
                return SharedSurfaceEdge{
                    static_cast<std::int32_t>(sourceIndex0),
                    static_cast<std::int32_t>(sourceIndex1),
                    static_cast<std::int32_t>(targetIndex0),
                    static_cast<std::int32_t>(targetIndex1),
                    tangent,
                };
            }
            if (verticesSharePosition(*source0, *target1)
                && verticesSharePosition(*source1, *target0))
            {
                return SharedSurfaceEdge{
                    static_cast<std::int32_t>(sourceIndex0),
                    static_cast<std::int32_t>(sourceIndex1),
                    static_cast<std::int32_t>(targetIndex1),
                    static_cast<std::int32_t>(targetIndex0),
                    tangent,
                };
            }
        }
    }

    return std::nullopt;
}

std::optional<SharedSurfaceVertex> findSharedSurfaceVertexByPosition(
    const Sector& sourceSector,
    const Surface& sourceSurface,
    const Sector& targetSector,
    const Surface& targetSurface) noexcept
{
    // Step 1: search by world position because the two surfaces usually live
    // in different sector vertex pools. The returned surface-vertex indices
    // anchor the copied UV coordinate.
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(sourceSurface.vertices.size()); ++i)
    {
        const Vertex* sourceVertex = surfaceVertexAt(sourceSector, sourceSurface, static_cast<std::size_t>(i));
        if (sourceVertex == nullptr)
        {
            continue;
        }

        for (std::int32_t j = 0; j < static_cast<std::int32_t>(targetSurface.vertices.size()); ++j)
        {
            const Vertex* targetVertex = surfaceVertexAt(targetSector, targetSurface, static_cast<std::size_t>(j));
            if (targetVertex == nullptr)
            {
                continue;
            }

            if (verticesSharePosition(*sourceVertex, *targetVertex))
            {
                return SharedSurfaceVertex{ i, j };
            }
        }
    }

    return std::nullopt;
}

bool projectSourceUvAxesAcrossSharedEdge(
    const Surface& sourceSurface,
    const Surface& targetSurface,
    TVector tangent,
    TVector& un,
    TVector& vn) noexcept
{
    TVector sourceNormal = sourceSurface.normal;
    TVector targetNormal = targetSurface.normal;
    if (tangent.normalize() == 0.0
        || sourceNormal.normalize() == 0.0
        || targetNormal.normalize() == 0.0)
    {
        return false;
    }

    // Rounded authored normals can contain a small component along the real
    // shared edge. Build both in-plane perpendiculars from the authoritative
    // edge so that component cannot turn into a visible seam on banked faces.
    TVector sourcePerpendicular = sourceNormal.cross(tangent);
    TVector targetPerpendicular = targetNormal.cross(tangent);
    if (sourcePerpendicular.normalize() == 0.0
        || targetPerpendicular.normalize() == 0.0)
    {
        return false;
    }

    // Exactly opposite coplanar normals are the legacy portal-face case. The
    // normal-cross projection deliberately leaves those axes in world space.
    if (sourcePerpendicular.dot(targetPerpendicular) < -0.999999)
    {
        return false;
    }

    const auto transport = [&tangent, &sourcePerpendicular, &targetPerpendicular](const TVector& axis)
    {
        return tangent.scaled(axis.dot(tangent))
            + targetPerpendicular.scaled(axis.dot(sourcePerpendicular));
    };

    const TVector projectedU = transport(un);
    const TVector projectedV = transport(vn);
    const auto isUsable = [](const TVector& axis)
    {
        return std::isfinite(axis.x)
            && std::isfinite(axis.y)
            && std::isfinite(axis.z)
            && axis.lengthSqr() > 0.0
            && std::isfinite(axis.lengthSqr());
    };
    if (!isUsable(projectedU) || !isUsable(projectedV))
    {
        return false;
    }

    un = projectedU;
    vn = projectedV;
    return true;
}

void projectSourceUvAxesToTargetSurface(const Surface& sourceSurface, const Surface& targetSurface, TVector& un, TVector& vn) noexcept
{
    // Step 1: when the surfaces are not coplanar, project the source UV axes
    // through the intersection line so texture orientation follows the target
    // surface rather than staying stuck in the old plane.
    TVector sourceNormal = sourceSurface.normal;
    TVector targetNormal = targetSurface.normal;
    if (sourceNormal.normalize() == 0.0 || targetNormal.normalize() == 0.0)
    {
        return;
    }

    TVector lv = sourceNormal.cross(targetNormal);
    if (lv.normalize() == 0.0)
    {
        return;
    }

    const TVector p1v = sourceNormal.cross(lv);
    const TVector p2v = targetNormal.cross(lv);

    const double a0 = p1v.dot(un);
    const double b0 = lv.dot(un);
    un = p2v.scaled(a0) + lv.scaled(b0);

    const double a1 = p1v.dot(vn);
    const double b1 = lv.dot(vn);
    vn = p2v.scaled(a1) + lv.scaled(b1);
}

void copyTextureScaleAndFlip(const Surface& sourceSurface, Surface& targetSurface) noexcept
{
    targetSurface.uScale = sourceSurface.uScale;
    targetSurface.vScale = sourceSurface.vScale;

    if ((sourceSurface.faceFlags & FF_SF_FLIP) != 0)
    {
        targetSurface.faceFlags |= FF_SF_FLIP;
    }
    else
    {
        targetSurface.faceFlags &= ~FF_SF_FLIP;
    }
}

bool surfaceHasCompleteUvGeometry(const Sector& sector, const Surface& surface) noexcept
{
    if (surface.vertices.size() < 3
        || surface.vertices.size() != surface.texVertices.size()
        || !std::isfinite(surface.uScale)
        || !std::isfinite(surface.vScale)
        || !std::isfinite(surface.normal.x)
        || !std::isfinite(surface.normal.y)
        || !std::isfinite(surface.normal.z))
    {
        return false;
    }

    const double normalLengthSqr = surface.normal.lengthSqr();
    if (!(normalLengthSqr > 0.0) || !std::isfinite(normalLengthSqr))
    {
        return false;
    }

    for (std::size_t i = 0; i < surface.vertices.size(); ++i)
    {
        const std::int32_t vertexIndex = surface.vertices[i];
        const TexVertex& texVertex = surface.texVertices[i];
        if (vertexIndex < 0
            || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size()
            || !std::isfinite(texVertex.u)
            || !std::isfinite(texVertex.v))
        {
            return false;
        }

        const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
        if (!std::isfinite(vertex.x)
            || !std::isfinite(vertex.y)
            || !std::isfinite(vertex.z))
        {
            return false;
        }
    }
    return true;
}

bool isFiniteUvAxis(const TVector& axis) noexcept
{
    if (!std::isfinite(axis.x)
        || !std::isfinite(axis.y)
        || !std::isfinite(axis.z))
    {
        return false;
    }

    const double lengthSqr = axis.lengthSqr();
    return lengthSqr > 0.0 && std::isfinite(lengthSqr);
}

} // namespace

bool surfaceAutoAlignUV(const double pixelsPerUnit, const Sector& sector, Surface& surface) noexcept
{
    if (!(pixelsPerUnit > 0.0) || !std::isfinite(pixelsPerUnit))
    {
        return false;
    }

    // RecalcAll in the original SED derives the replacement texture frame
    // from the authored UVs, then arranges the complete polygon around vertex
    // zero. Deriving a new frame from polygon edge zero instead rotates
    // otherwise continuous adjacent faces whenever their local vertex orders
    // begin on different world-space edges.
    Surface aligned = surface;
    aligned.normal = surfaceCalcNormalNewell(sector, aligned);
    if (!surfaceHasCompleteUvGeometry(sector, aligned))
    {
        return false;
    }

    TVector un{};
    TVector vn{};
    if (!surfaceCalcUVNormals(sector, aligned, un, vn)
        || !isFiniteUvAxis(un)
        || !isFiniteUvAxis(vn))
    {
        return false;
    }

    if (!surfaceArrangeTextureChecked(
            pixelsPerUnit, sector, aligned, 0, un, vn)
        || !surfaceHasCompleteUvGeometry(sector, aligned))
    {
        return false;
    }

    surface = std::move(aligned);
    return true;
}

bool levelStitchSurfaces(Level& level, const SurfaceRef source, const SurfaceRef target) noexcept
{
    if (!isValidSurfaceRef(level, source)
        || !isValidSurfaceRef(level, target)
        || (source.sector == target.sector && source.surface == target.surface))
    {
        return false;
    }

    const Sector& sourceSector = level.sectors[static_cast<std::size_t>(source.sector)];
    const Surface& sourceSurface = sourceSector.surfaces[static_cast<std::size_t>(source.surface)];

    Sector& targetSector = level.sectors[static_cast<std::size_t>(target.sector)];
    Surface& targetSurface = targetSector.surfaces[static_cast<std::size_t>(target.surface)];

    // Validate every source/target vertex and UV before deriving or publishing
    // anything. ArrangeTexture deliberately skips invalid indices, which is
    // useful as a low-level primitive but must not turn Stitch into a partial
    // successful edit on malformed C++ data.
    if (!surfaceHasCompleteUvGeometry(sourceSector, sourceSurface)
        || !surfaceHasCompleteUvGeometry(targetSector, targetSurface))
    {
        return false;
    }

    Surface stitchedTarget = targetSurface;

    std::int32_t nv1 = -1;
    std::int32_t nv2 = 0;

    // A complete common edge is more authoritative than rounded stored face
    // normals for angled/banked surfaces. Prefer it for both the UV anchor and
    // basis transport, while retaining Pascal's common-vertex fallback.
    const std::optional<SharedSurfaceEdge> sharedEdge =
        findSharedSurfaceEdgeByPosition(sourceSector, sourceSurface, targetSector, targetSurface);

    // Step 1: find a shared world-space vertex to anchor the target UV frame.
    if (sharedEdge)
    {
        nv1 = sharedEdge->sourceVertex0;
        nv2 = sharedEdge->targetVertex0;
    }
    else if (const std::optional<SharedSurfaceVertex> shared =
                 findSharedSurfaceVertexByPosition(sourceSector, sourceSurface, targetSector, targetSurface))
    {
        nv1 = shared->sourceVertex;
        nv2 = shared->targetVertex;
    }

    TVector un{};
    TVector vn{};

    // Step 2: use the shared source vertex when available, otherwise fall back
    // to the source surface's default UV basis and anchor target vertex 0.
    if (nv1 != -1)
    {
        if (!surfaceCalcUVNormalsFrom(sourceSector, sourceSurface, nv1, un, vn))
        {
            return false;
        }
    }
    else
    {
        if (!surfaceCalcUVNormals(sourceSector, sourceSurface, un, vn))
        {
            return false;
        }

        nv2 = 0;
    }

    if (!isFiniteUvAxis(un) || !isFiniteUvAxis(vn))
    {
        return false;
    }

    // The legacy basis builder uses normal.cross(U), which loses authored
    // mirrored V orientation. Recover axis signs from actual UV gradients;
    // retain the existing orthogonal frame and textureScale density policy.
    // Arrangement applies signed scale and flip, so remove those signs here.
    std::int32_t basisSecond = 0;
    std::int32_t basisThird = 0;
    const std::int32_t basisFirst = nv1 >= 0 ? nv1 : 0;
    if (!findTri(sourceSector, sourceSurface, basisFirst, basisSecond, basisThird))
        return false;
    const Vertex *p0 = nullptr, *p1 = nullptr, *p2 = nullptr;
    const TexVertex *uv0 = nullptr, *uv1 = nullptr, *uv2 = nullptr;
    if (!loadSurfaceUvTri(sourceSector, sourceSurface, basisFirst, basisSecond, basisThird,
            p0, p1, p2, uv0, uv1, uv2)) return false;
    TVector firstEdge = vertexPosition(*p1) - vertexPosition(*p0);
    TVector secondEdge = vertexPosition(*p2) - vertexPosition(*p0);
    const double edgeScale = std::max({std::abs(firstEdge.x), std::abs(firstEdge.y), std::abs(firstEdge.z),
        std::abs(secondEdge.x), std::abs(secondEdge.y), std::abs(secondEdge.z)});
    if (!(edgeScale > 0.0) || !std::isfinite(edgeScale)) return false;
    firstEdge = {firstEdge.x / edgeScale, firstEdge.y / edgeScale, firstEdge.z / edgeScale};
    secondEdge = {secondEdge.x / edgeScale, secondEdge.y / edgeScale, secondEdge.z / edgeScale};
    TVector area = firstEdge.cross(secondEdge);
    if (area.normalize() == 0.0) return false;
    const TVector firstDual = secondEdge.cross(area);
    const TVector secondDual = area.cross(firstEdge);
    const TVector uGradient = firstDual * (static_cast<double>(uv1->u) - uv0->u)
        + secondDual * (static_cast<double>(uv2->u) - uv0->u);
    const TVector vGradient = firstDual * (static_cast<double>(uv1->v) - uv0->v)
        + secondDual * (static_cast<double>(uv2->v) - uv0->v);
    const double uOrientation = un.dot(uGradient);
    const double vOrientation = vn.dot(vGradient);
    if (uOrientation != 0.0)
    {
        if (uOrientation < 0.0) un = -un;
        if (sourceSurface.uScale < 0.0f) un = -un;
    }
    if (vOrientation != 0.0)
    {
        if (vOrientation < 0.0) vn = -vn;
        if ((sourceSurface.vScale < 0.0f) != ((sourceSurface.faceFlags & FF_SF_FLIP) != 0)) vn = -vn;
    }

    // Step 3: project the source UV axes onto the target surface when needed.
    // Shared edges remove seam drift caused by rounded normals; disconnected
    // surfaces retain the original SED/JED normal-intersection projection.
    if (!sharedEdge
        || !projectSourceUvAxesAcrossSharedEdge(
            sourceSurface, stitchedTarget, sharedEdge->tangent, un, vn))
    {
        projectSourceUvAxesToTargetSurface(sourceSurface, stitchedTarget, un, vn);
    }
    if (!isFiniteUvAxis(un) || !isFiniteUvAxis(vn))
    {
        return false;
    }

    if (nv1 != -1)
    {
        if (nv2 < 0 || static_cast<std::size_t>(nv2) >= stitchedTarget.texVertices.size())
        {
            return false;
        }

        const TexVertex& tv = sourceSurface.texVertices[static_cast<std::size_t>(nv1)];
        stitchedTarget.texVertices[static_cast<std::size_t>(nv2)].u = tv.u;
        stitchedTarget.texVertices[static_cast<std::size_t>(nv2)].v = tv.v;
    }

    // Step 4: copy texture scale/flip state, then rebuild target UVs around
    // the selected anchor vertex. Publish only after the complete candidate
    // has been calculated so every failure path leaves the target untouched.
    copyTextureScaleAndFlip(sourceSurface, stitchedTarget);

    if (!surfaceArrangeTextureChecked(
            levelPixelPerUnit(level),
            targetSector,
            stitchedTarget,
            nv2,
            un,
            vn)
        || !surfaceHasCompleteUvGeometry(targetSector, stitchedTarget))
    {
        return false;
    }
    targetSurface = std::move(stitchedTarget);
    return true;
}

void surfaceCalcUV(const double pixelsPerUnit, const Sector& sector, Surface& surface, const std::int32_t orgVertex) noexcept
{
    TVector un{};
    TVector vn{};
    surfaceCalcDefaultUVNormals(sector, surface, orgVertex, un, vn);
    surfaceArrangeTexture(pixelsPerUnit, sector, surface, orgVertex, un, vn);
}

namespace
{

bool calcTextureScaleFromVertexDelta(
    const double pixelsPerUnit,
    const Sector& sector,
    const Surface& surface,
    const Vertex& referenceVertex,
    const std::int32_t surfaceVertex,
    const double uvDelta,
    const TVector& axis,
    float& outScale) noexcept
{
    if (surfaceVertex < 0 || static_cast<std::size_t>(surfaceVertex) >= surface.vertices.size())
    {
        return false;
    }

    const std::int32_t vertexIndex = surface.vertices[static_cast<std::size_t>(surfaceVertex)];
    if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
    {
        return false;
    }

    // Step 1: project the geometric vertex delta onto the requested UV axis.
    const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
    const TVector vertexDelta = vertexPosition(vertex) - vertexPosition(referenceVertex);
    const double axisDelta = axis.dot(vertexDelta);
    if (!std::isfinite(uvDelta) || !std::isfinite(axisDelta))
    {
        return false;
    }

    const double projectedDelta = axisDelta * pixelsPerUnit;
    if (!std::isfinite(projectedDelta))
    {
        return false;
    }

    // Step 2: convert UV delta over geometric delta into a positive scale.
    // A zero projected delta keeps the legacy 1.0 fallback. Values that
    // cannot be stored as float fail so a strict caller can reject the whole
    // import instead of mistaking a fallback for authored scale metadata.
    const double scale = (projectedDelta == 0.0)
        ? 1.0
        : std::abs(uvDelta / projectedDelta);
    if (!std::isfinite(scale)
        || scale > static_cast<double>(
            std::numeric_limits<float>::max()))
    {
        return false;
    }
    outScale = static_cast<float>(scale);
    return true;
}

} // namespace

bool surfaceFindUVScales(const double pixelsPerUnit, const Sector& sector, Surface& surface) noexcept
{
    if (!std::isfinite(pixelsPerUnit)
        || pixelsPerUnit <= 0.0
        || !surfaceHasCompleteUvGeometry(sector, surface))
    {
        return false;
    }

    // Step 1: derive the current UV axes. Without usable axes the existing
    // scale values are left untouched.
    TVector un{};
    TVector vn{};
    if (!surfaceCalcUVNormals(sector, surface, un, vn))
    {
        return false;
    }

    std::int32_t nv2 = 0;
    std::int32_t nv3 = 0;
    if (!findTri(sector, surface, 0, nv2, nv3))
    {
        return false;
    }

    if (surface.vertices.size() != surface.texVertices.size())
    {
        return false;
    }

    const std::int32_t v1Index = surface.vertices[0];
    if (v1Index < 0 || static_cast<std::size_t>(v1Index) >= sector.vertices.size())
    {
        return false;
    }

    const Vertex& v1 = sector.vertices[static_cast<std::size_t>(v1Index)];
    const TexVertex& tv1 = surface.texVertices[0];

    // Step 2: use the same stable triangle as UV-normal calculation so scale
    // recovery reads from meaningful texture deltas.
    const TexVertex& tv2u = surface.texVertices[static_cast<std::size_t>(nv2)];
    const TexVertex& tv3u = surface.texVertices[static_cast<std::size_t>(nv3)];

    // Step 3: prefer positive U deltas; when the first candidate does not move
    // in U, fall back to the third triangle vertex.
    double du = static_cast<double>(tv2u.u)
        - static_cast<double>(tv1.u);
    std::int32_t nvU = nv2;
    if (du <= 0.0)
    {
        du = static_cast<double>(tv3u.u)
            - static_cast<double>(tv1.u);
        nvU = nv3;
    }

    float uScale = 1.0f;
    if (!calcTextureScaleFromVertexDelta(
            pixelsPerUnit, sector, surface, v1, nvU, du, un, uScale))
    {
        return false;
    }

    // Step 4: repeat the same candidate selection for V scale.
    double dv = static_cast<double>(tv2u.v)
        - static_cast<double>(tv1.v);
    std::int32_t nvV = nv2;
    if (dv <= 0.0)
    {
        dv = static_cast<double>(tv3u.v)
            - static_cast<double>(tv1.v);
        nvV = nv3;
    }

    float vScale = 1.0f;
    if (!calcTextureScaleFromVertexDelta(
            pixelsPerUnit, sector, surface, v1, nvV, dv, vn, vScale))
    {
        return false;
    }

    surface.uScale = uScale;
    surface.vScale = vScale;
    return true;
}

void surfaceCheckIfFloor(Surface& surface) noexcept
{
    const double floorNormalThreshold = std::cos(std::numbers::pi_v<double> / 4.0);
    const double cosa = surface.normal.dot(TVector::up);
    if (cosa <= 1.0 && cosa > floorNormalThreshold)
    {
        surface.surfaceFlags |= SF_Floor | SF_Collision;
    }
    else
    {
        surface.surfaceFlags &= ~SF_Floor;
    }
}

void surfaceRecalcAll(const double pixelsPerUnit, Sector& sector, Surface& surface) noexcept
{
    surface.normal = surfaceCalcNormalNewell(sector, surface);

    TVector un{};
    TVector vn{};
    (void)surfaceCalcUVNormals(sector, surface, un, vn);
    surfaceArrangeTexture(pixelsPerUnit, sector, surface, 0, un, vn);
}

void surfaceRecalcNewAll(const double pixelsPerUnit, Sector& sector, Surface& surface, const std::int32_t orgVertex) noexcept
{
    surface.normal = surfaceCalcNormalNewell(sector, surface);
    surfaceCalcUV(pixelsPerUnit, sector, surface, orgVertex);
    surfaceCheckIfFloor(surface);
}

namespace
{

bool buildSurfaceProjectionAxes(
    const Sector& sector,
    const Surface& surface,
    const Vertex*& origin,
    TVector& xAxis,
    TVector& yAxis) noexcept
{
    if (surface.vertices.size() < 2)
    {
        return false;
    }

    origin = surfaceVertexAt(sector, surface, 0);
    const Vertex* edgeVertex = surfaceVertexAt(sector, surface, 1);
    if ( origin == nullptr || edgeVertex == nullptr || !finiteDirection(surface.normal)
        || !finiteDirection(vertexPosition(*origin)) || !finiteDirection(vertexPosition(*edgeVertex)) )
    {
        return false;
    }

    // Step 1: use the first surface edge as local X, preserving the previous
    // behavior where degenerate edges produce zero axes instead of failing here.
    xAxis = vertexPosition(*edgeVertex) - vertexPosition(*origin);
    if ( !finiteDirection(xAxis) )
    {
        return false;
    }
    if ( xAxis.x != 0.0 || xAxis.y != 0.0 || xAxis.z != 0.0 )
    {
        normalizeFiniteDirection(xAxis);
    }

    // Step 2: derive local Y from the surface normal.
    yAxis = surface.normal.cross(xAxis);
    return finiteDirection(yAxis);
}

bool expandProjectedSurfaceBounds(
    const Sector& sector,
    const Surface& surface,
    const Vertex& origin,
    const TVector& xAxis,
    const TVector& yAxis,
    double& minx,
    double& miny,
    double& maxx,
    double& maxy) noexcept
{
    // Bounds already contain the origin (0, 0); include every remaining corner.
    for (std::size_t i = 1; i < surface.vertices.size(); ++i)
    {
        const Vertex* vertex = surfaceVertexAt(sector, surface, i);
        if ( vertex == nullptr || !finiteDirection(vertexPosition(*vertex)) )
        {
            return false;
        }

        const TVector offset = vertexPosition(*vertex) - vertexPosition(origin);
        const double x = xAxis.dot(offset);
        const double y = yAxis.dot(offset);

        if ( !std::isfinite(x) || !std::isfinite(y) )
        {
            return false;
        }
        minx = std::min(minx, x);
        miny = std::min(miny, y);
        maxx = std::max(maxx, x);
        maxy = std::max(maxy, y);
    }

    return true;
}

bool calcSurfacePoseBasis(const Sector& sector, const Surface& surface, TVector& xAxis, TVector& yAxis, TVector& zAxis) noexcept
{
    if (surface.vertices.size() < 2)
    {
        return false;
    }

    const Vertex* v1 = surfaceVertexAt(sector, surface, 0);
    const Vertex* v2 = surfaceVertexAt(sector, surface, 1);
    if (v1 == nullptr || v2 == nullptr)
    {
        return false;
    }

    // Step 1: surface normal becomes object up.
    zAxis = surface.normal;
    if ( !normalizeFiniteDirection(zAxis) )
    {
        return false;
    }

    // Step 2: first surface edge becomes object forward.
    yAxis = vertexPosition(*v2) - vertexPosition(*v1);
    if ( !normalizeFiniteDirection(yAxis) )
    {
        return false;
    }

    // Step 3: right completes the basis.
    xAxis = yAxis.cross(zAxis);
    return normalizeFiniteDirection(xAxis);
}

} // namespace

bool surfaceCalcAxesAndBounds(
    const Sector& sector,
    const Surface& surface,
    TVector& xAxis,
    TVector& yAxis,
    double& minx,
    double& miny,
    double& maxx,
    double& maxy) noexcept
{
    const Vertex* origin = nullptr;
    TVector localX{}, localY{};
    double lowX = 0.0, lowY = 0.0, highX = 0.0, highY = 0.0;
    if ( !buildSurfaceProjectionAxes(sector, surface, origin, localX, localY)
        || !expandProjectedSurfaceBounds(sector, surface, *origin, localX, localY,
            lowX, lowY, highX, highY) )
    {
        return false;
    }
    xAxis = localX;
    yAxis = localY;
    minx = lowX;
    miny = lowY;
    maxx = highX;
    maxy = highY;
    return true;
}

bool surfaceCalcCenter(const Sector& sector, const Surface& surface, TVector& center) noexcept
{
    if (surface.vertices.size() < 3)
    {
        center = TVector::zero;
        return false;
    }

    const Vertex* first = surfaceVertexAt(sector, surface, 0);
    if (first == nullptr)
    {
        center = TVector::zero;
        return false;
    }

    // A vertex average (and the older projected-bounds midpoint) is biased by
    // redundant collinear edge vertices.  Triangulate the planar polygon as a
    // fan and weight each triangle centroid by its signed area along the
    // polygon's Newell normal.  The ratio is independent of winding, so the
    // result is the true planar area centroid for both inward and outward
    // authored faces.
    const TVector normal = surfaceCalcNormalNewell(sector, surface);
    const TVector origin = vertexPosition(*first);
    double extent = 0.0;
    bool valid = finiteDirection(origin);
    for ( std::size_t index = 1; valid && index < surface.vertices.size(); ++index )
    {
        const Vertex* vertex = surfaceVertexAt(sector, surface, index);
        if ( vertex == nullptr || !finiteDirection(vertexPosition(*vertex)) )
        {
            valid = false;
            break;
        }
        const TVector offset = vertexPosition(*vertex) - origin;
        valid = finiteDirection(offset);
        extent = std::max({extent, std::abs(offset.x), std::abs(offset.y), std::abs(offset.z)});
    }
    if ( valid && extent > 0.0 && finiteDirection(normal)
        && (normal.x != 0.0 || normal.y != 0.0 || normal.z != 0.0) )
    {
        std::array<double, 4> sums{}, corrections{};
        const auto add = [&](std::size_t index, double value)
        {
            const double adjusted = value - corrections[index];
            const double next = sums[index] + adjusted;
            corrections[index] = (next - sums[index]) - adjusted;
            sums[index] = next;
        };
        const auto localPoint = [&](const Vertex& vertex)
        {
            return TVector{(vertex.x - origin.x) / extent,
                (vertex.y - origin.y) / extent, (vertex.z - origin.z) / extent};
        };
        for ( std::size_t index = 1; index + 1 < surface.vertices.size(); ++index )
        {
            const TVector b = localPoint(*surfaceVertexAt(sector, surface, index));
            const TVector c = localPoint(*surfaceVertexAt(sector, surface, index + 1));
            const double area = b.cross(c).dot(normal);
            add(0, area);
            add(1, (b.x + c.x) * area);
            add(2, (b.y + c.y) * area);
            add(3, (b.z + c.z) * area);
        }
        if ( std::isfinite(sums[0]) && sums[0] != 0.0 )
        {
            const double denominator = 3.0 * sums[0];
            const TVector result{origin.x + (sums[1] / denominator) * extent,
                origin.y + (sums[2] / denominator) * extent,
                origin.z + (sums[3] / denominator) * extent};
            if ( finiteDirection(result) )
            {
                center = result;
                return true;
            }
        }
    }

    // Degenerate legacy geometry still needs a stable selection/placement
    // marker.  Fall back to the arithmetic mean of valid finite vertices; a
    // completely invalid face remains a hard failure.
    TVector sum{};
    std::size_t count = 0;
    for (std::size_t index = 0; index < surface.vertices.size(); ++index)
    {
        const Vertex* vertex = surfaceVertexAt(sector, surface, index);
        if (vertex == nullptr || !std::isfinite(vertex->x)
            || !std::isfinite(vertex->y) || !std::isfinite(vertex->z))
        {
            continue;
        }
        ++count;
        const double weight = 1.0 / static_cast<double>(count);
        sum = {std::lerp(sum.x, vertex->x, weight), std::lerp(sum.y, vertex->y, weight),
            std::lerp(sum.z, vertex->z, weight)};
    }
    if (count == 0)
    {
        center = TVector::zero;
        return false;
    }
    center = sum;
    return true;
}

bool thingCalcPoseOnSurface(
    const Thing& thing,
    const Sector& sector,
    const Surface& surface,
    TVector& position,
    double& pitch,
    double& yaw,
    double& roll) noexcept
{
    // The centroid's partial legacy marker fallback is not a placement surface.
    for ( const std::int32_t index : surface.vertices )
    {
        if ( index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size()
            || !finiteDirection(vertexPosition(sector.vertices[static_cast<std::size_t>(index)])) )
        {
            return false;
        }
    }
    TVector pos{}, xv{}, yv{}, zv{};
    if ( !std::isfinite(thing.bbox.z1) || !surfaceCalcCenter(sector, surface, pos)
        || !calcSurfacePoseBasis(sector, surface, xv, yv, zv) )
    {
        return false;
    }
    pos = pos + zv.scaled(-static_cast<double>(thing.bbox.z1));
    double localPitch = 0.0, localYaw = 0.0, localRoll = 0.0;
    sed::getJKPYR(xv, yv, zv, localPitch, localYaw, localRoll);
    if ( !finiteDirection(pos) || !finiteDirection({localPitch, localYaw, localRoll}) )
    {
        return false;
    }
    position = pos;
    pitch = localPitch;
    yaw = localYaw;
    roll = localRoll;
    return true;
}

} // namespace sed::level
