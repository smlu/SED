#include "sed/level.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <string>

namespace sed
{

namespace
{

bool isRawJklCogValue(
    const CogValue& value,
    const std::size_t parameterIndex) noexcept
{
    if (value.type != ValueType::str || value.cogType != CogType::unk
        || value.name.size() <= 5
        || value.name.compare(0, 5, "param") != 0)
    {
        return false;
    }

    std::size_t parsed = 0;
    for (std::size_t index = 5; index < value.name.size(); ++index)
    {
        const unsigned char character = static_cast<unsigned char>(value.name[index]);
        if (std::isdigit(character) == 0)
        {
            return false;
        }
        const std::size_t digit = static_cast<std::size_t>(character - '0');
        if (parsed > (std::numeric_limits<std::size_t>::max() - digit) / 10)
        {
            return false;
        }
        parsed = parsed * 10 + digit;
    }
    return parsed == parameterIndex;
}

} // namespace

std::int32_t Sector::findVertex(const double x, const double y, const double z) const noexcept
{
    for (std::size_t i = 0; i < vertices.size(); ++i)
    {
        const Vertex& v = vertices[i];
        if (!SameValue(v.x, x, CloseEnough) || !SameValue(v.y, y, CloseEnough) || !SameValue(v.z, z, CloseEnough))
        {
            continue;
        }
        return static_cast<std::int32_t>(i);
    }
    return -1;
}

std::int32_t Sector::newVertex(const double x, const double y, const double z)
{
    vertices.push_back(Vertex{ x, y, z, 0 });
    return static_cast<std::int32_t>(vertices.size() - 1);
}

std::int32_t Sector::addVertex(const double x, const double y, const double z)
{
    const std::int32_t index = findVertex(x, y, z);
    if (index >= 0)
    {
        return index;
    }
    return newVertex(x, y, z);
}

std::int32_t Sector::addSurface(Surface surface)
{
    surfaces.push_back(std::move(surface));
    return static_cast<std::int32_t>(surfaces.size() - 1);
}

void Level::clear()
{
    exportBufferExtras = {};
    uvCoordinateSpace = UvCoordinateSpace::Normalized;
    importedJklSurfaceRefs.clear();
    importedJklCogRowsPresent = false;
    sectors.clear();
    cogs.clear();
    templates.clear();
    things.clear();
    motsArchLighting.clear();
    motsArchLightingBakeBaselines.clear();
    lights.clear();
    h3doNodes.clear();
    colormaps.clear();
    materials.clear();
    layers.clear();
    masterCMP.clear();
    ppunit = 320.0;
    lvisstring.clear();
    curId = 1;
}

bool Level::canAllocateIds(const std::size_t count) const noexcept
{
    // INT32_MAX is the exhausted next-ID sentinel, so the last assignable ID is INT32_MAX - 1.
    const std::int64_t available =
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())
        - static_cast<std::int64_t>(curId);
    return available >= 0
        && count <= static_cast<std::uintmax_t>(available);
}

std::int32_t Level::newId()
{
    if ( !canAllocateIds(1) )
    {
        throw IdExhaustionError("Level entity ID space is exhausted");
    }
    // A successful final allocation advances curId to INT32_MAX, which remains reserved.
    return curId++;
}

std::string Level::getMasterCMP() const
{
    if (kind == ProjectType::IJIM)
    {
        return masterCMP;
    }

    if (!masterCMP.empty())
    {
        return masterCMP;
    }

    if (!sectors.empty() && !sectors[0].colormap.empty())
    {
        return sectors[0].colormap;
    }

    return "dflt.cmp";
}

std::string Level::getLayerName(const std::int32_t n) const
{
    if (n >= 0 && static_cast<std::size_t>(n) < layers.size())
    {
        return layers[static_cast<std::size_t>(n)];
    }

    return "LAYER" + std::to_string(n);
}

std::int32_t Level::addLayer(const std::string& name)
{
    const auto it = std::find(layers.begin(), layers.end(), name);
    if (it != layers.end())
    {
        return static_cast<std::int32_t>(it - layers.begin());
    }

    layers.push_back(name);
    return static_cast<std::int32_t>(layers.size() - 1);
}

void Level::addMissingLayers()
{
    for (Sector& sector : sectors)
    {
        if (sector.layer < 0 || static_cast<std::size_t>(sector.layer) >= layers.size())
        {
            sector.layer = addLayer("Layer" + std::to_string(sector.layer));
        }
    }

    for (Thing& thing : things)
    {
        if (thing.layer < 0 || static_cast<std::size_t>(thing.layer) >= layers.size())
        {
            thing.layer = addLayer("Layer" + std::to_string(thing.layer));
        }
    }

    for (SedLight& light : lights)
    {
        if (light.layer < 0 || static_cast<std::size_t>(light.layer) >= layers.size())
        {
            light.layer = addLayer("Layer" + std::to_string(light.layer));
        }
    }
}

std::vector<std::size_t> Level::layerUsageCounts() const
{
    std::vector<std::size_t> counts(layers.size(), 0);

    const auto countLayer = [&counts](const std::int32_t layer) noexcept
    {
        if (layer >= 0 && static_cast<std::size_t>(layer) < counts.size())
        {
            ++counts[static_cast<std::size_t>(layer)];
        }
    };

    for (const Sector& sector : sectors)
    {
        countLayer(sector.layer);
    }
    for (const Thing& thing : things)
    {
        countLayer(thing.layer);
    }
    for (const SedLight& light : lights)
    {
        countLayer(light.layer);
    }

    return counts;
}

bool Level::remapLayers(std::vector<std::string> nextLayers,
                        const std::vector<std::int32_t>& oldToNew)
{
    if (nextLayers.empty() || oldToNew.size() != layers.size())
    {
        return false;
    }

    std::vector<bool> claimedDestinations(nextLayers.size(), false);
    for (const std::int32_t destination : oldToNew)
    {
        if (destination < -1
            || (destination >= 0
                && static_cast<std::size_t>(destination) >= nextLayers.size()))
        {
            return false;
        }

        if (destination >= 0)
        {
            const std::size_t index = static_cast<std::size_t>(destination);
            if (claimedDestinations[index])
            {
                // Merging two semantic layers is deliberately not a reorder.
                return false;
            }
            claimedDestinations[index] = true;
        }
    }

    const auto validReferencedLayer = [this, &oldToNew](const std::int32_t layer) noexcept
    {
        return layer >= 0
            && static_cast<std::size_t>(layer) < layers.size()
            && oldToNew[static_cast<std::size_t>(layer)] >= 0;
    };

    if (std::any_of(sectors.begin(), sectors.end(), [&](const Sector& sector)
            {
                return !validReferencedLayer(sector.layer);
            })
        || std::any_of(things.begin(), things.end(), [&](const Thing& thing)
            {
                return !validReferencedLayer(thing.layer);
            })
        || std::any_of(lights.begin(), lights.end(), [&](const SedLight& light)
            {
                return !validReferencedLayer(light.layer);
            }))
    {
        return false;
    }

    for (Sector& sector : sectors)
    {
        sector.layer = oldToNew[static_cast<std::size_t>(sector.layer)];
    }
    for (Thing& thing : things)
    {
        thing.layer = oldToNew[static_cast<std::size_t>(thing.layer)];
    }
    for (SedLight& light : lights)
    {
        light.layer = oldToNew[static_cast<std::size_t>(light.layer)];
    }

    layers = std::move(nextLayers);
    return true;
}

std::int32_t Level::getGlobalSurfaceNumber(const std::int32_t sectorIndex, const std::int32_t surfaceIndex) const noexcept
{
    if (sectorIndex < 0 || surfaceIndex < 0)
    {
        return -1;
    }

    const std::size_t sec = static_cast<std::size_t>(sectorIndex);
    if (sec >= sectors.size())
    {
        return -1;
    }

    const std::size_t sf = static_cast<std::size_t>(surfaceIndex);
    if (sf >= sectors[sec].surfaces.size())
    {
        return -1;
    }

    std::int32_t base = 0;
    for (std::size_t i = 0; i < sec; ++i)
    {
        base += static_cast<std::int32_t>(sectors[i].surfaces.size());
    }

    return base + surfaceIndex;
}

std::optional<std::int32_t> Level::tryGetSectorIndexByNum(const std::int32_t sectorNumber) const noexcept
{
    for (std::size_t sectorIndex = 0; sectorIndex < sectors.size(); ++sectorIndex)
    {
        if (sectors[sectorIndex].num == sectorNumber)
        {
            return static_cast<std::int32_t>(sectorIndex);
        }
    }

    return std::nullopt;
}

std::optional<std::int32_t> Level::tryGetThingIndexByNum(const std::int32_t thingNumber) const noexcept
{
    for (std::size_t thingIndex = 0; thingIndex < things.size(); ++thingIndex)
    {
        if (things[thingIndex].num == thingNumber)
        {
            return static_cast<std::int32_t>(thingIndex);
        }
    }

    return std::nullopt;
}

std::optional<SurfaceRef> Level::tryGetSurfaceRefBySectorAndSurfaceNum(
    const std::int32_t sectorNumber,
    const std::int32_t surfaceNumber) const noexcept
{
    const std::optional<std::int32_t> sectorIndex = tryGetSectorIndexByNum(sectorNumber);
    if (!sectorIndex)
    {
        return std::nullopt;
    }

    const Sector& sector = sectors[static_cast<std::size_t>(*sectorIndex)];
    for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
    {
        if (sector.surfaces[surfaceIndex].num == surfaceNumber)
        {
            return SurfaceRef{*sectorIndex, static_cast<std::int32_t>(surfaceIndex)};
        }
    }

    return std::nullopt;
}

std::optional<SurfaceRef> Level::tryGetSurfaceRefByGlobalSurfaceNumber(const std::int32_t globalSurfaceNumber) const noexcept
{
    if (globalSurfaceNumber < 0)
    {
        return std::nullopt;
    }

    std::int32_t remaining = globalSurfaceNumber;
    for (std::size_t sectorIndex = 0; sectorIndex < sectors.size(); ++sectorIndex)
    {
        const std::int32_t count = static_cast<std::int32_t>(sectors[sectorIndex].surfaces.size());
        if (remaining < count)
        {
            return SurfaceRef{ static_cast<std::int32_t>(sectorIndex), remaining };
        }

        remaining -= count;
    }

    return std::nullopt;
}

bool Level::hasPendingJklCogValues() const noexcept
{
    if (importedJklSurfaceRefs.empty())
    {
        return false;
    }

    for (const Cog& cog : cogs)
    {
        for (std::size_t index = 0; index < cog.values.size(); ++index)
        {
            if (isRawJklCogValue(cog.values[index], index))
            {
                return true;
            }
        }
        // JKL permits a row with no initializers.  Until its script schema is
        // loaded, an empty imported row is just as unresolved as a row with
        // raw paramN values; otherwise a noncanonical GEORESOURCE order could
        // be saved with a wrong surface target.
        if (importedJklCogRowsPresent && cog.values.empty())
        {
            return true;
        }
    }

    return false;
}

bool Level::hasAmbiguousPendingJklCogSurfaceReferences() const noexcept
{
    if (!hasPendingJklCogValues())
    {
        return false;
    }

    for (std::size_t wireIndex = 0;
         wireIndex < importedJklSurfaceRefs.size();
         ++wireIndex)
    {
        const SurfaceRef& imported = importedJklSurfaceRefs[wireIndex];
        if (getGlobalSurfaceNumber(imported.sector, imported.surface)
            != static_cast<std::int32_t>(wireIndex))
        {
            return true;
        }
    }

    return false;
}

Sector& Level::newSector()
{
    if ( !canAllocateIds(1) )
    {
        throw IdExhaustionError("Level entity ID space is exhausted");
    }
    sectors.emplace_back();
    Sector& sector = sectors.back();
    sector.id = newId();
    sector.num = static_cast<std::int32_t>(sectors.size() - 1);
    return sector;
}

Cog& Level::newCog()
{
    if ( !canAllocateIds(1) )
    {
        throw IdExhaustionError("Level entity ID space is exhausted");
    }
    cogs.emplace_back();
    Cog& cog = cogs.back();
    cog.id = newId();
    cog.num = static_cast<std::int32_t>(cogs.size() - 1);
    return cog;
}

Thing& Level::newThing()
{
    if ( !canAllocateIds(1) )
    {
        throw IdExhaustionError("Level entity ID space is exhausted");
    }
    things.emplace_back();
    Thing& thing = things.back();
    thing.id = newId();
    thing.num = static_cast<std::int32_t>(things.size() - 1);
    return thing;
}

SedLight& Level::newLight()
{
    if ( !canAllocateIds(1) )
    {
        throw IdExhaustionError("Level entity ID space is exhausted");
    }
    lights.emplace_back();
    SedLight& light = lights.back();
    light.id = newId();
    return light;
}

HNode& Level::new3doNode()
{
    if ( !canAllocateIds(1) )
    {
        throw IdExhaustionError("Level entity ID space is exhausted");
    }
    h3doNodes.emplace_back();
    HNode& node = h3doNodes.back();
    node.id = newId();
    node.num = static_cast<std::int32_t>(h3doNodes.size() - 1);
    return node;
}

} // namespace sed
