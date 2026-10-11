#include "test_common.hpp"

#include "app_settings.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "render/cleave/cleave_wheel_session.hpp"
#include "sed/assets/sed.hpp"
#include "sed/level/build.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QTimer>
#include <QWheelEvent>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace sed::app_tests
{
namespace
{

using Draft = GlView::RecoveryState::CleaveDraftSnapshot;
using History = GlView::RecoveryState::CleaveHistory;
using Family = GlView::CleaveWorkspaceFamily;
using Mode = GlView::StagedCleaveTransformMode;

constexpr int kExpectedHudIdleIntervalMs = 400;
constexpr int kHudTimeoutDeliveryAllowanceMs = 100;

bool sameLine(const GlView::RecoveryState::CleaveLine& left,
    const GlView::RecoveryState::CleaveLine& right)
{
    const auto& a = left.authoredCamera;
    const auto& b = right.authoredCamera;
    return closeVec(left.first, right.first, 0.0)
        && closeVec(left.second, right.second, 0.0)
        && closeVec(left.planeNormal, right.planeNormal, 0.0)
        && closeVec(left.constructionPlaneNormal, right.constructionPlaneNormal, 0.0)
        && closeVec(left.angleReferenceAxis, right.angleReferenceAxis, 0.0)
        && left.linkedToPrevious == right.linkedToPrevious
        && left.linkedToNext == right.linkedToNext
        && a.orbit.center == b.orbit.center && a.orbit.pan == b.orbit.pan
        && a.orbit.radius == b.orbit.radius && a.orbit.distance == b.orbit.distance
        && a.orbit.yawDeg == b.orbit.yawDeg && a.orbit.pitchDeg == b.orbit.pitchDeg
        && a.orbit.rollDeg == b.orbit.rollDeg && a.projectionType == b.projectionType
        && a.fovXDeg == b.fovXDeg && a.orthoScale == b.orthoScale
        && a.nearPlane == b.nearPlane && a.farPlane == b.farPlane
        && a.clipEnabled == b.clipEnabled && a.valid == b.valid;
}

bool sameDraft(const Draft& left, const Draft& right)
{
    if ( left.lines.size() != right.lines.size()
        || left.applyOrder != right.applyOrder
        || left.selectedCleaveLines != right.selectedCleaveLines
        || left.selectedCleaveLine != right.selectedCleaveLine
        || left.context.valid != right.context.valid
        || left.context.surfaceMode != right.context.surfaceMode
        || left.context.projection != right.context.projection
        || left.context.view != right.context.view
        || left.context.viewportWidth != right.context.viewportWidth
        || left.context.viewportHeight != right.context.viewportHeight
        || left.context.sourceSurface != right.context.sourceSurface
        || left.context.targetSectors != right.context.targetSectors
        || left.context.targetSurfaces != right.context.targetSurfaces
        || left.chainOrigin.has_value() != right.chainOrigin.has_value() )
    {
        return false;
    }
    if ( left.chainOrigin && (left.chainOrigin->lineIndex != right.chainOrigin->lineIndex
            || left.chainOrigin->endpoint != right.chainOrigin->endpoint) )
    {
        return false;
    }
    return std::equal(left.lines.begin(), left.lines.end(), right.lines.begin(), sameLine);
}

bool sameHistory(const std::optional<History>& left, const std::optional<History>& right)
{
    if ( left.has_value() != right.has_value() )
    {
        return false;
    }
    return !left || (left->cursor == right->cursor
        && left->snapshots.size() == right->snapshots.size()
        && std::equal(left->snapshots.begin(), left->snapshots.end(),
            right->snapshots.begin(), sameDraft));
}

bool oneCommandAdded(const std::optional<History>& before,
    const std::optional<History>& after)
{
    return before && after && after->cursor == before->cursor + 1u
        && after->snapshots.size() == before->cursor + 2u;
}

std::vector<std::pair<QVector3D, QVector3D>> liveEndpoints(const GlView& view,
    const std::size_t lineCount)
{
    std::vector<std::pair<QVector3D, QVector3D>> endpoints{};
    for ( std::size_t index = 0; index < lineCount; ++index )
    {
        endpoints.emplace_back(view.stagedCleaveLineFirstForTest(index),
            view.stagedCleaveLineSecondForTest(index));
    }
    return endpoints;
}

bool wheel(QWidget& target, const int angleDelta,
    const Qt::KeyboardModifiers modifiers = Qt::ControlModifier,
    const QPoint pixelDelta = {})
{
    const QPointF position = target.rect().center();
    QWheelEvent event(position, target.mapToGlobal(position.toPoint()),
        pixelDelta, QPoint(0, angleDelta), Qt::NoButton, modifiers,
        Qt::NoScrollPhase, false);
    event.setAccepted(false);
    QApplication::sendEvent(&target, &event);
    return event.isAccepted();
}

bool releaseKey(QWidget& target, const int key,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier,
    const bool autoRepeat = false)
{
    QKeyEvent event(QEvent::KeyRelease, key, modifiers,
        QString{}, autoRepeat);
    event.setAccepted(false);
    QApplication::sendEvent(&target, &event);
    return event.isAccepted();
}

void releaseControl(QWidget& target, const bool autoRepeat = false)
{
    (void)releaseKey(target, Qt::Key_Control, Qt::NoModifier, autoRepeat);
}

void mouse(QWidget& target, const QEvent::Type type, const QPointF position,
    const Qt::MouseButton button = Qt::NoButton,
    const Qt::MouseButtons buttons = Qt::NoButton)
{
    const QPointF global = target.mapToGlobal(position.toPoint());
    QMouseEvent event(type, position, global, global, button, buttons, Qt::NoModifier);
    QApplication::sendEvent(&target, &event);
}

void escape(QWidget& target)
{
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&target, &event);
}

void saveWithoutModalErrors(QAction& action, QWidget* const shortcutReceiver = nullptr)
{
    int errorDialogs{};
    QTimer driver{};
    driver.setInterval(5);
    QObject::connect(&driver, &QTimer::timeout, [&]()
    {
        if ( auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) )
        {
            ++errorDialogs;
            message->accept();
        }
    });
    driver.start();
    if ( shortcutReceiver )
    {
        QKeyEvent save(QEvent::KeyPress, Qt::Key_S, Qt::ControlModifier);
        QApplication::sendEvent(shortcutReceiver, &save);
    }
    else
    {
        action.trigger();
    }
    driver.stop();
    check(errorDialogs == 0, "wheel session native Save succeeds without a modal error");
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

void configure(GlView& view, const editor::DocumentState& state,
    const bool perspective, const bool surface)
{
    view.resize(640, 480);
    view.setLevel(&state.level);
    editor::Selection selection = state.selection;
    if ( surface )
    {
        selection.sectors.clear();
        selection.surfaces.insert(SurfaceRef{0, 0});
    }
    view.setSelectionState(0, surface ? 0 : -1, -1, -1, selection);
    view.setSurfaceSubMode(GlView::SurfaceSubMode::Face);
    if ( surface )
    {
        view.setSurfaceCleaveToolEnabled(true);
    }
    else
    {
        view.setSectorCleaveToolEnabled(true);
    }
    // Selecting a family installs its independent saved/default options.
    // Arm this fixture's Multi-cut after that ownership exchange.
    view.setCleaveStagedMode(true);
    view.setCleaveChainEnabled(false);
    view.setCleaveBounded(true);
    check(view.isStagedCleaveInteractionActive(),
        "wheel fixture activates Multi-cut in the selected Sector or Surface family");
    view.setCameraProjectionType(perspective
        ? app::ViewportCamera::ProjectionType::Perspective
        : app::ViewportCamera::ProjectionType::Orthographic);
    view.setOrbitViewImmediate(31.0f, 22.0f, 13.0f);
    view.setCameraOrbitDistance(8.0f);
    view.setCameraOrthoScale(96.0f);
    view.setTransformGridSnapEnabled(false);
    view.captureStagedCleaveContextForTest(surface);
    view.addStagedCleaveLineForTest({-1, 0, -1}, {1, 0, -1}, {0, 0, 1});
    view.addStagedCleaveLineForTest({1, 0, -1}, {1, 0, 1}, {-1, 0, 0});
    view.addStagedCleaveLineForTest({1, 0, 1}, {-1, 0, 1}, {0, 0, -1});
    auto draft = view.captureStagedCleaveDraftState();
    draft.selectedCleaveLines = {0, 1};
    draft.selectedCleaveLine = 1;
    draft.applyOrder = {2, 0, 1};
    draft.chainOrigin = GlView::RecoveryState::CleaveChainOrigin{2, 0};
    view.restoreStagedCleaveDraftState(draft);
}

void testCumulativeScale(const bool perspective, const bool surface)
{
    auto state = document();
    GlView view{};
    configure(view, state, perspective, surface);
    view.setStagedCleaveTransformMode(Mode::Scale);
    const Draft before = view.captureStagedCleaveDraftState();
    const auto historyBefore = view.captureStagedCleaveHistory();
    const std::uint64_t authoredRevision = view.stagedCleaveAuthoredRevision();
    const float distance = view.cameraOrbitDistance();
    const float orthoScale = view.cameraOrthoScale();
    const auto levelBefore = assets::saveToSedBytes(state.level);
    check(wheel(view, 120)
            && view.stagedCleaveTransformHudText() == QStringLiteral("Scale 1.10×"),
        "the first Ctrl-held Scale detent starts a fresh presentation timeline");
    QTimer* const hideTimer = view.findChild<QTimer*>(
        "cleave_transform_hud_hide_timer");
    QTimer* const presentationTimer = view.findChild<QTimer*>(
        "cleave_transform_hud_presentation_timer");
    check(hideTimer && hideTimer->isSingleShot() && hideTimer->isActive()
            && hideTimer->interval() == kExpectedHudIdleIntervalMs
            && presentationTimer && presentationTimer->isSingleShot()
            && presentationTimer->isActive()
            && presentationTimer->interval() > 0
            && presentationTimer->interval() <= 12,
        "Cleave HUD uses one 400 ms hide deadline and a separate single-shot fade-edge wake");
    check(wheel(view, 120)
            && view.stagedCleaveTransformHudText() == QStringLiteral("Scale 1.21×"),
        "Ctrl-held Scale wheel reports the cumulative factor after two detents");
    check(sameHistory(historyBefore, view.captureStagedCleaveHistory())
            && view.stagedCleaveAuthoredRevision() == authoredRevision
            && sameDraft(before, view.captureStagedCleaveDraftState()),
        "live wheel samples retain the settled draft/history and do not publish authored commands");
    releaseControl(view, true);
    check(sameHistory(historyBefore, view.captureStagedCleaveHistory())
            && hideTimer && hideTimer->isActive()
            && presentationTimer && presentationTimer->isActive(),
        "an autorepeated Control release cannot finalize a live wheel session");
    check(wheel(view, -60) && wheel(view, 0, Qt::ControlModifier, QPoint(0, 80))
            && view.stagedCleaveTransformHudText() == QStringLiteral("Scale 1.27×"),
        "Scale wheel accumulates inverse, fractional and pixel-only samples in one session");
    const double factor = std::pow(1.1, 2.5);
    const TVector pivot{1.0 / 3.0, 0.0, -1.0 / 3.0};
    check(closeQVec(view.stagedCleaveLineFirstForTest(0),
            vector(pivot + (before.lines[0].first - pivot) * factor), 1.0e-6f)
            && view.stagedCleaveLineFirstForTest(2) == view.stagedCleaveLineSecondForTest(1)
            && view.stagedCleaveLineSecondForTest(2) == vector(before.lines[2].second),
        "live cumulative Scale uses the original distinct-endpoint pivot and carries only the shared neighbor");
    releaseControl(view);
    check(hideTimer && !hideTimer->isActive()
            && presentationTimer && !presentationTimer->isActive(),
        "the real Control release stops both the hide deadline and fade-edge wake immediately");
    const Draft scaled = view.captureStagedCleaveDraftState();
    const auto settledHistory = view.captureStagedCleaveHistory();
    check(oneCommandAdded(historyBefore, settledHistory)
            && closeVec(scaled.lines[0].first,
                pivot + (before.lines[0].first - pivot) * factor, 1.0e-12)
            && scaled.applyOrder == before.applyOrder
            && scaled.selectedCleaveLines == before.selectedCleaveLines
            && view.stagedCleaveAuthoredRevision() == authoredRevision + 1u,
        "Control release commits the cumulative Scale gesture as exactly one authored local command");
    check(view.cameraOrbitDistance() == distance && view.cameraOrthoScale() == orthoScale
            && assets::saveToSedBytes(state.level) == levelBefore,
        "cumulative wheel scaling leaves camera and authoritative Level unchanged");
    check(view.undoStagedCleaveHistory() && sameDraft(before, view.captureStagedCleaveDraftState())
            && view.redoStagedCleaveHistory() && sameDraft(scaled, view.captureStagedCleaveDraftState()),
        "one Undo and Redo restore every exact cumulative Scale draft field");
    check(wheel(view, 120)
            && view.stagedCleaveTransformHudText() == QStringLiteral("Scale 1.10×"),
        "the next Ctrl wheel session restarts its factor at the newly settled baseline");
    releaseControl(view);
    check(oneCommandAdded(settledHistory, view.captureStagedCleaveHistory()),
        "a second completed Ctrl session adds exactly one separate local command");
}

QTimer* transformHudExpiryTimer(GlView& view)
{
    QTimer* const timer = view.findChild<QTimer*>("cleave_transform_hud_hide_timer");
    check(timer != nullptr, "Cleave transform HUD owns an idle single-shot expiry timer");
    return timer;
}

QTimer* transformHudPresentationTimer(GlView& view)
{
    QTimer* const timer = view.findChild<QTimer*>(
        "cleave_transform_hud_presentation_timer");
    check(timer != nullptr,
        "Cleave transform HUD owns a single-shot presentation wake timer");
    return timer;
}

void dispatchHudExpiry(GlView& view, QTimer& timer)
{
    const int interval = timer.interval();
    int firings{};
    const auto connection = QObject::connect(&timer, &QTimer::timeout, &view,
        [&]() { ++firings; });
    timer.start(0);
    QApplication::processEvents();
    QObject::disconnect(connection);
    check(firings == 1 && !timer.isActive(),
        "the actual native HUD single-shot timer fires once and then stops");
    timer.setInterval(interval);
}

void testIdleWheelHudExpiry(const Mode mode, const bool cancel)
{
    auto state = document();
    GlView view{};
    configure(view, state, false, false);
    view.setStagedCleaveTransformMode(mode);
    if ( cancel )
    {
        wheel(view, 120);
        releaseControl(view);
        check(view.undoStagedCleaveHistory(),
            "idle wheel cancellation fixture retains an exact Redo branch");
    }
    const Draft before = view.captureStagedCleaveDraftState();
    const auto historyBefore = view.captureStagedCleaveHistory();
    const auto revisionBefore = view.stagedCleaveAuthoredRevision();
    const auto baselineCaptures = view.stagedCleaveBaselineCaptureCountForTest();
    const auto preparedWorksets = view.stagedCleaveTransformPreparationCountForTest();
    const auto completedCaptures = view.stagedCleaveHistoryCurrentCaptureCountForTest();
    const auto levelBefore = assets::saveToSedBytes(state.level);
    const float distance = view.cameraOrbitDistance();
    const float orthoScale = view.cameraOrthoScale();
    wheel(view, 120);
    wheel(view, 120);
    QTimer* const timer = transformHudExpiryTimer(view);
    if ( !timer )
    {
        return;
    }
    const auto endpoints = liveEndpoints(view, before.lines.size());
    check(timer->isActive() && timer->isSingleShot()
            && timer->interval() == kExpectedHudIdleIntervalMs
            && view.stagedCleaveTransformHudText() == (mode == Mode::Rotate
                ? QStringLiteral("Rotate 10.00°") : QStringLiteral("Scale 1.21×")),
        "real Ctrl-held wheel samples arm rotation and scale HUD idle expiry for exactly 400 ms");
    dispatchHudExpiry(view, *timer);
    check(view.stagedCleaveTransformHudText().isEmpty()
            && view.stagedCleaveTransformHudRectForTest().isEmpty()
            && liveEndpoints(view, before.lines.size()) == endpoints
            && sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory())
            && view.stagedCleaveAuthoredRevision() == revisionBefore
            && view.stagedCleaveBaselineCaptureCountForTest() == baselineCaptures + 1u
            && view.stagedCleaveTransformPreparationCountForTest() == preparedWorksets + 1u
            && view.stagedCleaveHistoryCurrentCaptureCountForTest() == completedCaptures,
        "idle wheel expiry hides only the HUD and retains every live endpoint, exact baseline/history and prepared workset");
    wheel(view, 120);
    check(timer->isActive() && timer->interval() == kExpectedHudIdleIntervalMs
            && view.stagedCleaveTransformHudText() == (mode == Mode::Rotate
                ? QStringLiteral("Rotate 15.00°") : QStringLiteral("Scale 1.33×"))
            && sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory())
            && view.stagedCleaveBaselineCaptureCountForTest() == baselineCaptures + 1u
            && view.stagedCleaveTransformPreparationCountForTest() == preparedWorksets + 1u
            && view.stagedCleaveHistoryCurrentCaptureCountForTest() == completedCaptures,
        "the next Ctrl-held wheel sample restores the cumulative readout and rearms expiry without a new baseline or workset");
    const QString renewedHud = view.stagedCleaveTransformHudText();
    QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
    check(view.stagedCleaveTransformHudText() == renewedHud && timer->isActive(),
        "an obsolete idle deadline cannot prematurely hide the next cumulative wheel readout");
    if ( cancel )
    {
        escape(view);
        releaseControl(view);
        check(!timer->isActive() && view.stagedCleaveTransformHudText().isEmpty()
                && sameDraft(before, view.captureStagedCleaveDraftState())
                && sameHistory(historyBefore, view.captureStagedCleaveHistory())
                && view.stagedCleaveAuthoredRevision() == revisionBefore
                && view.stagedCleaveHistoryCurrentCaptureCountForTest() == completedCaptures,
            "Escape after idle HUD expiry restores the exact wheel baseline and Redo without an authored command");
    }
    else
    {
        dispatchHudExpiry(view, *timer);
        check(releaseKey(view, Qt::Key_Control),
            "a wheel with an expired HUD still owns its real Control release");
        const Draft settled = view.captureStagedCleaveDraftState();
        check(!timer->isActive() && view.stagedCleaveTransformHudText().isEmpty()
                && oneCommandAdded(historyBefore, view.captureStagedCleaveHistory())
                && view.stagedCleaveAuthoredRevision() == revisionBefore + 1u
                && view.stagedCleaveHistoryCurrentCaptureCountForTest() == completedCaptures + 1u
                && view.undoStagedCleaveHistory()
                && sameDraft(before, view.captureStagedCleaveDraftState())
                && view.redoStagedCleaveHistory()
                && sameDraft(settled, view.captureStagedCleaveDraftState()),
            "Control release after repeated idle HUD expiry commits one cumulative command with exact Undo and Redo");
    }
    check(view.cameraOrbitDistance() == distance && view.cameraOrthoScale() == orthoScale
            && assets::saveToSedBytes(state.level) == levelBefore,
        "idle wheel HUD visibility changes preserve the camera and authoritative Level exactly");
}

void testRealIdleWheelHudDeadline()
{
    auto state = document();
    GlView view{};
    configure(view, state, false, false);
    view.setStagedCleaveTransformMode(Mode::Rotate);
    const Draft before = view.captureStagedCleaveDraftState();
    const auto historyBefore = view.captureStagedCleaveHistory();
    const auto revisionBefore = view.stagedCleaveAuthoredRevision();
    const auto completedCaptures = view.stagedCleaveHistoryCurrentCaptureCountForTest();
    QTimer* const timer = transformHudExpiryTimer(view);
    if ( !timer )
    {
        return;
    }
    const int interval = timer->interval();
    check(interval == kExpectedHudIdleIntervalMs,
        "the real held-wheel HUD timer retains its exact 400 ms default deadline");
    QEventLoop loop{};
    QElapsedTimer idle{};
    QTimer freshSample{};
    QTimer watchdog{};
    freshSample.setSingleShot(true);
    freshSample.setTimerType(Qt::PreciseTimer);
    freshSample.setInterval(kExpectedHudIdleIntervalMs / 2);
    watchdog.setSingleShot(true);
    watchdog.setTimerType(Qt::PreciseTimer);
    watchdog.setInterval(kExpectedHudIdleIntervalMs + kHudTimeoutDeliveryAllowanceMs);
    bool freshSampleDelivered{};
    bool timeoutDelivered{};
    bool watchdogExpired{};
    qint64 idleMilliseconds{};
    std::vector<std::pair<QVector3D, QVector3D>> endpoints{};
    QObject::connect(&freshSample, &QTimer::timeout, &loop, [&]()
    {
        idle.restart();
        wheel(view, 120);
        endpoints = liveEndpoints(view, before.lines.size());
        freshSampleDelivered = true;
        watchdog.start();
    });
    QObject::connect(&watchdog, &QTimer::timeout, &loop, [&]()
    {
        watchdogExpired = true;
        loop.quit();
    });
    const auto connection = QObject::connect(timer, &QTimer::timeout, &loop, [&]()
    {
        timeoutDelivered = true;
        idleMilliseconds = idle.elapsed();
        loop.quit();
    });
    idle.start();
    wheel(view, 120);
    freshSample.start();
    watchdog.start();
    loop.exec();
    freshSample.stop();
    watchdog.stop();
    QObject::disconnect(connection);
    check(freshSampleDelivered && timeoutDelivered && !watchdogExpired
            && idleMilliseconds >= interval
            && idleMilliseconds <= kExpectedHudIdleIntervalMs + kHudTimeoutDeliveryAllowanceMs
            && timer->interval() == interval && !timer->isActive()
            && view.stagedCleaveTransformHudText().isEmpty()
            && view.stagedCleaveTransformHudRectForTest().isEmpty(),
        "a fresh sample at half the default rearms the held-wheel HUD for 400 ms and expiry arrives within 500 ms");
    check(liveEndpoints(view, before.lines.size()) == endpoints
            && sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory())
            && view.stagedCleaveAuthoredRevision() == revisionBefore
            && view.stagedCleaveHistoryCurrentCaptureCountForTest() == completedCaptures,
        "real wall-clock HUD expiry retains live rotation and the exact unfinished draft/history");
    wheel(view, 120);
    check(view.stagedCleaveTransformHudText() == QStringLiteral("Rotate 15.00°"),
        "rotation after a real idle deadline remains cumulative from the original Ctrl-held baseline");
    releaseControl(view);
    check(oneCommandAdded(historyBefore, view.captureStagedCleaveHistory())
            && view.undoStagedCleaveHistory()
            && sameDraft(before, view.captureStagedCleaveDraftState()),
        "one real Control release after wall-clock expiry retains one command and exact Undo");
}

void testUnchangedWheelSampleRestoresExpiredHud(const bool shapeScale)
{
    auto state = document();
    GlView view{};
    configure(view, state, false, false);
    view.setStagedCleaveTransformMode(Mode::Rotate);
    view.setCleaveAngleStepDegrees(30.0);
    auto options = view.cleaveShapeOptions();
    if ( shapeScale )
    {
        options.placementArmed = true;
        options.cameraPlane = true;
        options.width = 5000.0;
        options.height = 5000.0;
        view.setCleaveShapeOptions(options);
    }
    const Draft before = view.captureStagedCleaveDraftState();
    const auto historyBefore = view.captureStagedCleaveHistory();
    const auto revisionBefore = view.stagedCleaveAuthoredRevision();
    wheel(view, shapeScale ? 100000 : 120);
    QTimer* const timer = transformHudExpiryTimer(view);
    if ( !timer )
    {
        return;
    }
    const QString expected = shapeScale
        ? QStringLiteral("Scale 2.00×") : QStringLiteral("Rotate 30.00°");
    const auto endpoints = liveEndpoints(view, before.lines.size());
    const auto baselineCaptures = view.stagedCleaveBaselineCaptureCountForTest();
    const auto preparedWorksets = view.stagedCleaveTransformPreparationCountForTest();
    const auto completedCaptures = view.stagedCleaveHistoryCurrentCaptureCountForTest();
    check(view.stagedCleaveTransformHudText() == expected,
        "unchanged-sample fixture starts a real quantized rotation or capped shape scale");
    dispatchHudExpiry(view, *timer);
    wheel(view, shapeScale ? 120 : 30);
    check(timer->isActive() && timer->interval() == kExpectedHudIdleIntervalMs
            && view.stagedCleaveTransformHudText() == expected
            && liveEndpoints(view, before.lines.size()) == endpoints
            && sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory())
            && view.stagedCleaveAuthoredRevision() == revisionBefore
            && view.stagedCleaveBaselineCaptureCountForTest() == baselineCaptures
            && view.stagedCleaveTransformPreparationCountForTest() == preparedWorksets
            && view.stagedCleaveHistoryCurrentCaptureCountForTest() == completedCaptures
            && (!shapeScale || (view.cleaveShapeOptions().width == 10000.0
                && view.cleaveShapeOptions().height == 10000.0)),
        "a nonzero consumed wheel sample restores and rearms the cumulative HUD even when rounding or the shape cap leaves geometry unchanged");
    escape(view);
    releaseControl(view);
    check(!timer->isActive() && view.stagedCleaveTransformHudText().isEmpty()
            && sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory())
            && (!shapeScale || (view.cleaveShapeOptions().width == options.width
                && view.cleaveShapeOptions().height == options.height)),
        "Escape after an unchanged wheel sample retains the exact line or shape session baseline");
}

void testIdlePointerHudExpiry(const Mode mode, const bool perspective, const bool cancel)
{
    auto state = document();
    GlView view{};
    configure(view, state, perspective, false);
    view.setStagedCleaveTransformMode(mode);
    if ( cancel )
    {
        wheel(view, 120);
        releaseControl(view);
        check(view.undoStagedCleaveHistory(),
            "idle pointer cancellation fixture retains an exact Redo branch");
    }
    const Draft before = view.captureStagedCleaveDraftState();
    const auto historyBefore = view.captureStagedCleaveHistory();
    const auto revisionBefore = view.stagedCleaveAuthoredRevision();
    const auto baselineCaptures = view.stagedCleaveBaselineCaptureCountForTest();
    const auto preparedWorksets = view.stagedCleaveTransformPreparationCountForTest();
    const auto completedCaptures = view.stagedCleaveHistoryCurrentCaptureCountForTest();
    const auto levelBefore = assets::saveToSedBytes(state.level);
    const float distance = view.cameraOrbitDistance();
    const float orthoScale = view.cameraOrthoScale();
    const auto body = view.projectWorldToViewport(vector(
        (before.lines[0].first + before.lines[0].second) * 0.5));
    check(body.has_value(), "idle pointer fixture projects the actual selected line body");
    if ( !body )
    {
        return;
    }
    const QPointF firstMotion = *body + QPoint(50, 12);
    const QPointF secondMotion = *body + QPoint(75, 25);
    mouse(view, QEvent::MouseButtonPress, *body, Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseMove, firstMotion, Qt::NoButton, Qt::LeftButton);
    QTimer* const timer = transformHudExpiryTimer(view);
    if ( !timer )
    {
        return;
    }
    const auto endpoints = liveEndpoints(view, before.lines.size());
    const QString liveHud = view.stagedCleaveTransformHudText();
    check(!liveHud.isEmpty() && endpoints.front().first != vector(before.lines[0].first)
            && view.viewportMouseGestureButton() == Qt::LeftButton
            && timer->isActive() && timer->isSingleShot()
            && timer->interval() == kExpectedHudIdleIntervalMs,
        "real held-pointer Rotate and Scale previews arm HUD idle expiry for exactly 400 ms");
    dispatchHudExpiry(view, *timer);
    check(view.stagedCleaveTransformHudText().isEmpty()
            && view.stagedCleaveTransformHudRectForTest().isEmpty()
            && view.viewportMouseGestureButton() == Qt::LeftButton
            && liveEndpoints(view, before.lines.size()) == endpoints
            && sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory())
            && view.stagedCleaveAuthoredRevision() == revisionBefore
            && view.stagedCleaveBaselineCaptureCountForTest() == baselineCaptures + 1u
            && view.stagedCleaveTransformPreparationCountForTest() == preparedWorksets + 1u
            && view.stagedCleaveHistoryCurrentCaptureCountForTest() == completedCaptures,
        "idle pointer expiry hides only presentation and retains the exact rollback baseline, geometry and one prepared workset");
    mouse(view, QEvent::MouseMove, secondMotion, Qt::NoButton, Qt::LeftButton);
    check(timer->isActive() && timer->interval() == kExpectedHudIdleIntervalMs
            && !view.stagedCleaveTransformHudText().isEmpty(),
        "motion after pointer HUD expiry restores the readout and rearms its exact 400 ms deadline");
    mouse(view, QEvent::MouseMove, firstMotion, Qt::NoButton, Qt::LeftButton);
    QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
    check(view.stagedCleaveTransformHudText() == liveHud
            && liveEndpoints(view, before.lines.size()) == endpoints
            && sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory())
            && view.stagedCleaveBaselineCaptureCountForTest() == baselineCaptures + 1u
            && view.stagedCleaveTransformPreparationCountForTest() == preparedWorksets + 1u
            && view.stagedCleaveHistoryCurrentCaptureCountForTest() == completedCaptures,
        "continued pointer motion uses the original baseline exactly and an obsolete deadline cannot hide its fresh readout");
    if ( cancel )
    {
        escape(view);
    }
    mouse(view, QEvent::MouseButtonRelease, firstMotion, Qt::LeftButton);
    check(!timer->isActive() && view.stagedCleaveTransformHudText().isEmpty()
            && view.stagedCleaveTransformHudRectForTest().isEmpty()
            && view.viewportMouseGestureButton() == Qt::NoButton,
        "real pointer release immediately clears the readout and cancels idle expiry");
    if ( cancel )
    {
        check(sameDraft(before, view.captureStagedCleaveDraftState())
                && sameHistory(historyBefore, view.captureStagedCleaveHistory())
                && view.stagedCleaveAuthoredRevision() == revisionBefore
                && view.stagedCleaveHistoryCurrentCaptureCountForTest() == completedCaptures,
            "Escape after idle pointer HUD expiry restores every draft/history field and retains Redo");
    }
    else
    {
        const Draft settled = view.captureStagedCleaveDraftState();
        check(oneCommandAdded(historyBefore, view.captureStagedCleaveHistory())
                && view.stagedCleaveAuthoredRevision() == revisionBefore + 1u
                && view.stagedCleaveHistoryCurrentCaptureCountForTest() == completedCaptures + 1u
                && view.undoStagedCleaveHistory()
                && sameDraft(before, view.captureStagedCleaveDraftState())
                && view.redoStagedCleaveHistory()
                && sameDraft(settled, view.captureStagedCleaveDraftState()),
            "pointer release after idle HUD expiry commits one command with exact Undo and Redo");
    }
    check(view.cameraOrbitDistance() == distance && view.cameraOrthoScale() == orthoScale
            && assets::saveToSedBytes(state.level) == levelBefore,
        "idle pointer HUD visibility changes preserve camera and authoritative Level exactly");
}

void testModifierReleaseHidesTransformHud(const Mode mode)
{
    auto state = document();
    GlView view{};
    configure(view, state, false, false);
    view.setStagedCleaveTransformMode(mode);
    const Draft before = view.captureStagedCleaveDraftState();
    const auto historyBefore = view.captureStagedCleaveHistory();
    const QString expected = mode == Mode::Rotate
        ? QStringLiteral("Rotate 10.00°") : QStringLiteral("Scale 1.21×");
    wheel(view, 120);
    wheel(view, 120);
    QTimer* const timer = transformHudExpiryTimer(view);
    if ( !timer )
    {
        return;
    }
    check(timer->isSingleShot() && timer->isActive()
            && timer->interval() == kExpectedHudIdleIntervalMs,
        "Ctrl-held cumulative transforms arm an exact 400 ms presentation deadline");
    (void)releaseKey(view, Qt::Key_Control, Qt::NoModifier, true);
    (void)releaseKey(view, Qt::Key_Shift, Qt::ControlModifier);
    (void)releaseKey(view, Qt::Key_A, Qt::ControlModifier);
    check(view.stagedCleaveTransformHudText() == expected
            && timer->isActive()
            && sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory()),
        "autorepeated Control and unrelated key releases preserve the live cumulative gesture and HUD");
    // An obsolete callback delivered before the current deadline cannot
    // hide a freshly updated readout or finalize its admitted baseline.
    QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
    check(view.stagedCleaveTransformHudText() == expected
            && sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory()),
        "an obsolete callback before the current deadline cannot clear or commit a Ctrl-held transform");
    check(releaseKey(view, Qt::Key_Control, Qt::ControlModifier),
        "the wheel-owned Control release is accepted instead of propagating to another input owner");
    const Draft settled = view.captureStagedCleaveDraftState();
    const auto settledHistory = view.captureStagedCleaveHistory();
    const auto settledRevision = view.stagedCleaveAuthoredRevision();
    check(view.stagedCleaveTransformHudText().isEmpty()
            && view.stagedCleaveTransformHudRectForTest().isEmpty()
            && !timer->isActive()
            && oneCommandAdded(historyBefore, settledHistory),
        "Control release immediately hides rotation and scale after committing exactly one command");
    QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
    check(view.stagedCleaveTransformHudText().isEmpty()
            && view.stagedCleaveTransformHudRectForTest().isEmpty()
            && sameDraft(settled, view.captureStagedCleaveDraftState())
            && sameHistory(settledHistory, view.captureStagedCleaveHistory())
            && view.stagedCleaveAuthoredRevision() == settledRevision,
        "a late HUD timeout after modifier release preserves the exact completed draft, history and dirty revision");
    check(view.undoStagedCleaveHistory()
            && sameDraft(before, view.captureStagedCleaveDraftState()),
        "one Undo restores the exact transform baseline after immediate modifier-release hiding");
}

void testFinishedTransformHudExpires(const Mode mode)
{
    auto state = document();
    GlView view{};
    configure(view, state, false, false);
    view.setStagedCleaveTransformMode(mode);
    const Draft before = view.captureStagedCleaveDraftState();
    const auto historyBefore = view.captureStagedCleaveHistory();
    const QString expected = mode == Mode::Rotate
        ? QStringLiteral("Rotate 10.00°") : QStringLiteral("Scale 1.21×");
    wheel(view, 120);
    wheel(view, 120);
    check(view.finishStagedCleaveWheelSession(),
        "explicit wheel completion admits a fallback readout deadline");
    QTimer* const timer = transformHudExpiryTimer(view);
    if ( !timer )
    {
        return;
    }
    const Draft settled = view.captureStagedCleaveDraftState();
    const auto settledHistory = view.captureStagedCleaveHistory();
    const auto settledRevision = view.stagedCleaveAuthoredRevision();
    check(timer->isSingleShot() && timer->isActive()
            && timer->interval() == kExpectedHudIdleIntervalMs
            && view.stagedCleaveTransformHudText() == expected
            && oneCommandAdded(historyBefore, settledHistory),
        "finished rotation and scale readouts use an exact 400 ms fallback");
    dispatchHudExpiry(view, *timer);
    check(view.stagedCleaveTransformHudText().isEmpty()
            && view.stagedCleaveTransformHudRectForTest().isEmpty()
            && sameDraft(settled, view.captureStagedCleaveDraftState())
            && sameHistory(settledHistory, view.captureStagedCleaveHistory())
            && view.stagedCleaveAuthoredRevision() == settledRevision,
        "fallback expiry hides only presentation while keeping exact draft, history and dirty revision");
    check(view.undoStagedCleaveHistory()
            && sameDraft(before, view.captureStagedCleaveDraftState())
            && view.redoStagedCleaveHistory()
            && sameDraft(settled, view.captureStagedCleaveDraftState()),
        "fallback expiry preserves one exact Undo and Redo of the completed transform");
}

void testCompletionRetainsLastSampleHudDeadline()
{
    for ( const bool focusLoss : {false, true} )
    {
        for ( const bool alreadyExpired : {false, true} )
        {
            auto state = document();
            GlView view{};
            configure(view, state, false, false);
            view.setStagedCleaveTransformMode(Mode::Scale);
            const Draft before = view.captureStagedCleaveDraftState();
            const auto historyBefore = view.captureStagedCleaveHistory();
            const auto revisionBefore = view.stagedCleaveAuthoredRevision();
            const auto completedCaptures = view.stagedCleaveHistoryCurrentCaptureCountForTest();
            wheel(view, 120);
            wheel(view, 120);
            QTimer* const timer = transformHudExpiryTimer(view);
            QTimer* const presentationTimer = transformHudPresentationTimer(view);
            if ( !timer || !presentationTimer )
            {
                return;
            }
            check(presentationTimer->isSingleShot() && presentationTimer->isActive()
                    && presentationTimer->interval() > 0
                    && presentationTimer->interval() < kExpectedHudIdleIntervalMs,
                "live Cleave samples schedule a single-shot fade wake before the hide deadline");
            const int interval = timer->interval();
            const int remainingDeadline = std::max(1, interval / 2);
            if ( alreadyExpired )
            {
                dispatchHudExpiry(view, *timer);
            }
            else
            {
                // Represent idle time already spent against the actual
                // single-shot timer, without a timing sleep or another timer.
                timer->start(remainingDeadline);
            }
            const int remainingBeforeCompletion = timer->remainingTime();
            const int presentationRemainingBeforeCompletion =
                presentationTimer->remainingTime();
            if ( focusLoss )
            {
                QFocusEvent focusOut(QEvent::FocusOut, Qt::OtherFocusReason);
                QApplication::sendEvent(&view, &focusOut);
            }
            else
            {
                check(view.finishStagedCleaveWheelSession(),
                    "the remaining-deadline fixture explicitly finishes its live wheel gesture");
            }
            const Draft settled = view.captureStagedCleaveDraftState();
            const auto settledHistory = view.captureStagedCleaveHistory();
            check(oneCommandAdded(historyBefore, settledHistory)
                    && view.stagedCleaveAuthoredRevision() == revisionBefore + 1u
                    && view.stagedCleaveHistoryCurrentCaptureCountForTest() == completedCaptures + 1u,
                "focus or explicit completion with an idle HUD records exactly one cumulative command");
            if ( alreadyExpired )
            {
                check(!timer->isActive() && view.stagedCleaveTransformHudText().isEmpty()
                        && view.stagedCleaveTransformHudRectForTest().isEmpty()
                        && !presentationTimer->isActive(),
                    "focus and explicit completion cannot resurrect an already expired transform readout");
            }
            else
            {
                check(timer->isActive() && timer->interval() == remainingDeadline
                        && timer->remainingTime() <= remainingBeforeCompletion
                        && presentationTimer->isActive()
                        && presentationTimer->remainingTime()
                            <= presentationRemainingBeforeCompletion
                        && view.stagedCleaveTransformHudText() == QStringLiteral("Scale 1.21×"),
                    "focus and explicit completion preserve both the remaining hide deadline and current fade wake without extending idle visibility");
                dispatchHudExpiry(view, *timer);
            }
            check(view.stagedCleaveTransformHudText().isEmpty()
                    && sameDraft(settled, view.captureStagedCleaveDraftState())
                    && sameHistory(settledHistory, view.captureStagedCleaveHistory())
                    && view.undoStagedCleaveHistory()
                    && sameDraft(before, view.captureStagedCleaveDraftState())
                    && view.redoStagedCleaveHistory()
                    && sameDraft(settled, view.captureStagedCleaveDraftState()),
                "completion at the original idle deadline retains the exact final draft/history and one Undo/Redo");
        }
    }
}

void testModifierReleaseCancelsPendingHudExpiry(const Mode mode)
{
    auto state = document();
    GlView view{};
    configure(view, state, false, false);
    view.setStagedCleaveTransformMode(mode);
    const Draft before = view.captureStagedCleaveDraftState();
    wheel(view, 120);
    check(view.finishStagedCleaveWheelSession(),
        "pending-release fixture finishes its wheel command before the modifier is released");
    QTimer* const timer = transformHudExpiryTimer(view);
    if ( !timer )
    {
        return;
    }
    const Draft settled = view.captureStagedCleaveDraftState();
    const auto settledHistory = view.captureStagedCleaveHistory();
    const auto settledRevision = view.stagedCleaveAuthoredRevision();
    check(timer->isActive() && !view.stagedCleaveTransformHudText().isEmpty(),
        "explicit completion retains a visible readout until release or fallback expiry");
    check(!releaseKey(view, Qt::Key_Control),
        "clearing a completed fallback does not own the editor's otherwise unrelated Control release");
    check(!timer->isActive() && view.stagedCleaveTransformHudText().isEmpty()
            && view.stagedCleaveTransformHudRectForTest().isEmpty()
            && sameDraft(settled, view.captureStagedCleaveDraftState())
            && sameHistory(settledHistory, view.captureStagedCleaveHistory())
            && view.stagedCleaveAuthoredRevision() == settledRevision
            && view.undoStagedCleaveHistory()
            && sameDraft(before, view.captureStagedCleaveDraftState()),
        "Control release cancels an already pending fallback immediately without adding another command");
}

void testNewTransformGestureCancelsOldHudExpiry()
{
    auto state = document();
    GlView view{};
    configure(view, state, false, false);
    view.setStagedCleaveTransformMode(Mode::Scale);
    wheel(view, 120);
    (void)view.finishStagedCleaveWheelSession();
    QTimer* const timer = transformHudExpiryTimer(view);
    if ( !timer )
    {
        return;
    }
    const Draft secondBaseline = view.captureStagedCleaveDraftState();
    const auto secondHistory = view.captureStagedCleaveHistory();
    check(timer->isActive(), "the completed first gesture has a pending HUD deadline");
    wheel(view, 120);
    wheel(view, 120);
    check(timer->isActive() && timer->interval() == kExpectedHudIdleIntervalMs
            && view.stagedCleaveTransformHudText() == QStringLiteral("Scale 1.21×"),
        "a new admitted gesture replaces the old deadline and measures from its new settled baseline");
    QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
    wheel(view, 120);
    check(view.stagedCleaveTransformHudText() == QStringLiteral("Scale 1.33×")
            && sameDraft(secondBaseline, view.captureStagedCleaveDraftState())
            && sameHistory(secondHistory, view.captureStagedCleaveHistory()),
        "a late previous deadline cannot clear the new active session or split its cumulative baseline");
    releaseControl(view);
    check(oneCommandAdded(secondHistory, view.captureStagedCleaveHistory())
            && !timer->isActive() && view.stagedCleaveTransformHudText().isEmpty()
            && view.undoStagedCleaveHistory()
            && sameDraft(secondBaseline, view.captureStagedCleaveDraftState()),
        "the new session retains exactly one Undo command after an obsolete deadline");
    check(!timer->isActive() && view.stagedCleaveTransformHudText().isEmpty(),
        "Undo clears a completed transform readout and its pending expiry");
}

void testHudFinishAndCancelBoundaries()
{
    for ( const int boundary : {0, 1, 2, 3, 4} )
    {
        auto state = document();
        GlView view{};
        configure(view, state, false, false);
        view.setStagedCleaveTransformMode(Mode::Rotate);
        const Draft before = view.captureStagedCleaveDraftState();
        const auto historyBefore = view.captureStagedCleaveHistory();
        wheel(view, 120);
        QTimer* const timer = transformHudExpiryTimer(view);
        if ( !timer )
        {
            return;
        }
        if ( boundary == 0 )
        {
            check(view.finishStagedCleaveWheelSession(),
                "explicit wheel finish completes the current gesture");
        }
        else if ( boundary == 1 )
        {
            QFocusEvent focusOut(QEvent::FocusOut, Qt::OtherFocusReason);
            QApplication::sendEvent(&view, &focusOut);
        }
        else if ( boundary == 2 )
        {
            view.setSectorCleaveToolEnabled(false);
        }
        else if ( boundary == 3 )
        {
            escape(view);
        }
        else
        {
            view.resetStagedCleaveWorkspaces();
        }
        if ( boundary <= 1 )
        {
            check(timer->isActive()
                    && timer->interval() == kExpectedHudIdleIntervalMs
                    && view.stagedCleaveTransformHudText() == QStringLiteral("Rotate 5.00°")
                    && oneCommandAdded(historyBefore, view.captureStagedCleaveHistory()),
                "explicit finish and focus loss retain the final readout for exactly 400 ms");
            dispatchHudExpiry(view, *timer);
            check(view.stagedCleaveTransformHudText().isEmpty()
                    && view.undoStagedCleaveHistory()
                    && sameDraft(before, view.captureStagedCleaveDraftState()),
                "focus and explicit completion expire cleanly while preserving exact Undo");
        }
        else
        {
            check(!timer->isActive() && view.stagedCleaveTransformHudText().isEmpty(),
                "tool deactivation, Escape and document reset clear the readout and cancel its timer");
            if ( boundary == 3 )
            {
                check(sameDraft(before, view.captureStagedCleaveDraftState())
                        && sameHistory(historyBefore, view.captureStagedCleaveHistory()),
                    "Escape keeps exact draft and history while cancelling the HUD deadline");
            }
        }
        releaseControl(view);
        check(!timer->isActive(),
            "late Control release cannot restart a finished or cancelled HUD timer");
    }
}

void testShapeWheelHudExpiry(const bool rotate)
{
    for ( const bool explicitFinish : {false, true} )
    {
        auto state = document();
        GlView view{};
        configure(view, state, false, false);
        auto options = view.cleaveShapeOptions();
        options.placementArmed = true;
        options.cameraPlane = true;
        view.setCleaveShapeOptions(options);
        const Draft before = view.captureStagedCleaveDraftState();
        const auto historyBefore = view.captureStagedCleaveHistory();
        const auto modifiers = rotate
            ? Qt::ControlModifier | Qt::ShiftModifier : Qt::ControlModifier;
        const QString expected = rotate
            ? QStringLiteral("Rotate 10.00°") : QStringLiteral("Scale 1.21×");
        wheel(view, 120, modifiers);
        QTimer* const timer = transformHudExpiryTimer(view);
        if ( !timer )
        {
            return;
        }
        (void)releaseKey(view, Qt::Key_Control, Qt::NoModifier, true);
        (void)releaseKey(view, Qt::Key_A, modifiers);
        dispatchHudExpiry(view, *timer);
        check(view.stagedCleaveTransformHudText().isEmpty()
                && view.stagedCleaveTransformHudRectForTest().isEmpty()
                && view.cleaveShapeOptions().rotationDegrees == (rotate
                    ? options.rotationDegrees + 5.0 : options.rotationDegrees)
                && close(view.cleaveShapeOptions().width,
                    rotate ? options.width : options.width * 1.1, 1.0e-14)
                && close(view.cleaveShapeOptions().height,
                    rotate ? options.height : options.height * 1.1, 1.0e-14)
                && view.cleaveShapeOptions().placementArmed
                && sameDraft(before, view.captureStagedCleaveDraftState())
                && sameHistory(historyBefore, view.captureStagedCleaveHistory()),
            "idle armed shape HUD expiry hides presentation while preserving the live options and exact draft/history");
        wheel(view, 120, modifiers);
        check(timer->isActive() && timer->interval() == kExpectedHudIdleIntervalMs
                && view.stagedCleaveTransformHudText() == expected,
            "armed shape wheel remains cumulative across idle expiry and unrelated releases while Control stays held");
        if ( explicitFinish )
        {
            check(view.finishStagedCleaveWheelSession(),
                "explicit completion settles the armed shape wheel options");
            check(timer->isActive() && timer->interval() == kExpectedHudIdleIntervalMs
                    && view.stagedCleaveTransformHudText() == expected,
                "finished armed shape rotation and scale use an exact 400 ms fallback deadline");
            dispatchHudExpiry(view, *timer);
        }
        else
        {
            check(releaseKey(view, Qt::Key_Control),
                "armed shape Control release remains owned by Cleave");
        }
        const auto settledOptions = view.cleaveShapeOptions();
        QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
        check(!timer->isActive() && view.stagedCleaveTransformHudText().isEmpty()
                && view.stagedCleaveTransformHudRectForTest().isEmpty()
                && settledOptions.rotationDegrees == (rotate
                    ? options.rotationDegrees + 10.0 : options.rotationDegrees)
                && close(settledOptions.width,
                    rotate ? options.width : options.width * 1.21, 1.0e-14)
                && close(settledOptions.height,
                    rotate ? options.height : options.height * 1.21, 1.0e-14)
                && view.cleaveShapeOptions().rotationDegrees == settledOptions.rotationDegrees
                && view.cleaveShapeOptions().width == settledOptions.width
                && view.cleaveShapeOptions().height == settledOptions.height
                && view.cleaveShapeOptions().placementArmed
                && sameDraft(before, view.captureStagedCleaveDraftState())
                && sameHistory(historyBefore, view.captureStagedCleaveHistory()),
            "armed shape release or fallback hiding preserves future options and exact line draft/history");
    }
}

void testPointerGestureCancelsOldHudExpiry()
{
    auto state = document();
    GlView view{};
    configure(view, state, false, false);
    view.setStagedCleaveTransformMode(Mode::Scale);
    wheel(view, 120);
    (void)view.finishStagedCleaveWheelSession();
    QTimer* const timer = transformHudExpiryTimer(view);
    if ( !timer )
    {
        return;
    }
    const Draft before = view.captureStagedCleaveDraftState();
    const auto historyBefore = view.captureStagedCleaveHistory();
    const auto body = view.projectWorldToViewport(vector(
        (before.lines[0].first + before.lines[0].second) * 0.5));
    check(body.has_value(), "HUD pointer fixture projects its actual selected line body");
    if ( !body )
    {
        return;
    }
    mouse(view, QEvent::MouseButtonPress, *body, Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseMove, *body + QPoint(50, 12), Qt::NoButton, Qt::LeftButton);
    const auto liveFirst = view.stagedCleaveLineFirstForTest(0);
    const QString liveHud = view.stagedCleaveTransformHudText();
    check(timer->isActive() && timer->interval() == kExpectedHudIdleIntervalMs
            && !liveHud.isEmpty()
            && liveFirst != vector(before.lines[0].first),
        "a pointer Scale preview replaces the old wheel deadline with its own presentation deadline");
    check(!releaseKey(view, Qt::Key_Control)
            && view.stagedCleaveLineFirstForTest(0) == liveFirst
            && view.stagedCleaveTransformHudText() == liveHud,
        "Control release does not consume or hide a pointer-only Scale preview");
    QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
    check(view.stagedCleaveLineFirstForTest(0) == liveFirst
            && view.stagedCleaveTransformHudText() == liveHud
            && sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory()),
        "a stale wheel deadline cannot clear or commit a live pointer Scale preview");
    escape(view);
    mouse(view, QEvent::MouseButtonRelease, *body + QPoint(50, 12), Qt::LeftButton);
    check(!timer->isActive() && view.stagedCleaveTransformHudText().isEmpty()
            && sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory()),
        "Escape and the late pointer release restore the exact draft/history without rearming expiry");
}

void selectSingleCleaveLine(GlView& view)
{
    auto draft = view.captureStagedCleaveDraftState();
    draft.selectedCleaveLines = {0};
    draft.selectedCleaveLine = 0;
    view.restoreStagedCleaveDraftState(draft);
}

void testCleaveHudUsesExplicitBannerKind()
{
    constexpr int kRotateDurationMs = 680;
    constexpr int kScaleDurationMs = 1450;
    for ( const bool armedShape : {false, true} )
    {
        for ( const bool rotate : {false, true} )
        {
            auto state = document();
            GlView view{};
            configure(view, state, false, false);
            selectSingleCleaveLine(view);

            const Mode toolbarMode = armedShape
                ? (rotate ? Mode::Scale : Mode::Rotate)
                : (rotate ? Mode::Rotate : Mode::Scale);
            view.setStagedCleaveTransformMode(toolbarMode);
            check(view.stagedCleaveTransformMode() == toolbarMode,
                "the banner fixture retains its selected toolbar transform mode");

            ViewportBannerSettings settings = view.viewportBannerSettings();
            settings.styles[static_cast<std::size_t>(ViewportBannerKind::cleaveRotate)]
                .hideAfterMs = kRotateDurationMs;
            settings.styles[static_cast<std::size_t>(ViewportBannerKind::cleaveScale)]
                .hideAfterMs = kScaleDurationMs;
            check(view.setViewportBannerSettings(settings),
                "distinct Cleave rotation and scale banner lifetimes are accepted");

            if ( armedShape )
            {
                auto options = view.cleaveShapeOptions();
                options.placementArmed = true;
                options.cameraPlane = true;
                view.setCleaveShapeOptions(options);
            }

            const Qt::KeyboardModifiers modifiers = armedShape && rotate
                ? Qt::ControlModifier | Qt::ShiftModifier
                : Qt::ControlModifier;
            check(wheel(view, 120, modifiers),
                "a Cleave transform sample is consumed by its active wheel gesture");
            QTimer* const timer = transformHudExpiryTimer(view);
            if ( !timer )
            {
                continue;
            }
            const int expectedDuration = rotate
                ? kRotateDurationMs : kScaleDurationMs;
            const QString expectedText = rotate
                ? QStringLiteral("Rotate 5.00°") : QStringLiteral("Scale 1.10×");
            check(timer->interval() == expectedDuration
                    && view.stagedCleaveTransformHudText() == expectedText
                    && view.stagedCleaveTransformMode() == toolbarMode,
                armedShape
                    ? "armed ShapeRotate and ShapeScale use their explicit banner kind without changing toolbar mode"
                    : "line Rotate and Scale use their distinct banner lifetimes");
            releaseControl(view);
        }
    }
}

void testViewportBannerSettingsCacheLifecycle()
{
    QTemporaryDir temporary{};
    check(temporary.isValid(), "banner cache fixture has an isolated settings directory");
    if ( !temporary.isValid() )
    {
        return;
    }

    const std::filesystem::path previousPath = appSettingsIniPath();
    struct RestoreSettingsPath
    {
        std::filesystem::path path{};
        ~RestoreSettingsPath() { setAppSettingsIniPathOverrideForTest(path); }
    } restore{previousPath};

    const std::filesystem::path settingsPath =
        std::filesystem::path(temporary.path().toStdWString()) / "banner-cache.ini";
    setAppSettingsIniPathOverrideForTest(settingsPath);
    ViewportBannerSettings persisted{};
    persisted.styles[static_cast<std::size_t>(ViewportBannerKind::placementYaw)]
        .hideAfterMs = 1234;
    persisted.styles[static_cast<std::size_t>(ViewportBannerKind::placementYaw)]
        .backgroundColor = QColor(31, 42, 53, 177);
    QSettings diskSettings = settingsStore();
    check(saveViewportBannerSettings(diskSettings, persisted),
        "the cache lifecycle fixture stores its baseline banner preferences");

    ViewportBannerSettings updated = persisted;
    updated.styles[static_cast<std::size_t>(ViewportBannerKind::cleaveRotate)]
        .hideAfterMs = 790;
    updated.styles[static_cast<std::size_t>(ViewportBannerKind::cleaveRotate)]
        .textColor = QColor(210, 80, 190, 220);
    {
        GlView view{};
        check(view.viewportBannerSettings() == persisted,
            "GlView loads banner preferences into its construction-time cache");

        int viewportPublications{};
        QObject::connect(&view, &GlView::viewportStateChanged, &view,
            [&]() { ++viewportPublications; });
        check(view.setViewportBannerSettings(updated)
                && view.viewportBannerSettings() == updated
                && viewportPublications == 0,
            "valid banner changes update the cached presentation without publishing viewport state");

        ViewportBannerSettings invalid = updated;
        invalid.styles[static_cast<std::size_t>(ViewportBannerKind::cleaveScale)]
            .hideAfterMs = 0;
        check(!view.setViewportBannerSettings(invalid)
                && view.viewportBannerSettings() == updated
                && viewportPublications == 0,
            "invalid banner settings are rejected atomically without viewport publication");
    }

    check(loadViewportBannerSettings(settingsStore()) == persisted,
        "changing a GlView cache does not persist banner preferences implicitly");
    GlView reopened{};
    check(reopened.viewportBannerSettings() == persisted,
        "a new GlView loads the last explicitly saved banner settings");
}

void testInactiveCleaveDoesNotOwnModifierRelease()
{
    auto state = document();
    GlView view{};
    configure(view, state, false, false);
    view.setStagedCleaveTransformMode(Mode::Scale);
    view.setSectorCleaveToolEnabled(false);
    const Draft before = view.captureStagedCleaveDraftState();
    const auto historyBefore = view.captureStagedCleaveHistory();
    const auto revisionBefore = view.stagedCleaveAuthoredRevision();
    wheel(view, 120);
    check(!releaseKey(view, Qt::Key_Control)
            && view.stagedCleaveTransformHudText().isEmpty()
            && sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory())
            && view.stagedCleaveAuthoredRevision() == revisionBefore,
        "inactive Cleave neither transforms its retained draft nor owns a Control release");
}

void testCumulativeRotation(const bool perspective, const bool surface, const double angleStep)
{
    auto state = document();
    GlView view{};
    configure(view, state, perspective, surface);
    view.setStagedCleaveTransformMode(Mode::Rotate);
    view.setCleaveAngleStepDegrees(angleStep);
    const Draft before = view.captureStagedCleaveDraftState();
    const auto historyBefore = view.captureStagedCleaveHistory();
    wheel(view, 120);
    wheel(view, 120);
    check(view.stagedCleaveTransformHudText() == (angleStep == 0.0
            ? QStringLiteral("Rotate 10.00°") : QStringLiteral("Rotate 60.00°")),
        "Ctrl-held Rotate wheel reports total signed rotation across detents");
    wheel(view, -60);
    const double degrees = angleStep == 0.0 ? 7.5 : 30.0;
    check(view.stagedCleaveTransformHudText() == (angleStep == 0.0
            ? QStringLiteral("Rotate 7.50°") : QStringLiteral("Rotate 30.00°")),
        "Free fractional rotation and configured whole-step rounding report the accumulated signed angle");
    check(sameHistory(historyBefore, view.captureStagedCleaveHistory()),
        "fractional and configured Rotate samples leave the settled history unfinished");
    check(sameDraft(before, view.captureStagedCleaveDraftState()),
        "fractional and configured Rotate samples keep the stable captured baseline exact");
    releaseControl(view);
    const Draft rotated = view.captureStagedCleaveDraftState();
    const TVector pivot{1.0 / 3.0, 0.0, -1.0 / 3.0};
    const double radians = degrees * 3.14159265358979323846 / 180.0;
    const QVector3D axis = vector(before.lines[0].constructionPlaneNormal).normalized();
    const QVector3D radial = vector(before.lines[0].first - pivot);
    const QVector3D expected = radial * static_cast<float>(std::cos(radians))
        + QVector3D::crossProduct(axis, radial) * static_cast<float>(std::sin(radians))
        + axis * QVector3D::dotProduct(axis, radial) * static_cast<float>(1.0 - std::cos(radians));
    check(oneCommandAdded(historyBefore, view.captureStagedCleaveHistory()),
        "one completed Rotate session records exactly one local command");
    check(closeQVec(vector(rotated.lines[0].first - pivot), expected, 1.0e-6f),
        "one completed Rotate session applies its displayed total angle to the selected line");
    check(closeVec(rotated.lines[2].first, rotated.lines[1].second, 0.0)
            && closeVec(rotated.lines[2].second, before.lines[2].second, 0.0),
        "completed Rotate carries the shared neighbor endpoint and pins its opposite endpoint exactly");
    check(view.undoStagedCleaveHistory(), "completed cumulative Rotate exposes one local Undo");
    check(sameDraft(before, view.captureStagedCleaveDraftState()),
        "one Undo restores the complete cumulative Rotate baseline exactly");
    check(view.redoStagedCleaveHistory(), "completed cumulative Rotate exposes its local Redo");
    check(sameDraft(rotated, view.captureStagedCleaveDraftState()),
        "one Redo restores the complete cumulative Rotate result exactly");
}

void testCancelAndIdentityPreserveRedo(const Mode mode)
{
    auto state = document();
    GlView view{};
    configure(view, state, false, false);
    view.setStagedCleaveTransformMode(mode);
    wheel(view, 120);
    releaseControl(view);
    check(view.undoStagedCleaveHistory(), "wheel cancellation fixture retains a real Redo branch");
    const Draft before = view.captureStagedCleaveDraftState();
    const auto historyBefore = view.captureStagedCleaveHistory();
    wheel(view, 120);
    wheel(view, 120);
    escape(view);
    releaseControl(view);
    check(sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory())
            && view.stagedCleaveTransformHudText().isEmpty(),
        "Escape cancels the entire wheel session with exact draft/history and existing Redo preserved");
    wheel(view, 120);
    wheel(view, -120);
    releaseControl(view);
    check(sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory()),
        "a wheel session returning to identity restores exact baseline and adds no command or Redo trim");
    wheel(view, 0);
    releaseControl(view);
    check(sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory()),
        "an empty wheel session leaves the completed history and baseline unchanged");
}

void testCompleteTurnsAndBoundedSessionWorkset()
{
    auto state = document();
    GlView view{};
    configure(view, state, false, false);
    view.setStagedCleaveTransformMode(Mode::Rotate);
    view.setCleaveAngleStepDegrees(0.0);
    const Draft before = view.captureStagedCleaveDraftState();
    const auto historyBefore = view.captureStagedCleaveHistory();
    const auto baselineCaptures = view.stagedCleaveBaselineCaptureCountForTest();
    const auto preparedWorksets = view.stagedCleaveTransformPreparationCountForTest();
    const auto completedCaptures = view.stagedCleaveHistoryCurrentCaptureCountForTest();
    wheel(view, 120);
    const std::size_t retainedBytes = view.stagedCleaveHistoryStorageBytesEstimate();
    for ( int sample = 1; sample < 75; ++sample )
    {
        wheel(view, 120);
    }
    check(view.stagedCleaveTransformHudText() == QStringLiteral("Rotate 375.00°")
            && sameHistory(historyBefore, view.captureStagedCleaveHistory())
            && view.stagedCleaveHistoryStorageBytesEstimate() == retainedBytes
            && view.stagedCleaveBaselineCaptureCountForTest() == baselineCaptures + 1u
            && view.stagedCleaveTransformPreparationCountForTest() == preparedWorksets + 1u
            && view.stagedCleaveHistoryCurrentCaptureCountForTest() == completedCaptures,
        "a long wheel gesture retains one bounded transform workset and displays true rotation beyond a full turn");
    releaseControl(view);
    check(oneCommandAdded(historyBefore, view.captureStagedCleaveHistory())
            && view.stagedCleaveHistoryCurrentCaptureCountForTest() == completedCaptures + 1u
            && view.undoStagedCleaveHistory()
            && sameDraft(before, view.captureStagedCleaveDraftState()),
        "seventy-five rotation samples still commit one exact local command");
    const auto redoHistory = view.captureStagedCleaveHistory();
    for ( int sample = 0; sample < 72; ++sample )
    {
        wheel(view, 120);
    }
    check(view.stagedCleaveTransformHudText() == QStringLiteral("Rotate 360.00°"),
        "the live readout retains a complete turn even when its final geometry is the baseline");
    releaseControl(view);
    check(sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(redoHistory, view.captureStagedCleaveHistory()),
        "a complete-turn wheel session restores exact authored frames without adding a command or trimming Redo");
}

void testScalarSessionBoundaries()
{
    using render::CleaveWheelOperation;
    render::CleaveWheelSession session{};
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    check(!session.active() && !session.accumulate(1.0)
            && session.angleDegrees() == 0.0 && session.scale() == 1.0,
        "an inactive scalar session rejects samples with its exact neutral outputs retained");
    check(session.begin(CleaveWheelOperation::LineScale, 0.0)
            && session.accumulate(1.0) && close(session.scale(), 1.1, 1.0e-15),
        "scalar Scale accepts a valid first detent with the established factor");
    const double before = session.scale();
    check(!session.begin(CleaveWheelOperation::None, 0.0)
            && !session.begin(CleaveWheelOperation::LineScale, nan)
            && !session.begin(CleaveWheelOperation::LineScale, 361.0)
            && !session.accumulate(infinity) && !session.accumulate(nan)
            && !session.accumulate(0.0) && !session.accumulate(1.0, -1.0, 2.0)
            && !session.accumulate(1.0, 2.0, 1.0)
            && session.operation() == CleaveWheelOperation::LineScale
            && session.scale() == before && session.angleDegrees() == 0.0,
        "invalid scalar session admission and samples preserve the complete prior operation and values");
    check(session.accumulate(-1.0) && session.scale() == 1.0 && session.identity(),
        "inverse scalar detents return to exact positive identity instead of accumulated multiply roundoff");
    check(session.begin(CleaveWheelOperation::ShapeScale, 0.0)
            && session.accumulate(1000.0, 0.5, 2.0) && session.scale() == 2.0
            && !session.accumulate(1.0, 0.5, 2.0)
            && session.accumulate(-1.0, 0.5, 2.0)
            && close(session.scale(), 2.0 / 1.1, 1.0e-14),
        "uniform shape limits clamp the cumulative value and allow an immediate inverse sample");
    check(session.begin(CleaveWheelOperation::LineRotate, 30.0)
            && session.accumulate(0.5) && session.angleDegrees() == 30.0
            && !session.accumulate(0.25) && session.angleDegrees() == 30.0
            && session.accumulate(-0.5) && session.angleDegrees() == 0.0 && session.identity(),
        "configured scalar angle steps round fractional samples individually and preserve exact inverse identity");
    check(session.begin(CleaveWheelOperation::ShapeRotate, 0.0)
            && session.accumulate(75.0) && session.angleDegrees() == 375.0
            && !session.accumulate(nan) && session.angleDegrees() == 375.0
            && session.transformsShape() && !session.transformsLines(),
        "Free scalar shape rotation retains true turns and rejects nonfinite samples without losing ownership");
    session.reset();
    check(!session.active() && session.operation() == CleaveWheelOperation::None
            && session.angleDegrees() == 0.0 && session.scale() == 1.0,
        "resetting a scalar wheel session clears ownership and restores exact neutral outputs");
}

void testOwnershipBoundaries()
{
    for ( const int boundary : {0, 1, 2, 3} )
    {
        auto state = document();
        GlView view{};
        configure(view, state, false, false);
        view.setStagedCleaveTransformMode(Mode::Scale);
        const Draft before = view.captureStagedCleaveDraftState();
        const auto historyBefore = view.captureStagedCleaveHistory();
        wheel(view, 120);
        wheel(view, 120);
        if ( boundary == 0 )
        {
            view.setSectorCleaveToolEnabled(false);
            view.setSectorCleaveToolEnabled(true);
        }
        else if ( boundary == 1 )
        {
            view.setCleaveStagedMode(false);
            view.setCleaveStagedMode(true);
        }
        else if ( boundary == 2 )
        {
            view.setRenderPipeline(GlView::RenderPipeline::GamePreview);
            view.setRenderPipeline(GlView::RenderPipeline::Editor);
        }
        else
        {
            view.setStagedCleaveTransformMode(Mode::Rotate);
        }
        const Draft changed = view.captureStagedCleaveDraftState();
        check(oneCommandAdded(historyBefore, view.captureStagedCleaveHistory())
                && !sameDraft(before, changed) && view.stagedCleaveTransformHudText().isEmpty()
                && view.undoStagedCleaveHistory()
                && sameDraft(before, view.captureStagedCleaveDraftState())
                && view.redoStagedCleaveHistory()
                && sameDraft(changed, view.captureStagedCleaveDraftState()),
            "Cleave tool, Multi-cut, Preview and mode transitions finalize one wheel command before ownership changes");
        releaseControl(view);
        check(view.captureStagedCleaveHistory()
                && view.captureStagedCleaveHistory()->snapshots.size() == historyBefore->cursor + 2u,
            "a late Control release cannot create a second command after ownership finalized the session");
    }

    for ( const auto subMode : {GlView::SurfaceSubMode::Edge, GlView::SurfaceSubMode::Vertex} )
    {
        auto state = document();
        GlView view{};
        configure(view, state, false, true);
        view.setStagedCleaveTransformMode(Mode::Scale);
        const Draft before = view.captureStagedCleaveDraftState();
        const auto historyBefore = view.captureStagedCleaveHistory();
        wheel(view, 120);
        wheel(view, 120);
        view.setSurfaceSubMode(subMode);
        const Draft settled = view.captureStagedCleaveDraftState();
        const auto completed = view.captureStagedCleaveHistory();
        check(view.surfaceSubMode() == subMode
                && oneCommandAdded(historyBefore, completed) && !sameDraft(before, settled),
            "leaving Surface Face mode finalizes the departing wheel when its Cleave ownership ends");
        releaseControl(view);
        view.setSurfaceSubMode(GlView::SurfaceSubMode::Face);
        check(sameHistory(completed, view.captureStagedCleaveHistory())
                && view.undoStagedCleaveHistory()
                && sameDraft(before, view.captureStagedCleaveDraftState()),
            "returning to Surface Face mode keeps one completed wheel command with exact Undo and no stale release");
    }
}

void testIndependentFamilySessions()
{
    auto state = document();
    GlView view{};
    configure(view, state, false, false);
    view.setStagedCleaveTransformMode(Mode::Scale);
    const auto sectorBefore = view.captureStagedCleaveWorkspace(Family::Sector);
    wheel(view, 120);
    wheel(view, 120);
    view.setSectorCleaveToolEnabled(false);
    editor::Selection surfaceSelection{};
    surfaceSelection.surfaces.insert(SurfaceRef{0, 0});
    view.setSelectionState(0, 0, -1, -1, surfaceSelection);
    view.setSurfaceCleaveToolEnabled(true);
    view.setCleaveStagedMode(true);
    view.captureStagedCleaveContextForTest(true);
    view.addStagedCleaveLineForTest({-0.8f, 0, -0.6f}, {0.8f, 0, -0.6f}, {0, 0, 1});
    const Draft surfaceSeed = view.captureStagedCleaveDraftState();
    view.restoreStagedCleaveDraftState(surfaceSeed);
    view.setStagedCleaveTransformMode(Mode::Rotate);
    const auto surfaceBefore = view.captureStagedCleaveWorkspace(Family::Surface);
    wheel(view, 120);
    wheel(view, 120);
    check(view.stagedCleaveTransformHudText() == QStringLiteral("Rotate 10.00°"),
        "Surface wheel session starts independently after a Sector session changes ownership");
    releaseControl(view);
    const auto surfaceAfter = view.captureStagedCleaveWorkspace(Family::Surface);
    const auto sectorAfter = view.captureStagedCleaveWorkspace(Family::Sector);
    check(oneCommandAdded(sectorBefore.history, sectorAfter.history)
            && oneCommandAdded(surfaceBefore.history, surfaceAfter.history),
        "Sector and Surface wheel gestures create exactly one command in each independent timeline");
    view.setSurfaceCleaveToolEnabled(false);
    view.setSelectionState(0, -1, -1, -1, state.selection);
    view.setSectorCleaveToolEnabled(true);
    check(sameDraft(sectorAfter.draft, view.captureStagedCleaveDraftState())
            && view.undoStagedCleaveHistory()
            && sameDraft(sectorBefore.draft, view.captureStagedCleaveDraftState())
            && sameDraft(surfaceAfter.draft, view.captureStagedCleaveWorkspace(Family::Surface).draft)
            && sameHistory(surfaceAfter.history, view.captureStagedCleaveWorkspace(Family::Surface).history),
        "returning to Sector restores its own session and Undo leaves the Surface draft/history exact");
}

void testShapeRotationReadout()
{
    auto state = document();
    GlView view{};
    configure(view, state, false, false);
    auto options = view.cleaveShapeOptions();
    options.kind = ui::viewport::CleaveShapeKind::Rectangle;
    options.dimensionsLinked = false;
    options.width = 1.6;
    options.height = 1.0;
    options.rotationDegrees = 170.0;
    options.placementArmed = true;
    options.cameraPlane = true;
    view.setCleaveShapeOptions(options);
    const Draft before = view.captureStagedCleaveDraftState();
    const auto historyBefore = view.captureStagedCleaveHistory();
    const float distance = view.cameraOrbitDistance();
    const float orthoScale = view.cameraOrthoScale();
    wheel(view, 120, Qt::ControlModifier | Qt::ShiftModifier);
    wheel(view, 120, Qt::ControlModifier | Qt::ShiftModifier);
    wheel(view, 120, Qt::ControlModifier | Qt::ShiftModifier);
    check(view.cleaveShapeOptions().rotationDegrees == -175.0
            && view.stagedCleaveTransformHudText() == QStringLiteral("Rotate 15.00°")
            && view.cleaveShapeOptions().width == options.width
            && view.cleaveShapeOptions().height == options.height
            && view.cameraOrbitDistance() == distance && view.cameraOrthoScale() == orthoScale
            && sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory()),
        "armed Ctrl+Shift wheel shows accumulated rotation across a complete-turn wrap without draft or camera edits");
    releaseControl(view);
    wheel(view, -120, Qt::ControlModifier | Qt::ShiftModifier);
    check(view.stagedCleaveTransformHudText() == QStringLiteral("Rotate -5.00°")
            && view.cleaveShapeOptions().rotationDegrees == -180.0,
        "a new armed-shape Ctrl+Shift session resets the signed readout while keeping absolute shape rotation");
    releaseControl(view);
    const auto settledOptions = view.cleaveShapeOptions();
    wheel(view, 120, Qt::ControlModifier | Qt::ShiftModifier);
    escape(view);
    releaseControl(view);
    check(view.cleaveShapeOptions().rotationDegrees == settledOptions.rotationDegrees
            && view.cleaveShapeOptions().width == settledOptions.width
            && view.cleaveShapeOptions().height == settledOptions.height
            && view.cleaveShapeOptions().placementArmed
            && view.stagedCleaveTransformHudText().isEmpty()
            && sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory()),
        "Escape restores armed-shape wheel options without disarming placement or retaining a stale release");
}

void testPresentationAndDocumentResetBoundaries()
{
    auto state = document();
    GlView view{};
    configure(view, state, false, false);
    view.setStagedCleaveTransformMode(Mode::Scale);
    const Draft before = view.captureStagedCleaveDraftState();
    const auto historyBefore = view.captureStagedCleaveHistory();
    const auto completedCaptures = view.stagedCleaveHistoryCurrentCaptureCountForTest();
    wheel(view, 120);
    view.setSelectionState(0, -1, -1, -1, state.selection);
    view.setLevel(&state.level, GlView::SetLevelMode::keepView);
    wheel(view, 120);
    check(view.stagedCleaveTransformHudText() == QStringLiteral("Scale 1.21×")
            && sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view.captureStagedCleaveHistory())
            && view.stagedCleaveHistoryCurrentCaptureCountForTest() == completedCaptures,
        "identical selection and current-Level presentation publication keep the cumulative wheel session live");
    releaseControl(view);
    check(oneCommandAdded(historyBefore, view.captureStagedCleaveHistory()),
        "a presentation refresh does not split the eventual wheel command");

    for ( const int boundary : {0, 1, 2} )
    {
        auto original = document();
        auto replacement = document();
        replacement.selection.clear();
        GlView resetView{};
        configure(resetView, original, false, false);
        resetView.setStagedCleaveTransformMode(Mode::Scale);
        wheel(resetView, 120);
        const auto finalCaptures = resetView.stagedCleaveHistoryCurrentCaptureCountForTest();
        if ( boundary == 0 )
        {
            resetView.setLevel(&replacement.level);
        }
        else if ( boundary == 1 )
        {
            resetView.setLevel(nullptr);
        }
        else
        {
            resetView.resetStagedCleaveWorkspaces();
        }
        const auto sector = resetView.captureStagedCleaveWorkspace(Family::Sector);
        const auto surface = resetView.captureStagedCleaveWorkspace(Family::Surface);
        check(sector.draft.lines.empty() && surface.draft.lines.empty()
                && sector.draft.selectedCleaveLines.empty()
                && surface.draft.selectedCleaveLines.empty()
                && sector.draft.selectedCleaveLine == -1
                && surface.draft.selectedCleaveLine == -1
                && !sector.history && !surface.history
                && resetView.stagedCleaveTransformHudText().isEmpty()
                && resetView.stagedCleaveHistoryCurrentCaptureCountForTest() == finalCaptures,
            "new/null Level and explicit document reset cancel the old wheel without capturing an authored command");
        releaseControl(resetView);
        check(!resetView.captureStagedCleaveHistory()
                && resetView.captureStagedCleaveDraftState().lines.empty()
                && resetView.stagedCleaveHistoryCurrentCaptureCountForTest() == finalCaptures,
            "late Control release after a document reset cannot reintroduce old lines, selection or history");
    }

    for ( const bool restoreWorkspace : {false, true} )
    {
        auto restoredState = document();
        GlView restoredView{};
        configure(restoredView, restoredState, false, false);
        restoredView.setStagedCleaveTransformMode(Mode::Scale);
        auto workspace = restoredView.captureStagedCleaveWorkspace(Family::Sector);
        const Draft restoredBefore = workspace.draft;
        const auto restoredHistory = workspace.history;
        const auto finalCaptures = restoredView.stagedCleaveHistoryCurrentCaptureCountForTest();
        wheel(restoredView, 120);
        if ( restoreWorkspace )
        {
            check(restoredView.restoreStagedCleaveWorkspace(Family::Sector, std::move(workspace)),
                "restoring a valid settled workspace replaces a live wheel preview");
        }
        else if ( restoredHistory )
        {
            restoredView.restoreStagedCleaveDraftState(restoredBefore, *restoredHistory);
        }
        releaseControl(restoredView);
        check(sameDraft(restoredBefore, restoredView.captureStagedCleaveDraftState())
                && sameHistory(restoredHistory, restoredView.captureStagedCleaveHistory())
                && restoredView.stagedCleaveHistoryCurrentCaptureCountForTest() == finalCaptures
                && restoredView.stagedCleaveTransformHudText().isEmpty(),
            "draft and workspace restore cancel the departing wheel and preserve exact installed state after late release");
    }
}

void testDraftCommandBoundaries()
{
    for ( const int boundary : {0, 1, 2} )
    {
        auto state = document();
        GlView view{};
        configure(view, state, false, false);
        view.setStagedCleaveTransformMode(Mode::Scale);
        const Draft before = view.captureStagedCleaveDraftState();
        const auto historyBefore = view.captureStagedCleaveHistory();
        wheel(view, 120);
        wheel(view, 120);
        if ( boundary == 2 )
        {
            std::optional<History> atApply{};
            Draft draftAtApply{};
            QObject::connect(&view, &GlView::sectorsCleaveBatchRequested, [&]()
            {
                atApply = view.captureStagedCleaveHistory();
                draftAtApply = view.captureStagedCleaveDraftState();
            });
            view.requestApplyStagedCleave();
            check(oneCommandAdded(historyBefore, atApply)
                    && !sameDraft(before, draftAtApply)
                    && sameDraft(draftAtApply, view.captureStagedCleaveDraftState()),
                "Apply publishes the completed wheel draft and its one local command before emitting the geometry batch");
            releaseControl(view);
            check(sameHistory(atApply, view.captureStagedCleaveHistory()),
                "Control release after Apply cannot append a second wheel command");
            continue;
        }
        if ( boundary == 0 )
        {
            view.clearStagedCleaveDraft();
        }
        else
        {
            check(view.deleteSelectedStagedCleaveLine(),
                "Delete succeeds after finalizing a live wheel session");
        }
        const auto completed = view.captureStagedCleaveHistory();
        check(historyBefore && completed
                && completed->cursor == historyBefore->cursor + 2u
                && completed->snapshots.size() == historyBefore->cursor + 3u
                && view.undoStagedCleaveHistory()
                && sameDraft(completed->snapshots[historyBefore->cursor + 1u],
                    view.captureStagedCleaveDraftState())
                && !sameDraft(before, view.captureStagedCleaveDraftState())
                && view.undoStagedCleaveHistory()
                && sameDraft(before, view.captureStagedCleaveDraftState()),
            "Clear and Delete each retain separate wheel and draft commands with exact successive Undo states");
        const auto undone = view.captureStagedCleaveHistory();
        releaseControl(view);
        check(sameHistory(undone, view.captureStagedCleaveHistory()),
            "a draft command and its Undo leave no stale Control-release owner");
    }

    auto state = document();
    GlView view{};
    configure(view, state, false, false);
    view.setStagedCleaveTransformMode(Mode::Scale);
    const Draft before = view.captureStagedCleaveDraftState();
    wheel(view, 120);
    releaseControl(view);
    check(view.undoStagedCleaveHistory(), "Undo seeds the live-wheel cancellation Redo branch");
    const auto redoHistory = view.captureStagedCleaveHistory();
    const auto finalCaptures = view.stagedCleaveHistoryCurrentCaptureCountForTest();
    wheel(view, 120);
    check(view.undoStagedCleaveHistory()
            && sameDraft(before, view.captureStagedCleaveDraftState())
            && sameHistory(redoHistory, view.captureStagedCleaveHistory()),
        "Undo during a wheel session cancels the exact preview baseline while preserving the existing Redo timeline");
    releaseControl(view);
    check(sameHistory(redoHistory, view.captureStagedCleaveHistory())
            && view.stagedCleaveHistoryCurrentCaptureCountForTest() == finalCaptures,
        "Control release after Undo cannot capture or commit the cancelled wheel");
}

void prepareWindow(MainWindow& window)
{
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.activateSectorEditModeForTest();
    window.setDocumentStateForTest(document());
    window.activateSectorEditModeForTest();
    GlView* const view = window.glViewForTest();
    check(view != nullptr, "wheel native fixture exposes its viewport");
    if ( !view )
    {
        return;
    }
    configure(*view, window.documentStateForTest(), false, false);
    window.updateEditActionsForTest();
    if ( QAction* const action = window.findChild<QAction*>(QStringLiteral("viewport.sector.cleave")) )
    {
        action->setChecked(true);
    }
    view->setSectorCleaveToolEnabled(true);
    view->setStagedCleaveTransformMode(Mode::Scale);
}

void testPointerReleaseOutsideViewport(const Mode mode)
{
    for ( const bool viewportReceivesRelease : {false, true} )
    {
        MainWindow window{};
        prepareWindow(window);
        GlView* const view = window.glViewForTest();
        if ( !view )
        {
            return;
        }
        view->setStagedCleaveTransformMode(mode);
        const Draft before = view->captureStagedCleaveDraftState();
        const auto historyBefore = view->captureStagedCleaveHistory();
        const auto revisionBefore = view->stagedCleaveAuthoredRevision();
        const auto completedCaptures = view->stagedCleaveHistoryCurrentCaptureCountForTest();
        const auto body = view->projectWorldToViewport(vector(
            (before.lines[0].first + before.lines[0].second) * 0.5));
        check(body.has_value(), "outside-release fixture projects the actual selected line body");
        if ( !body )
        {
            return;
        }
        mouse(*view, QEvent::MouseButtonPress, *body, Qt::LeftButton, Qt::LeftButton);
        mouse(*view, QEvent::MouseMove, *body + QPoint(50, 12), Qt::NoButton, Qt::LeftButton);
        QTimer* const timer = transformHudExpiryTimer(*view);
        if ( !timer )
        {
            return;
        }
        const auto endpoints = liveEndpoints(*view, before.lines.size());
        check(view->viewportMouseGestureButton() == Qt::LeftButton
                && !view->stagedCleaveTransformHudText().isEmpty()
                && endpoints.front().first != vector(before.lines[0].first),
            "outside-release fixture owns a real live Rotate or Scale pointer transform");
        if ( viewportReceivesRelease )
        {
            // Native mouse capture delivers an outside position to the
            // original viewport; its physical press owner still commits.
            const QPointF outside(-20, -20);
            check(!QRectF(view->rect()).contains(outside),
                "the captured pointer release position is outside the viewport");
            mouse(*view, QEvent::MouseButtonRelease, outside, Qt::LeftButton);
        }
        else
        {
            // A release actually delivered to another widget must pass
            // through the application's global capture-loss rollback path.
            QLineEdit differentReceiver(&window);
            mouse(differentReceiver, QEvent::MouseButtonRelease,
                differentReceiver.rect().center(), Qt::LeftButton);
        }
        check(!timer->isActive() && view->stagedCleaveTransformHudText().isEmpty()
                && view->stagedCleaveTransformHudRectForTest().isEmpty()
                && view->viewportMouseGestureButton() == Qt::NoButton,
            "actual pointer release outside the viewport immediately hides the HUD and cancels its deadline");
        if ( viewportReceivesRelease )
        {
            const Draft settled = view->captureStagedCleaveDraftState();
            check(liveEndpoints(*view, before.lines.size()) == endpoints
                    && oneCommandAdded(historyBefore, view->captureStagedCleaveHistory())
                    && view->stagedCleaveHistoryCurrentCaptureCountForTest() == completedCaptures + 1u
                    && view->undoStagedCleaveHistory()
                    && sameDraft(before, view->captureStagedCleaveDraftState())
                    && view->redoStagedCleaveHistory()
                    && sameDraft(settled, view->captureStagedCleaveDraftState()),
                "captured outside release commits the current pointer preview once and retains exact Undo and Redo");
        }
        else
        {
            check(sameDraft(before, view->captureStagedCleaveDraftState())
                    && sameHistory(historyBefore, view->captureStagedCleaveHistory())
                    && view->stagedCleaveAuthoredRevision() == revisionBefore
                    && view->stagedCleaveHistoryCurrentCaptureCountForTest() == completedCaptures,
                "foreign-widget pointer release rolls back the exact baseline without capturing a command");
        }
    }
}

void testGlobalReleaseAndStableNativeCapture(const std::filesystem::path& root)
{
    MainWindow window{};
    prepareWindow(window);
    GlView* const view = window.glViewForTest();
    if ( !view )
    {
        return;
    }
    // Ordinary QAction accelerators require an active window and keyboard
    // focus. Establish that ownership before admitting the live wheel.
    window.show();
    QApplication::setActiveWindow(&window);
    view->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    const Draft before = view->captureStagedCleaveDraftState();
    const auto historyBefore = view->captureStagedCleaveHistory();
    const std::size_t levelUndo = window.undoDepthForTest();
    const std::size_t levelRedo = window.redoDepthForTest();
    const auto finalCaptures = view->stagedCleaveHistoryCurrentCaptureCountForTest();
    const auto levelBefore = assets::saveToSedBytes(window.documentStateForTest().level);
    const std::filesystem::path nativePath = root / "held-control-wheel.sed";
    window.setCurrentPathForTitleForTest(nativePath);
    QAction* const save = window.findChild<QAction*>(QStringLiteral("file.save"));
    check(save != nullptr, "wheel native fixture exposes the production Save action");
    if ( !save )
    {
        return;
    }
    check(save->isEnabled(), "native Ctrl+S fixture enables the production Save action");
    wheel(*view, 120);
    wheel(*view, 120);
    const QByteArray captured = window.captureRecoveryEditorStateForTest();
    MainWindow capturedWindow{};
    capturedWindow.setAttribute(Qt::WA_DontShowOnScreen);
    capturedWindow.setDocumentStateForTest(document());
    check(capturedWindow.restoreRecoveryEditorStateForTest(captured)
            && capturedWindow.glViewForTest()
            && sameDraft(before, capturedWindow.glViewForTest()->captureStagedCleaveDraftState())
            && sameHistory(historyBefore, capturedWindow.glViewForTest()->captureStagedCleaveHistory()),
        "autosave/recovery capture during Ctrl wheel retains the exact pre-session settled draft and timeline");
    check(QApplication::activeWindow() == &window && QApplication::focusWidget() == view
            && save->isEnabled(),
        "native Ctrl+S is delivered with its owned window active and the viewport focused");
    saveWithoutModalErrors(*save, view);
    check(std::filesystem::is_regular_file(nativePath),
        "real global Ctrl+S writes the native file while a wheel gesture remains live");
    if ( !std::filesystem::is_regular_file(nativePath) )
    {
        escape(*view);
        return;
    }
    check(QApplication::activeWindow() == &window && QApplication::focusWidget() == view,
        "the non-interactive native Save progress keeps its original window active and viewport focused");
    check(!window.hasUnsavedChangesForTest(),
        "actual Ctrl+S marks the original document clean immediately while Control remains held");
    check(sameDraft(before, view->captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view->captureStagedCleaveHistory()),
        "actual Ctrl+S leaves the original wheel's stable captured draft and timeline at the pregesture baseline");
    check(view->stagedCleaveHistoryCurrentCaptureCountForTest() == finalCaptures
            && view->stagedCleaveLineFirstForTest(0) != vector(before.lines[0].first),
        "actual Ctrl+S keeps the changed live wheel geometry without capturing or finalizing a history command");
    QLineEdit differentReceiver(&window);
    releaseControl(differentReceiver);
    const Draft settled = view->captureStagedCleaveDraftState();
    const auto settledHistory = view->captureStagedCleaveHistory();
    QTimer* const timer = transformHudExpiryTimer(*view);
    check(oneCommandAdded(historyBefore, settledHistory)
            && !sameDraft(before, settled) && window.hasUnsavedChangesForTest()
            && view->stagedCleaveTransformHudText().isEmpty()
            && timer && !timer->isActive()
            && window.undoDepthForTest() == levelUndo && window.redoDepthForTest() == levelRedo
            && assets::saveToSedBytes(window.documentStateForTest().level) == levelBefore,
        "Control release received outside viewport immediately hides the HUD and finalizes one dirty local command without touching Level history");
    // Verify the file written during preview after releasing the original
    // gesture. Opening a second native window can pump ownership/focus events.
    MainWindow reopened{};
    reopened.setAttribute(Qt::WA_DontShowOnScreen);
    reopened.openLevelFromFilenameForTest(QString::fromStdWString(nativePath.wstring()));
    check(reopened.glViewForTest()
            && sameDraft(before, reopened.glViewForTest()->captureStagedCleaveDraftState()),
        "the native file written by live Ctrl+S reopens with the exact pregesture settled draft");
    check(reopened.glViewForTest()
            && sameHistory(historyBefore, reopened.glViewForTest()->captureStagedCleaveHistory()),
        "the native file written by live Ctrl+S reopens with the exact pregesture local timeline");
    saveWithoutModalErrors(*save);
    MainWindow settledReopened{};
    settledReopened.setAttribute(Qt::WA_DontShowOnScreen);
    settledReopened.openLevelFromFilenameForTest(QString::fromStdWString(nativePath.wstring()));
    GlView* const restored = settledReopened.glViewForTest();
    check(restored && sameDraft(settled, restored->captureStagedCleaveDraftState())
            && sameHistory(settledHistory, restored->captureStagedCleaveHistory())
            && restored->undoStagedCleaveHistory()
            && sameDraft(before, restored->captureStagedCleaveDraftState())
            && restored->redoStagedCleaveHistory()
            && sameDraft(settled, restored->captureStagedCleaveDraftState()),
        "native roundtrip keeps the cumulative wheel command and exact local Undo/Redo");
}

void testNativeDocumentReplacementCancelsWheel(const std::filesystem::path& root)
{
    MainWindow replacement{};
    replacement.setAttribute(Qt::WA_DontShowOnScreen);
    auto replacementState = document();
    replacementState.selection.clear();
    replacement.setDocumentStateForTest(std::move(replacementState));
    const auto replacementBytes = assets::saveToSedBytes(replacement.documentStateForTest().level);
    const std::filesystem::path path = root / "replacement-without-wheel.sed";
    replacement.setCurrentPathForTitleForTest(path);
    QAction* const save = replacement.findChild<QAction*>(QStringLiteral("file.save"));
    check(save != nullptr, "document replacement fixture exposes native Save");
    if ( !save )
    {
        return;
    }
    saveWithoutModalErrors(*save);
    check(std::filesystem::is_regular_file(path), "document replacement fixture writes its native file");
    if ( !std::filesystem::is_regular_file(path) )
    {
        return;
    }

    MainWindow window{};
    prepareWindow(window);
    GlView* const view = window.glViewForTest();
    if ( !view )
    {
        return;
    }
    window.markCurrentDocumentAsSavedForTest();
    wheel(*view, 120);
    wheel(*view, 120);
    const auto finalCaptures = view->stagedCleaveHistoryCurrentCaptureCountForTest();
    window.openLevelFromFilenameForTest(QString::fromStdWString(path.wstring()));
    const auto sector = view->captureStagedCleaveWorkspace(Family::Sector);
    const auto surface = view->captureStagedCleaveWorkspace(Family::Surface);
    check(window.currentPathForTest() == path && !window.hasUnsavedChangesForTest()
            && window.documentStateForTest().selection.empty()
            && assets::saveToSedBytes(window.documentStateForTest().level) == replacementBytes
            && sector.draft.lines.empty() && surface.draft.lines.empty()
            && sector.draft.selectedCleaveLines.empty() && surface.draft.selectedCleaveLines.empty()
            && !sector.history && !surface.history
            && view->stagedCleaveHistoryCurrentCaptureCountForTest() == finalCaptures,
        "native Open cancels the departing wheel without leaking its command, selection or dirty state into the replacement document");
    QLineEdit differentReceiver(&window);
    releaseControl(differentReceiver);
    check(!window.hasUnsavedChangesForTest() && !view->captureStagedCleaveHistory()
            && view->captureStagedCleaveDraftState().lines.empty()
            && view->stagedCleaveHistoryCurrentCaptureCountForTest() == finalCaptures,
        "a late global Control release after native Open leaves the replacement document clean and empty of old wheel history");
}

void testGlobalUndoCancelsFirstWheelSession()
{
    MainWindow window{};
    prepareWindow(window);
    GlView* const view = window.glViewForTest();
    if ( !view )
    {
        return;
    }
    window.markCurrentDocumentAsSavedForTest();
    const Draft before = view->captureStagedCleaveDraftState();
    const auto historyBefore = view->captureStagedCleaveHistory();
    const auto finalCaptures = view->stagedCleaveHistoryCurrentCaptureCountForTest();
    QAction* const undo = window.findChild<QAction*>(QStringLiteral("edit.undo"));
    check(undo && !undo->isEnabled(),
        "first wheel Undo fixture starts at a settled local baseline without an available command");
    wheel(*view, 120);
    check(undo && undo->isEnabled(),
        "the first accepted wheel sample exposes local Undo without committing an authored history snapshot");
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier);
    QApplication::sendEvent(view, &event);
    releaseControl(window);
    check(sameDraft(before, view->captureStagedCleaveDraftState())
            && sameHistory(historyBefore, view->captureStagedCleaveHistory())
            && view->stagedCleaveHistoryCurrentCaptureCountForTest() == finalCaptures
            && !window.hasUnsavedChangesForTest() && undo && !undo->isEnabled(),
        "real global Ctrl+Z cancels the first live wheel and leaves no authored command or stale release");
}

void testFocusOwnershipLoss()
{
    for ( const QEvent::Type eventType :
        {QEvent::FocusOut, QEvent::WindowDeactivate, QEvent::ApplicationDeactivate} )
    {
        MainWindow window{};
        prepareWindow(window);
        GlView* const view = window.glViewForTest();
        if ( !view )
        {
            continue;
        }
        const Draft before = view->captureStagedCleaveDraftState();
        const auto historyBefore = view->captureStagedCleaveHistory();
        wheel(*view, 120);
        wheel(*view, 120);
        if ( eventType == QEvent::FocusOut )
        {
            QFocusEvent event(QEvent::FocusOut, Qt::OtherFocusReason);
            QApplication::sendEvent(view, &event);
        }
        else
        {
            QEvent event(eventType);
            QApplication::sendEvent(&window, &event);
        }
        const Draft settled = view->captureStagedCleaveDraftState();
        check(oneCommandAdded(historyBefore, view->captureStagedCleaveHistory())
                && !sameDraft(before, settled)
                && view->undoStagedCleaveHistory()
                && sameDraft(before, view->captureStagedCleaveDraftState()),
            "focus/window/application ownership loss finalizes one cumulative wheel command with exact Undo");
        releaseControl(window);
        check(view->captureStagedCleaveHistory()
                && view->captureStagedCleaveHistory()->snapshots.size() == historyBefore->cursor + 2u,
            "Control release after focus loss cannot append a stale wheel command");
    }
}

void testFamilyFinalizationPublishesAuthoredDirtyState()
{
    for ( const bool throughModeAction : {false, true} )
    {
        MainWindow window{};
        prepareWindow(window);
        GlView* const view = window.glViewForTest();
        if ( !view )
        {
            continue;
        }
        window.markCurrentDocumentAsSavedForTest();
        const auto before = view->captureStagedCleaveWorkspace(Family::Sector);
        wheel(*view, 120);
        wheel(*view, 120);
        check(!window.hasUnsavedChangesForTest(),
            "live wheel preview keeps the saved authored projection at its settled baseline");
        if ( throughModeAction )
        {
            window.activateSurfaceEditModeForTest();
            window.activateSurfaceFaceSubModeForTest();
        }
        else
        {
            // This setter enters the family-exchange signal blocker directly.
            // It must publish the completed old family before that blocker.
            view->setSurfaceCleaveToolEnabled(true);
        }
        const auto after = view->captureStagedCleaveWorkspace(Family::Sector);
        check(oneCommandAdded(before.history, after.history)
                && !sameDraft(before.draft, after.draft)
                && window.hasUnsavedChangesForTest(),
            "direct family exchange and real edit-mode ownership changes publish the one completed authored dirty command");
        releaseControl(window);
        check(sameHistory(after.history, view->captureStagedCleaveWorkspace(Family::Sector).history),
            "Control release after a family transition preserves that family's exact completed timeline");
    }
}

} // namespace

void runCleaveWheelSessionTests()
{
    QTemporaryDir temporary{};
    check(temporary.isValid(), "wheel session fixtures isolate settings and native saves");
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
    const std::filesystem::path root(temporary.path().toStdWString());
    setAppSettingsIniPathOverrideForTest(root / "wheel-session.ini");
    testViewportBannerSettingsCacheLifecycle();
    testCleaveHudUsesExplicitBannerKind();
    for ( const bool perspective : {false, true} )
    {
        for ( const bool surface : {false, true} )
        {
            testCumulativeScale(perspective, surface);
            testCumulativeRotation(perspective, surface, 0.0);
            testCumulativeRotation(perspective, surface, 30.0);
        }
    }
    testCancelAndIdentityPreserveRedo(Mode::Scale);
    testCancelAndIdentityPreserveRedo(Mode::Rotate);
    testModifierReleaseHidesTransformHud(Mode::Scale);
    testModifierReleaseHidesTransformHud(Mode::Rotate);
    testRealIdleWheelHudDeadline();
    testUnchangedWheelSampleRestoresExpiredHud(false);
    testUnchangedWheelSampleRestoresExpiredHud(true);
    for ( const Mode mode : {Mode::Scale, Mode::Rotate} )
    {
        for ( const bool cancel : {false, true} )
        {
            testIdleWheelHudExpiry(mode, cancel);
            testIdlePointerHudExpiry(mode, false, cancel);
            testIdlePointerHudExpiry(mode, true, cancel);
        }
        testPointerReleaseOutsideViewport(mode);
    }
    testFinishedTransformHudExpires(Mode::Scale);
    testFinishedTransformHudExpires(Mode::Rotate);
    testCompletionRetainsLastSampleHudDeadline();
    testModifierReleaseCancelsPendingHudExpiry(Mode::Scale);
    testModifierReleaseCancelsPendingHudExpiry(Mode::Rotate);
    testNewTransformGestureCancelsOldHudExpiry();
    testHudFinishAndCancelBoundaries();
    testShapeWheelHudExpiry(false);
    testShapeWheelHudExpiry(true);
    testPointerGestureCancelsOldHudExpiry();
    testInactiveCleaveDoesNotOwnModifierRelease();
    testCompleteTurnsAndBoundedSessionWorkset();
    testScalarSessionBoundaries();
    testOwnershipBoundaries();
    testIndependentFamilySessions();
    testShapeRotationReadout();
    testPresentationAndDocumentResetBoundaries();
    testDraftCommandBoundaries();
    testGlobalReleaseAndStableNativeCapture(root);
    testNativeDocumentReplacementCancelsWheel(root);
    testGlobalUndoCancelsFirstWheelSession();
    testFocusOwnershipLoss();
    testFamilyFinalizationPublishesAuthoredDirtyState();
}

} // namespace sed::app_tests
