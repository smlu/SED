#include "main_window/common.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"

#include "app_logging.hpp"

#include "sed/assets/3do.hpp"
#include "sed/assets/material_dimensions.hpp"
#include "sed/text.hpp"
#include "ui/checks/level_check_utils.hpp"
#include "ui/sector/sector_import_failure_dialog.hpp"
#include "ui/sector/sector_import_utils.hpp"

#include <fstream>
#include <sstream>
#include <unordered_map>

#include <QSaveFile>

namespace sed
{
namespace
{

QString flattenedImportLogField(QString value, const qsizetype maximumLength)
{
    for (qsizetype index = 0; index < value.size(); ++index)
    {
        const ushort code = value.at(index).unicode();
        if (code < 0x20u || code == 0x7fu)
        {
            value[index] = QLatin1Char(' ');
        }
    }
    value = value.trimmed();
    if (value.size() > maximumLength)
    {
        value.truncate(maximumLength - 3);
        value += QStringLiteral("...");
    }
    return value;
}

void showSectorExportFailure(
    QWidget* parent,
    const QString& filename,
    const QString& reason)
{
    qCWarning(logCore).noquote()
        << QStringLiteral("Sector export failed: %1: %2")
               .arg(flattenedImportLogField(filename, 2048),
                   flattenedImportLogField(reason, 4096));

    QMessageBox dialog(
        QMessageBox::Warning,
        QStringLiteral("Export Sector(s) failed"),
        reason,
        QMessageBox::Ok,
        parent);
    dialog.setObjectName(QStringLiteral("sectorExportFailureDialog"));
    dialog.setProperty(kMessageBoxLoggingSkipProperty, true);
    (void)dialog.exec();
}

} // namespace

void ui::sector::showSectorImportFailure(
    QWidget* parent,
    const QString& filename,
    const QString& summary,
    const QString& details)
{
    const QString logFilename = flattenedImportLogField(filename, 2048);
    const QString logSummary = flattenedImportLogField(summary, 4096);
    QString record = QStringLiteral("Sector import failed: %1: %2")
        .arg(logFilename, logSummary);
    if (!details.isEmpty())
    {
        QString detailLines = details;
        detailLines.replace(QStringLiteral("\r\n"), QStringLiteral(" | "));
        detailLines.replace(QLatin1Char('\r'), QStringLiteral(" | "));
        detailLines.replace(QLatin1Char('\n'), QStringLiteral(" | "));
        const QString logDetails =
            flattenedImportLogField(detailLines, 16384);
        record += QStringLiteral(" Details: %1").arg(logDetails);
    }
    qCWarning(logCore).noquote() << record;

    QMessageBox dialog(
        QMessageBox::Warning,
        QStringLiteral("Import Sector(s) failed"),
        summary,
        QMessageBox::Ok,
        parent);
    dialog.setObjectName(QStringLiteral("sectorImportFailureDialog"));
    dialog.setProperty(kMessageBoxLoggingSkipProperty, true);
    dialog.setInformativeText(
        QStringLiteral("Source: %1").arg(filename));
    if (!details.isEmpty())
    {
        dialog.setDetailedText(details);
    }
    (void)dialog.exec();
}

void MainWindow::setupEditEntityMenus(QMenu* editMenu)
{
    if (!editMenu)
    {
        return;
    }

    // Step 1: create the entity-specific Edit submenus.
    editMenu->addSeparator();

    auto* sectorMenu = ui::shortcuts::addKeySequenceDisplayMenu(editMenu, "S&ector");
    sectorMenu->setObjectName("edit.sector");

    auto* surfaceMenu = ui::shortcuts::addKeySequenceDisplayMenu(editMenu, "&Surface");
    surfaceMenu->setObjectName("edit.surface");

    auto* thingMenu = ui::shortcuts::addKeySequenceDisplayMenu(editMenu, "&Thing");
    thingMenu->setObjectName("edit.thing");

    auto* lightMenu = ui::shortcuts::addKeySequenceDisplayMenu(editMenu, "&Light");
    lightMenu->setObjectName("edit.light");

    auto* cogMenu = ui::shortcuts::addKeySequenceDisplayMenu(editMenu, "CO&G");
    cogMenu->setObjectName("edit.cog");

    // QAction cannot expose independent enabled state in several menus.  Keep
    // Delete's shortcut on one canonical action, while each entity menu gets
    // a shortcut-free proxy which calls the same defensively guarded slot.
    const auto makeDeleteMenuAction = [this](const char* objectName)
    {
        auto* action = new QAction(
            deleteAction_ ? deleteAction_->icon() : QIcon{}, "&Delete", this);
        action->setObjectName(QString::fromLatin1(objectName));
        action->setToolTip(deleteAction_ ? deleteAction_->toolTip()
                                         : QStringLiteral("Delete the selection"));
        connect(action, &QAction::triggered, this, &MainWindow::deleteSelection);
        return action;
    };
    sectorDeleteMenuAction_ = makeDeleteMenuAction("edit.sector.delete");
    surfaceDeleteMenuAction_ = makeDeleteMenuAction("edit.surface.delete");
    thingDeleteMenuAction_ = makeDeleteMenuAction("edit.thing.delete");
    lightDeleteMenuAction_ = makeDeleteMenuAction("edit.light.delete");
    cogDeleteMenuAction_ = makeDeleteMenuAction("edit.cog.delete");

    auto* newCogScriptAction = new QAction("New COG &Script...", this);
    newCogScriptAction->setObjectName("edit.new_cog_script");
    newCogScriptAction->setToolTip(
        "Open a new unsaved COG script draft and add its placed COG to the level");
    connect(newCogScriptAction, &QAction::triggered, this, [this]()
        {
            if (!documentEditingAllowed())
            {
                return;
            }
            createNewCogScript();
        });
    cogMenu->addAction(newCogScriptAction);
    cogMenu->addSeparator();

    auto* transformMenu = ui::shortcuts::addKeySequenceDisplayMenu(editMenu, "T&ransform");
    transformMenu->setObjectName("edit.transform");

    addCubeAction_ = new QAction("Add &Sector", this);
    addCubeAction_->setObjectName("edit.add_cube");
    addCubeAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::actionAddCube));
    addCubeAction_->setShortcut(QKeySequence(Qt::Key_A));
    addCubeAction_->setShortcutContext(Qt::WidgetShortcut);
    addCubeAction_->setAutoRepeat(false);
    addCubeAction_->setCheckable(true);
    addCubeAction_->setToolTip("Add a sector primitive or saved shape");
    connect(addCubeAction_, &QAction::triggered, this, [this]()
        {
            const PlacementMode newMode = (placementMode_ == PlacementMode::cube) ? PlacementMode::none : PlacementMode::cube;
            setPlacementMode(newMode);
        });
    sectorMenu->addAction(addCubeAction_);
    sectorMenu->addAction(sectorDeleteMenuAction_);
    sectorMenu->addSeparator();

    importSectorAction_ = new QAction("&Import Sector(s)...", this);
    importSectorAction_->setObjectName("edit.import_sector");
    importSectorAction_->setToolTip(
        "Import one or more convex sectors from a 3DO model or Wavefront OBJ file");
    connect(importSectorAction_, &QAction::triggered, this, [this]()
        {
            if (editMode_ != EditMode::sector
                || (glView_ && glView_->renderPipeline() == GlView::RenderPipeline::GamePreview))
            {
                return;
            }
            const QString filename = QFileDialog::getOpenFileName(
                this,
                "Import Sector(s)",
                fileDialogStartLocation(FileDialogHistoryGroup::import_),
                "Sector geometry (*.3do *.obj);;3DO models (*.3do);;Wavefront OBJ (*.obj)");
            if (filename.isEmpty())
            {
                return;
            }
            rememberFileDialogSelection(
                FileDialogHistoryGroup::import_, filename);

            const std::filesystem::path path(filename.toStdWString());
            const std::string extension =
                sed::text::lowerAscii(path.extension().string());

            QDialog options(this);
            options.setObjectName("sectorImportOptionsDialog");
            options.setWindowTitle("Import Sector(s)");
            options.setWindowFlag(Qt::WindowMinimizeButtonHint, false);
            options.setWindowFlag(Qt::WindowMaximizeButtonHint, false);
            auto* optionsLayout = new QVBoxLayout(&options);
            optionsLayout->addWidget(new QLabel(
                "Choose how the selected geometry should be imported.",
                &options));
            auto* splitMeshes = new QCheckBox(
                "Split objects/nodes into connected sectors", &options);
            splitMeshes->setObjectName("sectorImportSplitMeshes");
            splitMeshes->setChecked(true);
            splitMeshes->setToolTip(
                "Create one sector per non-empty OBJ object/group or 3DO hierarchy node, then adjoin matching faces.");
            optionsLayout->addWidget(splitMeshes);

            ui::sector::ObjSectorImportOptions objImportOptions{};
            const std::shared_ptr<const assets::ResourceLocator>
                importResources = resourceLocatorSnapshot_
                    ? resourceLocatorSnapshot_
                    : std::make_shared<const assets::ResourceLocator>(
                        resources_);
            auto textureSizeCache = std::make_shared<
                std::unordered_map<std::string,
                    std::optional<level::UvTextureDimensions>>>();
            const ui::sector::SectorTextureDimensionsResolver
                resolveTextureDimensions =
                [importResources, textureSizeCache](
                    const std::string_view material)
                    -> std::optional<level::UvTextureDimensions>
            {
                const std::string trimmed = sed::text::trim(material);
                if (trimmed.empty()
                    || sed::text::iequalsAscii(trimmed, "none")
                    || sed::text::iequalsAscii(trimmed, "transparent"))
                {
                    return std::nullopt;
                }
                const std::string key = sed::text::upperAscii(trimmed);
                if (const auto found = textureSizeCache->find(key);
                    found != textureSizeCache->end())
                {
                    return found->second;
                }

                const std::optional<level::UvTextureDimensions> size =
                    assets::resolveMaterialTextureDimensions(
                        *importResources, trimmed);
                textureSizeCache->emplace(key, size);
                return size;
            };
            objImportOptions.materialTextureSize =
                [resolveTextureDimensions](const std::string_view material)
                    -> std::optional<
                        ui::sector::ObjSectorImportOptions::TextureSize>
            {
                const std::optional<level::UvTextureDimensions> size =
                    resolveTextureDimensions(material);
                if (!size)
                {
                    return std::nullopt;
                }
                return ui::sector::ObjSectorImportOptions::TextureSize{
                    static_cast<float>(size->width),
                    static_cast<float>(size->height) };
            };
            auto* convertObjCoordinates = new QCheckBox(
                "Transform OBJ coordinate system to SED coordinate system", &options);
            convertObjCoordinates->setObjectName(
                "sectorImportConvertObjYUpToZUp");
            convertObjCoordinates->setChecked(
                objImportOptions.convertYUpToSithZUp);
            convertObjCoordinates->setToolTip(
                "Rotate conventional OBJ geometry into the SED/Sith coordinate system without changing UVs.");
            convertObjCoordinates->setVisible(extension == ".obj");
            optionsLayout->addWidget(convertObjCoordinates);

            auto* optionButtons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &options);
            optionButtons->setObjectName("sectorImportOptionsButtons");
            if (QPushButton* importButton = optionButtons->button(
                    QDialogButtonBox::Ok))
            {
                importButton->setText("Import");
            }
            connect(optionButtons, &QDialogButtonBox::accepted,
                &options, &QDialog::accept);
            connect(optionButtons, &QDialogButtonBox::rejected,
                &options, &QDialog::reject);
            optionsLayout->addWidget(optionButtons);
            if (options.exec() != QDialog::Accepted)
            {
                return;
            }
            objImportOptions.convertYUpToSithZUp =
                convertObjCoordinates->isChecked();

            ui::sector::ImportedSectorMeshResult loaded{};
            try
            {
                if (extension == ".3do")
                {
                    assets::Model3doLoadOptions importOptions{};
                    importOptions.applyHierarchyTransforms = false;
                    loaded = ui::sector::importedSectorMeshesFrom3do(
                        assets::loadFrom3DO(path, importOptions));
                }
                else if (extension == ".obj")
                {
                    std::ifstream input(path);
                    if (!input)
                    {
                        loaded.error = "Unable to open the selected OBJ file.";
                    }
                    else
                    {
                        loaded = ui::sector::loadObjSectorMeshes(
                            input, objImportOptions);
                    }
                }
                else
                {
                    loaded.error = "Only 3DO and Wavefront OBJ files are supported.";
                }
            }
            catch (const std::exception& ex)
            {
                loaded.error = ex.what();
            }

            if (!loaded)
            {
                const QString message = QString::fromStdString(loaded.error);
                ui::sector::showSectorImportFailure(
                    this, filename, message);
                return;
            }

            std::vector<std::int32_t> importedSectors{};
            std::string importError{};
            std::string importDetails{};
            const bool importMultipleSectors =
                splitMeshes->isChecked() && loaded.meshes.size() > 1;
            bool applied = false;
            try
            {
                applied = document_.applyUndoable(
                    importMultipleSectors ? "Sectors: Imported" : "Sector: Imported",
                    editor::DocumentDirty::Structure | editor::DocumentDirty::LevelGeometry,
                    [&](editor::DocumentState& state) -> bool
                    {
                        const ui::sector::ImportedSectorBuildResult built =
                            ui::sector::appendImportedSectors(
                                state.level,
                                loaded.meshes,
                                splitMeshes->isChecked(),
                                ui::sector::ImportedSectorBuildOptions{
                                    resolveTextureDimensions });
                        if (!built)
                        {
                            importError = built.error;
                            importDetails = built.details;
                            return false;
                        }
                        const auto checkerIssues = ui::checks::collectConsistencyCheckIssues(
                            state.level,
                            ui::checks::defaultCheckRuleSelection(state.level.kind));
                        const auto importedContains = [&built](const int sector) noexcept
                        {
                            return std::find(
                                built.sectors.begin(), built.sectors.end(), sector)
                                != built.sectors.end();
                        };
                        const auto invalidImported = std::find_if(
                            checkerIssues.begin(), checkerIssues.end(),
                            [&importedContains](const ui::checks::CheckIssue& issue)
                            {
                                if (issue.severity != ui::checks::CheckSeverity::error)
                                {
                                    return false;
                                }
                                if (issue.navKind
                                    && *issue.navKind == OutlinerKind::sector
                                    && issue.navIndex)
                                {
                                    return importedContains(*issue.navIndex);
                                }
                                return issue.navSector
                                    && importedContains(*issue.navSector);
                            });
                        if (invalidImported != checkerIssues.end())
                        {
                            importError = invalidImported->message.toStdString();
                            return false;
                        }
                        importedSectors = built.sectors;
                        state.selection.clear();
                        state.selection.sectors.insert(
                            importedSectors.begin(), importedSectors.end());
                        return true;
                    });
            }
            catch (const std::exception& ex)
            {
                importError = ex.what();
            }
            if (!applied)
            {
                const QString message = QString::fromStdString(
                    importError.empty()
                        ? std::string(
                            "The imported geometry is not a valid convex sector.")
                        : importError);
                ui::sector::showSectorImportFailure(
                    this,
                    filename,
                    message,
                    QString::fromStdString(importDetails));
                return;
            }

            invalidateSurfaceSelectionContextForTopologyEdit();

            qInfo().noquote()
                << "Imported" << static_cast<qulonglong>(importedSectors.size())
                << "Sector(s) from" << filename;

            if (glView_)
            {
                glView_->notifyLevelGeometryChanged();
                const std::int32_t active = importedSectors.empty() ? -1 : importedSectors.front();
                glView_->setSelectionState(active, -1, -1, -1, document_.selection());
            }
            rebuildOutliner();
            rebuildProperties();
            updateEditActions();
            scheduleRebuildChecks(
                editor::DocumentDirty::Structure | editor::DocumentDirty::LevelGeometry);
        });
    sectorMenu->addAction(importSectorAction_);

    exportSectorAction_ = new QAction("&Export Sector(s)...", this);
    exportSectorAction_->setObjectName("edit.export_sectors");
    exportSectorAction_->setToolTip(
        "Export the selected sectors as Wavefront OBJ or 3DO polygon geometry");
    connect(exportSectorAction_, &QAction::triggered, this, [this]()
        {
            std::vector<std::int32_t> selected(
                document_.selection().sectors.begin(),
                document_.selection().sectors.end());
            std::sort(selected.begin(), selected.end());
            if (selected.empty())
            {
                return;
            }

            QString selectedFilter = "Wavefront OBJ (*.obj)";
            QString filename = QFileDialog::getSaveFileName(
                this,
                "Export Sector(s)",
                fileDialogStartLocation(FileDialogHistoryGroup::export_),
                "Wavefront OBJ (*.obj);;3DO model (*.3do)",
                &selectedFilter);
            if (filename.isEmpty())
            {
                return;
            }
            rememberFileDialogSelection(
                FileDialogHistoryGroup::export_, filename);

            std::filesystem::path path(filename.toStdWString());
            std::string extension = sed::text::lowerAscii(path.extension().string());
            if (extension.empty())
            {
                extension = selectedFilter.contains("3DO", Qt::CaseInsensitive)
                    ? ".3do" : ".obj";
                filename += QString::fromStdString(extension);
                path = std::filesystem::path(filename.toStdWString());
            }

            ui::sector::ExportedSectorTextResult exported{};
            try
            {
                const std::shared_ptr<const assets::ResourceLocator>
                    exportResources = resourceLocatorSnapshot_
                        ? resourceLocatorSnapshot_
                        : std::make_shared<const assets::ResourceLocator>(
                            resources_);
                auto exportTextureSizeCache = std::make_shared<
                    std::unordered_map<std::string,
                        std::optional<level::UvTextureDimensions>>>();
                const ui::sector::SectorExportOptions exportOptions{
                    [exportResources, exportTextureSizeCache](
                        const std::string_view material)
                        -> std::optional<level::UvTextureDimensions>
                    {
                        const std::string trimmed = text::trim(material);
                        const std::string key = text::upperAscii(trimmed);
                        if (const auto found =
                                exportTextureSizeCache->find(key);
                            found != exportTextureSizeCache->end())
                        {
                            return found->second;
                        }
                        const std::optional<level::UvTextureDimensions> size =
                            assets::resolveMaterialTextureDimensions(
                                *exportResources, trimmed);
                        exportTextureSizeCache->emplace(key, size);
                        return size;
                    } };
                if (extension == ".obj")
                {
                    exported = ui::sector::exportSectorsToObj(
                        document_.level(), selected, exportOptions);
                }
                else if (extension == ".3do")
                {
                    exported = ui::sector::exportSectorsTo3do(
                        document_.level(), selected, exportOptions);
                }
                else
                {
                    exported.error = "Choose either the .obj or .3do file extension.";
                }
            }
            catch (const std::exception& ex)
            {
                exported.error = ex.what();
            }
            if (!exported)
            {
                const QString message = QString::fromStdString(exported.error);
                showSectorExportFailure(this, filename, message);
                return;
            }

            QSaveFile output(filename);
            if (!output.open(QIODevice::WriteOnly)
                || output.write(exported.text.data(),
                       static_cast<qint64>(exported.text.size()))
                    != static_cast<qint64>(exported.text.size())
                || !output.commit())
            {
                showSectorExportFailure(
                    this,
                    filename,
                    QStringLiteral(
                        "The export file could not be written atomically."));
                return;
            }

            qInfo().noquote()
                << "Exported" << static_cast<qulonglong>(selected.size())
                << "Sector(s) to" << filename;
        });
    sectorMenu->addAction(exportSectorAction_);

    addSectorShapeMenu_ = new ui::shortcuts::KeySequenceDisplayMenu("Add Sector &Shape", this);
    addSectorShapeMenu_->setObjectName("edit.add_sector_shape");
    connect(addSectorShapeMenu_, &QMenu::aboutToShow, this, [this]()
        {
            rebuildAddSectorShapeMenu();
        });
    rebuildAddSectorShapeMenu();
    // The Add Sector action opens an options overlay with the same complete
    // shape selector. Keep this menu as the viewport tool-button drop-down,
    // but do not duplicate it as a second Edit > Sector command.

    templateEditorAction_ = new QAction("Template &Editor...", this);
    templateEditorAction_->setObjectName("edit.template_editor");
    connect(templateEditorAction_, &QAction::triggered, this, [this]()
        {
            showTemplateEditorDialog();
        });
    thingMenu->addAction(templateEditorAction_);
    thingMenu->addSeparator();

    addThingAction_ = new QAction("Show/Hide Thing &Placement Tool...", this);
    addThingAction_->setObjectName("edit.add_thing");
    addThingAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::actionAddThing));
    addThingAction_->setShortcut(QKeySequence(Qt::Key_A));
    addThingAction_->setShortcutContext(Qt::WidgetShortcut);
    addThingAction_->setAutoRepeat(false);
    addThingAction_->setCheckable(true);
    addThingAction_->setToolTip("Show or hide the Thing placement tool while Thing edit mode is active");
    connect(addThingAction_, &QAction::triggered, this, [this](const bool checked)
        {
            setThingPlacementPanelVisible(checked);
        });
    thingMenu->addAction(addThingAction_);

    addLightAction_ = new QAction("Add &Light", this);
    addLightAction_->setObjectName("edit.add_light");
    addLightAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::actionAddLight));
    addLightAction_->setShortcut(QKeySequence(Qt::Key_A));
    addLightAction_->setShortcutContext(Qt::WidgetShortcut);
    addLightAction_->setAutoRepeat(false);
    addLightAction_->setCheckable(true);
    connect(addLightAction_, &QAction::triggered, this, [this]()
        {
            const PlacementMode newMode = (placementMode_ == PlacementMode::light) ? PlacementMode::none : PlacementMode::light;
            setPlacementMode(newMode);
    });
    lightMenu->addAction(addLightAction_);
    lightMenu->addAction(recalcLightingAction_);

    autoRecalculateLightingAction_ = new QAction("&Auto Recalculate Lighting", this);
    autoRecalculateLightingAction_->setObjectName("edit.auto_recalculate_lighting");
    autoRecalculateLightingAction_->setCheckable(true);
    autoRecalculateLightingAction_->setChecked(
        settingsStore().value("lighting/auto_recalculate", false).toBool());
    autoRecalculateLightingAction_->setToolTip(
        "Automatically recalculate the affected baked-lighting area after a light changes. "
        "Changes are debounced and calculated in the background.");
    autoRecalculateLightingAction_->setStatusTip(autoRecalculateLightingAction_->toolTip());
    connect(autoRecalculateLightingAction_, &QAction::toggled, this, [this](const bool checked)
        {
            setLightBakeAutoEnabled(checked);
        });
    lightMenu->addAction(autoRecalculateLightingAction_);
    lightMenu->addSeparator();
    lightMenu->addAction(lightDeleteMenuAction_);

    thingMenu->addAction(thingDeleteMenuAction_);

    addCogAction_ = new QAction("Add &COG...", this);
    addCogAction_->setObjectName("edit.add_cog");
    addCogAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G));
    connect(addCogAction_, &QAction::triggered, this, [this]()
        {
            auto pickExtractionRoot = [&]() -> std::optional<std::filesystem::path>
            {
                if (!resourceRoots_.empty())
                {
                    // Prefer a Resource/ root when present (especially for IJIM, where Resource/ is the canonical disk root).
                    for (const std::filesystem::path& root : resourceRoots_)
                    {
                        if (root.filename() == "Resource")
                        {
                            return root;
                        }
                    }
                    return resourceRoots_.front();
                }
                if (!resources_.roots().empty())
                {
                    for (const std::filesystem::path& root : resources_.roots())
                    {
                        if (root.filename() == "Resource")
                        {
                            return root;
                        }
                    }
                    return resources_.roots().front();
                }
                return std::nullopt;
            };

            const auto picked = ui::pickCogScript(this, resources_, pickExtractionRoot());
            if (!picked || picked->scriptName.empty())
            {
                return;
            }

            const std::string scriptName = picked->scriptName;

            if (scriptName.empty())
            {
                return;
            }
            if ( !document_.level().canAllocateIds(1) )
            {
                constexpr const char* message =
                    "Cannot add COG: entity ID space exhausted";
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

            const std::vector<CogValue> values = ::sed::assets::loadCogSymbolsWithDefaults(resources_, scriptName, false);
            std::optional<std::int32_t> newIndex{};

            (void)document_.applyUndoable(
                "COG: Added",
                editor::DocumentDirty::Cogs | editor::DocumentDirty::Structure,
                [&](editor::DocumentState& state) -> bool
                {
                    Cog& cog = state.level.newCog();
                    cog.name = scriptName;
                    cog.values = values;
                    newIndex = cog.num;

                    state.selection.clear();
                    state.selection.cogs.insert(cog.num);
                    return true;
                });

            if (newIndex && outlinerTabs_)
            {
                outlinerTabs_->setCurrentIndex(2);
            }

            if (glView_)
            {
                glView_->setSelection(-1, -1, -1, -1);
                glView_->setHighlightedSelection(document_.selection());
            }

            syncCogScriptWatches();
            rebuildOutliner();
            rebuildProperties();
            updateEditActions();
        });
    cogMenu->addAction(addCogAction_);

    duplicateCogAction_ = new QAction("&Duplicate COG", this);
    duplicateCogAction_->setObjectName("edit.duplicate_cog");
    duplicateCogAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    connect(duplicateCogAction_, &QAction::triggered, this, [this]()
        {
            std::vector<std::int32_t> sources{};
            sources.reserve(document_.selection().cogs.size());
            for (const std::int32_t idx : document_.selection().cogs)
            {
                sources.push_back(idx);
            }
            std::sort(sources.begin(), sources.end());
            sources.erase(std::unique(sources.begin(), sources.end()), sources.end());
            std::erase_if(
                sources,
                [this](const std::int32_t index)
                {
                    return index < 0
                        || static_cast<std::size_t>(index)
                            >= document_.level().cogs.size();
                });

            if (sources.empty())
            {
                return;
            }
            if ( !document_.level().canAllocateIds(sources.size()) )
            {
                constexpr const char* message =
                    "Cannot duplicate COGs: entity ID space exhausted";
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

            std::vector<std::int32_t> newIndices{};
            const bool applied = document_.applyUndoable(
                ui::undo::action("COG", sources.size(), "Duplicated"),
                editor::DocumentDirty::Cogs | editor::DocumentDirty::Structure,
                [this, &sources, &newIndices](editor::DocumentState& state) -> bool
                {
                    newIndices.clear();
                    for (const std::int32_t sourceIndex : sources)
                    {
                        if (sourceIndex < 0 || static_cast<std::size_t>(sourceIndex) >= state.level.cogs.size())
                        {
                            continue;
                        }

                        const Cog& src = state.level.cogs[static_cast<std::size_t>(sourceIndex)];
                        // newCog() may reallocate the COG vector. Snapshot the
                        // source identity and payload before growing it so
                        // this loop never dereferences an invalid reference.
                        const std::string sourceName = src.name;
                        const std::vector<CogValue> sourceValues = src.values;
                        // Duplicate the authored payload exactly. Reloading a
                        // script here would discard unresolved/raw parameters
                        // and can change the result of a prior undo.
                        std::vector<CogValue> values = sourceValues;

                        Cog& dst = state.level.newCog();
                        dst.name = sourceName;
                        dst.values = std::move(values);
                        newIndices.push_back(dst.num);
                    }

                    if (newIndices.empty())
                    {
                        return false;
                    }

                    state.selection.clear();
                    for (const std::int32_t idx : newIndices)
                    {
                        state.selection.cogs.insert(idx);
                    }
                    return true;
                });

            if (!applied)
            {
                return;
            }

            if (outlinerTabs_)
            {
                outlinerTabs_->setCurrentIndex(2);
            }

            if (glView_)
            {
                glView_->setSelection(-1, -1, -1, -1);
                glView_->setHighlightedSelection(document_.selection());
            }

            syncCogScriptWatches();
            rebuildOutliner();
            rebuildProperties();
            updateEditActions();
        });
    cogMenu->addAction(duplicateCogAction_);
    cogMenu->addSeparator();
    cogMenu->addAction(cogDeleteMenuAction_);

    extrudeSurfaceAction_ = new QAction("E&xtrude", this);
    extrudeSurfaceAction_->setObjectName("surface.extrude");
    extrudeSurfaceAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::actionExtrude));
    extrudeSurfaceAction_->setCheckable(true);
    extrudeSurfaceAction_->setToolTip("Extrude surface");
    connect(extrudeSurfaceAction_, &QAction::triggered, this, [this](const bool checked)
        {
            if (checked)
            {
                beginSurfaceExtrudePreview();
                return;
            }

            if (surfaceExtrudePreviewActive_)
            {
                cancelSurfaceExtrudePreview();
            }
        });
    surfaceMenu->addAction(extrudeSurfaceAction_);

    makeAdjoinAction_ = new QAction("&Adjoin", this);
    makeAdjoinAction_->setObjectName("surface.make_adjoin");
    makeAdjoinAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::actionAdjoin));
    makeAdjoinAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_J));
    makeAdjoinAction_->setToolTip("Adjoin surfaces");
    connect(makeAdjoinAction_, &QAction::triggered, this, [this]()
        {
            if (document_.selection().surfaces.empty())
            {
                QMessageBox::information(this, "Adjoin surfaces", "Select a surface first.");
                return;
            }

            std::vector<SurfaceRef> selectedSurfaces{};
            selectedSurfaces.reserve(document_.selection().surfaces.size());
            for (const SurfaceRef ref : document_.selection().surfaces)
            {
                selectedSurfaces.push_back(ref);
            }
            std::sort(selectedSurfaces.begin(), selectedSurfaces.end(), [](const SurfaceRef& a, const SurfaceRef& b)
                {
                    if (a.sector != b.sector)
                    {
                        return a.sector < b.sector;
                    }
                    return a.surface < b.surface;
                });

            const bool tryConnectTwoSelected =
                selectedSurfaces.size() == 2
                && document_.selection().sectors.empty()
                && document_.selection().things.empty()
                && document_.selection().lights.empty()
                && document_.selection().cogs.empty()
                && selectedSurfaces[0].sector != selectedSurfaces[1].sector;

            const SurfaceRef surfaceRef = selectedSurfaces.front();

            const bool applied = document_.applyUndoable(
                "Surfaces: Adjoined",
                editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure,
                [&](editor::DocumentState& state) -> bool
                {
                    if (tryConnectTwoSelected)
                    {
                        if (::sed::level::levelConnectSurfaces(state.level, selectedSurfaces[0], selectedSurfaces[1]))
                        {
                            state.selection.clear();
                            return true;
                        }
                    }

                    if (!::sed::level::levelMakeAdjoin(state.level, surfaceRef, true))
                    {
                        return false;
                    }

                    state.selection.clear();
                    return true;
                });

            if (!applied)
            {
                QMessageBox::warning(
                    this,
                    "Adjoin surfaces failed",
                    "No compatible surface was found, or the surfaces could not be auto-cut into an adjoin.");
                return;
            }

            invalidateSurfaceSelectionContextForTopologyEdit();

            glView_->notifyLevelGeometryChanged();
            glView_->setSelection(-1, -1, -1, -1);
            glView_->setHighlightedSelection(document_.selection());
            rebuildOutliner();
            rebuildProperties();
            updateEditActions();
        });
    surfaceMenu->addAction(makeAdjoinAction_);

    unAdjoinAction_ = new QAction("Un&adjoin", this);
    unAdjoinAction_->setObjectName("surface.unadjoin");
    unAdjoinAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::actionUnadjoin));
    unAdjoinAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_J));
    unAdjoinAction_->setToolTip("Unadjoin surfaces");
    connect(unAdjoinAction_, &QAction::triggered, this, [this]()
        {
            if (document_.selection().surfaces.empty())
            {
                QMessageBox::information(this, "Unadjoin surfaces", "Select a surface first.");
                return;
            }

            const SurfaceRef surfaceRef = *document_.selection().surfaces.begin();

            const bool applied = document_.applyUndoable(
                "Surface: Unadjoined",
                editor::DocumentDirty::LevelGeometry,
                [&](editor::DocumentState& state) -> bool
                {
                    if (!::sed::level::levelUnadjoinSurface(state.level, surfaceRef))
                    {
                        return false;
                    }

                    state.selection.clear();
                    return true;
                });

            if (!applied)
            {
                QMessageBox::warning(this, "Unadjoin surfaces failed", "Surface is not adjoined or operation not permitted.");
                return;
            }

            glView_->notifyLevelGeometryChanged();
            glView_->setSelection(-1, -1, -1, -1);
            glView_->setHighlightedSelection(document_.selection());
            rebuildOutliner();
            rebuildProperties();
            updateEditActions();
        });
    surfaceMenu->addAction(unAdjoinAction_);

    connectSurfacesAction_ = new QAction("Connect &Surfaces", this);
    connectSurfacesAction_->setObjectName("surface.connect");
    connectSurfacesAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::toolConnect));
    connectSurfacesAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    connectSurfacesAction_->setToolTip("Connect surfaces");
    connect(connectSurfacesAction_, &QAction::triggered, this, [this]()
        {
            if (document_.selection().surfaces.size() != 2 || !document_.selection().sectors.empty()
                || !document_.selection().things.empty() || !document_.selection().lights.empty() || !document_.selection().cogs.empty())
            {
                QMessageBox::information(this, "Connect surfaces", "Select exactly two surfaces first.");
                return;
            }

            std::vector<SurfaceRef> surfaces{};
            surfaces.reserve(2);
            for (const SurfaceRef ref : document_.selection().surfaces)
            {
                surfaces.push_back(ref);
            }

            std::sort(surfaces.begin(), surfaces.end(), [](const SurfaceRef& a, const SurfaceRef& b)
                {
                    if (a.sector != b.sector)
                    {
                        return a.sector < b.sector;
                    }
                    return a.surface < b.surface;
                });

            if (surfaces.size() != 2 || surfaces[0].sector == surfaces[1].sector)
            {
                QMessageBox::information(this, "Connect surfaces", "Select two surfaces in different sectors.");
                return;
            }

            const bool applied = document_.applyUndoable(
                "Surfaces: Connected",
                editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure,
                [&](editor::DocumentState& state) -> bool
                {
                    if (!::sed::level::levelConnectSurfaces(state.level, surfaces[0], surfaces[1]))
                    {
                        return false;
                    }

                    state.selection.clear();
                    return true;
                });

            if (!applied)
            {
                QMessageBox::warning(this, "Connect surfaces failed", "Surfaces are not compatible or do not overlap.");
                return;
            }

            invalidateSurfaceSelectionContextForTopologyEdit();

            glView_->notifyLevelGeometryChanged();
            glView_->setSelection(-1, -1, -1, -1);
            glView_->setHighlightedSelection(document_.selection());
            rebuildOutliner();
            rebuildProperties();
            updateEditActions();
        });
    surfaceMenu->addAction(connectSurfacesAction_);

    setupUvEditorSurfaceActions(surfaceMenu);
    surfaceMenu->addSeparator();
    surfaceMenu->addAction(surfaceDeleteMenuAction_);
    surfaceMenu->addAction(forceDeleteSurfaceVertexAction_);

    connectSectorsAction_ = new QAction("Join Se&ctors", this);
    connectSectorsAction_->setObjectName("sector.connect");
    connectSectorsAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::toolConnect));
    connectSectorsAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));
    connectSectorsAction_->setToolTip("Join exactly two selected sectors");
    connect(connectSectorsAction_, &QAction::triggered, this, [this]()
        {
            if (document_.selection().sectors.size() != 2 || !document_.selection().surfaces.empty()
                || !document_.selection().things.empty() || !document_.selection().lights.empty() || !document_.selection().cogs.empty())
            {
                QMessageBox::information(this, "Join sectors", "Select exactly two sectors first.");
                return;
            }

            auto it = document_.selection().sectors.begin();
            const std::int32_t a = *it++;
            const std::int32_t b = *it;

            const std::int32_t keep = std::min(a, b);
            const std::int32_t remove = std::max(a, b);

            const bool applied = document_.applyUndoable(
                "Sectors: Joined",
                editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure,
                [&](editor::DocumentState& state) -> bool
                {
                    if (!::sed::level::levelConnectSectors(state.level, keep, remove))
                    {
                        return false;
                    }

                    state.selection.clear();
                    return true;
                });

            if (!applied)
            {
                qDebug().noquote()
                    << "Direct two-Sector join rejected for Sector indices"
                    << keep << "and" << remove
                    << ": no compatible overlap/adjoin-safe result";
                QMessageBox::warning(this, "Join sectors failed", "Sectors could not be joined.");
                return;
            }

            invalidateSurfaceSelectionContextForTopologyEdit();

            glView_->notifyLevelGeometryChanged();
            glView_->setSelection(-1, -1, -1, -1);
            glView_->setHighlightedSelection(document_.selection());
            rebuildOutliner();
            rebuildProperties();
            updateEditActions();
        });
    sectorMenu->addAction(connectSectorsAction_);
    // Step 2: delegate the transform submenu once the entity submenus exist.
    setupTransformMenu(transformMenu);
}

} // namespace sed
