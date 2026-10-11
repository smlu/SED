#include "main_window/common.hpp"

#include "ui/layers/layer_color_utils.hpp"

namespace sed
{

void MainWindow::resetLayerStatesFromLevel(const bool rebuildViewportMeshes)
{
    // Step 1: restore persisted visibility and reset transient lock state.
    layerVisualHistoryByAfterSignature_.clear();
    layerVisualHistoryAfterByBeforeSignature_.clear();
    const std::size_t layerCount = document_.level().layers.size();
    layerVisible_ = ui::layers::initialLayerVisibility(layerCount, document_.level().lvisstring);
    layerLocked_.assign(ui::layers::normalizedLayerCount(layerCount), false);
    thingPlacementLayerOverride_ = -1;

    // Step 2: repair vector sizes/colors, then push the state to the viewport.
    normalizeLayerVisualState();
    syncLayerViewState(rebuildViewportMeshes);
}

void MainWindow::resetLayerColorsToDefaults()
{
    layerColors_.clear();
    normalizeLayerVisualState();
}

std::string MainWindow::buildLayerVisibilityString() const
{
    return ui::layers::buildLayerVisibilityString(document_.level().layers.size(), layerVisible_);
}

void MainWindow::normalizeLayerVisualState()
{
    ui::layers::normalizeLayerVisualState(document_.level().layers.size(), layerVisible_, layerLocked_, layerColors_);
}

void MainWindow::persistLayerColors() const
{
    QSettings settings = settingsStore();
    QStringList storedColors{};
    storedColors.reserve(static_cast<int>(layerColors_.size()));
    for (const QColor& color : layerColors_)
    {
        storedColors.push_back(color.isValid() ? color.name(QColor::HexRgb) : QString{});
    }
    settings.setValue("viewport/layers/colors", storedColors);
}

MainWindow::LayerVisualHistoryState MainWindow::captureLayerVisualHistoryState() const
{
    LayerVisualHistoryState state{ layerVisible_, layerLocked_, layerColors_ };
    ui::layers::normalizeLayerVisualState(
        document_.level().layers.size(),
        state.visible,
        state.locked,
        state.colors);
    return state;
}

std::uint64_t MainWindow::layerVisualHistoryTransitionOwnedBytes(
    const LayerVisualHistoryState& before,
    const LayerVisualHistoryState& after) const noexcept
{
    std::uint64_t bytes = sizeof(LayerVisualHistoryTransition) + 64u;
    const auto add = [&bytes](const std::uint64_t count,
                              const std::uint64_t elementBytes) noexcept
    {
        if (elementBytes != 0u
            && count > (std::numeric_limits<std::uint64_t>::max() - bytes)
                / elementBytes)
        {
            bytes = std::numeric_limits<std::uint64_t>::max();
            return;
        }
        bytes += count * elementBytes;
    };
    const auto bitVectorBytes = [](const std::vector<bool>& values) noexcept
    {
        return static_cast<std::uint64_t>(values.capacity() / 8u
            + (values.capacity() % 8u != 0u ? 1u : 0u));
    };
    add(bitVectorBytes(before.visible), 1u);
    add(bitVectorBytes(before.locked), 1u);
    add(before.colors.capacity(), sizeof(QColor));
    add(bitVectorBytes(after.visible), 1u);
    add(bitVectorBytes(after.locked), 1u);
    add(after.colors.capacity(), sizeof(QColor));
    return bytes;
}

void MainWindow::applyLayerVisualHistoryState(LayerVisualHistoryState state)
{
    layerVisible_ = std::move(state.visible);
    layerLocked_ = std::move(state.locked);
    layerColors_ = std::move(state.colors);
    normalizeLayerVisualState();
}

void MainWindow::recordLayerVisualHistoryTransition(
    const std::uint64_t beforeSignature,
    LayerVisualHistoryState before)
{
    const std::uint64_t afterSignature = document_.undoSignature();
    if (beforeSignature == afterSignature)
    {
        return;
    }

    LayerVisualHistoryTransition transition{};
    if (const auto identity = document_.historyTransitionIdentityAtDepth(
            document_.undoDepth()))
    {
        transition.recordId = identity->recordId;
    }
    transition.beforeSignature = beforeSignature;
    transition.afterSignature = afterSignature;
    transition.before = std::move(before);
    transition.after = captureLayerVisualHistoryState();
    if (transition.recordId == 0u
        || !canFitCleaveWorkspaceAttachmentBudget(
            layerVisualHistoryTransitionOwnedBytes(
                transition.before, transition.after),
            true))
    {
        return;
    }
    if (const auto previous = layerVisualHistoryAfterByBeforeSignature_.find(beforeSignature);
        previous != layerVisualHistoryAfterByBeforeSignature_.end()
        && previous->second != afterSignature)
    {
        layerVisualHistoryByAfterSignature_.erase(previous->second);
    }
    layerVisualHistoryAfterByBeforeSignature_[beforeSignature] = afterSignature;
    layerVisualHistoryByAfterSignature_[afterSignature] = std::move(transition);
}

void MainWindow::publishPreallocatedLayerVisualHistoryTransition(
    const std::uint64_t beforeSignature,
    const std::uint64_t afterSignature,
    const std::uint64_t recordId,
    LayerVisualHistoryByAfter& stagedByAfter,
    LayerVisualHistoryAfterByBefore& stagedAfterByBefore)
{
    if (recordId == 0u || beforeSignature == afterSignature)
    {
        return;
    }
    auto transitionNode = stagedByAfter.extract(0u);
    auto reverseNode = stagedAfterByBefore.extract(0u);
    if (transitionNode.empty() || reverseNode.empty())
    {
        return;
    }

    // Remove only the superseded branch mappings. Both new nodes were fully
    // allocated before the Document commit and both destination maps were
    // reserved, so publishing the exact post-commit RecordID cannot allocate.
    if (const auto previous = layerVisualHistoryAfterByBeforeSignature_.find(
            beforeSignature);
        previous != layerVisualHistoryAfterByBeforeSignature_.end())
    {
        if (previous->second != afterSignature)
        {
            layerVisualHistoryByAfterSignature_.erase(previous->second);
        }
        layerVisualHistoryAfterByBeforeSignature_.erase(previous);
    }
    if (const auto previous = layerVisualHistoryByAfterSignature_.find(afterSignature);
        previous != layerVisualHistoryByAfterSignature_.end())
    {
        const std::uint64_t previousBefore = previous->second.beforeSignature;
        if (const auto reverse = layerVisualHistoryAfterByBeforeSignature_.find(
                previousBefore);
            reverse != layerVisualHistoryAfterByBeforeSignature_.end()
            && reverse->second == afterSignature)
        {
            layerVisualHistoryAfterByBeforeSignature_.erase(reverse);
        }
        layerVisualHistoryByAfterSignature_.erase(previous);
    }

    transitionNode.key() = afterSignature;
    transitionNode.mapped().recordId = recordId;
    transitionNode.mapped().beforeSignature = beforeSignature;
    transitionNode.mapped().afterSignature = afterSignature;
    reverseNode.key() = beforeSignature;
    reverseNode.mapped() = afterSignature;
    layerVisualHistoryByAfterSignature_.insert(std::move(transitionNode));
    layerVisualHistoryAfterByBeforeSignature_.insert(std::move(reverseNode));
}

bool MainWindow::restoreLayerVisualHistoryForUndo(
    const std::uint64_t fromSignature,
    const std::uint64_t toSignature)
{
    const auto transition = layerVisualHistoryByAfterSignature_.find(fromSignature);
    if (transition == layerVisualHistoryByAfterSignature_.end()
        || transition->second.beforeSignature != toSignature)
    {
        return false;
    }

    applyLayerVisualHistoryState(transition->second.before);
    return true;
}

bool MainWindow::restoreLayerVisualHistoryForRedo(
    const std::uint64_t fromSignature,
    const std::uint64_t toSignature)
{
    const auto after = layerVisualHistoryAfterByBeforeSignature_.find(fromSignature);
    if (after == layerVisualHistoryAfterByBeforeSignature_.end()
        || after->second != toSignature)
    {
        return false;
    }

    const auto transition = layerVisualHistoryByAfterSignature_.find(toSignature);
    if (transition == layerVisualHistoryByAfterSignature_.end()
        || transition->second.beforeSignature != fromSignature)
    {
        return false;
    }

    applyLayerVisualHistoryState(transition->second.after);
    return true;
}

void MainWindow::refreshLayerVisualHistoryForCurrentSignature()
{
    const std::uint64_t signature = document_.undoSignature();
    const LayerVisualHistoryState current = captureLayerVisualHistoryState();

    if (auto transition = layerVisualHistoryByAfterSignature_.find(signature);
        transition != layerVisualHistoryByAfterSignature_.end())
    {
        transition->second.after = current;
    }

    if (const auto after = layerVisualHistoryAfterByBeforeSignature_.find(signature);
        after != layerVisualHistoryAfterByBeforeSignature_.end())
    {
        if (auto transition = layerVisualHistoryByAfterSignature_.find(after->second);
            transition != layerVisualHistoryByAfterSignature_.end())
        {
            transition->second.before = current;
        }
    }
}

void MainWindow::syncLayerViewState(const bool rebuildViewportMeshes)
{
    normalizeLayerVisualState();
    document_.level().lvisstring = buildLayerVisibilityString();
    refreshLayerVisualHistoryForCurrentSignature();
    refreshThingPlacementLayerControl();

    if (!glView_)
    {
        return;
    }

    glView_->setLayerColors(layerColors_);
    glView_->setLayerWireframeColoringEnabled(wireframeColorByLayer_);
    const std::unordered_set<std::int32_t> hiddenLayers =
        ui::layers::hiddenLayersFromVisibility(layerVisible_);
    std::unordered_set<std::int32_t> lockedLayers{};
    for (std::size_t layer = 0; layer < layerLocked_.size(); ++layer)
    {
        if (layerLocked_[layer])
        {
            lockedLayers.insert(static_cast<std::int32_t>(layer));
        }
    }
    bool progressShown = false;
    QProgressBar* progressBar = nullptr;
    QElapsedTimer progressElapsed{};
    progressElapsed.start();
    constexpr qint64 kDefaultProgressDelayMs = 500;
    setProperty(
        "sed.layerVisibilityProgressDefaultDelayMs",
        kDefaultProgressDelayMs);
    const QVariant configuredDelay = property(
        "sed.layerVisibilityProgressDelayMs");
    const qint64 progressDelayMs = configuredDelay.isValid()
        ? std::max<qint64>(0, configuredDelay.toLongLong())
        : kDefaultProgressDelayMs;
    const auto progressPulse =
        [this, &progressShown, &progressBar, &progressElapsed,
         progressDelayMs]()
    {
        if (!progressShown
            && progressElapsed.elapsed() >= progressDelayMs)
        {
            progressShown = true;
            const qulonglong count = property(
                "sed.layerVisibilityProgressShowCount").toULongLong();
            setProperty(
                "sed.layerVisibilityProgressShowCount", count + 1u);
            setProperty("sed.layerVisibilityProgressVisible", true);
            const QString progressText =
                QStringLiteral("Updating layer visibility\u2026");
            const QString progressToolTip = QStringLiteral(
                "Progress updates between safe stages. A graphics-driver "
                "allocation cannot update again until that driver call returns.");
            setProperty(
                "sed.layerVisibilityProgressLastText", progressText);
            setProperty(
                "sed.layerVisibilityProgressLastToolTip",
                progressToolTip);
            progressBar = new QProgressBar(statusBar());
            progressBar->setObjectName(
                QStringLiteral("layerVisibilityProgress"));
            progressBar->setRange(0, 0);
            progressBar->setTextVisible(true);
            progressBar->setFormat(progressText);
            progressBar->setToolTip(progressToolTip);
            progressBar->setMinimumWidth(220);
            progressBar->setMaximumWidth(320);
            statusBar()->addPermanentWidget(progressBar);
            progressBar->show();
        }
        if (progressBar)
        {
            // repaint() is a synchronous paint of this one progress widget; it
            // does not pump timers/input or permit document mutation to
            // re-enter the in-progress mesh publication.
            progressBar->repaint();
        }
    };
    const auto restoreProgress =
        [this, &progressShown, &progressBar]()
    {
        if (!progressShown)
        {
            return;
        }
        setProperty("sed.layerVisibilityProgressVisible", false);
        if (progressBar)
        {
            statusBar()->removeWidget(progressBar);
            delete progressBar;
            progressBar = nullptr;
        }
    };

    try
    {
        glView_->setLayerVisibilityState(
            hiddenLayers,
            previewRespectHiddenLayers_,
            rebuildViewportMeshes
                ? GlView::LayerMeshPublication::rebuild
                : GlView::LayerMeshPublication::defer,
            rebuildViewportMeshes
                ? GlView::LayerMeshProgressPulse(progressPulse)
                : GlView::LayerMeshProgressPulse{});
        glView_->setLockedLayers(std::move(lockedLayers));
    }
    catch (...)
    {
        restoreProgress();
        throw;
    }
    restoreProgress();
}

bool MainWindow::isLayerLocked(const std::int32_t layer) const noexcept
{
    return ui::layers::isLayerLocked(layerLocked_, layer);
}

bool MainWindow::selectionTouchesLockedLayer(const editor::Selection &selection) const
{
    return ui::layers::selectionTouchesLockedLayer(document_.level(), selection, layerLocked_);
}

bool MainWindow::selectionTouchesHiddenLayer(const editor::Selection &selection) const
{
    return ui::layers::selectionTouchesHiddenLayer(document_.level(), selection, layerVisible_);
}

bool MainWindow::selectionTouchesNonEditableLayer(const editor::Selection &selection) const
{
    return ui::layers::selectionTouchesNonEditableLayer(
        document_.level(), selection, layerVisible_, layerLocked_);
}

} // namespace sed
