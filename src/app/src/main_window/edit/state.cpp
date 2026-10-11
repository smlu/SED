#include "main_window/common.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"
#include "render/gizmos/light_category_gizmo_utils.hpp"
#include "thing/thing_light_authoring_utils.hpp"
#include "ui/sector/sector_shape_utils.hpp"
#include "ui/shortcuts/shortcut_context_utils.hpp"

namespace sed
{

namespace
{

void setExclusiveActionGroupSelection(QActionGroup* group, QAction* target)
{
    if (!group)
    {
        return;
    }

    bool alreadySynchronized = group->checkedAction() == target;
    for (QAction* action : group->actions())
    {
        if (!action)
        {
            continue;
        }
        const bool shouldBeChecked = action == target;
        if (action->isChecked() != shouldBeChecked)
        {
            alreadySynchronized = false;
        }
    }
    if (alreadySynchronized)
    {
        return;
    }

    // Programmatic restores used to block toggled signals here. That kept the
    // visible booleans in sync but left QActionGroup::checkedAction stale.
    // Reconcile through normal toggles so shared overlay buttons and menus see
    // the same exclusive selection as the group itself.
    if (QAction* stale = group->checkedAction(); stale && !stale->isChecked())
    {
        stale->setChecked(true);
    }
    for (QAction* action : group->actions())
    {
        if (action && action->isChecked())
        {
            action->setChecked(false);
        }
    }
    if (target)
    {
        target->setChecked(true);
    }
}

} // namespace

void MainWindow::setupToolbars()
{
    editModeGroup_ = new QActionGroup(this);
    editModeGroup_->setExclusive(true);

    editSectorAction_ = new QAction("Sector", editModeGroup_);
    editSectorAction_->setCheckable(true);
    connect(editSectorAction_, &QAction::triggered, this, [this]()
        {
            toggleEditMode(EditMode::sector);
        });

    editSurfaceAction_ = new QAction("Surface", editModeGroup_);
    editSurfaceAction_->setCheckable(true);
    connect(editSurfaceAction_, &QAction::triggered, this, [this]()
        {
            toggleEditMode(EditMode::surface);
        });

    editThingAction_ = new QAction("Thing", editModeGroup_);
    editThingAction_->setCheckable(true);
    connect(editThingAction_, &QAction::triggered, this, [this]()
        {
            toggleEditMode(EditMode::thing);
        });

    editLightAction_ = new QAction("Light", editModeGroup_);
    editLightAction_->setObjectName("viewport.edit_mode.light");
    editLightAction_->setCheckable(true);
    connect(editLightAction_, &QAction::triggered, this, [this]()
        {
            toggleEditMode(EditMode::light);
        });

    editFrameAction_ = new QAction("Frame", editModeGroup_);
    editFrameAction_->setCheckable(true);
    connect(editFrameAction_, &QAction::triggered, this, [this]()
        {
            // Clicking "Frame" again should toggle back to Thing mode.
            if (editMode_ == EditMode::frame)
            {
                setEditMode(EditMode::thing);
            }
            else
            {
                setEditMode(EditMode::frame);
            }
        });

    surfaceSelectionGroup_ = new QActionGroup(this);
    surfaceSelectionGroup_->setExclusive(true);

    surfaceFaceAction_ = new QAction("Face", surfaceSelectionGroup_);
    surfaceFaceAction_->setCheckable(true);
    connect(surfaceFaceAction_, &QAction::triggered, this, [this]()
        {
            setSurfaceSelectionMode(SurfaceSelectionMode::face);
        });

    surfaceEdgeAction_ = new QAction("Edge", surfaceSelectionGroup_);
    surfaceEdgeAction_->setCheckable(true);
    connect(surfaceEdgeAction_, &QAction::triggered, this, [this]()
        {
            setSurfaceSelectionMode(SurfaceSelectionMode::edge);
        });

    surfaceVertexAction_ = new QAction("Vertex", surfaceSelectionGroup_);
    surfaceVertexAction_->setCheckable(true);
    connect(surfaceVertexAction_, &QAction::triggered, this, [this]()
        {
            setSurfaceSelectionMode(SurfaceSelectionMode::vertex);
        });

    const QSettings surfaceSettings = settingsStore();
    surfaceStrictSelectionAction_ = new QAction("Strict", this);
    surfaceStrictSelectionAction_->setObjectName("viewport.surface.strict_scope");
    surfaceStrictSelectionAction_->setToolTip(
        "<b>Strict selection</b><br>Limit Edge and Vertex picking to faces selected before switching to Edge or Vertex mode.");
    surfaceStrictSelectionAction_->setCheckable(true);
    surfaceStrictSelectionAction_->setChecked(
        surfaceSettings.value("viewport/surface_strict_selection", false).toBool());
    connect(surfaceStrictSelectionAction_, &QAction::toggled, this,
        [this](const bool checked)
        {
            if (checked)
            {
                strictSurfaceSelectionScope_ = document_.selection().surfaces;
            }
            else
            {
                strictSurfaceSelectionScope_.clear();
            }
            if (glView_)
            {
                glView_->setStrictSurfaceSelectionScope(
                    checked, strictSurfaceSelectionScope_);
            }
            QSettings settings = settingsStore();
            settings.setValue("viewport/surface_strict_selection", checked);
        });

    surfaceNormalsAction_ = new QAction("Normals", this);
    surfaceNormalsAction_->setObjectName("viewport.surface.show_normals");
    surfaceNormalsAction_->setToolTip(
        "<b>Surface normals</b><br>Show the direction of selected face normals in Face, Edge, and Vertex modes.");
    surfaceNormalsAction_->setCheckable(true);
    surfaceNormalsAction_->setChecked(
        surfaceSettings.value("viewport/show_selected_surface_normals", false).toBool());
    if (glView_)
    {
        glView_->setSurfaceNormalLengthScale(static_cast<float>(surfaceSettings.value(
            "viewport/surface_normal_length_scale", 1.0).toDouble()));
        glView_->setSurfaceNormalArrowheadScale(static_cast<float>(surfaceSettings.value(
            "viewport/surface_normal_arrowhead_scale", 1.0).toDouble()));
        glView_->setShowSurfaceNormalArrowhead(surfaceSettings.value(
            "viewport/surface_normal_show_arrowhead", true).toBool());
    }
    connect(surfaceNormalsAction_, &QAction::toggled, this,
        [this](const bool checked)
        {
            if (glView_)
            {
                glView_->setShowSelectedSurfaceNormals(checked);
            }
            QSettings settings = settingsStore();
            settings.setValue("viewport/show_selected_surface_normals", checked);
        });

    for ( QAction* const action : { surfaceStrictSelectionAction_, surfaceNormalsAction_ } )
    {
        action->setProperty("sed_keymap_expose", true);
        action->setShortcutContext(Qt::WidgetShortcut);
        action->setAutoRepeat(false);
        if ( glView_ )
        {
            glView_->addAction(action);
        }
    }

    lightEditSubModeGroup_ = new QActionGroup(this);
    lightEditSubModeGroup_->setExclusive(true);

    lightEditBakedAction_ = new QAction(ui::makeViewportIcon(ui::ViewportIcon::lightBaked), "Baked", lightEditSubModeGroup_);
    lightEditBakedAction_->setObjectName("viewport.light_mode.baked");
    lightEditBakedAction_->setToolTip(
        "<b>Baked lights</b><br>Edit lights whose effect is baked into level surfaces.");
    lightEditBakedAction_->setCheckable(true);
    connect(lightEditBakedAction_, &QAction::triggered, this, [this]()
        {
            setLightEditSubMode(LightEditSubMode::baked);
        });

    lightEditThingAction_ = new QAction(ui::makeViewportIcon(ui::ViewportIcon::lightThing), "Thing", lightEditSubModeGroup_);
    lightEditThingAction_->setObjectName("viewport.light_mode.thing");
    lightEditThingAction_->setToolTip(
        "<b>Thing lights</b><br>Edit lights attached to Things, including actor headlights. Changes update without recalculating baked lighting.");
    lightEditThingAction_->setCheckable(true);
    connect(lightEditThingAction_, &QAction::triggered, this, [this]()
        {
            setLightEditSubMode(LightEditSubMode::thing);
        });

    lightEditSectorAction_ = new QAction(ui::makeViewportIcon(ui::ViewportIcon::lightSector), "Sector", lightEditSubModeGroup_);
    lightEditSectorAction_->setObjectName("viewport.light_mode.sector");
    lightEditSectorAction_->setToolTip(
        "<b>Sector lights</b><br>Edit IJIM point lights attached to Sectors. Changes update without recalculating baked lighting.");
    lightEditSectorAction_->setCheckable(true);
    connect(lightEditSectorAction_, &QAction::triggered, this, [this]()
        {
            setLightEditSubMode(LightEditSubMode::sector);
        });

    lightEditArchitecturalAction_ = new QAction(
        ui::makeViewportIcon(ui::ViewportIcon::lightArchitectural),
        "Arch",
        lightEditSubModeGroup_);
    lightEditArchitecturalAction_->setObjectName(
        "viewport.light_mode.architectural");
    lightEditArchitecturalAction_->setToolTip(
        "<b>Arch lighting</b><br>Edit light colors on vertices of a MotS Arch Thing's model. This does not create a light source.");
    lightEditArchitecturalAction_->setVisible(
        document_.level().kind == ProjectType::MOTS);
    lightEditArchitecturalAction_->setCheckable(true);
    connect(lightEditArchitecturalAction_, &QAction::triggered, this, [this]()
        {
            setLightEditSubMode(LightEditSubMode::architectural);
        });

    addSectorLightAction_ = new QAction(
        ui::makeViewportIcon(ui::ViewportIcon::lightSector),
        "Add Sector light",
        this);
    addSectorLightAction_->setObjectName("viewport.light_mode.add_sector_light");
    addSectorLightAction_->setToolTip(
        "<b>Add Sector light</b><br>Click an IJIM Sector to place its point light at the Sector center.");
    addSectorLightAction_->setCheckable(true);
    connect(addSectorLightAction_, &QAction::toggled, this, [this](const bool checked)
        {
            if (checked)
            {
                setPlacementMode(PlacementMode::sectorLight);
            }
            else if (placementMode_ == PlacementMode::sectorLight)
            {
                setPlacementMode(PlacementMode::none);
            }
        });

    addThingLightAction_ = new QAction(
        ui::makeViewportIcon(ui::ViewportIcon::actionAddLight),
        "Add Thing light",
        this);
    addThingLightAction_->setObjectName("viewport.light_mode.add_thing_light");
    addThingLightAction_->setToolTip(
        "<b>Add Thing light</b><br>Click a placed Thing to attach a light. Its lighting updates without recalculating baked lighting.");
    addThingLightAction_->setCheckable(true);
    connect(addThingLightAction_, &QAction::toggled, this, [this](const bool checked)
        {
            if (checked)
            {
                setPlacementMode(PlacementMode::thingLight);
            }
            else if (placementMode_ == PlacementMode::thingLight)
            {
                setPlacementMode(PlacementMode::none);
            }
        });

    addThingLightMenu_ = new ui::shortcuts::KeySequenceDisplayMenu(this);
    addThingLightMenu_->setTitle("Thing light type");
    addThingLightMenu_->setToolTipsVisible(true);
    auto* thingLightKindGroup = new QActionGroup(addThingLightMenu_);
    thingLightKindGroup->setExclusive(true);

    addThingRegularLightAction_ = addThingLightMenu_->addAction(
        ui::makeViewportIcon(ui::ViewportIcon::lightThing),
        "Thing light");
    addThingRegularLightAction_->setObjectName("viewport.light_mode.add_thing_light.regular");
    addThingRegularLightAction_->setToolTip(
        "<b>Thing light</b><br>Attach a light to a placed Thing. Its lighting updates without recalculating baked lighting.");
    addThingRegularLightAction_->setCheckable(true);
    addThingRegularLightAction_->setChecked(true);
    thingLightKindGroup->addAction(addThingRegularLightAction_);
    connect(addThingRegularLightAction_, &QAction::triggered, this, [this]()
        {
            setThingLightPlacementKind(ThingLightPlacementKind::regular);
            if (addThingLightAction_ && addThingLightAction_->isEnabled())
            {
                addThingLightAction_->setChecked(true);
            }
        });

    addThingHeadlightAction_ = addThingLightMenu_->addAction(
        ui::makeViewportIcon(ui::ViewportIcon::lightHeadlight),
        "Actor headlight");
    addThingHeadlightAction_->setObjectName("viewport.light_mode.add_thing_light.headlight");
    addThingHeadlightAction_->setToolTip(
        "<b>Actor headlight</b><br>Attach a light that points in an actor or player's facing direction.");
    addThingHeadlightAction_->setCheckable(true);
    thingLightKindGroup->addAction(addThingHeadlightAction_);
    connect(addThingHeadlightAction_, &QAction::triggered, this, [this]()
        {
            setThingLightPlacementKind(ThingLightPlacementKind::headlight);
            if (addThingLightAction_ && addThingLightAction_->isEnabled())
            {
                addThingLightAction_->setChecked(true);
            }
        });

    {
        const QSettings settings = settingsStore();
        alwaysShowLightGizmoCategories_[static_cast<std::size_t>(render::LightGizmoCategory::Baked)] =
            settings.value("viewport/light_gizmos_always_baked", false).toBool();
        alwaysShowLightGizmoCategories_[static_cast<std::size_t>(render::LightGizmoCategory::Thing)] =
            settings.value("viewport/light_gizmos_always_thing", false).toBool();
        alwaysShowLightGizmoCategories_[static_cast<std::size_t>(render::LightGizmoCategory::Sector)] =
            settings.value("viewport/light_gizmos_always_sector", false).toBool();
        alwaysShowLightGizmoCategories_[static_cast<std::size_t>(render::LightGizmoCategory::Architectural)] =
            settings.value("viewport/light_gizmos_always_architectural", true).toBool();
    }

    auto makeAlwaysShowLightAction = [this](
        QAction*& action,
        const render::LightGizmoCategory category,
        const QString& text,
        const QString& objectName,
        const QString& editorSettingKey,
        const QString& previewSettingKey,
        const QString& toolTip)
    {
        action = new QAction(text, this);
        action->setObjectName(objectName);
        action->setToolTip(toolTip);
        action->setCheckable(true);
        action->setProperty("sed.editorSettingKey", editorSettingKey);
        action->setProperty("sed.previewSettingKey", previewSettingKey);
        action->setChecked(alwaysShowLightGizmoCategories_[static_cast<std::size_t>(category)]);
        connect(action, &QAction::toggled, this,
            [this, category, editorSettingKey, previewSettingKey](const bool checked)
            {
                const bool preview = glView_
                    && glView_->renderPipeline() == GlView::RenderPipeline::GamePreview;
                if (!preview)
                {
                    alwaysShowLightGizmoCategories_[static_cast<std::size_t>(category)] = checked;
                }
                QSettings settings = settingsStore();
                settings.setValue(preview ? previewSettingKey : editorSettingKey, checked);
                if (glView_)
                {
                    glView_->setAlwaysShowLightGizmoCategory(category, checked);
                }
            });
    };
    makeAlwaysShowLightAction(
        alwaysShowBakedLightGizmosAction_,
        render::LightGizmoCategory::Baked,
        "Baked lights",
        "viewport.light_gizmos.always_baked",
        "viewport/light_gizmos_always_baked",
        "viewport/preview_light_gizmos_always_baked",
        "<b>Baked light markers</b><br>Keep baked-light markers visible when another light type is active. "
        "Editor and Game Preview settings are saved separately.");
    makeAlwaysShowLightAction(
        alwaysShowThingLightGizmosAction_,
        render::LightGizmoCategory::Thing,
        "Thing lights",
        "viewport.light_gizmos.always_thing",
        "viewport/light_gizmos_always_thing",
        "viewport/preview_light_gizmos_always_thing",
        "<b>Thing light markers</b><br>Keep Thing-light and headlight markers visible when another light type is active. "
        "Editor and Game Preview settings are saved separately.");
    makeAlwaysShowLightAction(
        alwaysShowSectorLightGizmosAction_,
        render::LightGizmoCategory::Sector,
        "Sector lights",
        "viewport.light_gizmos.always_sector",
        "viewport/light_gizmos_always_sector",
        "viewport/preview_light_gizmos_always_sector",
        "<b>Sector light markers</b><br>Keep Sector light markers visible when another light type is active. "
        "Editor and Game Preview settings are saved separately.");
    makeAlwaysShowLightAction(
        alwaysShowArchitecturalLightGizmosAction_,
        render::LightGizmoCategory::Architectural,
        "Architectural lights",
        "viewport.light_gizmos.always_architectural",
        "viewport/light_gizmos_always_architectural",
        "viewport/preview_light_gizmos_always_architectural",
        "<b>Arch Thing markers</b><br>Keep MotS Arch Thing markers visible when another light type is active. "
        "Editor and Game Preview settings are saved separately.");

    if (editMode_ == EditMode::none)
    {
        // Leave all unchecked; `setEditMode()` will sync state.
    }
    else if (editMode_ == EditMode::sector)
    {
        editSectorAction_->setChecked(true);
    }
    else if (editMode_ == EditMode::surface)
    {
        editSurfaceAction_->setChecked(true);
    }
    else if (editMode_ == EditMode::thing)
    {
        editThingAction_->setChecked(true);
    }
    else if (editMode_ == EditMode::light)
    {
        editLightAction_->setChecked(true);
    }
    else
    {
        editFrameAction_->setChecked(true);
    }

    if (surfaceSelectionMode_ == SurfaceSelectionMode::face)
    {
        surfaceFaceAction_->setChecked(true);
    }
    else if (surfaceSelectionMode_ == SurfaceSelectionMode::edge)
    {
        surfaceEdgeAction_->setChecked(true);
    }
    else
    {
        surfaceVertexAction_->setChecked(true);
    }

    lightEditBakedAction_->setChecked(true);
    lightEditArchitecturalAction_->setChecked(false);
    if (glView_)
    {
        for (std::size_t i = 0; i < alwaysShowLightGizmoCategories_.size(); ++i)
        {
            glView_->setAlwaysShowLightGizmoCategory(
                static_cast<render::LightGizmoCategory>(i),
                alwaysShowLightGizmoCategories_[i]);
        }
    }

    setEditMode(editMode_);
}

void MainWindow::toggleEditMode(const EditMode mode)
{
    if (mode == EditMode::none)
    {
        setEditMode(EditMode::none);
        return;
    }

    if (editMode_ == mode)
    {
        setEditMode(EditMode::none);
        return;
    }

    setEditMode(mode);
}

void MainWindow::setEditMode(const EditMode mode)
{
    if (!documentEditingAllowed()
        && !recoveryEditorModeRestoreInProgress_
        && mode != editMode_)
    {
        updateEditActions();
        return;
    }
    if (mode == EditMode::light
        && ((lightEditSubMode_ == LightEditSubMode::sector
                && document_.level().kind != ProjectType::IJIM)
            || (lightEditSubMode_ == LightEditSubMode::architectural
                && document_.level().kind != ProjectType::MOTS)))
    {
        setLightEditSubMode(LightEditSubMode::baked);
    }

    const EditMode prevMode = editMode_;
    if (prevMode == EditMode::light && mode != EditMode::light
        && lightEditSubMode_ == LightEditSubMode::architectural)
    {
        cancelArchitecturalLightingInteractiveEdit();
    }
    editMode_ = mode;

    // If a placement tool is active but becomes unavailable due to mode changes, cancel it so users
    // can always deactivate tools by either clicking again or simply switching modes.
    const bool placementAllowed =
        placementMode_ == PlacementMode::none
        || (placementMode_ == PlacementMode::cube && editMode_ == EditMode::sector)
        || (placementMode_ == PlacementMode::thing && editMode_ == EditMode::thing)
        || (placementMode_ == PlacementMode::light
            && editMode_ == EditMode::light
            && lightEditSubMode_ == LightEditSubMode::baked)
        || (placementMode_ == PlacementMode::thingLight
            && editMode_ == EditMode::light
            && lightEditSubMode_ == LightEditSubMode::thing)
        || (placementMode_ == PlacementMode::sectorLight
            && editMode_ == EditMode::light
            && lightEditSubMode_ == LightEditSubMode::sector
            && document_.level().kind == ProjectType::IJIM);
    if (!placementAllowed)
    {
        setPlacementMode(PlacementMode::none);
    }

    // Keep the action group, overlays, and menus synchronized, including the
    // valid no-selection state where no edit-mode action is checked.
    QAction* const activeEditAction = mode == EditMode::sector ? editSectorAction_
        : mode == EditMode::surface ? editSurfaceAction_
        : mode == EditMode::thing ? editThingAction_
        : mode == EditMode::light ? editLightAction_
        : mode == EditMode::frame ? editFrameAction_
        : nullptr;
    setExclusiveActionGroupSelection(editModeGroup_, activeEditAction);

    if (glView_)
    {
        GlView::PickMode pickMode = GlView::PickMode::Auto;
        if (mode == EditMode::sector)
        {
            pickMode = GlView::PickMode::Sector;
        }
        else if (mode == EditMode::surface)
        {
            pickMode = GlView::PickMode::Surface;
        }
        else if (mode == EditMode::thing || mode == EditMode::frame)
        {
            pickMode = GlView::PickMode::Thing;
        }
        else if (mode == EditMode::light)
        {
            pickMode = static_cast<GlView::PickMode>(lightEditPickModeValue());
        }
        glView_->setPickMode(pickMode);
        const bool architecturalLightingActive =
            mode == EditMode::light
            && lightEditSubMode_ == LightEditSubMode::architectural
            && glView_->renderPipeline() == GlView::RenderPipeline::Editor;
        if (!architecturalLightingActive
            && glView_->architecturalLightingEditingActive()
            && glView_->renderPipeline() == GlView::RenderPipeline::Editor)
        {
            glView_->setArchitecturalLightingAuthoringMode(
                level::MotsArchLightingVertexMode::Rgb);
        }
        glView_->setArchitecturalLightingEditingActive(
            architecturalLightingActive);
        glView_->setLightGizmoEditingActive(
            mode == EditMode::light && !architecturalLightingActive);
        glView_->setLightGizmoCategory(
            mode == EditMode::light
                ? (lightEditSubMode_ == LightEditSubMode::thing
                    ? render::LightGizmoCategory::Thing
                    : lightEditSubMode_ == LightEditSubMode::architectural
                        ? render::LightGizmoCategory::Architectural
                    : (lightEditSubMode_ == LightEditSubMode::sector
                        ? render::LightGizmoCategory::Sector
                        : render::LightGizmoCategory::Baked))
                : render::LightGizmoCategory::Baked);
        glView_->setFrameEditEnabled(mode == EditMode::frame);
        glView_->setViewportPickingEnabled(
            glView_->renderPipeline() == GlView::RenderPipeline::GamePreview
            || mode != EditMode::none
            || placementMode_ != PlacementMode::none);
    }

    const bool enableSurfaceSubMode = (mode == EditMode::surface);
    if (surfaceFaceAction_)
    {
        surfaceFaceAction_->setEnabled(enableSurfaceSubMode);
    }
    if (surfaceEdgeAction_)
    {
        surfaceEdgeAction_->setEnabled(enableSurfaceSubMode);
    }
    if (surfaceVertexAction_)
    {
        surfaceVertexAction_->setEnabled(enableSurfaceSubMode);
    }
    if (surfaceStrictSelectionAction_)
    {
        surfaceStrictSelectionAction_->setEnabled(
            enableSurfaceSubMode && surfaceSelectionMode_ != SurfaceSelectionMode::face);
    }
    if (surfaceNormalsAction_)
    {
        surfaceNormalsAction_->setEnabled(enableSurfaceSubMode);
    }

    const bool inPreview = glView_ && (glView_->renderPipeline() == GlView::RenderPipeline::GamePreview);

    // Update viewport edit-mode overlay.
    if (viewportEditSectorButton_)
    {
        viewportEditSectorButton_->setChecked(mode == EditMode::sector);
    }
    if (viewportEditSurfaceButton_)
    {
        viewportEditSurfaceButton_->setChecked(mode == EditMode::surface);
    }
    if (viewportEditThingButton_)
    {
        viewportEditThingButton_->setChecked(mode == EditMode::thing || mode == EditMode::frame);
    }
    if (viewportEditLightButton_)
    {
        viewportEditLightButton_->setChecked(mode == EditMode::light);
    }
    if (viewportEditModeSurfacePanel_)
    {
        viewportEditModeSurfacePanel_->setVisible(mode == EditMode::surface);
    }
    if (viewportEditModeThingPanel_)
    {
        viewportEditModeThingPanel_->setVisible(mode == EditMode::thing || mode == EditMode::frame);
    }
    if (viewportEditModeLightPanel_)
    {
        viewportEditModeLightPanel_->setVisible(mode == EditMode::light);
    }
    const bool ijimLightModes = document_.level().kind == ProjectType::IJIM;
    if (viewportEditLightSectorButton_)
    {
        viewportEditLightSectorButton_->setVisible(ijimLightModes);
    }
    if (viewportEditLightArchitecturalButton_)
    {
        viewportEditLightArchitecturalButton_->setVisible(
            document_.level().kind == ProjectType::MOTS);
    }
    if (lightEditSectorAction_)
    {
        lightEditSectorAction_->setEnabled(ijimLightModes);
    }
    if (viewportEditModeOverlay_)
    {
        const bool showOverlay = !inPreview;
        viewportEditModeOverlay_->setVisible(showOverlay);
        viewportEditModeOverlay_->setEnabled(true);
        syncViewportSelectionEditabilityIndicator();
        viewportEditModeOverlay_->adjustSize();
        if (showOverlay)
        {
            positionViewportEditModeOverlay();
        }
    }

    // Light-family transforms expose position and authored range. A regular
    // Thing light moves with its owner while a Headlight Move edits its local
    // offset; none of the emitter families uses Rotate.
    if (viewportTransformOverlay_)
    {
        const bool showOverlay = (mode != EditMode::none)
            && !inPreview;
        viewportTransformOverlay_->setVisible(showOverlay);
        viewportTransformOverlay_->setEnabled(true);

        if (!showOverlay)
        {
            if (glView_)
            {
                glView_->setTransformTool(GlView::TransformTool::Select);
                glView_->setTransformInteractionEnabled(false);
            }
            // Preview hides this toolbar by default but deliberately keeps
            // point selection available. Retain a disabled, checked
            // Select sentinel for that input path; a genuine EditMode::none
            // reset still clears every transform owner.
            setViewportTransformActionChecks(
                inPreview && mode != EditMode::none
                    ? viewportTransformSelectAction_
                    : nullptr);
            // Game Preview is a temporary presentation pipeline. Preserve
            // the user's editor transform choice while its input is
            // suspended, just as GlView preserves the editor camera state.
            // A genuine EditMode::none reset still clears it.
            if (!inPreview)
            {
                rememberedViewportTransformAction_ = nullptr;
            }
            syncViewportSelectionInputEnabled();
        }
        else if (prevMode == EditMode::none
            && mode != EditMode::none
            && !viewportModificationToolActive()
            && (!viewportTransformSelectAction_ || !viewportTransformSelectAction_->isChecked())
            && (!viewportTransformMoveAction_ || !viewportTransformMoveAction_->isChecked())
            && (!viewportTransformRotateAction_ || !viewportTransformRotateAction_->isChecked())
            && (!viewportTransformScaleAction_ || !viewportTransformScaleAction_->isChecked()))
        {
            // Entering an edit mode from the neutral document state gives the
            // user a real selection owner.  This does not override an
            // explicit Select-off choice while switching between edit modes;
            // it only arms Select for the first mode activation after a normal
            // level/New Project reset.
            setViewportTransformActionChecks(viewportTransformSelectAction_);
            rememberedViewportTransformAction_ = viewportTransformSelectAction_;
            if (glView_)
            {
                glView_->setTransformTool(GlView::TransformTool::Select);
                glView_->setTransformInteractionEnabled(
                    !selectionTouchesNonEditableLayer(document_.selection()));
            }
            syncViewportSelectionInputEnabled();
        }

        const bool archSelectionOnly = mode == EditMode::light
            && lightEditSubMode_ == LightEditSubMode::architectural;
        if (archSelectionOnly && viewportTransformSelectAction_
            && documentEditingAllowed())
        {
            const bool selectOwnsViewport =
                viewportTransformSelectAction_->isChecked()
                && (!glView_ || glView_->transformTool() == GlView::TransformTool::Select)
                && !viewportModificationToolActive();
            if (!selectOwnsViewport)
            {
                if (!viewportTransformSelectAction_->isChecked())
                {
                    // Trigger the normal Select path so it also clears any
                    // suspended transform/edit tool and enables viewport picks.
                    viewportTransformSelectAction_->trigger();
                }
                else
                {
                    deactivateViewportEditFunctions();
                    setViewportTransformActionChecks(viewportTransformSelectAction_);
                    rememberedViewportTransformAction_ = viewportTransformSelectAction_;
                    if (glView_)
                    {
                        glView_->setTransformTool(GlView::TransformTool::Select);
                        glView_->setTransformInteractionEnabled(
                            !selectionTouchesNonEditableLayer(document_.selection()));
                    }
                    syncViewportSelectionInputEnabled();
                }
            }
        }

        // LIGHT_OWNER_TRANSFORM_POLICY: all light families expose Move and
        // source-correct range Scale, while Rotate is intentionally absent.
        const bool showRotate = mode != EditMode::light;
        const bool showScale = mode == EditMode::sector
            || mode == EditMode::surface
            || mode == EditMode::light;
        if (viewportTransformMoveAction_)
        {
            if (mode == EditMode::light && lightEditSubMode_ == LightEditSubMode::thing)
            {
                viewportTransformMoveAction_->setToolTip(
                    "<b>Move</b><br>Move a Thing light with its Thing, or move a Headlight relative to its actor.");
            }
            else if (mode == EditMode::light && lightEditSubMode_ == LightEditSubMode::sector)
            {
                viewportTransformMoveAction_->setToolTip(
                    "<b>Move</b><br>Move the selected Sector light.");
            }
            else
            {
                viewportTransformMoveAction_->setToolTip(
                    "<b>Move</b><br>Move the selected items.");
            }
        }
        if (viewportTransformRotateAction_)
        {
            viewportTransformRotateAction_->setToolTip(
                "<b>Rotate</b><br>Rotate the selected items.");
        }
        if (viewportTransformScaleAction_)
        {
            if (mode == EditMode::light && lightEditSubMode_ == LightEditSubMode::baked)
            {
                viewportTransformScaleAction_->setToolTip(
                    "<b>Scale</b><br>Change how far the selected Baked light reaches across surfaces.");
            }
            else if (mode == EditMode::light && lightEditSubMode_ == LightEditSubMode::sector)
            {
                viewportTransformScaleAction_->setToolTip(
                    "<b>Scale</b><br>Adjust the selected Sector light's falloff distances.");
            }
            else if (mode == EditMode::light && lightEditSubMode_ == LightEditSubMode::thing)
            {
                viewportTransformScaleAction_->setToolTip(
                    document_.level().kind == ProjectType::IJIM
                        ? "<b>Scale</b><br>Change the selected Thing light or Headlight's range."
                        : "<b>Scale</b><br>Change the Thing light or Headlight's brightness and resulting lit area.");
            }
            else
            {
                viewportTransformScaleAction_->setToolTip(
                    "<b>Scale</b><br>Scale the selected items.");
            }
        }
        const auto buttons = viewportTransformOverlay_->findChildren<QToolButton*>();
        for (QToolButton* b : buttons)
        {
            if (!b)
            {
                continue;
            }

            QAction* const action = b->defaultAction();
            if (archSelectionOnly)
            {
                // ARCH vertex authoring uses viewport selection. Transform,
                // orientation, and grid controls obscure the selection owner
                // and suggest operations that do not apply to ARCH vertices.
                b->setVisible(action == viewportTransformSelectAction_);
            }
            else if (action == viewportTransformRotateAction_)
            {
                b->setVisible(showRotate);
            }
            else if (action == viewportTransformScaleAction_)
            {
                b->setVisible(showScale);
            }
            else
            {
                // Restore Select, Move, orientation, and any other normal
                // overlay controls after leaving Arch-light mode.
                b->setVisible(true);
            }
        }

        viewportTransformOverlay_->adjustSize();
        positionViewportTransformOverlay();

        const bool hideActiveTool =
            (!showRotate && viewportTransformRotateAction_ && viewportTransformRotateAction_->isChecked())
            || (!showScale && viewportTransformScaleAction_ && viewportTransformScaleAction_->isChecked());

        if (hideActiveTool)
        {
            if (viewportTransformMoveAction_)
            {
                viewportTransformMoveAction_->trigger();
            }
            else if (viewportTransformSelectAction_)
            {
                viewportTransformSelectAction_->trigger();
            }
        }
    }

    syncViewportContextualOverlays();
    syncThingPlacementPanel();

    if (prevMode == mode)
    {
        // Same-mode calls are also the lightweight state-refresh seam used by
        // viewport pipeline and preference changes. Keep action availability
        // (notably editor-only Add/Import Sector) synchronized without
        // rebuilding selection or changing the active tool.
        updateEditActions();
        return;
    }

    // Save outgoing selection so switching modes can restore the last selection for each mode.
    auto saveSelectionForMode = [&](const EditMode m)
    {
        const auto& sel = document_.selection();
        switch (m)
        {
            case EditMode::sector:
            {
                lastSectorSelection_.clear();
                lastSectorSelection_.sectors = sel.sectors;
                break;
            }
            case EditMode::surface:
            {
                lastSurfaceSelection_.clear();
                lastSurfaceSelection_.sectors = sel.sectors;
                lastSurfaceSelection_.surfaces = sel.surfaces;
                break;
            }
            case EditMode::thing:
            case EditMode::frame:
            {
                lastThingSelection_.clear();
                lastThingSelection_.things = sel.things;
                break;
            }
            case EditMode::light:
            {
                if (lightEditSubMode_ == LightEditSubMode::thing
                    || lightEditSubMode_ == LightEditSubMode::architectural)
                {
                    lastLightThingSelection_.clear();
                    lastLightThingSelection_.things = sel.things;
                }
                else if (lightEditSubMode_ == LightEditSubMode::sector)
                {
                    lastLightSectorSelection_.clear();
                    lastLightSectorSelection_.sectors = sel.sectors;
                }
                else
                {
                    lastLightSelection_.clear();
                    lastLightSelection_.lights = sel.lights;
                }
                break;
            }
            case EditMode::none:
            default:
                break;
        }
    };

    const auto outgoingSurfaces = document_.selection().surfaces;
    const auto outgoingSectors = document_.selection().sectors;
    saveSelectionForMode(prevMode);

    editor::Selection restored{};
    if (mode == EditMode::sector)
    {
        // Surface -> Sector: select owning sector(s) of the current surface selection.
        if (prevMode == EditMode::surface && !outgoingSurfaces.empty())
        {
            for (const SurfaceRef& ref : outgoingSurfaces)
            {
                if (ref.sector >= 0)
                {
                    restored.sectors.insert(ref.sector);
                }
            }
        }
        else if (prevMode == EditMode::surface && !outgoingSectors.empty())
        {
            restored.sectors = outgoingSectors;
        }
        else
        {
            restored.sectors = lastSectorSelection_.sectors;
        }
    }
    else if (mode == EditMode::surface)
    {
        if (prevMode == EditMode::sector && !outgoingSectors.empty())
        {
            // Surface -> Sector maps the selected faces to their owners.  If
            // the user immediately returns to Surface mode, preserve those
            // exact faces.  Once the Sector selection differs, it is an
            // explicit replacement and every face of the newly selected
            // Sector(s) becomes the Surface selection.
            std::unordered_set<std::int32_t> priorSurfaceOwners{};
            for (const SurfaceRef& ref : lastSurfaceSelection_.surfaces)
            {
                if (ref.sector >= 0)
                {
                    priorSurfaceOwners.insert(ref.sector);
                }
            }

            if (!lastSurfaceSelection_.surfaces.empty()
                && priorSurfaceOwners == outgoingSectors)
            {
                restored.sectors = lastSurfaceSelection_.sectors;
                restored.surfaces = lastSurfaceSelection_.surfaces;
            }
            else
            {
                const Level& level = document_.level();
                for (const std::int32_t sectorIndex : outgoingSectors)
                {
                    if (sectorIndex < 0
                        || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
                    {
                        continue;
                    }
                    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
                    for (std::size_t surfaceIndex = 0;
                         surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
                    {
                        restored.surfaces.insert(SurfaceRef{
                            sectorIndex, static_cast<std::int32_t>(surfaceIndex) });
                    }
                }
            }
        }
        else
        {
            restored.sectors = lastSurfaceSelection_.sectors;
            restored.surfaces = lastSurfaceSelection_.surfaces;
        }
    }
    else if (mode == EditMode::thing || mode == EditMode::frame)
    {
        restored.things = lastThingSelection_.things;
    }
    else if (mode == EditMode::light)
    {
        if (lightEditSubMode_ == LightEditSubMode::thing
            || lightEditSubMode_ == LightEditSubMode::architectural)
        {
            restored.things = lastLightThingSelection_.things;
        }
        else if (lightEditSubMode_ == LightEditSubMode::sector)
        {
            restored.sectors = lastLightSectorSelection_.sectors;
        }
        else
        {
            restored.lights = lastLightSelection_.lights;
        }
    }

    document_.selection() = std::move(restored);

    if (outlinerTabs_)
    {
        const int targetOutlinerTab =
            (mode == EditMode::thing || mode == EditMode::frame) ? 1
            : (mode == EditMode::light) ? lightEditOutlinerTab()
            : 0;
        if (outlinerTabs_->currentIndex() != targetOutlinerTab)
        {
            const QSignalBlocker blocker(outlinerTabs_);
            outlinerTabs_->setCurrentIndex(targetOutlinerTab);
        }
    }

    if (glView_)
    {
        const auto& selection = document_.selection();

        auto minIndex = [](const auto& set) -> std::int32_t
        {
            if (set.empty())
            {
                return -1;
            }
            return *std::min_element(set.begin(), set.end());
        };

        auto minSurface = [&]() -> SurfaceRef
        {
            if (selection.surfaces.empty())
            {
                return SurfaceRef{ -1, -1 };
            }

            SurfaceRef best = *selection.surfaces.begin();
            for (const SurfaceRef& ref : selection.surfaces)
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

        if (mode == EditMode::thing || mode == EditMode::frame)
        {
            selThing = minIndex(selection.things);
        }
        else if (mode == EditMode::light)
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
        else if (mode == EditMode::surface)
        {
            if (!selection.surfaces.empty())
            {
                const SurfaceRef ref = minSurface();
                selSector = ref.sector;
                selSurface = ref.surface;
            }
            else
            {
                selSector = minIndex(selection.sectors);
            }
        }
        else if (mode == EditMode::sector)
        {
            selSector = minIndex(selection.sectors);
        }

        glView_->setSelectionState(selSector, selSurface, selThing, selLight, selection);
    }

    const auto outlinerGroupForMode = [this](const EditMode editMode) -> int
    {
        switch (editMode)
        {
            case EditMode::sector:
            case EditMode::surface:
                return 0;
            case EditMode::thing:
            case EditMode::frame:
                return 1;
            case EditMode::light:
                return lightEditOutlinerTab();
            default:
                return -1;
        }
    };

    if (outliner_ && outlinerTabs_
        && outlinerTabs_->currentIndex() == outlinerGroupForMode(mode)
        && outlinerGroupForMode(prevMode) == outlinerGroupForMode(mode))
    {
        syncOutlinerSelectionOnly();
    }
    else
    {
        rebuildOutliner();
    }
    updateEditActions();
    rebuildProperties();
}

void MainWindow::setSurfaceSelectionMode(const SurfaceSelectionMode mode)
{
    if (!documentEditingAllowed()
        && !recoveryEditorModeRestoreInProgress_
        && mode != surfaceSelectionMode_)
    {
        return;
    }
    const SurfaceSelectionMode previousMode = surfaceSelectionMode_;

    // Face-only modification tools must not remain armed after the pointer
    // changes to an edge or vertex. Use the normal QAction and preview
    // cancellation paths so the viewport tool owner and any suspended
    // transform state are unwound together.
    if ( previousMode != mode && mode != SurfaceSelectionMode::face )
    {
        if ( viewportSurfaceCleaveAction_ && viewportSurfaceCleaveAction_->isChecked() )
        {
            viewportSurfaceCleaveAction_->setChecked(false);
        }
        if ( surfaceExtrudePreviewActive_ )
        {
            cancelSurfaceExtrudePreview();
        }
    }

    bool restoredFaceSelection = false;
    if (previousMode == SurfaceSelectionMode::face
        && mode != SurfaceSelectionMode::face)
    {
        rememberedSurfaceFaceSelection_ = document_.selection().surfaces;
    }
    else if (previousMode != SurfaceSelectionMode::face
        && mode == SurfaceSelectionMode::face)
    {
        const Level& level = document_.level();
        const auto validSurface = [&level](const SurfaceRef ref) noexcept
        {
            return ref.sector >= 0 && ref.surface >= 0
                && static_cast<std::size_t>(ref.sector) < level.sectors.size()
                && static_cast<std::size_t>(ref.surface)
                    < level.sectors[static_cast<std::size_t>(ref.sector)].surfaces.size();
        };

        std::unordered_set<SurfaceRef, editor::SurfaceRefHash, editor::SurfaceRefEq>
            faces{};
        for (const SurfaceRef ref : rememberedSurfaceFaceSelection_)
        {
            if (validSurface(ref))
            {
                faces.insert(ref);
            }
        }
        // If Vertex mode was entered without a prior face selection, its
        // selected sub-elements still provide an unambiguous face context.
        if (faces.empty())
        {
            for (const editor::SurfaceVertexRef ref :
                document_.selection().surfaceVertices)
            {
                if (validSurface(ref.surface))
                {
                    faces.insert(ref.surface);
                }
            }
        }
        document_.selection().surfaces = std::move(faces);
        document_.selection().surfaceVertices.clear();
        rememberedSurfaceFaceSelection_ = document_.selection().surfaces;
        restoredFaceSelection = true;
    }
    if (surfaceStrictSelectionAction_
        && surfaceStrictSelectionAction_->isChecked()
        && previousMode == SurfaceSelectionMode::face
        && mode != SurfaceSelectionMode::face)
    {
        strictSurfaceSelectionScope_ = document_.selection().surfaces;
    }
    surfaceSelectionMode_ = mode;

    QAction* const activeSurfaceModeAction = mode == SurfaceSelectionMode::face
        ? surfaceFaceAction_
        : mode == SurfaceSelectionMode::edge ? surfaceEdgeAction_
        : surfaceVertexAction_;
    setExclusiveActionGroupSelection(surfaceSelectionGroup_, activeSurfaceModeAction);
    updateSurfaceTooltips();

    if (!glView_)
    {
        return;
    }

    GlView::SurfaceSubMode viewMode = GlView::SurfaceSubMode::Face;
    switch (mode)
    {
        case SurfaceSelectionMode::face:
        {
            viewMode = GlView::SurfaceSubMode::Face;
            break;
        }
        case SurfaceSelectionMode::edge:
        {
            viewMode = GlView::SurfaceSubMode::Edge;
            break;
        }
        case SurfaceSelectionMode::vertex:
        {
            viewMode = GlView::SurfaceSubMode::Vertex;
            break;
        }
    }

    glView_->setSurfaceSubMode(viewMode);
    glView_->setStrictSurfaceSelectionScope(
        surfaceStrictSelectionAction_ && surfaceStrictSelectionAction_->isChecked(),
        strictSurfaceSelectionScope_);
    glView_->setShowSelectedSurfaceNormals(
        surfaceNormalsAction_ && surfaceNormalsAction_->isChecked());

    // The viewport tool overlay depends on the surface sub-mode (face/edge/vertex). Refresh the
    // current edit-mode layout even when the edit mode itself hasn't changed.
    setEditMode(editMode_);
    if (restoredFaceSelection && glView_)
    {
        SurfaceRef primary{ -1, -1 };
        for (const SurfaceRef ref : document_.selection().surfaces)
        {
            if (primary.sector < 0 || ref.sector < primary.sector
                || (ref.sector == primary.sector
                    && ref.surface < primary.surface))
            {
                primary = ref;
            }
        }
        glView_->setSelectionState(
            primary.sector, primary.surface, -1, -1,
            document_.selection());
        syncOutlinerSelectionOnly();
    }
    syncCleaveOptionsOverlay();
    updateEditActions();
    rebuildProperties();
}

int MainWindow::lightEditOutlinerTab() const noexcept
{
    switch (lightEditSubMode_)
    {
        case LightEditSubMode::thing:  return 1;
        case LightEditSubMode::architectural: return 1;
        case LightEditSubMode::sector: return 0;
        case LightEditSubMode::baked:
        default:                       return 3;
    }
}

int MainWindow::lightEditPickModeValue() const noexcept
{
    switch (lightEditSubMode_)
    {
        case LightEditSubMode::thing:  return static_cast<int>(GlView::PickMode::Thing);
        case LightEditSubMode::sector: return static_cast<int>(GlView::PickMode::Sector);
        case LightEditSubMode::architectural:
            return static_cast<int>(GlView::PickMode::ArchitecturalLighting);
        case LightEditSubMode::baked:
        default:                       return static_cast<int>(GlView::PickMode::Light);
    }
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
void MainWindow::activateArchitecturalLightingEditModeForTest()
{
    setLightEditSubMode(LightEditSubMode::architectural);
    setEditMode(EditMode::light);
}

bool MainWindow::architecturalLightingEditModeActiveForTest() const noexcept
{
    return editMode_ == EditMode::light
        && lightEditSubMode_ == LightEditSubMode::architectural;
}
#endif

void MainWindow::setLightEditSubMode(LightEditSubMode mode)
{
    if (mode == LightEditSubMode::sector
        && document_.level().kind != ProjectType::IJIM)
    {
        mode = LightEditSubMode::baked;
    }
    if (mode == LightEditSubMode::architectural
        && document_.level().kind != ProjectType::MOTS)
    {
        mode = LightEditSubMode::baked;
    }
    if (!documentEditingAllowed()
        && !recoveryEditorModeRestoreInProgress_
        && mode != lightEditSubMode_)
    {
        return;
    }

    const LightEditSubMode previous = lightEditSubMode_;
    const bool changing = previous != mode;
    if (changing && previous == LightEditSubMode::architectural)
    {
        cancelArchitecturalLightingInteractiveEdit();
    }
    if (changing && editMode_ == EditMode::light)
    {
        const editor::Selection& selection = document_.selection();
        if (previous == LightEditSubMode::thing
            || previous == LightEditSubMode::architectural)
        {
            lastLightThingSelection_.clear();
            lastLightThingSelection_.things = selection.things;
        }
        else if (previous == LightEditSubMode::sector)
        {
            lastLightSectorSelection_.clear();
            lastLightSectorSelection_.sectors = selection.sectors;
        }
        else
        {
            lastLightSelection_.clear();
            lastLightSelection_.lights = selection.lights;
        }
    }

    lightEditSubMode_ = mode;

    QAction* const activeLightModeAction = mode == LightEditSubMode::baked
        ? lightEditBakedAction_
        : mode == LightEditSubMode::thing ? lightEditThingAction_
        : mode == LightEditSubMode::sector ? lightEditSectorAction_
        : lightEditArchitecturalAction_;
    setExclusiveActionGroupSelection(lightEditSubModeGroup_, activeLightModeAction);

    const bool sectorAvailable = document_.level().kind == ProjectType::IJIM;
    if (lightEditSectorAction_)
    {
        lightEditSectorAction_->setEnabled(sectorAvailable);
    }
    if (lightEditArchitecturalAction_)
    {
        const bool architecturalAvailable =
            document_.level().kind == ProjectType::MOTS;
        lightEditArchitecturalAction_->setVisible(architecturalAvailable);
        lightEditArchitecturalAction_->setEnabled(architecturalAvailable);
    }
    if (viewportEditLightSectorButton_)
    {
        viewportEditLightSectorButton_->setVisible(sectorAvailable);
    }
    if (viewportEditLightArchitecturalButton_)
    {
        viewportEditLightArchitecturalButton_->setVisible(
            document_.level().kind == ProjectType::MOTS);
    }

    if (glView_)
    {
        const render::LightGizmoCategory category =
            mode == LightEditSubMode::thing
                ? render::LightGizmoCategory::Thing
                : mode == LightEditSubMode::architectural
                    ? render::LightGizmoCategory::Architectural
                : (mode == LightEditSubMode::sector
                    ? render::LightGizmoCategory::Sector
                    : render::LightGizmoCategory::Baked);
        glView_->setLightGizmoCategory(
            editMode_ == EditMode::light ? category : render::LightGizmoCategory::Baked);
        const bool architecturalActive =
            editMode_ == EditMode::light
            && mode == LightEditSubMode::architectural
            && glView_->renderPipeline() == GlView::RenderPipeline::Editor;
        if (!architecturalActive
            && glView_->architecturalLightingEditingActive()
            && glView_->renderPipeline() == GlView::RenderPipeline::Editor)
        {
            glView_->setArchitecturalLightingAuthoringMode(
                level::MotsArchLightingVertexMode::Rgb);
        }
        glView_->setArchitecturalLightingEditingActive(architecturalActive);
        if (mode == LightEditSubMode::architectural)
        {
            // ARCH selection is always vertex-first.  The renderer still
            // keeps its internal target enum for the pick implementation,
            // but it is no longer exposed as a second editor mode.
            glView_->setArchitecturalLightingTargetMode(
                GlView::ArchitecturalLightingTargetMode::Vertices);
        }
        glView_->setLightGizmoEditingActive(
            editMode_ == EditMode::light && !architecturalActive);
        if (editMode_ == EditMode::light)
        {
            glView_->setPickMode(static_cast<GlView::PickMode>(lightEditPickModeValue()));
        }
    }

    if (!changing || editMode_ != EditMode::light)
    {
        syncLightBakeOverlay();
        if (viewportEditModeOverlay_)
        {
            viewportEditModeOverlay_->adjustSize();
            positionViewportEditModeOverlay();
        }
        return;
    }

    if ((placementMode_ == PlacementMode::light && mode != LightEditSubMode::baked)
        || (placementMode_ == PlacementMode::thingLight && mode != LightEditSubMode::thing)
        || (placementMode_ == PlacementMode::sectorLight && mode != LightEditSubMode::sector))
    {
        setPlacementMode(PlacementMode::none);
    }

    editor::Selection restored{};
    if (mode == LightEditSubMode::thing
        || mode == LightEditSubMode::architectural)
    {
        restored.things = lastLightThingSelection_.things;
    }
    else if (mode == LightEditSubMode::sector)
    {
        restored.sectors = lastLightSectorSelection_.sectors;
    }
    else
    {
        restored.lights = lastLightSelection_.lights;
    }
    document_.selection() = std::move(restored);

    auto minIndex = [](const auto& values) -> std::int32_t
    {
        return values.empty() ? -1 : *std::min_element(values.begin(), values.end());
    };
    if (glView_)
    {
        glView_->setSelectionState(
            mode == LightEditSubMode::sector ? minIndex(document_.selection().sectors) : -1,
            -1,
            (mode == LightEditSubMode::thing
                || mode == LightEditSubMode::architectural)
                ? minIndex(document_.selection().things)
                : -1,
            mode == LightEditSubMode::baked ? minIndex(document_.selection().lights) : -1,
            document_.selection());
    }

    if (outlinerTabs_ && outlinerTabs_->currentIndex() != lightEditOutlinerTab())
    {
        const QSignalBlocker blocker(outlinerTabs_);
        outlinerTabs_->setCurrentIndex(lightEditOutlinerTab());
    }
    rebuildOutliner();
    updateEditActions();
    rebuildProperties();
    syncLightBakeOverlay();
    syncViewportToolsOverlay();
    setEditMode(editMode_); // Refresh transform availability for this family.
}

void MainWindow::setThingLightPlacementKind(const ThingLightPlacementKind kind)
{
    thingLightPlacementKind_ = kind;
    QAction* const activeThingLightKindAction =
        kind == ThingLightPlacementKind::headlight
            ? addThingHeadlightAction_ : addThingRegularLightAction_;
    setExclusiveActionGroupSelection(
        addThingRegularLightAction_ ? addThingRegularLightAction_->actionGroup() : nullptr,
        activeThingLightKindAction);
    if (addThingLightAction_)
    {
        addThingLightAction_->setIcon(ui::makeViewportIcon(
            kind == ThingLightPlacementKind::headlight
                ? ui::ViewportIcon::lightHeadlight
                : ui::ViewportIcon::lightThing));
        addThingLightAction_->setText(
            kind == ThingLightPlacementKind::headlight
                ? QStringLiteral("Add actor headlight")
                : QStringLiteral("Add Thing light"));
        addThingLightAction_->setToolTip(
            kind == ThingLightPlacementKind::headlight
                ? QStringLiteral(
                      "<b>Actor headlight</b><br>Click an actor or player Thing to attach a light that points in its facing direction.")
                : QStringLiteral(
                      "<b>Thing light</b><br>Click a placed Thing to attach a light. Its lighting updates without recalculating baked lighting."));
    }
    if (viewportCommonAddThingLightButton_)
    {
        viewportCommonAddThingLightButton_->setToolTip(
            kind == ThingLightPlacementKind::headlight
                ? QStringLiteral(
                      "<b>Actor headlight</b><br>Click an actor or player Thing to attach a light that points in its facing direction.")
                : QStringLiteral(
                      "<b>Thing light</b><br>Click a placed Thing to attach a light. Its lighting updates without recalculating baked lighting."));
    }
    if (placementMode_ == PlacementMode::thingLight)
    {
        updateThingLightPlacementTarget(thingLightPlacementHoverThing_);
    }
}

bool MainWindow::thingLightPlacementTargetValid(const std::int32_t thingIndex) const
{
    const Level& level = document_.level();
    if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= level.things.size())
    {
        return false;
    }
    const Thing& thing = level.things[static_cast<std::size_t>(thingIndex)];
    if (isLayerLocked(thing.layer))
    {
        return false;
    }
    const assets::Templates* templatesDb = templates_.size() > 0 ? &templates_ : nullptr;
    return thing::light_authoring::eligibility(
        level,
        templatesDb,
        thing,
        thingLightPlacementKind_ == ThingLightPlacementKind::headlight
            ? thing::light_authoring::Kind::headlight
            : thing::light_authoring::Kind::regular)
        == thing::light_authoring::AttachmentEligibility::allowed;
}

void MainWindow::updateThingLightPlacementTarget(const std::int32_t thingIndex)
{
    thingLightPlacementHoverThing_ = thingIndex;
    if (!glView_ || placementMode_ != PlacementMode::thingLight)
    {
        return;
    }

    const bool valid = thingLightPlacementTargetValid(thingIndex);
    const ui::ViewportIcon cursorIcon =
        thingLightPlacementKind_ == ThingLightPlacementKind::headlight
            ? ui::ViewportIcon::lightHeadlight
            : ui::ViewportIcon::lightThing;
    QPixmap cursorPixmap = ui::makeViewportIcon(cursorIcon).pixmap(QSize(34, 34));
    if (cursorPixmap.isNull())
    {
        cursorPixmap = QPixmap(34, 34);
        cursorPixmap.fill(Qt::transparent);
    }
    QPainter painter(&cursorPixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(valid ? QColor(86, 226, 128) : QColor(248, 76, 76), 3.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawEllipse(QRectF(2.5, 2.5, 29.0, 29.0));
    if (!valid)
    {
        painter.drawLine(QPointF(6.0, 6.0), QPointF(28.0, 28.0));
        painter.drawLine(QPointF(28.0, 6.0), QPointF(6.0, 28.0));
    }
    painter.end();
    glView_->setCursor(QCursor(cursorPixmap, 17, 17));

    if (valid)
    {
        editor::Selection hovered{};
        hovered.things.insert(thingIndex);
        glView_->setHighlightedSelection(hovered);
    }
    else
    {
        glView_->setHighlightedSelection(document_.selection());
    }
}

bool MainWindow::attachThingLightToThing(const std::int32_t thingIndex)
{
    const thing::light_authoring::Kind authoredKind =
        thingLightPlacementKind_ == ThingLightPlacementKind::headlight
            ? thing::light_authoring::Kind::headlight
            : thing::light_authoring::Kind::regular;
    const assets::Templates* templatesDb = templates_.size() > 0 ? &templates_ : nullptr;

    if (!thingLightPlacementTargetValid(thingIndex))
    {
        if (glView_)
        {
            QString errorMessage = authoredKind == thing::light_authoring::Kind::headlight
                ? QStringLiteral("Choose a placed actor/player without a headlight")
                : QStringLiteral("Choose a placed Thing without a light parameter");

            const Level& level = document_.level();
            if (thingIndex >= 0 && static_cast<std::size_t>(thingIndex) < level.things.size())
            {
                const Thing& target = level.things[static_cast<std::size_t>(thingIndex)];
                if (isLayerLocked(target.layer))
                {
                    errorMessage = QStringLiteral("The target Thing is on a locked layer.");
                }
                else
                {
                    switch (thing::light_authoring::eligibility(level, templatesDb, target, authoredKind))
                    {
                    case thing::light_authoring::AttachmentEligibility::regularAlreadyPresent:
                        errorMessage = QStringLiteral("This Thing already has a regular dynamic light.");
                        break;
                    case thing::light_authoring::AttachmentEligibility::headlightAlreadyPresent:
                        errorMessage = QStringLiteral("This actor/player already has a Headlight.");
                        break;
                    case thing::light_authoring::AttachmentEligibility::headlightRequiresActor:
                        errorMessage = QStringLiteral("Headlights can only be added to actors or players.");
                        break;
                    case thing::light_authoring::AttachmentEligibility::allowed:
                    case thing::light_authoring::AttachmentEligibility::invalidPlacement:
                        break;
                    }
                }
            }
            glView_->showToast(
                errorMessage,
                2400,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
        }
        return false;
    }

    const bool applied = document_.applyUndoable(
        authoredKind == thing::light_authoring::Kind::headlight
            ? "Thing: Headlight added"
            : "Thing: Light added",
        editor::DocumentDirty::Things,
        [thingIndex, authoredKind, templatesDb](editor::DocumentState& state) -> bool
        {
            if (!thing::light_authoring::attachDefault(state.level, templatesDb, thingIndex, authoredKind))
            {
                return false;
            }
            state.selection.clear();
            state.selection.things.insert(thingIndex);
            return true;
        });
    if (!applied)
    {
        return false;
    }

    lastLightThingSelection_.things = document_.selection().things;
    if (glView_)
    {
        glView_->notifyThingsChanged();
        glView_->setSelectionState(-1, -1, thingIndex, -1, document_.selection());
    }
    rebuildOutliner();
    updateEditActions();
    rebuildProperties();
    scheduleRebuildChecks(editor::DocumentDirty::Things);
    setPlacementMode(PlacementMode::none);
    return true;
}

bool MainWindow::sectorLightPlacementTargetValid(const std::int32_t sectorIndex) const
{
    const Level& level = document_.level();
    if (level.kind != ProjectType::IJIM
        || sectorIndex < 0
        || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
    {
        return false;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    return !isLayerLocked(sector.layer)
        && !pointLightHasAuthoredParameters(sector.pointLight);
}

void MainWindow::updateSectorLightPlacementTarget(const std::int32_t sectorIndex)
{
    sectorLightPlacementHoverSector_ = sectorIndex;
    if (!glView_ || placementMode_ != PlacementMode::sectorLight)
    {
        return;
    }

    const bool valid = sectorLightPlacementTargetValid(sectorIndex);
    QPixmap cursorPixmap = ui::makeViewportIcon(ui::ViewportIcon::lightSector).pixmap(QSize(34, 34));
    if (cursorPixmap.isNull())
    {
        cursorPixmap = QPixmap(34, 34);
        cursorPixmap.fill(Qt::transparent);
    }
    QPainter painter(&cursorPixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(
        valid ? QColor(86, 226, 128) : QColor(248, 76, 76),
        3.0,
        Qt::SolidLine,
        Qt::RoundCap));
    painter.drawEllipse(QRectF(2.5, 2.5, 29.0, 29.0));
    if (!valid)
    {
        painter.drawLine(QPointF(6.0, 6.0), QPointF(28.0, 28.0));
        painter.drawLine(QPointF(28.0, 6.0), QPointF(6.0, 28.0));
    }
    painter.end();
    glView_->setCursor(QCursor(cursorPixmap, 17, 17));

    if (sectorIndex >= 0
        && static_cast<std::size_t>(sectorIndex) < document_.level().sectors.size())
    {
        editor::Selection hovered{};
        hovered.sectors.insert(sectorIndex);
        glView_->setHighlightedSelection(hovered);
    }
    else
    {
        glView_->setHighlightedSelection(document_.selection());
    }
}

bool MainWindow::placeSectorLightInSector(const std::int32_t sectorIndex)
{
    if (!sectorLightPlacementTargetValid(sectorIndex))
    {
        if (glView_)
        {
            const Level& level = document_.level();
            const bool duplicate = level.kind == ProjectType::IJIM
                && sectorIndex >= 0
                && static_cast<std::size_t>(sectorIndex) < level.sectors.size()
                && pointLightHasAuthoredParameters(
                    level.sectors[static_cast<std::size_t>(sectorIndex)].pointLight);
            glView_->showToast(
                duplicate
                    ? QStringLiteral("Sector already has an active point light")
                    : QStringLiteral("Choose an unlocked sector without a point light"),
                2600,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
        }
        return false;
    }

    const std::optional<PointLight> defaultLight = level::sectorMakeDefaultIjimPointLight(
        document_.level().sectors[static_cast<std::size_t>(sectorIndex)]);
    if (!defaultLight)
    {
        if (glView_)
        {
            glView_->showToast(
                QStringLiteral("Cannot calculate a point-light range for this sector"),
                2600,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
        }
        return false;
    }

    const bool applied = document_.applyUndoable(
        "Sector: Point light added",
        editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Things,
        [sectorIndex, defaultLight = *defaultLight](editor::DocumentState& state) -> bool
        {
            if (state.level.kind != ProjectType::IJIM
                || sectorIndex < 0
                || static_cast<std::size_t>(sectorIndex) >= state.level.sectors.size())
            {
                return false;
            }
            Sector& sector = state.level.sectors[static_cast<std::size_t>(sectorIndex)];
            if (pointLightHasAuthoredParameters(sector.pointLight))
            {
                return false;
            }

            sector.pointLight = defaultLight;
            state.selection.clear();
            state.selection.sectors.insert(sectorIndex);
            return true;
        });
    if (!applied)
    {
        return false;
    }

    lastLightSectorSelection_.clear();
    lastLightSectorSelection_.sectors = document_.selection().sectors;
    if (glView_)
    {
        glView_->notifyLevelGeometryChanged();
        glView_->notifyThingsChanged();
        glView_->setSelectionState(sectorIndex, -1, -1, -1, document_.selection());
    }
    rebuildOutliner();
    updateEditActions();
    rebuildProperties();
    scheduleRebuildChecks(editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Things);
    setPlacementMode(PlacementMode::none);
    return true;
}

void MainWindow::setPlacementMode(const PlacementMode mode)
{
    if (mode != PlacementMode::none && !documentEditingAllowed())
    {
        return;
    }

    // The Add Light placement tool creates baked SedLight entities. Runtime
    // Thing and Sector categories use their own owner-targeted placement paths.
    if (mode == PlacementMode::light
        && (editMode_ != EditMode::light || lightEditSubMode_ != LightEditSubMode::baked))
    {
        return;
    }
    if (mode == PlacementMode::thingLight
        && (editMode_ != EditMode::light || lightEditSubMode_ != LightEditSubMode::thing))
    {
        return;
    }
    if (mode == PlacementMode::sectorLight
        && (editMode_ != EditMode::light
            || lightEditSubMode_ != LightEditSubMode::sector
            || document_.level().kind != ProjectType::IJIM))
    {
        return;
    }
    const PlacementMode previousMode = placementMode_;

    if (mode != PlacementMode::none)
    {
        deactivateViewportEditFunctions();
        deactivateViewportTransformForExclusiveTool();
    }
    if (previousMode == PlacementMode::thing
        && mode != PlacementMode::thing
        && thingPlacementBrushStrokeActive_)
    {
        (void)finalizeThingPlacementBrushStroke();
    }
    if (mode != placementMode_ || mode != PlacementMode::cube)
    {
        cancelSectorDrawPlacement();
    }

    placementMode_ = mode;

    // Placement owns pointer interaction. Select remains GlView's internal
    // neutral sentinel while active; leaving placement restores the transform
    // tool that was active before placement began.
    if (mode == PlacementMode::none && previousMode != PlacementMode::none)
    {
        restoreViewportTransformAfterExclusiveTool();
    }

    if (addCubeAction_)
    {
        addCubeAction_->setChecked(mode == PlacementMode::cube);
    }
    if (addThingAction_)
    {
        const QSignalBlocker blocker(addThingAction_);
        addThingAction_->setChecked(thingPlacementPanelVisible_);
    }
    if (addLightAction_)
    {
        addLightAction_->setChecked(mode == PlacementMode::light);
    }
    if (addThingLightAction_)
    {
        const QSignalBlocker blocker(addThingLightAction_);
        addThingLightAction_->setChecked(mode == PlacementMode::thingLight);
    }
    if (addSectorLightAction_)
    {
        const QSignalBlocker blocker(addSectorLightAction_);
        addSectorLightAction_->setChecked(mode == PlacementMode::sectorLight);
    }
        if (glView_)
        {
            // Placement clicks must be reserved for placement; disable any click-based edit tools.
            if (mode != PlacementMode::none)
            {
                if (viewportSectorCleaveAction_)
                {
                    const QSignalBlocker b(viewportSectorCleaveAction_);
                    viewportSectorCleaveAction_->setChecked(false);
                }
                if (viewportSurfaceCleaveAction_)
                {
                    const QSignalBlocker b(viewportSurfaceCleaveAction_);
                    viewportSurfaceCleaveAction_->setChecked(false);
                }
                if (viewportFrameAddAction_)
                {
                    const QSignalBlocker b(viewportFrameAddAction_);
                    viewportFrameAddAction_->setChecked(false);
                }

                glView_->setSectorCleaveToolEnabled(false);
                glView_->setSurfaceCleaveToolEnabled(false);
            }

        if (mode == PlacementMode::thingLight)
        {
            glView_->setPickMode(GlView::PickMode::Thing);
        }
        else if (mode == PlacementMode::sectorLight)
        {
            glView_->setPickMode(GlView::PickMode::Sector);
        }
        else if (mode != PlacementMode::none)
        {
            glView_->setPickMode(GlView::PickMode::Surface);
        }
        else
        {
            GlView::PickMode pickMode = GlView::PickMode::Auto;
            if (editMode_ == EditMode::sector)
            {
                pickMode = GlView::PickMode::Sector;
            }
            else if (editMode_ == EditMode::surface)
            {
                pickMode = GlView::PickMode::Surface;
            }
            else if (editMode_ == EditMode::thing || editMode_ == EditMode::frame)
            {
                pickMode = GlView::PickMode::Thing;
            }
            else if (editMode_ == EditMode::light)
            {
                pickMode = static_cast<GlView::PickMode>(lightEditPickModeValue());
            }
            glView_->setPickMode(pickMode);
        }

        // Placement modes use their own point handling. Add Thing suppresses selection while active.
        const bool preferSurfaceHit = (mode == PlacementMode::none);
        glView_->setSurfaceClickPreferSurfaceHit(preferSurfaceHit);
        syncViewportSelectionInputEnabled();
        glView_->setPlacementGestureEnabled(
            mode == PlacementMode::cube
            || (mode == PlacementMode::thing && thingPlacementToolMode_ == ThingPlacementToolMode::brush));

        // Thing and Sector light placement need owner hover picking to update
        // their valid-target cursor while the pointer moves over the viewport.
        glView_->setHoverPickEnabled(
            mode == PlacementMode::thingLight
            || mode == PlacementMode::sectorLight);
        glView_->unsetCursor();
        glView_->setHighlightedSelection(document_.selection());
        thingLightPlacementHoverThing_ = -1;
        sectorLightPlacementHoverSector_ = -1;

        glView_->setViewportPickingEnabled(editMode_ != EditMode::none || mode != PlacementMode::none);
    }

    syncViewportContextualOverlays();
    syncThingPlacementPanel();
    if (mode == PlacementMode::cube && glView_ && sectorDrawStage_ == SectorDrawStage::none)
    {
        updateSectorDrawCursorGuide(glView_->mapFromGlobal(QCursor::pos()));
    }
    else if (mode == PlacementMode::thing && glView_)
    {
        updateThingPlacementPreview(glView_->mapFromGlobal(QCursor::pos()), QApplication::keyboardModifiers());
    }
    else if (mode == PlacementMode::thingLight && glView_)
    {
        updateThingLightPlacementTarget(-1);
    }
    else if (mode == PlacementMode::sectorLight && glView_)
    {
        updateSectorLightPlacementTarget(-1);
    }

    // Placement ownership is editor state even when the status bar is not
    // available during early MainWindow construction.
    noteRecoveryStateChanged(false);
    if (!statusBar())
    {
        return;
    }

    switch (mode)
    {
        case PlacementMode::none:
        {
            statusBar()->clearMessage();
            break;
        }
        case PlacementMode::cube:
        {
            const QString shape = selectedSectorShapeName_.empty()
                ? QStringLiteral("sector")
                : ui::sector::shapeLabel(selectedSectorShapeName_);
            statusBar()->showMessage(QString("Press and drag to draw %1 footprint; release for height (Shift: equal sides, Esc: cancel).").arg(shape));
            break;
        }
        case PlacementMode::thing:
        {
            const QString tiltKey = ui::shortcuts::modifierBindingsText(
                thingPlacementTiltModifierBindings());
            const QString freeKey = ui::shortcuts::modifierBindingsText(
                thingPlacementFreeModifierBindings());
            statusBar()->showMessage(
                QString("Click to place %1 (default: %2, %3 toggles tilt/surface, %4 toggles free/surface, Esc: cancel).")
                    .arg(selectedThingTemplateLabel(), thingPlacementModeLabel(thingPlacementDefaultMode_), tiltKey, freeKey));
            break;
        }
        case PlacementMode::light:
        {
            statusBar()->showMessage("Click in the viewport to place a light (Esc to cancel).");
            break;
        }
        case PlacementMode::thingLight:
        {
            statusBar()->showMessage(
                thingLightPlacementKind_ == ThingLightPlacementKind::headlight
                    ? QStringLiteral("Click a placed actor/player to add a headlight (Esc to cancel).")
                    : QStringLiteral("Click a placed Thing to add a runtime light (Esc to cancel)."));
            break;
        }
        case PlacementMode::sectorLight:
        {
            statusBar()->showMessage(
                "Click a sector to place a point light at its center (Esc to cancel).");
            break;
        }
    }
}

bool MainWindow::documentEditingAllowed() const noexcept
{
    return !glView_
        || glView_->renderPipeline() != GlView::RenderPipeline::GamePreview;
}

void MainWindow::captureEditingUiVisibilityForGamePreview()
{
    ++previewEditingUiTransitionSerial_;
    // A new Preview transition supersedes any queued Editor-side UV splitter
    // refinement. Its stale callbacks are serial-rejected, so release their
    // dedicated notification guard here as part of the same transition.
    previewUvEditorRestoreInProgress_ = false;
    QList<int> uvSplitterSizes = viewportUvSplitter_
        ? viewportUvSplitter_->sizes()
        : QList<int>{};
    if (viewportUvSplitter_
        && uvEditorPreviewSettledLogicalDocumentEpoch_ == recoveryDocumentEpoch_
        && uvEditorPreviewSettledLogicalSplitterSizes_.size()
            == viewportUvSplitter_->count())
    {
        uvSplitterSizes = uvEditorPreviewSettledLogicalSplitterSizes_;
    }
    // Consume regardless of validity. No later Preview transition or document
    // is allowed to reuse an endpoint produced by this settle/capture pair.
    uvEditorPreviewSettledLogicalSplitterSizes_.clear();
    uvEditorPreviewSettledLogicalDocumentEpoch_ = 0;
    previewEditingUiVisibility_ = {
        .valid = true,
        .documentEpoch = recoveryDocumentEpoch_,
        // isHidden() records the widget's explicit state even when a hidden
        // ancestor makes isVisible() false during startup and test setup.
        .uvEditor = uvEditorPanelHost_ && !uvEditorPanelHost_->isHidden(),
        .uvEditorSplitterSizes = uvSplitterSizes,
        .thingPlacement = thingPlacementPanel_
            && !thingPlacementPanel_->isHidden(),
        .thingPlacementCollapsed = thingPlacementCollapsedBar_
            && !thingPlacementCollapsedBar_->isHidden(),
        .thingPlacementGeometry = thingPlacementPanel_
            ? thingPlacementPanel_->geometry()
            : QRect{},
        .thingPlacementCollapsedGeometry = thingPlacementCollapsedBar_
            ? thingPlacementCollapsedBar_->geometry()
            : QRect{},
        .cogScriptEditor = cogScriptEditorDock_
            && !cogScriptEditorDock_->isHidden(),
        .cogScriptEditorGeometry = cogScriptEditorDock_
            ? cogScriptEditorDock_->geometry()
            : QRect{},
        .cogScriptEditorWidth = cogScriptEditorDock_
            ? cogScriptEditorDock_->width()
            : 0,
    };
    for ( const QPointer<QDockWidget>& dock : cogScriptEditorDocks() )
    {
        if ( dock && dock != cogScriptEditorDock_ )
        {
            previewEditingUiVisibility_.detachedCogScriptEditorVisibility.emplace_back(
                dock, !dock->isHidden());
        }
    }
}

void MainWindow::settleEditingUiTransitionsForGamePreview()
{
    // Preview snapshots describe the endpoint the user requested, not an
    // arbitrary animation sample.  Finishing these transitions first also
    // restores the splitter/widget invariants normally repaired by their
    // finished callbacks; Preview can then hide and later restore a stable
    // authored workspace in one pass.
    settleUvEditorPanelTransitionForGamePreview();

    const bool thingPanelTransitionActive = thingPlacementPanelAnimation_
        && (thingPlacementPanelAnimation_->state()
                == QAbstractAnimation::Running
            || thingPlacementPanelCollapsePending_);
    if (thingPanelTransitionActive)
    {
        const QRect requestedGeometry =
            thingPlacementPanelAnimation_->endValue().toRect();
        thingPlacementPanelAnimation_->stop();
        if (thingPlacementPanel_ && requestedGeometry.isValid())
        {
            thingPlacementPanel_->setGeometry(requestedGeometry);
        }
        if (!thingPlacementPanelVisible_ && thingPlacementCollapsedBar_
            && requestedGeometry.isValid())
        {
            thingPlacementCollapsedBar_->setGeometry(requestedGeometry);
        }
    }
    if (thingPlacementCollapsedBarAnimation_
        && thingPlacementCollapsedBarAnimation_->state()
            == QAbstractAnimation::Running)
    {
        const QRect requestedGeometry =
            thingPlacementCollapsedBarAnimation_->endValue().toRect();
        thingPlacementCollapsedBarAnimation_->stop();
        if (thingPlacementCollapsedBar_ && requestedGeometry.isValid())
        {
            thingPlacementCollapsedBar_->setGeometry(requestedGeometry);
        }
    }
    thingPlacementPanelCollapsePending_ = false;
    if (editMode_ == EditMode::thing)
    {
        if (thingPlacementPanel_)
        {
            thingPlacementPanel_->setVisible(thingPlacementPanelVisible_);
        }
        if (thingPlacementCollapsedBar_)
        {
            thingPlacementCollapsedBar_->setVisible(
                !thingPlacementPanelVisible_);
        }
    }
}

void MainWindow::hideEditingUiForGamePreview()
{
    if (documentEditingAllowed())
    {
        return;
    }

    if (uvEditorPanelAnimation_)
    {
        uvEditorPanelAnimation_->stop();
        uvEditorPanelAnimation_->deleteLater();
        uvEditorPanelAnimation_ = nullptr;
    }
    uvEditorPanelHideTransitionActive_ = false;
    if (thingPlacementPanelAnimation_)
    {
        thingPlacementPanelAnimation_->stop();
    }
    if (thingPlacementCollapsedBarAnimation_)
    {
        thingPlacementCollapsedBarAnimation_->stop();
    }
    thingPlacementPanelCollapsePending_ = false;

    for (QWidget* widget : std::array<QWidget*, 14>{
             uvEditorPanelHost_,
             thingPlacementPanel_,
             thingPlacementCollapsedBar_,
             viewportEditModeOverlay_,
             viewportTransformOverlay_,
             viewportToolsOverlay_,
             addSectorOptionsOverlay_,
             transformMoveOptionsOverlay_,
             lightBakeOverlay_,
             sectorJoinOptionsOverlay_,
             sectorThrustOptionsOverlay_,
             cleaveOptionsOverlay_,
             thingMoveOptionsOverlay_,
             surfaceExtrudeOptionsOverlay_ })
    {
        if (widget)
        {
            widget->hide();
        }
    }
    for ( const QPointer<QDockWidget>& dock : cogScriptEditorDocks() )
    {
        if ( !dock )
        {
            continue;
        }
        dock->hide();
        if ( QAction* const toggle = dock->toggleViewAction() )
        {
            toggle->setEnabled(false);
        }
    }
}

void MainWindow::applyEditingUiVisibilitySnapshotForGamePreview(
    const PreviewEditingUiVisibility& snapshot,
    const bool restoreWorkspaceDock)
{
    if (!snapshot.valid || snapshot.documentEpoch != recoveryDocumentEpoch_)
    {
        return;
    }

    // Preview visibility is transient. Avoid replaying dock animations while
    // restoring the authored workspace allocation.
    const bool mainWindowAnimated = isAnimated();
    const bool workspaceAnimated = workspaceWindow_
        ? workspaceWindow_->isAnimated()
        : false;
    setAnimated(false);
    if (workspaceWindow_)
    {
        workspaceWindow_->setAnimated(false);
    }

    // Restore the workspace dock first. Showing or hiding it reallocates the
    // central viewport; splitter and in-viewport geometry are therefore
    // restored only after this request has been published.
    if (restoreWorkspaceDock && cogScriptEditorDock_)
    {
        cogScriptEditorDock_->setVisible(snapshot.cogScriptEditor);
        if (snapshot.cogScriptEditor)
        {
            if (cogScriptEditorDock_->isFloating()
                && snapshot.cogScriptEditorGeometry.isValid())
            {
                cogScriptEditorDock_->setGeometry(
                    snapshot.cogScriptEditorGeometry);
            }
            else if (workspaceWindow_ && snapshot.cogScriptEditorWidth > 0)
            {
                workspaceWindow_->resizeDocks(
                    { cogScriptEditorDock_ },
                    { snapshot.cogScriptEditorWidth },
                    Qt::Horizontal);
            }
        }
    }
    if ( restoreWorkspaceDock )
    {
        const auto currentDocks = cogScriptEditorDocks();
        for ( const auto& [dock, visible] : snapshot.detachedCogScriptEditorVisibility )
        {
            if ( dock && std::find(currentDocks.begin(), currentDocks.end(), dock) != currentDocks.end() )
            {
                dock->setVisible(visible);
            }
        }
    }

    {
        // Showing a hidden QSplitter child can emit splitterMoved() while Qt
        // assigns its size hint. Treat visibility and exact-size restoration
        // as one programmatic transaction so that intermediate geometry does
        // not overwrite the captured authored extent.
        const QScopedValueRollback<bool> programmaticUvResize(
            uvEditorPanelProgrammaticResize_, true);
        const bool uvVisibilityStillMatches = uvEditorPanelHost_
            && (!uvEditorPanelHost_->isHidden()) == snapshot.uvEditor;
        if (snapshot.uvEditor && uvEditorPanelExpanded_
            && uvEditorPanelHost_ && viewportUvSplitter_
            && snapshot.uvEditorSplitterSizes.size()
                == viewportUvSplitter_->count())
        {
            const int panelIndex = viewportUvSplitter_->indexOf(
                uvEditorPanelHost_);
            if (panelIndex >= 0
                && panelIndex < snapshot.uvEditorSplitterSizes.size())
            {
                // The splitter allocation is the workspace state the user
                // could see when Preview was entered. Keep the UV panel's
                // remembered expanded extent in lockstep with it before
                // showing the host. Otherwise a later same-mode sync can
                // replace the exact allocation with a stale default extent.
                uvEditorPanelExpandedHeight_ = std::max(
                    1, snapshot.uvEditorSplitterSizes[panelIndex]);
            }
        }
        if (uvEditorPanelHost_ && restoreWorkspaceDock)
        {
            uvEditorPanelHost_->setVisible(snapshot.uvEditor);
        }
        if (uvEditorPanelHost_
            && (restoreWorkspaceDock || uvVisibilityStillMatches)
            && snapshot.uvEditor && viewportUvSplitter_
            && snapshot.uvEditorSplitterSizes.size()
                == viewportUvSplitter_->count())
        {
            viewportUvSplitter_->setSizes(snapshot.uvEditorSplitterSizes);
        }
    }

    const bool thingVisibilityStillMatches = thingPlacementPanel_
        && (!thingPlacementPanel_->isHidden()) == snapshot.thingPlacement;
    const bool collapsedVisibilityStillMatches = thingPlacementCollapsedBar_
        && (!thingPlacementCollapsedBar_->isHidden())
            == snapshot.thingPlacementCollapsed;
    const bool canRecomputeThingEndpoint = !restoreWorkspaceDock
        && editMode_ == EditMode::thing
        && thingVisibilityStillMatches
        && collapsedVisibilityStillMatches;
    if (thingPlacementPanel_
        && (restoreWorkspaceDock || thingVisibilityStillMatches))
    {
        if (!canRecomputeThingEndpoint
            && snapshot.thingPlacementGeometry.isValid())
        {
            thingPlacementPanel_->setGeometry(
                snapshot.thingPlacementGeometry);
        }
        if (restoreWorkspaceDock)
        {
            thingPlacementPanel_->setVisible(snapshot.thingPlacement);
        }
    }
    if (thingPlacementCollapsedBar_
        && (restoreWorkspaceDock || collapsedVisibilityStillMatches))
    {
        if (!canRecomputeThingEndpoint
            && snapshot.thingPlacementCollapsedGeometry.isValid())
        {
            thingPlacementCollapsedBar_->setGeometry(
                snapshot.thingPlacementCollapsedGeometry);
        }
        if (restoreWorkspaceDock)
        {
            thingPlacementCollapsedBar_->setVisible(
                snapshot.thingPlacementCollapsed);
        }
    }
    if (canRecomputeThingEndpoint)
    {
        // Recovery can only sample the in-viewport palette before the nested
        // dock's queued ratio has fully settled. Recompute its canonical
        // endpoint from the final viewport and persisted dock/size members on
        // the post-layout pass; this is the same geometry normal Editor resize
        // handling would establish, without replaying an animation.
        const QScopedValueRollback<bool> restorePreviewUiGuard(
            previewEditingUiRestoreInProgress_, true);
        positionThingPlacementPanel();
    }

    if (cogScriptEditorDock_
        && (!cogScriptEditorDock_->isHidden()) == snapshot.cogScriptEditor
        && snapshot.cogScriptEditorGeometry.isValid()
        && (!snapshot.cogScriptEditor || cogScriptEditorDock_->isFloating()))
    {
        // Hidden and floating docks do not have their exact rectangle managed
        // by the dock-area ratio. Preserve it explicitly after the viewport
        // panes, including during the queued post-layout refinement.
        cogScriptEditorDock_->setGeometry(
            snapshot.cogScriptEditorGeometry);
    }

    if (workspaceWindow_)
    {
        workspaceWindow_->setAnimated(workspaceAnimated);
    }
    setAnimated(mainWindowAnimated);
}

void MainWindow::restoreEditingUiVisibilityAfterGamePreview()
{
    const PreviewEditingUiVisibility snapshot = previewEditingUiVisibility_;
    previewEditingUiVisibility_ = {};
    previewUvEditorRestoreInProgress_ = snapshot.valid
        && snapshot.documentEpoch == recoveryDocumentEpoch_
        && snapshot.uvEditor;
    for ( const QPointer<QDockWidget>& dock : cogScriptEditorDocks() )
    {
        // Re-enabling the current document's Tools editor toggle is an
        // Editor-pipeline invariant, not part of restoring the previous
        // document's visibility. A document switch deliberately invalidates
        // the snapshot but must never strand this QAction disabled.
        if ( QAction* const toggle = dock ? dock->toggleViewAction() : nullptr )
        {
            toggle->setEnabled(true);
        }
    }
    if (!snapshot.valid || snapshot.documentEpoch != recoveryDocumentEpoch_)
    {
        previewUvEditorRestoreInProgress_ = false;
        return;
    }
    applyEditingUiVisibilitySnapshotForGamePreview(snapshot, true);

    // QMainWindow intentionally publishes dock ratios through its queued
    // layout pass. Reapply only the dependent allocations after that pass;
    // never spin processEvents() or force QLayout::activate(), both of which
    // can re-enter restoreState(). Visibility-match and transition guards keep
    // this refinement from overriding a newer mode, document, or Preview.
    const std::uint64_t transitionSerial =
        previewEditingUiTransitionSerial_;
    QTimer::singleShot(0, this,
        [this, snapshot, transitionSerial]()
        {
            if (transitionSerial != previewEditingUiTransitionSerial_
                || snapshot.documentEpoch != recoveryDocumentEpoch_
                || !documentEditingAllowed())
            {
                if (transitionSerial == previewEditingUiTransitionSerial_)
                {
                    previewUvEditorRestoreInProgress_ = false;
                }
                return;
            }
            applyEditingUiVisibilitySnapshotForGamePreview(snapshot, false);

            // restoreState()/dockLocationChanged can enqueue one more
            // zero-delay width restoration from this first turn. Publish a
            // second guarded refinement after that known nested callback so
            // QSplitter sizes and viewport-relative palette geometry are the
            // final observable state, not the intermediate dock allocation.
            QTimer::singleShot(0, this,
                [this, snapshot, transitionSerial]()
                {
                    if (transitionSerial
                            != previewEditingUiTransitionSerial_
                        || snapshot.documentEpoch
                            != recoveryDocumentEpoch_
                        || !documentEditingAllowed())
                    {
                        if (transitionSerial
                            == previewEditingUiTransitionSerial_)
                        {
                            previewUvEditorRestoreInProgress_ = false;
                        }
                        return;
                    }
                    applyEditingUiVisibilitySnapshotForGamePreview(
                        snapshot, false);

                    // QSplitter can publish the handle movement caused by
                    // the exact allocation after this callback returns. Keep
                    // the restore guard for one final event-loop turn, then
                    // reassert the authored geometry and release it.
                    QTimer::singleShot(0, this,
                        [this, snapshot, transitionSerial]()
                        {
                            if (transitionSerial
                                    != previewEditingUiTransitionSerial_
                                || snapshot.documentEpoch
                                    != recoveryDocumentEpoch_
                                || !documentEditingAllowed())
                            {
                                if (transitionSerial
                                    == previewEditingUiTransitionSerial_)
                                {
                                    previewUvEditorRestoreInProgress_ = false;
                                }
                                return;
                            }
                            applyEditingUiVisibilitySnapshotForGamePreview(
                                snapshot, false);
                            previewUvEditorRestoreInProgress_ = false;
                        });
                });
        });
}

void MainWindow::cancelDocumentMutationPreviewsForGamePreview()
{
    if (documentEditingAllowed())
    {
        return;
    }

    // Settle an already-authored brush stroke before making the document
    // view-only, then tear down every transient editor preview without
    // publishing another authored change in Game Preview.
    cancelArchitecturalLightingInteractiveEdit();
    if (thingPlacementBrushStrokeActive_)
    {
        (void)finalizeThingPlacementBrushStroke();
    }
    if (placementMode_ == PlacementMode::thing)
    {
        deselectThingPlacementTemplate();
    }
    else if (placementMode_ != PlacementMode::none)
    {
        setPlacementMode(PlacementMode::none);
    }
    cancelSectorDrawPlacement();
    (void)cancelActiveUvEditorModalTransform();
    deactivateViewportEditFunctions();
    if (sectorJoinPreviewActive_)
    {
        cancelSectorJoinPreview();
    }
    if (surfaceExtrudePreviewActive_)
    {
        cancelSurfaceExtrudePreview();
    }
    if (cogEntityPickActive_)
    {
        cogEntityPickActive_ = false;
        cogEntityPickCogIndex_ = -1;
        cogEntityPickValueIndex_ = -1;
        cogEntityPickRestorePlacementMode_ = PlacementMode::none;
        if (glView_)
        {
            glView_->setCogEntityPickHighlightEnabled(false);
            glView_->unsetCursor();
        }
    }
    restoreLightBakeTransformPreview();
    cancelLightBake(true);
    deactivateViewportTransformForExclusiveTool();
    // Selection remains available in Preview, but Select is only a checked
    // sentinel: the transform interaction gate stays off and every mutating
    // transform QAction is disabled by updateEditActions().
    setViewportTransformActionChecks(viewportTransformSelectAction_);
    if (glView_)
    {
        glView_->setTransformTool(GlView::TransformTool::Select);
        glView_->setTransformInteractionEnabled(false);
    }
    syncViewportSelectionInputEnabled();
    transformPreview_ = {};
}

void MainWindow::updateEditActions()
{
    const editor::Selection& selection = document_.selection();
    const bool selectionReadOnly = selectionTouchesNonEditableLayer(selection);
    const bool editingAllowed = documentEditingAllowed();

    // A read-only selection has one pointer owner: Select. Do not leave an
    // unavailable transform or modification tool armed, and deliberately
    // forget it so making the layer editable cannot resurrect it later.
    if (selectionReadOnly && editingAllowed)
    {
        if (placementMode_ == PlacementMode::thing)
        {
            deselectThingPlacementTemplate();
        }
        else if (placementMode_ != PlacementMode::none)
        {
            setPlacementMode(PlacementMode::none);
        }
        deactivateViewportEditFunctions();
        setViewportTransformActionChecks(viewportTransformSelectAction_);
        rememberedViewportTransformAction_ = viewportTransformSelectAction_;
        if (glView_)
        {
            // Drop any in-flight transform/property preview before publishing
            // Select, even if GlView was already using Select as its sentinel.
            glView_->setTransformInteractionEnabled(false);
            glView_->setTransformTool(GlView::TransformTool::Select);
        }
        syncViewportSelectionInputEnabled();
    }
    const bool canQueryCurrentMode =
        editMode_ == EditMode::sector
        || editMode_ == EditMode::surface
        || editMode_ == EditMode::thing
        || editMode_ == EditMode::frame;
    const bool hasSurfaceOnlySelection = !selection.surfaces.empty() && selection.sectors.empty() && selection.things.empty()
        && selection.lights.empty() && selection.cogs.empty();
    const bool hasExactlyOneSurfaceOnlySelection = selection.surfaces.size() == 1 && selection.sectors.empty() && selection.things.empty()
        && selection.lights.empty() && selection.cogs.empty();
    const bool hasExactlyTwoSurfacesOnlySelection = selection.surfaces.size() == 2 && selection.sectors.empty() && selection.things.empty()
        && selection.lights.empty() && selection.cogs.empty();
    const bool hasExactlyTwoSectorsOnlySelection = selection.sectors.size() == 2 && selection.surfaces.empty() && selection.things.empty()
        && selection.lights.empty() && selection.cogs.empty();

    bool hasExactlyTwoSurfacesInDifferentSectors = false;
    if (hasExactlyTwoSurfacesOnlySelection)
    {
        auto it = selection.surfaces.begin();
        const SurfaceRef a = *it++;
        const SurfaceRef b = *it;
        hasExactlyTwoSurfacesInDifferentSectors = (a.sector != b.sector);
    }

    const bool cogHistoryFocus = cogScriptEditorOwnsFocus();
    const bool textEntryHistoryFocus = ui::shortcuts::focusAcceptsTextInput(
        QApplication::focusWidget());
    const bool nativeTextHistoryFocus = ui::shortcuts::focusedNativeTextEditor(
        QApplication::focusWidget()) != nullptr;
    const bool stagedCleaveHistoryFocus = !cogHistoryFocus
        && !textEntryHistoryFocus
        && glView_
        && glView_->stagedCleaveHistoryOwnsUndo();
    const bool documentCleaveApplyRedo = cleaveApplyDocumentRedoHasPriority();
    if ( undoAction_ )
    {
        const bool canUndo = cogHistoryFocus
            ? activeCogScriptEditorCanUndoRedo(false)
            : nativeTextHistoryFocus
                ? ui::shortcuts::nativeTextEditorCanUndoRedo(
                    QApplication::focusWidget(), false)
                : textEntryHistoryFocus
                    ? false
                : stagedCleaveHistoryFocus
                    ? glView_->canUndoStagedCleaveHistory()
                    : document_.canUndo();
        undoAction_->setEnabled(editingAllowed && canUndo);
        const std::string_view label = cogHistoryFocus
                || nativeTextHistoryFocus
                || textEntryHistoryFocus
                || stagedCleaveHistoryFocus
            ? std::string_view{}
            : document_.nextUndoLabel();
        if (!label.empty())
        {
            undoAction_->setText(QString("&Undo %1").arg(QString::fromStdString(std::string(label))));
        }
        else
        {
            undoAction_->setText("&Undo");
        }
    }

    if ( redoAction_ )
    {
        const bool canRedo = cogHistoryFocus
            ? activeCogScriptEditorCanUndoRedo(true)
            : nativeTextHistoryFocus
                ? ui::shortcuts::nativeTextEditorCanUndoRedo(
                    QApplication::focusWidget(), true)
                : textEntryHistoryFocus
                    ? false
                : stagedCleaveHistoryFocus && !documentCleaveApplyRedo
                    ? glView_->canRedoStagedCleaveHistory()
                    : document_.canRedo();
        redoAction_->setEnabled(editingAllowed && canRedo);
        const std::string_view label = cogHistoryFocus
                || nativeTextHistoryFocus
                || textEntryHistoryFocus
                || (stagedCleaveHistoryFocus && !documentCleaveApplyRedo)
            ? std::string_view{}
            : document_.nextRedoLabel();
        if (!label.empty())
        {
            redoAction_->setText(QString("&Redo %1").arg(QString::fromStdString(std::string(label))));
        }
        else
        {
            redoAction_->setText("&Redo");
        }
    }

    // Preserve the viewport's base availability separately from the focused
    // COG override. The final focus-aware publication happens after every
    // action refresh, so this routine cannot accidentally enable a COG-only
    // shortcut while the viewport owns focus (or restore stale Find state).
    levelFindBaseEnabled_ = canQueryCurrentMode;
    levelFindNextBaseEnabled_ = canQueryCurrentMode;
    cogEditorActionsBaseEnabled_ = editingAllowed;

    rebuildHistoryView();

    if (deleteAction_)
    {
        bool enable = editingAllowed && !selection.empty();
        if (editMode_ == EditMode::frame)
        {
            // Frame mode primarily deletes frame points, but allow cog deletion too (cog editing is independent).
            enable = editingAllowed
                && (!selection.frames.empty() || !selection.cogs.empty());
        }
        if (selectionReadOnly)
        {
            enable = false;
        }
        deleteAction_->setEnabled(enable);

        if (sectorDeleteMenuAction_)
        {
            const bool sectorOnly = !selection.sectors.empty()
                && selection.surfaces.empty()
                && selection.surfaceVertices.empty()
                && selection.things.empty()
                && selection.frames.empty()
                && selection.lights.empty()
                && selection.cogs.empty();
            sectorDeleteMenuAction_->setEnabled(
                editingAllowed && !selectionReadOnly && sectorOnly
                && editMode_ == EditMode::sector);
        }
        if (surfaceDeleteMenuAction_)
        {
            const bool noOtherFamilies = selection.sectors.empty()
                && selection.things.empty()
                && selection.frames.empty()
                && selection.lights.empty()
                && selection.cogs.empty();
            const bool surfaceTarget =
                surfaceSelectionMode_ == SurfaceSelectionMode::face
                    ? !selection.surfaces.empty()
                    : surfaceSelectionMode_ == SurfaceSelectionMode::vertex
                        ? !selection.surfaceVertices.empty()
                        : false;
            surfaceDeleteMenuAction_->setEnabled(
                editingAllowed && !selectionReadOnly && noOtherFamilies
                && surfaceTarget && editMode_ == EditMode::surface);
        }
        if (thingDeleteMenuAction_)
        {
            const bool thingOnly = !selection.things.empty()
                && selection.sectors.empty()
                && selection.surfaces.empty()
                && selection.surfaceVertices.empty()
                && selection.lights.empty()
                && selection.cogs.empty();
            const bool thingTarget = editMode_ == EditMode::thing
                ? thingOnly && selection.frames.empty()
                : editMode_ == EditMode::frame
                    ? thingOnly && !selection.frames.empty()
                    : false;
            thingDeleteMenuAction_->setEnabled(
                editingAllowed && !selectionReadOnly && thingTarget);
        }
        if (lightDeleteMenuAction_)
        {
            const bool noSurfaceOrCogTargets = selection.surfaces.empty()
                && selection.surfaceVertices.empty()
                && selection.frames.empty()
                && selection.cogs.empty();
            const bool lightTarget = lightEditSubMode_ == LightEditSubMode::baked
                ? !selection.lights.empty() && selection.sectors.empty()
                    && selection.things.empty()
                : (lightEditSubMode_ == LightEditSubMode::thing
                    || lightEditSubMode_ == LightEditSubMode::architectural)
                    ? !selection.things.empty() && selection.sectors.empty()
                        && selection.lights.empty()
                    : !selection.sectors.empty() && selection.things.empty()
                        && selection.lights.empty();
            lightDeleteMenuAction_->setEnabled(
                editingAllowed && !selectionReadOnly
                && noSurfaceOrCogTargets && lightTarget
                && editMode_ == EditMode::light);
        }
        if (cogDeleteMenuAction_)
        {
            const bool cogOnly = !selection.cogs.empty()
                && selection.sectors.empty()
                && selection.surfaces.empty()
                && selection.surfaceVertices.empty()
                && selection.things.empty()
                && selection.frames.empty()
                && selection.lights.empty();
            cogDeleteMenuAction_->setEnabled(
                editingAllowed && cogOnly
                && !selectionReadOnly);
        }
    }

    if (addCubeAction_)
    {
        addCubeAction_->setEnabled(
            editingAllowed && editMode_ == EditMode::sector);
    }
    if (exportSectorAction_)
    {
        exportSectorAction_->setEnabled(!selection.sectors.empty());
    }
    if (forceDeleteSurfaceVertexAction_)
    {
        forceDeleteSurfaceVertexAction_->setEnabled(
            editMode_ == EditMode::surface
            && surfaceSelectionMode_ == SurfaceSelectionMode::vertex
            && !selection.surfaceVertices.empty()
            && !selectionReadOnly
            && editingAllowed);
    }
    if (importSectorAction_)
    {
        importSectorAction_->setEnabled(
            editingAllowed && editMode_ == EditMode::sector);
    }

    if (duplicateCogAction_)
    {
        duplicateCogAction_->setEnabled(
            editingAllowed
            && !selection.cogs.empty()
            && selection.sectors.empty()
            && selection.surfaces.empty()
            && selection.surfaceVertices.empty()
            && selection.things.empty()
            && selection.lights.empty());
    }

    if (extrudeSurfaceAction_)
    {
        extrudeSurfaceAction_->setEnabled(editingAllowed
            && ((hasExactlyOneSurfaceOnlySelection && !selectionReadOnly)
                || surfaceExtrudePreviewActive_));
    }

    if (connectSurfacesAction_)
    {
        connectSurfacesAction_->setEnabled(editingAllowed
            && hasExactlyTwoSurfacesInDifferentSectors && !selectionReadOnly);
    }

    if (connectSectorsAction_)
    {
        connectSectorsAction_->setEnabled(editingAllowed
            && hasExactlyTwoSectorsOnlySelection && !selectionReadOnly);
    }

    if (translateAction_)
    {
        translateAction_->setEnabled(editingAllowed && !selectionReadOnly && !(selection.sectors.empty() && selection.surfaces.empty()
            && selection.things.empty() && selection.lights.empty()));
    }

    if (rotateAction_)
    {
        rotateAction_->setEnabled(editingAllowed && !selectionReadOnly && !(selection.sectors.empty() && selection.surfaces.empty()
            && selection.things.empty() && selection.lights.empty()));
    }

    if (scaleAction_)
    {
        scaleAction_->setEnabled(editingAllowed && !selectionReadOnly && !(selection.sectors.empty() && selection.surfaces.empty()
            && selection.things.empty() && selection.lights.empty()));
    }

    if (mirrorAction_)
    {
        mirrorAction_->setEnabled(editingAllowed && !selectionReadOnly && selection.surfaces.empty() && selection.cogs.empty()
            && !(selection.sectors.empty() && selection.things.empty() && selection.lights.empty()));
    }

    if (flipSurfaceAction_)
    {
        flipSurfaceAction_->setEnabled(editingAllowed && hasSurfaceOnlySelection && !selectionReadOnly);
    }

    if (uvAutoAlignAction_)
    {
        uvAutoAlignAction_->setEnabled(editingAllowed && hasSurfaceOnlySelection && !selectionReadOnly);
    }
    if (uvEditorAction_)
    {
        uvEditorAction_->setEnabled(editingAllowed && hasSurfaceOnlySelection && !selectionReadOnly);
    }
    if (uvMoveAction_)
    {
        uvMoveAction_->setEnabled(editingAllowed && hasSurfaceOnlySelection && !selectionReadOnly);
    }
    if (uvRotateAction_)
    {
        uvRotateAction_->setEnabled(editingAllowed && hasSurfaceOnlySelection && !selectionReadOnly);
    }
    if (uvScaleAction_)
    {
        uvScaleAction_->setEnabled(editingAllowed && hasSurfaceOnlySelection && !selectionReadOnly);
    }
    if (uvStitchFromReferenceAction_)
    {
        uvStitchFromReferenceAction_->setEnabled(editingAllowed && hasSurfaceOnlySelection && selection.surfaces.size() >= 2 && !selectionReadOnly);
    }
    if (uvStitchConnectedAction_)
    {
        uvStitchConnectedAction_->setEnabled(editingAllowed && hasSurfaceOnlySelection && selection.surfaces.size() >= 2 && !selectionReadOnly);
    }
    if (uvCopyFromReferenceAction_)
    {
        uvCopyFromReferenceAction_->setEnabled(editingAllowed && hasSurfaceOnlySelection && selection.surfaces.size() >= 2 && !selectionReadOnly);
    }

    if (makeAdjoinAction_ || unAdjoinAction_)
    {
        bool canMakeAdjoin = false;
        bool canUnAdjoin = false;

        if (hasExactlyOneSurfaceOnlySelection)
        {
            const SurfaceRef surfaceRef = *selection.surfaces.begin();
            const Level& level = document_.level();

            if (surfaceRef.sector >= 0 && surfaceRef.surface >= 0
                && static_cast<std::size_t>(surfaceRef.sector) < level.sectors.size())
            {
                const Sector& sector = level.sectors[static_cast<std::size_t>(surfaceRef.sector)];
                if (static_cast<std::size_t>(surfaceRef.surface) < sector.surfaces.size())
                {
                    const Surface& surface = sector.surfaces[static_cast<std::size_t>(surfaceRef.surface)];
                    canMakeAdjoin = !surface.adjoin.has_value();
                    canUnAdjoin = surface.adjoin.has_value();
                }
            }
        }

        if (makeAdjoinAction_)
        {
            makeAdjoinAction_->setEnabled(editingAllowed && canMakeAdjoin && !selectionReadOnly);
        }
        if (unAdjoinAction_)
        {
            unAdjoinAction_->setEnabled(editingAllowed && canUnAdjoin && !selectionReadOnly);
        }
    }

    const bool lightMode = editMode_ == EditMode::light;
    const bool architecturalLightMode = lightMode
        && lightEditSubMode_ == LightEditSubMode::architectural;
    // Keep this in lock-step with LIGHT_OWNER_TRANSFORM_POLICY above. ARCH
    // authoring selects vertices only; its mode UI hides Move and Scale, so
    // their actions and shortcuts must be unavailable as well.
    const bool rotateMeaningful = !lightMode;
    const bool scaleMeaningful = editMode_ == EditMode::sector
        || editMode_ == EditMode::surface
        || editMode_ == EditMode::light;
    if (viewportTransformSelectAction_)
    {
        viewportTransformSelectAction_->setEnabled(editingAllowed);
    }
    if (viewportTransformMoveAction_)
    {
        viewportTransformMoveAction_->setEnabled(
            editingAllowed && !selectionReadOnly && !architecturalLightMode);
    }
    if (viewportTransformRotateAction_)
    {
        viewportTransformRotateAction_->setEnabled(editingAllowed && !selectionReadOnly && rotateMeaningful);
    }
    if (viewportTransformScaleAction_)
    {
        viewportTransformScaleAction_->setEnabled(
            editingAllowed && !selectionReadOnly && scaleMeaningful
            && !architecturalLightMode);
    }

    syncUvEditorPanel();
    const bool activeTransformUnavailable = glView_
        && ((glView_->transformTool() == GlView::TransformTool::Move
                && architecturalLightMode)
            || (glView_->transformTool() == GlView::TransformTool::Rotate && !rotateMeaningful)
            || (glView_->transformTool() == GlView::TransformTool::Scale
                && (!scaleMeaningful || architecturalLightMode)));
    if (activeTransformUnavailable
        && glView_
        && glView_->transformTool() != GlView::TransformTool::Select)
    {
        if (viewportTransformMoveAction_ && viewportTransformMoveAction_->isEnabled())
        {
            viewportTransformMoveAction_->trigger();
        }
        else
        {
            glView_->setTransformTool(GlView::TransformTool::Select);
            setViewportTransformActionChecks(viewportTransformSelectAction_);
            rememberedViewportTransformAction_ = viewportTransformSelectAction_;
            // The checked QAction is the authoritative pointer owner.  Keep
            // GlView interaction in lockstep when an unavailable Rotate/Scale
            // falls back to Select; previously the icon became active while
            // picking remained disabled until the user toggled it twice.
            glView_->setTransformInteractionEnabled(
                editingAllowed && !selectionReadOnly
                && !viewportModificationToolActive());
            syncViewportSelectionInputEnabled();
        }
    }
    if (viewportEditModeOverlay_)
    {
        const bool inPreview = glView_ && (glView_->renderPipeline() == GlView::RenderPipeline::GamePreview);
        const bool showEditOverlay = !inPreview;
        viewportEditModeOverlay_->setVisible(showEditOverlay);
        viewportEditModeOverlay_->setEnabled(editingAllowed);
        syncViewportSelectionEditabilityIndicator();
        if (showEditOverlay)
        {
            positionViewportEditModeOverlay();
        }
    }
    if (viewportTransformOverlay_)
    {
        const bool inPreview = glView_ && (glView_->renderPipeline() == GlView::RenderPipeline::GamePreview);
        const bool showTransformOverlay =
            (editMode_ != EditMode::none)
            && !inPreview;
        viewportTransformOverlay_->setVisible(showTransformOverlay);
        viewportTransformOverlay_->setEnabled(editingAllowed);
        if (showTransformOverlay)
        {
            positionViewportTransformOverlay();
        }
    }
    if (viewportToolsOverlay_)
    {
        viewportToolsOverlay_->setEnabled(editingAllowed && !selectionReadOnly);
    }
    if (glView_)
    {
        // A modification/placement tool owns the pointer while a remembered
        // transform remains suspended beneath it.  Keep transform gestures
        // disabled until that owner is deactivated; otherwise the final
        // updateEditActions() after recovery would accidentally re-enable
        // Move/Rotate/Scale beside the restored tool.
        const bool transformActionActive = std::ranges::any_of(
            std::array<QAction*, 4>{
                viewportTransformSelectAction_,
                viewportTransformMoveAction_,
                viewportTransformRotateAction_,
                viewportTransformScaleAction_ },
            [](const QAction* action)
            {
                return action && action->isChecked();
            });
        glView_->setTransformInteractionEnabled(
            editingAllowed && !selectionReadOnly && transformActionActive
            && !viewportModificationToolActive());
    }

    if (recalcLightingAction_)
    {
        const bool hasSectors = !document_.level().sectors.empty();
        recalcLightingAction_->setEnabled(
            editingAllowed
            && editMode_ == EditMode::light
            && lightEditSubMode_ == LightEditSubMode::baked
            && hasSectors);
    }
    const bool architecturalMode = editMode_ == EditMode::light
        && lightEditSubMode_ == LightEditSubMode::architectural
        && document_.level().kind == ProjectType::MOTS;
    if (QAction* const pasteAction = findChild<QAction*>(
            QStringLiteral("edit.paste_entities")))
    {
        const bool hasEntities = pasteAction->property(
            "sed.clipboard_has_entities").toBool();
        const bool hasArchitecturalLighting = pasteAction->property(
            "sed.clipboard_has_arch_lighting").toBool();
        pasteAction->setEnabled(
            editingAllowed
            && (architecturalMode ? hasArchitecturalLighting : hasEntities));
    }
    if (QAction* const copyAction = findChild<QAction*>(
            QStringLiteral("edit.copy_entities")))
    {
        copyAction->setText(
            architecturalMode ? QStringLiteral("&Copy ARCH lighting")
                              : QStringLiteral("&Copy Entities"));
        copyAction->setToolTip(
            architecturalMode
                ? QStringLiteral("Copy the active ARCHLIGHTING Thing for later paste.")
                : QStringLiteral("Copy selected Sectors, Things, or baked editor lights"));
        copyAction->setEnabled(editingAllowed);
    }
    if (QAction* const pasteAction = findChild<QAction*>(
            QStringLiteral("edit.paste_entities")))
    {
        pasteAction->setText(
            architecturalMode ? QStringLiteral("&Paste ARCH lighting")
                              : QStringLiteral("&Paste Entities"));
        pasteAction->setToolTip(
            architecturalMode
                ? QStringLiteral("Paste copied ARCHLIGHTING onto compatible selected Things.")
                : QStringLiteral("Paste copied entities at the viewport pointer"));
    }
    if (alwaysShowArchitecturalLightGizmosAction_)
    {
        // ARCHLIGHTING is a MotS-only authored resource. Keep its Map
        // Settings preference visible alongside the other light families,
        // but make the project boundary explicit rather than presenting a
        // checked, unusable control in JK or IJIM documents.
        alwaysShowArchitecturalLightGizmosAction_->setEnabled(
            document_.level().kind == ProjectType::MOTS);
    }
    if (lightEditArchitecturalAction_)
    {
        const bool inGamePreview = glView_
            && glView_->renderPipeline() == GlView::RenderPipeline::GamePreview;
        const bool architecturalAvailable =
            document_.level().kind == ProjectType::MOTS;
        lightEditArchitecturalAction_->setVisible(architecturalAvailable);
        lightEditArchitecturalAction_->setEnabled(
            editingAllowed && !inGamePreview && architecturalAvailable);
        if (viewportEditLightArchitecturalButton_)
        {
            viewportEditLightArchitecturalButton_->setVisible(
                architecturalAvailable);
        }
    }
    if (addLightAction_)
    {
        addLightAction_->setEnabled(
            editingAllowed
            && editMode_ == EditMode::light
            && lightEditSubMode_ == LightEditSubMode::baked
            && !selectionReadOnly);
    }
    if (addThingLightAction_)
    {
        addThingLightAction_->setEnabled(
            editingAllowed
            && editMode_ == EditMode::light
            && lightEditSubMode_ == LightEditSubMode::thing
            && !selectionReadOnly);
    }
    if (addSectorLightAction_)
    {
        addSectorLightAction_->setEnabled(
            editingAllowed
            && editMode_ == EditMode::light
            && lightEditSubMode_ == LightEditSubMode::sector
            && document_.level().kind == ProjectType::IJIM
            && !document_.level().sectors.empty()
            && !selectionReadOnly);
    }

    // These controls are either permanently available in Editor mode or own
    // their detailed eligibility internally. Recompute their preview state
    // from the live pipeline; never restore a cached enabled boolean.
    for (QAction* action : std::array<QAction*, 13>{
             addCogAction_,
             templateEditorAction_,
             autoRecalculateLightingAction_,
             editSectorAction_,
             editSurfaceAction_,
             editThingAction_,
             editLightAction_,
             editFrameAction_,
             surfaceFaceAction_,
             surfaceEdgeAction_,
             surfaceVertexAction_,
             viewportTransformGridSnapAction_,
             viewportTransformGridStickAction_ })
    {
        if (action)
        {
            action->setEnabled(editingAllowed);
        }
    }
    if (surfaceStrictSelectionAction_)
    {
        surfaceStrictSelectionAction_->setEnabled(
            editingAllowed && editMode_ == EditMode::surface
            && surfaceSelectionMode_ != SurfaceSelectionMode::face);
    }
    if (surfaceNormalsAction_)
    {
        surfaceNormalsAction_->setEnabled(
            editingAllowed && editMode_ == EditMode::surface);
    }
    if (addThingAction_)
    {
        addThingAction_->setEnabled(
            editingAllowed && editMode_ == EditMode::thing);
    }
    if (lightEditBakedAction_)
    {
        lightEditBakedAction_->setEnabled(editingAllowed);
    }
    if (lightEditThingAction_)
    {
        lightEditThingAction_->setEnabled(editingAllowed);
    }
    if (lightEditSectorAction_)
    {
        lightEditSectorAction_->setEnabled(
            editingAllowed && document_.level().kind == ProjectType::IJIM);
    }
    if (QAction* newCogScript =
            findChild<QAction*>(QStringLiteral("edit.new_cog_script")))
    {
        newCogScript->setEnabled(editingAllowed);
    }
    for (const QString& objectName : {
             QStringLiteral("edit.layers"),
             QStringLiteral("edit.episode_campaign_editor") })
    {
        if (QAction* action = findChild<QAction*>(objectName))
        {
            action->setEnabled(editingAllowed);
        }
    }
    for (const QString& objectName : {
             QStringLiteral("map.layers.visibility"),
             QStringLiteral("map.layers.locks") })
    {
        if (QMenu* menu = findChild<QMenu*>(objectName))
        {
            menu->menuAction()->setEnabled(editingAllowed);
        }
    }
    if (QAction* pasteEntities =
            findChild<QAction*>(QStringLiteral("edit.paste_entities")))
    {
        pasteEntities->setEnabled(editingAllowed
            && (architecturalMode
                ? pasteEntities->property("sed.clipboard_has_arch_lighting").toBool()
                : pasteEntities->property("sed.clipboard_has_entities").toBool()));
    }
    if (uvEditorPanelWidget_)
    {
        uvEditorPanelWidget_->setEnabled(editingAllowed);
    }
    // A Preview point-pick can rebuild Item Edit while the entire editor is
    // read-only. Refresh the rows and their embedded editors as well as the
    // outer panels when returning to Editor or changing layer editability.
    refreshPropertiesEditability();
    for ( const QPointer<QDockWidget>& dock : cogScriptEditorDocks() )
    {
        if ( !dock )
        {
            continue;
        }
        if ( QWidget* const contents = dock->widget() )
        {
            contents->setEnabled(editingAllowed);
        }
        if ( QAction* const toggle = dock->toggleViewAction() )
        {
            toggle->setEnabled(editingAllowed);
        }
    }
    for (const QString& objectName : {
             QStringLiteral("newCogScriptButton"),
             QStringLiteral("addCogButton"),
             QStringLiteral("duplicateCogButton"),
             QStringLiteral("deleteCogButton") })
    {
        if (QPushButton* button = findChild<QPushButton*>(objectName))
        {
            button->setEnabled(editingAllowed);
        }
    }
    if (thingPlacementPanel_)
    {
        thingPlacementPanel_->setEnabled(editingAllowed);
    }
    if (viewportMapSettingsButton_)
    {
        // The menu also owns Preview-only light/thrust visibility controls.
        // Its mutation rows are gated individually; keep the view controls
        // reachable in the read-only pipeline.
        viewportMapSettingsButton_->setEnabled(true);
    }
    for (QAction* action : std::array<QAction*, 5>{
             viewportGridAlignToViewAction_,
             viewportGridOrientToObjectAction_,
             viewportGridOrientToSelectionAction_,
             viewportGridCenterOnSelectionAction_,
             viewportGridClearCenterAction_ })
    {
        if (action)
        {
            action->setEnabled(editingAllowed);
        }
    }

    // Action refreshes can run synchronously between native FocusIn delivery
    // and QApplication::focusChanged. Re-derive COG ownership from Qt's
    // authoritative current focus instead of reapplying a potentially stale
    // cached bit. The focusChanged/FocusIn paths remain the normal live update.
    updateCogScriptEditorFocusFromWidget(QApplication::focusWidget());

    // Contextual overlays depend on the final action, edit-mode, selection,
    // placement, and GlView ownership state. Refresh them as one unit so no
    // tool can retain only its icon or only its interaction controller.
    syncViewportContextualOverlays();
    if (!editingAllowed)
    {
        // Keep queued action/selection refreshes from re-presenting an
        // Editor-only pane after Preview has already taken ownership.
        hideEditingUiForGamePreview();
    }
    pollLightBakeSourceChanges();
    refreshWindowTitle();
    // Action refreshes are not semantic mutations.  The canonical EDIT and
    // Level hashes still detect real same-undo edits, while this event-only
    // notification keeps the recovery scheduler aware of the final tool/UI
    // state without manufacturing a new semantic serial for every repaint.
    noteRecoveryStateChanged(false);
}

} // namespace sed
