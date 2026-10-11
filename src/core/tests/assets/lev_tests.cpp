#include "test_common.hpp"

#include "sed/assets/lev.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/level/uv.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace sed::tests
{

namespace
{

struct ZRange
{
    double min{};
    double max{};
};

ZRange zRange(const sed::Sector& sector, const sed::Surface& surface)
{
    ZRange out{};
    out.min = (std::numeric_limits<double>::max)();
    out.max = (std::numeric_limits<double>::lowest)();

    for (const std::int32_t vi : surface.vertices)
    {
        if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
        {
            continue;
        }

        const double z = sector.vertices[static_cast<std::size_t>(vi)].z;
        out.min = std::min(out.min, z);
        out.max = std::max(out.max, z);
    }

    if (out.min == (std::numeric_limits<double>::max)())
    {
        out.min = out.max = 0.0;
    }

    return out;
}

} // namespace

void runLevTests()
{
    {
        const char* levText = R"LEV(
leV 1
LevelName test
Textures 1
Texture: stone.bm
NumSectors 2
sEcToR 0
Ambient 31
Floor Altitude 40
Ceiling Altitude 0
Floor Texture 0 0 0
Ceiling Texture 0 0 0
Flags 0
Layer 0
Vertices 4
x: 0 z: 0
x: 40 z: 0
x: 40 z: 40
x: 0 z: 40
Walls 4
Wall Left: 0 Right: 3 Mid: 0 0 0 Top: -1 0 0 Bot: -1 0 0 Sign: -1 0 0 Adjoin: -1 Mirror: -1 Flags: 0 0 0 Light: 0
Wall Left: 3 Right: 2 Mid: 0 0 0 Top: -1 0 0 Bot: -1 0 0 Sign: -1 0 0 Adjoin: -1 Mirror: -1 Flags: 0 0 0 Light: 0
Wall Left: 2 Right: 1 Mid: 0 0 0 Top: -1 0 0 Bot: -1 0 0 Sign: -1 0 0 Adjoin: 1 Mirror: 0 Flags: 0 0 0 Light: 0
Wall Left: 1 Right: 0 Mid: 0 0 0 Top: -1 0 0 Bot: -1 0 0 Sign: -1 0 0 Adjoin: -1 Mirror: -1 Flags: 0 0 0 Light: 0
sEcToR 1
Ambient 31
Floor Altitude 40
Ceiling Altitude 0
Floor Texture 0 0 0
Ceiling Texture 0 0 0
Flags 0
Layer 0
Vertices 4
x: 40 z: 0
x: 80 z: 0
x: 80 z: 40
x: 40 z: 40
Walls 4
Wall Left: 0 Right: 3 Mid: 0 0 0 Top: -1 0 0 Bot: -1 0 0 Sign: -1 0 0 Adjoin: 0 Mirror: 2 Flags: 0 0 0 Light: 0
Wall Left: 3 Right: 2 Mid: 0 0 0 Top: -1 0 0 Bot: -1 0 0 Sign: -1 0 0 Adjoin: -1 Mirror: -1 Flags: 0 0 0 Light: 0
Wall Left: 2 Right: 1 Mid: 0 0 0 Top: -1 0 0 Bot: -1 0 0 Sign: -1 0 0 Adjoin: -1 Mirror: -1 Flags: 0 0 0 Light: 0
Wall Left: 1 Right: 0 Mid: 0 0 0 Top: -1 0 0 Bot: -1 0 0 Sign: -1 0 0 Adjoin: -1 Mirror: -1 Flags: 0 0 0 Light: 0
)LEV";

        sed::assets::LevLoadOptions opts{};
        opts.dfToJkFactor = 40.0;
        opts.textureMode  = sed::assets::LevTextureMode::Keep;
        opts.loadObjects  = false;

        std::istringstream iss(levText);
        const sed::Level level = sed::assets::loadFromLEV(iss, opts);

        check(level.uvCoordinateSpace == sed::UvCoordinateSpace::TexturePixels,
            "LEV: resource-independent import publishes generated UVs as pixel space");
        check(level.sectors.size() == 2, "LEV.mixedcase: sectors.size");
        if (level.sectors.size() == 2)
        {
            const sed::Sector& sec0 = level.sectors[0];
            const sed::Sector& sec1 = level.sectors[1];

            const auto sec0Adj = std::find_if(sec0.surfaces.begin(), sec0.surfaces.end(), [](const sed::Surface& s)
                {
                    return s.adjoin.has_value();
                });
            const auto sec1Adj = std::find_if(sec1.surfaces.begin(), sec1.surfaces.end(), [](const sed::Surface& s)
                {
                    return s.adjoin.has_value();
                });

            check(sec0Adj != sec0.surfaces.end(), "LEV.mixedcase: sec0 has adjoin surface");
            check(sec1Adj != sec1.surfaces.end(), "LEV.mixedcase: sec1 has adjoin surface");
        }

        sed::Level normalized = level;
        const std::vector<sed::TexVertex> pixelUvs =
            normalized.sectors[0].surfaces[0].texVertices;
        normalized.sectors[0].surfaces[0].uScale = -1.75f;
        normalized.sectors[0].surfaces[0].vScale = 2.5f;
        const sed::level::LevelUvConversionResult converted =
            sed::level::convertLevelUvCoordinateSpace(
                normalized,
                sed::UvCoordinateSpace::Normalized,
                [](const sed::Surface&)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    return sed::level::UvTextureDimensions{ 80.0, 40.0 };
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
                    pixelUvs[index].u / 80.0)
                && close(normalized.sectors[0].surfaces[0].texVertices[index].v,
                    pixelUvs[index].v / 40.0);
        }
        check(coordinatesConverted
                && close(
                    normalized.sectors[0].surfaces[0].uScale,
                    -1.75 / 80.0)
                && close(
                    normalized.sectors[0].surfaces[0].vScale,
                    2.5 / 40.0),
            "LEV: anisotropic material dimensions normalize signed and multitile generated UVs and per-axis texture scales");

        sed::Level unresolved = level;
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
            "LEV: missing material dimensions leave every imported pixel UV unchanged atomically");

        const std::filesystem::path root =
            std::filesystem::temp_directory_path()
            / ("sed_lev_uv_"
                + std::to_string(
                    std::chrono::steady_clock::now()
                        .time_since_epoch().count()));
        const std::filesystem::path matPath = root / "mat" / "stone.mat";
        std::error_code ec{};
        std::filesystem::create_directories(matPath.parent_path(), ec);
        {
            const std::vector<std::byte> bytes =
                makeIndexedMatFixture(80, 40);
            std::ofstream output(matPath, std::ios::binary | std::ios::trunc);
            output.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }

        sed::assets::ResourceLocator resources{};
        resources.addRoot(root);
        sed::assets::LevLoadOptions resourceOptions = opts;
        resourceOptions.resources = &resources;
        std::vector<std::string> diagnostics{};
        resourceOptions.onDiagnostic = [&diagnostics](
            const sed::DiagnosticSeverity,
            const std::string_view message)
        {
            diagnostics.emplace_back(message);
        };
        std::istringstream resourceStream(levText);
        const sed::Level resourceNormalized = sed::assets::loadFromLEV(
            resourceStream,
            resourceOptions);
        check(!ec && diagnostics.empty()
                && resourceNormalized.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && close(
                    resourceNormalized.sectors[0].surfaces[0]
                        .texVertices[0].u,
                    level.sectors[0].surfaces[0].texVertices[0].u / 80.0)
                && close(
                    resourceNormalized.sectors[0].surfaces[0]
                        .texVertices[0].v,
                    level.sectors[0].surfaces[0].texVertices[0].v / 40.0)
                && close(
                    resourceNormalized.sectors[0].surfaces[0].uScale,
                    level.sectors[0].surfaces[0].uScale / 80.0)
                && close(
                    resourceNormalized.sectors[0].surfaces[0].vScale,
                    level.sectors[0].surfaces[0].vScale / 40.0),
            "LEV: resource-backed import publishes normalized UVs and scales through actual anisotropic MAT dimensions");

        sed::assets::ResourceLocator missingResources{};
        resourceOptions.resources = &missingResources;
        diagnostics.clear();
        std::istringstream missingResourceStream(levText);
        const sed::Level missingResourceLevel = sed::assets::loadFromLEV(
            missingResourceStream,
            resourceOptions);
        check(missingResourceLevel.uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels
                && diagnostics.size() == 1u
                && diagnostics.front().find("sector 0, surface 0")
                    != std::string::npos,
            "LEV: unresolved resource-backed normalization stays pixel tagged and emits one precise diagnostic");

        {
            const std::vector<std::byte> bytes =
                makeIndexedMatFixture(0, 40);
            std::ofstream output(
                matPath,
                std::ios::binary | std::ios::trunc);
            output.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }
        resourceOptions.resources = &resources;
        diagnostics.clear();
        std::istringstream dimensionlessResourceStream(levText);
        const sed::Level dimensionlessResourceLevel =
            sed::assets::loadFromLEV(
                dimensionlessResourceStream, resourceOptions);
        bool dimensionlessUnchanged =
            dimensionlessResourceLevel.sectors.size()
                == level.sectors.size();
        for (std::size_t sectorIndex = 0;
             dimensionlessUnchanged
                && sectorIndex < level.sectors.size();
             ++sectorIndex)
        {
            const sed::Sector& actualSector =
                dimensionlessResourceLevel.sectors[sectorIndex];
            const sed::Sector& expectedSector = level.sectors[sectorIndex];
            dimensionlessUnchanged = actualSector.surfaces.size()
                == expectedSector.surfaces.size();
            for (std::size_t surfaceIndex = 0;
                 dimensionlessUnchanged
                    && surfaceIndex < expectedSector.surfaces.size();
                 ++surfaceIndex)
            {
                const sed::Surface& actualSurface =
                    actualSector.surfaces[surfaceIndex];
                const sed::Surface& expectedSurface =
                    expectedSector.surfaces[surfaceIndex];
                dimensionlessUnchanged =
                    actualSurface.uScale == expectedSurface.uScale
                    && actualSurface.vScale == expectedSurface.vScale
                    && actualSurface.texVertices.size()
                        == expectedSurface.texVertices.size();
                for (std::size_t texVertexIndex = 0;
                     dimensionlessUnchanged
                        && texVertexIndex
                            < expectedSurface.texVertices.size();
                     ++texVertexIndex)
                {
                    const sed::TexVertex& actual =
                        actualSurface.texVertices[texVertexIndex];
                    const sed::TexVertex& expected =
                        expectedSurface.texVertices[texVertexIndex];
                    dimensionlessUnchanged = actual.u == expected.u
                        && actual.v == expected.v
                        && actual.color == expected.color;
                }
            }
        }
        check(dimensionlessResourceLevel.uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels
                && dimensionlessUnchanged
                && diagnostics.size() == 1u
                && diagnostics.front().find("sector 0, surface 0")
                    != std::string::npos,
            "LEV: a dimensionless MAT leaves all generated sectors in pixel space without partial conversion");

        std::filesystem::remove_all(root, ec);

        sed::assets::LevLoadOptions defaultTextureOpts{};
        defaultTextureOpts.dfToJkFactor = 20.0;
        defaultTextureOpts.textureMode = sed::assets::LevTextureMode::Default;
        defaultTextureOpts.loadObjects = false;
        std::istringstream defaultTextureStream(levText);
        const sed::Level defaultTextureLevel =
            sed::assets::loadFromLEV(defaultTextureStream, defaultTextureOpts);
        bool allDefaultMaterials = !defaultTextureLevel.sectors.empty();
        double maximumPlanarCoordinate = 0.0;
        for (const sed::Sector& importedSector : defaultTextureLevel.sectors)
        {
            for (const sed::Vertex& importedVertex : importedSector.vertices)
            {
                maximumPlanarCoordinate = std::max(
                    maximumPlanarCoordinate,
                    std::max(
                        std::abs(importedVertex.x),
                        std::abs(importedVertex.y)));
            }
            for (const sed::Surface& importedSurface : importedSector.surfaces)
            {
                allDefaultMaterials = allDefaultMaterials
                    && (importedSurface.material.empty()
                        || importedSurface.material == "dflt.mat");
            }
        }
        check(defaultTextureLevel.sectors.size() == 2
                && close(maximumPlanarCoordinate, 4.0)
                && close(defaultTextureLevel.ppunit, 160.0)
                && allDefaultMaterials,
            "LEV.options: scale changes imported geometry and pixels-per-unit while Default assigns DFLT.MAT");
    }

    {
        const char* levText = R"LEV(
LEV 1
LEVELNAME test
TEXTURES 1
TEXTURE: stone.bm
NUMSECTORS 2
SECTOR 0
AMBIENT 31
FLOOR ALTITUDE 40
CEILING ALTITUDE 0
FLOOR TEXTURE 0 0 0
CEILING TEXTURE 0 0 0
FLAGS 0
LAYER 0
VERTICES 4
X: 0 Z: 0
X: 40 Z: 0
X: 40 Z: 40
X: 0 Z: 40
WALLS 4
WALL LEFT: 0 RIGHT: 3 MID: 0 0 0 TOP: -1 0 0 BOT: -1 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0
WALL LEFT: 3 RIGHT: 2 MID: 0 0 0 TOP: -1 0 0 BOT: -1 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0
WALL LEFT: 2 RIGHT: 1 MID: 0 0 0 TOP: -1 0 0 BOT: -1 0 0 SIGN: -1 0 0 ADJOIN: 1 MIRROR: 0 FLAGS: 0 0 0 LIGHT: 0
WALL LEFT: 1 RIGHT: 0 MID: 0 0 0 TOP: -1 0 0 BOT: -1 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0
SECTOR 1
AMBIENT 31
FLOOR ALTITUDE 40
CEILING ALTITUDE 0
FLOOR TEXTURE 0 0 0
CEILING TEXTURE 0 0 0
FLAGS 0
LAYER 0
VERTICES 4
X: 40 Z: 0
X: 80 Z: 0
X: 80 Z: 40
X: 40 Z: 40
WALLS 4
WALL LEFT: 0 RIGHT: 3 MID: 0 0 0 TOP: -1 0 0 BOT: -1 0 0 SIGN: -1 0 0 ADJOIN: 0 MIRROR: 2 FLAGS: 0 0 0 LIGHT: 0
WALL LEFT: 3 RIGHT: 2 MID: 0 0 0 TOP: -1 0 0 BOT: -1 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0
WALL LEFT: 2 RIGHT: 1 MID: 0 0 0 TOP: -1 0 0 BOT: -1 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0
WALL LEFT: 1 RIGHT: 0 MID: 0 0 0 TOP: -1 0 0 BOT: -1 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0
)LEV";

        sed::assets::LevLoadOptions opts{};
        opts.dfToJkFactor = 40.0;
        opts.textureMode  = sed::assets::LevTextureMode::Keep;
        opts.loadObjects  = false;

        std::istringstream iss(levText);
        const sed::Level level = sed::assets::loadFromLEV(iss, opts);

        check(level.sectors.size() == 2, "LEV: sectors.size");
        if (level.sectors.size() == 2)
        {
            const sed::Sector& sec0 = level.sectors[0];
            const sed::Sector& sec1 = level.sectors[1];

            check(sec0.surfaces.size() == 6, "LEV: sec0.surfaces.size");
            check(sec1.surfaces.size() == 6, "LEV: sec1.surfaces.size");

            const auto sec0Adj = std::find_if(sec0.surfaces.begin(), sec0.surfaces.end(), [](const sed::Surface& s)
                {
                    return s.adjoin.has_value();
                });
            const auto sec1Adj = std::find_if(sec1.surfaces.begin(), sec1.surfaces.end(), [](const sed::Surface& s)
                {
                    return s.adjoin.has_value();
                });

            check(sec0Adj != sec0.surfaces.end(), "LEV: sec0 has adjoin surface");
            check(sec1Adj != sec1.surfaces.end(), "LEV: sec1 has adjoin surface");

            if (sec0Adj != sec0.surfaces.end() && sec1Adj != sec1.surfaces.end())
            {
                check(sec0Adj->adjoin->sector == 1, "LEV: sec0 adjoin sector");
                check(sec1Adj->adjoin->sector == 0, "LEV: sec1 adjoin sector");
                check(sec0Adj->geo == 0, "LEV: sec0 adjoin geo");
                check(sec1Adj->geo == 0, "LEV: sec1 adjoin geo");
            }
        }
    }

    {
        const char* levText = R"LEV(
LEV 1
LEVELNAME test
TEXTURES 1
TEXTURE: stone.bm
NUMSECTORS 2
SECTOR 0
AMBIENT 31
FLOOR ALTITUDE 40
CEILING ALTITUDE 0
FLOOR TEXTURE 0 0 0
CEILING TEXTURE 0 0 0
FLAGS 0
LAYER 0
VERTICES 4
X: 0 Z: 0
X: 40 Z: 0
X: 40 Z: 40
X: 0 Z: 40
WALLS 4
WALL LEFT: 0 RIGHT: 3 MID: 0 0 0 TOP: 0 0 0 BOT: 0 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0
WALL LEFT: 3 RIGHT: 2 MID: 0 0 0 TOP: 0 0 0 BOT: 0 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0
WALL LEFT: 2 RIGHT: 1 MID: 0 0 0 TOP: 0 0 0 BOT: 0 0 0 SIGN: -1 0 0 ADJOIN: 1 MIRROR: 0 FLAGS: 0 0 0 LIGHT: 0
WALL LEFT: 1 RIGHT: 0 MID: 0 0 0 TOP: 0 0 0 BOT: 0 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0
SECTOR 1
AMBIENT 31
FLOOR ALTITUDE 20
CEILING ALTITUDE 0
FLOOR TEXTURE 0 0 0
CEILING TEXTURE 0 0 0
FLAGS 0
LAYER 0
VERTICES 4
X: 40 Z: 0
X: 80 Z: 0
X: 80 Z: 40
X: 40 Z: 40
WALLS 4
WALL LEFT: 0 RIGHT: 3 MID: 0 0 0 TOP: 0 0 0 BOT: 0 0 0 SIGN: -1 0 0 ADJOIN: 0 MIRROR: 2 FLAGS: 0 0 0 LIGHT: 0
WALL LEFT: 3 RIGHT: 2 MID: 0 0 0 TOP: 0 0 0 BOT: 0 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0
WALL LEFT: 2 RIGHT: 1 MID: 0 0 0 TOP: 0 0 0 BOT: 0 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0
WALL LEFT: 1 RIGHT: 0 MID: 0 0 0 TOP: 0 0 0 BOT: 0 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0
)LEV";

        sed::assets::LevLoadOptions opts{};
        opts.dfToJkFactor = 40.0;
        opts.textureMode  = sed::assets::LevTextureMode::Keep;
        opts.loadObjects  = false;

        std::istringstream iss(levText);
        const sed::Level level = sed::assets::loadFromLEV(iss, opts);

        check(level.sectors.size() == 2, "LEV.bot: sectors.size");
        if (level.sectors.size() == 2)
        {
            const sed::Sector& sec0 = level.sectors[0];
            const sed::Sector& sec1 = level.sectors[1];

            check(sec0.surfaces.size() == 7, "LEV.bot: sec0.surfaces.size");
            check(sec1.surfaces.size() == 6, "LEV.bot: sec1.surfaces.size");

            const auto sec0Adj = std::find_if(sec0.surfaces.begin(), sec0.surfaces.end(), [](const sed::Surface& s)
                {
                    return s.adjoin.has_value();
                });

            check(sec0Adj != sec0.surfaces.end(), "LEV.bot: sec0 has adjoin surface");
            if (sec0Adj != sec0.surfaces.end())
            {
                const ZRange zr = zRange(sec0, *sec0Adj);
                check(close(zr.min, -0.5), "LEV.bot: mid.minz");
                check(close(zr.max, 0.0), "LEV.bot: mid.maxz");
            }

            bool foundBot = false;
            for (const sed::Surface& surf : sec0.surfaces)
            {
                if (surf.adjoin)
                {
                    continue;
                }

                const ZRange zr = zRange(sec0, surf);
                if (close(zr.min, -1.0) && close(zr.max, -0.5))
                {
                    foundBot = true;
                    break;
                }
            }

            check(foundBot, "LEV.bot: found BOT surface");
        }
    }

    {
        const char* levText = R"LEV(
LEV 1
LEVELNAME test
TEXTURES 1
TEXTURE: stone.bm
NUMSECTORS 1
SECTOR 0
AMBIENT 31
FLOOR ALTITUDE 40
CEILING ALTITUDE 0
SECOND ALTITUDE 40
FLOOR TEXTURE 0 0 0
CEILING TEXTURE 0 0 0
FLAGS 0
LAYER 0
VERTICES 4
X: 0 Z: 0
X: 40 Z: 0
X: 40 Z: 40
X: 0 Z: 40
WALLS 4
WALL LEFT: 0 RIGHT: 3 MID: 0 0 0 TOP: -1 0 0 BOT: -1 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0
WALL LEFT: 3 RIGHT: 2 MID: 0 0 0 TOP: -1 0 0 BOT: -1 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0
WALL LEFT: 2 RIGHT: 1 MID: 0 0 0 TOP: -1 0 0 BOT: -1 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0
WALL LEFT: 1 RIGHT: 0 MID: 0 0 0 TOP: -1 0 0 BOT: -1 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0
)LEV";

        sed::assets::LevLoadOptions opts{};
        opts.dfToJkFactor = 40.0;
        opts.textureMode  = sed::assets::LevTextureMode::Keep;
        opts.loadObjects  = false;

        std::istringstream iss(levText);
        const sed::Level level = sed::assets::loadFromLEV(iss, opts);

        check(level.sectors.size() == 2, "LEV.uw: sectors.size");
        if (level.sectors.size() == 2)
        {
            std::int32_t mainIndex = -1;
            std::int32_t uwIndex   = -1;

            if (level.sectors[0].flags == sed::SECF_Underwater)
            {
                uwIndex   = 0;
                mainIndex = 1;
            }
            else if (level.sectors[1].flags == sed::SECF_Underwater)
            {
                uwIndex   = 1;
                mainIndex = 0;
            }

            check(uwIndex >= 0, "LEV.uw: found underwater sector");
            if (uwIndex >= 0 && mainIndex >= 0)
            {
                const sed::Sector& uw   = level.sectors[static_cast<std::size_t>(uwIndex)];
                const sed::Sector& main = level.sectors[static_cast<std::size_t>(mainIndex)];

                const auto uwAdj = std::find_if(uw.surfaces.begin(), uw.surfaces.end(), [](const sed::Surface& s)
                    {
                        return s.adjoin.has_value();
                    });

                check(uwAdj != uw.surfaces.end(), "LEV.uw: underwater has adjoin");
                if (uwAdj != uw.surfaces.end())
                {
                    check(uwAdj->adjoin->sector == mainIndex, "LEV.uw: underwater adjoin sector");

                    const ZRange zr = zRange(uw, *uwAdj);
                    check(close(zr.min, -1.0), "LEV.uw: ceiling.z.min");
                    check(close(zr.max, -1.0), "LEV.uw: ceiling.z.max");
                }

                bool foundMainFloor = false;
                for (const sed::Surface& surf : main.surfaces)
                {
                    if (!surf.adjoin || surf.adjoin->sector != uwIndex)
                    {
                        continue;
                    }

                    const ZRange zr = zRange(main, surf);
                    if (close(zr.min, -1.0) && close(zr.max, -1.0))
                    {
                        foundMainFloor = true;
                        break;
                    }
                }

                check(foundMainFloor, "LEV.uw: found main floor adjoin");
            }
        }
    }

    {
        const auto unique                      = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path root       = "tmp_lev_obj_root_" + unique;
        const std::filesystem::path levPath    = root / "test.lev";
        const std::filesystem::path objectPath = root / "test.O";

        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        check(!ec, "LEV.obj: create_directories");

        {
            std::ofstream out(levPath);
            out << "LEV 1\n";
            out << "LEVELNAME test\n";
            out << "TEXTURES 1\n";
            out << "TEXTURE: stone.bm\n";
            out << "NUMSECTORS 1\n";
            out << "SECTOR 0\n";
            out << "AMBIENT 31\n";
            out << "FLOOR ALTITUDE 40\n";
            out << "CEILING ALTITUDE 0\n";
            out << "FLOOR TEXTURE 0 0 0\n";
            out << "CEILING TEXTURE 0 0 0\n";
            out << "FLAGS 0\n";
            out << "LAYER 0\n";
            out << "VERTICES 4\n";
            out << "X: 0 Z: 0\n";
            out << "X: 40 Z: 0\n";
            out << "X: 40 Z: 40\n";
            out << "X: 0 Z: 40\n";
            out << "WALLS 4\n";
            out << "WALL LEFT: 0 RIGHT: 3 MID: 0 0 0 TOP: -1 0 0 BOT: -1 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0\n";
            out << "WALL LEFT: 3 RIGHT: 2 MID: 0 0 0 TOP: -1 0 0 BOT: -1 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0\n";
            out << "WALL LEFT: 2 RIGHT: 1 MID: 0 0 0 TOP: -1 0 0 BOT: -1 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0\n";
            out << "WALL LEFT: 1 RIGHT: 0 MID: 0 0 0 TOP: -1 0 0 BOT: -1 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 FLAGS: 0 0 0 LIGHT: 0\n";
        }

        {
            std::ofstream out(objectPath);
            out << "CLASS: SAFE X: 20 Y: 20 Z: 20 PCH: 0 YAW: 0 ROL: 0\n";
            out << "SEQ\n";
            out << "LOGIC: ITEM mylogic\n";
            out << "SEQEND\n";
        }

        sed::assets::Df2JkConvTable conversion{};
        conversion["MYLOGIC"] = "my_template";

        sed::assets::LevLoadOptions opts{};
        opts.dfToJkFactor     = 40.0;
        opts.textureMode      = sed::assets::LevTextureMode::Keep;
        opts.loadObjects      = true;
        opts.conversionTable  = &conversion;

        const sed::Level level = sed::assets::loadFromLEV(levPath, opts);

        check(level.sectors.size() == 1, "LEV.obj: sectors.size");
        check(level.things.size() == 1, "LEV.obj: things.size");
        if (level.things.size() == 1)
        {
            const sed::Thing& thing = level.things[0];
            check(thing.name == "my_template", "LEV.obj: thing.name");
            check(thing.sector == 0, "LEV.obj: thing.sector");
            check(close(thing.x, 0.5), "LEV.obj: thing.x");
            check(close(thing.y, 0.5), "LEV.obj: thing.y");
            check(close(thing.z, -0.5), "LEV.obj: thing.z");
        }

        std::filesystem::remove_all(root, ec);
    }
}

} // namespace sed::tests
