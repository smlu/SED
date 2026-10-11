#include "test_common.hpp"

#include "sed/editor/document.hpp"
#include "sed/editor/selection.hpp"
#include "sed/editor/undo.hpp"
#include "sed/assets/jkl.hpp"
#include "sed/level.hpp"
#include "sed/level/arch_lighting.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level/thing.hpp"
#include "sed/text.hpp"

#include <stdexcept>
#include <array>
#include <bit>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace sed::tests
{

namespace
{

sed::TplValue makeArchFlag()
{
    sed::TplValue flags{};
    flags.name = "thingflags";
    flags.type = sed::ValueType::integer;
    flags.intValue = 0x00000008;
    return flags;
}

sed::level::MotsArchLightingTopology archTopology()
{
    return sed::level::MotsArchLightingTopology{
        { { 3u, 2u }, { 3u, 2u } } };
}

sed::MotsArchLightObject archObject()
{
    sed::MotsArchLightObject object{};
    object.wireIndex = 5;
    sed::MotsArchLightMesh firstMesh{};
    firstMesh.wireIndex = 8;
    firstMesh.vertices = {
        sed::MotsArchLightVertex{ 0.25f, 0.125f, 0.25f, 0.5f },
        sed::MotsArchLightVertex{ 0.50f, 0.25f, 0.5f, 0.75f },
        sed::MotsArchLightVertex{ 0.75f, 0.5f, 0.75f, 1.0f },
    };
    sed::MotsArchLightMesh secondMesh{};
    secondMesh.wireIndex = 9;
    secondMesh.vertices.resize(2, sed::MotsArchLightVertex{ 1.0f, 1.0f, 1.0f, 1.0f });
    object.meshes.push_back(std::move(firstMesh));
    object.meshes.push_back(std::move(secondMesh));
    return object;
}

bool hasExpectedArchObjectTopology(const sed::MotsArchLightObject& object)
{
    return object.meshes.size() == 2
        && object.meshes[0].vertices.size() == 3
        && object.meshes[1].vertices.size() == 2;
}

bool sameArchFloatBits(const float left, const float right) noexcept
{
    return std::bit_cast<std::uint32_t>(left)
        == std::bit_cast<std::uint32_t>(right);
}

bool sameArchLightingPayload(
    const sed::Level& left,
    const sed::Level& right) noexcept
{
    if (left.things.size() != right.things.size()
        || left.motsArchLighting.size() != right.motsArchLighting.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.things.size(); ++index)
    {
        if (left.things[index].archLightingIndex
            != right.things[index].archLightingIndex)
        {
            return false;
        }
    }
    for (std::size_t objectIndex = 0;
         objectIndex < left.motsArchLighting.size();
         ++objectIndex)
    {
        const sed::MotsArchLightObject& lhs =
            left.motsArchLighting[objectIndex];
        const sed::MotsArchLightObject& rhs =
            right.motsArchLighting[objectIndex];
        if (lhs.wireIndex != rhs.wireIndex
            || lhs.meshes.size() != rhs.meshes.size())
        {
            return false;
        }
        for (std::size_t meshIndex = 0; meshIndex < lhs.meshes.size(); ++meshIndex)
        {
            const sed::MotsArchLightMesh& lhsMesh = lhs.meshes[meshIndex];
            const sed::MotsArchLightMesh& rhsMesh = rhs.meshes[meshIndex];
            if (lhsMesh.wireIndex != rhsMesh.wireIndex
                || lhsMesh.vertices.size() != rhsMesh.vertices.size())
            {
                return false;
            }
            for (std::size_t vertexIndex = 0;
                 vertexIndex < lhsMesh.vertices.size();
                 ++vertexIndex)
            {
                const sed::MotsArchLightVertex& lhsVertex =
                    lhsMesh.vertices[vertexIndex];
                const sed::MotsArchLightVertex& rhsVertex =
                    rhsMesh.vertices[vertexIndex];
                if (!sameArchFloatBits(lhsVertex.mono, rhsVertex.mono)
                    || !sameArchFloatBits(lhsVertex.red, rhsVertex.red)
                    || !sameArchFloatBits(lhsVertex.green, rhsVertex.green)
                    || !sameArchFloatBits(lhsVertex.blue, rhsVertex.blue))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

void runMotsArchLightingExportRoundTripRegressionTests()
{
    sed::Level initial{};
    initial.kind = sed::ProjectType::MOTS;
    // JKL/NDY export is a VERSION 1 MotS dialect.  Keep the synthetic
    // fixture's header explicit so the engine round-trip exercises the
    // ARCHLIGHTING section instead of being rejected before serialization.
    initial.header.version = 1;
    initial.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;
    for (int index = 0; index < 2; ++index)
    {
        sed::Thing& thing = initial.newThing();
        thing.name = "arch_export_" + std::to_string(index);
        thing.values.push_back(makeArchFlag());
        thing.archLightingIndex = 0;
    }
    initial.motsArchLighting.push_back(archObject());

    const std::vector<std::int32_t> selected{ 0, 1 };
    const std::vector<sed::level::MotsArchLightingTopology> topologies{
        archTopology(), archTopology() };
    sed::level::MotsArchLightingEdit sparseEdit{
        sed::level::MotsArchLightingEditKind::SetVertex,
        sed::level::MotsArchLightingVertexMode::Rgb,
        1.0f,
        sed::ColorWhite };
    sparseEdit.vertexEdits = {
        sed::level::MotsArchLightingVertexEdit{
            0u,
            0u,
            sed::MotsArchLightVertex{ -0.0f, -2.5f, 3.25f, 12.5f },
            { true, true, true, true },
            0 },
        sed::level::MotsArchLightingVertexEdit{
            0u,
            1u,
            sed::MotsArchLightVertex{ -8.0f, 4.5f, -6.25f, -0.0f },
            { true, true, true, true },
            1 } };

    sed::editor::Document document{};
    document.level() = initial;
    sed::level::MotsArchLightingEditResult editResult{};
    const bool applied = document.applyUndoable(
        "ARCHLIGHTING export batch edit",
        sed::editor::DocumentDirty::Things,
        [&](sed::editor::DocumentState& state)
        {
            editResult = sed::level::applyMotsArchLightingEdit(
                state.level, selected, topologies, sparseEdit);
            return editResult.status
                == sed::level::MotsArchLightingEditStatus::Applied;
        });
    const sed::Level edited = document.level();
    const std::vector<std::byte> beforeBytes =
        sed::assets::saveToSedBytes(initial);
    const std::vector<std::byte> editedBytes =
        sed::assets::saveToSedBytes(edited);
    check(applied
            && editResult.changedThingIndices == selected
            && edited.things[0].archLightingIndex != 0
            && edited.things[1].archLightingIndex != 0
            && edited.things[0].archLightingIndex
                != edited.things[1].archLightingIndex
            && edited.motsArchLighting.size() == 3
            && edited.motsArchLighting[static_cast<std::size_t>(
                    edited.things[0].archLightingIndex)]
                    .meshes[0].vertices[0]
                == sed::MotsArchLightVertex{ -0.0f, -2.5f, 3.25f, 12.5f }
            && edited.motsArchLighting[static_cast<std::size_t>(
                    edited.things[1].archLightingIndex)]
                    .meshes[0].vertices[1]
                == sed::MotsArchLightVertex{ -8.0f, 4.5f, -6.25f, -0.0f },
        "MOTS ARCHLIGHTING export fixture creates distinct per-owner records with all four edited channels");

    sed::Level nativeReload = sed::assets::loadFromSedBytes(editedBytes);
    check(sameArchLightingPayload(nativeReload, edited),
        "MOTS ARCHLIGHTING native SED round-trip preserves split bindings and exact channels");

    std::ostringstream engineOutput;
    sed::assets::JklSaveOptions saveOptions{};
    saveOptions.clampExtraLight = false;
    sed::assets::saveToJKL(edited, engineOutput, saveOptions);
    const std::string engineText = engineOutput.str();
    sed::assets::JklLoadOptions loadOptions{};
    loadOptions.projectType = sed::ProjectType::MOTS;
    std::istringstream engineInput(engineText);
    const sed::Level engineReload = sed::assets::loadFromJKL(
        engineInput, loadOptions);
    check(!engineText.empty()
            && engineReload.things.size() == 2
            && engineReload.motsArchLighting.size() == 2
            && engineReload.things[0].archLightingIndex == 0
            && engineReload.things[1].archLightingIndex == 1
            && engineReload.things[0].archLightingIndex
                != engineReload.things[1].archLightingIndex
            && engineReload.motsArchLighting[0]
                == edited.motsArchLighting[static_cast<std::size_t>(
                    edited.things[0].archLightingIndex)]
            && engineReload.motsArchLighting[1]
                == edited.motsArchLighting[static_cast<std::size_t>(
                    edited.things[1].archLightingIndex)],
        "MOTS ARCHLIGHTING engine export/reload compacts orphaned source records while preserving split owner channels");

    sed::editor::DocumentDirty dirty = sed::editor::DocumentDirty::None;
    check(document.undo(&dirty)
            && sed::assets::saveToSedBytes(document.level()) == beforeBytes
            && document.level().things[0].archLightingIndex == 0
            && document.level().things[1].archLightingIndex == 0,
        "MOTS ARCHLIGHTING export batch Undo restores the shared source record");
    check(document.redo(&dirty)
            && sed::assets::saveToSedBytes(document.level()) == editedBytes
            && sameArchLightingPayload(document.level(), edited),
        "MOTS ARCHLIGHTING export batch Redo restores split bindings and all channels");
}

void runMotsArchLightingAuthoringRegressionTests()
{
    sed::Level level{};
    level.kind = sed::ProjectType::MOTS;
    for (int index = 0; index < 4; ++index)
    {
        sed::Thing& thing = level.newThing();
        thing.name = "arch_" + std::to_string(index);
        thing.values.push_back(makeArchFlag());
    }
    level.motsArchLighting.push_back(archObject());
    level.things[0].archLightingIndex = 0;
    level.things[1].archLightingIndex = 0;
    level.things[2].archLightingIndex = 0;
    sed::ThingTemplate inheritedBase{};
    inheritedBase.name = "+arch_base";
    inheritedBase.values.push_back(makeArchFlag());
    sed::ThingTemplate inheritedChild{};
    inheritedChild.name = "+arch_child";
    inheritedChild.parent = "+arch_base";
    level.templates.push_back(inheritedBase);
    level.templates.push_back(inheritedChild);
    level.things[3].values.clear();
    level.things[3].templateName = "+arch_child";

    const sed::level::MotsArchLightingTopology topology = archTopology();
    const std::vector<std::int32_t> selected{ 0, 2 };
    const std::vector<sed::level::MotsArchLightingTopology> topologies{
        topology, topology };
    const std::vector<std::byte> beforeDuplicate =
        sed::assets::saveToSedBytes(level);
    const std::vector<std::int32_t> duplicateSelection{ 0, 0 };
    const sed::level::MotsArchLightingEditResult duplicateResult =
        sed::level::applyMotsArchLightingEdit(
            level, duplicateSelection, topologies,
            sed::level::MotsArchLightingEdit{
                sed::level::MotsArchLightingEditKind::Multiply,
                sed::level::MotsArchLightingVertexMode::Rgb,
                1.0f,
                sed::ColorF{ 2.0f, 0.5f, 1.5f, 1.0f } });
    check(duplicateResult.status == sed::level::MotsArchLightingEditStatus::Rejected
            && duplicateResult.message
                == "MotS ARCHLIGHTING Thing selection contains a duplicate index."
            && duplicateResult.changedThingIndices.empty()
            && sed::assets::saveToSedBytes(level) == beforeDuplicate,
        "MOTS ARCHLIGHTING authoring rejects duplicate selection indices atomically");

    const sed::level::MotsArchLightingEdit rgbEdit{
        sed::level::MotsArchLightingEditKind::Multiply,
        sed::level::MotsArchLightingVertexMode::Rgb,
        1.0f,
        sed::ColorF{ 2.0f, 0.5f, 1.5f, 1.0f } };
    const sed::level::MotsArchLightingEditResult rgbResult =
        sed::level::applyMotsArchLightingEdit(level, selected, topologies, rgbEdit);
    check(rgbResult.status == sed::level::MotsArchLightingEditStatus::Applied
            && rgbResult.message
                == "Updated RGB channels for selected MotS ARCHLIGHTING owners."
            && rgbResult.changedThingIndices == selected
            && level.motsArchLighting.size() == 2
            && level.things[0].archLightingIndex == 1
            && level.things[1].archLightingIndex == 0
            && level.things[2].archLightingIndex == 1
            && hasExpectedArchObjectTopology(level.motsArchLighting[0])
            && hasExpectedArchObjectTopology(level.motsArchLighting[1])
            && level.motsArchLighting[0].meshes[0].vertices[0]
                == sed::MotsArchLightVertex{ 0.25f, 0.125f, 0.25f, 0.5f }
            && level.motsArchLighting[1].meshes[0].vertices[0]
                == sed::MotsArchLightVertex{ 0.25f, 0.25f, 0.125f, 0.75f }
            && level.motsArchLighting[1].meshes[0].vertices[1]
                == sed::MotsArchLightVertex{ 0.5f, 0.5f, 0.25f, 1.125f }
            && level.motsArchLighting[1].meshes[0].vertices[2]
                == sed::MotsArchLightVertex{ 0.75f, 1.0f, 0.375f, 1.5f }
            && level.motsArchLighting[1].meshes[1].vertices[0]
                == sed::MotsArchLightVertex{ 1.0f, 2.0f, 0.5f, 1.5f }
            && level.motsArchLighting[1].meshes[1].vertices[1]
                == sed::MotsArchLightVertex{ 1.0f, 2.0f, 0.5f, 1.5f },
        "MOTS ARCHLIGHTING RGB edit uses multi-selection copy-on-write, preserves all vertices, and leaves the unselected shared record unchanged");

    const sed::level::MotsArchLightingEdit monoEdit{
        sed::level::MotsArchLightingEditKind::Multiply,
        sed::level::MotsArchLightingVertexMode::Mono,
        2.0f,
        sed::ColorWhite };
    const sed::level::MotsArchLightingEditResult monoResult =
        sed::level::applyMotsArchLightingEdit(level, selected, topologies, monoEdit);
    check(monoResult.status == sed::level::MotsArchLightingEditStatus::Applied
            && monoResult.message
                == "Updated mono channel for selected MotS ARCHLIGHTING owners."
            && monoResult.changedThingIndices == selected
            && level.motsArchLighting.size() == 2
            && hasExpectedArchObjectTopology(level.motsArchLighting[1])
            && level.motsArchLighting[1].meshes[0].vertices[0]
                == sed::MotsArchLightVertex{ 0.5f, 0.25f, 0.125f, 0.75f }
            && level.motsArchLighting[1].meshes[0].vertices[1]
                == sed::MotsArchLightVertex{ 1.0f, 0.5f, 0.25f, 1.125f }
            && level.motsArchLighting[1].meshes[0].vertices[2]
                == sed::MotsArchLightVertex{ 1.5f, 1.0f, 0.375f, 1.5f }
            && level.motsArchLighting[1].meshes[1].vertices[0]
                == sed::MotsArchLightVertex{ 2.0f, 2.0f, 0.5f, 1.5f }
            && level.motsArchLighting[1].meshes[1].vertices[1]
                == sed::MotsArchLightVertex{ 2.0f, 2.0f, 0.5f, 1.5f },
        "MOTS ARCHLIGHTING mono edit scales brightness on every mesh vertex without destroying RGB gradients");

    const sed::level::MotsArchLightingEdit setVertex{
        sed::level::MotsArchLightingEditKind::SetVertex,
        sed::level::MotsArchLightingVertexMode::Rgb,
        1.0f,
        sed::ColorWhite,
        0u,
        1u,
        sed::MotsArchLightVertex{ 0.0f, 0.125f, 0.875f, 0.0f } };
    const sed::level::MotsArchLightingEditResult setVertexResult =
        sed::level::applyMotsArchLightingEdit(level, selected, topologies, setVertex);
    check(setVertexResult.status == sed::level::MotsArchLightingEditStatus::Applied
            && setVertexResult.message
                == "Set MotS ARCHLIGHTING vertex channels for selected owners."
            && setVertexResult.changedThingIndices == selected
            && level.motsArchLighting.size() == 2
            && level.motsArchLighting[0].meshes[0].vertices[1]
                == sed::MotsArchLightVertex{ 0.5f, 0.25f, 0.5f, 0.75f }
            && level.motsArchLighting[1].meshes[0].vertices[1]
                == sed::MotsArchLightVertex{ 0.0f, 0.125f, 0.875f, 0.0f },
        "MOTS ARCHLIGHTING SetVertex edits one ordered mesh vertex without mutating an unselected shared owner");

    sed::Level virtualDefaultLevel{};
    virtualDefaultLevel.kind = sed::ProjectType::MOTS;
    sed::Thing& virtualDefaultThing = virtualDefaultLevel.newThing();
    virtualDefaultThing.name = "virtual_arch";
    virtualDefaultThing.values.push_back(makeArchFlag());
    const std::vector<std::int32_t> virtualSelection{ 0 };
    const std::vector<sed::level::MotsArchLightingTopology> virtualTopologies{
        topology };
    sed::level::MotsArchLightingEdit virtualSparseSet{
        sed::level::MotsArchLightingEditKind::SetVertex,
        sed::level::MotsArchLightingVertexMode::Rgb,
        1.0f,
        sed::ColorWhite };
    virtualSparseSet.vertexEdits = {
        sed::level::MotsArchLightingVertexEdit{
            0u,
            0u,
            sed::MotsArchLightVertex{ 0.0f, 0.625f, 0.0f, -0.0f },
            { false, true, false, true },
            0 },
        sed::level::MotsArchLightingVertexEdit{
            1u,
            1u,
            sed::MotsArchLightVertex{ 0.0f, 0.0f, 0.375f, 0.0f },
            { false, false, true, false },
            0 } };
    const std::vector<std::byte> virtualBeforeRejected =
        sed::assets::saveToSedBytes(virtualDefaultLevel);
    sed::level::MotsArchLightingEdit invalidVirtualSet = virtualSparseSet;
    invalidVirtualSet.vertexEdits[1].vertexIndex = 2u;
    const sed::level::MotsArchLightingEditResult invalidVirtualResult =
        sed::level::applyMotsArchLightingEdit(
            virtualDefaultLevel, virtualSelection, virtualTopologies,
            invalidVirtualSet);
    check(invalidVirtualResult.status
                == sed::level::MotsArchLightingEditStatus::Rejected
            && invalidVirtualResult.message
                == "ARCHLIGHTING model topology is incompatible with every render geoset."
            && invalidVirtualResult.changedThingIndices.empty()
            && virtualDefaultLevel.things[0].archLightingIndex == -1
            && virtualDefaultLevel.motsArchLighting.empty()
            && sed::assets::saveToSedBytes(virtualDefaultLevel)
                == virtualBeforeRejected,
        "MOTS ARCHLIGHTING rejects an invalid sparse edit on a virtual default without partial materialization");

    const sed::level::MotsArchLightingEditResult virtualSetResult =
        sed::level::applyMotsArchLightingEdit(
            virtualDefaultLevel, virtualSelection, virtualTopologies,
            virtualSparseSet);
    const sed::MotsArchLightObject* materializedVirtual =
        virtualDefaultLevel.motsArchLighting.empty()
            ? nullptr : &virtualDefaultLevel.motsArchLighting[0];
    check(virtualSetResult.status
                == sed::level::MotsArchLightingEditStatus::Applied
            && virtualSetResult.message
                == "Set MotS ARCHLIGHTING vertex channels for selected owners."
            && virtualSetResult.changedThingIndices == virtualSelection
            && virtualDefaultLevel.things[0].archLightingIndex == 0
            && virtualDefaultLevel.motsArchLighting.size() == 1
            && materializedVirtual
            && hasExpectedArchObjectTopology(*materializedVirtual)
            && materializedVirtual->meshes[0].vertices[0]
                == sed::MotsArchLightVertex{ 0.0f, 0.625f, 0.0f, -0.0f }
            && std::bit_cast<std::uint32_t>(
                   materializedVirtual->meshes[0].vertices[0].blue)
                == 0x80000000u
            && materializedVirtual->meshes[0].vertices[1]
                == sed::MotsArchLightVertex{}
            && materializedVirtual->meshes[1].vertices[0]
                == sed::MotsArchLightVertex{}
            && materializedVirtual->meshes[1].vertices[1]
                == sed::MotsArchLightVertex{ 0.0f, 0.0f, 0.375f, 0.0f },
        "MOTS ARCHLIGHTING sparse SetVertex materializes a zero baseline, edits multiple vertices, and preserves untouched channels");

    sed::Level virtualMultiplyLevel{};
    virtualMultiplyLevel.kind = sed::ProjectType::MOTS;
    sed::Thing& virtualMultiplyThing = virtualMultiplyLevel.newThing();
    virtualMultiplyThing.values.push_back(makeArchFlag());
    const sed::level::MotsArchLightingEdit negativeZeroMultiply{
        sed::level::MotsArchLightingEditKind::Multiply,
        sed::level::MotsArchLightingVertexMode::Rgb,
        1.0f,
        sed::ColorF{ -1.0f, 2.0f, 1.0f, 1.0f } };
    const sed::level::MotsArchLightingEditResult virtualMultiplyResult =
        sed::level::applyMotsArchLightingEdit(
            virtualMultiplyLevel, virtualSelection, virtualTopologies,
            negativeZeroMultiply);
    check(virtualMultiplyResult.status
                == sed::level::MotsArchLightingEditStatus::Applied
            && virtualMultiplyResult.changedThingIndices == virtualSelection
            && virtualMultiplyLevel.things[0].archLightingIndex == 0
            && virtualMultiplyLevel.motsArchLighting.size() == 1
            && virtualMultiplyLevel.motsArchLighting[0].meshes[0].vertices[0].mono
                == 0.0f
            && std::bit_cast<std::uint32_t>(
                   virtualMultiplyLevel.motsArchLighting[0].meshes[0].vertices[0].red)
                == 0x80000000u
            && virtualMultiplyLevel.motsArchLighting[0].meshes[0].vertices[0].green
                == 0.0f
            && virtualMultiplyLevel.motsArchLighting[0].meshes[0].vertices[0].blue
                == 0.0f,
        "MOTS ARCHLIGHTING Multiply materializes a virtual zero record only when the operation changes a channel bit pattern");

    sed::Level virtualMultiplyNoopLevel{};
    virtualMultiplyNoopLevel.kind = sed::ProjectType::MOTS;
    sed::Thing& virtualMultiplyNoopThing = virtualMultiplyNoopLevel.newThing();
    virtualMultiplyNoopThing.values.push_back(makeArchFlag());
    const sed::level::MotsArchLightingEdit positiveMultiply{
        sed::level::MotsArchLightingEditKind::Multiply,
        sed::level::MotsArchLightingVertexMode::Rgb,
        1.0f,
        sed::ColorF{ 2.0f, 0.5f, 1.5f, 1.0f } };
    const std::vector<std::byte> virtualNoopBefore =
        sed::assets::saveToSedBytes(virtualMultiplyNoopLevel);
    const sed::level::MotsArchLightingEditResult virtualMultiplyNoopResult =
        sed::level::applyMotsArchLightingEdit(
            virtualMultiplyNoopLevel, virtualSelection, virtualTopologies,
            positiveMultiply);
    check(virtualMultiplyNoopResult.status
                == sed::level::MotsArchLightingEditStatus::Noop
            && virtualMultiplyNoopResult.changedThingIndices.empty()
            && virtualMultiplyNoopLevel.things[0].archLightingIndex == -1
            && virtualMultiplyNoopLevel.motsArchLighting.empty()
            && sed::assets::saveToSedBytes(virtualMultiplyNoopLevel)
                == virtualNoopBefore,
        "MOTS ARCHLIGHTING Multiply on zero is a no-op and does not allocate a redundant record");

    sed::Level overBudgetVirtualLevel{};
    overBudgetVirtualLevel.kind = sed::ProjectType::MOTS;
    for (int index = 0; index < 2; ++index)
    {
        sed::Thing& thing = overBudgetVirtualLevel.newThing();
        thing.values.push_back(makeArchFlag());
    }
    const std::vector<std::int32_t> overBudgetVirtualSelection{ 0, 1 };
    const sed::level::MotsArchLightingTopology largeVirtualTopology{
        { { 1'000'000u } } };
    const std::vector<sed::level::MotsArchLightingTopology>
        overBudgetVirtualTopologies{
            largeVirtualTopology, largeVirtualTopology };
    const std::vector<std::byte> overBudgetVirtualBefore =
        sed::assets::saveToSedBytes(overBudgetVirtualLevel);
    const sed::level::MotsArchLightingEditResult overBudgetVirtualNoop =
        sed::level::applyMotsArchLightingEdit(
            overBudgetVirtualLevel,
            overBudgetVirtualSelection,
            overBudgetVirtualTopologies,
            positiveMultiply);
    check(overBudgetVirtualNoop.status
                == sed::level::MotsArchLightingEditStatus::Noop
            && overBudgetVirtualNoop.message
                == "No MotS ARCHLIGHTING changes were necessary."
            && overBudgetVirtualNoop.changedThingIndices.empty()
            && overBudgetVirtualLevel.motsArchLighting.empty()
            && overBudgetVirtualLevel.things[0].archLightingIndex == -1
            && overBudgetVirtualLevel.things[1].archLightingIndex == -1
            && sed::assets::saveToSedBytes(overBudgetVirtualLevel)
                == overBudgetVirtualBefore,
        "MOTS ARCHLIGHTING positive Multiply is a no-op before aggregate budget/materialization for multiple 1M-vertex virtual defaults");

    const std::vector<sed::level::MotsArchLightingTopology> invalidVirtualTopologies{
        sed::level::MotsArchLightingTopology{} };
    const sed::level::MotsArchLightingEditResult invalidVirtualNoop =
        sed::level::applyMotsArchLightingEdit(
            overBudgetVirtualLevel,
            virtualSelection,
            invalidVirtualTopologies,
            positiveMultiply);
    check(invalidVirtualNoop.status
                == sed::level::MotsArchLightingEditStatus::Rejected
            && invalidVirtualNoop.message
                == "ARCHLIGHTING model topology is incompatible with every render geoset."
            && invalidVirtualNoop.changedThingIndices.empty()
            && overBudgetVirtualLevel.motsArchLighting.empty()
            && overBudgetVirtualLevel.things[0].archLightingIndex == -1
            && sed::assets::saveToSedBytes(overBudgetVirtualLevel)
                == overBudgetVirtualBefore,
        "MOTS ARCHLIGHTING virtual-zero no-op still rejects invalid model topology atomically");

    const std::vector<std::byte> beforeDuplicateSparse =
        sed::assets::saveToSedBytes(level);
    sed::level::MotsArchLightingEdit duplicateSparse = setVertex;
    duplicateSparse.vertexEdits = {
        sed::level::MotsArchLightingVertexEdit{
            0u,
            1u,
            sed::MotsArchLightVertex{ 0.25f, 0.5f, 0.75f, 1.0f },
            { false, true, false, false },
            0 },
        sed::level::MotsArchLightingVertexEdit{
            0u,
            1u,
            sed::MotsArchLightVertex{ 0.25f, 0.25f, 0.75f, 1.0f },
            { false, false, true, false },
            0 } };
    const sed::level::MotsArchLightingEditResult duplicateSparseResult =
        sed::level::applyMotsArchLightingEdit(
            level, selected, topologies, duplicateSparse);
    check(duplicateSparseResult.status == sed::level::MotsArchLightingEditStatus::Rejected
            && duplicateSparseResult.message
                == "MotS ARCHLIGHTING edit contains a duplicate vertex."
            && duplicateSparseResult.changedThingIndices.empty()
            && sed::assets::saveToSedBytes(level) == beforeDuplicateSparse,
        "MOTS ARCHLIGHTING sparse SetVertex rejects duplicate owner/mesh/vertex edits atomically");

    sed::level::MotsArchLightingEdit ambiguousSparse = setVertex;
    const sed::level::MotsArchLightingVertexEdit scopedEdit{
        0u,
        1u,
        sed::MotsArchLightVertex{ 0.25f, 0.5f, 0.75f, 1.0f },
        { false, true, false, false },
        0 };
    const sed::level::MotsArchLightingVertexEdit globalEdit{
        0u,
        1u,
        sed::MotsArchLightVertex{ 0.25f, 0.25f, 0.75f, 1.0f },
        { false, false, true, false },
        -1 };
    for (const bool globalFirst : { false, true })
    {
        ambiguousSparse.vertexEdits = globalFirst
            ? std::vector<sed::level::MotsArchLightingVertexEdit>{
                  globalEdit, scopedEdit }
            : std::vector<sed::level::MotsArchLightingVertexEdit>{
                  scopedEdit, globalEdit };
        const std::vector<std::byte> beforeAmbiguousSparse =
            sed::assets::saveToSedBytes(level);
        const sed::level::MotsArchLightingEditResult ambiguousResult =
            sed::level::applyMotsArchLightingEdit(
                level, selected, topologies, ambiguousSparse);
        check(ambiguousResult.status == sed::level::MotsArchLightingEditStatus::Rejected
                && ambiguousResult.message
                    == "MotS ARCHLIGHTING edit contains a duplicate vertex."
                && ambiguousResult.changedThingIndices.empty()
                && sed::assets::saveToSedBytes(level) == beforeAmbiguousSparse,
            globalFirst
                ? "MOTS ARCHLIGHTING rejects global/scoped overlap independent of edit order (global first)"
                : "MOTS ARCHLIGHTING rejects global/scoped overlap independent of edit order (scoped first)");
    }

    sed::Level variantLevel{};
    variantLevel.kind = sed::ProjectType::MOTS;
    for (int index = 0; index < 2; ++index)
    {
        sed::Thing& thing = variantLevel.newThing();
        thing.name = "arch_variant_" + std::to_string(index);
        thing.values.push_back(makeArchFlag());
        thing.archLightingIndex = 0;
    }
    variantLevel.motsArchLighting.push_back(archObject());
    sed::level::MotsArchLightingEdit perOwnerSparse = setVertex;
    perOwnerSparse.vertexEdits = {
        sed::level::MotsArchLightingVertexEdit{
            0u,
            0u,
            sed::MotsArchLightVertex{ 0.25f, 0.91f, 0.25f, 0.5f },
            { false, true, false, false },
            0 },
        sed::level::MotsArchLightingVertexEdit{
            0u,
            1u,
            sed::MotsArchLightVertex{ 0.50f, 0.50f, 0.82f, 0.75f },
            { false, false, true, false },
            1 } };
    const std::vector<std::int32_t> variantSelection{ 0, 1 };
    const std::vector<sed::level::MotsArchLightingTopology> variantTopologies{
        topology, topology };
    const sed::level::MotsArchLightingEditResult variantResult =
        sed::level::applyMotsArchLightingEdit(
            variantLevel,
            variantSelection,
            variantTopologies,
            perOwnerSparse);
    check(variantResult.status == sed::level::MotsArchLightingEditStatus::Applied
            && variantResult.changedThingIndices == variantSelection
            && variantLevel.motsArchLighting.size() == 3
            && variantLevel.things[0].archLightingIndex != 0
            && variantLevel.things[1].archLightingIndex != 0
            && variantLevel.things[0].archLightingIndex
                != variantLevel.things[1].archLightingIndex
            && variantLevel.motsArchLighting[static_cast<std::size_t>(
                    variantLevel.things[0].archLightingIndex)]
                    .meshes[0].vertices[0].red == 0.91f
            && variantLevel.motsArchLighting[static_cast<std::size_t>(
                    variantLevel.things[0].archLightingIndex)]
                    .meshes[0].vertices[1].green == 0.5f
            && variantLevel.motsArchLighting[static_cast<std::size_t>(
                    variantLevel.things[1].archLightingIndex)]
                    .meshes[0].vertices[0].red == 0.125f
            && variantLevel.motsArchLighting[static_cast<std::size_t>(
                    variantLevel.things[1].archLightingIndex)]
                    .meshes[0].vertices[1].green == 0.82f,
        "MOTS ARCHLIGHTING per-owner sparse edits split shared records even when every owner is selected");

    sed::Level smallerLod = level;
    smallerLod.things.push_back(smallerLod.things[3]);
    const std::vector<std::int32_t> smallerLodThing{ 4 };
    const std::vector<sed::level::MotsArchLightingTopology> smallerLodTopologies{
        sed::level::MotsArchLightingTopology{ { { 2u, 1u } } } };
    const sed::level::MotsArchLightingEditResult smallerLodResult =
        sed::level::applyMotsArchLightingEdit(
            smallerLod, smallerLodThing, smallerLodTopologies,
            sed::level::MotsArchLightingEdit{
                sed::level::MotsArchLightingEditKind::Initialize,
                sed::level::MotsArchLightingVertexMode::Rgb,
                1.0f,
                sed::ColorWhite });
    check(smallerLodResult.status == sed::level::MotsArchLightingEditStatus::Applied
            && smallerLodResult.changedThingIndices == smallerLodThing
            && smallerLod.motsArchLighting.size() == 3
            && smallerLod.motsArchLighting[2].meshes.size() == 2
            && smallerLod.motsArchLighting[2].meshes[0].vertices.size() == 2
            && smallerLod.motsArchLighting[2].meshes[1].vertices.size() == 1,
        "MOTS ARCHLIGHTING initialize accepts a smaller later geoset and allocates the maximum required prefix");

    const std::vector<std::int32_t> newThing{ 3 };
    const std::vector<sed::level::MotsArchLightingTopology> newThingTopologies{ topology };
    const sed::level::MotsArchLightingEdit initialize{
        sed::level::MotsArchLightingEditKind::Initialize,
        sed::level::MotsArchLightingVertexMode::Rgb,
        1.0f,
        sed::ColorWhite };
    auto unboundedTopology = topology;
    unboundedTopology.geosets[0][0] = std::numeric_limits<std::size_t>::max();
    const std::vector<std::byte> beforeUnboundedInitialize =
        sed::assets::saveToSedBytes(level);
    const sed::level::MotsArchLightingEditResult unboundedInitialize =
        sed::level::applyMotsArchLightingEdit(
            level, newThing,
            std::vector<sed::level::MotsArchLightingTopology>{ unboundedTopology },
            initialize);
    check(unboundedInitialize.status == sed::level::MotsArchLightingEditStatus::Rejected
            && unboundedInitialize.message
                == "ARCHLIGHTING model topology is incompatible with every render geoset."
            && unboundedInitialize.changedThingIndices.empty()
            && level.things[3].archLightingIndex == -1
            && sed::assets::saveToSedBytes(level) == beforeUnboundedInitialize,
        "MOTS ARCHLIGHTING rejects an unbounded topology before allocating an initialized record");

    sed::Level saturatedRecords{};
    saturatedRecords.kind = sed::ProjectType::MOTS;
    sed::Thing& saturatedThing = saturatedRecords.newThing();
    saturatedThing.values.push_back(makeArchFlag());
    sed::MotsArchLightObject saturatedObject{};
    saturatedObject.wireIndex = 0;
    sed::MotsArchLightMesh saturatedMesh{};
    saturatedMesh.wireIndex = 0;
    saturatedMesh.vertices.resize(1'000'000u);
    saturatedObject.meshes.push_back(std::move(saturatedMesh));
    saturatedRecords.motsArchLighting.push_back(std::move(saturatedObject));
    const sed::level::MotsArchLightingEditResult saturatedInitialize =
        sed::level::applyMotsArchLightingEdit(
            saturatedRecords,
            std::vector<std::int32_t>{ 0 },
            std::vector<sed::level::MotsArchLightingTopology>{
                sed::level::MotsArchLightingTopology{ { { 1u } } } },
            initialize);
    check(saturatedInitialize.status == sed::level::MotsArchLightingEditStatus::Rejected
            && saturatedInitialize.message
                == "MotS ARCHLIGHTING edit exceeds the bounded record budget."
            && saturatedInitialize.changedThingIndices.empty()
            && saturatedRecords.things[0].archLightingIndex == -1
            && saturatedRecords.motsArchLighting.size() == 1
            && saturatedRecords.motsArchLighting[0].meshes.size() == 1
            && saturatedRecords.motsArchLighting[0].meshes[0].vertices.size()
                == 1'000'000u,
        "MOTS ARCHLIGHTING preflights aggregate vertex budget before allocating a candidate record");

    sed::Level highWireIndexLevel{};
    highWireIndexLevel.kind = sed::ProjectType::MOTS;
    sed::Thing& highWireThing = highWireIndexLevel.newThing();
    highWireThing.values.push_back(makeArchFlag());
    sed::MotsArchLightObject highWireObject{};
    highWireObject.wireIndex = std::numeric_limits<std::int32_t>::max();
    sed::MotsArchLightMesh highWireMesh{};
    highWireMesh.wireIndex = 0;
    highWireMesh.vertices.resize(1);
    highWireObject.meshes.push_back(std::move(highWireMesh));
    highWireIndexLevel.motsArchLighting.push_back(std::move(highWireObject));
    const sed::level::MotsArchLightingEditResult highWireInitialize =
        sed::level::applyMotsArchLightingEdit(
            highWireIndexLevel,
            std::vector<std::int32_t>{ 0 },
            std::vector<sed::level::MotsArchLightingTopology>{
                sed::level::MotsArchLightingTopology{ { { 1u } } } },
            initialize);
    check(highWireInitialize.status == sed::level::MotsArchLightingEditStatus::Applied
            && highWireInitialize.changedThingIndices == std::vector<std::int32_t>{ 0 }
            && highWireIndexLevel.things[0].archLightingIndex == 1
            && highWireIndexLevel.motsArchLighting.size() == 2
            && highWireIndexLevel.motsArchLighting[1].wireIndex == 0,
        "MOTS ARCHLIGHTING append chooses a free wire label when an existing record uses INT32_MAX");

    sed::Level signedZeroLevel{};
    signedZeroLevel.kind = sed::ProjectType::MOTS;
    sed::Thing& signedZeroThing = signedZeroLevel.newThing();
    signedZeroThing.values.push_back(makeArchFlag());
    signedZeroThing.archLightingIndex = 0;
    sed::MotsArchLightObject signedZeroObject{};
    signedZeroObject.wireIndex = 0;
    sed::MotsArchLightMesh signedZeroMesh{};
    signedZeroMesh.wireIndex = 0;
    signedZeroMesh.vertices.push_back(sed::MotsArchLightVertex{
        0.0f, 0.0f, 0.0f, 0.0f });
    signedZeroObject.meshes.push_back(std::move(signedZeroMesh));
    signedZeroLevel.motsArchLighting.push_back(std::move(signedZeroObject));
    const sed::level::MotsArchLightingEditResult signedZeroResult =
        sed::level::applyMotsArchLightingEdit(
            signedZeroLevel,
            std::vector<std::int32_t>{ 0 },
            std::vector<sed::level::MotsArchLightingTopology>{
                sed::level::MotsArchLightingTopology{ { { 1u } } } },
            sed::level::MotsArchLightingEdit{
                sed::level::MotsArchLightingEditKind::SetVertex,
                sed::level::MotsArchLightingVertexMode::Rgb,
                1.0f,
                sed::ColorWhite,
                0u,
                0u,
                sed::MotsArchLightVertex{ -0.0f, 0.0f, 0.0f, 0.0f } });
    check(signedZeroResult.status == sed::level::MotsArchLightingEditStatus::Applied
            && signedZeroResult.message
                == "Set MotS ARCHLIGHTING vertex channels for selected owners."
            && signedZeroResult.changedThingIndices == std::vector<std::int32_t>{ 0 }
            && std::bit_cast<std::uint32_t>(
                   signedZeroLevel.motsArchLighting[0].meshes[0].vertices[0].mono)
                == 0x80000000u,
        "MOTS ARCHLIGHTING SetVertex preserves an authored signed-zero channel change");

    const sed::level::MotsArchLightingEditResult initialized =
        sed::level::applyMotsArchLightingEdit(level, newThing, newThingTopologies, initialize);
    check(initialized.status == sed::level::MotsArchLightingEditStatus::Applied
            && initialized.message
                == "Initialized default-zero MotS ARCHLIGHTING records."
            && initialized.changedThingIndices == newThing
            && level.motsArchLighting.size() == 3
            && level.things[3].archLightingIndex == 2
            && hasExpectedArchObjectTopology(level.motsArchLighting[2])
            && level.motsArchLighting[2].meshes.size() == 2
            && level.motsArchLighting[2].meshes[0].vertices.size() == 3
            && level.motsArchLighting[2].meshes[0].vertices[0]
                == sed::MotsArchLightVertex{}
            && level.motsArchLighting[2].meshes[0].vertices[1]
                == sed::MotsArchLightVertex{}
            && level.motsArchLighting[2].meshes[0].vertices[2]
                == sed::MotsArchLightVertex{}
            && level.motsArchLighting[2].meshes[1].vertices.size() == 2
            && level.motsArchLighting[2].meshes[1].vertices[0]
                == sed::MotsArchLightVertex{}
            && level.motsArchLighting[2].meshes[1].vertices[1]
                == sed::MotsArchLightVertex{},
        "MOTS ARCHLIGHTING Initialize creates a zero-default record from validated topology");

    const std::vector<std::byte> beforeRejected =
        sed::assets::saveToSedBytes(level);
    auto invalidTopology = topology;
    invalidTopology.geosets[1][1] = 4u;
    const std::vector<sed::level::MotsArchLightingTopology> invalidTopologies{
        invalidTopology, invalidTopology };

    const std::vector<std::int32_t> mixedSelection{ 0, 1 };
    const std::vector<sed::level::MotsArchLightingTopology> mixedTopologies{
        topology, invalidTopology };
    const std::vector<std::byte> beforeMixedRejected =
        sed::assets::saveToSedBytes(level);
    const sed::level::MotsArchLightingEditResult mixedRejected =
        sed::level::applyMotsArchLightingEdit(
            level, mixedSelection, mixedTopologies, rgbEdit);
    check(mixedRejected.status == sed::level::MotsArchLightingEditStatus::Rejected
            && mixedRejected.message
                == "ARCHLIGHTING model topology is incompatible with every render geoset."
            && mixedRejected.changedThingIndices.empty()
            && sed::assets::saveToSedBytes(level) == beforeMixedRejected,
        "MOTS ARCHLIGHTING validates every selected topology before publishing a good-target/bad-target edit");

    const sed::level::MotsArchLightingEdit reset{
        sed::level::MotsArchLightingEditKind::Reset,
        sed::level::MotsArchLightingVertexMode::Rgb,
        1.0f,
        sed::ColorWhite };
    sed::Level missingArchive = level;
    missingArchive.motsArchLighting.clear();
    missingArchive.things[2].archLightingIndex = -2;
    const sed::Level missingArchiveBefore = missingArchive;
    const sed::level::MotsArchLightingEditResult rejectedMissingArchive =
        sed::level::applyMotsArchLightingEdit(
            missingArchive, selected, invalidTopologies, reset);
    check(rejectedMissingArchive.status == sed::level::MotsArchLightingEditStatus::Rejected
            && rejectedMissingArchive.message
                == "Selected Thing has no valid MotS ARCHLIGHTING record."
            && rejectedMissingArchive.changedThingIndices.empty()
            && sameArchLightingPayload(missingArchive, missingArchiveBefore),
        "MOTS ARCHLIGHTING Reset rejects invalid bindings without detaching or partially changing owners");

    const sed::level::MotsArchLightingEditResult rejected =
        sed::level::applyMotsArchLightingEdit(level, selected, invalidTopologies, rgbEdit);
    check(rejected.status == sed::level::MotsArchLightingEditStatus::Rejected
            && rejected.message
                == "ARCHLIGHTING model topology is incompatible with every render geoset."
            && rejected.changedThingIndices.empty()
            && sed::assets::saveToSedBytes(level) == beforeRejected,
        "MOTS ARCHLIGHTING edit rejects a later-geoset topology mismatch atomically");

    const std::vector<std::byte> beforeNoop = sed::assets::saveToSedBytes(level);
    const sed::level::MotsArchLightingEdit noop{
        sed::level::MotsArchLightingEditKind::Multiply,
        sed::level::MotsArchLightingVertexMode::Rgb,
        1.0f,
        sed::ColorWhite };
    const sed::level::MotsArchLightingEditResult noChange =
        sed::level::applyMotsArchLightingEdit(level, selected, topologies, noop);
    check(noChange.status == sed::level::MotsArchLightingEditStatus::Noop
            && noChange.message == "No MotS ARCHLIGHTING changes were necessary."
            && noChange.changedThingIndices.empty()
            && sed::assets::saveToSedBytes(level) == beforeNoop,
        "MOTS ARCHLIGHTING identity edit is a no-op and leaves the serialized state unchanged");

    sed::Level wrongProject = level;
    wrongProject.kind = sed::ProjectType::JKDF2;
    std::vector<std::int32_t> wrongProjectRefs{};
    wrongProjectRefs.reserve(wrongProject.things.size());
    for (const sed::Thing& thing : wrongProject.things)
    {
        wrongProjectRefs.push_back(thing.archLightingIndex);
    }
    const auto wrongProjectObjects = wrongProject.motsArchLighting;
    const sed::level::MotsArchLightingEditResult wrongProjectResult =
        sed::level::applyMotsArchLightingEdit(wrongProject, selected, topologies, rgbEdit);
    std::vector<std::int32_t> wrongProjectRefsAfter{};
    wrongProjectRefsAfter.reserve(wrongProject.things.size());
    for (const sed::Thing& thing : wrongProject.things)
    {
        wrongProjectRefsAfter.push_back(thing.archLightingIndex);
    }
    check(wrongProjectResult.status == sed::level::MotsArchLightingEditStatus::Rejected
            && wrongProjectResult.message
                == "MotS ARCHLIGHTING editing requires a MotS level."
            && wrongProjectResult.changedThingIndices.empty()
            && wrongProjectRefsAfter == wrongProjectRefs
            && wrongProject.motsArchLighting == wrongProjectObjects,
        "MOTS ARCHLIGHTING authoring rejects a non-MotS project without mutation");

    const std::vector<std::byte> beforeNonFinite = sed::assets::saveToSedBytes(level);
    sed::level::MotsArchLightingEdit nonFinite = monoEdit;
    nonFinite.monoMultiplier = std::numeric_limits<float>::infinity();
    const sed::level::MotsArchLightingEditResult nonFiniteResult =
        sed::level::applyMotsArchLightingEdit(level, selected, topologies, nonFinite);
    check(nonFiniteResult.status == sed::level::MotsArchLightingEditStatus::Rejected
            && nonFiniteResult.message
                == "MotS ARCHLIGHTING edit values must be finite."
            && nonFiniteResult.changedThingIndices.empty()
            && sed::assets::saveToSedBytes(level) == beforeNonFinite,
        "MOTS ARCHLIGHTING authoring rejects non-finite multipliers before changing any vertex");

    const sed::level::MotsArchLightingEditResult resetResult =
        sed::level::applyMotsArchLightingEdit(level, selected, topologies, reset);
    check(resetResult.status == sed::level::MotsArchLightingEditStatus::Applied
            && resetResult.message == "Reset MotS ARCHLIGHTING to the all-zero default."
            && resetResult.changedThingIndices == selected
            && level.things[0].archLightingIndex == 3
            && level.things[1].archLightingIndex == 0
            && level.things[2].archLightingIndex == 3
            && level.things[3].archLightingIndex == 2
            && level.motsArchLighting.size() == 4
            && sed::level::isDefaultMotsArchLighting(level.motsArchLighting[2])
            && sed::level::isDefaultMotsArchLighting(level.motsArchLighting[3])
            && level.motsArchLighting[0]
                == archObject(),
        "MOTS ARCHLIGHTING Reset copy-on-writes one zero record for selected owners and preserves the unselected custom record");
    const std::vector<std::byte> afterReset = sed::assets::saveToSedBytes(level);
    const sed::level::MotsArchLightingEditResult resetNoop =
        sed::level::applyMotsArchLightingEdit(level, selected, topologies, reset);
    check(resetNoop.status == sed::level::MotsArchLightingEditStatus::Noop
            && resetNoop.message == "No MotS ARCHLIGHTING changes were necessary."
            && resetNoop.changedThingIndices.empty()
            && sed::assets::saveToSedBytes(level) == afterReset,
        "MOTS ARCHLIGHTING Reset is a fixed-vector no-op for already-zero records");

    sed::Level staleTopologyLevel{};
    staleTopologyLevel.kind = sed::ProjectType::MOTS;
    sed::Thing& staleThing = staleTopologyLevel.newThing();
    staleThing.values.push_back(makeArchFlag());
    staleThing.archLightingIndex = 0;
    staleTopologyLevel.motsArchLighting.push_back(archObject());
    const std::vector<std::int32_t> staleThingIndex{ 0 };
    const std::vector<sed::level::MotsArchLightingTopology> changedTopology{
        sed::level::MotsArchLightingTopology{ { { 4u, 2u } } } };
    const std::vector<std::byte> staleBefore =
        sed::assets::saveToSedBytes(staleTopologyLevel);
    const sed::level::MotsArchLightingEditResult staleReset =
        sed::level::applyMotsArchLightingEdit(
            staleTopologyLevel, staleThingIndex, changedTopology, reset);
    check(staleReset.status == sed::level::MotsArchLightingEditStatus::Rejected
            && staleReset.message
                == "Selected Thing ARCHLIGHTING topology no longer matches its 3DO model."
            && staleReset.changedThingIndices.empty()
            && sed::assets::saveToSedBytes(staleTopologyLevel) == staleBefore,
        "MOTS ARCHLIGHTING Reset rejects stale mesh topology without changing the dormant/authored record");
}

void runMotsArchLightingArchFlagPromotionTests()
{
    sed::Level initial{};
    initial.kind = sed::ProjectType::MOTS;

    sed::ThingTemplate inherited{};
    inherited.name = "+arch_model";
    sed::TplValue inheritedFlags{};
    inheritedFlags.name = "thingflags";
    inheritedFlags.type = sed::ValueType::integer;
    inheritedFlags.intValue = 0x00000100;
    inherited.values.push_back(inheritedFlags);
    initial.templates.push_back(inherited);

    sed::Thing& thing = initial.newThing();
    thing.name = "arch_model_instance";
    thing.templateName = "+arch_model";
    sed::TplValue model{};
    model.name = "model3d";
    model.type = sed::ValueType::str;
    model.stringValue = "arch_model.3do";
    thing.values.push_back(model);

    const std::vector<std::int32_t> selected{ 0 };
    const std::vector<sed::level::MotsArchLightingTopology> topologies{
        sed::level::MotsArchLightingTopology{ { { 2u } } } };
    const sed::level::MotsArchLightingEdit initialize{
        sed::level::MotsArchLightingEditKind::Initialize,
        sed::level::MotsArchLightingVertexMode::Rgb,
        1.0f,
        sed::ColorWhite };

    const std::vector<std::byte> before = sed::assets::saveToSedBytes(initial);
    sed::editor::Document document{};
    document.level() = initial;
    sed::level::MotsArchLightingEditResult result{};
    const bool applied = document.applyUndoable(
        "ARCHLIGHTING Add sets the Arch flag on the model Thing",
        sed::editor::DocumentDirty::Things,
        [&](sed::editor::DocumentState& state)
        {
            result = sed::level::applyMotsArchLightingEdit(
                state.level, selected, topologies, initialize);
            return result.status
                == sed::level::MotsArchLightingEditStatus::Applied;
        });

    const sed::Thing& committedThing = document.level().things.front();
    const sed::TplValue* committedFlags = nullptr;
    for (auto it = committedThing.values.rbegin();
         it != committedThing.values.rend();
         ++it)
    {
        if (sed::text::iequalsAscii(it->name, "thingflags"))
        {
            committedFlags = &*it;
            break;
        }
    }
    check(applied
            && result.status == sed::level::MotsArchLightingEditStatus::Applied
            && result.changedThingIndices == selected
            && document.level().things.front().archLightingIndex >= 0
            && sed::level::thingFlags(
                document.level(), nullptr, committedThing) == 0x00000108
            && committedFlags
            && committedFlags->type == sed::ValueType::integer
            && committedFlags->intValue == 0x00000108
            && document.level().templates.front().values.front().intValue
                == 0x00000100
            && document.level().motsArchLighting.size() == 1,
        "MOTS ARCHLIGHTING Add sets the Arch flag on a valid model Thing without mutating its template");

    sed::editor::DocumentDirty dirty = sed::editor::DocumentDirty::None;
    check(document.undo(&dirty)
            && sed::assets::saveToSedBytes(document.level()) == before
            && document.level().things.front().archLightingIndex == -1
            && sed::level::thingFlags(
                document.level(), nullptr, document.level().things.front())
                == 0x00000100
            && document.level().motsArchLighting.empty(),
        "MOTS ARCHLIGHTING Add Undo restores the exact inherited flags and record table");
    check(document.redo(&dirty)
            && document.level().things.front().archLightingIndex >= 0
            && sed::level::thingFlags(
                document.level(), nullptr, document.level().things.front())
                == 0x00000108
            && document.level().templates.front().values.front().intValue
                == 0x00000100,
        "MOTS ARCHLIGHTING Add Redo reapplies only the instance flag and binding");

    const sed::Level nativeReload = sed::assets::loadFromSedBytes(
        sed::assets::saveToSedBytes(document.level()));
    check(nativeReload.things.size() == 1
            && nativeReload.things.front().archLightingIndex >= 0
            && sed::level::thingFlags(
                nativeReload, nullptr, nativeReload.things.front())
                == 0x00000108
            && nativeReload.templates.size() == 1
            && nativeReload.templates.front().values.front().intValue
                == 0x00000100
            && nativeReload.motsArchLighting.size() == 1
            && nativeReload.motsArchLighting.front().meshes.size() == 1
            && nativeReload.motsArchLighting.front().meshes.front().vertices.size()
                == 2,
        "MOTS ARCHLIGHTING promoted flags, binding, and record survive native SED export/reload");

    sed::Level rejected = initial;
    const std::vector<std::byte> beforeRejected =
        sed::assets::saveToSedBytes(rejected);
    const sed::level::MotsArchLightingEditResult rejectedResult =
        sed::level::applyMotsArchLightingEdit(
            rejected,
            selected,
            std::vector<sed::level::MotsArchLightingTopology>{
                sed::level::MotsArchLightingTopology{} },
            initialize);
    check(rejectedResult.status == sed::level::MotsArchLightingEditStatus::Rejected
            && rejectedResult.changedThingIndices.empty()
            && sed::assets::saveToSedBytes(rejected) == beforeRejected
            && rejected.things.front().archLightingIndex == -1
            && sed::level::thingFlags(
                rejected, nullptr, rejected.things.front()) == 0x00000100
            && rejected.templates.front().values.front().intValue == 0x00000100,
        "MOTS ARCHLIGHTING Add rejects invalid model topology atomically without promoting flags");

    sed::Level noModel = initial;
    noModel.things.front().values.clear();
    const std::vector<std::byte> beforeNoModel =
        sed::assets::saveToSedBytes(noModel);
    const sed::level::MotsArchLightingEditResult noModelResult =
        sed::level::applyMotsArchLightingEdit(
            noModel, selected, topologies, initialize);
    check(noModelResult.status == sed::level::MotsArchLightingEditStatus::Rejected
            && noModelResult.changedThingIndices.empty()
            && sed::assets::saveToSedBytes(noModel) == beforeNoModel
            && noModel.things.front().archLightingIndex == -1,
        "MOTS ARCHLIGHTING Add rejects a non-model Thing without setting the Arch flag");

    sed::Level invisible = initial;
    invisible.templates.front().values.front().intValue = 0x00000110;
    const std::vector<std::byte> beforeInvisible =
        sed::assets::saveToSedBytes(invisible);
    const sed::level::MotsArchLightingEditResult invisibleResult =
        sed::level::applyMotsArchLightingEdit(
            invisible, selected, topologies, initialize);
    check(invisibleResult.status == sed::level::MotsArchLightingEditStatus::Rejected
            && invisibleResult.changedThingIndices.empty()
            && sed::assets::saveToSedBytes(invisible) == beforeInvisible
            && invisible.things.front().archLightingIndex == -1
            && sed::level::thingFlags(
                invisible, nullptr, invisible.things.front()) == 0x00000110,
        "MOTS ARCHLIGHTING Add rejects an invisible model Thing without flag or record mutation");
}

} // namespace

void runEditorTests()
{
    runMotsArchLightingExportRoundTripRegressionTests();
    runMotsArchLightingAuthoringRegressionTests();
    runMotsArchLightingArchFlagPromotionTests();
    {
        const std::array<double, 6> expected{std::nextafter(1.0, 2.0),
            1000000.00000001, 1.234567890123456e-120, 3.141592653589793, -0.0,
            1.2345678901234567};
        const auto exact = [&](const sed::TplValue& frame)
        {
            std::array<double, 6> actual{};
            if (!frame.tryGetFrame(actual[0], actual[1], actual[2], actual[3], actual[4], actual[5]))
                return false;
            for (std::size_t i = 0; i < actual.size(); ++i)
                if (std::bit_cast<std::uint64_t>(actual[i]) != std::bit_cast<std::uint64_t>(expected[i]))
                    return false;
            return true;
        };
        sed::TplValue frame;
        frame.name = "frame";
        frame.type = sed::ValueType::frame;
        frame.adjType = sed::AdjType::frame;
        frame.setFrame(expected[0], expected[1], expected[2], expected[3], expected[4], expected[5]);
        check(exact(frame), "Frame setter preserves six exact authored doubles including signed zero");
        std::ostringstream spaced;
        spaced.imbue(std::locale::classic());
        spaced << std::setprecision(17);
        for (double component : expected) spaced << component << ' ';
        sed::TplValue normalized = frame;
        check(normalized.parse(spaced.str()) && exact(normalized),
            "Frame whitespace canonicalization preserves exact authored precision");
        sed::editor::Document doc;
        doc.level().newThing().values.push_back(frame);
        const auto initial = doc.level().things[0].values[0].stringValue;
        doc.pushThingSubsetUndo("Edit precise frame", sed::editor::DocumentDirty::Things, {0});
        doc.level().things[0].values[0].setFrame(0, 0, 0, 0, 0, 0);
        check(doc.undo() && exact(doc.level().things[0].values[0]),
            "Frame subset Undo restores exact authored component values");
        const auto restored = sed::assets::loadFromSedBytes(sed::assets::saveToSedBytes(doc.level()));
        check(restored.things[0].values[0].stringValue == initial && exact(restored.things[0].values[0]),
            "Native SED preserves exact edited Frame components");
        check(doc.redo() && doc.level().things[0].values[0].stringValue != initial && doc.undo()
                && exact(doc.level().things[0].values[0]),
            "Frame redo and subsequent Undo preserve the exact original edit");
    }
    {
        // Public mutable Level access can leave a compact record's final target
        // stale. Validate the entire target set before touching its first item.
        sed::editor::Document doc{};
        for ( int index = 0; index < 3; ++index )
        {
            doc.level().newThing().x = index;
        }
        doc.selection().things.insert(0);
        doc.pushThingSubsetUndo("Move subset", sed::editor::DocumentDirty::Things, {0, 2});
        doc.level().things[0].x = 42.0;
        doc.level().things.pop_back();
        const auto before = sed::assets::saveToSedBytes(doc.level());
        const auto signature = doc.undoSignature();
        auto dirty = editor::DocumentDirty::Structure;
        std::vector<std::int32_t> indices{-77};
        check(!doc.undo(&dirty, &indices)
                && dirty == editor::DocumentDirty::Structure && indices == std::vector<std::int32_t>{-77}
                && sed::assets::saveToSedBytes(doc.level()) == before
                && doc.undoDepth() == 1 && doc.redoDepth() == 0
                && doc.undoSignature() == signature
                && doc.selection().things.contains(0),
            "Document: invalid compact Thing target rejects before any earlier target is swapped");
    }
    {
        editor::Document empty;
        for (const bool redo : {false, true})
        {
            auto dirty = editor::DocumentDirty::Structure;
            std::vector<std::int32_t> indices{-77};
            check(!(redo ? empty.redo(&dirty, &indices) : empty.undo(&dirty, &indices))
                    && dirty == editor::DocumentDirty::Structure && indices.empty()
                    && empty.undoDepth() == 0 && empty.redoDepth() == 0,
                "Empty history returns false, clears target-list OUT and retains dirty-mask OUT");
            check(!(redo ? empty.redo() : empty.undo()), "Empty history accepts absent optional outputs");
        }
    }
    {
        // A transform of a small selection must not require a snapshot of all
        // Things. The compact record swaps only the addressed values while
        // preserving selection/curId and exact redo behavior.
        sed::editor::Document doc{};
        for (int i = 0; i < 128; ++i)
        {
            sed::Thing& thing = doc.level().newThing();
            thing.name = "thing_" + std::to_string(i);
            thing.x = static_cast<double>(i);
            thing.sector = 7;
        }
        doc.selection().things.insert(42);
        const std::int32_t beforeCurId = doc.level().curId;

        doc.pushThingSubsetUndo(
            "Move thing",
            sed::editor::DocumentDirty::Things,
            std::vector<std::int32_t>{42});
        doc.level().things[42].x = 900.0;
        doc.level().things[42].sector = -1;

        sed::editor::DocumentDirty dirty = sed::editor::DocumentDirty::None;
        std::vector<std::int32_t> compactTargets{};
        check(doc.undo(&dirty, &compactTargets)
                && dirty == sed::editor::DocumentDirty::Things
                && compactTargets == std::vector<std::int32_t>{42}
                && doc.level().things.size() == 128
                && doc.level().things[42].x == 42.0
                && doc.level().things[42].sector == 7
                && doc.level().curId == beforeCurId
                && doc.selection().things.contains(42),
            "Document: compact Thing-subset undo restores only the authored values");
        compactTargets.clear();
        dirty = sed::editor::DocumentDirty::Structure;
        check(doc.redo(&dirty, &compactTargets)
                && dirty == sed::editor::DocumentDirty::Things
                && compactTargets == std::vector<std::int32_t>{42}
                && doc.level().things.size() == 128
                && doc.level().things[42].x == 900.0
                && doc.level().things[42].sector == -1,
            "Document: compact Thing-subset redo restores the transformed value");

        // Recovery capture copies a Document while the editor remains live.
        // Historical payloads are shared immutably and become private on the
        // first undo/redo in either copy.
        sed::editor::Document recoveryCopy = doc;
        check(doc.undo() && recoveryCopy.undo()
                && doc.level().things[42].x == 42.0
                && recoveryCopy.level().things[42].x == 42.0,
            "Document: copied recovery history is copy-on-write and undo-safe");
        doc.level().things[42].x = -500.0;
        check(recoveryCopy.redo()
                && recoveryCopy.level().things[42].x == 900.0
                && doc.level().things[42].x == -500.0,
            "Document: GUI and recovery copies do not mutate shared snapshots across threads");
    }

    {
        // Pasting a MotS architectural-light object appends both the table
        // entry and a Thing whose reference is remapped to that entry.  The
        // ordinary Things snapshot must restore the pair atomically through
        // Undo and Redo; restoring only the Thing vector would leave a stale
        // table or a dangling archLightingIndex in the renderer.
        sed::editor::Document doc{};
        doc.level().kind = sed::ProjectType::MOTS;
        sed::Thing& original = doc.level().newThing();
        original.name = "original";
        original.archLightingIndex = 0;
        sed::MotsArchLightObject originalObject{};
        originalObject.wireIndex = 5;
        sed::MotsArchLightMesh originalMesh{};
        originalMesh.wireIndex = 8;
        originalMesh.vertices.push_back(sed::MotsArchLightVertex{
            0.25f, 0.5f, 0.75f, 1.0f });
        originalObject.meshes.push_back(std::move(originalMesh));
        doc.level().motsArchLighting.push_back(std::move(originalObject));
        doc.selection().things.insert(0);

        const bool pasted = doc.applyUndoable(
            "Paste ARCHLIGHTING Thing", sed::editor::DocumentDirty::Things,
            [](sed::editor::DocumentState& state)
            {
                sed::MotsArchLightObject pastedObject{};
                pastedObject.wireIndex = 13;
                sed::MotsArchLightMesh pastedMesh{};
                pastedMesh.wireIndex = 21;
                pastedMesh.vertices.push_back(sed::MotsArchLightVertex{
                    -1.0f, 2.0f, -3.0f, 4.0f });
                pastedObject.meshes.push_back(std::move(pastedMesh));
                state.level.motsArchLighting.push_back(std::move(pastedObject));

                sed::Thing& pastedThing = state.level.newThing();
                pastedThing.name = "pasted";
                pastedThing.archLightingIndex = 1;
                pastedThing.x = 42.0;
                state.selection.clear();
                state.selection.things.insert(1);
                return true;
            });
        const auto snapshotStats = doc.undoSnapshotStats();
        check(pasted && snapshotStats.full == 0 && snapshotStats.things == 1
                && doc.level().things.size() == 2
                && doc.level().motsArchLighting.size() == 2
                && doc.level().things[1].archLightingIndex == 1
                && doc.level().motsArchLighting[1].wireIndex == 13
                && doc.selection().things.contains(1),
            "Document: ARCHLIGHTING paste stores one complete Things snapshot");

        sed::editor::DocumentDirty dirty = sed::editor::DocumentDirty::None;
        check(doc.undo(&dirty)
                && sed::editor::hasAny(dirty, sed::editor::DocumentDirty::Things)
                && doc.level().things.size() == 1
                && doc.level().things[0].archLightingIndex == 0
                && doc.level().motsArchLighting.size() == 1
                && doc.level().motsArchLighting[0].wireIndex == 5
                && doc.selection().things.contains(0)
                && !doc.selection().things.contains(1),
            "Document: ARCHLIGHTING paste Undo removes appended Thing and table entry together");

        check(doc.redo(&dirty)
                && sed::editor::hasAny(dirty, sed::editor::DocumentDirty::Things)
                && doc.level().things.size() == 2
                && doc.level().things[1].archLightingIndex == 1
                && doc.level().motsArchLighting.size() == 2
                && doc.level().motsArchLighting[1].wireIndex == 13
                && doc.level().motsArchLighting[1].meshes.size() == 1
                && doc.level().motsArchLighting[1].meshes[0].wireIndex == 21
                && doc.selection().things.contains(1)
                && !doc.selection().things.contains(0),
            "Document: ARCHLIGHTING paste Redo restores the table and remapped Thing reference together");
    }

    {
        // Geometry transforms have the same bounded-history requirement as
        // Thing transforms. A sector subset owns complete touched Sectors
        // (including shared vertices and adjoins) but never copies unrelated
        // world geometry. The record is a bidirectional swap, so redo is
        // exact and the concrete representation is observable in tests.
        sed::editor::Document doc{};
        for (int index = 0; index < 96; ++index)
        {
            sed::Sector& sector = doc.level().newSector();
            sector.vertices.push_back(sed::Vertex{
                static_cast<double>(index), 0.0, 0.0, 0});
            sed::Surface surface{};
            surface.vertices.push_back(0);
            sector.surfaces.push_back(std::move(surface));
        }
        doc.selection().sectors.insert(7);
        doc.pushSectorSubsetUndo(
            "Move sector",
            sed::editor::DocumentDirty::LevelGeometry,
            std::vector<std::int32_t>{7});
        doc.level().sectors[7].vertices[0].x = 700.0;
        doc.selection().sectors.clear();
        doc.selection().sectors.insert(8);

        const sed::editor::DocumentUndoSnapshotStats stats = doc.undoSnapshotStats();
        check(stats.full == 0 && stats.sectorSubsets == 1,
            "Document: Sector transform retains a compact sector subset snapshot");
        check(doc.undo()
                && doc.level().sectors[7].vertices[0].x == 7.0
                && doc.selection().sectors.contains(7),
            "Document: compact Sector subset undo restores geometry and selection");
        check(doc.redo()
                && doc.level().sectors[7].vertices[0].x == 700.0
                && doc.selection().sectors.contains(8),
            "Document: compact Sector subset redo restores geometry and selection");

        // A sector transform also moves the vertices on every valid adjoined
        // mate surface. Capture both owner sectors before the real geometry
        // operation so undo/redo cannot leave one side of the portal behind.
        sed::editor::Document adjoinedDoc{};
        adjoinedDoc.level().sectors.resize(2);
        for (sed::Sector& sector : adjoinedDoc.level().sectors)
        {
            sector.vertices = {
                sed::Vertex{ 0.0, 0.0, 0.0, 0 },
                sed::Vertex{ 1.0, 0.0, 0.0, 0 },
                sed::Vertex{ 0.0, 1.0, 0.0, 0 },
            };
            sed::Surface surface{};
            surface.vertices = { 0, 1, 2 };
            sector.surfaces.push_back(std::move(surface));
        }
        adjoinedDoc.level().sectors[0].surfaces[0].adjoin = sed::SurfaceRef{ 1, 0 };
        adjoinedDoc.level().sectors[1].surfaces[0].adjoin = sed::SurfaceRef{ 0, 0 };
        adjoinedDoc.selection().sectors.insert(0);
        adjoinedDoc.pushSectorSubsetUndo(
            "Move adjoined sector",
            sed::editor::DocumentDirty::LevelGeometry,
            std::vector<std::int32_t>{ 0, 1 });
        const std::vector<std::int32_t> movedSector{ 0 };
        sed::level::levelTranslateSectors(
            adjoinedDoc.level(), movedSector, 4.0, 0.0, 0.0);
        check(adjoinedDoc.level().sectors[0].vertices[0].x == 4.0
                && adjoinedDoc.level().sectors[1].vertices[0].x == 4.0,
            "Document: sector transform moves both sides of an adjoined portal");
        check(adjoinedDoc.undo()
                && adjoinedDoc.level().sectors[0].vertices[0].x == 0.0
                && adjoinedDoc.level().sectors[1].vertices[0].x == 0.0
                && adjoinedDoc.selection().sectors.contains(0),
            "Document: adjoined Sector subset undo restores both owner sectors");
        check(adjoinedDoc.redo()
                && adjoinedDoc.level().sectors[0].vertices[0].x == 4.0
                && adjoinedDoc.level().sectors[1].vertices[0].x == 4.0,
            "Document: adjoined Sector subset redo restores both owner sectors");

        doc.selection().clear();
        doc.selection().surfaces.insert(sed::SurfaceRef{ 7, 0 });
        doc.pushSurfaceSubsetUndo(
            "Move surface",
            sed::editor::DocumentDirty::LevelGeometry,
            std::vector<std::int32_t>{7});
        doc.level().sectors[7].vertices[0].x = 701.0;
        doc.selection().clear();
        doc.selection().sectors.insert(9);
        check(doc.undoSnapshotStats().surfaceSubsets == 1,
            "Document: Surface transform retains a compact owning-sector snapshot");
        check(doc.undo()
                && doc.level().sectors[7].vertices[0].x == 700.0
                && doc.selection().surfaces.contains(sed::SurfaceRef{ 7, 0 }),
            "Document: compact Surface subset undo restores shared vertices and selection");
        check(doc.redo()
                && doc.level().sectors[7].vertices[0].x == 701.0
                && doc.selection().sectors.contains(9),
            "Document: compact Surface subset redo restores shared vertices and selection");
    }

    {
        sed::editor::UndoHistory<sed::Level> history{};
        history.current().clear();

        history.current().newSector();
        check(history.current().sectors.size() == 1, "Undo: initial state");
        check(!history.canUndo(), "Undo: cannot undo initial");

        history.pushUndo("Add sector");
        history.current().newSector();
        check(history.current().sectors.size() == 2, "Undo: after change");
        check(history.canUndo(), "Undo: can undo");
        check(history.nextUndoLabel() == "Add sector", "Undo: label");

        check(history.undo(), "Undo: undo");
        check(history.current().sectors.size() == 1, "Undo: after undo");
        check(history.canRedo(), "Undo: can redo");
        check(history.nextRedoLabel() == "Add sector", "Undo: redo label");

        check(history.redo(), "Undo: redo");
        check(history.current().sectors.size() == 2, "Undo: after redo");
    }

    {
        sed::editor::Selection selection{};
        check(selection.empty(), "Selection.empty");
        selection.things.insert(42);
        check(!selection.empty(), "Selection.insert");
        selection.clear();
        check(selection.empty(), "Selection.clear");

        selection.cogs.insert(7);
        check(!selection.empty(), "Selection.insert cogs");
    }

    {
        // Recovery must distinguish an empty timeline from the same current
        // document with a redo-only branch. Dirty/save identity deliberately
        // remains at the original undo position in both cases.
        sed::editor::Document doc{};
        sed::Sector& sector = doc.level().newSector();
        sector.flags = 0x100;
        const std::uint64_t cleanUndoSignature = doc.undoSignature();
        const std::uint64_t cleanRedoSignature = doc.redoSignature();

        doc.pushUndo("Edit sector flags",
            sed::editor::DocumentDirty::LevelGeometry);
        doc.level().sectors.front().flags = 0x200;
        check(doc.undo()
                && doc.level().sectors.front().flags == 0x100
                && doc.undoSignature() == cleanUndoSignature
                && doc.redoSignature() != cleanRedoSignature
                && doc.undoDepth() == 0
                && doc.redoDepth() == 1,
            "Document: redo signature distinguishes an undone branch at the clean Level");

        check(doc.redo()
                && doc.level().sectors.front().flags == 0x200
                && doc.undoDepth() == 1
                && doc.redoDepth() == 0
                && doc.redoSignature() == cleanRedoSignature,
            "Document: redo signature returns to its empty identity after Redo");
    }

    {
        sed::editor::Document doc{};
        doc.level().clear();
        doc.selection().clear();

        doc.level().newSector();
        doc.selection().sectors.insert(0);

        check(doc.level().sectors.size() == 1, "Document: initial sectors");
        check(doc.selection().sectors.contains(0), "Document: initial selection");
        check(!doc.canUndo(), "Document: cannot undo initial");

        doc.pushUndo("Change level+selection");
        doc.level().newSector();
        doc.selection().clear();
        doc.selection().sectors.insert(1);

        check(doc.level().sectors.size() == 2, "Document: after change sectors");
        check(doc.selection().sectors.contains(1), "Document: after change selection");
        check(doc.canUndo(), "Document: can undo");
        check(doc.nextUndoLabel() == "Change level+selection", "Document: undo label");

        check(doc.undo(), "Document: undo");
        check(doc.level().sectors.size() == 1, "Document: after undo sectors");
        check(doc.selection().sectors.contains(0), "Document: after undo selection");
        check(doc.canRedo(), "Document: can redo");
        check(doc.nextRedoLabel() == "Change level+selection", "Document: redo label");

        check(doc.redo(), "Document: redo");
        check(doc.level().sectors.size() == 2, "Document: after redo sectors");
        check(doc.selection().sectors.contains(1), "Document: after redo selection");

        doc.clear();
        check(doc.level().sectors.empty(), "Document: clear level");
        check(doc.selection().empty(), "Document: clear selection");
        check(!doc.canUndo(), "Document: clear cannot undo");
        check(!doc.canRedo(), "Document: clear cannot redo");
    }

    {
        // History navigation queries only compact transition selection
        // metadata.  It must identify both sides without moving the live
        // cursor or copying a complete Level into the returned DTO.
        sed::editor::Document doc{};
        doc.level().newSector();
        doc.level().newSector();
        doc.selection().sectors.insert(0);
        doc.pushUndo("Move selected sector", sed::editor::DocumentDirty::LevelGeometry);
        doc.selection().clear();
        doc.selection().sectors.insert(1);

        const auto transition = doc.transitionAtDepth(1);
        check(transition.has_value()
                && transition->beforeSelection.sectors.contains(0)
                && transition->afterSelection.sectors.contains(1)
                && doc.undoDepth() == 1
                && doc.redoDepth() == 0
                && doc.selection().sectors.contains(1),
            "Document: history transition query returns bounded before/after selections without moving cursor");

        check(doc.undo(), "Document: transition query test creates redo branch");
        const auto redoTransition = doc.transitionAtDepth(1);
        check(redoTransition.has_value()
                && redoTransition->beforeSelection.sectors.contains(0)
                && redoTransition->afterSelection.sectors.contains(1)
                && doc.undoDepth() == 0
                && doc.redoDepth() == 1,
            "Document: transition query remains exact on redo-only branch");
    }

    {
        // Compact subset records remain useful navigation metadata even when
        // the editor selection is empty.  Only the small index lists are
        // exposed; the stored Thing/Sector values never escape the history.
        sed::editor::Document doc{};
        doc.level().things.emplace_back();
        doc.pushThingSubsetUndo(
            "Move unselected Thing", sed::editor::DocumentDirty::Things, { 0 });
        doc.level().things[0].x = 4.0;

        const auto thingTransition = doc.transitionAtDepth(1);
        check(thingTransition.has_value()
                && thingTransition->beforeSelection.empty()
                && thingTransition->afterSelection.empty()
                && thingTransition->affectedThingIndices
                    == std::vector<std::int32_t>{ 0 },
            "Document: Thing subset transition exposes its affected index without selection metadata");

        doc.level().newSector();
        doc.pushSectorSubsetUndo(
            "Move unselected Sector", sed::editor::DocumentDirty::LevelGeometry, { 0 });
        doc.level().sectors[0].ambient = sed::ColorWhite;
        const auto sectorTransition = doc.transitionAtDepth(2);
        check(sectorTransition.has_value()
                && sectorTransition->affectedSectorIndices
                    == std::vector<std::int32_t>{ 0 },
            "Document: Sector subset transition exposes its affected index without selection metadata");
    }

    {
        // A structural command can replace its source selection with newly
        // created topology.  The History target must survive later ordinary
        // selection changes and both directions of the undo/redo swap without
        // retaining another Level.
        sed::editor::Document doc{};
        doc.level().newSector();
        doc.selection().sectors.insert(0);
        const bool applied = doc.applyUndoable(
            "Sector: Cleaved",
            sed::editor::DocumentDirty::LevelGeometry
                | sed::editor::DocumentDirty::Structure,
            [](sed::editor::DocumentState& state)
            {
                state.level.newSector();
                state.selection.clear();
                state.selection.sectors.insert(1);
                return true;
            });
        doc.selection().clear();

        const auto liveTransition = doc.transitionAtDepth(1);
        const bool undid = doc.undo();
        const auto redoTransition = doc.transitionAtDepth(1);
        const bool redid = doc.redo();
        const auto restoredTransition = doc.transitionAtDepth(1);
        check(applied
                && liveTransition
                && liveTransition->beforeSelection.sectors
                    == std::unordered_set<std::int32_t>{ 0 }
                && liveTransition->afterSelection.sectors
                    == std::unordered_set<std::int32_t>{ 1 }
                && undid
                && redoTransition
                && redoTransition->afterSelection.sectors
                    == std::unordered_set<std::int32_t>{ 1 }
                && redid
                && restoredTransition
                && restoredTransition->afterSelection.sectors
                    == std::unordered_set<std::int32_t>{ 1 },
            "Document: structural History navigation selection survives later selection changes and undo/redo");
    }

    {
        sed::editor::Document doc{};
        doc.level().newSector();
        doc.level().sectors[0].colormap = "authoritative.cmp";
        doc.selection().sectors.insert(0);
        check(doc.applyUndoable(
                "Sector: Added",
                sed::editor::DocumentDirty::LevelGeometry
                    | sed::editor::DocumentDirty::Structure,
                [](sed::editor::DocumentState& state)
                {
                    state.level.newSector();
                    state.selection.clear();
                    state.selection.sectors.insert(1);
                    return true;
                })
                && doc.undo(),
            "Document: exception rollback fixture creates a redo branch");
        const std::size_t undoDepth = doc.undoDepth();
        const std::size_t redoDepth = doc.redoDepth();
        const std::uint64_t undoSignature = doc.undoSignature();
        const std::uint64_t redoSignature = doc.redoSignature();
        const std::int32_t curId = doc.level().curId;

        bool threw = false;
        try
        {
            (void)doc.applyUndoable(
                "Sector: Broken command",
                sed::editor::DocumentDirty::LevelGeometry
                    | sed::editor::DocumentDirty::Structure,
                [](sed::editor::DocumentState& state) -> bool
                {
                    state.level.newSector();
                    state.level.sectors[0].colormap = "partial.cmp";
                    state.selection.clear();
                    state.selection.sectors.insert(1);
                    throw std::runtime_error("injected mutation failure");
                });
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        check(threw
                && doc.level().sectors.size() == 1
                && doc.level().sectors[0].colormap == "authoritative.cmp"
                && doc.level().curId == curId
                && doc.selection().sectors
                    == std::unordered_set<std::int32_t>{ 0 }
                && doc.undoDepth() == undoDepth
                && doc.redoDepth() == redoDepth
                && doc.undoSignature() == undoSignature
                && doc.redoSignature() == redoSignature,
            "Document: a throwing undoable mutation restores exact state, selection, IDs, and history stacks");
    }

    {
        constexpr std::int32_t maximumId =
            std::numeric_limits<std::int32_t>::max();
        sed::editor::Document doc{};
        doc.level().curId = maximumId - 1;

        check(doc.applyUndoable(
                "Selection marker",
                sed::editor::DocumentDirty::Structure,
                [](sed::editor::DocumentState& state)
                {
                    state.selection.sectors.insert(7);
                    return true;
                })
                && doc.undo(),
            "Document: ID exhaustion fixture retains a redo branch before the rejected edit");
        const std::vector<std::byte> levelBefore =
            sed::assets::saveToSedBytes(doc.level());
        const std::size_t undoDepthBefore = doc.undoDepth();
        const std::size_t redoDepthBefore = doc.redoDepth();
        const std::uint64_t undoSignatureBefore = doc.undoSignature();
        const std::uint64_t redoSignatureBefore = doc.redoSignature();
        bool exhausted = false;
        try
        {
            (void)doc.applyUndoable(
                "Thing: Exhausted batch",
                sed::editor::DocumentDirty::Structure
                    | sed::editor::DocumentDirty::Things,
                [](sed::editor::DocumentState& state)
                {
                    state.selection.clear();
                    state.selection.things.insert(11);
                    state.level.newThing();
                    state.level.newThing();
                    return true;
                });
        }
        catch (const sed::IdExhaustionError&)
        {
            exhausted = true;
        }

        check(exhausted
                && sed::assets::saveToSedBytes(doc.level()) == levelBefore
                && doc.selection().empty()
                && doc.level().curId == maximumId - 1
                && doc.undoDepth() == undoDepthBefore
                && doc.redoDepth() == redoDepthBefore
                && doc.undoSignature() == undoSignatureBefore
                && doc.redoSignature() == redoSignatureBefore,
            "Document: exhausted multi-Thing edit restores serialized state, selection, next ID, and exact Undo/Redo history");
        check(doc.redo()
                && doc.selection().sectors == std::unordered_set<std::int32_t>{ 7 }
                && doc.undo()
                && doc.selection().empty()
                && doc.level().curId == maximumId - 1,
            "Document: the preexisting Redo and Undo branch remains usable after ID exhaustion rollback");
    }

    {
        // A baked-light source command owns both the source record and the
        // persisted lighting output that Auto publishes shortly afterwards.
        // The derived touch must affect save-state identity without creating
        // a second undo item, and undo/redo must keep both halves coherent.
        sed::editor::Document doc{};
        sed::Sector& sector = doc.level().newSector();
        sed::Surface& surface = sector.surfaces.emplace_back();
        surface.texVertices.push_back(sed::TexVertex{0.0f, 0.0f, sed::ColorBlack});
        sector.ambient = sed::ColorBlack;
        sed::SedLight& light = doc.level().newLight();
        light.intensity = 1.0;

        doc.pushUndo("Edit baked light", sed::editor::DocumentDirty::Lights);
        doc.level().lights[0].intensity = 2.0;
        doc.level().sectors[0].surfaces[0].texVertices[0].color = sed::ColorRed;
        doc.level().sectors[0].ambient = sed::ColorBlue;

        const std::uint64_t beforeDerivedTouch = doc.undoSignature();
        check(doc.touchLatestUndoRecord(),
            "Document: derived baked output touches the source undo record");
        const std::uint64_t afterDerivedTouch = doc.undoSignature();
        check(afterDerivedTouch != beforeDerivedTouch && doc.undoDepth() == 1,
            "Document: derived baked output changes save identity without another undo item");

        sed::editor::DocumentDirty dirty = sed::editor::DocumentDirty::None;
        check(doc.undo(&dirty)
                && sed::editor::hasAny(dirty, sed::editor::DocumentDirty::Lights)
                && doc.level().lights[0].intensity == 1.0
                && doc.level().sectors[0].surfaces[0].texVertices[0].color == sed::ColorBlack
                && doc.level().sectors[0].ambient == sed::ColorBlack,
            "Document: Light undo restores source, baked colors, and ambient together");
        check(doc.redo(&dirty)
                && doc.level().lights[0].intensity == 2.0
                && doc.level().sectors[0].surfaces[0].texVertices[0].color == sed::ColorRed
                && doc.level().sectors[0].ambient == sed::ColorBlue
                && doc.undoSignature() == afterDerivedTouch,
            "Document: Light redo restores source, baked colors, ambient, and save identity together");
    }

    {
        sed::editor::Document doc{};
        doc.level().kind = sed::ProjectType::MOTS;
        sed::Thing& owner = doc.level().newThing();
        owner.id = 31;
        owner.name = "baked_arch";
        owner.archLightingIndex = 0;
        owner.values.push_back(makeArchFlag());
        sed::MotsArchLightObject authored = archObject();
        authored.wireIndex = 14;
        doc.level().motsArchLighting.push_back(authored);
        sed::SedLight& source = doc.level().newLight();
        source.id = 41;
        const sed::MotsArchLightingBakeBaseline baseline{
            owner.id, "arch.3do", 0, authored };

        doc.pushUndo("Bake ARCH light", sed::editor::DocumentDirty::Lights);
        doc.level().lights[0].flags = sed::LF_AffectArchThings;
        sed::MotsArchLightObject generated = authored;
        generated.wireIndex = 15;
        generated.meshes[0].vertices[0].red += 0.75f;
        doc.level().motsArchLighting.push_back(generated);
        doc.level().things[0].archLightingIndex = 1;
        doc.level().motsArchLightingBakeBaselines.push_back(baseline);
        const sed::MotsArchLightingBakeBaseline afterBaseline =
            doc.level().motsArchLightingBakeBaselines.front();

        sed::editor::DocumentDirty dirty = sed::editor::DocumentDirty::None;
        const bool undone = doc.undo(&dirty);
        const bool exactUndo = undone
            && dirty == sed::editor::DocumentDirty::Lights
            && doc.level().lights[0].flags == 0
            && doc.level().things[0].archLightingIndex == 0
            && doc.level().motsArchLighting.size() == 1
            && doc.level().motsArchLighting[0] == authored
            && doc.level().motsArchLightingBakeBaselines.empty();
        const bool redone = doc.redo(&dirty);
        check(exactUndo && redone
                && dirty == sed::editor::DocumentDirty::Lights
                && doc.level().lights[0].flags == sed::LF_AffectArchThings
                && doc.level().things[0].archLightingIndex == 1
                && doc.level().motsArchLighting.size() == 2
                && doc.level().motsArchLighting[0] == authored
                && doc.level().motsArchLighting[1] == generated
                && doc.level().motsArchLightingBakeBaselines.size() == 1
                && doc.level().motsArchLightingBakeBaselines.front() == afterBaseline,
            "Document: MotS Lights-only bake snapshots atomically restore and redo Thing references, ARCH output, and authored baseline");
    }

    {
        // Pure COG edits must not snapshot and later restore unrelated Level
        // domains. Manual-Save normalization can legitimately update derived
        // Thing/header data after the COG record was captured.
        sed::editor::Document doc{};
        sed::Thing& thing = doc.level().newThing();
        thing.x = 1.0;
        sed::Cog& cog = doc.level().newCog();
        cog.name = "before.cog";
        doc.level().lvisstring = "before-normalization";

        doc.pushUndo("Edit cog", sed::editor::DocumentDirty::Cogs);
        doc.level().cogs.front().name = "after.cog";

        // Simulate save-derived normalization performed outside the authored
        // COG command and verify it survives both directions.
        doc.level().things.front().x = 9.0;
        doc.level().lvisstring = "normalized";

        sed::editor::DocumentDirty dirty = sed::editor::DocumentDirty::None;
        check(doc.undo(&dirty)
                && sed::editor::hasAny(dirty, sed::editor::DocumentDirty::Cogs)
                && doc.level().cogs.front().name == "before.cog"
                && doc.level().things.front().x == 9.0
                && doc.level().lvisstring == "normalized",
            "Document: compact COG undo preserves unrelated normalized Level state");
        check(doc.redo(&dirty)
                && doc.level().cogs.front().name == "after.cog"
                && doc.level().things.front().x == 9.0
                && doc.level().lvisstring == "normalized",
            "Document: compact COG redo preserves unrelated normalized Level state");
    }

    {
        // A Thing brush stroke is one user-visible command even though it
        // appends several batches while the pointer is held. Capture the
        // pre-stroke Things state once, then rotate that record's identity as
        // later batches arrive so dirty/recovery tracking observes every
        // append without another complete Things snapshot.
        sed::editor::Document doc{};
        sed::Thing& original = doc.level().newThing();
        original.name = "original";
        doc.selection().things.insert(0);
        const std::int32_t preStrokeCurId = doc.level().curId;

        doc.pushThingTailUndo(
            "Brush add things",
            sed::editor::DocumentDirty::Structure
                | sed::editor::DocumentDirty::Things);
        const std::uint64_t firstStampSignature = doc.undoSignature();

        sed::Thing& first = doc.level().newThing();
        first.name = "first_stamp";
        first.x = 1.0;
        sed::Thing& second = doc.level().newThing();
        second.name = "first_stamp";
        second.x = 2.0;
        doc.selection().clear();
        doc.selection().things.insert(1);
        doc.selection().things.insert(2);

        sed::Thing& third = doc.level().newThing();
        third.name = "later_stamp";
        third.x = 3.0;
        doc.selection().clear();
        doc.selection().things.insert(3);
        check(doc.touchLatestUndoRecord(),
            "Document: later Thing-brush stamp touches the stroke undo record");
        const std::uint64_t completedStrokeSignature = doc.undoSignature();
        const auto tailTransition = doc.transitionAtDepth(1);

        check(doc.undoDepth() == 1
                && completedStrokeSignature != firstStampSignature
                && doc.level().things.size() == 4
                && doc.selection().things == std::unordered_set<std::int32_t>{3},
            "Document: multi-stamp Thing brush remains one dirty undo command");
        check(tailTransition.has_value()
                && tailTransition->affectedThingIndices
                    == std::vector<std::int32_t>{ 1, 2, 3 },
            "Document: finalized Thing-tail transition exposes the appended range");

        sed::editor::DocumentDirty dirty = sed::editor::DocumentDirty::None;
        check(doc.undo(&dirty)
                && sed::editor::hasAny(dirty, sed::editor::DocumentDirty::Structure)
                && sed::editor::hasAny(dirty, sed::editor::DocumentDirty::Things)
                && doc.level().things.size() == 1
                && doc.level().things.front().name == "original"
                && doc.level().curId == preStrokeCurId
                && doc.selection().things == std::unordered_set<std::int32_t>{0},
            "Document: Thing-brush undo restores the complete pre-stroke state");
        check(doc.redo(&dirty)
                && doc.level().things.size() == 4
                && doc.level().things[1].name == "first_stamp"
                && doc.level().things[2].x == 2.0
                && doc.level().things[3].name == "later_stamp"
                && doc.selection().things == std::unordered_set<std::int32_t>{3}
                && doc.undoSignature() == completedStrokeSignature,
            "Document: Thing-brush redo restores every batch and final selection atomically");
    }

    {
        // Incremental persistence must follow the concrete record variant,
        // not dirtyFlags. In particular, a mixed COG+Thing record is a full
        // snapshot, while an append-only Thing tail remains a bounded Thing
        // transition even though its descriptive flags include Structure.
        sed::editor::Document doc{};
        doc.level().newSector().surfaces.emplace_back();
        doc.level().newThing();
        doc.level().cogs.emplace_back();
        doc.level().lights.emplace_back();

        doc.pushUndo("full",
            sed::editor::DocumentDirty::Things
                | sed::editor::DocumentDirty::Cogs);
        doc.level().lvisstring = "full changed";

        doc.pushUndo("things", sed::editor::DocumentDirty::Things);
        doc.level().things.front().x += 1.0;

        doc.pushThingSubsetUndo("thing subset",
            sed::editor::DocumentDirty::Things, { 0 });
        doc.level().things.front().y += 2.0;

        doc.pushThingTailUndo("thing tail",
            sed::editor::DocumentDirty::Structure
                | sed::editor::DocumentDirty::Things);
        doc.level().newThing();

        doc.pushUndo("cogs", sed::editor::DocumentDirty::Cogs);
        doc.level().cogs.front().name = "changed.cog";

        doc.pushUndo("lights", sed::editor::DocumentDirty::Lights);
        doc.level().lights.front().range += 1.0;

        doc.pushUndo("things lights",
            sed::editor::DocumentDirty::Things
                | sed::editor::DocumentDirty::Lights);
        doc.level().things.front().z += 3.0;
        doc.level().lights.front().intensity += 2.0;

        doc.pushSectorSubsetUndo("sector subset",
            sed::editor::DocumentDirty::LevelGeometry, { 0 });
        doc.level().sectors.front().ambient.r += 0.125f;

        doc.pushSurfaceSubsetUndo("surface subset",
            sed::editor::DocumentDirty::LevelGeometry, { 0 });
        doc.level().sectors.front().surfaces.front().extraLight.g += 0.25f;

        std::vector<std::pair<std::string,
            sed::editor::DocumentHistoryTransitionDomain>> observed{};
        const bool visited = doc.visitRecoveryHistoryTransitions(
            [&observed](const sed::editor::DocumentRecoveryHistoryStack stack,
                const std::string_view label, const std::uint32_t,
                const sed::editor::DocumentHistoryTransitionDomain domains,
                sed::editor::DocumentState&)
            {
                if (stack != sed::editor::DocumentRecoveryHistoryStack::Undo)
                {
                    return false;
                }
                observed.emplace_back(std::string(label), domains);
                return true;
            });
        const auto domainFor = [&observed](const std::string_view label)
        {
            const auto found = std::find_if(observed.begin(), observed.end(),
                [label](const auto& item) { return item.first == label; });
            return found == observed.end()
                ? sed::editor::DocumentHistoryTransitionDomain::None
                : found->second;
        };
        using Domain = sed::editor::DocumentHistoryTransitionDomain;
        check(visited && observed.size() == 9
                && domainFor("full") == Domain::All
                && domainFor("things") == (Domain::Head | Domain::Things)
                && domainFor("thing subset")
                    == (Domain::Head | Domain::Things)
                && domainFor("thing tail")
                    == (Domain::Head | Domain::Things)
                && domainFor("cogs") == (Domain::Head | Domain::Cogs)
                && domainFor("lights")
                    == (Domain::Head | Domain::Sectors | Domain::Lights)
                && domainFor("things lights") == (Domain::Head
                    | Domain::Sectors | Domain::Things | Domain::Lights)
                && domainFor("sector subset")
                    == (Domain::Head | Domain::Sectors)
                && domainFor("surface subset")
                    == (Domain::Head | Domain::Sectors),
            "Document: recovery transition visitor derives every domain from the concrete Snapshot variant");

        std::vector<std::pair<std::string, std::vector<std::int32_t>>>
            observedSectorSubsets{};
        const bool detailedVisited = doc.visitRecoveryHistoryTransitions(
            [&observedSectorSubsets](
                const sed::editor::DocumentRecoveryHistoryStack stack,
                const std::string_view label, const std::uint32_t,
                const sed::editor::DocumentHistoryTransitionDomain,
                const sed::assets::SedHistoryTransitionToken& transition,
                sed::editor::DocumentState& state)
            {
                if (stack != sed::editor::DocumentRecoveryHistoryStack::Undo)
                    return false;
                if (!transition.matches(state.level))
                    return false;
                if (transition.sectorSubset())
                {
                    observedSectorSubsets.emplace_back(
                        std::string(label),
                        std::vector<std::int32_t>(
                            transition.sectorIndices().begin(),
                            transition.sectorIndices().end()));
                }
                return true;
            });
        check(detailedVisited && observedSectorSubsets.size() == 2
                && observedSectorSubsets[0].second
                    == std::vector<std::int32_t>{ 0 }
                && observedSectorSubsets[1].second
                    == std::vector<std::int32_t>{ 0 },
            "Document: detailed recovery transitions expose exact SectorSubset indices");
    }

    {
        // Native HIST import must construct compact Thing records directly
        // from the decoder's Level view. In particular, a large unchanged
        // geometry domain must never become a transient/retained FullSnapshot
        // merely to capture one changed Thing.
        sed::Level level{};
        level.clear();
        level.things.emplace_back();
        level.things.front().id = 1;
        level.things.front().num = 0;
        level.sectors.resize(64);
        for (sed::Sector& sector : level.sectors)
        {
            sector.vertices.resize(256);
        }
        sed::editor::Selection selection{};
        selection.things.insert(0);
        sed::editor::DocumentUndoHistory imported{};
        constexpr std::size_t importedEntries = 128;
        for (std::size_t index = 0; index < importedEntries; ++index)
        {
            level.things.front().x = static_cast<double>(index);
            imported.appendCompactRecoveryState(
                sed::editor::DocumentRecoveryHistoryStack::Undo,
                "Thing: Position changed",
                static_cast<std::uint32_t>(sed::editor::DocumentDirty::Things),
                level,
                selection,
                std::nullopt,
                std::vector<std::int32_t>{ 0 });
        }
        imported.finishCompactRecoveryImport();
        const sed::editor::DocumentUndoSnapshotStats stats =
            imported.snapshotStats();
        check(imported.undoDepth() == importedEntries
                && stats.full == 0
                && stats.thingSubsets == importedEntries
                && stats.things == 0,
            "Document: compact HIST Thing import retains zero full Levels across a long timeline");
    }

    {
        // The live editor intentionally retains at most 1,024 entries. Once
        // the cap is reached, discard only the state farthest
        // behind the cursor and retain exact undo/redo behavior for every
        // remaining compact record.
        sed::editor::Document doc{};
        sed::Thing& thing = doc.level().newThing();
        thing.x = 0.0;
        constexpr std::size_t extraEntries = 7;
        constexpr std::size_t authoredEntries =
            sed::editor::DocumentMaximumHistoryEntries + extraEntries;
        for (std::size_t index = 0; index < authoredEntries; ++index)
        {
            doc.pushThingSubsetUndo(
                "Move thing",
                sed::editor::DocumentDirty::Things,
                std::vector<std::int32_t>{0});
            doc.level().things.front().x = static_cast<double>(index + 1);
        }

        check(doc.undoDepth() == sed::editor::DocumentMaximumHistoryEntries
                && doc.redoDepth() == 0,
            "Document: live undo history is capped at 1,024 total entries");
        std::size_t undone{};
        while (doc.undo())
        {
            ++undone;
        }
        check(undone == sed::editor::DocumentMaximumHistoryEntries
                && doc.level().things.front().x
                    == static_cast<double>(extraEntries)
                && doc.redoDepth()
                    == sed::editor::DocumentMaximumHistoryEntries,
            "Document: history cap drops only the farthest undo states");
        std::size_t redone{};
        while (doc.redo())
        {
            ++redone;
        }
        check(redone == sed::editor::DocumentMaximumHistoryEntries
                && doc.level().things.front().x
                    == static_cast<double>(authoredEntries)
                && doc.undoDepth()
                    == sed::editor::DocumentMaximumHistoryEntries,
            "Document: capped history retains exact redo to the newest state");
    }
}

} // namespace sed::tests
