#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

#include "sed/level/lighting.hpp"

namespace sed::ui::lighting
{

enum class ManualLightBakeScope : std::uint8_t
{
    selectedLightArea,
    selectedSectors,
    visibleSectors,
    allSectors,
};

/** Stable, past-tense history label captured when a manual bake is launched. */
[[nodiscard]] constexpr std::string_view lightBakeUndoLabel(
    const ManualLightBakeScope scope) noexcept
{
    switch (scope)
    {
        case ManualLightBakeScope::selectedLightArea:
            return "Baked Lighting: Selected area recalculated";
        case ManualLightBakeScope::selectedSectors:
            return "Baked Lighting: Selected sectors recalculated";
        case ManualLightBakeScope::visibleSectors:
            return "Baked Lighting: Visible sectors recalculated";
        case ManualLightBakeScope::allSectors:
            return "Baked Lighting: All sectors recalculated";
    }
    return "Baked Lighting: Affected area recalculated";
}

/**
 * Tracks changes to inputs used by the baked-lighting algorithm.
 *
 * The first observation establishes a baseline. Later distinct fingerprints
 * advance the generation, which lets asynchronous jobs reject stale results.
 */
class LightBakeChangeTracker
{
public:
    bool observe(const std::uint64_t fingerprint) noexcept
    {
        if (!initialized_)
        {
            initialized_ = true;
            fingerprint_ = fingerprint;
            return false;
        }
        if (fingerprint_ == fingerprint)
        {
            return false;
        }

        fingerprint_ = fingerprint;
        ++generation_;
        return true;
    }

    void reset() noexcept
    {
        initialized_ = false;
        fingerprint_ = 0;
        generation_ = 0;
    }

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] std::uint64_t fingerprint() const noexcept { return fingerprint_; }
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }

private:
    bool initialized_{};
    std::uint64_t fingerprint_{};
    std::uint64_t generation_{};
};

/** Coalesces requests arriving while one bake is already running. */
class LightBakeRequestCoalescer
{
public:
    [[nodiscard]] bool request(const bool running) noexcept
    {
        if (!running)
        {
            return true;
        }
        pending_ = true;
        return false;
    }

    [[nodiscard]] bool takePending() noexcept
    {
        const bool pending = pending_;
        pending_ = false;
        return pending;
    }

    void clear() noexcept { pending_ = false; }
    [[nodiscard]] bool pending() const noexcept { return pending_; }

private:
    bool pending_{};
};

[[nodiscard]] inline int lightBakeProgressPercent(
    const std::size_t completed,
    const std::size_t total) noexcept
{
    if (total == 0)
    {
        return 100;
    }
    const std::size_t bounded = std::min(completed, total);
    return static_cast<int>((bounded * 100u) / total);
}

/** Counts usable light candidates whose ranges intersect at least one baked target. */
[[nodiscard]] inline std::size_t processedBakedLightCount(
    const Level& bakedLevel,
    const std::span<const std::int32_t> targetSectors)
{
    std::vector<std::array<double, 6>> targetBounds;
    targetBounds.reserve(targetSectors.size());
    for (const std::int32_t sectorIndex : targetSectors)
    {
        if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= bakedLevel.sectors.size())
        {
            continue;
        }
        const Sector& sector = bakedLevel.sectors[static_cast<std::size_t>(sectorIndex)];
        if (sector.vertices.empty())
        {
            continue;
        }

        std::array<double, 6> bounds{
            sector.vertices.front().x,
            sector.vertices.front().y,
            sector.vertices.front().z,
            sector.vertices.front().x,
            sector.vertices.front().y,
            sector.vertices.front().z,
        };
        for (const Vertex& vertex : sector.vertices)
        {
            bounds[0] = std::min(bounds[0], vertex.x);
            bounds[1] = std::min(bounds[1], vertex.y);
            bounds[2] = std::min(bounds[2], vertex.z);
            bounds[3] = std::max(bounds[3], vertex.x);
            bounds[4] = std::max(bounds[4], vertex.y);
            bounds[5] = std::max(bounds[5], vertex.z);
        }
        targetBounds.push_back(bounds);
    }

    return static_cast<std::size_t>(std::count_if(
        bakedLevel.lights.begin(),
        bakedLevel.lights.end(),
        [&bakedLevel, &targetBounds](const SedLight& light)
        {
            if (light.mark < 0
                || static_cast<std::size_t>(light.mark) >= bakedLevel.sectors.size()
                || light.range <= 0.0
                || light.intensity <= 0.0)
            {
                return false;
            }

            const double rangeSquared = light.range * light.range;
            return std::any_of(targetBounds.begin(), targetBounds.end(), [&light, rangeSquared](const auto& bounds)
                {
                    const double nearestX = std::clamp(light.position.x, bounds[0], bounds[3]);
                    const double nearestY = std::clamp(light.position.y, bounds[1], bounds[4]);
                    const double nearestZ = std::clamp(light.position.z, bounds[2], bounds[5]);
                    const double dx = light.position.x - nearestX;
                    const double dy = light.position.y - nearestY;
                    const double dz = light.position.z - nearestZ;
                    return (dx * dx) + (dy * dy) + (dz * dz) < rangeSquared;
                });
        }));
}

/**
 * Returns whether two lights have identical baked-lighting inputs.
 *
 * `mark` is deliberately ignored: it is scratch state populated by the core
 * lighting pass, not an editable light property.
 */
[[nodiscard]] inline bool lightBakeLightSourceEqual(
    const SedLight& lhs,
    const SedLight& rhs) noexcept
{
    return lhs.id == rhs.id
        && lhs.flags == rhs.flags
        && lhs.color.r == rhs.color.r
        && lhs.color.g == rhs.color.g
        && lhs.color.b == rhs.color.b
        && lhs.color.a == rhs.color.a
        && lhs.intensity == rhs.intensity
        && lhs.range == rhs.range
        && lhs.position.x == rhs.position.x
        && lhs.position.y == rhs.position.y
        && lhs.position.z == rhs.position.z;
}

[[nodiscard]] inline bool lightBakeLightSourcesEqual(
    const std::span<const SedLight> lhs,
    const std::span<const SedLight> rhs) noexcept
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    // Light IDs are stable across ordinary edits. Match by ID so harmless
    // storage reordering does not schedule a bake.
    for (const SedLight& light : lhs)
    {
        const auto match = std::find_if(rhs.begin(), rhs.end(), [&light](const SedLight& candidate)
            {
                return candidate.id == light.id;
            });
        if (match == rhs.end() || !lightBakeLightSourceEqual(light, *match))
        {
            return false;
        }
    }
    return true;
}

using LightBakeInfluenceSphere = ::sed::level::LightingInfluenceSphere;

[[nodiscard]] inline bool lightBakeSphereContains(
    const LightBakeInfluenceSphere& outer,
    const LightBakeInfluenceSphere& inner) noexcept
{
    if (!(inner.radius >= 0.0)
        || !(outer.radius >= inner.radius)
        || !std::isfinite(outer.radius)
        || !std::isfinite(inner.radius))
    {
        return false;
    }
    const double remainingRadius = outer.radius - inner.radius;
    return outer.center.DistanceSqr(inner.center) <= remainingRadius * remainingRadius;
}

namespace detail
{

[[nodiscard]] inline bool isActiveLightBakeSource(const SedLight& light) noexcept
{
    // Keep a positive range even when intensity is currently disabled. A
    // color/intensity/flags edit is still a baked-light edit, and retaining its
    // footprint makes request coalescing and stale-job replacement robust.
    return light.range > 0.0
        && std::isfinite(light.range)
        && std::isfinite(light.position.x)
        && std::isfinite(light.position.y)
        && std::isfinite(light.position.z);
}

inline void appendLightInfluence(
    std::vector<LightBakeInfluenceSphere>& influences,
    const SedLight& light)
{
    if (!isActiveLightBakeSource(light))
    {
        return;
    }
    influences.push_back(LightBakeInfluenceSphere{ light.position, light.range });
}

[[nodiscard]] inline const SedLight* findLightById(
    const std::span<const SedLight> lights,
    const std::int32_t id) noexcept
{
    const auto match = std::find_if(lights.begin(), lights.end(), [id](const SedLight& light)
        {
            return light.id == id;
        });
    return match == lights.end() ? nullptr : std::addressof(*match);
}

} // namespace detail

/** Appends influence spheres while removing duplicate or fully contained regions. */
inline void mergeLightBakeInfluences(
    std::vector<LightBakeInfluenceSphere>& destination,
    const std::span<const LightBakeInfluenceSphere> additions)
{
    for (const LightBakeInfluenceSphere& addition : additions)
    {
        const bool alreadyCovered = std::any_of(
            destination.begin(),
            destination.end(),
            [&addition](const LightBakeInfluenceSphere& existing)
            {
                return lightBakeSphereContains(existing, addition);
            });
        if (!alreadyCovered)
        {
            std::erase_if(destination, [&addition](const LightBakeInfluenceSphere& existing)
                {
                    return lightBakeSphereContains(addition, existing);
                });
            destination.push_back(addition);
        }
    }
}

/**
 * Computes the conservative area affected by a light edit.
 *
 * Both the old and new influence spheres are retained so moving, shrinking,
 * disabling, or deleting a light also clears its former area. Unchanged light
 * footprints are deliberately not expanded into the dirty area: the core
 * partial pass includes every current light while recomputing dirty vertices,
 * so their contributions are preserved without rebaking their other sectors.
 */
[[nodiscard]] inline std::vector<LightBakeInfluenceSphere> lightBakeChangedInfluences(
    const std::span<const SedLight> previous,
    const std::span<const SedLight> current)
{
    std::vector<LightBakeInfluenceSphere> affected{};
    auto appendChangedPair = [&affected](const SedLight* oldLight, const SedLight* newLight)
    {
        if (oldLight)
        {
            detail::appendLightInfluence(affected, *oldLight);
        }
        if (newLight)
        {
            detail::appendLightInfluence(affected, *newLight);
        }
    };

    for (const SedLight& oldLight : previous)
    {
        const SedLight* newLight = detail::findLightById(current, oldLight.id);
        if (!newLight || !lightBakeLightSourceEqual(oldLight, *newLight))
        {
            appendChangedPair(&oldLight, newLight);
        }
    }
    for (const SedLight& newLight : current)
    {
        if (!detail::findLightById(previous, newLight.id))
        {
            appendChangedPair(nullptr, &newLight);
        }
    }

    std::vector<LightBakeInfluenceSphere> uniqueAffected{};
    uniqueAffected.reserve(affected.size());
    mergeLightBakeInfluences(uniqueAffected, affected);
    return uniqueAffected;
}

/**
 * Returns the current and previously observed influence of selected Baked
 * lights. Selection stores current vector indices, while stable light IDs are
 * used to recover each old footprint after a move, shrink, or disable edit.
 *
 * The resulting spheres define only the destination region. The bake itself
 * must still evaluate every current light for those vertices.
 */
[[nodiscard]] inline std::vector<LightBakeInfluenceSphere>
lightBakeSelectedInfluences(
    const std::span<const SedLight> previous,
    const std::span<const SedLight> current,
    const std::span<const std::int32_t> selectedIndices)
{
    std::vector<LightBakeInfluenceSphere> affected{};
    affected.reserve(selectedIndices.size() * 2u);
    for (const std::int32_t selectedIndex : selectedIndices)
    {
        if (selectedIndex < 0
            || static_cast<std::size_t>(selectedIndex) >= current.size())
        {
            continue;
        }

        const SedLight& selected = current[static_cast<std::size_t>(selectedIndex)];
        std::vector<LightBakeInfluenceSphere> pair{};
        if (const SedLight* oldLight = detail::findLightById(previous, selected.id))
        {
            detail::appendLightInfluence(pair, *oldLight);
        }
        detail::appendLightInfluence(pair, selected);
        mergeLightBakeInfluences(affected, pair);
    }
    return affected;
}

/**
 * Resolves a Selected command without consuming caller-owned pending state.
 * A queued click-time footprint remains authoritative even if the live
 * selection is empty by the time an earlier worker exits. Pending automatic
 * footprints are appended only after a real current/captured selected scope
 * exists, so clicking a retained Selected default with no selection cannot
 * accidentally consume unrelated automatic work.
 */
[[nodiscard]] inline std::vector<LightBakeInfluenceSphere>
lightBakeSelectedRequestInfluences(
    const std::span<const SedLight> previous,
    const std::span<const SedLight> current,
    const std::span<const std::int32_t> selectedIndices,
    const std::span<const LightBakeInfluenceSphere> captured,
    const std::span<const LightBakeInfluenceSphere> pendingAutomatic)
{
    std::vector<LightBakeInfluenceSphere> affected =
        lightBakeSelectedInfluences(previous, current, selectedIndices);
    mergeLightBakeInfluences(affected, captured);
    if (affected.empty())
    {
        return affected;
    }
    mergeLightBakeInfluences(affected, pendingAutomatic);
    return affected;
}

/** Returns sectors containing at least one bakeable vertex in an affected range. */
[[nodiscard]] inline std::vector<std::int32_t> lightBakeAffectedSectorTargets(
    const Level& level,
    const std::span<const LightBakeInfluenceSphere> influences)
{
    std::vector<std::int32_t> targets{};
    if (influences.empty())
    {
        return targets;
    }

    targets.reserve(level.sectors.size());
    std::vector<const LightBakeInfluenceSphere*> sectorInfluences{};
    sectorInfluences.reserve(influences.size());
    for (std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex)
    {
        const Sector& sector = level.sectors[sectorIndex];
        if (sector.vertices.empty())
        {
            continue;
        }

        std::array<double, 6> bounds{
            sector.vertices.front().x,
            sector.vertices.front().y,
            sector.vertices.front().z,
            sector.vertices.front().x,
            sector.vertices.front().y,
            sector.vertices.front().z,
        };
        for (const Vertex& vertex : sector.vertices)
        {
            bounds[0] = std::min(bounds[0], vertex.x);
            bounds[1] = std::min(bounds[1], vertex.y);
            bounds[2] = std::min(bounds[2], vertex.z);
            bounds[3] = std::max(bounds[3], vertex.x);
            bounds[4] = std::max(bounds[4], vertex.y);
            bounds[5] = std::max(bounds[5], vertex.z);
        }

        sectorInfluences.clear();
        for (const LightBakeInfluenceSphere& influence : influences)
        {
            const double nearestX = std::clamp(influence.center.x, bounds[0], bounds[3]);
            const double nearestY = std::clamp(influence.center.y, bounds[1], bounds[4]);
            const double nearestZ = std::clamp(influence.center.z, bounds[2], bounds[5]);
            const double dx = influence.center.x - nearestX;
            const double dy = influence.center.y - nearestY;
            const double dz = influence.center.z - nearestZ;
            if ((dx * dx) + (dy * dy) + (dz * dz)
                <= influence.radius * influence.radius)
            {
                sectorInfluences.push_back(std::addressof(influence));
            }
        }
        if (sectorInfluences.empty())
        {
            continue;
        }

        bool hasAffectedVertex = false;
        for (const Surface& surface : sector.surfaces)
        {
            if (surface.vertices.empty()
                || surface.texVertices.size() != surface.vertices.size())
            {
                continue;
            }
            for (const std::int32_t vertexIndex : surface.vertices)
            {
                if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
                {
                    continue;
                }
                const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
                hasAffectedVertex = std::any_of(
                    sectorInfluences.begin(),
                    sectorInfluences.end(),
                    [&vertex](const LightBakeInfluenceSphere* influence)
                    {
                        const double dx = vertex.x - influence->center.x;
                        const double dy = vertex.y - influence->center.y;
                        const double dz = vertex.z - influence->center.z;
                        return (dx * dx) + (dy * dy) + (dz * dz)
                            < influence->radius * influence->radius;
                    });
                if (hasAffectedVertex)
                {
                    break;
                }
            }
            if (hasAffectedVertex)
            {
                break;
            }
        }

        if (hasAffectedVertex)
        {
            targets.push_back(static_cast<std::int32_t>(sectorIndex));
        }
    }
    return targets;
}

/** Sorted unique union used to coalesce rapid edits while a bake is running. */
inline void mergeLightBakeTargets(
    std::vector<std::int32_t>& destination,
    const std::span<const std::int32_t> additions)
{
    destination.insert(destination.end(), additions.begin(), additions.end());
    std::sort(destination.begin(), destination.end());
    destination.erase(std::unique(destination.begin(), destination.end()), destination.end());
}

namespace detail
{

class FingerprintBuilder
{
public:
    template <typename T>
    void addIntegral(const T value) noexcept
    {
        using U = std::make_unsigned_t<T>;
        U bits = static_cast<U>(value);
        for (std::size_t i = 0; i < sizeof(U); ++i)
        {
            addByte(static_cast<std::uint8_t>((bits >> (i * 8u)) & U{0xff}));
        }
    }

    void addBool(const bool value) noexcept { addByte(value ? 1u : 0u); }
    void addFloat(const float value) noexcept { addIntegral(std::bit_cast<std::uint32_t>(value)); }
    void addDouble(const double value) noexcept { addIntegral(std::bit_cast<std::uint64_t>(value)); }

    void addString(const std::string_view value) noexcept
    {
        addIntegral(value.size());
        for (const unsigned char c : value)
        {
            addByte(c);
        }
    }

    [[nodiscard]] std::uint64_t value() const noexcept { return value_; }

private:
    void addByte(const std::uint8_t byte) noexcept
    {
        value_ ^= static_cast<std::uint64_t>(byte);
        value_ *= 1099511628211ull;
    }

    std::uint64_t value_{1469598103934665603ull};
};

inline void addColor(FingerprintBuilder& out, const ColorF& color) noexcept
{
    out.addFloat(color.r);
    out.addFloat(color.g);
    out.addFloat(color.b);
    out.addFloat(color.a);
}

inline void addVector(FingerprintBuilder& out, const TVector& vector) noexcept
{
    out.addDouble(vector.x);
    out.addDouble(vector.y);
    out.addDouble(vector.z);
}

} // namespace detail

/**
 * Hashes all live inputs that affect the new baked-lighting pass while
 * deliberately excluding its outputs (surface vertex colors and ambient).
 */
[[nodiscard]] inline std::uint64_t lightBakeSourceFingerprint(
    const Level& level,
    const std::span<const std::int32_t> targetSectors = {}) noexcept
{
    detail::FingerprintBuilder out{};
    out.addIntegral(static_cast<std::int32_t>(level.kind));

    out.addIntegral(level.lights.size());
    for (const SedLight& light : level.lights)
    {
        out.addIntegral(light.id);
        out.addIntegral(light.flags);
        detail::addColor(out, light.color);
        out.addDouble(light.intensity);
        out.addDouble(light.range);
        detail::addVector(out, light.position);
    }

    out.addIntegral(level.sectors.size());
    for (const Sector& sector : level.sectors)
    {
        out.addIntegral(sector.id);
        out.addIntegral(sector.flags);
        out.addIntegral(sector.layer);
        detail::addColor(out, sector.extraLight);

        out.addIntegral(sector.vertices.size());
        for (const Vertex& vertex : sector.vertices)
        {
            out.addDouble(vertex.x);
            out.addDouble(vertex.y);
            out.addDouble(vertex.z);
        }

        out.addIntegral(sector.surfaces.size());
        for (const Surface& surface : sector.surfaces)
        {
            out.addIntegral(surface.surfaceFlags);
            out.addIntegral(surface.faceFlags);
            out.addIntegral(surface.geo);
            out.addIntegral(surface.light);
            out.addIntegral(surface.adjoinFlags);
            detail::addColor(out, surface.extraLight);
            detail::addVector(out, surface.normal);

            out.addBool(surface.adjoin.has_value());
            if (surface.adjoin)
            {
                out.addIntegral(surface.adjoin->sector);
                out.addIntegral(surface.adjoin->surface);
            }

            out.addIntegral(surface.vertices.size());
            for (const std::int32_t index : surface.vertices)
            {
                out.addIntegral(index);
            }
            // Texture-vertex colors are outputs and intentionally excluded,
            // but their count is structural input: publication requires the
            // worker/live surface arrays to remain index-compatible.
            out.addIntegral(surface.texVertices.size());
        }
    }

    out.addIntegral(targetSectors.size());
    for (const std::int32_t index : targetSectors)
    {
        out.addIntegral(index);
    }
    return out.value();
}

/**
 * Copies only the Level data consumed or produced by a baked-light worker.
 *
 * Automatic palette previews can start several short-lived jobs while the
 * pointer is moving. Copying Things, COGs, templates, resource tables, and
 * hierarchy data on the GUI thread for every one of those jobs made MotS and
 * IJIM RGB editing visibly hitch even though none of that data participates in
 * the bake. Sectors retain their complete records so core geometry and result
 * publication remain identical.
 */
[[nodiscard]] inline Level lightBakeWorkerSnapshot(const Level& source)
{
    Level snapshot{};
    snapshot.kind = source.kind;
    snapshot.sectors = source.sectors;
    snapshot.lights = source.lights;
    return snapshot;
}

/** Superseded or canceled results need no expensive live-source validation. */
[[nodiscard]] constexpr bool lightBakeCompletionRequiresSourceValidation(
    const bool canceled,
    const std::uint64_t resultGeneration,
    const std::uint64_t currentGeneration) noexcept
{
    return !canceled && resultGeneration == currentGeneration;
}

/** Copies only baked lighting outputs into a structurally matching live level. */
[[nodiscard]] inline bool applyLightBakeResult(
    Level& destination,
    const Level& baked,
    const std::span<const std::int32_t> targetSectors,
    const bool copySectorAmbient = true) noexcept
{
    if (destination.sectors.size() != baked.sectors.size())
    {
        return false;
    }

    // Validate the complete publication shape before mutating anything.
    // Manual application has an undo rollback, but automatic publication is
    // folded directly into the source edit and must be atomic on its own.
    for (const std::int32_t sectorIndex : targetSectors)
    {
        if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= destination.sectors.size())
        {
            return false;
        }

        Sector& dstSector = destination.sectors[static_cast<std::size_t>(sectorIndex)];
        const Sector& srcSector = baked.sectors[static_cast<std::size_t>(sectorIndex)];
        if (dstSector.surfaces.size() != srcSector.surfaces.size())
        {
            return false;
        }

        for (std::size_t surfaceIndex = 0; surfaceIndex < dstSector.surfaces.size(); ++surfaceIndex)
        {
            Surface& dstSurface = dstSector.surfaces[surfaceIndex];
            const Surface& srcSurface = srcSector.surfaces[surfaceIndex];
            if (dstSurface.texVertices.size() != srcSurface.texVertices.size())
            {
                return false;
            }
        }
    }

    for (const std::int32_t sectorIndex : targetSectors)
    {
        Sector& dstSector = destination.sectors[static_cast<std::size_t>(sectorIndex)];
        const Sector& srcSector = baked.sectors[static_cast<std::size_t>(sectorIndex)];
        for (std::size_t surfaceIndex = 0; surfaceIndex < dstSector.surfaces.size(); ++surfaceIndex)
        {
            Surface& dstSurface = dstSector.surfaces[surfaceIndex];
            const Surface& srcSurface = srcSector.surfaces[surfaceIndex];
            for (std::size_t vertexIndex = 0; vertexIndex < dstSurface.texVertices.size(); ++vertexIndex)
            {
                dstSurface.texVertices[vertexIndex].color = srcSurface.texVertices[vertexIndex].color;
            }
        }
        if (copySectorAmbient)
        {
            dstSector.ambient = srcSector.ambient;
        }
    }
    return true;
}

} // namespace sed::ui::lighting
