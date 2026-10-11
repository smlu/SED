#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include "sed/level.hpp"
#include "sed/level/arch_lighting.hpp"

namespace sed::assets
{
class Templates;
} // namespace sed::assets

namespace sed::level
{

void levelZeroVertexLights(Level& level, std::span<const std::int32_t> sectorIndices) noexcept;
void levelCalcSectorAmbients(Level& level, std::span<const std::int32_t> sectorIndices) noexcept;
void levelCalcLighting(Level& level, std::span<const std::int32_t> sectorIndices) noexcept;
void levelCalcLightingNew(Level& level, std::span<const std::int32_t> sectorIndices) noexcept;

using LightingProgressCallback = std::function<bool(std::size_t completed, std::size_t total)>;

/**
 * World-space region whose baked-light contribution may have changed.
 *
 * Passing one or more regions lets an automatic bake preserve texture-vertex
 * colors outside the edited lights' old/new ranges while fully recomputing
 * every current light contribution inside them.
 */
struct LightingInfluenceSphere
{
    TVector center{};
    double radius{};
};

/** One transformed render-mesh instance from one MotS model LOD. */
struct MotsArchLightingBakeMesh
{
    std::size_t sourceMeshIndex{};
    std::vector<TVector> worldVertices{};
};

/** All rendered mesh instances in one alternative MotS model LOD. */
struct MotsArchLightingBakeGeoset
{
    std::vector<MotsArchLightingBakeMesh> meshes{};
};

/**
 * Immutable app-resolved geometry for one eligible MotS Arch Thing. Mesh
 * vertices are already in world space; `sourceMeshIndex` maps hierarchy
 * instances back to ARCHLIGHTING's shared source-mesh arrays. `topology` and
 * `lods` must describe the same all-LOD vertex prefixes. One source mesh may
 * appear at most once in each LOD because the engine has no per-instance
 * ARCHLIGHTING array.
 */
struct MotsArchLightingBakeTarget
{
    std::int32_t thingIndex{ -1 };
    std::int32_t thingId{};
    std::int32_t sectorIndex{ -1 };
    std::string thingName{};
    std::string modelName{};
    MotsArchLightingTopology topology{};
    std::vector<MotsArchLightingBakeGeoset> lods{};
};

enum class MotsArchLightingBakeStatus : std::uint8_t
{
    Applied,
    Noop,
    Rejected,
    Cancelled,
};

struct MotsArchLightingBakeResult
{
    MotsArchLightingBakeStatus status{ MotsArchLightingBakeStatus::Rejected };
    std::string message{};
    std::vector<std::int32_t> changedThingIndices{};
    bool provenanceChanged{};
};

/**
 * Recomputes generated MotS ARCH values from each Thing's preserved authored
 * baseline plus SedLights marked LF_AffectArchThings. The caller resolves all
 * resources before dispatch. Validation and publication are failure-atomic.
 * With no flagged source lights, baselines are restored and deleted owners
 * pruned without geometry targets. Across alternative LODs, light
 * contributions for a shared source vertex are component-wise maximized;
 * contributions from lights within one LOD sum.
 */
[[nodiscard]] MotsArchLightingBakeResult levelCalcMotsArchLighting(
    Level& level,
    std::span<const MotsArchLightingBakeTarget> targets,
    const assets::Templates* templates = nullptr,
    const LightingProgressCallback& progress = {});

/**
 * Runs the new lighting pass once for the entire target set. The callback is
 * invoked at bounded work checkpoints and may return false to stop without
 * publishing the partial result. An empty dirty-influence span performs the
 * traditional full target-sector bake.
 */
[[nodiscard]] bool levelCalcLightingNewInterruptible(
    Level& level,
    std::span<const std::int32_t> sectorIndices,
    const LightingProgressCallback& progress,
    std::span<const LightingInfluenceSphere> dirtyInfluences = {});

} // namespace sed::level
