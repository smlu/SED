#include "test_common.hpp"

#include "sed/assets/asc.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/level/uv.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <vector>

namespace sed::tests
{

void runAscTests()
{
    {
        const char* asc = R"ASC(
        # comment
        Tri-mesh, vertices: 3 faces: 1
        Vertex list:
        Vertex 0: x 0 y 0 z 0
        Vertex 1: x 1 y 0 z 0
        Vertex 2: x 0 y 1 z 0
        Face list:
        Face 0: a 0 b 1 c 2
        )ASC";

        std::istringstream iss(asc);
        const sed::Level level = sed::assets::loadFromASC(iss);

        check(level.kind == sed::ProjectType::JKDF2, "ASC: kind");
        check(level.uvCoordinateSpace == sed::UvCoordinateSpace::TexturePixels,
            "ASC: resource-independent import publishes generated UVs as pixel space");
        check(level.header.version == 1, "ASC: header.version");
        check(close(level.header.gravity, 4.0), "ASC: header.gravity");

        check(level.sectors.size() == 1, "ASC: sectors.size");
        check(level.things.size() == 1, "ASC: things.size");

        if (level.sectors.size() == 1)
        {
            const sed::Sector& sector = level.sectors[0];
            check(sector.vertices.size() == 3, "ASC: sector.vertices.size");
            check(sector.surfaces.size() == 1, "ASC: sector.surfaces.size");

            if (sector.vertices.size() == 3)
            {
                check(close(sector.vertices[0].x, 0.0), "ASC: v0.x");
                check(close(sector.vertices[0].y, 0.0), "ASC: v0.y");
                check(close(sector.vertices[0].z, 0.0), "ASC: v0.z");

                check(close(sector.vertices[1].x, 1.0), "ASC: v1.x");
                check(close(sector.vertices[1].y, 0.0), "ASC: v1.y");
                check(close(sector.vertices[1].z, 0.0), "ASC: v1.z");

                check(close(sector.vertices[2].x, 0.0), "ASC: v2.x");
                check(close(sector.vertices[2].y, 1.0), "ASC: v2.y");
                check(close(sector.vertices[2].z, 0.0), "ASC: v2.z");
            }

            if (sector.surfaces.size() == 1)
            {
                const sed::Surface& surf = sector.surfaces[0];
                check(surf.material == "dflt.mat", "ASC: surface.material");
                check(surf.vertices.size() == 3, "ASC: surface.vertices.size");
                check(surf.texVertices.size() == 3, "ASC: surface.texVertices.size");
                check(closeVec(surf.normal, sed::TVector{ 0.0, 0.0, 1.0 }, kRotationEpsilon), "ASC: surface.normal");
                check((surf.surfaceFlags & sed::SF_Floor) != 0, "ASC: surfaceFlags floor");
                check((surf.surfaceFlags & sed::SF_Collision) != 0, "ASC: surfaceFlags collision");
            }
        }
    }

    {
        const char* asc = R"ASC(
        Tri-mesh, vertices: 3 faces: 1
        Vertex list:
        Vertex 0: x -2 y -1 z 0
        Vertex 1: x 3 y -1 z 0
        Vertex 2: x -2 y 4 z 0
        Face list:
        Face 0: a 0 b 1 c 2
        )ASC";

        std::istringstream source(asc);
        sed::Level normalized = sed::assets::loadFromASC(source);
        const std::vector<sed::TexVertex> pixelUvs =
            normalized.sectors[0].surfaces[0].texVertices;
        normalized.sectors[0].surfaces[0].uScale = -2.5f;
        normalized.sectors[0].surfaces[0].vScale = 3.25f;
        const sed::level::LevelUvConversionResult converted =
            sed::level::convertLevelUvCoordinateSpace(
                normalized,
                sed::UvCoordinateSpace::Normalized,
                [](const sed::Surface&)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    return sed::level::UvTextureDimensions{ 128.0, 64.0 };
                });
        bool coordinatesConverted = converted
            && normalized.uvCoordinateSpace
                == sed::UvCoordinateSpace::Normalized
            && normalized.sectors[0].surfaces[0].texVertices.size()
                == pixelUvs.size();
        for (std::size_t index = 0;
             coordinatesConverted && index < pixelUvs.size();
             ++index)
        {
            coordinatesConverted =
                close(normalized.sectors[0].surfaces[0].texVertices[index].u,
                    pixelUvs[index].u / 128.0)
                && close(normalized.sectors[0].surfaces[0].texVertices[index].v,
                    pixelUvs[index].v / 64.0);
        }
        check(coordinatesConverted
                && close(
                    normalized.sectors[0].surfaces[0].uScale,
                    -2.5 / 128.0)
                && close(
                    normalized.sectors[0].surfaces[0].vScale,
                    3.25 / 64.0),
            "ASC: anisotropic material dimensions normalize signed and multitile generated UVs and per-axis texture scales");

        std::istringstream unresolvedSource(asc);
        sed::Level unresolved = sed::assets::loadFromASC(unresolvedSource);
        const std::vector<sed::TexVertex> unresolvedBaseline =
            unresolved.sectors[0].surfaces[0].texVertices;
        const sed::level::LevelUvConversionResult unresolvedResult =
            sed::level::convertLevelUvCoordinateSpace(
                unresolved,
                sed::UvCoordinateSpace::Normalized,
                [](const sed::Surface&)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    return std::nullopt;
                });
        bool unresolvedUnchanged = unresolved.sectors[0].surfaces[0]
                .texVertices.size() == unresolvedBaseline.size();
        for (std::size_t index = 0;
             unresolvedUnchanged && index < unresolvedBaseline.size();
             ++index)
        {
            const sed::TexVertex& actual =
                unresolved.sectors[0].surfaces[0].texVertices[index];
            const sed::TexVertex& expected = unresolvedBaseline[index];
            unresolvedUnchanged = actual.u == expected.u
                && actual.v == expected.v && actual.color == expected.color;
        }
        check(!unresolvedResult
                && unresolved.uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels
                && unresolvedUnchanged,
            "ASC: missing material dimensions leave every imported pixel UV unchanged atomically");

        const std::filesystem::path root =
            std::filesystem::temp_directory_path()
            / ("sed_asc_uv_"
                + std::to_string(
                    std::chrono::steady_clock::now()
                        .time_since_epoch().count()));
        const std::filesystem::path matPath = root / "mat" / "dflt.mat";
        std::error_code ec{};
        std::filesystem::create_directories(matPath.parent_path(), ec);
        {
            const std::vector<std::byte> bytes =
                makeIndexedMatFixture(128, 64);
            std::ofstream output(matPath, std::ios::binary | std::ios::trunc);
            output.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }

        sed::assets::ResourceLocator resources{};
        resources.addRoot(root);
        sed::assets::AscLoadOptions options{};
        options.resources = &resources;
        std::vector<std::string> diagnostics{};
        options.onDiagnostic = [&diagnostics](
            const sed::DiagnosticSeverity,
            const std::string_view message)
        {
            diagnostics.emplace_back(message);
        };
        std::istringstream resourceSource(asc);
        const sed::Level resourceNormalized =
            sed::assets::loadFromASC(resourceSource, options);
        check(!ec && diagnostics.empty()
                && resourceNormalized.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && close(
                    resourceNormalized.sectors[0].surfaces[0]
                        .texVertices[1].u,
                    pixelUvs[1].u / 128.0)
                && close(
                    resourceNormalized.sectors[0].surfaces[0]
                        .texVertices[2].v,
                    pixelUvs[2].v / 64.0)
                && close(
                    resourceNormalized.sectors[0].surfaces[0].uScale,
                    1.0 / 128.0)
                && close(
                    resourceNormalized.sectors[0].surfaces[0].vScale,
                    1.0 / 64.0),
            "ASC: resource-backed import publishes normalized UVs and scales through actual anisotropic MAT dimensions");

        sed::assets::ResourceLocator missingResources{};
        options.resources = &missingResources;
        diagnostics.clear();
        std::istringstream missingResourceSource(asc);
        const sed::Level missingResourceLevel =
            sed::assets::loadFromASC(missingResourceSource, options);
        check(missingResourceLevel.uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels
                && diagnostics.size() == 1u
                && diagnostics.front().find("sector 0, surface 0")
                    != std::string::npos,
            "ASC: unresolved resource-backed normalization stays pixel tagged and emits one precise diagnostic");

        {
            const std::vector<std::byte> bytes =
                makeIndexedMatFixture(128, 0);
            std::ofstream output(
                matPath,
                std::ios::binary | std::ios::trunc);
            output.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }
        options.resources = &resources;
        diagnostics.clear();
        std::istringstream dimensionlessResourceSource(asc);
        const sed::Level dimensionlessResourceLevel =
            sed::assets::loadFromASC(
                dimensionlessResourceSource, options);
        const sed::Surface& dimensionlessSurface =
            dimensionlessResourceLevel.sectors[0].surfaces[0];
        bool dimensionlessUnchanged =
            dimensionlessSurface.texVertices.size() == pixelUvs.size();
        for (std::size_t index = 0;
             dimensionlessUnchanged && index < pixelUvs.size();
             ++index)
        {
            dimensionlessUnchanged =
                dimensionlessSurface.texVertices[index].u
                    == pixelUvs[index].u
                && dimensionlessSurface.texVertices[index].v
                    == pixelUvs[index].v;
        }
        check(dimensionlessResourceLevel.uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels
                && dimensionlessUnchanged
                && dimensionlessSurface.uScale == 1.0f
                && dimensionlessSurface.vScale == 1.0f
                && diagnostics.size() == 1u
                && diagnostics.front().find("sector 0, surface 0")
                    != std::string::npos,
            "ASC: a dimensionless MAT leaves generated coordinates and scales in pixel space atomically");

        std::filesystem::remove_all(root, ec);
    }

    {
        const char* asc = R"ASC(
        tri-mesh, vertices: 3 faces: 1
        vertex list:
        vertex 0: x 0 y 0 z 0
        vertex 1: x 1 y 0 z 0
        vertex 2: x 0 y 1 z 0
        face list:
        face 0: a 0 b 1 c 2

        tri-mesh, vertices: 3 faces: 1
        vertex list:
        vertex 0: x 0 y 0 z 1
        vertex 1: x 1 y 0 z 1
        vertex 2: x 0 y 1 z 1
        face list:
        face 0: a 0 b 1 c 2
        )ASC";

        std::istringstream iss(asc);
        const sed::Level level = sed::assets::loadFromASC(iss);

        check(level.sectors.size() == 2, "ASC.multimesh: sectors.size");
        if (level.sectors.size() == 2)
        {
            check(level.sectors[0].vertices.size() == 3, "ASC.multimesh: sector0.vertices.size");
            check(level.sectors[1].vertices.size() == 3, "ASC.multimesh: sector1.vertices.size");
        }
    }
}

} // namespace sed::tests
