#include "test_common.hpp"

#include "sed/assets/sed.hpp"
#include "sed/level/cleave.hpp"
#include "sed/level/merge.hpp"

#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <map>
#include <new>
#include <optional>
#include <string_view>

namespace
{
thread_local std::optional<std::size_t> allocationsBeforeFailure;
thread_local bool allocationFailureFired = false;
std::size_t currentOrdinal = 0;
bool currentAdjoined = false;

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
}

void* operator new(std::size_t bytes) { return allocate(bytes); }
void* operator new[](std::size_t bytes) { return allocate(bytes); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace
{
using sed::tests::check;
using Result = sed::level::SectorCleaveBatchResult;
using Error = sed::level::SectorCleaveBatchError;
std::optional<sed::SurfaceRef> mergeOutput;

sed::Level fixture(bool adjoined, int mode)
{
    sed::Level level;
    level.layers = {"Cleave allocation layer"};
    level.sectors.push_back(sed::tests::makeLitTestCubeSector(0.0, sed::ColorWhite));
    if ( (mode >= 4 && mode <= 6) || mode == 9 )
    {
        level.sectors.push_back(sed::tests::makeLitTestCubeSector(mode == 4 || mode == 9 ? 2.0 : 1.0, sed::ColorWhite));
        if ( mode == 9 )
        {
            // The source face covers only the middle of the target face, so
            // exact matching must fail and auto-adjoin must split the target.
            for ( auto& vertex : level.sectors[1].vertices )
            {
                vertex.y *= 2;
                vertex.z *= 2;
            }
        }
        if ( adjoined )
        {
            level.sectors.push_back(sed::tests::makeLitTestCubeSector(-2.0, sed::ColorWhite));
            level.sectors[0].surfaces[1].adjoin = sed::SurfaceRef{2, 0};
            level.sectors[2].surfaces[0].adjoin = sed::SurfaceRef{0, 1};
        }
    }
    else if ( adjoined )
    {
        level.sectors.push_back(sed::tests::makeLitTestCubeSector(2.0, sed::ColorWhite));
        level.sectors[0].surfaces[0].adjoin = sed::SurfaceRef{1, 1};
        level.sectors[1].surfaces[1].adjoin = sed::SurfaceRef{0, 0};
    }
    for ( auto& sector : level.sectors )
    {
        sector.layer = 0;
        sector.sound = "cleave_owner_metadata_kept_across_allocation_failure.wav";
        for ( auto& surface : sector.surfaces )
        {
            surface.material = "allocation_contract_material_long_enough_to_allocate.mat";
        }
    }
    if ( mode == 7 )
    {
        auto& source = level.sectors[0].surfaces[1];
        source.surfaceFlags = 11;
        source.faceFlags = 12;
        source.geo = 13;
        source.adjoinFlags = 14;
        std::string shortMaterial = "x";
        level.sectors[0].surfaces[0].material.swap(shortMaterial);
    }
    if ( mode == 8 )
    {
        check(sed::level::levelCleaveSurface(level, {0, 0}, {0, 0, 1}, 0, 0, 0),
            "Mirrored merge fault fixture splits a valid reciprocal cube portal");
        level.sectors[0].surfaces[0].material = "merged_target_material_long_enough_to_allocate.mat";
        level.sectors[0].surfaces[6].material = "x";
    }
    // Native SED bytes do not carry these runtime fields. Seed them after the
    // mirrored fixture split, then compare them separately on every failure.
    for ( std::size_t owner = 0; owner < level.sectors.size(); ++owner )
    {
        auto& sector = level.sectors[owner];
        sector.mark = 101 + static_cast<std::int32_t>(owner);
        for ( std::size_t vertex = 0; vertex < sector.vertices.size(); ++vertex )
        {
            sector.vertices[vertex].mark = 201 + static_cast<std::int32_t>(vertex);
        }
        for ( std::size_t face = 0; face < sector.surfaces.size(); ++face )
        {
            auto& surface = sector.surfaces[face];
            surface.mark = 301 + static_cast<std::int32_t>(face);
            surface.d3dId = 401 + static_cast<std::int32_t>(face);
            surface.nAdj = 501 + static_cast<std::int32_t>(face);
            surface.nMat = 601 + static_cast<std::int32_t>(face);
        }
    }
    auto& thing = level.newThing();
    thing.name = "unrelated_thing_metadata";
    thing.x = 0.25; thing.y = 0.25; thing.z = 0.5; thing.sector = 0;
    sed::Cog cog;
    cog.name = "geometry_reference_capture_allocation_fixture.cog";
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
    return level;
}

bool sameRuntimeState(const sed::Level& actual, const sed::Level& expected)
{
    if ( actual.curId != expected.curId || actual.sectors.size() != expected.sectors.size() )
    {
        return false;
    }
    const auto sameDouble = [](double a, double b)
    {
        return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
    };
    for ( std::size_t owner = 0; owner < actual.sectors.size(); ++owner )
    {
        const auto& a = actual.sectors[owner];
        const auto& b = expected.sectors[owner];
        if ( a.id != b.id || a.num != b.num || a.mark != b.mark
            || a.vertices.size() != b.vertices.size() || a.surfaces.size() != b.surfaces.size() )
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
            if ( x.num != y.num || x.mark != y.mark || x.d3dId != y.d3dId
                || x.nAdj != y.nAdj || x.nMat != y.nMat
                || !sameDouble(x.normal.x, y.normal.x) || !sameDouble(x.normal.y, y.normal.y)
                || !sameDouble(x.normal.z, y.normal.z) )
            {
                return false;
            }
        }
    }
    return true;
}

Result sentinel()
{
    Result result;
    result.error = Error::InvalidPlane;
    result.failedPlane = 71;
    result.failedSector = 72;
    result.failedSurface = 73;
    result.outputSectors = {74, 75};
    return result;
}

bool sameSentinel(const Result& result)
{
    return result.error == Error::InvalidPlane && result.failedPlane == 71
        && result.failedSector == 72 && result.failedSurface == 73
        && result.outputSectors == std::vector<std::int32_t>{74, 75};
}

bool cleave(sed::Level& level, int mode, Result& result)
{
    switch ( mode )
    {
    case 0: return sed::level::levelCleaveSurface(level, {0, 0}, {0, 0, 1}, 0, 0, 0);
    case 1: return sed::level::levelCleaveEdge(level, {0, 0}, 0, {0, 0, 1}, 0, 0, 0);
    case 2: return sed::level::levelCleaveSector(level, 0, {0, 0, 1}, 0, 0, 0);
    case 3:
    {
        const std::array<sed::level::SectorCleavePlane, 1> planes{{{{0, 0, 1}, {0, 0, 0}}}};
        result = sed::level::levelCleaveSectorByPlanes(level, 0, planes);
        return result.succeeded();
    }
    case 12:
    {
        const std::array<sed::level::SectorCleavePlane, 1> planes{{{{0, 0, 1}, {0, 0, 0}}}};
        const std::array<sed::level::SectorCleaveStroke, 1> strokes{{{{-0.5, 0, 0}, {0.5, 0, 0}}}};
        sed::level::SectorCleaveBatchOptions options{};
        options.strokes = strokes;
        options.bounded = true;
        result = sed::level::levelCleaveSectorByPlanes(level, 0, planes, options);
        return result.succeeded();
    }
    case 4: return sed::level::levelConnectSectors(level, 0, 1);
    case 5: return sed::level::levelConnectSectorsKnife(level, 0, 1,
        sed::level::SectorConnectKnifeMode::KeepTargetPieces);
    case 6: return sed::level::levelConnectSectorsKnife(level, 0, 1,
        sed::level::SectorConnectKnifeMode::CutTargetHolesOnly);
    case 7:
        sed::level::surfaceCopyAdjoinParams(level.sectors[0].surfaces[1], level.sectors[0].surfaces[0]);
        return true;
    case 8:
        mergeOutput = sed::level::levelMergeSurfaces(level, {0, 0}, {0, 6});
        return mergeOutput.has_value();
    case 9: return sed::level::levelMakeAdjoinFromSector(level, {0, 0}, 1, true);
    case 11: return sed::level::surfaceBoundariesMatchForDiagnostics(level.sectors[0], level.sectors[0].surfaces[0],
        level.sectors[1], level.sectors[1].surfaces[1]);
    }
    return false;
}

void checkSuccess(const sed::Level& level, int mode, bool adjoined, const Result& result)
{
    const std::size_t initialSectors = adjoined ? 2 : 1;
    const std::size_t expectedCount = mode == 4 || mode == 6 || mode == 9 ? (adjoined ? 3 : 2)
        : mode == 5 ? (adjoined ? 4 : 3)
        : initialSectors + (mode == 2 || mode == 3 || mode == 12 ? 1 : 0);
    check(level.sectors.size() == expectedCount,
        "Cleave success returns fixed sector count for selected operation");
    if ( mode == 3 || mode == 12 )
    {
        check(result.error == Error::None && result.failedPlane == -1 && result.failedSector == -1
                && result.failedSurface == -1 && result.outputSectors == std::vector<std::int32_t>{0,
                    static_cast<std::int32_t>(initialSectors)},
            "Batch Cleave success returns every fixed result field and ordered descendant");
    }
    else
    {
        check(sameSentinel(result), "Non-batch Cleave success preserves unrelated result OUT sentinels");
    }
    if ( mode == 7 )
    {
        const auto& copied = level.sectors[0].surfaces[0];
        check(copied.surfaceFlags == 11 && copied.faceFlags == 12 && copied.geo == 13
                && copied.adjoinFlags == 14
                && copied.material == "allocation_contract_material_long_enough_to_allocate.mat",
            "Adjoin parameter helper returns every complete fixed copied field");
        sed::Level expected = fixture(adjoined, mode);
        auto& destination = expected.sectors[0].surfaces[0];
        destination.surfaceFlags = 11;
        destination.faceFlags = 12;
        destination.geo = 13;
        destination.adjoinFlags = 14;
        destination.material = "allocation_contract_material_long_enough_to_allocate.mat";
        check(sed::assets::saveToSedBytes(level) == sed::assets::saveToSedBytes(expected)
                && sameRuntimeState(level, expected),
            "Adjoin parameter helper preserves every field outside the five copied parameters");
    }
    if ( mode == 8 )
    {
        check(mergeOutput == std::optional<sed::SurfaceRef>{{0, 0}}
                && level.sectors[0].surfaces.size() == 6 && level.sectors[1].surfaces.size() == 6
                && level.sectors[0].surfaces[0].material == "merged_target_material_long_enough_to_allocate.mat",
            "Mirrored merge publishes fixed surviving ref, symmetric surface counts and active material");
    }
    long double volume = 0.0L;
    std::size_t portals = 0;
    for ( std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex )
    {
        const auto& sector = level.sectors[sectorIndex];
        using Point = std::array<double, 3>;
        std::map<std::pair<Point, Point>, std::pair<int, int>> edges;
        const auto point = [](const sed::Vertex& vertex) { return Point{vertex.x, vertex.y, vertex.z}; };
        long double sectorVolume = 0.0L;
        for ( std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex )
        {
            const auto& surface = sector.surfaces[surfaceIndex];
            check(surface.vertices.size() >= 3 && surface.vertices.size() == surface.texVertices.size(),
                "Cleave success retains complete polygon and UV corner tuples");
            const bool validIndices = std::all_of(surface.vertices.begin(), surface.vertices.end(),
                [&](std::int32_t index)
                {
                    return index >= 0 && static_cast<std::size_t>(index) < sector.vertices.size();
                });
            check(validIndices, "Cleave success polygon references only existing sector vertices");
            if ( surface.vertices.size() < 3 || !validIndices ) continue;
            const auto& anchor = sector.vertices[static_cast<std::size_t>(surface.vertices[0])];
            for ( std::size_t corner = 0; corner < surface.vertices.size(); ++corner )
            {
                const auto a = surface.vertices[corner];
                const auto b = surface.vertices[(corner + 1) % surface.vertices.size()];
                const auto& vertex = sector.vertices[static_cast<std::size_t>(a)];
                const auto& endpoint = sector.vertices[static_cast<std::size_t>(b)];
                // Every fixture coordinate is an exact small integer. Split
                // collinear edges at actual vertex positions without using any
                // production geometry tolerance or closure helper.
                const Point start = point(vertex), finish = point(endpoint);
                Point delta{finish[0] - start[0], finish[1] - start[1], finish[2] - start[2]};
                std::size_t axis = 0;
                for ( std::size_t component = 1; component < 3; ++component )
                {
                    if ( std::abs(delta[component]) > std::abs(delta[axis]) )
                    {
                        axis = component;
                    }
                }
                std::vector<std::pair<double, Point>> segments{{0, start}, {1, finish}};
                if ( delta[axis] != 0 )
                {
                    for ( const auto& candidate : sector.vertices )
                    {
                        const Point q = point(candidate);
                        const double t = (q[axis] - start[axis]) / delta[axis];
                        if ( t <= 0 || t >= 1 )
                        {
                            continue;
                        }
                        if ( q[0] == start[0] + t * delta[0] && q[1] == start[1] + t * delta[1]
                            && q[2] == start[2] + t * delta[2] )
                        {
                            segments.emplace_back(t, q);
                        }
                    }
                }
                std::sort(segments.begin(), segments.end());
                segments.erase(std::unique(segments.begin(), segments.end()), segments.end());
                for ( std::size_t segment = 1; segment < segments.size(); ++segment )
                {
                    const auto& first = segments[segment - 1].second;
                    const auto& last = segments[segment].second;
                    auto& incidence = edges[std::minmax(first, last)];
                    ++incidence.first;
                    incidence.second += first < last ? 1 : -1;
                }
                const bool validX = (mode >= 4 && mode <= 6) || mode == 9
                    ? (vertex.x == -3 || vertex.x == -1 || vertex.x == 0 || vertex.x == 1 || vertex.x == 2 || vertex.x == 3)
                    : (vertex.x == -1 || vertex.x == 1 || vertex.x == 3);
                check(validX
                        && (vertex.y == -1 || vertex.y == 1 || (mode == 9 && (vertex.y == -2 || vertex.y == 2)))
                        && (vertex.z == -1 || vertex.z == 0 || vertex.z == 1 || (mode == 9 && (vertex.z == -2 || vertex.z == 2))),
                    "Cleave success uses only independently known cube and exact cut coordinates");
                if ( corner >= 1 && corner + 1 < surface.vertices.size() )
                {
                    const auto& next = sector.vertices[static_cast<std::size_t>(surface.vertices[corner + 1])];
                    sectorVolume += static_cast<long double>(anchor.x) * (vertex.y * next.z - vertex.z * next.y)
                        + static_cast<long double>(anchor.y) * (vertex.z * next.x - vertex.x * next.z)
                        + static_cast<long double>(anchor.z) * (vertex.x * next.y - vertex.y * next.x);
                }
            }
            if ( surface.adjoin )
            {
                ++portals;
                const auto ref = *surface.adjoin;
                const bool valid = ref.sector >= 0 && ref.surface >= 0
                    && static_cast<std::size_t>(ref.sector) < level.sectors.size()
                    && static_cast<std::size_t>(ref.surface) < level.sectors[ref.sector].surfaces.size();
                check(valid, "Cleave success portal reference addresses an existing surface");
                if ( valid )
                {
                    check(level.sectors[ref.sector].surfaces[ref.surface].adjoin
                            == std::optional<sed::SurfaceRef>{{static_cast<std::int32_t>(sectorIndex),
                                static_cast<std::int32_t>(surfaceIndex)}},
                        "Cleave success retains exact reciprocal portal owner reference");
                }
            }
        }
        check(std::all_of(edges.begin(), edges.end(), [](const auto& edge)
                { return edge.second.first == 2 && edge.second.second == 0; }),
            "Cleave success independently preserves closed oppositely oriented edge incidence");
        volume += std::abs(sectorVolume) / 6.0L;
    }
    const std::size_t expectedPortals = mode == 4 || mode == 6 || mode == 9 ? (adjoined ? 4 : 2)
        : mode == 5 ? (adjoined ? 6 : 4)
        : mode == 2 || mode == 3 || mode == 12 ? (adjoined ? 6 : 2)
        : (adjoined ? (mode == 0 ? 4 : 2) : 0);
    check(portals == expectedPortals, "Cleave success retains fixed portal pair count");
    const long double expectedVolume = mode == 9 ? (adjoined ? 48.0L : 40.0L)
        : mode == 4 ? (adjoined ? 24.0L : 16.0L)
        : mode == 5 || mode == 6 ? (adjoined ? 20.0L : 12.0L)
        : (adjoined ? 16.0L : 8.0L);
    check(std::abs(volume - expectedVolume) < 1e-12L,
        "Cleave success independently preserves exact analytic cube volume");
    check(level.layers == std::vector<std::string>{"Cleave allocation layer"}
            && level.things[0].name == "unrelated_thing_metadata",
        "Cleave success preserves fixed authored layer and Thing text");
}
void checkBatchConnectionAllocationContract()
{
    sed::Level original = fixture(true, 4);
    original.sectors[0].surfaces[1].adjoin.reset();
    original.sectors[2].surfaces[0].adjoin.reset();
    std::array<sed::Level, 3> expected{{original, original, original}};
    const auto setPortal = [](sed::Level& level, sed::SurfaceRef a, sed::SurfaceRef b)
    {
        // Fixed JK portal parameters, independently of the production helper.
        for ( const auto pair : {std::pair{a, b}, std::pair{b, a}} )
        {
            auto& face = level.sectors[pair.first.sector].surfaces[pair.first.surface];
            face.adjoin = pair.second;
            face.nAdj = -1;
            face.surfaceFlags = 0;
            face.faceFlags = 0;
            face.geo = 0;
            face.adjoinFlags = 7;
        }
    };
    setPortal(expected[1], {0, 0}, {1, 1});
    expected[2] = expected[1];
    setPortal(expected[2], {0, 1}, {2, 0});
    const std::array expectedBytes{sed::assets::saveToSedBytes(expected[0]),
        sed::assets::saveToSedBytes(expected[1]), sed::assets::saveToSedBytes(expected[2])};
    const std::vector<std::int32_t> sources{0};
    const std::vector<std::int32_t> targets{1, 2};
    bool reachedSuccess = false;
    bool sawPartialSuccess = false;
    for ( std::size_t position = 0; position < 4096; ++position )
    {
        sed::Level level = original;
        allocationFailureFired = false;
        currentOrdinal = position;
        allocationsBeforeFailure = position;
        const bool accepted = sed::level::levelConnectSectorSurfaceCandidates(level, sources, targets);
        allocationsBeforeFailure.reset();
        if ( level.sectors.size() != 3 || level.sectors[0].surfaces.size() != 6 )
        {
            check(false, "Multi-target Join preserves exact fixture sector and face counts");
            break;
        }
        const std::size_t count = (level.sectors[0].surfaces[0].adjoin ? 1 : 0)
            + (level.sectors[0].surfaces[1].adjoin ? 1 : 0);
        check(accepted == (count != 0),
            "Multi-target Join result reports whether any complete connection was committed");
        check(sed::assets::saveToSedBytes(level) == expectedBytes[count]
                && sameRuntimeState(level, expected[count]),
            "Multi-target Join publishes only independently fixed complete portal states");
        if ( allocationFailureFired )
        {
            sawPartialSuccess = sawPartialSuccess || count == 1;
        }
        else
        {
            check(accepted && count == 2, "No-fault multi-target Join commits both exact reciprocal portals");
            reachedSuccess = true;
            break;
        }
    }
    check(reachedSuccess && sawPartialSuccess,
        "Multi-target Join fault sweep retains an earlier successful connection when a later allocation fails");
}

}

int main(int argc, char** argv)
{
    constexpr std::array<std::string_view, 13> modes{{"surface", "edge", "sector", "batch",
        "join", "knife-keep", "knife-cut", "copy-adjoin-params", "merge-mirrored", "auto-adjoin", "join-batch",
        "diagnostic-boundary", "bounded-batch"}};
    if ( argc != 2 ) return 2;
    const auto found = std::find(modes.begin(), modes.end(), std::string_view(argv[1]));
    if ( found == modes.end() ) return 2;
    const int mode = static_cast<int>(found - modes.begin());
    std::set_terminate([]
    {
        std::fprintf(stderr, "Cleave terminated at allocation %zu, adjoined=%d\n", currentOrdinal,
            static_cast<int>(currentAdjoined));
        std::_Exit(86);
    });
    if ( mode == 10 )
    {
        checkBatchConnectionAllocationContract();
        return sed::tests::failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    for ( const bool adjoined : {false, true} )
    {
        if ( (mode == 8 || mode == 11) && !adjoined )
        {
            continue;
        }
        bool reachedSuccess = false;
        std::size_t rejected = 0;
        for ( std::size_t position = 0; position < 4096; ++position )
        {
            sed::Level level = fixture(adjoined, mode);
            const sed::Level runtimeBefore = level;
            const auto before = sed::assets::saveToSedBytes(level);
            Result result = sentinel();
            mergeOutput = sed::SurfaceRef{71, 72};
            bool accepted = false, caught = false;
            allocationFailureFired = false;
            currentOrdinal = position;
            currentAdjoined = adjoined;
            allocationsBeforeFailure = position;
            try { accepted = cleave(level, mode, result); }
            catch ( const std::bad_alloc& ) { caught = true; }
            allocationsBeforeFailure.reset();
            if ( allocationFailureFired )
            {
                ++rejected;
                check(!accepted, "Cleave allocation failure cannot report a successful partial edit");
                check(sed::assets::saveToSedBytes(level) == before,
                    "Geometry allocation failure preserves all native authored Level bytes");
                check(sameRuntimeState(level, runtimeBefore),
                    "Geometry allocation failure preserves marks, renderer IDs, caches and allocation IDs");
                if ( mode == 8 && caught )
                {
                    check(mergeOutput == std::optional<sed::SurfaceRef>{{71, 72}},
                        "Throwing mirrored merge preserves caller survivor OUT sentinel");
                }
                if ( (mode == 3 || mode == 12) && !caught )
                {
                    check(result.error == Error::CleaveFailed && result.failedPlane == -1
                            && result.failedSector == 0 && result.failedSurface == -1 && result.outputSectors.empty(),
                        "Batch allocation rejection returns fixed diagnostic fields and empty descendants");
                }
                else
                {
                    check(sameSentinel(result), "Cleave failure leaves unrelated or throwing result OUT sentinels unchanged");
                }
            }
            else
            {
                check(!caught && accepted, "Cleave allocation sweep reaches a real no-fault success");
                if ( accepted ) checkSuccess(level, mode, adjoined, result);
                if ( mode == 11 )
                {
                    check(sed::assets::saveToSedBytes(level) == before && sameRuntimeState(level, runtimeBefore),
                        "Diagnostic boundary success preserves every authored and transient input field");
                }
                reachedSuccess = true;
                break;
            }
        }
        check(rejected > 0 && reachedSuccess, "Cleave sweep visits allocation failures and successful complete publication");
    }
    return sed::tests::failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
