#include "test_common.hpp"

#include "sed/assets/sed.hpp"
#include "sed/level/build.hpp"
#include "sed/level/geometry.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace sed::tests
{

namespace
{

constexpr double kBackOffsetX = -0.2;

constexpr std::array<sed::TVector, 5> kPyr490SourcePoints{{
    {4.7343501999999997, -2.2712860099999999, 0.96540302},
    {4.7450408900000003, -2.2502579699999998, 0.95387697000000005},
    {4.7703599900000002, -2.2004570999999999, 0.92658001000000001},
    {4.7450408900000003, -2.2502579699999998, 0.92658001000000001},
    {4.7343501999999997, -2.2712860099999999, 0.92658001000000001}}};

constexpr std::array<sed::TexVertex, 5> kPyr490SourceTuples{{
    {0.0f, 0.0f, {0.25f, 0.5f, 0.75f, 1.0f}},
    {0.25f, 0.25f, {0.25f, 0.5f, 0.75f, 1.0f}},
    {0.5f, 0.5f, {0.25f, 0.5f, 0.75f, 1.0f}},
    {0.75f, 0.75f, {0.25f, 0.5f, 0.75f, 1.0f}},
    {1.0f, 1.0f, {0.25f, 0.5f, 0.75f, 1.0f}}}};

constexpr std::array<sed::TVector, 7> kPyr947SourcePoints{{
    {-0.10469299999999999, 0.90320897, 0.15381800000000001},
    {-0.10469299999999999, 0.95300001000000001, 0.20360900000000001},
    {-0.10469299999999999, 0.97799999000000004, 0.22860900000000001},
    {-0.10469299999999999, 0.99049997000000001, 0.22860900000000001},
    {-0.10469299999999999, 1.10300004, 0.22860900000000001},
    {-0.10469299999999999, 1.10300004, 0.02861},
    {-0.10469299999999999, 0.90320897, 0.02861}}};

constexpr std::array<sed::TexVertex, 7> kPyr947SourceTuples{{
    {0.0f, 0.0f, {0.25f, 0.5f, 0.75f, 1.0f}},
    {0.16666667f, 0.16666667f, {0.25f, 0.5f, 0.75f, 1.0f}},
    {0.33333334f, 0.33333334f, {0.25f, 0.5f, 0.75f, 1.0f}},
    {0.5f, 0.5f, {0.25f, 0.5f, 0.75f, 1.0f}},
    {0.66666669f, 0.66666669f, {0.25f, 0.5f, 0.75f, 1.0f}},
    {0.83333331f, 0.83333331f, {0.25f, 0.5f, 0.75f, 1.0f}},
    {1.0f, 1.0f, {0.25f, 0.5f, 0.75f, 1.0f}}}};

void checkCase(const bool condition, const char* const caseName,
    const char* const property)
{
    if ( condition )
    {
        return;
    }

    ++failures;
    std::cerr << "FAIL: " << caseName << ": " << property << '\n';
}

bool sameTextureVertex(const sed::TexVertex& first, const sed::TexVertex& second) noexcept
{
    return first.u == second.u && first.v == second.v && first.color == second.color;
}

std::array<long double, 3> independentFaceArea(
    const sed::Sector& sector, const sed::Surface& face)
{
    std::array<long double, 3> area{};
    if ( face.vertices.size() < 3
        || face.vertices.size() != face.texVertices.size()
        || face.vertices.front() < 0
        || static_cast<std::size_t>(face.vertices.front()) >= sector.vertices.size() )
    {
        return area;
    }

    const auto validIndex = [&](const std::int32_t index)
    {
        return index >= 0 && static_cast<std::size_t>(index) < sector.vertices.size();
    };
    const auto& anchor = sector.vertices[static_cast<std::size_t>(face.vertices.front())];
    const long double anchorX = static_cast<long double>(anchor.x);
    const long double anchorY = static_cast<long double>(anchor.y);
    const long double anchorZ = static_cast<long double>(anchor.z);
    for ( std::size_t corner = 1; corner + 1 < face.vertices.size(); ++corner )
    {
        if ( !validIndex(face.vertices[corner]) || !validIndex(face.vertices[corner + 1]) )
        {
            return {};
        }

        const auto& first = sector.vertices[static_cast<std::size_t>(face.vertices[corner])];
        const auto& second = sector.vertices[static_cast<std::size_t>(face.vertices[corner + 1])];
        const long double ax = static_cast<long double>(first.x) - anchorX;
        const long double ay = static_cast<long double>(first.y) - anchorY;
        const long double az = static_cast<long double>(first.z) - anchorZ;
        const long double bx = static_cast<long double>(second.x) - anchorX;
        const long double by = static_cast<long double>(second.y) - anchorY;
        const long double bz = static_cast<long double>(second.z) - anchorZ;
        area[0] += ay * bz - az * by;
        area[1] += az * bx - ax * bz;
        area[2] += ax * by - ay * bx;
    }
    return area;
}

bool outputFacesAreFiniteAndNonzero(const sed::Level& level)
{
    for ( const auto& sector : level.sectors )
    {
        if ( sector.surfaces.empty() )
        {
            return false;
        }
        for ( const auto& face : sector.surfaces )
        {
            if ( face.vertices.size() < 3
                || face.vertices.size() != face.texVertices.size() )
            {
                return false;
            }
            for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
            {
                const auto index = face.vertices[corner];
                if ( index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size() )
                {
                    return false;
                }
                const auto& point = sector.vertices[static_cast<std::size_t>(index)];
                const auto& tuple = face.texVertices[corner];
                if ( !std::isfinite(point.x) || !std::isfinite(point.y)
                    || !std::isfinite(point.z) || !std::isfinite(tuple.u)
                    || !std::isfinite(tuple.v) || !std::isfinite(tuple.color.r)
                    || !std::isfinite(tuple.color.g) || !std::isfinite(tuple.color.b)
                    || !std::isfinite(tuple.color.a) )
                {
                    return false;
                }
            }

            const auto area = independentFaceArea(sector, face);
            if ( !std::isfinite(area[0]) || !std::isfinite(area[1])
                || !std::isfinite(area[2])
                || (area[0] == 0.0L && area[1] == 0.0L && area[2] == 0.0L) )
            {
                return false;
            }
        }
    }
    return true;
}

bool directedEdgesClose(const sed::Level& level)
{
    for ( const auto& sector : level.sectors )
    {
        std::map<std::pair<std::int32_t, std::int32_t>, int> edges;
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
                ++edges[{first, second}];
            }
        }

        for ( const auto& [edge, count] : edges )
        {
            const auto reverse = edges.find({edge.second, edge.first});
            if ( count != 1 || reverse == edges.end() || reverse->second != 1 )
            {
                return false;
            }
        }
    }
    return true;
}

std::optional<std::size_t> reversedCyclicMatchStart(
    const sed::Sector& firstSector, const sed::Surface& first,
    const sed::Sector& secondSector, const sed::Surface& second)
{
    if ( first.vertices.size() != second.vertices.size() || first.vertices.size() < 3
        || first.vertices.size() != first.texVertices.size()
        || second.vertices.size() != second.texVertices.size() )
    {
        return std::nullopt;
    }

    const auto samePoint = [](const sed::Vertex& firstPoint,
        const sed::Vertex& secondPoint)
    {
        return std::isfinite(firstPoint.x) && std::isfinite(firstPoint.y)
            && std::isfinite(firstPoint.z) && std::isfinite(secondPoint.x)
            && std::isfinite(secondPoint.y) && std::isfinite(secondPoint.z)
            && firstPoint.x == secondPoint.x && firstPoint.y == secondPoint.y
            && firstPoint.z == secondPoint.z;
    };

    for ( std::size_t start = 0; start < second.vertices.size(); ++start )
    {
        bool matches = true;
        for ( std::size_t corner = 0; corner < first.vertices.size(); ++corner )
        {
            const auto firstIndex = first.vertices[corner];
            const auto secondIndex = second.vertices[
                (start + second.vertices.size() - corner) % second.vertices.size()];
            if ( firstIndex < 0 || secondIndex < 0
                || static_cast<std::size_t>(firstIndex) >= firstSector.vertices.size()
                || static_cast<std::size_t>(secondIndex) >= secondSector.vertices.size()
                || !samePoint(firstSector.vertices[static_cast<std::size_t>(firstIndex)],
                    secondSector.vertices[static_cast<std::size_t>(secondIndex)]) )
            {
                matches = false;
                break;
            }
        }
        if ( matches )
        {
            return start;
        }
    }
    return std::nullopt;
}

struct PortalOracleControls
{
    bool cyclic{};
    bool wrongWindingRejected{};
    bool oneUlpRejected{};
};

PortalOracleControls portalOracleControls()
{
    const std::array<sed::TVector, 4> points{{
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
        {1.0, 0.0, 1.0}, {0.0, 0.0, 1.0}}};
    sed::Sector firstSector;
    sed::Sector secondSector;
    for ( const auto& point : points )
    {
        firstSector.newVertex(point.x, point.y, point.z);
        secondSector.newVertex(point.x, point.y, point.z);
    }

    sed::Surface first;
    first.vertices = {0, 1, 2, 3};
    first.texVertices.resize(first.vertices.size());
    sed::Surface reversedCyclic;
    reversedCyclic.vertices = {2, 1, 0, 3};
    reversedCyclic.texVertices.resize(reversedCyclic.vertices.size());
    const bool cyclic = reversedCyclicMatchStart(
        firstSector, first, secondSector, reversedCyclic)
        == std::optional<std::size_t>{2};
    const bool wrongWindingRejected = !reversedCyclicMatchStart(
        firstSector, first, secondSector, first);

    sed::Sector oneUlpSector = secondSector;
    oneUlpSector.vertices[2].x = std::nextafter(
        oneUlpSector.vertices[2].x, std::numeric_limits<double>::infinity());
    const bool oneUlpRejected = !reversedCyclicMatchStart(
        firstSector, first, oneUlpSector, reversedCyclic);
    return {cyclic, wrongWindingRejected, oneUlpRejected};
}

bool reciprocalPortals(const sed::Level& level, std::size_t& portalCount)
{
    portalCount = 0;
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

            const auto& other = level.sectors[static_cast<std::size_t>(target.sector)];
            const auto& otherFace = other.surfaces[static_cast<std::size_t>(target.surface)];
            if ( !otherFace.adjoin
                || *otherFace.adjoin != sed::SurfaceRef{
                    static_cast<std::int32_t>(sectorIndex),
                    static_cast<std::int32_t>(surfaceIndex)}
                || face.vertices.size() != otherFace.vertices.size()
                || face.vertices.size() != face.texVertices.size()
                || otherFace.vertices.size() != otherFace.texVertices.size()
                || !reversedCyclicMatchStart(sector, face, other, otherFace) )
            {
                return false;
            }
        }
    }
    return portalCount != 0;
}

template <std::size_t Count>
sed::Level makePrism(
    const std::array<sed::TVector, Count>& sourcePoints,
    const std::array<sed::TexVertex, Count>& sourceTuples,
    const char* const sourceMaterial,
    const char* const sideMaterial)
{
    sed::Level level;
    level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
    auto& source = level.newSector();
    source.mark = 701;
    for ( std::size_t index = 0; index < Count; ++index )
    {
        const auto vertex = source.newVertex(sourcePoints[index].x,
            sourcePoints[index].y, sourcePoints[index].z);
        source.vertices[static_cast<std::size_t>(vertex)].mark
            = static_cast<std::int32_t>(4100 + index);
    }

    sed::Surface sourceFace;
    sourceFace.num = 0;
    sourceFace.mark = 5100;
    sourceFace.material = sourceMaterial;
    sourceFace.surfaceFlags = sed::SF_Collision;
    sourceFace.vertices.resize(Count);
    for ( std::size_t index = 0; index < Count; ++index )
    {
        sourceFace.vertices[index] = static_cast<std::int32_t>(index);
    }
    sourceFace.texVertices.assign(sourceTuples.begin(), sourceTuples.end());
    sourceFace.normal = sed::level::surfaceCalcNormalNewell(source, sourceFace);
    source.addSurface(std::move(sourceFace));

    for ( std::size_t index = 0; index < Count; ++index )
    {
        const auto& point = sourcePoints[index];
        const auto vertex = source.newVertex(point.x + kBackOffsetX, point.y, point.z);
        source.vertices[static_cast<std::size_t>(vertex)].mark
            = static_cast<std::int32_t>(4200 + index);
    }

    const auto appendFace = [&](std::vector<std::int32_t> vertices,
        const std::size_t tupleSeed)
    {
        sed::Surface face;
        face.num = static_cast<std::int32_t>(source.surfaces.size());
        face.mark = static_cast<std::int32_t>(5200 + face.num);
        face.material = sideMaterial;
        face.surfaceFlags = sed::SF_Collision;
        face.vertices = std::move(vertices);
        face.texVertices.resize(face.vertices.size());
        for ( std::size_t index = 0; index < face.texVertices.size(); ++index )
        {
            face.texVertices[index] = sourceTuples[(tupleSeed + index) % Count];
        }
        face.normal = sed::level::surfaceCalcNormalNewell(source, face);
        source.addSurface(std::move(face));
    };

    std::vector<std::int32_t> back;
    back.reserve(Count);
    for ( std::size_t index = 0; index < Count; ++index )
    {
        back.push_back(static_cast<std::int32_t>(2 * Count - 1 - index));
    }
    appendFace(std::move(back), 0);
    for ( std::size_t edge = 0; edge < Count; ++edge )
    {
        const auto next = (edge + 1) % Count;
        appendFace({
            static_cast<std::int32_t>(next),
            static_cast<std::int32_t>(edge),
            static_cast<std::int32_t>(edge + Count),
            static_cast<std::int32_t>(next + Count)}, edge);
    }
    return level;
}

template <std::size_t Count>
bool sourceCoordinatesAndTuplesSurvive(
    const sed::Level& level,
    const std::array<sed::TVector, Count>& sourcePoints,
    const std::array<sed::TexVertex, Count>& sourceTuples,
    const char* const material)
{
    if ( level.sectors.empty() || level.sectors.front().mark != 701
        || level.sectors.front().vertices.size() < Count * 2 )
    {
        return false;
    }

    const auto& sector = level.sectors.front();
    for ( std::size_t index = 0; index < Count * 2; ++index )
    {
        const auto& expected = sourcePoints[index % Count];
        const auto& point = sector.vertices[index];
        const double expectedX = index < Count
            ? expected.x : expected.x + kBackOffsetX;
        const std::int32_t expectedMark = static_cast<std::int32_t>(
            (index < Count ? 4100 : 4200) + index % Count);
        if ( point.x != expectedX || point.y != expected.y || point.z != expected.z
            || point.mark != expectedMark )
        {
            return false;
        }
    }

    bool sourceSurfaceFound = false;
    for ( const auto& face : sector.surfaces )
    {
        sourceSurfaceFound = sourceSurfaceFound
            || face.material == material;
    }
    if ( !sourceSurfaceFound )
    {
        return false;
    }

    for ( std::size_t corner = 0; corner < Count; ++corner )
    {
        bool found = false;
        for ( const auto& face : sector.surfaces )
        {
            // A generated side can carry a copied tuple and the same XYZ
            // coordinate. It is not evidence that the authored source tuple
            // survived. Restrict this oracle to the source face lineage.
            if ( face.material != material )
            {
                continue;
            }
            for ( std::size_t faceCorner = 0;
                faceCorner < face.vertices.size() && faceCorner < face.texVertices.size();
                ++faceCorner )
            {
                const auto index = face.vertices[faceCorner];
                if ( index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size() )
                {
                    continue;
                }
                const auto& point = sector.vertices[static_cast<std::size_t>(index)];
                if ( point.x == sourcePoints[corner].x
                    && point.y == sourcePoints[corner].y
                    && point.z == sourcePoints[corner].z
                    && sameTextureVertex(face.texVertices[faceCorner], sourceTuples[corner]) )
                {
                    found = true;
                }
            }
        }
        if ( !found )
        {
            return false;
        }
    }
    return true;
}

template <std::size_t Count>
bool anyFaceMatchesSourceTuples(
    const sed::Level& level,
    const std::array<sed::TVector, Count>& sourcePoints,
    const std::array<sed::TexVertex, Count>& sourceTuples)
{
    if ( level.sectors.empty() )
    {
        return false;
    }

    const auto& sector = level.sectors.front();
    for ( std::size_t corner = 0; corner < Count; ++corner )
    {
        bool found = false;
        for ( const auto& face : sector.surfaces )
        {
            for ( std::size_t faceCorner = 0;
                faceCorner < face.vertices.size() && faceCorner < face.texVertices.size();
                ++faceCorner )
            {
                const auto index = face.vertices[faceCorner];
                if ( index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size() )
                {
                    continue;
                }
                const auto& point = sector.vertices[static_cast<std::size_t>(index)];
                if ( point.x == sourcePoints[corner].x
                    && point.y == sourcePoints[corner].y
                    && point.z == sourcePoints[corner].z
                    && sameTextureVertex(face.texVertices[faceCorner], sourceTuples[corner]) )
                {
                    found = true;
                }
            }
        }
        if ( !found )
        {
            return false;
        }
    }
    return true;
}

template <std::size_t Count>
void runSourceTupleLineageControl(
    const char* const caseName,
    const std::array<sed::TVector, Count>& sourcePoints,
    const std::array<sed::TexVertex, Count>& sourceTuples,
    const char* const sourceMaterial,
    const char* const sideMaterial)
{
    sed::Level level = makePrism(sourcePoints, sourceTuples, sourceMaterial, sideMaterial);
    auto& source = level.sectors.front();
    const bool baselineAccepted = sourceCoordinatesAndTuplesSurvive(
        level, sourcePoints, sourceTuples, sourceMaterial);
    checkCase(baselineAccepted, caseName,
        "unmodified source-lineage tuples are accepted");
    if ( source.surfaces.size() < 2 || source.surfaces[0].texVertices.empty() )
    {
        checkCase(false, caseName, "fixture contains source and adjacent surfaces");
        return;
    }

    // Put the expected tuple on an adjacent face while corrupting it on the
    // authored source face. A whole-face scan would incorrectly accept this
    // state; the lineage-aware oracle must reject it.
    sed::TexVertex corrupted = sourceTuples[0];
    corrupted.u = -1.0f;
    corrupted.v = -1.0f;
    corrupted.color = {1.0f, 0.0f, 0.0f, 1.0f};
    source.surfaces[0].texVertices[0] = corrupted;
    bool placedAdjacentTuple = false;
    for ( std::size_t surfaceIndex = 1;
        !placedAdjacentTuple && surfaceIndex < source.surfaces.size(); ++surfaceIndex )
    {
        auto& face = source.surfaces[surfaceIndex];
        for ( std::size_t corner = 0;
            corner < face.vertices.size() && corner < face.texVertices.size(); ++corner )
        {
            if ( face.vertices[corner] == 0 )
            {
                face.texVertices[corner] = sourceTuples[0];
                placedAdjacentTuple = true;
                break;
            }
        }
    }
    checkCase(placedAdjacentTuple, caseName,
        "fixture places the matching tuple on an adjacent source-vertex face");
    const bool broadScanAccepts = anyFaceMatchesSourceTuples(
        level, sourcePoints, sourceTuples);
    const bool lineageScanRejects = !sourceCoordinatesAndTuplesSurvive(
        level, sourcePoints, sourceTuples, sourceMaterial);
    checkCase(broadScanAccepts && lineageScanRejects, caseName,
        "an adjacent matching tuple cannot mask source-lineage corruption");
}

template <std::size_t Count>
void runPrismCase(
    const char* const caseName,
    const std::array<sed::TVector, Count>& sourcePoints,
    const std::array<sed::TexVertex, Count>& sourceTuples,
    const char* const sourceMaterial,
    const char* const sideMaterial,
    const std::size_t expectedPortalCount)
{
    sed::Level level = makePrism(sourcePoints, sourceTuples, sourceMaterial, sideMaterial);
    sed::level::SurfaceExtrudeReport report{
        sed::level::SurfaceExtrudeError::InvalidResult, {-7, -8}};
    const bool accepted = sed::level::levelExtrudeSurface(
        level, {0, 0}, 0.1, -0.01, &report);
    std::size_t portalCount = 0;
    const bool reciprocal = reciprocalPortals(level, portalCount);

    checkCase(accepted, caseName, "extrusion returns true");
    checkCase(report.error == sed::level::SurfaceExtrudeError::None,
        caseName, "report returns SurfaceExtrudeError::None");
    checkCase(report.surface == sed::SurfaceRef{0, 0},
        caseName, "report returns the requested source surface");
    checkCase(level.sectors.size() == 2, caseName,
        "successful extrusion publishes exactly two sectors");
    checkCase(outputFacesAreFiniteAndNonzero(level), caseName,
        "every output face has finite tuples and independent nonzero area");
    checkCase(directedEdgesClose(level), caseName,
        "every sector has one opposite incidence for each directed edge");
    checkCase(reciprocal && portalCount == expectedPortalCount, caseName,
        "all expected portals are reciprocal with exact reverse-cyclic boundaries");
    checkCase(sourceCoordinatesAndTuplesSurvive(
            level, sourcePoints, sourceTuples, sourceMaterial), caseName,
        "authored coordinates, vertex marks, source material and tuples survive");
    bool sourcePrimaryMark = false;
    bool sideMaterialFound = false;
    for ( const auto& face : level.sectors.front().surfaces )
    {
        sourcePrimaryMark = sourcePrimaryMark
            || (face.material == sourceMaterial && face.mark == 5100);
        sideMaterialFound = sideMaterialFound || face.material == sideMaterial;
    }
    checkCase(sourcePrimaryMark, caseName,
        "at least one source-material portal fragment retains the primary source face mark");
    checkCase(sideMaterialFound, caseName,
        "back and side faces retain their distinct side material lineage");

    sed::Level rollback = makePrism(sourcePoints, sourceTuples, sourceMaterial, sideMaterial);
    const auto rollbackBefore = sed::assets::saveToSedBytes(rollback);
    sed::level::SurfaceExtrudeReport rollbackReport{
        sed::level::SurfaceExtrudeError::None, {-7, -8}};
    const bool rollbackAccepted = sed::level::levelExtrudeSurface(
        rollback, {0, 0}, 0.0, -0.01, &rollbackReport);
    const auto rollbackAfter = sed::assets::saveToSedBytes(rollback);
    checkCase(!rollbackAccepted, caseName,
        "invalid length returns false");
    checkCase(rollbackReport.error == sed::level::SurfaceExtrudeError::InvalidLength,
        caseName, "invalid length reports SurfaceExtrudeError::InvalidLength");
    checkCase(rollbackReport.surface == sed::SurfaceRef{0, 0}, caseName,
        "invalid length reports the requested source surface");
    checkCase(rollbackAfter == rollbackBefore
            && sed::assets::sedLevelsEqual(rollback,
                makePrism(sourcePoints, sourceTuples, sourceMaterial, sideMaterial)),
        caseName, "invalid length leaves serialized and in-memory level unchanged");
}

} // namespace

void runExtrudeCollinearSupportTests()
{
    const auto controls = portalOracleControls();
    check(controls.cyclic,
        "Extrude portal oracle accepts an exact reverse cyclic rotation");
    check(controls.wrongWindingRejected,
        "Extrude portal oracle rejects a same-winding loop");
    check(controls.oneUlpRejected,
        "Extrude portal oracle rejects a one-ULP coordinate mismatch");

    runSourceTupleLineageControl("PYR490 source tuple lineage control",
        kPyr490SourcePoints, kPyr490SourceTuples,
        "pyr490-1-source.mat", "pyr490-1-side.mat");
    runSourceTupleLineageControl("PYR947 source tuple lineage control",
        kPyr947SourcePoints, kPyr947SourceTuples,
        "pyr947-0-source.mat", "pyr947-0-side.mat");

    runPrismCase("PYR490 face 490:1 synthetic prism",
        kPyr490SourcePoints, kPyr490SourceTuples,
        "pyr490-1-source.mat", "pyr490-1-side.mat", 6);
    runPrismCase("PYR947 face 947:0 synthetic prism",
        kPyr947SourcePoints, kPyr947SourceTuples,
        "pyr947-0-source.mat", "pyr947-0-side.mat", 10);
}

} // namespace sed::tests
