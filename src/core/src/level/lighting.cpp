#include "sed/level/lighting.hpp"

#include "sed/level/geometry.hpp"
#include "sed/level/thing.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sed::level
{

namespace
{

struct SurfaceLightingData
{
    double d{};
    TBox bbox{};
    bool valid{};
};

bool calcSurfacePlaneD(const Sector& sector, const Surface& surface, double& d) noexcept
{
    if (surface.vertices.empty())
    {
        return false;
    }

    const std::int32_t v0Index = surface.vertices[0];
    if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= sector.vertices.size())
    {
        return false;
    }

    const Vertex& v0 = sector.vertices[static_cast<std::size_t>(v0Index)];
    d = surface.normal.Dot(v0.x, v0.y, v0.z);
    return true;
}

bool lightRangeIntersectsBounds(const SedLight& light, const TBox& bounds) noexcept
{
    const double nearestX = std::clamp(light.position.x, bounds.x1, bounds.x2);
    const double nearestY = std::clamp(light.position.y, bounds.y1, bounds.y2);
    const double nearestZ = std::clamp(light.position.z, bounds.z1, bounds.z2);
    const double dx = light.position.x - nearestX;
    const double dy = light.position.y - nearestY;
    const double dz = light.position.z - nearestZ;
    return (dx * dx) + (dy * dy) + (dz * dz) < light.range * light.range;
}

bool vertexWithinDirtyInfluences(
    const Vertex& vertex,
    const std::span<const LightingInfluenceSphere> dirtyInfluences) noexcept
{
    if (dirtyInfluences.empty())
    {
        return true;
    }

    return std::any_of(
        dirtyInfluences.begin(),
        dirtyInfluences.end(),
        [&vertex](const LightingInfluenceSphere& influence)
        {
            if (!(influence.radius > 0.0)
                || !std::isfinite(influence.radius)
                || !std::isfinite(influence.center.x)
                || !std::isfinite(influence.center.y)
                || !std::isfinite(influence.center.z))
            {
                return false;
            }

            const double dx = vertex.x - influence.center.x;
            const double dy = vertex.y - influence.center.y;
            const double dz = vertex.z - influence.center.z;
            return (dx * dx) + (dy * dy) + (dz * dz)
                < influence.radius * influence.radius;
        });
}

void zeroVertexLightsWithinInfluences(
    Level& level,
    const std::span<const std::int32_t> sectorIndices,
    const std::span<const LightingInfluenceSphere> dirtyInfluences) noexcept
{
    if (dirtyInfluences.empty())
    {
        levelZeroVertexLights(level, sectorIndices);
        return;
    }

    for (const std::int32_t sectorIndex : sectorIndices)
    {
        if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
        {
            continue;
        }

        Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        for (Surface& surface : sector.surfaces)
        {
            const std::size_t pairedCount = std::min(surface.vertices.size(), surface.texVertices.size());
            for (std::size_t vertexOffset = 0; vertexOffset < pairedCount; ++vertexOffset)
            {
                const std::int32_t vertexIndex = surface.vertices[vertexOffset];
                if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
                {
                    continue;
                }
                if (!vertexWithinDirtyInfluences(
                        sector.vertices[static_cast<std::size_t>(vertexIndex)], dirtyInfluences))
                {
                    continue;
                }

                ColorF& color = surface.texVertices[vertexOffset].color;
                color.r = 0.0f;
                color.g = 0.0f;
                color.b = 0.0f;
            }
        }
    }
}

} // namespace

void levelZeroVertexLights(Level& level, const std::span<const std::int32_t> sectorIndices) noexcept
{
    for (const std::int32_t sectorIndex : sectorIndices)
    {
        if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
        {
            continue;
        }

        Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        for (Surface& surface : sector.surfaces)
        {
            for (TexVertex& tv : surface.texVertices)
            {
                tv.color.r = 0.0f;
                tv.color.g = 0.0f;
                tv.color.b = 0.0f;
            }
        }
    }
}

void levelCalcSectorAmbients(Level& level, const std::span<const std::int32_t> sectorIndices) noexcept
{
    for (const std::int32_t sectorIndex : sectorIndices)
    {
        if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
        {
            continue;
        }

        Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        if ((sector.flags & SECF_NOAMBINETLIGHT) != 0)
        {
            continue;
        }

        ColorF vlight{};
        ColorF sflight{};
        vlight.setToZero();
        sflight.setToZero();

        std::size_t vertexCount = 0;

        for (Surface& surface : sector.surfaces)
        {
            sflight = sflight + surface.extraLight;
            vertexCount += surface.vertices.size();

            for (TexVertex& tv : surface.texVertices)
            {
                // Original SED clamps the stored baked color while deriving
                // ambient light, not merely a temporary copy.
                tv.color.normalize();
                vlight = vlight + tv.color;
            }
        }

        if (vertexCount > 0)
        {
            vlight = vlight / static_cast<float>(vertexCount);
        }

        if (!sector.surfaces.empty())
        {
            sflight = sflight / static_cast<float>(sector.surfaces.size());
        }

        if ((sector.flags & SECF_NORGBAMBINETLIGHT) != 0)
        {
            const float vi = vlight.intensity();
            const float si = sflight.intensity();
            vlight = ColorF{ vi, vi, vi, vlight.a };
            sflight = ColorF{ si, si, si, sflight.a };
        }

        sector.ambient = (vlight.intensity() > sflight.intensity()) ? vlight : sflight;
        sector.ambient.a = 0.0f;
    }
}

void levelCalcLighting(Level& level, const std::span<const std::int32_t> sectorIndices) noexcept
{
    levelZeroVertexLights(level, sectorIndices);

    std::int32_t totalSurfaces = 0;
    for (Sector& sector : level.sectors)
    {
        for (std::size_t i = 0; i < sector.surfaces.size(); ++i)
        {
            sector.surfaces[i].mark = totalSurfaces + static_cast<std::int32_t>(i);
        }

        totalSurfaces += static_cast<std::int32_t>(sector.surfaces.size());
    }

    std::vector<SurfaceLightingData> surfaceData;
    surfaceData.resize(static_cast<std::size_t>(std::max(totalSurfaces, 0)));

    for (const Sector& sector : level.sectors)
    {
        for (const Surface& surface : sector.surfaces)
        {
            if (surface.mark < 0 || static_cast<std::size_t>(surface.mark) >= surfaceData.size())
            {
                continue;
            }

            SurfaceLightingData& sd = surfaceData[static_cast<std::size_t>(surface.mark)];
            sd.bbox = surfaceCalcBBox(sector, surface);

            double d = 0.0;
            if (calcSurfacePlaneD(sector, surface, d))
            {
                sd.d = d;
                sd.valid = true;
            }
        }
    }

    for (SedLight& light : level.lights)
    {
        light.mark = levelFindSectorForXYZ(level, light.position.x, light.position.y, light.position.z);
    }

    auto isPointWithinLineLoose = [](const double x,
                                     const double y,
                                     const double z,
                                     const double x1,
                                     const double y1,
                                     const double z1,
                                     const double x2,
                                     const double y2,
                                     const double z2) noexcept
    {
        constexpr double eps = 1e-4;
        return (x - std::min(x1, x2) > -eps) && (std::max(x1, x2) - x > -eps)
            && (y - std::min(y1, y2) > -eps) && (std::max(y1, y2) - y > -eps)
            && (z - std::min(z1, z2) > -eps) && (std::max(z1, z2) - z > -eps);
    };

    auto isBlocked = [&](const SedLight& light,
                         const Surface& currentSurface,
                         const std::int32_t currentSectorIndex,
                         const double x1,
                         const double y1,
                         const double z1,
                         const double x2,
                         const double y2,
                         const double z2) noexcept
    {
        TBox lbox{};
        setBox(lbox, x1, x2, y1, y2, z1, z2);

        auto surfBlocks = [&](const Sector& sector, const Surface& surface) noexcept
        {
            if (&surface == &currentSurface)
            {
                return false;
            }

            if (surface.adjoin && ((surface.adjoinFlags & SAF_BlockLight) == 0))
            {
                return false;
            }

            if (surface.mark < 0 || static_cast<std::size_t>(surface.mark) >= surfaceData.size())
            {
                return false;
            }

            const SurfaceLightingData& sd = surfaceData[static_cast<std::size_t>(surface.mark)];
            if (!sd.valid)
            {
                return false;
            }

            if (!doBoxesIntersect(sd.bbox, lbox))
            {
                return false;
            }

            double ix = 0.0;
            double iy = 0.0;
            double iz = 0.0;
            if (!planeLineIntersection(surface.normal, sd.d, x1, y1, z1, x2, y2, z2, ix, iy, iz))
            {
                return false;
            }

            if (!isPointWithinLineLoose(ix, iy, iz, x1, y1, z1, x2, y2, z2))
            {
                return false;
            }

            if (VectorDistSqr3(ix, iy, iz, x2, y2, z2) < 0.0001)
            {
                return false;
            }
            if (VectorDistSqr3(ix, iy, iz, x1, y1, z1) < 0.0001)
            {
                return false;
            }

            return surfaceIsPointOn(sector, surface, ix, iy, iz);
        };

        if (light.mark != -1 && static_cast<std::size_t>(light.mark) < level.sectors.size())
        {
            const Sector& lightSector = level.sectors[static_cast<std::size_t>(light.mark)];
            for (const Surface& s : lightSector.surfaces)
            {
                if (surfBlocks(lightSector, s))
                {
                    return true;
                }
            }
        }

        if (currentSectorIndex >= 0 && static_cast<std::size_t>(currentSectorIndex) < level.sectors.size())
        {
            const Sector& curSector = level.sectors[static_cast<std::size_t>(currentSectorIndex)];
            for (const Surface& s : curSector.surfaces)
            {
                if (surfBlocks(curSector, s))
                {
                    return true;
                }
            }
        }

        for (std::int32_t i = 0; i < static_cast<std::int32_t>(level.sectors.size()); ++i)
        {
            if (i == currentSectorIndex || i == light.mark)
            {
                continue;
            }

            const Sector& other = level.sectors[static_cast<std::size_t>(i)];
            for (const Surface& s : other.surfaces)
            {
                if (surfBlocks(other, s))
                {
                    return true;
                }
            }
        }

        return false;
    };

    for (const std::int32_t selectedSectorIndex : sectorIndices)
    {
        if (selectedSectorIndex < 0 || static_cast<std::size_t>(selectedSectorIndex) >= level.sectors.size())
        {
            continue;
        }

        Sector& sector = level.sectors[static_cast<std::size_t>(selectedSectorIndex)];

        for (Surface& surface : sector.surfaces)
        {
            if (surface.vertices.empty())
            {
                continue;
            }
            if (surface.texVertices.size() != surface.vertices.size())
            {
                continue;
            }

            const std::int32_t v0Index = surface.vertices[0];
            if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= sector.vertices.size())
            {
                continue;
            }
            const Vertex& v0 = sector.vertices[static_cast<std::size_t>(v0Index)];

            for (const SedLight& light : level.lights)
            {
                const double surfaceFacing = surface.normal.Dot(light.position.Sub(v0.x, v0.y, v0.z));
                if (surfaceFacing < 0.001)
                {
                    continue;
                }
                if (light.range <= 0.0 || light.intensity <= 0.0)
                {
                    continue;
                }

                for (std::size_t i = 0; i < surface.vertices.size(); ++i)
                {
                    const std::int32_t vi = surface.vertices[i];
                    if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
                    {
                        continue;
                    }

                    const Vertex& vx = sector.vertices[static_cast<std::size_t>(vi)];
                    const double dist = light.position.Distance(vx.x, vx.y, vx.z);
                    if (dist >= light.range)
                    {
                        continue;
                    }

                    if (selectedSectorIndex != light.mark && ((light.flags & LF_NoBlock) == 0))
                    {
                        if (isBlocked(light, surface, selectedSectorIndex, light.position.x, light.position.y, light.position.z, vx.x, vx.y, vx.z))
                        {
                            continue;
                        }
                    }

                    const double falloff = (light.range - dist) / light.range;
                    const double intf = light.intensity * sqr(falloff);

                    TexVertex& tv = surface.texVertices[i];
                    if (level.kind == ProjectType::JKDF2)
                    {
                        tv.color = intensityToColor(tv.color.intensity() + static_cast<float>(intf));
                    }
                    else
                    {
                        tv.color.r += light.color.r * static_cast<float>(intf);
                        tv.color.g += light.color.g * static_cast<float>(intf);
                        tv.color.b += light.color.b * static_cast<float>(intf);
                    }
                }
            }
        }
    }

    levelCalcSectorAmbients(level, sectorIndices);
}

bool levelCalcLightingNewInterruptible(
    Level& level,
    const std::span<const std::int32_t> sectorIndices,
    const LightingProgressCallback& progress,
    const std::span<const LightingInfluenceSphere> dirtyInfluences)
{
    const std::size_t total = sectorIndices.size();
    if (progress && !progress(0, total))
    {
        return false;
    }

    std::vector<const SedLight*> validLights;
    validLights.reserve(level.lights.size());
    for (SedLight& light : level.lights)
    {
        light.mark = levelFindSectorForXYZ(level, light.position.x, light.position.y, light.position.z);
        if (light.mark != -1 && light.range > 0.0 && light.intensity > 0.0)
        {
            validLights.push_back(&light);
        }
    }

    if (validLights.empty())
    {
        // Removing or disabling the last usable light must clear the previous
        // bake in the requested full/dirty scope instead of leaving stale
        // vertex colors and sector ambients.
        zeroVertexLightsWithinInfluences(level, sectorIndices, dirtyInfluences);
        levelCalcSectorAmbients(level, sectorIndices);
        return !progress || progress(total, total);
    }

    zeroVertexLightsWithinInfluences(level, sectorIndices, dirtyInfluences);

    // The Pascal implementation reused Sector.mark as a visited flag and
    // cleared every sector before every light-to-vertex portal walk. On large
    // levels that O(sectors) clear dwarfs the actual graph traversal. A local
    // generation table preserves the traversal semantics without touching the
    // level or repeatedly sweeping all sectors.
    std::vector<std::uint32_t> portalVisitGeneration(level.sectors.size(), 0u);
    std::uint32_t currentPortalVisitGeneration = 0u;
    std::size_t workUnitsSinceProgressCheck = 0;
    std::vector<const SedLight*> sectorLights;
    sectorLights.reserve(validLights.size());
    std::vector<std::uint8_t> surfaceDirtyVertices;

    std::size_t completed = 0;
    bool cancellationRequested = false;
    auto continueLightingWork = [&]() -> bool
    {
        if (cancellationRequested)
        {
            return false;
        }
        if (++workUnitsSinceProgressCheck < 64)
        {
            return true;
        }
        workUnitsSinceProgressCheck = 0;
        if (progress && !progress(completed, total))
        {
            cancellationRequested = true;
            return false;
        }
        return true;
    };

    for (const std::int32_t selectedSectorIndex : sectorIndices)
    {
        if (completed > 0 && progress && !progress(completed, total))
        {
            return false;
        }

        if (selectedSectorIndex < 0 || static_cast<std::size_t>(selectedSectorIndex) >= level.sectors.size())
        {
            ++completed;
            continue;
        }

        Sector& sector = level.sectors[static_cast<std::size_t>(selectedSectorIndex)];

        // A light cannot affect any vertex in this sector when its range
        // sphere does not intersect the sector AABB. This conservative broad
        // phase avoids surface/portal work for distant lights while leaving
        // the original per-vertex range test authoritative.
        sectorLights.clear();
        if (!sector.vertices.empty())
        {
            const TBox sectorBounds = sectorCalcBBox(sector);
            for (const SedLight* light : validLights)
            {
                if (lightRangeIntersectsBounds(*light, sectorBounds))
                {
                    sectorLights.push_back(light);
                }
            }
        }

        for (Surface& surface : sector.surfaces)
        {
            if (!continueLightingWork())
            {
                return false;
            }
            if (surface.vertices.empty())
            {
                continue;
            }
            if (surface.texVertices.size() != surface.vertices.size())
            {
                continue;
            }

            const std::int32_t v0Index = surface.vertices[0];
            if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= sector.vertices.size())
            {
                continue;
            }
            const Vertex& v0 = sector.vertices[static_cast<std::size_t>(v0Index)];

            if (!dirtyInfluences.empty())
            {
                surfaceDirtyVertices.assign(surface.vertices.size(), std::uint8_t{0});
                for (std::size_t vertexOffset = 0; vertexOffset < surface.vertices.size(); ++vertexOffset)
                {
                    const std::int32_t vertexIndex = surface.vertices[vertexOffset];
                    if (vertexIndex >= 0
                        && static_cast<std::size_t>(vertexIndex) < sector.vertices.size()
                        && vertexWithinDirtyInfluences(
                            sector.vertices[static_cast<std::size_t>(vertexIndex)], dirtyInfluences))
                    {
                        surfaceDirtyVertices[vertexOffset] = std::uint8_t{1};
                    }
                }
                if (std::none_of(
                        surfaceDirtyVertices.begin(),
                        surfaceDirtyVertices.end(),
                        [](const std::uint8_t dirty) { return dirty != std::uint8_t{0}; }))
                {
                    continue;
                }
            }

            for (const SedLight* lightPtr : sectorLights)
            {
                if (!continueLightingWork())
                {
                    return false;
                }
                const SedLight& light = *lightPtr;

                const double surfaceFacing = light.position.Sub(v0.x, v0.y, v0.z).Dot(surface.normal);
                if (surfaceFacing <= 0.0)
                {
                    continue;
                }

                const double range2 = light.range * light.range;

                for (std::size_t i = 0; i < surface.vertices.size(); ++i)
                {
                    if (!continueLightingWork())
                    {
                        return false;
                    }

                    if (!dirtyInfluences.empty() && surfaceDirtyVertices[i] == std::uint8_t{0})
                    {
                        continue;
                    }

                    const std::int32_t vi = surface.vertices[i];
                    if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
                    {
                        continue;
                    }

                    const Vertex& vx = sector.vertices[static_cast<std::size_t>(vi)];
                    const TVector lvec = light.position.Sub(vx.x, vx.y, vx.z);

                    const double dist2 = lvec.Dot(lvec);
                    if (dist2 >= range2)
                    {
                        continue;
                    }

                    if ((light.flags & LF_NoBlock) == 0)
                    {
                        ++currentPortalVisitGeneration;
                        if (currentPortalVisitGeneration == 0u)
                        {
                            std::fill(portalVisitGeneration.begin(), portalVisitGeneration.end(), 0u);
                            currentPortalVisitGeneration = 1u;
                        }

                        auto canReach = [&](auto&& self, const std::int32_t viaSectorIndex) -> bool
                        {
                            if (!continueLightingWork())
                            {
                                return false;
                            }
                            if (viaSectorIndex < 0 || static_cast<std::size_t>(viaSectorIndex) >= level.sectors.size())
                            {
                                return false;
                            }

                            if (viaSectorIndex == selectedSectorIndex)
                            {
                                return true;
                            }

                            const std::size_t viaSectorOffset = static_cast<std::size_t>(viaSectorIndex);
                            if (portalVisitGeneration[viaSectorOffset] == currentPortalVisitGeneration)
                            {
                                return false;
                            }
                            portalVisitGeneration[viaSectorOffset] = currentPortalVisitGeneration;

                            const Sector& viaSec = level.sectors[viaSectorOffset];

                            for (const Surface& portal : viaSec.surfaces)
                            {
                                if (!portal.adjoin)
                                {
                                    continue;
                                }
                                if (portal.vertices.empty())
                                {
                                    continue;
                                }
                                if ((portal.adjoinFlags & SAF_BlockLight) != 0)
                                {
                                    continue;
                                }

                                if (lvec.Dot(portal.normal) <= -0.0001)
                                {
                                    continue;
                                }

                                if (!surfaceIsCrossed(viaSec, portal, light.position.x, light.position.y, light.position.z, vx.x, vx.y, vx.z))
                                {
                                    continue;
                                }

                                if (self(self, portal.adjoin->sector))
                                {
                                    return true;
                                }
                            }

                            return false;
                        };

                        const bool blocked = !canReach(canReach, light.mark);
                        if (cancellationRequested)
                        {
                            return false;
                        }
                        if (blocked)
                        {
                            continue;
                        }
                    }

                    const double dist = std::sqrt(dist2);
                    const double falloff = (light.range - dist) / light.range;
                    const double intf = light.intensity * sqr(falloff);

                    TexVertex& tv = surface.texVertices[i];
                    if (level.kind == ProjectType::JKDF2)
                    {
                        tv.color = intensityToColor(tv.color.intensity() + static_cast<float>(intf));
                    }
                    else
                    {
                        tv.color.r += light.color.r * static_cast<float>(intf);
                        tv.color.g += light.color.g * static_cast<float>(intf);
                        tv.color.b += light.color.b * static_cast<float>(intf);
                    }
                }
            }
        }
        ++completed;
    }

    levelCalcSectorAmbients(level, sectorIndices);
    return !progress || progress(total, total);
}

void levelCalcLightingNew(Level& level, const std::span<const std::int32_t> sectorIndices) noexcept
{
    (void)levelCalcLightingNewInterruptible(level, sectorIndices, {});
}

MotsArchLightingBakeResult levelCalcMotsArchLighting(
    Level& level,
    const std::span<const MotsArchLightingBakeTarget> targets,
    const assets::Templates* templates,
    const LightingProgressCallback& progress)
{
    constexpr std::string_view kProjectMessage =
        "MotS ARCH baked-light calculation requires a MotS level.";
    constexpr std::string_view kNoopMessage =
        "MotS ARCH baked-light calculation produced no changes.";
    constexpr std::string_view kAppliedMessage =
        "Updated MotS ARCHLIGHTING from flagged baked lights.";
    constexpr std::string_view kCancelledMessage =
        "MotS ARCH baked-light calculation was cancelled.";
    constexpr std::string_view kIndexMessage =
        "MotS ARCH bake target has an invalid Thing index.";
    constexpr std::string_view kIdentityMessage =
        "MotS ARCH bake target no longer matches its Thing identity.";
    constexpr std::string_view kModelMessage =
        "MotS ARCH bake target no longer matches its Thing model.";
    constexpr std::string_view kSectorMessage =
        "MotS ARCH bake target no longer matches its Thing sector.";
    constexpr std::string_view kFlagMessage =
        "MotS ARCH bake target is not an effective MotS Arch Thing.";
    constexpr std::string_view kTopologyMessage =
        "MotS ARCH bake target has invalid or incompatible all-LOD topology.";
    constexpr std::string_view kRecordMessage =
        "MotS ARCH bake target has an invalid existing ARCHLIGHTING record.";
    constexpr std::string_view kLightMessage =
        "MotS ARCH bake requires finite baked-light inputs.";
    constexpr std::string_view kVertexMessage =
        "MotS ARCH bake target contains a non-finite world-space vertex.";
    constexpr std::string_view kBudgetMessage =
        "MotS ARCH bake exceeds the bounded ARCHLIGHTING record budget.";
    constexpr std::size_t kMaxBakeItems = 1'000'000u;
    constexpr std::int32_t kAffectArchThingsFlag = 0x20;
    constexpr std::int32_t kNoBlockFlag = 0x01;

    const auto rejected = [](const std::string_view message)
    {
        return MotsArchLightingBakeResult{
            MotsArchLightingBakeStatus::Rejected,
            std::string(message),
            {},
        };
    };
    if (level.kind != ProjectType::MOTS)
    {
        return rejected(kProjectMessage);
    }
    if (targets.size() > kMaxBakeItems)
    {
        return rejected(kBudgetMessage);
    }

    const auto canonicalModelName = [](const std::string_view value)
    {
        std::string name(text::trimView(value));
        if (name.empty() || text::iequalsAscii(name, "none"))
        {
            return std::string{};
        }
        name = text::lowerAscii(name);
        if (name.size() < 4 || name.substr(name.size() - 4) != ".3do")
        {
            name += ".3do";
        }
        return name;
    };
    const auto modelNameForThing = [&](const Thing& thing)
    {
        const TplValue* model = thingFindValue(level, templates, thing, "model3d");
        if (!model)
        {
            model = thingFindValue(level, templates, thing, "model3do");
        }
        return model ? canonicalModelName(model->asString()) : std::string{};
    };
    const auto finiteVector = [](const TVector& point) noexcept
    {
        return std::isfinite(point.x) && std::isfinite(point.y)
            && std::isfinite(point.z);
    };
    const auto maximumVertexCounts = [&](const MotsArchLightingTopology& topology,
                                        std::vector<std::size_t>& counts)
    {
        if (topology.geosets.empty() || topology.geosets.size() > kMaxBakeItems)
        {
            return false;
        }
        std::size_t meshCount = 0;
        for (const std::vector<std::size_t>& geoset : topology.geosets)
        {
            if (geoset.size() > kMaxBakeItems)
            {
                return false;
            }
            for (const std::size_t count : geoset)
            {
                if (count > kMaxBakeItems)
                {
                    return false;
                }
            }
            meshCount = std::max(meshCount, geoset.size());
        }
        if (meshCount == 0)
        {
            return false;
        }
        counts.assign(meshCount, 0u);
        std::size_t totalVertices = 0;
        for (const std::vector<std::size_t>& geoset : topology.geosets)
        {
            for (std::size_t meshIndex = 0; meshIndex < geoset.size(); ++meshIndex)
            {
                counts[meshIndex] = std::max(counts[meshIndex], geoset[meshIndex]);
            }
        }
        for (const std::size_t count : counts)
        {
            if (totalVertices > kMaxBakeItems - count)
            {
                return false;
            }
            totalVertices += count;
        }
        return true;
    };
    const auto sameChannels = [](const MotsArchLightVertex& lhs,
                                 const MotsArchLightVertex& rhs) noexcept
    {
        return lhs.mono == rhs.mono && lhs.red == rhs.red
            && lhs.green == rhs.green && lhs.blue == rhs.blue;
    };
    const auto validRecord = [](const MotsArchLightObject& object)
    {
        if (object.wireIndex < 0 || object.meshes.size() > kMaxBakeItems)
        {
            return false;
        }
        std::unordered_set<std::int32_t> meshWireIndices{};
        meshWireIndices.reserve(object.meshes.size());
        std::size_t vertexCount = 0;
        for (const MotsArchLightMesh& mesh : object.meshes)
        {
            if (mesh.wireIndex < 0
                || !meshWireIndices.insert(mesh.wireIndex).second
                || mesh.vertices.size() > kMaxBakeItems
                || vertexCount > kMaxBakeItems - mesh.vertices.size())
            {
                return false;
            }
            vertexCount += mesh.vertices.size();
            for (const MotsArchLightVertex& vertex : mesh.vertices)
            {
                if (!std::isfinite(vertex.mono) || !std::isfinite(vertex.red)
                    || !std::isfinite(vertex.green) || !std::isfinite(vertex.blue))
                {
                    return false;
                }
            }
        }
        return true;
    };
    const auto sameRecordChannels = [&](const MotsArchLightObject& lhs,
                                        const MotsArchLightObject& rhs)
    {
        if (lhs.meshes.size() != rhs.meshes.size())
        {
            return false;
        }
        for (std::size_t meshIndex = 0; meshIndex < lhs.meshes.size(); ++meshIndex)
        {
            if (lhs.meshes[meshIndex].vertices.size()
                != rhs.meshes[meshIndex].vertices.size())
            {
                return false;
            }
            for (std::size_t vertexIndex = 0;
                 vertexIndex < lhs.meshes[meshIndex].vertices.size();
                 ++vertexIndex)
            {
                if (!sameChannels(lhs.meshes[meshIndex].vertices[vertexIndex],
                                  rhs.meshes[meshIndex].vertices[vertexIndex]))
                {
                    return false;
                }
            }
        }
        return true;
    };

    bool hasFlaggedSource = false;
    for (const SedLight& light : level.lights)
    {
        hasFlaggedSource = hasFlaggedSource
            || (light.flags & kAffectArchThingsFlag) != 0;
    }

    std::unordered_map<std::int32_t, std::size_t> baselineForThing{};
    baselineForThing.reserve(level.motsArchLightingBakeBaselines.size());
    for (std::size_t index = 0;
         index < level.motsArchLightingBakeBaselines.size(); ++index)
    {
        const MotsArchLightingBakeBaseline& baseline =
            level.motsArchLightingBakeBaselines[index];
        if (baseline.originalArchLightingIndex < -1
            || (baseline.originalArchLightingIndex == -1)
                != !baseline.originalArchLighting.has_value()
            || canonicalModelName(baseline.modelName) != baseline.modelName
            || baseline.modelName.empty()
            || (baseline.originalArchLighting
                && !validRecord(*baseline.originalArchLighting))
            || !baselineForThing.emplace(baseline.thingId, index).second)
        {
            return rejected("MotS ARCH bake provenance is invalid or duplicated.");
        }
    }

    if (!hasFlaggedSource)
    {
        if (level.motsArchLightingBakeBaselines.empty())
        {
            return { MotsArchLightingBakeStatus::Noop, std::string(kNoopMessage), {} };
        }

        std::unordered_map<std::int32_t, std::size_t> thingForId{};
        std::unordered_set<std::int32_t> duplicateThingIds{};
        thingForId.reserve(level.things.size());
        for (std::size_t index = 0; index < level.things.size(); ++index)
        {
            if (!thingForId.emplace(level.things[index].id, index).second)
            {
                duplicateThingIds.insert(level.things[index].id);
            }
        }

        std::vector<MotsArchLightObject> stagedRecords = level.motsArchLighting;
        std::vector<std::int32_t> stagedReferences{};
        stagedReferences.reserve(level.things.size());
        for (const Thing& thing : level.things)
        {
            stagedReferences.push_back(thing.archLightingIndex);
        }
        std::vector<std::size_t> recordReferences(stagedRecords.size(), 0u);
        for (const std::int32_t reference : stagedReferences)
        {
            if (reference >= 0
                && static_cast<std::size_t>(reference) < recordReferences.size())
            {
                ++recordReferences[static_cast<std::size_t>(reference)];
            }
        }
        std::size_t aggregateMeshCount = 0;
        std::size_t aggregateVertexCount = 0;
        std::unordered_set<std::int32_t> usedWireIndices{};
        for (const MotsArchLightObject& object : stagedRecords)
        {
            usedWireIndices.insert(object.wireIndex);
        }
        std::int64_t nextWireIndex = 0;
        auto appendOriginal = [&](const MotsArchLightObject& original)
        {
            if (stagedRecords.size() >= kMaxBakeItems
                || original.meshes.size() > kMaxBakeItems - aggregateMeshCount)
            {
                return std::int32_t{ -1 };
            }
            std::size_t vertexCount = 0;
            for (const MotsArchLightMesh& mesh : original.meshes)
            {
                if (mesh.vertices.size() > kMaxBakeItems - vertexCount)
                {
                    return std::int32_t{ -1 };
                }
                vertexCount += mesh.vertices.size();
            }
            if (vertexCount > kMaxBakeItems - aggregateVertexCount)
            {
                return std::int32_t{ -1 };
            }
            MotsArchLightObject copy = original;
            if (usedWireIndices.contains(copy.wireIndex))
            {
                while (nextWireIndex <= std::numeric_limits<std::int32_t>::max()
                    && usedWireIndices.contains(static_cast<std::int32_t>(nextWireIndex)))
                {
                    ++nextWireIndex;
                }
                if (nextWireIndex > std::numeric_limits<std::int32_t>::max())
                {
                    return std::int32_t{ -1 };
                }
                copy.wireIndex = static_cast<std::int32_t>(nextWireIndex++);
            }
            usedWireIndices.insert(copy.wireIndex);
            aggregateMeshCount += copy.meshes.size();
            aggregateVertexCount += vertexCount;
            stagedRecords.push_back(std::move(copy));
            return static_cast<std::int32_t>(stagedRecords.size() - 1u);
        };

        if (stagedRecords.size() > kMaxBakeItems)
        {
            return rejected(kBudgetMessage);
        }
        for (const MotsArchLightObject& object : stagedRecords)
        {
            if (object.meshes.size() > kMaxBakeItems - aggregateMeshCount)
            {
                return rejected(kBudgetMessage);
            }
            aggregateMeshCount += object.meshes.size();
            for (const MotsArchLightMesh& mesh : object.meshes)
            {
                if (mesh.vertices.size() > kMaxBakeItems - aggregateVertexCount)
                {
                    return rejected(kBudgetMessage);
                }
                aggregateVertexCount += mesh.vertices.size();
            }
        }

        std::vector<std::int32_t> changedThings{};
        changedThings.reserve(level.motsArchLightingBakeBaselines.size());
        for (const MotsArchLightingBakeBaseline& baseline :
             level.motsArchLightingBakeBaselines)
        {
            if (duplicateThingIds.contains(baseline.thingId))
            {
                return rejected("MotS ARCH bake provenance matches duplicate Thing IDs.");
            }
            const auto found = thingForId.find(baseline.thingId);
            if (found == thingForId.end())
            {
                continue; // The owner was deleted; discard only its provenance.
            }
            const std::size_t thingIndex = found->second;
            const std::int32_t currentReference = stagedReferences[thingIndex];
            std::int32_t restoredReference = -1;
            bool recordChanged = false;
            if (baseline.originalArchLighting)
            {
                const MotsArchLightObject& original = *baseline.originalArchLighting;
                const auto exact = std::find(stagedRecords.begin(), stagedRecords.end(), original);
                if (exact != stagedRecords.end())
                {
                    restoredReference = static_cast<std::int32_t>(
                        std::distance(stagedRecords.begin(), exact));
                }
                else if (currentReference >= 0
                    && static_cast<std::size_t>(currentReference) < stagedRecords.size()
                    && recordReferences[static_cast<std::size_t>(currentReference)] == 1u)
                {
                    MotsArchLightObject& current = stagedRecords[
                        static_cast<std::size_t>(currentReference)];
                    recordChanged = current != original;
                    current = original;
                    restoredReference = currentReference;
                }
                else
                {
                    restoredReference = appendOriginal(original);
                    if (restoredReference < 0) return rejected(kBudgetMessage);
                }
            }
            if (currentReference != restoredReference || recordChanged)
            {
                stagedReferences[thingIndex] = restoredReference;
                changedThings.push_back(static_cast<std::int32_t>(thingIndex));
            }
        }

        level.motsArchLighting.swap(stagedRecords);
        for (std::size_t index = 0; index < level.things.size(); ++index)
        {
            level.things[index].archLightingIndex = stagedReferences[index];
        }
        level.motsArchLightingBakeBaselines.clear();
        std::sort(changedThings.begin(), changedThings.end());
        changedThings.erase(std::unique(changedThings.begin(), changedThings.end()),
            changedThings.end());
        MotsArchLightingBakeResult result{};
        result.status = MotsArchLightingBakeStatus::Applied;
        result.message = "Restored authored MotS ARCHLIGHTING after flagged baked lights were removed.";
        result.changedThingIndices = std::move(changedThings);
        result.provenanceChanged = true;
        return result;
    }

    std::vector<MotsArchLightingBakeBaseline> stagedBaselines =
        level.motsArchLightingBakeBaselines;
    bool provenanceChanged = false;

    struct StagedBakeTarget
    {
        const MotsArchLightingBakeTarget* target{};
        MotsArchLightObject object{};
        MotsArchLightObject generated{};
        std::vector<std::vector<std::uint8_t>> seenLodValues{};
        std::int32_t originalIndex{ -1 };
        std::int32_t destinationIndex{ -1 };
        bool baselineAdded{};
        bool changed{};
    };
    std::vector<StagedBakeTarget> staged{};
    staged.reserve(targets.size());
    std::unordered_set<std::int32_t> seenTargets{};
    seenTargets.reserve(targets.size());
    std::unordered_set<std::int32_t> seenTargetThingIds{};
    seenTargetThingIds.reserve(targets.size());
    std::size_t totalVertexSamples = 0;

    for (const MotsArchLightingBakeTarget& target : targets)
    {
        if (target.thingIndex < 0
            || static_cast<std::size_t>(target.thingIndex) >= level.things.size())
        {
            return rejected(kIndexMessage);
        }
        if (!seenTargets.insert(target.thingIndex).second)
        {
            return rejected("MotS ARCH bake target list contains a duplicate Thing.");
        }
        const Thing& thing = level.things[static_cast<std::size_t>(target.thingIndex)];
        if (!seenTargetThingIds.insert(target.thingId).second)
        {
            return rejected("MotS ARCH bake targets contain a duplicate stable Thing ID.");
        }
        if (thing.id != target.thingId || thing.name != target.thingName)
        {
            return rejected(kIdentityMessage);
        }
        if (thing.sector != target.sectorIndex)
        {
            return rejected(kSectorMessage);
        }
        if (thing.sector < 0 || static_cast<std::size_t>(thing.sector) >= level.sectors.size())
        {
            return rejected(kSectorMessage);
        }
        if (!isMotsArchThing(level, thing, templates))
        {
            return rejected(kFlagMessage);
        }
        if (canonicalModelName(target.modelName) != modelNameForThing(thing)
            || canonicalModelName(target.modelName).empty())
        {
            return rejected(kModelMessage);
        }
        if (thing.archLightingIndex < -1
            || (thing.archLightingIndex >= 0
                && static_cast<std::size_t>(thing.archLightingIndex)
                    >= level.motsArchLighting.size()))
        {
            return rejected(kRecordMessage);
        }

        std::vector<std::size_t> meshVertexCounts{};
        if (!maximumVertexCounts(target.topology, meshVertexCounts)
            || target.lods.size() != target.topology.geosets.size())
        {
            return rejected(kTopologyMessage);
        }
        std::size_t meshVertexTotal = 0;
        for (const std::size_t count : meshVertexCounts)
        {
            if (meshVertexTotal > kMaxBakeItems - count)
            {
                return rejected(kBudgetMessage);
            }
            meshVertexTotal += count;
        }
        std::size_t targetVertexSamples = 0;
        for (const MotsArchLightingBakeGeoset& lod : target.lods)
        {
            for (const MotsArchLightingBakeMesh& mesh : lod.meshes)
            {
                if (mesh.worldVertices.size() > kMaxBakeItems - targetVertexSamples)
                {
                    return rejected(kBudgetMessage);
                }
                targetVertexSamples += mesh.worldVertices.size();
            }
        }
        if (targetVertexSamples > kMaxBakeItems - totalVertexSamples)
        {
            return rejected(kBudgetMessage);
        }
        totalVertexSamples += targetVertexSamples;

        if (thing.archLightingIndex >= 0)
        {
            const MotsArchLightObject& current = level.motsArchLighting[
                static_cast<std::size_t>(thing.archLightingIndex)];
            if (!validRecord(current) || current.meshes.size() != meshVertexCounts.size())
            {
                return rejected(kRecordMessage);
            }
            for (std::size_t meshIndex = 0; meshIndex < meshVertexCounts.size(); ++meshIndex)
            {
                if (current.meshes[meshIndex].vertices.size() != meshVertexCounts[meshIndex])
                {
                    return rejected(kTopologyMessage);
                }
            }
        }

        std::size_t baselineIndex = 0;
        const auto existingBaseline = baselineForThing.find(thing.id);
        bool baselineAdded = existingBaseline == baselineForThing.end();
        if (!baselineAdded)
        {
            baselineIndex = existingBaseline->second;
            const MotsArchLightingBakeBaseline& baseline =
                stagedBaselines[baselineIndex];
            if (baseline.modelName != canonicalModelName(target.modelName))
            {
                return rejected("MotS ARCH bake baseline model no longer matches Thing "
                    + std::to_string(target.thingId) + " '" + target.thingName + "'.");
            }
            if (baseline.originalArchLighting)
            {
                const MotsArchLightObject& original = *baseline.originalArchLighting;
                if (original.meshes.size() != meshVertexCounts.size())
                {
                    return rejected(kTopologyMessage);
                }
                for (std::size_t meshIndex = 0;
                     meshIndex < meshVertexCounts.size(); ++meshIndex)
                {
                    if (original.meshes[meshIndex].vertices.size()
                        != meshVertexCounts[meshIndex])
                    {
                        return rejected(kTopologyMessage);
                    }
                }
            }
        }
        else
        {
            baselineIndex = stagedBaselines.size();
            MotsArchLightingBakeBaseline baseline{};
            baseline.thingId = thing.id;
            baseline.modelName = canonicalModelName(target.modelName);
            baseline.originalArchLightingIndex = thing.archLightingIndex;
            if (thing.archLightingIndex >= 0)
            {
                baseline.originalArchLighting = level.motsArchLighting[
                    static_cast<std::size_t>(thing.archLightingIndex)];
            }
            stagedBaselines.push_back(std::move(baseline));
            baselineForThing.emplace(thing.id, baselineIndex);
            provenanceChanged = true;
        }

        for (std::size_t lodIndex = 0; lodIndex < target.lods.size(); ++lodIndex)
        {
            const MotsArchLightingBakeGeoset& lod = target.lods[lodIndex];
            for (const MotsArchLightingBakeMesh& mesh : lod.meshes)
            {
                if (mesh.sourceMeshIndex >= meshVertexCounts.size()
                    || mesh.sourceMeshIndex >= target.topology.geosets[lodIndex].size())
                {
                    return rejected(kTopologyMessage);
                }
                // Multiple hierarchy nodes may clone one source mesh in a LOD.
                // Each rendered instance contributes independently; the shared
                // ARCH source-vertex stream receives the per-channel maximum.
                if (mesh.worldVertices.size()
                    != target.topology.geosets[lodIndex][mesh.sourceMeshIndex])
                {
                    return rejected(kTopologyMessage);
                }
                for (const TVector& vertex : mesh.worldVertices)
                {
                    if (!finiteVector(vertex))
                    {
                        return rejected(kVertexMessage);
                    }
                }
            }
        }

        StagedBakeTarget stagedTarget{};
        stagedTarget.target = &target;
        stagedTarget.originalIndex = thing.archLightingIndex;
        stagedTarget.destinationIndex = thing.archLightingIndex;
        stagedTarget.baselineAdded = baselineAdded;
        const MotsArchLightingBakeBaseline& baseline = stagedBaselines[baselineIndex];
        stagedTarget.object.wireIndex = baseline.originalArchLighting
            ? baseline.originalArchLighting->wireIndex : -1;
        stagedTarget.generated.wireIndex = stagedTarget.object.wireIndex;
        stagedTarget.object.meshes.reserve(meshVertexCounts.size());
        stagedTarget.generated.meshes.reserve(meshVertexCounts.size());
        stagedTarget.seenLodValues.reserve(meshVertexCounts.size());
        for (std::size_t meshIndex = 0; meshIndex < meshVertexCounts.size(); ++meshIndex)
        {
            MotsArchLightMesh outputMesh{};
            MotsArchLightMesh generatedMesh{};
            if (baseline.originalArchLighting)
            {
                outputMesh = baseline.originalArchLighting->meshes[meshIndex];
            }
            else
            {
                outputMesh.wireIndex = static_cast<std::int32_t>(meshIndex);
                outputMesh.vertices.assign(meshVertexCounts[meshIndex],
                    MotsArchLightVertex{});
            }
            generatedMesh.wireIndex = outputMesh.wireIndex;
            generatedMesh.vertices.assign(meshVertexCounts[meshIndex],
                MotsArchLightVertex{});
            stagedTarget.seenLodValues.emplace_back(
                meshVertexCounts[meshIndex], std::uint8_t{ 0 });
            stagedTarget.object.meshes.push_back(std::move(outputMesh));
            stagedTarget.generated.meshes.push_back(std::move(generatedMesh));
        }
        staged.push_back(std::move(stagedTarget));
    }

    for (std::size_t thingIndex = 0; thingIndex < level.things.size(); ++thingIndex)
    {
        const Thing& thing = level.things[thingIndex];
        if (isMotsArchThing(level, thing, templates)
            && !modelNameForThing(thing).empty()
            && !seenTargets.contains(static_cast<std::int32_t>(thingIndex)))
        {
            return rejected("MotS ARCH bake target geometry is missing for Thing "
                + std::to_string(thing.id) + " '" + thing.name
                + "'; the model resource could not be resolved.");
        }
    }

    struct BakeLight
    {
        const SedLight* source{};
        std::int32_t sector{ -1 };
    };
    std::vector<BakeLight> bakeLights{};
    bakeLights.reserve(level.lights.size());
    for (const SedLight& light : level.lights)
    {
        if ((light.flags & kAffectArchThingsFlag) == 0)
        {
            continue;
        }
        if (!finiteVector(light.position)
            || !std::isfinite(light.range)
            || !std::isfinite(light.intensity)
            || !std::isfinite(light.color.r)
            || !std::isfinite(light.color.g)
            || !std::isfinite(light.color.b)
            || !std::isfinite(light.color.a))
        {
            return rejected(kLightMessage);
        }
        if (!(light.range > 0.0) || !(light.intensity > 0.0))
        {
            continue;
        }
        bakeLights.push_back(BakeLight{
            &light,
            levelFindSectorForXYZ(level, light.position.x, light.position.y, light.position.z),
        });
    }

    std::size_t workTotal = 0;
    if (totalVertexSamples != 0
        && bakeLights.size() > (std::numeric_limits<std::size_t>::max() / totalVertexSamples))
    {
        return rejected(kBudgetMessage);
    }
    workTotal = totalVertexSamples * bakeLights.size();
    if (workTotal == 0)
    {
        workTotal = std::max<std::size_t>(totalVertexSamples, 1u);
    }
    if (progress && !progress(0, workTotal))
    {
        return { MotsArchLightingBakeStatus::Cancelled, std::string(kCancelledMessage), {} };
    }

    std::size_t workSinceProgress = 0;
    std::size_t progressDone = 0;
    bool cancelled = false;
    std::vector<std::uint32_t> portalVisitGeneration(level.sectors.size(), 0u);
    std::uint32_t currentPortalVisitGeneration = 0u;
    std::vector<std::int32_t> portalPending{};
    portalPending.reserve(std::min<std::size_t>(level.sectors.size(), 256u));
    const auto continueWork = [&]()
    {
        if (++workSinceProgress < 64u)
        {
            return true;
        }
        workSinceProgress = 0;
        if (progress && !progress(progressDone, workTotal))
        {
            cancelled = true;
            return false;
        }
        return true;
    };
    const auto lightCanReach = [&](const std::int32_t sourceSector,
                                   const std::int32_t targetSector,
                                   const TVector& lightPosition,
                                   const TVector& vertex,
                                   const TVector& lightToVertex)
    {
        if (sourceSector == targetSector)
        {
            return true;
        }
        ++currentPortalVisitGeneration;
        if (currentPortalVisitGeneration == 0u)
        {
            std::fill(portalVisitGeneration.begin(), portalVisitGeneration.end(), 0u);
            currentPortalVisitGeneration = 1u;
        }
        portalPending.clear();
        portalPending.push_back(sourceSector);
        portalVisitGeneration[static_cast<std::size_t>(sourceSector)] =
            currentPortalVisitGeneration;
        while (!portalPending.empty())
        {
            if (!continueWork())
            {
                return false;
            }
            const std::int32_t viaSectorIndex = portalPending.back();
            portalPending.pop_back();
            if (viaSectorIndex == targetSector)
            {
                return true;
            }
            const Sector& viaSector = level.sectors[static_cast<std::size_t>(viaSectorIndex)];
            for (const Surface& portal : viaSector.surfaces)
            {
                if (!portal.adjoin || portal.vertices.empty()
                    || (portal.adjoinFlags & SAF_BlockLight) != 0
                    || lightToVertex.Dot(portal.normal) <= -0.0001
                    || !surfaceIsCrossed(
                        viaSector, portal,
                        lightPosition.x, lightPosition.y, lightPosition.z,
                        vertex.x, vertex.y, vertex.z))
                {
                    continue;
                }
                const std::int32_t nextSector = portal.adjoin->sector;
                if (nextSector < 0
                    || static_cast<std::size_t>(nextSector) >= level.sectors.size()
                    || portalVisitGeneration[static_cast<std::size_t>(nextSector)]
                        == currentPortalVisitGeneration)
                {
                    continue;
                }
                portalVisitGeneration[static_cast<std::size_t>(nextSector)] =
                    currentPortalVisitGeneration;
                portalPending.push_back(nextSector);
            }
        }
        return false;
    };

    // Validate candidate light output and calculate one independent result per
    // LOD occurrence. Each source-mesh/vertex pair stores the channel-wise max
    // across alternative LODs; repeated lights in one LOD are summed.
    for (std::size_t targetOrdinal = 0; targetOrdinal < staged.size(); ++targetOrdinal)
    {
        StagedBakeTarget& stagedTarget = staged[targetOrdinal];
        const MotsArchLightingBakeTarget& target = *stagedTarget.target;
        const std::int32_t ownerSector = target.sectorIndex;
        for (std::size_t lodIndex = 0; lodIndex < target.lods.size(); ++lodIndex)
        {
            for (const MotsArchLightingBakeMesh& mesh : target.lods[lodIndex].meshes)
            {
                MotsArchLightMesh& outputMesh =
                    stagedTarget.generated.meshes[mesh.sourceMeshIndex];
                for (std::size_t vertexIndex = 0;
                     vertexIndex < mesh.worldVertices.size();
                     ++vertexIndex)
                {
                    const TVector& vertex = mesh.worldVertices[vertexIndex];
                    double mono = 0.0;
                    double red = 0.0;
                    double green = 0.0;
                    double blue = 0.0;
                    if (bakeLights.empty())
                    {
                        ++progressDone;
                        if (!continueWork())
                        {
                            return { MotsArchLightingBakeStatus::Cancelled,
                                std::string(kCancelledMessage), {} };
                        }
                    }
                    for (const BakeLight& bakeLight : bakeLights)
                    {
                        const SedLight& light = *bakeLight.source;
                        const double dx = light.position.x - vertex.x;
                        const double dy = light.position.y - vertex.y;
                        const double dz = light.position.z - vertex.z;
                        const double distSquared = dx * dx + dy * dy + dz * dz;
                        const double rangeSquared = light.range * light.range;
                        if (!std::isfinite(distSquared) || !std::isfinite(rangeSquared))
                        {
                            return rejected(kLightMessage);
                        }
                        if (distSquared >= rangeSquared)
                        {
                            ++progressDone;
                            if (!continueWork())
                            {
                                return { MotsArchLightingBakeStatus::Cancelled,
                                    std::string(kCancelledMessage), {} };
                            }
                            continue;
                        }
                        if (bakeLight.sector < 0)
                        {
                            ++progressDone;
                            if (!continueWork())
                            {
                                return { MotsArchLightingBakeStatus::Cancelled,
                                    std::string(kCancelledMessage), {} };
                            }
                            continue;
                        }
                        if ((light.flags & kNoBlockFlag) == 0
                            && !lightCanReach(
                                bakeLight.sector,
                                ownerSector,
                                light.position,
                                vertex,
                                TVector{ dx, dy, dz }))
                        {
                            if (cancelled)
                            {
                                return { MotsArchLightingBakeStatus::Cancelled,
                                    std::string(kCancelledMessage), {} };
                            }
                            ++progressDone;
                            if (!continueWork())
                            {
                                return { MotsArchLightingBakeStatus::Cancelled,
                                    std::string(kCancelledMessage), {} };
                            }
                            continue;
                        }

                        const double distance = std::sqrt(distSquared);
                        const double falloff = (light.range - distance) / light.range;
                        const double factor = light.intensity * falloff * falloff;
                        mono += static_cast<double>(light.color.intensity()) * factor;
                        red += static_cast<double>(light.color.r) * factor;
                        green += static_cast<double>(light.color.g) * factor;
                        blue += static_cast<double>(light.color.b) * factor;
                        if (!std::isfinite(mono) || !std::isfinite(red)
                            || !std::isfinite(green) || !std::isfinite(blue))
                        {
                            return rejected(kLightMessage);
                        }
                        ++progressDone;
                        if (!continueWork())
                        {
                            return { MotsArchLightingBakeStatus::Cancelled,
                                std::string(kCancelledMessage), {} };
                        }
                    }

                    const MotsArchLightVertex generated{
                        static_cast<float>(mono),
                        static_cast<float>(red),
                        static_cast<float>(green),
                        static_cast<float>(blue),
                    };
                    if (!std::isfinite(generated.mono) || !std::isfinite(generated.red)
                        || !std::isfinite(generated.green) || !std::isfinite(generated.blue))
                    {
                        return rejected(kLightMessage);
                    }
                    const bool wasSeen = stagedTarget.seenLodValues
                        [mesh.sourceMeshIndex][vertexIndex] != 0;
                    MotsArchLightVertex& destination = outputMesh.vertices[vertexIndex];
                    if (!wasSeen)
                    {
                        destination = generated;
                        stagedTarget.seenLodValues
                            [mesh.sourceMeshIndex][vertexIndex] = 1;
                    }
                    else
                    {
                        destination.mono = std::max(destination.mono, generated.mono);
                        destination.red = std::max(destination.red, generated.red);
                        destination.green = std::max(destination.green, generated.green);
                        destination.blue = std::max(destination.blue, generated.blue);
                    }
                }
            }
        }

        for (std::size_t meshIndex = 0;
             meshIndex < stagedTarget.object.meshes.size(); ++meshIndex)
        {
            MotsArchLightMesh& outputMesh = stagedTarget.object.meshes[meshIndex];
            const MotsArchLightMesh& generatedMesh =
                stagedTarget.generated.meshes[meshIndex];
            for (std::size_t vertexIndex = 0;
                 vertexIndex < outputMesh.vertices.size(); ++vertexIndex)
            {
                MotsArchLightVertex& output = outputMesh.vertices[vertexIndex];
                const MotsArchLightVertex& contribution =
                    generatedMesh.vertices[vertexIndex];
                output.mono += contribution.mono;
                output.red += contribution.red;
                output.green += contribution.green;
                output.blue += contribution.blue;
                if (!std::isfinite(output.mono) || !std::isfinite(output.red)
                    || !std::isfinite(output.green) || !std::isfinite(output.blue))
                {
                    return rejected(kLightMessage);
                }
            }
        }
    }

    if (progress && !progress(workTotal, workTotal))
    {
        return { MotsArchLightingBakeStatus::Cancelled, std::string(kCancelledMessage), {} };
    }

    // Restore provenance for deleted, unflagged, or model-less owners before
    // applying current targets. Keep every mutation staged until all validation
    // and archive-budget checks have completed.
    std::vector<MotsArchLightObject> stagedRecords = level.motsArchLighting;
    std::vector<std::int32_t> stagedReferences{};
    stagedReferences.reserve(level.things.size());
    std::unordered_map<std::int32_t, std::size_t> thingForId{};
    std::unordered_set<std::int32_t> duplicateThingIds{};
    thingForId.reserve(level.things.size());
    for (std::size_t thingIndex = 0; thingIndex < level.things.size(); ++thingIndex)
    {
        const Thing& thing = level.things[thingIndex];
        stagedReferences.push_back(thing.archLightingIndex);
        if (!thingForId.emplace(thing.id, thingIndex).second)
        {
            duplicateThingIds.insert(thing.id);
        }
    }
    std::unordered_set<std::int32_t> activeTargetThingIds{};
    activeTargetThingIds.reserve(staged.size());
    for (const StagedBakeTarget& target : staged)
    {
        activeTargetThingIds.insert(target.target->thingId);
    }

    std::size_t aggregateMeshCount = 0;
    std::size_t aggregateVertexCount = 0;
    bool existingRecordBudgetValid = stagedRecords.size() <= kMaxBakeItems;
    std::unordered_set<std::int32_t> usedWireIndices{};
    usedWireIndices.reserve(stagedRecords.size() + staged.size());
    for (const MotsArchLightObject& object : stagedRecords)
    {
        if (object.wireIndex >= 0) usedWireIndices.insert(object.wireIndex);
        if (object.meshes.size() > kMaxBakeItems - aggregateMeshCount)
        {
            existingRecordBudgetValid = false;
        }
        else aggregateMeshCount += object.meshes.size();
        for (const MotsArchLightMesh& mesh : object.meshes)
        {
            if (mesh.vertices.size() > kMaxBakeItems - aggregateVertexCount)
            {
                existingRecordBudgetValid = false;
            }
            else aggregateVertexCount += mesh.vertices.size();
        }
    }
    if (!existingRecordBudgetValid) return rejected(kBudgetMessage);

    std::int64_t nextWireIndex = 0;
    const auto appendRecord = [&](MotsArchLightObject object)
        -> std::int32_t
    {
        if (stagedRecords.size() >= kMaxBakeItems
            || object.meshes.size() > kMaxBakeItems - aggregateMeshCount)
        {
            return -1;
        }
        std::size_t vertexCount = 0;
        for (const MotsArchLightMesh& mesh : object.meshes)
        {
            if (mesh.vertices.size() > kMaxBakeItems - vertexCount)
            {
                return -1;
            }
            vertexCount += mesh.vertices.size();
        }
        if (vertexCount > kMaxBakeItems - aggregateVertexCount)
        {
            return -1;
        }
        while (nextWireIndex <= std::numeric_limits<std::int32_t>::max()
            && usedWireIndices.contains(static_cast<std::int32_t>(nextWireIndex)))
        {
            ++nextWireIndex;
        }
        if (nextWireIndex > std::numeric_limits<std::int32_t>::max()) return -1;
        if (object.wireIndex < 0 || usedWireIndices.contains(object.wireIndex))
        {
            object.wireIndex = static_cast<std::int32_t>(nextWireIndex++);
        }
        usedWireIndices.insert(object.wireIndex);
        aggregateMeshCount += object.meshes.size();
        aggregateVertexCount += vertexCount;
        stagedRecords.push_back(std::move(object));
        return static_cast<std::int32_t>(stagedRecords.size() - 1u);
    };

    std::vector<std::int32_t> restoredThingIndices{};
    std::vector<MotsArchLightingBakeBaseline> retainedBaselines{};
    retainedBaselines.reserve(stagedBaselines.size());
    for (const MotsArchLightingBakeBaseline& baseline : stagedBaselines)
    {
        if (duplicateThingIds.contains(baseline.thingId))
        {
            return rejected("MotS ARCH bake provenance matches duplicate Thing IDs.");
        }
        const auto owner = thingForId.find(baseline.thingId);
        if (owner != thingForId.end())
        {
            const Thing& thing = level.things[owner->second];
            const bool stillBakeTarget =
                isMotsArchThing(level, thing, templates)
                && !modelNameForThing(thing).empty()
                && activeTargetThingIds.contains(baseline.thingId);
            if (stillBakeTarget)
            {
                retainedBaselines.push_back(baseline);
                continue;
            }
        }

        provenanceChanged = true;
        if (owner == thingForId.end()) continue; // Deleted owner: prune metadata only.

        const std::size_t thingIndex = owner->second;
        const std::int32_t currentReference = stagedReferences[thingIndex];
        std::int32_t restoredReference = -1;
        bool recordChanged = false;
        if (baseline.originalArchLighting)
        {
            const MotsArchLightObject& original = *baseline.originalArchLighting;
            const auto exact = std::find(
                stagedRecords.begin(), stagedRecords.end(), original);
            if (exact != stagedRecords.end())
            {
                restoredReference = static_cast<std::int32_t>(
                    std::distance(stagedRecords.begin(), exact));
            }
            else if (currentReference >= 0
                && static_cast<std::size_t>(currentReference) < stagedRecords.size())
            {
                // Do not overwrite a record still shared by another Thing.
                std::size_t currentReferences = 0;
                for (const std::int32_t reference : stagedReferences)
                {
                    currentReferences += reference == currentReference ? 1u : 0u;
                }
                if (currentReferences == 1u)
                {
                    MotsArchLightObject& current = stagedRecords[
                        static_cast<std::size_t>(currentReference)];
                    recordChanged = current != original;
                    current = original;
                    restoredReference = currentReference;
                }
                else
                {
                    restoredReference = appendRecord(original);
                    if (restoredReference < 0) return rejected(kBudgetMessage);
                }
            }
            else
            {
                restoredReference = appendRecord(original);
                if (restoredReference < 0) return rejected(kBudgetMessage);
            }
        }
        if (currentReference != restoredReference || recordChanged)
        {
            stagedReferences[thingIndex] = restoredReference;
            restoredThingIndices.push_back(static_cast<std::int32_t>(thingIndex));
        }
    }
    stagedBaselines.swap(retainedBaselines);

    std::vector<std::size_t> recordReferences(stagedRecords.size(), 0u);
    for (const std::int32_t reference : stagedReferences)
    {
        if (reference >= 0
            && static_cast<std::size_t>(reference) < recordReferences.size())
        {
            ++recordReferences[static_cast<std::size_t>(reference)];
        }
    }

    MotsArchLightingBakeResult appliedResult{};
    appliedResult.status = MotsArchLightingBakeStatus::Applied;
    appliedResult.message = kAppliedMessage;
    appliedResult.changedThingIndices.reserve(
        staged.size() + restoredThingIndices.size());
    appliedResult.changedThingIndices.insert(
        appliedResult.changedThingIndices.end(),
        restoredThingIndices.begin(), restoredThingIndices.end());
    for (StagedBakeTarget& target : staged)
    {
        const std::int32_t existingIndex = target.originalIndex;
        const MotsArchLightObject* existing = existingIndex >= 0
            ? &level.motsArchLighting[static_cast<std::size_t>(existingIndex)]
            : nullptr;
        target.changed = target.baselineAdded || existing == nullptr
            || !sameRecordChannels(*existing, target.object);
        if (!target.changed)
        {
            continue;
        }
        const bool canUpdateInPlace = existing != nullptr
            && recordReferences[static_cast<std::size_t>(existingIndex)] == 1u;
        if (canUpdateInPlace)
        {
            target.object.wireIndex = existing->wireIndex;
            for (std::size_t meshIndex = 0;
                 meshIndex < target.object.meshes.size();
                 ++meshIndex)
            {
                target.object.meshes[meshIndex].wireIndex =
                    existing->meshes[meshIndex].wireIndex;
            }
            stagedRecords[static_cast<std::size_t>(existingIndex)] = target.object;
            continue;
        }

        if (!existingRecordBudgetValid
            || stagedRecords.size() >= kMaxBakeItems
            || target.object.meshes.size() > kMaxBakeItems - aggregateMeshCount)
        {
            return rejected(kBudgetMessage);
        }
        std::size_t targetVertexCount = 0;
        for (const MotsArchLightMesh& mesh : target.object.meshes)
        {
            if (mesh.vertices.size() > kMaxBakeItems - targetVertexCount)
            {
                return rejected(kBudgetMessage);
            }
            targetVertexCount += mesh.vertices.size();
        }
        if (targetVertexCount > kMaxBakeItems - aggregateVertexCount)
        {
            return rejected(kBudgetMessage);
        }
        while (nextWireIndex <= std::numeric_limits<std::int32_t>::max()
            && usedWireIndices.contains(static_cast<std::int32_t>(nextWireIndex)))
        {
            ++nextWireIndex;
        }
        if (nextWireIndex > std::numeric_limits<std::int32_t>::max())
        {
            return rejected(kBudgetMessage);
        }
        target.object.wireIndex = static_cast<std::int32_t>(nextWireIndex);
        aggregateMeshCount += target.object.meshes.size();
        aggregateVertexCount += targetVertexCount;
        usedWireIndices.insert(target.object.wireIndex);
        ++nextWireIndex;
        stagedRecords.push_back(target.object);
        target.destinationIndex = static_cast<std::int32_t>(stagedRecords.size() - 1u);
    }

    for (const StagedBakeTarget& target : staged)
    {
        if (target.changed)
        {
            appliedResult.changedThingIndices.push_back(target.target->thingIndex);
        }
    }
    appliedResult.provenanceChanged = provenanceChanged;
    if (appliedResult.changedThingIndices.empty() && !provenanceChanged)
    {
        return { MotsArchLightingBakeStatus::Noop, std::string(kNoopMessage), {} };
    }

    // All allocations and validation are complete. Swap the new ARCH table
    // and provenance, then publish already-validated owner references.
    for (const StagedBakeTarget& target : staged)
    {
        if (target.changed)
        {
            stagedReferences[static_cast<std::size_t>(target.target->thingIndex)] =
                target.destinationIndex;
        }
    }
    level.motsArchLighting.swap(stagedRecords);
    level.motsArchLightingBakeBaselines.swap(stagedBaselines);
    for (std::size_t thingIndex = 0; thingIndex < level.things.size(); ++thingIndex)
    {
        level.things[thingIndex].archLightingIndex = stagedReferences[thingIndex];
    }
    std::sort(appliedResult.changedThingIndices.begin(),
        appliedResult.changedThingIndices.end());
    appliedResult.changedThingIndices.erase(
        std::unique(appliedResult.changedThingIndices.begin(),
            appliedResult.changedThingIndices.end()),
        appliedResult.changedThingIndices.end());
    return appliedResult;
}

} // namespace sed::level
