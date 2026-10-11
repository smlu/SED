#include "main_window/common.hpp"

namespace sed
{

void MainWindow::resetSelectionContextState()
{
    if (glView_)
    {
        // Cancel before tool teardown can settle a draft belonging to the
        // document being replaced, including replacements at the same address.
        (void)glView_->cancelStagedCleaveWheelSession();
        glView_->cancelViewportMouseGesture();
    }
    flyHeldKeys_.clear();
    // A Level replacement does not necessarily change the address of the
    // Document's Level object.  GlView therefore cannot use its pointer-change
    // guard to clear interaction state for Open/New.  Tear down every pointer
    // owner explicitly before publishing the new selection context; otherwise
    // a hidden Multi-cut/Cleave or Sector Thrust flag can survive the load even
    // when its toolbar action is no longer checked.
    // A transition caller finalizes a completed brush gesture before replacing
    // the document.  If this function is used directly during state restore,
    // cancel an in-progress gesture instead of creating a new undo record.
    thingPlacementBrushStrokeActive_ = false;
    thingPlacementBrushStrokeHaveLast_ = false;
    thingPlacementBrushStrokePlaced_ = false;
    thingPlacementBrushStrokeUndoPushed_ = false;
    thingPlacementBrushStrokeFirstThing_ = 0;
    thingPlacementBrushStrokeLastPoint_ = {};
    resetUvEditorWorkspaceState();
    deactivateViewportEditFunctions();
    if (placementMode_ != PlacementMode::none)
    {
        setPlacementMode(PlacementMode::none);
    }
    if (sectorJoinPreviewActive_)
    {
        cancelSectorJoinPreview();
    }
    if (surfaceExtrudePreviewActive_)
    {
        cancelSurfaceExtrudePreview();
    }

    // Keep QAction state and GlView state in lockstep without allowing a
    // teardown signal to resurrect the remembered transform tool.
    if (viewportEditToolsGroup_)
    {
        for (QAction* action : viewportEditToolsGroup_->actions())
        {
            if (!action || !action->isCheckable())
            {
                continue;
            }
            const QSignalBlocker blocker(action);
            action->setChecked(false);
        }
    }
    setViewportTransformActionChecks(nullptr);
    rememberedViewportTransformAction_ = nullptr;
    transformPreview_ = {};

    editMode_ = EditMode::none;
    surfaceSelectionMode_ = SurfaceSelectionMode::face;
    lightEditSubMode_ = LightEditSubMode::baked;
    const auto setChecked = [](QAction* action, const bool checked)
    {
        if (!action)
        {
            return;
        }
        const QSignalBlocker blocker(action);
        action->setChecked(checked);
    };
    setChecked(editSectorAction_, false);
    setChecked(editSurfaceAction_, false);
    setChecked(editThingAction_, false);
    setChecked(editLightAction_, false);
    setChecked(editFrameAction_, false);
    setChecked(lightEditBakedAction_, true);
    setChecked(lightEditThingAction_, false);
    setChecked(lightEditSectorAction_, false);
    setChecked(lightEditArchitecturalAction_, false);
    clearArchitecturalLightingInteractiveState();
    setChecked(surfaceFaceAction_, false);
    setChecked(surfaceEdgeAction_, false);
    setChecked(surfaceVertexAction_, false);

    placementMode_ = PlacementMode::none;
    sectorJoinPreviewActive_ = false;
    sectorJoinPreviewSelection_.clear();
    surfaceExtrudePreviewActive_ = false;

    cancelSectorDrawPlacement();

    document_.selection().clear();
    lastSectorSelection_.clear();
    lastSurfaceSelection_.clear();
    rememberedSurfaceFaceSelection_.clear();
    lastThingSelection_.clear();
    lastLightSelection_.clear();
    lastLightThingSelection_.clear();
    lastLightSectorSelection_.clear();
    strictSurfaceSelectionScope_.clear();

    haveLastSurfaceClick_ = false;
    lastSurfaceClickSurface_ = {};
    lastSurfaceClickPos_ = {};
    lastViewportClickSector_ = -1;
    lastViewportClickSurface_ = -1;
    lastViewportClickThing_ = -1;
    lastViewportClickLight_ = -1;

    cogEntityPickActive_ = false;
    cogEntityPickCogIndex_ = -1;
    cogEntityPickValueIndex_ = -1;
    cogEntityPickType_ = CogType::int_;
    cogEntityPickRestorePlacementMode_ = PlacementMode::none;

    cogEntityNavigatePending_ = false;
    cogEntityNavigatePendingKind_ = OutlinerKind::none;
    cogEntityNavigatePendingIndex_ = -1;
    cogEntityNavigatePendingSector_ = -1;
    cogEntityNavigatePendingSurface_ = -1;

    if (!glView_)
    {
        return;
    }

    // These flags are intentionally reset directly as well as through the
    // QAction path above.  This covers payloads produced by older versions and
    // prevents stale hidden interaction from leaking into runtime-format loads
    // or New Project.
    glView_->setSectorCleaveToolEnabled(false);
    glView_->setSurfaceCleaveToolEnabled(false);
    glView_->setSectorThrustToolEnabled(false);
    glView_->resetStagedCleaveWorkspaces();
    glView_->setFrameEditEnabled(false);
    glView_->setTransformInteractionEnabled(false);
    glView_->setTransformTool(GlView::TransformTool::Select);
    glView_->setPlacementGestureEnabled(false);
    glView_->setViewportPickingEnabled(false);
    glView_->setArchitecturalLightingTargetMode(
        GlView::ArchitecturalLightingTargetMode::Owner);
    glView_->clearArchitecturalLightingSelectedVertices();
    glView_->setSurfaceClickSelectionEnabled(false);
    glView_->setStrictSurfaceSelectionScope(false, {});
    glView_->setHoverPickEnabled(false);
    glView_->setCogEntityPickHighlightEnabled(false);
    glView_->unsetCursor();
    glView_->clearFrameSelection();
    glView_->setSelectionState(-1, -1, -1, -1, document_.selection());
}

void MainWindow::invalidateSurfaceSelectionContextForTopologyEdit()
{
    // Positional surface ordinals can name a different face after structural
    // geometry changes, including when an earlier sector is removed. Keep the
    // operation's committed face selection, but discard the previous frozen
    // scope and mode cache before changing sub-mode.
    strictSurfaceSelectionScope_.clear();
    rememberedSurfaceFaceSelection_ = document_.selection().surfaces;
    lastSurfaceSelection_.clear();
    document_.selection().surfaceVertices.clear();

    // Keep the application-wide Strict preference intact while returning to
    // the usable face picker for the next Surface edit.
    setSurfaceSelectionMode(SurfaceSelectionMode::face);
}

} // namespace sed
