#include "test_common.hpp"

#include "sed/level/geometry.hpp"
#include "sed/level/merge.hpp"
#include "sed/assets/sed.hpp"
#include "sed/editor/document.hpp"

#include <array>
#include <cmath>
#include <map>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

namespace sed::tests
{

namespace
{

bool surfaceHasVertex(const sed::Sector& sector, const sed::Surface& surface, const double x, const double y, const double z)
{
    for (const std::int32_t vi : surface.vertices)
    {
        if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
        {
            continue;
        }

        const sed::Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
        if (close(v.x, x) && close(v.y, y) && close(v.z, z))
        {
            return true;
        }
    }

    return false;
}

bool sameTextureVertex(
    const sed::TexVertex& lhs,
    const sed::TexVertex& rhs) noexcept
{
    return lhs.u == rhs.u
        && lhs.v == rhs.v
        && lhs.color == rhs.color;
}

std::int32_t addFlatQuad(
    sed::Sector& sector,
    const double x,
    const bool reverse = false)
{
    const std::int32_t a = sector.addVertex(x, 0.0, 0.0);
    const std::int32_t b = sector.addVertex(x + 1.0, 0.0, 0.0);
    const std::int32_t c = sector.addVertex(x + 1.0, 1.0, 0.0);
    const std::int32_t d = sector.addVertex(x, 1.0, 0.0);
    sed::Surface surface{};
    surface.vertices = reverse
        ? std::vector<std::int32_t>{ d, c, b, a }
        : std::vector<std::int32_t>{ a, b, c, d };
    surface.texVertices.resize(4);
    surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);
    return sector.addSurface(std::move(surface));
}

sed::Level makeSurfaceUvMergeLevel(
    const sed::UvCoordinateSpace coordinateSpace,
    const float uDimension = 1.0f,
    const float vDimension = 1.0f)
{
    sed::Level level{};
    level.ppunit = 1.0;
    level.uvCoordinateSpace = coordinateSpace;
    sed::Sector& sector = level.newSector();

    const std::int32_t a = sector.newVertex(0.0, 0.0, 0.0);
    const std::int32_t b = sector.newVertex(1.0, 0.0, 0.0);
    const std::int32_t c = sector.newVertex(1.0, 1.0, 0.0);
    const std::int32_t d = sector.newVertex(0.0, 1.0, 0.0);
    const std::int32_t e = sector.newVertex(2.0, 0.0, 0.0);
    const std::int32_t f = sector.newVertex(2.0, 1.0, 0.0);

    sed::Surface first{};
    first.vertices = { a, b, c, d };
    first.texVertices = {
        { -0.5f * uDimension, 1.25f * vDimension, sed::ColorRed },
        { -0.25f * uDimension, 1.75f * vDimension, sed::ColorGreen },
        { -0.375f * uDimension, 2.5f * vDimension, sed::ColorBlue },
        { -0.625f * uDimension, 2.0f * vDimension, sed::ColorWhite },
    };
    first.uScale = -0.03125f * uDimension;
    first.vScale = 0.09375f * vDimension;
    first.normal = sed::TVector::up;
    (void)sector.addSurface(std::move(first));

    sed::Surface second{};
    second.vertices = { b, e, f, c };
    second.texVertices = {
        { 7.0f * uDimension, -11.0f * vDimension, sed::ColorRed },
        { 13.0f * uDimension, 17.0f * vDimension, sed::ColorGreen },
        { -19.0f * uDimension, 23.0f * vDimension, sed::ColorBlue },
        { 29.0f * uDimension, -31.0f * vDimension, sed::ColorWhite },
    };
    second.uScale = 0.125f * uDimension;
    second.vScale = -0.25f * vDimension;
    second.normal = sed::TVector::up;
    (void)sector.addSurface(std::move(second));

    return level;
}

const sed::TexVertex* textureVertexAtPosition(
    const sed::Sector& sector,
    const sed::Surface& surface,
    const sed::Vertex& position)
{
    for (std::size_t index = 0; index < surface.vertices.size(); ++index)
    {
        const std::int32_t vertexIndex = surface.vertices[index];
        if (vertexIndex < 0
            || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size()
            || index >= surface.texVertices.size())
        {
            continue;
        }
        const sed::Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
        if (vertex.x == position.x
            && vertex.y == position.y
            && vertex.z == position.z)
        {
            return &surface.texVertices[index];
        }
    }
    return nullptr;
}

sed::Level makeSectorUvMergeLevel()
{
    sed::Level level{};
    level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
    (void)level.newSector();
    (void)level.newSector();
    sed::Sector& keep = level.sectors[0];
    sed::Sector& donor = level.sectors[1];

    const std::int32_t kA = keep.newVertex(0.0, 0.0, 0.0);
    const std::int32_t kB = keep.newVertex(1.0, 0.0, 0.0);
    const std::int32_t kC = keep.newVertex(1.0, 1.0, 0.0);
    const std::int32_t kD = keep.newVertex(0.0, 1.0, 0.0);
    sed::Surface keepPortal{};
    keepPortal.vertices = { kA, kB, kC, kD };
    keepPortal.texVertices.resize(keepPortal.vertices.size());
    keepPortal.normal = sed::level::surfaceCalcNormalNewell(keep, keepPortal);
    (void)keep.addSurface(std::move(keepPortal));

    const std::int32_t dA = donor.newVertex(0.0, 0.0, 0.0);
    const std::int32_t dB = donor.newVertex(1.0, 0.0, 0.0);
    const std::int32_t dC = donor.newVertex(1.0, 1.0, 0.0);
    const std::int32_t dD = donor.newVertex(0.0, 1.0, 0.0);
    sed::Surface donorPortal{};
    donorPortal.vertices = { dA, dD, dC, dB };
    donorPortal.texVertices.resize(donorPortal.vertices.size());
    donorPortal.normal = sed::level::surfaceCalcNormalNewell(donor, donorPortal);
    (void)donor.addSurface(std::move(donorPortal));

    const std::int32_t dE = donor.newVertex(0.0, 0.0, 1.0);
    const std::int32_t dF = donor.newVertex(1.0, 0.0, 1.0);
    const std::int32_t dG = donor.newVertex(1.0, 1.0, 1.0);
    const std::int32_t dH = donor.newVertex(0.0, 1.0, 1.0);
    sed::Surface donorTop{};
    donorTop.vertices = { dE, dH, dG, dF };
    donorTop.texVertices = {
        { -0.25f, 0.5f, sed::ColorRed },
        { 0.75f, -1.0f, sed::ColorGreen },
        { 1.25f, 1.5f, sed::ColorBlue },
        { -1.75f, 2.0f, sed::ColorWhite },
    };
    donorTop.uScale = -0.03125f;
    donorTop.vScale = 0.09375f;
    donorTop.normal = sed::level::surfaceCalcNormalNewell(donor, donorTop);
    (void)donor.addSurface(std::move(donorTop));

    (void)sed::level::levelSysAdjoinSurfaces(
        level, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 1, 0 });
    return level;
}

namespace roundedMergeContracts
{
using namespace sed;
void outcome(const char* label, const bool accepted, const bool expected)
{
    check(accepted == expected, label);
}
TVector point(const Vertex& p)
{
    return {p.x, p.y, p.z};
}
TVector cross(TVector a, TVector b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double dot(TVector a, TVector b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
bool near(double a, double b)
{
    return std::isfinite(a) && std::abs(a - b) <= 1e-10;
}
void recalc(Sector& sector)
{
    for ( std::size_t index = 0; index < sector.surfaces.size(); ++index )
    {
        auto& face = sector.surfaces[index];
        face.num = static_cast<int>(index);
        TVector area{};
        const auto a = point(sector.vertices[face.vertices[0]]);
        for ( std::size_t i = 1; i + 1 < face.vertices.size(); ++i )
        {
            const auto c = cross(point(sector.vertices[face.vertices[i]]) - a,
                                 point(sector.vertices[face.vertices[i + 1]]) - a);
            area = area + c;
        }
        const double length = std::sqrt(dot(area, area));
        face.normal = area * (1.0 / length);
        face.texVertices.resize(face.vertices.size());
        for ( std::size_t i = 0; i < face.vertices.size(); ++i )
        {
            const auto& p = sector.vertices[face.vertices[i]];
            face.texVertices[i].u = static_cast<float>(p.x + 2 * p.y);
            face.texVertices[i].v = static_cast<float>(p.z + p.y);
        }
    }
}
Sector box(double x0, double x1, double y0, double y1, double z0, double z1)
{
    Sector sector;
    const std::array<TVector, 8> points{{{x0, y0, z0},
                                         {x1, y0, z0},
                                         {x1, y1, z0},
                                         {x0, y1, z0},
                                         {x0, y0, z1},
                                         {x1, y0, z1},
                                         {x1, y1, z1},
                                         {x0, y1, z1}}};
    for ( const auto p : points )
    {
        sector.addVertex(p.x, p.y, p.z);
    }
    const std::array<std::array<int, 4>, 6> faces{
        {{{1, 5, 6, 2}}, {{0, 3, 7, 4}}, {{3, 2, 6, 7}}, {{0, 4, 5, 1}}, {{4, 7, 6, 5}}, {{0, 1, 2, 3}}}};
    for ( const auto& indices : faces )
    {
        Surface face;
        face.vertices = std::vector<std::int32_t>(indices.begin(), indices.end());
        sector.surfaces.push_back(std::move(face));
    }
    recalc(sector);
    return sector;
}
void append(Level& level, Sector sector)
{
    sector.num = static_cast<int>(level.sectors.size());
    sector.id = ++level.curId;
    level.sectors.push_back(std::move(sector));
}
void link(Level& level, SurfaceRef a, SurfaceRef b)
{
    level.sectors[a.sector].surfaces[a.surface].adjoin = b;
    level.sectors[b.sector].surfaces[b.surface].adjoin = a;
    level.sectors[a.sector].surfaces[a.surface].nAdj = -1;
    level.sectors[b.sector].surfaces[b.surface].nAdj = -1;
}
Level adjacent(double internalWarp = 0, double retainedWarp = 0, double gap = 0)
{
    Level level;
    append(level, box(-1, 1, -1, 1, -1, 1));
    append(level, box(1 + gap, 3 + gap, -1, 1, -1, 1));
    level.sectors[0].vertices[6].x += internalWarp;
    level.sectors[1].vertices[7].x += internalWarp;
    // A different, far exterior corner: its x=-1 face becomes rounded.
    level.sectors[0].vertices[7].x -= retainedWarp;
    for ( auto& sector : level.sectors )
    {
        recalc(sector);
    }
    auto& mirror = level.sectors[1].surfaces[1];
    std::rotate(mirror.vertices.begin(), mirror.vertices.begin() + 1, mirror.vertices.end());
    std::rotate(mirror.texVertices.begin(), mirror.texVertices.begin() + 1, mirror.texVertices.end());
    return level;
}
bool validFace(const Sector& sector, const Surface& face)
{
    bool valid = face.vertices.size() >= 3 && face.texVertices.size() == face.vertices.size();
    for ( const auto i : face.vertices )
    {
        valid = valid && i >= 0 && static_cast<std::size_t>(i) < sector.vertices.size();
    }
    check(valid, "Output face cardinality and indices are valid");
    return valid;
}
std::vector<TVector> coordinates(const Sector& sector, const Surface& face)
{
    std::vector<TVector> result;
    if ( !validFace(sector, face) )
    {
        return result;
    }
    for ( const auto i : face.vertices )
    {
        result.push_back(point(sector.vertices[i]));
    }
    return result;
}
void removeExactlyCollinearCorners(std::vector<TVector>& polygon)
{
    bool changed = true;
    while ( changed && polygon.size() > 3 )
    {
        changed = false;
        for ( std::size_t index = 0; index < polygon.size(); ++index )
        {
            const auto u = polygon[index] - polygon[(index + polygon.size() - 1) % polygon.size()];
            const auto v = polygon[(index + 1) % polygon.size()] - polygon[index];
            const auto turn = cross(u, v);
            if ( turn.x == 0 && turn.y == 0 && turn.z == 0 && dot(u, v) > 0 )
            {
                polygon.erase(polygon.begin() + static_cast<std::ptrdiff_t>(index));
                changed = true;
                break;
            }
        }
    }
}
bool reverseEqual(std::vector<TVector> a, std::vector<TVector> b)
{
    // Portal geometry permits extra collinear edge points on either side.
    removeExactlyCollinearCorners(a);
    removeExactlyCollinearCorners(b);
    if ( a.size() < 3 || a.size() != b.size() )
    {
        return false;
    }
    for ( std::size_t start = 0; start < b.size(); ++start )
    {
        bool equal = true;
        for ( std::size_t i = 0; i < a.size(); ++i )
        {
            equal = equal && a[i] == b[(start + b.size() - i) % b.size()];
        }
        if ( equal )
        {
            return true;
        }
    }
    return false;
}
int verifyPortals(const Level& level)
{
    int count = 0;
    for ( std::size_t s = 0; s < level.sectors.size(); ++s )
    {
        for ( std::size_t f = 0; f < level.sectors[s].surfaces.size(); ++f )
        {
            const auto& face = level.sectors[s].surfaces[f];
            if ( !face.adjoin )
            {
                continue;
            }
            ++count;
            const auto ref = *face.adjoin;
            const bool valid =
                ref.sector >= 0 && static_cast<std::size_t>(ref.sector) < level.sectors.size() &&
                ref.surface >= 0 &&
                static_cast<std::size_t>(ref.surface) < level.sectors[ref.sector].surfaces.size();
            check(valid, "Portal target exists");
            if ( !valid )
            {
                continue;
            }
            const auto& other = level.sectors[ref.sector];
            const auto& mirror = other.surfaces[ref.surface];
            check(mirror.adjoin == std::optional<SurfaceRef>{{static_cast<int>(s), static_cast<int>(f)}},
                  "Portal has the exact reciprocal target");
            check(reverseEqual(coordinates(level.sectors[s], face), coordinates(other, mirror)),
                  "Portal boundaries are exact reversed cyclic coordinates");
        }
    }
    return count;
}
double verifySolid(const Sector& sector)
{
    double signedVolume = 0;
    std::map<std::pair<int, int>, int> edges;
    for ( const auto& v : sector.vertices )
    {
        check(std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z), "Output vertices are finite");
    }
    check(!sector.surfaces.empty(), "Output solid has a boundary");
    for ( const auto& face : sector.surfaces )
    {
        if ( !validFace(sector, face) )
        {
            continue;
        }
        const auto a = point(sector.vertices[face.vertices[0]]);
        for ( std::size_t i = 0; i < face.vertices.size(); ++i )
        {
            ++edges[{face.vertices[i], face.vertices[(i + 1) % face.vertices.size()]}];
            if ( i > 0 && i + 1 < face.vertices.size() )
            {
                signedVolume += dot(a, cross(point(sector.vertices[face.vertices[i]]) - a,
                                             point(sector.vertices[face.vertices[i + 1]]) - a)) /
                                6;
            }
        }
    }
    for ( const auto& [edge, count] : edges )
    {
        check(count == 1 && edges.contains({edge.second, edge.first}) &&
                  edges.at({edge.second, edge.first}) == 1,
              "Each solid edge has exactly two opposite incidences");
    }
    check(std::isfinite(signedVolume) && signedVolume < 0,
          "Output solid has positive inward-oriented enclosed volume");
    return -signedVolume;
}
bool hasPoint(const Level& level, TVector expected)
{
    for ( const auto& sector : level.sectors )
    {
        for ( const auto& vertex : sector.vertices )
        {
            if ( point(vertex) == expected )
            {
                return true;
            }
        }
    }
    return false;
}
void sectorMergeCases()
{
    for ( const auto scenario :
          std::array<std::array<double, 2>, 3>{{{{0, 0}}, {{0.00005, 0}}, {{0, 0.00005}}}} )
    {
        Level level = adjacent(scenario[0], scenario[1]);
        link(level, {0, 0}, {1, 1});
        const Level before = level;
        const auto result = level::levelMergeSectors(level, 0, 1);
        Level batch = before;
        const std::array<std::int32_t, 2> selected{0, 1};
        const auto batchResult = level::levelMergeSelectedSectors(batch, selected);
        check(batchResult && batchResult->mergeCount == 1 &&
                  batchResult->selection == std::vector<std::int32_t>{0},
              "Rounded Sector Merge batch returns exact count and complete selection");
        check(assets::sedLevelsEqual(batch, level),
              "Rounded Sector Merge batch matches the independently checked single merge");
        outcome(scenario[0] != 0   ? "Sector Merge removes rounded internal portal"
                : scenario[1] != 0 ? "Sector Merge retains rounded exterior"
                                   : "Sector Merge strict",
                result.has_value(), true);
        if ( !result )
        {
            check(assets::sedLevelsEqual(level, before), "Failed Sector Merge preserves the complete Level");
            continue;
        }
        check(*result == 0 && level.sectors.size() == 1,
              "Sector Merge returns kept index0 and exactly one sector");
        if ( level.sectors.size() != 1 )
        {
            continue;
        }
        check(verifyPortals(level) == 0, "Sector Merge removes exactly the internal portal");
        const double volume = verifySolid(level.sectors[0]);
        check(std::abs(volume - 16) <= scenario[1] * 4 + 1e-10,
              "Merged volume equals analytic box16 within retained warp envelope");
        if ( scenario[1] != 0 )
        {
            check(hasPoint(level, {-1.00005, 1, 1}), "Merge retains exact exterior rounding");
        }
        for ( const auto& face : level.sectors[0].surfaces )
        {
            const auto polygon = coordinates(level.sectors[0], face);
            if ( polygon.empty() )
            {
                continue;
            }
            bool x = true, y = true, z = true;
            for ( const auto p : polygon )
            {
                x = x && near(p.x, polygon[0].x);
                y = y && near(p.y, polygon[0].y);
                z = z && near(p.z, polygon[0].z);
                check(p.x >= -1 - scenario[1] && p.x <= 3 && p.y >= -1 && p.y <= 1 && p.z >= -1 && p.z <= 1,
                      "Merged coordinates remain in the fixed exterior bounds");
            }
            if ( scenario[1] == 0 )
            {
                check(x || y || z, "Removing warped internal portal leaves exact exterior planes");
            }
        }
    }
    {
        Level chain;
        for ( int owner = 0; owner < 4; ++owner )
        {
            append(chain, box(2 * owner - 1, 2 * owner + 1, -1, 1, -1, 1));
        }
        chain.sectors[0].vertices[7].x -= 0.00005;
        recalc(chain.sectors[0]);
        for ( int owner = 0; owner < 3; ++owner )
        {
            link(chain, {owner, 0}, {owner + 1, 1});
        }
        // Every step captures at least two original owners and moves each
        // geometry profile from its return temporary into transaction storage.
        // ASan use-after-scope catches a cached SurfacePlaneError borrowing the
        // temporary Sector instead of the stable stored profile.
        const std::array<std::int32_t, 4> selected{0, 1, 2, 3};
        const auto result = level::levelMergeSelectedSectors(chain, selected);
        check(result && result->mergeCount == 3 && result->selection == std::vector<std::int32_t>{0},
            "Repeated multi-owner profile transfers preserve the exact batch result");
        check(chain.sectors.size() == 1 && hasPoint(chain, {-1.00005, 1, 1}),
            "Repeated multi-owner merges retain exact far exterior rounding");
        if ( chain.sectors.size() == 1 )
        {
            check(std::abs(verifySolid(chain.sectors[0]) - 32) <= 0.0002 + 1e-10,
                "Four-box chain conserves analytic volume32 within its single inherited face envelope");
            check(verifyPortals(chain) == 0, "Four-box chain removes all three internal portal pairs");
        }
    }
    for ( const double protrusion : {0.00000001, 0.0005} )
    {
        Level outside = adjacent(0, 0.00005);
        for ( auto& vertex : outside.sectors[1].vertices )
        {
            if ( vertex.x == 3 && vertex.y == 1 )
            {
                vertex.y += protrusion;
            }
        }
        recalc(outside.sectors[1]);
        link(outside, {0, 0}, {1, 1});
        const Level before = outside;
        check(!level::levelMergeSectors(outside, 0, 1),
              "Inherited rounding never licenses a new foreign vertex outside a supporting plane");
        check(assets::sedLevelsEqual(outside, before),
              "New foreign-support violation preserves the complete Level");
    }
    Level invalid = adjacent(0, 0.01);
    link(invalid, {0, 0}, {1, 1});
    const Level invalidBefore = invalid;
    check(!level::levelMergeSectors(invalid, 0, 1),
          "Sector Merge rejects source rounding outside the bounded admission contract");
    check(assets::sedLevelsEqual(invalid, invalidBefore),
          "Invalid source Sector Merge preserves the complete Level");
    Level reflex;
    auto a = box(-1, 1, -1, 1, -1, 1);
    a.addVertex(1, 0, -1);
    a.addVertex(1, 0, 1);
    a.surfaces[0].vertices = std::vector<std::int32_t>{1, 5, 9, 8};
    Surface upper;
    upper.vertices = std::vector<std::int32_t>{8, 9, 6, 2};
    a.surfaces.push_back(upper);
    a.surfaces[4].vertices = std::vector<std::int32_t>{4, 7, 6, 9, 5};
    a.surfaces[5].vertices = std::vector<std::int32_t>{0, 1, 8, 2, 3};
    recalc(a);
    append(reflex, std::move(a));
    append(reflex, box(1, 3, -1, 0, -1, 1));
    link(reflex, {0, 0}, {1, 1});
    const Level before = reflex;
    outcome("Sector Merge genuinely reflex union", level::levelMergeSectors(reflex, 0, 1).has_value(), false);
    check(assets::sedLevelsEqual(reflex, before), "Reflex merge rejection preserves the complete Level");
}
void surfaceMergeCases()
{
    for ( int scenario = 0; scenario < 3; ++scenario )
    {
        Level level;
        auto sector = box(-1, 1, -1, 1, -1, 1);
        sector.surfaces[5].vertices = std::vector<std::int32_t>{0, 1, 2};
        Surface second;
        second.vertices = std::vector<std::int32_t>{0, 2, 3};
        sector.surfaces.push_back(second);
        if ( scenario == 1 )
        {
            sector.vertices[6].z += 0.00005;
        }
        if ( scenario == 2 )
        {
            sector.vertices[2].z -= 0.00005;
        }
        recalc(sector);
        append(level, std::move(sector));
        const Level before = level;
        const auto result = level::levelMergeSurfaces(level, {0, 5}, {0, 6});
        Level batch = before;
        const std::array<SurfaceRef, 2> selected{{{0, 5}, {0, 6}}};
        const auto batchResult = level::levelMergeSelectedSurfaces(batch, selected);
        check(batchResult.has_value() == (scenario != 2),
              "Rounded Surface Merge batch follows the independent positive and negative contracts");
        if ( batchResult )
        {
            check(batchResult->mergeCount == 1 && batchResult->selection == std::vector<SurfaceRef>{{0, 5}},
                  "Rounded Surface Merge batch returns exact count and complete selection");
        }
        check(assets::sedLevelsEqual(batch, level),
              "Rounded Surface Merge batch matches the independently checked single merge");
        outcome(scenario == 0   ? "Surface Merge strict"
                : scenario == 1 ? "Surface Merge retains unrelated rounding"
                                : "Surface Merge new nonplanar polygon",
                result.has_value(), scenario != 2);
        if ( !result )
        {
            check(assets::sedLevelsEqual(level, before),
                  "Rejected Surface Merge preserves the complete Level");
            continue;
        }
        check(*result == SurfaceRef{0, 5} && level.sectors.size() == 1 &&
                  level.sectors[0].surfaces.size() == 6,
              "Surface Merge returns exact survivor and face cardinality");
        if ( level.sectors.size() != 1 || level.sectors[0].surfaces.size() != 6 )
        {
            continue;
        }
        const auto& out = level.sectors[0];
        check(out.vertices.size() == 8, "Surface Merge retains all eight geometric vertices");
        for ( const auto& vertex : before.sectors[0].vertices )
        {
            check(hasPoint(level, point(vertex)), "Surface Merge preserves each exact input coordinate");
        }
        for ( std::size_t face = 0; face < 5; ++face )
        {
            check(coordinates(out, out.surfaces[face]) ==
                      coordinates(before.sectors[0], before.sectors[0].surfaces[face]),
                  "Surface Merge preserves every unrelated face boundary exactly");
        }
        const auto merged = coordinates(out, out.surfaces[5]);
        const std::array<TVector, 4> expected{{{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}}};
        check(merged.size() == 4, "Merged bottom has exactly four corners");
        for ( const auto p : expected )
        {
            check(std::count(merged.begin(), merged.end(), p) == 1,
                  "Merged bottom has each fixed analytic corner exactly once");
        }
        verifySolid(out);
        if ( scenario == 1 )
        {
            editor::DocumentUndoHistory history;
            history.current().level = before;
            check(
                history.apply("Rounded surface merge", [](auto& state)
                              { return level::levelMergeSurfaces(state.level, {0, 5}, {0, 6}).has_value(); }),
                "Rounded Surface Merge commits through document history");
            check(assets::sedLevelsEqual(history.current().level, level),
                  "History commit matches the complete independently checked merge result");
            check(history.undo() && assets::sedLevelsEqual(history.current().level, before),
                  "Rounded Surface Merge Undo restores every authored field");
            check(history.redo() && assets::sedLevelsEqual(history.current().level, level),
                  "Rounded Surface Merge Redo restores every authored field");
        }
    }
}
void sourceNormalCases()
{
    for ( const bool rounded : {false, true} )
    {
        for ( int normalCase = 0; normalCase < 4; ++normalCase )
        {
            for ( const bool poisonSelected : {false, true} )
            {
                for ( int operation = 0; operation < 5; ++operation )
                {
                    Level input;
                    const bool surfaceOperation = operation < 3;
                    if ( surfaceOperation )
                    {
                        auto sector = box(-1, 1, -1, 1, -1, 1);
                        sector.surfaces[5].vertices = std::vector<std::int32_t>{0, 1, 2};
                        Surface second;
                        second.vertices = std::vector<std::int32_t>{0, 2, 3};
                        sector.surfaces.push_back(second);
                        if ( rounded )
                        {
                            sector.vertices[6].z += 0.00005;
                        }
                        recalc(sector);
                        append(input, std::move(sector));
                    }
                    else
                    {
                        input = adjacent(0, rounded ? 0.00005 : 0.0);
                        link(input, {0, 0}, {1, 1});
                    }
                    const std::size_t poisonedFace = poisonSelected ? (surfaceOperation ? 5 : 0) : 1;
                    auto& normal = input.sectors[0].surfaces[poisonedFace].normal;
                    if ( normalCase == 0 )
                    {
                        normal = {};
                    }
                    else if ( normalCase == 1 )
                    {
                        normal.x = std::numeric_limits<double>::quiet_NaN();
                    }
                    else if ( normalCase == 2 )
                    {
                        normal = -normal;
                    }
                    else
                    {
                        normal = TVector{std::round(normal.x * 1000) / 1000,
                            std::round(normal.y * 1000) / 1000, std::round(normal.z * 1000) / 1000} * 1e200;
                    }
                    const Level before = input;
                    bool accepted = false;
                    if ( operation == 0 )
                    {
                        const auto result = level::levelMergeSurfaces(input, {0, 5}, {0, 6});
                        accepted = result.has_value();
                        if ( result )
                        {
                            check(*result == SurfaceRef{0, 5}, "Merge normal contract returns exact Surface survivor");
                        }
                    }
                    else if ( operation == 1 )
                    {
                        const std::array<SurfaceRef, 2> selected{{{0, 5}, {0, 6}}};
                        const auto result = level::levelMergeSelectedSurfaces(input, selected);
                        accepted = result.has_value();
                        if ( result )
                        {
                            check(result->mergeCount == 1 && result->selection == std::vector<SurfaceRef>{{0, 5}},
                                "Merge normal contract returns exact Surface batch outputs");
                        }
                    }
                    else if ( operation == 2 )
                    {
                        accepted = level::levelMergeSurfacesAt(input, {0, 5}, 2);
                    }
                    else if ( operation == 3 )
                    {
                        const auto result = level::levelMergeSectors(input, 0, 1);
                        accepted = result.has_value();
                        if ( result )
                        {
                            check(*result == 0, "Merge normal contract returns exact Sector survivor");
                        }
                    }
                    else
                    {
                        const std::array<std::int32_t, 2> selected{0, 1};
                        const auto result = level::levelMergeSelectedSectors(input, selected);
                        accepted = result.has_value();
                        if ( result )
                        {
                            check(result->mergeCount == 1 && result->selection == std::vector<std::int32_t>{0},
                                "Merge normal contract returns exact Sector batch outputs");
                        }
                    }
                    check(accepted == (normalCase == 0 || normalCase == 3),
                        "All Merge entrypoints accept unset/same-facing quantized caches and reject NaN/opposed normals");
                    if ( (normalCase != 0 && normalCase != 3) || !accepted )
                    {
                        check(assets::sedLevelsEqual(input, before),
                            "Malformed source-normal rejection preserves every authored and runtime field");
                        continue;
                    }
                    check(input.sectors.size() == 1, "Unset-normal merge publishes exactly one output Sector");
                    if ( input.sectors.size() != 1 )
                    {
                        continue;
                    }
                    const double expectedVolume = surfaceOperation ? 8 : 16;
                    check(std::abs(verifySolid(input.sectors[0]) - expectedVolume) <=
                        (rounded ? 0.0002 : 0.0) + 1e-10,
                        "Unset-normal merge preserves independently bounded analytic volume");
                    check(verifyPortals(input) == 0, "Unset-normal merge publishes no stale internal portal");
                    if ( !poisonSelected )
                    {
                        const auto expectedBoundary = coordinates(before.sectors[0], before.sectors[0].surfaces[1]);
                        bool preserved = false;
                        for ( const auto& face : input.sectors[0].surfaces )
                        {
                            if ( coordinates(input.sectors[0], face) == expectedBoundary )
                            {
                                preserved = face.normal == before.sectors[0].surfaces[1].normal;
                            }
                        }
                        check(preserved, "Untouched accepted cache and complete boundary retain their exact source values");
                    }
                }
            }
        }
    }
}
} // namespace roundedMergeContracts

} // namespace

void runLevelMergeTests()
{
    roundedMergeContracts::sectorMergeCases();
    roundedMergeContracts::surfaceMergeCases();
    roundedMergeContracts::sourceNormalCases();
    for ( const auto space : {sed::UvCoordinateSpace::Normalized, sed::UvCoordinateSpace::TexturePixels} )
    {
        for ( const auto dimensions : {std::pair{100.0, 0.00001}, std::pair{1000000.0, 0.01}} )
        {
            const auto [length, width] = dimensions;
            const double uDimension = space == sed::UvCoordinateSpace::TexturePixels ? 512.0 : 1.0;
            const double vDimension = space == sed::UvCoordinateSpace::TexturePixels ? 128.0 : 1.0;
            // Each portal side has its own affine mapping. The oracle is
            // analytic, independent of merge's projection capture/solve.
            const auto expectedUv = [&](const sed::Vertex& point, const std::size_t side)
            {
                const double x = point.x / length;
                const double y = point.y / width;
                return side == 0
                    ? std::pair{uDimension * (0.25 * x - 0.125 * y - 0.5),
                        vDimension * (0.5 * x + 0.75 * y + 1.25)}
                    : std::pair{uDimension * (-2.0 * x + 0.5 * y + 3.0),
                        vDimension * (1.25 * x - 1.5 * y - 0.25)};
            };
            const auto expectedColor = [&](const sed::Vertex& point, const std::size_t side)
            {
                return sed::ColorF{static_cast<float>(point.x / length * 0.25),
                    static_cast<float>(point.y / width * 0.5), side == 0 ? 0.25f : 0.75f,
                    static_cast<float>(0.25 + point.x / length * 0.125 + point.y / width * 0.125)};
            };
            sed::Level source{};
            source.uvCoordinateSpace = space;
            for ( std::size_t side = 0; side < 2; ++side )
            {
                auto& sector = source.newSector();
                for ( const auto point : {std::pair{0.0, 0.0}, std::pair{length, 0.0},
                        std::pair{length, width}, std::pair{0.0, width},
                        std::pair{2.0 * length, 0.0}, std::pair{2.0 * length, width}} )
                {
                    sector.newVertex(point.first, point.second, 0.0);
                }
                for ( auto indices : {std::vector<std::int32_t>{0, 1, 2},
                        std::vector<std::int32_t>{0, 2, 3},
                        std::vector<std::int32_t>{1, 4, 5},
                        std::vector<std::int32_t>{1, 5, 2}} )
                {
                    if ( side == 1 )
                    {
                        std::reverse(indices.begin(), indices.end());
                    }
                    sed::Surface face{};
                    face.vertices = indices;
                    face.uScale = static_cast<float>(uDimension * (side == 0 ? -0.03125 : 0.125));
                    face.vScale = static_cast<float>(vDimension * (side == 0 ? 0.09375 : -0.25));
                    for ( const auto index : indices )
                    {
                        const auto [u, v] = expectedUv(sector.vertices[static_cast<std::size_t>(index)], side);
                        face.texVertices.push_back({static_cast<float>(u), static_cast<float>(v),
                            expectedColor(sector.vertices[static_cast<std::size_t>(index)], side)});
                    }
                    face.normal = sed::level::surfaceCalcNormalNewell(sector, face);
                    sector.addSurface(std::move(face));
                }
            }
            for ( std::int32_t face = 0; face < 4; ++face )
            {
                check(sed::level::levelSysAdjoinSurfaces(source, {0, face}, {1, face}),
                    "release F5: thin mirrored chain fixture has exact reciprocal portals");
            }
            const auto encode = [](const sed::Level& level)
            {
                return sed::assets::saveToSedBytes(level, {}, sed::assets::SedSerializationProfile::ProtectedV1, 0);
            };
            const auto before = encode(source);
            sed::editor::DocumentUndoHistory history{};
            history.current().level = source;
            const std::array selected{sed::SurfaceRef{0, 0}, sed::SurfaceRef{0, 1},
                sed::SurfaceRef{0, 2}, sed::SurfaceRef{0, 3}};
            const bool committed = history.apply("thin mirrored surface chain", [&](auto& state)
            {
                return sed::level::levelMergeSelectedSurfaces(state.level, selected).has_value();
            });
            check(committed, "release F5: thin mirrored chain commits one batch transaction");
            if ( !committed )
            {
                continue;
            }
            const auto& output = history.current().level;
            check(output.uvCoordinateSpace == space && output.sectors.size() == 2,
                "release F5: mirrored chain retains UV units and both owners");
            for ( std::size_t side = 0; side < std::min(std::size_t{2}, output.sectors.size()); ++side )
            {
                const auto& sector = output.sectors[side];
                check(sector.surfaces.size() == 1,
                    "release F5: chained merge collapses all four faces on both portal sides");
                if ( sector.surfaces.size() != 1 )
                {
                    continue;
                }
                const auto& face = sector.surfaces[0];
                check(face.vertices.size() >= 4 && face.texVertices.size() == face.vertices.size(),
                    "release F5: merged portal retains a complete textured boundary");
                if ( face.texVertices.size() != face.vertices.size() )
                {
                    continue;
                }
                check(face.adjoin && *face.adjoin == sed::SurfaceRef{static_cast<std::int32_t>(1 - side), 0},
                    "release F5: thin mirrored chain keeps reciprocal output portal");
                check(face.uScale == source.sectors[side].surfaces[0].uScale
                        && face.vScale == source.sectors[side].surfaces[0].vScale,
                    "release F5: mirrored chain preserves each active side's authored texture scales");
                long double twiceArea = 0.0L;
                for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
                {
                    const auto& vertex = sector.vertices[static_cast<std::size_t>(face.vertices[corner])];
                    const auto& next = sector.vertices[static_cast<std::size_t>(
                        face.vertices[(corner + 1) % face.vertices.size()])];
                    twiceArea += static_cast<long double>(vertex.x) * next.y
                        - static_cast<long double>(vertex.y) * next.x;
                    const auto [u, v] = expectedUv(vertex, side);
                    check(std::abs(face.texVertices[corner].u - u) <= uDimension * 1.0e-6
                            && std::abs(face.texVertices[corner].v - v) <= vDimension * 1.0e-6,
                        "release F5: every mirrored/chained output corner matches its side's analytic UV");
                    check(face.texVertices[corner].color == expectedColor(vertex, side),
                        "release F5: every mirrored/chained output corner retains its authored side's color");
                }
                check(std::abs(std::abs(twiceArea) - 4.0L * length * width) <= length * width * 1.0e-12L,
                    "release F5: chained portal retains the complete analytic rectangle area");
            }
            const auto after = encode(output);
            check(history.undo() && encode(history.current().level) == before,
                "release F5: mirrored chain Undo restores exact native geometry, UVs, colors, scales and portals");
            check(history.redo() && encode(history.current().level) == after,
                "release F5: mirrored chain Redo restores exact native merged state");
        }
    }
    for ( const std::size_t retainedFaces : { std::size_t{5}, std::size_t{3} } )
    {
        sed::Level level{};
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorWhite));
        level.sectors.push_back(makeLitTestCubeSector(2.0, sed::ColorWhite));
        check(sed::level::levelAdjoinSectors(level, 0, 1, true),
            "release F2: missing-wall fixture starts with exact closed cubes");
        // Keep the portal and only a subset of donor boundary faces. Reciprocal
        // connectivity and inward half-spaces alone still appear valid.
        auto& faces = level.sectors[1].surfaces;
        while ( faces.size() > retainedFaces )
        {
            const auto removable = std::find_if(faces.begin(), faces.end(),
                [](const auto& face) { return !face.adjoin; });
            faces.erase(removable);
        }
        // Erasing lower-index walls changed the portal index.
        for ( std::size_t index = 0; index < faces.size(); ++index )
        {
            if ( faces[index].adjoin )
            {
                const auto back = *faces[index].adjoin;
                level.sectors[back.sector].surfaces[back.surface].adjoin =
                    sed::SurfaceRef{1, static_cast<std::int32_t>(index)};
            }
        }
        const auto before = sed::assets::saveToSedBytes(level, {}, sed::assets::SedSerializationProfile::ProtectedV1, 0);
        check(!sed::level::levelMergeSectors(level, 0, 1),
            "release F2: merge rejects volumetric input with missing boundary walls");
        check(sed::assets::saveToSedBytes(level, {}, sed::assets::SedSerializationProfile::ProtectedV1, 0) == before,
            "release F2: open-solid merge rejection preserves entire input");
    }
    for ( const double scale : { 1.0, 0.01 } )
    {
        sed::Level level{};
        auto& sector = level.newSector();
        for ( const auto point : { std::pair{0.0, 0.0}, std::pair{1.0, 0.0},
                std::pair{0.25, 0.25}, std::pair{0.0, 1.0} } )
        {
            sector.newVertex(point.first * scale, point.second * scale, 0.0);
        }
        for ( const auto indices : { std::vector<std::int32_t>{0, 1, 2},
                std::vector<std::int32_t>{0, 2, 3} } )
        {
            sed::Surface face{};
            face.vertices = indices;
            face.texVertices.resize(3);
            face.normal = sed::level::surfaceCalcNormalNewell(sector, face);
            sector.addSurface(std::move(face));
        }
        const auto before = sed::assets::saveToSedBytes(level, {}, sed::assets::SedSerializationProfile::ProtectedV1, 0);
        // The union has a reflex turn of -0.5 * scale^2 at (.25,.25).
        check(!sed::level::levelMergeSurfaces(level, {0, 0}, {0, 1}),
            "release F1: analytically concave surface union rejects at every scale");
        const std::array selected{ sed::SurfaceRef{0, 0}, sed::SurfaceRef{0, 1} };
        check(!sed::level::levelMergeSelectedSurfaces(level, selected),
            "release F1: batch rejects analytically concave surface union");
        check(sed::assets::saveToSedBytes(level, {}, sed::assets::SedSerializationProfile::ProtectedV1, 0) == before,
            "release F1: rejected surface merge preserves complete native state");
    }
    for ( const double scale : { 1.0, 0.01 } )
    {
        sed::Level level{};
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorWhite));
        level.sectors.push_back(makeLitTestCubeSector(2.0, sed::ColorWhite));
        for ( std::size_t index = 0; index < level.sectors.size(); ++index )
        {
            auto& sector = level.sectors[index];
            for ( auto& vertex : sector.vertices )
            {
                const bool expanded = index == 1 && vertex.x > 2.0 && vertex.y > 0.0;
                vertex.x *= scale;
                vertex.y = vertex.y * scale + (expanded ? 0.0005 : 0.0);
                vertex.z *= scale;
            }
            for ( auto& face : sector.surfaces )
            {
                face.normal = sed::level::surfaceCalcNormalNewell(sector, face);
            }
        }
        check(sed::level::levelAdjoinSectors(level, 0, 1, true),
            "release F1: convex input solids have an exactly coincident portal");
        const auto before = sed::assets::saveToSedBytes(level, {}, sed::assets::SedSerializationProfile::ProtectedV1, 0);
        // The far upper donor corners exceed the first cube's y=scale
        // supporting plane by exactly .0005; each input is a convex prism.
        check(!sed::level::levelMergeSectors(level, 0, 1),
            "release F1: sector union with analytic outside corners rejects");
        const std::array<std::int32_t, 2> selected{0, 1};
        check(!sed::level::levelMergeSelectedSectors(level, selected),
            "release F1: batch rejects concave sector union");
        check(sed::assets::saveToSedBytes(level, {}, sed::assets::SedSerializationProfile::ProtectedV1, 0) == before,
            "release F1: rejected sector merge is failure atomic");
    }
    for ( const auto space : { sed::UvCoordinateSpace::Normalized, sed::UvCoordinateSpace::TexturePixels } )
    {
        for ( const auto dimensions : { std::pair{100.0, 0.00001}, std::pair{1000000.0, 0.01} } )
        {
            const auto [length, width] = dimensions;
            sed::Level level{};
            level.uvCoordinateSpace = space;
            auto& sector = level.newSector();
            sector.newVertex(0.0, 0.0, 0.0);
            sector.newVertex(length, 0.0, 0.0);
            sector.newVertex(length, width, 0.0);
            sector.newVertex(0.0, width, 0.0);
            for ( const auto indices : { std::vector<std::int32_t>{0, 1, 2},
                    std::vector<std::int32_t>{0, 2, 3} } )
            {
                sed::Surface face{};
                face.vertices = indices;
                for ( const auto index : indices )
                {
                    const auto& vertex = sector.vertices[index];
                    face.texVertices.push_back({static_cast<float>(vertex.x / length),
                        static_cast<float>(vertex.y / width), sed::ColorWhite});
                }
                face.normal = sed::level::surfaceCalcNormalNewell(sector, face);
                sector.addSurface(std::move(face));
            }
            const auto merged = sed::level::levelMergeSurfaces(level, {0, 0}, {0, 1});
            check(merged.has_value(), "release F5: thin valid rectangle merge remains admitted");
            if ( merged )
            {
                const auto& output = level.sectors[0];
                const auto& face = output.surfaces[0];
                for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
                {
                    const auto& vertex = output.vertices[face.vertices[corner]];
                    check(std::abs(face.texVertices[corner].u - vertex.x / length) < 1.0e-6
                            && std::abs(face.texVertices[corner].v - vertex.y / width) < 1.0e-6,
                        "release F5: thin rectangle preserves analytic affine UV mapping");
                }
            }
        }
    }
    static_assert(!noexcept(sed::level::levelMergeSurfaces(
        std::declval<sed::Level&>(), sed::SurfaceRef{}, sed::SurfaceRef{})));
    static_assert(!noexcept(sed::level::levelMergeSectors(
        std::declval<sed::Level&>(), 0, 1)));
    static_assert(!noexcept(sed::level::levelMergeVertices(
        std::declval<sed::Level&>(), std::span<const sed::VertexRef>{})));

    {
        sed::Level surfaceLevel = makeSurfaceUvMergeLevel(
            sed::UvCoordinateSpace::Normalized);
        surfaceLevel.uvCoordinateSpace =
            static_cast<sed::UvCoordinateSpace>(99);
        const std::size_t surfaceCountBefore =
            surfaceLevel.sectors[0].surfaces.size();
        const auto surfaceMerged = sed::level::levelMergeSurfaces(
            surfaceLevel,
            sed::SurfaceRef{ 0, 0 },
            sed::SurfaceRef{ 0, 1 });
        const std::array surfaceSelection{
            sed::SurfaceRef{ 0, 0 },
            sed::SurfaceRef{ 0, 1 },
        };
        const auto surfacesMerged = sed::level::levelMergeSelectedSurfaces(
            surfaceLevel,
            surfaceSelection);
        const bool surfaceMergedAt = sed::level::levelMergeSurfacesAt(
            surfaceLevel,
            sed::SurfaceRef{ 0, 0 },
            1);
        check(!surfaceMerged
                && !surfacesMerged
                && !surfaceMergedAt
                && surfaceLevel.sectors.size() == 1
                && surfaceLevel.sectors[0].surfaces.size()
                    == surfaceCountBefore
                && surfaceLevel.uvCoordinateSpace
                    == static_cast<sed::UvCoordinateSpace>(99),
            "Level.mergeSurfaces.invalidUvSpace rejects single, batch, and edge-targeted Surface merges atomically");

        sed::Level sectorLevel = makeSectorUvMergeLevel();
        sectorLevel.uvCoordinateSpace =
            static_cast<sed::UvCoordinateSpace>(99);
        const std::size_t sectorCountBefore = sectorLevel.sectors.size();
        const auto sectorMerged = sed::level::levelMergeSectors(
            sectorLevel, 0, 1);
        const std::array<std::int32_t, 2> sectorSelection{ 0, 1 };
        const auto sectorsMerged = sed::level::levelMergeSelectedSectors(
            sectorLevel,
            sectorSelection);
        const std::array vertexSelection{
            sed::VertexRef{ 0, 0 },
            sed::VertexRef{ 0, 1 },
        };
        const std::int32_t verticesMerged = sed::level::levelMergeVertices(
            sectorLevel,
            vertexSelection);
        check(!sectorMerged
                && !sectorsMerged
                && verticesMerged == 0
                && sectorLevel.sectors.size() == sectorCountBefore
                && sectorLevel.uvCoordinateSpace
                    == static_cast<sed::UvCoordinateSpace>(99),
            "Level.mergeSectors.invalidUvSpace rejects single/batch Sector and Vertex merges atomically");
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();

        const std::int32_t vA = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t vB = sector.newVertex(1.0, 0.0, 0.0);
        const std::int32_t vC = sector.newVertex(1.0, 1.0, 0.0);
        const std::int32_t vD = sector.newVertex(0.0, 1.0, 0.0);
        const std::int32_t vE = sector.newVertex(2.0, 0.0, 0.0);
        const std::int32_t vF = sector.newVertex(2.0, 1.0, 0.0);

        sed::Surface a{};
        a.vertices    = { vA, vB, vC, vD };
        a.texVertices = std::vector<sed::TexVertex>(a.vertices.size());
        (void)sector.addSurface(std::move(a));

        sed::Surface b{};
        b.vertices    = { vB, vE, vF, vC };
        b.texVertices = std::vector<sed::TexVertex>(b.vertices.size());
        (void)sector.addSurface(std::move(b));

        const std::int32_t sectorNum = sector.num;
        const auto merged = sed::level::levelMergeSurfaces(level, sed::SurfaceRef{ sectorNum, 0 }, sed::SurfaceRef{ sectorNum, 1 });
        const sed::Sector& mergedSector = level.sectors[static_cast<std::size_t>(sectorNum)];
        check(merged.has_value(), "Level.mergeSurfaces: merged");
        check(mergedSector.surfaces.size() == 1, "Level.mergeSurfaces: surface count");

        if (!mergedSector.surfaces.empty())
        {
            const sed::Surface& out = mergedSector.surfaces[0];
            check(out.vertices.size() == 6, "Level.mergeSurfaces: vertex count");
            check(out.texVertices.size() == 6, "Level.mergeSurfaces: tex vertex count");

            check(surfaceHasVertex(mergedSector, out, 0.0, 0.0, 0.0), "Level.mergeSurfaces: has A");
            check(surfaceHasVertex(mergedSector, out, 1.0, 0.0, 0.0), "Level.mergeSurfaces: has B");
            check(surfaceHasVertex(mergedSector, out, 1.0, 1.0, 0.0), "Level.mergeSurfaces: has C");
            check(surfaceHasVertex(mergedSector, out, 0.0, 1.0, 0.0), "Level.mergeSurfaces: has D");
            check(surfaceHasVertex(mergedSector, out, 2.0, 0.0, 0.0), "Level.mergeSurfaces: has E");
            check(surfaceHasVertex(mergedSector, out, 2.0, 1.0, 0.0), "Level.mergeSurfaces: has F");
        }
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();

        const std::int32_t vA = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t vB = sector.newVertex(1.0, 0.0, 0.0);
        const std::int32_t vC = sector.newVertex(1.0, 1.0, 0.0);
        const std::int32_t vD = sector.newVertex(0.0, 1.0, 0.0);
        const std::int32_t vE = sector.newVertex(2.0, 0.0, 0.0);
        const std::int32_t vF = sector.newVertex(2.0, 1.0, 0.0);

        sed::Surface a{};
        a.vertices    = { vA, vB, vC, vD };
        a.texVertices = std::vector<sed::TexVertex>(a.vertices.size());
        (void)sector.addSurface(std::move(a));

        sed::Surface b{};
        b.vertices    = { vB, vE, vE, vF, vC };
        b.texVertices = std::vector<sed::TexVertex>(b.vertices.size());
        (void)sector.addSurface(std::move(b));

        const std::int32_t sectorNum = sector.num;
        const auto merged = sed::level::levelMergeSurfaces(level, sed::SurfaceRef{ sectorNum, 0 }, sed::SurfaceRef{ sectorNum, 1 });
        const sed::Sector& mergedSector = level.sectors[static_cast<std::size_t>(sectorNum)];
        check(merged.has_value(), "Level.mergeSurfaces.duplicateCleanup: merged");
        check(mergedSector.surfaces.size() == 1, "Level.mergeSurfaces.duplicateCleanup: surface count");

        if (!mergedSector.surfaces.empty())
        {
            const sed::Surface& out = mergedSector.surfaces[0];
            check(out.vertices.size() == 6, "Level.mergeSurfaces.duplicateCleanup: vertex count");
            check(out.texVertices.size() == 6, "Level.mergeSurfaces.duplicateCleanup: tex vertex count");

            for (std::size_t i = 0; i < out.vertices.size(); ++i)
            {
                const std::size_t next = (i + 1) % out.vertices.size();
                check(out.vertices[i] != out.vertices[next], "Level.mergeSurfaces.duplicateCleanup: no adjacent duplicates");
            }
        }
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();

        const std::int32_t vA = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t vB = sector.newVertex(1.0, 0.0, 0.0);
        const std::int32_t vC = sector.newVertex(1.0, 1.0, 0.0);
        const std::int32_t vD = sector.newVertex(0.0, 1.0, 0.0);
        const std::int32_t vE = sector.newVertex(2.0, 0.0, 0.0);
        const std::int32_t vF = sector.newVertex(2.0, 1.0, 0.0);

        sed::Surface a{};
        a.vertices    = { vA, vB, vC, vD };
        a.texVertices = std::vector<sed::TexVertex>(a.vertices.size());
        (void)sector.addSurface(std::move(a));

        sed::Surface b{};
        b.vertices    = { vB, vE, vF, vC };
        b.texVertices = std::vector<sed::TexVertex>(b.vertices.size());
        (void)sector.addSurface(std::move(b));

        const std::int32_t sectorNum = sector.num;
        const bool ok = sed::level::levelMergeSurfacesAt(level, sed::SurfaceRef{ sectorNum, 0 }, 1);
        const sed::Sector& mergedSector = level.sectors[static_cast<std::size_t>(sectorNum)];
        check(ok, "Level.mergeSurfacesAt: ok");
        check(mergedSector.surfaces.size() == 1, "Level.mergeSurfacesAt: surface count");
        check(!mergedSector.surfaces.empty() && mergedSector.surfaces[0].vertices.size() == 6, "Level.mergeSurfacesAt: vertex count");
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();

        const std::int32_t vA = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t vB = sector.newVertex(1.0, 0.0, 0.0);
        const std::int32_t vC = sector.newVertex(1.0, 1.0, 0.0);
        const std::int32_t vD = sector.newVertex(0.0, 1.0, 0.0);
        const std::int32_t vE = sector.newVertex(2.0, 0.0, 0.0);
        const std::int32_t vF = sector.newVertex(2.0, 1.0, 0.0);

        sed::Surface a{};
        a.vertices    = { vA, vB, vC, vD };
        a.texVertices = std::vector<sed::TexVertex>(a.vertices.size());
        a.adjoin      = sed::SurfaceRef{ 0, 0 };
        (void)sector.addSurface(std::move(a));

        sed::Surface b{};
        b.vertices    = { vB, vE, vF, vC };
        b.texVertices = std::vector<sed::TexVertex>(b.vertices.size());
        (void)sector.addSurface(std::move(b));

        const auto merged = sed::level::levelMergeSurfaces(level, sed::SurfaceRef{ sector.num, 0 }, sed::SurfaceRef{ sector.num, 1 });
        check(!merged.has_value(), "Level.mergeSurfaces.adjoinMismatch: fails");
        check(sector.surfaces.size() == 2, "Level.mergeSurfaces.adjoinMismatch: surface count");
    }

    {
        sed::Level level{};
        sed::Sector& primary = level.newSector();
        (void)addFlatQuad(primary, 0.0);
        (void)addFlatQuad(primary, 1.0);
        primary.num = 0;
        sed::Sector& mirror = level.newSector();
        (void)addFlatQuad(mirror, 0.0, true);
        (void)addFlatQuad(mirror, 1.0, true);
        mirror.num = 1;
        check(sed::level::levelSysAdjoinSurfaces(
                  level, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 1, 0 }),
            "Level.mergeSurfaces.portal: first adjoin");
        check(sed::level::levelSysAdjoinSurfaces(
                  level, sed::SurfaceRef{ 0, 1 }, sed::SurfaceRef{ 1, 1 }),
            "Level.mergeSurfaces.portal: second adjoin");

        sed::Cog& cog = level.newCog();
        sed::CogValue primaryDonor{};
        primaryDonor.type = sed::ValueType::ptr;
        primaryDonor.cogType = sed::CogType::surface;
        primaryDonor.intValue = level.getGlobalSurfaceNumber(0, 1);
        primaryDonor.stringValue = "0 1";
        cog.values.push_back(primaryDonor);
        sed::CogValue mirrorDonor = primaryDonor;
        mirrorDonor.intValue = level.getGlobalSurfaceNumber(1, 1);
        mirrorDonor.stringValue = "1 1";
        cog.values.push_back(mirrorDonor);
        sed::CogValue invalid = primaryDonor;
        invalid.intValue = 99;
        invalid.stringValue = "99 99";
        cog.values.push_back(invalid);

        const auto merged = sed::level::levelMergeSurfaces(
            level, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 0, 1 });
        check(merged && level.sectors[0].surfaces.size() == 1
                && level.sectors[1].surfaces.size() == 1,
            "Level.mergeSurfaces.portal: both sides merged atomically");
        check(level.sectors[0].surfaces[0].adjoin
                && *level.sectors[0].surfaces[0].adjoin == sed::SurfaceRef{ 1, 0 }
                && level.sectors[1].surfaces[0].adjoin
                && *level.sectors[1].surfaces[0].adjoin == sed::SurfaceRef{ 0, 0 },
            "Level.mergeSurfaces.portal: reciprocal merged adjoin");
        check(level.cogs[0].values[0].intValue == 0
                && level.cogs[0].values[0].stringValue.empty()
                && level.cogs[0].values[0].asJedString(level) == "0 0"
                && level.cogs[0].values[1].intValue == 1
                && level.cogs[0].values[1].stringValue.empty()
                && level.cogs[0].values[1].asJedString(level) == "1 0",
            "Level.mergeSurfaces.portal: primary and mirror COG refs remapped coherently");
        check(level.cogs[0].values[2].intValue == 99
                && level.cogs[0].values[2].stringValue == "99 99",
            "Level.mergeSurfaces.portal: invalid COG ref cannot become retargeted");
    }

    {
        sed::Level level{};
        sed::Sector& primary = level.newSector();
        (void)addFlatQuad(primary, 0.0);
        (void)addFlatQuad(primary, 1.0);
        primary.num = 0;
        sed::Sector& mirror = level.newSector();
        (void)addFlatQuad(mirror, 0.0, true);
        (void)addFlatQuad(mirror, 1.0, true);
        mirror.num = 1;
        check(sed::level::levelSysAdjoinSurfaces(
                  level, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 1, 0 })
                && sed::level::levelSysAdjoinSurfaces(
                    level, sed::SurfaceRef{ 0, 1 }, sed::SurfaceRef{ 1, 1 }),
            "Level.mergeSurfaces.portalInvalid: setup adjoins");
        for (sed::Vertex& vertex : level.sectors[1].vertices)
        {
            if (close(vertex.x, 2.0))
            {
                vertex.z = 0.5;
            }
        }
        level.sectors[1].surfaces[1].normal =
            sed::level::surfaceCalcNormalNewell(
                level.sectors[1], level.sectors[1].surfaces[1]);

        const auto rejected = sed::level::levelMergeSurfaces(
            level, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 0, 1 });
        check(!rejected && level.sectors[0].surfaces.size() == 2
                && level.sectors[1].surfaces.size() == 2,
            "Level.mergeSurfaces.portalInvalid: incompatible mirror rejects transaction");
        check(level.sectors[0].surfaces[0].adjoin
                && level.sectors[0].surfaces[1].adjoin
                && level.sectors[1].surfaces[0].adjoin
                && level.sectors[1].surfaces[1].adjoin,
            "Level.mergeSurfaces.portalInvalid: both portal pairs remain intact");
    }

    {
        const auto makeBoundaryLevel = [](const std::int32_t donorBottomSegments)
        {
            sed::Level level{};
            sed::Sector& sector = level.newSector();
            const auto addRectangle = [&sector](
                const double x0,
                const double x1,
                const std::int32_t bottomSegments,
                const std::int32_t rightSegments,
                const std::int32_t topSegments,
                const std::int32_t leftSegments)
            {
                sed::Surface surface{};
                for (std::int32_t i = 0; i < bottomSegments; ++i)
                {
                    surface.vertices.push_back(sector.addVertex(
                        x0 + (x1 - x0) * static_cast<double>(i)
                            / static_cast<double>(bottomSegments),
                        0.0, 0.0));
                }
                for (std::int32_t i = 0; i < rightSegments; ++i)
                {
                    surface.vertices.push_back(sector.addVertex(
                        x1, static_cast<double>(i)
                            / static_cast<double>(rightSegments), 0.0));
                }
                for (std::int32_t i = 0; i < topSegments; ++i)
                {
                    surface.vertices.push_back(sector.addVertex(
                        x1 - (x1 - x0) * static_cast<double>(i)
                            / static_cast<double>(topSegments),
                        1.0, 0.0));
                }
                for (std::int32_t i = 0; i < leftSegments; ++i)
                {
                    surface.vertices.push_back(sector.addVertex(
                        x0, 1.0 - static_cast<double>(i)
                            / static_cast<double>(leftSegments), 0.0));
                }
                surface.texVertices.resize(surface.vertices.size());
                surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);
                (void)sector.addSurface(std::move(surface));
            };
            addRectangle(0.0, 1.0, 4, 1, 4, 4); // 13 vertices
            addRectangle(1.0, 2.0, donorBottomSegments, 4, 4, 1);
            return level;
        };

        sed::Level atLimit = makeBoundaryLevel(4); // 13 + 13 - 2 = 24
        const auto allowed = sed::level::levelMergeSurfaces(
            atLimit, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 0, 1 });
        check(allowed && atLimit.sectors[0].surfaces[0].vertices.size() == 24,
            "Level.mergeSurfaces.vertexLimit: exact JK limit accepted");

        sed::Level overLimit = makeBoundaryLevel(5); // 13 + 14 - 2 = 25
        const auto rejected = sed::level::levelMergeSurfaces(
            overLimit, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 0, 1 });
        check(!rejected && overLimit.sectors[0].surfaces.size() == 2,
            "Level.mergeSurfaces.vertexLimit: post-cleanup JK limit enforced transactionally");
    }

    {
        sed::Level level{};
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        (void)level.newSector();
        (void)level.newSector();
        (void)level.newSector();

        constexpr std::int32_t keepIndex  = 0;
        constexpr std::int32_t otherIndex = 1;
        constexpr std::int32_t delIndex   = 2;

        sed::Sector& keep  = level.sectors[keepIndex];
        sed::Sector& other = level.sectors[otherIndex];
        sed::Sector& del   = level.sectors[delIndex];

        const std::int32_t kA = keep.newVertex(0.0, 0.0, 0.0);
        const std::int32_t kB = keep.newVertex(1.0, 0.0, 0.0);
        const std::int32_t kC = keep.newVertex(1.0, 1.0, 0.0);
        const std::int32_t kD = keep.newVertex(0.0, 1.0, 0.0);

        sed::Surface keepPortal{};
        keepPortal.vertices    = { kA, kB, kC, kD };
        keepPortal.texVertices = std::vector<sed::TexVertex>(keepPortal.vertices.size());
        keepPortal.normal = sed::level::surfaceCalcNormalNewell(keep, keepPortal);
        (void)keep.addSurface(std::move(keepPortal));

        const std::int32_t dA = del.newVertex(0.0, 0.0, 0.0);
        const std::int32_t dB = del.newVertex(1.0, 0.0, 0.0);
        const std::int32_t dC = del.newVertex(1.0, 1.0, 0.0);
        const std::int32_t dD = del.newVertex(0.0, 1.0, 0.0);

        sed::Surface delPortal{};
        delPortal.vertices    = { dA, dD, dC, dB };
        delPortal.texVertices = std::vector<sed::TexVertex>(delPortal.vertices.size());
        delPortal.normal = sed::level::surfaceCalcNormalNewell(del, delPortal);
        (void)del.addSurface(std::move(delPortal));

        const std::int32_t dE = del.newVertex(0.0, 0.0, 1.0);
        const std::int32_t dF = del.newVertex(1.0, 0.0, 1.0);
        const std::int32_t dG = del.newVertex(1.0, 1.0, 1.0);
        const std::int32_t dH = del.newVertex(0.0, 1.0, 1.0);

        sed::Surface delTop{};
        // The surviving half-space faces inward toward the deleted portal;
        // keeping the opposite winding would make this intentionally minimal
        // open fixture non-convex under the production sector validator.
        delTop.vertices    = { dE, dH, dG, dF };
        delTop.texVertices = std::vector<sed::TexVertex>(delTop.vertices.size());
        delTop.texVertices[0] = sed::TexVertex{
            -7.0f, 11.0f, sed::ColorF{ 0.1f, 0.2f, 0.3f, 0.4f } };
        delTop.texVertices[1] = sed::TexVertex{
            19.0f, -23.0f, sed::ColorF{ 0.2f, 0.3f, 0.4f, 0.5f } };
        delTop.texVertices[2] = sed::TexVertex{
            29.0f, 31.0f, sed::ColorF{ 0.3f, 0.4f, 0.5f, 0.6f } };
        delTop.texVertices[3] = sed::TexVertex{
            -37.0f, 41.0f, sed::ColorF{ 0.4f, 0.5f, 0.6f, 0.7f } };
        delTop.uScale = -0.03125f;
        delTop.vScale = 0.09375f;
        delTop.normal = sed::level::surfaceCalcNormalNewell(del, delTop);
        const sed::Surface expectedMovedTextureState = delTop;
        (void)del.addSurface(std::move(delTop));

        const std::int32_t oE = other.newVertex(0.0, 0.0, 1.0);
        const std::int32_t oF = other.newVertex(1.0, 0.0, 1.0);
        const std::int32_t oG = other.newVertex(1.0, 1.0, 1.0);
        const std::int32_t oH = other.newVertex(0.0, 1.0, 1.0);

        sed::Surface otherSurf{};
        otherSurf.vertices    = { oE, oF, oG, oH };
        otherSurf.texVertices = std::vector<sed::TexVertex>(otherSurf.vertices.size());
        otherSurf.normal = sed::level::surfaceCalcNormalNewell(other, otherSurf);
        (void)other.addSurface(std::move(otherSurf));

        check(sed::level::levelSysAdjoinSurfaces(level, sed::SurfaceRef{ keepIndex, 0 }, sed::SurfaceRef{ delIndex, 0 }),
            "Level.mergeSectors: portal adjoin");
        check(sed::level::levelSysAdjoinSurfaces(level, sed::SurfaceRef{ otherIndex, 0 }, sed::SurfaceRef{ delIndex, 1 }),
            "Level.mergeSectors: moved surface adjoin");

        sed::Thing& thing = level.newThing();
        thing.sector      = delIndex;

        const auto merged = sed::level::levelMergeSectors(level, keepIndex, delIndex);
        const bool mergedOk = merged.has_value() && *merged == keepIndex;
        check(mergedOk, "Level.mergeSectors: merged");

        if (mergedOk)
        {
            check(level.sectors.size() == 2, "Level.mergeSectors: sector count");
            check(level.things.size() == 1 && level.things[0].sector == keepIndex,
                "Level.mergeSectors: donor Thing remapped to survivor");

            if (level.sectors.size() >= 2)
            {
                check(level.sectors[keepIndex].surfaces.size() == 1, "Level.mergeSectors: keep surface count");
                check(level.sectors[otherIndex].surfaces.size() == 1, "Level.mergeSectors: other surface count");

                if (level.sectors[keepIndex].surfaces.size() == 1 && level.sectors[otherIndex].surfaces.size() == 1)
                {
                    const sed::Surface& keepOut = level.sectors[keepIndex].surfaces[0];
                    check(keepOut.adjoin.has_value(), "Level.mergeSectors: keep surface adjoin");
                    check(level.uvCoordinateSpace
                                == sed::UvCoordinateSpace::Normalized
                            && keepOut.texVertices.size()
                                == expectedMovedTextureState.texVertices.size()
                            && sameTextureVertex(
                                keepOut.texVertices[0],
                                expectedMovedTextureState.texVertices[0])
                            && sameTextureVertex(
                                keepOut.texVertices[1],
                                expectedMovedTextureState.texVertices[1])
                            && sameTextureVertex(
                                keepOut.texVertices[2],
                                expectedMovedTextureState.texVertices[2])
                            && sameTextureVertex(
                                keepOut.texVertices[3],
                                expectedMovedTextureState.texVertices[3])
                            && keepOut.uScale
                                == expectedMovedTextureState.uScale
                            && keepOut.vScale
                                == expectedMovedTextureState.vScale,
                        "Level.mergeSectors: moved signed multi-tile UVs, colors, scales, and coordinate-space tag remain exact");
                    if (keepOut.adjoin)
                    {
                        check(keepOut.adjoin->sector == otherIndex, "Level.mergeSectors: keep surface adjoin sector");
                        check(keepOut.adjoin->surface == 0, "Level.mergeSectors: keep surface adjoin surface");
                    }

                    const sed::Surface& otherOut = level.sectors[otherIndex].surfaces[0];
                    check(otherOut.adjoin.has_value(), "Level.mergeSectors: other surface adjoin");
                    if (otherOut.adjoin)
                    {
                        check(otherOut.adjoin->sector == keepIndex, "Level.mergeSectors: other surface adjoin sector");
                        check(otherOut.adjoin->surface == 0, "Level.mergeSectors: other surface adjoin surface");
                    }
                }
            }
        }
    }

    {
        bool rejectedAtomically = true;
        for (std::int32_t malformedCase = 0;
            malformedCase < 3;
            ++malformedCase)
        {
            sed::Level level = makeSectorUvMergeLevel();
            sed::Surface& moved = level.sectors[1].surfaces[1];
            if (malformedCase == 0)
            {
                moved.texVertices.pop_back();
            }
            else if (malformedCase == 1)
            {
                moved.texVertices[1].u =
                    std::numeric_limits<float>::infinity();
            }
            else
            {
                moved.vScale = std::numeric_limits<float>::quiet_NaN();
            }
            const std::size_t movedUvCount = moved.texVertices.size();

            const auto merged = sed::level::levelMergeSectors(level, 0, 1);
            rejectedAtomically = rejectedAtomically
                && !merged
                && level.sectors.size() == 2
                && level.sectors[0].surfaces.size() == 1
                && level.sectors[1].surfaces.size() == 2
                && level.sectors[1].surfaces[1].vertices.size() == 4
                && level.sectors[1].surfaces[1].texVertices.size()
                    == movedUvCount
                && level.sectors[0].surfaces[0].adjoin
                && *level.sectors[0].surfaces[0].adjoin
                    == sed::SurfaceRef{ 1, 0 };
        }
        check(rejectedAtomically,
            "Level.mergeSectors.invalidUv: malformed moved UV data rejects without partial sector publication or padding");
    }

    {
        sed::Level level{};
        (void)level.newSector();
        (void)level.newSector();

        constexpr std::int32_t keepIndex = 0;
        constexpr std::int32_t delIndex  = 1;

        sed::Sector& keep = level.sectors[keepIndex];
        sed::Sector& del  = level.sectors[delIndex];

        const std::int32_t kA = keep.newVertex(0.0, 0.0, 0.0);
        const std::int32_t kB = keep.newVertex(1.0, 0.0, 0.0);
        const std::int32_t kC = keep.newVertex(1.0, 1.0, 0.0);
        const std::int32_t kD = keep.newVertex(0.0, 1.0, 0.0);

        sed::Surface a{};
        a.vertices    = { kA, kB, kC, kD };
        a.texVertices = std::vector<sed::TexVertex>(a.vertices.size());
        (void)keep.addSurface(std::move(a));

        const std::int32_t dA = del.newVertex(0.0, 0.0, 0.0);
        const std::int32_t dB = del.newVertex(1.0, 0.0, 0.0);
        const std::int32_t dC = del.newVertex(1.0, 1.0, 0.0);
        const std::int32_t dD = del.newVertex(0.0, 1.0, 0.0);

        sed::Surface b{};
        b.vertices    = { dA, dB, dC, dD };
        b.texVertices = std::vector<sed::TexVertex>(b.vertices.size());
        (void)del.addSurface(std::move(b));

        check(sed::level::levelSysAdjoinSurfaces(level, sed::SurfaceRef{ keepIndex, 0 }, sed::SurfaceRef{ delIndex, 0 }),
            "Level.mergeSectors.noOverlap: adjoin");

        const auto merged = sed::level::levelMergeSectors(level, keepIndex, delIndex);
        check(!merged.has_value(), "Level.mergeSectors.noOverlap: fails");
        check(level.sectors.size() == 2, "Level.mergeSectors.noOverlap: sector count");
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();

        const std::int32_t vA = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t vB = sector.newVertex(1.0, 0.0, 0.0);
        const std::int32_t vC = sector.newVertex(1.0, 0.0, 0.0);
        const std::int32_t vD = sector.newVertex(0.0, 1.0, 0.0);

        sed::Surface s{};
        s.vertices    = { vA, vB, vC, vD };
        s.texVertices = std::vector<sed::TexVertex>(s.vertices.size());
        (void)sector.addSurface(std::move(s));

        const std::array<sed::VertexRef, 2> selection{
            sed::VertexRef{ sector.num, vB },
            sed::VertexRef{ sector.num, vC },
        };

        const std::int32_t merged = sed::level::levelMergeVertices(level, selection);
        check(merged == 1, "Level.mergeVertices: merged count");
        check(sector.vertices.size() == 3, "Level.mergeVertices: vertex count");
        check(sector.surfaces.size() == 1, "Level.mergeVertices: surface count");

        if (!sector.surfaces.empty())
        {
            const sed::Surface& out = sector.surfaces[0];
            check(out.vertices.size() == 3, "Level.mergeVertices: surface vertices");
            check(out.texVertices.size() == 3, "Level.mergeVertices: surface tex vertices");

            check(surfaceHasVertex(sector, out, 0.0, 0.0, 0.0), "Level.mergeVertices: has A");
            check(surfaceHasVertex(sector, out, 1.0, 0.0, 0.0), "Level.mergeVertices: has B/C");
            check(surfaceHasVertex(sector, out, 0.0, 1.0, 0.0), "Level.mergeVertices: has D");
        }
    }

    {
        sed::Level level{};
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        sed::Sector& sector = level.newSector();

        const std::int32_t a = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t keep = sector.newVertex(1.0, 0.0, 0.0);
        const std::int32_t remove = sector.newVertex(1.0, 0.0, 0.0);
        const std::int32_t c = sector.newVertex(1.0, 1.0, 0.0);
        const std::int32_t d = sector.newVertex(0.0, 1.0, 0.0);

        sed::Surface surface{};
        surface.vertices = { a, keep, remove, c, d };
        surface.texVertices = {
            { -3.25f, 7.5f, sed::ColorF{ 0.1f, 0.2f, 0.3f, 0.4f } },
            { 5.75f, -2.5f, sed::ColorF{ 0.2f, 0.3f, 0.4f, 0.5f } },
            { 91.0f, 92.0f, sed::ColorF{ 0.9f, 0.8f, 0.7f, 0.6f } },
            { 11.125f, 13.25f, sed::ColorF{ 0.3f, 0.4f, 0.5f, 0.6f } },
            { -17.5f, 19.75f, sed::ColorF{ 0.4f, 0.5f, 0.6f, 0.7f } },
        };
        surface.uScale = -0.03125f;
        surface.vScale = 0.09375f;
        surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);
        (void)sector.addSurface(surface);

        const std::array<sed::TexVertex, 4> expectedTextureVertices{
            surface.texVertices[0],
            surface.texVertices[1],
            surface.texVertices[3],
            surface.texVertices[4]
        };
        const std::array<sed::VertexRef, 2> selection{
            sed::VertexRef{ 0, keep },
            sed::VertexRef{ 0, remove }
        };

        const std::int32_t merged =
            sed::level::levelMergeVertices(level, selection);
        check(merged == 1,
            "Level.mergeVertices.authoredUv: coincident selected vertices merge");
        check(level.uvCoordinateSpace == sed::UvCoordinateSpace::Normalized,
            "Level.mergeVertices.authoredUv: normalized coordinate-space tag survives");
        check(sector.surfaces.size() == 1
                && sector.surfaces[0].vertices.size() == 4
                && sector.surfaces[0].texVertices.size() == 4,
            "Level.mergeVertices.authoredUv: only the collapsed duplicate corner is removed");
        if ( sector.surfaces.size() == 1
            && sector.surfaces[0].texVertices.size()
                == expectedTextureVertices.size() )
        {
            const sed::Surface& output = sector.surfaces[0];
            bool exactTextureVertices = true;
            for ( std::size_t corner = 0;
                corner < expectedTextureVertices.size();
                ++corner )
            {
                exactTextureVertices = exactTextureVertices
                    && sameTextureVertex(
                        output.texVertices[corner],
                        expectedTextureVertices[corner]);
            }
            check(exactTextureVertices,
                "Level.mergeVertices.authoredUv: every surviving signed multi-tile UV and baked color stays exact");
            check(output.uScale == surface.uScale
                    && output.vScale == surface.vScale,
                "Level.mergeVertices.authoredUv: anisotropic signed scale metadata stays exact");
        }
    }

    {
        bool rejectedAtomically = true;
        for (std::int32_t malformedCase = 0;
            malformedCase < 3;
            ++malformedCase)
        {
            sed::Level level{};
            level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
            sed::Sector& sector = level.newSector();
            const std::int32_t a = sector.newVertex(0.0, 0.0, 0.0);
            const std::int32_t keep = sector.newVertex(1.0, 0.0, 0.0);
            const std::int32_t remove = sector.newVertex(1.0, 0.0, 0.0);
            const std::int32_t d = sector.newVertex(0.0, 1.0, 0.0);
            sed::Surface surface{};
            surface.vertices = { a, keep, remove, d };
            surface.texVertices.resize(surface.vertices.size());
            surface.normal = sed::level::surfaceCalcNormalNewell(
                sector, surface);
            (void)sector.addSurface(std::move(surface));

            if (malformedCase == 0)
            {
                sector.surfaces[0].texVertices.pop_back();
            }
            else if (malformedCase == 1)
            {
                sector.surfaces[0].texVertices[1].v =
                    std::numeric_limits<float>::infinity();
            }
            else
            {
                sector.surfaces[0].uScale =
                    std::numeric_limits<float>::quiet_NaN();
            }
            const std::size_t vertexCount = sector.vertices.size();
            const std::size_t uvCount = sector.surfaces[0].texVertices.size();
            const std::array<sed::VertexRef, 2> selection{
                sed::VertexRef{ 0, keep },
                sed::VertexRef{ 0, remove }
            };

            const std::int32_t merged =
                sed::level::levelMergeVertices(level, selection);
            rejectedAtomically = rejectedAtomically
                && merged == 0
                && sector.vertices.size() == vertexCount
                && sector.surfaces.size() == 1
                && sector.surfaces[0].vertices.size() == 4
                && sector.surfaces[0].texVertices.size() == uvCount;
        }
        check(rejectedAtomically,
            "Level.mergeVertices.invalidUv: malformed UV data rejects without partial topology or synthesized corners");
    }

    {
        sed::Level level{};
        (void)level.newSector();
        (void)level.newSector();

        constexpr std::int32_t s0Index = 0;
        constexpr std::int32_t s1Index = 1;

        sed::Sector& s0 = level.sectors[s0Index];
        sed::Sector& s1 = level.sectors[s1Index];

        const std::int32_t v0 = s0.newVertex(0.0, 0.0, 0.0);
        const std::int32_t v1 = s1.newVertex(0.0, 0.0, 0.0);

        const std::array<sed::VertexRef, 2> selection{
            sed::VertexRef{ s0Index, v0 },
            sed::VertexRef{ s1Index, v1 },
        };

        const std::int32_t merged = sed::level::levelMergeVertices(level, selection);
        check(merged == 0, "Level.mergeVertices.multiSector: merged count");
        check(level.sectors.size() == 2, "Level.mergeVertices.multiSector: sector count");
        check(level.sectors[s0Index].vertices.size() == 1, "Level.mergeVertices.multiSector: s0 vertex count");
        check(level.sectors[s1Index].vertices.size() == 1, "Level.mergeVertices.multiSector: s1 vertex count");
    }

    {
        sed::Level level{};
        level.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;
        sed::Sector& sector = level.newSector();
        const std::int32_t a = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t b = sector.newVertex(1.0, 0.0, 0.0);
        const std::int32_t c = sector.newVertex(0.0, 1.0, 0.0);

        sed::Surface surface{};
        surface.vertices = { a, b, c };
        surface.texVertices = {
            { -64.0f, 96.0f, sed::ColorRed },
            { 320.0f, -192.0f, sed::ColorGreen },
            { 576.0f, 448.0f, sed::ColorBlue },
        };
        surface.uScale = -16.0f;
        surface.vScale = 32.0f;
        sector.addSurface(surface);
        const std::vector<sed::Vertex> expectedVertices = sector.vertices;

        const std::array<sed::VertexRef, 2> selection{
            sed::VertexRef{ 0, a },
            sed::VertexRef{ 0, b }
        };
        check(sed::level::levelMergeVertices(level, selection) == 0,
            "Level.mergeVertices.nonCoincident: rejects distinct selected positions");
        bool exactVertices = sector.vertices.size()
            == expectedVertices.size();
        for ( std::size_t vertexIndex = 0;
            exactVertices && vertexIndex < expectedVertices.size();
            ++vertexIndex )
        {
            const sed::Vertex& actual = sector.vertices[vertexIndex];
            const sed::Vertex& expected = expectedVertices[vertexIndex];
            exactVertices = actual.x == expected.x
                && actual.y == expected.y
                && actual.z == expected.z
                && actual.mark == expected.mark;
        }
        check(level.uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels
                && exactVertices
                && sector.surfaces.size() == 1
                && sector.surfaces[0].vertices == surface.vertices
                && sector.surfaces[0].texVertices.size()
                    == surface.texVertices.size()
                && sameTextureVertex(
                    sector.surfaces[0].texVertices[0],
                    surface.texVertices[0])
                && sameTextureVertex(
                    sector.surfaces[0].texVertices[1],
                    surface.texVertices[1])
                && sameTextureVertex(
                    sector.surfaces[0].texVertices[2],
                    surface.texVertices[2])
                && sector.surfaces[0].uScale == surface.uScale
                && sector.surfaces[0].vScale == surface.vScale,
            "Level.mergeVertices.nonCoincident: rejection leaves legacy pixel geometry and texture state atomic");
    }

    {
        sed::Level level{};
        level.ppunit = 1.0;
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        sed::Sector& sector = level.newSector();

        const std::int32_t a = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t b = sector.newVertex(1.0, 0.0, 0.0);
        const std::int32_t c = sector.newVertex(1.0, 1.0, 0.0);
        const std::int32_t d = sector.newVertex(0.0, 1.0, 0.0);
        const std::int32_t e = sector.newVertex(2.0, 0.0, 0.0);
        const std::int32_t f = sector.newVertex(2.0, 1.0, 0.0);

        sed::Surface first{};
        first.vertices = { a, b, c, d };
        first.texVertices = {
            { -2.25f, 3.5f, sed::ColorF{ 0.1f, 0.2f, 0.3f, 0.4f } },
            { 4.75f, -5.5f, sed::ColorF{ 0.2f, 0.3f, 0.4f, 0.5f } },
            { 7.25f, 8.5f, sed::ColorF{ 0.3f, 0.4f, 0.5f, 0.6f } },
            { -9.75f, 11.5f, sed::ColorF{ 0.4f, 0.5f, 0.6f, 0.7f } },
        };
        first.normal = sed::TVector::up;
        first.uScale = -0.03125f;
        first.vScale = 0.09375f;
        const sed::TexVertex expectedAnchor = first.texVertices[0];
        const sed::TexVertex expectedOriginalCorner = first.texVertices[3];
        const float expectedUScale = first.uScale;
        const float expectedVScale = first.vScale;
        (void)sector.addSurface(std::move(first));

        sed::Surface second{};
        second.vertices = { b, e, f, c };
        second.texVertices.resize(4);
        second.normal = sed::TVector::up;
        (void)sector.addSurface(std::move(second));

        const auto merged = sed::level::levelMergeSurfaces(
            level, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 0, 1 });
        check(merged.has_value(), "Level.mergeSurfaces.uvProjection: merged");
        check(level.uvCoordinateSpace == sed::UvCoordinateSpace::Normalized,
            "Level.mergeSurfaces.uvProjection: preserves the canonical Level UV tag");
        const sed::Sector& mergedSector = level.sectors[0];
        if (merged && !mergedSector.surfaces.empty())
        {
            const sed::Surface& output = mergedSector.surfaces[0];
            const auto uvAtVertex = [&](const std::int32_t vertex) -> const sed::TexVertex*
            {
                const auto found = std::find(output.vertices.begin(), output.vertices.end(), vertex);
                return found == output.vertices.end()
                    ? nullptr
                    : &output.texVertices[static_cast<std::size_t>(std::distance(output.vertices.begin(), found))];
            };
            const sed::TexVertex* anchor = uvAtVertex(a);
            const sed::TexVertex* originalCorner = uvAtVertex(d);
            check(anchor
                    && sameTextureVertex(*anchor, expectedAnchor),
                "Level.mergeSurfaces.uvProjection: signed multi-tile anchor UV and color remain exact");
            check(originalCorner
                    && sameTextureVertex(
                        *originalCorner,
                        expectedOriginalCorner),
                "Level.mergeSurfaces.uvProjection: every retained source-corner tuple remains exact");
            check(output.uScale == expectedUScale
                    && output.vScale == expectedVScale,
                "Level.mergeSurfaces.uvProjection: anisotropic signed normalized scale metadata follows the source surface");
        }
    }

    {
        constexpr float uDimension = 320.0f;
        constexpr float vDimension = 96.0f;
        sed::Level normalized = makeSurfaceUvMergeLevel(
            sed::UvCoordinateSpace::Normalized);
        sed::Level pixels = makeSurfaceUvMergeLevel(
            sed::UvCoordinateSpace::TexturePixels,
            uDimension,
            vDimension);

        const auto normalizedMerged = sed::level::levelMergeSurfaces(
            normalized, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 0, 1 });
        const auto pixelMerged = sed::level::levelMergeSurfaces(
            pixels, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 0, 1 });
        bool equivalent = normalizedMerged.has_value()
            && pixelMerged.has_value()
            && normalized.uvCoordinateSpace
                == sed::UvCoordinateSpace::Normalized
            && pixels.uvCoordinateSpace
                == sed::UvCoordinateSpace::TexturePixels
            && normalized.sectors.size() == 1
            && pixels.sectors.size() == 1
            && normalized.sectors[0].surfaces.size() == 1
            && pixels.sectors[0].surfaces.size() == 1;
        if (equivalent)
        {
            const sed::Sector& normalizedSector = normalized.sectors[0];
            const sed::Sector& pixelSector = pixels.sectors[0];
            const sed::Surface& normalizedSurface =
                normalizedSector.surfaces[0];
            const sed::Surface& pixelSurface = pixelSector.surfaces[0];
            equivalent = normalizedSurface.vertices.size()
                    == pixelSurface.vertices.size()
                && normalizedSurface.texVertices.size()
                    == normalizedSurface.vertices.size()
                && pixelSurface.texVertices.size()
                    == pixelSurface.vertices.size()
                && close(pixelSurface.uScale,
                    normalizedSurface.uScale * uDimension)
                && close(pixelSurface.vScale,
                    normalizedSurface.vScale * vDimension);
            for (std::size_t index = 0;
                equivalent && index < normalizedSurface.vertices.size();
                ++index)
            {
                const sed::Vertex& position = normalizedSector.vertices[
                    static_cast<std::size_t>(normalizedSurface.vertices[index])];
                const sed::TexVertex* pixelUv = textureVertexAtPosition(
                    pixelSector, pixelSurface, position);
                const sed::TexVertex& normalizedUv =
                    normalizedSurface.texVertices[index];
                equivalent = pixelUv
                    && close(pixelUv->u,
                        normalizedUv.u * uDimension, 1.0e-4)
                    && close(pixelUv->v,
                        normalizedUv.v * vDimension, 1.0e-4)
                    && pixelUv->color == normalizedUv.color;
            }
        }
        check(equivalent,
            "Level.mergeSurfaces.unitNeutral: normalized and anisotropic pixel mappings remain equivalent");
    }

    {
        bool rejectedAtomically = true;
        for (const std::size_t malformedSurface : {
                std::size_t{ 0 }, std::size_t{ 1 } })
        {
            sed::Level level = makeSurfaceUvMergeLevel(
                sed::UvCoordinateSpace::Normalized);
            level.sectors[0].surfaces[malformedSurface].texVertices.pop_back();
            const std::size_t firstUvCount =
                level.sectors[0].surfaces[0].texVertices.size();
            const std::size_t secondUvCount =
                level.sectors[0].surfaces[1].texVertices.size();

            const auto merged = sed::level::levelMergeSurfaces(
                level, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 0, 1 });
            rejectedAtomically = rejectedAtomically
                && !merged
                && level.sectors.size() == 1
                && level.sectors[0].surfaces.size() == 2
                && level.sectors[0].surfaces[0].vertices.size() == 4
                && level.sectors[0].surfaces[1].vertices.size() == 4
                && level.sectors[0].surfaces[0].texVertices.size()
                    == firstUvCount
                && level.sectors[0].surfaces[1].texVertices.size()
                    == secondUvCount;
        }
        check(rejectedAtomically,
            "Level.mergeSurfaces.uvCardinality: incomplete survivor or donor UVs reject without synthesized replacements");
    }

    {
        bool rejectedAtomically = true;
        for (std::int32_t malformedCase = 0;
            malformedCase < 4;
            ++malformedCase)
        {
            sed::Level level = makeSurfaceUvMergeLevel(
                sed::UvCoordinateSpace::Normalized);
            if (malformedCase == 0)
            {
                level.sectors[0].surfaces[0].texVertices[1].u =
                    std::numeric_limits<float>::infinity();
            }
            else if (malformedCase == 1)
            {
                level.sectors[0].surfaces[1].texVertices[2].v =
                    std::numeric_limits<float>::quiet_NaN();
            }
            else if (malformedCase == 2)
            {
                level.sectors[0].surfaces[0].uScale =
                    std::numeric_limits<float>::infinity();
            }
            else
            {
                level.sectors[0].surfaces[1].vScale =
                    std::numeric_limits<float>::quiet_NaN();
            }

            const auto merged = sed::level::levelMergeSurfaces(
                level, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 0, 1 });
            rejectedAtomically = rejectedAtomically
                && !merged
                && level.sectors.size() == 1
                && level.sectors[0].surfaces.size() == 2
                && level.sectors[0].surfaces[0].vertices.size() == 4
                && level.sectors[0].surfaces[1].vertices.size() == 4
                && level.sectors[0].surfaces[0].texVertices.size() == 4
                && level.sectors[0].surfaces[1].texVertices.size() == 4;
        }
        check(rejectedAtomically,
            "Level.mergeSurfaces.nonFiniteUv: non-finite coordinates or scales reject atomically on either input");
    }

    {
        sed::Level level = makeSurfaceUvMergeLevel(
            sed::UvCoordinateSpace::Normalized);
        sed::Surface& survivor = level.sectors[0].surfaces[0];
        const float maximum = std::numeric_limits<float>::max();
        survivor.texVertices[0].u = 0.0f;
        survivor.texVertices[1].u = maximum;
        survivor.texVertices[2].u = maximum;
        survivor.texVertices[3].u = 0.0f;

        const auto merged = sed::level::levelMergeSurfaces(
            level, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 0, 1 });
        check(!merged
                && level.sectors.size() == 1
                && level.sectors[0].surfaces.size() == 2
                && level.sectors[0].surfaces[0].texVertices[1].u
                    == maximum,
            "Level.mergeSurfaces.uvFloatOverflow: finite affine results outside float range reject atomically");
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();
        const auto addQuad = [&](const double x) -> std::int32_t
        {
            const std::int32_t a = sector.addVertex(x, 0.0, 0.0);
            const std::int32_t b = sector.addVertex(x + 1.0, 0.0, 0.0);
            const std::int32_t c = sector.addVertex(x + 1.0, 1.0, 0.0);
            const std::int32_t d = sector.addVertex(x, 1.0, 0.0);
            sed::Surface surface{};
            surface.vertices = { a, b, c, d };
            surface.texVertices.resize(4);
            surface.normal = sed::TVector::up;
            return sector.addSurface(std::move(surface));
        };

        // addVertex de-duplicates the shared edge within each pair.
        (void)addQuad(0.0);
        (void)addQuad(1.0);
        (void)addQuad(10.0);
        (void)addQuad(11.0);

        const std::array<sed::SurfaceRef, 4> selection{
            sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 0, 1 },
            sed::SurfaceRef{ 0, 2 }, sed::SurfaceRef{ 0, 3 }
        };
        const auto merged = sed::level::levelMergeSelectedSurfaces(level, selection);
        check(merged.has_value(), "Level.mergeSelectedSurfaces: merged");
        check(merged && merged->mergeCount == 2,
            "Level.mergeSelectedSurfaces: merge count");
        check(level.sectors[0].surfaces.size() == 2,
            "Level.mergeSelectedSurfaces: surface count");
        check(merged && merged->selection.size() == 2,
            "Level.mergeSelectedSurfaces: remapped selection");
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();
        const auto addQuad = [&sector](const double x) -> std::int32_t
        {
            const std::int32_t a = sector.addVertex(x, 0.0, 0.0);
            const std::int32_t b = sector.addVertex(x + 1.0, 0.0, 0.0);
            const std::int32_t c = sector.addVertex(x + 1.0, 1.0, 0.0);
            const std::int32_t d = sector.addVertex(x, 1.0, 0.0);
            sed::Surface surface{};
            surface.vertices = { a, b, c, d };
            surface.texVertices.resize(4);
            surface.normal = sed::TVector::up;
            return sector.addSurface(std::move(surface));
        };
        (void)addQuad(0.0);
        (void)addQuad(1.0);
        (void)addQuad(2.0);

        const std::array<sed::SurfaceRef, 3> selection{
            sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 0, 1 },
            sed::SurfaceRef{ 0, 2 }
        };
        const auto merged = sed::level::levelMergeSelectedSurfaces(level, selection);
        check(merged && merged->mergeCount == 2,
            "Level.mergeSelectedSurfaces.chain: reaches fixed point");
        check(merged && merged->selection == std::vector<sed::SurfaceRef>{ { 0, 0 } },
            "Level.mergeSelectedSurfaces.chain: survivor selection");
        check(level.sectors[0].surfaces.size() == 1
                && sed::level::surfaceIsPlanar(level.sectors[0], level.sectors[0].surfaces[0])
                && sed::level::surfaceIsConvex(level.sectors[0], level.sectors[0].surfaces[0]),
            "Level.mergeSelectedSurfaces.chain: valid planar convex output");
    }

    {
        sed::Level level{};
        sed::Sector& bent = level.newSector();
        const std::int32_t a = bent.addVertex(0.0, 0.0, 0.0);
        const std::int32_t b = bent.addVertex(1.0, 0.0, 0.0);
        const std::int32_t c = bent.addVertex(1.0, 1.0, 0.0);
        const std::int32_t d = bent.addVertex(0.0, 1.0, 0.0);
        const std::int32_t e = bent.addVertex(2.0, 0.0, 1.0);
        const std::int32_t f = bent.addVertex(2.0, 1.0, 1.0);
        sed::tests::appendTestSurface(bent, { a, b, c, d });
        sed::tests::appendTestSurface(bent, { b, e, f, c });
        for (sed::Surface& surface : bent.surfaces)
        {
            surface.texVertices.resize(surface.vertices.size());
        }

        sed::Sector& valid = level.newSector();
        const auto addValidQuad = [&valid](const double x)
        {
            const std::int32_t q0 = valid.addVertex(x, 0.0, 0.0);
            const std::int32_t q1 = valid.addVertex(x + 1.0, 0.0, 0.0);
            const std::int32_t q2 = valid.addVertex(x + 1.0, 1.0, 0.0);
            const std::int32_t q3 = valid.addVertex(x, 1.0, 0.0);
            sed::tests::appendTestSurface(valid, { q0, q1, q2, q3 });
            valid.surfaces.back().texVertices.resize(
                valid.surfaces.back().vertices.size());
        };
        addValidQuad(10.0);
        addValidQuad(11.0);

        const std::array<sed::SurfaceRef, 4> selection{
            sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 0, 1 },
            sed::SurfaceRef{ 1, 0 }, sed::SurfaceRef{ 1, 1 }
        };
        const auto merged = sed::level::levelMergeSelectedSurfaces(level, selection);
        check(merged && merged->mergeCount == 1,
            "Level.mergeSelectedSurfaces.invalidSkip: valid independent pair merged");
        check(level.sectors[0].surfaces.size() == 2
                && level.sectors[1].surfaces.size() == 1,
            "Level.mergeSelectedSurfaces.invalidSkip: bent pair retained");
        check(merged && merged->selection.size() == 3,
            "Level.mergeSelectedSurfaces.invalidSkip: rejected surfaces remain selected");

        const std::size_t originalVertices = level.sectors[0].vertices.size();
        const auto rejected = sed::level::levelMergeSurfaces(
            level, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 0, 1 });
        check(!rejected && level.sectors[0].surfaces.size() == 2
                && level.sectors[0].vertices.size() == originalVertices,
            "Level.mergeSurfaces.invalid: transaction leaves input unchanged");
    }

    {
        sed::Level level{};
        level.sectors.push_back(makeTestCubeSector(0.0));
        level.sectors.push_back(makeTestCubeSector(2.0));
        level.sectors.push_back(makeTestCubeSector(4.0));
        for (std::size_t i = 0; i < level.sectors.size(); ++i)
        {
            level.sectors[i].num = static_cast<std::int32_t>(i);
            for (sed::Surface& surface : level.sectors[i].surfaces)
            {
                surface.texVertices.resize(surface.vertices.size());
            }
        }
        check(sed::level::levelSysAdjoinSurfaces(
                  level, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 1, 1 }),
            "Level.mergeSelectedSectors.chain: first adjoin");
        check(sed::level::levelSysAdjoinSurfaces(
                  level, sed::SurfaceRef{ 1, 0 }, sed::SurfaceRef{ 2, 1 }),
            "Level.mergeSelectedSectors.chain: second adjoin");

        level.newThing().sector = 1;
        level.newThing().sector = 2;
        sed::Cog& cog = level.newCog();
        sed::CogValue donorSector{};
        donorSector.type = sed::ValueType::ptr;
        donorSector.cogType = sed::CogType::sec;
        donorSector.intValue = 2;
        donorSector.stringValue = "2";
        cog.values.push_back(donorSector);
        sed::CogValue donorSurface{};
        donorSurface.type = sed::ValueType::ptr;
        donorSurface.cogType = sed::CogType::surface;
        donorSurface.intValue = level.getGlobalSurfaceNumber(2, 2);
        donorSurface.stringValue = "2 2";
        cog.values.push_back(donorSurface);
        sed::CogValue deletedPortal = donorSurface;
        deletedPortal.intValue = level.getGlobalSurfaceNumber(2, 1);
        deletedPortal.stringValue = "2 1";
        cog.values.push_back(deletedPortal);

        const std::array<std::int32_t, 3> selection{ 0, 1, 2 };
        const auto merged = sed::level::levelMergeSelectedSectors(level, selection);
        check(merged && merged->mergeCount == 2,
            "Level.mergeSelectedSectors.chain: reaches fixed point");
        check(merged && merged->selection == std::vector<std::int32_t>{ 0 },
            "Level.mergeSelectedSectors.chain: survivor selection");
        check(level.sectors.size() == 1
                && sed::level::sectorIsConvex(level.sectors[0]),
            "Level.mergeSelectedSectors.chain: valid convex output");
        check(level.things.size() == 2
                && level.things[0].sector == 0
                && level.things[1].sector == 0,
            "Level.mergeSelectedSectors.chain: donor Things follow survivor through chained deletes");
        const auto remappedSurface = level.tryGetSurfaceRefByGlobalSurfaceNumber(
            level.cogs[0].values[1].intValue);
        check(level.cogs[0].values[0].intValue == 0
                && level.cogs[0].values[0].stringValue == "0"
                && remappedSurface && remappedSurface->sector == 0
                && level.cogs[0].values[1].stringValue.empty()
                && level.cogs[0].values[1].asJedString(level).starts_with("0 "),
            "Level.mergeSelectedSectors.chain: Sector and Surface COG refs remap through chain");
        check(level.cogs[0].values[2].intValue == -1
                && level.cogs[0].values[2].stringValue == "-1",
            "Level.mergeSelectedSectors.chain: removed internal portal COG ref cleared coherently");
    }

    {
        sed::Level level{};
        level.sectors.push_back(makeTestCubeSector(10.0));
        level.sectors.push_back(makeTestCubeSector(0.0));
        level.sectors.push_back(makeTestCubeSector(2.0));
        for (std::size_t i = 0; i < level.sectors.size(); ++i)
        {
            level.sectors[i].num = static_cast<std::int32_t>(i);
            for (sed::Surface& surface : level.sectors[i].surfaces)
            {
                surface.texVertices.resize(surface.vertices.size());
            }
        }
        check(sed::level::levelSysAdjoinSurfaces(
                  level, sed::SurfaceRef{ 1, 0 }, sed::SurfaceRef{ 2, 1 }),
            "Level.mergeSelectedSectors.invalidSkip: adjoin");

        const std::array<std::int32_t, 3> selection{ 0, 1, 2 };
        const auto merged = sed::level::levelMergeSelectedSectors(level, selection);
        check(merged && merged->mergeCount == 1,
            "Level.mergeSelectedSectors.invalidSkip: adjacent pair merged");
        check(merged && merged->selection == std::vector<std::int32_t>({ 0, 1 })
                && level.sectors.size() == 2,
            "Level.mergeSelectedSectors.invalidSkip: nonadjacent sector retained and remapped");
    }

    {
        sed::Level level{};
        level.sectors.push_back(makeTestCubeSector(0.0));
        level.sectors.push_back(makeTestCubeSector(2.0));
        for (std::size_t i = 0; i < level.sectors.size(); ++i)
        {
            level.sectors[i].num = static_cast<std::int32_t>(i);
            for (sed::Surface& surface : level.sectors[i].surfaces)
            {
                surface.texVertices.resize(surface.vertices.size());
            }
        }
        check(sed::level::levelSysAdjoinSurfaces(
                  level, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 1, 1 }),
            "Level.mergeSectors.invalid: adjoin");
        // Make a non-portal face genuinely non-planar.  Flipping only the
        // cached Surface::normal is not malformed geometry: production merge
        // validation deliberately derives a fresh Newell normal so stale
        // authored normals cannot reject an otherwise valid sector.
        level.sectors[0].vertices[0].z += 0.5;

        const auto rejected = sed::level::levelMergeSectors(level, 0, 1);
        check(!rejected && level.sectors.size() == 2,
            "Level.mergeSectors.invalid: transaction leaves sector count unchanged");
        check(level.sectors[0].surfaces[0].adjoin
                && level.sectors[0].surfaces[0].adjoin->sector == 1,
            "Level.mergeSectors.invalid: transaction preserves original adjoin");
    }
}

} // namespace sed::tests
