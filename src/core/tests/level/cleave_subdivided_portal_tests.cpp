#include "test_common.hpp"

#include "sed/assets/sed.hpp"
#include "sed/level/cleave.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace sed::tests
{

namespace
{

struct AuthoredCorner
{
    double x{};
    double y{};
    double z{};
    TexVertex texture{};
};

bool sameTextureVertex(const TexVertex& first, const TexVertex& second) noexcept
{
    return first.u == second.u
        && first.v == second.v
        && first.color == second.color;
}

bool samePoint(const Vertex& first, const AuthoredCorner& second) noexcept
{
    return first.x == second.x && first.y == second.y && first.z == second.z;
}

bool containsAuthoredCorner(
    const Level& level,
    const std::int32_t sectorIndex,
    const AuthoredCorner& expected) noexcept
{
    if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
    {
        return false;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    for (const Surface& surface : sector.surfaces)
    {
        for (std::size_t corner = 0;
            corner < surface.vertices.size() && corner < surface.texVertices.size();
            ++corner)
        {
            const std::int32_t vertexIndex = surface.vertices[corner];
            if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
            {
                continue;
            }
            if (samePoint(sector.vertices[static_cast<std::size_t>(vertexIndex)], expected)
                && sameTextureVertex(surface.texVertices[corner], expected.texture))
            {
                return true;
            }
        }
    }
    return false;
}

bool containsInterpolatedCorner(
    const Level& level,
    const std::int32_t sectorIndex,
    const TVector& point,
    const TexVertex& texture) noexcept
{
    if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
    {
        return false;
    }
    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    for (const Surface& surface : sector.surfaces)
    {
        for (std::size_t corner = 0;
            corner < surface.vertices.size() && corner < surface.texVertices.size();
            ++corner)
        {
            const std::int32_t vertexIndex = surface.vertices[corner];
            if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
            {
                continue;
            }
            const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
            const TexVertex& actual = surface.texVertices[corner];
            if (close(vertex.x, point.x, 1.0e-12)
                && close(vertex.y, point.y, 1.0e-12)
                && close(vertex.z, point.z, 1.0e-12)
                && close(actual.u, texture.u, 1.0e-6)
                && close(actual.v, texture.v, 1.0e-6)
                && close(actual.color.r, texture.color.r, 1.0e-6)
                && close(actual.color.g, texture.color.g, 1.0e-6)
                && close(actual.color.b, texture.color.b, 1.0e-6)
                && close(actual.color.a, texture.color.a, 1.0e-6))
            {
                return true;
            }
        }
    }
    return false;
}

bool containsPoint(
    const Level& level,
    const std::int32_t sectorIndex,
    const TVector& expected,
    const double epsilon = 1.0e-12) noexcept
{
    if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
    {
        return false;
    }
    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    return std::any_of(
        sector.vertices.begin(),
        sector.vertices.end(),
        [&expected, epsilon](const Vertex& point)
        {
            return close(point.x, expected.x, epsilon)
                && close(point.y, expected.y, epsilon)
                && close(point.z, expected.z, epsilon);
        });
}

bool hasClosedMesh(const Level& level, const std::int32_t sectorIndex) noexcept
{
    if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
    {
        return false;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    if (sector.surfaces.empty())
    {
        return false;
    }

    std::map<std::pair<std::int32_t, std::int32_t>, std::size_t> edgeUse;
    std::map<std::pair<std::int32_t, std::int32_t>, std::size_t> directedEdgeUse;
    for (const Surface& surface : sector.surfaces)
    {
        if (surface.vertices.size() < 3u || surface.vertices.size() != surface.texVertices.size()
            || surface.vertices.front() == surface.vertices.back())
        {
            return false;
        }
        for (std::size_t corner = 0; corner < surface.vertices.size(); ++corner)
        {
            const std::int32_t first = surface.vertices[corner];
            const std::int32_t second = surface.vertices[(corner + 1) % surface.vertices.size()];
            if (first < 0 || second < 0
                || static_cast<std::size_t>(first) >= sector.vertices.size()
                || static_cast<std::size_t>(second) >= sector.vertices.size()
                || first == second)
            {
                return false;
            }
            if (std::find(
                    surface.vertices.begin() + static_cast<std::ptrdiff_t>(corner + 1),
                    surface.vertices.end(),
                    first) != surface.vertices.end())
            {
                return false;
            }
            ++edgeUse[std::minmax(first, second)];
            ++directedEdgeUse[{ first, second }];
        }
    }

    for ( const auto& entry : edgeUse )
    {
        if ( entry.second != 2u )
        {
            return false;
        }
        const auto forward = directedEdgeUse.find(entry.first);
        const auto reverse = directedEdgeUse.find(
            { entry.first.second, entry.first.first });
        if ( forward == directedEdgeUse.end()
            || reverse == directedEdgeUse.end()
            || forward->second != 1u
            || reverse->second != 1u )
        {
            return false;
        }
    }
    return true;
}

bool reciprocalPortalLinks(
    const Level& level,
    const std::size_t expectedLinks) noexcept
{
    if (level.sectors.size() < 2u)
    {
        return false;
    }

    std::size_t links = 0;
    for (std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex )
    {
        const Sector& sector = level.sectors[sectorIndex];
        for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
        {
            const auto& adjoin = sector.surfaces[surfaceIndex].adjoin;
            if (!adjoin)
            {
                continue;
            }
            if (adjoin->sector < 0 || static_cast<std::size_t>(adjoin->sector) >= level.sectors.size()
                || adjoin->surface < 0
                || static_cast<std::size_t>(adjoin->surface)
                    >= level.sectors[static_cast<std::size_t>(adjoin->sector)].surfaces.size())
            {
                return false;
            }
            const auto& back = level.sectors[static_cast<std::size_t>(adjoin->sector)]
                .surfaces[static_cast<std::size_t>(adjoin->surface)].adjoin;
            if (!back || back->sector != static_cast<std::int32_t>(sectorIndex)
                || back->surface != static_cast<std::int32_t>(surfaceIndex))
            {
                return false;
            }
            if (adjoin->sector != static_cast<std::int32_t>(sectorIndex))
            {
                ++links;
            }
        }
    }
    return links == expectedLinks;
}

Surface makeFace(
    Sector& sector,
    const std::vector<std::int32_t>& vertices,
    const std::vector<TexVertex>& texVertices)
{
    Surface face;
    face.vertices = vertices;
    face.texVertices = texVertices;
    face.normal = sed::level::surfaceCalcNormalNewell(sector, face);
    return face;
}

TexVertex sideTexture(const std::size_t face, const std::size_t corner) noexcept
{
    const float offset = static_cast<float>(face * 0.07 + corner * 0.013);
    return TexVertex{
        0.2f + offset,
        0.8f - offset,
        ColorF{
            0.1f + static_cast<float>(face) * 0.03f,
            0.2f + static_cast<float>(corner) * 0.04f,
            0.7f,
            0.55f + static_cast<float>(face) * 0.02f,
        },
    };
}

void addCubeFaces(
    Sector& sector,
    const bool mirror,
    const bool subdividePortalEdge)
{
    const std::int32_t midpoint = subdividePortalEdge
        ? static_cast<std::int32_t>(sector.vertices.size() - 1)
        : -1;

    if (!mirror)
    {
        sector.addSurface(makeFace(
            sector,
            { 0, 1, 2, 3 },
            {
                { 0.125f, 0.375f, ColorF{ 0.11f, 0.23f, 0.37f, 0.41f } },
                { 0.875f, 0.125f, ColorF{ 0.59f, 0.17f, 0.31f, 0.73f } },
                { 0.625f, 0.9375f, ColorF{ 0.79f, 0.43f, 0.19f, 0.67f } },
                { 0.0625f, 0.8125f, ColorF{ 0.27f, 0.71f, 0.83f, 0.53f } },
            }));
    }
    else
    {
        sector.addSurface(makeFace(
            sector,
            { 0, 3, 2, 1, midpoint },
            {
                { 0.9375f, 0.0625f, ColorF{ 0.91f, 0.13f, 0.29f, 0.47f } },
                { 0.1875f, 0.6875f, ColorF{ 0.21f, 0.67f, 0.41f, 0.59f } },
                { 0.8125f, 0.875f, ColorF{ 0.47f, 0.39f, 0.77f, 0.83f } },
                { 0.3125f, 0.1875f, ColorF{ 0.73f, 0.57f, 0.17f, 0.61f } },
                { 0.03125f, 0.96875f, ColorF{ 0.37f, 0.11f, 0.89f, 0.71f } },
            }));
    }

    const auto addSide = [&](const std::vector<std::int32_t>& vertices)
    {
        std::vector<TexVertex> texVertices;
        texVertices.reserve(vertices.size());
        for (std::size_t corner = 0; corner < vertices.size(); ++corner)
        {
            texVertices.push_back(sideTexture(sector.surfaces.size(), corner));
        }
        sector.addSurface(makeFace(sector, vertices, texVertices));
    };
    const auto addReversedSide = [&](std::vector<std::int32_t> vertices)
    {
        std::vector<TexVertex> texVertices;
        texVertices.reserve(vertices.size());
        for (std::size_t corner = 0; corner < vertices.size(); ++corner)
        {
            texVertices.push_back(sideTexture(sector.surfaces.size(), corner));
        }
        std::reverse(vertices.begin(), vertices.end());
        std::reverse(texVertices.begin(), texVertices.end());
        sector.addSurface(makeFace(sector, vertices, texVertices));
    };

    if (!mirror)
    {
        addSide({ 4, 7, 6, 5 });
        addSide({ 0, 4, 5, 1 });
        addSide({ 1, 5, 6, 2 });
        addSide({ 2, 6, 7, 3 });
        addSide({ 3, 7, 4, 0 });
    }
    else
    {
        addSide({ 4, 5, 6, 7 });
        // Keep the midpoint on the incident side face so the source and
        // mirror shells remain closed before and after the portal cleave.
        addReversedSide({ 0, 4, 5, 1, midpoint });
        addReversedSide({ 1, 5, 6, 2 });
        addReversedSide({ 2, 6, 7, 3 });
        addReversedSide({ 3, 7, 4, 0 });
    }
}

Level makeSubdividedPortalLevel(const double mirrorLeft, const double midpointY = 0.0)
{
    Level level;
    level.kind = ProjectType::IJIM;
    level.uvCoordinateSpace = UvCoordinateSpace::Normalized;

    Sector& source = level.newSector();
    source.vertices = {
        { 0.0, 0.0, 0.0, 0 },
        { 2.0, 0.0, 0.0, 0 },
        { 2.0, 2.0, 0.0, 0 },
        { 0.0, 2.0, 0.0, 0 },
        { 0.0, 0.0, -2.0, 0 },
        { 2.0, 0.0, -2.0, 0 },
        { 2.0, 2.0, -2.0, 0 },
        { 0.0, 2.0, -2.0, 0 },
    };
    addCubeFaces(source, false, false);

    Sector& mirror = level.newSector();
    mirror.vertices = {
        { mirrorLeft, 0.0, 0.0, 0 },
        { 2.0, 0.0, 0.0, 0 },
        { 2.0, 2.0, 0.0, 0 },
        { mirrorLeft, 2.0, 0.0, 0 },
        { mirrorLeft, 0.0, 2.0, 0 },
        { 2.0, 0.0, 2.0, 0 },
        { 2.0, 2.0, 2.0, 0 },
        { mirrorLeft, 2.0, 2.0, 0 },
        { 1.0, midpointY, 0.0, 0 },
    };
    addCubeFaces(mirror, true, true);

    level.sectors[0].surfaces[0].adjoin = SurfaceRef{ 1, 0 };
    level.sectors[0].surfaces[0].nAdj = 0;
    level.sectors[1].surfaces[0].adjoin = SurfaceRef{ 0, 0 };
    level.sectors[1].surfaces[0].nAdj = 0;
    return level;
}

Level makeMultiMidpointPortalLevel()
{
    Level level = makeSubdividedPortalLevel(0.0, 1.0e-9);
    Sector& mirror = level.sectors[1];
    mirror.vertices[8].x = 0.5;
    mirror.vertices.push_back({ 1.5, 1.0e-9, 0.0, 0 });
    Surface& portal = mirror.surfaces[0];
    portal.vertices = { 0, 3, 2, 1, 9, 8 };
    portal.texVertices.insert(portal.texVertices.end() - 1, {
        0.111f, 0.222f, ColorF{ 0.17f, 0.29f, 0.43f, 0.71f } });
    portal.normal = sed::level::surfaceCalcNormalNewell(mirror, portal);
    Surface& incidentWall = mirror.surfaces[2];
    incidentWall.vertices = { 8, 9, 1, 5, 4, 0 };
    incidentWall.texVertices.insert(incidentWall.texVertices.begin() + 1, {
        0.333f, 0.444f, ColorF{ 0.23f, 0.31f, 0.47f, 0.79f } });
    incidentWall.normal = sed::level::surfaceCalcNormalNewell(mirror, incidentWall);
    return level;
}

void appendDuplicateIdIncidentFace(
    Sector& sector,
    const std::int32_t sharedAnchor,
    const double primaryMismatchX,
    const double row,
    const double bend)
{
    const auto appendVertex = [&sector](
        const double x,
        const double y,
        const double z)
    {
        sector.vertices.push_back({ x, y, z, 0 });
        return static_cast<std::int32_t>(sector.vertices.size() - 1);
    };

    // The first four corners are an extended horizontal run. The bend is an
    // independently authored, small in-plane residual that gives the chain a
    // measured source deviation. The last two corners close the run one unit
    // away from the portal plane while remaining on z = y - row.
    const std::int32_t outerLeft = appendVertex(-1.0, row, 0.0);
    const std::int32_t duplicateMismatch = appendVertex(primaryMismatchX, row, 0.0);
    const std::int32_t redundantBend = appendVertex(1.0, row + bend, bend);
    const std::int32_t outerRight = appendVertex(3.0, row, 0.0);
    const std::int32_t closeRight = appendVertex(3.0, row - 1.0, -1.0);
    const std::int32_t closeLeft = appendVertex(-1.0, row - 1.0, -1.0);
    sector.addSurface(makeFace(
        sector,
        { outerLeft, duplicateMismatch, redundantBend, sharedAnchor,
            outerRight, closeRight, closeLeft },
        {
            sideTexture(sector.surfaces.size(), 0),
            sideTexture(sector.surfaces.size(), 1),
            sideTexture(sector.surfaces.size(), 2),
            sideTexture(sector.surfaces.size(), 3),
            sideTexture(sector.surfaces.size(), 4),
            sideTexture(sector.surfaces.size(), 5),
            sideTexture(sector.surfaces.size(), 6),
        }));
}

Level makeDuplicateIdIncidentPortalLevel()
{
    constexpr double mirrorLeft = 1.0e-9;
    constexpr double bend = 2.0e-9;
    Level level = makeSubdividedPortalLevel(mirrorLeft, mirrorLeft);
    if ( level.sectors.size() != 2u
        || level.sectors[0].surfaces.empty()
        || level.sectors[1].surfaces.empty() )
    {
        return {};
    }

    // Each incident face shares only its indexed x=2 anchor with the scoped
    // portal. The mismatch point is deliberately a coordinate duplicate with
    // a fresh Sector vertex ID, so a coordinate-only contour lookup can borrow
    // this disconnected face as false evidence.
    appendDuplicateIdIncidentFace(level.sectors[0], 1, 0.0, 0.0, bend);
    appendDuplicateIdIncidentFace(level.sectors[0], 2, 0.0, 2.0, bend);
    appendDuplicateIdIncidentFace(level.sectors[1], 1, mirrorLeft, 0.0, bend);
    appendDuplicateIdIncidentFace(level.sectors[1], 2, mirrorLeft, 2.0, bend);
    return level;
}

Level makeThreeSectorRoundedPortalLevel()
{
    Level level = makeSubdividedPortalLevel(0.0, 1.0e-9);
    if ( level.sectors.size() != 2u )
    {
        return {};
    }
    Sector& third = level.newSector();
    third.vertices = {
        { 0.0, 0.0, -2.0, 0 }, { 2.0, 0.0, -2.0, 0 },
        { 2.0, 0.0, 2.0, 0 }, { 0.0, 0.0, 2.0, 0 },
        { 0.0, -2.0, -2.0, 0 }, { 2.0, -2.0, -2.0, 0 },
        { 2.0, -2.0, 2.0, 0 }, { 0.0, -2.0, 2.0, 0 },
    };
    addCubeFaces(third, false, false);
    for (Surface& surface : third.surfaces)
    {
        std::reverse(surface.vertices.begin(), surface.vertices.end());
        std::reverse(surface.texVertices.begin(), surface.texVertices.end());
        surface.normal = sed::level::surfaceCalcNormalNewell(third, surface);
    }
    third.vertices.push_back({ 0.0, 0.0, 0.0, 0 });
    third.vertices.push_back({ 2.0, 0.0, 0.0, 0 });
    const auto makeSyntheticFace = [&third](const std::vector<std::int32_t>& vertices,
        const std::size_t textureSeed)
    {
        std::vector<TexVertex> texVertices;
        texVertices.reserve(vertices.size());
        for (std::size_t corner = 0; corner < vertices.size(); ++corner)
        {
            texVertices.push_back(sideTexture(textureSeed, corner));
        }
        return makeFace(third, vertices, texVertices);
    };
    third.surfaces[0] = makeSyntheticFace({ 0, 8, 9, 1 }, 0);
    third.surfaces[3].vertices.push_back(9);
    third.surfaces[3].texVertices.insert(
        third.surfaces[3].texVertices.end(), sideTexture(3, 4));
    third.surfaces[3].normal = sed::level::surfaceCalcNormalNewell(
        third, third.surfaces[3]);
    third.surfaces[5].vertices.push_back(8);
    third.surfaces[5].texVertices.insert(
        third.surfaces[5].texVertices.end(), sideTexture(5, 4));
    third.surfaces[5].normal = sed::level::surfaceCalcNormalNewell(
        third, third.surfaces[5]);
    third.surfaces.push_back(makeSyntheticFace({ 3, 2, 9, 8 }, 6));

    Surface& sourcePortal = level.sectors[0].surfaces[2];
    Surface& mirrorIncident = level.sectors[1].surfaces[2];
    Surface& thirdLowerPortal = third.surfaces[0];
    Surface& thirdUpperPortal = third.surfaces[6];
    sourcePortal.adjoin = SurfaceRef{ 2, 0 };
    sourcePortal.nAdj = 0;
    thirdLowerPortal.adjoin = SurfaceRef{ 0, 2 };
    thirdLowerPortal.nAdj = 2;
    mirrorIncident.adjoin = SurfaceRef{ 2, 6 };
    mirrorIncident.nAdj = 6;
    thirdUpperPortal.adjoin = SurfaceRef{ 1, 2 };
    thirdUpperPortal.nAdj = 2;

    if ( !hasClosedMesh(level, 0) || !hasClosedMesh(level, 1)
        || !hasClosedMesh(level, 2) )
    {
        return {};
    }
    const TVector sourceNormal = sourcePortal.normal.normalized();
    const TVector mirrorNormal = mirrorIncident.normal.normalized();
    const TVector lowerNormal = thirdLowerPortal.normal.normalized();
    const TVector upperNormal = thirdUpperPortal.normal.normalized();
    if ( sourceNormal.dot(lowerNormal) >= 0.0
        || mirrorNormal.dot(upperNormal) >= 0.0
        || sourcePortal.vertices.size() != thirdLowerPortal.vertices.size()
        || !containsPoint(level, 0, { 0.0, 0.0, 0.0 })
        || !containsPoint(level, 2, { 0.0, 0.0, 0.0 })
        || !containsPoint(level, 0, { 2.0, 0.0, 0.0 })
        || !containsPoint(level, 2, { 2.0, 0.0, 0.0 })
        || !containsPoint(level, 1, { 1.0, 1.0e-9, 0.0 }) )
    {
        return {};
    }
    return level;
}

} // namespace

void runCleaveSubdividedPortalTests()
{
    const std::vector<AuthoredCorner> sourceCorners = {
        { 0.0, 0.0, 0.0, { 0.125f, 0.375f, ColorF{ 0.11f, 0.23f, 0.37f, 0.41f } } },
        { 2.0, 0.0, 0.0, { 0.875f, 0.125f, ColorF{ 0.59f, 0.17f, 0.31f, 0.73f } } },
        { 2.0, 2.0, 0.0, { 0.625f, 0.9375f, ColorF{ 0.79f, 0.43f, 0.19f, 0.67f } } },
        { 0.0, 2.0, 0.0, { 0.0625f, 0.8125f, ColorF{ 0.27f, 0.71f, 0.83f, 0.53f } } },
    };
    const std::vector<AuthoredCorner> mirrorCorners = {
        { 0.0, 0.0, 0.0, { 0.9375f, 0.0625f, ColorF{ 0.91f, 0.13f, 0.29f, 0.47f } } },
        { 0.0, 2.0, 0.0, { 0.1875f, 0.6875f, ColorF{ 0.21f, 0.67f, 0.41f, 0.59f } } },
        { 2.0, 2.0, 0.0, { 0.8125f, 0.875f, ColorF{ 0.47f, 0.39f, 0.77f, 0.83f } } },
        { 2.0, 0.0, 0.0, { 0.3125f, 0.1875f, ColorF{ 0.73f, 0.57f, 0.17f, 0.61f } } },
        { 1.0, 0.0, 0.0, { 0.03125f, 0.96875f, ColorF{ 0.37f, 0.11f, 0.89f, 0.71f } } },
    };
    const std::vector<AuthoredCorner> roundedMirrorCorners = {
        { 0.0, 0.0, 0.0, { 0.9375f, 0.0625f, ColorF{ 0.91f, 0.13f, 0.29f, 0.47f } } },
        { 0.0, 2.0, 0.0, { 0.1875f, 0.6875f, ColorF{ 0.21f, 0.67f, 0.41f, 0.59f } } },
        { 2.0, 2.0, 0.0, { 0.8125f, 0.875f, ColorF{ 0.47f, 0.39f, 0.77f, 0.83f } } },
        { 2.0, 0.0, 0.0, { 0.3125f, 0.1875f, ColorF{ 0.73f, 0.57f, 0.17f, 0.61f } } },
        { 1.0, 1.0e-9, 0.0, { 0.03125f, 0.96875f, ColorF{ 0.37f, 0.11f, 0.89f, 0.71f } } },
    };

    {
        Level level = makeSubdividedPortalLevel(0.0);
        check(hasClosedMesh(level, 0) && hasClosedMesh(level, 1),
            "Cleave subdivided reciprocal portal: initial cube shells are closed");
        check(reciprocalPortalLinks(level, 2u),
            "Cleave subdivided reciprocal portal: initial portal links are reciprocal");
        check(level.sectors[0].surfaces[0].vertices == std::vector<std::int32_t>{ 0, 1, 2, 3 }
                && level.sectors[1].surfaces[0].vertices == std::vector<std::int32_t>{ 0, 3, 2, 1, 8 },
            "Cleave subdivided reciprocal portal: mirror alone subdivides the source long edge");

        const bool accepted = sed::level::levelCleaveSurface(
            level,
            SurfaceRef{ 0, 0 },
            TVector::right,
            1.3,
            0.0,
            0.0);
        check(accepted,
            "Cleave subdivided reciprocal portal: cut crosses a different point on the mirror edge chain");
        check(level.sectors.size() == 2u
                && level.sectors[0].surfaces.size() == 7u
                && level.sectors[1].surfaces.size() == 7u,
            "Cleave subdivided reciprocal portal: each closed cube gains one portal fragment");
        check(containsPoint(level, 0, { 1.3, 0.0, 0.0 })
                && containsPoint(level, 0, { 1.3, 2.0, 0.0 })
                && containsPoint(level, 1, { 1.3, 0.0, 0.0 })
                && containsPoint(level, 1, { 1.3, 2.0, 0.0 }),
            "Cleave subdivided reciprocal portal: both long and subdivided contours receive the cut points");
        check(hasClosedMesh(level, 0) && hasClosedMesh(level, 1),
            "Cleave subdivided reciprocal portal: side-face propagation preserves closed shells");
        check(reciprocalPortalLinks(level, 4u),
            "Cleave subdivided reciprocal portal: both resulting portal pairs are reciprocal");

        bool sourceTextureRetained = true;
        for (const AuthoredCorner& corner : sourceCorners)
        {
            sourceTextureRetained = sourceTextureRetained
                && containsAuthoredCorner(level, 0, corner);
        }
        bool mirrorTextureRetained = true;
        for (const AuthoredCorner& corner : mirrorCorners)
        {
            mirrorTextureRetained = mirrorTextureRetained
                && containsAuthoredCorner(level, 1, corner);
        }
        check(sourceTextureRetained,
            "Cleave subdivided reciprocal portal: source portal UV/RGBA tuples remain exact");
        check(mirrorTextureRetained,
            "Cleave subdivided reciprocal portal: mirror endpoint and midpoint UV/RGBA tuples remain exact");
    }

    for ( const bool reverseSource : { false, true } )
    {
        // The source portal owns the long edge. The mirrored portal keeps the
        // same edge as two authored segments, with a bounded decimal bend at
        // its midpoint. Reverse mode starts from the segment that crosses the
        // requested plane and must still propagate to the source long edge.
        Level level = makeSubdividedPortalLevel(0.0, 1.0e-9);
        bool sourceFixtureRetained = true;
        for ( const AuthoredCorner& corner : sourceCorners )
        {
            sourceFixtureRetained = sourceFixtureRetained
                && containsAuthoredCorner(level, 0, corner);
        }
        bool roundedMirrorFixtureRetained = true;
        for ( const AuthoredCorner& corner : roundedMirrorCorners )
        {
            roundedMirrorFixtureRetained = roundedMirrorFixtureRetained
                && containsAuthoredCorner(level, 1, corner);
        }
        check(sourceFixtureRetained && roundedMirrorFixtureRetained,
            reverseSource
                ? "Cleave rounded portal edge: reverse fixture retains authored source and rounded mirror tuples"
                : "Cleave rounded portal edge: forward fixture retains authored source and rounded mirror tuples");
        const bool accepted = reverseSource
            ? sed::level::levelCleaveEdge(
                level,
                SurfaceRef{ 1, 0 },
                3,
                TVector::right,
                1.3,
                0.0,
                0.0)
            : sed::level::levelCleaveEdge(
                level,
                SurfaceRef{ 0, 0 },
                0,
                TVector::right,
                1.3,
                0.0,
                0.0);
        check(accepted,
            reverseSource
                ? "Cleave rounded portal edge: reverse source accepts the subdivided edge cut"
                : "Cleave rounded portal edge: forward source accepts the long edge cut");
        check(level.sectors[0].surfaces[0].vertices.size() == 5u
                && level.sectors[1].surfaces[0].vertices.size() == 6u,
            reverseSource
                ? "Cleave rounded portal edge: reverse cut adds one source and one native mirror corner"
                : "Cleave rounded portal edge: forward cut adds one source and one native mirror corner");
        check(containsPoint(level, 0, { 1.3, 0.0, 0.0 }, 1.0e-14)
                && containsPoint(level, 1, { 1.3, 7.0e-10, 0.0 }, 1.0e-14),
            reverseSource
                ? "Cleave rounded portal edge: reverse cut retains source and mirror native points"
                : "Cleave rounded portal edge: forward cut retains source and mirror native points");

        bool sourceTextureRetained = true;
        for ( const AuthoredCorner& corner : sourceCorners )
        {
            sourceTextureRetained = sourceTextureRetained
                && containsAuthoredCorner(level, 0, corner);
        }
        bool mirrorTextureRetained = true;
        for ( const AuthoredCorner& corner : roundedMirrorCorners )
        {
            mirrorTextureRetained = mirrorTextureRetained
                && containsAuthoredCorner(level, 1, corner);
        }
        check(sourceTextureRetained && mirrorTextureRetained,
            reverseSource
                ? "Cleave rounded portal edge: reverse cut preserves all authored portal UV/RGBA tuples"
                : "Cleave rounded portal edge: forward cut preserves all authored portal UV/RGBA tuples");
        check(containsInterpolatedCorner(
                  level,
                  0,
                  { 1.3, 0.0, 0.0 },
                  { 0.6125f, 0.2125f,
                      ColorF{ 0.422f, 0.191f, 0.331f, 0.618f } })
                && containsInterpolatedCorner(
                    level,
                    1,
                    { 1.3, 7.0e-10, 0.0 },
                    { 0.115625f, 0.734375f,
                        ColorF{ 0.478f, 0.248f, 0.674f, 0.68f } }),
            reverseSource
                ? "Cleave rounded portal edge: reverse cut interpolates each native portal UV/RGBA frame"
                : "Cleave rounded portal edge: forward cut interpolates each native portal UV/RGBA frame");
        check(hasClosedMesh(level, 0) && hasClosedMesh(level, 1)
                && reciprocalPortalLinks(level, 2u),
            reverseSource
                ? "Cleave rounded portal edge: reverse cut keeps closed reciprocal shells"
                : "Cleave rounded portal edge: forward cut keeps closed reciprocal shells");
    }

    for ( const double mirrorLeft : { 1.0e-9, 1.0e-5 } )
    {
        for ( const bool reverseSource : { false, true } )
        {
            // A translated mirror chain leaves a real uncovered interval on
            // the source long edge. Both public directions must reject it
            // without changing serialized geometry or the original links.
            Level level = makeSubdividedPortalLevel(mirrorLeft, 1.0e-9);
            const std::vector<std::byte> beforeBytes = sed::assets::saveToSedBytes(level);
            const bool accepted = reverseSource
                ? sed::level::levelCleaveEdge(
                    level,
                    SurfaceRef{ 1, 0 },
                    3,
                    TVector::right,
                    1.3,
                    0.0,
                    0.0)
                : sed::level::levelCleaveEdge(
                    level,
                    SurfaceRef{ 0, 0 },
                    0,
                    TVector::right,
                    1.3,
                    0.0,
                    0.0);
            check(!accepted,
                reverseSource
                    ? "Cleave rounded portal edge: reverse translated chain is rejected"
                    : "Cleave rounded portal edge: forward translated chain is rejected");
            check(sed::assets::saveToSedBytes(level) == beforeBytes
                    && hasClosedMesh(level, 0)
                    && hasClosedMesh(level, 1)
                    && reciprocalPortalLinks(level, 2u),
                reverseSource
                    ? "Cleave rounded portal edge: reverse translated rejection preserves bytes and links"
                    : "Cleave rounded portal edge: forward translated rejection preserves bytes and links");
        }
    }

    {
        // The source and mirror endpoints are identical, while the imported
        // mirror midpoint carries a bounded decimal-coordinate bend. The
        // bend is intentionally above the machine-epsilon onSegment proof,
        // so the test exercises source-chain compatibility rather than exact
        // coordinate welding.
        Level level = makeSubdividedPortalLevel(0.0, 1.0e-9);
        const Level before = level;
        const Vertex& roundedMidpoint = level.sectors[1].vertices[8];
        check(roundedMidpoint.y == 1.0e-9 && roundedMidpoint.y != 0.0,
            "Cleave rounded portal chain: fixed midpoint carries an independent nonzero decimal bend");
        const bool accepted = sed::level::levelCleaveSurface(
            level,
            SurfaceRef{ 0, 0 },
            TVector::right,
            1.3,
            0.0,
            0.0);
        check(accepted,
            "Cleave rounded portal chain: bounded authored midpoint bend remains cleavable");
        check(level.sectors[0].surfaces.size() == 7u
                && level.sectors[1].surfaces.size() == 7u,
            "Cleave rounded portal chain: both descendants have seven surfaces");
        check(containsPoint(level, 0, { 1.3, 0.0, 0.0 })
                && containsPoint(level, 1, { 1.3, 7.0e-10, 0.0 }),
            "Cleave rounded portal chain: native source and mirror cut points are retained");
        const bool sourceUv = containsInterpolatedCorner(
            level, 0, { 1.3, 0.0, 0.0 },
            { 0.6125f, 0.2125f,
                ColorF{ 0.422f, 0.191f, 0.331f, 0.618f } });
        const bool mirrorUv = containsInterpolatedCorner(
            level, 1, { 1.3, 7.0e-10, 0.0 },
            { 0.115625f, 0.734375f,
                ColorF{ 0.478f, 0.248f, 0.674f, 0.68f } });
        if (!sourceUv || !mirrorUv)
        {
            std::cerr << "Rounded UV mismatch source=" << sourceUv
                << " mirror=" << mirrorUv << '\n';
            for (std::size_t sectorIndex = 0; sectorIndex < 2u; ++sectorIndex)
            {
                const Sector& sector = level.sectors[sectorIndex];
                for (const Surface& surface : sector.surfaces)
                {
                    for (std::size_t corner = 0;
                        corner < surface.vertices.size()
                            && corner < surface.texVertices.size(); ++corner)
                    {
                        const std::int32_t vertexIndex = surface.vertices[corner];
                        if (vertexIndex < 0
                            || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
                        {
                            continue;
                        }
                        const Vertex& vertex = sector.vertices[
                            static_cast<std::size_t>(vertexIndex)];
                        if (vertex.x == 1.3)
                        {
                            const TexVertex& texture = surface.texVertices[corner];
                            std::cerr << " rounded sector=" << sectorIndex
                                << " point=" << vertex.x << ',' << vertex.y << ','
                                << vertex.z << " uv=" << texture.u << ','
                                << texture.v << " rgba=" << texture.color.r << ','
                                << texture.color.g << ',' << texture.color.b << ','
                                << texture.color.a << '\n';
                        }
                    }
                }
            }
        }
        check(sourceUv && mirrorUv,
            "Cleave rounded portal chain: native edge UV/RGBA interpolation remains exact");
        check(hasClosedMesh(level, 0) && hasClosedMesh(level, 1),
            "Cleave rounded portal chain: side-face propagation preserves closed shells");
        check(reciprocalPortalLinks(level, 4u),
            "Cleave rounded portal chain: resulting portal pairs remain reciprocal");
        check(!sed::assets::sedLevelsEqual(level, before),
            "Cleave rounded portal chain: accepted edit publishes a changed level");
    }

    {
        // A materially bent imported midpoint must remain outside the source
        // compatibility bound and reject atomically.
        Level level = makeSubdividedPortalLevel(0.0, 2.0e-2);
        const Level before = level;
        check(!sed::level::levelCleaveSurface(
                  level,
                  SurfaceRef{ 0, 0 },
                  TVector::right,
                  1.3,
                  0.0,
                  0.0),
            "Cleave rounded portal chain: materially bent midpoint is rejected");
        check(sed::assets::sedLevelsEqual(level, before),
            "Cleave rounded portal chain: rejected material bend leaves the level unchanged");
    }

    {
        // After the first rounded-chain cut, the generated portal fragments
        // retain their native per-sector cut points. A second independent cut
        // must still preserve the reciprocal portal
        // pairs and the closed shells.
        Level level = makeSubdividedPortalLevel(0.0, 1.0e-9);
        const std::array<std::vector<Vertex>, 2> authoredVertices{
            level.sectors[0].vertices,
            level.sectors[1].vertices};
        const auto authoredVerticesUnchanged = [&level, &authoredVertices]() noexcept
        {
            for ( std::size_t sectorIndex = 0; sectorIndex < authoredVertices.size(); ++sectorIndex )
            {
                const auto& beforeVertices = authoredVertices[sectorIndex];
                const auto& afterVertices = level.sectors[sectorIndex].vertices;
                if ( afterVertices.size() < beforeVertices.size() )
                {
                    return false;
                }
                for ( std::size_t vertexIndex = 0; vertexIndex < beforeVertices.size(); ++vertexIndex )
                {
                    const Vertex& beforeVertex = beforeVertices[vertexIndex];
                    const Vertex& afterVertex = afterVertices[vertexIndex];
                    if ( beforeVertex.x != afterVertex.x
                        || beforeVertex.y != afterVertex.y
                        || beforeVertex.z != afterVertex.z )
                    {
                        return false;
                    }
                }
            }
            return true;
        };
        const auto findExactCutPoint = [&level, &authoredVertices](const std::int32_t sectorIndex)
            -> std::optional<TVector>
        {
            if ( sectorIndex < 0
                || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
            {
                return std::nullopt;
            }
            const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
            std::optional<TVector> result;
            const std::size_t firstGenerated = authoredVertices[
                static_cast<std::size_t>(sectorIndex)].size();
            for ( std::size_t vertexIndex = firstGenerated;
                vertexIndex < sector.vertices.size(); ++vertexIndex )
            {
                const Vertex& vertex = sector.vertices[vertexIndex];
                const double expectedY = sectorIndex == 0u ? 0.0 : 7.0e-10;
                if ( vertex.x == 1.3
                    && close(vertex.y, expectedY, 1.0e-14)
                    && vertex.z == 0.0 )
                {
                    if ( result )
                    {
                        return std::nullopt;
                    }
                    result = TVector{ vertex.x, vertex.y, vertex.z };
                }
            }
            return result;
        };
        check(sed::level::levelCleaveSurface(
                  level,
                  SurfaceRef{ 0, 0 },
                  TVector::right,
                  1.3,
                  0.0,
                  0.0),
            "Cleave rounded portal repeat: first cut succeeds before the second plane");
        const std::optional<TVector> firstCutPoint = findExactCutPoint(0);
        const std::optional<TVector> mirrorCutPoint = findExactCutPoint(1);
        check(level.sectors[0].surfaces.size() == 7u
                && level.sectors[1].surfaces.size() == 7u
                && containsPoint(level, 1, { 1.3, 7.0e-10, 0.0 })
                && firstCutPoint && mirrorCutPoint
                && firstCutPoint->x == 1.3
                && close(firstCutPoint->y, 0.0, 1.0e-14)
                && firstCutPoint->z == 0.0
                && mirrorCutPoint->x == 1.3
                && close(mirrorCutPoint->y, 7.0e-10, 1.0e-14)
                && mirrorCutPoint->z == 0.0
                && authoredVerticesUnchanged(),
            "Cleave rounded portal repeat: first native paired cut points are retained");

        const auto crossesPlaneY = [&level](
            const SurfaceRef ref, const double y) noexcept
        {
            if ( ref.sector < 0 || ref.surface < 0
                || static_cast<std::size_t>(ref.sector) >= level.sectors.size()
                || static_cast<std::size_t>(ref.surface)
                    >= level.sectors[static_cast<std::size_t>(ref.sector)].surfaces.size() )
            {
                return false;
            }
            const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
            const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
            bool below = false;
            bool above = false;
            for ( const std::int32_t vertexIndex : surface.vertices)
            {
                if ( vertexIndex < 0
                    || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size() )
                {
                    return false;
                }
                const double distance = sector.vertices[
                    static_cast<std::size_t>(vertexIndex)].y - y;
                below = below || distance < 0.0;
                above = above || distance > 0.0;
            }
            return below && above;
        };
        check(crossesPlaneY({ 0, 0 }, 1.0) && crossesPlaneY({ 0, 6 }, 1.0),
            "Cleave rounded portal repeat: both first-cut fragments straddle the second plane");
        const TVector secondPlaneNormal{ 0.0, 1.0, 0.0 };
        const bool firstFragment = sed::level::levelCleaveSurface(
            level,
            SurfaceRef{ 0, 0 },
            secondPlaneNormal,
            0.0,
            1.0,
            0.0);
        const bool secondFragment = sed::level::levelCleaveSurface(
            level,
            SurfaceRef{ 0, 6 },
            secondPlaneNormal,
            0.0,
            1.0,
            0.0);
        check(firstFragment && secondFragment,
            "Cleave rounded portal repeat: both first-cut fragments accept a second plane");
        check(level.sectors[0].surfaces.size() == 9u
                && level.sectors[1].surfaces.size() == 9u
                && hasClosedMesh(level, 0)
                && hasClosedMesh(level, 1)
                && authoredVerticesUnchanged()
                && reciprocalPortalLinks(level, 8u),
            "Cleave rounded portal repeat: second cut keeps closed reciprocal geometry");
    }

    {
        // Start from the mirrored portal owner to verify native intersection handling does
        // not depend on which side initiates the first cut.
        Level level = makeSubdividedPortalLevel(0.0, 1.0e-9);
        const Level before = level;
        const std::array<std::size_t, 2> beforeVertexCounts{
            level.sectors[0].vertices.size(),
            level.sectors[1].vertices.size() };
        const bool accepted = sed::level::levelCleaveSurface(
            level,
            SurfaceRef{ 1, 0 },
            TVector::right,
            1.3,
            0.0,
            0.0);
        check(accepted,
            "Cleave rounded portal reverse repeat: mirror source accepts the first cut");

        const auto authoredVerticesUnchanged = [&before, &level]() noexcept
        {
            if ( before.sectors.size() != level.sectors.size() )
            {
                return false;
            }
            for ( std::size_t sectorIndex = 0;
                sectorIndex < before.sectors.size();
                ++sectorIndex )
            {
                const auto& beforeVertices = before.sectors[sectorIndex].vertices;
                const auto& afterVertices = level.sectors[sectorIndex].vertices;
                if ( afterVertices.size() < beforeVertices.size() )
                {
                    return false;
                }
                for ( std::size_t vertexIndex = 0;
                    vertexIndex < beforeVertices.size();
                    ++vertexIndex )
                {
                    const Vertex& expected = beforeVertices[vertexIndex];
                    const Vertex& actual = afterVertices[vertexIndex];
                    if ( expected.x != actual.x
                        || expected.y != actual.y
                        || expected.z != actual.z )
                    {
                        return false;
                    }
                }
            }
            return true;
        };
        check(level.sectors[0].surfaces.size() == 7u
                && level.sectors[1].surfaces.size() == 7u
                && hasClosedMesh(level, 0)
                && hasClosedMesh(level, 1)
                && reciprocalPortalLinks(level, 4u),
            "Cleave rounded portal reverse repeat: first cut preserves both closed reciprocal shells");
        check(authoredVerticesUnchanged(),
            "Cleave rounded portal reverse repeat: every originally authored vertex remains unchanged");

        const auto findCutPoint = [&level, &beforeVertexCounts](const std::int32_t sectorIndex)
            -> std::optional<TVector>
        {
            if ( sectorIndex < 0
                || static_cast<std::size_t>(sectorIndex) >= level.sectors.size() )
            {
                return std::nullopt;
            }
            const std::size_t sectorOffset = static_cast<std::size_t>(sectorIndex);
            const Sector& sector = level.sectors[sectorOffset];
            std::optional<TVector> result;
            for ( std::size_t vertexIndex = beforeVertexCounts[sectorOffset];
                vertexIndex < sector.vertices.size();
                ++vertexIndex )
            {
                const Vertex& vertex = sector.vertices[vertexIndex];
                const double expectedY = sectorOffset == 0u ? 0.0 : 7.0e-10;
                if ( vertex.x == 1.3
                    && close(vertex.y, expectedY, 1.0e-14)
                    && vertex.z == 0.0 )
                {
                    if ( result )
                    {
                        return std::nullopt;
                    }
                    result = TVector{ vertex.x, vertex.y, vertex.z };
                }
            }
            return result;
        };
        const std::optional<TVector> firstCutPoint = findCutPoint(0);
        const std::optional<TVector> mirrorCutPoint = findCutPoint(1);
        check(firstCutPoint && mirrorCutPoint
            && firstCutPoint->x == 1.3
            && close(firstCutPoint->y, 0.0, 1.0e-14)
            && firstCutPoint->z == 0.0
            && mirrorCutPoint->x == 1.3
            && close(mirrorCutPoint->y, 7.0e-10, 1.0e-14)
            && mirrorCutPoint->z == 0.0
            && firstCutPoint->x == mirrorCutPoint->x
            && firstCutPoint->z == mirrorCutPoint->z,
            "Cleave rounded portal reverse repeat: paired native first-cut coordinates retain their source-edge intersections");

        const auto crossesPlaneY = [&level](const SurfaceRef ref, const double y) noexcept
        {
            if ( ref.sector < 0 || ref.surface < 0
                || static_cast<std::size_t>(ref.sector) >= level.sectors.size()
                || static_cast<std::size_t>(ref.surface)
                    >= level.sectors[static_cast<std::size_t>(ref.sector)].surfaces.size() )
            {
                return false;
            }
            const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
            const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
            bool below = false;
            bool above = false;
            for ( const std::int32_t vertexIndex : surface.vertices )
            {
                if ( vertexIndex < 0
                    || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size() )
                {
                    return false;
                }
                const double distance = sector.vertices[
                    static_cast<std::size_t>(vertexIndex)].y - y;
                below = below || distance < 0.0;
                above = above || distance > 0.0;
            }
            return below && above;
        };
        check(crossesPlaneY({ 1, 0 }, 1.0)
                && crossesPlaneY({ 1, 6 }, 1.0),
            "Cleave rounded portal reverse repeat: both mirror fragments straddle the second plane");

        const TVector secondPlaneNormal{ 0.0, 1.0, 0.0 };
        const bool firstFragment = sed::level::levelCleaveSurface(
            level,
            SurfaceRef{ 1, 0 },
            secondPlaneNormal,
            0.0,
            1.0,
            0.0);
        const bool secondFragment = sed::level::levelCleaveSurface(
            level,
            SurfaceRef{ 1, 6 },
            secondPlaneNormal,
            0.0,
            1.0,
            0.0);
        check(firstFragment && secondFragment,
            "Cleave rounded portal reverse repeat: both mirror fragments accept the second plane");
        check(level.sectors[0].surfaces.size() == 9u
                && level.sectors[1].surfaces.size() == 9u
                && authoredVerticesUnchanged()
                && hasClosedMesh(level, 0)
                && hasClosedMesh(level, 1)
                && reciprocalPortalLinks(level, 8u),
            "Cleave rounded portal reverse repeat: second cut preserves closed reciprocal geometry");
    }
    for ( const bool reverseSource : { false, true } )
    {
        Level level = makeThreeSectorRoundedPortalLevel();
        check(level.sectors.size() == 3u,
            "Cleave three-sector rounded portal: fixture has all three sectors");
        if ( level.sectors.size() != 3u )
        {
            continue;
        }
        const std::array<std::vector<Vertex>, 3> authored{
            level.sectors[0].vertices, level.sectors[1].vertices,
            level.sectors[2].vertices };
        check(level.sectors.size() == 3u
                && hasClosedMesh(level, 0) && hasClosedMesh(level, 1)
                && hasClosedMesh(level, 2),
            "Cleave three-sector rounded portal: preflight shells are closed");
        check(reciprocalPortalLinks(level, 6u),
            "Cleave three-sector rounded portal: initial cross-sector links are reciprocal");
        check(sed::level::levelCleaveSurface(
                  level,
                  reverseSource ? SurfaceRef{ 1, 0 } : SurfaceRef{ 0, 0 },
                  TVector::right, 1.3, 0.0, 0.0),
            "Cleave three-sector rounded portal: primary cut succeeds from both directions");
        bool allNativeIntersections = true;
        for ( std::size_t sectorIndex = 0; sectorIndex < 3u; ++sectorIndex )
        {
            bool found = false;
            const double expectedY = sectorIndex == 1u ? 7.0e-10 : 0.0;
            for ( const Vertex& vertex : level.sectors[sectorIndex].vertices )
            {
                found = found || (vertex.x == 1.3
                    && close(vertex.y, expectedY, 1.0e-14)
                    && vertex.z == 0.0);
            }
            allNativeIntersections = allNativeIntersections && found;
        }
        bool authoredUnchanged = true;
        for ( std::size_t sectorIndex = 0; sectorIndex < authored.size(); ++sectorIndex )
        {
            for ( std::size_t vertexIndex = 0; vertexIndex < authored[sectorIndex].size(); ++vertexIndex )
            {
                const Vertex& before = authored[sectorIndex][vertexIndex];
                const Vertex& after = level.sectors[sectorIndex].vertices[vertexIndex];
                authoredUnchanged = authoredUnchanged
                    && before.x == after.x && before.y == after.y && before.z == after.z;
            }
        }
        check(allNativeIntersections && authoredUnchanged
                && hasClosedMesh(level, 0) && hasClosedMesh(level, 1)
                && hasClosedMesh(level, 2)
                && reciprocalPortalLinks(level, 8u),
            "Cleave three-sector rounded portal: native intersections propagate and authored vertices remain unchanged");
    }

    {
        // The mirror contour starts at x=.25, leaving a certified gap in the
        // requested source edge. The operation must reject before mutation.
        Level level = makeSubdividedPortalLevel(0.25);
        const Level before = level;
        check(hasClosedMesh(level, 0) && hasClosedMesh(level, 1),
            "Cleave subdivided reciprocal portal gap: malformed correspondence still has closed input shells");
        check(!sed::level::levelCleaveSurface(
                  level,
                  SurfaceRef{ 0, 0 },
                  TVector::right,
                  1.3,
                  0.0,
                  0.0),
            "Cleave subdivided reciprocal portal gap: an uncovered source interval is rejected");
        check(sed::assets::sedLevelsEqual(level, before),
            "Cleave subdivided reciprocal portal gap: rejected chain leaves the complete level unchanged");
        check(reciprocalPortalLinks(level, 2u),
            "Cleave subdivided reciprocal portal gap: rollback preserves the original portal link");
    }

    {
        // The two primary portal contours differ by 1e-9 at their left
        // endpoints. Each sector also has two unadjoined incident faces that
        // share only one indexed portal anchor. Their extended runs contain
        // coordinate duplicates of the primary mismatch vertices, with a
        // measured 2e-9 redundant bend. A contour proof must retain the
        // immutable vertex IDs while collecting those runs; borrowing one of
        // these disconnected faces by coordinates would falsely validate the
        // translated portal.
        Level level = makeDuplicateIdIncidentPortalLevel();
        check(level.sectors.size() == 2u
                && level.sectors[0].surfaces.size() == 8u
                && level.sectors[1].surfaces.size() == 8u,
            "Cleave duplicate-ID incident contour: fixture appends four unadjoined topology faces");
        if ( level.sectors.size() != 2u
            || level.sectors[0].surfaces.size() != 8u
            || level.sectors[1].surfaces.size() != 8u )
        {
            return;
        }

        const auto& source = level.sectors[0];
        const auto& mirror = level.sectors[1];
        const Surface& sourcePortal = source.surfaces[0];
        const Surface& mirrorPortal = mirror.surfaces[0];
        const Surface& sourceBottom = source.surfaces[6];
        const Surface& sourceTop = source.surfaces[7];
        const Surface& mirrorBottom = mirror.surfaces[6];
        const Surface& mirrorTop = mirror.surfaces[7];
        const auto pointEquals = [](const Vertex& first, const Vertex& second) noexcept
        {
            return first.x == second.x && first.y == second.y && first.z == second.z;
        };
        const bool duplicateCoordinates = sourceBottom.vertices.size() == 7u
            && sourceTop.vertices.size() == 7u
            && mirrorBottom.vertices.size() == 7u
            && mirrorTop.vertices.size() == 7u
            && sourceBottom.vertices[1] != sourcePortal.vertices[0]
            && sourceTop.vertices[1] != sourcePortal.vertices[3]
            && mirrorBottom.vertices[1] != mirrorPortal.vertices[0]
            && mirrorTop.vertices[1] != mirrorPortal.vertices[1]
            && pointEquals(
                source.vertices[static_cast<std::size_t>(sourceBottom.vertices[1])],
                source.vertices[static_cast<std::size_t>(sourcePortal.vertices[0])])
            && pointEquals(
                source.vertices[static_cast<std::size_t>(sourceTop.vertices[1])],
                source.vertices[static_cast<std::size_t>(sourcePortal.vertices[3])])
            && pointEquals(
                mirror.vertices[static_cast<std::size_t>(mirrorBottom.vertices[1])],
                mirror.vertices[static_cast<std::size_t>(mirrorPortal.vertices[0])])
            && pointEquals(
                mirror.vertices[static_cast<std::size_t>(mirrorTop.vertices[1])],
                mirror.vertices[static_cast<std::size_t>(mirrorPortal.vertices[1])]);
        const bool indexedAnchorShared = sourceBottom.vertices[3] == sourcePortal.vertices[1]
            && sourceTop.vertices[3] == sourcePortal.vertices[2]
            && mirrorBottom.vertices[3] == mirrorPortal.vertices[3]
            && mirrorTop.vertices[3] == mirrorPortal.vertices[2];
        const auto onWallPlane = [](
            const Sector& sector,
            const Surface& face,
            const double row) noexcept
        {
            return std::all_of(
                face.vertices.begin(),
                face.vertices.end(),
                [&sector, row](const std::int32_t vertexIndex) noexcept
                {
                    const Vertex& vertex = sector.vertices[
                        static_cast<std::size_t>(vertexIndex)];
                    return std::abs(vertex.z - (vertex.y - row)) <= 1.0e-15;
                });
        };
        const bool boundedBendsPresent = source.vertices[
                static_cast<std::size_t>(sourceBottom.vertices[2])].y == 2.0e-9
            && source.vertices[static_cast<std::size_t>(sourceBottom.vertices[2])].z == 2.0e-9
            && source.vertices[static_cast<std::size_t>(sourceTop.vertices[2])].y == 2.0 + 2.0e-9
            && source.vertices[static_cast<std::size_t>(sourceTop.vertices[2])].z == 2.0e-9
            && mirror.vertices[static_cast<std::size_t>(mirrorBottom.vertices[2])].y == 2.0e-9
            && mirror.vertices[static_cast<std::size_t>(mirrorTop.vertices[2])].y == 2.0 + 2.0e-9;
        const bool translatedPrimary = sourcePortal.vertices.size() == 4u
            && mirrorPortal.vertices.size() == 5u
            && source.vertices[static_cast<std::size_t>(sourcePortal.vertices[0])].x == 0.0
            && mirror.vertices[static_cast<std::size_t>(mirrorPortal.vertices[0])].x == 1.0e-9;
        const bool planarWalls = onWallPlane(source, sourceBottom, 0.0)
            && onWallPlane(source, sourceTop, 2.0)
            && onWallPlane(mirror, mirrorBottom, 0.0)
            && onWallPlane(mirror, mirrorTop, 2.0)
            && source.vertices[static_cast<std::size_t>(sourceBottom.vertices[5])].z == -1.0
            && source.vertices[static_cast<std::size_t>(sourceBottom.vertices[6])].z == -1.0
            && source.vertices[static_cast<std::size_t>(sourceTop.vertices[5])].z == -1.0
            && source.vertices[static_cast<std::size_t>(sourceTop.vertices[6])].z == -1.0
            && mirror.vertices[static_cast<std::size_t>(mirrorBottom.vertices[5])].z == -1.0
            && mirror.vertices[static_cast<std::size_t>(mirrorBottom.vertices[6])].z == -1.0
            && mirror.vertices[static_cast<std::size_t>(mirrorTop.vertices[5])].z == -1.0
            && mirror.vertices[static_cast<std::size_t>(mirrorTop.vertices[6])].z == -1.0;
        const bool topologyOnly = !sourceBottom.adjoin && !sourceTop.adjoin
            && !mirrorBottom.adjoin && !mirrorTop.adjoin
            && std::isfinite(sourceBottom.normal.x)
            && std::isfinite(sourceBottom.normal.y)
            && std::isfinite(sourceBottom.normal.z)
            && std::isfinite(sourceTop.normal.x)
            && std::isfinite(sourceTop.normal.y)
            && std::isfinite(sourceTop.normal.z)
            && std::isfinite(mirrorBottom.normal.x)
            && std::isfinite(mirrorBottom.normal.y)
            && std::isfinite(mirrorBottom.normal.z)
            && std::isfinite(mirrorTop.normal.x)
            && std::isfinite(mirrorTop.normal.y)
            && std::isfinite(mirrorTop.normal.z);
        check(duplicateCoordinates && indexedAnchorShared && boundedBendsPresent
                && translatedPrimary && planarWalls && topologyOnly,
            "Cleave duplicate-ID incident contour: duplicate mismatch IDs, indexed anchors, and planar-wall inputs are explicit");

        for ( const bool reverseSource : { false, true } )
        {
            Level candidate = level;
            const std::vector<std::byte> beforeBytes = sed::assets::saveToSedBytes(candidate);
            const bool accepted = sed::level::levelCleaveSurface(
                candidate,
                reverseSource ? SurfaceRef{ 1, 0 } : SurfaceRef{ 0, 0 },
                TVector::right,
                1.3,
                0.0,
                0.0);
            check(!accepted,
                reverseSource
                    ? "Cleave duplicate-ID incident contour: reverse translated portal is rejected"
                    : "Cleave duplicate-ID incident contour: forward translated portal is rejected");
            check(sed::assets::saveToSedBytes(candidate) == beforeBytes
                    && reciprocalPortalLinks(candidate, 2u),
                reverseSource
                    ? "Cleave duplicate-ID incident contour: reverse rejection rolls back bytes and links"
                    : "Cleave duplicate-ID incident contour: forward rejection rolls back bytes and links");
        }
    }

    for ( const bool reverseSource : { false, true } )
    {
        Level level = makeMultiMidpointPortalLevel();
        const std::array<AuthoredCorner, 2> extraCorners{
            AuthoredCorner{
                0.5, 1.0e-9, 0.0,
                { 0.03125f, 0.96875f,
                    ColorF{ 0.37f, 0.11f, 0.89f, 0.71f } } },
            AuthoredCorner{
                1.5, 1.0e-9, 0.0,
                { 0.111f, 0.222f,
                    ColorF{ 0.17f, 0.29f, 0.43f, 0.71f } } },
        };
        const std::array<std::size_t, 2> beforeVertexCounts{
            level.sectors[0].vertices.size(),
            level.sectors[1].vertices.size() };
        const std::array<std::vector<Vertex>, 2> originalVertices{
            level.sectors[0].vertices,
            level.sectors[1].vertices };
        const auto authoredVerticesUnchanged = [&level, &originalVertices]() noexcept
        {
            for ( std::size_t sectorIndex = 0;
                sectorIndex < originalVertices.size(); ++sectorIndex )
            {
                const auto& beforeVertices = originalVertices[sectorIndex];
                const auto& afterVertices = level.sectors[sectorIndex].vertices;
                if ( afterVertices.size() < beforeVertices.size() )
                {
                    return false;
                }
                for ( std::size_t vertexIndex = 0;
                    vertexIndex < beforeVertices.size(); ++vertexIndex )
                {
                    const Vertex& before = beforeVertices[vertexIndex];
                    const Vertex& after = afterVertices[vertexIndex];
                    if ( before.x != after.x || before.y != after.y
                        || before.z != after.z)
                    {
                        return false;
                    }
                }
            }
            return true;
        };
        check(hasClosedMesh(level, 0)
                && hasClosedMesh(level, 1)
                && reciprocalPortalLinks(level, 2u)
                && level.sectors[1].vertices[8].x < level.sectors[1].vertices[9].x
                && level.sectors[1].vertices[8].y == level.sectors[1].vertices[9].y,
            "Cleave multi-midpoint portal: preflight shells and monotonic imported portal points are valid");
        const bool accepted = sed::level::levelCleaveSurface(
            level,
            reverseSource ? SurfaceRef{ 1, 0 } : SurfaceRef{ 0, 0 },
            TVector::right,
            1.0,
            0.0,
            0.0);
        check(accepted
                && level.sectors[0].surfaces.size() == 7u
                && level.sectors[1].surfaces.size() == 7u
                && hasClosedMesh(level, 0)
                && hasClosedMesh(level, 1)
                && reciprocalPortalLinks(level, 4u),
            "Cleave multi-midpoint portal: first cut keeps equal closed reciprocal shells from both source directions");
        bool sourceNativePoint = false;
        bool mirrorNativePoint = false;
        for ( std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex )
        {
            const Sector& sector = level.sectors[sectorIndex];
            const std::size_t firstGenerated = beforeVertexCounts[sectorIndex];
            for ( std::size_t vertexIndex = firstGenerated;
                vertexIndex < sector.vertices.size(); ++vertexIndex )
            {
                const Vertex& vertex = sector.vertices[vertexIndex];
                if ( vertex.x == 1.0 && vertex.z == 0.0
                    && ((sectorIndex == 0u && vertex.y == 0.0)
                        || (sectorIndex == 1u && close(vertex.y, 1.0e-9, 1.0e-14))) )
                {
                    sourceNativePoint = sourceNativePoint || sectorIndex == 0u;
                    mirrorNativePoint = mirrorNativePoint || sectorIndex == 1u;
                }
            }
        }
        check(sourceNativePoint && mirrorNativePoint
                && containsPoint(level, 1, { 0.5, 1.0e-9, 0.0 })
                && containsPoint(level, 1, { 1.5, 1.0e-9, 0.0 })
                && containsAuthoredCorner(level, 1, extraCorners[0])
                && containsAuthoredCorner(level, 1, extraCorners[1]),
            "Cleave multi-midpoint portal: native cut intersections and both authored interior points remain exact");

        const auto straddlesSecondPlane = [&level](const std::int32_t sectorIndex)
        {
            if ( sectorIndex < 0
                || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
            {
                return false;
            }
            for ( const Surface& surface : level.sectors[
                     static_cast<std::size_t>(sectorIndex)].surfaces)
            {
                bool below = false;
                bool above = false;
                for ( const std::int32_t vertexIndex : surface.vertices )
                {
                    const double y = level.sectors[static_cast<std::size_t>(sectorIndex)]
                        .vertices[static_cast<std::size_t>(vertexIndex)].y;
                    below = below || y < 1.0;
                    above = above || y > 1.0;
                }
                if ( below && above )
                {
                    return true;
                }
            }
            return false;
        };
        check(straddlesSecondPlane(0) && straddlesSecondPlane(1),
            "Cleave multi-midpoint portal: both descendants independently straddle the second plane");
        const std::vector<std::byte> serializedAfterFirst =
            sed::assets::saveToSedBytes(level);
        Level decodedAfterFirst = sed::assets::loadFromSedBytes(serializedAfterFirst);
        check(sed::assets::saveToSedBytes(decodedAfterFirst) == serializedAfterFirst,
            "Cleave multi-midpoint portal: SED encode/decode preserves the first-cut state before repeat");
        level = std::move(decodedAfterFirst);

        const bool firstFragment = sed::level::levelCleaveSurface(
            level,
            reverseSource ? SurfaceRef{ 1, 0 } : SurfaceRef{ 0, 0 },
            TVector{ 0.0, 1.0, 0.0 },
            0.0,
            1.0,
            0.0);
        const bool secondFragment = sed::level::levelCleaveSurface(
            level,
            reverseSource ? SurfaceRef{ 1, 6 } : SurfaceRef{ 0, 6 },
            TVector{ 0.0, 1.0, 0.0 },
            0.0,
            1.0,
            0.0);
        check(firstFragment && secondFragment
                && hasClosedMesh(level, 0)
                && hasClosedMesh(level, 1)
                && reciprocalPortalLinks(level, 8u)
                && level.sectors[0].surfaces.size() == 9u
                && level.sectors[1].surfaces.size() == 9u
                && authoredVerticesUnchanged()
                && containsAuthoredCorner(level, 1, extraCorners[0])
                && containsAuthoredCorner(level, 1, extraCorners[1]),
            "Cleave multi-midpoint portal: second cut closes both descendants and preserves both authored points");
    }

    {
        // A complete mirror chain translated by a small amount must not be
        // admitted by the rounded-chain residual. Endpoint anchors remain
        // authored identities, so this is a rollback control rather than a
        // tolerance-based portal weld.
        Level level = makeSubdividedPortalLevel(1.0e-5, 1.0e-9);
        const Level before = level;
        check(hasClosedMesh(level, 0) && hasClosedMesh(level, 1)
                && reciprocalPortalLinks(level, 2u),
            "Cleave shifted portal chain: translated input remains closed and reciprocal");
        check(!sed::level::levelCleaveSurface(
                  level,
                  SurfaceRef{ 0, 0 },
                  TVector::right,
                  1.3,
                  0.0,
                  0.0),
            "Cleave shifted portal chain: endpoint translation is rejected");
        check(sed::assets::sedLevelsEqual(level, before)
                && reciprocalPortalLinks(level, 2u),
            "Cleave shifted portal chain: rejected translation rolls back atomically");
    }

    {
        // Even a translation below the rounded-coordinate eligibility bound
        // must reject because endpoint identity is part of the portal contract.
        Level level = makeSubdividedPortalLevel(1.0e-9, 0.0);
        const Level before = level;
        check(!sed::level::levelCleaveSurface(
                  level,
                  SurfaceRef{ 0, 0 },
                  TVector::right,
                  1.3,
                  0.0,
                  0.0),
            "Cleave tiny shifted portal chain: sub-threshold endpoint translation is rejected");
        check(sed::assets::sedLevelsEqual(level, before)
                && reciprocalPortalLinks(level, 2u),
            "Cleave tiny shifted portal chain: rejected sub-threshold translation rolls back atomically");
    }

    for ( const bool reverseSource : { false, true } )
    {
        Level level = makeSubdividedPortalLevel(0.0, 1.0e-9);
        check(level.sectors.size() == 2u,
            "Cleave rounded multi-plane: fixture has both portal sectors");
        if ( level.sectors.size() != 2u )
        {
            continue;
        }
        const Level authored = level;
        const auto authoredCornersUnchanged = [&authored, &level]() noexcept
        {
            for ( std::size_t sectorIndex = 0;
                sectorIndex < authored.sectors.size(); ++sectorIndex )
            {
                const Sector& beforeSector = authored.sectors[sectorIndex];
                const Sector& afterSector = level.sectors[sectorIndex];
                for ( const Surface& beforeSurface : beforeSector.surfaces )
                {
                    for ( std::size_t corner = 0;
                        corner < beforeSurface.vertices.size(); ++corner )
                    {
                        const std::int32_t vertexIndex = beforeSurface.vertices[corner];
                        if ( vertexIndex < 0
                            || static_cast<std::size_t>(vertexIndex)
                                >= beforeSector.vertices.size()
                            || corner >= beforeSurface.texVertices.size() )
                        {
                            return false;
                        }
                        const Vertex& expectedPoint = beforeSector.vertices[
                            static_cast<std::size_t>(vertexIndex)];
                        const TexVertex& expectedTexture = beforeSurface.texVertices[corner];
                        bool found = false;
                        for ( const Surface& afterSurface : afterSector.surfaces )
                        {
                            for ( std::size_t afterCorner = 0;
                                afterCorner < afterSurface.vertices.size()
                                    && afterCorner < afterSurface.texVertices.size();
                                ++afterCorner )
                            {
                                const std::int32_t afterIndex =
                                    afterSurface.vertices[afterCorner];
                                if ( afterIndex < 0
                                    || static_cast<std::size_t>(afterIndex)
                                        >= afterSector.vertices.size() )
                                {
                                    continue;
                                }
                                const Vertex& actualPoint = afterSector.vertices[
                                    static_cast<std::size_t>(afterIndex)];
                                found = found
                                    || ( expectedPoint.x == actualPoint.x
                                        && expectedPoint.y == actualPoint.y
                                        && expectedPoint.z == actualPoint.z
                                        && sameTextureVertex(
                                            expectedTexture,
                                            afterSurface.texVertices[afterCorner]) );
                            }
                        }
                        if ( !found )
                        {
                            return false;
                        }
                    }
                }
            }
            return true;
        };
        const auto straddles = [&level](
            const SurfaceRef ref,
            const TVector& normal,
            const TVector& point) noexcept
        {
            if ( ref.sector < 0 || ref.surface < 0
                || static_cast<std::size_t>(ref.sector) >= level.sectors.size()
                || static_cast<std::size_t>(ref.surface)
                    >= level.sectors[static_cast<std::size_t>(ref.sector)].surfaces.size() )
            {
                return false;
            }
            const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
            const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
            bool negative = false;
            bool positive = false;
            for ( const std::int32_t vertexIndex : surface.vertices )
            {
                if ( vertexIndex < 0
                    || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size() )
                {
                    return false;
                }
                const Vertex& vertex = sector.vertices[
                    static_cast<std::size_t>(vertexIndex)];
                const double distance = normal.dot(
                    vertex.x - point.x, vertex.y - point.y, vertex.z - point.z);
                negative = negative || distance < 0.0;
                positive = positive || distance > 0.0;
            }
            return negative && positive;
        };
        bool rejectedCutRollback = true;
        const auto cutAllPortalDescendants = [
            &level, &straddles, &rejectedCutRollback](
            const std::int32_t preferredSector,
            const TVector& normal,
            const TVector& point)
        {
            for ( std::size_t iteration = 0; iteration < 32u; ++iteration )
            {
                std::optional<SurfaceRef> candidate;
                if ( preferredSector < 0
                    || static_cast<std::size_t>(preferredSector) >= level.sectors.size() )
                {
                    return false;
                }
                const Sector& sector = level.sectors[
                    static_cast<std::size_t>(preferredSector)];
                for ( std::size_t surfaceIndex = 0;
                    surfaceIndex < sector.surfaces.size(); ++surfaceIndex )
                {
                    const Surface& surface = sector.surfaces[surfaceIndex];
                    if ( surface.adjoin
                        && straddles(
                            { preferredSector, static_cast<std::int32_t>(surfaceIndex) },
                            normal, point) )
                    {
                        candidate = SurfaceRef{
                            preferredSector, static_cast<std::int32_t>(surfaceIndex) };
                        break;
                    }
                }
                if ( !candidate )
                {
                    return true;
                }
                const std::vector<std::byte> beforeRejected =
                    sed::assets::saveToSedBytes(level);
                const bool accepted = sed::level::levelCleaveSurface(
                    level, *candidate, normal, point.x, point.y, point.z);
                if ( !accepted )
                {
                    const bool rolledBack =
                        sed::assets::saveToSedBytes(level) == beforeRejected;
                    std::cerr << "rounded multi-plane cut failure sector="
                        << candidate->sector << " surface=" << candidate->surface
                        << " rolledBack=" << rolledBack << '\n';
                    rejectedCutRollback = rejectedCutRollback && rolledBack;
                    return false;
                }
            }
            return false;
        };
        const std::array<TVector, 4> normals{
            TVector::right, TVector{ 0.0, 1.0, 0.0 },
            TVector::right, TVector{ 0.0, 1.0, 0.0 } };
        const std::array<TVector, 4> points{
            TVector{ 1.3, 0.0, 0.0 }, TVector{ 0.0, 1.0, 0.0 },
            TVector{ 0.65, 0.0, 0.0 }, TVector{ 0.0, 0.5, 0.0 } };
        bool planesAccepted = true;
        const std::array<std::size_t, 4> expectedSurfaceCounts{ 7u, 9u, 11u, 14u };
        for ( std::size_t planeIndex = 0; planeIndex < normals.size(); ++planeIndex )
        {
            if ( planesAccepted )
            {
                planesAccepted = cutAllPortalDescendants(
                    reverseSource ? 1 : 0, normals[planeIndex], points[planeIndex]);
                if ( !planesAccepted )
                {
                    std::cerr << "rounded multi-plane failed plane=" << planeIndex
                        << " direction=" << (reverseSource ? "reverse" : "forward")
                        << '\n';
                }
            }
            check(!planesAccepted
                    || (level.sectors[0].surfaces.size() == expectedSurfaceCounts[planeIndex]
                        && level.sectors[1].surfaces.size() == expectedSurfaceCounts[planeIndex]),
                "Cleave rounded multi-plane: expected portal fragment count after each plane");
            if ( planeIndex == 1u && planesAccepted )
            {
                const std::vector<std::byte> serialized =
                    sed::assets::saveToSedBytes(level);
                level = sed::assets::loadFromSedBytes(serialized);
                check(sed::assets::saveToSedBytes(level) == serialized,
                    "Cleave rounded multi-plane: SED round-trip preserves the two-plane state");
            }
        }
        check(planesAccepted,
            "Cleave rounded multi-plane: all four planes accept from each source direction");
        check(level.sectors[0].surfaces.size() == 14u
                && level.sectors[1].surfaces.size() == 14u,
            "Cleave rounded multi-plane: final surface counts are fourteen per sector");
        check(reciprocalPortalLinks(level, 18u),
            "Cleave rounded multi-plane: final directed portal links are reciprocal");
        check(hasClosedMesh(level, 0) && hasClosedMesh(level, 1),
            "Cleave rounded multi-plane: final shells are directed closed meshes");
        check(authoredCornersUnchanged(),
            "Cleave rounded multi-plane: authored vertices and texture tuples remain unchanged");
        check(rejectedCutRollback,
            "Cleave rounded multi-plane: every rejected descendant cut rolls back byte-for-byte");
        const std::vector<std::byte> beforeRejectedCut = sed::assets::saveToSedBytes(level);
        check(!sed::level::levelCleaveSurface(
                  level,
                  reverseSource ? SurfaceRef{ 1, 0 } : SurfaceRef{ 0, 0 },
                  TVector::right,
                  10.0,
                  0.0,
                  0.0)
                && sed::assets::saveToSedBytes(level) == beforeRejectedCut,
            "Cleave rounded multi-plane: non-straddling fifth plane rejects atomically");
    }
}

} // namespace sed::tests
