#include "test_common.hpp"

#include "sed/assets/sed.hpp"
#include "sed/level/transform.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <optional>
#include <string_view>
#include <vector>

namespace
{
thread_local std::optional<std::size_t> allocationsBeforeFailure;
thread_local bool allocationFailureFired = false;
thread_local std::size_t currentFailurePosition = 0;

void* allocate(const std::size_t bytes)
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
    if ( void* const memory = std::malloc(bytes == 0 ? 1 : bytes) )
    {
        return memory;
    }
    throw std::bad_alloc();
}
}

void* operator new(const std::size_t bytes) { return allocate(bytes); }
void* operator new[](const std::size_t bytes) { return allocate(bytes); }
void operator delete(void* const memory) noexcept { std::free(memory); }
void operator delete[](void* const memory) noexcept { std::free(memory); }
void operator delete(void* const memory, const std::size_t) noexcept { std::free(memory); }
void operator delete[](void* const memory, const std::size_t) noexcept { std::free(memory); }

namespace
{
using sed::tests::check;

enum class Mode : std::size_t
{
    recalc,
    translateSectors,
    translateSurfaces,
    translateEdges,
    translateVertices,
    rotateSectors,
    rotateSurfaces,
    rotateEdges,
    scaleSectors,
    scaleSurfaces,
    flipSectorsAxis,
    flipSectorsPlane,
    duplicateSector,
};

constexpr std::array<std::string_view, 13> kModeNames{{
    "recalc",
    "translate-sectors",
    "translate-surfaces",
    "translate-edges",
    "translate-vertices",
    "rotate-sectors",
    "rotate-surfaces",
    "rotate-edges",
    "scale-sectors-textures",
    "scale-surfaces-textures",
    "flip-sectors-axis",
    "flip-sectors-plane",
    "duplicate-sector",
}};

sed::Level fixture()
{
    sed::Level level;
    level.layers = {"Geometry transform allocation layer"};
    level.sectors.push_back(sed::tests::makeLitTestCubeSector(0.0, sed::ColorWhite));
    level.sectors.push_back(sed::tests::makeLitTestCubeSector(4.0, sed::ColorWhite));
    level.sectors.push_back(sed::tests::makeLitTestCubeSector(8.0, sed::ColorWhite));

    level.sectors[0].surfaces[0].adjoin = sed::SurfaceRef{1, 1};
    level.sectors[1].surfaces[1].adjoin = sed::SurfaceRef{0, 0};
    level.sectors[0].surfaces[2].adjoin = sed::SurfaceRef{2, 3};
    level.sectors[2].surfaces[3].adjoin = sed::SurfaceRef{0, 2};

    for ( std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex )
    {
        sed::Sector& sector = level.sectors[sectorIndex];
        sector.id = 101 + static_cast<std::int32_t>(sectorIndex);
        sector.num = 201 + static_cast<std::int32_t>(sectorIndex);
        sector.mark = 301 + static_cast<std::int32_t>(sectorIndex);
        sector.sound = "geometry_transform_allocation_sector_" + std::to_string(sectorIndex);
        for ( std::size_t vertexIndex = 0; vertexIndex < sector.vertices.size(); ++vertexIndex )
        {
            sector.vertices[vertexIndex].mark = 401 + static_cast<std::int32_t>(vertexIndex);
        }
        for ( std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex )
        {
            sed::Surface& surface = sector.surfaces[surfaceIndex];
            surface.mark = 501 + static_cast<std::int32_t>(surfaceIndex);
            surface.d3dId = 601 + static_cast<std::int32_t>(surfaceIndex);
            surface.nAdj = 701 + static_cast<std::int32_t>(surfaceIndex);
            surface.nMat = 801 + static_cast<std::int32_t>(surfaceIndex);
            surface.normal = sed::TVector{
                0.125 + static_cast<double>(surfaceIndex),
                0.25 + static_cast<double>(sectorIndex),
                -0.375};
            surface.uScale = 1.25f + static_cast<float>(surfaceIndex) * 0.125f;
            surface.vScale = -0.75f - static_cast<float>(sectorIndex) * 0.125f;
            surface.material = "geometry_transform_allocation_material.mat";
        }
    }
    return level;
}

bool sameRuntimeState(const sed::Level& actual, const sed::Level& expected)
{
    if ( actual.curId != expected.curId || actual.sectors.size() != expected.sectors.size() )
    {
        return false;
    }

    const auto sameDouble = [](const double left, const double right)
    {
        return std::bit_cast<std::uint64_t>(left) == std::bit_cast<std::uint64_t>(right);
    };
    for ( std::size_t sectorIndex = 0; sectorIndex < actual.sectors.size(); ++sectorIndex )
    {
        const sed::Sector& left = actual.sectors[sectorIndex];
        const sed::Sector& right = expected.sectors[sectorIndex];
        if ( left.id != right.id || left.num != right.num || left.mark != right.mark ||
            left.vertices.size() != right.vertices.size() || left.surfaces.size() != right.surfaces.size() )
        {
            return false;
        }
        for ( std::size_t vertexIndex = 0; vertexIndex < left.vertices.size(); ++vertexIndex )
        {
            if ( left.vertices[vertexIndex].mark != right.vertices[vertexIndex].mark )
            {
                return false;
            }
        }
        for ( std::size_t surfaceIndex = 0; surfaceIndex < left.surfaces.size(); ++surfaceIndex )
        {
            const sed::Surface& a = left.surfaces[surfaceIndex];
            const sed::Surface& b = right.surfaces[surfaceIndex];
            if ( a.num != b.num || a.mark != b.mark || a.d3dId != b.d3dId ||
                a.nAdj != b.nAdj || a.nMat != b.nMat ||
                !sameDouble(a.normal.x, b.normal.x) ||
                !sameDouble(a.normal.y, b.normal.y) ||
                !sameDouble(a.normal.z, b.normal.z) )
            {
                return false;
            }
        }
    }
    return true;
}

bool exactLevel(const sed::Level& actual, const sed::Level& expected, const std::vector<std::byte>& expectedBytes)
{
    return sed::assets::saveToSedBytes(actual) == expectedBytes &&
           sed::assets::sedLevelsEqual(actual, expected) &&
           sameRuntimeState(actual, expected);
}

std::vector<std::vector<bool>> expectedVisitedVertices(
    const sed::Level& level,
    const Mode mode)
{
    std::vector<std::vector<bool>> visited{};
    visited.reserve(level.sectors.size());
    for ( const sed::Sector& sector : level.sectors )
    {
        visited.emplace_back(sector.vertices.size(), false);
    }

    const auto markSurface = [&level, &visited](const sed::SurfaceRef ref)
    {
        if ( ref.sector < 0 || ref.surface < 0 ||
            static_cast<std::size_t>(ref.sector) >= level.sectors.size() )
        {
            return;
        }
        const sed::Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
        if ( static_cast<std::size_t>(ref.surface) >= sector.surfaces.size() )
        {
            return;
        }
        for ( const std::int32_t vertex : sector.surfaces[static_cast<std::size_t>(ref.surface)].vertices )
        {
            if ( vertex >= 0 && static_cast<std::size_t>(vertex) < visited[static_cast<std::size_t>(ref.sector)].size() )
            {
                visited[static_cast<std::size_t>(ref.sector)][static_cast<std::size_t>(vertex)] = true;
            }
        }
    };

    switch ( mode )
    {
    case Mode::translateSectors:
    case Mode::rotateSectors:
    case Mode::scaleSectors:
        std::fill(visited[0].begin(), visited[0].end(), true);
        for ( const sed::Surface& surface : level.sectors[0].surfaces )
        {
            if ( surface.adjoin )
            {
                markSurface(*surface.adjoin);
            }
        }
        break;
    case Mode::translateSurfaces:
    case Mode::rotateSurfaces:
    case Mode::scaleSurfaces:
        markSurface({0, 0});
        if ( level.sectors[0].surfaces[0].adjoin )
        {
            markSurface(*level.sectors[0].surfaces[0].adjoin);
        }
        break;
    case Mode::translateEdges:
    case Mode::rotateEdges:
    {
        const sed::Surface& surface = level.sectors[0].surfaces[0];
        for ( const std::size_t corner : {0u, 1u} )
        {
            const std::int32_t vertex = surface.vertices[corner];
            if ( vertex >= 0 && static_cast<std::size_t>(vertex) < visited[0].size() )
            {
                visited[0][static_cast<std::size_t>(vertex)] = true;
            }
        }
        break;
    }
    case Mode::translateVertices:
        visited[0][0] = true;
        break;
    case Mode::flipSectorsAxis:
    case Mode::flipSectorsPlane:
        std::fill(visited[0].begin(), visited[0].end(), true);
        break;
    case Mode::recalc:
    case Mode::duplicateSector:
        break;
    }

    return visited;
}

void transform(sed::Level& level, const Mode mode, std::int32_t& duplicateResult)
{
    const std::array<std::int32_t, 1> sectors{{0}};
    const std::array<sed::SurfaceRef, 1> surfaces{{{0, 0}}};
    const std::array<sed::level::EdgeRef, 1> edges{{{0, 0, 0}}};
    const std::array<sed::VertexRef, 1> vertices{{{0, 0}}};
    switch ( mode )
    {
    case Mode::recalc:
        sed::level::levelRecalcSector(level, 0);
        break;
    case Mode::translateSectors:
        sed::level::levelTranslateSectors(level, sectors, 0.25, -0.5, 1.0);
        break;
    case Mode::translateSurfaces:
        sed::level::levelTranslateSurfaces(level, surfaces, 0.25, -0.5, 1.0);
        break;
    case Mode::translateEdges:
        sed::level::levelTranslateEdges(level, edges, 0.25, -0.5, 1.0);
        break;
    case Mode::translateVertices:
        sed::level::levelTranslateVertices(level, vertices, 0.25, -0.5, 1.0);
        break;
    case Mode::rotateSectors:
        sed::level::levelRotateSectors(level, sectors, {0.0, 0.0, 1.0}, 0.0, 0.0, 0.0, 17.0);
        break;
    case Mode::rotateSurfaces:
        sed::level::levelRotateSurfaces(level, surfaces, {0.0, 0.0, 1.0}, 0.0, 0.0, 0.0, 17.0);
        break;
    case Mode::rotateEdges:
        sed::level::levelRotateEdges(level, edges, {0.0, 0.0, 1.0}, 0.0, 0.0, 0.0, 17.0);
        break;
    case Mode::scaleSectors:
    {
        sed::ScaleData scale{};
        scale.sfactor = 1.25;
        sed::level::levelScaleSectors(level, sectors, scale, true);
        break;
    }
    case Mode::scaleSurfaces:
    {
        sed::ScaleData scale{};
        scale.sfactor = 1.25;
        sed::level::levelScaleSurfaces(level, surfaces, scale, true);
        break;
    }
    case Mode::flipSectorsAxis:
        sed::level::levelFlipSectors(level, sectors, 0.0, 0.0, 0.0, sed::level::Axis::x);
        break;
    case Mode::flipSectorsPlane:
        sed::level::levelFlipSectorsOverPlane(level, sectors, {1.0, 0.0, 0.0}, 0.0, 0.0, 0.0);
        break;
    case Mode::duplicateSector:
        duplicateResult = sed::level::levelDuplicateSector(
            level,
            0,
            0.0,
            0.0,
            0.0,
            {1.0, 0.0, 0.0},
            {0.0, 1.0, 0.0},
            {0.0, 0.0, 1.0},
            10.0,
            20.0,
            30.0);
        break;
    }
}

bool close(const double actual, const double expected)
{
    return std::abs(actual - expected) <= 1.0e-12;
}

void checkNoFaultResult(const sed::Level& before, const sed::Level& actual, const Mode mode,
                        const std::int32_t duplicateResult)
{
    if ( mode == Mode::recalc )
    {
        bool visitedNormalsMatch = true;
        bool staleNormalChanged = false;
        for ( std::size_t sectorIndex = 0; sectorIndex < actual.sectors.size(); ++sectorIndex )
        {
            for ( std::size_t surfaceIndex = 0; surfaceIndex < actual.sectors[sectorIndex].surfaces.size(); ++surfaceIndex )
            {
                const sed::Sector& sector = actual.sectors[sectorIndex];
                const sed::Surface& surface = sector.surfaces[surfaceIndex];
                const sed::TVector expected = sed::level::surfaceCalcNormalNewell(sector, surface);
                visitedNormalsMatch = visitedNormalsMatch && surface.normal.x == expected.x &&
                    surface.normal.y == expected.y && surface.normal.z == expected.z;
                staleNormalChanged = staleNormalChanged || surface.normal.x !=
                    before.sectors[sectorIndex].surfaces[surfaceIndex].normal.x ||
                    surface.normal.y != before.sectors[sectorIndex].surfaces[surfaceIndex].normal.y ||
                    surface.normal.z != before.sectors[sectorIndex].surfaces[surfaceIndex].normal.z;
            }
        }
        check(visitedNormalsMatch && staleNormalChanged,
              "No-fault Sector recalc refreshes every normal reached through reciprocal adjoins");
        for ( std::size_t sectorIndex = 0; sectorIndex < actual.sectors.size(); ++sectorIndex )
        {
            for ( std::size_t vertexIndex = 0; vertexIndex < actual.sectors[sectorIndex].vertices.size(); ++vertexIndex )
            {
                check(actual.sectors[sectorIndex].vertices[vertexIndex].mark ==
                          before.sectors[sectorIndex].vertices[vertexIndex].mark,
                      "Sector recalc leaves transform visit marks untouched");
            }
        }
        return;
    }

    if ( mode == Mode::duplicateSector )
    {
        check(duplicateResult == 3 && actual.sectors.size() == 4,
              "No-fault Sector duplication appends exactly one complete Sector and returns its index");
        if ( actual.sectors.size() == 4 )
        {
            const sed::Sector& duplicate = actual.sectors[3];
            check(duplicate.vertices.size() == before.sectors[0].vertices.size() &&
                      duplicate.surfaces.size() == before.sectors[0].surfaces.size() &&
                      close(duplicate.vertices[0].x, 9.0) &&
                      close(duplicate.vertices[0].y, 19.0) &&
                      close(duplicate.vertices[0].z, 29.0),
                  "No-fault Sector duplication transforms each source vertex through the fixed basis");
            check(std::all_of(duplicate.surfaces.begin(), duplicate.surfaces.end(),
                              [](const sed::Surface& surface) { return !surface.adjoin.has_value(); }),
                  "No-fault Sector duplication clears every copied adjoin");
        }
        return;
    }

    const sed::Sector& sourceSector = before.sectors[0];
    const sed::Sector& transformedSector = actual.sectors[0];
    const sed::Vertex& first = transformedSector.vertices[0];
    switch ( mode )
    {
    case Mode::translateSectors:
        check(close(first.x, -0.75) && close(first.y, -1.5) && close(first.z, 0.0),
              "No-fault Sector translation publishes its fixed selected-vertex result");
        break;
    case Mode::translateSurfaces:
        check(close(transformedSector.vertices[1].x, sourceSector.vertices[1].x + 0.25) &&
                  close(transformedSector.vertices[1].y, sourceSector.vertices[1].y - 0.5) &&
                  close(transformedSector.vertices[0].x, sourceSector.vertices[0].x),
              "No-fault Surface translation moves selected corners and retains unselected corners");
        break;
    case Mode::translateEdges:
        check(close(transformedSector.vertices[1].x, sourceSector.vertices[1].x + 0.25) &&
                  close(transformedSector.vertices[5].z, sourceSector.vertices[5].z + 1.0) &&
                  close(transformedSector.vertices[6].x, sourceSector.vertices[6].x),
              "No-fault Edge translation moves only its two source endpoints");
        break;
    case Mode::translateVertices:
        check(close(first.x, sourceSector.vertices[0].x + 0.25) &&
                  close(first.y, sourceSector.vertices[0].y - 0.5) &&
                  close(first.z, sourceSector.vertices[0].z + 1.0) &&
                  close(transformedSector.vertices[1].x, sourceSector.vertices[1].x),
              "No-fault Vertex translation moves only the selected vertex");
        break;
    case Mode::rotateSectors:
    case Mode::rotateSurfaces:
    case Mode::rotateEdges:
    {
        const double angle = 17.0 * std::acos(-1.0) / 180.0;
        const std::size_t vertexIndex =
            mode == Mode::rotateSectors ? 0u : 1u;
        const sed::Vertex& point = transformedSector.vertices[vertexIndex];
        const sed::Vertex& source = sourceSector.vertices[vertexIndex];
        // prepareRotateData stores its rotated basis in TMatrix43's legacy
        // transposed row vectors, so rotateXYZ applies the negative-angle form.
        check(close(point.x, source.x * std::cos(angle) + source.y * std::sin(angle)) &&
                  close(point.y, -source.x * std::sin(angle) + source.y * std::cos(angle)) &&
                  close(point.z, source.z),
              "No-fault geometry rotation matches its fixed double-precision world-axis result");
        break;
    }
    case Mode::scaleSectors:
        check(close(first.x, -1.25) && close(first.y, -1.25) && close(first.z, -1.25) &&
                  close(transformedSector.surfaces[0].uScale,
                        static_cast<double>(sourceSector.surfaces[0].uScale) / 1.25),
              "No-fault Sector Scale transforms the fixed vertex and texture-density metadata");
        break;
    case Mode::scaleSurfaces:
        check(close(transformedSector.vertices[1].x, sourceSector.vertices[1].x * 1.25) &&
                  close(transformedSector.vertices[0].x, sourceSector.vertices[0].x) &&
                  close(transformedSector.surfaces[0].uScale,
                        static_cast<double>(sourceSector.surfaces[0].uScale) / 1.25),
              "No-fault Surface Scale transforms only selected corners and its texture density");
        break;
    case Mode::flipSectorsAxis:
    case Mode::flipSectorsPlane:
        check(close(first.x, 1.0) && close(first.y, sourceSector.vertices[0].y) &&
                  close(first.z, sourceSector.vertices[0].z) &&
                  !transformedSector.surfaces[0].adjoin.has_value() &&
                  !actual.sectors[1].surfaces[1].adjoin.has_value(),
              "No-fault Sector mirror reflects its fixed coordinate and removes external adjoins");
        break;
    case Mode::recalc:
    case Mode::duplicateSector:
        break;
    }

    const std::vector<std::vector<bool>> expectedMarks =
        expectedVisitedVertices(before, mode);
    bool marksMatch = true;
    for ( std::size_t sectorIndex = 0; sectorIndex < actual.sectors.size(); ++sectorIndex )
    {
        for ( std::size_t vertexIndex = 0;
             vertexIndex < actual.sectors[sectorIndex].vertices.size();
             ++vertexIndex )
        {
            marksMatch = marksMatch &&
                actual.sectors[sectorIndex].vertices[vertexIndex].mark ==
                    (expectedMarks[sectorIndex][vertexIndex] ? 1 : 0);
        }
    }
    check(marksMatch,
          "No-fault geometry transform retains the legacy visited-vertex marks");
}
}

int main(const int argc, char** argv)
{
    if ( argc != 2 )
    {
        return 2;
    }
    const auto found = std::find(kModeNames.begin(), kModeNames.end(), std::string_view(argv[1]));
    if ( found == kModeNames.end() )
    {
        return 2;
    }
    const Mode mode = static_cast<Mode>(static_cast<std::size_t>(found - kModeNames.begin()));
    std::set_terminate([]
    {
        std::fprintf(stderr, "Geometry transform terminated at allocation %zu\n", currentFailurePosition);
        std::_Exit(86);
    });

    bool reachedSuccess = false;
    std::size_t rejected = 0;
    for ( std::size_t failurePosition = 0; failurePosition < 4096; ++failurePosition )
    {
        sed::Level level = fixture();
        const sed::Level before = level;
        const std::vector<std::byte> beforeBytes = sed::assets::saveToSedBytes(level);
        std::int32_t duplicateResult = -2;
        bool caught = false;
        allocationFailureFired = false;
        currentFailurePosition = failurePosition;
        allocationsBeforeFailure = failurePosition;
        try
        {
            transform(level, mode, duplicateResult);
        }
        catch (const std::bad_alloc&)
        {
            caught = true;
        }
        allocationsBeforeFailure.reset();

        if ( allocationFailureFired )
        {
            ++rejected;
            check(!caught, "Legacy geometry transform wrapper handles preflight allocation rejection");
            check(exactLevel(level, before, beforeBytes),
                  "Injected geometry transform allocation failure preserves authored and runtime Level state");
            if ( mode == Mode::duplicateSector )
            {
                check(duplicateResult == -1,
                      "Rejected Sector duplication returns its failure sentinel without appending a partial Sector");
            }
        }
        else
        {
            check(!caught, "Geometry transform allocation sweep reaches a no-fault call");
            checkNoFaultResult(before, level, mode, duplicateResult);
            reachedSuccess = true;
            break;
        }
    }
    check(rejected > 0 && reachedSuccess,
          "Geometry transform allocation sweep reaches every rejection before an independently checked success");
    return sed::tests::failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
