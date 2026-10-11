#include "main_window/common.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"

namespace sed
{

void MainWindow::syncThingModelLodActionAvailability()
{
    if (!thingModelLodAction_)
    {
        return;
    }

    const ProjectType kind = document_.level().kind;
    const bool available = kind == ProjectType::JKDF2
        || kind == ProjectType::MOTS;
    thingModelLodAction_->setVisible(available);
    thingModelLodAction_->setEnabled(available);
    if (glView_)
    {
        const QSignalBlocker blocker(thingModelLodAction_);
        thingModelLodAction_->setChecked(glView_->thingModelLodEnabled());
    }
}

void MainWindow::setupViewMenu(QMenu* viewMenu)
{
    if (!viewMenu)
    {
        return;
    }

    viewMenu->setObjectName("view.menu");
    const QString activeMenuRowsStyle = QStringLiteral(
        "QMenu::item { margin: 2px 5px; padding: 6px 10px; border: 1px solid transparent; border-radius: 5px; }"
        "QMenu::item:selected { background-color: rgba(255, 255, 255, 20); }"
        "QMenu::item:checked { background-color: rgba(46, 104, 176, 165); border-color: rgba(116, 177, 244, 205); }"
        "QMenu::indicator { width: 0px; height: 0px; }");
    viewMenu->setStyleSheet(activeMenuRowsStyle);

    const auto describeAction = [this](QAction* action, const char* objectName, const char* description)
    {
        action->setObjectName(QString::fromLatin1(objectName));
        action->setToolTip(QString::fromLatin1(description));
        action->setStatusTip(QString::fromLatin1(description));
        if ( action->objectName() != QStringLiteral("viewport.render.wireframe_overlay") )
        {
            action->setProperty("sed_keymap_expose", true);
            action->setShortcutContext(Qt::WidgetShortcut);
            action->setAutoRepeat(false);
            if ( glView_ )
            {
                glView_->addAction(action);
            }
        }
    };

    // Step 1: create the viewport render-mode action groups and their toggles.
    shadingGroup_ = new QActionGroup(this);
    shadingGroup_->setExclusive(true);

    viewportVerticesAction_ = new QAction("&Vertices", shadingGroup_);
    viewportVerticesAction_->setCheckable(true);
    viewportVerticesAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::vertices));
    describeAction(viewportVerticesAction_, "viewport.render.vertices", "<b>Vertices</b><br>Show level vertices without filled surfaces.");
    connect(viewportVerticesAction_, &QAction::triggered, this, [this]()
        {
            glView_->setShowSolid(false);
            glView_->setShowTextures(false);
            glView_->setShowVertices(true);
        });

    viewportWireframeAction_ = new QAction("&Wireframe", shadingGroup_);
    viewportWireframeAction_->setCheckable(true);
    viewportWireframeAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::wireframe));
    describeAction(viewportWireframeAction_, "viewport.render.wireframe", "<b>Wireframe</b><br>Show level geometry as edges without filled surfaces.");
    connect(viewportWireframeAction_, &QAction::triggered, this, [this]()
        {
            glView_->setShowSolid(false);
            glView_->setShowTextures(false);
            glView_->setShowVertices(false);
        });

    viewportSolidAction_ = new QAction("&Solid", shadingGroup_);
    viewportSolidAction_->setCheckable(true);
    viewportSolidAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::solid));
    describeAction(viewportSolidAction_, "viewport.render.solid", "<b>Solid</b><br>Fill level surfaces without drawing their textures.");
    connect(viewportSolidAction_, &QAction::triggered, this, [this]()
        {
            glView_->setShowSolid(true);
            glView_->setShowTextures(false);
            glView_->setShowVertices(false);
        });

    viewportTexturedAction_ = new QAction("&Textured", shadingGroup_);
    viewportTexturedAction_->setCheckable(true);
    viewportTexturedAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::textured));
    describeAction(viewportTexturedAction_, "viewport.render.textured", "<b>Textured</b><br>Show level surfaces with their materials and textures.");
    connect(viewportTexturedAction_, &QAction::triggered, this, [this]()
        {
            glView_->setShowSolid(true);
            glView_->setShowTextures(true);
            glView_->setShowVertices(false);
        });

    viewportTexturedAction_->setChecked(true);

    viewportWireframeOverlayAction_ = new QAction("&Wireframe Overlay", this);
    viewportWireframeOverlayAction_->setCheckable(true);
    viewportWireframeOverlayAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::wireframeOverlay));
    describeAction(viewportWireframeOverlayAction_, "viewport.render.wireframe_overlay", "<b>Wireframe overlay</b><br>Draw level edges over filled surfaces.");
    connect(viewportWireframeOverlayAction_, &QAction::toggled, this, [this](const bool checked)
        {
            glView_->setShowWireframe(checked);
        });
    viewportWireframeOverlayAction_->setChecked(true);

    viewportGamePreviewAction_ = new QAction("&Game Preview", this);
    viewportGamePreviewAction_->setCheckable(true);
    viewportGamePreviewAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::gamePreview));
    describeAction(viewportGamePreviewAction_, "viewport.render.game_preview", "<b>Game Preview</b><br>Render the viewport with its Game Preview settings.");
    connect(viewportGamePreviewAction_, &QAction::toggled, this, [this](const bool checked)
        {
            glView_->setRenderPipeline(checked ? GlView::RenderPipeline::GamePreview : GlView::RenderPipeline::Editor);
        });
    viewportGamePreviewAction_->setChecked(false);

    viewportMuteSoundAction_ = new QAction("Mute sector sounds (Editor)", this);
    viewportMuteSoundAction_->setCheckable(true);
    viewportMuteSoundAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::sound));
    describeAction(viewportMuteSoundAction_, "viewport.audio.mute_sector_sounds", "<b>Mute Sector sounds</b><br>Mute ambient sounds assigned to Sectors in the current view.");
    connect(viewportMuteSoundAction_, &QAction::toggled, this, [this](const bool checked)
        {
            setCurrentViewportSoundMuted(checked);
        });

    lightingGroup_ = new QActionGroup(this);
    lightingGroup_->setExclusive(true);

    viewportFullbrightAction_ = new QAction("&Fully Lit", lightingGroup_);
    viewportFullbrightAction_->setCheckable(true);
    viewportFullbrightAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::lightingFullbright));
    describeAction(viewportFullbrightAction_, "viewport.lighting.fully_lit", "<b>Fully Lit</b><br>Show surfaces at full brightness.");
    connect(viewportFullbrightAction_, &QAction::triggered, this, [this]()
        {
            glView_->setLightingMode(GlView::LightingMode::FullyLit);
        });

    viewportLitAction_ = new QAction("&Lit", lightingGroup_);
    viewportLitAction_->setCheckable(true);
    viewportLitAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::lightingLit));
    describeAction(viewportLitAction_, "viewport.lighting.lit", "<b>Lit</b><br>Show baked lighting and Sector Ambient colors.");
    connect(viewportLitAction_, &QAction::triggered, this, [this]()
        {
            glView_->setLightingMode(GlView::LightingMode::Lit);
        });

    viewportDiffuseAction_ = new QAction("&Diffuse", lightingGroup_);
    viewportDiffuseAction_->setCheckable(true);
    viewportDiffuseAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::lightingDiffuse));
    describeAction(viewportDiffuseAction_, "viewport.lighting.diffuse", "<b>Diffuse</b><br>Show each surface with flat directional lighting.");
    connect(viewportDiffuseAction_, &QAction::triggered, this, [this]()
        {
            glView_->setLightingMode(GlView::LightingMode::Diffuse);
        });

    viewportGouraudAction_ = new QAction("&Gouraud", lightingGroup_);
    viewportGouraudAction_->setCheckable(true);
    viewportGouraudAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::lightingGouraud));
    describeAction(viewportGouraudAction_, "viewport.lighting.gouraud", "<b>Gouraud</b><br>Blend light smoothly across each surface.");
    connect(viewportGouraudAction_, &QAction::triggered, this, [this]()
        {
            glView_->setLightingMode(GlView::LightingMode::Gouraud);
        });

    viewportGouraudAction_->setChecked(true);

    thingLightRenderingAction_ = new QAction("&Thing Lights", this);
    thingLightRenderingAction_->setCheckable(true);
    thingLightRenderingAction_->setChecked(
        glView_ ? glView_->thingLightRenderingEnabled() : true);
    describeAction(
        thingLightRenderingAction_,
        "viewport.lighting.thing_lights",
        "<b>Thing lights</b><br>Include lights attached to Things, actor headlights, and the light each Thing casts on itself.");
    connect(thingLightRenderingAction_, &QAction::toggled, this,
        [this](const bool checked)
        {
            QSettings settings = settingsStore();
            settings.setValue("viewport/render_thing_lights", checked);
            if ( glView_ )
            {
                glView_->setThingLightRenderingEnabled(checked);
            }
        });

    sectorPointLightRenderingAction_ = new QAction("&Sector Point Lights", this);
    sectorPointLightRenderingAction_->setCheckable(true);
    sectorPointLightRenderingAction_->setChecked(
        glView_ ? glView_->sectorPointLightRenderingEnabled() : true);
    describeAction(
        sectorPointLightRenderingAction_,
        "viewport.lighting.sector_point_lights",
        "<b>Sector point lights</b><br>Include point lights attached to IJIM Sectors.");
    connect(sectorPointLightRenderingAction_, &QAction::toggled, this,
        [this](const bool checked)
        {
            QSettings settings = settingsStore();
            settings.setValue("viewport/render_sector_point_lights", checked);
            if ( glView_ )
            {
                glView_->setSectorPointLightRenderingEnabled(checked);
            }
        });

    const auto syncSectorPointLightAvailability = [this]()
    {
        if ( sectorPointLightRenderingAction_ )
        {
            const bool available = document_.level().kind == ProjectType::IJIM;
            sectorPointLightRenderingAction_->setVisible(true);
            sectorPointLightRenderingAction_->setEnabled(available);
        }
    };
    syncSectorPointLightAvailability();

    thingViewGroup_ = new QActionGroup(this);
    thingViewGroup_->setExclusive(true);

    thingRenderAction_ = new QAction("Render &Things", this);
    describeAction(thingRenderAction_, "viewport.render_things", "<b>Thing visibility</b><br>Show or hide all Things in the viewport.");
    thingRenderAction_->setCheckable(true);
    thingRenderAction_->setChecked(glView_ ? glView_->showThings() : true);
    connect(thingRenderAction_, &QAction::toggled, this, [this](const bool checked)
        {
            if (glView_)
            {
                glView_->setShowThings(checked);
            }
        });

    thingHiddenAction_ = new QAction("&Hidden Things", this);
    thingHiddenAction_->setCheckable(true);
    thingHiddenAction_->setChecked(glView_ ? glView_->showHiddenThings() : true);
    describeAction(
        thingHiddenAction_,
        "viewport.things.hidden",
        "<b>Hidden Things</b><br>Show Things that the game treats as invisible or disabled.");
    connect(thingHiddenAction_, &QAction::toggled, this, [this](const bool checked)
        {
            if (glView_)
            {
                glView_->setShowHiddenThings(checked);
            }
        });

    thingCollisionShapesAction_ = new QAction("&Collision Shapes", this);
    thingCollisionShapesAction_->setObjectName("viewport.things.collision_shapes");
    thingCollisionShapesAction_->setCheckable(true);
    thingCollisionShapesAction_->setChecked(glView_ ? glView_->showThingCollisionShapes() : false);
    describeAction(
        thingCollisionShapesAction_,
        "viewport.things.collision_shapes",
        "<b>Collision shapes</b><br>Show Thing collision shapes, including size spheres, MotS mesh spheres, and model faces. The collision-preview setting changes only their displayed size.");
    connect(thingCollisionShapesAction_, &QAction::toggled, this, [this](const bool checked)
        {
            if (glView_)
            {
                const bool preview = glView_->renderPipeline()
                    == GlView::RenderPipeline::GamePreview;
                QSettings settings = settingsStore();
                settings.setValue(
                    preview
                        ? QStringLiteral("viewport/preview_show_thing_collision_shapes")
                        : QStringLiteral("viewport/editor_show_thing_collision_shapes"),
                    checked);
                glView_->setDefaultShowThingCollisionShapes(
                    preview ? GlView::RenderPipeline::GamePreview
                            : GlView::RenderPipeline::Editor,
                    checked);
            }
        });

    thingVisualBoundsAction_ = new QAction("Visual &Bounds", this);
    thingVisualBoundsAction_->setObjectName("viewport.things.visual_bounds");
    thingVisualBoundsAction_->setCheckable(true);
    thingVisualBoundsAction_->setChecked(glView_ ? glView_->showThingVisualBounds() : false);
    describeAction(
        thingVisualBoundsAction_,
        "viewport.things.visual_bounds",
        "<b>Visual bounds</b><br>Show each Thing's visual extent as a box. 3DO boxes use highest-detail geometry even when lower detail is shown; sprite boxes do not change with the camera.");
    connect(thingVisualBoundsAction_, &QAction::toggled, this, [this](const bool checked)
        {
            if (glView_)
            {
                const bool preview = glView_->renderPipeline()
                    == GlView::RenderPipeline::GamePreview;
                QSettings settings = settingsStore();
                settings.setValue(
                    preview
                        ? QStringLiteral("viewport/preview_show_thing_visual_bounds")
                        : QStringLiteral("viewport/editor_show_thing_visual_bounds"),
                    checked);
                glView_->setDefaultShowThingVisualBounds(
                    preview ? GlView::RenderPipeline::GamePreview
                            : GlView::RenderPipeline::Editor,
                    checked);
            }
        });

    thingModelLodAction_ = new QAction("3DO Model &LOD", this);
    thingModelLodAction_->setCheckable(true);
    thingModelLodAction_->setChecked(glView_ && glView_->thingModelLodEnabled());
    describeAction(
        thingModelLodAction_,
        "viewport.things.model_lod",
        "<b>3DO model detail</b><br>Let classic 3DO Things use the level's detail distances. Arch Things stay at highest detail; collision shapes, Visual Bounds, and Arch editing also use highest detail. Turn this off to show all models at highest detail.");
    connect(thingModelLodAction_, &QAction::toggled, this,
        [this](const bool checked)
        {
            if (!glView_ || (document_.level().kind != ProjectType::JKDF2
                && document_.level().kind != ProjectType::MOTS))
            {
                return;
            }
            const GlView::RenderPipeline pipeline = glView_->renderPipeline();
            const bool preview = pipeline == GlView::RenderPipeline::GamePreview;
            QSettings settings = settingsStore();
            settings.setValue(
                preview
                    ? QStringLiteral("viewport/preview_thing_model_lod_enabled")
                    : QStringLiteral("viewport/editor_thing_model_lod_enabled"),
                checked);
            glView_->setDefaultThingModelLodEnabled(pipeline, checked);
        });
    syncThingModelLodActionAvailability();

    thingDotsAction_ = new QAction("&Dots", thingViewGroup_);
    thingDotsAction_->setCheckable(true);
    thingDotsAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::thingDots));
    describeAction(thingDotsAction_, "viewport.things.dots", "<b>Dots</b><br>Show each Thing as a point marker.");
    connect(thingDotsAction_, &QAction::triggered, this, [this]()
        {
            glView_->setThingViewMode(GlView::ThingViewMode::Dots);
        });

    thingBoxesAction_ = new QAction("&Boxes", thingViewGroup_);
    thingBoxesAction_->setCheckable(true);
    thingBoxesAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::thingBoxes));
    describeAction(thingBoxesAction_, "viewport.things.boxes", "<b>Boxes</b><br>Show each Thing as an editor box.");
    connect(thingBoxesAction_, &QAction::triggered, this, [this]()
        {
            glView_->setThingViewMode(GlView::ThingViewMode::Boxes);
        });

    thingWireframesAction_ = new QAction("&Wireframes", thingViewGroup_);
    thingWireframesAction_->setCheckable(true);
    thingWireframesAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::thingWireframes));
    describeAction(thingWireframesAction_, "viewport.things.wireframes", "<b>Wireframes</b><br>Show Thing models as edges without shaded surfaces.");
    connect(thingWireframesAction_, &QAction::triggered, this, [this]()
        {
            glView_->setThingViewMode(GlView::ThingViewMode::Wireframes);
        });

    thingShadedAction_ = new QAction("S&haded", thingViewGroup_);
    thingShadedAction_->setCheckable(true);
    thingShadedAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::thingShaded));
    describeAction(thingShadedAction_, "viewport.things.shaded", "<b>Shaded</b><br>Show Thing models with shaded materials.");
    connect(thingShadedAction_, &QAction::triggered, this, [this]()
        {
            glView_->setThingViewMode(GlView::ThingViewMode::Shaded);
        });

    thingShadedAction_->setChecked(true);

    const auto refreshThingRenderActionPresentation = [this]()
    {
        if (!thingRenderAction_)
        {
            return;
        }

        const bool visible = thingRenderAction_->isChecked();
        const QAction* mode = thingViewGroup_ ? thingViewGroup_->checkedAction() : nullptr;
        const QIcon baseIcon = mode
            ? mode->icon()
            : ui::makeViewportIcon(ui::ViewportIcon::thingShaded);
        thingRenderAction_->setText(visible ? "Render &Things: On" : "Render &Things: Off");
        thingRenderAction_->setToolTip(visible
                ? "<b>Thing visibility</b><br>Things are shown in the viewport."
                : "<b>Thing visibility</b><br>Things are hidden in the viewport.");
        thingRenderAction_->setIcon(visible
                ? baseIcon
                : ui::makeCrossedOutViewportIcon(baseIcon));
    };
    connect(thingRenderAction_, &QAction::toggled, this,
        [refreshThingRenderActionPresentation](const bool)
        {
            refreshThingRenderActionPresentation();
        });
    connect(thingViewGroup_, &QActionGroup::triggered, this,
        [refreshThingRenderActionPresentation](QAction*)
        {
            refreshThingRenderActionPresentation();
        });
    refreshThingRenderActionPresentation();

    thingGizmosAction_ = new QAction("Thing &Gizmos", this);
    thingGizmosAction_->setCheckable(true);
    thingGizmosAction_->setChecked(glView_ ? glView_->showThingGizmos() : false);
    thingGizmosAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::thingGizmos));
    describeAction(thingGizmosAction_, "viewport.things.gizmos", "<b>Thing markers</b><br>Show editor icons and direction arrows for Things.");
    connect(thingGizmosAction_, &QAction::toggled, this, [this](const bool checked)
        {
            glView_->setShowThingGizmos(checked);
        });

    thingCubesAction_ = new QAction("&Non-visual Cubes", this);
    thingCubesAction_->setCheckable(true);
    thingCubesAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::thingBoxes));
    describeAction(thingCubesAction_, "viewport.things.non_visual_cubes", "<b>Non-visual cubes</b><br>Show a cube for each Thing without a visual model.");
    thingCubesAction_->setChecked(glView_ ? (glView_->renderPipeline() == GlView::RenderPipeline::GamePreview ? glView_->showPreviewCubes() : glView_->showEditorCubes()) : true);
    connect(thingCubesAction_, &QAction::toggled, this, [this](const bool checked)
        {
            if (!glView_)
            {
                return;
            }

            if (glView_->renderPipeline() == GlView::RenderPipeline::GamePreview)
            {
                glView_->setShowPreviewCubes(checked);
            }
            else
            {
                glView_->setShowEditorCubes(checked);
            }
        });

    // Step 2: keep related state in named submenus. Checked actions use an
    // active-row treatment because a faint platform checkmark is easy to miss
    // beside the mode icons in the dark theme.
    auto* renderModesMenu = ui::shortcuts::addKeySequenceDisplayMenu(viewMenu, "&Render Mode");
    renderModesMenu->setObjectName("view.render_modes");
    renderModesMenu->setIcon(viewportTexturedAction_->icon());
    renderModesMenu->setToolTipsVisible(true);
    renderModesMenu->setStyleSheet(activeMenuRowsStyle);
    renderModesMenu->addAction(viewportVerticesAction_);
    renderModesMenu->addAction(viewportWireframeAction_);
    renderModesMenu->addAction(viewportSolidAction_);
    renderModesMenu->addAction(viewportTexturedAction_);
    renderModesMenu->addSeparator();
    renderModesMenu->addAction(viewportWireframeOverlayAction_);
    renderModesMenu->addAction(viewportGamePreviewAction_);
    connect(shadingGroup_, &QActionGroup::triggered, renderModesMenu,
        [renderModesMenu](QAction* action)
        {
            if (action)
            {
                renderModesMenu->setIcon(action->icon());
            }
        });

    auto* lightingModesMenu = ui::shortcuts::addKeySequenceDisplayMenu(viewMenu, "&Lighting");
    lightingModesMenu->setObjectName("view.lighting_modes");
    lightingModesMenu->setIcon(viewportGouraudAction_->icon());
    lightingModesMenu->setToolTipsVisible(true);
    lightingModesMenu->setStyleSheet(activeMenuRowsStyle);
    lightingModesMenu->addAction(viewportFullbrightAction_);
    lightingModesMenu->addAction(viewportLitAction_);
    lightingModesMenu->addAction(viewportDiffuseAction_);
    lightingModesMenu->addAction(viewportGouraudAction_);
    lightingModesMenu->addSeparator();
    lightingModesMenu->addAction(thingLightRenderingAction_);
    lightingModesMenu->addAction(sectorPointLightRenderingAction_);
    connect(
        lightingModesMenu,
        &QMenu::aboutToShow,
        this,
        syncSectorPointLightAvailability);
    connect(lightingGroup_, &QActionGroup::triggered, lightingModesMenu,
        [lightingModesMenu](QAction* action)
        {
            if (action)
            {
                lightingModesMenu->setIcon(action->icon());
            }
        });

    auto* thingsMenu = ui::shortcuts::addKeySequenceDisplayMenu(viewMenu, "&Things");
    thingsMenu->setObjectName("view.things");
    thingsMenu->setIcon(thingRenderAction_->icon());
    thingsMenu->setToolTipsVisible(true);
    thingsMenu->setStyleSheet(activeMenuRowsStyle);
    thingsMenu->addAction(thingRenderAction_);
    thingsMenu->addAction(thingHiddenAction_);
    thingsMenu->addAction(thingGizmosAction_);
    thingsMenu->addAction(thingCubesAction_);
    thingsMenu->addSeparator();
    thingsMenu->addAction(thingCollisionShapesAction_);
    thingsMenu->addAction(thingVisualBoundsAction_);
    thingsMenu->addAction(thingModelLodAction_);
    thingsMenu->addSeparator();
    thingsMenu->addAction(thingDotsAction_);
    thingsMenu->addAction(thingBoxesAction_);
    thingsMenu->addAction(thingWireframesAction_);
    thingsMenu->addAction(thingShadedAction_);
    const auto refreshThingsMenuIcon = [this, thingsMenu]()
    {
        if (!thingsMenu || !thingRenderAction_)
        {
            return;
        }
        thingsMenu->setIcon(thingRenderAction_->icon());
    };
    connect(thingRenderAction_, &QAction::toggled, thingsMenu,
        [refreshThingsMenuIcon](const bool) { refreshThingsMenuIcon(); });
    connect(thingViewGroup_, &QActionGroup::triggered, thingsMenu,
        [refreshThingsMenuIcon](QAction*) { refreshThingsMenuIcon(); });
    refreshThingsMenuIcon();
    connect(thingsMenu, &QMenu::aboutToShow, this,
        [this]() { syncThingModelLodActionAvailability(); });

    viewMenu->addSeparator();
    viewMenu->addAction(viewportMuteSoundAction_);
    viewMenu->addSeparator();

    // Dock windows (outliner/properties, checks, messages, etc).
    {
        auto* windowsMenu = ui::shortcuts::addKeySequenceDisplayMenu(viewMenu, "&Windows");
        windowsMenu->setObjectName("view.windows");

        QList<QDockWidget*> docks = findChildren<QDockWidget*>();
        std::sort(docks.begin(), docks.end(), [](const QDockWidget* a, const QDockWidget* b)
            {
                return a && b ? (a->windowTitle() < b->windowTitle()) : (a != nullptr);
            });

        for (QDockWidget* dock : docks)
        {
            const bool resourceEditor = std::any_of(resourceEditorTabHosts_.begin(), resourceEditorTabHosts_.end(),
                [dock](const ResourceEditorTabHost& host)
                {
                    return host.dock.data() == dock;
                });
            if ( !dock || dock == cogScriptEditorDock_ || resourceEditor
                || dock->property("sed.cogScriptEditorDock").toBool() )
            {
                continue;
            }

            QAction* toggle = dock->toggleViewAction();
            if (!toggle)
            {
                continue;
            }

            windowsMenu->addAction(toggle);
        }
    }

    setupViewportKeyBindings(viewMenu);
}

void MainWindow::setupViewportKeyBindings(QMenu* viewMenu)
{
    if (!viewMenu || !glView_)
    {
        return;
    }

    auto addViewportShortcut = [&](QAction*& action, const QString& id, const QString& text, const QList<QKeySequence>& shortcuts)
    {
        action = new QAction(text, this);
        action->setObjectName(id);
        action->setProperty("sed_keymap_expose", true);
        action->setShortcuts(shortcuts);
        action->setShortcutContext(Qt::WidgetShortcut);
        action->setAutoRepeat(true);
        glView_->addAction(action);
        return action;
    };

    auto addViewportShortcutNoRepeat = [&](QAction*& action, const QString& id, const QString& text, const QList<QKeySequence>& shortcuts)
    {
        action = new QAction(text, this);
        action->setObjectName(id);
        action->setProperty("sed_keymap_expose", true);
        action->setShortcuts(shortcuts);
        action->setShortcutContext(Qt::WidgetShortcut);
        action->setAutoRepeat(false);
        glView_->addAction(action);
        return action;
    };

    auto* toggleWireframeOverlayAction = new QAction("Toggle Wireframe Overlay", this);
    toggleWireframeOverlayAction->setObjectName("viewport.render.toggle_wireframe_overlay");
    toggleWireframeOverlayAction->setShortcut(QKeySequence(Qt::Key_Tab));
    toggleWireframeOverlayAction->setShortcutContext(Qt::WidgetShortcut);
    toggleWireframeOverlayAction->setAutoRepeat(false);
    toggleWireframeOverlayAction->setStatusTip(
        "Toggle the wireframe overlay while the viewport has focus.");
    glView_->addAction(toggleWireframeOverlayAction);
    connect(toggleWireframeOverlayAction, &QAction::triggered, this,
        [this]()
        {
            if (!viewportWireframeOverlayAction_)
            {
                return;
            }
            viewportWireframeOverlayAction_->setChecked(
                !viewportWireframeOverlayAction_->isChecked());
        });

    viewportCancelAction_ = new QAction("Cancel", this);
    viewportCancelAction_->setObjectName("viewport.cancel");
    viewportCancelAction_->setShortcut(QKeySequence(Qt::Key_Escape));
    viewportCancelAction_->setShortcutContext(Qt::WindowShortcut);
    viewportCancelAction_->setAutoRepeat(false);
    addAction(viewportCancelAction_);
    connect(viewportCancelAction_, &QAction::triggered, this, [this]()
        {
            if (cancelActiveUvEditorModalTransform())
            {
                return;
            }

            if (surfaceExtrudePreviewActive_)
            {
                cancelSurfaceExtrudePreview();
                return;
            }

            if (glView_ && glView_->cancelActiveStagedCleaveInteraction())
            {
                return;
            }

            if (glView_)
            {
                glView_->cancelFlyNavigation();
            }

            // Cancel any active viewport tool (e.g. cleave).
            if (viewportSectorCleaveAction_ && viewportSectorCleaveAction_->isChecked())
            {
                viewportSectorCleaveAction_->setChecked(false);
            }
            if (viewportSurfaceCleaveAction_ && viewportSurfaceCleaveAction_->isChecked())
            {
                viewportSurfaceCleaveAction_->setChecked(false);
            }
            if (viewportFrameAddAction_ && viewportFrameAddAction_->isChecked())
            {
                viewportFrameAddAction_->setChecked(false);
            }

            if (cogEntityPickActive_)
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
                statusBar()->showMessage("COG variable pick canceled.", 2000);
                return;
            }

            if (placementMode_ == PlacementMode::thing || !selectedThingTemplateName_.empty())
            {
                deselectThingPlacementTemplate();
                return;
            }

            // Restore normal viewport pick mode after canceling special temporary tools.
            setPlacementMode(PlacementMode::none);
        });

    auto* navMenu = ui::shortcuts::addKeySequenceDisplayMenu(viewMenu, "Navigation");

    addViewportShortcutNoRepeat(
        viewportFrameLevelAction_,
        "viewport.frame_level",
        "Frame Level",
        { QKeySequence(Qt::Key_F) });
    connect(viewportFrameLevelAction_, &QAction::triggered, this, [this]()
        {
            if (glView_)
            {
                glView_->fitViewToLevel();
            }
        });
    navMenu->addAction(viewportFrameLevelAction_);

    QAction* zoomInAction{};
    addViewportShortcut(
        zoomInAction, "viewport.zoom_in", "Zoom In",
        { QKeySequence(Qt::KeypadModifier | Qt::Key_Plus) });
    zoomInAction->setToolTip("Zoom the level viewport in one step. Wheel scrolling preferences are independent.");
    connect(zoomInAction, &QAction::triggered, this, [this]()
        {
            if ( glView_ )
            {
                glView_->zoomCameraBySteps(1.0f);
            }
        });
    navMenu->addAction(zoomInAction);

    QAction* zoomOutAction{};
    addViewportShortcut(
        zoomOutAction, "viewport.zoom_out", "Zoom Out",
        { QKeySequence(Qt::KeypadModifier | Qt::Key_Minus) });
    zoomOutAction->setToolTip("Zoom the level viewport out one step. Wheel scrolling preferences are independent.");
    connect(zoomOutAction, &QAction::triggered, this, [this]()
        {
            if ( glView_ )
            {
                glView_->zoomCameraBySteps(-1.0f);
            }
        });
    navMenu->addAction(zoomOutAction);

    addViewportShortcutNoRepeat(
        viewportToggleProjectionAction_,
        "viewport.toggle_projection",
        "Toggle Projection",
        {
            QKeySequence(Qt::KeypadModifier | Qt::Key_5),
            QKeySequence(Qt::KeypadModifier | Qt::Key_Clear),
            QKeySequence(Qt::Key_Clear),
        });

    connect(viewportToggleProjectionAction_, &QAction::triggered, this, [this]()
        {
            if (!glView_)
            {
                return;
            }

            // Only allow a plain keypad press (no Shift/Ctrl/Alt). This avoids stealing combinations like Shift+Numpad5.
            const Qt::KeyboardModifiers mods = QGuiApplication::keyboardModifiers();
            if ((mods & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) != Qt::NoModifier)
            {
                return;
            }

            const auto current = glView_->cameraProjectionType();
            const auto next =
                (current == app::ViewportCamera::ProjectionType::Perspective)
                    ? app::ViewportCamera::ProjectionType::Orthographic
                    : app::ViewportCamera::ProjectionType::Perspective;

            if (viewportCameraProjectionCombo_)
            {
                const QSignalBlocker blocker(viewportCameraProjectionCombo_);
                for (int i = 0; i < viewportCameraProjectionCombo_->count(); ++i)
                {
                    if (viewportCameraProjectionCombo_->itemData(i).toInt() == static_cast<int>(next))
                    {
                        viewportCameraProjectionCombo_->setCurrentIndex(i);
                        break;
                    }
                }
            }

            glView_->setCameraProjectionType(next);
        });
    navMenu->addAction(viewportToggleProjectionAction_);

    navMenu->addSeparator();

    addViewportShortcut(
        viewportOrbitLeftAction_,
        "viewport.orbit_left",
        "Orbit Left",
        { QKeySequence(Qt::KeypadModifier | Qt::Key_4) });
    connect(viewportOrbitLeftAction_, &QAction::triggered, this, [this]() { glView_->orbitRotateLeft(); });
    navMenu->addAction(viewportOrbitLeftAction_);

    addViewportShortcut(
        viewportOrbitRightAction_,
        "viewport.orbit_right",
        "Orbit Right",
        { QKeySequence(Qt::KeypadModifier | Qt::Key_6) });
    connect(viewportOrbitRightAction_, &QAction::triggered, this, [this]() { glView_->orbitRotateRight(); });
    navMenu->addAction(viewportOrbitRightAction_);

    addViewportShortcut(
        viewportOrbitUpAction_,
        "viewport.orbit_up",
        "Orbit Up",
        { QKeySequence(Qt::KeypadModifier | Qt::Key_8) });
    connect(viewportOrbitUpAction_, &QAction::triggered, this, [this]() { glView_->orbitRotateUp(); });
    navMenu->addAction(viewportOrbitUpAction_);

    addViewportShortcut(
        viewportOrbitDownAction_,
        "viewport.orbit_down",
        "Orbit Down",
        { QKeySequence(Qt::KeypadModifier | Qt::Key_2) });
    connect(viewportOrbitDownAction_, &QAction::triggered, this, [this]() { glView_->orbitRotateDown(); });
    navMenu->addAction(viewportOrbitDownAction_);

    addViewportShortcut(
        viewportViewOppositeAction_,
        "viewport.view_opposite",
        "View Opposite",
        { QKeySequence(Qt::KeypadModifier | Qt::Key_9) });
    connect(viewportViewOppositeAction_, &QAction::triggered, this, [this]() { glView_->orbitViewOpposite(); });
    navMenu->addAction(viewportViewOppositeAction_);

    navMenu->addSeparator();

    addViewportShortcut(
        viewportViewFrontAction_,
        "viewport.view_front",
        "View Front",
        { QKeySequence(Qt::KeypadModifier | Qt::Key_1) });
    connect(viewportViewFrontAction_, &QAction::triggered, this, [this]() { glView_->orbitViewFront(); });
    navMenu->addAction(viewportViewFrontAction_);

    addViewportShortcut(
        viewportViewBackAction_,
        "viewport.view_back",
        "View Back",
        { QKeySequence(Qt::KeypadModifier | Qt::ControlModifier | Qt::Key_1) });
    connect(viewportViewBackAction_, &QAction::triggered, this, [this]() { glView_->orbitViewBack(); });
    navMenu->addAction(viewportViewBackAction_);

    addViewportShortcut(
        viewportViewRightAction_,
        "viewport.view_right",
        "View Right",
        { QKeySequence(Qt::KeypadModifier | Qt::Key_3) });
    connect(viewportViewRightAction_, &QAction::triggered, this, [this]() { glView_->orbitViewRight(); });
    navMenu->addAction(viewportViewRightAction_);

    addViewportShortcut(
        viewportViewLeftAction_,
        "viewport.view_left",
        "View Left",
        { QKeySequence(Qt::KeypadModifier | Qt::ControlModifier | Qt::Key_3) });
    connect(viewportViewLeftAction_, &QAction::triggered, this, [this]() { glView_->orbitViewLeft(); });
    navMenu->addAction(viewportViewLeftAction_);

    addViewportShortcut(
        viewportViewTopAction_,
        "viewport.view_top",
        "View Top",
        { QKeySequence(Qt::KeypadModifier | Qt::Key_7) });
    connect(viewportViewTopAction_, &QAction::triggered, this, [this]() { glView_->orbitViewTop(); });
    navMenu->addAction(viewportViewTopAction_);

    addViewportShortcut(
        viewportViewBottomAction_,
        "viewport.view_bottom",
        "View Bottom",
        { QKeySequence(Qt::KeypadModifier | Qt::ControlModifier | Qt::Key_7) });
    connect(viewportViewBottomAction_, &QAction::triggered, this, [this]() { glView_->orbitViewBottom(); });
    navMenu->addAction(viewportViewBottomAction_);

    navMenu->addSeparator();

    addViewportShortcut(
        viewportPanLeftAction_,
        "viewport.pan_left",
        "Pan Left",
        { QKeySequence(Qt::KeypadModifier | Qt::ControlModifier | Qt::Key_4) });
    connect(viewportPanLeftAction_, &QAction::triggered, this, [this]() { glView_->orbitPanLeft(); });
    navMenu->addAction(viewportPanLeftAction_);

    addViewportShortcut(
        viewportPanRightAction_,
        "viewport.pan_right",
        "Pan Right",
        { QKeySequence(Qt::KeypadModifier | Qt::ControlModifier | Qt::Key_6) });
    connect(viewportPanRightAction_, &QAction::triggered, this, [this]() { glView_->orbitPanRight(); });
    navMenu->addAction(viewportPanRightAction_);

    addViewportShortcut(
        viewportPanUpAction_,
        "viewport.pan_up",
        "Pan Up",
        { QKeySequence(Qt::KeypadModifier | Qt::ControlModifier | Qt::Key_8) });
    connect(viewportPanUpAction_, &QAction::triggered, this, [this]() { glView_->orbitPanUp(); });
    navMenu->addAction(viewportPanUpAction_);

    addViewportShortcut(
        viewportPanDownAction_,
        "viewport.pan_down",
        "Pan Down",
        { QKeySequence(Qt::KeypadModifier | Qt::ControlModifier | Qt::Key_2) });
    connect(viewportPanDownAction_, &QAction::triggered, this, [this]() { glView_->orbitPanDown(); });
    navMenu->addAction(viewportPanDownAction_);

    navMenu->addSeparator();

    addViewportShortcut(
        viewportRollLeftAction_,
        "viewport.roll_left",
        "Roll Left",
        {
            QKeySequence(Qt::KeypadModifier | Qt::ShiftModifier | Qt::Key_4),
            QKeySequence(Qt::KeypadModifier | Qt::ShiftModifier | Qt::Key_Left),
        });
    connect(viewportRollLeftAction_, &QAction::triggered, this, [this]() { glView_->orbitRollLeft(); });
    navMenu->addAction(viewportRollLeftAction_);

    addViewportShortcut(
        viewportRollRightAction_,
        "viewport.roll_right",
        "Roll Right",
        {
            QKeySequence(Qt::KeypadModifier | Qt::ShiftModifier | Qt::Key_6),
            QKeySequence(Qt::KeypadModifier | Qt::ShiftModifier | Qt::Key_Right),
        });
    connect(viewportRollRightAction_, &QAction::triggered, this, [this]() { glView_->orbitRollRight(); });
    navMenu->addAction(viewportRollRightAction_);

    addViewportShortcutNoRepeat(
        viewportRollResetAction_,
        "viewport.roll_reset",
        "Roll Reset",
        {
            QKeySequence(Qt::KeypadModifier | Qt::ShiftModifier | Qt::Key_5),
            QKeySequence(Qt::KeypadModifier | Qt::ShiftModifier | Qt::Key_Clear),
        });
    connect(viewportRollResetAction_, &QAction::triggered, this, [this]()
        {
            if (!glView_)
            {
                return;
            }

            const Qt::KeyboardModifiers mods = QGuiApplication::keyboardModifiers();
            if (!mods.testFlag(Qt::ShiftModifier))
            {
                return;
            }

            glView_->orbitRollReset();
        });
    navMenu->addAction(viewportRollResetAction_);

    auto* gridMenu = ui::shortcuts::addKeySequenceDisplayMenu(viewMenu, "Grid");
    gridMenu->setObjectName("view.grid");
    gridMenu->menuAction()->setObjectName("view.grid.action");

    addViewportShortcutNoRepeat(
        viewportGridVisibleThroughWallsAction_,
        "viewport.grid_visible_through_walls",
        "Grid Visible Through Walls",
        {});
    viewportGridVisibleThroughWallsAction_->setCheckable(true);
    viewportGridVisibleThroughWallsAction_->setIcon(
        ui::makeViewportIcon(ui::ViewportIcon::mapGridVisibleThrough));
    viewportGridVisibleThroughWallsAction_->setToolTip(
        "Draw the grid over solid level geometry and Thing objects. Disable both visibility modes for the default depth-occluded grid.");
    connect(viewportGridVisibleThroughWallsAction_, &QAction::triggered, this, [this](const bool checked)
        {
            if (!glView_)
            {
                return;
            }

            QSettings settings = settingsStore();
            settings.setValue("viewport/grid_visible_through_walls", checked);
            if (checked)
            {
                settings.setValue("viewport/grid_always_behind", false);
                if (viewportGridAlwaysBehindAction_)
                {
                    const QSignalBlocker blocker(viewportGridAlwaysBehindAction_);
                    viewportGridAlwaysBehindAction_->setChecked(false);
                }
            }
            glView_->setGridVisibleThroughWalls(checked);
        });
    gridMenu->addAction(viewportGridVisibleThroughWallsAction_);

    addViewportShortcutNoRepeat(
        viewportGridAlwaysBehindAction_,
        "viewport.grid_always_behind",
        "Grid Always Behind Geometry",
        {
            QKeySequence(Qt::Key_Insert),
        });
    viewportGridAlwaysBehindAction_->setCheckable(true);
    viewportGridAlwaysBehindAction_->setIcon(
        ui::makeViewportIcon(ui::ViewportIcon::mapGridBehind));
    connect(viewportGridAlwaysBehindAction_, &QAction::triggered, this, [this](const bool checked)
        {
            if (!glView_)
            {
                return;
            }

            const Qt::KeyboardModifiers mods = QGuiApplication::keyboardModifiers();
            if ((mods & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) != Qt::NoModifier)
            {
                const QSignalBlocker blocker(viewportGridAlwaysBehindAction_);
                viewportGridAlwaysBehindAction_->setChecked(glView_->gridAlwaysBehind());
                return;
            }

            QSettings settings = settingsStore();
            settings.setValue("viewport/grid_always_behind", checked);
            if (checked)
            {
                settings.setValue("viewport/grid_visible_through_walls", false);
                if (viewportGridVisibleThroughWallsAction_)
                {
                    const QSignalBlocker blocker(viewportGridVisibleThroughWallsAction_);
                    viewportGridVisibleThroughWallsAction_->setChecked(false);
                }
            }
            glView_->setGridAlwaysBehind(checked);
        });
    gridMenu->addAction(viewportGridAlwaysBehindAction_);

    addViewportShortcutNoRepeat(
        viewportViewAlignToGridAction_,
        "viewport.view_align_to_grid",
        "Align View to Grid Plane",
        { QKeySequence(Qt::Key_F2) });
    viewportViewAlignToGridAction_->setToolTip(
        "Look straight at the current grid plane and center the view on its origin");
    connect(viewportViewAlignToGridAction_, &QAction::triggered, this, [this]()
        {
            if (glView_)
            {
                glView_->alignViewToGridPlane();
            }
        });
    gridMenu->addAction(viewportViewAlignToGridAction_);

    addViewportShortcutNoRepeat(
        viewportGridAlignToViewAction_,
        "viewport.grid_align_to_view",
        "Align Grid Plane to View",
        { QKeySequence(Qt::Key_F3), QKeySequence(Qt::KeypadModifier | Qt::Key_0) });
    connect(viewportGridAlignToViewAction_, &QAction::triggered, this, [this]()
        {
            if (glView_)
            {
                glView_->alignGridPlaneToView();
            }
        });
    gridMenu->addAction(viewportGridAlignToViewAction_);

    addViewportShortcutNoRepeat(
        viewportGridOrientToObjectAction_,
        "viewport.grid_orient_to_object",
        "Snap Grid to Item",
        { QKeySequence(Qt::SHIFT | Qt::Key_S) });
    connect(viewportGridOrientToObjectAction_, &QAction::triggered, this, [this]()
        {
            // Shift+S is also accelerated backward movement while RMB fly
            // navigation is active. The ShortcutOverride route normally keeps
            // the QAction from firing; retain this state guard as a final
            // safety net for platform/native shortcut delivery differences.
            if (glView_ && !glView_->flyNavigationActive())
            {
                glView_->snapGridToObject();
            }
        });
    gridMenu->addAction(viewportGridOrientToObjectAction_);

    viewportGridOrientToSelectionAction_ = new QAction("Orient Grid to Selection", this);
    viewportGridOrientToSelectionAction_->setObjectName("viewport.grid_orient_to_selection");
    viewportGridOrientToSelectionAction_->setProperty("sed_keymap_expose", true);
    viewportGridOrientToSelectionAction_->setShortcutContext(Qt::WidgetShortcut);
    glView_->addAction(viewportGridOrientToSelectionAction_);
    connect(viewportGridOrientToSelectionAction_, &QAction::triggered, this, [this]()
        {
            if (glView_)
            {
                glView_->orientGridToSelection();
            }
        });
    gridMenu->addAction(viewportGridOrientToSelectionAction_);

    addViewportShortcutNoRepeat(
        viewportGridCenterOnSelectionAction_,
        "viewport.grid_center_on_selection",
        "Center Grid on Selection",
        { QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_G) });
    connect(viewportGridCenterOnSelectionAction_, &QAction::triggered, this, [this]()
        {
            if (glView_)
            {
                glView_->centerGridOnSelection();
            }
        });
    gridMenu->addAction(viewportGridCenterOnSelectionAction_);

    viewportGridClearCenterAction_ = new QAction("Reset grid align/center", this);
    // Keep this command distinct from the persistent "Grid follows camera"
    // toggle.  Reusing that object name made QObject::findChild() and the
    // keymap/preferences model resolve the wrong action, so the reset state
    // could appear stale or trigger the follow-camera setting instead.
    viewportGridClearCenterAction_->setObjectName("viewport.grid_clear_center");
    viewportGridClearCenterAction_->setProperty("sed_keymap_expose", true);
    viewportGridClearCenterAction_->setShortcutContext(Qt::WidgetShortcut);
    glView_->addAction(viewportGridClearCenterAction_);
    connect(viewportGridClearCenterAction_, &QAction::triggered, this, [this]()
        {
            if (glView_)
            {
                glView_->resetGridOverrides();
            }
        });
    gridMenu->addAction(viewportGridClearCenterAction_);

    const auto syncGridMenuAvailability = [this, gridMenu]()
    {
        const bool editorMode = glView_
            && glView_->renderPipeline() != GlView::RenderPipeline::GamePreview;
        gridMenu->menuAction()->setVisible(editorMode);
        gridMenu->menuAction()->setEnabled(editorMode);
        for (QAction* action : gridMenu->actions())
        {
            if (action)
            {
                action->setVisible(editorMode);
                action->setEnabled(editorMode);
            }
        }
        if (viewportGridClearCenterAction_)
        {
            viewportGridClearCenterAction_->setEnabled(
                editorMode && glView_->gridOverridesActive());
        }
    };
    connect(
        glView_,
        &GlView::viewportStateChanged,
        gridMenu,
        syncGridMenuAvailability);
    connect(viewMenu, &QMenu::aboutToShow, gridMenu, syncGridMenuAvailability);
    syncGridMenuAvailability();

    auto addFlyBinding = [&](QAction*& action, const QString& id, const QString& text, const QKeySequence& shortcut)
    {
        action = new QAction(text, this);
        action->setObjectName(id);
        action->setShortcut(shortcut);
        // Not added to menus: used as rebindable storage for the fly-navigation event filter.
    };

    addFlyBinding(flyFastAction_, "viewport.fly.fast", "Fly Faster", QKeySequence(Qt::Key_Shift));
    addFlyBinding(flySlowAction_, "viewport.fly.slow", "Fly Slower", QKeySequence(Qt::Key_Control));
    addFlyBinding(flyForwardAction_, "viewport.fly.forward", "Fly Forward", QKeySequence(Qt::Key_W));
    addFlyBinding(flyBackwardAction_, "viewport.fly.backward", "Fly Backward", QKeySequence(Qt::Key_S));
    addFlyBinding(flyLeftAction_, "viewport.fly.left", "Fly Left", QKeySequence(Qt::Key_A));
    addFlyBinding(flyRightAction_, "viewport.fly.right", "Fly Right", QKeySequence(Qt::Key_D));
    addFlyBinding(flyDownAction_, "viewport.fly.down", "Fly Down", QKeySequence(Qt::Key_Q));
    addFlyBinding(flyUpAction_, "viewport.fly.up", "Fly Up", QKeySequence(Qt::Key_E));
    addFlyBinding(flySpeedUpAction_, "viewport.fly.speed_up", "Fly Speed Up",
        QKeySequence(Qt::KeypadModifier | Qt::Key_Plus));
    flySpeedUpAction_->setToolTip(
        "Increase the chosen Fly speed by one 10% step while Fly Look owns input. Releasing the key keeps that speed.");
    connect(flySpeedUpAction_, &QAction::triggered, this, [this]()
        {
            if ( glView_ )
            {
                glView_->adjustFlyNavigationSpeedBySteps(1.0f);
            }
        });
    addFlyBinding(flySpeedDownAction_, "viewport.fly.speed_down", "Fly Speed Down",
        QKeySequence(Qt::KeypadModifier | Qt::Key_Minus));
    flySpeedDownAction_->setToolTip(
        "Decrease the chosen Fly speed by the inverse 10% step while Fly Look owns input. Releasing the key keeps that speed.");
    connect(flySpeedDownAction_, &QAction::triggered, this, [this]()
        {
            if ( glView_ )
            {
                glView_->adjustFlyNavigationSpeedBySteps(-1.0f);
            }
        });

    auto addSelectionBinding = [&](QAction*& action, const QString& id, const QString& text, const QList<QKeySequence>& shortcuts)
    {
        action = new QAction(text, this);
        action->setObjectName(id);
        action->setShortcuts(shortcuts);
        // Not added to menus: used as rebindable storage for selection behavior.
    };

    addSelectionBinding(
        selectionAddModeAction_,
        "selection.mode.add",
        "Selection: Add (Hold)",
        { QKeySequence(Qt::Key_Shift) });
    addSelectionBinding(
        selectionRemoveModeAction_,
        "selection.mode.remove",
        "Selection: Remove (Hold)",
        { QKeySequence(Qt::Key_Control) });
    addSelectionBinding(
        selectionSurfacePathModeAction_,
        "selection.mode.surface_shortest_path",
        "Selection: Surface Shortest Path (Hold+Click)",
        { QKeySequence(QStringLiteral("Ctrl+Shift"), QKeySequence::PortableText) });
    addSelectionBinding(
        selectionSurfaceBypassFiltersModeAction_,
        "selection.mode.surface_bypass_filters",
        "Selection: Surface Ignore Filters (Hold)",
        { QKeySequence(Qt::Key_Alt) });
    addSelectionBinding(
        thingPlacementTiltModifierAction_,
        "thing_placement.modifier.surface_tilt",
        "Add Thing: Surface Tilt Modifier",
        { QKeySequence(Qt::Key_Control) });
    addSelectionBinding(
        thingPlacementFreeModifierAction_,
        "thing_placement.modifier.free_insert",
        "Add Thing: Free Insert Modifier",
        { QKeySequence(Qt::Key_Shift) });
    addSelectionBinding(
        thingToolSelectKeyAction_,
        "editor.tool.select",
        "Editor: Select Tool",
        { QKeySequence(Qt::Key_S) });
    addSelectionBinding(
        thingToolMoveKeyAction_,
        "editor.tool.move",
        "Editor: Move/Transform Tool",
        { QKeySequence(Qt::Key_T) });
    addSelectionBinding(
        thingToolRotateKeyAction_,
        "editor.tool.rotate",
        "Editor: Rotate Tool",
        { QKeySequence(Qt::Key_R) });
    addSelectionBinding(
        thingToolScaleKeyAction_,
        "editor.tool.scale",
        "Editor: Scale Tool",
        { QKeySequence(Qt::Key_E) });
    addSelectionBinding(
        thingToolToggleOrientationKeyAction_,
        "editor.transform.toggle_local_world",
        "Editor: Toggle Local/World",
        { QKeySequence(int(Qt::ShiftModifier) | Qt::Key_X) });
    addSelectionBinding(
        editModeSectorKeyAction_,
        "edit_mode.sector",
        "Edit Mode: Sector",
        { QKeySequence(Qt::Key_1) });
    addSelectionBinding(
        editModeSurfaceKeyAction_,
        "edit_mode.surface",
        "Edit Mode: Surface",
        { QKeySequence(Qt::Key_2) });
    addSelectionBinding(
        editModeThingKeyAction_,
        "edit_mode.thing",
        "Edit Mode: Thing",
        { QKeySequence(Qt::Key_3) });
    addSelectionBinding(
        editModeLightKeyAction_,
        "edit_mode.light",
        "Edit Mode: Light",
        { QKeySequence(Qt::Key_4) });
    addSelectionBinding(
        sectorCycleAddModeKeyAction_,
        "sector.command.cycle_add_mode",
        "Sector: Cycle Add Mode",
        { QKeySequence(int(Qt::ShiftModifier) | Qt::Key_A) });
    addSelectionBinding(
        sectorCleaveKeyAction_,
        "sector.command.cleave",
        "Sector: Cleave",
        { QKeySequence(Qt::Key_C) });
    addSelectionBinding(
        sectorJoinKeyAction_,
        "sector.command.join",
        "Sector: Join Sectors",
        { QKeySequence(Qt::Key_J) });
    addSelectionBinding(
        sectorMergeKeyAction_,
        "sector.command.merge",
        "Sector: Merge Sectors",
        { QKeySequence(Qt::Key_M) });
    addSelectionBinding(
        surfaceMakeAdjoinKeyAction_,
        "surface.command.make_adjoin",
        "Surface: Adjoin",
        { QKeySequence(Qt::Key_A) });
    addSelectionBinding(
        surfaceUnadjoinKeyAction_,
        "surface.command.unadjoin",
        "Surface: Unadjoin",
        { QKeySequence(int(Qt::ShiftModifier) | Qt::Key_A) });
    addSelectionBinding(
        surfaceCycleSubmodeKeyAction_,
        "surface.command.cycle_submode",
        "Surface: Cycle Submode",
        { QKeySequence(int(Qt::ShiftModifier) | Qt::Key_Tab) });
    addSelectionBinding(
        surfaceCleaveKeyAction_,
        "surface.command.cleave",
        "Surface: Cleave",
        { QKeySequence(Qt::Key_C) });
    addSelectionBinding(
        surfaceMergeKeyAction_,
        "surface.command.merge",
        "Surface: Merge",
        { QKeySequence(Qt::Key_M) });
    addSelectionBinding(
        surfaceExtrudeKeyAction_,
        "surface.command.extrude",
        "Surface: Extrude",
        { QKeySequence(Qt::Key_X) });
    addSelectionBinding(
        surfaceConnectKeyAction_,
        "surface.command.connect",
        "Surface: Connect Surfaces",
        { QKeySequence(Qt::Key_K) });
    addSelectionBinding(
        surfaceFaceModeKeyAction_,
        "surface.submode.face",
        "Surface: Face Submode",
        { QKeySequence(int(Qt::ShiftModifier) | Qt::Key_1),
          QKeySequence(int(Qt::ShiftModifier) | Qt::Key_F) });
    addSelectionBinding(
        surfaceEdgeModeKeyAction_,
        "surface.submode.edge",
        "Surface: Edge Submode",
        { QKeySequence(int(Qt::ShiftModifier) | Qt::Key_2),
          QKeySequence(int(Qt::ShiftModifier) | Qt::Key_E) });
    addSelectionBinding(
        surfaceVertexModeKeyAction_,
        "surface.submode.vertex",
        "Surface: Vertex Submode",
        { QKeySequence(int(Qt::ShiftModifier) | Qt::Key_3),
          QKeySequence(int(Qt::ShiftModifier) | Qt::Key_V) });
    addSelectionBinding(
        thingToggleFrameModeKeyAction_,
        "thing.command.toggle_frame_mode",
        "Thing: Toggle Frame Mode",
        { QKeySequence(int(Qt::ShiftModifier) | Qt::Key_1),
          QKeySequence(int(Qt::ShiftModifier) | Qt::Key_Tab) });
    QAction* lightSubmodeBinding = nullptr;
    addSelectionBinding(lightSubmodeBinding,
        "light.submode.baked", "Light: Baked Submode",
        { QKeySequence(int(Qt::ShiftModifier) | Qt::Key_1) });
    setProperty("light.submode.baked", QVariant::fromValue(lightSubmodeBinding));
    addSelectionBinding(lightSubmodeBinding,
        "light.submode.thing", "Light: Thing Submode",
        { QKeySequence(int(Qt::ShiftModifier) | Qt::Key_2) });
    setProperty("light.submode.thing", QVariant::fromValue(lightSubmodeBinding));
    addSelectionBinding(lightSubmodeBinding,
        "light.submode.sector", "Light: IJIM Sector Submode",
        { QKeySequence(int(Qt::ShiftModifier) | Qt::Key_3) });
    setProperty("light.submode.sector", QVariant::fromValue(lightSubmodeBinding));
    addSelectionBinding(lightSubmodeBinding,
        "light.submode.architectural", "Light: MotS Architectural Submode",
        { QKeySequence(int(Qt::ShiftModifier) | Qt::Key_3) });
    setProperty("light.submode.architectural", QVariant::fromValue(lightSubmodeBinding));
    addSelectionBinding(
        transformGridSnapInvertModifierAction_,
        "viewport.transform.modifier.invert_snap",
        "Transform Move: Invert Grid Snap (Hold)",
        { QKeySequence(Qt::Key_Control) });
    addSelectionBinding(
        transformGridStickInvertModifierAction_,
        "viewport.transform.modifier.invert_grid_stick",
        "Transform Move: Invert Grid Stick (Hold)",
        { QKeySequence(QStringLiteral("Shift+Alt"), QKeySequence::PortableText) });
    addSelectionBinding(
        sectorMoveSnapAlignInvertModifierAction_,
        "sector_move.modifier.invert_snap_align",
        "Sector Move: Invert Surface Snap Align (Hold)",
        { QKeySequence(Qt::Key_Control) });
    addSelectionBinding(
        surfaceGeometrySnapInvertModifierAction_,
        "surface_move.modifier.invert_nearby_snap",
        "Surface Transform: Invert Nearby Geometry Snap (Hold)",
        { QKeySequence(Qt::Key_Shift) });
    addSelectionBinding(
        thingMoveSurfaceStickInvertModifierAction_,
        "thing_move.modifier.invert_surface_stick",
        "Thing Move: Invert Surface Stick (Hold)",
        { QKeySequence(Qt::Key_Shift) });
    addSelectionBinding(
        thingMoveSurfaceAlignInvertModifierAction_,
        "thing_move.modifier.invert_surface_align",
        "Thing Move: Invert Surface Alignment (Hold)",
        { QKeySequence(QStringLiteral("Ctrl+Shift"), QKeySequence::PortableText) });
    addSelectionBinding(
        thingMoveSnapInvertModifierAction_,
        "thing_move.modifier.invert_snap_to_thing",
        "Thing Move: Invert Snap To Thing (Hold)",
        { QKeySequence(Qt::Key_Control) });
    addSelectionBinding(
        thingMoveSnapRotateInvertModifierAction_,
        "thing_move.modifier.invert_snap_rotate",
        "Thing Move: Invert Snap Rotation (Hold)",
        { QKeySequence(QStringLiteral("Ctrl+Alt"), QKeySequence::PortableText) });
    for (QAction* action : {
             transformGridSnapInvertModifierAction_,
             transformGridStickInvertModifierAction_,
             sectorMoveSnapAlignInvertModifierAction_,
             surfaceGeometrySnapInvertModifierAction_,
             thingMoveSurfaceStickInvertModifierAction_,
             thingMoveSurfaceAlignInvertModifierAction_,
             thingMoveSnapInvertModifierAction_,
             thingMoveSnapRotateInvertModifierAction_ })
    {
        action->setProperty("sed_modifier_only_shortcut", true);
    }

    syncSelectionModifierBindings();
}

} // namespace sed
