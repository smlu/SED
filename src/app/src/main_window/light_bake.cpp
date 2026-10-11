#include "main_window/common.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"
#include "ui/lighting/light_bake_utils.hpp"
#include "ui/lighting/mots_arch_bake_utils.hpp"

#include <QtConcurrent/QtConcurrentRun>

#include <QFutureWatcher>

namespace sed
{

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
bool MainWindow::lightBakeDebounceActiveForTest() const noexcept
{
    return lightBakeDebounceTimer_ && lightBakeDebounceTimer_->isActive();
}

void MainWindow::startPendingAutomaticLightBakeForTest()
{
    if (lightBakeDebounceTimer_)
    {
        lightBakeDebounceTimer_->stop();
    }
    requestLightBake(true);
}
#endif

namespace
{

constexpr int kLightBakePollIntervalMs = 50;
constexpr int kLightBakeThrottleMs = 75;

struct LightBakeWorkerResult
{
    Level level{};
    std::vector<std::int32_t> targets{};
    std::uint64_t generation{};
    std::uint64_t sourceFingerprint{};
    std::uint64_t motsArchSourceFingerprint{};
    std::vector<std::int32_t> changedArchThings{};
    bool motsArchRequested{};
    bool motsArchSucceeded{ true };
    std::string motsArchMessage{};
    std::shared_ptr<const assets::ResourceLocator> resourceSnapshot{};
    bool canceled{};
    bool automatic{};
    bool updateSectorAmbient{};
    std::string manualUndoLabel{};
};

struct LightBakeTransformPreviewWorkerResult
{
    Level level{};
    std::vector<std::int32_t> targets{};
    std::vector<std::int32_t> changedArchThings{};
    std::vector<level::LightingInfluenceSphere> dirtyInfluences{};
    std::string motsArchMessage{};
    std::uint64_t sourceGeneration{};
    bool motsArchRequested{};
    bool motsArchSucceeded{ true };
    bool canceled{};
    bool updateSectorAmbient{};
};

struct MotsArchBakePublication
{
    std::vector<MotsArchLightObject> arch{};
    std::vector<MotsArchLightingBakeBaseline> baselines{};
    std::vector<std::pair<std::int32_t, std::int32_t>> thingReferences{};
    bool valid{};
    bool changed{};
};

[[nodiscard]] MotsArchBakePublication prepareMotsArchBakePublication(
    const Level& destination,
    const Level& baked)
{
    MotsArchBakePublication publication{};
    if (destination.things.size() != baked.things.size())
    {
        return publication;
    }
    publication.thingReferences.reserve(baked.things.size());
    for (std::size_t index = 0; index < baked.things.size(); ++index)
    {
        const Thing& current = destination.things[index];
        const Thing& result = baked.things[index];
        if (current.id != result.id)
        {
            return MotsArchBakePublication{};
        }
        if (current.archLightingIndex != result.archLightingIndex)
        {
            publication.thingReferences.emplace_back(
                static_cast<std::int32_t>(index), result.archLightingIndex);
        }
    }
    publication.changed = destination.motsArchLighting != baked.motsArchLighting
        || destination.motsArchLightingBakeBaselines
            != baked.motsArchLightingBakeBaselines
        || !publication.thingReferences.empty();
    publication.arch = baked.motsArchLighting;
    publication.baselines = baked.motsArchLightingBakeBaselines;
    publication.valid = true;
    return publication;
}

void commitMotsArchBakePublication(
    Level& destination,
    MotsArchBakePublication& publication) noexcept
{
    destination.motsArchLighting.swap(publication.arch);
    destination.motsArchLightingBakeBaselines.swap(publication.baselines);
    for (const auto& [thingIndex, archIndex] : publication.thingReferences)
    {
        destination.things[static_cast<std::size_t>(thingIndex)].archLightingIndex =
            archIndex;
    }
}

} // namespace

void MainWindow::setupLightBakeControls()
{
    if (!glView_ || lightBakeOverlay_)
    {
        return;
    }

    QSettings settings = settingsStore();

    lightBakeOverlay_ = new QWidget(glView_);
    lightBakeOverlay_->setAttribute(Qt::WA_StyledBackground, true);
    lightBakeOverlay_->setObjectName("lightBakeOverlay");

    auto* layout = new QHBoxLayout(lightBakeOverlay_);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    auto* title = new QLabel("Light bake", lightBakeOverlay_);
    title->setObjectName("lightBakeOverlayLabel");
    layout->addWidget(title);

    auto* separator = new QFrame(lightBakeOverlay_);
    separator->setObjectName("lightBakeOverlaySeparator");
    separator->setFrameShape(QFrame::VLine);
    layout->addWidget(separator);

    lightBakeRecalculateButton_ = new QToolButton(lightBakeOverlay_);
    lightBakeRecalculateButton_->setObjectName("lightBakeRecalculateButton");
    lightBakeRecalculateButton_->setText("Recalculate");
    lightBakeRecalculateButton_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::bakeRecalculate));
    lightBakeRecalculateButton_->setIconSize(QSize(22, 22));
    lightBakeRecalculateButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    lightBakeRecalculateButton_->setPopupMode(QToolButton::MenuButtonPopup);
    lightBakeRecalculateButton_->setAccessibleName("Recalculate baked lighting");
    lightBakeRecalculateButton_->setCursor(Qt::PointingHandCursor);

    auto* recalculateMenu = new ui::shortcuts::KeySequenceDisplayMenu(lightBakeRecalculateButton_);
    recalculateMenu->setObjectName("lightBakeRecalculateMenu");
    recalculateMenu->setToolTipsVisible(true);
    const bool initialVisibleOnly =
        settings.value("lighting/visible_sectors_only", true).toBool();
    lightBakeRecalculateAllAction_ = recalculateMenu->addAction(
        initialVisibleOnly ? "Recalc. Visible" : "Recalc. All");
    lightBakeRecalculateAllAction_->setObjectName(
        "lightBakeRecalculateAllAction");
    lightBakeRecalculateAllAction_->setToolTip(
        initialVisibleOnly
            ? "<b>Recalculate visible</b><br>Recalculate baked lighting for sectors on visible, unlocked layers."
            : "<b>Recalculate all</b><br>Recalculate baked lighting for all unlocked sectors.");
    lightBakeRecalculateSelectedAction_ =
        recalculateMenu->addAction("Recalc. Selected");
    lightBakeRecalculateSelectedAction_->setObjectName(
        "lightBakeRecalculateSelectedAction");
    lightBakeRecalculateSelectedAction_->setToolTip(
        "<b>Recalculate selected</b><br>Update lighting where selected Baked lights affect the level now and where their previous settings affected it. Recalculate all Baked lights together so overlapping lights stay correct. Visible only and locked-layer rules still apply.");
    lightBakeRecalculateButton_->setMenu(recalculateMenu);
    lightBakeRecalculateSelectedDefault_ = settings.value(
        "lighting/manual_recalculate_selected", false).toBool();

    const auto updateRecalculateDefault = [this](const bool selected)
    {
        lightBakeRecalculateSelectedDefault_ = selected;
        settingsStore().setValue("lighting/manual_recalculate_selected", selected);
        if (!lightBakeRecalculateButton_)
        {
            return;
        }
        const bool visibleOnly = lightBakeVisibleOnlyButton_
            ? lightBakeVisibleOnlyButton_->isChecked()
            : settingsStore().value(
                  "lighting/visible_sectors_only", true).toBool();
        const QString fullScopeText = visibleOnly
            ? QStringLiteral("Recalc. Visible")
            : QStringLiteral("Recalc. All");
        const QString fullScopeTooltip = visibleOnly
            ? QStringLiteral("<b>Recalculate visible</b><br>Recalculate baked lighting for sectors on visible, unlocked layers.")
            : QStringLiteral("<b>Recalculate all</b><br>Recalculate baked lighting for all unlocked sectors.");
        const QString fullScopeDescription = visibleOnly
            ? QStringLiteral("Recalculate baked lighting for sectors on visible, unlocked layers.")
            : QStringLiteral("Recalculate baked lighting for all unlocked sectors.");
        if (lightBakeRecalculateAllAction_)
        {
            lightBakeRecalculateAllAction_->setText(fullScopeText);
            lightBakeRecalculateAllAction_->setToolTip(fullScopeTooltip);
        }
        lightBakeRecalculateButton_->setText(
            selected ? QStringLiteral("Recalc. Selected") : fullScopeText);
        const QString selectedTooltip = QStringLiteral(
            "<b>Recalculate selected</b><br>Update lighting where selected Baked lights affect the level now and where their previous settings affected it. Recalculate all Baked lights together so overlapping lights stay correct. Visible only and locked-layer rules still apply.");
        const QString selectedDescription = QStringLiteral(
            "Update lighting where selected Baked lights affect the level now and where their previous settings affected it. Recalculate all Baked lights together so overlapping lights stay correct. Visible only and locked-layer rules still apply.");
        lightBakeRecalculateButton_->setToolTip(
            selected ? selectedTooltip : fullScopeTooltip);
        lightBakeRecalculateButton_->setAccessibleDescription(
            selected ? selectedDescription : fullScopeDescription);
    };
    updateRecalculateDefault(lightBakeRecalculateSelectedDefault_);
    layout->addWidget(lightBakeRecalculateButton_);

    lightBakeVisibleOnlyButton_ = new QToolButton(lightBakeOverlay_);
    lightBakeVisibleOnlyButton_->setObjectName("lightBakeVisibleOnlyButton");
    lightBakeVisibleOnlyButton_->setText("Visible only");
    lightBakeVisibleOnlyButton_->setCheckable(true);
    // Keep existing user choices, but make the safer/scoped bake the default for
    // a fresh settings file.
    lightBakeVisibleOnlyButton_->setChecked(initialVisibleOnly);
    lightBakeVisibleOnlyButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    lightBakeVisibleOnlyButton_->setToolTip(
        "<b>Visible only</b><br>Limit the bake to sectors on visible, unlocked layers. Turn this off to bake all unlocked sectors.");
    lightBakeVisibleOnlyButton_->setAccessibleDescription(
        "When active, recalculate only sectors on visible, unlocked layers. Otherwise all unlocked sectors are baked.");
    lightBakeVisibleOnlyButton_->setCursor(Qt::PointingHandCursor);
    layout->addWidget(lightBakeVisibleOnlyButton_);

    lightBakeUpdateAmbientButton_ = new QToolButton(lightBakeOverlay_);
    lightBakeUpdateAmbientButton_->setObjectName("lightBakeUpdateAmbientButton");
    lightBakeUpdateAmbientButton_->setText("Ambient");
    lightBakeUpdateAmbientButton_->setCheckable(true);
    lightBakeUpdateAmbientButton_->setChecked(
        settings.value("lighting/update_sector_ambient", false).toBool());
    lightBakeUpdateAmbientButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    lightBakeUpdateAmbientButton_->setToolTip(
        "<b>Derive ambient</b><br>Update Sector Ambient colors from baked vertex lighting and Surface Extra Light.");
    lightBakeUpdateAmbientButton_->setAccessibleName("Derive sector ambient during light bake");
    lightBakeUpdateAmbientButton_->setAccessibleDescription(
        "Update Sector Ambient colors from baked vertex lighting and Surface Extra Light.");
    lightBakeUpdateAmbientButton_->setCursor(Qt::PointingHandCursor);
    layout->addWidget(lightBakeUpdateAmbientButton_);

    lightBakeAutoButton_ = new QToolButton(lightBakeOverlay_);
    lightBakeAutoButton_->setObjectName("lightBakeAutoButton");
    lightBakeAutoButton_->setText("Auto");
    lightBakeAutoButton_->setCheckable(true);
    lightBakeAutoButton_->setChecked(
        autoRecalculateLightingAction_
            ? autoRecalculateLightingAction_->isChecked()
            : settings.value("lighting/auto_recalculate", false).toBool());
    lightBakeAutoButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    lightBakeAutoButton_->setToolTip(
        "<b>Automatic bake</b><br>Recalculate affected baked lighting automatically when a Baked light changes.");
    lightBakeAutoButton_->setAccessibleName("Automatically recalculate baked lighting");
    lightBakeAutoButton_->setAccessibleDescription(
        "Recalculate affected baked lighting automatically when a Baked light changes.");
    lightBakeAutoButton_->setCursor(Qt::PointingHandCursor);
    layout->addWidget(lightBakeAutoButton_);

    lightBakeOverlay_->setStyleSheet(
        "QWidget#lightBakeOverlay {"
        " background: rgba(18, 18, 22, 235);"
        " border: 1px solid rgba(255, 255, 255, 40);"
        " border-radius: 8px;"
        "}"
        "QLabel#lightBakeOverlayLabel { color: rgba(230, 232, 238, 220); padding-left: 2px; }"
        "QFrame#lightBakeOverlaySeparator { color: rgba(255, 255, 255, 32); }"
        "QToolButton {"
        " color: rgba(236, 238, 242, 245);"
        " background: transparent;"
        " border: 1px solid transparent;"
        " border-radius: 5px;"
        " padding: 4px 7px;"
        "}"
        "QToolButton:hover { background: rgba(255, 255, 255, 24); }"
        "QToolButton:checked {"
        " background: rgba(90, 145, 220, 105);"
        " border-color: rgba(150, 195, 255, 90);"
        "}"
        "QToolButton:disabled { color: rgba(236, 238, 242, 85); }");

    connect(lightBakeRecalculateButton_, &QToolButton::clicked, this, [this]()
        {
            requestLightBake(false, lightBakeRecalculateSelectedDefault_);
        });
    connect(lightBakeRecalculateAllAction_, &QAction::triggered, this,
        [this, updateRecalculateDefault]()
        {
            updateRecalculateDefault(false);
            requestLightBake(false, false);
        });
    connect(lightBakeRecalculateSelectedAction_, &QAction::triggered, this,
        [this, updateRecalculateDefault]()
        {
            updateRecalculateDefault(true);
            requestLightBake(false, true);
        });
    connect(lightBakeVisibleOnlyButton_, &QToolButton::toggled, this,
        [this, updateRecalculateDefault](const bool checked)
        {
            settingsStore().setValue("lighting/visible_sectors_only", checked);
            updateRecalculateDefault(lightBakeRecalculateSelectedDefault_);
        });
    connect(lightBakeUpdateAmbientButton_, &QToolButton::toggled, this, [this](const bool checked)
        {
            settingsStore().setValue("lighting/update_sector_ambient", checked);
            if (transformPreview_.active
                && editMode_ == EditMode::light
                && !document_.selection().lights.empty())
            {
                updateLightBakeTransformPreview();
            }
        });
    connect(lightBakeAutoButton_, &QToolButton::toggled, this, [this](const bool checked)
        {
            setLightBakeAutoEnabled(checked);
        });

    lightBakeStatusButton_ = new QToolButton(statusBar());
    lightBakeStatusButton_->setObjectName("lightBakeStatusButton");
    lightBakeStatusButton_->setText("Lighting 0%");
    lightBakeStatusButton_->setToolTip(
        "<b>Lighting calculation</b><br>A baked-lighting calculation is running. Click to view its progress or cancel it.");
    lightBakeStatusButton_->setCursor(Qt::PointingHandCursor);
    lightBakeStatusButton_->setAutoRaise(true);
    lightBakeStatusButton_->hide();
    statusBar()->addWidget(lightBakeStatusButton_);
    connect(lightBakeStatusButton_, &QToolButton::clicked, this, &MainWindow::showLightBakeProgressDialog);

    lightBakeProgressDialog_ = new QDialog(this);
    lightBakeProgressDialog_->setObjectName("lightBakeProgressDialog");
    lightBakeProgressDialog_->setWindowTitle("Recalculating lighting");
    lightBakeProgressDialog_->setModal(false);
    lightBakeProgressDialog_->setMinimumWidth(390);

    auto* progressLayout = new QVBoxLayout(lightBakeProgressDialog_);
    progressLayout->setContentsMargins(14, 14, 14, 14);
    progressLayout->setSpacing(10);
    lightBakeProgressLabel_ = new QLabel("Preparing baked lighting...", lightBakeProgressDialog_);
    lightBakeProgressLabel_->setObjectName("lightBakeProgressLabel");
    progressLayout->addWidget(lightBakeProgressLabel_);
    lightBakeProgressBar_ = new QProgressBar(lightBakeProgressDialog_);
    lightBakeProgressBar_->setObjectName("lightBakeProgressBar");
    lightBakeProgressBar_->setRange(0, 100);
    lightBakeProgressBar_->setValue(0);
    lightBakeProgressBar_->setTextVisible(true);
    progressLayout->addWidget(lightBakeProgressBar_);
    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);
    lightBakeStopButton_ = new QPushButton("Stop", lightBakeProgressDialog_);
    lightBakeStopButton_->setObjectName("lightBakeStopButton");
    buttonRow->addWidget(lightBakeStopButton_);
    progressLayout->addLayout(buttonRow);
    connect(lightBakeStopButton_, &QPushButton::clicked, this, [this]()
        {
            cancelLightBake(true);
        });

    lightBakeDebounceTimer_ = new QTimer(this);
    lightBakeDebounceTimer_->setObjectName("lightBakeDebounceTimer");
    lightBakeDebounceTimer_->setSingleShot(true);
    lightBakeDebounceTimer_->setInterval(kLightBakeThrottleMs);
    connect(lightBakeDebounceTimer_, &QTimer::timeout, this, [this]()
        {
            requestLightBake(true);
        });

    lightBakePollTimer_ = new QTimer(this);
    lightBakePollTimer_->setObjectName("lightBakePollTimer");
    lightBakePollTimer_->setInterval(kLightBakePollIntervalMs);
    connect(lightBakePollTimer_, &QTimer::timeout, this, &MainWindow::pollLightBakeSourceChanges);
    // Polling is only needed while automatic baked-light recalculation is
    // enabled.  In the default/manual mode this timer otherwise scans the
    // complete authored light vector every 50 ms while the editor is idle.
    if (lightBakeAutoButton_->isChecked())
    {
        lightBakePollTimer_->start();
    }

    lightBakeTransformPreviewTimer_ = new QTimer(this);
    lightBakeTransformPreviewTimer_->setObjectName("lightBakeTransformPreviewTimer");
    lightBakeTransformPreviewTimer_->setSingleShot(true);
    lightBakeTransformPreviewTimer_->setInterval(kLightBakeThrottleMs);
    connect(lightBakeTransformPreviewTimer_, &QTimer::timeout, this, &MainWindow::updateLightBakeTransformPreview);

    pollLightBakeSourceChanges();
    syncLightBakeOverlay();
}

void MainWindow::positionLightBakeOverlay()
{
    if (!glView_ || !lightBakeOverlay_)
    {
        return;
    }

    lightBakeOverlay_->adjustSize();
    constexpr int margin = 10;
    int x = std::max(margin, (glView_->width() - lightBakeOverlay_->width()) / 2);
    const int y = margin;

    int minimumX = margin;
    for (QWidget* leftOverlay : std::array<QWidget*, 3>{
             viewportEditModeOverlay_, viewportTransformOverlay_, viewportToolsOverlay_})
    {
        if (leftOverlay && !leftOverlay->isHidden())
        {
            leftOverlay->adjustSize();
            minimumX = std::max(minimumX, leftOverlay->geometry().right() + 1 + margin);
        }
    }
    x = std::max(x, minimumX);

    // The bake strip owns the first top-center row in Light mode. Keep it away
    // from the right-side viewport controls on unusually narrow viewports.
    if (viewportOverlay_ && !viewportOverlay_->isHidden())
    {
        const QRect bakeRect(x, y, lightBakeOverlay_->width(), lightBakeOverlay_->height());
        const QRect avoidRect = viewportOverlay_->geometry().adjusted(-margin, -margin, margin, margin);
        if (bakeRect.intersects(avoidRect))
        {
            const int leftOfViewportControls = avoidRect.left() - lightBakeOverlay_->width() - margin;
            if (leftOfViewportControls >= minimumX)
            {
                x = leftOfViewportControls;
            }
        }
    }

    lightBakeOverlay_->move(x, y);
    reflowTopCenterOverlays();

    // Light transform options are contextual to this primary toolbar and form
    // the next centered row instead of competing for the same top edge.
    if (transformMoveOptionsOverlay_ && transformMoveOptionsOverlay_->isVisible())
    {
        positionTransformMoveOptionsOverlay();
    }
}

void MainWindow::syncLightBakeOverlay()
{
    if (!lightBakeOverlay_)
    {
        return;
    }

    const bool inPreview = glView_ && glView_->renderPipeline() == GlView::RenderPipeline::GamePreview;
    const bool visible = editMode_ == EditMode::light
        && lightEditSubMode_ == LightEditSubMode::baked
        && !inPreview;
    lightBakeOverlay_->setVisible(visible);
    if (lightBakeRecalculateButton_)
    {
        lightBakeRecalculateButton_->setEnabled(
            !inPreview && !document_.level().sectors.empty());
    }
    if (lightBakeRecalculateAllAction_)
    {
        lightBakeRecalculateAllAction_->setEnabled(
            !inPreview && !document_.level().sectors.empty());
    }
    if (lightBakeRecalculateSelectedAction_)
    {
        const bool hasValidSelectedLight = std::ranges::any_of(
            document_.selection().lights,
            [this](const std::int32_t index)
            {
                return index >= 0
                    && static_cast<std::size_t>(index)
                        < document_.level().lights.size();
            });
        lightBakeRecalculateSelectedAction_->setEnabled(
            !inPreview && !document_.level().sectors.empty()
            && hasValidSelectedLight);
    }
    for (QToolButton* option : std::array<QToolButton*, 3>{
             lightBakeVisibleOnlyButton_,
             lightBakeUpdateAmbientButton_,
             lightBakeAutoButton_ })
    {
        if (option)
        {
            option->setEnabled(!inPreview);
        }
    }
    if (visible)
    {
        positionLightBakeOverlay();
        lightBakeOverlay_->raise();
    }
}

void MainWindow::setLightBakeAutoEnabled(const bool enabled)
{
    settingsStore().setValue("lighting/auto_recalculate", enabled);

    if (autoRecalculateLightingAction_ && autoRecalculateLightingAction_->isChecked() != enabled)
    {
        const QSignalBlocker blocker(autoRecalculateLightingAction_);
        autoRecalculateLightingAction_->setChecked(enabled);
    }
    if (lightBakeAutoButton_ && lightBakeAutoButton_->isChecked() != enabled)
    {
        const QSignalBlocker blocker(lightBakeAutoButton_);
        lightBakeAutoButton_->setChecked(enabled);
    }

    if (!enabled)
    {
        restoreLightBakeTransformPreview();
        lightBakePendingAuto_ = false;
        lightBakePendingAutoTargets_.clear();
        lightBakePendingAutoInfluences_.clear();
        if (lightBakeDebounceTimer_)
        {
            lightBakeDebounceTimer_->stop();
        }
    }

    if (lightBakePollTimer_)
    {
        if (enabled)
        {
            // Catch edits made while polling was disabled before arming the
            // periodic watcher, then keep the low-latency path for live auto
            // bake edits.
            pollLightBakeSourceChanges();
            lightBakePollTimer_->start();
        }
        else
        {
            lightBakePollTimer_->stop();
        }
    }
}

void MainWindow::resetLightBakeSourceTracking()
{
    if (lightBakeTransformPreviewTimer_)
    {
        lightBakeTransformPreviewTimer_->stop();
    }
    cancelLightBakeTransformPreviewWorker();
    lightBakeTransformPreviewBaseline_.reset();
    lightBakeTransformPreviewSelection_.clear();
    lightBakeTransformPreviewTargets_.clear();
    lightBakeTransformPreviewInfluences_.clear();
    lightBakeTransformPreviewUpdatedAmbient_ = false;
    lightBakeTransformPreviewArchApplied_ = false;
    lightBakeTransformPreviewTemplates_.clear();
    lightBakeTransformPreviewResourceSnapshot_.reset();
    lightBakeTransformCommittedPreview_.reset();
    lightBakeTransformCommittedPreviewTargets_.clear();
    lightBakeTransformCommittedPreviewInfluences_.clear();
    lightBakeTransformCommittedPreviewUpdatedAmbient_ = false;
    lightBakeTransformCommittedPreviewArchApplied_ = false;
    ++lightBakeSourceGeneration_;
    lightBakeFingerprintInitialized_ = true;
    lightBakeObservedLights_ = document_.level().lights;
    lightBakePendingAuto_ = false;
    lightBakePendingAutoTargets_.clear();
    lightBakePendingAutoInfluences_.clear();
    lightBakePendingManual_ = false;
    lightBakePendingManualSelectedOnly_ = false;
    lightBakePendingManualTargets_.clear();
    lightBakePendingManualPermittedTargets_.clear();
    lightBakePendingManualInfluences_.clear();
    lightBakePendingManualUndoLabel_.clear();
    lightBakePendingManualUpdateAmbient_ = false;
    lightBakeActiveTargets_.clear();
    lightBakeActiveInfluences_.clear();
    if (lightBakeDebounceTimer_)
    {
        lightBakeDebounceTimer_->stop();
    }
    if (lightBakeRunning_ && lightBakeCancelFlag_)
    {
        lightBakeCancelFlag_->store(true, std::memory_order_relaxed);
    }
}

void MainWindow::cancelLightBakeTransformPreviewWorker()
{
    ++lightBakeTransformPreviewJobSerial_;
    lightBakeTransformPreviewPending_ = false;
    if (lightBakeTransformPreviewCancelFlag_)
    {
        lightBakeTransformPreviewCancelFlag_->store(true, std::memory_order_relaxed);
    }
}

std::vector<std::int32_t> MainWindow::collectLightBakeSectorTargets(const bool visibleOnly) const
{
    const Level& level = document_.level();
    std::vector<std::int32_t> targets{};
    targets.reserve(level.sectors.size());
    for (std::size_t i = 0; i < level.sectors.size(); ++i)
    {
        const Sector& sector = level.sectors[i];
        if (isLayerLocked(sector.layer))
        {
            continue;
        }
        if (visibleOnly
            && sector.layer >= 0
            && static_cast<std::size_t>(sector.layer) < layerVisible_.size()
            && !layerVisible_[static_cast<std::size_t>(sector.layer)])
        {
            continue;
        }
        targets.push_back(static_cast<std::int32_t>(i));
    }
    return targets;
}

void MainWindow::pollLightBakeSourceChanges()
{
    if (!documentEditingAllowed())
    {
        return;
    }
    // Polling runs every 50 ms while the editor is open.  Keep the current
    // document vector borrowed for the common unchanged case; copying the
    // entire light list on every tick caused avoidable allocation/memcpy work
    // in large levels.  We only copy into the observed snapshot after the
    // first observation or an actual source change.
    const std::vector<SedLight>& currentLights = document_.level().lights;
    if (!lightBakeFingerprintInitialized_)
    {
        lightBakeFingerprintInitialized_ = true;
        lightBakeObservedLights_ = currentLights;
        return;
    }
    if (ui::lighting::lightBakeLightSourcesEqual(lightBakeObservedLights_, currentLights))
    {
        return;
    }

    const std::vector<ui::lighting::LightBakeInfluenceSphere> influences =
        ui::lighting::lightBakeChangedInfluences(lightBakeObservedLights_, currentLights);
    lightBakeObservedLights_ = currentLights;
    if (influences.empty())
    {
        // A wholly disabled, zero-range light cannot affect baked output.
        return;
    }

    const bool automatic = lightBakeAutoButton_ && lightBakeAutoButton_->isChecked();
    if (automatic)
    {
        // Exact sphere/sector discovery walks every referenced level vertex.
        // Keep the GUI-side source poll cheap during palette and typed RGB
        // previews; the worker owns target discovery from its coherent level
        // snapshot before it starts the bake.
        ui::lighting::mergeLightBakeInfluences(lightBakePendingAutoInfluences_, influences);
    }
    else
    {
        lightBakePendingAutoTargets_.clear();
        lightBakePendingAutoInfluences_.clear();
    }
    ++lightBakeSourceGeneration_;

    if (lightBakeRunning_ && lightBakeCancelFlag_)
    {
        if (automatic)
        {
            // A superseded automatic result is discarded in full. Carry its
            // complete dirty region into the replacement job so rapid moves
            // cannot strand lighting in an earlier old-position footprint.
            ui::lighting::mergeLightBakeTargets(
                lightBakePendingAutoTargets_, lightBakeActiveTargets_);
            ui::lighting::mergeLightBakeInfluences(
                lightBakePendingAutoInfluences_, lightBakeActiveInfluences_);
        }
        lightBakePendingAuto_ = automatic
            && (!lightBakePendingAutoTargets_.empty()
                || !lightBakePendingAutoInfluences_.empty());
        lightBakeCancelFlag_->store(true, std::memory_order_relaxed);
    }
    if (automatic && lightBakeDebounceTimer_)
    {
        // Throttle instead of restarting a trailing debounce. Continuous
        // scrubbing/color edits therefore keep producing visible updates.
        if (!lightBakeDebounceTimer_->isActive())
        {
            lightBakeDebounceTimer_->start();
        }
    }
}

void MainWindow::updateLightBakeTransformPreview()
{
    if (!documentEditingAllowed()
        || !transformPreview_.active
        || !lightBakeAutoButton_
        || !lightBakeAutoButton_->isChecked()
        || document_.selection().lights.empty())
    {
        restoreLightBakeTransformPreview();
        return;
    }

    if (!lightBakeTransformPreviewBaseline_)
    {
        lightBakeTransformPreviewBaseline_ = std::make_shared<Level>(document_.level());
        lightBakeTransformPreviewTemplates_ = templates_;
        if (!resourceLocatorSnapshot_)
        {
            publishResourceLocatorSnapshot();
        }
        lightBakeTransformPreviewResourceSnapshot_ = resourceLocatorSnapshot_;
        lightBakeTransformPreviewSelection_.assign(
            document_.selection().lights.begin(),
            document_.selection().lights.end());
        std::sort(lightBakeTransformPreviewSelection_.begin(), lightBakeTransformPreviewSelection_.end());

        // A normal worker must not overwrite the temporary viewport result.
        ++lightBakeSourceGeneration_;
        ui::lighting::mergeLightBakeTargets(lightBakePendingAutoTargets_, lightBakeActiveTargets_);
        ui::lighting::mergeLightBakeInfluences(
            lightBakePendingAutoInfluences_, lightBakeActiveInfluences_);
        lightBakePendingAuto_ = !lightBakePendingAutoTargets_.empty()
            || !lightBakePendingAutoInfluences_.empty();
        if (lightBakeDebounceTimer_)
        {
            lightBakeDebounceTimer_->stop();
        }
        if (lightBakeRunning_ && lightBakeCancelFlag_)
        {
            lightBakeCancelFlag_->store(true, std::memory_order_relaxed);
        }
    }

    const Level& baseline = *lightBakeTransformPreviewBaseline_;
    const bool scaling = std::abs(transformPreview_.scaleFactor - 1.0) > 1.0e-8;
    const bool moving = transformPreview_.translation.DistanceSqr(TVector{}) > 1.0e-12;

    // Returning the handle to its origin should immediately restore the exact
    // pre-drag bake, including sectors touched at an intermediate position.
    if (!scaling && !moving)
    {
        cancelLightBakeTransformPreviewWorker();
        bool restoredOutput = false;
        const std::vector<std::int32_t> restoredTargets =
            lightBakeTransformPreviewTargets_;
        if (!restoredTargets.empty())
        {
            restoredOutput = ui::lighting::applyLightBakeResult(
                document_.level(),
                baseline,
                restoredTargets,
                lightBakeTransformPreviewUpdatedAmbient_);
        }
        if (lightBakeTransformPreviewArchApplied_)
        {
            MotsArchBakePublication restore = prepareMotsArchBakePublication(
                document_.level(), baseline);
            if (restore.valid)
            {
                restoredOutput = restore.changed || restoredOutput;
                commitMotsArchBakePublication(document_.level(), restore);
            }
        }
        if (restoredOutput && glView_)
        {
            const bool archWasRestored = lightBakeTransformPreviewArchApplied_;
            glView_->notifyBakedLightingChanged(
                restoredTargets,
                lightBakeTransformPreviewUpdatedAmbient_ && !archWasRestored);
            if (archWasRestored)
            {
                glView_->notifyThingAppearanceChanged();
            }
        }
        lightBakeTransformPreviewTargets_.clear();
        lightBakeTransformPreviewUpdatedAmbient_ = false;
        lightBakeTransformPreviewArchApplied_ = false;
        lightBakeTransformPreviewInfluences_.clear();
        return;
    }

    if (lightBakeTransformPreviewWorkerRunning_)
    {
        // Keep only the newest drag state. The running calculation observes
        // the flag at fine-grained work checkpoints and its completion handler
        // immediately launches this pending state.
        lightBakeTransformPreviewPending_ = true;
        if (lightBakeTransformPreviewCancelFlag_)
        {
            lightBakeTransformPreviewCancelFlag_->store(true, std::memory_order_relaxed);
        }
        return;
    }

    const bool visibleOnly = lightBakeVisibleOnlyButton_ && lightBakeVisibleOnlyButton_->isChecked();
    const std::vector<std::int32_t> permitted = collectLightBakeSectorTargets(visibleOnly);
    const bool updateSectorAmbient =
        lightBakeUpdateAmbientButton_ && lightBakeUpdateAmbientButton_->isChecked();
    const std::shared_ptr<const Level> baselineSnapshot = lightBakeTransformPreviewBaseline_;
    const std::vector<std::int32_t> selection = lightBakeTransformPreviewSelection_;
    const std::vector<level::LightingInfluenceSphere> pendingDirtyInfluences =
        lightBakePendingAutoInfluences_;
    const TVector translation = transformPreview_.translation;
    const double scaleFactor = transformPreview_.scaleFactor;
    const assets::Templates templateSnapshot = lightBakeTransformPreviewTemplates_;
    const std::shared_ptr<const assets::ResourceLocator> resourceSnapshot =
        lightBakeTransformPreviewResourceSnapshot_;
    const bool motsArchInputRequested =
        ui::lighting::motsArchBakeRequested(*baselineSnapshot);
    const std::uint64_t motsArchSourceFingerprint = motsArchInputRequested
        ? ui::lighting::motsArchLightingBakeSourceFingerprint(
            *baselineSnapshot, &templateSnapshot, resourceSnapshot.get())
        : 0u;
    const std::uint64_t sourceGeneration = lightBakeSourceGeneration_;

    lightBakeTransformPreviewWorkerRunning_ = true;
    lightBakeTransformPreviewPending_ = false;
    lightBakeTransformPreviewCancelFlag_ = std::make_shared<std::atomic_bool>(false);
    const std::shared_ptr<std::atomic_bool> cancelFlag = lightBakeTransformPreviewCancelFlag_;
    const std::uint64_t jobSerial = ++lightBakeTransformPreviewJobSerial_;

    using ResultPtr = std::shared_ptr<LightBakeTransformPreviewWorkerResult>;
    auto* watcher = new QFutureWatcher<ResultPtr>(this);
    watcher->setObjectName("lightBakeTransformPreviewFutureWatcher");
    lightBakeTransformPreviewWatcher_ = watcher;
    lightBakeTransformPreviewWaitForFinished_ = [watcher]()
    {
        watcher->waitForFinished();
    };

    connect(watcher, &QFutureWatcher<ResultPtr>::finished, this,
        [this, watcher, jobSerial, motsArchSourceFingerprint]()
        {
            const ResultPtr result = watcher->result();
            watcher->deleteLater();

            if (lightBakeTransformPreviewWatcher_ == watcher)
            {
                lightBakeTransformPreviewWatcher_ = nullptr;
                lightBakeTransformPreviewWaitForFinished_ = {};
                lightBakeTransformPreviewCancelFlag_.reset();
                lightBakeTransformPreviewWorkerRunning_ = false;
            }

            const bool latestStateWaiting = lightBakeTransformPreviewPending_;
            const bool currentJob = jobSerial == lightBakeTransformPreviewJobSerial_;
            if (!currentJob || !result || result->canceled || latestStateWaiting)
            {
                if (transformPreview_.active
                    && lightBakeTransformPreviewBaseline_
                    && latestStateWaiting)
                {
                    QTimer::singleShot(0, this, [this]()
                        {
                            updateLightBakeTransformPreview();
                        });
                }
                return;
            }

            const auto restoreDisplayedPreview = [this]()
            {
                if (!lightBakeTransformPreviewBaseline_)
                {
                    return;
                }
                bool changed = false;
                if (!lightBakeTransformPreviewTargets_.empty())
                {
                    changed = ui::lighting::applyLightBakeResult(
                        document_.level(), *lightBakeTransformPreviewBaseline_,
                        lightBakeTransformPreviewTargets_,
                        lightBakeTransformPreviewUpdatedAmbient_);
                }
                if (lightBakeTransformPreviewArchApplied_)
                {
                    MotsArchBakePublication restore = prepareMotsArchBakePublication(
                        document_.level(), *lightBakeTransformPreviewBaseline_);
                    if (restore.valid)
                    {
                        changed = restore.changed || changed;
                        commitMotsArchBakePublication(document_.level(), restore);
                    }
                }
                if (changed && glView_)
                {
                    const bool archWasRestored =
                        lightBakeTransformPreviewArchApplied_;
                    glView_->notifyBakedLightingChanged(
                        lightBakeTransformPreviewTargets_,
                        lightBakeTransformPreviewUpdatedAmbient_
                            && !archWasRestored);
                    if (archWasRestored)
                    {
                        glView_->notifyThingAppearanceChanged();
                    }
                }
                lightBakeTransformPreviewTargets_.clear();
                lightBakeTransformPreviewInfluences_.clear();
                lightBakeTransformPreviewUpdatedAmbient_ = false;
                lightBakeTransformPreviewArchApplied_ = false;
            };
            // Restore the previous preview before validating the live source:
            // its temporary ARCH output is derived data and must not make an
            // otherwise-current source fingerprint look stale.
            restoreDisplayedPreview();
            const bool archSourceMatches = !result->motsArchRequested
                || (lightBakeTransformPreviewResourceSnapshot_
                        == resourceLocatorSnapshot_
                    && motsArchSourceFingerprint
                        == ui::lighting::motsArchLightingBakeSourceFingerprint(
                            document_.level(), &templates_,
                            resourceLocatorSnapshot_.get()));
            if (result->sourceGeneration != lightBakeSourceGeneration_
                || !archSourceMatches)
            {
                return;
            }
            if (!result->motsArchSucceeded)
            {
                restoreDisplayedPreview();
                if (!result->motsArchMessage.empty() && statusBar())
                {
                    statusBar()->showMessage(
                        QString::fromStdString(result->motsArchMessage), 3500);
                }
                return;
            }

            std::vector<std::int32_t> changedTargets =
                lightBakeTransformPreviewTargets_;
            ui::lighting::mergeLightBakeTargets(changedTargets, result->targets);

            bool outputsChanged = false;
            const bool restoredAmbient = lightBakeTransformPreviewUpdatedAmbient_;
            if (!lightBakeTransformPreviewTargets_.empty())
            {
                outputsChanged = ui::lighting::applyLightBakeResult(
                    document_.level(),
                    *lightBakeTransformPreviewBaseline_,
                    lightBakeTransformPreviewTargets_,
                    restoredAmbient);
            }
            if (lightBakeTransformPreviewArchApplied_)
            {
                MotsArchBakePublication restore = prepareMotsArchBakePublication(
                    document_.level(), *lightBakeTransformPreviewBaseline_);
                if (!restore.valid)
                {
                    return;
                }
                outputsChanged = restore.changed || outputsChanged;
                commitMotsArchBakePublication(document_.level(), restore);
            }
            if (!result->targets.empty())
            {
                outputsChanged = ui::lighting::applyLightBakeResult(
                    document_.level(),
                    result->level,
                    result->targets,
                    result->updateSectorAmbient) || outputsChanged;
            }
            bool archOutputChanged = false;
            if (result->motsArchRequested)
            {
                MotsArchBakePublication publication = prepareMotsArchBakePublication(
                    document_.level(), result->level);
                if (!publication.valid)
                {
                    return;
                }
                if (publication.changed)
                {
                    commitMotsArchBakePublication(document_.level(), publication);
                    outputsChanged = true;
                    archOutputChanged = true;
                }
            }
            lightBakeTransformPreviewTargets_ = std::move(result->targets);
            lightBakeTransformPreviewInfluences_ = std::move(result->dirtyInfluences);
            lightBakeTransformPreviewUpdatedAmbient_ = result->updateSectorAmbient;
            lightBakeTransformPreviewArchApplied_ = archOutputChanged;
            if (outputsChanged && glView_)
            {
                glView_->notifyBakedLightingChanged(
                    changedTargets,
                    (restoredAmbient || result->updateSectorAmbient)
                        && !archOutputChanged);
                if (archOutputChanged)
                {
                    glView_->notifyThingAppearanceChanged();
                }
            }
        });

    watcher->setFuture(QtConcurrent::run(QThreadPool::globalInstance(),
        [baselineSnapshot,
         selection,
         permitted,
         pendingDirtyInfluences,
         translation,
         scaleFactor,
         scaling,
         updateSectorAmbient,
         templateSnapshot,
         resourceSnapshot,
         sourceGeneration,
         cancelFlag]() mutable -> ResultPtr
        {
            auto result = std::make_shared<LightBakeTransformPreviewWorkerResult>();
            result->updateSectorAmbient = updateSectorAmbient;
            result->sourceGeneration = sourceGeneration;
            if (cancelFlag->load(std::memory_order_relaxed))
            {
                result->canceled = true;
                return result;
            }

            result->level = *baselineSnapshot;
            for (const std::int32_t lightIndex : selection)
            {
                if (lightIndex < 0
                    || static_cast<std::size_t>(lightIndex) >= result->level.lights.size()
                    || static_cast<std::size_t>(lightIndex) >= baselineSnapshot->lights.size())
                {
                    continue;
                }

                SedLight& light = result->level.lights[static_cast<std::size_t>(lightIndex)];
                const SedLight& original = baselineSnapshot->lights[static_cast<std::size_t>(lightIndex)];
                if (scaling)
                {
                    light.range = std::max(0.0, original.range * scaleFactor);
                }
                else
                {
                    light.position = original.position.Add(translation);
                }
            }

            result->dirtyInfluences = ui::lighting::lightBakeChangedInfluences(
                baselineSnapshot->lights, result->level.lights);
            ui::lighting::mergeLightBakeInfluences(
                result->dirtyInfluences, pendingDirtyInfluences);
            result->targets =
                ui::lighting::lightBakeAffectedSectorTargets(
                    result->level, result->dirtyInfluences);
            std::erase_if(result->targets, [&permitted](const std::int32_t target)
                {
                    return !std::binary_search(permitted.begin(), permitted.end(), target);
                });

            result->motsArchRequested =
                ui::lighting::motsArchBakeRequested(result->level);
            if (result->motsArchRequested)
            {
                const bool hasSource = std::ranges::any_of(
                    result->level.lights,
                    [](const SedLight& light)
                    {
                        return (light.flags & LF_AffectArchThings) != 0;
                    });
                if (hasSource)
                {
                    if (!resourceSnapshot)
                    {
                        result->motsArchSucceeded = false;
                        result->motsArchMessage =
                            "MotS ARCH bake rejected: resource locator is unavailable.";
                        return result;
                    }
                    const auto resolved = ui::lighting::resolveMotsArchLightingBakeTargets(
                        result->level, &templateSnapshot, *resourceSnapshot, cancelFlag.get());
                    if (resolved.canceled)
                    {
                        result->canceled = true;
                        return result;
                    }
                    if (!resolved.success)
                    {
                        result->motsArchSucceeded = false;
                        result->motsArchMessage = resolved.message;
                        return result;
                    }
                    const auto archResult = level::levelCalcMotsArchLighting(
                        result->level, resolved.targets, &templateSnapshot,
                        [cancelFlag](const std::size_t, const std::size_t) -> bool
                        {
                            return !cancelFlag->load(std::memory_order_relaxed);
                        });
                    if (archResult.status == level::MotsArchLightingBakeStatus::Cancelled)
                    {
                        result->canceled = true;
                        return result;
                    }
                    if (archResult.status == level::MotsArchLightingBakeStatus::Rejected)
                    {
                        result->motsArchSucceeded = false;
                        result->motsArchMessage = archResult.message;
                        return result;
                    }
                    result->changedArchThings = archResult.changedThingIndices;
                }
                else
                {
                    const auto archResult = level::levelCalcMotsArchLighting(
                        result->level, {}, &templateSnapshot,
                        [cancelFlag](const std::size_t, const std::size_t) -> bool
                        {
                            return !cancelFlag->load(std::memory_order_relaxed);
                        });
                    if (archResult.status == level::MotsArchLightingBakeStatus::Cancelled)
                    {
                        result->canceled = true;
                        return result;
                    }
                    if (archResult.status == level::MotsArchLightingBakeStatus::Rejected)
                    {
                        result->motsArchSucceeded = false;
                        result->motsArchMessage = archResult.message;
                        return result;
                    }
                    result->changedArchThings = archResult.changedThingIndices;
                }
            }

            if (!result->targets.empty())
            {
                const bool completed = ::sed::level::levelCalcLightingNewInterruptible(
                    result->level,
                    result->targets,
                    [cancelFlag](const std::size_t, const std::size_t) -> bool
                    {
                        return !cancelFlag->load(std::memory_order_relaxed);
                    },
                    result->dirtyInfluences);
                if (!completed)
                {
                    result->canceled = true;
                    return result;
                }

                if (!updateSectorAmbient)
                {
                    for (const std::int32_t sectorIndex : result->targets)
                    {
                        if (sectorIndex >= 0
                            && static_cast<std::size_t>(sectorIndex) < result->level.sectors.size()
                            && static_cast<std::size_t>(sectorIndex) < baselineSnapshot->sectors.size())
                        {
                            result->level.sectors[static_cast<std::size_t>(sectorIndex)].ambient =
                                baselineSnapshot->sectors[static_cast<std::size_t>(sectorIndex)].ambient;
                        }
                    }
                }
            }

            result->canceled = cancelFlag->load(std::memory_order_relaxed);
            return result;
        }));
}

void MainWindow::restoreLightBakeTransformPreview()
{
    if (lightBakeTransformPreviewTimer_)
    {
        lightBakeTransformPreviewTimer_->stop();
    }
    cancelLightBakeTransformPreviewWorker();
    bool outputsChanged = false;
    const bool finishingCommittedPreview =
        lightBakeTransformCommittedPreview_
        && (!lightBakeTransformCommittedPreviewTargets_.empty()
            || lightBakeTransformCommittedPreviewArchApplied_);
    const std::vector<std::int32_t> committedPreviewTargets =
        lightBakeTransformCommittedPreviewTargets_;
    const std::vector<level::LightingInfluenceSphere> committedPreviewInfluences =
        lightBakeTransformCommittedPreviewInfluences_;
    const bool committedPreviewUpdatedAmbient =
        lightBakeTransformCommittedPreviewUpdatedAmbient_;
    const bool committedPreviewArchApplied =
        lightBakeTransformCommittedPreviewArchApplied_;
    const bool previewUpdatedAmbient = lightBakeTransformPreviewUpdatedAmbient_;
    if (finishingCommittedPreview)
    {
        // The light itself has now been committed, so keep displaying the last
        // fully calculated drag preview until the authoritative async pass
        // replaces it. This avoids flashing the pre-drag/no-light state.
        outputsChanged = lightBakeTransformCommittedPreviewTargets_.empty()
            || ui::lighting::applyLightBakeResult(
                document_.level(),
                *lightBakeTransformCommittedPreview_,
                lightBakeTransformCommittedPreviewTargets_,
                committedPreviewUpdatedAmbient);
        if (committedPreviewArchApplied)
        {
            MotsArchBakePublication restore = prepareMotsArchBakePublication(
                document_.level(), *lightBakeTransformCommittedPreview_);
            if (restore.valid)
            {
                outputsChanged = restore.changed || outputsChanged;
                commitMotsArchBakePublication(document_.level(), restore);
            }
        }
    }
    else if (lightBakeTransformPreviewBaseline_)
    {
        if (!lightBakeTransformPreviewTargets_.empty())
        {
            outputsChanged = ui::lighting::applyLightBakeResult(
                document_.level(),
                *lightBakeTransformPreviewBaseline_,
                lightBakeTransformPreviewTargets_,
                previewUpdatedAmbient);
        }
        if (lightBakeTransformPreviewArchApplied_)
        {
            MotsArchBakePublication restore = prepareMotsArchBakePublication(
                document_.level(), *lightBakeTransformPreviewBaseline_);
            if (restore.valid)
            {
                outputsChanged = restore.changed || outputsChanged;
                commitMotsArchBakePublication(document_.level(), restore);
            }
        }
    }
    if (outputsChanged && glView_)
    {
        const std::vector<std::int32_t>& changedTargets = finishingCommittedPreview
            ? lightBakeTransformCommittedPreviewTargets_
            : lightBakeTransformPreviewTargets_;
        const bool archWasRestored = finishingCommittedPreview
            ? committedPreviewArchApplied
            : lightBakeTransformPreviewArchApplied_;
        glView_->notifyBakedLightingChanged(
            changedTargets,
            (finishingCommittedPreview
                ? committedPreviewUpdatedAmbient
                : previewUpdatedAmbient)
                && !archWasRestored);
        if (archWasRestored)
        {
            glView_->notifyThingAppearanceChanged();
        }
    }
    lightBakeTransformPreviewBaseline_.reset();
    lightBakeTransformPreviewSelection_.clear();
    lightBakeTransformPreviewTargets_.clear();
    lightBakeTransformPreviewInfluences_.clear();
    lightBakeTransformPreviewUpdatedAmbient_ = false;
    lightBakeTransformCommittedPreview_.reset();
    lightBakeTransformCommittedPreviewTargets_.clear();
    lightBakeTransformCommittedPreviewInfluences_.clear();
    lightBakeTransformCommittedPreviewUpdatedAmbient_ = false;
    lightBakeTransformCommittedPreviewArchApplied_ = false;

    if (finishingCommittedPreview)
    {
        // An asynchronous preview can intentionally lag the final mouse
        // position. Include every sector painted by the retained preview in
        // the authoritative pass so an intermediate-only influence cannot be
        // left behind after the light commits elsewhere.
        ui::lighting::mergeLightBakeTargets(
            lightBakePendingAutoTargets_, committedPreviewTargets);
        ui::lighting::mergeLightBakeInfluences(
            lightBakePendingAutoInfluences_, committedPreviewInfluences);
        // Observe the committed light immediately instead of waiting for the
        // next polling tick. The retained preview masks the worker latency.
        pollLightBakeSourceChanges();
    }
    if (lightBakeAutoButton_
        && lightBakeAutoButton_->isChecked()
        && (!lightBakePendingAutoTargets_.empty()
            || !lightBakePendingAutoInfluences_.empty())
        && lightBakeDebounceTimer_
        && !lightBakeDebounceTimer_->isActive())
    {
        lightBakeDebounceTimer_->start();
    }
}

void MainWindow::prepareLightBakeTransformPreviewCommit()
{
    if (lightBakeTransformPreviewTimer_)
    {
        lightBakeTransformPreviewTimer_->stop();
    }
    cancelLightBakeTransformPreviewWorker();

    lightBakeTransformCommittedPreview_.reset();
    lightBakeTransformCommittedPreviewTargets_.clear();
    lightBakeTransformCommittedPreviewInfluences_.clear();
    lightBakeTransformCommittedPreviewUpdatedAmbient_ = false;
    lightBakeTransformCommittedPreviewArchApplied_ = false;
    if (lightBakeTransformPreviewBaseline_
        && (!lightBakeTransformPreviewTargets_.empty()
            || lightBakeTransformPreviewArchApplied_))
    {
        // Save the visible preview, then restore the exact pre-drag outputs so
        // the light-move undo snapshot remains correct. No mesh notification is
        // issued for this short-lived internal state; endTransformDrag emits
        // its committed light signal and inactive-preview signal synchronously.
        Level committedPreview = document_.level();
        const bool surfaceRestored = lightBakeTransformPreviewTargets_.empty()
            || ui::lighting::applyLightBakeResult(
                document_.level(),
                *lightBakeTransformPreviewBaseline_,
                lightBakeTransformPreviewTargets_,
                lightBakeTransformPreviewUpdatedAmbient_);
        bool archRestored = true;
        if (lightBakeTransformPreviewArchApplied_)
        {
            MotsArchBakePublication restore = prepareMotsArchBakePublication(
                document_.level(), *lightBakeTransformPreviewBaseline_);
            archRestored = restore.valid;
            if (restore.valid)
            {
                commitMotsArchBakePublication(document_.level(), restore);
            }
        }
        if (surfaceRestored && archRestored)
        {
            lightBakeTransformCommittedPreview_ = std::move(committedPreview);
            lightBakeTransformCommittedPreviewTargets_ = lightBakeTransformPreviewTargets_;
            lightBakeTransformCommittedPreviewInfluences_ =
                lightBakeTransformPreviewInfluences_;
            lightBakeTransformCommittedPreviewUpdatedAmbient_ =
                lightBakeTransformPreviewUpdatedAmbient_;
            lightBakeTransformCommittedPreviewArchApplied_ =
                lightBakeTransformPreviewArchApplied_;
        }
    }

    lightBakeTransformPreviewBaseline_.reset();
    lightBakeTransformPreviewSelection_.clear();
    lightBakeTransformPreviewTargets_.clear();
    lightBakeTransformPreviewInfluences_.clear();
    lightBakeTransformPreviewUpdatedAmbient_ = false;
    lightBakeTransformPreviewArchApplied_ = false;
    lightBakeTransformPreviewTemplates_.clear();
    lightBakeTransformPreviewResourceSnapshot_.reset();
}

void MainWindow::requestLightBake(
    const bool automatic,
    const bool selectedLightsOnly,
    std::vector<level::LightingInfluenceSphere> capturedSelectedInfluences)
{
    if (!documentEditingAllowed())
    {
        return;
    }
    if (automatic && (!lightBakeAutoButton_ || !lightBakeAutoButton_->isChecked()))
    {
        return;
    }

    std::vector<level::LightingInfluenceSphere> selectedInfluences{};
    if (!automatic && selectedLightsOnly)
    {
        std::vector<std::int32_t> selectedIndices{
            document_.selection().lights.begin(),
            document_.selection().lights.end()};
        std::sort(selectedIndices.begin(), selectedIndices.end());
        if (selectedIndices.empty() && capturedSelectedInfluences.empty()
            && !ui::lighting::motsArchBakeRequested(document_.level()))
        {
            if (statusBar())
            {
                statusBar()->showMessage(
                    "Select at least one Baked light to recalculate.", 3500);
            }
            return;
        }

        // Preserve the pre-poll snapshot for the selected lights' old
        // footprints, then observe all sources before taking ownership of the
        // pending automatic union.  In particular, an unrelated light can be
        // edited immediately before this click; pollLightBakeSourceChanges()
        // must first append that light's old/new footprints so the selected
        // manual job cannot clear them and strand stale baked output.
        const std::vector<SedLight> observedBeforePoll = lightBakeObservedLights_;
        pollLightBakeSourceChanges();
        // Multiple selected requests can supersede the same running worker.
        // Carry the already queued click-time union forward without consuming
        // it until a replacement worker is actually accepted for launch.
        if (lightBakePendingManualSelectedOnly_)
        {
            ui::lighting::mergeLightBakeInfluences(
                capturedSelectedInfluences,
                lightBakePendingManualInfluences_);
        }
        selectedInfluences = ui::lighting::lightBakeSelectedRequestInfluences(
            observedBeforePoll, document_.level().lights,
            selectedIndices, capturedSelectedInfluences,
            lightBakePendingAutoInfluences_);
        // An enabled Auto pass may already have observed the latest light and
        // retained its old footprint in the pending union. A manual selected
        // request supersedes that job, so conservatively finish those pending
        // regions rather than losing stale output from an earlier position.
        if (selectedInfluences.empty()
            && !ui::lighting::motsArchBakeRequested(document_.level()))
        {
            if (statusBar())
            {
                statusBar()->showMessage(
                    "The selected Baked lights have no current or previous "
                    "positive-range influence to recalculate.",
                    3500);
            }
            return;
        }
    }

    if (!automatic)
    {
        // A manual pass is authoritative for the current light state. Observe
        // a just-completed edit now so the next poll cannot enqueue a duplicate
        // automatic pass for the same state.
        if (!selectedLightsOnly)
        {
            pollLightBakeSourceChanges();
        }
        lightBakePendingAuto_ = false;
        lightBakePendingAutoTargets_.clear();
        lightBakePendingAutoInfluences_.clear();
        if (lightBakeDebounceTimer_)
        {
            lightBakeDebounceTimer_->stop();
        }
        if (!selectedLightsOnly)
        {
            lightBakePendingManualInfluences_.clear();
        }
    }

    const bool visibleOnly = lightBakeVisibleOnlyButton_ && lightBakeVisibleOnlyButton_->isChecked();
    std::vector<std::int32_t> targets{};
    std::vector<level::LightingInfluenceSphere> dirtyInfluences{};
    std::vector<std::int32_t> permittedTargets{};
    if (automatic)
    {
        permittedTargets = collectLightBakeSectorTargets(visibleOnly);
        targets = std::move(lightBakePendingAutoTargets_);
        lightBakePendingAutoTargets_.clear();
        dirtyInfluences = std::move(lightBakePendingAutoInfluences_);
        lightBakePendingAutoInfluences_.clear();
        std::erase_if(targets, [&permittedTargets](const std::int32_t target)
            {
                return !std::binary_search(
                    permittedTargets.begin(),
                    permittedTargets.end(),
                    target);
            });
    }
    else if (selectedLightsOnly)
    {
        permittedTargets = collectLightBakeSectorTargets(visibleOnly);
        if (permittedTargets.empty()
            && !ui::lighting::motsArchBakeRequested(document_.level()))
        {
            if (statusBar())
            {
                statusBar()->showMessage(
                    "No visible, unlocked sectors are available for the "
                    "selected-light recalculation.",
                    3500);
            }
            return;
        }
        dirtyInfluences = std::move(selectedInfluences);
    }
    else
    {
        targets = collectLightBakeSectorTargets(visibleOnly);
    }
    const std::string manualUndoLabel = automatic
        ? std::string{}
        : selectedLightsOnly
            ? std::string(ui::lighting::lightBakeUndoLabel(
                  ui::lighting::ManualLightBakeScope::selectedLightArea))
            : visibleOnly
                ? std::string(ui::lighting::lightBakeUndoLabel(
                      ui::lighting::ManualLightBakeScope::visibleSectors))
                : std::string(ui::lighting::lightBakeUndoLabel(
                      ui::lighting::ManualLightBakeScope::allSectors));
    startLightBake(
        std::move(targets),
        automatic,
        std::move(dirtyInfluences),
        std::move(permittedTargets),
        manualUndoLabel);
}

void MainWindow::startLightBake(
    std::vector<std::int32_t> targets,
    const bool automatic,
    std::vector<level::LightingInfluenceSphere> dirtyInfluences,
    std::vector<std::int32_t> permittedTargets,
    std::string manualUndoLabel,
    const std::optional<bool> requestedUpdateAmbient)
{
    if (!documentEditingAllowed())
    {
        return;
    }
    if (lightBakeRunning_)
    {
        if (automatic)
        {
            lightBakePendingAuto_ = true;
            ui::lighting::mergeLightBakeTargets(
                lightBakePendingAutoTargets_, targets);
            ui::lighting::mergeLightBakeInfluences(
                lightBakePendingAutoInfluences_, dirtyInfluences);
        }
        else
        {
            // A queued manual request is a complete click-time transaction.
            // Preserve its exact target/scope/option tuple instead of
            // reconstructing it later from whatever selection and toolbar
            // state happen to be current when the canceled worker exits.
            lightBakePendingManual_ = true;
            lightBakePendingManualSelectedOnly_ = targets.empty()
                && !dirtyInfluences.empty();
            lightBakePendingManualTargets_ = std::move(targets);
            lightBakePendingManualPermittedTargets_ =
                std::move(permittedTargets);
            lightBakePendingManualInfluences_ =
                std::move(dirtyInfluences);
            lightBakePendingManualUndoLabel_ =
                std::move(manualUndoLabel);
            lightBakePendingManualUpdateAmbient_ =
                requestedUpdateAmbient.value_or(
                    lightBakeUpdateAmbientButton_
                    && lightBakeUpdateAmbientButton_->isChecked());
            lightBakePendingAuto_ = false;
            lightBakePendingAutoTargets_.clear();
            lightBakePendingAutoInfluences_.clear();
        }
        if (lightBakeCancelFlag_)
        {
            lightBakeCancelFlag_->store(true, std::memory_order_relaxed);
        }
        return;
    }
    if (targets.empty() && dirtyInfluences.empty()
        && !ui::lighting::motsArchBakeRequested(document_.level()))
    {
        if (!automatic && statusBar())
        {
            statusBar()->showMessage("No visible, unlocked sectors are available for lighting recalculation.", 3500);
        }
        return;
    }

    const std::uint64_t generation = lightBakeSourceGeneration_;
    const bool motsArchRequested =
        ui::lighting::motsArchBakeRequested(document_.level());
    const bool motsArchHasSource = std::ranges::any_of(
        document_.level().lights,
        [](const SedLight& light)
        {
            return (light.flags & LF_AffectArchThings) != 0;
        });
    Level snapshot = ui::lighting::lightBakeWorkerSnapshot(document_.level());
    assets::Templates templateSnapshot{};
    std::shared_ptr<const assets::ResourceLocator> resourceSnapshot{};
    std::uint64_t motsArchSourceFingerprint = 0u;
    if (motsArchRequested)
    {
        // The ordinary Surface bake keeps its compact, fast snapshot. MotS
        // ARCH generation additionally needs stable Thing/ARCH identities,
        // template inheritance, and an immutable resource locator.
        snapshot.things = document_.level().things;
        snapshot.motsArchLighting = document_.level().motsArchLighting;
        snapshot.motsArchLightingBakeBaselines =
            document_.level().motsArchLightingBakeBaselines;
        templateSnapshot = templates_;
        if (!resourceLocatorSnapshot_)
        {
            publishResourceLocatorSnapshot();
        }
        resourceSnapshot = resourceLocatorSnapshot_;
        motsArchSourceFingerprint =
            ui::lighting::motsArchLightingBakeSourceFingerprint(
                document_.level(), &templates_, resourceSnapshot.get());
    }
    const bool updateSectorAmbient = requestedUpdateAmbient.value_or(
        lightBakeUpdateAmbientButton_
        && lightBakeUpdateAmbientButton_->isChecked());

    lightBakeRunning_ = true;
    const bool selectedManualScope =
        !automatic && targets.empty() && !dirtyInfluences.empty();
    lightBakeActiveTargets_ = targets;
    lightBakeActiveInfluences_ = dirtyInfluences;
    lightBakePendingAuto_ = false;
    lightBakePendingManual_ = false;
    lightBakePendingManualSelectedOnly_ = false;
    lightBakePendingManualTargets_.clear();
    lightBakePendingManualPermittedTargets_.clear();
    lightBakePendingManualInfluences_.clear();
    lightBakePendingManualUndoLabel_.clear();
    lightBakePendingManualUpdateAmbient_ = false;
    lightBakeCancelFlag_ = std::make_shared<std::atomic_bool>(false);
    const std::shared_ptr<std::atomic_bool> cancelFlag = lightBakeCancelFlag_;
    const std::uint64_t jobSerial = ++lightBakeJobSerial_;

    if (!automatic)
    {
        updateLightBakeProgress(0);
        if (statusBar())
        {
            // The clickable progress widget owns the status-bar text while a
            // manual bake is active. Automatic live previews deliberately do
            // not relayout progress UI for every short-lived sample.
            clearSelectionModifierHintIfShown();
            statusBar()->clearMessage();
        }
        if (lightBakeStatusButton_)
        {
            lightBakeStatusButton_->show();
        }
        if (lightBakeStopButton_)
        {
            lightBakeStopButton_->setEnabled(true);
            lightBakeStopButton_->setText("Stop");
        }
        if (lightBakeProgressLabel_)
        {
            lightBakeProgressLabel_->setText(selectedManualScope
                ? QStringLiteral(
                    "Finding sectors affected by the selected Baked lights...")
                : motsArchRequested && targets.empty()
                    ? QStringLiteral("Calculating MotS Arch lighting...")
                    : QString("Calculating baked lighting for %1 sector(s)...")
                        .arg(static_cast<int>(targets.size())));
        }
    }

    using ResultPtr = std::shared_ptr<LightBakeWorkerResult>;
    auto* watcher = new QFutureWatcher<ResultPtr>(this);
    watcher->setObjectName("lightBakeFutureWatcher");
    lightBakeWatcher_ = watcher;
    lightBakeWaitForFinished_ = [watcher]()
    {
        watcher->waitForFinished();
    };

    QPointer<MainWindow> guardedWindow(this);
    connect(watcher, &QFutureWatcher<ResultPtr>::finished, this,
        [this, watcher, jobSerial]()
        {
            const ResultPtr result = watcher->result();
            watcher->deleteLater();
            if (jobSerial != lightBakeJobSerial_)
            {
                return;
            }
            lightBakeWatcher_ = nullptr;
            lightBakeWaitForFinished_ = {};
            if (!result)
            {
                lightBakeRunning_ = false;
                return;
            }
            finishLightBake(
                std::move(result->level),
                std::move(result->targets),
                result->generation,
                result->sourceFingerprint,
                result->canceled,
                result->automatic,
                result->updateSectorAmbient,
                std::move(result->manualUndoLabel),
                result->motsArchSourceFingerprint,
                result->motsArchRequested,
                result->motsArchSucceeded,
                std::move(result->motsArchMessage),
                std::move(result->changedArchThings),
                std::move(result->resourceSnapshot));
        });

    watcher->setFuture(QtConcurrent::run(QThreadPool::globalInstance(),
        [snapshot = std::move(snapshot),
         targets = std::move(targets),
         generation,
         automatic,
         updateSectorAmbient,
         motsArchRequested,
         motsArchHasSource,
         motsArchSourceFingerprint,
         templateSnapshot = std::move(templateSnapshot),
         resourceSnapshot = std::move(resourceSnapshot),
         manualUndoLabel = std::move(manualUndoLabel),
         dirtyInfluences = std::move(dirtyInfluences),
         permittedTargets = std::move(permittedTargets),
         cancelFlag,
         guardedWindow,
         jobSerial]() mutable -> ResultPtr
        {
            auto result = std::make_shared<LightBakeWorkerResult>();
            result->level = std::move(snapshot);
            result->targets = std::move(targets);
            result->generation = generation;
            result->automatic = automatic;
            result->updateSectorAmbient = updateSectorAmbient;
            result->motsArchRequested = motsArchRequested;
            result->motsArchSourceFingerprint = motsArchSourceFingerprint;
            result->resourceSnapshot = resourceSnapshot;
            result->manualUndoLabel = std::move(manualUndoLabel);

            if (cancelFlag->load(std::memory_order_relaxed))
            {
                result->canceled = true;
                return result;
            }

            if (!dirtyInfluences.empty())
            {
                // Automatic previews and Recalc. Selected both define a dirty
                // influence union. Exact affected-vertex discovery stays on
                // the worker instead of blocking the GUI thread.
                const std::vector<std::int32_t> affected =
                    ui::lighting::lightBakeAffectedSectorTargets(
                        result->level, dirtyInfluences);
                ui::lighting::mergeLightBakeTargets(result->targets, affected);
                std::erase_if(
                    result->targets,
                    [&permittedTargets](const std::int32_t target)
                    {
                        return !std::binary_search(
                            permittedTargets.begin(),
                            permittedTargets.end(),
                            target);
                    });
            }

            if (cancelFlag->load(std::memory_order_relaxed))
            {
                result->canceled = true;
                return result;
            }

            result->sourceFingerprint = ui::lighting::lightBakeSourceFingerprint(
                result->level, result->targets);
            if (motsArchRequested)
            {
                if (motsArchHasSource)
                {
                    const ui::lighting::MotsArchLightingBakeResolveResult resolved =
                        ui::lighting::resolveMotsArchLightingBakeTargets(
                            result->level,
                            &templateSnapshot,
                            *resourceSnapshot,
                            cancelFlag.get());
                    if (resolved.canceled)
                    {
                        result->canceled = true;
                        return result;
                    }
                    if (!resolved.success)
                    {
                        result->motsArchSucceeded = false;
                        result->motsArchMessage = resolved.message;
                        return result;
                    }
                    const level::MotsArchLightingBakeResult archResult =
                        level::levelCalcMotsArchLighting(
                            result->level,
                            resolved.targets,
                            &templateSnapshot,
                            [cancelFlag](const std::size_t, const std::size_t) -> bool
                            {
                                return !cancelFlag->load(std::memory_order_relaxed);
                            });
                    if (archResult.status == level::MotsArchLightingBakeStatus::Cancelled)
                    {
                        result->canceled = true;
                        return result;
                    }
                    if (archResult.status == level::MotsArchLightingBakeStatus::Rejected)
                    {
                        result->motsArchSucceeded = false;
                        result->motsArchMessage = archResult.message;
                        return result;
                    }
                    result->changedArchThings = archResult.changedThingIndices;
                }
                else
                {
                    // Empty-target cleanup restores authored ARCH data from
                    // SED provenance without resolving model resources.
                    const level::MotsArchLightingBakeResult archResult =
                        level::levelCalcMotsArchLighting(
                            result->level, {}, &templateSnapshot,
                            [cancelFlag](const std::size_t, const std::size_t) -> bool
                            {
                                return !cancelFlag->load(std::memory_order_relaxed);
                            });
                    if (archResult.status == level::MotsArchLightingBakeStatus::Cancelled)
                    {
                        result->canceled = true;
                        return result;
                    }
                    if (archResult.status == level::MotsArchLightingBakeStatus::Rejected)
                    {
                        result->motsArchSucceeded = false;
                        result->motsArchMessage = archResult.message;
                        return result;
                    }
                    result->changedArchThings = archResult.changedThingIndices;
                }
            }
            if (result->targets.empty())
            {
                return result;
            }

            std::vector<std::pair<std::int32_t, ColorF>> preservedAmbients{};
            if (!updateSectorAmbient)
            {
                preservedAmbients.reserve(result->targets.size());
                for (const std::int32_t sectorIndex : result->targets)
                {
                    if (sectorIndex >= 0
                        && static_cast<std::size_t>(sectorIndex) < result->level.sectors.size())
                    {
                        preservedAmbients.emplace_back(
                            sectorIndex,
                            result->level.sectors[static_cast<std::size_t>(sectorIndex)].ambient);
                    }
                }
            }

            // Run setup (including light-sector lookup and selective dirty-
            // vertex zeroing) once for the complete affected set. Manual jobs
            // pass an empty dirty span and retain the traditional full bake.
            // The callback keeps cancellation responsive without flooding the
            // GUI event queue with duplicate percentages.
            int lastPostedPercent = -1;
            const bool completed = ::sed::level::levelCalcLightingNewInterruptible(
                result->level,
                result->targets,
                [cancelFlag, guardedWindow, jobSerial, automatic, &lastPostedPercent](
                    const std::size_t done,
                    const std::size_t total) -> bool
                {
                    if (cancelFlag->load(std::memory_order_relaxed))
                    {
                        return false;
                    }
                    if (automatic)
                    {
                        // Automatic live previews are short and frequently
                        // superseded. Queuing percentage updates for every
                        // transient job starved palette mouse/key events.
                        return true;
                    }
                    const int percent = ui::lighting::lightBakeProgressPercent(done, total);
                    if (percent == lastPostedPercent)
                    {
                        return true;
                    }
                    lastPostedPercent = percent;
                    if (QCoreApplication* app = QCoreApplication::instance())
                    {
                        QMetaObject::invokeMethod(app,
                            [guardedWindow, jobSerial, percent]()
                            {
                                if (guardedWindow && guardedWindow->lightBakeJobSerial_ == jobSerial)
                                {
                                    guardedWindow->updateLightBakeProgress(percent);
                                }
                            },
                            Qt::QueuedConnection);
                    }
                    return !cancelFlag->load(std::memory_order_relaxed);
                },
                dirtyInfluences);
            if (completed && !updateSectorAmbient)
            {
                // The core routine deliberately retains Pascal SED's original
                // ambient derivation and vertex-color normalization. The app
                // option separates those outputs by preserving edited Ambient
                // values while retaining the normalized baked vertex colors.
                for (const auto& [sectorIndex, ambient] : preservedAmbients)
                {
                    if (sectorIndex >= 0
                        && static_cast<std::size_t>(sectorIndex) < result->level.sectors.size())
                    {
                        result->level.sectors[static_cast<std::size_t>(sectorIndex)].ambient =
                            ambient;
                    }
                }
            }
            result->canceled = !completed || cancelFlag->load(std::memory_order_relaxed);
            return result;
        }));
}

void MainWindow::updateLightBakeProgress(const int percent)
{
    const int bounded = std::clamp(percent, 0, 100);
    if (lightBakeStatusButton_)
    {
        lightBakeStatusButton_->setText(QString("Lighting %1%").arg(bounded));
    }
    if (lightBakeProgressBar_)
    {
        lightBakeProgressBar_->setValue(bounded);
    }
}

void MainWindow::finishLightBake(
    Level bakedLevel,
    std::vector<std::int32_t> targets,
    const std::uint64_t generation,
    const std::uint64_t sourceFingerprint,
    const bool canceled,
    const bool automatic,
    const bool updateSectorAmbient,
    std::string manualUndoLabel,
    const std::uint64_t motsArchSourceFingerprint,
    const bool motsArchRequested,
    const bool motsArchSucceeded,
    std::string motsArchMessage,
    std::vector<std::int32_t> changedArchThings,
    std::shared_ptr<const assets::ResourceLocator> resourceSnapshot)
{
    lightBakeRunning_ = false;
    lightBakeCancelFlag_.reset();
    lightBakeActiveTargets_.clear();
    lightBakeActiveInfluences_.clear();

    const bool generationMatches = generation == lightBakeSourceGeneration_;
    const bool validateSource = documentEditingAllowed()
        && (!targets.empty() || motsArchRequested)
        && ui::lighting::lightBakeCompletionRequiresSourceValidation(
            canceled, generation, lightBakeSourceGeneration_);
    // Rapid palette movement deliberately cancels most automatic workers.
    // Do not rescan/hash the complete live geometry for a result that cannot
    // possibly publish.
    const bool surfaceSourceMatches = validateSource
        && sourceFingerprint
            == ui::lighting::lightBakeSourceFingerprint(document_.level(), targets);
    const bool resourceSnapshotMatches = !motsArchRequested
        || resourceSnapshot == resourceLocatorSnapshot_;
    const bool motsArchSourceMatches = !motsArchRequested
        || (resourceSnapshotMatches
            && motsArchSourceFingerprint
                == ui::lighting::motsArchLightingBakeSourceFingerprint(
                    document_.level(), &templates_, resourceSnapshot.get()));
    const bool sourceMatches = surfaceSourceMatches && motsArchSourceMatches;
    MotsArchBakePublication archPublication{};
    if (motsArchRequested && motsArchSucceeded && sourceMatches)
    {
        archPublication = prepareMotsArchBakePublication(
            document_.level(), bakedLevel);
    }
    const bool archOutputChanged = motsArchRequested && motsArchSucceeded
        && archPublication.valid && archPublication.changed;
    const std::size_t processedLightCount = !automatic && validateSource && sourceMatches
        ? ui::lighting::processedBakedLightCount(bakedLevel, targets)
        : 0u;
    const bool ambientChanged = validateSource && sourceMatches && updateSectorAmbient
        && std::ranges::any_of(
            targets,
            [this, &bakedLevel](const std::int32_t sectorIndex)
            {
                return sectorIndex >= 0
                    && static_cast<std::size_t>(sectorIndex) < document_.level().sectors.size()
                    && static_cast<std::size_t>(sectorIndex) < bakedLevel.sectors.size()
                    && document_.level().sectors[static_cast<std::size_t>(sectorIndex)].ambient
                        != bakedLevel.sectors[static_cast<std::size_t>(sectorIndex)].ambient;
            });
    bool applied = false;

    if ((!targets.empty() || archOutputChanged)
        && validateSource && sourceMatches
        && (!motsArchRequested || (motsArchSucceeded && archPublication.valid)))
    {
        constexpr editor::DocumentDirty dirty =
            editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Things;
        if (automatic)
        {
            // The originating live Light edit already owns the undo record.
            // Intermediate automatic publications are derived output; adding
            // a full DocumentState snapshot every 75 ms both bloated history
            // and stalled the GUI thread on large levels.
            const bool surfaceApplied = targets.empty()
                || ui::lighting::applyLightBakeResult(
                    document_.level(), bakedLevel, targets, updateSectorAmbient);
            if (surfaceApplied && archOutputChanged)
            {
                commitMotsArchBakePublication(document_.level(), archPublication);
            }
            applied = surfaceApplied && (archOutputChanged || !targets.empty());
        }
        else
        {
            applied = document_.applyUndoable(
                manualUndoLabel.empty()
                    ? std::string("Baked Lighting: Affected area recalculated")
                    : std::move(manualUndoLabel),
                dirty,
                [&bakedLevel, &targets, updateSectorAmbient, archOutputChanged,
                 &archPublication](editor::DocumentState& state) -> bool
                {
                    const bool surfaceApplied = targets.empty()
                        || ui::lighting::applyLightBakeResult(
                            state.level, bakedLevel, targets, updateSectorAmbient);
                    if (!surfaceApplied)
                    {
                        return false;
                    }
                    if (archOutputChanged)
                    {
                        commitMotsArchBakePublication(state.level, archPublication);
                    }
                    return archOutputChanged || !targets.empty();
                });
        }

        if (applied)
        {
            if (automatic)
            {
                // Preserve one user-facing history item for the source edit,
                // but rotate its identity so Save/dirty tracking observes
                // this later publication of persisted derived output.
                (void)document_.touchLatestUndoRecord();
            }
            if (glView_)
            {
                std::vector<std::int32_t> redrawSectors = targets;
                for (const std::int32_t thingIndex : changedArchThings)
                {
                    if (thingIndex >= 0
                        && static_cast<std::size_t>(thingIndex) < document_.level().things.size())
                    {
                        redrawSectors.push_back(
                            document_.level().things[static_cast<std::size_t>(thingIndex)].sector);
                    }
                }
                ui::lighting::mergeLightBakeTargets(redrawSectors, {});
                glView_->notifyBakedLightingChanged(
                    redrawSectors, ambientChanged && !archOutputChanged);
                if (archOutputChanged)
                {
                    glView_->notifyThingAppearanceChanged();
                }
            }
            if (ambientChanged
                && std::ranges::any_of(
                    targets,
                    [this](const std::int32_t sectorIndex)
                    {
                        return document_.selection().sectors.contains(sectorIndex);
                    }))
            {
                rebuildProperties();
            }
            updateEditActions();
            if (!automatic)
            {
                updateLightBakeProgress(100);
            }
        }
    }

    // Remove the in-progress text before publishing any completion message;
    // QStatusBar otherwise briefly lays out both labels in its left slot.
    if (!automatic && lightBakeStatusButton_)
    {
        lightBakeStatusButton_->hide();
    }

    if (!automatic && applied && statusBar())
    {
        statusBar()->showMessage(
            targets.empty() && archOutputChanged
                ? QString("MotS Arch lighting recalculated for %1 Thing(s).")
                    .arg(static_cast<qulonglong>(changedArchThings.size()))
                : QString("Lighting recalculated for %1 sector(s); %2 light(s) considered.")
                    .arg(static_cast<qulonglong>(targets.size()))
                    .arg(static_cast<qulonglong>(processedLightCount)),
            3500);
    }

    if (!automatic && !applied && !canceled && statusBar())
    {
        statusBar()->showMessage(
            !motsArchSucceeded && !motsArchMessage.empty()
                ? QString::fromStdString(motsArchMessage)
                : sourceMatches && generationMatches
                    ? QStringLiteral("Lighting recalculation could not be applied.")
                    : QStringLiteral("Lighting changed while calculating; stale results were discarded."),
            3500);
    }
    else if (!automatic && canceled && statusBar())
    {
        statusBar()->showMessage("Lighting recalculation stopped.", 2500);
    }

    if (!automatic && lightBakeProgressDialog_)
    {
        lightBakeProgressDialog_->hide();
    }

    const bool restartManual = lightBakePendingManual_;
    std::vector<std::int32_t> restartManualTargets =
        std::move(lightBakePendingManualTargets_);
    std::vector<std::int32_t> restartManualPermittedTargets =
        std::move(lightBakePendingManualPermittedTargets_);
    std::vector<level::LightingInfluenceSphere>
        restartManualInfluences =
            std::move(lightBakePendingManualInfluences_);
    std::string restartManualUndoLabel =
        std::move(lightBakePendingManualUndoLabel_);
    const bool restartManualUpdateAmbient =
        lightBakePendingManualUpdateAmbient_;
    lightBakePendingManualTargets_.clear();
    lightBakePendingManualPermittedTargets_.clear();
    lightBakePendingManualInfluences_.clear();
    lightBakePendingManualUndoLabel_.clear();
    lightBakePendingManualUpdateAmbient_ = false;
    const bool restartAuto = lightBakePendingAuto_
        && lightBakeAutoButton_
        && lightBakeAutoButton_->isChecked()
        && !lightBakeTransformPreviewBaseline_;
    lightBakePendingManual_ = false;
    lightBakePendingManualSelectedOnly_ = false;
    lightBakePendingAuto_ = false;

    if (restartManual)
    {
        QTimer::singleShot(
            0,
            this,
            [this,
             restartManualTargets = std::move(restartManualTargets),
             restartManualInfluences = std::move(restartManualInfluences),
             restartManualPermittedTargets =
                 std::move(restartManualPermittedTargets),
             restartManualUndoLabel =
                 std::move(restartManualUndoLabel),
             restartManualUpdateAmbient]() mutable
            {
                startLightBake(
                    std::move(restartManualTargets),
                    false,
                    std::move(restartManualInfluences),
                    std::move(restartManualPermittedTargets),
                    std::move(restartManualUndoLabel),
                    restartManualUpdateAmbient);
            });
    }
    else if (restartAuto && (!lightBakeDebounceTimer_ || !lightBakeDebounceTimer_->isActive()))
    {
        QTimer::singleShot(0, this, [this]()
            {
                requestLightBake(true);
            });
    }
}

void MainWindow::cancelLightBake(const bool discardPending)
{
    if (discardPending)
    {
        lightBakePendingAuto_ = false;
        lightBakePendingManual_ = false;
        lightBakePendingManualSelectedOnly_ = false;
        lightBakePendingManualTargets_.clear();
        lightBakePendingManualPermittedTargets_.clear();
        lightBakePendingManualInfluences_.clear();
        lightBakePendingManualUndoLabel_.clear();
        lightBakePendingManualUpdateAmbient_ = false;
        lightBakePendingAutoTargets_.clear();
        lightBakePendingAutoInfluences_.clear();
        if (lightBakeDebounceTimer_)
        {
            lightBakeDebounceTimer_->stop();
        }
    }
    if (!lightBakeRunning_ || !lightBakeCancelFlag_)
    {
        return;
    }

    lightBakeCancelFlag_->store(true, std::memory_order_relaxed);
    if (lightBakeProgressLabel_)
    {
        lightBakeProgressLabel_->setText("Stopping safely after the current sector...");
    }
    if (lightBakeStopButton_)
    {
        lightBakeStopButton_->setEnabled(false);
        lightBakeStopButton_->setText("Stopping...");
    }
    if (lightBakeStatusButton_)
    {
        lightBakeStatusButton_->setText("Lighting: stopping");
    }
}

void MainWindow::showLightBakeProgressDialog()
{
    if (!lightBakeProgressDialog_ || !lightBakeRunning_)
    {
        return;
    }
    lightBakeProgressDialog_->show();
    lightBakeProgressDialog_->raise();
    lightBakeProgressDialog_->activateWindow();
}

} // namespace sed
