#include "sed/level/clipboard.hpp"

#include "sed/assets/sed.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level/transform.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace sed::level
{
namespace
{

constexpr std::array<char, 4> kClipboardSectionType{ 'c', 'l', 'i', 'p' };
constexpr std::array<std::byte, 8> kClipboardMetadataMagic{
    std::byte{ 'S' }, std::byte{ 'E' }, std::byte{ 'D' }, std::byte{ 'C' },
    std::byte{ 'L' }, std::byte{ 'I' }, std::byte{ 'P' }, std::byte{ 0 }
};
constexpr std::uint32_t kClipboardMetadataVersion = 1;
constexpr std::size_t kMaximumClipboardMetadataBytes = 16u * 1024u * 1024u;
constexpr std::size_t kMaxMotsArchLightingRecords = 1'000'000;

void setClipboardError(std::string* const error, std::string message)
{
    if (error)
    {
        *error = std::move(message);
    }
}

void appendU32(std::vector<std::byte>& bytes, const std::uint32_t value)
{
    for (std::uint32_t shift = 0; shift < 32; shift += 8)
    {
        bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffu));
    }
}

void appendI32(std::vector<std::byte>& bytes, const std::int32_t value)
{
    appendU32(bytes, static_cast<std::uint32_t>(value));
}

void appendDouble(std::vector<std::byte>& bytes, const double value)
{
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
    for (std::uint32_t shift = 0; shift < 64; shift += 8)
    {
        bytes.push_back(static_cast<std::byte>((bits >> shift) & 0xffu));
    }
}

class ClipboardMetadataReader final
{
public:
    explicit ClipboardMetadataReader(const std::span<const std::byte> bytes)
        : bytes_(bytes)
    {
    }

    std::span<const std::byte> take(const std::size_t count)
    {
        if (count > bytes_.size() - offset_)
        {
            throw std::runtime_error("Entity clipboard metadata is truncated.");
        }
        const std::span<const std::byte> result = bytes_.subspan(offset_, count);
        offset_ += count;
        return result;
    }

    std::uint32_t u32()
    {
        const auto bytes = take(4);
        return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[0]))
            | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[1])) << 8u)
            | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[2])) << 16u)
            | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[3])) << 24u);
    }

    std::int32_t i32()
    {
        return static_cast<std::int32_t>(u32());
    }

    double f64()
    {
        const auto bytes = take(8);
        std::uint64_t bits = 0;
        for (std::uint32_t index = 0; index < 8; ++index)
        {
            bits |= static_cast<std::uint64_t>(
                std::to_integer<std::uint8_t>(bytes[index])) << (index * 8u);
        }
        return std::bit_cast<double>(bits);
    }

    std::string string()
    {
        const std::uint32_t length = u32();
        const auto bytes = take(length);
        return std::string(
            reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    bool empty() const noexcept
    {
        return offset_ == bytes_.size();
    }

private:
    std::span<const std::byte> bytes_{};
    std::size_t offset_{};
};

void appendLayerRef(
    std::vector<std::byte>& bytes,
    const ClipboardLayerRef& ref)
{
    if (ref.name.size() > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::runtime_error("Entity clipboard layer name is too long.");
    }
    appendI32(bytes, ref.sourceIndex);
    appendU32(bytes, static_cast<std::uint32_t>(ref.name.size()));
    bytes.insert(bytes.end(),
        reinterpret_cast<const std::byte*>(ref.name.data()),
        reinterpret_cast<const std::byte*>(ref.name.data() + ref.name.size()));
}

std::vector<ClipboardLayerRef> clipboardLayerRefs(
    const EntityClipboard& clipboard)
{
    if (const auto* sectors = std::get_if<SectorClipboardPayload>(&clipboard))
    {
        return sectors->layers;
    }
    std::vector<ClipboardLayerRef> refs{};
    if (const auto* things = std::get_if<ThingClipboardPayload>(&clipboard))
    {
        refs.reserve(things->things.size());
        for (const ThingClipboardEntry& entry : things->things)
        {
            refs.push_back(entry.layer);
        }
    }
    else if (const auto* lights = std::get_if<LightClipboardPayload>(&clipboard))
    {
        refs.reserve(lights->lights.size());
        for (const LightClipboardEntry& entry : lights->lights)
        {
            refs.push_back(entry.layer);
        }
    }
    return refs;
}

ProjectType clipboardSourceProject(const EntityClipboard& clipboard) noexcept
{
    if (const auto* sectors = std::get_if<SectorClipboardPayload>(&clipboard))
    {
        return sectors->sourceProject;
    }
    if (const auto* things = std::get_if<ThingClipboardPayload>(&clipboard))
    {
        return things->sourceProject;
    }
    if (const auto* lights = std::get_if<LightClipboardPayload>(&clipboard))
    {
        return lights->sourceProject;
    }
    return ProjectType::JKDF2;
}

TVector clipboardAnchor(const EntityClipboard& clipboard) noexcept
{
    if (const auto* sectors = std::get_if<SectorClipboardPayload>(&clipboard))
    {
        return sectors->anchor;
    }
    if (const auto* things = std::get_if<ThingClipboardPayload>(&clipboard))
    {
        return things->anchor;
    }
    if (const auto* lights = std::get_if<LightClipboardPayload>(&clipboard))
    {
        return lights->anchor;
    }
    return {};
}

template <typename IsValid>
std::vector<std::int32_t> sortedValidIndices(
    const std::span<const std::int32_t> indices,
    IsValid&& isValid)
{
    std::vector<std::int32_t> out{};
    out.reserve(indices.size());
    for (const std::int32_t index : indices)
    {
        if (isValid(index))
        {
            out.push_back(index);
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

ClipboardLayerRef layerRefFor(const Level& level, const std::int32_t layer)
{
    ClipboardLayerRef ref{};
    ref.sourceIndex = layer;
    if (layer >= 0 && static_cast<std::size_t>(layer) < level.layers.size())
    {
        ref.name = level.layers[static_cast<std::size_t>(layer)];
    }
    return ref;
}

std::int32_t resolveLayer(Level& level, const ClipboardLayerRef& ref, bool& addedLayer)
{
    if (!ref.name.empty())
    {
        const auto found = std::find(level.layers.begin(), level.layers.end(), ref.name);
        if (found != level.layers.end())
        {
            return static_cast<std::int32_t>(std::distance(level.layers.begin(), found));
        }

        addedLayer = true;
        return level.addLayer(ref.name);
    }

    if (ref.sourceIndex >= 0
        && static_cast<std::size_t>(ref.sourceIndex) < level.layers.size())
    {
        return ref.sourceIndex;
    }

    if (level.layers.empty())
    {
        addedLayer = true;
        return level.addLayer("Layer0");
    }
    return 0;
}

TVector clipboardTranslation(const TVector& anchor, const std::optional<TVector>& target) noexcept
{
    return target ? (*target - anchor) : TVector{};
}

bool validThingArchLightingPayload(const ThingClipboardPayload& payload)
{
    if (payload.motsArchLighting.size()
            != payload.motsArchLightingSourceIndices.size()
        || payload.motsArchLighting.size() > 1'000'000)
    {
        return false;
    }

    if (payload.sourceProject != ProjectType::MOTS
        && !payload.motsArchLighting.empty())
    {
        return false;
    }

    std::unordered_set<std::int32_t> sourceIndices{};
    std::unordered_set<std::int32_t> objectIndices{};
    objectIndices.reserve(payload.motsArchLighting.size());
    for (std::size_t objectIndex = 0;
         objectIndex < payload.motsArchLighting.size();
         ++objectIndex)
    {
        const std::int32_t sourceIndex =
            payload.motsArchLightingSourceIndices[objectIndex];
        if (sourceIndex < -1
            || (sourceIndex >= 0 && !sourceIndices.insert(sourceIndex).second))
        {
            return false;
        }

        const MotsArchLightObject& object = payload.motsArchLighting[objectIndex];
        if (object.wireIndex < 0 || !objectIndices.insert(object.wireIndex).second
            || object.meshes.size() > 1'000'000)
        {
            return false;
        }
        std::unordered_set<std::int32_t> meshIndices{};
        meshIndices.reserve(object.meshes.size());
        for (const MotsArchLightMesh& mesh : object.meshes)
        {
            if (mesh.wireIndex < 0 || !meshIndices.insert(mesh.wireIndex).second
                || mesh.vertices.size() > 1'000'000)
            {
                return false;
            }
            for (const MotsArchLightVertex& vertex : mesh.vertices)
            {
                if (!std::isfinite(vertex.mono) || !std::isfinite(vertex.red)
                    || !std::isfinite(vertex.green)
                    || !std::isfinite(vertex.blue))
                {
                    return false;
                }
            }
        }
    }

    for (const ThingClipboardEntry& entry : payload.things)
    {
        if (entry.thing.archLightingIndex < -1
            || (entry.thing.archLightingIndex >= 0
                && static_cast<std::size_t>(entry.thing.archLightingIndex)
                    >= payload.motsArchLighting.size()))
        {
            return false;
        }
    }
    return true;
}

bool isValidLocalAdjoin(
    const SectorClipboardPayload& payload,
    const SurfaceRef ref) noexcept
{
    if (ref.sector < 0 || ref.surface < 0
        || static_cast<std::size_t>(ref.sector) >= payload.sectors.size())
    {
        return false;
    }
    const Sector& target = payload.sectors[static_cast<std::size_t>(ref.sector)];
    return static_cast<std::size_t>(ref.surface) < target.surfaces.size();
}

std::optional<EntityPasteResult> pasteSectors(
    Level& level,
    const SectorClipboardPayload& payload,
    const std::optional<TVector>& target,
    const UvTextureDimensionsResolver& resolveDimensions)
{
    if (payload.sectors.empty() || payload.layers.size() != payload.sectors.size())
    {
        return std::nullopt;
    }
    if ( !level.canAllocateIds(payload.sectors.size()) )
    {
        return std::nullopt;
    }

    const auto validUvCoordinateSpace = [](const UvCoordinateSpace space)
    {
        return space == UvCoordinateSpace::TexturePixels
            || space == UvCoordinateSpace::Normalized;
    };
    if (!validUvCoordinateSpace(payload.uvCoordinateSpace)
        || !validUvCoordinateSpace(level.uvCoordinateSpace))
    {
        return std::nullopt;
    }

    std::optional<SectorClipboardPayload> convertedPayload{};
    const SectorClipboardPayload* source = &payload;
    if (payload.uvCoordinateSpace != level.uvCoordinateSpace)
    {
        if (!resolveDimensions)
        {
            return std::nullopt;
        }

        convertedPayload = payload;
        for (std::size_t sectorIndex = 0;
             sectorIndex < convertedPayload->sectors.size();
             ++sectorIndex)
        {
            const LevelUvConversionResult converted =
                convertSectorUvCoordinateSpace(
                    convertedPayload->sectors[sectorIndex],
                    static_cast<std::int32_t>(sectorIndex),
                    payload.uvCoordinateSpace,
                    level.uvCoordinateSpace,
                    resolveDimensions);
            if (!converted)
            {
                return std::nullopt;
            }
        }
        convertedPayload->uvCoordinateSpace = level.uvCoordinateSpace;
        source = &*convertedPayload;
    }

    // Step 1: reject a corrupted application payload before changing Level.
    for (const Sector& sector : source->sectors)
    {
        for (const Surface& surface : sector.surfaces)
        {
            if (surface.texVertices.size() != surface.vertices.size()
                || !std::isfinite(surface.uScale)
                || !std::isfinite(surface.vScale)
                || std::ranges::any_of(
                    surface.texVertices,
                    [](const TexVertex& texVertex)
                    {
                        return !std::isfinite(texVertex.u)
                            || !std::isfinite(texVertex.v);
                    }))
            {
                return std::nullopt;
            }
            for (const std::int32_t vertex : surface.vertices)
            {
                if (vertex < 0 || static_cast<std::size_t>(vertex) >= sector.vertices.size())
                {
                    return std::nullopt;
                }
            }
            if (surface.adjoin && !isValidLocalAdjoin(*source, *surface.adjoin))
            {
                return std::nullopt;
            }
        }
    }

    const TVector translation = clipboardTranslation(source->anchor, target);
    const std::int32_t firstIndex = static_cast<std::int32_t>(level.sectors.size());

    EntityPasteResult result{};
    result.kind = EntityClipboardKind::sectors;
    result.indices.reserve(source->sectors.size());

    // Step 2: append complete value copies with fresh editor identities.
    level.sectors.reserve(level.sectors.size() + source->sectors.size());
    for (std::size_t localIndex = 0; localIndex < source->sectors.size(); ++localIndex)
    {
        Sector copy = source->sectors[localIndex];
        copy.id = level.newId();
        copy.num = static_cast<std::int32_t>(level.sectors.size());
        copy.layer = resolveLayer(level, source->layers[localIndex], result.addedLayers);
        copy.mark = 0;

        for (Vertex& vertex : copy.vertices)
        {
            vertex.x += translation.x;
            vertex.y += translation.y;
            vertex.z += translation.z;
            vertex.mark = 0;
        }

        if (pointLightHasAuthoredParameters(copy.pointLight))
        {
            copy.pointLight.position = copy.pointLight.position + translation;
        }
        if (!copy.collideBox.IsZero())
        {
            copy.collideBox.x1 += translation.x;
            copy.collideBox.x2 += translation.x;
            copy.collideBox.y1 += translation.y;
            copy.collideBox.y2 += translation.y;
            copy.collideBox.z1 += translation.z;
            copy.collideBox.z2 += translation.z;
        }

        for (std::size_t surfaceIndex = 0; surfaceIndex < copy.surfaces.size(); ++surfaceIndex)
        {
            Surface& surface = copy.surfaces[surfaceIndex];
            surface.num = static_cast<std::int32_t>(surfaceIndex);
            surface.mark = 0;
            surface.d3dId = 0;
            surface.nAdj = -1;
            if (surface.adjoin)
            {
                surface.adjoin->sector += firstIndex;
            }
        }

        const std::int32_t newIndex = copy.num;
        level.sectors.push_back(std::move(copy));
        result.indices.push_back(newIndex);
    }

    // A pure translation preserves surface normals, UV axes/scales, and
    // texture coordinates exactly. Recalculating here would traverse the same
    // connected pasted component once per Sector and make large pastes
    // quadratic on the UI thread.
    return result;
}

std::optional<EntityPasteResult> pasteThings(
    Level& level,
    const ThingClipboardPayload& payload,
    const std::optional<TVector>& target)
{
    if ( payload.things.empty() )
    {
        return std::nullopt;
    }
    if (!validThingArchLightingPayload(payload)
        || (level.kind != ProjectType::MOTS
            && !payload.motsArchLighting.empty()))
    {
        return std::nullopt;
    }

    const auto maximumId = std::numeric_limits<std::int32_t>::max();
    const auto maximumCount = static_cast<std::uint64_t>(maximumId) + 1;
    if ( !level.canAllocateIds(payload.things.size())
        || level.things.size() > maximumCount
        || payload.things.size() > maximumCount - level.things.size() )
    {
        return std::nullopt;
    }

    const TVector translation = clipboardTranslation(payload.anchor, target);
    const auto finite = [](const TVector& point)
    {
        return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
    };
    if ( !finite(translation) )
    {
        return std::nullopt;
    }

    // A false transform result also means a valid numeric no-op. Check the
    // complete payload before assigning IDs or importing layers to distinguish
    // that case from a rejected translated pose.
    bool expectedPoseChange = false;
    const auto admitPosition = [&](const TVector& original)
    {
        const TVector moved = original + translation;
        if ( !finite(moved) )
        {
            return false;
        }
        expectedPoseChange = expectedPoseChange || moved.x != original.x
            || moved.y != original.y || moved.z != original.z;
        return true;
    };
    for ( const auto& entry : payload.things )
    {
        if ( !admitPosition({entry.thing.x, entry.thing.y, entry.thing.z}) )
        {
            return std::nullopt;
        }
        for ( const auto& value : entry.thing.values )
        {
            if ( value.type != ValueType::frame )
            {
                continue;
            }
            TVector position;
            TVector rotation;
            if ( value.tryGetFrame(position.x, position.y, position.z,
                    rotation.x, rotation.y, rotation.z) && !admitPosition(position) )
            {
                return std::nullopt;
            }
        }
    }

    const bool importsArchiveLighting = !payload.motsArchLighting.empty();

    // Export applies one aggregate budget to all MotS ARCHLIGHTING mesh and
    // vertex records.  Validate the destination plus only the incoming
    // objects that will actually be appended before copying or publishing
    // the staged Level.  Entries reused through a same-document source index
    // already contribute to the destination totals and must not be counted
    // twice.
    std::vector<std::int32_t> reusedArchiveLightingIndices(
        payload.motsArchLighting.size(), -1);
    if (importsArchiveLighting)
    {
        std::size_t meshRecords = 0;
        std::size_t vertexRecords = 0;
        const auto addWithinBudget = [](std::size_t& total,
                                        const std::size_t amount)
        {
            if (total > kMaxMotsArchLightingRecords
                || amount > kMaxMotsArchLightingRecords - total)
            {
                return false;
            }
            total += amount;
            return true;
        };

        for (const MotsArchLightObject& object : level.motsArchLighting)
        {
            if (!addWithinBudget(meshRecords, object.meshes.size()))
            {
                return std::nullopt;
            }
            for (const MotsArchLightMesh& mesh : object.meshes)
            {
                if (!addWithinBudget(vertexRecords, mesh.vertices.size()))
                {
                    return std::nullopt;
                }
            }
        }

        for (std::size_t payloadIndex = 0;
             payloadIndex < payload.motsArchLighting.size();
             ++payloadIndex)
        {
            const std::int32_t sourceIndex =
                payload.motsArchLightingSourceIndices[payloadIndex];
            if (sourceIndex >= 0
                && static_cast<std::size_t>(sourceIndex)
                    < level.motsArchLighting.size()
                && level.motsArchLighting[static_cast<std::size_t>(sourceIndex)]
                    == payload.motsArchLighting[payloadIndex])
            {
                reusedArchiveLightingIndices[payloadIndex] = sourceIndex;
                continue;
            }

            const MotsArchLightObject& object =
                payload.motsArchLighting[payloadIndex];
            if (!addWithinBudget(meshRecords, object.meshes.size()))
            {
                return std::nullopt;
            }
            for (const MotsArchLightMesh& mesh : object.meshes)
            {
                if (!addWithinBudget(vertexRecords, mesh.vertices.size()))
                {
                    return std::nullopt;
                }
            }
        }
    }

    Level staged;
    staged.kind = level.kind;
    staged.layers = level.layers;
    staged.curId = level.curId;
    if (importsArchiveLighting)
    {
        staged.motsArchLighting = level.motsArchLighting;
    }

    std::vector<std::int32_t> archiveLightingMap{};
    archiveLightingMap.reserve(payload.motsArchLighting.size());
    std::unordered_set<std::int32_t> usedObjectWireIndices{};
    std::int32_t nextObjectWireIndex = 0;
    if (importsArchiveLighting)
    {
        usedObjectWireIndices.reserve(staged.motsArchLighting.size()
            + payload.motsArchLighting.size());
        for (const MotsArchLightObject& object : staged.motsArchLighting)
        {
            usedObjectWireIndices.insert(object.wireIndex);
            if (object.wireIndex >= nextObjectWireIndex
                && object.wireIndex < std::numeric_limits<std::int32_t>::max())
            {
                nextObjectWireIndex = object.wireIndex + 1;
            }
        }
    }
    for (std::size_t payloadIndex = 0;
         payloadIndex < payload.motsArchLighting.size();
         ++payloadIndex)
    {
        std::int32_t destinationIndex = -1;
        const std::int32_t reusedIndex =
            reusedArchiveLightingIndices[payloadIndex];
        if (reusedIndex >= 0)
        {
            // A same-document duplicate keeps the original authored table
            // slot and therefore preserves sharing between Things.
            destinationIndex = reusedIndex;
        }
        else
        {
            if (staged.motsArchLighting.size() >= 1'000'000
                || staged.motsArchLighting.size()
                    >= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
            {
                return std::nullopt;
            }
            destinationIndex = static_cast<std::int32_t>(staged.motsArchLighting.size());
            MotsArchLightObject object = payload.motsArchLighting[payloadIndex];
            if (!usedObjectWireIndices.insert(object.wireIndex).second)
            {
                while (nextObjectWireIndex < std::numeric_limits<std::int32_t>::max()
                    && usedObjectWireIndices.contains(nextObjectWireIndex))
                {
                    ++nextObjectWireIndex;
                }
                if (nextObjectWireIndex == std::numeric_limits<std::int32_t>::max())
                {
                    return std::nullopt;
                }
                object.wireIndex = nextObjectWireIndex;
                usedObjectWireIndices.insert(nextObjectWireIndex);
                ++nextObjectWireIndex;
            }
            staged.motsArchLighting.push_back(std::move(object));
        }
        archiveLightingMap.push_back(destinationIndex);
    }
    staged.things.reserve(payload.things.size());
    std::vector<std::int32_t> localIndices;
    localIndices.reserve(payload.things.size());
    EntityPasteResult result{};
    result.kind = EntityClipboardKind::things;
    result.indices.reserve(payload.things.size());
    for ( const ThingClipboardEntry& entry : payload.things )
    {
        const auto localIndex = static_cast<std::int32_t>(staged.things.size());
        const auto destinationIndex = static_cast<std::int32_t>(level.things.size() + staged.things.size());
        Thing copy = entry.thing;
        if (copy.archLightingIndex >= 0)
        {
            copy.archLightingIndex = archiveLightingMap[
                static_cast<std::size_t>(copy.archLightingIndex)];
        }
        copy.id = staged.newId();
        copy.num = destinationIndex;
        copy.layer = resolveLayer(staged, entry.layer, result.addedLayers);
        copy.sector = -1;
        staged.things.push_back(std::move(copy));
        localIndices.push_back(localIndex);
        result.indices.push_back(destinationIndex);
    }

    const bool changed = levelTranslateThings(staged, localIndices,
        translation.x, translation.y, translation.z, true, false);
    if ( changed != expectedPoseChange )
    {
        return std::nullopt;
    }
    for ( auto& thing : staged.things )
    {
        thing.sector = levelFindSectorForXYZ(level, thing.x, thing.y, thing.z);
    }

    // Complete allocations before publishing. Only payload owners and layer/ID
    // metadata are staged; the destination's geometry is never copied.
    static_assert(std::is_nothrow_move_constructible_v<Thing>);
    static_assert(std::is_nothrow_constructible_v<std::optional<EntityPasteResult>, EntityPasteResult&&>);
    static_assert(std::is_nothrow_move_constructible_v<std::optional<EntityPasteResult>>);
    level.things.reserve(level.things.size() + staged.things.size());
    for ( auto& thing : staged.things )
    {
        level.things.push_back(std::move(thing));
    }
    if (importsArchiveLighting)
    {
        level.motsArchLighting.swap(staged.motsArchLighting);
    }
    level.layers.swap(staged.layers);
    level.curId = staged.curId;
    return result;
}

std::optional<EntityPasteResult> pasteLights(
    Level& level,
    const LightClipboardPayload& payload,
    const std::optional<TVector>& target)
{
    if (payload.lights.empty())
    {
        return std::nullopt;
    }
    if ( !level.canAllocateIds(payload.lights.size()) )
    {
        return std::nullopt;
    }

    const TVector translation = clipboardTranslation(payload.anchor, target);
    EntityPasteResult result{};
    result.kind = EntityClipboardKind::lights;
    result.indices.reserve(payload.lights.size());

    level.lights.reserve(level.lights.size() + payload.lights.size());
    for (const LightClipboardEntry& entry : payload.lights)
    {
        SedLight copy = entry.light;
        copy.id = level.newId();
        copy.layer = resolveLayer(level, entry.layer, result.addedLayers);
        copy.mark = 0;
        copy.position = copy.position + translation;
        level.lights.push_back(std::move(copy));
        result.indices.push_back(static_cast<std::int32_t>(level.lights.size() - 1));
    }
    return result;
}

} // namespace

EntityClipboardKind entityClipboardKind(const EntityClipboard& clipboard) noexcept
{
    if (std::holds_alternative<SectorClipboardPayload>(clipboard))
    {
        return EntityClipboardKind::sectors;
    }
    if (std::holds_alternative<ThingClipboardPayload>(clipboard))
    {
        return EntityClipboardKind::things;
    }
    if (std::holds_alternative<LightClipboardPayload>(clipboard))
    {
        return EntityClipboardKind::lights;
    }
    return EntityClipboardKind::none;
}

std::size_t entityClipboardCount(const EntityClipboard& clipboard) noexcept
{
    if (const auto* sectors = std::get_if<SectorClipboardPayload>(&clipboard))
    {
        return sectors->sectors.size();
    }
    if (const auto* things = std::get_if<ThingClipboardPayload>(&clipboard))
    {
        return things->things.size();
    }
    if (const auto* lights = std::get_if<LightClipboardPayload>(&clipboard))
    {
        return lights->lights.size();
    }
    return 0;
}

bool entityClipboardRequiresLayerImport(
    const Level& level,
    const EntityClipboard& clipboard) noexcept
{
    auto needsImport = [&level](const ClipboardLayerRef& ref) noexcept
    {
        if (!ref.name.empty())
        {
            return std::find(level.layers.begin(), level.layers.end(), ref.name)
                == level.layers.end();
        }
        return level.layers.empty()
            && (ref.sourceIndex < 0
                || static_cast<std::size_t>(ref.sourceIndex) >= level.layers.size());
    };

    if (const auto* sectors = std::get_if<SectorClipboardPayload>(&clipboard))
    {
        return std::any_of(sectors->layers.begin(), sectors->layers.end(), needsImport);
    }
    if (const auto* things = std::get_if<ThingClipboardPayload>(&clipboard))
    {
        return std::any_of(
            things->things.begin(),
            things->things.end(),
            [&needsImport](const ThingClipboardEntry& entry) { return needsImport(entry.layer); });
    }
    if (const auto* lights = std::get_if<LightClipboardPayload>(&clipboard))
    {
        return std::any_of(
            lights->lights.begin(),
            lights->lights.end(),
            [&needsImport](const LightClipboardEntry& entry) { return needsImport(entry.layer); });
    }
    return false;
}

std::optional<EntityClipboard> copySectorsToClipboard(
    const Level& level,
    const std::span<const std::int32_t> sectorIndices,
    std::int32_t primarySector)
{
    const std::vector<std::int32_t> sources = sortedValidIndices(
        sectorIndices,
        [&level](const std::int32_t index)
        {
            return index >= 0 && static_cast<std::size_t>(index) < level.sectors.size();
        });
    if (sources.empty())
    {
        return std::nullopt;
    }

    if (!std::binary_search(sources.begin(), sources.end(), primarySector))
    {
        primarySector = sources.back();
    }

    std::unordered_map<std::int32_t, std::int32_t> sourceToLocal{};
    sourceToLocal.reserve(sources.size());
    for (std::size_t i = 0; i < sources.size(); ++i)
    {
        sourceToLocal.emplace(sources[i], static_cast<std::int32_t>(i));
    }

    SectorClipboardPayload payload{};
    payload.sourceProject = level.kind;
    payload.uvCoordinateSpace = level.uvCoordinateSpace;
    payload.anchor = sectorCalcCenter(level.sectors[static_cast<std::size_t>(primarySector)]);
    payload.sectors.reserve(sources.size());
    payload.layers.reserve(sources.size());

    for (const std::int32_t sourceIndex : sources)
    {
        Sector copy = level.sectors[static_cast<std::size_t>(sourceIndex)];
        payload.layers.push_back(layerRefFor(level, copy.layer));

        // Store only self-contained relationships, and rewrite their Sector
        // component into payload-local space before the source Level changes.
        for (Surface& surface : copy.surfaces)
        {
            surface.nAdj = -1;
            if (!surface.adjoin)
            {
                continue;
            }
            const auto mapped = sourceToLocal.find(surface.adjoin->sector);
            if (mapped == sourceToLocal.end()
                || surface.adjoin->surface < 0
                || static_cast<std::size_t>(surface.adjoin->surface)
                    >= level.sectors[static_cast<std::size_t>(surface.adjoin->sector)].surfaces.size())
            {
                // A copied portal whose mate is outside the payload becomes a
                // normal solid boundary. Clearing only the reference would
                // leave an invisible/non-colliding portal opening to nowhere.
                surfaceClearAdjoinParams(surface);
                surfaceCheckIfFloor(surface);
                surface.adjoin.reset();
                continue;
            }
            surface.adjoin->sector = mapped->second;
        }
        payload.sectors.push_back(std::move(copy));
    }
    return EntityClipboard{ std::move(payload) };
}

std::optional<EntityClipboard> copyThingsToClipboard(
    const Level& level,
    const std::span<const std::int32_t> thingIndices,
    std::int32_t primaryThing)
{
    const std::vector<std::int32_t> sources = sortedValidIndices(
        thingIndices,
        [&level](const std::int32_t index)
        {
            return index >= 0 && static_cast<std::size_t>(index) < level.things.size();
        });
    if (sources.empty())
    {
        return std::nullopt;
    }
    if (!std::binary_search(sources.begin(), sources.end(), primaryThing))
    {
        primaryThing = sources.back();
    }

    const Thing& primary = level.things[static_cast<std::size_t>(primaryThing)];
    ThingClipboardPayload payload{};
    payload.sourceProject = level.kind;
    payload.anchor = TVector{ primary.x, primary.y, primary.z };
    payload.things.reserve(sources.size());

    std::unordered_set<std::int32_t> referencedArchiveLighting{};
    if (level.kind == ProjectType::MOTS)
    {
        for (const std::int32_t sourceIndex : sources)
        {
            const std::int32_t archiveIndex =
                level.things[static_cast<std::size_t>(sourceIndex)].archLightingIndex;
            if (archiveIndex < -1
                || (archiveIndex >= 0
                    && static_cast<std::size_t>(archiveIndex)
                        >= level.motsArchLighting.size()))
            {
                return std::nullopt;
            }
            if (archiveIndex >= 0)
            {
                referencedArchiveLighting.insert(archiveIndex);
            }
        }

        // Walk the source table, rather than the selection order, so a
        // cross-level paste retains the authored object ordering.  The
        // payload-local Thing indices are filled after this map is complete.
        std::unordered_map<std::int32_t, std::int32_t> sourceToPayload{};
        sourceToPayload.reserve(referencedArchiveLighting.size());
        for (std::size_t sourceArchiveIndex = 0;
             sourceArchiveIndex < level.motsArchLighting.size();
             ++sourceArchiveIndex)
        {
            const std::int32_t sourceIndex = static_cast<std::int32_t>(sourceArchiveIndex);
            if (!referencedArchiveLighting.contains(sourceIndex))
            {
                continue;
            }
            sourceToPayload.emplace(
                sourceIndex,
                static_cast<std::int32_t>(payload.motsArchLighting.size()));
            payload.motsArchLighting.push_back(level.motsArchLighting[sourceArchiveIndex]);
            payload.motsArchLightingSourceIndices.push_back(sourceIndex);
        }

        for (const std::int32_t sourceIndex : sources)
        {
            const std::int32_t archiveIndex =
                level.things[static_cast<std::size_t>(sourceIndex)].archLightingIndex;
            if (archiveIndex < 0)
            {
                continue;
            }
            if (!sourceToPayload.contains(archiveIndex))
            {
                return std::nullopt;
            }
        }
    }
    else if (std::any_of(
                 sources.begin(), sources.end(),
                 [&level](const std::int32_t sourceIndex)
                 {
                     return level.things[static_cast<std::size_t>(sourceIndex)]
                         .archLightingIndex != -1;
                 }))
    {
        return std::nullopt;
    }

    for (const std::int32_t sourceIndex : sources)
    {
        ThingClipboardEntry entry{};
        entry.thing = level.things[static_cast<std::size_t>(sourceIndex)];
        if (entry.thing.archLightingIndex >= 0)
        {
            const auto found = std::find(
                payload.motsArchLightingSourceIndices.begin(),
                payload.motsArchLightingSourceIndices.end(),
                entry.thing.archLightingIndex);
            if (found == payload.motsArchLightingSourceIndices.end())
            {
                return std::nullopt;
            }
            entry.thing.archLightingIndex = static_cast<std::int32_t>(
                std::distance(payload.motsArchLightingSourceIndices.begin(), found));
        }
        entry.layer = layerRefFor(level, entry.thing.layer);
        payload.things.push_back(std::move(entry));
    }
    return EntityClipboard{ std::move(payload) };
}

std::optional<EntityClipboard> copyLightsToClipboard(
    const Level& level,
    const std::span<const std::int32_t> lightIndices,
    std::int32_t primaryLight)
{
    const std::vector<std::int32_t> sources = sortedValidIndices(
        lightIndices,
        [&level](const std::int32_t index)
        {
            return index >= 0 && static_cast<std::size_t>(index) < level.lights.size();
        });
    if (sources.empty())
    {
        return std::nullopt;
    }
    if (!std::binary_search(sources.begin(), sources.end(), primaryLight))
    {
        primaryLight = sources.back();
    }

    LightClipboardPayload payload{};
    payload.sourceProject = level.kind;
    payload.anchor = level.lights[static_cast<std::size_t>(primaryLight)].position;
    payload.lights.reserve(sources.size());
    for (const std::int32_t sourceIndex : sources)
    {
        LightClipboardEntry entry{};
        entry.light = level.lights[static_cast<std::size_t>(sourceIndex)];
        entry.layer = layerRefFor(level, entry.light.layer);
        payload.lights.push_back(std::move(entry));
    }
    return EntityClipboard{ std::move(payload) };
}

std::optional<EntityPasteResult> pasteEntityClipboard(
    Level& level,
    const EntityClipboard& clipboard,
    const std::optional<TVector> target,
    const UvTextureDimensionsResolver& resolveDimensions)
{
    if (const auto* sectors = std::get_if<SectorClipboardPayload>(&clipboard))
    {
        return pasteSectors(level, *sectors, target, resolveDimensions);
    }
    if (const auto* things = std::get_if<ThingClipboardPayload>(&clipboard))
    {
        return pasteThings(level, *things, target);
    }
    if (const auto* lights = std::get_if<LightClipboardPayload>(&clipboard))
    {
        return pasteLights(level, *lights, target);
    }
    return std::nullopt;
}

std::optional<std::vector<std::byte>> encodeEntityClipboard(
    const EntityClipboard& clipboard,
    std::string* const error) noexcept
{
    if (error)
    {
        error->clear();
    }
    try
    {
        const EntityClipboardKind kind = entityClipboardKind(clipboard);
        const std::size_t count = entityClipboardCount(clipboard);
        if (kind == EntityClipboardKind::none || count == 0
            || count > std::numeric_limits<std::uint32_t>::max())
        {
            throw std::runtime_error("Entity clipboard is empty or too large.");
        }

        if (const auto* thingPayload = std::get_if<ThingClipboardPayload>(&clipboard);
            thingPayload && !validThingArchLightingPayload(*thingPayload))
        {
            throw std::runtime_error(
                "Entity clipboard Thing ARCHLIGHTING metadata is inconsistent.");
        }

        Level transport{};
        transport.kind = clipboardSourceProject(clipboard);
        transport.layers.emplace_back("Clipboard");
        if (const auto* sectors = std::get_if<SectorClipboardPayload>(&clipboard))
        {
            if (sectors->layers.size() != sectors->sectors.size())
            {
                throw std::runtime_error("Entity clipboard Sector layer metadata is inconsistent.");
            }
            transport.sectors = sectors->sectors;
            transport.uvCoordinateSpace = sectors->uvCoordinateSpace;
            for (Sector& sector : transport.sectors)
            {
                sector.layer = 0;
            }
        }
        else if (const auto* things = std::get_if<ThingClipboardPayload>(&clipboard))
        {
            transport.motsArchLighting = things->motsArchLighting;
            transport.things.reserve(things->things.size());
            for (const ThingClipboardEntry& entry : things->things)
            {
                Thing thing = entry.thing;
                thing.layer = 0;
                thing.sector = -1;
                transport.things.push_back(std::move(thing));
            }
        }
        else if (const auto* lights = std::get_if<LightClipboardPayload>(&clipboard))
        {
            transport.lights.reserve(lights->lights.size());
            for (const LightClipboardEntry& entry : lights->lights)
            {
                SedLight light = entry.light;
                light.layer = 0;
                transport.lights.push_back(std::move(light));
            }
        }

        std::vector<std::byte> metadata{};
        metadata.reserve(64 + count * 16);
        metadata.insert(metadata.end(),
            kClipboardMetadataMagic.begin(), kClipboardMetadataMagic.end());
        appendU32(metadata, kClipboardMetadataVersion);
        appendU32(metadata, static_cast<std::uint32_t>(kind));
        const TVector anchor = clipboardAnchor(clipboard);
        appendDouble(metadata, anchor.x);
        appendDouble(metadata, anchor.y);
        appendDouble(metadata, anchor.z);
        const std::vector<ClipboardLayerRef> refs =
            clipboardLayerRefs(clipboard);
        if (refs.size() != count)
        {
            throw std::runtime_error("Entity clipboard layer metadata is inconsistent.");
        }
        appendU32(metadata, static_cast<std::uint32_t>(refs.size()));
        for (const ClipboardLayerRef& ref : refs)
        {
            appendLayerRef(metadata, ref);
            if (metadata.size() > kMaximumClipboardMetadataBytes)
            {
                throw std::runtime_error("Entity clipboard metadata is too large.");
            }
        }

        const assets::SedOptionalSection section{
            kClipboardSectionType, std::move(metadata) };
        return assets::saveToSedBytes(
            transport,
            std::span<const assets::SedOptionalSection>(&section, 1),
            assets::SedSerializationProfile::ProtectedV1,
            0);
    }
    catch (const std::exception& exception)
    {
        setClipboardError(error, exception.what());
    }
    catch (...)
    {
        setClipboardError(error, "Entity clipboard encoding failed.");
    }
    return std::nullopt;
}

std::optional<EntityClipboard> decodeEntityClipboard(
    const std::span<const std::byte> bytes,
    std::string* const error) noexcept
{
    if (error)
    {
        error->clear();
    }
    try
    {
        if (bytes.empty() || bytes.size() > assets::SedMaximumFileBytes)
        {
            throw std::runtime_error("Entity clipboard data is empty or too large.");
        }
        assets::SedFile file = assets::loadSedFileBytes(bytes);
        const assets::SedOptionalSection* metadataSection = nullptr;
        for (const assets::SedOptionalSection& section : file.optionalSections)
        {
            if (section.type == kClipboardSectionType)
            {
                if (metadataSection)
                {
                    throw std::runtime_error("Entity clipboard contains duplicate metadata.");
                }
                metadataSection = &section;
            }
        }
        if (!metadataSection
            || metadataSection->payload.size() > kMaximumClipboardMetadataBytes)
        {
            throw std::runtime_error("Entity clipboard metadata is missing or too large.");
        }

        ClipboardMetadataReader reader(metadataSection->payload);
        const auto magic = reader.take(kClipboardMetadataMagic.size());
        if (!std::equal(magic.begin(), magic.end(),
                kClipboardMetadataMagic.begin()))
        {
            throw std::runtime_error("Entity clipboard metadata signature is invalid.");
        }
        if (reader.u32() != kClipboardMetadataVersion)
        {
            throw std::runtime_error("Entity clipboard version is not supported.");
        }
        const auto kind = static_cast<EntityClipboardKind>(reader.u32());
        if (kind != EntityClipboardKind::sectors
            && kind != EntityClipboardKind::things
            && kind != EntityClipboardKind::lights)
        {
            throw std::runtime_error("Entity clipboard kind is invalid.");
        }
        const TVector anchor{ reader.f64(), reader.f64(), reader.f64() };
        if (!std::isfinite(anchor.x) || !std::isfinite(anchor.y)
            || !std::isfinite(anchor.z))
        {
            throw std::runtime_error("Entity clipboard anchor is invalid.");
        }

        const std::uint32_t layerCount = reader.u32();
        if (layerCount > assets::SedMaximumDecodedItems)
        {
            throw std::runtime_error("Entity clipboard layer count is too large.");
        }
        std::vector<ClipboardLayerRef> refs{};
        refs.reserve(layerCount);
        for (std::uint32_t index = 0; index < layerCount; ++index)
        {
            refs.push_back(ClipboardLayerRef{ reader.i32(), reader.string() });
        }
        if (!reader.empty())
        {
            throw std::runtime_error("Entity clipboard metadata has trailing data.");
        }

        if (kind == EntityClipboardKind::sectors)
        {
            if (file.level.sectors.empty()
                || file.level.sectors.size() != refs.size()
                || !file.level.things.empty() || !file.level.lights.empty())
            {
                throw std::runtime_error("Entity clipboard Sector payload is inconsistent.");
            }
            SectorClipboardPayload payload{};
            payload.sourceProject = file.level.kind;
            payload.uvCoordinateSpace = file.level.uvCoordinateSpace;
            payload.sectors = std::move(file.level.sectors);
            payload.layers = std::move(refs);
            payload.anchor = anchor;
            return EntityClipboard{ std::move(payload) };
        }
        if (kind == EntityClipboardKind::things)
        {
            if (file.level.things.empty()
                || file.level.things.size() != refs.size()
                || !file.level.sectors.empty() || !file.level.lights.empty())
            {
                throw std::runtime_error("Entity clipboard Thing payload is inconsistent.");
            }
            ThingClipboardPayload payload{};
            payload.sourceProject = file.level.kind;
            payload.anchor = anchor;
            payload.things.reserve(file.level.things.size());
            for (std::size_t index = 0; index < file.level.things.size(); ++index)
            {
                payload.things.push_back(ThingClipboardEntry{
                    std::move(file.level.things[index]), std::move(refs[index]) });
            }
            payload.motsArchLighting = std::move(file.level.motsArchLighting);
            payload.motsArchLightingSourceIndices.assign(
                payload.motsArchLighting.size(), -1);
            if (!validThingArchLightingPayload(payload))
            {
                throw std::runtime_error(
                    "Entity clipboard Thing ARCHLIGHTING metadata is inconsistent.");
            }
            return EntityClipboard{ std::move(payload) };
        }

        if (file.level.lights.empty()
            || file.level.lights.size() != refs.size()
            || !file.level.sectors.empty() || !file.level.things.empty())
        {
            throw std::runtime_error("Entity clipboard light payload is inconsistent.");
        }
        LightClipboardPayload payload{};
        payload.sourceProject = file.level.kind;
        payload.anchor = anchor;
        payload.lights.reserve(file.level.lights.size());
        for (std::size_t index = 0; index < file.level.lights.size(); ++index)
        {
            payload.lights.push_back(LightClipboardEntry{
                std::move(file.level.lights[index]), std::move(refs[index]) });
        }
        return EntityClipboard{ std::move(payload) };
    }
    catch (const std::exception& exception)
    {
        setClipboardError(error, exception.what());
    }
    catch (...)
    {
        setClipboardError(error, "Entity clipboard decoding failed.");
    }
    return std::nullopt;
}

} // namespace sed::level
