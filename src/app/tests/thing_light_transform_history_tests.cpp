#include "test_common.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/assets/sed.hpp"
#include "thing/thing_light_authoring_utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sed::app_tests
{
namespace
{

TplValue makeStringValue(const std::string& name, const std::string& value)
{
    TplValue out{};
    out.name = name;
    out.type = ValueType::str;
    out.stringValue = value;
    return out;
}

const TplValue* findValue(const Thing& thing, const std::string_view name)
{
    const auto it = std::find_if(
        thing.values.begin(), thing.values.end(),
        [name](const TplValue& value)
        {
            return text::iequalsAscii(value.name, name);
        });
    return it == thing.values.end() ? nullptr : &*it;
}

bool sameSelection(const editor::Selection& actual,
                   const editor::Selection& expected)
{
    const auto equalSet = [](const auto& left, const auto& right)
    {
        return left.size() == right.size()
            && std::all_of(
                left.begin(), left.end(),
                [&right](const auto& item)
                {
                    return right.contains(item);
                });
    };
    return equalSet(actual.sectors, expected.sectors)
        && equalSet(actual.surfaces, expected.surfaces)
        && equalSet(actual.surfaceVertices, expected.surfaceVertices)
        && equalSet(actual.things, expected.things)
        && equalSet(actual.frames, expected.frames)
        && equalSet(actual.lights, expected.lights)
        && equalSet(actual.cogs, expected.cogs);
}

editor::DocumentState fixture()
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::MOTS;
    state.level.header.version = 3;
    state.level.layers.emplace_back("Layer0");
    state.level.sectors.resize(1);
    state.level.sectors[0].layer = 0;

    MotsArchLightObject regularArch{};
    regularArch.wireIndex = 40;
    MotsArchLightMesh regularMesh{};
    regularMesh.wireIndex = 41;
    regularMesh.vertices.push_back(MotsArchLightVertex{ 0.1f, 0.2f, 0.3f, 0.4f });
    regularArch.meshes.push_back(std::move(regularMesh));

    MotsArchLightObject headArch{};
    headArch.wireIndex = 50;
    MotsArchLightMesh headMesh{};
    headMesh.wireIndex = 51;
    headMesh.vertices.push_back(MotsArchLightVertex{ 0.5f, 0.6f, 0.7f, 0.8f });
    headArch.meshes.push_back(std::move(headMesh));

    state.level.motsArchLighting.push_back(std::move(regularArch));
    state.level.motsArchLighting.push_back(std::move(headArch));

    Thing regular{};
    regular.name = "regular-light";
    regular.layer = 0;
    regular.sector = 0;
    regular.archLightingIndex = 0;
    regular.x = 10.0;
    regular.y = 20.0;
    regular.z = 30.0;
    regular.values.push_back(makeStringValue("light", "2"));

    Thing head{};
    head.name = "headlight";
    head.layer = 0;
    head.sector = 0;
    head.archLightingIndex = 1;
    head.x = 40.0;
    head.y = 50.0;
    head.z = 60.0;
    head.values.push_back(makeStringValue("type", "actor"));
    head.values.push_back(makeStringValue("typeflags", "0x4"));
    head.values.push_back(makeStringValue("lightintensity", "3"));
    head.values.push_back(makeStringValue("lightoffset", "1/2/3"));

    state.level.things.push_back(std::move(regular));
    state.level.things.push_back(std::move(head));
    return state;
}

} // namespace

void runThingLightTransformHistoryTests()
{
    struct TransformCase
    {
        render::LightGizmoKind kind;
        std::int32_t thingIndex;
        const char* label;
    };
    const TransformCase cases[] = {
        { render::LightGizmoKind::ThingRegular, 0, "regular Thing-light" },
        { render::LightGizmoKind::ThingHead, 1, "Headlight" },
    };

    for (const TransformCase testCase : cases)
    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        editor::DocumentState initial = fixture();
        initial.selection.things.insert(testCase.thingIndex);
        window.setDocumentStateForTest(std::move(initial));
        window.markCurrentDocumentAsSavedForTest();

        const editor::DocumentState before = window.documentStateForTest();
        const std::vector<MotsArchLightObject> archBefore =
            before.level.motsArchLighting;
        const auto& liveArchBefore =
            window.documentStateForTest().level.motsArchLighting;
        const MotsArchLightObject* const archStorage =
            liveArchBefore.data();
        const std::size_t archCapacity = liveArchBefore.capacity();
        const editor::DocumentUndoSnapshotStats statsBefore =
            window.undoSnapshotStatsForTest();
        GlView* const view = window.glViewForTest();

        check(view != nullptr,
            "Thing-light transform history fixture exposes the real viewport");
        if (!view)
        {
            continue;
        }

        view->thingLightTranslateRequested(
            static_cast<std::int32_t>(testCase.kind), 1.25, -2.5, 3.75);

        const editor::DocumentState committed = window.documentStateForTest();
        const Thing& beforeThing =
            before.level.things[static_cast<std::size_t>(testCase.thingIndex)];
        const Thing& committedThing = committed.level.things[
            static_cast<std::size_t>(testCase.thingIndex)];
        const TplValue* const beforeOffset = findValue(beforeThing, "lightoffset");
        const TplValue* const committedOffset =
            findValue(committedThing, "lightoffset");
        const auto parsedOffset = committedOffset
            ? thing::light_authoring::parseAuthoredOffset(*committedOffset)
            : std::nullopt;
        const bool exactHeadOffset = testCase.kind == render::LightGizmoKind::ThingHead
            && beforeOffset != nullptr
            && committedOffset != nullptr
            && parsedOffset.has_value()
            && parsedOffset->x == 2.25
            && parsedOffset->y == -0.5
            && parsedOffset->z == 6.75
            && committedOffset->asString() == "(2.25/-0.5/6.75)";
        const std::size_t unrelatedThingIndex =
            testCase.thingIndex == 0 ? 1u : 0u;
        Level expectedUnrelatedState = before.level;
        expectedUnrelatedState.things[static_cast<std::size_t>(
            testCase.thingIndex)] = committedThing;
        const bool unrelatedStateUnchanged = assets::sedLevelsEqual(
            committed.level, expectedUnrelatedState);
        const bool ownerChanged = testCase.kind == render::LightGizmoKind::ThingRegular
            ? committedThing.x == beforeThing.x + 1.25
                && committedThing.y == beforeThing.y - 2.5
                && committedThing.z == beforeThing.z + 3.75
            : findValue(beforeThing, "lightoffset") != nullptr
                && findValue(committedThing, "lightoffset") != nullptr
                && committedThing.x == beforeThing.x
                && committedThing.y == beforeThing.y
                && committedThing.z == beforeThing.z
                && exactHeadOffset;
        const auto& liveCommittedArch =
            window.documentStateForTest().level.motsArchLighting;
        const bool archUnchanged = liveCommittedArch == archBefore
            && liveCommittedArch.data() == archStorage
            && liveCommittedArch.capacity() == archCapacity;
        const editor::DocumentUndoSnapshotStats statsCommitted =
            window.undoSnapshotStatsForTest();
        check(ownerChanged,
            (std::string(testCase.label) + " transform changes only its authored owner")
                .c_str());
        check(unrelatedStateUnchanged
                && committed.level.things[unrelatedThingIndex].name
                    == before.level.things[unrelatedThingIndex].name,
            (std::string(testCase.label) + " transform leaves the unrelated Thing and level state unchanged")
                .c_str());
        check(archUnchanged,
            (std::string(testCase.label) + " transform leaves ARCHLIGHTING contents and storage unchanged")
                .c_str());
        check(window.undoDepthForTest() == 1
                && statsCommitted.thingSubsets == statsBefore.thingSubsets + 1
                && statsCommitted.things == statsBefore.things
                && statsCommitted.full == statsBefore.full,
            (std::string(testCase.label) + " transform stores one bounded Thing-subset history record")
                .c_str());
        check(window.hasUnsavedChangesForTest()
                && sameSelection(committed.selection, before.selection),
            (std::string(testCase.label) + " transform marks the document dirty without changing selection")
                .c_str());

        std::vector<std::int32_t> undoSubsetIndices{};
        check(window.undoForTest(&undoSubsetIndices)
                && undoSubsetIndices.size() == 1
                && undoSubsetIndices.front() == testCase.thingIndex
                && assets::sedLevelsEqual(window.documentStateForTest().level,
                                          before.level)
                && sameSelection(window.documentStateForTest().selection,
                                 before.selection)
                && !window.hasUnsavedChangesForTest()
                && window.documentStateForTest().level.motsArchLighting.data()
                    == archStorage
                && window.documentStateForTest().level.motsArchLighting.capacity()
                    == archCapacity
                && window.documentStateForTest().level.motsArchLighting == archBefore,
            (std::string(testCase.label) + " Undo reports the selected subset and restores exact clean state")
                .c_str());

        std::vector<std::int32_t> redoSubsetIndices{};
        check(window.redoForTest(&redoSubsetIndices)
                && redoSubsetIndices.size() == 1
                && redoSubsetIndices.front() == testCase.thingIndex
                && assets::sedLevelsEqual(window.documentStateForTest().level,
                                          committed.level)
                && sameSelection(window.documentStateForTest().selection,
                                 committed.selection)
                && window.hasUnsavedChangesForTest()
                && window.documentStateForTest().level.motsArchLighting.data()
                    == archStorage
                && window.documentStateForTest().level.motsArchLighting.capacity()
                    == archCapacity
                && window.documentStateForTest().level.motsArchLighting == archBefore,
            (std::string(testCase.label) + " Redo restores exact dirty state without touching ARCHLIGHTING")
                .c_str());
    }

    struct ScaleCase
    {
        render::LightGizmoKind kind;
        std::int32_t thingIndex;
        const char* field;
        double expected;
        const char* label;
    };
    const ScaleCase scaleCases[] = {
        { render::LightGizmoKind::ThingRegular, 0, "light", 4.0, "regular Thing-light" },
        { render::LightGizmoKind::ThingHead, 1, "lightintensity", 6.0, "Headlight" },
    };

    for (const ScaleCase testCase : scaleCases)
    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        editor::DocumentState initial = fixture();
        initial.selection.things.insert(testCase.thingIndex);
        window.setDocumentStateForTest(std::move(initial));
        window.markCurrentDocumentAsSavedForTest();

        const editor::DocumentState before = window.documentStateForTest();
        const auto& liveArchBefore =
            window.documentStateForTest().level.motsArchLighting;
        const MotsArchLightObject* const archStorage = liveArchBefore.data();
        const std::size_t archCapacity = liveArchBefore.capacity();
        const editor::DocumentUndoSnapshotStats statsBefore =
            window.undoSnapshotStatsForTest();
        GlView* const view = window.glViewForTest();
        check(view != nullptr,
            (std::string(testCase.label) + " range fixture exposes the real viewport")
                .c_str());
        if (!view)
        {
            continue;
        }

        view->thingLightRangeScaleRequested(
            static_cast<std::int32_t>(testCase.kind), 2.0);

        const editor::DocumentState committed = window.documentStateForTest();
        const Thing& committedThing = committed.level.things[
            static_cast<std::size_t>(testCase.thingIndex)];
        const TplValue* const committedValue = findValue(committedThing, testCase.field);
        std::optional<double> parsedValue{};
        if (committedValue)
        {
            parsedValue = thing::light_authoring::parseAuthoredScalar(*committedValue);
        }
        const auto& liveCommittedArch =
            window.documentStateForTest().level.motsArchLighting;
        const editor::DocumentUndoSnapshotStats statsCommitted =
            window.undoSnapshotStatsForTest();
        check(parsedValue.has_value() && *parsedValue == testCase.expected
                && liveCommittedArch.data() == archStorage
                && liveCommittedArch.capacity() == archCapacity
                && liveCommittedArch == before.level.motsArchLighting
                && assets::sedLevelsEqual(
                    committed.level,
                    [&before, &committed, testCase]
                    {
                        Level expected = before.level;
                        expected.things[static_cast<std::size_t>(testCase.thingIndex)] =
                            committed.level.things[static_cast<std::size_t>(testCase.thingIndex)];
                        return expected;
                    }()),
            (std::string(testCase.label) + " range scale changes only its authored owner")
                .c_str());
        check(window.undoDepthForTest() == 1
                && statsCommitted.thingSubsets == statsBefore.thingSubsets + 1
                && statsCommitted.things == statsBefore.things
                && statsCommitted.full == statsBefore.full
                && window.hasUnsavedChangesForTest(),
            (std::string(testCase.label) + " range scale stores one bounded Thing-subset history record")
                .c_str());

        std::vector<std::int32_t> undoSubsetIndices{};
        check(window.undoForTest(&undoSubsetIndices)
                && undoSubsetIndices.size() == 1
                && undoSubsetIndices.front() == testCase.thingIndex
                && assets::sedLevelsEqual(window.documentStateForTest().level, before.level)
                && !window.hasUnsavedChangesForTest()
                && window.documentStateForTest().level.motsArchLighting.data() == archStorage
                && window.documentStateForTest().level.motsArchLighting.capacity() == archCapacity,
            (std::string(testCase.label) + " range scale Undo restores exact clean state")
                .c_str());

        std::vector<std::int32_t> redoSubsetIndices{};
        check(window.redoForTest(&redoSubsetIndices)
                && redoSubsetIndices.size() == 1
                && redoSubsetIndices.front() == testCase.thingIndex
                && assets::sedLevelsEqual(window.documentStateForTest().level, committed.level)
                && window.hasUnsavedChangesForTest()
                && window.documentStateForTest().level.motsArchLighting.data() == archStorage
                && window.documentStateForTest().level.motsArchLighting.capacity() == archCapacity,
            (std::string(testCase.label) + " range scale Redo restores exact dirty state")
                .c_str());
    }

    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        editor::DocumentState initial = fixture();
        initial.selection.things.insert(-1);
        window.setDocumentStateForTest(std::move(initial));
        window.markCurrentDocumentAsSavedForTest();
        const editor::DocumentState before = window.documentStateForTest();
        const auto& liveArch = window.documentStateForTest().level.motsArchLighting;
        const MotsArchLightObject* const archStorage = liveArch.data();
        GlView* const view = window.glViewForTest();
        if (view)
        {
            view->thingLightTranslateRequested(
                static_cast<std::int32_t>(render::LightGizmoKind::ThingRegular),
                1.0, 2.0, 3.0);
        }
        check(view != nullptr
                && window.undoDepthForTest() == 0
                && !window.hasUnsavedChangesForTest()
                && assets::sedLevelsEqual(window.documentStateForTest().level, before.level)
                && window.documentStateForTest().level.motsArchLighting.data() == archStorage,
            "invalid Thing-light selection creates no history or mutation");
    }

    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        editor::DocumentState initial = fixture();
        initial.selection.things.insert(0);
        window.setDocumentStateForTest(std::move(initial));
        window.markCurrentDocumentAsSavedForTest();
        const editor::DocumentState before = window.documentStateForTest();
        GlView* const view = window.glViewForTest();
        if (view)
        {
            view->thingLightTranslateRequested(
                static_cast<std::int32_t>(render::LightGizmoKind::ThingRegular),
                0.0, 0.0, 0.0);
        }
        check(view != nullptr
                && window.undoDepthForTest() == 0
                && !window.hasUnsavedChangesForTest()
                && assets::sedLevelsEqual(window.documentStateForTest().level, before.level),
            "zero Thing-light translation creates no history or mutation");
    }

    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        editor::DocumentState initial = fixture();
        initial.selection.things.insert(0);
        window.setDocumentStateForTest(std::move(initial));
        window.setLayerVisualStateForTest({ true }, { true });
        window.markCurrentDocumentAsSavedForTest();
        const editor::DocumentState before = window.documentStateForTest();
        GlView* const view = window.glViewForTest();
        if (view)
        {
            view->thingLightTranslateRequested(
                static_cast<std::int32_t>(render::LightGizmoKind::ThingRegular),
                1.0, 2.0, 3.0);
        }
        check(view != nullptr
                && window.undoDepthForTest() == 0
                && !window.hasUnsavedChangesForTest()
                && assets::sedLevelsEqual(window.documentStateForTest().level, before.level),
            "locked Thing-light selection creates no history or mutation");
    }

    for (const TransformCase testCase : cases)
    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        editor::DocumentState initial = fixture();
        initial.selection.things.insert(0);
        initial.selection.things.insert(1);
        window.setDocumentStateForTest(std::move(initial));
        window.markCurrentDocumentAsSavedForTest();
        const editor::DocumentState before = window.documentStateForTest();
        const auto& liveArch = window.documentStateForTest().level.motsArchLighting;
        const MotsArchLightObject* const archStorage = liveArch.data();
        const std::size_t archCapacity = liveArch.capacity();
        const editor::DocumentUndoSnapshotStats statsBefore =
            window.undoSnapshotStatsForTest();
        GlView* const view = window.glViewForTest();
        if (view)
        {
            view->thingLightTranslateRequested(
                static_cast<std::int32_t>(testCase.kind), 1.0, 2.0, 3.0);
        }
        const editor::DocumentState committed = window.documentStateForTest();
        const editor::DocumentUndoSnapshotStats statsCommitted =
            window.undoSnapshotStatsForTest();
        const Thing& beforeUnrelated = before.level.things[
            static_cast<std::size_t>(testCase.thingIndex == 0 ? 1 : 0)];
        const Thing& committedUnrelated = committed.level.things[
            static_cast<std::size_t>(testCase.thingIndex == 0 ? 1 : 0)];
        check(view != nullptr
                && window.undoDepthForTest() == 1
                && statsCommitted.thingSubsets == statsBefore.thingSubsets + 1
                && statsCommitted.things == statsBefore.things
                && statsCommitted.full == statsBefore.full
                && assets::sedLevelsEqual(
                    committed.level,
                    [&before, &committed, testCase]
                    {
                        Level expected = before.level;
                        expected.things[static_cast<std::size_t>(testCase.thingIndex)] =
                            committed.level.things[static_cast<std::size_t>(testCase.thingIndex)];
                        return expected;
                    }())
                && committedUnrelated.x == beforeUnrelated.x
                && committedUnrelated.y == beforeUnrelated.y
                && committedUnrelated.z == beforeUnrelated.z
                && window.documentStateForTest().level.motsArchLighting.data() == archStorage
                && window.documentStateForTest().level.motsArchLighting.capacity() == archCapacity,
            (std::string(testCase.label) + " mixed selection snapshots only the matching light owner")
                .c_str());
    }
}

} // namespace sed::app_tests
