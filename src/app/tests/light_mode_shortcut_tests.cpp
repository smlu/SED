#include "test_common.hpp"

#include "app_settings.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "render/gizmos/light_category_gizmo_utils.hpp"
#include "ui/shortcuts/shortcut_context_utils.hpp"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QSettings>

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>

namespace sed::app_tests
{
namespace
{

struct KeyDispatchResult
{
    bool shortcutOverrideAccepted{};
};

KeyDispatchResult sendKey(
    QWidget* target,
    const int key,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier,
    const bool autoRepeat = false,
    const QString& text = {})
{
    if ( !target )
    {
        return {};
    }

    QKeyEvent shortcutOverride(
        QEvent::ShortcutOverride, key, modifiers, text, autoRepeat, 1);
    shortcutOverride.setAccepted(false);
    QApplication::sendEvent(target, &shortcutOverride);

    QKeyEvent press(QEvent::KeyPress, key, modifiers, text, autoRepeat, 1);
    press.setAccepted(false);
    QApplication::sendEvent(target, &press);

    QKeyEvent release(QEvent::KeyRelease, key, modifiers, text, autoRepeat, 1);
    release.setAccepted(false);
    QApplication::sendEvent(target, &release);

    return { shortcutOverride.isAccepted() };
}

bool shortcutOverrideClaimed(
    MainWindow& window,
    QObject* watched,
    const int key,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier,
    const bool autoRepeat = false)
{
    QKeyEvent event(
        QEvent::ShortcutOverride, key, modifiers, QString{}, autoRepeat, 1);
    event.setAccepted(false);
    const bool filtered = window.eventFilterForTest(watched, &event);
    return filtered && event.isAccepted();
}

bool keyPressConsumed(
    MainWindow& window,
    QObject* watched,
    const int key,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier,
    const bool autoRepeat = false)
{
    QKeyEvent event(QEvent::KeyPress, key, modifiers, QString{}, autoRepeat, 1);
    event.setAccepted(false);
    return window.eventFilterForTest(watched, &event);
}

TplValue stringValue(const std::string& name, const std::string& value)
{
    TplValue result{};
    result.name = name;
    result.type = ValueType::str;
    result.stringValue = value;
    return result;
}

editor::DocumentState emptyLightLevel(const ProjectType project)
{
    editor::DocumentState state{};
    state.level.kind = project;
    state.level.header.version = project == ProjectType::IJIM ? 3 : 1;
    state.level.layers.emplace_back("Layer0");
    if ( project == ProjectType::IJIM )
    {
        state.level.newSector().layer = 0;
    }
    return state;
}

void setSavedBinding(const char* id, const QStringList& binding)
{
    QSettings settings = settingsStore();
    settings.beginGroup(QStringLiteral("keymap/shortcuts"));
    settings.setValue(QString::fromLatin1(id), binding);
    settings.endGroup();
    settings.sync();
}

void removeSavedBinding(const char* id)
{
    QSettings settings = settingsStore();
    settings.beginGroup(QStringLiteral("keymap/shortcuts"));
    settings.remove(QString::fromLatin1(id));
    settings.endGroup();
    settings.sync();
}

void setLightShortcutLayoutVersion(const int version)
{
    QSettings settings = settingsStore();
    settings.beginGroup(QStringLiteral("keymap/shortcuts"));
    settings.setValue(QStringLiteral("_light_add_shortcut_layout_version"), version);
    settings.endGroup();
    settings.sync();
}

void testDefaultBindingsAndContext()
{
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);

    QAction* const lightAdd = window.findChild<QAction*>(QStringLiteral("edit.add_light"));
    QAction* const sectorAdd = window.findChild<QAction*>(QStringLiteral("edit.add_cube"));
    QAction* const thingAdd = window.findChild<QAction*>(QStringLiteral("edit.add_thing"));
    QAction* const oldLightAdd = window.findChild<QAction*>(
        QStringLiteral("light.command.add"));
    QAction* const oldSectorAdd = window.findChild<QAction*>(
        QStringLiteral("sector.command.add"));
    const QList<QKeySequence> defaultA{ QKeySequence(Qt::Key_A) };
    check(lightAdd && lightAdd->shortcuts() == defaultA
            && lightAdd->shortcutContext() == Qt::WidgetShortcut
            && !lightAdd->autoRepeat(),
        "the canonical Add Light action has the Light-local A default");
    check(sectorAdd && sectorAdd->shortcuts() == defaultA
            && sectorAdd->shortcutContext() == Qt::WidgetShortcut
            && !sectorAdd->autoRepeat(),
        "the canonical Add Sector action has the Sector-local A default");
    check(thingAdd && thingAdd->shortcuts() == defaultA
            && thingAdd->shortcutContext() == Qt::WidgetShortcut
            && !thingAdd->autoRepeat(),
        "the canonical Add Thing action has the Thing-local A default");
    check(!oldLightAdd && !oldSectorAdd,
        "Light and Sector Add each have one real keymap command rather than a synthetic duplicate");

    if ( !lightAdd || !sectorAdd || !thingAdd )
    {
        return;
    }

    const QString crossModeConflict = ui::shortcuts::keymapShortcutConflict({
        { lightAdd, QStringLiteral("Add Light"), lightAdd->shortcuts() },
        { sectorAdd, QStringLiteral("Add Sector"), sectorAdd->shortcuts() },
        { thingAdd, QStringLiteral("Add Thing"), thingAdd->shortcuts() },
    });
    check(crossModeConflict.isEmpty(),
        "the three A defaults remain conflict-free in their separate edit-mode shortcut domains");

    QAction sectorAction(QStringLiteral("test sector command"), &window);
    sectorAction.setObjectName(QStringLiteral("sector.command.test"));
    sectorAction.setShortcut(QKeySequence(Qt::Key_A));
    sectorAction.setShortcutContext(Qt::WidgetShortcut);
    sectorAction.setProperty("sed_default_shortcuts", QStringList{});
    const QString sectorConflict = ui::shortcuts::keymapShortcutConflict({
        { sectorAdd, QStringLiteral("Add Sector"), sectorAdd->shortcuts() },
        { &sectorAction, QStringLiteral("Sector command"), sectorAction.shortcuts() },
    });
    check(!sectorConflict.isEmpty(),
        "a second Sector-local A binding still conflicts with Add Sector");
}

void testSupportedSubmodeRoutes()
{
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(emptyLightLevel(ProjectType::JKDF2));
    GlView* const view = window.glViewForTest();
    QAction* const bakedAdd = window.findChild<QAction*>(QStringLiteral("edit.add_light"));
    QAction* const thingAdd = window.findChild<QAction*>(
        QStringLiteral("viewport.light_mode.add_thing_light"));
    QAction* const sectorAdd = window.findChild<QAction*>(
        QStringLiteral("viewport.light_mode.add_sector_light"));
    QAction* const sectorMode = window.findChild<QAction*>(
        QStringLiteral("viewport.light_mode.sector"));
    QAction* const transformMove = window.findChild<QAction*>(
        QStringLiteral("editor.tool.move"));
    QAction* const addThingMenu = window.findChild<QAction*>(QStringLiteral("edit.add_thing"));
    check(view && bakedAdd && thingAdd && sectorAdd && sectorMode,
        "light Add route fixture exposes the viewport and each light-family action");
    if ( !view || !bakedAdd || !thingAdd || !sectorAdd || !sectorMode )
    {
        return;
    }

    int bakedTriggers = 0;
    int thingTriggers = 0;
    int sectorTriggers = 0;
    QObject::connect(bakedAdd, &QAction::triggered, &window,
        [&bakedTriggers]() { ++bakedTriggers; });
    QObject::connect(thingAdd, &QAction::triggered, &window,
        [&thingTriggers]() { ++thingTriggers; });
    QObject::connect(sectorAdd, &QAction::triggered, &window,
        [&sectorTriggers]() { ++sectorTriggers; });

    const auto triggerViewportKey = [&](const int key,
                                        const Qt::KeyboardModifiers modifiers = Qt::NoModifier,
                                        const bool autoRepeat = false)
    {
        return sendKey(view, key, modifiers, autoRepeat);
    };

    bool routed = true;
    const auto exerciseBakedAndThing = [&](const ProjectType project)
    {
        window.setDocumentStateForTest(emptyLightLevel(project));
        window.activateBakedLightEditModeForTest();
        const int bakedBefore = bakedTriggers;
        const int thingBefore = thingTriggers;
        const KeyDispatchResult bakedKey = triggerViewportKey(Qt::Key_A);
        routed = routed && bakedKey.shortcutOverrideAccepted
            && bakedTriggers == bakedBefore + 1
            && thingTriggers == thingBefore
            && bakedAdd->isChecked();

        window.activateThingLightEditModeForTest();
        const int thingBeforeAdd = thingTriggers;
        const int bakedBeforeThing = bakedTriggers;
        const KeyDispatchResult thingKey = triggerViewportKey(Qt::Key_A);
        routed = routed && thingKey.shortcutOverrideAccepted
            && thingTriggers == thingBeforeAdd + 1
            && bakedTriggers == bakedBeforeThing
            && thingAdd->isChecked();
        const int thingBeforeRepeat = thingTriggers;
        const KeyDispatchResult thingRepeat = triggerViewportKey(Qt::Key_A,
            Qt::NoModifier, true);
        routed = routed && thingRepeat.shortcutOverrideAccepted
            && keyPressConsumed(window, view, Qt::Key_A, Qt::NoModifier, true)
            && thingTriggers == thingBeforeRepeat
            && thingAdd->isChecked();
    };

    exerciseBakedAndThing(ProjectType::JKDF2);
    exerciseBakedAndThing(ProjectType::MOTS);
    exerciseBakedAndThing(ProjectType::IJIM);

    window.setDocumentStateForTest(emptyLightLevel(ProjectType::IJIM));
    window.activateBakedLightEditModeForTest();
    sectorMode->trigger();
    const int sectorBefore = sectorTriggers;
    const int bakedBeforeSector = bakedTriggers;
    const KeyDispatchResult sectorKey = triggerViewportKey(Qt::Key_A);
    routed = routed && sectorKey.shortcutOverrideAccepted
        && sectorTriggers == sectorBefore + 1
        && bakedTriggers == bakedBeforeSector
        && sectorAdd->isChecked();

    // With no eligible Sector owners, A is still owned by Sector-light mode:
    // it must not activate another family or escape to a global tool.
    editor::DocumentState noSectorState = emptyLightLevel(ProjectType::IJIM);
    noSectorState.level.sectors.clear();
    window.setDocumentStateForTest(std::move(noSectorState));
    window.activateBakedLightEditModeForTest();
    sectorMode->trigger();
    window.updateEditActionsForTest();
    const int disabledSectorBefore = sectorTriggers;
    const int disabledBakedBefore = bakedTriggers;
    const KeyDispatchResult disabledSectorKey = triggerViewportKey(Qt::Key_A);
    routed = routed && !sectorAdd->isEnabled()
        && disabledSectorKey.shortcutOverrideAccepted
        && keyPressConsumed(window, view, Qt::Key_A)
        && sectorTriggers == disabledSectorBefore
        && bakedTriggers == disabledBakedBefore;

    window.setDocumentStateForTest(emptyLightLevel(ProjectType::MOTS));
    window.activateArchitecturalLightingEditModeForTest();
    const GlView::TransformTool priorTransform = view->transformTool();
    const int archBakedBefore = bakedTriggers;
    const int archThingBefore = thingTriggers;
    const int archSectorBefore = sectorTriggers;
    const KeyDispatchResult archKey = triggerViewportKey(Qt::Key_A);
    routed = routed && archKey.shortcutOverrideAccepted
        && keyPressConsumed(window, view, Qt::Key_A)
        && bakedTriggers == archBakedBefore
        && thingTriggers == archThingBefore
        && sectorTriggers == archSectorBefore
        && (!addThingMenu || !addThingMenu->isChecked())
        && (!transformMove || !transformMove->isChecked())
        && view->transformTool() == priorTransform;

    check(routed,
        "viewport Light A routes to each supported placement action and consumes disabled/Sector/Arch cases");
}

void testBakedAddRepeatAndFormerBinding()
{
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(emptyLightLevel(ProjectType::JKDF2));
    window.activateBakedLightEditModeForTest();
    GlView* const view = window.glViewForTest();
    QAction* const bakedAdd = window.findChild<QAction*>(QStringLiteral("edit.add_light"));
    check(view && bakedAdd, "baked Add regression fixture has its action and viewport");
    if ( !view || !bakedAdd )
    {
        return;
    }

    int triggers = 0;
    QObject::connect(bakedAdd, &QAction::triggered, &window,
        [&triggers]() { ++triggers; });
    const int before = triggers;
    const KeyDispatchResult initial = sendKey(view, Qt::Key_A);
    const int afterInitial = triggers;
    const KeyDispatchResult repeated = sendKey(view, Qt::Key_A, Qt::NoModifier, true);
    const bool repeatedKeyConsumed = keyPressConsumed(
        window, view, Qt::Key_A, Qt::NoModifier, true);
    check(initial.shortcutOverrideAccepted
            && afterInitial == before + 1 && bakedAdd->isChecked()
            && repeated.shortcutOverrideAccepted
            && repeatedKeyConsumed
            && triggers == afterInitial && bakedAdd->isChecked(),
        "Baked A starts Add Light once and a repeated key cannot toggle placement off");

    const int beforeOldAlias = triggers;
    const KeyDispatchResult formerAlias = sendKey(view, Qt::Key_L);
    check(!formerAlias.shortcutOverrideAccepted
            && !shortcutOverrideClaimed(window, view, Qt::Key_L)
            && triggers == beforeOldAlias,
        "L is no longer an Add Light alias");

    const int beforeFormerChord = triggers;
    const KeyDispatchResult formerChord = sendKey(
        view, Qt::Key_L, Qt::ControlModifier | Qt::ShiftModifier);
    check(!formerChord.shortcutOverrideAccepted
            && triggers == beforeFormerChord,
        "the former Ctrl+Shift+L Add Light default is no longer active");
}

void testRebindingsAndDefaultMigration()
{
    setLightShortcutLayoutVersion(1);
    setSavedBinding("edit.add_light", { QStringLiteral("L") });
    setSavedBinding("edit.add_cube", { QStringLiteral("Ctrl+Shift+C") });
    setSavedBinding("edit.add_thing", { QStringLiteral("Ctrl+A") });
    setSavedBinding("light.command.add", { QStringLiteral("A") });
    setSavedBinding("sector.command.add", { QStringLiteral("A") });
    {
        MainWindow migrated(nullptr, true);
        QAction* const bakedAdd = migrated.findChild<QAction*>(QStringLiteral("edit.add_light"));
        QAction* const sectorAdd = migrated.findChild<QAction*>(QStringLiteral("edit.add_cube"));
        QAction* const thingAdd = migrated.findChild<QAction*>(QStringLiteral("edit.add_thing"));
        QSettings settings = settingsStore();
        settings.beginGroup(QStringLiteral("keymap/shortcuts"));
        check(bakedAdd && bakedAdd->shortcuts()
                  == QList<QKeySequence>{ QKeySequence(Qt::Key_A) }
                && sectorAdd && sectorAdd->shortcuts()
                  == QList<QKeySequence>{ QKeySequence(Qt::Key_A) }
                && thingAdd && thingAdd->shortcuts()
                  == QList<QKeySequence>{ QKeySequence(Qt::Key_A) },
            "exact former Baked, Sector, and Thing Add defaults migrate to A");
        check(!settings.contains(QStringLiteral("light.command.add"))
                && !settings.contains(QStringLiteral("sector.command.add")),
            "obsolete synthetic Add bindings are removed after migration");
    }

    setLightShortcutLayoutVersion(0);
    setSavedBinding("edit.add_light", { QStringLiteral("Ctrl+Shift+L") });
    {
        MainWindow migrated(nullptr, true);
        QAction* const bakedAdd = migrated.findChild<QAction*>(QStringLiteral("edit.add_light"));
        check(bakedAdd && bakedAdd->shortcuts()
                  == QList<QKeySequence>{ QKeySequence(Qt::Key_A) },
            "the original exact Ctrl+Shift+L Baked default also migrates directly to A");
    }

    setLightShortcutLayoutVersion(1);
    setSavedBinding("edit.add_light", { QStringLiteral("L") });
    setSavedBinding("light.command.add", { QStringLiteral("F12") });
    setSavedBinding("edit.add_cube", { QStringLiteral("Ctrl+Shift+C") });
    setSavedBinding("sector.command.add", { QStringLiteral("F13") });
    setSavedBinding("edit.add_thing", { QStringLiteral("Ctrl+A") });
    {
        MainWindow migratedLegacyCustoms(nullptr, true);
        migratedLegacyCustoms.setAttribute(Qt::WA_DontShowOnScreen);
        migratedLegacyCustoms.setDocumentStateForTest(
            emptyLightLevel(ProjectType::JKDF2));
        QAction* const bakedAdd = migratedLegacyCustoms.findChild<QAction*>(
            QStringLiteral("edit.add_light"));
        QAction* const sectorAdd = migratedLegacyCustoms.findChild<QAction*>(
            QStringLiteral("edit.add_cube"));
        QAction* const thingAdd = migratedLegacyCustoms.findChild<QAction*>(
            QStringLiteral("edit.add_thing"));
        check(bakedAdd && bakedAdd->shortcuts()
                  == QList<QKeySequence>{ QKeySequence(Qt::Key_F12) }
                && sectorAdd && sectorAdd->shortcuts()
                  == QList<QKeySequence>{ QKeySequence(Qt::Key_F13) }
                && thingAdd && thingAdd->shortcuts()
                  == QList<QKeySequence>{ QKeySequence(Qt::Key_A) },
            "legacy synthetic custom bindings transfer only over exact former defaults");

        GlView* const view = migratedLegacyCustoms.glViewForTest();
        migratedLegacyCustoms.activateBakedLightEditModeForTest();
        int bakedTriggers = 0;
        if ( bakedAdd )
        {
            QObject::connect(bakedAdd, &QAction::triggered, &migratedLegacyCustoms,
                [&bakedTriggers]() { ++bakedTriggers; });
        }
        const KeyDispatchResult customLightKey = sendKey(view, Qt::Key_F12);
        migratedLegacyCustoms.activateSectorEditModeForTest();
        int sectorTriggers = 0;
        if ( sectorAdd )
        {
            QObject::connect(sectorAdd, &QAction::triggered, &migratedLegacyCustoms,
                [&sectorTriggers]() { ++sectorTriggers; });
        }
        const KeyDispatchResult customSectorKey = sendKey(view, Qt::Key_F13);
        check(view && customLightKey.shortcutOverrideAccepted
                && bakedTriggers == 1 && customSectorKey.shortcutOverrideAccepted
                && sectorTriggers == 1,
            "transferred Baked and Sector custom Add bindings still dispatch their canonical actions");
    }

    setLightShortcutLayoutVersion(1);
    setSavedBinding("edit.add_light", { QStringLiteral("Ctrl+Alt+X") });
    setSavedBinding("light.command.add", { QStringLiteral("F12") });
    {
        MainWindow custom(nullptr, true);
        QAction* const bakedAdd = custom.findChild<QAction*>(QStringLiteral("edit.add_light"));
        check(bakedAdd && bakedAdd->shortcuts()
                  == QList<QKeySequence>{ QKeySequence(QStringLiteral("Ctrl+Alt+X")) },
            "the Add Light migration preserves a canonical user-customized binding over a legacy custom one");
    }

    setLightShortcutLayoutVersion(1);
    setSavedBinding("edit.add_light", {});
    setSavedBinding("light.command.add", { QStringLiteral("F12") });
    {
        MainWindow disabled(nullptr, true);
        QAction* const bakedAdd = disabled.findChild<QAction*>(QStringLiteral("edit.add_light"));
        check(bakedAdd && bakedAdd->shortcuts().isEmpty(),
            "an explicitly cleared canonical binding remains disabled over a legacy custom binding");
    }

    setLightShortcutLayoutVersion(1);
    removeSavedBinding("edit.add_cube");
    setSavedBinding("sector.command.add", {});
    {
        MainWindow clearedLegacy(nullptr, true);
        QAction* const sectorAdd = clearedLegacy.findChild<QAction*>(
            QStringLiteral("edit.add_cube"));
        check(sectorAdd && sectorAdd->shortcuts().isEmpty(),
            "a cleared legacy Sector Add binding remains explicitly disabled after migration");
    }

    setLightShortcutLayoutVersion(1);
    setSavedBinding("edit.add_light", { QStringLiteral("A") });
    setSavedBinding("light.command.add", { QStringLiteral("F12") });
    {
        MainWindow legacyCustom(nullptr, true);
        QAction* const bakedAdd = legacyCustom.findChild<QAction*>(
            QStringLiteral("edit.add_light"));
        check(bakedAdd && bakedAdd->shortcuts()
                  == QList<QKeySequence>{ QKeySequence(Qt::Key_A) },
            "a canonical custom binding wins when a legacy synthetic command was also customized");
    }

    // Leave later tests with no custom keymap state. The executable itself
    // uses a process-local temporary INI, so these settings never touch a
    // developer's persistent preferences.
    removeSavedBinding("edit.add_light");
    removeSavedBinding("edit.add_cube");
    removeSavedBinding("edit.add_thing");
    removeSavedBinding("light.command.add");
    removeSavedBinding("sector.command.add");
    setLightShortcutLayoutVersion(2);
}

void testTextInputAndNonLightAdd()
{
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(emptyLightLevel(ProjectType::JKDF2));
    window.activateBakedLightEditModeForTest();
    QAction* const bakedAdd = window.findChild<QAction*>(QStringLiteral("edit.add_light"));
    int bakedTriggers = 0;
    if ( bakedAdd )
    {
        QObject::connect(bakedAdd, &QAction::triggered, &window,
            [&bakedTriggers]() { ++bakedTriggers; });
    }

    QLineEdit lineEdit(&window);
    lineEdit.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    lineEdit.show();
    window.activateWindow();
    QApplication::setActiveWindow(&window);
    lineEdit.setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();

    const bool focused = QApplication::focusWidget() == &lineEdit;
    const bool textInputOwnsLightBindings =
        !shortcutOverrideClaimed(window, &lineEdit, Qt::Key_A)
        && !shortcutOverrideClaimed(window, &lineEdit, Qt::Key_L);
    (void)sendKey(&lineEdit, Qt::Key_A, Qt::NoModifier, false, QStringLiteral("a"));
    (void)sendKey(&lineEdit, Qt::Key_L, Qt::NoModifier, false, QStringLiteral("l"));
    check(focused && textInputOwnsLightBindings
            && lineEdit.text() == QStringLiteral("al")
            && bakedTriggers == 0,
        "Light Add remains ordinary text when a native text editor owns focus");

    // Sector Add owns the same letter in its own mode-specific command.
    GlView* const view = window.glViewForTest();
    lineEdit.hide();
    lineEdit.clearFocus();
    window.activateSectorEditModeForTest();
    if ( view )
    {
        view->setFocus(Qt::OtherFocusReason);
    }
    QApplication::processEvents();
    const bool viewportFocused = view && QApplication::focusWidget() == view;
    QAction* const addCube = window.findChild<QAction*>(QStringLiteral("edit.add_cube"));
    int cubeTriggers = 0;
    if ( addCube )
    {
        QObject::connect(addCube, &QAction::triggered, &window,
            [&cubeTriggers]() { ++cubeTriggers; });
    }
    const int before = cubeTriggers;
    const KeyDispatchResult sectorA = sendKey(view, Qt::Key_A);
    const int afterSectorA = cubeTriggers;
    const KeyDispatchResult sectorRepeat = sendKey(view, Qt::Key_A, Qt::NoModifier, true);
    check(viewportFocused && addCube
            && sectorA.shortcutOverrideAccepted
            && afterSectorA == before + 1 && sectorRepeat.shortcutOverrideAccepted
            && keyPressConsumed(window, view, Qt::Key_A, Qt::NoModifier, true)
            && cubeTriggers == afterSectorA && addCube->isChecked(),
        "Sector A starts its canonical Add Sector action in Sector mode");

    window.activateThingEditModeForTest();
    if ( view )
    {
        view->setFocus(Qt::OtherFocusReason);
    }
    QApplication::processEvents();
    QAction* const addThing = window.findChild<QAction*>(QStringLiteral("edit.add_thing"));
    if ( addThing && addThing->isChecked() )
    {
        addThing->trigger();
    }
    const bool thingPanelInitiallyClosed = addThing && !addThing->isChecked();
    int thingTriggers = 0;
    if ( addThing )
    {
        QObject::connect(addThing, &QAction::triggered, &window,
            [&thingTriggers]() { ++thingTriggers; });
    }
    const int thingBefore = thingTriggers;
    const KeyDispatchResult thingA = sendKey(view, Qt::Key_A);
    const int afterThingA = thingTriggers;
    const bool thingCheckedAfterFirstA = addThing && addThing->isChecked();
    const KeyDispatchResult thingRepeat = sendKey(view, Qt::Key_A, Qt::NoModifier, true);
    const int afterThingRepeat = thingTriggers;
    const bool thingCheckedAfterRepeat = addThing && addThing->isChecked();
    const KeyDispatchResult closeThingA = sendKey(view, Qt::Key_A);
    check(view && QApplication::focusWidget() == view && addThing
            && thingPanelInitiallyClosed
            && thingA.shortcutOverrideAccepted
            && afterThingA == thingBefore + 1 && thingCheckedAfterFirstA
            && thingRepeat.shortcutOverrideAccepted
            && keyPressConsumed(window, view, Qt::Key_A, Qt::NoModifier, true)
            && afterThingRepeat == afterThingA && thingCheckedAfterRepeat
            && closeThingA.shortcutOverrideAccepted
            && thingTriggers == afterThingRepeat + 1 && !addThing->isChecked(),
        "Thing A opens the canonical Add Thing action once, ignores repeats, and closes it on the next press");
}

void testFlyRetainsAddA()
{
    bool flyStoppedDuringMainWindowTeardown = false;
    {
        MainWindow window(nullptr, true);
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.setDocumentStateForTest(emptyLightLevel(ProjectType::JKDF2));
        window.activateBakedLightEditModeForTest();
        window.show();
        GlView* const view = window.glViewForTest();
        QAction* const bakedAdd = window.findChild<QAction*>(QStringLiteral("edit.add_light"));
        check(view && bakedAdd, "Fly ownership fixture exposes the viewport and Add Light");
        if ( !view || !bakedAdd )
        {
            return;
        }

        QObject::connect(view, &GlView::flyNavigationActiveChanged, &window,
            [&flyStoppedDuringMainWindowTeardown](const bool active)
            {
                flyStoppedDuringMainWindowTeardown =
                    flyStoppedDuringMainWindowTeardown || !active;
            });

        window.activateWindow();
        QApplication::setActiveWindow(&window);
        view->setFocus(Qt::OtherFocusReason);
        QApplication::processEvents();
        const QPointF point(view->rect().center());
        const QPointF global = view->mapToGlobal(point.toPoint());
        QMouseEvent flyStart(QEvent::MouseButtonPress, point, global, global,
            Qt::RightButton, Qt::RightButton,
            Qt::ControlModifier | Qt::ShiftModifier);
        QApplication::sendEvent(view, &flyStart);

        int bakedTriggers = 0;
        QObject::connect(bakedAdd, &QAction::triggered, &window,
            [&bakedTriggers]() { ++bakedTriggers; });
        const KeyDispatchResult addA = sendKey(view, Qt::Key_A);
        check(view->flyNavigationActive()
                && addA.shortcutOverrideAccepted
                && keyPressConsumed(window, view, Qt::Key_A)
                && bakedTriggers == 0,
            "active Fly keeps A and prevents the Light Add action from triggering");
    }
    check(flyStoppedDuringMainWindowTeardown,
        "MainWindow stops Fly while its signal receivers are still alive during teardown");
}

void testUvPointerRetainsLightShortcutOwnership()
{
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    editor::DocumentState state{};
    state.level.kind = ProjectType::JKDF2;
    state.level.layers.emplace_back("Layer0");
    state.level.uvCoordinateSpace = UvCoordinateSpace::Normalized;
    Sector& sector = state.level.newSector();
    sector.layer = 0;
    sector.vertices = {
        Vertex{0.0, 0.0, 0.0, 0},
        Vertex{1.0, 0.0, 0.0, 1},
        Vertex{1.0, 1.0, 0.0, 2},
        Vertex{0.0, 1.0, 0.0, 3},
    };
    Surface& surface = sector.surfaces.emplace_back();
    surface.vertices = {0, 1, 2, 3};
    surface.normal = TVector{0.0, 0.0, 1.0};
    surface.texVertices = {
        TexVertex{0.0f, 0.0f, ColorWhite},
        TexVertex{1.0f, 0.0f, ColorWhite},
        TexVertex{1.0f, 1.0f, ColorWhite},
        TexVertex{0.0f, 1.0f, ColorWhite},
    };
    state.selection.surfaces.insert(SurfaceRef{0, 0});
    window.setDocumentStateForTest(std::move(state));
    window.activateSurfaceEditModeForTest();
    window.mutableDocumentStateForTest().selection.surfaces = {
        SurfaceRef{0, 0}};
    window.updateEditActionsForTest();
    window.resize(1440, 900);
    window.show();
    window.activateWindow();
    QApplication::setActiveWindow(&window);
    QApplication::processEvents();

    QAction* const showUvEditor = window.findChild<QAction*>(
        QStringLiteral("surface.uv.editor"));
    QWidget* const uvPanel = window.findChild<QWidget*>(
        QStringLiteral("uvEditorPanelHost"));
    QWidget* const uvCanvas = window.findChild<QWidget*>(
        QStringLiteral("uvEditorCanvas"));
    GlView* const view = window.glViewForTest();
    QAction* const bakedAdd = window.findChild<QAction*>(
        QStringLiteral("edit.add_light"));
    check(showUvEditor && uvPanel && uvCanvas && view && bakedAdd,
        "UV pointer ownership fixture exposes the real canvas and Light action");
    if ( !showUvEditor || !uvPanel || !uvCanvas || !view || !bakedAdd )
    {
        return;
    }

    check(showUvEditor->isEnabled(),
        "a selected real Surface enables the production UV Editor action");
    showUvEditor->trigger();
    QApplication::processEvents();
    const bool canvasPresented = uvPanel->isVisible()
        && uvCanvas->isVisible()
        && uvCanvas->width() > 0
        && uvCanvas->height() > 0;
    window.activateBakedLightEditModeForTest();
    const QPoint previousCursorPosition = QCursor::pos();
    const QPoint uvCanvasCenter = uvCanvas->mapToGlobal(uvCanvas->rect().center());
    QCursor::setPos(uvCanvasCenter);
    view->setFocus(Qt::OtherFocusReason);

    int bakedTriggers = 0;
    QObject::connect(bakedAdd, &QAction::triggered, &window,
        [&bakedTriggers]() { ++bakedTriggers; });
    const bool viewportFocusRetained = QApplication::focusWidget() == view;
    const bool pointerInsideUvCanvas = uvPanel->isVisible()
        && uvCanvas->isVisible()
        && uvCanvas->rect().contains(uvCanvas->mapFromGlobal(QCursor::pos()));
    const bool addAOverrideClaimed = shortcutOverrideClaimed(
        window, view, Qt::Key_A);
    const bool addLOverrideClaimed = shortcutOverrideClaimed(
        window, view, Qt::Key_L);
    const KeyDispatchResult addA = sendKey(view, Qt::Key_A);
    const int bakedTriggersAfterA = bakedTriggers;
    const KeyDispatchResult addL = sendKey(view, Qt::Key_L);
    const int bakedTriggersAfterL = bakedTriggers;
    const QPoint cursorPosition = QCursor::pos();
    const QPoint canvasLocalCursor = uvCanvas->mapFromGlobal(cursorPosition);
    QCursor::setPos(previousCursorPosition);
    if ( !canvasPresented || !viewportFocusRetained || !pointerInsideUvCanvas
        || addAOverrideClaimed || addLOverrideClaimed || bakedTriggers != 0 )
    {
        std::fprintf(stderr,
            "UV pointer diagnostics: presented=%d panelVisible=%d canvasVisible=%d "
            "canvas=%dx%d pointerInside=%d cursor=(%d,%d) local=(%d,%d) "
            "uvActionEnabled=%d viewportFocus=%d filterClaimA=%d filterClaimL=%d "
            "eventAcceptedA=%d eventAcceptedL=%d triggersAfterA=%d "
            "triggersAfterL=%d\n",
            canvasPresented,
            uvPanel->isVisible(),
            uvCanvas->isVisible(),
            uvCanvas->width(),
            uvCanvas->height(),
            pointerInsideUvCanvas,
            cursorPosition.x(),
            cursorPosition.y(),
            canvasLocalCursor.x(),
            canvasLocalCursor.y(),
            showUvEditor->isEnabled(),
            viewportFocusRetained,
            addAOverrideClaimed,
            addLOverrideClaimed,
            addA.shortcutOverrideAccepted,
            addL.shortcutOverrideAccepted,
            bakedTriggersAfterA,
            bakedTriggersAfterL);
    }
    check(canvasPresented,
        "the UV pointer ownership fixture presents its real canvas before entering Light mode");
    check(viewportFocusRetained && pointerInsideUvCanvas,
        "the UV canvas remains under the virtual pointer while GlView owns keyboard focus");
    check(!addAOverrideClaimed && !addLOverrideClaimed,
        "the Light viewport shortcut filter does not claim A or L over the UV canvas");
    check(bakedTriggersAfterA == 0,
        "UV pointer ownership prevents A from triggering Baked Add Light");
    check(bakedTriggersAfterL == 0,
        "UV pointer ownership prevents unhandled L from triggering Baked Add Light");
}

void testDeleteOwnershipAndDefaults()
{
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    editor::DocumentState state{};
    state.level.kind = ProjectType::JKDF2;
    state.level.layers.emplace_back("Layer0");
    Thing& owner = state.level.newThing();
    owner.layer = 0;
    owner.values.push_back(stringValue("light", "2"));
    state.selection.things.insert(0);
    window.setDocumentStateForTest(std::move(state));
    window.activateThingLightEditModeForTest();

    QAction* const deleteAction = window.findChild<QAction*>(QStringLiteral("edit.delete"));
    QAction* const forceDeleteAction = window.findChild<QAction*>(
        QStringLiteral("surface.force_delete_vertex"));
    GlView* const view = window.glViewForTest();
    check(deleteAction && deleteAction->shortcuts()
              == QList<QKeySequence>{ QKeySequence(Qt::Key_Delete) }
            && forceDeleteAction && forceDeleteAction->shortcuts()
                == QList<QKeySequence>{ QKeySequence(Qt::SHIFT | Qt::Key_Delete) },
        "ordinary Delete and confirmed force-delete retain their canonical Del and Shift+Del defaults");
    if ( !deleteAction || !view )
    {
        return;
    }

    editor::Selection& selection = window.mutableDocumentStateForTest().selection;
    selection.clear();
    selection.things.insert(0);
    view->setSelectionState(-1, -1, 0, -1, selection);
    view->setProperty(
        "sed.selectedLightGizmoKind",
        static_cast<int>(render::LightGizmoKind::ThingRegular));
    window.updateEditActionsForTest();
    const KeyDispatchResult deleteKey = sendKey(view, Qt::Key_Delete);

    const Level& afterDelete = window.documentStateForTest().level;
    const bool thingRetained = afterDelete.things.size() == 1;
    const bool localLightStillPresent = thingRetained && std::any_of(
        afterDelete.things.front().values.begin(),
        afterDelete.things.front().values.end(),
        [](const TplValue& value) { return value.name == "light"; });
    check(deleteKey.shortcutOverrideAccepted
            && thingRetained && !localLightStillPresent,
        "Thing-light Delete removes the selected owner's local light data and keeps the Thing itself");
}

} // namespace

void runLightModeShortcutTests()
{
    const bool offscreen = QApplication::platformName() == QStringLiteral("offscreen");
    check(offscreen,
        "Light Add routing tests run on the isolated Qt offscreen platform");
    if ( !offscreen )
    {
        return;
    }

    testDefaultBindingsAndContext();
    testSupportedSubmodeRoutes();
    testBakedAddRepeatAndFormerBinding();
    testRebindingsAndDefaultMigration();
    testTextInputAndNonLightAdd();
    testFlyRetainsAddA();
    testUvPointerRetainsLightShortcutOwnership();
    testDeleteOwnershipAndDefaults();
}

} // namespace sed::app_tests
