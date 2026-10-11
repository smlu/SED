#include "test_common.hpp"

#include "gl_view.hpp"
#include "sed/level/build.hpp"

#include <QApplication>
#include <QEventLoop>
#include <QMouseEvent>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace sed::app_tests
{
namespace
{

void sendMouseEvent(
    GlView& view,
    const QEvent::Type type,
    const QPointF& position,
    const Qt::MouseButton button,
    const Qt::MouseButtons buttons)
{
    const QPointF globalPosition = view.mapToGlobal(position.toPoint());
    QMouseEvent event(
        type,
        position,
        globalPosition,
        globalPosition,
        button,
        buttons,
        Qt::NoModifier);
    QApplication::sendEvent(&view, &event);
}

void sendClick(GlView& view, const QPointF& position)
{
    sendMouseEvent(
        view,
        QEvent::MouseButtonPress,
        position,
        Qt::LeftButton,
        Qt::LeftButton);
    sendMouseEvent(
        view,
        QEvent::MouseButtonRelease,
        position,
        Qt::LeftButton,
        Qt::NoButton);
    QApplication::processEvents(QEventLoop::AllEvents);
}

} // namespace

void runCleaveChainBodyRecoveryTests()
{
    Level originLevel{};
    level::levelCreateCube(
        originLevel,
        0.0,
        0.0,
        0.0,
        TVector::up,
        TVector::right).layer = 0;
    const Sector& sourceSector = originLevel.sectors.front();
    const std::int32_t faceVertexIndex = sourceSector.surfaces.front().vertices.front();
    const float faceZ = static_cast<float>(
        sourceSector.vertices[static_cast<std::size_t>(faceVertexIndex)].z);

    editor::Selection selection{};
    selection.surfaces.insert(SurfaceRef{ 0, 0 });

    GlView view{};
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.resize(640, 480);
    view.setLevel(&originLevel);
    view.setSelectionState(0, 0, -1, -1, selection);
    view.setSurfaceSubMode(GlView::SurfaceSubMode::Face);
    view.setSurfaceCleaveToolEnabled(true);
    view.setCleaveStagedMode(true);
    view.setCleaveChainEnabled(true);
    view.setCameraProjectionType(
        sed::app::ViewportCamera::ProjectionType::Orthographic);
    view.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
    view.setCameraOrthoScale(64.0f);
    view.setShowGrid(false);
    view.setShowSolid(false);
    view.setShowTextures(false);
    view.setShowWireframe(false);
    view.setShowVertices(false);
    view.setShowFog(false);
    view.setShowThings(false);
    view.setShowAxisWidget(false);
    view.setShowToastOverlays(false);
    view.show();
    QApplication::processEvents(QEventLoop::AllEvents);
    view.captureStagedCleaveContextForTest(true);

    const QVector3D lineFirst(0.1f, 0.5f, faceZ);
    const QVector3D lineSecond(0.9f, 0.5f, faceZ);
    view.addStagedCleaveLineForTest(
        lineFirst,
        lineSecond,
        QVector3D(0.0f, 1.0f, 0.0f));

    const auto sourceEndpoint = view.projectWorldToViewport(lineFirst);
    const auto otherEndpoint = view.projectWorldToViewport(lineSecond);
    const auto bodyStart = view.projectWorldToViewport(
        (lineFirst + lineSecond) * 0.5f);
    const auto bodyEnd = view.projectWorldToViewport(
        (lineFirst + lineSecond) * 0.5f + QVector3D(-0.35f, 0.25f, 0.0f));
    check(sourceEndpoint && otherEndpoint && bodyStart && bodyEnd,
        "Cleave chain body recovery: source and body gesture project into viewport");
    if ( !sourceEndpoint || !otherEndpoint || !bodyStart || !bodyEnd )
    {
        return;
    }
    const auto screenDistance = [](const QPointF& first, const QPointF& second)
    {
        return std::hypot(first.x() - second.x(), first.y() - second.y());
    };
    const double bodyEndpointClearance = std::min(
        screenDistance(*bodyStart, *sourceEndpoint),
        screenDistance(*bodyStart, *otherEndpoint));
    const double bodyMotionPixels = screenDistance(*bodyStart, *bodyEnd);
    check(bodyEndpointClearance > 12.0 && bodyMotionPixels > 2.0,
        "Cleave chain body recovery: body press stays clear of endpoint handles and motion exceeds click threshold");
    if ( bodyEndpointClearance <= 12.0 || bodyMotionPixels <= 2.0 )
    {
        return;
    }

    sendClick(view, *sourceEndpoint);
    const GlView::RecoveryState afterOriginSelection = view.recoveryState();
    check(afterOriginSelection.cleaveChainOrigin
            && afterOriginSelection.cleaveChainOrigin->lineIndex == 0
            && afterOriginSelection.cleaveChainOrigin->endpoint == 0,
        "Cleave chain body recovery: endpoint click records selected source");
    check(afterOriginSelection.cleaveLines.size() == 1,
        "Cleave chain body recovery: authored source line remains staged");
    if ( afterOriginSelection.cleaveLines.size() != 1 )
    {
        return;
    }
    const GlView::RecoveryState::CleaveLine lineBefore =
        afterOriginSelection.cleaveLines.front();

    const TVector sourceBefore = lineBefore.first;
    int stateChangeCount = 0;
    QObject::connect(
        &view,
        &GlView::viewportStateChanged,
        &view,
        [&stateChangeCount]()
        {
            ++stateChangeCount;
        });

    sendMouseEvent(
        view,
        QEvent::MouseButtonPress,
        *bodyStart,
        Qt::LeftButton,
        Qt::LeftButton);
    const QPointF globalEnd = view.mapToGlobal(bodyEnd->toPoint());
    QMouseEvent move(
        QEvent::MouseMove,
        *bodyEnd,
        globalEnd,
        globalEnd,
        Qt::NoButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(&view, &move);
    QApplication::processEvents(QEventLoop::AllEvents);

    check(stateChangeCount == 0,
        "Cleave chain body recovery: pointer motion does not publish intermediate state");
    sendMouseEvent(
        view,
        QEvent::MouseButtonRelease,
        *bodyEnd,
        Qt::LeftButton,
        Qt::NoButton);
    QApplication::processEvents(QEventLoop::AllEvents);

    const GlView::RecoveryState afterBodyDrag = view.recoveryState();
    const TVector sourceAfter = afterBodyDrag.cleaveLines.empty()
        ? sourceBefore
        : afterBodyDrag.cleaveLines[0].first;
    const TVector secondAfter = afterBodyDrag.cleaveLines.empty()
        ? lineBefore.second
        : afterBodyDrag.cleaveLines[0].second;
    const double sourceDeltaX = sourceAfter.x - sourceBefore.x;
    const double sourceDeltaY = sourceAfter.y - sourceBefore.y;
    const double sourceDeltaZ = sourceAfter.z - sourceBefore.z;
    const double secondDeltaX = secondAfter.x - lineBefore.second.x;
    const double secondDeltaY = secondAfter.y - lineBefore.second.y;
    const double secondDeltaZ = secondAfter.z - lineBefore.second.z;
    const bool sourceMoved = sourceDeltaX * sourceDeltaX
            + sourceDeltaY * sourceDeltaY
            + sourceDeltaZ * sourceDeltaZ
        > 1.0e-4;
    const double translationDifferenceX = sourceDeltaX - secondDeltaX;
    const double translationDifferenceY = sourceDeltaY - secondDeltaY;
    const double translationDifferenceZ = sourceDeltaZ - secondDeltaZ;
    const bool bothEndpointsTranslatedTogether =
        translationDifferenceX * translationDifferenceX
            + translationDifferenceY * translationDifferenceY
            + translationDifferenceZ * translationDifferenceZ
        < 1.0e-6;
    const double beforeDx = lineBefore.second.x - lineBefore.first.x;
    const double beforeDy = lineBefore.second.y - lineBefore.first.y;
    const double beforeDz = lineBefore.second.z - lineBefore.first.z;
    const double afterDx = secondAfter.x - sourceAfter.x;
    const double afterDy = secondAfter.y - sourceAfter.y;
    const double afterDz = secondAfter.z - sourceAfter.z;
    const double lengthBefore = std::sqrt(
        beforeDx * beforeDx + beforeDy * beforeDy + beforeDz * beforeDz);
    const double lengthAfter = std::sqrt(
        afterDx * afterDx + afterDy * afterDy + afterDz * afterDz);
    const bool lineLengthPreserved = std::abs(lengthAfter - lengthBefore) < 1.0e-5;
    check(sourceMoved,
        "Cleave chain body recovery: authored line body drag moves its source endpoint");
    check(afterBodyDrag.cleaveChainOrigin
            && afterBodyDrag.cleaveChainOrigin->lineIndex == 0
            && afterBodyDrag.cleaveChainOrigin->endpoint == 0
            && afterBodyDrag.cleaveLines.size() == 1
            && sourceMoved
            && bothEndpointsTranslatedTogether
            && lineLengthPreserved,
        "Cleave chain body recovery: selected source follows a rigidly moved line without endpoint reshaping");
    check(stateChangeCount == 1,
        "Cleave chain body recovery: completed body drag emits one recovery state change");
}

} // namespace sed::app_tests
