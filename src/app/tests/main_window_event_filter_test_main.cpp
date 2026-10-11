#include "app_logging.hpp"
#include "app_settings.hpp"
#include "ui/components/preview_worker_pool.hpp"
#include "test_common.hpp"

#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>

#include <atomic>
#include <filesystem>
#include <string>
#include <system_error>

namespace sed::app_tests
{

void runMainWindowEventFilterTests();
void runCleaveOptionsLayoutTests();
void runCleaveUiOwnershipTests();
void runCleaveShapeOverlayTests();
void runCleaveFamilyControlsTests();
void runCleaveChainOriginTests();
void runCleaveChainBodyRecoveryTests();
void runCleaveConnectionHistoryTests();
void runCleaveWorkspacePersistenceTests();
void runCleaveDocumentResetOwnershipTests();
void runCleaveApplyWorkspaceHistoryTests();
void runCleaveAdvancedInteractionTests();
void runCleaveInputRoutingTests();
void runCleaveWheelSessionTests();
void runCleaveUvKeyReleaseTests();
void runCleaveModeChangeTests();
void runCleavePointerOwnershipTests();
void runCleaveCameraPreviewTests();
void runCleaveFlyAndCustomShortcutTests();
void runCleaveUvCustomShortcutOwnershipTests();
void runCleaveShapeApplyTests();
void runMinorGridPointsAppearanceTests();
void runUvGridPointsAppearanceTests();
void runViewportGridComboPopupLifetimeTests();
void runFrameHistorySelectionTests();
void runFrameAddMoveBehaviorTests();
void runFrameRenderPerformanceTests();
void runSurfaceExtraLightHdrTests();
void runEngineExportOptionsTests();
void runMotsResourceOpenTests();
void runCleavePrecisionUiTests();
void runResourcePickerReleaseTests();
void runResourceAuthoringEditorTests();
void runResourceAuthoringPresentationTests();
void runResourceAuthoringHistoryCaretRegressionTests();
void runResourceWorkspaceRegressionTests();
void runResourceAuthoringLifecycleTests();
void runResourceAuthoringSpriteGeometryTests();
void runResourceAuthoringHistoryTests();
void runRuntimeLightingSwitchTests();
void runCogInteractionRegressionTests();
void runCogShortcutContextTests();
void runCogOverwriteLifetimeTests();
void runCogDocumentDockingTests();
void runLooseDocumentWatcherTests();
void runNativeSaveEditValidationTests();
void runNativeCaptionVisibilityTests();
void runTemplateArchiveRefreshTests();
void runTemplateCollisionBoundsPreviewTests();
void runTemplateDuplicateTypeTests();
void runParameterHelperDialogTests();
void runThingCollisionOverlayCacheTests();
void runToastExpiryTimerTests();
void runHoldModifierBindingTests();
void runPriorityMouseReleaseTests();
void runLightModeShortcutTests();
void runSurfaceMultiCutRegressionTests();
void runSurfaceRoundoffMultiCutRegressionTests();
void runLightGizmoSelectionTests();
void runCleaveBoundedRecoveryTests();
void runThingPlacementBehaviorTests();
void runThingPlacementIdExhaustionTests();
void runThingOrthoThinPlacementTests();
void runThingMoveDragHotPathTests();
void runThingTransformPreviewSectorTests();
void runCogScriptLifecycleTests();
void runCogInlineAssignmentTests();
void runProjectResourcePrecisionTests();
void runUvCommandPrecisionTests();
void runThingTransformCommandTests();
void runThingLightTransformHistoryTests();
void runMotsArchLightingDialogTests();
void runMotsArchLightingStagingTests();
void runMotsArchLightingRenderTests();
void runMotsArchLightingPickingTests();
void runMotsArchLightingInvalidationTests();
void runMotsArchLightingWorkspaceRecoveryTests();
void runTranslateMenuContractTests();
void runTransformNumericPrecisionTests();
void runResourceIoBoundaryTests();
void runDesignerUiPrecisionTests();
void runConsistencyPrecisionDialogTests();
void runEditingRejectionUiTests();
void runUvNumericInputPrecisionTests();
void runUvZoomPreferenceTests();
void runUiDecimalPolicyTests();
void runUiDecimalConsumerTests();
void runResourceUiDecimalTests();
void runSectorAmbientPropertyTests();
void runMotsArchBakeFlagTests();
void runMotsArchBakePipelineTests();
void runSurfaceActivePropertyTests();
void runSurfaceSelectionHistoryTests();
void runThingSectorPropertyNavigationTests();
void runThingFindLightingMenuTests();
void runViewportShortcutTests();
void runNavigationPreferencesTests();
void runNavigationShortcutDisplayTests();
void runNavigationShortcutContextTests();
void runViewportThingOverlayStateTests();
void runStrictSurfaceSelectionRecoveryTests();
void runStrictSurfaceContextTopologyTests();
void runCogSymbolMenuOrderingTests();
void runPlacedCogReferencePickerTests();

} // namespace sed::app_tests

namespace
{

std::atomic_int priorMessageHandlerCalls{};

void priorMessageHandlerFixture(
    const QtMsgType,
    const QMessageLogContext&,
    const QString& message)
{
    if (message.startsWith(
            QStringLiteral("Application logger prior-handler sentinel")))
    {
        priorMessageHandlerCalls.fetch_add(1);
    }
}

class ScopedSettingsIsolation final
{
public:
    explicit ScopedSettingsIsolation(const std::filesystem::path& iniPath)
    {
        sed::setAppSettingsIniPathBaselineOverrideForTest(iniPath);
        sed::setAppSettingsIniPathOverrideForTest(iniPath);
        // Pre-create the INI directly, before logging calls settingsStore().
        // A missing INI would allow migration from the developer's registry.
        QSettings settings(QString::fromStdWString(iniPath.wstring()), QSettings::IniFormat);
        settings.setFallbacksEnabled(false);
        settings.clear();
        settings.setValue(QStringLiteral("app/last_project_kind"), QStringLiteral("jkdf2"));
        settings.setValue(QStringLiteral("resources/roots"), QStringList{});
        settings.setValue(QStringLiteral("resources/gobs"), QStringList{});
        for ( const char* const project : { "jkdf2", "mots", "ijim" } )
        {
            const std::filesystem::path installRoot = iniPath.parent_path()
                / (std::string("empty-install-") + project);
            std::error_code directoryError{};
            std::filesystem::create_directories(installRoot, directoryError);
            if ( directoryError )
            {
                return;
            }
            const QString prefix = QStringLiteral("resources/%1/").arg(QString::fromLatin1(project));
            settings.setValue(prefix + QStringLiteral("install_dir"), QString::fromStdWString(installRoot.wstring()));
            settings.setValue(prefix + QStringLiteral("roots"), QStringList{});
            settings.setValue(prefix + QStringLiteral("gobs"), QStringList{});
        }
        settings.sync();
        std::error_code fileError{};
        prepared_ = settings.status() == QSettings::NoError
            && std::filesystem::is_regular_file(iniPath, fileError) && !fileError;
    }

    ~ScopedSettingsIsolation()
    {
        sed::setAppSettingsIniPathOverrideForTest({});
        sed::setAppSettingsIniPathBaselineOverrideForTest({});
    }

    ScopedSettingsIsolation(const ScopedSettingsIsolation&) = delete;
    ScopedSettingsIsolation& operator=(const ScopedSettingsIsolation&) = delete;

    bool prepared() const noexcept
    {
        return prepared_;
    }

private:
    bool prepared_{};
};

} // namespace

int main(int argc, char** argv)
{
    QTemporaryDir settingsDirectory{};
    if ( !settingsDirectory.isValid() )
    {
        return 2;
    }

    const std::filesystem::path settingsPath =
        std::filesystem::path(settingsDirectory.path().toStdWString()) / "sed-main-window-tests.ini";
    const ScopedSettingsIsolation settingsIsolation(settingsPath);
    if ( !settingsIsolation.prepared() )
    {
        return 2;
    }

    // Test the process-wide fallback before constructing QApplication or any
    // production object. Individual fixtures may temporarily clear their
    // narrower override, but they must never fall through to the developer's
    // portable sed.ini during this suite.
    sed::setAppSettingsIniPathOverrideForTest({});
    if ( sed::appSettingsIniPath() != settingsPath )
    {
        return 2;
    }
    sed::setAppSettingsIniPathOverrideForTest(settingsPath);

    const std::filesystem::path expectedRecoveryPath =
        settingsPath.parent_path() / "recovery";
    if ( sed::appRecoveryDirectoryPath() != expectedRecoveryPath )
    {
        return 2;
    }

    // The export-options regression drives the same Qt QFileDialog modal
    // chain used by production from a timer. Keeping that chooser in Qt's
    // widget implementation makes temporary-path selection deterministic on
    // Windows as well as on the offscreen CI platform.
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    qInstallMessageHandler(priorMessageHandlerFixture);
    sed::installApplicationLogging(app);
    {
        QSettings settings = sed::settingsStore();
        bool isolated = settings.value(QStringLiteral("resources/roots")).toStringList().isEmpty()
            && settings.value(QStringLiteral("resources/gobs")).toStringList().isEmpty();
        for ( const char* const project : { "jkdf2", "mots", "ijim" } )
        {
            const QString prefix = QStringLiteral("resources/%1/").arg(QString::fromLatin1(project));
            const std::filesystem::path expectedInstall = settingsPath.parent_path()
                / (std::string("empty-install-") + project);
            isolated = isolated && settings.value(prefix + QStringLiteral("roots")).toStringList().isEmpty()
                && settings.value(prefix + QStringLiteral("gobs")).toStringList().isEmpty()
                && settings.value(prefix + QStringLiteral("install_dir")).toString()
                    == QString::fromStdWString(expectedInstall.wstring());
        }
        sed::app_tests::check(isolated,
            "main-window suite pre-created INI blocks legacy registry migration and starts with only controlled empty installs");
        if ( !isolated )
        {
            return 2;
        }
    }
    qWarning().noquote() << "Application logger prior-handler sentinel";
    if (priorMessageHandlerCalls.load() != 1)
    {
        return 2;
    }
    {
        QSettings settings = sed::settingsStore();
        settings.setValue(QStringLiteral("logging/enabled"), false);
        settings.sync();
        sed::reloadApplicationLogSettings();
        qWarning().noquote()
            << "Application logger prior-handler sentinel master-off";
        if (priorMessageHandlerCalls.load() != 2)
        {
            return 2;
        }

        settings.setValue(QStringLiteral("logging/enabled"), true);
        settings.setValue(
            QStringLiteral("logging/category_application"), false);
        settings.sync();
        sed::reloadApplicationLogSettings();
        qWarning().noquote()
            << "Application logger prior-handler sentinel category-off";
        if (priorMessageHandlerCalls.load() != 3)
        {
            return 2;
        }

        settings.remove(QStringLiteral("logging/enabled"));
        settings.remove(QStringLiteral("logging/category_application"));
        settings.sync();
        sed::reloadApplicationLogSettings();
    }
    // This process intentionally creates and closes many independent top-level
    // modal fixtures before its long-lived MainWindow fixture. A production
    // last-window quit request must not terminate the next nested dialog loop.
    app.setQuitOnLastWindowClosed(false);
    if (app.arguments().contains(QStringLiteral("--native-caption-visibility")))
    {
        sed::app_tests::runNativeCaptionVisibilityTests();
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--cleave-bounded-recovery")) )
    {
        sed::app_tests::runCleaveBoundedRecoveryTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--light-gizmo-selection")) )
    {
        sed::app_tests::runLightGizmoSelectionTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--light-mode-shortcuts")))
    {
        sed::app_tests::runLightModeShortcutTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--surface-multicut-regression")))
    {
        sed::app_tests::runSurfaceMultiCutRegressionTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(
            QStringLiteral("--minor-grid-appearance")) )
    {
        sed::app_tests::runMinorGridPointsAppearanceTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(
            QStringLiteral("--uv-grid-points-appearance")))
    {
        sed::app_tests::runUvGridPointsAppearanceTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(
            QStringLiteral("--viewport-grid-combo-popup-lifetime")))
    {
        sed::app_tests::runViewportGridComboPopupLifetimeTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(
            QStringLiteral("--frame-history-selection")))
    {
        sed::app_tests::runFrameHistorySelectionTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(
            QStringLiteral("--frame-add-move-behavior")))
    {
        sed::app_tests::runFrameAddMoveBehaviorTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(
            QStringLiteral("--frame-render-performance")))
    {
        sed::app_tests::runFrameRenderPerformanceTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--ui-decimal-policy")))
    {
        sed::app_tests::runUiDecimalPolicyTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--ui-decimal-consumers")))
    {
        sed::app_tests::runUiDecimalConsumerTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--resource-ui-decimal")))
    {
        sed::app_tests::runResourceUiDecimalTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--uv-zoom-preferences")))
    {
        sed::app_tests::runUvZoomPreferenceTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--thing-transform-preview-sector")))
    {
        sed::app_tests::runThingTransformPreviewSectorTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--thing-move-hotpath")))
    {
        sed::app_tests::runThingMoveDragHotPathTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--thing-light-history")))
    {
        sed::app_tests::runThingLightTransformHistoryTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--transform-numeric-precision")))
    {
        sed::app_tests::runTransformNumericPrecisionTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--resource-io-boundary")))
    {
        sed::app_tests::runResourceIoBoundaryTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--uv-numeric-precision")))
    {
        sed::app_tests::runUvNumericInputPrecisionTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--mots-arch-lighting")))
    {
        sed::app_tests::runMotsArchLightingRenderTests();
        sed::app_tests::runMotsArchLightingStagingTests();
        sed::app_tests::runMotsArchLightingDialogTests();
        sed::app_tests::runMotsArchLightingPickingTests();
        sed::app_tests::runMotsArchLightingInvalidationTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--parameter-helper-dialogs")))
    {
        sed::app_tests::runParameterHelperDialogTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--cog-menu-instance")))
    {
        sed::app_tests::runCogSymbolMenuOrderingTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--cog-instance-picker")))
    {
        sed::app_tests::runPlacedCogReferencePickerTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--template-duplicate-type")))
    {
        sed::app_tests::runTemplateDuplicateTypeTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--template-collision-preview")))
    {
        sed::app_tests::runTemplateCollisionBoundsPreviewTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--thing-collision-cache")))
    {
        sed::app_tests::runThingCollisionOverlayCacheTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--toast-expiry-timer")))
    {
        sed::app_tests::runToastExpiryTimerTests();
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--hold-modifier-bindings")))
    {
        sed::app_tests::runHoldModifierBindingTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--viewport-window-mouse-release")))
    {
        sed::app_tests::runPriorityMouseReleaseTests();
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--thing-find-lighting-menu")))
    {
        sed::app_tests::runThingFindLightingMenuTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--viewport-shortcuts")))
    {
        sed::app_tests::runViewportShortcutTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--navigation-preferences")))
    {
        sed::app_tests::runNavigationPreferencesTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--navigation-shortcut-context")) )
    {
        sed::app_tests::runNavigationShortcutContextTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--navigation-shortcut-display")))
    {
        sed::app_tests::runNavigationShortcutDisplayTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--surface-roundoff-multicut")))
    {
        sed::app_tests::runSurfaceRoundoffMultiCutRegressionTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--cleave-wheel-session")))
    {
        sed::app_tests::runCleaveWheelSessionTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--cog-script-lifecycle")))
    {
        sed::app_tests::runCogScriptLifecycleTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--cog-inline-assignment")))
    {
        sed::app_tests::runCogInlineAssignmentTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--thing-ortho-thin-placement")))
    {
        sed::app_tests::runThingOrthoThinPlacementTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--thing-placement-hotpath")))
    {
        sed::app_tests::runThingPlacementBehaviorTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--thing-placement-id-exhaustion")))
    {
        sed::app_tests::runThingPlacementIdExhaustionTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--surface-extra-light-hdr")))
    {
        sed::app_tests::runSurfaceExtraLightHdrTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--mots-arch-bake-flags")))
    {
        sed::app_tests::runMotsArchBakeFlagTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--mots-arch-bake")))
    {
        sed::app_tests::runMotsArchBakeFlagTests();
        sed::app_tests::runMotsArchBakePipelineTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--sector-ambient")))
    {
        sed::app_tests::runSectorAmbientPropertyTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--engine-export-options")))
    {
        sed::app_tests::runEngineExportOptionsTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--viewport-thing-overlays")))
    {
        sed::app_tests::runViewportThingOverlayStateTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(
            QStringLiteral("--strict-surface-selection-recovery")))
    {
        sed::app_tests::runStrictSurfaceSelectionRecoveryTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(
            QStringLiteral("--strict-surface-context-topology")))
    {
        sed::app_tests::runStrictSurfaceContextTopologyTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--mots-resource-open")))
    {
        sed::app_tests::runMotsResourceOpenTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--main-window-event-filter-only")))
    {
        sed::app_tests::runMainWindowEventFilterTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--cleave-ui")) )
    {
        sed::app_tests::runCleaveUiOwnershipTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--cleave-options-layout")))
    {
        sed::app_tests::runCleaveOptionsLayoutTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--cleave-shape-overlay")) )
    {
        sed::app_tests::runCleaveShapeOverlayTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--cleave-family-controls")))
    {
        sed::app_tests::runCleaveFamilyControlsTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--cleave-workspace-persistence")) )
    {
        sed::app_tests::runCleaveWorkspacePersistenceTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--cleave-document-reset")))
    {
        sed::app_tests::runCleaveDocumentResetOwnershipTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--cleave-apply-workspace-history")) )
    {
        sed::app_tests::runCleaveApplyWorkspaceHistoryTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--cleave-advanced-interaction")) )
    {
        sed::app_tests::runCleaveAdvancedInteractionTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--cleave-input-routing")) )
    {
        sed::app_tests::runCleaveInputRoutingTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--cleave-uv-key-release")) )
    {
        sed::app_tests::runCleaveUvKeyReleaseTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--cleave-mode-change")) )
    {
        sed::app_tests::runCleaveModeChangeTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--cleave-pointer-ownership")) )
    {
        sed::app_tests::runCleavePointerOwnershipTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--cleave-camera-preview")) )
    {
        sed::app_tests::runCleaveCameraPreviewTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--cleave-uv-custom-owner")) )
    {
        sed::app_tests::runCleaveUvCustomShortcutOwnershipTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--cleave-fly-custom-shortcuts")) )
    {
        sed::app_tests::runCleaveFlyAndCustomShortcutTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--cleave-shape-apply")) )
    {
        sed::app_tests::runCleaveShapeApplyTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--mots-arch-workspace-recovery")) )
    {
        sed::app_tests::runMotsArchLightingWorkspaceRecoveryTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--cleave-chain-origin")) )
    {
        sed::app_tests::runCleaveChainOriginTests();
        sed::app_tests::runCleaveChainBodyRecoveryTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--cleave-connection-history")) )
    {
        sed::app_tests::runCleaveConnectionHistoryTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--resource-picker-release")))
    {
        sed::app_tests::runResourcePickerReleaseTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--resource-authoring")))
    {
        sed::app_tests::runResourceAuthoringEditorTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--resource-authoring-presentation")))
    {
        sed::app_tests::runResourceAuthoringPresentationTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if ( app.arguments().contains(QStringLiteral("--resource-authoring-history-caret")) )
    {
        sed::app_tests::runResourceAuthoringHistoryCaretRegressionTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--resource-workspace-regression")))
    {
        sed::app_tests::runResourceWorkspaceRegressionTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--resource-authoring-lifecycle")))
    {
        sed::app_tests::runResourceAuthoringLifecycleTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--resource-document-history")))
    {
        sed::app_tests::runResourceAuthoringHistoryTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--resource-authoring-sprite-geometry")))
    {
        sed::app_tests::runResourceAuthoringSpriteGeometryTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--runtime-lighting-switches")))
    {
        sed::app_tests::runRuntimeLightingSwitchTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--loose-document-watcher")))
    {
        sed::app_tests::runLooseDocumentWatcherTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--cog-document-docking")))
    {
        sed::app_tests::runCogDocumentDockingTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--cog-overwrite-lifetime")))
    {
        sed::app_tests::runCogOverwriteLifetimeTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--cog-interaction-regressions")))
    {
        sed::app_tests::runCogInteractionRegressionTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--cog-shortcut-context")))
    {
        sed::app_tests::runCogShortcutContextTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--native-save-edit-validation")))
    {
        sed::app_tests::runNativeSaveEditValidationTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--overlay-then-main-window-event-filter")))
    {
        sed::app_tests::runViewportThingOverlayStateTests();
        sed::app_tests::runMainWindowEventFilterTests();
        sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return sed::app_tests::failures == 0 ? 0 : 1;
    }
    sed::app_tests::runNativeCaptionVisibilityTests();
    sed::app_tests::runUvCommandPrecisionTests();
    sed::app_tests::runThingTransformCommandTests();
    sed::app_tests::runTranslateMenuContractTests();
    sed::app_tests::runTransformNumericPrecisionTests();
    sed::app_tests::runDesignerUiPrecisionTests();
    sed::app_tests::runConsistencyPrecisionDialogTests();
    sed::app_tests::runEditingRejectionUiTests();
    sed::app_tests::runUvNumericInputPrecisionTests();
    sed::app_tests::runUvZoomPreferenceTests();
    sed::app_tests::runProjectResourcePrecisionTests();
    sed::app_tests::runResourcePickerReleaseTests();
    sed::app_tests::runResourceAuthoringEditorTests();
    sed::app_tests::runResourceAuthoringLifecycleTests();
    sed::app_tests::runResourceAuthoringSpriteGeometryTests();
    sed::app_tests::runResourceAuthoringHistoryTests();
    sed::app_tests::runRuntimeLightingSwitchTests();
    sed::app_tests::runNativeSaveEditValidationTests();
    sed::app_tests::runTemplateArchiveRefreshTests();
    sed::app_tests::runTemplateCollisionBoundsPreviewTests();
    sed::app_tests::runTemplateDuplicateTypeTests();
    sed::app_tests::runParameterHelperDialogTests();
    sed::app_tests::runSurfaceMultiCutRegressionTests();
    sed::app_tests::runSurfaceRoundoffMultiCutRegressionTests();
    sed::app_tests::runThingPlacementBehaviorTests();
    sed::app_tests::runLightGizmoSelectionTests();
    sed::app_tests::runCleaveBoundedRecoveryTests();
    sed::app_tests::runCleaveConnectionHistoryTests();
    sed::app_tests::runCleaveShapeOverlayTests();
    sed::app_tests::runCleaveInputRoutingTests();
    sed::app_tests::runCleaveWheelSessionTests();
    sed::app_tests::runThingOrthoThinPlacementTests();
    sed::app_tests::runThingMoveDragHotPathTests();
    sed::app_tests::runCogScriptLifecycleTests();
    sed::app_tests::runCogInteractionRegressionTests();
    sed::app_tests::runCogShortcutContextTests();
    sed::app_tests::runCogDocumentDockingTests();
    sed::app_tests::runLooseDocumentWatcherTests();
    sed::app_tests::runCogInlineAssignmentTests();
    sed::app_tests::runThingSectorPropertyNavigationTests();
    sed::app_tests::runCleavePrecisionUiTests();
    sed::app_tests::runViewportThingOverlayStateTests();
    sed::app_tests::runStrictSurfaceSelectionRecoveryTests();
    sed::app_tests::runUvGridPointsAppearanceTests();
    sed::app_tests::runNavigationPreferencesTests();
    sed::app_tests::runViewportGridComboPopupLifetimeTests();
    sed::app_tests::runFrameHistorySelectionTests();
    sed::app_tests::runFrameAddMoveBehaviorTests();
    sed::app_tests::runFrameRenderPerformanceTests();
    if (!app.arguments().contains(QStringLiteral("--release-precision")))
    {
        sed::app_tests::runMainWindowEventFilterTests();
        sed::app_tests::runSectorAmbientPropertyTests();
        sed::app_tests::runSurfaceActivePropertyTests();
        sed::app_tests::runSurfaceSelectionHistoryTests();
    }
    sed::app_tests::runUiDecimalPolicyTests();
    sed::app_tests::runUiDecimalConsumerTests();
    sed::app_tests::runResourceUiDecimalTests();
    sed::ui::detail::previewWorkerPool().waitForDone();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    return sed::app_tests::failures == 0 ? 0 : 1;
}
