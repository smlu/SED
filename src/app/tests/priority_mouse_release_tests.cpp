#include "test_common.hpp"

#include "app_settings.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QWindow>

#include <array>

namespace sed::app_tests
{

void runPriorityMouseReleaseTests()
{
    const bool offscreen = QGuiApplication::platformName() == QStringLiteral("offscreen");
    check(offscreen, "preliminary viewport-release regression is restricted to Qt offscreen");
    if ( !offscreen )
    {
        return;
    }

    MainWindow window(nullptr, true);
    GlView* const view = window.glViewForTest();
    check(view != nullptr, "preliminary viewport-release regression has a live GlView");
    if ( !view )
    {
        return;
    }

    // Keep the fixture hidden and force the ordinary physical LMB mapping.
    // Mouse events are sent to the widget and MainWindow filter seam directly;
    // no native input or OpenGL context is involved.
    view->setViewportNavigationSettings(ViewportNavigationSettings{});
    view->setViewportPickingEnabled(true);
    int clickPickedCount = 0;
    std::array<std::int32_t, 4> pickedEntity{ -2, -2, -2, -2 };
    QObject::connect(
        view,
        &GlView::clickPicked,
        &window,
        [&clickPickedCount, &pickedEntity](
            const std::int32_t sector,
            const std::int32_t surface,
            const std::int32_t thing,
            const std::int32_t light)
        {
            ++clickPickedCount;
            pickedEntity = { sector, surface, thing, light };
        });
    check(view->context() == nullptr,
        "hidden viewport-release fixture does not initialize an OpenGL context");

    const auto sendPress = [view]()
    {
        const QPointF position(1.0, 1.0);
        QMouseEvent press(
            QEvent::MouseButtonPress,
            position,
            position,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QCoreApplication::sendEvent(view, &press);
    };
    const auto makeRelease = [](const Qt::MouseButton button)
    {
        const QPointF position(1.0, 1.0);
        return QMouseEvent(
            QEvent::MouseButtonRelease,
            position,
            position,
            button,
            Qt::NoButton,
            Qt::NoModifier);
    };

    // Qt may notify the application filter about a native QWindow release
    // before it forwards the same release to the owning QWidget. That
    // preliminary non-widget event must not surrender the captured edit.
    QWindow nativeReceiver{};
    sendPress();
    check(view->viewportMouseGestureButton() == Qt::LeftButton,
        "synthetic GlView press captures physical LMB for Pick/Edit");
    check(clickPickedCount == 0,
        "press capture does not itself dispatch a viewport click");
    QMouseEvent preliminaryRelease = makeRelease(Qt::LeftButton);
    (void)window.eventFilterForTest(&nativeReceiver, &preliminaryRelease);
    check(view->viewportMouseGestureButton() == Qt::LeftButton,
        "preliminary QWindow release preserves capture until GlView receives its release");
    check(clickPickedCount == 0,
        "preliminary QWindow release cannot reach the GlView click path");

    QMouseEvent widgetRelease = makeRelease(Qt::LeftButton);
    QCoreApplication::sendEvent(view, &widgetRelease);
    check(view->viewportMouseGestureButton() == Qt::NoButton,
        "the actual GlView release ends the captured Pick/Edit gesture");
    check(clickPickedCount == 1
            && pickedEntity == std::array<std::int32_t, 4>{ -1, -1, -1, -1 },
        "the actual stationary GlView release dispatches exactly one ordinary viewport click");
    const int clicksAfterWidgetRelease = clickPickedCount;

    QWidget outsideWidget{};
    sendPress();
    QMouseEvent unrelatedRelease = makeRelease(Qt::RightButton);
    (void)window.eventFilterForTest(&outsideWidget, &unrelatedRelease);
    check(view->viewportMouseGestureButton() == Qt::LeftButton,
        "an outside release for another physical button cannot cancel the active gesture");
    check(clickPickedCount == clicksAfterWidgetRelease,
        "an unrelated outside release does not dispatch another viewport click");
    QMouseEvent outsideMatchingRelease = makeRelease(Qt::LeftButton);
    (void)window.eventFilterForTest(&outsideWidget, &outsideMatchingRelease);
    check(view->viewportMouseGestureButton() == Qt::NoButton,
        "a matching release delivered to an outside QWidget still cancels capture");
    check(clickPickedCount == clicksAfterWidgetRelease,
        "cancelling through an outside QWidget does not synthesize a click");

    sendPress();
    QEvent captureLost(QEvent::UngrabMouse);
    (void)window.eventFilterForTest(view, &captureLost);
    check(view->viewportMouseGestureButton() == Qt::NoButton,
        "the existing mouse-capture-loss guard still cancels an active gesture");
    check(clickPickedCount == clicksAfterWidgetRelease,
        "mouse-capture loss still cancels without synthesizing a click");
    check(view->context() == nullptr,
        "all hidden input-route checks remain independent of an OpenGL context");
}

} // namespace sed::app_tests
