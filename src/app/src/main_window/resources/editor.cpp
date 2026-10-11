#include "main_window/common.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"
#include "app_icon.hpp"

#include "ui/cog/cog_script_tab_utils.hpp"
#include "ui/components/preferred_width_dock_widget.hpp"
#include "ui/components/document_tab_drag_filter.hpp"
#include "ui/components/dock_window_drop_filter.hpp"
#include "ui/components/dock_widget_retirement.hpp"
#include "ui/recovery/document_recovery_utils.hpp"
#include "ui/resources/resource_authoring_editor.hpp"

#include <QAction>
#include <QApplication>
#include <QCborArray>
#include <QCryptographicHash>
#include <QContextMenuEvent>
#include <QDockWidget>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMargins>
#include <QPointer>
#include <QSet>
#include <QScreen>
#include <QSignalBlocker>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QCborValue>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>

namespace sed
{
namespace
{

using ui::resources::ResourceAuthoringContext;
using ui::resources::ResourceAuthoringKind;

constexpr char kResourceNativeBaselineStateProperty[] =
    "resource_authoring.nativeBaselineHash";
constexpr char kResourceNotifiedModifiedProperty[] =
    "resource_authoring.notifiedModified";
constexpr char kResourceNotifiedIdentityProperty[] =
    "resource_authoring.notifiedIdentity";
constexpr std::size_t kMaximumResourceEditorPanes = 64;
constexpr int kResourceEditorLimitMessageMilliseconds = 5000;
constexpr char kResourceEditorLimitMessage[] =
    "The workspace already has the maximum of 64 resource editor tabs.";
constexpr std::size_t kMaximumResourceEditorHosts = 64;
constexpr char kResourceEditorHostLimitMessage[] =
    "The workspace already has the maximum of 64 resource editor windows.";

struct EditorDockWindowHost
{
    QPointer<QDockWidget> dock{};
    QPointer<QTabWidget> tabs{};
    QString stableId{};
    bool primary{};
    // COG has its independent host registry and no ResourceAuthoringKind.
    std::optional<ResourceAuthoringKind> resourceKind{};
};

class ResourceTabSignalBlocker final
{
public:
    explicit ResourceTabSignalBlocker(QTabWidget* const tabs)
        : tabs_(tabs), previous_(tabs && tabs->blockSignals(true))
    {
    }

    ~ResourceTabSignalBlocker()
    {
        if ( tabs_ )
        {
            tabs_->blockSignals(previous_);
        }
    }

    ResourceTabSignalBlocker(const ResourceTabSignalBlocker&) = delete;
    ResourceTabSignalBlocker& operator=(const ResourceTabSignalBlocker&) = delete;

private:
    QPointer<QTabWidget> tabs_{};
    bool previous_{};
};

QString resourcePathKey(const std::filesystem::path& path)
{
    try
    {
        return ui::recovery::normalizedSourcePath(path).toCaseFolded();
    }
    catch (...)
    {
        return {};
    }
}

QString resourceIdentityFromState(const QCborMap& state)
{
    const int sourceKind = static_cast<int>(state.value(QStringLiteral("sourceKind")).toInteger());
    if (sourceKind == 1)
    {
        return QStringLiteral("loose:") + resourcePathKey(std::filesystem::path(
            state.value(QStringLiteral("path")).toString().toStdWString()));
    }
    if (sourceKind == 2)
    {
        return QStringLiteral("project:")
            + state.value(QStringLiteral("projectResource")).toString().toCaseFolded();
    }
    return QStringLiteral("draft:") + state.value(QStringLiteral("draftIdentity")).toString();
}

void ensureFloatingDockMaximizeButton(QDockWidget* const rawDock)
{
    QPointer<QDockWidget> dock(rawDock);
    if ( !dock || !dock->isFloating() )
    {
        return;
    }

    constexpr Qt::WindowFlags kNativeFloatingWindowFlags =
        Qt::Window
        | Qt::CustomizeWindowHint
        | Qt::WindowTitleHint
        | Qt::WindowSystemMenuHint
        | Qt::WindowCloseButtonHint
        | Qt::WindowMaximizeButtonHint;
    const Qt::WindowFlags currentFlags = dock->windowFlags();
    if ( (currentFlags & Qt::WindowType_Mask) == Qt::Window
        && (currentFlags & kNativeFloatingWindowFlags) == kNativeFloatingWindowFlags )
    {
        return;
    }

    const bool wasVisible = dock->isVisible();
    const bool wasMaximized = dock->isMaximized();
    const bool wasActive = dock->isActiveWindow();
    const bool wasShowWithoutActivating = dock->testAttribute(Qt::WA_ShowWithoutActivating);
    const QRect previousFrameGeometry = dock->frameGeometry();
    const QSize previousClientSize = dock->size();
    QPointer<QWidget> previousFocus(QApplication::focusWidget());
    const bool focusWasInside = previousFocus
        && (previousFocus.data() == dock.data() || dock->isAncestorOf(previousFocus.data()));
    Qt::WindowFlags repairedFlags = currentFlags;
    repairedFlags &= ~Qt::WindowType_Mask;
    repairedFlags |= kNativeFloatingWindowFlags;

    // Qt reconstructs QDockWidget's top-level flags as Qt::Tool when it floats,
    // which suppresses the native maximize control on Windows. Repair the
    // complete caption only after the native drag transition has ended.
    if ( wasVisible )
    {
        dock->setAttribute(Qt::WA_ShowWithoutActivating, true);
        if ( !dock )
        {
            return;
        }
    }
    dock->setWindowFlags(repairedFlags);
    if ( !dock )
    {
        return;
    }
    if ( wasVisible )
    {
        if ( wasMaximized )
        {
            dock->showMaximized();
            if ( !dock )
            {
                return;
            }
        }
        else
        {
            dock->resize(previousClientSize);
            if ( !dock )
            {
                return;
            }
            dock->show();
            if ( !dock )
            {
                return;
            }
            const QRect currentFrameGeometry = dock->frameGeometry();
            dock->move(dock->pos()
                + previousFrameGeometry.topLeft() - currentFrameGeometry.topLeft());
            if ( !dock )
            {
                return;
            }
        }
    }
    if ( wasVisible )
    {
        dock->setAttribute(Qt::WA_ShowWithoutActivating, wasShowWithoutActivating);
        if ( !dock )
        {
            return;
        }
    }
    if ( !dock )
    {
        return;
    }
    if ( wasActive )
    {
        dock->raise();
        if ( !dock )
        {
            return;
        }
        dock->activateWindow();
        if ( !dock )
        {
            return;
        }
    }
    if ( focusWasInside && previousFocus && previousFocus->isVisible() )
    {
        previousFocus->setFocus(Qt::OtherFocusReason);
    }
}

QString resourceTabPath(QWidget* page)
{
    const QString sourceTooltip = page ? page->property("resource_authoring.sourceTooltip").toString() : QString{};
    if ( !sourceTooltip.isEmpty() )
    {
        return sourceTooltip;
    }
    const QString loosePath = ui::resources::resourceAuthoringPaneLooseFilePath(page);
    const QString draftTitle = ui::resources::resourceAuthoringPaneTitle(page);
    return loosePath.isEmpty()
        ? (draftTitle.isEmpty() ? QStringLiteral("Unsaved resource") : draftTitle)
        : loosePath;
}

bool validResourceHostId(const QString& value)
{
    if (value.isEmpty() || value.size() > 64)
    {
        return false;
    }
    for (const QChar character : value)
    {
        const ushort code = character.unicode();
        const bool allowed = (code >= 'a' && code <= 'z')
            || (code >= 'A' && code <= 'Z')
            || (code >= '0' && code <= '9')
            || code == '_' || code == '-';
        if (!allowed)
        {
            return false;
        }
    }
    return true;
}

QString newResourceHostId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-'));
}

QString resourceGroupKey(const ResourceAuthoringKind kind)
{
    switch ( kind )
    {
        case ResourceAuthoringKind::sprite: return QStringLiteral("sprite");
        case ResourceAuthoringKind::soundClass: return QStringLiteral("soundClass");
        case ResourceAuthoringKind::stringTable: return QStringLiteral("uni");
    }
    return {};
}

QString resourcePrimaryHostId(const ResourceAuthoringKind kind)
{
    switch ( kind )
    {
        case ResourceAuthoringKind::sprite: return QStringLiteral("primary");
        case ResourceAuthoringKind::soundClass: return QStringLiteral("primarySoundClass");
        case ResourceAuthoringKind::stringTable: return QStringLiteral("primaryUni");
    }
    return {};
}

QString resourceGroupTitle(const ResourceAuthoringKind kind)
{
    switch ( kind )
    {
        case ResourceAuthoringKind::sprite: return QStringLiteral("Sprite");
        case ResourceAuthoringKind::soundClass: return QStringLiteral("Sound Class");
        case ResourceAuthoringKind::stringTable: return QStringLiteral("UNI");
    }
    return {};
}

bool validResourceKind(const QCborValue& value)
{
    return value.isInteger() && value.toInteger() >= 0 && value.toInteger() <= 2;
}

class ResourceEditorGroupHeader final : public QWidget
{
public:
    ResourceEditorGroupHeader(QDockWidget* const dock,
        std::function<std::optional<std::uint64_t>()> contextGeneration)
        : QWidget(dock), dock_(dock), contextGeneration_(std::move(contextGeneration))
    {
        setObjectName(QStringLiteral("resource_authoring.group_header"));
        setProperty("resourceGroupHeader", true);
        setAttribute(Qt::WA_StyledBackground);
        setStyleSheet(QStringLiteral("QWidget[resourceGroupHeader=\"true\"] { background-color: #252525; }"));
        auto* const layout = new QHBoxLayout(this);
        layout->setContentsMargins(6, 2, 4, 2);
        layout->setSpacing(4);
        auto* const icon = new QLabel(this);
        icon->setObjectName(QStringLiteral("resource_group.icon"));
        icon->setPixmap(dock->windowIcon().pixmap(16, 16));
        icon->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(icon);
        auto* const title = new QLabel(dock->windowTitle(), this);
        title->setObjectName(QStringLiteral("resource_group.title"));
        title->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(title, 1);
        auto* const undock = new QToolButton(this);
        undock->setObjectName(QStringLiteral("resource_group.undock"));
        undock->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton));
        undock->setToolTip(QStringLiteral("Undock %1").arg(dock->windowTitle()));
        undock->setFocusPolicy(Qt::NoFocus);
        undock->setAutoRaise(true);
        layout->addWidget(undock);
        auto* const close = new QToolButton(this);
        close->setObjectName(QStringLiteral("resource_group.close"));
        close->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
        close->setToolTip(QStringLiteral("Close editor group"));
        close->setAutoRaise(true);
        layout->addWidget(close);
        connect(close, &QToolButton::clicked, dock, &QDockWidget::close);
        connect(undock, &QToolButton::clicked, dock, [guardedDock = QPointer<QDockWidget>(dock)]()
            {
                if ( !guardedDock )
                {
                    return;
                }
                guardedDock->setFloating(!guardedDock->isFloating());
                if ( guardedDock )
                {
                    guardedDock->show();
                    if ( guardedDock )
                    {
                        guardedDock->raise();
                    }
                    if ( guardedDock )
                    {
                        guardedDock->activateWindow();
                    }
                }
            });
        connect(dock, &QDockWidget::topLevelChanged, this, [this](const bool)
            {
                queueFloatingChrome();
            });
        dock->installEventFilter(this);
    }

    bool prepareFloatingChromeForRecoveryRestore()
    {
        const QPointer<ResourceEditorGroupHeader> self(this);
        const QPointer<QDockWidget> dock = dock_;
        if ( !dock || !dock->isFloating() || headerDragActive_
            || QApplication::mouseButtons().testFlag(Qt::LeftButton)
            || QWidget::mouseGrabber() == dock.data() )
        {
            return false;
        }

        ++chromeUpdateSerial_;
        updateFloatingChrome(true);
        return self && dock && dock->isFloating() && !dock->titleBarWidget();
    }

protected:
    bool eventFilter(QObject* const watched, QEvent* const event) override
    {
        if ( watched == dock_.data() && (event->type() == QEvent::UngrabMouse
                || event->type() == QEvent::Close
                || (event->type() == QEvent::MouseButtonRelease
                    && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton)) )
        {
            finishHeaderDrag();
            queueFloatingChrome();
        }
        if ( watched == dock_.data() && (event->type() == QEvent::NonClientAreaMouseButtonRelease
                || (event->type() == QEvent::NonClientAreaMouseMove
                    && !static_cast<QMouseEvent*>(event)->buttons().testFlag(Qt::LeftButton))) )
        {
            queueFloatingChrome();
        }
        if ( watched == dock_.data() && (event->type() == QEvent::Show || event->type() == QEvent::Hide) )
        {
            finishHeaderDrag();
        }
        if ( watched == dock_.data() && event->type() == QEvent::ContextMenu
            && dock_ && dock_->isFloating() )
        {
            event->accept();
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }

    void mousePressEvent(QMouseEvent* const event) override
    {
        if ( event->button() == Qt::LeftButton && dock_ && dock_->titleBarWidget() == this )
        {
            ++headerDragGeneration_;
            headerDragCompletionPending_ = false;
            headerDragActive_ = true;
        }
        // Ignoring the grip event lets QDockWidget handle its native drag.
        QWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* const event) override
    {
        if ( event->button() == Qt::LeftButton )
        {
            finishHeaderDrag();
        }
        QWidget::mouseReleaseEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent* const event) override
    {
        event->accept();
        if ( !dock_ || dock_->isFloating() )
        {
            return;
        }
        ui::shortcuts::KeySequenceDisplayMenu menu{};
        connect(this, &QObject::destroyed, &menu, [&menu]()
            {
                menu.close();
            });
        connect(dock_.data(), &QObject::destroyed, &menu, [&menu]()
            {
                menu.close();
            });
        QAction* const undock = menu.addAction(QStringLiteral("Undock"));
        const QPointer<QDockWidget> dock = dock_;
        if ( menu.exec(event->globalPos()) == undock && dock )
        {
            dock->setFloating(true);
            if ( dock )
            {
                dock->show();
            }
            if ( dock )
            {
                dock->raise();
            }
            if ( dock )
            {
                dock->activateWindow();
            }
        }
    }

private:
    void queueFloatingChrome()
    {
        if ( !dock_ )
        {
            return;
        }
        const std::uint64_t serial = ++chromeUpdateSerial_;
        const std::uint64_t gesture = headerDragGeneration_;
        const std::optional<std::uint64_t> context = contextGeneration_();
        if ( !context )
        {
            return;
        }
        const bool floating = dock_->isFloating();
        const QPointer<ResourceEditorGroupHeader> self(this);
        const QPointer<QDockWidget> dock = dock_;
        // setTitleBarWidget changes Qt's native window state. Wait until the
        // topLevelChanged transition and any native mouse grab have finished.
        QTimer::singleShot(0, this, [self, dock, serial, gesture, context, floating]()
            {
                if ( !self || !dock || self->chromeUpdateSerial_ != serial
                    || self->headerDragGeneration_ != gesture
                    || self->contextGeneration_() != context
                    || dock->isFloating() != floating || self->headerDragActive_
                    || QApplication::mouseButtons().testFlag(Qt::LeftButton)
                    || QWidget::mouseGrabber() == dock.data() )
                {
                    return;
                }
                self->updateFloatingChrome(floating);
            });
    }

    void finishHeaderDrag()
    {
        if ( !headerDragActive_ || headerDragCompletionPending_ )
        {
            return;
        }
        headerDragCompletionPending_ = true;
        const std::uint64_t generation = headerDragGeneration_;
        // Qt must finish the release/drop before changing its titlebar or
        // native window flags. This is an event boundary, not a timed delay.
        QTimer::singleShot(0, this, [this, generation]()
            {
                if ( generation != headerDragGeneration_ )
                {
                    return;
                }
                headerDragCompletionPending_ = false;
                // QWidget::releaseMouse does not emit UngrabMouse on every
                // platform. Abort can instead recreate/show the dock window.
                // Start-drag also shows it, so keep its live Qt grab intact.
                if ( dock_ && QWidget::mouseGrabber() == dock_.data() )
                {
                    return;
                }
                headerDragActive_ = false;
                if ( dock_ )
                {
                    queueFloatingChrome();
                }
            });
    }

    void updateFloatingChrome(const bool floating)
    {
        QPointer<ResourceEditorGroupHeader> self(this);
        const QPointer<QDockWidget> dock = dock_;
        QPointer<QWidget> body(dock ? dock->widget() : nullptr);
        const QPointer<QVBoxLayout> bodyLayout(body ? qobject_cast<QVBoxLayout*>(body->layout()) : nullptr);
        const std::optional<std::uint64_t> context = contextGeneration_();
        if ( !context || !dock || !body || !bodyLayout || dock->isFloating() != floating || headerDragActive_ )
        {
            return;
        }
        // A custom QDockWidget titlebar suppresses native floating decorations.
        // Retain this same grip as a body row until Qt docks the window again.
        if ( floating )
        {
            if ( dock->titleBarWidget() )
            {
                dock->setTitleBarWidget(nullptr);
                if ( !self || !dock || !body || !bodyLayout
                    || contextGeneration_() != context || !dock->isFloating()
                    || QWidget::mouseGrabber() == dock.data() )
                {
                    return;
                }
                refreshWindowsNativeWindowFrame(*dock);
            }
            if ( !self || !dock || !body || !bodyLayout
                || contextGeneration_() != context || !dock->isFloating() )
            {
                return;
            }
            ensureFloatingDockMaximizeButton(dock.data());
            if ( !self || !dock || !body || !bodyLayout
                || contextGeneration_() != context || !dock->isFloating() )
            {
                return;
            }
            setParent(body.data());
            if ( !self || !dock || !body || !bodyLayout )
            {
                return;
            }
            if ( bodyLayout->indexOf(this) < 0 )
            {
                bodyLayout->insertWidget(0, this);
            }
        }
        else
        {
            if ( dock->titleBarWidget() != this )
            {
                bodyLayout->removeWidget(this);
                setParent(dock.data());
                if ( !self || !dock || !body || !bodyLayout )
                {
                    return;
                }
                dock->setTitleBarWidget(this);
            }
        }
        if ( !self || !dock )
        {
            return;
        }
        if ( QToolButton* const toggle = findChild<QToolButton*>(QStringLiteral("resource_group.undock")) )
        {
            const QString action = floating ? QStringLiteral("Dock %1") : QStringLiteral("Undock %1");
            toggle->setToolTip(action.arg(dock->windowTitle()));
            toggle->setAccessibleName(toggle->toolTip());
            toggle->show();
        }
        show();
    }

    QPointer<QDockWidget> dock_{};
    std::function<std::optional<std::uint64_t>()> contextGeneration_{};
    std::uint64_t chromeUpdateSerial_{};
    std::uint64_t headerDragGeneration_{};
    bool headerDragActive_{};
    bool headerDragCompletionPending_{};
};

template<typename Hosts>
void refreshResourceGroupToolbars(const Hosts& hosts, const std::uint64_t generation)
{
    std::array<bool, 3> modified{};
    for ( const auto& host : hosts )
    {
        if ( !host.tabs || host.sessionGeneration != generation )
        {
            continue;
        }
        for ( int index = 0; index < host.tabs->count(); ++index )
        {
            modified[static_cast<std::size_t>(host.kind)] = modified[static_cast<std::size_t>(host.kind)]
                || ui::resources::resourceAuthoringPaneIsModified(host.tabs->widget(index));
        }
    }
    for ( const auto& host : hosts )
    {
        if ( !host.tabs || !host.dock || host.sessionGeneration != generation || !host.dock->widget() )
        {
            continue;
        }
        QWidget* const toolbar = host.dock->widget()->template findChild<QWidget*>(
            QStringLiteral("resource_authoring.group_toolbar"), Qt::FindDirectChildrenOnly);
        QToolButton* const save = toolbar ? toolbar->findChild<QToolButton*>(
            QStringLiteral("resource_authoring.save"), Qt::FindDirectChildrenOnly) : nullptr;
        if ( !save || !save->menu() )
        {
            continue;
        }
        const bool currentModified = ui::resources::resourceAuthoringPaneIsModified(host.tabs->currentWidget());
        const bool groupModified = modified[static_cast<std::size_t>(host.kind)];
        save->setProperty("resource_authoring.currentModified", currentModified);
        save->setEnabled(currentModified || groupModified);
        QAction* const saveAll = save->menu()->findChild<QAction*>(
            QStringLiteral("resource_authoring.save_all"), Qt::FindDirectChildrenOnly);
        if ( saveAll )
        {
            saveAll->setEnabled(groupModified);
        }
    }
}

void restoreResourceDockPresentation(QDockWidget* const dock, const QCborMap& state)
{
    QPointer<QDockWidget> guardedDock(dock);
    if ( !guardedDock )
    {
        return;
    }
    guardedDock->setFloating(state.value(QStringLiteral("floating")).toBool());
    if ( !guardedDock )
    {
        return;
    }
    if ( guardedDock->isFloating() )
    {
        if ( auto* const header = dynamic_cast<ResourceEditorGroupHeader*>(
                guardedDock->titleBarWidget()) )
        {
            if ( !header->prepareFloatingChromeForRecoveryRestore() || !guardedDock )
            {
                return;
            }
        }
        ensureFloatingDockMaximizeButton(guardedDock.data());
        if ( !guardedDock )
        {
            return;
        }
    }
    const QByteArray geometry = state.value(QStringLiteral("geometry")).toByteArray();
    // The nested main-window state owns docked local geometry. QWidget's
    // geometry codec applies screen/frame adjustments to top-level windows.
    if ( guardedDock->isFloating() && !geometry.isEmpty() )
    {
        guardedDock->restoreGeometry(geometry);
        if ( !guardedDock )
        {
            return;
        }
    }
    if ( guardedDock->isFloating() )
    {
        const QRect desired = guardedDock->frameGeometry();
        QScreen* screen = QGuiApplication::screenAt(desired.center());
        if ( !screen )
        {
            screen = QGuiApplication::primaryScreen();
        }
        if ( screen )
        {
            const QRect available = screen->availableGeometry();
            const QRect client(
                guardedDock->mapToGlobal(QPoint{}), guardedDock->size());
            const QMargins frameMargins(
                std::max(0, client.left() - desired.left()),
                std::max(0, client.top() - desired.top()),
                std::max(0, desired.right() - client.right()),
                std::max(0, desired.bottom() - client.bottom()));
            const QSize maximumClientSize(
                std::max(1, available.width() - frameMargins.left() - frameMargins.right()),
                std::max(1, available.height() - frameMargins.top() - frameMargins.bottom()));
            const QSize size = guardedDock->size().boundedTo(maximumClientSize);
            if ( size != guardedDock->size() )
            {
                guardedDock->resize(size);
                if ( !guardedDock )
                {
                    return;
                }
            }
            const QRect frame = guardedDock->frameGeometry();
            const QPoint origin(
                std::clamp(frame.left(), available.left(),
                    std::max(available.left(), available.right() - frame.width() + 1)),
                std::clamp(frame.top(), available.top(),
                    std::max(available.top(), available.bottom() - frame.height() + 1)));
            guardedDock->move(guardedDock->pos() + origin - frame.topLeft());
            if ( !guardedDock )
            {
                return;
            }
        }
    }
    guardedDock->setVisible(state.value(QStringLiteral("visible")).toBool());
}

QCborMap resourceContentProjection(const QCborMap& state)
{
    QCborMap projection{};
    for (const QString& key : {
             QStringLiteral("kind"),
             QStringLiteral("profile"),
             QStringLiteral("name"),
             QStringLiteral("sprite"),
             QStringLiteral("spritePendingNumericEdits"),
             QStringLiteral("rows"),
             QStringLiteral("capacity") })
    {
        if (state.contains(key))
        {
            projection.insert(key, state.value(key));
        }
    }
    return projection;
}

QCborMap resourceAuthoredTabProjection(const QCborMap& state)
{
    QCborMap projection = resourceContentProjection(state);
    for (const QString& key : {
             QStringLiteral("sourceKind"),
             QStringLiteral("path"),
             QStringLiteral("projectResource"),
             QStringLiteral("draftIdentity") })
    {
        if (state.contains(key))
        {
            projection.insert(key, state.value(key));
        }
    }
    return projection;
}

QByteArray resourceAuthoredPageHash(const QCborMap& state)
{
    return QCryptographicHash::hash(
        QCborValue(resourceAuthoredTabProjection(state)).toCbor(),
        QCryptographicHash::Sha256);
}

void setResourceNativeBaselineState(QWidget* page, const QCborMap& state)
{
    if (page)
    {
        page->setProperty(
            kResourceNativeBaselineStateProperty,
            resourceAuthoredPageHash(state));
    }
}

QByteArray resourceNativeBaselineHash(const QWidget* page)
{
    if (!page)
    {
        return {};
    }
    return page->property(kResourceNativeBaselineStateProperty).toByteArray();
}


} // namespace

void MainWindow::setupResourceEditorsDock()
{
    if ( !workspaceWindow_ )
    {
        return;
    }

    workspaceWindow_->setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);
    QDockWidget* first = nullptr;
    for ( const ResourceAuthoringKind kind : {
              ResourceAuthoringKind::sprite, ResourceAuthoringKind::soundClass,
              ResourceAuthoringKind::stringTable } )
    {
        QDockWidget* const dock = createResourceEditorDockHost(
            resourcePrimaryHostId(kind), false, kind, true);
        if ( !first )
        {
            first = dock;
        }
        else if ( dock )
        {
            workspaceWindow_->tabifyDockWidget(first, dock);
        }
    }
}

void MainWindow::registerResourceEditorTabHost(
    QDockWidget* const dock,
    QTabWidget* const tabs,
    const bool primary,
    QString stableId,
    const ResourceAuthoringKind kind)
{
    if ( !dock || !tabs )
    {
        return;
    }
    resourceEditorTabHosts_.erase(
        std::remove_if(
            resourceEditorTabHosts_.begin(),
            resourceEditorTabHosts_.end(),
            [](const ResourceEditorTabHost& host)
            {
                return !host.dock || !host.tabs;
            }),
        resourceEditorTabHosts_.end());
    for ( ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( host.tabs == tabs )
        {
            host.dock = dock;
            host.sessionGeneration = resourceEditorDocumentContextGeneration_;
            if (!stableId.isEmpty())
            {
                host.stableId = std::move(stableId);
            }
            host.primary = host.primary || primary;
            host.kind = kind;
            dock->setProperty("resource_authoring.kind", static_cast<int>(kind));
            if ( primary && kind == ResourceAuthoringKind::sprite )
            {
                resourceEditorsDock_ = dock;
                resourceEditorTabs_ = tabs;
            }
            return;
        }
    }
    ResourceEditorTabHost host{};
    host.dock = dock;
    host.tabs = tabs;
    host.sessionGeneration = resourceEditorDocumentContextGeneration_;
    host.stableId = stableId.isEmpty()
        ? (primary ? QStringLiteral("primary") : newResourceHostId())
        : std::move(stableId);
    host.primary = primary;
    host.kind = kind;
    dock->setProperty("resource_authoring.kind", static_cast<int>(kind));
    resourceEditorTabHosts_.push_back(host);
    if ( primary && kind == ResourceAuthoringKind::sprite )
    {
        resourceEditorsDock_ = dock;
        resourceEditorTabs_ = tabs;
    }
}

void MainWindow::unregisterResourceEditorTabHost(QTabWidget* const tabs)
{
    if ( !tabs )
    {
        return;
    }
    const auto iterator = std::remove_if(
        resourceEditorTabHosts_.begin(),
        resourceEditorTabHosts_.end(),
        [tabs](const ResourceEditorTabHost& host)
        {
            return !host.tabs || host.tabs == tabs;
        });
    resourceEditorTabHosts_.erase(iterator, resourceEditorTabHosts_.end());
}

QDockWidget* MainWindow::createResourceEditorDockHost(
    const QString& stableId,
    const bool floating,
    const ResourceAuthoringKind kind,
    const bool primary)
{
    if (!workspaceWindow_ || !validResourceHostId(stableId)
        || (primary ? stableId != resourcePrimaryHostId(kind)
            : stableId == resourcePrimaryHostId(ResourceAuthoringKind::sprite)
                || stableId == resourcePrimaryHostId(ResourceAuthoringKind::soundClass)
                || stableId == resourcePrimaryHostId(ResourceAuthoringKind::stringTable)))
    {
        return nullptr;
    }
    auto* const preferredDock = new ui::PreferredWidthDockWidget(
        resourceGroupTitle(kind), workspaceWindow_);
    auto* const dock = preferredDock;
    // Seed the maximize hint while the host is hidden; Qt reconstructs dock
    // flags when floating, so the safe transition path repairs it after release.
    dock->setWindowFlag(Qt::WindowMaximizeButtonHint, true);
    dock->setObjectName(primary
        ? kind == ResourceAuthoringKind::sprite ? QStringLiteral("resourceEditorsDock")
            : kind == ResourceAuthoringKind::soundClass ? QStringLiteral("soundClassResourceEditorsDock")
            : QStringLiteral("uniResourceEditorsDock")
        : QStringLiteral("resourceEditorsDetachedDock_%1").arg(stableId));
    dock->setProperty("resource_authoring.hostId", stableId);
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setFeatures(
        QDockWidget::DockWidgetMovable
        | QDockWidget::DockWidgetFloatable
        | QDockWidget::DockWidgetClosable);
    dock->setWindowIcon(ui::makeViewportIcon(kind == ResourceAuthoringKind::sprite
        ? ui::ViewportIcon::resourceSprite : kind == ResourceAuthoringKind::soundClass
            ? ui::ViewportIcon::sound : ui::ViewportIcon::actionWhitespace));
    dock->setTitleBarWidget(new ResourceEditorGroupHeader(dock,
        [owner = QPointer<MainWindow>(this)]() -> std::optional<std::uint64_t>
        {
            if ( !owner || owner->recoveryShuttingDown_ )
            {
                return std::nullopt;
            }
            return owner->resourceEditorDocumentContextGeneration_;
        }));
    dock->setMinimumWidth(400);
    preferredDock->setPreferredWidth(570);

    auto* const root = new QWidget(dock);
    root->setObjectName(QStringLiteral("resourceEditorGroupCanvas"));
    root->setStyleSheet(QStringLiteral(
        "QWidget#resourceEditorGroupCanvas, QDialog, QTabWidget::pane {"
        " background-color: #303030;"
        "}"));
    auto* const layout = new QVBoxLayout(root);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    auto* const tabs = new QTabWidget(root);
    tabs->setObjectName(primary
        ? kind == ResourceAuthoringKind::sprite ? QStringLiteral("resourceEditorTabs")
            : kind == ResourceAuthoringKind::soundClass ? QStringLiteral("soundClassResourceEditorTabs")
            : QStringLiteral("uniResourceEditorTabs")
        : QStringLiteral("resourceEditorTabsDetached"));
    tabs->setDocumentMode(true);
    tabs->setTabsClosable(true);
    tabs->setMovable(true);
    tabs->setProperty("cogHoverIndex", -1);
    layout->addWidget(tabs, 1);
    dock->setWidget(root);
    workspaceWindow_->addDockWidget(Qt::RightDockWidgetArea, dock);
    registerResourceEditorTabHost(dock, tabs, primary, stableId, kind);
    configureResourceEditorTabHost(dock, tabs);
    const QPointer<QDockWidget> guardedDock(dock);
    guardedDock->setFloating(floating);
    if ( !guardedDock )
    {
        return nullptr;
    }
    if ( floating )
    {
        ensureFloatingDockMaximizeButton(guardedDock.data());
    }
    if ( !guardedDock )
    {
        return nullptr;
    }
    guardedDock->hide();
    return guardedDock.data();
}

QCborMap MainWindow::normalizeResourceEditorRecoveryState(const QCborMap& payload) const
{
    if ( payload.contains(QStringLiteral("groups")) )
    {
        return payload;
    }
    QCborMap normalized = payload;
    QCborArray records = payload.value(QStringLiteral("tabs")).toArray();
    QCborArray sourceHosts = payload.value(QStringLiteral("hosts")).toArray();
    if ( sourceHosts.isEmpty() )
    {
        QCborMap primary{};
        primary.insert(QStringLiteral("id"), QStringLiteral("primary"));
        primary.insert(QStringLiteral("primary"), true);
        primary.insert(QStringLiteral("visible"), payload.value(QStringLiteral("dockVisible")));
        primary.insert(QStringLiteral("floating"), false);
        primary.insert(QStringLiteral("geometry"), payload.value(QStringLiteral("dockGeometry")));
        primary.insert(QStringLiteral("active"), payload.value(QStringLiteral("active")));
        QCborArray indices{};
        for ( qsizetype index = 0; index < records.size(); ++index )
        {
            indices.append(index);
        }
        primary.insert(QStringLiteral("tabs"), indices);
        sourceHosts.append(primary);
    }
    QCborMap groups{};
    for ( const ResourceAuthoringKind kind : { ResourceAuthoringKind::sprite,
              ResourceAuthoringKind::soundClass, ResourceAuthoringKind::stringTable } )
    {
        QTabWidget* const tabs = primaryResourceEditorTabHost(kind);
        QDockWidget* const dock = resourceEditorDockForTabs(tabs);
        QCborMap state{};
        state.insert(QStringLiteral("visible"), false);
        state.insert(QStringLiteral("floating"), false);
        state.insert(QStringLiteral("geometry"), dock ? dock->saveGeometry() : QByteArray{});
        state.insert(QStringLiteral("active"), -1);
        groups.insert(resourceGroupKey(kind), state);
    }
    QCborArray hosts{};
    QSet<QString> assignedIds{};
    for ( const QCborValue& sourceValue : sourceHosts )
    {
        const QCborMap source = sourceValue.toMap();
        const QCborArray sourceIndices = source.value(QStringLiteral("tabs")).toArray();
        const qint64 sourceActive = source.value(QStringLiteral("active")).toInteger();
        const qint64 activeGlobal = sourceActive >= 0 && sourceActive < sourceIndices.size()
            ? sourceIndices.at(static_cast<qsizetype>(sourceActive)).toInteger() : -1;
        for ( const ResourceAuthoringKind kind : { ResourceAuthoringKind::sprite,
                  ResourceAuthoringKind::soundClass, ResourceAuthoringKind::stringTable } )
        {
            QCborArray indices{};
            qint64 activeLocal = 0;
            for ( const QCborValue& indexValue : sourceIndices )
            {
                const qint64 index = indexValue.toInteger();
                const QCborMap page = records.at(static_cast<qsizetype>(index)).toMap()
                    .value(QStringLiteral("page")).toMap();
                if ( page.value(QStringLiteral("kind")).toInteger() != static_cast<int>(kind) )
                {
                    continue;
                }
                if ( index == activeGlobal )
                {
                    activeLocal = indices.size();
                }
                indices.append(index);
            }
            if ( indices.isEmpty() )
            {
                continue;
            }
            QCborMap host = source;
            const bool primary = source.value(QStringLiteral("primary")).toBool();
            QString id = primary ? resourcePrimaryHostId(kind)
                : source.value(QStringLiteral("id")).toString();
            const bool reservedId = id == resourcePrimaryHostId(ResourceAuthoringKind::sprite)
                || id == resourcePrimaryHostId(ResourceAuthoringKind::soundClass)
                || id == resourcePrimaryHostId(ResourceAuthoringKind::stringTable);
            if ( !primary && (assignedIds.contains(id) || reservedId) )
            {
                id = QString::fromLatin1(QCryptographicHash::hash(
                    (id + QLatin1Char(':') + resourceGroupKey(kind)).toUtf8(),
                    QCryptographicHash::Sha256).toHex().left(32));
            }
            assignedIds.insert(id);
            host.insert(QStringLiteral("id"), id);
            host.insert(QStringLiteral("kind"), static_cast<int>(kind));
            host.insert(QStringLiteral("tabs"), indices);
            host.insert(QStringLiteral("active"), activeLocal);
            for ( const QCborValue& indexValue : indices )
            {
                const qsizetype index = static_cast<qsizetype>(indexValue.toInteger());
                QCborMap record = records.at(index).toMap();
                record.insert(QStringLiteral("host"), hosts.size());
                records[index] = record;
            }
            if ( primary )
            {
                QCborMap group{};
                for ( const QString& key : { QStringLiteral("visible"), QStringLiteral("floating"),
                          QStringLiteral("geometry"), QStringLiteral("active") } )
                {
                    group.insert(key, host.value(key));
                }
                groups.insert(resourceGroupKey(kind), group);
            }
            hosts.append(host);
        }
    }
    const QCborMap spriteGroup = groups.value(QStringLiteral("sprite")).toMap();
    normalized.insert(QStringLiteral("dockVisible"), spriteGroup.value(QStringLiteral("visible")));
    normalized.insert(QStringLiteral("dockGeometry"), spriteGroup.value(QStringLiteral("geometry")));
    normalized.insert(QStringLiteral("groups"), groups);
    normalized.insert(QStringLiteral("tabs"), records);
    normalized.insert(QStringLiteral("hosts"), hosts);
    return normalized;
}

bool MainWindow::prepareResourceEditorRecoveryHosts(const QCborMap& payload)
{
    QString warning{};
    if ( !workspaceWindow_ || !validateResourceEditorRecoveryState(payload, &warning) )
    {
        return false;
    }
    const QCborMap normalized = normalizeResourceEditorRecoveryState(payload);
    QSet<QString> desiredIds{};
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( host.primary )
        {
            desiredIds.insert(host.stableId);
        }
    }
    for ( const QCborValue& hostValue : normalized.value(QStringLiteral("hosts")).toArray() )
    {
        const QCborMap state = hostValue.toMap();
        const QString id = state.value(QStringLiteral("id")).toString();
        const auto kind = static_cast<ResourceAuthoringKind>(state.value(QStringLiteral("kind")).toInteger());
        desiredIds.insert(id);
        bool found = false;
        for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
        {
            if ( host.stableId == id && host.dock && host.tabs && host.kind == kind )
            {
                found = true;
            }
        }
        if ( !found && !createResourceEditorDockHost(id,
                state.value(QStringLiteral("floating")).toBool(), kind,
                state.value(QStringLiteral("primary")).toBool()) )
        {
            return false;
        }
    }
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( !host.primary && host.dock && !desiredIds.contains(host.stableId) )
        {
            host.dock->hide();
            workspaceWindow_->removeDockWidget(host.dock.data());
        }
    }
    return true;
}

void MainWindow::transferResourceEditorTab(
    QTabWidget* const source,
    QWidget* const page,
    const QPoint& globalPosition)
{
    QPointer<MainWindow> self(this);
    QPointer<QTabWidget> sourceTabs(source);
    QPointer<QWidget> pageGuard(page);
    if ( !sourceTabs || !pageGuard
        || resourceEditorTabsForPage(pageGuard.data()) != sourceTabs.data() )
    {
        return;
    }

    const std::uint64_t contextGeneration =
        resourceEditorDocumentContextGeneration_;
    const ResourceAuthoringKind kind = ui::resources::resourceAuthoringPaneKind(page);
    QDockWidget* const sourceDock = resourceEditorDockForTabs(sourceTabs.data());
    QPointer<QDockWidget> sourceDockGuard(sourceDock);
    const int sourceIndex = sourceTabs->indexOf(pageGuard.data());
    if ( !sourceDockGuard || sourceIndex < 0 )
    {
        return;
    }

    bool rejectCrossKindTabBar = false;
    QTabWidget* targetTabs = resourceEditorDropTargetAtPosition(
        sourceTabs.data(), pageGuard.data(), globalPosition, &rejectCrossKindTabBar);
    if ( rejectCrossKindTabBar )
    {
        return;
    }
    QDockWidget* targetDock = targetTabs ? resourceEditorDockForTabs(targetTabs) : nullptr;
    if ( targetTabs == sourceTabs.data() )
    {
        ui::cog::normalizeCogScriptTabOrder(sourceTabs.data());
        ui::cog::refreshCogScriptDockTabPresentation(sourceTabs.data());
        return;
    }

    const QString title = sourceTabs->tabText(sourceIndex);
    const QIcon icon = sourceTabs->tabIcon(sourceIndex);
    const bool createDetachedHost = targetTabs == nullptr;
    if ( createDetachedHost )
    {
        const std::size_t liveHostCount = static_cast<std::size_t>(std::count_if(
            resourceEditorTabHosts_.begin(),
            resourceEditorTabHosts_.end(),
            [contextGeneration](const ResourceEditorTabHost& host)
            {
                return host.dock && host.tabs
                    && host.tabs->count() > 0
                    && host.sessionGeneration == contextGeneration;
            }));
        if ( liveHostCount >= kMaximumResourceEditorHosts )
        {
            if ( statusBar() )
            {
                statusBar()->showMessage(
                    QString::fromLatin1(kResourceEditorHostLimitMessage),
                    kResourceEditorLimitMessageMilliseconds);
            }
            return;
        }

        const QString stableId = newResourceHostId();
        targetDock = createResourceEditorDockHost(stableId, true, kind);
        targetTabs = targetDock
            ? targetDock->findChild<QTabWidget*>(
                QStringLiteral("resourceEditorTabsDetached"))
            : nullptr;
        if ( !targetDock || !targetTabs )
        {
            if ( targetDock )
            {
                unregisterResourceEditorTabHost(targetTabs);
                workspaceWindow_->removeDockWidget(targetDock);
                delete targetDock;
            }
            return;
        }

        constexpr QSize preferredSize(680, 460);
        QPoint origin = globalPosition
            - QPoint(preferredSize.width() / 2, 24);
        QScreen* screen = QGuiApplication::screenAt(globalPosition);
        if ( !screen )
        {
            screen = QGuiApplication::primaryScreen();
        }
        if ( screen )
        {
            const QRect available = screen->availableGeometry();
            origin.setX(std::clamp(
                origin.x(), available.left(),
                std::max(available.left(),
                    available.right() - preferredSize.width() + 1)));
            origin.setY(std::clamp(
                origin.y(), available.top(),
                std::max(available.top(),
                    available.bottom() - preferredSize.height() + 1)));
        }
        targetDock->setGeometry(QRect(origin, preferredSize));
    }

    QPointer<QDockWidget> targetDockGuard(targetDock);
    QPointer<QTabWidget> targetTabsGuard(targetTabs);
    const auto discardNewHost = [&]()
    {
        if ( !createDetachedHost || !self || !targetDockGuard )
        {
            return;
        }
        if ( targetTabsGuard )
        {
            self->unregisterResourceEditorTabHost(targetTabsGuard.data());
        }
        if ( self->workspaceWindow_ )
        {
            self->workspaceWindow_->removeDockWidget(targetDockGuard.data());
        }
        delete targetDockGuard.data();
    };

    if ( !self || !sourceTabs || !pageGuard || !sourceDockGuard
        || !targetDockGuard || !targetTabsGuard
        || resourceEditorDocumentContextGeneration_ != contextGeneration
        || resourceEditorTabsForPage(pageGuard.data()) != sourceTabs.data()
        || resourceEditorDockForTabs(sourceTabs.data()) != sourceDockGuard.data()
        || resourceEditorDockForTabs(targetTabsGuard.data()) != targetDockGuard.data()
        || sourceTabs->indexOf(pageGuard.data()) < 0 )
    {
        discardNewHost();
        return;
    }

    int targetIndex = targetTabsGuard->count();
    if ( !createDetachedHost )
    {
        if ( QTabBar* const targetBar = targetTabsGuard->tabBar() )
        {
            const QPoint localPosition =
                targetBar->mapFromGlobal(globalPosition);
            const int hoveredIndex = targetBar->tabAt(localPosition);
            if ( hoveredIndex >= 0 )
            {
                targetIndex = hoveredIndex;
            }
        }
    }

    const int currentSourceIndex = sourceTabs->indexOf(pageGuard.data());
    if ( currentSourceIndex < 0 )
    {
        discardNewHost();
        return;
    }
    {
        const ResourceTabSignalBlocker sourceSignals(sourceTabs.data());
        sourceTabs->removeTab(currentSourceIndex);
    }
    if ( !self || !sourceTabs || !pageGuard || !targetTabsGuard
        || !sourceDockGuard || !targetDockGuard
        || resourceEditorDocumentContextGeneration_ != contextGeneration )
    {
        if ( sourceTabs && pageGuard
            && sourceTabs->indexOf(pageGuard.data()) < 0 )
        {
            sourceTabs->insertTab(
                std::clamp(sourceIndex, 0, sourceTabs->count()),
                pageGuard.data(), icon, title);
        }
        discardNewHost();
        return;
    }
    targetIndex = std::clamp(targetIndex, 0, targetTabsGuard->count());
    {
        const ResourceTabSignalBlocker targetSignals(targetTabsGuard.data());
        targetTabsGuard->insertTab(targetIndex, pageGuard.data(), icon, title);
    }
    if ( !self || !sourceTabs || !pageGuard || !targetTabsGuard
        || targetTabsGuard->indexOf(pageGuard.data()) < 0
        || resourceEditorDocumentContextGeneration_ != contextGeneration )
    {
        return;
    }
    targetTabsGuard->setCurrentWidget(pageGuard.data());

    if ( !self || !sourceTabs || !pageGuard || !sourceDockGuard
        || !targetDockGuard || !targetTabsGuard
        || resourceEditorTabsForPage(pageGuard.data()) != targetTabsGuard.data() )
    {
        return;
    }
    const auto transferredStillCurrent = [self, sourceTabs, pageGuard, sourceDockGuard,
        targetDockGuard, targetTabsGuard, contextGeneration]()
    {
        return self && sourceTabs && pageGuard && sourceDockGuard && targetDockGuard
            && targetTabsGuard && self->resourceEditorDocumentContextGeneration_ == contextGeneration
            && self->resourceEditorTabsForPage(pageGuard.data()) == targetTabsGuard.data()
            && self->resourceEditorDockForTabs(sourceTabs.data()) == sourceDockGuard.data()
            && self->resourceEditorDockForTabs(targetTabsGuard.data()) == targetDockGuard.data();
    };
    if ( createDetachedHost )
    {
        targetDockGuard->show();
        if ( !transferredStillCurrent() )
        {
            return;
        }
        targetDockGuard->raise();
        if ( !transferredStillCurrent() )
        {
            return;
        }
        targetDockGuard->activateWindow();
    }
    else if ( targetDockGuard->isHidden() )
    {
        targetDockGuard->show();
    }
    if ( !transferredStillCurrent() )
    {
        return;
    }

    ui::cog::normalizeCogScriptTabOrder(sourceTabs.data());
    if ( !transferredStillCurrent() )
    {
        return;
    }
    ui::cog::refreshCogScriptDockTabPresentation(sourceTabs.data());
    if ( !transferredStillCurrent() )
    {
        return;
    }
    ui::cog::normalizeCogScriptTabOrder(targetTabsGuard.data());
    if ( !transferredStillCurrent() )
    {
        return;
    }
    ui::cog::refreshCogScriptDockTabPresentation(targetTabsGuard.data());
    if ( !transferredStillCurrent() )
    {
        return;
    }

    if ( sourceTabs->count() == 0 )
    {
        if ( resourceEditorTabHostIsPrimary(sourceTabs.data()) )
        {
            sourceDockGuard->hide();
        }
        else
        {
            const QPointer<QMainWindow> workspace(workspaceWindow_);
            if ( !workspace )
            {
                return;
            }
            if ( !ui::removeDockWidgetAndCompleteAnimations(workspace.data(), sourceDockGuard.data(),
                [self, contextGeneration]()
                {
                    return self && self->resourceEditorDocumentContextGeneration_ == contextGeneration;
                }) )
            {
                return;
            }
            if ( !self || !workspace || !sourceDockGuard || !sourceTabs
                || self->resourceEditorDocumentContextGeneration_ != contextGeneration )
            {
                return;
            }
            self->unregisterResourceEditorTabHost(sourceTabs.data());
            sourceDockGuard->deleteLater();
        }
    }
    if ( self && self->resourceEditorDocumentContextGeneration_ == contextGeneration )
    {
        self->noteRecoveryEditorStateChangedIfNeeded();
    }
}

void MainWindow::observeResourceEditorWindowDrops(QDockWidget* const dock)
{
    if ( !dock || dock->property("resource_authoring.windowDropObserved").toBool() )
    {
        return;
    }
    dock->setProperty("resource_authoring.windowDropObserved", true);
    const QPointer<MainWindow> self(this);
    new ui::DockWindowDropFilter(dock,
        [self]()
        {
            return self ? self->resourceEditorDocumentContextGeneration_ : std::uint64_t{};
        },
        [self]()
        {
            std::vector<QPointer<QDockWidget>> docks{};
            if ( !self || self->recoveryShuttingDown_ )
            {
                return docks;
            }
            const std::uint64_t generation = self->resourceEditorDocumentContextGeneration_;
            for ( const ResourceEditorTabHost& host : self->resourceEditorTabHosts_ )
            {
                if ( host.dock && host.tabs && host.sessionGeneration == generation )
                {
                    docks.push_back(host.dock);
                }
            }
            for ( const CogScriptEditorTabHost& host : self->cogScriptEditorTabHosts_ )
            {
                if ( host.dock && host.tabs && host.sessionGeneration == generation )
                {
                    docks.push_back(host.dock);
                }
            }
            return docks;
        },
        [self](QDockWidget* const source, QDockWidget* const target, const std::uint64_t generation)
        {
            if ( self )
            {
                self->mergeResourceEditorDockHosts(source, target, generation);
            }
        });
}

void MainWindow::mergeResourceEditorDockHosts(QDockWidget* const sourceDock,
    QDockWidget* const targetDock, const std::uint64_t generation)
{
    if ( !sourceDock || !targetDock || sourceDock == targetDock || !workspaceWindow_
        || recoveryShuttingDown_ || !documentEditingAllowed()
        || generation != resourceEditorDocumentContextGeneration_
        || QWidget::mouseGrabber() || sourceDock->isFloating()
        || !workspaceWindow_->tabifiedDockWidgets(sourceDock).contains(targetDock) )
    {
        return;
    }
    std::optional<EditorDockWindowHost> sourceHost{};
    std::optional<EditorDockWindowHost> targetHost{};
    const auto consider = [&sourceHost, &targetHost, sourceDock, targetDock](const EditorDockWindowHost& host)
    {
        if ( host.dock == sourceDock )
        {
            sourceHost = host;
        }
        if ( host.dock == targetDock )
        {
            targetHost = host;
        }
    };
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( host.sessionGeneration == generation && host.dock && host.tabs )
        {
            consider({ host.dock, host.tabs, host.stableId, host.primary, host.kind });
        }
    }
    for ( const CogScriptEditorTabHost& host : cogScriptEditorTabHosts_ )
    {
        if ( host.sessionGeneration == generation && host.dock && host.tabs )
        {
            consider({ host.dock, host.tabs, host.stableId, host.primary, std::nullopt });
        }
    }
    if ( !sourceHost || !targetHost || sourceHost->resourceKind != targetHost->resourceKind
        || sourceHost->tabs->count() == 0 )
    {
        return;
    }
    const QPointer<MainWindow> self(this);
    const QPointer<QMainWindow> workspace(workspaceWindow_);
    const QPointer<QWidget> sourceCurrent(sourceHost->tabs->currentWidget());
    const EditorDockWindowHost destination = sourceHost->primary ? *sourceHost : *targetHost;
    const EditorDockWindowHost retired = sourceHost->primary ? *targetHost : *sourceHost;
    if ( !sourceCurrent || retired.primary )
    {
        return;
    }
    const auto hostDock = [self](const EditorDockWindowHost& host) -> QDockWidget*
    {
        if ( !self || !host.tabs )
        {
            return nullptr;
        }
        return host.resourceKind ? self->resourceEditorDockForTabs(host.tabs.data())
            : self->cogScriptEditorDockForTabs(host.tabs.data());
    };
    const auto pageHost = [self, destination](const QWidget* const page) -> QTabWidget*
    {
        if ( !self || !page )
        {
            return nullptr;
        }
        return destination.resourceKind ? self->resourceEditorTabsForPage(page)
            : self->cogScriptEditorTabsForPage(page);
    };
    const auto owned = [self, workspace, sourceHost, targetHost, hostDock, generation]()
    {
        return self && workspace && sourceHost->dock && sourceHost->tabs
            && targetHost->dock && targetHost->tabs && !self->recoveryShuttingDown_
            && self->resourceEditorDocumentContextGeneration_ == generation
            && hostDock(*sourceHost) == sourceHost->dock.data()
            && hostDock(*targetHost) == targetHost->dock.data();
    };
    std::vector<QPointer<QWidget>> movingPages{};
    for ( int index = 0; index < retired.tabs->count(); ++index )
    {
        QWidget* const page = retired.tabs->widget(index);
        if ( !page || pageHost(page) != retired.tabs.data()
            || (retired.resourceKind
                && ui::resources::resourceAuthoringPaneKind(page) != *retired.resourceKind) )
        {
            return;
        }
        movingPages.emplace_back(page);
    }
    int insertion = sourceHost->primary ? 0 : destination.tabs->count();
    {
        const ResourceTabSignalBlocker from(retired.tabs.data());
        const ResourceTabSignalBlocker to(destination.tabs.data());
        for ( const QPointer<QWidget>& page : movingPages )
        {
            if ( !owned() || !page || pageHost(page.data()) != retired.tabs.data() )
            {
                return;
            }
            const int index = retired.tabs->indexOf(page.data());
            const QIcon icon = retired.tabs->tabIcon(index);
            const QString title = retired.tabs->tabText(index);
            const QString tooltip = retired.tabs->tabToolTip(index);
            const bool enabled = retired.tabs->isTabEnabled(index);
            retired.tabs->removeTab(index);
            if ( !owned() || !page )
            {
                return;
            }
            const int inserted = destination.tabs->insertTab(insertion++, page.data(), icon, title);
            if ( !owned() || !page || pageHost(page.data()) != destination.tabs.data() )
            {
                return;
            }
            destination.tabs->setTabToolTip(inserted, tooltip);
            if ( !owned() || !page )
            {
                return;
            }
            destination.tabs->setTabEnabled(inserted, enabled);
            if ( !owned() || !page )
            {
                return;
            }
        }
    }
    if ( !owned() || !sourceCurrent || destination.tabs->indexOf(sourceCurrent.data()) < 0 )
    {
        return;
    }
    ui::cog::normalizeCogScriptTabOrder(destination.tabs.data());
    if ( !owned() )
    {
        return;
    }
    ui::cog::refreshCogScriptDockTabPresentation(destination.tabs.data());
    if ( !owned() || !sourceCurrent )
    {
        return;
    }
    destination.tabs->setCurrentWidget(sourceCurrent.data());
    if ( !owned() )
    {
        return;
    }
    if ( destination.resourceKind )
    {
        self->setProperty("resource_authoring.lastActiveHostId", destination.stableId);
    }
    else
    {
        self->lastActiveCogScriptEditorHost_ = destination.tabs;
        if ( self->preferredCogScriptEditorHost_ == retired.tabs )
        {
            self->preferredCogScriptEditorHost_ = destination.tabs;
        }
    }
    if ( !owned() )
    {
        return;
    }
    destination.dock->show();
    if ( !owned() )
    {
        return;
    }
    destination.dock->raise();
    if ( !owned() )
    {
        return;
    }
    if ( destination.resourceKind )
    {
        self->unregisterResourceEditorTabHost(retired.tabs.data());
        if ( !ui::removeDockWidgetAndCompleteAnimations(workspace.data(), retired.dock.data(),
            [self, generation]()
            {
                return self && self->resourceEditorDocumentContextGeneration_ == generation;
            }) )
        {
            return;
        }
        if ( !self || !workspace || !retired.dock
            || self->resourceEditorDocumentContextGeneration_ != generation )
        {
            return;
        }
        retired.dock->hide();
        if ( !self || !workspace
            || self->resourceEditorDocumentContextGeneration_ != generation )
        {
            return;
        }
        if ( retired.dock )
        {
            retired.dock->deleteLater();
        }
    }
    else
    {
        // This path unregisters the empty COG host and cancels its native
        // target animation through removeDockWidget before deferred deletion.
        self->cleanupEmptyCogScriptEditorHosts();
    }
    if ( !self || self->resourceEditorDocumentContextGeneration_ != generation )
    {
        return;
    }
    refreshResourceGroupToolbars(self->resourceEditorTabHosts_, generation);
    if ( self && self->resourceEditorDocumentContextGeneration_ == generation )
    {
        self->refreshCogScriptEditorToolbars();
    }
    if ( self && self->resourceEditorDocumentContextGeneration_ == generation )
    {
        self->syncCogScriptEditorActionFocus();
    }
    if ( self && self->resourceEditorDocumentContextGeneration_ == generation )
    {
        if ( destination.resourceKind )
        {
            self->noteRecoveryEditorStateChangedIfNeeded();
        }
        else
        {
            self->noteRecoveryStateChanged();
        }
    }
}

void MainWindow::configureResourceEditorTabHost(
    QDockWidget* const dock,
    QTabWidget* const tabs)
{
    if ( !dock || !tabs )
    {
        return;
    }
    observeResourceEditorWindowDrops(dock);
    QPointer<MainWindow> self(this);
    QPointer<QDockWidget> dockGuard(dock);
    QPointer<QTabWidget> tabsGuard(tabs);
    const auto kind = static_cast<ResourceAuthoringKind>(
        dock->property("resource_authoring.kind").toInt());
    auto* const toolbar = new QWidget(dock->widget());
    toolbar->setObjectName(QStringLiteral("resource_authoring.group_toolbar"));
    auto* const toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(4);
    const auto makeButton = [toolbar, toolbarLayout](
                                const ui::ViewportIcon icon,
                                const QString& tooltip,
                                const char* name)
    {
        auto* const button = new QToolButton(toolbar);
        button->setObjectName(QString::fromLatin1(name));
        button->setIcon(ui::makeViewportIcon(icon));
        button->setToolTip(tooltip);
        button->setAutoRaise(true);
        toolbarLayout->addWidget(button);
        return button;
    };
    QToolButton* const newButton = makeButton(ui::ViewportIcon::resourceNew,
        QStringLiteral("New %1 resource").arg(resourceGroupTitle(kind)), "resource_authoring.new");
    QToolButton* const openButton = makeButton(ui::ViewportIcon::resourceOpen,
        QStringLiteral("Open %1 resource from the active project").arg(resourceGroupTitle(kind)), "resource_authoring.open");
    QToolButton* const saveButton = makeButton(ui::ViewportIcon::actionSave,
        QStringLiteral("Save the current resource"), "resource_authoring.save");
    auto* const saveMenu = new ui::shortcuts::KeySequenceDisplayMenu(saveButton);
    QAction* const saveAll = saveMenu->addAction(QStringLiteral("Save All %1").arg(resourceGroupTitle(kind)));
    saveAll->setObjectName(QStringLiteral("resource_authoring.save_all"));
    saveButton->setMenu(saveMenu);
    saveButton->setPopupMode(QToolButton::MenuButtonPopup);
    toolbarLayout->addStretch(1);
    if ( auto* const layout = qobject_cast<QVBoxLayout*>(dock->widget()->layout()) )
    {
        layout->insertWidget(0, toolbar);
    }
    const auto updateToolbar = [self]()
    {
        if ( self )
        {
            refreshResourceGroupToolbars(self->resourceEditorTabHosts_,
                self->resourceEditorDocumentContextGeneration_);
        }
    };
    connect(newButton, &QToolButton::clicked, this, [self, tabsGuard, kind]()
        {
            if ( self && tabsGuard )
            {
                self->openResourceEditor(kind, 0, tabsGuard.data());
            }
        });
    connect(openButton, &QToolButton::clicked, this, [self, tabsGuard, kind]()
        {
            if ( self && tabsGuard )
            {
                (void)self->openProjectResourceInNewTab(nullptr, kind, tabsGuard.data());
            }
        });
    connect(saveButton, &QToolButton::clicked, this, [self, tabsGuard]()
        {
            if ( self && tabsGuard && tabsGuard->currentWidget()
                && ui::resources::resourceAuthoringPaneIsModified(tabsGuard->currentWidget()) )
            {
                (void)ui::resources::resourceAuthoringPaneSave(tabsGuard->currentWidget(), false);
            }
        });
    connect(saveAll, &QAction::triggered, this, [self, kind]()
        {
            if ( self )
            {
                (void)self->saveResourceEditorGroup(kind);
            }
        });
    connect(tabs, &QTabWidget::currentChanged, toolbar, updateToolbar);
    updateToolbar();
    connect(tabs, &QTabWidget::tabCloseRequested, this,
        [self, dockGuard, tabsGuard](const int requestedIndex)
        {
            if ( !self || !dockGuard || !tabsGuard
                || self->resourceEditorDockForTabs(tabsGuard.data()) != dockGuard.data()
                || requestedIndex < 0 || requestedIndex >= tabsGuard->count() )
            {
                return;
            }
            QPointer<QWidget> page(tabsGuard->widget(requestedIndex));
            if ( !page || !ui::cog::cogScriptTabCanClose(page.data()) )
            {
                return;
            }
            const QCborMap before =
                ui::resources::captureResourceAuthoringPaneState(page.data(), false);
            struct GuardedResourcePage
            {
                QPointer<QWidget> page{};
                QPointer<QTabWidget> tabs{};
                QCborMap authored{};
            };
            std::vector<GuardedResourcePage> otherPages{};
            for (const QPointer<QWidget>& otherPage : self->resourceEditorPages())
            {
                if (otherPage && otherPage.data() != page.data())
                {
                    QTabWidget* const otherTabs =
                        self->resourceEditorTabsForPage(otherPage.data());
                    otherPages.push_back({
                        otherPage,
                        otherTabs,
                        resourceAuthoredTabProjection(
                            ui::resources::captureResourceAuthoringPaneState(
                                otherPage.data(), false))});
                }
            }
            const QString identity =
                ui::resources::resourceAuthoringPaneIdentity(page.data());
            bool discarded = false;
            if ( !ui::resources::resourceAuthoringPaneConfirmClose(
                    page.data(), &discarded) )
            {
                return;
            }
            if ( !self || !dockGuard || !tabsGuard || !page
                || self->resourceEditorDockForTabs(tabsGuard.data()) != dockGuard.data()
                || tabsGuard->indexOf(page.data()) < 0
                || !ui::cog::cogScriptTabCanClose(page.data())
                || ui::resources::resourceAuthoringPaneIdentity(page.data()) != identity
                || resourceContentProjection(before)
                    != resourceContentProjection(
                        ui::resources::captureResourceAuthoringPaneState(page.data(), false)) )
            {
                return;
            }
            for (const GuardedResourcePage& otherPage : otherPages)
            {
                if (!otherPage.page || !otherPage.tabs
                    || self->resourceEditorTabsForPage(otherPage.page.data())
                        != otherPage.tabs.data()
                    || resourceAuthoredTabProjection(
                           ui::resources::captureResourceAuthoringPaneState(
                               otherPage.page.data(), false))
                        != otherPage.authored)
                {
                    return;
                }
            }
            const int currentIndex = tabsGuard->indexOf(page.data());
            if ( ui::resources::resourceAuthoringPaneIsModified(page.data()) )
            {
                ++self->resourceEditorAuthoredGeneration_;
            }
            tabsGuard->removeTab(currentIndex);
            ui::resources::closeResourceAuthoringPaneAfterReplacement(page.data());
            self->resourceEditorPaneChanged(nullptr);
            if ( tabsGuard->count() == 0 )
            {
                const bool primaryHost = self->resourceEditorTabHostIsPrimary(tabsGuard.data());
                if ( !primaryHost )
                {
                    self->unregisterResourceEditorTabHost(tabsGuard.data());
                }
                if ( !primaryHost )
                {
                    dockGuard->hide();
                    dockGuard->deleteLater();
                }
            }
        });
    connect(tabs, &QTabWidget::currentChanged, this, [self, dockGuard, tabsGuard](const int)
        {
            if ( !self || !dockGuard || !tabsGuard )
            {
                return;
            }
            if ( tabsGuard->count() == 0 && !self->resourceEditorTabHostIsPrimary(tabsGuard.data()) )
            {
                dockGuard->hide();
            }
            if ( tabsGuard->count() > 0 )
            {
                self->setProperty("resource_authoring.lastActiveHostId",
                    dockGuard->property("resource_authoring.hostId"));
            }
            self->noteRecoveryEditorStateChangedIfNeeded();
        });
    connect(dock, &QDockWidget::visibilityChanged, this, [self, dockGuard, tabsGuard](const bool visible)
        {
            if ( self )
            {
                if ( visible && dockGuard && tabsGuard && tabsGuard->count() > 0 )
                {
                    self->setProperty("resource_authoring.lastActiveHostId",
                        dockGuard->property("resource_authoring.hostId"));
                }
                self->noteRecoveryEditorStateChangedIfNeeded();
            }
        });

    QTabBar* const tabBar = tabs->tabBar();
    if ( !tabBar )
    {
        return;
    }
    tabBar->setMouseTracking(true);
    tabBar->setIconSize(QSize(16, 16));
    tabBar->installEventFilter(new ui::cog::CogScriptTabHoverFilter(tabs));
    auto* const dragFilter = new ui::DocumentTabDragFilter(
        [self](QTabWidget* const sourceTabs,
               QWidget* const page,
               const QPoint& globalPosition)
        {
            if ( self )
            {
                self->transferResourceEditorTab(
                    sourceTabs, page, globalPosition);
            }
        },
        tabBar,
        [self](QTabWidget* const sourceTabs, QWidget* const page, const QPoint& globalPosition)
        {
            return self ? QPointer<QTabWidget>(self->resourceEditorDropTargetAtPosition(
                sourceTabs, page, globalPosition)) : QPointer<QTabWidget>{};
        },
        [self]()
        {
            return self ? self->resourceEditorDocumentContextGeneration_ : std::uint64_t{};
        },
        QStringLiteral("Join %1 file tabs").arg(resourceGroupTitle(kind)));
    qApp->installEventFilter(dragFilter);
    tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tabBar, &QTabBar::customContextMenuRequested, this,
        [self, dockGuard, tabsGuard, tabBar](const QPoint& position)
        {
            QPointer<QTabBar> tabBarGuard(tabBar);
            if ( !self || !dockGuard || !tabsGuard || !tabBarGuard
                || self->resourceEditorDockForTabs(tabsGuard.data()) != dockGuard.data() )
            {
                return;
            }
            const int index = tabBarGuard->tabAt(position);
            QPointer<QWidget> page(index >= 0 ? tabsGuard->widget(index) : nullptr);
            if ( !page )
            {
                return;
            }
            std::vector<QPointer<QWidget>> initialPages{};
            for ( int tab = 0; tab < tabsGuard->count(); ++tab )
            {
                initialPages.emplace_back(tabsGuard->widget(tab));
            }
            const QString identity =
                ui::resources::resourceAuthoringPaneIdentity(page.data());
            const bool pinned = page->property("cogPinned").toBool();
            ui::shortcuts::KeySequenceDisplayMenu menu{};
            QObject::connect(self.data(), &QObject::destroyed, &menu, [&menu]()
                {
                    menu.close();
                });
            QObject::connect(tabBar, &QObject::destroyed, &menu, [&menu]()
                {
                    menu.close();
                });
            QAction* const close = menu.addAction(QStringLiteral("Close"));
            QAction* const closeOthers = menu.addAction(QStringLiteral("Close Others"));
            QAction* const closeRight = menu.addAction(
                QStringLiteral("Close Tabs to the Right"));
            QAction* const closeSaved = menu.addAction(QStringLiteral("Close Saved Tabs"));
            QAction* const closeAll = menu.addAction(QStringLiteral("Close All"));
            menu.addSeparator();
            QAction* const pin = menu.addAction(
                pinned ? QStringLiteral("Unpin Tab") : QStringLiteral("Pin Tab"));
            QAction* const selected =
                menu.exec(tabBarGuard->mapToGlobal(position));
            if ( !self || !dockGuard || !tabsGuard || !tabBarGuard || !page
                || self->resourceEditorDockForTabs(tabsGuard.data()) != dockGuard.data()
                || tabsGuard->count() != static_cast<int>(initialPages.size())
                || tabsGuard->indexOf(page.data()) != index
                || ui::resources::resourceAuthoringPaneIdentity(page.data()) != identity
                || page->property("cogPinned").toBool() != pinned )
            {
                return;
            }
            for ( int tab = 0; tab < tabsGuard->count(); ++tab )
            {
                if ( !initialPages[static_cast<std::size_t>(tab)]
                    || tabsGuard->widget(tab)
                        != initialPages[static_cast<std::size_t>(tab)].data() )
                {
                    return;
                }
            }
            if ( selected == pin )
            {
                page->setProperty("cogPinned", !pinned);
                ui::cog::normalizeCogScriptTabOrder(tabsGuard.data());
                ui::cog::refreshCogScriptDockTabPresentation(tabsGuard.data());
                self->noteRecoveryEditorStateChangedIfNeeded();
            }
            else if (selected == close || selected == closeOthers
                || selected == closeRight || selected == closeSaved || selected == closeAll)
            {
                std::vector<QPointer<QWidget>> targets{};
                const int currentContextIndex = tabsGuard->indexOf(page.data());
                for (int tab = 0; tab < tabsGuard->count(); ++tab)
                {
                    QWidget* const target = tabsGuard->widget(tab);
                    if (!target || !ui::cog::cogScriptTabCanClose(target))
                    {
                        continue;
                    }
                    const bool include = selected == close
                        ? target == page.data()
                        : selected == closeOthers
                            ? target != page.data()
                            : selected == closeRight
                                ? tab > currentContextIndex
                                : selected == closeSaved
                                    ? !ui::resources::resourceAuthoringPaneIsModified(target)
                                    : true;
                    if (include)
                    {
                        targets.emplace_back(target);
                    }
                }
                for (const QPointer<QWidget>& target : targets)
                {
                    if (!self || !tabsGuard || !target)
                    {
                        return;
                    }
                    const int targetIndex = tabsGuard->indexOf(target.data());
                    if (targetIndex >= 0)
                    {
                        Q_EMIT tabsGuard->tabCloseRequested(targetIndex);
                    }
                }
            }
        });
    connect(tabBar, &QTabBar::tabMoved, this, [self, tabsGuard](const int, const int)
        {
            if ( !self || !tabsGuard )
            {
                return;
            }
            ui::cog::normalizeCogScriptTabOrder(tabsGuard.data());
            ui::cog::refreshCogScriptDockTabPresentation(tabsGuard.data());
            self->noteRecoveryEditorStateChangedIfNeeded();
        });
}

std::vector<QPointer<QWidget>> MainWindow::resourceEditorPages() const
{
    std::vector<QPointer<QWidget>> pages{};
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( !host.tabs || host.tabs->count() == 0
            || host.sessionGeneration != resourceEditorDocumentContextGeneration_ )
        {
            continue;
        }
        for ( int index = 0; index < host.tabs->count(); ++index )
        {
            if ( QWidget* const page = host.tabs->widget(index) )
            {
                pages.emplace_back(page);
            }
        }
    }
    if ( pages.empty() && resourceEditorTabs_ )
    {
        for ( int index = 0; index < resourceEditorTabs_->count(); ++index )
        {
            if ( QWidget* const page = resourceEditorTabs_->widget(index) )
            {
                pages.emplace_back(page);
            }
        }
    }
    return pages;
}

QTabWidget* MainWindow::resourceEditorTabsForPage(const QWidget* const page) const noexcept
{
    if ( !page )
    {
        return nullptr;
    }
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( host.tabs && host.sessionGeneration == resourceEditorDocumentContextGeneration_
            && host.tabs->indexOf(const_cast<QWidget*>(page)) >= 0 )
        {
            return host.tabs.data();
        }
    }
    return nullptr;
}

QTabWidget* MainWindow::resourceEditorDropTargetAtPosition(
    QTabWidget* const source,
    QWidget* const page,
    const QPoint& globalPosition,
    bool* const rejectCrossKindTabBar) const noexcept
{
    if ( rejectCrossKindTabBar )
    {
        *rejectCrossKindTabBar = false;
    }
    const std::uint64_t context = resourceEditorDocumentContextGeneration_;
    if ( !source || !page || resourceEditorTabsForPage(page) != source )
    {
        return nullptr;
    }
    const ResourceAuthoringKind kind = ui::resources::resourceAuthoringPaneKind(page);
    const ResourceEditorTabHost* sourceHost = nullptr;
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( host.tabs == source && host.dock && host.sessionGeneration == context && host.kind == kind )
        {
            sourceHost = &host;
            break;
        }
    }
    if ( !sourceHost || !sourceHost->dock->isVisible() )
    {
        return nullptr;
    }

    const auto hostForDock = [this, context](const QDockWidget* const dock)
    {
        return std::find_if(resourceEditorTabHosts_.cbegin(), resourceEditorTabHosts_.cend(),
            [dock, context](const ResourceEditorTabHost& host)
            {
                return host.dock == dock && host.tabs && host.sessionGeneration == context;
            });
    };
    const auto isKnownResourceTabs = [](const QTabWidget* const tabs)
    {
        return tabs && (tabs->objectName().startsWith(QStringLiteral("resourceEditorTabs"))
            || tabs->objectName() == QStringLiteral("soundClassResourceEditorTabs")
            || tabs->objectName() == QStringLiteral("uniResourceEditorTabs"));
    };
    if ( QWidget* const underPointer = QApplication::widgetAt(globalPosition) )
    {
        QTabWidget* targetTabs = nullptr;
        QDockWidget* targetDock = nullptr;
        MainWindow* pointerOwner = nullptr;
        for ( QWidget* current = underPointer; current; current = current->parentWidget() )
        {
            if ( !pointerOwner )
            {
                pointerOwner = dynamic_cast<MainWindow*>(current);
            }
            if ( !targetTabs )
            {
                targetTabs = qobject_cast<QTabWidget*>(current);
            }
            if ( !targetDock )
            {
                targetDock = qobject_cast<QDockWidget*>(current);
            }
        }
        if ( pointerOwner && pointerOwner != this )
        {
            if ( rejectCrossKindTabBar )
            {
                *rejectCrossKindTabBar = true;
            }
            return nullptr;
        }
        if ( targetDock )
        {
            const bool isCogDock = targetDock->property("sed.cogScriptEditorDock").toBool()
                || targetDock->property("sed.cogScriptEditorHostId").isValid();
            const bool isResourceDock = targetDock->property("resource_authoring.kind").isValid();
            if ( isCogDock )
            {
                if ( rejectCrossKindTabBar )
                {
                    *rejectCrossKindTabBar = true;
                }
                return nullptr;
            }
            if ( isResourceDock )
            {
                const auto found = hostForDock(targetDock);
                if ( found == resourceEditorTabHosts_.cend() || found->kind != kind
                    || !targetDock->isVisible() || targetDock->visibleRegion().isEmpty() )
                {
                    if ( rejectCrossKindTabBar )
                    {
                        *rejectCrossKindTabBar = true;
                    }
                    return nullptr;
                }
                return found->tabs.data();
            }
            // This window's unrelated docks have no Join target; let the ordinary drop detach.
            return nullptr;
        }
        if ( isKnownResourceTabs(targetTabs)
            || (targetTabs && targetTabs->property("sed.cogScriptEditorHost").toBool()) )
        {
            const auto found = std::find_if(resourceEditorTabHosts_.cbegin(), resourceEditorTabHosts_.cend(),
                [targetTabs, context](const ResourceEditorTabHost& host)
                {
                    return host.tabs == targetTabs && host.dock && host.sessionGeneration == context;
                });
            if ( targetTabs->property("sed.cogScriptEditorHost").toBool()
                || found == resourceEditorTabHosts_.cend() || found->kind != kind
                || !found->dock->isVisible() || found->dock->visibleRegion().isEmpty() )
            {
                if ( rejectCrossKindTabBar )
                {
                    *rejectCrossKindTabBar = true;
                }
                return nullptr;
            }
            return targetTabs;
        }
        // A visible widget owns this point, so do not fall through to an obscured dock rectangle.
        return nullptr;
    }

    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( !host.dock || !host.tabs || host.sessionGeneration != context
            || !host.dock->isVisible() || host.dock->visibleRegion().isEmpty() )
        {
            continue;
        }
        const QRect globalGeometry(host.dock->mapToGlobal(QPoint{}), host.dock->size());
        if ( !globalGeometry.contains(globalPosition) )
        {
            continue;
        }
        if ( host.kind != kind )
        {
            QTabBar* const fileBar = host.tabs->tabBar();
            if ( fileBar && fileBar->isVisible()
                && QRect(fileBar->mapToGlobal(QPoint{}), fileBar->size()).contains(globalPosition) )
            {
                if ( rejectCrossKindTabBar )
                {
                    *rejectCrossKindTabBar = true;
                }
                return nullptr;
            }
            continue;
        }
        return host.tabs.data();
    }
    return nullptr;
}

QDockWidget* MainWindow::resourceEditorDockForTabs(const QTabWidget* const tabs) const noexcept
{
    if ( !tabs )
    {
        return nullptr;
    }
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( host.tabs == tabs && host.sessionGeneration == resourceEditorDocumentContextGeneration_ )
        {
            return host.dock.data();
        }
    }
    return nullptr;
}

QTabWidget* MainWindow::activeResourceEditorTabHost() const noexcept
{
    QWidget* const focus = QApplication::focusWidget();
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( !host.tabs || host.sessionGeneration != resourceEditorDocumentContextGeneration_ )
        {
            continue;
        }
        if ( focus && host.dock && (host.dock.data() == focus || host.dock->isAncestorOf(focus)) )
        {
            return host.tabs.data();
        }
        // Native window activation can lag QWidget focus when returning from
        // a floating dock. Use it only while no widget owns focus.
        if ( !focus && host.dock && host.tabs->count() > 0 && host.dock->isFloating()
            && QApplication::activeWindow() == host.dock.data() )
        {
            return host.tabs.data();
        }
    }
    const QString lastActiveId = property("resource_authoring.lastActiveHostId").toString();
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( host.stableId == lastActiveId && host.tabs && host.tabs->count() > 0
            && host.sessionGeneration == resourceEditorDocumentContextGeneration_ )
        {
            return host.tabs.data();
        }
    }
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( host.tabs && host.tabs->count() > 0
            && host.sessionGeneration == resourceEditorDocumentContextGeneration_
            && host.primary )
        {
            return host.tabs.data();
        }
    }
    for ( auto host = resourceEditorTabHosts_.rbegin();
          host != resourceEditorTabHosts_.rend();
          ++host )
    {
        if ( host->tabs && host->tabs->count() > 0
            && host->sessionGeneration == resourceEditorDocumentContextGeneration_ )
        {
            return host->tabs.data();
        }
    }
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( host.primary && host.tabs
            && host.sessionGeneration == resourceEditorDocumentContextGeneration_ )
        {
            return host.tabs.data();
        }
    }
    return nullptr;
}

QTabWidget* MainWindow::primaryResourceEditorTabHost(const ResourceAuthoringKind kind) const noexcept
{
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( host.primary && host.kind == kind && host.tabs )
        {
            return host.tabs.data();
        }
    }
    return nullptr;
}

bool MainWindow::resourceEditorTabHostIsPrimary(const QTabWidget* const tabs) const noexcept
{
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( host.tabs.data() == tabs )
        {
            return host.primary;
        }
    }
    return false;
}

bool MainWindow::saveResourceEditorGroup(const ResourceAuthoringKind kind)
{
    const std::uint64_t generation = resourceEditorDocumentContextGeneration_;
    QPointer<MainWindow> self(this);
    const QPointer<QTabWidget> activeTabs(activeResourceEditorTabHost());
    const QPointer<QWidget> activePage(activeTabs ? activeTabs->currentWidget() : nullptr);
    QString activeId{};
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( host.tabs == activeTabs )
        {
            activeId = host.stableId;
            break;
        }
    }
    const auto preserveSelection = [self, generation, activeTabs, activePage, activeId]()
    {
        if ( self && generation == self->resourceEditorDocumentContextGeneration_
            && activeTabs && activePage
            && self->resourceEditorTabsForPage(activePage.data()) == activeTabs.data() )
        {
            activeTabs->setCurrentWidget(activePage.data());
            self->setProperty("resource_authoring.lastActiveHostId", activeId);
        }
    };
    std::vector<QPointer<QWidget>> targets{};
    for ( const QPointer<QWidget>& page : resourceEditorPages() )
    {
        if ( page && ui::resources::resourceAuthoringPaneKind(page.data()) == kind
            && ui::resources::resourceAuthoringPaneIsModified(page.data()) )
        {
            targets.push_back(page);
        }
    }
    for ( const QPointer<QWidget>& page : targets )
    {
        if ( !self || generation != self->resourceEditorDocumentContextGeneration_
            || !page || !self->resourceEditorTabsForPage(page.data())
            || !ui::resources::resourceAuthoringPaneSave(page.data(), false) )
        {
            preserveSelection();
            return false;
        }
    }
    preserveSelection();
    return true;
}

ResourceAuthoringContext MainWindow::makeResourceAuthoringContext(
    const ResourceAuthoringKind kind,
    QTabWidget* preferredHost,
    const bool enableProjectOpen)
{
    const std::filesystem::path projectDirectory = currentPath_.empty()
        ? std::filesystem::path{}
        : currentPath_.parent_path();
    std::filesystem::path startingDirectory = projectDirectory;
    std::filesystem::path automaticSaveDirectory = projectDirectory.empty()
        ? std::filesystem::path{} : projectDirectory / "misc";
    if ( !automaticSaveDirectory.empty() && kind == ResourceAuthoringKind::sprite )
    {
        automaticSaveDirectory /= "spr";
    }
    else if ( !automaticSaveDirectory.empty() && kind == ResourceAuthoringKind::soundClass )
    {
        automaticSaveDirectory /= "snd";
    }
    if ( startingDirectory.empty() )
    {
        std::filesystem::path configuredLooseRoot{};
        const auto existingDirectory = [](const std::filesystem::path& path)
        {
            std::error_code error{};
            return !path.empty()
                && std::filesystem::is_directory(path, error)
                && !error;
        };
        for ( const std::filesystem::path& root : resourceRoots_ )
        {
            QString leaf = QString::fromStdWString(root.filename().wstring());
            if ( leaf.compare(QStringLiteral("Resource"), Qt::CaseInsensitive) == 0
                && existingDirectory(root) )
            {
                configuredLooseRoot = root;
                break;
            }
        }
        if ( configuredLooseRoot.empty() )
        {
            for ( const std::filesystem::path& root : resourceRoots_ )
            {
                QString leaf = QString::fromStdWString(root.filename().wstring());
                if ( leaf.compare(QStringLiteral("Episode"), Qt::CaseInsensitive) != 0
                    && existingDirectory(root) )
                {
                    configuredLooseRoot = root;
                    break;
                }
            }
        }
        if ( !configuredLooseRoot.empty() )
        {
            startingDirectory = configuredLooseRoot / "misc";
            if ( kind == ResourceAuthoringKind::sprite )
            {
                startingDirectory /= "spr";
            }
            else if ( kind == ResourceAuthoringKind::soundClass )
            {
                startingDirectory /= "snd";
            }
            automaticSaveDirectory = startingDirectory;
        }
        else
        {
            startingDirectory = std::filesystem::path(
                fileDialogStartLocation(FileDialogHistoryGroup::openSave)
                    .toStdWString());
        }
    }
    const ProjectType expectedType = document_.level().kind;
    const std::uint64_t expectedContextGeneration =
        resourceEditorDocumentContextGeneration_;
    QPointer<MainWindow> self(this);
    QPointer<QTabWidget> preferredTabs(preferredHost
        ? preferredHost : activeResourceEditorTabHost());

    ResourceAuthoringContext context{};
    context.groupedWorkspaceChrome = true;
    context.resources = makeResourceLocatorSnapshot(resources_);
    context.projectType = expectedType;
    context.projectDirectory = projectDirectory;
    context.startingDirectory = startingDirectory;
    context.automaticSaveDirectory = automaticSaveDirectory;
    context.masterCmp = document_.level().getMasterCMP();
    context.briefNotice = [self, expectedType, expectedContextGeneration](
                              QWidget* const page, const QString& message)
    {
        if ( self && page
            && self->document_.level().kind == expectedType
            && self->resourceEditorDocumentContextGeneration_ == expectedContextGeneration
            && self->resourceEditorTabsForPage(page) )
        {
            self->statusBar()->showMessage(message, 5000);
        }
    };
    context.activateExistingSource = [self](QWidget* source, const QString& identity)
    {
        if ( !self )
        {
            return false;
        }
        for ( const QPointer<QWidget>& page : self->resourceEditorPages() )
        {
            if ( !page || page.data() == source
                || ui::resources::resourceAuthoringPaneIdentity(page.data()) != identity )
            {
                continue;
            }
            QTabWidget* const sourceTabs = self->resourceEditorTabsForPage(page.data());
            QDockWidget* const sourceDock = self->resourceEditorDockForTabs(sourceTabs);
            if ( !sourceTabs || !sourceDock )
            {
                return false;
            }
            sourceTabs->setCurrentWidget(page.data());
            sourceDock->show();
            sourceDock->raise();
            if ( sourceDock->isFloating() )
            {
                sourceDock->activateWindow();
            }
            if ( source && !self->resourceEditorTabsForPage(source) )
            {
                source->setProperty("resource_authoring.activatedExistingSource", true);
            }
            else if ( source
                && source->property("resource_authoring.transientOpen").toBool() )
            {
                QTabWidget* const transientTabs =
                    self->resourceEditorTabsForPage(source);
                if ( transientTabs )
                {
                    const int transientIndex = transientTabs->indexOf(source);
                    if ( transientIndex >= 0 )
                    {
                        transientTabs->removeTab(transientIndex);
                        ui::resources::closeResourceAuthoringPaneAfterReplacement(source);
                    }
                }
            }
            return true;
        }
        return false;
    };
    context.destinationAvailable = [self](
                                       QWidget* source,
                                       const std::filesystem::path& target)
    {
        if ( !self )
        {
            return false;
        }
        const QString targetPath = resourcePathKey(target);
        const QString targetName = QString::fromStdWString(
            target.filename().wstring()).toCaseFolded();
        for ( const QPointer<QWidget>& page : self->resourceEditorPages() )
        {
            if ( !page || page.data() == source )
            {
                continue;
            }
            const QCborMap state =
                ui::resources::captureResourceAuthoringPaneState(page.data(), false);
            const int sourceKind = static_cast<int>(
                state.value(QStringLiteral("sourceKind")).toInteger());
            const QString existingPath = sourceKind == 1
                ? resourcePathKey(std::filesystem::path(
                    state.value(QStringLiteral("path")).toString().toStdWString()))
                : QString{};
            const QString existingName = sourceKind == 1
                ? QFileInfo(state.value(QStringLiteral("path")).toString())
                    .fileName().toCaseFolded()
                : sourceKind == 2
                    ? QFileInfo(state.value(QStringLiteral("projectResource")).toString())
                        .fileName().toCaseFolded()
                    : state.value(QStringLiteral("name")).toString().toCaseFolded();
            if ( (!targetPath.isEmpty() && targetPath == existingPath)
                || (!targetName.isEmpty() && targetName == existingName) )
            {
                return false;
            }
        }
        return true;
    };
    context.projectContextStillCurrent = [
        self,
        expectedType,
        expectedContextGeneration]()
    {
        return self
            && self->document_.level().kind == expectedType
            && self->resourceEditorDocumentContextGeneration_ == expectedContextGeneration;
    };
    context.createNewDraft = [self, preferredTabs](
                                 QWidget* source,
                                 const ResourceAuthoringKind requestedKind)
    {
        if ( !self || !source )
        {
            return false;
        }
        QPointer<QWidget> sourceGuard(source);
        QPointer<QTabWidget> host(self->resourceEditorTabsForPage(source));
        if ( !host )
        {
            host = preferredTabs;
        }
        if ( !host || self->resourceEditorDockForTabs(host.data()) == nullptr )
        {
            return false;
        }
        const int beforeCount = host->count();
        self->openResourceEditor(requestedKind, 0, host.data());
        return self && sourceGuard && host
            && self->resourceEditorTabsForPage(sourceGuard.data()) == host.data()
            && host->count() == beforeCount + 1;
    };
    if ( enableProjectOpen )
    {
        context.openProjectResourceInNewTab = [self, preferredTabs](
                                                  QWidget* source,
                                                  const ResourceAuthoringKind requestedKind)
        {
            if ( !self || !source )
            {
                return false;
            }
            QTabWidget* host = self->resourceEditorTabsForPage(source);
            if ( !host )
            {
                host = preferredTabs.data();
            }
            return host && self->openProjectResourceInNewTab(
                source, requestedKind, host);
        };
    }
    return context;
}

void MainWindow::openResourceEditor(
    const ResourceAuthoringKind kind,
    const int openMode,
    QTabWidget* host)
{
    for ( ResourceEditorTabHost& existingHost : resourceEditorTabHosts_ )
    {
        if ( existingHost.primary && existingHost.tabs && existingHost.dock
            && existingHost.tabs->count() == 0 )
        {
            existingHost.sessionGeneration = resourceEditorDocumentContextGeneration_;
        }
    }
    if ( !host || !resourceEditorDockForTabs(host)
        || resourceEditorDockForTabs(host)->property("resource_authoring.kind").toInt() != static_cast<int>(kind) )
    {
        host = primaryResourceEditorTabHost(kind);
    }
    QDockWidget* const dock = resourceEditorDockForTabs(host);
    if ( !host || !dock )
    {
        return;
    }
    if ( openMode < 0 )
    {
        refreshResourceGroupToolbars(resourceEditorTabHosts_, resourceEditorDocumentContextGeneration_);
        dock->show();
        dock->raise();
        if ( dock->isFloating() )
        {
            dock->activateWindow();
        }
        noteRecoveryEditorStateChangedIfNeeded();
        return;
    }
    if ( !documentEditingAllowed() )
    {
        return;
    }
    if ( resourceEditorPages().size() >= kMaximumResourceEditorPanes )
    {
        statusBar()->showMessage(
            QString::fromLatin1(kResourceEditorLimitMessage),
            kResourceEditorLimitMessageMilliseconds);
        return;
    }

    ResourceAuthoringContext context = makeResourceAuthoringContext(kind, host);
    QWidget* const page = ui::resources::createResourceAuthoringPane(
        this,
        kind,
        std::move(context),
        [this](QWidget* changedPage)
        {
            resourceEditorPaneChanged(changedPage);
        });
    if ( !page )
    {
        return;
    }
    page->setProperty("cogPinned", false);
    host->addTab(page, ui::resources::resourceAuthoringPaneTitle(page));
    ui::cog::updateCogScriptDockTabPresentation(host, page);
    ui::cog::normalizeCogScriptTabOrder(host);
    ui::cog::refreshCogScriptDockTabPresentation(host);
    host->setCurrentWidget(page);
    dock->show();
    dock->raise();
    if ( dock->isFloating() )
    {
        dock->activateWindow();
    }
    page->setFocus(Qt::OtherFocusReason);
    resourceEditorPaneChanged(page);
}

void MainWindow::openResourceEditorProjectFile(const ResourceAuthoringKind kind)
{
    (void)openProjectResourceInNewTab(
        nullptr, kind, activeResourceEditorTabHost());
}

void MainWindow::openResourceEditorLooseFile(const ResourceAuthoringKind kind)
{
    (void)openLooseResourceInNewTab(kind, activeResourceEditorTabHost());
}

bool MainWindow::openProjectResourceInNewTab(
    QWidget* const source,
    const ResourceAuthoringKind kind,
    QTabWidget* preferredHost)
{
    if ( !documentEditingAllowed() )
    {
        return false;
    }
    if ( source )
    {
        QTabWidget* const sourceHost = resourceEditorTabsForPage(source);
        if ( !sourceHost )
        {
            return false;
        }
        if ( preferredHost && preferredHost != sourceHost )
        {
            return false;
        }
        preferredHost = sourceHost;
    }
    if ( !preferredHost )
    {
        preferredHost = primaryResourceEditorTabHost(kind);
    }
    QDockWidget* const targetDock = resourceEditorDockForTabs(preferredHost);
    if ( !preferredHost || !targetDock )
    {
        return false;
    }

    const ProjectType expectedType = document_.level().kind;
    const std::filesystem::path expectedPath = currentPath_;
    const std::uint64_t expectedContextGeneration =
        resourceEditorDocumentContextGeneration_;
    QPointer<MainWindow> self(this);
    QPointer<QWidget> sourceGuard(source);
    QPointer<QTabWidget> hostGuard(preferredHost);
    ResourceAuthoringContext context = makeResourceAuthoringContext(kind, preferredHost);
    context.transientProjectOpenTarget = true;
    QWidget* const candidate = ui::resources::createResourceAuthoringPane(
        this,
        kind,
        std::move(context),
        [this](QWidget* changedPage)
        {
            resourceEditorPaneChanged(changedPage);
        });
    QPointer<QWidget> candidateGuard(candidate);
    if ( !candidateGuard )
    {
        return false;
    }
    candidateGuard->setProperty("resource_authoring.transientOpen", true);
    const bool opened =
        ui::resources::resourceAuthoringPaneOpenProjectResourceDirect(candidateGuard.data());
    if ( !self || !candidateGuard )
    {
        return false;
    }
    if ( candidateGuard->property(
            "resource_authoring.activatedExistingSource").toBool() )
    {
        delete candidateGuard.data();
        return true;
    }
    const bool contextStillCurrent = document_.level().kind == expectedType
        && currentPath_ == expectedPath
        && resourceEditorDocumentContextGeneration_ == expectedContextGeneration;
    const bool sourceStillCurrent = !source
        || (sourceGuard && hostGuard
            && resourceEditorTabsForPage(sourceGuard.data()) == hostGuard.data());
    if ( !opened || !contextStillCurrent || !sourceStillCurrent
        || !hostGuard || resourceEditorDockForTabs(hostGuard.data()) != targetDock )
    {
        delete candidateGuard.data();
        return false;
    }
    if ( resourceEditorPages().size() >= kMaximumResourceEditorPanes )
    {
        statusBar()->showMessage(
            QString::fromLatin1(kResourceEditorLimitMessage),
            kResourceEditorLimitMessageMilliseconds);
        delete candidateGuard.data();
        return false;
    }

    candidateGuard->setProperty("resource_authoring.transientOpen", false);
    candidateGuard->setProperty("cogPinned", false);
    hostGuard->addTab(
        candidateGuard.data(),
        ui::resources::resourceAuthoringPaneTitle(candidateGuard.data()));
    ui::cog::updateCogScriptDockTabPresentation(hostGuard.data(), candidateGuard.data());
    ui::cog::normalizeCogScriptTabOrder(hostGuard.data());
    ui::cog::refreshCogScriptDockTabPresentation(hostGuard.data());
    hostGuard->setCurrentWidget(candidateGuard.data());
    targetDock->show();
    targetDock->raise();
    if ( targetDock->isFloating() )
    {
        targetDock->activateWindow();
    }
    resourceEditorPaneChanged(candidateGuard.data());
    return true;
}

bool MainWindow::openLooseResourceInNewTab(
    const ResourceAuthoringKind kind,
    QTabWidget* preferredHost)
{
    if ( !documentEditingAllowed() )
    {
        return false;
    }
    if ( !preferredHost )
    {
        preferredHost = activeResourceEditorTabHost();
    }
    QDockWidget* const targetDock = resourceEditorDockForTabs(preferredHost);
    if ( !preferredHost || !targetDock )
    {
        return false;
    }

    const ProjectType expectedType = document_.level().kind;
    const std::filesystem::path expectedPath = currentPath_;
    const std::uint64_t expectedContextGeneration =
        resourceEditorDocumentContextGeneration_;
    QPointer<MainWindow> self(this);
    QPointer<QTabWidget> hostGuard(preferredHost);
    ResourceAuthoringContext context = makeResourceAuthoringContext(kind, preferredHost);
    QWidget* const candidate = ui::resources::createResourceAuthoringPane(
        this,
        kind,
        std::move(context),
        [this](QWidget* changedPage)
        {
            resourceEditorPaneChanged(changedPage);
        });
    QPointer<QWidget> candidateGuard(candidate);
    if ( !candidateGuard )
    {
        return false;
    }
    candidateGuard->setProperty("resource_authoring.transientOpen", true);
    const bool opened = ui::resources::resourceAuthoringPaneOpenLooseFileDirect(
        candidateGuard.data());
    if ( !self || !candidateGuard )
    {
        return false;
    }
    if ( candidateGuard->property(
            "resource_authoring.activatedExistingSource").toBool() )
    {
        delete candidateGuard.data();
        return true;
    }
    const bool contextStillCurrent = document_.level().kind == expectedType
        && currentPath_ == expectedPath
        && resourceEditorDocumentContextGeneration_ == expectedContextGeneration;
    if ( !opened || !contextStillCurrent || !hostGuard
        || resourceEditorDockForTabs(hostGuard.data()) != targetDock )
    {
        delete candidateGuard.data();
        return false;
    }
    if ( resourceEditorPages().size() >= kMaximumResourceEditorPanes )
    {
        statusBar()->showMessage(
            QString::fromLatin1(kResourceEditorLimitMessage),
            kResourceEditorLimitMessageMilliseconds);
        delete candidateGuard.data();
        return false;
    }

    candidateGuard->setProperty("resource_authoring.transientOpen", false);
    candidateGuard->setProperty("cogPinned", false);
    hostGuard->addTab(
        candidateGuard.data(),
        ui::resources::resourceAuthoringPaneTitle(candidateGuard.data()));
    ui::cog::updateCogScriptDockTabPresentation(hostGuard.data(), candidateGuard.data());
    ui::cog::normalizeCogScriptTabOrder(hostGuard.data());
    ui::cog::refreshCogScriptDockTabPresentation(hostGuard.data());
    hostGuard->setCurrentWidget(candidateGuard.data());
    targetDock->show();
    targetDock->raise();
    if ( targetDock->isFloating() )
    {
        targetDock->activateWindow();
    }
    resourceEditorPaneChanged(candidateGuard.data());
    return true;
}

void MainWindow::resourceEditorPaneChanged(QWidget* page)
{
    if ( recoveryShuttingDown_ )
    {
        return;
    }
    if ( page && !resourceEditorTabsForPage(page) )
    {
        return;
    }
    if ( page )
    {
        const bool opened = ui::resources::resourceAuthoringPaneTakeSuccessfulOpenNotification(page);
        const bool modified = ui::resources::resourceAuthoringPaneIsModified(page);
        const QString identity = ui::resources::resourceAuthoringPaneIdentity(page);
        const QVariant previousModified = page->property(kResourceNotifiedModifiedProperty);
        const QVariant previousIdentity = page->property(kResourceNotifiedIdentityProperty);
        const bool modifiedChanged = previousModified.isValid() && previousModified.toBool() != modified;
        const bool identityChanged = previousIdentity.isValid() && previousIdentity.toString() != identity;
        page->setProperty(kResourceNotifiedModifiedProperty, modified);
        page->setProperty(kResourceNotifiedIdentityProperty, identity);
        // A clean source reassociation or return to the saved cursor changes
        // authored workspace state too. Clean search, Loop and expansion
        // notifications retain the generation without capturing their rows.
        if ( modified || modifiedChanged || identityChanged || opened )
        {
            ++resourceEditorAuthoredGeneration_;
        }
        if ( opened )
        {
            setResourceNativeBaselineState(
                page,
                ui::resources::captureResourceAuthoringPaneState(page, false));
            (void)resourceEditorAuthoredStateMatchesSavedBaseline();
        }
        refreshResourceEditorTab(page);
    }
    refreshResourceGroupToolbars(resourceEditorTabHosts_, resourceEditorDocumentContextGeneration_);
    syncCogScriptEditorActionFocus();
    noteRecoveryEditorStateChangedIfNeeded();
    refreshWindowTitle();
}

void MainWindow::refreshResourceEditorTab(QWidget* page)
{
    QTabWidget* const tabs = resourceEditorTabsForPage(page);
    if (!page || !tabs || tabs->indexOf(page) < 0)
    {
        return;
    }
    if ( !page->property(kResourceNotifiedModifiedProperty).isValid() )
    {
        page->setProperty(kResourceNotifiedModifiedProperty,
            ui::resources::resourceAuthoringPaneIsModified(page));
        page->setProperty(kResourceNotifiedIdentityProperty,
            ui::resources::resourceAuthoringPaneIdentity(page));
    }
    QWidget* tools = page->findChild<QWidget*>(QStringLiteral("resource_authoring.document_tools"));
    if ( !tools )
    {
        auto* const layout = qobject_cast<QVBoxLayout*>(page->layout());
        if ( layout )
        {
            tools = new QWidget(page);
            tools->setObjectName(QStringLiteral("resource_authoring.document_tools"));
            auto* const toolsLayout = new QHBoxLayout(tools);
            toolsLayout->setContentsMargins(0, 0, 0, 0);
            toolsLayout->setSpacing(4);
            const QPointer<QWidget> guardedPage(page);
            for ( const bool redo : { false, true } )
            {
                auto* const button = new QToolButton(tools);
                button->setObjectName(redo ? QStringLiteral("resource_authoring.redo")
                    : QStringLiteral("resource_authoring.undo"));
                ui::configureDocumentHistoryButton(button, redo ? redoAction_ : undoAction_, redo);
                toolsLayout->addWidget(button);
                connect(button, &QToolButton::clicked, this, [guardedPage, redo]()
                    {
                        if ( guardedPage )
                        {
                            (void)ui::resources::resourceAuthoringPaneDocumentUndoRedo(guardedPage.data(), redo);
                        }
                    });
            }
            toolsLayout->addStretch(1);
            layout->insertWidget(0, tools);
        }
    }
    if ( tools )
    {
        for ( const bool redo : { false, true } )
        {
            QToolButton* const button = tools->findChild<QToolButton*>(redo
                ? QStringLiteral("resource_authoring.redo")
                : QStringLiteral("resource_authoring.undo"));
            if ( button )
            {
                button->setEnabled(ui::resources::resourceAuthoringPaneCanUndoRedo(page, redo));
            }
        }
    }
    QString title = ui::resources::resourceAuthoringPaneTitle(page);
    const QString loosePath = ui::resources::resourceAuthoringPaneLooseFilePath(page);
    if (!loosePath.isEmpty())
    {
        page->setProperty("cogFullPath", loosePath);
        page->setProperty("cogLooseFile", true);
        title = QFileInfo(loosePath).fileName();
    }
    else
    {
        page->setProperty("cogFullPath", resourceTabPath(page));
        page->setProperty("cogLooseFile", false);
    }
    if (ui::resources::resourceAuthoringPaneIsModified(page))
    {
        title += QStringLiteral(" *");
    }
    ui::cog::updateCogScriptDockTabPresentation(tabs, page, title);
    ui::cog::refreshCogScriptDockTabPresentation(tabs);
}

bool MainWindow::confirmCloseResourceEditors(bool* discardedAny)
{
    if (discardedAny)
    {
        *discardedAny = false;
    }
    QPointer<MainWindow> self(this);
    std::vector<QPointer<QTabWidget>> hosts{};
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( host.tabs && host.sessionGeneration == resourceEditorDocumentContextGeneration_ )
        {
            hosts.emplace_back(host.tabs);
        }
    }
    for ( const QPointer<QTabWidget>& host : hosts )
    {
        if ( !self || !host )
        {
            return false;
        }
        int index = 0;
        while ( index < host->count() )
        {
            QPointer<QWidget> target(host->widget(index));
            if ( !target || !ui::resources::resourceAuthoringPaneIsModified(target.data()) )
            {
                ++index;
                continue;
            }

            std::vector<QPointer<QWidget>> order{};
            std::vector<std::pair<QPointer<QWidget>, QCborMap>> beforePages{};
            order.reserve(static_cast<std::size_t>(host->count()));
            for ( int other = 0; other < host->count(); ++other )
            {
                QWidget* const page = host->widget(other);
                order.emplace_back(page);
                if ( page && page != target.data() )
                {
                    beforePages.emplace_back(
                        QPointer<QWidget>(page),
                        resourceAuthoredTabProjection(
                            ui::resources::captureResourceAuthoringPaneState(page, false)));
                }
            }
            const QCborMap beforeTarget =
                ui::resources::captureResourceAuthoringPaneState(target.data(), false);
            bool discarded = false;
            if ( !ui::resources::resourceAuthoringPaneConfirmClose(
                    target.data(), &discarded) )
            {
                return false;
            }
            if ( !self || !host || !target
                || self->resourceEditorTabsForPage(target.data()) != host.data()
                || host->count() != static_cast<int>(order.size()) )
            {
                return false;
            }
            for ( int current = 0; current < host->count(); ++current )
            {
                if ( !order[static_cast<std::size_t>(current)]
                    || host->widget(current) != order[static_cast<std::size_t>(current)].data() )
                {
                    return false;
                }
            }
            for ( const auto& [page, state] : beforePages )
            {
                if ( !page || host->indexOf(page.data()) < 0
                    || resourceAuthoredTabProjection(
                           ui::resources::captureResourceAuthoringPaneState(page.data(), false))
                        != state )
                {
                    return false;
                }
            }
            const QCborMap afterTarget =
                ui::resources::captureResourceAuthoringPaneState(target.data(), false);
            if ( resourceContentProjection(beforeTarget)
                != resourceContentProjection(afterTarget) )
            {
                return false;
            }
            if ( discarded && discardedAny )
            {
                *discardedAny = true;
            }
            ++index;
        }
    }
    return true;
}

void MainWindow::closeResourceEditorsAfterReplacement()
{
    ++resourceEditorDocumentContextGeneration_;
    for ( ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        QPointer<QTabWidget> tabs(host.tabs);
        QPointer<QDockWidget> dock(host.dock);
        const bool primary = host.primary;
        if ( tabs )
        {
            while ( tabs->count() > 0 )
            {
                QWidget* const page = tabs->widget(0);
                if ( ui::resources::resourceAuthoringPaneIsModified(page) )
                {
                    ++resourceEditorAuthoredGeneration_;
                }
                tabs->removeTab(0);
                ui::resources::closeResourceAuthoringPaneAfterReplacement(page);
            }
        }
        if ( dock )
        {
            dock->hide();
            if ( !primary )
            {
                dock->deleteLater();
            }
        }
        if ( primary )
        {
            host.sessionGeneration = resourceEditorDocumentContextGeneration_;
        }
        else
        {
            host.tabs.clear();
            host.dock.clear();
        }
    }
    resourceEditorTabHosts_.erase(
        std::remove_if(
            resourceEditorTabHosts_.begin(),
            resourceEditorTabHosts_.end(),
            [](const ResourceEditorTabHost& host)
            {
                return !host.primary || !host.tabs || !host.dock;
            }),
        resourceEditorTabHosts_.end());
    if ( resourceEditorsDock_ )
    {
        resourceEditorsDock_->hide();
    }
    // Open commits COG closure before this shared context advances, while New
    // does it afterward. Keep the retained empty COG primary current in both paths.
    for ( CogScriptEditorTabHost& host : cogScriptEditorTabHosts_ )
    {
        if ( host.primary )
        {
            host.sessionGeneration = resourceEditorDocumentContextGeneration_;
        }
    }
    observeCogScriptEditorRecoveryHosts();
    refreshResourceGroupToolbars(resourceEditorTabHosts_, resourceEditorDocumentContextGeneration_);
}

bool MainWindow::hasUnsavedResourceEditorChanges() const
{
    for ( const QPointer<QWidget>& page : resourceEditorPages() )
    {
        if ( page && ui::resources::resourceAuthoringPaneIsModified(page.data()) )
        {
            return true;
        }
    }
    return false;
}

bool MainWindow::triggerActiveResourceEditorSave(const bool saveAs)
{
    if ( !documentEditingAllowed() )
    {
        return false;
    }
    if (QWidget* const focus = QApplication::focusWidget())
    {
        for (QWidget* current = focus; current; current = current->parentWidget())
        {
            for ( const QPointer<QWidget>& page : resourceEditorPages() )
            {
                if ( page && (page.data() == current || page->isAncestorOf(focus)) )
                {
                    return ui::resources::resourceAuthoringPaneSave(page.data(), saveAs);
                }
            }
        }
    }
    QTabWidget* const host = activeResourceEditorTabHost();
    QWidget* const page = host ? host->currentWidget() : nullptr;
    return page && ui::resources::resourceAuthoringPaneSave(page, saveAs);
}

bool MainWindow::resourceEditorOwnsFocus() const
{
    const QWidget* focus = QApplication::focusWidget();
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( host.tabs && host.dock && host.sessionGeneration == resourceEditorDocumentContextGeneration_
            && ((focus && (host.dock.data() == focus || host.dock->isAncestorOf(focus)))
                || (host.dock->isFloating() && QApplication::activeWindow() == host.dock.data())) )
        {
            return true;
        }
    }
    return false;
}

bool MainWindow::triggerActiveResourceEditorUndoRedo(const bool redo)
{
    QWidget* const focus = QApplication::focusWidget();
    if ( auto* const edit = qobject_cast<QLineEdit*>(focus) )
    {
        if ( redo )
        {
            edit->redo();
        }
        else
        {
            edit->undo();
        }
        return true;
    }
    if ( auto* const edit = qobject_cast<QPlainTextEdit*>(focus) )
    {
        if ( redo )
        {
            edit->redo();
        }
        else
        {
            edit->undo();
        }
        return true;
    }
    QTabWidget* const tabs = activeResourceEditorTabHost();
    QWidget* const page = tabs ? tabs->currentWidget() : nullptr;
    return page && ui::resources::resourceAuthoringPaneUndoRedo(page, redo);
}

QCborMap MainWindow::captureResourceEditorRecoveryState() const
{
    QCborMap payload{};
    payload.insert(QStringLiteral("version"), 1);
    QCborMap groups{};
    QCborArray tabs{};
    QCborArray hosts{};
    qint64 activeIndex = -1;
    qint64 fallbackActiveIndex = -1;
    QTabWidget* const activeTabs = activeResourceEditorTabHost();
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( !host.tabs || !host.dock
            || (host.sessionGeneration != resourceEditorDocumentContextGeneration_
                && !(host.primary && host.tabs->count() == 0)) )
        {
            continue;
        }
        if ( host.primary )
        {
            QCborMap group{};
            group.insert(QStringLiteral("visible"), !host.dock->isHidden());
            group.insert(QStringLiteral("floating"), host.dock->isFloating());
            group.insert(QStringLiteral("geometry"), host.dock->saveGeometry());
            group.insert(QStringLiteral("active"), host.tabs->currentIndex());
            groups.insert(resourceGroupKey(host.kind), group);
        }
        if ( host.tabs->count() == 0 )
        {
            continue;
        }
        const qsizetype hostIndex = hosts.size();
        QCborMap hostRecord{};
        hostRecord.insert(QStringLiteral("primary"), host.primary);
        hostRecord.insert(QStringLiteral("kind"), static_cast<int>(host.kind));
        hostRecord.insert(QStringLiteral("id"), host.stableId);
        hostRecord.insert(QStringLiteral("visible"), !host.dock->isHidden());
        hostRecord.insert(QStringLiteral("floating"), host.dock->isFloating());
        hostRecord.insert(QStringLiteral("geometry"), host.dock->saveGeometry());
        hostRecord.insert(QStringLiteral("active"), host.tabs->currentIndex());
        QCborArray hostTabs{};
        for ( int index = 0; index < host.tabs->count(); ++index )
        {
            QWidget* const page = host.tabs->widget(index);
            if ( !page )
            {
                continue;
            }
            QCborMap record{};
            record.insert(QStringLiteral("page"),
                ui::resources::captureResourceAuthoringPaneState(page));
            record.insert(QStringLiteral("pinned"), page->property("cogPinned").toBool());
            record.insert(QStringLiteral("nativeBaselineHash"),
                resourceNativeBaselineHash(page));
            record.insert(QStringLiteral("host"), hostIndex);
            hostTabs.append(tabs.size());
            tabs.append(record);
            if ( host.tabs->currentWidget() == page )
            {
                const qint64 globalIndex = tabs.size() - 1;
                if ( fallbackActiveIndex < 0 )
                {
                    fallbackActiveIndex = globalIndex;
                }
                if ( host.tabs.data() == activeTabs )
                {
                    activeIndex = globalIndex;
                }
            }
        }
        hostRecord.insert(QStringLiteral("tabs"), hostTabs);
        hosts.append(hostRecord);
    }
    if ( activeIndex < 0 )
    {
        activeIndex = fallbackActiveIndex;
    }
    payload.insert(QStringLiteral("active"), activeIndex);
    payload.insert(QStringLiteral("dockVisible"), resourceEditorsDock_
        && !resourceEditorsDock_->isHidden());
    payload.insert(QStringLiteral("dockGeometry"), resourceEditorsDock_
        ? resourceEditorsDock_->saveGeometry() : QByteArray{});
    payload.insert(QStringLiteral("tabs"), tabs);
    payload.insert(QStringLiteral("hosts"), hosts);
    payload.insert(QStringLiteral("groups"), groups);
    return payload;
}

QByteArray MainWindow::captureAuthoredResourceEditorRecoveryState() const
{
    QCborMap authored{};
    authored.insert(QStringLiteral("version"), 1);
    QCborMap tabs{};
    for ( const QPointer<QWidget>& page : resourceEditorPages() )
    {
        if ( !page )
        {
            continue;
        }
        const QCborMap pageState =
            ui::resources::captureResourceAuthoringPaneState(page.data(), false);
        const QByteArray baselineHash = resourceNativeBaselineHash(page.data());
        if ( (!baselineHash.isEmpty()
                && resourceAuthoredPageHash(pageState) == baselineHash)
            || (baselineHash.isEmpty()
                && !ui::resources::resourceAuthoringPaneIsModified(page.data())) )
        {
            continue;
        }
        QCborMap record{};
        const QString identity = resourceIdentityFromState(pageState);
        record.insert(QStringLiteral("identity"), identity);
        record.insert(QStringLiteral("page"), resourceAuthoredTabProjection(pageState));
        tabs.insert(identity, record);
    }
    authored.insert(QStringLiteral("tabs"), tabs);
    return QCborValue(authored).toCbor();
}

bool MainWindow::resourceEditorAuthoredStateMatchesSavedBaseline() const
{
    if ( resourceEditorAuthoredGeneration_ == recoverySavedResourceEditorGeneration_ )
    {
        return true;
    }
    if (recoverySavedResourceEditorStateHash_.isEmpty())
    {
        return resourceEditorAuthoredGeneration_
            == recoverySavedResourceEditorGeneration_;
    }

    try
    {
        const QByteArray currentState = captureAuthoredResourceEditorRecoveryState();
        const QByteArray currentHash = QCryptographicHash::hash(
            currentState, QCryptographicHash::Sha256);
        if (currentHash != recoverySavedResourceEditorStateHash_)
        {
            return false;
        }
        recoverySavedResourceEditorGeneration_ = resourceEditorAuthoredGeneration_;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool MainWindow::validateResourceEditorRecoveryState(
    const QCborMap& payload,
    QString* warning) const
{
    const auto fail = [warning](const QString& message)
    {
        if (warning)
        {
            *warning = message;
        }
        return false;
    };
    const QCborValue version = payload.value(QStringLiteral("version"));
    const QCborValue active = payload.value(QStringLiteral("active"));
    const QCborValue visible = payload.value(QStringLiteral("dockVisible"));
    const QCborValue geometry = payload.value(QStringLiteral("dockGeometry"));
    const QCborValue tabsValue = payload.value(QStringLiteral("tabs"));
    const QCborValue hostsValue = payload.value(QStringLiteral("hosts"));
    const bool grouped = payload.contains(QStringLiteral("groups"));
    const QCborValue groupsValue = payload.value(QStringLiteral("groups"));
    if (!version.isInteger() || version.toInteger() != 1
        || !active.isInteger() || !visible.isBool()
        || !geometry.isByteArray() || geometry.toByteArray().size() > 1024 * 1024
        || !tabsValue.isArray() || tabsValue.toArray().size() > 64
        || (grouped && (!groupsValue.isMap() || !payload.contains(QStringLiteral("hosts"))))
        || (payload.contains(QStringLiteral("hosts"))
            && (!hostsValue.isArray() || hostsValue.toArray().size() > 64)))
    {
        return fail(QStringLiteral("Resource editor workspace metadata is invalid."));
    }
    const QCborArray tabs = tabsValue.toArray();
    const QCborArray hosts = hostsValue.toArray();
    if ( grouped )
    {
        const QCborMap groups = groupsValue.toMap();
        for ( const ResourceAuthoringKind kind : { ResourceAuthoringKind::sprite,
                  ResourceAuthoringKind::soundClass, ResourceAuthoringKind::stringTable } )
        {
            const QCborValue groupValue = groups.value(resourceGroupKey(kind));
            const QCborMap group = groupValue.toMap();
            const QCborValue groupActive = group.value(QStringLiteral("active"));
            const QCborValue groupGeometry = group.value(QStringLiteral("geometry"));
            if ( !groupValue.isMap() || !group.value(QStringLiteral("visible")).isBool()
                || !group.value(QStringLiteral("floating")).isBool()
                || !groupGeometry.isByteArray() || groupGeometry.toByteArray().size() > 1024 * 1024
                || !groupActive.isInteger() || groupActive.toInteger() < -1 || groupActive.toInteger() >= 64 )
            {
                return fail(QStringLiteral("Resource editor group presentation is invalid."));
            }
        }
        const QCborMap sprite = groups.value(QStringLiteral("sprite")).toMap();
        if ( sprite.value(QStringLiteral("visible")) != visible
            || sprite.value(QStringLiteral("geometry")) != geometry )
        {
            return fail(QStringLiteral("Sprite editor group metadata is inconsistent."));
        }
    }
    if ((tabs.isEmpty() && active.toInteger() != -1)
        || (!tabs.isEmpty() && (active.toInteger() < 0 || active.toInteger() >= tabs.size())))
    {
        return fail(QStringLiteral("Resource editor active tab is out of range."));
    }

    std::vector<int> hostForTab(static_cast<std::size_t>(tabs.size()), -1);
    if (payload.contains(QStringLiteral("hosts")))
    {
        if (hosts.isEmpty() && (!grouped || !tabs.isEmpty()))
        {
            return fail(QStringLiteral("Resource editor host list is empty."));
        }
        int primaryCount = 0;
        QSet<QString> hostIds{};
        QSet<qint64> primaryKinds{};
        for (qsizetype hostIndex = 0; hostIndex < hosts.size(); ++hostIndex)
        {
            const QCborValue hostValue = hosts.at(hostIndex);
            if (!hostValue.isMap())
            {
                return fail(QStringLiteral("Resource editor host %1 is invalid.").arg(hostIndex + 1));
            }
            const QCborMap host = hostValue.toMap();
            const QCborValue primary = host.value(QStringLiteral("primary"));
            const QCborValue hostId = host.value(QStringLiteral("id"));
            const QCborValue hostVisible = host.value(QStringLiteral("visible"));
            const QCborValue floating = host.value(QStringLiteral("floating"));
            const QCborValue hostGeometry = host.value(QStringLiteral("geometry"));
            const QCborValue hostActive = host.value(QStringLiteral("active"));
            const QCborValue hostTabsValue = host.value(QStringLiteral("tabs"));
            const QCborValue kindValue = host.value(QStringLiteral("kind"));
            if (!primary.isBool() || !hostId.isString()
                || !validResourceHostId(hostId.toString())
                || hostIds.contains(hostId.toString())
                || !hostVisible.isBool() || !floating.isBool()
                || !hostGeometry.isByteArray() || hostGeometry.toByteArray().size() > 1024 * 1024
                || !hostActive.isInteger() || !hostTabsValue.isArray()
                || hostTabsValue.toArray().size() > tabs.size()
                || (grouped && !validResourceKind(kindValue)))
            {
                return fail(QStringLiteral("Resource editor host %1 has invalid fields.")
                    .arg(hostIndex + 1));
            }
            hostIds.insert(hostId.toString());
            if (primary.toBool())
            {
                ++primaryCount;
                if ( grouped )
                {
                    const auto kind = static_cast<ResourceAuthoringKind>(kindValue.toInteger());
                    if ( primaryKinds.contains(kindValue.toInteger())
                        || hostId.toString() != resourcePrimaryHostId(kind) )
                    {
                        return fail(QStringLiteral("A primary resource editor group is duplicated or has an invalid ID."));
                    }
                    primaryKinds.insert(kindValue.toInteger());
                    const QCborMap group = groupsValue.toMap().value(resourceGroupKey(kind)).toMap();
                    if ( group.value(QStringLiteral("visible")) != hostVisible
                        || group.value(QStringLiteral("floating")) != floating
                        || group.value(QStringLiteral("geometry")) != hostGeometry
                        || group.value(QStringLiteral("active")) != hostActive )
                    {
                        return fail(QStringLiteral("Resource editor group and host metadata are inconsistent."));
                    }
                }
                else if (hostIndex != 0 || hostId.toString() != QStringLiteral("primary"))
                {
                    return fail(QStringLiteral("The primary resource editor host must be first."));
                }
            }
            else if (hostId.toString() == QStringLiteral("primary")
                || (grouped && (hostId.toString() == QStringLiteral("primarySoundClass")
                    || hostId.toString() == QStringLiteral("primaryUni"))))
            {
                return fail(QStringLiteral("A detached resource editor host has a reserved ID."));
            }
            const QCborArray hostTabs = hostTabsValue.toArray();
            if ((grouped || hostIndex > 0) && hostTabs.isEmpty())
            {
                return fail(QStringLiteral("Resource editor host %1 has no tabs.")
                        .arg(hostIndex + 1));
            }
            bool sawUnpinnedTab = false;
            if ((hostTabs.isEmpty() && hostActive.toInteger() != -1)
                || (!hostTabs.isEmpty()
                    && (hostActive.toInteger() < 0
                        || hostActive.toInteger() >= hostTabs.size())))
            {
                return fail(QStringLiteral("Resource editor host %1 active tab is out of range.")
                    .arg(hostIndex + 1));
            }
            for (const QCborValue& tabIndexValue : hostTabs)
            {
                if (!tabIndexValue.isInteger() || tabIndexValue.toInteger() < 0
                    || tabIndexValue.toInteger() >= tabs.size())
                {
                    return fail(QStringLiteral("Resource editor host %1 has an invalid tab index.")
                        .arg(hostIndex + 1));
                }
                const std::size_t tabIndex = static_cast<std::size_t>(tabIndexValue.toInteger());
                if (hostForTab[tabIndex] >= 0)
                {
                    return fail(QStringLiteral("A resource editor tab belongs to multiple hosts."));
                }
                hostForTab[tabIndex] = static_cast<int>(hostIndex);
                const QCborValue tabRecordValue = tabs.at(static_cast<qsizetype>(tabIndex));
                if (tabRecordValue.isMap())
                {
                    if ( grouped && tabRecordValue.toMap().value(QStringLiteral("page")).toMap()
                            .value(QStringLiteral("kind")) != kindValue )
                    {
                        return fail(QStringLiteral("A resource editor host contains a different resource kind."));
                    }
                    const QCborValue pinned = tabRecordValue.toMap()
                        .value(QStringLiteral("pinned"));
                    if (pinned.isBool())
                    {
                        if (!pinned.toBool())
                        {
                            sawUnpinnedTab = true;
                        }
                        else if (sawUnpinnedTab)
                        {
                            return fail(QStringLiteral(
                                "Pinned resource editor tabs must precede unpinned tabs."));
                        }
                    }
                }
            }
        }
        if (!grouped && primaryCount != 1)
        {
            return fail(QStringLiteral("Resource editor workspace must contain one primary host."));
        }
        if ( grouped )
        {
            for ( const ResourceAuthoringKind kind : { ResourceAuthoringKind::sprite,
                      ResourceAuthoringKind::soundClass, ResourceAuthoringKind::stringTable } )
            {
                if ( !primaryKinds.contains(static_cast<int>(kind))
                    && groupsValue.toMap().value(resourceGroupKey(kind)).toMap()
                            .value(QStringLiteral("active")).toInteger() != -1 )
                {
                    return fail(QStringLiteral("An empty resource editor group has an active file."));
                }
            }
        }
        if (std::any_of(hostForTab.begin(), hostForTab.end(), [](const int hostIndex)
            {
                return hostIndex < 0;
            }))
        {
            return fail(QStringLiteral("A resource editor tab is missing its host."));
        }

        const QCborMap primaryHost = hosts.isEmpty() ? QCborMap{} : hosts.at(0).toMap();
        if (!grouped && (primaryHost.value(QStringLiteral("visible")) != visible
            || primaryHost.value(QStringLiteral("geometry")) != geometry))
        {
            return fail(QStringLiteral("Primary resource editor host metadata is inconsistent."));
        }
        if (active.toInteger() >= 0)
        {
            const int activeHostIndex = hostForTab[static_cast<std::size_t>(active.toInteger())];
            const QCborMap activeHost = hosts.at(activeHostIndex).toMap();
            const QCborArray activeHostTabs = activeHost.value(QStringLiteral("tabs")).toArray();
            const qint64 localActive = activeHost.value(QStringLiteral("active")).toInteger();
            if (localActive < 0 || localActive >= activeHostTabs.size()
                || activeHostTabs.at(static_cast<qsizetype>(localActive)).toInteger()
                    != active.toInteger())
            {
                return fail(QStringLiteral("Resource editor active tab metadata is inconsistent."));
            }
        }
    }

    QSet<QString> identities{};
    for (qsizetype index = 0; index < tabs.size(); ++index)
    {
        const QCborValue tabValue = tabs.at(index);
        if (!tabValue.isMap())
        {
            return fail(QStringLiteral("Resource editor tab %1 is invalid.").arg(index + 1));
        }
        const QCborMap tab = tabValue.toMap();
        const QCborValue page = tab.value(QStringLiteral("page"));
        const QCborValue pinned = tab.value(QStringLiteral("pinned"));
        const QCborValue hostIndex = tab.value(QStringLiteral("host"));
        const QCborValue nativeBaselineHash =
            tab.value(QStringLiteral("nativeBaselineHash"));
        if (!page.isMap() || !pinned.isBool()
            || (payload.contains(QStringLiteral("hosts"))
                && (!hostIndex.isInteger()
                    || hostIndex.toInteger() != hostForTab[static_cast<std::size_t>(index)]))
            || (tab.contains(QStringLiteral("nativeBaselineHash"))
                && (!nativeBaselineHash.isByteArray()
                    || (nativeBaselineHash.toByteArray().size() != 0
                        && nativeBaselineHash.toByteArray().size() != 32))))
        {
            return fail(QStringLiteral("Resource editor tab %1 has invalid fields.").arg(index + 1));
        }
        QString pageWarning{};
        const QCborMap state = page.toMap();
        if (!ui::resources::validateResourceAuthoringPaneState(state, &pageWarning))
        {
            return fail(QStringLiteral("Resource editor tab %1: %2")
                .arg(index + 1).arg(pageWarning));
        }
        const auto kind = static_cast<ResourceAuthoringKind>(
            state.value(QStringLiteral("kind")).toInteger());
        const qint64 profile = state.value(QStringLiteral("profile")).toInteger();
        const qint64 expectedProjectProfile = document_.level().kind == ProjectType::IJIM ? 1 : 0;
        if ((kind == ResourceAuthoringKind::soundClass
                || kind == ResourceAuthoringKind::stringTable)
            && profile != expectedProjectProfile)
        {
            return fail(QStringLiteral(
                "Resource editor tab %1 uses a profile that does not match the active project.")
                    .arg(index + 1));
        }
        const QString identity = resourceIdentityFromState(state);
        if (identity.endsWith(QLatin1Char(':')) || identities.contains(identity))
        {
            return fail(QStringLiteral("Resource editor tab %1 has a duplicate or missing identity.")
                .arg(index + 1));
        }
        identities.insert(identity);
    }
    return true;
}

bool MainWindow::restoreResourceEditorRecoveryState(
    const QCborMap& payload,
    QStringList* warnings)
{
    QString validationWarning{};
    if ( !validateResourceEditorRecoveryState(payload, &validationWarning) )
    {
        if ( warnings )
        {
            warnings->push_back(validationWarning);
        }
        return false;
    }
    const QCborMap normalized = normalizeResourceEditorRecoveryState(payload);
    if ( !validateResourceEditorRecoveryState(normalized, &validationWarning) )
    {
        if ( warnings )
        {
            warnings->push_back(validationWarning);
        }
        return false;
    }
    const QCborArray records = normalized.value(QStringLiteral("tabs")).toArray();
    const QCborArray savedHosts = normalized.value(QStringLiteral("hosts")).toArray();
    std::vector<QPointer<QWidget>> staged{};
    staged.reserve(static_cast<std::size_t>(records.size()));
    const auto discardStaged = [&staged]()
    {
        for ( const QPointer<QWidget>& page : staged )
        {
            delete page.data();
        }
    };
    for ( const QCborValue& recordValue : records )
    {
        const QCborMap record = recordValue.toMap();
        const QCborMap state = record.value(QStringLiteral("page")).toMap();
        const auto kind = static_cast<ResourceAuthoringKind>(state.value(QStringLiteral("kind")).toInteger());
        QTabWidget* const primary = primaryResourceEditorTabHost(kind);
        if ( !primary )
        {
            discardStaged();
            return false;
        }
        QWidget* const page = ui::resources::createResourceAuthoringPane(
            this, kind, makeResourceAuthoringContext(kind, primary), [this](QWidget* changed)
            {
                resourceEditorPaneChanged(changed);
            });
        staged.emplace_back(page);
        if ( !page || !ui::resources::restoreResourceAuthoringPaneState(page, state, &validationWarning) )
        {
            if ( warnings )
            {
                warnings->push_back(validationWarning);
            }
            discardStaged();
            return false;
        }
        page->setProperty("cogPinned", record.value(QStringLiteral("pinned")).toBool());
        if ( record.contains(QStringLiteral("nativeBaselineHash")) )
        {
            page->setProperty(kResourceNativeBaselineStateProperty,
                record.value(QStringLiteral("nativeBaselineHash")).toByteArray());
        }
    }
    if ( !prepareResourceEditorRecoveryHosts(normalized) )
    {
        if ( warnings )
        {
            warnings->push_back(QStringLiteral("Resource editor host windows are unavailable."));
        }
        discardStaged();
        return false;
    }
    std::vector<QPointer<QTabWidget>> targets{};
    QSet<QString> desiredIds{};
    for ( const QCborValue& hostValue : savedHosts )
    {
        const QString id = hostValue.toMap().value(QStringLiteral("id")).toString();
        QTabWidget* tabs = nullptr;
        for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
        {
            if ( host.stableId == id && host.dock && host.tabs )
            {
                tabs = host.tabs.data();
            }
        }
        if ( !tabs )
        {
            discardStaged();
            return false;
        }
        targets.emplace_back(tabs);
        desiredIds.insert(id);
    }
    const std::vector<ResourceEditorTabHost> previousHosts = resourceEditorTabHosts_;
    for ( const ResourceEditorTabHost& host : previousHosts )
    {
        if ( !host.tabs )
        {
            continue;
        }
        {
            const QSignalBlocker blocker(host.tabs.data());
            while ( host.tabs->count() > 0 )
            {
                QWidget* const page = host.tabs->widget(0);
                host.tabs->removeTab(0);
                ui::resources::closeResourceAuthoringPaneAfterReplacement(page);
            }
        }
        if ( !host.primary && !desiredIds.contains(host.stableId) && host.dock )
        {
            unregisterResourceEditorTabHost(host.tabs.data());
            workspaceWindow_->removeDockWidget(host.dock.data());
            delete host.dock.data();
        }
        else if ( host.dock )
        {
            registerResourceEditorTabHost(host.dock.data(), host.tabs.data(),
                host.primary, host.stableId, host.kind);
        }
    }
    for ( qsizetype hostIndex = 0; hostIndex < savedHosts.size(); ++hostIndex )
    {
        const QCborMap hostState = savedHosts.at(hostIndex).toMap();
        QTabWidget* const tabs = targets[static_cast<std::size_t>(hostIndex)].data();
        const QSignalBlocker blocker(tabs);
        for ( const QCborValue& globalIndex : hostState.value(QStringLiteral("tabs")).toArray() )
        {
            QWidget* const page = staged[static_cast<std::size_t>(globalIndex.toInteger())].data();
            tabs->addTab(page, ui::resources::resourceAuthoringPaneTitle(page));
            refreshResourceEditorTab(page);
            if ( ui::resources::resourceAuthoringPaneIsModified(page) )
            {
                ++resourceEditorAuthoredGeneration_;
            }
        }
        tabs->setCurrentIndex(static_cast<int>(hostState.value(QStringLiteral("active")).toInteger()));
        if ( !hostState.value(QStringLiteral("primary")).toBool() )
        {
            restoreResourceDockPresentation(resourceEditorDockForTabs(tabs), hostState);
        }
        ui::cog::refreshCogScriptDockTabPresentation(tabs);
    }
    const QCborMap groups = normalized.value(QStringLiteral("groups")).toMap();
    for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
    {
        if ( host.primary && host.dock && host.tabs )
        {
            const QCborMap group = groups.value(resourceGroupKey(host.kind)).toMap();
            host.tabs->setCurrentIndex(static_cast<int>(group.value(QStringLiteral("active")).toInteger()));
            restoreResourceDockPresentation(host.dock.data(), group);
        }
    }
    refreshResourceGroupToolbars(resourceEditorTabHosts_, resourceEditorDocumentContextGeneration_);
    const qint64 active = normalized.value(QStringLiteral("active")).toInteger();
    if ( active >= 0 && active < static_cast<qint64>(staged.size()) )
    {
        QWidget* const page = staged[static_cast<std::size_t>(active)].data();
        QTabWidget* const tabs = resourceEditorTabsForPage(page);
        if ( tabs )
        {
            tabs->setCurrentWidget(page);
            for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
            {
                if ( host.tabs.data() == tabs )
                {
                    setProperty("resource_authoring.lastActiveHostId", host.stableId);
                }
            }
        }
    }
    noteRecoveryEditorStateChangedIfNeeded();
    return true;
}

} // namespace sed
