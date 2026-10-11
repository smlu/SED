#include "test_common.hpp"

#include "sed/assets/sed.hpp"
#include "sed/level/build.hpp"
#include "sed/level.hpp"
#include "sed/level/transform.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace sed::tests
{
namespace
{

struct TestBounds
{
    sed::TVector min{};
    sed::TVector max{};
};

sed::TVector vertexPosition(const sed::Sector& sector, const std::size_t index)
{
    const sed::Vertex& vertex = sector.vertices[index];
    return sed::TVector{ vertex.x, vertex.y, vertex.z };
}

TestBounds sectorBounds(const sed::Sector& sector)
{
    TestBounds bounds{
        sed::TVector{
            std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity() },
        sed::TVector{
            -std::numeric_limits<double>::infinity(),
            -std::numeric_limits<double>::infinity(),
            -std::numeric_limits<double>::infinity() }
    };

    for ( const sed::Vertex& vertex : sector.vertices )
    {
        bounds.min.x = std::min(bounds.min.x, vertex.x);
        bounds.min.y = std::min(bounds.min.y, vertex.y);
        bounds.min.z = std::min(bounds.min.z, vertex.z);
        bounds.max.x = std::max(bounds.max.x, vertex.x);
        bounds.max.y = std::max(bounds.max.y, vertex.y);
        bounds.max.z = std::max(bounds.max.z, vertex.z);
    }

    return bounds;
}

sed::Surface makeSurface(const std::initializer_list<std::int32_t> vertices, const sed::TVector& normal = sed::TVector::up)
{
    sed::Surface surface{};
    surface.vertices.assign(vertices.begin(), vertices.end());
    surface.texVertices.resize(surface.vertices.size());
    surface.normal = normal;
    surface.material = "test.mat";
    surface.surfaceFlags = sed::SF_Collision;
    return surface;
}

sed::Sector makeDegenerateShape()
{
    sed::Sector shape{};
    shape.addVertex(1.0, 1.0, 1.0);
    shape.addVertex(1.0, 1.0, 1.0);
    shape.addVertex(1.0, 1.0, 1.0);
    shape.addSurface(makeSurface({ 0, 1, 2 }));
    return shape;
}

bool allSurfaceNormalsFaceSectorInterior(const sed::Sector& sector)
{
    if ( sector.vertices.empty() )
    {
        return false;
    }

    sed::TVector center{};
    for ( const sed::Vertex& vertex : sector.vertices )
    {
        center.x += vertex.x;
        center.y += vertex.y;
        center.z += vertex.z;
    }
    center = center * (1.0 / static_cast<double>(sector.vertices.size()));

    for ( const sed::Surface& surface : sector.surfaces )
    {
        if ( surface.vertices.empty() )
        {
            return false;
        }
        sed::TVector surfaceCenter{};
        for ( const std::int32_t vertexIndex : surface.vertices )
        {
            if ( vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size() )
            {
                return false;
            }
            const sed::Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
            surfaceCenter.x += vertex.x;
            surfaceCenter.y += vertex.y;
            surfaceCenter.z += vertex.z;
        }
        surfaceCenter = surfaceCenter * (1.0 / static_cast<double>(surface.vertices.size()));
        if ( surface.normal.Dot(center - surfaceCenter) <= 0.0 )
        {
            return false;
        }
    }
    return true;
}

bool sameTextureVertex(
    const sed::TexVertex& lhs,
    const sed::TexVertex& rhs) noexcept
{
    return lhs.u == rhs.u
        && lhs.v == rhs.v
        && lhs.color == rhs.color;
}

enum class ShapePlacementEntryPoint
{
    FromShape,
    Box,
    BoxAxes,
};

const char* shapePlacementEntryPointName(
    const ShapePlacementEntryPoint entryPoint) noexcept
{
    switch (entryPoint)
    {
        case ShapePlacementEntryPoint::FromShape:
            return "FromShape";
        case ShapePlacementEntryPoint::Box:
            return "Box";
        case ShapePlacementEntryPoint::BoxAxes:
            return "BoxAxes";
    }
    return "Unknown";
}

sed::Sector makeShapeUvAdmissionFixture()
{
    sed::Sector shape = sed::level::makeSectorPrimitiveShape(
        sed::ProjectType::JKDF2,
        sed::level::SectorPrimitive::Cube);
    for (std::size_t surfaceIndex = 0;
         surfaceIndex < shape.surfaces.size();
         ++surfaceIndex)
    {
        sed::Surface& surface = shape.surfaces[surfaceIndex];
        surface.material = "anisotropic.mat";
        surface.uScale = static_cast<float>(
            -24.0 - 8.0 * static_cast<double>(surfaceIndex));
        surface.vScale = static_cast<float>(
            6.0 + 4.0 * static_cast<double>(surfaceIndex));
        for (std::size_t corner = 0;
             corner < surface.texVertices.size();
             ++corner)
        {
            surface.texVertices[corner] = sed::TexVertex{
                static_cast<float>(
                    -320.0 + 96.0 * static_cast<double>(surfaceIndex)
                    + 40.0 * static_cast<double>(corner)),
                static_cast<float>(
                    112.0 - 40.0 * static_cast<double>(surfaceIndex)
                    - 12.0 * static_cast<double>(corner)),
                sed::ColorF{
                    static_cast<float>(0.02 * (surfaceIndex + 1u)),
                    static_cast<float>(0.04 * (corner + 1u)),
                    static_cast<float>(0.03 * (surfaceIndex + corner + 1u)),
                    static_cast<float>(0.35 + 0.05 * corner)
                }
            };
        }
    }
    return shape;
}

std::int32_t placeTaggedShape(
    sed::Level& level,
    const sed::Sector& shape,
    const sed::UvCoordinateSpace source,
    const ShapePlacementEntryPoint entryPoint,
    const sed::level::UvTextureDimensionsResolver& resolveDimensions)
{
    switch (entryPoint)
    {
        case ShapePlacementEntryPoint::FromShape:
            return sed::level::levelCreateSectorFromShape(
                level,
                shape,
                source,
                sed::TVector{ 3.0, 4.0, 5.0 },
                7.0,
                resolveDimensions);
        case ShapePlacementEntryPoint::Box:
            return sed::level::levelCreateSectorFromShapeBox(
                level,
                shape,
                source,
                sed::TVector{ -3.0, 2.0, -5.0 },
                sed::TVector{ 7.0, 11.0, 13.0 },
                resolveDimensions);
        case ShapePlacementEntryPoint::BoxAxes:
            return sed::level::levelCreateSectorFromShapeBoxAxes(
                level,
                shape,
                source,
                sed::TVector{ -3.0, 2.0, -5.0 },
                sed::TVector{ 7.0, 11.0, 13.0 },
                1,
                2,
                0,
                resolveDimensions);
    }
    return -1;
}

bool placedShapeTextureStateMatches(
    const sed::Sector& source,
    const sed::Sector& placed,
    const double uDivisor,
    const double vDivisor) noexcept
{
    if (placed.surfaces.size() != source.surfaces.size())
    {
        return false;
    }
    for (std::size_t surfaceIndex = 0;
         surfaceIndex < source.surfaces.size();
         ++surfaceIndex)
    {
        const sed::Surface& before = source.surfaces[surfaceIndex];
        const sed::Surface& after = placed.surfaces[surfaceIndex];
        if (after.vertices.size() != before.vertices.size()
            || after.texVertices.size() != before.texVertices.size()
            || after.uScale != static_cast<float>(
                static_cast<double>(before.uScale) / uDivisor)
            || after.vScale != static_cast<float>(
                static_cast<double>(before.vScale) / vDivisor))
        {
            return false;
        }
        for (std::size_t outputCorner = 0;
             outputCorner < after.vertices.size();
             ++outputCorner)
        {
            const auto sourceCorner = std::find(
                before.vertices.begin(),
                before.vertices.end(),
                after.vertices[outputCorner]);
            if (sourceCorner == before.vertices.end())
            {
                return false;
            }
            const std::size_t sourceCornerIndex =
                static_cast<std::size_t>(std::distance(
                    before.vertices.begin(), sourceCorner));
            const sed::TexVertex& sourceTexVertex =
                before.texVertices[sourceCornerIndex];
            const sed::TexVertex& placedTexVertex =
                after.texVertices[outputCorner];
            if (placedTexVertex.u != static_cast<float>(
                    static_cast<double>(sourceTexVertex.u) / uDivisor)
                || placedTexVertex.v != static_cast<float>(
                    static_cast<double>(sourceTexVertex.v) / vDivisor)
                || placedTexVertex.color != sourceTexVertex.color)
            {
                return false;
            }
        }
    }
    return true;
}

bool primitiveUvsAreCompleteAndEdgeAligned(
    const sed::Level& level,
    const sed::Sector& sector) noexcept
{
    const double coordinateUnitsPerWorldUnit =
        level.uvCoordinateSpace == sed::UvCoordinateSpace::Normalized
        ? sed::level::levelPixelPerUnit(level)
            / sed::level::DefaultGeneratedSectorTextureDimensions.width
        : sed::level::levelPixelPerUnit(level);
    for ( const sed::Surface& surface : sector.surfaces )
    {
        if ( surface.vertices.size() < 3
            || surface.vertices.size() != surface.texVertices.size() )
        {
            return false;
        }

        float minV = std::numeric_limits<float>::infinity();
        float maxV = -std::numeric_limits<float>::infinity();
        for ( const sed::TexVertex& texVertex : surface.texVertices )
        {
            if ( !std::isfinite(texVertex.u) || !std::isfinite(texVertex.v) )
            {
                return false;
            }
            minV = std::min(minV, texVertex.v);
            maxV = std::max(maxV, texVertex.v);
        }

        const std::int32_t firstIndex = surface.vertices[0];
        const std::int32_t secondIndex = surface.vertices[1];
        if ( firstIndex < 0 || secondIndex < 0
            || static_cast<std::size_t>(firstIndex) >= sector.vertices.size()
            || static_cast<std::size_t>(secondIndex) >= sector.vertices.size() )
        {
            return false;
        }

        const double edgeLength = (
            vertexPosition(sector, static_cast<std::size_t>(secondIndex))
            - vertexPosition(sector, static_cast<std::size_t>(firstIndex))).Length();
        const double edgeU = static_cast<double>(surface.texVertices[1].u)
            - static_cast<double>(surface.texVertices[0].u);
        const double edgeV = static_cast<double>(surface.texVertices[1].v)
            - static_cast<double>(surface.texVertices[0].v);
        if ( !(edgeLength > 0.0)
            || !close(edgeU, edgeLength * coordinateUnitsPerWorldUnit, 1.0e-3)
            || !close(edgeV, 0.0, 1.0e-3)
            || !(static_cast<double>(maxV) - static_cast<double>(minV) > 0.0) )
        {
            return false;
        }
    }
    return !sector.surfaces.empty();
}

bool primitiveUvsSurviveImmediateGeometryRecalculation(
    const sed::Level& level,
    const sed::Sector& sector) noexcept
{
    sed::Level recalculated = level;
    if ( sector.num < 0
        || static_cast<std::size_t>(sector.num) >= recalculated.sectors.size() )
    {
        return false;
    }

    sed::level::levelRecalcSector(recalculated, sector.num);
    const sed::Sector& recalculatedSector =
        recalculated.sectors[static_cast<std::size_t>(sector.num)];

    if ( recalculatedSector.surfaces.size() != sector.surfaces.size() )
    {
        return false;
    }
    for ( std::size_t surfaceIndex = 0;
        surfaceIndex < sector.surfaces.size();
        ++surfaceIndex )
    {
        const std::vector<sed::TexVertex>& original =
            sector.surfaces[surfaceIndex].texVertices;
        const std::vector<sed::TexVertex>& refreshed =
            recalculatedSector.surfaces[surfaceIndex].texVertices;
        if ( original.size() != refreshed.size() )
        {
            return false;
        }
        for ( std::size_t vertexIndex = 0;
            vertexIndex < original.size();
            ++vertexIndex )
        {
            if ( !sameTextureVertex(
                    original[vertexIndex], refreshed[vertexIndex]) )
            {
                return false;
            }
        }
    }
    return true;
}

} // namespace

void runLevelBuildTests()
{
    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;

        sed::Sector& sector = sed::level::levelCreateCube(level, 0.0, 0.0, 0.0, sed::TVector::up, sed::TVector::right);

        check(level.sectors.size() == 1, "createCube: level.sectors.size");
        check(level.uvCoordinateSpace == sed::UvCoordinateSpace::Normalized,
            "createCube: default normalized Level retains its UV-space tag");
        check(sector.vertices.size() == 8, "createCube: sector.vertices.size");
        check(sector.surfaces.size() == 6, "createCube: sector.surfaces.size");

        auto vx = [&](const std::size_t i) -> sed::TVector
        {
            const sed::Vertex& v = sector.vertices[i];
            return sed::TVector{ v.x, v.y, v.z };
        };

        check(closeVec(vx(0), sed::TVector{ 0.0, 0.0, 0.0 }), "createCube: v0");
        check(closeVec(vx(1), sed::TVector{ 1.0, 0.0, 0.0 }), "createCube: v1");
        check(closeVec(vx(2), sed::TVector{ 1.0, 1.0, 0.0 }), "createCube: v2");
        check(closeVec(vx(3), sed::TVector{ 0.0, 1.0, 0.0 }), "createCube: v3");

        check(closeVec(vx(4), sed::TVector{ 0.0, 1.0, 1.0 }), "createCube: v4");
        check(closeVec(vx(5), sed::TVector{ 1.0, 1.0, 1.0 }), "createCube: v5");
        check(closeVec(vx(6), sed::TVector{ 1.0, 0.0, 1.0 }), "createCube: v6");
        check(closeVec(vx(7), sed::TVector{ 0.0, 0.0, 1.0 }), "createCube: v7");

        check(sector.surfaces.size() >= 2, "createCube: surface count >= 2");
        if (sector.surfaces.size() >= 2)
        {
            const sed::Surface& bottom = sector.surfaces[0];
            const sed::Surface& top    = sector.surfaces[1];

            check(bottom.vertices.size() == 4, "createCube: bottom.vertexCount");
            check(top.vertices.size() == 4, "createCube: top.vertexCount");

            check(bottom.vertices.size() == bottom.texVertices.size(), "createCube: bottom.texVertexCount");
            check(top.vertices.size() == top.texVertices.size(), "createCube: top.texVertexCount");

            check(bottom.vertices.size() == 4 && bottom.vertices[0] == 0, "createCube: bottom.v0");
            check(bottom.vertices.size() == 4 && bottom.vertices[1] == 1, "createCube: bottom.v1");
            check(bottom.vertices.size() == 4 && bottom.vertices[2] == 2, "createCube: bottom.v2");
            check(bottom.vertices.size() == 4 && bottom.vertices[3] == 3, "createCube: bottom.v3");

            check(top.vertices.size() == 4 && top.vertices[0] == 4, "createCube: top.v0");
            check(top.vertices.size() == 4 && top.vertices[1] == 5, "createCube: top.v1");
            check(top.vertices.size() == 4 && top.vertices[2] == 6, "createCube: top.v2");
            check(top.vertices.size() == 4 && top.vertices[3] == 7, "createCube: top.v3");

            check(closeVec(bottom.normal, sed::TVector::up, kRotationEpsilon), "createCube: bottom.normal");
            check(closeVec(top.normal, sed::TVector::down, kRotationEpsilon), "createCube: top.normal");

            check((bottom.surfaceFlags & sed::SF_Floor) != 0, "createCube: bottom.SF_Floor");
            check((top.surfaceFlags & sed::SF_Floor) == 0, "createCube: top.SF_Floor");

            check(bottom.material == "dflt.mat", "createCube: bottom.material");
            check(top.material == "dflt.mat", "createCube: top.material");

            check(bottom.geo == 4, "createCube: bottom.geo");
            check(bottom.light == 3, "createCube: bottom.light");
            check(bottom.tex == 1, "createCube: bottom.tex");
            check(close(bottom.uScale, 1.0 / 64.0)
                    && close(bottom.vScale, 1.0 / 64.0)
                    && close(bottom.texVertices[1].u, 5.0),
                "createCube: default normalized UVs use the built-in material dimensions");
        }
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;
        level.masterCMP = "dflt.cmp";

        const std::int32_t cube = sed::level::levelCreateSectorPrimitiveBox(
            level,
            sed::level::SectorPrimitive::Cube,
            sed::TVector{ -2.0, 1.0, 3.0 },
            sed::TVector{ 4.0, 5.0, 9.0 });

        check(cube == 0, "createCubeBox: index");
        check(level.sectors.size() == 1, "createCubeBox: level.sectors.size");
        check(level.sectors[0].vertices.size() == 8, "createCubeBox: sector.vertices.size");
        check(level.sectors[0].colormap == "dflt.cmp", "createCubeBox: sector inherits master colormap");
        check(level.sectors[0].ambient == sed::ColorWhite, "createCubeBox: sector keeps full-bright ambient");

        double minX = 100000.0;
        double minY = 100000.0;
        double minZ = 100000.0;
        double maxX = -100000.0;
        double maxY = -100000.0;
        double maxZ = -100000.0;
        for (const sed::Vertex& vertex : level.sectors[0].vertices)
        {
            minX = std::min(minX, vertex.x);
            minY = std::min(minY, vertex.y);
            minZ = std::min(minZ, vertex.z);
            maxX = std::max(maxX, vertex.x);
            maxY = std::max(maxY, vertex.y);
            maxZ = std::max(maxZ, vertex.z);
        }

        check(close(minX, -2.0), "createCubeBox: minX");
        check(close(minY, 1.0), "createCubeBox: minY");
        check(close(minZ, 3.0), "createCubeBox: minZ");
        check(close(maxX, 4.0), "createCubeBox: maxX");
        check(close(maxY, 5.0), "createCubeBox: maxY");
        check(close(maxZ, 9.0), "createCubeBox: maxZ");
    }

    {
        constexpr std::array projectTypes{
            sed::ProjectType::JKDF2,
            sed::ProjectType::MOTS,
        };
        constexpr std::array primitives{
            sed::level::SectorPrimitive::Cube,
            sed::level::SectorPrimitive::Cone,
            sed::level::SectorPrimitive::Cylinder,
            sed::level::SectorPrimitive::UvSphere,
            sed::level::SectorPrimitive::IcoSphere,
        };

        bool allClassicPrimitivesCreated = true;
        bool allClassicPrimitiveUvsComplete = true;
        bool allClassicPrimitiveUvsImmediate = true;
        for ( const sed::ProjectType projectType : projectTypes )
        {
            for ( const sed::level::SectorPrimitive primitive : primitives )
            {
                sed::Level level{};
                level.clear();
                level.kind = projectType;
                level.ppunit = projectType == sed::ProjectType::JKDF2
                    ? 96.0
                    : 160.0;

                const std::int32_t sectorIndex =
                    sed::level::levelCreateSectorPrimitiveBoxAxes(
                        level,
                        primitive,
                        sed::TVector{ -2.0, 1.0, -3.0 },
                        sed::TVector{ 4.0, 6.0, 5.0 },
                        1,
                        2,
                        0,
                        8);
                const bool created = sectorIndex == 0
                    && level.sectors.size() == 1u;
                allClassicPrimitivesCreated =
                    allClassicPrimitivesCreated && created;
                if ( !created )
                {
                    continue;
                }

                const sed::Sector& sector = level.sectors.front();
                allClassicPrimitiveUvsComplete =
                    allClassicPrimitiveUvsComplete
                    && primitiveUvsAreCompleteAndEdgeAligned(level, sector);
                allClassicPrimitiveUvsImmediate =
                    allClassicPrimitiveUvsImmediate
                    && primitiveUvsSurviveImmediateGeometryRecalculation(
                        level, sector);
            }
        }

        check(allClassicPrimitivesCreated,
            "createPrimitiveBoxAxes.classicUvs: creates every JKDF2 and MotS primitive");
        check(allClassicPrimitiveUvsComplete,
            "createPrimitiveBoxAxes.classicUvs: returns complete non-collapsed UVs aligned to face edge zero at the level pixels-per-unit");
        check(allClassicPrimitiveUvsImmediate,
            "createPrimitiveBoxAxes.classicUvs: UVs are final on return and survive the first geometry-only recalculation");
    }

    {
        constexpr std::array projectTypes{
            sed::ProjectType::JKDF2,
            sed::ProjectType::MOTS,
        };
        const std::array<std::vector<std::int32_t>, 6> expectedFaces{
            std::vector<std::int32_t>{ 0, 1, 2, 3 },
            std::vector<std::int32_t>{ 7, 6, 5, 4 },
            std::vector<std::int32_t>{ 0, 3, 7, 4 },
            std::vector<std::int32_t>{ 1, 0, 4, 5 },
            std::vector<std::int32_t>{ 2, 1, 5, 6 },
            std::vector<std::int32_t>{ 3, 2, 6, 7 },
        };
        const std::array<std::array<float, 2>, 4> expectedFaceUvs{
            std::array<float, 2>{ 0.0f, 0.0f },
            std::array<float, 2>{ 320.0f, 0.0f },
            std::array<float, 2>{ 320.0f, -320.0f },
            std::array<float, 2>{ 0.0f, -320.0f },
        };

        bool exactClassicCubeTopology = true;
        bool exactClassicCubeUvs = true;
        for ( const sed::ProjectType projectType : projectTypes )
        {
            sed::Level level{};
            level.clear();
            level.kind = projectType;
            level.ppunit = 320.0;
            level.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;

            const std::int32_t sectorIndex =
                sed::level::levelCreateSectorPrimitiveBoxAxes(
                    level,
                    sed::level::SectorPrimitive::Cube,
                    sed::TVector{ 0.0, 0.0, 0.0 },
                    sed::TVector{ 1.0, 1.0, 1.0 },
                    0,
                    1,
                    2,
                    0);
            if ( sectorIndex != 0 || level.sectors.size() != 1u )
            {
                exactClassicCubeTopology = false;
                exactClassicCubeUvs = false;
                continue;
            }

            const sed::Sector& sector = level.sectors.front();
            if ( sector.surfaces.size() != expectedFaces.size() )
            {
                exactClassicCubeTopology = false;
                exactClassicCubeUvs = false;
                continue;
            }
            for ( std::size_t surfaceIndex = 0;
                surfaceIndex < expectedFaces.size();
                ++surfaceIndex )
            {
                const sed::Surface& surface = sector.surfaces[surfaceIndex];
                exactClassicCubeTopology = exactClassicCubeTopology
                    && surface.vertices == expectedFaces[surfaceIndex];
                if ( surface.texVertices.size() != expectedFaceUvs.size() )
                {
                    exactClassicCubeUvs = false;
                    continue;
                }
                for ( std::size_t cornerIndex = 0;
                    cornerIndex < expectedFaceUvs.size();
                    ++cornerIndex )
                {
                    exactClassicCubeUvs = exactClassicCubeUvs
                        && surface.texVertices[cornerIndex].u
                            == expectedFaceUvs[cornerIndex][0]
                        && surface.texVertices[cornerIndex].v
                            == expectedFaceUvs[cornerIndex][1];
                }
            }
        }

        check(exactClassicCubeTopology,
            "createPrimitiveBoxAxes.classicCube: face order and corner anchors match Pascal CreateCubeSec");
        check(exactClassicCubeUvs,
            "createPrimitiveBoxAxes.classicCube: every face starts with the classic 320-pixel edge-aligned UV tuple");
    }

    {
        constexpr sed::level::UvTextureDimensions dimensions{ 128.0, 32.0 };
        const auto resolveDimensions = [dimensions](const sed::Surface&)
            -> std::optional<sed::level::UvTextureDimensions>
        {
            return dimensions;
        };

        sed::Level pixelLevel{};
        pixelLevel.clear();
        pixelLevel.kind = sed::ProjectType::JKDF2;
        pixelLevel.ppunit = 320.0;
        pixelLevel.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;
        const std::int32_t pixelSector =
            sed::level::levelCreateSectorPrimitiveBoxAxes(
                pixelLevel,
                sed::level::SectorPrimitive::Cube,
                sed::TVector{ 0.0, 0.0, 0.0 },
                sed::TVector{ 1.0, 1.0, 1.0 },
                0,
                1,
                2,
                0,
                resolveDimensions);

        sed::Level normalizedLevel{};
        normalizedLevel.clear();
        normalizedLevel.kind = sed::ProjectType::JKDF2;
        normalizedLevel.ppunit = 320.0;
        normalizedLevel.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        const std::int32_t normalizedSector =
            sed::level::levelCreateSectorPrimitiveBoxAxes(
                normalizedLevel,
                sed::level::SectorPrimitive::Cube,
                sed::TVector{ 0.0, 0.0, 0.0 },
                sed::TVector{ 1.0, 1.0, 1.0 },
                0,
                1,
                2,
                0,
                resolveDimensions);

        bool equivalent = pixelSector == 0 && normalizedSector == 0
            && pixelLevel.sectors.size() == 1u
            && normalizedLevel.sectors.size() == 1u
            && pixelLevel.sectors[0].surfaces.size()
                == normalizedLevel.sectors[0].surfaces.size();
        for ( std::size_t surfaceIndex = 0;
            equivalent
                && surfaceIndex < pixelLevel.sectors[0].surfaces.size();
            ++surfaceIndex )
        {
            const sed::Surface& pixel =
                pixelLevel.sectors[0].surfaces[surfaceIndex];
            const sed::Surface& normalized =
                normalizedLevel.sectors[0].surfaces[surfaceIndex];
            equivalent = pixel.texVertices.size()
                    == normalized.texVertices.size()
                && close(normalized.uScale, pixel.uScale / dimensions.width)
                && close(normalized.vScale, pixel.vScale / dimensions.height);
            for ( std::size_t cornerIndex = 0;
                equivalent && cornerIndex < pixel.texVertices.size();
                ++cornerIndex )
            {
                equivalent = close(
                        normalized.texVertices[cornerIndex].u,
                        pixel.texVertices[cornerIndex].u / dimensions.width)
                    && close(
                        normalized.texVertices[cornerIndex].v,
                        pixel.texVertices[cornerIndex].v / dimensions.height);
            }
        }
        check(equivalent
                && normalizedLevel.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized,
            "createPrimitiveBoxAxes.normalized: explicit anisotropic dimensions convert every generated corner and scale before publication");

        sed::Level missingDimensions{};
        missingDimensions.clear();
        missingDimensions.newSector().id = 741;
        const std::size_t missingSectorCount = missingDimensions.sectors.size();
        const std::int32_t missingResult =
            sed::level::levelCreateSectorPrimitiveBoxAxes(
                missingDimensions,
                sed::level::SectorPrimitive::Cube,
                sed::TVector{ 0.0, 0.0, 0.0 },
                sed::TVector{ 1.0, 1.0, 1.0 },
                0,
                1,
                2,
                0,
                {});
        check(missingResult == -1
                && missingDimensions.sectors.size() == missingSectorCount
                && missingDimensions.sectors[0].id == 741,
            "createPrimitiveBoxAxes.normalized: missing dimensions reject without publishing a partial Sector");

        sed::Level invalidDimensions = missingDimensions;
        const std::int32_t invalidResult =
            sed::level::levelCreateSectorPrimitiveBoxAxes(
                invalidDimensions,
                sed::level::SectorPrimitive::Cube,
                sed::TVector{ 0.0, 0.0, 0.0 },
                sed::TVector{ 1.0, 1.0, 1.0 },
                0,
                1,
                2,
                0,
                [](const sed::Surface&)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    return sed::level::UvTextureDimensions{ 128.0, 0.0 };
                });
        check(invalidResult == -1
                && invalidDimensions.sectors.size() == missingSectorCount
                && invalidDimensions.sectors[0].id == 741,
            "createPrimitiveBoxAxes.normalized: invalid dimensions reject atomically");

        sed::Level resolvedCube{};
        resolvedCube.clear();
        resolvedCube.ppunit = 320.0;
        const std::optional<std::int32_t> resolvedCubeIndex =
            sed::level::levelCreateCube(
                resolvedCube,
                0.0,
                0.0,
                0.0,
                sed::TVector::up,
                sed::TVector::right,
                resolveDimensions);
        check(resolvedCubeIndex == 0
                && resolvedCube.sectors.size() == 1u
                && close(resolvedCube.sectors[0].surfaces[0].uScale,
                    1.0 / dimensions.width)
                && close(resolvedCube.sectors[0].surfaces[0].vScale,
                    1.0 / dimensions.height)
                && close(resolvedCube.sectors[0].surfaces[0]
                        .texVertices[1].u,
                    320.0 / dimensions.width),
            "createCube.normalized: explicit dimensions publish converted UVs and scales");

        sed::Level unresolvedCube{};
        unresolvedCube.clear();
        unresolvedCube.newSector().id = 852;
        const std::optional<std::int32_t> unresolvedCubeIndex =
            sed::level::levelCreateCube(
                unresolvedCube,
                0.0,
                0.0,
                0.0,
                sed::TVector::up,
                sed::TVector::right,
                {});
        check(!unresolvedCubeIndex
                && unresolvedCube.sectors.size() == 1u
                && unresolvedCube.sectors[0].id == 852,
            "createCube.normalized: missing dimensions reject atomically");

        sed::Level unresolvedCubeSectorLevel{};
        unresolvedCubeSectorLevel.clear();
        sed::Sector unresolvedCubeSector{};
        unresolvedCubeSector.id = 963;
        const bool cubeSectorCreated = sed::level::levelCreateCubeSec(
            unresolvedCubeSectorLevel,
            unresolvedCubeSector,
            0.0,
            0.0,
            0.0,
            sed::TVector::up,
            sed::TVector::right,
            {});
        check(!cubeSectorCreated
                && unresolvedCubeSector.vertices.empty()
                && unresolvedCubeSector.surfaces.empty()
                && unresolvedCubeSector.id == 963,
            "createCubeSec.normalized: missing dimensions leave the destination Sector unchanged");
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;

        const std::int32_t cylinder = sed::level::levelCreateSectorPrimitiveBoxAxes(
            level,
            sed::level::SectorPrimitive::Cylinder,
            sed::TVector{ 0.0, 0.0, 0.0 },
            sed::TVector{ 4.0, 2.0, 2.0 },
            1,
            2,
            0,
            8);

        check(cylinder == 0, "createCylinderBoxAxes: index");
        check(level.sectors.size() == 1, "createCylinderBoxAxes: level.sectors.size");
        check(level.sectors[0].vertices.size() == 16, "createCylinderBoxAxes: sector.vertices.size");
        check(level.sectors[0].surfaces.size() == 10, "createCylinderBoxAxes: sector.surfaces.size");

        const TestBounds bounds = sectorBounds(level.sectors[0]);
        check(closeVec(bounds.min, sed::TVector{ 0.0, 0.0, 0.0 }), "createCylinderBoxAxes: bounds.min");
        check(closeVec(bounds.max, sed::TVector{ 4.0, 2.0, 2.0 }), "createCylinderBoxAxes: bounds.max");
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;

        const std::int32_t cylinder = sed::level::levelCreateSectorPrimitive(
            level,
            sed::level::SectorPrimitive::Cylinder,
            sed::TVector{ 1.0, 2.0, 3.0 },
            1.0);

        check(cylinder == 0, "createCylinder: index");
        check(level.sectors.size() == 1, "createCylinder: level.sectors.size");
        check(level.sectors[0].vertices.size() == 32, "createCylinder: sector.vertices.size");
        check(level.sectors[0].surfaces.size() == 18, "createCylinder: sector.surfaces.size");

        for (const sed::Surface& surface : level.sectors[0].surfaces)
        {
            check(surface.vertices.size() == surface.texVertices.size(), "createCylinder: tex vertex count");
        }
    }

    {
        const sed::Sector cube = sed::level::makeSectorPrimitiveShape(sed::ProjectType::JKDF2, sed::level::SectorPrimitive::Cube);
        check(cube.vertices.size() == 8, "makeSectorPrimitiveShape.cube: vertices");
        check(cube.surfaces.size() == 6, "makeSectorPrimitiveShape.cube: surfaces");
        check(!cube.surfaces.empty() && cube.surfaces[0].tex == 1, "makeSectorPrimitiveShape.cube: tex jk");
        check(!cube.surfaces.empty() && (cube.surfaces[0].adjoinFlags & sed::SAF_AllowSoundPass) != 0,
            "makeSectorPrimitiveShape.cube: sound pass jk");

        const sed::Sector ijimCube =
            sed::level::makeSectorPrimitiveShape(sed::ProjectType::IJIM, sed::level::SectorPrimitive::Cube);
        check(!ijimCube.surfaces.empty() && ijimCube.surfaces[0].tex == 3, "makeSectorPrimitiveShape.cube: tex ijim");
        check(!ijimCube.surfaces.empty() && (ijimCube.surfaces[0].adjoinFlags & sed::SAF_AllowSoundPass) == 0,
            "makeSectorPrimitiveShape.cube: sound pass ijim");

        const sed::Sector cone = sed::level::makeSectorPrimitiveShape(sed::ProjectType::JKDF2, sed::level::SectorPrimitive::Cone, 1);
        check(cone.vertices.size() == 4, "makeSectorPrimitiveShape.coneClamped: vertices");
        check(cone.surfaces.size() == 4, "makeSectorPrimitiveShape.coneClamped: surfaces");

        const sed::Sector cylinder =
            sed::level::makeSectorPrimitiveShape(sed::ProjectType::JKDF2, sed::level::SectorPrimitive::Cylinder, 5);
        check(cylinder.vertices.size() == 10, "makeSectorPrimitiveShape.cylinder5: vertices");
        check(cylinder.surfaces.size() == 7, "makeSectorPrimitiveShape.cylinder5: surfaces");

        const sed::Sector uvSphere =
            sed::level::makeSectorPrimitiveShape(sed::ProjectType::JKDF2, sed::level::SectorPrimitive::UvSphere, 2);
        check(uvSphere.vertices.size() == 10, "makeSectorPrimitiveShape.uvSphereClamped: vertices");
        check(uvSphere.surfaces.size() == 12, "makeSectorPrimitiveShape.uvSphereClamped: surfaces");

        const sed::Sector icoSphere =
            sed::level::makeSectorPrimitiveShape(sed::ProjectType::JKDF2, sed::level::SectorPrimitive::IcoSphere);
        check(icoSphere.vertices.size() == 12, "makeSectorPrimitiveShape.icoSphere: vertices");
        check(icoSphere.surfaces.size() == 20, "makeSectorPrimitiveShape.icoSphere: surfaces");
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;

        const sed::Sector shape = sed::level::makeSectorPrimitiveShape(sed::ProjectType::JKDF2, sed::level::SectorPrimitive::Cube);
        const std::int32_t mapped = sed::level::levelCreateSectorFromShapeBoxAxes(
            level,
            shape,
            sed::TVector{ 10.0, 20.0, 30.0 },
            sed::TVector{ 14.0, 26.0, 38.0 },
            1,
            2,
            0);

        check(mapped == 0, "createSectorFromShapeBoxAxes.remap: index");
        check(level.sectors.size() == 1, "createSectorFromShapeBoxAxes.remap: sector count");
        check(level.sectors[0].vertices.size() == shape.vertices.size(), "createSectorFromShapeBoxAxes.remap: vertex count");
        check(closeVec(vertexPosition(level.sectors[0], 0), sed::TVector{ 10.0, 20.0, 30.0 }),
            "createSectorFromShapeBoxAxes.remap: v0");
        check(closeVec(vertexPosition(level.sectors[0], 1), sed::TVector{ 10.0, 26.0, 30.0 }),
            "createSectorFromShapeBoxAxes.remap: v1");
        check(closeVec(vertexPosition(level.sectors[0], 4), sed::TVector{ 14.0, 20.0, 30.0 }),
            "createSectorFromShapeBoxAxes.remap: v4");

        const TestBounds bounds = sectorBounds(level.sectors[0]);
        check(closeVec(bounds.min, sed::TVector{ 10.0, 20.0, 30.0 }), "createSectorFromShapeBoxAxes.remap: bounds.min");
        check(closeVec(bounds.max, sed::TVector{ 14.0, 26.0, 38.0 }), "createSectorFromShapeBoxAxes.remap: bounds.max");
        check(allSurfaceNormalsFaceSectorInterior(level.sectors[0]),
            "createSectorFromShapeBoxAxes.XPlane: inward normals");
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;

        sed::Sector shape = sed::level::makeSectorPrimitiveShape(
            sed::ProjectType::JKDF2,
            sed::level::SectorPrimitive::Cube);
        for ( std::size_t surfaceIndex = 0;
            surfaceIndex < shape.surfaces.size();
            ++surfaceIndex )
        {
            sed::Surface& surface = shape.surfaces[surfaceIndex];
            surface.uScale = static_cast<float>(
                -0.015625 * static_cast<double>(surfaceIndex + 1u));
            surface.vScale = static_cast<float>(
                0.0234375 * static_cast<double>(surfaceIndex + 1u));
            for ( std::size_t corner = 0;
                corner < surface.texVertices.size();
                ++corner )
            {
                surface.texVertices[corner] = sed::TexVertex{
                    static_cast<float>(
                        -2.25 + 11.5 * surfaceIndex + 3.75 * corner),
                    static_cast<float>(
                        5.5 - 7.25 * surfaceIndex - 2.5 * corner),
                    sed::ColorF{
                        static_cast<float>(0.03 * (surfaceIndex + 1u)),
                        static_cast<float>(0.05 * (corner + 1u)),
                        static_cast<float>(0.02 * (surfaceIndex + corner + 1u)),
                        static_cast<float>(0.4 + 0.05 * corner)
                    }
                };
            }
        }

        const std::int32_t mapped =
            sed::level::levelCreateSectorFromShapeBoxAxes(
                level,
                shape,
                sed::TVector{ -3.0, 2.0, -5.0 },
                sed::TVector{ 7.0, 11.0, 13.0 },
                1,
                2,
                0);
        bool exactTextureState = mapped == 0
            && level.uvCoordinateSpace
                == sed::UvCoordinateSpace::Normalized
            && level.sectors.size() == 1u
            && level.sectors[0].surfaces.size()
                == shape.surfaces.size();
        if ( exactTextureState )
        {
            for ( std::size_t surfaceIndex = 0;
                surfaceIndex < shape.surfaces.size();
                ++surfaceIndex )
            {
                const sed::Surface& source = shape.surfaces[surfaceIndex];
                const sed::Surface& output =
                    level.sectors[0].surfaces[surfaceIndex];
                exactTextureState = exactTextureState
                    && output.vertices.size() == source.vertices.size()
                    && output.texVertices.size()
                        == source.texVertices.size()
                    && output.uScale == source.uScale
                    && output.vScale == source.vScale;
                for ( std::size_t outputCorner = 0;
                    exactTextureState
                        && outputCorner < output.vertices.size();
                    ++outputCorner )
                {
                    const auto sourceCorner = std::find(
                        source.vertices.begin(),
                        source.vertices.end(),
                        output.vertices[outputCorner]);
                    if ( sourceCorner == source.vertices.end() )
                    {
                        exactTextureState = false;
                        break;
                    }
                    const std::size_t sourceCornerIndex =
                        static_cast<std::size_t>(std::distance(
                            source.vertices.begin(), sourceCorner));
                    const sed::TexVertex& before =
                        source.texVertices[sourceCornerIndex];
                    const sed::TexVertex& after =
                        output.texVertices[outputCorner];
                    exactTextureState = exactTextureState
                        && after.u == before.u
                        && after.v == before.v
                        && after.color == before.color;
                }
            }
        }
        check(exactTextureState,
            "createSectorFromShapeBoxAxes.normalizedUv: axis remap and any winding repair preserve every vertex/UV/color tuple and signed scale exactly");
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;

        const sed::Sector shape = sed::level::makeSectorPrimitiveShape(
            sed::ProjectType::JKDF2, sed::level::SectorPrimitive::Cube);
        const std::int32_t mapped = sed::level::levelCreateSectorFromShapeBoxAxes(
            level,
            shape,
            sed::TVector{ 10.0, 20.0, 30.0 },
            sed::TVector{ 14.0, 26.0, 38.0 },
            0,
            2,
            1);

        check(mapped == 0, "createSectorFromShapeBoxAxes.YPlane: index");
        check(allSurfaceNormalsFaceSectorInterior(level.sectors[0]),
            "createSectorFromShapeBoxAxes.YPlane: inward normals");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::JKDF2;
        const sed::Sector shape = sed::level::makeSectorPrimitiveShape(
            sed::ProjectType::JKDF2,
            sed::level::SectorPrimitive::Cube);
        const std::size_t sourceVertexCount = shape.vertices.size();
        const std::size_t sourceSurfaceCount = shape.surfaces.size();
        const sed::Vertex sourceFirstVertex = shape.vertices.front();
        const std::vector<std::int32_t> sourceFirstSurface =
            shape.surfaces.front().vertices;

        const std::int32_t mapped = sed::level::levelCreateSectorFromShapeBox(
            level,
            shape,
            sed::TVector{ 4.0, 6.0, 8.0 },
            sed::TVector{ -2.0, -4.0, -6.0 });

        check(mapped == 0 && level.sectors.size() == 1u,
            "createSectorFromShapeBox.wrapper: creates one sector from reversed corners");
        if ( mapped == 0 )
        {
            const TestBounds bounds = sectorBounds(level.sectors[0]);
            check(closeVec(bounds.min, sed::TVector{ -2.0, -4.0, -6.0 })
                    && closeVec(bounds.max, sed::TVector{ 4.0, 6.0, 8.0 }),
                "createSectorFromShapeBox.wrapper: normalizes and maps all three axes");
            check(level.sectors[0].surfaces.size() == shape.surfaces.size()
                    && allSurfaceNormalsFaceSectorInterior(level.sectors[0]),
                "createSectorFromShapeBox.wrapper: preserves topology and inward winding");
        }
        check(shape.vertices.size() == sourceVertexCount
                && shape.surfaces.size() == sourceSurfaceCount
                && close(shape.vertices.front().x, sourceFirstVertex.x)
                && close(shape.vertices.front().y, sourceFirstVertex.y)
                && close(shape.vertices.front().z, sourceFirstVertex.z)
                && shape.surfaces.front().vertices == sourceFirstSurface,
            "createSectorFromShapeBox.wrapper: leaves the source template unchanged");
    }

    {
        constexpr std::array<ShapePlacementEntryPoint, 3> entryPoints{
            ShapePlacementEntryPoint::FromShape,
            ShapePlacementEntryPoint::Box,
            ShapePlacementEntryPoint::BoxAxes,
        };
        constexpr sed::level::UvTextureDimensions anisotropicDimensions{
            128.0,
            32.0,
        };

        for (const ShapePlacementEntryPoint entryPoint : entryPoints)
        {
            const std::string prefix = std::string(
                "createSectorFromShape.tagged.")
                + shapePlacementEntryPointName(entryPoint);
            const sed::Sector source = makeShapeUvAdmissionFixture();
            const auto checkPlacement =
                [&prefix](const bool condition, const std::string& suffix)
                {
                    const std::string message = prefix + suffix;
                    check(condition, message.c_str());
                };

            {
                sed::Level level{};
                level.clear();
                level.kind = sed::ProjectType::JKDF2;
                level.uvCoordinateSpace =
                    sed::UvCoordinateSpace::Normalized;
                std::size_t resolverCalls{};
                const std::int32_t placed = placeTaggedShape(
                    level,
                    source,
                    sed::UvCoordinateSpace::TexturePixels,
                    entryPoint,
                    [&resolverCalls, anisotropicDimensions](
                        const sed::Surface& surface)
                        -> std::optional<
                            sed::level::UvTextureDimensions>
                    {
                        ++resolverCalls;
                        if (surface.material != "anisotropic.mat")
                        {
                            return std::nullopt;
                        }
                        return anisotropicDimensions;
                    });
                checkPlacement(
                    placed == 0 && level.sectors.size() == 1u,
                    ".pixelToNormalized: publishes one sector");
                checkPlacement(
                    placed == 0
                        && placedShapeTextureStateMatches(
                            source,
                            level.sectors[0],
                            anisotropicDimensions.width,
                            anisotropicDimensions.height),
                    ".pixelToNormalized: converts every signed and multi-tile UV, scale, and color with anisotropic dimensions");
                checkPlacement(
                    resolverCalls == source.surfaces.size(),
                    ".pixelToNormalized: resolves every surface exactly once");
            }

            {
                sed::Level level{};
                level.clear();
                level.kind = sed::ProjectType::JKDF2;
                level.uvCoordinateSpace =
                    sed::UvCoordinateSpace::Normalized;
                std::size_t resolverCalls{};
                const std::int32_t placed = placeTaggedShape(
                    level,
                    source,
                    sed::UvCoordinateSpace::Normalized,
                    entryPoint,
                    [&resolverCalls](const sed::Surface&)
                        -> std::optional<
                            sed::level::UvTextureDimensions>
                    {
                        ++resolverCalls;
                        return std::nullopt;
                    });
                checkPlacement(
                    placed == 0 && level.sectors.size() == 1u,
                    ".normalizedToNormalized: publishes one sector");
                checkPlacement(
                    placed == 0
                        && placedShapeTextureStateMatches(
                            source, level.sectors[0], 1.0, 1.0),
                    ".normalizedToNormalized: preserves UVs, scales, and colors exactly");
                checkPlacement(
                    resolverCalls == 0u,
                    ".normalizedToNormalized: does not resolve dimensions");
            }

            {
                sed::Level level{};
                level.clear();
                level.kind = sed::ProjectType::JKDF2;
                level.uvCoordinateSpace =
                    sed::UvCoordinateSpace::TexturePixels;
                std::size_t resolverCalls{};
                const std::int32_t placed = placeTaggedShape(
                    level,
                    source,
                    sed::UvCoordinateSpace::Normalized,
                    entryPoint,
                    [&resolverCalls, anisotropicDimensions](
                        const sed::Surface&)
                        -> std::optional<
                            sed::level::UvTextureDimensions>
                    {
                        ++resolverCalls;
                        return anisotropicDimensions;
                    });
                checkPlacement(
                    placed == 0 && level.sectors.size() == 1u,
                    ".normalizedToPixel: publishes one sector");
                checkPlacement(
                    placed == 0
                        && placedShapeTextureStateMatches(
                            source,
                            level.sectors[0],
                            1.0 / anisotropicDimensions.width,
                            1.0 / anisotropicDimensions.height),
                    ".normalizedToPixel: converts every UV and scale per anisotropic axis");
                checkPlacement(
                    resolverCalls == source.surfaces.size(),
                    ".normalizedToPixel: resolves every surface exactly once");
            }

            {
                sed::Level level{};
                level.clear();
                level.kind = sed::ProjectType::JKDF2;
                level.uvCoordinateSpace =
                    sed::UvCoordinateSpace::TexturePixels;
                std::size_t resolverCalls{};
                const std::int32_t placed = placeTaggedShape(
                    level,
                    source,
                    sed::UvCoordinateSpace::TexturePixels,
                    entryPoint,
                    [&resolverCalls](const sed::Surface&)
                        -> std::optional<
                            sed::level::UvTextureDimensions>
                    {
                        ++resolverCalls;
                        return std::nullopt;
                    });
                checkPlacement(
                    placed == 0 && level.sectors.size() == 1u,
                    ".pixelToPixel: publishes one sector");
                checkPlacement(
                    placed == 0
                        && placedShapeTextureStateMatches(
                            source, level.sectors[0], 1.0, 1.0),
                    ".pixelToPixel: preserves UVs, scales, and colors exactly");
                checkPlacement(
                    resolverCalls == 0u,
                    ".pixelToPixel: does not resolve dimensions");
            }

            const auto checkFailureRollback =
                [&](sed::Level level,
                    sed::Sector failingShape,
                    const sed::UvCoordinateSpace sourceSpace,
                    const sed::level::UvTextureDimensionsResolver&
                        resolveDimensions,
                    const std::string& scenario)
                {
                    level.curId = 731;
                    const sed::Level before = level;
                    const std::int32_t placed = placeTaggedShape(
                        level,
                        failingShape,
                        sourceSpace,
                        entryPoint,
                        resolveDimensions);
                    checkPlacement(
                        placed == -1,
                        "." + scenario + ": rejects placement");
                    checkPlacement(
                        sed::assets::sedLevelsEqual(level, before),
                        "." + scenario
                            + ": rolls back the full Level bit-exactly");
                };

            sed::Level normalizedLevel{};
            normalizedLevel.clear();
            normalizedLevel.kind = sed::ProjectType::JKDF2;
            normalizedLevel.uvCoordinateSpace =
                sed::UvCoordinateSpace::Normalized;
            sed::Sector& rollbackSentinel = normalizedLevel.newSector();
            rollbackSentinel.id = 517;
            rollbackSentinel.flags = 0x2au;
            rollbackSentinel.layer = 4;
            rollbackSentinel.addVertex(-7.0, 8.0, -9.0);

            checkFailureRollback(
                normalizedLevel,
                source,
                sed::UvCoordinateSpace::TexturePixels,
                {},
                "missingDimensions");
            checkFailureRollback(
                normalizedLevel,
                source,
                sed::UvCoordinateSpace::TexturePixels,
                [](const sed::Surface&)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    return sed::level::UvTextureDimensions{ 128.0, 0.0 };
                },
                "invalidDimensions");
            checkFailureRollback(
                normalizedLevel,
                source,
                static_cast<sed::UvCoordinateSpace>(99),
                [anisotropicDimensions](const sed::Surface&)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    return anisotropicDimensions;
                },
                "invalidSourceTag");

            sed::Level invalidDestinationLevel = normalizedLevel;
            invalidDestinationLevel.uvCoordinateSpace =
                static_cast<sed::UvCoordinateSpace>(99);
            checkFailureRollback(
                invalidDestinationLevel,
                source,
                sed::UvCoordinateSpace::TexturePixels,
                [anisotropicDimensions](const sed::Surface&)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    return anisotropicDimensions;
                },
                "invalidDestinationTag");

            sed::Sector nonFiniteShape = source;
            nonFiniteShape.surfaces[0].texVertices[0].u =
                std::numeric_limits<float>::infinity();
            checkFailureRollback(
                normalizedLevel,
                std::move(nonFiniteShape),
                sed::UvCoordinateSpace::TexturePixels,
                [anisotropicDimensions](const sed::Surface&)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    return anisotropicDimensions;
                },
                "nonFiniteCoordinate");

            sed::Level pixelLevel = normalizedLevel;
            pixelLevel.uvCoordinateSpace =
                sed::UvCoordinateSpace::TexturePixels;
            sed::Sector overflowShape = source;
            overflowShape.surfaces[0].uScale =
                std::numeric_limits<float>::max();
            checkFailureRollback(
                pixelLevel,
                std::move(overflowShape),
                sed::UvCoordinateSpace::Normalized,
                [](const sed::Surface&)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    return sed::level::UvTextureDimensions{
                        std::numeric_limits<double>::max(),
                        32.0,
                    };
                },
                "overflowScale");
        }
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;

        const sed::Sector shape = sed::level::makeSectorPrimitiveShape(sed::ProjectType::JKDF2, sed::level::SectorPrimitive::Cube);
        check(sed::level::levelCreateSectorFromShape(level, sed::Sector{}, sed::TVector::zero, 1.0) == -1,
            "createSectorFromShape.invalid: empty");
        check(sed::level::levelCreateSectorFromShape(level, shape, sed::TVector::zero, 0.0) == -1,
            "createSectorFromShape.invalid: zero size");
        check(sed::level::levelCreateSectorFromShape(level, shape, sed::TVector::zero, -1.0) == -1,
            "createSectorFromShape.invalid: negative size");
        check(sed::level::levelCreateSectorFromShape(level, shape, sed::TVector::zero, std::numeric_limits<double>::infinity()) == -1,
            "createSectorFromShape.invalid: infinite size");
        check(sed::level::levelCreateSectorFromShape(level, makeDegenerateShape(), sed::TVector::zero, 1.0) == -1,
            "createSectorFromShape.invalid: degenerate shape");

        check(sed::level::levelCreateSectorFromShapeBoxAxes(
                  level, shape, sed::TVector::zero, sed::TVector{ 1.0, 1.0, 1.0 }, 0, 0, 2)
                == -1,
            "createSectorFromShapeBoxAxes.invalid: duplicate axis");
        check(sed::level::levelCreateSectorFromShapeBoxAxes(
                  level, shape, sed::TVector::zero, sed::TVector{ 1.0, 1.0, 1.0 }, 0, 1, 3)
                == -1,
            "createSectorFromShapeBoxAxes.invalid: out of range axis");
        check(sed::level::levelCreateSectorFromShapeBoxAxes(
                  level,
                  shape,
                  sed::TVector{ 0.0, 0.0, 0.0 },
                  sed::TVector{ 0.0, 1.0, 1.0 },
                  0,
                  1,
                  2)
                == -1,
            "createSectorFromShapeBoxAxes.invalid: zero x extent");
        check(sed::level::levelCreateSectorFromShapeBoxAxes(
                  level,
                  shape,
                  sed::TVector{ 0.0, 0.0, 0.0 },
                  sed::TVector{ std::numeric_limits<double>::infinity(), 1.0, 1.0 },
                  0,
                  1,
                  2)
                == -1,
            "createSectorFromShapeBoxAxes.invalid: infinite corner");
        check(sed::level::levelCreateSectorPrimitive(level, sed::level::SectorPrimitive::Cube, sed::TVector::zero, 0.0) == -1,
            "createSectorPrimitive.invalid: zero size");
        check(sed::level::levelCreateSectorPrimitiveBox(
                  level, sed::level::SectorPrimitive::Cube, sed::TVector::zero, sed::TVector{ 0.0, 1.0, 1.0 })
                == -1,
            "createSectorPrimitiveBox.invalid: zero extent");

        check(level.sectors.empty(), "build invalid calls do not append sectors");
    }

    {
        constexpr std::array<ShapePlacementEntryPoint, 3> entryPoints{
            ShapePlacementEntryPoint::FromShape,
            ShapePlacementEntryPoint::Box,
            ShapePlacementEntryPoint::BoxAxes,
        };
        struct CardinalityCase
        {
            const char* name{};
            std::size_t textureCoordinateCount{};
        };
        const std::array cardinalityCases{
            CardinalityCase{ "missing corner coordinate", 3u },
            CardinalityCase{ "excess corner coordinate", 5u },
        };

        for (const ShapePlacementEntryPoint entryPoint : entryPoints)
        {
            for (const bool crossSpace : { false, true })
            {
                for (const CardinalityCase& testCase : cardinalityCases)
                {
                    sed::Sector malformed = makeShapeUvAdmissionFixture();
                    malformed.surfaces[0].texVertices.resize(
                        testCase.textureCoordinateCount,
                        sed::TexVertex{ 17.0f, -23.0f, sed::ColorRed });

                    sed::Level level{};
                    level.clear();
                    level.kind = sed::ProjectType::JKDF2;
                    level.uvCoordinateSpace =
                        sed::UvCoordinateSpace::Normalized;
                    const sed::Sector sentinel =
                        sed::level::makeSectorPrimitiveShape(
                            sed::ProjectType::JKDF2,
                            sed::level::SectorPrimitive::Cone);
                    level.sectors.push_back(sentinel);
                    level.sectors[0].num = 0;

                    const std::size_t originalSectorCount =
                        level.sectors.size();
                    const std::vector<sed::Vertex> originalVertices =
                        level.sectors[0].vertices;
                    const std::vector<sed::Surface> originalSurfaces =
                        level.sectors[0].surfaces;
                    std::size_t resolverCalls = 0;
                    const std::int32_t placed = placeTaggedShape(
                        level,
                        malformed,
                        crossSpace
                            ? sed::UvCoordinateSpace::TexturePixels
                            : sed::UvCoordinateSpace::Normalized,
                        entryPoint,
                        [&resolverCalls](const sed::Surface&)
                            -> std::optional<
                                sed::level::UvTextureDimensions>
                        {
                            ++resolverCalls;
                            return sed::level::UvTextureDimensions{
                                128.0,
                                32.0,
                            };
                        });

                    bool sentinelUnchanged = level.sectors.size()
                            == originalSectorCount
                        && level.sectors[0].vertices.size()
                            == originalVertices.size()
                        && level.sectors[0].surfaces.size()
                            == originalSurfaces.size();
                    for (std::size_t vertexIndex = 0;
                         sentinelUnchanged
                            && vertexIndex < originalVertices.size();
                         ++vertexIndex)
                    {
                        const sed::Vertex& actual =
                            level.sectors[0].vertices[vertexIndex];
                        const sed::Vertex& expected =
                            originalVertices[vertexIndex];
                        sentinelUnchanged = actual.x == expected.x
                            && actual.y == expected.y
                            && actual.z == expected.z
                            && actual.mark == expected.mark;
                    }
                    for (std::size_t surfaceIndex = 0;
                         sentinelUnchanged
                            && surfaceIndex < originalSurfaces.size();
                         ++surfaceIndex)
                    {
                        const sed::Surface& actual =
                            level.sectors[0].surfaces[surfaceIndex];
                        const sed::Surface& expected =
                            originalSurfaces[surfaceIndex];
                        sentinelUnchanged = actual.vertices
                                == expected.vertices
                            && actual.texVertices.size()
                                == expected.texVertices.size()
                            && actual.uScale == expected.uScale
                            && actual.vScale == expected.vScale;
                        for (std::size_t corner = 0;
                             sentinelUnchanged
                                && corner < expected.texVertices.size();
                             ++corner)
                        {
                            sentinelUnchanged = sameTextureVertex(
                                actual.texVertices[corner],
                                expected.texVertices[corner]);
                        }
                    }

                    const std::string message =
                        "createSectorFromShape rejects "
                        + std::string(testCase.name)
                        + " atomically through "
                        + shapePlacementEntryPointName(entryPoint)
                        + (crossSpace ? " across UV spaces" : " in normalized space");
                    check(placed == -1
                            && resolverCalls == 0u
                            && level.uvCoordinateSpace
                                == sed::UvCoordinateSpace::Normalized
                            && sentinelUnchanged,
                        message.c_str());
                }
            }
        }
    }

    {
        std::istringstream input(R"tpl(
SEC Mini Tetra
dflt.cmp - 0 0 5 0 0 0 0 0 UV normalized
VXS 4
0 0 0
1 0 0
0 1 0
0 0 1
SURFS 4
dflt.mat 5 4 4 3 1 0 3 -1 -1 7
 0 0 0 5 1 0 0 5 2 0 0 5
dflt.mat 5 4 4 3 1 0 3 -1 -1 7
 0 0 0 5 3 0 0 5 1 0 0 5
dflt.mat 5 4 4 3 1 0 3 -1 -1 7
 1 0 0 5 3 0 0 5 2 0 0 5
dflt.mat 5 4 4 3 1 0 3 -1 -1 7
 2 0 0 5 3 0 0 5 0 0 0 5
END
)tpl");

        const std::vector<sed::level::SectorShape> shapes = sed::level::loadSectorShapesTpl(input);
        check(shapes.size() == 1, "loadSectorShapesTpl: count");
        check(!shapes.empty() && shapes[0].name == "Mini Tetra", "loadSectorShapesTpl: name");
        check(!shapes.empty() && shapes[0].uvCoordinateSpace == sed::UvCoordinateSpace::Normalized,
            "loadSectorShapesTpl: explicit normalized UV coordinate space");
        check(!shapes.empty() && shapes[0].sector.vertices.size() == 4, "loadSectorShapesTpl: vertices");
        check(!shapes.empty() && shapes[0].sector.surfaces.size() == 4, "loadSectorShapesTpl: surfaces");

        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;
        const std::int32_t added = shapes.empty()
            ? -1
            : sed::level::levelCreateSectorFromShape(level, shapes[0].sector, sed::TVector{ 0.0, 0.0, 0.0 }, 2.0);
        check(added == 0, "createSectorFromShape: index");
        check(level.sectors.size() == 1, "createSectorFromShape: level.sectors.size");
        check(level.sectors[0].vertices.size() == 4, "createSectorFromShape: vertices");
        check(level.sectors[0].surfaces.size() == 4, "createSectorFromShape: surfaces");
    }

    {
        std::istringstream input(R"tpl(
SEC Normalized UV Shape
- - 0 0 0 0 0 0 0 0 UV normalized
VXS 3
0 0 0
1 0 0
0 1 1
SURFS 1
wide.mat 5 4 4 3 1 0 3 -1 -1 7 0.015625 0.046875
0 -0.25 2.5 0.25 1 1.75 -1.5 0.5 2 3.25 4.5 0.75 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9
END
)tpl");

        const std::vector<sed::level::SectorShape> shapes =
            sed::level::loadSectorShapesTpl(input);
        check(shapes.size() == 1
                && shapes[0].uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized,
            "loadSectorShapesTpl.normalizedUv: retains the explicit coordinate-space contract");

        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        const std::int32_t added = shapes.empty()
            ? -1
            : sed::level::levelCreateSectorFromShape(
                level,
                shapes[0].sector,
                sed::TVector::zero,
                2.0);
        const sed::Surface* placed = added == 0
                && !level.sectors.empty()
                && !level.sectors[0].surfaces.empty()
            ? &level.sectors[0].surfaces[0]
            : nullptr;
        check(placed
                && close(placed->uScale, 0.015625f)
                && close(placed->vScale, 0.046875f)
                && placed->texVertices.size() == 3
                && close(placed->texVertices[0].u, -0.25f)
                && close(placed->texVertices[0].v, 2.5f)
                && close(placed->texVertices[1].u, 1.75f)
                && close(placed->texVertices[1].v, -1.5f)
                && close(placed->texVertices[2].u, 3.25f)
                && close(placed->texVertices[2].v, 4.5f),
            "createSectorFromShape.normalizedUv: preserves signed multi-tile UVs and anisotropic scales exactly");
    }

    {
        std::istringstream input(R"tpl(
# Leading comments and blank lines should be ignored.
SEC # unnamed shape uses generated name
- - 0.75 2a 8 1 0.2 0.3 0.4 5
VXS 3
0 0 0
1 0 0
0 1 0
SURFS 1
- 5 4 4 3 1 2 3 -1 -1 7 2.5 0.5
0 11 12 0.25 1 21 22 0.5 2 31 32 0.75 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9
END
)tpl");

        const std::vector<sed::level::SectorShape> shapes = sed::level::loadSectorShapesTpl(input);
        check(shapes.size() == 1, "loadSectorShapesTpl.generatedName: count");
        check(!shapes.empty() && shapes[0].uvCoordinateSpace == sed::UvCoordinateSpace::TexturePixels,
            "loadSectorShapesTpl.generatedName: legacy shapes default to texture-pixel UVs");
        check(!shapes.empty() && shapes[0].name == "Shape 1", "loadSectorShapesTpl.generatedName: name");
        check(!shapes.empty() && shapes[0].sector.colormap.empty(), "loadSectorShapesTpl.generatedName: colormap");
        check(!shapes.empty() && shapes[0].sector.sound.empty(), "loadSectorShapesTpl.generatedName: sound");
        check(!shapes.empty() && close(shapes[0].sector.soundVolume, 0.75), "loadSectorShapesTpl.generatedName: sound volume");
        check(!shapes.empty() && shapes[0].sector.flags == 0x2a, "loadSectorShapesTpl.generatedName: flags");
        check(!shapes.empty() && shapes[0].sector.layer == 5, "loadSectorShapesTpl.generatedName: layer");
        check(!shapes.empty() && close(shapes[0].sector.tint.r, 0.2f), "loadSectorShapesTpl.generatedName: tint r");
        check(!shapes.empty() && close(shapes[0].sector.tint.g, 0.3f), "loadSectorShapesTpl.generatedName: tint g");
        check(!shapes.empty() && close(shapes[0].sector.tint.b, 0.4f), "loadSectorShapesTpl.generatedName: tint b");

        if (!shapes.empty() && !shapes[0].sector.surfaces.empty())
        {
            const sed::Surface& surface = shapes[0].sector.surfaces[0];
            check(surface.material.empty(), "loadSectorShapesTpl.generatedName: surface material");
            check(surface.surfaceFlags == 5, "loadSectorShapesTpl.generatedName: surface flags");
            check(surface.faceFlags == 4, "loadSectorShapesTpl.generatedName: face flags");
            check(close(surface.uScale, 2.5f), "loadSectorShapesTpl.generatedName: u scale");
            check(close(surface.vScale, 0.5f), "loadSectorShapesTpl.generatedName: v scale");
            check(!surface.adjoin.has_value(), "loadSectorShapesTpl.generatedName: no adjoin");
            check(surface.texVertices.size() == 3, "loadSectorShapesTpl.generatedName: tex vertex count");
            if (surface.texVertices.size() == 3)
            {
                check(close(surface.texVertices[0].u, 11.0f), "loadSectorShapesTpl.generatedName: tv0.u");
                check(close(surface.texVertices[0].v, 12.0f), "loadSectorShapesTpl.generatedName: tv0.v");
                check(close(surface.texVertices[0].color.a, 0.25f), "loadSectorShapesTpl.generatedName: tv0.a");
                check(close(surface.texVertices[0].color.r, 0.1f), "loadSectorShapesTpl.generatedName: tv0.r");
                check(close(surface.texVertices[1].color.g, 0.5f), "loadSectorShapesTpl.generatedName: tv1.g");
                check(close(surface.texVertices[2].color.b, 0.9f), "loadSectorShapesTpl.generatedName: tv2.b");
            }
        }

        sed::Level compatibilityLevel{};
        compatibilityLevel.clear();
        compatibilityLevel.kind = sed::ProjectType::JKDF2;
        compatibilityLevel.uvCoordinateSpace =
            sed::UvCoordinateSpace::TexturePixels;
        const std::int32_t placedIndex = shapes.empty()
            ? -1
            : sed::level::levelCreateSectorFromShape(
                compatibilityLevel,
                shapes[0].sector,
                sed::TVector{ 3.0, -5.0, 7.0 },
                2.0);
        const sed::Surface* placed = placedIndex == 0
                && compatibilityLevel.sectors.size() == 1
                && compatibilityLevel.sectors[0].surfaces.size() == 1
            ? &compatibilityLevel.sectors[0].surfaces[0]
            : nullptr;
        check(placed
                && compatibilityLevel.uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels
                && placed->uScale == 2.5f
                && placed->vScale == 0.5f
                && placed->texVertices.size() == 3
                && placed->texVertices[0].u == 11.0f
                && placed->texVertices[0].v == 12.0f
                && placed->texVertices[0].color.a == 0.25f
                && placed->texVertices[1].u == 21.0f
                && placed->texVertices[1].v == 22.0f
                && placed->texVertices[2].u == 31.0f
                && placed->texVertices[2].v == 32.0f,
            "createSectorFromShape.pixelCompatibility: legacy shape placement keeps its explicit pixel-space tag, corner values, colors, and scales exact");
    }

    {
        std::istringstream input(R"tpl(
SEC Broken
- - 0 0 0 0 0 0 0 0
VXS 0
SURFS 0
END
)tpl");

        const std::vector<sed::level::SectorShape> shapes = sed::level::loadSectorShapesTpl(input);
        check(shapes.empty(), "loadSectorShapesTpl.invalid: empty geometry skipped");
    }

    {
        std::istringstream input(R"tpl(
SEC Incomplete UV Topology
- - 0 0 0 0 0 0 0 0 UV normalized
VXS 4
0 0 0
1 0 0
1 1 0
0 1 0
SURFS 1
dflt.mat 5 4 4 3 1 0 4 -1 -1 7 1 1
0 0 0 1 1 1 0 1 2 1 1 1
END
)tpl");

        const std::vector<sed::level::SectorShape> shapes =
            sed::level::loadSectorShapesTpl(input);
        check(shapes.empty(),
            "loadSectorShapesTpl.invalidUvTopology rejects a surface whose declared corner count exceeds its complete UV tuples");
    }

    {
        std::istringstream input(R"tpl(
SEC Unknown UV Space
- - 0 0 0 0 0 0 0 0 UV screen
VXS 3
0 0 0
1 0 0
0 1 1
SURFS 1
dflt.mat 5 4 4 3 1 0 3 -1 -1 7 1 1
0 0 0 1 1 1 0 1 2 0 1 1
END
)tpl");

        const std::vector<sed::level::SectorShape> shapes =
            sed::level::loadSectorShapesTpl(input);
        check(shapes.empty(),
            "loadSectorShapesTpl.invalidUvSpace: rejects an explicit unknown coordinate-space marker");
    }

    {
        for (const std::string_view uvSpace : {
                 std::string_view("pixels"),
                 std::string_view("normalized") })
        {
            for (const bool overflowScale : { false, true })
            {
                const std::string surfaceScale = overflowScale
                    ? "3.5e38"
                    : "1";
                const std::string coordinate = overflowScale
                    ? "0"
                    : "-3.5e38";
                std::istringstream input(
                    "SEC Overflow UV Shape\n"
                    "- - 0 0 0 0 0 0 0 0 UV "
                    + std::string(uvSpace)
                    + "\n"
                    "VXS 3\n"
                    "0 0 0\n"
                    "1 0 0\n"
                    "0 1 1\n"
                    "SURFS 1\n"
                    "dflt.mat 5 4 4 3 1 0 3 -1 -1 7 "
                    + surfaceScale
                    + " 1\n"
                    "0 "
                    + coordinate
                    + " 0 1 1 1 0 1 2 0 1 1\n"
                    "END\n");

                const std::vector<sed::level::SectorShape> shapes =
                    sed::level::loadSectorShapesTpl(input);
                const std::string message =
                    "loadSectorShapesTpl rejects finite-double-to-float "
                    + std::string(overflowScale ? "scale" : "coordinate")
                    + " overflow for " + std::string(uvSpace)
                    + " UVs";
                check(shapes.empty(), message.c_str());
            }
        }
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;

        check(!sed::level::levelExtrudeSurface(level, sed::SurfaceRef{ 0, 0 }, 1.0), "extrude.invalid: empty level");

        sed::Sector& sector = level.newSector();
        sector.addVertex(0.0, 0.0, 0.0);
        sector.addVertex(1.0, 0.0, 0.0);
        sector.addVertex(0.0, 1.0, 0.0);
        sector.addSurface(makeSurface({ 0, 1 }));
        check(!sed::level::levelExtrudeSurface(level, sed::SurfaceRef{ 0, 0 }, 1.0), "extrude.invalid: too few vertices");

        sector.surfaces.clear();
        sector.addSurface(makeSurface({ 0, 1, 9 }));
        check(!sed::level::levelExtrudeSurface(level, sed::SurfaceRef{ 0, 0 }, 1.0), "extrude.invalid: bad vertex ref");

        sector.surfaces.clear();
        sed::Surface adjoined = makeSurface({ 0, 1, 2 });
        adjoined.adjoin = sed::SurfaceRef{ 1, 0 };
        sector.addSurface(std::move(adjoined));
        check(!sed::level::levelBuildSurfaceExtrudePreview(level, sed::SurfaceRef{ 0, 0 }, 1.0, 0.5),
            "extrude.invalid: preview rejects adjoined");
        check(!sed::level::levelBuildSurfaceExtrudeSectorPreview(level, sed::SurfaceRef{ 0, 0 }, 1.0, 0.5),
            "extrude.invalid: sector preview rejects adjoined");
        check(!sed::level::levelExtrudeSurface(level, sed::SurfaceRef{ 0, 0 }, 1.0), "extrude.invalid: adjoined");
        check(level.sectors.size() == 1, "extrude.invalid: no sector appended");

        sector.surfaces.clear();
        sector.addSurface(makeSurface({ 0, 1, 2 }));
        check(!sed::level::levelBuildSurfaceExtrudePreview(level, sed::SurfaceRef{ 0, 0 }, std::numeric_limits<double>::infinity(), 0.0),
            "extrude.invalid: preview rejects infinite length");
        check(!sed::level::levelBuildSurfaceExtrudeSectorPreview(level, sed::SurfaceRef{ 0, 0 }, 1.0, std::numeric_limits<double>::quiet_NaN()),
            "extrude.invalid: sector preview rejects nan expand");
        check(!sed::level::levelExtrudeSurface(level, sed::SurfaceRef{ 0, 0 }, 1.0, std::numeric_limits<double>::quiet_NaN()),
            "extrude.invalid: rejects nan expand");
        check(level.sectors.size() == 1, "extrude.invalid: no sector appended for nan expand");
    }

    {
        std::ifstream input("data/shapes.tpl");
        if (input)
        {
            const std::vector<sed::level::SectorShape> shapes = sed::level::loadSectorShapesTpl(input);
            check(shapes.size() >= 3, "loadSectorShapesTpl.file: count");
        }
    }
}

} // namespace sed::tests
