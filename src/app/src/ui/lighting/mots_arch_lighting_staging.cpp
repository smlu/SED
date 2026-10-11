#include "ui/lighting/mots_arch_lighting_dialog.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sed::ui::lighting
{

namespace
{

constexpr std::size_t kArchLightingBudget = 1'000'000u;

constexpr std::string_view kNoopMessage =
    "No MotS ARCHLIGHTING changes are staged.";
constexpr std::string_view kInvalidPlanMessage =
    "MotS ARCHLIGHTING staging data is invalid.";
constexpr std::string_view kResourceMessage =
    "The selected ARCHLIGHTING model or topology changed while the dialog was open. Reopen the dialog and try again.";
constexpr std::string_view kCurrentMessage =
    "MotS ARCHLIGHTING resources are current.";

struct RecordCounts
{
    std::size_t records{};
    std::size_t meshes{};
    std::size_t vertices{};
};

struct SourcePreview
{
    std::size_t stageIndex{};
    MotsArchLightObject object{};
};

bool finiteVertex(const MotsArchLightVertex& vertex) noexcept
{
    return std::isfinite(vertex.mono)
        && std::isfinite(vertex.red)
        && std::isfinite(vertex.green)
        && std::isfinite(vertex.blue);
}

bool sameVertexBits(
    const MotsArchLightVertex& left,
    const MotsArchLightVertex& right) noexcept
{
    return std::bit_cast<std::uint32_t>(left.mono)
            == std::bit_cast<std::uint32_t>(right.mono)
        && std::bit_cast<std::uint32_t>(left.red)
            == std::bit_cast<std::uint32_t>(right.red)
        && std::bit_cast<std::uint32_t>(left.green)
            == std::bit_cast<std::uint32_t>(right.green)
        && std::bit_cast<std::uint32_t>(left.blue)
            == std::bit_cast<std::uint32_t>(right.blue);
}

bool addBudget(std::size_t& total, const std::size_t amount) noexcept
{
    if (amount > kArchLightingBudget
        || total > kArchLightingBudget - amount)
    {
        return false;
    }
    total += amount;
    return true;
}

bool validArchiveObject(const MotsArchLightObject& object)
{
    if (object.wireIndex < 0
        || object.meshes.size() > kArchLightingBudget)
    {
        return false;
    }

    std::unordered_set<std::int32_t> meshWireIndices{};
    meshWireIndices.reserve(object.meshes.size());
    std::size_t vertexTotal = 0;
    for (const MotsArchLightMesh& mesh : object.meshes)
    {
        if (mesh.wireIndex < 0
            || !meshWireIndices.insert(mesh.wireIndex).second
            || mesh.vertices.size() > kArchLightingBudget
            || !addBudget(vertexTotal, mesh.vertices.size()))
        {
            return false;
        }
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

bool topologyFitsObject(
    const level::MotsArchLightingTopology& topology,
    const MotsArchLightObject& object) noexcept
{
    if (topology.geosets.empty()
        || topology.geosets.size() > kArchLightingBudget)
    {
        return false;
    }

    for (const std::vector<std::size_t>& geoset : topology.geosets)
    {
        if (geoset.size() > object.meshes.size()
            || geoset.size() > kArchLightingBudget)
        {
            return false;
        }
        for (std::size_t meshIndex = 0; meshIndex < geoset.size(); ++meshIndex)
        {
            if (geoset[meshIndex] > kArchLightingBudget
                || geoset[meshIndex]
                    > object.meshes[meshIndex].vertices.size())
            {
                return false;
            }
        }
    }
    return true;
}

bool topologyContainsVertex(
    const level::MotsArchLightingTopology& topology,
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

std::optional<std::vector<std::size_t>> maximumMeshVertexCounts(
    const level::MotsArchLightingTopology& topology)
{
    if (topology.geosets.empty()
        || topology.geosets.size() > kArchLightingBudget)
    {
        return std::nullopt;
    }

    std::size_t meshCount = 0;
    for (const std::vector<std::size_t>& geoset : topology.geosets)
    {
        if (geoset.size() > kArchLightingBudget)
        {
            return std::nullopt;
        }
        for (const std::size_t vertexCount : geoset)
        {
            if (vertexCount > kArchLightingBudget)
            {
                return std::nullopt;
            }
        }
        meshCount = std::max(meshCount, geoset.size());
    }
    if (meshCount == 0)
    {
        return std::nullopt;
    }

    std::vector<std::size_t> vertexCounts(meshCount, 0);
    std::size_t vertexTotal = 0;
    for (const std::vector<std::size_t>& geoset : topology.geosets)
    {
        for (std::size_t meshIndex = 0; meshIndex < geoset.size(); ++meshIndex)
        {
            vertexCounts[meshIndex] = std::max(
                vertexCounts[meshIndex], geoset[meshIndex]);
        }
    }
    for (const std::size_t vertexCount : vertexCounts)
    {
        if (!addBudget(vertexTotal, vertexCount))
        {
            return std::nullopt;
        }
    }
    return vertexCounts;
}

MotsArchLightObject defaultObject(
    const std::vector<std::size_t>& vertexCounts)
{
    MotsArchLightObject object{};
    object.meshes.reserve(vertexCounts.size());
    for (std::size_t meshIndex = 0; meshIndex < vertexCounts.size(); ++meshIndex)
    {
        MotsArchLightMesh mesh{};
        mesh.wireIndex = static_cast<std::int32_t>(meshIndex);
        mesh.vertices.assign(
            vertexCounts[meshIndex],
            MotsArchLightVertex{ 0.0f, 0.0f, 0.0f, 0.0f });
        object.meshes.push_back(std::move(mesh));
    }
    return object;
}

const MotsArchLightObject* stageBaseline(
    const MotsArchLightingRecordStage& stage) noexcept
{
    if (stage.originalRecord)
    {
        return &*stage.originalRecord;
    }
    if (stage.stagedRecord)
    {
        return &*stage.stagedRecord;
    }
    return nullptr;
}

bool addRecordCounts(
    RecordCounts& counts,
    const MotsArchLightObject& object) noexcept
{
    if (!addBudget(counts.meshes, object.meshes.size()))
    {
        return false;
    }
    for (const MotsArchLightMesh& mesh : object.meshes)
    {
        if (!addBudget(counts.vertices, mesh.vertices.size()))
        {
            return false;
        }
    }
    return true;
}

bool countPlanRecords(
    const MotsArchLightingPlan& plan,
    RecordCounts& counts,
    const bool includeNewStages = true) noexcept
{
    if (plan.records.size() > kArchLightingBudget)
    {
        return false;
    }
    for (const MotsArchLightingRecordStage& stage : plan.records)
    {
        if (!includeNewStages
            && stage.originalRecordIndex < 0
            && !stage.originalRecord)
        {
            continue;
        }
        const MotsArchLightObject* object = stageBaseline(stage);
        if (!object || !addRecordCounts(counts, *object))
        {
            return false;
        }
        if (!addBudget(counts.records, 1))
        {
            return false;
        }
    }
    return true;
}

const MotsArchLightingTarget* targetForThing(
    const MotsArchLightingPlan& plan,
    const std::int32_t thingIndex) noexcept
{
    const auto found = std::find_if(
        plan.targets.begin(), plan.targets.end(),
        [thingIndex](const MotsArchLightingTarget& target)
        {
            return target.thingIndex == thingIndex;
        });
    return found == plan.targets.end() ? nullptr : &*found;
}

MotsArchLightingTarget* targetForThing(
    MotsArchLightingPlan& plan,
    const std::int32_t thingIndex) noexcept
{
    const auto found = std::find_if(
        plan.targets.begin(), plan.targets.end(),
        [thingIndex](const MotsArchLightingTarget& target)
        {
            return target.thingIndex == thingIndex;
        });
    return found == plan.targets.end() ? nullptr : &*found;
}

bool validSelection(const MotsArchLightingPlan& plan)
{
    if (plan.selectedThingIndices.size() > kArchLightingBudget)
    {
        return false;
    }
    std::unordered_set<std::size_t> seen{};
    seen.reserve(plan.selectedThingIndices.size());
    for (const std::int32_t thingIndex : plan.selectedThingIndices)
    {
        if (thingIndex < 0
            || !targetForThing(plan, thingIndex)
            || !seen.insert(static_cast<std::size_t>(thingIndex)).second)
        {
            return false;
        }
    }
    return true;
}

bool validMode(const level::MotsArchLightingVertexMode mode) noexcept
{
    return mode == level::MotsArchLightingVertexMode::Mono
        || mode == level::MotsArchLightingVertexMode::Rgb;
}

bool finiteRgb(const ColorF& value) noexcept
{
    return std::isfinite(value.r)
        && std::isfinite(value.g)
        && std::isfinite(value.b);
}

bool applyMultiply(
    MotsArchLightObject& object,
    const level::MotsArchLightingVertexMode mode,
    const float monoMultiplier,
    const ColorF& rgbMultiplier,
    bool& changed) noexcept
{
    changed = false;
    if (!validMode(mode))
    {
        return false;
    }
    for (MotsArchLightMesh& mesh : object.meshes)
    {
        for (MotsArchLightVertex& vertex : mesh.vertices)
        {
            const MotsArchLightVertex before = vertex;
            if (mode == level::MotsArchLightingVertexMode::Mono)
            {
                vertex.mono *= monoMultiplier;
            }
            else
            {
                vertex.red *= rgbMultiplier.r;
                vertex.green *= rgbMultiplier.g;
                vertex.blue *= rgbMultiplier.b;
            }
            if (!finiteVertex(vertex))
            {
                return false;
            }
            changed = changed || !sameVertexBits(before, vertex);
        }
    }
    return true;
}

bool applySetVertex(
    MotsArchLightObject& object,
    const std::size_t meshIndex,
    const std::size_t vertexIndex,
    const MotsArchLightVertex& value,
    bool& changed) noexcept
{
    if (meshIndex >= object.meshes.size()
        || vertexIndex >= object.meshes[meshIndex].vertices.size()
        || !finiteVertex(value))
    {
        return false;
    }
    MotsArchLightVertex& destination =
        object.meshes[meshIndex].vertices[vertexIndex];
    changed = !sameVertexBits(destination, value);
    destination = value;
    return true;
}

bool selectedStageSources(
    const MotsArchLightingPlan& plan,
    std::vector<SourcePreview>& previews,
    std::unordered_map<std::int32_t, std::size_t>& stageMap)
{
    if (!validSelection(plan))
    {
        return false;
    }

    previews.clear();
    stageMap.clear();
    stageMap.reserve(plan.selectedThingIndices.size());
    for (const std::int32_t thingIndex : plan.selectedThingIndices)
    {
        const MotsArchLightingTarget* target =
            targetForThing(plan, thingIndex);
        if (!target
            || !target->modelAvailable
            || !target->valueEditingAllowed
            || target->recordStageIndex < 0
            || static_cast<std::size_t>(target->recordStageIndex)
                >= plan.records.size())
        {
            return false;
        }

        const std::size_t stageIndex =
            static_cast<std::size_t>(target->recordStageIndex);
        const auto [stageIt, inserted] = stageMap.emplace(
            target->recordStageIndex, previews.size());
        if (inserted)
        {
            const MotsArchLightObject* source =
                stageBaseline(plan.records[stageIndex]);
            if (!source || !validArchiveObject(*source))
            {
                return false;
            }
            previews.push_back(SourcePreview{ stageIndex, *source });
        }

        const MotsArchLightObject& source = previews[stageIt->second].object;
        if (!topologyFitsObject(target->topology, source))
        {
            return false;
        }
    }
    return true;
}

void clearPendingInitialization(MotsArchLightingPlan& plan)
{
    std::vector<std::size_t> remap(plan.records.size(),
        std::numeric_limits<std::size_t>::max());
    std::vector<MotsArchLightingRecordStage> retained{};
    retained.reserve(plan.records.size());
    for (std::size_t index = 0; index < plan.records.size(); ++index)
    {
        MotsArchLightingRecordStage& stage = plan.records[index];
        if (stage.originalRecordIndex < 0 && !stage.originalRecord)
        {
            continue;
        }
        stage.stagedRecord = stage.originalRecord;
        remap[index] = retained.size();
        retained.push_back(std::move(stage));
    }
    plan.records = std::move(retained);
    for (MotsArchLightingTarget& target : plan.targets)
    {
        if (target.recordStageIndex < 0
            || static_cast<std::size_t>(target.recordStageIndex) >= remap.size()
            || remap[static_cast<std::size_t>(target.recordStageIndex)]
                == std::numeric_limits<std::size_t>::max())
        {
            if (target.recordStageIndex >= 0)
            {
                target.recordStageIndex = -1;
            }
            continue;
        }
        target.recordStageIndex = static_cast<std::int32_t>(
            remap[static_cast<std::size_t>(target.recordStageIndex)]);
    }
}

bool sameTargetIdentity(
    const MotsArchLightingTarget& expected,
    const MotsArchLightingTarget& current) noexcept
{
    return expected.thingIndex == current.thingIndex
        && expected.modelName == current.modelName
        && expected.resourceRevision == current.resourceRevision
        && expected.topology.geosets == current.topology.geosets
        && expected.originalRecordIndex == current.originalRecordIndex
        && expected.hasArchFlag == current.hasArchFlag
        && expected.modelAvailable == current.modelAvailable
        && expected.valueEditingAllowed == current.valueEditingAllowed;
}

} // namespace

MotsArchLightingStaging::MotsArchLightingStaging(MotsArchLightingPlan plan)
    : plan_(std::move(plan))
{
    std::unordered_map<std::int32_t, std::size_t> sourceStages{};
    sourceStages.reserve(plan_.records.size());
    std::vector<std::size_t> remap(
        plan_.records.size(), std::numeric_limits<std::size_t>::max());
    std::vector<MotsArchLightingRecordStage> normalized{};
    normalized.reserve(plan_.records.size());
    for (std::size_t index = 0; index < plan_.records.size(); ++index)
    {
        MotsArchLightingRecordStage& stage = plan_.records[index];
        if (stage.originalRecord && !stage.stagedRecord)
        {
            stage.stagedRecord = stage.originalRecord;
        }
        if (stage.originalRecordIndex >= 0)
        {
            const auto [found, inserted] = sourceStages.emplace(
                stage.originalRecordIndex, normalized.size());
            if (!inserted)
            {
                MotsArchLightingRecordStage& existing = normalized[found->second];
                if (!existing.originalRecord && stage.originalRecord)
                {
                    existing.originalRecord = std::move(stage.originalRecord);
                }
                if (!existing.stagedRecord && stage.stagedRecord)
                {
                    existing.stagedRecord = std::move(stage.stagedRecord);
                }
                remap[index] = found->second;
                continue;
            }
        }
        remap[index] = normalized.size();
        normalized.push_back(std::move(stage));
    }
    plan_.records = std::move(normalized);
    for (MotsArchLightingTarget& target : plan_.targets)
    {
        if (target.recordStageIndex < 0
            || static_cast<std::size_t>(target.recordStageIndex) >= remap.size()
            || remap[static_cast<std::size_t>(target.recordStageIndex)]
                == std::numeric_limits<std::size_t>::max())
        {
            target.recordStageIndex = -1;
            continue;
        }
        target.recordStageIndex = static_cast<std::int32_t>(
            remap[static_cast<std::size_t>(target.recordStageIndex)]);
    }
    plan_.hasPendingEdit = false;
}

const std::vector<MotsArchLightingTarget>&
MotsArchLightingStaging::targets() const noexcept
{
    return plan_.targets;
}

const std::vector<MotsArchLightingRecordStage>&
MotsArchLightingStaging::records() const noexcept
{
    return plan_.records;
}

const std::vector<std::int32_t>&
MotsArchLightingStaging::selectedThingIndices() const noexcept
{
    return plan_.selectedThingIndices;
}

std::vector<level::MotsArchLightingTopology>
MotsArchLightingStaging::selectedTopologies() const
{
    std::vector<level::MotsArchLightingTopology> topologies{};
    if (!validSelection(plan_))
    {
        return topologies;
    }
    topologies.reserve(plan_.selectedThingIndices.size());
    for (const std::int32_t thingIndex : plan_.selectedThingIndices)
    {
        const MotsArchLightingTarget* target = targetForThing(plan_, thingIndex);
        if (!target)
        {
            return {};
        }
        topologies.push_back(target->topology);
    }
    return topologies;
}

const MotsArchLightingPlan& MotsArchLightingStaging::plan() const noexcept
{
    return plan_;
}

bool MotsArchLightingStaging::hasChanges() const noexcept
{
    return plan_.hasPendingEdit;
}

bool MotsArchLightingStaging::setSelectedThingIndices(
    std::vector<std::int32_t> thingIndices)
{
    if (plan_.hasPendingEdit
        || thingIndices.size() > kArchLightingBudget)
    {
        return false;
    }
    std::unordered_set<std::size_t> seen{};
    seen.reserve(thingIndices.size());
    for (const std::int32_t thingIndex : thingIndices)
    {
        if (thingIndex < 0
            || !targetForThing(plan_, thingIndex)
            || !seen.insert(static_cast<std::size_t>(thingIndex)).second)
        {
            return false;
        }
    }
    plan_.selectedThingIndices = std::move(thingIndices);
    return true;
}

bool MotsArchLightingStaging::initializeDefault()
{
    const bool repeat = plan_.hasPendingEdit
        && plan_.edit.kind == level::MotsArchLightingEditKind::Initialize;
    if (plan_.hasPendingEdit && !repeat)
    {
        return false;
    }
    if (!validSelection(plan_)
        || plan_.selectedThingIndices.empty())
    {
        return false;
    }

    std::vector<std::vector<std::size_t>> counts{};
    counts.reserve(plan_.selectedThingIndices.size());
    RecordCounts totals{};
    if (!countPlanRecords(plan_, totals, !repeat)
        || totals.records > kArchLightingBudget
        || plan_.selectedThingIndices.size()
            > kArchLightingBudget - totals.records)
    {
        return false;
    }

    for (const std::int32_t thingIndex : plan_.selectedThingIndices)
    {
        const MotsArchLightingTarget* target =
            targetForThing(plan_, thingIndex);
        bool hasPendingInitializationStage = false;
        if (repeat && target && target->recordStageIndex >= 0
            && static_cast<std::size_t>(target->recordStageIndex)
                < plan_.records.size())
        {
            const MotsArchLightingRecordStage& stage = plan_.records[
                static_cast<std::size_t>(target->recordStageIndex)];
            hasPendingInitializationStage =
                stage.originalRecordIndex < 0 && !stage.originalRecord;
        }
        if (!target
            || !target->modelAvailable
            || !target->valueEditingAllowed
            || target->originalRecordIndex != -1
            || (target->recordStageIndex != -1
                && !hasPendingInitializationStage))
        {
            return false;
        }
        const std::optional<std::vector<std::size_t>> maximum =
            maximumMeshVertexCounts(target->topology);
        if (!maximum
            || !addBudget(totals.meshes, maximum->size()))
        {
            return false;
        }
        std::size_t vertices = 0;
        for (const std::size_t count : *maximum)
        {
            if (!addBudget(vertices, count))
            {
                return false;
            }
        }
        if (!addBudget(totals.vertices, vertices))
        {
            return false;
        }
        counts.push_back(*maximum);
    }

    if (repeat)
    {
        clearPendingInitialization(plan_);
    }
    plan_.edit = level::MotsArchLightingEdit{};
    plan_.edit.kind = level::MotsArchLightingEditKind::Initialize;
    plan_.edit.mode = level::MotsArchLightingVertexMode::Rgb;
    for (std::size_t index = 0; index < counts.size(); ++index)
    {
        MotsArchLightingRecordStage stage{};
        stage.originalRecordIndex = -1;
        stage.stagedRecord = defaultObject(counts[index]);
        const std::size_t stageIndex = plan_.records.size();
        if (stageIndex > static_cast<std::size_t>(
                std::numeric_limits<std::int32_t>::max()))
        {
            return false;
        }
        plan_.records.push_back(std::move(stage));
        MotsArchLightingTarget* target = targetForThing(
            plan_, plan_.selectedThingIndices[index]);
        if (!target)
        {
            return false;
        }
        target->recordStageIndex = static_cast<std::int32_t>(stageIndex);
    }
    plan_.hasPendingEdit = true;
    return true;
}

bool MotsArchLightingStaging::multiply(
    const level::MotsArchLightingVertexMode mode,
    const float monoMultiplier,
    const ColorF& rgbMultiplier)
{
    if (plan_.hasPendingEdit
        && plan_.edit.kind != level::MotsArchLightingEditKind::Multiply)
    {
        return false;
    }
    if (!validSelection(plan_)
        || plan_.selectedThingIndices.empty()
        || !validMode(mode)
        || !std::isfinite(monoMultiplier)
        || !finiteRgb(rgbMultiplier))
    {
        return false;
    }

    std::vector<SourcePreview> previews{};
    std::unordered_map<std::int32_t, std::size_t> stageMap{};
    if (!selectedStageSources(plan_, previews, stageMap))
    {
        return false;
    }

    bool anyChanges = false;
    for (SourcePreview& preview : previews)
    {
        bool changed = false;
        if (!applyMultiply(
                preview.object, mode, monoMultiplier, rgbMultiplier, changed))
        {
            return false;
        }
        anyChanges = anyChanges || changed;
    }

    plan_.edit = level::MotsArchLightingEdit{};
    plan_.edit.kind = level::MotsArchLightingEditKind::Multiply;
    plan_.edit.mode = mode;
    plan_.edit.monoMultiplier = monoMultiplier;
    plan_.edit.rgbMultiplier = rgbMultiplier;
    if (anyChanges || plan_.hasPendingEdit)
    {
        for (SourcePreview& preview : previews)
        {
            plan_.records[preview.stageIndex].stagedRecord =
                std::move(preview.object);
        }
    }
    plan_.hasPendingEdit = anyChanges;
    return true;
}

bool MotsArchLightingStaging::setVertex(
    const std::size_t meshIndex,
    const std::size_t vertexIndex,
    const MotsArchLightVertex& value)
{
    if (plan_.hasPendingEdit
        && plan_.edit.kind != level::MotsArchLightingEditKind::SetVertex)
    {
        return false;
    }
    if (plan_.hasPendingEdit
        && (plan_.edit.meshIndex != meshIndex
            || plan_.edit.vertexIndex != vertexIndex))
    {
        return false;
    }
    if (!validSelection(plan_)
        || plan_.selectedThingIndices.empty()
        || !finiteVertex(value))
    {
        return false;
    }

    std::vector<SourcePreview> previews{};
    std::unordered_map<std::int32_t, std::size_t> stageMap{};
    if (!selectedStageSources(plan_, previews, stageMap))
    {
        return false;
    }
    for (const std::int32_t thingIndex : plan_.selectedThingIndices)
    {
        const MotsArchLightingTarget* target =
            targetForThing(plan_, thingIndex);
        if (!target
            || !topologyContainsVertex(target->topology, meshIndex, vertexIndex))
        {
            return false;
        }
    }

    bool anyChanges = false;
    for (SourcePreview& preview : previews)
    {
        if (meshIndex >= preview.object.meshes.size()
            || vertexIndex >= preview.object.meshes[meshIndex].vertices.size())
        {
            return false;
        }
        anyChanges = anyChanges || !sameVertexBits(
            preview.object.meshes[meshIndex].vertices[vertexIndex], value);
        preview.object.meshes[meshIndex].vertices[vertexIndex] = value;
    }

    plan_.edit = level::MotsArchLightingEdit{};
    plan_.edit.kind = level::MotsArchLightingEditKind::SetVertex;
    plan_.edit.mode = level::MotsArchLightingVertexMode::Rgb;
    plan_.edit.meshIndex = meshIndex;
    plan_.edit.vertexIndex = vertexIndex;
    plan_.edit.vertexValue = value;
    if (anyChanges || plan_.hasPendingEdit)
    {
        for (SourcePreview& preview : previews)
        {
            plan_.records[preview.stageIndex].stagedRecord =
                std::move(preview.object);
        }
    }
    plan_.hasPendingEdit = anyChanges;
    return true;
}

bool MotsArchLightingStaging::reset()
{
    const bool hadChanges = plan_.hasPendingEdit;
    bool removedNewStage = false;
    for (const MotsArchLightingRecordStage& stage : plan_.records)
    {
        if (stage.originalRecordIndex < 0 && !stage.originalRecord)
        {
            removedNewStage = true;
            break;
        }
    }
    clearPendingInitialization(plan_);
    plan_.edit = level::MotsArchLightingEdit{};
    plan_.hasPendingEdit = false;
    return hadChanges || removedNewStage;
}

bool MotsArchLightingStaging::resetToDefault()
{
    if (plan_.hasPendingEdit
        && plan_.edit.kind != level::MotsArchLightingEditKind::Reset)
    {
        return false;
    }
    if (!validSelection(plan_)
        || plan_.selectedThingIndices.empty())
    {
        return false;
    }
    for (const std::int32_t thingIndex : plan_.selectedThingIndices)
    {
        const MotsArchLightingTarget* target =
            targetForThing(plan_, thingIndex);
        if (!target
            || target->originalRecordIndex < 0
            || !target->modelAvailable
            || !target->valueEditingAllowed
            || target->topology.geosets.empty())
        {
            return false;
        }
    }

    plan_.edit = level::MotsArchLightingEdit{};
    plan_.edit.kind = level::MotsArchLightingEditKind::Reset;
    plan_.edit.mode = level::MotsArchLightingVertexMode::Rgb;
    plan_.hasPendingEdit = true;
    return true;
}

MotsArchLightingPlanResult validateMotsArchLightingPlanResources(
    const MotsArchLightingPlan& plan,
    const std::vector<MotsArchLightingTarget>& currentTargets)
{
    if (!plan.hasPendingEdit)
    {
        return {
            MotsArchLightingPlanStatus::Noop,
            std::string(kNoopMessage) };
    }
    if (plan.project != ProjectType::MOTS
        || !validSelection(plan)
        || plan.selectedThingIndices.empty())
    {
        return {
            MotsArchLightingPlanStatus::Rejected,
            std::string(kInvalidPlanMessage) };
    }

    std::unordered_map<std::int32_t, const MotsArchLightingTarget*> current{};
    current.reserve(currentTargets.size());
    for (const MotsArchLightingTarget& target : currentTargets)
    {
        if (!current.emplace(target.thingIndex, &target).second)
        {
            return {
                MotsArchLightingPlanStatus::Rejected,
                std::string(kResourceMessage) };
        }
    }

    const bool resetting =
        plan.edit.kind == level::MotsArchLightingEditKind::Reset;
    if (plan.edit.kind != level::MotsArchLightingEditKind::Initialize
        && plan.edit.kind != level::MotsArchLightingEditKind::Multiply
        && plan.edit.kind != level::MotsArchLightingEditKind::SetVertex
        && !resetting)
    {
        return {
            MotsArchLightingPlanStatus::Rejected,
            std::string(kInvalidPlanMessage) };
    }

    for (const std::int32_t thingIndex : plan.selectedThingIndices)
    {
        const MotsArchLightingTarget* expected =
            targetForThing(plan, thingIndex);
        const auto found = current.find(thingIndex);
        if (!expected || found == current.end())
        {
            return {
                MotsArchLightingPlanStatus::Rejected,
                std::string(kResourceMessage) };
        }
        const MotsArchLightingTarget& actual = *found->second;
        if (!actual.modelAvailable
            || !actual.valueEditingAllowed
            || !sameTargetIdentity(*expected, actual))
        {
            return {
                MotsArchLightingPlanStatus::Rejected,
                std::string(kResourceMessage) };
        }
    }
    return {
        MotsArchLightingPlanStatus::Applied,
        std::string(kCurrentMessage) };
}

} // namespace sed::ui::lighting
