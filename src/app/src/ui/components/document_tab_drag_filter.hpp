#pragma once

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMetaObject>
#include <QMouseEvent>
#include <QObject>
#include <QPointer>
#include <QTabBar>
#include <QTabWidget>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace sed::ui
{

/** Moves a document tab through a guarded owner-supplied transfer callback. */
class DocumentTabDragFilter final : public QObject
{
public:
    using DropHandler = std::function<void(QTabWidget*, QWidget*, const QPoint&)>;
    using FeedbackResolver = std::function<QPointer<QTabWidget>(QTabWidget*, QWidget*, const QPoint&)>;
    using ContextProvider = std::function<std::uint64_t()>;

    DocumentTabDragFilter(DropHandler dropHandler, QObject* parent,
        FeedbackResolver feedbackResolver = {}, ContextProvider contextProvider = {}, QString feedbackText = {})
        : QObject(parent),
          dropHandler_(std::move(dropHandler)),
          feedbackResolver_(std::move(feedbackResolver)),
          contextProvider_(std::move(contextProvider)),
          feedbackText_(std::move(feedbackText)),
          hostTabBar_(qobject_cast<QTabBar*>(parent))
    {
    }

    ~DocumentTabDragFilter() override
    {
        clearFeedback();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        auto* const tabBar = qobject_cast<QTabBar*>(watched);
        if ( !event )
        {
            return QObject::eventFilter(watched, event);
        }
        const QPointer<DocumentTabDragFilter> self(this);

        const bool ownerLeaving = event->type() == QEvent::Hide || event->type() == QEvent::Close
            || event->type() == QEvent::Destroy;
        if ( page_ && ownerLeaving && (watched == sourceTabs_.data() || watched == sourceTabBar_.data()) )
        {
            clearFeedback();
            if ( !self )
            {
                return false;
            }
            cancelAndRestoreOrder();
            if ( !self )
            {
                return false;
            }
        }
        if ( page_ && dragging_ && event->type() == QEvent::MouseButtonPress
            && watched != sourceTabBar_.data()
            && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton )
        {
            clearFeedback();
            if ( !self )
            {
                return false;
            }
            cancelAndRestoreOrder();
            if ( !self )
            {
                return false;
            }
            clearGesture();
            if ( !self )
            {
                return false;
            }
        }
        if ( preview_ && ownerLeaving
            && std::any_of(previewAncestors_.begin(), previewAncestors_.end(), [watched](const auto& ancestor)
                {
                    return ancestor && ancestor.data() == watched;
                }) )
        {
            clearFeedback();
            if ( !self )
            {
                return false;
            }
        }

        // The application filter sees owner/context changes even when the
        // pointer does not move again. Keep this check gesture-scoped so an
        // idle registered tab bar adds no application-wide work.
        if ( dragging_ && page_ && contextProvider_ )
        {
            const ContextProvider contextProvider = contextProvider_;
            const std::uint64_t expectedContext = gestureContext_;
            const std::uint64_t currentContext = contextProvider();
            if ( !self )
            {
                return false;
            }
            if ( currentContext != expectedContext )
            {
                clearFeedback();
                if ( !self )
                {
                    return false;
                }
                cancelAndRestoreOrder();
                if ( !self )
                {
                    return false;
                }
            }
        }
        if ( dragging_ && (!page_ || !sourceTabs_ || !sourceTabBar_) )
        {
            clearFeedback();
            if ( !self )
            {
                return false;
            }
            clearGesture();
            if ( !self )
            {
                return false;
            }
        }
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        if ( hostTabBar_ && ((tabBar == hostTabBar_
                && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove
                    || event->type() == QEvent::MouseButtonRelease))
            || (page_ && (event->type() == QEvent::KeyPress || event->type() == QEvent::WindowDeactivate
                || event->type() == QEvent::ApplicationDeactivate || event->type() == QEvent::UngrabMouse))) )
        {
            QString trace = event->type() == QEvent::MouseButtonPress ? QString{}
                : hostTabBar_->property("resource_authoring.dragTrace").toString();
            trace.append(QStringLiteral("[%1 page=%2 drag=%3 cancel=%4]")
                .arg(static_cast<int>(event->type())).arg(static_cast<bool>(page_)).arg(dragging_).arg(canceled_));
            hostTabBar_->setProperty("resource_authoring.dragTrace", trace.right(1024));
        }
#endif

        if ( (event->type() == QEvent::ApplicationDeactivate
                || event->type() == QEvent::WindowDeactivate
                || event->type() == QEvent::UngrabMouse)
            && page_ )
        {
            clearFeedback();
            if ( !self )
            {
                return false;
            }
            cancelAndRestoreOrder();
            if ( !self )
            {
                return false;
            }
            return QObject::eventFilter(watched, event);
        }

        if ( event->type() == QEvent::ShortcutOverride && page_ && (dragging_ || canceled_) )
        {
            const auto* const key = static_cast<QKeyEvent*>(event);
            if ( key->key() == Qt::Key_Escape )
            {
                event->accept();
                clearFeedback();
                return true;
            }
        }

        if ( event->type() == QEvent::KeyPress && page_ )
        {
            const auto* const key = static_cast<QKeyEvent*>(event);
            if ( key->key() == Qt::Key_Escape )
            {
                if ( dragging_ || canceled_ )
                {
                    clearFeedback();
                    if ( !self )
                    {
                        return true;
                    }
                    cancelAndRestoreOrder();
                    if ( !self )
                    {
                        return true;
                    }
                    return true;
                }
                clearFeedback();
                if ( !self )
                {
                    return true;
                }
                clearGesture();
                if ( !self )
                {
                    return true;
                }
            }
            return QObject::eventFilter(watched, event);
        }

        if ( tabBar && tabBar == hostTabBar_
            && event->type() == QEvent::MouseButtonPress )
        {
            if ( page_ )
            {
                clearFeedback();
                if ( !self )
                {
                    return false;
                }
                cancelAndRestoreOrder();
                if ( !self )
                {
                    return false;
                }
            }
            auto* const mouse = static_cast<QMouseEvent*>(event);
            if ( mouse->button() != Qt::LeftButton )
            {
                clearFeedback();
                if ( !self )
                {
                    return false;
                }
                clearGesture();
                if ( !self )
                {
                    return false;
                }
                return QObject::eventFilter(watched, event);
            }

            QTabWidget* tabs = nullptr;
            for ( QWidget* parent = tabBar->parentWidget(); parent; parent = parent->parentWidget() )
            {
                if ( (tabs = qobject_cast<QTabWidget*>(parent)) )
                {
                    break;
                }
            }
            const int index = tabBar->tabAt(mouse->position().toPoint());
            QPointer<QWidget> page(tabs && index >= 0 ? tabs->widget(index) : nullptr);
            if ( tabs && page )
            {
                sourceTabs_ = tabs;
                sourceTabBar_ = tabBar;
                page_ = page;
                pressPosition_ = mouse->position().toPoint();
                originalIndex_ = index;
                originalOrder_.clear();
                originalOrder_.reserve(static_cast<std::size_t>(tabs->count()));
                for ( int tab = 0; tab < tabs->count(); ++tab )
                {
                    originalOrder_.emplace_back(tabs->widget(tab));
                }
                dragging_ = false;
                canceled_ = false;
                const ContextProvider contextProvider = contextProvider_;
                const std::uint64_t currentContext = contextProvider ? contextProvider() : std::uint64_t{};
                if ( !self )
                {
                    return false;
                }
                gestureContext_ = currentContext;
                if ( !self )
                {
                    return false;
                }
                const std::uint64_t serial = ++gestureSerial_;
                QObject::disconnect(pageDestroyedConnection_);
                pageDestroyedConnection_ = QObject::connect(page.data(), &QObject::destroyed, this, [this, serial]()
                    {
                        if ( gestureSerial_ == serial )
                        {
                            const QPointer<DocumentTabDragFilter> self(this);
                            clearFeedback();
                            if ( !self )
                            {
                                return;
                            }
                            clearGesture();
                        }
                    });
            }
            else
            {
                clearFeedback();
                if ( !self )
                {
                    return false;
                }
                clearGesture();
                if ( !self )
                {
                    return false;
                }
            }
            return QObject::eventFilter(watched, event);
        }

        if ( tabBar && tabBar == sourceTabBar_ && event->type() == QEvent::MouseMove
            && page_ && sourceTabs_ )
        {
            if ( canceled_ )
            {
                return true;
            }
            auto* const mouse = static_cast<QMouseEvent*>(event);
            if ( mouse->buttons().testFlag(Qt::LeftButton)
                && (mouse->position().toPoint() - pressPosition_).manhattanLength()
                    >= QApplication::startDragDistance() )
            {
                dragging_ = true;
            }
            const bool consumeMove = dragging_;
            if ( dragging_ && mouse->buttons().testFlag(Qt::LeftButton) )
            {
                updateFeedback(mouse->globalPosition().toPoint());
                if ( !self )
                {
                    return consumeMove;
                }
            }
            else if ( dragging_ )
            {
                clearFeedback();
                if ( !self )
                {
                    return consumeMove;
                }
            }
            // Once the pointer crosses the drag threshold, this editor owns the
            // gesture. Letting QTabBar handle the same move can reorder tabs
            // before a later Escape/deactivation restores the original order.
            return dragging_;
        }

        if ( event->type() == QEvent::MouseMove && page_ && dragging_ )
        {
            auto* const mouse = static_cast<QMouseEvent*>(event);
            if ( mouse->buttons().testFlag(Qt::LeftButton) )
            {
                updateFeedback(mouse->globalPosition().toPoint());
                if ( !self )
                {
                    return false;
                }
            }
            else
            {
                clearFeedback();
                if ( !self )
                {
                    return false;
                }
            }
        }

        if ( tabBar && tabBar == sourceTabBar_
            && event->type() == QEvent::MouseButtonRelease && page_ && sourceTabs_ )
        {
            auto* const mouse = static_cast<QMouseEvent*>(event);
            const QPointer<QTabWidget> sourceTabs = sourceTabs_;
            const QPointer<QWidget> page = page_;
            const bool shouldTransfer = mouse->button() == Qt::LeftButton
                && dragging_ && !canceled_;
            const bool consumeRelease = dragging_ || canceled_;
            const QPoint terminalPosition = mouse->globalPosition().toPoint();
            const DropHandler dropHandler = dropHandler_;
            clearFeedback();
            if ( self && canceled_ )
            {
                restoreOriginalOrder();
            }
            if ( self )
            {
                clearGesture();
            }
            if ( shouldTransfer && sourceTabs && page && dropHandler )
            {
                dropHandler(sourceTabs.data(), page.data(), terminalPosition);
            }
            return consumeRelease;
        }

        return QObject::eventFilter(watched, event);
    }

private:
    void updateFeedback(const QPoint globalPosition)
    {
        if ( !feedbackResolver_ || feedbackText_.isEmpty() || !sourceTabs_ || !page_ )
        {
            clearFeedback();
            return;
        }
        const QPointer<DocumentTabDragFilter> self(this);
        const FeedbackResolver resolver = feedbackResolver_;
        const ContextProvider contextProvider = contextProvider_;
        const QPointer<QTabWidget> source = sourceTabs_;
        const QPointer<QWidget> page = page_;
        const std::uint64_t expectedContext = gestureContext_;
        const std::uint64_t expectedSerial = gestureSerial_;
        QPointer<QTabWidget> target = resolver(source.data(), page.data(), globalPosition);
        if ( !self )
        {
            return;
        }
        const auto contextStillCurrent = [&]()
        {
            if ( !self || !source || !page || !self->dragging_
                || self->gestureSerial_ != expectedSerial
                || self->sourceTabs_ != source || self->page_ != page
                || self->gestureContext_ != expectedContext )
            {
                return false;
            }
            const bool sameOwnerContext = !contextProvider || contextProvider() == expectedContext;
            return self && source && page && self->dragging_
                && self->gestureSerial_ == expectedSerial
                && self->sourceTabs_ == source && self->page_ == page
                && self->gestureContext_ == expectedContext && sameOwnerContext;
        };
        const auto targetStillCurrent = [&]()
        {
            return contextStillCurrent() && target;
        };
        if ( !contextStillCurrent() )
        {
            if ( self )
            {
                self->clearFeedback();
                if ( self )
                {
                    self->cancelAndRestoreOrder();
                }
            }
            return;
        }
        QPointer<QTabBar> targetBar(target ? target->tabBar() : nullptr);
        if ( !target || target == source || !target->isVisible() || !targetBar )
        {
            clearFeedback();
            return;
        }

        if ( !preview_ || preview_->parentWidget() != target.data() )
        {
            clearFeedback();
            if ( !contextStillCurrent() )
            {
                if ( self )
                {
                    self->cancelAndRestoreOrder();
                }
                return;
            }
            QPointer<QLabel> createdPreview(new QLabel(feedbackText_, target.data()));
            if ( !self || !targetStillCurrent() || !createdPreview )
            {
                if ( createdPreview )
                {
                    delete createdPreview.data();
                }
                if ( self )
                {
                    self->clearFeedback();
                    if ( self )
                    {
                        self->cancelAndRestoreOrder();
                    }
                }
                return;
            }
            self->preview_ = createdPreview;
        }
        const auto abortStalePreview = [&]()
        {
            if ( targetStillCurrent() && self->preview_ )
            {
                return false;
            }
            if ( self )
            {
                self->clearFeedback();
                if ( self )
                {
                    self->cancelAndRestoreOrder();
                }
            }
            return true;
        };
        if ( abortStalePreview() )
        {
            return;
        }
        self->preview_->setObjectName(QStringLiteral("documentTabJoinPreview"));
        if ( abortStalePreview() )
        {
            return;
        }
        self->preview_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        if ( abortStalePreview() )
        {
            return;
        }
        self->preview_->setAttribute(Qt::WA_ShowWithoutActivating, true);
        if ( abortStalePreview() )
        {
            return;
        }
        self->preview_->setFocusPolicy(Qt::NoFocus);
        if ( abortStalePreview() )
        {
            return;
        }
        self->preview_->setStyleSheet(QStringLiteral(
            "QLabel#documentTabJoinPreview { color: white; background-color: rgba(42, 104, 164, 232); "
            "border: 1px solid rgba(230, 242, 255, 220); border-radius: 4px; padding: 2px 8px; }"));
        if ( abortStalePreview() )
        {
            return;
        }
        self->preview_->setText(feedbackText_);
        if ( abortStalePreview() )
        {
            return;
        }
        self->preview_->adjustSize();
        if ( abortStalePreview() || !targetBar )
        {
            return;
        }
        QRect tabBarRect = targetBar->geometry();
        if ( !targetBar->isVisible() || tabBarRect.isEmpty() )
        {
            const int fallbackHeight = std::min(28, target->height());
            tabBarRect = QRect(0, 0, target->width(), fallbackHeight);
        }
        const QSize hint = self->preview_->sizeHint();
        if ( abortStalePreview() )
        {
            return;
        }
        const int width = std::min(hint.width(), tabBarRect.width());
        const int height = std::min(hint.height(), tabBarRect.height());
        if ( width <= 0 || height <= 0 )
        {
            self->clearFeedback();
            return;
        }
        self->preview_->setGeometry(tabBarRect.x() + (tabBarRect.width() - width) / 2,
            tabBarRect.y() + (tabBarRect.height() - height) / 2, width, height);
        if ( abortStalePreview() )
        {
            return;
        }
        self->previewAncestors_.clear();
        for ( QWidget* ancestor = target.data(); ancestor; ancestor = ancestor->parentWidget() )
        {
            self->previewAncestors_.emplace_back(ancestor);
        }
        self->preview_->show();
        if ( abortStalePreview() )
        {
            return;
        }
        self->preview_->raise();
        if ( abortStalePreview() )
        {
            return;
        }
        self->preview_->update();
        if ( abortStalePreview() )
        {
            return;
        }
    }

    void clearFeedback()
    {
        QPointer<QLabel> preview = preview_;
        preview_.clear();
        previewAncestors_.clear();
        if ( preview )
        {
            delete preview.data();
        }
    }

    void clearGesture()
    {
        const QPointer<DocumentTabDragFilter> self(this);
        clearFeedback();
        if ( !self )
        {
            return;
        }
        page_.clear();
        sourceTabs_.clear();
        sourceTabBar_.clear();
        originalOrder_.clear();
        QObject::disconnect(pageDestroyedConnection_);
        pageDestroyedConnection_ = {};
        originalIndex_ = -1;
        pressPosition_ = {};
        gestureContext_ = {};
        dragging_ = false;
        canceled_ = false;
    }

    void cancelAndRestoreOrder()
    {
        const QPointer<DocumentTabDragFilter> self(this);
        canceled_ = true;
        if ( dragging_ )
        {
            restoreOriginalOrder();
            if ( !self )
            {
                return;
            }
        }
        dragging_ = false;
    }

    void restoreOriginalOrder()
    {
        const QPointer<DocumentTabDragFilter> self(this);
        if ( !sourceTabs_ || originalOrder_.empty()
            || sourceTabs_->count() < static_cast<int>(originalOrder_.size()) )
        {
            return;
        }
        for ( std::size_t desired = 0; desired < originalOrder_.size(); ++desired )
        {
            const QPointer<QWidget>& expected = originalOrder_[desired];
            if ( !expected )
            {
                return;
            }
            const int current = sourceTabs_->indexOf(expected.data());
            if ( current < 0 )
            {
                return;
            }
            if ( current != static_cast<int>(desired) )
            {
                sourceTabs_->tabBar()->moveTab(current, static_cast<int>(desired));
                if ( !self )
                {
                    return;
                }
            }
        }
        if ( page_ && sourceTabs_->indexOf(page_.data()) >= 0 )
        {
            sourceTabs_->setCurrentWidget(page_.data());
        }
    }

    DropHandler dropHandler_{};
    FeedbackResolver feedbackResolver_{};
    ContextProvider contextProvider_{};
    QString feedbackText_{};
    QPointer<QTabBar> hostTabBar_{};
    QPointer<QTabWidget> sourceTabs_{};
    QPointer<QTabBar> sourceTabBar_{};
    QPointer<QWidget> page_{};
    std::vector<QPointer<QWidget>> originalOrder_{};
    QPointer<QLabel> preview_{};
    std::vector<QPointer<QWidget>> previewAncestors_{};
    QMetaObject::Connection pageDestroyedConnection_{};
    QPoint pressPosition_{};
    int originalIndex_{-1};
    std::uint64_t gestureContext_{};
    std::uint64_t gestureSerial_{};
    bool dragging_{};
    bool canceled_{};
};


} // namespace sed::ui
