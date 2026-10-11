#include "main_window/common.hpp"
#include "ui/shortcuts/shortcut_context_utils.hpp"
#include "ui/shortcuts/selection_modifier_utils.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"

namespace sed
{

namespace
{
constexpr auto kSelectionHintVisibleProperty = "sedSelectionHintVisible";
constexpr auto kSelectionHintTextProperty = "sedSelectionHintText";

/**
 * Returns an action shortcut list for utility functions that intentionally do
 * not depend on QAction. Null actions map to an empty binding list.
 */
QList<QKeySequence> shortcutBindingsForAction(const QAction* action)
{
    return action ? action->shortcuts() : QList<QKeySequence>{};
}

} // namespace

MainWindow::ThingPlacementDefaultMode MainWindow::thingPlacementModeForModifiers(
    const Qt::KeyboardModifiers modifiers) const noexcept
{
    const Qt::KeyboardModifiers held = modifiers & ui::shortcuts::editorModifierMask();
    if (held == Qt::NoModifier)
    {
        return thingPlacementDefaultMode_;
    }

    if (ui::shortcuts::modifierBindingsExact(held, thingPlacementFreeModifierBindings()))
    {
        return thingPlacementDefaultMode_ == ThingPlacementDefaultMode::free
            ? ThingPlacementDefaultMode::surface
            : ThingPlacementDefaultMode::free;
    }

    if (ui::shortcuts::modifierBindingsExact(held, thingPlacementTiltModifierBindings()))
    {
        return thingPlacementDefaultMode_ == ThingPlacementDefaultMode::surfaceTilt
            ? ThingPlacementDefaultMode::surface
            : ThingPlacementDefaultMode::surfaceTilt;
    }

    return thingPlacementDefaultMode_;
}

Qt::KeyboardModifiers MainWindow::thingPlacementEffectiveModifiers(
    const Qt::KeyboardModifiers modifiers) const noexcept
{
    switch (thingPlacementModeForModifiers(modifiers))
    {
        case ThingPlacementDefaultMode::surfaceTilt:
            return Qt::ControlModifier;
        case ThingPlacementDefaultMode::free:
            return Qt::ShiftModifier;
        case ThingPlacementDefaultMode::surface:
        default:
            return Qt::NoModifier;
    }
}

ui::shortcuts::SelectionHintMode MainWindow::selectionHintModeForEditMode() const noexcept
{
    switch (editMode_)
    {
        case EditMode::sector:  return ui::shortcuts::SelectionHintMode::sector;
        case EditMode::surface: return ui::shortcuts::SelectionHintMode::surface;
        case EditMode::thing:   return ui::shortcuts::SelectionHintMode::thing;
        case EditMode::none:
        case EditMode::light:
        case EditMode::frame:
        default:                return ui::shortcuts::SelectionHintMode::none;
    }
}

void MainWindow::applySelectionModifierHint(const QString& hintText)
{
    QStatusBar* bar = statusBar();
    if (!bar)
    {
        return;
    }

    // Step 1: Snapshot only the status-bar state owned by selection hints.
    const ui::shortcuts::SelectionModifierHintStatusState currentState{
        .visible        = bar->property(kSelectionHintVisibleProperty).toBool(),
        .trackedText    = bar->property(kSelectionHintTextProperty).toString(),
        .currentMessage = bar->currentMessage(),
    };
    const ui::shortcuts::SelectionModifierHintStatusUpdate update =
        ui::shortcuts::selectionModifierHintStatusUpdate(currentState, hintText);

    // Step 2: Apply the planned UI change while preserving unrelated messages.
    switch (update.action)
    {
        case ui::shortcuts::SelectionModifierHintStatusAction::show:
            bar->showMessage(update.trackedText);
            bar->setProperty(kSelectionHintVisibleProperty, true);
            bar->setProperty(kSelectionHintTextProperty, update.trackedText);
            return;
        case ui::shortcuts::SelectionModifierHintStatusAction::hide:
            if (update.clearCurrentMessage)
            {
                bar->clearMessage();
            }
            bar->setProperty(kSelectionHintVisibleProperty, false);
            bar->setProperty(kSelectionHintTextProperty, QString{});
            return;
        case ui::shortcuts::SelectionModifierHintStatusAction::none:
            return;
    }
}

void MainWindow::clearSelectionModifierHintIfShown()
{
    applySelectionModifierHint({});
}

void MainWindow::updateSelectionModifierHintForKeyState(
    const int key,
    const Qt::KeyboardModifiers eventModifiers,
    const bool pressed,
    const bool viewportFocusContext,
    const bool viewportPointerContext)
{
    if (!statusBar())
    {
        return;
    }

    const Qt::KeyboardModifiers held =
        ui::shortcuts::modifiersWithKeyState(
            eventModifiers & ui::shortcuts::editorModifierMask(),
            key,
            pressed);

    // Cleave owns Shift while its viewport tool is active. Do not leak the
    // generic selection hint into this interaction; describe the actual snap
    // candidates instead.
    if (glView_
        && (glView_->sectorCleaveToolEnabled() || glView_->surfaceCleaveToolEnabled()))
    {
        if (!viewportFocusContext && !viewportPointerContext)
        {
            clearSelectionModifierHintIfShown();
            return;
        }
        QString cleaveHint{};
        if (glView_->cleaveStagedMode()
            && glView_->cleaveStagedEndpointEditing()
            && held.testFlag(Qt::ShiftModifier)
            && held.testFlag(Qt::ControlModifier))
        {
            cleaveHint = QStringLiteral(
                "Ctrl Shift Drag - Constrain an edited Multi-cut endpoint to the configured angle step");
        }
        else if (held.testFlag(Qt::ShiftModifier))
        {
            const bool multiCut = glView_->cleaveStagedMode();
            if (glView_->surfaceCleaveToolEnabled())
            {
                cleaveHint = multiCut
                    ? QStringLiteral("Shift Drag - Snap a point or either whole-line endpoint to vertices, edge centers, face center, or Multi-cut endpoints")
                    : QStringLiteral("Shift Click/Drag - Snap to vertices, edge centers, or face center");
            }
            else
            {
                cleaveHint = multiCut
                    ? QStringLiteral("Shift Drag - Snap a point or either whole-line endpoint to vertices, edge centers, surface centers, or Multi-cut endpoints")
                    : QStringLiteral("Shift Click/Drag - Snap to vertices, edge centers, or surface centers");
            }
        }
        applySelectionModifierHint(cleaveHint);
        return;
    }

    // Step 1: Gather the viewport/tool state that decides whether a hint should
    // be shown at all.
    const bool selectionToolActive =
        !viewportTransformSelectAction_ || viewportTransformSelectAction_->isChecked();
    const bool editToolActive =
        (placementMode_ != PlacementMode::none)
        || (viewportEditToolsGroup_ && viewportEditToolsGroup_->checkedAction() != nullptr);
    const ui::shortcuts::SelectionModifierHintEligibility eligibility{
        .viewportAvailable      = glView_ != nullptr,
        .flyNavigationActive    = glView_ && glView_->flyNavigationActive(),
        .selectionToolActive    = selectionToolActive,
        .editToolActive         = editToolActive,
        .viewportFocusContext   = viewportFocusContext,
        .viewportPointerContext = viewportPointerContext,
    };

    if (!ui::shortcuts::shouldShowSelectionModifierHint(eligibility))
    {
        clearSelectionModifierHintIfShown();
        return;
    }

    // Step 2: Convert the current key event into the effective held modifier
    // state, including the modifier key that is being pressed or released.
    const ui::shortcuts::SelectionModifierHintBindingsConfig hintConfig{
        .addBindings           = selectionAddModifierBindings(),
        .removeBindings        = selectionRemoveModifierBindings(),
        .shortestPathBindings  = selectionShortestPathModifierBindings(),
        .bypassFiltersBindings = selectionBypassSurfaceFiltersModifierBindings(),
    };

    // Step 3: Render and apply the hint. Empty text clears only the tracked
    // selection hint, leaving unrelated status messages alone.
    const QString hint = ui::shortcuts::selectionModifierHintText(
        selectionHintModeForEditMode(),
        held,
        hintConfig);
    applySelectionModifierHint(hint);
}

void MainWindow::syncSelectionModifierBindings()
{
    using ui::shortcuts::ModifierBindings;
    using ui::shortcuts::modifierBindingsFromActionShortcuts;

    selectionAddModifierBindings_ = modifierBindingsFromActionShortcuts(
        selectionAddModeAction_, Qt::ShiftModifier);
    selectionRemoveModifierBindings_ = modifierBindingsFromActionShortcuts(
        selectionRemoveModeAction_, Qt::ControlModifier);
    selectionShortestPathModifierBindings_ = modifierBindingsFromActionShortcuts(
        selectionSurfacePathModeAction_, Qt::ControlModifier | Qt::ShiftModifier);
    selectionBypassSurfaceFiltersModifierBindings_ = modifierBindingsFromActionShortcuts(
        selectionSurfaceBypassFiltersModeAction_, Qt::AltModifier);
    thingPlacementTiltModifierBindings_ = modifierBindingsFromActionShortcuts(
        thingPlacementTiltModifierAction_, Qt::ControlModifier);
    thingPlacementFreeModifierBindings_ = modifierBindingsFromActionShortcuts(
        thingPlacementFreeModifierAction_, Qt::ShiftModifier);
    transformGridSnapInvertModifierBindings_ = modifierBindingsFromActionShortcuts(
        transformGridSnapInvertModifierAction_,
        transformGridSnapInvertModifierAction_ ? Qt::NoModifier : Qt::ControlModifier);
    transformGridStickInvertModifierBindings_ = modifierBindingsFromActionShortcuts(
        transformGridStickInvertModifierAction_,
        transformGridStickInvertModifierAction_ ? Qt::NoModifier
                                                 : (Qt::ShiftModifier | Qt::AltModifier));
    sectorMoveSnapAlignInvertModifierBindings_ = modifierBindingsFromActionShortcuts(
        sectorMoveSnapAlignInvertModifierAction_,
        sectorMoveSnapAlignInvertModifierAction_ ? Qt::NoModifier : Qt::ControlModifier);
    surfaceGeometrySnapInvertModifierBindings_ = modifierBindingsFromActionShortcuts(
        surfaceGeometrySnapInvertModifierAction_,
        surfaceGeometrySnapInvertModifierAction_ ? Qt::NoModifier : Qt::ShiftModifier);
    thingMoveSurfaceStickInvertModifierBindings_ = modifierBindingsFromActionShortcuts(
        thingMoveSurfaceStickInvertModifierAction_,
        thingMoveSurfaceStickInvertModifierAction_ ? Qt::NoModifier : Qt::ShiftModifier);
    thingMoveSurfaceAlignInvertModifierBindings_ = modifierBindingsFromActionShortcuts(
        thingMoveSurfaceAlignInvertModifierAction_,
        thingMoveSurfaceAlignInvertModifierAction_ ? Qt::NoModifier
                                                   : (Qt::ControlModifier | Qt::ShiftModifier));
    thingMoveSnapInvertModifierBindings_ = modifierBindingsFromActionShortcuts(
        thingMoveSnapInvertModifierAction_,
        thingMoveSnapInvertModifierAction_ ? Qt::NoModifier : Qt::ControlModifier);
    thingMoveSnapRotateInvertModifierBindings_ = modifierBindingsFromActionShortcuts(
        thingMoveSnapRotateInvertModifierAction_,
        thingMoveSnapRotateInvertModifierAction_ ? Qt::NoModifier
                                                 : (Qt::ControlModifier | Qt::AltModifier));

    if (!glView_)
    {
        return;
    }

    glView_->setStagedCleaveTransformShortcutBindings(
        shortcutBindingsForAction(findChild<QAction*>(QStringLiteral("cleave.transform.move"))),
        shortcutBindingsForAction(findChild<QAction*>(QStringLiteral("cleave.transform.rotate"))),
        shortcutBindingsForAction(findChild<QAction*>(QStringLiteral("cleave.transform.scale"))));
    glView_->setSelectionModifierBindings(
        selectionAddModifierBindings(),
        selectionRemoveModifierBindings(),
        selectionBypassSurfaceFiltersModifierBindings());
    glView_->setFlyNavigationSpeedModifierBindings(
        shortcutBindingsForAction(flyFastAction_),
        shortcutBindingsForAction(flySlowAction_));
    const auto shortcutHint = [](const QAction* action)
    {
        QStringList labels{};
        for (const QKeySequence& shortcut : shortcutBindingsForAction(action))
        {
            labels.push_back(ui::shortcuts::keySequenceDisplayText(shortcut));
        }
        return labels.isEmpty() ? QStringLiteral("No shortcut assigned")
            : labels.join(QStringLiteral(" or "));
    };
    if (QAction* const grid = findChild<QAction*>(QStringLiteral("viewport.show_grid")))
    {
        grid->setToolTip(QStringLiteral(
            "<b>Grid</b><br>Show or hide the construction grid in the viewport.<br>%1")
            .arg(shortcutHint(grid)));
    }
    if (viewportGridAlignToViewAction_)
    {
        viewportGridAlignToViewAction_->setToolTip(QStringLiteral(
            "Align the construction grid plane to the current view.<br>%1")
            .arg(shortcutHint(viewportGridAlignToViewAction_)));
    }
    glView_->setTransformGridSnapInvertModifierBinding(transformGridSnapInvertModifierBindings());
    glView_->setTransformGridStickInvertModifierBinding(transformGridStickInvertModifierBindings());
    glView_->setSectorMoveSnapAlignInvertModifierBinding(sectorMoveSnapAlignInvertModifierBindings());
    glView_->setGeometryTransformSnapInvertModifierBinding(surfaceGeometrySnapInvertModifierBindings());
    glView_->setThingMoveSurfaceStickInvertModifierBinding(thingMoveSurfaceStickInvertModifierBindings());
    glView_->setThingMoveSurfaceAlignInvertModifierBinding(thingMoveSurfaceAlignInvertModifierBindings());
    glView_->setThingMoveSnapInvertModifierBinding(thingMoveSnapInvertModifierBindings());
    glView_->setThingMoveSnapRotateInvertModifierBinding(thingMoveSnapRotateInvertModifierBindings());

    auto holdHint = [](const ModifierBindings& bindings)
    {
        return bindings.isEmpty()
            ? QStringLiteral("No hold shortcut is assigned to temporarily switch this option.")
            : QStringLiteral("Hold %1 to temporarily switch this option.")
                  .arg(ui::shortcuts::modifierBindingsText(bindings));
    };
    if (viewportTransformGridSnapAction_)
    {
        viewportTransformGridSnapAction_->setToolTip(
            QStringLiteral("<b>Grid snap</b><br>Snap the tool's active point to intersections on the current grid. %1")
                .arg(holdHint(transformGridSnapInvertModifierBindings())));
        if (addSectorGridSnapButton_)
        {
            addSectorGridSnapButton_->setAccessibleDescription(
                viewportTransformGridSnapAction_->toolTip());
        }
        if (viewportTransformGridSnapButton_)
        {
            viewportTransformGridSnapButton_->setAccessibleDescription(viewportTransformGridSnapAction_->toolTip());
        }
        if (viewportThingMoveGridSnapButton_)
        {
            viewportThingMoveGridSnapButton_->setAccessibleDescription(viewportTransformGridSnapAction_->toolTip());
        }
    }
    if (viewportTransformGridStickAction_)
    {
        viewportTransformGridStickAction_->setToolTip(
            QStringLiteral("<b>Grid stick</b><br>Pause a moved selection briefly when it reaches the grid plane. Keep dragging to push through. %1")
                .arg(holdHint(transformGridStickInvertModifierBindings())));
        if (viewportTransformGridStickButton_)
        {
            viewportTransformGridStickButton_->setAccessibleDescription(viewportTransformGridStickAction_->toolTip());
        }
        if (viewportThingMoveGridStickButton_)
        {
            viewportThingMoveGridStickButton_->setAccessibleDescription(viewportTransformGridStickAction_->toolTip());
        }
    }
    if (viewportSectorMoveSnapAlignButton_)
    {
        viewportSectorMoveSnapAlignButton_->setToolTip(
            QStringLiteral("<b>Snap and align</b><br>Snap a moving Sector to nearby geometry and turn matching faces into alignment. %1")
                .arg(holdHint(sectorMoveSnapAlignInvertModifierBindings())));
        viewportSectorMoveSnapAlignButton_->setAccessibleDescription(viewportSectorMoveSnapAlignButton_->toolTip());
    }
    if (viewportSurfaceGeometrySnapButton_)
    {
        viewportSurfaceGeometrySnapButton_->setToolTip(
            QStringLiteral("<b>Snap to geometry</b><br>Match a moved Vertex with a nearby Vertex, or a moved Edge or Face with similarly oriented nearby geometry. %1")
                .arg(holdHint(surfaceGeometrySnapInvertModifierBindings())));
        viewportSurfaceGeometrySnapButton_->setAccessibleDescription(
            viewportSurfaceGeometrySnapButton_->toolTip());
    }
    if (viewportThingMoveStickButton_)
    {
        viewportThingMoveStickButton_->setToolTip(
            QStringLiteral("<b>Surface stick</b><br>Stop a moved Thing when its bounds touch solid level geometry. Keep dragging to push through. %1")
                .arg(holdHint(thingMoveSurfaceStickInvertModifierBindings())));
        viewportThingMoveStickButton_->setAccessibleDescription(viewportThingMoveStickButton_->toolTip());
    }
    if (viewportThingMoveSurfaceAlignButton_)
    {
        viewportThingMoveSurfaceAlignButton_->setToolTip(
            QStringLiteral("<b>Align to surface</b><br>While Surface Stick has contact, turn the Thing to sit against the touched surface. %1")
                .arg(holdHint(thingMoveSurfaceAlignInvertModifierBindings())));
        viewportThingMoveSurfaceAlignButton_->setAccessibleDescription(viewportThingMoveSurfaceAlignButton_->toolTip());
    }
    if (viewportThingMoveSnapButton_)
    {
        viewportThingMoveSnapButton_->setToolTip(
            QStringLiteral("<b>Snap to Thing</b><br>Snap a moved Thing to a nearby Thing by matching connectors or sides. %1")
                .arg(holdHint(thingMoveSnapInvertModifierBindings())));
        viewportThingMoveSnapButton_->setAccessibleDescription(viewportThingMoveSnapButton_->toolTip());
    }
    if (viewportThingMoveSnapRotateButton_)
    {
        viewportThingMoveSnapRotateButton_->setToolTip(
            QStringLiteral("<b>Rotate to snap</b><br>When two Things snap together, turn the moved Thing so matching connectors or faces line up. %1")
                .arg(holdHint(thingMoveSnapRotateInvertModifierBindings())));
        viewportThingMoveSnapRotateButton_->setAccessibleDescription(viewportThingMoveSnapRotateButton_->toolTip());
    }

    const auto placementModifierHint = [](
                                           const ModifierBindings& bindings,
                                           const QString& alternateMode)
    {
        return bindings.isEmpty()
            ? QStringLiteral("No shortcut is assigned for switching between Surface and %1.")
                  .arg(alternateMode)
            : QStringLiteral("While placing, hold %1 to switch between Surface and %2.")
                  .arg(ui::shortcuts::modifierBindingsText(bindings), alternateMode);
    };
    if (thingPlacementModeSurfaceButton_)
    {
        thingPlacementModeSurfaceButton_->setToolTip(
            QStringLiteral("<b>Surface placement</b><br>Place on the picked surface without changing the template rotation. Set this as the default mode. %1 %2")
                .arg(
                    placementModifierHint(thingPlacementTiltModifierBindings(), QStringLiteral("Tilt")),
                    placementModifierHint(thingPlacementFreeModifierBindings(), QStringLiteral("Free"))));
    }
    if (thingPlacementModeTiltButton_)
    {
        thingPlacementModeTiltButton_->setToolTip(
            QStringLiteral("<b>Align placement</b><br>Place on the picked surface and turn the Thing to sit against it. Set this as the default mode. %1")
                .arg(placementModifierHint(
                    thingPlacementTiltModifierBindings(), QStringLiteral("Tilt"))));
    }
    if (thingPlacementModeFreeButton_)
    {
        thingPlacementModeFreeButton_->setToolTip(
            QStringLiteral("<b>Free placement</b><br>Place freely inside the containing Sector. Set this as the default mode. %1")
                .arg(placementModifierHint(
                    thingPlacementFreeModifierBindings(), QStringLiteral("Free"))));
    }
}

void MainWindow::updateTransformMoveModifierPreviewForKeyState(
    const int key,
    const Qt::KeyboardModifiers eventModifiers,
    const bool pressed)
{
    if (!ui::shortcuts::isEditorModifierKey(key))
    {
        return;
    }

    const Qt::KeyboardModifiers held = ui::shortcuts::modifiersWithKeyState(
        eventModifiers & ui::shortcuts::editorModifierMask(),
        key,
        pressed);
    auto preview = [](QAbstractButton* button, const bool effective)
    {
        if (!button)
        {
            return;
        }
        const QSignalBlocker blocker(button);
        button->setChecked(effective);
    };

    const QSettings settings = settingsStore();
    const bool gridSnapDefault =
        settings.value("viewport/transform_grid_snap", false).toBool();
    const bool surfaceGeometrySnapDefault =
        settings.value("viewport/transform_geometry_snap", true).toBool();
    const ui::shortcuts::SurfaceTransformSnapModifierState surfaceSnapState =
        ui::shortcuts::surfaceTransformSnapModifierState(
            gridSnapDefault,
            surfaceGeometrySnapDefault,
            held,
            transformGridSnapInvertModifierBindings(),
            surfaceGeometrySnapInvertModifierBindings());
    const bool gridSnap = editMode_ == EditMode::surface
        ? surfaceSnapState.gridSnap
        : ui::shortcuts::modifierToggledSetting(
            gridSnapDefault, held, transformGridSnapInvertModifierBindings());
    const bool gridStick = ui::shortcuts::modifierToggledSetting(
        settings.value("viewport/transform_grid_stick_through", true).toBool(),
        held,
        transformGridStickInvertModifierBindings());
    const bool sectorAlign = ui::shortcuts::modifierToggledSetting(
        settings.value("sector_move/snap_align", true).toBool(),
        held,
        sectorMoveSnapAlignInvertModifierBindings());
    const bool surfaceGeometrySnap = surfaceSnapState.nearbyGeometrySnap;
    const bool surfaceStick = ui::shortcuts::modifierToggledSetting(
        settings.value("thing_move/surface_stick", true).toBool(),
        held,
        thingMoveSurfaceStickInvertModifierBindings());
    const bool surfaceAlign = ui::shortcuts::modifierToggledSetting(
        settings.value("thing_move/surface_align", false).toBool(),
        held,
        thingMoveSurfaceAlignInvertModifierBindings());
    const bool thingSnap = ui::shortcuts::modifierToggledSetting(
        settings.value("thing_move/snap_to_thing", true).toBool(),
        held,
        thingMoveSnapInvertModifierBindings());
    const bool thingRotate = ui::shortcuts::modifierToggledSetting(
        settings.value("thing_move/snap_rotate", true).toBool(),
        held,
        thingMoveSnapRotateInvertModifierBindings());

    auto previewAction = [](QAction* action, const bool effective)
    {
        if (!action || action->isChecked() == effective)
        {
            return;
        }
        action->setProperty("sed.heldModifierPreview", true);
        action->setChecked(effective);
        action->setProperty("sed.heldModifierPreview", false);
    };
    // Action-backed buttons mirror QAction state, so temporarily set the
    // shared action while its persistence/live-default handler is guarded.
    previewAction(viewportTransformGridSnapAction_, gridSnap);
    previewAction(viewportTransformGridStickAction_, gridStick);
    preview(viewportSectorMoveSnapAlignButton_, sectorAlign);
    preview(viewportSurfaceGeometrySnapButton_, surfaceGeometrySnap);
    if (viewportSurfaceGeometryRotateButton_)
    {
        viewportSurfaceGeometryRotateButton_->setEnabled(surfaceGeometrySnap);
    }
    preview(viewportThingMoveStickButton_, surfaceStick);
    preview(viewportThingMoveSurfaceAlignButton_, surfaceAlign);
    preview(viewportThingMoveSnapButton_, thingSnap);
    preview(viewportThingMoveSnapRotateButton_, thingRotate);

    if (viewportThingMoveSurfaceAlignButton_)
    {
        viewportThingMoveSurfaceAlignButton_->setEnabled(surfaceStick);
    }
    if (viewportThingMoveSnapRotateButton_)
    {
        viewportThingMoveSnapRotateButton_->setEnabled(thingSnap);
    }

    if (glView_
        && editMode_ == EditMode::frame
        && viewportFrameAddAction_
        && viewportFrameAddAction_->isChecked())
    {
        updateFramePlacementPreview(
            glView_->mapFromGlobal(QCursor::pos()), held);
    }
}

void MainWindow::updateSectorDrawModifierPreviewForKeyState(
    const int key,
    const Qt::KeyboardModifiers eventModifiers,
    const bool pressed,
    const bool viewportFocusContext,
    const bool viewportPointerContext)
{
    const ui::shortcuts::ModifierPreviewContext context{
        .viewportAvailable      = glView_ != nullptr,
        .viewportFocusContext   = viewportFocusContext,
        .viewportPointerContext = viewportPointerContext,
        .flyNavigationActive    = glView_ && glView_->flyNavigationActive(),
    };
    if (!ui::shortcuts::shouldUpdateSectorDrawModifierPreview(
            context,
            key,
            placementMode_ == PlacementMode::cube,
            sectorDrawStage_ != SectorDrawStage::none))
    {
        return;
    }

    // Step 1: Recompute the active editor modifiers using the key transition.
    const Qt::KeyboardModifiers drawModifiers =
        ui::shortcuts::modifiersWithKeyState(eventModifiers, key, pressed);

    // Step 2: Refresh the live sector box at the current viewport cursor.
    updateSectorDrawPreview(glView_->mapFromGlobal(QCursor::pos()), drawModifiers);
}

void MainWindow::updateThingPlacementModifierPreviewForKeyState(
    const int key,
    const Qt::KeyboardModifiers eventModifiers,
    const bool pressed,
    const bool viewportFocusContext,
    const bool viewportPointerContext)
{
    const ui::shortcuts::ModifierPreviewContext context{
        .viewportAvailable      = glView_ != nullptr,
        .viewportFocusContext   = viewportFocusContext,
        .viewportPointerContext = viewportPointerContext,
        .flyNavigationActive    = glView_ && glView_->flyNavigationActive(),
    };
    if (!ui::shortcuts::shouldUpdateThingPlacementModifierPreview(
            context,
            key,
            placementMode_ == PlacementMode::thing))
    {
        return;
    }

    // Step 1: Recompute the placement mode modifiers using the key transition.
    const Qt::KeyboardModifiers placeModifiers =
        ui::shortcuts::modifiersWithKeyState(eventModifiers, key, pressed);

    // Step 2: Refresh the placement preview and echo the resolved mode.
    updateThingPlacementPreview(glView_->mapFromGlobal(QCursor::pos()), placeModifiers);
    if (statusBar())
    {
        statusBar()->showMessage(
            QStringLiteral("Add Thing: %1.").arg(thingPlacementModeLabel(thingPlacementModeForModifiers(placeModifiers))),
            1400);
    }
}

bool MainWindow::triggerActiveCogScriptEditorSaveForKeyState(
    const int key,
    const Qt::KeyboardModifiers modifiers,
    const bool pressed,
    const bool autoRepeat,
    const bool cogEditorFocusContext)
{
    const ui::shortcuts::EditorSaveShortcutContext context{
        .keyEventAvailable     = true,
        .pressed               = pressed,
        .autoRepeat            = autoRepeat,
        .cogEditorFocusContext = cogEditorFocusContext,
    };
    if (!ui::shortcuts::shouldTriggerEditorSaveShortcut(context, key, modifiers))
    {
        return false;
    }

    return triggerActiveCogScriptEditorSave();
}

bool MainWindow::dispatchFlyNavigationShortcut(
    const int key,
    const Qt::KeyboardModifiers modifiers,
    const bool pressed)
{
    if (!glView_ || !glView_->flyNavigationActive())
    {
        flyHeldKeys_.clear();
        return false;
    }

    if (ui::shortcuts::isEditorModifierKey(key))
    {
        glView_->refreshFlyNavigationSpeedModifiers(
            ui::shortcuts::modifiersWithKeyState(modifiers, key, pressed));
    }

    // Step 1: Convert the current QAction shortcut state into plain binding
    // lists so the matching rules stay reusable and unit-tested.
    const ui::shortcuts::FlyNavigationShortcutBindings flyBindings{
        shortcutBindingsForAction(flyFastAction_),
        shortcutBindingsForAction(flyForwardAction_),
        shortcutBindingsForAction(flyBackwardAction_),
        shortcutBindingsForAction(flyLeftAction_),
        shortcutBindingsForAction(flyRightAction_),
        shortcutBindingsForAction(flyDownAction_),
        shortcutBindingsForAction(flyUpAction_),
        shortcutBindingsForAction(flySlowAction_),
        shortcutBindingsForAction(flySpeedUpAction_),
        shortcutBindingsForAction(flySpeedDownAction_),
    };

    // A release belongs to the command resolved at press, even when speed
    // modifiers changed in between. Multiple aliases can hold one direction.
    ui::shortcuts::FlyNavigationShortcut command{};
    if (const auto held = flyHeldKeys_.find(key); held != flyHeldKeys_.end())
    {
        if (pressed)
        {
            return true;
        }
        command = held->second;
        flyHeldKeys_.erase(held);
        if (std::any_of(flyHeldKeys_.begin(), flyHeldKeys_.end(),
            [command](const auto& entry) { return entry.second == command; }))
        {
            return true;
        }
    }
    else
    {
        command = ui::shortcuts::flyNavigationShortcutForKey(flyBindings, key, modifiers);
        if (!pressed || command == ui::shortcuts::FlyNavigationShortcut::none)
        {
            return ui::shortcuts::isEditorModifierKey(key);
        }
        if (!ui::shortcuts::isEditorModifierKey(key)
            && command != ui::shortcuts::FlyNavigationShortcut::speedUp
            && command != ui::shortcuts::FlyNavigationShortcut::speedDown)
        {
            flyHeldKeys_.emplace(key, command);
        }
    }
    switch (command)
    {
        case ui::shortcuts::FlyNavigationShortcut::speedUp:
            if ( pressed && flySpeedUpAction_ )
            {
                flySpeedUpAction_->trigger();
            }
            return true;
        case ui::shortcuts::FlyNavigationShortcut::speedDown:
            if ( pressed && flySpeedDownAction_ )
            {
                flySpeedDownAction_->trigger();
            }
            return true;
        case ui::shortcuts::FlyNavigationShortcut::fast:
            if (!ui::shortcuts::isEditorModifierKey(key))
            {
                glView_->setFlyNavigationFast(pressed);
            }
            return true;
        case ui::shortcuts::FlyNavigationShortcut::slow:
            if (!ui::shortcuts::isEditorModifierKey(key))
            {
                glView_->setFlyNavigationSlow(pressed);
            }
            return true;
        case ui::shortcuts::FlyNavigationShortcut::forward:
            glView_->setFlyNavigationDirection(GlView::FlyDirection::Forward, pressed);
            return true;
        case ui::shortcuts::FlyNavigationShortcut::backward:
            glView_->setFlyNavigationDirection(GlView::FlyDirection::Backward, pressed);
            return true;
        case ui::shortcuts::FlyNavigationShortcut::left:
            glView_->setFlyNavigationDirection(GlView::FlyDirection::Left, pressed);
            return true;
        case ui::shortcuts::FlyNavigationShortcut::right:
            glView_->setFlyNavigationDirection(GlView::FlyDirection::Right, pressed);
            return true;
        case ui::shortcuts::FlyNavigationShortcut::down:
            glView_->setFlyNavigationDirection(GlView::FlyDirection::Down, pressed);
            return true;
        case ui::shortcuts::FlyNavigationShortcut::up:
            glView_->setFlyNavigationDirection(GlView::FlyDirection::Up, pressed);
            return true;
        case ui::shortcuts::FlyNavigationShortcut::none:
            return false;
    }

    return false;
}

bool MainWindow::forwardUvEditorShortcutIfNeeded(
    const int key,
    const Qt::KeyboardModifiers modifiers,
    const bool autoRepeat,
    const bool uvPanelVisible,
    const bool uvFocusContext,
    const bool uvCanvasPointerContext,
    const bool textEntryFocusContext)
{
    if (!documentEditingAllowed())
    {
        return false;
    }

    static bool forwardingUvKey = false;

    // Step 1: Reuse the tested shortcut gate so visibility/focus/pointer/key
    // rules are kept outside the event-filter body.
    const ui::shortcuts::UvEditorShortcutForwardingContext context{
        .keyEventAvailable       = true,
        .autoRepeat              = autoRepeat,
        .panelVisible            = uvPanelVisible,
        .focusInsidePanel        = uvFocusContext,
        .pointerInsideCanvas     = uvCanvasPointerContext,
        .textEntryFocusContext   = textEntryFocusContext,
        .editorWidgetAvailable   = uvEditorPanelWidget_ != nullptr,
        .forwardingAlreadyActive = forwardingUvKey,
    };
    if (!ui::shortcuts::shouldForwardUvEditorShortcut(context, key, modifiers))
    {
        return false;
    }

    // Step 2: Forward once. The guard prevents synthetic/editor-generated key
    // handling from recursively re-entering this forwarding path.
    forwardingUvKey = true;
    const bool handled = dispatchUvEditorShortcut(key, modifiers);
    forwardingUvKey = false;
    return handled;
}

bool MainWindow::dispatchViewportRollShortcut(
    const int key,
    const Qt::KeyboardModifiers modifiers,
    const bool inViewport)
{
    if (!glView_)
    {
        return false;
    }

    // Step 1: Resolve Shift + keypad/arrow aliases into a viewport roll command.
    // Some Windows/Qt setups report keypad 4/5/6 as Left/Clear/Right.
    switch (ui::shortcuts::viewportRollShortcutForKey(key, modifiers, inViewport))
    {
        case ui::shortcuts::ViewportRollShortcut::left:
            // Step 2: Apply the resolved roll through the viewport camera controller.
            glView_->orbitRollLeft();
            return true;
        case ui::shortcuts::ViewportRollShortcut::right:
            glView_->orbitRollRight();
            return true;
        case ui::shortcuts::ViewportRollShortcut::reset:
            glView_->orbitRollReset();
            return true;
        case ui::shortcuts::ViewportRollShortcut::none:
            return false;
    }

    return false;
}

bool MainWindow::dispatchEditorShortcut(const int key, const Qt::KeyboardModifiers modifiers)
{
    if (!glView_ || glView_->flyNavigationActive())
    {
        return false;
    }

    auto matches = [key, modifiers](const QAction* action) noexcept
    {
        return ui::shortcuts::actionMatchesShortcut(action, key, modifiers, false);
    };

    auto triggerEnabled = [](QAction* action) -> bool
    {
        if (!action || !action->isEnabled())
        {
            return false;
        }
        action->trigger();
        return true;
    };

    if ( documentEditingAllowed() && glView_->isStagedCleaveInteractionActive() )
    {
        if ( const auto mode = glView_->stagedCleaveTransformShortcutForKey(key, modifiers) )
        {
            glView_->setStagedCleaveTransformMode(*mode);
            return true;
        }
        // A cleared or reassigned local binding must not expose a conflicting
        // global tool or grid command while Cleave owns the viewport.
        if ( matches(thingToolSelectKeyAction_) || matches(thingToolMoveKeyAction_)
            || matches(thingToolRotateKeyAction_) || matches(thingToolScaleKeyAction_)
            || matches(findChild<QAction*>(QStringLiteral("viewport.show_grid"))) )
        {
            return true;
        }
    }

    // G newly defaults to Grid. An existing explicit G Move binding keeps
    // its command priority; the stored global Move default remains T.
    const bool customMoveOwnsGridDefault = documentEditingAllowed()
        && key == Qt::Key_G && modifiers == Qt::NoModifier
        && matches(thingToolMoveKeyAction_);
    if ( QAction* const grid = findChild<QAction*>(QStringLiteral("viewport.show_grid"));
        matches(grid) && !customMoveOwnsGridDefault )
    {
        return triggerEnabled(grid);
    }

    // Keep Escape on the same deterministic viewport shortcut path as the
    // other editor keys. This matters for placement started from the ARCH
    // tool: an actual key event must cancel it before any model click route
    // can run, while the QAction remains the single owner of the cleanup.
    if (QAction* cancel = viewportCancelAction_; matches(cancel))
    {
        return triggerEnabled(cancel);
    }

    // Light Add is one canonical keymap command. Route it to the active
    // supported light family, and consume it when the action is disabled or
    // the active family deliberately has no placement command.
    if ( editMode_ == EditMode::light )
    {
        if ( matches(addLightAction_) )
        {
            if ( documentEditingAllowed() )
            {
                switch ( lightEditSubMode_ )
                {
                    case LightEditSubMode::baked:
                        (void)triggerEnabled(addLightAction_);
                        break;
                    case LightEditSubMode::thing:
                        (void)triggerEnabled(addThingLightAction_);
                        break;
                    case LightEditSubMode::sector:
                        (void)triggerEnabled(addSectorLightAction_);
                        break;
                    case LightEditSubMode::architectural:
                        break;
                }
            }
            return true;
        }
    }

    if (!documentEditingAllowed())
    {
        // Copy is deliberately view-only. All mode, tool, transform, paste,
        // and UV commands below this boundary mutate Level/editor state.
        if (QAction* copyEntities =
                findChild<QAction*>(QStringLiteral("edit.copy_entities"));
            matches(copyEntities))
        {
            return triggerEnabled(copyEntities);
        }
        return false;
    }

    auto switchEditMode = [this, triggerEnabled](QAction* action, const EditMode mode) -> bool
    {
        if (!triggerEnabled(action))
        {
            setEditMode(mode);
        }
        return true;
    };

    auto activateTool = [&](QAction* action, const GlView::TransformTool tool)
    {
        if (!action || !action->isEnabled())
        {
            return;
        }
        deactivateViewportEditFunctions();
        cancelPlacementForViewportTool(action);
        setViewportTransformActionChecks(action);
        glView_->setTransformTool(tool);
        glView_->setTransformInteractionEnabled(
            action && action->isEnabled()
            && !selectionTouchesNonEditableLayer(document_.selection()));
        rememberedViewportTransformAction_ = action;
        syncViewportSelectionInputEnabled();
        syncTransformMoveOptionsOverlay();
        syncSectorJoinOptionsOverlay();
        syncThingMoveOptionsOverlay();
    };

    // Step 0: entity clipboard commands are real keymap actions, but they are
    // dispatched here so Ctrl+C/Ctrl+V remain viewport-only and never steal
    // native editing from a focused text widget.
    if (QAction* copyEntities = findChild<QAction*>(QStringLiteral("edit.copy_entities"));
        matches(copyEntities))
    {
        return triggerEnabled(copyEntities);
    }
    if (QAction* pasteEntities = findChild<QAction*>(QStringLiteral("edit.paste_entities"));
        matches(pasteEntities))
    {
        return triggerEnabled(pasteEntities);
    }
    // Force Delete owns its full Shift+Delete chord before ordinary Delete.
    // Keeping the two commands in this explicit order avoids platform
    // standard-key aliasing and guarantees the warned path is used.
    if (matches(forceDeleteSurfaceVertexAction_))
    {
        return triggerEnabled(forceDeleteSurfaceVertexAction_);
    }
    if (matches(deleteAction_))
    {
        const bool triggered = triggerEnabled(deleteAction_);
        // The canonical Del binding remains owned by Light mode when its
        // delete action is disabled (for example, on a read-only selection).
        return editMode_ == EditMode::light || triggered;
    }

    // Step 1: Edit-mode keys are mode agnostic and may cancel any active
    // placement through the normal setEditMode path.
    if (matches(editModeSectorKeyAction_))
    {
        return switchEditMode(editSectorAction_, EditMode::sector);
    }
    if (matches(editModeSurfaceKeyAction_))
    {
        return switchEditMode(editSurfaceAction_, EditMode::surface);
    }
    if (matches(editModeThingKeyAction_))
    {
        return switchEditMode(editThingAction_, EditMode::thing);
    }
    if (matches(editModeLightKeyAction_))
    {
        return switchEditMode(editLightAction_, EditMode::light);
    }

    // Step 2: Add-mode shortcuts can start or modify placement in their owning
    // edit modes. Shift+A cycles the sector shape list even when not already
    // placing a sector.
    if (editMode_ == EditMode::sector)
    {
        if (matches(sectorCycleAddModeKeyAction_))
        {
            cycleSelectedSectorShape();
            return true;
        }
        if (matches(addCubeAction_))
        {
            return triggerEnabled(addCubeAction_);
        }
    }
    if ( editMode_ == EditMode::thing && matches(addThingAction_) )
    {
        return triggerEnabled(addThingAction_);
    }

    const bool transformMayCancelThingPlacement = placementMode_ == PlacementMode::thing;
    if (placementMode_ != PlacementMode::none && !transformMayCancelThingPlacement)
    {
        return false;
    }

    // Step 3: Transform tool shortcuts are editor-wide. They use the same
    // priority resolver that keeps configured tool shortcuts deterministic.
    const ui::shortcuts::ThingEditShortcutBindings toolBindings{
        shortcutBindingsForAction(thingToolSelectKeyAction_),
        shortcutBindingsForAction(thingToolMoveKeyAction_),
        shortcutBindingsForAction(thingToolRotateKeyAction_),
        shortcutBindingsForAction(thingToolScaleKeyAction_),
        shortcutBindingsForAction(thingToolToggleOrientationKeyAction_),
    };
    switch (ui::shortcuts::thingEditShortcutForKey(toolBindings, key, modifiers))
    {
        case ui::shortcuts::ThingEditShortcut::select:
            activateTool(viewportTransformSelectAction_, GlView::TransformTool::Select);
            return true;
        case ui::shortcuts::ThingEditShortcut::move:
            activateTool(viewportTransformMoveAction_, GlView::TransformTool::Move);
            return true;
        case ui::shortcuts::ThingEditShortcut::rotate:
            activateTool(viewportTransformRotateAction_, GlView::TransformTool::Rotate);
            return true;
        case ui::shortcuts::ThingEditShortcut::scale:
            activateTool(viewportTransformScaleAction_, GlView::TransformTool::Scale);
            return true;
        case ui::shortcuts::ThingEditShortcut::toggleOrientation:
        {
            const bool currentlyLocal =
                glView_->transformOrientation() == GlView::TransformOrientation::Local;
            QAction* nextAction = currentlyLocal
                ? viewportTransformOrientationWorldAction_
                : viewportTransformOrientationLocalAction_;
            if (nextAction)
            {
                nextAction->trigger();
            }
            else
            {
                glView_->setTransformOrientation(
                    currentlyLocal
                        ? GlView::TransformOrientation::World
                        : GlView::TransformOrientation::Local);
            }
            return true;
        }
        case ui::shortcuts::ThingEditShortcut::none:
            break;
    }

    // Placement continues to own all non-transform keys. A matching transform
    // shortcut above cancels it through the same path as clicking the toolbar.
    if (transformMayCancelThingPlacement)
    {
        return false;
    }

    // Step 4: Mode-specific viewport commands intentionally reuse bare letters
    // because only one edit mode owns the command at a time.
    if (editMode_ == EditMode::sector)
    {
        if (matches(sectorCleaveKeyAction_))
        {
            return triggerEnabled(viewportSectorCleaveAction_);
        }
        if (matches(sectorJoinKeyAction_))
        {
            return triggerEnabled(viewportSectorConnectAction_);
        }
        if (matches(sectorMergeKeyAction_))
        {
            return triggerEnabled(viewportSectorMergeAction_);
        }
    }
    else if (editMode_ == EditMode::surface)
    {
        if (matches(surfaceCycleSubmodeKeyAction_))
        {
            switch (surfaceSelectionMode_)
            {
                case SurfaceSelectionMode::face:
                    setSurfaceSelectionMode(SurfaceSelectionMode::edge);
                    return true;
                case SurfaceSelectionMode::edge:
                    setSurfaceSelectionMode(SurfaceSelectionMode::vertex);
                    return true;
                case SurfaceSelectionMode::vertex:
                    setSurfaceSelectionMode(SurfaceSelectionMode::face);
                    return true;
            }
        }
        if (matches(surfaceFaceModeKeyAction_))
        {
            setSurfaceSelectionMode(SurfaceSelectionMode::face);
            return true;
        }
        if (matches(surfaceEdgeModeKeyAction_))
        {
            setSurfaceSelectionMode(SurfaceSelectionMode::edge);
            return true;
        }
        if (matches(surfaceVertexModeKeyAction_))
        {
            setSurfaceSelectionMode(SurfaceSelectionMode::vertex);
            return true;
        }
        if (matches(surfaceMakeAdjoinKeyAction_))
        {
            return triggerEnabled(makeAdjoinAction_);
        }
        if (matches(surfaceUnadjoinKeyAction_))
        {
            return triggerEnabled(unAdjoinAction_);
        }
        if (matches(surfaceCleaveKeyAction_))
        {
            return triggerEnabled(viewportSurfaceCleaveAction_);
        }
        if (matches(surfaceMergeKeyAction_))
        {
            return triggerEnabled(viewportSurfaceMergeAction_);
        }
        if (matches(surfaceExtrudeKeyAction_))
        {
            return triggerEnabled(extrudeSurfaceAction_);
        }
        if (matches(surfaceConnectKeyAction_))
        {
            return triggerEnabled(connectSurfacesAction_);
        }
    }
    else if (editMode_ == EditMode::thing || editMode_ == EditMode::frame)
    {
        if (matches(thingToggleFrameModeKeyAction_))
        {
            return triggerEnabled(editFrameAction_);
        }
    }
    else if ( editMode_ == EditMode::light )
    {
        // These cached actions are rebindable storage, owned for the lifetime
        // of MainWindow; no widget-tree walk occurs on the input path.
        if ( matches(property("light.submode.baked").value<QAction*>()) )
        {
            return triggerEnabled(lightEditBakedAction_);
        }
        if ( matches(property("light.submode.thing").value<QAction*>()) )
        {
            return triggerEnabled(lightEditThingAction_);
        }
        if ( document_.level().kind == ProjectType::IJIM
            && matches(property("light.submode.sector").value<QAction*>()) )
        {
            return triggerEnabled(lightEditSectorAction_);
        }
        if ( document_.level().kind == ProjectType::MOTS
            && matches(property("light.submode.architectural").value<QAction*>()) )
        {
            return triggerEnabled(lightEditArchitecturalAction_);
        }
    }

    return false;
}

void MainWindow::initializeKeymapBindings()
{
    const std::array<const char*, 3> cleaveIds{
        "cleave.transform.move", "cleave.transform.rotate", "cleave.transform.scale"};
    const std::array<QString, 3> cleaveLabels{
        QStringLiteral("Cleave Move"), QStringLiteral("Cleave Rotate"), QStringLiteral("Cleave Scale")};
    const std::array<int, 3> cleaveKeys{Qt::Key_G, Qt::Key_R, Qt::Key_S};
    const std::array<GlView::StagedCleaveTransformMode, 3> cleaveModes{
        GlView::StagedCleaveTransformMode::Move,
        GlView::StagedCleaveTransformMode::Rotate,
        GlView::StagedCleaveTransformMode::Scale};
    for ( std::size_t index = 0; index < cleaveIds.size(); ++index )
    {
        if ( findChild<QAction*>(QString::fromLatin1(cleaveIds[index])) ) continue;
        auto* action = new QAction(cleaveLabels[index], this);
        action->setObjectName(QString::fromLatin1(cleaveIds[index]));
        action->setProperty("sed_keymap_expose", true);
        action->setShortcut(QKeySequence(cleaveKeys[index]));
        action->setShortcutContext(Qt::WidgetShortcut);
        action->setToolTip(QStringLiteral(
            "Choose %1 for the active Cleave Multi-cut draft while the viewport owns input. "
            "Text fields, the UV canvas, and Fly navigation retain their input.").arg(cleaveLabels[index]));
        // These actions expose settings. The contextual key dispatcher owns
        // activation; attaching them to MainWindow would create global races.
        connect(action, &QAction::triggered, this, [this, mode = cleaveModes[index]]()
        {
            if ( documentEditingAllowed() && glView_ && glView_->isStagedCleaveInteractionActive()
                && !glView_->flyNavigationActive() )
            {
                glView_->setStagedCleaveTransformMode(mode);
            }
        });
    }

    auto makeAutoKeymapBase = [](const QAction* action) -> std::string
    {
        if (!action)
        {
            return "action";
        }

        QString seed = action->text().trimmed();
        seed.remove('&');
        if (seed.isEmpty())
        {
            seed = action->toolTip().trimmed();
        }
        if (seed.isEmpty())
        {
            seed = action->statusTip().trimmed();
        }
        if (seed.isEmpty())
        {
            seed = action->whatsThis().trimmed();
        }
        if (seed.isEmpty())
        {
            seed = QString::fromLatin1(action->metaObject()->className());
        }
        seed = seed.toLower();

        std::string out{};
        out.reserve(static_cast<std::size_t>(seed.size()));
        bool lastUnderscore = false;
        for (const QChar ch : seed)
        {
            const ushort code = ch.unicode();
            const bool isAlphaNum = (code >= 'a' && code <= 'z') || (code >= '0' && code <= '9');
            if (isAlphaNum)
            {
                out.push_back(static_cast<char>(code));
                lastUnderscore = false;
            }
            else if (!lastUnderscore)
            {
                out.push_back('_');
                lastUnderscore = true;
            }
        }

        while (!out.empty() && out.front() == '_')
        {
            out.erase(out.begin());
        }
        while (!out.empty() && out.back() == '_')
        {
            out.pop_back();
        }

        if (out.empty())
        {
            out = "action";
        }
        return out;
    };

    std::unordered_map<std::string, int> autoKeymapCounts{};
    auto keymapIdForAction = [&autoKeymapCounts, makeAutoKeymapBase](QAction* action) -> QString
    {
        if (!action)
        {
            return {};
        }

        const QString explicitId = action->objectName().trimmed();
        if (!explicitId.isEmpty())
        {
            return explicitId;
        }

        const QString cachedAutoId = action->property("sed_generated_keymap_id").toString().trimmed();
        if (!cachedAutoId.isEmpty())
        {
            return cachedAutoId;
        }

        const std::string base = makeAutoKeymapBase(action);
        int& ordinal = autoKeymapCounts[base];
        ++ordinal;

        const QString generated = QString("auto.%1.%2").arg(QString::fromStdString(base)).arg(ordinal);
        action->setProperty("sed_generated_keymap_id", generated);
        return generated;
    };

    QSettings settings = settingsStore();
    settings.beginGroup("keymap/shortcuts");

    constexpr int kEditModeNumberShortcutLayoutVersion = 1;
    if ( settings.value("_edit_mode_number_shortcut_layout_version", 0).toInt()
        < kEditModeNumberShortcutLayoutVersion )
    {
        const auto migrateDefault = [&settings](
                                        const char* id,
                                        const QList<QKeySequence>& former,
                                        const QList<QKeySequence>& current)
        {
            if ( !settings.contains(id)
                || !ui::shortcuts::shortcutSettingMatchesFormerDefault(
                    settings.value(id).toStringList(), former) )
            {
                return;
            }
            QStringList values{};
            for ( const QKeySequence& sequence : current )
            {
                values.push_back(sequence.toString(QKeySequence::PortableText));
            }
            settings.setValue(id, values);
        };
        const std::array<const char*, 4> ids{
            "edit_mode.sector", "edit_mode.surface", "edit_mode.thing", "edit_mode.light" };
        for ( std::size_t index = 0; index < ids.size(); ++index )
        {
            const int key = Qt::Key_1 + static_cast<int>(index);
            migrateDefault(ids[index], { QKeySequence(int(Qt::ShiftModifier) | key) },
                { QKeySequence(key) });
        }
        migrateDefault("surface.submode.face", { QKeySequence(Qt::SHIFT | Qt::Key_F) },
            { QKeySequence(Qt::SHIFT | Qt::Key_1), QKeySequence(Qt::SHIFT | Qt::Key_F) });
        migrateDefault("surface.submode.edge", { QKeySequence(Qt::SHIFT | Qt::Key_E) },
            { QKeySequence(Qt::SHIFT | Qt::Key_2), QKeySequence(Qt::SHIFT | Qt::Key_E) });
        migrateDefault("surface.submode.vertex", { QKeySequence(Qt::SHIFT | Qt::Key_V) },
            { QKeySequence(Qt::SHIFT | Qt::Key_3), QKeySequence(Qt::SHIFT | Qt::Key_V) });
        migrateDefault("thing.command.toggle_frame_mode", { QKeySequence(Qt::SHIFT | Qt::Key_Tab) },
            { QKeySequence(Qt::SHIFT | Qt::Key_1), QKeySequence(Qt::SHIFT | Qt::Key_Tab) });
        settings.setValue("_edit_mode_number_shortcut_layout_version", kEditModeNumberShortcutLayoutVersion);
    }

    // Restore the original JED grid-alignment pair (F2 View to Grid, F3 Grid
    // to View) without taking a binding away from anyone who customized it.
    // Find Next formerly occupied F3 in this editor and moves to Ctrl+G only
    // when its persisted value is exactly that former default.
    constexpr int kGridAlignmentShortcutLayoutVersion = 1;
    if (settings.value("_grid_alignment_shortcut_layout_version", 0).toInt()
        < kGridAlignmentShortcutLayoutVersion)
    {
        const auto migrateFormerDefault =
            [&settings](
                const char* actionId,
                const QList<QKeySequence>& former,
                const QKeySequence& current)
        {
            if (!settings.contains(actionId))
            {
                return;
            }

            if (!ui::shortcuts::shortcutSettingMatchesFormerDefault(
                    settings.value(actionId).toStringList(), former))
            {
                return;
            }

            settings.setValue(
                actionId,
                QStringList{
                    current.toString(QKeySequence::PortableText) });
        };

        migrateFormerDefault(
            "viewport.grid_align_to_view",
            { QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_G) },
            QKeySequence(Qt::Key_F3));
        migrateFormerDefault(
            "edit.find_next",
            { QKeySequence(QKeySequence::FindNext),
              QKeySequence(Qt::Key_F3) },
            QKeySequence(Qt::CTRL | Qt::Key_G));
        settings.setValue(
            "_grid_alignment_shortcut_layout_version",
            kGridAlignmentShortcutLayoutVersion);
    }

    constexpr int kCleaveGridShortcutLayoutVersion = 1;
    if ( settings.value("_cleave_grid_shortcut_layout_version", 0).toInt()
        < kCleaveGridShortcutLayoutVersion )
    {
        const auto migrateDefault = [&settings](
                                        const char* id,
                                        const QList<QKeySequence>& former,
                                        const QList<QKeySequence>& current)
        {
            if ( !settings.contains(id)
                || !ui::shortcuts::shortcutSettingMatchesFormerDefault(
                    settings.value(id).toStringList(), former) )
            {
                return;
            }
            QStringList values{};
            for ( const QKeySequence& sequence : current )
            {
                values.push_back(sequence.toString(QKeySequence::PortableText));
            }
            settings.setValue(id, values);
        };
        migrateDefault("viewport.grid_always_behind",
            {QKeySequence(Qt::KeypadModifier | Qt::Key_0), QKeySequence(Qt::Key_Insert)},
            {QKeySequence(Qt::Key_Insert)});
        migrateDefault("viewport.grid_align_to_view", {QKeySequence(Qt::Key_F3)},
            {QKeySequence(Qt::Key_F3), QKeySequence(Qt::KeypadModifier | Qt::Key_0)});
        settings.setValue("_cleave_grid_shortcut_layout_version", kCleaveGridShortcutLayoutVersion);
    }

    constexpr int kLightAddShortcutLayoutVersion = 2;
    if ( settings.value("_light_add_shortcut_layout_version", 0).toInt()
        < kLightAddShortcutLayoutVersion )
    {
        const QList<QList<QKeySequence>> formerLightAddDefaults{
            { QKeySequence(Qt::Key_L) },
            { QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L) },
        };
        const QList<QList<QKeySequence>> formerSectorAddDefaults{
            { QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C) },
        };
        const QList<QList<QKeySequence>> formerThingAddDefaults{
            { QKeySequence(Qt::CTRL | Qt::Key_A) },
        };
        const auto matchesFormerVersion = [](
                                            const QStringList& configured,
                                            const QList<QList<QKeySequence>>& versions)
        {
            return std::any_of(
                versions.cbegin(), versions.cend(),
                [&configured](const QList<QKeySequence>& former)
                {
                    return ui::shortcuts::shortcutSettingMatchesFormerDefault(
                        configured, former);
                });
        };
        const auto migrateAddCommand = [
            &settings, &matchesFormerVersion](
                                           const char* actionId,
                                           const QList<QList<QKeySequence>>& formerDefaults,
                                           const char* legacyActionId)
        {
            const bool canonicalWasAbsent = !settings.contains(actionId);
            const bool canonicalWasFormerDefault = !canonicalWasAbsent
                && matchesFormerVersion(
                    settings.value(actionId).toStringList(), formerDefaults);
            if ( canonicalWasFormerDefault )
            {
                settings.setValue(actionId, QStringList{ QStringLiteral("A") });
            }

            if ( legacyActionId && settings.contains(legacyActionId) )
            {
                if ( canonicalWasAbsent || canonicalWasFormerDefault )
                {
                    settings.setValue(
                        actionId, settings.value(legacyActionId).toStringList());
                }
                settings.remove(legacyActionId);
            }
        };

        migrateAddCommand(
            "edit.add_light", formerLightAddDefaults, "light.command.add");
        migrateAddCommand(
            "edit.add_cube", formerSectorAddDefaults, "sector.command.add");
        migrateAddCommand("edit.add_thing", formerThingAddDefaults, nullptr);
        settings.setValue(
            "_light_add_shortcut_layout_version",
            kLightAddShortcutLayoutVersion);
    }

    // Split the former shared snap/free modifiers without discarding a user's
    // existing custom bindings. Each migrated setting now controls only the
    // behavior its old command most closely represented.
    constexpr auto kTransformInvertSnapId = "viewport.transform.modifier.invert_snap";
    if (!settings.contains(kTransformInvertSnapId))
    {
        constexpr auto kLegacyGridSnapId = "transform.modifier.toggle_grid_snap";
        if (settings.contains(kLegacyGridSnapId))
        {
            settings.setValue(kTransformInvertSnapId, settings.value(kLegacyGridSnapId));
        }
    }
    auto migrateModifier = [&settings](const char* nextId, const char* legacyId)
    {
        if (!settings.contains(nextId) && settings.contains(legacyId))
        {
            settings.setValue(nextId, settings.value(legacyId));
        }
    };
    migrateModifier("sector_move.modifier.invert_snap_align", "sector_move.modifier.toggle_snap_align");
    migrateModifier("thing_move.modifier.invert_surface_stick", "thing_move.modifier.free");
    migrateModifier("thing_move.modifier.invert_snap_to_thing", "thing_move.modifier.no_snap");

    // Ctrl is the shared snap-family inverter: it temporarily swaps Grid Snap
    // against Sector Snap Align or Thing Snap. Shift keeps its established
    // Thing Surface Stick meaning. Migrate only untouched former defaults so
    // every custom binding remains intact.
    constexpr int kTransformModifierLayoutVersion = 3;
    if (settings.value("_transform_modifier_layout_version", 0).toInt() < kTransformModifierLayoutVersion)
    {
        auto migrateFormerDefault = [&settings](const char* actionId, const char* former, const char* current)
        {
            if (!settings.contains(actionId))
            {
                return;
            }

            const QStringList configured = settings.value(actionId).toStringList();
            if (configured.size() != 1)
            {
                return;
            }

            const QString normalizedConfigured =
                QKeySequence(configured.front(), QKeySequence::PortableText).toString(QKeySequence::PortableText);
            const QString normalizedFormer =
                QKeySequence(QString::fromLatin1(former), QKeySequence::PortableText).toString(QKeySequence::PortableText);
            if (normalizedConfigured == normalizedFormer)
            {
                settings.setValue(
                    actionId,
                    QStringList{ QKeySequence(QString::fromLatin1(current), QKeySequence::PortableText)
                                     .toString(QKeySequence::PortableText) });
            }
        };

        migrateFormerDefault("viewport.transform.modifier.invert_snap", "Shift", "Ctrl");
        migrateFormerDefault("viewport.transform.modifier.invert_grid_stick", "Ctrl+Shift+Alt", "Shift+Alt");
        migrateFormerDefault("sector_move.modifier.invert_snap_align", "Alt", "Ctrl");
        migrateFormerDefault("thing_move.modifier.invert_surface_stick", "Ctrl+Shift", "Shift");
        migrateFormerDefault("thing_move.modifier.invert_surface_align", "Ctrl", "Ctrl+Shift");
        migrateFormerDefault("thing_move.modifier.invert_snap_to_thing", "Alt", "Ctrl");
        settings.setValue("_transform_modifier_layout_version", kTransformModifierLayoutVersion);
    }

    const QList<QAction*> actions = findChildren<QAction*>();
    for (QAction* action : actions)
    {
        if (!action)
        {
            continue;
        }

        const QString actionId = keymapIdForAction(action);
        if (actionId.isEmpty())
        {
            continue;
        }

        if (!action->property("sed_default_shortcuts").isValid())
        {
            QStringList defaults{};
            for (const QKeySequence& seq : action->shortcuts())
            {
                const QString portable = seq.toString(QKeySequence::PortableText).trimmed();
                if (!portable.isEmpty())
                {
                    defaults.push_back(portable);
                }
            }
            action->setProperty("sed_default_shortcuts", defaults);
        }

        QStringList configured = action->property("sed_default_shortcuts").toStringList();
        if (settings.contains(actionId))
        {
            configured = settings.value(actionId).toStringList();
        }

        QList<QKeySequence> shortcuts{};
        QSet<QString> dedup{};
        for (const QString& raw : configured)
        {
            const QString text = raw.trimmed();
            if (text.isEmpty())
            {
                continue;
            }

            const QKeySequence seq(text, QKeySequence::PortableText);
            if (seq.isEmpty())
            {
                continue;
            }
            if (action->property("sed_modifier_only_shortcut").toBool()
                && !ui::shortcuts::decodeModifierOnlyShortcut(seq).has_value())
            {
                continue;
            }

            const QString canonical = seq.toString(QKeySequence::PortableText);
            if (dedup.contains(canonical))
            {
                continue;
            }

            dedup.insert(canonical);
            shortcuts.push_back(seq);
        }

        action->setShortcuts(shortcuts);
    }

    settings.endGroup();
    syncSelectionModifierBindings();
}

} // namespace sed
