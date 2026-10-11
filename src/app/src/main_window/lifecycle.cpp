#include "main_window/common.hpp"
#include "app_icon.hpp"
#include "ui/audio/app_sound_player.hpp"
#include "ui/components/dock_widget_retirement.hpp"

#include <QShowEvent>

#include <algorithm>
#include <array>
#include <vector>

namespace sed
{

namespace
{

// Version 4 adds stable object names for every outer dock. Earlier state could
// restore Messages but not the unnamed Checks/History docks, producing a
// different order and width on every launch.
constexpr int kMainWindowStateVersion = 4;
constexpr int kWorkspaceStateVersion = 2;
constexpr const char* kMainWindowGeometryKey = "app/main_window_geometry";
constexpr const char* kMainWindowStateKey = "app/main_window_state";
constexpr const char* kWorkspaceStateKey = "app/workspace_state";

Qt::WindowStates launchVisibleState(const Qt::WindowStates state) noexcept
{
    return state & ~Qt::WindowMinimized;
}

bool completeOwnedNativeGeometryAnimations(QWidget* const owner)
{
    const QPointer<QWidget> guardedOwner(owner);
    if ( !guardedOwner )
    {
        return false;
    }
    // A completion can reapply the layout and post another generation of
    // zero-duration Qt animations. Bound the retirement by the live widget
    // topology, while inspecting only exact owner-targeted geometry actors.
    const qsizetype maximumPasses = guardedOwner->findChildren<QWidget*>().size() + 2;
    for ( qsizetype pass = 0; pass < maximumPasses && guardedOwner; ++pass )
    {
        std::vector<QPointer<QPropertyAnimation>> animations{};
        for ( QPropertyAnimation* const animation : guardedOwner->findChildren<QPropertyAnimation*>() )
        {
            auto* const target = qobject_cast<QWidget*>(animation->targetObject());
            if ( target && animation->parent() == target
                && animation->propertyName() == QByteArrayLiteral("geometry") )
            {
                animations.emplace_back(animation);
            }
        }
        if ( animations.empty() )
        {
            return true;
        }
        // Complete native tabbar and central-widget actors before a dock or
        // native group completion can replace those layout-owned widgets.
        const auto nativeDockTarget = [](const QPointer<QPropertyAnimation>& animation)
        {
            auto* const target = animation ? qobject_cast<QWidget*>(animation->targetObject()) : nullptr;
            return target && (qobject_cast<QDockWidget*>(target) || target->isWindow());
        };
        std::stable_sort(animations.begin(), animations.end(),
            [&nativeDockTarget](const auto& first, const auto& second)
            {
                return nativeDockTarget(first) < nativeDockTarget(second);
            });
        for ( const QPointer<QPropertyAnimation>& animation : animations )
        {
            if ( animation )
            {
                animation->stop();
            }
            if ( !guardedOwner )
            {
                return false;
            }
        }
        for ( const QPointer<QPropertyAnimation>& animation : animations )
        {
            if ( animation )
            {
                QCoreApplication::sendPostedEvents(animation.data(), QEvent::DeferredDelete);
            }
            if ( !guardedOwner || animation )
            {
                return false;
            }
        }
    }
    return false;
}

} // namespace

MainWindow::~MainWindow()
{
    // Child focus can change during explicit page deletion and later QObject
    // base teardown, after the document and editor registries cease to exist.
    if ( qApp )
    {
        disconnect(qApp, &QApplication::focusChanged, this, nullptr);
    }

    // QWidget base teardown can deliver GlView focus events after MainWindow's
    // members have been destroyed. Stop Fly here so its synchronous state
    // signal reaches every MainWindow receiver while its members still live.
    if ( glView_ && glView_->flyNavigationActive() )
    {
        glView_->cancelFlyNavigation();
    }

    // GlView owns a watcher callback that captures this MainWindow.  Clear it
    // before member teardown so a queued archive retry can never call into a
    // partially destroyed owner.
    if ( glView_ )
    {
        glView_->setResourceArchiveReloadCallback({});
    }

    // A MainWindow can be destroyed without receiving closeEvent (tests,
    // session teardown, or an embedding host).  Capture the newest live Level
    // and COG buffers before invalidating the publication gate in that path.
    // A deliberate close has already shut recovery down and removed its slot,
    // so this is a no-op there.
    if (!recoveryShuttingDown_)
    {
        // Destruction can bypass closeEvent (tests, embedding hosts, or Qt
        // teardown). Force one final coherent capture even when the last
        // published key looks clean; no later callback can protect an
        // editor-only mutation after this object is gone.
        flushRecoverySnapshot(true);
    }
    shutdownRecoverySupport(false);

    if ( !completeOwnedNativeGeometryAnimations(this) )
    {
        qWarning("Native geometry animation completion did not reach quiescence before window teardown.");
    }

    // Qt 6.10.2 completes dock animation work from each animation's
    // destructor, including animations already stopped by removeDockWidget.
    // Retire those exact queued callbacks while both native layouts and
    // their tab bars still live, after the final user-layout capture.
    const std::array<QPointer<QMainWindow>, 2> nativeLayouts{this, workspaceWindow_};
    for ( const QPointer<QMainWindow>& layoutOwner : nativeLayouts )
    {
        if ( !layoutOwner )
        {
            continue;
        }
        std::vector<QPointer<QDockWidget>> docks{};
        for ( QDockWidget* const dock : layoutOwner->findChildren<QDockWidget*>(
                  QString{}, Qt::FindDirectChildrenOnly) )
        {
            docks.emplace_back(dock);
        }
        for ( const QPointer<QDockWidget>& dock : docks )
        {
            if ( layoutOwner && dock )
            {
                (void)ui::removeDockWidgetAndCompleteAnimations(layoutOwner.data(), dock.data(),
                    [layoutOwner]() { return !layoutOwner.isNull(); });
                if ( !completeOwnedNativeGeometryAnimations(this) )
                {
                    qWarning("Native geometry animation completion did not reach quiescence after dock retirement.");
                }
            }
        }
    }

    // QObject deletes child widgets from its base destructor, after
    // MainWindow's C++ members have already been destroyed.  COG tab
    // destroyed handlers maintain openCogScriptEditors_, so destroy those
    // pages now while the map and recovery state are still alive.  Leaving
    // them for QObject teardown is a use-after-destruction at application
    // shutdown.
    for ( const QPointer<QWidget>& page : cogScriptEditorPages() )
    {
        delete page.data();
    }
    openCogScriptEditors_.clear();

    // Resource pane callbacks and native-baseline properties refer to
    // MainWindow-owned registries. Destroy every page while those members
    // still exist, including pages in floating tab hosts.
    for (const ResourceEditorTabHost& host : resourceEditorTabHosts_)
    {
        if (!host.tabs)
        {
            continue;
        }
        while (host.tabs->count() > 0)
        {
            QWidget* const page = host.tabs->widget(0);
            host.tabs->removeTab(0);
            delete page;
        }
    }
    resourceEditorTabHosts_.clear();

    cancelLightBakeTransformPreviewWorker();
    cancelLightBake(true);
    if (lightBakeTransformPreviewWaitForFinished_)
    {
        lightBakeTransformPreviewWaitForFinished_();
        lightBakeTransformPreviewWaitForFinished_ = {};
    }
    if (lightBakeWaitForFinished_)
    {
        lightBakeWaitForFinished_();
        lightBakeWaitForFinished_ = {};
    }
    ui::audio::AppSoundPlayer::instance().shutdown();
    if (qApp)
    {
        qApp->removeEventFilter(this);
    }
}

void MainWindow::restoreWindowPlacement()
{
    QSettings settings = settingsStore();

    const QByteArray geometry = settings.value(kMainWindowGeometryKey).toByteArray();
    const QByteArray state = settings.value(kMainWindowStateKey).toByteArray();
    const QByteArray workspaceState = settings.value(kWorkspaceStateKey).toByteArray();

    if (!geometry.isEmpty())
    {
        restoreGeometry(geometry);
    }
    if (!state.isEmpty())
    {
        restoreState(state, kMainWindowStateVersion);
    }
    if (workspaceWindow_ && !workspaceState.isEmpty())
    {
        workspaceWindow_->restoreState(workspaceState, kWorkspaceStateVersion);
    }
    if (geometry.isEmpty() && state.isEmpty())
    {
        setWindowState(windowState() | Qt::WindowMaximized);
    }

    // If the previous session was closed while minimized from the taskbar,
    // restore the normal/maximized layout but keep startup visible.
    setWindowState(launchVisibleState(windowState()));
}

void MainWindow::saveWindowPlacement() const
{
    QSettings settings = settingsStore();
    settings.setValue(kMainWindowGeometryKey, saveGeometry());
    settings.setValue(kMainWindowStateKey, saveState(kMainWindowStateVersion));
    if (workspaceWindow_)
    {
        settings.setValue(
            kWorkspaceStateKey,
            workspaceWindow_->saveState(kWorkspaceStateVersion));
    }
    settings.sync();
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);

    // The startup dialog may keep the main viewport hidden beyond the first
    // refit timeout. Showing the editor is the authoritative retry point.
    if (pendingEditorViewRefitAfterResize_ && editorViewRefitTimer_)
    {
        editorViewRefitTimer_->start();
    }

    // Qt can assign its small transparent caption icon while creating the
    // native window. Reapply the contrasting executable icon afterwards so
    // the taskbar and Alt+Tab use the same shell artwork as startup.
    QTimer::singleShot(0, this, [this]()
        {
            applyWindowsTaskbarIcon(*this);
            if (pendingEditorViewRefitAfterResize_ && editorViewRefitTimer_)
            {
                editorViewRefitTimer_->start();
            }
        });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QString deferredDiscardIdentity{};
    QString deferredDiscardSessionId{};
    if (!authorizeDocumentReplacement(
            QStringLiteral("quitting"), {}, &deferredDiscardIdentity,
            &deferredDiscardSessionId))
    {
        if (event)
        {
            event->ignore();
        }
        return;
    }
    if (!replacementAuthorizationStillCurrent())
    {
        clearReplacementAuthorizationFingerprint();
        if (event)
        {
            event->ignore();
        }
        return;
    }
    const auto replacementFingerprintGuard = qScopeGuard(
        [this]() { clearReplacementAuthorizationFingerprint(); });

    // All Level and COG prompts have now succeeded. Commit a deliberate
    // Discard only at this boundary; a cancelled later COG prompt above never
    // deletes the still-open document's sole durable checkpoint/history.
    // A normal close retires only the live crash-offer slot. Periodic
    // `.sedautosave` checkpoints are explicitly user history and must remain
    // browsable after the process exits, even when the user chose Discard for
    // the currently open dirty state.
    if (!deferredDiscardIdentity.isEmpty())
    {
        discardRecoveryLiveFamily(
            deferredDiscardIdentity, deferredDiscardSessionId);
    }

    // Reaching this point is a deliberate, clean close (including an explicit
    // Discard choice). A crash or power loss bypasses closeEvent and leaves the
    // last atomically published snapshot available on the next launch.
    shutdownRecoverySupport(!recoverySystemShutdown_);

    saveWindowPlacement();

    cancelLightBakeTransformPreviewWorker();
    cancelLightBake(true);

    // Persist COG script editor dock width for next session.
    if (cogScriptDockWidthLearned_
        && cogScriptDockPreferredWidth_ >= kMinimumCogScriptDockWidth)
    {
        QSettings settings = settingsStore();
        settings.setValue("cog_editor/dock_width", cogScriptDockPreferredWidth_);
    }

    if (QThreadPool *pool = QThreadPool::globalInstance())
    {
        pool->clear();
        (void)pool->waitForDone(2500);
    }
    thingPlacementModelThumbnailRenderer_.reset();
    thingPlacementTemplatePreviewIconCache_.clear();
    thingPlacementTemplateIconPrepInFlight_.clear();
    thingPlacementTemplateIconActiveJobs_ = 0;
    ui::detail::clearSharedPreviewAssetCaches();

    QMainWindow::closeEvent(event);
}

} // namespace sed
