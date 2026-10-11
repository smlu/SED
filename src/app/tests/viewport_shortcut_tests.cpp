#include "test_common.hpp"

#include "app_settings.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QSettings>
#include <QTemporaryDir>
#include <QWidget>

#include <array>

namespace sed::app_tests
{
namespace
{

bool activateAndFocus(QWidget* const target)
{
    if (!target)
    {
        return false;
    }
    QWidget* const topLevel = target->window();
    if (!topLevel)
    {
        return false;
    }
    topLevel->activateWindow();
    topLevel->raise();
    QApplication::setActiveWindow(topLevel);
    target->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    return QApplication::activeWindow() == topLevel
        && QApplication::focusWidget() == target;
}

void sendKeyPress(
    QWidget* const target,
    const int key,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    if (!target)
    {
        return;
    }

    const QString text = key == Qt::Key_Tab && modifiers == Qt::NoModifier
        ? QStringLiteral("\t")
        : QString{};
    QKeyEvent shortcutOverride(QEvent::ShortcutOverride, key, modifiers, text);
    shortcutOverride.setAccepted(false);
    QApplication::sendEvent(target, &shortcutOverride);

    QKeyEvent press(QEvent::KeyPress, key, modifiers, text);
    press.setAccepted(false);
    QApplication::sendEvent(target, &press);

    QKeyEvent release(QEvent::KeyRelease, key, modifiers);
    QApplication::sendEvent(target, &release);
}

struct GeometryMode
{
    QAction* action{};
    bool solid{};
    bool textures{};
    bool vertices{};
};

void testNumberRowEditModesAndProjectSubmodes()
{
    QTemporaryDir temporary{};
    check(temporary.isValid(), "number-row shortcuts use isolated settings");
    if ( !temporary.isValid() )
    {
        return;
    }
    const std::filesystem::path previousSettings = appSettingsIniPath();
    struct RestoreSettingsPath
    {
        std::filesystem::path path{};
        ~RestoreSettingsPath() { setAppSettingsIniPathOverrideForTest(path); }
    } restoreSettings{ previousSettings };
    setAppSettingsIniPathOverrideForTest(
        std::filesystem::path(temporary.path().toStdWString()) / "numeric-shortcuts.ini");
    const std::array<QString, 4> modeIds{
        QStringLiteral("edit_mode.sector"), QStringLiteral("edit_mode.surface"),
        QStringLiteral("edit_mode.thing"), QStringLiteral("edit_mode.light") };
    {
        QSettings settings = settingsStore();
        settings.beginGroup(QStringLiteral("keymap/shortcuts"));
        for ( std::size_t index = 0; index < modeIds.size(); ++index )
        {
            settings.setValue(modeIds[index], QStringList{
                QKeySequence(int(Qt::ShiftModifier) | (Qt::Key_1 + static_cast<int>(index)))
                    .toString(QKeySequence::PortableText) });
        }
        settings.setValue(QStringLiteral("surface.submode.face"), QStringList{ QStringLiteral("Shift+F") });
        settings.setValue(QStringLiteral("surface.submode.edge"), QStringList{ QStringLiteral("Shift+E") });
        settings.setValue(QStringLiteral("surface.submode.vertex"), QStringList{ QStringLiteral("Shift+V") });
        settings.setValue(QStringLiteral("thing.command.toggle_frame_mode"), QStringList{ QStringLiteral("Shift+Tab") });
    }
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    GlView* const view = window.glViewForTest();
    window.activateBakedLightEditModeForTest();
    const int lightMode = window.editModeForTest();
    const std::array<int, 4> modeValues{
        window.sectorEditModeValueForTest(), window.surfaceEditModeValueForTest(),
        window.thingEditModeValueForTest(), lightMode };
    bool numericDefaults = view && activateAndFocus(view);
    for ( std::size_t index = 0; index < modeIds.size(); ++index )
    {
        QAction* const action = window.findChild<QAction*>(modeIds[index]);
        numericDefaults = numericDefaults && action
            && action->shortcuts() == QList<QKeySequence>{ QKeySequence(Qt::Key_1 + static_cast<int>(index)) };
        sendKeyPress(view, Qt::Key_1 + static_cast<int>(index));
        numericDefaults = numericDefaults && window.editModeForTest() == modeValues[index];
    }
    check(numericDefaults,
        "number-row shortcuts migrate former exact Shift-number defaults and route plain 1/2/3/4 to Sector/Surface/Thing/Light");

    window.activateSurfaceEditModeForTest();
    sendKeyPress(view, Qt::Key_2, Qt::ShiftModifier);
    const int edgeMode = window.surfaceSelectionModeForTest();
    sendKeyPress(view, Qt::Key_3, Qt::ShiftModifier);
    const int vertexMode = window.surfaceSelectionModeForTest();
    sendKeyPress(view, Qt::Key_1, Qt::ShiftModifier);
    check(window.editModeForTest() == window.surfaceEditModeValueForTest()
            && window.surfaceSelectionModeForTest() == window.surfaceFaceModeValueForTest()
            && edgeMode != vertexMode && edgeMode != window.surfaceFaceModeValueForTest()
            && vertexMode != window.surfaceFaceModeValueForTest(),
        "Surface Shift+1/2/3 selects Face/Edge/Vertex without changing the major edit mode");
    sendKeyPress(view, Qt::Key_At, Qt::ShiftModifier);
    const bool atSelectsEdge = window.surfaceSelectionModeForTest() == edgeMode;
    sendKeyPress(view, Qt::Key_QuoteDbl, Qt::ShiftModifier);
    const bool quoteSelectsEdge = window.surfaceSelectionModeForTest() == edgeMode;
    sendKeyPress(view, Qt::Key_Exclam, Qt::ShiftModifier);
    const bool exclamationSelectsFace = window.surfaceSelectionModeForTest()
        == window.surfaceFaceModeValueForTest();
    sendKeyPress(view, Qt::Key_NumberSign, Qt::ShiftModifier);
    const bool numberSignSelectsVertex = window.surfaceSelectionModeForTest() == vertexMode;
    sendKeyPress(view, Qt::Key_Dollar, Qt::ShiftModifier);
    check(atSelectsEdge && quoteSelectsEdge && exclamationSelectsFace
            && numberSignSelectsVertex && window.surfaceSelectionModeForTest() == vertexMode
            && window.editModeForTest() == window.surfaceEditModeValueForTest(),
        "actual Qt shifted punctuation for number-row keys routes supported Surface submodes and leaves Shift+4 unchanged");
    QAction* const faceKey = window.findChild<QAction*>(QStringLiteral("surface.submode.face"));
    QAction* const edgeKey = window.findChild<QAction*>(QStringLiteral("surface.submode.edge"));
    QAction* const vertexKey = window.findChild<QAction*>(QStringLiteral("surface.submode.vertex"));
    check(faceKey && edgeKey && vertexKey
            && faceKey->shortcuts().contains(QKeySequence(QStringLiteral("Shift+F")))
            && edgeKey->shortcuts().contains(QKeySequence(QStringLiteral("Shift+E")))
            && vertexKey->shortcuts().contains(QKeySequence(QStringLiteral("Shift+V"))),
        "Surface retains its unconflicted letter-key aliases after numeric default migration");
    window.activateThingEditModeForTest();
    sendKeyPress(view, Qt::Key_1, Qt::ShiftModifier);
    const bool enteredFrame = window.editModeForTest() == window.frameEditModeValueForTest();
    sendKeyPress(view, Qt::Key_1, Qt::ShiftModifier);
    check(enteredFrame && window.editModeForTest() == window.thingEditModeValueForTest(),
        "Thing Shift+1 toggles Frame and returns to Thing");

    bool supportedLightModes = true;
    for ( const ProjectType profile : { ProjectType::JKDF2, ProjectType::IJIM, ProjectType::MOTS } )
    {
        window.setProjectTypeForTest(profile);
        window.activateBakedLightEditModeForTest();
        QAction* const baked = window.findChild<QAction*>(QStringLiteral("viewport.light_mode.baked"));
        QAction* const thing = window.findChild<QAction*>(QStringLiteral("viewport.light_mode.thing"));
        QAction* const sector = window.findChild<QAction*>(QStringLiteral("viewport.light_mode.sector"));
        QAction* const architectural = window.findChild<QAction*>(QStringLiteral("viewport.light_mode.architectural"));
        sendKeyPress(view, Qt::Key_2, Qt::ShiftModifier);
        supportedLightModes = supportedLightModes && thing && thing->isChecked();
        sendKeyPress(view, Qt::Key_3, Qt::ShiftModifier);
        supportedLightModes = supportedLightModes
            && (profile == ProjectType::IJIM ? sector && sector->isChecked()
                : profile == ProjectType::MOTS ? architectural && architectural->isChecked()
                : thing && thing->isChecked());
        sendKeyPress(view, Qt::Key_1, Qt::ShiftModifier);
        supportedLightModes = supportedLightModes && baked && baked->isChecked()
            && window.editModeForTest() == lightMode;
    }
    check(supportedLightModes,
        "Light Shift+1/2 selects Baked/Thing; Shift+3 selects IJIM Sector or MotS Architectural and is unchanged in JKDF2");

    window.activateThingEditModeForTest();
    const int beforeKeypad = window.editModeForTest();
    for ( int key = Qt::Key_1; key <= Qt::Key_4; ++key )
    {
        sendKeyPress(view, key, Qt::KeypadModifier);
        sendKeyPress(view, key, Qt::KeypadModifier | Qt::ShiftModifier);
    }
    check(window.editModeForTest() == beforeKeypad,
        "number-row bindings leave every plain and Shift Numpad camera chord outside edit-mode routing");

    QLineEdit textEditor(&window);
    QDoubleSpinBox numericEditor(&window);
    QComboBox editableCombo(&window);
    editableCombo.setEditable(true);
    std::array<QWidget*, 3> inputWidgets{ &textEditor, &numericEditor, &editableCombo };
    bool inputsKeepMode = true;
    for ( QWidget* const input : inputWidgets )
    {
        input->show();
        inputsKeepMode = inputsKeepMode && activateAndFocus(input);
        for ( int key = Qt::Key_1; key <= Qt::Key_4; ++key )
        {
            sendKeyPress(input, key);
            sendKeyPress(input, key, Qt::ShiftModifier);
            inputsKeepMode = inputsKeepMode && window.editModeForTest() == beforeKeypad;
        }
        input->hide();
    }
    check(inputsKeepMode,
        "number-row shortcuts leave focused text, numeric, and editable combo controls in their current mode");

    {
        QSettings settings = settingsStore();
        settings.beginGroup(QStringLiteral("keymap/shortcuts"));
        settings.remove(QStringLiteral("_edit_mode_number_shortcut_layout_version"));
        settings.setValue(QStringLiteral("edit_mode.sector"), QStringList{ QStringLiteral("Ctrl+9") });
        settings.setValue(QStringLiteral("surface.submode.face"), QStringList{ QStringLiteral("Ctrl+Alt+9") });
    }
    MainWindow customWindow(nullptr, true);
    QAction* const customSector = customWindow.findChild<QAction*>(QStringLiteral("edit_mode.sector"));
    QAction* const customFace = customWindow.findChild<QAction*>(QStringLiteral("surface.submode.face"));
    check(customSector && customFace
            && customSector->shortcuts() == QList<QKeySequence>{ QKeySequence(QStringLiteral("Ctrl+9")) }
            && customFace->shortcuts() == QList<QKeySequence>{ QKeySequence(QStringLiteral("Ctrl+Alt+9")) },
        "numeric shortcut migration preserves customized major-mode and submode bindings exactly");
}

} // namespace

void runViewportShortcutTests()
{
    testNumberRowEditModesAndProjectSubmodes();
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.activateThingEditModeForTest();
    window.show();
    window.activateWindow();
    window.raise();
    QApplication::setActiveWindow(&window);
    QApplication::processEvents();

    GlView* const view = window.glViewForTest();
    QAction* const redoAction = window.findChild<QAction*>(
        QStringLiteral("edit.redo"));
    const QKeySequence redoShortcut(
        int(Qt::ControlModifier) | int(Qt::ShiftModifier) | Qt::Key_Z);
    check(redoAction && redoAction->shortcuts().contains(redoShortcut),
        "Redo uses the Ctrl+Shift+Z shortcut");

    const QAction* const thingFrameModeAction =
        window.findChild<QAction*>(
            QStringLiteral("thing.command.toggle_frame_mode"));
    const QAction* const surfaceSubmodeAction =
        window.findChild<QAction*>(
            QStringLiteral("surface.command.cycle_submode"));
    check(thingFrameModeAction
            && thingFrameModeAction->shortcuts().contains(QKeySequence(
                int(Qt::ShiftModifier) | Qt::Key_Tab)),
        "Thing frame mode retains Shift+Tab");
    check(surfaceSubmodeAction
            && surfaceSubmodeAction->shortcuts().contains(QKeySequence(
                int(Qt::ShiftModifier) | Qt::Key_Tab)),
        "Surface submode cycling retains Shift+Tab");

    QAction* const toggleWireframeOverlayAction = window.findChild<QAction*>(
        QStringLiteral("viewport.render.toggle_wireframe_overlay"));
    QAction* const wireframeOverlayAction = window.findChild<QAction*>(
        QStringLiteral("viewport.render.wireframe_overlay"));
    check(toggleWireframeOverlayAction
            && toggleWireframeOverlayAction->shortcuts().contains(
                QKeySequence(Qt::Key_Tab))
            && toggleWireframeOverlayAction->shortcutContext()
                == Qt::WidgetShortcut
            && view && view->actions().contains(toggleWireframeOverlayAction)
            && wireframeOverlayAction
            && wireframeOverlayAction->isCheckable()
            && wireframeOverlayAction->isChecked() == view->showWireframe(),
        "Plain Tab is a viewport-only wireframe-overlay shortcut");

    QAction* const wireframeAction = window.findChild<QAction*>(
        QStringLiteral("viewport.render.wireframe"));
    QAction* const texturedAction = window.findChild<QAction*>(
        QStringLiteral("viewport.render.textured"));
    QAction* const solidAction = window.findChild<QAction*>(
        QStringLiteral("viewport.render.solid"));
    QAction* const verticesAction = window.findChild<QAction*>(
        QStringLiteral("viewport.render.vertices"));
    if (!view || !wireframeAction || !texturedAction
        || !solidAction || !verticesAction || !wireframeOverlayAction)
    {
        return;
    }

    QPlainTextEdit textEditor(&window);
    textEditor.setGeometry(0, 0, 180, 50);
    textEditor.setTabChangesFocus(false);
    textEditor.show();
    const bool editorHasFocus = activateAndFocus(&textEditor);
    const bool solidBeforeEditorTab = view->showSolid();
    const bool texturesBeforeEditorTab = view->showTextures();
    const bool verticesBeforeEditorTab = view->showVertices();
    const bool wireframeBeforeEditorTab = view->showWireframe();
    const bool wireframeActionBeforeEditorTab =
        wireframeOverlayAction->isChecked();
    sendKeyPress(&textEditor, Qt::Key_Tab);
    QApplication::processEvents();
    check(editorHasFocus,
        "The text editor can receive focused input");
    const QString editorTextAfterTab = textEditor.toPlainText();
    if (editorTextAfterTab != QStringLiteral("\t"))
    {
        const QByteArray actualText = editorTextAfterTab.toUtf8();
        std::cerr << "Tab editor diagnostic: focus="
                  << (QApplication::focusWidget() == &textEditor)
                  << ", length=" << editorTextAfterTab.size()
                  << ", utf8_hex=" << actualText.toHex().constData()
                  << ", tab_changes_focus=" << textEditor.tabChangesFocus()
                  << "\n";
    }
    check(editorTextAfterTab == QStringLiteral("\t"),
        "Tab inserts a tab when a text editor has focus");
    check(view->showSolid() == solidBeforeEditorTab
            && view->showTextures() == texturesBeforeEditorTab
            && view->showVertices() == verticesBeforeEditorTab,
        "Tab in a focused text editor leaves viewport geometry unchanged");
    check(view->showWireframe() == wireframeBeforeEditorTab
            && wireframeOverlayAction->isChecked()
                == wireframeActionBeforeEditorTab,
        "Tab in a focused text editor leaves wireframe overlay unchanged");

    const bool viewportHasFocus = activateAndFocus(view);
    check(viewportHasFocus,
        "The viewport can receive focused shortcut input");

    const std::array<GeometryMode, 3> modes{
        GeometryMode{ verticesAction, false, false, true },
        GeometryMode{ solidAction, true, false, false },
        GeometryMode{ texturedAction, true, true, false },
    };
    const std::array<bool, 2> overlayStates{ false, true };
    for (const GeometryMode& mode : modes)
    {
        mode.action->trigger();
        QApplication::processEvents();
        const bool initialModeMatches = view->showSolid() == mode.solid
            && view->showTextures() == mode.textures
            && view->showVertices() == mode.vertices;
        check(initialModeMatches,
            "The viewport can enter its requested geometry presentation mode");

        for (const bool initialOverlayVisible : overlayStates)
        {
            wireframeOverlayAction->setChecked(initialOverlayVisible);
            QApplication::processEvents();
            const bool initialOverlayMatches =
                view->showWireframe() == initialOverlayVisible
                && wireframeOverlayAction->isChecked() == initialOverlayVisible;
            check(initialOverlayMatches,
                "The wireframe overlay action sets its requested visibility");

            sendKeyPress(view, Qt::Key_Tab);
            QApplication::processEvents();
            check(view->showSolid() == mode.solid
                    && view->showTextures() == mode.textures
                    && view->showVertices() == mode.vertices
                    && mode.action->isChecked()
                    && !wireframeAction->isChecked()
                    && view->showWireframe() == !initialOverlayVisible
                    && wireframeOverlayAction->isChecked()
                        == !initialOverlayVisible,
                "Tab toggles the wireframe overlay without changing geometry mode");

            sendKeyPress(view, Qt::Key_Tab);
            QApplication::processEvents();
            check(view->showSolid() == mode.solid
                    && view->showTextures() == mode.textures
                    && view->showVertices() == mode.vertices
                    && mode.action->isChecked()
                    && !wireframeAction->isChecked()
                    && view->showWireframe() == initialOverlayVisible
                    && wireframeOverlayAction->isChecked()
                        == initialOverlayVisible,
                "A second Tab restores prior overlay visibility and geometry mode");
        }
    }
}

} // namespace sed::app_tests
