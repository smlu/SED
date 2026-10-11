#include "test_common.hpp"

#include "sed/assets/jkl.hpp"
#include "sed/assets/mat.hpp"
#include "sed/assets/material_dimensions.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/sed.hpp"
#include "sed/assets/tpl.hpp"
#include "sed/level.hpp"
#include "sed/text.hpp"
#include "../../src/assets/atomic_file_write.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace sed::tests
{

namespace
{

void appendLittleU32(std::vector<std::byte>& bytes, const std::uint32_t value)
{
    bytes.push_back(static_cast<std::byte>(value & 0xffu));
    bytes.push_back(static_cast<std::byte>((value >> 8u) & 0xffu));
    bytes.push_back(static_cast<std::byte>((value >> 16u) & 0xffu));
    bytes.push_back(static_cast<std::byte>((value >> 24u) & 0xffu));
}

std::vector<std::byte> makeIndexedMat(
    const std::int32_t width,
    const std::int32_t height)
{
    std::vector<std::byte> bytes{
        std::byte{ 'M' },
        std::byte{ 'A' },
        std::byte{ 'T' },
        std::byte{ ' ' },
    };
    appendLittleU32(bytes, 0x32u);
    appendLittleU32(bytes, 2u);
    appendLittleU32(bytes, 1u);
    appendLittleU32(bytes, 1u);

    appendLittleU32(bytes, static_cast<std::uint32_t>(sed::ColorMode::Indexed));
    appendLittleU32(bytes, 8u);
    for (std::size_t index = 0; index < 12u; ++index)
    {
        appendLittleU32(bytes, 0u);
    }

    appendLittleU32(bytes, 8u);
    appendLittleU32(bytes, 0u);
    for (std::size_t index = 0; index < 8u; ++index)
    {
        appendLittleU32(bytes, 0u);
    }

    appendLittleU32(bytes, static_cast<std::uint32_t>(width));
    appendLittleU32(bytes, static_cast<std::uint32_t>(height));
    appendLittleU32(bytes, 0u);
    appendLittleU32(bytes, 0u);
    appendLittleU32(bytes, 0u);
    appendLittleU32(bytes, 1u);
    bytes.resize(bytes.size()
        + static_cast<std::size_t>(width)
            * static_cast<std::size_t>(height));
    return bytes;
}

bool writeBytes(
    const std::filesystem::path& path,
    const std::vector<std::byte>& bytes)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

std::string readTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

sed::Level makeUvBoundaryLevel(const sed::ProjectType kind)
{
    sed::Level level{};
    level.clear();
    level.kind = kind;
    level.header.version = kind == sed::ProjectType::IJIM ? 3 : 1;
    level.materials = { "uv_boundary.mat" };

    sed::Sector& sector = level.newSector();
    sector.addVertex(0.0, 0.0, 0.0);
    sector.addVertex(1.0, 0.0, 0.0);
    sector.addVertex(0.0, 1.0, 0.0);

    sed::Surface surface{};
    surface.nMat = 0;
    surface.material = "uv_boundary.mat";
    surface.vertices = { 0, 1, 2 };
    surface.texVertices = {
        sed::TexVertex{ 0.0f, 0.0f, sed::ColorWhite },
        sed::TexVertex{ 0.25f, 0.5f, sed::ColorWhite },
        sed::TexVertex{ 0.75f, 0.25f, sed::ColorWhite },
    };
    surface.normal = sed::TVector{ 0.0, 0.0, 1.0 };
    sector.addSurface(std::move(surface));
    return level;
}

bool hasUvValues(
    const sed::Level& level,
    const std::array<sed::TPoint2, 3>& expected)
{
    if (level.sectors.size() != 1u
        || level.sectors[0].surfaces.size() != 1u
        || level.sectors[0].surfaces[0].texVertices.size() != expected.size())
    {
        return false;
    }

    const std::vector<sed::TexVertex>& actual =
        level.sectors[0].surfaces[0].texVertices;
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        if (!close(actual[index].u, expected[index].x)
            || !close(actual[index].v, expected[index].y))
        {
            return false;
        }
    }
    return true;
}

std::string makeJklUvNormalizationFixture(
    const std::string_view secondMaterialIndex,
    const std::string_view materialName = "uv_boundary.mat")
{
    return std::string(R"JKL(
        SECTION: HEADER
        VERSION 1
        PIXELS PER UNIT 320
        END

        SECTION: MATERIALS
        WORLD MATERIALS 1
        0: MAT: )JKL")
        + std::string(materialName)
        + R"JKL(
        END

        SECTION: GEORESOURCE
        WORLD COLORMAPS 0
        WORLD VERTICES 7
        0: 0 0 0
        1: 1 0 0
        2: 0 1 0
        3: 0 0 1
        4: 2 0 1
        5: 2 1 1
        6: 0 1 1
        WORLD TEXTURE VERTICES 7
        0: 0 0
        1: 8 8
        2: 24 4
        3: 7 9
        4: 11 13
        5: -3 5
        6: 17 -2
        WORLD ADJOINS 0
        WORLD SURFACES 2
        0: 0 0x0 0x0 0 3 3 -1 0 3 0,0 1,1 2,2 1 1 1
        0: 0 0 1
        1: )JKL"
        + std::string(secondMaterialIndex)
        + R"JKL( 0x0 0x0 0 3 3 -1 0 4 3,3 4,4 5,5 6,6 1 2 3 4
        0: 0 0 1
        END

        SECTION: SECTORS
        WORLD SECTORS 1
        SECTOR 0
        FLAGS 0
        AMBIENT LIGHT 0
        EXTRA LIGHT 0
        SURFACES 0 2
        END
        )JKL";
}

struct ParsedJklGeometry
{
    std::vector<sed::TVector> worldVertices{};
    std::vector<std::vector<std::int32_t>> surfaceVertices{};
    std::vector<std::int32_t> surfaceAdjoins{};
    std::vector<std::int32_t> adjoinMirrors{};
    std::vector<double> adjoinDistances{};
    std::vector<sed::ColorF> surfaceExtraLights{};
    std::vector<bool> surfaceExtraLightParsed{};
    std::vector<sed::TVector> surfaceNormals{};
    std::vector<sed::TBox> sectorBounds{};
    std::vector<sed::TBox> sectorCollideBoxes{};
    std::vector<sed::TVector> sectorCenters{};
    std::vector<double> sectorRadii{};
};

ParsedJklGeometry parseJklGeometry(const std::string& text)
{
    ParsedJklGeometry parsed{};
    const bool isIjim = text.find("VERSION 3") != std::string::npos;
    std::istringstream input(text);
    std::vector<std::string> lines{};
    for (std::string line{}; std::getline(input, line);)
    {
        lines.push_back(std::move(line));
    }

    for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
    {
        const std::string& line = lines[lineIndex];
        constexpr std::string_view kWorldVertices = "WORLD VERTICES ";
        constexpr std::string_view kWorldAdjoins = "WORLD ADJOINS ";
        constexpr std::string_view kWorldSurfaces = "WORLD SURFACES ";

        if (line.rfind(kWorldVertices, 0) == 0)
        {
            std::int32_t count = 0;
            std::istringstream countInput(line.substr(kWorldVertices.size()));
            countInput >> count;
            parsed.worldVertices.reserve(static_cast<std::size_t>(std::max(count, 0)));
            for (std::int32_t row = 0; row < count && lineIndex + 1 < lines.size(); ++row)
            {
                ++lineIndex;
                const std::size_t colon = lines[lineIndex].find(':');
                if (colon == std::string::npos)
                {
                    continue;
                }
                std::istringstream vertexInput(lines[lineIndex].substr(colon + 1));
                sed::TVector vertex{};
                vertexInput >> vertex.x >> vertex.y >> vertex.z;
                parsed.worldVertices.push_back(vertex);
            }
            continue;
        }

        if (line.rfind(kWorldAdjoins, 0) == 0)
        {
            std::int32_t count = 0;
            std::istringstream countInput(line.substr(kWorldAdjoins.size()));
            countInput >> count;
            parsed.adjoinMirrors.reserve(static_cast<std::size_t>(std::max(count, 0)));
            for (std::int32_t row = 0; row < count && lineIndex + 1 < lines.size(); ++row)
            {
                ++lineIndex;
                const std::size_t colon = lines[lineIndex].find(':');
                if (colon == std::string::npos)
                {
                    parsed.adjoinMirrors.push_back(-1);
                    continue;
                }
                std::istringstream adjoinInput(lines[lineIndex].substr(colon + 1));
                std::string flags{};
                std::int32_t mirror = -1;
                double distance = 0.0;
                adjoinInput >> flags >> mirror >> distance;
                parsed.adjoinMirrors.push_back(mirror);
                parsed.adjoinDistances.push_back(distance);
            }
            continue;
        }

        if (line.rfind(kWorldSurfaces, 0) == 0)
        {
            std::int32_t count = 0;
            std::istringstream countInput(line.substr(kWorldSurfaces.size()));
            countInput >> count;
            parsed.surfaceVertices.reserve(static_cast<std::size_t>(std::max(count, 0)));
            parsed.surfaceAdjoins.reserve(static_cast<std::size_t>(std::max(count, 0)));
            for (std::int32_t row = 0; row < count && lineIndex + 1 < lines.size(); ++row)
            {
                ++lineIndex;
                const std::size_t colon = lines[lineIndex].find(':');
                if (colon == std::string::npos)
                {
                    parsed.surfaceVertices.emplace_back();
                    parsed.surfaceAdjoins.push_back(-1);
                    continue;
                }

                std::istringstream surfaceInput(lines[lineIndex].substr(colon + 1));
                std::vector<std::string> fields{};
                for (std::string field{}; surfaceInput >> field;)
                {
                    fields.push_back(std::move(field));
                }

                std::int32_t adjoin = -1;
                if (fields.size() > 6u)
                {
                    std::istringstream adjoinInput(fields[6]);
                    adjoinInput >> adjoin;
                }

                std::vector<std::int32_t> vertices{};
                for (const std::string& field : fields)
                {
                    const std::size_t comma = field.find(',');
                    if (comma == std::string::npos)
                    {
                        continue;
                    }
                    std::istringstream vertexInput(field.substr(0, comma));
                    std::int32_t vertex = -1;
                    vertexInput >> vertex;
                    vertices.push_back(vertex);
                }

                sed::ColorF extraLight{};
                bool parsedExtraLight = false;
                if (fields.size() > 7u)
                {
                    std::istringstream redInput(fields[7]);
                    parsedExtraLight = static_cast<bool>(redInput >> extraLight.r);
                    if (isIjim && fields.size() > 10u)
                    {
                        std::istringstream greenInput(fields[8]);
                        std::istringstream blueInput(fields[9]);
                        std::istringstream alphaInput(fields[10]);
                        parsedExtraLight = parsedExtraLight
                            && static_cast<bool>(greenInput >> extraLight.g)
                            && static_cast<bool>(blueInput >> extraLight.b)
                            && static_cast<bool>(alphaInput >> extraLight.a);
                    }
                    else
                    {
                        extraLight.g = extraLight.r;
                        extraLight.b = extraLight.r;
                    }
                }

                parsed.surfaceVertices.push_back(std::move(vertices));
                parsed.surfaceAdjoins.push_back(adjoin);
                parsed.surfaceExtraLights.push_back(extraLight);
                parsed.surfaceExtraLightParsed.push_back(parsedExtraLight);
            }

            for (std::int32_t row = 0; row < count && lineIndex + 1 < lines.size(); ++row)
            {
                ++lineIndex;
                const std::size_t colon = lines[lineIndex].find(':');
                if (colon == std::string::npos)
                {
                    continue;
                }
                std::istringstream normalInput(lines[lineIndex].substr(colon + 1));
                sed::TVector normal{};
                normalInput >> normal.x >> normal.y >> normal.z;
                parsed.surfaceNormals.push_back(normal);
            }
        }

        constexpr std::string_view kBoundBox = "BOUNDBOX ";
        if (line.rfind(kBoundBox, 0) == 0)
        {
            std::istringstream boundsInput(line.substr(kBoundBox.size()));
            sed::TBox bounds{};
            boundsInput >> bounds.x1 >> bounds.y1 >> bounds.z1
                >> bounds.x2 >> bounds.y2 >> bounds.z2;
            parsed.sectorBounds.push_back(bounds);
        }

        constexpr std::string_view kCollideBox = "COLLIDEBOX ";
        if (line.rfind(kCollideBox, 0) == 0)
        {
            std::istringstream collideInput(line.substr(kCollideBox.size()));
            sed::TBox bounds{};
            collideInput >> bounds.x1 >> bounds.y1 >> bounds.z1
                >> bounds.x2 >> bounds.y2 >> bounds.z2;
            parsed.sectorCollideBoxes.push_back(bounds);
        }

        constexpr std::string_view kCenter = "CENTER ";
        if (line.rfind(kCenter, 0) == 0)
        {
            std::istringstream centerInput(line.substr(kCenter.size()));
            sed::TVector center{};
            centerInput >> center.x >> center.y >> center.z;
            parsed.sectorCenters.push_back(center);
        }

        constexpr std::string_view kRadius = "RADIUS ";
        if (line.rfind(kRadius, 0) == 0)
        {
            std::istringstream radiusInput(line.substr(kRadius.size()));
            double radius = 0.0;
            radiusInput >> radius;
            parsed.sectorRadii.push_back(radius);
        }
    }

    return parsed;
}

sed::Level makeVertexWeldLevel(
    const sed::ProjectType kind,
    const double diagonalOffset,
    const bool adjoin)
{
    sed::Level level{};
    level.clear();
    level.kind = kind;
    level.header.version = kind == sed::ProjectType::IJIM ? 3 : 1;
    level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;

    const auto addTriangle = [&](sed::Sector& sector, const double offset, const bool reverse)
    {
        sector.addVertex(-2.0 + offset, -3.0 + offset, -4.0 + offset);
        sector.addVertex(0.0, 0.0, 0.0);
        sector.addVertex(3.0, 0.0, 0.0);

        sed::Surface surface{};
        surface.vertices = reverse
            ? std::vector<std::int32_t>{ 0, 2, 1 }
            : std::vector<std::int32_t>{ 0, 1, 2 };
        surface.texVertices = {
            sed::TexVertex{ 0.125f, -0.25f, sed::ColorF{ 0.1f, 0.2f, 0.3f, 0.4f } },
            sed::TexVertex{ 1.25f, 0.5f, sed::ColorF{ 0.5f, 0.6f, 0.7f, 0.8f } },
            sed::TexVertex{ -0.75f, 2.0f, sed::ColorF{ 0.9f, 0.8f, 0.7f, 0.6f } },
        };
        surface.normal = sed::TVector{ 0.0, 0.0, reverse ? -1.0 : 1.0 };
        sector.addSurface(std::move(surface));
    };

    sed::Sector& first = level.newSector();
    addTriangle(first, 0.0, false);

    sed::Sector& second = level.newSector();
    addTriangle(second, diagonalOffset, adjoin);

    if (adjoin)
    {
        level.sectors[0].surfaces[0].adjoin = sed::SurfaceRef{ 1, 0 };
        level.sectors[0].surfaces[0].adjoinFlags = sed::SAF_Move;
        level.sectors[1].surfaces[0].adjoin = sed::SurfaceRef{ 0, 0 };
        level.sectors[1].surfaces[0].adjoinFlags = sed::SAF_Visible;
    }

    return level;
}

sed::Level makeOriginVertexWeldLevel(
    const sed::ProjectType kind,
    const sed::TVector offset)
{
    sed::Level level{};
    level.clear();
    level.kind = kind;
    level.header.version = kind == sed::ProjectType::IJIM ? 3 : 1;
    level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;

    const auto addTriangle = [&](sed::Sector& sector, const sed::TVector first)
    {
        sector.addVertex(first.x, first.y, first.z);
        sector.addVertex(10.0, 0.0, 0.0);
        sector.addVertex(0.0, 10.0, 0.0);

        sed::Surface surface{};
        surface.vertices = { 0, 1, 2 };
        surface.texVertices.resize(3u);
        surface.normal = sed::TVector{ 0.0, 0.0, 1.0 };
        sector.addSurface(std::move(surface));
    };

    sed::Sector& first = level.newSector();
    addTriangle(first, sed::TVector::zero);

    sed::Sector& second = level.newSector();
    addTriangle(second, offset);
    return level;
}

sed::Level makeExtraLightExportLevel(const sed::ProjectType kind)
{
    sed::Level level{};
    level.clear();
    level.kind = kind;
    level.header.version = kind == sed::ProjectType::IJIM ? 3 : 1;
    level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;

    sed::Sector& sector = level.newSector();
    sector.newVertex(0.0, 0.0, 0.0);
    sector.newVertex(1.0, 0.0, 0.0);
    sector.newVertex(0.0, 1.0, 0.0);

    const std::array<sed::ColorF, 4> extraLights{
        sed::ColorF{ 10.0f, 10.0f, 10.0f, 1.5f },
        sed::ColorF{ 1.08f, 1.08f, 1.08f, -0.5f },
        sed::ColorF{ -2.0f, -2.0f, -2.0f, 2.0f },
        sed::ColorF{ 10.0f, 1.08f, -2.0f, 0.75f },
    };
    for (const sed::ColorF extraLight : extraLights)
    {
        sed::Surface surface{};
        surface.vertices = { 0, 1, 2 };
        surface.texVertices.resize(3u);
        surface.normal = sed::TVector{ 0.0, 0.0, 1.0 };
        surface.extraLight = extraLight;
        sector.addSurface(std::move(surface));
    }

    return level;
}

sed::Level makeVertexWeldChainLevel(
    const sed::ProjectType kind,
    const double step)
{
    sed::Level level{};
    level.clear();
    level.kind = kind;
    level.header.version = kind == sed::ProjectType::IJIM ? 3 : 1;
    level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;

    const std::array<double, 3> firstCoordinates{
        -10.0,
        -10.0 + step,
        -10.0 + step * 2.0,
    };
    for (const double firstX : firstCoordinates)
    {
        sed::Sector& sector = level.newSector();
        sector.addVertex(firstX, -10.0, -10.0);
        sector.addVertex(0.0, 0.0, 0.0);
        sector.addVertex(3.0, 0.0, 0.0);

        sed::Surface surface{};
        surface.vertices = { 0, 1, 2 };
        surface.texVertices.resize(3u);
        surface.normal = sed::TVector{ 0.0, 0.0, 1.0 };
        sector.addSurface(std::move(surface));
    }

    return level;
}

sed::Level makeCollapsedWeldLevel(const sed::ProjectType kind)
{
    sed::Level level{};
    level.clear();
    level.kind = kind;
    level.header.version = kind == sed::ProjectType::IJIM ? 3 : 1;
    level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;

    sed::Sector& sector = level.newSector();
    // addVertex intentionally applies the editor's CloseEnough policy. Keep
    // these source corners distinct so the exporter can exercise its own
    // opt-in collapse rejection.
    sector.newVertex(-2.0, -3.0, -4.0);
    sector.newVertex(-2.0 + 1.0e-6, -3.0, -4.0);
    sector.newVertex(3.0, 0.0, 0.0);

    sed::Surface surface{};
    surface.vertices = { 0, 1, 2 };
    surface.texVertices.resize(3u);
    surface.normal = sed::TVector{ 0.0, 0.0, 1.0 };
    sector.addSurface(std::move(surface));
    return level;
}

sed::Level makeNonPlanarQuadWeldLevel(const sed::ProjectType kind)
{
    sed::Level level{};
    level.clear();
    level.kind = kind;
    level.header.version = kind == sed::ProjectType::IJIM ? 3 : 1;
    level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;

    sed::Sector& sector = level.newSector();
    // The first vertex is an unreferenced representative. The final quad
    // corner is close enough to weld to it, lifting that corner out of the
    // original quad plane without collapsing an index.
    sector.addVertex(0.0, 2.0, 0.5);
    sector.addVertex(0.0, 0.0, 0.0);
    sector.addVertex(2.0, 0.0, 0.0);
    sector.addVertex(2.0, 2.0, 0.0);
    sector.addVertex(0.0, 2.0, 0.0);

    sed::Surface surface{};
    surface.vertices = { 1, 2, 3, 4 };
    surface.texVertices.resize(4u);
    surface.normal = sed::TVector{ 0.0, 0.0, 1.0 };
    sector.addSurface(std::move(surface));
    return level;
}

sed::Level makeConcaveQuadWeldLevel(const sed::ProjectType kind)
{
    sed::Level level{};
    level.clear();
    level.kind = kind;
    level.header.version = kind == sed::ProjectType::IJIM ? 3 : 1;
    level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;

    sed::Sector& sector = level.newSector();
    // The first vertex is an unreferenced representative. Welding the top
    // corner through the broad opposite edge makes the quad concave while
    // keeping every exported corner distinct.
    sector.addVertex(0.0, -1.0, 0.0);
    sector.addVertex(0.0, -100.0, 0.0);
    sector.addVertex(-100.0, 0.0, 0.0);
    sector.addVertex(0.0, 1.0, 0.0);
    sector.addVertex(100.0, 0.0, 0.0);

    sed::Surface surface{};
    surface.vertices = { 1, 2, 3, 4 };
    surface.texVertices.resize(4u);
    surface.normal = sed::TVector{ 0.0, 0.0, -1.0 };
    sector.addSurface(std::move(surface));
    return level;
}

sed::Level makeConvexSectorWeldLevel(const sed::ProjectType kind)
{
    sed::Level level{};
    level.clear();
    level.kind = kind;
    level.header.version = kind == sed::ProjectType::IJIM ? 3 : 1;
    level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;

    // Keep the nearby representative outside the tested source sector. This
    // lets the source tetrahedron remain convex while its apex is mapped
    // across the opposite base plane during export.
    sed::Sector& representativeSector = level.newSector();
    representativeSector.addVertex(0.0, 0.0, -1.0);

    sed::Sector& sector = level.newSector();
    sector.addVertex(-100.0, -100.0, 0.0); // A
    sector.addVertex(100.0, -100.0, 0.0);  // B
    sector.addVertex(0.0, 100.0, 0.0);     // C
    sector.addVertex(0.0, 0.0, 1.0);       // D

    const auto addTriangle = [&](const std::initializer_list<std::int32_t> vertices)
    {
        sed::Surface surface{};
        surface.vertices.assign(vertices.begin(), vertices.end());
        surface.texVertices.resize(surface.vertices.size());
        surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);
        sector.addSurface(std::move(surface));
    };

    addTriangle({ 0, 1, 2 }); // base, inward normal +Z
    addTriangle({ 0, 3, 1 });
    addTriangle({ 1, 3, 2 });
    addTriangle({ 2, 3, 0 });
    return level;
}

sed::Level makeMappedAdjoinDistanceWeldLevel(const sed::ProjectType kind)
{
    sed::Level level{};
    level.clear();
    level.kind = kind;
    level.header.version = kind == sed::ProjectType::IJIM ? 3 : 1;
    level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;

    // Representatives for the first portal corner and for an off-plane
    // sector-center vertex are encountered before the adjoined sector.
    sed::Sector& representatives = level.newSector();
    representatives.addVertex(0.0, 0.0, 0.0);
    representatives.addVertex(3.0, 3.0, 0.0);

    sed::Sector& changed = level.newSector();
    changed.addVertex(0.0, 0.0, 1.0);
    changed.addVertex(10.0, 0.0, 0.0);
    changed.addVertex(0.0, 10.0, 0.0);
    // This point is inside the source portal plane but welds onto the mapped
    // plane, moving the mapped sector center onto that plane.
    changed.addVertex(3.0, 3.0, 0.1);

    sed::Surface changedSurface{};
    changedSurface.vertices = { 0, 2, 1 };
    changedSurface.texVertices.resize(3u);
    changedSurface.normal = sed::level::surfaceCalcNormalNewell(
        changed,
        changedSurface);
    changedSurface.adjoin = sed::SurfaceRef{ 2, 0 };
    changedSurface.adjoinFlags = sed::SAF_Move;
    changed.addSurface(std::move(changedSurface));

    sed::Sector& matching = level.newSector();
    matching.addVertex(0.0, 0.0, 0.0);
    matching.addVertex(10.0, 0.0, 0.0);
    matching.addVertex(0.0, 10.0, 0.0);
    sed::Surface matchingSurface{};
    matchingSurface.vertices = { 0, 1, 2 };
    matchingSurface.texVertices.resize(3u);
    matchingSurface.normal = sed::level::surfaceCalcNormalNewell(
        matching,
        matchingSurface);
    matchingSurface.adjoin = sed::SurfaceRef{ 1, 0 };
    matchingSurface.adjoinFlags = sed::SAF_Visible;
    matching.addSurface(std::move(matchingSurface));
    return level;
}

bool sameWeldSourceGeometry(const sed::Level& left, const sed::Level& right)
{
    const auto sameFloat = [](const float lhs, const float rhs) noexcept
    {
        return lhs == rhs || (std::isnan(lhs) && std::isnan(rhs));
    };

    if (left.sectors.size() != right.sectors.size())
    {
        return false;
    }

    for (std::size_t sectorIndex = 0; sectorIndex < left.sectors.size(); ++sectorIndex)
    {
        const sed::Sector& lhsSector = left.sectors[sectorIndex];
        const sed::Sector& rhsSector = right.sectors[sectorIndex];
        if (lhsSector.vertices.size() != rhsSector.vertices.size()
            || lhsSector.surfaces.size() != rhsSector.surfaces.size())
        {
            return false;
        }

        for (std::size_t vertexIndex = 0; vertexIndex < lhsSector.vertices.size(); ++vertexIndex)
        {
            const sed::Vertex& lhs = lhsSector.vertices[vertexIndex];
            const sed::Vertex& rhs = rhsSector.vertices[vertexIndex];
            if (lhs.x != rhs.x || lhs.y != rhs.y || lhs.z != rhs.z || lhs.mark != rhs.mark)
            {
                return false;
            }
        }

        for (std::size_t surfaceIndex = 0; surfaceIndex < lhsSector.surfaces.size(); ++surfaceIndex)
        {
            const sed::Surface& lhs = lhsSector.surfaces[surfaceIndex];
            const sed::Surface& rhs = rhsSector.surfaces[surfaceIndex];
            if (lhs.vertices != rhs.vertices
                || lhs.adjoin != rhs.adjoin
                || lhs.adjoinFlags != rhs.adjoinFlags
                || lhs.texVertices.size() != rhs.texVertices.size()
                || !sameFloat(lhs.extraLight.r, rhs.extraLight.r)
                || !sameFloat(lhs.extraLight.g, rhs.extraLight.g)
                || !sameFloat(lhs.extraLight.b, rhs.extraLight.b)
                || !sameFloat(lhs.extraLight.a, rhs.extraLight.a)
                || lhs.normal.x != rhs.normal.x
                || lhs.normal.y != rhs.normal.y
                || lhs.normal.z != rhs.normal.z)
            {
                return false;
            }

            for (std::size_t texIndex = 0; texIndex < lhs.texVertices.size(); ++texIndex)
            {
                const sed::TexVertex& lhsTex = lhs.texVertices[texIndex];
                const sed::TexVertex& rhsTex = rhs.texVertices[texIndex];
                if (lhsTex.u != rhsTex.u
                    || lhsTex.v != rhsTex.v
                    || lhsTex.color.r != rhsTex.color.r
                    || lhsTex.color.g != rhsTex.color.g
                    || lhsTex.color.b != rhsTex.color.b
                    || lhsTex.color.a != rhsTex.color.a)
                {
                    return false;
                }
            }
        }
    }

    return true;
}

} // namespace

void runJklSaveTests()
{
    {
        const std::string unique = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path root =
            std::filesystem::temp_directory_path() / ("sed_jkl_atomic_save_" + unique);
        std::error_code error;
        std::filesystem::create_directories(root, error);
        check(!error, "JKL atomic path save: temporary directory is created");

        const auto countStagedFiles = [&]()
        {
            std::size_t count = 0;
            if ( std::filesystem::exists(root) )
            {
                for ( const std::filesystem::directory_entry& entry
                    : std::filesystem::directory_iterator(root) )
                {
                    const std::string name = entry.path().filename().string();
                    if ( name.starts_with(".sed-save-") && name.ends_with(".tmp") )
                    {
                        ++count;
                    }
                }
            }
            return count;
        };

        const std::array<sed::ProjectType, 3> projectTypes{
            sed::ProjectType::JKDF2,
            sed::ProjectType::MOTS,
            sed::ProjectType::IJIM,
        };
        for ( std::size_t index = 0; index < projectTypes.size(); ++index )
        {
            const sed::ProjectType projectType = projectTypes[index];
            const sed::Level source = makeExtraLightExportLevel(projectType);
            std::ostringstream streamOutput;
            sed::assets::saveToJKL(source, streamOutput);

            const std::filesystem::path path =
                root / ("roundtrip_" + std::to_string(index) + ".jkl");
            sed::assets::saveToJKL(source, path);
            const std::string savedText = readTextFile(path);
            check(savedText == streamOutput.str(),
                "JKL atomic path save: successful path output matches the readable stream serializer");

            std::istringstream input(savedText);
            sed::assets::JklLoadOptions loadOptions{};
            loadOptions.projectType = projectType;
            const sed::Level roundTrip = sed::assets::loadFromJKL(input, loadOptions);
            check(roundTrip.kind == projectType
                    && roundTrip.header.version
                        == (projectType == sed::ProjectType::IJIM ? 3 : 1)
                    && roundTrip.sectors.size() == 1
                    && roundTrip.sectors[0].surfaces.size() == 4,
                "JKL atomic path save: successful files round-trip for JKDF2, MotS, and IJIM");
        }

        const sed::Level source = makeExtraLightExportLevel(sed::ProjectType::IJIM);
        const std::array<sed::assets::detail::StagedAssetFileFaultPointForTests, 4> faultPoints{
            sed::assets::detail::StagedAssetFileFaultPointForTests::write,
            sed::assets::detail::StagedAssetFileFaultPointForTests::flush,
            sed::assets::detail::StagedAssetFileFaultPointForTests::close,
            sed::assets::detail::StagedAssetFileFaultPointForTests::publish,
        };
        const std::string sentinel = "previous complete JKL destination\n";
        const std::filesystem::path destination = root / "atomic_failure.ndy";
        for ( const auto faultPoint : faultPoints )
        {
            {
                std::ofstream existing(destination, std::ios::binary | std::ios::trunc);
                existing.write(sentinel.data(), static_cast<std::streamsize>(sentinel.size()));
                check(static_cast<bool>(existing),
                    "JKL atomic path save: fault fixture installs prior complete contents");
            }

            bool threw = false;
            bool injected = false;
            {
                sed::assets::detail::ScopedStagedAssetFileFaultForTests fault(faultPoint);
                try
                {
                    sed::assets::saveToJKL(source, destination);
                }
                catch (const std::exception&)
                {
                    threw = true;
                }
                injected = fault.triggered();
            }
            const bool restored = sed::assets::detail::stagedAssetFileFaultStateForTests.point
                == sed::assets::detail::StagedAssetFileFaultPointForTests::none;
            check(injected,
                "JKL atomic path save: selected filesystem failure checkpoint was reached");
            check(threw && readTextFile(destination) == sentinel,
                "JKL atomic path save: staged write, flush, close, or publish failure keeps the prior file");
            check(restored && countStagedFiles() == 0,
                "JKL atomic path save: fault guard restores state and removes every staging file");
        }

        const std::filesystem::path directoryDestination = root / "directory_target.ndy";
        std::filesystem::create_directory(directoryDestination, error);
        check(!error, "JKL atomic path save: directory rejection fixture is created");
        const std::filesystem::path child = directoryDestination / "keep.txt";
        {
            std::ofstream keep(child, std::ios::binary | std::ios::trunc);
            keep << "directory contents stay intact\n";
            check(static_cast<bool>(keep),
                "JKL atomic path save: directory rejection fixture has retained contents");
        }
        bool directoryRejected = false;
        try
        {
            sed::assets::saveToJKL(source, directoryDestination);
        }
        catch (const std::exception&)
        {
            directoryRejected = true;
        }
        check(directoryRejected && std::filesystem::is_directory(directoryDestination)
                && readTextFile(child) == "directory contents stay intact\n",
            "JKL atomic path save: real filesystem publication rejection preserves a directory");
        check(countStagedFiles() == 0,
            "JKL atomic path save: real filesystem rejection removes the sibling staging file");

        std::filesystem::remove_all(root, error);
    }

    {
        sed::Level source = makeExtraLightExportLevel(sed::ProjectType::IJIM);
        const float largestFinite = std::numeric_limits<float>::max();
        source.sectors[0].surfaces[0].extraLight = sed::ColorF{
            largestFinite, -largestFinite, 10.0f, 0.75f };
        sed::assets::JklSaveOptions options{};
        options.clampExtraLight = false;

        std::ostringstream output;
        sed::assets::saveToJKL(source, output, options);
        const std::string text = output.str();
        const std::size_t surfaceHeader = text.find("WORLD SURFACES ");
        const std::size_t headerEnd = surfaceHeader == std::string::npos
            ? std::string::npos : text.find('\n', surfaceHeader);
        const std::size_t surfaceRowStart = headerEnd == std::string::npos
            ? std::string::npos : headerEnd + 1;
        const std::size_t surfaceRowEnd = surfaceRowStart == std::string::npos
            ? std::string::npos : text.find('\n', surfaceRowStart);
        std::vector<std::string_view> surfaceFields{};
        if ( surfaceRowEnd != std::string::npos )
        {
            surfaceFields = sed::text::splitWords(std::string_view(text).substr(
                surfaceRowStart, surfaceRowEnd - surfaceRowStart));
        }

        const double largestFiniteAsDouble = static_cast<double>(largestFinite);
        const double previousFloat = static_cast<double>(
            std::nextafter(largestFinite, 0.0f));
        const double nextFloat = std::ldexp(
            1.0, std::numeric_limits<float>::max_exponent);
        const double lowerFloatMaxRoundingBoundary =
            (previousFloat + largestFiniteAsDouble) / 2.0;
        const double upperFloatMaxRoundingBoundary =
            (largestFiniteAsDouble + nextFloat) / 2.0;
        const auto wireValueRoundsToFloatMax = [&](
            const std::string_view token,
            const bool negative)
        {
            const std::optional<double> value = sed::text::parseDouble(token);
            if ( !value || !std::isfinite(*value) )
            {
                return false;
            }
            return negative
                ? *value < -lowerFloatMaxRoundingBoundary
                    && *value > -upperFloatMaxRoundingBoundary
                : *value > lowerFloatMaxRoundingBoundary
                    && *value < upperFloatMaxRoundingBoundary;
        };
        // The first IJIM WORLD SURFACES row stores extra-light RGB starting
        // at zero-based field 8. Validate the red and green tokens against
        // float max's rounding interval instead of assuming one spelling.
        check(surfaceFields.size() >= 12
                && wireValueRoundsToFloatMax(surfaceFields[8], false)
                && wireValueRoundsToFloatMax(surfaceFields[9], true),
            "NDY serializer: emitted signed channels round to finite float maximum");

        std::istringstream input(text);
        sed::assets::JklLoadOptions loadOptions{};
        loadOptions.projectType = sed::ProjectType::IJIM;
        std::optional<sed::Level> roundTrip;
        try
        {
            roundTrip = sed::assets::loadFromJKL(input, loadOptions);
        }
        catch (const std::exception&)
        {
        }
        check(roundTrip.has_value()
                && roundTrip->sectors.size() == 1
                && roundTrip->sectors[0].surfaces.size() == 4
                && roundTrip->sectors[0].surfaces[0].extraLight.r == largestFinite
                && roundTrip->sectors[0].surfaces[0].extraLight.g == -largestFinite,
            "NDY serializer max-float output reloads as finite signed maximum channels");
    }

    {
        // Sector scalar lights are authored signed values. The export option
        // for surface-light clamping must not alter these sector fields, and
        // a save/load cycle must preserve finite negative and HDR values.
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;
        level.header.version = 1;
        level.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;

        sed::Sector& sector = level.newSector();
        sector.ambient = sed::ColorF{ -2.5f, -2.5f, -2.5f, 0.0f };
        sector.extraLight = sed::ColorF{ 10.0f, 10.0f, 10.0f, 0.0f };

        for (const bool clampExtraLight : { false, true })
        {
            sed::assets::JklSaveOptions options{};
            options.clampExtraLight = clampExtraLight;
            std::ostringstream output;
            sed::assets::saveToJKL(level, output, options);
            const std::string text = output.str();
            check(text.find("AMBIENT LIGHT -2.50000000") != std::string::npos,
                "JKL save sector light: signed ambient scalar is emitted exactly");
            check(text.find("EXTRA LIGHT 10.00000000") != std::string::npos,
                "JKL save sector light: HDR extra scalar is emitted exactly");

            std::istringstream input(text);
            const sed::Level roundTrip = sed::assets::loadFromJKL(input);
            check(roundTrip.sectors.size() == 1,
                "JKL save sector light: scalar round-trip keeps one sector");
            if (roundTrip.sectors.size() == 1)
            {
                const sed::Sector& loaded = roundTrip.sectors[0];
                check(close(loaded.ambient.r, -2.5)
                        && close(loaded.ambient.g, -2.5)
                        && close(loaded.ambient.b, -2.5)
                        && close(loaded.ambient.a, 0.0),
                    "JKL save sector light: negative ambient scalar survives reload");
                check(close(loaded.extraLight.r, 10.0)
                        && close(loaded.extraLight.g, 10.0)
                        && close(loaded.extraLight.b, 10.0)
                        && close(loaded.extraLight.a, 0.0),
                    "JKL save sector light: HDR extra scalar survives reload");
            }
        }

        {
            sed::Level extreme = level;
            extreme.sectors[0].ambient = sed::ColorF{
                3.0e38f, 3.0e38f, 3.0e38f, 0.0f };
            extreme.sectors[0].extraLight = sed::ColorF{
                -3.0e38f, -3.0e38f, -3.0e38f, 0.0f };
            sed::assets::JklSaveOptions options{};
            options.clampExtraLight = false;
            std::ostringstream output;
            sed::assets::saveToJKL(extreme, output, options);
            const std::string text = output.str();
            check(text.find("inf") == std::string::npos
                    && text.find("nan") == std::string::npos,
                "JKL save sector light: finite extreme scalars never serialize as nonfinite");

            std::istringstream input(text);
            const sed::Level roundTrip = sed::assets::loadFromJKL(input);
            check(roundTrip.sectors.size() == 1
                    && std::isfinite(roundTrip.sectors[0].ambient.r)
                    && std::isfinite(roundTrip.sectors[0].extraLight.r)
                    && close(static_cast<double>(roundTrip.sectors[0].ambient.r)
                            / 3.0e38, 1.0, 1.0e-6)
                    && close(static_cast<double>(roundTrip.sectors[0].extraLight.r)
                            / -3.0e38, 1.0, 1.0e-6),
                "JKL save sector light: finite extreme scalars survive reload");
        }

        for (const bool ambientField : { false, true })
        {
            for (int channel = 0; channel < 4; ++channel)
            {
                sed::Level invalid = level;
                sed::ColorF& color = ambientField
                    ? invalid.sectors[0].ambient
                    : invalid.sectors[0].extraLight;
                float* component = channel == 0 ? &color.r
                    : channel == 1 ? &color.g
                    : channel == 2 ? &color.b : &color.a;
                *component = (ambientField && channel == 0)
                    ? std::numeric_limits<float>::quiet_NaN()
                    : std::numeric_limits<float>::infinity();

                std::ostringstream output;
                output << "sentinel";
                std::string diagnostic;
                try
                {
                    sed::assets::saveToJKL(invalid, output);
                }
                catch (const std::exception& exception)
                {
                    diagnostic = exception.what();
                }
                check(diagnostic
                        == "Cannot export JKL/NDY sector light: a color value is not finite.",
                    "JKL save sector light: nonfinite scalar has the fixed diagnostic");
                check(output.str() == "sentinel",
                    "JKL save sector light: nonfinite rejection is atomic");
            }
        }
    }

    {
        // IJIM's absent TINT is an all-zero engine state, while a newly
        // authored explicit tint, including a small nonzero channel, must be
        // written and survive a round trip.
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::IJIM;
        level.header.version = 3;
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        sed::Sector& sector = level.newSector();
        sector.tint.setToZero();

        std::ostringstream absentOutput;
        sed::assets::saveToJKL(level, absentOutput);
        const std::string absentText = absentOutput.str();
        check(absentText.find("TINT ") == std::string::npos,
            "JKL save IJIM tint: all-zero tint is omitted");

        sed::assets::JklLoadOptions loadOptions{};
        loadOptions.projectType = sed::ProjectType::IJIM;
        std::istringstream absentInput(absentText);
        const sed::Level absent = sed::assets::loadFromJKL(absentInput, loadOptions);
        check(absent.sectors.size() == 1,
            "JKL save IJIM tint: omitted tint reload keeps one sector");
        if (absent.sectors.size() == 1)
        {
            check(absent.sectors[0].tint.isZero(),
                "JKL save IJIM tint: omitted tint reloads as all-zero");
        }

        sector.tint = sed::ColorF{ 0.00000010f, 0.0f, 0.0f, 0.0f };
        std::ostringstream explicitOutput;
        sed::assets::saveToJKL(level, explicitOutput);
        const std::string explicitText = explicitOutput.str();
        check(explicitText.find("TINT 0.00000010 0.00000000 0.00000000")
                != std::string::npos,
            "JKL save IJIM tint: explicit small RGB is serialized");

        std::istringstream explicitInput(explicitText);
        const sed::Level explicitRoundTrip = sed::assets::loadFromJKL(
            explicitInput, loadOptions);
        check(explicitRoundTrip.sectors.size() == 1,
            "JKL save IJIM tint: explicit tint reload keeps one sector");
        if (explicitRoundTrip.sectors.size() == 1)
        {
            const sed::ColorF& tint = explicitRoundTrip.sectors[0].tint;
            check(tint.r == 0.00000010f && tint.g == 0.0f
                    && tint.b == 0.0f,
                "JKL save IJIM tint: explicit RGB survives reload");
        }
    }

    {
        const auto unique                  = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path root   = "tmp_jkl_save_root_" + unique;
        const std::filesystem::path cogFile = root / "cog" / "test.cog";

        std::error_code ec;
        std::filesystem::create_directories(cogFile.parent_path(), ec);
        check(!ec, "JKL save: create_directories");

        {
            std::ofstream out(cogFile);
            out << "symbols\n";
            out << "    int foo=0\n";
            out << "    float bar=0\n";
            out << "    vector baz=(0/0/0)\n";
            out << "end\n";
        }

        sed::Level level{};
        level.clear();
        level.kind  = sed::ProjectType::JKDF2;
        level.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;

        level.header.version                 = 1;
        level.header.gravity                 = 9.0;
        level.header.ceilingSky.height       = 123.0;
        level.header.ceilingSky.offset       = sed::TPoint2::Create(1.5, 2.5);
        level.header.horizonSky.distance     = 500.0;
        level.header.horizonSky.pixelsPerRev = 999.0;
        level.header.horizonSky.offset       = sed::TPoint2::Create(7.0, 8.0);
        level.header.mipmapDistances         = { 11.0, 22.0, 33.0, 44.0 };
        level.header.lodDistances            = { 1.1, 2.2, 3.3, 4.4 };
        level.header.perspectiveDistance     = 5.5;
        level.header.gouraudDistance         = 6.6;

        level.materials  = { "test.mat" };
        level.colormaps  = { "test.cmp" };
        level.layers     = { "Main", "Alt" };
        level.lvisstring = "lvis test";

        {
            sed::Sector& sector0 = level.newSector();
            sector0.flags               = 0x123 | sed::SECF_CollideBox | sed::SECF_3DO;
            sector0.ambient             = sed::ColorF{ 0.5f, 0.5f, 0.5f, 0.0f };
            sector0.extraLight          = sed::ColorF{ 0.1f, 0.1f, 0.1f, 0.0f };
            sector0.pointLight.color    = sed::ColorF{ 0.11f, 0.22f, 0.33f, 1.0f };
            sector0.pointLight.position = sed::TVector{ 1.0, 2.0, 3.0 };
            sector0.pointLight.minRange = 4.0;
            sector0.pointLight.maxRange = 5.0;
            sector0.sound               = "amb.wav";
            sector0.soundVolume         = 0.5;
            sector0.colormap            = "test.cmp";
            sector0.collideBox          = sed::TBox{ 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
            sector0.tint                = sed::ColorF{ 0.2f, 0.3f, 0.4f, 1.0f };
            sector0.thrust              = sed::TVector{ 7.0, 8.0, 9.0 };
            sector0.layer               = 0;

            sector0.addVertex(0.0, 0.0, 0.0);
            sector0.addVertex(1.0, 0.0, 0.0);
            sector0.addVertex(0.0, 1.0, 0.0);

            sed::Surface s{};
            s.nMat        = 0;
            s.material    = "test.mat";
            s.surfaceFlags = 0x1;
            s.faceFlags   = 0x20u;
            s.geo         = 1;
            s.light       = 2;
            s.tex         = 3;
            s.nAdj        = -1;
            s.extraLight  = sed::ColorF{ -0.25f, -0.25f, -0.25f, 1.0f };
            s.vertices    = { 0, 1, 2 };
            s.texVertices = {
                sed::TexVertex{ 0.0f, 0.0f, sed::ColorF{ 0.1f, 0.1f, 0.1f, 0.1f } },
                sed::TexVertex{ 1.0f, 0.0f, sed::ColorF{ 0.2f, 0.2f, 0.2f, 0.2f } },
                sed::TexVertex{ 0.0f, 1.0f, sed::ColorF{ 0.3f, 0.3f, 0.3f, 0.3f } },
            };
            s.normal      = sed::TVector{ 0.0, 0.0, 1.0 };
            sector0.addSurface(std::move(s));
        }

        {
            sed::Sector& sector1 = level.newSector();
            sector1.flags      = 0;
            sector1.ambient    = sed::ColorF{ 0.25f, 0.25f, 0.25f, 0.0f };
            sector1.extraLight = sed::ColorF{ 0.05f, 0.05f, 0.05f, 0.0f };
            sector1.colormap   = "test.cmp";
            sector1.collideBox = sed::TBox{ 10.0, 11.0, 12.0, 13.0, 14.0, 15.0 };
            sector1.tint       = sed::ColorF{ 0.0f, 0.0f, 0.0f, 1.0f };
            sector1.thrust     = sed::TVector{ 0.0, 0.0, 0.0 };
            sector1.layer      = 1;

            sector1.addVertex(0.0, 0.0, 1.0);
            sector1.addVertex(1.0, 0.0, 1.0);
            sector1.addVertex(0.0, 1.0, 1.0);

            sed::Surface s{};
            s.nMat        = 0;
            s.material    = "test.mat";
            s.surfaceFlags = 0x2;
            s.faceFlags   = 0x40u;
            s.geo         = 9;
            s.light       = 8;
            s.tex         = 7;
            s.nAdj        = -1;
            s.extraLight  = sed::ColorF{ 0.0f, 0.0f, 0.0f, 1.0f };
            s.vertices    = { 0, 1, 2 };
            s.texVertices = {
                sed::TexVertex{ 0.0f, 0.0f, sed::ColorF{ 0.9f, 0.9f, 0.9f, 0.9f } },
                sed::TexVertex{ 1.0f, 0.0f, sed::ColorF{ 0.8f, 0.8f, 0.8f, 0.8f } },
                sed::TexVertex{ 0.0f, 1.0f, sed::ColorF{ 0.7f, 0.7f, 0.7f, 0.7f } },
            };
            s.normal      = sed::TVector{ 0.0, 0.0, 1.0 };
            s.adjoin      = sed::SurfaceRef{ 0, 0 };
            s.adjoinFlags = sed::SAF_Visible;
            sector1.addSurface(std::move(s));
        }

        level.sectors[0].surfaces[0].adjoin      = sed::SurfaceRef{ 1, 0 };
        level.sectors[0].surfaces[0].adjoinFlags = sed::SAF_Move;

        {
            sed::Thing& thing = level.newThing();
            thing.name   = "child";
            thing.x      = 1.0;
            thing.y      = 2.0;
            thing.z      = 3.0;
            thing.pch    = 10.0;
            thing.yaw    = 20.0;
            thing.rol    = 30.0;
            thing.sector = 0;
            thing.layer  = 1;

            sed::TplValue v{};
            v.name        = "health";
            v.type        = sed::ValueType::str;
            v.stringValue = "100";
            thing.values.push_back(std::move(v));
        }

        {
            sed::ThingTemplate tpl{};
            tpl.name = "child";
            tpl.parent = "base";

            sed::TplValue create{};
            create.name = "creatething";
            create.type = sed::ValueType::str;
            create.adjType = sed::AdjType::tpl;
            create.stringValue = "base";
            tpl.values.push_back(std::move(create));

            level.templates.push_back(std::move(tpl));
        }

        {
            sed::ThingTemplate tpl{};
            tpl.name = "base";
            tpl.parent = "none";

            sed::TplValue flags{};
            flags.name = "thingflags";
            flags.type = sed::ValueType::flag;
            flags.intValue = 0x10;
            tpl.values.push_back(std::move(flags));

            level.templates.push_back(std::move(tpl));
        }

        {
            sed::Cog& cog = level.newCog();
            cog.name = "test.cog";

            sed::CogValue foo{};
            foo.type = sed::ValueType::integer;
            foo.intValue = 42;
            cog.values.push_back(std::move(foo));

            sed::CogValue bar{};
            bar.type = sed::ValueType::floating;
            bar.floatValue = 1.5;
            cog.values.push_back(std::move(bar));

            sed::CogValue baz{};
            baz.type = sed::ValueType::vect;
            baz.stringValue = "(1/2/3)";
            cog.values.push_back(std::move(baz));
        }

        {
            sed::SedLight& light = level.newLight();
            light.flags    = 0x1;
            light.layer    = 0;
            light.position = sed::TVector{ 1.0, 2.0, 3.0 };
            light.range    = 4.0;
            light.intensity = 5.0;
            light.color    = sed::ColorF{ 0.25f, 0.5f, 0.75f, 1.0f };
        }

        std::ostringstream oss;
        sed::assets::saveToJKL(level, oss);

        const std::string saved = oss.str();
        check(saved.find("BOUNDBOX ") != std::string::npos, "JKL save: writes BOUNDBOX");
        check(saved.find("CENTER ") != std::string::npos, "JKL save: writes CENTER");
        check(saved.find("RADIUS ") != std::string::npos, "JKL save: writes RADIUS");
        check(saved.find("FLAGS 0x1123") != std::string::npos, "JKL save: clears SECF_3DO on write");

        std::size_t collideCount = 0;
        for (std::size_t pos = saved.find("COLLIDEBOX "); pos != std::string::npos; pos = saved.find("COLLIDEBOX ", pos + 1))
        {
            ++collideCount;
        }
        check(collideCount == 1, "JKL save: COLLIDEBOX written only when flagged");

        std::istringstream iss(saved);
        sed::assets::ResourceLocator locator{};
        locator.addRoot(root);

        sed::assets::JklLoadOptions opts{};
        opts.resources = &locator;

        const sed::Level loaded = sed::assets::loadFromJKL(iss, opts);

        check(loaded.kind == sed::ProjectType::JKDF2, "JKL save: kind");
        check(loaded.header.version == 1, "JKL save: version");
        check(close(loaded.header.gravity, 9.0), "JKL save: gravity");

        check(loaded.materials.size() == 1 && loaded.materials[0] == "test.mat", "JKL save: materials");
        check(loaded.colormaps.size() == 1 && loaded.colormaps[0] == "test.cmp", "JKL save: colormaps");

        check(loaded.sectors.size() == 2, "JKL save: sectors");
        if (loaded.sectors.size() >= 2)
        {
            const sed::Sector& s0 = loaded.sectors[0];
            check(s0.flags == 0x1123, "JKL save: sector0.flags");
            check(s0.sound == "amb.wav", "JKL save: sector0.sound");
            check(close(s0.soundVolume, 0.5), "JKL save: sector0.soundVolume");
            check(s0.colormap == "test.cmp", "JKL save: sector0.colormap");

            check(s0.surfaces.size() == 1, "JKL save: sector0.surfaces");
            if (!s0.surfaces.empty())
            {
                const sed::Surface& surf = s0.surfaces[0];
                check(surf.material == "test.mat", "JKL save: s0.surf0.material");
                check(surf.nMat == 0, "JKL save: s0.surf0.nMat");
                check(surf.nAdj >= 0, "JKL save: s0.surf0.nAdj");
                check(close(surf.extraLight.intensity(), -0.25),
                    "JKL save: signed surface extra light round-trips");
                check(surf.adjoin.has_value(), "JKL save: s0.surf0.adjoin");
                if (surf.adjoin)
                {
                    check(surf.adjoin->sector == 1, "JKL save: s0.surf0.adjoin.sector");
                    check(surf.adjoin->surface == 0, "JKL save: s0.surf0.adjoin.surface");
                }
                check(surf.adjoinFlags == sed::SAF_Move, "JKL save: s0.surf0.adjoinFlags");
            }

            const sed::Sector& s1 = loaded.sectors[1];
            check(s1.surfaces.size() == 1, "JKL save: sector1.surfaces");
            if (!s1.surfaces.empty())
            {
                const sed::Surface& surf = s1.surfaces[0];
                check(surf.adjoin.has_value(), "JKL save: s1.surf0.adjoin");
                if (surf.adjoin)
                {
                    check(surf.adjoin->sector == 0, "JKL save: s1.surf0.adjoin.sector");
                    check(surf.adjoin->surface == 0, "JKL save: s1.surf0.adjoin.surface");
                }
                check(surf.adjoinFlags == sed::SAF_Visible, "JKL save: s1.surf0.adjoinFlags");
            }
        }

        check(loaded.things.size() == 1, "JKL save: things");
        if (!loaded.things.empty())
        {
            const sed::Thing& thing = loaded.things[0];
            check(thing.name == "child", "JKL save: thing.name");
            check(thing.sector == 0, "JKL save: thing.sector");
        }

        check(loaded.lights.size() == 1, "JKL save: lights");
        if (!loaded.lights.empty())
        {
            const sed::SedLight& light = loaded.lights[0];
            check(light.layer == 0, "JKL save: light.layer");
            check(light.flags == 0x1, "JKL save: light.flags");
        }

        check(loaded.layers.size() == 2 && loaded.layers[0] == "Main" && loaded.layers[1] == "Alt", "JKL save: layers");
        check(loaded.lvisstring == "lvis test", "JKL save: lvisstring");

        check(loaded.templates.size() == 2, "JKL save: templates.size");
        if (loaded.templates.size() >= 2)
        {
            const sed::ThingTemplate& base = loaded.templates[0];
            check(base.name == "base", "JKL save: templates[0].name");
            check(base.parent == "none", "JKL save: templates[0].parent");
            check(base.values.size() == 1, "JKL save: templates[0].values.size");
            if (!base.values.empty())
            {
                check(base.values[0].name == "thingflags", "JKL save: templates[0].values[0].name");
                check(base.values[0].type == sed::ValueType::flag, "JKL save: templates[0].values[0].type");
                check(base.values[0].intValue == 0x10, "JKL save: templates[0].values[0].intValue");
            }

            const sed::ThingTemplate& child = loaded.templates[1];
            check(child.name == "child", "JKL save: templates[1].name");
            check(child.parent == "base", "JKL save: templates[1].parent");
            check(child.values.size() == 1, "JKL save: templates[1].values.size");
            if (!child.values.empty())
            {
                check(child.values[0].name == "creatething", "JKL save: templates[1].values[0].name");
                check(child.values[0].adjType == sed::AdjType::tpl, "JKL save: templates[1].values[0].adjType");
                check(child.values[0].stringValue == "base", "JKL save: templates[1].values[0].stringValue");
            }
        }

        check(loaded.cogs.size() == 1, "JKL save: cogs.size");
        if (!loaded.cogs.empty())
        {
            const sed::Cog& cog = loaded.cogs[0];
            check(cog.name == "test.cog", "JKL save: cogs[0].name");
            check(cog.values.size() == 3, "JKL save: cogs[0].values.size");
            if (cog.values.size() >= 3)
            {
                check(cog.values[0].name == "foo", "JKL save: cogs[0].values[0].name");
                check(cog.values[0].type == sed::ValueType::integer, "JKL save: cogs[0].values[0].type");
                check(cog.values[0].intValue == 42, "JKL save: cogs[0].values[0].intValue");

                check(cog.values[1].name == "bar", "JKL save: cogs[0].values[1].name");
                check(cog.values[1].type == sed::ValueType::floating, "JKL save: cogs[0].values[1].type");
                check(close(cog.values[1].floatValue, 1.5), "JKL save: cogs[0].values[1].floatValue");

                check(cog.values[2].name == "baz", "JKL save: cogs[0].values[2].name");
                check(cog.values[2].type == sed::ValueType::vect, "JKL save: cogs[0].values[2].type");
                check(cog.values[2].stringValue == "(1/2/3)", "JKL save: cogs[0].values[2].stringValue");
            }
        }

        std::filesystem::remove_all(root, ec);
    }

    {
        const auto unique = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path root =
            "tmp_jkl_uv_boundary_" + unique;
        const std::filesystem::path matPath =
            root / "mat" / "uv_boundary.mat";
        std::error_code ec{};
        std::filesystem::create_directories(matPath.parent_path(), ec);
        check(!ec && writeBytes(matPath, makeIndexedMat(32, 16)),
            "JKL/NDY UV boundary fixture writes a 32x16 MAT");

        const std::filesystem::path zeroWidthMatPath =
            root / "mat" / "zero_width.mat";
        const std::filesystem::path zeroHeightMatPath =
            root / "mat" / "zero_height.mat";
        const std::filesystem::path malformedMatPath =
            root / "mat" / "malformed.mat";
        check(writeBytes(zeroWidthMatPath, makeIndexedMat(0, 16))
                && writeBytes(zeroHeightMatPath, makeIndexedMat(32, 0))
                && writeBytes(malformedMatPath,
                    std::vector<std::byte>{
                        std::byte{ 'M' },
                        std::byte{ 'A' },
                        std::byte{ 'T' },
                        std::byte{ ' ' } }),
            "JKL/NDY UV boundary fixture writes dimensionless and malformed MATs");

        sed::assets::ResourceLocator locator{};
        locator.addRoot(root);
        check(!sed::assets::resolveMaterialTextureDimensions(
                    locator, "zero_width.mat")
                && !sed::assets::resolveMaterialTextureDimensions(
                    locator, "zero_height.mat")
                && !sed::assets::resolveMaterialTextureDimensions(
                    locator, "malformed.mat")
                && !sed::assets::resolveMaterialTextureDimensions(
                    locator, "missing.mat")
                && !sed::assets::resolveMaterialTextureDimensions(
                    locator, "none")
                && !sed::assets::resolveMaterialTextureDimensions(
                    locator, ""),
            "MAT UV dimensions: zero, malformed, missing, none, and empty resources never invent a texture size");
        sed::assets::JklSaveOptions saveOptions{};
        saveOptions.resources = &locator;

        const std::array<sed::TPoint2, 3> normalizedUvs{
            sed::TPoint2{ 0.0, 0.0 },
            sed::TPoint2{ 0.25, 0.5 },
            sed::TPoint2{ 0.75, 0.25 },
        };

        const sed::Level classic = makeUvBoundaryLevel(
            sed::ProjectType::JKDF2);
        std::ostringstream jklOutput{};
        sed::assets::saveToJKL(classic, jklOutput, saveOptions);
        const std::string jklText = jklOutput.str();
        check(jklText.find("8.00000000 8.00000000")
                    != std::string::npos
                && jklText.find("24.00000000 4.00000000")
                    != std::string::npos,
            "JKL UV boundary expands normalized Level coordinates to 32x16 pixel-space wire values");

        sed::assets::JklLoadOptions classicLoadOptions{};
        classicLoadOptions.projectType = sed::ProjectType::JKDF2;
        classicLoadOptions.resources = &locator;
        std::string overflowJklText = jklText;
        const std::size_t overflowJklUv = overflowJklText.find(
            "8.00000000 8.00000000");
        if (overflowJklUv != std::string::npos)
        {
            overflowJklText.replace(
                overflowJklUv,
                std::string_view("8.00000000").size(),
                "3.5e38");
        }
        bool overflowJklRejected = false;
        try
        {
            std::istringstream overflowJklInput(overflowJklText);
            (void)sed::assets::loadFromJKL(
                overflowJklInput, classicLoadOptions);
        }
        catch (const std::runtime_error& exception)
        {
            overflowJklRejected = std::string_view(exception.what()).find(
                "texture vertex") != std::string_view::npos;
        }
        check(overflowJklUv != std::string::npos && overflowJklRejected,
            "JKL loader rejects finite texture-coordinate doubles that overflow float storage");

        std::istringstream jklInput(jklText);
        const sed::Level classicRoundTrip = sed::assets::loadFromJKL(
            jklInput, classicLoadOptions);
        check(classicRoundTrip.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && hasUvValues(classicRoundTrip, normalizedUvs),
            "JKL UV boundary normalizes pixel-space wire values while MAT dimensions are available");

        const sed::Level mots = makeUvBoundaryLevel(
            sed::ProjectType::MOTS);
        std::ostringstream motsOutput{};
        sed::assets::saveToJKL(mots, motsOutput, saveOptions);
        const std::string motsText = motsOutput.str();
        check(motsText.find("8.00000000 8.00000000")
                    != std::string::npos
                && motsText.find("24.00000000 4.00000000")
                    != std::string::npos,
            "MotS JKL UV boundary expands normalized coordinates through real anisotropic MAT dimensions");

        sed::assets::JklLoadOptions motsLoadOptions = classicLoadOptions;
        motsLoadOptions.projectType = sed::ProjectType::MOTS;
        std::istringstream motsInput(motsText);
        const sed::Level motsRoundTrip = sed::assets::loadFromJKL(
            motsInput, motsLoadOptions);
        check(motsRoundTrip.kind == sed::ProjectType::MOTS
                && motsRoundTrip.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && hasUvValues(motsRoundTrip, normalizedUvs),
            "MotS JKL UV boundary retains caller project identity and normalizes classic pixel coordinates");

        sed::assets::JklLoadOptions classicMissingResourceOptions{};
        classicMissingResourceOptions.projectType = sed::ProjectType::JKDF2;
        std::istringstream jklMissingResourceInput(jklText);
        const sed::Level classicWithoutDimensions = sed::assets::loadFromJKL(
            jklMissingResourceInput, classicMissingResourceOptions);
        const std::array<sed::TPoint2, 3> pixelUvs{
            sed::TPoint2{ 0.0, 0.0 },
            sed::TPoint2{ 8.0, 8.0 },
            sed::TPoint2{ 24.0, 4.0 },
        };
        check(classicWithoutDimensions.uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels
                && hasUvValues(classicWithoutDimensions, pixelUvs),
            "JKL UV boundary preserves and tags pixel values when MAT dimensions are unavailable");

        // A surface with no material is valid in JKDF2/MotS.  Its authored
        // coordinates have no MAT dimensions to apply, but it must not block
        // normalization of neighboring textured surfaces.  The no-material
        // values remain unchanged while the resolved surface uses 32x16.
        for (const auto projectType : {sed::ProjectType::JKDF2, sed::ProjectType::MOTS})
        {
            std::vector<std::string> mixedWarnings{};
            sed::assets::JklLoadOptions mixedLoadOptions{};
            mixedLoadOptions.projectType = projectType;
            mixedLoadOptions.resources = &locator;
            mixedLoadOptions.onDiagnostic = [&mixedWarnings](
                const sed::DiagnosticSeverity severity,
                const std::string_view message)
            {
                if (severity == sed::DiagnosticSeverity::warning)
                {
                    mixedWarnings.emplace_back(message);
                }
            };
            std::istringstream mixedInput(makeJklUvNormalizationFixture("-1"));
            const sed::Level mixed = sed::assets::loadFromJKL(
                mixedInput, mixedLoadOptions);
            bool mixedValuesArePreserved = false;
            if (mixed.sectors.size() == 1u
                && mixed.sectors[0].surfaces.size() == 2u)
            {
                const sed::Surface& resolved = mixed.sectors[0].surfaces[0];
                const sed::Surface& materialLess = mixed.sectors[0].surfaces[1];
                mixedValuesArePreserved = resolved.texVertices.size() == 3u
                    && materialLess.texVertices.size() == 4u
                    && close(resolved.texVertices[1].u, 0.25)
                    && close(resolved.texVertices[1].v, 0.5)
                    && close(materialLess.texVertices[0].u, 0.0)
                    && close(materialLess.texVertices[0].v, 0.5)
                    && close(materialLess.texVertices[1].u, 1.0)
                    && close(materialLess.texVertices[1].v, 0.5)
                    && close(materialLess.texVertices[2].u, 1.0)
                    && close(materialLess.texVertices[2].v, 0.0)
                    && close(materialLess.texVertices[3].u, 0.0)
                    && close(materialLess.texVertices[3].v, 0.0)
                    && close(materialLess.uScale, 1.0 / 640.0)
                    && close(materialLess.vScale, 1.0 / 640.0)
                    && close(materialLess.texVertices[0].color.r, 1.0)
                    && close(materialLess.texVertices[0].color.g, 1.0)
                    && close(materialLess.texVertices[0].color.b, 1.0)
                    && close(materialLess.texVertices[1].color.r, 2.0)
                    && close(materialLess.texVertices[1].color.g, 2.0)
                    && close(materialLess.texVertices[1].color.b, 2.0)
                    && close(materialLess.texVertices[2].color.r, 3.0)
                    && close(materialLess.texVertices[2].color.g, 3.0)
                    && close(materialLess.texVertices[2].color.b, 3.0)
                    && close(materialLess.texVertices[3].color.r, 4.0)
                    && close(materialLess.texVertices[3].color.g, 4.0)
                    && close(materialLess.texVertices[3].color.b, 4.0)
                    && close(materialLess.texVertices[0].color.a, 1.0)
                    && close(materialLess.texVertices[1].color.a, 2.0)
                    && close(materialLess.texVertices[2].color.a, 3.0)
                    && close(materialLess.texVertices[3].color.a, 4.0);
            }
            check(mixed.uvCoordinateSpace == sed::UvCoordinateSpace::Normalized
                    && mixedValuesArePreserved
                    && mixedWarnings.empty(),
                projectType == sed::ProjectType::JKDF2
                    ? "JKDF2 JKL UV normalization keeps material-less surface data"
                    : "MotS JKL UV normalization keeps material-less surface data");
        }

        // The export boundary has the same exception: a normalized level may
        // contain a material-less surface whose authored UVs must pass through
        // unchanged while neighboring textured surfaces use their MAT size.
        for (const auto projectType : {sed::ProjectType::JKDF2, sed::ProjectType::MOTS})
        {
            sed::Level mixedExport = makeUvBoundaryLevel(projectType);
            sed::Surface materialLess = mixedExport.sectors[0].surfaces[0];
            materialLess.nMat = -1;
            materialLess.material.clear();
            materialLess.texVertices[0].u = 7.0f;
            materialLess.texVertices[0].v = 9.0f;
            mixedExport.sectors[0].addSurface(std::move(materialLess));
            bool exported = true;
            std::string exportedText{};
            try
            {
                std::ostringstream output{};
                sed::assets::JklSaveOptions exportOptions{};
                exportOptions.resources = &locator;
                sed::assets::saveToJKL(mixedExport, output, exportOptions);
                exportedText = output.str();
            }
            catch (const std::exception&)
            {
                exported = false;
            }
            check(exported,
                projectType == sed::ProjectType::JKDF2
                    ? "JKDF2 JKL export accepts material-less normalized UVs"
                    : "MotS JKL export accepts material-less normalized UVs");
            if (exported)
            {
                sed::assets::JklLoadOptions reloadOptions{};
                reloadOptions.projectType = projectType;
                std::istringstream reloadInput(exportedText);
                const sed::Level reloaded = sed::assets::loadFromJKL(
                    reloadInput, reloadOptions);
                check(reloaded.sectors.size() == 1u
                        && reloaded.sectors[0].surfaces.size() == 2u
                        && reloaded.sectors[0].surfaces[1].material.empty()
                        && close(reloaded.sectors[0].surfaces[1].texVertices[0].u, 7.0)
                        && close(reloaded.sectors[0].surfaces[1].texVertices[0].v, 9.0),
                    projectType == sed::ProjectType::JKDF2
                        ? "JKDF2 JKL export preserves material-less UV values"
                        : "MotS JKL export preserves material-less UV values");
            }
        }

        // A normalized all-no-material face must bypass MAT lookup even when
        // an imported stale material index is present and the spelling varies
        // in case or surrounding whitespace.
        sed::Level noMaterialExport = makeUvBoundaryLevel(sed::ProjectType::JKDF2);
        sed::Surface& noMaterialSurface = noMaterialExport.sectors[0].surfaces[0];
        noMaterialSurface.nMat = 99;
        noMaterialSurface.material = " NoNe ";
        noMaterialSurface.texVertices[0].u = 0.125f;
        noMaterialSurface.texVertices[0].v = -0.75f;
        noMaterialSurface.texVertices[1].u = 1.5f;
        noMaterialSurface.texVertices[1].v = 2.25f;
        noMaterialSurface.texVertices[2].u = -2.0f;
        noMaterialSurface.texVertices[2].v = 0.5f;
        std::ostringstream noMaterialOutput{};
        bool noMaterialExported = true;
        try
        {
            sed::assets::JklSaveOptions noMaterialSaveOptions{};
            sed::assets::saveToJKL(
                noMaterialExport, noMaterialOutput, noMaterialSaveOptions);
        }
        catch (const std::exception&)
        {
            noMaterialExported = false;
        }
        sed::assets::JklLoadOptions noMaterialLoadOptions{};
        noMaterialLoadOptions.projectType = sed::ProjectType::JKDF2;
        std::istringstream noMaterialInput(noMaterialOutput.str());
        const sed::Level noMaterialRoundTrip = sed::assets::loadFromJKL(
            noMaterialInput, noMaterialLoadOptions);
        check(noMaterialExported
                && noMaterialRoundTrip.uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels
                && noMaterialRoundTrip.sectors.size() == 1u
                && noMaterialRoundTrip.sectors[0].surfaces.size() == 1u
                && noMaterialRoundTrip.sectors[0].surfaces[0].texVertices.size() == 3u
                && noMaterialRoundTrip.sectors[0].surfaces[0].material.empty()
                && close(noMaterialRoundTrip.sectors[0].surfaces[0].texVertices[0].u, 0.125)
                && close(noMaterialRoundTrip.sectors[0].surfaces[0].texVertices[0].v, -0.75)
                && close(noMaterialRoundTrip.sectors[0].surfaces[0].texVertices[1].u, 1.5)
                && close(noMaterialRoundTrip.sectors[0].surfaces[0].texVertices[1].v, 2.25)
                && close(noMaterialRoundTrip.sectors[0].surfaces[0].texVertices[2].u, -2.0)
                && close(noMaterialRoundTrip.sectors[0].surfaces[0].texVertices[2].v, 0.5),
            "JKL normalized all-no-material export bypasses stale MAT lookup and preserves raw UVs");

        for (const auto projectType : {sed::ProjectType::JKDF2, sed::ProjectType::MOTS})
        {
            std::string zeroText = makeJklUvNormalizationFixture("-1");
            const std::string authoredTextureVertices =
                "3: 7 9\n        4: 11 13\n        5: -3 5\n        6: 17 -2";
            const std::string zeroTextureVertices =
                "3: 0 0\n        4: 0 0\n        5: 0 0\n        6: 0 0";
            const std::size_t zeroOffset = zeroText.find(authoredTextureVertices);
            if (zeroOffset != std::string::npos)
            {
                zeroText.replace(zeroOffset, authoredTextureVertices.size(), zeroTextureVertices);
            }
            sed::assets::JklLoadOptions zeroOptions{};
            zeroOptions.projectType = projectType;
            zeroOptions.resources = &locator;
            std::istringstream zeroInput(zeroText);
            const sed::Level zeroLevel = sed::assets::loadFromJKL(zeroInput, zeroOptions);
            bool zeroPreserved = false;
            if (zeroLevel.sectors.size() == 1u && zeroLevel.sectors[0].surfaces.size() == 2u)
            {
                const sed::Surface& surface = zeroLevel.sectors[0].surfaces[1];
                zeroPreserved = std::all_of(
                    surface.texVertices.cbegin(),
                    surface.texVertices.cend(),
                    [](const sed::TexVertex& vertex)
                    {
                        return close(vertex.u, 0.0) && close(vertex.v, 0.0);
                    });
            }
            check(zeroLevel.uvCoordinateSpace == sed::UvCoordinateSpace::Normalized
                    && zeroPreserved,
                projectType == sed::ProjectType::JKDF2
                    ? "JKDF2 JKL zero material-less UVs remain zero"
                    : "MotS JKL zero material-less UVs remain zero");

            std::string repeatedCornerText = makeJklUvNormalizationFixture("-1");
            const std::string regularRectangleSurface =
                "1: -1 0x0 0x0 0 3 3 -1 0 4 3,3 4,4 5,5 6,6 1 2 3 4";
            const std::string repeatedCornerSurface =
                "1: -1 0x0 0x0 0 3 3 -1 0 5 3,3 3,3 4,4 5,5 6,6 1 2 3 4 5";
            const std::size_t repeatedCornerOffset = repeatedCornerText.find(
                regularRectangleSurface);
            check(repeatedCornerOffset != std::string::npos,
                "JKL repeated-corner fixture finds the complete surface record");
            if (repeatedCornerOffset != std::string::npos)
            {
                repeatedCornerText.replace(
                    repeatedCornerOffset,
                    regularRectangleSurface.size(),
                    repeatedCornerSurface);
            }
            sed::assets::JklLoadOptions repeatedCornerOptions{};
            repeatedCornerOptions.projectType = projectType;
            repeatedCornerOptions.resources = &locator;
            std::istringstream repeatedCornerInput(repeatedCornerText);
            const sed::Level repeatedCornerLevel = sed::assets::loadFromJKL(
                repeatedCornerInput, repeatedCornerOptions);
            bool repeatedCornerSafe = false;
            if (repeatedCornerLevel.sectors.size() == 1u
                && repeatedCornerLevel.sectors[0].surfaces.size() == 2u)
            {
                const sed::Surface& surface = repeatedCornerLevel.sectors[0].surfaces[1];
                repeatedCornerSafe = surface.texVertices.size() == 5u
                    && close(surface.texVertices[0].u, 0.0)
                    && close(surface.texVertices[0].v, 0.5)
                    && close(surface.texVertices[1].u, 0.0)
                    && close(surface.texVertices[1].v, 0.5)
                    && close(surface.texVertices[2].u, 1.0)
                    && close(surface.texVertices[2].v, 0.5)
                    && close(surface.texVertices[3].u, 1.0)
                    && close(surface.texVertices[3].v, 0.0)
                    && close(surface.texVertices[4].u, 0.0)
                    && close(surface.texVertices[4].v, 0.0)
                    && close(surface.uScale, 1.0 / 640.0)
                    && close(surface.vScale, 1.0 / 640.0)
                    && (surface.faceFlags & sed::FF_SF_FLIP) != 0u;
            }
            check(repeatedCornerLevel.uvCoordinateSpace == sed::UvCoordinateSpace::Normalized
                    && repeatedCornerSafe,
                projectType == sed::ProjectType::JKDF2
                    ? "JKDF2 JKL material-less rectangle scans past a repeated first corner"
                    : "MotS JKL material-less rectangle scans past a repeated first corner");

            std::string degenerateText = makeJklUvNormalizationFixture("-1");
            const std::string rectangleVertices =
                "4: 2 0 1\n        5: 2 1 1\n        6: 0 1 1";
            const std::string collapsedVertices =
                "4: 0 0 1\n        5: 0 0 1\n        6: 0 0 1";
            const std::size_t degenerateOffset = degenerateText.find(rectangleVertices);
            if (degenerateOffset != std::string::npos)
            {
                degenerateText.replace(
                    degenerateOffset,
                    rectangleVertices.size(),
                    collapsedVertices);
            }
            const std::string normalFaceFlags = "1: -1 0x0 0x0 0 3 3 -1 0 4";
            const std::string flippedFaceFlags = "1: -1 0x0 0x80000000 0 3 3 -1 0 4";
            const std::size_t faceFlagsOffset = degenerateText.find(normalFaceFlags);
            check(faceFlagsOffset != std::string::npos,
                "JKL degenerate fixture finds the complete surface prefix");
            if (faceFlagsOffset != std::string::npos)
            {
                degenerateText.replace(
                    faceFlagsOffset,
                    normalFaceFlags.size(),
                    flippedFaceFlags);
            }
            sed::assets::JklLoadOptions degenerateOptions{};
            degenerateOptions.projectType = projectType;
            degenerateOptions.resources = &locator;
            std::istringstream degenerateInput(degenerateText);
            const sed::Level degenerateLevel = sed::assets::loadFromJKL(
                degenerateInput, degenerateOptions);
            bool degenerateSafe = false;
            if (degenerateLevel.sectors.size() == 1u
                && degenerateLevel.sectors[0].surfaces.size() == 2u)
            {
                const sed::Surface& surface = degenerateLevel.sectors[0].surfaces[1];
                degenerateSafe = surface.texVertices.size() == 4u
                    && std::all_of(
                    surface.texVertices.cbegin(),
                    surface.texVertices.cend(),
                    [](const sed::TexVertex& vertex)
                    {
                        return close(vertex.u, 0.0) && close(vertex.v, 0.0);
                    });
                degenerateSafe = degenerateSafe
                    && close(surface.uScale, 1.0)
                    && close(surface.vScale, 1.0)
                    && (surface.faceFlags & sed::FF_SF_FLIP) == 0u
                    && close(surface.texVertices[0].color.r, 1.0)
                    && close(surface.texVertices[0].color.g, 1.0)
                    && close(surface.texVertices[0].color.b, 1.0)
                    && close(surface.texVertices[0].color.a, 1.0)
                    && close(surface.texVertices[1].color.r, 2.0)
                    && close(surface.texVertices[1].color.g, 2.0)
                    && close(surface.texVertices[1].color.b, 2.0)
                    && close(surface.texVertices[1].color.a, 2.0)
                    && close(surface.texVertices[2].color.r, 3.0)
                    && close(surface.texVertices[2].color.g, 3.0)
                    && close(surface.texVertices[2].color.b, 3.0)
                    && close(surface.texVertices[2].color.a, 3.0)
                    && close(surface.texVertices[3].color.r, 4.0)
                    && close(surface.texVertices[3].color.g, 4.0)
                    && close(surface.texVertices[3].color.b, 4.0)
                    && close(surface.texVertices[3].color.a, 4.0);
            }
            check(degenerateLevel.uvCoordinateSpace == sed::UvCoordinateSpace::Normalized
                    && degenerateSafe,
                projectType == sed::ProjectType::JKDF2
                    ? "JKDF2 JKL degenerate material-less UVs use a safe zero island"
                    : "MotS JKL degenerate material-less UVs use a safe zero island");
        }

        // An unresolved named MAT must keep the complete conversion atomic,
        // and the warning must identify the MAT filename and exact location.
        std::vector<std::string> missingMatWarnings{};
        sed::assets::JklLoadOptions missingMatOptions{};
        missingMatOptions.projectType = sed::ProjectType::JKDF2;
        missingMatOptions.resources = &locator;
        missingMatOptions.onDiagnostic = [&missingMatWarnings](
            const sed::DiagnosticSeverity severity,
            const std::string_view message)
        {
            if (severity == sed::DiagnosticSeverity::warning)
            {
                missingMatWarnings.emplace_back(message);
            }
        };
        std::istringstream missingMatInput(makeJklUvNormalizationFixture(
            "-1", "missing_from_jkl_uv_test.mat"));
        const sed::Level missingMat = sed::assets::loadFromJKL(
            missingMatInput, missingMatOptions);
        check(missingMat.uvCoordinateSpace == sed::UvCoordinateSpace::TexturePixels
                && missingMatWarnings.size() == 1u
                && missingMat.sectors.size() == 1u
                && missingMat.sectors[0].surfaces.size() == 2u
                && missingMat.sectors[0].surfaces[1].texVertices.size() == 4u
                && close(missingMat.sectors[0].surfaces[1].texVertices[0].u, 7.0)
                && close(missingMat.sectors[0].surfaces[1].texVertices[0].v, 9.0)
                && missingMatWarnings.front()
                    == "JKL texture coordinates remain in legacy pixel space because the referenced MAT dimensions could not be resolved for material 'missing_from_jkl_uv_test.mat' at sector 0, surface 0.",
            "JKL UV normalization warning names the unresolved MAT and location");

        // A malformed positive or negative material index is not equivalent to
        // an explicit -1/none surface.  It must not silently authorize a
        // partial normalized Level.
        const std::array<std::tuple<std::string_view, std::string_view>, 3> invalidMaterialIndices{
            std::tuple<std::string_view, std::string_view>{
                "99", "world material index is outside the 1-entry material table"},
            std::tuple<std::string_view, std::string_view>{
                "-2", "negative indices are reserved for material-less surfaces"},
            std::tuple<std::string_view, std::string_view>{
                "32867", "static material index 99 is not defined for this project"},
        };
        for (const auto [invalidIndex, reason] : invalidMaterialIndices)
        {
            std::vector<std::string> invalidIndexWarnings{};
            sed::assets::JklLoadOptions invalidIndexOptions{};
            invalidIndexOptions.projectType = sed::ProjectType::MOTS;
            invalidIndexOptions.resources = &locator;
            invalidIndexOptions.onDiagnostic = [&invalidIndexWarnings](
                const sed::DiagnosticSeverity severity,
                const std::string_view message)
            {
                if (severity == sed::DiagnosticSeverity::warning)
                {
                    invalidIndexWarnings.emplace_back(message);
                }
            };
            std::istringstream invalidIndexInput(
                makeJklUvNormalizationFixture(invalidIndex));
            const sed::Level invalidIndexLevel = sed::assets::loadFromJKL(
                invalidIndexInput, invalidIndexOptions);
            check(invalidIndexLevel.uvCoordinateSpace
                        == sed::UvCoordinateSpace::TexturePixels
                    && invalidIndexLevel.sectors.size() == 1u
                    && invalidIndexLevel.sectors[0].surfaces.size() == 2u
                    && close(invalidIndexLevel.sectors[0].surfaces[0]
                            .texVertices[1].u,
                        8.0)
                    && close(invalidIndexLevel.sectors[0].surfaces[1]
                            .texVertices[0].u,
                        7.0)
                    && invalidIndexWarnings.size() == 1u
                    && invalidIndexWarnings.front()
                        == "JKL texture coordinates remain in legacy pixel space because raw material index "
                            + std::string(invalidIndex)
                            + " is invalid at sector 0, surface 1: "
                            + std::string(reason) + ".",
                invalidIndex == "99"
                    ? "MotS JKL invalid positive material index blocks UV normalization with an exact diagnostic"
                    : invalidIndex == "-2"
                        ? "MotS JKL invalid negative material index blocks UV normalization with an exact diagnostic"
                        : "MotS JKL invalid static material index blocks UV normalization with an exact diagnostic");
        }

        const std::array<sed::TPoint2, 3> tiledNormalizedUvs{
            sed::TPoint2{ -0.5, -0.25 },
            sed::TPoint2{ 1.25, 1.5 },
            sed::TPoint2{ 3.0, -0.5 },
        };
        sed::Level tiledClassic = makeUvBoundaryLevel(
            sed::ProjectType::JKDF2);
        for ( std::size_t index = 0; index < tiledNormalizedUvs.size(); ++index )
        {
            tiledClassic.sectors[0].surfaces[0].texVertices[index].u =
                static_cast<float>(tiledNormalizedUvs[index].x);
            tiledClassic.sectors[0].surfaces[0].texVertices[index].v =
                static_cast<float>(tiledNormalizedUvs[index].y);
        }
        std::ostringstream tiledJklOutput{};
        sed::assets::saveToJKL(tiledClassic, tiledJklOutput, saveOptions);
        const std::string tiledJklText = tiledJklOutput.str();
        check(tiledJklText.find("-16.00000000 -4.00000000")
                    != std::string::npos
                && tiledJklText.find("96.00000000 -8.00000000")
                    != std::string::npos,
            "JKL UV boundary expands signed and multi-tile normalized coordinates on export");
        std::istringstream tiledJklInput(tiledJklText);
        const sed::Level tiledClassicRoundTrip = sed::assets::loadFromJKL(
            tiledJklInput, classicLoadOptions);
        check(tiledClassicRoundTrip.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && hasUvValues(tiledClassicRoundTrip, tiledNormalizedUvs),
            "JKL UV boundary round-trips signed and multi-tile normalized coordinates");

        const std::filesystem::path laterMatPath =
            root / "mat" / "later.mat";
        check(writeBytes(laterMatPath, makeIndexedMat(20, 10)),
            "JKL UV atomicity fixture writes the later surface MAT");
        sed::Level multiSurface = makeUvBoundaryLevel(
            sed::ProjectType::JKDF2);
        multiSurface.materials.push_back("later.mat");
        sed::Surface laterSurface =
            multiSurface.sectors[0].surfaces[0];
        laterSurface.nMat = 1;
        laterSurface.material = "later.mat";
        laterSurface.texVertices[0].u = -0.5f;
        laterSurface.texVertices[0].v = 1.5f;
        laterSurface.texVertices[1].u = 2.0f;
        laterSurface.texVertices[1].v = -0.25f;
        multiSurface.sectors[0].addSurface(std::move(laterSurface));

        std::ostringstream multiSurfaceOutput{};
        sed::assets::saveToJKL(
            multiSurface, multiSurfaceOutput, saveOptions);
        std::filesystem::remove(laterMatPath, ec);

        std::vector<std::string> atomicDiagnostics{};
        sed::assets::JklLoadOptions atomicLoadOptions = classicLoadOptions;
        atomicLoadOptions.onDiagnostic = [&atomicDiagnostics](
            const sed::DiagnosticSeverity,
            const std::string_view message)
        {
            atomicDiagnostics.emplace_back(message);
        };
        std::istringstream multiSurfaceInput(multiSurfaceOutput.str());
        const sed::Level unresolvedMultiSurface =
            sed::assets::loadFromJKL(
                multiSurfaceInput, atomicLoadOptions);
        check(unresolvedMultiSurface.uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels
                && unresolvedMultiSurface.sectors.size() == 1u
                && unresolvedMultiSurface.sectors[0].surfaces.size() == 2u
                && close(unresolvedMultiSurface.sectors[0].surfaces[0]
                        .texVertices[1].u,
                    8.0)
                && close(unresolvedMultiSurface.sectors[0].surfaces[0]
                        .texVertices[1].v,
                    8.0)
                && close(unresolvedMultiSurface.sectors[0].surfaces[1]
                        .texVertices[0].u,
                    -10.0)
                && close(unresolvedMultiSurface.sectors[0].surfaces[1]
                        .texVertices[0].v,
                    15.0)
                && atomicDiagnostics.size() == 1u
                && atomicDiagnostics.front()
                    == "JKL texture coordinates remain in legacy pixel space because the referenced MAT dimensions could not be resolved for material 'later.mat' at sector 0, surface 1.",
            "JKL UV boundary leaves every earlier surface in pixel space when a later MAT cannot resolve");

        const sed::Level ijim = makeUvBoundaryLevel(sed::ProjectType::IJIM);
        std::ostringstream ndyOutput{};
        sed::assets::saveToJKL(ijim, ndyOutput, saveOptions);
        const std::string ndyText = ndyOutput.str();
        check(ndyText.find("0.25000000 0.50000000")
                    != std::string::npos
                && ndyText.find("0.75000000 0.25000000")
                    != std::string::npos,
            "NDY UV boundary writes canonical normalized Level coordinates directly");

        sed::assets::JklLoadOptions ijimLoadOptions{};
        ijimLoadOptions.projectType = sed::ProjectType::IJIM;
        ijimLoadOptions.resources = &locator;
        std::string overflowNdyText = ndyText;
        const std::size_t overflowNdyUv = overflowNdyText.find(
            "0.25000000 0.50000000");
        if (overflowNdyUv != std::string::npos)
        {
            overflowNdyText.replace(
                overflowNdyUv,
                std::string_view("0.25000000").size(),
                "-3.5e38");
        }
        bool overflowNdyRejected = false;
        try
        {
            std::istringstream overflowNdyInput(overflowNdyText);
            (void)sed::assets::loadFromJKL(
                overflowNdyInput, ijimLoadOptions);
        }
        catch (const std::runtime_error& exception)
        {
            overflowNdyRejected = std::string_view(exception.what()).find(
                "texture vertex") != std::string_view::npos;
        }
        check(overflowNdyUv != std::string::npos && overflowNdyRejected,
            "NDY loader rejects finite texture-coordinate doubles that overflow float storage");

        std::istringstream ndyInput(ndyText);
        const sed::Level ijimRoundTrip = sed::assets::loadFromJKL(
            ndyInput, ijimLoadOptions);
        check(ijimRoundTrip.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && hasUvValues(ijimRoundTrip, normalizedUvs),
            "NDY UV boundary retains normalized wire values as canonical Level coordinates");

        sed::Level tiledIjim = makeUvBoundaryLevel(sed::ProjectType::IJIM);
        for ( std::size_t index = 0; index < tiledNormalizedUvs.size(); ++index )
        {
            tiledIjim.sectors[0].surfaces[0].texVertices[index].u =
                static_cast<float>(tiledNormalizedUvs[index].x);
            tiledIjim.sectors[0].surfaces[0].texVertices[index].v =
                static_cast<float>(tiledNormalizedUvs[index].y);
        }
        std::ostringstream tiledNdyOutput{};
        sed::assets::saveToJKL(tiledIjim, tiledNdyOutput, saveOptions);
        const std::string tiledNdyText = tiledNdyOutput.str();
        check(tiledNdyText.find("-0.50000000 -0.25000000")
                    != std::string::npos
                && tiledNdyText.find("1.25000000 1.50000000")
                    != std::string::npos
                && tiledNdyText.find("3.00000000 -0.50000000")
                    != std::string::npos,
            "NDY UV boundary preserves signed and multi-tile normalized coordinates without clamping or wrapping them");
        std::istringstream tiledNdyInput(tiledNdyText);
        const sed::Level tiledIjimRoundTrip = sed::assets::loadFromJKL(
            tiledNdyInput, ijimLoadOptions);
        check(hasUvValues(tiledIjimRoundTrip, tiledNormalizedUvs),
            "NDY UV boundary round-trips signed and multi-tile coordinates through normalized wire values");

        const std::vector<std::byte> classicNativeBytes =
            sed::assets::saveToSedBytes(tiledClassic);
        const sed::Level classicFromNative =
            sed::assets::loadFromSedBytes(classicNativeBytes);
        std::ostringstream classicFromNativeWire{};
        sed::assets::saveToJKL(
            classicFromNative,
            classicFromNativeWire,
            saveOptions);
        std::istringstream classicFromNativeInput(
            classicFromNativeWire.str());
        const sed::Level classicAfterAllBoundaries =
            sed::assets::loadFromJKL(
                classicFromNativeInput,
                classicLoadOptions);
        check(classicNativeBytes.size() >= 8u
                && classicNativeBytes[4] == std::byte{ 2 }
                && classicFromNative.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && classicFromNativeWire.str().find(
                    "-16.00000000 -4.00000000") != std::string::npos
                && classicAfterAllBoundaries.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && hasUvValues(
                    classicAfterAllBoundaries,
                    tiledNormalizedUvs)
                && classicFromNative.sectors[0].surfaces[0].uScale
                    == tiledClassic.sectors[0].surfaces[0].uScale
                && classicFromNative.sectors[0].surfaces[0].vScale
                    == tiledClassic.sectors[0].surfaces[0].vScale,
            "SED V2 to JKL to canonical Level chain preserves native scale metadata and converts normalized UV coordinates to classic pixels only on the JKL wire");

        const std::vector<std::byte> ijimNativeBytes =
            sed::assets::saveToSedBytes(tiledIjim);
        const sed::Level ijimFromNative =
            sed::assets::loadFromSedBytes(ijimNativeBytes);
        std::ostringstream ijimFromNativeWire{};
        sed::assets::saveToJKL(
            ijimFromNative,
            ijimFromNativeWire,
            saveOptions);
        std::istringstream ijimFromNativeInput(
            ijimFromNativeWire.str());
        const sed::Level ijimAfterAllBoundaries =
            sed::assets::loadFromJKL(
                ijimFromNativeInput,
                ijimLoadOptions);
        check(ijimNativeBytes.size() >= 8u
                && ijimNativeBytes[4] == std::byte{ 2 }
                && ijimFromNative.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && ijimFromNativeWire.str().find(
                    "-0.50000000 -0.25000000") != std::string::npos
                && ijimAfterAllBoundaries.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && hasUvValues(
                    ijimAfterAllBoundaries,
                    tiledNormalizedUvs)
                && ijimFromNative.sectors[0].surfaces[0].uScale
                    == tiledIjim.sectors[0].surfaces[0].uScale
                && ijimFromNative.sectors[0].surfaces[0].vScale
                    == tiledIjim.sectors[0].surfaces[0].vScale,
            "SED V2 to NDY to canonical Level chain preserves native scale metadata and normalized signed multi-tile wire coordinates");

        const std::array<sed::TPoint2, 3> tiledPixelUvs{
            sed::TPoint2{ -16.0, -4.0 },
            sed::TPoint2{ 40.0, 24.0 },
            sed::TPoint2{ 96.0, -8.0 },
        };
        sed::Level pixelClassic = makeUvBoundaryLevel(
            sed::ProjectType::JKDF2);
        pixelClassic.uvCoordinateSpace =
            sed::UvCoordinateSpace::TexturePixels;
        for (std::size_t index = 0; index < tiledPixelUvs.size(); ++index)
        {
            pixelClassic.sectors[0].surfaces[0].texVertices[index].u =
                static_cast<float>(tiledPixelUvs[index].x);
            pixelClassic.sectors[0].surfaces[0].texVertices[index].v =
                static_cast<float>(tiledPixelUvs[index].y);
        }
        sed::assets::JklSaveOptions noResourceSaveOptions{};
        std::ostringstream pixelClassicOutput{};
        sed::assets::saveToJKL(
            pixelClassic, pixelClassicOutput, noResourceSaveOptions);
        const std::string pixelClassicText = pixelClassicOutput.str();
        check(pixelClassicText.find("-16.00000000 -4.00000000")
                    != std::string::npos
                && pixelClassicText.find("40.00000000 24.00000000")
                    != std::string::npos
                && pixelClassicText.find("96.00000000 -8.00000000")
                    != std::string::npos,
            "JKL export-space matrix writes already-pixel UVs directly without resolving MAT dimensions");
        std::istringstream pixelClassicInput(pixelClassicText);
        const sed::Level pixelClassicRoundTrip = sed::assets::loadFromJKL(
            pixelClassicInput, classicMissingResourceOptions);
        check(pixelClassicRoundTrip.uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels
                && hasUvValues(pixelClassicRoundTrip, tiledPixelUvs),
            "JKL export-space matrix round-trips direct pixel wire values without resources");

        sed::Level pixelIjim = makeUvBoundaryLevel(sed::ProjectType::IJIM);
        pixelIjim.uvCoordinateSpace =
            sed::UvCoordinateSpace::TexturePixels;
        for (std::size_t index = 0; index < tiledPixelUvs.size(); ++index)
        {
            pixelIjim.sectors[0].surfaces[0].texVertices[index].u =
                static_cast<float>(tiledPixelUvs[index].x);
            pixelIjim.sectors[0].surfaces[0].texVertices[index].v =
                static_cast<float>(tiledPixelUvs[index].y);
        }
        std::ostringstream pixelIjimOutput{};
        sed::assets::saveToJKL(pixelIjim, pixelIjimOutput, saveOptions);
        const std::string pixelIjimText = pixelIjimOutput.str();
        check(pixelIjimText.find("-0.50000000 -0.25000000")
                    != std::string::npos
                && pixelIjimText.find("1.25000000 1.50000000")
                    != std::string::npos
                && pixelIjimText.find("3.00000000 -0.50000000")
                    != std::string::npos,
            "NDY export-space matrix normalizes pixel UVs through anisotropic MAT dimensions");
        std::istringstream pixelIjimInput(pixelIjimText);
        const sed::Level pixelIjimRoundTrip = sed::assets::loadFromJKL(
            pixelIjimInput, ijimLoadOptions);
        check(pixelIjimRoundTrip.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && hasUvValues(pixelIjimRoundTrip, tiledNormalizedUvs),
            "NDY export-space matrix reloads converted pixel input as normalized UVs");

        enum class ExportFailureKind
        {
            classicMissingDimensions,
            ijimMissingDimensions,
            classicInvalidSpace,
            ijimInvalidSpace,
            classicCoordinateOverflow,
            classicNonFiniteCoordinate,
            ijimNonFiniteCoordinate,
            classicInvalidDimensions,
            classicSameSpaceNonFiniteCoordinate,
            classicSameSpaceNonFiniteScale,
            classicIncompleteTextureVertices,
            ijimSameSpaceNonFiniteCoordinate,
            ijimSameSpaceNonFiniteScale,
            ijimIncompleteTextureVertices,
            classicHeaderVersionMismatch,
            ijimHeaderVersionMismatch,
        };
        struct ExportFailureCase
        {
            const char* name{};
            ExportFailureKind kind{};
            std::string_view expectedMessage{};
        };
        const std::array exportFailureCases{
            ExportFailureCase{
                "classic normalized UVs without MAT dimensions",
                ExportFailureKind::classicMissingDimensions,
                "MAT dimensions are unavailable" },
            ExportFailureCase{
                "IJIM pixel UVs without MAT dimensions",
                ExportFailureKind::ijimMissingDimensions,
                "MAT dimensions are unavailable" },
            ExportFailureCase{
                "classic invalid coordinate-space tag",
                ExportFailureKind::classicInvalidSpace,
                "Level UV coordinate space is invalid" },
            ExportFailureCase{
                "IJIM invalid coordinate-space tag",
                ExportFailureKind::ijimInvalidSpace,
                "Level UV coordinate space is invalid" },
            ExportFailureCase{
                "classic normalized coordinate overflow",
                ExportFailureKind::classicCoordinateOverflow,
                "UV value is invalid or outside" },
            ExportFailureCase{
                "classic non-finite normalized coordinate",
                ExportFailureKind::classicNonFiniteCoordinate,
                "UV value is invalid or outside" },
            ExportFailureCase{
                "IJIM non-finite pixel coordinate",
                ExportFailureKind::ijimNonFiniteCoordinate,
                "UV value is invalid or outside" },
            ExportFailureCase{
                "classic normalized UVs with an invalid MAT extent",
                ExportFailureKind::classicInvalidDimensions,
                "MAT dimensions are unavailable" },
            ExportFailureCase{
                "classic same-space non-finite coordinate",
                ExportFailureKind::classicSameSpaceNonFiniteCoordinate,
                "UV value is invalid" },
            ExportFailureCase{
                "classic same-space non-finite scale",
                ExportFailureKind::classicSameSpaceNonFiniteScale,
                "UV value is invalid" },
            ExportFailureCase{
                "classic incomplete texture vertices",
                ExportFailureKind::classicIncompleteTextureVertices,
                "texture-vertex data is incomplete" },
            ExportFailureCase{
                "IJIM same-space non-finite coordinate",
                ExportFailureKind::ijimSameSpaceNonFiniteCoordinate,
                "UV value is invalid" },
            ExportFailureCase{
                "IJIM same-space non-finite scale",
                ExportFailureKind::ijimSameSpaceNonFiniteScale,
                "UV value is invalid" },
            ExportFailureCase{
                "IJIM incomplete texture vertices",
                ExportFailureKind::ijimIncompleteTextureVertices,
                "texture-vertex data is incomplete" },
            ExportFailureCase{
                "classic project with an IJIM header version",
                ExportFailureKind::classicHeaderVersionMismatch,
                "header version contradicts project type" },
            ExportFailureCase{
                "IJIM project with a classic header version",
                ExportFailureKind::ijimHeaderVersionMismatch,
                "header version contradicts project type" },
        };
        for (const ExportFailureCase& testCase : exportFailureCases)
        {
            sed::Level candidate = makeUvBoundaryLevel(
                testCase.kind == ExportFailureKind::ijimMissingDimensions
                        || testCase.kind == ExportFailureKind::ijimInvalidSpace
                        || testCase.kind
                            == ExportFailureKind::ijimNonFiniteCoordinate
                        || testCase.kind
                            == ExportFailureKind::ijimSameSpaceNonFiniteCoordinate
                        || testCase.kind
                            == ExportFailureKind::ijimSameSpaceNonFiniteScale
                        || testCase.kind
                            == ExportFailureKind::ijimIncompleteTextureVertices
                        || testCase.kind
                            == ExportFailureKind::ijimHeaderVersionMismatch
                    ? sed::ProjectType::IJIM
                    : sed::ProjectType::JKDF2);
            sed::assets::JklSaveOptions candidateOptions = saveOptions;

            switch (testCase.kind)
            {
            case ExportFailureKind::classicMissingDimensions:
                candidateOptions.resources = nullptr;
                break;
            case ExportFailureKind::ijimMissingDimensions:
                candidate.uvCoordinateSpace =
                    sed::UvCoordinateSpace::TexturePixels;
                candidateOptions.resources = nullptr;
                break;
            case ExportFailureKind::classicInvalidSpace:
            case ExportFailureKind::ijimInvalidSpace:
                candidate.uvCoordinateSpace =
                    static_cast<sed::UvCoordinateSpace>(99);
                break;
            case ExportFailureKind::classicCoordinateOverflow:
                candidate.sectors[0].surfaces[0].texVertices[1].u =
                    std::numeric_limits<float>::max();
                break;
            case ExportFailureKind::classicNonFiniteCoordinate:
                candidate.sectors[0].surfaces[0].texVertices[1].v =
                    std::numeric_limits<float>::quiet_NaN();
                break;
            case ExportFailureKind::ijimNonFiniteCoordinate:
                candidate.uvCoordinateSpace =
                    sed::UvCoordinateSpace::TexturePixels;
                candidate.sectors[0].surfaces[0].texVertices[1].u =
                    std::numeric_limits<float>::infinity();
                break;
            case ExportFailureKind::classicInvalidDimensions:
                candidate.materials[0] = "zero_width.mat";
                candidate.sectors[0].surfaces[0].material =
                    "zero_width.mat";
                break;
            case ExportFailureKind::classicSameSpaceNonFiniteCoordinate:
                candidate.uvCoordinateSpace =
                    sed::UvCoordinateSpace::TexturePixels;
                candidate.sectors[0].surfaces[0].texVertices[0].u =
                    std::numeric_limits<float>::infinity();
                candidateOptions.resources = nullptr;
                break;
            case ExportFailureKind::classicSameSpaceNonFiniteScale:
                candidate.uvCoordinateSpace =
                    sed::UvCoordinateSpace::TexturePixels;
                candidate.sectors[0].surfaces[0].vScale =
                    -std::numeric_limits<float>::infinity();
                candidateOptions.resources = nullptr;
                break;
            case ExportFailureKind::classicIncompleteTextureVertices:
                candidate.uvCoordinateSpace =
                    sed::UvCoordinateSpace::TexturePixels;
                candidate.sectors[0].surfaces[0].texVertices.pop_back();
                candidateOptions.resources = nullptr;
                break;
            case ExportFailureKind::ijimSameSpaceNonFiniteCoordinate:
                candidate.sectors[0].surfaces[0].texVertices[0].v =
                    std::numeric_limits<float>::quiet_NaN();
                candidateOptions.resources = nullptr;
                break;
            case ExportFailureKind::ijimSameSpaceNonFiniteScale:
                candidate.sectors[0].surfaces[0].uScale =
                    std::numeric_limits<float>::infinity();
                candidateOptions.resources = nullptr;
                break;
            case ExportFailureKind::ijimIncompleteTextureVertices:
                candidate.sectors[0].surfaces[0].texVertices.pop_back();
                candidateOptions.resources = nullptr;
                break;
            case ExportFailureKind::classicHeaderVersionMismatch:
                candidate.header.version = 3;
                break;
            case ExportFailureKind::ijimHeaderVersionMismatch:
                candidate.header.version = 1;
                break;
            }

            const sed::Level sourceBaseline = candidate;
            bool rejected = false;
            std::ostringstream failureOutput{};
            try
            {
                sed::assets::saveToJKL(
                    candidate, failureOutput, candidateOptions);
            }
            catch (const std::runtime_error& exception)
            {
                rejected = std::string_view(exception.what()).find(
                    testCase.expectedMessage) != std::string_view::npos;
            }
            const std::string message =
                "JKL/NDY export-space failure matrix rejects atomically "
                + std::string(testCase.name);
            check(rejected
                    && failureOutput.str().empty()
                    && sed::assets::sedLevelsEqual(
                        candidate, sourceBaseline),
                message.c_str());

            const std::filesystem::path failurePath =
                root / "atomic_failure.jkl";
            const std::string sentinel = "existing destination contents\n";
            {
                std::ofstream sentinelOutput(
                    failurePath,
                    std::ios::binary | std::ios::trunc);
                sentinelOutput << sentinel;
                check(static_cast<bool>(sentinelOutput),
                    "JKL/NDY atomic path fixture writes existing destination data");
            }

            bool pathRejected = false;
            try
            {
                sed::assets::saveToJKL(
                    candidate, failurePath, candidateOptions);
            }
            catch (const std::runtime_error& exception)
            {
                pathRejected = std::string_view(exception.what()).find(
                    testCase.expectedMessage) != std::string_view::npos;
            }
            const std::string pathMessage =
                "JKL/NDY export-space failure matrix preserves an existing "
                "destination for "
                + std::string(testCase.name);
            check(pathRejected
                    && readTextFile(failurePath) == sentinel
                    && sed::assets::sedLevelsEqual(
                        candidate, sourceBaseline),
                pathMessage.c_str());
        }

        check(writeBytes(matPath, makeIndexedMat(64, 32)),
            "JKL/NDY UV boundary fixture replaces the MAT with 64x32 dimensions");
        std::istringstream resizedNdyInput(ndyText);
        const sed::Level resizedIjim = sed::assets::loadFromJKL(
            resizedNdyInput, ijimLoadOptions);
        check(hasUvValues(resizedIjim, normalizedUvs),
            "NDY UV boundary remains normalized when the referenced MAT dimensions change");

        std::ostringstream resizedNdyOutput{};
        sed::assets::saveToJKL(ijim, resizedNdyOutput, saveOptions);
        const std::string resizedNdyText = resizedNdyOutput.str();
        check(resizedNdyText.find("0.25000000 0.50000000")
                    != std::string::npos
                && resizedNdyText.find("0.75000000 0.25000000")
                    != std::string::npos,
            "NDY UV boundary export is independent of a live MAT dimension change");
        std::istringstream resizedNdyRoundTripInput(resizedNdyText);
        const sed::Level resizedNdyRoundTrip = sed::assets::loadFromJKL(
            resizedNdyRoundTripInput, ijimLoadOptions);
        check(hasUvValues(resizedNdyRoundTrip, normalizedUvs),
            "NDY UV boundary re-export after a MAT resize round-trips unchanged normalized coordinates");

        std::filesystem::remove_all(root, ec);
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;
        level.header.version = 1;

        sed::Thing& thing = level.newThing();
        thing.name = "base";

        {
            sed::ThingTemplate tpl{};
            tpl.name = "base";
            tpl.parent = "none";

            sed::TplValue model{};
            model.name = "model3d";
            model.type = sed::ValueType::str;
            model.adjType = sed::AdjType::model3do;
            model.stringValue = "test.3do";
            tpl.values.push_back(std::move(model));

            sed::TplValue sprite{};
            sprite.name = "sprite";
            sprite.type = sed::ValueType::str;
            sprite.adjType = sed::AdjType::spr;
            sprite.stringValue = "test.spr";
            tpl.values.push_back(std::move(sprite));

            sed::TplValue puppet{};
            puppet.name = "puppet";
            puppet.type = sed::ValueType::str;
            puppet.adjType = sed::AdjType::pup;
            puppet.stringValue = "test.pup";
            tpl.values.push_back(std::move(puppet));

            sed::TplValue sound{};
            sound.name = "soundclass";
            sound.type = sed::ValueType::str;
            sound.adjType = sed::AdjType::snd;
            sound.stringValue = "test.snd";
            tpl.values.push_back(std::move(sound));

            sed::TplValue ai{};
            ai.name = "aiclass";
            ai.type = sed::ValueType::str;
            ai.adjType = sed::AdjType::ai;
            ai.stringValue = "test.ai";
            tpl.values.push_back(std::move(ai));

            sed::TplValue cog{};
            cog.name = "cog";
            cog.type = sed::ValueType::str;
            cog.adjType = sed::AdjType::cog;
            cog.stringValue = "other.cog";
            tpl.values.push_back(std::move(cog));

            level.templates.push_back(std::move(tpl));
        }

        {
            sed::Cog& cog = level.newCog();
            cog.name = "placed.cog";

            sed::CogValue key{};
            key.cogType = sed::CogType::key;
            key.type = sed::ValueType::str;
            key.stringValue = "anim.key";
            cog.values.push_back(std::move(key));

            sed::CogValue ai{};
            ai.cogType = sed::CogType::ai;
            ai.type = sed::ValueType::str;
            ai.stringValue = "other.ai";
            cog.values.push_back(std::move(ai));

            sed::CogValue model{};
            model.cogType = sed::CogType::model3do;
            model.type = sed::ValueType::str;
            model.stringValue = "other.3do";
            cog.values.push_back(std::move(model));

            sed::CogValue wav{};
            wav.cogType = sed::CogType::wav;
            wav.type = sed::ValueType::str;
            wav.stringValue = "test.wav";
            cog.values.push_back(std::move(wav));
        }

        std::ostringstream oss;
        sed::assets::saveToJKL(level, oss);

        const std::string saved = oss.str();
        check(saved.find("SECTION: JK") != std::string::npos, "JKL save: SECTION: JK");
        check(saved.find("SECTION: COPYRIGHT") != std::string::npos, "JKL save: SECTION: COPYRIGHT");
        check(saved.find("lucasarts") != std::string::npos, "JKL save: copyright logo");

        check(saved.find("SECTION: SOUNDS") != std::string::npos, "JKL save: SECTION: SOUNDS");
        check(saved.find("WORLD SOUNDS 1") != std::string::npos, "JKL save: sounds header");
        check(saved.find("test.wav") != std::string::npos, "JKL save: sounds test.wav");

        check(saved.find("Section: AICLASS") != std::string::npos, "JKL save: list section AICLASS");
        check(saved.find("World AIClasses 2") != std::string::npos, "JKL save: list AICLASS required count");
        check(saved.find("test.ai") != std::string::npos, "JKL save: list AICLASS test.ai");
        check(saved.find("other.ai") != std::string::npos, "JKL save: list AICLASS other.ai");

        check(saved.find("Section: MODELS") != std::string::npos, "JKL save: list section MODELS");
        check(saved.find("World models 2") != std::string::npos, "JKL save: list MODELS required count");
        check(saved.find("test.3do") != std::string::npos, "JKL save: list MODELS test.3do");
        check(saved.find("other.3do") != std::string::npos, "JKL save: list MODELS other.3do");

        check(saved.find("Section: SPRITES") != std::string::npos, "JKL save: list section SPRITES");
        check(saved.find("World sprites 1") != std::string::npos, "JKL save: list SPRITES required count");
        check(saved.find("test.spr") != std::string::npos, "JKL save: list SPRITES test.spr");

        check(saved.find("Section: KEYFRAMES") != std::string::npos, "JKL save: list section KEYFRAMES");
        check(saved.find("World keyframes 1") != std::string::npos, "JKL save: list KEYFRAMES required count");
        check(saved.find("anim.key") != std::string::npos, "JKL save: list KEYFRAMES anim.key");

        check(saved.find("Section: ANIMCLASS") != std::string::npos, "JKL save: list section ANIMCLASS");
        check(saved.find("World puppets 1") != std::string::npos, "JKL save: list ANIMCLASS required count");
        check(saved.find("test.pup") != std::string::npos, "JKL save: list ANIMCLASS test.pup");

        check(saved.find("Section: Soundclass") != std::string::npos, "JKL save: list section Soundclass");
        check(saved.find("World soundclasses 1") != std::string::npos, "JKL save: list Soundclass required count");
        check(saved.find("test.snd") != std::string::npos, "JKL save: list Soundclass test.snd");

        check(saved.find("Section: cogscripts") != std::string::npos, "JKL save: list section cogscripts");
        check(saved.find("World scripts 2") != std::string::npos, "JKL save: list cogscripts required count");
        check(saved.find("placed.cog") != std::string::npos, "JKL save: list cogscripts placed.cog");
        check(saved.find("other.cog") != std::string::npos, "JKL save: list cogscripts other.cog");
    }

    {
        const auto unique                 = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path root  = "tmp_jkl_save_lists_root_" + unique;
        const std::filesystem::path pupFile = root / "misc" / "pup" / "test.pup";
        const std::filesystem::path sndFile = root / "misc" / "snd" / "test.snd";
        const std::filesystem::path sprFile = root / "misc" / "spr" / "test.spr";
        const std::filesystem::path parFile = root / "misc" / "par" / "test.par";
        const std::filesystem::path cogFile = root / "cog" / "other.cog";
        const std::filesystem::path modelFile = root / "3do" / "test.3do";

        std::error_code ec;
        std::filesystem::create_directories(pupFile.parent_path(), ec);
        check(!ec, "JKL save lists: create_directories pup");
        std::filesystem::create_directories(sndFile.parent_path(), ec);
        check(!ec, "JKL save lists: create_directories snd");
        std::filesystem::create_directories(sprFile.parent_path(), ec);
        check(!ec, "JKL save lists: create_directories spr");
        std::filesystem::create_directories(parFile.parent_path(), ec);
        check(!ec, "JKL save lists: create_directories par");
        std::filesystem::create_directories(cogFile.parent_path(), ec);
        check(!ec, "JKL save lists: create_directories cog");
        std::filesystem::create_directories(modelFile.parent_path(), ec);
        check(!ec, "JKL save lists: create_directories 3do");

        {
            std::ofstream out(pupFile);
            out << "walk walk.key\n";
            out << "shoot SHOOT.KEY\n";
        }

        {
            std::ofstream out(sndFile);
            out << "0 foo.wav\n";
        }

        {
            std::ofstream out(sprFile);
            out << "spr.mat\n";
        }

        {
            std::ofstream out(parFile);
            out << "MATERIAL par.mat\n";
        }

        {
            std::ofstream out(cogFile);
            out << "symbols\n";
            out << "    sound snd=fromcog.wav\n";
            out << "end\n";
        }

        {
            std::ofstream out(modelFile);
            out << "SECTION: HEADER\n";
            out << "3DO 2.3\n";
            out << "END\n";
            out << "SECTION: MODELRESOURCE\n";
            out << "MATERIALS 1\n";
            out << "0: model.mat\n";
            out << "END\n";
        }

        sed::assets::ResourceLocator locator{};
        locator.addRoot(root);

        sed::Level level{};
        level.clear();
        level.kind          = sed::ProjectType::JKDF2;
        level.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;
        level.header.version = 1;

        {
            sed::ThingTemplate tpl{};
            tpl.name   = "testtpl";
            tpl.parent = "none";

            sed::TplValue sprite{};
            sprite.name        = "sprite";
            sprite.type        = sed::ValueType::str;
            sprite.adjType     = sed::AdjType::spr;
            sprite.stringValue = "test.spr";
            tpl.values.push_back(std::move(sprite));

            sed::TplValue particle{};
            particle.name        = "particle";
            particle.type        = sed::ValueType::str;
            particle.adjType     = sed::AdjType::par;
            particle.stringValue = "test.par";
            tpl.values.push_back(std::move(particle));

            sed::TplValue puppet{};
            puppet.name        = "puppet";
            puppet.type        = sed::ValueType::str;
            puppet.adjType     = sed::AdjType::pup;
            puppet.stringValue = "test.pup";
            tpl.values.push_back(std::move(puppet));

            sed::TplValue sound{};
            sound.name        = "soundclass";
            sound.type        = sed::ValueType::str;
            sound.adjType     = sed::AdjType::snd;
            sound.stringValue = "test.snd";
            tpl.values.push_back(std::move(sound));

            sed::TplValue model{};
            model.name        = "model3d";
            model.type        = sed::ValueType::str;
            model.adjType     = sed::AdjType::model3do;
            model.stringValue = "test.3do";
            tpl.values.push_back(std::move(model));

            sed::TplValue cog{};
            cog.name        = "cog";
            cog.type        = sed::ValueType::str;
            cog.adjType     = sed::AdjType::cog;
            cog.stringValue = "other.cog";
            tpl.values.push_back(std::move(cog));

            level.templates.push_back(std::move(tpl));
        }

        {
            sed::Thing& thing = level.newThing();
            thing.name   = "testtpl";
            thing.sector = -1;
        }

        sed::assets::JklSaveOptions saveOpts{};
        saveOpts.resources = &locator;

        std::ostringstream oss;
        sed::assets::saveToJKL(level, oss, saveOpts);

        const std::string saved = oss.str();
        check(saved.find("fromcog.wav") != std::string::npos, "JKL save lists: fromcog.wav");
        check(saved.find("foo.wav") != std::string::npos, "JKL save lists: foo.wav");
        check(saved.find("walk.key") != std::string::npos, "JKL save lists: walk.key");
        check(saved.find("SHOOT.KEY") != std::string::npos, "JKL save lists: SHOOT.KEY");
        check(saved.find("spr.mat") != std::string::npos, "JKL save lists: spr.mat");
        check(saved.find("par.mat") != std::string::npos, "JKL save lists: par.mat");
        check(saved.find("model.mat") != std::string::npos, "JKL save lists: model.mat");

        std::filesystem::remove_all(root, ec);
    }

    {
        const auto unique                      = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path root1      = "tmp_jkl_save_cog_root1_" + unique;
        const std::filesystem::path root2      = "tmp_jkl_save_cog_root2_" + unique;
        const std::filesystem::path cogFile1   = root1 / "cog" / "shared.cog";
        const std::filesystem::path cogFile2   = root2 / "cog" / "shared.cog";

        std::error_code ec;
        std::filesystem::create_directories(cogFile1.parent_path(), ec);
        check(!ec, "JKL save cog precedence: create_directories root1");
        std::filesystem::create_directories(cogFile2.parent_path(), ec);
        check(!ec, "JKL save cog precedence: create_directories root2");

        {
            std::ofstream out(cogFile1);
            out << "symbols\n";
            out << "    material mat=first.mat\n";
            out << "end\n";
        }
        {
            std::ofstream out(cogFile2);
            out << "symbols\n";
            out << "    material mat=second.mat\n";
            out << "end\n";
        }

        sed::assets::ResourceLocator locator{};
        locator.addRoot(root1);
        locator.addRoot(root2);

        sed::Level level{};
        level.clear();
        level.kind           = sed::ProjectType::JKDF2;
        level.header.version = 1;

        {
            sed::Cog& cog = level.newCog();
            cog.name = "shared.cog";
        }

        sed::assets::JklSaveOptions saveOpts{};
        saveOpts.resources = &locator;

        std::ostringstream oss;
        sed::assets::saveToJKL(level, oss, saveOpts);

        const std::string saved = oss.str();
        check(saved.find("second.mat") != std::string::npos, "JKL save cog precedence: last root wins");
        check(saved.find("first.mat") == std::string::npos, "JKL save cog precedence: excludes earlier root");

        std::filesystem::remove_all(root1, ec);
        std::filesystem::remove_all(root2, ec);
    }

    {
        const auto unique                          = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path root1          = "tmp_jkl_save_items_root1_" + unique;
        const std::filesystem::path root2          = "tmp_jkl_save_items_root2_" + unique;
        const std::filesystem::path itemsDatFile1  = root1 / "items.dat";
        const std::filesystem::path itemsDatFile2  = root2 / "items.dat";
        const std::filesystem::path cogFile1       = root1 / "cog" / "a.cog";
        const std::filesystem::path cogFile2       = root2 / "cog" / "b.cog";

        std::error_code ec;
        std::filesystem::create_directories(cogFile1.parent_path(), ec);
        check(!ec, "JKL save items.dat precedence: create_directories root1");
        std::filesystem::create_directories(cogFile2.parent_path(), ec);
        check(!ec, "JKL save items.dat precedence: create_directories root2");

        {
            std::ofstream out(itemsDatFile1);
            out << "cog=a.cog\n";
        }
        {
            std::ofstream out(itemsDatFile2);
            out << "cog=b.cog\n";
        }

        {
            std::ofstream out(cogFile1);
            out << "symbols\n";
            out << "    material mat=first.mat\n";
            out << "end\n";
        }
        {
            std::ofstream out(cogFile2);
            out << "symbols\n";
            out << "    material mat=second.mat\n";
            out << "end\n";
        }

        sed::assets::ResourceLocator locator{};
        locator.addRoot(root1);
        locator.addRoot(root2);

        sed::Level level{};
        level.clear();
        level.kind           = sed::ProjectType::JKDF2;
        level.header.version = 1;

        sed::assets::JklSaveOptions saveOpts{};
        saveOpts.resources = &locator;

        std::ostringstream oss;
        sed::assets::saveToJKL(level, oss, saveOpts);

        const std::string saved = oss.str();
        check(saved.find("second.mat") != std::string::npos, "JKL save items.dat precedence: last root wins");
        check(saved.find("first.mat") == std::string::npos, "JKL save items.dat precedence: excludes earlier root");

        std::filesystem::remove_all(root1, ec);
        std::filesystem::remove_all(root2, ec);
    }

    {
        auto findDataFile = [](const std::filesystem::path& filename) -> std::optional<std::filesystem::path>
        {
            std::filesystem::path cur = std::filesystem::current_path();
            for (int depth = 0; depth < 10; ++depth)
            {
                const std::filesystem::path candidate = cur / "data" / filename;

                std::error_code ec;
                if (std::filesystem::exists(candidate, ec) && !ec)
                {
                    return candidate;
                }

                const std::filesystem::path parent = cur.parent_path();
                if (parent == cur)
                {
                    break;
                }
                cur = parent;
            }

            return std::nullopt;
        };

        const auto masterTpl = findDataFile("master.tpl");
        check(masterTpl.has_value(), "JKL save templatesDb: finds data/master.tpl");

        sed::assets::Templates templatesDb = sed::assets::loadTemplates(*masterTpl);

        sed::Level level{};
        level.clear();
        level.kind           = sed::ProjectType::JKDF2;
        level.header.version = 1;

        sed::Thing& thing = level.newThing();
        thing.name   = "walkplayer";
        thing.sector = -1;

        sed::assets::JklSaveOptions saveOpts{};
        saveOpts.templatesDb = &templatesDb;

        std::ostringstream oss;
        sed::assets::saveToJKL(level, oss, saveOpts);

        const std::string saved = oss.str();
        check(saved.find("ky.3do") != std::string::npos, "JKL save templatesDb: model3d pulled from master.tpl");

        const std::size_t tplStart = saved.find("Section: TEMPLATES");
        const std::size_t thingsStart = saved.find("Section: Things");
        check(tplStart != std::string::npos, "JKL save templatesDb: templates section present");
        check(thingsStart != std::string::npos, "JKL save templatesDb: things section present");
        check(thingsStart > tplStart, "JKL save templatesDb: templates section precedes things section");

        const std::string_view tplSection(saved.data() + tplStart, thingsStart - tplStart);
        check(tplSection.find("walkplayer") != std::string::npos, "JKL save templatesDb: template definition written");
    }

    {
        sed::Level level{};
        level.clear();
        level.kind          = sed::ProjectType::JKDF2;
        level.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;
        level.header.version = 1;

        {
            sed::Sector& sector0 = level.newSector();
            sector0.addVertex(0.0, 0.0, 0.0);
            sector0.addVertex(1.0, 0.0, 0.0);
            sector0.addVertex(0.0, 1.0, 0.0);

            sed::Surface surf{};
            surf.material    = "test.mat";
            surf.vertices    = { 0, 1, 2 };
            surf.texVertices = {
                sed::TexVertex{ 0.0f, 0.0f },
                sed::TexVertex{ 1.0f, 0.0f },
                sed::TexVertex{ 0.0f, 1.0f },
            };
            surf.normal      = sed::TVector{ 0.0, 0.0, 1.0 };
            sector0.addSurface(std::move(surf));
        }

        {
            sed::Sector& sector1 = level.newSector();
            sector1.addVertex(0.0, 0.0, 0.0); // duplicate
            sector1.addVertex(0.0, 0.0, 1.0); // new
            sector1.addVertex(1.0, 0.0, 0.0); // duplicate

            sed::Surface surf{};
            surf.material    = "test.mat";
            surf.vertices    = { 0, 1, 2 };
            surf.texVertices = {
                sed::TexVertex{ 0.0f, 0.0f },
                sed::TexVertex{ 1.0f, 0.0f },
                sed::TexVertex{ 0.0f, 1.0f },
            };
            surf.normal      = sed::TVector{ 0.0, 0.0, 1.0 };
            sector1.addSurface(std::move(surf));
        }

        std::ostringstream oss;
        sed::assets::saveToJKL(level, oss);

        std::int32_t worldVertexCount = -1;
        {
            std::istringstream iss(oss.str());
            std::string line{};
            while (std::getline(iss, line))
            {
                std::istringstream ls(line);
                std::string w1{};
                std::string w2{};
                std::int32_t n = 0;
                if (!(ls >> w1 >> w2 >> n))
                {
                    continue;
                }

                if (w1 == "WORLD" && w2 == "VERTICES")
                {
                    worldVertexCount = n;
                    break;
                }
            }
        }

        check(worldVertexCount == 4, "JKL save: pooled world vertices");
    }

    {
        sed::Level level{};
        level.clear();
        level.kind          = sed::ProjectType::IJIM;
        level.header.version = 3;

        sed::Sector& sector = level.newSector();
        sector.ambient = sed::ColorF{ 0.25f, 0.25f, 0.25f, 0.0f };

        sector.addVertex(0.0, 0.0, 0.0);
        sector.addVertex(1.0, 0.0, 0.0);
        sector.addVertex(0.0, 1.0, 0.0);

        sed::Surface surf{};
        surf.material    = "gen_4indy_shadow.mat";
        surf.extraLight  = sed::ColorF{ 0.1f, 0.2f, 0.3f, 1.0f };
        surf.vertices    = { 0, 1, 2 };
        surf.texVertices = {
            sed::TexVertex{ 0.0f, 0.0f, sed::ColorF{ 1.0f, 0.0f, 0.0f, 1.0f } },
            sed::TexVertex{ 1.0f, 0.0f, sed::ColorF{ 0.0f, 1.0f, 0.0f, 1.0f } },
            sed::TexVertex{ 0.0f, 1.0f, sed::ColorF{ 0.0f, 0.0f, 1.0f, 1.0f } },
        };
        surf.normal      = sed::TVector{ 0.0, 0.0, 1.0 };
        sector.addSurface(std::move(surf));

        std::ostringstream oss;
        sed::assets::saveToJKL(level, oss);

        std::int32_t worldMaterialCount = -1;
        std::int32_t firstSurfaceMat = -1;
        {
            std::istringstream iss(oss.str());
            std::string line{};
            bool expectSurfaceLine = false;

            while (std::getline(iss, line))
            {
                if (line.rfind("WORLD MATERIALS ", 0) == 0)
                {
                    std::istringstream tokens(line);
                    std::string w1{};
                    std::string w2{};
                    tokens >> w1 >> w2 >> worldMaterialCount;
                    continue;
                }

                if (line.rfind("WORLD SURFACES ", 0) == 0)
                {
                    expectSurfaceLine = true;
                    continue;
                }

                if (expectSurfaceLine && !line.empty())
                {
                    expectSurfaceLine = false;

                    const std::size_t colon = line.find(':');
                    if (colon != std::string::npos)
                    {
                        std::istringstream surfaceTokens(line.substr(colon + 1));
                        surfaceTokens >> firstSurfaceMat;
                    }
                    break;
                }
            }
        }

        check(worldMaterialCount == 0, "JKL save IJIM: WORLD MATERIALS has no hidden buffer");
        check(firstSurfaceMat == 0x8000, "JKL save IJIM: static material index mask");
        check(oss.str().find("gen_4indy_shadow.mat") == std::string::npos, "JKL save IJIM: static material excluded");
    }

    {
        sed::Level level{};
        level.clear();
        level.kind           = sed::ProjectType::IJIM;
        level.header.version = 3;
        level.materials.resize(200);
        level.exportBufferExtras.materials = 64;

        sed::Sector& sector = level.newSector();
        sector.addVertex(0.0, 0.0, 0.0);
        sector.addVertex(1.0, 0.0, 0.0);
        sector.addVertex(0.0, 1.0, 0.0);

        sed::Surface surf{};
        surf.material    = "gen_4indy_shadow.mat"; // static, filtered from the MATERIALS list
        surf.vertices    = { 0, 1, 2 };
        surf.texVertices = {
            sed::TexVertex{ 0.0f, 0.0f, sed::ColorF{ 1.0f, 1.0f, 1.0f, 1.0f } },
            sed::TexVertex{ 1.0f, 0.0f, sed::ColorF{ 1.0f, 1.0f, 1.0f, 1.0f } },
            sed::TexVertex{ 0.0f, 1.0f, sed::ColorF{ 1.0f, 1.0f, 1.0f, 1.0f } },
        };
        sector.addSurface(std::move(surf));

        std::ostringstream oss;
        sed::assets::saveToJKL(level, oss);

        std::int32_t worldMaterialCount = -1;
        {
            std::istringstream iss(oss.str());
            std::string line{};
            while (std::getline(iss, line))
            {
                if (line.rfind("WORLD MATERIALS ", 0) != 0)
                {
                    continue;
                }

                std::istringstream tokens(line);
                std::string w1{};
                std::string w2{};
                tokens >> w1 >> w2 >> worldMaterialCount;
                break;
            }
        }

        check(worldMaterialCount == 64, "JKL save IJIM: WORLD MATERIALS keeps explicit extra buffer");
    }

    {
        sed::Level level{};
        level.clear();
        level.kind                   = sed::ProjectType::IJIM;
        level.header.version         = 3;
        level.header.gravity         = 9.0;
        level.ppunit                 = 320.0;
        level.materials              = { "test.mat" };
        level.masterCMP              = "test.cmp";
        level.header.ceilingSky.height = 123.0;

        sed::Sector& sector0 = level.newSector();
        sector0.flags = sed::SECF_CollideBox;

        sector0.ambient    = sed::ColorF{ 0.1f, 0.2f, 0.3f, 0.0f };
        sector0.extraLight = sed::ColorF{ 0.01f, 0.02f, 0.03f, 0.0f };
        sector0.tint.setToZero();
        sector0.pointLight.color.setToZero();

        sector0.addVertex(0.0, 0.0, 0.0);
        sector0.addVertex(1.0, 0.0, 0.0);
        sector0.addVertex(0.0, 1.0, 0.0);

        sed::Surface surface{};
        surface.material    = "test.mat";
        surface.vertices    = { 0, 1, 2 };
        surface.texVertices = {
            sed::TexVertex{ 0.0f, 0.0f, sed::ColorF{ 1.0f, 1.0f, 1.0f, 1.0f } },
            sed::TexVertex{ 1.0f, 0.0f, sed::ColorF{ 1.0f, 1.0f, 1.0f, 1.0f } },
            sed::TexVertex{ 0.0f, 1.0f, sed::ColorF{ 1.0f, 1.0f, 1.0f, 1.0f } },
        };
        sector0.addSurface(std::move(surface));

        std::ostringstream oss;
        sed::assets::saveToJKL(level, oss);

        const std::string saved = oss.str();
        check(saved.find("BOUNDBOX ") != std::string::npos, "JKL save IJIM: writes BOUNDBOX");
        check(saved.find("CENTER ") != std::string::npos, "JKL save IJIM: writes CENTER");
        check(saved.find("RADIUS ") != std::string::npos, "JKL save IJIM: writes RADIUS");
        check(saved.find("COLLIDEBOX ") == std::string::npos, "JKL save IJIM: omits COLLIDEBOX when zero");

        std::istringstream iss(saved);
        sed::assets::JklLoadOptions opts{};
        opts.projectType = sed::ProjectType::IJIM;
        const sed::Level loaded = sed::assets::loadFromJKL(iss, opts);

        check(loaded.kind == sed::ProjectType::IJIM, "JKL save IJIM: kind");
        check(loaded.sectors.size() == 1, "JKL save IJIM: sectors");

        if (!loaded.sectors.empty())
        {
            const sed::Sector& s0 = loaded.sectors[0];
            check((s0.flags & sed::SECF_CollideBox) == 0, "JKL save IJIM: clears SECF_CollideBox on write");
            check(close(static_cast<double>(s0.ambient.r), 0.1), "JKL save IJIM: ambient.r");
            check(close(static_cast<double>(s0.ambient.g), 0.2), "JKL save IJIM: ambient.g");
            check(close(static_cast<double>(s0.ambient.b), 0.3), "JKL save IJIM: ambient.b");
        }
    }

    {
        sed::Level level{};
        level.clear();
        level.kind           = sed::ProjectType::IJIM;
        level.header.version = 3;

        sed::Thing& thing = level.newThing();
        thing.name = "none";

        sed::TplValue explode{};
        explode.name        = "explode";
        explode.type        = sed::ValueType::str;
        explode.adjType     = sed::AdjType::tpl;
        explode.stringValue = "EC.3do";
        thing.values.push_back(std::move(explode));

        std::vector<std::string> warnings{};
        sed::assets::JklSaveOptions opts{};
        opts.onDiagnostic = [&](const sed::DiagnosticSeverity severity, const std::string_view message)
            {
                if (severity == sed::DiagnosticSeverity::warning)
                {
                    warnings.push_back(std::string(message));
                }
            };

        std::ostringstream oss;
        sed::assets::saveToJKL(level, oss, opts);

        check(oss.str().find("EC.3do") != std::string::npos, "JKL save: treat *.3do in tpl adjacency as model");
        check(warnings.empty(), "JKL save: no missing template warning for *.3do");
    }

    {
        const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path root = "tmp_jkl_cog_capacity_" + unique;
        const std::filesystem::path cogFile = root / "cog" / "class_shared.cog";
        std::error_code ec;
        std::filesystem::create_directories(cogFile.parent_path(), ec);
        check(!ec, "JKL save COG capacity: create resource directory");
        {
            std::ofstream out(cogFile);
            out << "symbols\nend\n";
        }

        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;
        level.header.version = 1;

        auto addTemplate = [&level](const std::string& name, const std::string& script)
        {
            sed::ThingTemplate tpl{};
            tpl.name = name;
            tpl.parent = "none";
            sed::TplValue cog{};
            cog.name = "cog";
            cog.type = sed::ValueType::str;
            cog.adjType = sed::AdjType::cog;
            cog.stringValue = script;
            tpl.values.push_back(std::move(cog));
            level.templates.push_back(std::move(tpl));
        };
        addTemplate("+grenade", "class_shared.cog");
        addTemplate("+satchel", "class_shared.cog");
        addTemplate("+no_cog", "none");
        {
            sed::ThingTemplate inherited{};
            inherited.name = "+inherited";
            inherited.parent = "+grenade";
            level.templates.push_back(std::move(inherited));
        }

        sed::Thing& grenade = level.newThing();
        grenade.name = "+grenade";
        grenade.templateName = "+grenade";

        sed::Thing& satchel = level.newThing();
        satchel.name = "+satchel";
        satchel.templateName = "+satchel";
        sed::TplValue localCog{};
        localCog.name = "cog";
        localCog.type = sed::ValueType::str;
        localCog.adjType = sed::AdjType::cog;
        localCog.stringValue = "class_shared.cog";
        satchel.values.push_back(std::move(localCog));

        // Make the explicit none-valued template reachable as well. It must
        // not consume an instance even though its parameter is serialized.
        sed::Thing& noCog = level.newThing();
        noCog.name = "+no_cog";
        noCog.templateName = "+no_cog";

        sed::Thing& inherited = level.newThing();
        inherited.name = "+inherited";
        inherited.templateName = "+inherited";

        sed::assets::ResourceLocator locator{};
        locator.addRoot(root);
        sed::assets::JklSaveOptions options{};
        options.resources = &locator;

        for (const auto& [kind, version, label] : std::array{
                 std::tuple{sed::ProjectType::JKDF2, 1, std::string_view{"JKDF2"}},
                 std::tuple{sed::ProjectType::MOTS, 1, std::string_view{"MotS"}},
                 std::tuple{sed::ProjectType::IJIM, 2, std::string_view{"IJIM V2"}},
                 std::tuple{sed::ProjectType::IJIM, 3, std::string_view{"IJIM V3"}},
             })
        {
            level.kind = kind;
            level.header.version = version;
            std::ostringstream out;
            sed::assets::saveToJKL(level, out, options);
            const std::string saved = out.str();
            const std::string scriptMessage = std::string("JKL save COG capacity ")
                + std::string(label) + ": duplicate filenames stay one script resource";
            const std::string instanceMessage = std::string("JKL save COG capacity ")
                + std::string(label)
                + ": duplicate template uses and Thing-local overrides reserve instances without recounting inheritance";
            check(saved.find("World scripts 1") != std::string::npos,
                scriptMessage.c_str());
            check(saved.find("World cogs\t3") != std::string::npos,
                instanceMessage.c_str());
        }

        std::filesystem::remove_all(root, ec);
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;
        level.header.version = 1;
        level.exportBufferExtras = sed::LevelExportBufferExtras{
            .sounds = 1,
            .materials = 2,
            .aiClasses = 3,
            .models = 4,
            .sprites = 5,
            .keyframes = 6,
            .puppets = 7,
            .soundClasses = 8,
            .cogScripts = 9,
            .cogs = 10,
            .templates = 11,
            .things = 12,
        };

        std::ostringstream out;
        sed::assets::saveToJKL(level, out);
        const sed::assets::JklExportBufferReport report =
            sed::assets::inspectJklExportBuffers(level);
        const std::string saved = out.str();
        check(saved.find("WORLD SOUNDS 1") != std::string::npos,
            "JKL export buffers: sounds extra is added to exact dependencies");
        check(saved.find("WORLD MATERIALS 2") != std::string::npos,
            "JKL export buffers: materials extra is added to exact dependencies");
        check(saved.find("World AIClasses 3") != std::string::npos,
            "JKL export buffers: AI extra is literal");
        check(saved.find("World models 4") != std::string::npos,
            "JKL export buffers: model extra is literal");
        check(saved.find("World sprites 5") != std::string::npos,
            "JKL export buffers: sprite extra is literal");
        check(saved.find("World keyframes 6") != std::string::npos,
            "JKL export buffers: keyframe extra is literal");
        check(saved.find("World puppets 7") != std::string::npos,
            "JKL export buffers: puppet extra is literal");
        check(saved.find("World soundclasses 8") != std::string::npos,
            "JKL export buffers: sound-class extra is literal");
        check(saved.find("World scripts 9") != std::string::npos,
            "JKL export buffers: COG-script extra is literal");
        check(saved.find("World cogs\t10") != std::string::npos,
            "JKL export buffers: COG-instance extra follows exact parser demand");
        check(saved.find("World templates 11") != std::string::npos,
            "JKL export buffers: template extra follows emitted templates");
        check(saved.find("World things 12") != std::string::npos,
            "JKL export buffers: Thing extra is literal");

        check(report.dependencyScanComplete
                && report.sounds == sed::assets::JklExportBufferUsage{0, 0, 1, 1}
                && report.materials == sed::assets::JklExportBufferUsage{0, 0, 2, 2}
                && report.aiClasses == sed::assets::JklExportBufferUsage{0, 0, 3, 3}
                && report.models == sed::assets::JklExportBufferUsage{0, 0, 4, 4}
                && report.sprites == sed::assets::JklExportBufferUsage{0, 0, 5, 5}
                && report.keyframes == sed::assets::JklExportBufferUsage{0, 0, 6, 6}
                && report.puppets == sed::assets::JklExportBufferUsage{0, 0, 7, 7}
                && report.soundClasses == sed::assets::JklExportBufferUsage{0, 0, 8, 8}
                && report.cogScripts == sed::assets::JklExportBufferUsage{0, 0, 9, 9}
                && report.cogs == sed::assets::JklExportBufferUsage{0, 0, 10, 10}
                && report.templates == sed::assets::JklExportBufferUsage{0, 0, 11, 11}
                && report.things == sed::assets::JklExportBufferUsage{0, 0, 12, 12},
            "JKL export-buffer report exposes exact required, extra, and final capacities as the writer");

        std::istringstream in(saved);
        const sed::Level loaded = sed::assets::loadFromJKL(in);
        check(loaded.exportBufferExtras == level.exportBufferExtras,
            "JKL export buffers: intentional spare capacity survives load/save round-trip");

        level.kind = sed::ProjectType::IJIM;
        level.header.version = 3;
        level.exportBufferExtras = {};
        const sed::assets::JklExportBufferReport ijimReport =
            sed::assets::inspectJklExportBuffers(level);
        check(ijimReport.materials == sed::assets::JklExportBufferUsage{0, 0, 0, 0},
            "JKL export-buffer report adds no hidden IJIM material reserve");

        level.kind = sed::ProjectType::JKDF2;
        level.header.version = 1;
        sed::Thing& missingCogThing = level.newThing();
        missingCogThing.name = "none";
        sed::TplValue missingCog{};
        missingCog.name = "cog";
        missingCog.type = sed::ValueType::str;
        missingCog.adjType = sed::AdjType::cog;
        missingCog.stringValue = "missing_capacity_test.cog";
        missingCogThing.values.push_back(std::move(missingCog));
        sed::assets::ResourceLocator emptyLocator{};
        std::vector<std::string> missingWarnings{};
        sed::assets::JklSaveOptions missingOptions{};
        missingOptions.resources = &emptyLocator;
        missingOptions.onDiagnostic = [&missingWarnings](
                                          const sed::DiagnosticSeverity severity,
                                          const std::string_view message)
        {
            if (severity == sed::DiagnosticSeverity::warning)
            {
                missingWarnings.emplace_back(message);
            }
        };
        const sed::assets::JklExportBufferReport incompleteReport =
            sed::assets::inspectJklExportBuffers(level, missingOptions);
        check(!incompleteReport.dependencyScanComplete
                && incompleteReport.cogs == sed::assets::JklExportBufferUsage{1, 0, 0, 1}
                && std::any_of(
                    missingWarnings.cbegin(),
                    missingWarnings.cend(),
                    [](const std::string& warning)
                    {
                        return warning.find("missing_capacity_test.cog") != std::string::npos;
                    }),
            "JKL export-buffer report flags a missing COG dependency without inventing hidden fallback reserve");
    }

    {
        const auto makeIjimDynamicMaterialLevel = [](
            const std::int32_t version,
            const std::uint32_t materialExtra)
        {
            sed::Level level = makeUvBoundaryLevel(sed::ProjectType::IJIM);
            level.header.version = version;
            level.exportBufferExtras.materials = materialExtra;
            return level;
        };

        for ( const std::int32_t version : { 2, 3 } )
        {
            for ( const std::uint32_t materialExtra : { 0u, 64u } )
            {
                const sed::Level level = makeIjimDynamicMaterialLevel(version, materialExtra);
                std::ostringstream output{};
                sed::assets::saveToJKL(level, output);
                const std::string expectedHeader = "WORLD MATERIALS "
                    + std::to_string(1u + materialExtra) + "\n";
                const sed::assets::JklExportBufferReport report =
                    sed::assets::inspectJklExportBuffers(level);
                check(output.str().find(expectedHeader) != std::string::npos,
                    "IJIM NDY material capacity: V2/V3 write required plus the selected zero/64 extra");
                check(report.dependencyScanComplete
                        && report.materials
                            == sed::assets::JklExportBufferUsage{
                                1, 0, materialExtra, 1u + materialExtra },
                    "IJIM NDY material capacity: V2/V3 report the exact dynamic MAT requirement and extra");
            }
        }

        const std::string unique = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path root = std::filesystem::temp_directory_path()
            / ("sed_ijim_ndy_material_capacity_" + unique);
        std::error_code error;
        std::filesystem::create_directories(root, error);
        check(!error,
            "IJIM NDY material capacity: temporary path-safety directory is created");

        if ( !error )
        {
            const std::string streamSentinel = "existing stream sentinel";
            const std::string pathSentinel = "existing path sentinel\n";
            const std::filesystem::path destination = root / "invalid_material_capacity.ndy";

            for ( const std::int32_t version : { 2, 3 } )
            {
                for ( const std::uint32_t materialExtra : { 1u, 65u } )
                {
                    const sed::Level level = makeIjimDynamicMaterialLevel(version, materialExtra);
                    const sed::assets::JklExportBufferReport report =
                        sed::assets::inspectJklExportBuffers(level);
                    check(report.dependencyScanComplete
                            && report.materials
                                == sed::assets::JklExportBufferUsage{
                                    1, 0, materialExtra, 1u + materialExtra },
                        "IJIM NDY material capacity: report-only inspection retains an invalid authored extra");

                    std::ostringstream output{};
                    output << streamSentinel;
                    bool streamRejected = false;
                    std::string streamDiagnostic{};
                    try
                    {
                        sed::assets::saveToJKL(level, output);
                    }
                    catch ( const std::runtime_error& exception )
                    {
                        streamRejected = true;
                        streamDiagnostic = exception.what();
                    }
                    check(streamRejected
                            && streamDiagnostic.find("Extra Buffers") != std::string::npos,
                        "IJIM NDY material capacity: V2/V3 extras 1/65 reject with an Extra Buffers diagnostic");
                    check(output.str() == streamSentinel,
                        "IJIM NDY material capacity: rejected stream export preserves its existing contents");

                    {
                        std::ofstream existing(destination, std::ios::binary | std::ios::trunc);
                        existing.write(pathSentinel.data(),
                            static_cast<std::streamsize>(pathSentinel.size()));
                        check(static_cast<bool>(existing),
                            "IJIM NDY material capacity: path-safety sentinel is installed");
                    }

                    bool pathRejected = false;
                    std::string pathDiagnostic{};
                    try
                    {
                        sed::assets::saveToJKL(level, destination);
                    }
                    catch ( const std::runtime_error& exception )
                    {
                        pathRejected = true;
                        pathDiagnostic = exception.what();
                    }
                    check(pathRejected
                            && pathDiagnostic.find("Extra Buffers") != std::string::npos,
                        "IJIM NDY material capacity: invalid path export points to Extra Buffers");
                    check(readTextFile(destination) == pathSentinel,
                        "IJIM NDY material capacity: rejected path export preserves its existing contents");
                }
            }

            std::filesystem::remove_all(root, error);
        }

        const sed::Level nativeSource = makeIjimDynamicMaterialLevel(3, 1u);
        const std::vector<std::byte> nativeBytes = sed::assets::saveToSedBytes(nativeSource);
        const sed::Level nativeRoundTrip = sed::assets::loadFromSedBytes(nativeBytes);
        check(nativeRoundTrip.kind == sed::ProjectType::IJIM
                && nativeRoundTrip.exportBufferExtras.materials == 1u,
            "IJIM NDY material capacity: native SED preserves authored invalid material extra for repair");
    }

    // Editor-only Lights/Layers sections are independently optional for all
    // three engine text dialects.  Compact output must remain accepted by the
    // same parser and use LF/tab/short decimal spellings without padded
    // template columns.
    for (const auto projectType : {sed::ProjectType::JKDF2, sed::ProjectType::MOTS, sed::ProjectType::IJIM})
    {
        sed::Level level{};
        level.clear();
        level.kind = projectType;
        level.uvCoordinateSpace = projectType == sed::ProjectType::IJIM
            ? sed::UvCoordinateSpace::Normalized
            : sed::UvCoordinateSpace::TexturePixels;
        level.header.version = projectType == sed::ProjectType::IJIM ? 3 : 1;
        level.header.gravity = 0.015;
        // Exercise compact's finite-number boundary cases.  The writer first
        // emits its normal field precision; compact mode must shorten that
        // spelling without changing the parsed value or the sign of -0.
        level.header.horizonSky.distance = 0.000001;
        level.header.horizonSky.offset = sed::TPoint2::Create(-0.0, -1234.5);
        level.header.ceilingSky.height = 100000000.0;
        level.header.perspectiveDistance = 123.0;
        // Layer names and LVIS are opaque line-oriented user strings.  Their
        // spaces must survive compact export even though structural fields
        // use tabs.
        level.layers = {"Main Layer"};
        level.lvisstring = "user lvis 0.015";
        level.lights.push_back(sed::SedLight{});
        level.lights.back().position = sed::TVector{-0.015, 0.0, 0.0};
        sed::Sector& numericSoundSector = level.newSector();
        numericSoundSector.sound = "0.015";
        numericSoundSector.soundVolume = 0.5;
        numericSoundSector.addVertex(0.0, 0.0, 0.0);
        numericSoundSector.addVertex(1.0, 0.0, 0.0);
        numericSoundSector.addVertex(0.0, 1.0, 0.0);
        sed::Surface numericMaterialSurface{};
        numericMaterialSurface.material = "0.015";
        numericMaterialSurface.nMat = 0;
        numericMaterialSurface.vertices = {0, 1, 2};
        numericMaterialSurface.texVertices = {
            sed::TexVertex{0.0f, 0.0f, sed::ColorWhite},
            sed::TexVertex{1.0f, 0.0f, sed::ColorWhite},
            sed::TexVertex{0.0f, 1.0f, sed::ColorWhite},
        };
        numericSoundSector.addSurface(std::move(numericMaterialSurface));

        sed::ThingTemplate textFixture{};
        textFixture.name = "folder/0.015foo";
        textFixture.parent = "none";
        sed::TplValue quotedText{};
        quotedText.name = "label";
        quotedText.type = sed::ValueType::str;
        quotedText.stringValue = "\"folder/0.015 foo\"";
        textFixture.values.push_back(quotedText);
        sed::TplValue unknownNumericText{};
        unknownNumericText.name = "custom_text";
        unknownNumericText.type = sed::ValueType::str;
        unknownNumericText.stringValue = "001.5000";
        textFixture.values.push_back(unknownNumericText);
        level.templates.push_back(textFixture);
        sed::Thing& textThing = level.newThing();
        textThing.templateName = textFixture.name;
        textThing.name = "thing/folder/0.015foo";
        textThing.sector = -1;
        sed::Cog& numericTextCog = level.newCog();
        numericTextCog.name = "numeric.cog";
        sed::CogValue numericTextValue{};
        numericTextValue.type = sed::ValueType::str;
        numericTextValue.stringValue = "001.5000";
        numericTextCog.values.push_back(numericTextValue);

        for (int optionMask = 0; optionMask != 4; ++optionMask)
        {
            sed::assets::JklSaveOptions sectionOptions{};
            sectionOptions.includeLights = (optionMask & 1) != 0;
            sectionOptions.includeLayers = (optionMask & 2) != 0;
            std::ostringstream sectionOutput;
            sed::assets::saveToJKL(level, sectionOutput, sectionOptions);
            const std::string sectionText = sectionOutput.str();
            const bool hasLights = sectionText.find("Section: Lights") != std::string::npos;
            const bool hasLayers = sectionText.find("Section: Layers") != std::string::npos;
            check(hasLights == sectionOptions.includeLights
                    && hasLayers == sectionOptions.includeLayers,
                "JKL optional editor section choices control complete sections");

            sed::assets::JklLoadOptions sectionLoadOptions{};
            sectionLoadOptions.projectType = projectType;
            std::istringstream sectionInput(sectionText);
            const sed::Level sectionLoaded = sed::assets::loadFromJKL(sectionInput, sectionLoadOptions);
            check((sectionLoaded.lights.empty() == !sectionOptions.includeLights)
                    && (sectionLoaded.layers.empty() == !sectionOptions.includeLayers)
                    && close(sectionLoaded.header.gravity, level.header.gravity),
                "JKL optional editor section combinations remain parseable for every project type");
        }

        sed::assets::JklSaveOptions compactOptions{};
        compactOptions.compact = true;
        std::ostringstream compact;
        sed::assets::saveToJKL(level, compact, compactOptions);
        const std::string compactText = compact.str();
        check(compactText.find('\r') == std::string::npos
                && compactText.find(".015") != std::string::npos
                && compactText.find("-.015") != std::string::npos,
            "JKL compact export uses LF and shortest fractional spellings");
        check(compactText.find("1e-6") != std::string::npos
                && compactText.find("1e8") != std::string::npos
                && compactText.find("-1234.5") != std::string::npos
                && (projectType == sed::ProjectType::IJIM
                    || compactText.find("PERSPECTIVE\tDISTANCE\t123\n") != std::string::npos)
                && compactText.find("HORIZON\tSKY\tOFFSET\t-0\t-1234.5\n") != std::string::npos,
            "JKL compact export uses shortest exponent, integer, negative, and signed-zero spellings");
        check(compactText.find("#Num") == std::string::npos
                && compactText.find("#Name:") == std::string::npos
                && compactText.find("user lvis 0.015\n") != std::string::npos,
            "JKL compact export drops only its parser-only field comments");

        const auto copyrightBody = [](const std::string& text) -> std::string
        {
            const std::size_t copyright = text.find("COPYRIGHT");
            const std::size_t header = copyright == std::string::npos
                ? std::string::npos
                : text.rfind("SECTION:", copyright);
            if (header == std::string::npos)
            {
                return {};
            }
            const std::size_t bodyStart = text.find('\n', header);
            const std::size_t nextSection = bodyStart == std::string::npos
                ? std::string::npos
                : text.find("SECTION:", bodyStart + 1);
            if (bodyStart == std::string::npos || nextSection == std::string::npos)
            {
                return {};
            }
            return text.substr(bodyStart + 1, nextSection - bodyStart - 1);
        };
        const std::string regularText = [&]()
        {
            sed::assets::JklSaveOptions regularOptions{};
            std::ostringstream regular;
            sed::assets::saveToJKL(level, regular, regularOptions);
            return regular.str();
        }();
        const bool readableGeometrySpacing =
            (projectType == sed::ProjectType::IJIM
                || regularText.find("\n\nWORLD VERTICES ") != std::string::npos)
            && regularText.find("\n\nWORLD TEXTURE VERTICES ") != std::string::npos
            && regularText.find("\n\nWORLD ADJOINS ") != std::string::npos
            && regularText.find("\n\nWORLD SURFACES ") != std::string::npos;
        check(readableGeometrySpacing,
            "JKL readable export separates GEORESOURCE subsections with blank lines");
        check(compactText.find("\n\nWORLD TEXTURE VERTICES ") == std::string::npos
                && compactText.find("\n\nWORLD ADJOINS ") == std::string::npos
                && compactText.find("\n\nWORLD SURFACES ") == std::string::npos,
            "JKL compact export removes GEORESOURCE subsection blank lines");
        check(copyrightBody(regularText) == copyrightBody(compactText),
            "JKL compact export preserves copyright block bytes exactly");
        check(compactText.find("folder/0.015foo") != std::string::npos
                && compactText.find("thing/folder/0.015foo") != std::string::npos
                && compactText.find("\"folder/0.015 foo\"") != std::string::npos
                && compactText.find("Main Layer\n") != std::string::npos
                && compactText.find("custom_text=001.5000") != std::string::npos
                && compactText.find("numeric.cog\t001.5000") != std::string::npos
                && compactText.find("SOUNDS\t1\n0.015\n") != std::string::npos
                && compactText.find("MATERIALS\t1\n0:\t0.015") != std::string::npos
                && compactText.find("SOUND\t0.015\t.5\n") != std::string::npos,
            "JKL compact export does not rewrite decimal-looking resource/name text");

        bool compactSeparators = true;
        bool inCopyright = false;
        std::istringstream compactLines(compactText);
        std::string line;
        while (std::getline(compactLines, line))
        {
            const std::string trimmed = sed::text::trim(line);
            const std::string upper = sed::text::upperAscii(trimmed);
            bool entersCopyright = false;
            if (upper.rfind("SECTION:", 0) == 0)
            {
                entersCopyright = sed::text::iequalsAscii(
                    sed::text::trim(trimmed.substr(std::string("SECTION:").size())), "COPYRIGHT");
            }
            const bool leavesCopyright = inCopyright
                && (sed::text::iequalsAscii(trimmed, "END")
                    || (upper.rfind("SECTION:", 0) == 0 && !entersCopyright));
            bool hasUnquotedSpace = false;
            bool quoted = false;
            bool escaped = false;
            for (const char c : line)
            {
                if (quoted)
                {
                    if (escaped)
                    {
                        escaped = false;
                    }
                    else if (c == '\\')
                    {
                        escaped = true;
                    }
                    else if (c == '"')
                    {
                        quoted = false;
                    }
                }
                else if (c == '"')
                {
                    quoted = true;
                }
                else if (c == ' ')
                {
                    hasUnquotedSpace = true;
                    break;
                }
            }
            if ((!inCopyright || leavesCopyright) && hasUnquotedSpace)
            {
                // These are the two opaque line-oriented user strings in the
                // fixture; their spaces are data rather than separators.
                const bool opaqueLayerText = trimmed == "Main Layer"
                || trimmed == "user lvis 0.015";
                if (!opaqueLayerText)
                {
                    compactSeparators = false;
                }
            }
            if (entersCopyright)
            {
                inCopyright = true;
            }
            else if (leavesCopyright)
            {
                inCopyright = false;
            }
        }
        check(compactSeparators, "JKL compact export has no space-padded fields");

        sed::assets::JklLoadOptions compactLoadOptions{};
        compactLoadOptions.projectType = projectType;
        std::istringstream compactInput(compactText);
        const sed::Level compactLoaded = sed::assets::loadFromJKL(compactInput, compactLoadOptions);
        const bool unknownNumericStringPreserved = [&compactLoaded]()
        {
            for (const sed::ThingTemplate& tpl : compactLoaded.templates)
            {
                for (const sed::TplValue& value : tpl.values)
                {
                    if (sed::text::iequalsAscii(value.name, "custom_text"))
                    {
                        return value.stringValue == "001.5000";
                    }
                }
            }
            return false;
        }();
        const bool cogNumericStringPreserved = compactLoaded.cogs.size() == 1
            && compactLoaded.cogs.front().values.size() == 1
            && compactLoaded.cogs.front().values.front().stringValue == "001.5000";
        check(compactLoaded.lights.size() == level.lights.size()
                && compactLoaded.layers == level.layers
                && close(compactLoaded.header.gravity, level.header.gravity)
                && close(compactLoaded.lights.front().position.x, level.lights.front().position.x)
                && close(compactLoaded.header.horizonSky.distance, level.header.horizonSky.distance)
                && close(compactLoaded.header.ceilingSky.height, level.header.ceilingSky.height)
                && std::signbit(compactLoaded.header.horizonSky.offset.x)
                && close(compactLoaded.header.horizonSky.offset.y, level.header.horizonSky.offset.y)
                && compactLoaded.lvisstring == level.lvisstring
                && (projectType == sed::ProjectType::IJIM
                    || close(compactLoaded.header.perspectiveDistance, level.header.perspectiveDistance))
                && unknownNumericStringPreserved
                && cogNumericStringPreserved,
            "JKL compact export round-trips editor sections for every project type");
    }

    {
        constexpr double kNearDistance = 2.0e-5;
        const std::array<sed::ProjectType, 3> projectTypes{
            sed::ProjectType::JKDF2,
            sed::ProjectType::MOTS,
            sed::ProjectType::IJIM,
        };

        for (const sed::ProjectType projectType : projectTypes)
        {
            const double diagonalOffset = kNearDistance / std::sqrt(3.0);
            sed::Level level = makeVertexWeldLevel(projectType, diagonalOffset, true);
            const sed::Level sourceBefore = level;

            std::ostringstream defaultOutput{};
            sed::assets::saveToJKL(level, defaultOutput);
            const std::string defaultText = defaultOutput.str();
            const ParsedJklGeometry defaultGeometry = parseJklGeometry(defaultText);

            check(defaultGeometry.worldVertices.size() == 4u,
                "JKL vertex weld: default export preserves nearby diagonal vertices and exact-deduplicates all projects");
            check(!defaultGeometry.worldVertices.empty()
                    && defaultGeometry.worldVertices[0].x == -2.0
                    && defaultGeometry.worldVertices[0].y == -3.0
                    && defaultGeometry.worldVertices[0].z == -4.0,
                "JKL vertex weld: negative world coordinates retain their exact representative");
            check(defaultGeometry.surfaceVertices.size() == 2u
                    && defaultGeometry.surfaceVertices[0].size() == 3u
                    && defaultGeometry.surfaceVertices[1].size() == 3u,
                "JKL vertex weld: default export retains both surface index triples");
            if (defaultGeometry.surfaceVertices.size() == 2u
                && defaultGeometry.surfaceVertices[0].size() == 3u
                && defaultGeometry.surfaceVertices[1].size() == 3u)
            {
                check(defaultGeometry.surfaceVertices[0][0]
                        != defaultGeometry.surfaceVertices[1][0],
                    "JKL vertex weld: default export does not use legacy componentwise epsilon");
                check(defaultGeometry.surfaceVertices[0][1]
                        == defaultGeometry.surfaceVertices[1][2]
                        && defaultGeometry.surfaceVertices[0][2]
                        == defaultGeometry.surfaceVertices[1][1],
                    "JKL vertex weld: exact-coordinate duplicates always share a world vertex");
                check(defaultGeometry.surfaceAdjoins.size() == 2u
                        && defaultGeometry.surfaceAdjoins[0] >= 0
                        && defaultGeometry.surfaceAdjoins[1] >= 0,
                    "JKL vertex weld: default export keeps reciprocal adjoin records");
                if (defaultGeometry.surfaceAdjoins.size() == 2u
                    && defaultGeometry.adjoinMirrors.size() > static_cast<std::size_t>(
                        std::max(defaultGeometry.surfaceAdjoins[0], defaultGeometry.surfaceAdjoins[1])))
                {
                    check(defaultGeometry.adjoinMirrors[static_cast<std::size_t>(defaultGeometry.surfaceAdjoins[0])]
                            == defaultGeometry.surfaceAdjoins[1]
                            && defaultGeometry.adjoinMirrors[static_cast<std::size_t>(defaultGeometry.surfaceAdjoins[1])]
                            == defaultGeometry.surfaceAdjoins[0],
                        "JKL vertex weld: default export keeps reciprocal adjoin mirrors");
                }
            }
            check(sameWeldSourceGeometry(level, sourceBefore),
                "JKL vertex weld: default export leaves source geometry, UV, colors, and adjoins unchanged");

            sed::assets::JklSaveOptions disabledInvalid{};
            disabledInvalid.weldNearbyVertices = false;
            disabledInvalid.vertexWeldDistance = std::numeric_limits<double>::quiet_NaN();
            std::ostringstream disabledOutput{};
            sed::assets::saveToJKL(level, disabledOutput, disabledInvalid);
            check(disabledOutput.str() == defaultText,
                "JKL vertex weld: invalid distance is ignored when nearby welding is disabled");
        }
    }

    {
        constexpr double kWeldDistance = 3.0;
        const std::array<sed::ProjectType, 3> projectTypes{
            sed::ProjectType::JKDF2,
            sed::ProjectType::MOTS,
            sed::ProjectType::IJIM,
        };

        for (const sed::ProjectType projectType : projectTypes)
        {
            const std::array<double, 3> multipliers{ 0.999, 1.0, 1.001 };
            for (const double multiplier : multipliers)
            {
                const double scale = multiplier * kWeldDistance / 3.0;
                const sed::TVector diagonalOffset{
                    scale,
                    scale * 2.0,
                    scale * 2.0,
                };
                sed::Level level = makeOriginVertexWeldLevel(projectType, diagonalOffset);
                const sed::Level sourceBefore = level;

                sed::assets::JklSaveOptions options{};
                options.weldNearbyVertices = true;
                options.vertexWeldDistance = kWeldDistance;

                std::ostringstream firstOutput{};
                sed::assets::saveToJKL(level, firstOutput, options);
                const std::string firstText = firstOutput.str();
                const ParsedJklGeometry geometry = parseJklGeometry(firstText);
                const std::size_t expectedCount = multiplier <= 1.0 ? 3u : 4u;
                check(geometry.worldVertices.size() == expectedCount,
                    "JKL vertex weld: opt-in diagonal Euclidean boundary obeys below/at/above threshold");

                std::ostringstream secondOutput{};
                sed::assets::saveToJKL(level, secondOutput, options);
                check(secondOutput.str() == firstText,
                    "JKL vertex weld: opt-in export is byte-deterministic");
                check(sameWeldSourceGeometry(level, sourceBefore),
                    "JKL vertex weld: opt-in export leaves source geometry, UV, colors, and adjoins unchanged");
            }
        }
    }

    {
        constexpr double kWeldDistance = 1.0;
        const std::array<sed::ProjectType, 3> projectTypes{
            sed::ProjectType::JKDF2,
            sed::ProjectType::MOTS,
            sed::ProjectType::IJIM,
        };

        for (const sed::ProjectType projectType : projectTypes)
        {
            sed::Level level = makeVertexWeldChainLevel(projectType, kWeldDistance * 0.75);
            sed::assets::JklSaveOptions options{};
            options.weldNearbyVertices = true;
            options.vertexWeldDistance = kWeldDistance;

            std::ostringstream output{};
            sed::assets::saveToJKL(level, output, options);
            const ParsedJklGeometry geometry = parseJklGeometry(output.str());
            check(geometry.worldVertices.size() == 4u,
                "JKL vertex weld: nearby matching does not drift through a transitive chain");
            check(geometry.surfaceVertices.size() == 3u
                    && geometry.surfaceVertices[0].size() == 3u
                    && geometry.surfaceVertices[1].size() == 3u
                    && geometry.surfaceVertices[2].size() == 3u,
                "JKL vertex weld: chain fixture retains each source surface");
            if (geometry.surfaceVertices.size() == 3u
                && geometry.surfaceVertices[0].size() == 3u
                && geometry.surfaceVertices[1].size() == 3u
                && geometry.surfaceVertices[2].size() == 3u)
            {
                check(geometry.surfaceVertices[0][0] == geometry.surfaceVertices[1][0]
                        && geometry.surfaceVertices[1][0] != geometry.surfaceVertices[2][0],
                    "JKL vertex weld: chain representatives are checked against the original representative");
            }
        }
    }

    {
        constexpr double kWeldDistance = 3.0;
        const std::array<sed::ProjectType, 3> projectTypes{
            sed::ProjectType::JKDF2,
            sed::ProjectType::MOTS,
            sed::ProjectType::IJIM,
        };

        for (const sed::ProjectType projectType : projectTypes)
        {
            sed::Level level = makeOriginVertexWeldLevel(
                projectType,
                sed::TVector{ 1.0, 2.0, 2.0 });
            sed::assets::JklSaveOptions options{};
            options.weldNearbyVertices = true;
            options.vertexWeldDistance = kWeldDistance;

            std::ostringstream output{};
            sed::assets::saveToJKL(level, output, options);
            const ParsedJklGeometry geometry = parseJklGeometry(output.str());
            check(geometry.sectorBounds.size() == 2u
                    && geometry.sectorCenters.size() == 2u
                    && geometry.sectorRadii.size() == 2u,
                "JKL vertex weld: derived sector records are emitted for mapped geometry");
            check(geometry.surfaceNormals.size() == 2u,
                "JKL vertex weld: mapped surface normals are emitted for every face");

            if (geometry.sectorBounds.size() == 2u
                && geometry.sectorCenters.size() == 2u
                && geometry.sectorRadii.size() == 2u)
            {
                const sed::TBox& bounds = geometry.sectorBounds[1];
                const sed::TVector& center = geometry.sectorCenters[1];
                const std::array<sed::TVector, 3> mappedCorners{
                    sed::TVector{ 0.0, 0.0, 0.0 },
                    sed::TVector{ 10.0, 0.0, 0.0 },
                    sed::TVector{ 0.0, 10.0, 0.0 },
                };
                bool enclosesCorners = true;
                for (const sed::TVector& corner : mappedCorners)
                {
                    enclosesCorners = enclosesCorners
                        && bounds.x1 <= corner.x && corner.x <= bounds.x2
                        && bounds.y1 <= corner.y && corner.y <= bounds.y2
                        && bounds.z1 <= corner.z && corner.z <= bounds.z2
                        && std::hypot(
                            corner.x - center.x,
                            corner.y - center.y,
                            corner.z - center.z)
                            <= geometry.sectorRadii[1] + 1.0e-7;
                }
                const sed::TVector expectedCenter{
                    10.0 / 3.0,
                    10.0 / 3.0,
                    0.0,
                };
                const double expectedRadius = std::hypot(
                    20.0 / 3.0,
                    10.0 / 3.0);
                check(enclosesCorners
                        && closeVec(center, expectedCenter, 1.0e-6)
                        && close(geometry.sectorRadii[1], expectedRadius, 1.0e-6)
                        && close(bounds.z1, 0.0)
                        && close(bounds.z2, 0.0)
                        && close(center.z, 0.0),
                    "JKL vertex weld: bounds, center, and radius enclose mapped sector corners");
            }
            if (geometry.surfaceNormals.size() == 2u)
            {
                check(closeVec(geometry.surfaceNormals[1],
                        sed::TVector{ 0.0, 0.0, 1.0 }),
                    "JKL vertex weld: exported normal matches mapped triangle geometry");
            }
        }
    }

    {
        const std::array<sed::ProjectType, 3> projectTypes{
            sed::ProjectType::JKDF2,
            sed::ProjectType::MOTS,
            sed::ProjectType::IJIM,
        };
        for (const sed::ProjectType projectType : projectTypes)
        {
            sed::Level level = makeOriginVertexWeldLevel(
                projectType,
                sed::TVector{ 1.0, 2.0, 2.0 });
            level.sectors[1].flags |= sed::SECF_CollideBox;
            level.sectors[1].collideBox = sed::TBox{
                100.0, 101.0, 102.0,
                110.0, 111.0, 112.0,
            };

            sed::assets::JklSaveOptions options{};
            options.weldNearbyVertices = true;
            options.vertexWeldDistance = 3.0;

            std::ostringstream output{};
            sed::assets::saveToJKL(level, output, options);
            const ParsedJklGeometry geometry = parseJklGeometry(output.str());
            check(geometry.sectorCollideBoxes.size() == 1u,
                "JKL vertex weld: explicit collide box remains present with mapped geometry");
            if (geometry.sectorCollideBoxes.size() == 1u)
            {
                const sed::TBox& box = geometry.sectorCollideBoxes.front();
                check(close(box.x1, 100.0) && close(box.y1, 101.0)
                        && close(box.z1, 102.0) && close(box.x2, 110.0)
                        && close(box.y2, 111.0) && close(box.z2, 112.0),
                    "JKL vertex weld: explicit collide box values are authored values");
            }
        }
    }

    {
        sed::Level level = makeVertexWeldLevel(
            sed::ProjectType::JKDF2,
            1.0e-6,
            true);
        const std::array<double, 4> invalidDistances{
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(),
            0.0,
            -1.0,
        };

        for (const double invalidDistance : invalidDistances)
        {
            sed::assets::JklSaveOptions options{};
            options.weldNearbyVertices = true;
            options.vertexWeldDistance = invalidDistance;

            std::ostringstream output{};
            output << "existing stream content";
            bool threw = false;
            std::string diagnostic{};
            try
            {
                sed::assets::saveToJKL(level, output, options);
            }
            catch (const std::exception& exception)
            {
                threw = true;
                diagnostic = exception.what();
            }
            catch (...)
            {
                threw = true;
            }
            check(threw,
                "JKL vertex weld: enabled non-finite, zero, and negative distances are rejected");
            check(diagnostic == "Cannot export JKL/NDY: vertex weld distance must be finite and positive.",
                "JKL vertex weld: invalid distance diagnostic is stable");
            check(output.str() == "existing stream content",
                "JKL vertex weld: invalid distance rejection is atomic for caller-owned streams");
        }

        {
            sed::Level extreme = makeVertexWeldLevel(
                sed::ProjectType::JKDF2,
                1.0e-6,
                false);
            extreme.sectors[0].vertices[0].x = std::numeric_limits<double>::max();

            sed::assets::JklSaveOptions options{};
            options.weldNearbyVertices = true;
            options.vertexWeldDistance = 1.0e-5;

            std::ostringstream output{};
            output << "existing stream content";
            std::string diagnostic{};
            try
            {
                sed::assets::saveToJKL(extreme, output, options);
            }
            catch (const std::exception& exception)
            {
                diagnostic = exception.what();
            }
            check(diagnostic == "Cannot export JKL/NDY: nearby vertex welding spatial cell range is unsupported at sector 0, vertex 0.",
                "JKL vertex weld: extreme finite coordinate has a stable spatial-cell diagnostic");
            check(output.str() == "existing stream content",
                "JKL vertex weld: extreme spatial-cell rejection is atomic");
        }

        {
            sed::Level nonFinite = makeVertexWeldLevel(
                sed::ProjectType::JKDF2,
                1.0e-6,
                false);
            nonFinite.sectors[0].vertices[0].x = std::numeric_limits<double>::quiet_NaN();

            sed::assets::JklSaveOptions options{};
            options.weldNearbyVertices = true;
            options.vertexWeldDistance = 1.0e-5;

            std::ostringstream output{};
            output << "existing stream content";
            std::string diagnostic{};
            try
            {
                sed::assets::saveToJKL(nonFinite, output, options);
            }
            catch (const std::exception& exception)
            {
                diagnostic = exception.what();
            }
            check(diagnostic == "Cannot export JKL/NDY: world coordinates must be finite at sector 0, vertex 0.",
                "JKL vertex weld: non-finite source coordinate has a stable diagnostic");
            check(output.str() == "existing stream content",
                "JKL vertex weld: non-finite source-coordinate rejection is atomic");

            sed::assets::JklSaveOptions disabledOptions{};
            disabledOptions.weldNearbyVertices = false;
            disabledOptions.vertexWeldDistance = std::numeric_limits<double>::quiet_NaN();
            std::ostringstream disabledOutput{};
            disabledOutput << "existing stream content";
            diagnostic.clear();
            try
            {
                sed::assets::saveToJKL(nonFinite, disabledOutput, disabledOptions);
            }
            catch (const std::exception& exception)
            {
                diagnostic = exception.what();
            }
            check(diagnostic == "Cannot export JKL/NDY: world coordinates must be finite at sector 0, vertex 0.",
                "JKL vertex weld: exact default mode rejects non-finite source coordinate with the same diagnostic");
            check(disabledOutput.str() == "existing stream content",
                "JKL vertex weld: exact-mode non-finite rejection is atomic");
        }
    }

    {
        const std::array<sed::ProjectType, 3> projectTypes{
            sed::ProjectType::JKDF2,
            sed::ProjectType::MOTS,
            sed::ProjectType::IJIM,
        };
        for (const sed::ProjectType projectType : projectTypes)
        {
            sed::Level level = makeVertexWeldLevel(projectType, 1.0e-6, true);
            sed::assets::JklSaveOptions options{};
            options.weldNearbyVertices = true;
            options.vertexWeldDistance = 1.0e-5;

            std::ostringstream output{};
            sed::assets::saveToJKL(level, output, options);
            const ParsedJklGeometry geometry = parseJklGeometry(output.str());
            check(geometry.surfaceVertices.size() == 2u
                    && geometry.surfaceVertices[0].size() == 3u
                    && geometry.surfaceVertices[1].size() == 3u,
                "JKL vertex weld: adjoin fixture retains both source faces");
            if (geometry.surfaceVertices.size() == 2u
                && geometry.surfaceVertices[0].size() == 3u
                && geometry.surfaceVertices[1].size() == 3u)
            {
                check(geometry.surfaceVertices[0][0] == geometry.surfaceVertices[1][0]
                        && geometry.surfaceVertices[0][1] == geometry.surfaceVertices[1][2]
                        && geometry.surfaceVertices[0][2] == geometry.surfaceVertices[1][1],
                    "JKL vertex weld: adjoined surfaces share all welded coordinate indices");
                check(geometry.surfaceAdjoins.size() == 2u
                        && geometry.surfaceAdjoins[0] >= 0
                        && geometry.surfaceAdjoins[1] >= 0,
                    "JKL vertex weld: adjoin pair remains represented after nearby welding");
                if (geometry.surfaceAdjoins.size() == 2u
                    && geometry.adjoinMirrors.size() > static_cast<std::size_t>(
                        std::max(geometry.surfaceAdjoins[0], geometry.surfaceAdjoins[1])))
                {
                    check(geometry.adjoinMirrors[static_cast<std::size_t>(geometry.surfaceAdjoins[0])]
                            == geometry.surfaceAdjoins[1]
                            && geometry.adjoinMirrors[static_cast<std::size_t>(geometry.surfaceAdjoins[1])]
                            == geometry.surfaceAdjoins[0],
                        "JKL vertex weld: adjoin pair retains reciprocal world mirrors");
                }
            }
        }
    }

    {
        const std::array<sed::ProjectType, 3> projectTypes{
            sed::ProjectType::JKDF2,
            sed::ProjectType::MOTS,
            sed::ProjectType::IJIM,
        };
        for (const sed::ProjectType projectType : projectTypes)
        {
            sed::Level level = makeMappedAdjoinDistanceWeldLevel(projectType);
            sed::assets::JklSaveOptions options{};
            options.weldNearbyVertices = true;
            options.vertexWeldDistance = 1.0;

            std::ostringstream output{};
            sed::assets::saveToJKL(level, output, options);
            const ParsedJklGeometry geometry = parseJklGeometry(output.str());
            check(geometry.adjoinDistances.size() == 2u
                    && close(geometry.adjoinDistances[0], 0.0, 1.0e-6)
                    && close(geometry.adjoinDistances[1], 0.0, 1.0e-6),
                "JKL vertex weld: adjoin distances use mapped sector centers and normals");
        }
    }

    {
        const std::array<sed::ProjectType, 3> projectTypes{
            sed::ProjectType::JKDF2,
            sed::ProjectType::MOTS,
            sed::ProjectType::IJIM,
        };
        for (const sed::ProjectType projectType : projectTypes)
        {
            sed::Level level = makeNonPlanarQuadWeldLevel(projectType);
            const sed::Level sourceBefore = level;
            sed::assets::JklSaveOptions options{};
            options.weldNearbyVertices = true;
            options.vertexWeldDistance = 0.75;

            std::ostringstream output{};
            output << "existing stream content";
            bool threw = false;
            std::string diagnostic{};
            try
            {
                sed::assets::saveToJKL(level, output, options);
            }
            catch (const std::exception& exception)
            {
                threw = true;
                diagnostic = exception.what();
            }
            catch (...)
            {
                threw = true;
            }
            check(threw,
                "JKL vertex weld: opt-in welding rejects a newly non-planar quad");
            check(diagnostic == "Cannot export JKL/NDY: nearby vertex welding makes surface non-planar at sector 0, surface 0.",
                "JKL vertex weld: non-planar quad diagnostic is stable");
            check(output.str() == "existing stream content"
                    && sameWeldSourceGeometry(level, sourceBefore),
                "JKL vertex weld: non-planar quad rejection preserves stream and source state");
        }
    }

    {
        const std::array<sed::ProjectType, 3> projectTypes{
            sed::ProjectType::JKDF2,
            sed::ProjectType::MOTS,
            sed::ProjectType::IJIM,
        };
        for (const sed::ProjectType projectType : projectTypes)
        {
            sed::Level level = makeConcaveQuadWeldLevel(projectType);
            const sed::Level sourceBefore = level;
            sed::assets::JklSaveOptions options{};
            options.weldNearbyVertices = true;
            options.vertexWeldDistance = 2.0;

            std::ostringstream output{};
            output << "existing stream content";
            bool threw = false;
            std::string diagnostic{};
            try
            {
                sed::assets::saveToJKL(level, output, options);
            }
            catch (const std::exception& exception)
            {
                threw = true;
                diagnostic = exception.what();
            }
            catch (...)
            {
                threw = true;
            }
            check(threw,
                "JKL vertex weld: opt-in welding rejects a newly concave quad");
            check(diagnostic == "Cannot export JKL/NDY: nearby vertex welding makes surface concave at sector 0, surface 0.",
                "JKL vertex weld: concave quad diagnostic is stable");
            check(output.str() == "existing stream content"
                    && sameWeldSourceGeometry(level, sourceBefore),
                "JKL vertex weld: concave quad rejection preserves stream and source state");
        }
    }

    {
        const std::array<sed::ProjectType, 3> projectTypes{
            sed::ProjectType::JKDF2,
            sed::ProjectType::MOTS,
            sed::ProjectType::IJIM,
        };
        for (const sed::ProjectType projectType : projectTypes)
        {
            sed::Level level = makeConvexSectorWeldLevel(projectType);
            const sed::Level sourceBefore = level;
            sed::assets::JklSaveOptions options{};
            options.weldNearbyVertices = true;
            options.vertexWeldDistance = 2.0;

            std::ostringstream output{};
            output << "existing stream content";
            bool threw = false;
            std::string diagnostic{};
            try
            {
                sed::assets::saveToJKL(level, output, options);
            }
            catch (const std::exception& exception)
            {
                threw = true;
                diagnostic = exception.what();
            }
            catch (...)
            {
                threw = true;
            }
            check(threw,
                "JKL vertex weld: opt-in welding rejects a convex sector made non-convex");
            check(diagnostic == "Cannot export JKL/NDY: nearby vertex welding makes sector non-convex at sector 1.",
                "JKL vertex weld: convex-sector diagnostic is stable");
            check(output.str() == "existing stream content"
                    && sameWeldSourceGeometry(level, sourceBefore),
                "JKL vertex weld: convex-sector rejection preserves stream and source state");
        }
    }

    {
        const std::array<sed::ProjectType, 3> projectTypes{
            sed::ProjectType::JKDF2,
            sed::ProjectType::MOTS,
            sed::ProjectType::IJIM,
        };
        for (const sed::ProjectType projectType : projectTypes)
        {
            sed::Level level = makeCollapsedWeldLevel(projectType);
            bool validFixture = false;
            if (level.sectors.size() == 1u
                && level.sectors[0].vertices.size() == 3u
                && level.sectors[0].surfaces.size() == 1u)
            {
                const sed::Sector& sector = level.sectors[0];
                const std::vector<std::int32_t>& corners = sector.surfaces[0].vertices;
                if (corners.size() == 3u
                    && std::all_of(corners.begin(), corners.end(), [&sector](const std::int32_t index) {
                           return index >= 0
                               && static_cast<std::size_t>(index) < sector.vertices.size();
                       })
                    && corners[0] != corners[1]
                    && corners[0] != corners[2]
                    && corners[1] != corners[2])
                {
                    const sed::Vertex& first = sector.vertices[
                        static_cast<std::size_t>(corners[0])];
                    const sed::Vertex& second = sector.vertices[
                        static_cast<std::size_t>(corners[1])];
                    validFixture = close(
                        std::hypot(
                            first.x - second.x,
                            first.y - second.y,
                            first.z - second.z),
                        1.0e-6,
                        1.0e-12);
                }
            }
            check(validFixture,
                "JKL vertex weld: collapsed-face fixture has three valid distinct corners with a close pair");
            const sed::Level sourceBefore = level;
            sed::assets::JklSaveOptions options{};
            options.weldNearbyVertices = true;
            options.vertexWeldDistance = 1.0e-5;

            std::ostringstream output{};
            output << "existing stream content";
            bool threw = false;
            std::string diagnostic{};
            try
            {
                sed::assets::saveToJKL(level, output, options);
            }
            catch (const std::exception& exception)
            {
                threw = true;
                diagnostic = exception.what();
            }
            catch (...)
            {
                threw = true;
            }
            check(threw,
                "JKL vertex weld: opt-in welding rejects a face that would collapse to repeated indices");
            check(diagnostic == "Cannot export JKL/NDY: nearby vertex welding collapses surface at sector 0, surface 0.",
                "JKL vertex weld: collapsed-face diagnostic is stable");
            check(output.str() == "existing stream content"
                    && sameWeldSourceGeometry(level, sourceBefore),
                "JKL vertex weld: collapsed-face rejection preserves stream and source state");
        }
    }

    {
        const std::array<sed::ProjectType, 3> projectTypes{
            sed::ProjectType::JKDF2,
            sed::ProjectType::MOTS,
            sed::ProjectType::IJIM,
        };
        const std::array<sed::ColorF, 4> authoredExtraLights{
            sed::ColorF{ 10.0f, 10.0f, 10.0f, 1.5f },
            sed::ColorF{ 1.08f, 1.08f, 1.08f, -0.5f },
            sed::ColorF{ -2.0f, -2.0f, -2.0f, 2.0f },
            sed::ColorF{ 10.0f, 1.08f, -2.0f, 0.75f },
        };
        // This is the fixed mixed-channel scalar vector.  The classic writer
        // averages the authored channels before applying its signed clamp.
        constexpr float mixedClassicIntensity = 3.026666667f;
        const std::array<sed::ColorF, 4> clampedClassic{
            sed::ColorF{ 1.0f, 1.0f, 1.0f, 1.0f },
            sed::ColorF{ 1.0f, 1.0f, 1.0f, 1.0f },
            sed::ColorF{ -1.0f, -1.0f, -1.0f, 1.0f },
            sed::ColorF{ 1.0f, 1.0f, 1.0f, 1.0f },
        };
        const std::array<sed::ColorF, 4> clampedIjim{
            sed::ColorF{ 1.0f, 1.0f, 1.0f, 1.0f },
            sed::ColorF{ 1.0f, 1.0f, 1.0f, 0.0f },
            sed::ColorF{ -1.0f, -1.0f, -1.0f, 1.0f },
            sed::ColorF{ 1.0f, 1.0f, -1.0f, 0.75f },
        };
        const std::array<sed::ColorF, 4> preservedClassic{
            sed::ColorF{ 10.0f, 10.0f, 10.0f, 1.0f },
            sed::ColorF{ 1.08f, 1.08f, 1.08f, 1.0f },
            sed::ColorF{ -2.0f, -2.0f, -2.0f, 1.0f },
            sed::ColorF{
                mixedClassicIntensity,
                mixedClassicIntensity,
                mixedClassicIntensity,
                1.0f },
        };
        const std::array<sed::ColorF, 4> preservedIjim{
            sed::ColorF{ 10.0f, 10.0f, 10.0f, 1.0f },
            sed::ColorF{ 1.08f, 1.08f, 1.08f, 0.0f },
            sed::ColorF{ -2.0f, -2.0f, -2.0f, 1.0f },
            sed::ColorF{ 10.0f, 1.08f, -2.0f, 0.75f },
        };

        const auto checkWireExtraLights = [](
            const ParsedJklGeometry& geometry,
            const std::array<sed::ColorF, 4>& expected,
            const char* message)
        {
            bool matches = geometry.surfaceExtraLights.size() == expected.size()
                && geometry.surfaceExtraLightParsed.size() == expected.size();
            if (matches)
            {
                for (std::size_t index = 0; index < expected.size(); ++index)
                {
                    const sed::ColorF& actual = geometry.surfaceExtraLights[index];
                    const sed::ColorF& wanted = expected[index];
                    matches = matches
                        && geometry.surfaceExtraLightParsed[index]
                        && close(actual.r, wanted.r, 1.0e-5)
                        && close(actual.g, wanted.g, 1.0e-5)
                        && close(actual.b, wanted.b, 1.0e-5)
                        && close(actual.a, wanted.a, 1.0e-5);
                }
            }
            check(matches, message);
        };

        check(sed::assets::JklSaveOptions{}.clampExtraLight,
            "JKL extra light: clamping is enabled by default");

        for (const sed::ProjectType projectType : projectTypes)
        {
            const bool isIjim = projectType == sed::ProjectType::IJIM;
            const sed::Level source = makeExtraLightExportLevel(projectType);

            sed::Level defaultLevel = source;
            const sed::Level defaultBefore = defaultLevel;
            std::ostringstream defaultOutput{};
            sed::assets::saveToJKL(defaultLevel, defaultOutput);
            checkWireExtraLights(
                parseJklGeometry(defaultOutput.str()),
                isIjim ? clampedIjim : clampedClassic,
                "JKL extra light: default export clamps signed scalar/RGB values and alpha");
            check(sameWeldSourceGeometry(defaultLevel, defaultBefore),
                "JKL extra light: default clamp leaves authored source values unchanged");

            sed::Level enabledLevel = source;
            const sed::Level enabledBefore = enabledLevel;
            sed::assets::JklSaveOptions enabledOptions{};
            enabledOptions.clampExtraLight = true;
            std::ostringstream enabledOutput{};
            sed::assets::saveToJKL(enabledLevel, enabledOutput, enabledOptions);
            checkWireExtraLights(
                parseJklGeometry(enabledOutput.str()),
                isIjim ? clampedIjim : clampedClassic,
                "JKL extra light: enabled clamp emits independently checked wire values");
            check(sameWeldSourceGeometry(enabledLevel, enabledBefore),
                "JKL extra light: enabled clamp leaves authored source values unchanged");

            sed::Level disabledLevel = source;
            const sed::Level disabledBefore = disabledLevel;
            sed::assets::JklSaveOptions disabledOptions{};
            disabledOptions.clampExtraLight = false;
            std::ostringstream disabledOutput{};
            sed::assets::saveToJKL(disabledLevel, disabledOutput, disabledOptions);
            checkWireExtraLights(
                parseJklGeometry(disabledOutput.str()),
                isIjim ? preservedIjim : preservedClassic,
                "JKL extra light: disabled clamp preserves signed scalar/RGB wire values");
            check(sameWeldSourceGeometry(disabledLevel, disabledBefore),
                "JKL extra light: disabled clamp leaves authored source values unchanged");
        }
    }

    {
        const std::array<sed::ProjectType, 3> projectTypes{
            sed::ProjectType::JKDF2,
            sed::ProjectType::MOTS,
            sed::ProjectType::IJIM,
        };
        for (const sed::ProjectType projectType : projectTypes)
        {
            for (const bool clampExtraLight : { true, false })
            {
                sed::Level level = makeExtraLightExportLevel(projectType);
                level.sectors[0].surfaces[0].extraLight.r =
                    std::numeric_limits<float>::quiet_NaN();
                const sed::Level sourceBefore = level;

                sed::assets::JklSaveOptions options{};
                options.clampExtraLight = clampExtraLight;
                std::ostringstream output{};
                output << "existing stream content";
                bool threw = false;
                std::string diagnostic{};
                try
                {
                    sed::assets::saveToJKL(level, output, options);
                }
                catch (const std::runtime_error& exception)
                {
                    threw = true;
                    diagnostic = exception.what();
                }
                check(threw
                        && diagnostic
                            == "Cannot export JKL/NDY extra light: a color value is not finite.",
                    "JKL extra light: finite validation precedes clamping in each option state");
                check(output.str() == "existing stream content"
                        && std::isnan(level.sectors[0].surfaces[0].extraLight.r)
                        && sameWeldSourceGeometry(level, sourceBefore),
                    "JKL extra light: non-finite rejection is atomic and source-preserving");
            }
        }
    }
}

} // namespace sed::tests
