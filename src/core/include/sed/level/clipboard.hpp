#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "sed/level.hpp"
#include "sed/level/uv.hpp"

namespace sed::level
{

/** Identifies the one entity family carried by the editor clipboard. */
enum class EntityClipboardKind : std::uint8_t
{
    none,
    sectors,
    things,
    lights,
};

/** Stable layer identity used when a clipboard survives a document change. */
struct ClipboardLayerRef
{
    std::int32_t sourceIndex{ -1 };
    std::string name{};
};

/** Sector payload whose adjoin sector indices are local to `sectors`. */
struct SectorClipboardPayload
{
    ProjectType sourceProject{ ProjectType::JKDF2 };
    UvCoordinateSpace uvCoordinateSpace{ UvCoordinateSpace::Normalized };
    std::vector<Sector> sectors{};
    std::vector<ClipboardLayerRef> layers{};
    TVector anchor{};
};

struct ThingClipboardEntry
{
    Thing thing{};
    ClipboardLayerRef layer{};
};

struct ThingClipboardPayload
{
    ProjectType sourceProject{ ProjectType::JKDF2 };
    std::vector<ThingClipboardEntry> things{};
    // ARCHLIGHTING objects referenced by the copied Things.  The parallel
    // source-index vector records each object's source Level ordinal so a
    // same-document duplicate can reuse that table slot; entries' Thing
    // indices are local to this payload and are remapped on paste.
    std::vector<MotsArchLightObject> motsArchLighting{};
    std::vector<std::int32_t> motsArchLightingSourceIndices{};
    TVector anchor{};
};

struct LightClipboardEntry
{
    SedLight light{};
    ClipboardLayerRef layer{};
};

struct LightClipboardPayload
{
    ProjectType sourceProject{ ProjectType::JKDF2 };
    std::vector<LightClipboardEntry> lights{};
    TVector anchor{};
};

/**
 * In-process, value-owning clipboard. It deliberately contains no Level,
 * Selection, QObject, or raw-pointer references, so document replacement and
 * vector reallocation cannot leave a dangling payload.
 */
using EntityClipboard = std::variant<
    std::monostate,
    SectorClipboardPayload,
    ThingClipboardPayload,
    LightClipboardPayload>;

struct EntityPasteResult
{
    EntityClipboardKind kind{ EntityClipboardKind::none };
    std::vector<std::int32_t> indices{};
    bool addedLayers{};
};

[[nodiscard]] EntityClipboardKind entityClipboardKind(const EntityClipboard& clipboard) noexcept;
[[nodiscard]] std::size_t entityClipboardCount(const EntityClipboard& clipboard) noexcept;
/** Whether Paste must append at least one named/default layer to this Level. */
[[nodiscard]] bool entityClipboardRequiresLayerImport(
    const Level& level,
    const EntityClipboard& clipboard) noexcept;

/** Copies valid, sorted, unique Sector indices and preserves only internal adjoins. */
[[nodiscard]] std::optional<EntityClipboard> copySectorsToClipboard(
    const Level& level,
    std::span<const std::int32_t> sectorIndices,
    std::int32_t primarySector = -1);

/** Copies valid, sorted, unique Things including values, frames, and stable layer identity. */
[[nodiscard]] std::optional<EntityClipboard> copyThingsToClipboard(
    const Level& level,
    std::span<const std::int32_t> thingIndices,
    std::int32_t primaryThing = -1);

/** Copies valid, sorted, unique editor/baked lights. */
[[nodiscard]] std::optional<EntityClipboard> copyLightsToClipboard(
    const Level& level,
    std::span<const std::int32_t> lightIndices,
    std::int32_t primaryLight = -1);

/**
 * Appends the clipboard to `level` and returns the new vector indices.
 *
 * A target translates the payload anchor to that world position (normal
 * Paste). `nullopt` keeps authored coordinates unchanged (Paste in Place).
 * Sector-local adjoins are remapped to the appended Sector range; external
 * source adjoins remain severed. Thing frames move with their Thing owner.
 */
[[nodiscard]] std::optional<EntityPasteResult> pasteEntityClipboard(
    Level& level,
    const EntityClipboard& clipboard,
    std::optional<TVector> target = std::nullopt,
    const UvTextureDimensionsResolver& resolveDimensions = {});

/**
 * Encodes an entity clipboard as a bounded, checksummed native SED payload.
 * The returned bytes contain no QObject or process-local address and can be
 * placed on the operating-system clipboard for another SED instance.
 */
[[nodiscard]] std::optional<std::vector<std::byte>> encodeEntityClipboard(
    const EntityClipboard& clipboard,
    std::string* error = nullptr) noexcept;

/** Strictly validates and decodes bytes produced by encodeEntityClipboard. */
[[nodiscard]] std::optional<EntityClipboard> decodeEntityClipboard(
    std::span<const std::byte> bytes,
    std::string* error = nullptr) noexcept;

} // namespace sed::level
