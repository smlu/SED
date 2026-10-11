#include "test_common.hpp"

#include "app_settings.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/level/build.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCursor>
#include <QDoubleSpinBox>
#include <QDialog>
#include <QFocusEvent>
#include <QKeySequenceEdit>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QSettings>
#include <QTemporaryDir>
#include <QWheelEvent>

#ifdef Q_OS_WIN
#  include <QtCore/qt_windows.h>
#endif

#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <utility>

namespace sed::app_tests
{
namespace
{

void focus(QWidget& target)
{
    QApplication::setActiveWindow(target.window());
    target.window()->activateWindow();
    target.setFocus(Qt::OtherFocusReason);
    QCursor::setPos(target.mapToGlobal(target.rect().center()));
    QApplication::processEvents();
}

bool key(QWidget& target, const int code,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier,
    const QString& text = {})
{
    QKeyEvent overrideEvent(QEvent::ShortcutOverride, code, modifiers, text);
    overrideEvent.setAccepted(false);
    QApplication::sendEvent(&target, &overrideEvent);
    QKeyEvent press(QEvent::KeyPress, code, modifiers, text);
    press.setAccepted(false);
    QApplication::sendEvent(&target, &press);
    QKeyEvent release(QEvent::KeyRelease, code, modifiers, text);
    QApplication::sendEvent(&target, &release);
    return overrideEvent.isAccepted();
}

bool wheel(QWidget& target, const int delta,
    const Qt::KeyboardModifiers modifiers = Qt::ControlModifier,
    const QPoint pixelDelta = {})
{
    const QPointF position = target.rect().center();
    QWheelEvent event(position, target.mapToGlobal(position.toPoint()),
        pixelDelta, QPoint(0, delta), Qt::NoButton, modifiers,
        Qt::NoScrollPhase, false);
    event.setAccepted(false);
    QApplication::sendEvent(&target, &event);
    return event.isAccepted();
}

void releaseControl(QWidget& target)
{
    QKeyEvent event(QEvent::KeyRelease, Qt::Key_Control, Qt::NoModifier);
    QApplication::sendEvent(&target, &event);
}

void mouse(QWidget& target, const QEvent::Type type, const QPointF position,
    const Qt::MouseButton button = Qt::NoButton,
    const Qt::MouseButtons buttons = Qt::NoButton,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    const QPointF global = target.mapToGlobal(position.toPoint());
    QMouseEvent event(type, position, global, global, button, buttons, modifiers);
    QApplication::sendEvent(&target, &event);
}

void click(QWidget& target, const QPointF position)
{
    mouse(target, QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::LeftButton);
    mouse(target, QEvent::MouseButtonRelease, position, Qt::LeftButton);
}

QVector3D vector(const TVector& value)
{
    return QVector3D(static_cast<float>(value.x), static_cast<float>(value.y),
        static_cast<float>(value.z));
}

editor::DocumentState document()
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.layers.emplace_back("Layer0");
    level::levelCreateCube(state.level, -2.0, 0.0, -2.0,
        TVector{0.0, -1.0, 0.0}, TVector{4.0, 0.0, 0.0}).layer = 0;
    state.selection.sectors.insert(0);
    return state;
}

void configure(GlView& view, const Level& level, const editor::Selection& selection,
    const bool perspective = false)
{
    view.setLevel(&level);
    view.setSelectionState(0, -1, -1, -1, selection);
    view.setCleaveStagedMode(true);
    view.setCleaveChainEnabled(false);
    view.setCleaveBounded(true);
    view.setSectorCleaveToolEnabled(true);
    view.setCameraProjectionType(perspective
        ? app::ViewportCamera::ProjectionType::Perspective
        : app::ViewportCamera::ProjectionType::Orthographic);
    view.setOrbitViewImmediate(31.0f, 22.0f, 13.0f);
    view.setCameraOrbitDistance(8.0f);
    view.setCameraOrthoScale(96.0f);
    view.setTransformGridSnapEnabled(false);
    view.captureStagedCleaveContextForTest(false);
}

void addGroup(GlView& view)
{
    view.addStagedCleaveLineForTest({-1, 0, -1}, {1, 0, -1}, {0, 0, 1});
    view.addStagedCleaveLineForTest({1, 0, -1}, {1, 0, 1}, {-1, 0, 0});
    view.addStagedCleaveLineForTest({1, 0, 1}, {-1, 0, 1}, {0, 0, -1});
    auto draft = view.captureStagedCleaveDraftState();
    draft.selectedCleaveLines = {0, 1};
    draft.selectedCleaveLine = 1;
    view.restoreStagedCleaveDraftState(draft);
}

bool sameLines(const GlView::RecoveryState::CleaveDraftSnapshot& left,
    const GlView::RecoveryState::CleaveDraftSnapshot& right)
{
    if ( left.lines.size() != right.lines.size() )
    {
        return false;
    }
    for ( std::size_t index = 0; index < left.lines.size(); ++index )
    {
        if ( !closeVec(left.lines[index].first, right.lines[index].first, 0.0)
            || !closeVec(left.lines[index].second, right.lines[index].second, 0.0)
            || !closeVec(left.lines[index].planeNormal, right.lines[index].planeNormal, 0.0)
            || !closeVec(left.lines[index].constructionPlaneNormal,
                right.lines[index].constructionPlaneNormal, 0.0)
            || !closeVec(left.lines[index].angleReferenceAxis,
                right.lines[index].angleReferenceAxis, 0.0)
            || left.lines[index].linkedToPrevious != right.lines[index].linkedToPrevious
            || left.lines[index].linkedToNext != right.lines[index].linkedToNext )
        {
            return false;
        }
        const auto& a = left.lines[index].authoredCamera;
        const auto& b = right.lines[index].authoredCamera;
        if ( a.orbit.center != b.orbit.center || a.orbit.pan != b.orbit.pan
            || a.orbit.radius != b.orbit.radius || a.orbit.distance != b.orbit.distance
            || a.orbit.yawDeg != b.orbit.yawDeg || a.orbit.pitchDeg != b.orbit.pitchDeg
            || a.orbit.rollDeg != b.orbit.rollDeg || a.projectionType != b.projectionType
            || a.fovXDeg != b.fovXDeg || a.orthoScale != b.orthoScale
            || a.nearPlane != b.nearPlane || a.farPlane != b.farPlane
            || a.clipEnabled != b.clipEnabled || a.valid != b.valid )
        {
            return false;
        }
    }
    return left.selectedCleaveLines == right.selectedCleaveLines
        && left.selectedCleaveLine == right.selectedCleaveLine
        && left.applyOrder == right.applyOrder
        && left.context.valid == right.context.valid
        && left.context.surfaceMode == right.context.surfaceMode
        && left.context.projection == right.context.projection
        && left.context.view == right.context.view
        && left.context.viewportWidth == right.context.viewportWidth
        && left.context.viewportHeight == right.context.viewportHeight
        && left.context.sourceSurface == right.context.sourceSurface
        && left.context.targetSectors == right.context.targetSectors
        && left.context.targetSurfaces == right.context.targetSurfaces
        && left.chainOrigin.has_value() == right.chainOrigin.has_value()
        && (!left.chainOrigin || (left.chainOrigin->lineIndex == right.chainOrigin->lineIndex
            && left.chainOrigin->endpoint == right.chainOrigin->endpoint));
}

bool sameHistory(const std::optional<GlView::RecoveryState::CleaveHistory>& left,
    const std::optional<GlView::RecoveryState::CleaveHistory>& right)
{
    if ( left.has_value() != right.has_value() )
    {
        return false;
    }
    if ( !left )
    {
        return true;
    }
    if ( left->cursor != right->cursor || left->snapshots.size() != right->snapshots.size() )
    {
        return false;
    }
    for ( std::size_t index = 0; index < left->snapshots.size(); ++index )
    {
        if ( !sameLines(left->snapshots[index], right->snapshots[index]) )
        {
            return false;
        }
    }
    return true;
}

void uvRoutingDiagnostic(MainWindow& window, GlView& view, QWidget& canvas,
    const char* phase, const int code = 0)
{
    const QPoint cursor = QCursor::pos();
    const QRect canvasGlobal(canvas.mapToGlobal(QPoint{}), canvas.size());
    const QRect viewportGlobal(view.mapToGlobal(QPoint{}), view.size());
    const QWidget* const focused = QApplication::focusWidget();
    std::cerr << "UV routing diagnostic: phase=" << phase << " key=" << code
              << " cursor=(" << cursor.x() << ',' << cursor.y() << ")"
              << " canvas=(" << canvasGlobal.x() << ',' << canvasGlobal.y()
              << ',' << canvasGlobal.width() << ',' << canvasGlobal.height() << ")"
              << " viewport=(" << viewportGlobal.x() << ',' << viewportGlobal.y()
              << ',' << viewportGlobal.width() << ',' << viewportGlobal.height() << ")"
              << " insideCanvas=" << canvasGlobal.contains(cursor)
              << " insideViewport=" << viewportGlobal.contains(cursor)
              << " visible=" << canvas.isVisible()
              << " focused=" << (focused ? focused->objectName().toStdString() : "null")
              << " focusClass=" << (focused ? focused->metaObject()->className() : "null")
              << " windowActive=" << (QApplication::activeWindow() == &window)
              << " selectedUV=" << canvas.property("sed.uvSelectedFaceCount").toInt()
              << " localMode=" << int(view.stagedCleaveTransformMode())
              << " globalTool=" << int(view.transformTool())
              << " grid=" << view.showGrid()
              << " fly=" << view.flyNavigationActive()
              << " modifiers=" << int(QGuiApplication::keyboardModifiers())
              << " storedMove=" << settingsStore().value(
                     QStringLiteral("keymap/shortcuts/editor.tool.move")).toStringList().join('|').toStdString()
              << " storedGrid=" << settingsStore().value(
                     QStringLiteral("keymap/shortcuts/viewport.show_grid")).toStringList().join('|').toStdString()
              << '\n';
}

class FlyHoverOwnershipTrace final : public QObject
{
public:
    FlyHoverOwnershipTrace(MainWindow& window, GlView& view, QWidget& canvas)
        : window_(window), view_(view), canvas_(canvas)
    {
        QApplication::instance()->installEventFilter(this);
        connect(&view_, &GlView::flyNavigationActiveChanged, this, [this]()
        {
            record("flyNavigationActiveChanged", nullptr);
        });
    }

    ~FlyHoverOwnershipTrace() override
    {
        QApplication::instance()->removeEventFilter(this);
    }

    void checkpoint(const char* phase)
    {
        phase_ = phase;
        record("checkpoint", nullptr);
    }

    void print() const
    {
        for (std::size_t index = 0; index < count_; ++index)
        {
            const auto& sample = samples_[index];
            std::cerr << "FLY-HOVER phase=" << sample.phase << " event=" << sample.event
                << " receiver=" << sample.receiver << " focus=" << sample.focus
                << " fly=" << sample.fly << " capturedButton=" << sample.capturedButton
                << " eventButtons=" << sample.eventButtons
                << " physicalButtons=" << sample.physicalButtons
                << " focusReason=" << sample.focusReason
                << " spontaneous=" << sample.spontaneous << '\n';
        }
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched != &view_ && watched != &canvas_ && watched != &window_
            && watched != QApplication::instance())
        {
            return false;
        }
        const char* type{};
        switch (event->type())
        {
            case QEvent::Enter: type = "Enter"; break;
            case QEvent::Leave: type = "Leave"; break;
            case QEvent::FocusIn: type = "FocusIn"; break;
            case QEvent::FocusOut: type = "FocusOut"; break;
            case QEvent::MouseMove: type = "MouseMove"; break;
            case QEvent::UngrabMouse: type = "UngrabMouse"; break;
            case QEvent::WindowDeactivate: type = "WindowDeactivate"; break;
            case QEvent::ApplicationDeactivate: type = "ApplicationDeactivate"; break;
            default: break;
        }
        if (type)
        {
            record(type, watched, event);
        }
        return false;
    }

private:
    struct Sample
    {
        const char* phase{};
        const char* event{};
        const char* receiver{};
        const char* focus{};
        bool fly{};
        int capturedButton{};
        int eventButtons{};
        int physicalButtons{};
        int focusReason{-1};
        bool spontaneous{};
    };

    const char* owner(QObject* object) const
    {
        return object == &view_ ? "viewport"
            : object == &canvas_ ? "UV canvas"
            : object == &window_ ? "window"
            : object ? object->metaObject()->className() : "null";
    }

    void record(const char* type, QObject* watched, QEvent* event = nullptr)
    {
        if (count_ == samples_.size())
        {
            return;
        }
        Sample& sample = samples_[count_++];
        sample.phase = phase_;
        sample.event = type;
        sample.receiver = owner(watched);
        sample.focus = owner(QApplication::focusWidget());
        sample.fly = view_.flyNavigationActive();
        sample.capturedButton = static_cast<int>(view_.viewportMouseGestureButton());
        sample.physicalButtons = static_cast<int>(QApplication::mouseButtons());
        if (event)
        {
            sample.spontaneous = event->spontaneous();
            if (event->type() == QEvent::MouseMove)
            {
                sample.eventButtons = static_cast<int>(static_cast<QMouseEvent*>(event)->buttons());
            }
            else if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)
            {
                sample.focusReason = static_cast<int>(static_cast<QFocusEvent*>(event)->reason());
            }
        }
    }

    MainWindow& window_;
    GlView& view_;
    QWidget& canvas_;
    const char* phase_{"initial"};
    std::array<Sample, 40> samples_{};
    std::size_t count_{};
};

void testNativeShortcutOwnership()
{
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(1280, 800);
    window.activateSectorEditModeForTest();
    window.setDocumentStateForTest(document());
    window.activateSectorEditModeForTest();
    window.setLayerVisualStateForTest({true}, {false});
    window.show();
    GlView* const view = window.glViewForTest();
    check(view != nullptr, "Cleave input fixture has a live viewport");
    if ( !view )
    {
        return;
    }
    configure(*view, window.documentStateForTest().level,
        window.documentStateForTest().selection);
    addGroup(*view);
    focus(*view);
    const auto globalTool = view->transformTool();
    const bool grid = view->showGrid();
    const auto before = view->captureStagedCleaveDraftState();
    key(*view, Qt::Key_R);
    check(view->stagedCleaveTransformMode() == GlView::StagedCleaveTransformMode::Rotate
            && view->sectorCleaveToolEnabled() && view->transformTool() == globalTool
            && view->showGrid() == grid && sameLines(before, view->captureStagedCleaveDraftState()),
        "active Cleave consumes native R exclusively as local Rotate without tool, grid, or draft mutation");
    key(*view, Qt::Key_S);
    check(view->stagedCleaveTransformMode() == GlView::StagedCleaveTransformMode::Scale
            && view->sectorCleaveToolEnabled() && view->transformTool() == globalTool,
        "active Cleave consumes native S as local Scale without switching to global Select");
    key(*view, Qt::Key_G);
    check(view->stagedCleaveTransformMode() == GlView::StagedCleaveTransformMode::Move
            && view->showGrid() == grid && view->sectorCleaveToolEnabled(),
        "active Cleave consumes native G as local Move without toggling grid");

    view->setStagedCleaveTransformMode(GlView::StagedCleaveTransformMode::Scale);
    QLineEdit text(&window);
    text.show();
    focus(text);
    key(text, Qt::Key_G, Qt::NoModifier, QStringLiteral("g"));
    key(text, Qt::Key_R, Qt::NoModifier, QStringLiteral("r"));
    key(text, Qt::Key_S, Qt::NoModifier, QStringLiteral("s"));
    check(text.text() == QStringLiteral("grs")
            && view->stagedCleaveTransformMode() == GlView::StagedCleaveTransformMode::Scale
            && view->sectorCleaveToolEnabled() && view->showGrid() == grid,
        "focused text keeps native G/R/S typing while active Cleave state remains unchanged");
    text.hide();
    focus(*view);
    view->setSectorCleaveToolEnabled(false);
    key(*view, Qt::Key_G);
    check(view->showGrid() != grid && view->transformTool() == globalTool,
        "viewport G toggles grid after Cleave loses input ownership, including a retained hidden draft");
    QAction* const behind = window.findChild<QAction*>(QStringLiteral("viewport.grid_always_behind"));
    QAction* const align = window.findChild<QAction*>(QStringLiteral("viewport.grid_align_to_view"));
    check(behind && align && behind->shortcuts() == QList<QKeySequence>{QKeySequence(Qt::Key_Insert)}
            && align->shortcuts().contains(QKeySequence(Qt::KeypadModifier | Qt::Key_0)),
        "Insert alone retains grid-behind and Numpad0 joins grid-to-view defaults");
}

void testWheelTransforms(const bool perspective)
{
    auto state = document();
    GlView view{};
    view.resize(640, 480);
    configure(view, state.level, state.selection, perspective);
    addGroup(view);
    const float distance = view.cameraOrbitDistance();
    const float orthoScale = view.cameraOrthoScale();
    const auto before = view.captureStagedCleaveDraftState();
    const TVector pivot{1.0 / 3.0, 0.0, -1.0 / 3.0};
    view.setStagedCleaveTransformMode(GlView::StagedCleaveTransformMode::Scale);
    check(wheel(view, 120), "Cleave Scale accepts Ctrl-wheel instead of propagating it");
    check(view.stagedCleaveTransformHudText() == QStringLiteral("Scale 1.10×"),
        "the live Scale wheel HUD reports its actual factor before Control release");
    releaseControl(view);
    const auto scaled = view.captureStagedCleaveDraftState();
    bool geometryMatches = scaled.lines.size() == 3;
    if ( geometryMatches )
    {
        geometryMatches = closeVec(scaled.lines[0].first,
                pivot + (before.lines[0].first - pivot) * 1.1, 1.0e-9)
            && closeVec(scaled.lines[0].second,
                pivot + (before.lines[0].second - pivot) * 1.1, 1.0e-9)
            && closeVec(scaled.lines[1].second,
                pivot + (before.lines[1].second - pivot) * 1.1, 1.0e-9)
            && closeVec(scaled.lines[2].first, scaled.lines[1].second, 0.0)
            && closeVec(scaled.lines[2].second, before.lines[2].second, 0.0);
    }
    check(geometryMatches && view.cameraOrbitDistance() == distance
            && view.cameraOrthoScale() == orthoScale
            && view.stagedCleaveTransformHudText().isEmpty(),
        "Ctrl-wheel scales the selected group 1.1 about distinct endpoints, carries only shared neighbor, keeps camera fixed, and hides its HUD on Control release");
    check(view.undoStagedCleaveHistory() && sameLines(before, view.captureStagedCleaveDraftState()),
        "one local Undo exactly restores the complete Scale wheel event and line selection");
    check(view.redoStagedCleaveHistory() && sameLines(scaled, view.captureStagedCleaveDraftState()),
        "local Redo exactly restores wheel-scaled draft");
    (void)view.undoStagedCleaveHistory();
    view.setStagedCleaveTransformMode(GlView::StagedCleaveTransformMode::Rotate);
    view.setCleaveAngleStepDegrees(30.0);
    check(wheel(view, 120), "Cleave Rotate accepts Ctrl-wheel");
    check(view.stagedCleaveTransformHudText() == QStringLiteral("Rotate 30.00°"),
        "the live Rotate wheel HUD reports its actual configured angle before Control release");
    releaseControl(view);
    const auto rotated = view.captureStagedCleaveDraftState();
    if ( rotated.lines.size() == 3 )
    {
        const TVector radial = before.lines[0].first - pivot;
        const QVector3D normal = vector(before.lines[0].constructionPlaneNormal).normalized();
        const QVector3D source = vector(radial);
        const QVector3D expected = source * static_cast<float>(std::sqrt(3.0) / 2.0)
            + QVector3D::crossProduct(normal, source) * 0.5f
            + normal * QVector3D::dotProduct(normal, source)
                * static_cast<float>(1.0 - std::sqrt(3.0) / 2.0);
        check(closeQVec(vector(rotated.lines[0].first - pivot), expected, 1.0e-6f)
                && closeVec(rotated.lines[2].first, rotated.lines[1].second, 0.0)
                && closeVec(rotated.lines[2].second, before.lines[2].second, 0.0),
            "Rotate wheel applies the configured signed 30 degree step and preserves pinned neighboring endpoints");
    }
    check(view.cameraOrbitDistance() == distance && view.cameraOrthoScale() == orthoScale
            && view.stagedCleaveTransformHudText().isEmpty(),
        "Rotate wheel hides its HUD on Control release and leaves both projection zoom values unchanged");
    check(view.undoStagedCleaveHistory() && sameLines(before, view.captureStagedCleaveDraftState()),
        "one local Undo restores the Rotate wheel transaction exactly");
    const auto history = view.captureStagedCleaveHistory();
    const auto afterZeroHistory = [&view]() { return view.captureStagedCleaveHistory(); };
    check(wheel(view, 0) && sameLines(before, view.captureStagedCleaveDraftState())
            && history && afterZeroHistory()
            && afterZeroHistory()->cursor == history->cursor,
        "zero Ctrl-wheel is consumed without a draft or camera change and without trimming Redo");
    view.setStagedCleaveTransformMode(GlView::StagedCleaveTransformMode::Scale);
    wheel(view, -120);
    releaseControl(view);
    const auto smaller = view.captureStagedCleaveDraftState();
    check(closeVec(smaller.lines[0].first,
            pivot + (before.lines[0].first - pivot) * (1.0 / 1.1), 1.0e-9),
        "negative Ctrl-wheel uses inverse positive group scale");
    (void)view.undoStagedCleaveHistory();
    wheel(view, 60);
    releaseControl(view);
    const auto fractional = view.captureStagedCleaveDraftState();
    check(closeVec(fractional.lines[0].first,
            pivot + (before.lines[0].first - pivot) * std::sqrt(1.1), 1.0e-9),
        "fractional Ctrl-wheel preserves fractional group scale");
    (void)view.undoStagedCleaveHistory();
    wheel(view, 0, Qt::ControlModifier, QPoint(0, 80));
    releaseControl(view);
    check(closeVec(view.captureStagedCleaveDraftState().lines[0].first,
            pivot + (before.lines[0].first - pivot) * 1.1, 1.0e-9),
        "pixel-only touchpad Ctrl-wheel shares the established 80-pixel detent scale");
    (void)view.undoStagedCleaveHistory();
    const auto body = view.projectWorldToViewport({0, 0, -1});
    if ( body )
    {
        mouse(view, QEvent::MouseButtonPress, *body, Qt::LeftButton, Qt::LeftButton);
        check(view.cursor().shape() == Qt::SizeAllCursor,
            "wheel gesture boundary fixture begins a real selected-group Scale drag");
        check(wheel(view, 120) && sameLines(before, view.captureStagedCleaveDraftState()),
            "Ctrl-wheel during a live mouse transform is consumed without replacing its draft rollback baseline");
        key(view, Qt::Key_Escape);
        check(sameLines(before, view.captureStagedCleaveDraftState()),
            "Escape after an ignored Ctrl-wheel restores the exact pointer-drag baseline and selection");
        mouse(view, QEvent::MouseButtonRelease, *body, Qt::LeftButton);
    }
    wheel(view, 120, Qt::NoModifier);
    check(perspective ? view.cameraOrbitDistance() != distance
            : view.cameraOrthoScale() != orthoScale,
        "plain wheel retains ordinary camera navigation while Cleave Rotate is selected");
}

void testShapeWheelAndCameraPlane(const bool perspective)
{
    auto state = document();
    GlView view{};
    view.resize(640, 480);
    configure(view, state.level, state.selection, perspective);
    auto options = view.cleaveShapeOptions();
    options.kind = ui::viewport::CleaveShapeKind::Rectangle;
    options.dimensionsLinked = false;
    options.width = 1.6;
    options.height = 1.0;
    options.placementArmed = true;
    options.cameraPlane = true;
    view.setCleaveShapeOptions(options);
    const QPoint center = view.rect().center();
    mouse(view, QEvent::MouseMove, center);
    const float distance = view.cameraOrbitDistance();
    const float scale = view.cameraOrthoScale();
    check(wheel(view, 120) && close(view.cleaveShapeOptions().width, 1.76, 1.0e-12)
            && close(view.cleaveShapeOptions().height, 1.1, 1.0e-12)
            && view.cameraOrbitDistance() == distance && view.cameraOrthoScale() == scale
            && view.stagedCleaveLineCount() == 0,
        "Ctrl-wheel scales only armed shape dimensions and preserves rectangular aspect, camera, and draft");
    mouse(view, QEvent::MouseMove, QPoint(12, 12));
    check(view.stagedCleaveShapePreviewCenterForTest().has_value(),
        "Sector Camera plane shape has a free preview even away from every wall surface");
    click(view, center);
    const auto placed = view.captureStagedCleaveDraftState();
    check(placed.lines.size() == 4, "Camera plane click places the rectangle as four ordinary staged strokes");
    if ( placed.lines.size() != 4 )
    {
        return;
    }
    const QVector3D firstDirection = vector(placed.lines[0].second - placed.lines[0].first).normalized();
    const QVector3D secondDirection = vector(placed.lines[1].second - placed.lines[1].first).normalized();
    const QVector3D normal = vector(placed.lines[0].constructionPlaneNormal).normalized();
    check(std::abs(QVector3D::dotProduct(firstDirection, view.cameraRight())) > 0.99999f
            && std::abs(QVector3D::dotProduct(secondDirection, view.cameraUp())) > 0.99999f
            && std::abs(QVector3D::dotProduct(normal, view.cameraForward())) > 0.99999f,
        "Sector Camera plane uses rotated and banked camera right/up/forward instead of a wall or grid frame");
    view.setOrbitViewImmediate(-46.0f, 37.0f, 0.0f);
    check(sameLines(placed, view.captureStagedCleaveDraftState()),
        "completed camera-plane strokes keep their authored frame after camera orbit");
    options = view.cleaveShapeOptions();
    options.width = 9000.0;
    options.height = 3000.0;
    view.setCleaveShapeOptions(options);
    wheel(view, 12000);
    check(view.cleaveShapeOptions().width == 10000.0
            && close(view.cleaveShapeOptions().height, 10000.0 / 3.0, 1.0e-9),
        "shape wheel maximum clamps one uniform factor and preserves unlocked rectangular aspect");
    options = view.cleaveShapeOptions();
    options.width = 0.006;
    options.height = 0.018;
    view.setCleaveShapeOptions(options);
    wheel(view, -12000);
    check(close(view.cleaveShapeOptions().width, 0.005, 1.0e-15)
            && close(view.cleaveShapeOptions().height, 0.015, 1.0e-15),
        "shape wheel minimum clamps one uniform factor and preserves positive unlocked aspect");
}

void testArmedShapeRotationWheel(const bool perspective)
{
    struct RotationCase
    {
        double step{};
        int angleDelta{};
        QPoint pixelDelta{};
        double expectedDegrees{};
    };
    const std::array<RotationCase, 6> cases{{
        {30.0, 120, {}, 30.0},
        {30.0, 60, {}, 30.0},
        {30.0, -120, {}, -30.0},
        {0.0, 120, {}, 5.0},
        {0.0, 60, {}, 2.5},
        {15.0, 0, QPoint(0, 80), 15.0},
    }};
    for ( const auto& test : cases )
    {
        auto state = document();
        GlView view{};
        view.resize(640, 480);
        configure(view, state.level, state.selection, perspective);
        auto options = view.cleaveShapeOptions();
        options.kind = ui::viewport::CleaveShapeKind::Rectangle;
        options.dimensionsLinked = false;
        options.width = 1.6;
        options.height = 1.0;
        options.placementArmed = true;
        options.cameraPlane = true;
        view.setCleaveShapeOptions(options);
        view.setCleaveAngleStepDegrees(test.step);
        const QPoint center = view.rect().center();
        mouse(view, QEvent::MouseMove, center);
        const auto draft = view.captureStagedCleaveDraftState();
        const auto history = view.captureStagedCleaveHistory();
        const float distance = view.cameraOrbitDistance();
        const float scale = view.cameraOrthoScale();
        const QVector3D axisA = view.cameraRight();
        const QVector3D axisB = view.cameraUp();
        const bool consumed = wheel(view, test.angleDelta,
            Qt::ControlModifier | Qt::ShiftModifier, test.pixelDelta);
        check(consumed && view.cameraOrbitDistance() == distance
                && view.cameraOrthoScale() == scale
                && view.cleaveShapeOptions().width == options.width
                && view.cleaveShapeOptions().height == options.height
                && sameLines(draft, view.captureStagedCleaveDraftState())
                && sameHistory(history, view.captureStagedCleaveHistory()),
            "armed Ctrl+Shift wheel rotates settings without zoom, dimension scaling, draft edits, or history entries");
        click(view, center);
        const auto placed = view.captureStagedCleaveDraftState();
        check(placed.lines.size() == 4,
            "rotated armed Rectangle still places exactly four ordinary strokes");
        if ( placed.lines.size() != 4 )
        {
            continue;
        }
        const double radians = test.expectedDegrees * 3.14159265358979323846 / 180.0;
        const QVector3D expectedA = axisA * static_cast<float>(std::cos(radians))
            + axisB * static_cast<float>(std::sin(radians));
        const QVector3D expectedB = axisB * static_cast<float>(std::cos(radians))
            - axisA * static_cast<float>(std::sin(radians));
        const QVector3D first = vector(placed.lines[0].second - placed.lines[0].first);
        const QVector3D second = vector(placed.lines[1].second - placed.lines[1].first);
        check(std::abs(QVector3D::dotProduct(first.normalized(), expectedA)) > 0.99999f
                && std::abs(QVector3D::dotProduct(second.normalized(), expectedB)) > 0.99999f
                && close(first.length(), options.width, 1.0e-5)
                && close(second.length(), options.height, 1.0e-5),
            "Ctrl+Shift wheel applies configured signed whole steps or fractional Free rotation to the actual placement frame");
        check(state.level.sectors.size() == 1 && state.level.sectors.front().surfaces.size() == 6,
            "armed shape rotation and placement leave authoritative Level topology unchanged before Apply");
    }
}

void testModeChangeDuringPointerGesture(const bool perspective)
{
    auto state = document();
    GlView view{};
    view.resize(640, 480);
    configure(view, state.level, state.selection, perspective);
    addGroup(view);
    view.setStagedCleaveTransformMode(GlView::StagedCleaveTransformMode::Scale);
    wheel(view, 120);
    releaseControl(view);
    (void)view.undoStagedCleaveHistory();
    const auto baseline = view.captureStagedCleaveDraftState();
    const auto history = view.captureStagedCleaveHistory();
    const auto body = view.projectWorldToViewport({0, 0, -1});
    check(body.has_value(), "mode-switch fixture projects an actual selected line body");
    if ( !body )
    {
        return;
    }
    mouse(view, QEvent::MouseButtonPress, *body, Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseMove, *body + QPoint(50, 12), Qt::NoButton, Qt::LeftButton);
    const QVector3D draggedFirst = view.stagedCleaveLineFirstForTest(0);
    const QString hud = view.stagedCleaveTransformHudText();
    check(draggedFirst != vector(baseline.lines[0].first) && !hud.isEmpty(),
        "mode-switch fixture starts a real Scale pointer preview with actual HUD");
    key(view, Qt::Key_S);
    check(view.stagedCleaveLineFirstForTest(0) == draggedFirst
            && view.stagedCleaveTransformHudText() == hud,
        "same-mode native S preserves the current gesture and its actual Scale HUD");
    key(view, Qt::Key_R);
    check(view.stagedCleaveTransformMode() == GlView::StagedCleaveTransformMode::Rotate
            && view.stagedCleaveLineFirstForTest(0) == vector(baseline.lines[0].first)
            && sameLines(baseline, view.captureStagedCleaveDraftState())
            && sameHistory(history, view.captureStagedCleaveHistory())
            && view.stagedCleaveTransformHudText().isEmpty(),
        "a real native mode change cancels the live pointer gesture and restores exact draft/history including Redo");
    mouse(view, QEvent::MouseMove, *body + QPoint(75, 25), Qt::NoButton, Qt::LeftButton);
    mouse(view, QEvent::MouseButtonRelease, *body + QPoint(75, 25), Qt::LeftButton);
    check(sameLines(baseline, view.captureStagedCleaveDraftState())
            && sameHistory(history, view.captureStagedCleaveHistory()),
        "pointer movement and release after a mode change cannot continue the cancelled Scale drag");
}

void testCameraPlanePreviewAfterStationaryOrbit(const bool perspective)
{
    auto state = document();
    GlView view{};
    view.resize(640, 480);
    configure(view, state.level, state.selection, perspective);
    auto options = view.cleaveShapeOptions();
    options.kind = ui::viewport::CleaveShapeKind::Rectangle;
    options.width = 1.0;
    options.height = 1.0;
    options.placementArmed = true;
    options.cameraPlane = true;
    view.setCleaveShapeOptions(options);
    click(view, view.rect().center());
    const auto placed = view.captureStagedCleaveDraftState();
    const QPoint pointer = view.rect().center() + QPoint(87, -61);
    mouse(view, QEvent::MouseMove, pointer);
    const auto before = view.stagedCleaveShapePreviewCenterForTest();
    check(placed.lines.size() == 4 && before.has_value(),
        "stationary camera-plane fixture has authored strokes and a real off-center pending preview");
    if ( placed.lines.size() != 4 || !before )
    {
        return;
    }
    view.setOrbitViewImmediate(-46.0f, 37.0f, -17.0f);
    const QVector3D target = view.cameraTarget();
    const QVector3D normal = view.cameraForward();
    const auto expectedDocument = view.viewportPointOnPlane(pointer,
        TVector{target.x(), target.y(), target.z()},
        TVector{normal.x(), normal.y(), normal.z()});
    const std::optional<QVector3D> expected = expectedDocument
        ? std::optional<QVector3D>{vector(*expectedDocument)} : std::nullopt;
    const auto after = view.stagedCleaveShapePreviewCenterForTest();
    const auto projectedAfter = after ? view.projectWorldToViewport(*after) : std::nullopt;
    const bool currentPreview = expected && after
        && !closeQVec(*before, *expected, 1.0e-3f)
        && closeQVec(*after, *expected, 1.0e-4f)
        && projectedAfter && std::abs(projectedAfter->x() - pointer.x()) < 1.0e-3
        && std::abs(projectedAfter->y() - pointer.y()) < 1.0e-3;
    if ( !currentPreview )
    {
        const auto point = [](const char* name, const QVector3D& value)
        {
            std::cerr << name << "=(" << value.x() << ',' << value.y() << ',' << value.z() << ") ";
        };
        std::cerr << "Stationary camera-plane diagnostic: projection="
                  << (perspective ? "perspective" : "orthographic")
                  << " pointer=(" << pointer.x() << ',' << pointer.y() << ")"
                  << " modifiers=" << int(QGuiApplication::keyboardModifiers())
                  << " active=" << view.isStagedCleaveInteractionActive()
                  << " sector=" << view.sectorCleaveToolEnabled()
                  << " surface=" << view.surfaceCleaveToolEnabled()
                  << " armed=" << view.cleaveShapeOptions().placementArmed
                  << " cameraPlane=" << view.cleaveShapeOptions().cameraPlane
                  << " cachedPlan=" << view.stagedCleaveShapePlacementActiveForTest()
                  << " previewCenter=" << after.has_value()
                  << " expected=" << expected.has_value() << ' ';
        point("before", *before);
        if ( expected ) point("expected", *expected);
        if ( after ) point("actual", *after);
        point("target", view.cameraTarget());
        point("eye", view.cameraEye());
        point("right", view.cameraRight());
        point("up", view.cameraUp());
        point("forward", view.cameraForward());
        const auto documentExpected = view.viewportPointOnPlane(pointer,
            TVector{target.x(), target.y(), target.z()},
            TVector{normal.x(), normal.y(), normal.z()});
        if ( documentExpected ) point("documentRayExpected", vector(*documentExpected));
        if ( after )
        {
            const auto projected = view.projectWorldToViewport(*after);
            if ( projected )
            {
                std::cerr << "actualScreen=(" << projected->x() << ',' << projected->y() << ") ";
            }
        }
        std::cerr << '\n';
    }
    check(currentPreview,
        "camera orbit immediately refreshes an armed camera-plane preview at a stationary pointer to the current camera-facing plane");
    check(sameLines(placed, view.captureStagedCleaveDraftState()),
        "stationary camera-plane preview refresh leaves authored strokes and frames exactly fixed");
}

void testUvKeyReleaseRouting()
{
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(1280, 800);
    auto state = document();
    state.selection.clear();
    state.selection.surfaces.insert({0, 0});
    auto& surface = state.level.sectors.front().surfaces.front();
    surface.texVertices = {
        TexVertex{0.0f, 0.0f, ColorWhite}, TexVertex{1.0f, 0.0f, ColorWhite},
        TexVertex{1.0f, 1.0f, ColorWhite}, TexVertex{0.0f, 1.0f, ColorWhite}};
    window.activateSurfaceEditModeForTest();
    window.setDocumentStateForTest(std::move(state));
    window.activateSurfaceEditModeForTest();
    window.setLayerVisualStateForTest({true}, {false});
    window.show();
    QApplication::processEvents();
    GlView* const view = window.glViewForTest();
    QWidget* const canvas = window.findChild<QWidget*>(QStringLiteral("uvEditorCanvas"));
    check(view && canvas && canvas->isVisible(),
        "UV key routing fixture exposes a selected face and visible real canvas");
    if ( !view || !canvas || !canvas->isVisible() )
    {
        return;
    }
    const auto before = window.uvEditorStoredPointsForTest({0, 0});
    check(before.size() == 4, "UV key routing retains its complete four-corner baseline");
    const QPoint center = canvas->rect().center();
    focus(*view);
    QCursor::setPos(canvas->mapToGlobal(center));
    view->setFocus(Qt::OtherFocusReason);
    QKeyEvent release(QEvent::KeyRelease, Qt::Key_G, Qt::NoModifier, QStringLiteral("g"));
    QApplication::sendEvent(view, &release);
    mouse(*canvas, QEvent::MouseMove, center + QPoint(36, 12));
    check(window.uvEditorStoredPointsForTest({0, 0}) == before,
        "a hover-forwarded G KeyRelease cannot start a UV transform or mutate its preview");
    key(*canvas, Qt::Key_Escape);
    const bool grid = view->showGrid();
    focus(*canvas);
    mouse(*canvas, QEvent::MouseMove, center);
    key(*canvas, Qt::Key_G);
    mouse(*canvas, QEvent::MouseMove, center + QPoint(36, 12));
    const bool uvMoveStarted = window.uvEditorStoredPointsForTest({0, 0}) != before
        && view->showGrid() == grid;
    if ( !uvMoveStarted )
    {
        uvRoutingDiagnostic(window, *view, *canvas, "focused G Move", Qt::Key_G);
    }
    check(uvMoveStarted,
        "UV-focused native G still begins local Move and leaves viewport grid visibility unchanged");
    key(*canvas, Qt::Key_Escape);
    check(window.uvEditorStoredPointsForTest({0, 0}) == before,
        "UV G Move Escape restores exact working UVs without a Level edit");
}

void testUvPointerShortcutOwnership()
{
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(1280, 800);
    auto state = document();
    state.selection.clear();
    state.selection.surfaces.insert({0, 0});
    auto& surface = state.level.sectors.front().surfaces.front();
    surface.texVertices = {
        TexVertex{0.0f, 0.0f, ColorWhite}, TexVertex{1.0f, 0.0f, ColorWhite},
        TexVertex{1.0f, 1.0f, ColorWhite}, TexVertex{0.0f, 1.0f, ColorWhite}};
    window.activateSurfaceEditModeForTest();
    window.setDocumentStateForTest(std::move(state));
    window.activateSurfaceEditModeForTest();
    window.setLayerVisualStateForTest({true}, {false});
    window.show();
    QApplication::processEvents();
    GlView* const view = window.glViewForTest();
    QWidget* const canvas = window.findChild<QWidget*>(QStringLiteral("uvEditorCanvas"));
    check(view && canvas && canvas->isVisible(),
        "UV pointer ownership fixture has its actual visible canvas");
    if ( !view || !canvas || !canvas->isVisible() )
    {
        return;
    }
    view->setSurfaceCleaveToolEnabled(true);
    view->setCleaveStagedMode(true);
    view->captureStagedCleaveContextForTest(true);
    addGroup(*view);
    check(view->isStagedCleaveInteractionActive(), "UV pointer fixture owns an actual active Surface Multi-cut draft");
    focus(*canvas);
    click(*canvas, QPoint(6, 6));
    check(canvas->property("sed.uvSelectedFaceCount").toInt() == 0,
        "a real empty-canvas click makes UV G/R/S ineligible by clearing island selection");
    const bool grid = view->showGrid();
    const auto tool = view->transformTool();
    const auto baseline = view->captureStagedCleaveDraftState();
    const auto uvBefore = window.uvEditorStoredPointsForTest({0, 0});
    const auto requireCanvasPointer = [&](const char* const phase, const int code,
                                          const QPoint requested)
    {
        const QPoint actual = QCursor::pos();
        const bool ready = canvas->isVisible()
            && canvas->rect().contains(canvas->mapFromGlobal(actual))
            && !view->rect().contains(view->mapFromGlobal(actual));
        check(ready, "UV pointer ownership requires actual canvas hover before key dispatch");
        if ( !ready )
        {
            uvRoutingDiagnostic(window, *view, *canvas, phase, code);
            std::cerr << "UV requested pointer=(" << requested.x() << ',' << requested.y() << ")\n";
#ifdef Q_OS_WIN
            POINT nativeCursor{};
            RECT clip{};
            const bool cursorAvailable = GetCursorPos(&nativeCursor) != FALSE;
            const bool clipAvailable = GetClipCursor(&clip) != FALSE;
            const HWND foreground = GetForegroundWindow();
            GUITHREADINFO input{};
            input.cbSize = static_cast<DWORD>(sizeof(input));
            const DWORD thread = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;
            const bool inputAvailable = thread && GetGUIThreadInfo(thread, &input) != FALSE;
            std::cerr << "UV native pointer: available=" << cursorAvailable
                      << " cursor=(" << nativeCursor.x << ',' << nativeCursor.y << ")"
                      << " clipAvailable=" << clipAvailable << " clip=(" << clip.left << ','
                      << clip.top << ',' << clip.right << ',' << clip.bottom << ")"
                      << " capture=" << static_cast<void*>(GetCapture())
                      << " foreground=" << static_cast<void*>(foreground)
                      << " foregroundInput=" << inputAvailable
                      << " foregroundCapture=" << static_cast<void*>(input.hwndCapture) << '\n';
#endif
        }
        return ready;
    };
    for ( const int code : {Qt::Key_G, Qt::Key_R, Qt::Key_S} )
    {
        view->setStagedCleaveTransformMode(code == Qt::Key_S
            ? GlView::StagedCleaveTransformMode::Rotate
            : GlView::StagedCleaveTransformMode::Scale);
        const auto mode = view->stagedCleaveTransformMode();
        focus(*view);
        view->setFocus(Qt::OtherFocusReason);
        const QPoint canvasCenter = canvas->mapToGlobal(canvas->rect().center());
        QCursor::setPos(canvasCenter);
        QKeyEvent overrideEvent(QEvent::ShortcutOverride, code, Qt::NoModifier);
        overrideEvent.setAccepted(false);
        if ( !requireCanvasPointer("before ShortcutOverride", code, canvasCenter) )
        {
            return;
        }
        QApplication::sendEvent(view, &overrideEvent);
        if ( !overrideEvent.isAccepted() )
        {
            uvRoutingDiagnostic(window, *view, *canvas, "ShortcutOverride", code);
        }
        check(overrideEvent.isAccepted(),
            "UV pointer claims bare G/R/S ShortcutOverride even when no island is selected");
        QKeyEvent press(QEvent::KeyPress, code, Qt::NoModifier);
        if ( !requireCanvasPointer("before KeyPress", code, canvasCenter) )
        {
            return;
        }
        QApplication::sendEvent(view, &press);
        QKeyEvent release(QEvent::KeyRelease, code, Qt::NoModifier);
        QApplication::sendEvent(view, &release);
        const bool uvRetainedOwnership = view->stagedCleaveTransformMode() == mode
            && view->showGrid() == grid && view->transformTool() == tool
            && view->surfaceCleaveToolEnabled()
            && sameLines(baseline, view->captureStagedCleaveDraftState())
            && window.uvEditorStoredPointsForTest({0, 0}) == uvBefore;
        if ( !uvRetainedOwnership )
        {
            uvRoutingDiagnostic(window, *view, *canvas, "ineligible KeyPress", code);
            std::cerr << "UV expected localMode=" << int(mode) << " grid=" << grid
                      << " globalTool=" << int(tool) << '\n';
        }
        check(uvRetainedOwnership,
            "ineligible UV G/R/S remains owned by its canvas and cannot fall through to Cleave, grid, or global tools");
    }
    focus(*canvas);
    QCursor::setPos(view->mapToGlobal(view->rect().center()));
    QApplication::processEvents();
    canvas->setFocus(Qt::OtherFocusReason);
    key(*canvas, Qt::Key_G);
    check(view->stagedCleaveTransformMode() == GlView::StagedCleaveTransformMode::Move
            && view->showGrid() == grid && view->surfaceCleaveToolEnabled(),
        "Cleave under the viewport pointer owns G despite stale UV keyboard focus");

    focus(*canvas);
    key(*canvas, Qt::Key_A);
    mouse(*canvas, QEvent::MouseMove, canvas->rect().center());
    check(canvas->property("sed.uvSelectedFaceCount").toInt() == 1,
        "Fly priority fixture restores a selected UV island for a meaningful modal-transform check");
    focus(*view);
    view->setStagedCleaveTransformMode(GlView::StagedCleaveTransformMode::Scale);
    mouse(*view, QEvent::MouseButtonPress, view->rect().center(),
        Qt::RightButton, Qt::RightButton);
    check(view->flyNavigationActive(), "a real RMB press starts Fly for the input-priority regression");
    key(*view, Qt::Key_G);
    check(view->flyNavigationActive()
            && view->stagedCleaveTransformMode() == GlView::StagedCleaveTransformMode::Scale
            && sameLines(baseline, view->captureStagedCleaveDraftState())
            && view->showGrid() == grid,
        "RMB Fly consumes native G without changing the active Cleave mode, draft, or grid");
    view->setStagedCleaveTransformMode(GlView::StagedCleaveTransformMode::Scale);
    const auto flyUvBefore = window.uvEditorStoredPointsForTest({0, 0});
    FlyHoverOwnershipTrace trace(window, *view, *canvas);
    trace.checkpoint("before canvas warp");
    QCursor::setPos(canvas->mapToGlobal(canvas->rect().center()));
    trace.checkpoint("after canvas warp");
    view->setFocus(Qt::OtherFocusReason);
    trace.checkpoint("after viewport focus");
    key(*view, Qt::Key_G);
    trace.checkpoint("after viewport G");
    mouse(*canvas, QEvent::MouseMove, canvas->rect().center() + QPoint(36, 12));
    trace.checkpoint("after canvas MouseMove");
    const bool flyRetainedOwnership = view->flyNavigationActive()
        && window.uvEditorStoredPointsForTest({0, 0}) == flyUvBefore;
    if (!flyRetainedOwnership)
    {
        trace.print();
        uvRoutingDiagnostic(window, *view, *canvas, "Fly hovered canvas", Qt::Key_G);
    }
    check(flyRetainedOwnership,
        "RMB Fly retains keyboard ownership over a hovered UV canvas and cannot start a UV Move");
    check(view->viewportMouseGestureButton() == Qt::RightButton
            && QApplication::focusWidget() == view,
        "UV hover preserves the captured Fly button and the originating viewport focus");
    mouse(*view, QEvent::MouseButtonRelease, view->rect().center(), Qt::RightButton);
    check(!view->flyNavigationActive() && view->viewportMouseGestureButton() == Qt::NoButton,
        "the matching RMB release ends Fly after its pointer hovered the UV canvas");
    mouse(*canvas, QEvent::MouseMove, canvas->rect().center());
    check(QApplication::focusWidget() == canvas,
        "ordinary UV hover resumes its canvas focus after Fly releases ownership");
    key(*canvas, Qt::Key_Escape);
}

void testPersistedMoveShortcutCollision()
{
    struct SavedSetting
    {
        QString key{};
        bool existed{};
        QVariant value{};
    };
    struct RestoreSettings
    {
        std::array<SavedSetting, 2> saved{};
        ~RestoreSettings()
        {
            QSettings settings = settingsStore();
            for ( const auto& entry : saved )
            {
                if ( entry.existed )
                {
                    settings.setValue(entry.key, entry.value);
                }
                else
                {
                    settings.remove(entry.key);
                }
            }
            settings.sync();
        }
    } restore{};
    {
        QSettings settings = settingsStore();
        const std::array<QString, 2> keys{
            QStringLiteral("keymap/shortcuts/editor.tool.move"),
            QStringLiteral("keymap/shortcuts/viewport.show_grid")};
        for ( std::size_t index = 0; index < keys.size(); ++index )
        {
            restore.saved[index] = {keys[index], settings.contains(keys[index]), settings.value(keys[index])};
        }
    }
    for ( const int gridConfiguration : {0, 1, 2} )
    {
        {
            QSettings settings = settingsStore();
            settings.setValue(QStringLiteral("keymap/shortcuts/editor.tool.move"),
                QStringList{QStringLiteral("G")});
            if ( gridConfiguration == 0 )
            {
                settings.remove(QStringLiteral("keymap/shortcuts/viewport.show_grid"));
            }
            else
            {
                settings.setValue(QStringLiteral("keymap/shortcuts/viewport.show_grid"),
                    gridConfiguration == 1 ? QStringList{QStringLiteral("H")} : QStringList{});
            }
            settings.sync();
        }
        MainWindow window(nullptr, true);
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.resize(1280, 800);
        window.activateSectorEditModeForTest();
        window.setDocumentStateForTest(document());
        window.activateSectorEditModeForTest();
        window.setLayerVisualStateForTest({true}, {false});
        window.show();
        GlView* const view = window.glViewForTest();
        QAction* const moveBinding = window.findChild<QAction*>(QStringLiteral("editor.tool.move"));
        QAction* const moveTool = window.findChild<QAction*>(QStringLiteral("viewport.transform.move"));
        QAction* const gridBinding = window.findChild<QAction*>(QStringLiteral("viewport.show_grid"));
        check(view && moveBinding && moveTool && gridBinding
                && moveBinding->shortcuts() == QList<QKeySequence>{QKeySequence(Qt::Key_G)},
            "startup reads the persisted explicit G Move shortcut from its real editor.tool.move action");
        if ( !view || !moveBinding || !moveTool || !gridBinding )
        {
            continue;
        }
        focus(*view);
        const bool grid = view->showGrid();
        key(*view, Qt::Key_G);
        check(moveTool->isChecked() && view->transformTool() == GlView::TransformTool::Move
                && view->showGrid() == grid,
            "persisted explicit Move=G remains operational when the new default grid G would otherwise collide");
        if ( gridConfiguration == 1 )
        {
            check(gridBinding->shortcuts() == QList<QKeySequence>{QKeySequence(Qt::Key_H)},
                "an explicit custom grid shortcut is preserved exactly");
            key(*view, Qt::Key_H);
            check(view->showGrid() != grid,
                "the custom grid shortcut toggles the grid independently of custom Move=G");
        }
        else if ( gridConfiguration == 2 )
        {
            check(gridBinding->shortcuts().isEmpty(),
                "an explicitly cleared grid shortcut stays cleared after startup");
        }
    }
}

void testConfiguredCleaveShortcutsAndPreferences()
{
    constexpr std::array<const char*, 3> ids{
        "cleave.transform.move", "cleave.transform.rotate", "cleave.transform.scale"};
    struct RestoreBindings
    {
        std::array<QVariant, 3> values{};
        std::array<bool, 3> present{};
        ~RestoreBindings()
        {
            QSettings settings = settingsStore();
            for (std::size_t index = 0; index < ids.size(); ++index)
            {
                const QString key = QStringLiteral("keymap/shortcuts/") + ids[index];
                if (present[index]) settings.setValue(key, values[index]);
                else settings.remove(key);
            }
            settings.sync();
        }
        const std::array<const char*, 3> ids{
            "cleave.transform.move", "cleave.transform.rotate", "cleave.transform.scale"};
    } restore{};
    {
        QSettings settings = settingsStore();
        for (std::size_t index = 0; index < ids.size(); ++index)
        {
            const QString key = QStringLiteral("keymap/shortcuts/") + ids[index];
            restore.present[index] = settings.contains(key);
            restore.values[index] = settings.value(key);
        }
    }
    const std::array<int, 3> codes{Qt::Key_J, Qt::Key_K, Qt::Key_L};
    const std::array<GlView::StagedCleaveTransformMode, 3> modes{
        GlView::StagedCleaveTransformMode::Move,
        GlView::StagedCleaveTransformMode::Rotate,
        GlView::StagedCleaveTransformMode::Scale};
    for (const bool cleared : {false, true})
    {
        {
            QSettings settings = settingsStore();
            for (std::size_t index = 0; index < ids.size(); ++index)
            {
                settings.setValue(QStringLiteral("keymap/shortcuts/") + ids[index],
                    cleared ? QStringList{} : QStringList{
                        QKeySequence(codes[index]).toString(QKeySequence::PortableText)});
            }
            settings.sync();
        }
        MainWindow window(nullptr, true);
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.resize(1280, 800);
        window.activateSectorEditModeForTest();
        window.setDocumentStateForTest(document());
        window.activateSectorEditModeForTest();
        window.setLayerVisualStateForTest({true}, {false});
        window.show();
        GlView* const view = window.glViewForTest();
        auto* owner = window.findChild<QAction*>("viewport.sector.cleave");
        auto* staged = window.findChild<QToolButton*>("cleaveMultiCutButton");
        check(view && owner && owner->isEnabled() && staged,
            "configured Cleave shortcut fixture has its actual enabled Sector command and Multi-cut control");
        if (!view || !owner || !owner->isEnabled() || !staged) continue;
        if (!owner->isChecked()) owner->trigger();
        staged->setChecked(true);
        view->captureStagedCleaveContextForTest(false);
        addGroup(*view);
        check(view->isStagedCleaveInteractionActive(),
            "custom and cleared shortcut checks run in an actual active Multi-cut owner");
        focus(*view);
        const bool grid = view->showGrid();
        const auto globalTool = view->transformTool();
        const auto draft = view->captureStagedCleaveDraftState();
        for (std::size_t index = 0; index < ids.size(); ++index)
        {
            QAction* const action = window.findChild<QAction*>(ids[index]);
            check(action && action->shortcuts() == (cleared ? QList<QKeySequence>{}
                    : QList<QKeySequence>{QKeySequence(codes[index])}),
                "local Cleave actions publish the exact persisted custom or cleared keyboard binding");
            if (!cleared)
            {
                view->setStagedCleaveTransformMode(modes[(index + 1u) % modes.size()]);
                const bool captured = key(*view, codes[index]);
                check(captured && view->stagedCleaveTransformMode() == modes[index],
                    "configured local Cleave key claims native ShortcutOverride and changes only its transform mode");
            }
        }
        for (const int code : {Qt::Key_G, Qt::Key_R, Qt::Key_S, Qt::Key_T, Qt::Key_E})
        {
            view->setStagedCleaveTransformMode(GlView::StagedCleaveTransformMode::Scale);
            const bool captured = key(*view, code);
            check(captured && view->stagedCleaveTransformMode() == GlView::StagedCleaveTransformMode::Scale
                    && view->showGrid() == grid && view->transformTool() == globalTool
                    && view->sectorCleaveToolEnabled()
                    && sameLines(draft, view->captureStagedCleaveDraftState()),
                "custom or cleared local mappings consume conflicting global G/R/S/T/E without tool, grid, or draft leakage");
        }
        QLineEdit text(&window);
        text.show();
        focus(text);
        key(text, Qt::Key_J, Qt::NoModifier, QStringLiteral("j"));
        check(text.text() == QStringLiteral("j")
                && view->stagedCleaveTransformMode() == GlView::StagedCleaveTransformMode::Scale,
            "custom Cleave keyboard bindings preserve focused text typing");
        text.hide();
        if (cleared) continue;
        QAction* const preferences = window.findChild<QAction*>("edit.preferences");
        check(preferences != nullptr, "Cleave bindings are inspected through actual Preferences");
        if (!preferences) continue;
        bool inspected = false;
        QTimer::singleShot(0, &window, [&]()
        {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            check(dialog != nullptr, "Preferences opens its real modal dialog for the Cleave keymap regression");
            if (!dialog) return;
            auto* tree = dialog->findChild<QTreeWidget*>("pref_keymap_tree");
            auto* search = dialog->findChild<QLineEdit*>("pref_keymap_search");
            check(tree && search, "Preferences exposes its keymap tree and search control");
            if (tree && search)
            {
                const auto findRow = [&](const QString& id) -> QTreeWidgetItem*
                {
                    QTreeWidgetItemIterator iterator(tree);
                    while (*iterator)
                    {
                        if ((*iterator)->toolTip(0).contains(QStringLiteral("Binding ID: ") + id))
                            return *iterator;
                        ++iterator;
                    }
                    return nullptr;
                };
                for (std::size_t index = 0; index < ids.size(); ++index)
                {
                    auto* row = findRow(QString::fromLatin1(ids[index]));
                    QWidget* controls = row ? tree->itemWidget(row, 1) : nullptr;
                    auto* editor = controls ? controls->findChild<QKeySequenceEdit*>() : nullptr;
                    check(row && editor && editor->keySequence() == QKeySequence(codes[index])
                            && row->toolTip(0).contains("Multi-cut"),
                        "Preferences lists each editable local Cleave transform with its active Multi-cut context");
                }
                auto* rotation = findRow(QStringLiteral("cleave.gesture.shape_rotate"));
                check(rotation && !tree->itemWidget(rotation, 1)
                        && rotation->text(1) == QStringLiteral("Ctrl Shift Wheel")
                        && rotation->toolTip(0).contains("armed shape"),
                    "Preferences lists the fixed armed-shape wheel rotation and its capture context without a keyboard editor");
                search->setText(QStringLiteral("shape rotate"));
                check(rotation && !rotation->isHidden(),
                    "keymap search keeps matching fixed Cleave gesture instructions visible");
                inspected = true;
            }
            dialog->reject();
        });
        preferences->trigger();
        check(inspected, "Cleave keymap rows were inspected in the actual Preferences dialog");
    }
}

void testCustomCleaveKeysRespectUvCommands()
{
    for (const int command : {Qt::Key_A, Qt::Key_Return, Qt::Key_Escape})
    {
        MainWindow window(nullptr, true);
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.resize(1280, 800);
        auto state = document();
        state.selection.clear();
        state.selection.surfaces.insert({0, 0});
        state.level.sectors.front().surfaces.front().texVertices = {
            TexVertex{0.0f, 0.0f, ColorWhite}, TexVertex{1.0f, 0.0f, ColorWhite},
            TexVertex{1.0f, 1.0f, ColorWhite}, TexVertex{0.0f, 1.0f, ColorWhite}};
        window.activateSurfaceEditModeForTest();
        window.setDocumentStateForTest(std::move(state));
        window.activateSurfaceEditModeForTest();
        window.setLayerVisualStateForTest({true}, {false});
        window.show();
        QApplication::processEvents();
        GlView* const view = window.glViewForTest();
        auto* canvas = window.findChild<QWidget*>("uvEditorCanvas");
        auto* owner = window.findChild<QAction*>("viewport.surface.cleave");
        auto* staged = window.findChild<QToolButton*>("cleaveMultiCutButton");
        check(view && canvas && canvas->isVisible() && owner && owner->isEnabled() && staged,
            "custom Cleave/UV command fixture exposes the actual canvas and enabled Surface Cleave owner");
        if (!view || !canvas || !canvas->isVisible() || !owner || !owner->isEnabled() || !staged) continue;
        view->setSelectionState(0, 0, -1, -1, window.documentStateForTest().selection);
        if (!owner->isChecked()) owner->trigger();
        staged->setChecked(true);
        view->captureStagedCleaveContextForTest(true);
        addGroup(*view);
        view->setStagedCleaveTransformMode(GlView::StagedCleaveTransformMode::Scale);
        check(view->isStagedCleaveInteractionActive(),
            "custom UV command checks run with an actual active Surface Multi-cut draft");
        const auto cleaveBefore = view->captureStagedCleaveDraftState();
        check(cleaveBefore.context.sourceSurface == SurfaceRef{0, 0}
                && view->stagedCleaveTargetsMatchCurrentForTest(true),
            "custom UV command fixture captures the valid active Surface and current document targets");
        const auto globalTool = view->transformTool();
        const bool grid = view->showGrid();
        const auto before = window.uvEditorStoredPointsForTest({0, 0});
        const auto beforeDepth = window.undoDepthForTest();
        focus(*canvas);
        click(*canvas, QPointF(6, 6));
        check(canvas->property("sed.uvSelectedFaceCount").toInt() == 0,
            "an actual empty-canvas click clears UV selection before the custom-key checks");
        if (command != Qt::Key_A)
        {
            key(*canvas, Qt::Key_A);
            check(canvas->property("sed.uvSelectedFaceCount").toInt() == 1,
                "UV confirmation and cancellation begin with a real selected island");
        }
        view->setStagedCleaveTransformShortcutBindings(
            {QKeySequence(Qt::Key_A)},
            {QKeySequence(Qt::Key_Return)},
            {QKeySequence(Qt::Key_Escape)});
        const QPoint center = canvas->rect().center();
        mouse(*canvas, QEvent::MouseMove, center);
        focus(*view);
        QCursor::setPos(canvas->mapToGlobal(center));
        if (command != Qt::Key_A)
        {
            key(*view, Qt::Key_G);
            mouse(*canvas, QEvent::MouseMove, center + QPoint(36, 12));
            check(window.uvEditorStoredPointsForTest({0, 0}) != before,
                "the native UV G gesture produces a changed preview before confirm or cancel");
        }
        const auto preview = window.uvEditorStoredPointsForTest({0, 0});
        if (command == Qt::Key_A)
        {
            view->setFocus(Qt::OtherFocusReason);
        }
        QCursor::setPos(canvas->mapToGlobal(center));
        if (command != Qt::Key_A)
        {
            check(window.uvEditorStoredPointsForTest({0, 0}) == preview && preview != before,
                "UV confirmation and cancellation reach a changed preview without cancelling it through a focus transfer");
        }
        const bool captured = key(*view, command);
        if (command == Qt::Key_A)
        {
            check(captured && canvas->property("sed.uvSelectedFaceCount").toInt() == 1
                    && window.uvEditorStoredPointsForTest({0, 0}) == before
                    && window.undoDepthForTest() == beforeDepth,
                "UV pointer A selects the actual island despite a conflicting custom Cleave Move=A binding");
        }
        else
        {
            const auto expected = command == Qt::Key_Return ? preview : before;
            const auto expectedDepth = command == Qt::Key_Return ? beforeDepth + 1 : beforeDepth;
            const auto afterCommand = window.uvEditorStoredPointsForTest({0, 0});
            const auto afterCommandDepth = window.undoDepthForTest();
            mouse(*canvas, QEvent::MouseMove, center + QPoint(70, 28));
            const auto afterMove = window.uvEditorStoredPointsForTest({0, 0});
            const auto afterMoveDepth = window.undoDepthForTest();
            const bool completed = captured && afterMove == expected
                && afterMoveDepth == expectedDepth;
            if (!completed)
            {
                std::cerr << "Custom UV command: key=" << command << " captured=" << captured
                    << " previewChanged=" << (preview != before)
                    << " commandEqualsExpected=" << (afterCommand == expected)
                    << " commandEqualsBaseline=" << (afterCommand == before)
                    << " followupEqualsExpected=" << (afterMove == expected)
                    << " followupEqualsCommand=" << (afterMove == afterCommand)
                    << " depth=" << beforeDepth << ',' << afterCommandDepth << ',' << afterMoveDepth
                    << " expectedDepth=" << expectedDepth << '\n';
                uvRoutingDiagnostic(window, *view, *canvas, "custom-command-after-move", command);
            }
            check(completed,
                command == Qt::Key_Return
                    ? "UV pointer Enter commits one real UV edit and ends its preview despite custom Cleave Rotate=Enter"
                    : "UV pointer Escape restores the exact UV baseline without history despite custom Cleave Scale=Escape");
        }
        const auto cleaveAfter = view->captureStagedCleaveDraftState();
        const bool cleaveUnchanged = view->stagedCleaveTransformMode() == GlView::StagedCleaveTransformMode::Scale
                && view->surfaceCleaveToolEnabled() && view->showGrid() == grid
                && view->transformTool() == globalTool
                && sameLines(cleaveBefore, cleaveAfter);
        if (!cleaveUnchanged)
        {
            std::cerr << "Custom UV/Cleave invariant: key=" << command
                << " mode=" << int(view->stagedCleaveTransformMode())
                << " expectedMode=" << int(GlView::StagedCleaveTransformMode::Scale)
                << " surfaceOwner=" << view->surfaceCleaveToolEnabled()
                << " gridEquals=" << (view->showGrid() == grid)
                << " toolEquals=" << (view->transformTool() == globalTool)
                << " draftEquals=" << sameLines(cleaveBefore, cleaveAfter)
                << " lines=" << cleaveBefore.lines.size() << ',' << cleaveAfter.lines.size()
                << " selectedEquals=" << (cleaveBefore.selectedCleaveLines == cleaveAfter.selectedCleaveLines)
                << " orderEquals=" << (cleaveBefore.applyOrder == cleaveAfter.applyOrder)
                << " contextValid=" << cleaveBefore.context.valid << ',' << cleaveAfter.context.valid
                << " contextViewEquals=" << (cleaveBefore.context.view == cleaveAfter.context.view)
                << " contextProjectionEquals=" << (cleaveBefore.context.projection == cleaveAfter.context.projection)
                << " contextViewport=" << cleaveBefore.context.viewportWidth << 'x'
                << cleaveBefore.context.viewportHeight << ',' << cleaveAfter.context.viewportWidth << 'x'
                << cleaveAfter.context.viewportHeight << '\n';
        }
        check(cleaveUnchanged,
            "custom UV select/confirm/cancel commands leave the active Cleave draft, mode and global tools unchanged");
        // End a deliberately failed UV modal using its normal native Escape,
        // with the conflicting test bindings removed before fixture teardown.
        view->setStagedCleaveTransformShortcutBindings(
            {QKeySequence(Qt::Key_G)}, {QKeySequence(Qt::Key_R)}, {QKeySequence(Qt::Key_S)});
        focus(*canvas);
        key(*canvas, Qt::Key_Escape);
    }
}

} // namespace

void runCleaveInputRoutingTests()
{
    QTemporaryDir temporary{};
    check(temporary.isValid(), "Cleave input uses isolated settings");
    if ( !temporary.isValid() )
    {
        return;
    }
    const std::filesystem::path previousSettings = appSettingsIniPath();
    struct RestoreSettings
    {
        std::filesystem::path path{};
        ~RestoreSettings() { setAppSettingsIniPathOverrideForTest(path); }
    } restoreSettings{previousSettings};
    setAppSettingsIniPathOverrideForTest(
        std::filesystem::path(temporary.path().toStdWString()) / "cleave-input.ini");
    testNativeShortcutOwnership();
    testUvKeyReleaseRouting();
    testUvPointerShortcutOwnership();
    testPersistedMoveShortcutCollision();
    testConfiguredCleaveShortcutsAndPreferences();
    for ( const bool perspective : {false, true} )
    {
        testWheelTransforms(perspective);
        testShapeWheelAndCameraPlane(perspective);
        testArmedShapeRotationWheel(perspective);
        testModeChangeDuringPointerGesture(perspective);
        testCameraPlanePreviewAfterStationaryOrbit(perspective);
    }
}

void runCleaveUvKeyReleaseTests()
{
    testUvKeyReleaseRouting();
}

void runCleaveModeChangeTests()
{
    for ( const bool perspective : {false, true} )
    {
        testModeChangeDuringPointerGesture(perspective);
    }
}

void runCleavePointerOwnershipTests()
{
    testUvPointerShortcutOwnership();
}

void runCleaveCameraPreviewTests()
{
    for ( const bool perspective : {false, true} )
    {
        testCameraPlanePreviewAfterStationaryOrbit(perspective);
    }
}

void runCleaveUvCustomShortcutOwnershipTests()
{
    testCustomCleaveKeysRespectUvCommands();
}

void runCleaveFlyAndCustomShortcutTests()
{
    testUvPointerShortcutOwnership();
    testPersistedMoveShortcutCollision();
    testConfiguredCleaveShortcutsAndPreferences();
}

} // namespace sed::app_tests
