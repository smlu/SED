#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "sed/level.hpp"

namespace sed::level
{

struct BoundingSphere
{
    TVector center{};
    double radius{};
};

constexpr bool doBoxesIntersect(const TBox& box1, const TBox& box2) noexcept
{
    if (box1.x2 < box2.x1)
    {
        return false;
    }
    if (box1.x1 > box2.x2)
    {
        return false;
    }
    if (box1.y2 < box2.y1)
    {
        return false;
    }
    if (box1.y1 > box2.y2)
    {
        return false;
    }
    if (box1.z2 < box2.z1)
    {
        return false;
    }
    if (box1.z1 > box2.z2)
    {
        return false;
    }
    return true;
}

constexpr bool isPointInBox(const TBox& box, const double x, const double y, const double z) noexcept
{
    return (x - box.x1 >= -CloseEnough) && (x - box.x2 <= CloseEnough)
        && (y - box.y1 >= -CloseEnough) && (y - box.y2 <= CloseEnough)
        && (z - box.z1 >= -CloseEnough) && (z - box.z2 <= CloseEnough);
}

constexpr void setBox(TBox& box, const double x1, const double x2, const double y1, const double y2, const double z1, const double z2) noexcept
{
    box.Set(
        std::min(x1, x2),
        std::min(y1, y2),
        std::min(z1, z2),
        std::max(x1, x2),
        std::max(y1, y2),
        std::max(z1, z2));
}

constexpr bool doBoxesIntersect(const TBox2& box1, const TBox2& box2) noexcept
{
    if (box1.x2 < box2.x1)
    {
        return false;
    }
    if (box1.x1 > box2.x2)
    {
        return false;
    }
    if (box1.y2 < box2.y1)
    {
        return false;
    }
    if (box1.y1 > box2.y2)
    {
        return false;
    }
    return true;
}

constexpr bool isPointInBox(const TBox2& box, const double x, const double y) noexcept
{
    return (x - box.x1 >= -CloseEnough) && (x - box.x2 <= CloseEnough)
        && (y - box.y1 >= -CloseEnough) && (y - box.y2 <= CloseEnough);
}

constexpr void setBox(TBox2& box, const double x1, const double x2, const double y1, const double y2) noexcept
{
    box.Set(
        std::min(x1, x2),
        std::min(y1, y2),
        std::max(x1, x2),
        std::max(y1, y2));
}

constexpr std::int32_t nextIndex(const std::int32_t index, const std::int32_t count) noexcept
{
    if (count <= 0)
    {
        return 0;
    }

    if (index == count - 1)
    {
        return 0;
    }

    return index + 1;
}

constexpr std::int32_t prevIndex(const std::int32_t index, const std::int32_t count) noexcept
{
    if (count <= 0)
    {
        return 0;
    }

    if (index == 0)
    {
        return count - 1;
    }

    return index - 1;
}

constexpr bool isAbove180(const double x, const double y, const double xa, const double ya, const double xb, const double yb) noexcept
{
    const double x1 = x - xb;
    const double x2 = x - xa;
    const double y1 = y - yb;
    const double y2 = y - ya;
    return (x1 * y2 - x2 * y1) > 0.0;
}

double getAngle(double x, double y, double xa, double ya, double xb, double yb) noexcept;
bool doIntersect(double x11, double y11, double x12, double y12, double x21, double y21, double x22, double y22) noexcept;

constexpr bool isPointWithinLine(
    const double x,
    const double y,
    const double z,
    const double x1,
    const double y1,
    const double z1,
    const double x2,
    const double y2,
    const double z2) noexcept
{
    return (x >= std::min(x1, x2)) && (x <= std::max(x1, x2))
        && (y >= std::min(y1, y2)) && (y <= std::max(y1, y2))
        && (z >= std::min(z1, z2)) && (z <= std::max(z1, z2));
}

bool isOnLine(double x1, double y1, double z1, double x2, double y2, double z2, double x, double y, double z) noexcept;

bool planeLineIntersection(
    const TVector& normal,
    double px,
    double py,
    double pz,
    double x1,
    double y1,
    double z1,
    double x2,
    double y2,
    double z2,
    double& x,
    double& y,
    double& z) noexcept;

bool planeLineIntersection(
    const TVector& normal,
    double d,
    double x1,
    double y1,
    double z1,
    double x2,
    double y2,
    double z2,
    double& x,
    double& y,
    double& z) noexcept;

bool getNearestVertexToLine(
    std::span<const Vertex> vertices,
    double lx,
    double ly,
    double lz,
    const TVector& lineDirection,
    double radius,
    double& vx,
    double& vy,
    double& vz,
    double& minDistanceSqr) noexcept;

bool getNearestVertexToLine(
    std::span<const Vertex> vertices,
    double lx,
    double ly,
    double lz,
    const TVector& lineDirection,
    double radius,
    double& vx,
    double& vy,
    double& vz) noexcept;

TBox sectorCalcBBox(const Sector& sector) noexcept;
TVector sectorCalcCenter(const Sector& sector) noexcept;
BoundingSphere sectorCalcBoundingSphere(const Sector& sector) noexcept;
/** Build the editor default for a newly authored IJIM Sector point light.
 *
 * Original IJIM levels use the Sector AABB diagonal as the first/outer
 * `AVERAGE LIGHT FALLOFF` value and one quarter of that distance as the
 * second/inner value.  The source is placed at the Sector vertex centroid.
 * Empty, degenerate, or non-finite geometry has no usable default.
 */
std::optional<PointLight> sectorMakeDefaultIjimPointLight(const Sector& sector) noexcept;
bool sectorContainsPoint(const Sector& sector, double x, double y, double z) noexcept;
bool sectorContainsPointNoSides(const Sector& sector, double x, double y, double z) noexcept;
bool sectorFindCollideBox(const Sector& sector, const TBox& bbox, double cx, double cy, double cz, TBox& collideBox) noexcept;
bool sectorDoOverlap(const Sector& sector1, const Sector& sector2) noexcept;
bool sectorIsConvex(const Sector& sector) noexcept;

TBox surfaceCalcBBox(const Sector& sector, const Surface& surface) noexcept;
TVector surfaceCalcNormalNewell(const Sector& sector, const Surface& surface) noexcept;
bool surfaceIsTextureFlipped(const Sector& sector, const Surface& surface) noexcept;
bool surfaceIsCrossed(const Sector& sector, const Surface& surface, double x1, double y1, double z1, double x2, double y2, double z2) noexcept;
bool surfaceDoOverlap(const Sector& sector1, const Surface& surface1, const Sector& sector2, const Surface& surface2) noexcept;
double surfaceDistToPlaneAbs(const Sector& sector, const Surface& surface, double x, double y, double z) noexcept;
bool surfaceIsPointOn(const Sector& sector, const Surface& surface, double x, double y, double z) noexcept;
bool surfaceIsPlanar(const Sector& sector, const Surface& surface) noexcept;
bool surfaceIsConvex(const Sector& sector, const Surface& surface) noexcept;
// Compatibility diagnostics for legacy text-coordinate quantization. Editing
// publication must continue to use the strict predicates above.
bool surfaceIsPlanarForDiagnostics(const Sector& sector, const Surface& surface) noexcept;
bool surfaceIsConvexForDiagnostics(const Sector& sector, const Surface& surface) noexcept;
bool sectorIsConvexForDiagnostics(const Sector& sector) noexcept;
bool surfaceBoundariesMatchForDiagnostics(const Sector& firstSector, const Surface& first,
    const Sector& secondSector, const Surface& second) noexcept;
bool surfaceCalcAxesAndBounds(
    const Sector& sector,
    const Surface& surface,
    TVector& xAxis,
    TVector& yAxis,
    double& minx,
    double& miny,
    double& maxx,
    double& maxy) noexcept;
bool surfaceCalcCenter(const Sector& sector, const Surface& surface, TVector& center) noexcept;

void surfaceSetAdjoinParams(ProjectType projectType, Surface& surface) noexcept;
void surfaceClearAdjoinParams(Surface& surface) noexcept;
// Copies all five parameters atomically; allocation failure leaves dst unchanged.
void surfaceCopyAdjoinParams(const Surface& src, Surface& dst);

void surfaceCalcDefaultUVNormals(const Sector& sector, const Surface& surface, std::int32_t orgVertex, TVector& un, TVector& vn) noexcept;
bool surfaceCalcUVNormalsFrom(const Sector& sector, const Surface& surface, std::int32_t nv1, TVector& un, TVector& vn) noexcept;
bool surfaceCalcUVNormals(const Sector& sector, const Surface& surface, TVector& un, TVector& vn) noexcept;
bool surfaceAutoAlignUV(double pixelsPerUnit, const Sector& sector, Surface& surface) noexcept;
void surfaceArrangeTextureBy(
    double pixelsPerUnit,
    const Sector& sector,
    Surface& surface,
    const TVector& un,
    const TVector& vn,
    double refx,
    double refy,
    double refz,
    float refu,
    float refv) noexcept;
void surfaceArrangeTexture(
    double pixelsPerUnit,
    const Sector& sector,
    Surface& surface,
    std::int32_t orgVertex,
    const TVector& un,
    const TVector& vn) noexcept;
void surfaceCalcUV(double pixelsPerUnit, const Sector& sector, Surface& surface, std::int32_t orgVertex) noexcept;
bool surfaceFindUVScales(double pixelsPerUnit, const Sector& sector, Surface& surface) noexcept;
void surfaceCheckIfFloor(Surface& surface) noexcept;
void surfaceRecalcAll(double pixelsPerUnit, Sector& sector, Surface& surface) noexcept;
void surfaceRecalcNewAll(double pixelsPerUnit, Sector& sector, Surface& surface, std::int32_t orgVertex) noexcept;

bool thingCalcPoseOnSurface(
    const Thing& thing,
    const Sector& sector,
    const Surface& surface,
    TVector& position,
    double& pitch,
    double& yaw,
    double& roll) noexcept;

double levelPixelPerUnit(const Level& level) noexcept;
bool levelStitchSurfaces(Level& level, SurfaceRef source, SurfaceRef target) noexcept;
bool levelFlattenSurface(Level& level, SurfaceRef surface) noexcept;
bool levelGetNearestWorldVertexToLine(
    const Level& level,
    double lx,
    double ly,
    double lz,
    const TVector& lineDirection,
    double radius,
    TVector& vertex) noexcept;

struct SurfacePickHit
{
    SurfaceRef surface{};
    TVector position{};
    double distance{};
};

/**
 * Reusable broad phase for repeated ray picks against an unchanged Level.
 *
 * The index stores only surface bounds and references; final plane and polygon
 * tests still use the same exact picker as levelPickSurfaces(). Call rebuild()
 * after level geometry changes.
 */
class SurfacePickIndex
{
public:
    void rebuild(const Level& level);
    void rebuild(
        const Level& level,
        const std::function<bool(std::int32_t)>& includeSector);

    [[nodiscard]] std::vector<SurfacePickHit> pickSurfaces(
        const Level& level,
        const TVector& rayOrigin,
        const TVector& rayDirection,
        double maxDistance) const noexcept;

    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
    [[nodiscard]] std::size_t surfaceCount() const noexcept { return entries_.size(); }
    [[nodiscard]] std::size_t lastCandidateCount() const noexcept { return lastCandidateCount_; }

private:
    struct Entry
    {
        SurfaceRef surface{};
        TBox bounds{};
        TVector center{};
        bool alwaysTest{};
    };

    struct Node
    {
        TBox bounds{};
        std::uint32_t first{};
        std::uint32_t count{};
        std::uint32_t left{std::numeric_limits<std::uint32_t>::max()};
        std::uint32_t right{std::numeric_limits<std::uint32_t>::max()};
    };

    std::uint32_t buildNode(std::uint32_t first, std::uint32_t count);

    std::vector<Entry> entries_{};
    std::vector<std::uint32_t> alwaysTestEntries_{};
    std::vector<std::uint32_t> order_{};
    std::vector<Node> nodes_{};
    mutable std::size_t lastCandidateCount_{};
};

/**
 * Reusable all-sector broad phase for Thing/camera containment queries.
 *
 * Layer visibility is intentionally not part of this index: hiding authoring
 * geometry must not change the logical Sector assigned to a Thing. Exact
 * containment still uses sectorContainsPoint(), preserving legacy behavior.
 */
class SectorContainmentIndex
{
public:
    void rebuild(const Level& level);

    [[nodiscard]] std::int32_t findContainingSector(
        const Level& level,
        double x,
        double y,
        double z,
        std::int32_t preferredSector = -1) const noexcept;

    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
    [[nodiscard]] std::size_t sectorCount() const noexcept { return entries_.size(); }
    [[nodiscard]] bool matchesLevel(const Level& level) const noexcept
    {
        // A sector count alone is not a cache identity.  Two different level
        // instances can have the same number of sectors while their geometry
        // (and therefore containment bounds) is completely different.  Keep
        // the source pointer so callers never use an index built for another
        // document during staged level/recovery restores.
        return sourceLevel_ == &level
            && sourceSectorCount_ == level.sectors.size();
    }
    [[nodiscard]] std::size_t lastCandidateCount() const noexcept { return lastCandidateCount_; }

private:
    struct Entry
    {
        std::int32_t sector{-1};
        TBox bounds{};
        TVector center{};
    };

    struct Node
    {
        TBox bounds{};
        std::uint32_t first{};
        std::uint32_t count{};
        std::uint32_t left{std::numeric_limits<std::uint32_t>::max()};
        std::uint32_t right{std::numeric_limits<std::uint32_t>::max()};
    };

    std::uint32_t buildNode(std::uint32_t first, std::uint32_t count);

    std::vector<Entry> entries_{};
    std::vector<std::uint32_t> order_{};
    std::vector<Node> nodes_{};
    const Level* sourceLevel_{};
    std::size_t sourceSectorCount_{};
    mutable std::size_t lastCandidateCount_{};
};

std::vector<SurfacePickHit> levelPickSurfaces(
    const Level& level,
    const TVector& rayOrigin,
    const TVector& rayDirection,
    double maxDistance) noexcept;

std::optional<SurfacePickHit> levelPickSurface(
    const Level& level,
    const TVector& rayOrigin,
    const TVector& rayDirection,
    double maxDistance) noexcept;

bool levelSysAdjoinSurfaces(Level& level, SurfaceRef surf1, SurfaceRef surf2, bool reset = true) noexcept;
bool levelMakeAdjoin(Level& level, SurfaceRef surface, bool reset = true) noexcept;
bool levelMakeAdjoinFromSector(Level& level, SurfaceRef surface, std::int32_t startSector, bool reset = true) noexcept;
bool levelUnadjoinSurface(Level& level, SurfaceRef surface) noexcept;
bool levelUnadjoinSectors(Level& level, std::int32_t sector, std::int32_t otherSector) noexcept;
bool levelAdjoinSectors(Level& level, std::int32_t sector, std::int32_t otherSector, bool reset = true) noexcept;
std::int32_t levelFindSectorForXYZ(const Level& level, double x, double y, double z) noexcept;
std::int32_t levelFindSectorForThing(const Level& level, const Thing& thing) noexcept;

} // namespace sed::level
