#include "sed/level/arch_lighting.hpp"

#include "sed/level/thing.hpp"
#include "sed/text.hpp"

#include <bit>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sed::level
{

namespace
{

constexpr std::int32_t kArchFlag = 0x00000008;
constexpr std::size_t kMaxArchLightingItems = 1'000'000u;
constexpr std::size_t kNoStagedObject = std::numeric_limits<std::size_t>::max();

constexpr std::string_view kNoopMessage =
    "No MotS ARCHLIGHTING changes were necessary.";
constexpr std::string_view kProjectMessage =
    "MotS ARCHLIGHTING editing requires a MotS level.";
constexpr std::string_view kThingMessage =
    "MotS ARCHLIGHTING editing requires an Arch Thing.";
constexpr std::string_view kIndexMessage =
    "MotS ARCHLIGHTING Thing selection contains an invalid index.";
constexpr std::string_view kDuplicateMessage =
    "MotS ARCHLIGHTING Thing selection contains a duplicate index.";
constexpr std::string_view kDuplicateVertexMessage =
    "MotS ARCHLIGHTING edit contains a duplicate vertex.";
constexpr std::string_view kTopologyCountMessage =
    "ARCHLIGHTING model topology is required for every selected Thing.";
constexpr std::string_view kTopologyMessage =
    "ARCHLIGHTING model topology is incompatible with every render geoset.";
constexpr std::string_view kFiniteMessage =
    "MotS ARCHLIGHTING edit values must be finite.";
constexpr std::string_view kRecordMessage =
    "Selected Thing has no valid MotS ARCHLIGHTING record.";
constexpr std::string_view kExistingMessage =
    "Selected Thing already has MotS ARCHLIGHTING.";
constexpr std::string_view kBudgetMessage =
    "MotS ARCHLIGHTING edit exceeds the bounded record budget.";
constexpr std::string_view kMultiplyMessage =
    "Updated RGB channels for selected MotS ARCHLIGHTING owners.";
constexpr std::string_view kMonoMultiplyMessage =
    "Updated mono channel for selected MotS ARCHLIGHTING owners.";
constexpr std::string_view kSetMessage =
    "Set MotS ARCHLIGHTING vertex channels for selected owners.";
constexpr std::string_view kInitializeMessage =
    "Initialized default-zero MotS ARCHLIGHTING records.";
constexpr std::string_view kResetMessage =
    "Reset MotS ARCHLIGHTING to the all-zero default.";
constexpr std::string_view kResetTopologyMessage =
    "Selected Thing ARCHLIGHTING topology no longer matches its 3DO model.";
constexpr std::string_view kCopyMessage =
    "Copied MotS ARCHLIGHTING to selected owners.";
constexpr std::string_view kCopyIdentityMessage =
    "MotS ARCHLIGHTING copy requires matching model identity for every target.";
constexpr std::string_view kCopyEmptyIdentityMessage =
    "MotS ARCHLIGHTING copy requires a non-empty source model identity.";
constexpr std::string_view kCopyTargetIdentityMessage =
    "MotS ARCHLIGHTING copy requires a non-empty model identity for every target.";
constexpr std::string_view kCopyTopologyMessage =
    "MotS ARCHLIGHTING copy requires compatible all-LOD topology for every target.";
constexpr std::string_view kCopyStructureMessage =
    "MotS ARCHLIGHTING copy source record structure is invalid.";
constexpr std::string_view kCopyFiniteMessage =
    "MotS ARCHLIGHTING copy source channels must be finite.";

bool hasUsableModel(
    const Level& level,
    const assets::Templates* const templates,
    const Thing& thing)
{
    const TplValue* model = thingFindValue(level, templates, thing, "model3d");
    if (!model)
    {
        model = thingFindValue(level, templates, thing, "model3do");
    }
    if (!model)
    {
        return false;
    }
    const std::string modelName = model->asString();
    const std::string_view trimmed = text::trimView(modelName);
    return !trimmed.empty() && !text::iequalsAscii(trimmed, "none");
}

void promoteThingFlags(
    std::vector<TplValue>& values,
    const std::int32_t effectiveFlags)
{
    for (auto it = values.rbegin(); it != values.rend(); ++it)
    {
        if (!text::iequalsAscii(it->name, "thingflags"))
        {
            continue;
        }
        if (it->type == ValueType::integer
            || it->type == ValueType::flag
            || it->type == ValueType::ptr)
        {
            it->intValue |= kArchFlag;
            return;
        }
        break;
    }

    TplValue overrideValue{};
    overrideValue.name = "thingflags";
    overrideValue.type = ValueType::integer;
    overrideValue.intValue = effectiveFlags | kArchFlag;
    values.push_back(std::move(overrideValue));
}

struct StagedReference
{
    std::size_t thingIndex{};
    std::size_t appendOrdinal{};
};

struct StagedObject
{
    // kNoStagedObject means append this object after the existing table.
    std::size_t destination{kNoStagedObject};
    std::size_t appendOrdinal{kNoStagedObject};
    MotsArchLightObject object{};
};

struct RecordCounts
{
    std::size_t meshes{};
    std::size_t vertices{};
};

struct SourcePlan
{
    bool changes{};
    bool append{};
};

struct SparseVertexKey
{
    std::int32_t thingIndex{};
    std::size_t meshIndex{};
    std::size_t vertexIndex{};

    bool operator==(const SparseVertexKey& other) const noexcept
    {
        return thingIndex == other.thingIndex
            && meshIndex == other.meshIndex
            && vertexIndex == other.vertexIndex;
    }
};

struct SparseVertexKeyHash
{
    std::size_t operator()(const SparseVertexKey& key) const noexcept
    {
        std::size_t hash = std::hash<std::int32_t>{}(key.thingIndex);
        hash ^= std::hash<std::size_t>{}(key.meshIndex)
            + static_cast<std::size_t>(0x9e3779b9u)
            + (hash << 6u)
            + (hash >> 2u);
        hash ^= std::hash<std::size_t>{}(key.vertexIndex)
            + static_cast<std::size_t>(0x9e3779b9u)
            + (hash << 6u)
            + (hash >> 2u);
        return hash;
    }
};

MotsArchLightingEditResult resultWith(
    const MotsArchLightingEditStatus status,
    const std::string_view message)
{
    MotsArchLightingEditResult result{};
    result.status = status;
    result.message = message;
    return result;
}

bool finiteVertex(const MotsArchLightVertex& vertex) noexcept
{
    return std::isfinite(vertex.mono)
        && std::isfinite(vertex.red)
        && std::isfinite(vertex.green)
        && std::isfinite(vertex.blue);
}

bool finiteEditedVertex(const MotsArchLightingVertexEdit& edit) noexcept
{
    return (!edit.channels[0] || std::isfinite(edit.vertexValue.mono))
        && (!edit.channels[1] || std::isfinite(edit.vertexValue.red))
        && (!edit.channels[2] || std::isfinite(edit.vertexValue.green))
        && (!edit.channels[3] || std::isfinite(edit.vertexValue.blue));
}

bool finiteRgbMultiplier(const ColorF& multiplier) noexcept
{
    return std::isfinite(multiplier.r)
        && std::isfinite(multiplier.g)
        && std::isfinite(multiplier.b);
}

bool sameVertexBits(
    const MotsArchLightVertex& lhs,
    const MotsArchLightVertex& rhs) noexcept
{
    return std::bit_cast<std::uint32_t>(lhs.mono)
            == std::bit_cast<std::uint32_t>(rhs.mono)
        && std::bit_cast<std::uint32_t>(lhs.red)
            == std::bit_cast<std::uint32_t>(rhs.red)
        && std::bit_cast<std::uint32_t>(lhs.green)
            == std::bit_cast<std::uint32_t>(rhs.green)
        && std::bit_cast<std::uint32_t>(lhs.blue)
            == std::bit_cast<std::uint32_t>(rhs.blue);
}

bool validArchiveStructure(const MotsArchLightObject& object)
{
    if (object.wireIndex < 0 || object.meshes.size() > kMaxArchLightingItems)
    {
        return false;
    }

    std::unordered_set<std::int32_t> meshIndices{};
    meshIndices.reserve(object.meshes.size());
    std::size_t vertexTotal = 0;
    for (const MotsArchLightMesh& mesh : object.meshes)
    {
        if (mesh.wireIndex < 0
            || !meshIndices.insert(mesh.wireIndex).second
            || mesh.vertices.size() > kMaxArchLightingItems
            || vertexTotal > kMaxArchLightingItems - mesh.vertices.size())
        {
            return false;
        }
        vertexTotal += mesh.vertices.size();
    }
    return true;
}

bool finiteArchiveChannels(const MotsArchLightObject& object) noexcept
{
    for (const MotsArchLightMesh& mesh : object.meshes)
    {
        for (const MotsArchLightVertex& vertex : mesh.vertices)
        {
            if (!finiteVertex(vertex))
            {
                return false;
            }
        }
    }
    return true;
}

bool validArchiveObject(const MotsArchLightObject& object)
{
    return validArchiveStructure(object) && finiteArchiveChannels(object);
}

bool topologyFitsObject(
    const MotsArchLightingTopology& topology,
    const MotsArchLightObject& object) noexcept
{
    if (topology.geosets.empty()
        || topology.geosets.size() > kMaxArchLightingItems)
    {
        return false;
    }

    for (const std::vector<std::size_t>& geoset : topology.geosets)
    {
        if (geoset.size() > object.meshes.size()
            || geoset.size() > kMaxArchLightingItems)
        {
            return false;
        }
        for (std::size_t meshIndex = 0; meshIndex < geoset.size(); ++meshIndex)
        {
            const std::size_t vertexCount = geoset[meshIndex];
            if (vertexCount > kMaxArchLightingItems
                || vertexCount > object.meshes[meshIndex].vertices.size())
            {
                return false;
            }
        }
    }
    return true;
}

std::vector<std::size_t> maximumMeshVertexCounts(
    const MotsArchLightingTopology& topology);

bool topologyMatchesObject(
    const MotsArchLightingTopology& topology,
    const MotsArchLightObject& object)
{
    const std::vector<std::size_t> expectedCounts =
        maximumMeshVertexCounts(topology);
    if (expectedCounts.empty() || expectedCounts.size() != object.meshes.size())
    {
        return false;
    }
    for (std::size_t meshIndex = 0; meshIndex < expectedCounts.size(); ++meshIndex)
    {
        if (expectedCounts[meshIndex] != object.meshes[meshIndex].vertices.size())
        {
            return false;
        }
    }
    return true;
}

bool topologyContainsVertex(
    const MotsArchLightingTopology& topology,
    const std::size_t meshIndex,
    const std::size_t vertexIndex) noexcept
{
    for (const std::vector<std::size_t>& geoset : topology.geosets)
    {
        if (meshIndex < geoset.size() && vertexIndex < geoset[meshIndex])
        {
            return true;
        }
    }
    return false;
}

bool sameTopology(
    const MotsArchLightingTopology& left,
    const MotsArchLightingTopology& right) noexcept
{
    return left.geosets == right.geosets;
}

bool addWithinBudget(std::size_t& total, const std::size_t amount) noexcept
{
    if (amount > kMaxArchLightingItems - total)
    {
        return false;
    }
    total += amount;
    return true;
}

bool countObjectRecords(
    const MotsArchLightObject& object,
    RecordCounts& counts) noexcept
{
    if (!addWithinBudget(counts.meshes, object.meshes.size()))
    {
        return false;
    }
    for (const MotsArchLightMesh& mesh : object.meshes)
    {
        if (!addWithinBudget(counts.vertices, mesh.vertices.size()))
        {
            return false;
        }
    }
    return true;
}

bool countLevelRecords(const Level& level, RecordCounts& counts) noexcept
{
    for (const MotsArchLightObject& object : level.motsArchLighting)
    {
        if (!countObjectRecords(object, counts))
        {
            return false;
        }
    }
    return true;
}

std::vector<std::size_t> maximumMeshVertexCounts(
    const MotsArchLightingTopology& topology)
{
    if (topology.geosets.empty()
        || topology.geosets.size() > kMaxArchLightingItems)
    {
        return {};
    }
    std::size_t meshCount = 0;
    for (const std::vector<std::size_t>& geoset : topology.geosets)
    {
        if (geoset.size() > kMaxArchLightingItems
            || std::any_of(
                geoset.begin(), geoset.end(),
                [](const std::size_t vertexCount)
                {
                    return vertexCount > kMaxArchLightingItems;
                }))
        {
            return {};
        }
        meshCount = std::max(meshCount, geoset.size());
    }
    if (meshCount == 0 || meshCount > kMaxArchLightingItems)
    {
        return {};
    }

    std::vector<std::size_t> vertexCounts(meshCount, 0);
    std::size_t vertexTotal = 0;
    for (const std::vector<std::size_t>& geoset : topology.geosets)
    {
        for (std::size_t meshIndex = 0; meshIndex < geoset.size(); ++meshIndex)
        {
            vertexCounts[meshIndex] =
                std::max(vertexCounts[meshIndex], geoset[meshIndex]);
        }
    }
    for (const std::size_t vertexCount : vertexCounts)
    {
        if (vertexTotal > kMaxArchLightingItems - vertexCount)
        {
            return {};
        }
        vertexTotal += vertexCount;
    }
    return vertexCounts;
}

MotsArchLightObject initializedObject(
    const std::vector<std::size_t>& vertexCounts)
{
    MotsArchLightObject object{};
    object.meshes.reserve(vertexCounts.size());
    for (std::size_t meshIndex = 0; meshIndex < vertexCounts.size(); ++meshIndex)
    {
        MotsArchLightMesh mesh{};
        mesh.wireIndex = static_cast<std::int32_t>(meshIndex);
        mesh.vertices.assign(
            vertexCounts[meshIndex], MotsArchLightVertex{});
        object.meshes.push_back(std::move(mesh));
    }
    return object;
}

bool applyMultiplier(
    MotsArchLightObject& object,
    const MotsArchLightingEdit& edit) noexcept
{
    for (MotsArchLightMesh& mesh : object.meshes)
    {
        for (MotsArchLightVertex& vertex : mesh.vertices)
        {
            if (edit.mode == MotsArchLightingVertexMode::Mono)
            {
                vertex.mono *= edit.monoMultiplier;
            }
            else if (edit.mode == MotsArchLightingVertexMode::Rgb)
            {
                vertex.red *= edit.rgbMultiplier.r;
                vertex.green *= edit.rgbMultiplier.g;
                vertex.blue *= edit.rgbMultiplier.b;
            }
            else
            {
                return false;
            }

            if (!finiteVertex(vertex))
            {
                return false;
            }
        }
    }
    return true;
}

bool applySetVertex(
    MotsArchLightObject& object,
    const MotsArchLightingEdit& edit,
    const std::int32_t thingIndex = -1) noexcept
{
    const auto appliesToThing = [thingIndex](
        const MotsArchLightingVertexEdit& vertexEdit)
    {
        return vertexEdit.thingIndex < 0
            || thingIndex < 0
            || vertexEdit.thingIndex == thingIndex;
    };
    const auto applyOne = [&object, &appliesToThing](
        const MotsArchLightingVertexEdit& vertexEdit)
    {
        if (!appliesToThing(vertexEdit))
        {
            return true;
        }
        if (vertexEdit.meshIndex >= object.meshes.size()
            || vertexEdit.vertexIndex
                >= object.meshes[vertexEdit.meshIndex].vertices.size()
            || !finiteEditedVertex(vertexEdit))
        {
            return false;
        }
        MotsArchLightVertex candidate = object.meshes[vertexEdit.meshIndex]
            .vertices[vertexEdit.vertexIndex];
        if (vertexEdit.channels[0])
        {
            candidate.mono = vertexEdit.vertexValue.mono;
        }
        if (vertexEdit.channels[1])
        {
            candidate.red = vertexEdit.vertexValue.red;
        }
        if (vertexEdit.channels[2])
        {
            candidate.green = vertexEdit.vertexValue.green;
        }
        if (vertexEdit.channels[3])
        {
            candidate.blue = vertexEdit.vertexValue.blue;
        }
        if (!finiteVertex(candidate))
        {
            return false;
        }
        object.meshes[vertexEdit.meshIndex].vertices[vertexEdit.vertexIndex] = candidate;
        return true;
    };
    if (!edit.vertexEdits.empty())
    {
        for (const MotsArchLightingVertexEdit& vertexEdit : edit.vertexEdits)
        {
            if (!applyOne(vertexEdit))
            {
                return false;
            }
        }
        return true;
    }
    return applyOne(MotsArchLightingVertexEdit{
        edit.meshIndex,
        edit.vertexIndex,
        edit.vertexValue,
        { true, true, true, true } });
}

bool previewMultiplier(
    const MotsArchLightObject& object,
    const MotsArchLightingEdit& edit,
    bool& changes) noexcept
{
    changes = false;
    for (const MotsArchLightMesh& mesh : object.meshes)
    {
        for (const MotsArchLightVertex& vertex : mesh.vertices)
        {
            MotsArchLightVertex candidate = vertex;
            if (edit.mode == MotsArchLightingVertexMode::Mono)
            {
                candidate.mono *= edit.monoMultiplier;
            }
            else if (edit.mode == MotsArchLightingVertexMode::Rgb)
            {
                candidate.red *= edit.rgbMultiplier.r;
                candidate.green *= edit.rgbMultiplier.g;
                candidate.blue *= edit.rgbMultiplier.b;
            }
            else
            {
                return false;
            }
            if (!finiteVertex(candidate))
            {
                return false;
            }
            changes = changes || !sameVertexBits(candidate, vertex);
        }
    }
    return true;
}

bool previewSetVertex(
    const MotsArchLightObject& object,
    const MotsArchLightingEdit& edit,
    bool& changes,
    const std::int32_t thingIndex = -1) noexcept
{
    const auto appliesToThing = [thingIndex](
        const MotsArchLightingVertexEdit& vertexEdit)
    {
        return vertexEdit.thingIndex < 0
            || thingIndex < 0
            || vertexEdit.thingIndex == thingIndex;
    };
    const auto previewOne = [&object, &changes, &appliesToThing](
        const MotsArchLightingVertexEdit& vertexEdit)
    {
        if (!appliesToThing(vertexEdit))
        {
            return true;
        }
        if (vertexEdit.meshIndex >= object.meshes.size()
            || vertexEdit.vertexIndex
                >= object.meshes[vertexEdit.meshIndex].vertices.size()
            || !finiteEditedVertex(vertexEdit))
        {
            return false;
        }
        MotsArchLightVertex candidate = object.meshes[vertexEdit.meshIndex]
            .vertices[vertexEdit.vertexIndex];
        if (vertexEdit.channels[0])
        {
            candidate.mono = vertexEdit.vertexValue.mono;
        }
        if (vertexEdit.channels[1])
        {
            candidate.red = vertexEdit.vertexValue.red;
        }
        if (vertexEdit.channels[2])
        {
            candidate.green = vertexEdit.vertexValue.green;
        }
        if (vertexEdit.channels[3])
        {
            candidate.blue = vertexEdit.vertexValue.blue;
        }
        if (!finiteVertex(candidate))
        {
            return false;
        }
        changes = changes || !sameVertexBits(
            object.meshes[vertexEdit.meshIndex].vertices[vertexEdit.vertexIndex],
            candidate);
        return true;
    };
    if (!edit.vertexEdits.empty())
    {
        for (const MotsArchLightingVertexEdit& vertexEdit : edit.vertexEdits)
        {
            if (!previewOne(vertexEdit))
            {
                return false;
            }
        }
        return true;
    }
    return previewOne(MotsArchLightingVertexEdit{
        edit.meshIndex,
        edit.vertexIndex,
        edit.vertexValue,
        { true, true, true, true } });
}

bool assignObjectWireIndices(
    const Level& level,
    std::vector<StagedObject>& stagedObjects)
{
    std::size_t appendCount = 0;
    for (const StagedObject& staged : stagedObjects)
    {
        if (staged.destination == kNoStagedObject)
        {
            ++appendCount;
        }
    }
    if (appendCount == 0)
    {
        return true;
    }

    std::unordered_set<std::int32_t> used{};
    used.reserve(level.motsArchLighting.size() + appendCount);
    std::int64_t next = 0;
    for (const MotsArchLightObject& object : level.motsArchLighting)
    {
        if (object.wireIndex >= 0)
        {
            used.insert(object.wireIndex);
        }
    }

    for (StagedObject& staged : stagedObjects)
    {
        if (staged.destination != kNoStagedObject)
        {
            continue;
        }
        while (next <= std::numeric_limits<std::int32_t>::max()
            && used.contains(static_cast<std::int32_t>(next)))
        {
            ++next;
        }
        if (next > std::numeric_limits<std::int32_t>::max())
        {
            return false;
        }
        staged.object.wireIndex = static_cast<std::int32_t>(next);
        used.insert(staged.object.wireIndex);
        ++next;
    }
    return true;
}

bool aggregateBudgetAllowsAppend(
    const Level& level,
    const std::vector<StagedObject>& stagedObjects) noexcept
{
    std::size_t appendObjects = 0;
    for (const StagedObject& staged : stagedObjects)
    {
        if (staged.destination == kNoStagedObject)
        {
            ++appendObjects;
        }
    }
    if (appendObjects == 0)
    {
        return true;
    }

    std::size_t meshTotal = 0;
    std::size_t vertexTotal = 0;
    for (const MotsArchLightObject& object : level.motsArchLighting)
    {
        if (!addWithinBudget(meshTotal, object.meshes.size()))
        {
            return false;
        }
        for (const MotsArchLightMesh& mesh : object.meshes)
        {
            if (!addWithinBudget(vertexTotal, mesh.vertices.size()))
            {
                return false;
            }
        }
    }
    for (const StagedObject& staged : stagedObjects)
    {
        if (staged.destination != kNoStagedObject)
        {
            continue;
        }
        if (!addWithinBudget(meshTotal, staged.object.meshes.size()))
        {
            return false;
        }
        for (const MotsArchLightMesh& mesh : staged.object.meshes)
        {
            if (!addWithinBudget(vertexTotal, mesh.vertices.size()))
            {
                return false;
            }
        }
    }
    return true;
}

} // namespace

bool isMotsArchThing(
    const Level& level,
    const Thing& thing,
    const assets::Templates* templates) noexcept
{
    return level.kind == ProjectType::MOTS
        && (thingFlags(level, templates, thing) & kArchFlag) != 0;
}

const MotsArchLightObject* findActiveMotsArchLighting(
    const Level& level,
    const Thing& thing,
    const assets::Templates* templates) noexcept
{
    if (!isMotsArchThing(level, thing, templates)
        || thing.archLightingIndex < 0
        || static_cast<std::size_t>(thing.archLightingIndex)
            >= level.motsArchLighting.size())
    {
        return nullptr;
    }
    return &level.motsArchLighting[
        static_cast<std::size_t>(thing.archLightingIndex)];
}

bool isDefaultMotsArchLighting(const MotsArchLightObject& object) noexcept
{
    for (const MotsArchLightMesh& mesh : object.meshes)
    {
        for (const MotsArchLightVertex& vertex : mesh.vertices)
        {
            if (vertex.mono != 0.0f || vertex.red != 0.0f
                || vertex.green != 0.0f || vertex.blue != 0.0f)
            {
                return false;
            }
        }
    }
    return true;
}

bool matchesMotsArchLightingTopology(
    const MotsArchLightObject& object,
    const MotsArchLightingTopology& topology)
{
    return topologyMatchesObject(topology, object);
}

MotsArchLightingEditResult applyMotsArchLightingEdit(
    Level& level,
    const std::span<const std::int32_t> thingIndices,
    const std::span<const MotsArchLightingTopology> topologies,
    const MotsArchLightingEdit& edit,
    const assets::Templates* templates)
{
    if (level.kind != ProjectType::MOTS)
    {
        return resultWith(MotsArchLightingEditStatus::Rejected, kProjectMessage);
    }
    if (thingIndices.empty())
    {
        return resultWith(MotsArchLightingEditStatus::Noop, kNoopMessage);
    }
    if (thingIndices.size() > kMaxArchLightingItems)
    {
        return resultWith(MotsArchLightingEditStatus::Rejected, kBudgetMessage);
    }

    std::unordered_set<std::size_t> seenThings{};
    seenThings.reserve(thingIndices.size());
    for (const std::int32_t thingIndex : thingIndices)
    {
        if (thingIndex < 0
            || static_cast<std::size_t>(thingIndex) >= level.things.size())
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kIndexMessage);
        }
        if (!seenThings.insert(static_cast<std::size_t>(thingIndex)).second)
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kDuplicateMessage);
        }
    }

    if (edit.kind == MotsArchLightingEditKind::Reset)
    {
        if (topologies.size() != thingIndices.size())
        {
            return resultWith(
                MotsArchLightingEditStatus::Rejected, kTopologyCountMessage);
        }

        std::vector<std::vector<std::size_t>> zeroShapes{};
        std::vector<std::size_t> shapeForThing(
            thingIndices.size(), kNoStagedObject);
        std::vector<std::int32_t> changed{};
        changed.reserve(thingIndices.size());

        for (std::size_t selected = 0; selected < thingIndices.size(); ++selected)
        {
            const std::int32_t thingIndex = thingIndices[selected];
            const Thing& thing = level.things[static_cast<std::size_t>(thingIndex)];
            if (!isMotsArchThing(level, thing, templates))
            {
                return resultWith(MotsArchLightingEditStatus::Rejected, kThingMessage);
            }

            std::vector<std::size_t> counts =
                maximumMeshVertexCounts(topologies[selected]);
            if (counts.empty())
            {
                return resultWith(
                    MotsArchLightingEditStatus::Rejected, kTopologyMessage);
            }
            if (thing.archLightingIndex < -1
                || (thing.archLightingIndex >= 0
                    && static_cast<std::size_t>(thing.archLightingIndex)
                        >= level.motsArchLighting.size()))
            {
                return resultWith(
                    MotsArchLightingEditStatus::Rejected, kRecordMessage);
            }
            if (thing.archLightingIndex < 0)
            {
                // An unbound Arch Thing already has the virtual all-zero
                // default; do not allocate a redundant object for Reset.
                continue;
            }

            const MotsArchLightObject& current = level.motsArchLighting[
                static_cast<std::size_t>(thing.archLightingIndex)];
            if (!validArchiveObject(current)
                || !topologyMatchesObject(topologies[selected], current))
            {
                return resultWith(
                    MotsArchLightingEditStatus::Rejected,
                    kResetTopologyMessage);
            }
            if (isDefaultMotsArchLighting(current))
            {
                continue;
            }

            std::size_t shape = 0;
            for (; shape < zeroShapes.size(); ++shape)
            {
                if (zeroShapes[shape] == counts)
                {
                    break;
                }
            }
            if (shape == zeroShapes.size())
            {
                zeroShapes.push_back(std::move(counts));
            }
            shapeForThing[selected] = shape;
            changed.push_back(thingIndex);
        }

        if (changed.empty())
        {
            return resultWith(MotsArchLightingEditStatus::Noop, kNoopMessage);
        }
        if (level.motsArchLighting.size() > kMaxArchLightingItems
            || zeroShapes.size()
                > kMaxArchLightingItems - level.motsArchLighting.size())
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kBudgetMessage);
        }

        std::vector<StagedObject> stagedObjects{};
        stagedObjects.reserve(zeroShapes.size());
        for (std::size_t shape = 0; shape < zeroShapes.size(); ++shape)
        {
            stagedObjects.push_back(StagedObject{
                kNoStagedObject, shape, initializedObject(zeroShapes[shape]) });
        }
        if (!aggregateBudgetAllowsAppend(level, stagedObjects)
            || !assignObjectWireIndices(level, stagedObjects))
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kBudgetMessage);
        }

        MotsArchLightingEditResult result = resultWith(
            MotsArchLightingEditStatus::Applied, kResetMessage);
        result.changedThingIndices = changed;
        const std::int32_t firstNewIndex = static_cast<std::int32_t>(
            level.motsArchLighting.size());
        level.motsArchLighting.reserve(
            level.motsArchLighting.size() + stagedObjects.size());
        for (StagedObject& object : stagedObjects)
        {
            level.motsArchLighting.push_back(std::move(object.object));
        }
        for (std::size_t selected = 0; selected < thingIndices.size(); ++selected)
        {
            if (shapeForThing[selected] != kNoStagedObject)
            {
                level.things[static_cast<std::size_t>(thingIndices[selected])]
                    .archLightingIndex = firstNewIndex
                        + static_cast<std::int32_t>(shapeForThing[selected]);
            }
        }
        return result;
    }

    if (topologies.size() != thingIndices.size())
    {
        return resultWith(
            MotsArchLightingEditStatus::Rejected, kTopologyCountMessage);
    }

    if (edit.kind == MotsArchLightingEditKind::Multiply)
    {
        if (edit.mode == MotsArchLightingVertexMode::Mono
            && !std::isfinite(edit.monoMultiplier))
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kFiniteMessage);
        }
        if (edit.mode == MotsArchLightingVertexMode::Rgb
            && !finiteRgbMultiplier(edit.rgbMultiplier))
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kFiniteMessage);
        }
        if (edit.mode != MotsArchLightingVertexMode::Mono
            && edit.mode != MotsArchLightingVertexMode::Rgb)
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kFiniteMessage);
        }
    }
    else if (edit.kind != MotsArchLightingEditKind::Initialize)
    {
        if (edit.kind != MotsArchLightingEditKind::SetVertex)
        {
            return resultWith(
                MotsArchLightingEditStatus::Rejected,
                "MotS ARCHLIGHTING edit kind is not supported.");
        }
        if (edit.vertexEdits.size() > kMaxArchLightingItems)
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kFiniteMessage);
        }
        if (edit.vertexEdits.empty())
        {
            if (!finiteVertex(edit.vertexValue))
            {
                return resultWith(MotsArchLightingEditStatus::Rejected, kFiniteMessage);
            }
        }
        else if (!std::all_of(
                     edit.vertexEdits.begin(), edit.vertexEdits.end(),
                     [](const MotsArchLightingVertexEdit& vertexEdit)
                     {
                         return finiteEditedVertex(vertexEdit);
                     }))
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kFiniteMessage);
        }
        else if (std::any_of(
                     edit.vertexEdits.begin(), edit.vertexEdits.end(),
                     [&seenThings](const MotsArchLightingVertexEdit& vertexEdit)
                     {
                         return vertexEdit.thingIndex >= 0
                             && !seenThings.contains(
                                 static_cast<std::size_t>(vertexEdit.thingIndex));
                     }))
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kIndexMessage);
        }
        else
        {
            std::unordered_set<SparseVertexKey, SparseVertexKeyHash>
                seenVertexEdits{};
            std::unordered_set<SparseVertexKey, SparseVertexKeyHash>
                seenGlobalTargets{};
            std::unordered_set<SparseVertexKey, SparseVertexKeyHash>
                seenScopedTargets{};
            seenVertexEdits.reserve(edit.vertexEdits.size());
            seenGlobalTargets.reserve(edit.vertexEdits.size());
            seenScopedTargets.reserve(edit.vertexEdits.size());
            for (const MotsArchLightingVertexEdit& vertexEdit : edit.vertexEdits)
            {
                // Every negative scope applies to all selected owners, so
                // normalize it before checking for duplicate logical targets.
                const SparseVertexKey key{
                    vertexEdit.thingIndex < 0 ? -1 : vertexEdit.thingIndex,
                    vertexEdit.meshIndex,
                    vertexEdit.vertexIndex };
                if (!seenVertexEdits.insert(key).second)
                {
                    return resultWith(
                        MotsArchLightingEditStatus::Rejected,
                        kDuplicateVertexMessage);
                }
                const SparseVertexKey coordinateKey{
                    0, vertexEdit.meshIndex, vertexEdit.vertexIndex };
                (vertexEdit.thingIndex < 0 ? seenGlobalTargets
                                            : seenScopedTargets)
                    .insert(coordinateKey);
            }
            for (const SparseVertexKey& globalTarget : seenGlobalTargets)
            {
                if (seenScopedTargets.contains(globalTarget))
                {
                    // A global edit and an owner-scoped edit for the same
                    // vertex have order-dependent results. Reject the
                    // ambiguous payload instead of grouping it incorrectly.
                    return resultWith(
                        MotsArchLightingEditStatus::Rejected,
                        kDuplicateVertexMessage);
                }
            }
        }
    }

    std::vector<bool> needsArchFlagPromotion(
        edit.kind == MotsArchLightingEditKind::Initialize
            ? thingIndices.size() : 0u,
        false);
    std::vector<bool> needsInitializationObject(
        edit.kind == MotsArchLightingEditKind::Initialize
            ? thingIndices.size() : 0u,
        false);
    std::vector<bool> needsVirtualDefaultObject(thingIndices.size(), false);
    std::vector<std::vector<std::size_t>> virtualDefaultVertexCounts(
        thingIndices.size());

    for (std::size_t selected = 0; selected < thingIndices.size(); ++selected)
    {
        const std::int32_t thingIndex = thingIndices[selected];
        const Thing& thing = level.things[static_cast<std::size_t>(thingIndex)];
        const std::int32_t flags = thingFlags(level, templates, thing);
        if ((flags & TF_INVISIBLE) != 0
            && edit.kind == MotsArchLightingEditKind::Initialize)
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kThingMessage);
        }
        if ((flags & kArchFlag) == 0)
        {
            if (edit.kind != MotsArchLightingEditKind::Initialize)
            {
                return resultWith(MotsArchLightingEditStatus::Rejected, kThingMessage);
            }
            needsArchFlagPromotion[selected] = true;
        }
        if (edit.kind == MotsArchLightingEditKind::Initialize)
        {
            if (thing.archLightingIndex < -1
                || (thing.archLightingIndex >= 0
                    && static_cast<std::size_t>(thing.archLightingIndex)
                        >= level.motsArchLighting.size()))
            {
                return resultWith(
                    MotsArchLightingEditStatus::Rejected, kRecordMessage);
            }
            if (thing.archLightingIndex >= 0)
            {
                // An unflagged Thing may carry custom dormant values. Restore
                // them on promotion only when the current 3DO still has the
                // exact mesh/vertex layout they were authored against.
                if (!needsArchFlagPromotion[selected])
                {
                    return resultWith(
                        MotsArchLightingEditStatus::Rejected, kExistingMessage);
                }
                const MotsArchLightObject& dormant = level.motsArchLighting[
                    static_cast<std::size_t>(thing.archLightingIndex)];
                if (!validArchiveObject(dormant)
                    || !topologyMatchesObject(topologies[selected], dormant))
                {
                    return resultWith(
                        MotsArchLightingEditStatus::Rejected,
                        kResetTopologyMessage);
                }
            }
            else
            {
                needsInitializationObject[selected] = true;
            }
        }
        else if (thing.archLightingIndex < -1
            || (thing.archLightingIndex >= 0
                && static_cast<std::size_t>(thing.archLightingIndex)
                    >= level.motsArchLighting.size()))
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kRecordMessage);
        }
        else if (thing.archLightingIndex == -1)
        {
            // An Arch Thing with no explicit record has a virtual all-zero
            // record. Validate and size that baseline before edit planning;
            // it is appended only if the requested operation changes a value.
            virtualDefaultVertexCounts[selected] = maximumMeshVertexCounts(
                topologies[selected]);
            if (virtualDefaultVertexCounts[selected].empty())
            {
                return resultWith(
                    MotsArchLightingEditStatus::Rejected, kTopologyMessage);
            }
            needsVirtualDefaultObject[selected] = true;
        }
    }

    const bool hasVirtualDefaults = std::any_of(
        needsVirtualDefaultObject.begin(), needsVirtualDefaultObject.end(),
        [](const bool needsObject) { return needsObject; });
    if (hasVirtualDefaults && edit.kind == MotsArchLightingEditKind::Multiply)
    {
        const bool allSelectedAreVirtualDefaults = std::all_of(
            thingIndices.begin(), thingIndices.end(),
            [&level](const std::int32_t thingIndex)
            {
                return level.things[static_cast<std::size_t>(thingIndex)]
                    .archLightingIndex == -1;
            });
        const bool multiplierPreservesPositiveZero =
            (edit.mode == MotsArchLightingVertexMode::Mono
                && !std::signbit(edit.monoMultiplier))
            || (edit.mode == MotsArchLightingVertexMode::Rgb
                && !std::signbit(edit.rgbMultiplier.r)
                && !std::signbit(edit.rgbMultiplier.g)
                && !std::signbit(edit.rgbMultiplier.b));
        if (allSelectedAreVirtualDefaults && multiplierPreservesPositiveZero)
        {
            // A finite nonnegative multiplier leaves every channel of the
            // virtual all-zero source bit-identical. Avoid materializing large
            // topology-shaped records for an operation that is a true no-op.
            return resultWith(MotsArchLightingEditStatus::Noop, kNoopMessage);
        }
    }

    std::vector<MotsArchLightObject> virtualDefaultSources(thingIndices.size());
    if (hasVirtualDefaults)
    {
        RecordCounts totals{};
        if (!countLevelRecords(level, totals))
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kBudgetMessage);
        }
        for (std::size_t selected = 0; selected < thingIndices.size(); ++selected)
        {
            if (!needsVirtualDefaultObject[selected])
            {
                continue;
            }
            const std::vector<std::size_t>& counts =
                virtualDefaultVertexCounts[selected];
            if (!addWithinBudget(totals.meshes, counts.size()))
            {
                return resultWith(MotsArchLightingEditStatus::Rejected, kBudgetMessage);
            }
            for (const std::size_t vertexCount : counts)
            {
                if (!addWithinBudget(totals.vertices, vertexCount))
                {
                    return resultWith(MotsArchLightingEditStatus::Rejected, kBudgetMessage);
                }
            }
        }
        for (std::size_t selected = 0; selected < thingIndices.size(); ++selected)
        {
            if (needsVirtualDefaultObject[selected])
            {
                virtualDefaultSources[selected] = initializedObject(
                    virtualDefaultVertexCounts[selected]);
            }
        }
    }

    if (edit.kind == MotsArchLightingEditKind::Multiply
        || edit.kind == MotsArchLightingEditKind::SetVertex)
    {
        std::unordered_set<std::int32_t> validatedSources{};
        validatedSources.reserve(thingIndices.size());
        for (std::size_t selected = 0; selected < thingIndices.size(); ++selected)
        {
            const std::int32_t thingIndex = thingIndices[selected];
            const Thing& thing = level.things[
                static_cast<std::size_t>(thingIndex)];
            const std::int32_t sourceIndex = thing.archLightingIndex;
            const MotsArchLightObject& source = sourceIndex < 0
                ? virtualDefaultSources[selected]
                : level.motsArchLighting[static_cast<std::size_t>(sourceIndex)];
            if ((validatedSources.insert(sourceIndex).second
                    && !validArchiveObject(source))
                || !topologyFitsObject(topologies[selected], source))
            {
                return resultWith(
                    MotsArchLightingEditStatus::Rejected, kTopologyMessage);
            }
            if (edit.kind == MotsArchLightingEditKind::SetVertex
                && (edit.vertexEdits.empty()
                        ? !topologyContainsVertex(
                            topologies[selected], edit.meshIndex, edit.vertexIndex)
                        : !std::all_of(
                            edit.vertexEdits.begin(), edit.vertexEdits.end(),
                            [&topology = topologies[selected], thingIndex](
                                const MotsArchLightingVertexEdit& vertexEdit)
                            {
                                return (vertexEdit.thingIndex >= 0
                                        && vertexEdit.thingIndex != thingIndex)
                                    || topologyContainsVertex(
                                        topology,
                                        vertexEdit.meshIndex,
                                        vertexEdit.vertexIndex);
                            })))
            {
                return resultWith(
                    MotsArchLightingEditStatus::Rejected, kTopologyMessage);
            }
        }
    }

    std::unordered_map<std::int32_t, bool> sourceHasUnselectedOwner{};
    sourceHasUnselectedOwner.reserve(thingIndices.size());
    if (edit.kind == MotsArchLightingEditKind::Multiply
        || edit.kind == MotsArchLightingEditKind::SetVertex)
    {
        for (const std::int32_t thingIndex : thingIndices)
        {
            const std::int32_t sourceIndex = level.things[
                static_cast<std::size_t>(thingIndex)].archLightingIndex;
            if (sourceIndex >= 0)
            {
                sourceHasUnselectedOwner.emplace(sourceIndex, false);
            }
        }
        for (std::size_t thingIndex = 0;
             thingIndex < level.things.size(); ++thingIndex)
        {
            const std::int32_t sourceIndex = level.things[thingIndex].archLightingIndex;
            if (sourceIndex < 0)
            {
                continue;
            }
            const auto source = sourceHasUnselectedOwner.find(sourceIndex);
            if (source != sourceHasUnselectedOwner.end()
                && !seenThings.contains(thingIndex))
            {
                source->second = true;
            }
        }
    }

    std::vector<std::vector<std::size_t>> initializationVertexCounts{};
    std::vector<SourcePlan> sourcePlans{};
    std::vector<std::int32_t> sourcePlanSources{};
    std::vector<std::size_t> sourcePlanRepresentatives{};
    std::vector<std::size_t> sourcePlanForThing(
        thingIndices.size(), kNoStagedObject);
    std::size_t pendingAppends = 0;
    if (edit.kind == MotsArchLightingEditKind::Initialize)
    {
        const std::size_t initializationObjectCount = static_cast<std::size_t>(
            std::count(needsInitializationObject.begin(),
                needsInitializationObject.end(), true));
        if (level.motsArchLighting.size() > kMaxArchLightingItems
            || initializationObjectCount
                > kMaxArchLightingItems - level.motsArchLighting.size())
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kBudgetMessage);
        }

        RecordCounts totals{};
        if (!countLevelRecords(level, totals))
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kBudgetMessage);
        }
        initializationVertexCounts.reserve(thingIndices.size());
        for (std::size_t selected = 0; selected < thingIndices.size(); ++selected)
        {
            if (!needsInitializationObject[selected])
            {
                initializationVertexCounts.emplace_back();
                continue;
            }
            std::vector<std::size_t> counts =
                maximumMeshVertexCounts(topologies[selected]);
            if (counts.empty())
            {
                return resultWith(
                    MotsArchLightingEditStatus::Rejected, kTopologyMessage);
            }
            if (!addWithinBudget(totals.meshes, counts.size()))
            {
                return resultWith(MotsArchLightingEditStatus::Rejected, kBudgetMessage);
            }
            std::size_t vertexCount = 0;
            for (const std::size_t count : counts)
            {
                if (!addWithinBudget(vertexCount, count))
                {
                    return resultWith(
                        MotsArchLightingEditStatus::Rejected, kBudgetMessage);
                }
            }
            if (!addWithinBudget(totals.vertices, vertexCount))
            {
                return resultWith(MotsArchLightingEditStatus::Rejected, kBudgetMessage);
            }
            initializationVertexCounts.push_back(std::move(counts));
        }

        // Topology and aggregate budgets are checked before model admission so
        // malformed snapshots keep their established topology diagnostic. The
        // model-name check is the final eligibility gate for an automatic
        // per-instance Arch-flag promotion.
        for (std::size_t selected = 0; selected < thingIndices.size(); ++selected)
        {
            if (!needsArchFlagPromotion[selected])
            {
                continue;
            }
            const Thing& thing = level.things[
                static_cast<std::size_t>(thingIndices[selected])];
            if (!hasUsableModel(level, templates, thing))
            {
                return resultWith(
                    MotsArchLightingEditStatus::Rejected, kThingMessage);
            }
        }
    }
    else
    {
        sourcePlans.reserve(thingIndices.size());
        sourcePlanSources.reserve(thingIndices.size());
        sourcePlanRepresentatives.reserve(thingIndices.size());
        std::unordered_map<
            std::int32_t,
            std::vector<const MotsArchLightingVertexEdit*>> sparseEditsByThing{};
        if (edit.kind == MotsArchLightingEditKind::SetVertex)
        {
            sparseEditsByThing.reserve(edit.vertexEdits.size());
            for (const MotsArchLightingVertexEdit& vertexEdit : edit.vertexEdits)
            {
                if (vertexEdit.thingIndex >= 0)
                {
                    sparseEditsByThing[vertexEdit.thingIndex].push_back(&vertexEdit);
                }
            }
            const auto sparseEditLess = [](const auto* lhs, const auto* rhs)
            {
                if (lhs->meshIndex != rhs->meshIndex)
                {
                    return lhs->meshIndex < rhs->meshIndex;
                }
                return lhs->vertexIndex < rhs->vertexIndex;
            };
            for (auto& [thingIndex, sparseEdits] : sparseEditsByThing)
            {
                (void)thingIndex;
                std::sort(sparseEdits.begin(), sparseEdits.end(), sparseEditLess);
            }
        }
        const auto sameSparsePayload = [&edit, &sparseEditsByThing](
            const std::int32_t leftThing,
            const std::int32_t rightThing)
        {
            if (edit.kind != MotsArchLightingEditKind::SetVertex)
            {
                return true;
            }
            const auto left = sparseEditsByThing.find(leftThing);
            const auto right = sparseEditsByThing.find(rightThing);
            if ((left == sparseEditsByThing.end()) != (right == sparseEditsByThing.end()))
            {
                return false;
            }
            if (left == sparseEditsByThing.end())
            {
                // Negative scopes are common to every owner. Their duplicate
                // logical targets were rejected before this planning pass.
                return true;
            }
            if (left->second.size() != right->second.size())
            {
                return false;
            }
            const auto sameEdit = [](const auto& lhs, const auto& rhs)
            {
                return lhs.meshIndex == rhs.meshIndex
                    && lhs.vertexIndex == rhs.vertexIndex
                    && lhs.channels == rhs.channels
                    && sameVertexBits(lhs.vertexValue, rhs.vertexValue);
            };
            return std::equal(
                left->second.begin(), left->second.end(),
                right->second.begin(),
                [&sameEdit](const auto* lhs, const auto* rhs)
                {
                    return sameEdit(*lhs, *rhs);
                });
        };
        for (std::size_t selected = 0; selected < thingIndices.size(); ++selected)
        {
            const std::int32_t thingIndex = thingIndices[selected];
            const Thing& thing = level.things[static_cast<std::size_t>(thingIndex)];
            const std::int32_t sourceIndex = thing.archLightingIndex;
            std::size_t planIndex = kNoStagedObject;
            if (sourceIndex >= 0)
            {
                for (std::size_t candidate = 0;
                     candidate < sourcePlans.size(); ++candidate)
                {
                    if (sourcePlanSources[candidate] == sourceIndex
                        && sameSparsePayload(
                            thingIndex,
                            thingIndices[sourcePlanRepresentatives[candidate]]))
                    {
                        planIndex = candidate;
                        break;
                    }
                }
            }
            if (planIndex == kNoStagedObject)
            {
                planIndex = sourcePlans.size();
                sourcePlans.push_back(SourcePlan{
                    false,
                    sourceIndex < 0 || sourceHasUnselectedOwner[sourceIndex] });
                sourcePlanSources.push_back(sourceIndex);
                sourcePlanRepresentatives.push_back(selected);
            }
            sourcePlanForThing[selected] = planIndex;
        }
        for (std::size_t selected = 0; selected < thingIndices.size(); ++selected)
        {
            const std::size_t planIndex = sourcePlanForThing[selected];
            SourcePlan& plan = sourcePlans[planIndex];
            const std::int32_t thingIndex = thingIndices[selected];
            const std::int32_t sourceIndex = sourcePlanSources[planIndex];
            const std::size_t representative =
                sourcePlanRepresentatives[planIndex];
            const MotsArchLightObject& source = sourceIndex < 0
                ? virtualDefaultSources[representative]
                : level.motsArchLighting[static_cast<std::size_t>(sourceIndex)];
            bool changes = false;
            const bool previewSucceeded = edit.kind == MotsArchLightingEditKind::Multiply
                ? previewMultiplier(source, edit, changes)
                : previewSetVertex(source, edit, changes, thingIndex);
            if (!previewSucceeded)
            {
                return resultWith(
                    MotsArchLightingEditStatus::Rejected, kFiniteMessage);
            }
            plan.changes = plan.changes || changes;
        }
        for (std::size_t planIndex = 0; planIndex < sourcePlans.size(); ++planIndex)
        {
            const std::int32_t sourceIndex = sourcePlanSources[planIndex];
            const bool hasVariants = std::count(
                sourcePlanSources.begin(), sourcePlanSources.end(), sourceIndex) > 1;
            if (hasVariants)
            {
                sourcePlans[planIndex].append = true;
            }
            if (sourcePlans[planIndex].changes && sourcePlans[planIndex].append)
            {
                ++pendingAppends;
            }
        }

        if (pendingAppends > 0)
        {
            if (level.motsArchLighting.size() > kMaxArchLightingItems
                || pendingAppends
                    > kMaxArchLightingItems - level.motsArchLighting.size())
            {
                return resultWith(MotsArchLightingEditStatus::Rejected, kBudgetMessage);
            }
            RecordCounts totals{};
            if (!countLevelRecords(level, totals))
            {
                return resultWith(MotsArchLightingEditStatus::Rejected, kBudgetMessage);
            }
            for (std::size_t planIndex = 0;
                 planIndex < sourcePlans.size(); ++planIndex)
            {
                const SourcePlan& plan = sourcePlans[planIndex];
                const std::int32_t sourceIndex = sourcePlanSources[planIndex];
                if (!plan.changes || !plan.append)
                {
                    continue;
                }
                const std::size_t representative =
                    sourcePlanRepresentatives[planIndex];
                const MotsArchLightObject& source = sourceIndex < 0
                    ? virtualDefaultSources[representative]
                    : level.motsArchLighting[
                        static_cast<std::size_t>(sourceIndex)];
                RecordCounts sourceCounts{};
                if (!countObjectRecords(source, sourceCounts)
                    || !addWithinBudget(totals.meshes, sourceCounts.meshes)
                    || !addWithinBudget(totals.vertices, sourceCounts.vertices))
                {
                    return resultWith(MotsArchLightingEditStatus::Rejected, kBudgetMessage);
                }
            }
        }
    }

    std::size_t plannedStages = 0;
    if (edit.kind == MotsArchLightingEditKind::Initialize)
    {
        plannedStages = static_cast<std::size_t>(
            std::count(needsInitializationObject.begin(),
                needsInitializationObject.end(), true));
    }
    else
    {
        for (const SourcePlan& plan : sourcePlans)
        {
            if (plan.changes)
            {
                ++plannedStages;
            }
        }
    }
    std::vector<StagedObject> stagedObjects{};
    stagedObjects.reserve(plannedStages);
    std::vector<StagedReference> stagedReferences{};
    stagedReferences.reserve(thingIndices.size());
    std::vector<std::size_t> clones(
        edit.kind == MotsArchLightingEditKind::Initialize
            ? 0 : sourcePlans.size(),
        kNoStagedObject);
    std::vector<std::int32_t> changedThingIndices{};
    changedThingIndices.reserve(thingIndices.size());
    std::size_t appendCount = 0;

    // Build local flag overrides before archive publication.  A copied value
    // vector is the transaction's prepared instance state; publishing it is a
    // non-throwing vector move after all archive allocations are reserved.
    std::vector<std::optional<std::vector<TplValue>>> stagedThingValues{};
    if (edit.kind == MotsArchLightingEditKind::Initialize)
    {
        stagedThingValues.resize(thingIndices.size());
        for (std::size_t selected = 0; selected < thingIndices.size(); ++selected)
        {
            if (!needsArchFlagPromotion[selected])
            {
                continue;
            }
            const Thing& thing = level.things[
                static_cast<std::size_t>(thingIndices[selected])];
            auto& values = stagedThingValues[selected].emplace(thing.values);
            values.reserve(values.size() + 1u);
            promoteThingFlags(
                values, thingFlags(level, templates, thing));
        }
    }

    for (std::size_t selected = 0; selected < thingIndices.size(); ++selected)
    {
        const std::size_t thingIndex =
            static_cast<std::size_t>(thingIndices[selected]);
        const Thing& thing = level.things[thingIndex];

        if (edit.kind == MotsArchLightingEditKind::Initialize)
        {
            if (needsInitializationObject[selected])
            {
                stagedObjects.push_back(StagedObject{
                    kNoStagedObject,
                    appendCount,
                    initializedObject(initializationVertexCounts[selected]) });
                ++appendCount;
                stagedReferences.push_back(StagedReference{
                    thingIndex, appendCount - 1 });
            }
            changedThingIndices.push_back(static_cast<std::int32_t>(thingIndex));
            continue;
        }

        const std::int32_t sourceIndex = thing.archLightingIndex;
        const std::size_t planIndex = sourcePlanForThing[selected];
        if (planIndex >= sourcePlans.size())
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kTopologyMessage);
        }
        if (clones[planIndex] != kNoStagedObject)
        {
            const StagedObject& staged = stagedObjects[clones[planIndex]];
            changedThingIndices.push_back(static_cast<std::int32_t>(thingIndex));
            if (staged.appendOrdinal != kNoStagedObject)
            {
                stagedReferences.push_back(StagedReference{
                    thingIndex, staged.appendOrdinal });
            }
            continue;
        }

        const MotsArchLightObject& source = sourceIndex < 0
            ? virtualDefaultSources[sourcePlanRepresentatives[planIndex]]
            : level.motsArchLighting[static_cast<std::size_t>(sourceIndex)];
        if (!sourcePlans[planIndex].changes)
        {
            continue;
        }

        MotsArchLightObject edited = source;
        const bool editedSuccessfully = edit.kind == MotsArchLightingEditKind::Multiply
            ? applyMultiplier(edited, edit)
            : applySetVertex(edited, edit, thingIndex);
        if (!editedSuccessfully)
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kFiniteMessage);
        }

        const bool shouldAppend = sourcePlans[planIndex].append;
        const std::size_t destination = shouldAppend
            ? kNoStagedObject : static_cast<std::size_t>(sourceIndex);
        const std::size_t appendOrdinal = shouldAppend
            ? appendCount++ : kNoStagedObject;
        stagedObjects.push_back(StagedObject{
            destination, appendOrdinal, std::move(edited) });
        clones[planIndex] = stagedObjects.size() - 1;
        changedThingIndices.push_back(static_cast<std::int32_t>(thingIndex));
        if (appendOrdinal != kNoStagedObject)
        {
            stagedReferences.push_back(StagedReference{
                thingIndex, appendOrdinal });
        }
    }

    if (stagedObjects.empty())
    {
        return resultWith(MotsArchLightingEditStatus::Noop, kNoopMessage);
    }
    if (level.motsArchLighting.size() > kMaxArchLightingItems
        || appendCount
            > kMaxArchLightingItems - level.motsArchLighting.size())
    {
        return resultWith(MotsArchLightingEditStatus::Rejected, kBudgetMessage);
    }
    if (!aggregateBudgetAllowsAppend(level, stagedObjects))
    {
        return resultWith(MotsArchLightingEditStatus::Rejected, kBudgetMessage);
    }
    if (!assignObjectWireIndices(level, stagedObjects))
    {
        return resultWith(MotsArchLightingEditStatus::Rejected, kBudgetMessage);
    }

    MotsArchLightingEditResult result = resultWith(
        MotsArchLightingEditStatus::Applied,
        edit.kind == MotsArchLightingEditKind::Initialize
            ? kInitializeMessage
            : (edit.kind == MotsArchLightingEditKind::SetVertex
                    ? kSetMessage
                    : (edit.mode == MotsArchLightingVertexMode::Mono
                            ? kMonoMultiplyMessage : kMultiplyMessage)));
    result.changedThingIndices.reserve(changedThingIndices.size());
    const std::int32_t firstNewIndex = static_cast<std::int32_t>(
        level.motsArchLighting.size());
    for (const std::int32_t thingIndex : changedThingIndices)
    {
        result.changedThingIndices.push_back(thingIndex);
    }

    // Reserve before publishing. Moves of the already-staged archive objects
    // and scalar Thing index assignments below are non-throwing.
    level.motsArchLighting.reserve(
        level.motsArchLighting.size() + appendCount);
    for (StagedObject& object : stagedObjects)
    {
        if (object.destination == kNoStagedObject)
        {
            level.motsArchLighting.push_back(std::move(object.object));
        }
        else
        {
            level.motsArchLighting[object.destination] = std::move(object.object);
        }
    }
    for (std::size_t referenceIndex = 0;
         referenceIndex < stagedReferences.size(); ++referenceIndex)
    {
        const StagedReference& reference = stagedReferences[referenceIndex];
        level.things[reference.thingIndex].archLightingIndex =
            firstNewIndex + static_cast<std::int32_t>(reference.appendOrdinal);
    }
    for (std::size_t selected = 0; selected < thingIndices.size(); ++selected)
    {
        if (stagedThingValues.empty() || !stagedThingValues[selected])
        {
            continue;
        }
        level.things[static_cast<std::size_t>(thingIndices[selected])].values =
            std::move(*stagedThingValues[selected]);
    }
    return result;
}

MotsArchLightingEditResult copyMotsArchLighting(
    Level& level,
    const std::int32_t sourceThingIndex,
    const std::string_view sourceModelIdentity,
    const MotsArchLightingTopology& sourceTopology,
    const std::span<const MotsArchLightingCopyTarget> targets,
    const assets::Templates* templates)
{
    if (level.kind != ProjectType::MOTS)
    {
        return resultWith(MotsArchLightingEditStatus::Rejected, kProjectMessage);
    }
    if (targets.empty())
    {
        return resultWith(MotsArchLightingEditStatus::Noop, kNoopMessage);
    }
    if (targets.size() > kMaxArchLightingItems)
    {
        return resultWith(MotsArchLightingEditStatus::Rejected, kBudgetMessage);
    }
    if (sourceThingIndex < 0
        || static_cast<std::size_t>(sourceThingIndex) >= level.things.size())
    {
        return resultWith(MotsArchLightingEditStatus::Rejected, kIndexMessage);
    }
    if (sourceModelIdentity.empty())
    {
        return resultWith(
            MotsArchLightingEditStatus::Rejected, kCopyEmptyIdentityMessage);
    }

    const Thing& sourceThing = level.things[
        static_cast<std::size_t>(sourceThingIndex)];
    if ((thingFlags(level, templates, sourceThing) & kArchFlag) == 0)
    {
        return resultWith(MotsArchLightingEditStatus::Rejected, kThingMessage);
    }
    if (sourceThing.archLightingIndex < 0
        || static_cast<std::size_t>(sourceThing.archLightingIndex)
            >= level.motsArchLighting.size())
    {
        return resultWith(MotsArchLightingEditStatus::Rejected, kRecordMessage);
    }

    const std::int32_t sourceRecordIndex = sourceThing.archLightingIndex;
    const MotsArchLightObject& sourceRecord = level.motsArchLighting[
        static_cast<std::size_t>(sourceRecordIndex)];
    if (!validArchiveStructure(sourceRecord))
    {
        return resultWith(
            MotsArchLightingEditStatus::Rejected, kCopyStructureMessage);
    }
    if (!finiteArchiveChannels(sourceRecord))
    {
        return resultWith(
            MotsArchLightingEditStatus::Rejected, kCopyFiniteMessage);
    }
    if (!topologyFitsObject(sourceTopology, sourceRecord))
    {
        return resultWith(
            MotsArchLightingEditStatus::Rejected, kCopyTopologyMessage);
    }

    std::unordered_set<std::size_t> seenTargets{};
    seenTargets.reserve(targets.size());
    std::vector<std::int32_t> changedThingIndices{};
    changedThingIndices.reserve(targets.size());
    std::vector<std::pair<std::size_t, std::vector<TplValue>>> stagedFlags{};
    stagedFlags.reserve(targets.size());

    // Validate the complete target batch before publishing any binding.  The
    // application resolves model identity/revision before it calls this
    // core operation; that identity is authoritative for the hierarchy.
    // Exact all-LOD topology equality also catches a stale count/order
    // snapshot before a binding is published.
    for (const MotsArchLightingCopyTarget& target : targets)
    {
        if (target.thingIndex < 0
            || static_cast<std::size_t>(target.thingIndex) >= level.things.size())
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kIndexMessage);
        }
        const std::size_t targetIndex =
            static_cast<std::size_t>(target.thingIndex);
        if (!seenTargets.insert(targetIndex).second)
        {
            return resultWith(
                MotsArchLightingEditStatus::Rejected, kDuplicateMessage);
        }
        if (target.modelIdentity.empty())
        {
            return resultWith(
                MotsArchLightingEditStatus::Rejected, kCopyTargetIdentityMessage);
        }
        if (target.modelIdentity != sourceModelIdentity)
        {
            return resultWith(
                MotsArchLightingEditStatus::Rejected, kCopyIdentityMessage);
        }
        if (!sameTopology(target.topology, sourceTopology))
        {
            return resultWith(
                MotsArchLightingEditStatus::Rejected, kCopyTopologyMessage);
        }

        const Thing& targetThing = level.things[targetIndex];
        const std::int32_t targetFlags = thingFlags(
            level, templates, targetThing);
        const bool promoteTarget = (targetFlags & kArchFlag) == 0;
        if (promoteTarget
            && ((targetFlags & TF_INVISIBLE) != 0
                || !hasUsableModel(level, templates, targetThing)))
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kThingMessage);
        }
        if (targetThing.archLightingIndex < -1
            || (targetThing.archLightingIndex != -1
                && static_cast<std::size_t>(targetThing.archLightingIndex)
                    >= level.motsArchLighting.size()))
        {
            return resultWith(MotsArchLightingEditStatus::Rejected, kRecordMessage);
        }

        if (targetThing.archLightingIndex != sourceRecordIndex || promoteTarget)
        {
            changedThingIndices.push_back(target.thingIndex);
        }
        if (promoteTarget)
        {
            std::vector<TplValue> values = targetThing.values;
            values.reserve(values.size() + 1u);
            promoteThingFlags(values, targetFlags);
            stagedFlags.emplace_back(targetIndex, std::move(values));
        }
    }

    if (changedThingIndices.empty())
    {
        return resultWith(MotsArchLightingEditStatus::Noop, kNoopMessage);
    }

    MotsArchLightingEditResult result = resultWith(
        MotsArchLightingEditStatus::Applied, kCopyMessage);
    result.changedThingIndices = std::move(changedThingIndices);

    // The source archive object is already immutable in this operation. Bind
    // changed owners to that slot; subsequent value edits use the existing
    // source/unselected-owner COW path. No archive-table clone is needed.
    for (const std::int32_t thingIndex : result.changedThingIndices)
    {
        level.things[static_cast<std::size_t>(thingIndex)].archLightingIndex =
            sourceRecordIndex;
    }
    for (auto& [thingIndex, values] : stagedFlags)
    {
        level.things[thingIndex].values = std::move(values);
    }

    return result;
}

} // namespace sed::level
