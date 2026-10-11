#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "sed/level.hpp"

namespace sed::assets
{
class Templates;
} // namespace sed::assets

namespace sed::level
{

/** Engine vertex-color path used by a MotS ARCHLIGHTING record. */
enum class MotsArchLightingVertexMode : std::uint8_t
{
    Mono,
    Rgb,
};

/** Mesh vertex counts for every model geoset used by the renderer. */
struct MotsArchLightingTopology
{
    std::vector<std::vector<std::size_t>> geosets{};
};

/** One sparse SetVertex payload used by the interactive ARCH editor. */
struct MotsArchLightingVertexEdit
{
    std::size_t meshIndex{};
    std::size_t vertexIndex{};
    MotsArchLightVertex vertexValue{};
    // Unticked channels retain each destination owner's authored value. This
    // is required when a multi-owner popup starts with mixed HDR/signed-zero
    // payloads and the user changes only one field.
    std::array<bool, 4> channels{ true, true, true, true };
    // -1 applies this sparse edit to every selected owner (the legacy
    // single-value behavior). A non-negative value scopes it to one Thing so
    // selections with different sparse vertex sets cannot cross-apply.
    std::int32_t thingIndex{ -1 };
};

enum class MotsArchLightingEditKind : std::uint8_t
{
    Initialize,
    Multiply,
    SetVertex,
    Reset,
    // Source compatibility for older callers; deleting an ARCH edit now
    // resets the bound record to the engine-authentic all-zero default.
    Remove = Reset,
};

/** One bounded MotS ARCHLIGHTING authoring operation. */
struct MotsArchLightingEdit
{
    MotsArchLightingEditKind kind{ MotsArchLightingEditKind::Multiply };
    MotsArchLightingVertexMode mode{ MotsArchLightingVertexMode::Rgb };
    float monoMultiplier{ 1.0f };
    ColorF rgbMultiplier{ ColorWhite };
    std::size_t meshIndex{};
    std::size_t vertexIndex{};
    MotsArchLightVertex vertexValue{};
    std::vector<MotsArchLightingVertexEdit> vertexEdits{};
};

enum class MotsArchLightingEditStatus : std::uint8_t
{
    Applied,
    Noop,
    Rejected,
};

struct MotsArchLightingEditResult
{
    MotsArchLightingEditStatus status{ MotsArchLightingEditStatus::Rejected };
    std::string message{};
    std::vector<std::int32_t> changedThingIndices{};
};

/** True when this MotS Thing's effective template/instance flags mark it as
 * the Arch flag (0x8). A bound ARCH record can remain dormant while this is false. */
bool isMotsArchThing(
    const Level& level,
    const Thing& thing,
    const assets::Templates* templates = nullptr) noexcept;

/** Returns the record used by this Arch Thing, or nullptr for the virtual
 * all-zero default / an inactive or invalid reference. */
const MotsArchLightObject* findActiveMotsArchLighting(
    const Level& level,
    const Thing& thing,
    const assets::Templates* templates = nullptr) noexcept;

/** Exact all-channel zero test used by the MotS default-record export option. */
bool isDefaultMotsArchLighting(const MotsArchLightObject& object) noexcept;

/** Exact maximum mesh/vertex shape check used before dormant data is rebound. */
bool matchesMotsArchLightingTopology(
    const MotsArchLightObject& object,
    const MotsArchLightingTopology& topology);

/**
 * One destination in a MotS ARCHLIGHTING copy operation.
 *
 * The application supplies the resolved 3DO identity and all selected LOD
 * prefixes from its immutable resource snapshot.  Keeping those facts at the
 * core boundary prevents a copy from silently crossing model resources or
 * relying on only the currently displayed LOD.
 */
struct MotsArchLightingCopyTarget
{
    std::int32_t thingIndex{ -1 };
    std::string modelIdentity{};
    MotsArchLightingTopology topology{};
};

/**
 * Applies one MotS-only ARCHLIGHTING edit after validating every target and
 * every model geoset. The operation must be failure-atomic.
 */
MotsArchLightingEditResult applyMotsArchLightingEdit(
    Level& level,
    std::span<const std::int32_t> thingIndices,
    std::span<const MotsArchLightingTopology> topologies,
    const MotsArchLightingEdit& edit,
    const assets::Templates* templates = nullptr);

/**
 * Copies one bound source record to compatible selected MotS model Things.
 * An eligible unflagged instance receives a local Arch-flag promotion in the
 * same failure-atomic operation. The operation validates the source and every
 * target before publishing anything, and returns Noop for an empty/self-only
 * destination set. Later value edits retain the existing destination/source
 * copy-on-write behavior.
 */
MotsArchLightingEditResult copyMotsArchLighting(
    Level& level,
    std::int32_t sourceThingIndex,
    std::string_view sourceModelIdentity,
    const MotsArchLightingTopology& sourceTopology,
    std::span<const MotsArchLightingCopyTarget> targets,
    const assets::Templates* templates = nullptr);

} // namespace sed::level
