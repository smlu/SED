#include "main_window/common.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"

namespace sed
{

bool MainWindow::viewportSelectionToolActive() const noexcept
{
    return viewportTransformSelectAction_
        && viewportTransformSelectAction_->isChecked()
        && placementMode_ == PlacementMode::none
        && !viewportModificationToolActive();
}

bool MainWindow::viewportEntitySelectionInputActive() const noexcept
{
    if (placementMode_ != PlacementMode::none || viewportModificationToolActive())
    {
        return false;
    }

    // Game Preview owns a mode-independent Auto picker. It must not inherit
    // whether the Editor happened to have Select/Move/etc. checked (or even
    // an edit mode active), and it never enables transform interaction.
    if (glView_
        && glView_->renderPipeline() == GlView::RenderPipeline::GamePreview)
    {
        return true;
    }

    return std::ranges::any_of(
        std::array<QAction*, 4>{
            viewportTransformSelectAction_,
            viewportTransformMoveAction_,
            viewportTransformRotateAction_,
            viewportTransformScaleAction_ },
        [](const QAction* action)
        {
            return action && action->isChecked();
        });
}

bool MainWindow::viewportModificationToolActive() const noexcept
{
    if (placementMode_ != PlacementMode::none
        || sectorJoinPreviewActive_
        || surfaceExtrudePreviewActive_)
    {
        return true;
    }

    if (!viewportEditToolsGroup_)
    {
        return false;
    }

    return std::ranges::any_of(
        viewportEditToolsGroup_->actions(),
        [](const QAction* action)
        {
            return action && action->isCheckable() && action->isChecked();
        });
}

void MainWindow::syncViewportSelectionInputEnabled()
{
    if (glView_)
    {
        glView_->setSurfaceClickSelectionEnabled(viewportEntitySelectionInputActive());
    }
}

void MainWindow::setViewportTransformActionChecks(QAction* const activeAction)
{
    for (QAction* action : std::array<QAction*, 4>{
             viewportTransformSelectAction_,
             viewportTransformMoveAction_,
             viewportTransformRotateAction_,
             viewportTransformScaleAction_ })
    {
        if (!action)
        {
            continue;
        }
        const QSignalBlocker blocker(action);
        action->setChecked(action == activeAction);
    }
}

void MainWindow::cancelPlacementForViewportTool(QAction* const activatingAction)
{
    if (placementMode_ == PlacementMode::none)
    {
        return;
    }

    // A selected template is the Thing placement tool, not merely a palette
    // selection, so it must be cleared as placement is released. Other
    // placement families only need their common mode unwound.
    if (placementMode_ == PlacementMode::thing)
    {
        deselectThingPlacementTemplate();
    }
    else
    {
        setPlacementMode(PlacementMode::none);
    }

    // setPlacementMode() restores Select while unwinding placement, so
    // re-check the requested action afterwards.
    if (activatingAction)
    {
        // Placement teardown may briefly restore the suspended action. Publish
        // one exact final owner instead of relying on QActionGroup's cached
        // checked action while nested signals are still unwinding.
        setViewportTransformActionChecks(activatingAction);
    }
}

void MainWindow::deactivateViewportEditFunctions()
{
    if (viewportEditToolsGroup_)
    {
        for (QAction* action : viewportEditToolsGroup_->actions())
        {
            if (action && action->isCheckable() && action->isChecked())
            {
                action->setChecked(false);
            }
        }
    }

    if (surfaceExtrudePreviewActive_)
    {
        cancelSurfaceExtrudePreview();
    }
}

void MainWindow::deactivateViewportTransformForExclusiveTool()
{
    // The field tracks the last explicit transform choice. Only a visible
    // non-Select action needs to update it here; Select is recorded by its own
    // activation handler and remains the initial default. This avoids letting
    // the internal Select sentinel overwrite a suspended Move/Rotate/Scale.
    for (QAction* action : std::array<QAction*, 3>{
             viewportTransformMoveAction_,
             viewportTransformRotateAction_,
             viewportTransformScaleAction_ })
    {
        if (action && action->isChecked())
        {
            rememberedViewportTransformAction_ = action;
            break;
        }
    }

    if (glView_)
    {
        glView_->setTransformTool(GlView::TransformTool::Select);
        glView_->setTransformInteractionEnabled(false);
    }

    // Select is an implementation sentinel while another viewport function
    // owns clicks; leaving it checked would still present two active tools.
    setViewportTransformActionChecks(nullptr);

    syncViewportSelectionInputEnabled();

    syncTransformMoveOptionsOverlay();
    syncSectorJoinOptionsOverlay();
    syncThingMoveOptionsOverlay();
}

void MainWindow::restoreViewportTransformAfterExclusiveTool()
{
    if (!documentEditingAllowed())
    {
        setViewportTransformActionChecks(nullptr);
        if (glView_)
        {
            glView_->setTransformTool(GlView::TransformTool::Select);
            glView_->setTransformInteractionEnabled(false);
        }
        syncViewportSelectionInputEnabled();
        syncTransformMoveOptionsOverlay();
        syncSectorJoinOptionsOverlay();
        syncThingMoveOptionsOverlay();
        return;
    }

    if (selectionTouchesNonEditableLayer(document_.selection()))
    {
        deactivateViewportEditFunctions();
        setViewportTransformActionChecks(viewportTransformSelectAction_);
        rememberedViewportTransformAction_ = viewportTransformSelectAction_;
        if (glView_)
        {
            glView_->setTransformInteractionEnabled(false);
            glView_->setTransformTool(GlView::TransformTool::Select);
        }
        syncViewportSelectionInputEnabled();
        syncTransformMoveOptionsOverlay();
        syncSectorJoinOptionsOverlay();
        syncThingMoveOptionsOverlay();
        return;
    }

    if (!rememberedViewportTransformAction_ || viewportModificationToolActive())
    {
        syncViewportSelectionInputEnabled();
        return;
    }

    QAction* requestedAction = rememberedViewportTransformAction_;

    GlView::TransformTool restored = GlView::TransformTool::Select;
    if (requestedAction == viewportTransformMoveAction_)
    {
        restored = GlView::TransformTool::Move;
    }
    else if (requestedAction == viewportTransformRotateAction_)
    {
        restored = GlView::TransformTool::Rotate;
    }
    else if (requestedAction == viewportTransformScaleAction_)
    {
        restored = GlView::TransformTool::Scale;
    }
    if (!requestedAction || !requestedAction->isEnabled())
    {
        requestedAction = viewportTransformSelectAction_;
        restored = GlView::TransformTool::Select;
        rememberedViewportTransformAction_ = requestedAction;
    }

    setViewportTransformActionChecks(requestedAction);
    if (glView_)
    {
        glView_->setTransformTool(restored);
        glView_->setTransformInteractionEnabled(true);
    }

    syncViewportSelectionInputEnabled();
    syncTransformMoveOptionsOverlay();
    syncSectorJoinOptionsOverlay();
    syncThingMoveOptionsOverlay();
}

void MainWindow::setupViewportTransformOverlay()
{
    if (!glView_)
    {
        return;
    }

    viewportTransformOverlay_ = new QWidget(glView_);
    viewportTransformOverlay_->setAttribute(Qt::WA_StyledBackground, true);
    viewportTransformOverlay_->setObjectName("viewportTransformOverlay");

    auto* layout = new QVBoxLayout(viewportTransformOverlay_);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(5);

    viewportTransformToolGroup_ = new QActionGroup(viewportTransformOverlay_);
    viewportTransformToolGroup_->setExclusionPolicy(
        QActionGroup::ExclusionPolicy::ExclusiveOptional);

    viewportTransformSelectAction_ = new QAction("Select", viewportTransformOverlay_);
    viewportTransformSelectAction_->setObjectName("viewport.transform.select");
    viewportTransformSelectAction_->setCheckable(true);
    // No transform tool owns viewport input until the user explicitly selects
    // one (or a native .sed/.sedrecovery editor-state payload restores it).
    // The GlView transform enum defaults to Select as its neutral sentinel;
    // showing this QAction as checked here used to make a freshly-created or
    // runtime-format-loaded document look selectable while picking was still
    // disabled by resetSelectionContextState().
    viewportTransformSelectAction_->setChecked(false);
    viewportTransformSelectAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::toolSelect));
    viewportTransformSelectAction_->setToolTip(
        "<b>Select</b><br>Select level objects and geometry in the viewport.");

    viewportTransformMoveAction_ = new QAction("Move", viewportTransformOverlay_);
    viewportTransformMoveAction_->setObjectName("viewport.transform.move");
    viewportTransformMoveAction_->setCheckable(true);
    viewportTransformMoveAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::toolMove));
    viewportTransformMoveAction_->setToolTip(
        "<b>Move</b><br>Move the selected items.");

    viewportTransformRotateAction_ = new QAction("Rotate", viewportTransformOverlay_);
    viewportTransformRotateAction_->setObjectName("viewport.transform.rotate");
    viewportTransformRotateAction_->setCheckable(true);
    viewportTransformRotateAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::toolRotate));
    viewportTransformRotateAction_->setToolTip(
        "<b>Rotate</b><br>Rotate the selected items.");

    viewportTransformScaleAction_ = new QAction("Scale", viewportTransformOverlay_);
    viewportTransformScaleAction_->setObjectName("viewport.transform.scale");
    viewportTransformScaleAction_->setCheckable(true);
    viewportTransformScaleAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::toolScale));
    viewportTransformScaleAction_->setToolTip(
        "<b>Scale</b><br>Scale the selected items.");

    viewportTransformToolGroup_->addAction(viewportTransformSelectAction_);
    viewportTransformToolGroup_->addAction(viewportTransformMoveAction_);
    viewportTransformToolGroup_->addAction(viewportTransformRotateAction_);
    viewportTransformToolGroup_->addAction(viewportTransformScaleAction_);
    rememberedViewportTransformAction_ = nullptr;

    connect(viewportTransformSelectAction_, &QAction::triggered, this, [this](const bool checked)
        {
            if (!documentEditingAllowed())
            {
                return;
            }
            const bool toggleOff = !checked && !viewportModificationToolActive();
            deactivateViewportEditFunctions();
            cancelPlacementForViewportTool(viewportTransformSelectAction_);
            if (glView_)
            {
                glView_->setTransformTool(GlView::TransformTool::Select);
            }
            if (toggleOff)
            {
                setViewportTransformActionChecks(nullptr);
                rememberedViewportTransformAction_ = nullptr;
            }
            else
            {
                setViewportTransformActionChecks(viewportTransformSelectAction_);
                rememberedViewportTransformAction_ = viewportTransformSelectAction_;
            }
            if (glView_)
            {
                glView_->setTransformInteractionEnabled(
                    !toggleOff
                    && !selectionTouchesNonEditableLayer(document_.selection()));
            }
            syncViewportSelectionInputEnabled();
            syncTransformMoveOptionsOverlay();
            syncSectorJoinOptionsOverlay();
            syncThingMoveOptionsOverlay();
            noteRecoveryStateChanged(false);
        });
    connect(viewportTransformMoveAction_, &QAction::triggered, this, [this]()
        {
            if (!glView_ || !documentEditingAllowed())
            {
                return;
            }

            const bool toggleOff = !viewportModificationToolActive()
                && glView_->transformTool() == GlView::TransformTool::Move;
            deactivateViewportEditFunctions();
            cancelPlacementForViewportTool(viewportTransformMoveAction_);

            // Clicking the active tool again deselects it (returns to Select).
            if (toggleOff)
            {
                setViewportTransformActionChecks(viewportTransformSelectAction_);
                glView_->setTransformTool(GlView::TransformTool::Select);
                glView_->setTransformInteractionEnabled(
                    !selectionTouchesNonEditableLayer(document_.selection()));
                rememberedViewportTransformAction_ = viewportTransformSelectAction_;
                syncViewportSelectionInputEnabled();
                syncTransformMoveOptionsOverlay();
                syncSectorJoinOptionsOverlay();
                syncThingMoveOptionsOverlay();
                noteRecoveryStateChanged(false);
                return;
            }

            setViewportTransformActionChecks(viewportTransformMoveAction_);
            glView_->setTransformTool(GlView::TransformTool::Move);
            glView_->setTransformInteractionEnabled(
                !selectionTouchesNonEditableLayer(document_.selection()));
            rememberedViewportTransformAction_ = viewportTransformMoveAction_;
            syncViewportSelectionInputEnabled();
            syncTransformMoveOptionsOverlay();
            syncSectorJoinOptionsOverlay();
            syncThingMoveOptionsOverlay();
            noteRecoveryStateChanged(false);
        });
    connect(viewportTransformRotateAction_, &QAction::triggered, this, [this]()
        {
            if (!glView_ || !documentEditingAllowed())
            {
                return;
            }

            const bool toggleOff = !viewportModificationToolActive()
                && glView_->transformTool() == GlView::TransformTool::Rotate;
            deactivateViewportEditFunctions();
            cancelPlacementForViewportTool(viewportTransformRotateAction_);

            // Clicking the active tool again deselects it (returns to Select).
            if (toggleOff)
            {
                setViewportTransformActionChecks(viewportTransformSelectAction_);
                glView_->setTransformTool(GlView::TransformTool::Select);
                glView_->setTransformInteractionEnabled(
                    !selectionTouchesNonEditableLayer(document_.selection()));
                rememberedViewportTransformAction_ = viewportTransformSelectAction_;
                syncViewportSelectionInputEnabled();
                syncTransformMoveOptionsOverlay();
                syncSectorJoinOptionsOverlay();
                syncThingMoveOptionsOverlay();
                noteRecoveryStateChanged(false);
                return;
            }

            setViewportTransformActionChecks(viewportTransformRotateAction_);
            glView_->setTransformTool(GlView::TransformTool::Rotate);
            glView_->setTransformInteractionEnabled(
                !selectionTouchesNonEditableLayer(document_.selection()));
            rememberedViewportTransformAction_ = viewportTransformRotateAction_;
            syncViewportSelectionInputEnabled();
            syncTransformMoveOptionsOverlay();
            syncSectorJoinOptionsOverlay();
            syncThingMoveOptionsOverlay();
            noteRecoveryStateChanged(false);
        });
    connect(viewportTransformScaleAction_, &QAction::triggered, this, [this]()
        {
            if (!glView_ || !documentEditingAllowed())
            {
                return;
            }

            const bool toggleOff = !viewportModificationToolActive()
                && glView_->transformTool() == GlView::TransformTool::Scale;
            deactivateViewportEditFunctions();
            cancelPlacementForViewportTool(viewportTransformScaleAction_);

            // Clicking the active tool again deselects it (returns to Select).
            if (toggleOff)
            {
                setViewportTransformActionChecks(viewportTransformSelectAction_);
                glView_->setTransformTool(GlView::TransformTool::Select);
                glView_->setTransformInteractionEnabled(
                    !selectionTouchesNonEditableLayer(document_.selection()));
                rememberedViewportTransformAction_ = viewportTransformSelectAction_;
                syncViewportSelectionInputEnabled();
                syncTransformMoveOptionsOverlay();
                syncSectorJoinOptionsOverlay();
                syncThingMoveOptionsOverlay();
                noteRecoveryStateChanged(false);
                return;
            }

            setViewportTransformActionChecks(viewportTransformScaleAction_);
            glView_->setTransformTool(GlView::TransformTool::Scale);
            glView_->setTransformInteractionEnabled(
                !selectionTouchesNonEditableLayer(document_.selection()));
            rememberedViewportTransformAction_ = viewportTransformScaleAction_;
            syncViewportSelectionInputEnabled();
            syncTransformMoveOptionsOverlay();
            syncSectorJoinOptionsOverlay();
            syncThingMoveOptionsOverlay();
            noteRecoveryStateChanged(false);
        });

    auto makeButton = [&](QAction* action) -> QToolButton*
    {
        auto* button = new QToolButton(viewportTransformOverlay_);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setAutoRaise(true);
        button->setIconSize(QSize(28, 28));
        button->setFixedSize(QSize(44, 44));
        return button;
    };

    layout->addWidget(makeButton(viewportTransformSelectAction_), 0, Qt::AlignHCenter);
    layout->addWidget(makeButton(viewportTransformMoveAction_), 0, Qt::AlignHCenter);
    layout->addWidget(makeButton(viewportTransformRotateAction_), 0, Qt::AlignHCenter);
    layout->addWidget(makeButton(viewportTransformScaleAction_), 0, Qt::AlignHCenter);

    viewportTransformOrientationGroup_ = new QActionGroup(viewportTransformOverlay_);
    viewportTransformOrientationGroup_->setExclusive(true);

    viewportTransformOrientationLocalAction_ = new QAction("Local", viewportTransformOverlay_);
    viewportTransformOrientationLocalAction_->setCheckable(true);
    viewportTransformOrientationLocalAction_->setChecked(true);
    viewportTransformOrientationLocalAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::transformLocal));
    viewportTransformOrientationLocalAction_->setToolTip(
        "<b>Local orientation</b><br>Move, rotate, or scale along the selected items' local axes.");

    viewportTransformOrientationWorldAction_ = new QAction("World", viewportTransformOverlay_);
    viewportTransformOrientationWorldAction_->setCheckable(true);
    viewportTransformOrientationWorldAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::transformWorld));
    viewportTransformOrientationWorldAction_->setToolTip(
        "<b>World orientation</b><br>Move, rotate, or scale along the level's world axes.");

    viewportTransformOrientationGroup_->addAction(viewportTransformOrientationLocalAction_);
    viewportTransformOrientationGroup_->addAction(viewportTransformOrientationWorldAction_);

    auto* orientMenu = new ui::shortcuts::KeySequenceDisplayMenu(viewportTransformOverlay_);
    orientMenu->addAction(viewportTransformOrientationLocalAction_);
    orientMenu->addAction(viewportTransformOrientationWorldAction_);

    viewportTransformOrientationButton_ = new QToolButton(viewportTransformOverlay_);
    viewportTransformOrientationButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    viewportTransformOrientationButton_->setAutoRaise(true);
    viewportTransformOrientationButton_->setIconSize(QSize(28, 28));
    viewportTransformOrientationButton_->setFixedSize(QSize(44, 44));
    viewportTransformOrientationButton_->setPopupMode(QToolButton::InstantPopup);
    viewportTransformOrientationButton_->setMenu(orientMenu);
    viewportTransformOrientationButton_->setIcon(viewportTransformOrientationLocalAction_->icon());
    viewportTransformOrientationButton_->setToolTip(
        "<b>Transform orientation</b><br>Choose whether transforms follow local or world axes.");

    connect(viewportTransformOrientationLocalAction_, &QAction::triggered, this, [this]()
        {
            if (glView_)
            {
                glView_->setTransformOrientation(GlView::TransformOrientation::Local);
            }
        });
    connect(viewportTransformOrientationWorldAction_, &QAction::triggered, this, [this]()
        {
            if (glView_)
            {
                glView_->setTransformOrientation(GlView::TransformOrientation::World);
            }
        });

    const auto syncOrientationIcon = [this](QAction* action)
    {
        if (viewportTransformOrientationButton_ && action)
        {
            viewportTransformOrientationButton_->setIcon(action->icon());
        }
    };
    connect(viewportTransformOrientationLocalAction_, &QAction::toggled, this,
        [this, syncOrientationIcon](const bool checked)
        {
            if (checked)
            {
                syncOrientationIcon(viewportTransformOrientationLocalAction_);
            }
        });
    connect(viewportTransformOrientationWorldAction_, &QAction::toggled, this,
        [this, syncOrientationIcon](const bool checked)
        {
            if (checked)
            {
                syncOrientationIcon(viewportTransformOrientationWorldAction_);
            }
        });
    connect(viewportTransformOrientationGroup_, &QActionGroup::triggered, this,
        [syncOrientationIcon](QAction* action)
        {
            syncOrientationIcon(action);
        });

    layout->addWidget(viewportTransformOrientationButton_, 0, Qt::AlignHCenter);

    viewportTransformGridSnapAction_ = new QAction("Grid snap", viewportTransformOverlay_);
    viewportTransformGridSnapAction_->setObjectName("viewport.transform.grid_snap");
    viewportTransformGridSnapAction_->setCheckable(true);
    viewportTransformGridSnapAction_->setChecked(false);
    viewportTransformGridSnapAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::gridSnap));
    viewportTransformGridSnapAction_->setToolTip(
        "<b>Grid snap</b><br>Snap the active tool's reference point or placement input to intersections on the active grid.");

    viewportTransformGridStickAction_ = new QAction("Grid stick", viewportTransformOverlay_);
    viewportTransformGridStickAction_->setObjectName("viewport.transform.grid_stick_through");
    viewportTransformGridStickAction_->setCheckable(true);
    viewportTransformGridStickAction_->setChecked(true);
    viewportTransformGridStickAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::gridStick));
    viewportTransformGridStickAction_->setToolTip(
        "<b>Grid stick</b><br>Pause the moved selection briefly when it reaches the active grid plane; keep dragging to push through.");

    // The transform overlay is created after the initial keymap pass. Keep
    // these same toggles as the shortcut actions and restore all alternatives,
    // including an explicit empty binding, before the viewport receives input.
    const QSettings toolbarKeymapSettings = settingsStore();
    for ( QAction* const action : {
        viewportTransformGridSnapAction_, viewportTransformGridStickAction_ } )
    {
        action->setProperty("sed_keymap_expose", true);
        action->setProperty("sed_default_shortcuts", QStringList{});
        action->setShortcutContext(Qt::WidgetShortcut);
        action->setAutoRepeat(false);
        const QStringList configured = toolbarKeymapSettings.value(
            QStringLiteral("keymap/shortcuts/") + action->objectName(), QStringList{}).toStringList();
        QList<QKeySequence> shortcuts{};
        for ( const QString& text : configured )
        {
            const QKeySequence sequence(text.trimmed(), QKeySequence::PortableText);
            if ( !sequence.isEmpty() && !shortcuts.contains(sequence) )
            {
                shortcuts.push_back(sequence);
            }
        }
        action->setShortcuts(shortcuts);
        glView_->addAction(action);
    }

    connect(viewportTransformGridSnapAction_, &QAction::toggled, this, [this](const bool checked)
        {
            if (viewportTransformGridSnapAction_->property("sed.heldModifierPreview").toBool())
            {
                return;
            }
            if (glView_)
            {
                glView_->setTransformGridSnapEnabled(checked);
            }
            QSettings settings = settingsStore();
            settings.setValue("viewport/transform_grid_snap", checked);
        });
    connect(viewportTransformGridStickAction_, &QAction::toggled, this, [this](const bool checked)
        {
            if (viewportTransformGridStickAction_->property("sed.heldModifierPreview").toBool())
            {
                return;
            }
            if (glView_)
            {
                glView_->setTransformGridPlaneStickEnabled(checked);
            }
            QSettings settings = settingsStore();
            settings.setValue("viewport/transform_grid_stick_through", checked);
        });

    layout->addStretch(1);

    viewportTransformOverlay_->setMinimumWidth(62);

    viewportTransformOverlay_->setStyleSheet(
        "QWidget#viewportTransformOverlay {"
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
        "  border: 0;"
        "}"
        "QToolButton:checked {"
        "  background: rgba(120, 160, 255, 60);"
        "  border: 1px solid rgba(155, 190, 255, 105);"
        "  border-radius: 6px;"
        "}"
        "QToolButton:checked:disabled {"
        "  background: rgba(120, 160, 255, 28);"
        "  border: 1px solid rgba(155, 190, 255, 48);"
        "  border-radius: 6px;"
        "}"
        "QToolButton::menu-indicator {"
        "  image: none;"
        "  width: 0px;"
        "  height: 0px;"
        "}");

    viewportTransformOverlay_->adjustSize();
    positionViewportTransformOverlay();
    viewportTransformOverlay_->show();
}

void MainWindow::positionViewportTransformOverlay()
{
    if (!glView_ || !viewportTransformOverlay_)
    {
        return;
    }

    viewportTransformOverlay_->adjustSize();
    const int margin = 10;
    int x = margin;
    int y = margin;

    if (viewportEditModeOverlay_ && viewportEditModeOverlay_->isVisible())
    {
        viewportEditModeOverlay_->adjustSize();
        y = viewportEditModeOverlay_->geometry().bottom() + 1 + margin;
    }

    viewportTransformOverlay_->move(
        avoidThingPlacementPanelForOverlay(viewportTransformOverlay_->size(), QPoint(x, y), margin));

    // Keep tool overlay stacked below the transform overlay.
    positionViewportToolsOverlay();
}


} // namespace sed
