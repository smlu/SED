#include "sed/level/edit.hpp"

#include "sed/level/geometry.hpp"
#include "cog_reference_utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

namespace sed::level
{
namespace
{

bool isValidSectorIndex(const Level& level, const std::int32_t sectorIndex) noexcept
{
    if (sectorIndex < 0)
    {
        return false;
    }

    return static_cast<std::size_t>(sectorIndex) < level.sectors.size();
}

bool isValidThingIndex(const Level& level, const std::int32_t thingIndex) noexcept
{
    if (thingIndex < 0)
    {
        return false;
    }

    return static_cast<std::size_t>(thingIndex) < level.things.size();
}

bool isValidLightIndex(const Level& level, const std::int32_t lightIndex) noexcept
{
    if (lightIndex < 0)
    {
        return false;
    }

    return static_cast<std::size_t>(lightIndex) < level.lights.size();
}

bool isValidCogIndex(const Level& level, const std::int32_t cogIndex) noexcept
{
    if (cogIndex < 0)
    {
        return false;
    }

    return static_cast<std::size_t>(cogIndex) < level.cogs.size();
}

void renumberThings(Level& level) noexcept
{
    for (std::size_t i = 0; i < level.things.size(); ++i)
    {
        level.things[i].num = static_cast<std::int32_t>(i);
    }
}

void renumberCogs(Level& level) noexcept
{
    for (std::size_t i = 0; i < level.cogs.size(); ++i)
    {
        level.cogs[i].num = static_cast<std::int32_t>(i);
    }
}

bool eraseSectorVertexUnchecked(
    Sector& sector,
    const std::int32_t vertexIndex)
{
    if (vertexIndex < 0
        || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
    {
        return false;
    }

    for (Surface& surface : sector.surfaces)
    {
        if (surface.texVertices.size() != surface.vertices.size())
        {
            return false;
        }
        for (std::size_t index = surface.vertices.size(); index-- > 0;)
        {
            if (surface.vertices[index] != vertexIndex)
            {
                continue;
            }
            surface.vertices.erase(surface.vertices.begin()
                + static_cast<std::ptrdiff_t>(index));
            surface.texVertices.erase(surface.texVertices.begin()
                + static_cast<std::ptrdiff_t>(index));
        }
    }

    sector.vertices.erase(sector.vertices.begin()
        + static_cast<std::ptrdiff_t>(vertexIndex));
    for (Surface& surface : sector.surfaces)
    {
        for (std::int32_t& vi : surface.vertices)
        {
            if (vi > vertexIndex)
            {
                --vi;
            }
        }
    }
    for (std::size_t index = 0; index < sector.surfaces.size(); ++index)
    {
        sector.surfaces[index].num = static_cast<std::int32_t>(index);
    }
    return true;
}

bool isRedundantSurfaceVertex(
    const Sector& sector,
    const Surface& surface,
    const std::int32_t localVertex) noexcept
{
    if (surface.vertices.size() < 4 || localVertex < 0
        || static_cast<std::size_t>(localVertex) >= surface.vertices.size())
    {
        return false;
    }
    const std::int32_t previous = surface.vertices[static_cast<std::size_t>(
        prevIndex(localVertex, static_cast<std::int32_t>(surface.vertices.size())))];
    const std::int32_t current = surface.vertices[static_cast<std::size_t>(localVertex)];
    const std::int32_t next = surface.vertices[static_cast<std::size_t>(
        nextIndex(localVertex, static_cast<std::int32_t>(surface.vertices.size())))];
    if (previous < 0 || current < 0 || next < 0
        || static_cast<std::size_t>(previous) >= sector.vertices.size()
        || static_cast<std::size_t>(current) >= sector.vertices.size()
        || static_cast<std::size_t>(next) >= sector.vertices.size())
    {
        return false;
    }
    const Vertex& av = sector.vertices[static_cast<std::size_t>(previous)];
    const Vertex& pv = sector.vertices[static_cast<std::size_t>(current)];
    const Vertex& bv = sector.vertices[static_cast<std::size_t>(next)];
    const TVector a{ av.x, av.y, av.z };
    const TVector p{ pv.x, pv.y, pv.z };
    const TVector b{ bv.x, bv.y, bv.z };
    const TVector ab = b - a;
    const double ab2 = ab.Dot(ab);
    if (!std::isfinite(ab2) || ab2 <= math::kTinyEpsilon)
    {
        return false;
    }
    const double t = (p - a).Dot(ab) / ab2;
    const TVector residual = p - (a + ab.scaled(t));
    const double tolerance2 = std::max(1.0, ab2) * 1.0e-12;
    return std::isfinite(t) && t > math::kTinyEpsilon
        && t < 1.0 - math::kTinyEpsilon
        && residual.Dot(residual) <= tolerance2;
}

} // namespace

bool levelDeleteSectorVertex(Level& level, const std::int32_t sectorIndex, const std::int32_t vertexIndex, const bool silent)
{
    (void)silent;

    if (!isValidSectorIndex(level, sectorIndex))
    {
        return false;
    }
    if (level.hasAmbiguousPendingJklCogSurfaceReferences())
    {
        return false;
    }

    detail::GeometryCogReferences cogReferences(level);

    // Work on a complete sector candidate. Vector growth/compaction may throw;
    // the authoritative Level must remain untouched unless every topology and
    // UV update succeeds.
    Sector candidate = level.sectors[static_cast<std::size_t>(sectorIndex)];
    Sector& sector = candidate;
    if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
    {
        return false;
    }

    struct SurfaceVertexUse
    {
        std::int32_t surface{};
        std::int32_t vertex{};
    };

    std::vector<SurfaceVertexUse> uses{};
    uses.reserve(2);

    for (std::int32_t sf = 0; sf < static_cast<std::int32_t>(sector.surfaces.size()); ++sf)
    {
        Surface& surface = sector.surfaces[static_cast<std::size_t>(sf)];

        const auto it = std::find(surface.vertices.begin(), surface.vertices.end(), vertexIndex);
        if (it == surface.vertices.end())
        {
            continue;
        }

        // Safe Delete validates one topological occurrence per polygon. A
        // malformed/recovered polygon that repeats the same global vertex can
        // have different neighbours at each occurrence, so removing all of
        // them through the unchecked compactor would bypass that validation.
        // Leave duplicate-reference repair to confirmed Force Delete.
        if (std::find(std::next(it), surface.vertices.end(), vertexIndex)
            != surface.vertices.end())
        {
            return false;
        }

        const std::int32_t iv = static_cast<std::int32_t>(std::distance(surface.vertices.begin(), it));
        uses.push_back(SurfaceVertexUse{ sf, iv });

        if (!isRedundantSurfaceVertex(sector, surface, iv))
        {
            return false;
        }

        if (uses.size() > 2)
        {
            return false;
        }
    }

    if (uses.size() == 2)
    {
        Surface& surface1 = sector.surfaces[static_cast<std::size_t>(uses[0].surface)];
        Surface& surface2 = sector.surfaces[static_cast<std::size_t>(uses[1].surface)];

        if (surface1.vertices.size() < 4 || surface2.vertices.size() < 4)
        {
            return false;
        }

        const std::int32_t ed1 = uses[0].vertex;
        const std::int32_t ed2 = uses[1].vertex;

        const std::int32_t prev1 = prevIndex(ed1, static_cast<std::int32_t>(surface1.vertices.size()));
        const std::int32_t next1 = nextIndex(ed1, static_cast<std::int32_t>(surface1.vertices.size()));

        const std::int32_t prev2 = prevIndex(ed2, static_cast<std::int32_t>(surface2.vertices.size()));
        const std::int32_t next2 = nextIndex(ed2, static_cast<std::int32_t>(surface2.vertices.size()));

        const std::int32_t previousVertex1 =
            surface1.vertices[static_cast<std::size_t>(prev1)];
        const std::int32_t nextVertex1 =
            surface1.vertices[static_cast<std::size_t>(next1)];
        const std::int32_t previousVertex2 =
            surface2.vertices[static_cast<std::size_t>(prev2)];
        const std::int32_t nextVertex2 =
            surface2.vertices[static_cast<std::size_t>(next2)];

        // Both polygons must restore the same unsplit edge.  Winding is not a
        // deletion-safety condition: recovered/imported geometry can contain
        // equal-winding owners even though ordinary closed sectors use the
        // reciprocal order.  Requiring only reciprocal order rejected a
        // genuinely redundant shared-edge subdivision.  The unordered pair
        // still prevents two unrelated collinear polygon corners from being
        // removed as one global vertex.
        const bool sameEndpointPair =
            (previousVertex1 == previousVertex2
                && nextVertex1 == nextVertex2)
            || (previousVertex1 == nextVertex2
                && nextVertex1 == previousVertex2);
        if (!sameEndpointPair)
        {
            return false;
        }
    }

    if (!eraseSectorVertexUnchecked(sector, vertexIndex))
    {
        return false;
    }
    level.sectors[static_cast<std::size_t>(sectorIndex)] = std::move(candidate);
    cogReferences.publish(level);
    return true;
}

bool levelForceDeleteSectorVertex(
    Level& level,
    const std::int32_t sectorIndex,
    const std::int32_t vertexIndex)
{
    if (!isValidSectorIndex(level, sectorIndex))
    {
        return false;
    }
    if (level.hasAmbiguousPendingJklCogSurfaceReferences())
    {
        return false;
    }
    detail::GeometryCogReferences cogReferences(level);
    Sector candidate = level.sectors[static_cast<std::size_t>(sectorIndex)];
    if (!eraseSectorVertexUnchecked(candidate, vertexIndex))
    {
        return false;
    }
    level.sectors[static_cast<std::size_t>(sectorIndex)] = std::move(candidate);
    cogReferences.publish(level);
    return true;
}

bool levelDeleteThing(Level& level, const std::int32_t thingIndex) noexcept
{
    if (!isValidThingIndex(level, thingIndex))
    {
        return false;
    }

    const std::size_t oldThingCount = level.things.size();
    level.things.erase(level.things.begin() + static_cast<std::ptrdiff_t>(thingIndex));

    for (Cog& cog : level.cogs)
    {
        for (CogValue& value : cog.values)
        {
            if (value.type != ValueType::ptr || value.cogType != CogType::thing
                || value.intValue < thingIndex
                || static_cast<std::size_t>(value.intValue) >= oldThingCount)
            {
                continue;
            }

            if (value.intValue == thingIndex)
            {
                value.intValue = -1;
            }
            else
            {
                --value.intValue;
            }
            // A paired/authored text token must not retain the deleted or
            // pre-compaction Thing number.  Typed Thing values are formatted
            // from intValue, so the stale text is only an ambiguity source for
            // later import/editor operations.
            value.stringValue.clear();
        }
    }

    renumberThings(level);
    return true;
}

bool levelDeleteLight(Level& level, const std::int32_t lightIndex) noexcept
{
    if (!isValidLightIndex(level, lightIndex))
    {
        return false;
    }

    level.lights.erase(level.lights.begin() + static_cast<std::ptrdiff_t>(lightIndex));
    return true;
}

bool levelDeleteCog(Level& level, const std::int32_t cogIndex) noexcept
{
    if (!isValidCogIndex(level, cogIndex))
    {
        return false;
    }

    level.cogs.erase(level.cogs.begin() + static_cast<std::ptrdiff_t>(cogIndex));

    for (Cog& cog : level.cogs)
    {
        for (CogValue& value : cog.values)
        {
            if (value.cogType != CogType::cog || value.type != ValueType::integer)
            {
                continue;
            }

            if (value.intValue == cogIndex)
            {
                value.intValue = -1;
            }
            else if (value.intValue > cogIndex)
            {
                value.intValue -= 1;
            }
        }
    }

    renumberCogs(level);
    return true;
}

} // namespace sed::level
