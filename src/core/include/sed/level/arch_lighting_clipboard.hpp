#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "sed/level/arch_lighting.hpp"

namespace sed::assets
{
class Templates;
} // namespace sed::assets

namespace sed::level
{

/** Bounds shared by the in-process and binary ARCHLIGHTING clipboard paths. */
inline constexpr std::size_t kMotsArchLightingClipboardMaxItems = 1'000'000u;
inline constexpr std::size_t kMotsArchLightingClipboardMaxBytes = 16u * 1024u * 1024u;
inline constexpr std::size_t kMotsArchLightingClipboardMaxIdentityBytes = 4096u;

/** Value-owned ARCHLIGHTING data that can outlive its source Level. */
struct MotsArchLightingClipboardPayload
{
    std::string modelIdentity{};
    MotsArchLightingTopology topology{};
    MotsArchLightObject object{};
};

/** Resource facts for one selected paste destination. */
struct MotsArchLightingClipboardTarget
{
    std::int32_t thingIndex{ -1 };
    std::string modelIdentity{};
    MotsArchLightingTopology topology{};
};

enum class MotsArchLightingClipboardStatus : std::uint8_t
{
    Applied,
    Noop,
    Rejected,
};

struct MotsArchLightingClipboardResult
{
    MotsArchLightingClipboardStatus status{
        MotsArchLightingClipboardStatus::Rejected };
    std::string message{};
    std::vector<std::int32_t> changedThingIndices{};
    std::int32_t recordIndex{ -1 };
};

/**
 * Canonicalizes a model identity for same-model matching. Leading/trailing
 * whitespace is removed, '\\' becomes '/', and ASCII letters are lowercased;
 * content/fingerprint suffixes remain part of the identity.
 */
[[nodiscard]] std::string normalizeMotsArchLightingModelIdentity(
    std::string_view identity);

/** Copies one bound source record without retaining a Level reference. */
[[nodiscard]] std::optional<MotsArchLightingClipboardPayload>
copyMotsArchLightingClipboard(
    const Level& level,
    std::int32_t sourceThingIndex,
    std::string_view modelIdentity,
    const MotsArchLightingTopology& topology,
    const assets::Templates* templates = nullptr,
    std::string* error = nullptr);

/**
 * Encodes a bounded, little-endian, bit-exact ARCHLIGHTING payload.
 *
 * The current version-1 layout is:
 * `SEDARCH\\0`, u32 version, u32 identity-byte-count, identity bytes, u32
 * geoset count, repeated u32 mesh-prefix count plus u32 vertex counts, i32
 * object wire index, u32 mesh count, and repeated i32 mesh wire index, u32
 * vertex count, followed by four u32 IEEE-754 float bit patterns per vertex.
 * Identity is limited to 4096 bytes; geoset, mesh, and vertex totals are each
 * bounded by `kMotsArchLightingClipboardMaxItems`; the complete payload is
 * bounded by `kMotsArchLightingClipboardMaxBytes`. Extra or truncated bytes
 * are rejected by the decoder.
 */
[[nodiscard]] std::optional<std::vector<std::byte>>
encodeMotsArchLightingClipboard(
    const MotsArchLightingClipboardPayload& payload,
    std::string* error = nullptr) noexcept;

/** Strictly decodes and validates an ARCHLIGHTING clipboard payload. */
[[nodiscard]] std::optional<MotsArchLightingClipboardPayload>
decodeMotsArchLightingClipboard(
    std::span<const std::byte> bytes,
    std::string* error = nullptr) noexcept;

/**
 * Binds all compatible selected model Things to one reused or newly appended
 * record. An eligible unflagged instance receives a local Arch-flag promotion
 * in the same failure-atomic operation. Every target is validated before the
 * Level is changed.
 */
[[nodiscard]] MotsArchLightingClipboardResult pasteMotsArchLightingClipboard(
    Level& level,
    const MotsArchLightingClipboardPayload& payload,
    std::span<const MotsArchLightingClipboardTarget> targets,
    const assets::Templates* templates = nullptr,
    std::string* error = nullptr);

} // namespace sed::level
