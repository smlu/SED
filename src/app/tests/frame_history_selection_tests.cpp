#include "test_common.hpp"

#include <QAction>
#include <QCborMap>
#include <QCborValue>
#include <QLineEdit>
#include <QMetaObject>
#include <QTemporaryDir>
#include <QToolButton>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "main_window/document/native_save_edit_utils.hpp"
#include "sed/assets/sed.hpp"
#include "sed/editor/document.hpp"
#include "sed/level/thing.hpp"
#include "ui/recovery/document_recovery_utils.hpp"

namespace sed::app_tests
{
namespace
{

TplValue stringParameter(const std::string& name, const std::string& value)
{
    TplValue parameter{};
    parameter.name = name;
    parameter.type = ValueType::str;
    parameter.stringValue = value;
    return parameter;
}

TplValue integerParameter(const std::string& name, const std::int32_t value)
{
    TplValue parameter{};
    parameter.name = name;
    parameter.type = ValueType::integer;
    parameter.intValue = value;
    return parameter;
}

TplValue frameParameter(
    const double x,
    const double y,
    const double z,
    const double pitch,
    const double yaw,
    const double roll)
{
    TplValue frame{};
    frame.name = "frame";
    frame.type = ValueType::frame;
    frame.adjType = AdjType::frame;
    frame.setFrame(x, y, z, pitch, yaw, roll);
    return frame;
}

editor::DocumentState frameState(std::vector<TplValue> values)
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.layers.emplace_back("Layer0");

    Thing thing{};
    thing.name = "+history_path_thing";
    thing.templateName = "+history_path_thing";
    thing.layer = 0;
    thing.x = 1.25;
    thing.y = -2.5;
    thing.z = 3.75;
    thing.yaw = 37.0;
    thing.values = std::move(values);
    state.level.things.push_back(std::move(thing));
    state.selection.things.insert(0);
    return state;
}

std::optional<std::int32_t> frameValueAtOrdinal(
    const Thing& thing,
    const std::size_t ordinal)
{
    std::size_t found = 0;
    for (std::size_t index = 0; index < thing.values.size(); ++index)
    {
        const TplValue& value = thing.values[index];
        if (value.type != ValueType::frame || value.name != "frame")
        {
            continue;
        }
        if (found == ordinal)
        {
            return static_cast<std::int32_t>(index);
        }
        ++found;
    }
    return std::nullopt;
}

std::optional<std::int32_t> thingNumFramesValue(const Thing& thing)
{
    for ( const TplValue& value : thing.values )
    {
        if ( value.type == ValueType::integer && value.name == "numframes" )
        {
            return value.intValue;
        }
    }
    return std::nullopt;
}

bool selectedFrameIsValue(
    const editor::Selection& selection,
    const Thing& thing,
    const std::int32_t valueIndex)
{
    if (valueIndex < 0
        || static_cast<std::size_t>(valueIndex) >= thing.values.size()
        || thing.values[static_cast<std::size_t>(valueIndex)].type != ValueType::frame
        || thing.values[static_cast<std::size_t>(valueIndex)].name != "frame")
    {
        return false;
    }
    return selection.frames.size() == 1u
        && selection.frames.contains(level::FrameRef{ 0, valueIndex });
}

bool invokeFramePointAdd(GlView* view, const QVector3D& point)
{
    return view && QMetaObject::invokeMethod(
        view,
        "surfaceClicked",
        Qt::DirectConnection,
        Q_ARG(QVector3D, point),
        Q_ARG(std::int32_t, -1),
        Q_ARG(std::int32_t, -1),
        Q_ARG(QPoint, QPoint(420, 330)),
        Q_ARG(Qt::KeyboardModifiers, Qt::NoModifier));
}

QByteArray bytesToArray(const std::vector<std::byte>& bytes)
{
    if (bytes.empty())
    {
        return {};
    }
    return QByteArray(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<qsizetype>(bytes.size()));
}

assets::SedOptionalSection optionalSection(
    const std::array<char, 4> type,
    const QByteArray& bytes)
{
    assets::SedOptionalSection section{};
    section.type = type;
    if (!bytes.isEmpty())
    {
        const auto* first = reinterpret_cast<const std::byte*>(bytes.constData());
        section.payload.assign(first, first + bytes.size());
    }
    return section;
}

QByteArray optionalPayload(const assets::SedFile& file, const std::array<char, 4> type)
{
    for (const assets::SedOptionalSection& section : file.optionalSections)
    {
        if (section.type == type)
        {
            return bytesToArray(section.payload);
        }
    }
    return {};
}

bool selectionEqual(const editor::Selection& lhs, const editor::Selection& rhs)
{
    return ui::recovery::serializeSelection(lhs)
        == ui::recovery::serializeSelection(rhs);
}

void printThingHistoryLevelDiff(
    const char* label,
    const Level& expected,
    const Level& actual)
{
    std::fprintf(stderr,
        "%s Level counts: things=%zu/%zu sectors=%zu/%zu cogs=%zu/%zu templates=%zu/%zu lights=%zu/%zu layers=%zu/%zu kind=%d/%d\n",
        label, expected.things.size(), actual.things.size(),
        expected.sectors.size(), actual.sectors.size(),
        expected.cogs.size(), actual.cogs.size(),
        expected.templates.size(), actual.templates.size(),
        expected.lights.size(), actual.lights.size(),
        expected.layers.size(), actual.layers.size(),
        static_cast<int>(expected.kind), static_cast<int>(actual.kind));
    const std::size_t thingCount = std::max(
        expected.things.size(), actual.things.size());
    for ( std::size_t thingIndex = 0; thingIndex < thingCount; ++thingIndex )
    {
        if ( thingIndex >= expected.things.size()
            || thingIndex >= actual.things.size() )
        {
            std::fprintf(stderr,
                "%s thing[%zu]: expectedPresent=%d actualPresent=%d\n",
                label, thingIndex,
                thingIndex < expected.things.size(),
                thingIndex < actual.things.size());
            continue;
        }
        const Thing& expectedThing = expected.things[thingIndex];
        const Thing& actualThing = actual.things[thingIndex];
        std::fprintf(stderr,
            "%s thing[%zu] expected{id=%d num=%d sector=%d arch=%d name='%s' template='%s' pos=%.17g,%.17g,%.17g angles=%.17g,%.17g,%.17g layer=%d bbox=%.17g,%.17g,%.17g,%.17g,%.17g,%.17g values=%zu} actual{id=%d num=%d sector=%d arch=%d name='%s' template='%s' pos=%.17g,%.17g,%.17g angles=%.17g,%.17g,%.17g layer=%d bbox=%.17g,%.17g,%.17g,%.17g,%.17g,%.17g values=%zu}\n",
            label, thingIndex,
            expectedThing.id, expectedThing.num, expectedThing.sector,
            expectedThing.archLightingIndex, expectedThing.name.c_str(),
            expectedThing.templateName.c_str(), expectedThing.x,
            expectedThing.y, expectedThing.z, expectedThing.pch,
            expectedThing.yaw, expectedThing.rol, expectedThing.layer,
            expectedThing.bbox.x1, expectedThing.bbox.y1,
            expectedThing.bbox.z1, expectedThing.bbox.x2,
            expectedThing.bbox.y2, expectedThing.bbox.z2,
            expectedThing.values.size(),
            actualThing.id, actualThing.num, actualThing.sector,
            actualThing.archLightingIndex, actualThing.name.c_str(),
            actualThing.templateName.c_str(), actualThing.x,
            actualThing.y, actualThing.z, actualThing.pch,
            actualThing.yaw, actualThing.rol, actualThing.layer,
            actualThing.bbox.x1, actualThing.bbox.y1,
            actualThing.bbox.z1, actualThing.bbox.x2,
            actualThing.bbox.y2, actualThing.bbox.z2,
            actualThing.values.size());

        const std::size_t valueCount = std::max(
            expectedThing.values.size(), actualThing.values.size());
        for ( std::size_t valueIndex = 0; valueIndex < valueCount; ++valueIndex )
        {
            const TplValue* const expectedValue =
                valueIndex < expectedThing.values.size()
                    ? &expectedThing.values[valueIndex]
                    : nullptr;
            const TplValue* const actualValue =
                valueIndex < actualThing.values.size()
                    ? &actualThing.values[valueIndex]
                    : nullptr;
            const std::string expectedText = expectedValue
                ? expectedValue->asString()
                : std::string{};
            const std::string actualText = actualValue
                ? actualValue->asString()
                : std::string{};
            std::fprintf(stderr,
                "%s thing[%zu].value[%zu] expected{present=%d name='%s' type=%d int=%d float=%.17g text='%s' adj=%d} actual{present=%d name='%s' type=%d int=%d float=%.17g text='%s' adj=%d}\n",
                label, thingIndex, valueIndex,
                expectedValue != nullptr,
                expectedValue ? expectedValue->name.c_str() : "",
                expectedValue ? static_cast<int>(expectedValue->type) : -1,
                expectedValue ? expectedValue->intValue : 0,
                expectedValue ? expectedValue->floatValue : 0.0,
                expectedText.c_str(),
                expectedValue ? static_cast<int>(expectedValue->adjType) : -1,
                actualValue != nullptr,
                actualValue ? actualValue->name.c_str() : "",
                actualValue ? static_cast<int>(actualValue->type) : -1,
                actualValue ? actualValue->intValue : 0,
                actualValue ? actualValue->floatValue : 0.0,
                actualText.c_str(),
                actualValue ? static_cast<int>(actualValue->adjType) : -1);
        }
    }
}

void printFrameHistorySelection(
    const char* label,
    const editor::Selection& selection,
    const Level& level)
{
    std::fprintf(stderr,
        "%s selection: Things=%zu FrameRefs=%zu\n",
        label, selection.things.size(), selection.frames.size());
    for ( const level::FrameRef ref : selection.frames )
    {
        const Thing* thing = ref.thing >= 0
                && static_cast<std::size_t>(ref.thing) < level.things.size()
            ? &level.things[static_cast<std::size_t>(ref.thing)]
            : nullptr;
        const TplValue* value = thing && ref.value >= 0
                && static_cast<std::size_t>(ref.value) < thing->values.size()
            ? &thing->values[static_cast<std::size_t>(ref.value)]
            : nullptr;
        std::fprintf(stderr,
            "%s FrameRef{thing=%d value=%d} valid=%d name='%s' type=%d\n",
            label, ref.thing, ref.value, value != nullptr,
            value ? value->name.c_str() : "",
            value ? static_cast<int>(value->type) : -1);
    }
}

void checkSemanticAddSelectionForValues(
    const std::vector<TplValue>& values,
    const std::size_t initialFrameCount,
    const char* message)
{
    auto window = std::make_unique<MainWindow>();
    window->setAttribute(Qt::WA_DontShowOnScreen);
    window->resize(1000, 700);
    window->activateFrameEditModeForTest();
    window->setDocumentStateForTest(frameState(values));
    window->updateEditActionsForTest();

    GlView* view = window->glViewForTest();
    if (view)
    {
        view->resize(1000, 700);
        view->setCameraProjectionType(
            app::ViewportCamera::ProjectionType::Perspective);
        view->setOrbitViewImmediate(25.0f, -18.0f, 0.0f);
        view->setCameraOrbitDistance(8.0f);
    }

    QAction* addAction = window->findChild<QAction*>(
        QStringLiteral("viewport.frame.add"));
    if (addAction)
    {
        addAction->setEnabled(true);
        addAction->trigger();
    }

    const bool addInvoked = invokeFramePointAdd(view, QVector3D(6.0f, 7.0f, 8.0f));
    const editor::DocumentState& result = window->documentStateForTest();
    const Thing* thing = result.level.things.empty()
        ? nullptr
        : &result.level.things.front();
    const std::optional<std::int32_t> selectedOrdinal = thing
        ? frameValueAtOrdinal(*thing, initialFrameCount)
        : std::nullopt;
    check(addAction && addAction->isChecked() && addInvoked && thing
            && sed::level::thingCountFrames(*thing) == initialFrameCount + 1u
            && selectedOrdinal
            && selectedFrameIsValue(result.selection, *thing, *selectedOrdinal)
            && view && view->activeFrameValueIndex() == *selectedOrdinal,
        message);
}

bool frameSelectionNamesThings(
    const editor::DocumentState& state,
    const std::int32_t expectedValueIndex,
    const std::size_t expectedThingCount)
{
    if (state.selection.frames.size() != expectedThingCount)
    {
        return false;
    }
    for (std::size_t thingIndex = 0; thingIndex < expectedThingCount; ++thingIndex)
    {
        const level::FrameRef ref{
            static_cast<std::int32_t>(thingIndex), expectedValueIndex };
        if (!state.selection.frames.contains(ref)
            || thingIndex >= state.level.things.size()
            || expectedValueIndex < 0
            || static_cast<std::size_t>(expectedValueIndex)
                >= state.level.things[thingIndex].values.size())
        {
            return false;
        }
        const TplValue& value = state.level.things[thingIndex].values[
            static_cast<std::size_t>(expectedValueIndex)];
        if (value.type != ValueType::frame || value.name != "frame")
        {
            return false;
        }
    }
    return true;
}

void checkLastFrameDeleteUndoRedoWithEmptyCurrentFrameRefs()
{
    for ( const bool framesOnly : std::array<bool, 2>{ false, true } )
    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.activateFrameEditModeForTest();
        editor::DocumentState state = frameState({
            frameParameter(1.0, 2.0, 3.0, 4.0, 5.0, 6.0),
            integerParameter("numframes", 1),
        });
        if ( framesOnly )
        {
            state.selection.things.clear();
        }
        else
        {
            Thing secondThing = state.level.things.front();
            secondThing.name = "+history_path_thing_b";
            secondThing.templateName = "+history_path_thing_b";
            state.level.things.push_back(std::move(secondThing));
            state.selection.things.insert(1);
        }
        state.selection.frames.insert({ 0, 0 });
        window.setDocumentStateForTest(std::move(state));
        window.updateEditActionsForTest();

        QAction* const deleteAction = window.findChild<QAction*>(
            QStringLiteral("edit.delete"));
        if ( deleteAction )
        {
            deleteAction->trigger();
        }
        const editor::DocumentState& afterDelete = window.documentStateForTest();
        const bool frameWasDeleted = deleteAction
            && afterDelete.level.things.size() == (framesOnly ? 1u : 2u)
            && sed::level::thingCountFrames(afterDelete.level.things.front()) == 0u
            && afterDelete.selection.frames.empty();
        check(frameWasDeleted,
            framesOnly
                ? "Frame Delete removes the last frame from a frame-only selection"
                : "Frame Delete removes the last selected frame with multiple selected Things");

        QAction* const undoAction = window.findChild<QAction*>(
            QStringLiteral("edit.undo"));
        if ( undoAction )
        {
            undoAction->trigger();
        }
        const editor::DocumentState& afterUndo = window.documentStateForTest();
        const std::optional<std::int32_t> restoredFrame =
            afterUndo.level.things.empty()
                ? std::nullopt
                : frameValueAtOrdinal(afterUndo.level.things.front(), 0u);
        const bool thingSelectionRestored = framesOnly
            ? afterUndo.selection.things.empty()
            : afterUndo.selection.things.size() == 2u
                && afterUndo.selection.things.contains(0)
                && afterUndo.selection.things.contains(1);
        const bool undoRestoredFrameSelection = undoAction
            && restoredFrame
            && sed::level::thingCountFrames(afterUndo.level.things.front()) == 1u
            && afterUndo.selection.frames.size() == 1u
            && afterUndo.selection.frames.contains(
                level::FrameRef{ 0, *restoredFrame })
            && thingSelectionRestored;
        check(undoRestoredFrameSelection,
            framesOnly
                ? "Undo restores the last frame ref after a frame-only selection was emptied"
                : "Undo restores the last frame ref after multi-Thing selection was emptied");

        QAction* const redoAction = window.findChild<QAction*>(
            QStringLiteral("edit.redo"));
        if ( redoAction )
        {
            redoAction->trigger();
        }
        const editor::DocumentState& afterRedo = window.documentStateForTest();
        const bool redoRestoredEmptySelection = redoAction
            && afterRedo.level.things.size() == (framesOnly ? 1u : 2u)
            && sed::level::thingCountFrames(afterRedo.level.things.front()) == 0u
            && afterRedo.selection.frames.empty()
            && (framesOnly
                    ? afterRedo.selection.things.empty()
                    : afterRedo.selection.things.size() == 2u
                        && afterRedo.selection.things.contains(0)
                        && afterRedo.selection.things.contains(1));
        check(redoRestoredEmptySelection,
            framesOnly
                ? "Redo restores the frame-only deletion state"
                : "Redo restores the multi-Thing frame deletion state");
    }
}

QLineEdit* findFrameValueEdit(
    MainWindow& window,
    const std::int32_t thingIndex,
    const std::int32_t valueIndex)
{
    for ( QLineEdit* edit : window.findChildren<QLineEdit*>() )
    {
        bool thingIndexValid = false;
        bool valueIndexValid = false;
        const int editThingIndex = edit->property(
            "sed.frameThingIndex").toInt(&thingIndexValid);
        const int editValueIndex = edit->property(
            "sed.frameValueIndex").toInt(&valueIndexValid);
        if ( thingIndexValid && valueIndexValid
            && editThingIndex == thingIndex
            && editValueIndex == valueIndex )
        {
            return edit;
        }
    }
    return nullptr;
}

void checkStableFrameUndoKeepsCurrentDeselection()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.activateFrameEditModeForTest();
    editor::DocumentState state = frameState({
        frameParameter(1.0, 2.0, 3.0, 4.0, 5.0, 6.0),
        integerParameter("numframes", 1),
    });
    state.selection.frames.insert({ 0, 0 });
    const std::string originalFrame = state.level.things.front()
        .values.front().asString();
    window.setDocumentStateForTest(std::move(state));
    window.updateEditActionsForTest();

    QLineEdit* const frameEdit = findFrameValueEdit(window, 0, 0);
    bool poseEditInvoked = false;
    if ( frameEdit )
    {
        frameEdit->setText(QStringLiteral("(11/12/13:14/15/16)"));
        poseEditInvoked = QMetaObject::invokeMethod(
            frameEdit, "editingFinished", Qt::DirectConnection);
    }
    const editor::DocumentState& afterPoseEdit = window.documentStateForTest();
    const bool poseEditApplied = poseEditInvoked
        && afterPoseEdit.level.things.front().values.front().asString()
            != originalFrame;

    window.mutableDocumentStateForTest().selection.frames.clear();
    QAction* const undoAction = window.findChild<QAction*>(
        QStringLiteral("edit.undo"));
    if ( undoAction )
    {
        undoAction->trigger();
    }
    const editor::DocumentState& afterUndo = window.documentStateForTest();
    const bool poseUndoRestored = afterUndo.level.things.size() == 1u
        && afterUndo.level.things.front().values.front().asString()
            == originalFrame;
    check(poseEditApplied && undoAction && poseUndoRestored
            && afterUndo.selection.frames.empty(),
        "Undo of a stable frame pose edit retains the user's current frame deselection");
}

void checkThingParameterRemovalKeepsFrameSelections()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    editor::DocumentState state = frameState({
        stringParameter("move", "path"),
        stringParameter("description", "before frame"),
        frameParameter(1.0, 2.0, 3.0, 4.0, 5.0, 6.0),
        integerParameter("numframes", 1),
    });
    Thing secondThing = state.level.things.front();
    secondThing.name = "+history_path_thing_b";
    secondThing.templateName = "+history_path_thing_b";
    secondThing.x = 9.0;
    state.level.things.push_back(std::move(secondThing));
    state.selection.things.insert(1);
    state.selection.frames.insert({ 0, 2 });
    state.selection.frames.insert({ 1, 2 });
    window.setDocumentStateForTest(std::move(state));

    QToolButton* const removeDescription = window.findChild<QToolButton*>(
        QStringLiteral("prop.thing.value.remove.description"));
    if (removeDescription)
    {
        removeDescription->click();
    }
    const editor::DocumentState removed = window.documentStateForTest();
    const bool removedBoth = removed.level.things.size() == 2u
        && std::none_of(removed.level.things[0].values.begin(),
            removed.level.things[0].values.end(),
            [](const TplValue& value) { return value.name == "description"; })
        && std::none_of(removed.level.things[1].values.begin(),
            removed.level.things[1].values.end(),
            [](const TplValue& value) { return value.name == "description"; });
    check(removeDescription && removedBoth
            && frameSelectionNamesThings(removed, 1, 2),
        "Removing a parameter before selected frames keeps both references on their frame values");

    const bool undoApplied = window.undoForTest();
    const editor::DocumentState undone = window.documentStateForTest();
    check(undoApplied && frameSelectionNamesThings(undone, 2, 2),
        "Undo of Thing parameter removal restores both original selected frame references");

    const bool redoApplied = window.redoForTest();
    const editor::DocumentState redone = window.documentStateForTest();
    check(redoApplied && frameSelectionNamesThings(redone, 1, 2),
        "Redo of Thing parameter removal keeps both selected references on frame values");
}

} // namespace

void runFrameHistorySelectionTests()
{
    checkLastFrameDeleteUndoRedoWithEmptyCurrentFrameRefs();
    checkStableFrameUndoKeepsCurrentDeselection();
    checkThingParameterRemovalKeepsFrameSelections();

    // The Frame Add toggle initializes the first point at the selected Thing's
    // pose. With unrelated fields preceding it, synchronization inserts
    // numframes into the vector and changes the frame's raw value index.
    auto window = std::make_unique<MainWindow>();
    window->setAttribute(Qt::WA_DontShowOnScreen);
    window->resize(1000, 700);
    window->activateFrameEditModeForTest();
    editor::DocumentState state = frameState({
        stringParameter("move", "path"),
        stringParameter("description", "interleaved parameter"),
    });
    window->setDocumentStateForTest(std::move(state));
    const editor::DocumentState noFrameState = window->documentStateForTest();
    window->updateEditActionsForTest();

    GlView* view = window->glViewForTest();
    if (view)
    {
        view->resize(1000, 700);
        view->setCameraProjectionType(
            app::ViewportCamera::ProjectionType::Perspective);
        view->setOrbitViewImmediate(25.0f, -18.0f, 0.0f);
        view->setCameraOrbitDistance(8.0f);
    }

    QAction* addAction = window->findChild<QAction*>(
        QStringLiteral("viewport.frame.add"));
    if (addAction)
    {
        addAction->setEnabled(true);
        addAction->trigger();
    }

    const editor::DocumentState initialized = window->documentStateForTest();
    const Thing* initializedThing = initialized.level.things.empty()
        ? nullptr
        : &initialized.level.things.front();
    const std::optional<std::int32_t> firstFrame = initializedThing
        ? frameValueAtOrdinal(*initializedThing, 0u)
        : std::nullopt;
    check(addAction && addAction->isChecked() && initializedThing && firstFrame
            && sed::level::thingCountFrames(*initializedThing) == 1u
            && selectedFrameIsValue(
                initialized.selection, *initializedThing, *firstFrame)
            && view && view->activeFrameValueIndex() == *firstFrame,
        "Frame Add selects the actual first frame value after numframes insertion");

    // Invoke the same production surface-click signal that the viewport emits
    // for the next empty-space point, then verify both current and History
    // selections still name frame-typed values.
    const bool secondPointAdded = invokeFramePointAdd(
        view, QVector3D(6.0f, 7.0f, 8.0f));
    const editor::DocumentState afterSecondPoint = window->documentStateForTest();
    const Thing* afterThing = afterSecondPoint.level.things.empty()
        ? nullptr
        : &afterSecondPoint.level.things.front();
    const std::optional<std::int32_t> secondFrame = afterThing
        ? frameValueAtOrdinal(*afterThing, 1u)
        : std::nullopt;
    check(secondPointAdded && afterThing
            && sed::level::thingCountFrames(*afterThing) == 2u
            && secondFrame
            && selectedFrameIsValue(
                afterSecondPoint.selection, *afterThing, *secondFrame)
            && view && view->activeFrameValueIndex() == *secondFrame,
        "Viewport Frame Add point selects its frame-typed raw value after synchronization");

    QAction* undo = window->findChild<QAction*>(QStringLiteral("edit.undo"));
    if (undo)
    {
        undo->trigger();
    }
    const editor::DocumentState& afterUndo = window->documentStateForTest();
    const Thing* undoneThing = afterUndo.level.things.empty()
        ? nullptr
        : &afterUndo.level.things.front();
    const std::optional<std::int32_t> undoFrame = undoneThing
        ? frameValueAtOrdinal(*undoneThing, 0u)
        : std::nullopt;
    check(undo && undoneThing && undoFrame
            && sed::level::thingCountFrames(*undoneThing) == 1u
            && selectedFrameIsValue(afterUndo.selection, *undoneThing, *undoFrame),
        "Undo of the subsequent point restores a selection that names the first frame value");

    QAction* redo = window->findChild<QAction*>(QStringLiteral("edit.redo"));
    if (redo)
    {
        redo->trigger();
    }
    const editor::DocumentState& afterRedo = window->documentStateForTest();
    const Thing* redoneThing = afterRedo.level.things.empty()
        ? nullptr
        : &afterRedo.level.things.front();
    const std::optional<std::int32_t> redoFrame = redoneThing
        ? frameValueAtOrdinal(*redoneThing, 1u)
        : std::nullopt;
    check(redo && redoneThing && redoFrame
            && sed::level::thingCountFrames(*redoneThing) == 2u
            && selectedFrameIsValue(afterRedo.selection, *redoneThing, *redoFrame),
        "Redo restores the subsequent frame point's exact frame-typed selection");

    // Leave both Undo and Redo populated so the real UI-authored frame history
    // exercises the production recovery serializer/reader in both directions.
    if (undo)
    {
        undo->trigger();
    }
    const editor::DocumentState recoveryCurrent = window->documentStateForTest();
    QTemporaryDir recoveryDirectory{};
    const std::filesystem::path recoveryRoot(
        recoveryDirectory.path().toStdWString());
    window->setRecoveryDirectoryForTest(recoveryRoot);
    window->beginRecoveryDocumentForTest(
        recoveryRoot / "frame-history.sed", true);
    window->flushRecoverySnapshotForTest(true);
    window->waitForRecoverySnapshotForTest();
    const auto recoveryRecord = ui::recovery::readRecoveryRecord(
        window->currentRecoveryPathForTest());
    QString recoveryError{};
    const auto recovered = recoveryRecord
        ? ui::recovery::loadRecoveredDocument(*recoveryRecord, &recoveryError)
        : std::optional<ui::recovery::RecoveredDocument>{};
    const QByteArray recoveredEdit = recovered
        ? recovered->editorStatePayload
        : QByteArray{};
    const QCborMap recoveredEditMap = QCborValue::fromCbor(recoveredEdit).toMap();
    QString recoveredSelectionError{};
    const auto recoveredSelection = ui::recovery::deserializeSelection(
        recoveredEditMap.value(QStringLiteral("selection")).toByteArray(),
        &recoveredSelectionError);
    editor::Document recoveredFrameHistory{};
    bool recoveredUndoMatches = false;
    bool recoveredRedoMatches = false;
    bool recoveredUndoApplied = false;
    bool recoveredUndoLevelMatches = false;
    bool recoveredUndoSelectionMatches = false;
    bool recoveredRedoApplied = false;
    bool recoveredRedoLevelMatches = false;
    bool recoveredRedoSelectionMatches = false;
    bool recoveredFutureRedoApplied = false;
    bool recoveredFutureRedoLevelMatches = false;
    bool recoveredFutureRedoSelectionMatches = false;
    bool recoveredHistoryShape = false;
    if (recovered && recovered->history)
    {
        editor::DocumentState recoveredCurrent = recovered->current;
        if ( recoveredSelection )
        {
            recoveredCurrent.selection = *recoveredSelection;
        }
        recoveredFrameHistory.restoreRecoveryHistory(
            std::move(recoveredCurrent), *recovered->history);
        recoveredHistoryShape = recoveredFrameHistory.undoDepth() == 1u
            && recoveredFrameHistory.redoDepth() == 1u;
        recoveredUndoApplied = recoveredFrameHistory.undo();
        recoveredUndoLevelMatches = recoveredUndoApplied
            && assets::sedLevelsEqual(
                recoveredFrameHistory.current().level, noFrameState.level);
        recoveredUndoSelectionMatches = recoveredUndoApplied
            && selectionEqual(
                recoveredFrameHistory.current().selection,
                noFrameState.selection);
        recoveredUndoMatches = recoveredUndoLevelMatches
            && recoveredUndoSelectionMatches;
        recoveredRedoApplied = recoveredUndoApplied
            && recoveredFrameHistory.redo();
        recoveredRedoLevelMatches = recoveredRedoApplied
            && assets::sedLevelsEqual(
                recoveredFrameHistory.current().level, initialized.level);
        recoveredRedoSelectionMatches = recoveredRedoApplied
            && selectionEqual(
                recoveredFrameHistory.current().selection,
                initialized.selection);
        recoveredRedoMatches = recoveredRedoLevelMatches
            && recoveredRedoSelectionMatches;
        recoveredFutureRedoApplied = recoveredRedoApplied
            && recoveredFrameHistory.redo();
        recoveredFutureRedoLevelMatches = recoveredFutureRedoApplied
            && assets::sedLevelsEqual(
                recoveredFrameHistory.current().level,
                afterSecondPoint.level);
        recoveredFutureRedoSelectionMatches = recoveredFutureRedoApplied
            && selectionEqual(
                recoveredFrameHistory.current().selection,
                afterSecondPoint.selection);
    }
    const Thing* recoveryThing = recoveryCurrent.level.things.empty()
        ? nullptr
        : &recoveryCurrent.level.things.front();
    const std::optional<std::int32_t> recoveryFrame = recoveryThing
        ? frameValueAtOrdinal(*recoveryThing, 0u)
        : std::nullopt;
    const bool recoveredCurrentLevelMatches = recovered
        && assets::sedLevelsEqual(
            recovered->current.level, recoveryCurrent.level);
    const bool recoveredEditSelectionMatches = recoveredSelection
        && recoveryThing && recoveryFrame
        && selectedFrameIsValue(
            *recoveredSelection, *recoveryThing, *recoveryFrame)
        && selectionEqual(recoveryCurrent.selection, *recoveredSelection);
    const bool recoverySucceeded = recoveryDirectory.isValid() && undo
            && recoveryRecord && recovered
            && recoveryError.isEmpty() && recovered->history
            && recoveredHistoryShape
            && recoveredCurrentLevelMatches
            && recoveredSelection && recoveredSelectionError.isEmpty()
            && recoveredEditSelectionMatches
            && recoveredUndoMatches && recoveredRedoMatches
            && recoveredFutureRedoLevelMatches
            && recoveredFutureRedoSelectionMatches;
    if ( !recoverySucceeded )
    {
        const QByteArray recoveryErrorBytes = recoveryError.toUtf8();
        const QByteArray recoveredSelectionErrorBytes =
            recoveredSelectionError.toUtf8();
        std::fprintf(stderr,
            "frame recovery diagnostics: temp=%d undoAction=%d record=%d recovered=%d recoveryError='%s' history=%d historyDepths=%d undoEntries=%zu redoEntries=%zu currentLevel=%d selection=%d selectionError='%s' frameOrdinal=%d currentEditSelection=%d historyUndo=%d undoApplied=%d undoLevel=%d undoSelection=%d historyRedo=%d redoApplied=%d redoLevel=%d redoSelection=%d futureRedo=%d futureLevel=%d futureSelection=%d\n",
            recoveryDirectory.isValid(), undo != nullptr,
            recoveryRecord.has_value(), recovered.has_value(),
            recoveryErrorBytes.constData(),
            recovered && recovered->history.has_value(),
            recoveredHistoryShape,
            recovered && recovered->history
                ? recovered->history->undoEntryCount()
                : 0u,
            recovered && recovered->history
                ? recovered->history->redoEntryCount()
                : 0u,
            recoveredCurrentLevelMatches,
            recoveredSelection.has_value(),
            recoveredSelectionErrorBytes.constData(),
            recoveryFrame ? *recoveryFrame : -1,
            recoveredEditSelectionMatches, recoveredUndoMatches,
            recoveredUndoApplied, recoveredUndoLevelMatches,
            recoveredUndoSelectionMatches, recoveredRedoMatches,
            recoveredRedoApplied, recoveredRedoLevelMatches,
            recoveredRedoSelectionMatches, recoveredFutureRedoApplied,
            recoveredFutureRedoLevelMatches,
            recoveredFutureRedoSelectionMatches);
        if ( recoveredFutureRedoApplied )
        {
            printFrameHistorySelection(
                "recovery expected future redo",
                afterSecondPoint.selection,
                afterSecondPoint.level);
            printFrameHistorySelection(
                "recovery actual future redo",
                recoveredFrameHistory.current().selection,
                recoveredFrameHistory.current().level);
        }
    }
    check(recoverySucceeded,
        "Production recovery preserves UI-added frame Level, EDIT selection, and exact frame Undo/Redo history");

    auto saveCandidateWindow = std::make_unique<MainWindow>();
    saveCandidateWindow->setAttribute(Qt::WA_DontShowOnScreen);
    saveCandidateWindow->activateFrameEditModeForTest();
    editor::DocumentState saveCandidateState = frameState({
        frameParameter(1.0, 2.0, 3.0, 4.0, 5.0, 6.0),
        integerParameter("numframes", 1),
    });
    saveCandidateState.selection.frames.insert({ 0, 0 });
    saveCandidateWindow->setDocumentStateForTest(saveCandidateState);
    GlView* saveCandidateView = saveCandidateWindow->glViewForTest();
    if (saveCandidateView)
    {
        saveCandidateView->setSelectionState(
            -1, -1, 0, -1, saveCandidateState.selection);
        saveCandidateView->setSelectedFrameValueIndex(0);
    }
    const QByteArray capturedSaveEdit =
        saveCandidateWindow->captureRecoveryEditorStateForTest();
    QString saveEditValidationError{};
    const bool saveEditValid = saveCandidateWindow->validateRecoveryEditorStateForTest(
        capturedSaveEdit, &saveEditValidationError);
    Level normalizedCandidate = saveCandidateState.level;
    sed::level::thingSyncNumFrames(normalizedCandidate.things.front());
    QString candidateRemapError{};
    const auto normalizedEdit = detail::remapRecoveryEditorFrameRefsForCandidateLevel(
        capturedSaveEdit, saveCandidateState.level, normalizedCandidate,
        &candidateRemapError);
    const QCborMap normalizedEditMap = normalizedEdit
        ? QCborValue::fromCbor(*normalizedEdit).toMap()
        : QCborMap{};
    QString normalizedSelectionError{};
    const auto normalizedSelection = ui::recovery::deserializeSelection(
        normalizedEditMap.value(QStringLiteral("selection")).toByteArray(),
        &normalizedSelectionError);
    const QCborArray normalizedActive = normalizedEditMap
        .value(QStringLiteral("active")).toArray();
    const auto normalizedFrame = frameValueAtOrdinal(
        normalizedCandidate.things.front(), 0u);
    editor::DocumentState normalizedCandidateState = saveCandidateState;
    normalizedCandidateState.level = normalizedCandidate;
    saveCandidateWindow->setDocumentStateForTest(
        std::move(normalizedCandidateState));
    const bool normalizedEditRestored = normalizedEdit
        && saveCandidateWindow->restoreRecoveryEditorStateForTest(*normalizedEdit);

    editor::Selection malformedSelection = saveCandidateState.selection;
    malformedSelection.frames.clear();
    malformedSelection.frames.insert({ 0, 1 });
    QCborMap malformedEditMap = QCborValue::fromCbor(capturedSaveEdit).toMap();
    malformedEditMap.insert(QStringLiteral("selection"),
        ui::recovery::serializeSelection(malformedSelection));
    const QByteArray malformedEdit = ui::recovery::serializeCanonicalCbor(
        QCborValue(malformedEditMap));
    const auto malformedRemap =
        detail::remapRecoveryEditorFrameRefsForCandidateLevel(
            malformedEdit, saveCandidateState.level, normalizedCandidate);
    Level candidateWithoutFrame = saveCandidateState.level;
    candidateWithoutFrame.things.front().values.erase(
        std::remove_if(candidateWithoutFrame.things.front().values.begin(),
            candidateWithoutFrame.things.front().values.end(),
            [](const TplValue& value)
            {
                return value.type == ValueType::frame
                    && value.name == "frame";
            }),
        candidateWithoutFrame.things.front().values.end());
    const auto missingCandidateRemap =
        detail::remapRecoveryEditorFrameRefsForCandidateLevel(
            capturedSaveEdit, saveCandidateState.level, candidateWithoutFrame);
    check(saveEditValid && saveEditValidationError.isEmpty()
            && normalizedEdit && candidateRemapError.isEmpty()
            && normalizedSelection && normalizedSelectionError.isEmpty()
            && normalizedFrame
            && normalizedSelection->frames.contains(
                level::FrameRef{ 0, *normalizedFrame })
            && normalizedActive.size() == 7
            && normalizedActive.at(5).toInteger() == 0
            && normalizedActive.at(6).toInteger() == *normalizedFrame
            && normalizedEditRestored
            && saveCandidateWindow->glViewForTest()->activeFrameValueIndex()
                == *normalizedFrame
            && !malformedRemap && !missingCandidateRemap,
        "Native Save EDIT remaps selected and active frame ordinals into the normalized candidate and rejects invalid frame pairs");

    checkSemanticAddSelectionForValues(
        { integerParameter("numframes", 1),
          stringParameter("move", "path"),
          frameParameter(1.0, 2.0, 3.0, 4.0, 5.0, 6.0),
          stringParameter("note", "after canonical count") },
        1u,
        "Frame Add with canonical numframes and interleaved parameters selects the appended frame value");
    checkSemanticAddSelectionForValues(
        { stringParameter("move", "path"),
          frameParameter(1.0, 2.0, 3.0, 4.0, 5.0, 6.0),
          stringParameter("note", "missing count") },
        1u,
        "Frame Add with missing numframes and interleaved parameters selects the appended frame value");
    checkSemanticAddSelectionForValues(
        { stringParameter("move", "path"),
          frameParameter(1.0, 2.0, 3.0, 4.0, 5.0, 6.0),
          stringParameter("note", "before trailing count"),
          integerParameter("numframes", 91) },
        1u,
        "Frame Add with trailing numframes and interleaved parameters selects the appended frame value");
    checkSemanticAddSelectionForValues(
        { integerParameter("numframes", 91),
          stringParameter("move", "path"),
          frameParameter(1.0, 2.0, 3.0, 4.0, 5.0, 6.0),
          stringParameter("note", "between duplicate counts"),
          integerParameter("numframes", 92) },
        1u,
        "Frame Add with duplicate numframes and interleaved parameters selects the appended frame value");

    // Exercise the real EDIT capture/restore, HIST Save encoder/restore decoder,
    // and the production native Level + optional-section codec as one document.
    const QByteArray editBytes = window->captureRecoveryEditorStateForTest();
    editor::Document historyDocument(window->documentStateForTest());
    const bool firstHistoryMutation = historyDocument.applyUndoable(
        "Thing frame-history position",
        editor::DocumentDirty::Things,
        [](editor::DocumentState& current)
        {
            current.level.things[0].x += 12.0;
            return true;
        });
    const bool secondHistoryMutation = historyDocument.applyUndoable(
        "Thing frame-history second frame",
        editor::DocumentDirty::Things,
        [](editor::DocumentState& current)
        {
            Thing& thing = current.level.things[0];
            thing.values.push_back(
                frameParameter(20.0, 21.0, 22.0, 23.0, 24.0, 25.0));
            sed::level::thingSyncNumFrames(thing);
            const std::optional<std::int32_t> appendedFrame =
                frameValueAtOrdinal(thing, 1u);
            if ( !appendedFrame )
            {
                return false;
            }
            current.selection.frames.clear();
            current.selection.frames.insert({ 0, *appendedFrame });
            const std::optional<std::int32_t> numFrames =
                thingNumFramesValue(thing);
            return sed::level::thingCountFrames(thing) == 2u
                && numFrames && *numFrames == 2;
        });
    const bool historyUndo = historyDocument.undo();
    const editor::Document persistedExpected = historyDocument;
    editor::Document expectedUndo = historyDocument;
    const bool expectedUndoApplied = expectedUndo.undo();
    editor::Document expectedRedo = historyDocument;
    const bool expectedRedoApplied = expectedRedo.redo();

    const QByteArray historyBytes = ui::recovery::serializeEditorDocumentHistoryForSave(
        historyDocument);
    const std::array<assets::SedOptionalSection, 2> sections{
        optionalSection({ 'e', 'd', 'i', 't' }, editBytes),
        optionalSection({ 'h', 'i', 's', 't' }, historyBytes),
    };
    const std::vector<std::byte> nativeBytes = assets::saveToSedBytes(
        historyDocument.current().level, sections);
    const assets::SedFile nativeFile = assets::loadSedFileBytes(nativeBytes);
    const QByteArray loadedEdit = optionalPayload(nativeFile, { 'e', 'd', 'i', 't' });
    const QByteArray loadedHistory = optionalPayload(nativeFile, { 'h', 'i', 's', 't' });
    QString historyError{};
    const auto restoredHistory = ui::recovery::deserializeEditorDocumentHistoryForRestore(
        loadedHistory, nativeFile.level.kind, nativeFile.level, &historyError);

    QCborMap editMap = QCborValue::fromCbor(loadedEdit).toMap();
    const QByteArray selectionBytes = editMap.value(QStringLiteral("selection")).toByteArray();
    QString selectionError{};
    const auto decodedSelection = ui::recovery::deserializeSelection(
        selectionBytes, &selectionError);
    auto editRestoredWindow = std::make_unique<MainWindow>();
    editRestoredWindow->setAttribute(Qt::WA_DontShowOnScreen);
    editor::DocumentState restoredWorkspace{};
    restoredWorkspace.level = nativeFile.level;
    editRestoredWindow->setDocumentStateForTest(std::move(restoredWorkspace));
    const bool editRestored = editRestoredWindow->restoreRecoveryEditorStateForTest(loadedEdit);

    editor::DocumentState restoredCurrent{};
    restoredCurrent.level = nativeFile.level;
    if (decodedSelection)
    {
        restoredCurrent.selection = *decodedSelection;
    }
    editor::Document restoredDocument{};
    bool restoredHistoryShape = false;
    if (restoredHistory)
    {
        restoredDocument.restoreRecoveryHistory(
            std::move(restoredCurrent), *restoredHistory);
        restoredHistoryShape = restoredDocument.undoDepth() == 1u
            && restoredDocument.redoDepth() == 1u;
    }
    const bool restoredUndo = restoredHistory && restoredDocument.undo();
    const bool restoredUndoLevelMatches = restoredUndo && expectedUndoApplied
        && assets::sedLevelsEqual(
            restoredDocument.current().level, expectedUndo.current().level);
    const bool restoredUndoSelectionMatches = restoredUndo
        && selectionEqual(
            restoredDocument.current().selection,
            expectedUndo.current().selection);
    const bool restoredUndoExact = restoredUndoLevelMatches
        && restoredUndoSelectionMatches;
    const bool restoredRedo = restoredUndo && restoredDocument.redo();
    const bool restoredRedoCurrentLevelMatches = restoredRedo
        && assets::sedLevelsEqual(
            restoredDocument.current().level,
            persistedExpected.current().level);
    const bool restoredRedoCurrentSelectionMatches = restoredRedo
        && selectionEqual(
            restoredDocument.current().selection,
            persistedExpected.current().selection);
    const bool restoredFutureRedo = restoredRedo
        && restoredDocument.redo();
    const bool restoredFutureRedoLevelMatches = restoredFutureRedo
        && expectedRedoApplied
        && assets::sedLevelsEqual(
            restoredDocument.current().level, expectedRedo.current().level);
    const bool restoredFutureRedoSelectionMatches = restoredFutureRedo
        && selectionEqual(
            restoredDocument.current().selection,
            expectedRedo.current().selection);
    const bool nativeCurrentLevelMatches = assets::sedLevelsEqual(
        nativeFile.level, historyDocument.current().level);
    const bool editRoundTripMatches = loadedEdit == editBytes;
    const bool historyRoundTripMatches = loadedHistory == historyBytes;
    const bool restoredEditSelectionMatches = editRestored
        && selectionEqual(
            editRestoredWindow->documentStateForTest().selection,
            window->documentStateForTest().selection);
    const bool decodedEditSelectionMatches = decodedSelection
        && selectionEqual(
            *decodedSelection, persistedExpected.current().selection);
    const Thing* const expectedFutureThing = expectedRedo.current().level.things.empty()
        ? nullptr
        : &expectedRedo.current().level.things.front();
    const std::optional<std::int32_t> expectedFutureNumFrames = expectedFutureThing
        ? thingNumFramesValue(*expectedFutureThing)
        : std::nullopt;
    const bool expectedFutureFrameCountValid = expectedFutureThing
        && sed::level::thingCountFrames(*expectedFutureThing) == 2u
        && expectedFutureNumFrames && *expectedFutureNumFrames == 2;
    const Thing* const restoredFutureThing = restoredDocument.current().level.things.empty()
        ? nullptr
        : &restoredDocument.current().level.things.front();
    const std::optional<std::int32_t> restoredFutureNumFrames = restoredFutureThing
        ? thingNumFramesValue(*restoredFutureThing)
        : std::nullopt;
    const bool restoredFutureFrameCountValid = restoredFutureThing
        && sed::level::thingCountFrames(*restoredFutureThing) == 2u
        && restoredFutureNumFrames && *restoredFutureNumFrames == 2;
    const bool restoredRedoCurrentExact = restoredRedoCurrentLevelMatches
        && restoredRedoCurrentSelectionMatches;
    const bool restoredFutureRedoExact = restoredFutureRedoLevelMatches
        && restoredFutureRedoSelectionMatches;
    const bool nativeSaveSucceeded = !editBytes.isEmpty()
        && firstHistoryMutation && secondHistoryMutation
        && historyUndo && expectedUndoApplied && expectedRedoApplied
        && !historyBytes.isEmpty() && nativeCurrentLevelMatches
        && editRoundTripMatches && historyRoundTripMatches
        && historyError.isEmpty() && restoredHistory
        && restoredHistoryShape && decodedSelection && selectionError.isEmpty()
        && restoredEditSelectionMatches && decodedEditSelectionMatches
        && restoredUndoExact && restoredRedoCurrentExact
        && restoredFutureRedoExact && expectedFutureFrameCountValid
        && restoredFutureFrameCountValid;
    if ( !nativeSaveSucceeded )
    {
        const QByteArray historyErrorBytes = historyError.toUtf8();
        const QByteArray selectionErrorBytes = selectionError.toUtf8();
        std::fprintf(stderr,
            "native frame history diagnostics: edit=%d mutation1=%d mutation2=%d sourceUndo=%d expectedUndo=%d expectedFutureRedo=%d historyBytes=%d nativeLevel=%d editBytes=%d historyBytesRoundtrip=%d historyError='%s' history=%d historyDepths=%d undoEntries=%zu redoEntries=%zu selection=%d selectionError='%s' editRestore=%d editSelection=%d decodedSelection=%d undo=%d undoLevel=%d undoSelection=%d redoCurrent=%d redoCurrentLevel=%d redoCurrentSelection=%d futureRedo=%d futureLevel=%d futureSelection=%d expectedFutureFramesValid=%d restoredFutureFramesValid=%d\n",
            !editBytes.isEmpty(), firstHistoryMutation, secondHistoryMutation,
            historyUndo, expectedUndoApplied, expectedRedoApplied,
            !historyBytes.isEmpty(), nativeCurrentLevelMatches,
            editRoundTripMatches, historyRoundTripMatches,
            historyErrorBytes.constData(), restoredHistory.has_value(),
            restoredHistoryShape,
            restoredHistory ? restoredHistory->undoEntryCount() : 0u,
            restoredHistory ? restoredHistory->redoEntryCount() : 0u,
            decodedSelection.has_value(), selectionErrorBytes.constData(),
            editRestored, restoredEditSelectionMatches,
            decodedEditSelectionMatches, restoredUndo, restoredUndoLevelMatches,
            restoredUndoSelectionMatches, restoredRedo,
            restoredRedoCurrentLevelMatches,
            restoredRedoCurrentSelectionMatches, restoredFutureRedo,
            restoredFutureRedoLevelMatches,
            restoredFutureRedoSelectionMatches,
            expectedFutureFrameCountValid, restoredFutureFrameCountValid);
        if ( restoredRedo && !restoredRedoCurrentLevelMatches )
        {
            printThingHistoryLevelDiff(
                "native expected persisted-current redo",
                persistedExpected.current().level,
                restoredDocument.current().level);
        }
        if ( restoredFutureRedo && !restoredFutureRedoLevelMatches )
        {
            printThingHistoryLevelDiff(
                "native expected future redo",
                expectedRedo.current().level,
                restoredDocument.current().level);
        }
        if ( restoredRedo && !restoredRedoCurrentSelectionMatches )
        {
            printFrameHistorySelection(
                "native expected persisted-current redo",
                persistedExpected.current().selection,
                persistedExpected.current().level);
            printFrameHistorySelection(
                "native actual persisted-current redo",
                restoredDocument.current().selection,
                restoredDocument.current().level);
        }
        if ( restoredFutureRedo && !restoredFutureRedoSelectionMatches )
        {
            printFrameHistorySelection(
                "native expected future redo",
                expectedRedo.current().selection,
                expectedRedo.current().level);
            printFrameHistorySelection(
                "native actual future redo",
                restoredDocument.current().selection,
                restoredDocument.current().level);
        }
    }
    check(nativeSaveSucceeded,
        "Native Level + EDIT + HIST Save/Open preserves exact selected frame and Undo/Redo states");
}

} // namespace sed::app_tests
