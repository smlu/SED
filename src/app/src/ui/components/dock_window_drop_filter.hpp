#pragma once

#include <QDockWidget>
#include <QEvent>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMouseEvent>
#include <QObject>
#include <QPointer>
#include <QPropertyAnimation>
#include <QTimer>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace sed::ui
{

/** Publishes a completed native dock-window drop without changing Qt's drag. */
class DockWindowDropFilter final : public QObject
{
public:
    using GenerationProvider = std::function<std::uint64_t()>;
    using DockProvider = std::function<std::vector<QPointer<QDockWidget>>()>;
    using DropHandler = std::function<void(QDockWidget*, QDockWidget*, std::uint64_t)>;

    DockWindowDropFilter(QDockWidget* const dock, GenerationProvider generationProvider,
        DockProvider dockProvider, DropHandler dropHandler)
        : QObject(dock), dock_(dock), generationProvider_(std::move(generationProvider)),
          dockProvider_(std::move(dockProvider)), dropHandler_(std::move(dropHandler))
    {
        dock->installEventFilter(this);
        connect(dock, &QDockWidget::topLevelChanged, this, [this](const bool floating)
            {
                if ( !floating )
                {
                    queueCompletion();
                }
            });
    }

protected:
    bool eventFilter(QObject* const watched, QEvent* const event) override
    {
        if ( watched != dock_.data() || !dock_ )
        {
            return false;
        }
        if ( event->type() == QEvent::NonClientAreaMouseButtonPress )
        {
            const auto* const mouse = static_cast<QMouseEvent*>(event);
            if ( mouse->button() == Qt::LeftButton && dock_->isFloating()
                && !dock_->titleBarWidget() )
            {
                ++gestureSerial_;
                completionPending_ = false;
                active_ = true;
                canceled_ = false;
                terminalPointRecorded_ = false;
                preGestureAnimations_ = sourceGeometryAnimations();
                observedGestureAnimations_.clear();
                const QPointer<DockWindowDropFilter> self(this);
                const GenerationProvider generation = generationProvider_;
                const DockProvider peers = dockProvider_;
                const std::uint64_t context = generation();
                if ( !self || !self->dock_ )
                {
                    return true;
                }
                const std::vector<QPointer<QDockWidget>> candidates = peers();
                if ( !self || !self->dock_ )
                {
                    return true;
                }
                contextGeneration_ = context;
                lastGlobalPosition_ = mouse->globalPosition().toPoint();
                visiblePeers_.clear();
                // Remember the selected editor before Qt selects the dropped
                // window. An obscured same-kind peer is not the user's target.
                for ( const QPointer<QDockWidget>& candidate : candidates )
                {
                    if ( candidate && candidate != dock_ && candidate->isVisible()
                        && !candidate->visibleRegion().isEmpty() )
                    {
                        visiblePeers_.push_back({ candidate,
                            QRect(candidate->mapToGlobal(QPoint{}), candidate->size()) });
                    }
                }
            }
        }
        if ( active_ && (event->type() == QEvent::NonClientAreaMouseMove
                || event->type() == QEvent::NonClientAreaMouseButtonRelease
                || event->type() == QEvent::MouseMove
                || event->type() == QEvent::MouseButtonRelease) )
        {
            const auto* const mouse = static_cast<QMouseEvent*>(event);
            if ( !terminalPointRecorded_ )
            {
                lastGlobalPosition_ = mouse->globalPosition().toPoint();
                if ( !mouse->buttons().testFlag(Qt::LeftButton) )
                {
                    terminalPointRecorded_ = true;
                    updateVisiblePeerBounds();
                }
            }
            if ( !mouse->buttons().testFlag(Qt::LeftButton) )
            {
                queueCompletion();
            }
        }
        if ( active_ && event->type() == QEvent::UngrabMouse )
        {
            // Retry a completion that encountered Qt's grab only when the
            // real gesture ends. Do not poll or interrupt the native drag.
            updateVisiblePeerBounds();
            queueCompletion();
        }
        if ( active_ && event->type() == QEvent::KeyPress
            && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape )
        {
            canceled_ = true;
            queueCompletion();
        }
        if ( event->type() == QEvent::Close )
        {
            ++gestureSerial_;
            active_ = false;
            completionPending_ = false;
        }
        return false;
    }

private:
    struct VisiblePeer
    {
        QPointer<QDockWidget> dock{};
        QRect bounds{};
    };

    std::vector<QPointer<QPropertyAnimation>> sourceGeometryAnimations() const
    {
        std::vector<QPointer<QPropertyAnimation>> animations{};
        if ( !dock_ )
        {
            return animations;
        }
        for ( QPropertyAnimation* const animation :
            dock_->findChildren<QPropertyAnimation*>(QString{}, Qt::FindDirectChildrenOnly) )
        {
            if ( animation->targetObject() == dock_.data()
                && animation->propertyName() == QByteArrayLiteral("geometry") )
            {
                animations.emplace_back(animation);
            }
        }
        return animations;
    }

    bool awaitingNativePlugCompletion()
    {
        bool awaiting = false;
        for ( const QPointer<QPropertyAnimation>& animation : sourceGeometryAnimations() )
        {
            if ( std::find(preGestureAnimations_.begin(), preGestureAnimations_.end(), animation)
                != preGestureAnimations_.end() )
            {
                continue;
            }
            awaiting = true;
            if ( std::find(observedGestureAnimations_.begin(), observedGestureAnimations_.end(), animation)
                != observedGestureAnimations_.end() )
            {
                continue;
            }
            observedGestureAnimations_.push_back(animation);
            const std::uint64_t serial = gestureSerial_;
            const QPointer<DockWindowDropFilter> self(this);
            const auto retryCompletion = [self, serial]()
                {
                    if ( self && self->gestureSerial_ == serial )
                    {
                        self->queueCompletion();
                    }
                };
            connect(animation.data(), &QObject::destroyed, this, retryCompletion);
            connect(animation.data(), &QAbstractAnimation::stateChanged, this,
                [retryCompletion](const QAbstractAnimation::State state,
                    const QAbstractAnimation::State)
                {
                    if ( state == QAbstractAnimation::Stopped )
                    {
                        retryCompletion();
                    }
                });
        }
        return awaiting;
    }

    void updateVisiblePeerBounds()
    {
        for ( VisiblePeer& peer : visiblePeers_ )
        {
            if ( peer.dock )
            {
                // Unplugging can resize a target; retain its selected identity
                // while following its actual current geometry.
                peer.bounds = QRect(peer.dock->mapToGlobal(QPoint{}), peer.dock->size());
            }
        }
    }

    void queueCompletion()
    {
        if ( !active_ || completionPending_ )
        {
            return;
        }
        completionPending_ = true;
        const std::uint64_t serial = gestureSerial_;
        const QPointer<DockWindowDropFilter> self(this);
        QTimer::singleShot(0, this, [self, serial]()
            {
                if ( !self || serial != self->gestureSerial_ )
                {
                    return;
                }
                self->completionPending_ = false;
                if ( QWidget::mouseGrabber() )
                {
                    return;
                }
                if ( !self->dock_ || self->canceled_ )
                {
                    self->active_ = false;
                    return;
                }
                const GenerationProvider generation = self->generationProvider_;
                const std::uint64_t current = generation();
                if ( !self || !self->dock_ || current != self->contextGeneration_ )
                {
                    if ( self )
                    {
                        self->active_ = false;
                    }
                    return;
                }
                // Only a new source-owned geometry actor can keep this drop
                // pending through Qt's plug animation. A rejected floating
                // drop must not be reused by a later programmatic Dock action.
                if ( self->dock_->isFloating() )
                {
                    if ( !self->awaitingNativePlugCompletion() )
                    {
                        self->active_ = false;
                    }
                    return;
                }
                self->active_ = false;
                const QPointer<QMainWindow> workspace(
                    qobject_cast<QMainWindow*>(self->dock_->parentWidget()));
                if ( !workspace )
                {
                    return;
                }
                const QList<QDockWidget*> nativePeers =
                    workspace->tabifiedDockWidgets(self->dock_.data());
                QPointer<QDockWidget> target{};
                for ( const VisiblePeer& peer : self->visiblePeers_ )
                {
                    if ( peer.dock && nativePeers.contains(peer.dock.data())
                        && peer.bounds.contains(self->lastGlobalPosition_) )
                    {
                        target = peer.dock;
                        break;
                    }
                }
                if ( !target )
                {
                    return;
                }
                const QPointer<QDockWidget> source = self->dock_;
                const std::uint64_t context = self->contextGeneration_;
                const DropHandler handler = self->dropHandler_;
                // Owner callbacks can destroy this filter. Invoke a local
                // function copy and do not touch any member afterward.
                handler(source.data(), target.data(), context);
            });
    }

    QPointer<QDockWidget> dock_{};
    GenerationProvider generationProvider_{};
    DockProvider dockProvider_{};
    DropHandler dropHandler_{};
    std::vector<VisiblePeer> visiblePeers_{};
    std::vector<QPointer<QPropertyAnimation>> preGestureAnimations_{};
    std::vector<QPointer<QPropertyAnimation>> observedGestureAnimations_{};
    QPoint lastGlobalPosition_{};
    std::uint64_t contextGeneration_{};
    std::uint64_t gestureSerial_{};
    bool active_{};
    bool canceled_{};
    bool terminalPointRecorded_{};
    bool completionPending_{};
};

} // namespace sed::ui
