#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/assets/sed.hpp"
#include "sed/level/geometry.hpp"
#include "test_common.hpp"
#include "ui/components/scrub_float_edit.hpp"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QLineEdit>
#include <QLayout>
#include <QMargins>
#include <QStyle>
#include <QToolButton>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace sed::app_tests
{
namespace
{

editor::DocumentState surfaceToolFixture()
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::JKDF2;
    state.level.header.version = 1;
    state.level.layers = { "Layer0" };
    Sector& sector = level::levelCreateCube(
        state.level, 0.0, 0.0, 0.0, TVector::up, TVector::right);
    sector.layer = 0;
    state.selection.surfaces.insert(SurfaceRef{ 0, 0 });
    return state;
}

void selectSurfaceForTool(MainWindow& window, const SurfaceRef ref)
{
    window.mutableDocumentStateForTest().selection.surfaces = { ref };
    window.updateEditActionsForTest();
    if ( GlView* const view = window.glViewForTest() )
    {
        view->setSelectionState(ref.sector, ref.surface, -1, -1,
            window.documentStateForTest().selection);
    }
}

editor::DocumentState frameToolFixture()
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.header.version = 3;
    state.level.layers = { "Layer0" };

    Thing thing{};
    thing.name = "frame-scale-capability-test";
    thing.templateName = thing.name;
    thing.layer = 0;
    thing.x = 1.0;
    thing.y = 2.0;
    thing.z = 3.0;

    TplValue frame{};
    frame.name = "frame";
    frame.setFrame(1.0, 2.0, 3.0, 0.0, 90.0, 0.0);
    thing.values.push_back(std::move(frame));
    state.level.things.push_back(std::move(thing));
    state.selection.things.insert(0);
    state.selection.frames.insert(level::FrameRef{ 0, 0 });
    return state;
}

editor::DocumentState ordinaryThingFixture(const bool twoThings)
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::JKDF2;
    state.level.header.version = 1;
    state.level.layers = { "Layer0" };

    for ( int index = 0; index < (twoThings ? 2 : 1); ++index )
    {
        Thing thing{};
        thing.name = twoThings
            ? (index == 0 ? "ordinary-thing-a" : "ordinary-thing-b")
            : "ordinary-thing";
        thing.templateName = thing.name;
        thing.layer = 0;
        thing.x = index == 0 ? 1.0 : 5.0;
        thing.y = 2.0;
        thing.z = 3.0;
        state.level.things.push_back(std::move(thing));
        state.selection.things.insert(index);
    }
    return state;
}

void checkScrubFieldGeometry(
    ui::ScrubFloatEdit* const edit,
    const char* const fieldName,
    const double expectedStep)
{
    check(edit != nullptr, fieldName);
    if ( !edit )
    {
        return;
    }

    QApplication::processEvents();

    const QString expectedText = QStringLiteral("-1000000.000000");

    const QList<QToolButton*> buttons = edit->findChildren<QToolButton*>();
    check(buttons.size() == 2,
        "Extrude scrub field owns exactly one decrease and one increase stepper");
    if ( buttons.size() != 2 )
    {
        return;
    }

    QEvent enter(QEvent::Enter);
    QApplication::sendEvent(edit->lineEdit(), &enter);
    QApplication::processEvents();
    check(std::all_of(buttons.begin(), buttons.end(), [](const QToolButton* button)
        {
            return button && button->isVisible();
        }),
        "Extrude scrub arrows appear on the actual field hover path");

    const QFontMetrics metrics(edit->lineEdit()->font());
    const int valueWidth = metrics.horizontalAdvance(expectedText);
    const QMargins margins = edit->lineEdit()->textMargins();
    const int frameWidth = edit->lineEdit()->style()->pixelMetric(
        QStyle::PM_DefaultFrameWidth, nullptr, edit->lineEdit());
    const int textAreaWidth = edit->lineEdit()->width()
        - margins.left() - margins.right() - 2 * frameWidth;
    check(textAreaWidth >= valueWidth,
        "Extrude scrub field leaves enough horizontal text area for its full fixed value");
    check(edit->lineEdit()->width() >= buttons[0]->width() + buttons[1]->width(),
        "Extrude scrub field keeps its numeric text area wider than the two arrows combined");
    check(std::isfinite(expectedStep) && expectedStep > 0.0
            && edit->singleStep() == expectedStep
            && edit->decimals() == 6,
        "Extrude scrub field exposes its computed step and display precision contract");
}

void checkCleaveAngleFieldGeometry(ui::ScrubFloatEdit* const edit)
{
    check(edit != nullptr,
        "Cleave Multi-cut options expose the angle scrub field");
    if ( !edit )
    {
        return;
    }

    edit->setValue(360.0);
    QApplication::processEvents();

    const QList<QToolButton*> buttons = edit->findChildren<QToolButton*>();
    check(buttons.size() == 2,
        "Cleave angle scrub owns exactly one decrease and one increase stepper");
    if ( buttons.size() != 2 )
    {
        return;
    }

    QEvent enter(QEvent::Enter);
    QApplication::sendEvent(edit->lineEdit(), &enter);
    edit->updateGeometry();
    if ( edit->layout() )
    {
        edit->layout()->activate();
    }
    QApplication::processEvents();
    if ( !std::all_of(buttons.begin(), buttons.end(), [](const QToolButton* button)
        {
            return button && button->isVisible();
        }) )
    {
        // The offscreen test cursor can be outside the synthetic field after
        // event delivery. Re-enter the same field before sampling its live
        // hover geometry, matching the user-visible interaction boundary.
        QApplication::sendEvent(edit->lineEdit(), &enter);
        edit->updateGeometry();
        if ( edit->layout() )
        {
            edit->layout()->activate();
        }
    }
    check(std::all_of(buttons.begin(), buttons.end(), [](const QToolButton* button)
        {
            return button && button->isVisible();
        }),
        "Cleave angle arrows appear on the actual field hover path");

    const QFontMetrics metrics(edit->lineEdit()->font());
    const int valueWidth = metrics.horizontalAdvance(QStringLiteral("360.0"));
    const QMargins margins = edit->lineEdit()->textMargins();
    const int frameWidth = edit->lineEdit()->style()->pixelMetric(
        QStyle::PM_DefaultFrameWidth, nullptr, edit->lineEdit());
    const int textAreaWidth = edit->lineEdit()->width()
        - margins.left() - margins.right() - 2 * frameWidth;
    check(edit->lineEdit()->text() == QStringLiteral("360.0")
            && textAreaWidth >= valueWidth,
        "Cleave angle field keeps 360.0 readable while its arrows are visible");
    const int arrowWidth = buttons[0]->width() + buttons[1]->width();
    const bool compactArrowLayout = textAreaWidth > arrowWidth;
    check(compactArrowLayout,
        "Cleave angle field keeps its numeric text area wider than both compact arrows");
    if ( !compactArrowLayout )
    {
        std::cerr << "Cleave angle scrub geometry: textAreaWidth=" << textAreaWidth
            << " arrowWidth=" << arrowWidth
            << " firstArrow=" << buttons[0]->width()
            << " secondArrow=" << buttons[1]->width()
            << " fieldWidth=" << edit->width()
            << " lineWidth=" << edit->lineEdit()->width() << "\n";
    }

    const QRect lineRect = edit->lineEdit()->geometry();
    bool childrenFit = edit->rect().contains(lineRect);
    bool childrenDoNotOverlap = true;
    for ( const QToolButton* const button : buttons )
    {
        if ( !button )
        {
            childrenFit = false;
            childrenDoNotOverlap = false;
            continue;
        }
        const QRect buttonRect = button->geometry();
        childrenFit = childrenFit && edit->rect().contains(buttonRect);
        childrenDoNotOverlap = childrenDoNotOverlap && !lineRect.intersects(buttonRect);
    }
    check(childrenFit && childrenDoNotOverlap,
        "Cleave angle field keeps its visible arrows and editor inside one non-overlapping layout");
    if ( !childrenFit || !childrenDoNotOverlap )
    {
        std::cerr << "Cleave angle scrub rectangles: field=" << edit->rect().width()
            << 'x' << edit->rect().height()
            << " line=(" << lineRect.x() << ',' << lineRect.y() << ','
            << lineRect.width() << 'x' << lineRect.height() << ')';
        for ( const QToolButton* const button : buttons )
        {
            if ( button )
            {
                const QRect buttonRect = button->geometry();
                std::cerr << " arrow=(" << buttonRect.x() << ',' << buttonRect.y()
                    << ',' << buttonRect.width() << 'x' << buttonRect.height() << ')';
            }
        }
        std::cerr << '\n';
    }

    buttons[0]->setDown(true);
    if ( edit->layout() )
    {
        edit->layout()->activate();
    }
    const int pressedTextAreaWidth = edit->lineEdit()->width()
        - margins.left() - margins.right() - 2 * frameWidth;
    const bool pressedArrowsVisible = std::all_of(buttons.begin(), buttons.end(),
        [](const QToolButton* button)
        {
            return button && button->isVisible();
        });
    check(pressedArrowsVisible
            && buttons[0]->isDown()
            && edit->lineEdit()->text() == QStringLiteral("360.0")
            && pressedTextAreaWidth >= valueWidth
            && pressedTextAreaWidth > arrowWidth,
        "Cleave angle field keeps 360.0 readable while an arrow is pressed");
    buttons[0]->setDown(false);

    edit->setValue(0.0);
    const QString exactAngle = QStringLiteral("360.0");
    edit->lineEdit()->setText(exactAngle);
    QMetaObject::invokeMethod(
        edit->lineEdit(), "textEdited", Qt::DirectConnection,
        Q_ARG(QString, exactAngle));
    QMetaObject::invokeMethod(
        edit->lineEdit(), "editingFinished", Qt::DirectConnection);
    check(edit->value() == 360.0 && edit->lineEdit()->text() == exactAngle,
        "Cleave angle field accepts an exact manual 360.0 entry");
}

} // namespace

void runDesignerUiPrecisionTests()
{
    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        QToolButton* const clearButton = window.findChild<QToolButton*>(
            QStringLiteral("cleaveCancelButton"));
        check(clearButton != nullptr,
            "Multi-cut options expose the clear control by its stable object name");
        if ( clearButton )
        {
            check(clearButton->text() == QStringLiteral("Clear"),
                "Multi-cut cancellation uses the fixed Clear label");
            check(clearButton->toolTip()
                    == QStringLiteral(
                        "<b>Clear cuts</b><br>Discard every staged cut line without changing the level. "
                        "Undo can restore lines cleared during this draft session."),
                "Multi-cut Clear retains the complete fixed cancellation tooltip");
        }
    }

    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.resize(1400, 900);
        window.setDocumentStateForTest(surfaceToolFixture());
        window.activateSurfaceEditModeForTest();
        window.activateSurfaceFaceSubModeForTest();
        selectSurfaceForTool(window, SurfaceRef{ 0, 0 });
        window.updateEditActionsForTest();
        window.show();
        QApplication::processEvents();

        ui::ScrubFloatEdit* const angleEdit = window.findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("cleaveAngleStepSpin"));
        QAction* const cleaveAction = window.findChild<QAction*>(
            QStringLiteral("viewport.surface.cleave"));
        QToolButton* const multiCutButton = window.findChild<QToolButton*>(
            QStringLiteral("cleaveMultiCutButton"));
        const double priorAngle = angleEdit ? angleEdit->value() : 0.0;
        const QString priorAngleText = angleEdit && angleEdit->lineEdit()
            ? angleEdit->lineEdit()->text()
            : QString{};
        const bool priorMultiCut = multiCutButton && multiCutButton->isChecked();
        check(cleaveAction && cleaveAction->isEnabled() && multiCutButton,
            "Cleave angle geometry fixture exposes an enabled Multi-cut control");
        if ( cleaveAction && cleaveAction->isEnabled() )
        {
            cleaveAction->trigger();
        }
        QApplication::processEvents();
        if ( multiCutButton && !multiCutButton->isChecked() )
        {
            multiCutButton->click();
        }
        QApplication::processEvents();

        checkCleaveAngleFieldGeometry(angleEdit);
        check(window.glViewForTest()
                && window.glViewForTest()->cleaveAngleStepDegrees() == 360.0,
            "Cleave angle field publishes the exact manual 360.0 value to the viewport");

        if ( angleEdit && !priorAngleText.isEmpty() )
        {
            angleEdit->lineEdit()->setText(priorAngleText);
            QMetaObject::invokeMethod(
                angleEdit->lineEdit(), "textEdited", Qt::DirectConnection,
                Q_ARG(QString, priorAngleText));
            QMetaObject::invokeMethod(
                angleEdit->lineEdit(), "editingFinished", Qt::DirectConnection);
        }
        if ( multiCutButton && multiCutButton->isChecked() != priorMultiCut )
        {
            multiCutButton->click();
        }
        QApplication::processEvents();
        check(angleEdit && angleEdit->value() == priorAngle
                && window.glViewForTest()
                    && window.glViewForTest()->cleaveAngleStepDegrees() == priorAngle
                && multiCutButton && multiCutButton->isChecked() == priorMultiCut,
            "Cleave angle fixture restores its prior settings before teardown");
    }

    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.resize(1400, 900);
        window.setDocumentStateForTest(surfaceToolFixture());
        window.activateSurfaceEditModeForTest();
        window.activateSurfaceFaceSubModeForTest();
        selectSurfaceForTool(window, SurfaceRef{ 0, 0 });
        window.updateEditActionsForTest();
        window.show();
        QApplication::processEvents();

        QAction* const extrudeAction = window.findChild<QAction*>(
            QStringLiteral("surface.extrude"));
        QAction* const gridSnapAction = window.findChild<QAction*>(
            QStringLiteral("viewport.transform.grid_snap"));
        check(extrudeAction && extrudeAction->isEnabled(),
            "Extrude scrub geometry fixture exposes an enabled face operation");
        if ( gridSnapAction )
        {
            gridSnapAction->setChecked(false);
        }
        check(gridSnapAction && !gridSnapAction->isChecked(),
            "Extrude scrub geometry fixture uses the unsnapped numeric contract");
        if ( extrudeAction && extrudeAction->isEnabled() )
        {
            extrudeAction->trigger();
        }
        QApplication::processEvents();

        auto* const lengthEdit = window.findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("surfaceExtrudeLengthSpin"));
        auto* const expandEdit = window.findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("surfaceExtrudeExpandSpin"));
        // The cube's source span is exactly one level unit. Expand therefore
        // follows the documented min-span / 64 rule: 1 / 64, while Length
        // keeps the fixed 1 / 8 step when Grid Snap is disabled.
        constexpr double expectedExpandStep = 1.0 / 64.0;
        checkScrubFieldGeometry(
            lengthEdit,
            "Extrude options expose the Length scrub field",
            0.125);
        checkScrubFieldGeometry(
            expandEdit,
            "Extrude options expose the Expand scrub field",
            expectedExpandStep);
        QToolButton* const cancelButton = window.findChild<QToolButton*>(
            QStringLiteral("surfaceExtrudeCancelButton"));
        const auto before = assets::saveToSedBytes(
            window.documentStateForTest().level);
        const std::size_t undoDepth = window.undoDepthForTest();
        if ( lengthEdit && expandEdit && cancelButton )
        {
            const QString exactLength = QStringLiteral("0.00000123456789");
            const QString exactExpand = QStringLiteral("-0.125");
            lengthEdit->lineEdit()->setText(exactLength);
            QMetaObject::invokeMethod(
                lengthEdit->lineEdit(), "textEdited", Qt::DirectConnection,
                Q_ARG(QString, exactLength));
            QMetaObject::invokeMethod(
                lengthEdit->lineEdit(), "editingFinished", Qt::DirectConnection);
            expandEdit->lineEdit()->setText(exactExpand);
            QMetaObject::invokeMethod(
                expandEdit->lineEdit(), "textEdited", Qt::DirectConnection,
                Q_ARG(QString, exactExpand));
            QMetaObject::invokeMethod(
                expandEdit->lineEdit(), "editingFinished", Qt::DirectConnection);
            check(lengthEdit->value() == 0.00000123456789
                    && expandEdit->value() == -0.125,
                "Extrude typed Length and Expand retain exact representable values");

            cancelButton->click();
            check(!extrudeAction || !extrudeAction->isChecked(),
                "Extrude Cancel clears the active preview action");
            check(window.undoDepthForTest() == undoDepth
                    && assets::saveToSedBytes(window.documentStateForTest().level)
                        == before,
                "Extrude Cancel leaves the Level bytes and history unchanged");
        }
        else
        {
            check(false,
                "Extrude scrub fixture exposes both fields and its Cancel control");
        }
    }

    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.setDocumentStateForTest(surfaceToolFixture());
        window.activateSurfaceEditModeForTest();
        window.activateSurfaceFaceSubModeForTest();
        selectSurfaceForTool(window, SurfaceRef{ 0, 0 });
        window.updateEditActionsForTest();

        QAction* const cleaveAction = window.findChild<QAction*>(
            QStringLiteral("viewport.surface.cleave"));
        QAction* const extrudeAction = window.findChild<QAction*>(
            QStringLiteral("surface.extrude"));
        check(cleaveAction && extrudeAction,
            "Surface Face mode exposes both modification actions");
        check(window.surfaceCleaveActionEnabledForTest(),
            "Surface Face mode enables Cleave for one valid face");
        check(window.surfaceExtrudeActionEnabledForTest(),
            "Surface Face mode enables Extrude for one valid face");

        if ( cleaveAction && cleaveAction->isEnabled() )
        {
            cleaveAction->trigger();
        }
        check(cleaveAction && cleaveAction->isChecked(),
            "Surface Face mode can arm the Cleave action before a submode switch");

        const auto beforeCleaveSwitch = assets::saveToSedBytes(
            window.documentStateForTest().level);
        const std::size_t cleaveUndoDepth = window.undoDepthForTest();
        window.activateSurfaceEdgeSubModeForTest();
        window.updateEditActionsForTest();
        check(cleaveAction && !cleaveAction->isChecked(),
            "Surface Edge mode cancels an armed Face Cleave independently of Edge availability");
        check(window.undoDepthForTest() == cleaveUndoDepth
                && assets::saveToSedBytes(window.documentStateForTest().level)
                    == beforeCleaveSwitch,
            "Switching from Face to Edge leaves the Cleave document and history unchanged");

        GlView* const edgeView = window.glViewForTest();
        if ( edgeView )
        {
            // Use the same point-selection seam as a viewport edge click,
            // then publish its owning face so action availability and the
            // transient edge set describe one target.
            edgeView->applySurfaceEdgePointSelectionForTest(
                SurfaceRef{ 0, 0 }, 0, Qt::NoModifier);
            selectSurfaceForTool(window, SurfaceRef{ 0, 0 });
        }
        window.updateEditActionsForTest();
        check(edgeView
                && edgeView->selectedSurfaceEdgeCountForTest() == 1
                && edgeView->activeSurfaceEdgeForTest()
                    == std::optional<std::pair<SurfaceRef, std::int32_t>>{
                        std::pair{ SurfaceRef{ 0, 0 }, 0 } }
                && window.documentStateForTest().selection.surfaces
                    == std::unordered_set<SurfaceRef,
                        editor::SurfaceRefHash,
                        editor::SurfaceRefEq>{ SurfaceRef{ 0, 0 } },
            "Surface Edge mode publishes one valid edge and its owning face for rearm");
        check(window.surfaceCleaveActionEnabledForTest(),
            "Surface Edge Cleave becomes available after a valid edge is selected");
        if ( cleaveAction && cleaveAction->isEnabled() )
        {
            cleaveAction->trigger();
        }
        check(cleaveAction && cleaveAction->isChecked(),
            "Surface Edge mode can explicitly rearm its supported immediate Cleave tool");
        window.activateSurfaceEdgeSubModeForTest();
        window.updateEditActionsForTest();
        check(cleaveAction && cleaveAction->isChecked(),
            "Reapplying the active Surface Edge mode preserves its armed Cleave tool");
        if ( cleaveAction && cleaveAction->isChecked() )
        {
            cleaveAction->trigger();
        }

        window.activateSurfaceFaceSubModeForTest();
        window.updateEditActionsForTest();
        check(window.surfaceCleaveActionEnabledForTest()
                && window.surfaceExtrudeActionEnabledForTest(),
            "Returning to Surface Face mode restores both face tools");
        if ( extrudeAction && extrudeAction->isEnabled() )
        {
            extrudeAction->trigger();
        }
        check(extrudeAction && extrudeAction->isChecked(),
            "Surface Face mode can arm Extrude independently of Cleave");

        const auto beforeExtrudeSwitch = assets::saveToSedBytes(
            window.documentStateForTest().level);
        const std::size_t extrudeUndoDepth = window.undoDepthForTest();
        window.activateSurfaceEdgeSubModeForTest();
        window.updateEditActionsForTest();
        check(extrudeAction && !extrudeAction->isChecked(),
            "Surface Edge mode cancels an armed Face Extrude independently of Edge availability");
        check(window.undoDepthForTest() == extrudeUndoDepth
                && assets::saveToSedBytes(window.documentStateForTest().level)
                    == beforeExtrudeSwitch,
            "Switching an Extrude preview to Edge leaves the document and history unchanged");

        window.activateSurfaceFaceSubModeForTest();
        window.updateEditActionsForTest();
        if ( cleaveAction && cleaveAction->isEnabled() )
        {
            cleaveAction->trigger();
        }
        check(cleaveAction && cleaveAction->isChecked(),
            "Surface Face mode can arm Cleave before a Vertex transition");
        const auto beforeCleaveVertexSwitch = assets::saveToSedBytes(
            window.documentStateForTest().level);
        const std::size_t cleaveVertexUndoDepth = window.undoDepthForTest();
        window.activateSurfaceVertexSubModeForTest();
        window.updateEditActionsForTest();
        check(cleaveAction && !cleaveAction->isChecked(),
            "Surface Vertex mode cancels an armed Face Cleave independently of tool availability");
        check(!window.surfaceCleaveActionEnabledForTest(),
            "Surface Vertex mode keeps Cleave unavailable");
        check(window.undoDepthForTest() == cleaveVertexUndoDepth
                && assets::saveToSedBytes(window.documentStateForTest().level)
                    == beforeCleaveVertexSwitch,
            "Switching a Cleave preview to Vertex leaves the document and history unchanged");

        window.activateSurfaceFaceSubModeForTest();
        window.updateEditActionsForTest();
        if ( extrudeAction && extrudeAction->isEnabled() )
        {
            extrudeAction->trigger();
        }
        check(extrudeAction && extrudeAction->isChecked(),
            "Surface Face mode can rearm Extrude before a Vertex transition");
        const auto beforeExtrudeVertexSwitch = assets::saveToSedBytes(
            window.documentStateForTest().level);
        const std::size_t extrudeVertexUndoDepth = window.undoDepthForTest();
        window.activateSurfaceVertexSubModeForTest();
        window.updateEditActionsForTest();
        check(extrudeAction && !extrudeAction->isChecked(),
            "Surface Vertex mode cancels an armed Face Extrude independently of Cleave availability");
        check(!window.surfaceCleaveActionEnabledForTest(),
            "Surface Vertex mode keeps Cleave unavailable after cancelling Extrude");
        check(window.undoDepthForTest() == extrudeVertexUndoDepth
                && assets::saveToSedBytes(window.documentStateForTest().level)
                    == beforeExtrudeVertexSwitch,
            "Switching an Extrude preview to Vertex leaves the document and history unchanged");
    }

    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.setDocumentStateForTest(ordinaryThingFixture(false));
        window.activateThingEditModeForTest();
        window.updateEditActionsForTest();

        QAction* const scaleAction = window.findChild<QAction*>(
            QStringLiteral("viewport.transform.scale"));
        GlView* const view = window.glViewForTest();
        check(scaleAction && view,
            "Thing mode exposes the real Scale action and viewport for capability checks");
        if ( scaleAction && view )
        {
            check(!scaleAction->isEnabled(),
                "A single ordinary Thing has no viewport Scale capability");
            window.dispatchEditorShortcutForTest(Qt::Key_E, Qt::NoModifier);
            check(!scaleAction->isChecked()
                    && view->transformTool() != GlView::TransformTool::Scale,
                "E cannot arm a viewport Scale gizmo for an ordinary Thing");
        }
    }

    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.setDocumentStateForTest(frameToolFixture());
        window.activateFrameEditModeForTest();
        window.updateEditActionsForTest();

        QAction* const scaleAction = window.findChild<QAction*>(
            QStringLiteral("viewport.transform.scale"));
        GlView* const view = window.glViewForTest();
        check(scaleAction && view && !scaleAction->isEnabled(),
            "Frame mode disables the non-committing viewport Scale gizmo");
        if ( scaleAction && view )
        {
            window.dispatchEditorShortcutForTest(Qt::Key_E, Qt::NoModifier);
            check(!scaleAction->isChecked()
                    && view->transformTool() != GlView::TransformTool::Scale,
                "E cannot arm a viewport Scale gizmo for a Frame target");
        }
    }

    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        editor::DocumentState state{};
        state.level.kind = ProjectType::JKDF2;
        state.level.header.version = 1;
        state.level.layers = { "Layer0" };
        level::levelCreateCube(
            state.level, 0.0, 0.0, 0.0, TVector::up, TVector::right).layer = 0;
        state.selection.sectors.insert(0);
        window.setDocumentStateForTest(std::move(state));
        window.activateSectorEditModeForTest();
        window.updateEditActionsForTest();
        QAction* const scaleAction = window.findChild<QAction*>(
            QStringLiteral("viewport.transform.scale"));
        GlView* const view = window.glViewForTest();
        check(scaleAction && scaleAction->isEnabled() && view,
            "Sector mode retains a committing viewport Scale capability");
        if ( scaleAction && scaleAction->isEnabled() && view )
        {
            window.dispatchEditorShortcutForTest(Qt::Key_E, Qt::NoModifier);
            check(scaleAction->isChecked()
                    && view->transformTool() == GlView::TransformTool::Scale,
                "E still arms the viewport Scale gizmo for selected geometry");
        }
    }
}

} // namespace sed::app_tests
