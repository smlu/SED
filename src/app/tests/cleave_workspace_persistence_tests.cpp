#include "test_common.hpp"

#include "app_settings.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/assets/sed.hpp"
#include "sed/editor/document.hpp"
#include "sed/level/build.hpp"
#include "ui/project/sed_editor_file_utils.hpp"
#include "ui/recovery/document_recovery_utils.hpp"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QScopeGuard>
#include <QSettings>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTimer>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sed::app_tests
{
namespace
{

editor::DocumentState makeCleaveDocument()
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::JKDF2;
    state.level.header.version = 1;
    level::levelCreateCube(
        state.level, 0.0, 0.0, 0.0, TVector::up, TVector::right);
    state.selection.sectors.insert(0);
    return state;
}

void prepareCleaveWindow(MainWindow& window)
{
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.activateSectorEditModeForTest();
    window.setDocumentStateForTest(makeCleaveDocument());
    window.activateSectorEditModeForTest();
    if (GlView* const view = window.glViewForTest())
    {
        view->setSelectionState(
            0, -1, -1, -1, window.documentStateForTest().selection);
        view->setSectorCleaveToolEnabled(true);
        view->setCleaveStagedMode(true);
        view->setCleaveChainEnabled(true);
        view->setCleaveBounded(false);
        window.updateEditActionsForTest();
        QAction* const cleaveAction = window.findChild<QAction*>(
            QStringLiteral("viewport.sector.cleave"));
        if (cleaveAction && cleaveAction->isEnabled()
            && !cleaveAction->isChecked())
        {
            cleaveAction->trigger();
        }
        view->captureStagedCleaveContextForTest(false);
        check(cleaveAction && cleaveAction->isChecked()
                && view->sectorCleaveToolEnabled(),
            "Cleave workspace fixture owns the checked viewport action");
    }
}

void prepareInteractiveCleaveWindow(MainWindow& window)
{
    prepareCleaveWindow(window);
    window.resize(1280, 900);
    GlView* const view = window.glViewForTest();
    if (!view)
    {
        return;
    }
    view->setLevel(
        &window.documentStateForTest().level,
        GlView::SetLevelMode::resetView);
    view->setSelectionState(
        0, -1, -1, -1, window.documentStateForTest().selection);
    view->setCameraProjectionType(
        sed::app::ViewportCamera::ProjectionType::Orthographic);
    view->resetEditorTopDownView();
    view->setCameraOrthoScale(16.0f);
    view->setShowGrid(false);
    view->setShowSolid(false);
    view->setShowTextures(false);
    view->setShowWireframe(false);
    view->setShowVertices(false);
    view->setShowFog(false);
    view->setShowThings(false);
    window.show();
    window.activateWindow();
    QApplication::processEvents(QEventLoop::AllEvents);
    window.updateEditActionsForTest();

    if (QToolButton* const multiCut = window.findChild<QToolButton*>(
            QStringLiteral("cleaveMultiCutButton")))
    {
        multiCut->setChecked(true);
    }
    if (QToolButton* const chain = window.findChild<QToolButton*>(
            QStringLiteral("cleaveChainButton")))
    {
        chain->setChecked(false);
    }
    if (QToolButton* const bounded = window.findChild<QToolButton*>(
            QStringLiteral("cleaveBoundedButton")))
    {
        bounded->setChecked(false);
    }
    if (QAction* const cleaveAction = window.findChild<QAction*>(
            QStringLiteral("viewport.sector.cleave"));
        cleaveAction && !cleaveAction->isChecked())
    {
        cleaveAction->setChecked(true);
    }
    view->captureStagedCleaveContextForTest(false);
    QApplication::setActiveWindow(&window);
    view->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents(QEventLoop::AllEvents);
}

void sendCleaveMouseEvent(
    QWidget& target,
    const QEvent::Type type,
    const QPointF& position,
    const Qt::MouseButton button,
    const Qt::MouseButtons buttons)
{
    const QPointF globalPosition = target.mapToGlobal(position.toPoint());
    QMouseEvent event(
        type, position, globalPosition, globalPosition, button, buttons,
        Qt::NoModifier);
    QApplication::sendEvent(&target, &event);
}

bool drawCleaveLineAtWorldPoints(
    GlView& view,
    const QVector3D& first,
    const QVector3D& second,
    const bool processPendingEvents = true)
{
    const std::optional<QPointF> firstScreen = view.projectWorldToViewport(first);
    const std::optional<QPointF> secondScreen = view.projectWorldToViewport(second);
    if (!firstScreen || !secondScreen)
    {
        return false;
    }
    sendCleaveMouseEvent(
        view, QEvent::MouseButtonPress, *firstScreen,
        Qt::LeftButton, Qt::LeftButton);
    sendCleaveMouseEvent(
        view, QEvent::MouseMove, *secondScreen,
        Qt::NoButton, Qt::LeftButton);
    sendCleaveMouseEvent(
        view, QEvent::MouseButtonRelease, *secondScreen,
        Qt::LeftButton, Qt::NoButton);
    if (processPendingEvents)
    {
        QApplication::processEvents(QEventLoop::AllEvents);
    }
    return true;
}

QCborMap editorRoot(const QByteArray& payload)
{
    const QCborValue value = QCborValue::fromCbor(payload);
    return value.isMap() ? value.toMap() : QCborMap{};
}

QCborArray viewportArray(const QCborMap& root)
{
    return root.value(QStringLiteral("viewport")).toArray();
}

QCborMap cleaveMap(const QCborMap& root)
{
    const QCborArray viewport = viewportArray(root);
    return viewport.size() > 38 && viewport.at(38).isMap()
        ? viewport.at(38).toMap()
        : QCborMap{};
}

QByteArray editorPayloadWithRoot(QCborMap root)
{
    return ui::recovery::serializeCanonicalCbor(QCborValue(std::move(root)));
}

QCborArray selectedLines(const std::initializer_list<qint64> indexes)
{
    QCborArray result{};
    for (const qint64 index : indexes)
    {
        result.append(index);
    }
    return result;
}

QCborArray linePrefix(const QCborArray& lines, const qsizetype count)
{
    QCborArray result{};
    for (qsizetype index = 0; index < count; ++index)
    {
        result.append(lines.at(index));
    }
    return result;
}

QCborMap draftSnapshot(
    const QCborArray& lines,
    const QCborArray& context,
    const QCborArray& origin,
    const QCborArray& order,
    const QCborArray& selectionLines,
    const qint64 active)
{
    return QCborMap{
        { QStringLiteral("lines"), lines },
        { QStringLiteral("context"), context },
        { QStringLiteral("chainOrigin"), origin },
        { QStringLiteral("applyOrder"), order },
        { QStringLiteral("selection"), QCborMap{
            { QStringLiteral("lines"), selectionLines },
            { QStringLiteral("active"), active },
        } },
    };
}

QByteArray cleaveWorkspacePayload(
    const QByteArray& captured,
    const bool withRedoBranch = true)
{
    QCborMap root = editorRoot(captured);
    QCborArray viewport = viewportArray(root);
    if (viewport.size() <= 38 || !viewport.at(38).isMap())
    {
        return {};
    }

    QCborMap cleave = viewport.at(38).toMap();
    const QCborArray lines = cleave.value(QStringLiteral("lines")).toArray();
    const QCborArray context = cleave.value(QStringLiteral("context")).toArray();
    QCborArray options = cleave.value(QStringLiteral("options")).toArray();
    if (lines.size() < (withRedoBranch ? 3 : 2) || options.size() != 8)
    {
        return {};
    }

    const QCborArray currentOrder{ qint64(2), qint64(0), qint64(1) };
    const QCborArray currentOrigin{ qint64(2), qint64(0) };
    const QCborArray currentSelected = selectedLines({ 0, 2 });
    constexpr qint64 currentActive = 2;
    options[5] = currentActive;

    const QCborMap baseline = draftSnapshot(
        linePrefix(lines, 1), context,
        QCborArray{ qint64(0), qint64(1) },
        selectedLines({ 0 }), selectedLines({ 0 }), 0);
    const QCborMap cursorState = draftSnapshot(
        lines, context, currentOrigin, currentOrder,
        selectedLines({ 1 }), 1);
    const QCborMap redoState = draftSnapshot(
        linePrefix(lines, 2), context,
        QCborArray{ qint64(1), qint64(0) },
        QCborArray{ qint64(1), qint64(0) },
        selectedLines({ 0, 1 }), 1);
    QCborArray snapshots{ baseline, cursorState };
    if (withRedoBranch)
    {
        snapshots.append(redoState);
    }

    cleave.insert(QStringLiteral("applyOrder"), currentOrder);
    cleave.insert(QStringLiteral("chainOrigin"), currentOrigin);
    cleave.insert(QStringLiteral("selection"), QCborMap{
        { QStringLiteral("lines"), currentSelected },
        { QStringLiteral("active"), currentActive },
    });
    cleave.insert(QStringLiteral("transformMode"), 1);
    cleave.insert(QStringLiteral("history"), QCborMap{
        { QStringLiteral("version"), 1 },
        { QStringLiteral("cursor"), 1 },
        { QStringLiteral("snapshots"), snapshots },
    });
    viewport[38] = cleave;
    root.insert(QStringLiteral("viewport"), viewport);

    QCborMap gridAppearance = root.value(QStringLiteral("gridAppearance")).toMap();
    gridAppearance.insert(QStringLiteral("axisColors"), QCborArray{
        qint64(QColor(241, 35, 48).rgba()),
        qint64(QColor(41, 217, 90).rgba()),
        qint64(QColor(54, 96, 238).rgba()),
    });
    root.insert(QStringLiteral("gridAppearance"), gridAppearance);
    return editorPayloadWithRoot(std::move(root));
}

bool sameLine(
    const GlView::RecoveryState::CleaveLine& left,
    const GlView::RecoveryState::CleaveLine& right)
{
    return left.first.x == right.first.x
        && left.first.y == right.first.y
        && left.first.z == right.first.z
        && left.second.x == right.second.x
        && left.second.y == right.second.y
        && left.second.z == right.second.z
        && left.planeNormal.x == right.planeNormal.x
        && left.planeNormal.y == right.planeNormal.y
        && left.planeNormal.z == right.planeNormal.z
        && left.constructionPlaneNormal.x == right.constructionPlaneNormal.x
        && left.constructionPlaneNormal.y == right.constructionPlaneNormal.y
        && left.constructionPlaneNormal.z == right.constructionPlaneNormal.z
        && left.angleReferenceAxis.x == right.angleReferenceAxis.x
        && left.angleReferenceAxis.y == right.angleReferenceAxis.y
        && left.angleReferenceAxis.z == right.angleReferenceAxis.z
        && left.linkedToPrevious == right.linkedToPrevious
        && left.linkedToNext == right.linkedToNext
        && left.authoredCamera.valid == right.authoredCamera.valid
        && left.authoredCamera.projectionType == right.authoredCamera.projectionType
        && left.authoredCamera.orbit.center == right.authoredCamera.orbit.center
        && left.authoredCamera.orbit.pan == right.authoredCamera.orbit.pan
        && left.authoredCamera.orbit.radius == right.authoredCamera.orbit.radius
        && left.authoredCamera.orbit.distance == right.authoredCamera.orbit.distance
        && left.authoredCamera.orbit.yawDeg == right.authoredCamera.orbit.yawDeg
        && left.authoredCamera.orbit.pitchDeg == right.authoredCamera.orbit.pitchDeg
        && left.authoredCamera.orbit.rollDeg == right.authoredCamera.orbit.rollDeg
        && left.authoredCamera.fovXDeg == right.authoredCamera.fovXDeg
        && left.authoredCamera.orthoScale == right.authoredCamera.orthoScale
        && left.authoredCamera.nearPlane == right.authoredCamera.nearPlane
        && left.authoredCamera.farPlane == right.authoredCamera.farPlane
        && left.authoredCamera.clipEnabled == right.authoredCamera.clipEnabled;
}

bool sameDraftSnapshot(
    const GlView::RecoveryState::CleaveDraftSnapshot& left,
    const GlView::RecoveryState::CleaveDraftSnapshot& right)
{
    if (left.lines.size() != right.lines.size()
        || left.applyOrder != right.applyOrder
        || left.selectedCleaveLines != right.selectedCleaveLines
        || left.selectedCleaveLine != right.selectedCleaveLine
        || left.chainOrigin.has_value() != right.chainOrigin.has_value()
        || left.context.valid != right.context.valid
        || left.context.surfaceMode != right.context.surfaceMode
        || left.context.viewportWidth != right.context.viewportWidth
        || left.context.viewportHeight != right.context.viewportHeight
        || left.context.sourceSurface.sector != right.context.sourceSurface.sector
        || left.context.sourceSurface.surface != right.context.sourceSurface.surface
        || left.context.targetSectors != right.context.targetSectors
        || left.context.targetSurfaces.size() != right.context.targetSurfaces.size())
    {
        return false;
    }
    if (left.chainOrigin
        && (left.chainOrigin->lineIndex != right.chainOrigin->lineIndex
            || left.chainOrigin->endpoint != right.chainOrigin->endpoint))
    {
        return false;
    }
    if (left.context.projection != right.context.projection
        || left.context.view != right.context.view)
    {
        return false;
    }
    for (std::size_t index = 0; index < left.lines.size(); ++index)
    {
        if (!sameLine(left.lines[index], right.lines[index]))
        {
            return false;
        }
    }
    for (std::size_t index = 0; index < left.context.targetSurfaces.size(); ++index)
    {
        if (left.context.targetSurfaces[index].sector
                != right.context.targetSurfaces[index].sector
            || left.context.targetSurfaces[index].surface
                != right.context.targetSurfaces[index].surface)
        {
            return false;
        }
    }
    return true;
}

bool sameCleaveHistory(
    const GlView::RecoveryState::CleaveHistory& left,
    const GlView::RecoveryState::CleaveHistory& right)
{
    if (left.cursor != right.cursor
        || left.snapshots.size() != right.snapshots.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.snapshots.size(); ++index)
    {
        if (!sameDraftSnapshot(left.snapshots[index], right.snapshots[index]))
        {
            return false;
        }
    }
    return true;
}

GlView::RecoveryState::CleaveDraftSnapshot currentDraft(
    const GlView::RecoveryState& state)
{
    return GlView::RecoveryState::CleaveDraftSnapshot{
        state.cleaveLines,
        state.cleaveContext,
        state.cleaveChainOrigin,
        state.applyOrder,
        state.selectedCleaveLines,
        state.selectedCleaveLine,
    };
}

bool sameSelection(const editor::Selection& left, const editor::Selection& right)
{
    return left.sectors == right.sectors
        && left.surfaces == right.surfaces
        && left.surfaceVertices == right.surfaceVertices
        && left.things == right.things
        && left.frames == right.frames
        && left.lights == right.lights
        && left.cogs == right.cogs;
}

std::optional<assets::SedFile> loadNativeFile(const std::filesystem::path& path)
{
    try
    {
        return assets::loadSedFile(path);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

QByteArray nativeEditorPayload(const assets::SedFile& file)
{
    return ui::project::findSedOptionalSection(
        file.optionalSections, ui::project::SedEditorStateSection);
}

int triggerActionAndDismissDialogs(QAction* action)
{
    int errorDialogCount{};
    QTimer driver{};
    driver.setInterval(5);
    QObject::connect(&driver, &QTimer::timeout, [&]()
    {
        if ( auto* chooser = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()) )
        {
            chooser->reject();
        }
        else if ( auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) )
        {
            ++errorDialogCount;
            message->accept();
        }
    });
    driver.start();
    if ( action )
    {
        action->trigger();
    }
    driver.stop();
    return errorDialogCount;
}

int saveAsToPath(QAction* action, const std::filesystem::path& path)
{
    int errorDialogCount{};
    bool acceptedSaveDialog = false;
    QTimer driver{};
    driver.setInterval(5);
    QObject::connect(&driver, &QTimer::timeout, [&]()
    {
        if (auto* chooser = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()))
        {
            if (!acceptedSaveDialog)
            {
                acceptedSaveDialog = true;
                chooser->selectFile(QString::fromStdWString(path.wstring()));
                QMetaObject::invokeMethod(
                    chooser, "accept", Qt::DirectConnection);
            }
        }
        else if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
        {
            ++errorDialogCount;
            message->accept();
        }
    });
    driver.start();
    if (action)
    {
        action->trigger();
    }
    driver.stop();
    return acceptedSaveDialog ? errorDialogCount : -1;
}


void activateCleaveFamily(GlView& view, const bool surfaceMode)
{
    view.setSectorCleaveToolEnabled(false);
    view.setSurfaceCleaveToolEnabled(false);
    editor::Selection targets{};
    if ( surfaceMode )
    {
        targets.surfaces.insert(SurfaceRef{ 0, 0 });
    }
    else
    {
        targets.sectors.insert(0);
    }
    view.setSelectionState(0, surfaceMode ? 0 : -1, -1, -1, targets);
    if ( surfaceMode )
    {
        view.setSurfaceCleaveToolEnabled(true);
    }
    else
    {
        view.setSectorCleaveToolEnabled(true);
    }
}

GlView::RecoveryState seedIndependentCleaveWorkspace(
    GlView& view, const bool surfaceMode)
{
    view.cancelStagedCleave();
    view.setCleaveStagedMode(true);
    view.setCleaveChainEnabled(surfaceMode);
    view.setCleaveBounded(!surfaceMode);
    view.setCleaveAnglesVisible(!surfaceMode);
    view.setCleaveLengthsVisible(surfaceMode);
    view.setCleaveLineNumbersVisible(surfaceMode);
    view.setCleaveMeasureUnit(surfaceMode
        ? GlView::CleaveMeasureUnit::Meters : GlView::CleaveMeasureUnit::Bare);
    view.setCleaveAngleStepDegrees(surfaceMode ? 15.0 : 30.0);
    auto shape = view.cleaveShapeOptions();
    shape.kind = ui::viewport::CleaveShapeKind::Rectangle;
    shape.width = surfaceMode ? 0.35 : 0.65;
    shape.height = surfaceMode ? 0.45 : 0.55;
    shape.dimensionsLinked = false;
    shape.segments = surfaceMode ? 9 : 13;
    shape.points = surfaceMode ? 6 : 8;
    shape.cameraPlane = !surfaceMode;
    shape.placementArmed = false;
    view.setCleaveShapeOptions(shape);
    view.captureStagedCleaveContextForTest(surfaceMode);
    const auto baseline = view.captureStagedCleaveDraftState();
    const float offset = surfaceMode ? 0.25f : 0.5f;
    view.addStagedCleaveLineForTest(
        QVector3D(0.0f, offset, 0.0f), QVector3D(1.0f, offset, 0.0f),
        QVector3D(0.0f, 1.0f, 0.0f));
    view.addStagedCleaveLineForTest(
        QVector3D(offset, 0.0f, 0.0f), QVector3D(offset, 1.0f, 0.0f),
        QVector3D(1.0f, 0.0f, 0.0f));
    auto current = view.captureStagedCleaveDraftState();
    current.applyOrder = { 1, 0 };
    current.selectedCleaveLines = { 0, 1 };
    current.selectedCleaveLine = 1;
    current.chainOrigin = GlView::RecoveryState::CleaveChainOrigin{ 0, 1 };
    auto future = current;
    future.lines[0].first.y += 0.125;
    future.lines[0].second.y += 0.125;
    GlView::RecoveryState::CleaveHistory history{};
    history.snapshots = { baseline, current, future };
    history.cursor = 1;
    view.restoreStagedCleaveDraftState(current, history);
    return view.recoveryState();
}

bool sameIndependentCleaveWorkspace(
    const GlView& view, const GlView::RecoveryState& expected)
{
    const auto history = view.captureStagedCleaveHistory();
    const auto& shape = view.cleaveShapeOptions();
    const auto& expectedShape = expected.cleaveShapeOptions;
    return sameDraftSnapshot(view.captureStagedCleaveDraftState(), currentDraft(expected))
        && history && expected.cleaveHistory
        && sameCleaveHistory(*history, *expected.cleaveHistory)
        && view.cleaveStagedMode() == expected.cleaveStagedMode
        && view.cleaveChainEnabled() == expected.cleaveChainEnabled
        && view.cleaveBounded() == expected.cleaveBounded.value_or(false)
        && view.cleaveAnglesVisible() == expected.cleaveAnglesVisible
        && view.cleaveLengthsVisible() == expected.cleaveLengthsVisible
        && view.cleaveLineNumbersVisible() == expected.cleaveLineNumbersVisible
        && view.cleaveMeasureUnit() == expected.cleaveMeasureUnit
        && view.cleaveAngleStepDegrees() == expected.cleaveAngleStepDegrees
        && view.stagedCleaveTransformMode() == expected.cleaveTransformMode
        && shape.kind == expectedShape.kind
        && shape.width == expectedShape.width && shape.height == expectedShape.height
        && shape.dimensionsLinked == expectedShape.dimensionsLinked
        && shape.segments == expectedShape.segments && shape.points == expectedShape.points
        && shape.cameraPlane == expectedShape.cameraPlane
        && shape.rotationDegrees == expectedShape.rotationDegrees
        && shape.placementArmed == expectedShape.placementArmed;
}

void runIndependentCleaveWorkspaceSwitchTests()
{
    for ( const bool surfaceFirst : { false, true } )
    {
        auto document = makeCleaveDocument();
        GlView view{};
        view.setLevel(&document.level);
        activateCleaveFamily(view, surfaceFirst);
        const auto first = seedIndependentCleaveWorkspace(view, surfaceFirst);
        check(first.cleaveContext.valid
                && first.cleaveContext.surfaceMode == surfaceFirst
                && first.cleaveHistory && first.cleaveHistory->snapshots.size() == 3
                && first.cleaveHistory->cursor == 1
                && view.isStagedCleaveInteractionActive()
                && view.canUndoStagedCleaveHistory() && view.canRedoStagedCleaveHistory(),
            "independent Cleave fixture begins with matching captured family and local Undo/Redo branch");

        activateCleaveFamily(view, !surfaceFirst);
        check(view.stagedCleaveLineCount() == 0
                && !view.captureStagedCleaveDraftState().context.valid
                && !view.captureStagedCleaveHistory(),
            "switching Cleave family opens its empty independent workspace rather than another family's pending planes");
        auto second = seedIndependentCleaveWorkspace(view, !surfaceFirst);
        activateCleaveFamily(view, surfaceFirst);
        check(sameIndependentCleaveWorkspace(view, first)
                && view.isStagedCleaveInteractionActive()
                && view.canUndoStagedCleaveHistory() && view.canRedoStagedCleaveHistory(),
            "returning to the first Cleave family restores exact planes, context, origin, order, selection, options and local timeline");
        check(view.undoStagedCleaveHistory()
                && sameDraftSnapshot(view.captureStagedCleaveDraftState(),
                    first.cleaveHistory->snapshots[0])
                && view.redoStagedCleaveHistory()
                && sameIndependentCleaveWorkspace(view, first),
            "local Undo and Redo after a family switch navigate only that family's retained draft timeline");

        activateCleaveFamily(view, !surfaceFirst);
        check(sameIndependentCleaveWorkspace(view, second),
            "the other Cleave family remains exact after local Undo/Redo in its peer");
        view.clearStagedCleaveDraft();
        check(view.stagedCleaveLineCount() == 0
                && view.undoStagedCleaveHistory()
                && sameDraftSnapshot(view.captureStagedCleaveDraftState(), currentDraft(second))
                && view.canRedoStagedCleaveHistory(),
            "Clear is an undoable edit of only the current Cleave workspace");
        // Clear replaces this family's former Redo branch with the clear edit.
        second = view.recoveryState();
        activateCleaveFamily(view, surfaceFirst);
        check(sameIndependentCleaveWorkspace(view, first),
            "clearing and undoing the peer workspace cannot discard the first family's pending planes or options");
        view.setSectorCleaveToolEnabled(false);
        view.setSurfaceCleaveToolEnabled(false);
        check(!view.stagedCleaveHistoryOwnsUndo()
                && !view.undoStagedCleaveHistory() && !view.redoStagedCleaveHistory(),
            "both retained Cleave workspaces stay dormant without capturing local history commands when their tools are disabled");
        activateCleaveFamily(view, !surfaceFirst);
        check(sameIndependentCleaveWorkspace(view, second),
            "ordinary tool deactivation preserves the peer family's complete workspace for a later return");
    }
}


void runIndependentCleaveAuthoredProjectionTests()
{
    MainWindow window{};
    prepareCleaveWindow(window);
    GlView* const view = window.glViewForTest();
    if ( !view )
    {
        check(false, "independent Cleave dirty fixture has a viewport");
        return;
    }
    const auto sector = seedIndependentCleaveWorkspace(*view, false);
    activateCleaveFamily(*view, true);
    const auto surface = seedIndependentCleaveWorkspace(*view, true);
    window.markCurrentDocumentAsSavedForTest();
    check(!window.hasUnsavedChangesForTest(),
        "saving an independent Cleave fixture checkpoints both settled family drafts as clean");
    activateCleaveFamily(*view, false);
    // A settled publication invalidates the revision cache without changing
    // either family's authored data, so the stable family projection is tested.
    view->restoreStagedCleaveDraftState(currentDraft(sector), *sector.cleaveHistory);
    check(!window.hasUnsavedChangesForTest()
            && !window.hasRecoveryOfferEligibleChangesForTest(),
        "switching and republishing an unchanged family cannot dirty two saved independent Cleave workspaces");
    window.markCurrentDocumentAsSavedForTest();
    auto parkedSurface = view->captureStagedCleaveWorkspace(GlView::CleaveWorkspaceFamily::Surface);
    parkedSurface.draft.lines[0].first.y += 0.03125;
    parkedSurface.draft.lines[0].second.y += 0.03125;
    parkedSurface.history->snapshots[parkedSurface.history->cursor] = parkedSurface.draft;
    const bool restoredPeer = view->restoreStagedCleaveWorkspace(
        GlView::CleaveWorkspaceFamily::Surface, std::move(parkedSurface));
    check(restoredPeer && sameIndependentCleaveWorkspace(*view, sector),
        "a parked-family authored change leaves the active family's complete draft and timeline untouched");
    check(window.hasUnsavedChangesForTest()
            && window.hasRecoveryOfferEligibleChangesForTest(),
        "authored changes confined to the parked Cleave family participate in dirty state and recovery offers");
    activateCleaveFamily(*view, true);
    view->restoreStagedCleaveDraftState(currentDraft(surface), *surface.cleaveHistory);
    check(!window.hasUnsavedChangesForTest(),
        "restoring the parked family's saved authored draft returns the complete independent workspace to clean state");
}

QCborMap cleaveMapWithPeer(
    QCborMap current, QCborMap peer, const bool surfaceCurrent)
{
    current.remove(QStringLiteral("peerWorkspace"));
    peer.remove(QStringLiteral("peerWorkspace"));
    current.insert(QStringLiteral("currentFamily"), surfaceCurrent ? 1 : 0);
    peer.insert(QStringLiteral("currentFamily"), surfaceCurrent ? 0 : 1);
    peer.remove(QStringLiteral("peerWorkspace"));
    QCborArray peerOptions = peer.value(QStringLiteral("options")).toArray();
    peerOptions[6] = false;
    peerOptions[7] = false;
    peer.insert(QStringLiteral("options"), peerOptions);
    current.insert(QStringLiteral("peerWorkspace"), peer);
    return current;
}

QByteArray editorPayloadWithCleave(const QByteArray& source, const QCborMap& cleave)
{
    QCborMap root = editorRoot(source);
    QCborArray viewport = viewportArray(root);
    viewport[38] = cleave;
    root.insert(QStringLiteral("viewport"), viewport);
    return editorPayloadWithRoot(std::move(root));
}

void runIndependentCleaveMapAdmissionTests()
{
    MainWindow sectorWindow{};
    MainWindow surfaceWindow{};
    prepareCleaveWindow(sectorWindow);
    prepareCleaveWindow(surfaceWindow);
    GlView* const sectorView = sectorWindow.glViewForTest();
    GlView* const surfaceView = surfaceWindow.glViewForTest();
    if ( !sectorView || !surfaceView )
    {
        check(false, "independent Cleave map admission fixtures own both viewports");
        return;
    }
    seedIndependentCleaveWorkspace(*sectorView, false);
    activateCleaveFamily(*surfaceView, true);
    seedIndependentCleaveWorkspace(*surfaceView, true);
    const QByteArray sourcePayload = sectorWindow.captureRecoveryEditorStateForTest();
    const QCborMap peer = cleaveMap(editorRoot(surfaceWindow.captureRecoveryEditorStateForTest()));
    const QCborMap dual = cleaveMapWithPeer(cleaveMap(editorRoot(sourcePayload)), peer, false);
    const QByteArray validPayload = editorPayloadWithCleave(sourcePayload, dual);
    QString warning{};
    check(sectorWindow.validateRecoveryEditorStateForTest(validPayload, &warning)
            && warning.isEmpty(),
        "a writer-shaped current Sector workspace plus independent dormant Surface workspace is admitted");
    const QByteArray before = sectorWindow.captureRecoveryEditorStateForTest();
    std::vector<QCborMap> malformed{};
    for ( const QCborValue& family : { QCborValue(true), QCborValue(2),
             QCborValue(QStringLiteral("Sector")), QCborValue(QCborValue::Null) } )
    {
        QCborMap candidate = dual;
        candidate.insert(QStringLiteral("currentFamily"), family);
        malformed.push_back(std::move(candidate));
    }
    for ( const QCborValue& value : { QCborValue(false), QCborValue(QCborArray{}),
             QCborValue(QCborMap{}), QCborValue(QCborValue::Null) } )
    {
        QCborMap candidate = dual;
        candidate.insert(QStringLiteral("peerWorkspace"), value);
        malformed.push_back(std::move(candidate));
    }
    for ( const QCborValue& family : { QCborValue(0), QCborValue(true), QCborValue(2) } )
    {
        QCborMap candidate = dual;
        QCborMap invalidPeer = candidate.value(QStringLiteral("peerWorkspace")).toMap();
        invalidPeer.insert(QStringLiteral("currentFamily"), family);
        candidate.insert(QStringLiteral("peerWorkspace"), invalidPeer);
        malformed.push_back(std::move(candidate));
    }
    {
        QCborMap candidate = dual;
        QCborMap invalidPeer = candidate.value(QStringLiteral("peerWorkspace")).toMap();
        QCborArray context = invalidPeer.value(QStringLiteral("context")).toArray();
        context[7] = 99;
        context[9] = QCborArray{ QCborArray{ 0, 99 } };
        invalidPeer.insert(QStringLiteral("context"), context);
        candidate.insert(QStringLiteral("peerWorkspace"), invalidPeer);
        malformed.push_back(std::move(candidate));
    }
    {
        QCborMap candidate = dual;
        QCborMap invalidPeer = candidate.value(QStringLiteral("peerWorkspace")).toMap();
        QCborArray context = invalidPeer.value(QStringLiteral("context")).toArray();
        context[1] = false;
        invalidPeer.insert(QStringLiteral("context"), context);
        candidate.insert(QStringLiteral("peerWorkspace"), invalidPeer);
        malformed.push_back(std::move(candidate));
    }
    for ( const QCborValue& rotation : { QCborValue(QStringLiteral("30")),
             QCborValue(std::numeric_limits<double>::infinity()),
             QCborValue(std::numeric_limits<double>::quiet_NaN()) } )
    {
        QCborMap candidate = dual;
        QCborMap invalidPeer = candidate.value(QStringLiteral("peerWorkspace")).toMap();
        QCborMap shape = invalidPeer.value(QStringLiteral("shape")).toMap();
        shape.insert(QStringLiteral("rotationDegrees"), rotation);
        invalidPeer.insert(QStringLiteral("shape"), shape);
        candidate.insert(QStringLiteral("peerWorkspace"), invalidPeer);
        malformed.push_back(std::move(candidate));
    }
    for ( const auto& candidate : malformed )
    {
        const QByteArray malformedPayload = editorPayloadWithCleave(sourcePayload, candidate);
        check(!sectorWindow.validateRecoveryEditorStateForTest(malformedPayload, &warning)
                && !sectorWindow.restoreRecoveryEditorStateForTest(malformedPayload, &warning)
                && sectorWindow.captureRecoveryEditorStateForTest() == before,
            "malformed family identity, peer data, dormant targets or shape rotation rejects complete EDIT before publishing any workspace");
    }
    MainWindow restored{};
    prepareCleaveWindow(restored);
    const bool restoredBoth = restored.restoreRecoveryEditorStateForTest(validPayload, &warning);
    const QCborMap restoredDual = cleaveMap(editorRoot(restored.captureRecoveryEditorStateForTest()));
    const QByteArray expectedBytes = ui::recovery::serializeCanonicalCbor(QCborValue(dual));
    const QByteArray restoredBytes = ui::recovery::serializeCanonicalCbor(QCborValue(restoredDual));
    if ( !restoredBoth || restoredBytes != expectedBytes )
    {
        std::cerr << "dual EDIT restore=" << restoredBoth
                  << " warning=" << warning.toStdString()
                  << " expectedBytes=" << expectedBytes.size()
                  << " restoredBytes=" << restoredBytes.size() << '\n';
    }
    // A composed QCborMap can retain insertion order. Compare the complete
    // canonical bytes so every field remains exact independent of that order.
    check(restoredBoth && restoredBytes == expectedBytes,
        "EDIT restores and captures both exact Cleave workspaces without dropping dormant state");
}

void runCleaveEditRoundTripTests(const std::filesystem::path& root)
{
    MainWindow window{};
    prepareCleaveWindow(window);
    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "Cleave EDIT persistence fixture owns a live viewport");
    if (!view)
    {
        return;
    }

    view->addStagedCleaveLineForTest(
        QVector3D(-2.0f, 0.0f, 0.0f), QVector3D(2.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 1.0f, 0.0f));
    view->addStagedCleaveLineForTest(
        QVector3D(0.0f, -2.0f, 0.0f), QVector3D(0.0f, 2.0f, 0.0f),
        QVector3D(1.0f, 0.0f, 0.0f));
    view->addStagedCleaveLineForTest(
        QVector3D(-2.0f, -2.0f, 0.0f), QVector3D(2.0f, 2.0f, 0.0f),
        QVector3D(-1.0f, 1.0f, 0.0f));
    const QColor axisX(241, 35, 48);
    const QColor axisY(41, 217, 90);
    const QColor axisZ(54, 96, 238);
    view->setGridAxisXColor(axisX);
    view->setGridAxisYColor(axisY);
    view->setGridAxisZColor(axisZ);

    const QByteArray captured = window.captureRecoveryEditorStateForTest();
    const QByteArray extended = cleaveWorkspacePayload(captured);
    check(!extended.isEmpty(),
        "Cleave EDIT persistence fixture creates a three-line workspace payload");
    if (extended.isEmpty())
    {
        return;
    }

    QString warning{};
    check(window.validateRecoveryEditorStateForTest(extended, &warning)
            && warning.isEmpty(),
        "Cleave EDIT accepts an ordered, multi-selected draft with a retained Undo/Redo branch");
    check(window.restoreRecoveryEditorStateForTest(extended, &warning)
            && warning.isEmpty(),
        "Cleave EDIT restores the accepted draft and its retained timeline");

    const GlView::RecoveryState restored = view->recoveryState();
    check(restored.applyOrder == std::vector<std::int32_t>{ 2, 0, 1 }
            && restored.selectedCleaveLines == std::vector<std::int32_t>{ 0, 2 }
            && restored.selectedCleaveLine == 2
            && restored.cleaveHistory
            && restored.cleaveHistory->cursor == 1
            && restored.cleaveHistory->snapshots.size() == 3
            && restored.cleaveTransformMode
                == GlView::StagedCleaveTransformMode::Rotate,
        "Cleave EDIT restores execution order, multi-selection, active line, transform mode, and both history branches");
    if (!restored.cleaveHistory)
    {
        return;
    }
    check(restored.gridAxisXColor == axisX
            && restored.gridAxisYColor == axisY
            && restored.gridAxisZColor == axisZ,
        "Cleave EDIT restores all three custom grid-axis colors");

    const QCborMap recaptured = editorRoot(window.captureRecoveryEditorStateForTest());
    const QCborMap recapturedCleave = cleaveMap(recaptured);
    const QCborMap recapturedSelection = recapturedCleave.value(
        QStringLiteral("selection")).toMap();
    const QCborArray recapturedOrder = recapturedCleave.value(
        QStringLiteral("applyOrder")).toArray();
    const QCborMap recapturedHistory = recapturedCleave.value(
        QStringLiteral("history")).toMap();
    check(recapturedOrder == QCborArray{ qint64(2), qint64(0), qint64(1) }
            && recapturedSelection.value(QStringLiteral("lines")).toArray()
                == selectedLines({ 0, 2 })
            && recapturedSelection.value(QStringLiteral("active")).toInteger() == 2
            && recapturedHistory.value(QStringLiteral("cursor")).toInteger() == 1
            && recapturedHistory.value(QStringLiteral("snapshots")).toArray().size() == 3,
        "Cleave EDIT writer emits the restored order, physical selection, and full local timeline");
    const QCborArray recapturedAxes = recaptured.value(
        QStringLiteral("gridAppearance")).toMap()
            .value(QStringLiteral("axisColors")).toArray();
    check(recapturedAxes == QCborArray{
                qint64(axisX.rgba()), qint64(axisY.rgba()), qint64(axisZ.rgba()) },
        "Cleave EDIT writer emits the current X/Y/Z grid-axis colors");

    QCborMap malformedRoot = editorRoot(extended);
    QCborMap malformedGrid = malformedRoot.value(
        QStringLiteral("gridAppearance")).toMap();
    malformedGrid.insert(QStringLiteral("axisColors"), QCborArray{ qint64(axisX.rgba()) });
    malformedRoot.insert(QStringLiteral("gridAppearance"), malformedGrid);
    const QByteArray malformedAxes = editorPayloadWithRoot(std::move(malformedRoot));
    const QByteArray beforeMalformedRestore = window.captureRecoveryEditorStateForTest();
    check(!window.validateRecoveryEditorStateForTest(malformedAxes, &warning)
            && !window.restoreRecoveryEditorStateForTest(malformedAxes)
            && window.captureRecoveryEditorStateForTest() == beforeMalformedRestore,
        "malformed grid-axis arrays reject before changing the live viewport or Cleave timeline");

    const auto rejectCleaveMutationAtomically = [&](auto mutate)
    {
        QCborMap malformedRoot = editorRoot(extended);
        QCborArray malformedViewport = viewportArray(malformedRoot);
        QCborMap malformedCleave = malformedViewport.at(38).toMap();
        mutate(malformedCleave);
        malformedViewport[38] = malformedCleave;
        malformedRoot.insert(QStringLiteral("viewport"), malformedViewport);
        const QByteArray malformed = editorPayloadWithRoot(
            std::move(malformedRoot));
        const QByteArray liveBefore = window.captureRecoveryEditorStateForTest();
        QString malformedWarning{};
        const bool valid = window.validateRecoveryEditorStateForTest(
            malformed, &malformedWarning);
        const bool restoredMalformed =
            window.restoreRecoveryEditorStateForTest(malformed);
        return !valid && !restoredMalformed
            && window.captureRecoveryEditorStateForTest() == liveBefore;
    };
    check(rejectCleaveMutationAtomically([](QCborMap& malformedCleave)
        {
            QCborMap selection = malformedCleave.value(
                QStringLiteral("selection")).toMap();
            selection.insert(QStringLiteral("lines"), selectedLines({ 2, 0 }));
            malformedCleave.insert(QStringLiteral("selection"), selection);
        }),
        "noncanonical unsorted Cleave physical selection rejects atomically");
    check(rejectCleaveMutationAtomically([](QCborMap& malformedCleave)
        {
            malformedCleave.insert(QStringLiteral("applyOrder"),
                selectedLines({ 0, 0, 1 }));
        }),
        "duplicate Cleave apply-order indices reject atomically");
    check(rejectCleaveMutationAtomically([](QCborMap& malformedCleave)
        {
            malformedCleave.insert(QStringLiteral("applyOrder"),
                selectedLines({ 0, 1, 3 }));
        }),
        "out-of-range Cleave apply-order indices reject atomically");
    check(rejectCleaveMutationAtomically([](QCborMap& malformedCleave)
        {
            malformedCleave.insert(QStringLiteral("applyOrder"),
                selectedLines({ 0, 1 }));
        }),
        "Cleave apply-order length mismatch rejects atomically");
    check(rejectCleaveMutationAtomically([](QCborMap& malformedCleave)
        {
            const QCborArray context = malformedCleave.value(
                QStringLiteral("context")).toArray();
            QCborArray invalidContext = context;
            invalidContext[8] = selectedLines({ 0, 0 });
            malformedCleave.insert(QStringLiteral("context"), invalidContext);
            QCborArray snapshots = malformedCleave.value(
                QStringLiteral("history")).toMap()
                    .value(QStringLiteral("snapshots")).toArray();
            for (qsizetype index = 0; index < snapshots.size(); ++index)
            {
                QCborMap snapshot = snapshots.at(index).toMap();
                snapshot.insert(QStringLiteral("context"), invalidContext);
                snapshots[index] = snapshot;
            }
            QCborMap history = malformedCleave.value(
                QStringLiteral("history")).toMap();
            history.insert(QStringLiteral("snapshots"), snapshots);
            malformedCleave.insert(QStringLiteral("history"), history);
        }),
        "duplicate target Sector references in current and historical Cleave states reject atomically");
    check(rejectCleaveMutationAtomically([](QCborMap& malformedCleave)
        {
            QCborArray context = malformedCleave.value(
                QStringLiteral("context")).toArray();
            context[8] = selectedLines({ 999 });
            malformedCleave.insert(QStringLiteral("context"), context);
            QCborArray snapshots = malformedCleave.value(
                QStringLiteral("history")).toMap()
                    .value(QStringLiteral("snapshots")).toArray();
            for (qsizetype index = 0; index < snapshots.size(); ++index)
            {
                QCborMap snapshot = snapshots.at(index).toMap();
                snapshot.insert(QStringLiteral("context"), context);
                snapshots[index] = snapshot;
            }
            QCborMap history = malformedCleave.value(
                QStringLiteral("history")).toMap();
            history.insert(QStringLiteral("snapshots"), snapshots);
            malformedCleave.insert(QStringLiteral("history"), history);
        }),
        "out-of-Level Cleave target references in current and historical states reject atomically");
    check(rejectCleaveMutationAtomically([](QCborMap& malformedCleave)
        {
            QCborArray lines = malformedCleave.value(
                QStringLiteral("lines")).toArray();
            QCborArray line = lines.at(0).toArray();
            line[2] = QCborArray{ 0.0, 0.0, 0.0 };
            lines[0] = line;
            malformedCleave.insert(QStringLiteral("lines"), lines);
            QCborMap history = malformedCleave.value(
                QStringLiteral("history")).toMap();
            QCborArray snapshots = history.value(
                QStringLiteral("snapshots")).toArray();
            for (qsizetype index = 0; index < snapshots.size(); ++index)
            {
                QCborMap snapshot = snapshots.at(index).toMap();
                QCborArray snapshotLines = snapshot.value(
                    QStringLiteral("lines")).toArray();
                QCborArray snapshotLine = snapshotLines.at(0).toArray();
                snapshotLine[2] = QCborArray{ 0.0, 0.0, 0.0 };
                snapshotLines[0] = snapshotLine;
                snapshot.insert(QStringLiteral("lines"), snapshotLines);
                snapshots[index] = snapshot;
            }
            history.insert(QStringLiteral("snapshots"), snapshots);
            malformedCleave.insert(QStringLiteral("history"), history);
        }),
        "zero-normal current and future Cleave lines reject atomically");
    check(rejectCleaveMutationAtomically([](QCborMap& malformedCleave)
        {
            const auto collapseFirstLine = [](QCborArray lines)
            {
                QCborArray line = lines.at(0).toArray();
                line[1] = line.at(0);
                lines[0] = line;
                return lines;
            };
            malformedCleave.insert(QStringLiteral("lines"), collapseFirstLine(
                malformedCleave.value(QStringLiteral("lines")).toArray()));
            QCborMap history = malformedCleave.value(
                QStringLiteral("history")).toMap();
            QCborArray snapshots = history.value(
                QStringLiteral("snapshots")).toArray();
            QCborMap cursor = snapshots.at(1).toMap();
            cursor.insert(QStringLiteral("lines"), collapseFirstLine(
                cursor.value(QStringLiteral("lines")).toArray()));
            snapshots[1] = cursor;
            history.insert(QStringLiteral("snapshots"), snapshots);
            malformedCleave.insert(QStringLiteral("history"), history);
        }),
        "collapsed current Cleave segments reject atomically");

    const std::filesystem::path nativePath = root / "cleave-workspace.sed";
    window.setCurrentPathForTitleForTest(nativePath);
    QAction* const saveAction = window.findChild<QAction*>(QStringLiteral("file.save"));
    check(saveAction != nullptr,
        "Cleave EDIT native roundtrip fixture exposes the production Save action");
    if (saveAction)
    {
        const int errorDialogCount = triggerActionAndDismissDialogs(saveAction);
        check(errorDialogCount == 0,
            "native Save accepts the current Cleave EDIT workspace without a modal error");
    }
    const std::optional<assets::SedFile> saved = loadNativeFile(nativePath);
    const QByteArray savedEdit = saved ? nativeEditorPayload(*saved) : QByteArray{};
    MainWindow reopened{};
    reopened.setAttribute(Qt::WA_DontShowOnScreen);
    reopened.setRecoveryDirectoryForTest(root / "native-reopen-recovery");
    reopened.openLevelFromFilenameForTest(QString::fromStdWString(nativePath.wstring()));
    const GlView* const reopenedView = reopened.glViewForTest();
    const GlView::RecoveryState reopenedState = reopenedView
        ? reopenedView->recoveryState() : GlView::RecoveryState{};
    const GlView::RecoveryState::CleaveDraftSnapshot reopenedDraft =
        currentDraft(reopenedState);
    bool nativeLinesMatch = reopenedDraft.lines.size() == restored.cleaveLines.size();
    for (std::size_t index = 0;
         nativeLinesMatch && index < reopenedDraft.lines.size(); ++index)
    {
        nativeLinesMatch = sameLine(
            reopenedDraft.lines[index], restored.cleaveLines[index]);
    }
    const bool nativeContextMatches =
        reopenedDraft.context.valid == restored.cleaveContext.valid
        && reopenedDraft.context.surfaceMode == restored.cleaveContext.surfaceMode
        && reopenedDraft.context.projection == restored.cleaveContext.projection
        && reopenedDraft.context.view == restored.cleaveContext.view
        && reopenedDraft.context.viewportWidth == restored.cleaveContext.viewportWidth
        && reopenedDraft.context.viewportHeight == restored.cleaveContext.viewportHeight
        && reopenedDraft.context.sourceSurface.sector
            == restored.cleaveContext.sourceSurface.sector
        && reopenedDraft.context.sourceSurface.surface
            == restored.cleaveContext.sourceSurface.surface
        && reopenedDraft.context.targetSectors == restored.cleaveContext.targetSectors
        && reopenedDraft.context.targetSurfaces.size()
            == restored.cleaveContext.targetSurfaces.size();
    bool nativeTargetSurfacesMatch = nativeContextMatches;
    for (std::size_t index = 0;
         nativeTargetSurfacesMatch
             && index < reopenedDraft.context.targetSurfaces.size(); ++index)
    {
        nativeTargetSurfacesMatch =
            reopenedDraft.context.targetSurfaces[index].sector
                == restored.cleaveContext.targetSurfaces[index].sector
            && reopenedDraft.context.targetSurfaces[index].surface
                == restored.cleaveContext.targetSurfaces[index].surface;
    }
    const bool nativeOrderAndSelectionMatch =
        reopenedDraft.applyOrder == restored.applyOrder
        && reopenedDraft.selectedCleaveLines == restored.selectedCleaveLines
        && reopenedDraft.selectedCleaveLine == restored.selectedCleaveLine
        && reopenedDraft.chainOrigin.has_value()
            == restored.cleaveChainOrigin.has_value()
        && (!reopenedDraft.chainOrigin
            || (reopenedDraft.chainOrigin->lineIndex
                    == restored.cleaveChainOrigin->lineIndex
                && reopenedDraft.chainOrigin->endpoint
                    == restored.cleaveChainOrigin->endpoint));
    check(saved && !savedEdit.isEmpty() && reopenedView && nativeLinesMatch,
        "native Save/reopen retains every exact authored Cleave line and camera frame");
    check(saved && !savedEdit.isEmpty() && reopenedView && nativeTargetSurfacesMatch,
        "native Save/reopen retains the exact Cleave target and captured context");
    check(saved && !savedEdit.isEmpty() && reopenedView
            && nativeOrderAndSelectionMatch,
        "native Save/reopen retains exact Cleave execution order and physical selection");
    check(saved && !savedEdit.isEmpty() && reopenedView
            && reopenedState.cleaveHistory
            && sameCleaveHistory(*reopenedState.cleaveHistory,
                *restored.cleaveHistory),
        "native Save/reopen retains the complete local Cleave Undo/Redo timeline");
    check(saved && !savedEdit.isEmpty() && reopenedView
            && reopenedState.cleaveTransformMode
                == GlView::StagedCleaveTransformMode::Rotate
            && reopenedState.gridAxisXColor == axisX
            && reopenedState.gridAxisYColor == axisY
            && reopenedState.gridAxisZColor == axisZ,
        "native Save/reopen restores Cleave transform mode and all grid axes");

    MainWindow recovery{};
    prepareInteractiveCleaveWindow(recovery);
    recovery.setRecoveryDirectoryForTest(root / "recovery-checkpoint");
    recovery.beginRecoveryDocumentForTest(root / "pending-cut.sed", false);
    GlView* const recoveryView = recovery.glViewForTest();
    check(!recovery.hasUnsavedChangesForTest()
            && !recovery.hasRecoveryOfferEligibleChangesForTest(),
        "a clean Cleave workspace stays ineligible before an authored pointer gesture");
    if (recoveryView)
    {
        auto cameraShape = recoveryView->cleaveShapeOptions();
        cameraShape.cameraPlane = true;
        recoveryView->setCleaveShapeOptions(cameraShape);
        check(drawCleaveLineAtWorldPoints(
                *recoveryView,
                QVector3D(0.5f, -1.0f, 0.5f),
                QVector3D(0.5f, 2.0f, 0.5f))
                && recoveryView->stagedCleaveLineCount() == 1,
            "a real Sector Cleave pointer gesture authors the pending recovery line");
    }
    check(recovery.hasUnsavedChangesForTest()
            && recovery.hasRecoveryOfferEligibleChangesForTest(),
        "a settled pending Cleave line makes a clean Level eligible for unsaved recovery");
    recovery.triggerForcedRecoverySnapshotForTest();
    recovery.waitForRecoverySnapshotForTest();
    const std::filesystem::path liveRecoveryPath = recovery.currentRecoveryPathForTest();
    const auto liveRecovery = ui::recovery::readRecoveryRecord(liveRecoveryPath);
    check(liveRecovery && liveRecovery->metadata.requiresUserRecovery
            && cleaveMap(editorRoot(liveRecovery->editorStatePayload))
                .value(QStringLiteral("lines")).toArray().size() == 1
            && cleaveMap(editorRoot(liveRecovery->editorStatePayload))
                .value(QStringLiteral("shape")).toMap()
                .value(QStringLiteral("cameraPlane")).toBool(),
        "recovery checkpoint marks a pending Cleave draft as authored work and keeps its line");

    if (recoveryView)
    {
        auto cameraShape = recoveryView->cleaveShapeOptions();
        cameraShape.cameraPlane = false;
        recoveryView->setCleaveShapeOptions(cameraShape);
        check(drawCleaveLineAtWorldPoints(
                *recoveryView,
                QVector3D(0.65f, -1.0f, 0.5f),
                QVector3D(0.65f, 2.0f, 0.5f),
                false)
                && recoveryView->stagedCleaveLineCount() == 2,
            "a second real Cleave gesture creates the next recoverable workspace state");
    }
    const bool periodicAuthoredDirty = recovery.hasUnsavedChangesForTest();
    check(periodicAuthoredDirty,
        "the next settled Cleave gesture is authored work before periodic archive publication");
    // Request periodic rotation before dispatching a debounced EDIT capture,
    // so the preceding durable checkpoint is still the first authored line.
    recovery.triggerPeriodicRecoverySnapshotForTest();
    recovery.waitForRecoverySnapshotForTest();
    const auto archived = ui::recovery::scanAutosaveHistory(
        root / "recovery-checkpoint");
    check(archived.size() == 1,
        "periodic autosave archives the preceding Cleave-authored checkpoint");
    std::optional<ui::recovery::RecoveryRecord> archivedRecord{};
    QString archivedReadError{};
    std::size_t archivedCleaveLineCount = 0;
    if (archived.size() == 1)
    {
        archivedRecord = ui::recovery::readRecoveryRecord(
            archived.front().recoveryPath, &archivedReadError);
        if (archivedRecord)
        {
            archivedCleaveLineCount = static_cast<std::size_t>(cleaveMap(
                editorRoot(archivedRecord->editorStatePayload))
                    .value(QStringLiteral("lines")).toArray().size());
        }
    }
    const bool archivedCleaveWorkspaceExact = archivedRecord
        && archivedRecord->metadata.requiresUserRecovery
        && archivedCleaveLineCount == 1u
        && cleaveMap(editorRoot(archivedRecord->editorStatePayload))
            .value(QStringLiteral("shape")).toMap()
            .value(QStringLiteral("cameraPlane")).toBool();
    if (!archivedCleaveWorkspaceExact)
    {
        std::cerr << "DIAG autosave Cleave checkpoint: readError=\""
                  << archivedReadError.toStdString()
                  << "\" archiveCount=" << archived.size()
                  << " requiresUserRecovery="
                  << (archivedRecord
                      ? archivedRecord->metadata.requiresUserRecovery
                      : false)
                  << " archivedCleaveLines=" << archivedCleaveLineCount
                  << " currentLiveCleaveLines="
                  << (recoveryView ? recoveryView->stagedCleaveLineCount() : 0u)
                  << " periodicAuthoredDirty=" << periodicAuthoredDirty
                  << " currentPath=\""
                  << recovery.currentRecoveryPathForTest().string()
                  << "\" candidatePaths=[";
        for (std::size_t index = 0; index < archived.size(); ++index)
        {
            if (index != 0u)
            {
                std::cerr << ", ";
            }
            std::cerr << '"' << archived[index].recoveryPath.string() << '"';
        }
        std::cerr << "]\n";
    }
    check(archivedCleaveWorkspaceExact,
        "autosave history retains the exact preceding pending Cleave workspace and Camera-plane on");
    const auto latestRecord = ui::recovery::readRecoveryRecord(liveRecoveryPath);
    check(latestRecord
            && !cleaveMap(editorRoot(latestRecord->editorStatePayload))
                .value(QStringLiteral("shape")).toMap()
                .value(QStringLiteral("cameraPlane")).toBool(),
        "an authored Cleave autosave transition stores Camera-plane off in its latest active checkpoint");
    recovery.discardCurrentRecoveryForTest();
}

void runCleaveDormantWorkspaceTests(const std::filesystem::path& root)
{
    MainWindow window{};
    prepareCleaveWindow(window);
    GlView* const view = window.glViewForTest();
    QAction* const cleaveAction = window.findChild<QAction*>(
        QStringLiteral("viewport.sector.cleave"));
    if (!view || !cleaveAction)
    {
        check(false, "dormant Cleave fixture exposes its viewport and owning action");
        return;
    }
    view->addStagedCleaveLineForTest(
        QVector3D(-2.0f, 0.0f, 0.0f), QVector3D(2.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 1.0f, 0.0f));
    view->addStagedCleaveLineForTest(
        QVector3D(0.0f, -2.0f, 0.0f), QVector3D(0.0f, 2.0f, 0.0f),
        QVector3D(1.0f, 0.0f, 0.0f));
    view->addStagedCleaveLineForTest(
        QVector3D(-2.0f, -2.0f, 0.0f), QVector3D(2.0f, 2.0f, 0.0f),
        QVector3D(0.0f, 0.0f, 1.0f));
    const QByteArray payload = cleaveWorkspacePayload(
        window.captureRecoveryEditorStateForTest());
    check(!payload.isEmpty()
            && window.restoreRecoveryEditorStateForTest(payload),
        "dormant Cleave fixture restores a draft with order, origin and local Redo");
    if (payload.isEmpty())
    {
        return;
    }
    GlView::RecoveryState::CleaveShapeOptions shape{};
    shape.kind = ui::viewport::CleaveShapeKind::Arch;
    shape.width = 0.35;
    shape.height = 0.2;
    shape.dimensionsLinked = false;
    shape.segments = 23;
    shape.points = 11;
    shape.cameraPlane = true;
    view->setCleaveShapeOptions(shape);
    view->setCleaveMeasureUnit(GlView::CleaveMeasureUnit::JKU);
    view->setCleaveAngleStepDegrees(7.5);
    view->setStagedCleaveTransformMode(GlView::StagedCleaveTransformMode::Move);
    const GlView::RecoveryState expected = view->recoveryState();
    const auto exactDraftAndHistory = [&](const GlView::RecoveryState& state)
    {
        return sameDraftSnapshot(currentDraft(state), currentDraft(expected))
            && state.cleaveHistory && expected.cleaveHistory
            && sameCleaveHistory(*state.cleaveHistory, *expected.cleaveHistory);
    };
    const auto exactOptions = [&](const GlView::RecoveryState& state)
    {
        return state.cleaveChainEnabled == expected.cleaveChainEnabled
            && state.cleaveBounded == expected.cleaveBounded
            && state.cleaveMeasureUnit == expected.cleaveMeasureUnit
            && state.cleaveAngleStepDegrees == expected.cleaveAngleStepDegrees
            && state.cleaveTransformMode == expected.cleaveTransformMode
            && state.cleaveShapeOptions.kind == shape.kind
            && state.cleaveShapeOptions.width == shape.width
            && state.cleaveShapeOptions.height == shape.height
            && state.cleaveShapeOptions.dimensionsLinked == shape.dimensionsLinked
            && state.cleaveShapeOptions.segments == shape.segments
            && state.cleaveShapeOptions.points == shape.points
            && state.cleaveShapeOptions.cameraPlane == shape.cameraPlane;
    };

    cleaveAction->setChecked(false);
    check(exactDraftAndHistory(view->recoveryState())
            && exactOptions(view->recoveryState())
            && view->hasStagedCleaveDraft(),
        "unchecking Cleave retains the exact pending draft, history, selection, order and options");
    check(!view->stagedCleaveHistoryOwnsUndo()
            && !view->canUndoStagedCleaveHistory()
            && !view->canRedoStagedCleaveHistory(),
        "a dormant Cleave draft releases local Undo and Redo ownership");

    window.activateThingEditModeForTest();
    const editor::Selection dormantSelection = window.documentStateForTest().selection;
    check(exactDraftAndHistory(view->recoveryState())
            && exactOptions(view->recoveryState())
            && window.editModeForTest() == window.thingEditModeValueForTest(),
        "changing edit family retains the dormant Cleave workspace without restoring Sector mode");
    const QByteArray dormantPayload = window.captureRecoveryEditorStateForTest();
    check(window.validateRecoveryEditorStateForTest(dormantPayload)
            && cleaveMap(editorRoot(dormantPayload))
                .value(QStringLiteral("lines")).toArray().size() == 3
            && cleaveMap(editorRoot(dormantPayload))
                .value(QStringLiteral("history")).toMap()
                .value(QStringLiteral("snapshots")).toArray().size() == 3,
        "EDIT captures a dormant draft and both local history branches in another edit family");
    check(cleaveMap(editorRoot(dormantPayload)).value(QStringLiteral("shape")).toMap()
            .value(QStringLiteral("cameraPlane")).isBool()
            && cleaveMap(editorRoot(dormantPayload)).value(QStringLiteral("shape")).toMap()
                .value(QStringLiteral("cameraPlane")).toBool(),
        "EDIT stores the dormant Sector shape Camera-plane choice as a named Boolean");
    QCborMap malformedRoot = editorRoot(dormantPayload);
    QCborArray malformedViewport = viewportArray(malformedRoot);
    QCborMap malformedCleave = malformedViewport.at(38).toMap();
    QCborMap malformedShape = malformedCleave.value(QStringLiteral("shape")).toMap();
    malformedShape.insert(QStringLiteral("cameraPlane"), 1);
    malformedCleave.insert(QStringLiteral("shape"), malformedShape);
    malformedViewport[38] = malformedCleave;
    malformedRoot.insert(QStringLiteral("viewport"), malformedViewport);
    const QByteArray malformedCameraPlane = editorPayloadWithRoot(std::move(malformedRoot));
    check(!window.validateRecoveryEditorStateForTest(malformedCameraPlane)
            && !window.restoreRecoveryEditorStateForTest(malformedCameraPlane)
            && window.captureRecoveryEditorStateForTest() == dormantPayload,
        "a non-Boolean Camera-plane option rejects the entire EDIT workspace before mutation");
    QCborMap legacyRoot = editorRoot(dormantPayload);
    QCborArray legacyViewport = viewportArray(legacyRoot);
    QCborMap legacyCleave = legacyViewport.at(38).toMap();
    QCborMap legacyShape = legacyCleave.value(QStringLiteral("shape")).toMap();
    legacyShape.remove(QStringLiteral("cameraPlane"));
    legacyCleave.insert(QStringLiteral("shape"), legacyShape);
    legacyViewport[38] = legacyCleave;
    legacyRoot.insert(QStringLiteral("viewport"), legacyViewport);
    MainWindow legacyRestored{};
    legacyRestored.setAttribute(Qt::WA_DontShowOnScreen);
    legacyRestored.setDocumentStateForTest(makeCleaveDocument());
    check(legacyRestored.restoreRecoveryEditorStateForTest(
            editorPayloadWithRoot(std::move(legacyRoot)))
            && legacyRestored.glViewForTest()
            && !legacyRestored.glViewForTest()->cleaveShapeOptions().cameraPlane,
        "legacy EDIT without Camera-plane restores its default off independently of live options");
    view->setLevel(&window.documentStateForTest().level, GlView::SetLevelMode::keepView);
    check(exactDraftAndHistory(view->recoveryState()),
        "republishing the current Level preserves a dormant draft's complete local timeline");
    view->notifyLevelGeometryChanged();
    check(exactDraftAndHistory(view->recoveryState()),
        "a notified Level update retains world-authored dormant cuts whose target references remain valid");

    const std::filesystem::path nativePath = root / "dormant-cleave.sed";
    QAction* const saveAs = window.findChild<QAction*>(QStringLiteral("file.save_as"));
    check(saveAs && saveAsToPath(saveAs, nativePath) == 0
            && std::filesystem::is_regular_file(nativePath),
        "native Save writes a dormant Cleave workspace");
    MainWindow reopened{};
    reopened.setAttribute(Qt::WA_DontShowOnScreen);
    reopened.openLevelFromFilenameForTest(QString::fromStdWString(nativePath.wstring()));
    GlView* const reopenedView = reopened.glViewForTest();
    check(reopenedView && exactDraftAndHistory(reopenedView->recoveryState())
            && exactOptions(reopenedView->recoveryState())
            && !reopenedView->sectorCleaveToolEnabled()
            && !reopenedView->surfaceCleaveToolEnabled()
            && !reopenedView->stagedCleaveHistoryOwnsUndo()
            && reopened.editModeForTest() == reopened.thingEditModeValueForTest()
            && sameSelection(reopened.documentStateForTest().selection, dormantSelection),
        "native Open restores the dormant draft and options while preserving its current edit mode and Level selection");

    const std::filesystem::path recoveryDirectory = root / "dormant-cleave-recovery";
    window.setRecoveryDirectoryForTest(recoveryDirectory);
    window.beginRecoveryDocumentForTest(nativePath, false);
    window.triggerForcedRecoverySnapshotForTest();
    window.waitForRecoverySnapshotForTest();
    const std::filesystem::path checkpoint = window.currentRecoveryPathForTest();
    MainWindow recovered{};
    recovered.setAttribute(Qt::WA_DontShowOnScreen);
    recovered.setRecoveryDirectoryForTest(root / "dormant-cleave-restored");
    check(recovered.applyRecoveryFileForTest(checkpoint)
            && recovered.glViewForTest()
            && exactDraftAndHistory(recovered.glViewForTest()->recoveryState())
            && exactOptions(recovered.glViewForTest()->recoveryState())
            && !recovered.glViewForTest()->stagedCleaveHistoryOwnsUndo()
            && recovered.editModeForTest() == recovered.thingEditModeValueForTest(),
        "autosave/recovery restores dormant cuts and local history without activating their tool");
    auto cameraOff = view->cleaveShapeOptions();
    cameraOff.cameraPlane = false;
    view->setCleaveShapeOptions(cameraOff);
    window.triggerPeriodicRecoverySnapshotForTest();
    window.waitForRecoverySnapshotForTest();
    const auto archives = ui::recovery::scanAutosaveHistory(recoveryDirectory);
    const auto currentCheckpoint = ui::recovery::readRecoveryRecord(checkpoint);
    check(archives.empty() && currentCheckpoint
            && !cleaveMap(editorRoot(currentCheckpoint->editorStatePayload))
                .value(QStringLiteral("shape")).toMap()
                .value(QStringLiteral("cameraPlane")).toBool(),
        "an option-only Camera-plane edit refreshes the latest dormant checkpoint without rotating autosave history");
    view->setCleaveShapeOptions(shape);

    window.activateSectorEditModeForTest();
    view->setSelectionState(0, -1, -1, -1, makeCleaveDocument().selection);
    const editor::Selection beforeResume = window.documentStateForTest().selection;
    view->setSectorCleaveToolEnabled(true);
    check(exactDraftAndHistory(view->recoveryState())
            && exactOptions(view->recoveryState())
            && sameSelection(window.documentStateForTest().selection, beforeResume)
            && view->stagedCleaveHistoryOwnsUndo(),
        "returning to Cleave resumes its exact local timeline without changing Level selection");
    view->setCleaveStagedMode(false);
    check(exactDraftAndHistory(view->recoveryState())
            && !view->stagedCleaveHistoryOwnsUndo(),
        "leaving Multi-cut retains its pending arrangement and suspends local history routing");
    view->setCleaveStagedMode(true);
    view->setRenderPipeline(GlView::RenderPipeline::GamePreview);
    check(exactDraftAndHistory(view->recoveryState())
            && !view->stagedCleaveHistoryOwnsUndo(),
        "Game Preview keeps dormant draft data while releasing Cleave command ownership");
    view->setRenderPipeline(GlView::RenderPipeline::Editor);
    check(exactDraftAndHistory(view->recoveryState())
            && view->stagedCleaveHistoryOwnsUndo(),
        "returning from Game Preview resumes retained Cleave draft history");
    window.discardCurrentRecoveryForTest();
    recovered.discardCurrentRecoveryForTest();

    view->setSectorCleaveToolEnabled(false);
    const Level* const reusedDocumentAddress = &window.documentStateForTest().level;
    check(exactDraftAndHistory(view->recoveryState()),
        "the same-address lifetime reset starts with the exact dormant draft and local history");
    window.resetSelectionContextStateForTest();
    view->setLevel(&window.documentStateForTest().level, GlView::SetLevelMode::keepView);
    check(&window.documentStateForTest().level == reusedDocumentAddress
            && !view->hasStagedCleaveDraft() && !view->captureStagedCleaveHistory()
            && !view->sectorCleaveToolEnabled() && !view->surfaceCleaveToolEnabled(),
        "New/Open ownership teardown clears a dormant draft and history even when Level storage is reused");

    view->setLevel(nullptr, GlView::SetLevelMode::keepView);
    check(!view->hasStagedCleaveDraft() && !view->captureStagedCleaveHistory(),
        "detaching the document ends dormant draft ownership before a new document lifetime");
}

void runCleaveTypedNormalizationTests()
{
    GlView view{};
    Level classic{};
    classic.kind = ProjectType::JKDF2;
    level::levelCreateCube(classic, 0.0, 0.0, 0.0, TVector::up, TVector::right);
    view.setLevel(&classic);
    check(view.cleaveShapeOptions().kind == ui::viewport::CleaveShapeKind::Rectangle
            && view.cleaveShapeOptions().width == 0.1
            && view.cleaveShapeOptions().height == 0.1,
        "typed Cleave shape defaults are a Rectangle measuring one displayed game unit");
    view.setCleaveMeasureUnit(GlView::CleaveMeasureUnit::IJU);
    check(view.cleaveMeasureUnit() == GlView::CleaveMeasureUnit::Meters,
        "the typed Cleave unit setter rejects IJU in a classic project");
    view.setCleaveMeasureUnit(GlView::CleaveMeasureUnit::JKU);
    Level ijim{};
    ijim.kind = ProjectType::IJIM;
    view.setLevel(&ijim);
    check(view.cleaveMeasureUnit() == GlView::CleaveMeasureUnit::Meters,
        "switching projects normalizes an unavailable Cleave unit");
    GlView::RecoveryState state = view.recoveryState();
    state.cleaveMeasureUnit = GlView::CleaveMeasureUnit::JKU;
    check(view.restoreRecoveryState(state)
            && view.cleaveMeasureUnit() == GlView::CleaveMeasureUnit::Meters,
        "viewport recovery normalizes unsupported project measurement units");
    GlView::RecoveryState::CleaveShapeOptions shape{};
    shape.kind = ui::viewport::CleaveShapeKind::Square;
    shape.width = 0.3;
    shape.height = 0.4;
    shape.dimensionsLinked = false;
    view.setCleaveShapeOptions(shape);
    check(view.cleaveShapeOptions().kind == ui::viewport::CleaveShapeKind::Rectangle
            && view.cleaveShapeOptions().width == 0.3
            && view.cleaveShapeOptions().height == 0.3
            && view.cleaveShapeOptions().dimensionsLinked,
        "legacy Square options migrate to an equal-sided linked Rectangle");
    shape.kind = ui::viewport::CleaveShapeKind::Rectangle;
    shape.width = std::numeric_limits<double>::quiet_NaN();
    shape.height = std::numeric_limits<double>::infinity();
    view.setCleaveShapeOptions(shape);
    check(view.cleaveShapeOptions().width == 0.1
            && view.cleaveShapeOptions().height == 0.1,
        "invalid typed shape dimensions fall back to one displayed unit");
    shape = view.cleaveShapeOptions();
    shape.cameraPlane = true;
    view.setCleaveShapeOptions(shape);
    check(view.cleaveShapeOptions().cameraPlane,
        "changing only Camera-plane updates the typed shape options");
    shape.cameraPlane = false;
    view.setCleaveShapeOptions(shape);
    check(!view.cleaveShapeOptions().cameraPlane,
        "Camera-plane can be turned off independently of shape dimensions");

    view.setLevel(&classic);
    view.setSectorCleaveToolEnabled(true);
    view.setCleaveStagedMode(true);
    view.setSelectionState(0, -1, -1, -1, makeCleaveDocument().selection);
    view.captureStagedCleaveContextForTest(false);
    view.addStagedCleaveLineForTest(
        QVector3D(-1.0f, 0.0f, 0.0f), QVector3D(1.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 1.0f, 0.0f));
    view.addStagedCleaveLineForTest(
        QVector3D(0.0f, -1.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f),
        QVector3D(0.0f, 0.0f, 1.0f));
    auto draft = view.captureStagedCleaveDraftState();
    draft.selectedCleaveLines = { 0, 1 };
    draft.selectedCleaveLine = 1;
    view.restoreStagedCleaveDraftState(draft);
    view.setStagedCleaveTransformMode(GlView::StagedCleaveTransformMode::Rotate);
    check(!view.canTransformSelectedStagedCleave(GlView::StagedCleaveTransformMode::Rotate)
            && view.stagedCleaveTransformMode() == GlView::StagedCleaveTransformMode::Move,
        "a disabled Cleave Rotate mode falls back to Move in the typed API");
    view.setStagedCleaveTransformMode(GlView::StagedCleaveTransformMode::Scale);
    check(view.stagedCleaveTransformMode() == GlView::StagedCleaveTransformMode::Move,
        "a disabled Cleave Scale mode falls back to Move in the typed API");
}

void runCleaveTypedCheckpointSignalTests()
{
    GlView view{};
    int signals = 0;
    QObject::connect(&view, &GlView::viewportStateChanged,
        [&signals]() { ++signals; });
    const auto emitsOneSettledSignal = [&](auto change, const char* description)
    {
        const int before = signals;
        change();
        check(signals == before + 1, description);
        change();
        check(signals == before + 1,
            "reapplying the same Cleave option does not emit an extra checkpoint signal");
    };
    const bool chain = !view.cleaveChainEnabled();
    emitsOneSettledSignal([&]() { view.setCleaveChainEnabled(chain); },
        "typed Chain changes invalidate the editor checkpoint exactly once");
    emitsOneSettledSignal([&]() { view.setCleaveMeasureUnit(GlView::CleaveMeasureUnit::Bare); },
        "typed Units changes invalidate the editor checkpoint exactly once");
    emitsOneSettledSignal([&]() { view.setCleaveAngleStepDegrees(7.5); },
        "typed Angle step changes invalidate the editor checkpoint exactly once");
    const bool angles = !view.cleaveAnglesVisible();
    emitsOneSettledSignal([&]() { view.setCleaveAnglesVisible(angles); },
        "typed angle-guide visibility changes invalidate the editor checkpoint exactly once");
    const bool lengths = !view.cleaveLengthsVisible();
    emitsOneSettledSignal([&]() { view.setCleaveLengthsVisible(lengths); },
        "typed length-label visibility changes invalidate the editor checkpoint exactly once");
    const bool numbers = !view.cleaveLineNumbersVisible();
    emitsOneSettledSignal([&]() { view.setCleaveLineNumbersVisible(numbers); },
        "typed execution-rank visibility changes invalidate the editor checkpoint exactly once");
}

void runCleaveMainWindowRestoreBudgetAtomicityTest()
{
    MainWindow source{};
    prepareCleaveWindow(source);
    GlView* const sourceView = source.glViewForTest();
    if (!sourceView)
    {
        check(false, "whole-window restore refusal fixture exposes a viewport");
        return;
    }
    sourceView->addStagedCleaveLineForTest(
        QVector3D(0.25f, 0.25f, 0.5f), QVector3D(0.75f, 0.25f, 0.5f),
        QVector3D(0.0f, 1.0f, 0.0f));
    QCborMap root = editorRoot(source.captureRecoveryEditorStateForTest());
    QCborArray viewport = viewportArray(root);
    QCborMap cleave = viewport.at(38).toMap();
    const QCborValue line = cleave.value(QStringLiteral("lines")).toArray().at(0);
    const QCborArray context = cleave.value(QStringLiteral("context")).toArray();
    constexpr qsizetype lineCount = 18'000;
    constexpr std::size_t existingLiveLineCount = 8'000u;
    constexpr std::size_t budget = 16u * 1024u * 1024u;
    check((static_cast<std::size_t>(lineCount) * 4u
            + existingLiveLineCount * 2u)
            * (sizeof(GlView::RecoveryState::CleaveLine) + sizeof(std::int32_t))
            > budget,
        "whole-window refusal fixture exceeds the restore workset while fitting decoded EDIT");
    QCborArray lines{};
    QCborArray order{};
    for (qsizetype index = 0; index < lineCount; ++index)
    {
        lines.append(line);
        order.append(static_cast<qint64>(index));
    }
    cleave.insert(QStringLiteral("lines"), lines);
    cleave.insert(QStringLiteral("applyOrder"), order);
    cleave.remove(QStringLiteral("chainOrigin"));
    cleave.insert(QStringLiteral("selection"), QCborMap{
        { QStringLiteral("lines"), QCborArray{} },
        { QStringLiteral("active"), -1 },
    });
    QCborArray options = cleave.value(QStringLiteral("options")).toArray();
    options[5] = -1;
    cleave.insert(QStringLiteral("options"), options);
    QCborMap cursorSnapshot = draftSnapshot(
        lines, context, QCborArray{}, order, QCborArray{}, -1);
    cursorSnapshot.remove(QStringLiteral("chainOrigin"));
    cleave.insert(QStringLiteral("history"), QCborMap{
        { QStringLiteral("version"), 1 },
        { QStringLiteral("cursor"), 0 },
        { QStringLiteral("snapshots"), QCborArray{ cursorSnapshot } },
    });
    viewport[38] = cleave;
    root.insert(QStringLiteral("viewport"), viewport);
    const QByteArray oversizedRestore = editorPayloadWithRoot(std::move(root));

    MainWindow destination{};
    prepareCleaveWindow(destination);
    GlView* const destinationView = destination.glViewForTest();
    if (!destinationView)
    {
        return;
    }
    destinationView->addStagedCleaveLineForTest(
        QVector3D(0.2f, 0.2f, 0.5f), QVector3D(0.8f, 0.2f, 0.5f),
        QVector3D(0.0f, 1.0f, 0.0f));
    auto existingDraft = destinationView->captureStagedCleaveDraftState();
    existingDraft.lines.assign(existingLiveLineCount, existingDraft.lines.front());
    existingDraft.applyOrder.resize(existingLiveLineCount);
    for (std::size_t index = 0; index < existingLiveLineCount; ++index)
    {
        existingDraft.applyOrder[index] = static_cast<std::int32_t>(index);
    }
    existingDraft.selectedCleaveLines.clear();
    existingDraft.selectedCleaveLine = -1;
    GlView::RecoveryState::CleaveHistory existingHistory{};
    existingHistory.snapshots.push_back(existingDraft);
    destinationView->restoreStagedCleaveDraftState(
        std::move(existingDraft), std::move(existingHistory));
    destination.activateThingEditModeForTest();
    destinationView->setCameraOrthoScale(23.0f);
    destination.setLayerVisualStateForTest({ false }, { true });
    if (auto* repeat = destination.findChild<QToolButton*>(
            QStringLiteral("uvEditorRepeatButton")))
    {
        repeat->setChecked(!repeat->isChecked());
    }
    const QByteArray before = destination.captureRecoveryEditorStateForTest();
    const editor::Selection beforeSelection = destination.documentStateForTest().selection;
    const int beforeMode = destination.editModeForTest();
    const std::size_t retainedBytes = destinationView->cleaveRecoveryLiveOwnedArrayBytesForTest();
    const std::size_t minimumSourceBytes = static_cast<std::size_t>(lineCount) * 2u
        * (sizeof(GlView::RecoveryState::CleaveLine) + sizeof(std::int32_t));
    check(2u * minimumSourceBytes + retainedBytes > budget,
        "the admitted source plus actual retained live arrays and candidate copy exceed the aggregate budget");
    QString warning{};
    const bool admitted = destination.validateRecoveryEditorStateForTest(
        oversizedRestore, &warning);
    if (!admitted)
    {
        std::cerr << "DIAG whole-window restore admission: payloadBytes="
                  << oversizedRestore.size() << " warning=\""
                  << warning.toStdString() << "\"\n";
    }
    check(admitted,
        "whole-window refusal input passes the real bounded canonical EDIT decoder");
    if (!admitted)
    {
        return;
    }
    const bool restored = destination.restoreRecoveryEditorStateForTest(oversizedRestore, &warning);
    if (restored)
    {
        std::cerr << "DIAG whole-window restore unexpectedly admitted aggregate workset: warning=\""
                  << warning.toStdString() << "\"\n";
    }
    check(!restored && !warning.isEmpty()
            && destination.captureRecoveryEditorStateForTest() == before
            && destination.editModeForTest() == beforeMode
            && sameSelection(destination.documentStateForTest().selection, beforeSelection),
        "a viewport workset budget refusal preserves the entire live MainWindow workspace before teardown");
}

void runIndependentCleaveNativeRecoveryTests(const std::filesystem::path& root)
{
    for ( const bool surfaceCurrent : { false, true } )
    {
        for ( const bool dormant : { false, true } )
        {
            const std::string name = std::string(surfaceCurrent ? "surface" : "sector")
                + (dormant ? "-dormant" : "-active");
            MainWindow window{};
            prepareCleaveWindow(window);
            GlView* const view = window.glViewForTest();
            if ( !view ) return;
            const auto activate = [&](const bool surface)
            {
                if ( surface ) window.activateSurfaceEditModeForTest();
                else window.activateSectorEditModeForTest();
                editor::Selection targets{};
                if ( surface ) targets.surfaces.insert({ 0, 0 });
                else targets.sectors.insert(0);
                window.mutableDocumentStateForTest().selection = targets;
                view->setSelectionState(0, surface ? 0 : -1, -1, -1, targets);
                window.updateEditActionsForTest();
                QAction* const action = window.findChild<QAction*>(surface
                    ? QStringLiteral("viewport.surface.cleave") : QStringLiteral("viewport.sector.cleave"));
                check(action && action->isEnabled(),
                    "independent persistence fixture has a real authoritative selected Cleave owner");
                if ( action ) action->setChecked(true);
                return action;
            };
            activate(!surfaceCurrent);
            seedIndependentCleaveWorkspace(*view, !surfaceCurrent);
            auto shape = view->cleaveShapeOptions();
            shape.rotationDegrees = surfaceCurrent ? -17.5 : 22.5;
            view->setCleaveShapeOptions(shape);
            QAction* const currentAction = activate(surfaceCurrent);
            seedIndependentCleaveWorkspace(*view, surfaceCurrent);
            shape = view->cleaveShapeOptions();
            shape.rotationDegrees = surfaceCurrent ? 31.25 : -42.5;
            view->setCleaveShapeOptions(shape);
            if ( dormant )
            {
                if ( currentAction ) currentAction->setChecked(false);
                window.activateThingEditModeForTest();
            }
            const QCborMap expectedNative = cleaveMap(editorRoot(window.captureRecoveryEditorStateForTest()));
            const std::filesystem::path nativePath = root / ("dual-" + name + ".sed");
            QAction* const saveAs = window.findChild<QAction*>(QStringLiteral("file.save_as"));
            check(saveAs && saveAsToPath(saveAs, nativePath) == 0,
                "native Save accepts both independent Cleave workspaces with shape rotation");
            const auto saved = loadNativeFile(nativePath);
            MainWindow reopened{};
            reopened.setAttribute(Qt::WA_DontShowOnScreen);
            reopened.openLevelFromFilenameForTest(QString::fromStdWString(nativePath.wstring()));
            check(saved && cleaveMap(editorRoot(nativeEditorPayload(*saved))) == expectedNative
                    && reopened.glViewForTest()
                    && cleaveMap(editorRoot(reopened.captureRecoveryEditorStateForTest())) == expectedNative
                    && reopened.glViewForTest()->stagedCleaveHistoryOwnsUndo() == !dormant,
                "native Save/Open restores both exact timelines, targets, line order/selection and options for either active or dormant current family");
            const std::filesystem::path recoveryDirectory = root / ("dual-recovery-" + name);
            window.setRecoveryDirectoryForTest(recoveryDirectory);
            window.beginRecoveryDocumentForTest(nativePath, false);
            const auto peerFamily = surfaceCurrent ? GlView::CleaveWorkspaceFamily::Sector
                : GlView::CleaveWorkspaceFamily::Surface;
            auto peer = view->captureStagedCleaveWorkspace(peerFamily);
            peer.draft.lines.front().first.y += 0.03125;
            peer.draft.lines.front().second.y += 0.03125;
            peer.history->snapshots[peer.history->cursor] = peer.draft;
            check(view->restoreStagedCleaveWorkspace(peerFamily, std::move(peer))
                    && window.hasRecoveryOfferEligibleChangesForTest(),
                "a parked-only authored edit makes the saved double workspace recoverable");
            const QCborMap expectedForced = cleaveMap(editorRoot(window.captureRecoveryEditorStateForTest()));
            window.triggerForcedRecoverySnapshotForTest();
            window.waitForRecoverySnapshotForTest();
            const auto checkpoint = window.currentRecoveryPathForTest();
            const auto forced = ui::recovery::readRecoveryRecord(checkpoint);
            check(forced && forced->metadata.requiresUserRecovery
                    && cleaveMap(editorRoot(forced->editorStatePayload)) == expectedForced,
                "forced autosave retains exact active/current and parked workspaces including both shape rotations");
            const auto family = view->cleaveWorkspaceFamily();
            auto current = view->captureStagedCleaveWorkspace(family);
            current.draft.lines.front().first.x += 0.0625;
            current.draft.lines.front().second.x += 0.0625;
            current.history->snapshots[current.history->cursor] = current.draft;
            check(view->restoreStagedCleaveWorkspace(family, std::move(current)),
                "the next settled current-family draft edit creates a real authored autosave transition");
            const QCborMap expectedLatest = cleaveMap(editorRoot(window.captureRecoveryEditorStateForTest()));
            window.triggerPeriodicRecoverySnapshotForTest();
            window.waitForRecoverySnapshotForTest();
            const auto archives = ui::recovery::scanAutosaveHistory(recoveryDirectory);
            const auto archived = archives.size() == 1u
                ? ui::recovery::readRecoveryRecord(archives.front().recoveryPath) : std::nullopt;
            const auto latest = ui::recovery::readRecoveryRecord(checkpoint);
            check(archived && cleaveMap(editorRoot(archived->editorStatePayload)) == expectedForced
                    && latest && cleaveMap(editorRoot(latest->editorStatePayload)) == expectedLatest,
                "periodic autosave archives the exact preceding double workspace and publishes both updated local timelines");
            MainWindow recovered{};
            recovered.setAttribute(Qt::WA_DontShowOnScreen);
            recovered.setRecoveryDirectoryForTest(root / ("dual-recovered-" + name));
            check(recovered.applyRecoveryFileForTest(checkpoint)
                    && recovered.glViewForTest()
                    && cleaveMap(editorRoot(recovered.captureRecoveryEditorStateForTest())) == expectedLatest
                    && recovered.glViewForTest()->stagedCleaveHistoryOwnsUndo() == !dormant,
                "recovery applies both families exactly while only the matching active owner captures local Undo");
            window.discardCurrentRecoveryForTest();
        }
    }
}

void runIndependentCleaveDocumentResetAndDefaultsTests()
{
    QSettings preferences = settingsStore();
    const std::array<QString, 10> keys{
        QStringLiteral("cleave/multi_cut_mode"), QStringLiteral("cleave/staged_mode"),
        QStringLiteral("cleave/chain"), QStringLiteral("cleave/bounded"),
        QStringLiteral("cleave/show_angles"), QStringLiteral("cleave/show_lengths"),
        QStringLiteral("cleave/show_line_numbers"), QStringLiteral("cleave/measure_unit"),
        QStringLiteral("cleave/angle_step_degrees"), QStringLiteral("cleave/show_measurements") };
    std::array<std::optional<QVariant>, 10> previous{};
    for ( std::size_t index = 0; index < keys.size(); ++index )
    {
        if ( preferences.contains(keys[index]) ) previous[index] = preferences.value(keys[index]);
    }
    const auto restorePreferences = qScopeGuard([&]()
    {
        for ( std::size_t index = 0; index < keys.size(); ++index )
        {
            if ( previous[index] ) preferences.setValue(keys[index], *previous[index]);
            else preferences.remove(keys[index]);
        }
        preferences.sync();
    });
    preferences.setValue(keys[0], true);
    preferences.setValue(keys[2], true);
    preferences.setValue(keys[3], false);
    preferences.setValue(keys[4], false);
    preferences.setValue(keys[5], true);
    preferences.setValue(keys[6], true);
    preferences.setValue(keys[7], static_cast<int>(GlView::CleaveMeasureUnit::Bare));
    preferences.setValue(keys[8], 12.5);
    preferences.remove(keys[9]);
    preferences.sync();
    const auto emptyPreferenceWorkspace = [](const GlView::RecoveryState::CleaveWorkspace& workspace)
    {
        return workspace.draft.lines.empty() && !workspace.draft.context.valid
            && !workspace.draft.chainOrigin && workspace.draft.applyOrder.empty()
            && workspace.draft.selectedCleaveLines.empty() && workspace.draft.selectedCleaveLine == -1
            && !workspace.history && workspace.stagedMode && workspace.chainEnabled
            && workspace.bounded == false && !workspace.anglesVisible && workspace.lengthsVisible
            && workspace.lineNumbersVisible && workspace.measureUnit == GlView::CleaveMeasureUnit::Bare
            && workspace.angleStepDegrees == 12.5
            && workspace.shapeOptions.kind == ui::viewport::CleaveShapeKind::Rectangle
            && workspace.shapeOptions.width == 0.1 && workspace.shapeOptions.height == 0.1
            && workspace.shapeOptions.rotationDegrees == 0.0
            && workspace.transformMode == GlView::StagedCleaveTransformMode::Move;
    };
    MainWindow window{};
    prepareCleaveWindow(window);
    GlView* const view = window.glViewForTest();
    if ( !view ) return;
    view->setCleaveBounded(true);
    view->setCleaveChainEnabled(false);
    view->setCleaveAnglesVisible(true);
    view->setCleaveLengthsVisible(false);
    view->setCleaveLineNumbersVisible(false);
    view->setCleaveMeasureUnit(GlView::CleaveMeasureUnit::Meters);
    view->setCleaveAngleStepDegrees(45.0);
    activateCleaveFamily(*view, true);
    check(emptyPreferenceWorkspace(view->captureStagedCleaveWorkspace(GlView::CleaveWorkspaceFamily::Surface)),
        "first activating an untouched Cleave family uses application preferences independently of customized peer options");
    seedIndependentCleaveWorkspace(*view, true);
    activateCleaveFamily(*view, false);
    seedIndependentCleaveWorkspace(*view, false);
    QCborMap legacyRoot = editorRoot(window.captureRecoveryEditorStateForTest());
    QCborMap legacyWorkspace = cleaveMap(legacyRoot);
    legacyWorkspace.remove(QStringLiteral("currentFamily"));
    legacyWorkspace.remove(QStringLiteral("peerWorkspace"));
    const QByteArray legacyPayload = editorPayloadWithCleave(
        editorPayloadWithRoot(std::move(legacyRoot)), legacyWorkspace);
    MainWindow legacy{};
    prepareCleaveWindow(legacy);
    check(legacy.validateRecoveryEditorStateForTest(legacyPayload)
            && legacy.restoreRecoveryEditorStateForTest(legacyPayload)
            && legacy.glViewForTest()
            && legacy.glViewForTest()->cleaveWorkspaceFamily() == GlView::CleaveWorkspaceFamily::Sector
            && emptyPreferenceWorkspace(legacy.glViewForTest()->captureStagedCleaveWorkspace(
                GlView::CleaveWorkspaceFamily::Surface))
            && sameDraftSnapshot(legacy.glViewForTest()->captureStagedCleaveDraftState(),
                view->captureStagedCleaveDraftState()),
        "legacy EDIT without family/peer fields restores its current draft and initializes an independent empty peer from application preferences");
    const Level* const documentAddress = &window.documentStateForTest().level;
    view->setSectorCleaveToolEnabled(false);
    window.resetSelectionContextStateForTest();
    view->setLevel(documentAddress, GlView::SetLevelMode::keepView);
    check(&window.documentStateForTest().level == documentAddress
            && emptyPreferenceWorkspace(view->captureStagedCleaveWorkspace(GlView::CleaveWorkspaceFamily::Sector))
            && emptyPreferenceWorkspace(view->captureStagedCleaveWorkspace(GlView::CleaveWorkspaceFamily::Surface))
            && !view->sectorCleaveToolEnabled() && !view->surfaceCleaveToolEnabled(),
        "same-address New/Open clears both dormant drafts and timelines and independently resets both options to application preferences");
    for ( const bool dormant : { false, true } )
    {
        GlView detached{};
        Level first = makeCleaveDocument().level;
        Level replacement = makeCleaveDocument().level;
        detached.setLevel(&first);
        activateCleaveFamily(detached, false);
        seedIndependentCleaveWorkspace(detached, false);
        activateCleaveFamily(detached, true);
        seedIndependentCleaveWorkspace(detached, true);
        if ( dormant ) detached.setSurfaceCleaveToolEnabled(false);
        detached.setLevel(&replacement);
        check(emptyPreferenceWorkspace(detached.captureStagedCleaveWorkspace(GlView::CleaveWorkspaceFamily::Sector))
                && emptyPreferenceWorkspace(detached.captureStagedCleaveWorkspace(GlView::CleaveWorkspaceFamily::Surface)),
            "replacing the Level pointer clears active and dormant drafts in both families without inheriting previous-document options");
        activateCleaveFamily(detached, true);
        seedIndependentCleaveWorkspace(detached, true);
        activateCleaveFamily(detached, false);
        seedIndependentCleaveWorkspace(detached, false);
        detached.setLevel(nullptr, GlView::SetLevelMode::keepView);
        check(emptyPreferenceWorkspace(detached.captureStagedCleaveWorkspace(GlView::CleaveWorkspaceFamily::Sector))
                && emptyPreferenceWorkspace(detached.captureStagedCleaveWorkspace(GlView::CleaveWorkspaceFamily::Surface)),
            "detaching a document ends both independent draft lifetimes including parked history");
    }
}

void runIndependentCleaveRestoreBudgetAtomicityTest()
{
    MainWindow window{};
    prepareCleaveWindow(window);
    GlView* const view = window.glViewForTest();
    if ( !view ) return;
    seedIndependentCleaveWorkspace(*view, false);
    activateCleaveFamily(*view, true);
    seedIndependentCleaveWorkspace(*view, true);
    activateCleaveFamily(*view, false);
    const QByteArray before = window.captureRecoveryEditorStateForTest();
    GlView::RecoveryState incoming = view->recoveryState();
    constexpr std::size_t budget = 16u * 1024u * 1024u;
    constexpr std::size_t reservedLines = budget
        / (2u * (sizeof(GlView::RecoveryState::CleaveLine) + sizeof(std::int32_t))) + 1u;
    auto& peer = *incoming.cleavePeerWorkspace;
    const auto repeatedLine = peer.draft.lines.front();
    peer.draft.lines.assign(reservedLines, repeatedLine);
    peer.draft.applyOrder.resize(reservedLines);
    for ( std::size_t index = 0; index < reservedLines; ++index )
    {
        peer.draft.applyOrder[index] = static_cast<std::int32_t>(index);
    }
    peer.draft.selectedCleaveLines.clear();
    peer.draft.selectedCleaveLine = -1;
    peer.draft.chainOrigin.reset();
    peer.history.reset();
    const std::size_t peerBytes = GlView::stagedCleaveWorkspaceOwnedBytesEstimate(
        *incoming.cleavePeerWorkspace);
    check(peerBytes < budget && peerBytes > budget / 2u
            && incoming.cleavePeerWorkspace->draft.lines.size() == reservedLines
            && !incoming.cleavePeerWorkspace->history,
        "dual restore budget fixture has a valid dormant workspace with actual authored arrays and owned capacity below its single-family limit");
    QString error{};
    check(!view->canRestoreRecoveryState(incoming, &error) && !error.isEmpty()
            && !view->restoreRecoveryState(incoming, false, &error)
            && window.captureRecoveryEditorStateForTest() == before,
        "aggregate current plus dormant source and prepared storage is refused before publishing either family or MainWindow state");
}

void runCleaveTopologyInvalidationTest()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    editor::DocumentState state = makeCleaveDocument();
    level::levelCreateCube(state.level, 2.0, 0.0, 0.0, TVector::up, TVector::right);
    level::levelCreateCube(state.level, 4.0, 0.0, 0.0, TVector::up, TVector::right);
    state.selection.sectors = { 1 };
    window.setDocumentStateForTest(std::move(state));
    window.activateSectorEditModeForTest();
    GlView* const view = window.glViewForTest();
    QAction* const remove = window.findChild<QAction*>(QStringLiteral("edit.delete"));
    QAction* const undo = window.findChild<QAction*>(QStringLiteral("edit.undo"));
    QAction* const redo = window.findChild<QAction*>(QStringLiteral("edit.redo"));
    if ( !view || !remove || !undo || !redo )
    {
        check(false, "topology invalidation fixture exposes viewport and real Delete/Undo/Redo actions");
        return;
    }
    const auto seedTargetFamilies = [&]()
    {
        for ( const bool surface : { false, true } )
        {
            activateCleaveFamily(*view, surface);
            seedIndependentCleaveWorkspace(*view, surface);
            const auto family = surface ? GlView::CleaveWorkspaceFamily::Surface
                : GlView::CleaveWorkspaceFamily::Sector;
            auto workspace = view->captureStagedCleaveWorkspace(family);
            const auto addressTarget = [surface](auto& draft)
            {
                draft.context.targetSectors = { 1 };
                draft.context.targetSurfaces = surface ? std::vector<SurfaceRef>{ { 1, 0 } }
                    : std::vector<SurfaceRef>{};
                draft.context.sourceSurface = surface ? SurfaceRef{ 1, 0 } : SurfaceRef{ -1, -1 };
            };
            addressTarget(workspace.draft);
            for ( auto& snapshot : workspace.history->snapshots ) addressTarget(snapshot);
            check(view->restoreStagedCleaveWorkspace(family, std::move(workspace)),
                "both topology fixtures hold matching valid target contexts and local timelines");
        }
        view->setSurfaceCleaveToolEnabled(false);
        editor::Selection preceding{};
        preceding.sectors = { 0 };
        window.mutableDocumentStateForTest().selection = preceding;
        view->setSelectionState(0, -1, -1, -1, preceding);
        window.updateEditActionsForTest();
    };
    const auto bothClearedWithOptions = [&]()
    {
        const auto sector = view->captureStagedCleaveWorkspace(GlView::CleaveWorkspaceFamily::Sector);
        const auto surface = view->captureStagedCleaveWorkspace(GlView::CleaveWorkspaceFamily::Surface);
        return sector.draft.lines.empty() && !sector.draft.context.valid && !sector.history
            && surface.draft.lines.empty() && !surface.draft.context.valid && !surface.history
            && sector.stagedMode && surface.stagedMode && !sector.chainEnabled && surface.chainEnabled
            && sector.bounded == true && surface.bounded == false
            && sector.angleStepDegrees == 30.0 && surface.angleStepDegrees == 15.0;
    };
    seedTargetFamilies();
    check(view->stagedCleaveDraftView(GlView::CleaveWorkspaceFamily::Sector).context->targetSectors
                == std::vector<std::int32_t>{ 1 }
            && view->stagedCleaveDraftView(GlView::CleaveWorkspaceFamily::Surface).context->targetSurfaces
                == std::vector<SurfaceRef>{ { 1, 0 } } && remove->isEnabled(),
        "both dormant families retain Sector 1 targets before deleting preceding Sector 0");
    remove->trigger();
    check(window.documentStateForTest().level.sectors.size() == 2
            && bothClearedWithOptions()
            && !view->toastTextForTest(GlView::ToastGroup::Error).isEmpty()
            && window.validateRecoveryEditorStateForTest(window.captureRecoveryEditorStateForTest()),
        "real preceding-Sector Delete invalidates both dormant timelines without changing independent options or retargeting a same index");
    window.updateEditActionsForTest();
    check(undo->isEnabled(), "actual structural Delete remains undoable after both draft invalidations");
    undo->trigger();
    check(window.documentStateForTest().level.sectors.size() == 3 && bothClearedWithOptions(),
        "structural Undo restores Level topology while both invalidated family drafts remain cleared");
    seedTargetFamilies();
    check(redo->isEnabled(), "authoring new independent drafts preserves the real structural Level Redo branch");
    redo->trigger();
    check(window.documentStateForTest().level.sectors.size() == 2 && bothClearedWithOptions()
            && window.validateRecoveryEditorStateForTest(window.captureRecoveryEditorStateForTest()),
        "real structural Redo clears both newly authored dormant families instead of silently reindexing either target");
}

void runCleaveRecoveryRestoreOwnedArrayBudgetTest()
{
    constexpr qsizetype lineCount = 18'000;
    constexpr std::size_t historyBudget = 16u * 1024u * 1024u;

    GlView::RecoveryState::CleaveLine expectedLine{};
    QByteArray payload{};
    {
        MainWindow authoringWindow{};
        prepareInteractiveCleaveWindow(authoringWindow);
        GlView* const authoringView = authoringWindow.glViewForTest();
        check(authoringView != nullptr,
            "Cleave restore-allocation fixture owns an authoring viewport");
        if (!authoringView)
        {
            return;
        }

        const auto emptyDraft = authoringView->captureStagedCleaveDraftState();
        check(emptyDraft.context.valid
                && emptyDraft.context.targetSectors == std::vector<std::int32_t>{ 0 },
            "Cleave restore-allocation fixture captures a valid selected-sector context");
        if (!emptyDraft.context.valid
            || emptyDraft.context.targetSectors != std::vector<std::int32_t>{ 0 })
        {
            return;
        }

        expectedLine.first = TVector{ 0.25, 0.25, 0.5 };
        expectedLine.second = TVector{ 0.75, 0.25, 0.5 };
        expectedLine.planeNormal = TVector{ 0.0, 1.0, 0.0 };
        expectedLine.constructionPlaneNormal = TVector{ 0.0, 0.0, 1.0 };
        expectedLine.angleReferenceAxis = TVector{ 1.0, 0.0, 0.0 };
        GlView::RecoveryState::CleaveDraftSnapshot sourceDraft{};
        sourceDraft.lines = { expectedLine };
        sourceDraft.context = emptyDraft.context;
        sourceDraft.applyOrder = { 0 };
        GlView::RecoveryState::CleaveHistory sourceHistory{};
        sourceHistory.snapshots = { sourceDraft };
        sourceHistory.cursor = 0;
        authoringView->restoreStagedCleaveDraftState(
            std::move(sourceDraft), std::move(sourceHistory));

        const QByteArray smallPayload =
            authoringWindow.captureRecoveryEditorStateForTest();
        QCborMap root = editorRoot(smallPayload);
        QCborArray viewport = viewportArray(root);
        if (viewport.size() <= 38 || !viewport.at(38).isMap())
        {
            check(false,
                "Cleave restore-allocation fixture captures the versioned EDIT Cleave map");
            return;
        }

        const QCborMap smallCleave = viewport.at(38).toMap();
        const QCborArray oneLine = smallCleave.value(
            QStringLiteral("lines")).toArray();
        const QCborArray context = smallCleave.value(
            QStringLiteral("context")).toArray();
        check(oneLine.size() == 1 && context.size() == 10
                && context.at(8).toArray() == selectedLines({ 0 }),
            "Cleave restore-allocation fixture starts from a real writer-shaped current draft and context");
        if (oneLine.size() != 1 || context.size() != 10
            || context.at(8).toArray() != selectedLines({ 0 }))
        {
            return;
        }

        QCborArray largeLines{};
        QCborArray applyOrder{};
        const QCborValue encodedLine = oneLine.at(0);
        for (qsizetype index = 0; index < lineCount; ++index)
        {
            largeLines.append(encodedLine);
            applyOrder.append(static_cast<qint64>(index));
        }
        const QCborMap emptyLineSelection{
            { QStringLiteral("lines"), QCborArray{} },
            { QStringLiteral("active"), -1 },
        };
        QCborMap cursorSnapshot{
            { QStringLiteral("lines"), largeLines },
            { QStringLiteral("context"), context },
            { QStringLiteral("applyOrder"), applyOrder },
            { QStringLiteral("selection"), emptyLineSelection },
        };
        QCborMap largeCleave = smallCleave;
        largeCleave.remove(QStringLiteral("chainOrigin"));
        largeCleave.insert(QStringLiteral("lines"), largeLines);
        largeCleave.insert(QStringLiteral("applyOrder"), applyOrder);
        largeCleave.insert(QStringLiteral("selection"), emptyLineSelection);
        QCborArray options = largeCleave.value(QStringLiteral("options")).toArray();
        if (options.size() == 8)
        {
            options[5] = -1;
            largeCleave.insert(QStringLiteral("options"), options);
        }
        largeCleave.insert(QStringLiteral("history"), QCborMap{
            { QStringLiteral("version"), 1 },
            { QStringLiteral("cursor"), 0 },
            { QStringLiteral("snapshots"), QCborArray{ cursorSnapshot } },
        });
        viewport[38] = largeCleave;
        root.insert(QStringLiteral("viewport"), viewport);
        payload = editorPayloadWithRoot(std::move(root));
    }

    MainWindow restoredWindow{};
    prepareInteractiveCleaveWindow(restoredWindow);
    GlView* const restoredView = restoredWindow.glViewForTest();
    check(restoredView != nullptr,
        "Cleave restore-allocation fixture owns a fresh destination viewport");
    if (!restoredView)
    {
        return;
    }

    QString warning{};
    const bool admitted = restoredWindow.validateRecoveryEditorStateForTest(
        payload, &warning);
    check(admitted && warning.isEmpty(),
        "writer-shaped 18k-line EDIT with one exact cursor snapshot passes the real checked decoder");
    if (!admitted)
    {
        return;
    }

    restoredView->beginCleaveRecoveryRestoreOwnedArrayTrackingForTest();
    const bool restored = restoredWindow.restoreRecoveryEditorStateForTest(payload);
    const std::size_t restoreSourceOwnedArrayBytes =
        restoredView->cleaveRecoveryRestoreSourceOwnedArrayBytesForTest();
    const std::size_t restorePeakOwnedArrayBytes =
        restoredView->cleaveRecoveryRestorePeakOwnedArrayBytesForTest();
    const std::size_t restoredLiveOwnedArrayBytes =
        restoredView->cleaveRecoveryLiveOwnedArrayBytesForTest();
    restoredView->endCleaveRecoveryRestoreOwnedArrayTrackingForTest();
    check(restored,
        "a decoder-admitted large Cleave EDIT restores successfully through the real recovery path");
    const std::size_t minimumIncomingLineAndOrderBytes =
        static_cast<std::size_t>(lineCount) * 2u
            * (sizeof(GlView::RecoveryState::CleaveLine) + sizeof(std::int32_t));
    check(restoreSourceOwnedArrayBytes >= minimumIncomingLineAndOrderBytes
            && restorePeakOwnedArrayBytes
                >= restoreSourceOwnedArrayBytes + restoredLiveOwnedArrayBytes,
        "restore allocation observer counts incoming current/history buffers and installed live arrays");
    check(restorePeakOwnedArrayBytes <= historyBudget,
        "current EDIT restore keeps simultaneously owned Cleave line/order arrays within 16 MiB");
    if (!restored)
    {
        return;
    }

    const GlView::RecoveryState restoredState = restoredView->recoveryState();
    bool exactCurrentAndHistory = restoredState.cleaveLines.size()
            == static_cast<std::size_t>(lineCount)
        && restoredState.applyOrder.size() == static_cast<std::size_t>(lineCount)
        && restoredState.selectedCleaveLines.empty()
        && restoredState.selectedCleaveLine == -1
        && restoredState.cleaveHistory
        && restoredState.cleaveHistory->cursor == 0
        && restoredState.cleaveHistory->snapshots.size() == 1;
    if (exactCurrentAndHistory)
    {
        const auto& snapshot = restoredState.cleaveHistory->snapshots.front();
        exactCurrentAndHistory = snapshot.lines.size()
                == static_cast<std::size_t>(lineCount)
            && snapshot.applyOrder.size() == static_cast<std::size_t>(lineCount)
            && snapshot.selectedCleaveLines.empty()
            && snapshot.selectedCleaveLine == -1
            && restoredState.cleaveContext.valid
            && restoredState.cleaveContext.targetSectors
                == std::vector<std::int32_t>{ 0 }
            && snapshot.context.valid
            && snapshot.context.targetSectors == std::vector<std::int32_t>{ 0 };
        for (std::size_t index = 0;
             exactCurrentAndHistory && index < static_cast<std::size_t>(lineCount);
             ++index)
        {
            exactCurrentAndHistory = sameLine(restoredState.cleaveLines[index], expectedLine)
                && sameLine(snapshot.lines[index], expectedLine)
                && restoredState.applyOrder[index] == static_cast<std::int32_t>(index)
                && snapshot.applyOrder[index] == static_cast<std::int32_t>(index);
        }
    }
    check(exactCurrentAndHistory,
        "large Cleave EDIT retains exact current and cursor arrays, empty selection, and local timeline after budget tracking");

    // This exact 18k-line payload passed the checked writer-shaped EDIT path
    // above. Add an already-settled runtime draft to the same live viewport so
    // the GlView restore preflight sees the complete aggregate workset before
    // replacing it. processRecoveryEditorState resets pointer ownership before
    // publishing a recovery, so exercise the lower-level restore seam directly
    // to preserve the old live arrays across a refusal.
    constexpr std::size_t existingLiveLineCount = 8'000u;
    MainWindow rejectedWindow{};
    prepareInteractiveCleaveWindow(rejectedWindow);
    GlView* const rejectedView = rejectedWindow.glViewForTest();
    QString overBudgetWarning{};
    const bool overBudgetInputAdmitted = rejectedWindow
        .validateRecoveryEditorStateForTest(payload, &overBudgetWarning);
    GlView::RecoveryState::CleaveDraftSnapshot existingDraft = rejectedView
        ? rejectedView->captureStagedCleaveDraftState()
        : GlView::RecoveryState::CleaveDraftSnapshot{};
    existingDraft.lines.assign(existingLiveLineCount, expectedLine);
    existingDraft.applyOrder.resize(existingLiveLineCount);
    for (std::size_t index = 0; index < existingLiveLineCount; ++index)
    {
        existingDraft.applyOrder[index] = static_cast<std::int32_t>(index);
    }
    existingDraft.selectedCleaveLines.clear();
    existingDraft.selectedCleaveLine = -1;
    GlView::RecoveryState::CleaveHistory existingHistory{};
    existingHistory.snapshots.push_back(existingDraft);
    existingHistory.cursor = 0u;
    if (rejectedView)
    {
        rejectedView->restoreStagedCleaveDraftState(
            std::move(existingDraft), std::move(existingHistory));
    }
    const GlView::RecoveryState existingRuntimeState = rejectedView
        ? rejectedView->recoveryState()
        : GlView::RecoveryState{};
    const bool existingRuntimeSeeded = rejectedView
        && rejectedView->sectorCleaveToolEnabled()
        && existingRuntimeState.cleaveLines.size() == existingLiveLineCount
        && existingRuntimeState.applyOrder.size() == existingLiveLineCount
        && existingRuntimeState.cleaveHistory
        && existingRuntimeState.cleaveHistory->cursor == 0u
        && existingRuntimeState.cleaveHistory->snapshots.size() == 1u
        && existingRuntimeState.cleaveContext.valid
        && existingRuntimeState.cleaveContext.targetSectors
            == std::vector<std::int32_t>{ 0 };

    const GlView::RecoveryState incomingState = restoredState;
    check(incomingState.cleaveLines.size()
                == static_cast<std::size_t>(lineCount)
            && incomingState.cleaveHistory
            && incomingState.cleaveHistory->cursor == 0u
            && incomingState.cleaveHistory->snapshots.size() == 1u,
        "aggregate refusal reuses the state captured from a real decoded 18k-line restore");
    GlView::RecoveryState::CleaveDraftSnapshot existingContextOnly{};
    existingContextOnly.context = existingRuntimeState.cleaveContext;
    GlView::RecoveryState::CleaveDraftSnapshot incomingContextOnly{};
    incomingContextOnly.context = incomingState.cleaveContext;
    const bool restoreContextsMatch =
        sameDraftSnapshot(existingContextOnly, incomingContextOnly);

    const std::size_t existingLiveBytesBefore = rejectedView
        ? rejectedView->cleaveRecoveryLiveOwnedArrayBytesForTest()
        : 0u;
    const std::size_t minimumIncomingStateOwnedBytes =
        static_cast<std::size_t>(lineCount) * 2u
            * (sizeof(GlView::RecoveryState::CleaveLine) + sizeof(std::int32_t));
    const std::size_t minimumAggregateWorksetBytes =
        2u * minimumIncomingStateOwnedBytes + existingLiveBytesBefore;
    if (rejectedView)
    {
        rejectedView->beginCleaveRecoveryRestoreOwnedArrayTrackingForTest();
    }
    bool overBudgetRestoreAccepted = false;
    bool overBudgetRestoreThrew = false;
    QString overBudgetRestoreException{};
    try
    {
        overBudgetRestoreAccepted = rejectedView
            && rejectedView->restoreRecoveryState(
                incomingState, false, &overBudgetWarning);
    }
    catch (const std::exception& exception)
    {
        overBudgetRestoreThrew = true;
        overBudgetRestoreException = QString::fromUtf8(exception.what());
    }
    const std::size_t overBudgetPeakBytes = rejectedView
        ? rejectedView->cleaveRecoveryRestorePeakOwnedArrayBytesForTest()
        : 0u;
    const std::size_t existingLiveBytesAfter = rejectedView
        ? rejectedView->cleaveRecoveryLiveOwnedArrayBytesForTest()
        : 0u;
    if (rejectedView)
    {
        rejectedView->endCleaveRecoveryRestoreOwnedArrayTrackingForTest();
    }
    const GlView::RecoveryState rejectedState = rejectedView
        ? rejectedView->recoveryState()
        : GlView::RecoveryState{};
    const bool existingDraftPreserved = existingRuntimeSeeded
        && sameDraftSnapshot(
            currentDraft(existingRuntimeState), currentDraft(rejectedState))
        && existingRuntimeState.cleaveHistory
        && rejectedState.cleaveHistory
        && sameCleaveHistory(
            *existingRuntimeState.cleaveHistory,
            *rejectedState.cleaveHistory);
    if (!overBudgetInputAdmitted || overBudgetRestoreAccepted
        || overBudgetRestoreThrew
        || !overBudgetWarning.contains(QStringLiteral("16 MiB"))
        || !existingRuntimeSeeded || !existingDraftPreserved
        || !restoreContextsMatch
        || minimumAggregateWorksetBytes <= historyBudget)
    {
        std::cerr << "DIAG Cleave restore aggregate refusal: admitted="
                  << overBudgetInputAdmitted
                  << " accepted=" << overBudgetRestoreAccepted
                  << " threw=" << overBudgetRestoreThrew
                  << " exception=\"" << overBudgetRestoreException.toStdString()
                  << "\" warning=\"" << overBudgetWarning.toStdString()
                  << "\" liveBefore=" << existingLiveBytesBefore
                  << " peak=" << overBudgetPeakBytes
                  << " liveAfter=" << existingLiveBytesAfter
                  << " minimumAggregate=" << minimumAggregateWorksetBytes
                  << " seeded=" << existingRuntimeSeeded
                  << " contextMatch=" << restoreContextsMatch
                  << " oldDraftPreserved=" << existingDraftPreserved << '\n';
    }
    check(overBudgetInputAdmitted,
        "the 18k-line current/history payload passes bounded wire admission");
    check(existingRuntimeSeeded,
        "the aggregate refusal fixture starts with a settled same-context live draft and baseline");
    check(restoreContextsMatch,
        "the incoming valid EDIT and existing draft target the same captured viewport context");
    check(minimumAggregateWorksetBytes > historyBudget,
        "incoming source, candidate, and settled live arrays exceed the aggregate restore budget");
    check(!overBudgetRestoreAccepted && !overBudgetRestoreThrew
            && overBudgetWarning.contains(QStringLiteral("16 MiB")),
        "an aggregate-over-budget Cleave restore returns a 16 MiB refusal without throwing");
    check(existingDraftPreserved
            && existingLiveBytesBefore == existingLiveBytesAfter,
        "a refused Cleave restore preserves the exact pre-existing live draft and timeline");

    constexpr qsizetype currentOnlyLineCount = 29'000;
    QCborMap currentOnlyRoot = editorRoot(payload);
    QCborArray currentOnlyViewport = viewportArray(currentOnlyRoot);
    QCborMap currentOnlyCleave = currentOnlyViewport.at(38).toMap();
    const QCborArray encodedCurrentLines = currentOnlyCleave.value(
        QStringLiteral("lines")).toArray();
    const QCborValue encodedCurrentLine = encodedCurrentLines.at(0);
    QCborArray currentOnlyLines{};
    QCborArray currentOnlyOrder{};
    for (qsizetype index = 0; index < currentOnlyLineCount; ++index)
    {
        currentOnlyLines.append(encodedCurrentLine);
        currentOnlyOrder.append(static_cast<qint64>(index));
    }
    currentOnlyCleave.insert(QStringLiteral("lines"), currentOnlyLines);
    currentOnlyCleave.insert(QStringLiteral("applyOrder"), currentOnlyOrder);
    currentOnlyCleave.remove(QStringLiteral("history"));
    currentOnlyViewport[38] = currentOnlyCleave;
    currentOnlyRoot.insert(QStringLiteral("viewport"), currentOnlyViewport);
    const QByteArray currentOnlyPayload =
        editorPayloadWithRoot(std::move(currentOnlyRoot));

    MainWindow currentOnlyWindow{};
    prepareInteractiveCleaveWindow(currentOnlyWindow);
    GlView* const currentOnlyView = currentOnlyWindow.glViewForTest();
    QString currentOnlyWarning{};
    const bool currentOnlyAdmitted =
        currentOnlyWindow.validateRecoveryEditorStateForTest(
            currentOnlyPayload, &currentOnlyWarning);
    if (currentOnlyView)
    {
        currentOnlyView->beginCleaveRecoveryRestoreOwnedArrayTrackingForTest();
    }
    const bool currentOnlyRestored =
        currentOnlyWindow.restoreRecoveryEditorStateForTest(
            currentOnlyPayload, &currentOnlyWarning);
    const std::size_t currentOnlySourceBytes = currentOnlyView
        ? currentOnlyView->cleaveRecoveryRestoreSourceOwnedArrayBytesForTest()
        : 0u;
    const std::size_t currentOnlyPeakBytes = currentOnlyView
        ? currentOnlyView->cleaveRecoveryRestorePeakOwnedArrayBytesForTest()
        : 0u;
    const std::size_t currentOnlyLiveBytes = currentOnlyView
        ? currentOnlyView->cleaveRecoveryLiveOwnedArrayBytesForTest()
        : 0u;
    if (currentOnlyView)
    {
        currentOnlyView->endCleaveRecoveryRestoreOwnedArrayTrackingForTest();
    }
    const GlView::RecoveryState currentOnlyState = currentOnlyView
        ? currentOnlyView->recoveryState()
        : GlView::RecoveryState{};
    bool exactCurrentOnlyDraft = currentOnlyState.cleaveLines.size()
            == static_cast<std::size_t>(currentOnlyLineCount)
        && currentOnlyState.applyOrder.size()
            == static_cast<std::size_t>(currentOnlyLineCount)
        && currentOnlyState.cleaveContext.valid
        && currentOnlyState.cleaveContext.targetSectors
            == std::vector<std::int32_t>{ 0 };
    for (std::size_t index = 0;
         exactCurrentOnlyDraft
             && index < static_cast<std::size_t>(currentOnlyLineCount);
         ++index)
    {
        exactCurrentOnlyDraft = sameLine(currentOnlyState.cleaveLines[index], expectedLine)
            && currentOnlyState.applyOrder[index] == static_cast<std::int32_t>(index);
    }
    check(currentOnlyAdmitted,
        "a valid current-only legacy Cleave EDIT passes checked decoder admission");
    check(currentOnlyRestored && exactCurrentOnlyDraft,
        "an over-budget baseline clone does not discard the current-only legacy draft");
    const std::size_t minimumCurrentOnlySourceBytes =
        static_cast<std::size_t>(currentOnlyLineCount)
            * (sizeof(GlView::RecoveryState::CleaveLine) + sizeof(std::int32_t));
    check(currentOnlySourceBytes >= minimumCurrentOnlySourceBytes
            && currentOnlyPeakBytes
                >= currentOnlySourceBytes + currentOnlyLiveBytes,
        "current-only restore observer samples source plus the post-baseline live arrays");
    check(currentOnlyPeakBytes <= historyBudget,
        "current-only restore retains its draft without exceeding the 16 MiB workset");
    check(!currentOnlyState.cleaveHistory,
        "current-only recovery omits its initial Undo baseline when that copy exceeds the aggregate limit");
    check(currentOnlyView && currentOnlyView->stagedCleaveHistoryOwnsUndo()
            && !currentOnlyView->canUndoStagedCleaveHistory()
            && !currentOnlyView->canRedoStagedCleaveHistory(),
        "current-only recovery keeps local Undo routing without publishing an unavailable baseline");
}

void runCleaveAsymmetricMatrixRestoreTest()
{
    MainWindow window{};
    prepareInteractiveCleaveWindow(window);
    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "nonsymmetric matrix fixture owns a viewport");
    if (!view)
    {
        return;
    }

    GlView::RecoveryState::CleaveDraftSnapshot draft =
        view->captureStagedCleaveDraftState();
    GlView::RecoveryState::CleaveLine line{};
    line.first = TVector{ 0.25, 0.375, 0.5 };
    line.second = TVector{ 0.75, 0.625, 0.5 };
    line.planeNormal = TVector{ -0.4472135954999579, 0.8944271909999159, 0.0 };
    line.constructionPlaneNormal = TVector{ 0.0, 0.0, 1.0 };
    line.angleReferenceAxis = TVector{ 1.0, 0.0, 0.0 };
    draft.lines = { line };
    draft.applyOrder = { 0 };
    draft.selectedCleaveLines = { 0 };
    draft.selectedCleaveLine = 0;
    GlView::RecoveryState::CleaveHistory history{};
    history.snapshots = { draft };
    history.cursor = 0;
    view->restoreStagedCleaveDraftState(draft, history);
    check(view->stagedCleaveLineCount() == 1
            && view->captureStagedCleaveHistory()
            && view->sectorCleaveToolEnabled(),
        "nonsymmetric matrix fixture installs a live line and coherent local history");

    // Build a real asymmetric camera frame before capturing EDIT. Restoring
    // Cleave context can then legitimately retain its projection/view through
    // the ordinary viewport validation and publication path.
    GlView::RecoveryState orbitState = view->recoveryState();
    orbitState.editor.projectionType =
        sed::app::ViewportCamera::ProjectionType::Perspective;
    orbitState.editor.fovXDeg = 73.0f;
    orbitState.editor.orbit.center = QVector3D(1.25f, -0.75f, 0.5f);
    orbitState.editor.orbit.pan = QVector3D(0.125f, -0.25f, 0.375f);
    orbitState.editor.orbit.yawDeg = 37.0f;
    orbitState.editor.orbit.pitchDeg = 23.0f;
    orbitState.editor.orbit.rollDeg = 14.0f;
    orbitState.editor.orbit.distance = 12.5f;
    view->restoreRecoveryState(orbitState, false);
    view->captureStagedCleaveContextForTest(false);
    const GlView::RecoveryState::CleaveDraftSnapshot authoredDraft =
        view->captureStagedCleaveDraftState();
    check(authoredDraft.context.valid,
        "asymmetric camera fixture captures a current Cleave projection/view pair");
    if (!authoredDraft.context.valid)
    {
        return;
    }

    const auto asColumnMajor = [](const QMatrix4x4& matrix)
    {
        std::array<double, 16> values{};
        const float* const source = matrix.constData();
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            values[index] = static_cast<double>(source[index]);
        }
        return values;
    };
    const std::array<double, 16> projectionValues =
        asColumnMajor(authoredDraft.context.projection);
    const std::array<double, 16> viewValues =
        asColumnMajor(authoredDraft.context.view);
    check(authoredDraft.context.projection(2, 3)
            != authoredDraft.context.projection(3, 2)
            && authoredDraft.context.view(0, 3)
                != authoredDraft.context.view(3, 0),
        "asymmetric camera fixture uses valid nonsymmetric projection and view matrices");
    const QByteArray payload = window.captureRecoveryEditorStateForTest();
    const QCborMap root = editorRoot(payload);
    const QCborArray viewport = viewportArray(root);
    const QCborMap cleave = viewport.size() > 38
        ? viewport.at(38).toMap() : QCborMap{};
    const QCborArray context = cleave.value(QStringLiteral("context")).toArray();
    check(context.size() == 10,
        "nonsymmetric matrix fixture serializes a complete current Cleave context");
    check(window.validateRecoveryEditorStateForTest(payload),
        "nonsymmetric Cleave matrix payload validates before restore");
    check(window.restoreRecoveryEditorStateForTest(payload),
        "nonsymmetric Cleave matrix payload restores");

    const GlView::RecoveryState restored = view
        ? view->recoveryState() : GlView::RecoveryState{};
    const auto matrixMatches = [](const QMatrix4x4& matrix,
                                  const std::array<double, 16>& values)
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                if (matrix(row, column)
                    != static_cast<float>(values[static_cast<std::size_t>(column * 4 + row)]))
                {
                    return false;
                }
            }
        }
        return true;
    };
    check(view
            && view->stagedCleaveLineCount() == 1
            && matrixMatches(restored.cleaveContext.projection, projectionValues)
            && matrixMatches(restored.cleaveContext.view, viewValues),
        "Cleave recovery preserves a valid asymmetric camera context in a retained nonempty draft");
}

void runCleaveApplyMementoTests(const std::filesystem::path& root)
{
    MainWindow window{};
    prepareInteractiveCleaveWindow(window);
    GlView* const view = window.glViewForTest();
    QAction* const undoAction = window.findChild<QAction*>(QStringLiteral("edit.undo"));
    QAction* const redoAction = window.findChild<QAction*>(QStringLiteral("edit.redo"));
    check(view && undoAction && redoAction,
        "Cleave Apply memento fixture exposes viewport and native history actions");
    if (!view || !undoAction || !redoAction)
    {
        return;
    }

    editor::Selection selectionWithActiveSurface =
        window.documentStateForTest().selection;
    selectionWithActiveSurface.surfaces.insert(SurfaceRef{ 0, 5 });
    window.mutableDocumentStateForTest().selection = selectionWithActiveSurface;
    view->setSelectionState(0, 5, -1, -1, selectionWithActiveSurface);

    view->addStagedCleaveLineForTest(
        QVector3D(0.5f, -1.0f, 0.5f), QVector3D(0.5f, 2.0f, 0.5f),
        QVector3D(1.0f, 0.0f, 0.0f));
    const GlView::RecoveryState::CleaveDraftSnapshot baseline =
        view->captureStagedCleaveDraftState();
    view->addStagedCleaveLineForTest(
        QVector3D(-1.0f, 0.5f, 0.5f), QVector3D(2.0f, 0.5f, 0.5f),
        QVector3D(0.0f, 1.0f, 0.0f));
    GlView::RecoveryState::CleaveDraftSnapshot current =
        view->captureStagedCleaveDraftState();
    current.applyOrder = { 1, 0 };
    current.selectedCleaveLines = { 0, 1 };
    current.selectedCleaveLine = 1;
    current.chainOrigin = GlView::RecoveryState::CleaveChainOrigin{ 1, 0 };
    GlView::RecoveryState::CleaveDraftSnapshot future = current;
    future.lines[1].first.y += 0.125;
    future.lines[1].second.y += 0.125;
    GlView::RecoveryState::CleaveHistory history{};
    history.snapshots = { baseline, current, future };
    history.cursor = 1;
    view->restoreStagedCleaveDraftState(current, history);

    const GlView::RecoveryState::CleaveDraftSnapshot expectedDraft =
        view->captureStagedCleaveDraftState();
    const auto capturedHistory = view->captureStagedCleaveHistory();
    check(capturedHistory.has_value(),
        "Cleave Apply memento fixture prepares a retained local history branch");
    if (!capturedHistory)
    {
        return;
    }
    const GlView::RecoveryState::CleaveHistory expectedHistory = *capturedHistory;
    const editor::DocumentState beforeApply = window.documentStateForTest();
    const std::size_t undoDepthBefore = window.undoDepthForTest();
    window.markCurrentDocumentAsSavedForTest();
    QToolButton* const applyButton = window.findChild<QToolButton*>(
        QStringLiteral("cleaveApplyButton"));
    check(applyButton && applyButton->isEnabled(),
        "the real staged-Cleave Apply control is enabled for the valid interior cuts");
    if (!applyButton || !applyButton->isEnabled())
    {
        return;
    }
    applyButton->click();
    const std::size_t sectorsAfterApply =
        window.documentStateForTest().level.sectors.size();
    check(view->stagedCleaveLineCount() == 0
            && window.undoDepthForTest() == undoDepthBefore + 1
            && sectorsAfterApply > beforeApply.level.sectors.size(),
        "a valid two-line Sector Apply publishes one Level transaction and clears the live draft");
    if (view->stagedCleaveLineCount() != 0
        || window.undoDepthForTest() != undoDepthBefore + 1
        || sectorsAfterApply <= beforeApply.level.sectors.size())
    {
        return;
    }

    QApplication::setActiveWindow(&window);
    view->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents(QEventLoop::AllEvents);
    undoAction->trigger();
    const GlView::RecoveryState afterUndo = view->recoveryState();
    check(sameSelection(window.documentStateForTest().selection, beforeApply.selection)
            && view->activeSurfaceSectorIndex() == 0
            && view->activeSurfaceIndex() == 5
            && window.editModeForTest() == window.sectorEditModeValueForTest()
            && sameDraftSnapshot(currentDraft(afterUndo), expectedDraft)
            && afterUndo.cleaveHistory
            && sameCleaveHistory(*afterUndo.cleaveHistory, expectedHistory),
        "Undo of Apply restores exact document selection, edit mode, authored draft, and retained local Redo branch");

    // Exercise the immediate, same-window priority before Save changes the
    // recovery publication epoch.  A second Undo returns to the exact saved
    // Apply-Undo side so the following native Save/Open check remains separate.
    redoAction->trigger();
    check(window.documentStateForTest().level.sectors.size() == sectorsAfterApply
            && view->stagedCleaveLineCount() == 0,
        "immediate document Redo takes priority over the restored local Redo branch");
    undoAction->trigger();
    const GlView::RecoveryState afterSecondUndo = view->recoveryState();
    check(sameDraftSnapshot(currentDraft(afterSecondUndo), expectedDraft)
            && afterSecondUndo.cleaveHistory
            && sameCleaveHistory(*afterSecondUndo.cleaveHistory, expectedHistory),
        "a repeated Apply Undo restores the immutable original workspace attachment");

    QAction* const originalUndo = window.findChild<QAction*>(QStringLiteral("edit.undo"));
    QAction* const originalRedo = window.findChild<QAction*>(QStringLiteral("edit.redo"));
    const std::filesystem::path savedAsPath = root / "apply-undone-save-as.sed";
    const int saveAsErrors = saveAsToPath(
        window.findChild<QAction*>(QStringLiteral("file.save_as")), savedAsPath);
    check(saveAsErrors == 0 && QFileInfo::exists(
                QString::fromStdWString(savedAsPath.wstring()))
            && originalRedo && originalRedo->isEnabled(),
        "native Save As keeps the same-document Apply Redo attachment and priority");
    if (saveAsErrors == 0 && originalRedo && originalRedo->isEnabled())
    {
        originalRedo->trigger();
        check(window.documentStateForTest().level.sectors.size() == sectorsAfterApply
                && view->stagedCleaveLineCount() == 0,
            "same-document Redo still applies after Save As");
        if (originalUndo)
        {
            originalUndo->trigger();
        }
        const GlView::RecoveryState afterSaveAsUndo = view->recoveryState();
        check(sameDraftSnapshot(currentDraft(afterSaveAsUndo), expectedDraft)
                && afterSaveAsUndo.cleaveHistory
                && sameCleaveHistory(*afterSaveAsUndo.cleaveHistory, expectedHistory),
            "Undo after Save As still restores the immutable Cleave memento");
    }
    QAction* const saveAction = window.findChild<QAction*>(QStringLiteral("file.save"));
    const int saveErrors = triggerActionAndDismissDialogs(saveAction);
    check(saveErrors == 0 && QFileInfo::exists(
                QString::fromStdWString(savedAsPath.wstring()))
            && originalRedo && originalRedo->isEnabled(),
        "native Save keeps the same-document Apply Redo attachment and priority");
    if (saveErrors == 0 && originalRedo && originalRedo->isEnabled())
    {
        originalRedo->trigger();
        check(window.documentStateForTest().level.sectors.size() == sectorsAfterApply
                && view->stagedCleaveLineCount() == 0,
            "same-document Redo still applies after native Save");
        if (originalUndo)
        {
            originalUndo->trigger();
        }
    }
    MainWindow reopened{};
    reopened.setAttribute(Qt::WA_DontShowOnScreen);
    reopened.setRecoveryDirectoryForTest(root / "apply-reopen-recovery");
    reopened.openLevelFromFilenameForTest(
        QString::fromStdWString(savedAsPath.wstring()));
    QAction* const reopenedRedo = reopened.findChild<QAction*>(QStringLiteral("edit.redo"));
    if (GlView* const reopenedView = reopened.glViewForTest())
    {
        QApplication::setActiveWindow(&reopened);
        reopenedView->setFocus(Qt::OtherFocusReason);
        QApplication::processEvents(QEventLoop::AllEvents);
        if (reopenedRedo)
        {
            reopenedRedo->trigger();
        }
        check(reopenedRedo && reopened.documentStateForTest().level.sectors.size()
                > beforeApply.level.sectors.size()
                && reopenedView->stagedCleaveLineCount() == 0,
            "native Save/reopen rebinds the pending Apply Redo to the imported HIST record");
    }

}

void runCleaveAuthoredRecoveryUndoTests(const std::filesystem::path& root)
{
    MainWindow window{};
    prepareInteractiveCleaveWindow(window);
    window.setRecoveryDirectoryForTest(root / "cleave-undo-clean-recovery");
    window.beginRecoveryDocumentForTest(root / "cleave-undo-clean.sed", false);
    window.triggerForcedRecoverySnapshotForTest();
    window.waitForRecoverySnapshotForTest();

    GlView* const view = window.glViewForTest();
    QAction* const undoAction = window.findChild<QAction*>(
        QStringLiteral("edit.undo"));
    check(view && undoAction,
        "Cleave recovery Undo fixture exposes a viewport and local Undo action");
    if (!view || !undoAction)
    {
        return;
    }

    check(drawCleaveLineAtWorldPoints(
            *view,
            QVector3D(0.5f, -1.0f, 0.5f),
            QVector3D(0.5f, 2.0f, 0.5f))
            && view->stagedCleaveLineCount() == 1,
        "Cleave recovery Undo fixture authors one real local-history line");
    QTimer* const authoredDebounce = window.findChild<QTimer*>(
        QStringLiteral("recoveryDebounceTimer"));
    check(authoredDebounce && authoredDebounce->isActive(),
        "a settled Cleave edit starts the authored recovery debounce");

    QApplication::setActiveWindow(&window);
    view->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents(QEventLoop::AllEvents);
    undoAction->trigger();
    check(view->stagedCleaveLineCount() == 0
            && !window.hasUnsavedChangesForTest(),
        "local Undo to the saved empty Cleave draft refreshes the cached dirty projection");

    window.triggerForcedRecoverySnapshotForTest();
    window.waitForRecoverySnapshotForTest();
    const auto cleanCheckpoint = ui::recovery::readRecoveryRecord(
        window.currentRecoveryPathForTest());
    check(cleanCheckpoint && !cleanCheckpoint->metadata.requiresUserRecovery,
        "a recovery checkpoint after local Undo to saved Cleave state is not marked as user recovery");
    check(!window.hasRecoveryOfferEligibleChangesForTest(),
        "local Undo to saved Cleave state is ineligible for a recovery offer after checkpoint publication");

    view->setCameraOrthoScale(12.0f);
    check(!window.hasUnsavedChangesForTest()
            && !window.hasRecoveryOfferEligibleChangesForTest()
            && (!authoredDebounce || !authoredDebounce->isActive()),
        "camera changes after returning to the saved Cleave draft remain recovery-quiet");
    window.discardCurrentRecoveryForTest();
}

void runCleaveReplacementAuthorizationRaceTests(
    const std::filesystem::path& root)
{
    MainWindow window{};
    prepareInteractiveCleaveWindow(window);
    window.setRecoveryDirectoryForTest(root / "cleave-replacement-race-recovery");
    const std::filesystem::path sourcePath = root / "cleave-replacement-race.sed";
    window.beginRecoveryDocumentForTest(sourcePath, false);
    window.markCurrentDocumentAsSavedForTest();
    window.pushUndoForTitleForTest();

    const QCborMap cogTab{
        { QStringLiteral("name"), QStringLiteral("replacement-race.cog") },
        { QStringLiteral("path"), QString{} },
        { QStringLiteral("text"), QStringLiteral("symbols") },
        { QStringLiteral("modified"), true },
        { QStringLiteral("cursor"), 0 },
        { QStringLiteral("anchor"), 0 },
        { QStringLiteral("vscroll"), 0 },
        { QStringLiteral("hscroll"), 0 },
        { QStringLiteral("whitespace"), false },
        { QStringLiteral("zoom"), 100 },
        { QStringLiteral("findVisible"), false },
        { QStringLiteral("findText"), QString{} },
        { QStringLiteral("findWhole"), false },
        { QStringLiteral("findCase"), false },
        { QStringLiteral("splitter"), QCborArray{ 600, 300 } },
    };
    const QCborMap cogRoot{
        { QStringLiteral("version"), 1 },
        { QStringLiteral("active"), 0 },
        { QStringLiteral("dockVisible"), false },
        { QStringLiteral("tabs"), QCborArray{ cogTab } },
    };
    const bool restoredCog = window.restoreCogEditorRecoveryStateForTest(
        ui::recovery::serializeCanonicalCbor(QCborValue(cogRoot)));
    check(restoredCog && window.hasUnsavedCogScriptChangesForTest(),
        "Cleave replacement race fixture prepares a modified COG tab for the later confirmation");
    if (!restoredCog || !window.hasUnsavedCogScriptChangesForTest())
    {
        return;
    }

    window.triggerForcedRecoverySnapshotForTest();
    window.waitForRecoverySnapshotForTest();
    const std::filesystem::path retainedRecoveryPath =
        window.currentRecoveryPathForTest();
    check(std::filesystem::is_regular_file(retainedRecoveryPath),
        "Cleave replacement race fixture publishes a recovery family before prompting");

    int promptCount = 0;
    QPointer<QMessageBox> lastPrompt{};
    bool draftMutationApplied = false;
    std::size_t draftLineCountBeforeLaterPrompt = 0;
    std::uint64_t draftRevisionBeforeLaterPrompt = 0;
    QTimer promptDriver{};
    promptDriver.setInterval(5);
    QObject::connect(&promptDriver, &QTimer::timeout, [&]()
        {
            auto* const prompt = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget());
            if (!prompt || lastPrompt == prompt)
            {
                return;
            }
            lastPrompt = prompt;
            ++promptCount;
            if (promptCount == 2)
            {
                if (GlView* const view = window.glViewForTest())
                {
                    draftLineCountBeforeLaterPrompt = view->stagedCleaveLineCount();
                    draftRevisionBeforeLaterPrompt =
                        view->stagedCleaveAuthoredRevision();
                    const bool drawDispatched = drawCleaveLineAtWorldPoints(
                        *view,
                        QVector3D(0.5f, -1.0f, 0.5f),
                        QVector3D(0.5f, 2.0f, 0.5f));
                    draftMutationApplied = drawDispatched
                        && view->stagedCleaveLineCount()
                            == draftLineCountBeforeLaterPrompt + 1u
                        && view->stagedCleaveAuthoredRevision()
                            != draftRevisionBeforeLaterPrompt;
                }
            }
            prompt->done(QMessageBox::Discard);
        });
    promptDriver.start();
    QString deferredDiscardIdentity{};
    const bool replacementAuthorized =
        window.authorizeDocumentReplacementForTest(
            QStringLiteral("opening another level"),
            retainedRecoveryPath,
            &deferredDiscardIdentity);
    promptDriver.stop();
    check(promptCount == 2 && draftMutationApplied
            && !replacementAuthorized
            && window.hasUnsavedChangesForTest()
            && std::filesystem::is_regular_file(retainedRecoveryPath),
        "a Cleave edit during the later COG close prompt cancels Level Discard and retains its recovery family");
    window.discardCurrentRecoveryForTest();
}

} // namespace

void runCleaveWorkspacePersistenceTests()
{
    QTemporaryDir directory{};
    check(directory.isValid(),
        "Cleave workspace persistence tests use an isolated native/recovery directory");
    if (!directory.isValid())
    {
        return;
    }
    const std::filesystem::path root(directory.path().toStdWString());
    runIndependentCleaveWorkspaceSwitchTests();
    runIndependentCleaveAuthoredProjectionTests();
    runIndependentCleaveMapAdmissionTests();
    runIndependentCleaveNativeRecoveryTests(root);
    runIndependentCleaveDocumentResetAndDefaultsTests();
    runIndependentCleaveRestoreBudgetAtomicityTest();
    runCleaveEditRoundTripTests(root);
    runCleaveDormantWorkspaceTests(root);
    runCleaveTypedNormalizationTests();
    runCleaveTypedCheckpointSignalTests();
    runCleaveMainWindowRestoreBudgetAtomicityTest();
    runCleaveTopologyInvalidationTest();
    runCleaveRecoveryRestoreOwnedArrayBudgetTest();
    runCleaveAsymmetricMatrixRestoreTest();
    runCleaveAuthoredRecoveryUndoTests(root);
    runCleaveReplacementAuthorizationRaceTests(root);
    runCleaveApplyMementoTests(root);
}

} // namespace sed::app_tests
