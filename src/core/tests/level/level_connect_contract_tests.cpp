#include "test_common.hpp"

// Synthetic public-API contracts for source admission and exact portal geometry.
#include "sed/assets/sed.hpp"
#include "sed/editor/document.hpp"
#include "sed/level/cleave.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level/merge.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <map>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace
{
using namespace sed;
using sed::tests::check;
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
Level partial(bool roundedInterface, double remoteWarp, double interfaceWarp)
{
    Level result;
    auto first = box(-1, 1, -1, 1, -1, 1);
    first.vertices[7].x -= remoteWarp;
    if ( roundedInterface )
    {
        first.addVertex(1, 0, -1);
        first.addVertex(1, 0, 1);
        first.surfaces[0].vertices = std::vector<std::int32_t>{1, 5, 9, 6, 2, 8};
        first.surfaces[4].vertices = std::vector<std::int32_t>{4, 7, 6, 9, 5};
        first.surfaces[5].vertices = std::vector<std::int32_t>{0, 1, 8, 2, 3};
        // Only the portion outside the overlap is warped. The retained
        // portal y=[0,1] has four existing, exact x=1 boundary corners.
        first.vertices[5].x += interfaceWarp;
    }
    recalc(first);
    append(result, std::move(first));
    append(result, box(1, 3, 0, 1, -1, 1));
    return result;
}
long double portalTurn(const TVector& a, const TVector& b, const TVector& c)
{
    return (static_cast<long double>(b.y) - a.y) * (static_cast<long double>(c.z) - a.z)
        - (static_cast<long double>(b.z) - a.z) * (static_cast<long double>(c.y) - a.y);
}
void verifyPartialPortalCoverage(const Sector& sector)
{
    std::vector<std::vector<TVector>> polygons;
    long double totalArea = 0;
    const std::array<TVector, 4> corners{{{1, 0, -1}, {1, 0, 1}, {1, 1, 1}, {1, 1, -1}}};
    for ( const auto& face : sector.surfaces )
    {
        if ( !face.adjoin )
        {
            continue;
        }
        auto polygon = coordinates(sector, face);
        if ( polygon.size() < 3 )
        {
            continue;
        }
        long double twiceArea = 0;
        for ( std::size_t i = 0; i < polygon.size(); ++i )
        {
            const auto& a = polygon[i];
            const auto& b = polygon[(i + 1) % polygon.size()];
            check(a.x == 1 && a.y >= 0 && a.y <= 1 && a.z >= -1 && a.z <= 1,
                "Partial portal corners lie exactly in the fixed overlap plane and rectangle");
            twiceArea += static_cast<long double>(a.y) * b.z - static_cast<long double>(a.z) * b.y;
        }
        check(std::isfinite(twiceArea) && twiceArea != 0,
            "Every partial portal fragment has nonzero finite area");
        if ( twiceArea < 0 )
        {
            std::reverse(polygon.begin(), polygon.end());
        }
        totalArea += std::abs(twiceArea) / 2;
        for ( std::size_t i = 0; i < polygon.size(); ++i )
        {
            for ( const auto& vertex : polygon )
            {
                check(portalTurn(polygon[i], polygon[(i + 1) % polygon.size()], vertex) >= 0,
                    "Each partial portal fragment is independently convex in the overlap plane");
            }
        }
        polygons.push_back(std::move(polygon));
    }
    // Convex fragments inside the rectangle, with disjoint interiors and its
    // full area, cover the analytic overlap regardless of valid partitioning.
    check(std::abs(totalArea - 2.0L) <= 32 * std::numeric_limits<long double>::epsilon(),
        "Partial portal union has the fixed analytic overlap area two");
    for ( const auto corner : corners )
    {
        check(std::any_of(polygons.begin(), polygons.end(), [&](const auto& polygon)
            { return std::find(polygon.begin(), polygon.end(), corner) != polygon.end(); }),
            "Partial portal union includes every exact fixed outer overlap corner");
    }
    const auto separates = [](const auto& first, const auto& second)
    {
        for ( std::size_t i = 0; i < first.size(); ++i )
        {
            const auto& a = first[i];
            const auto& b = first[(i + 1) % first.size()];
            if ( a == b )
            {
                continue;
            }
            if ( std::all_of(second.begin(), second.end(), [&](const auto& point)
                { return portalTurn(a, b, point) <= 0; }) )
            {
                return true;
            }
        }
        return false;
    };
    for ( std::size_t i = 0; i < polygons.size(); ++i )
    {
        for ( std::size_t j = i + 1; j < polygons.size(); ++j )
        {
            check(separates(polygons[i], polygons[j]) || separates(polygons[j], polygons[i]),
                "Distinct partial portal fragments have disjoint interiors");
        }
    }
}
void runCase(Level input, bool useSurface, const char* label, bool expectedSuccess, bool wholeFace,
             double expectedVolume, double sourceWarp)
{
    const Level before = input;
    const bool accepted = useSurface ? level::levelConnectSurfaces(input, {0, 0}, {1, 1})
                                     : level::levelConnectSectors(input, 0, 1);
    outcome(label, accepted, expectedSuccess);
    if ( !expectedSuccess || !accepted )
    {
        check(assets::sedLevelsEqual(input, before),
              "Rejected Connect preserves complete Level including normals/UVs/links");
        return;
    }
    check(input.sectors.size() == 2, "Connect preserves exactly two original Sector owners");
    if ( input.sectors.size() != 2 )
    {
        return;
    }
    if ( wholeFace )
    {
        Level expected = before;
        link(expected, {0, 0}, {1, 1});
        for ( const SurfaceRef ref : std::array<SurfaceRef, 2>{{{0, 0}, {1, 1}}} )
        {
            auto& face = expected.sectors[ref.sector].surfaces[ref.surface];
            face.surfaceFlags = 0;
            face.faceFlags = 0;
            face.geo = 0;
            face.adjoinFlags = SAF_Visible | SAF_Move | SAF_AllowSoundPass;
        }
        check(assets::sedLevelsEqual(input, expected),
              "Whole-face Connect publishes only the exact reciprocal links/flags");
    }
    const int portalCount = verifyPortals(input);
    if ( wholeFace )
    {
        check(portalCount == 2, "Whole-face Connect creates exactly one reciprocal coincident portal pair");
    }
    else
    {
        check(portalCount >= 2 && portalCount % 2 == 0,
            "Partial Connect creates complete reciprocal portal pairs");
    }
    double volume = 0;
    for ( std::size_t owner = 0; owner < 2; ++owner )
    {
        volume += verifySolid(input.sectors[owner]);
        for ( const auto& vertex : before.sectors[owner].vertices )
        {
            check(std::any_of(input.sectors[owner].vertices.begin(), input.sectors[owner].vertices.end(),
                              [&](const Vertex& actual) { return point(actual) == point(vertex); }),
                  "Connect preserves every original coordinate in its original Sector");
        }
        if ( wholeFace )
        {
            continue;
        }
        verifyPartialPortalCoverage(input.sectors[owner]);
    }
    check(std::abs(volume - expectedVolume) <= 4 * sourceWarp + 1e-9,
          "Connect preserves analytic source volume within its immutable source-warp envelope");
}
void connectCases(bool useSurface)
{
    runCase(adjacent(), useSurface, "Connect exact strict interface", true, true, 16, 0);
    runCase(adjacent(0, 0.00005), useSurface, "Connect exact interface with rounded remote face", true, true,
            16, 0.00005);
    runCase(adjacent(0.00005), useSurface, "Connect exact rounded actual interface", true, true, 16, 0.00005);
    runCase(adjacent(0, 0.01), useSurface, "Connect material-invalid remote face", false, true, 16, 0.01);
    runCase(adjacent(0.01), useSurface, "Connect material-invalid actual interface", false, true, 16, 0.01);
    runCase(adjacent(0, 0, 0.000001), useSurface, "Connect real interface gap", false, true, 16, 0);
    runCase(partial(false, 0, 0), useSurface, "Connect partial strict interface", true, false, 12, 0);
    runCase(partial(false, 0.00005, 0), useSurface, "Connect partial interface with rounded remote face",
            true, false, 12, 0.00005);
    runCase(partial(true, 0, 0.00005), useSurface,
            "Connect partial rounded source with exact overlap boundary", true, false, 12, 0.00005);
    runCase(partial(false, 0.01, 0), useSurface,
            "Connect partial interface with material-invalid remote face", false, false, 12, 0.01);
    runCase(partial(true, 0, 0.01), useSurface, "Connect partial interface with material-invalid source face",
            false, false, 12, 0.01);
}
void invalidTextureCases(const bool useSurface)
{
    for ( const bool selected : {false, true} )
    {
        for ( const int defect : {0, 1, 2} )
        {
            Level input = adjacent();
            auto& face = input.sectors[0].surfaces[selected ? 0 : 1];
            if ( defect == 0 )
            {
                face.texVertices.pop_back();
            }
            else if ( defect == 1 )
            {
                face.texVertices[0].u = std::numeric_limits<float>::infinity();
            }
            else
            {
                face.uScale = std::numeric_limits<double>::infinity();
            }
            const Level before = input;
            const bool accepted = useSurface ? level::levelConnectSurfaces(input, {0, 0}, {1, 1})
                : level::levelConnectSectors(input, 0, 1);
            check(!accepted, "Connect rejects malformed selected and untouched source texture data");
            check(assets::sedLevelsEqual(input, before),
                "Rejected malformed texture Connect preserves the complete source Level");
        }
    }
}
void unsetNormalCases(const bool useSurface)
{
    for ( const bool surfaceOnly : {false, true} )
    {
        for ( const bool selected : {false, true} )
        {
            if ( surfaceOnly && !selected )
            {
                continue;
            }
            Level input = adjacent();
            if ( surfaceOnly )
            {
                Surface first = input.sectors[0].surfaces[0];
                Surface second = input.sectors[1].surfaces[1];
                second.num = 0;
                input.sectors[0].surfaces.clear();
                input.sectors[0].surfaces.push_back(std::move(first));
                input.sectors[1].surfaces.clear();
                input.sectors[1].surfaces.push_back(std::move(second));
            }
            const SurfaceRef first{0, 0};
            const SurfaceRef second{1, surfaceOnly ? 0 : 1};
            const std::size_t unsetFace = selected ? 0 : 1;
            Level expected = input;
            input.sectors[0].surfaces[unsetFace].normal = {};
            if ( !selected )
            {
                expected.sectors[0].surfaces[unsetFace].normal = {};
            }
            link(expected, first, second);
            for ( const auto ref : {first, second} )
            {
                auto& face = expected.sectors[ref.sector].surfaces[ref.surface];
                face.surfaceFlags = 0;
                face.faceFlags = 0;
                face.geo = 0;
                face.adjoinFlags = SAF_Visible | SAF_Move | SAF_AllowSoundPass;
            }
            const bool accepted = useSurface ? level::levelConnectSurfaces(input, first, second)
                                             : level::levelConnectSectors(input, 0, 1);
            check(accepted,
                  "Public Connect accepts existing unset-normal solid and standalone-face contracts");
            // Surface Connect historically resolves the selected cache while
            // the Sector exact-adjoin shortcut leaves even that cache unset.
            if ( selected && !useSurface )
            {
                expected.sectors[0].surfaces[unsetFace].normal = {};
            }
            check(assets::sedLevelsEqual(input, expected),
                  "Unset-normal Connect publishes the exact established geometry/cache/link state");
        }
    }
}

void expectConnectCogValues(const Level& level, const bool connected, const std::string_view originalText)
{
    const bool valid = level.cogs.size() == 1 && level.cogs[0].values.size() == 3;
    check(valid, "Public Connect preserves the complete fixed COG value collection");
    if ( !valid )
    {
        return;
    }
    const std::array<std::string_view, 3> names{"later_face", "source_bottom", "later_sector"};
    const std::array<std::string_view, 3> descriptions{
        "Unedited later face.", "Original source bottom.", "Unedited later sector."};
    const std::array<double, 3> scalars{-17.125, 29.5, -0.25};
    const std::array<std::int32_t, 3> masks{0x123, 0x456, 0x789};
    for ( std::size_t i = 0; i < 3; ++i )
    {
        const auto& value = level.cogs[0].values[i];
        check(value.name == names[i] && value.floatValue == scalars[i]
                  && value.local == (i != 1) && value.description == descriptions[i] && value.mask == masks[i],
              "Public Connect retains every independently fixed non-reference CogValue field");
        const std::int32_t integer = i == 0 ? (connected ? 17 : 16) : i == 1 ? 5 : 2;
        const std::string_view global = i == 0 ? (connected ? "17" : "16") : i == 1 ? "5" : "2";
        const std::string_view jed = i == 0 ? "2 4" : i == 1 ? "0 5" : "2";
        const std::string_view stored = i == 0 ? (connected ? "" : originalText) : i == 1 ? "0 5" : "2";
        check(value.type == ValueType::ptr && value.cogType == (i == 2 ? CogType::sec : CogType::surface)
                  && value.intValue == integer && value.stringValue == stored,
              "Public Connect retains the complete fixed pointer type, target and stored text");
        check(value.asString() == global && value.asJedString(level) == jed,
              "Public Connect emits complete fixed global and JED reference tokens");
    }
}
void cogPublicationCases(const bool useSurface)
{
    for ( const std::string_view text : {"", "16", "2 4"} )
    {
        for ( const bool reject : {false, true} )
        {
            editor::Document document;
            document.level() = partial(false, reject ? .01 : 0, 0);
            append(document.level(), box(6, 8, -1, 1, -1, 1));
            Cog cog;
            const std::array<std::string_view, 3> names{"later_face", "source_bottom", "later_sector"};
            const std::array<std::string_view, 3> descriptions{
                "Unedited later face.", "Original source bottom.", "Unedited later sector."};
            const std::array<double, 3> scalars{-17.125, 29.5, -0.25};
            const std::array<std::int32_t, 3> masks{0x123, 0x456, 0x789};
            for ( std::size_t i = 0; i < 3; ++i )
            {
                CogValue value;
                value.name = names[i];
                value.type = ValueType::ptr;
                value.cogType = i == 2 ? CogType::sec : CogType::surface;
                value.intValue = i == 0 ? 16 : i == 1 ? 5 : 2;
                value.stringValue = i == 0 ? text : i == 1 ? "0 5" : "2";
                value.floatValue = scalars[i];
                value.local = i != 1;
                value.description = descriptions[i];
                value.mask = masks[i];
                cog.values.push_back(std::move(value));
            }
            document.level().cogs.push_back(std::move(cog));
            const Level before = document.level();
            expectConnectCogValues(before, false, text);
            if ( !reject )
            {
                document.pushUndo("Connect partial faces", editor::DocumentDirty::All);
            }
            const bool accepted = useSurface
                ? level::levelConnectSurfaces(document.level(), {0, 0}, {1, 1})
                : level::levelConnectSectors(document.level(), 0, 1);
            check(accepted == !reject, "Public Connect returns the fixed COG fixture acceptance result");
            if ( reject || !accepted )
            {
                check(assets::sedLevelsEqual(document.level(), before),
                      "Failed public Connect preserves geometry and every authored COG field");
                expectConnectCogValues(document.level(), false, text);
                continue;
            }
            // Clipping the source quad at y=0 appends one retained face. The
            // contained destination and distant third owner each keep six faces.
            check(document.level().sectors.size() == 3
                      && document.level().sectors[0].surfaces.size() == 7
                      && document.level().sectors[1].surfaces.size() == 6
                      && document.level().sectors[2].surfaces.size() == 6,
                  "Partial Connect has the independently fixed 7/6/6 face layout");
            expectConnectCogValues(document.level(), true, text);
            const Level committed = document.level();
            const auto restored = assets::loadFromSedBytes(assets::saveToSedBytes(committed));
            expectConnectCogValues(restored, true, text);
            check(document.undo() && assets::sedLevelsEqual(document.level(), before),
                  "Undo public Connect restores complete geometry and all authored COG fields");
            expectConnectCogValues(document.level(), false, text);
            check(document.redo() && assets::sedLevelsEqual(document.level(), committed),
                  "Redo public Connect restores complete geometry and all committed COG fields");
            expectConnectCogValues(document.level(), true, text);
        }
    }
}

} // namespace
namespace sed::tests
{
void runLevelConnectContractTests()
{
    cogPublicationCases(true);
    cogPublicationCases(false);
    connectCases(true);
    connectCases(false);
    invalidTextureCases(true);
    invalidTextureCases(false);
    unsetNormalCases(true);
    unsetNormalCases(false);
}
} // namespace sed::tests
