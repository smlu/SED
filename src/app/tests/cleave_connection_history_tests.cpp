#include "test_common.hpp"

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/level/build.hpp"
#include "ui/navigation/outliner_types.hpp"
#include "ui/components/scrub_float_edit.hpp"

#include <QAction>
#include <QApplication>
#include <QEventLoop>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMouseEvent>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sed::app_tests
{
namespace
{

QVector3D toQVector3D(const TVector &point)
{
    return QVector3D(static_cast<float>(point.x), static_cast<float>(point.y),
                     static_cast<float>(point.z));
}

bool samePoint(const TVector &left, const TVector &right)
{
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

bool sameStagedLines(const std::vector<GlView::RecoveryState::CleaveLine> &left,
                     const std::vector<GlView::RecoveryState::CleaveLine> &right)
{
    if ( left.size() != right.size() )
    {
        return false;
    }

    for ( std::size_t index = 0; index < left.size(); ++index )
    {
        const auto &a = left[index];
        const auto &b = right[index];
        if ( !samePoint(a.first, b.first) || !samePoint(a.second, b.second) ||
             !samePoint(a.planeNormal, b.planeNormal) ||
             !samePoint(a.constructionPlaneNormal, b.constructionPlaneNormal) ||
             !samePoint(a.angleReferenceAxis, b.angleReferenceAxis) ||
             a.linkedToPrevious != b.linkedToPrevious || a.linkedToNext != b.linkedToNext ||
             a.authoredCamera.valid != b.authoredCamera.valid ||
             a.authoredCamera.projectionType != b.authoredCamera.projectionType ||
             a.authoredCamera.orbit.center != b.authoredCamera.orbit.center ||
             a.authoredCamera.orbit.pan != b.authoredCamera.orbit.pan ||
             a.authoredCamera.orbit.radius != b.authoredCamera.orbit.radius ||
             a.authoredCamera.orbit.distance != b.authoredCamera.orbit.distance ||
             a.authoredCamera.orbit.yawDeg != b.authoredCamera.orbit.yawDeg ||
             a.authoredCamera.orbit.pitchDeg != b.authoredCamera.orbit.pitchDeg ||
             a.authoredCamera.orbit.rollDeg != b.authoredCamera.orbit.rollDeg ||
             a.authoredCamera.fovXDeg != b.authoredCamera.fovXDeg ||
             a.authoredCamera.orthoScale != b.authoredCamera.orthoScale ||
             a.authoredCamera.nearPlane != b.authoredCamera.nearPlane ||
             a.authoredCamera.farPlane != b.authoredCamera.farPlane ||
             a.authoredCamera.clipEnabled != b.authoredCamera.clipEnabled )
        {
            return false;
        }
    }
    return true;
}

bool sameMatrix(const QMatrix4x4 &left, const QMatrix4x4 &right)
{
    const float *const leftData = left.constData();
    const float *const rightData = right.constData();
    for ( std::size_t index = 0; index < 16; ++index )
    {
        if ( leftData[index] != rightData[index] )
        {
            return false;
        }
    }
    return true;
}

bool sameCleaveContext(const GlView::RecoveryState::CleaveContext &left,
                       const GlView::RecoveryState::CleaveContext &right)
{
    return left.valid == right.valid && left.surfaceMode == right.surfaceMode &&
           sameMatrix(left.projection, right.projection) && sameMatrix(left.view, right.view) &&
           left.viewportWidth == right.viewportWidth &&
           left.viewportHeight == right.viewportHeight &&
           left.sourceSurface.sector == right.sourceSurface.sector &&
           left.sourceSurface.surface == right.sourceSurface.surface &&
           left.targetSectors == right.targetSectors &&
           left.targetSurfaces.size() == right.targetSurfaces.size() &&
           std::equal(left.targetSurfaces.begin(), left.targetSurfaces.end(),
                      right.targetSurfaces.begin(),
                      [](const SurfaceRef &a, const SurfaceRef &b)
                      {
                          return a.sector == b.sector && a.surface == b.surface;
                      });
}

bool sameCleaveDraft(const GlView::RecoveryState &left, const GlView::RecoveryState &right)
{
    const bool sameOrigin =
        left.cleaveChainOrigin.has_value() == right.cleaveChainOrigin.has_value() &&
        (!left.cleaveChainOrigin ||
         (left.cleaveChainOrigin->lineIndex == right.cleaveChainOrigin->lineIndex &&
          left.cleaveChainOrigin->endpoint == right.cleaveChainOrigin->endpoint));
    return sameStagedLines(left.cleaveLines, right.cleaveLines) &&
           sameCleaveContext(left.cleaveContext, right.cleaveContext) && sameOrigin &&
           left.applyOrder == right.applyOrder;
}

bool sameCleaveOptions(
    const GlView::RecoveryState &left,
    const GlView::RecoveryState &right)
{
    const auto &a = left.cleaveShapeOptions;
    const auto &b = right.cleaveShapeOptions;
    return left.cleaveBounded == right.cleaveBounded &&
           left.cleaveStagedMode == right.cleaveStagedMode &&
           left.cleaveChainEnabled == right.cleaveChainEnabled &&
           left.cleaveMeasurementsVisible == right.cleaveMeasurementsVisible &&
           left.cleaveMeasureUnit == right.cleaveMeasureUnit &&
           left.cleaveAnglesVisible == right.cleaveAnglesVisible &&
           left.cleaveLengthsVisible == right.cleaveLengthsVisible &&
           left.cleaveLineNumbersVisible == right.cleaveLineNumbersVisible &&
           left.cleaveAngleStepDegrees == right.cleaveAngleStepDegrees &&
           a.kind == b.kind && a.width == b.width && a.height == b.height &&
           a.dimensionsLinked == b.dimensionsLinked && a.segments == b.segments &&
           a.points == b.points && a.placementArmed == b.placementArmed;
}

bool sameCleaveSnapshot(
    const GlView::RecoveryState::CleaveDraftSnapshot &left,
    const GlView::RecoveryState::CleaveDraftSnapshot &right)
{
    const bool sameOrigin = left.chainOrigin.has_value() == right.chainOrigin.has_value() &&
        (!left.chainOrigin ||
         (left.chainOrigin->lineIndex == right.chainOrigin->lineIndex &&
          left.chainOrigin->endpoint == right.chainOrigin->endpoint));
    return sameStagedLines(left.lines, right.lines) &&
           sameCleaveContext(left.context, right.context) && sameOrigin &&
           left.applyOrder == right.applyOrder &&
           left.selectedCleaveLines == right.selectedCleaveLines &&
           left.selectedCleaveLine == right.selectedCleaveLine;
}

bool sameCleaveHistory(
    const std::optional<GlView::RecoveryState::CleaveHistory> &left,
    const std::optional<GlView::RecoveryState::CleaveHistory> &right)
{
    if ( left.has_value() != right.has_value() )
    {
        return false;
    }
    if ( !left )
    {
        return true;
    }
    if ( left->cursor != right->cursor ||
         left->snapshots.size() != right->snapshots.size() )
    {
        return false;
    }
    for ( std::size_t index = 0; index < left->snapshots.size(); ++index )
    {
        if ( !sameCleaveSnapshot(left->snapshots[index], right->snapshots[index]) )
        {
            return false;
        }
    }
    return true;
}

void sendMouseEvent(QWidget &target, const QEvent::Type type, const QPointF &position,
                    const Qt::MouseButton button, const Qt::MouseButtons buttons,
                    const Qt::KeyboardModifiers modifiers)
{
    const QPointF globalPosition = target.mapToGlobal(position.toPoint());
    QMouseEvent event(type, position, globalPosition, globalPosition, button, buttons, modifiers);
    QApplication::sendEvent(&target, &event);
}

void sendDrag(QWidget &target, const QPointF &start, const std::vector<QPointF> &moves,
              const Qt::KeyboardModifiers modifiers)
{
    sendMouseEvent(target, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton,
                   modifiers);
    for ( const QPointF &position : moves )
    {
        sendMouseEvent(target, QEvent::MouseMove, position, Qt::NoButton, Qt::LeftButton,
                       modifiers);
    }
    if ( !moves.empty() )
    {
        sendMouseEvent(target, QEvent::MouseButtonRelease, moves.back(), Qt::LeftButton,
                       Qt::NoButton, modifiers);
    }
    QApplication::processEvents(QEventLoop::AllEvents);
}

void sendClick(QWidget &target, const QPointF &position,
               const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    sendMouseEvent(target, QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::LeftButton,
                   modifiers);
    sendMouseEvent(target, QEvent::MouseButtonRelease, position, Qt::LeftButton, Qt::NoButton,
                   modifiers);
    QApplication::processEvents(QEventLoop::AllEvents);
}

void sendKey(QWidget &target, const QEvent::Type type, const int key,
             const Qt::KeyboardModifiers modifiers, const QString &text = {})
{
    QKeyEvent event(type, key, modifiers, text, false, 1);
    QApplication::sendEvent(&target, &event);
}

void configureQuietView(GlView &view)
{
    view.setShowGrid(false);
    view.setShowSolid(false);
    view.setShowTextures(false);
    view.setShowWireframe(false);
    view.setShowVertices(false);
    view.setShowFog(false);
    view.setShowThings(false);
    view.setShowAxisWidget(false);
    view.setShowToastOverlays(false);
}

void makeTestCube(Level &cubeLevel)
{
    cubeLevel.layers.emplace_back("Layer0");
    level::levelCreateCube(cubeLevel, 0.0, 0.0, 0.0, TVector::up, TVector::right).layer = 0;
}

std::optional<QPointF> project(GlView &view, const QVector3D &point)
{
    return view.projectWorldToViewport(point);
}

} // namespace

void runCleaveConnectionHistoryTests()
{
    {
        Level level{};
        makeTestCube(level);
        editor::Selection targets{};
        targets.sectors.insert(0);

        GlView view{};
        view.setAttribute(Qt::WA_DontShowOnScreen);
        view.resize(640, 480);
        view.setLevel(&level);
        view.setSelectionState(0, -1, -1, -1, targets);
        view.setCleaveStagedMode(true);
        view.setCleaveChainEnabled(true);
        view.setSectorCleaveToolEnabled(true);
        view.setCameraProjectionType(sed::app::ViewportCamera::ProjectionType::Perspective);
        view.setOrbitViewImmediate(31.0f, -23.0f, 0.0f);
        view.setCameraOrbitDistance(8.0f);
        configureQuietView(view);
        view.show();
        QApplication::processEvents(QEventLoop::AllEvents);
        view.captureStagedCleaveContextForTest(false);

        QVector3D right = view.cameraRight();
        QVector3D up = view.cameraUp();
        QVector3D forward = view.cameraForward();
        right.normalize();
        up.normalize();
        forward.normalize();
        const QVector3D center = view.cameraTarget();
        const QVector3D firstLineFirst = center - right * 0.60f - up * 0.20f;
        const QVector3D firstLineSecond = center - right * 0.05f - up * 0.20f;
        const QVector3D chainedFirst = center - right * 0.35f + up * 0.42f;
        const QVector3D chainedJoint = center + right * 0.25f + up * 0.42f;
        const QVector3D chainedTerminal = center + right * 0.66f + up * 0.12f;

        // Line 0 was authored independently. Lines 1 and 2 already form a
        // canonical adjacent chain, so joining line 0 to line 2's terminal
        // exercises a real mixed independent/chained endpoint group.
        view.addStagedCleaveLineForTest(firstLineFirst, firstLineSecond, forward);
        view.addStagedCleaveLineForTest(chainedFirst, chainedJoint, forward, false, true);
        view.addStagedCleaveLineForTest(chainedJoint, chainedTerminal, forward, true, false);
        view.selectStagedCleaveLineForTest(0);

        const auto movingEndpoint = project(view, firstLineSecond);
        const auto chainEndpoint = project(view, chainedTerminal);
        check(movingEndpoint && chainEndpoint,
              "mixed Multi-cut join fixture projects the independent and chained endpoints");
        if ( !movingEndpoint || !chainEndpoint )
        {
            return;
        }

        sendDrag(view, *movingEndpoint, {*chainEndpoint}, Qt::ShiftModifier);
        const auto afterJoin = view.recoveryState();
        check(afterJoin.cleaveLines.size() == 3 &&
                  (toQVector3D(afterJoin.cleaveLines[0].second) - chainedTerminal).lengthSquared() <
                      1.0e-8f &&
                  (toQVector3D(afterJoin.cleaveLines[2].second) - chainedTerminal).lengthSquared() <
                      1.0e-8f &&
                  afterJoin.cleaveLines[0].linkedToNext == false &&
                  afterJoin.cleaveLines[2].linkedToPrevious &&
                  afterJoin.cleaveLines[1].linkedToNext,
              "Shift-drag joins an independent line to any existing endpoint without changing "
              "authored order or chain topology");
        if ( afterJoin.cleaveLines.size() != 3 )
        {
            return;
        }

        const TVector chainFirstBeforeMove = afterJoin.cleaveLines[2].first;
        const TVector firstLineFirstBeforeMove = afterJoin.cleaveLines[0].first;
        const QVector3D lineBody = (toQVector3D(afterJoin.cleaveLines[0].first) +
                                    toQVector3D(afterJoin.cleaveLines[0].second)) *
                                   0.5f;
        const auto bodyStart = project(view, lineBody);
        std::optional<QPointF> bodyEnd{};
        if ( bodyStart )
        {
            bodyEnd = *bodyStart + QPointF(0.0, -24.0);
        }
        check(bodyStart && bodyEnd,
              "mixed Multi-cut join fixture projects a body drag away from both handles");
        if ( !bodyStart || !bodyEnd )
        {
            return;
        }

        sendDrag(view, *bodyStart, {*bodyEnd}, Qt::NoModifier);
        const auto afterBodyDrag = view.recoveryState();
        bool pickedLineMovedRigidly = false;
        bool carriedJoinedEndpoint = false;
        bool collateralEndpointStayedPinned = false;
        if ( afterBodyDrag.cleaveLines.size() == 3 )
        {
            const QVector3D pickedLineTranslation =
                toQVector3D(afterBodyDrag.cleaveLines[0].first) -
                toQVector3D(afterJoin.cleaveLines[0].first);
            const QVector3D movedPickedSecond =
                toQVector3D(afterBodyDrag.cleaveLines[0].second) -
                toQVector3D(afterJoin.cleaveLines[0].second);
            pickedLineMovedRigidly =
                (pickedLineTranslation - movedPickedSecond).lengthSquared() < 1.0e-8f &&
                !samePoint(afterBodyDrag.cleaveLines[0].first, firstLineFirstBeforeMove);
            carriedJoinedEndpoint =
                (toQVector3D(afterBodyDrag.cleaveLines[0].second) -
                 toQVector3D(afterBodyDrag.cleaveLines[2].second))
                        .lengthSquared() < 1.0e-8f;
            collateralEndpointStayedPinned =
                samePoint(afterBodyDrag.cleaveLines[2].first, chainFirstBeforeMove);
        }
        check(afterBodyDrag.cleaveLines.size() == 3 && carriedJoinedEndpoint &&
                  pickedLineMovedRigidly && collateralEndpointStayedPinned,
              "whole-line translation carries the joined endpoint into the chained line while "
              "preserving picked-line rigidity and pinning collateral opposite endpoints");

        auto directView = std::make_unique<GlView>();
        const QVector3D zeroLineFirst(-1.0f, 0.0f, 0.0f);
        const QVector3D sharedPoint(0.0f, 0.0f, 0.0f);
        const QVector3D zeroLineNeighborSecond(1.0f, 0.0f, 0.0f);
        directView->addStagedCleaveLineForTest(zeroLineFirst, sharedPoint,
                                               QVector3D(0.0f, 0.0f, 1.0f));
        directView->addStagedCleaveLineForTest(sharedPoint, zeroLineNeighborSecond,
                                               QVector3D(0.0f, 0.0f, 1.0f));
        const auto beforeRejectedTranslation = directView->recoveryState();
        const bool rejectedCollapse =
            directView->translateStagedCleavePlaneForTest(0, QVector3D(1.0f, 0.0f, 0.0f));
        const auto afterRejectedTranslation = directView->recoveryState();
        check(!rejectedCollapse && sameStagedLines(beforeRejectedTranslation.cleaveLines,
                                                   afterRejectedTranslation.cleaveLines),
              "Multi-cut joined-line translation rejects a zero-length collateral segment "
              "atomically");

        const QVector3D normalDragPoint(0.4f, 0.5f, 0.0f);
        const auto beforeNormalEndpoint = view.recoveryState();
        const bool normalEndpointMoved =
            view.reshapeStagedCleaveEndpointForTest(0, 1, normalDragPoint);
        const auto afterNormalEndpoint = view.recoveryState();
        check(normalEndpointMoved && afterNormalEndpoint.cleaveLines.size() == 3 &&
                  closeQVec(toQVector3D(afterNormalEndpoint.cleaveLines[0].second), normalDragPoint,
                            1.0e-5f) &&
                  closeQVec(toQVector3D(afterNormalEndpoint.cleaveLines[2].second), normalDragPoint,
                            1.0e-5f),
              "ordinary endpoint drag moves every member of a spatially joined group");

        const QVector3D detachedPoint(-0.2f, -0.4f, 0.1f);
        const TVector neighborBeforeDetach = afterNormalEndpoint.cleaveLines[2].second;
        const bool altDetached =
            view.reshapeDisjointedStagedCleaveEndpointForTest(0, 1, detachedPoint);
        const auto afterDetach = view.recoveryState();
        check(
            altDetached &&
                closeQVec(toQVector3D(afterDetach.cleaveLines[0].second), detachedPoint, 1.0e-5f) &&
                samePoint(afterDetach.cleaveLines[2].second, neighborBeforeDetach) &&
                !afterDetach.cleaveLines[0].linkedToNext &&
                !afterDetach.cleaveLines[1].linkedToPrevious,
            "Alt endpoint detach moves only the picked endpoint and preserves unrelated chain "
            "links");

        view.setCleaveChainEnabled(false);
        const QVector3D independentTarget = toQVector3D(afterDetach.cleaveLines[0].first);
        const QVector3D unchainedFirst = independentTarget + right * 0.20f;
        const QVector3D unchainedSecond = independentTarget + right * 1.20f;
        view.addStagedCleaveLineForTest(unchainedFirst, unchainedSecond, forward);
        const auto unchainedStart = project(view, unchainedFirst);
        const auto independentTargetPoint = project(view, independentTarget);
        check(unchainedStart && independentTargetPoint,
              "Chain-off mixed-line fixture projects its real Shift join endpoints");
        if ( unchainedStart && independentTargetPoint )
        {
            sendDrag(view, *unchainedStart, {*independentTargetPoint}, Qt::ShiftModifier);
            const auto afterUnchainedJoin = view.recoveryState();
            const bool joinedWithoutChain = afterUnchainedJoin.cleaveLines.size() == 4
                && samePoint(afterUnchainedJoin.cleaveLines[0].first,
                             afterUnchainedJoin.cleaveLines[3].first)
                && !afterUnchainedJoin.cleaveLines[0].linkedToPrevious
                && !afterUnchainedJoin.cleaveLines[3].linkedToPrevious;
            const TVector neighborSecondBeforeMove = afterUnchainedJoin.cleaveLines[0].second;
            const QVector3D unchainedBody =
                (toQVector3D(afterUnchainedJoin.cleaveLines[3].first)
                 + toQVector3D(afterUnchainedJoin.cleaveLines[3].second)) * 0.5f;
            const auto unchainedBodyStart = project(view, unchainedBody);
            check(joinedWithoutChain && unchainedBodyStart,
                  "Chain-off Shift snap joins an independent endpoint without authoring a chain");
            if ( joinedWithoutChain && unchainedBodyStart )
            {
                const QVector3D firstBeforeMove =
                    toQVector3D(afterUnchainedJoin.cleaveLines[3].first);
                const QVector3D secondBeforeMove =
                    toQVector3D(afterUnchainedJoin.cleaveLines[3].second);
                const QPointF bodyEnd = *unchainedBodyStart + QPointF(0.0, -24.0);
                sendDrag(view, *unchainedBodyStart, {bodyEnd}, Qt::NoModifier);
                const auto afterUnchainedBodyMove = view.recoveryState();
                bool movedRigidly = false;
                if ( afterUnchainedBodyMove.cleaveLines.size() == 4 )
                {
                    const QVector3D movedFirst =
                        toQVector3D(afterUnchainedBodyMove.cleaveLines[3].first);
                    const QVector3D movedSecond =
                        toQVector3D(afterUnchainedBodyMove.cleaveLines[3].second);
                    const QVector3D pickedTranslation = movedFirst - firstBeforeMove;
                    const QVector3D secondTranslation = movedSecond - secondBeforeMove;
                    movedRigidly = closeQVec(pickedTranslation, secondTranslation, 1.0e-5f)
                        && !closeQVec(pickedTranslation, QVector3D{}, 1.0e-5f);
                }
                check(afterUnchainedBodyMove.cleaveLines.size() == 4
                          && samePoint(afterUnchainedBodyMove.cleaveLines[0].first,
                                       afterUnchainedBodyMove.cleaveLines[3].first)
                          && samePoint(afterUnchainedBodyMove.cleaveLines[0].second,
                                       neighborSecondBeforeMove)
                          && movedRigidly,
                      "Chain-off whole-line movement carries the endpoint group and preserves "
                      "both authored orders");
            }
        }

        const auto expectedDraftAfterLocalRedo = view.recoveryState();
        const bool canCreatePersistedRedoBranch = view.canUndoStagedCleaveHistory();
        check(canCreatePersistedRedoBranch && view.undoStagedCleaveHistory() &&
                  view.canRedoStagedCleaveHistory(),
              "native draft history fixture creates a real persisted Redo branch");
        const auto savedDraft = view.recoveryState();
        GlView recoveredView{};
        recoveredView.setLevel(&level);
        recoveredView.setSelectionState(0, -1, -1, -1, targets);
        recoveredView.restoreRecoveryState(savedDraft, false);
        const auto restoredDraft = recoveredView.recoveryState();
        check(sameCleaveDraft(restoredDraft, savedDraft) &&
                  sameCleaveHistory(restoredDraft.cleaveHistory, savedDraft.cleaveHistory) &&
                  recoveredView.stagedCleaveHistoryOwnsUndo() &&
                  recoveredView.canUndoStagedCleaveHistory() ==
                      view.canUndoStagedCleaveHistory() &&
                  recoveredView.canRedoStagedCleaveHistory() &&
                  recoveredView.redoStagedCleaveHistory() &&
                  sameCleaveDraft(recoveredView.recoveryState(), expectedDraftAfterLocalRedo),
              "restored native Multi-cut draft retains its bounded cursor and Redo branch");
        recoveredView.cancelStagedCleave();
        check(recoveredView.stagedCleaveLineCount() == 0
                  && !recoveredView.stagedCleaveHistoryOwnsUndo()
                  && !recoveredView.canRedoStagedCleaveHistory(),
              "Cancel clears the temporary Multi-cut history along with its pending draft");
    }

    const auto runSurfaceProjectionCase = [](
        const sed::app::ViewportCamera::ProjectionType projectionType,
        const float yawDegrees,
        const float pitchDegrees,
        const char *const label)
    {
        const auto projectionCheck = [label](
            const bool condition,
            const char *const description)
        {
            const std::string message = std::string(label) + " " + description;
            check(condition, message.c_str());
        };

        Level level{};
        makeTestCube(level);
        const Sector &sector = level.sectors.front();
        const Surface &face = sector.surfaces.front();
        editor::Selection selectedSurface{};
        selectedSurface.surfaces.insert(SurfaceRef{0, 0});

        GlView view{};
        view.setAttribute(Qt::WA_DontShowOnScreen);
        view.resize(640, 480);
        view.setLevel(&level);
        view.setSelectionState(0, 0, -1, -1, selectedSurface);
        view.setSurfaceSubMode(GlView::SurfaceSubMode::Face);
        view.setSurfaceCleaveToolEnabled(true);
        view.setCleaveStagedMode(true);
        view.setCleaveChainEnabled(false);
        view.setCameraProjectionType(projectionType);
        view.setOrbitViewImmediate(yawDegrees, pitchDegrees, 0.0f);
        view.setCameraOrbitDistance(8.0f);
        if ( projectionType == sed::app::ViewportCamera::ProjectionType::Orthographic )
        {
            // OrthoScale is pixels per world unit times a fixed viewport
            // factor: larger values zoom in. At scale 4 the authored .06
            // world-unit join gap projects below the pointer drag threshold.
            view.setCameraOrthoScale(64.0f);
        }
        else
        {
            view.setCameraOrbitDistance(4.0f);
        }
        configureQuietView(view);
        view.show();
        QApplication::processEvents(QEventLoop::AllEvents);
        view.captureStagedCleaveContextForTest(true);
        projectionCheck(view.stagedCleaveContextMatchesCurrentForTest(true),
                        "Surface source/target context matches before Shift join");

        projectionCheck(face.vertices.size() >= 4,
                        "selected Surface exposes a polygon to author test strokes on its plane");
        if ( face.vertices.size() < 4 )
        {
            return;
        }
        const auto vertexPosition = [&sector, &face](const std::size_t vertexIndex)
        {
            const Vertex &vertex = sector.vertices.at(
                static_cast<std::size_t>(face.vertices.at(vertexIndex)));
            return QVector3D(
                static_cast<float>(vertex.x),
                static_cast<float>(vertex.y),
                static_cast<float>(vertex.z));
        };
        const QVector3D faceOrigin = vertexPosition(0);
        const QVector3D faceAxisA = vertexPosition(1) - faceOrigin;
        const QVector3D faceAxisB = vertexPosition(3) - faceOrigin;
        const QVector3D surfaceNormal = toQVector3D(face.normal).normalized();
        const QVector3D lineDirection = faceAxisA.normalized();
        const QVector3D cutPlaneNormal =
            QVector3D::crossProduct(surfaceNormal, lineDirection).normalized();
        const auto pointOnFace = [faceOrigin, faceAxisA, faceAxisB](
                                     const float alongA,
                                     const float alongB)
        {
            return faceOrigin + faceAxisA * alongA + faceAxisB * alongB;
        };
        const QVector3D firstLineFirst = pointOnFace(0.05f, 0.15f);
        const QVector3D firstLineSecond = pointOnFace(0.42f, 0.15f);
        const QVector3D secondLineFirst = pointOnFace(0.48f, 0.15f);
        const QVector3D secondLineSecond = pointOnFace(0.78f, 0.35f);
        view.addStagedCleaveLineForTest(firstLineFirst, firstLineSecond, cutPlaneNormal);
        view.addStagedCleaveLineForTest(secondLineFirst, secondLineSecond, cutPlaneNormal);
        view.selectStagedCleaveLineForTest(0);

        const auto movingPoint = project(view, firstLineSecond);
        const auto targetPoint = project(view, secondLineFirst);
        projectionCheck(movingPoint && targetPoint,
                        "Surface endpoints project for a real Shift join");
        if ( !movingPoint || !targetPoint )
        {
            return;
        }
        const double shiftJoinMotionPx = std::hypot(
            movingPoint->x() - targetPoint->x(),
            movingPoint->y() - targetPoint->y());
        if ( shiftJoinMotionPx <= 2.0 || shiftJoinMotionPx > 18.0 )
        {
            std::cerr << label << " Surface Shift-join projection diagnostic: deltaPx="
                      << shiftJoinMotionPx
                      << " first=(" << movingPoint->x() << ", " << movingPoint->y()
                      << ") target=(" << targetPoint->x() << ", " << targetPoint->y()
                      << ") cameraOrthoScale=" << view.cameraOrthoScale()
                      << " cameraDistance=" << view.cameraOrbitDistance()
                      << " faceAxisA=" << faceAxisA.length()
                      << " worldGap=" << (secondLineFirst - firstLineSecond).length()
                      << '\n';
        }
        projectionCheck(shiftJoinMotionPx > 2.0 && shiftJoinMotionPx <= 18.0,
                        "Shift-join fixture moves beyond drag threshold but within endpoint snap radius");
        if ( shiftJoinMotionPx <= 2.0 || shiftJoinMotionPx > 18.0 )
        {
            return;
        }

        const auto beforeJoin = view.recoveryState();
        sendDrag(view, *movingPoint, {*targetPoint}, Qt::ShiftModifier);
        const auto joined = view.recoveryState();
        const bool joinedEndpoints = joined.cleaveLines.size() == 2
            && samePoint(joined.cleaveLines[0].second, joined.cleaveLines[1].first)
            && closeQVec(toQVector3D(joined.cleaveLines[0].second), secondLineFirst, 1.0e-5f);
        if ( !joinedEndpoints || !view.canUndoStagedCleaveHistory() )
        {
            std::cerr << label << " Surface Shift join diagnostic: lines="
                      << joined.cleaveLines.size()
                      << " contextMatches="
                      << view.stagedCleaveContextMatchesCurrentForTest(true)
                      << " canUndo=" << view.canUndoStagedCleaveHistory();
            if ( joined.cleaveLines.size() == 2 )
            {
                const QVector3D firstEnd = toQVector3D(joined.cleaveLines[0].second);
                const QVector3D secondStart = toQVector3D(joined.cleaveLines[1].first);
                std::cerr << " endpointDelta=" << (firstEnd - secondStart).length()
                          << " expectedTargetDelta="
                          << (firstEnd - secondLineFirst).length();
            }
            std::cerr << '\n';
        }
        projectionCheck(joinedEndpoints && view.canUndoStagedCleaveHistory(),
                        "Surface Shift join creates a temporary undo step");
        if ( joinedEndpoints )
        {
            projectionCheck(view.undoStagedCleaveHistory()
                                && sameCleaveDraft(view.recoveryState(), beforeJoin),
                            "Surface Undo restores both independent lines and context");
            projectionCheck(view.redoStagedCleaveHistory()
                                && sameCleaveDraft(view.recoveryState(), joined),
                            "Surface Redo restores the joined endpoint group");

            const QVector3D body =
                (toQVector3D(joined.cleaveLines[0].first)
                 + toQVector3D(joined.cleaveLines[0].second)) * 0.5f;
            const auto bodyStart = project(view, body);
            const auto joinedFirst = project(
                view, toQVector3D(joined.cleaveLines[0].first));
            const auto joinedSecond = project(
                view, toQVector3D(joined.cleaveLines[0].second));
            const TVector neighborSecond = joined.cleaveLines[1].second;
            const auto screenDistance = [](const QPointF& left, const QPointF& right)
            {
                return std::hypot(left.x() - right.x(), left.y() - right.y());
            };
            const bool bodyPickClear = bodyStart && joinedFirst && joinedSecond
                && screenDistance(*bodyStart, *joinedFirst) > 10.0
                && screenDistance(*bodyStart, *joinedSecond) > 10.0;
            projectionCheck(bodyPickClear,
                            "Surface body-drag fixture is visibly outside both endpoint pick radii");
            if ( bodyPickClear )
            {
                sendDrag(view, *bodyStart,
                         {*bodyStart + QPointF(0.0, -20.0)}, Qt::NoModifier);
                const auto moved = view.recoveryState();
                projectionCheck(moved.cleaveLines.size() == 2
                          && samePoint(moved.cleaveLines[0].second,
                                       moved.cleaveLines[1].first)
                          && samePoint(moved.cleaveLines[1].second, neighborSecond),
                                "Surface body movement carries the joint and pins its other end");
                projectionCheck(view.undoStagedCleaveHistory()
                          && sameCleaveDraft(view.recoveryState(), joined),
                                "Surface body movement is one reversible edit");
                projectionCheck(view.redoStagedCleaveHistory()
                          && sameCleaveDraft(view.recoveryState(), moved),
                                "Surface body movement redoes exactly");
            }
        }
    };
    runSurfaceProjectionCase(
        sed::app::ViewportCamera::ProjectionType::Orthographic,
        0.0f,
        90.0f,
        "orthographic");
    runSurfaceProjectionCase(
        sed::app::ViewportCamera::ProjectionType::Perspective,
        31.0f,
        -23.0f,
        "perspective");

    const auto runSharedJointCycleCase = [](
        const bool surfaceMode,
        const sed::app::ViewportCamera::ProjectionType projectionType,
        const char *const label)
    {
        const auto checkLabel = [label](const bool condition, const std::string& description)
        {
            const std::string message = std::string(label) + " " + description;
            check(condition, message.c_str());
        };

        Level level{};
        makeTestCube(level);
        const Sector &sector = level.sectors.front();
        const Surface &face = sector.surfaces.front();
        editor::Selection selection{};
        if ( surfaceMode )
        {
            selection.surfaces.insert(SurfaceRef{0, 0});
        }
        else
        {
            selection.sectors.insert(0);
        }

        GlView view{};
        view.setAttribute(Qt::WA_DontShowOnScreen);
        view.resize(640, 480);
        view.setLevel(&level);
        view.setSelectionState(0, surfaceMode ? 0 : -1, -1, -1, selection);
        view.setSurfaceSubMode(GlView::SurfaceSubMode::Face);
        view.setCameraProjectionType(projectionType);
        float orbitYaw =
            projectionType == sed::app::ViewportCamera::ProjectionType::Perspective
                ? 31.0f : 0.0f;
        float orbitPitch =
            projectionType == sed::app::ViewportCamera::ProjectionType::Perspective
                ? -23.0f : 90.0f;
        if ( surfaceMode
            && projectionType
                == sed::app::ViewportCamera::ProjectionType::Perspective )
        {
            const QVector3D surfaceNormal = toQVector3D(face.normal).normalized();
            constexpr double kRadiansToDegrees = 180.0 / 3.14159265358979323846;
            orbitYaw = static_cast<float>(
                std::atan2(surfaceNormal.x(), -surfaceNormal.y())
                * kRadiansToDegrees);
            orbitPitch = static_cast<float>(
                std::asin(std::clamp(surfaceNormal.z(), -1.0f, 1.0f))
                * kRadiansToDegrees);
        }
        view.setOrbitViewImmediate(orbitYaw, orbitPitch, 0.0f);
        view.setCameraOrbitDistance(8.0f);
        if ( projectionType == sed::app::ViewportCamera::ProjectionType::Orthographic )
        {
            view.setCameraOrthoScale(16.0f);
        }
        configureQuietView(view);
        if ( surfaceMode )
        {
            view.setSurfaceCleaveToolEnabled(true);
        }
        else
        {
            view.setSectorCleaveToolEnabled(true);
        }
        view.setCleaveStagedMode(true);
        view.setCleaveChainEnabled(false);
        view.show();
        QApplication::processEvents(QEventLoop::AllEvents);
        view.captureStagedCleaveContextForTest(surfaceMode);
        checkLabel(view.stagedCleaveContextMatchesCurrentForTest(surfaceMode),
                   "viewport, source Surface, and target context match before pointer input");

        QVector3D joint{};
        QVector3D first{};
        QVector3D second{};
        QVector3D third{};
        QVector3D planeNormal{};
        QVector3D firstPlaneNormal{};
        QVector3D secondPlaneNormal{};
        QVector3D thirdPlaneNormal{};
        if ( surfaceMode && face.vertices.size() >= 4 )
        {
            const auto vertexPosition = [&sector, &face](const std::size_t vertexIndex)
            {
                const Vertex &vertex = sector.vertices.at(
                    static_cast<std::size_t>(face.vertices.at(vertexIndex)));
                return QVector3D(
                    static_cast<float>(vertex.x),
                    static_cast<float>(vertex.y),
                    static_cast<float>(vertex.z));
            };
            const QVector3D origin = vertexPosition(0);
            const QVector3D axisA = vertexPosition(1) - origin;
            const QVector3D axisB = vertexPosition(3) - origin;
            const QVector3D surfaceNormal = toQVector3D(face.normal).normalized();
            const auto pointOnFace = [origin, axisA, axisB](const float alongA,
                                                            const float alongB)
            {
                return origin + axisA * alongA + axisB * alongB;
            };
            joint = pointOnFace(0.5f, 0.5f);
            first = pointOnFace(0.08f, 0.5f);
            second = pointOnFace(0.92f, 0.5f);
            third = pointOnFace(0.5f, 0.92f);
            firstPlaneNormal = QVector3D::crossProduct(
                surfaceNormal,
                (joint - first).normalized()).normalized();
            secondPlaneNormal = QVector3D::crossProduct(
                surfaceNormal,
                (second - joint).normalized()).normalized();
            thirdPlaneNormal = QVector3D::crossProduct(
                surfaceNormal,
                (joint - third).normalized()).normalized();
        }
        else if ( !surfaceMode )
        {
            QVector3D right = view.cameraRight();
            QVector3D up = view.cameraUp();
            QVector3D forward = view.cameraForward();
            right.normalize();
            up.normalize();
            forward.normalize();
            joint = view.cameraTarget();
            first = joint - right * 0.7f;
            second = joint + right * 0.7f;
            third = joint + up * 0.7f;
            planeNormal = forward;
            firstPlaneNormal = planeNormal;
            secondPlaneNormal = planeNormal;
            thirdPlaneNormal = planeNormal;
        }
        else
        {
            checkLabel(false, "shared-joint fixture exposes a four-point face");
            return;
        }

        view.addStagedCleaveLineForTest(first, joint, firstPlaneNormal);
        view.addStagedCleaveLineForTest(joint, second, secondPlaneNormal);
        view.addStagedCleaveLineForTest(third, joint, thirdPlaneNormal);
        view.selectStagedCleaveLineForTest(2);
        const auto jointPosition = project(view, joint);
        checkLabel(jointPosition.has_value(), "shared joint projects for Alt-click cycling");
        if ( !jointPosition )
        {
            return;
        }

        const auto beforeCycle = view.recoveryState();
        const bool ownsUndoBeforeCycle = view.stagedCleaveHistoryOwnsUndo();
        const bool canUndoBeforeCycle = view.canUndoStagedCleaveHistory();
        const bool canRedoBeforeCycle = view.canRedoStagedCleaveHistory();
        for ( std::int32_t expectedLine : {0, 1, 2, 0} )
        {
            sendClick(view, *jointPosition, Qt::AltModifier);
            const auto afterClick = view.recoveryState();
            checkLabel(
                afterClick.selectedCleaveLines == std::vector<std::int32_t>{expectedLine} &&
                    afterClick.selectedCleaveLine == expectedLine,
                "Alt-click cycles to incident staged line " + std::to_string(expectedLine));
            checkLabel(
                sameStagedLines(afterClick.cleaveLines, beforeCycle.cleaveLines) &&
                    sameCleaveContext(afterClick.cleaveContext, beforeCycle.cleaveContext) &&
                    afterClick.cleaveChainOrigin.has_value() ==
                        beforeCycle.cleaveChainOrigin.has_value() &&
                    view.stagedCleaveHistoryOwnsUndo() == ownsUndoBeforeCycle &&
                    view.canUndoStagedCleaveHistory() == canUndoBeforeCycle &&
                    view.canRedoStagedCleaveHistory() == canRedoBeforeCycle,
                "Alt-click cycling preserves authored joints and local history");
        }
    };
    runSharedJointCycleCase(
        false,
        sed::app::ViewportCamera::ProjectionType::Perspective,
        "Sector perspective");
    runSharedJointCycleCase(
        false,
        sed::app::ViewportCamera::ProjectionType::Orthographic,
        "Sector orthographic");
    runSharedJointCycleCase(
        true,
        sed::app::ViewportCamera::ProjectionType::Perspective,
        "Surface perspective");
    runSharedJointCycleCase(
        true,
        sed::app::ViewportCamera::ProjectionType::Orthographic,
        "Surface orthographic");

    const auto runMultiSelectionMoveCase = [](
        const bool surfaceMode,
        const sed::app::ViewportCamera::ProjectionType projectionType,
        const char *const label)
    {
        const auto checkLabel = [label](const bool condition, const std::string& description)
        {
            const std::string message = std::string(label) + " " + description;
            check(condition, message.c_str());
        };
        Level level{};
        makeTestCube(level);
        const Sector &sector = level.sectors.front();
        const Surface &face = sector.surfaces.front();
        editor::Selection selection{};
        if ( surfaceMode )
        {
            selection.surfaces.insert(SurfaceRef{0, 0});
        }
        else
        {
            selection.sectors.insert(0);
        }

        GlView view{};
        view.setAttribute(Qt::WA_DontShowOnScreen);
        view.resize(640, 480);
        view.setLevel(&level);
        view.setSelectionState(0, surfaceMode ? 0 : -1, -1, -1, selection);
        view.setSurfaceSubMode(GlView::SurfaceSubMode::Face);
        view.setCameraProjectionType(projectionType);
        const bool perspective =
            projectionType == sed::app::ViewportCamera::ProjectionType::Perspective;
        float orbitYaw = perspective ? 31.0f : 0.0f;
        float orbitPitch = perspective ? -23.0f : 90.0f;
        if ( surfaceMode && perspective )
        {
            const QVector3D surfaceNormal = toQVector3D(face.normal).normalized();
            constexpr double kRadiansToDegrees = 180.0 / 3.14159265358979323846;
            orbitYaw = static_cast<float>(
                std::atan2(surfaceNormal.x(), -surfaceNormal.y())
                * kRadiansToDegrees);
            orbitPitch = static_cast<float>(
                std::asin(std::clamp(surfaceNormal.z(), -1.0f, 1.0f))
                * kRadiansToDegrees);
        }
        view.setOrbitViewImmediate(orbitYaw, orbitPitch, 0.0f);
        view.setCameraOrbitDistance(
            surfaceMode
                && projectionType == sed::app::ViewportCamera::ProjectionType::Perspective
            ? 2.0f
            : 4.0f);
        if ( !perspective )
        {
            view.setCameraOrthoScale(surfaceMode ? 128.0f : 64.0f);
        }
        configureQuietView(view);
        if ( surfaceMode )
        {
            view.setSurfaceCleaveToolEnabled(true);
        }
        else
        {
            view.setSectorCleaveToolEnabled(true);
        }
        view.setCleaveStagedMode(true);
        view.setCleaveChainEnabled(false);
        view.show();
        QApplication::processEvents(QEventLoop::AllEvents);
        view.captureStagedCleaveContextForTest(surfaceMode);
        checkLabel(view.stagedCleaveContextMatchesCurrentForTest(surfaceMode),
                   "viewport, source Surface, and target context match before pointer input");

        QVector3D joint{};
        QVector3D first{};
        QVector3D second{};
        QVector3D third{};
        QVector3D planeNormal{};
        QVector3D firstPlaneNormal{};
        QVector3D secondPlaneNormal{};
        QVector3D thirdPlaneNormal{};
        if ( surfaceMode && face.vertices.size() >= 4 )
        {
            const auto vertexPosition = [&sector, &face](const std::size_t vertexIndex)
            {
                const Vertex &vertex = sector.vertices.at(
                    static_cast<std::size_t>(face.vertices.at(vertexIndex)));
                return QVector3D(static_cast<float>(vertex.x),
                                 static_cast<float>(vertex.y),
                                 static_cast<float>(vertex.z));
            };
            const QVector3D origin = vertexPosition(0);
            const QVector3D axisA = vertexPosition(1) - origin;
            const QVector3D axisB = vertexPosition(3) - origin;
            const QVector3D surfaceNormal = toQVector3D(face.normal).normalized();
            const auto pointOnFace = [origin, axisA, axisB](const float alongA,
                                                            const float alongB)
            {
                return origin + axisA * alongA + axisB * alongB;
            };
            joint = pointOnFace(0.5f, 0.5f);
            first = pointOnFace(0.18f, 0.5f);
            second = pointOnFace(0.82f, 0.5f);
            third = pointOnFace(0.5f, 0.82f);
            firstPlaneNormal = QVector3D::crossProduct(
                surfaceNormal,
                (joint - first).normalized()).normalized();
            secondPlaneNormal = QVector3D::crossProduct(
                surfaceNormal,
                (second - joint).normalized()).normalized();
            thirdPlaneNormal = QVector3D::crossProduct(
                surfaceNormal,
                (joint - third).normalized()).normalized();
        }
        else if ( !surfaceMode )
        {
            QVector3D right = view.cameraRight();
            QVector3D up = view.cameraUp();
            QVector3D forward = view.cameraForward();
            right.normalize();
            up.normalize();
            forward.normalize();
            joint = view.cameraTarget();
            first = joint - right * 0.7f;
            second = joint + right * 0.7f;
            third = joint + up * 0.7f;
            planeNormal = forward;
            firstPlaneNormal = planeNormal;
            secondPlaneNormal = planeNormal;
            thirdPlaneNormal = planeNormal;
        }
        else
        {
            checkLabel(false, "multi-selection fixture exposes a four-point Surface");
            return;
        }

        view.addStagedCleaveLineForTest(first, joint, firstPlaneNormal);
        view.addStagedCleaveLineForTest(joint, second, secondPlaneNormal);
        view.addStagedCleaveLineForTest(third, joint, thirdPlaneNormal);
        const auto firstMidpoint = project(view, (first + joint) * 0.5f);
        const auto secondMidpoint = project(view, (joint + second) * 0.5f);
        const auto thirdMidpoint = project(view, (third + joint) * 0.5f);
        const auto firstEndpoint = project(view, first);
        const auto jointPoint = project(view, joint);
        const auto secondEndpoint = project(view, second);
        const auto thirdEndpoint = project(view, third);
        checkLabel(firstMidpoint && secondMidpoint && thirdMidpoint && firstEndpoint
                       && jointPoint && secondEndpoint && thirdEndpoint,
                   "staged line bodies project for modifier selection");
        if ( !firstMidpoint || !secondMidpoint || !thirdMidpoint || !firstEndpoint
            || !jointPoint || !secondEndpoint || !thirdEndpoint )
        {
            return;
        }
        const std::array<QPointF, 4> projectedEndpoints{
            *firstEndpoint,
            *jointPoint,
            *secondEndpoint,
            *thirdEndpoint,
        };
        const auto midpointClearOfEndpoints = [&projectedEndpoints](
            const QPointF& midpoint)
        {
            return std::all_of(
                projectedEndpoints.begin(),
                projectedEndpoints.end(),
                [&midpoint](const QPointF& endpoint)
                {
                    return std::hypot(midpoint.x() - endpoint.x(),
                                      midpoint.y() - endpoint.y()) > 10.0;
                });
        };
        const bool firstMidpointClear = midpointClearOfEndpoints(*firstMidpoint);
        const bool secondMidpointClear = midpointClearOfEndpoints(*secondMidpoint);
        const bool thirdMidpointClear = midpointClearOfEndpoints(*thirdMidpoint);
        if ( !firstMidpointClear || !secondMidpointClear || !thirdMidpointClear )
        {
            const auto distance = [](const QPointF& left, const QPointF& right)
            {
                return std::hypot(left.x() - right.x(), left.y() - right.y());
            };
            std::cerr << label << " body-clearance diagnostic: projection="
                      << (perspective ? "perspective" : "orthographic")
                      << " scale=" << view.cameraOrthoScale()
                      << " distance=" << view.cameraOrbitDistance()
                      << " first=(" << distance(*firstMidpoint, *firstEndpoint) << ", "
                      << distance(*firstMidpoint, *jointPoint) << ") second=("
                      << distance(*secondMidpoint, *jointPoint) << ", "
                      << distance(*secondMidpoint, *secondEndpoint) << ") third=("
                      << distance(*thirdMidpoint, *jointPoint) << ", "
                      << distance(*thirdMidpoint, *thirdEndpoint) << ")\n";
        }
        checkLabel(firstMidpointClear && secondMidpointClear && thirdMidpointClear,
                   "every intended body pick is more than the endpoint radius from both endpoints");

        const auto beforeEndpointSelection = view.recoveryState();
        const auto clickLineBody = [&view](
                                       const QPointF& point,
                                       const Qt::KeyboardModifiers modifiers)
        {
            sendMouseEvent(view,
                           QEvent::MouseButtonPress,
                           point,
                           Qt::LeftButton,
                           Qt::LeftButton,
                           modifiers);
            const bool acquiredLine = view.stagedCleavePlaneDraggingForTest();
            const bool editingEndpoint = view.cleaveStagedEndpointEditing();
            sendMouseEvent(view,
                           QEvent::MouseButtonRelease,
                           point,
                           Qt::LeftButton,
                           Qt::NoButton,
                           modifiers);
            QApplication::processEvents(QEventLoop::AllEvents);
            return std::pair<bool, bool>{acquiredLine, editingEndpoint};
        };
        sendClick(view, *firstEndpoint, Qt::ShiftModifier);
        const auto afterEndpointAdd = view.recoveryState();
        checkLabel(afterEndpointAdd.selectedCleaveLines ==
                       std::vector<std::int32_t>{0, 2}
                       && sameStagedLines(afterEndpointAdd.cleaveLines,
                                          beforeEndpointSelection.cleaveLines)
                       && sameCleaveHistory(afterEndpointAdd.cleaveHistory,
                                            beforeEndpointSelection.cleaveHistory),
                   "Shift-clicking a stationary endpoint adds its physical line without "
                   "breaking the joint or creating a draft edit");
        sendClick(view, *firstEndpoint, Qt::ControlModifier);
        const auto afterEndpointRemove = view.recoveryState();
        checkLabel(afterEndpointRemove.selectedCleaveLines ==
                       std::vector<std::int32_t>{2}
                       && sameStagedLines(afterEndpointRemove.cleaveLines,
                                          beforeEndpointSelection.cleaveLines)
                       && sameCleaveHistory(afterEndpointRemove.cleaveHistory,
                                            beforeEndpointSelection.cleaveHistory),
                   "Ctrl-clicking a stationary endpoint removes only its physical line");

        const auto plainBodyHit = clickLineBody(*firstMidpoint, Qt::NoModifier);
        checkLabel(view.recoveryState().selectedCleaveLines ==
                       std::vector<std::int32_t>{0},
                   "plain click selects one physical staged line");
        const auto shiftBodyHit = clickLineBody(*secondMidpoint, Qt::ShiftModifier);
        checkLabel(view.recoveryState().selectedCleaveLines ==
                       std::vector<std::int32_t>{0, 1},
                   "Shift-click adds the second physical line");
        const auto afterShiftSelection = view.recoveryState();
        if ( afterShiftSelection.selectedCleaveLines
            != std::vector<std::int32_t>{0, 1} )
        {
            std::cerr << label << " body-click selection diagnostic: plainHit=("
                      << plainBodyHit.first << ", " << plainBodyHit.second
                      << ") shiftHit=(" << shiftBodyHit.first << ", "
                      << shiftBodyHit.second << ") selected={";
            for ( std::size_t index = 0;
                  index < afterShiftSelection.selectedCleaveLines.size();
                  ++index )
            {
                if ( index != 0 )
                {
                    std::cerr << ',';
                }
                std::cerr << afterShiftSelection.selectedCleaveLines[index];
            }
            std::cerr << "} contextMatches="
                      << view.stagedCleaveContextMatchesCurrentForTest(surfaceMode)
                      << " historyOwns=" << view.stagedCleaveHistoryOwnsUndo()
                      << '\n';
        }
        clickLineBody(*firstMidpoint, Qt::ControlModifier);
        checkLabel(view.recoveryState().selectedCleaveLines ==
                       std::vector<std::int32_t>{1},
                   "Ctrl-click removes the hit physical line");
        clickLineBody(*firstMidpoint, Qt::ShiftModifier);
        const auto beforeGroupMove = view.recoveryState();
        checkLabel(beforeGroupMove.selectedCleaveLines ==
                       std::vector<std::int32_t>{0, 1},
                   "Shift-click restores the two-line movement set");

        sendDrag(view,
                 *firstMidpoint,
                 {*firstMidpoint + QPointF(0.0, -20.0)},
                 Qt::NoModifier);
        const auto afterGroupMove = view.recoveryState();
        bool firstLineMovedRigidly = false;
        bool secondLineMovedRigidly = false;
        bool nonSelectedNeighborFollowsOnlyAtJoint = false;
        if ( afterGroupMove.cleaveLines.size() == 3 )
        {
            const QVector3D firstDelta =
                toQVector3D(afterGroupMove.cleaveLines[0].first) -
                toQVector3D(beforeGroupMove.cleaveLines[0].first);
            const QVector3D secondDelta =
                toQVector3D(afterGroupMove.cleaveLines[0].second) -
                toQVector3D(beforeGroupMove.cleaveLines[0].second);
            const QVector3D thirdFirstDelta =
                toQVector3D(afterGroupMove.cleaveLines[1].first) -
                toQVector3D(beforeGroupMove.cleaveLines[1].first);
            const QVector3D thirdSecondDelta =
                toQVector3D(afterGroupMove.cleaveLines[1].second) -
                toQVector3D(beforeGroupMove.cleaveLines[1].second);
            const QVector3D neighborJointDelta =
                toQVector3D(afterGroupMove.cleaveLines[2].second) -
                toQVector3D(beforeGroupMove.cleaveLines[2].second);
            const QVector3D neighborOtherDelta =
                toQVector3D(afterGroupMove.cleaveLines[2].first) -
                toQVector3D(beforeGroupMove.cleaveLines[2].first);
            firstLineMovedRigidly = closeQVec(firstDelta, secondDelta, 1.0e-5f) &&
                !closeQVec(firstDelta, QVector3D{}, 1.0e-5f);
            secondLineMovedRigidly = closeQVec(firstDelta, thirdFirstDelta, 1.0e-5f) &&
                closeQVec(firstDelta, thirdSecondDelta, 1.0e-5f);
            nonSelectedNeighborFollowsOnlyAtJoint =
                closeQVec(firstDelta, neighborJointDelta, 1.0e-5f) &&
                closeQVec(neighborOtherDelta, QVector3D{}, 1.0e-5f);
        }
        checkLabel(firstLineMovedRigidly && secondLineMovedRigidly &&
                       nonSelectedNeighborFollowsOnlyAtJoint &&
                       afterGroupMove.selectedCleaveLines ==
                           std::vector<std::int32_t>{0, 1},
                   "dragging a selected line rigidly moves the group once and carries only "
                   "the unselected neighbor joint");
        const std::uint64_t authoredRevisionAfterGroupMove =
            view.stagedCleaveAuthoredRevision();
        std::optional<QPointF> movedBodyClick{};
        if ( afterGroupMove.cleaveLines.size() == 3 )
        {
            const QVector3D movedMidpoint =
                (toQVector3D(afterGroupMove.cleaveLines[0].first) +
                 toQVector3D(afterGroupMove.cleaveLines[0].second)) * 0.5f;
            movedBodyClick = project(view, movedMidpoint);
        }
        checkLabel(movedBodyClick.has_value(),
                   "moved finite group line projects for camera-recall selection");
        if ( movedBodyClick )
        {
            sendClick(view, *movedBodyClick, Qt::ShiftModifier);
        }
        const auto afterMovedBodyClick = view.recoveryState();
        checkLabel(movedBodyClick &&
                       sameStagedLines(afterGroupMove.cleaveLines,
                                       afterMovedBodyClick.cleaveLines) &&
                       afterGroupMove.applyOrder == afterMovedBodyClick.applyOrder &&
                       authoredRevisionAfterGroupMove ==
                           view.stagedCleaveAuthoredRevision(),
                   "selecting a moved finite stroke restores only its camera and preserves "
                   "authored endpoints, planes, links, order, and revision");
        const auto beforeControlDrag = view.recoveryState();
        std::optional<QPointF> controlDragStart{};
        if ( beforeControlDrag.cleaveLines.size() == 3 )
        {
            controlDragStart = project(
                view,
                (toQVector3D(beforeControlDrag.cleaveLines[1].first) +
                 toQVector3D(beforeControlDrag.cleaveLines[1].second)) * 0.5f);
        }
        checkLabel(controlDragStart.has_value(),
                   "moved selected line projects for Ctrl-grid drag");
        if ( controlDragStart )
        {
            sendDrag(view,
                     *controlDragStart,
                     {*controlDragStart + QPointF(0.0, -16.0)},
                     Qt::ControlModifier);
        }
        const auto afterControlDrag = view.recoveryState();
        checkLabel(controlDragStart &&
                       afterControlDrag.selectedCleaveLines ==
                           std::vector<std::int32_t>{0, 1} &&
                       !sameStagedLines(afterControlDrag.cleaveLines,
                                        beforeControlDrag.cleaveLines),
                   "Ctrl-drag keeps multi-selection intact while using grid snapping");
    };
    runMultiSelectionMoveCase(
        false,
        sed::app::ViewportCamera::ProjectionType::Perspective,
        "Sector perspective");
    runMultiSelectionMoveCase(
        false,
        sed::app::ViewportCamera::ProjectionType::Orthographic,
        "Sector orthographic");
    runMultiSelectionMoveCase(
        true,
        sed::app::ViewportCamera::ProjectionType::Perspective,
        "Surface perspective");
    runMultiSelectionMoveCase(
        true,
        sed::app::ViewportCamera::ProjectionType::Orthographic,
        "Surface orthographic");

    {
        Level level{};
        makeTestCube(level);
        editor::Selection targets{};
        targets.sectors.insert(0);
        GlView view{};
        view.setAttribute(Qt::WA_DontShowOnScreen);
        view.resize(640, 480);
        view.setLevel(&level);
        view.setSelectionState(0, -1, -1, -1, targets);
        view.setCleaveStagedMode(true);
        view.setSectorCleaveToolEnabled(true);
        view.setCameraProjectionType(
            sed::app::ViewportCamera::ProjectionType::Perspective);
        view.setOrbitViewImmediate(31.0f, -23.0f, 0.0f);
        view.setCameraOrbitDistance(8.0f);
        configureQuietView(view);
        view.show();
        QApplication::processEvents(QEventLoop::AllEvents);
        view.captureStagedCleaveContextForTest(false);
        QVector3D right = view.cameraRight();
        QVector3D forward = view.cameraForward();
        right.normalize();
        forward.normalize();
        const QVector3D center = view.cameraTarget();
        view.addStagedCleaveLineForTest(
            center - right * 0.45f,
            center + right * 0.45f,
            forward);
        const auto beforeBodyClick = view.recoveryState();
        const auto body = project(
            view,
            (toQVector3D(beforeBodyClick.cleaveLines.front().first) +
             toQVector3D(beforeBodyClick.cleaveLines.front().second)) * 0.5f);
        check(body.has_value(),
              "short ordinary Sector stroke projects for camera-recall selection");
        const std::uint64_t authoredRevision = view.stagedCleaveAuthoredRevision();
        if ( body )
        {
            sendClick(view, *body);
        }
        const auto afterBodyClick = view.recoveryState();
        check(body && sameStagedLines(beforeBodyClick.cleaveLines,
                                      afterBodyClick.cleaveLines) &&
                  beforeBodyClick.applyOrder == afterBodyClick.applyOrder &&
                  authoredRevision == view.stagedCleaveAuthoredRevision(),
              "camera recall on a short Sector stroke body click preserves exact finite "
              "geometry and does not create an authored edit");
    }

    {
        auto window = std::make_unique<MainWindow>();
        window->setAttribute(Qt::WA_DontShowOnScreen);
        window->resize(1280, 900);
        window->activateSectorEditModeForTest();

        editor::DocumentState state{};
        state.level.kind = ProjectType::IJIM;
        makeTestCube(state.level);
        state.selection.sectors.insert(0);
        window->setDocumentStateForTest(std::move(state));
        window->activateSectorEditModeForTest();
        window->setLayerVisualStateForTest({true}, {false});
        const bool documentHistorySeeded = window->applyStructuralHistoryEditForTest(
            [](editor::DocumentState &edited)
            {
                edited.level.header.gravity += 0.25;
                return true;
            });
        check(documentHistorySeeded,
              "MainWindow history fixture commits a real non-geometry Level-header edit");
        const std::size_t documentUndoDepth = window->undoDepthForTest();
        check(documentUndoDepth > 0,
              "MainWindow history fixture retains the committed Level-header edit");
        GlView *const view = window->glViewForTest();
        if ( !view )
        {
            check(false, "Multi-cut local history fixture has a viewport");
            return;
        }

        view->setLevel(&window->documentStateForTest().level, GlView::SetLevelMode::resetView);
        view->setSelectionState(0, -1, -1, -1, window->documentStateForTest().selection);
        view->setCameraProjectionType(sed::app::ViewportCamera::ProjectionType::Orthographic);
        view->resetEditorTopDownView();
        view->setCameraOrthoScale(16.0f);
        configureQuietView(*view);
        window->show();
        window->activateWindow();
        QApplication::processEvents(QEventLoop::AllEvents);
        window->updateEditActionsForTest();

        QToolButton *const multiCutButton = window->findChild<QToolButton *>(
            QStringLiteral("cleaveMultiCutButton"));
        QToolButton *const chainButton = window->findChild<QToolButton *>(
            QStringLiteral("cleaveChainButton"));
        QToolButton *const boundedButton = window->findChild<QToolButton *>(
            QStringLiteral("cleaveBoundedButton"));
        QAction *const sectorCleaveAction = window->findChild<QAction *>(
            QStringLiteral("viewport.sector.cleave"));
        check(multiCutButton && chainButton && boundedButton && sectorCleaveAction,
              "MainWindow exposes its actual Multi-cut and Sector Cleave controls");
        if ( multiCutButton )
        {
            multiCutButton->setChecked(true);
        }
        if ( chainButton )
        {
            chainButton->setChecked(false);
        }
        if ( boundedButton )
        {
            boundedButton->setChecked(false);
        }
        check(sectorCleaveAction && sectorCleaveAction->isEnabled(),
              "MainWindow enables Sector Cleave for the selected Sector");
        if ( sectorCleaveAction )
        {
            sectorCleaveAction->setChecked(true);
        }
        check(multiCutButton && multiCutButton->isChecked()
                  && sectorCleaveAction && sectorCleaveAction->isChecked()
                  && view->cleaveStagedMode() && view->sectorCleaveToolEnabled(),
              "actual MainWindow controls arm staged Sector Cleave");

        const auto drawLineAtViewCoordinates = [view](const float offsetAlongUp) -> bool
        {
            QVector3D right = view->cameraRight();
            QVector3D up = view->cameraUp();
            right.normalize();
            up.normalize();
            const QVector3D center = view->cameraTarget();
            const auto first = project(*view, center - right * 1.5f + up * offsetAlongUp);
            const auto second = project(*view, center + right * 1.5f + up * offsetAlongUp);
            if ( !first || !second )
            {
                return false;
            }
            sendDrag(*view, *first, {*second}, Qt::NoModifier);
            return true;
        };

        check(drawLineAtViewCoordinates(0.0f) && view->stagedCleaveLineCount() == 1,
              "real MainWindow Sector gesture creates one pending Multi-cut line");
        if ( view->stagedCleaveLineCount() != 1 )
        {
            return;
        }

        QAction *const undoAction = window->findChild<QAction *>(QStringLiteral("edit.undo"));
        QAction *const redoAction = window->findChild<QAction *>(QStringLiteral("edit.redo"));
        const auto beforeEndpointGesture = view->recoveryState();
        const auto fixedEndpoint = project(
            *view,
            toQVector3D(beforeEndpointGesture.cleaveLines.front().first));
        const auto movingEndpoint = project(
            *view,
            toQVector3D(beforeEndpointGesture.cleaveLines.front().second));
        check(fixedEndpoint && movingEndpoint,
              "pending Multi-cut endpoints project for a partial-edit Undo rollback");
        if ( fixedEndpoint && movingEndpoint )
        {
            sendMouseEvent(*view, QEvent::MouseButtonPress, *movingEndpoint,
                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            const QPointF partialEdit = *fixedEndpoint + QPointF(0.0, 16.0);
            sendMouseEvent(*view, QEvent::MouseMove, partialEdit,
                           Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::processEvents(QEventLoop::AllEvents);
            const bool endpointGestureActive = view->cleaveStagedEndpointEditing();
            check(endpointGestureActive && view->canUndoStagedCleaveHistory()
                      && undoAction && undoAction->isEnabled(),
                  "a partial endpoint preview exposes the active gesture to Undo before release");
            if ( endpointGestureActive && undoAction && undoAction->isEnabled() )
            {
                undoAction->trigger();
            }
            sendMouseEvent(*view, QEvent::MouseButtonRelease, partialEdit,
                           Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::processEvents(QEventLoop::AllEvents);
        }
        check(sameCleaveDraft(view->recoveryState(), beforeEndpointGesture),
              "Undo during a partial endpoint drag restores the exact pending draft and context");
        check(!view->cleaveStagedEndpointEditing(),
              "Undo during a partial endpoint drag clears its preview before release");

        const auto beforeMismatchEndpoint = view->recoveryState();
        const auto mismatchStart = project(
            *view,
            toQVector3D(beforeMismatchEndpoint.cleaveLines.front().second));
        const auto mismatchFixed = project(
            *view,
            toQVector3D(beforeMismatchEndpoint.cleaveLines.front().first));
        check(mismatchStart && mismatchFixed,
              "pending Multi-cut endpoint projects for a mid-gesture selection mismatch");
        if ( mismatchStart && mismatchFixed )
        {
            const QPointF mismatchEnd = *mismatchFixed + QPointF(0.0, 16.0);
            sendMouseEvent(*view, QEvent::MouseButtonPress, *mismatchStart,
                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            sendMouseEvent(*view, QEvent::MouseMove, mismatchEnd,
                           Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            check(view->cleaveStagedEndpointEditing(),
                  "mid-gesture mismatch fixture has an active endpoint preview before selection changes");
            editor::Selection noTargets{};
            view->setSelectionState(-1, -1, -1, -1, noTargets);
            sendMouseEvent(*view, QEvent::MouseButtonRelease, mismatchEnd,
                           Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::processEvents(QEventLoop::AllEvents);
            check(!view->cleaveStagedEndpointEditing()
                      && sameCleaveDraft(view->recoveryState(), beforeMismatchEndpoint)
                      && view->stagedCleaveHistoryOwnsUndo(),
                  "mid-endpoint selection mismatch cancels its preview, restores the draft, and retains its local session");
            view->setSelectionState(0, -1, -1, -1,
                                    window->documentStateForTest().selection);
            QApplication::processEvents(QEventLoop::AllEvents);
        }

        if ( chainButton )
        {
            chainButton->setChecked(true);
        }
        check(view->cleaveChainEnabled(),
              "the actual Chain option controls staged chain-origin editing");
        const auto firstLine = view->recoveryState();
        const auto firstEndpoint = project(*view, toQVector3D(firstLine.cleaveLines.front().first));
        check(firstEndpoint.has_value(),
              "pending Multi-cut line projects the endpoint used to choose a chain origin");
        if ( firstEndpoint )
        {
            sendClick(*view, *firstEndpoint);
        }
        const auto withChosenOrigin = view->recoveryState();
        check(withChosenOrigin.cleaveChainOrigin.has_value() &&
                  withChosenOrigin.cleaveChainOrigin->lineIndex == 0 &&
                  withChosenOrigin.cleaveChainOrigin->endpoint == 0,
              "clicking a staged endpoint chooses the chain origin");

        check(undoAction && undoAction->isEnabled(),
              "a pending Multi-cut edit owns Undo while Level history is available");

        const auto undoChosenOrigin = [&]()
        {
            if ( undoAction && undoAction->isEnabled() )
            {
                undoAction->trigger();
            }
        };
        const auto redoChosenOrigin = [&]()
        {
            if ( redoAction && redoAction->isEnabled() )
            {
                redoAction->trigger();
            }
        };

        undoChosenOrigin();
        const auto afterOriginUndo = view->recoveryState();
        check(afterOriginUndo.cleaveLines.size() == 1 && !afterOriginUndo.cleaveChainOrigin &&
                  window->undoDepthForTest() == documentUndoDepth,
              "Undo restores the previous chain-origin choice without consuming Level history");
        redoChosenOrigin();
        check(sameCleaveDraft(view->recoveryState(), withChosenOrigin) &&
                  window->undoDepthForTest() == documentUndoDepth,
              "Redo restores the chosen chain origin and the exact staged draft");

        const auto beforeMove = view->recoveryState();
        const QVector3D lineBody = (toQVector3D(beforeMove.cleaveLines.front().first) +
                                    toQVector3D(beforeMove.cleaveLines.front().second)) *
                                   0.5f;
        const auto moveStart = project(*view, lineBody);
        check(moveStart.has_value(),
              "pending Multi-cut line projects its body for a multi-event drag");
        if ( moveStart )
        {
            const QPointF moveOne = *moveStart + QPointF(0.0, -8.0);
            const QPointF moveTwo = *moveStart + QPointF(0.0, -16.0);
            const QPointF moveThree = *moveStart + QPointF(0.0, -24.0);
            sendDrag(*view, *moveStart, {moveOne, moveTwo, moveThree}, Qt::NoModifier);
        }
        const auto afterMove = view->recoveryState();
        check(moveStart.has_value() &&
                  !sameStagedLines(beforeMove.cleaveLines, afterMove.cleaveLines),
              "one staged body gesture changes the pending line");
        if ( undoAction && undoAction->isEnabled() )
        {
            undoAction->trigger();
        }
        check(
            sameCleaveDraft(view->recoveryState(), beforeMove) &&
                window->undoDepthForTest() == documentUndoDepth,
            "one local Undo reverses the complete multi-event drag and restores its saved context");
        if ( redoAction && redoAction->isEnabled() )
        {
            redoAction->trigger();
        }
        check(sameCleaveDraft(view->recoveryState(), afterMove),
              "local Redo restores the complete staged drag result");

        const QKeySequence originalUndoShortcut = undoAction
            ? undoAction->shortcut()
            : QKeySequence{};
        const QKeySequence originalRedoShortcut = redoAction
            ? redoAction->shortcut()
            : QKeySequence{};
        const auto beforeConfiguredShortcut = view->recoveryState();
        const auto configuredOriginTarget = project(
            *view,
            toQVector3D(beforeConfiguredShortcut.cleaveLines.front().second));
        check(undoAction && redoAction && configuredOriginTarget,
              "Undo and Redo actions expose configurable Multi-cut shortcut targets");
        if ( undoAction && redoAction && configuredOriginTarget )
        {
            sendClick(*view, *configuredOriginTarget);
            const auto afterConfiguredShortcutEdit = view->recoveryState();
            undoAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+Z")));
            redoAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+Shift+Z")));
            view->setFocus(Qt::OtherFocusReason);
            sendKey(*view, QEvent::KeyPress, Qt::Key_Z,
                    Qt::ControlModifier | Qt::AltModifier);
            sendKey(*view, QEvent::KeyRelease, Qt::Key_Z,
                    Qt::ControlModifier | Qt::AltModifier);
            check(sameCleaveDraft(view->recoveryState(), beforeConfiguredShortcut),
                  "configured Undo QAction binding routes to the pending Multi-cut timeline");
            sendKey(*view, QEvent::KeyPress, Qt::Key_Z,
                    Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier);
            sendKey(*view, QEvent::KeyRelease, Qt::Key_Z,
                    Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier);
            check(sameCleaveDraft(view->recoveryState(), afterConfiguredShortcutEdit),
                  "configured Redo QAction binding restores the pending Multi-cut edit");
            undoAction->setShortcut(originalUndoShortcut);
            redoAction->setShortcut(originalRedoShortcut);

            if ( undoAction->isEnabled() )
            {
                undoAction->trigger();
            }
            const auto beforeNoOpOrigin = view->recoveryState();
            const auto sameOriginPoint = project(
                *view,
                toQVector3D(beforeNoOpOrigin.cleaveLines.front().first));
            if ( sameOriginPoint )
            {
                sendClick(*view, *sameOriginPoint);
            }
            const auto afterNoOpOrigin = view->recoveryState();
            const bool localRedoAfterNoOp = view->canRedoStagedCleaveHistory();
            const bool sameHistoryAfterNoOp = sameCleaveHistory(
                afterNoOpOrigin.cleaveHistory,
                beforeNoOpOrigin.cleaveHistory);
            if ( !sameOriginPoint || !localRedoAfterNoOp || !sameHistoryAfterNoOp
                || !redoAction->isEnabled() )
            {
                const std::size_t beforeCursor = beforeNoOpOrigin.cleaveHistory
                    ? beforeNoOpOrigin.cleaveHistory->cursor
                    : 0;
                const std::size_t afterCursor = afterNoOpOrigin.cleaveHistory
                    ? afterNoOpOrigin.cleaveHistory->cursor
                    : 0;
                const std::size_t beforeCount = beforeNoOpOrigin.cleaveHistory
                    ? beforeNoOpOrigin.cleaveHistory->snapshots.size()
                    : 0;
                const std::size_t afterCount = afterNoOpOrigin.cleaveHistory
                    ? afterNoOpOrigin.cleaveHistory->snapshots.size()
                    : 0;
                std::cerr << "No-op origin diagnostic: projected="
                          << sameOriginPoint.has_value()
                          << " localCanRedo=" << localRedoAfterNoOp
                          << " actionEnabled=" << redoAction->isEnabled()
                          << " historyEqual=" << sameHistoryAfterNoOp
                          << " cursor=" << beforeCursor << "->" << afterCursor
                          << " snapshots=" << beforeCount << "->" << afterCount
                          << '\n';
            }
            check(sameOriginPoint && localRedoAfterNoOp && sameHistoryAfterNoOp
                      && redoAction->isEnabled(),
                  "a no-op origin click preserves the existing Redo branch");
            if ( redoAction->isEnabled() )
            {
                redoAction->trigger();
            }
            check(sameCleaveDraft(view->recoveryState(), afterConfiguredShortcutEdit),
                  "Redo after a no-op gesture restores the exact pending draft");
        }

        // Build an exact-capacity, homogeneous timeline. Every retained state
        // owns the same nested arrays, so dropping the oldest state cannot
        // hide descriptor-capacity growth in the owned-byte estimate.
        const auto capacitySeed = view->captureStagedCleaveDraftState();
        if (capacitySeed.lines.empty())
        {
            check(false, "the history-capacity fixture has a staged line");
        }
        else
        {
            auto originFirst = capacitySeed;
            originFirst.chainOrigin = GlView::RecoveryState::CleaveChainOrigin{0, 0};
            auto originSecond = capacitySeed;
            originSecond.chainOrigin = GlView::RecoveryState::CleaveChainOrigin{0, 1};

            GlView::RecoveryState::CleaveHistory fullCapacityHistory{};
            fullCapacityHistory.snapshots.reserve(64u);
            for (std::size_t index = 0; index < 64u; ++index)
            {
                fullCapacityHistory.snapshots.push_back(
                    index % 2u == 0u ? originFirst : originSecond);
            }
            fullCapacityHistory.cursor = 63u;
            const std::size_t inputHistoryCapacity =
                fullCapacityHistory.snapshots.capacity();
            auto liveCursorState = originSecond;
            view->restoreStagedCleaveDraftState(
                std::move(liveCursorState), std::move(fullCapacityHistory));

            const std::size_t storageAtCapacity =
                view->stagedCleaveHistoryStorageBytesEstimate();
            const std::uint64_t revisionAtCapacity =
                view->stagedCleaveAuthoredRevision();
            const auto originFirstPoint = project(
                *view, toQVector3D(view->recoveryState().cleaveLines.front().first));
            if (!originFirstPoint)
            {
                check(false, "the full-capacity timeline projects its next authored origin");
            }
            else
            {
                sendClick(*view, *originFirstPoint);
                const auto afterCapacityEdit = view->captureStagedCleaveHistory();
                const std::size_t storageAfterOverflow =
                    view->stagedCleaveHistoryStorageBytesEstimate();
                const std::size_t stateShellGrowthThreshold =
                    sizeof(GlView::RecoveryState::CleaveDraftSnapshot);
                const bool capacityEditCommitted =
                    view->stagedCleaveAuthoredRevision() > revisionAtCapacity
                    && afterCapacityEdit
                    && afterCapacityEdit->snapshots.size() == 64u;
                if ( !capacityEditCommitted
                    || storageAfterOverflow > storageAtCapacity + stateShellGrowthThreshold)
                {
                    std::cerr << "Cleave history-capacity diagnostic: inputCapacity="
                              << inputHistoryCapacity
                              << " stateShell="
                              << sizeof(GlView::RecoveryState::CleaveDraftSnapshot)
                              << " storage=" << storageAtCapacity << "->"
                              << storageAfterOverflow
                              << " delta=" << (storageAfterOverflow >= storageAtCapacity
                                      ? storageAfterOverflow - storageAtCapacity
                                      : 0u)
                              << " growthThreshold=" << stateShellGrowthThreshold
                              << " revision=" << revisionAtCapacity << "->"
                              << view->stagedCleaveAuthoredRevision()
                              << " snapshots="
                              << (afterCapacityEdit ? afterCapacityEdit->snapshots.size() : 0u)
                              << " cursor="
                              << (afterCapacityEdit ? afterCapacityEdit->cursor : 0u)
                              << '\n';
                }
                check(capacityEditCommitted,
                      "the 65th authored state retains the bounded 64-state timeline");
                check(storageAfterOverflow <= storageAtCapacity + stateShellGrowthThreshold,
                      "trimming the 65th state does not retain vector-capacity growth");
            }
        }

        for ( std::size_t index = 0; index < 70; ++index )
        {
            const auto state = view->recoveryState();
            const std::size_t endpoint = index % 2 == 0 ? 0 : 1;
            const TVector& authoredPoint = endpoint == 0
                ? state.cleaveLines.front().first
                : state.cleaveLines.front().second;
            const auto originPoint = project(*view, toQVector3D(authoredPoint));
            if ( !originPoint )
            {
                check(false, "bounded Multi-cut history projects each chosen-origin endpoint");
                break;
            }
            sendClick(*view, *originPoint);
        }
        std::size_t boundedUndoCount = 0;
        while ( view->canUndoStagedCleaveHistory() )
        {
            (void)view->undoStagedCleaveHistory();
            ++boundedUndoCount;
        }
        check(boundedUndoCount == 63 && !view->canUndoStagedCleaveHistory(),
              "temporary Multi-cut Undo storage retains at most 64 draft states");
        while ( view->canRedoStagedCleaveHistory() )
        {
            (void)view->redoStagedCleaveHistory();
        }

        const auto spin =
            window->findChild<ui::ScrubFloatEdit *>(QStringLiteral("cleaveAngleStepSpin"));
        QLineEdit *const angleEdit = spin ? spin->lineEdit() : nullptr;
        check(angleEdit != nullptr, "Multi-cut overlay exposes its editable angle-step input");
        if ( angleEdit )
        {
            angleEdit->setText(QStringLiteral("12"));
            angleEdit->setCursorPosition(angleEdit->text().size());
            QApplication::setActiveWindow(window.get());
            angleEdit->setFocus(Qt::OtherFocusReason);
            QApplication::processEvents(QEventLoop::AllEvents);
            QWidget *const actualFocus = QApplication::focusWidget();
            const bool nativeEditorFocused = actualFocus == angleEdit;
            check(nativeEditorFocused,
                  "the editable Multi-cut text field has actual QApplication keyboard focus");
            const auto beforeNativeTextHistory = view->recoveryState();
            if ( nativeEditorFocused )
            {
                sendKey(*angleEdit, QEvent::KeyPress, Qt::Key_3,
                        Qt::NoModifier, QStringLiteral("3"));
                sendKey(*angleEdit, QEvent::KeyRelease, Qt::Key_3, Qt::NoModifier);
                sendKey(*angleEdit, QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier);
                sendKey(*angleEdit, QEvent::KeyRelease, Qt::Key_Z, Qt::ControlModifier);
                check(angleEdit->text() == QStringLiteral("12")
                          && view->stagedCleaveLineCount() == 1
                          && sameCleaveDraft(view->recoveryState(), beforeNativeTextHistory)
                          && window->undoDepthForTest() == documentUndoDepth,
                      "focused Multi-cut text input consumes Ctrl+Z in its native editor without "
                      "changing either history");
                sendKey(*angleEdit, QEvent::KeyPress, Qt::Key_Z,
                        Qt::ControlModifier | Qt::ShiftModifier);
                sendKey(*angleEdit, QEvent::KeyRelease, Qt::Key_Z,
                        Qt::ControlModifier | Qt::ShiftModifier);
                check(angleEdit->text() == QStringLiteral("123")
                          && view->stagedCleaveLineCount() == 1
                          && sameCleaveDraft(view->recoveryState(), beforeNativeTextHistory)
                          && window->undoDepthForTest() == documentUndoDepth,
                      "focused Multi-cut text input consumes Ctrl+Shift+Z in its native editor");
            }
        }

        QApplication::setActiveWindow(window.get());
        view->setFocus(Qt::OtherFocusReason);
        QApplication::processEvents(QEventLoop::AllEvents);
        check(QApplication::focusWidget() == view,
              "the viewport has actual keyboard focus before Outliner and Clear history commands");

        QTreeWidget *const outliner = window->findChild<QTreeWidget *>(QStringLiteral("outlinerTree"));
        const auto findSectorItem = [](auto &&self, QTreeWidgetItem *parent) -> QTreeWidgetItem *
        {
            const int childCount = parent ? parent->childCount() : 0;
            for ( int index = 0; index < childCount; ++index )
            {
                QTreeWidgetItem *const child = parent->child(index);
                if ( child->data(0, kOutlinerKindRole).toInt()
                        == static_cast<int>(OutlinerKind::sector)
                    && child->data(0, kOutlinerIndexRole).toInt() == 0 )
                {
                    return child;
                }
                if ( QTreeWidgetItem *const nested = self(self, child) )
                {
                    return nested;
                }
            }
            return nullptr;
        };
        QTreeWidgetItem *sectorItem = nullptr;
        if ( outliner )
        {
            sectorItem = findSectorItem(findSectorItem, nullptr);
            if ( !sectorItem )
            {
                for ( int index = 0;
                      index < outliner->topLevelItemCount() && !sectorItem;
                      ++index )
                {
                    QTreeWidgetItem *const item = outliner->topLevelItem(index);
                    if ( item->data(0, kOutlinerKindRole).toInt()
                            == static_cast<int>(OutlinerKind::sector)
                        && item->data(0, kOutlinerIndexRole).toInt() == 0 )
                    {
                        sectorItem = item;
                    }
                    else
                    {
                        sectorItem = findSectorItem(findSectorItem, item);
                    }
                }
            }
        }
        check(outliner && sectorItem,
              "Multi-cut selection mismatch fixture exposes the real Sector Outliner row");
        if ( !outliner || !sectorItem )
        {
            return;
        }
        outliner->clearSelection();
        QApplication::processEvents(QEventLoop::AllEvents);
        check(window->documentStateForTest().selection.sectors.empty()
                  && view->sectorCleaveToolEnabled()
                  && view->stagedCleaveLineCount() == 1,
              "clearing the Outliner selection preserves an armed pending Multi-cut draft");
        view->requestApplyStagedCleave();
        check(view->stagedCleaveLineCount() == 1,
              "failed Multi-cut Apply caused by a temporary target mismatch preserves the draft");
        sectorItem->setSelected(true);
        QApplication::processEvents(QEventLoop::AllEvents);
        check(view->stagedCleaveLineCount() == 1
                  && view->cleaveStagedMode()
                  && view->sectorCleaveToolEnabled()
                  && multiCutButton && multiCutButton->isChecked()
                  && sectorCleaveAction && sectorCleaveAction->isChecked()
                  && undoAction && undoAction->isEnabled() &&
                  window->undoDepthForTest() == documentUndoDepth,
              "restoring the authored target resumes pending local history after failed Apply");

        const auto draftBeforeClear = view->recoveryState();
        const std::size_t documentUndoDepthBeforeClear = window->undoDepthForTest();
        QToolButton *const clearButton =
            window->findChild<QToolButton *>(QStringLiteral("cleaveCancelButton"));
        check(clearButton != nullptr, "Multi-cut overlay exposes its Clear button");
        if ( clearButton )
        {
            clearButton->click();
        }
        const auto draftAfterClear = view->recoveryState();
        const std::size_t documentUndoDepthAfterClear = window->undoDepthForTest();
        check(draftAfterClear.cleaveLines.empty(),
              "Multi-cut Clear removes the visible pending draft");
        const bool undoEnabledImmediatelyAfterClear =
            undoAction && undoAction->isEnabled();
        const bool historyOwnedImmediatelyAfterClear =
            view->stagedCleaveHistoryOwnsUndo();
        const bool canUndoImmediatelyAfterClear =
            view->canUndoStagedCleaveHistory();
        const bool canRedoImmediatelyAfterClear =
            view->canRedoStagedCleaveHistory();
        QWidget *const focusImmediatelyAfterClear = QApplication::focusWidget();
        if ( undoEnabledImmediatelyAfterClear )
        {
            undoAction->trigger();
        }
        const auto draftAfterClearUndo = view->recoveryState();
        const std::size_t documentUndoDepthAfterClearUndo = window->undoDepthForTest();
        const bool clearUndoRestored = sameCleaveDraft(
            draftAfterClearUndo, draftBeforeClear);
        if ( !clearUndoRestored
            || documentUndoDepthAfterClear != documentUndoDepthBeforeClear
            || documentUndoDepthAfterClearUndo != documentUndoDepthBeforeClear )
        {
            std::cerr << "DIAG Clear Undo: focus="
                      << (focusImmediatelyAfterClear
                              ? focusImmediatelyAfterClear->metaObject()->className()
                              : "null")
                      << ':'
                      << (focusImmediatelyAfterClear
                              ? focusImmediatelyAfterClear->objectName().toStdString()
                              : std::string{})
                      << " undoEnabledImmediatelyAfterClear="
                      << undoEnabledImmediatelyAfterClear
                      << " triggerSkipped=" << !undoEnabledImmediatelyAfterClear
                      << " localOwnsImmediatelyAfterClear="
                      << historyOwnedImmediatelyAfterClear
                      << " canUndoImmediatelyAfterClear=" << canUndoImmediatelyAfterClear
                      << " canRedoImmediatelyAfterClear=" << canRedoImmediatelyAfterClear
                      << " undoActionEnabledAfterAttempt="
                      << (undoAction && undoAction->isEnabled())
                      << " historyOwns=" << view->stagedCleaveHistoryOwnsUndo()
                      << " canUndo=" << view->canUndoStagedCleaveHistory()
                      << " canRedo=" << view->canRedoStagedCleaveHistory()
                      << " activeSector=" << view->activeSectorIndex()
                      << " stagedMode=" << view->cleaveStagedMode()
                      << " sectorTool=" << view->sectorCleaveToolEnabled()
                      << " multiCutButtonChecked="
                      << (multiCutButton && multiCutButton->isChecked())
                      << " sectorActionChecked="
                      << (sectorCleaveAction && sectorCleaveAction->isChecked())
                      << " documentUndoDepth=" << window->undoDepthForTest()
                      << " expectedDocumentUndoDepth=" << documentUndoDepth
                      << " depthBeforeClear=" << documentUndoDepthBeforeClear
                      << " depthAfterClear=" << documentUndoDepthAfterClear
                      << " depthAfterClearUndo=" << documentUndoDepthAfterClearUndo
                      << " selectedDocumentSectors=";
            for ( const std::int32_t sectorIndex
                  : window->documentStateForTest().selection.sectors )
            {
                std::cerr << sectorIndex << ',';
            }
            const auto printContext = [](
                                         const char *const label,
                                         const GlView::RecoveryState::CleaveContext &context)
            {
                std::cerr << ' ' << label << "{valid=" << context.valid
                          << " surface=" << context.surfaceMode << " targets=";
                for ( const std::int32_t sectorIndex : context.targetSectors )
                {
                    std::cerr << sectorIndex << ',';
                }
                std::cerr << " sourceSurface=" << context.sourceSurface.sector << ':'
                          << context.sourceSurface.surface << '}';
            };
            std::cerr << " linesBefore=" << draftBeforeClear.cleaveLines.size()
                      << " linesAfterClear=" << draftAfterClear.cleaveLines.size()
                      << " linesAfterUndo=" << draftAfterClearUndo.cleaveLines.size();
            printContext(" before", draftBeforeClear.cleaveContext);
            printContext(" afterClear", draftAfterClear.cleaveContext);
            printContext(" afterUndo", draftAfterClearUndo.cleaveContext);
            std::cerr << '\n';
        }
        check(clearUndoRestored && window->undoDepthForTest() == documentUndoDepth,
              "one local Undo restores a cleared Multi-cut draft");

        const auto draftBeforeApply = view->recoveryState();
        const editor::Selection selectionBeforeApply =
            window->documentStateForTest().selection;
        const std::size_t sectorsBeforeApply = window->documentStateForTest().level.sectors.size();
        view->requestApplyStagedCleave();
        const std::size_t undoDepthAfterApply = window->undoDepthForTest();
        check(
            view->stagedCleaveLineCount() == 0 && undoDepthAfterApply == documentUndoDepth + 1 &&
                window->documentStateForTest().level.sectors.size() > sectorsBeforeApply,
            "successful Multi-cut Apply ends local history and publishes one document transaction");
        if ( undoAction && undoAction->isEnabled() )
        {
            undoAction->trigger();
        }
        const auto draftAfterApplyUndo = view->recoveryState();
        const editor::Selection selectionAfterApplyUndo =
            window->documentStateForTest().selection;
        const bool selectionRestored =
            selectionAfterApplyUndo.sectors == selectionBeforeApply.sectors &&
            selectionAfterApplyUndo.surfaces == selectionBeforeApply.surfaces &&
            selectionAfterApplyUndo.surfaceVertices == selectionBeforeApply.surfaceVertices &&
            selectionAfterApplyUndo.things == selectionBeforeApply.things &&
            selectionAfterApplyUndo.frames == selectionBeforeApply.frames &&
            selectionAfterApplyUndo.lights == selectionBeforeApply.lights &&
            selectionAfterApplyUndo.cogs == selectionBeforeApply.cogs;
        check(window->documentStateForTest().level.sectors.size() == sectorsBeforeApply &&
                  window->undoDepthForTest() == documentUndoDepth &&
                  sameCleaveDraft(draftAfterApplyUndo, draftBeforeApply) &&
                  sameCleaveOptions(draftAfterApplyUndo, draftBeforeApply) &&
                  draftAfterApplyUndo.selectedCleaveLines ==
                      draftBeforeApply.selectedCleaveLines &&
                  draftAfterApplyUndo.selectedCleaveLine ==
                      draftBeforeApply.selectedCleaveLine &&
                  selectionRestored,
              "Undo after successful Apply restores the exact staged draft, line selection, options, "
              "document selection, and applied Level geometry");

    }

    {
        auto cancelWindow = std::make_unique<MainWindow>();
        cancelWindow->setAttribute(Qt::WA_DontShowOnScreen);
        cancelWindow->resize(1280, 900);
        cancelWindow->activateSectorEditModeForTest();
        editor::DocumentState state{};
        state.level.kind = ProjectType::IJIM;
        makeTestCube(state.level);
        state.selection.sectors.insert(0);
        cancelWindow->setDocumentStateForTest(std::move(state));
        cancelWindow->activateSectorEditModeForTest();
        cancelWindow->setLayerVisualStateForTest({true}, {false});
        const bool cancelDocumentHistorySeeded = cancelWindow->applyStructuralHistoryEditForTest(
            [](editor::DocumentState &edited)
            {
                edited.level.header.gravity += 0.25;
                return true;
            });
        check(cancelDocumentHistorySeeded,
              "isolated Cancel fixture commits a real Level-header edit before enabling Cleave");
        GlView *const cancelView = cancelWindow->glViewForTest();
        if ( !cancelView )
        {
            check(false, "empty-session Cancel fixture has a viewport");
            return;
        }

        cancelView->setLevel(
            &cancelWindow->documentStateForTest().level,
            GlView::SetLevelMode::resetView);
        cancelView->setSelectionState(
            0,
            -1,
            -1,
            -1,
            cancelWindow->documentStateForTest().selection);
        cancelView->setCameraProjectionType(
            sed::app::ViewportCamera::ProjectionType::Orthographic);
        cancelView->resetEditorTopDownView();
        cancelView->setCameraOrthoScale(32.0f);
        configureQuietView(*cancelView);
        cancelWindow->show();
        cancelWindow->activateWindow();
        QApplication::processEvents(QEventLoop::AllEvents);
        QApplication::setActiveWindow(cancelWindow.get());
        cancelView->setFocus(Qt::OtherFocusReason);
        QApplication::processEvents(QEventLoop::AllEvents);
        check(QApplication::focusWidget() == cancelView,
              "isolated empty-session fixture gives keyboard focus to the viewport");

        cancelWindow->updateEditActionsForTest();
        check(cancelWindow->documentStateForTest().selection.sectors.size() == 1
                  && cancelWindow->documentStateForTest().selection.sectors.contains(0),
              "isolated Cancel fixture retains Sector 0 in the document selection");
        QToolButton *const cancelMultiCutButton = cancelWindow->findChild<QToolButton *>(
            QStringLiteral("cleaveMultiCutButton"));
        QToolButton *const cancelChainButton = cancelWindow->findChild<QToolButton *>(
            QStringLiteral("cleaveChainButton"));
        check(cancelMultiCutButton && cancelChainButton,
              "isolated Cancel fixture exposes actual Multi-cut options");
        if ( cancelMultiCutButton )
        {
            cancelMultiCutButton->setChecked(true);
        }
        if ( cancelChainButton )
        {
            cancelChainButton->setChecked(false);
        }
        QAction *const cancelCleaveAction = cancelWindow->findChild<QAction *>(
            QStringLiteral("viewport.sector.cleave"));
        check(cancelCleaveAction && cancelCleaveAction->isEnabled(),
              "isolated empty-session fixture exposes the active Sector Cleave command");
        if ( cancelCleaveAction && !cancelCleaveAction->isChecked() )
        {
            cancelCleaveAction->setChecked(true);
        }
        check(cancelMultiCutButton && cancelMultiCutButton->isChecked()
                  && cancelCleaveAction && cancelCleaveAction->isChecked()
                  && cancelView->cleaveStagedMode()
                  && cancelView->sectorCleaveToolEnabled(),
              "actual Multi-cut and Sector Cleave controls arm the isolated viewport");

        const auto drawCancelLine = [cancelView]()
        {
            QVector3D right = cancelView->cameraRight();
            QVector3D up = cancelView->cameraUp();
            right.normalize();
            up.normalize();
            const QVector3D center = cancelView->cameraTarget();
            const auto first = project(
                *cancelView,
                center - right * 1.5f + up * 0.4f);
            const auto second = project(
                *cancelView,
                center + right * 1.5f + up * 0.4f);
            if ( !first || !second )
            {
                return false;
            }
            sendDrag(*cancelView, *first, {*second}, Qt::NoModifier);
            return true;
        };

        QAction *const cancelUndoAction = cancelWindow->findChild<QAction *>(
            QStringLiteral("edit.undo"));
        cancelWindow->updateEditActionsForTest();
        check(cancelWindow->undoDepthForTest() > 0
                  && cancelUndoAction && cancelUndoAction->isEnabled(),
              "isolated empty-session fixture starts with enabled document Undo");
        check(drawCancelLine() && cancelView->stagedCleaveLineCount() == 1,
              "isolated empty-session fixture creates a first pending line");
        QAction *const viewportCancelAction = cancelWindow->findChild<QAction *>(
            QStringLiteral("viewport.cancel"));
        const auto beforeEscapeGesture = cancelView->recoveryState();
        std::optional<QPointF> escapeGestureStart{};
        if ( beforeEscapeGesture.cleaveLines.size() == 1 )
        {
            escapeGestureStart = project(
                *cancelView,
                (toQVector3D(beforeEscapeGesture.cleaveLines[0].first) +
                 toQVector3D(beforeEscapeGesture.cleaveLines[0].second)) * 0.5f);
        }
        const QPointF escapeGestureEnd = escapeGestureStart
            ? *escapeGestureStart + QPointF(0.0, -30.0)
            : QPointF{};
        std::optional<QPointF> escapeLineFirst{};
        std::optional<QPointF> escapeLineSecond{};
        if ( beforeEscapeGesture.cleaveLines.size() == 1 )
        {
            escapeLineFirst = project(
                *cancelView,
                toQVector3D(beforeEscapeGesture.cleaveLines[0].first));
            escapeLineSecond = project(
                *cancelView,
                toQVector3D(beforeEscapeGesture.cleaveLines[0].second));
        }
        const bool escapeBodyClearOfEndpoints = escapeGestureStart
            && escapeLineFirst && escapeLineSecond
            && std::hypot(
                   escapeGestureStart->x() - escapeLineFirst->x(),
                   escapeGestureStart->y() - escapeLineFirst->y()) > 10.0
            && std::hypot(
                   escapeGestureStart->x() - escapeLineSecond->x(),
                   escapeGestureStart->y() - escapeLineSecond->y()) > 10.0;
        if ( !escapeBodyClearOfEndpoints || !viewportCancelAction )
        {
            const auto distance = [](const QPointF& left, const QPointF& right)
            {
                return std::hypot(left.x() - right.x(), left.y() - right.y());
            };
            std::cerr << "Escape body projection diagnostic: action="
                      << (viewportCancelAction ? 1 : 0)
                      << " cameraOrthoScale=" << cancelView->cameraOrthoScale()
                      << " cameraDistance=" << cancelView->cameraOrbitDistance();
            if ( escapeGestureStart && escapeLineFirst && escapeLineSecond )
            {
                std::cerr << " midpoint=(" << escapeGestureStart->x() << ", "
                          << escapeGestureStart->y() << ") first=("
                          << escapeLineFirst->x() << ", " << escapeLineFirst->y()
                          << ") second=(" << escapeLineSecond->x() << ", "
                          << escapeLineSecond->y() << ") endpointDistances=("
                          << distance(*escapeGestureStart, *escapeLineFirst) << ", "
                          << distance(*escapeGestureStart, *escapeLineSecond) << ")";
            }
            else
            {
                std::cerr << " missingProjectedPoint=1";
            }
            std::cerr << '\n';
        }
        check(escapeBodyClearOfEndpoints && viewportCancelAction,
              "active-gesture Escape fixture exposes an unambiguous line body and WindowShortcut action");
        if ( escapeBodyClearOfEndpoints && viewportCancelAction )
        {
            sendMouseEvent(*cancelView,
                           QEvent::MouseButtonPress,
                           *escapeGestureStart,
                           Qt::LeftButton,
                           Qt::LeftButton,
                           Qt::NoModifier);
            check(cancelView->stagedCleavePlaneDraggingForTest(),
                  "active-gesture Escape press acquires the staged line body");
            sendMouseEvent(*cancelView,
                           QEvent::MouseMove,
                           escapeGestureEnd,
                           Qt::NoButton,
                           Qt::LeftButton,
                           Qt::NoModifier);
            const bool liveEndpointMoved =
                !closeQVec(
                    cancelView->stagedCleaveLineFirstForTest(0),
                    toQVector3D(beforeEscapeGesture.cleaveLines[0].first),
                    1.0e-5f)
                || !closeQVec(
                    cancelView->stagedCleaveLineSecondForTest(0),
                    toQVector3D(beforeEscapeGesture.cleaveLines[0].second),
                    1.0e-5f);
            check(liveEndpointMoved && cancelView->stagedCleavePlaneDraggingForTest(),
                  "active-gesture Escape fixture previews a real staged-line move");
            viewportCancelAction->trigger();
            sendMouseEvent(*cancelView,
                           QEvent::MouseButtonRelease,
                           escapeGestureEnd,
                           Qt::LeftButton,
                           Qt::NoButton,
                           Qt::NoModifier);
            QApplication::processEvents(QEventLoop::AllEvents);
            const auto afterEscapeGesture = cancelView->recoveryState();
            check(sameCleaveDraft(afterEscapeGesture, beforeEscapeGesture) &&
                      sameCleaveHistory(afterEscapeGesture.cleaveHistory,
                                        beforeEscapeGesture.cleaveHistory) &&
                      afterEscapeGesture.selectedCleaveLines ==
                          beforeEscapeGesture.selectedCleaveLines &&
                      cancelView->cleaveStagedMode() &&
                      cancelView->sectorCleaveToolEnabled() &&
                      cancelView->stagedCleaveHistoryOwnsUndo() &&
                      cancelView->canUndoStagedCleaveHistory(),
                  "Escape through viewport.cancel restores an active gesture baseline and keeps "
                  "the Multi-cut tool armed");
        }
        if ( cancelUndoAction && cancelUndoAction->isEnabled() )
        {
            cancelUndoAction->trigger();
        }
        QApplication::processEvents(QEventLoop::AllEvents);
        check(cancelView->stagedCleaveLineCount() == 0
                  && cancelView->stagedCleaveHistoryOwnsUndo()
                  && !cancelView->canUndoStagedCleaveHistory()
                  && cancelView->canRedoStagedCleaveHistory()
                  && cancelUndoAction && !cancelUndoAction->isEnabled(),
              "Undo of the first staged line owns an empty local baseline over document Undo");
        cancelView->cancelStagedCleave();
        QApplication::processEvents(QEventLoop::AllEvents);
        check(!cancelView->stagedCleaveHistoryOwnsUndo()
                  && cancelWindow->undoDepthForTest() > 0
                  && cancelUndoAction && cancelUndoAction->isEnabled(),
              "Cancel refreshes Undo to the available document history after an empty local session");
    }

    {
        Level level{};
        makeTestCube(level);
        editor::Selection targets{};
        targets.sectors.insert(0);
        GlView view{};
        view.setLevel(&level);
        view.setSelectionState(0, -1, -1, -1, targets);
        view.setCleaveStagedMode(true);
        view.setSectorCleaveToolEnabled(true);
        view.captureStagedCleaveContextForTest(false);
        const QVector3D first(-0.75f, 0.0f, 0.0f);
        const QVector3D second(0.25f, 0.0f, 0.0f);
        view.addStagedCleaveLineForTest(
            first,
            second,
            QVector3D(0.0f, 0.0f, 1.0f));
        const GlView::RecoveryState::CleaveDraftSnapshot beforeEdit =
            view.captureStagedCleaveDraftState();
        GlView::RecoveryState::CleaveDraftSnapshot afterEdit = beforeEdit;
        afterEdit.lines[0].first.x += 0.375;
        afterEdit.lines[0].second.x += 0.375;
        GlView::RecoveryState::CleaveHistory history{};
        history.snapshots = { beforeEdit, afterEdit };
        history.cursor = 1;
        view.restoreStagedCleaveDraftState(afterEdit, history);
        check(view.isStagedCleaveInteractionActive()
                  && view.stagedCleaveHistoryOwnsUndo(),
              "authored revision fixture has the matching active Sector Cleave owner");

        std::vector<GlView::RecoveryState::CleaveDraftSnapshot> publishedDrafts{};
        std::vector<std::uint64_t> publishedRevisions{};
        QObject::connect(
            &view,
            &GlView::stagedCleaveAuthoredStateChanged,
            &view,
            [&view, &publishedDrafts, &publishedRevisions](
                const std::uint64_t revision)
            {
                publishedRevisions.push_back(revision);
                publishedDrafts.push_back(view.captureStagedCleaveDraftState());
            });

        const std::uint64_t revisionBeforeUndo = view.stagedCleaveAuthoredRevision();
        const bool undoApplied = view.undoStagedCleaveHistory();
        const auto afterUndo = view.captureStagedCleaveDraftState();
        check(undoApplied
                  && publishedDrafts.size() == 1
                  && sameCleaveSnapshot(publishedDrafts.front(), beforeEdit)
                  && sameCleaveSnapshot(afterUndo, beforeEdit)
                  && publishedRevisions.front() == revisionBeforeUndo + 1
                  && publishedRevisions.front() == view.stagedCleaveAuthoredRevision(),
              "local Undo publishes its restored authored snapshot under the matching revision");

        const std::uint64_t revisionBeforeRedo = view.stagedCleaveAuthoredRevision();
        const bool redoApplied = view.redoStagedCleaveHistory();
        const auto afterRedo = view.captureStagedCleaveDraftState();
        check(redoApplied
                  && publishedDrafts.size() == 2
                  && sameCleaveSnapshot(publishedDrafts.back(), afterEdit)
                  && sameCleaveSnapshot(afterRedo, afterEdit)
                  && publishedRevisions.back() == revisionBeforeRedo + 1
                  && publishedRevisions.back() == view.stagedCleaveAuthoredRevision(),
              "local Redo publishes its restored authored snapshot under the matching revision");

        const std::uint64_t revisionBeforeCancel = view.stagedCleaveAuthoredRevision();
        view.cancelStagedCleave();
        const auto afterCancel = view.captureStagedCleaveDraftState();
        check(publishedDrafts.size() == 3
                  && publishedDrafts.back().lines.empty()
                  && publishedRevisions.back() == revisionBeforeCancel + 1
                  && publishedRevisions.back() == view.stagedCleaveAuthoredRevision()
                  && afterCancel.lines.empty(),
              "Cancel publishes an authored clear under a new revision exactly once");
    }

    const auto runLargeHistoryCopyRefusal = [](const bool targetIsRedo)
    {
        constexpr std::size_t kHistoryBudget = 16u * 1024u * 1024u;
        constexpr std::size_t kTargetPayload = 9u * 1024u * 1024u;
        using Snapshot = GlView::RecoveryState::CleaveDraftSnapshot;
        using CleaveLine = GlView::RecoveryState::CleaveLine;

        Level level{};
        makeTestCube(level);
        editor::Selection targets{};
        targets.sectors.insert(0);

        GlView view{};
        view.setAttribute(Qt::WA_DontShowOnScreen);
        view.resize(640, 480);
        view.setLevel(&level);
        view.setSelectionState(0, -1, -1, -1, targets);
        view.setCleaveStagedMode(true);
        view.setCleaveChainEnabled(true);
        view.setSectorCleaveToolEnabled(true);
        view.setCameraProjectionType(
            sed::app::ViewportCamera::ProjectionType::Perspective);
        configureQuietView(view);
        view.show();
        QApplication::processEvents(QEventLoop::AllEvents);
        view.captureStagedCleaveContextForTest(false);

        QVector3D right = view.cameraRight();
        QVector3D up = view.cameraUp();
        QVector3D forward = view.cameraForward();
        right.normalize();
        up.normalize();
        forward.normalize();
        const QVector3D center = view.cameraTarget();
        view.addStagedCleaveLineForTest(
            center - right * 0.60f - up * 0.20f,
            center - right * 0.05f - up * 0.20f,
            forward);

        const Snapshot authored = view.captureStagedCleaveDraftState();
        check(authored.context.valid && authored.context.targetSectors.size() == 1u
                  && authored.context.targetSectors.front() == 0
                  && authored.lines.size() == 1u,
              "the large-history fixture starts from a valid authored target context");
        if (!authored.context.valid || authored.lines.empty())
        {
            return;
        }

        const std::size_t bytesPerLine = sizeof(CleaveLine) + sizeof(std::int32_t);
        const std::size_t lineCount =
            (kTargetPayload + bytesPerLine - 1u) / bytesPerLine;
        Snapshot large = authored;
        large.lines.resize(lineCount, authored.lines.front());
        large.applyOrder.resize(lineCount);
        for (std::size_t index = 0; index < lineCount; ++index)
        {
            large.applyOrder[index] = static_cast<std::int32_t>(index);
        }

        Snapshot empty = authored;
        empty.lines.clear();
        empty.applyOrder.clear();
        empty.selectedCleaveLines.clear();
        empty.selectedCleaveLine = -1;
        empty.chainOrigin.reset();

        GlView::RecoveryState::CleaveHistory history{};
        history.snapshots.reserve(2u);
        if (targetIsRedo)
        {
            history.snapshots.push_back(empty);
            history.snapshots.push_back(std::move(large));
            history.cursor = 0u;
        }
        else
        {
            history.snapshots.push_back(std::move(large));
            history.snapshots.push_back(empty);
            history.cursor = 1u;
        }
        auto liveEmptyState = empty;
        view.restoreStagedCleaveDraftState(
            std::move(liveEmptyState), std::move(history));

        const std::size_t storageBefore =
            view.stagedCleaveHistoryStorageBytesEstimate();
        const bool canUndoBefore = view.canUndoStagedCleaveHistory();
        const bool canRedoBefore = view.canRedoStagedCleaveHistory();
        const bool actionInitiallyAvailable = targetIsRedo
            ? canRedoBefore
            : canUndoBefore;
        const std::uint64_t revisionBefore = view.stagedCleaveAuthoredRevision();
        check(storageBefore > 8u * 1024u * 1024u
                  && storageBefore <= kHistoryBudget
                  && actionInitiallyAvailable,
              targetIsRedo
                  ? "the valid 9 MiB Redo target is initially available within budget"
                  : "the valid 9 MiB Undo target is initially available within budget");

        const bool changed = targetIsRedo
            ? view.redoStagedCleaveHistory()
            : view.undoStagedCleaveHistory();
        const std::size_t storageAfter =
            view.stagedCleaveHistoryStorageBytesEstimate();
        const bool capabilitiesUnchanged = view.canUndoStagedCleaveHistory() == canUndoBefore
            && view.canRedoStagedCleaveHistory() == canRedoBefore;
        check(actionInitiallyAvailable && !changed
                  && view.stagedCleaveLineCount() == 0u
                  && capabilitiesUnchanged
                  && view.stagedCleaveAuthoredRevision() == revisionBefore
                  && storageAfter <= kHistoryBudget,
              targetIsRedo
                  ? "over-budget Redo refusal preserves cursor, revision and empty live draft"
                  : "over-budget Undo refusal preserves cursor, revision and empty live draft");
    };

    runLargeHistoryCopyRefusal(false);
    runLargeHistoryCopyRefusal(true);

    {
        constexpr std::size_t kHistoryBudget = 16u * 1024u * 1024u;
        using Snapshot = GlView::RecoveryState::CleaveDraftSnapshot;
        using CleaveLine = GlView::RecoveryState::CleaveLine;

        const auto ownedSnapshotBytes = [](const Snapshot& state)
        {
            return sizeof(Snapshot)
                + state.lines.capacity() * sizeof(CleaveLine)
                + state.context.targetSectors.capacity() * sizeof(std::int32_t)
                + state.context.targetSurfaces.capacity() * sizeof(SurfaceRef)
                + state.applyOrder.capacity() * sizeof(std::int32_t)
                + state.selectedCleaveLines.capacity() * sizeof(std::int32_t);
        };

        Level level{};
        makeTestCube(level);
        editor::Selection targets{};
        targets.sectors.insert(0);

        GlView view{};
        view.setAttribute(Qt::WA_DontShowOnScreen);
        view.resize(640, 480);
        view.setLevel(&level);
        view.setSelectionState(0, -1, -1, -1, targets);
        view.setCleaveStagedMode(true);
        view.setCleaveChainEnabled(true);
        view.setSectorCleaveToolEnabled(true);
        view.setCameraProjectionType(
            sed::app::ViewportCamera::ProjectionType::Perspective);
        configureQuietView(view);
        view.show();
        QApplication::processEvents(QEventLoop::AllEvents);
        view.captureStagedCleaveContextForTest(false);

        QVector3D right = view.cameraRight();
        QVector3D up = view.cameraUp();
        QVector3D forward = view.cameraForward();
        right.normalize();
        up.normalize();
        forward.normalize();
        const QVector3D center = view.cameraTarget();
        view.addStagedCleaveLineForTest(
            center - right * 0.60f - up * 0.20f,
            center - right * 0.05f - up * 0.20f,
            forward);
        const Snapshot seed = view.captureStagedCleaveDraftState();
        check(seed.context.valid && seed.context.targetSectors.size() == 1u
                  && seed.context.targetSectors.front() == 0 && seed.lines.size() == 1u,
              "near-limit mutation fixtures start from a valid authored Cleave target");
        if ( !seed.context.valid || seed.lines.size() != 1u )
        {
            return;
        }

        const auto makeLargeSnapshot = [&seed](const std::size_t count)
        {
            Snapshot large = seed;
            large.lines.resize(count, seed.lines.front());
            large.applyOrder.resize(count);
            for ( std::size_t index = 0; index < count; ++index )
            {
                large.applyOrder[index] = static_cast<std::int32_t>(index);
            }
            large.selectedCleaveLines = {0};
            large.selectedCleaveLine = 0;
            return large;
        };
        const auto installTwoStateTimeline = [&view, &makeLargeSnapshot](
                                                const std::size_t count)
        {
            Snapshot large = makeLargeSnapshot(count);
            GlView::RecoveryState::CleaveHistory history{};
            history.snapshots.reserve(2u);
            history.snapshots.push_back(large);
            history.snapshots.push_back(large);
            history.cursor = 1u;
            view.restoreStagedCleaveDraftState(std::move(large), std::move(history));
        };

        const std::size_t linePayloadBytes = sizeof(CleaveLine)
            + sizeof(std::int32_t);
        const std::size_t descriptorReserveBytes =
            64u * sizeof(Snapshot) + 256u * 1024u;
        const std::size_t deleteLineCount =
            (kHistoryBudget - descriptorReserveBytes)
            / (4u * linePayloadBytes);
        installTwoStateTimeline(deleteLineCount);
        const std::size_t deleteStorageBefore =
            view.stagedCleaveHistoryStorageBytesEstimate();
        const Snapshot deleteShape = makeLargeSnapshot(deleteLineCount);
        const std::size_t deletePressBaselineBytes =
            ownedSnapshotBytes(deleteShape) + 64u * sizeof(Snapshot);
        const std::size_t deleteCandidateBytes = deleteLineCount
            * (sizeof(CleaveLine) + 2u * sizeof(std::int32_t) + 1u);
        const bool deleteFixtureFitsPressBaseline = deleteStorageBefore <= kHistoryBudget
            && deletePressBaselineBytes <= kHistoryBudget - deleteStorageBefore;
        const bool deleteCandidateCrossesBudget = deleteFixtureFitsPressBaseline
            && deleteCandidateBytes
                > kHistoryBudget - deleteStorageBefore - deletePressBaselineBytes;
        const bool deleteInitiallyAvailable = view.canUndoStagedCleaveHistory()
            && !view.canRedoStagedCleaveHistory();
        const std::uint64_t revisionBeforeDelete = view.stagedCleaveAuthoredRevision();
        const QVector3D firstBeforeDelete = view.stagedCleaveLineFirstForTest(0u);
        check(deleteFixtureFitsPressBaseline && deleteCandidateCrossesBudget
                  && deleteInitiallyAvailable
                  && view.stagedCleaveLineCount() == deleteLineCount,
              "near-limit Delete fixture admits its gesture baseline while candidate arrays cross the 16 MiB cap");
        if ( !deleteFixtureFitsPressBaseline || !deleteCandidateCrossesBudget
            || !deleteInitiallyAvailable )
        {
            std::cerr << "Cleave Delete admission fixture: count=" << deleteLineCount
                      << " storage=" << deleteStorageBefore
                      << " pressBaseline=" << deletePressBaselineBytes
                      << " candidates=" << deleteCandidateBytes
                      << " limit=" << kHistoryBudget
                      << " canUndo=" << view.canUndoStagedCleaveHistory()
                      << " canRedo=" << view.canRedoStagedCleaveHistory() << '\n';
            return;
        }

        const bool deleteChanged = view.deleteSelectedStagedCleaveLine();
        check(!deleteChanged && view.stagedCleaveLineCount() == deleteLineCount
                  && view.stagedCleaveLineFirstForTest(0u) == firstBeforeDelete
                  && view.stagedCleaveAuthoredRevision() == revisionBeforeDelete
                  && view.canUndoStagedCleaveHistory()
                  && !view.canRedoStagedCleaveHistory(),
              "Delete refuses before allocating over-budget remap arrays and preserves the draft and timeline");

        const std::size_t finishLineCount = deleteLineCount;
        installTwoStateTimeline(finishLineCount);
        const std::size_t finishStorageBefore =
            view.stagedCleaveHistoryStorageBytesEstimate();
        const Snapshot finishShape = makeLargeSnapshot(finishLineCount);
        const std::size_t finishPressBaselineBytes =
            ownedSnapshotBytes(finishShape) + 64u * sizeof(Snapshot);
        const bool finishPressFitsBudget = finishStorageBefore <= kHistoryBudget
            && finishPressBaselineBytes <= kHistoryBudget - finishStorageBefore;
        const bool finishHasLocalUndo = view.canUndoStagedCleaveHistory()
            && !view.canRedoStagedCleaveHistory();
        const std::uint64_t revisionBeforeFinish = view.stagedCleaveAuthoredRevision();
        const QVector3D finishFirstEndpoint = view.stagedCleaveLineFirstForTest(0u);
        const auto finishPressPoint = project(view, finishFirstEndpoint);
        check(finishPressFitsBudget && finishHasLocalUndo && finishPressPoint,
              "near-limit gesture fixture admits a real press-time rollback baseline");
        if ( !finishPressFitsBudget || !finishHasLocalUndo || !finishPressPoint )
        {
            std::cerr << "Cleave finish admission fixture: count=" << finishLineCount
                      << " storage=" << finishStorageBefore
                      << " pressBaseline=" << finishPressBaselineBytes
                      << " limit=" << kHistoryBudget
                      << " projected=" << finishPressPoint.has_value() << '\n';
            return;
        }

        sendMouseEvent(
            view,
            QEvent::MouseButtonPress,
            *finishPressPoint,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        const std::size_t finishStorageDuringGesture =
            view.stagedCleaveHistoryStorageBytesEstimate();
        const std::size_t finishCandidateCopyBytes = ownedSnapshotBytes(finishShape);
        const bool finishCandidateCrossesBudget = finishStorageDuringGesture
                <= kHistoryBudget
            && finishCandidateCopyBytes
                > kHistoryBudget - finishStorageDuringGesture;
        check(finishStorageDuringGesture > finishStorageBefore
                  && finishCandidateCrossesBudget,
              "the real endpoint press retains its baseline while the finish snapshot copy would exceed 16 MiB");
        sendMouseEvent(
            view,
            QEvent::MouseButtonRelease,
            *finishPressPoint,
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::processEvents(QEventLoop::AllEvents);
        const Snapshot draftAfterFinish = view.captureStagedCleaveDraftState();
        const auto historyAfterFinish = view.captureStagedCleaveHistory();
        const bool exactTimelineAfterFinish = historyAfterFinish
            && historyAfterFinish->cursor == 1u
            && historyAfterFinish->snapshots.size() == 2u
            && sameCleaveSnapshot(historyAfterFinish->snapshots[0], finishShape)
            && sameCleaveSnapshot(historyAfterFinish->snapshots[1], finishShape);
        check(sameCleaveSnapshot(draftAfterFinish, finishShape)
                  && exactTimelineAfterFinish
                  && view.stagedCleaveLineCount() == finishLineCount
                  && view.stagedCleaveLineFirstForTest(0u) == finishFirstEndpoint
                  && view.stagedCleaveAuthoredRevision() == revisionBeforeFinish
                  && view.canUndoStagedCleaveHistory()
                  && !view.canRedoStagedCleaveHistory()
                  && !view.cleaveStagedEndpointEditing()
                  && view.stagedCleaveHistoryStorageBytesEstimate() <= kHistoryBudget,
              "a refused near-limit finish restores the exact live draft and keeps its local history branch");

        const std::size_t rankLineCount = kHistoryBudget
            / (3u * linePayloadBytes + sizeof(std::int32_t) + 2u);
        installTwoStateTimeline(rankLineCount);
        view.setCleaveLineNumbersVisible(true);
        const QRect rankLabel = view.stagedCleaveOrderLabelRectForTest(0);
        const std::size_t rankStorageBefore =
            view.stagedCleaveHistoryStorageBytesEstimate();
        const std::size_t rankBaselineBytes =
            rankLineCount * sizeof(std::int32_t);
        const bool rankFixtureCrossesBudget = !rankLabel.isEmpty()
            && rankStorageBefore <= kHistoryBudget
            && rankBaselineBytes > kHistoryBudget - rankStorageBefore;
        const bool rankInitiallyAvailable = view.canUndoStagedCleaveHistory()
            && !view.canRedoStagedCleaveHistory();
        const std::uint64_t revisionBeforeRankEdit = view.stagedCleaveAuthoredRevision();
        check(rankFixtureCrossesBudget && rankInitiallyAvailable,
              "near-limit Apply-order fixture admits the draft but not its editor baseline copy");
        if ( !rankFixtureCrossesBudget || !rankInitiallyAvailable )
        {
            std::cerr << "Cleave order admission fixture: count=" << rankLineCount
                      << " storage=" << rankStorageBefore
                      << " baseline=" << rankBaselineBytes
                      << " limit=" << kHistoryBudget
                      << " labelEmpty=" << rankLabel.isEmpty() << '\n';
            return;
        }

        sendClick(view, rankLabel.center());
        QLineEdit *const orderEditor = view.findChild<QLineEdit *>(
            QStringLiteral("cleaveLineOrderEditor"));
        const std::size_t rankStorageAfter =
            view.stagedCleaveHistoryStorageBytesEstimate();
        check(orderEditor == nullptr && rankStorageAfter <= kHistoryBudget
                  && view.stagedCleaveLineCount() == rankLineCount
                  && view.stagedCleaveAuthoredRevision() == revisionBeforeRankEdit
                  && view.canUndoStagedCleaveHistory()
                  && !view.canRedoStagedCleaveHistory(),
              "Apply-order editing refuses its over-budget baseline before opening the editor");
        if ( orderEditor )
        {
            sendKey(*orderEditor, QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            sendKey(*orderEditor, QEvent::KeyRelease, Qt::Key_Escape, Qt::NoModifier);
            QApplication::processEvents(QEventLoop::AllEvents);
        }
    }
}

} // namespace sed::app_tests
