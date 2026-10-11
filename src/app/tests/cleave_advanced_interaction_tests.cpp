#include "test_common.hpp"

#include "gl_view.hpp"
#include "sed/level/build.hpp"
#include "sed/level/cleave.hpp"

#include <QApplication>
#include <QByteArray>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QRectF>
#include <QTimer>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sed::app_tests
{
namespace
{

void sendMouse(
    QWidget& target,
    const QEvent::Type type,
    const QPointF& position,
    const Qt::MouseButton button,
    const Qt::MouseButtons buttons,
    const Qt::KeyboardModifiers modifiers)
{
    const QPointF global = target.mapToGlobal(position.toPoint());
    QMouseEvent event(
        type,
        position,
        global,
        global,
        button,
        buttons,
        modifiers);
    QApplication::sendEvent(&target, &event);
}

void click(
    QWidget& target,
    const QPointF& position,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    sendMouse(
        target,
        QEvent::MouseButtonPress,
        position,
        Qt::LeftButton,
        Qt::LeftButton,
        modifiers);
    sendMouse(
        target,
        QEvent::MouseButtonRelease,
        position,
        Qt::LeftButton,
        Qt::NoButton,
        modifiers);
    QApplication::processEvents(QEventLoop::AllEvents);
}

void drag(
    QWidget& target,
    const QPointF& start,
    const QPointF& end,
    const Qt::KeyboardModifiers modifiers)
{
    sendMouse(
        target,
        QEvent::MouseButtonPress,
        start,
        Qt::LeftButton,
        Qt::LeftButton,
        modifiers);
    sendMouse(
        target,
        QEvent::MouseMove,
        end,
        Qt::NoButton,
        Qt::LeftButton,
        modifiers);
    sendMouse(
        target,
        QEvent::MouseButtonRelease,
        end,
        Qt::LeftButton,
        Qt::NoButton,
        modifiers);
    QApplication::processEvents(QEventLoop::AllEvents);
}

void key(
    QWidget& target,
    const QEvent::Type type,
    const int keyCode,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier,
    const QString& text = {})
{
    QKeyEvent event(type, keyCode, modifiers, text, false, 1);
    QApplication::sendEvent(&target, &event);
}

QVector3D toVector(const TVector& point)
{
    return QVector3D(
        static_cast<float>(point.x),
        static_cast<float>(point.y),
        static_cast<float>(point.z));
}

std::optional<QPointF> project(GlView& view, const QVector3D& point)
{
    return view.projectWorldToViewport(point);
}

QVector3D lineFirst(const GlView::RecoveryState& state, const std::size_t index)
{
    return index < state.cleaveLines.size()
        ? toVector(state.cleaveLines[index].first)
        : QVector3D{};
}

QVector3D lineSecond(const GlView::RecoveryState& state, const std::size_t index)
{
    return index < state.cleaveLines.size()
        ? toVector(state.cleaveLines[index].second)
        : QVector3D{};
}

bool sameCamera(
    const GlView::RecoveryState::CleaveCamera& left,
    const GlView::RecoveryState::CleaveCamera& right)
{
    return left.orbit.center == right.orbit.center
        && left.orbit.pan == right.orbit.pan
        && left.orbit.radius == right.orbit.radius
        && left.orbit.distance == right.orbit.distance
        && left.orbit.yawDeg == right.orbit.yawDeg
        && left.orbit.pitchDeg == right.orbit.pitchDeg
        && left.orbit.rollDeg == right.orbit.rollDeg
        && left.projectionType == right.projectionType
        && left.fovXDeg == right.fovXDeg
        && left.orthoScale == right.orthoScale
        && left.nearPlane == right.nearPlane
        && left.farPlane == right.farPlane
        && left.clipEnabled == right.clipEnabled
        && left.valid == right.valid;
}

bool sameLines(
    const std::vector<GlView::RecoveryState::CleaveLine>& left,
    const std::vector<GlView::RecoveryState::CleaveLine>& right)
{
    if ( left.size() != right.size() )
    {
        return false;
    }
    for ( std::size_t index = 0; index < left.size(); ++index )
    {
        const auto& a = left[index];
        const auto& b = right[index];
        if ( !closeVec(a.first, b.first, 1.0e-8)
            || !closeVec(a.second, b.second, 1.0e-8)
            || !closeVec(a.planeNormal, b.planeNormal, 1.0e-8)
            || !closeVec(a.constructionPlaneNormal, b.constructionPlaneNormal, 1.0e-8)
            || !closeVec(a.angleReferenceAxis, b.angleReferenceAxis, 1.0e-8)
            || !sameCamera(a.authoredCamera, b.authoredCamera)
            || a.linkedToPrevious != b.linkedToPrevious
            || a.linkedToNext != b.linkedToNext )
        {
            return false;
        }
    }
    return true;
}

QVector3D rotateVectorForTest(
    const QVector3D& vector,
    const QVector3D& axis,
    const double angleDegrees)
{
    constexpr double kDegreesToRadians =
        3.14159265358979323846 / 180.0;
    const float angle = static_cast<float>(angleDegrees * kDegreesToRadians);
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const QVector3D normalizedAxis = axis.normalized();
    return vector * cosine
        + QVector3D::crossProduct(normalizedAxis, vector) * sine
        + normalizedAxis * QVector3D::dotProduct(normalizedAxis, vector)
            * (1.0f - cosine);
}

struct CleaveViewportFixture
{
    Level level{};
    GlView view{};
    QVector3D surfaceNormal{};
    bool allBodyPicksClearOfEndpoints{true};

    explicit CleaveViewportFixture(const bool sectorMode = false)
    {
        level.layers.emplace_back("Layer0");
        sed::level::levelCreateCube(
            level,
            -2.0,
            0.0,
            -2.0,
            TVector{0.0, -1.0, 0.0},
            TVector{4.0, 0.0, 0.0}).layer = 0;
        const Surface& face = level.sectors.front().surfaces.front();
        surfaceNormal = toVector(face.normal).normalized();

        editor::Selection selection{};
        if ( sectorMode )
        {
            selection.sectors.insert(0);
        }
        else
        {
            selection.surfaces.insert(SurfaceRef{0, 0});
        }
        view.setAttribute(Qt::WA_DontShowOnScreen);
        view.resize(640, 480);
        view.setLevel(&level);
        view.setSelectionState(0, 0, -1, -1, selection);
        view.setSurfaceSubMode(GlView::SurfaceSubMode::Face);
        view.setSectorCleaveToolEnabled(sectorMode);
        view.setSurfaceCleaveToolEnabled(!sectorMode);
        view.setCleaveStagedMode(true);
        view.setCleaveChainEnabled(false);
        view.setCameraProjectionType(
            sed::app::ViewportCamera::ProjectionType::Orthographic);
        const double toDegrees = 180.0 / 3.14159265358979323846;
        const float yaw = static_cast<float>(
            std::atan2(surfaceNormal.x(), -surfaceNormal.y()) * toDegrees);
        const float pitch = static_cast<float>(
            std::asin(std::clamp(surfaceNormal.z(), -1.0f, 1.0f)) * toDegrees);
        view.setOrbitViewImmediate(yaw, pitch, 0.0f);
        view.setCameraOrbitDistance(8.0f);
        view.setCameraOrthoScale(128.0f);
        view.setGridPlane(GlView::GridPlane::XZ);
        view.setGridMinorSpacing(0.5f);
        view.setGridTransformOrigin(QVector3D{});
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
        QApplication::setActiveWindow(&view);
        view.setFocus(Qt::OtherFocusReason);
        QApplication::processEvents(QEventLoop::AllEvents);
        view.setFocus(Qt::OtherFocusReason);
        QApplication::processEvents(QEventLoop::AllEvents);
        view.captureStagedCleaveContextForTest(!sectorMode);
    }

    QVector3D planeNormalFor(const QVector3D& first, const QVector3D& second) const
    {
        return QVector3D::crossProduct(
            surfaceNormal,
            second - first).normalized();
    }

    void addLine(
        const QVector3D& first,
        const QVector3D& second,
        const bool linkedPrevious = false,
        const bool linkedNext = false)
    {
        view.addStagedCleaveLineForTest(
            first,
            second,
            planeNormalFor(first, second),
            linkedPrevious,
            linkedNext);
    }

    void addLineWithPlaneNormal(
        const QVector3D& first,
        const QVector3D& second,
        const QVector3D& planeNormal)
    {
        view.addStagedCleaveLineForTest(first, second, planeNormal);
    }

    std::optional<QPointF> body(const std::size_t index)
    {
        const GlView::RecoveryState state = view.recoveryState();
        if ( index >= state.cleaveLines.size() )
        {
            return std::nullopt;
        }
        return project(view, (lineFirst(state, index) + lineSecond(state, index)) * 0.5f);
    }

    bool clickBody(const std::size_t index, const Qt::KeyboardModifiers modifiers)
    {
        const auto point = body(index);
        if ( !point )
        {
            return false;
        }
        const GlView::RecoveryState state = view.recoveryState();
        double closestEndpointPixels = std::numeric_limits<double>::infinity();
        for ( const auto& line : state.cleaveLines )
        {
            for ( const TVector* endpoint : {&line.first, &line.second} )
            {
                const auto endpointPosition = project(view, toVector(*endpoint));
                if ( !endpointPosition )
                {
                    allBodyPicksClearOfEndpoints = false;
                    continue;
                }
                closestEndpointPixels = std::min(
                    closestEndpointPixels,
                    std::hypot(point->x() - endpointPosition->x(),
                               point->y() - endpointPosition->y()));
            }
        }
        allBodyPicksClearOfEndpoints = allBodyPicksClearOfEndpoints
            && closestEndpointPixels > 10.0;
        click(view, *point, modifiers);
        return true;
    }

    bool sourceGeometryProjectsInside()
    {
        const GlView::RecoveryState state = view.recoveryState();
        const Surface& sourceSurface = level.sectors.front().surfaces.front();
        const Sector& sourceSector = level.sectors.front();
        double minimumX = std::numeric_limits<double>::infinity();
        double maximumX = -std::numeric_limits<double>::infinity();
        double minimumZ = std::numeric_limits<double>::infinity();
        double maximumZ = -std::numeric_limits<double>::infinity();
        for ( const std::int32_t vertexIndex : sourceSurface.vertices )
        {
            const Vertex& vertex = sourceSector.vertices.at(
                static_cast<std::size_t>(vertexIndex));
            minimumX = std::min(minimumX, vertex.x);
            maximumX = std::max(maximumX, vertex.x);
            minimumZ = std::min(minimumZ, vertex.z);
            maximumZ = std::max(maximumZ, vertex.z);
        }
        const Vertex& firstVertex = sourceSector.vertices.at(
            static_cast<std::size_t>(sourceSurface.vertices.front()));
        const QVector3D faceOrigin(
            static_cast<float>(firstVertex.x),
            static_cast<float>(firstVertex.y),
            static_cast<float>(firstVertex.z));
        for ( const auto& line : state.cleaveLines )
        {
            for ( const TVector* endpoint : {&line.first, &line.second} )
            {
                const QVector3D point = toVector(*endpoint);
                if ( std::abs(QVector3D::dotProduct(
                        point - faceOrigin,
                        surfaceNormal)) > 1.0e-5f
                    || point.x() < minimumX - 1.0e-5
                    || point.x() > maximumX + 1.0e-5
                    || point.z() < minimumZ - 1.0e-5
                    || point.z() > maximumZ + 1.0e-5 )
                {
                    return false;
                }
                const auto screen = project(view, point);
                if ( !screen || !view.rect().contains(screen->toPoint()) )
                {
                    return false;
                }
            }
        }
        return true;
    }

    bool sectorGeometryProjectsInside()
    {
        if ( level.sectors.empty() )
        {
            return false;
        }
        const Sector& sourceSector = level.sectors.front();
        std::array<double, 3> minimum{
            std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity() };
        std::array<double, 3> maximum{
            -std::numeric_limits<double>::infinity(),
            -std::numeric_limits<double>::infinity(),
            -std::numeric_limits<double>::infinity() };
        for ( const Vertex& vertex : sourceSector.vertices )
        {
            minimum[0] = std::min(minimum[0], vertex.x);
            minimum[1] = std::min(minimum[1], vertex.y);
            minimum[2] = std::min(minimum[2], vertex.z);
            maximum[0] = std::max(maximum[0], vertex.x);
            maximum[1] = std::max(maximum[1], vertex.y);
            maximum[2] = std::max(maximum[2], vertex.z);
        }
        const GlView::RecoveryState state = view.recoveryState();
        for ( const auto& line : state.cleaveLines )
        {
            for ( const TVector* endpoint : { &line.first, &line.second } )
            {
                const QVector3D point = toVector(*endpoint);
                if ( point.x() < minimum[0] - 1.0e-5
                    || point.x() > maximum[0] + 1.0e-5
                    || point.y() < minimum[1] - 1.0e-5
                    || point.y() > maximum[1] + 1.0e-5
                    || point.z() < minimum[2] - 1.0e-5
                    || point.z() > maximum[2] + 1.0e-5 )
                {
                    return false;
                }
                const auto screen = project(view, point);
                if ( !screen || !view.rect().contains(screen->toPoint()) )
                {
                    return false;
                }
            }
        }
        return true;
    }

    bool dragWorld(
        const QVector3D& first,
        const QVector3D& second,
        const Qt::KeyboardModifiers modifiers)
    {
        const auto start = project(view, first);
        const auto end = project(view, second);
        if ( !start || !end )
        {
            return false;
        }
        drag(view, *start, *end, modifiers);
        return true;
    }
};

bool historyMatches(
    const std::optional<GlView::RecoveryState::CleaveHistory>& left,
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
    if ( left->cursor != right->cursor
        || left->snapshots.size() != right->snapshots.size() )
    {
        return false;
    }
    for ( std::size_t index = 0; index < left->snapshots.size(); ++index )
    {
        const auto& a = left->snapshots[index];
        const auto& b = right->snapshots[index];
        if ( !sameLines(a.lines, b.lines)
            || a.context.valid != b.context.valid
            || a.context.surfaceMode != b.context.surfaceMode
            || a.context.projection != b.context.projection
            || a.context.view != b.context.view
            || a.context.viewportWidth != b.context.viewportWidth
            || a.context.viewportHeight != b.context.viewportHeight
            || a.context.sourceSurface != b.context.sourceSurface
            || a.context.targetSectors != b.context.targetSectors
            || a.context.targetSurfaces != b.context.targetSurfaces
            || a.chainOrigin.has_value() != b.chainOrigin.has_value()
            || (a.chainOrigin
                && (a.chainOrigin->lineIndex != b.chainOrigin->lineIndex
                    || a.chainOrigin->endpoint != b.chainOrigin->endpoint))
            || a.applyOrder != b.applyOrder
            || a.selectedCleaveLines != b.selectedCleaveLines
            || a.selectedCleaveLine != b.selectedCleaveLine )
        {
            return false;
        }
    }
    return true;
}

bool sameCleaveContext(
    const GlView::RecoveryState::CleaveContext& left,
    const GlView::RecoveryState::CleaveContext& right)
{
    return left.valid == right.valid
        && left.surfaceMode == right.surfaceMode
        && left.projection == right.projection
        && left.view == right.view
        && left.viewportWidth == right.viewportWidth
        && left.viewportHeight == right.viewportHeight
        && left.sourceSurface == right.sourceSurface
        && left.targetSectors == right.targetSectors
        && left.targetSurfaces == right.targetSurfaces;
}

bool sameCleaveDraftState(
    const GlView::RecoveryState& left,
    const GlView::RecoveryState& right)
{
    const bool sameOrigin = left.cleaveChainOrigin.has_value()
        == right.cleaveChainOrigin.has_value()
        && (!left.cleaveChainOrigin
            || (left.cleaveChainOrigin->lineIndex == right.cleaveChainOrigin->lineIndex
                && left.cleaveChainOrigin->endpoint
                    == right.cleaveChainOrigin->endpoint));
    return sameLines(left.cleaveLines, right.cleaveLines)
        && sameCleaveContext(left.cleaveContext, right.cleaveContext)
        && sameOrigin
        && left.applyOrder == right.applyOrder
        && left.selectedCleaveLines == right.selectedCleaveLines
        && left.selectedCleaveLine == right.selectedCleaveLine
        && historyMatches(left.cleaveHistory, right.cleaveHistory);
}

bool seedSettledCleaveHistoryBaseline(GlView& view)
{
    GlView::RecoveryState::CleaveHistory history{};
    history.snapshots.push_back(view.captureStagedCleaveDraftState());
    history.cursor = 0;
    view.restoreStagedCleaveDraftState(history.snapshots.front(), history);
    const std::optional<GlView::RecoveryState::CleaveHistory> restored =
        view.captureStagedCleaveHistory();
    return restored
        && restored->cursor == 0
        && restored->snapshots.size() == 1
        && view.stagedCleaveHistoryOwnsUndo()
        && !view.canUndoStagedCleaveHistory()
        && !view.canRedoStagedCleaveHistory();
}

void printCleaveFailureState(
    const char* const label,
    const GlView::RecoveryState& before,
    const GlView::RecoveryState& after,
    const GlView& view,
    const std::uint64_t revisionBefore)
{
    const auto historyCursor = [](const GlView::RecoveryState& state)
    {
        return state.cleaveHistory
            ? static_cast<long long>(state.cleaveHistory->cursor)
            : -1LL;
    };
    const auto historyCount = [](const GlView::RecoveryState& state)
    {
        return state.cleaveHistory
            ? state.cleaveHistory->snapshots.size()
            : std::size_t{};
    };
    const QString hud = view.stagedCleaveTransformHudTextForTest();
    const QByteArray hudBytes = hud.toUtf8();
    std::fprintf(
        stderr,
        "Cleave advanced failure: %s lines=%zu/%zu order=%zu/%zu "
        "context=(%d,%d,%zu,%zu)/(%d,%d,%zu,%zu) "
        "history=(%d,%lld,%zu)/(%d,%lld,%zu) "
        "selection=(%d,%zu)/(%d,%zu) origin=(%d,%d,%d)/(%d,%d,%d) "
        "revision=%llu/%llu camera=(%d,%.3f,%.3f) hud=\"%s\"\n",
        label,
        before.cleaveLines.size(),
        after.cleaveLines.size(),
        before.applyOrder.size(),
        after.applyOrder.size(),
        before.cleaveContext.valid ? 1 : 0,
        before.cleaveContext.surfaceMode ? 1 : 0,
        before.cleaveContext.targetSectors.size(),
        before.cleaveContext.targetSurfaces.size(),
        after.cleaveContext.valid ? 1 : 0,
        after.cleaveContext.surfaceMode ? 1 : 0,
        after.cleaveContext.targetSectors.size(),
        after.cleaveContext.targetSurfaces.size(),
        before.cleaveHistory.has_value() ? 1 : 0,
        historyCursor(before),
        historyCount(before),
        after.cleaveHistory.has_value() ? 1 : 0,
        historyCursor(after),
        historyCount(after),
        before.selectedCleaveLine,
        before.selectedCleaveLines.size(),
        after.selectedCleaveLine,
        after.selectedCleaveLines.size(),
        before.cleaveChainOrigin.has_value() ? 1 : 0,
        before.cleaveChainOrigin ? before.cleaveChainOrigin->lineIndex : -1,
        before.cleaveChainOrigin ? before.cleaveChainOrigin->endpoint : -1,
        after.cleaveChainOrigin.has_value() ? 1 : 0,
        after.cleaveChainOrigin ? after.cleaveChainOrigin->lineIndex : -1,
        after.cleaveChainOrigin ? after.cleaveChainOrigin->endpoint : -1,
        static_cast<unsigned long long>(revisionBefore),
        static_cast<unsigned long long>(view.stagedCleaveAuthoredRevision()),
        static_cast<int>(view.cameraProjectionType()),
        static_cast<double>(view.cameraOrthoScale()),
        static_cast<double>(view.cameraOrbitDistance()),
        hudBytes.constData());

    std::fprintf(stderr, "  selectedBefore=[");
    for (std::size_t index = 0; index < before.selectedCleaveLines.size(); ++index)
    {
        std::fprintf(stderr, "%s%d", index == 0 ? "" : ",", before.selectedCleaveLines[index]);
    }
    std::fprintf(stderr, "] selectedAfter=[");
    for (std::size_t index = 0; index < after.selectedCleaveLines.size(); ++index)
    {
        std::fprintf(stderr, "%s%d", index == 0 ? "" : ",", after.selectedCleaveLines[index]);
    }
    std::fprintf(stderr, "] orderBefore=[");
    for (std::size_t index = 0; index < before.applyOrder.size(); ++index)
    {
        std::fprintf(stderr, "%s%d", index == 0 ? "" : ",", before.applyOrder[index]);
    }
    std::fprintf(stderr, "] orderAfter=[");
    for (std::size_t index = 0; index < after.applyOrder.size(); ++index)
    {
        std::fprintf(stderr, "%s%d", index == 0 ? "" : ",", after.applyOrder[index]);
    }
    std::fprintf(stderr, "]\n");

    for (std::size_t index = 0;
         index < std::max(before.cleaveLines.size(), after.cleaveLines.size());
         ++index)
    {
        const QVector3D firstBefore = lineFirst(before, index);
        const QVector3D secondBefore = lineSecond(before, index);
        const QVector3D firstAfter = lineFirst(after, index);
        const QVector3D secondAfter = lineSecond(after, index);
        std::fprintf(
            stderr,
            "  line[%zu] first=(%.6f,%.6f,%.6f)->(%.6f,%.6f,%.6f) "
            "second=(%.6f,%.6f,%.6f)->(%.6f,%.6f,%.6f)\n",
            index,
            firstBefore.x(), firstBefore.y(), firstBefore.z(),
            firstAfter.x(), firstAfter.y(), firstAfter.z(),
            secondBefore.x(), secondBefore.y(), secondBefore.z(),
            secondAfter.x(), secondAfter.y(), secondAfter.z());
    }
}

std::size_t cleaveDraftOwnedBytesForTest(
    const GlView::RecoveryState::CleaveDraftSnapshot& snapshot)
{
    std::size_t bytes = sizeof(snapshot);
    bytes += snapshot.lines.capacity()
        * sizeof(GlView::RecoveryState::CleaveLine);
    bytes += snapshot.context.targetSectors.capacity() * sizeof(std::int32_t);
    bytes += snapshot.context.targetSurfaces.capacity() * sizeof(SurfaceRef);
    bytes += snapshot.applyOrder.capacity() * sizeof(std::int32_t);
    bytes += snapshot.selectedCleaveLines.capacity() * sizeof(std::int32_t);
    return bytes;
}

QImage captureViewportFramebuffer(GlView& view)
{
    view.update();
    QApplication::processEvents(QEventLoop::AllEvents);
    return view.grabFramebuffer();
}

bool framebufferRegionChanged(
    const QImage& before,
    const QImage& after,
    const QRect& logicalRegion)
{
    if ( before.isNull() || after.isNull() || before.size() != after.size() )
    {
        return false;
    }

    const qreal scale = std::max<qreal>(1.0, before.devicePixelRatio());
    const QRect pixelRegion(
        qRound(logicalRegion.x() * scale),
        qRound(logicalRegion.y() * scale),
        qRound(logicalRegion.width() * scale),
        qRound(logicalRegion.height() * scale));
    const QRect clipped = pixelRegion.intersected(before.rect());
    if ( clipped.isEmpty() )
    {
        return false;
    }

    int changedPixels = 0;
    for ( int y = clipped.top(); y <= clipped.bottom(); ++y )
    {
        for ( int x = clipped.left(); x <= clipped.right(); ++x )
        {
            const QColor first = before.pixelColor(x, y);
            const QColor second = after.pixelColor(x, y);
            const int delta = std::abs(first.red() - second.red())
                + std::abs(first.green() - second.green())
                + std::abs(first.blue() - second.blue())
                + std::abs(first.alpha() - second.alpha());
            if ( delta > 24 )
            {
                ++changedPixels;
            }
        }
    }
    return changedPixels >= 3;
}

int framebufferBrightNeutralPixelCount(
    const QImage& image,
    const QRect& logicalRegion)
{
    if ( image.isNull() )
    {
        return 0;
    }

    const qreal scale = std::max<qreal>(1.0, image.devicePixelRatio());
    const QRect pixelRegion(
        qRound(logicalRegion.x() * scale),
        qRound(logicalRegion.y() * scale),
        qRound(logicalRegion.width() * scale),
        qRound(logicalRegion.height() * scale));
    const QRect clipped = pixelRegion.intersected(image.rect());
    int count = 0;
    for ( int y = clipped.top(); y <= clipped.bottom(); ++y )
    {
        for ( int x = clipped.left(); x <= clipped.right(); ++x )
        {
            const QColor pixel = image.pixelColor(x, y);
            const int minimum = std::min({pixel.red(), pixel.green(), pixel.blue()});
            const int maximum = std::max({pixel.red(), pixel.green(), pixel.blue()});
            if ( pixel.alpha() > 80
                && minimum >= 150
                && maximum - minimum <= 70 )
            {
                ++count;
            }
        }
    }
    return count;
}

bool framebufferHudTextAppeared(
    const QImage& before,
    const QImage& during,
    const QRect& logicalRegion)
{
    const int beforePixels = framebufferBrightNeutralPixelCount(before, logicalRegion);
    const int duringPixels = framebufferBrightNeutralPixelCount(during, logicalRegion);
    return duringPixels >= 8 && duringPixels >= beforePixels + 5;
}

QImage captureViewportFramebufferWithHudText(
    GlView& view,
    const QImage& before,
    const QRect& logicalRegion)
{
    QTimer* const hideTimer = view.findChild<QTimer*>(
        "cleave_transform_hud_hide_timer");
    const int originalRemainingMs = hideTimer && hideTimer->isActive()
        ? hideTimer->remainingTime() : 0;
    QElapsedTimer elapsed{};
    elapsed.start();

    QImage current = captureViewportFramebuffer(view);
    while ( !framebufferHudTextAppeared(before, current, logicalRegion)
        && originalRemainingMs > elapsed.elapsed()
        && hideTimer && hideTimer->isActive()
        && !view.stagedCleaveTransformHudTextForTest().isEmpty() )
    {
        const int watchdogMs = originalRemainingMs
            - static_cast<int>(elapsed.elapsed());
        if ( watchdogMs <= 0 )
        {
            break;
        }
        QTimer* const presentationTimer = view.findChild<QTimer*>(
            "cleave_transform_hud_presentation_timer");
        if ( !presentationTimer || !presentationTimer->isActive() )
        {
            break;
        }

        // Wait for the renderer's existing presentation wake or the original
        // hide deadline, then inspect the framebuffer again. This keeps the
        // test event-driven and bounded by the banner's current lifetime.
        QEventLoop wakeLoop{};
        const QMetaObject::Connection presentationWake = QObject::connect(
            presentationTimer,
            &QTimer::timeout,
            &wakeLoop,
            &QEventLoop::quit);
        const QMetaObject::Connection hideDeadline = QObject::connect(
            hideTimer,
            &QTimer::timeout,
            &wakeLoop,
            &QEventLoop::quit);
        QTimer watchdog{};
        watchdog.setSingleShot(true);
        watchdog.setTimerType(Qt::PreciseTimer);
        const QMetaObject::Connection watchdogDeadline = QObject::connect(
            &watchdog,
            &QTimer::timeout,
            &wakeLoop,
            &QEventLoop::quit);
        watchdog.start(watchdogMs);
        wakeLoop.exec(QEventLoop::AllEvents);
        watchdog.stop();
        QObject::disconnect(presentationWake);
        QObject::disconnect(hideDeadline);
        QObject::disconnect(watchdogDeadline);
        current = captureViewportFramebuffer(view);
    }
    return current;
}

bool framebufferPatchChanged(
    const QImage& before,
    const QImage& after,
    const QPointF& logicalCenter,
    const int logicalRadius = 4)
{
    return framebufferRegionChanged(
        before,
        after,
        QRect(
            qRound(logicalCenter.x()) - logicalRadius,
            qRound(logicalCenter.y()) - logicalRadius,
            logicalRadius * 2 + 1,
            logicalRadius * 2 + 1));
}



void runInitialAnchorGridSnapCases()
{
    for ( const bool sectorMode : { false, true } )
    {
        for ( const bool gridEnabled : { false, true } )
        {
            for ( const bool controlHeld : { false, true } )
            {
                CleaveViewportFixture fixture(sectorMode);
                fixture.view.setTransformGridSnapEnabled(gridEnabled);
                const QVector3D start(0.22f, 0.0f, 0.18f);
                const QVector3D end(1.28f, 0.0f, 0.92f);
                const Qt::KeyboardModifiers modifiers = controlHeld
                    ? Qt::ControlModifier
                    : Qt::NoModifier;
                const auto startScreen = project(fixture.view, start);
                const auto endScreen = project(fixture.view, end);
                std::optional<TVector> pressAnchor{};
                bool pressToolEnabled = false;
                bool firstAnchorOnGrid = false;
                bool firstAnchorOffGrid = false;
                const bool expectedGridSnap = gridEnabled != controlHeld;
                const auto onGrid = [](const double value)
                {
                    return close(value / 0.5, std::round(value / 0.5), 0.06);
                };
                const std::string label = std::string(
                    sectorMode ? "Sector" : "Surface")
                    + (gridEnabled ? " enabled" : " disabled")
                    + (controlHeld ? " Ctrl" : " plain")
                    + " first-point acquisition follows shared Grid Snap and Ctrl inversion";
                if ( startScreen && endScreen )
                {
                    sendMouse(
                        fixture.view,
                        QEvent::MouseButtonPress,
                        *startScreen,
                        Qt::LeftButton,
                        Qt::LeftButton,
                        modifiers);
                    pressAnchor = fixture.view.stagedCleaveFirstPointForTest();
                    pressToolEnabled = sectorMode
                        ? fixture.view.sectorCleaveToolEnabled()
                        : fixture.view.surfaceCleaveToolEnabled();
                    if ( pressAnchor )
                    {
                        firstAnchorOnGrid = onGrid(pressAnchor->x)
                            && onGrid(pressAnchor->z);
                        firstAnchorOffGrid = std::abs(
                                pressAnchor->x / 0.5
                                    - std::round(pressAnchor->x / 0.5)) > 0.12
                            || std::abs(
                                pressAnchor->z / 0.5
                                    - std::round(pressAnchor->z / 0.5)) > 0.12;
                    }
                    check(
                        pressAnchor.has_value()
                            && (expectedGridSnap
                                ? firstAnchorOnGrid
                                : firstAnchorOffGrid),
                        (label + " is verified at the initial press").c_str());
                    sendMouse(
                        fixture.view,
                        QEvent::MouseMove,
                        *endScreen,
                        Qt::NoButton,
                        Qt::LeftButton,
                        modifiers);
                    sendMouse(
                        fixture.view,
                        QEvent::MouseButtonRelease,
                        *endScreen,
                        Qt::LeftButton,
                        Qt::NoButton,
                        modifiers);
                    QApplication::processEvents(QEventLoop::AllEvents);
                }
                const bool projected = startScreen.has_value()
                    && endScreen.has_value();
                const GlView::RecoveryState draft = fixture.view.recoveryState();
                const bool authoredLine = draft.cleaveLines.size() == 1
                    && draft.cleaveContext.valid
                    && draft.cleaveContext.surfaceMode == !sectorMode;
                const bool sourceSurfaceGeometryInside =
                    fixture.sourceGeometryProjectsInside();
                const bool sourceSectorGeometryInside =
                    fixture.sectorGeometryProjectsInside();
                const bool targetGeometryInside = sectorMode
                    ? sourceSectorGeometryInside
                    : sourceSurfaceGeometryInside;
                const bool targetContextMatches =
                    fixture.view.stagedCleaveTargetsMatchCurrentForTest(!sectorMode);
                if ( !targetGeometryInside || !targetContextMatches )
                {
                    const QVector3D actualFirst = authoredLine
                        ? toVector(draft.cleaveLines.front().first)
                        : QVector3D{};
                    const QVector3D actualSecond = authoredLine
                        ? toVector(draft.cleaveLines.front().second)
                        : QVector3D{};
                    std::fprintf(
                        stderr,
                        "Initial-anchor target fixture mismatch: mode=%s sourceSurfaceGeometry=%d sourceSectorGeometry=%d selectedGeometryInside=%d targetsMatch=%d count=%zu contextValid=%d contextSurface=%d source=(%d,%d) targets=(%zu,%zu) line=(%.12g,%.12g,%.12g)->(%.12g,%.12g,%.12g)\n",
                        sectorMode ? "Sector" : "Surface",
                        sourceSurfaceGeometryInside ? 1 : 0,
                        sourceSectorGeometryInside ? 1 : 0,
                        targetGeometryInside ? 1 : 0,
                        targetContextMatches ? 1 : 0,
                        draft.cleaveLines.size(),
                        draft.cleaveContext.valid ? 1 : 0,
                        draft.cleaveContext.surfaceMode ? 1 : 0,
                        draft.cleaveContext.sourceSurface.sector,
                        draft.cleaveContext.sourceSurface.surface,
                        draft.cleaveContext.targetSectors.size(),
                        draft.cleaveContext.targetSurfaces.size(),
                        actualFirst.x(),
                        actualFirst.y(),
                        actualFirst.z(),
                        actualSecond.x(),
                        actualSecond.y(),
                        actualSecond.z());
                }
                if ( !authoredLine )
                {
                    const bool releaseToolEnabled = sectorMode
                        ? fixture.view.sectorCleaveToolEnabled()
                        : fixture.view.surfaceCleaveToolEnabled();
                    const QVector3D actualPressAnchor = pressAnchor
                        ? toVector(*pressAnchor)
                        : QVector3D{};
                    std::fprintf(
                        stderr,
                        "Initial-anchor authored-line failure: mode=%s grid=%d ctrl=%d projected=%d pressAnchor=%d(%.12g,%.12g,%.12g) count=%zu contextValid=%d contextSurface=%d source=(%d,%d) targets=(%zu,%zu) pressTool=%d releaseTool=%d finalTool=(sector%d,surface%d) activeSector=%d activeSurface=(%d,%d)\n",
                        sectorMode ? "Sector" : "Surface",
                        gridEnabled ? 1 : 0,
                        controlHeld ? 1 : 0,
                        projected ? 1 : 0,
                        pressAnchor.has_value() ? 1 : 0,
                        actualPressAnchor.x(),
                        actualPressAnchor.y(),
                        actualPressAnchor.z(),
                        draft.cleaveLines.size(),
                        draft.cleaveContext.valid ? 1 : 0,
                        draft.cleaveContext.surfaceMode ? 1 : 0,
                        draft.cleaveContext.sourceSurface.sector,
                        draft.cleaveContext.sourceSurface.surface,
                        draft.cleaveContext.targetSectors.size(),
                        draft.cleaveContext.targetSurfaces.size(),
                        pressToolEnabled ? 1 : 0,
                        releaseToolEnabled ? 1 : 0,
                        fixture.view.sectorCleaveToolEnabled() ? 1 : 0,
                        fixture.view.surfaceCleaveToolEnabled() ? 1 : 0,
                        fixture.view.activeSectorIndex(),
                        fixture.view.activeSurfaceSectorIndex(),
                        fixture.view.activeSurfaceIndex());
                }
                const bool anchorMatchesPolicy = expectedGridSnap
                    ? firstAnchorOnGrid
                    : firstAnchorOffGrid;
                if ( !anchorMatchesPolicy )
                {
                    const QVector3D actualPressAnchor = pressAnchor
                        ? toVector(*pressAnchor)
                        : QVector3D{};
                    const QVector3D actualFirst = authoredLine
                        ? toVector(draft.cleaveLines.front().first)
                        : QVector3D{};
                    const QVector3D actualSecond = authoredLine
                        ? toVector(draft.cleaveLines.front().second)
                        : QVector3D{};
                    std::fprintf(
                        stderr,
                        "Initial anchor mismatch: mode=%s grid=%d ctrl=%d expectedGrid=%d projected=%d startPx=(%.4f,%.4f) endPx=(%.4f,%.4f) pressAnchor=%d(%.12g,%.12g,%.12g) authored=%d count=%zu finalFirst=(%.12g,%.12g,%.12g) finalSecond=(%.12g,%.12g,%.12g) onGrid=%d offGrid=%d activeSector=%d activeSurface=(%d,%d) context=(%d,%d) targets=(%zu,%zu)\n",
                        sectorMode ? "Sector" : "Surface",
                        gridEnabled ? 1 : 0,
                        controlHeld ? 1 : 0,
                        expectedGridSnap ? 1 : 0,
                        projected ? 1 : 0,
                        startScreen ? startScreen->x() : -1.0,
                        startScreen ? startScreen->y() : -1.0,
                        endScreen ? endScreen->x() : -1.0,
                        endScreen ? endScreen->y() : -1.0,
                        pressAnchor.has_value() ? 1 : 0,
                        actualPressAnchor.x(),
                        actualPressAnchor.y(),
                        actualPressAnchor.z(),
                        authoredLine ? 1 : 0,
                        draft.cleaveLines.size(),
                        actualFirst.x(), actualFirst.y(), actualFirst.z(),
                        actualSecond.x(), actualSecond.y(), actualSecond.z(),
                        firstAnchorOnGrid ? 1 : 0,
                        firstAnchorOffGrid ? 1 : 0,
                        fixture.view.activeSectorIndex(),
                        fixture.view.activeSurfaceSectorIndex(),
                        fixture.view.activeSurfaceIndex(),
                        draft.cleaveContext.valid ? 1 : 0,
                        draft.cleaveContext.surfaceMode ? 1 : 0,
                        draft.cleaveContext.targetSectors.size(),
                        draft.cleaveContext.targetSurfaces.size());
                }
                check(projected && authoredLine,
                    (label + " publishes a valid authored line").c_str());
                check(targetGeometryInside,
                    (label + " remains within the actual source geometry").c_str());
                check(targetContextMatches,
                    (label + " retains the original source and target context").c_str());
            }
        }
    }
}

void runSectorBoundedFiniteStrokeCase()
{
    CleaveViewportFixture fixture(true);
    fixture.view.setTransformGridSnapEnabled(false);
    fixture.view.setCleaveBounded(true);
    const bool firstStrokeDrawn = fixture.dragWorld(
        QVector3D(0.0f, 0.0f, -1.8f),
        QVector3D(0.0f, 0.0f, 1.8f),
        Qt::NoModifier);
    const bool secondStrokeDrawn = fixture.dragWorld(
        QVector3D(0.25f, 0.0f, 0.5f),
        QVector3D(1.6f, 0.0f, 0.5f),
        Qt::NoModifier);
    const GlView::RecoveryState beforeBoundedToggle = fixture.view.recoveryState();
    fixture.view.setCleaveBounded(false);
    const GlView::RecoveryState afterBoundedOff = fixture.view.recoveryState();
    fixture.view.setCleaveBounded(true);
    const GlView::RecoveryState authored = fixture.view.recoveryState();
    const bool boundedTogglePreservedGeometry =
        sameLines(beforeBoundedToggle.cleaveLines, afterBoundedOff.cleaveLines)
        && sameLines(beforeBoundedToggle.cleaveLines, authored.cleaveLines);
    const double secondStrokeLength = authored.cleaveLines.size() == 2
        ? std::hypot(
            authored.cleaveLines[1].second.x - authored.cleaveLines[1].first.x,
            authored.cleaveLines[1].second.y - authored.cleaveLines[1].first.y,
            authored.cleaveLines[1].second.z - authored.cleaveLines[1].first.z)
        : 0.0;
    if ( authored.cleaveLines.size() == 2
        && !(secondStrokeLength >= 1.0 && secondStrokeLength < 2.0) )
    {
        const auto& second = authored.cleaveLines[1];
        std::fprintf(
            stderr,
            "Sector bounded stroke length mismatch: length=%.12g endpoints=(%.12g,%.12g,%.12g)->(%.12g,%.12g,%.12g) bounded=%d context=%d mode=%d\n",
            secondStrokeLength,
            second.first.x,
            second.first.y,
            second.first.z,
            second.second.x,
            second.second.y,
            second.second.z,
            fixture.view.cleaveBounded() ? 1 : 0,
            authored.cleaveContext.valid ? 1 : 0,
            authored.cleaveContext.surfaceMode ? 1 : 0);
    }
    check(firstStrokeDrawn && secondStrokeDrawn
            && authored.cleaveLines.size() == 2
            && authored.cleaveContext.valid
            && !authored.cleaveContext.surfaceMode
            && fixture.view.cleaveBounded(),
        "Sector bounded-stroke fixture authors two real viewport lines under one Sector target");
    check(secondStrokeLength >= 1.0 && secondStrokeLength < 2.0,
        "Sector release preserves the short second stroke instead of stretching it across the Sector");
    check(boundedTogglePreservedGeometry,
        "toggling Bounded after authoring leaves the original finite Sector strokes unchanged");

    std::vector<sed::level::SectorCleavePlane> planes{};
    std::vector<sed::level::SectorCleaveStroke> strokes{};
    bool capturedBatch = false;
    bool capturedBounded = false;
    bool capturedClosed = true;
    sed::level::SectorCleaveBatchResult result{};
    QObject::connect(
        &fixture.view,
        &GlView::sectorsCleaveBatchRequested,
        &fixture.view,
        [&](const QList<TVector>& planeNormals,
            const QList<TVector>& points,
            const bool closedShape,
            const TVector& interiorPoint,
            const QList<TVector>& strokeSecondPoints,
            const bool bounded)
        {
            capturedBatch = true;
            capturedBounded = bounded;
            capturedClosed = closedShape;
            if ( planeNormals.size() != points.size()
                || points.size() != strokeSecondPoints.size() )
            {
                return;
            }
            planes.reserve(static_cast<std::size_t>(planeNormals.size()));
            strokes.reserve(static_cast<std::size_t>(points.size()));
            for ( int index = 0; index < planeNormals.size(); ++index )
            {
                planes.push_back(sed::level::SectorCleavePlane{
                    planeNormals[index], points[index] });
                strokes.push_back(sed::level::SectorCleaveStroke{
                    points[index], strokeSecondPoints[index] });
            }
            const sed::level::SectorCleaveBatchOptions options{
                closedShape,
                interiorPoint,
                strokes,
                bounded,
            };
            result = sed::level::levelCleaveSectorByPlanes(
                fixture.level,
                0,
                planes,
                options);
        });
    fixture.view.requestApplyStagedCleave();
    check(capturedBatch && capturedBounded && !capturedClosed
            && planes.size() == 2 && strokes.size() == 2,
        "real Sector Multi-cut Apply publishes both finite strokes with Bounded enabled");
    if ( !result.succeeded() || result.outputSectors.size() != 3
        || fixture.level.sectors.size() != 3 )
    {
        std::fprintf(
            stderr,
            "Sector bounded apply mismatch: captured=%d bounded=%d closed=%d planeCount=%zu strokeCount=%zu resultError=%d resultSectors=%zu levelSectors=%zu\n",
            capturedBatch ? 1 : 0,
            capturedBounded ? 1 : 0,
            capturedClosed ? 1 : 0,
            planes.size(),
            strokes.size(),
            static_cast<int>(result.error),
            result.outputSectors.size(),
            fixture.level.sectors.size());
    }
    check(result.succeeded() && result.outputSectors.size() == 3
            && fixture.level.sectors.size() == 3,
        "bounded Sector Apply leaves the short second stroke confined to the descendant it reaches");
}

void runMovePressHistoryBudgetCase()
{
    CleaveViewportFixture fixture{};
    fixture.view.setCameraOrthoScale(32.0f);
    fixture.view.captureStagedCleaveContextForTest(true);
    fixture.addLine(QVector3D(-0.8f, 0.0f, 0.0f), QVector3D(0.8f, 0.0f, 0.0f));
    const bool smallRouteTargetsMatch =
        fixture.view.stagedCleaveTargetsMatchCurrentForTest(true);

    const GlView::RecoveryState smallRouteState = fixture.view.recoveryState();
    const auto smallRouteBody = project(
        fixture.view,
        (lineFirst(smallRouteState, 0u) + lineSecond(smallRouteState, 0u)) * 0.5f);
    double smallRouteEndpointClearance = std::numeric_limits<double>::infinity();
    if ( smallRouteBody )
    {
        for ( const TVector* endpoint : {
                 &smallRouteState.cleaveLines.front().first,
                 &smallRouteState.cleaveLines.front().second } )
        {
            const auto endpointPosition = project(fixture.view, toVector(*endpoint));
            if ( endpointPosition )
            {
                smallRouteEndpointClearance = std::min(
                    smallRouteEndpointClearance,
                    std::hypot(smallRouteBody->x() - endpointPosition->x(),
                               smallRouteBody->y() - endpointPosition->y()));
            }
        }
    }
    bool smallRouteEnteredMove = false;
    if ( smallRouteTargetsMatch && smallRouteBody
        && smallRouteEndpointClearance > 10.0 )
    {
        sendMouse(
            fixture.view,
            QEvent::MouseButtonPress,
            *smallRouteBody,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        smallRouteEnteredMove = fixture.view.stagedCleavePlaneDraggingForTest();
        sendMouse(
            fixture.view,
            QEvent::MouseButtonRelease,
            *smallRouteBody,
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::processEvents(QEventLoop::AllEvents);
    }

    constexpr std::size_t kHistoryBudgetBytes = 16u * 1024u * 1024u;
    constexpr std::size_t kHeadroomTargetBytes = 4096u;
    constexpr std::size_t kHistorySnapshotLimit = 64u;
    using DraftSnapshot = GlView::RecoveryState::CleaveDraftSnapshot;
    using CleaveLine = GlView::RecoveryState::CleaveLine;
    const DraftSnapshot smallDraft = fixture.view.captureStagedCleaveDraftState();
    if ( smallDraft.lines.empty() )
    {
        check(false, "Move-press budget fixture starts with one valid authored line");
        return;
    }

    const std::size_t bytesPerPhysicalLine = sizeof(CleaveLine)
        + sizeof(std::int32_t);
    const std::size_t bytesPerLineAcrossCurrentBaseline = 3u * bytesPerPhysicalLine;
    const std::size_t retainedStateShellBytes =
        kHistorySnapshotLimit * sizeof(DraftSnapshot);
    const std::size_t lineCount = (kHistoryBudgetBytes
        - kHeadroomTargetBytes
        - retainedStateShellBytes)
        / bytesPerLineAcrossCurrentBaseline;
    DraftSnapshot largeDraft = smallDraft;
    largeDraft.lines.clear();
    largeDraft.lines.reserve(lineCount);
    largeDraft.applyOrder.clear();
    largeDraft.applyOrder.reserve(lineCount);
    largeDraft.selectedCleaveLines = {0};
    largeDraft.selectedCleaveLine = 0;
    largeDraft.chainOrigin.reset();
    for ( std::size_t index = 0; index < lineCount; ++index )
    {
        CleaveLine line = smallDraft.lines.front();
        if ( index != 0u )
        {
            const double offset = 10.0 + static_cast<double>(index) * 3.0;
            line.first.x += offset;
            line.second.x += offset;
        }
        largeDraft.lines.push_back(std::move(line));
        largeDraft.applyOrder.push_back(static_cast<std::int32_t>(index));
    }

    const std::size_t baselineCopyBytes = cleaveDraftOwnedBytesForTest(largeDraft);
    GlView::RecoveryState::CleaveHistory history{};
    history.snapshots.reserve(kHistorySnapshotLimit);
    history.snapshots.push_back(largeDraft);
    history.cursor = 0u;
    fixture.view.restoreStagedCleaveDraftState(
        std::move(largeDraft),
        std::move(history));

    const std::size_t storageBefore =
        fixture.view.stagedCleaveHistoryStorageBytesEstimate();
    const std::size_t affectedMaskLowerBound =
        lineCount * 2u * sizeof(std::int32_t);
    const std::size_t headroomAfterGestureBaseline =
        storageBefore <= kHistoryBudgetBytes
        && baselineCopyBytes <= kHistoryBudgetBytes - storageBefore
        ? kHistoryBudgetBytes - storageBefore - baselineCopyBytes
        : 0u;
    const bool budgetFixtureFits = storageBefore < kHistoryBudgetBytes
        && baselineCopyBytes <= kHistoryBudgetBytes - storageBefore
        && headroomAfterGestureBaseline < affectedMaskLowerBound;

    const GlView::RecoveryState before = fixture.view.recoveryState();
    const std::uint64_t revisionBefore =
        fixture.view.stagedCleaveAuthoredRevision();
    const bool canUndoBefore = fixture.view.canUndoStagedCleaveHistory();
    const bool canRedoBefore = fixture.view.canRedoStagedCleaveHistory();
    const auto body = project(
        fixture.view,
        (lineFirst(before, 0u) + lineSecond(before, 0u)) * 0.5f);
    double nearestEndpointPixels = std::numeric_limits<double>::infinity();
    if ( body )
    {
        for ( const auto& line : before.cleaveLines )
        {
            for ( const TVector* endpoint : {&line.first, &line.second} )
            {
                const auto endpointPosition = project(fixture.view, toVector(*endpoint));
                if ( endpointPosition )
                {
                    nearestEndpointPixels = std::min(
                        nearestEndpointPixels,
                        std::hypot(body->x() - endpointPosition->x(),
                                   body->y() - endpointPosition->y()));
                }
            }
        }
    }
    const bool bodyPickClear = nearestEndpointPixels > 10.0;
    bool dragStartedAtPress = false;
    std::size_t storageAtPress = storageBefore;
    if ( budgetFixtureFits && body )
    {
        sendMouse(
            fixture.view,
            QEvent::MouseButtonPress,
            *body,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        dragStartedAtPress = fixture.view.stagedCleavePlaneDraggingForTest();
        storageAtPress = fixture.view.stagedCleaveHistoryStorageBytesEstimate();
        sendMouse(
            fixture.view,
            QEvent::MouseButtonRelease,
            *body,
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::processEvents(QEventLoop::AllEvents);
    }

    const GlView::RecoveryState after = fixture.view.recoveryState();
    const bool unchangedDraft = sameLines(before.cleaveLines, after.cleaveLines)
        && before.applyOrder == after.applyOrder
        && before.selectedCleaveLines == after.selectedCleaveLines
        && before.selectedCleaveLine == after.selectedCleaveLine
        && before.cleaveContext.valid == after.cleaveContext.valid
        && before.cleaveContext.surfaceMode == after.cleaveContext.surfaceMode
        && before.cleaveContext.sourceSurface == after.cleaveContext.sourceSurface
        && before.cleaveContext.targetSectors == after.cleaveContext.targetSectors
        && before.cleaveContext.targetSurfaces == after.cleaveContext.targetSurfaces
        && before.cleaveContext.projection == after.cleaveContext.projection
        && before.cleaveContext.view == after.cleaveContext.view
        && historyMatches(before.cleaveHistory, after.cleaveHistory);
    const bool unchangedOwnership = fixture.view.stagedCleaveAuthoredRevision()
            == revisionBefore
        && fixture.view.canUndoStagedCleaveHistory() == canUndoBefore
        && fixture.view.canRedoStagedCleaveHistory() == canRedoBefore;
    const bool atomicRefusal = body.has_value()
        && bodyPickClear
        && !dragStartedAtPress
        && storageAtPress == storageBefore
        && fixture.view.stagedCleaveHistoryStorageBytesEstimate() == storageBefore
        && !fixture.view.stagedCleavePlaneDraggingForTest()
        && unchangedDraft
        && unchangedOwnership;

    if ( !budgetFixtureFits || !body || !atomicRefusal )
    {
        std::fprintf(
            stderr,
            "Cleave Move press budget diagnostic: targetsMatch=%d lines=%zu baseline=%zu before=%zu headroom=%zu worksetFloor=%zu body=%d endpointClearance=%.3f started=%d atPress=%zu afterRelease=%zu revisionBefore=%llu revisionAfter=%llu unchangedDraft=%d unchangedOwnership=%d\n",
            smallRouteTargetsMatch ? 1 : 0,
            lineCount,
            baselineCopyBytes,
            storageBefore,
            headroomAfterGestureBaseline,
            affectedMaskLowerBound,
            body.has_value() ? 1 : 0,
            nearestEndpointPixels,
            dragStartedAtPress ? 1 : 0,
            storageAtPress,
            fixture.view.stagedCleaveHistoryStorageBytesEstimate(),
            static_cast<unsigned long long>(revisionBefore),
            static_cast<unsigned long long>(
                fixture.view.stagedCleaveAuthoredRevision()),
            unchangedDraft ? 1 : 0,
            unchangedOwnership ? 1 : 0);
    }

    check(budgetFixtureFits,
          "Move-press budget fixture admits the history baseline but leaves less headroom than its full affected-line workset");
    check(smallRouteEnteredMove,
          "a small control fixture routes an endpoint-clear body press into the staged Move gesture");
    check(smallRouteTargetsMatch,
          "Move-press budget control starts with a matching Surface target context");
    check(bodyPickClear,
          "Move-press budget fixture clicks the line body outside every endpoint hit target");
    check(atomicRefusal,
          "a Move press that cannot fit its affected-line workset is refused before allocation or draft/history/selection/revision changes");
}

} // namespace

void runCleaveAdvancedInteractionTests()
{
    {
        CleaveViewportFixture fixture{};
        fixture.view.setCleaveAnglesVisible(true);
        fixture.view.setCleaveLengthsVisible(false);
        fixture.view.setTransformGridSnapEnabled(false);
        fixture.view.setCleaveAngleStepDegrees(0.0);
        const std::array<QVector3D, 4> corners{{{-0.8f, 0, -0.5f}, {0.8f, 0, -0.5f},
            {0.8f, 0, 0.5f}, {-0.8f, 0, 0.5f}}};
        for (std::size_t index = 0; index < corners.size(); ++index)
            fixture.addLine(corners[index], corners[(index + 1u) % corners.size()]);
        const auto rightAngles = [&]()
        {
            const auto& guides = fixture.view.stagedCleaveJointAnglesForTest();
            return guides.size() == 4 && std::all_of(guides.begin(), guides.end(), [](const auto& guide)
                { return close(guide.degrees, 90.0, 1.0e-8); });
        };
        (void)captureViewportFramebuffer(fixture.view);
        check(rightAngles(), "rendered independent Rectangle edges show four actual right-angle corners");
        const auto* cachedGuides = fixture.view.stagedCleaveJointAnglesForTest().data();
        fixture.view.setOrbitViewImmediate(18.0f, 32.0f, 14.0f);
        (void)captureViewportFramebuffer(fixture.view);
        check(rightAngles() && cachedGuides == fixture.view.stagedCleaveJointAnglesForTest().data(),
            "camera-only rotation projects cached world corner angles without rebuilding or changing their values");
        const auto original = fixture.view.recoveryState();
        const auto start = project(fixture.view, corners[2]);
        const auto end = project(fixture.view, QVector3D(1.2f, 0, 0.8f));
        if (start && end)
        {
            sendMouse(fixture.view, QEvent::MouseButtonPress, *start, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            sendMouse(fixture.view, QEvent::MouseMove, *end, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        }
        (void)captureViewportFramebuffer(fixture.view);
        const auto& changedGuides = fixture.view.stagedCleaveJointAnglesForTest();
        check(start && end && changedGuides.size() == 4
                && std::any_of(changedGuides.begin(), changedGuides.end(), [](const auto& guide)
                    { return std::abs(guide.degrees - 90.0) > 1.0; }),
            "endpoint drag previews invalidate corner measurements before the authored revision commits");
        key(fixture.view, QEvent::KeyPress, Qt::Key_Escape);
        (void)captureViewportFramebuffer(fixture.view);
        check(rightAngles() && sameLines(original.cleaveLines, fixture.view.recoveryState().cleaveLines),
            "Escape restores exact Rectangle handles and right-angle guide cache after a transient endpoint edit");
    }

    {
        CleaveViewportFixture fixture{};
        const QVector3D first(-1.0f, 0.0f, 0.0f);
        const QVector3D second(1.0f, 0.0f, 0.0f);
        fixture.addLine(first, second);
        fixture.view.setTransformGridSnapEnabled(true);
        const QVector3D midpoint = (first + second) * 0.5f;
        const GlView::RecoveryState beforePlain = fixture.view.recoveryState();
        const bool plainDragProjected = fixture.dragWorld(
            midpoint,
            midpoint + QVector3D(0.37f, 0.0f, 0.31f),
            Qt::NoModifier);
        const QVector3D plainAnchor = fixture.view.stagedCleavePlanePointForTest(0);
        const bool defaultSnapOnGrid =
            close(plainAnchor.x() / 0.5, std::round(plainAnchor.x() / 0.5), 1.0e-4)
            && close(plainAnchor.z() / 0.5, std::round(plainAnchor.z() / 0.5), 1.0e-4);
        const QVector3D plainDelta = plainAnchor - toVector(beforePlain.cleaveLines[0].first);
        check(plainDragProjected && defaultSnapOnGrid
                  && plainDelta.lengthSquared() > 0.1f
                  && fixture.sourceGeometryProjectsInside(),
              "enabled shared Grid Snap applies to a plain staged-line move");

        const GlView::RecoveryState afterPlain = fixture.view.recoveryState();
        const QVector3D movedMidpoint =
            (lineFirst(afterPlain, 0) + lineSecond(afterPlain, 0)) * 0.5f;
        const bool controlDragProjected = fixture.dragWorld(
            movedMidpoint,
            movedMidpoint + QVector3D(0.21f, 0.0f, 0.14f),
            Qt::ControlModifier);
        const QVector3D controlAnchor = fixture.view.stagedCleavePlanePointForTest(0);
        const QVector3D controlDelta = controlAnchor - plainAnchor;
        const GlView::RecoveryState afterControl = fixture.view.recoveryState();
        const bool controlInvertsGrid =
            !close(controlAnchor.x() / 0.5, std::round(controlAnchor.x() / 0.5), 1.0e-4)
            || !close(controlAnchor.z() / 0.5, std::round(controlAnchor.z() / 0.5), 1.0e-4);
        check(controlDragProjected && controlInvertsGrid
                  && controlDelta.lengthSquared() > 0.01f
                  && afterControl.selectedCleaveLines == std::vector<std::int32_t>{0},
              "Ctrl temporarily inverts enabled Grid Snap for staged-line movement");
    }

    {
        CleaveViewportFixture fixture{};
        const QVector3D first(-0.7f, 0.0f, 0.0f);
        const QVector3D fixed(0.1f, 0.0f, 0.0f);
        fixture.addLine(first, fixed);
        fixture.view.setCleaveAngleStepDegrees(45.0);
        const auto start = project(fixture.view, first);
        const QVector3D requestedFirst(-0.35f, 0.0f, 0.28f);
        const auto destination = project(fixture.view, requestedFirst);
        if ( start && destination )
        {
            drag(fixture.view, *start, *destination, Qt::NoModifier);
        }
        const QVector3D resultDirection = fixture.view.stagedCleaveLineSecondForTest(0)
            - fixture.view.stagedCleaveLineFirstForTest(0);
        const double angleDegrees = std::atan2(
            resultDirection.z(), resultDirection.x()) * 180.0 / 3.14159265358979323846;
        const bool endpointMoved = !closeQVec(
            fixture.view.stagedCleaveLineFirstForTest(0),
            first,
            1.0e-5f);
        check(start && destination && endpointMoved && resultDirection.lengthSquared() > 0.0f
                  && fixture.sourceGeometryProjectsInside()
                  && std::abs(angleDegrees + 45.0) < 1.0,
              "an existing endpoint drag uses the configured angle step without a modifier");
    }

    {
        CleaveViewportFixture fixture{};
        const QVector3D fixed(0.0f, 0.0f, 0.0f);
        const QVector3D almostOpposite(-0.7f, 0.0f, 0.1234f);
        fixture.addLine(fixed, almostOpposite);
        GlView::RecoveryState::CleaveDraftSnapshot wrapBaseline =
            fixture.view.captureStagedCleaveDraftState();
        wrapBaseline.lines[0].angleReferenceAxis = TVector{ 1.0, 0.0, 0.0 };
        fixture.view.restoreStagedCleaveDraftState(wrapBaseline);
        fixture.view.setCleaveAngleStepDegrees(15.0);
        const auto start = project(fixture.view, almostOpposite);
        const auto destination = project(
            fixture.view,
            QVector3D(-0.7f, 0.0f, -0.0081f));
        if ( start && destination )
        {
            drag(fixture.view, *start, *destination, Qt::NoModifier);
        }
        const QVector3D direction = fixture.view.stagedCleaveLineSecondForTest(0)
            - fixture.view.stagedCleaveLineFirstForTest(0);
        const double angle = std::atan2(direction.z(), direction.x())
            * 180.0 / 3.14159265358979323846;
        const bool movedAcrossWrap = !closeQVec(
            fixture.view.stagedCleaveLineSecondForTest(0),
            almostOpposite,
            1.0e-5f);
        check(start && destination && movedAcrossWrap && direction.lengthSquared() > 0.0f
                  && fixture.sourceGeometryProjectsInside()
                  && std::abs(std::abs(angle) - 180.0) < 1.0,
              "angle-step snapping crosses the signed 180-degree boundary without a discontinuity");
    }

    {
        CleaveViewportFixture fixture{};
        const QVector3D joint(0.0f, 0.0f, 0.0f);
        const QVector3D selectedEnd(0.9f, 0.0f, 0.0f);
        const QVector3D neighborEnd(0.9f, 0.0f, 0.9f);
        fixture.addLine(QVector3D(-0.9f, 0.0f, 0.0f), joint, false, true);
        fixture.addLine(joint, selectedEnd, true, true);
        fixture.addLine(selectedEnd, neighborEnd, true, false);
        fixture.view.selectStagedCleaveLineForTest(-1);
        const GlView::RecoveryState unselectedState = fixture.view.recoveryState();
        const auto firstLineMidpoint = project(
            fixture.view,
            (lineFirst(unselectedState, 0) + lineSecond(unselectedState, 0)) * 0.5f);
        const auto secondLineMidpoint = project(
            fixture.view,
            (lineFirst(unselectedState, 1) + lineSecond(unselectedState, 1)) * 0.5f);
        const QImage unselectedFramebuffer = captureViewportFramebuffer(fixture.view);
        const bool pickedFirst = fixture.clickBody(0, Qt::NoModifier);
        const bool pickedSecond = fixture.clickBody(1, Qt::ShiftModifier);
        const QImage multiSelectedFramebuffer = captureViewportFramebuffer(fixture.view);
        const bool firstMemberVisiblySelected = firstLineMidpoint
            && framebufferPatchChanged(
                unselectedFramebuffer,
                multiSelectedFramebuffer,
                *firstLineMidpoint);
        const bool secondMemberVisiblySelected = secondLineMidpoint
            && framebufferPatchChanged(
                unselectedFramebuffer,
                multiSelectedFramebuffer,
                *secondLineMidpoint);
        const GlView::RecoveryState beforePlainMove = fixture.view.recoveryState();
        const bool plainMoved = fixture.dragWorld(
            QVector3D(-0.45f, 0.0f, 0.0f),
            QVector3D(-0.45f, 0.0f, 0.12f),
            Qt::NoModifier);
        const GlView::RecoveryState afterPlainMove = fixture.view.recoveryState();
        const bool plainMoveRetainedSelection = afterPlainMove.selectedCleaveLines
            == std::vector<std::int32_t>{0, 1};
        const bool plainMoveKeptLinkedNeighborJoined =
            closeQVec(lineFirst(afterPlainMove, 2),
                      lineSecond(afterPlainMove, 1),
                      1.0e-5f)
            && closeQVec(lineSecond(afterPlainMove, 2),
                         lineSecond(beforePlainMove, 2),
                         1.0e-5f);
        const auto before = fixture.view.recoveryState();
        const bool groupSelected = before.selectedCleaveLines
            == std::vector<std::int32_t>{0, 1};
        const QVector3D altDragStart =
            (lineFirst(before, 0) + lineSecond(before, 0)) * 0.5f;
        const bool moved = fixture.dragWorld(
            altDragStart,
            altDragStart + QVector3D(0.0f, 0.0f, 0.24f),
            Qt::AltModifier);
        const GlView::RecoveryState after = fixture.view.recoveryState();
        const QVector3D groupDelta = lineFirst(after, 0) - lineFirst(before, 0);
        const bool groupMovedRigidly =
            closeQVec(lineSecond(after, 0) - lineSecond(before, 0), groupDelta, 1.0e-5f)
            && closeQVec(lineFirst(after, 1) - lineFirst(before, 1), groupDelta, 1.0e-5f)
            && closeQVec(lineSecond(after, 1) - lineSecond(before, 1), groupDelta, 1.0e-5f)
            && closeQVec(lineSecond(after, 0), lineFirst(after, 1), 1.0e-5f);
        const bool outsideNeighborStayedPinned =
            closeQVec(lineFirst(after, 2), lineFirst(before, 2), 1.0e-5f)
            && closeQVec(lineSecond(after, 2), lineSecond(before, 2), 1.0e-5f);
        check(fixture.sourceGeometryProjectsInside()
                  && fixture.allBodyPicksClearOfEndpoints
                  && pickedFirst && pickedSecond && plainMoved
                  && firstMemberVisiblySelected && secondMemberVisiblySelected
                  && plainMoveRetainedSelection && plainMoveKeptLinkedNeighborJoined
                  && groupSelected && moved
                  && groupDelta.lengthSquared() > 0.01f
                  && groupMovedRigidly && outsideNeighborStayedPinned
                  && after.cleaveLines[0].linkedToNext
                  && after.cleaveLines[1].linkedToPrevious
                  && !after.cleaveLines[1].linkedToNext
                  && !after.cleaveLines[2].linkedToPrevious
                  && after.selectedCleaveLines == std::vector<std::int32_t>{0, 1},
              "both members of a multi-line selection are visibly highlighted, and Alt detaches the selected group from its outside linked neighbor");
    }

    {
        CleaveViewportFixture fixture{};
        fixture.addLine(QVector3D(-0.8f, 0.0f, -0.6f), QVector3D(0.8f, 0.0f, -0.6f));
        fixture.addLine(QVector3D(-0.8f, 0.0f, 0.0f), QVector3D(0.8f, 0.0f, 0.0f));
        fixture.addLine(QVector3D(-0.8f, 0.0f, 0.6f), QVector3D(0.8f, 0.0f, 0.6f));
        const bool firstPicked = fixture.clickBody(0, Qt::NoModifier);
        const bool secondAdded = fixture.clickBody(1, Qt::ShiftModifier);
        const bool ctrlRemoved = fixture.clickBody(0, Qt::ControlModifier);
        const auto afterCtrlRemove = fixture.view.recoveryState();
        const bool ctrlOnUnselectedNoOp = fixture.clickBody(0, Qt::ControlModifier);
        const auto afterCtrlUnselected = fixture.view.recoveryState();
        const bool shiftReadded = fixture.clickBody(0, Qt::ShiftModifier);
        const GlView::RecoveryState before = fixture.view.recoveryState();
        const bool deleted = fixture.view.deleteSelectedStagedCleaveLine();
        const GlView::RecoveryState afterDelete = fixture.view.recoveryState();
        const bool oneBatchDelete = deleted
            && afterDelete.cleaveLines.size() == 1
            && closeVec(afterDelete.cleaveLines.front().first,
                        before.cleaveLines[2].first,
                        1.0e-8)
            && closeVec(afterDelete.cleaveLines.front().second,
                        before.cleaveLines[2].second,
                        1.0e-8);
        const bool oneUndoRestoresBatch = fixture.view.undoStagedCleaveHistory()
            && sameLines(fixture.view.recoveryState().cleaveLines, before.cleaveLines);
        check(fixture.sourceGeometryProjectsInside()
                  && fixture.allBodyPicksClearOfEndpoints
                  && firstPicked && secondAdded && ctrlRemoved
                  && ctrlOnUnselectedNoOp && shiftReadded
                  && afterCtrlRemove.selectedCleaveLines == std::vector<std::int32_t>{1}
                  && afterCtrlUnselected.selectedCleaveLines
                      == std::vector<std::int32_t>{1}
                  && before.selectedCleaveLines.size() == 2
                  && oneBatchDelete && oneUndoRestoresBatch,
              "Ctrl removes and re-adds a staged line in the selection, then Delete removes the batch in one local edit");
    }

    {
        CleaveViewportFixture fixture{};
        fixture.addLine(QVector3D(-0.8f, 0.0f, -0.6f), QVector3D(0.8f, 0.0f, -0.6f));
        fixture.addLine(QVector3D(-0.8f, 0.0f, 0.0f), QVector3D(0.8f, 0.0f, 0.0f));
        fixture.addLine(QVector3D(-0.8f, 0.0f, 0.6f), QVector3D(0.8f, 0.0f, 0.6f));
        fixture.view.selectStagedCleaveLineForTest(0);
        const auto start = project(fixture.view, QVector3D(-1.2f, 0.0f, -0.85f));
        const auto end = project(fixture.view, QVector3D(1.2f, 0.0f, 0.25f));
        const GlView::RecoveryState before = fixture.view.recoveryState();
        if ( start && end )
        {
            drag(fixture.view, *start, *end, Qt::AltModifier);
        }
        const GlView::RecoveryState afterAdd = fixture.view.recoveryState();
        const auto removeStart = project(fixture.view, QVector3D(-1.2f, 0.0f, -0.85f));
        const auto removeEnd = project(fixture.view, QVector3D(1.2f, 0.0f, -0.35f));
        if ( removeStart && removeEnd )
        {
            drag(fixture.view,
                 *removeStart,
                 *removeEnd,
                 Qt::AltModifier | Qt::ControlModifier);
        }
        const GlView::RecoveryState afterRemove = fixture.view.recoveryState();
        check(fixture.sourceGeometryProjectsInside()
                  && start && end && removeStart && removeEnd
                  && afterAdd.selectedCleaveLines == std::vector<std::int32_t>{0, 1}
                  && afterRemove.selectedCleaveLines == std::vector<std::int32_t>{1}
                  && sameLines(afterRemove.cleaveLines, before.cleaveLines),
              "Alt-drag adds staged lines inside a marquee and Alt+Ctrl-drag removes them without changing geometry");
    }

    {
        CleaveViewportFixture fixture{};
        const QVector3D joint(0.2f, 0.0f, -0.3f);
        const QVector3D first(-1.2f, 0.0f, -0.3f);
        const QVector3D second(1.0f, 0.0f, -0.3f);
        const QVector3D neighborEnd(0.2f, 0.0f, 0.8f);
        const QVector3D pivot(0.0f, 0.0f, -0.3f);
        fixture.addLine(first, joint, false, true);
        fixture.addLine(joint, second, true, false);
        fixture.addLine(joint, neighborEnd);
        fixture.addLineWithPlaneNormal(
            QVector3D(-0.7f, 0.0f, 1.4f),
            QVector3D(0.7f, 0.0f, 1.4f),
            QVector3D(0.0f, 1.0f, 0.0f));
        const bool firstPicked = fixture.clickBody(0, Qt::NoModifier);
        const bool secondAdded = fixture.clickBody(1, Qt::ShiftModifier);
        const auto before = fixture.view.recoveryState();
        const bool eligible = fixture.view.canTransformSelectedStagedCleave(
            GlView::StagedCleaveTransformMode::Rotate);
        fixture.view.setCleaveAngleStepDegrees(45.0);
        fixture.view.setStagedCleaveTransformMode(
            GlView::StagedCleaveTransformMode::Rotate);
        const auto projectedPivot = project(fixture.view, pivot);
        const QRect transformHudRegion = projectedPivot
            ? QRect(static_cast<int>(projectedPivot->x()) - 116,
                static_cast<int>(projectedPivot->y()) + 22, 232, 48)
            : QRect{};
        const QImage firstRotateHudBaseline = captureViewportFramebuffer(fixture.view);
        const QImage beforeRotateHud = captureViewportFramebuffer(fixture.view);
        const int firstRotateBaselinePixels = framebufferBrightNeutralPixelCount(
            firstRotateHudBaseline,
            transformHudRegion);
        const int rotateBaselinePixels = framebufferBrightNeutralPixelCount(
            beforeRotateHud,
            transformHudRegion);
        const bool rotateBaselineSettled =
            std::abs(rotateBaselinePixels - firstRotateBaselinePixels) <= 2;

        const auto rotateStart = project(fixture.view, QVector3D(0.6f, 0.0f, -0.3f));
        const auto rotateEnd = project(
            fixture.view,
            pivot + QVector3D(0.0f, 0.0f, 0.6f));
        if ( rotateStart && rotateEnd )
        {
            sendMouse(
                fixture.view,
                QEvent::MouseButtonPress,
                *rotateStart,
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::NoModifier);
            sendMouse(
                fixture.view,
                QEvent::MouseMove,
                *rotateEnd,
                Qt::NoButton,
                Qt::LeftButton,
                Qt::NoModifier);
        }
        const QString rotateHud = fixture.view.stagedCleaveTransformHudTextForTest();
        const QImage duringRotateHud = captureViewportFramebufferWithHudText(
            fixture.view,
            beforeRotateHud,
            transformHudRegion);
        const QRectF rotateBadge = fixture.view.stagedCleaveTransformHudRectForTest();
        const bool rotateBadgePositioned = projectedPivot && !rotateBadge.isEmpty()
            && QRectF(fixture.view.rect()).adjusted(8, 8, -8, -8).contains(rotateBadge)
            && std::abs(rotateBadge.center().x() - projectedPivot->x()) < 1.0
            && std::abs(rotateBadge.top() - projectedPivot->y() - 24.0) < 1.0;
        const bool rotateHudPainted = framebufferHudTextAppeared(
            beforeRotateHud,
            duringRotateHud,
            transformHudRegion);
        const QVector3D previewRotateFirst = fixture.view.stagedCleaveLineFirstForTest(0);
        const QVector3D previewRotateSecond = fixture.view.stagedCleaveLineSecondForTest(0);
        const bool rotatePreviewChanged =
            !closeQVec(previewRotateFirst, lineFirst(before, 0), 1.0e-5f)
            || !closeQVec(previewRotateSecond, lineSecond(before, 0), 1.0e-5f);
        if ( rotateStart && rotateEnd )
        {
            sendMouse(
                fixture.view,
                QEvent::MouseButtonRelease,
                *rotateEnd,
                Qt::LeftButton,
                Qt::NoButton,
                Qt::NoModifier);
        }
        const bool rotated = rotateStart && rotateEnd && rotatePreviewChanged;
        const auto afterRotate = fixture.view.recoveryState();
        const QVector3D beforeDirection = lineSecond(before, 0) - lineFirst(before, 0);
        const QVector3D afterDirection = lineSecond(afterRotate, 0) - lineFirst(afterRotate, 0);
        const bool directionChanged = std::abs(
            QVector3D::dotProduct(beforeDirection.normalized(), afterDirection.normalized()))
            < 0.999f;
        const double rotationDegrees = std::atan2(afterDirection.z(), afterDirection.x())
            * 180.0 / 3.14159265358979323846;
        const double snappedRotationDegrees = std::round(rotationDegrees / 45.0) * 45.0;
        const bool rotationUsesAngleStep =
            std::abs(rotationDegrees - snappedRotationDegrees) < 1.0;
        const QVector3D expectedFirst0 = pivot + QVector3D(0.0f, 0.0f, -1.2f);
        const QVector3D expectedJoint = pivot + QVector3D(0.0f, 0.0f, 0.2f);
        const QVector3D expectedSecond1 = pivot + QVector3D(0.0f, 0.0f, 1.0f);
        const bool selectedGeometryRotatedAroundPivot =
            closeQVec(lineFirst(afterRotate, 0), expectedFirst0, 0.02f)
            && closeQVec(lineSecond(afterRotate, 0), expectedJoint, 0.02f)
            && closeQVec(lineFirst(afterRotate, 1), expectedJoint, 0.02f)
            && closeQVec(lineSecond(afterRotate, 1), expectedSecond1, 0.02f)
            && std::abs((lineSecond(afterRotate, 0) - lineFirst(afterRotate, 0)).length()
                        - (lineSecond(before, 0) - lineFirst(before, 0)).length()) < 0.02f
            && std::abs((lineSecond(afterRotate, 1) - lineFirst(afterRotate, 1)).length()
                        - (lineSecond(before, 1) - lineFirst(before, 1)).length()) < 0.02f;
        const bool neighborOtherEndpointPinned =
            closeQVec(lineFirst(afterRotate, 2), expectedJoint, 0.02f)
            && closeQVec(lineSecond(afterRotate, 2), neighborEnd, 1.0e-5f);
        const double expectedHudAngle = std::atan2(
            QVector3D::dotProduct(toVector(before.cleaveLines[0].constructionPlaneNormal),
                QVector3D::crossProduct(beforeDirection, afterDirection)),
            QVector3D::dotProduct(beforeDirection, afterDirection)) * 180.0 / 3.14159265358979323846;
        check(fixture.sourceGeometryProjectsInside()
                  && fixture.allBodyPicksClearOfEndpoints
                  && firstPicked && secondAdded && eligible && rotated && directionChanged
                  && rotationUsesAngleStep && selectedGeometryRotatedAroundPivot
                  && neighborOtherEndpointPinned
                  && rotateHud == QStringLiteral("Rotate %1°")
                      .arg(expectedHudAngle, 0, 'f', 2),
              "Rotate transforms the common-plane selection with a snapped angle HUD and pins the attached neighbor's other end");
        check(rotateHudPainted && rotateBaselineSettled && rotateBadgePositioned,
              "Rotate paints a complete badge below the selected cuts' center inside the viewport");

        const GlView::RecoveryState beforeScale = fixture.view.recoveryState();
        fixture.view.setStagedCleaveTransformMode(
            GlView::StagedCleaveTransformMode::Scale);
        const QImage firstScaleHudBaseline = captureViewportFramebuffer(fixture.view);
        const QImage beforeScaleHud = captureViewportFramebuffer(fixture.view);
        const int firstScaleBaselinePixels = framebufferBrightNeutralPixelCount(
            firstScaleHudBaseline,
            transformHudRegion);
        const int scaleBaselinePixels = framebufferBrightNeutralPixelCount(
            beforeScaleHud,
            transformHudRegion);
        const bool scaleBaselineSettled =
            std::abs(scaleBaselinePixels - firstScaleBaselinePixels) <= 2;

        const QVector3D scalePivot = pivot;
        const QVector3D scaleStartWorld =
            (lineFirst(beforeScale, 1) + lineSecond(beforeScale, 1)) * 0.5f;
        const QVector3D scaleEndWorld =
            scalePivot + (scaleStartWorld - scalePivot) * 2.0f;
        const auto scaleStart = project(fixture.view, scaleStartWorld);
        const auto scaleEnd = project(fixture.view, scaleEndWorld);
        double expectedScaleFactor = std::numeric_limits<double>::quiet_NaN();
        if ( scaleStart && scaleEnd && beforeScale.cleaveLines.size() >= 2u )
        {
            // The gesture uses integer pointer coordinates. Its common pivot is
            // the centroid of these three distinct authored handles, not the
            // nominal float pivot used to choose an approximately 2x drag.
            const TVector authoredPivot = (beforeScale.cleaveLines[0].first
                + beforeScale.cleaveLines[0].second
                + beforeScale.cleaveLines[1].second) * (1.0 / 3.0);
            TVector constructionNormal =
                beforeScale.cleaveLines[0].constructionPlaneNormal;
            const double normalLength = constructionNormal.length();
            if ( std::isfinite(normalLength) && normalLength > 0.0 )
            {
                constructionNormal = constructionNormal * (1.0 / normalLength);
                const auto startHit = fixture.view.viewportPointOnPlane(
                    scaleStart->toPoint(), authoredPivot, constructionNormal);
                const auto endHit = fixture.view.viewportPointOnPlane(
                    scaleEnd->toPoint(), authoredPivot, constructionNormal);
                if ( startHit && endHit )
                {
                    const auto planeRadius = [&authoredPivot, &constructionNormal](
                                                 const TVector& point)
                    {
                        const TVector radial = point - authoredPivot;
                        return (radial - constructionNormal
                            * radial.dot(constructionNormal)).length();
                    };
                    const double startRadius = planeRadius(*startHit);
                    if ( std::isfinite(startRadius) && startRadius > 0.0 )
                    {
                        expectedScaleFactor = planeRadius(*endHit) / startRadius;
                    }
                }
            }
        }
        const float selectedLengthBefore =
            (lineSecond(beforeScale, 0) - lineFirst(beforeScale, 0)).length();
        if ( scaleStart && scaleEnd )
        {
            sendMouse(
                fixture.view,
                QEvent::MouseButtonPress,
                *scaleStart,
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::NoModifier);
            sendMouse(
                fixture.view,
                QEvent::MouseMove,
                *scaleEnd,
                Qt::NoButton,
                Qt::LeftButton,
                Qt::NoModifier);
        }
        const QString scaleHud = fixture.view.stagedCleaveTransformHudTextForTest();
        const QImage duringScaleHud = captureViewportFramebufferWithHudText(
            fixture.view,
            beforeScaleHud,
            transformHudRegion);
        const QRectF scaleBadge = fixture.view.stagedCleaveTransformHudRectForTest();
        const bool scaleBadgePositioned = projectedPivot && !scaleBadge.isEmpty()
            && QRectF(fixture.view.rect()).adjusted(8, 8, -8, -8).contains(scaleBadge)
            && std::abs(scaleBadge.center().x() - projectedPivot->x()) < 1.0
            && std::abs(scaleBadge.top() - projectedPivot->y() - 24.0) < 1.0;
        const bool scaleHudPainted = framebufferHudTextAppeared(
            beforeScaleHud,
            duringScaleHud,
            transformHudRegion);
        const float previewScaleLength =
            (fixture.view.stagedCleaveLineSecondForTest(0)
             - fixture.view.stagedCleaveLineFirstForTest(0)).length();
        const bool scalePreviewChanged = selectedLengthBefore > 0.0f
            && std::abs(previewScaleLength / selectedLengthBefore - 2.0f) < 0.03f;
        if ( scaleStart && scaleEnd )
        {
            sendMouse(
                fixture.view,
                QEvent::MouseButtonRelease,
                *scaleEnd,
                Qt::LeftButton,
                Qt::NoButton,
                Qt::NoModifier);
        }
        const GlView::RecoveryState afterScale = fixture.view.recoveryState();
        const float selectedLengthAfter =
            (lineSecond(afterScale, 0) - lineFirst(afterScale, 0)).length();
        const bool scaledUniformly = selectedLengthBefore > 0.0f
            && std::abs(selectedLengthAfter / selectedLengthBefore - 2.0f) < 0.03f;
        const bool allSelectedEndpointsScaled =
            closeQVec(lineFirst(afterScale, 0),
                      scalePivot + QVector3D(0.0f, 0.0f, -2.4f),
                      0.03f)
            && closeQVec(lineSecond(afterScale, 0),
                         scalePivot + QVector3D(0.0f, 0.0f, 0.4f),
                         0.03f)
            && closeQVec(lineFirst(afterScale, 1),
                         scalePivot + QVector3D(0.0f, 0.0f, 0.4f),
                         0.03f)
            && closeQVec(lineSecond(afterScale, 1),
                         scalePivot + QVector3D(0.0f, 0.0f, 2.0f),
                         0.03f)
            && closeQVec(lineFirst(afterScale, 2),
                         scalePivot + QVector3D(0.0f, 0.0f, 0.4f),
                         0.03f)
            && closeQVec(lineSecond(afterScale, 2), neighborEnd, 1.0e-5f);
        const QString scaleHudPrefix = QStringLiteral("Scale ");
        const bool scaleHudFramed = scaleHud.startsWith(scaleHudPrefix)
            && scaleHud.endsWith(QChar(0x00d7));
        const QString scaleHudNumber = scaleHudFramed
            ? scaleHud.mid(scaleHudPrefix.size(),
                           scaleHud.size() - scaleHudPrefix.size() - 1)
            : QString{};
        bool scaleHudParsed = false;
        const double scaleHudFactor = scaleHudNumber.toDouble(&scaleHudParsed);
        const qsizetype scaleHudDecimal = scaleHudNumber.indexOf(QLatin1Char('.'));
        const bool scaleHudFormatting = scaleHudFramed
            && scaleHudNumber == scaleHudNumber.trimmed()
            && scaleHudDecimal > 0
            && scaleHudNumber.size() - scaleHudDecimal - 1 == 2
            && !scaleHudNumber.contains(QLatin1Char('e'), Qt::CaseInsensitive);
        // Display precision is two decimals; geometry is checked independently
        // against the ray-derived factor and all transformed endpoint positions.
        const bool scaleHudMatchesFactor = scaleHudFormatting && scaleHudParsed
            && std::isfinite(expectedScaleFactor) && expectedScaleFactor > 0.0
            && std::isfinite(scaleHudFactor) && scaleHudFactor > 0.0
            && scaleHudNumber == QString::number(expectedScaleFactor, 'f', 2);
        check(scaleStart && scaleEnd && scalePreviewChanged && scaledUniformly
                  && allSelectedEndpointsScaled
                  && scaleHudMatchesFactor,
              "Scale previews selected lines about their common pivot and reports its positive factor before release");
        if ( !scaleStart || !scaleEnd || !scalePreviewChanged || !scaledUniformly
            || !allSelectedEndpointsScaled || !scaleHudMatchesFactor )
        {
            const QByteArray hudBytes = scaleHud.toUtf8();
            std::fprintf(stderr,
                "Scale contract diagnostic: start=%d end=%d previewChanged=%d uniform=%d endpoints=%d "
                "hud=[%s] expectedFactor=%.17g displayedFactor=%.17g formatted=%d parsed=%d matches=%d "
                "lengths=(%.12g,%.12g,%.12g) ratios=(%.12g,%.12g) selection=%zu mode=%d\n",
                scaleStart ? 1 : 0, scaleEnd ? 1 : 0,
                scalePreviewChanged ? 1 : 0, scaledUniformly ? 1 : 0,
                allSelectedEndpointsScaled ? 1 : 0, hudBytes.constData(),
                expectedScaleFactor, scaleHudFactor, scaleHudFormatting ? 1 : 0,
                scaleHudParsed ? 1 : 0, scaleHudMatchesFactor ? 1 : 0,
                static_cast<double>(selectedLengthBefore), static_cast<double>(previewScaleLength),
                static_cast<double>(selectedLengthAfter),
                static_cast<double>(previewScaleLength / selectedLengthBefore),
                static_cast<double>(selectedLengthAfter / selectedLengthBefore),
                afterScale.selectedCleaveLines.size(),
                static_cast<int>(fixture.view.stagedCleaveTransformMode()));
            for ( std::size_t index = 0; index < 3u && index < afterScale.cleaveLines.size(); ++index )
            {
                const auto& line = afterScale.cleaveLines[index];
                std::fprintf(stderr,
                    "Scale line %zu: first=(%.12g,%.12g,%.12g) second=(%.12g,%.12g,%.12g)\n",
                    index, line.first.x, line.first.y, line.first.z,
                    line.second.x, line.second.y, line.second.z);
            }
        }
        check(scaleHudPainted && scaleBaselineSettled && scaleBadgePositioned,
              "Scale paints a complete badge below the selected cuts' center inside the viewport");

        const GlView::RecoveryState beforeEscape = fixture.view.recoveryState();
        const auto beforeEscapeHistory = beforeEscape.cleaveHistory;
        const QVector3D escapeStartWorld =
            (lineFirst(beforeEscape, 1) + lineSecond(beforeEscape, 1)) * 0.5f;
        const QVector3D escapeEndWorld =
            scalePivot + (escapeStartWorld - scalePivot) * 0.25f;
        const auto escapeStart = project(fixture.view, escapeStartWorld);
        const auto escapeEnd = project(fixture.view, escapeEndWorld);
        if ( escapeStart && escapeEnd )
        {
            sendMouse(
                fixture.view,
                QEvent::MouseButtonPress,
                *escapeStart,
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::NoModifier);
            sendMouse(
                fixture.view,
                QEvent::MouseMove,
                *escapeEnd,
                Qt::NoButton,
                Qt::LeftButton,
                Qt::NoModifier);
        }
        const QVector3D liveFirst0 = fixture.view.stagedCleaveLineFirstForTest(0);
        const QVector3D liveSecond0 = fixture.view.stagedCleaveLineSecondForTest(0);
        const QVector3D liveFirst1 = fixture.view.stagedCleaveLineFirstForTest(1);
        const QVector3D liveSecond1 = fixture.view.stagedCleaveLineSecondForTest(1);
        const bool previewActive = escapeStart && escapeEnd
            && !fixture.view.stagedCleaveTransformHudTextForTest().isEmpty()
            && (!closeQVec(liveFirst0, lineFirst(beforeEscape, 0), 1.0e-5f)
                || !closeQVec(liveSecond0, lineSecond(beforeEscape, 0), 1.0e-5f)
                || !closeQVec(liveFirst1, lineFirst(beforeEscape, 1), 1.0e-5f)
                || !closeQVec(liveSecond1, lineSecond(beforeEscape, 1), 1.0e-5f));
        key(fixture.view, QEvent::KeyPress, Qt::Key_Escape);
        key(fixture.view, QEvent::KeyRelease, Qt::Key_Escape);
        QApplication::processEvents(QEventLoop::AllEvents);
        const bool canceled = fixture.view.stagedCleaveTransformHudTextForTest().isEmpty();
        if ( escapeStart && escapeEnd )
        {
            sendMouse(
                fixture.view,
                QEvent::MouseButtonRelease,
                *escapeEnd,
                Qt::LeftButton,
                Qt::NoButton,
                Qt::NoModifier);
        }
        const GlView::RecoveryState afterEscape = fixture.view.recoveryState();
        check(previewActive && canceled
                  && sameLines(afterEscape.cleaveLines, beforeEscape.cleaveLines)
                  && historyMatches(afterEscape.cleaveHistory, beforeEscapeHistory),
              "Escape cancels an active Scale preview exactly without leaving a local edit");

        const GlView::RecoveryState beforeScaleFloor = fixture.view.recoveryState();
        const QVector3D floorStartWorld =
            (lineFirst(beforeScaleFloor, 0) + lineSecond(beforeScaleFloor, 0)) * 0.5f;
        const auto floorStart = project(fixture.view, floorStartWorld);
        const auto floorEnd = project(fixture.view, scalePivot);
        if ( floorStart && floorEnd )
        {
            drag(fixture.view, *floorStart, *floorEnd, Qt::NoModifier);
        }
        const GlView::RecoveryState afterScaleFloor = fixture.view.recoveryState();
        bool scaleRemainsValid = floorStart && floorEnd;
        for ( const std::int32_t index : afterScaleFloor.selectedCleaveLines )
        {
            if ( index < 0
                || static_cast<std::size_t>(index) >= afterScaleFloor.cleaveLines.size() )
            {
                scaleRemainsValid = false;
                continue;
            }
            const auto& line = afterScaleFloor.cleaveLines[static_cast<std::size_t>(index)];
            const QVector3D firstPoint = toVector(line.first);
            const QVector3D secondPoint = toVector(line.second);
            scaleRemainsValid = scaleRemainsValid
                && std::isfinite(firstPoint.x())
                && std::isfinite(firstPoint.y())
                && std::isfinite(firstPoint.z())
                && std::isfinite(secondPoint.x())
                && std::isfinite(secondPoint.y())
                && std::isfinite(secondPoint.z())
                && (secondPoint - firstPoint).lengthSquared() > 1.0e-8f;
        }
        check(scaleRemainsValid,
              "scaling to the pivot rejects or clamps safely without collapsing selected lines");

        const GlView::RecoveryState beforeSubthresholdScale =
            fixture.view.recoveryState();
        fixture.view.setStagedCleaveTransformMode(
            GlView::StagedCleaveTransformMode::Scale);
        const auto subthresholdStart = project(
            fixture.view,
            (lineFirst(beforeSubthresholdScale, 1)
             + lineSecond(beforeSubthresholdScale, 1)) * 0.5f);
        bool subthresholdPreviewChanged = false;
        bool subthresholdHudShown = false;
        if ( subthresholdStart )
        {
            const QPointF subthresholdEnd = *subthresholdStart + QPointF(1.0, 0.0);
            sendMouse(
                fixture.view,
                QEvent::MouseButtonPress,
                *subthresholdStart,
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::NoModifier);
            sendMouse(
                fixture.view,
                QEvent::MouseMove,
                subthresholdEnd,
                Qt::NoButton,
                Qt::LeftButton,
                Qt::NoModifier);
            subthresholdPreviewChanged =
                !closeQVec(
                    fixture.view.stagedCleaveLineFirstForTest(0),
                    lineFirst(beforeSubthresholdScale, 0),
                    1.0e-6f)
                || !closeQVec(
                    fixture.view.stagedCleaveLineSecondForTest(0),
                    lineSecond(beforeSubthresholdScale, 0),
                    1.0e-6f);
            subthresholdHudShown =
                !fixture.view.stagedCleaveTransformHudTextForTest().isEmpty();
            sendMouse(
                fixture.view,
                QEvent::MouseButtonRelease,
                subthresholdEnd,
                Qt::LeftButton,
                Qt::NoButton,
                Qt::NoModifier);
        }
        const GlView::RecoveryState afterSubthresholdScale =
            fixture.view.recoveryState();
        check(subthresholdStart && subthresholdPreviewChanged
                  && subthresholdHudShown
                  && sameLines(
                      afterSubthresholdScale.cleaveLines,
                      beforeSubthresholdScale.cleaveLines)
                  && historyMatches(
                      afterSubthresholdScale.cleaveHistory,
                      beforeSubthresholdScale.cleaveHistory),
              "a below-threshold Scale preview rolls back exactly without creating an edit");

        fixture.view.selectStagedCleaveLineForTest(0);
        const bool addedIncompatibleLine = fixture.clickBody(3, Qt::ShiftModifier);
        const bool incompatibleTransformDisabled =
            !fixture.view.canTransformSelectedStagedCleave(
                GlView::StagedCleaveTransformMode::Rotate)
            && !fixture.view.canTransformSelectedStagedCleave(
                GlView::StagedCleaveTransformMode::Scale);
        check(addedIncompatibleLine && incompatibleTransformDisabled,
              "Rotate and Scale are unavailable when selected lines do not share a construction plane");
    }

    {
        CleaveViewportFixture fixture{};
        fixture.addLineWithPlaneNormal(
            QVector3D(-1.2f, 0.0f, -0.5f),
            QVector3D(-0.3f, 0.0f, -0.5f),
            fixture.surfaceNormal);
        fixture.addLineWithPlaneNormal(
            QVector3D(0.3f, 0.0f, 0.5f),
            QVector3D(1.2f, 0.0f, 0.5f),
            fixture.surfaceNormal);
        const bool firstPicked = fixture.clickBody(0, Qt::NoModifier);
        const bool secondAdded = fixture.clickBody(1, Qt::ShiftModifier);
        const GlView::RecoveryState beforeEligibility = fixture.view.recoveryState();
        const bool refusedParallelOffsetPlanes =
            !fixture.view.canTransformSelectedStagedCleave(
                GlView::StagedCleaveTransformMode::Rotate)
            && !fixture.view.canTransformSelectedStagedCleave(
                GlView::StagedCleaveTransformMode::Scale);
        const GlView::RecoveryState afterEligibility = fixture.view.recoveryState();
        check(firstPicked && secondAdded
                  && fixture.allBodyPicksClearOfEndpoints
                  && beforeEligibility.selectedCleaveLines
                      == std::vector<std::int32_t>{0, 1}
                  && refusedParallelOffsetPlanes
                  && sameLines(beforeEligibility.cleaveLines,
                               afterEligibility.cleaveLines)
                  && afterEligibility.selectedCleaveLines
                      == beforeEligibility.selectedCleaveLines,
              "Rotate and Scale refuse parallel selected construction planes with a nonzero offset atomically");
    }

    {
        CleaveViewportFixture fixture(true);
        fixture.view.setCameraProjectionType(
            sed::app::ViewportCamera::ProjectionType::Perspective);
        fixture.view.setCameraOrbitDistance(8.0f);
        fixture.view.captureStagedCleaveContextForTest(false);
        const QVector3D joint(0.2f, 0.0f, -0.4f);
        const QVector3D first(-1.2f, 0.0f, -0.4f);
        const QVector3D second(1.0f, 0.0f, -0.4f);
        const QVector3D neighborEnd(0.2f, 0.0f, 0.8f);
        const QVector3D pivot(0.0f, 0.0f, -0.4f);
        fixture.addLine(first, joint, false, true);
        fixture.addLine(joint, second, true, false);
        fixture.addLine(joint, neighborEnd);
        const bool contextReady = fixture.view.stagedCleaveContextMatchesCurrentForTest(false);
        const bool firstPicked = fixture.clickBody(0, Qt::NoModifier);
        const bool secondAdded = fixture.clickBody(1, Qt::ShiftModifier);
        const bool eligible = fixture.view.canTransformSelectedStagedCleave(
            GlView::StagedCleaveTransformMode::Rotate);
        fixture.view.setCleaveAngleStepDegrees(45.0);
        fixture.view.setStagedCleaveTransformMode(
            GlView::StagedCleaveTransformMode::Rotate);
        const QVector3D startWorld(0.6f, 0.0f, -0.4f);
        const QVector3D destinationWorld = pivot + QVector3D(0.0f, 0.0f, 0.6f);
        const auto start = project(fixture.view, startWorld);
        const auto destination = project(fixture.view, destinationWorld);
        if ( start && destination )
        {
            sendMouse(
                fixture.view,
                QEvent::MouseButtonPress,
                *start,
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::NoModifier);
            sendMouse(
                fixture.view,
                QEvent::MouseMove,
                *destination,
                Qt::NoButton,
                Qt::LeftButton,
                Qt::NoModifier);
        }
        const QString hudDuringDrag = fixture.view.stagedCleaveTransformHudTextForTest();
        const QVector3D liveJoint = fixture.view.stagedCleaveLineSecondForTest(0);
        const bool jointPreviewMoved = !closeQVec(liveJoint, joint, 1.0e-5f);
        if ( start && destination )
        {
            sendMouse(
                fixture.view,
                QEvent::MouseButtonRelease,
                *destination,
                Qt::LeftButton,
                Qt::NoButton,
                Qt::NoModifier);
        }
        const GlView::RecoveryState after = fixture.view.recoveryState();
        check(contextReady && fixture.sourceGeometryProjectsInside()
                  && fixture.allBodyPicksClearOfEndpoints
                  && firstPicked && secondAdded && eligible && start && destination
                  && jointPreviewMoved
                  && closeQVec(lineSecond(after, 0), liveJoint, 1.0e-5f)
                  && closeQVec(lineFirst(after, 1), liveJoint, 1.0e-5f)
                  && closeQVec(lineSecond(after, 2), neighborEnd, 1.0e-5f)
                  && hudDuringDrag.contains(QChar(0x00b0)),
              "Sector perspective Rotate uses the common construction plane, previews a snapped HUD angle, and carries only the attached endpoint");
    }

    {
        CleaveViewportFixture fixture{};
        fixture.addLine(QVector3D(-0.8f, 0.0f, -0.6f), QVector3D(-0.2f, 0.0f, -0.6f));
        fixture.addLine(QVector3D(-0.8f, 0.0f, 0.0f), QVector3D(-0.2f, 0.0f, 0.0f));
        fixture.addLine(QVector3D(-0.8f, 0.0f, 0.6f), QVector3D(-0.2f, 0.0f, 0.6f));
        fixture.view.setCleaveLineNumbersVisible(true);
        fixture.view.selectStagedCleaveLineForTest(0);
        const GlView::RecoveryState beforeLabelEdit = fixture.view.recoveryState();
        const QRect label = fixture.view.stagedCleaveOrderLabelRectForTest(0);
        check(label.isValid() && !label.isEmpty(),
              "visible line-number label exposes the same hit rectangle used for inline order editing");
        if ( label.isValid() && !label.isEmpty() )
        {
            bool editorReceivedNativeFocus = false;
            const auto openEditor = [&fixture, &editorReceivedNativeFocus](
                                        const std::int32_t physicalLine)
            {
                const QRect currentLabel =
                    fixture.view.stagedCleaveOrderLabelRectForTest(physicalLine);
                if ( !currentLabel.isValid() || currentLabel.isEmpty() )
                {
                    return static_cast<QLineEdit*>(nullptr);
                }
                fixture.view.setFocus(Qt::OtherFocusReason);
                QApplication::processEvents(QEventLoop::AllEvents);
                const bool viewportHadFocusBeforeClick =
                    QApplication::focusWidget() == &fixture.view;
                click(fixture.view, currentLabel.center());
                QLineEdit* const editor = fixture.view.findChild<QLineEdit*>(
                    QStringLiteral("cleaveLineOrderEditor"));
                editorReceivedNativeFocus = viewportHadFocusBeforeClick
                    && editor
                    && QApplication::focusWidget() == editor;
                return editor;
            };
            const auto pressEnter = [](QLineEdit& editor)
            {
                key(editor, QEvent::KeyPress, Qt::Key_Return);
                key(editor, QEvent::KeyRelease, Qt::Key_Return);
                QApplication::processEvents(QEventLoop::AllEvents);
            };
            QLineEdit* const editor = openEditor(0);
            check(fixture.sourceGeometryProjectsInside()
                      && editorReceivedNativeFocus
                      && editor != nullptr && editor->hasFocus()
                      && QApplication::focusWidget() == editor,
                  "clicking a visible line number opens its focused inline rank editor");
            if ( editor )
            {
                editor->selectAll();
                key(*editor, QEvent::KeyPress, Qt::Key_2, Qt::NoModifier, QStringLiteral("2"));
                key(*editor, QEvent::KeyRelease, Qt::Key_2, Qt::NoModifier, QStringLiteral("2"));
                key(*editor, QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier);
                key(*editor, QEvent::KeyRelease, Qt::Key_Z, Qt::ControlModifier);
                QApplication::processEvents(QEventLoop::AllEvents);
                check(editor->text() == QStringLiteral("1")
                          && fixture.view.recoveryState().applyOrder
                              == std::vector<std::int32_t>{0, 1, 2},
                      "Ctrl+Z edits the inline rank text before it can consume draft or document history");

                editor->setText(QString{});
                pressEnter(*editor);
                check(fixture.view.recoveryState().applyOrder
                          == std::vector<std::int32_t>{0, 1, 2},
                      "empty rank text leaves execution order unchanged");

                QLineEdit* const invalidEditor = openEditor(0);
                if ( invalidEditor )
                {
                    invalidEditor->setText(QStringLiteral("12x"));
                    pressEnter(*invalidEditor);
                }
                check(invalidEditor != nullptr
                          && fixture.view.recoveryState().applyOrder
                              == std::vector<std::int32_t>{0, 1, 2},
                      "nonnumeric rank text leaves execution order unchanged");

                QLineEdit* const canceledEditor = openEditor(0);
                const auto beforeEscapedRank = fixture.view.recoveryState();
                if ( canceledEditor )
                {
                    canceledEditor->setText(QStringLiteral("3"));
                    key(*canceledEditor, QEvent::KeyPress, Qt::Key_Escape);
                    key(*canceledEditor, QEvent::KeyRelease, Qt::Key_Escape);
                    QApplication::processEvents(QEventLoop::AllEvents);
                }
                const auto afterEscapedRank = fixture.view.recoveryState();
                check(canceledEditor != nullptr
                          && afterEscapedRank.applyOrder
                              == std::vector<std::int32_t>{0, 1, 2}
                          && fixture.view.cleaveStagedMode()
                          && historyMatches(beforeEscapedRank.cleaveHistory,
                                            afterEscapedRank.cleaveHistory),
                      "Escape cancels line-rank editing without changing the order, history, or Multi-cut mode");

                QLineEdit* const lowerClampEditor = openEditor(1);
                if ( lowerClampEditor )
                {
                    lowerClampEditor->setText(QStringLiteral("-27"));
                    pressEnter(*lowerClampEditor);
                }
                const auto afterLowerClamp = fixture.view.recoveryState();
                check(lowerClampEditor != nullptr
                          && afterLowerClamp.applyOrder
                              == std::vector<std::int32_t>{1, 0, 2},
                      "a negative numeric rank clamps to position one and moves the selected identity there");
                const bool lowerClampUndone = fixture.view.undoStagedCleaveHistory();
                check(lowerClampUndone
                          && fixture.view.recoveryState().applyOrder
                              == std::vector<std::int32_t>{0, 1, 2},
                      "one local Undo restores the order before lower-rank clamping");

                QLineEdit* const lowerBoundEditor = openEditor(1);
                if ( lowerBoundEditor )
                {
                    lowerBoundEditor->setText(QStringLiteral("0"));
                    pressEnter(*lowerBoundEditor);
                }
                check(lowerBoundEditor != nullptr
                          && fixture.view.recoveryState().applyOrder
                              == std::vector<std::int32_t>{1, 0, 2},
                      "rank zero clamps to one and inserts the selected physical line at the first position");
                check(fixture.view.undoStagedCleaveHistory()
                          && fixture.view.recoveryState().applyOrder
                              == std::vector<std::int32_t>{0, 1, 2},
                      "one local Undo restores the order before zero-rank clamping");

                QLineEdit* const ordinaryEditor = openEditor(0);
                const auto beforeOrdinaryReorder = fixture.view.recoveryState();
                if ( ordinaryEditor )
                {
                    ordinaryEditor->setText(QStringLiteral("2"));
                    pressEnter(*ordinaryEditor);
                }
                const GlView::RecoveryState afterOrdinaryReorder = fixture.view.recoveryState();
                const bool oneOrderEdit = beforeOrdinaryReorder.cleaveHistory
                    && afterOrdinaryReorder.cleaveHistory
                    && afterOrdinaryReorder.cleaveHistory->cursor
                        == beforeOrdinaryReorder.cleaveHistory->cursor + 1;
                check(ordinaryEditor != nullptr && oneOrderEdit
                          && afterOrdinaryReorder.applyOrder
                              == std::vector<std::int32_t>{1, 0, 2},
                      "Enter commits an in-range rank as exactly one stable-identity order edit");

                QLineEdit* const overflowEditor = openEditor(0);
                if ( overflowEditor )
                {
                    overflowEditor->setText(QStringLiteral("999999999999999999999999999999999999999"));
                    fixture.view.setFocus(Qt::OtherFocusReason);
                    QApplication::processEvents(QEventLoop::AllEvents);
                }
                const GlView::RecoveryState afterOverflow = fixture.view.recoveryState();
                check(overflowEditor != nullptr
                          && afterOverflow.applyOrder
                              == std::vector<std::int32_t>{1, 2, 0},
                      "focus-out commits an oversized decimal rank clamped to the final position");
                check(sameLines(afterOverflow.cleaveLines,
                                beforeLabelEdit.cleaveLines)
                          && afterOverflow.selectedCleaveLines
                              == beforeLabelEdit.selectedCleaveLines
                          && afterOverflow.selectedCleaveLine
                              == beforeLabelEdit.selectedCleaveLine,
                      "rank editing preserves physical line geometry, topology, and selection identity");
                check(fixture.view.undoStagedCleaveHistory()
                          && fixture.view.recoveryState().applyOrder
                              == std::vector<std::int32_t>{1, 0, 2}
                          && fixture.view.undoStagedCleaveHistory()
                          && fixture.view.recoveryState().applyOrder
                              == std::vector<std::int32_t>{0, 1, 2},
                      "two local Undo steps restore the rank edit and the prior in-range permutation");
            }
        }
    }

    {
        CleaveViewportFixture fixture{};
        fixture.addLine(QVector3D(-0.8f, 0.0f, 0.0f), QVector3D(0.8f, 0.0f, 0.0f));
        GlView::RecoveryState::CleaveDraftSnapshot baseline =
            fixture.view.captureStagedCleaveDraftState();
        GlView::RecoveryState::CleaveHistory history{};
        history.snapshots.reserve(64);
        for ( std::int32_t index = 0; index < 64; ++index )
        {
            auto snapshot = baseline;
            const double offset = static_cast<double>(index) * 0.002;
            snapshot.lines[0].first.z += offset;
            snapshot.lines[0].second.z += offset;
            history.snapshots.push_back(std::move(snapshot));
        }
        history.cursor = 0;
        int restoredTimelineNotifications = 0;
        bool observerSawInstalledRedoBranch = false;
        QObject::connect(
            &fixture.view,
            &GlView::viewportStateChanged,
            &fixture.view,
            [&]()
            {
                ++restoredTimelineNotifications;
                const GlView::RecoveryState observed = fixture.view.recoveryState();
                observerSawInstalledRedoBranch = observerSawInstalledRedoBranch
                    || (historyMatches(observed.cleaveHistory, history)
                        && fixture.view.canRedoStagedCleaveHistory());
            });
        fixture.view.restoreStagedCleaveDraftState(history.snapshots.front(), history);
        check(restoredTimelineNotifications == 1 && observerSawInstalledRedoBranch,
              "published recovery observers see the requested staged history and Redo branch atomically");
        const auto beforeNoOp = fixture.view.recoveryState();
        const bool bodyClicked = fixture.clickBody(0, Qt::NoModifier);
        const auto afterNoOp = fixture.view.recoveryState();
        check(fixture.sourceGeometryProjectsInside()
                  && fixture.allBodyPicksClearOfEndpoints
                  && bodyClicked
                  && historyMatches(beforeNoOp.cleaveHistory, afterNoOp.cleaveHistory)
                  && afterNoOp.cleaveHistory
                  && afterNoOp.cleaveHistory->cursor == 0
                  && fixture.view.canRedoStagedCleaveHistory(),
              "a no-op gesture at the 64-state history boundary preserves its full Redo branch and cursor");

        history.cursor = 63;
        fixture.view.restoreStagedCleaveDraftState(history.snapshots.back(), history);
        const auto beforeLastCursorNoOp = fixture.view.recoveryState();
        const bool lastCursorBodyClicked = fixture.clickBody(0, Qt::NoModifier);
        const auto afterLastCursorNoOp = fixture.view.recoveryState();
        check(lastCursorBodyClicked
                  && historyMatches(beforeLastCursorNoOp.cleaveHistory,
                                    afterLastCursorNoOp.cleaveHistory)
                  && afterLastCursorNoOp.cleaveHistory
                  && afterLastCursorNoOp.cleaveHistory->cursor == 63
                  && afterLastCursorNoOp.cleaveHistory->snapshots.size() == 64,
              "a no-op gesture at the 64-state history boundary preserves its latest Undo state");

        const auto beforeCanceledDrag = fixture.view.recoveryState();
        const QVector3D midpoint =
            (lineFirst(beforeCanceledDrag, 0) + lineSecond(beforeCanceledDrag, 0)) * 0.5f;
        const auto dragStart = project(fixture.view, midpoint);
        if ( dragStart )
        {
            const QPointF dragEnd = *dragStart + QPointF(28.0, 16.0);
            sendMouse(
                fixture.view,
                QEvent::MouseButtonPress,
                *dragStart,
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::NoModifier);
            sendMouse(
                fixture.view,
                QEvent::MouseMove,
                dragEnd,
                Qt::NoButton,
                Qt::LeftButton,
                Qt::NoModifier);
        }
        const bool dragPreviewActive = fixture.view.stagedCleavePlaneDraggingForTest();
        key(fixture.view, QEvent::KeyPress, Qt::Key_Escape);
        key(fixture.view, QEvent::KeyRelease, Qt::Key_Escape);
        QApplication::processEvents(QEventLoop::AllEvents);
        const bool canceledDrag = !fixture.view.stagedCleavePlaneDraggingForTest();
        if ( dragStart )
        {
            sendMouse(
                fixture.view,
                QEvent::MouseButtonRelease,
                *dragStart + QPointF(28.0, 16.0),
                Qt::LeftButton,
                Qt::NoButton,
                Qt::NoModifier);
        }
        const auto afterCanceledDrag = fixture.view.recoveryState();
        check(dragStart && dragPreviewActive && canceledDrag
                  && sameLines(beforeCanceledDrag.cleaveLines,
                               afterCanceledDrag.cleaveLines)
                  && historyMatches(beforeCanceledDrag.cleaveHistory,
                                    afterCanceledDrag.cleaveHistory)
                  && beforeCanceledDrag.selectedCleaveLines
                      == afterCanceledDrag.selectedCleaveLines,
              "canceling a history-boundary drag restores geometry, selection, and the exact timeline");
    }

    {
        const auto runQuantizedRotationCase = [](const double angleStep,
                                                 const double requestedAngle,
                                                 const double expectedAngle,
                                                 const char* const label)
        {
            CleaveViewportFixture fixture{};
            fixture.view.setCameraOrthoScale(64.0f);
            fixture.view.captureStagedCleaveContextForTest(true);
            const QVector3D first(-1.4f, 0.0f, -0.3f);
            const QVector3D joint(0.2f, 0.0f, -0.3f);
            const QVector3D second(1.2f, 0.0f, -0.3f);
            const QVector3D pivot(0.0f, 0.0f, -0.3f);
            fixture.addLine(first, joint, false, true);
            fixture.addLine(joint, second, true, false);
            const bool firstPicked = fixture.clickBody(0, Qt::NoModifier);
            const bool secondAdded = fixture.clickBody(1, Qt::ShiftModifier);
            const bool historySeeded = seedSettledCleaveHistoryBaseline(fixture.view);
            const GlView::RecoveryState before = fixture.view.recoveryState();
            const std::uint64_t revisionBefore =
                fixture.view.stagedCleaveAuthoredRevision();
            const bool contextReady =
                fixture.view.stagedCleaveContextMatchesCurrentForTest(true);
            const bool transformEligible = fixture.view.canTransformSelectedStagedCleave(
                GlView::StagedCleaveTransformMode::Rotate);
            fixture.view.setCleaveAngleStepDegrees(angleStep);
            fixture.view.setStagedCleaveTransformMode(
                GlView::StagedCleaveTransformMode::Rotate);

            const QVector3D startWorld(0.7f, 0.0f, -0.3f);
            const QVector3D axis = toVector(
                before.cleaveLines.front().constructionPlaneNormal).normalized();
            const QVector3D startRadial = startWorld - pivot;
            const QVector3D destinationWorld = pivot
                + rotateVectorForTest(startRadial, axis, requestedAngle);
            const auto start = project(fixture.view, startWorld);
            const auto destination = project(fixture.view, destinationWorld);
            double startEndpointClearance = std::numeric_limits<double>::infinity();
            if ( start )
            {
                for ( const auto& line : before.cleaveLines )
                {
                    for ( const TVector* endpoint : {&line.first, &line.second} )
                    {
                        const auto endpointScreen = project(
                            fixture.view,
                            toVector(*endpoint));
                        if ( endpointScreen )
                        {
                            startEndpointClearance = std::min(
                                startEndpointClearance,
                                std::hypot(
                                    start->x() - endpointScreen->x(),
                                    start->y() - endpointScreen->y()));
                        }
                    }
                }
            }
            if ( start && destination )
            {
                sendMouse(
                    fixture.view,
                    QEvent::MouseButtonPress,
                    *start,
                    Qt::LeftButton,
                    Qt::LeftButton,
                    Qt::NoModifier);
                sendMouse(
                    fixture.view,
                    QEvent::MouseMove,
                    *destination,
                    Qt::NoButton,
                    Qt::LeftButton,
                    Qt::NoModifier);
            }

            const QString expectedHud = QStringLiteral("Rotate %1°")
                .arg(expectedAngle, 0, 'f', 2);
            const QString previewHud = fixture.view.stagedCleaveTransformHudTextForTest();
            const QVector3D previewFirst = fixture.view.stagedCleaveLineFirstForTest(0);
            const QVector3D previewJoint = fixture.view.stagedCleaveLineSecondForTest(0);
            const QVector3D previewSecond = fixture.view.stagedCleaveLineSecondForTest(1);
            const QVector3D expectedFirst = pivot
                + rotateVectorForTest(first - pivot, axis, expectedAngle);
            const QVector3D expectedJoint = pivot
                + rotateVectorForTest(joint - pivot, axis, expectedAngle);
            const QVector3D expectedSecond = pivot
                + rotateVectorForTest(second - pivot, axis, expectedAngle);
            const bool previewMatchesQuantizedStep =
                closeQVec(fixture.view.stagedCleaveLineFirstForTest(0),
                          expectedFirst,
                          0.035f)
                && closeQVec(fixture.view.stagedCleaveLineSecondForTest(0),
                             expectedJoint,
                             0.035f)
                && closeQVec(fixture.view.stagedCleaveLineFirstForTest(1),
                             expectedJoint,
                             0.035f)
                && closeQVec(fixture.view.stagedCleaveLineSecondForTest(1),
                             expectedSecond,
                             0.035f);
            if ( start && destination )
            {
                sendMouse(
                    fixture.view,
                    QEvent::MouseButtonRelease,
                    *destination,
                    Qt::LeftButton,
                    Qt::NoButton,
                    Qt::NoModifier);
            }
            const GlView::RecoveryState after = fixture.view.recoveryState();
            const bool committedMatchesQuantizedStep =
                closeQVec(lineFirst(after, 0), expectedFirst, 0.035f)
                && closeQVec(lineSecond(after, 0), expectedJoint, 0.035f)
                && closeQVec(lineFirst(after, 1), expectedJoint, 0.035f)
                && closeQVec(lineSecond(after, 1), expectedSecond, 0.035f);
            const QVector3D initialDirection = lineSecond(before, 0) - lineFirst(before, 0);
            const QVector3D actualDirection = lineSecond(after, 0) - lineFirst(after, 0);
            const double actualAngle = std::atan2(
                QVector3D::dotProduct(
                    axis,
                    QVector3D::crossProduct(initialDirection, actualDirection)),
                QVector3D::dotProduct(initialDirection, actualDirection))
                * (180.0 / 3.14159265358979323846);
            const bool passed = historySeeded
                && firstPicked && secondAdded && contextReady && transformEligible
                      && start && destination && startEndpointClearance > 10.0
                      && fixture.allBodyPicksClearOfEndpoints
                      && previewHud == expectedHud
                      && previewMatchesQuantizedStep
                      && committedMatchesQuantizedStep;
            if ( !passed )
            {
                const double startX = start ? start->x() : -1.0;
                const double startY = start ? start->y() : -1.0;
                const double destinationX = destination ? destination->x() : -1.0;
                const double destinationY = destination ? destination->y() : -1.0;
                const QByteArray expectedHudBytes = expectedHud.toUtf8();
                const QByteArray previewHudBytes = previewHud.toUtf8();
                std::fprintf(
                    stderr,
                    "Cleave rotation preconditions: label=%s step=%.3f requested=%.3f "
                    "expected=%.3f picked=(%d,%d) historySeeded=%d context=%d eligible=%d "
                    "start=(%.3f,%.3f) destination=(%.3f,%.3f) endpointClearance=%.3f "
                    "bodyPickClear=%d HUD=\"%s\" expectedHUD=\"%s\" "
                    "previewMatch=%d commitMatch=%d actualAngle=%.3f "
                    "previewDelta=(%.6f,%.6f,%.6f) committedDelta=(%.6f,%.6f,%.6f)\n",
                    label,
                    angleStep,
                    requestedAngle,
                    expectedAngle,
                    firstPicked ? 1 : 0,
                    secondAdded ? 1 : 0,
                    historySeeded ? 1 : 0,
                    contextReady ? 1 : 0,
                    transformEligible ? 1 : 0,
                    startX,
                    startY,
                    destinationX,
                    destinationY,
                    startEndpointClearance,
                    fixture.allBodyPicksClearOfEndpoints ? 1 : 0,
                    previewHudBytes.constData(),
                    expectedHudBytes.constData(),
                    previewMatchesQuantizedStep ? 1 : 0,
                    committedMatchesQuantizedStep ? 1 : 0,
                    actualAngle,
                    previewFirst.x() - lineFirst(before, 0).x(),
                    previewFirst.y() - lineFirst(before, 0).y(),
                    previewFirst.z() - lineFirst(before, 0).z(),
                    lineFirst(after, 0).x() - lineFirst(before, 0).x(),
                    lineFirst(after, 0).y() - lineFirst(before, 0).y(),
                    lineFirst(after, 0).z() - lineFirst(before, 0).z());
                std::fprintf(
                    stderr,
                    "  preview joint=(%.6f,%.6f,%.6f) second=(%.6f,%.6f,%.6f) "
                    "expected first=(%.6f,%.6f,%.6f) joint=(%.6f,%.6f,%.6f) "
                    "second=(%.6f,%.6f,%.6f)\n",
                    previewJoint.x(), previewJoint.y(), previewJoint.z(),
                    previewSecond.x(), previewSecond.y(), previewSecond.z(),
                    expectedFirst.x(), expectedFirst.y(), expectedFirst.z(),
                    expectedJoint.x(), expectedJoint.y(), expectedJoint.z(),
                    expectedSecond.x(), expectedSecond.y(), expectedSecond.z());
                printCleaveFailureState(
                    label,
                    before,
                    after,
                    fixture.view,
                    revisionBefore);
            }
            check(passed, label);
        };

        runQuantizedRotationCase(
            100.0,
            160.0,
            200.0,
            "a 100-degree Rotate step keeps the rounded 200-degree preview and HUD instead of clamping it to 180");
        runQuantizedRotationCase(
            270.0,
            150.0,
            270.0,
            "a 270-degree Rotate step keeps the rounded 270-degree preview and HUD instead of clamping it to 180");
    }

    {
        CleaveViewportFixture fixture{};
        fixture.view.setCameraOrthoScale(32.0f);
        fixture.view.captureStagedCleaveContextForTest(true);
        const QVector3D first(-1.7f, 0.0f, 0.3f);
        const QVector3D second(1.7f, 0.0f, 0.3f);
        fixture.addLine(first, second);
        const bool historySeeded = seedSettledCleaveHistoryBaseline(fixture.view);
        const GlView::RecoveryState before = fixture.view.recoveryState();
        const std::uint64_t revisionBefore =
            fixture.view.stagedCleaveAuthoredRevision();
        const QVector3D cameraProbe(0.0f, 0.0f, 0.8f);
        const auto authoredCameraProjection = project(fixture.view, cameraProbe);
        fixture.view.setOrbitViewImmediate(22.0f, 12.0f, 0.0f);
        QApplication::processEvents(QEventLoop::AllEvents);

        const auto start = project(fixture.view, (first + second) * 0.5f);
        QVector3D previewFirst{};
        QVector3D previewSecond{};
        double nearestEndpointPixels = std::numeric_limits<double>::infinity();
        if ( start )
        {
            for ( const QVector3D endpoint : {first, second} )
            {
                const auto endpointScreen = project(fixture.view, endpoint);
                if ( endpointScreen )
                {
                    nearestEndpointPixels = std::min(
                        nearestEndpointPixels,
                        std::hypot(start->x() - endpointScreen->x(),
                                   start->y() - endpointScreen->y()));
                }
            }
        }
        const bool contextReady = fixture.view.stagedCleaveContextMatchesCurrentForTest(true);
        bool dragStarted = false;
        bool previewChanged = false;
        if ( start )
        {
            const QPointF jitter = *start + QPointF(1.0, 0.0);
            sendMouse(
                fixture.view,
                QEvent::MouseButtonPress,
                *start,
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::NoModifier);
            dragStarted = fixture.view.stagedCleavePlaneDraggingForTest();
            sendMouse(
                fixture.view,
                QEvent::MouseMove,
                jitter,
                Qt::NoButton,
                Qt::LeftButton,
                Qt::NoModifier);
            previewFirst = fixture.view.stagedCleaveLineFirstForTest(0);
            previewSecond = fixture.view.stagedCleaveLineSecondForTest(0);
            previewChanged = !closeQVec(
                previewFirst,
                lineFirst(before, 0),
                1.0e-6f)
                || !closeQVec(
                    previewSecond,
                    lineSecond(before, 0),
                    1.0e-6f);
            sendMouse(
                fixture.view,
                QEvent::MouseButtonRelease,
                jitter,
                Qt::LeftButton,
                Qt::NoButton,
                Qt::NoModifier);
        }
        const GlView::RecoveryState after = fixture.view.recoveryState();
        const auto recalledCameraProjection = project(fixture.view, cameraProbe);
        const bool cameraRecalled = authoredCameraProjection && recalledCameraProjection
            && std::hypot(
                   authoredCameraProjection->x() - recalledCameraProjection->x(),
                   authoredCameraProjection->y() - recalledCameraProjection->y())
                < 1.0;
        const bool passed = historySeeded
                  && contextReady && start && nearestEndpointPixels > 10.0
                  && dragStarted && previewChanged
                  && sameCleaveDraftState(before, after)
                  && fixture.view.stagedCleaveAuthoredRevision() == revisionBefore
                  && cameraRecalled;
        if ( !passed )
        {
            std::fprintf(
                stderr,
                "Cleave subthreshold Move preconditions: historySeeded=%d context=%d "
                "start=%d endpointClearance=%.3f dragStarted=%d previewChanged=%d "
                "cameraRecalled=%d cameraPixelDelta=%.3f probe=%d revisionStable=%d "
                "draftStable=%d previewFirst=(%.6f,%.6f,%.6f) "
                "previewSecond=(%.6f,%.6f,%.6f)\n",
                historySeeded ? 1 : 0,
                contextReady ? 1 : 0,
                start.has_value() ? 1 : 0,
                nearestEndpointPixels,
                dragStarted ? 1 : 0,
                previewChanged ? 1 : 0,
                cameraRecalled ? 1 : 0,
                authoredCameraProjection && recalledCameraProjection
                    ? std::hypot(
                          authoredCameraProjection->x() - recalledCameraProjection->x(),
                          authoredCameraProjection->y() - recalledCameraProjection->y())
                    : -1.0,
                authoredCameraProjection && recalledCameraProjection ? 1 : 0,
                fixture.view.stagedCleaveAuthoredRevision() == revisionBefore ? 1 : 0,
                sameCleaveDraftState(before, after) ? 1 : 0,
                previewFirst.x(), previewFirst.y(), previewFirst.z(),
                previewSecond.x(), previewSecond.y(), previewSecond.z());
            printCleaveFailureState(
                "one-pixel Move jitter",
                before,
                after,
                fixture.view,
                revisionBefore);
        }
        check(passed,
              "a one-pixel Move jitter restores the exact draft, selection, context, timeline, and revision while retaining authored-camera recall");
    }

    {
        CleaveViewportFixture fixture{};
        fixture.view.setCameraOrthoScale(32.0f);
        fixture.view.captureStagedCleaveContextForTest(true);
        fixture.addLine(
            QVector3D(-1.7f, 0.0f, -0.3f),
            QVector3D(1.7f, 0.0f, -0.3f));
        const bool historySeeded = seedSettledCleaveHistoryBaseline(fixture.view);
        const GlView::RecoveryState before = fixture.view.recoveryState();
        const std::uint64_t revisionBefore =
            fixture.view.stagedCleaveAuthoredRevision();
        const auto start = fixture.body(0);
        bool dragStarted = false;
        bool crossedThreshold = false;
        bool returnedPreviewChanged = false;
        QVector3D crossedPreviewFirst{};
        QVector3D crossedPreviewSecond{};
        QVector3D returnedPreviewFirst{};
        QVector3D returnedPreviewSecond{};
        if ( start )
        {
            const QPointF outOfThreshold = *start + QPointF(8.0, 0.0);
            const QPointF backUnderThreshold = *start + QPointF(1.0, 0.0);
            sendMouse(
                fixture.view,
                QEvent::MouseButtonPress,
                *start,
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::NoModifier);
            dragStarted = fixture.view.stagedCleavePlaneDraggingForTest();
            sendMouse(
                fixture.view,
                QEvent::MouseMove,
                outOfThreshold,
                Qt::NoButton,
                Qt::LeftButton,
                Qt::NoModifier);
            crossedPreviewFirst = fixture.view.stagedCleaveLineFirstForTest(0);
            crossedPreviewSecond = fixture.view.stagedCleaveLineSecondForTest(0);
            crossedThreshold = !closeQVec(
                crossedPreviewFirst,
                lineFirst(before, 0),
                1.0e-5f);
            sendMouse(
                fixture.view,
                QEvent::MouseMove,
                backUnderThreshold,
                Qt::NoButton,
                Qt::LeftButton,
                Qt::NoModifier);
            returnedPreviewFirst = fixture.view.stagedCleaveLineFirstForTest(0);
            returnedPreviewSecond = fixture.view.stagedCleaveLineSecondForTest(0);
            returnedPreviewChanged = !closeQVec(
                returnedPreviewFirst,
                lineFirst(before, 0),
                1.0e-6f);
            sendMouse(
                fixture.view,
                QEvent::MouseButtonRelease,
                backUnderThreshold,
                Qt::LeftButton,
                Qt::NoButton,
                Qt::NoModifier);
        }
        const GlView::RecoveryState after = fixture.view.recoveryState();
        const bool passed = historySeeded
                  && start && dragStarted && crossedThreshold && returnedPreviewChanged
                  && sameCleaveDraftState(before, after)
                  && fixture.view.stagedCleaveAuthoredRevision() == revisionBefore;
        if ( !passed )
        {
            std::fprintf(
                stderr,
                "Cleave threshold-return Move preconditions: historySeeded=%d start=%d "
                "dragStarted=%d crossedThreshold=%d returnedPreviewChanged=%d "
                "revisionStable=%d draftStable=%d "
                "crossedFirst=(%.6f,%.6f,%.6f) crossedSecond=(%.6f,%.6f,%.6f) "
                "returnedFirst=(%.6f,%.6f,%.6f) returnedSecond=(%.6f,%.6f,%.6f)\n",
                historySeeded ? 1 : 0,
                start.has_value() ? 1 : 0,
                dragStarted ? 1 : 0,
                crossedThreshold ? 1 : 0,
                returnedPreviewChanged ? 1 : 0,
                fixture.view.stagedCleaveAuthoredRevision() == revisionBefore ? 1 : 0,
                sameCleaveDraftState(before, after) ? 1 : 0,
                crossedPreviewFirst.x(), crossedPreviewFirst.y(), crossedPreviewFirst.z(),
                crossedPreviewSecond.x(), crossedPreviewSecond.y(), crossedPreviewSecond.z(),
                returnedPreviewFirst.x(), returnedPreviewFirst.y(), returnedPreviewFirst.z(),
                returnedPreviewSecond.x(), returnedPreviewSecond.y(), returnedPreviewSecond.z());
            printCleaveFailureState(
                "Move threshold-cross-and-return",
                before,
                after,
                fixture.view,
                revisionBefore);
        }
        check(passed,
              "a Move preview that crosses the drag threshold and returns under it restores the exact press-time draft");
    }

    {
        CleaveViewportFixture fixture{};
        fixture.view.setCleaveLineNumbersVisible(false);
        fixture.view.captureStagedCleaveContextForTest(true);
        fixture.view.setTransformGridSnapEnabled(false);
        const QVector3D base(0.0f, 0.0f, 0.2f);
        const auto origin = project(fixture.view, base);
        const auto projectedX = project(fixture.view, base + QVector3D(1.0f, 0.0f, 0.0f));
        const auto projectedZ = project(fixture.view, base + QVector3D(0.0f, 0.0f, 1.0f));
        const QPointF axisX = origin && projectedX ? *projectedX - *origin : QPointF{};
        const QPointF axisZ = origin && projectedZ ? *projectedZ - *origin : QPointF{};
        const double determinant = axisX.x() * axisZ.y() - axisX.y() * axisZ.x();
        const bool projectionReady = origin && projectedX && projectedZ
            && std::abs(determinant) > 1.0e-8;
        const auto worldAtScreenOffset = [&](const double screenX, const double screenY)
        {
            if (!projectionReady)
            {
                return base + QVector3D(
                    static_cast<float>(screenX * 0.01),
                    0.0f,
                    static_cast<float>(screenY * 0.01));
            }
            const double worldX =
                (screenX * axisZ.y() - screenY * axisZ.x()) / determinant;
            const double worldZ =
                (axisX.x() * screenY - axisX.y() * screenX) / determinant;
            return base + QVector3D(
                static_cast<float>(worldX),
                0.0f,
                static_cast<float>(worldZ));
        };
        const QVector3D first = worldAtScreenOffset(-40.0, 0.0);
        const QVector3D second = worldAtScreenOffset(40.0, 0.0);
        const QVector3D movingGroupHandle = worldAtScreenOffset(-35.0, 4.0);
        const QVector3D movingGroupEnd = worldAtScreenOffset(-35.0, 60.0);
        fixture.addLine(first, second);
        fixture.addLine(
            movingGroupHandle,
            movingGroupEnd);
        const QVector3D externalSnapTarget = worldAtScreenOffset(-39.0, 11.0);
        const QVector3D externalSnapEnd = worldAtScreenOffset(-90.0, 11.0);
        fixture.addLine(
            externalSnapTarget,
            externalSnapEnd);

        const bool firstPicked = fixture.clickBody(0, Qt::NoModifier);
        const bool secondAdded = fixture.clickBody(1, Qt::ShiftModifier);
        const bool firstReactivated = fixture.clickBody(0, Qt::ShiftModifier);
        const bool historySeeded = seedSettledCleaveHistoryBaseline(fixture.view);
        const bool lineNumbersHidden = !fixture.view.cleaveLineNumbersVisible();
        const GlView::RecoveryState before = fixture.view.recoveryState();
        const bool contextReady =
            fixture.view.stagedCleaveContextMatchesCurrentForTest(true);
        const std::uint64_t revisionBefore =
            fixture.view.stagedCleaveAuthoredRevision();
        const auto start = fixture.body(0);
        bool dragStarted = false;
        bool externalTargetPreview = false;
        bool movingGroupPreview = false;
        std::optional<render::CleaveSnapTargetKind> snapMarker{};
        QPointF jitter{};
        double startEndpointClearance = std::numeric_limits<double>::infinity();
        double externalTargetDistancePixels = std::numeric_limits<double>::infinity();
        double movingSelfTargetDistancePixels = std::numeric_limits<double>::infinity();
        bool snapCandidatesHaveExpectedClearance = false;
        QVector3D previewFirst0{};
        QVector3D previewSecond0{};
        QVector3D previewFirst1{};
        QVector3D previewSecond1{};
        if ( start )
        {
            for ( const auto& line : before.cleaveLines )
            {
                for ( const TVector* endpoint : {&line.first, &line.second} )
                {
                    const auto endpointScreen = project(fixture.view, toVector(*endpoint));
                    if ( endpointScreen )
                    {
                        startEndpointClearance = std::min(
                            startEndpointClearance,
                            std::hypot(
                                start->x() - endpointScreen->x(),
                                start->y() - endpointScreen->y()));
                    }
                }
            }
            jitter = *start + QPointF(1.0, 0.0);
            const auto firstEndpoint = project(fixture.view, first);
            const auto movingSelfEndpoint = project(fixture.view, movingGroupHandle);
            const auto externalEndpoint = project(fixture.view, externalSnapTarget);
            if ( firstEndpoint && movingSelfEndpoint && externalEndpoint )
            {
                const QPointF movedFirst = *firstEndpoint + QPointF(1.0, 0.0);
                externalTargetDistancePixels = std::hypot(
                    movedFirst.x() - externalEndpoint->x(),
                    movedFirst.y() - externalEndpoint->y());
                movingSelfTargetDistancePixels = std::hypot(
                    movedFirst.x() - movingSelfEndpoint->x(),
                    movedFirst.y() - movingSelfEndpoint->y());
                snapCandidatesHaveExpectedClearance =
                    externalTargetDistancePixels > 10.0
                    && externalTargetDistancePixels <= 14.0
                    && movingSelfTargetDistancePixels > 0.0
                    && movingSelfTargetDistancePixels < externalTargetDistancePixels
                    && movingSelfTargetDistancePixels <= 14.0;
            }
            sendMouse(
                fixture.view,
                QEvent::MouseButtonPress,
                *start,
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::ShiftModifier);
            dragStarted = fixture.view.stagedCleavePlaneDraggingForTest();
            sendMouse(
                fixture.view,
                QEvent::MouseMove,
                jitter,
                Qt::NoButton,
                Qt::LeftButton,
                Qt::ShiftModifier);
            snapMarker = fixture.view.cleaveSnapMarkerKindForTest();
            previewFirst0 = fixture.view.stagedCleaveLineFirstForTest(0);
            previewSecond0 = fixture.view.stagedCleaveLineSecondForTest(0);
            previewFirst1 = fixture.view.stagedCleaveLineFirstForTest(1);
            previewSecond1 = fixture.view.stagedCleaveLineSecondForTest(1);
            externalTargetPreview = closeQVec(
                previewFirst0,
                externalSnapTarget,
                0.005f);
            const QVector3D groupDelta =
                previewFirst0 - lineFirst(before, 0);
            const QVector3D expectedSnapDelta = externalSnapTarget - first;
            movingGroupPreview = expectedSnapDelta.length() > 1.0e-6f
                && closeQVec(groupDelta, expectedSnapDelta, 0.005f)
                && closeQVec(
                    previewSecond0 - lineSecond(before, 0),
                    groupDelta,
                    1.0e-5f)
                && closeQVec(
                    previewFirst1 - lineFirst(before, 1),
                    groupDelta,
                    1.0e-5f)
                && closeQVec(
                    previewSecond1 - lineSecond(before, 1),
                    groupDelta,
                    1.0e-5f)
                && closeQVec(
                    fixture.view.stagedCleaveLineFirstForTest(2),
                    lineFirst(before, 2),
                    1.0e-5f)
                && closeQVec(
                    fixture.view.stagedCleaveLineSecondForTest(2),
                    lineSecond(before, 2),
                    1.0e-5f);
            sendMouse(
                fixture.view,
                QEvent::MouseButtonRelease,
                jitter,
                Qt::LeftButton,
                Qt::NoButton,
                Qt::ShiftModifier);
        }
        const GlView::RecoveryState after = fixture.view.recoveryState();
        const bool passed = historySeeded
                  && projectionReady && lineNumbersHidden
                  && firstPicked && secondAdded && firstReactivated
                  && contextReady && start
                  && startEndpointClearance > 10.0
                  && snapCandidatesHaveExpectedClearance
                  && fixture.allBodyPicksClearOfEndpoints
                  && before.selectedCleaveLines == std::vector<std::int32_t>{0, 1}
                  && before.selectedCleaveLine == 0
                  && !before.cleaveChainOrigin
                  && dragStarted && externalTargetPreview && movingGroupPreview
                  && snapMarker == render::CleaveSnapTargetKind::StagedEndpoint
                  && sameCleaveDraftState(before, after)
                  && fixture.view.stagedCleaveAuthoredRevision() == revisionBefore;
        if ( !passed )
        {
            std::fprintf(
                stderr,
                "Cleave Shift-group snap preconditions: historySeeded=%d projection=%d numbersHidden=%d "
                "picked=(%d,%d,%d) context=%d start=%d clearance=%.3f bodyPicksClear=%d "
                "snapClearance=%d externalPx=%.3f selfPx=%.3f "
                "selectionCount=%zu active=%d dragStarted=%d externalPreview=%d "
                "groupPreview=%d snapMarker=%d revisionStable=%d draftStable=%d "
                "start=(%.3f,%.3f) jitter=(%.3f,%.3f) target=(%.4f,%.4f,%.4f) "
                "preview0=(%.4f,%.4f,%.4f)->(%.4f,%.4f,%.4f) "
                "preview1=(%.4f,%.4f,%.4f)->(%.4f,%.4f,%.4f)\n",
                historySeeded ? 1 : 0,
                projectionReady ? 1 : 0,
                lineNumbersHidden ? 1 : 0,
                firstPicked ? 1 : 0,
                secondAdded ? 1 : 0,
                firstReactivated ? 1 : 0,
                contextReady ? 1 : 0,
                start.has_value() ? 1 : 0,
                startEndpointClearance,
                fixture.allBodyPicksClearOfEndpoints ? 1 : 0,
                snapCandidatesHaveExpectedClearance ? 1 : 0,
                externalTargetDistancePixels,
                movingSelfTargetDistancePixels,
                before.selectedCleaveLines.size(),
                before.selectedCleaveLine,
                dragStarted ? 1 : 0,
                externalTargetPreview ? 1 : 0,
                movingGroupPreview ? 1 : 0,
                snapMarker ? static_cast<int>(*snapMarker) : -1,
                fixture.view.stagedCleaveAuthoredRevision() == revisionBefore ? 1 : 0,
                sameCleaveDraftState(before, after) ? 1 : 0,
                start ? start->x() : -1.0,
                start ? start->y() : -1.0,
                jitter.x(),
                jitter.y(),
                externalSnapTarget.x(),
                externalSnapTarget.y(),
                externalSnapTarget.z(),
                previewFirst0.x(), previewFirst0.y(), previewFirst0.z(),
                previewSecond0.x(), previewSecond0.y(), previewSecond0.z(),
                previewFirst1.x(), previewFirst1.y(), previewFirst1.z(),
                previewSecond1.x(), previewSecond1.y(), previewSecond1.z());
            printCleaveFailureState(
                "Shift group Move external target snap",
                before,
                after,
                fixture.view,
                revisionBefore);
        }
        check(passed,
              "Shift group Move ignores its own moving endpoints, can preview an external endpoint snap, and rolls a one-pixel snap back exactly");
    }

    runInitialAnchorGridSnapCases();
    runSectorBoundedFiniteStrokeCase();
    runMovePressHistoryBudgetCase();
}

} // namespace sed::app_tests
