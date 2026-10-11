#include "sed/level/merge.hpp"

#include "sed/level/cleave.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level/uv.hpp"
#include "geometry_precision.hpp"
#include "cog_reference_utils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace sed::level
{
namespace
{

constexpr std::int32_t dfMaxSurfVerts = 24;
constexpr std::int32_t ijimMaxSurfVerts = 64;

std::int32_t maxSurfaceVertices(const Level& level) noexcept
{
    return (level.kind == ProjectType::IJIM) ? ijimMaxSurfVerts : dfMaxSurfVerts;
}

bool isValidSurfaceRef(const Level& level, const SurfaceRef ref) noexcept
{
    if (ref.sector < 0 || ref.surface < 0)
    {
        return false;
    }

    const std::size_t sectorIndex = static_cast<std::size_t>(ref.sector);
    if (sectorIndex >= level.sectors.size())
    {
        return false;
    }

    const Sector& sector = level.sectors[sectorIndex];
    const std::size_t surfaceIndex = static_cast<std::size_t>(ref.surface);
    return surfaceIndex < sector.surfaces.size();
}

bool isValidSectorIndex(const Level& level, const std::int32_t sectorIndex) noexcept
{
    if (sectorIndex < 0)
    {
        return false;
    }

    return static_cast<std::size_t>(sectorIndex) < level.sectors.size();
}

using detail::CapturedCogReference;
using detail::captureCogReferences;

void setCogSectorReference(
    Level& level,
    const CapturedCogReference& reference,
    const std::int32_t sectorIndex)
{
    CogValue& value = level.cogs[reference.cogIndex].values[reference.valueIndex];
    if (!isValidSectorIndex(level, sectorIndex))
    {
        value.intValue = -1;
        value.stringValue = "-1";
        return;
    }
    value.intValue = sectorIndex;
    value.stringValue =
        std::to_string(level.sectors[static_cast<std::size_t>(sectorIndex)].num);
}

void setCogSurfaceReference(
    Level& level,
    const CapturedCogReference& reference,
    const std::optional<SurfaceRef> surface)
{
    CogValue& value = level.cogs[reference.cogIndex].values[reference.valueIndex];
    if (!surface || !isValidSurfaceRef(level, *surface))
    {
        value.intValue = -1;
        value.stringValue = "-1";
        return;
    }

    value.intValue = level.getGlobalSurfaceNumber(surface->sector, surface->surface);
    // Surface text is preferentially parsed by CogValue::asJedString(). Keep
    // it empty after a structural edit so the authoritative remapped global
    // index is used; rebuilding text from stale/duplicate legacy face nums can
    // silently retarget the value during the next chained merge.
    value.stringValue.clear();
}

struct SurfaceDeleteRemap
{
    std::int32_t sector{};
    std::int32_t keep{};
    std::int32_t deleted{};
};

void remapCogReferencesAfterSurfaceMerge(
    Level& level,
    const std::vector<CapturedCogReference>& references,
    const std::span<const SurfaceDeleteRemap> deletions)
{
    for (const CapturedCogReference& reference : references)
    {
        if (reference.type == CogType::sec)
        {
            setCogSectorReference(level, reference, reference.sector);
            continue;
        }

        SurfaceRef surface = reference.surface;
        for (const SurfaceDeleteRemap deletion : deletions)
        {
            if (surface.sector != deletion.sector)
            {
                continue;
            }
            if (surface.surface == deletion.deleted)
            {
                surface.surface = deletion.keep;
            }
            else if (surface.surface > deletion.deleted)
            {
                --surface.surface;
            }
        }
        setCogSurfaceReference(level, reference, surface);
    }
}

void remapCogReferencesAfterSectorMerge(
    Level& level,
    const std::vector<CapturedCogReference>& references,
    const std::int32_t keepSector,
    const std::int32_t deletedSector,
    const std::vector<std::optional<std::int32_t>>& keepSurfaceMap,
    const std::vector<std::optional<std::int32_t>>& deletedSurfaceMap)
{
    for (const CapturedCogReference& reference : references)
    {
        if (reference.type == CogType::sec)
        {
            std::int32_t sector = reference.sector;
            if (sector == deletedSector)
            {
                sector = keepSector;
            }
            else if (sector > deletedSector)
            {
                --sector;
            }
            setCogSectorReference(level, reference, sector);
            continue;
        }

        SurfaceRef surface = reference.surface;
        std::optional<SurfaceRef> mapped = surface;
        if (surface.sector == keepSector)
        {
            if (surface.surface < 0
                || static_cast<std::size_t>(surface.surface) >= keepSurfaceMap.size()
                || !keepSurfaceMap[static_cast<std::size_t>(surface.surface)])
            {
                mapped.reset();
            }
            else
            {
                mapped = SurfaceRef{ keepSector,
                    *keepSurfaceMap[static_cast<std::size_t>(surface.surface)] };
            }
        }
        else if (surface.sector == deletedSector)
        {
            if (surface.surface < 0
                || static_cast<std::size_t>(surface.surface) >= deletedSurfaceMap.size()
                || !deletedSurfaceMap[static_cast<std::size_t>(surface.surface)])
            {
                mapped.reset();
            }
            else
            {
                mapped = SurfaceRef{ keepSector,
                    *deletedSurfaceMap[static_cast<std::size_t>(surface.surface)] };
            }
        }
        else if (surface.sector > deletedSector)
        {
            --mapped->sector;
        }
        setCogSurfaceReference(level, reference, mapped);
    }
}

void insertVertexAt(Surface& surface, const std::size_t pos, const std::int32_t vertexIndex,
    const TexVertex& textureVertex)
{
    const std::size_t insertPos = std::min(pos, surface.vertices.size());
    surface.vertices.insert(surface.vertices.begin() + static_cast<std::ptrdiff_t>(insertPos), vertexIndex);

    const std::size_t insertTvPos = std::min(insertPos, surface.texVertices.size());
    surface.texVertices.insert(surface.texVertices.begin() + static_cast<std::ptrdiff_t>(insertTvPos), textureVertex);
}

void deleteVertexAt(Surface& surface, const std::int32_t pos) noexcept
{
    if (pos < 0 || static_cast<std::size_t>(pos) >= surface.vertices.size())
    {
        return;
    }

    const std::size_t erasePos = static_cast<std::size_t>(pos);
    surface.vertices.erase(surface.vertices.begin() + static_cast<std::ptrdiff_t>(erasePos));

    if (erasePos < surface.texVertices.size())
    {
        surface.texVertices.erase(surface.texVertices.begin() + static_cast<std::ptrdiff_t>(erasePos));
    }
}

bool findCommonEdge(const Surface& surf1, const Surface& surf2, std::int32_t& ed1, std::int32_t& ed2) noexcept
{
    const std::int32_t count1 = static_cast<std::int32_t>(surf1.vertices.size());
    const std::int32_t count2 = static_cast<std::int32_t>(surf2.vertices.size());
    if (count1 < 2 || count2 < 2)
    {
        return false;
    }

    for (std::int32_t i = 0; i < count1; ++i)
    {
        const std::int32_t v1 = surf1.vertices[static_cast<std::size_t>(i)];
        const auto it = std::find(surf2.vertices.begin(), surf2.vertices.end(), v1);
        if (it == surf2.vertices.end())
        {
            continue;
        }

        const std::int32_t j = static_cast<std::int32_t>(std::distance(surf2.vertices.begin(), it));
        if (surf1.vertices[static_cast<std::size_t>(nextIndex(i, count1))]
            == surf2.vertices[static_cast<std::size_t>(prevIndex(j, count2))])
        {
            ed1 = i;
            ed2 = prevIndex(j, count2);
            return true;
        }
    }

    return false;
}

bool surfaceIsPlanarWithNormal(
    const Sector& sector,
    const Surface& surface,
    const TVector& normal) noexcept
{
    const auto planeContext=precision::surfacePlaneError(sector,surface,normal);
    if (!planeContext.valid) return false;
    if (surface.vertices.size() < 3 || normal.lengthSqr() <= 1.0e-12)
    {
        return false;
    }
    const std::int32_t first = surface.vertices.front();
    if (first < 0 || static_cast<std::size_t>(first) >= sector.vertices.size())
    {
        return false;
    }
    const Vertex& origin = sector.vertices[static_cast<std::size_t>(first)];
    for (const std::int32_t index : surface.vertices)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size())
        {
            return false;
        }
        const Vertex& vertex = sector.vertices[static_cast<std::size_t>(index)];
        const TVector point = precision::position(vertex);
        const TVector anchor = precision::position(origin);
        const double distance = normal.dot(point - anchor);
        const double allowance=planeContext.at(normal,point,anchor);
        if (!std::isfinite(distance) || allowance<0.0 || std::abs(distance)>allowance)
        {
            return false;
        }
    }
    return true;
}

bool surfaceIsConvexWithNormal(
    const Sector& sector,
    const Surface& surface,
    const TVector& normal) noexcept
{
    const std::size_t count = surface.vertices.size();
    if (count < 3)
    {
        return false;
    }
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::int32_t i1 = surface.vertices[i];
        const std::int32_t i2 = surface.vertices[(i + 1) % count];
        const std::int32_t i3 = surface.vertices[(i + 2) % count];
        if (i1 < 0 || i2 < 0 || i3 < 0
            || static_cast<std::size_t>(i1) >= sector.vertices.size()
            || static_cast<std::size_t>(i2) >= sector.vertices.size()
            || static_cast<std::size_t>(i3) >= sector.vertices.size())
        {
            return false;
        }
        const Vertex& v1 = sector.vertices[static_cast<std::size_t>(i1)];
        const Vertex& v2 = sector.vertices[static_cast<std::size_t>(i2)];
        const Vertex& v3 = sector.vertices[static_cast<std::size_t>(i3)];
        const TVector a{ v3.x - v2.x, v3.y - v2.y, v3.z - v2.z };
        const TVector b{ v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
        const double turn = a.cross(b).dot(normal);
        if (!std::isfinite(turn) || turn < -precision::cornerTurnError(
                precision::position(v1), precision::position(v2), precision::position(v3), normal))
        {
            return false;
        }
    }
    return true;
}

bool surfacePairSharesValidPlaneEdge(
    const Level& level,
    const SurfaceRef first,
    const SurfaceRef second) noexcept
{
    if (!isValidSurfaceRef(level, first) || !isValidSurfaceRef(level, second)
        || first.sector != second.sector || first.surface == second.surface)
    {
        return false;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(first.sector)];
    const Surface& a = sector.surfaces[static_cast<std::size_t>(first.surface)];
    const Surface& b = sector.surfaces[static_cast<std::size_t>(second.surface)];
    std::int32_t firstEdge{};
    std::int32_t secondEdge{};
    if (!findCommonEdge(a, b, firstEdge, secondEdge))
    {
        return false;
    }

    const TVector firstNormal = surfaceCalcNormalNewell(sector, a).normalized();
    const TVector secondNormal = surfaceCalcNormalNewell(sector, b).normalized();
    // Both polygons are individually planar and the reversed common edge
    // contributes two shared points. Matching normals therefore also fixes
    // the plane offset; no separate arbitrary-point distance test is needed.
    return surfaceIsPlanarWithNormal(sector, a, firstNormal)
        && surfaceIsPlanarWithNormal(sector, b, secondNormal)
        && surfaceIsConvexWithNormal(sector, a, firstNormal)
        && surfaceIsConvexWithNormal(sector, b, secondNormal)
        && firstNormal.lengthSqr() > 1.0e-12
        && secondNormal.lengthSqr() > 1.0e-12
        && firstNormal.dot(secondNormal) >= 1.0 - 1.0e-4;
}

bool surfaceMergePairPlausible(
    const Level& level,
    const SurfaceRef first,
    const SurfaceRef second) noexcept
{
    if (!surfacePairSharesValidPlaneEdge(level, first, second))
    {
        return false;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(first.sector)];
    const Surface& a = sector.surfaces[static_cast<std::size_t>(first.surface)];
    const Surface& b = sector.surfaces[static_cast<std::size_t>(second.surface)];
    if (a.adjoin.has_value() != b.adjoin.has_value())
    {
        return false;
    }
    if (!a.adjoin)
    {
        return true;
    }

    // Portal faces are one atomic pair: both mirrors must be reciprocal and
    // form the same valid coplanar shared-edge merge on the other sector.
    // Otherwise merging only the selected side would silently destroy portal
    // connectivity when the donor face is deleted.
    if (!isValidSurfaceRef(level, *a.adjoin)
        || !isValidSurfaceRef(level, *b.adjoin)
        || a.adjoin->sector == first.sector
        || a.adjoin->sector != b.adjoin->sector
        || a.adjoin->surface == b.adjoin->surface
        || !surfacePairSharesValidPlaneEdge(level, *a.adjoin, *b.adjoin))
    {
        return false;
    }
    const Surface& mirrorA = level.sectors[static_cast<std::size_t>(a.adjoin->sector)]
        .surfaces[static_cast<std::size_t>(a.adjoin->surface)];
    const Surface& mirrorB = level.sectors[static_cast<std::size_t>(b.adjoin->sector)]
        .surfaces[static_cast<std::size_t>(b.adjoin->surface)];
    return mirrorA.adjoin && *mirrorA.adjoin == first
        && mirrorB.adjoin && *mirrorB.adjoin == second;
}

bool sectorsHaveMergeableAdjoin(
    const Level& level,
    const std::int32_t sectorA,
    const std::int32_t sectorB) noexcept
{
    if (!isValidSectorIndex(level, sectorA)
        || !isValidSectorIndex(level, sectorB)
        || sectorA == sectorB)
    {
        return false;
    }
    const Sector& first = level.sectors[static_cast<std::size_t>(sectorA)];
    const Sector& second = level.sectors[static_cast<std::size_t>(sectorB)];
    for (const Surface& surface : first.surfaces)
    {
        if (!surface.adjoin || surface.adjoin->sector != sectorB
            || !isValidSurfaceRef(level, *surface.adjoin))
        {
            continue;
        }
        const Surface& mate = second.surfaces[
            static_cast<std::size_t>(surface.adjoin->surface)];
        if (surfaceDoOverlap(first, surface, second, mate))
        {
            return true;
        }
    }
    return false;
}

bool sectorHasVolumeBoundary(const Sector& sector) noexcept
{
    // Low-level UV/reference operations also accept parallel surface-only
    // fixtures. Multiple boundary directions identify an actual volume edit.
    TVector first{};
    for ( const auto& surface : sector.surfaces )
    {
        const TVector normal = surfaceCalcNormalNewell(sector, surface);
        if ( first.lengthSqr() == 0.0 )
        {
            first = normal;
        }
        else if ( first.cross(normal).lengthSqr() > precision::kRoundoff * precision::kRoundoff )
        {
            return true;
        }
    }
    return false;
}

bool sectorGeometryIsValid(const Sector& sector)
{
    for (const Surface& surface : sector.surfaces)
    {
        const TVector normal = surfaceCalcNormalNewell(sector, surface).normalized();
        const auto planeContext=precision::surfacePlaneError(sector,surface,normal);
        if (!planeContext.valid) return false;
        if (!surfaceIsPlanarWithNormal(sector, surface, normal)
            || !surfaceIsConvexWithNormal(sector, surface, normal))
        {
            return false;
        }
        const Vertex& origin = sector.vertices[
            static_cast<std::size_t>(surface.vertices.front())];
        for (const Vertex& vertex : sector.vertices)
        {
            const TVector offset{
                vertex.x - origin.x, vertex.y - origin.y, vertex.z - origin.z };
            const double distance = normal.dot(offset);
            const double allowance=planeContext.at(normal,precision::position(vertex),precision::position(origin));
            if (!std::isfinite(distance) || allowance<0.0 || distance < -allowance)
            {
                return false;
            }
        }
    }
    return !sectorHasVolumeBoundary(sector) || precision::closedBoundary(sector);
}

struct MergeFaceProfile
{
    TVector normal;
    TVector anchor;
    double supportMinimum{};
};

struct MergeSourceProfile
{
    // Geometry only: no duplicate resource strings, UVs or gameplay metadata.
    Sector geometry;
    std::vector<MergeFaceProfile> faces;
};

bool mergeSourceNormalCachesAreValid(const Sector& source) noexcept
{
    for ( const Surface& face : source.surfaces )
    {
        if ( !precision::finite(face.normal) )
        {
            return false;
        }
        const double scale = std::max({std::abs(face.normal.x), std::abs(face.normal.y),
            std::abs(face.normal.z)});
        if ( scale == 0.0 )
        {
            // An unset normal is derived when needed; inspecting it must not
            // author an unrelated cache merely to prepare the transaction.
            continue;
        }
        const TVector cached{face.normal.x / scale, face.normal.y / scale, face.normal.z / scale};
        const TVector geometric = surfaceCalcNormalNewell(source, face);
        if ( !precision::finite(geometric) || cached.dot(geometric) <= 0.0 )
        {
            return false;
        }
    }
    return true;
}

std::optional<MergeSourceProfile> captureMergeSourceProfile(const Sector& source)
{
    MergeSourceProfile result;
    // Inspect the original cache before constructing geometry-only profiles:
    // resolving it first would hide non-finite or reversed authored normals.
    if ( !mergeSourceNormalCachesAreValid(source) )
    {
        return std::nullopt;
    }
    // Parallel surface-only callers historically rely on validation of the
    // surviving faces, even when a removed portal was not a solid half-space.
    // Give them no inherited-error exemptions and preserve that contract.
    if ( !sectorHasVolumeBoundary(source) )
    {
        return result;
    }
    result.geometry.vertices = source.vertices;
    result.geometry.surfaces.reserve(source.surfaces.size());
    result.faces.reserve(source.surfaces.size());
    for ( const Surface& face : source.surfaces )
    {
        Surface geometry;
        geometry.vertices = face.vertices;
        geometry.normal = surfaceCalcNormalNewell(source, face).normalized();
        result.geometry.surfaces.push_back(std::move(geometry));
    }
    // Only a closed, independently admitted solid may inherit its original
    // coordinate-rounding residuals instead of strict half-spaces.
    const bool strict = sectorGeometryIsValid(source);
    if ( !strict &&
         (!precision::closedBoundary(source) || !precision::sourceSectorIsConvex(result.geometry)) )
    {
        return std::nullopt;
    }
    for ( const Surface& face : result.geometry.surfaces )
    {
        if ( !strict && (!precision::sourceSurfaceIsPlanar(result.geometry, face) ||
                         !precision::sourceSurfaceIsConvex(result.geometry, face)) )
        {
            return std::nullopt;
        }
        MergeFaceProfile profile;
        profile.normal = face.normal;
        const auto arithmetic = precision::surfacePlaneError(result.geometry, face, profile.normal);
        if ( !arithmetic.valid || face.vertices.size() < 3 )
        {
            return std::nullopt;
        }
        profile.anchor = precision::position(result.geometry.vertices[face.vertices.front()]);
        for ( const Vertex& vertex : result.geometry.vertices )
        {
            const double distance = profile.normal.dot(precision::position(vertex) - profile.anchor);
            if ( !std::isfinite(distance) )
            {
                return std::nullopt;
            }
            profile.supportMinimum = std::min(profile.supportMinimum, distance);
        }
        result.faces.push_back(profile);
    }
    return result;
}

bool unchangedMergeFaceSupportsOutput(const Sector& output, const Surface& face,
                                      const MergeSourceProfile& source, const std::size_t index) noexcept
{
    const Surface& original = source.geometry.surfaces[index];
    if ( face.vertices.size() != original.vertices.size() )
    {
        return false;
    }
    // Exact ordered coordinates are the provenance certificate. Do not use
    // a geometric proximity budget to classify a newly altered face as old.
    for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
    {
        const auto vertex = face.vertices[corner];
        if ( vertex < 0 || static_cast<std::size_t>(vertex) >= output.vertices.size() ||
             precision::position(output.vertices[vertex]) !=
                 precision::position(source.geometry.vertices[original.vertices[corner]]) )
        {
            return false;
        }
    }
    const MergeFaceProfile& profile = source.faces[index];
    // SurfacePlaneError borrows its Sector and Surface. Construct it only
    // after the owning profile has reached its stable transaction storage.
    const auto arithmetic = precision::surfacePlaneError(source.geometry, original, profile.normal);
    if ( !arithmetic.valid )
    {
        return false;
    }
    for ( const Vertex& vertex : output.vertices )
    {
        const TVector point = precision::position(vertex);
        const double distance = profile.normal.dot(point - profile.anchor);
        const double error = arithmetic.at(profile.normal, point, profile.anchor);
        if ( !std::isfinite(distance) || !std::isfinite(error) || error < 0.0 )
        {
            return false;
        }
        const bool inherited =
            std::any_of(source.geometry.vertices.begin(), source.geometry.vertices.end(),
                        [&point](const Vertex& input) { return precision::position(input) == point; });
        // An original point and unchanged face retain their exact relationship;
        // the measured source minimum is an additional immutable bound. Foreign
        // points introduce new union constraints and receive roundoff only.
        if ( distance < (inherited ? profile.supportMinimum : 0.0) - error )
        {
            return false;
        }
    }
    return true;
}

bool sectorGeometryIsValidWithSources(const Sector& sector, const std::span<const MergeSourceProfile> sources,
                                      const std::span<const std::int32_t> strictFaces = {})
{
    for ( std::size_t index = 0; index < sector.surfaces.size(); ++index )
    {
        const Surface& face = sector.surfaces[index];
        bool inherited = false;
        if ( std::find(strictFaces.begin(), strictFaces.end(), static_cast<std::int32_t>(index)) ==
             strictFaces.end() )
        {
            for ( const MergeSourceProfile& source : sources )
            {
                for ( std::size_t original = 0; original < source.faces.size(); ++original )
                {
                    if ( unchangedMergeFaceSupportsOutput(sector, face, source, original) )
                    {
                        inherited = true;
                        break;
                    }
                }
                if ( inherited )
                {
                    break;
                }
            }
        }
        if ( inherited )
        {
            continue;
        }
        const TVector normal = surfaceCalcNormalNewell(sector, face).normalized();
        const auto arithmetic = precision::surfacePlaneError(sector, face, normal);
        if ( !arithmetic.valid || !surfaceIsPlanarWithNormal(sector, face, normal) ||
             !surfaceIsConvexWithNormal(sector, face, normal) )
        {
            return false;
        }
        const TVector anchor = precision::position(sector.vertices[face.vertices.front()]);
        for ( const Vertex& vertex : sector.vertices )
        {
            const TVector point = precision::position(vertex);
            const double distance = normal.dot(point - anchor);
            const double error = arithmetic.at(normal, point, anchor);
            if ( !std::isfinite(distance) || error < 0.0 || distance < -error )
            {
                return false;
            }
        }
    }
    return !sectorHasVolumeBoundary(sector) || precision::closedBoundary(sector);
}
bool affectedAdjoinsAreReciprocal(
    const Level& level,
    const std::span<const std::int32_t> affectedSectors) noexcept
{
    const auto affected = [affectedSectors](const std::int32_t sector) noexcept
    {
        return std::find(affectedSectors.begin(), affectedSectors.end(), sector)
            != affectedSectors.end();
    };
    for (std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex)
    {
        const Sector& sector = level.sectors[sectorIndex];
        for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
        {
            const Surface& surface = sector.surfaces[surfaceIndex];
            if (!surface.adjoin)
            {
                continue;
            }
            if (!affected(static_cast<std::int32_t>(sectorIndex))
                && !affected(surface.adjoin->sector))
            {
                continue;
            }
            if (!isValidSurfaceRef(level, *surface.adjoin))
            {
                return false;
            }
            const Surface& mate = level.sectors[static_cast<std::size_t>(surface.adjoin->sector)]
                .surfaces[static_cast<std::size_t>(surface.adjoin->surface)];
            if (!mate.adjoin
                || mate.adjoin->sector != static_cast<std::int32_t>(sectorIndex)
                || mate.adjoin->surface != static_cast<std::int32_t>(surfaceIndex)
                || !surfaceDoOverlap(sector, surface,
                    level.sectors[static_cast<std::size_t>(surface.adjoin->sector)], mate))
            {
                return false;
            }
        }
    }
    return true;
}

void compactSectorVertices(Sector& sector)
{
    std::vector<bool> used(sector.vertices.size(), false);
    for (const Surface& surface : sector.surfaces)
    {
        for (const std::int32_t vi : surface.vertices)
        {
            if (vi < 0 || static_cast<std::size_t>(vi) >= used.size())
            {
                continue;
            }
            used[static_cast<std::size_t>(vi)] = true;
        }
    }

    std::vector<std::int32_t> remap(sector.vertices.size(), -1);
    std::vector<Vertex> newVertices{};
    newVertices.reserve(sector.vertices.size());

    for (std::size_t i = 0; i < used.size(); ++i)
    {
        if (!used[i])
        {
            continue;
        }

        remap[i] = static_cast<std::int32_t>(newVertices.size());
        newVertices.push_back(sector.vertices[i]);
    }

    for (Surface& surface : sector.surfaces)
    {
        for (std::int32_t& vi : surface.vertices)
        {
            if (vi < 0 || static_cast<std::size_t>(vi) >= remap.size())
            {
                continue;
            }

            vi = remap[static_cast<std::size_t>(vi)];
        }
    }

    sector.vertices = std::move(newVertices);
}

void renumberSectorSurfaces(Sector& sector) noexcept
{
    for (std::size_t i = 0; i < sector.surfaces.size(); ++i)
    {
        sector.surfaces[i].num = static_cast<std::int32_t>(i);
    }
}

struct SurfaceUvProjection
{
    TVector origin{};
    TVector uGradient{};
    TVector vGradient{};
    float originU{};
    float originV{};
    std::vector<std::pair<std::int32_t, TexVertex>> authoredVertices{};
};

bool surfaceHasValidUvData(const Surface& surface) noexcept
{
    if (surface.texVertices.size() != surface.vertices.size()
        || !std::isfinite(surface.uScale)
        || !std::isfinite(surface.vScale))
    {
        return false;
    }
    return std::all_of(
        surface.texVertices.begin(),
        surface.texVertices.end(),
        [](const TexVertex& textureVertex) noexcept
        {
            return std::isfinite(textureVertex.u)
                && std::isfinite(textureVertex.v);
        });
}

bool sectorHasValidUvData(const Sector& sector) noexcept
{
    return std::all_of(
        sector.surfaces.begin(),
        sector.surfaces.end(),
        [](const Surface& surface) noexcept
        {
            return surfaceHasValidUvData(surface);
        });
}

/**
 * Capture the complete affine UV projection of one planar surface.
 *
 * surfaceCalcUVNormals() intentionally returns unit orientation axes for the
 * legacy RecalcAll path.  A merge must also preserve arbitrary authored texel
 * density, so using those unit axes can change (for example) a three-texel
 * span into the project's pixels-per-unit span.  These gradients directly
 * solve U and V per world unit from a non-collinear source triangle.  Exact
 * source-corner UVs are retained separately to avoid rounding existing data.
 */
std::optional<SurfaceUvProjection> captureSurfaceUvProjection(
    const Sector& sector,
    const Surface& surface)
{
    if (surface.vertices.size() < 3 || !surfaceHasValidUvData(surface))
    {
        return std::nullopt;
    }

    const auto validVertex = [&sector](const std::int32_t index) noexcept
    {
        return index >= 0
            && static_cast<std::size_t>(index) < sector.vertices.size();
    };
    const std::int32_t originIndex = surface.vertices.front();
    if (!validVertex(originIndex))
    {
        return std::nullopt;
    }

    const Vertex& originVertex = sector.vertices[static_cast<std::size_t>(originIndex)];
    const TVector origin{ originVertex.x, originVertex.y, originVertex.z };
    TVector basisA{};
    TVector basisB{};
    TVector basisArea{};
    double basisDeterminant = 0.0;
    std::size_t basisFirst = surface.vertices.size();
    std::size_t basisSecond = surface.vertices.size();

    // Stored UVs are floats, so a very short but valid leading edge can lose
    // its UV delta to rounding. Prefer the best-spread origin-fan triangle;
    // strict comparison keeps the first triangle on equal-area ties.
    for (std::size_t first = 1; first + 1 < surface.vertices.size(); ++first)
    {
        const std::int32_t firstIndex = surface.vertices[first];
        if (!validVertex(firstIndex))
        {
            continue;
        }
        const Vertex& firstVertex = sector.vertices[static_cast<std::size_t>(firstIndex)];
        const TVector a = TVector{ firstVertex.x, firstVertex.y, firstVertex.z } - origin;

        for (std::size_t second = first + 1; second < surface.vertices.size(); ++second)
        {
            const std::int32_t secondIndex = surface.vertices[second];
            if (!validVertex(secondIndex))
            {
                continue;
            }
            const Vertex& secondVertex = sector.vertices[static_cast<std::size_t>(secondIndex)];
            const TVector b = TVector{ secondVertex.x, secondVertex.y, secondVertex.z } - origin;
            const TVector area = a.cross(b);
            const double determinant = area.dot(area);
            if (!std::isfinite(determinant) || !(determinant > 0.0))
            {
                continue;
            }
            if ( determinant > basisDeterminant )
            {
                basisA = a;
                basisB = b;
                basisArea = area;
                basisDeterminant = determinant;
                basisFirst = first;
                basisSecond = second;
            }
        }
    }
    if ( basisFirst >= surface.vertices.size() )
    {
        return std::nullopt;
    }

    const TexVertex& originUv = surface.texVertices.front();
    const TexVertex& firstUv = surface.texVertices[basisFirst];
    const TexVertex& secondUv = surface.texVertices[basisSecond];
    const double duA = static_cast<double>(firstUv.u) - originUv.u;
    const double duB = static_cast<double>(secondUv.u) - originUv.u;
    const double dvA = static_cast<double>(firstUv.v) - originUv.v;
    const double dvB = static_cast<double>(secondUv.v) - originUv.v;

    SurfaceUvProjection projection{};
    projection.origin = origin;
    projection.originU = originUv.u;
    projection.originV = originUv.v;
    // Cross-product duals avoid cancellation between nearly equal squared
    // edge lengths on long, thin but valid polygons.
    projection.uGradient = basisB.cross(basisArea) * duA
        + basisArea.cross(basisA) * duB;
    projection.vGradient = basisB.cross(basisArea) * dvA
        + basisArea.cross(basisA) * dvB;
    projection.uGradient.x /= basisDeterminant;
    projection.uGradient.y /= basisDeterminant;
    projection.uGradient.z /= basisDeterminant;
    projection.vGradient.x /= basisDeterminant;
    projection.vGradient.y /= basisDeterminant;
    projection.vGradient.z /= basisDeterminant;
    if ( !std::isfinite(projection.uGradient.x)
        || !std::isfinite(projection.uGradient.y)
        || !std::isfinite(projection.uGradient.z)
        || !std::isfinite(projection.vGradient.x)
        || !std::isfinite(projection.vGradient.y)
        || !std::isfinite(projection.vGradient.z) )
    {
        return std::nullopt;
    }
    projection.authoredVertices.reserve(surface.vertices.size());
    for ( std::size_t index = 0; index < surface.vertices.size(); ++index )
    {
        projection.authoredVertices.emplace_back(
            surface.vertices[index], surface.texVertices[index]);
    }
    return projection;
}

bool applySurfaceUvProjection(
    const Sector& sector,
    Surface& surface,
    const SurfaceUvProjection& projection)
{
    if (surface.texVertices.size() != surface.vertices.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < surface.vertices.size(); ++index)
    {
        const std::int32_t vertexIndex = surface.vertices[index];
        const auto authored = std::find_if(
            projection.authoredVertices.begin(),
            projection.authoredVertices.end(),
            [vertexIndex](const auto& value) noexcept
            {
                return value.first == vertexIndex;
            });
        if (authored != projection.authoredVertices.end())
        {
            surface.texVertices[index] = authored->second;
            continue;
        }
        if (vertexIndex < 0
            || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
        {
            return false;
        }
        const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
        const TVector offset = TVector{ vertex.x, vertex.y, vertex.z } - projection.origin;
        const double projectedU = static_cast<double>(projection.originU)
            + projection.uGradient.Dot(offset);
        const double projectedV = static_cast<double>(projection.originV)
            + projection.vGradient.Dot(offset);
        constexpr double maxFloat =
            static_cast<double>(std::numeric_limits<float>::max());
        if (!std::isfinite(projectedU)
            || !std::isfinite(projectedV)
            || std::abs(projectedU) > maxFloat
            || std::abs(projectedV) > maxFloat)
        {
            return false;
        }
        surface.texVertices[index].u = static_cast<float>(projectedU);
        surface.texVertices[index].v = static_cast<float>(projectedV);
    }
    return true;
}

bool appendSurfaceVerticesAcrossSharedEdge(
    Surface& target,
    const Surface& donor,
    const std::int32_t targetEdge,
    const std::int32_t donorEdge)
{
    const std::int32_t donorCount = static_cast<std::int32_t>(donor.vertices.size());
    if (donorCount < 3)
    {
        return false;
    }

    // Step 1: insert the donor path just after the target shared edge.
    std::int32_t v = prevIndex(donorEdge, donorCount);
    const std::int32_t lastv = nextIndex(donorEdge, donorCount);
    const std::size_t insertPos =
        static_cast<std::size_t>(nextIndex(targetEdge, static_cast<std::int32_t>(target.vertices.size())));

    // Step 2: walk backward around the donor, skipping the two vertices that form the shared edge.
    while (v != lastv)
    {
        // Geometry adopts the donor corner's authored color/intensity. The
        // subsequent projection step intentionally reconciles only its UVs
        // to the active face, preserving this independent corner metadata.
        insertVertexAt(target, insertPos, donor.vertices[static_cast<std::size_t>(v)],
            donor.texVertices[static_cast<std::size_t>(v)]);
        v = prevIndex(v, donorCount);
    }

    return true;
}

std::int32_t removeFoldedDuplicateVertices(Surface& surface) noexcept
{
    std::int32_t removed = 0;
    const std::int32_t count = static_cast<std::int32_t>(surface.vertices.size());

    // Step 1: remove a vertex when its neighbors collapsed onto the same original vertex.
    for (std::int32_t i = count - 1; i >= 0; --i)
    {
        const std::int32_t currentCount = static_cast<std::int32_t>(surface.vertices.size());
        if (currentCount < 3 || i >= currentCount)
        {
            continue;
        }

        const std::int32_t prev = prevIndex(i, currentCount);
        const std::int32_t next = nextIndex(i, currentCount);

        if (surface.vertices[static_cast<std::size_t>(prev)] == surface.vertices[static_cast<std::size_t>(next)])
        {
            deleteVertexAt(surface, i);
            ++removed;
        }
    }

    return removed;
}

std::int32_t removeAdjacentDuplicateVertices(Surface& surface) noexcept
{
    std::int32_t removed = 0;
    const std::int32_t count = static_cast<std::int32_t>(surface.vertices.size());

    // Step 1: remove direct duplicate pairs left by repeated donor vertices.
    for (std::int32_t i = count - 1; i >= 0; --i)
    {
        const std::int32_t currentCount = static_cast<std::int32_t>(surface.vertices.size());
        if (currentCount < 2 || i >= currentCount)
        {
            continue;
        }

        const std::int32_t next = nextIndex(i, currentCount);

        if (surface.vertices[static_cast<std::size_t>(i)] == surface.vertices[static_cast<std::size_t>(next)])
        {
            deleteVertexAt(surface, i);
            ++removed;
        }
    }

    return removed;
}

void cleanupMergedSurfaceVertices(Surface& surface)
{
    // Step 1: repeat cleanup until folded and adjacent duplicate passes reach a fixed point.
    while (true)
    {
        const std::int32_t removed = removeFoldedDuplicateVertices(surface)
            + removeAdjacentDuplicateVertices(surface);
        if (removed == 0)
        {
            break;
        }
    }

}

bool replaceVertexReferenceAndCollapseSurface(
    Surface& surface,
    const std::int32_t keepVertex,
    const std::int32_t delVertex)
{
    bool changed = false;

    // Step 1: replace deleted vertex references from back to front so edge-collapse deletes are stable.
    for (std::int32_t vx = static_cast<std::int32_t>(surface.vertices.size()) - 1; vx >= 0; --vx)
    {
        if (surface.vertices[static_cast<std::size_t>(vx)] != delVertex)
        {
            continue;
        }

        surface.vertices[static_cast<std::size_t>(vx)] = keepVertex;
        changed = true;

        // Step 2: if replacement made the next edge zero-length, remove the replaced vertex.
        std::int32_t count = static_cast<std::int32_t>(surface.vertices.size());
        if (count >= 3)
        {
            const std::int32_t next = nextIndex(vx, count);
            if (surface.vertices[static_cast<std::size_t>(next)] == keepVertex)
            {
                deleteVertexAt(surface, vx);
            }
        }

        // Step 3: repeat against the previous edge because the first delete may have shifted indices.
        count = static_cast<std::int32_t>(surface.vertices.size());
        if (count >= 3 && vx >= 0 && vx < count)
        {
            const std::int32_t prev = prevIndex(vx, count);
            if (surface.vertices[static_cast<std::size_t>(prev)] == keepVertex)
            {
                deleteVertexAt(surface, vx);
            }
        }
    }

    return changed;
}

void rewriteSurfaceVertexIndicesAfterDelete(Sector& sector, const std::int32_t delVertex)
{
    // Step 1: after removing a sector vertex, shift every higher surface vertex index down by one.
    for (Surface& surface : sector.surfaces)
    {
        for (std::int32_t& vi : surface.vertices)
        {
            if (vi > delVertex)
            {
                vi -= 1;
            }
        }
    }
}

void rewriteSelectionVertexIndicesAfterDelete(
    std::vector<VertexRef>& selection,
    const std::int32_t sectorIndex,
    const std::int32_t keepVertex,
    const std::int32_t delVertex) noexcept
{
    // Step 1: keep the normalized selection usable for subsequent same-sector merge candidates.
    for (VertexRef& ref : selection)
    {
        if (ref.sector != sectorIndex)
        {
            continue;
        }

        if (ref.vertex == delVertex)
        {
            ref.vertex = keepVertex;
        }
        else if (ref.vertex > delVertex)
        {
            ref.vertex -= 1;
        }
    }
}

bool sysMergeSurfaces(
    Level& level,
    SurfaceRef surf1Ref,
    SurfaceRef surf2Ref,
    std::int32_t edge1,
    std::int32_t edge2,
    bool mergeMirroredAdjoin);

bool relinkMergedAdjoinSurfaces(
    Level& level,
    const SurfaceRef targetRef,
    Surface& target,
    Surface& donor)
{
    if (!target.adjoin || !donor.adjoin)
    {
        return true;
    }

    const SurfaceRef targetAdjoin = *target.adjoin;
    const SurfaceRef donorAdjoin = *donor.adjoin;

    if (!isValidSurfaceRef(level, targetAdjoin) || !isValidSurfaceRef(level, donorAdjoin))
    {
        return false;
    }

    Sector& targetAdjoinSector = level.sectors[static_cast<std::size_t>(targetAdjoin.sector)];
    Surface& targetAdjoinSurface =
        targetAdjoinSector.surfaces[static_cast<std::size_t>(targetAdjoin.surface)];

    Sector& donorAdjoinSector = level.sectors[static_cast<std::size_t>(donorAdjoin.sector)];
    Surface& donorAdjoinSurface =
        donorAdjoinSector.surfaces[static_cast<std::size_t>(donorAdjoin.surface)];

    std::int32_t targetAdjoinEdge{};
    std::int32_t donorAdjoinEdge{};

    // Step 2: when both mirror surfaces share an edge, merge them too so both sides stay symmetric.
    if (targetAdjoin.sector != donorAdjoin.sector
        || !findCommonEdge(targetAdjoinSurface, donorAdjoinSurface, targetAdjoinEdge, donorAdjoinEdge))
    {
        return false;
    }

    // Step 1: keep the surviving surface's adjoin flags/material metadata
    // consistent with the deleted one only after the mirrored pair preflight.
    surfaceCopyAdjoinParams(target, donor);

    SurfaceRef keep = targetAdjoin;
    SurfaceRef del = donorAdjoin;
    std::int32_t keepEdge = targetAdjoinEdge;
    std::int32_t delEdge = donorAdjoinEdge;

    if (keep.surface > del.surface)
    {
        std::swap(keep, del);
        std::swap(keepEdge, delEdge);
    }

    // Step 3: after the mirror merge, recreate the adjoin link only if the two resulting surfaces overlap.
    if (!sysMergeSurfaces(level, keep, del, keepEdge, delEdge, false))
    {
        return false;
    }

    if (!isValidSurfaceRef(level, targetRef) || !isValidSurfaceRef(level, keep))
    {
        return false;
    }

    const Sector& secA = level.sectors[static_cast<std::size_t>(targetRef.sector)];
    const Surface& surfA = secA.surfaces[static_cast<std::size_t>(targetRef.surface)];

    const Sector& secB = level.sectors[static_cast<std::size_t>(keep.sector)];
    const Surface& surfB = secB.surfaces[static_cast<std::size_t>(keep.surface)];

    return surfaceDoOverlap(secA, surfA, secB, surfB)
        && levelSysAdjoinSurfaces(level, targetRef, keep, false);
}

bool sysMergeSurfaces(
    Level& level,
    const SurfaceRef surf1Ref,
    const SurfaceRef surf2Ref,
    const std::int32_t edge1,
    const std::int32_t edge2,
    const bool mergeMirroredAdjoin)
{
    if (!isValidSurfaceRef(level, surf1Ref) || !isValidSurfaceRef(level, surf2Ref))
    {
        return false;
    }

    if (surf1Ref.sector != surf2Ref.sector)
    {
        return false;
    }

    Sector& sector = level.sectors[static_cast<std::size_t>(surf1Ref.sector)];
    if (surf1Ref.surface == surf2Ref.surface)
    {
        return false;
    }

    if (surf1Ref.surface < 0 || surf2Ref.surface < 0)
    {
        return false;
    }

    const std::size_t surf1Index = static_cast<std::size_t>(surf1Ref.surface);
    const std::size_t surf2Index = static_cast<std::size_t>(surf2Ref.surface);
    if (surf1Index >= sector.surfaces.size() || surf2Index >= sector.surfaces.size())
    {
        return false;
    }

    Surface& surf1 = sector.surfaces[surf1Index];
    Surface& surf2 = sector.surfaces[surf2Index];

    if (surf1Ref.surface > surf2Ref.surface)
    {
        return false;
    }

    if (edge1 < 0 || static_cast<std::size_t>(edge1) >= surf1.vertices.size() || edge2 < 0
        || static_cast<std::size_t>(edge2) >= surf2.vertices.size())
    {
        return false;
    }

    if (surf1.adjoin.has_value() != surf2.adjoin.has_value())
    {
        return false;
    }

    // Step 1: malformed texture data is not repairable here. Reject it before
    // topology edits so no missing corner is silently replaced with zero UVs.
    if (!surfaceHasValidUvData(surf1) || !surfaceHasValidUvData(surf2))
    {
        return false;
    }

    // The lower-index survivor is the deterministic projection owner. Capture
    // its complete affine mapping before donor vertices are inserted.
    const std::optional<SurfaceUvProjection> preservedUvProjection =
        captureSurfaceUvProjection(sector, surf1);
    if (!preservedUvProjection)
    {
        return false;
    }

    // Step 2: splice the donor polygon path into the target around the shared edge.
    if (!appendSurfaceVerticesAcrossSharedEdge(surf1, surf2, edge1, edge2))
    {
        return false;
    }

    // Step 3: remove duplicate vertices introduced by the splice and
    // recalculate geometry while retaining the surviving UV frame.
    cleanupMergedSurfaceVertices(surf1);
    // Count the actual post-cleanup polygon. The old exclusive-vertex estimate
    // omitted the two shared edge endpoints and allowed max+2 output.
    if (surf1.vertices.size()
        > static_cast<std::size_t>(maxSurfaceVertices(level)))
    {
        return false;
    }
    surf1.normal = surfaceCalcNormalNewell(sector, surf1);
    if (!applySurfaceUvProjection(sector, surf1, *preservedUvProjection))
    {
        return false;
    }

    // Step 4: if the merged surfaces were portals, merge/relink the mirrored
    // side as one atomic operation. The mirrored call suppresses recursion.
    if (mergeMirroredAdjoin
        && !relinkMergedAdjoinSurfaces(level, surf1Ref, surf1, surf2))
    {
        return false;
    }

    // Step 5: delete the donor surface and compact unused vertices left behind in the sector.
    if (!levelDeleteSurface(level, surf2Ref))
    {
        return false;
    }

    Sector& finalSector = level.sectors[static_cast<std::size_t>(surf1Ref.sector)];
    compactSectorVertices(finalSector);
    renumberSectorSurfaces(finalSector);

    return true;
}

} // namespace

static std::optional<SurfaceRef> mergeSurfacesCheckedOwnedCandidate(
    Level& candidate,
    const SurfaceRef surf1,
    const SurfaceRef surf2)
{
    if (!surfaceMergePairPlausible(candidate, surf1, surf2))
    {
        return std::nullopt;
    }

    SurfaceRef keep = surf1;
    SurfaceRef del = surf2;
    if (keep.surface > del.surface)
    {
        std::swap(keep, del);
    }

    // Remember every sector whose geometry the mirrored portal merge may
    // touch.  Those sectors are validated together before publication.
    std::vector<std::int32_t> affectedSectors{ keep.sector };
    const Sector& sourceSector = candidate.sectors[static_cast<std::size_t>(keep.sector)];
    const Surface& sourceKeep =
        sourceSector.surfaces[static_cast<std::size_t>(keep.surface)];
    const Surface& sourceDel =
        sourceSector.surfaces[static_cast<std::size_t>(del.surface)];
    const std::vector<CapturedCogReference> cogReferences =
        captureCogReferences(candidate);
    std::vector<SurfaceDeleteRemap> cogSurfaceDeletions{};
    if (sourceKeep.adjoin)
    {
        affectedSectors.push_back(sourceKeep.adjoin->sector);
    }
    if (sourceDel.adjoin)
    {
        affectedSectors.push_back(sourceDel.adjoin->sector);
    }
    if (sourceKeep.adjoin && sourceDel.adjoin)
    {
        cogSurfaceDeletions.push_back(SurfaceDeleteRemap{
            sourceKeep.adjoin->sector,
            std::min(sourceKeep.adjoin->surface, sourceDel.adjoin->surface),
            std::max(sourceKeep.adjoin->surface, sourceDel.adjoin->surface) });
    }
    cogSurfaceDeletions.push_back(SurfaceDeleteRemap{
        keep.sector, keep.surface, del.surface });
    std::sort(affectedSectors.begin(), affectedSectors.end());
    affectedSectors.erase(
        std::unique(affectedSectors.begin(), affectedSectors.end()),
        affectedSectors.end());

    std::vector<MergeSourceProfile> sourceProfiles;
    sourceProfiles.reserve(affectedSectors.size());
    for ( const auto owner : affectedSectors )
    {
        auto profile = captureMergeSourceProfile(candidate.sectors[static_cast<std::size_t>(owner)]);
        if ( !profile )
        {
            return std::nullopt;
        }
        sourceProfiles.push_back(std::move(*profile));
    }
    std::vector<SurfaceRef> combinedFaces{keep};
    if ( sourceKeep.adjoin && sourceDel.adjoin )
    {
        combinedFaces.push_back({sourceKeep.adjoin->sector,
            std::min(sourceKeep.adjoin->surface, sourceDel.adjoin->surface)});
    }

    Sector& sector = candidate.sectors[static_cast<std::size_t>(keep.sector)];
    const Surface& a = sector.surfaces[static_cast<std::size_t>(keep.surface)];
    const Surface& b = sector.surfaces[static_cast<std::size_t>(del.surface)];

    std::int32_t edge1{};
    std::int32_t edge2{};
    if (!findCommonEdge(a, b, edge1, edge2))
    {
        return std::nullopt;
    }

    if (!sysMergeSurfaces(candidate, keep, del, edge1, edge2, true))
    {
        return std::nullopt;
    }

    for ( const std::int32_t sectorIndex : affectedSectors )
    {
        std::vector<std::int32_t> strictFaces;
        for ( const SurfaceRef face : combinedFaces )
        {
            if ( face.sector == sectorIndex )
            {
                strictFaces.push_back(face.surface);
            }
        }
        if ( !isValidSectorIndex(candidate, sectorIndex)
            || !sectorGeometryIsValidWithSources(candidate.sectors[static_cast<std::size_t>(sectorIndex)],
                sourceProfiles, strictFaces) )
        {
            return std::nullopt;
        }
    }
    if (!affectedAdjoinsAreReciprocal(candidate, affectedSectors))
    {
        return std::nullopt;
    }

    remapCogReferencesAfterSurfaceMerge(
        candidate, cogReferences, cogSurfaceDeletions);

    return keep;
}

std::optional<SurfaceRef> levelMergeSurfaces(
    Level& level,
    const SurfaceRef surf1,
    const SurfaceRef surf2)
{
    // Reject impossible pairs before copying the Level. A shared edge plus
    // matching planes is required; merely touching at one vertex, bending
    // across the edge, or mixing portal/non-portal faces cannot form one face.
    if (!isValidUvCoordinateSpace(level.uvCoordinateSpace)
        || !surfaceMergePairPlausible(level, surf1, surf2))
    {
        return std::nullopt;
    }

    Level candidate = level;
    const std::optional<SurfaceRef> merged =
        mergeSurfacesCheckedOwnedCandidate(candidate, surf1, surf2);
    if (!merged)
    {
        return std::nullopt;
    }

    level = std::move(candidate);
    return merged;
}

std::optional<SurfaceMergeBatchResult> levelMergeSelectedSurfaces(
    Level& level,
    const std::span<const SurfaceRef> surfaces)
{
    if (!isValidUvCoordinateSpace(level.uvCoordinateSpace))
    {
        return std::nullopt;
    }

    std::vector<SurfaceRef> selection(surfaces.begin(), surfaces.end());
    std::sort(selection.begin(), selection.end(), [](const SurfaceRef& a, const SurfaceRef& b) noexcept
        {
            return a.sector != b.sector ? a.sector < b.sector : a.surface < b.surface;
        });
    selection.erase(std::unique(selection.begin(), selection.end()), selection.end());
    selection.erase(std::remove_if(selection.begin(), selection.end(), [&level](const SurfaceRef ref) noexcept
        {
            return !isValidSurfaceRef(level, ref);
        }), selection.end());

    if (selection.size() < 2)
    {
        return std::nullopt;
    }

    Level candidate = level;
    std::vector<SurfaceRef> current = selection;
    std::size_t mergeCount = 0;

    const auto sortUnique = [](std::vector<SurfaceRef>& refs)
    {
        std::sort(refs.begin(), refs.end(), [](const SurfaceRef& a, const SurfaceRef& b) noexcept
            {
                return a.sector != b.sector ? a.sector < b.sector : a.surface < b.surface;
            });
        refs.erase(std::unique(refs.begin(), refs.end()), refs.end());
    };
    const auto remapDeletedSurface = [](std::vector<SurfaceRef>& refs,
        const std::int32_t sectorIndex,
        const std::int32_t keptSurface,
        const std::int32_t deletedSurface)
    {
        for (SurfaceRef& ref : refs)
        {
            if (ref.sector != sectorIndex)
            {
                continue;
            }
            if (ref.surface == deletedSurface)
            {
                ref.surface = keptSurface;
            }
            else if (ref.surface > deletedSurface)
            {
                --ref.surface;
            }
        }
    };

    // Restart the lexicographic scan after every accepted merge.  The new
    // polygon is therefore reconsidered against the remaining selection and
    // chains collapse to a fixed point. Invalid candidates are simply skipped.
    bool mergedInPass = true;
    while (mergedInPass)
    {
        mergedInPass = false;
        sortUnique(current);
        for (std::size_t i = 0; i < current.size() && !mergedInPass; ++i)
        {
            for (std::size_t j = i + 1; j < current.size(); ++j)
            {
                const SurfaceRef first = current[i];
                const SurfaceRef second = current[j];
                if (second.sector != first.sector)
                {
                    if (second.sector > first.sector)
                    {
                        break;
                    }
                    continue;
                }
                if (!surfaceMergePairPlausible(candidate, first, second))
                {
                    continue;
                }

                std::optional<SurfaceRef> mirrorFirst{};
                std::optional<SurfaceRef> mirrorSecond{};
                const Sector& beforeSector =
                    candidate.sectors[static_cast<std::size_t>(first.sector)];
                mirrorFirst = beforeSector.surfaces[static_cast<std::size_t>(first.surface)].adjoin;
                mirrorSecond = beforeSector.surfaces[static_cast<std::size_t>(second.surface)].adjoin;

                Level trial = candidate;
                const std::optional<SurfaceRef> merged =
                    mergeSurfacesCheckedOwnedCandidate(trial, first, second);
                if (!merged)
                {
                    continue;
                }

                const std::int32_t deleted =
                    std::max(first.surface, second.surface);
                remapDeletedSurface(
                    current, merged->sector, merged->surface, deleted);

                // Portal surface merge may also delete the higher-index
                // mirrored face. Remap any selected refs in that sector too.
                if (mirrorFirst && mirrorSecond
                    && mirrorFirst->sector == mirrorSecond->sector
                    && mirrorFirst->surface != mirrorSecond->surface
                    && isValidSectorIndex(candidate, mirrorFirst->sector)
                    && isValidSectorIndex(trial, mirrorFirst->sector))
                {
                    const std::size_t beforeCount = candidate.sectors[
                        static_cast<std::size_t>(mirrorFirst->sector)].surfaces.size();
                    const std::size_t afterCount = trial.sectors[
                        static_cast<std::size_t>(mirrorFirst->sector)].surfaces.size();
                    if (afterCount + 1 == beforeCount)
                    {
                        remapDeletedSurface(
                            current,
                            mirrorFirst->sector,
                            std::min(mirrorFirst->surface, mirrorSecond->surface),
                            std::max(mirrorFirst->surface, mirrorSecond->surface));
                    }
                }

                candidate = std::move(trial);
                ++mergeCount;
                mergedInPass = true;
                break;
            }
        }
    }

    std::sort(current.begin(), current.end(), [](const SurfaceRef& a, const SurfaceRef& b) noexcept
        {
            return a.sector != b.sector ? a.sector < b.sector : a.surface < b.surface;
        });
    current.erase(std::unique(current.begin(), current.end()), current.end());

    if (mergeCount == 0)
    {
        return std::nullopt;
    }

    level = std::move(candidate);
    return SurfaceMergeBatchResult{ std::move(current), mergeCount };
}

bool levelMergeSurfacesAt(Level& level, const SurfaceRef surfRef, const std::int32_t edge)
{
    // Step 1: validate the selected surface and edge before searching neighboring polygons.
    if (!isValidUvCoordinateSpace(level.uvCoordinateSpace)
        || !isValidSurfaceRef(level, surfRef))
    {
        return false;
    }

    Sector& sector = level.sectors[static_cast<std::size_t>(surfRef.sector)];
    const std::size_t surfaceIndex = static_cast<std::size_t>(surfRef.surface);
    if (surfaceIndex >= sector.surfaces.size())
    {
        return false;
    }

    const Surface& surf = sector.surfaces[surfaceIndex];
    const std::int32_t count = static_cast<std::int32_t>(surf.vertices.size());
    if (count < 3 || edge < 0 || edge >= count)
    {
        return false;
    }

    const std::int32_t v1 = surf.vertices[static_cast<std::size_t>(edge)];
    const std::int32_t v2 = surf.vertices[static_cast<std::size_t>(nextIndex(edge, count))];

    std::optional<std::pair<std::int32_t, std::int32_t>> other{};

    // Step 2: search for exactly one other surface that uses the selected edge in reverse order.
    for (std::int32_t sf = 0; sf < static_cast<std::int32_t>(sector.surfaces.size()); ++sf)
    {
        if (sf == surfRef.surface)
        {
            continue;
        }

        const Surface& asf = sector.surfaces[static_cast<std::size_t>(sf)];
        const std::int32_t ac = static_cast<std::int32_t>(asf.vertices.size());
        for (std::int32_t v = 0; v < ac; ++v)
        {
            const std::int32_t av1 = asf.vertices[static_cast<std::size_t>(v)];
            const std::int32_t av2 = asf.vertices[static_cast<std::size_t>(nextIndex(v, ac))];
            if (v1 != av2 || v2 != av1)
            {
                continue;
            }

            if (other)
            {
                return false;
            }

            other = std::make_pair(sf, v);
        }
    }

    if (!other)
    {
        return false;
    }

    SurfaceRef keep = surfRef;
    SurfaceRef del = SurfaceRef{ surfRef.sector, other->first };

    std::int32_t edge1 = edge;
    std::int32_t edge2 = other->second;

    // Step 3: keep the lower surface index stable because deleting the donor shifts later indices.
    if (keep.surface > del.surface)
    {
        std::swap(keep, del);
        std::swap(edge1, edge2);
    }

    // Keep the unique-edge semantics above, but publish through the checked
    // transactional pair path so bent/concave or adjoin-unsafe output cannot
    // leak through this legacy entry point.
    return levelMergeSurfaces(level, keep, del).has_value();
}

static std::optional<std::int32_t> mergeSectorsUnchecked(
    Level& level,
    std::int32_t sectorA,
    std::int32_t sectorB)
{
    // Step 1: normalize the request so the lower sector index is kept and deleted indices shift predictably.
    if (!isValidSectorIndex(level, sectorA) || !isValidSectorIndex(level, sectorB))
    {
        return std::nullopt;
    }

    if (sectorA == sectorB)
    {
        return std::nullopt;
    }

    std::int32_t keepIndex = sectorA;
    std::int32_t delIndex = sectorB;

    if (keepIndex > delIndex)
    {
        std::swap(keepIndex, delIndex);
    }

    Sector& keep = level.sectors[static_cast<std::size_t>(keepIndex)];
    const Sector& del = level.sectors[static_cast<std::size_t>(delIndex)];
    const std::vector<CapturedCogReference> cogReferences =
        captureCogReferences(level);

    std::vector<std::int32_t> deleteKeepSurfaces{};
    std::vector<std::int32_t> deleteDelSurfaces{};

    // Step 2: find overlapping portal pairs between the two sectors; these become internal walls to remove.
    for (std::int32_t sf = 0; sf < static_cast<std::int32_t>(keep.surfaces.size()); ++sf)
    {
        const Surface& surf = keep.surfaces[static_cast<std::size_t>(sf)];
        if (!surf.adjoin)
        {
            continue;
        }
        if (surf.adjoin->sector != delIndex)
        {
            continue;
        }

        const SurfaceRef otherRef = *surf.adjoin;
        if (!isValidSurfaceRef(level, otherRef))
        {
            continue;
        }

        const Surface& other = del.surfaces[static_cast<std::size_t>(otherRef.surface)];
        if (!surfaceDoOverlap(keep, surf, del, other))
        {
            continue;
        }

        deleteKeepSurfaces.push_back(sf);
        deleteDelSurfaces.push_back(otherRef.surface);
    }

    if (deleteKeepSurfaces.empty())
    {
        return std::nullopt;
    }

    // Step 3: de-duplicate internal wall lists before deleting from the back to keep indices valid.
    std::sort(deleteKeepSurfaces.begin(), deleteKeepSurfaces.end());
    deleteKeepSurfaces.erase(std::unique(deleteKeepSurfaces.begin(), deleteKeepSurfaces.end()), deleteKeepSurfaces.end());

    std::sort(deleteDelSurfaces.begin(), deleteDelSurfaces.end());
    deleteDelSurfaces.erase(std::unique(deleteDelSurfaces.begin(), deleteDelSurfaces.end()), deleteDelSurfaces.end());

    std::vector<std::optional<std::int32_t>> keepToKeep(
        keep.surfaces.size());
    std::int32_t nextKeepSurface = 0;
    for (std::size_t sf = 0; sf < keep.surfaces.size(); ++sf)
    {
        if (!std::binary_search(
                deleteKeepSurfaces.begin(), deleteKeepSurfaces.end(),
                static_cast<std::int32_t>(sf)))
        {
            keepToKeep[sf] = nextKeepSurface++;
        }
    }

    for (auto it = deleteKeepSurfaces.rbegin(); it != deleteKeepSurfaces.rend(); ++it)
    {
        (void)levelDeleteSurface(level, SurfaceRef{ keepIndex, *it });
    }

    if (!isValidSectorIndex(level, keepIndex) || !isValidSectorIndex(level, delIndex))
    {
        return std::nullopt;
    }

    Sector& keepAfter = level.sectors[static_cast<std::size_t>(keepIndex)];
    const Sector& delAfter = level.sectors[static_cast<std::size_t>(delIndex)];

    // Step 4: mark the deleted sector surfaces that were internal portals and should not be moved.
    std::vector<bool> skipDel(delAfter.surfaces.size(), false);
    for (const std::int32_t sf : deleteDelSurfaces)
    {
        if (sf < 0 || static_cast<std::size_t>(sf) >= skipDel.size())
        {
            continue;
        }
        skipDel[static_cast<std::size_t>(sf)] = true;
    }

    std::vector<std::optional<std::int32_t>> delToKeep(delAfter.surfaces.size());

    // Step 5: move all remaining deleted-sector surfaces into the kept sector with remapped vertices.
    for (std::int32_t sf = 0; sf < static_cast<std::int32_t>(delAfter.surfaces.size()); ++sf)
    {
        if (sf < 0 || static_cast<std::size_t>(sf) >= skipDel.size() || skipDel[static_cast<std::size_t>(sf)])
        {
            continue;
        }

        Surface moved = delAfter.surfaces[static_cast<std::size_t>(sf)];

        for (std::int32_t& vi : moved.vertices)
        {
            if (vi < 0 || static_cast<std::size_t>(vi) >= delAfter.vertices.size())
            {
                return std::nullopt;
            }

            const Vertex& v = delAfter.vertices[static_cast<std::size_t>(vi)];
            vi = precision::addVertex(keepAfter, v.x, v.y, v.z);
        }

        const std::int32_t newIndex = static_cast<std::int32_t>(keepAfter.surfaces.size());
        moved.num = newIndex;
        keepAfter.surfaces.push_back(std::move(moved));

        delToKeep[static_cast<std::size_t>(sf)] = newIndex;
    }

    renumberSectorSurfaces(keepAfter);

    // Step 6: rewrite every adjoin that pointed at the deleted sector to its new kept-sector surface.
    for (Sector& sector : level.sectors)
    {
        for (Surface& surface : sector.surfaces)
        {
            if (!surface.adjoin)
            {
                continue;
            }

            if (surface.adjoin->sector != delIndex)
            {
                continue;
            }

            const std::int32_t oldSf = surface.adjoin->surface;
            if (oldSf < 0 || static_cast<std::size_t>(oldSf) >= delToKeep.size() || !delToKeep[static_cast<std::size_t>(oldSf)])
            {
                surface.adjoin.reset();
                surface.nAdj = -1;
                surfaceClearAdjoinParams(surface);
                continue;
            }

            surface.adjoin->sector = keepIndex;
            surface.adjoin->surface = *delToKeep[static_cast<std::size_t>(oldSf)];
        }
    }

    // Step 7: Things in the donor now belong to the merged survivor. Do this
    // before levelDeleteSector, whose generic delete behavior intentionally
    // clears direct references to the deleted sector.
    for (Thing& thing : level.things)
    {
        if (thing.sector == delIndex)
        {
            thing.sector = keepIndex;
        }
    }

    // Step 8: remove the old sector and let the lower-level delete routine
    // shift references that point beyond the deleted index.
    if (!levelDeleteSector(level, delIndex))
    {
        return std::nullopt;
    }

    // Rebuild semantic COG pointers from their pre-merge references. This
    // covers survivor deletions, donor surfaces moved into the survivor, and
    // global-number/index shifts in every later sector. Deleted internal
    // portal faces are cleared rather than silently retargeted.
    remapCogReferencesAfterSectorMerge(
        level, cogReferences, keepIndex, delIndex, keepToKeep, delToKeep);

    return keepIndex;
}

static std::optional<std::int32_t> mergeSectorsCheckedOwnedCandidate(
    Level& candidate,
    const std::int32_t sectorA,
    const std::int32_t sectorB)
{
    if (!sectorsHaveMergeableAdjoin(candidate, sectorA, sectorB))
    {
        return std::nullopt;
    }
    const bool requireClosed = sectorHasVolumeBoundary(candidate.sectors[static_cast<std::size_t>(sectorA)])
        || sectorHasVolumeBoundary(candidate.sectors[static_cast<std::size_t>(sectorB)]);
    if ( requireClosed && (!precision::closedBoundary(candidate.sectors[static_cast<std::size_t>(sectorA)])
            || !precision::closedBoundary(candidate.sectors[static_cast<std::size_t>(sectorB)])) )
    {
        return std::nullopt;
    }
    if (!sectorHasValidUvData(
            candidate.sectors[static_cast<std::size_t>(sectorA)])
        || !sectorHasValidUvData(
            candidate.sectors[static_cast<std::size_t>(sectorB)]))
    {
        return std::nullopt;
    }
    std::vector<MergeSourceProfile> sourceProfiles;
    sourceProfiles.reserve(2);
    for ( const auto owner : {sectorA, sectorB} )
    {
        auto profile = captureMergeSourceProfile(candidate.sectors[static_cast<std::size_t>(owner)]);
        if ( !profile )
        {
            return std::nullopt;
        }
        sourceProfiles.push_back(std::move(*profile));
    }
    const std::optional<std::int32_t> merged =
        mergeSectorsUnchecked(candidate, sectorA, sectorB);
    if (!merged || !isValidSectorIndex(candidate, *merged)
        || !sectorGeometryIsValidWithSources(
            candidate.sectors[static_cast<std::size_t>(*merged)], sourceProfiles)
        || (requireClosed && !precision::closedBoundary(candidate.sectors[static_cast<std::size_t>(*merged)])))
    {
        return std::nullopt;
    }

    const std::array<std::int32_t, 1> affected{ *merged };
    if (!affectedAdjoinsAreReciprocal(candidate, affected))
    {
        return std::nullopt;
    }

    return merged;
}

std::optional<std::int32_t> levelMergeSectors(
    Level& level,
    const std::int32_t sectorA,
    const std::int32_t sectorB)
{
    if (!isValidUvCoordinateSpace(level.uvCoordinateSpace)
        || !sectorsHaveMergeableAdjoin(level, sectorA, sectorB))
    {
        return std::nullopt;
    }

    Level candidate = level;
    const std::optional<std::int32_t> merged =
        mergeSectorsCheckedOwnedCandidate(candidate, sectorA, sectorB);
    if (!merged)
    {
        return std::nullopt;
    }

    level = std::move(candidate);
    return merged;
}

std::optional<SectorMergeBatchResult> levelMergeSelectedSectors(
    Level& level,
    const std::span<const std::int32_t> sectors)
{
    if (!isValidUvCoordinateSpace(level.uvCoordinateSpace))
    {
        return std::nullopt;
    }

    std::vector<std::int32_t> current(sectors.begin(), sectors.end());
    std::sort(current.begin(), current.end());
    current.erase(std::unique(current.begin(), current.end()), current.end());
    current.erase(std::remove_if(current.begin(), current.end(), [&level](const std::int32_t sector) noexcept
        {
            return !isValidSectorIndex(level, sector);
        }), current.end());
    if (current.size() < 2)
    {
        return std::nullopt;
    }

    Level candidate = level;
    std::size_t mergeCount = 0;
    bool mergedInPass = true;
    while (mergedInPass)
    {
        mergedInPass = false;
        std::sort(current.begin(), current.end());
        current.erase(std::unique(current.begin(), current.end()), current.end());
        for (std::size_t i = 0; i < current.size() && !mergedInPass; ++i)
        {
            for (std::size_t j = i + 1; j < current.size(); ++j)
            {
                const std::int32_t first = current[i];
                const std::int32_t second = current[j];

                if (!sectorsHaveMergeableAdjoin(candidate, first, second))
                {
                    continue;
                }

                // The checked helper performs the authoritative topology and
                // geometry validation on this private trial. Failed pairs do
                // not poison other independent/chain candidates in the batch.
                Level trial = candidate;
                const std::optional<std::int32_t> merged =
                    mergeSectorsCheckedOwnedCandidate(trial, first, second);
                if (!merged)
                {
                    continue;
                }

                const std::int32_t deleted = std::max(first, second);
                for (std::int32_t& selected : current)
                {
                    if (selected == deleted)
                    {
                        selected = *merged;
                    }
                    else if (selected > deleted)
                    {
                        --selected;
                    }
                }

                candidate = std::move(trial);
                ++mergeCount;
                mergedInPass = true;
                break;
            }
        }
    }

    std::sort(current.begin(), current.end());
    current.erase(std::unique(current.begin(), current.end()), current.end());
    if (mergeCount == 0)
    {
        return std::nullopt;
    }

    level = std::move(candidate);
    return SectorMergeBatchResult{ std::move(current), mergeCount };
}

std::int32_t levelMergeVertices(Level& level, const std::span<const VertexRef> vertexSelection)
{
    // Step 1: sort and de-duplicate the incoming selection so pair scans are deterministic.
    if (!isValidUvCoordinateSpace(level.uvCoordinateSpace))
    {
        return 0;
    }

    std::vector<VertexRef> selection(vertexSelection.begin(), vertexSelection.end());
    std::sort(selection.begin(), selection.end(), [](const VertexRef& a, const VertexRef& b) noexcept {
        if (a.sector != b.sector)
        {
            return a.sector < b.sector;
        }
        return a.vertex < b.vertex;
    });
    selection.erase(std::unique(selection.begin(), selection.end(), [](const VertexRef& a, const VertexRef& b) noexcept {
        return a.sector == b.sector && a.vertex == b.vertex;
    }), selection.end());

    // Preflight every selected sector before mutating any of them. Vertex
    // merging preserves authored UVs and may remove a paired corner, but it
    // must not pad or publish malformed texture data along the way.
    std::int32_t previousSector = -1;
    for (const VertexRef ref : selection)
    {
        if (ref.sector == previousSector || !isValidSectorIndex(level, ref.sector))
        {
            continue;
        }
        previousSector = ref.sector;
        if (!sectorHasValidUvData(
                level.sectors[static_cast<std::size_t>(ref.sector)]))
        {
            return 0;
        }
    }

    std::int32_t merged = 0;

    // Step 2: compare selected vertices only within the same sector and merge coincident pairs.
    for (std::size_t i = 0; i + 1 < selection.size(); ++i)
    {
        const VertexRef v1Ref = selection[i];
        if (!isValidSectorIndex(level, v1Ref.sector))
        {
            continue;
        }

        Sector& sector = level.sectors[static_cast<std::size_t>(v1Ref.sector)];
        if (v1Ref.vertex < 0 || static_cast<std::size_t>(v1Ref.vertex) >= sector.vertices.size())
        {
            continue;
        }

        const Vertex& v1 = sector.vertices[static_cast<std::size_t>(v1Ref.vertex)];

        for (std::size_t j = i + 1; j < selection.size(); ++j)
        {
            const VertexRef v2Ref = selection[j];
            if (v2Ref.sector != v1Ref.sector)
            {
                break;
            }

            if (v2Ref.vertex < 0 || static_cast<std::size_t>(v2Ref.vertex) >= sector.vertices.size())
            {
                continue;
            }

            if (v1Ref.vertex == v2Ref.vertex)
            {
                continue;
            }

            const Vertex& v2 = sector.vertices[static_cast<std::size_t>(v2Ref.vertex)];

            if (!SameValue(v1.x, v2.x, CloseEnough) || !SameValue(v1.y, v2.y, CloseEnough) || !SameValue(v1.z, v2.z, CloseEnough))
            {
                continue;
            }

            const std::int32_t keepVertex = std::min(v1Ref.vertex, v2Ref.vertex);
            const std::int32_t delVertex = std::max(v1Ref.vertex, v2Ref.vertex);

            // Step 3: rewrite affected surface references and collapse any zero-length edges.
            for (Surface& surface : sector.surfaces)
            {
                if (replaceVertexReferenceAndCollapseSurface(surface, keepVertex, delVertex))
                {
                    // The topology helper removes the matching texture corner
                    // together with a collapsed geometric corner. Recalculate
                    // only the derived normal: a vertex merge must not
                    // reproject or otherwise rewrite surviving authored UVs.
                    surface.normal = surfaceCalcNormalNewell(sector, surface);
                }
            }

            // Step 4: remove the duplicate sector vertex and shift all higher indices down.
            sector.vertices.erase(sector.vertices.begin() + static_cast<std::ptrdiff_t>(delVertex));
            rewriteSurfaceVertexIndicesAfterDelete(sector, delVertex);

            // Step 5: update the remaining normalized selection so later comparisons target current indices.
            rewriteSelectionVertexIndicesAfterDelete(selection, v1Ref.sector, keepVertex, delVertex);

            ++merged;
        }
    }

    return merged;
}

} // namespace sed::level
