#include "test_common.hpp"

#include "gl_view.hpp"

#include <unordered_set>

#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QGuiApplication>
#include <QTimer>
#include <QWindow>

namespace sed::app_tests
{
namespace
{

class GlViewPaintEventProbe final : public QObject
{
public:
    explicit GlViewPaintEventProbe(GlView& view) : view_(&view)
    {
        QCoreApplication::instance()->installEventFilter(this);
    }

    ~GlViewPaintEventProbe() override
    {
        QCoreApplication::instance()->removeEventFilter(this);
    }

    [[nodiscard]] int repaintEventCount() const noexcept
    {
        return repaintEventCount_;
    }

    [[nodiscard]] int paintEventCount() const noexcept
    {
        return paintEventCount_;
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if ( watched != view_ && watched != view_->windowHandle() )
        {
            return QObject::eventFilter(watched, event);
        }

        if ( event->type() == QEvent::Show || event->type() == QEvent::Resize )
        {
            // QOpenGLWidget may initialize its context from either event.
            return true;
        }

        if ( event->type() == QEvent::UpdateRequest || event->type() == QEvent::UpdateLater )
        {
            ++repaintEventCount_;
            return QObject::eventFilter(watched, event);
        }

        if ( event->type() == QEvent::Paint )
        {
            ++paintEventCount_;
            ++repaintEventCount_;
            // Observe Qt's scheduled repaint before QOpenGLWidget enters its
            // GL-backed paint path. The timer regression needs event delivery,
            // not a graphics context or rendered pixels.
            return true;
        }

        return QObject::eventFilter(watched, event);
    }

private:
    GlView* view_{};
    int repaintEventCount_{};
    int paintEventCount_{};
};

QTimer* newlyStartedTimer(GlView& view, const std::unordered_set<QTimer*>& activeBefore)
{
    QTimer* started{};
    for ( QTimer* const timer : view.findChildren<QTimer*>(QString{}, Qt::FindDirectChildrenOnly) )
    {
        if ( !timer->isActive() || activeBefore.contains(timer) )
        {
            continue;
        }

        if ( started )
        {
            return nullptr;
        }
        started = timer;
    }
    return started;
}

void testLastToastExpirySchedulesFinalPaintAndStops()
{
    const bool offscreen = QGuiApplication::platformName() == QStringLiteral("offscreen");
    check(offscreen, "toast timer regression runs only on the offscreen Qt platform");
    if ( !offscreen )
    {
        return;
    }

    GlView view{};
    GlViewPaintEventProbe paintProbe{view};
    view.resize(360, 240);
    view.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    check(view.context() == nullptr,
        "offscreen toast timer fixture blocks QOpenGLWidget context initialization");

    std::unordered_set<QTimer*> activeTimersBefore{};
    for ( QTimer* const timer : view.findChildren<QTimer*>(QString{}, Qt::FindDirectChildrenOnly) )
    {
        if ( timer->isActive() )
        {
            activeTimersBefore.insert(timer);
        }
    }

    view.showToast(
        QStringLiteral("Short message"),
        80,
        GlView::ToastPlacement::BottomCenter,
        GlView::ToastGroup::General);
    QTimer* const expiryTimer = newlyStartedTimer(view, activeTimersBefore);
    check(expiryTimer != nullptr,
        "showToast starts one production-owned expiry timer on the GlView");
    if ( !expiryTimer )
    {
        view.hide();
        return;
    }

    view.showToast(
        QStringLiteral("Longer error"),
        240,
        GlView::ToastPlacement::BottomCenter,
        GlView::ToastGroup::Error);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    bool mixedExpiryObserved = false;
    bool finalExpiryObserved = false;
    bool timerStoppedOnFinalExpiry = false;
    bool watchdogFired = false;
    int timerTimeoutCount = 0;
    int paintEventCountAtFinalExpiry = 0;
    QEventLoop expiryLoop{};
    QTimer watchdog{};
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &expiryLoop, [&]()
    {
        watchdogFired = true;
        expiryLoop.quit();
    });
    QObject::connect(expiryTimer, &QTimer::timeout, &view, [&]()
    {
        ++timerTimeoutCount;
        const bool generalToastActive = !view.toastTextForTest(GlView::ToastGroup::General).isEmpty();
        const bool errorToastActive = !view.toastTextForTest(GlView::ToastGroup::Error).isEmpty();
        if ( !generalToastActive && errorToastActive )
        {
            mixedExpiryObserved = true;
        }

        if ( !generalToastActive && !errorToastActive )
        {
            finalExpiryObserved = true;
            timerStoppedOnFinalExpiry = !expiryTimer->isActive();
            paintEventCountAtFinalExpiry = paintProbe.paintEventCount();
            expiryLoop.quit();
            return;
        }

        // Drain the production update for each live/mixed tick. This leaves no
        // earlier paint pending to be mistaken for the repaint at final expiry.
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    });

    watchdog.start(1500);
    expiryLoop.exec();

    check(!watchdogFired && mixedExpiryObserved && finalExpiryObserved,
        "the real toast timer clears one expired group while another toast remains, then reaches the last expiry");
    check(view.toastTextForTest(GlView::ToastGroup::General).isEmpty()
            && view.toastTextForTest(GlView::ToastGroup::Error).isEmpty()
            && timerStoppedOnFinalExpiry,
        "the last timeout clears all toast text and stops the production timer");

    QEventLoop repaintDeliveryLoop{};
    QTimer repaintPoller{};
    repaintPoller.setInterval(1);
    QObject::connect(&repaintPoller, &QTimer::timeout, &repaintDeliveryLoop, [&]()
    {
        if ( paintProbe.paintEventCount() > paintEventCountAtFinalExpiry )
        {
            repaintDeliveryLoop.quit();
        }
    });
    QTimer repaintWatchdog{};
    repaintWatchdog.setSingleShot(true);
    QObject::connect(&repaintWatchdog, &QTimer::timeout, &repaintDeliveryLoop,
        &QEventLoop::quit);
    repaintPoller.start();
    repaintWatchdog.start(250);
    repaintDeliveryLoop.exec();
    repaintPoller.stop();
    check(paintProbe.paintEventCount() > paintEventCountAtFinalExpiry,
        "last-toast timer expiry schedules and delivers a final viewport paint event");
    check(view.context() == nullptr,
        "final toast expiry repaint delivery remains independent of an OpenGL context");

    const int timerCallbacksAtStop = timerTimeoutCount;
    const int repaintsAfterFinalDelivery = paintProbe.repaintEventCount();
    QEventLoop quietLoop{};
    QTimer::singleShot(64, &quietLoop, &QEventLoop::quit);
    quietLoop.exec();
    check(!expiryTimer->isActive() && timerTimeoutCount == timerCallbacksAtStop
            && paintProbe.repaintEventCount() == repaintsAfterFinalDelivery,
        "toast expiry stops timer and repaint churn after the final frame");

    view.hide();
}

} // namespace

void runToastExpiryTimerTests()
{
    testLastToastExpirySchedulesFinalPaintAndStops();
}

} // namespace sed::app_tests
