#include "main_window/common.hpp"

namespace sed
{

void MainWindow::setupViewportEditModeOverlay()
{
    if (!glView_)
    {
        return;
    }

    viewportEditModeOverlay_ = new QWidget(glView_);
    viewportEditModeOverlay_->setAttribute(Qt::WA_StyledBackground, true);
    viewportEditModeOverlay_->setObjectName("viewportEditModeOverlay");

    auto* rootLayout = new QVBoxLayout(viewportEditModeOverlay_);
    rootLayout->setContentsMargins(6, 6, 6, 6);
    rootLayout->setSpacing(4);

    auto* headerRow = new QWidget(viewportEditModeOverlay_);
    auto* headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(2);

    auto makeModeButton = [&](const QString& text, const QString& tooltip) -> QToolButton*
    {
        auto* button = new QToolButton(viewportEditModeOverlay_);
        button->setText(text);
        button->setToolTip(tooltip);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setAutoRaise(true);
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        return button;
    };

    viewportEditSectorButton_ = makeModeButton(
        "Sector", "<b>Sector mode</b><br>Select and edit whole Sectors.");
    connect(viewportEditSectorButton_, &QToolButton::clicked, this, [this]() { toggleEditMode(EditMode::sector); });

    viewportEditSurfaceButton_ = makeModeButton(
        "Surface", "<b>Surface mode</b><br>Select and edit Faces, Edges, or Vertices.");
    connect(viewportEditSurfaceButton_, &QToolButton::clicked, this, [this]() { toggleEditMode(EditMode::surface); });

    viewportEditThingButton_ = makeModeButton(
        "Thing", "<b>Thing mode</b><br>Select and edit Things in the level.");
    connect(viewportEditThingButton_, &QToolButton::clicked, this, [this]() { toggleEditMode(EditMode::thing); });

    viewportEditLightButton_ = makeModeButton(
        "Light", "<b>Light mode</b><br>Select and edit baked lights, Thing lights, and other available light types.");
    connect(viewportEditLightButton_, &QToolButton::clicked, this, [this]() { toggleEditMode(EditMode::light); });

    auto addSeparator = [&]()
    {
        auto* sep = new QFrame(viewportEditModeOverlay_);
        sep->setFrameShape(QFrame::VLine);
        sep->setFrameShadow(QFrame::Plain);
        sep->setStyleSheet("QFrame { color: rgba(255, 255, 255, 60); }");
        headerLayout->addWidget(sep);
    };

    headerLayout->addWidget(viewportEditSectorButton_);
    headerLayout->addWidget(viewportEditSurfaceButton_);
    addSeparator();
    headerLayout->addWidget(viewportEditThingButton_);
    headerLayout->addWidget(viewportEditLightButton_);

    viewportSelectionEditabilityBadge_ = new QToolButton(viewportEditModeOverlay_);
    viewportSelectionEditabilityBadge_->setObjectName("viewportSelectionEditabilityBadge");
    viewportSelectionEditabilityBadge_->setIcon(
        ui::makeViewportIcon(ui::ViewportIcon::layerLocked));
    viewportSelectionEditabilityBadge_->setIconSize(QSize(15, 15));
    viewportSelectionEditabilityBadge_->setText("Read-only");
    viewportSelectionEditabilityBadge_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    viewportSelectionEditabilityBadge_->setAutoRaise(false);
    viewportSelectionEditabilityBadge_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    viewportSelectionEditabilityBadge_->setCursor(Qt::ArrowCursor);
    viewportSelectionEditabilityBadge_->hide();
    headerLayout->addSpacing(5);
    headerLayout->addWidget(viewportSelectionEditabilityBadge_);

    rootLayout->addWidget(headerRow);

    auto makeSubButton = [&](QAction* action) -> QToolButton*
    {
        auto* button = new QToolButton(viewportEditModeOverlay_);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setAutoRaise(true);
        button->setCheckable(action ? action->isCheckable() : false);
        button->setCursor(Qt::PointingHandCursor);
        return button;
    };

    // Surface sub-modes (face/edge/vertex)
    viewportEditModeSurfacePanel_ = new QWidget(viewportEditModeOverlay_);
    viewportEditModeSurfacePanel_->setObjectName("viewportEditModeSurfacePanel");
    auto* surfaceLayout = new QHBoxLayout(viewportEditModeSurfacePanel_);
    surfaceLayout->setContentsMargins(12, 0, 0, 0);
    surfaceLayout->setSpacing(2);
    if (surfaceFaceAction_)
    {
        surfaceLayout->addWidget(makeSubButton(surfaceFaceAction_));
    }
    if (surfaceEdgeAction_)
    {
        surfaceLayout->addWidget(makeSubButton(surfaceEdgeAction_));
    }
    if (surfaceVertexAction_)
    {
        surfaceLayout->addWidget(makeSubButton(surfaceVertexAction_));
    }
    // Strict edge/vertex picking is a transform-tool option, not an edit-mode
    // sub-mode.  It is hosted in the top-center contextual options row below;
    // keeping it out of this row avoids making the control look available to
    // face selection and to modification tools.
    if (surfaceNormalsAction_)
    {
        surfaceLayout->addWidget(makeSubButton(surfaceNormalsAction_));
    }
    rootLayout->addWidget(viewportEditModeSurfacePanel_);

    // Thing sub-modes (frame)
    viewportEditModeThingPanel_ = new QWidget(viewportEditModeOverlay_);
    viewportEditModeThingPanel_->setObjectName("viewportEditModeThingPanel");
    auto* thingLayout = new QHBoxLayout(viewportEditModeThingPanel_);
    thingLayout->setContentsMargins(12, 0, 0, 0);
    thingLayout->setSpacing(2);
    if (editFrameAction_)
    {
        thingLayout->addWidget(makeSubButton(editFrameAction_));
    }
    rootLayout->addWidget(viewportEditModeThingPanel_);

    // Light families. The icon used here is also used by each viewport gizmo,
    // so the category remains recognizable between the control and the scene.
    viewportEditModeLightPanel_ = new QWidget(viewportEditModeOverlay_);
    viewportEditModeLightPanel_->setObjectName("viewportEditModeLightPanel");
    auto* lightLayout = new QHBoxLayout(viewportEditModeLightPanel_);
    lightLayout->setContentsMargins(12, 0, 0, 0);
    lightLayout->setSpacing(2);
    auto addLightSubButton = [&](QAction* action) -> QToolButton*
    {
        QToolButton* button = makeSubButton(action);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIconSize(QSize(18, 18));
        lightLayout->addWidget(button);
        return button;
    };
    if (lightEditBakedAction_)
    {
        addLightSubButton(lightEditBakedAction_);
    }
    if (lightEditThingAction_)
    {
        addLightSubButton(lightEditThingAction_);
    }
    if (lightEditArchitecturalAction_)
    {
        viewportEditLightArchitecturalButton_ =
            addLightSubButton(lightEditArchitecturalAction_);
        viewportEditLightArchitecturalButton_->setObjectName(
            "viewportEditLightArchitecturalButton");
    }
    if (lightEditSectorAction_)
    {
        viewportEditLightSectorButton_ = addLightSubButton(lightEditSectorAction_);
    }
    rootLayout->addWidget(viewportEditModeLightPanel_);

    viewportEditModeOverlay_->setStyleSheet(
        "QWidget#viewportEditModeOverlay {"
        "  background: rgba(18, 18, 22, 180);"
        "  border: 1px solid rgba(255, 255, 255, 40);"
        "  border-radius: 8px;"
        "}"
        "QToolButton {"
        "  background: transparent;"
        "  border: 0;"
        "  padding: 4px 8px;"
        "}"
        "QToolButton:hover {"
        "  background: rgba(255, 255, 255, 25);"
        "  border-radius: 6px;"
        "}"
        "QToolButton:checked {"
        "  background: rgba(255, 255, 255, 45);"
        "  border-radius: 6px;"
        "}"
        "QToolButton#viewportSelectionEditabilityBadge {"
        "  color: rgb(255, 211, 122);"
        "  background: rgba(112, 70, 12, 120);"
        "  border: 1px solid rgba(255, 190, 72, 145);"
        "  border-radius: 6px;"
        "  padding: 3px 7px 3px 5px;"
        "  font-weight: 600;"
        "}"
        "QWidget#viewportEditModeSurfacePanel, QWidget#viewportEditModeThingPanel, QWidget#viewportEditModeLightPanel {"
        "  background: rgba(0, 0, 0, 35);"
        "  border-radius: 6px;"
        "}");

    viewportEditModeOverlay_->adjustSize();
    positionViewportEditModeOverlay();
    viewportEditModeOverlay_->show();

    // Sync overlay state to current edit mode/sub-mode.
    setEditMode(editMode_);
    setSurfaceSelectionMode(surfaceSelectionMode_);
    setLightEditSubMode(lightEditSubMode_);
}

void MainWindow::syncViewportSelectionEditabilityIndicator()
{
    if (!viewportSelectionEditabilityBadge_)
    {
        return;
    }

    const editor::Selection& selection = document_.selection();
    const bool hidden = selectionTouchesHiddenLayer(selection);
    const bool locked = selectionTouchesLockedLayer(selection);
    const bool readOnly = hidden || locked;

    viewportSelectionEditabilityBadge_->setVisible(readOnly);
    if (readOnly)
    {
        viewportSelectionEditabilityBadge_->setText(
            hidden && locked ? QStringLiteral("Hidden + locked")
            : hidden ? QStringLiteral("Hidden layer")
                     : QStringLiteral("Locked layer"));
        viewportSelectionEditabilityBadge_->setToolTip(
            QStringLiteral("<b>Read-only selection</b><br>The selection is on a hidden or locked layer. You can still select and navigate, but editing tools are disabled."));
    }

    if (viewportEditModeOverlay_)
    {
        viewportEditModeOverlay_->adjustSize();
        positionViewportEditModeOverlay();
    }
}

void MainWindow::positionViewportEditModeOverlay()
{
    if (!glView_ || !viewportEditModeOverlay_)
    {
        return;
    }

    viewportEditModeOverlay_->adjustSize();

    const int margin = 10;
    viewportEditModeOverlay_->move(
        avoidThingPlacementPanelForOverlay(viewportEditModeOverlay_->size(), QPoint(margin, margin), margin));

    // Keep transform overlay stacked below the edit-mode overlay.
    positionViewportTransformOverlay();
    // Light bake is a separate top-center stack whose geometry may need to
    // respond when the viewport's other overlay rows change.
    positionLightBakeOverlay();
}


} // namespace sed
