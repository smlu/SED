#pragma once

#include "sed/assets/3do.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/tpl.hpp"
#include "sed/level/arch_lighting.hpp"
#include "sed/level/lighting.hpp"
#include "sed/level/thing.hpp"
#include "ui/components/preview_resource_revision_utils.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sed::ui::lighting
{

struct MotsArchLightingBakeResolveResult
{
    bool success{};
    bool canceled{};
    std::vector<level::MotsArchLightingBakeTarget> targets{};
    std::string message{};
};

[[nodiscard]] inline std::string motsArchBakeCanonicalModelName(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing)
{
    const TplValue* value = level::thingFindValue(level, templates, thing, "model3d");
    if (!value)
    {
        value = level::thingFindValue(level, templates, thing, "model3do");
    }
    if (!value)
    {
        return {};
    }

    std::string name = text::trim(value->asString());
    if (name.empty() || text::iequalsAscii(name, "none"))
    {
        return {};
    }
    name = text::lowerAscii(name);
    if (!text::iendsWithAscii(name, ".3do"))
    {
        name += ".3do";
    }
    return name;
}

[[nodiscard]] inline bool motsArchBakeRequested(const Level& level) noexcept
{
    if (level.kind != ProjectType::MOTS)
    {
        return false;
    }
    constexpr std::int32_t kAffectArchThings = 0x20;
    return !level.motsArchLightingBakeBaselines.empty()
        || std::ranges::any_of(level.lights, [](const SedLight& light)
            {
                return (light.flags & kAffectArchThings) != 0;
            });
}

namespace detail
{

class MotsArchBakeFingerprint
{
public:
    void addBytes(const std::string_view value) noexcept
    {
        addInteger(value.size());
        for (const unsigned char byte : value)
        {
            value_ ^= byte;
            value_ *= 1099511628211ull;
        }
    }

    template<typename T>
        requires(std::is_integral_v<T>)
    void addInteger(const T value) noexcept
    {
        using Unsigned = std::make_unsigned_t<T>;
        Unsigned bits = static_cast<Unsigned>(value);
        for (std::size_t index = 0; index < sizeof(Unsigned); ++index)
        {
            value_ ^= static_cast<std::uint8_t>(bits & 0xffu);
            value_ *= 1099511628211ull;
            bits >>= 8u;
        }
    }

    void addDouble(const double value) noexcept
    {
        addInteger(std::bit_cast<std::uint64_t>(value));
    }

    void addFloat(const float value) noexcept
    {
        addInteger(std::bit_cast<std::uint32_t>(value));
    }

    [[nodiscard]] std::uint64_t value() const noexcept { return value_; }

private:
    std::uint64_t value_{14695981039346656037ull};
};

inline void addMotsArchObject(
    MotsArchBakeFingerprint& hash,
    const MotsArchLightObject& object) noexcept
{
    hash.addInteger(object.wireIndex);
    hash.addInteger(object.meshes.size());
    for (const MotsArchLightMesh& mesh : object.meshes)
    {
        hash.addInteger(mesh.wireIndex);
        hash.addInteger(mesh.vertices.size());
        for (const MotsArchLightVertex& vertex : mesh.vertices)
        {
            hash.addFloat(vertex.mono);
            hash.addFloat(vertex.red);
            hash.addFloat(vertex.green);
            hash.addFloat(vertex.blue);
        }
    }
}

} // namespace detail

/**
 * Fingerprints MotS Arch owners, ARCH data/provenance, baked-light sources,
 * and the exact 3DO revisions used by the optional ARCH bake. This metadata
 * lookup does not read or parse any resource bytes.
 */
[[nodiscard]] inline std::uint64_t motsArchLightingBakeSourceFingerprint(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources) noexcept
{
    detail::MotsArchBakeFingerprint hash{};
    hash.addInteger(static_cast<std::int32_t>(level.kind));
    hash.addInteger(level.lights.size());
    for (const SedLight& light : level.lights)
    {
        hash.addInteger(light.id);
        hash.addInteger(light.flags);
        hash.addFloat(light.color.r);
        hash.addFloat(light.color.g);
        hash.addFloat(light.color.b);
        hash.addFloat(light.color.a);
        hash.addDouble(light.intensity);
        hash.addDouble(light.range);
        hash.addDouble(light.position.x);
        hash.addDouble(light.position.y);
        hash.addDouble(light.position.z);
    }

    hash.addInteger(level.things.size());
    const bool hasActiveBakeSource = level.kind == ProjectType::MOTS
        && std::ranges::any_of(level.lights, [](const SedLight& light)
            {
                return (light.flags & 0x20) != 0;
            });
    std::unordered_set<std::int32_t> baselineThingIds{};
    baselineThingIds.reserve(level.motsArchLightingBakeBaselines.size());
    for (const MotsArchLightingBakeBaseline& baseline :
         level.motsArchLightingBakeBaselines)
    {
        baselineThingIds.insert(baseline.thingId);
    }
    for (const Thing& thing : level.things)
    {
        const bool effectiveArch = level.kind == ProjectType::MOTS
            && level::isMotsArchThing(level, thing, templates);
        const bool baselineOwner = baselineThingIds.contains(thing.id);
        if (!effectiveArch && !baselineOwner)
        {
            continue;
        }

        hash.addInteger(thing.id);
        hash.addBytes(thing.name);
        hash.addBytes(thing.templateName);
        hash.addInteger(thing.sector);
        hash.addInteger(thing.layer);
        hash.addInteger(thing.archLightingIndex);
        hash.addInteger(level::thingFlags(level, templates, thing));
        hash.addDouble(thing.x);
        hash.addDouble(thing.y);
        hash.addDouble(thing.z);
        hash.addDouble(thing.pch);
        hash.addDouble(thing.yaw);
        hash.addDouble(thing.rol);
        const std::string modelName =
            motsArchBakeCanonicalModelName(level, templates, thing);
        hash.addBytes(modelName);
        if (effectiveArch && hasActiveBakeSource)
        {
            hash.addBytes(::sed::ui::detail::previewResourceRevisionToken(
                resources, modelName, ".3do"));
        }
    }

    hash.addInteger(level.motsArchLighting.size());
    for (const MotsArchLightObject& object : level.motsArchLighting)
    {
        detail::addMotsArchObject(hash, object);
    }
    hash.addInteger(level.motsArchLightingBakeBaselines.size());
    for (const MotsArchLightingBakeBaseline& baseline : level.motsArchLightingBakeBaselines)
    {
        hash.addInteger(baseline.thingId);
        hash.addBytes(baseline.modelName);
        hash.addInteger(baseline.originalArchLightingIndex);
        hash.addInteger(static_cast<std::uint8_t>(
            baseline.originalArchLighting.has_value()));
        if (baseline.originalArchLighting)
        {
            detail::addMotsArchObject(hash, *baseline.originalArchLighting);
        }
    }
    hash.addInteger(resources ? resources->previewCacheIdentity() : 0u);
    return hash.value();
}

struct MotsArchBakeGeosetCount
{
    std::optional<std::int32_t> count{};
    std::string message{};
};

[[nodiscard]] inline MotsArchBakeGeosetCount motsArchBakeDeclaredGeosetCount(
    const std::span<const std::byte> bytes)
{
    const char* const data = bytes.empty()
        ? ""
        : reinterpret_cast<const char*>(bytes.data());
    const std::string_view textView(data, bytes.size());
    std::istringstream input{ std::string(textView) };
    std::string line{};
    bool inGeometrySection = false;
    bool found = false;
    std::int32_t geosetCount = 0;
    while (std::getline(input, line))
    {
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos)
        {
            line.resize(comment);
        }
        std::istringstream tokens(line);
        std::string first{};
        std::string second{};
        if (!(tokens >> first))
        {
            continue;
        }
        if (text::iequalsAscii(first, "SECTION:"))
        {
            if (!(tokens >> second))
            {
                inGeometrySection = false;
                continue;
            }
            inGeometrySection = text::iequalsAscii(second, "GEOMETRYDEF");
            continue;
        }
        if (text::iequalsAscii(first, "END"))
        {
            inGeometrySection = false;
            continue;
        }
        if (!inGeometrySection || !text::iequalsAscii(first, "GEOSETS"))
        {
            continue;
        }

        found = true;
        std::string value{};
        if (!(tokens >> value))
        {
            return { std::nullopt, "invalid GEOSETS count (expected 1..4)." };
        }
        const char* const begin = value.data();
        const char* const end = begin + value.size();
        const auto parsed = std::from_chars(begin, end, geosetCount);
        if (parsed.ec != std::errc{} || parsed.ptr != end
            || geosetCount < 1 || geosetCount > 4)
        {
            return { std::nullopt,
                "invalid GEOSETS count " + value + " (expected 1..4)." };
        }
        break;
    }

    if (!found)
    {
        return { std::nullopt, "missing GEOSETS declaration (expected 1..4)." };
    }
    return { geosetCount, {} };
}

[[nodiscard]] inline MotsArchLightingBakeResolveResult
resolveMotsArchLightingBakeTargets(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator& resources,
    const std::atomic_bool* cancelFlag = nullptr)
{
    MotsArchLightingBakeResolveResult result{};
    if (level.kind != ProjectType::MOTS)
    {
        result.success = true;
        return result;
    }
    const auto canceled = [&]() noexcept
    {
        return cancelFlag && cancelFlag->load(std::memory_order_relaxed);
    };
    const auto thingPrefix = [](const Thing& thing, const std::string_view model)
    {
        return "MotS ARCH bake rejected: Thing '" + thing.name + "' (ID "
            + std::to_string(thing.id) + "), model '" + std::string(model) + "': ";
    };
    std::unordered_map<std::string, std::vector<std::byte>> modelByteCache{};
    for (std::size_t thingIndex = 0; thingIndex < level.things.size(); ++thingIndex)
    {
        if (canceled())
        {
            result.canceled = true;
            return result;
        }
        const Thing& thing = level.things[thingIndex];
        if (!level::isMotsArchThing(level, thing, templates))
        {
            continue;
        }

        const std::string modelName =
            motsArchBakeCanonicalModelName(level, templates, thing);
        if (modelName.empty())
        {
            // An effective Arch Thing without a declared model has no vertex
            // geometry to bake. Leave its authored ARCH data untouched. A
            // declared model is still required and fails atomically if absent.
            continue;
        }

        auto cachedBytes = modelByteCache.find(modelName);
        if (cachedBytes == modelByteCache.end())
        {
            std::optional<std::vector<std::byte>> modelBytes{};
            try
            {
                modelBytes = resources.tryReadBinaryWithExt(modelName, ".3do");
            }
            catch (const std::exception& error)
            {
                result.message = thingPrefix(thing, modelName)
                    + "resource read failed: " + error.what() + ".";
                return result;
            }
            catch (...)
            {
                result.message = thingPrefix(thing, modelName)
                    + "resource read failed.";
                return result;
            }
            if (!modelBytes)
            {
                result.message = thingPrefix(thing, modelName) + "resource not found.";
                return result;
            }
            cachedBytes = modelByteCache.emplace(
                modelName, std::move(*modelBytes)).first;
        }
        const std::vector<std::byte>& modelBytes = cachedBytes->second;

        const MotsArchBakeGeosetCount geosetCount =
            motsArchBakeDeclaredGeosetCount(modelBytes);
        if (!geosetCount.count)
        {
            result.message = thingPrefix(thing, modelName) + geosetCount.message;
            return result;
        }

        level::MotsArchLightingBakeTarget target{};
        target.thingIndex = static_cast<std::int32_t>(thingIndex);
        target.thingId = thing.id;
        target.sectorIndex = thing.sector;
        target.thingName = thing.name;
        target.modelName = modelName;
        target.topology.geosets.resize(static_cast<std::size_t>(*geosetCount.count));
        target.lods.resize(static_cast<std::size_t>(*geosetCount.count));

        TVector pyr{};
        pyr.pitch = thing.pch;
        pyr.yaw = thing.yaw;
        pyr.roll = thing.rol;
        const TMatrix43 thingTransform =
            TMatrix43::Create(TVector{ thing.x, thing.y, thing.z }, pyr);

        for (std::int32_t lod = 0; lod < *geosetCount.count; ++lod)
        {
            if (canceled())
            {
                result.canceled = true;
                return result;
            }
            try
            {
                const std::span<const std::byte> modelSpan(
                    modelBytes.data(), modelBytes.size());
                const assets::Model3do rawModel = assets::loadFrom3DO(
                    modelSpan, assets::Model3doLoadOptions{ lod, false });
                const assets::Model3do renderModel = assets::loadFrom3DO(
                    modelSpan, assets::Model3doLoadOptions{ lod, true });
                if (rawModel.meshes.empty() || renderModel.meshes.empty())
                {
                    result.message = thingPrefix(thing, modelName)
                        + "GEOSET " + std::to_string(lod)
                        + " has no meshes.";
                    return result;
                }

                auto rawSourceIndex = [&rawModel](const std::size_t meshIndex)
                {
                    return rawModel.meshSourceIndices.size() == rawModel.meshes.size()
                        ? rawModel.meshSourceIndices[meshIndex]
                        : static_cast<std::int32_t>(meshIndex);
                };
                std::vector<std::size_t>& vertexCounts =
                    target.topology.geosets[static_cast<std::size_t>(lod)];
                for (std::size_t meshIndex = 0;
                     meshIndex < rawModel.meshes.size();
                     ++meshIndex)
                {
                    const std::int32_t sourceIndex = rawSourceIndex(meshIndex);
                    if (sourceIndex < 0 || sourceIndex > 1'000'000)
                    {
                        result.message = thingPrefix(thing, modelName)
                            + "GEOSET " + std::to_string(lod)
                            + " has an invalid source-mesh index.";
                        return result;
                    }
                    const std::size_t source = static_cast<std::size_t>(sourceIndex);
                    if (vertexCounts.size() <= source)
                    {
                        vertexCounts.resize(source + 1u, 0u);
                    }
                    vertexCounts[source] = rawModel.meshes[meshIndex].vertices.size();
                }

                auto renderSourceIndex = [&renderModel](const std::size_t meshIndex)
                {
                    return renderModel.meshSourceIndices.size() == renderModel.meshes.size()
                        ? renderModel.meshSourceIndices[meshIndex]
                        : static_cast<std::int32_t>(meshIndex);
                };
                level::MotsArchLightingBakeGeoset& geoset =
                    target.lods[static_cast<std::size_t>(lod)];
                geoset.meshes.reserve(renderModel.meshes.size());
                for (std::size_t meshIndex = 0;
                     meshIndex < renderModel.meshes.size();
                     ++meshIndex)
                {
                    const std::int32_t sourceIndex = renderSourceIndex(meshIndex);
                    if (sourceIndex < 0
                        || static_cast<std::size_t>(sourceIndex) >= vertexCounts.size())
                    {
                        result.message = thingPrefix(thing, modelName)
                            + "GEOSET " + std::to_string(lod)
                            + " has a hierarchy mesh with no source-mesh mapping.";
                        return result;
                    }

                    level::MotsArchLightingBakeMesh mesh{};
                    mesh.sourceMeshIndex = static_cast<std::size_t>(sourceIndex);
                    mesh.worldVertices.reserve(
                        renderModel.meshes[meshIndex].vertices.size());
                    for (const assets::Model3doVertex& vertex :
                         renderModel.meshes[meshIndex].vertices)
                    {
                        const TVector world = thingTransform.TransformPoint(
                            TVector{ vertex.x, vertex.y, vertex.z });
                        if (!std::isfinite(world.x)
                            || !std::isfinite(world.y)
                            || !std::isfinite(world.z))
                        {
                            result.message = thingPrefix(thing, modelName)
                                + "GEOSET " + std::to_string(lod)
                                + " contains a non-finite transformed vertex.";
                            return result;
                        }
                        mesh.worldVertices.push_back(world);
                    }
                    geoset.meshes.push_back(std::move(mesh));
                }
            }
            catch (const std::exception& error)
            {
                result.message = thingPrefix(thing, modelName)
                    + "failed to parse GEOSET " + std::to_string(lod)
                    + ": " + error.what() + ".";
                return result;
            }
            catch (...)
            {
                result.message = thingPrefix(thing, modelName)
                    + "failed to parse GEOSET " + std::to_string(lod) + ".";
                return result;
            }
        }

        result.targets.push_back(std::move(target));
    }
    result.success = true;
    return result;
}

} // namespace sed::ui::lighting
