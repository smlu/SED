#include "render/transform/geometry_transform_preview_utils.hpp"

#include "sed/level/geometry.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace sed::render
{
namespace
{

using VertexMask = std::vector<std::vector<bool>>;

bool validSector(const Level& level, const std::int32_t sector) noexcept
{
    return sector >= 0
        && static_cast<std::size_t>(sector) < level.sectors.size();
}

bool validSurface(const Level& level, const SurfaceRef ref) noexcept
{
    return validSector(level, ref.sector)
        && ref.surface >= 0
        && static_cast<std::size_t>(ref.surface)
            < level.sectors[static_cast<std::size_t>(ref.sector)].surfaces.size();
}

VertexMask makeVertexMask(const Level& level)
{
    VertexMask mask{};
    mask.reserve(level.sectors.size());
    for (const Sector& sector : level.sectors)
    {
        mask.emplace_back(sector.vertices.size(), false);
    }
    return mask;
}

void markSurfaceVertices(
    const Level& level,
    const SurfaceRef ref,
    VertexMask& moved) noexcept
{
    if (!validSurface(level, ref))
    {
        return;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
    const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
    std::vector<bool>& sectorMask = moved[static_cast<std::size_t>(ref.sector)];
    for (const std::int32_t vertex : surface.vertices)
    {
        if (vertex >= 0
            && static_cast<std::size_t>(vertex) < sectorMask.size())
        {
            sectorMask[static_cast<std::size_t>(vertex)] = true;
        }
    }
}

void markSurfaceAndMateVertices(
    const Level& level,
    const SurfaceRef ref,
    VertexMask& moved) noexcept
{
    if (!validSurface(level, ref))
    {
        return;
    }

    markSurfaceVertices(level, ref, moved);
    const Surface& surface = level.sectors[static_cast<std::size_t>(ref.sector)]
        .surfaces[static_cast<std::size_t>(ref.surface)];
    if (surface.adjoin && validSurface(level, *surface.adjoin))
    {
        markSurfaceVertices(level, *surface.adjoin, moved);
    }
}

GeometryTransformPreviewPlan makePlan(
    const Level& level,
    const VertexMask& moved)
{
    GeometryTransformPreviewPlan plan{};

    for (std::size_t sectorIndex = 0;
         sectorIndex < level.sectors.size();
         ++sectorIndex)
    {
        const Sector& sector = level.sectors[sectorIndex];
        const std::vector<bool>& sectorMask = moved[sectorIndex];
        if (std::ranges::none_of(sectorMask, [](const bool value)
            {
                return value;
            }))
        {
            continue;
        }

        for (std::size_t vertexIndex = 0;
             vertexIndex < sector.vertices.size();
             ++vertexIndex)
        {
            if (!sectorMask[vertexIndex])
            {
                continue;
            }
            const Vertex& vertex = sector.vertices[vertexIndex];
            plan.movedVertices.push_back(GeometryTransformPreviewVertex{
                static_cast<std::int32_t>(sectorIndex),
                static_cast<std::int32_t>(vertexIndex),
                TVector{ vertex.x, vertex.y, vertex.z } });
        }

        for (std::size_t surfaceIndex = 0;
             surfaceIndex < sector.surfaces.size();
             ++surfaceIndex)
        {
            const Surface& surface = sector.surfaces[surfaceIndex];
            std::size_t validVertexCount = 0;
            std::size_t movedVertexCount = 0;
            for (const std::int32_t vertex : surface.vertices)
            {
                if (vertex < 0
                    || static_cast<std::size_t>(vertex) >= sectorMask.size())
                {
                    continue;
                }
                ++validVertexCount;
                if (sectorMask[static_cast<std::size_t>(vertex)])
                {
                    ++movedVertexCount;
                }
            }
            if (movedVertexCount == 0u)
            {
                continue;
            }

            const SurfaceRef surfaceRef{
                static_cast<std::int32_t>(sectorIndex),
                static_cast<std::int32_t>(surfaceIndex) };
            if (movedVertexCount == validVertexCount)
            {
                plan.wholeSurfaces.push_back(surfaceRef);
                continue;
            }

            std::vector<bool> emitted(sectorMask.size(), false);
            for (const std::int32_t vertexIndex : surface.vertices)
            {
                if (vertexIndex < 0
                    || static_cast<std::size_t>(vertexIndex) >= sectorMask.size()
                    || !sectorMask[static_cast<std::size_t>(vertexIndex)]
                    || emitted[static_cast<std::size_t>(vertexIndex)])
                {
                    continue;
                }
                emitted[static_cast<std::size_t>(vertexIndex)] = true;
                const Vertex& vertex =
                    sector.vertices[static_cast<std::size_t>(vertexIndex)];
                plan.partialCorners.push_back(GeometryTransformPreviewCorner{
                    surfaceRef,
                    vertexIndex,
                    TVector{ vertex.x, vertex.y, vertex.z } });
            }
        }
    }

    return plan;
}

bool sectorIsConvexInEitherWinding(const Sector& sector)
{
    if ( level::sectorIsConvex(sector) )
    {
        return true;
    }

    Sector reversedWinding = sector;
    for ( Surface& surface : reversedWinding.surfaces )
    {
        std::reverse(surface.vertices.begin(), surface.vertices.end());
        std::reverse(surface.texVertices.begin(), surface.texVertices.end());
        surface.normal = level::surfaceCalcNormalNewell(reversedWinding, surface);
    }
    return level::sectorIsConvex(reversedWinding);
}

} // namespace

bool preservesAffectedScaleGeometry(
    const Level& source,
    const Level& candidate,
    const std::span<const std::int32_t> sectorIndices,
    const bool allowReversedWinding) noexcept
{
    try
    {
        for ( const std::int32_t sectorIndex : sectorIndices )
        {
            if ( sectorIndex < 0
                || static_cast<std::size_t>(sectorIndex) >= source.sectors.size()
                || static_cast<std::size_t>(sectorIndex) >= candidate.sectors.size() )
            {
                continue;
            }

            const Sector& sourceSector =
                source.sectors[static_cast<std::size_t>(sectorIndex)];
            const Sector& candidateSector =
                candidate.sectors[static_cast<std::size_t>(sectorIndex)];
            if ( sourceSector.surfaces.size() != candidateSector.surfaces.size() )
            {
                return false;
            }

            for ( std::size_t surfaceIndex = 0;
                 surfaceIndex < sourceSector.surfaces.size();
                 ++surfaceIndex )
            {
                const Surface& sourceSurface = sourceSector.surfaces[surfaceIndex];
                const Surface& candidateSurface =
                    candidateSector.surfaces[surfaceIndex];
                const TVector sourceNormal =
                    level::surfaceCalcNormalNewell(sourceSector, sourceSurface);
                const TVector candidateNormal =
                    level::surfaceCalcNormalNewell(
                        candidateSector, candidateSurface);
                if ( !sourceNormal.isZero() && candidateNormal.isZero() )
                {
                    return false;
                }
                if ( level::surfaceIsPlanar(sourceSector, sourceSurface)
                    && !level::surfaceIsPlanar(
                        candidateSector, candidateSurface) )
                {
                    return false;
                }
                if ( level::surfaceIsConvex(sourceSector, sourceSurface)
                    && !level::surfaceIsConvex(
                        candidateSector, candidateSurface) )
                {
                    return false;
                }
            }

            // A surface can remain locally valid while pulling a convex solid
            // into a concave shape. Rounded or previously nonconvex source
            // sectors are deliberately exempt from this stricter check.
            if ( level::sectorIsConvex(sourceSector) )
            {
                const bool candidateIsConvex = allowReversedWinding
                    ? sectorIsConvexInEitherWinding(candidateSector)
                    : level::sectorIsConvex(candidateSector);
                if ( !candidateIsConvex )
                {
                    return false;
                }
            }
        }
    }
    catch (...)
    {
        return false;
    }

    return true;
}

GeometryTransformPreviewPlan buildSurfaceTransformPreviewPlan(
    const Level& level,
    const std::span<const SurfaceRef> selectedSurfaces)
{
    VertexMask moved = makeVertexMask(level);
    for (const SurfaceRef ref : selectedSurfaces)
    {
        markSurfaceAndMateVertices(level, ref, moved);
    }
    return makePlan(level, moved);
}

GeometryTransformPreviewPlan buildSectorTransformPreviewPlan(
    const Level& level,
    const std::span<const std::int32_t> selectedSectors)
{
    VertexMask moved = makeVertexMask(level);
    for (const std::int32_t sectorIndex : selectedSectors)
    {
        if (!validSector(level, sectorIndex))
        {
            continue;
        }

        std::vector<bool>& sectorMask = moved[static_cast<std::size_t>(sectorIndex)];
        std::ranges::fill(sectorMask, true);

        const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        for (std::size_t surfaceIndex = 0;
             surfaceIndex < sector.surfaces.size();
             ++surfaceIndex)
        {
            const Surface& surface = sector.surfaces[surfaceIndex];
            if (surface.adjoin && validSurface(level, *surface.adjoin))
            {
                markSurfaceVertices(level, *surface.adjoin, moved);
            }
        }
    }
    return makePlan(level, moved);
}

level::ConnectedSurfaceUvPlan buildConnectedSurfaceUvPreviewPlan(
    const Level& level,
    const GeometryTransformPreviewPlan& geometryPlan,
    const bool includeFullyMovedSurfaces)
{
    std::vector<VertexRef> moved{};
    moved.reserve(geometryPlan.movedVertices.size());
    for (const GeometryTransformPreviewVertex& vertex :
         geometryPlan.movedVertices)
    {
        moved.push_back(VertexRef{ vertex.sector, vertex.vertex });
    }
    return level::captureConnectedSurfaceUvPlan(
        level, moved, includeFullyMovedSurfaces);
}

level::ConnectedSurfaceUvPlan buildConnectedSurfaceUvTransformPlan(
    const Level& level,
    const std::span<const std::int32_t> selectedSectors,
    const std::span<const SurfaceRef> selectedSurfaces,
    const bool includeFullyMovedSurfaces)
{
    VertexMask moved = makeVertexMask(level);
    for (const std::int32_t sectorIndex : selectedSectors)
    {
        if (!validSector(level, sectorIndex))
        {
            continue;
        }
        std::ranges::fill(
            moved[static_cast<std::size_t>(sectorIndex)], true);
        const Sector& sector = level.sectors[
            static_cast<std::size_t>(sectorIndex)];
        for (const Surface& surface : sector.surfaces)
        {
            if (surface.adjoin && validSurface(level, *surface.adjoin))
            {
                markSurfaceVertices(level, *surface.adjoin, moved);
            }
        }
    }
    for (const SurfaceRef ref : selectedSurfaces)
    {
        markSurfaceAndMateVertices(level, ref, moved);
    }
    return buildConnectedSurfaceUvPreviewPlan(
        level, makePlan(level, moved), includeFullyMovedSurfaces);
}

} // namespace sed::render
