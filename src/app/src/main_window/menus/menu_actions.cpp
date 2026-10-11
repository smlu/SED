#include "main_window/common.hpp"

#include "ui/components/about_dialog.hpp"
#include "ui/resources/resource_authoring_editor.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"

namespace sed
{

void MainWindow::setupMenus()
{
    auto* fileMenu = ui::shortcuts::addKeySequenceDisplayMenu(menuBar(), "&File");
    auto* editMenu = ui::shortcuts::addKeySequenceDisplayMenu(menuBar(), "&Edit");
    auto* mapMenu = ui::shortcuts::addKeySequenceDisplayMenu(menuBar(), "&Map");
    auto* viewMenu = ui::shortcuts::addKeySequenceDisplayMenu(menuBar(), "&View");
    auto* toolsMenu = ui::shortcuts::addKeySequenceDisplayMenu(menuBar(), "&Tools");
    auto* helpMenu = ui::shortcuts::addKeySequenceDisplayMenu(menuBar(), "&Help");

    // Step 1: the File menu owns project/open/save/resource-root workflows.
    setupFileMenu(fileMenu);

    // Step 2: the Edit menu core owns undo/redo/delete and global edit commands.
    setupEditCoreActions(editMenu);

    // Step 3: the Map menu owns level-wide tools; the campaign editor is appended to Edit.
    setupMapMenu(mapMenu, editMenu);

    // Step 4: the Edit entity submenus own sector/surface/thing/light/cog commands.
    setupEditEntityMenus(editMenu);

    auto* resourcesMenu = ui::shortcuts::addKeySequenceDisplayMenu(editMenu, "&Resources");
    resourcesMenu->setObjectName("edit.resources");
    auto addLegacyResourceAuthoringAction = [this, resourcesMenu](
        const QString& title,
        const char* objectName,
        const char* toolTip,
        const ui::resources::ResourceAuthoringKind kind)
    {
        auto* action = resourcesMenu->addAction(title);
        action->setObjectName(QString::fromLatin1(objectName));
        action->setToolTip(QString::fromLatin1(toolTip));
        connect(action, &QAction::triggered, this, [this, kind]()
            {
                openResourceEditor(kind, 0);
            });
    };
    addLegacyResourceAuthoringAction(
        "&Sprite (.spr)...",
        "edit.resources.sprite_spr",
        "Create or edit a SPR sprite resource with an isolated 3D preview",
        ui::resources::ResourceAuthoringKind::sprite);
    addLegacyResourceAuthoringAction(
        "&Sound Class (.snd)...",
        "edit.resources.sound_class_snd",
        "Create or edit ordered SND sound-class playback rows",
        ui::resources::ResourceAuthoringKind::soundClass);
    addLegacyResourceAuthoringAction(
        "&String Table (.uni)...",
        "edit.resources.string_table_uni",
        "Create or edit a UNI COG string table",
        ui::resources::ResourceAuthoringKind::stringTable);

    const auto addResourceEditorAction = [this, toolsMenu](
        const QString& label,
        const char* objectName,
        const ui::resources::ResourceAuthoringKind kind)
    {
        QAction* const action = toolsMenu->addAction(label);
        action->setIcon(ui::makeViewportIcon(kind == ui::resources::ResourceAuthoringKind::sprite
            ? ui::ViewportIcon::resourceSprite : kind == ui::resources::ResourceAuthoringKind::soundClass
                ? ui::ViewportIcon::sound : ui::ViewportIcon::actionWhitespace));
        action->setObjectName(QString::fromLatin1(objectName));
        action->setToolTip(QStringLiteral("Open %1")
            .arg(label));
        connect(action, &QAction::triggered, this, [this, kind]()
            {
                openResourceEditor(kind, -1);
            });
    };
    addResourceEditorAction(
        QStringLiteral("Sprite Editor"),
        "tools.resource_editors.sprite_spr.new",
        ui::resources::ResourceAuthoringKind::sprite);
    addResourceEditorAction(
        QStringLiteral("Sound Class Editor"),
        "tools.resource_editors.sound_class_snd.new",
        ui::resources::ResourceAuthoringKind::soundClass);
    addResourceEditorAction(
        QStringLiteral("UNI Editor"),
        "tools.resource_editors.string_table_uni.new",
        ui::resources::ResourceAuthoringKind::stringTable);

    if ( cogScriptEditorDock_ )
    {
        QAction* const action = cogScriptEditorDock_->toggleViewAction();
        action->setText(QStringLiteral("COG Script Editor"));
        action->setObjectName(QStringLiteral("tools.cog_script_editor"));
        action->setIcon(ui::makeViewportIcon(ui::ViewportIcon::outlinerCogs));
        action->setToolTip(QStringLiteral("Open COG Script Editor"));
        toolsMenu->addAction(action);
    }

    // Step 5: the View menu owns viewport render modes, dock windows, and viewport shortcuts.
    setupViewMenu(viewMenu);

    // Step 6: Help owns application identity and project links.
    auto* aboutAction = helpMenu->addAction("&About SED");
    aboutAction->setObjectName(QStringLiteral("help.about_sed"));
    connect(aboutAction, &QAction::triggered, this, [this]()
        {
            ui::AboutDialog dialog(this);
            dialog.exec();
        });
}

} // namespace sed
