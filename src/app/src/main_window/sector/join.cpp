#include "main_window/common.hpp"

#include "ui/sector/sector_join_utils.hpp"
#include "ui/selection/selection_tool_availability_utils.hpp"

#include <QDebug>

namespace sed
{
namespace
{

level::SectorConnectKnifeMode sectorJoinModeFromButton(const QToolButton* button) noexcept
{
    return (button == nullptr || button->isChecked())
        ? level::SectorConnectKnifeMode::KeepTargetPieces
        : level::SectorConnectKnifeMode::CutTargetHolesOnly;
}

} // namespace

void MainWindow::beginSectorJoinPreview()
{
    if (!glView_ || !documentEditingAllowed())
    {
        return;
    }

    if (!ui::selection::canJoinSectorSelection(document_.selection()))
    {
        if (viewportSectorConnectAction_)
        {
            const QSignalBlocker blocker(viewportSectorConnectAction_);
            viewportSectorConnectAction_->setChecked(false);
        }
        glView_->showToast("Join sectors: select 2 or more sectors only", 2500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
        return;
    }

    if (placementMode_ == PlacementMode::thing)
    {
        deselectThingPlacementTemplate();
    }
    else if (placementMode_ != PlacementMode::none)
    {
        setPlacementMode(PlacementMode::none);
    }
    deactivateViewportEditFunctions();
    deactivateViewportTransformForExclusiveTool();

    sectorJoinPreviewActive_ = true;
    sectorJoinPreviewSelection_ = document_.selection();
    if (viewportSectorConnectAction_)
    {
        const QSignalBlocker blocker(viewportSectorConnectAction_);
        viewportSectorConnectAction_->setChecked(true);
    }

    glView_->setPlacementPreviewPersistent(true);
    syncSectorJoinOptionsOverlay();
    updateSectorJoinPreview();

    if (statusBar())
    {
        statusBar()->showMessage("Join sectors", 2500);
    }
}

void MainWindow::updateSectorJoinPreview()
{
    if (!sectorJoinPreviewActive_ || !glView_)
    {
        return;
    }

    bool idExhausted = false;
    const std::optional<ui::sector::SectorJoinPreviewResult> preview =
        ui::sector::buildSectorJoinPreview(
            document_.level(),
            document_.selection(),
            glView_->activeSectorIndex(),
            sectorJoinModeFromButton(viewportSectorJoinKeepPiecesButton_),
            &idExhausted);

    if ( !preview )
    {
        qDebug().noquote()
            << "Join sectors preview rejected:"
            << (idExhausted ? "entity ID space exhausted" : "no valid convex/adjoin result")
            << "for" << static_cast<qulonglong>(
                document_.selection().sectors.size())
            << "selected sectors; active sector"
            << glView_->activeSectorIndex() << "; mode"
            << static_cast<int>(sectorJoinModeFromButton(
                viewportSectorJoinKeepPiecesButton_));
        glView_->clearPlacementPreviewLines();
        glView_->showToast(
            idExhausted
                ? "Join sectors: entity ID space exhausted"
                : "Join sectors failed for current selection",
            2500,
            GlView::ToastPlacement::BottomCenter,
            GlView::ToastGroup::Error);
        if ( statusBar() && idExhausted )
        {
            statusBar()->showMessage(
                "Join sectors: entity ID space exhausted", 3500);
        }
        return;
    }

    std::vector<QVector3D> lines = ui::sector::buildSectorJoinPreviewLines(*preview);
    if (lines.empty())
    {
        glView_->clearPlacementPreviewLines();
        return;
    }

    glView_->setPlacementPreviewLines(std::move(lines));
}

void MainWindow::cancelSectorJoinPreview()
{
    sectorJoinPreviewActive_ = false;
    sectorJoinPreviewSelection_.clear();
    if (viewportSectorConnectAction_)
    {
        const QSignalBlocker blocker(viewportSectorConnectAction_);
        viewportSectorConnectAction_->setChecked(false);
    }
    if (sectorJoinOptionsOverlay_)
    {
        sectorJoinOptionsOverlay_->hide();
    }
    if (glView_)
    {
        glView_->setPlacementPreviewPersistent(false);
        glView_->clearPlacementPreviewLines();
    }
    restoreViewportTransformAfterExclusiveTool();
}

void MainWindow::applySectorJoinPreview()
{
    if (!sectorJoinPreviewActive_)
    {
        beginSectorJoinPreview();
        return;
    }

    const level::SectorConnectKnifeMode mode = sectorJoinModeFromButton(viewportSectorJoinKeepPiecesButton_);
    std::int32_t activeSector = glView_ ? glView_->activeSectorIndex() : -1;
    editor::Selection appliedSelection{};
    bool idExhausted = false;

    const bool applied = document_.applyUndoable(
        "Sectors: Joined",
        editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure,
        [mode, &activeSector, &appliedSelection, &idExhausted](editor::DocumentState& state) -> bool
        {
            const std::optional<ui::sector::SectorJoinPreviewResult> preview =
                ui::sector::buildSectorJoinPreview(
                    state.level,
                    state.selection,
                    activeSector,
                    mode,
                    &idExhausted);
            if ( !preview )
            {
                return false;
            }

            state.level = preview->level;
            state.selection = preview->selection;
            activeSector = preview->activeSector;
            appliedSelection = preview->selection;
            return true;
        });

    if ( !applied )
    {
        if ( idExhausted )
        {
            if ( glView_ )
            {
                glView_->showToast(
                    "Join sectors: entity ID space exhausted",
                    2500,
                    GlView::ToastPlacement::BottomCenter,
                    GlView::ToastGroup::Error);
            }
            if ( statusBar() )
            {
                statusBar()->showMessage(
                    "Join sectors: entity ID space exhausted", 3500);
            }
            updateSectorJoinPreview();
            return;
        }
        qDebug().noquote()
            << "Join sectors apply rejected: the selected geometry no longer"
            << "produces a valid convex/adjoin-safe result; selected sectors"
            << static_cast<qulonglong>(
                document_.selection().sectors.size());
        QMessageBox::warning(this, "Join sectors failed", "Operation not permitted for current selection.");
        updateSectorJoinPreview();
        return;
    }

    invalidateSurfaceSelectionContextForTopologyEdit();

    cancelSectorJoinPreview();

    if (glView_)
    {
        const bool hadPendingCuts =
            !glView_->stagedCleaveDraftView(GlView::CleaveWorkspaceFamily::Sector).lines.empty()
            || !glView_->stagedCleaveDraftView(GlView::CleaveWorkspaceFamily::Surface).lines.empty();
        glView_->cancelAllStagedCleaveWorkspaces();
        if (hadPendingCuts)
        {
            glView_->showToast(
                "Multi-cut: level topology changed; the pending draft was cleared",
                3200, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
        }
        glView_->notifyLevelGeometryChanged();
        glView_->setSelectionState(activeSector, -1, -1, -1, appliedSelection);
    }
    rebuildOutliner();
    rebuildProperties();
    updateEditActions();
    scheduleRebuildChecks(editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure);
}

} // namespace sed
