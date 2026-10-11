#include "main_window/common.hpp"

#include "ui/layers/layer_color_utils.hpp"
#include "ui/selection/selection_tool_availability_utils.hpp"

#include <QFontMetrics>
#include <QStyle>

#include <algorithm>

namespace sed
{

namespace
{

void clearCleaveAfterExternalTopologyChange(GlView& view)
{
    const auto sector = view.stagedCleaveDraftView(GlView::CleaveWorkspaceFamily::Sector);
    const auto surface = view.stagedCleaveDraftView(GlView::CleaveWorkspaceFamily::Surface);
    const bool hadPendingCuts = !sector.lines.empty() || !surface.lines.empty()
        || (sector.context && sector.context->valid)
        || (surface.context && surface.context->valid);
    // Empty current drafts can still own a retained Undo timeline in either
    // family. Clear both after success, before publishing reindexed geometry.
    view.cancelAllStagedCleaveWorkspaces();
    if ( hadPendingCuts )
    {
        view.showToast(
            "Multi-cut: level topology changed; the pending drafts were cleared",
            3200, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
    }
}

} // namespace

void MainWindow::setupViewportToolsOverlay()
{
    if (!glView_)
    {
        return;
    }

    viewportToolsOverlay_ = new QWidget(glView_);
    viewportToolsOverlay_->setAttribute(Qt::WA_StyledBackground, true);
    viewportToolsOverlay_->setObjectName("viewportToolsOverlay");

    auto* layout = new QVBoxLayout(viewportToolsOverlay_);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(5);

    viewportEditToolsGroup_ = new QActionGroup(viewportToolsOverlay_);
    // Allow deactivating a tool by clicking it again; we enforce "only one active" manually.
    viewportEditToolsGroup_->setExclusive(false);

    connect(viewportEditToolsGroup_, &QActionGroup::triggered, this, [this](QAction* action)
        {
            if (!action || !action->isCheckable() || !viewportEditToolsGroup_)
            {
                return;
            }

            if (!action->isChecked())
            {
                QTimer::singleShot(0, this, [this]()
                    {
                        restoreViewportTransformAfterExclusiveTool();
                    });
                return;
            }

            // Transform, placement, and click-tools all own the same viewport
            // pointer interaction. Transfer ownership before activating the
            // requested click-tool so toolbar state and GlView behavior agree.
            if (placementMode_ == PlacementMode::thing)
            {
                deselectThingPlacementTemplate();
            }
            else if (placementMode_ != PlacementMode::none)
            {
                setPlacementMode(PlacementMode::none);
            }
            if (surfaceExtrudePreviewActive_)
            {
                cancelSurfaceExtrudePreview();
            }
            deactivateViewportTransformForExclusiveTool();

            // Enforce at most one active click-tool (keep support for "click again to deactivate").
            for (QAction* other : viewportEditToolsGroup_->actions())
            {
                if (!other || other == action || !other->isCheckable() || !other->isChecked())
                {
                    continue;
                }
                other->setChecked(false);
            }

            syncViewportSelectionInputEnabled();
        });

    viewportSectorCleaveAction_ = new QAction("Cleave", viewportToolsOverlay_);
    viewportSectorCleaveAction_->setObjectName("viewport.sector.cleave");
    viewportSectorCleaveAction_->setCheckable(true);
    viewportSectorCleaveAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::toolCleave));
    viewportSectorCleaveAction_->setToolTip(
        "<b>Cleave sectors</b><br>Draw a cut across selected sectors to split them into new sectors.");
    viewportEditToolsGroup_->addAction(viewportSectorCleaveAction_);

    connect(viewportSectorCleaveAction_, &QAction::toggled, this, [this](const bool checked)
        {
            if (checked)
            {
                clearSelectionModifierHintIfShown();
                if (surfaceExtrudePreviewActive_)
                {
                    cancelSurfaceExtrudePreview();
                }
            }

            // Click-based tools must not overlap with placement mode.
            if (checked && placementMode_ != PlacementMode::none)
            {
                setPlacementMode(PlacementMode::none);
            }

            if (checked && viewportSurfaceCleaveAction_ && viewportSurfaceCleaveAction_->isChecked())
            {
                viewportSurfaceCleaveAction_->setChecked(false);
            }
            if (checked && viewportFrameAddAction_ && viewportFrameAddAction_->isChecked())
            {
                viewportFrameAddAction_->setChecked(false);
            }

            if (glView_)
            {
                glView_->setSectorCleaveToolEnabled(checked);
            }
            if (!checked)
            {
                restoreViewportTransformAfterExclusiveTool();
            }
        });

    viewportSectorConnectAction_ = new QAction("Join sectors", viewportToolsOverlay_);
    viewportSectorConnectAction_->setObjectName("viewport.sector.join");
    viewportSectorConnectAction_->setCheckable(true);
    viewportSectorConnectAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::toolConnect));
    viewportSectorConnectAction_->setToolTip(
        "<b>Join sectors</b><br>Use the active Sector to cut openings into the other selected Sectors. Review the preview, then Apply or Cancel.");
    viewportEditToolsGroup_->addAction(viewportSectorConnectAction_);
    connect(viewportSectorConnectAction_, &QAction::triggered, this, [this](const bool checked)
        {
            if (!checked)
            {
                return;
            }
            beginSectorJoinPreview();
        });
    connect(viewportSectorConnectAction_, &QAction::toggled, this, [this](const bool checked)
        {
            if (!checked && sectorJoinPreviewActive_)
            {
                cancelSectorJoinPreview();
            }
            else if (!checked)
            {
                restoreViewportTransformAfterExclusiveTool();
            }
        });

    viewportSectorMergeAction_ = new QAction("Merge sectors", viewportToolsOverlay_);
    viewportSectorMergeAction_->setObjectName("viewport.sector.merge");
    viewportSectorMergeAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::toolMerge));
    viewportSectorMergeAction_->setToolTip(
        "<b>Merge sectors</b><br>Combine compatible adjacent Sectors when the result stays convex. Incompatible pairs remain separate.");
    connect(viewportSectorMergeAction_, &QAction::triggered, this, [this]()
        {
            if (!glView_)
            {
                return;
            }

            if (!ui::selection::canMergeSectorSelection(document_.selection()))
            {
                glView_->showToast("Merge sectors: select two or more sectors only", 2500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
                return;
            }

            std::int32_t activeMergedSector = -1;

            const bool applied = document_.applyUndoable(
                "Sectors: Merged",
                editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure,
                [&](editor::DocumentState& state) -> bool
                {
                    if (!ui::selection::canMergeSectorSelection(state.selection))
                    {
                        return false;
                    }

                    std::vector<std::int32_t> selected(
                        state.selection.sectors.begin(), state.selection.sectors.end());
                    const auto merged =
                        sed::level::levelMergeSelectedSectors(state.level, selected);
                    if (!merged || merged->selection.empty())
                    {
                        return false;
                    }

                    state.selection.clear();
                    state.selection.sectors.insert(
                        merged->selection.begin(), merged->selection.end());
                    activeMergedSector = merged->selection.front();
                    return true;
                });

            if (!applied)
            {
                glView_->showToast("Merge sectors failed (no selected adjacent valid sector pair)", 2500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
                return;
            }

            invalidateSurfaceSelectionContextForTopologyEdit();

            clearCleaveAfterExternalTopologyChange(*glView_);
            glView_->notifyLevelGeometryChanged();
            glView_->setSelectionState(activeMergedSector, -1, -1, -1, document_.selection());
            rebuildOutliner();
            rebuildProperties();
            updateEditActions();
        });

    viewportSectorThrustAction_ = new QAction("Sector thrust", viewportToolsOverlay_);
    viewportSectorThrustAction_->setObjectName("viewport.sector.thrust");
    viewportSectorThrustAction_->setCheckable(true);
    viewportSectorThrustAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::sectorThrust));
    viewportSectorThrustAction_->setToolTip(
        "<b>Sector thrust</b><br>Edit IJIM Sector thrust. Click an arrow or zero-vector center handle to drag; use selection modifiers to add or remove Sectors before dragging.");
    viewportEditToolsGroup_->addAction(viewportSectorThrustAction_);
    connect(viewportSectorThrustAction_, &QAction::toggled, this, [this](const bool checked)
        {
            if (checked)
            {
                clearSelectionModifierHintIfShown();
                if (surfaceExtrudePreviewActive_)
                {
                    cancelSurfaceExtrudePreview();
                }
                if (placementMode_ != PlacementMode::none)
                {
                    setPlacementMode(PlacementMode::none);
                }
            }
            if (glView_)
            {
                glView_->setSectorThrustToolEnabled(checked);
            }
            if (!checked)
            {
                restoreViewportTransformAfterExclusiveTool();
            }
        });

    viewportSurfaceCleaveAction_ = new QAction("Cleave", viewportToolsOverlay_);
    viewportSurfaceCleaveAction_->setObjectName("viewport.surface.cleave");
    viewportSurfaceCleaveAction_->setCheckable(true);
    viewportSurfaceCleaveAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::toolCleave));
    viewportEditToolsGroup_->addAction(viewportSurfaceCleaveAction_);

    connect(viewportSurfaceCleaveAction_, &QAction::toggled, this, [this](const bool checked)
        {
            if (checked)
            {
                clearSelectionModifierHintIfShown();
                if (surfaceExtrudePreviewActive_)
                {
                    cancelSurfaceExtrudePreview();
                }
            }

            // Click-based tools must not overlap with placement mode.
            if (checked && placementMode_ != PlacementMode::none)
            {
                setPlacementMode(PlacementMode::none);
            }

            if (checked && viewportSectorCleaveAction_ && viewportSectorCleaveAction_->isChecked())
            {
                viewportSectorCleaveAction_->setChecked(false);
            }
            if (checked && viewportFrameAddAction_ && viewportFrameAddAction_->isChecked())
            {
                viewportFrameAddAction_->setChecked(false);
            }

            if (glView_)
            {
                glView_->setSurfaceCleaveToolEnabled(checked);
            }
            if (!checked)
            {
                restoreViewportTransformAfterExclusiveTool();
            }
        });

    viewportSurfaceMergeAction_ = new QAction("Merge", viewportToolsOverlay_);
    viewportSurfaceMergeAction_->setObjectName("viewport.surface.merge");
    viewportSurfaceMergeAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::toolMerge));
    viewportSurfaceMergeAction_->setToolTip(
        "<b>Merge surfaces</b><br>Merge adjacent flat Faces in the same Sector when they lie in the same plane and the result stays convex.");
    connect(viewportSurfaceMergeAction_, &QAction::triggered, this, [this]()
        {
            if (!glView_)
            {
                return;
            }

            if (editMode_ != EditMode::surface
                || surfaceSelectionMode_ != SurfaceSelectionMode::face
                || selectionTouchesNonEditableLayer(document_.selection())
                || document_.selection().surfaces.size() < 2
                || !document_.selection().surfaceVertices.empty()
                || !document_.selection().sectors.empty()
                || !document_.selection().things.empty()
                || !document_.selection().frames.empty()
                || !document_.selection().lights.empty()
                || !document_.selection().cogs.empty())
            {
                glView_->showToast(
                    "Merge: select two or more surfaces only",
                    2500,
                    GlView::ToastPlacement::BottomCenter,
                    GlView::ToastGroup::Error);
                return;
            }

            SurfaceRef activeMergedSurface{};

            const bool applied = document_.applyUndoable(
                "Surfaces: Merged",
                editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure,
                [&](editor::DocumentState& state) -> bool
                {
                    if (state.selection.surfaces.size() < 2
                        || editMode_ != EditMode::surface
                        || surfaceSelectionMode_ != SurfaceSelectionMode::face
                        || ui::layers::selectionTouchesNonEditableLayer(
                            state.level, state.selection, layerVisible_, layerLocked_)
                        || !state.selection.surfaceVertices.empty()
                        || !state.selection.sectors.empty()
                        || !state.selection.things.empty()
                        || !state.selection.frames.empty()
                        || !state.selection.lights.empty()
                        || !state.selection.cogs.empty())
                    {
                        return false;
                    }

                    std::vector<SurfaceRef> selected(
                        state.selection.surfaces.begin(), state.selection.surfaces.end());
                    const auto merged =
                        sed::level::levelMergeSelectedSurfaces(state.level, selected);
                    if (!merged || merged->selection.empty())
                    {
                        return false;
                    }

                    state.selection.clear();
                    state.selection.surfaces.insert(
                        merged->selection.begin(), merged->selection.end());
                    activeMergedSurface = merged->selection.front();
                    return true;
                });

            if (!applied)
            {
                glView_->showToast(
                    "Merge failed (no selected adjacent surface pair)",
                    2500,
                    GlView::ToastPlacement::BottomCenter,
                    GlView::ToastGroup::Error);
                return;
            }

            invalidateSurfaceSelectionContextForTopologyEdit();

            clearCleaveAfterExternalTopologyChange(*glView_);
            glView_->notifyLevelGeometryChanged();
            glView_->setSelectionState(
                activeMergedSurface.sector,
                activeMergedSurface.surface,
                -1,
                -1,
                document_.selection());
            rebuildOutliner();
            rebuildProperties();
            updateEditActions();
        });

    // Thing-frame/path edit tools.
    viewportFrameAddAction_ = new QAction("Add frame", viewportToolsOverlay_);
    viewportFrameAddAction_->setObjectName("viewport.frame.add");
    viewportFrameAddAction_->setCheckable(true);
    viewportFrameAddAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::actionAddThing));
    viewportFrameAddAction_->setToolTip(
        "<b>Add frame</b><br>Add a point to the selected Thing's frame path using the surface, alignment, Thing snap, and free-placement options.");
    viewportEditToolsGroup_->addAction(viewportFrameAddAction_);

    connect(viewportFrameAddAction_, &QAction::toggled, this, [this](const bool checked)
        {
            syncThingMoveOptionsOverlay();
            if (!checked)
            {
                return;
            }

            clearSelectionModifierHintIfShown();
            if (surfaceExtrudePreviewActive_)
            {
                cancelSurfaceExtrudePreview();
            }

            // Click-based tools must not overlap with placement mode.
            if (placementMode_ != PlacementMode::none)
            {
                setPlacementMode(PlacementMode::none);
            }

            // Only one "click tool" should be active at a time.
            if (viewportSectorCleaveAction_ && viewportSectorCleaveAction_->isChecked())
            {
                viewportSectorCleaveAction_->setChecked(false);
            }
            if (viewportSurfaceCleaveAction_ && viewportSurfaceCleaveAction_->isChecked())
            {
                viewportSurfaceCleaveAction_->setChecked(false);
            }

            // If the selected thing has no frames yet, initialize the first path point at the current thing pose.
            // This matches expected editor behavior where a new path starts from the thing's current position.
            if (editMode_ != EditMode::frame)
            {
                return;
            }

            const auto& sel = document_.selection();
            if (sel.things.size() != 1)
            {
                return;
            }

            const std::int32_t thingIndex = *sel.things.begin();
            const Level& lvl = document_.level();
            if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= lvl.things.size())
            {
                return;
            }

            if (sed::level::thingCountFrames(lvl.things[static_cast<std::size_t>(thingIndex)]) > 0)
            {
                return;
            }

            std::optional<std::int32_t> newValueIndex{};
            const bool applied = document_.applyThingSubsetUndoable(
                "Thing Frame: Initialized",
                std::vector<std::int32_t>{thingIndex},
                [thingIndex, &newValueIndex](editor::DocumentState& state) -> bool
                {
                    if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= state.level.things.size())
                    {
                        return false;
                    }

                    Thing& thing = state.level.things[static_cast<std::size_t>(thingIndex)];
                    if (!std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z)
                        || !std::isfinite(thing.pch) || !std::isfinite(thing.yaw) || !std::isfinite(thing.rol))
                    {
                        return false;
                    }

                    sed::TplValue v{};
                    v.name = "frame";
                    v.setFrame(thing.x, thing.y, thing.z, thing.pch, thing.yaw, thing.rol);
                    thing.values.push_back(std::move(v));
                    sed::level::thingSyncNumFrames(thing);

                    // Synchronization inserts the canonical `numframes`
                    // parameter before the first frame and can shift the
                    // appended point's raw value index.
                    for (std::size_t valueIndex = thing.values.size();
                         valueIndex > 0; --valueIndex)
                    {
                        const std::size_t candidateIndex = valueIndex - 1;
                        const sed::TplValue& candidate =
                            thing.values[candidateIndex];
                        if (candidate.type == ValueType::frame
                            && candidate.name == "frame")
                        {
                            newValueIndex = static_cast<std::int32_t>(candidateIndex);
                            break;
                        }
                    }

                    state.selection.things.clear();
                    state.selection.things.insert(thingIndex);
                    state.selection.frames.clear();
                    if (newValueIndex)
                    {
                        state.selection.frames.insert(
                            sed::level::FrameRef{ thingIndex, *newValueIndex });
                    }
                    return true;
                });

            if (!applied)
            {
                return;
            }

            if (glView_)
            {
                glView_->notifyThingFramesChanged(
                    std::vector<std::int32_t>{thingIndex});
                glView_->setSelectionState(-1, -1, thingIndex, -1, document_.selection());
                if (newValueIndex)
                {
                    glView_->setSelectedFrameValueIndex(*newValueIndex);
                }
            }

            rebuildProperties();
            updateEditActions();
        });

    viewportFrameDeleteAction_ = new QAction("Delete frame", viewportToolsOverlay_);
    viewportFrameDeleteAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::actionDelete));
    viewportFrameDeleteAction_->setToolTip(
        "<b>Delete frame</b><br>Remove the selected point or points from the Thing's frame path.");
    connect(viewportFrameDeleteAction_, &QAction::triggered, this, [this]()
        {
            if (deleteAction_)
            {
                deleteAction_->trigger();
            }
        });

    auto makeButton = [&](QAction* action) -> QToolButton*
    {
        auto* button = new QToolButton(viewportToolsOverlay_);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setAutoRaise(true);
        button->setIconSize(QSize(28, 28));
        button->setFixedSize(QSize(44, 44));
        return button;
    };

    viewportSectorCleaveButton_ = makeButton(viewportSectorCleaveAction_);
    viewportSectorConnectButton_ = makeButton(viewportSectorConnectAction_);
    viewportSectorMergeButton_ = makeButton(viewportSectorMergeAction_);
    viewportSectorThrustButton_ = makeButton(viewportSectorThrustAction_);
    viewportSurfaceCleaveButton_ = makeButton(viewportSurfaceCleaveAction_);
    viewportSurfaceMergeButton_ = makeButton(viewportSurfaceMergeAction_);
    viewportFrameAddButton_ = makeButton(viewportFrameAddAction_);
    viewportFrameDeleteButton_ = makeButton(viewportFrameDeleteAction_);

    viewportSurfaceExtrudeButton_ = extrudeSurfaceAction_ ? makeButton(extrudeSurfaceAction_) : nullptr;
    viewportSurfaceFlipButton_ = flipSurfaceAction_ ? makeButton(flipSurfaceAction_) : nullptr;
    viewportSurfaceMakeAdjoinButton_ = makeAdjoinAction_ ? makeButton(makeAdjoinAction_) : nullptr;
    viewportSurfaceUnadjoinButton_ = unAdjoinAction_ ? makeButton(unAdjoinAction_) : nullptr;
    viewportSurfaceConnectSurfacesButton_ = connectSurfacesAction_ ? makeButton(connectSurfacesAction_) : nullptr;

    updateSurfaceTooltips();

    viewportCommonAddCubeButton_ = addCubeAction_ ? makeButton(addCubeAction_) : nullptr;
    if (viewportCommonAddCubeButton_ && addSectorShapeMenu_)
    {
        viewportCommonAddCubeButton_->setMenu(addSectorShapeMenu_);
        viewportCommonAddCubeButton_->setPopupMode(QToolButton::MenuButtonPopup);
        viewportCommonAddCubeButton_->setToolTip(
            "<b>Sector shape</b><br>Choose the footprint shape to draw when adding a Sector.");
    }
    viewportCommonAddThingButton_ = nullptr;
    viewportCommonAddLightButton_ = addLightAction_ ? makeButton(addLightAction_) : nullptr;
    viewportCommonAddThingLightButton_ = addThingLightAction_ ? makeButton(addThingLightAction_) : nullptr;
    viewportCommonAddSectorLightButton_ = addSectorLightAction_ ? makeButton(addSectorLightAction_) : nullptr;
    if (viewportCommonAddThingLightButton_)
    {
        viewportCommonAddThingLightButton_->setObjectName("viewportCommonAddThingLightButton");
    }
    if (viewportCommonAddSectorLightButton_)
    {
        viewportCommonAddSectorLightButton_->setObjectName("viewportCommonAddSectorLightButton");
    }
    if (viewportCommonAddThingLightButton_ && addThingLightMenu_)
    {
        viewportCommonAddThingLightButton_->setMenu(addThingLightMenu_);
        viewportCommonAddThingLightButton_->setPopupMode(QToolButton::MenuButtonPopup);
        viewportCommonAddThingLightButton_->setToolTip(addThingLightAction_->toolTip());
    }

    auto addToolButton = [&](QToolButton* button)
    {
        if (button)
        {
            layout->addWidget(button, 0, Qt::AlignHCenter);
        }
    };

    addToolButton(viewportSectorCleaveButton_);
    addToolButton(viewportSectorConnectButton_);
    addToolButton(viewportSectorMergeButton_);
    addToolButton(viewportSectorThrustButton_);
    addToolButton(viewportSurfaceCleaveButton_);
    addToolButton(viewportSurfaceMergeButton_);
    addToolButton(viewportFrameAddButton_);
    addToolButton(viewportFrameDeleteButton_);

    addToolButton(viewportSurfaceExtrudeButton_);
    addToolButton(viewportSurfaceFlipButton_);
    addToolButton(viewportSurfaceMakeAdjoinButton_);
    addToolButton(viewportSurfaceUnadjoinButton_);
    addToolButton(viewportSurfaceConnectSurfacesButton_);

    layout->addStretch(1);

    addToolButton(viewportCommonAddCubeButton_);
    addToolButton(viewportCommonAddLightButton_);
    addToolButton(viewportCommonAddThingLightButton_);
    addToolButton(viewportCommonAddSectorLightButton_);

    viewportToolsOverlay_->setMinimumWidth(62);
    viewportToolsOverlay_->setStyleSheet(
        "QWidget#viewportToolsOverlay {"
        "  background: rgba(18, 18, 22, 180);"
        "  border: 1px solid rgba(255, 255, 255, 40);"
        "  border-radius: 8px;"
        "}"
        "QToolButton {"
        "  background: transparent;"
        "  border: 0;"
        "  padding: 4px;"
        "}"
        "QToolButton:hover {"
        "  background: rgba(255, 255, 255, 25);"
        "  border-radius: 6px;"
        "}"
        "QToolButton:disabled {"
        "  background: transparent;"
        "  color: rgba(236, 238, 242, 170);"
        "  border: 0;"
        "}"
        "QToolButton:checked {"
        "  background: rgba(120, 160, 255, 60);"
        "  border: 1px solid rgba(155, 190, 255, 105);"
        "  border-radius: 6px;"
        "}"
        "QToolButton:checked:disabled {"
        "  background: transparent;"
        "  border: 0;"
        "}"
        "QToolButton::menu-indicator {"
        "  image: none;"
        "  width: 0px;"
        "  height: 0px;"
        "}");

    viewportToolsOverlay_->adjustSize();
    positionViewportToolsOverlay();
    viewportToolsOverlay_->hide();
}

void MainWindow::positionViewportToolsOverlay()
{
    if (!glView_ || !viewportToolsOverlay_)
    {
        return;
    }

    viewportToolsOverlay_->adjustSize();

    const int margin = 10;
    int x = margin;
    int y = margin;

    if (viewportTransformOverlay_ && viewportTransformOverlay_->isVisible())
    {
        viewportTransformOverlay_->adjustSize();
        y = viewportTransformOverlay_->geometry().bottom() + 1 + margin;
    }
    else if (viewportEditModeOverlay_ && viewportEditModeOverlay_->isVisible())
    {
        viewportEditModeOverlay_->adjustSize();
        y = viewportEditModeOverlay_->geometry().bottom() + 1 + margin;
    }

    viewportToolsOverlay_->move(
        avoidThingPlacementPanelForOverlay(viewportToolsOverlay_->size(), QPoint(x, y), margin));
}

void MainWindow::updateSurfaceTooltips()
{
    if (viewportSurfaceCleaveAction_)
    {
        QString cleaveTooltip = QStringLiteral("Cleave surface");
        if (surfaceSelectionMode_ == SurfaceSelectionMode::edge)
        {
            cleaveTooltip = QStringLiteral("<b>Split edge</b><br>Split the selected edge into two edges.");
        }
        else
        {
            cleaveTooltip = QStringLiteral(
                "<b>Cleave surface</b><br>Draw a cut across the selected surface to split it.");
        }
        viewportSurfaceCleaveAction_->setToolTip(cleaveTooltip);
    }
    if (viewportSurfaceMergeAction_)
    {
        viewportSurfaceMergeAction_->setToolTip(
            "<b>Merge surfaces</b><br>Merge adjacent flat Faces in the same Sector when they lie in the same plane and the result stays convex.");
    }
    if (extrudeSurfaceAction_)
    {
        extrudeSurfaceAction_->setToolTip(
            "<b>Extrude surface</b><br>Extend the selected surface to create new geometry.");
    }
    if (flipSurfaceAction_)
    {
        flipSurfaceAction_->setToolTip(
            "<b>Flip surface</b><br>Reverse the selected surface's vertex order and facing direction.");
    }
    if (makeAdjoinAction_)
    {
        makeAdjoinAction_->setToolTip(
            "<b>Adjoin surfaces</b><br>Make the selected matching sector surfaces connect as an opening.");
    }
    if (unAdjoinAction_)
    {
        unAdjoinAction_->setToolTip(
            "<b>Unadjoin surfaces</b><br>Remove the connection between the selected adjoining surfaces.");
    }
    if (connectSurfacesAction_)
    {
        connectSurfacesAction_->setToolTip(
            "<b>Connect surfaces</b><br>Create matching openings over the shared area where back-to-back Faces in different Sectors overlap.");
    }
}

void MainWindow::syncViewportToolsOverlay()
{
    if (!glView_ || !viewportToolsOverlay_)
    {
        return;
    }

    const editor::Selection& selection = document_.selection();
    const bool inPreview = glView_->renderPipeline() == GlView::RenderPipeline::GamePreview;
    const bool selectionReadOnly = selectionTouchesNonEditableLayer(selection);
    const bool haveOneThingSelected = selection.things.size() == 1;
    const bool haveMergeableSurfaceOnlySelection =
        selection.surfaces.size() >= 2
        && selection.sectors.empty()
        && selection.surfaceVertices.empty()
        && selection.things.empty()
        && selection.lights.empty()
        && selection.frames.empty()
        && selection.cogs.empty();

    const bool showSectorTools = (editMode_ == EditMode::sector);
    const bool showSectorThrust =
        showSectorTools && !inPreview && document_.level().kind == ProjectType::IJIM;
    const bool showFrameTools = (editMode_ == EditMode::frame) && haveOneThingSelected;
    const bool showSurfaceCleave =
        (editMode_ == EditMode::surface)
        && (surfaceSelectionMode_ == SurfaceSelectionMode::face || surfaceSelectionMode_ == SurfaceSelectionMode::edge);
    const bool showSurfaceMerge =
        (editMode_ == EditMode::surface)
        && (surfaceSelectionMode_ == SurfaceSelectionMode::face)
        && haveMergeableSurfaceOnlySelection;
    const bool showSurfaceExtras = (editMode_ == EditMode::surface) && (surfaceSelectionMode_ == SurfaceSelectionMode::face);
    const bool showSurfaceTools = showSurfaceCleave || showSurfaceMerge || showSurfaceExtras;
    const bool showAddCube = (editMode_ == EditMode::sector);
    const bool showAddLight =
        editMode_ == EditMode::light && lightEditSubMode_ == LightEditSubMode::baked;
    const bool showAddThingLight =
        editMode_ == EditMode::light && lightEditSubMode_ == LightEditSubMode::thing;
    const bool showAddSectorLight =
        editMode_ == EditMode::light
        && lightEditSubMode_ == LightEditSubMode::sector
        && document_.level().kind == ProjectType::IJIM;
    const bool canSectorCleave =
        !inPreview
        && showSectorTools
        && !selectionReadOnly
        && ui::selection::canCleaveSectorSelection(selection);
    const bool canSectorJoin =
        !inPreview
        && showSectorTools
        && !selectionReadOnly
        && ui::selection::canJoinSectorSelection(selection);
    const bool canSectorMerge =
        !inPreview
        && showSectorTools
        && !selectionReadOnly
        && ui::selection::canMergeSectorSelection(selection);
    const bool canSurfaceCleave =
        !inPreview
        && showSurfaceCleave
        && !selectionReadOnly
        && ((surfaceSelectionMode_ == SurfaceSelectionMode::face && ui::selection::canCleaveSurfaceSelection(selection))
            || (surfaceSelectionMode_ == SurfaceSelectionMode::edge && ui::selection::canCleaveSurfaceEdgeSelection(selection)));
    const bool preserveSectorCleaveDraft =
        !inPreview
        && !selectionReadOnly
        && showSectorTools
        && glView_->cleaveStagedMode()
        && glView_->sectorCleaveToolEnabled()
        && glView_->hasStagedCleaveDraft();
    const bool preserveSurfaceCleaveDraft =
        !inPreview
        && !selectionReadOnly
        && showSurfaceCleave
        && glView_->cleaveStagedMode()
        && glView_->surfaceCleaveToolEnabled()
        && glView_->hasStagedCleaveDraft();

    const bool anyTools = showSectorTools || showSurfaceTools || showFrameTools
        || showAddCube || showAddLight || showAddThingLight || showAddSectorLight;
    viewportToolsOverlay_->setVisible(
        (editMode_ != EditMode::none)
        && anyTools
        && !inPreview);
    viewportToolsOverlay_->setEnabled(!inPreview && !selectionReadOnly);
    updateSurfaceTooltips();

    // If a hidden tool is currently active, uncheck it so it can't keep affecting the viewport.
    if ( (!showSectorTools || (!canSectorCleave && !preserveSectorCleaveDraft))
        && viewportSectorCleaveAction_ && viewportSectorCleaveAction_->isChecked() )
    {
        viewportSectorCleaveAction_->setChecked(false);
    }
    if ((!showSectorTools || !canSectorJoin) && viewportSectorConnectAction_ && viewportSectorConnectAction_->isChecked())
    {
        viewportSectorConnectAction_->setChecked(false);
    }
    if ( (!showSurfaceCleave || (!canSurfaceCleave && !preserveSurfaceCleaveDraft))
        && viewportSurfaceCleaveAction_ && viewportSurfaceCleaveAction_->isChecked() )
    {
        viewportSurfaceCleaveAction_->setChecked(false);
    }
    if (!showFrameTools && viewportFrameAddAction_ && viewportFrameAddAction_->isChecked())
    {
        viewportFrameAddAction_->setChecked(false);
    }
    if (!showSectorThrust && viewportSectorThrustAction_ && viewportSectorThrustAction_->isChecked())
    {
        viewportSectorThrustAction_->setChecked(false);
    }

    if (viewportSectorCleaveButton_)
    {
        viewportSectorCleaveButton_->setVisible(showSectorTools);
    }
    if (viewportSectorCleaveAction_)
    {
        viewportSectorCleaveAction_->setEnabled(canSectorCleave);
    }
    if (viewportSectorConnectButton_)
    {
        viewportSectorConnectButton_->setVisible(showSectorTools);
    }
    if (viewportSectorConnectAction_)
    {
        viewportSectorConnectAction_->setEnabled(canSectorJoin);
    }
    if (viewportSectorMergeButton_)
    {
        viewportSectorMergeButton_->setVisible(showSectorTools);
    }
    if (viewportSectorMergeAction_)
    {
        viewportSectorMergeAction_->setEnabled(canSectorMerge);
    }
    if (viewportSectorThrustButton_)
    {
        viewportSectorThrustButton_->setVisible(showSectorThrust);
        viewportSectorThrustButton_->setEnabled(showSectorThrust);
    }
    if (viewportSurfaceCleaveButton_)
    {
        viewportSurfaceCleaveButton_->setVisible(showSurfaceCleave);
    }
    if (viewportSurfaceCleaveAction_)
    {
        viewportSurfaceCleaveAction_->setEnabled(canSurfaceCleave);
    }
    if (viewportSurfaceMergeButton_)
    {
        viewportSurfaceMergeButton_->setVisible(showSurfaceMerge);
    }
    if (viewportSurfaceMergeAction_)
    {
        // QAction shortcuts remain callable while their toolbar button is
        // hidden, so editability and Face-submode eligibility belong on the
        // action itself as well as on the overlay container.
        viewportSurfaceMergeAction_->setEnabled(
            !inPreview && showSurfaceMerge && !selectionReadOnly);
    }
    if (viewportFrameAddButton_)
    {
        viewportFrameAddButton_->setVisible(showFrameTools);
    }
    if (viewportFrameDeleteButton_)
    {
        viewportFrameDeleteButton_->setVisible(showFrameTools);
        viewportFrameDeleteButton_->setEnabled(
            !inPreview && showFrameTools && !selection.frames.empty());
    }

    if (viewportSurfaceExtrudeButton_)
    {
        viewportSurfaceExtrudeButton_->setVisible(showSurfaceExtras);
    }
    if (viewportSurfaceFlipButton_)
    {
        viewportSurfaceFlipButton_->setVisible(showSurfaceExtras);
    }
    if (viewportSurfaceMakeAdjoinButton_)
    {
        viewportSurfaceMakeAdjoinButton_->setVisible(showSurfaceExtras);
    }
    if (viewportSurfaceUnadjoinButton_)
    {
        viewportSurfaceUnadjoinButton_->setVisible(showSurfaceExtras);
    }
    if (viewportSurfaceConnectSurfacesButton_)
    {
        viewportSurfaceConnectSurfacesButton_->setVisible(showSurfaceExtras);
    }

    if (viewportCommonAddCubeButton_)
    {
        viewportCommonAddCubeButton_->setVisible(showAddCube);
    }
    if (viewportCommonAddLightButton_)
    {
        viewportCommonAddLightButton_->setVisible(showAddLight);
    }
    if (viewportCommonAddThingLightButton_)
    {
        viewportCommonAddThingLightButton_->setVisible(showAddThingLight);
    }
    if (viewportCommonAddSectorLightButton_)
    {
        viewportCommonAddSectorLightButton_->setVisible(showAddSectorLight);
    }
    viewportToolsOverlay_->adjustSize();
    positionViewportToolsOverlay();
}

} // namespace sed
