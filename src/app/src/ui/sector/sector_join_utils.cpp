#include "ui/sector/sector_join_utils.hpp"

#include "sed/level/geometry.hpp"
#include "ui/sector/sector_wire_utils.hpp"
#include "ui/selection/selection_tool_availability_utils.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace sed::ui::sector
{
namespace
{

struct PendingTarget
{
    Sector seed{};
    std::int32_t sectorIndex{-1};
};

struct JoinSectorIdentityContext
{
    std::unordered_set<std::int32_t> originalSectorIds{};
    std::unordered_set<std::int32_t> originalUnselectedSectorIds{};
    std::unordered_set<std::int32_t> generatedSectorIds{};

    bool admits(const Sector& sector) const
    {
        if (originalUnselectedSectorIds.contains(sector.id))
        {
            return false;
        }

        return originalSectorIds.contains(sector.id) || generatedSectorIds.contains(sector.id);
    }

    void observeGeneratedSectors(const Level& level)
    {
        for (const Sector& sector : level.sectors)
        {
            if (!originalSectorIds.contains(sector.id))
            {
                generatedSectorIds.insert(sector.id);
            }
        }
    }
};

bool hasValidJoinIdentityState(const Level& level)
{
    if ( level.curId == std::numeric_limits<std::int32_t>::max() )
    {
        return false;
    }

    std::unordered_set<std::int32_t> sectorIds{};
    sectorIds.reserve(level.sectors.size());
    std::int32_t maximumId = std::numeric_limits<std::int32_t>::lowest();
    for (const Sector& sector : level.sectors)
    {
        if (!sectorIds.insert(sector.id).second)
        {
            return false;
        }
        maximumId = std::max(maximumId, sector.id);
    }

    // Level::newId allocates IDs for all native entity owners, not just sectors.
    // Keep Join previews from reusing another entity's existing identity too.
    const auto considerIds = [&maximumId](const auto& entities)
    {
        for (const auto& entity : entities)
        {
            maximumId = std::max(maximumId, entity.id);
        }
    };
    considerIds(level.cogs);
    considerIds(level.things);
    considerIds(level.lights);
    considerIds(level.h3doNodes);

    return level.curId > maximumId;
}

JoinSectorIdentityContext makeJoinSectorIdentityContext(
    const Level& level,
    const std::vector<std::int32_t>& selectedSectors)
{
    JoinSectorIdentityContext context{};
    context.originalSectorIds.reserve(level.sectors.size());
    context.originalUnselectedSectorIds.reserve(level.sectors.size() - selectedSectors.size());
    context.generatedSectorIds.reserve(level.sectors.size());

    std::unordered_set<std::int32_t> selectedIds{};
    selectedIds.reserve(selectedSectors.size());
    for (const std::int32_t sectorIndex : selectedSectors)
    {
        selectedIds.insert(level.sectors[static_cast<std::size_t>(sectorIndex)].id);
    }

    for (const Sector& sector : level.sectors)
    {
        context.originalSectorIds.insert(sector.id);
        if (!selectedIds.contains(sector.id))
        {
            context.originalUnselectedSectorIds.insert(sector.id);
        }
    }

    return context;
}

bool representativeInside(const Sector& sector, const Sector& container) noexcept
{
    if (sector.vertices.empty())
    {
        return false;
    }

    const TVector center = level::sectorCalcCenter(sector);
    if (level::sectorContainsPoint(container, center.x, center.y, center.z))
    {
        return true;
    }

    return std::ranges::any_of(
        sector.vertices,
        [&container](const Vertex& vertex)
        {
            return level::sectorContainsPoint(container, vertex.x, vertex.y, vertex.z);
        });
}

TVector resolvedSurfaceNormal(const Sector& sector, const Surface& surface) noexcept
{
    TVector normal = surface.normal;
    if (normal.length() == 0.0)
    {
        normal = level::surfaceCalcNormalNewell(sector, surface);
    }

    if (normal.normalize() == 0.0)
    {
        return TVector{};
    }

    return normal;
}

bool containsInteriorPoint(const Sector& sector, const TVector& point) noexcept
{
    for (const Surface& surface : sector.surfaces)
    {
        if (surface.vertices.empty())
        {
            continue;
        }

        const std::int32_t v0Index = surface.vertices[0];
        if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= sector.vertices.size())
        {
            return false;
        }

        const TVector normal = resolvedSurfaceNormal(sector, surface);
        if (normal.length() == 0.0)
        {
            continue;
        }

        const Vertex& v0 = sector.vertices[static_cast<std::size_t>(v0Index)];
        if (normal.dot(point.x - v0.x, point.y - v0.y, point.z - v0.z) <= 0.001)
        {
            return false;
        }
    }

    return true;
}

bool sectorHasInteriorSampleInside(const Sector& source, const Sector& container) noexcept
{
    // Step 1: the centroid is the cheapest reliable hint for most clipped cutter fragments.
    if (containsInteriorPoint(container, level::sectorCalcCenter(source)))
    {
        return true;
    }

    // Step 2: rotated cutters can cross a target through edges or split faces, so sample the simple
    // features that remain stable after cleaving: vertices, edge midpoints, and surface centroids.
    for (const Surface& surface : source.surfaces)
    {
        TVector surfaceSum{};
        std::int32_t validVertexCount = 0;

        for (std::size_t i = 0; i < surface.vertices.size(); ++i)
        {
            const std::int32_t currentIndex = surface.vertices[i];
            if (currentIndex < 0 || static_cast<std::size_t>(currentIndex) >= source.vertices.size())
            {
                continue;
            }

            const Vertex& currentVertex = source.vertices[static_cast<std::size_t>(currentIndex)];
            const TVector current{ currentVertex.x, currentVertex.y, currentVertex.z };
            surfaceSum = surfaceSum + current;
            ++validVertexCount;

            if (containsInteriorPoint(container, current))
            {
                return true;
            }

            if (surface.vertices.size() < 2)
            {
                continue;
            }

            const std::int32_t nextIndex = surface.vertices[(i + 1) % surface.vertices.size()];
            if (nextIndex < 0 || static_cast<std::size_t>(nextIndex) >= source.vertices.size())
            {
                continue;
            }

            const Vertex& nextVertex = source.vertices[static_cast<std::size_t>(nextIndex)];
            const TVector midpoint{
                (currentVertex.x + nextVertex.x) * 0.5,
                (currentVertex.y + nextVertex.y) * 0.5,
                (currentVertex.z + nextVertex.z) * 0.5,
            };
            if (containsInteriorPoint(container, midpoint))
            {
                return true;
            }
        }

        if (validVertexCount <= 0)
        {
            continue;
        }

        const TVector surfaceCenter = surfaceSum * (1.0 / static_cast<double>(validVertexCount));
        if (containsInteriorPoint(container, surfaceCenter))
        {
            return true;
        }
    }

    return false;
}

bool interiorOverlaps(const Sector& sector, const Sector& container) noexcept
{
    if (sector.vertices.empty() || container.vertices.empty())
    {
        return false;
    }

    if (!level::doBoxesIntersect(level::sectorCalcBBox(sector), level::sectorCalcBBox(container)))
    {
        return false;
    }

    const auto sectorsOverlap = [](const Sector& a, const Sector& b) noexcept
    {
        const auto surfacesDoNotSeparate = [](const Sector& source, const Sector& other) noexcept
        {
            for (const Surface& surface : source.surfaces)
            {
                if (surface.vertices.empty())
                {
                    continue;
                }

                const std::int32_t v0Index = surface.vertices[0];
                if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= source.vertices.size())
                {
                    return false;
                }

                const TVector normal = resolvedSurfaceNormal(source, surface);
                if (normal.length() == 0.0)
                {
                    continue;
                }

                const Vertex& v0 = source.vertices[static_cast<std::size_t>(v0Index)];
                bool hasVertexInsidePlane = false;
                for (const Vertex& vertex : other.vertices)
                {
                    if (normal.dot(vertex.x - v0.x, vertex.y - v0.y, vertex.z - v0.z) > 0.01)
                    {
                        hasVertexInsidePlane = true;
                        break;
                    }
                }

                if (!hasVertexInsidePlane)
                {
                    return false;
                }
            }

            return true;
        };

        return surfacesDoNotSeparate(a, b) && surfacesDoNotSeparate(b, a);
    };

    return sectorsOverlap(sector, container)
        || sectorHasInteriorSampleInside(sector, container)
        || sectorHasInteriorSampleInside(container, sector);
}

std::vector<std::int32_t> sortedSelectedSectors(const editor::Selection& selection)
{
    std::vector<std::int32_t> sectors{};
    sectors.reserve(selection.sectors.size());
    for (const std::int32_t sectorIndex : selection.sectors)
    {
        sectors.push_back(sectorIndex);
    }

    std::sort(sectors.begin(), sectors.end());
    sectors.erase(std::unique(sectors.begin(), sectors.end()), sectors.end());
    return sectors;
}

std::int32_t chooseKnifeSector(const std::vector<std::int32_t>& sectors, const std::int32_t activeSector) noexcept
{
    if (std::ranges::find(sectors, activeSector) != sectors.end())
    {
        return activeSector;
    }

    return sectors.empty() ? -1 : sectors.front();
}

std::vector<std::int32_t> collectSectorsInsideSeed(
    const Level& level,
    const Sector& seed,
    const JoinSectorIdentityContext& identityContext)
{
    std::vector<std::int32_t> result{};
    result.reserve(level.sectors.size());

    for (std::int32_t sectorIndex = 0; sectorIndex < static_cast<std::int32_t>(level.sectors.size()); ++sectorIndex)
    {
        const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        if (identityContext.admits(sector) && representativeInside(sector, seed))
        {
            result.push_back(sectorIndex);
        }
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::vector<std::int32_t> collectSectorsInsideSeeds(
    const Level& level,
    const std::vector<Sector>& seeds,
    const JoinSectorIdentityContext& identityContext)
{
    std::vector<std::int32_t> result{};

    for (const Sector& seed : seeds)
    {
        std::vector<std::int32_t> seedSectors = collectSectorsInsideSeed(level, seed, identityContext);
        result.insert(result.end(), seedSectors.begin(), seedSectors.end());
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

bool interiorOverlapsAnySeed(const Sector& sector, const std::vector<Sector>& seeds) noexcept
{
    return std::ranges::any_of(
        seeds,
        [&sector](const Sector& seed)
        {
            return interiorOverlaps(sector, seed);
        });
}

std::vector<std::int32_t> collectKnifeFamilyForMode(
    const Level& level,
    const Sector& knifeSeed,
    const std::vector<Sector>& targetSeeds,
    const level::SectorConnectKnifeMode mode,
    const JoinSectorIdentityContext& identityContext)
{
    std::vector<std::int32_t> result = collectSectorsInsideSeed(level, knifeSeed, identityContext);
    if (mode != level::SectorConnectKnifeMode::CutTargetHolesOnly)
    {
        return result;
    }

    // Step 1: hole-only mode clips the knife at the target boundary, so target-interior pieces are not knife results.
    std::erase_if(
        result,
        [&level, &targetSeeds](const std::int32_t sectorIndex)
        {
            if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
            {
                return true;
            }

            return interiorOverlapsAnySeed(level.sectors[static_cast<std::size_t>(sectorIndex)], targetSeeds);
        });

    return result;
}

std::int32_t findSectorFromSeed(
    const Level& level,
    const Sector& seed,
    const std::int32_t preferred,
    const JoinSectorIdentityContext& identityContext)
{
    if (preferred >= 0 && static_cast<std::size_t>(preferred) < level.sectors.size())
    {
        const Sector& sector = level.sectors[static_cast<std::size_t>(preferred)];
        if (identityContext.admits(sector) && representativeInside(sector, seed))
        {
            return preferred;
        }
    }

    const std::vector<std::int32_t> candidates = collectSectorsInsideSeed(level, seed, identityContext);
    return candidates.empty() ? -1 : candidates.front();
}

std::vector<std::int32_t> collectPreviewSectors(
    const Level& level,
    const std::vector<Sector>& selectedSeeds,
    const JoinSectorIdentityContext& identityContext)
{
    std::vector<std::int32_t> result{};

    for (std::int32_t sectorIndex = 0; sectorIndex < static_cast<std::int32_t>(level.sectors.size()); ++sectorIndex)
    {
        const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        if (identityContext.admits(sector)
            && std::ranges::any_of(
                selectedSeeds,
                [&sector](const Sector& seed)
                {
                    return representativeInside(sector, seed);
                }))
        {
            result.push_back(sectorIndex);
        }
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

bool applySectorJoin(
    Level& level,
    editor::Selection& selection,
    const std::int32_t activeSector,
    const level::SectorConnectKnifeMode mode,
    std::int32_t& newActiveSector,
    std::vector<std::int32_t>& previewSectors,
    bool* const idExhausted)
{
    if ( !ui::selection::canJoinSectorSelection(selection) )
    {
        return false;
    }

    const std::vector<std::int32_t> selectedSectors = sortedSelectedSectors(selection);
    const std::int32_t keep = chooseKnifeSector(selectedSectors, activeSector);
    if (keep < 0 || static_cast<std::size_t>(keep) >= level.sectors.size())
    {
        return false;
    }

    for (const std::int32_t sectorIndex : selectedSectors)
    {
        if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
        {
            return false;
        }
    }

    JoinSectorIdentityContext identityContext = makeJoinSectorIdentityContext(level, selectedSectors);

    std::vector<Sector> selectedSeeds{};
    selectedSeeds.reserve(selectedSectors.size());
    for (const std::int32_t sectorIndex : selectedSectors)
    {
        selectedSeeds.push_back(level.sectors[static_cast<std::size_t>(sectorIndex)]);
    }

    const Sector knifeSeed = level.sectors[static_cast<std::size_t>(keep)];

    std::vector<PendingTarget> pendingTargets{};
    std::vector<Sector> targetSeeds{};
    pendingTargets.reserve(selectedSectors.size());
    targetSeeds.reserve(selectedSectors.size());
    for (const std::int32_t sectorIndex : selectedSectors)
    {
        if (sectorIndex == keep)
        {
            continue;
        }

        const Sector& targetSeed = level.sectors[static_cast<std::size_t>(sectorIndex)];
        pendingTargets.push_back(PendingTarget{targetSeed, sectorIndex});
        targetSeeds.push_back(targetSeed);
    }

    std::vector<std::int32_t> knifeFamily{keep};
    std::vector<Sector> processedTargetSeeds{};
    processedTargetSeeds.reserve(targetSeeds.size());
    bool any = false;
    bool progress = true;
    while (!pendingTargets.empty() && progress)
    {
        progress = false;

        for (std::ptrdiff_t pendingIndex = static_cast<std::ptrdiff_t>(pendingTargets.size()) - 1; pendingIndex >= 0; --pendingIndex)
        {
            PendingTarget& pending = pendingTargets[static_cast<std::size_t>(pendingIndex)];
            pending.sectorIndex = findSectorFromSeed(
                level,
                pending.seed,
                pending.sectorIndex,
                identityContext);
            if (pending.sectorIndex < 0)
            {
                continue;
            }

            bool joinedTarget = false;
            for (const std::int32_t knifeCandidate : knifeFamily)
            {
                if (knifeCandidate < 0 || static_cast<std::size_t>(knifeCandidate) >= level.sectors.size())
                {
                    continue;
                }
                if (knifeCandidate == pending.sectorIndex)
                {
                    continue;
                }

                bool candidateIdExhausted = false;
                if (!level::levelConnectSectorsKnife(
                        level,
                        knifeCandidate,
                        pending.sectorIndex,
                        mode,
                        &candidateIdExhausted))
                {
                    if ( candidateIdExhausted )
                    {
                        if ( idExhausted )
                        {
                            *idExhausted = true;
                        }
                        return false;
                    }
                    continue;
                }

                identityContext.observeGeneratedSectors(level);
                any = true;
                progress = true;
                joinedTarget = true;
                break;
            }

            if (!joinedTarget)
            {
                continue;
            }

            processedTargetSeeds.push_back(pending.seed);
            knifeFamily = collectKnifeFamilyForMode(
                level,
                knifeSeed,
                processedTargetSeeds,
                mode,
                identityContext);
            pendingTargets.erase(pendingTargets.begin() + pendingIndex);
            break;
        }
    }

    if (!any)
    {
        return false;
    }

    knifeFamily = collectKnifeFamilyForMode(level, knifeSeed, targetSeeds, mode, identityContext);
    if (mode == level::SectorConnectKnifeMode::CutTargetHolesOnly)
    {
        std::vector<std::int32_t> targetFamily = collectSectorsInsideSeeds(
            level,
            targetSeeds,
            identityContext);
        std::erase_if(
            targetFamily,
            [&level, &knifeFamily](const std::int32_t sectorIndex)
            {
                return sectorIndex < 0
                    || static_cast<std::size_t>(sectorIndex) >= level.sectors.size()
                    || std::ranges::find(knifeFamily, sectorIndex) != knifeFamily.end();
            });

        // Step 5: after every target has been cut, one final narrow surface pass catches
        // portal faces that only become valid once the whole selected target family is settled.
        const bool connectedSurfaceCandidates = level::levelConnectSectorSurfaceCandidates(
            level,
            knifeFamily,
            targetFamily);
        identityContext.observeGeneratedSectors(level);
        if (connectedSurfaceCandidates)
        {
            knifeFamily = collectKnifeFamilyForMode(level, knifeSeed, targetSeeds, mode, identityContext);
        }
    }

    selection.clear();
    for (const std::int32_t sectorIndex : knifeFamily)
    {
        if (sectorIndex >= 0 && static_cast<std::size_t>(sectorIndex) < level.sectors.size())
        {
            selection.sectors.insert(sectorIndex);
        }
    }

    std::sort(knifeFamily.begin(), knifeFamily.end());
    newActiveSector = knifeFamily.empty() ? -1 : knifeFamily.front();
    previewSectors = collectPreviewSectors(level, selectedSeeds, identityContext);
    return true;
}

} // namespace

std::optional<SectorJoinPreviewResult> buildSectorJoinPreview(
    const Level& level,
    const editor::Selection& selection,
    const std::int32_t activeSector,
    const level::SectorConnectKnifeMode mode)
{
    return buildSectorJoinPreview(
        level, selection, activeSector, mode, nullptr);
}

std::optional<SectorJoinPreviewResult> buildSectorJoinPreview(
    const Level& level,
    const editor::Selection& selection,
    const std::int32_t activeSector,
    const level::SectorConnectKnifeMode mode,
    bool* const idExhausted)
{
    if ( idExhausted )
    {
        *idExhausted = false;
    }
    if ( level.curId == std::numeric_limits<std::int32_t>::max() )
    {
        if ( idExhausted )
        {
            *idExhausted = true;
        }
        return std::nullopt;
    }
    if ( !hasValidJoinIdentityState(level) )
    {
        return std::nullopt;
    }

    try
    {
        SectorJoinPreviewResult result{};
        result.level = level;
        result.selection = selection;

        if ( !applySectorJoin(
                result.level,
                result.selection,
                activeSector,
                mode,
                result.activeSector,
                result.previewSectors,
                idExhausted) )
        {
            return std::nullopt;
        }

        return result;
    }
    catch ( const IdExhaustionError& )
    {
        if ( idExhausted )
        {
            *idExhausted = true;
        }
        return std::nullopt;
    }
}

std::vector<QVector3D> buildSectorJoinPreviewLines(const SectorJoinPreviewResult& preview)
{
    std::vector<QVector3D> lines{};

    for (const std::int32_t sectorIndex : preview.previewSectors)
    {
        if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= preview.level.sectors.size())
        {
            continue;
        }

        std::vector<QVector3D> sectorLines =
            wireLinesFromSector(preview.level.sectors[static_cast<std::size_t>(sectorIndex)]);
        lines.insert(lines.end(), sectorLines.begin(), sectorLines.end());
    }

    return lines;
}

} // namespace sed::ui::sector
