#include "main_window/common.hpp"

#include "render/gizmos/light_category_gizmo_utils.hpp"
#include "render/transform/geometry_transform_preview_utils.hpp"
#include "sed/level/cleave_stroke_reach.hpp"
#include "sed/level/geometry.hpp"
#include "thing/thing_light_authoring_utils.hpp"
#include "ui/layers/layer_color_utils.hpp"
#include "ui/sector/sector_shape_utils.hpp"
#include "ui/selection/selection_tool_availability_utils.hpp"
#include "ui/shortcuts/selection_modifier_utils.hpp"
#include "ui/transform/rotation_utils.hpp"
#include "ui/viewport/cleave_message_utils.hpp"
#include "ui/viewport/cleave_plane_equivalence_utils.hpp"
#include "ui/components/preview_resource_disk_watcher.hpp"

namespace sed
{

namespace
{

void setExclusiveActionChecked(QAction* target)
{
    if (!target)
    {
        return;
    }

    if (QActionGroup* group = target->actionGroup())
    {
        bool alreadySynchronized = target->isChecked()
            && group->checkedAction() == target;
        for (QAction* action : group->actions())
        {
            if (action && action != target && action->isChecked())
            {
                alreadySynchronized = false;
                break;
            }
        }
        if (alreadySynchronized)
        {
            return;
        }

        // Do not block these signals: QActionGroup tracks its checkedAction
        // from the toggled events, and several menus share the same actions.
        // Clearing every peer first also repairs state left inconsistent by
        // an earlier signal-blocked programmatic update.
        for (QAction* action : group->actions())
        {
            if (action && action->isChecked())
            {
                action->setChecked(false);
            }
        }
    }
    target->setChecked(true);
}

bool finiteTransformVector(const TVector& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

TplValue& runtimeLightUpsertThingValue(Thing& thing, const std::string_view name)
{
    const auto existing = std::find_if(thing.values.begin(), thing.values.end(), [name](const TplValue& value)
    {
        return text::iequalsAscii(value.name, name);
    });
    if (existing != thing.values.end())
    {
        return *existing;
    }

    TplValue value{};
    value.name = std::string(name);
    value.type = tplValueTypeForName(name);
    value.adjType = tplAdjTypeForName(name);
    thing.values.push_back(std::move(value));
    return thing.values.back();
}

std::string runtimeLightFormatComponents(
    const double x,
    const double y,
    const double z,
    const std::optional<double> w = std::nullopt)
{
    if (w)
    {
        return formatTplTuple({ x, y, z, *w });
    }
    return formatTplTuple({ x, y, z });
}

// Geometry transforms keep the selected topology welded by also touching the
// valid adjoined mate surfaces.  Subset undo snapshots must include those
// owner sectors; otherwise undo would restore only one side of a portal and
// leave the level in a partially transformed state.  Keep this collection
// deliberately in the viewport signal layer so topology-changing operations
// (cleave/join) can continue using their full snapshots.
void appendSectorTransformUndoSectors(
    const Level& level,
    const std::vector<std::int32_t>& selected,
    std::vector<std::int32_t>& output)
{
    std::unordered_set<std::int32_t> unique{};
    unique.reserve(selected.size() * 2u);
    const auto append = [&](const std::int32_t index) {
        if (index < 0 || static_cast<std::size_t>(index) >= level.sectors.size())
        {
            return;
        }
        if (unique.insert(index).second)
        {
            output.push_back(index);
        }
    };

    for (const std::int32_t sectorIndex : selected)
    {
        if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
        {
            continue;
        }
        append(sectorIndex);
        const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        for (const Surface& surface : sector.surfaces)
        {
            if (!surface.adjoin)
            {
                continue;
            }
            append(surface.adjoin->sector);
        }
    }
}

void appendSurfaceTransformUndoSectors(
    const Level& level,
    const std::vector<sed::SurfaceRef>& surfaces,
    std::vector<std::int32_t>& output)
{
    std::unordered_set<std::int32_t> unique{};
    unique.reserve(surfaces.size() * 2u);
    const auto append = [&](const std::int32_t index) {
        if (index < 0 || static_cast<std::size_t>(index) >= level.sectors.size())
        {
            return;
        }
        if (unique.insert(index).second)
        {
            output.push_back(index);
        }
    };

    for (const sed::SurfaceRef ref : surfaces)
    {
        if (ref.sector < 0 || ref.surface < 0
            || static_cast<std::size_t>(ref.sector) >= level.sectors.size())
        {
            continue;
        }
        const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
        if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
        {
            continue;
        }
        append(ref.sector);
        const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
        if (surface.adjoin)
        {
            append(surface.adjoin->sector);
        }
    }
}

level::ConnectedSurfaceUvPlan captureSurfaceTransformUvs(
    const Level& level,
    const std::vector<SurfaceRef>& surfaces,
    const bool includeFullyMovedSurfaces)
{
    return render::buildConnectedSurfaceUvPreviewPlan(
        level,
        render::buildSurfaceTransformPreviewPlan(level, surfaces),
        includeFullyMovedSurfaces);
}

level::ConnectedSurfaceUvPlan captureSectorTransformUvs(
    const Level& level,
    const std::vector<std::int32_t>& sectors,
    const bool includeFullyMovedSurfaces)
{
    return render::buildConnectedSurfaceUvPreviewPlan(
        level,
        render::buildSectorTransformPreviewPlan(level, sectors),
        includeFullyMovedSurfaces);
}

bool applyConnectedSurfaceUvs(
    Level& level,
    const level::ConnectedSurfaceUvPlan& plan)
{
    const level::ConnectedSurfaceUvEvaluation evaluation =
        level::evaluateConnectedSurfaceUvPlan(plan, level);
    return level::applyConnectedSurfaceUvEvaluation(level, evaluation);
}

template <typename Transform>
std::optional<Level> makeGeometryTransformCandidate(
    const Level& source,
    const Transform& transform)
{
    try
    {
        Level candidate = source;
        if ( !transform(candidate) )
        {
            return std::nullopt;
        }
        return candidate;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

template <typename Transform>
std::optional<Level> makeConnectedSurfaceUvCandidate(
    const Level& source,
    const level::ConnectedSurfaceUvPlan& plan,
    const Transform& transform)
{
    if (!plan.valid)
    {
        return std::nullopt;
    }

    std::optional<Level> candidate =
        makeGeometryTransformCandidate(source, transform);
    if ( !candidate )
    {
        return std::nullopt;
    }

    try
    {
        if ( !applyConnectedSurfaceUvs(*candidate, plan) )
        {
            return std::nullopt;
        }
    }
    catch (...)
    {
        return std::nullopt;
    }
    return candidate;
}

template <typename Transform>
std::optional<Level> makeTransformCandidate(
    const Level& source,
    const bool preserveUvs,
    const level::ConnectedSurfaceUvPlan& plan,
    const Transform& transform)
{
    return preserveUvs
        ? makeConnectedSurfaceUvCandidate(source, plan, transform)
        : makeGeometryTransformCandidate(source, transform);
}

void reportGeometryTransformCandidateFailure(
    GlView& view,
    const bool preserveUvs)
{
    if ( preserveUvs )
    {
        view.reportConnectedSurfaceUvFailure(
            QStringLiteral("The committed geometry no longer matches the captured texture mapping"));
        return;
    }

    view.showToast(
        QStringLiteral("Transform cancelled because the geometry could not be prepared"),
        4000,
        GlView::ToastPlacement::BottomCenter,
        GlView::ToastGroup::Error);
}

} // namespace

bool MainWindow::reloadViewportResourceArchives()
{
    try
    {
        // ResourceLocator builds every replacement index first and swaps it
        // only after all mounts open successfully.  Keep the previous index
        // authoritative while an archive is still being written.
        resources_.reloadGobs();
        ++legacyUvResourceGeneration_;
        publishResourceLocatorSnapshot();
        clearFramePlacementPreview();
        invalidateThingPlacementSnapCaches();
        if ( thingPlacementResourceDiskWatcher_ )
        {
            thingPlacementResourceDiskWatcher_->setResourceLocator(&resources_);
        }
        invalidateThingPlacementResourcePreviews();
        scheduleLegacyUvPromotionRetry();
        resourceArchiveReloadWarningShown_ = false;
        return true;
    }
    catch ( const std::exception& e )
    {
        if ( !resourceArchiveReloadWarningShown_ )
        {
            logMessage(
                MessageSeverity::warning,
                QString("Mounted resource archive changed but could not be reloaded yet: %1")
                    .arg(QString::fromUtf8(e.what())),
                MessageLogCategory::resources);
            resourceArchiveReloadWarningShown_ = true;
        }
        return false;
    }
}

void MainWindow::connectViewportSignals()
{
    glView_->setResourceArchiveReloadCallback(
        [this]()
        {
            return reloadViewportResourceArchives();
        });

    connect(glView_, &GlView::flyNavigationActiveChanged, this,
        [this](const bool active)
        {
            flyHeldKeys_.clear();
            const Qt::KeyboardModifiers modifiers = active
                ? Qt::NoModifier
                : (QGuiApplication::keyboardModifiers()
                    & ui::shortcuts::editorModifierMask());
            glView_->refreshTransformDragForModifiers(modifiers);
            glView_->refreshCleaveSnapPreviewForModifiers(modifiers);
            updateTransformMoveModifierPreviewForKeyState(
                Qt::Key_Shift, modifiers, !active && modifiers.testFlag(Qt::ShiftModifier));
            if (active)
            {
                clearSelectionModifierHintIfShown();
            }
        });

    connect(glView_, &GlView::infoMessage, this, [this](const QString &message) {
        logMessage(MessageSeverity::info, message);
    });
    connect(glView_, &GlView::warningMessage, this, [this](const QString &message) {
        logMessage(MessageSeverity::warning, message);
    });
    connect(glView_, &GlView::errorMessage, this, [this](const QString &message) {
        logMessage(MessageSeverity::error, message);
    });
    connect(glView_, &GlView::resourceWarningMessage, this,
        [this](const QString& message)
        {
            logMessage(
                MessageSeverity::warning,
                message,
                MessageLogCategory::resources);
        });
    connect(glView_, &GlView::resourceErrorMessage, this,
        [this](const QString& message)
        {
            logMessage(
                MessageSeverity::error,
                message,
                MessageLogCategory::resources);
        });
    connect(glView_, &GlView::resourceFilesChanged, this, [this]() {
        cancelArchitecturalLightingInteractiveEdit();
        ++legacyUvResourceGeneration_;
        try
        {
            clearFramePlacementPreview();
        }
        catch (const std::bad_alloc&)
        {
            // `clearFramePlacementPreview()` drops the cached candidate before
            // rebuilding the optional viewport ghost. If that rebuild cannot
            // allocate, no stale candidate can be committed: the cache reset
            // below forces the next pointer or camera update to resolve again.
        }
        invalidateThingPlacementSnapCaches();
        scheduleLegacyUvPromotionRetry();
    });
    connect(glView_, &GlView::resourceArchivesChanged, this, [this]() {
        cancelArchitecturalLightingInteractiveEdit();
        updateEditActions();
    });

    connect(glView_, &GlView::transformPreviewChanged, this,
            [this](const bool active, const GlView::TransformTool tool, const QVector3D &translation, const QVector3D &axis, const double angleDeg,
                   const QVector3D &pivot, const double scaleFactor) {
                if (active && !documentEditingAllowed())
                {
                    transformPreview_ = {};
                    return;
                }
                const bool hadThingOwnerPreview =
                    transformPreview_.active
                    && !document_.selection().things.empty();
                const bool movesThingOwner =
                    active && glView_ && glView_->transformPreviewMovesThingOwner();
                const bool previewsBakedLight =
                    active
                    && editMode_ == EditMode::light
                    && lightEditSubMode_ == LightEditSubMode::baked
                    && !document_.selection().lights.empty();
                transformPreview_ = {};
                // This state feeds authored property previews and the baked
                // light preview worker. Headlight Move/Scale is parameter-only:
                // its runtime light and icon are previewed inside GlView, but it
                // must never make the owning Thing's XYZ fields appear moved.
                transformPreview_.active = movesThingOwner || previewsBakedLight;
                transformPreview_.rotating = (active && tool == GlView::TransformTool::Rotate);
                transformPreview_.translation =
                    TVector{static_cast<double>(translation.x()), static_cast<double>(translation.y()), static_cast<double>(translation.z())};
                transformPreview_.axis = TVector{static_cast<double>(axis.x()), static_cast<double>(axis.y()), static_cast<double>(axis.z())};
                transformPreview_.pivot = TVector{static_cast<double>(pivot.x()), static_cast<double>(pivot.y()), static_cast<double>(pivot.z())};
                transformPreview_.angleDeg = angleDeg;
                transformPreview_.scaleFactor = scaleFactor;

                if (movesThingOwner
                    && !document_.selection().things.empty())
                {
                    updateThingPropertiesPreview(true);
                }
                else if (!active && hadThingOwnerPreview)
                {
                    // Cancellation and zero-delta release have no direct
                    // commit signal. Restore the authoritative fields when
                    // their transient preview ends.
                    updateThingPropertiesPreview(false);
                }

                if (active && editMode_ == EditMode::light
                    && lightEditSubMode_ == LightEditSubMode::baked
                    && !document_.selection().lights.empty())
                {
                    if (!lightBakeTransformPreviewBaseline_)
                    {
                        // Capture/cancel immediately; the zero-delta first
                        // update establishes the exact output to restore.
                        updateLightBakeTransformPreview();
                    }
                    else if (lightBakeTransformPreviewTimer_ && !lightBakeTransformPreviewTimer_->isActive())
                    {
                        lightBakeTransformPreviewTimer_->start();
                    }
                }
                else if (!active)
                {
                    restoreLightBakeTransformPreview();
                }
            });

    // Restore temporary baked outputs before the light commit creates its undo
    // snapshot, while retaining the final preview for seamless display after
    // the synchronous commit completes.
    connect(glView_, &GlView::transformPreviewCommitStarting,
        this, [this]()
        {
            if (documentEditingAllowed())
            {
                prepareLightBakeTransformPreviewCommit();
            }
        });

    connect(glView_, &GlView::viewportStateChanged, this, [this]() {
        if (!glView_)
        {
            return;
        }

        // Sync toolbar toggle states with restored viewport state.
        if (viewportWireframeOverlayAction_)
        {
            const QSignalBlocker b(viewportWireframeOverlayAction_);
            viewportWireframeOverlayAction_->setChecked(glView_->showWireframe());
        }
        if (thingRenderAction_)
        {
            const QSignalBlocker b(thingRenderAction_);
            thingRenderAction_->setChecked(glView_->showThings());
        }
        if (thingHiddenAction_)
        {
            const QSignalBlocker b(thingHiddenAction_);
            thingHiddenAction_->setChecked(glView_->showHiddenThings());
        }
        if (thingCollisionShapesAction_)
        {
            const QSignalBlocker b(thingCollisionShapesAction_);
            thingCollisionShapesAction_->setChecked(glView_->showThingCollisionShapes());
        }
        if (thingVisualBoundsAction_)
        {
            const QSignalBlocker b(thingVisualBoundsAction_);
            thingVisualBoundsAction_->setChecked(glView_->showThingVisualBounds());
        }
        syncThingModelLodActionAvailability();
        if ( thingLightRenderingAction_ )
        {
            const QSignalBlocker blocker(thingLightRenderingAction_);
            thingLightRenderingAction_->setChecked(
                glView_->thingLightRenderingEnabled());
        }
        if ( sectorPointLightRenderingAction_ )
        {
            const QSignalBlocker blocker(sectorPointLightRenderingAction_);
            sectorPointLightRenderingAction_->setChecked(
                glView_->sectorPointLightRenderingEnabled());
            const bool available = document_.level().kind == ProjectType::IJIM;
            sectorPointLightRenderingAction_->setVisible(true);
            sectorPointLightRenderingAction_->setEnabled(available);
        }
        if (thingGizmosAction_)
        {
            const QSignalBlocker b(thingGizmosAction_);
            thingGizmosAction_->setChecked(glView_->showThingGizmos());
        }
        if (thingCubesAction_)
        {
            const QSignalBlocker b(thingCubesAction_);
            const bool checked = (glView_->renderPipeline() == GlView::RenderPipeline::GamePreview) ? glView_->showPreviewCubes() : glView_->showEditorCubes();
            thingCubesAction_->setChecked(checked);
        }

        // Update rendering mode radio group.
        {
            const bool hasVerts = glView_->showVertices();
            const bool hasSolid = glView_->showSolid();
            const bool hasTex = glView_->showTextures();

            QAction *target = viewportSolidAction_;
            if (hasVerts && !hasSolid)
            {
                target = viewportVerticesAction_;
            }
            else if (!hasSolid && !hasVerts)
            {
                target = viewportWireframeAction_;
            }
            else if (hasTex)
            {
                target = viewportTexturedAction_;
            }

            setExclusiveActionChecked(target);

            if (viewportRenderModeButton_ && target)
            {
                viewportRenderModeButton_->setIcon(target->icon());
            }
            if (auto* menu = findChild<QMenu*>("view.render_modes"); menu && target)
            {
                menu->setIcon(target->icon());
            }
        }

        // Update lighting radio group.
        {
            QAction *target = nullptr;
            switch (glView_->lightingMode())
            {
            case GlView::LightingMode::FullyLit:
                target = viewportFullbrightAction_;
                break;
            case GlView::LightingMode::Lit:
                target = viewportLitAction_;
                break;
            case GlView::LightingMode::Diffuse:
                target = viewportDiffuseAction_;
                break;
            case GlView::LightingMode::Gouraud:
                target = viewportGouraudAction_;
                break;
            }
            setExclusiveActionChecked(target);

            if (viewportLightingModeButton_ && target)
            {
                viewportLightingModeButton_->setIcon(target->icon());
            }
            if (auto* menu = findChild<QMenu*>("view.lighting_modes"); menu && target)
            {
                menu->setIcon(target->icon());
            }
        }

        // Update thing view mode radio group.
        {
            QAction *target = nullptr;
            switch (glView_->thingViewMode())
            {
            case GlView::ThingViewMode::Dots:
                target = thingDotsAction_;
                break;
            case GlView::ThingViewMode::Boxes:
                target = thingBoxesAction_;
                break;
            case GlView::ThingViewMode::Wireframes:
                target = thingWireframesAction_;
                break;
            case GlView::ThingViewMode::Shaded:
                target = thingShadedAction_;
                break;
            case GlView::ThingViewMode::Vertices:
                // Model-vertex rendering is intentionally preview-only.
                break;
            }
            setExclusiveActionChecked(target);

            if (viewportThingModeButton_ && target)
            {
                const bool visible = glView_->showThings();
                const QIcon baseIcon = target->icon();
                QString modeName = target->text();
                modeName.remove('&');
                modeName.remove(QStringLiteral("Things: "));
                viewportThingModeButton_->setIcon(visible
                        ? baseIcon
                        : ui::makeCrossedOutViewportIcon(baseIcon));
                viewportThingModeButton_->setToolTip(visible
                        ? QStringLiteral("<b>Thing display</b><br>Things are shown as %1. Click to open Thing display options.")
                              .arg(modeName)
                        : QStringLiteral("<b>Thing display</b><br>Things are hidden (%1 is selected). Click to open Thing display options.")
                              .arg(modeName));
                viewportThingModeButton_->setAccessibleName(visible
                        ? QStringLiteral("Thing rendering on")
                        : QStringLiteral("Thing rendering off"));
                viewportThingModeButton_->setAccessibleDescription(visible
                        ? QStringLiteral("Things are shown as %1. Click to open Thing display options.")
                              .arg(modeName)
                        : QStringLiteral("Things are hidden; %1 is selected. Click to open Thing display options.")
                              .arg(modeName));
            }
            if (thingRenderAction_ && target)
            {
                const bool visible = glView_->showThings();
                const QIcon baseIcon = target->icon();
                thingRenderAction_->setText(visible ? "Render &Things: On" : "Render &Things: Off");
                thingRenderAction_->setToolTip(visible
                        ? "<b>Thing visibility</b><br>Things are shown in the viewport."
                        : "<b>Thing visibility</b><br>Things are hidden in the viewport.");
                thingRenderAction_->setIcon(visible
                        ? baseIcon
                        : ui::makeCrossedOutViewportIcon(baseIcon));
                if (auto* menu = findChild<QMenu*>("view.things"))
                {
                    menu->setIcon(thingRenderAction_->icon());
                }
            }
        }

        // Keep surface sub-mode actions/tool overlays in sync with the viewport
        // (face/edge/vertex).
        {
            SurfaceSelectionMode desired = surfaceSelectionMode_;
            switch (glView_->surfaceSubMode())
            {
            case GlView::SurfaceSubMode::Face:
                desired = SurfaceSelectionMode::face;
                break;
            case GlView::SurfaceSubMode::Edge:
                desired = SurfaceSelectionMode::edge;
                break;
            case GlView::SurfaceSubMode::Vertex:
                desired = SurfaceSelectionMode::vertex;
                break;
            }

            if (desired != surfaceSelectionMode_)
            {
                surfaceSelectionMode_ = desired;

                auto setChecked = [](QAction *action, const bool checked) {
                    if (!action)
                    {
                        return;
                    }
                    const QSignalBlocker b(action);
                    action->setChecked(checked);
                };

                setChecked(surfaceFaceAction_, desired == SurfaceSelectionMode::face);
                setChecked(surfaceEdgeAction_, desired == SurfaceSelectionMode::edge);
                setChecked(surfaceVertexAction_, desired == SurfaceSelectionMode::vertex);

                // Refresh overlays/tool visibility that depend on the sub-mode.
                setEditMode(editMode_);
            }
        }

        // Game Preview owns mode-independent Auto point selection, so the
        // Editor mode bar is always hidden while the pipeline is active.
        const bool isPreview = glView_->renderPipeline() == GlView::RenderPipeline::GamePreview;
        if (viewportGamePreviewAction_)
        {
            // Recovery restores GlView directly. Keep the QAction as a true
            // mirror of that authoritative pipeline so its next activation
            // exits Preview instead of attempting to enter it again.
            const QSignalBlocker blocker(viewportGamePreviewAction_);
            viewportGamePreviewAction_->setChecked(isPreview);
        }
        if (isPreview != lastViewportWasPreview_)
        {
            if (isPreview)
            {
                settleEditingUiTransitionsForGamePreview();
                captureEditingUiVisibilityForGamePreview();
                cancelDocumentMutationPreviewsForGamePreview();
            }
            const bool restoreEditingUiWithoutAnimation = !isPreview
                && previewEditingUiVisibility_.valid
                && previewEditingUiVisibility_.documentEpoch
                    == recoveryDocumentEpoch_;
            lastViewportWasPreview_ = isPreview;
            previewEditingUiRestoreInProgress_ = restoreEditingUiWithoutAnimation;
            if (!isPreview)
            {
                // Restore the panes before the same-mode refresh so UV/Thing
                // synchronization observes their preserved geometry instead
                // of treating them as newly opened widgets.
                restoreEditingUiVisibilityAfterGamePreview();
            }
            setEditMode(editMode_);
            previewEditingUiRestoreInProgress_ = false;
            if (isPreview)
            {
                hideEditingUiForGamePreview();
            }
            else
            {
                restoreViewportTransformAfterExclusiveTool();
            }
        }
        if ( !isPreview )
        {
            // Document replacement may invalidate the old visibility
            // snapshot or settle the pipeline before the transition branch
            // observes it. The current Editor document must still retain a
            // usable Tools editor toggle in every case.
            for ( const QPointer<QDockWidget>& dock : cogScriptEditorDocks() )
            {
                if ( QAction* const toggle = dock ? dock->toggleViewAction() : nullptr )
                {
                    toggle->setEnabled(true);
                }
            }
        }
        syncViewportSoundControls();
        updateSectorSoundPlayback();
        // Cleave authored revisions are O(1) to compare and the projection
        // hash is recomputed only after such a revision changes. This keeps
        // the document title current without serializing the local timeline
        // on camera, selection, or option-only viewport updates.
        refreshWindowTitle();
    });

    connect(glView_, &GlView::viewTransformChanged, this, [this]()
        {
            if (editMode_ == EditMode::frame
                && viewportFrameAddAction_
                && viewportFrameAddAction_->isChecked()
                && framePlacementPreview_)
            {
                updateFramePlacementPreview(
                    framePlacementPreview_->widgetPosition,
                    QApplication::keyboardModifiers()
                        & ui::shortcuts::editorModifierMask());
            }
            if (documentEditingAllowed()
                && !uvEditorSuppressViewTransformSync_
                && uvEditorViewportFacing_
                && editMode_ == EditMode::surface
                && !uvEditorPanelAnimation_)
            {
                syncUvEditorViewportFacingOrientation();
                // View Face makes the camera-derived mirror/rotation and its
                // anchor-preserving pan part of the settled UV workspace.
                // In particular, viewport wheel input has no matching mouse
                // release event, so publish this EDIT-only change here.
                noteRecoveryStateChanged();
            }
        });

    connect(glView_, &GlView::clickPicked, this,
            [this](const std::int32_t sectorIndex, const std::int32_t surfaceIndex, const std::int32_t thingIndex, const std::int32_t lightIndex) {
                lastViewportClickSector_ = sectorIndex;
                lastViewportClickSurface_ = surfaceIndex;
                lastViewportClickThing_ = thingIndex;
                lastViewportClickLight_ = lightIndex;
            });

    connect(glView_, &GlView::architecturalLightingVertexPicked, this,
        [this](const std::int32_t thingIndex,
               const std::int32_t,
               const std::int32_t,
               const std::int32_t,
               const Qt::KeyboardModifiers) {
            if (editMode_ != EditMode::light
                || lightEditSubMode_ != LightEditSubMode::architectural
                || thingIndex < 0
                || static_cast<std::size_t>(thingIndex)
                    >= document_.level().things.size())
            {
                return;
            }
            // This low-level signal is emitted before GlView applies the
            // Replace/Add/Remove vertex operation.  The generic
            // selectionPicked callback has already published the owning Thing
            // selection; wait for architecturalLightingVertexSelectionChanged
            // for the post-click vertex set.  Opening a modal editor here
            // would make a plain click consume the next click and would also
            // erase Shift-selected owners.
            lastViewportClickThing_ = thingIndex;
            cancelArchitecturalLightingInteractiveEdit();
            updateEditActions();
        });

    connect(glView_, &GlView::architecturalLightingVertexSelectionChanged,
        this, [this]()
        {
            if (editMode_ != EditMode::light
                || lightEditSubMode_ != LightEditSubMode::architectural)
            {
                return;
            }
            // A changed viewport selection invalidates a staged popup, but it
            // does not alter the authored document.  Selection state itself is
            // kept by GlView and the normal selection callbacks.
            if (architecturalLightingVertexDialog_)
            {
                cancelArchitecturalLightingInteractiveEdit();
            }
            updateEditActions();
            syncViewportToolsOverlay();
        });

    connect(glView_, &GlView::architecturalLightingVertexEditRequested,
        this, [this](const std::int32_t thingIndex,
               const std::int32_t lodIndex,
               const std::int32_t meshIndex,
               const std::int32_t vertexIndex,
               const Qt::KeyboardModifiers) {
            if (editMode_ != EditMode::light
                || lightEditSubMode_ != LightEditSubMode::architectural
                || document_.level().kind != ProjectType::MOTS
                || !glView_)
            {
                return;
            }

            const GlView::ArchitecturalLightingVertexIdentity requested{
                thingIndex, lodIndex, meshIndex, vertexIndex};
            const auto ownerVertices =
                glView_->architecturalLightingVerticesForThing(thingIndex);
            const bool requestedIdentityStillValid = std::any_of(
                ownerVertices.begin(), ownerVertices.end(),
                [&requested](const GlView::ArchitecturalLightingRenderVertex& vertex)
                {
                    return vertex.thingIndex == requested.thingIndex
                        && vertex.lodIndex == requested.lodIndex
                        && vertex.meshIndex == requested.meshIndex
                        && vertex.vertexIndex == requested.vertexIndex;
                });
            if (!requestedIdentityStillValid)
            {
                return;
            }

            std::vector<GlView::ArchitecturalLightingVertexIdentity> identities(
                glView_->architecturalLightingSelectedVertices().begin(),
                glView_->architecturalLightingSelectedVertices().end());
            if (std::find(identities.begin(), identities.end(), requested)
                == identities.end())
            {
                identities.push_back(requested);
                glView_->setArchitecturalLightingSelectedVertices(identities);
            }

            std::vector<ArchitecturalLightingVertexSelection> selected{};
            selected.reserve(identities.size());
            for (const GlView::ArchitecturalLightingVertexIdentity& identity
                 : identities)
            {
                selected.push_back(ArchitecturalLightingVertexSelection{
                    identity.thingIndex,
                    identity.lodIndex,
                    identity.meshIndex,
                    identity.vertexIndex});
            }
            openArchitecturalLightingVertexEditor(selected);
        },
        Qt::QueuedConnection);

    connect(glView_, &GlView::selectionPicked, this,
            [this](const std::int32_t sectorIndex,
                   const std::int32_t surfaceIndex,
                   const std::int32_t thingIndex,
                   const std::int32_t lightIndex,
                   const Qt::KeyboardModifiers modifiers) {
                if (cogEntityPickActive_)
                {
                    const std::int32_t targetCog = cogEntityPickCogIndex_;
                    const std::int32_t targetValue = cogEntityPickValueIndex_;
                    const CogType targetType = cogEntityPickType_;

                    const Level &lvl = document_.level();
                    if (targetCog < 0 || targetValue < 0 || static_cast<std::size_t>(targetCog) >= lvl.cogs.size() ||
                        static_cast<std::size_t>(targetValue) >= lvl.cogs[static_cast<std::size_t>(targetCog)].values.size())
                    {
                        const PlacementMode restoreMode = cogEntityPickRestorePlacementMode_;
                        cogEntityPickActive_ = false;
                        cogEntityPickCogIndex_ = -1;
                        cogEntityPickValueIndex_ = -1;
                        cogEntityPickRestorePlacementMode_ = PlacementMode::none;
                        setPlacementMode(restoreMode);
                        if (glView_)
                        {
                            glView_->setCogEntityPickHighlightEnabled(false);
                            glView_->setHoverPickEnabled(false);
                            glView_->unsetCursor();
                            glView_->setHighlightedSelection(document_.selection());
                        }
                        return;
                    }

                    CogValue parsed = lvl.cogs[static_cast<std::size_t>(targetCog)].values[static_cast<std::size_t>(targetValue)];
                    bool havePickedValue = false;

                    switch (targetType)
                    {
                    case CogType::sec: {
                        if (sectorIndex >= 0 && static_cast<std::size_t>(sectorIndex) < lvl.sectors.size())
                        {
                            parsed.intValue = sectorIndex;
                            havePickedValue = true;
                        }
                        break;
                    }
                    case CogType::surface: {
                        if (sectorIndex >= 0 && surfaceIndex >= 0 && static_cast<std::size_t>(sectorIndex) < lvl.sectors.size() &&
                            static_cast<std::size_t>(surfaceIndex) < lvl.sectors[static_cast<std::size_t>(sectorIndex)].surfaces.size())
                        {
                            const std::int32_t global = lvl.getGlobalSurfaceNumber(sectorIndex, surfaceIndex);
                            if (global >= 0)
                            {
                                parsed.intValue = global;
                                havePickedValue = true;
                            }
                        }
                        break;
                    }
                    case CogType::thing: {
                        if (thingIndex >= 0 && static_cast<std::size_t>(thingIndex) < lvl.things.size())
                        {
                            parsed.intValue = thingIndex;
                            havePickedValue = true;
                        }
                        break;
                    }
                    default:
                        break;
                    }

                    if (!havePickedValue)
                    {
                        statusBar()->showMessage("Pick missed. Click a valid entity or press Esc to cancel.", 2500);
                        return;
                    }

                    std::vector<std::int32_t> targets{};
                    targets.reserve(document_.selection().cogs.size());
                    for (const std::int32_t idx : document_.selection().cogs)
                    {
                        targets.push_back(idx);
                    }
                    if (targets.empty())
                    {
                        targets.push_back(targetCog);
                    }

                    (void)document_.applyUndoable("COG: Value picked", editor::DocumentDirty::Cogs,
                                                  [targets, targetValue, parsed](editor::DocumentState &state) -> bool {
                                                      bool any = false;
                                                      for (const std::int32_t idx : targets)
                                                      {
                                                          if (idx < 0 || static_cast<std::size_t>(idx) >= state.level.cogs.size())
                                                          {
                                                              continue;
                                                          }

                                                          Cog &c = state.level.cogs[static_cast<std::size_t>(idx)];
                                                          if (targetValue < 0 || static_cast<std::size_t>(targetValue) >= c.values.size())
                                                          {
                                                              continue;
                                                          }

                                                          const std::string before = c.values[static_cast<std::size_t>(targetValue)].asJedString(state.level);
                                                          const std::string after = parsed.asJedString(state.level);
                                                          if (before == after)
                                                          {
                                                              continue;
                                                          }

                                                          c.values[static_cast<std::size_t>(targetValue)] = parsed;
                                                          any = true;
                                                      }
                                                      return any;
                                                  });

                    const PlacementMode restoreMode = cogEntityPickRestorePlacementMode_;
                    cogEntityPickActive_ = false;
                    cogEntityPickCogIndex_ = -1;
                    cogEntityPickValueIndex_ = -1;
                    cogEntityPickRestorePlacementMode_ = PlacementMode::none;
                    setPlacementMode(restoreMode);
                    if (glView_)
                    {
                        glView_->setCogEntityPickHighlightEnabled(false);
                        glView_->setHoverPickEnabled(false);
                        glView_->unsetCursor();
                        glView_->setHighlightedSelection(document_.selection());
                    }

                    rebuildOutliner();
                    updateEditActions();
                    rebuildProperties();
                    statusBar()->showMessage("COG variable assigned from viewport pick.", 2000);
                    return;
                }

                if (glView_
                    && glView_->renderPipeline()
                        == GlView::RenderPipeline::GamePreview)
                {
                    if (!viewportEntitySelectionInputActive()
                        || placementMode_ != PlacementMode::none)
                    {
                        return;
                    }

                    // Preview selection is intentionally independent of the
                    // remembered Editor mode/submode. The picker publishes
                    // exactly one entity family; modifiers only control the
                    // selection set and never switch an Editor sidecar.
                    const bool removeOnly = ui::shortcuts::modifierBindingsActive(
                        modifiers, selectionRemoveModifierBindings());
                    const bool additive = !removeOnly
                        && ui::shortcuts::modifierBindingsActive(
                            modifiers, selectionAddModifierBindings());
                    editor::Selection& selection = document_.selection();
                    if (!additive && !removeOnly)
                    {
                        selection.clear();
                    }

                    auto applyIndex = [removeOnly](auto& values, const auto value)
                    {
                        if (removeOnly)
                        {
                            values.erase(value);
                        }
                        else
                        {
                            values.insert(value);
                        }
                    };

                    if (thingIndex >= 0)
                    {
                        applyIndex(selection.things, thingIndex);
                    }
                    else if (lightIndex >= 0)
                    {
                        applyIndex(selection.lights, lightIndex);
                    }
                    else if (sectorIndex >= 0 && surfaceIndex >= 0)
                    {
                        applyIndex(
                            selection.surfaces,
                            SurfaceRef{sectorIndex, surfaceIndex});
                    }
                    else if (sectorIndex >= 0)
                    {
                        applyIndex(selection.sectors, sectorIndex);
                    }

                    auto firstIndex = [](const auto& values) -> std::int32_t
                    {
                        return values.empty()
                            ? -1
                            : *std::min_element(values.begin(), values.end());
                    };
                    std::int32_t activeSector = -1;
                    std::int32_t activeSurface = -1;
                    std::int32_t activeThing = -1;
                    std::int32_t activeLight = -1;
                    if (!removeOnly && thingIndex >= 0)
                    {
                        activeThing = thingIndex;
                    }
                    else if (!removeOnly && lightIndex >= 0)
                    {
                        activeLight = lightIndex;
                    }
                    else if (!removeOnly
                        && sectorIndex >= 0 && surfaceIndex >= 0)
                    {
                        activeSector = sectorIndex;
                        activeSurface = surfaceIndex;
                    }
                    else if (!removeOnly && sectorIndex >= 0)
                    {
                        activeSector = sectorIndex;
                    }
                    else if (!selection.things.empty())
                    {
                        activeThing = firstIndex(selection.things);
                    }
                    else if (!selection.lights.empty())
                    {
                        activeLight = firstIndex(selection.lights);
                    }
                    else if (!selection.surfaces.empty())
                    {
                        const SurfaceRef ref = *std::min_element(
                            selection.surfaces.begin(),
                            selection.surfaces.end(),
                            [](const SurfaceRef lhs, const SurfaceRef rhs)
                            {
                                return lhs.sector != rhs.sector
                                    ? lhs.sector < rhs.sector
                                    : lhs.surface < rhs.surface;
                            });
                        activeSector = ref.sector;
                        activeSurface = ref.surface;
                    }
                    else
                    {
                        activeSector = firstIndex(selection.sectors);
                    }

                    glView_->setSelectionState(
                        activeSector, activeSurface, activeThing, activeLight,
                        selection);
                    syncOutlinerSelectionOnly();
                    updateEditActions();
                    rebuildProperties();
                    return;
                }

                const bool sectorThrustSelection =
                    viewportSectorThrustAction_
                    && viewportSectorThrustAction_->isChecked()
                    && editMode_ == EditMode::sector
                    && sectorIndex >= 0
                    && surfaceIndex < 0
                    && thingIndex < 0
                    && lightIndex < 0;
                if (!viewportEntitySelectionInputActive() && !sectorThrustSelection)
                {
                    return;
                }

                // Normal viewport picks supersede any pending cog-editor navigation
                // context.
                cogEntityNavigatePending_ = false;
                cogEntityNavigatePendingKind_ = OutlinerKind::none;
                cogEntityNavigatePendingIndex_ = -1;
                cogEntityNavigatePendingSector_ = -1;
                cogEntityNavigatePendingSurface_ = -1;

                if (placementMode_ != PlacementMode::none)
                {
                    return;
                }

                if (editMode_ == EditMode::none)
                {
                    return;
                }

                const bool surfacePathSelect = (editMode_ == EditMode::surface)
                    && ui::shortcuts::modifierBindingsActive(
                        modifiers, selectionShortestPathModifierBindings());
                const bool removeOnly = ui::shortcuts::modifierBindingsActive(
                    modifiers, selectionRemoveModifierBindings()) && !surfacePathSelect;
                const bool additive = ui::shortcuts::modifierBindingsActive(
                    modifiers, selectionAddModifierBindings()) && !surfacePathSelect;
                const bool surfacePathSelectAllowHidden = surfacePathSelect
                    && ui::shortcuts::modifierBindingsActive(
                        modifiers, selectionBypassSurfaceFiltersModifierBindings());

                auto &selection = document_.selection();
                auto eraseIndex = [&](auto &set, const auto &value) -> bool {
                    if (const auto it = set.find(value); it != set.end())
                    {
                        set.erase(it);
                        return true;
                    }
                    return false;
                };

                std::int32_t primarySector = -1;
                SurfaceRef primarySurface{-1, -1};
                std::int32_t primaryThing = -1;
                std::int32_t primaryLight = -1;

                auto minSurfaceFromSet = [](const auto &set) -> SurfaceRef {
                    if (set.empty())
                    {
                        return SurfaceRef{-1, -1};
                    }

                    SurfaceRef best = *set.begin();
                    for (const SurfaceRef &ref : set)
                    {
                        if (ref.sector < best.sector || (ref.sector == best.sector && ref.surface < best.surface))
                        {
                            best = ref;
                        }
                    }
                    return best;
                };

                bool handledSurfacePathSelect = false;
                if (editMode_ == EditMode::surface && surfacePathSelect && sectorIndex >= 0 && surfaceIndex >= 0)
                {
                    const Level &lvl = document_.level();

                    auto isValidSurfaceRef = [&](const SurfaceRef ref) -> bool {
                        if (ref.sector < 0 || ref.surface < 0)
                        {
                            return false;
                        }
                        if (static_cast<std::size_t>(ref.sector) >= lvl.sectors.size())
                        {
                            return false;
                        }
                        const Sector &sec = lvl.sectors[static_cast<std::size_t>(ref.sector)];
                        return static_cast<std::size_t>(ref.surface) < sec.surfaces.size();
                    };

                    const SurfaceRef clickedRef{sectorIndex, surfaceIndex};
                    if (isValidSurfaceRef(clickedRef))
                    {
                        SurfaceRef activeRef{-1, -1};

                        if (haveLastSurfaceClick_ && selection.surfaces.find(lastSurfaceClickSurface_) != selection.surfaces.end() &&
                            isValidSurfaceRef(lastSurfaceClickSurface_))
                        {
                            activeRef = lastSurfaceClickSurface_;
                        }
                        else
                        {
                            const SurfaceRef lastViewportRef{lastViewportClickSector_, lastViewportClickSurface_};
                            if (selection.surfaces.find(lastViewportRef) != selection.surfaces.end() && isValidSurfaceRef(lastViewportRef))
                            {
                                activeRef = lastViewportRef;
                            }
                        }

                        if (activeRef.sector < 0 || activeRef.surface < 0)
                        {
                            activeRef = minSurfaceFromSet(selection.surfaces);
                        }

                        auto surfaceKey = [](const SurfaceRef ref) -> std::uint64_t {
                            return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(ref.sector)) << 32) | static_cast<std::uint32_t>(ref.surface);
                        };

                        const QVector3D cameraEye = glView_ ? glView_->cameraEye() : QVector3D(0.0f, 0.0f, 0.0f);
                        const bool cameraPerspective = glView_ && glView_->cameraProjectionType() == app::ViewportCamera::ProjectionType::Perspective;

                        QVector3D cameraForward = glView_ ? glView_->cameraForward() : QVector3D(0.0f, 1.0f, 0.0f);
                        if (cameraForward.lengthSquared() <= sed::math::kTinyEpsilonF)
                        {
                            cameraForward = QVector3D(0.0f, 1.0f, 0.0f);
                        }

                        std::unordered_map<std::uint64_t, QVector3D> surfaceCenterCache{};
                        auto surfaceCenter = [&](const SurfaceRef ref) -> QVector3D {
                            const std::uint64_t key = surfaceKey(ref);
                            if (const auto it = surfaceCenterCache.find(key); it != surfaceCenterCache.end())
                            {
                                return it->second;
                            }

                            QVector3D center(0.0f, 0.0f, 0.0f);
                            std::int32_t count = 0;
                            if (isValidSurfaceRef(ref))
                            {
                                const Sector &sector = lvl.sectors[static_cast<std::size_t>(ref.sector)];
                                const Surface &surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
                                for (const std::int32_t vertexIndex : surface.vertices)
                                {
                                    if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
                                    {
                                        continue;
                                    }
                                    const Vertex &vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
                                    center += QVector3D(static_cast<float>(vertex.x), static_cast<float>(vertex.y), static_cast<float>(vertex.z));
                                    ++count;
                                }
                            }
                            if (count > 0)
                            {
                                center /= static_cast<float>(count);
                            }

                            surfaceCenterCache[key] = center;
                            return center;
                        };

                        std::unordered_map<std::uint64_t, bool> pathPickableCache{};
                        auto surfacePathPickable = [&](const SurfaceRef ref) -> bool {
                            if (!isValidSurfaceRef(ref))
                            {
                                return false;
                            }
                            if (surfacePathSelectAllowHidden)
                            {
                                return true;
                            }

                            const std::uint64_t key = surfaceKey(ref);
                            if (const auto it = pathPickableCache.find(key); it != pathPickableCache.end())
                            {
                                return it->second;
                            }

                            const Sector &sector = lvl.sectors[static_cast<std::size_t>(ref.sector)];
                            const Surface &surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];

                            bool pickable = surface.geo >= static_cast<std::int32_t>(TGeoMode::Solid);
                            if (pickable && (surface.faceFlags & FF_DoubleSided) == 0u)
                            {
                                QVector3D direction = cameraForward;
                                if (cameraPerspective)
                                {
                                    direction = surfaceCenter(ref) - cameraEye;
                                    if (direction.lengthSquared() <= sed::math::kTinyEpsilonF)
                                    {
                                        direction = cameraForward;
                                    }
                                    else
                                    {
                                        direction.normalize();
                                    }
                                }

                                const TVector dir{static_cast<double>(direction.x()), static_cast<double>(direction.y()), static_cast<double>(direction.z())};
                                pickable = surface.normal.Dot(dir) <= 1e-9;
                            }

                            pathPickableCache[key] = pickable;
                            return pickable;
                        };

                        std::unordered_map<std::uint64_t, bool> pathTraversableCache{};
                        auto surfacePathTraversable = [&](const SurfaceRef ref) -> bool {
                            if (!isValidSurfaceRef(ref))
                            {
                                return false;
                            }
                            if (surfacePathSelectAllowHidden)
                            {
                                return true;
                            }

                            const std::uint64_t key = surfaceKey(ref);
                            if (const auto it = pathTraversableCache.find(key); it != pathTraversableCache.end())
                            {
                                return it->second;
                            }

                            const Sector &sector = lvl.sectors[static_cast<std::size_t>(ref.sector)];
                            const Surface &surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];

                            // In strict shortest-path mode (Ctrl+Shift), traversal itself is
                            // topology-driven:
                            // - pass through solid faces
                            // - allow adjoin portals as bridges even when non-solid
                            // Final insertion into selection still filters with
                            // surfacePathPickable().
                            const bool traversable =
                                surface.geo >= static_cast<std::int32_t>(TGeoMode::Solid) || (surface.adjoin && isValidSurfaceRef(*surface.adjoin));

                            pathTraversableCache[key] = traversable;
                            return traversable;
                        };

                        auto buildSurfacePath = [&](const SurfaceRef start, const SurfaceRef goal) -> std::vector<SurfaceRef> {
                            std::vector<SurfaceRef> path{};
                            if (!isValidSurfaceRef(start) || !isValidSurfaceRef(goal))
                            {
                                return path;
                            }
                            if (!surfacePathPickable(goal))
                            {
                                return path;
                            }
                            if (start == goal)
                            {
                                path.push_back(start);
                                return path;
                            }

                            std::vector<std::unordered_map<std::uint64_t, std::vector<std::int32_t>>> sectorEdges{};
                            sectorEdges.resize(lvl.sectors.size());
                            for (std::size_t secIndex = 0; secIndex < lvl.sectors.size(); ++secIndex)
                            {
                                const Sector &sec = lvl.sectors[secIndex];
                                auto &edgeMap = sectorEdges[secIndex];
                                for (std::size_t surfIndex = 0; surfIndex < sec.surfaces.size(); ++surfIndex)
                                {
                                    const Surface &surf = sec.surfaces[surfIndex];
                                    if (surf.vertices.size() < 2)
                                    {
                                        continue;
                                    }

                                    for (std::size_t i = 0; i < surf.vertices.size(); ++i)
                                    {
                                        const std::int32_t va = surf.vertices[i];
                                        const std::int32_t vb = surf.vertices[(i + 1) % surf.vertices.size()];
                                        if (va < 0 || vb < 0)
                                        {
                                            continue;
                                        }
                                        const std::uint32_t a = static_cast<std::uint32_t>(std::min(va, vb));
                                        const std::uint32_t b = static_cast<std::uint32_t>(std::max(va, vb));
                                        const std::uint64_t edgeKey = (static_cast<std::uint64_t>(a) << 32) | b;
                                        edgeMap[edgeKey].push_back(static_cast<std::int32_t>(surfIndex));
                                    }
                                }
                            }

                            const std::uint64_t startKey = surfaceKey(start);
                            const std::uint64_t goalKey = surfaceKey(goal);
                            const QVector3D goalCenter = surfaceCenter(goal);

                            struct PathCost
                            {
                                int hidden{std::numeric_limits<int>::max()};
                                double dist{std::numeric_limits<double>::infinity()};
                            };

                            auto betterCost = [](const PathCost &a, const PathCost &b) -> bool {
                                if (a.hidden != b.hidden)
                                {
                                    return a.hidden < b.hidden;
                                }
                                return (a.dist + 1e-6) < b.dist;
                            };

                            std::unordered_map<std::uint64_t, PathCost> best{};
                            std::unordered_map<std::uint64_t, std::uint64_t> parent{};
                            std::unordered_map<std::uint64_t, SurfaceRef> refByKey{};
                            std::unordered_set<std::uint64_t> closed{};
                            std::vector<std::uint64_t> open{};
                            std::unordered_set<std::uint64_t> inOpen{};

                            best[startKey] = PathCost{0, 0.0};
                            parent[startKey] = startKey;
                            refByKey[startKey] = start;
                            open.push_back(startKey);
                            inOpen.insert(startKey);

                            while (!open.empty())
                            {
                                std::size_t bestOpenIndex = 0;
                                for (std::size_t i = 1; i < open.size(); ++i)
                                {
                                    const std::uint64_t lhsKey = open[i];
                                    const std::uint64_t rhsKey = open[bestOpenIndex];
                                    const PathCost &lhsCost = best[lhsKey];
                                    const PathCost &rhsCost = best[rhsKey];
                                    if (betterCost(lhsCost, rhsCost))
                                    {
                                        bestOpenIndex = i;
                                        continue;
                                    }
                                    if (betterCost(rhsCost, lhsCost))
                                    {
                                        continue;
                                    }

                                    const QVector3D lhsCenter = surfaceCenter(refByKey[lhsKey]);
                                    const QVector3D rhsCenter = surfaceCenter(refByKey[rhsKey]);
                                    const double lhsHeuristic = static_cast<double>((lhsCenter - goalCenter).lengthSquared());
                                    const double rhsHeuristic = static_cast<double>((rhsCenter - goalCenter).lengthSquared());
                                    if ((lhsHeuristic + 1e-6) < rhsHeuristic)
                                    {
                                        bestOpenIndex = i;
                                        continue;
                                    }
                                    if ((rhsHeuristic + 1e-6) < lhsHeuristic)
                                    {
                                        continue;
                                    }
                                    if (lhsKey < rhsKey)
                                    {
                                        bestOpenIndex = i;
                                    }
                                }

                                const std::uint64_t curKey = open[bestOpenIndex];
                                open[bestOpenIndex] = open.back();
                                open.pop_back();
                                inOpen.erase(curKey);

                                if (!closed.insert(curKey).second)
                                {
                                    continue;
                                }
                                if (curKey == goalKey)
                                {
                                    break;
                                }

                                const SurfaceRef cur = refByKey[curKey];
                                if (!isValidSurfaceRef(cur))
                                {
                                    continue;
                                }

                                const Sector &curSector = lvl.sectors[static_cast<std::size_t>(cur.sector)];
                                const Surface &curSurface = curSector.surfaces[static_cast<std::size_t>(cur.surface)];

                                std::unordered_set<std::uint64_t> neighborKeys{};
                                std::vector<SurfaceRef> neighbors{};
                                neighbors.reserve(16);

                                auto addNeighbor = [&](const SurfaceRef ref) {
                                    if (!isValidSurfaceRef(ref))
                                    {
                                        return;
                                    }
                                    if (!surfacePathTraversable(ref))
                                    {
                                        return;
                                    }
                                    const std::uint64_t key = surfaceKey(ref);
                                    if (!neighborKeys.insert(key).second)
                                    {
                                        return;
                                    }
                                    neighbors.push_back(ref);
                                };

                                if (curSurface.adjoin && isValidSurfaceRef(*curSurface.adjoin))
                                {
                                    addNeighbor(*curSurface.adjoin);
                                }

                                if (curSurface.vertices.size() >= 2)
                                {
                                    const auto &edgeMap = sectorEdges[static_cast<std::size_t>(cur.sector)];
                                    for (std::size_t i = 0; i < curSurface.vertices.size(); ++i)
                                    {
                                        const std::int32_t va = curSurface.vertices[i];
                                        const std::int32_t vb = curSurface.vertices[(i + 1) % curSurface.vertices.size()];
                                        if (va < 0 || vb < 0)
                                        {
                                            continue;
                                        }
                                        const std::uint32_t a = static_cast<std::uint32_t>(std::min(va, vb));
                                        const std::uint32_t b = static_cast<std::uint32_t>(std::max(va, vb));
                                        const std::uint64_t edgeKey = (static_cast<std::uint64_t>(a) << 32) | b;

                                        const auto edgeIt = edgeMap.find(edgeKey);
                                        if (edgeIt == edgeMap.end())
                                        {
                                            continue;
                                        }

                                        for (const std::int32_t surfIndex : edgeIt->second)
                                        {
                                            if (surfIndex == cur.surface)
                                            {
                                                continue;
                                            }
                                            addNeighbor(SurfaceRef{cur.sector, surfIndex});
                                        }
                                    }
                                }

                                std::sort(neighbors.begin(), neighbors.end(), [&](const SurfaceRef &a, const SurfaceRef &b) {
                                    const QVector3D ca = surfaceCenter(a);
                                    const QVector3D cb = surfaceCenter(b);
                                    const float da = (ca - goalCenter).lengthSquared();
                                    const float db = (cb - goalCenter).lengthSquared();
                                    if (std::abs(da - db) > 1e-6f)
                                    {
                                        return da < db;
                                    }
                                    if (a.sector != b.sector)
                                    {
                                        return a.sector < b.sector;
                                    }
                                    return a.surface < b.surface;
                                });

                                const PathCost curCost = best[curKey];
                                const QVector3D curCenter = surfaceCenter(cur);
                                for (const SurfaceRef &neighborRef : neighbors)
                                {
                                    const std::uint64_t neighborKey = surfaceKey(neighborRef);
                                    if (closed.find(neighborKey) != closed.end())
                                    {
                                        continue;
                                    }

                                    PathCost candidate = curCost;
                                    if (!surfacePathSelectAllowHidden && !surfacePathPickable(neighborRef))
                                    {
                                        candidate.hidden += 1;
                                    }

                                    const QVector3D neighborCenter = surfaceCenter(neighborRef);
                                    candidate.dist += static_cast<double>((neighborCenter - curCenter).length());

                                    const auto itBest = best.find(neighborKey);
                                    if (itBest != best.end() && !betterCost(candidate, itBest->second))
                                    {
                                        continue;
                                    }

                                    best[neighborKey] = candidate;
                                    parent[neighborKey] = curKey;
                                    refByKey[neighborKey] = neighborRef;
                                    if (inOpen.insert(neighborKey).second)
                                    {
                                        open.push_back(neighborKey);
                                    }
                                }
                            }

                            if (best.find(goalKey) == best.end())
                            {
                                return {};
                            }

                            std::uint64_t curKey = goalKey;
                            for (;;)
                            {
                                const auto refIt = refByKey.find(curKey);
                                if (refIt == refByKey.end())
                                {
                                    return {};
                                }
                                path.push_back(refIt->second);
                                if (curKey == startKey)
                                {
                                    break;
                                }

                                const auto parentIt = parent.find(curKey);
                                if (parentIt == parent.end())
                                {
                                    return {};
                                }
                                curKey = parentIt->second;
                            }

                            std::reverse(path.begin(), path.end());
                            return path;
                        };

                        if (isValidSurfaceRef(activeRef))
                        {
                            const std::vector<SurfaceRef> path = buildSurfacePath(activeRef, clickedRef);
                            if (!path.empty())
                            {
                                if (surfacePathSelectAllowHidden)
                                {
                                    selection.surfaces.insert(path.begin(), path.end());
                                }
                                else
                                {
                                    for (const SurfaceRef &ref : path)
                                    {
                                        if (surfacePathPickable(ref))
                                        {
                                            selection.surfaces.insert(ref);
                                        }
                                    }
                                }

                                if (selection.surfaces.find(clickedRef) != selection.surfaces.end())
                                {
                                    primarySurface = clickedRef;
                                    handledSurfacePathSelect = true;
                                }
                            }
                        }
                    }
                }

                if (!handledSurfacePathSelect)
                {
                    const bool preserveThingSelectionForFrameAdd =
                        (editMode_ == EditMode::frame) && (viewportFrameAddAction_ != nullptr) && viewportFrameAddAction_->isChecked() && (thingIndex < 0);
                    const bool replaceSelection = !additive && !removeOnly;
                    const auto preservedThings = (replaceSelection && preserveThingSelectionForFrameAdd) ? selection.things : decltype(selection.things){};
                    if (replaceSelection)
                    {
                        selection.clear();
                        if (preserveThingSelectionForFrameAdd)
                        {
                            selection.things = preservedThings;
                        }
                    }

                    auto applyIntPick = [&](auto &set, const std::int32_t value, std::int32_t &primary) {
                        if (value < 0)
                        {
                            return;
                        }

                        if (removeOnly)
                        {
                            (void)eraseIndex(set, value);
                            return;
                        }

                        set.insert(value);
                        primary = value;
                    };

                    auto applySurfacePick = [&](const SurfaceRef ref) {
                        if (ref.sector < 0 || ref.surface < 0)
                        {
                            return;
                        }

                        if (removeOnly)
                        {
                            (void)eraseIndex(selection.surfaces, ref);
                            return;
                        }

                        selection.surfaces.insert(ref);
                        primarySurface = ref;
                    };

                    if (editMode_ == EditMode::none)
                    {
                        // "None" edit mode: select whatever the viewport picked.
                        if (thingIndex >= 0)
                        {
                            applyIntPick(selection.things, thingIndex, primaryThing);
                        }
                        else if (lightIndex >= 0)
                        {
                            applyIntPick(selection.lights, lightIndex, primaryLight);
                        }
                        else if (surfaceIndex >= 0 && sectorIndex >= 0)
                        {
                            applySurfacePick(SurfaceRef{sectorIndex, surfaceIndex});
                        }
                        else if (sectorIndex >= 0)
                        {
                            applyIntPick(selection.sectors, sectorIndex, primarySector);
                        }
                    }
                    else if (editMode_ == EditMode::sector)
                    {
                        if (sectorIndex >= 0)
                        {
                            applyIntPick(selection.sectors, sectorIndex, primarySector);
                        }
                    }
                    else if (editMode_ == EditMode::surface)
                    {
                        if (surfaceIndex >= 0 && sectorIndex >= 0)
                        {
                            const SurfaceRef ref{sectorIndex, surfaceIndex};

                            const bool subElementPick =
                                (surfaceSelectionMode_ == SurfaceSelectionMode::edge) || (surfaceSelectionMode_ == SurfaceSelectionMode::vertex);

                            if (surfaceSelectionMode_
                                    == SurfaceSelectionMode::edge)
                            {
                                // Exact Edge add/remove is already applied by
                                // GlView. Keep the existing Face context on a
                                // removal; reconciliation below will add any
                                // newly required exact-edge owners without
                                // dropping independently selected Faces.
                                if (!removeOnly)
                                {
                                    selection.surfaces.insert(ref);
                                    primarySurface = ref;
                                }
                            }
                            else if (subElementPick && !removeOnly)
                            {
                                // In edge/vertex sub-modes, Shift-click is used for
                                // multi-selecting sub-elements. Keep click behavior add-only
                                // (never toggle-off on Shift).
                                selection.surfaces.insert(ref);
                                primarySurface = ref;
                            }
                            else
                            {
                                applySurfacePick(ref);
                            }
                        }
                        else if (sectorIndex >= 0)
                        {
                            applyIntPick(selection.sectors, sectorIndex, primarySector);
                        }
                    }
                    else if (editMode_ == EditMode::thing || editMode_ == EditMode::frame)
                    {
                        if (editMode_ == EditMode::frame)
                        {
                            selection.frames.clear();
                            if (glView_)
                            {
                                glView_->clearFrameSelection();
                            }
                        }

                        if (thingIndex >= 0)
                        {
                            applyIntPick(selection.things, thingIndex, primaryThing);
                        }
                    }
                    else if (editMode_ == EditMode::light)
                    {
                        if ((lightEditSubMode_ == LightEditSubMode::thing
                                || lightEditSubMode_ == LightEditSubMode::architectural)
                            && thingIndex >= 0)
                        {
                            applyIntPick(selection.things, thingIndex, primaryThing);
                            if (lightEditSubMode_ == LightEditSubMode::architectural)
                            {
                                (void)refreshArchitecturalLightingInteractiveOwner(
                                    primaryThing >= 0 ? primaryThing : thingIndex);
                            }
                        }
                        else if (lightEditSubMode_ == LightEditSubMode::sector && sectorIndex >= 0)
                        {
                            applyIntPick(selection.sectors, sectorIndex, primarySector);
                        }
                        else if (lightEditSubMode_ == LightEditSubMode::baked && lightIndex >= 0)
                        {
                            applyIntPick(selection.lights, lightIndex, primaryLight);
                        }
                    }
                }

                if (editMode_ == EditMode::surface
                    && surfaceSelectionMode_ == SurfaceSelectionMode::edge
                    && glView_)
                {
                    // Exact local-edge refs remain viewport-owned. A plain
                    // click replaces Face context, while additive/removal and
                    // path picks preserve every already selected Face so its
                    // blue owner wire cannot disappear merely because no edge
                    // on that Face has been picked yet.
                    ui::selection::reconcileSurfaceEdgeOwnerSelection(
                        selection,
                        glView_->selectedSurfaceEdgeOwners(),
                        !additive && !removeOnly && !surfacePathSelect);

                    // GlView has already removed the clicked exact edge and
                    // deterministically promoted a surviving member. Keep the
                    // document's primary Surface on that promoted edge owner;
                    // otherwise the retained Face context can leave the
                    // removed owner yellow while the real active edge is
                    // suppressed from both active and secondary ranges.
                    if (const std::optional<SurfaceRef> activeOwner =
                            glView_->activeSurfaceEdgeOwner();
                        activeOwner
                        && selection.surfaces.contains(*activeOwner))
                    {
                        primarySurface = *activeOwner;
                    }
                }

                if (glView_)
                {
                    std::int32_t selSector = -1;
                    std::int32_t selSurface = -1;
                    std::int32_t selThing = -1;
                    std::int32_t selLight = -1;

                    auto minIndex = [](const auto &set) -> std::int32_t {
                        if (set.empty())
                        {
                            return -1;
                        }
                        return *std::min_element(set.begin(), set.end());
                    };

                    auto minSurface = [&]() -> SurfaceRef {
                        return minSurfaceFromSet(selection.surfaces);
                    };

                    if (editMode_ == EditMode::surface)
                    {
                        if (primarySurface.sector >= 0 && primarySurface.surface >= 0)
                        {
                            selSector = primarySurface.sector;
                            selSurface = primarySurface.surface;
                        }
                        else if (!selection.surfaces.empty())
                        {
                            const SurfaceRef ref = minSurface();
                            selSector = ref.sector;
                            selSurface = ref.surface;
                        }
                        else if (!selection.sectors.empty())
                        {
                            selSector = minIndex(selection.sectors);
                        }
                    }
                    else if (editMode_ == EditMode::sector)
                    {
                        if (primarySector >= 0)
                        {
                            selSector = primarySector;
                        }
                        else if (!selection.sectors.empty())
                        {
                            selSector = minIndex(selection.sectors);
                        }
                    }
                    else if (editMode_ == EditMode::thing || editMode_ == EditMode::frame)
                    {
                        if (primaryThing >= 0)
                        {
                            selThing = primaryThing;
                        }
                        else if (!selection.things.empty())
                        {
                            selThing = minIndex(selection.things);
                        }
                    }
                    else if (editMode_ == EditMode::light)
                    {
                        if (lightEditSubMode_ == LightEditSubMode::thing
                            || lightEditSubMode_ == LightEditSubMode::architectural)
                        {
                            selThing = primaryThing >= 0 ? primaryThing : minIndex(selection.things);
                        }
                        else if (lightEditSubMode_ == LightEditSubMode::sector)
                        {
                            selSector = primarySector >= 0 ? primarySector : minIndex(selection.sectors);
                        }
                        else
                        {
                            selLight = primaryLight >= 0 ? primaryLight : minIndex(selection.lights);
                        }
                    }
                    else
                    {
                        if (!selection.surfaces.empty())
                        {
                            const SurfaceRef ref = minSurface();
                            selSector = ref.sector;
                            selSurface = ref.surface;
                        }
                        else if (!selection.sectors.empty())
                        {
                            selSector = minIndex(selection.sectors);
                        }
                        else if (!selection.things.empty())
                        {
                            selThing = minIndex(selection.things);
                        }
                        else if (!selection.lights.empty())
                        {
                            selLight = minIndex(selection.lights);
                        }
                    }

                    glView_->setSelectionState(selSector, selSurface, selThing, selLight, selection);
                }

                if (!outliner_)
                {
                    updateEditActions();
                    rebuildProperties();
                    return;
                }

                const int targetOutlinerTab =
                    (editMode_ == EditMode::thing || editMode_ == EditMode::frame)
                        ? 1
                        : (editMode_ == EditMode::light ? lightEditOutlinerTab() : 0);

                if (outlinerTabs_ && outlinerTabs_->currentIndex() != targetOutlinerTab)
                {
                    outlinerTabs_->setCurrentIndex(targetOutlinerTab);
                    return;
                }

                syncOutlinerSelectionOnly();
                updateEditActions();
                rebuildProperties();
            });

    connect(glView_, &GlView::hoverPicked, this,
            [this](const std::int32_t sectorIndex, const std::int32_t surfaceIndex, const std::int32_t thingIndex, const std::int32_t lightIndex) {
                (void)lightIndex;

                if (placementMode_ == PlacementMode::thingLight)
                {
                    updateThingLightPlacementTarget(thingIndex);
                    return;
                }
                if (placementMode_ == PlacementMode::sectorLight)
                {
                    updateSectorLightPlacementTarget(sectorIndex);
                    return;
                }

                if (!cogEntityPickActive_ || !glView_)
                {
                    return;
                }

                editor::Selection hoverSelection{};
                switch (cogEntityPickType_)
                {
                case CogType::sec: {
                    if (sectorIndex >= 0)
                    {
                        hoverSelection.sectors.insert(sectorIndex);
                    }
                    break;
                }
                case CogType::surface: {
                    if (sectorIndex >= 0 && surfaceIndex >= 0)
                    {
                        hoverSelection.surfaces.insert(SurfaceRef{sectorIndex, surfaceIndex});
                    }
                    break;
                }
                case CogType::thing: {
                    if (thingIndex >= 0)
                    {
                        hoverSelection.things.insert(thingIndex);
                    }
                    break;
                }
                default:
                    break;
                }

                if (hoverSelection.empty())
                {
                    glView_->setHighlightedSelection(document_.selection());
                    return;
                }

                glView_->setHighlightedSelection(hoverSelection);
            });

    connect(glView_, &GlView::selectionBoxPicked, this, [this](editor::Selection boxSelection, const GlView::SelectionBoxMode boxMode) {
        if (glView_
            && glView_->renderPipeline()
                == GlView::RenderPipeline::GamePreview)
        {
            return;
        }
        if (!viewportEntitySelectionInputActive())
        {
            return;
        }

        if (placementMode_ != PlacementMode::none)
        {
            return;
        }

        if (editMode_ == EditMode::none)
        {
            return;
        }

        auto &selection = document_.selection();
        const bool replaceSelection = boxMode == GlView::SelectionBoxMode::Replace;
        const bool addToSelection = boxMode == GlView::SelectionBoxMode::Add;
        const bool removeFromSelection = boxMode == GlView::SelectionBoxMode::Remove;

        if (replaceSelection)
        {
            selection.clear();
        }

        if (editMode_ == EditMode::thing || editMode_ == EditMode::frame)
        {
            if (removeFromSelection)
            {
                for (const std::int32_t idx : boxSelection.things)
                {
                    selection.things.erase(idx);
                }
            }
            else if (addToSelection)
            {
                selection.things.insert(boxSelection.things.begin(), boxSelection.things.end());
            }
            else
            {
                selection.things = std::move(boxSelection.things);
            }

            if (editMode_ == EditMode::frame)
            {
                selection.frames.clear();
                if (glView_)
                {
                    glView_->clearFrameSelection();
                }
            }
        }
        else if (editMode_ == EditMode::light)
        {
            if (lightEditSubMode_ == LightEditSubMode::thing
                || lightEditSubMode_ == LightEditSubMode::architectural)
            {
                if (removeFromSelection)
                {
                    for (const std::int32_t idx : boxSelection.things)
                    {
                        selection.things.erase(idx);
                    }
                }
                else if (addToSelection)
                {
                    selection.things.insert(boxSelection.things.begin(), boxSelection.things.end());
                }
                else
                {
                    selection.things = std::move(boxSelection.things);
                }
            }
            else if (lightEditSubMode_ == LightEditSubMode::sector)
            {
                if (removeFromSelection)
                {
                    for (const std::int32_t idx : boxSelection.sectors)
                    {
                        selection.sectors.erase(idx);
                    }
                }
                else if (addToSelection)
                {
                    selection.sectors.insert(boxSelection.sectors.begin(), boxSelection.sectors.end());
                }
                else
                {
                    selection.sectors = std::move(boxSelection.sectors);
                }
            }
            else if (removeFromSelection)
            {
                for (const std::int32_t idx : boxSelection.lights)
                {
                    selection.lights.erase(idx);
                }
            }
            else if (addToSelection)
            {
                selection.lights.insert(boxSelection.lights.begin(), boxSelection.lights.end());
            }
            else
            {
                selection.lights = std::move(boxSelection.lights);
            }
        }
        else if (editMode_ == EditMode::surface)
        {
            if (surfaceSelectionMode_ == SurfaceSelectionMode::vertex)
            {
                if (removeFromSelection)
                {
                    for (const editor::SurfaceVertexRef& ref :
                        boxSelection.surfaceVertices)
                    {
                        selection.surfaceVertices.erase(ref);
                    }
                }
                else if (addToSelection)
                {
                    selection.surfaceVertices.insert(
                        boxSelection.surfaceVertices.begin(),
                        boxSelection.surfaceVertices.end());
                }
                else
                {
                    selection.surfaceVertices =
                        std::move(boxSelection.surfaceVertices);
                }

                // In Vertex mode Surface refs are ownership/wire context,
                // never an independent face selection. Rebuild that context
                // from the authoritative explicit vertex refs after every
                // replace/add/remove operation so stale owner-only state
                // cannot reach face commands.
                selection.surfaces.clear();
                for (const editor::SurfaceVertexRef& ref :
                    selection.surfaceVertices)
                {
                    selection.surfaces.insert(ref.surface);
                }
            }
            else if (surfaceSelectionMode_ == SurfaceSelectionMode::edge
                && glView_)
            {
                ui::selection::reconcileSurfaceEdgeOwnerSelection(
                    selection,
                    glView_->selectedSurfaceEdgeOwners(),
                    replaceSelection);
            }
            else
            {
                if (removeFromSelection)
                {
                    for (const SurfaceRef &ref : boxSelection.surfaces)
                    {
                        selection.surfaces.erase(ref);
                    }
                }
                else if (addToSelection)
                {
                    selection.surfaces.insert(
                        boxSelection.surfaces.begin(),
                        boxSelection.surfaces.end());
                }
                else
                {
                    selection.surfaces = std::move(boxSelection.surfaces);
                }
            }
        }
        else if (editMode_ == EditMode::sector)
        {
            if (removeFromSelection)
            {
                for (const std::int32_t idx : boxSelection.sectors)
                {
                    selection.sectors.erase(idx);
                }
            }
            else if (addToSelection)
            {
                selection.sectors.insert(boxSelection.sectors.begin(), boxSelection.sectors.end());
            }
            else
            {
                selection.sectors = std::move(boxSelection.sectors);
            }
        }

        if (editMode_ == EditMode::light
            && lightEditSubMode_ == LightEditSubMode::architectural)
        {
            const std::int32_t owner = selection.things.empty()
                ? -1
                : *std::min_element(
                    selection.things.begin(), selection.things.end());
            (void)refreshArchitecturalLightingInteractiveOwner(owner);
        }

        if (glView_)
        {
            auto minIndex = [](const auto &set) -> std::int32_t {
                if (set.empty())
                {
                    return -1;
                }
                return *std::min_element(set.begin(), set.end());
            };

            auto minSurface = [&]() -> SurfaceRef {
                if (selection.surfaces.empty())
                {
                    return SurfaceRef{-1, -1};
                }

                SurfaceRef best = *selection.surfaces.begin();
                for (const SurfaceRef &ref : selection.surfaces)
                {
                    if (ref.sector < best.sector || (ref.sector == best.sector && ref.surface < best.surface))
                    {
                        best = ref;
                    }
                }
                return best;
            };

            std::int32_t selSector = -1;
            std::int32_t selSurface = -1;
            std::int32_t selThing = -1;
            std::int32_t selLight = -1;

            if (editMode_ == EditMode::thing || editMode_ == EditMode::frame)
            {
                selThing = minIndex(selection.things);
            }
            else if (editMode_ == EditMode::light)
            {
                        if (lightEditSubMode_ == LightEditSubMode::thing
                            || lightEditSubMode_ == LightEditSubMode::architectural)
                {
                    selThing = minIndex(selection.things);
                }
                else if (lightEditSubMode_ == LightEditSubMode::sector)
                {
                    selSector = minIndex(selection.sectors);
                }
                else
                {
                    selLight = minIndex(selection.lights);
                }
            }
            else if (editMode_ == EditMode::surface)
            {
                if (!selection.surfaces.empty())
                {
                    SurfaceRef ref = minSurface();
                    if (surfaceSelectionMode_
                            == SurfaceSelectionMode::edge
                        && glView_)
                    {
                        if (const std::optional<SurfaceRef> activeOwner =
                                glView_->activeSurfaceEdgeOwner();
                            activeOwner
                            && selection.surfaces.contains(*activeOwner))
                        {
                            ref = *activeOwner;
                        }
                    }
                    selSector = ref.sector;
                    selSurface = ref.surface;
                }
                else
                {
                    selSector = minIndex(selection.sectors);
                }
            }
            else if (editMode_ == EditMode::sector)
            {
                selSector = minIndex(selection.sectors);
            }

            glView_->setSelectionState(selSector, selSurface, selThing, selLight, selection);
        }

        const int targetOutlinerTab =
            (editMode_ == EditMode::thing || editMode_ == EditMode::frame)
                ? 1
                : (editMode_ == EditMode::light ? lightEditOutlinerTab() : 0);

        if (outlinerTabs_ && outlinerTabs_->currentIndex() != targetOutlinerTab)
        {
            outlinerTabs_->setCurrentIndex(targetOutlinerTab);
            return;
        }

        syncOutlinerSelectionOnly();

        updateEditActions();
        rebuildProperties();
    });

    connect(glView_, &GlView::framePicked, this, [this](const std::int32_t thingIndex, const std::int32_t valueIndex) {
        if (!viewportEntitySelectionInputActive() || editMode_ != EditMode::frame)
        {
            return;
        }

        auto &selection = document_.selection();
        selection.sectors.clear();
        selection.surfaces.clear();
        selection.surfaceVertices.clear();
        selection.lights.clear();
        selection.things.clear();
        selection.things.insert(thingIndex);
        selection.frames.clear();
        selection.frames.insert(sed::level::FrameRef{thingIndex, valueIndex});

        if (glView_)
        {
            glView_->setSelectionState(-1, -1, thingIndex, -1, selection);
            glView_->setSelectedFrameValueIndex(valueIndex);
        }

        updateEditActions();
        rebuildProperties();
    });

    connect(glView_, &GlView::thingsTranslateRequested, this,
            [this](const double dx, const double dy, const double dz)
            {
                if (!documentEditingAllowed() || !glView_)
                {
                    return;
                }
                std::vector<std::int32_t> things;
                try
                {
                    things.assign(document_.selection().things.begin(), document_.selection().things.end());
                    if (things.empty())
                    {
                        return;
                    }
                    const bool applied = document_.applyThingSubsetUndoable(
                        ui::undo::action("Thing", things.size(), "Moved"), things,
                        [&](editor::DocumentState& state)
                        {
                            if (!level::levelTranslateThings(state.level, things, dx, dy, dz, true, false))
                            {
                                return false;
                            }
                            for (const auto index : things)
                            {
                                if (index < 0 || static_cast<std::size_t>(index) >= state.level.things.size())
                                {
                                    continue;
                                }
                                Thing& thing = state.level.things[static_cast<std::size_t>(index)];
                                thing.sector = glView_->findContainingSector(
                                    TVector{thing.x, thing.y, thing.z}, thing.sector);
                            }
                            return true;
                        });
                    if (!applied)
                    {
                        return;
                    }
                }
                catch (const std::bad_alloc&)
                {
                    return;
                }
                glView_->notifyThingTransformsChanged(things);
                updateEditActions();
                updateThingPropertiesPreview(false);
                refreshThingChecks(things, false, true);
            });

    const auto transformSelectedThings =
        [this](const TVector& translation, const QVector3D& axis, const double angleDeg, const TVector& pivot)
    {
        if (!documentEditingAllowed() || !glView_)
        {
            return;
        }
        const TVector authoredAxis{axis.x(), axis.y(), axis.z()};
        if (!finiteTransformVector(translation) || !finiteTransformVector(pivot) ||
            !finiteTransformVector(authoredAxis) || !std::isfinite(angleDeg) ||
            !std::isfinite(static_cast<float>(angleDeg)))
        {
            return;
        }
        const bool rotate = axis.lengthSquared() > 1e-10f && angleDeg != 0.0;
        const bool translate = translation.x != 0.0 || translation.y != 0.0 || translation.z != 0.0;
        if (!rotate && !translate)
        {
            return;
        }
        std::vector<std::int32_t> things;
        try
        {
            things.assign(document_.selection().things.begin(), document_.selection().things.end());
            if (things.empty())
            {
                return;
            }
            TVector normalizedAxis = authoredAxis;
            const bool haveRotation = rotate && normalizedAxis.Normalize() != 0.0;
            const TMatrix43 rotation =
                haveRotation ? TMatrix43::CreateFromVectorAngle(normalizedAxis, static_cast<float>(angleDeg))
                             : TMatrix43::identity();
            if (!document_.applyThingSubsetUndoable(
                    ui::undo::action("Thing", things.size(),
                                     rotate && translate ? "Moved and rotated"
                                     : rotate            ? "Rotated"
                                                         : "Moved"),
                    things,
                    [&](editor::DocumentState& state)
                    {
                        bool changed = false;
                        for (const auto index : things)
                        {
                            if (index < 0 || static_cast<std::size_t>(index) >= state.level.things.size())
                            {
                                continue;
                            }
                            Thing& thing = state.level.things[static_cast<std::size_t>(index)];
                            const TVector original{thing.x, thing.y, thing.z};
                            const TVector position =
                                (haveRotation ? rotation.TransformVector(original - pivot) + pivot
                                              : original) +
                                translation;
                            TVector pyr{thing.pch, thing.yaw, thing.rol};
                            if (!finiteTransformVector(position) || !finiteTransformVector(pyr))
                            {
                                return false;
                            }
                            if (haveRotation)
                            {
                                const TMatrix43 oldRotation = TMatrix43::CreateRotate(pyr);
                                TMatrix43 nextRotation{};
                                nextRotation.rvec = rotation.TransformVector(oldRotation.rvec);
                                nextRotation.lvec = rotation.TransformVector(oldRotation.lvec);
                                nextRotation.uvec = rotation.TransformVector(oldRotation.uvec);
                                nextRotation.dvec = TVector::zero;
                                nextRotation.Normalize();
                                pyr = ui::transform::extractPYRCreateRotateCompatible(nextRotation);
                                if (!finiteTransformVector(pyr))
                                {
                                    return false;
                                }
                            }
                            const auto sector = glView_->findContainingSector(position, thing.sector);
                            changed = changed || position.x != thing.x || position.y != thing.y ||
                                      position.z != thing.z || pyr.pitch != thing.pch ||
                                      pyr.yaw != thing.yaw || pyr.roll != thing.rol || sector != thing.sector;
                            thing.x = position.x;
                            thing.y = position.y;
                            thing.z = position.z;
                            thing.pch = pyr.pitch;
                            thing.yaw = pyr.yaw;
                            thing.rol = pyr.roll;
                            thing.sector = sector;
                        }
                        return changed;
                    }))
            {
                return;
            }
        }
        catch (const std::bad_alloc&)
        {
            return;
        }
        glView_->notifyThingTransformsChanged(things);
        updateEditActions();
        updateThingPropertiesPreview(false);
        refreshThingChecks(things, false, true);
    };
    connect(glView_, &GlView::thingsSnapTransformRequested, this, transformSelectedThings);

    connect(glView_, &GlView::frameTranslateRequested, this,
            [this](const std::int32_t thingIndex, const std::int32_t valueIndex, const double dx,
                   const double dy, const double dz)
            {
                if (!documentEditingAllowed() || !glView_ || editMode_ != EditMode::frame)
                {
                    return;
                }
                std::vector<std::int32_t> owners;
                try
                {
                    std::vector<level::FrameRef> frames(document_.selection().frames.begin(),
                                                        document_.selection().frames.end());
                    if (frames.empty())
                    {
                        frames.push_back({thingIndex, valueIndex});
                    }
                    owners.reserve(frames.size());
                    for (const auto ref : frames)
                    {
                        owners.push_back(ref.thing);
                    }
                    std::sort(owners.begin(), owners.end());
                    owners.erase(std::unique(owners.begin(), owners.end()), owners.end());
                    if (!document_.applyThingSubsetUndoable(
                            ui::undo::action("Thing Frame", frames.size(), "Moved"), owners,
                            [&](editor::DocumentState& state)
                            {
                                return level::levelTranslateFrames(state.level, frames, dx, dy, dz);
                            }))
                    {
                        return;
                    }
                }
                catch (const std::bad_alloc&)
                {
                    return;
                }
                glView_->notifyThingFramesChanged(owners);
                updateEditActions();
                rebuildProperties();
            });

    const auto transformSelectedFrames = [this](const std::int32_t thingIndex, const std::int32_t valueIndex,
                                                const TVector& translation, const QVector3D& axis,
                                                const double angleDeg, const TVector& pivot,
                                                const std::string_view verb)
    {
        if (!documentEditingAllowed() || !glView_ || editMode_ != EditMode::frame)
        {
            return;
        }
        TVector rotationAxis{axis.x(), axis.y(), axis.z()};
        if (!finiteTransformVector(translation) || !finiteTransformVector(pivot) ||
            !finiteTransformVector(rotationAxis) || !std::isfinite(angleDeg) ||
            !std::isfinite(static_cast<float>(angleDeg)) || angleDeg == 0.0 ||
            axis.lengthSquared() < 1e-10f || rotationAxis.Normalize() == 0.0)
        {
            return;
        }
        std::vector<std::int32_t> owners;
        try
        {
            std::vector<level::FrameRef> frames(document_.selection().frames.begin(),
                                                document_.selection().frames.end());
            if (frames.empty())
            {
                frames.push_back({thingIndex, valueIndex});
            }
            owners.reserve(frames.size());
            for (const auto ref : frames)
            {
                owners.push_back(ref.thing);
            }
            std::sort(owners.begin(), owners.end());
            owners.erase(std::unique(owners.begin(), owners.end()), owners.end());
            const TMatrix43 rotation =
                TMatrix43::CreateFromVectorAngle(rotationAxis, static_cast<float>(angleDeg));
            if (!document_.applyThingSubsetUndoable(
                    ui::undo::action("Thing Frame", frames.size(), verb), owners,
                    [&](editor::DocumentState& state)
                    {
                        bool changed = false;
                        for (const auto ref : frames)
                        {
                            if (ref.thing < 0 || ref.value < 0 ||
                                static_cast<std::size_t>(ref.thing) >= state.level.things.size())
                            {
                                continue;
                            }
                            Thing& owner = state.level.things[static_cast<std::size_t>(ref.thing)];
                            if (static_cast<std::size_t>(ref.value) >= owner.values.size())
                            {
                                continue;
                            }
                            TplValue& value = owner.values[static_cast<std::size_t>(ref.value)];
                            double x{}, y{}, z{}, pitch{}, yaw{}, roll{};
                            if (value.type != ValueType::frame || !text::iequalsAscii(value.name, "frame") ||
                                !value.tryGetFrame(x, y, z, pitch, yaw, roll))
                            {
                                continue;
                            }
                            const TVector position =
                                rotation.TransformVector(TVector{x, y, z} - pivot) + pivot + translation;
                            if (!finiteTransformVector(position))
                            {
                                return false;
                            }
                            const TMatrix43 original = TMatrix43::CreateRotate(TVector{pitch, yaw, roll});
                            TMatrix43 next{};
                            next.rvec = rotation.TransformVector(original.rvec);
                            next.lvec = rotation.TransformVector(original.lvec);
                            next.uvec = rotation.TransformVector(original.uvec);
                            next.dvec = TVector::zero;
                            next.Normalize();
                            const TVector pyr = ui::transform::extractPYRCreateRotateCompatible(next);
                            if (!finiteTransformVector(pyr))
                            {
                                return false;
                            }
                            if (position.x == x && position.y == y && position.z == z && pyr.pitch == pitch &&
                                pyr.yaw == yaw && pyr.roll == roll)
                            {
                                continue;
                            }
                            value.setFrame(position.x, position.y, position.z, pyr.pitch, pyr.yaw, pyr.roll);
                            changed = true;
                        }
                        return changed;
                    }))
            {
                return;
            }
        }
        catch (const std::bad_alloc&)
        {
            return;
        }
        glView_->notifyThingFramesChanged(owners);
        updateEditActions();
        rebuildProperties();
    };
    connect(glView_, &GlView::frameSnapTransformRequested, this,
            [transformSelectedFrames](const std::int32_t thingIndex, const std::int32_t valueIndex,
                                      const TVector& translation, const QVector3D& axis,
                                      const double angleDeg, const TVector& pivot)
            {
                transformSelectedFrames(thingIndex, valueIndex, translation, axis, angleDeg, pivot,
                                        "Moved and rotated");
            });

    connect(glView_, &GlView::frameRotateRequested, this,
            [transformSelectedFrames](const std::int32_t thingIndex, const std::int32_t valueIndex,
                                      const QVector3D& axis, const double angleDeg, const TVector& pivot)
            {
                transformSelectedFrames(thingIndex, valueIndex, TVector::zero, axis, angleDeg, pivot,
                                        "Rotated");
            });
    connect(glView_, &GlView::thingsRotateRequested, this,
            [transformSelectedThings](const QVector3D& axis, const double angleDeg, const TVector& pivot)
            {
                transformSelectedThings(TVector::zero, axis, angleDeg, pivot);
            });

    connect(glView_, &GlView::lightsTranslateRequested, this, [this](const double dx, const double dy, const double dz) {
        if (!documentEditingAllowed() || !glView_)
        {
            return;
        }

        std::vector<std::int32_t> lights{};
        lights.reserve(document_.selection().lights.size());
        for (const std::int32_t idx : document_.selection().lights)
        {
            lights.push_back(idx);
        }

        if (lights.empty())
        {
            return;
        }

        document_.pushUndo(
            ui::undo::action("Baked Light", lights.size(), "Moved"),
            editor::DocumentDirty::Lights);
        Level &level = document_.level();
        for (const std::int32_t lightIndex : lights)
        {
            if (lightIndex < 0 || static_cast<std::size_t>(lightIndex) >= level.lights.size())
            {
                continue;
            }

            SedLight &light = level.lights[static_cast<std::size_t>(lightIndex)];
            light.position.x += dx;
            light.position.y += dy;
            light.position.z += dz;
        }

        glView_->notifyLightsChanged();
        updateEditActions();
        rebuildProperties();
    });

    connect(glView_, &GlView::lightsRangeScaleRequested, this, [this](const double scaleFactor) {
        if (!documentEditingAllowed() || !glView_)
        {
            return;
        }

        if (!std::isfinite(scaleFactor) || scaleFactor <= 0.0)
        {
            return;
        }

        std::vector<std::int32_t> lights{};
        lights.reserve(document_.selection().lights.size());
        for (const std::int32_t idx : document_.selection().lights)
        {
            lights.push_back(idx);
        }

        if (lights.empty())
        {
            return;
        }

        document_.pushUndo(
            ui::undo::action("Baked Light", lights.size(), "Range changed"),
            editor::DocumentDirty::Lights);
        Level &level = document_.level();
        for (const std::int32_t lightIndex : lights)
        {
            if (lightIndex < 0 || static_cast<std::size_t>(lightIndex) >= level.lights.size())
            {
                continue;
            }

            SedLight &light = level.lights[static_cast<std::size_t>(lightIndex)];
            const double r = light.range * scaleFactor;
            light.range = std::clamp(r, 0.01, 1.0e6);
        }

        glView_->notifyLightsChanged();
        updateEditActions();
        rebuildProperties();
    });

    connect(glView_, &GlView::thingLightTranslateRequested, this,
            [this](const std::int32_t kindValue, const double dx, const double dy, const double dz) {
                const auto kind = static_cast<render::LightGizmoKind>(kindValue);
                if (!documentEditingAllowed() || !glView_
                    || (kind != render::LightGizmoKind::ThingRegular
                        && kind != render::LightGizmoKind::ThingHead)
                    || !std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz)
                    || (dx == 0.0 && dy == 0.0 && dz == 0.0))
                {
                    return;
                }

                const TVector worldDelta{dx, dy, dz};
                const assets::Templates* templatesDb = templates_.size() > 0 ? &templates_ : nullptr;
                if (kind == render::LightGizmoKind::ThingRegular)
                {
                    std::vector<std::int32_t> owners{};
                    owners.reserve(document_.selection().things.size());
                    for (const std::int32_t thingIndex : document_.selection().things)
                    {
                        if (thingIndex < 0
                            || static_cast<std::size_t>(thingIndex) >= document_.level().things.size())
                        {
                            continue;
                        }
                        const Thing& thing = document_.level().things[
                            static_cast<std::size_t>(thingIndex)];
                        if (isLayerLocked(thing.layer))
                        {
                            continue;
                        }
                        const render::lighting::RuntimeThingLight light =
                            render::lighting::resolveRuntimeThingLight(
                                document_.level(),
                                templatesDb,
                                thing,
                                thingIndex,
                                std::nullopt,
                                render::lighting::ThingLightOwnerPolicy::AuthoringPreview);
                        if (light.classification != render::lighting::ThingLightClass::None)
                        {
                            owners.push_back(thingIndex);
                        }
                    }
                    if (owners.empty())
                    {
                        return;
                    }

                    const std::string undoLabel = ui::undo::action(
                        "Thing",
                        document_.selection().things.size(),
                        "Moved");
                    const bool applied = document_.applyThingSubsetUndoable(
                        undoLabel,
                        owners,
                        [owners, dx, dy, dz](editor::DocumentState& state) -> bool {
                            return sed::level::levelTranslateThings(
                                state.level, owners, dx, dy, dz, true);
                        });
                    if (applied)
                    {
                        glView_->notifyThingsChanged();
                        updateEditActions();
                        rebuildProperties();
                    }
                    return;
                }

                const std::string undoLabel = ui::undo::parameterAction(
                    "Thing", document_.selection().things.size(),
                    "lightoffset", "changed");
                std::vector<std::int32_t> owners{};
                owners.reserve(document_.selection().things.size());
                for (const std::int32_t thingIndex : document_.selection().things)
                {
                    if (thingIndex < 0
                        || static_cast<std::size_t>(thingIndex) >= document_.level().things.size())
                    {
                        continue;
                    }
                    const Thing& thing = document_.level().things[
                        static_cast<std::size_t>(thingIndex)];
                    if (isLayerLocked(thing.layer))
                    {
                        continue;
                    }
                    const render::lighting::RuntimeThingLight headLight =
                        render::lighting::resolveRuntimeHeadLight(
                            document_.level(),
                            templatesDb,
                            thing,
                            thingIndex,
                            std::nullopt,
                            render::lighting::ThingLightOwnerPolicy::AuthoringPreview);
                    if (headLight.classification == render::lighting::ThingLightClass::Dynamic
                        && headLight.sourceThing == thingIndex)
                    {
                        owners.push_back(thingIndex);
                    }
                }
                if (owners.empty())
                {
                    return;
                }

                const bool applied = document_.applyThingSubsetUndoable(
                    undoLabel,
                    owners,
                    [this, templatesDb, owners, worldDelta](editor::DocumentState& state) -> bool {
                        bool changed = false;
                        for (const std::int32_t thingIndex : owners)
                        {
                            if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= state.level.things.size())
                            {
                                continue;
                            }
                            Thing& thing = state.level.things[static_cast<std::size_t>(thingIndex)];
                            if (isLayerLocked(thing.layer))
                            {
                                continue;
                            }

                            // A mixed Thing selection may include owners with no effective
                            // Headlight emitter. Only move sources that resolve to the same
                            // dynamic Headlight kind as the transform handle; otherwise this
                            // interaction would manufacture stray `lightoffset` values.
                            const render::lighting::RuntimeThingLight headLight =
                                render::lighting::resolveRuntimeHeadLight(
                                    state.level,
                                    templatesDb,
                                    thing,
                                    thingIndex,
                                    std::nullopt,
                                    render::lighting::ThingLightOwnerPolicy::AuthoringPreview);
                            if (headLight.classification
                                    != render::lighting::ThingLightClass::Dynamic
                                || headLight.sourceThing != thingIndex)
                            {
                                continue;
                            }

                            TVector localOffset{};
                            if (const TplValue* authored = level::thingFindValue(
                                    state.level, templatesDb, thing, "lightoffset"))
                            {
                                const auto parsed = ::sed::thing::light_authoring::parseAuthoredOffset(*authored);
                                if (!parsed) continue;
                                localOffset = *parsed;
                            }

                            TVector pyr{};
                            pyr.pitch = thing.pch;
                            pyr.yaw = thing.yaw;
                            pyr.roll = thing.rol;
                            const TMatrix43 inverseOrientation =
                                TMatrix43::Create(TVector::zero, pyr).InvertOrtho();
                            const TVector localDelta = inverseOrientation.TransformVector(worldDelta);
                            const TVector nextOffset = localOffset + localDelta;
                            if (!std::isfinite(nextOffset.x) || !std::isfinite(nextOffset.y)
                                || !std::isfinite(nextOffset.z)) continue;
                            const std::string next = runtimeLightFormatComponents(
                                nextOffset.x, nextOffset.y, nextOffset.z);
                            TplValue& value = runtimeLightUpsertThingValue(thing, "lightoffset");
                            const std::string before = value.asString();
                            changed = (value.parse(next) && value.asString() != before) || changed;
                        }
                        return changed;
                    });
                if (applied)
                {
                    glView_->notifyThingsChanged();
                    updateEditActions();
                    rebuildProperties();
                }
            });

    connect(glView_, &GlView::thingLightRangeScaleRequested, this,
            [this](const std::int32_t kindValue, const double scaleFactor) {
                const auto kind = static_cast<render::LightGizmoKind>(kindValue);
                if (!documentEditingAllowed() || !glView_
                    || (kind != render::LightGizmoKind::ThingRegular
                        && kind != render::LightGizmoKind::ThingHead)
                    || !std::isfinite(scaleFactor) || scaleFactor <= 0.0 || scaleFactor == 1.0)
                {
                    return;
                }

                const assets::Templates* templatesDb = templates_.size() > 0 ? &templates_ : nullptr;
                const std::string undoLabel = ui::undo::parameterAction(
                    "Thing", document_.selection().things.size(),
                    kind == render::LightGizmoKind::ThingHead
                        ? "lightintensity"
                        : "light",
                    "changed");
                const std::string_view field = kind == render::LightGizmoKind::ThingHead
                    ? std::string_view("lightintensity")
                    : std::string_view("light");
                const auto hasScalableValue = [this, templatesDb, field](const Thing& thing)
                {
                    const TplValue* authored = level::thingFindValue(
                        document_.level(), templatesDb, thing, field);
                    if (!authored)
                    {
                        return false;
                    }
                    if (document_.level().kind == ProjectType::IJIM)
                    {
                        return ::sed::thing::light_authoring::parseAuthoredTuple(*authored).has_value();
                    }
                    return ::sed::thing::light_authoring::parseAuthoredScalar(*authored).has_value();
                };
                std::vector<std::int32_t> owners{};
                owners.reserve(document_.selection().things.size());
                for (const std::int32_t thingIndex : document_.selection().things)
                {
                    if (thingIndex < 0
                        || static_cast<std::size_t>(thingIndex) >= document_.level().things.size())
                    {
                        continue;
                    }
                    const Thing& thing = document_.level().things[static_cast<std::size_t>(thingIndex)];
                    if (isLayerLocked(thing.layer) || !hasScalableValue(thing))
                    {
                        continue;
                    }
                    owners.push_back(thingIndex);
                }
                if (owners.empty())
                {
                    return;
                }

                const bool applied = document_.applyThingSubsetUndoable(
                    undoLabel,
                    owners,
                    [this, templatesDb, kind, scaleFactor, owners](editor::DocumentState& state) -> bool {
                        const std::string_view field = kind == render::LightGizmoKind::ThingHead
                            ? std::string_view("lightintensity")
                            : std::string_view("light");
                        bool changed = false;
                        for (const std::int32_t thingIndex : owners)
                        {
                            if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= state.level.things.size())
                            {
                                continue;
                            }
                            Thing& thing = state.level.things[static_cast<std::size_t>(thingIndex)];
                            if (isLayerLocked(thing.layer))
                            {
                                continue;
                            }
                            const TplValue* authored = level::thingFindValue(
                                state.level, templatesDb, thing, field);
                            if (!authored)
                            {
                                continue;
                            }

                            std::string next{};
                            if (state.level.kind == ProjectType::IJIM)
                            {
                                const auto parsed = ::sed::thing::light_authoring::parseAuthoredTuple(*authored);
                                if (!parsed)
                                {
                                    continue;
                                }
                                if (kind == render::LightGizmoKind::ThingRegular)
                                {
                                    // IJIM regular Thing light has an independent fourth-component range.
                                    next = runtimeLightFormatComponents(
                                        (*parsed)[0], (*parsed)[1], (*parsed)[2],
                                        std::clamp((*parsed)[3] * scaleFactor, 0.011, 1.0e6));
                                }
                                else
                                {
                                    // IJIM Headlight alpha is an independent shared range. Legacy three-component
                                    // values have alpha zero, so materialize their current outer derived radius
                                    // before applying the visible gizmo scale.
                                    const double average =
                                        ((*parsed)[0]
                                            + (*parsed)[1]
                                            + (*parsed)[2])
                                        / 3.0;
                                    const double baseRange = (*parsed)[3] != 0.0
                                        ? (*parsed)[3]
                                        : average / static_cast<double>(
                                            render::lighting::kIjimAttenuationMin);
                                    next = runtimeLightFormatComponents(
                                        (*parsed)[0],
                                        (*parsed)[1],
                                        (*parsed)[2],
                                        std::clamp(baseRange * scaleFactor, 0.0, 1.0e6));
                                }
                            }
                            else
                            {
                                const auto intensity = ::sed::thing::light_authoring::parseAuthoredScalar(*authored);
                                if (!intensity) continue;
                                next = ::sed::thing::light_authoring::formatScalar(
                                    std::clamp(*intensity * scaleFactor, 0.001, 1.0e6));
                            }

                            TplValue& value = runtimeLightUpsertThingValue(thing, field);
                            const std::string before = value.asString();
                            changed = (value.parse(next) && value.asString() != before) || changed;
                        }
                        return changed;
                    });
                if (applied)
                {
                    glView_->notifyThingsChanged();
                    updateEditActions();
                    rebuildProperties();
                }
            });

    connect(glView_, &GlView::sectorLightTranslateRequested, this,
            [this](const double dx, const double dy, const double dz) {
                if (!documentEditingAllowed() || !glView_ || document_.level().kind != ProjectType::IJIM
                    || !std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz))
                {
                    return;
                }
                const std::string undoLabel = ui::undo::action(
                    "Sector", "Sectors", document_.selection().sectors.size(),
                    "Point light moved");
                const bool applied = document_.applyUndoable(
                    undoLabel,
                    editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Things,
                    [this, dx, dy, dz](editor::DocumentState& state) -> bool {
                        bool changed = false;
                        for (const std::int32_t sectorIndex : state.selection.sectors)
                        {
                            if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= state.level.sectors.size())
                            {
                                continue;
                            }
                            Sector& sector = state.level.sectors[static_cast<std::size_t>(sectorIndex)];
                            if (isLayerLocked(sector.layer)
                                || !render::sectorPointLightHasEditorGizmo(sector.pointLight))
                            {
                                continue;
                            }
                            sector.pointLight.position.x += dx;
                            sector.pointLight.position.y += dy;
                            sector.pointLight.position.z += dz;
                            changed = true;
                        }
                        return changed;
                    });
                if (applied)
                {
                    glView_->notifyLevelGeometryChanged();
                    glView_->notifyThingsChanged();
                    updateEditActions();
                    rebuildProperties();
                }
            });

    connect(glView_, &GlView::sectorLightRangeScaleRequested, this,
            [this](const double scaleFactor) {
                if (!documentEditingAllowed() || !glView_ || document_.level().kind != ProjectType::IJIM
                    || !std::isfinite(scaleFactor) || scaleFactor <= 0.0)
                {
                    return;
                }
                const std::string undoLabel = ui::undo::action(
                    "Sector", "Sectors", document_.selection().sectors.size(),
                    "Point light range changed");
                const bool applied = document_.applyUndoable(
                    undoLabel,
                    editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Things,
                    [this, scaleFactor](editor::DocumentState& state) -> bool {
                        bool changed = false;
                        for (const std::int32_t sectorIndex : state.selection.sectors)
                        {
                            if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= state.level.sectors.size())
                            {
                                continue;
                            }
                            Sector& sector = state.level.sectors[static_cast<std::size_t>(sectorIndex)];
                            if (isLayerLocked(sector.layer)
                                || !render::sectorPointLightHasEditorGizmo(sector.pointLight))
                            {
                                continue;
                            }
                            const auto scaleExplicitRange = [scaleFactor](double& authoredRange) -> bool
                            {
                                // Zero is an IJIM sentinel whose effective
                                // falloff is engine-derived. Do not silently
                                // replace it with a baked editor value.
                                if (authoredRange == 0.0)
                                {
                                    return false;
                                }
                                const double scaled =
                                    std::clamp(authoredRange * scaleFactor, 0.001, 1.0e6);
                                if (scaled == authoredRange)
                                {
                                    return false;
                                }
                                authoredRange = scaled;
                                return true;
                            };
                            changed = scaleExplicitRange(sector.pointLight.minRange) || changed;
                            changed = scaleExplicitRange(sector.pointLight.maxRange) || changed;
                        }
                        return changed;
                    });
                if (applied)
                {
                    glView_->notifyLevelGeometryChanged();
                    glView_->notifyThingsChanged();
                    updateEditActions();
                    rebuildProperties();
                }
            });

    connect(glView_, &GlView::sectorsSnapTransformRequested, this,
            [this](const TVector &translation, const QVector3D &axis, const double angleDeg, const TVector &pivot) {
                if (!documentEditingAllowed() || !glView_)
                {
                    return;
                }

                std::vector<std::int32_t> sectors{};
                sectors.reserve(document_.selection().sectors.size());
                for (const std::int32_t idx : document_.selection().sectors)
                {
                    sectors.push_back(idx);
                }

                if (sectors.empty())
                {
                    return;
                }

                const bool hasTranslation = (translation.x != 0.0 || translation.y != 0.0 || translation.z != 0.0);
                const bool hasRotation = axis.lengthSquared() > 1.0e-10f && std::abs(angleDeg) > 1.0e-9;
                if (!hasTranslation && !hasRotation)
                {
                    return;
                }

                std::vector<std::int32_t> undoSectors{};
                undoSectors.reserve(sectors.size() * 2u);
                appendSectorTransformUndoSectors(document_.level(), sectors, undoSectors);
                if (undoSectors.empty())
                {
                    return;
                }

                const bool preserveUvs =
                    glView_->preserveConnectedSurfaceUvsEnabled();
                const level::ConnectedSurfaceUvPlan uvPlan =
                    preserveUvs
                    ? captureSectorTransformUvs(
                        document_.level(), sectors, false)
                    : level::ConnectedSurfaceUvPlan{};
                const auto transform = [&](Level& level) -> bool
                {
                    if (hasRotation)
                    {
                        TVector normal{
                            static_cast<double>(axis.x()),
                            static_cast<double>(axis.y()),
                            static_cast<double>(axis.z()) };
                        if (normal.Normalize() != 0.0)
                        {
                            if ( !level::levelRotateSectorsChecked(
                                level, sectors, normal,
                                static_cast<double>(pivot.x),
                                static_cast<double>(pivot.y),
                                static_cast<double>(pivot.z), angleDeg) )
                            {
                                return false;
                            }
                        }
                        else
                        {
                            return false;
                        }
                    }
                    if (hasTranslation)
                    {
                        if ( !level::levelTranslateSectorsChecked(
                            level, sectors,
                            static_cast<double>(translation.x),
                            static_cast<double>(translation.y),
                            static_cast<double>(translation.z)) )
                        {
                            return false;
                        }
                    }
                    return true;
                };
                std::optional<Level> candidate = makeTransformCandidate(
                    document_.level(), preserveUvs, uvPlan, transform);
                if ( !candidate )
                {
                    reportGeometryTransformCandidateFailure(*glView_, preserveUvs);
                    return;
                }

                document_.pushSectorSubsetUndo(
                    ui::undo::action(
                        "Sector",
                        sectors.size(),
                        hasRotation && hasTranslation
                            ? "Moved and rotated"
                            : hasRotation ? "Rotated" : "Moved"),
                    editor::DocumentDirty::LevelGeometry,
                    undoSectors);
                document_.level() = std::move(*candidate);

                glView_->notifyLevelGeometryChanged();
                updateEditActions();
                rebuildProperties();
                scheduleRebuildChecks(editor::DocumentDirty::LevelGeometry);
            });

    connect(glView_, &GlView::sectorsTranslateRequested, this, [this](const double dx, const double dy, const double dz) {
        if (!documentEditingAllowed() || !glView_)
        {
            return;
        }

        std::vector<std::int32_t> sectors{};
        sectors.reserve(document_.selection().sectors.size());
        for (const std::int32_t idx : document_.selection().sectors)
        {
            sectors.push_back(idx);
        }

        if (sectors.empty())
        {
            return;
        }

        std::vector<std::int32_t> undoSectors{};
        undoSectors.reserve(sectors.size() * 2u);
        appendSectorTransformUndoSectors(document_.level(), sectors, undoSectors);
        if (undoSectors.empty())
        {
            return;
        }

        const bool preserveUvs = glView_->preserveConnectedSurfaceUvsEnabled();
        const level::ConnectedSurfaceUvPlan uvPlan =
            preserveUvs
            ? captureSectorTransformUvs(document_.level(), sectors, false)
            : level::ConnectedSurfaceUvPlan{};
        const auto transform = [&](Level& level) -> bool
        {
            return level::levelTranslateSectorsChecked(level, sectors, dx, dy, dz);
        };
        std::optional<Level> candidate = makeTransformCandidate(
            document_.level(), preserveUvs, uvPlan, transform);
        if ( !candidate )
        {
            reportGeometryTransformCandidateFailure(*glView_, preserveUvs);
            return;
        }

        document_.pushSectorSubsetUndo(
            ui::undo::action("Sector", sectors.size(), "Moved"),
            editor::DocumentDirty::LevelGeometry,
            undoSectors);
        document_.level() = std::move(*candidate);
        glView_->notifyLevelGeometryChanged();
        updateEditActions();
        rebuildProperties();
        scheduleRebuildChecks(editor::DocumentDirty::LevelGeometry);
    });

    connect(glView_, &GlView::sectorThrustVectorsRequested, this,
            [this](const QList<std::int32_t> &sectorIndices, const QVector3D &thrust) {
                if (!documentEditingAllowed() || !glView_ || document_.level().kind != ProjectType::IJIM || sectorIndices.empty()
                    || !std::isfinite(thrust.x()) || !std::isfinite(thrust.y()) || !std::isfinite(thrust.z()))
                {
                    return;
                }

                const bool applied = document_.applyUndoable(
                    "Sector: Thrust changed",
                    editor::DocumentDirty::LevelGeometry,
                    [this, sectorIndices, thrust](editor::DocumentState &state) -> bool {
                        bool changed = false;
                        for (const std::int32_t sectorIndex : sectorIndices)
                        {
                            if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= state.level.sectors.size())
                            {
                                continue;
                            }
                            if (!state.selection.sectors.contains(sectorIndex))
                            {
                                continue;
                            }

                            Sector &sector = state.level.sectors[static_cast<std::size_t>(sectorIndex)];
                            if (isLayerLocked(sector.layer))
                            {
                                continue;
                            }
                            TVector next{
                                static_cast<double>(thrust.x()),
                                static_cast<double>(thrust.y()),
                                static_cast<double>(thrust.z()) };
                            if (next.LengthSqr() <= 1.0e-18)
                            {
                                next = TVector::zero;
                            }
                            const std::int32_t nextFlags = next.IsZero()
                                ? (sector.flags & ~SECF_IJIM_UseThrust)
                                : (sector.flags | SECF_IJIM_UseThrust);
                            if (sector.thrust == next && sector.flags == nextFlags)
                            {
                                continue;
                            }
                            sector.thrust = next;
                            sector.flags = nextFlags;
                            changed = true;
                        }
                        return changed;
                    });

                if (!applied)
                {
                    return;
                }
                glView_->update();
                updateEditActions();
                rebuildProperties();
                scheduleRebuildChecks(editor::DocumentDirty::LevelGeometry);
            });

    connect(glView_, &GlView::sectorThrustPrimaryChanged, this,
            [this](const std::int32_t sectorIndex) {
                if (!glView_ || !document_.selection().sectors.contains(sectorIndex))
                {
                    return;
                }
                glView_->setSelectionState(sectorIndex, -1, -1, -1, document_.selection());
                syncOutlinerSelectionOnly();
                if (outliner_ && (!outlinerTabs_ || outlinerTabs_->currentIndex() == 0))
                {
                    const QSignalBlocker blocker(outliner_);
                    for (int row = 0; row < outliner_->topLevelItemCount(); ++row)
                    {
                        QTreeWidgetItem* item = outliner_->topLevelItem(row);
                        if (!item
                            || static_cast<OutlinerKind>(item->data(0, kOutlinerKindRole).toInt())
                                != OutlinerKind::sector
                            || item->data(0, kOutlinerIndexRole).toInt() != sectorIndex)
                        {
                            continue;
                        }
                        outliner_->setCurrentItem(item);
                        item->setSelected(true);
                        break;
                    }
                }
                rebuildProperties();
            });

    connect(glView_, &GlView::sectorsScaleRequested, this, [this](const QVector3D &axis, const double scaleFactor, const TVector &pivot) {
        if (!documentEditingAllowed() || !glView_)
        {
            return;
        }

        if (!std::isfinite(scaleFactor) || scaleFactor <= 0.0 || std::abs(scaleFactor - 1.0) < 1e-9)
        {
            return;
        }

        std::vector<std::int32_t> sectors{};
        sectors.reserve(document_.selection().sectors.size());
        for (const std::int32_t idx : document_.selection().sectors)
        {
            sectors.push_back(idx);
        }

        if (sectors.empty())
        {
            return;
        }

        ScaleHow how = ScaleHow::xyz;
        if (axis.lengthSquared() > 1e-8f)
        {
            const QVector3D n = axis.normalized();
            const float ax = std::abs(n.x());
            const float ay = std::abs(n.y());
            const float az = std::abs(n.z());
            if (ax >= ay && ax >= az)
            {
                how = ScaleHow::x;
            }
            else if (ay >= az)
            {
                how = ScaleHow::y;
            }
            else
            {
                how = ScaleHow::z;
            }
        }

        ScaleData sd{};
        sd.cx = static_cast<double>(pivot.x);
        sd.cy = static_cast<double>(pivot.y);
        sd.cz = static_cast<double>(pivot.z);
        sd.sfactor = scaleFactor;
        sd.how = how;

        std::vector<std::int32_t> undoSectors{};
        undoSectors.reserve(sectors.size() * 2u);
        appendSectorTransformUndoSectors(document_.level(), sectors, undoSectors);
        if (undoSectors.empty())
        {
            return;
        }

        const bool preserveUvs = glView_->preserveConnectedSurfaceUvsEnabled();
        const level::ConnectedSurfaceUvPlan uvPlan =
            preserveUvs
            ? captureSectorTransformUvs(document_.level(), sectors, true)
            : level::ConnectedSurfaceUvPlan{};
        const auto transform = [&](Level& level) -> bool
        {
            return level::levelScaleSectorsChecked(level, sectors, sd, false);
        };
        std::optional<Level> candidate = makeTransformCandidate(
            document_.level(), preserveUvs, uvPlan, transform);
        if ( !candidate )
        {
            reportGeometryTransformCandidateFailure(*glView_, preserveUvs);
            return;
        }
        if ( !render::preservesAffectedScaleGeometry(
                document_.level(), *candidate, undoSectors, false) )
        {
            glView_->showToast(
                QStringLiteral("Scale cancelled because it would produce degenerate geometry"),
                4000,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
            return;
        }

        document_.pushSectorSubsetUndo(
            ui::undo::action("Sector", sectors.size(), "Scaled"),
            editor::DocumentDirty::LevelGeometry,
            undoSectors);
        document_.level() = std::move(*candidate);
        glView_->notifyLevelGeometryChanged();
        updateEditActions();
        rebuildProperties();
        scheduleRebuildChecks(editor::DocumentDirty::LevelGeometry);
    });

    connect(glView_, &GlView::sectorsRotateRequested, this, [this](const QVector3D &axis, const double angleDeg, const TVector &pivot) {
        if (!documentEditingAllowed() || !glView_)
        {
            return;
        }

        std::vector<std::int32_t> sectors{};
        sectors.reserve(document_.selection().sectors.size());
        for (const std::int32_t idx : document_.selection().sectors)
        {
            sectors.push_back(idx);
        }

        if (sectors.empty())
        {
            return;
        }

        if (axis.lengthSquared() < 1e-10f || !std::isfinite(angleDeg) || angleDeg == 0.0)
        {
            return;
        }

        std::vector<std::int32_t> undoSectors{};
        undoSectors.reserve(sectors.size() * 2u);
        appendSectorTransformUndoSectors(document_.level(), sectors, undoSectors);
        if (undoSectors.empty())
        {
            return;
        }

        const bool preserveUvs = glView_->preserveConnectedSurfaceUvsEnabled();
        const level::ConnectedSurfaceUvPlan uvPlan =
            preserveUvs
            ? captureSectorTransformUvs(document_.level(), sectors, false)
            : level::ConnectedSurfaceUvPlan{};
        TVector rotationAxis{
            static_cast<double>(axis.x()),
            static_cast<double>(axis.y()),
            static_cast<double>(axis.z()) };
        if (rotationAxis.Normalize() == 0.0)
        {
            return;
        }
        const auto transform = [&](Level& level) -> bool
        {
            return level::levelRotateSectorsChecked(
                level, sectors, rotationAxis,
                static_cast<double>(pivot.x),
                static_cast<double>(pivot.y),
                static_cast<double>(pivot.z), angleDeg);
        };
        std::optional<Level> candidate = makeTransformCandidate(
            document_.level(), preserveUvs, uvPlan, transform);
        if ( !candidate )
        {
            reportGeometryTransformCandidateFailure(*glView_, preserveUvs);
            return;
        }

        document_.pushSectorSubsetUndo(
            ui::undo::action("Sector", sectors.size(), "Rotated"),
            editor::DocumentDirty::LevelGeometry,
            undoSectors);
        document_.level() = std::move(*candidate);

        glView_->notifyLevelGeometryChanged();
        updateEditActions();
        rebuildProperties();
        scheduleRebuildChecks(editor::DocumentDirty::LevelGeometry);
    });

    connect(glView_, &GlView::sectorsCleaveRequested, this, [this](const TVector &planeNormal, const TVector &pointOnPlane) {
        if (!documentEditingAllowed() || !glView_)
        {
            return;
        }

        if (document_.selection().sectors.empty())
        {
            glView_->showToast("Cleave: select a sector first", 2500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
            return;
        }

        sed::TVector n{};
        n.x = static_cast<double>(planeNormal.x);
        n.y = static_cast<double>(planeNormal.y);
        n.z = static_cast<double>(planeNormal.z);
        if (!render::cleaveNormalize(n))
        {
            glView_->showToast("Cleave: invalid plane", 2500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
            return;
        }

        const sed::TVector p{
            static_cast<double>(pointOnPlane.x),
            static_cast<double>(pointOnPlane.y),
            static_cast<double>(pointOnPlane.z),
        };

        sed::level::SectorCleaveBatchResult failure;
        failure.error = sed::level::SectorCleaveBatchError::CleaveFailed;
        std::int32_t newActiveSector = -1;
        const std::string undoLabel = ui::undo::action("Sector", document_.selection().sectors.size(), "Cleaved");
        const bool applied = document_.applyUndoable(undoLabel, editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure,
                                                     [&](editor::DocumentState &state) -> bool
                                                     {
                                                         if (state.selection.sectors.empty())
                                                         {
                                                             return false;
                                                         }

                                                         std::vector<std::int32_t> sectors{};
                                                         sectors.reserve(state.selection.sectors.size());
                                                         for (const std::int32_t idx : state.selection.sectors)
                                                         {
                                                             sectors.push_back(idx);
                                                         }
                                                         std::sort(sectors.begin(), sectors.end());

                                                         const std::size_t beforeCount = state.level.sectors.size();
                                                         Level candidate = state.level;
                                                         const std::array planes{
                                                             sed::level::SectorCleavePlane{ n, p },
                                                         };

                                                         bool any = false;
                                                         for (const std::int32_t sectorIndex : sectors)
                                                         {
                                                             if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= candidate.sectors.size())
                                                             {
                                                                 return false;
                                                             }

                                                             const sed::level::SectorCleaveBatchResult result =
                                                                 sed::level::levelCleaveSectorByPlanes(
                                                                     candidate,
                                                                     sectorIndex,
                                                                     planes);
                                                             if (!result.succeeded())
                                                             {
                                                                 failure = result;
                                                                 if (result.error == sed::level::SectorCleaveBatchError::PlaneDoesNotSplit
                                                                     && result.failedPlane == 0)
                                                                 {
                                                                     continue;
                                                                 }
                                                                 return false;
                                                             }
                                                             any = true;
                                                         }

                                                         if (!any)
                                                         {
                                                             return false;
                                                         }

                                                         const std::size_t afterCount = candidate.sectors.size();
                                                         state.level = std::move(candidate);

                                                         // Select only newly created sectors (make the newest one active).
                                                         state.selection.clear();
                                                         for (std::size_t i = beforeCount; i < afterCount; ++i)
                                                         {
                                                             state.selection.sectors.insert(static_cast<std::int32_t>(i));
                                                         }
                                                         if (afterCount > beforeCount)
                                                         {
                                                             newActiveSector = static_cast<std::int32_t>(afterCount - 1);
                                                         }

                                                         return true;
                                                     });

        if (!applied)
        {
            glView_->showToast(ui::viewport::sectorCleaveFailureMessage(failure), 2500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
            return;
        }

        invalidateSurfaceSelectionContextForTopologyEdit();

        if (viewportSectorCleaveAction_ && viewportSectorCleaveAction_->isChecked())
        {
            viewportSectorCleaveAction_->setChecked(false);
        }
        glView_->notifyLevelGeometryChanged();
        if (newActiveSector >= 0)
        {
            glView_->setSelectedSector(newActiveSector);
        }
        glView_->setHighlightedSelection(document_.selection());
        rebuildOutliner();
        rebuildProperties();
        updateEditActions();
    });

    connect(glView_, &GlView::sectorsCleaveBatchRequested, this,
        [this](const QList<TVector>& planeNormals,
               const QList<TVector>& pointsOnPlane,
               const bool closedShape,
               const TVector& interiorPoint,
               const QList<TVector>& strokeSecondPoints,
               const bool bounded)
        {
            if (!documentEditingAllowed() || !glView_ || planeNormals.isEmpty() || planeNormals.size() != pointsOnPlane.size())
            {
                return;
            }
            if (document_.selection().sectors.empty())
            {
                glView_->showToast("Cleave: select a sector first", 2500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
                return;
            }

            std::vector<sed::level::SectorCleavePlane> planes{};
            planes.reserve(static_cast<std::size_t>(planeNormals.size()));
            for (int index = 0; index < planeNormals.size(); ++index)
            {
                const TVector& normal = planeNormals[index];
                const TVector& point = pointsOnPlane[index];
                planes.push_back(sed::level::SectorCleavePlane{
                    TVector{ normal.x, normal.y, normal.z },
                    TVector{ point.x, point.y, point.z },
                });
            }
            std::vector<sed::level::SectorCleaveStroke> strokes{};
            if ( bounded && !closedShape )
            {
                if ( strokeSecondPoints.size() != pointsOnPlane.size() )
                {
                    glView_->showToast(
                        "Multi-cut: bounded strokes do not match the cutting planes",
                        2500,
                        GlView::ToastPlacement::BottomCenter,
                        GlView::ToastGroup::Error);
                    return;
                }
                strokes.reserve(static_cast<std::size_t>(pointsOnPlane.size()));
                for ( int index = 0; index < pointsOnPlane.size(); ++index )
                {
                    const TVector& first = pointsOnPlane[index];
                    const TVector& second = strokeSecondPoints[index];
                    const double dx = second.x - first.x;
                    const double dy = second.y - first.y;
                    const double dz = second.z - first.z;
                    if ( !finiteTransformVector(first) || !finiteTransformVector(second)
                        || !std::isfinite(std::hypot(dx, dy, dz))
                        || std::hypot(dx, dy, dz) <= 0.0 )
                    {
                        glView_->showToast(
                            "Multi-cut: bounded strokes must have finite, distinct endpoints",
                            2500,
                            GlView::ToastPlacement::BottomCenter,
                            GlView::ToastGroup::Error);
                        return;
                    }
                    strokes.push_back(sed::level::SectorCleaveStroke{
                        TVector{ first.x, first.y, first.z },
                        TVector{ second.x, second.y, second.z },
                    });
                }
            }
            const sed::level::SectorCleaveBatchOptions options{
                closedShape,
                TVector{ interiorPoint.x, interiorPoint.y, interiorPoint.z },
                strokes,
                bounded,
            };

            sed::level::SectorCleaveBatchResult failure{};
            bool haveFailure = false;
            std::int32_t newActiveSector = -1;
            const std::string undoLabel = ui::undo::action(
                "Sector",
                document_.selection().sectors.size(),
                planes.size() == 1 ? "Multi-cut" : "Multi-cut (multiple planes)");
            std::optional<CleaveApplyWorkspaceMemento> applyWorkspace{};
            if (glView_->hasStagedCleaveDraft())
            {
                applyWorkspace = captureCleaveApplyWorkspaceMemento();
                if (!applyWorkspace)
                {
                    glView_->showToast(
                        "Multi-cut: this workspace exceeds the safe Apply Undo limit; reduce its history before applying",
                        4200,
                        GlView::ToastPlacement::BottomCenter,
                        GlView::ToastGroup::Error);
                    return;
                }
            }
            const bool applied = document_.applyUndoable(
                undoLabel,
                editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure,
                [&](editor::DocumentState& state) -> bool
                {
                    std::vector<std::int32_t> selected(
                        state.selection.sectors.begin(),
                        state.selection.sectors.end());
                    std::sort(selected.begin(), selected.end());
                    editor::Selection nextSelection{};
                    bool any = false;
                    for (const std::int32_t sectorIndex : selected)
                    {
                        const std::size_t sectorCountBefore = state.level.sectors.size();
                        const sed::level::SectorCleaveBatchResult result =
                            sed::level::levelCleaveSectorByPlanes(state.level, sectorIndex, planes, options);
                        if (!result.succeeded())
                        {
                            // A selected sector that none of the Multi-cut planes
                            // crosses is simply outside this knife. Structural
                            // or partial-sequence failures remain atomic errors.
                            if (result.error == sed::level::SectorCleaveBatchError::PlaneDoesNotSplit
                                && result.failedPlane == 0)
                            {
                                continue;
                            }
                            failure = result;
                            haveFailure = true;
                            return false;
                        }
                        any = true;
                        for (const std::int32_t output : result.outputSectors)
                        {
                            // levelCleaveSectorByPlanes returns every final
                            // descendant, including the original Sector index.
                            // Keep only appended pieces selected so Delete can
                            // immediately remove the cut-off geometry while the
                            // original Sector remains in place.
                            if (output < 0
                                || static_cast<std::size_t>(output) < sectorCountBefore)
                            {
                                continue;
                            }
                            nextSelection.sectors.insert(output);
                            newActiveSector = std::max(newActiveSector, output);
                        }
                    }
                    if (!any)
                    {
                        return false;
                    }
                    state.selection = std::move(nextSelection);
                    return true;
                });

            if (!applied)
            {
                if ( !haveFailure )
                {
                    failure.error = sed::level::SectorCleaveBatchError::PlaneDoesNotSplit;
                }
                const QString message = ui::viewport::sectorCleaveFailureMessage(failure);
                glView_->showToast(message, 3200, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
                return;
            }

            invalidateSurfaceSelectionContextForTopologyEdit();

            if (applyWorkspace)
            {
                if (const std::optional<std::uint64_t> recordId =
                        document_.nextUndoRecordId())
                {
                    recordCleaveApplyWorkspaceMemento(
                        std::move(*applyWorkspace), *recordId);
                }
            }
            glView_->cancelStagedCleaveWorkspace(GlView::CleaveWorkspaceFamily::Sector);
            if (viewportSectorCleaveAction_ && viewportSectorCleaveAction_->isChecked())
            {
                viewportSectorCleaveAction_->setChecked(false);
            }
            glView_->notifyLevelGeometryChanged();
            glView_->setSelectionState(newActiveSector, -1, -1, -1, document_.selection());
            rebuildOutliner();
            rebuildProperties();
            updateEditActions();
        });

    connect(glView_, &GlView::surfacesCleaveRequested, this, [this](const TVector &planeNormal, const TVector &pointOnPlane) {
        if (!documentEditingAllowed() || !glView_)
        {
            return;
        }

        if (document_.selection().surfaces.empty())
        {
            glView_->showToast("Cleave surface: select a surface first", 2500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
            return;
        }

        sed::TVector n{};
        n.x = static_cast<double>(planeNormal.x);
        n.y = static_cast<double>(planeNormal.y);
        n.z = static_cast<double>(planeNormal.z);
        if (!render::cleaveNormalize(n))
        {
            glView_->showToast("Cleave surface: invalid plane", 2500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
            return;
        }

        const sed::TVector p{
            static_cast<double>(pointOnPlane.x),
            static_cast<double>(pointOnPlane.y),
            static_cast<double>(pointOnPlane.z),
        };

        sed::SurfaceRef newActiveSurface{-1, -1};
        const std::string undoLabel = ui::undo::action("Surface", document_.selection().surfaces.size(), "Cleaved");
        const bool applied = document_.applyUndoable(
            undoLabel, editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure, [&](editor::DocumentState &state) -> bool {
                if (state.selection.surfaces.empty())
                {
                    return false;
                }

                std::vector<sed::SurfaceRef> surfaces{};
                surfaces.reserve(state.selection.surfaces.size());
                for (const sed::SurfaceRef ref : state.selection.surfaces)
                {
                    surfaces.push_back(ref);
                }
                std::sort(surfaces.begin(), surfaces.end(), [](const sed::SurfaceRef &a, const sed::SurfaceRef &b) {
                    if (a.sector != b.sector)
                    {
                        return a.sector < b.sector;
                    }
                    return a.surface < b.surface;
                });
                surfaces.erase(std::unique(surfaces.begin(), surfaces.end(),
                                           [](const sed::SurfaceRef &a, const sed::SurfaceRef &b) {
                                               return a.sector == b.sector && a.surface == b.surface;
                                           }),
                               surfaces.end());

                bool any = false;
                // levelCleaveSurface keeps the original face in its slot and
                // appends the positive-side output.  The operation should
                // leave the newly-created output selected, rather than
                // silently retaining the source face.
                editor::Selection newSel{};

                for (const sed::SurfaceRef ref : surfaces)
                {
                    if (ref.sector < 0 || ref.surface < 0)
                    {
                        continue;
                    }
                    if (static_cast<std::size_t>(ref.sector) >= state.level.sectors.size())
                    {
                        continue;
                    }

                    const std::size_t sectorIndex = static_cast<std::size_t>(ref.sector);
                    const std::size_t surfaceIndex = static_cast<std::size_t>(ref.surface);
                    const std::size_t beforeCount = state.level.sectors[sectorIndex].surfaces.size();
                    if (surfaceIndex >= beforeCount)
                    {
                        continue;
                    }

                    if (!sed::level::levelCleaveSurface(state.level, ref, n, p.x, p.y, p.z))
                    {
                        continue;
                    }

                    any = true;

                    if (sectorIndex >= state.level.sectors.size())
                    {
                        continue;
                    }

                    const std::size_t afterCount = state.level.sectors[sectorIndex].surfaces.size();
                    for (std::size_t i = beforeCount; i < afterCount; ++i)
                    {
                        newSel.surfaces.insert(sed::SurfaceRef{static_cast<std::int32_t>(sectorIndex), static_cast<std::int32_t>(i)});
                        newActiveSurface = sed::SurfaceRef{static_cast<std::int32_t>(sectorIndex), static_cast<std::int32_t>(i)};
                    }
                }

                if (!any)
                {
                    return false;
                }

                state.selection.clear();
                state.selection.surfaces = std::move(newSel.surfaces);
                return !state.selection.surfaces.empty();
            });

        if (!applied)
        {
            glView_->showToast("Cleave surface failed", 2500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
            return;
        }

        invalidateSurfaceSelectionContextForTopologyEdit();

        if (viewportSurfaceCleaveAction_ && viewportSurfaceCleaveAction_->isChecked())
        {
            viewportSurfaceCleaveAction_->setChecked(false);
        }
        glView_->notifyLevelGeometryChanged();

        glView_->setSelectionState(newActiveSurface.sector, newActiveSurface.surface, -1, -1, document_.selection());
        rebuildOutliner();
        rebuildProperties();
        updateEditActions();
    });

    connect(glView_, &GlView::surfacesCleaveBatchRequested, this,
        [this](const QList<TVector>& planeNormals,
               const QList<TVector>& pointsOnPlane,
               const bool closedShape,
               const TVector& interiorPoint,
               const QList<TVector>& strokeSecondPoints,
               const bool bounded)
        {
            if (!documentEditingAllowed() || !glView_ || planeNormals.isEmpty() || planeNormals.size() != pointsOnPlane.size())
            {
                return;
            }
            if (document_.selection().surfaces.empty())
            {
                glView_->showToast("Cleave surface: select a surface first", 2500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
                return;
            }

            std::vector<sed::level::SectorCleavePlane> planes{};
            planes.reserve(static_cast<std::size_t>(planeNormals.size()));
            for (int index = 0; index < planeNormals.size(); ++index)
            {
                const TVector& normal = planeNormals[index];
                const TVector& point = pointsOnPlane[index];
                TVector n{ normal.x, normal.y, normal.z };
                if (!render::cleaveNormalize(n))
                {
                    glView_->showToast("Multi-cut surface: invalid plane", 2500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
                    return;
                }
                planes.push_back(sed::level::SectorCleavePlane{
                    n,
                    TVector{ point.x, point.y, point.z },
                });
            }

            std::vector<sed::level::SectorCleaveStroke> strokes{};
            if ( bounded && !closedShape )
            {
                if ( strokeSecondPoints.size() != pointsOnPlane.size() )
                {
                    glView_->showToast(
                        "Multi-cut: bounded strokes do not match the cutting planes",
                        2500,
                        GlView::ToastPlacement::BottomCenter,
                        GlView::ToastGroup::Error);
                    return;
                }
                strokes.reserve(static_cast<std::size_t>(pointsOnPlane.size()));
                for ( int index = 0; index < pointsOnPlane.size(); ++index )
                {
                    const TVector& first = pointsOnPlane[index];
                    const TVector& second = strokeSecondPoints[index];
                    const double dx = second.x - first.x;
                    const double dy = second.y - first.y;
                    const double dz = second.z - first.z;
                    if ( !finiteTransformVector(first) || !finiteTransformVector(second)
                        || !std::isfinite(std::hypot(dx, dy, dz))
                        || std::hypot(dx, dy, dz) <= 0.0 )
                    {
                        glView_->showToast(
                            "Multi-cut: bounded strokes must have finite, distinct endpoints",
                            2500,
                            GlView::ToastPlacement::BottomCenter,
                            GlView::ToastGroup::Error);
                        return;
                    }
                    strokes.push_back(sed::level::SectorCleaveStroke{
                        TVector{ first.x, first.y, first.z },
                        TVector{ second.x, second.y, second.z },
                    });
                }
            }

            std::int32_t failedPlane = -1;
            bool invalidResult = false;
            SurfaceRef newActiveSurface{ -1, -1 };
            const std::string undoLabel = ui::undo::action(
                "Surface",
                document_.selection().surfaces.size(),
                planes.size() == 1 ? "Multi-cut" : "Multi-cut (multiple planes)");
            std::optional<CleaveApplyWorkspaceMemento> applyWorkspace{};
            if (glView_->hasStagedCleaveDraft())
            {
                applyWorkspace = captureCleaveApplyWorkspaceMemento();
                if (!applyWorkspace)
                {
                    glView_->showToast(
                        "Multi-cut: this workspace exceeds the safe Apply Undo limit; reduce its history before applying",
                        4200,
                        GlView::ToastPlacement::BottomCenter,
                        GlView::ToastGroup::Error);
                    return;
                }
            }
            const bool applied = document_.applyUndoable(
                undoLabel,
                editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure,
                [&](editor::DocumentState& state) -> bool
                {
                    struct SurfaceCleaveBaseline
                    {
                        std::vector<std::int32_t> vertices{};
                        TVector normal{};
                        std::optional<SurfaceRef> adjoin{};
                    };

                    // Surface cleave preserves the sector volume and only
                    // changes split faces, shared-edge faces, and mirrored
                    // adjoins. Remember the original surface structures so
                    // validation ignores unrelated pre-existing geometry.
                    std::vector<std::vector<SurfaceCleaveBaseline>> baseline{};
                    baseline.reserve(state.level.sectors.size());
                    for (const Sector& sector : state.level.sectors)
                    {
                        std::vector<SurfaceCleaveBaseline> surfaces{};
                        surfaces.reserve(sector.surfaces.size());
                        for (const Surface& surface : sector.surfaces)
                        {
                            surfaces.push_back(SurfaceCleaveBaseline{
                                surface.vertices,
                                surface.normal,
                                surface.adjoin,
                            });
                        }
                        baseline.push_back(std::move(surfaces));
                    }

                    std::vector<SurfaceRef> descendants(
                        state.selection.surfaces.begin(),
                        state.selection.surfaces.end());
                    std::sort(descendants.begin(), descendants.end(), [](const SurfaceRef& lhs, const SurfaceRef& rhs)
                        {
                            return lhs.sector == rhs.sector
                                ? lhs.surface < rhs.surface
                                : lhs.sector < rhs.sector;
                        });
                    descendants.erase(std::unique(descendants.begin(), descendants.end()), descendants.end());
                    const std::vector<SurfaceRef> originalSurfaces = descendants;
                    std::vector<SurfaceRef> activeDescendants = descendants;
                    const auto surfaceLineageKey = [](const SurfaceRef ref) noexcept
                    {
                        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(ref.sector)) << 32u)
                            | static_cast<std::uint32_t>(ref.surface);
                    };
                    std::unordered_map<std::uint64_t,
                        std::vector<sed::level::CleaveStrokeHalfSpace>> strokeAncestry{};
                    if ( bounded && !closedShape )
                    {
                        strokeAncestry.reserve(descendants.size());
                        for ( const SurfaceRef ref : descendants )
                        {
                            strokeAncestry.emplace(surfaceLineageKey(ref),
                                std::vector<sed::level::CleaveStrokeHalfSpace>{});
                        }
                    }
                    bool splitAnyPlane = false;
                    std::vector<std::size_t> successfulIndependentPlanes{};
                    if (!closedShape)
                    {
                        successfulIndependentPlanes.reserve(planes.size());
                    }

                    for (std::size_t planeIndex = 0; planeIndex < planes.size(); ++planeIndex)
                    {
                        const auto& plane = planes[planeIndex];
                        const double interiorDistance = closedShape
                            ? plane.normal.dot(
                                static_cast<double>(interiorPoint.x) - plane.point.x,
                                static_cast<double>(interiorPoint.y) - plane.point.y,
                                static_cast<double>(interiorPoint.z) - plane.point.z)
                            : 0.0;
                        if (closedShape && std::abs(interiorDistance) <= sed::math::kGeometryEpsilon)
                        {
                            failedPlane = static_cast<std::int32_t>(planeIndex);
                            return false;
                        }

                        const std::vector<SurfaceRef> beforePlane = closedShape
                            ? activeDescendants
                            : descendants;
                        std::vector<SurfaceRef> nextActiveDescendants{};
                        bool planeSplit = false;
                        for (const SurfaceRef ref : beforePlane)
                        {
                            if (ref.sector < 0 || ref.surface < 0
                                || static_cast<std::size_t>(ref.sector) >= state.level.sectors.size())
                            {
                                continue;
                            }
                            Sector& sector = state.level.sectors[static_cast<std::size_t>(ref.sector)];
                            if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
                            {
                                continue;
                            }
                            std::vector<sed::level::CleaveStrokeHalfSpace> inheritedStrokeAncestry{};
                            if ( bounded && !closedShape )
                            {
                                const auto ancestry = strokeAncestry.find(surfaceLineageKey(ref));
                                if ( ancestry == strokeAncestry.end() )
                                {
                                    invalidResult = true;
                                    return false;
                                }
                                if ( !sed::level::cleaveStrokeReachesHalfSpaces(
                                        strokes[planeIndex],
                                        ancestry->second) )
                                {
                                    continue;
                                }
                                inheritedStrokeAncestry = ancestry->second;
                            }
                            const Surface& sourceSurface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
                            std::optional<SurfaceRef> trackedMirrorSource{};
                            std::vector<sed::level::CleaveStrokeHalfSpace> inheritedMirrorAncestry{};
                            if ( bounded && !closedShape && sourceSurface.adjoin )
                            {
                                const auto mirrorAncestry = strokeAncestry.find(
                                    surfaceLineageKey(*sourceSurface.adjoin));
                                if ( mirrorAncestry != strokeAncestry.end() )
                                {
                                    trackedMirrorSource = *sourceSurface.adjoin;
                                    inheritedMirrorAncestry = mirrorAncestry->second;
                                }
                            }
                            bool hasNegative = false;
                            bool hasPositive = false;
                            for (const std::int32_t vertexIndex : sourceSurface.vertices)
                            {
                                if (vertexIndex < 0
                                    || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
                                {
                                    invalidResult = true;
                                    return false;
                                }
                                const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
                                const double distance = plane.normal.dot(
                                    vertex.x - plane.point.x,
                                    vertex.y - plane.point.y,
                                    vertex.z - plane.point.z);
                                if (std::abs(distance) <= sed::math::kGeometryEpsilon)
                                {
                                    continue;
                                }
                                hasNegative = hasNegative || distance < 0.0;
                                hasPositive = hasPositive || distance > 0.0;
                            }
                            if (!hasNegative || !hasPositive)
                            {
                                if (closedShape
                                    && ((interiorDistance < 0.0 && hasNegative)
                                        || (interiorDistance > 0.0 && hasPositive)))
                                {
                                    nextActiveDescendants.push_back(ref);
                                }
                                continue;
                            }

                            const std::size_t beforeCount = sector.surfaces.size();
                            if (!sed::level::levelCleaveSurface(
                                    state.level,
                                    ref,
                                    plane.normal,
                                    plane.point.x,
                                    plane.point.y,
                                    plane.point.z))
                            {
                                // This face was proven to cross the plane, so
                                // a core rejection is a failed batch stage,
                                // not a boundary no-op eligible for duplicate
                                // supporting-plane handling.
                                failedPlane = static_cast<std::int32_t>(planeIndex);
                                return false;
                            }
                            planeSplit = true;
                            splitAnyPlane = true;
                            const std::size_t afterCount = state.level.sectors[static_cast<std::size_t>(ref.sector)].surfaces.size();
                            if (afterCount != beforeCount + 1)
                            {
                                invalidResult = true;
                                return false;
                            }
                            const SurfaceRef added{
                                ref.sector,
                                static_cast<std::int32_t>(beforeCount),
                            };
                            std::optional<std::pair<SurfaceRef, SurfaceRef>> trackedMirrorChildren{};
                            if ( bounded && !closedShape )
                            {
                                auto negativeAncestry = inheritedStrokeAncestry;
                                negativeAncestry.push_back(sed::level::CleaveStrokeHalfSpace{
                                    plane.normal,
                                    plane.point,
                                    false,
                                });
                                inheritedStrokeAncestry.push_back(sed::level::CleaveStrokeHalfSpace{
                                    plane.normal,
                                    plane.point,
                                    true,
                                });
                                strokeAncestry[surfaceLineageKey(ref)] = std::move(negativeAncestry);
                                strokeAncestry[surfaceLineageKey(added)] = std::move(inheritedStrokeAncestry);

                                if ( trackedMirrorSource )
                                {
                                    const Sector& splitSector = state.level.sectors[
                                        static_cast<std::size_t>(ref.sector)];
                                    if ( static_cast<std::size_t>(ref.surface) >= splitSector.surfaces.size()
                                        || static_cast<std::size_t>(added.surface) >= splitSector.surfaces.size() )
                                    {
                                        invalidResult = true;
                                        return false;
                                    }
                                    const std::optional<SurfaceRef> negativeMirror =
                                        splitSector.surfaces[static_cast<std::size_t>(ref.surface)].adjoin;
                                    const std::optional<SurfaceRef> positiveMirror =
                                        splitSector.surfaces[static_cast<std::size_t>(added.surface)].adjoin;
                                    const auto validMirrorRef = [&state](const std::optional<SurfaceRef>& mirror)
                                    {
                                        return mirror
                                            && mirror->sector >= 0
                                            && mirror->surface >= 0
                                            && static_cast<std::size_t>(mirror->sector)
                                                < state.level.sectors.size()
                                            && static_cast<std::size_t>(mirror->surface)
                                                < state.level.sectors[static_cast<std::size_t>(mirror->sector)]
                                                    .surfaces.size();
                                    };
                                    if ( !validMirrorRef(negativeMirror)
                                        || !validMirrorRef(positiveMirror)
                                        || *negativeMirror == *positiveMirror
                                        || !state.level.sectors[static_cast<std::size_t>(negativeMirror->sector)]
                                            .surfaces[static_cast<std::size_t>(negativeMirror->surface)].adjoin
                                        || *state.level.sectors[static_cast<std::size_t>(negativeMirror->sector)]
                                            .surfaces[static_cast<std::size_t>(negativeMirror->surface)].adjoin != ref
                                        || !state.level.sectors[static_cast<std::size_t>(positiveMirror->sector)]
                                            .surfaces[static_cast<std::size_t>(positiveMirror->surface)].adjoin
                                        || *state.level.sectors[static_cast<std::size_t>(positiveMirror->sector)]
                                            .surfaces[static_cast<std::size_t>(positiveMirror->surface)].adjoin != added )
                                    {
                                        invalidResult = true;
                                        return false;
                                    }
                                    auto negativeMirrorAncestry = inheritedMirrorAncestry;
                                    negativeMirrorAncestry.push_back(sed::level::CleaveStrokeHalfSpace{
                                        plane.normal,
                                        plane.point,
                                        false,
                                    });
                                    inheritedMirrorAncestry.push_back(sed::level::CleaveStrokeHalfSpace{
                                        plane.normal,
                                        plane.point,
                                        true,
                                    });
                                    strokeAncestry[surfaceLineageKey(*negativeMirror)] =
                                        std::move(negativeMirrorAncestry);
                                    strokeAncestry[surfaceLineageKey(*positiveMirror)] =
                                        std::move(inheritedMirrorAncestry);
                                    trackedMirrorChildren = std::pair{
                                        *negativeMirror,
                                        *positiveMirror,
                                    };
                                }
                            }
                            for (std::size_t surfaceIndex = beforeCount; surfaceIndex < afterCount; ++surfaceIndex)
                            {
                                descendants.push_back(SurfaceRef{
                                    ref.sector,
                                    static_cast<std::int32_t>(surfaceIndex),
                                });
                            }
                            if ( trackedMirrorChildren )
                            {
                                for ( const SurfaceRef mirrorChild : {
                                        trackedMirrorChildren->first,
                                        trackedMirrorChildren->second,
                                    } )
                                {
                                    if ( std::find(descendants.begin(), descendants.end(), mirrorChild)
                                        == descendants.end() )
                                    {
                                        descendants.push_back(mirrorChild);
                                    }
                                }
                            }
                            if (closedShape)
                            {
                                // sysCleaveSurface keeps the negative side in
                                // the source slot and appends the positive side.
                                nextActiveDescendants.push_back(interiorDistance < 0.0
                                    ? ref
                                    : added);
                            }
                        }
                        if (closedShape)
                        {
                            activeDescendants = std::move(nextActiveDescendants);
                            if (activeDescendants.empty())
                            {
                                failedPlane = static_cast<std::int32_t>(planeIndex);
                                return false;
                            }
                        }
                        else if (!planeSplit)
                        {
                            const bool repeatsAppliedSupportingPlane = std::any_of(
                                successfulIndependentPlanes.begin(),
                                successfulIndependentPlanes.end(),
                                [&](const std::size_t earlierIndex)
                                {
                                    const auto& earlier = planes[earlierIndex];
                                    if ( ui::viewport::cleaveSupportingPlanesEquivalent(
                                            earlier.normal,
                                            earlier.point,
                                            plane.normal,
                                            plane.point) )
                                    {
                                        return true;
                                    }
                                    // Both cut modes retain the native endpoints.
                                    // Plane-only requests keep the exact rule;
                                    // rounded finite continuations additionally
                                    // need an exact shared joint and coordinate-
                                    // derived construction bounds.
                                    return strokeSecondPoints.size() == pointsOnPlane.size()
                                        && ui::viewport::cleaveStrokesShareRoundedSupportingPlane(
                                            earlier.normal,
                                            earlier.point,
                                            strokeSecondPoints[static_cast<qsizetype>(earlierIndex)],
                                            plane.normal,
                                            plane.point,
                                            strokeSecondPoints[static_cast<qsizetype>(planeIndex)]);
                                });
                            if (!repeatsAppliedSupportingPlane)
                            {
                                failedPlane = static_cast<std::int32_t>(planeIndex);
                                return false;
                            }
                        }
                        else
                        {
                            successfulIndependentPlanes.push_back(planeIndex);
                        }
                    }

                    if (closedShape && !splitAnyPlane)
                    {
                        failedPlane = 0;
                        return false;
                    }

                    std::unordered_set<std::int32_t> affectedSectors{};
                    for (const SurfaceRef ref : descendants)
                    {
                        affectedSectors.insert(ref.sector);
                    }
                    const std::vector<std::int32_t> initiallyAffected(
                        affectedSectors.begin(),
                        affectedSectors.end());
                    for (const std::int32_t sectorIndex : initiallyAffected)
                    {
                        if (sectorIndex < 0
                            || static_cast<std::size_t>(sectorIndex) >= state.level.sectors.size())
                        {
                            invalidResult = true;
                            return false;
                        }
                        for (const Surface& surface : state.level.sectors[static_cast<std::size_t>(sectorIndex)].surfaces)
                        {
                            if (surface.adjoin)
                            {
                                affectedSectors.insert(surface.adjoin->sector);
                            }
                        }
                    }

                    for (const std::int32_t sectorIndex : affectedSectors)
                    {
                        if (sectorIndex < 0
                            || static_cast<std::size_t>(sectorIndex) >= state.level.sectors.size())
                        {
                            invalidResult = true;
                            return false;
                        }
                        const Sector& sector = state.level.sectors[static_cast<std::size_t>(sectorIndex)];
                        for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
                        {
                            const Surface& surface = sector.surfaces[surfaceIndex];
                            const bool existed = static_cast<std::size_t>(sectorIndex) < baseline.size()
                                && surfaceIndex < baseline[static_cast<std::size_t>(sectorIndex)].size();
                            const SurfaceCleaveBaseline* before = existed
                                ? &baseline[static_cast<std::size_t>(sectorIndex)][surfaceIndex]
                                : nullptr;
                            const bool geometryChanged = !before
                                || before->vertices != surface.vertices
                                || before->normal.x != surface.normal.x
                                || before->normal.y != surface.normal.y
                                || before->normal.z != surface.normal.z;
                            const bool adjoinChanged = !before || before->adjoin != surface.adjoin;

                            if (!surface.adjoin || (!geometryChanged && !adjoinChanged))
                            {
                                continue;
                            }
                            const SurfaceRef mirror = *surface.adjoin;
                            if (mirror.sector < 0 || mirror.surface < 0
                                || static_cast<std::size_t>(mirror.sector) >= state.level.sectors.size())
                            {
                                invalidResult = true;
                                return false;
                            }
                            const Sector& otherSector = state.level.sectors[static_cast<std::size_t>(mirror.sector)];
                            if (static_cast<std::size_t>(mirror.surface) >= otherSector.surfaces.size())
                            {
                                invalidResult = true;
                                return false;
                            }
                            const Surface& otherSurface = otherSector.surfaces[static_cast<std::size_t>(mirror.surface)];
                            const SurfaceRef expected{ sectorIndex, static_cast<std::int32_t>(surfaceIndex) };
                            // Each successful levelCleaveSurface call has
                            // already validated its complete affected portal
                            // family against the immutable cleave profile.
                            // Keep this transaction-level guard focused on the
                            // references themselves; the public strict overlap
                            // helper would reject valid inherited rounded
                            // boundaries that the core profile accepted.
                            if (!otherSurface.adjoin || *otherSurface.adjoin != expected)
                            {
                                invalidResult = true;
                                return false;
                            }
                        }
                    }

                    state.selection.clear();
                    for (const SurfaceRef ref : descendants)
                    {
                        // The original source face remains in its original
                        // slot after each split.  Only appended descendants
                        // are new output pieces and should become the next
                        // active selection.
                        if (std::find(originalSurfaces.begin(), originalSurfaces.end(), ref)
                            != originalSurfaces.end())
                        {
                            continue;
                        }
                        state.selection.surfaces.insert(ref);
                        newActiveSurface = ref;
                    }
                    return !state.selection.surfaces.empty();
                });

            if (!applied)
            {
                const QString message = invalidResult
                    ? QStringLiteral("Cleave surface rejected: result has invalid topology or adjoin reciprocity")
                    : failedPlane >= 0
                    ? QStringLiteral("Cleave surface failed: line %1 does not split the selected faces").arg(failedPlane + 1)
                    : QStringLiteral("Cleave surface failed");
                glView_->showToast(message, 3000, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
                return;
            }

            invalidateSurfaceSelectionContextForTopologyEdit();

            if (applyWorkspace)
            {
                if (const std::optional<std::uint64_t> recordId =
                        document_.nextUndoRecordId())
                {
                    recordCleaveApplyWorkspaceMemento(
                        std::move(*applyWorkspace), *recordId);
                }
            }
            glView_->cancelStagedCleaveWorkspace(GlView::CleaveWorkspaceFamily::Surface);
            if (viewportSurfaceCleaveAction_ && viewportSurfaceCleaveAction_->isChecked())
            {
                viewportSurfaceCleaveAction_->setChecked(false);
            }
            glView_->notifyLevelGeometryChanged();
            glView_->setSelectionState(
                newActiveSurface.sector,
                newActiveSurface.surface,
                -1,
                -1,
                document_.selection());
            rebuildOutliner();
            rebuildProperties();
            updateEditActions();
        });

    connect(glView_, &GlView::surfaceEdgeCleaveRequested, this,
            [this](const std::int32_t sectorIndex, const std::int32_t surfaceIndex, const std::int32_t edgeIndex, const TVector &planeNormal,
                   const TVector &pointOnPlane) {
                if (!documentEditingAllowed() || !glView_)
                {
                    return;
                }

                if (sectorIndex < 0 || surfaceIndex < 0 || edgeIndex < 0)
                {
                    glView_->showToast("Cleave edge: invalid selection", 2500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
                    return;
                }

                sed::TVector n{};
                n.x = static_cast<double>(planeNormal.x);
                n.y = static_cast<double>(planeNormal.y);
                n.z = static_cast<double>(planeNormal.z);
                if (!render::cleaveNormalize(n))
                {
                    glView_->showToast("Cleave edge: invalid plane", 2500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
                    return;
                }

                const sed::TVector p{
                    static_cast<double>(pointOnPlane.x),
                    static_cast<double>(pointOnPlane.y),
                    static_cast<double>(pointOnPlane.z),
                };

                const sed::SurfaceRef ref{sectorIndex, surfaceIndex};

                const bool applied = document_.applyUndoable("Surface: Edge cleaved", editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure,
                                                             [&](editor::DocumentState &state) -> bool {
                                                                 if (ref.sector < 0 || ref.surface < 0)
                                                                 {
                                                                     return false;
                                                                 }
                                                                 if (static_cast<std::size_t>(ref.sector) >= state.level.sectors.size())
                                                                 {
                                                                     return false;
                                                                 }

                                                                 const Sector &sector = state.level.sectors[static_cast<std::size_t>(ref.sector)];
                                                                 if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
                                                                 {
                                                                     return false;
                                                                 }

                                                                 return sed::level::levelCleaveEdge(state.level, ref, edgeIndex, n, p.x, p.y, p.z);
                                                             });

                if (!applied)
                {
                    glView_->showToast("Cleave edge failed", 2500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
                    return;
                }

                invalidateSurfaceSelectionContextForTopologyEdit();

                if (viewportSurfaceCleaveAction_ && viewportSurfaceCleaveAction_->isChecked())
                {
                    viewportSurfaceCleaveAction_->setChecked(false);
                }
                glView_->notifyLevelGeometryChanged();
                glView_->setHighlightedSelection(document_.selection());
                rebuildOutliner();
                rebuildProperties();
                updateEditActions();
                scheduleRebuildChecks(editor::DocumentDirty::LevelGeometry);
            });

    connect(glView_, &GlView::surfacesSnapTransformRequested, this,
            [this](const TVector &translation, const QVector3D &axis, const double angleDeg, const TVector &pivot) {
                if (!documentEditingAllowed() || !glView_)
                {
                    return;
                }

                std::vector<sed::SurfaceRef> surfaces{};
                surfaces.reserve(document_.selection().surfaces.size());
                for (const sed::SurfaceRef ref : document_.selection().surfaces)
                {
                    surfaces.push_back(ref);
                }

                if (surfaces.empty())
                {
                    return;
                }

                const bool hasTranslation = (translation.x != 0.0 || translation.y != 0.0 || translation.z != 0.0);
                const bool hasRotation = axis.lengthSquared() > 1.0e-10f && std::abs(angleDeg) > 1.0e-9;
                if (!hasTranslation && !hasRotation)
                {
                    return;
                }

                std::vector<std::int32_t> undoSectors{};
                undoSectors.reserve(surfaces.size() * 2u);
                appendSurfaceTransformUndoSectors(document_.level(), surfaces, undoSectors);
                if (undoSectors.empty())
                {
                    return;
                }

                const bool preserveUvs =
                    glView_->preserveConnectedSurfaceUvsEnabled();
                const level::ConnectedSurfaceUvPlan uvPlan =
                    preserveUvs
                    ? captureSurfaceTransformUvs(
                        document_.level(), surfaces, false)
                    : level::ConnectedSurfaceUvPlan{};
                const auto transform = [&](Level& level) -> bool
                {
                    if (hasRotation)
                    {
                        TVector normal{
                            static_cast<double>(axis.x()),
                            static_cast<double>(axis.y()),
                            static_cast<double>(axis.z()) };
                        if (normal.Normalize() != 0.0)
                        {
                            if ( !level::levelRotateSurfacesChecked(
                                level, surfaces, normal,
                                static_cast<double>(pivot.x),
                                static_cast<double>(pivot.y),
                                static_cast<double>(pivot.z), angleDeg) )
                            {
                                return false;
                            }
                        }
                        else
                        {
                            return false;
                        }
                    }
                    if (hasTranslation)
                    {
                        if ( !level::levelTranslateSurfacesChecked(
                            level, surfaces,
                            static_cast<double>(translation.x),
                            static_cast<double>(translation.y),
                            static_cast<double>(translation.z)) )
                        {
                            return false;
                        }
                    }
                    return true;
                };
                std::optional<Level> candidate = makeTransformCandidate(
                    document_.level(), preserveUvs, uvPlan, transform);
                if ( !candidate )
                {
                    reportGeometryTransformCandidateFailure(*glView_, preserveUvs);
                    return;
                }

                document_.pushSurfaceSubsetUndo(
                    ui::undo::action(
                        "Surface",
                        surfaces.size(),
                        hasRotation && hasTranslation
                            ? "Moved and rotated"
                            : hasRotation ? "Rotated" : "Moved"),
                    editor::DocumentDirty::LevelGeometry,
                    undoSectors);
                document_.level() = std::move(*candidate);

                glView_->notifyLevelGeometryChanged();
                updateEditActions();
                rebuildProperties();
                scheduleRebuildChecks(editor::DocumentDirty::LevelGeometry);
            });

    connect(glView_, &GlView::surfacesTranslateRequested, this, [this](const double dx, const double dy, const double dz) {
        if (!documentEditingAllowed() || !glView_)
        {
            return;
        }

        std::vector<sed::SurfaceRef> surfaces{};
        surfaces.reserve(document_.selection().surfaces.size());
        for (const sed::SurfaceRef ref : document_.selection().surfaces)
        {
            surfaces.push_back(ref);
        }

        if (surfaces.empty())
        {
            return;
        }

        std::vector<std::int32_t> undoSectors{};
        undoSectors.reserve(surfaces.size() * 2u);
        appendSurfaceTransformUndoSectors(document_.level(), surfaces, undoSectors);
        if (undoSectors.empty())
        {
            return;
        }

        const bool preserveUvs = glView_->preserveConnectedSurfaceUvsEnabled();
        const level::ConnectedSurfaceUvPlan uvPlan =
            preserveUvs
            ? captureSurfaceTransformUvs(document_.level(), surfaces, false)
            : level::ConnectedSurfaceUvPlan{};
        const auto transform = [&](Level& level) -> bool
        {
            return level::levelTranslateSurfacesChecked(level, surfaces, dx, dy, dz);
        };
        std::optional<Level> candidate = makeTransformCandidate(
            document_.level(), preserveUvs, uvPlan, transform);
        if ( !candidate )
        {
            reportGeometryTransformCandidateFailure(*glView_, preserveUvs);
            return;
        }

        document_.pushSurfaceSubsetUndo(
            ui::undo::action("Surface", surfaces.size(), "Moved"),
            editor::DocumentDirty::LevelGeometry,
            undoSectors);
        document_.level() = std::move(*candidate);
        glView_->notifyLevelGeometryChanged();
        updateEditActions();
        rebuildProperties();
        scheduleRebuildChecks(editor::DocumentDirty::LevelGeometry);
    });

    connect(glView_, &GlView::surfacesScaleRequested, this, [this](const QVector3D &axis, const double scaleFactor, const TVector &pivot) {
        if (!documentEditingAllowed() || !glView_)
        {
            return;
        }

        if (!std::isfinite(scaleFactor) || scaleFactor <= 0.0 || std::abs(scaleFactor - 1.0) < 1e-9)
        {
            return;
        }

        std::vector<sed::SurfaceRef> surfaces{};
        surfaces.reserve(document_.selection().surfaces.size());
        for (const sed::SurfaceRef ref : document_.selection().surfaces)
        {
            surfaces.push_back(ref);
        }

        if (surfaces.empty())
        {
            return;
        }

        ScaleHow how = ScaleHow::xyz;
        if (axis.lengthSquared() > 1e-8f)
        {
            const QVector3D n = axis.normalized();
            const float ax = std::abs(n.x());
            const float ay = std::abs(n.y());
            const float az = std::abs(n.z());
            if (ax >= ay && ax >= az)
            {
                how = ScaleHow::x;
            }
            else if (ay >= az)
            {
                how = ScaleHow::y;
            }
            else
            {
                how = ScaleHow::z;
            }
        }

        ScaleData sd{};
        sd.cx = static_cast<double>(pivot.x);
        sd.cy = static_cast<double>(pivot.y);
        sd.cz = static_cast<double>(pivot.z);
        sd.sfactor = scaleFactor;
        sd.how = how;

        std::vector<std::int32_t> undoSectors{};
        undoSectors.reserve(surfaces.size() * 2u);
        appendSurfaceTransformUndoSectors(document_.level(), surfaces, undoSectors);
        if (undoSectors.empty())
        {
            return;
        }

        const bool preserveUvs = glView_->preserveConnectedSurfaceUvsEnabled();
        const level::ConnectedSurfaceUvPlan uvPlan =
            preserveUvs
            ? captureSurfaceTransformUvs(document_.level(), surfaces, true)
            : level::ConnectedSurfaceUvPlan{};
        const auto transform = [&](Level& level) -> bool
        {
            return level::levelScaleSurfacesChecked(level, surfaces, sd, false);
        };
        std::optional<Level> candidate = makeTransformCandidate(
            document_.level(), preserveUvs, uvPlan, transform);
        if ( !candidate )
        {
            reportGeometryTransformCandidateFailure(*glView_, preserveUvs);
            return;
        }
        if ( !render::preservesAffectedScaleGeometry(
                document_.level(), *candidate, undoSectors, false) )
        {
            glView_->showToast(
                QStringLiteral("Scale cancelled because it would produce degenerate geometry"),
                4000,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
            return;
        }

        document_.pushSurfaceSubsetUndo(
            ui::undo::action("Surface", surfaces.size(), "Scaled"),
            editor::DocumentDirty::LevelGeometry,
            undoSectors);
        document_.level() = std::move(*candidate);
        glView_->notifyLevelGeometryChanged();
        updateEditActions();
        rebuildProperties();
        scheduleRebuildChecks(editor::DocumentDirty::LevelGeometry);
    });

    connect(glView_, &GlView::surfaceVertexTranslateRequested, this,
            [this](const std::int32_t sectorIndex, const std::int32_t vertexIndex, const double dx, const double dy, const double dz) {
                if (!documentEditingAllowed() || !glView_)
                {
                    return;
                }

                if (sectorIndex < 0 || vertexIndex < 0)
                {
                    return;
                }

                std::vector<sed::VertexRef> refs{};
                refs.reserve(document_.selection().surfaceVertices.size() + 1);
                auto addRef = [&](const sed::VertexRef ref)
                {
                    if (ref.sector < 0 || ref.vertex < 0)
                    {
                        return;
                    }
                    const bool duplicate = std::any_of(refs.begin(), refs.end(), [&](const sed::VertexRef existing)
                    {
                        return existing.sector == ref.sector && existing.vertex == ref.vertex;
                    });
                    if (!duplicate)
                    {
                        refs.push_back(ref);
                    }
                };

                const Level& level = document_.level();
                for (const editor::SurfaceVertexRef& selected : document_.selection().surfaceVertices)
                {
                    if (selected.surface.sector < 0 || selected.surface.surface < 0 || selected.localVertex < 0
                        || static_cast<std::size_t>(selected.surface.sector) >= level.sectors.size())
                    {
                        continue;
                    }
                    const Sector& sector = level.sectors[static_cast<std::size_t>(selected.surface.sector)];
                    if (static_cast<std::size_t>(selected.surface.surface) >= sector.surfaces.size())
                    {
                        continue;
                    }
                    const Surface& surface = sector.surfaces[static_cast<std::size_t>(selected.surface.surface)];
                    if (static_cast<std::size_t>(selected.localVertex) >= surface.vertices.size())
                    {
                        continue;
                    }
                    addRef(sed::VertexRef{
                        selected.surface.sector,
                        surface.vertices[static_cast<std::size_t>(selected.localVertex)] });
                }
                if (refs.empty())
                {
                    addRef(sed::VertexRef{sectorIndex, vertexIndex});
                }
                if (refs.empty())
                {
                    return;
                }

                std::vector<std::int32_t> undoSectors{};
                undoSectors.reserve(refs.size());
                std::unordered_set<std::int32_t> uniqueUndoSectors{};
                for (const sed::VertexRef ref : refs)
                {
                    if (ref.sector >= 0
                        && static_cast<std::size_t>(ref.sector) < level.sectors.size()
                        && uniqueUndoSectors.insert(ref.sector).second)
                    {
                        undoSectors.push_back(ref.sector);
                    }
                }
                if (undoSectors.empty())
                {
                    return;
                }

                const bool preserveUvs =
                    glView_->preserveConnectedSurfaceUvsEnabled();
                const level::ConnectedSurfaceUvPlan uvPlan = preserveUvs
                    ? level::captureConnectedSurfaceUvPlan(
                        document_.level(), refs, false)
                    : level::ConnectedSurfaceUvPlan{};
                const auto transform = [&](Level& level) -> bool
                {
                    return level::levelTranslateVerticesChecked(level, refs, dx, dy, dz);
                };
                std::optional<Level> candidate = makeTransformCandidate(
                    document_.level(), preserveUvs, uvPlan, transform);
                if ( !candidate )
                {
                    reportGeometryTransformCandidateFailure(*glView_, preserveUvs);
                    return;
                }

                document_.pushSurfaceSubsetUndo(
                    ui::undo::action("Vertex", refs.size(), "Moved"),
                    editor::DocumentDirty::LevelGeometry,
                    undoSectors);
                document_.level() = std::move(*candidate);
                glView_->notifyLevelGeometryChanged();
                updateEditActions();
                rebuildProperties();
                scheduleRebuildChecks(editor::DocumentDirty::LevelGeometry);
            });

    connect(glView_, &GlView::surfaceEdgeTranslateRequested, this,
            [this](const std::int32_t sectorIndex, const std::int32_t surfaceIndex, const std::int32_t edgeIndex, const double dx, const double dy,
                   const double dz) {
                if (!documentEditingAllowed() || !glView_)
                {
                    return;
                }

                if (sectorIndex < 0 || surfaceIndex < 0 || edgeIndex < 0)
                {
                    return;
                }

                std::vector<std::int32_t> undoSectors{};
                undoSectors.reserve(1);
                if (static_cast<std::size_t>(sectorIndex) < document_.level().sectors.size())
                {
                    undoSectors.push_back(sectorIndex);
                }
                if (undoSectors.empty())
                {
                    return;
                }

                const Sector& sourceSector = document_.level().sectors[
                    static_cast<std::size_t>(sectorIndex)];
                if (static_cast<std::size_t>(surfaceIndex)
                        >= sourceSector.surfaces.size()
                    || sourceSector.surfaces[static_cast<std::size_t>(surfaceIndex)]
                        .vertices.size() < 2u
                    || static_cast<std::size_t>(edgeIndex)
                        >= sourceSector.surfaces[static_cast<std::size_t>(surfaceIndex)]
                            .vertices.size())
                {
                    return;
                }
                const Surface& sourceSurface = sourceSector.surfaces[
                    static_cast<std::size_t>(surfaceIndex)];
                const std::size_t edge = static_cast<std::size_t>(edgeIndex);
                const std::array<level::EdgeRef, 1> refs{
                    level::EdgeRef{ sectorIndex, surfaceIndex, edgeIndex } };
                const std::array<VertexRef, 2> movedVertices{
                    VertexRef{ sectorIndex, sourceSurface.vertices[edge] },
                    VertexRef{
                        sectorIndex,
                        sourceSurface.vertices[(edge + 1u)
                            % sourceSurface.vertices.size()] },
                };
                const bool preserveUvs =
                    glView_->preserveConnectedSurfaceUvsEnabled();
                const level::ConnectedSurfaceUvPlan uvPlan = preserveUvs
                    ? level::captureConnectedSurfaceUvPlan(
                        document_.level(), movedVertices, false)
                    : level::ConnectedSurfaceUvPlan{};
                const auto transform = [&](Level& level) -> bool
                {
                    return level::levelTranslateEdgesChecked(level, refs, dx, dy, dz);
                };
                std::optional<Level> candidate = makeTransformCandidate(
                    document_.level(), preserveUvs, uvPlan, transform);
                if ( !candidate )
                {
                    reportGeometryTransformCandidateFailure(*glView_, preserveUvs);
                    return;
                }

                document_.pushSurfaceSubsetUndo(
                    ui::undo::action("Edge", 1u, "Moved"),
                    editor::DocumentDirty::LevelGeometry,
                    undoSectors);
                document_.level() = std::move(*candidate);
                glView_->notifyLevelGeometryChanged();
                updateEditActions();
                rebuildProperties();
                scheduleRebuildChecks(editor::DocumentDirty::LevelGeometry);
            });

    connect(glView_, &GlView::surfaceEdgeSnapTransformRequested, this,
            [this](const std::int32_t sectorIndex,
                   const std::int32_t surfaceIndex,
                   const std::int32_t edgeIndex,
                   const TVector& translation,
                   const QVector3D& axis,
                   const double angleDeg,
                   const TVector& pivot) {
                if (!documentEditingAllowed() || !glView_
                    || sectorIndex < 0 || surfaceIndex < 0 || edgeIndex < 0)
                {
                    return;
                }

                if (static_cast<std::size_t>(sectorIndex)
                    >= document_.level().sectors.size())
                {
                    return;
                }
                const bool hasTranslation =
                    (translation.x != 0.0 || translation.y != 0.0 || translation.z != 0.0);
                const bool hasRotation = axis.lengthSquared() > 1.0e-10f
                    && std::abs(angleDeg) > 1.0e-9;
                if (!hasTranslation && !hasRotation)
                {
                    return;
                }

                const std::vector<std::int32_t> undoSectors{sectorIndex};
                const Sector& sourceSector = document_.level().sectors[
                    static_cast<std::size_t>(sectorIndex)];
                if (static_cast<std::size_t>(surfaceIndex)
                        >= sourceSector.surfaces.size()
                    || sourceSector.surfaces[static_cast<std::size_t>(surfaceIndex)]
                        .vertices.size() < 2u
                    || static_cast<std::size_t>(edgeIndex)
                        >= sourceSector.surfaces[static_cast<std::size_t>(surfaceIndex)]
                            .vertices.size())
                {
                    return;
                }
                const Surface& sourceSurface = sourceSector.surfaces[
                    static_cast<std::size_t>(surfaceIndex)];
                const std::size_t edge = static_cast<std::size_t>(edgeIndex);
                const std::array<level::EdgeRef, 1> refs{
                    level::EdgeRef{ sectorIndex, surfaceIndex, edgeIndex } };
                const std::array<VertexRef, 2> movedVertices{
                    VertexRef{ sectorIndex, sourceSurface.vertices[edge] },
                    VertexRef{
                        sectorIndex,
                        sourceSurface.vertices[(edge + 1u)
                            % sourceSurface.vertices.size()] },
                };
                const bool preserveUvs =
                    glView_->preserveConnectedSurfaceUvsEnabled();
                const level::ConnectedSurfaceUvPlan uvPlan = preserveUvs
                    ? level::captureConnectedSurfaceUvPlan(
                        document_.level(), movedVertices, false)
                    : level::ConnectedSurfaceUvPlan{};
                const auto transform = [&](Level& level) -> bool
                {
                    if (hasRotation)
                    {
                        TVector normal{
                            static_cast<double>(axis.x()),
                            static_cast<double>(axis.y()),
                            static_cast<double>(axis.z()) };
                        if (normal.Normalize() != 0.0)
                        {
                            if ( !level::levelRotateEdgesChecked(
                                level, refs, normal,
                                static_cast<double>(pivot.x),
                                static_cast<double>(pivot.y),
                                static_cast<double>(pivot.z), angleDeg) )
                            {
                                return false;
                            }
                        }
                        else
                        {
                            return false;
                        }
                    }
                    if (hasTranslation)
                    {
                        if ( !level::levelTranslateEdgesChecked(
                            level, refs,
                            static_cast<double>(translation.x),
                            static_cast<double>(translation.y),
                            static_cast<double>(translation.z)) )
                        {
                            return false;
                        }
                    }
                    return true;
                };
                std::optional<Level> candidate = makeTransformCandidate(
                    document_.level(), preserveUvs, uvPlan, transform);
                if ( !candidate )
                {
                    reportGeometryTransformCandidateFailure(*glView_, preserveUvs);
                    return;
                }

                document_.pushSurfaceSubsetUndo(
                    ui::undo::action(
                        "Edge", 1u,
                        hasTranslation && hasRotation
                            ? "Moved and oriented"
                            : hasRotation ? "Oriented" : "Moved"),
                    editor::DocumentDirty::LevelGeometry,
                    undoSectors);

                document_.level() = std::move(*candidate);

                glView_->notifyLevelGeometryChanged();
                updateEditActions();
                rebuildProperties();
                scheduleRebuildChecks(editor::DocumentDirty::LevelGeometry);
            });

    connect(glView_, &GlView::surfacesRotateRequested, this, [this](const QVector3D &axis, const double angleDeg, const TVector &pivot) {
        if (!documentEditingAllowed() || !glView_)
        {
            return;
        }

        std::vector<sed::SurfaceRef> surfaces{};
        surfaces.reserve(document_.selection().surfaces.size());
        for (const sed::SurfaceRef ref : document_.selection().surfaces)
        {
            surfaces.push_back(ref);
        }

        if (surfaces.empty())
        {
            return;
        }

        if (axis.lengthSquared() < 1e-10f || !std::isfinite(angleDeg) || angleDeg == 0.0)
        {
            return;
        }

        std::vector<std::int32_t> undoSectors{};
        undoSectors.reserve(surfaces.size() * 2u);
        appendSurfaceTransformUndoSectors(document_.level(), surfaces, undoSectors);
        if (undoSectors.empty())
        {
            return;
        }

        const bool preserveUvs = glView_->preserveConnectedSurfaceUvsEnabled();
        const level::ConnectedSurfaceUvPlan uvPlan = preserveUvs
            ? captureSurfaceTransformUvs(document_.level(), surfaces, false)
            : level::ConnectedSurfaceUvPlan{};
        TVector rotationAxis{
            static_cast<double>(axis.x()),
            static_cast<double>(axis.y()),
            static_cast<double>(axis.z()) };
        if (rotationAxis.Normalize() == 0.0)
        {
            return;
        }
        const auto transform = [&](Level& level) -> bool
        {
            return level::levelRotateSurfacesChecked(
                level, surfaces, rotationAxis,
                static_cast<double>(pivot.x),
                static_cast<double>(pivot.y),
                static_cast<double>(pivot.z), angleDeg);
        };
        std::optional<Level> candidate = makeTransformCandidate(
            document_.level(), preserveUvs, uvPlan, transform);
        if ( !candidate )
        {
            reportGeometryTransformCandidateFailure(*glView_, preserveUvs);
            return;
        }

        document_.pushSurfaceSubsetUndo(
            ui::undo::action("Surface", surfaces.size(), "Rotated"),
            editor::DocumentDirty::LevelGeometry,
            undoSectors);
        document_.level() = std::move(*candidate);

        glView_->notifyLevelGeometryChanged();
        updateEditActions();
        rebuildProperties();
        scheduleRebuildChecks(editor::DocumentDirty::LevelGeometry);
    });

    connect(glView_, &GlView::placementPreviewMouseMoved, this, [this](const QPoint &widgetPos, const Qt::KeyboardModifiers modifiers) {
        if (!documentEditingAllowed())
        {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
            if (editMode_ == EditMode::frame
                && viewportFrameAddAction_
                && viewportFrameAddAction_->isChecked())
            {
                setProperty(
                    "sed.framePlacementPreviewDiagnostic",
                    QStringLiteral("hover handler rejected: document editing is unavailable"));
            }
#endif
            return;
        }
        if (editMode_ == EditMode::frame
            && viewportFrameAddAction_
            && viewportFrameAddAction_->isChecked())
        {
            updateFramePlacementPreview(widgetPos, modifiers);
            return;
        }
        if (placementMode_ == PlacementMode::thing)
        {
            updateThingPlacementPreview(widgetPos, modifiers);
            return;
        }

        updateSectorDrawPreview(widgetPos, modifiers);
    });

    connect(glView_, &GlView::placementGestureStarted, this,
            [this](const QVector3D &position, const bool haveSurfaceHit, const QPoint &widgetPos, const Qt::KeyboardModifiers modifiers) {
                if (!documentEditingAllowed())
                {
                    return;
                }
                if (placementMode_ == PlacementMode::thing && thingPlacementToolMode_ == ThingPlacementToolMode::brush)
                {
                    (void)position;
                    (void)haveSurfaceHit;
                    if (thingPlacementBrushStrokeActive_)
                    {
                        (void)finalizeThingPlacementBrushStroke();
                    }
                    thingPlacementBrushStrokeActive_ = true;
                    thingPlacementBrushStrokeHaveLast_ = false;
                    thingPlacementBrushStrokePlaced_ = false;
                    thingPlacementBrushStrokeUndoPushed_ = false;
                    thingPlacementBrushStrokeFirstThing_ = document_.level().things.size();
                    (void)stampThingPlacementBrushAt(widgetPos, modifiers, true);
                    return;
                }

                if (placementMode_ != PlacementMode::cube || sectorDrawStage_ != SectorDrawStage::none)
                {
                    return;
                }

                (void)beginSectorDrawPlacement(position, haveSurfaceHit, widgetPos, modifiers);
            });

    connect(glView_, &GlView::placementGestureDragged, this, [this](const QPoint &widgetPos, const Qt::KeyboardModifiers modifiers) {
        if (!documentEditingAllowed())
        {
            return;
        }
        if (placementMode_ == PlacementMode::thing && thingPlacementToolMode_ == ThingPlacementToolMode::brush)
        {
            bool previewRefreshedByCommit = false;
            if (thingPlacementBrushStrokeActive_)
            {
                previewRefreshedByCommit = stampThingPlacementBrushAt(widgetPos, modifiers, false);
            }
            if (!previewRefreshedByCommit)
            {
                updateThingPlacementPreview(widgetPos, modifiers);
            }
            return;
        }

        updateSectorDrawPreview(widgetPos, modifiers);
    });

    connect(glView_, &GlView::placementGestureCanceled, this, [this]()
    {
        // A brush commits each spacing stamp as it happens. Ending its live
        // transaction publishes only those existing stamps; cube placement
        // previews are discarded without creating geometry.
        if (thingPlacementBrushStrokeActive_)
        {
            (void)finalizeThingPlacementBrushStroke();
        }
        if (placementMode_ == PlacementMode::cube)
        {
            cancelSectorDrawPlacement();
        }
    });

    connect(glView_, &GlView::placementGestureReleased, this, [this](const QPoint &widgetPos, const Qt::KeyboardModifiers modifiers, const bool dragged) {
        if (!documentEditingAllowed())
        {
            return;
        }
        if (placementMode_ == PlacementMode::thing && thingPlacementToolMode_ == ThingPlacementToolMode::brush)
        {
            (void)modifiers;
            (void)dragged;
            const bool placed = finalizeThingPlacementBrushStroke();
            if (placed && !thingPlacementKeepActive_)
            {
                deselectThingPlacementTemplate();
            }
            else
            {
                updateThingPlacementPreview(widgetPos, QApplication::keyboardModifiers());
            }
            return;
        }

        if (placementMode_ != PlacementMode::cube)
        {
            return;
        }

        if (sectorDrawStage_ == SectorDrawStage::footprint)
        {
            (void)finishSectorDrawFootprint(widgetPos, modifiers, dragged);
        }
        else if (sectorDrawStage_ == SectorDrawStage::height && !dragged)
        {
            updateSectorDrawPreview(widgetPos, modifiers);
            (void)commitSectorDrawPlacement();
        }
    });

    connect(glView_, &GlView::surfaceClicked, this,
            [this](const QVector3D &position, const std::int32_t sectorIndex, const std::int32_t surfaceIndex, const QPoint &widgetPos,
                   const Qt::KeyboardModifiers modifiers) {
                const Level &level = document_.level();

                const bool haveSurfaceHit = (sectorIndex >= 0) && (surfaceIndex >= 0) && (static_cast<std::size_t>(sectorIndex) < level.sectors.size()) &&
                                            (static_cast<std::size_t>(surfaceIndex) < level.sectors[static_cast<std::size_t>(sectorIndex)].surfaces.size());
                const bool selectionToolActive = viewportEntitySelectionInputActive();

                if (haveSurfaceHit)
                {
                    haveLastSurfaceClick_ = true;
                    lastSurfaceClickSurface_ = SurfaceRef{sectorIndex, surfaceIndex};
                    lastSurfaceClickPos_ = TVector{static_cast<double>(position.x()), static_cast<double>(position.y()), static_cast<double>(position.z())};
                }
                else
                {
                    haveLastSurfaceClick_ = false;
                    lastSurfaceClickSurface_ = {};
                    lastSurfaceClickPos_ = {};
                }

                if (selectionToolActive && editMode_ == EditMode::surface &&
                    (surfaceSelectionMode_ == SurfaceSelectionMode::edge || surfaceSelectionMode_ == SurfaceSelectionMode::vertex) && haveSurfaceHit)
                {
                    rebuildProperties();
                }

                if (placementMode_ == PlacementMode::none)
                {
                    if (editMode_ == EditMode::frame)
                    {
                        // Frame mode: click empty space to add a `frame` value to the
                        // selected thing. If the click hit an existing thing, let normal
                        // picking/selection handle it.
                        const bool addFrameTool = viewportFrameAddAction_ && viewportFrameAddAction_->isChecked();
                        if (addFrameTool && lastViewportClickThing_ < 0)
                        {
                            const std::optional<FramePlacementPreview> candidate =
                                framePlacementPreviewForCommit(widgetPos, modifiers);
                            if (!candidate)
                            {
                                clearFramePlacementPreview();
                                glView_->showToast(
                                    "Frame: no valid placement target",
                                    2500,
                                    GlView::ToastPlacement::BottomCenter,
                                    GlView::ToastGroup::Error);
                                return;
                            }

                            const std::int32_t targetThing = candidate->ownerThingIndex;
                            const Thing framePose = candidate->pose;
                            std::optional<std::int32_t> newFrameValueIndex{};
                            const bool applied = document_.applyThingSubsetUndoable(
                                "Thing Frame: Added",
                                std::vector<std::int32_t>{targetThing},
                                [targetThing, framePose, &newFrameValueIndex](editor::DocumentState &state) -> bool {
                                        if (targetThing < 0 || static_cast<std::size_t>(targetThing) >= state.level.things.size())
                                        {
                                            return false;
                                        }

                                        Thing &thing = state.level.things[static_cast<std::size_t>(targetThing)];

                                        // If the path is empty (no `frame` values yet),
                                        // initialize the first point at the current thing pose so
                                        // the path starts from the thing position/orientation.
                                        // (Normally this happens when enabling the add-frame
                                        // tool, but keep this resilient in case frames were
                                        // cleared while the tool stayed active.)
                                        if (sed::level::thingCountFrames(thing) == 0 && std::isfinite(thing.x) && std::isfinite(thing.y) &&
                                            std::isfinite(thing.z) && std::isfinite(thing.pch) && std::isfinite(thing.yaw) && std::isfinite(thing.rol))
                                        {
                                            sed::TplValue init{};
                                            init.name = "frame";
                                            init.setFrame(thing.x, thing.y, thing.z, thing.pch, thing.yaw, thing.rol);
                                            thing.values.push_back(std::move(init));
                                        }

                                        sed::TplValue v{};
                                        v.name = "frame";

                                        v.setFrame(
                                            framePose.x,
                                            framePose.y,
                                            framePose.z,
                                            framePose.pch,
                                            framePose.yaw,
                                            framePose.rol);
                                        thing.values.push_back(std::move(v));

                                        // Keep `numframes` in sync. This may
                                        // insert before the first frame and
                                        // shift raw value indices.
                                        sed::level::thingSyncNumFrames(thing);
                                        for (std::size_t valueIndex = thing.values.size();
                                             valueIndex > 0; --valueIndex)
                                        {
                                            const std::size_t candidateIndex = valueIndex - 1;
                                            const TplValue& candidate =
                                                thing.values[candidateIndex];
                                            if (candidate.type == ValueType::frame
                                                && candidate.name == "frame")
                                            {
                                                newFrameValueIndex = static_cast<std::int32_t>(candidateIndex);
                                                break;
                                            }
                                        }
                                        if (newFrameValueIndex)
                                        {
                                            state.selection.frames.clear();
                                            state.selection.frames.insert(
                                                sed::level::FrameRef{
                                                    targetThing,
                                                    *newFrameValueIndex });
                                        }
                                        return true;
                                });

                            if (applied && glView_)
                            {
                                glView_->notifyThingFramesChanged(
                                    std::vector<std::int32_t>{targetThing});
                            }
                            clearFramePlacementPreview();

                            if (applied)
                            {
                                if (newFrameValueIndex)
                                {
                                    if (glView_)
                                    {
                                        glView_->setSelectedFrameValueIndex(*newFrameValueIndex);
                                        glView_->setHighlightedSelection(document_.selection());
                                    }
                                }

                                if (outlinerTabs_)
                                {
                                    outlinerTabs_->setCurrentIndex(1);
                                }
                                updateEditActions();
                                rebuildProperties();
                            }

                    }
                    return;
                }

                if (selectionToolActive && editMode_ == EditMode::surface && surfaceSelectionMode_ == SurfaceSelectionMode::vertex && haveSurfaceHit)
                    {
                        const bool removeOnly = ui::shortcuts::modifierBindingsActive(
                            modifiers, selectionRemoveModifierBindings());
                        const bool additive = !removeOnly
                            && ui::shortcuts::modifierBindingsActive(
                                modifiers, selectionAddModifierBindings());

                        auto &selection = document_.selection();
                        if (!additive && !removeOnly)
                        {
                            selection.surfaceVertices.clear();
                        }

                        const Level &lvl = document_.level();
                        std::optional<editor::SurfaceVertexRef> pickedActiveVertex{};
                        if (sectorIndex >= 0 && surfaceIndex >= 0 && static_cast<std::size_t>(sectorIndex) < lvl.sectors.size() &&
                            static_cast<std::size_t>(surfaceIndex) < lvl.sectors[static_cast<std::size_t>(sectorIndex)].surfaces.size())
                        {
                            const Sector &sector = lvl.sectors[static_cast<std::size_t>(sectorIndex)];
                            const Surface &surface = sector.surfaces[static_cast<std::size_t>(surfaceIndex)];

                            if (!surface.vertices.empty() && !sector.vertices.empty())
                            {
                                const TVector p = lastSurfaceClickPos_;
                                std::size_t bestLocal = 0;
                                double bestD2 = std::numeric_limits<double>::infinity();

                                for (std::size_t i = 0; i < surface.vertices.size(); ++i)
                                {
                                    const std::int32_t vidx = surface.vertices[i];
                                    if (vidx < 0 || static_cast<std::size_t>(vidx) >= sector.vertices.size())
                                    {
                                        continue;
                                    }

                                    const Vertex &v = sector.vertices[static_cast<std::size_t>(vidx)];
                                    const double dx = p.x - v.x;
                                    const double dy = p.y - v.y;
                                    const double dz = p.z - v.z;
                                    const double d2 = dx * dx + dy * dy + dz * dz;
                                    if (d2 < bestD2)
                                    {
                                        bestD2 = d2;
                                        bestLocal = i;
                                    }
                                }

                                if (bestLocal < surface.vertices.size())
                                {
                                    const editor::SurfaceVertexRef vref{SurfaceRef{sectorIndex, surfaceIndex}, static_cast<std::int32_t>(bestLocal)};

                                    if (removeOnly)
                                    {
                                        selection.surfaceVertices.erase(vref);
                                    }
                                    else
                                    {
                                        selection.surfaceVertices.insert(vref);
                                        pickedActiveVertex = vref;
                                    }
                                }
                            }
                        }

                        // The exact vertex set is authoritative in Vertex
                        // mode. selectionPicked runs immediately before this
                        // callback and can only publish the owning Surface;
                        // rebuild that context after the exact sub-element is
                        // known, then refresh every consumer before returning.
                        selection.surfaces.clear();
                        for (auto it = selection.surfaceVertices.begin();
                             it != selection.surfaceVertices.end();)
                        {
                            const editor::SurfaceVertexRef ref = *it;
                            const bool validSector = ref.surface.sector >= 0
                                && static_cast<std::size_t>(ref.surface.sector)
                                    < lvl.sectors.size();
                            const Sector* ownerSector = validSector
                                ? &lvl.sectors[static_cast<std::size_t>(
                                      ref.surface.sector)]
                                : nullptr;
                            const bool validSurface = ownerSector
                                && ref.surface.surface >= 0
                                && static_cast<std::size_t>(ref.surface.surface)
                                    < ownerSector->surfaces.size();
                            const Surface* ownerSurface = validSurface
                                ? &ownerSector->surfaces[static_cast<std::size_t>(
                                      ref.surface.surface)]
                                : nullptr;
                            const bool validLocalVertex = ownerSurface
                                && ref.localVertex >= 0
                                && static_cast<std::size_t>(ref.localVertex)
                                    < ownerSurface->vertices.size();
                            const std::int32_t sectorVertex = validLocalVertex
                                ? ownerSurface->vertices[static_cast<std::size_t>(
                                      ref.localVertex)]
                                : -1;
                            const bool validVertex = validLocalVertex
                                && sectorVertex >= 0
                                && static_cast<std::size_t>(sectorVertex)
                                    < ownerSector->vertices.size();
                            if (!validVertex)
                            {
                                it = selection.surfaceVertices.erase(it);
                                continue;
                            }

                            selection.surfaces.insert(ref.surface);
                            ++it;
                        }

                        SurfaceRef activeSurface{ -1, -1 };
                        if (!removeOnly)
                        {
                            const SurfaceRef clicked{ sectorIndex, surfaceIndex };
                            if (selection.surfaces.contains(clicked))
                            {
                                activeSurface = clicked;
                            }
                        }
                        if (activeSurface.sector < 0
                            && !selection.surfaces.empty())
                        {
                            activeSurface = *std::min_element(
                                selection.surfaces.begin(),
                                selection.surfaces.end(),
                                [](const SurfaceRef lhs, const SurfaceRef rhs)
                                {
                                    return lhs.sector != rhs.sector
                                        ? lhs.sector < rhs.sector
                                        : lhs.surface < rhs.surface;
                                });
                        }

                        if (glView_)
                        {
                            glView_->setSelectionState(
                                activeSurface.sector,
                                activeSurface.surface,
                                -1,
                                -1,
                                selection);
                            // selectionPicked publishes only the owner before
                            // this exact point-pick callback runs. Make the
                            // clicked vertex authoritative now so Shift+LMB
                            // promotes it to red and the previous active
                            // member becomes secondary. syncOutlinerSelectionOnly
                            // consumes this same ref and therefore cannot
                            // replace it with the unordered set's minimum.
                            if (pickedActiveVertex
                                && selection.surfaceVertices.contains(
                                    *pickedActiveVertex))
                            {
                                glView_->setActiveSurfaceVertexHint(
                                    pickedActiveVertex->surface.sector,
                                    pickedActiveVertex->surface.surface,
                                    pickedActiveVertex->localVertex);
                            }
                        }
                        syncOutlinerSelectionOnly();
                        updateEditActions();
                        rebuildProperties();
                    }

                    return;
                }

                if (placementMode_ == PlacementMode::thingLight)
                {
                    (void)attachThingLightToThing(lastViewportClickThing_);
                    return;
                }
                if (placementMode_ == PlacementMode::sectorLight)
                {
                    const std::int32_t targetSector = sectorIndex >= 0
                        ? sectorIndex
                        : (sectorLightPlacementHoverSector_ >= 0
                            ? sectorLightPlacementHoverSector_
                            : lastViewportClickSector_);
                    (void)placeSectorLightInSector(targetSector);
                    return;
                }

                if (placementMode_ == PlacementMode::cube)
                {
                    if (sectorDrawStage_ == SectorDrawStage::none)
                    {
                        (void)beginSectorDrawPlacement(position, haveSurfaceHit, widgetPos, modifiers);
                        return;
                    }

                    updateSectorDrawPreview(widgetPos, modifiers);
                    (void)commitSectorDrawPlacement();
                    return;
                }

                if (placementMode_ == PlacementMode::thing)
                {
                    const std::optional<ThingPlacementTarget> target = thingPlacementTargetForViewport(widgetPos, modifiers);
                    if (target)
                    {
                        (void)commitThingPlacementAt(target->position, target->haveSurfaceHit || target->havePlacementTarget, target->surfaceNormal, modifiers,
                                                     target->sectorFallback, &widgetPos, &target->documentPosition);
                    }
                    return;
                }

                const TVector hitPos{static_cast<double>(position.x()), static_cast<double>(position.y()), static_cast<double>(position.z())};

                std::int32_t selectedSector = -1;
                std::int32_t selectedSurface = -1;
                std::int32_t selectedThing = -1;
                std::int32_t selectedLight = -1;
                std::int32_t bakedLightPlacementLayer = 0;
                std::string undoLabel{};

                // Precompute placement parameters before mutating the level.
                constexpr double sectorShapeSize = 1.0;
                const std::string sectorShapeName = selectedSectorShapeName_.empty() ? std::string("Cube") : selectedSectorShapeName_;
                std::optional<sed::level::SectorPrimitive> sectorPrimitive{};
                std::optional<sed::level::SectorShape> legacySectorShape{};

                if (placementMode_ == PlacementMode::cube)
                {
                    sectorPrimitive = ui::sector::primitiveForName(sectorShapeName);
                    if (!sectorPrimitive)
                    {
                        legacySectorShape = ui::sector::legacyShapeByName(sectorShapeName);
                    }

                    if (!sectorPrimitive && !legacySectorShape)
                    {
                        if (glView_)
                        {
                            glView_->showToast(QString("Can't find sector shape: %1").arg(ui::sector::shapeLabel(sectorShapeName)), 2600,
                                               GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
                        }
                        return;
                    }

                    undoLabel = "Sector: Added";
                }
                else if (placementMode_ == PlacementMode::light)
                {
                    std::int32_t targetSector = sectorIndex;
                    if (targetSector < 0
                        || static_cast<std::size_t>(targetSector)
                            >= level.sectors.size())
                    {
                        targetSector = glView_
                            ? glView_->findContainingSector(
                                  hitPos, lastViewportClickSector_)
                            : sed::level::levelFindSectorForXYZ(
                                  level,
                                  hitPos.x,
                                  hitPos.y,
                                  hitPos.z);
                    }

                    const std::int32_t targetLayer = targetSector >= 0
                            && static_cast<std::size_t>(targetSector)
                                < level.sectors.size()
                        ? level.sectors[static_cast<std::size_t>(targetSector)].layer
                        : -1;
                    bakedLightPlacementLayer = ui::layers::resolvePlacementLayer(
                        level.layers.size(),
                        -1,
                        targetLayer,
                        layerVisible_);
                    undoLabel = "Baked Light: Added";
                }

                if (undoLabel.empty())
                {
                    return;
                }
                if ( placementMode_ == PlacementMode::light
                    && !document_.level().canAllocateIds(1) )
                {
                    constexpr const char* message =
                        "Cannot add light: entity ID space exhausted";
                    if ( glView_ )
                    {
                        glView_->showToast(
                            message,
                            3000,
                            GlView::ToastPlacement::BottomCenter,
                            GlView::ToastGroup::Error);
                    }
                    if ( statusBar() )
                    {
                        statusBar()->showMessage(message, 4000);
                    }
                    return;
                }

                editor::DocumentDirty dirty = editor::DocumentDirty::Structure;
                if (placementMode_ == PlacementMode::cube)
                {
                    dirty |= editor::DocumentDirty::LevelGeometry;
                }
                else if (placementMode_ == PlacementMode::light)
                {
                    dirty |= editor::DocumentDirty::Lights;
                }

                const bool applied = document_.applyUndoable(undoLabel, dirty, [&](editor::DocumentState &state) -> bool {
                    editor::Selection newSelection{};

                    if (placementMode_ == PlacementMode::cube)
                    {
                        std::int32_t newSectorIndex = -1;
                        if (sectorPrimitive)
                        {
                            newSectorIndex = sed::level::levelCreateSectorPrimitive(state.level, *sectorPrimitive, hitPos, sectorShapeSize);
                        }
                        else if (legacySectorShape)
                        {
                            newSectorIndex = createSectorFromTaggedShape(
                                state.level,
                                *legacySectorShape,
                                hitPos,
                                sectorShapeSize);
                        }

                        if (newSectorIndex < 0)
                        {
                            return false;
                        }

                        selectedSector = newSectorIndex;
                        newSelection.sectors.insert(selectedSector);
                    }
                    else if (placementMode_ == PlacementMode::light)
                    {
                        SedLight &light = state.level.newLight();
                        light.position = hitPos;
                        light.layer = bakedLightPlacementLayer;

                        selectedLight = static_cast<std::int32_t>(state.level.lights.size() - 1);
                        newSelection.lights.insert(selectedLight);
                    }
                    else
                    {
                        return false;
                    }

                    state.selection = std::move(newSelection);
                    return true;
                });

                if (!applied)
                {
                    return;
                }

                if (editor::hasAny(dirty, editor::DocumentDirty::Structure)
                    && editor::hasAny(dirty, editor::DocumentDirty::LevelGeometry))
                {
                    invalidateSurfaceSelectionContextForTopologyEdit();
                }

                if (glView_)
                {
                    // Adding a light should not require a full level mesh rebuild: lights
                    // are rendered as gizmo sprites.
                    if (placementMode_ == PlacementMode::cube)
                    {
                        glView_->notifyLevelGeometryChanged();
                    }
                    else if (placementMode_ == PlacementMode::thing)
                    {
                        glView_->notifyThingsChanged();
                    }
                    else if (placementMode_ == PlacementMode::light)
                    {
                        glView_->notifyLightsChanged();
                    }
                    glView_->setSelectionState(selectedSector, selectedSurface, selectedThing, selectedLight, document_.selection());
                }

                bool outlinerUpdated = false;
                if (outliner_)
                {
                    const QSignalBlocker blocker(outliner_);
                    outliner_->clearSelection();
                    outliner_->setCurrentItem(nullptr);
                    outlinerUpdated = true;

                    const Level &levelNow = document_.level();
                    const int tab = outlinerTabs_ ? outlinerTabs_->currentIndex() : 0;

                    auto addItem = [&](const QString &text, const OutlinerKind kind, const int index) -> QTreeWidgetItem * {
                        auto *item = new QTreeWidgetItem(outliner_);
                        item->setText(0, text);
                        item->setData(0, kOutlinerKindRole, static_cast<int>(kind));
                        item->setData(0, kOutlinerIndexRole, index);
                        return item;
                    };

                    auto selectCurrent = [&](QTreeWidgetItem *item) {
                        if (!item)
                        {
                            return;
                        }
                        item->setSelected(true);
                        outliner_->setCurrentItem(item);
                    };

                    if (placementMode_ == PlacementMode::cube && tab == 0 && selectedSector >= 0)
                    {
                        if (outliner_->topLevelItemCount() + 1 == static_cast<int>(levelNow.sectors.size()) &&
                            static_cast<std::size_t>(selectedSector) < levelNow.sectors.size())
                        {
                            const Sector &sector = levelNow.sectors[static_cast<std::size_t>(selectedSector)];
                            QTreeWidgetItem *item =
                                addItem(QString("%1: Sector (id %2, surfaces %3)").arg(selectedSector).arg(sector.id).arg(sector.surfaces.size()),
                                        OutlinerKind::sector, selectedSector);
                            selectCurrent(item);
                        }
                        else
                        {
                            outlinerUpdated = false;
                        }

                        if (outlinerCountLabel_)
                        {
                            outlinerCountLabel_->setText(QString("Sectors: %1").arg(levelNow.sectors.size()));
                        }
                        refreshOutlinerTabCaptions();
                    }
                    else if (placementMode_ == PlacementMode::thing && tab == 1 && selectedThing >= 0)
                    {
                        if (outliner_->topLevelItemCount() + 1 == static_cast<int>(levelNow.things.size()) &&
                            static_cast<std::size_t>(selectedThing) < levelNow.things.size())
                        {
                            const Thing &thing = levelNow.things[static_cast<std::size_t>(selectedThing)];
                            QTreeWidgetItem *item =
                                addItem(QString("%1: %2").arg(selectedThing).arg(QString::fromStdString(thing.name)), OutlinerKind::thing, selectedThing);
                            selectCurrent(item);
                        }
                        else
                        {
                            outlinerUpdated = false;
                        }

                        if (outlinerCountLabel_)
                        {
                            outlinerCountLabel_->setText(QString("Things: %1").arg(levelNow.things.size()));
                        }
                        refreshOutlinerTabCaptions();
                    }
                    else if (placementMode_ == PlacementMode::light && tab == 3 && selectedLight >= 0)
                    {
                        appendLightsToOutliner(
                            static_cast<std::size_t>(selectedLight));
                    }
                }

                if (!outlinerUpdated)
                {
                    rebuildOutliner();
                }
                rebuildProperties();
                updateEditActions();

                setPlacementMode(PlacementMode::none);
            });
}

} // namespace sed
