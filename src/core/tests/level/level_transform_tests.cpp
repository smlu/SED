#include "test_common.hpp"

#include "sed/assets/sed.hpp"
#include "sed/level/build.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level/transform.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace sed::tests
{

namespace
{

sed::Surface makeTransformTestSurface()
{
    sed::Surface surface{};
    surface.vertices = { 0, 1, 2 };
    surface.texVertices = {
        { 0.0f, 0.0f, sed::ColorWhite },
        { 1.0f, 0.0f, sed::ColorWhite },
        { 0.0f, 1.0f, sed::ColorWhite },
    };
    return surface;
}

bool sameFloatBits(const float lhs, const float rhs) noexcept
{
    return std::bit_cast<std::uint32_t>(lhs)
        == std::bit_cast<std::uint32_t>(rhs);
}

bool sameTexVertex(const sed::TexVertex& lhs, const sed::TexVertex& rhs) noexcept
{
    return sameFloatBits(lhs.u, rhs.u)
        && sameFloatBits(lhs.v, rhs.v)
        && sameFloatBits(lhs.color.r, rhs.color.r)
        && sameFloatBits(lhs.color.g, rhs.color.g)
        && sameFloatBits(lhs.color.b, rhs.color.b)
        && sameFloatBits(lhs.color.a, rhs.color.a);
}

bool sameSectorTextureState(const sed::Sector& lhs, const sed::Sector& rhs) noexcept
{
    if ( lhs.surfaces.size() != rhs.surfaces.size() )
    {
        return false;
    }

    for ( std::size_t surfaceIndex = 0; surfaceIndex < lhs.surfaces.size(); ++surfaceIndex )
    {
        const sed::Surface& lhsSurface = lhs.surfaces[surfaceIndex];
        const sed::Surface& rhsSurface = rhs.surfaces[surfaceIndex];
        if ( !sameFloatBits(lhsSurface.uScale, rhsSurface.uScale)
            || !sameFloatBits(lhsSurface.vScale, rhsSurface.vScale) )
        {
            return false;
        }

        const std::vector<sed::TexVertex>& lhsUvs = lhsSurface.texVertices;
        const std::vector<sed::TexVertex>& rhsUvs = rhsSurface.texVertices;
        if ( lhsUvs.size() != rhsUvs.size() )
        {
            return false;
        }

        for ( std::size_t vertexIndex = 0; vertexIndex < lhsUvs.size(); ++vertexIndex )
        {
            if ( !sameTexVertex(lhsUvs[vertexIndex], rhsUvs[vertexIndex]) )
            {
                return false;
            }
        }
    }
    return true;
}

bool sameSectorTextureVertices(const sed::Sector& lhs, const sed::Sector& rhs) noexcept
{
    if ( lhs.surfaces.size() != rhs.surfaces.size() )
    {
        return false;
    }

    for ( std::size_t surfaceIndex = 0; surfaceIndex < lhs.surfaces.size(); ++surfaceIndex )
    {
        const std::vector<sed::TexVertex>& lhsUvs = lhs.surfaces[surfaceIndex].texVertices;
        const std::vector<sed::TexVertex>& rhsUvs = rhs.surfaces[surfaceIndex].texVertices;
        if ( lhsUvs.size() != rhsUvs.size() )
        {
            return false;
        }

        for ( std::size_t vertexIndex = 0; vertexIndex < lhsUvs.size(); ++vertexIndex )
        {
            if ( !sameTexVertex(lhsUvs[vertexIndex], rhsUvs[vertexIndex]) )
            {
                return false;
            }
        }
    }
    return true;
}

bool sameSectorSurfaceFlags(const sed::Sector& lhs, const sed::Sector& rhs) noexcept
{
    if ( lhs.surfaces.size() != rhs.surfaces.size() )
    {
        return false;
    }

    for ( std::size_t surfaceIndex = 0; surfaceIndex < lhs.surfaces.size(); ++surfaceIndex )
    {
        if ( lhs.surfaces[surfaceIndex].surfaceFlags != rhs.surfaces[surfaceIndex].surfaceFlags )
        {
            return false;
        }
    }
    return true;
}

sed::Level makeAuthoredUvTransformLevel()
{
    sed::Level level{};
    level.kind = sed::ProjectType::JKDF2;
    level.clear();
    level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;

    const std::int32_t sectorIndex = sed::level::levelCreateSectorPrimitiveBox(
        level,
        sed::level::SectorPrimitive::Cube,
        sed::TVector{ -1.0, -1.5, -0.5 },
        sed::TVector{ 2.0, 3.0, 1.5 });
    if ( sectorIndex < 0 )
    {
        return level;
    }

    sed::Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    for ( std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex )
    {
        sed::Surface& surface = sector.surfaces[surfaceIndex];
        if ( surface.normal.dot(sed::TVector::up) > 0.8 )
        {
            surface.surfaceFlags &= ~sed::SF_Floor;
        }
        else
        {
            surface.surfaceFlags |= sed::SF_Floor;
        }

        for ( std::size_t vertexIndex = 0; vertexIndex < surface.texVertices.size(); ++vertexIndex )
        {
            sed::TexVertex& texVertex = surface.texVertices[vertexIndex];
            texVertex.u = static_cast<float>(11.125 + 37.25 * surfaceIndex + 12.75 * vertexIndex);
            texVertex.v = static_cast<float>(-8.5 - 19.625 * surfaceIndex + 7.375 * vertexIndex);
            texVertex.color = sed::ColorF{
                static_cast<float>(0.05 * (surfaceIndex + 1)),
                static_cast<float>(0.07 * (vertexIndex + 1)),
                static_cast<float>(0.03 * (surfaceIndex + vertexIndex + 1)),
                static_cast<float>(0.2 + 0.05 * vertexIndex)
            };
        }
    }

    return level;
}

sed::Level makeAuthoredUvAdjoinedTransformLevel()
{
    sed::Level level{};
    level.kind = sed::ProjectType::JKDF2;
    level.clear();
    level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;

    const std::int32_t leftSector = sed::level::levelCreateSectorPrimitiveBox(
        level,
        sed::level::SectorPrimitive::Cube,
        sed::TVector{ -2.0, -1.0, -1.0 },
        sed::TVector{ 0.0, 1.0, 1.0 });
    const std::int32_t rightSector = sed::level::levelCreateSectorPrimitiveBox(
        level,
        sed::level::SectorPrimitive::Cube,
        sed::TVector{ 0.0, -1.0, -1.0 },
        sed::TVector{ 2.0, 1.0, 1.0 });
    if ( leftSector < 0 || rightSector < 0 )
    {
        return level;
    }

    const auto faceOnSharedPlane = [&level](const std::int32_t sectorIndex)
    {
        const sed::Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        for ( std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex )
        {
            const sed::Surface& surface = sector.surfaces[surfaceIndex];
            const bool onPlane = std::all_of(
                surface.vertices.begin(),
                surface.vertices.end(),
                [&sector](const std::int32_t vertexIndex)
                {
                    return vertexIndex >= 0
                        && static_cast<std::size_t>(vertexIndex) < sector.vertices.size()
                        && close(sector.vertices[static_cast<std::size_t>(vertexIndex)].x, 0.0);
                });
            if ( onPlane && !surface.vertices.empty() )
            {
                return static_cast<std::int32_t>(surfaceIndex);
            }
        }
        return std::int32_t{ -1 };
    };

    const std::int32_t leftSurface = faceOnSharedPlane(leftSector);
    const std::int32_t rightSurface = faceOnSharedPlane(rightSector);
    if ( leftSurface < 0 || rightSurface < 0
        || !sed::level::levelSysAdjoinSurfaces(
            level,
            sed::SurfaceRef{ leftSector, leftSurface },
            sed::SurfaceRef{ rightSector, rightSurface },
            false) )
    {
        return level;
    }

    for ( std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex )
    {
        sed::Sector& sector = level.sectors[sectorIndex];
        for ( std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex )
        {
            sed::Surface& surface = sector.surfaces[surfaceIndex];
            for ( std::size_t corner = 0; corner < surface.texVertices.size(); ++corner )
            {
                sed::TexVertex& texVertex = surface.texVertices[corner];
                texVertex.u = static_cast<float>(17.25 + 101.5 * sectorIndex + 23.75 * surfaceIndex + 5.5 * corner);
                texVertex.v = static_cast<float>(-9.75 - 73.25 * sectorIndex - 11.5 * surfaceIndex + 3.25 * corner);
                texVertex.color = sed::ColorF{
                    static_cast<float>(0.11 + 0.17 * sectorIndex),
                    static_cast<float>(0.07 + 0.09 * surfaceIndex),
                    static_cast<float>(0.05 + 0.08 * corner),
                    static_cast<float>(0.31 + 0.07 * corner)
                };
            }
        }
    }
    return level;
}

std::optional<sed::SurfaceRef> findAdjoinedSurface(
    const sed::Level& level,
    const std::int32_t sectorIndex) noexcept
{
    if ( sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size() )
    {
        return std::nullopt;
    }

    const sed::Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    for ( std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex )
    {
        if ( sector.surfaces[surfaceIndex].adjoin )
        {
            return sed::SurfaceRef{
                sectorIndex,
                static_cast<std::int32_t>(surfaceIndex)
            };
        }
    }
    return std::nullopt;
}

template <typename Transform>
void checkTransformPreservesAuthoredUvs(
    const sed::Level& baseline,
    const Transform& transform,
    const std::string& label,
    const double expectedScaleDivisor = 1.0)
{
    sed::Level transformed = baseline;
    transform(transformed);

    const std::string sectorCountLabel = label + ": sector count";
    check(
        transformed.sectors.size() == baseline.sectors.size(),
        sectorCountLabel.c_str());
    const std::string coordinateSpaceLabel =
        label + ": coordinate-space tag";
    check(
        transformed.uvCoordinateSpace == baseline.uvCoordinateSpace,
        coordinateSpaceLabel.c_str());
    if ( transformed.sectors.size() != baseline.sectors.size() )
    {
        return;
    }

    for ( std::size_t sectorIndex = 0; sectorIndex < baseline.sectors.size(); ++sectorIndex )
    {
        const sed::Sector& beforeSector = baseline.sectors[sectorIndex];
        const sed::Sector& afterSector = transformed.sectors[sectorIndex];
        const std::string textureVerticesLabel = label + ": exact authored texture vertices";
        check(
            sameSectorTextureVertices(afterSector, beforeSector),
            textureVerticesLabel.c_str());

        bool scaleMetadataMatches =
            afterSector.surfaces.size() == beforeSector.surfaces.size();
        if ( scaleMetadataMatches )
        {
            for ( std::size_t surfaceIndex = 0;
                surfaceIndex < beforeSector.surfaces.size();
                ++surfaceIndex )
            {
                const sed::Surface& before = beforeSector.surfaces[surfaceIndex];
                const sed::Surface& after = afterSector.surfaces[surfaceIndex];
                scaleMetadataMatches = scaleMetadataMatches
                    && after.uScale
                        == static_cast<float>(
                            static_cast<double>(before.uScale)
                            / expectedScaleDivisor)
                    && after.vScale
                        == static_cast<float>(
                            static_cast<double>(before.vScale)
                            / expectedScaleDivisor);
            }
        }
        const std::string scaleMetadataLabel =
            label + ": exact per-axis texture-scale contract";
        check(scaleMetadataMatches, scaleMetadataLabel.c_str());

        const std::string surfaceFlagsLabel = label + ": authored surface flags";
        check(
            sameSectorSurfaceFlags(afterSector, beforeSector),
            surfaceFlagsLabel.c_str());
    }
}

sed::TplValue makeFrameValue(
    const double x,
    const double y,
    const double z,
    const double pch = 0.0,
    const double yaw = 0.0,
    const double rol = 0.0,
    const char* name = "frame")
{
    sed::TplValue value{};
    value.name = name;
    value.setFrame(x, y, z, pch, yaw, rol);
    return value;
}

void checkFrame(
    const sed::TplValue& value,
    const double expectedX,
    const double expectedY,
    const double expectedZ,
    const double expectedPch,
    const double expectedYaw,
    const double expectedRol,
    const char* label)
{
    double x{};
    double y{};
    double z{};
    double pch{};
    double yaw{};
    double rol{};
    const bool ok = value.tryGetFrame(x, y, z, pch, yaw, rol);
    check(ok, label);
    check(close(x, expectedX, kRotationEpsilon), label);
    check(close(y, expectedY, kRotationEpsilon), label);
    check(close(z, expectedZ, kRotationEpsilon), label);
    check(close(pch, expectedPch, kRotationEpsilon), label);
    check(close(yaw, expectedYaw, kRotationEpsilon), label);
    check(close(rol, expectedRol, kRotationEpsilon), label);
}

} // namespace

void runLevelTransformTests()
{
    {
        const sed::Level newSectorLevel = makeAuthoredUvTransformLevel();
        check(newSectorLevel.sectors.size() == 1, "transform UV preservation: new sector created");

        const auto sectorTranslate = [](sed::Level& level)
        {
            const std::array<std::int32_t, 1> sectors{ 0 };
            sed::level::levelTranslateSectors(level, sectors, 3.25, -2.5, 0.75);
        };
        checkTransformPreservesAuthoredUvs(
            newSectorLevel,
            sectorTranslate,
            "transform UV preservation: new sector first move");

        const std::vector<std::byte> sedBytes = sed::assets::saveToSedBytes(newSectorLevel);
        const sed::Level restoredLevel = sed::assets::loadFromSedBytes(sedBytes);
        check(
            restoredLevel.sectors.size() == 1
                && restoredLevel.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && sameSectorTextureState(restoredLevel.sectors[0], newSectorLevel.sectors[0]),
            "transform UV preservation: native SED round trip retains normalized authored texture vertices and tag");

        checkTransformPreservesAuthoredUvs(restoredLevel, sectorTranslate, "transform UV preservation: sector move");

        const auto sectorRotate = [](sed::Level& level)
        {
            const std::array<std::int32_t, 1> sectors{ 0 };
            sed::level::levelRotateSectors(level, sectors, sed::TVector{ 1.0, 2.0, 3.0 }, 0.5, -0.25, 0.75, 37.0);
        };
        checkTransformPreservesAuthoredUvs(restoredLevel, sectorRotate, "transform UV preservation: sector rotate");

        const auto sectorScale = [](sed::Level& level)
        {
            sed::ScaleData scale{};
            scale.cx = 0.5;
            scale.cy = -0.25;
            scale.cz = 0.75;
            scale.sfactor = 1.75;
            scale.how = sed::ScaleHow::xyz;

            const std::array<std::int32_t, 1> sectors{ 0 };
            sed::level::levelScaleSectors(level, sectors, scale, false);
        };
        checkTransformPreservesAuthoredUvs(restoredLevel, sectorScale, "transform UV preservation: sector scale");

        constexpr std::array scaleModes{
            sed::ScaleHow::x,
            sed::ScaleHow::y,
            sed::ScaleHow::z,
            sed::ScaleHow::vec
        };
        for ( const sed::ScaleHow mode : scaleModes )
        {
            const auto sectorScaleMode = [mode](sed::Level& level)
            {
                sed::ScaleData scale{};
                scale.cx = 0.5;
                scale.cy = -0.25;
                scale.cz = 0.75;
                scale.sfactor = 1.375;
                scale.how = mode;
                scale.vec = sed::TVector{ 1.0, 2.0, -3.0 }.Normalized();

                const std::array<std::int32_t, 1> sectors{ 0 };
                sed::level::levelScaleSectors(
                    level, sectors, scale, false);
            };
            checkTransformPreservesAuthoredUvs(
                restoredLevel,
                sectorScaleMode,
                "transform UV preservation: every sector scale axis/vector mode");
        }

        const auto sectorScaleWithTextureDensity = [](sed::Level& level)
        {
            sed::ScaleData scale{};
            scale.cx = 0.5;
            scale.cy = -0.25;
            scale.cz = 0.75;
            scale.sfactor = 1.75;
            scale.how = sed::ScaleHow::xyz;

            const std::array<std::int32_t, 1> sectors{ 0 };
            sed::level::levelScaleSectors(level, sectors, scale, true);
        };
        checkTransformPreservesAuthoredUvs(
            restoredLevel,
            sectorScaleWithTextureDensity,
            "transform UV preservation: sector scale with texture density",
            1.75);

        const auto zeroFactorTextureDensity = [](sed::Level& level)
        {
            sed::ScaleData scale{};
            scale.sfactor = 0.0;
            scale.how = sed::ScaleHow::x;

            const std::array<std::int32_t, 1> sectors{ 0 };
            sed::level::levelScaleSectors(level, sectors, scale, true);
        };
        checkTransformPreservesAuthoredUvs(
            restoredLevel,
            zeroFactorTextureDensity,
            "transform UV preservation: zero scale never divides texture metadata");

        sed::Level overflowDensityLevel = restoredLevel;
        sed::Surface& overflowDensitySurface =
            overflowDensityLevel.sectors[0].surfaces[0];
        overflowDensitySurface.uScale =
            std::numeric_limits<float>::max();
        overflowDensitySurface.vScale = 1.0f;
        sed::ScaleData overflowDensityScale{};
        overflowDensityScale.sfactor = 0.5;
        overflowDensityScale.how = sed::ScaleHow::xyz;
        const std::array<sed::SurfaceRef, 1> overflowDensitySurfaces{
            sed::SurfaceRef{ 0, 0 }
        };
        sed::level::levelScaleSurfaces(
            overflowDensityLevel,
            overflowDensitySurfaces,
            overflowDensityScale,
            true);
        check(sameFloatBits(
                  overflowDensitySurface.uScale,
                  std::numeric_limits<float>::max())
                && sameFloatBits(overflowDensitySurface.vScale, 1.0f),
            "transform UV preservation: unrepresentable texture-density scaling preserves both scale values atomically");

        const auto surfaceTranslate = [](sed::Level& level)
        {
            const std::array<sed::SurfaceRef, 1> surfaces{ sed::SurfaceRef{ 0, 0 } };
            sed::level::levelTranslateSurfaces(level, surfaces, 0.5, 0.25, -0.75);
        };
        checkTransformPreservesAuthoredUvs(restoredLevel, surfaceTranslate, "transform UV preservation: surface move");

        const auto surfaceRotate = [](sed::Level& level)
        {
            const std::array<sed::SurfaceRef, 1> surfaces{ sed::SurfaceRef{ 0, 0 } };
            sed::level::levelRotateSurfaces(level, surfaces, sed::TVector::up, 0.0, 0.0, 0.0, 23.0);
        };
        checkTransformPreservesAuthoredUvs(restoredLevel, surfaceRotate, "transform UV preservation: surface rotate");

        const auto surfaceScale = [](sed::Level& level)
        {
            sed::ScaleData scale{};
            scale.cx = 0.0;
            scale.cy = 0.0;
            scale.cz = 0.0;
            scale.sfactor = 1.25;
            scale.how = sed::ScaleHow::xyz;

            const std::array<sed::SurfaceRef, 1> surfaces{ sed::SurfaceRef{ 0, 0 } };
            sed::level::levelScaleSurfaces(level, surfaces, scale, false);
        };
        checkTransformPreservesAuthoredUvs(restoredLevel, surfaceScale, "transform UV preservation: surface scale");

        sed::Level surfaceDensityScaledLevel = restoredLevel;
        sed::ScaleData surfaceDensityScale{};
        surfaceDensityScale.sfactor = 1.25;
        surfaceDensityScale.how = sed::ScaleHow::xyz;
        const sed::SurfaceRef densityScaledSurface{ 0, 0 };
        const std::array<sed::SurfaceRef, 1> densityScaledSurfaces{
            densityScaledSurface
        };
        const std::optional<sed::SurfaceRef> densityScaledPortalMate =
            restoredLevel.sectors[static_cast<std::size_t>(densityScaledSurface.sector)]
                .surfaces[static_cast<std::size_t>(densityScaledSurface.surface)]
                .adjoin;
        sed::level::levelScaleSurfaces(
            surfaceDensityScaledLevel,
            densityScaledSurfaces,
            surfaceDensityScale,
            true);

        bool exactSurfaceDensityScale =
            surfaceDensityScaledLevel.uvCoordinateSpace
                == restoredLevel.uvCoordinateSpace
            && surfaceDensityScaledLevel.sectors.size()
                == restoredLevel.sectors.size();
        if ( exactSurfaceDensityScale )
        {
            for ( std::size_t sectorIndex = 0;
                sectorIndex < restoredLevel.sectors.size();
                ++sectorIndex )
            {
                exactSurfaceDensityScale = exactSurfaceDensityScale
                    && sameSectorTextureVertices(
                        surfaceDensityScaledLevel.sectors[sectorIndex],
                        restoredLevel.sectors[sectorIndex])
                    && surfaceDensityScaledLevel.sectors[sectorIndex]
                            .surfaces.size()
                        == restoredLevel.sectors[sectorIndex]
                            .surfaces.size();
                for ( std::size_t surfaceIndex = 0;
                    exactSurfaceDensityScale
                        && surfaceIndex
                            < restoredLevel.sectors[sectorIndex]
                                .surfaces.size();
                    ++surfaceIndex )
                {
                    const sed::Surface& before =
                        restoredLevel.sectors[sectorIndex]
                            .surfaces[surfaceIndex];
                    const sed::Surface& after =
                        surfaceDensityScaledLevel.sectors[sectorIndex]
                            .surfaces[surfaceIndex];
                    const bool isSelected =
                        densityScaledSurface.sector
                                == static_cast<std::int32_t>(sectorIndex)
                            && densityScaledSurface.surface
                                == static_cast<std::int32_t>(surfaceIndex);
                    const bool isPortalMate = densityScaledPortalMate
                        && densityScaledPortalMate->sector
                            == static_cast<std::int32_t>(sectorIndex)
                        && densityScaledPortalMate->surface
                            == static_cast<std::int32_t>(surfaceIndex);
                    const double divisor = isSelected || isPortalMate
                        ? 1.25
                        : 1.0;
                    exactSurfaceDensityScale = exactSurfaceDensityScale
                        && after.uScale
                            == static_cast<float>(
                                static_cast<double>(before.uScale)
                                / divisor)
                        && after.vScale
                            == static_cast<float>(
                                static_cast<double>(before.vScale)
                                / divisor);
                }
            }
        }
        check(exactSurfaceDensityScale,
            "transform UV preservation: surface texture density changes only the selected face and portal mate once");

        const auto edgeTranslate = [](sed::Level& level)
        {
            const std::array<sed::level::EdgeRef, 1> edges{ sed::level::EdgeRef{ 0, 0, 0 } };
            sed::level::levelTranslateEdges(level, edges, 0.125, -0.25, 0.5);
        };
        checkTransformPreservesAuthoredUvs(restoredLevel, edgeTranslate, "transform UV preservation: edge move");

        const auto edgeRotate = [](sed::Level& level)
        {
            const std::array<sed::level::EdgeRef, 1> edges{ sed::level::EdgeRef{ 0, 0, 0 } };
            sed::level::levelRotateEdges(level, edges, sed::TVector::up, 0.0, 0.0, 0.0, 11.0);
        };
        checkTransformPreservesAuthoredUvs(restoredLevel, edgeRotate, "transform UV preservation: edge rotate");

        const auto vertexTranslate = [](sed::Level& level)
        {
            const std::array<sed::VertexRef, 1> vertices{ sed::VertexRef{ 0, 0 } };
            sed::level::levelTranslateVertices(level, vertices, -0.125, 0.25, -0.5);
        };
        checkTransformPreservesAuthoredUvs(restoredLevel, vertexTranslate, "transform UV preservation: vertex move");

        const auto flattenSurface = [](sed::Level& level)
        {
            sed::Surface& surface = level.sectors[0].surfaces[0];
            const std::int32_t displacedVertexIndex = surface.vertices.back();
            sed::Vertex& displacedVertex =
                level.sectors[0].vertices[static_cast<std::size_t>(displacedVertexIndex)];
            displacedVertex.x += surface.normal.x * 0.25;
            displacedVertex.y += surface.normal.y * 0.25;
            displacedVertex.z += surface.normal.z * 0.25;

            check(
                sed::level::levelFlattenSurface(level, sed::SurfaceRef{ 0, 0 }),
                "transform UV preservation: flatten succeeds");
        };
        checkTransformPreservesAuthoredUvs(
            restoredLevel,
            flattenSurface,
            "transform UV preservation: native SED flatten");
    }

    {
        const sed::Level adjoinedLevel = makeAuthoredUvAdjoinedTransformLevel();
        check(
            adjoinedLevel.sectors.size() == 2
                && findAdjoinedSurface(adjoinedLevel, 0).has_value()
                && findAdjoinedSurface(adjoinedLevel, 1).has_value(),
            "transform UV preservation: authored adjoined sectors created");

        const std::vector<std::byte> sedBytes = sed::assets::saveToSedBytes(adjoinedLevel);
        const sed::Level restoredLevel = sed::assets::loadFromSedBytes(sedBytes);
        check(
            restoredLevel.sectors.size() == 2
                && sameSectorTextureState(restoredLevel.sectors[0], adjoinedLevel.sectors[0])
                && sameSectorTextureState(restoredLevel.sectors[1], adjoinedLevel.sectors[1]),
            "transform UV preservation: native SED adjoined round trip retains independent corner frames");

        const auto sectorTranslate = [](sed::Level& level)
        {
            const std::array<std::int32_t, 1> sectors{ 0 };
            sed::level::levelTranslateSectors(level, sectors, 0.375, -0.625, 1.125);
        };
        checkTransformPreservesAuthoredUvs(
            restoredLevel,
            sectorTranslate,
            "transform UV preservation: native SED adjoined sector move");

        const auto sectorRotate = [](sed::Level& level)
        {
            const std::array<std::int32_t, 1> sectors{ 0 };
            sed::level::levelRotateSectors(
                level,
                sectors,
                sed::TVector{ 0.25, -0.5, 0.75 },
                0.0,
                1.0,
                0.0,
                31.0);
        };
        checkTransformPreservesAuthoredUvs(
            restoredLevel,
            sectorRotate,
            "transform UV preservation: native SED adjoined sector rotate");

        const auto sectorScale = [](sed::Level& level)
        {
            sed::ScaleData scale{};
            scale.cx = 0.0;
            scale.cy = 0.0;
            scale.cz = 0.0;
            scale.sfactor = 1.375;
            scale.how = sed::ScaleHow::xyz;

            const std::array<std::int32_t, 1> sectors{ 0 };
            sed::level::levelScaleSectors(level, sectors, scale, false);
        };
        checkTransformPreservesAuthoredUvs(
            restoredLevel,
            sectorScale,
            "transform UV preservation: native SED adjoined sector scale");

        const auto surfaceTranslate = [](sed::Level& level)
        {
            const std::optional<sed::SurfaceRef> surface = findAdjoinedSurface(level, 0);
            check(surface.has_value(), "transform UV preservation: adjoined surface move target");
            if ( !surface )
            {
                return;
            }

            const std::array<sed::SurfaceRef, 1> surfaces{ *surface };
            sed::level::levelTranslateSurfaces(level, surfaces, 0.125, 0.25, -0.375);
        };
        checkTransformPreservesAuthoredUvs(
            restoredLevel,
            surfaceTranslate,
            "transform UV preservation: native SED adjoined surface move");

        const auto surfaceRotate = [](sed::Level& level)
        {
            const std::optional<sed::SurfaceRef> surface = findAdjoinedSurface(level, 0);
            check(surface.has_value(), "transform UV preservation: adjoined surface rotate target");
            if ( !surface )
            {
                return;
            }

            const std::array<sed::SurfaceRef, 1> surfaces{ *surface };
            sed::level::levelRotateSurfaces(
                level,
                surfaces,
                sed::TVector::up,
                0.0,
                0.0,
                0.0,
                19.0);
        };
        checkTransformPreservesAuthoredUvs(
            restoredLevel,
            surfaceRotate,
            "transform UV preservation: native SED adjoined surface rotate");

        const auto surfaceScale = [](sed::Level& level)
        {
            const std::optional<sed::SurfaceRef> surface = findAdjoinedSurface(level, 0);
            check(surface.has_value(), "transform UV preservation: adjoined surface scale target");
            if ( !surface )
            {
                return;
            }

            sed::ScaleData scale{};
            scale.cx = 0.0;
            scale.cy = 0.0;
            scale.cz = 0.0;
            scale.sfactor = 0.875;
            scale.how = sed::ScaleHow::xyz;

            const std::array<sed::SurfaceRef, 1> surfaces{ *surface };
            sed::level::levelScaleSurfaces(level, surfaces, scale, false);
        };
        checkTransformPreservesAuthoredUvs(
            restoredLevel,
            surfaceScale,
            "transform UV preservation: native SED adjoined surface scale");

        const auto selectedAdjoinedSectorScaleWithTextureDensity = [](sed::Level& level)
        {
            sed::ScaleData scale{};
            scale.sfactor = 2.0;
            scale.how = sed::ScaleHow::xyz;

            const std::array<std::int32_t, 2> sectors{ 0, 1 };
            sed::level::levelScaleSectors(level, sectors, scale, true);
        };
        checkTransformPreservesAuthoredUvs(
            restoredLevel,
            selectedAdjoinedSectorScaleWithTextureDensity,
            "transform UV preservation: selected adjoined sectors scale texture density once",
            2.0);

        sed::Level surfaceScaledLevel = restoredLevel;
        const std::optional<sed::SurfaceRef> first =
            findAdjoinedSurface(surfaceScaledLevel, 0);
        const std::optional<sed::SurfaceRef> second =
            findAdjoinedSurface(surfaceScaledLevel, 1);
        check(first.has_value() && second.has_value(),
            "transform UV preservation: selected adjoined surface scale targets");
        if ( first && second )
        {
            sed::ScaleData scale{};
            scale.sfactor = 2.0;
            scale.how = sed::ScaleHow::xyz;

            const std::array<sed::SurfaceRef, 2> surfaces{
                *first,
                *second
            };
            sed::level::levelScaleSurfaces(
                surfaceScaledLevel, surfaces, scale, true);

            bool exactSelectedSurfaceScale =
                surfaceScaledLevel.uvCoordinateSpace
                    == restoredLevel.uvCoordinateSpace;
            for ( std::size_t sectorIndex = 0;
                sectorIndex < restoredLevel.sectors.size();
                ++sectorIndex )
            {
                exactSelectedSurfaceScale = exactSelectedSurfaceScale
                    && sameSectorTextureVertices(
                        surfaceScaledLevel.sectors[sectorIndex],
                        restoredLevel.sectors[sectorIndex]);
                for ( std::size_t surfaceIndex = 0;
                    surfaceIndex
                        < restoredLevel.sectors[sectorIndex].surfaces.size();
                    ++surfaceIndex )
                {
                    const sed::Surface& before =
                        restoredLevel.sectors[sectorIndex]
                            .surfaces[surfaceIndex];
                    const sed::Surface& after =
                        surfaceScaledLevel.sectors[sectorIndex]
                            .surfaces[surfaceIndex];
                    const bool selected =
                        (first->sector == static_cast<std::int32_t>(sectorIndex)
                            && first->surface
                                == static_cast<std::int32_t>(surfaceIndex))
                        || (second->sector
                                == static_cast<std::int32_t>(sectorIndex)
                            && second->surface
                                == static_cast<std::int32_t>(surfaceIndex));
                    const double divisor = selected ? 2.0 : 1.0;
                    exactSelectedSurfaceScale = exactSelectedSurfaceScale
                        && after.uScale
                            == static_cast<float>(
                                static_cast<double>(before.uScale) / divisor)
                        && after.vScale
                            == static_cast<float>(
                                static_cast<double>(before.vScale) / divisor);
                }
            }
            check(exactSelectedSurfaceScale,
                "transform UV preservation: selected adjoined surfaces scale texture density once without touching other faces");
        }
    }

    {
        sed::Level pixelBaseline = makeAuthoredUvTransformLevel();
        pixelBaseline.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;
        for ( sed::Surface& surface : pixelBaseline.sectors[0].surfaces )
        {
            surface.uScale = -37.25f;
            surface.vScale = 83.5f;
            for ( sed::TexVertex& texVertex : surface.texVertices )
            {
                texVertex.u *= 128.0f;
                texVertex.v *= 64.0f;
            }
        }

        const auto mixedCompatibilityTransforms = [](sed::Level& level)
        {
            const std::array<std::int32_t, 1> sectors{ 0 };
            const std::array<sed::SurfaceRef, 1> surfaces{
                sed::SurfaceRef{ 0, 0 }
            };
            const std::array<sed::level::EdgeRef, 1> edges{
                sed::level::EdgeRef{ 0, 1, 0 }
            };
            const std::array<sed::VertexRef, 1> vertices{
                sed::VertexRef{ 0, 6 }
            };

            sed::level::levelTranslateSectors(
                level, sectors, 0.25, -0.5, 0.75);
            sed::level::levelRotateSurfaces(
                level,
                surfaces,
                sed::TVector::up,
                0.0,
                0.0,
                0.0,
                13.0);
            sed::level::levelTranslateEdges(
                level, edges, -0.125, 0.375, -0.625);
            sed::level::levelTranslateVertices(
                level, vertices, 0.5, -0.25, 0.125);

            sed::ScaleData scale{};
            scale.cx = 0.25;
            scale.cy = -0.5;
            scale.cz = 0.75;
            scale.sfactor = 1.125;
            scale.how = sed::ScaleHow::xyz;
            sed::level::levelScaleSectors(level, sectors, scale, false);
        };
        checkTransformPreservesAuthoredUvs(
            pixelBaseline,
            mixedCompatibilityTransforms,
            "transform UV preservation: explicit legacy pixel-space compatibility");
    }

    {
        sed::Level level{};
        level.sectors.resize(2);

        sed::Sector& a = level.sectors[0];
        a.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        sed::Sector& b = level.sectors[1];
        b.vertices = {
            { 10.0, 0.0, 0.0, 0 },
            { 11.0, 0.0, 0.0, 0 },
            { 10.0, 1.0, 0.0, 0 },
        };

        sed::Surface sa{};
        sa.vertices = { 0, 1, 2 };
        sa.texVertices = {
            { 0.0f, 0.0f, sed::ColorWhite },
            { 1.0f, 0.0f, sed::ColorWhite },
            { 0.0f, 1.0f, sed::ColorWhite },
        };
        sa.adjoin = sed::SurfaceRef{ 1, 0 };
        a.surfaces.push_back(sa);

        sed::Surface sb{};
        sb.vertices = { 0, 1, 2 };
        sb.texVertices = sa.texVertices;
        sb.adjoin = sed::SurfaceRef{ 0, 0 };
        b.surfaces.push_back(sb);

        const std::array<std::int32_t, 1> sectors{ 0 };
        sed::level::levelTranslateSectors(level, sectors, 1.0, 2.0, 3.0);

        check(close(a.vertices[0].x, 1.0), "translateSectors: a.v0.x");
        check(close(a.vertices[0].y, 2.0), "translateSectors: a.v0.y");
        check(close(a.vertices[0].z, 3.0), "translateSectors: a.v0.z");

        check(close(b.vertices[0].x, 11.0), "translateSectors: b.v0.x");
        check(close(b.vertices[0].y, 2.0), "translateSectors: b.v0.y");
        check(close(b.vertices[0].z, 3.0), "translateSectors: b.v0.z");

        check(a.vertices[0].mark == 1, "translateSectors: a.v0.mark");
        check(b.vertices[0].mark == 1, "translateSectors: b.v0.mark");
    }

    {
        sed::Level level{};
        level.sectors.resize(2);

        sed::Sector& a = level.sectors[0];
        a.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        sed::Sector& b = level.sectors[1];
        b.vertices = {
            { 10.0, 0.0, 0.0, 0 },
            { 11.0, 0.0, 0.0, 0 },
            { 10.0, 1.0, 0.0, 0 },
        };

        sed::Surface sa{};
        sa.vertices = { 0, 1, 2 };
        sa.texVertices = {
            { 0.0f, 0.0f, sed::ColorWhite },
            { 1.0f, 0.0f, sed::ColorWhite },
            { 0.0f, 1.0f, sed::ColorWhite },
        };
        sa.adjoin = sed::SurfaceRef{ 1, 0 };
        a.surfaces.push_back(sa);

        sed::Surface sb{};
        sb.vertices = { 0, 1, 2 };
        sb.texVertices = sa.texVertices;
        sb.adjoin = sed::SurfaceRef{ 0, 0 };
        b.surfaces.push_back(sb);

        const std::array<std::int32_t, 2> sectors{ 0, 1 };
        sed::level::levelTranslateSectors(level, sectors, 1.0, 0.0, 0.0);

        check(close(a.vertices[0].x, 1.0), "translateSectors.adjoinedBoth: a once");
        check(close(b.vertices[0].x, 11.0), "translateSectors.adjoinedBoth: b once");
        check(a.vertices[0].mark == 1, "translateSectors.adjoinedBoth: a mark");
        check(b.vertices[0].mark == 1, "translateSectors.adjoinedBoth: b mark");
    }

    {
        sed::Level level{};
        level.sectors.resize(1);

        sed::Sector& sector = level.sectors[0];
        sector.vertices = {
            { 0.0, 0.0, 0.0, 7 },
            { 1.0, 0.0, 0.0, 8 },
            { 0.0, 1.0, 0.0, 9 },
        };
        sector.surfaces.push_back(makeTransformTestSurface());

        sed::level::levelClearVertexMarks(level);
        check(sector.vertices[0].mark == 0, "levelClearVertexMarks: v0");
        check(sector.vertices[1].mark == 0, "levelClearVertexMarks: v1");
        check(sector.vertices[2].mark == 0, "levelClearVertexMarks: v2");

        const std::array<sed::VertexRef, 2> vertices{ sed::VertexRef{ 0, 0 }, sed::VertexRef{ 9, 0 } };
        sed::level::levelTranslateVertices(level, vertices, 1.0, 2.0, 3.0);
        check(close(sector.vertices[0].x, 1.0), "translateVertices: v0.x");
        check(close(sector.vertices[0].y, 2.0), "translateVertices: v0.y");
        check(close(sector.vertices[0].z, 3.0), "translateVertices: v0.z");
        check(close(sector.vertices[1].x, 1.0), "translateVertices.invalid: v1.x");

        const std::array<sed::level::EdgeRef, 2> edges{ sed::level::EdgeRef{ 0, 0, 1 }, sed::level::EdgeRef{ 0, 0, 99 } };
        sed::level::levelTranslateEdges(level, edges, 0.0, 10.0, 0.0);
        check(close(sector.vertices[0].y, 2.0), "translateEdges: untouched v0.y");
        check(close(sector.vertices[1].y, 10.0), "translateEdges: v1.y");
        check(close(sector.vertices[2].y, 11.0), "translateEdges: v2.y");

        const std::array<sed::SurfaceRef, 2> surfaces{ sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 0, 99 } };
        sed::level::levelTranslateSurfaces(level, surfaces, 0.0, 0.0, 5.0);
        check(close(sector.vertices[0].z, 8.0), "translateSurfaces: v0.z");
        check(close(sector.vertices[1].z, 5.0), "translateSurfaces: v1.z");
        check(close(sector.vertices[2].z, 5.0), "translateSurfaces: v2.z");
    }

    {
        sed::Level level{};
        level.things.resize(1);
        level.things[0].x = 1.0;
        level.things[0].y = 2.0;
        level.things[0].z = 3.0;
        level.things[0].values.push_back(makeFrameValue(10.0, 20.0, 30.0, 1.0, 2.0, 3.0));
        level.things[0].values.push_back(makeFrameValue(40.0, 50.0, 60.0, 4.0, 5.0, 6.0, "not-frame"));

        const std::array<std::int32_t, 2> things{ 0, 99 };
        sed::level::levelTranslateThings(level, things, 1.0, 2.0, 3.0, false);
        check(close(level.things[0].x, 2.0), "translateThings.noFrames: x");
        check(close(level.things[0].y, 4.0), "translateThings.noFrames: y");
        check(close(level.things[0].z, 6.0), "translateThings.noFrames: z");
        checkFrame(level.things[0].values[0], 10.0, 20.0, 30.0, 1.0, 2.0, 3.0, "translateThings.noFrames: frame");

        sed::level::levelTranslateThings(level, things, -2.0, 1.0, 0.5, true);
        check(close(level.things[0].x, 0.0), "translateThings.withFrames: x");
        check(close(level.things[0].y, 5.0), "translateThings.withFrames: y");
        check(close(level.things[0].z, 6.5), "translateThings.withFrames: z");
        checkFrame(level.things[0].values[0], 8.0, 21.0, 30.5, 1.0, 2.0, 3.0, "translateThings.withFrames: frame");

        const std::array<sed::level::FrameRef, 3> frames{
            sed::level::FrameRef{ 0, 0 },
            sed::level::FrameRef{ 0, 1 },
            sed::level::FrameRef{ 9, 0 },
        };
        sed::level::levelTranslateFrames(level, frames, 1.0, 1.0, 1.0);
        checkFrame(level.things[0].values[0], 9.0, 22.0, 31.5, 1.0, 2.0, 3.0, "translateFrames: valid");
        checkFrame(level.things[0].values[1], 38.0, 51.0, 60.5, 4.0, 5.0, 6.0, "translateFrames: invalid-name skipped");

        level.lights.resize(1);
        level.lights[0].position = sed::TVector{ 1.0, 2.0, 3.0 };
        const std::array<std::int32_t, 2> lights{ 0, 42 };
        sed::level::levelTranslateLights(level, lights, 3.0, -1.0, 2.0);
        check(closeVec(level.lights[0].position, sed::TVector{ 4.0, 1.0, 5.0 }, kRotationEpsilon), "translateLights: position");
    }

    {
        sed::Level level{};
        level.clear();
        const std::int32_t firstSector =
            sed::level::levelCreateSectorPrimitiveBox(
                level,
                sed::level::SectorPrimitive::Cube,
                sed::TVector{ -1.0, -1.0, -1.0 },
                sed::TVector{ 1.0, 1.0, 1.0 });
        const std::int32_t secondSector =
            sed::level::levelCreateSectorPrimitiveBox(
                level,
                sed::level::SectorPrimitive::Cube,
                sed::TVector{ 3.0, -1.0, -1.0 },
                sed::TVector{ 5.0, 1.0, 1.0 });

        level.things.resize(1);
        level.things[0].sector = firstSector;
        const std::array<std::int32_t, 1> thing{ 0 };

        sed::level::levelTranslateThings(
            level, thing, 4.0, 0.0, 0.0, false, true);
        const bool enteredSecond =
            level.things[0].sector == secondSector;

        sed::level::levelTranslateThings(
            level, thing, 3.0, 0.0, 0.0, false, false);
        const bool explicitNoUpdatePreservedSector =
            level.things[0].sector == secondSector;

        sed::level::levelTranslateThings(
            level, thing, 0.0, 0.0, 0.0, false, true);
        const bool outsideAllSectors = level.things[0].sector == -1;

        sed::level::levelTranslateThings(
            level, thing, -7.0, 0.0, 0.0, false, true);
        check(firstSector == 0 && secondSector == 1 && enteredSecond
                && explicitNoUpdatePreservedSector && outsideAllSectors
                && level.things[0].sector == firstSector,
            "levelTranslateThings updates Sector ownership after movement, preserves it when disabled, and reports outside geometry");
    }

    {
        sed::Level level{};
        level.clear();
        const auto addBox = [&level](const sed::TVector& minimum, const sed::TVector& maximum)
        {
            return sed::level::levelCreateSectorPrimitiveBox(
                level, sed::level::SectorPrimitive::Cube, minimum, maximum);
        };
        const std::int32_t leftSector = addBox(
            sed::TVector{-5.0, -3.0, -1.0}, sed::TVector{-3.0, 3.0, 1.0});
        const std::int32_t rightSector = addBox(
            sed::TVector{3.0, -1.0, -1.0}, sed::TVector{5.0, 1.0, 1.0});
        const std::int32_t centerSector = addBox(
            sed::TVector{-1.0, -1.0, -1.0}, sed::TVector{1.0, 1.0, 1.0});

        level.things.resize(4);
        level.things[0].x = -4.0;
        level.things[0].y = 0.25;
        level.things[0].sector = leftSector;
        level.things[1].x = 4.0;
        level.things[1].y = -0.25;
        level.things[1].sector = rightSector;
        level.things[2].x = 0.5;
        level.things[2].sector = centerSector;
        level.things[3].x = -4.0;
        level.things[3].y = 2.5;
        level.things[3].sector = leftSector;

        const std::array<std::int32_t, 4> things{0, 1, 2, 3};
        sed::level::levelRotateThings(
            level, things, sed::TVector::up, 0.0, 0.0, 0.0, 180.0, false);

        check(leftSector == 0 && rightSector == 1 && centerSector == 2
                && close(level.things[0].x, 4.0, kRotationEpsilon)
                && close(level.things[0].y, -0.25, kRotationEpsilon)
                && close(level.things[0].z, 0.0, kRotationEpsilon)
                && level.things[0].sector == rightSector
                && close(level.things[1].x, -4.0, kRotationEpsilon)
                && close(level.things[1].y, 0.25, kRotationEpsilon)
                && close(level.things[1].z, 0.0, kRotationEpsilon)
                && level.things[1].sector == leftSector
                && close(level.things[2].x, -0.5, kRotationEpsilon)
                && close(level.things[2].y, 0.0, kRotationEpsilon)
                && close(level.things[2].z, 0.0, kRotationEpsilon)
                && level.things[2].sector == centerSector
                && close(level.things[3].x, 4.0, kRotationEpsilon)
                && close(level.things[3].y, -2.5, kRotationEpsilon)
                && close(level.things[3].z, 0.0, kRotationEpsilon)
                && level.things[3].sector == -1,
            "levelRotateThings updates cross-sector Thing ownership, preserves same-sector ownership, and reports outside geometry");
    }

    {
        sed::Level level{};
        level.clear();
        const auto addBox = [&level](const sed::TVector& minimum, const sed::TVector& maximum)
        {
            return sed::level::levelCreateSectorPrimitiveBox(
                level, sed::level::SectorPrimitive::Cube, minimum, maximum);
        };

        constexpr std::int32_t kCrossingCount = 32;
        std::array<std::int32_t, kCrossingCount> sourceSectors{};
        std::array<std::int32_t, kCrossingCount> targetSectors{};
        for (std::int32_t i = 0; i < kCrossingCount; ++i)
        {
            const double sourceY = static_cast<double>(i) * 4.0;
            const double targetY = sourceY * 2.0;
            sourceSectors[static_cast<std::size_t>(i)] = addBox(
                sed::TVector{1.0, sourceY - 1.0, -1.0},
                sed::TVector{2.0, sourceY + 1.0, 1.0});
            targetSectors[static_cast<std::size_t>(i)] = addBox(
                sed::TVector{3.0, targetY - 1.0, -1.0},
                sed::TVector{4.0, targetY + 1.0, 1.0});
        }
        const std::int32_t centerSector = addBox(
            sed::TVector{-0.5, -0.5, -0.5}, sed::TVector{0.5, 0.5, 0.5});
        const std::int32_t boundarySourceSector = addBox(
            sed::TVector{10.0, -1.0, -1.0}, sed::TVector{11.0, 1.0, 1.0});
        const std::int32_t firstBoundarySector = addBox(
            sed::TVector{20.0, -1.0, -1.0}, sed::TVector{21.0, 1.0, 1.0});
        const std::int32_t secondBoundarySector = addBox(
            sed::TVector{21.0, -1.0, -1.0}, sed::TVector{22.0, 1.0, 1.0});

        for (std::int32_t i = 0; i < kCrossingCount; ++i)
        {
            sed::Thing thing{};
            thing.x = 1.5;
            thing.y = static_cast<double>(i) * 4.0;
            thing.sector = sourceSectors[static_cast<std::size_t>(i)];
            level.things.push_back(thing);
        }
        sed::Thing outsideThing{};
        outsideThing.x = 1.5;
        outsideThing.y = 0.75;
        outsideThing.sector = sourceSectors[0];
        level.things.push_back(outsideThing);
        sed::Thing sameSectorThing{};
        sameSectorThing.sector = centerSector;
        level.things.push_back(sameSectorThing);
        sed::Thing boundaryThing{};
        boundaryThing.x = 10.5;
        boundaryThing.sector = boundarySourceSector;
        level.things.push_back(boundaryThing);

        std::vector<std::int32_t> things{};
        things.reserve(level.things.size());
        for (std::size_t i = 0; i < level.things.size(); ++i)
        {
            things.push_back(static_cast<std::int32_t>(i));
        }

        sed::ScaleData scale{};
        scale.sfactor = 2.0;
        scale.how = sed::ScaleHow::xyz;
        sed::level::levelScaleThings(level, things, scale);

        bool everyCrossingResolved = true;
        for (std::int32_t i = 0; i < kCrossingCount; ++i)
        {
            const sed::Thing& thing = level.things[static_cast<std::size_t>(i)];
            everyCrossingResolved = everyCrossingResolved
                && thing.x == 3.0
                && thing.y == static_cast<double>(i) * 8.0
                && thing.z == 0.0
                && thing.sector == targetSectors[static_cast<std::size_t>(i)];
        }
        const sed::Thing& scaledOutside = level.things[kCrossingCount];
        const sed::Thing& scaledSameSector = level.things[kCrossingCount + 1u];
        const sed::Thing& scaledBoundary = level.things[kCrossingCount + 2u];
        check(level.sectors.size() == 2u * kCrossingCount + 4u
                && firstBoundarySector < secondBoundarySector
                && everyCrossingResolved
                && scaledOutside.x == 3.0
                && scaledOutside.y == 1.5
                && scaledOutside.sector == -1
                && scaledSameSector.x == 0.0
                && scaledSameSector.y == 0.0
                && scaledSameSector.z == 0.0
                && scaledSameSector.sector == centerSector
                && scaledBoundary.x == 21.0
                && scaledBoundary.y == 0.0
                && scaledBoundary.sector == firstBoundarySector,
            "levelScaleThings resolves a large cross-sector selection, retains its same-sector owner, reports misses, and preserves lowest-index portal ties");
    }

    {
        sed::Level rotated{};
        rotated.clear();
        const auto addRotateBox = [&rotated](const sed::TVector& minimum, const sed::TVector& maximum)
        {
            return sed::level::levelCreateSectorPrimitiveBox(
                rotated, sed::level::SectorPrimitive::Cube, minimum, maximum);
        };
        const std::int32_t rotateSourceSector = addRotateBox(
            sed::TVector{-1.0, -1.0, -1.0}, sed::TVector{1.0, 1.0, 1.0});
        const std::int32_t rotateTargetSector = addRotateBox(
            sed::TVector{3.0, -1.0, -1.0}, sed::TVector{5.0, 1.0, 1.0});
        rotated.things.resize(1);
        rotated.things[0].sector = rotateSourceSector;
        const std::array<std::int32_t, 1> oneThing{0};
        sed::level::levelRotateThings(
            rotated, oneThing, sed::TVector::up, 2.0, 0.0, 0.0, 180.0, false);

        sed::Level scaled{};
        scaled.clear();
        const auto addScaleBox = [&scaled](const sed::TVector& minimum, const sed::TVector& maximum)
        {
            return sed::level::levelCreateSectorPrimitiveBox(
                scaled, sed::level::SectorPrimitive::Cube, minimum, maximum);
        };
        const std::int32_t scaleSourceSector = addScaleBox(
            sed::TVector{1.0, -1.0, -1.0}, sed::TVector{2.0, 1.0, 1.0});
        const std::int32_t scaleTargetSector = addScaleBox(
            sed::TVector{3.0, -1.0, -1.0}, sed::TVector{4.0, 1.0, 1.0});
        scaled.things.resize(1);
        scaled.things[0].x = 1.5;
        scaled.things[0].y = 0.25;
        scaled.things[0].sector = scaleSourceSector;
        sed::ScaleData scale{};
        scale.sfactor = 2.0;
        scale.how = sed::ScaleHow::xyz;
        sed::level::levelScaleThings(scaled, oneThing, scale);

        check(rotateSourceSector == 0 && rotateTargetSector == 1
                && close(rotated.things[0].x, 4.0, kRotationEpsilon)
                && close(rotated.things[0].y, 0.0, kRotationEpsilon)
                && close(rotated.things[0].z, 0.0, kRotationEpsilon)
                && rotated.things[0].sector == rotateTargetSector
                && scaleSourceSector == 0 && scaleTargetSector == 1
                && scaled.things[0].x == 3.0
                && scaled.things[0].y == 0.5
                && scaled.things[0].z == 0.0
                && scaled.things[0].sector == scaleTargetSector,
            "single-Thing Rotate and Scale resolve cross-sector ownership through their non-index fallback");
    }

    {
        sed::Level level{};
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        level.sectors.resize(1);
        sed::Sector& sector = level.sectors[0];
        sector.vertices = {
            { -1.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };
        sector.surfaces.push_back(makeTransformTestSurface());

        const std::array<sed::level::EdgeRef, 2> edges{
            sed::level::EdgeRef{0, 0, 0},
            sed::level::EdgeRef{0, 0, 99},
        };
        sed::level::levelRotateEdges(
            level, edges, sed::TVector::up,
            0.0, 0.0, 0.0, 90.0);
        check(close(sector.vertices[0].x, 0.0, kRotationEpsilon)
                  && close(sector.vertices[0].y, 1.0, kRotationEpsilon)
                  && close(sector.vertices[0].z, 0.0, kRotationEpsilon),
              "rotateEdges: first endpoint");
        check(close(sector.vertices[1].x, 0.0, kRotationEpsilon)
                  && close(sector.vertices[1].y, -1.0, kRotationEpsilon)
                  && close(sector.vertices[1].z, 0.0, kRotationEpsilon),
              "rotateEdges: second endpoint");
        check(close(sector.vertices[2].x, 0.0, kRotationEpsilon)
                  && close(sector.vertices[2].y, 1.0, kRotationEpsilon)
                  && close(sector.vertices[2].z, 0.0, kRotationEpsilon),
              "rotateEdges: unselected vertex unchanged");
    }

    {
        sed::Level level{};
        level.sectors.resize(1);

        sed::Sector& sector = level.sectors[0];
        sector.vertices = {
            { 1.0, 0.0, 0.0, 0 },
            { 0.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        const std::array<std::int32_t, 1> sectors{ 0 };
        sed::level::levelRotateSectors(level, sectors, sed::TVector::up, 0.0, 0.0, 0.0, 90.0);

        check(close(sector.vertices[0].x, 0.0, kRotationEpsilon), "rotateSectors: v0.x");
        check(close(sector.vertices[0].y, -1.0, kRotationEpsilon), "rotateSectors: v0.y");
        check(close(sector.vertices[0].z, 0.0, kRotationEpsilon), "rotateSectors: v0.z");
    }

    {
        sed::Level level{};
        level.things.resize(1);

        sed::Thing& thing = level.things[0];
        thing.pch = 350.0;
        thing.yaw = 0.0;
        thing.rol = 0.0;

        sed::level::rotatePYR(thing.pch, thing.yaw, thing.rol, sed::TVector::right, 20.0);

        check(close(thing.pch, 10.0, kExactEpsilon), "rotatePYR: clamp +");
        check(close(thing.yaw, 0.0, kExactEpsilon), "rotatePYR: yaw");
        check(close(thing.rol, 0.0, kExactEpsilon), "rotatePYR: roll");
    }

    {
        sed::Level level{};
        level.sectors.resize(1);

        sed::Sector& sector = level.sectors[0];
        sector.vertices = {
            { 1.0, 0.0, 0.0, 0 },
            { 0.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };
        sector.surfaces.push_back(makeTransformTestSurface());

        const std::array<sed::SurfaceRef, 2> surfaces{ sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 0, 99 } };
        sed::level::levelRotateSurfaces(level, surfaces, sed::TVector::up, 0.0, 0.0, 0.0, 90.0);

        check(close(sector.vertices[0].x, 0.0, kRotationEpsilon), "rotateSurfaces: v0.x");
        check(close(sector.vertices[0].y, -1.0, kRotationEpsilon), "rotateSurfaces: v0.y");
        check(close(sector.vertices[2].x, 1.0, kRotationEpsilon), "rotateSurfaces: v2.x");
        check(close(sector.vertices[2].y, 0.0, kRotationEpsilon), "rotateSurfaces: v2.y");
    }

    {
        sed::Level level{};
        level.things.resize(2);
        level.things[0].x = 1.0;
        level.things[0].yaw = 10.0;
        level.things[0].values.push_back(makeFrameValue(1.0, 0.0, 0.0, 0.0, 10.0, 0.0));
        level.things[0].values.push_back(makeFrameValue(2.0, 0.0, 0.0, 0.0, 20.0, 0.0, "not-frame"));

        level.things[1].x = 0.0;
        level.things[1].y = 1.0;
        level.things[1].yaw = 33.0;

        const std::array<std::int32_t, 2> firstThing{ 0, 99 };
        sed::level::levelRotateThings(level, firstThing, sed::TVector::up, 0.0, 0.0, 0.0, 90.0, true);
        check(close(level.things[0].x, 0.0, kRotationEpsilon), "rotateThings.withPYR: x");
        check(close(level.things[0].y, -1.0, kRotationEpsilon), "rotateThings.withPYR: y");
        check(close(level.things[0].yaw, -80.0, kRotationEpsilon), "rotateThings.withPYR: yaw");

        const std::array<std::int32_t, 1> secondThing{ 1 };
        sed::level::levelRotateThings(level, secondThing, sed::TVector::up, 0.0, 0.0, 0.0, 90.0, false);
        check(close(level.things[1].x, 1.0, kRotationEpsilon), "rotateThings.noPYR: x");
        check(close(level.things[1].y, 0.0, kRotationEpsilon), "rotateThings.noPYR: y");
        check(close(level.things[1].yaw, 33.0, kRotationEpsilon), "rotateThings.noPYR: yaw");

        level.lights.resize(1);
        level.lights[0].position = sed::TVector{ 1.0, 0.0, 0.0 };
        const std::array<std::int32_t, 2> lights{ 0, 99 };
        sed::level::levelRotateLights(level, lights, sed::TVector::up, 0.0, 0.0, 0.0, 90.0);
        check(closeVec(level.lights[0].position, sed::TVector{ 0.0, -1.0, 0.0 }, kRotationEpsilon), "rotateLights: position");

        const std::array<sed::level::FrameRef, 3> frames{
            sed::level::FrameRef{ 0, 0 },
            sed::level::FrameRef{ 0, 1 },
            sed::level::FrameRef{ 9, 0 },
        };
        sed::level::levelRotateFrames(level, frames, sed::TVector::up, 0.0, 0.0, 0.0, 90.0, true);
        checkFrame(level.things[0].values[0], 0.0, -1.0, 0.0, 0.0, -80.0, 0.0, "rotateFrames: valid");
        checkFrame(level.things[0].values[1], 2.0, 0.0, 0.0, 0.0, 20.0, 0.0, "rotateFrames: invalid-name skipped");
    }

    {
        sed::Level unitAxis{};
        unitAxis.things.resize(1);
        unitAxis.things[0].x = 2.0;
        unitAxis.things[0].yaw = 15.0;
        unitAxis.things[0].values.push_back(
            makeFrameValue(2.0, 0.0, 0.0, 0.0, 15.0, 0.0));
        sed::Level scaledAxis = unitAxis;
        const std::array<std::int32_t, 1> thing{ 0 };
        const std::array<sed::level::FrameRef, 1> frame{
            sed::level::FrameRef{ 0, 0 }
        };

        sed::level::levelRotateThings(
            unitAxis, thing, sed::TVector::up,
            0.0, 0.0, 0.0, 30.0, true);
        sed::level::levelRotateFrames(
            unitAxis, frame, sed::TVector::up,
            0.0, 0.0, 0.0, 30.0, true);

        const sed::TVector nonUnitAxis = sed::TVector::up * 7.0;
        sed::level::levelRotateThings(
            scaledAxis, thing, nonUnitAxis,
            0.0, 0.0, 0.0, 30.0, true);
        sed::level::levelRotateFrames(
            scaledAxis, frame, nonUnitAxis,
            0.0, 0.0, 0.0, 30.0, true);

        check(close(unitAxis.things[0].x, scaledAxis.things[0].x,
                    kRotationEpsilon)
                && close(unitAxis.things[0].y, scaledAxis.things[0].y,
                    kRotationEpsilon)
                && close(unitAxis.things[0].z, scaledAxis.things[0].z,
                    kRotationEpsilon)
                && close(unitAxis.things[0].pch,
                    scaledAxis.things[0].pch, kRotationEpsilon)
                && close(unitAxis.things[0].yaw,
                    scaledAxis.things[0].yaw, kRotationEpsilon)
                && close(unitAxis.things[0].rol,
                    scaledAxis.things[0].rol, kRotationEpsilon)
                && unitAxis.things[0].values[0].asString()
                    == scaledAxis.things[0].values[0].asString(),
            "Thing and Frame rotation positions and PYR are independent of finite axis magnitude");
    }

    {
        sed::Level level{};
        level.things.push_back(sed::Thing{});
        level.things[0].x = 3.0;
        level.things[0].values.push_back(
            makeFrameValue(3.0, 1.0, 2.0, 4.0, 5.0, 6.0));

        level.lights.push_back(sed::SedLight{});
        level.lights[0].position = sed::TVector{ 3.0, 1.0, 2.0 };

        level.sectors.resize(1);
        sed::Sector& sector = level.sectors[0];
        sector.vertices = {
            { 3.0, 0.0, 0.0, 0 },
            { 0.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        sed::Surface surface{};
        surface.vertices = { 0, 1, 2 };
        surface.texVertices = {
            { 0.0f, 0.0f, sed::ColorWhite },
            { 1.0f, 0.0f, sed::ColorWhite },
            { 0.0f, 1.0f, sed::ColorWhite },
        };
        surface.uScale = 2.0f;
        surface.vScale = 4.0f;
        sector.surfaces.push_back(surface);

        sed::ScaleData sd{};
        sd.cx = 0.0;
        sd.cy = 0.0;
        sd.cz = 0.0;
        sd.sfactor = 2.0;
        sd.vec = sed::TVector::right;
        sd.how = sed::ScaleHow::vec;

        const std::array<std::int32_t, 1> things{ 0 };
        sed::level::levelScaleThings(level, things, sd);
        check(close(level.things[0].x, 6.0, kExactEpsilon), "levelScaleThings.scale_vec: x");

        const std::array<std::int32_t, 1> lights{ 0 };
        sed::level::levelScaleLights(level, lights, sd);
        check(closeVec(level.lights[0].position, sed::TVector{ 6.0, 1.0, 2.0 }, kExactEpsilon),
            "levelScaleLights.scale_vec uses the same multiplicative axis projection as geometry");

        const std::array<sed::level::FrameRef, 1> frames{ sed::level::FrameRef{ 0, 0 } };
        sed::level::levelScaleFrames(level, frames, sd);
        checkFrame(level.things[0].values[0], 6.0, 1.0, 2.0, 4.0, 5.0, 6.0,
            "levelScaleFrames.scale_vec uses the same multiplicative axis projection as geometry");

        const std::array<sed::SurfaceRef, 1> surfaces{ sed::SurfaceRef{ 0, 0 } };
        sed::level::levelScaleSurfaces(level, surfaces, sd, true);

        check(close(sector.vertices[0].x, 6.0, kExactEpsilon), "levelScaleSurfaces.scale_vec: v0.x");
        check(close(sector.surfaces[0].uScale, 1.0f, kRotationEpsilon), "levelScaleSurfaces: uScale");
        check(close(sector.surfaces[0].vScale, 2.0f, kRotationEpsilon), "levelScaleSurfaces: vScale");
    }

    {
        sed::Level level{};
        level.sectors.resize(1);

        sed::Sector& sector = level.sectors[0];
        sector.vertices = {
            { 1.0, 2.0, 3.0, 0 },
            { 2.0, 0.0, 0.0, 0 },
            { 0.0, 2.0, 0.0, 0 },
        };
        sed::Surface surface = makeTransformTestSurface();
        surface.uScale = 4.0f;
        surface.vScale = 8.0f;
        sector.surfaces.push_back(surface);

        sed::ScaleData sd{};
        sd.sfactor = 2.0;
        sd.how = sed::ScaleHow::xyz;

        const std::array<std::int32_t, 2> sectors{ 0, 99 };
        sed::level::levelScaleSectors(level, sectors, sd, true);
        check(close(sector.vertices[0].x, 2.0), "levelScaleSectors: v0.x");
        check(close(sector.vertices[0].y, 4.0), "levelScaleSectors: v0.y");
        check(close(sector.vertices[0].z, 6.0), "levelScaleSectors: v0.z");
        check(close(sector.surfaces[0].uScale, 2.0f), "levelScaleSectors: uScale");
        check(close(sector.surfaces[0].vScale, 4.0f), "levelScaleSectors: vScale");

        level.lights.resize(1);
        level.lights[0].position = sed::TVector{ 2.0, 3.0, 4.0 };
        const std::array<std::int32_t, 2> lights{ 0, 99 };
        sed::level::levelScaleLights(level, lights, sd);
        check(closeVec(level.lights[0].position, sed::TVector{ 4.0, 6.0, 8.0 }, kRotationEpsilon), "levelScaleLights: position");

        level.things.resize(1);
        level.things[0].values.push_back(makeFrameValue(1.0, 2.0, 3.0, 4.0, 5.0, 6.0));
        level.things[0].values.push_back(makeFrameValue(7.0, 8.0, 9.0, 10.0, 11.0, 12.0, "not-frame"));

        const std::array<sed::level::FrameRef, 3> frames{
            sed::level::FrameRef{ 0, 0 },
            sed::level::FrameRef{ 0, 1 },
            sed::level::FrameRef{ 9, 0 },
        };
        sed::level::levelScaleFrames(level, frames, sd);
        checkFrame(level.things[0].values[0], 2.0, 4.0, 6.0, 4.0, 5.0, 6.0, "levelScaleFrames: valid");
        checkFrame(level.things[0].values[1], 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, "levelScaleFrames: invalid-name skipped");
    }

    {
        sed::Level level{};
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        level.sectors.resize(1);

        sed::Sector& sector = level.sectors[0];
        sector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        sed::Surface surface{};
        surface.vertices = { 0, 1, 2 };
        surface.texVertices = {
            { -2.5f, 4.25f, sed::ColorRed },
            { 7.75f, -3.5f, sed::ColorGreen },
            { 1.125f, 9.5f, sed::ColorBlue },
        };
        surface.uScale = -0.125f;
        surface.vScale = 0.375f;
        surface.faceFlags = 0;
        sector.surfaces.push_back(surface);

        const std::vector<std::int32_t> originalVertices =
            sector.surfaces[0].vertices;
        const std::vector<sed::TexVertex> originalTextureVertices =
            sector.surfaces[0].texVertices;

        const std::array<sed::SurfaceRef, 1> surfaces{ sed::SurfaceRef{ 0, 0 } };
        sed::level::levelFlipSurfaces(level, surfaces);

        check(sector.surfaces[0].vertices.size() == 3, "levelFlipSurfaces: vertex count");
        check(sector.surfaces[0].vertices[0] == 2, "levelFlipSurfaces: v0");
        check(sector.surfaces[0].vertices[1] == 1, "levelFlipSurfaces: v1");
        check(sector.surfaces[0].vertices[2] == 0, "levelFlipSurfaces: v2");
        const std::vector<sed::TexVertex> reversedTextureVertices(
            originalTextureVertices.rbegin(),
            originalTextureVertices.rend());
        check(sector.surfaces[0].texVertices.size()
                    == reversedTextureVertices.size()
                && std::equal(
                    sector.surfaces[0].texVertices.begin(),
                    sector.surfaces[0].texVertices.end(),
                    reversedTextureVertices.begin(),
                    sameTexVertex)
                && sector.surfaces[0].uScale == -0.125f
                && sector.surfaces[0].vScale == 0.375f
                && level.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized,
            "levelFlipSurfaces.uv: winding reversal keeps every authored vertex/UV/color pair and scale exact");

        check((sector.surfaces[0].faceFlags & sed::FF_SF_FLIP) != 0, "levelFlipSurfaces: FF_SF_FLIP set");
        check(closeVec(sector.surfaces[0].normal, sed::TVector{ 0.0, 0.0, -1.0 }, kRotationEpsilon), "levelFlipSurfaces: normal flipped");

        sed::level::levelFlipSurfaces(level, surfaces);
        check((sector.surfaces[0].faceFlags & sed::FF_SF_FLIP) == 0, "levelFlipSurfaces: FF_SF_FLIP cleared");
        check(closeVec(sector.surfaces[0].normal, sed::TVector{ 0.0, 0.0, 1.0 }, kRotationEpsilon), "levelFlipSurfaces: normal restored");
        check(sector.surfaces[0].vertices == originalVertices
                && sector.surfaces[0].texVertices.size()
                    == originalTextureVertices.size()
                && std::equal(
                    sector.surfaces[0].texVertices.begin(),
                    sector.surfaces[0].texVertices.end(),
                    originalTextureVertices.begin(),
                    sameTexVertex)
                && sector.surfaces[0].uScale == -0.125f
                && sector.surfaces[0].vScale == 0.375f,
            "levelFlipSurfaces.uv: double flip restores the complete authored texture state");
    }

    {
        // Keep the transform-side texture winding check aligned with the UV
        // geometry helper for valid faces below the preferred triangle size.
        // Real levels contain faces at this scale.
        sed::Level level{};
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        level.sectors.resize(1);

        sed::Sector& sector = level.sectors[0];
        sector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 0.0012, 0.0, 0.0, 0 },
            { 0.0, 0.0012, 0.0, 0 },
        };

        sed::Surface surface{};
        surface.vertices = { 0, 1, 2 };
        surface.texVertices = {
            { 0.0f, 0.0f, sed::ColorWhite },
            { 0.0012f, 0.0f, sed::ColorWhite },
            { 0.0f, 0.0012f, sed::ColorWhite },
        };
        surface.normal = sed::TVector{ 0.0, 0.0, 1.0 };
        sector.surfaces.push_back(surface);

        const std::array<sed::SurfaceRef, 1> surfaces{
            sed::SurfaceRef{ 0, 0 },
        };
        sed::level::levelFlipSurfaces(level, surfaces);

        check((sector.surfaces[0].faceFlags & sed::FF_SF_FLIP) != 0,
            "levelFlipSurfaces.smallFace: valid sub-centimetre face preserves texture winding classification");
    }

    {
        // Mirror has the same containing-Sector contract as Move/Rotate/Scale.
        // Exercise both public helpers along every principal axis, including
        // an outside result and a shared-boundary lowest-index tie.
        for ( const bool planeMirror : {false, true} )
        {
            for ( const sed::level::Axis axis :
                {sed::level::Axis::x, sed::level::Axis::y, sed::level::Axis::z} )
            {
                const auto point = [axis](const double primary, const double secondary,
                    const double third = 0.0)
                {
                    if ( axis == sed::level::Axis::x )
                    {
                        return sed::TVector{primary, secondary, third};
                    }
                    if ( axis == sed::level::Axis::y )
                    {
                        return sed::TVector{secondary, primary, third};
                    }
                    return sed::TVector{secondary, third, primary};
                };
                sed::Level level{};
                level.clear();
                const auto addBox = [&level, &point](const double minimum, const double maximum,
                    const double secondaryMinimum = -1.0, const double secondaryMaximum = 1.0)
                {
                    return sed::level::levelCreateSectorPrimitiveBox(
                        level, sed::level::SectorPrimitive::Cube,
                        point(minimum, secondaryMinimum, -1.0),
                        point(maximum, secondaryMaximum, 1.0));
                };
                const std::int32_t left = addBox(-5.0, -3.0, -3.0, 3.0);
                const std::int32_t right = addBox(3.0, 5.0);
                const std::int32_t center = addBox(-1.0, 1.0);
                const std::int32_t firstBoundary = addBox(7.0, 8.0);
                const std::int32_t secondBoundary = addBox(8.0, 9.0);
                const std::int32_t boundarySource = addBox(-9.0, -7.0);
                const std::array<sed::TVector, 6> positions{
                    point(-4.0, 0.25), point(4.0, -0.25), point(0.5, 0.0),
                    point(-4.0, 2.5), point(-8.0, 0.0), point(8.5, 0.0),
                };
                const std::array<std::int32_t, 6> owners{
                    left, right, center, left, boundarySource, secondBoundary,
                };
                level.things.resize(positions.size());
                for ( std::size_t index = 0; index < positions.size(); ++index )
                {
                    sed::Thing& thing = level.things[index];
                    thing.x = positions[index].x;
                    thing.y = positions[index].y;
                    thing.z = positions[index].z;
                    thing.sector = owners[index];
                    thing.pch = 12.5;
                    thing.yaw = -27.25;
                    thing.rol = 3.75;
                }
                const std::array<std::int32_t, 7> selected{0, 1, 2, 3, 4, -1, 99};
                if ( planeMirror )
                {
                    sed::level::levelFlipThingsOverPlane(
                        level, selected, point(1.0, 0.0), 0.0, 0.0, 0.0);
                }
                else
                {
                    sed::level::levelFlipThings(level, selected, 0.0, 0.0, 0.0, axis);
                }
                const std::array<sed::TVector, 6> expectedPositions{
                    point(4.0, 0.25), point(-4.0, -0.25), point(-0.5, 0.0),
                    point(4.0, 2.5), point(8.0, 0.0), point(8.5, 0.0),
                };
                const std::array<std::int32_t, 6> expectedOwners{
                    right, left, center, -1, firstBoundary, secondBoundary,
                };
                bool positionsAndMetadataPreserved = true;
                bool ownershipResolved = true;
                for ( std::size_t index = 0; index < level.things.size(); ++index )
                {
                    const sed::Thing& thing = level.things[index];
                    positionsAndMetadataPreserved = positionsAndMetadataPreserved
                        && thing.x == expectedPositions[index].x
                        && thing.y == expectedPositions[index].y
                        && thing.z == expectedPositions[index].z
                        && thing.pch == 12.5 && thing.yaw == -27.25 && thing.rol == 3.75;
                    ownershipResolved = ownershipResolved && thing.sector == expectedOwners[index];
                }
                check(left == 0 && right == 1 && center == 2 && firstBoundary == 3
                        && secondBoundary == 4 && boundarySource == 5
                        && positionsAndMetadataPreserved,
                    "Thing Mirror preserves analytic positions, orientation, and unselected owners for every axis");
                check(ownershipResolved, planeMirror
                    ? "plane Thing Mirror resolves cross-sector, same-sector, outside, and boundary-tie ownership"
                    : "axis Thing Mirror resolves cross-sector, same-sector, outside, and boundary-tie ownership");
            }
        }
    }

    {
        sed::Level level{};
        level.clear();
        const std::int32_t source = sed::level::levelCreateSectorPrimitiveBox(
            level, sed::level::SectorPrimitive::Cube,
            sed::TVector{-5.0, -1.0, -1.0}, sed::TVector{-3.0, 1.0, 1.0});
        const std::int32_t target = sed::level::levelCreateSectorPrimitiveBox(
            level, sed::level::SectorPrimitive::Cube,
            sed::TVector{-1.0, 3.0, -1.0}, sed::TVector{1.0, 5.0, 1.0});
        level.things.resize(1);
        level.things[0].x = -4.0;
        level.things[0].y = 0.25;
        level.things[0].sector = source;
        const std::array<std::int32_t, 1> selected{0};
        sed::level::levelFlipThingsOverPlane(
            level, selected, sed::TVector{1.0, 1.0, 0.0}, 0.0, 0.0, 0.0);
        check(source == 0 && target == 1
                && closeVec(sed::TVector{level.things[0].x, level.things[0].y, level.things[0].z},
                    sed::TVector{-0.25, 4.0, 0.0}, kRotationEpsilon)
                && level.things[0].sector == target,
            "single-Thing oblique plane Mirror resolves its containing Sector without the multi-owner index");
    }

    {
        sed::Level level{};
        level.things.resize(3);
        for (sed::Thing& thing : level.things)
        {
            thing.x = 3.0;
            thing.y = 4.0;
            thing.z = 5.0;
        }

        const std::array<std::int32_t, 2> thing0{ 0, 99 };
        const std::array<std::int32_t, 1> thing1{ 1 };
        const std::array<std::int32_t, 1> thing2{ 2 };
        sed::level::levelFlipThings(level, thing0, 1.0, 2.0, 3.0, sed::level::Axis::x);
        sed::level::levelFlipThings(level, thing1, 1.0, 2.0, 3.0, sed::level::Axis::y);
        sed::level::levelFlipThings(level, thing2, 1.0, 2.0, 3.0, sed::level::Axis::z);
        check(close(level.things[0].x, -1.0), "levelFlipThings.x: x");
        check(close(level.things[1].y, 0.0), "levelFlipThings.y: y");
        check(close(level.things[2].z, 1.0), "levelFlipThings.z: z");

        sed::level::levelFlipThingsOverPlane(level, thing0, sed::TVector::right, 0.0, 0.0, 0.0);
        check(close(level.things[0].x, 1.0), "levelFlipThingsOverPlane: x");

        level.lights.resize(1);
        level.lights[0].position = sed::TVector{ 2.0, 3.0, 4.0 };
        const std::array<std::int32_t, 2> lights{ 0, 99 };
        sed::level::levelFlipLightsOverPlane(level, lights, sed::TVector::right, 0.0, 0.0, 0.0);
        check(closeVec(level.lights[0].position, sed::TVector{ -2.0, 3.0, 4.0 }, kRotationEpsilon), "levelFlipLightsOverPlane: position");

        level.things[0].values.push_back(makeFrameValue(2.0, 3.0, 4.0, 5.0, 6.0, 7.0));
        level.things[0].values.push_back(makeFrameValue(8.0, 9.0, 10.0, 11.0, 12.0, 13.0, "not-frame"));
        const std::array<sed::level::FrameRef, 3> frames{
            sed::level::FrameRef{ 0, 0 },
            sed::level::FrameRef{ 0, 1 },
            sed::level::FrameRef{ 9, 0 },
        };
        sed::level::levelFlipFramesOverPlane(level, frames, sed::TVector::right, 0.0, 0.0, 0.0);
        checkFrame(level.things[0].values[0], -2.0, 3.0, 4.0, 5.0, 6.0, 7.0, "levelFlipFramesOverPlane: valid");
        checkFrame(level.things[0].values[1], 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, "levelFlipFramesOverPlane: invalid-name skipped");
    }

    {
        sed::Level level{};
        level.sectors.resize(1);

        sed::Sector& sector = level.sectors[0];
        sector.vertices = {
            { 1.0, 4.0, 6.0, 0 },
            { 2.0, 4.0, 6.0, 0 },
            { 1.0, 5.0, 6.0, 0 },
        };
        sector.surfaces.push_back(makeTransformTestSurface());

        const std::array<std::int32_t, 1> sectors{ 0 };
        sed::level::levelFlipSectors(level, sectors, 0.0, 2.0, 3.0, sed::level::Axis::y);
        check(close(sector.vertices[0].y, 0.0), "levelFlipSectors.y: v0.y");

        sed::level::levelFlipSectors(level, sectors, 0.0, 2.0, 3.0, sed::level::Axis::z);
        check(close(sector.vertices[0].z, 0.0), "levelFlipSectors.z: v0.z");

        sed::level::levelFlipSectorsOverPlane(level, sectors, sed::TVector::right, 0.0, 0.0, 0.0);
        check(close(sector.vertices[0].x, -1.0), "levelFlipSectorsOverPlane: v0.x");
    }

    {
        sed::Level level{};
        level.sectors.resize(2);

        sed::Sector& a = level.sectors[0];
        a.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        sed::Sector& b = level.sectors[1];
        b.vertices = {
            { 10.0, 0.0, 0.0, 0 },
            { 11.0, 0.0, 0.0, 0 },
            { 10.0, 1.0, 0.0, 0 },
        };

        sed::Surface sa{};
        sa.vertices = { 0, 1, 2 };
        sa.texVertices = {
            { 0.0f, 0.0f, sed::ColorWhite },
            { 1.0f, 0.0f, sed::ColorWhite },
            { 0.0f, 1.0f, sed::ColorWhite },
        };
        sa.adjoin = sed::SurfaceRef{ 1, 0 };
        sa.adjoinFlags = 0x7B;
        sa.nAdj = 42;
        a.surfaces.push_back(sa);

        sed::Surface sb{};
        sb.vertices = { 0, 1, 2 };
        sb.texVertices = sa.texVertices;
        sb.adjoin = sed::SurfaceRef{ 0, 0 };
        sb.adjoinFlags = 0x1C8;
        sb.nAdj = 43;
        b.surfaces.push_back(sb);

        const std::array<std::int32_t, 1> sectors{ 0 };
        sed::level::levelFlipSectorsOverPlane(level, sectors, sed::TVector::right, 0.0, 0.0, 0.0);

        check(!a.surfaces[0].adjoin.has_value(), "levelFlipSectorsOverPlane: a unadjoined");
        check(!b.surfaces[0].adjoin.has_value(), "levelFlipSectorsOverPlane: b unadjoined");
        check(a.surfaces[0].adjoinFlags == 0x7B, "levelFlipSectorsOverPlane: a.adjoinFlags");
        check(a.surfaces[0].nAdj == -1, "levelFlipSectorsOverPlane: a.nAdj");
        check(b.surfaces[0].adjoinFlags == 0x1C8, "levelFlipSectorsOverPlane: b.adjoinFlags");
        check(b.surfaces[0].nAdj == -1, "levelFlipSectorsOverPlane: b.nAdj");
    }

    {
        sed::Level level{};
        level.sectors.resize(2);

        sed::Sector& a = level.sectors[0];
        a.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        sed::Sector& b = level.sectors[1];
        b.vertices = {
            { 10.0, 0.0, 0.0, 0 },
            { 11.0, 0.0, 0.0, 0 },
            { 10.0, 1.0, 0.0, 0 },
        };

        sed::Surface sa{};
        sa.vertices = { 0, 1, 2 };
        sa.texVertices = {
            { 0.0f, 0.0f, sed::ColorWhite },
            { 1.0f, 0.0f, sed::ColorWhite },
            { 0.0f, 1.0f, sed::ColorWhite },
        };
        sa.adjoin = sed::SurfaceRef{ 1, 0 };
        sa.adjoinFlags = 0xAA;
        sa.nAdj = 12;
        a.surfaces.push_back(sa);

        sed::Surface sb{};
        sb.vertices = { 0, 1, 2 };
        sb.texVertices = sa.texVertices;
        sb.adjoin = sed::SurfaceRef{ 0, 0 };
        sb.adjoinFlags = 0xBB;
        sb.nAdj = 13;
        b.surfaces.push_back(sb);

        const std::array<std::int32_t, 1> sectors{ 0 };
        sed::level::levelFlipSectors(level, sectors, 0.0, 0.0, 0.0, sed::level::Axis::x);

        check(!a.surfaces[0].adjoin.has_value(), "levelFlipSectors: a unadjoined");
        check(!b.surfaces[0].adjoin.has_value(), "levelFlipSectors: b unadjoined");
        check(a.surfaces[0].adjoinFlags == 0xAA, "levelFlipSectors: a.adjoinFlags");
        check(a.surfaces[0].nAdj == -1, "levelFlipSectors: a.nAdj");
        check(b.surfaces[0].adjoinFlags == 0xBB, "levelFlipSectors: b.adjoinFlags");
        check(b.surfaces[0].nAdj == -1, "levelFlipSectors: b.nAdj");
    }

    {
        sed::Level level{};
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        (void)level.newSector();
        (void)level.newSector();

        sed::Sector& a = level.sectors[0];
        a.flags       = 0x123;
        a.layer       = 7;
        a.sound       = "steam";
        a.soundVolume = 0.25;
        a.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        sed::Surface sa{};
        sa.vertices    = { 0, 1, 2 };
        sa.texVertices = {
            { -1.25f, 2.5f,
                sed::ColorF{ -0.0f, 0.25f, 0.5f, 0.75f } },
            { 3.75f, -0.625f,
                sed::ColorF{ 0.125f, 0.875f, -0.0f, 1.0f } },
            { -4.5f, 7.25f,
                sed::ColorF{ 0.9f, 0.8f, 0.7f, 0.6f } },
        };
        sa.uScale      = -0.375f;
        sa.vScale      = 2.625f;
        sa.adjoin      = sed::SurfaceRef{ 1, 0 };
        sa.nAdj        = 42;
        a.surfaces.push_back(sa);

        sed::Sector& b = level.sectors[1];
        b.vertices = {
            { 10.0, 0.0, 0.0, 0 },
            { 11.0, 0.0, 0.0, 0 },
            { 10.0, 1.0, 0.0, 0 },
        };

        sed::Surface sb = sa;
        sb.adjoin = sed::SurfaceRef{ 0, 0 };
        b.surfaces.push_back(sb);

        sed::level::levelRecalcSector(level, 0);
        const sed::Sector sourceTextureBaseline = level.sectors[0];

        const std::int32_t idx = sed::level::levelDuplicateSector(
            level,
            0,
            0.0,
            0.0,
            0.0,
            sed::TVector{ 1.0, 0.0, 0.0 },
            sed::TVector{ 0.0, 1.0, 0.0 },
            sed::TVector{ 0.0, 0.0, 1.0 },
            10.0,
            20.0,
            30.0);

        check(idx == 2, "levelDuplicateSector: index");

        const sed::Sector& c = level.sectors[2];
        check(c.flags == 0x123, "levelDuplicateSector: flags");
        check(c.layer == 7, "levelDuplicateSector: layer");
        check(c.sound == "steam", "levelDuplicateSector: sound");
        check(close(c.soundVolume, 0.25), "levelDuplicateSector: soundVolume");

        check(c.vertices.size() == 3, "levelDuplicateSector: vertex count");
        check(close(c.vertices[0].x, 10.0), "levelDuplicateSector: v0.x");
        check(close(c.vertices[0].y, 20.0), "levelDuplicateSector: v0.y");
        check(close(c.vertices[0].z, 30.0), "levelDuplicateSector: v0.z");

        check(c.surfaces.size() == 1, "levelDuplicateSector: surface count");
        check(c.surfaces[0].vertices == level.sectors[0].surfaces[0].vertices, "levelDuplicateSector: vertices");
        check(level.uvCoordinateSpace == sed::UvCoordinateSpace::Normalized
                && sameSectorTextureState(c, sourceTextureBaseline),
            "levelDuplicateSector: normalized signed multi-tile UVs, colors, scales, and Level tag remain exact");

        check(!c.surfaces[0].adjoin.has_value(), "levelDuplicateSector: unadjoined");
        check(c.surfaces[0].nAdj == -1, "levelDuplicateSector: nAdj");

        check(closeVec(c.surfaces[0].normal, sed::TVector::up, kRotationEpsilon), "levelDuplicateSector: normal");
    }

    {
        sed::Level level{};
        (void)level.newSector();

        sed::Sector& source = level.sectors[0];
        source.vertices = {
            { 10.0, 20.0, 30.0, 0 },
            { 12.0, 20.0, 30.0, 0 },
            { 10.0, 23.0, 30.0, 0 },
        };

        sed::Surface surface{};
        surface.vertices = { 0, 1, 2 };
        surface.texVertices = {
            { 0.0f, 0.0f, sed::ColorWhite },
            { 1.0f, 0.0f, sed::ColorWhite },
            { 0.0f, 1.0f, sed::ColorWhite },
        };
        source.surfaces.push_back(surface);

        const std::int32_t idx = sed::level::levelDuplicateSector(
            level,
            0,
            10.0,
            20.0,
            30.0,
            sed::TVector{ 0.0, 1.0, 0.0 },
            sed::TVector{ -1.0, 0.0, 0.0 },
            sed::TVector{ 0.0, 0.0, 1.0 },
            5.0,
            -2.0,
            4.0);

        check(idx == 1, "levelDuplicateSector.basis: index");

        const sed::Sector& duplicate = level.sectors[1];
        check(duplicate.vertices.size() == 3, "levelDuplicateSector.basis: vertex count");
        check(close(duplicate.vertices[0].x, 15.0), "levelDuplicateSector.basis: v0.x");
        check(close(duplicate.vertices[0].y, 18.0), "levelDuplicateSector.basis: v0.y");
        check(close(duplicate.vertices[0].z, 34.0), "levelDuplicateSector.basis: v0.z");
        check(close(duplicate.vertices[1].x, 15.0), "levelDuplicateSector.basis: v1.x");
        check(close(duplicate.vertices[1].y, 20.0), "levelDuplicateSector.basis: v1.y");
        check(close(duplicate.vertices[2].x, 12.0), "levelDuplicateSector.basis: v2.x");
        check(close(duplicate.vertices[2].y, 18.0), "levelDuplicateSector.basis: v2.y");
        check(closeVec(duplicate.surfaces[0].normal, sed::TVector::up, kRotationEpsilon), "levelDuplicateSector.basis: normal");
    }

    {
        sed::Level level{};
        level.clear();

        double x = 1.0;
        double y = 2.0;
        double z = 3.0;

        sed::level::levelOffsetByNode(level, 0, x, y, z);
        check(close(x, 1.0), "levelOffsetByNode.invalid: x");
        check(close(y, 2.0), "levelOffsetByNode.invalid: y");
        check(close(z, 3.0), "levelOffsetByNode.invalid: z");

        sed::level::levelUnoffsetByNode(level, 0, x, y, z);
        check(close(x, 1.0), "levelUnoffsetByNode.invalid: x");
        check(close(y, 2.0), "levelUnoffsetByNode.invalid: y");
        check(close(z, 3.0), "levelUnoffsetByNode.invalid: z");
    }

    {
        sed::Level level{};
        level.clear();

        sed::HNode& root = level.new3doNode();
        root.parent        = -1;
        root.position      = sed::TVector{ 1.0, 2.0, 3.0 };
        root.rotation      = sed::TVector{ 0.0, 0.0, 0.0 };
        root.pivot         = sed::TVector{ 0.0, 0.0, 0.0 };
        root.mesh          = 0;
        root.type          = 0x1;
        root.name          = "root";

        sed::HNode& child = level.new3doNode();
        child.parent        = 0;
        child.position      = sed::TVector{ 4.0, 5.0, 6.0 };
        child.rotation      = sed::TVector{ 0.0, 0.0, 0.0 };
        child.pivot         = sed::TVector{ 0.0, 0.0, 0.0 };
        child.mesh          = 1;
        child.type          = 0x2;
        child.name          = "child";

        double x = 7.0;
        double y = 8.0;
        double z = 9.0;

        sed::level::levelOffsetByNode(level, 1, x, y, z);
        check(close(x, 7.0 + 4.0 + 1.0), "levelOffsetByNode.translate: x");
        check(close(y, 8.0 + 5.0 + 2.0), "levelOffsetByNode.translate: y");
        check(close(z, 9.0 + 6.0 + 3.0), "levelOffsetByNode.translate: z");

        sed::level::levelUnoffsetByNode(level, 1, x, y, z);
        check(close(x, 7.0, kGeometryEpsilon), "levelUnoffsetByNode.translate: x");
        check(close(y, 8.0, kGeometryEpsilon), "levelUnoffsetByNode.translate: y");
        check(close(z, 9.0, kGeometryEpsilon), "levelUnoffsetByNode.translate: z");
    }

    {
        sed::Level level{};
        level.clear();

        sed::HNode& root = level.new3doNode();
        root.parent        = -1;
        root.position      = sed::TVector{ 10.0, -2.0, 1.0 };
        root.rotation      = sed::TVector{ 0.0, 90.0, 0.0 };
        root.pivot         = sed::TVector{ 0.25, -0.5, 0.75 };
        root.mesh          = 0;
        root.type          = 0x10;
        root.name          = "root";

        sed::HNode& child = level.new3doNode();
        child.parent        = 0;
        child.position      = sed::TVector{ -3.0, 4.0, 5.0 };
        child.rotation      = sed::TVector{ 15.0, 25.0, -30.0 };
        child.pivot         = sed::TVector{ 0.0, 1.0, 2.0 };
        child.mesh          = 1;
        child.type          = 0x20;
        child.name          = "child";

        const double x0 = 1.25;
        const double y0 = -0.5;
        const double z0 = 2.0;

        double x = x0;
        double y = y0;
        double z = z0;

        sed::level::levelOffsetByNode(level, 1, x, y, z);
        sed::level::levelUnoffsetByNode(level, 1, x, y, z);

        check(close(x, x0, kRotationEpsilon), "levelUnoffsetByNode.roundtrip: x");
        check(close(y, y0, kRotationEpsilon), "levelUnoffsetByNode.roundtrip: y");
        check(close(z, z0, kRotationEpsilon), "levelUnoffsetByNode.roundtrip: z");
    }

    {
        // Include a self-parent and a two-node cycle as well as out-of-range ancestry.
        for ( const std::int32_t invalidParent : {-2, 2, std::numeric_limits<std::int32_t>::max(), 0, 1} )
        {
            sed::Level malformed{};
            malformed.h3doNodes.resize(2);
            malformed.h3doNodes[0].parent = invalidParent;
            malformed.h3doNodes[0].position = sed::TVector{10.0, 20.0, 30.0};
            malformed.h3doNodes[1].parent = 0;
            malformed.h3doNodes[1].position = sed::TVector{1.0, 2.0, 3.0};

            for ( const auto transform : {sed::level::levelOffsetByNode, sed::level::levelUnoffsetByNode} )
            {
                double x = 4.0;
                double y = -5.0;
                double z = 6.0;
                transform(malformed, 1, x, y, z);
                check(x == 4.0 && y == -5.0 && z == 6.0,
                      "node transform rejects malformed ancestry without publishing a partial transform");
            }
        }
    }

    {
        const std::array nonFiniteValues{
            std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
            -std::numeric_limits<double>::infinity(),
        };
        for ( const auto transform : {sed::level::levelOffsetByNode, sed::level::levelUnoffsetByNode} )
        {
            for ( const double invalid : nonFiniteValues )
            {
                for ( const bool invalidateRotation : {false, true} )
                {
                    sed::Level level{};
                    level.h3doNodes.resize(2);
                    level.h3doNodes[0].parent = -1;
                    level.h3doNodes[1].parent = 0;
                    level.h3doNodes[1].position = sed::TVector{1.0, 2.0, 3.0};
                    if ( invalidateRotation )
                    {
                        level.h3doNodes[0].rotation.y = invalid;
                    }
                    else
                    {
                        level.h3doNodes[0].position.z = invalid;
                    }
                    double x = 4.0;
                    double y = -5.0;
                    double z = 6.0;
                    transform(level, 1, x, y, z);
                    check(x == 4.0 && y == -5.0 && z == 6.0,
                          "node transform rejects a non-finite ancestor without changing any coordinate");
                }

                sed::Level root{};
                root.h3doNodes.resize(1);
                root.h3doNodes[0].parent = -1;
                root.h3doNodes[0].position = sed::TVector{1.0, 2.0, 3.0};
                double x = invalid;
                double y = -5.0;
                double z = 6.0;
                transform(root, 0, x, y, z);
                check(std::bit_cast<std::uint64_t>(x) == std::bit_cast<std::uint64_t>(invalid) &&
                          y == -5.0 && z == 6.0,
                      "node transform preserves every input bit when the input point is non-finite");
            }

            sed::Level deep{};
            deep.h3doNodes.resize(128);
            for ( std::size_t index = 0; index < deep.h3doNodes.size(); ++index )
            {
                deep.h3doNodes[index].parent = static_cast<std::int32_t>(index) - 1;
                deep.h3doNodes[index].position = sed::TVector{1.0, 2.0, -3.0};
            }
            double x = 0.0;
            double y = 0.0;
            double z = 0.0;
            const double direction = transform == sed::level::levelOffsetByNode ? 1.0 : -1.0;
            transform(deep, 127, x, y, z);
            check(x == 128.0 * direction && y == 256.0 * direction && z == -384.0 * direction,
                  "node transform accepts a valid ancestry using every node including the last root visit");

            for ( const std::int32_t invalidIndex : {-1, 128} )
            {
                x = 4.0;
                y = -5.0;
                z = 6.0;
                transform(deep, invalidIndex, x, y, z);
                check(x == 4.0 && y == -5.0 && z == 6.0,
                      "node transform rejects an invalid starting index without mutation");
            }

            sed::Level overflow{};
            overflow.h3doNodes.resize(2);
            overflow.h3doNodes[0].parent = -1;
            overflow.h3doNodes[1].parent = 0;
            overflow.h3doNodes[0].position.x = std::numeric_limits<double>::max();
            overflow.h3doNodes[1].position.x = std::numeric_limits<double>::max();
            x = 4.0;
            y = -5.0;
            z = 6.0;
            transform(overflow, 1, x, y, z);
            check(x == 4.0 && y == -5.0 && z == 6.0,
                  "finite node translations that overflow do not publish any partial output");
        }
    }

    {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double infinity = std::numeric_limits<double>::infinity();
        const std::array<std::int32_t, 1> sectors{0};
        const std::array<sed::SurfaceRef, 1> surfaces{sed::SurfaceRef{0, 0}};

        sed::Level translated = makeAuthoredUvAdjoinedTransformLevel();
        const sed::Vertex translatedBefore = translated.sectors[0].vertices[0];
        sed::level::levelTranslateSectors(translated, sectors, nan, 0.0, 0.0);
        check(translated.sectors[0].vertices[0].x == translatedBefore.x &&
                  translated.sectors[0].vertices[0].y == translatedBefore.y &&
                  translated.sectors[0].vertices[0].z == translatedBefore.z,
              "transform validation rejects a non-finite translation before mutating geometry");

        sed::Level rotated = makeAuthoredUvAdjoinedTransformLevel();
        const sed::Vertex rotatedBefore = rotated.sectors[0].vertices[0];
        sed::level::levelRotateSurfaces(
            rotated, surfaces, sed::TVector::up, 0.0, 0.0, 0.0, infinity);
        check(rotated.sectors[0].vertices[0].x == rotatedBefore.x &&
                  rotated.sectors[0].vertices[0].y == rotatedBefore.y &&
                  rotated.sectors[0].vertices[0].z == rotatedBefore.z,
              "transform validation rejects a non-finite rotation before mutating geometry");

        sed::Level scaled = makeAuthoredUvAdjoinedTransformLevel();
        const sed::Vertex scaledBefore = scaled.sectors[0].vertices[0];
        sed::ScaleData nonFiniteScale{};
        nonFiniteScale.sfactor = nan;
        nonFiniteScale.how = sed::ScaleHow::xyz;
        sed::level::levelScaleSectors(scaled, sectors, nonFiniteScale, true);
        check(scaled.sectors[0].vertices[0].x == scaledBefore.x &&
                  scaled.sectors[0].vertices[0].y == scaledBefore.y &&
                  scaled.sectors[0].vertices[0].z == scaledBefore.z &&
                  sameSectorTextureState(scaled.sectors[0],
                                         makeAuthoredUvAdjoinedTransformLevel().sectors[0]),
              "transform validation rejects a non-finite Scale and leaves authored UV state unchanged");

        sed::Level unknownScale = makeAuthoredUvAdjoinedTransformLevel();
        const std::vector<std::byte> unknownScaleBefore = sed::assets::saveToSedBytes(unknownScale);
        sed::ScaleData invalidScale{};
        invalidScale.sfactor = 2.0;
        invalidScale.how = static_cast<sed::ScaleHow>(std::numeric_limits<std::int32_t>::max());
        sed::level::levelScaleSectors(unknownScale, sectors, invalidScale, true);
        check(sed::assets::saveToSedBytes(unknownScale) == unknownScaleBefore,
              "transform validation rejects an unknown Scale mode without changing geometry, marks, normals, or texture density");

        sed::Level zeroVectorScale = makeAuthoredUvAdjoinedTransformLevel();
        const std::vector<std::byte> zeroVectorScaleBefore =
            sed::assets::saveToSedBytes(zeroVectorScale);
        sed::ScaleData degenerateScale{};
        degenerateScale.sfactor = 2.0;
        degenerateScale.how = sed::ScaleHow::vec;
        sed::level::levelScaleSectors(
            zeroVectorScale, sectors, degenerateScale, true);
        check(sed::assets::saveToSedBytes(zeroVectorScale) ==
                  zeroVectorScaleBefore,
              "transform validation rejects a zero vector-axis Scale without changing texture density");

        sed::Level flipped = makeAuthoredUvAdjoinedTransformLevel();
        const std::vector<std::byte> flippedBefore = sed::assets::saveToSedBytes(flipped);
        sed::level::levelFlipSectors(
            flipped, sectors, 0.0, 0.0, 0.0,
            static_cast<sed::level::Axis>(std::numeric_limits<std::int32_t>::max()));
        check(sed::assets::saveToSedBytes(flipped) == flippedBefore,
              "transform validation rejects an unknown flip axis before disconnecting portals or reversing winding");

        sed::Level duplicated = makeAuthoredUvAdjoinedTransformLevel();
        const std::vector<std::byte> duplicatedBefore = sed::assets::saveToSedBytes(duplicated);
        const std::int32_t duplicateResult = sed::level::levelDuplicateSector(
            duplicated, 0, nan, 0.0, 0.0, sed::TVector::right,
            sed::TVector::forward, sed::TVector::up, 0.0, 0.0, 0.0);
        check(duplicateResult == -1 &&
                  sed::assets::saveToSedBytes(duplicated) == duplicatedBefore,
              "transform validation rejects a non-finite duplicate basis before appending a Sector");
    }

    {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double infinity = std::numeric_limits<double>::infinity();
        const auto invalidAxis =
            static_cast<sed::level::Axis>(std::numeric_limits<std::int32_t>::max());
        const std::array<std::int32_t, 1> sectors{0};
        const std::array<sed::SurfaceRef, 1> surfaces{sed::SurfaceRef{0, 0}};
        const std::array<sed::level::EdgeRef, 1> edges{sed::level::EdgeRef{0, 0, 0}};
        const std::array<sed::level::VertexRef, 1> vertices{sed::level::VertexRef{0, 0}};
        const std::array<std::int32_t, 1> things{0};
        const std::array<sed::level::FrameRef, 1> frames{sed::level::FrameRef{0, 0}};
        const std::array<std::int32_t, 1> lights{0};

        sed::Level level = makeAuthoredUvAdjoinedTransformLevel();
        level.things.resize(1);
        level.things[0].x = 0.25;
        level.things[0].y = -0.5;
        level.things[0].z = 0.75;
        level.things[0].values.push_back(
            makeFrameValue(1.0, 2.0, 3.0, 4.0, 5.0, 6.0));
        level.lights.resize(1);
        level.lights[0].position = sed::TVector{1.0, -2.0, 3.0};
        level.sectors[0].surfaces[0].normal =
            sed::TVector{ 0.125, -0.25, 0.5 };
        const std::vector<std::byte> before = sed::assets::saveToSedBytes(level);

        sed::level::levelTranslateSurfaces(level, surfaces, 0.0, nan, 0.0);
        sed::level::levelTranslateEdges(level, edges, 0.0, 0.0, infinity);
        sed::level::levelTranslateVertices(level, vertices, nan, 0.0, 0.0);
        sed::level::levelTranslateThings(level, things, 0.0, infinity, 0.0, true);
        sed::level::levelTranslateFrames(level, frames, 0.0, 0.0, nan);
        sed::level::levelTranslateLights(level, lights, infinity, 0.0, 0.0);

        const sed::TVector invalidRotationAxis{nan, 0.0, 0.0};
        sed::level::levelRotateSectors(
            level, sectors, invalidRotationAxis, 0.0, 0.0, 0.0, 15.0);
        sed::level::levelRotateSectors(
            level, sectors, sed::TVector::zero, 0.0, 0.0, 0.0, 15.0);
        sed::level::levelRotateEdges(
            level, edges, sed::TVector::up, 0.0, nan, 0.0, 15.0);
        sed::level::levelRotateThings(
            level, things, sed::TVector::up, 0.0, 0.0, 0.0, infinity, true);
        sed::level::levelRotateLights(
            level, lights, sed::TVector::up, infinity, 0.0, 0.0, 15.0);
        sed::level::levelRotateFrames(
            level, frames, sed::TVector::up, 0.0, 0.0, 0.0, nan, true);

        sed::ScaleData invalidScale{};
        invalidScale.sfactor = nan;
        sed::level::levelScaleSurfaces(level, surfaces, invalidScale, true);
        sed::level::levelScaleThings(level, things, invalidScale);
        sed::level::levelScaleLights(level, lights, invalidScale);
        sed::level::levelScaleFrames(level, frames, invalidScale);

        sed::level::levelFlipThings(level, things, 0.0, 0.0, 0.0, invalidAxis);
        sed::level::levelFlipThingsOverPlane(
            level, things, sed::TVector::zero, 0.0, 0.0, 0.0);
        sed::level::levelFlipLightsOverPlane(
            level, lights, sed::TVector{infinity, 0.0, 0.0}, 0.0, 0.0, 0.0);
        sed::level::levelFlipFramesOverPlane(
            level, frames, sed::TVector::zero, 0.0, 0.0, 0.0);
        sed::level::levelFlipSectorsOverPlane(
            level, sectors, sed::TVector::zero, 0.0, 0.0, 0.0);

        double pitch = 10.0;
        double yaw = 20.0;
        double roll = 30.0;
        sed::level::rotatePYR(pitch, yaw, roll, invalidRotationAxis, 15.0);

        check(sed::assets::saveToSedBytes(level) == before &&
                  pitch == 10.0 && yaw == 20.0 && roll == 30.0,
              "all Sector, Surface, Edge, Vertex, Thing, Frame, and Light transform families reject malformed or degenerate inputs without partial mutation");
    }
}

} // namespace sed::tests
