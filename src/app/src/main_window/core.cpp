#include "main_window/common.hpp"
#include "ui/audio/app_sound_player.hpp"
#include "ui/audio/audio_resource_identity_utils.hpp"
#include "ui/resources/project_resource_settings.hpp"
#include "ui/components/ui_decimal_format.hpp"

#include <cstring>
#include <QFutureWatcher>
#include <QPalette>

namespace sed
{

#include "main_window/detail/helpers.hpp"

namespace
{

constexpr QSize kMainWindowMinimumSize{1440, 900};
constexpr QSize kMainWindowDefaultSize{1680, 980};

QString mainWindowChromeStyleSheet()
{
    return QStringLiteral(
        "QMainWindow#sed_main_window { background-color: #252525; }"
        "QMainWindow#sed_main_window QMenuBar {"
        " background-color: #1f1f1f;"
        " color: #f2f2f2;"
        " border-bottom: 1px solid #383838;"
        " padding: 0px 2px;"
        "}"
        "QMainWindow#sed_main_window QMenuBar::item {"
        " background-color: transparent;"
        " padding: 5px 8px;"
        "}"
        "QMainWindow#sed_main_window QMenuBar::item:selected { background-color: #343434; }"
        "QMainWindow#sed_main_window QMenuBar::item:pressed { background-color: #3d3d3d; }"
        "QMainWindow#sed_main_window QMenuBar::item:disabled { color: #686868; }"
        "QMainWindow#sed_main_window QStatusBar {"
        " background-color: #252525;"
        " color: #e8e8e8;"
        " border-top: 1px solid #383838;"
        "}"
        "QMainWindow#sed_main_window QSplitter::handle { background-color: #252525; }");
}

} // namespace

MainWindow::MainWindow(
    QWidget *parent, const bool deferWindowPlacementRestore)
    : QMainWindow(parent),
      windowPlacementRestoreDeferred_(deferWindowPlacementRestore)
{
    if ( qApp && !ui::uiDecimalPolicyInstalled() )
    {
        const QSettings settings = settingsStore();
        ui::installUiDecimalPolicy(*qApp, loadUiDecimalSeparator(settings));
    }
    ui::ensureGlobalComponentStyles();
    setObjectName("sed_main_window");
    QPalette chromePalette = palette();
    chromePalette.setColor(QPalette::Window, ui::applicationCanvasColor());
    setPalette(chromePalette);
    setAutoFillBackground(true);
    setStyleSheet(mainWindowChromeStyleSheet());
    setMinimumSize(kMainWindowMinimumSize);
    resize(kMainWindowDefaultSize);

    // The nested workspace gives the COG editor its own dock boundary. The
    // outer outliner and inner COG dock can therefore be resized independently.
    workspaceWindow_ = new QMainWindow();
    // QMainWindow promotes itself to Qt::Window during construction. Reparent
    // it explicitly and clear that flag so it cannot detach from or become
    // visible independently of the application window.
    workspaceWindow_->setParent(this, Qt::Widget);
    workspaceWindow_->setWindowFlag(Qt::Window, false);
    workspaceWindow_->setObjectName("workspaceWindow");
    workspaceWindow_->setDockNestingEnabled(true);
    workspaceWindow_->setDockOptions(
        QMainWindow::AllowNestedDocks
        | QMainWindow::AllowTabbedDocks
        | QMainWindow::GroupedDragging
        | QMainWindow::AnimatedDocks);

    viewportUvSplitter_ = new QSplitter(Qt::Vertical, workspaceWindow_);
    viewportUvSplitter_->setObjectName(QStringLiteral("viewportUvSplitter"));
    viewportUvSplitter_->setChildrenCollapsible(false);
    viewportUvSplitter_->setHandleWidth(6);

    glView_ = new GlView(viewportUvSplitter_);
    glView_->setAcceptDrops(true);
    viewportUvSplitter_->addWidget(glView_);

    uvEditorPanelHost_ = new QWidget(viewportUvSplitter_);
    uvEditorPanelHost_->setObjectName("uvEditorPanelHost");
    viewportUvSplitter_->addWidget(uvEditorPanelHost_);

    workspaceWindow_->setCentralWidget(viewportUvSplitter_);
    setCentralWidget(workspaceWindow_);
    workspaceWindow_->show();

    if (qApp)
    {
        qApp->installEventFilter(this);
    }

    {
        const ProjectType initialKind = determineInitialProjectKind();
        document_.level().kind = initialKind;
        const std::int64_t timestamp = std::max<std::int64_t>(
            1, QDateTime::currentMSecsSinceEpoch());
        nativeDocumentTimestamp_ = timestamp;
        loadResourceSettings(initialKind);
    }
    rebuildResourceLocator({});
    glView_->setResourceLocator(&resources_);
    glView_->setLevel(&document_.level());
    resetLayerStatesFromLevel();
    resetSelectionContextState();
    markCurrentDocumentAsSaved();

    // Default initial viewport state.
    glView_->setRenderPipeline(GlView::RenderPipeline::Editor);
    glView_->setLightingMode(GlView::LightingMode::Gouraud);
    glView_->setCameraProjectionType(app::ViewportCamera::ProjectionType::Orthographic);
    glView_->resetEditorTopDownView();
    pendingEditorViewRefitAfterResize_ = true;
    glView_->resetFlySpeedMultiplier();
    glView_->setFocus(Qt::OtherFocusReason);

    setupDockWidgets();
    setupResourceEditorsDock();
    editorViewRefitTimer_ = new QTimer(this);
    editorViewRefitTimer_->setSingleShot(true);
    editorViewRefitTimer_->setInterval(90);
    connect(editorViewRefitTimer_, &QTimer::timeout, this, [this]() {
        if (!pendingEditorViewRefitAfterResize_ || !glView_)
        {
            return;
        }
        if (!editorViewRefitViewportReady())
        {
            if (glView_->isVisible())
            {
                editorViewRefitTimer_->start();
            }
            return;
        }

        // A level load can trigger several dock and splitter passes. Fit only
        // after observing the same usable viewport size twice in succession.
        const QSize viewportSize = glView_->size();
        if (viewportSize != editorViewRefitLastViewportSize_)
        {
            editorViewRefitLastViewportSize_ = viewportSize;
            editorViewRefitTimer_->start();
            return;
        }

        pendingEditorViewRefitAfterResize_ = false;
        glView_->resetEditorTopDownView();
        if (!document_.level().sectors.empty() || !document_.level().things.empty() || !document_.level().lights.empty() || !document_.level().cogs.empty())
        {
            glView_->setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
        }
        positionViewportOverlay();
        positionAddSectorOptionsOverlay();
        positionTransformMoveOptionsOverlay();
        positionLightBakeOverlay();
        positionSectorJoinOptionsOverlay();
        positionSectorThrustOptionsOverlay();
        positionCleaveOptionsOverlay();
        positionThingMoveOptionsOverlay();
        positionSurfaceExtrudeOptionsOverlay();
        positionThingPlacementPanel();
        positionViewportTransformOverlay();
        positionViewportToolsOverlay();
        positionViewportEditModeOverlay();
        reflowTopCenterOverlays();
        glView_->update();
    });

    QTimer::singleShot(0, this, [this]() {
        restoreStandaloneOutlinerDockWidth();
        updateCogScriptDockWidthConstraints();
        if (cogScriptEditorDock_ && !cogScriptEditorDock_->isHidden())
        {
            restoreCogScriptDockWidth();
        }
        if (pendingEditorViewRefitAfterResize_ && editorViewRefitTimer_)
        {
            editorViewRefitTimer_->start();
        }
    });

    checksRebuildTimer_ = new QTimer(this);
    checksRebuildTimer_->setSingleShot(true);
    checksRebuildTimer_->setInterval(200);
    connect(checksRebuildTimer_, &QTimer::timeout, this, [this]() {
        rebuildChecks();
    });

    setupMenus();
    // The shared Edit > Find action is created by setupMenus(). Install COG
    // focus-aware shortcut routing afterwards so Ctrl+F has exactly one
    // authoritative application action.
    setupCogScriptEditorActions();
    setupToolbars();
    projectTypeStatusLabel_ = new QLabel(this);
    projectTypeStatusLabel_->setObjectName("status_project_type");
    projectTypeStatusLabel_->setAlignment(Qt::AlignCenter);
    projectTypeStatusLabel_->setMinimumWidth(62);
    projectTypeStatusLabel_->setContentsMargins(8, 1, 8, 1);
    statusBar()->addPermanentWidget(projectTypeStatusLabel_);
    updateProjectTypeStatusLabel();
    legacyUvStatusLabel_ = new QLabel(this);
    legacyUvStatusLabel_->setObjectName("status_legacy_uv");
    legacyUvStatusLabel_->setAlignment(Qt::AlignCenter);
    legacyUvStatusLabel_->setContentsMargins(8, 1, 8, 1);
    legacyUvStatusLabel_->setStyleSheet(
        "QLabel#status_legacy_uv {"
        " color: #ffd98a;"
        " background-color: #4a391c;"
        " border: 1px solid #9b7029;"
        " border-radius: 3px;"
        " padding: 2px 8px;"
        " font-weight: 650;"
        "}");
    legacyUvStatusLabel_->hide();
    statusBar()->addPermanentWidget(legacyUvStatusLabel_);
    if (!windowPlacementRestoreDeferred_)
    {
        restoreWindowPlacement();
    }
    initializeKeymapBindings();
    setupUvEditorPanel();
    setupViewportOverlay();
    setupViewportTransformOverlay();
    setupAddSectorOptionsOverlay();
    setupTransformMoveOptionsOverlay();
    setupLightBakeControls();
    setupSectorJoinOptionsOverlay();
    setupThingMoveOptionsOverlay();
    setupSurfaceExtrudeOptionsOverlay();
    setupThingPlacementPanel();
    setupViewportToolsOverlay();
    setupCleaveOptionsOverlay();
    setupSectorThrustOptionsOverlay();
    setupViewportEditModeOverlay();
    loadGlobalTemplates();

    loadViewportPreferences();

    connectViewportSignals();

    sectorSoundTimer_ = new QTimer(this);
    sectorSoundTimer_->setInterval(125);
    connect(sectorSoundTimer_, &QTimer::timeout, this, [this]()
        {
            updateSectorSoundPlayback();
        });
    sectorSoundLoadWatcher_ = new QFutureWatcher<QByteArray>(this);
    connect(sectorSoundLoadWatcher_, &QFutureWatcher<QByteArray>::finished,
        this, [this]()
        {
            using ui::audio::AppSoundPlayer;
            using ui::audio::SoundPlaybackOwner;

            viewportSoundLoadPending_ = false;
            QByteArray bytes;
            try { bytes = sectorSoundLoadWatcher_->result(); }
            catch (const std::exception&) { /* Existing unavailable-resource path below. */ }
            const std::string liveResourceKey = viewportSoundLocatorGeneration_
                ? ui::audio::soundResourceRequestKey(*viewportSoundLocatorGeneration_, viewportSoundName_)
                : std::string{};
            const bool stillWanted =
                viewportPendingSoundLoadGeneration_ == viewportSoundLoadGeneration_
                && ui::audio::ambientSoundResultCurrent(viewportPendingSoundResourceKey_,
                    viewportSoundResourceKey_, liveResourceKey)
                && !viewportSoundResourceKey_.empty()
                && !currentViewportSoundMuted()
                && !QApplication::activeModalWidget()
                && !AppSoundPlayer::instance().hasPreviewOwner();
            if (stillWanted && !bytes.isEmpty())
            {
                std::vector<std::byte> soundBytes(
                    static_cast<std::size_t>(bytes.size()));
                std::memcpy(
                    soundBytes.data(), bytes.constData(), soundBytes.size());
                viewportSoundPlaybackCurrent_ =
                    AppSoundPlayer::instance().playBytes(
                        SoundPlaybackOwner::viewport,
                        viewportPendingSoundResourceKey_,
                        std::move(soundBytes),
                        true,
                        viewportPendingSoundVolume_);
                if (!viewportSoundPlaybackCurrent_)
                {
                    viewportFailedSoundResourceKey_ =
                        viewportPendingSoundResourceKey_;
                }
            }
            else if (stillWanted)
            {
                viewportFailedSoundResourceKey_ =
                    viewportPendingSoundResourceKey_;
                logMessage(
                    MessageSeverity::warning,
                    QStringLiteral("Sector sound resource could not be loaded: %1")
                        .arg(QString::fromStdString(viewportSoundName_)),
                    MessageLogCategory::resources);
            }

            QTimer::singleShot(0, this,
                [this]() { updateSectorSoundPlayback(); });
        });
    sectorSoundTimer_->start();

    rebuildOutliner();
    rebuildProperties();
    updateEditActions();
    // Recovery captures the complete editor workspace, so install its signal
    // subscriptions and freeze the initial clean baseline only after every
    // dock, overlay, toolbar, preference, and neutral tool state exists.
    // Initializing earlier made an untouched startup appear EDIT-dirty and
    // also missed widgets that had not been constructed yet.
    initializeRecoverySupport();
    statusBar()->showMessage("Ready");
}

void MainWindow::scheduleEditorViewRefitAfterLayout()
{
    pendingEditorViewRefitAfterResize_ = true;
    editorViewRefitLastViewportSize_ = {};
    if (editorViewRefitTimer_)
    {
        editorViewRefitTimer_->start();
    }
}

bool MainWindow::editorViewRefitViewportReady() const noexcept
{
    if (!glView_ || !glView_->isVisible())
    {
        return false;
    }

    constexpr int kMinStableViewportSize = 128;
    constexpr double kMinStableAspect = 0.20;
    constexpr double kMaxStableAspect = 8.00;

    const QSize size = glView_->size();
    if (size.width() < kMinStableViewportSize || size.height() < kMinStableViewportSize)
    {
        return false;
    }

    const double aspect = static_cast<double>(size.width()) / static_cast<double>(size.height());
    return aspect >= kMinStableAspect && aspect <= kMaxStableAspect;
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
void MainWindow::scheduleEditorViewRefitAfterLayoutForTest()
{
    scheduleEditorViewRefitAfterLayout();
}

bool MainWindow::editorViewRefitViewportReadyForTest() const noexcept
{
    return editorViewRefitViewportReady();
}

bool MainWindow::pendingEditorViewRefitForTest() const noexcept
{
    return pendingEditorViewRefitAfterResize_;
}

bool MainWindow::editorViewRefitTimerActiveForTest() const noexcept
{
    return editorViewRefitTimer_ && editorViewRefitTimer_->isActive();
}
#endif

} // namespace sed
