#include "main_window/common.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"
#include "ui/sector/sector_shape_utils.hpp"

namespace sed
{

void MainWindow::saveSelectedSectorAsShape()
{
    const editor::Selection& selection = document_.selection();
    const bool hasSingleSectorOnly = selection.sectors.size() == 1
        && selection.surfaces.empty()
        && selection.surfaceVertices.empty()
        && selection.things.empty()
        && selection.frames.empty()
        && selection.lights.empty()
        && selection.cogs.empty();
    if (!hasSingleSectorOnly)
    {
        const QString message = QStringLiteral("Select exactly one sector to save it as a shape.");
        if (glView_)
        {
            glView_->showToast(message, 2500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
        }
        if (statusBar())
        {
            statusBar()->showMessage(message, 3000);
        }
        return;
    }

    const std::int32_t sectorIndex = *selection.sectors.begin();
    const Level& level = document_.level();
    if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
    {
        const QString message = QStringLiteral("Selected sector is no longer valid.");
        if (glView_)
        {
            glView_->showToast(message, 2500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
        }
        if (statusBar())
        {
            statusBar()->showMessage(message, 3000);
        }
        return;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    const std::vector<sed::level::SectorShape> existingShapes = ui::sector::loadLegacyShapes();
    const std::string shapeName = ui::sector::uniqueShapeName(existingShapes);

    QString error{};
    const std::optional<std::filesystem::path> savedPath =
        ui::sector::saveShapeTpl(
            shapeName,
            sector,
            level.uvCoordinateSpace,
            &error);
    if (!savedPath)
    {
        const QString message = error.isEmpty() ? QStringLiteral("Could not save sector shape.") : error;
        if (glView_)
        {
            glView_->showToast(message, 3500, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
        }
        if (statusBar())
        {
            statusBar()->showMessage(message, 5000);
        }
        return;
    }

    selectedSectorShapeName_ = shapeName;
    setPlacementMode(PlacementMode::cube);
    syncAddSectorOptionsOverlay();
    if (glView_ && sectorDrawStage_ == SectorDrawStage::none)
    {
        updateSectorDrawCursorGuide(glView_->mapFromGlobal(QCursor::pos()));
    }

    QTimer::singleShot(0, this, [this]()
        {
            rebuildAddSectorShapeMenu();
        });

    const QString message = QStringLiteral("Saved shape: %1").arg(QString::fromStdString(shapeName));
    if (glView_)
    {
        glView_->showToast(message, 2200, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::General);
    }
    if (statusBar())
    {
        statusBar()->showMessage(
            QStringLiteral("%1 (%2)").arg(message, QString::fromStdString(savedPath->string())),
            5000);
    }
}

void MainWindow::cycleSelectedSectorShape()
{
    std::vector<std::string> shapeNames{};

    // Step 1: Build the same shape sequence shown by the Add Sector shape menu.
    for (const ui::sector::ShapeBuiltin& builtin : ui::sector::builtinShapes())
    {
        shapeNames.push_back(builtin.name);
    }
    for (const sed::level::SectorShape& shape : ui::sector::loadLegacyShapes())
    {
        shapeNames.push_back(shape.name);
    }

    if (shapeNames.empty())
    {
        return;
    }

    // Step 2: Advance from the current selection, falling back to the first
    // available shape if settings reference a removed custom shape.
    const std::string currentName = selectedSectorShapeName_.empty()
        ? std::string("Cube")
        : selectedSectorShapeName_;
    auto current = std::find_if(
        shapeNames.begin(),
        shapeNames.end(),
        [&currentName](const std::string& candidate)
        {
            return sed::text::iequalsAscii(candidate, currentName);
        });
    const std::size_t nextIndex = current == shapeNames.end()
        ? 0
        : (static_cast<std::size_t>(std::distance(shapeNames.begin(), current)) + 1) % shapeNames.size();

    // Step 3: Switch the placement shape through the normal Add Sector path so
    // menu state, overlays, and cursor guide stay synchronized.
    cancelSectorDrawPlacement();
    selectedSectorShapeName_ = shapeNames[nextIndex];
    setPlacementMode(PlacementMode::cube);
    syncAddSectorOptionsOverlay();
    rebuildAddSectorShapeMenu();
    if (glView_ && sectorDrawStage_ == SectorDrawStage::none)
    {
        updateSectorDrawCursorGuide(glView_->mapFromGlobal(QCursor::pos()));
        glView_->showToast(
            QStringLiteral("Add sector: %1").arg(ui::sector::shapeLabel(selectedSectorShapeName_)),
            1400,
            GlView::ToastPlacement::BottomCenter,
            GlView::ToastGroup::General);
    }
}

void MainWindow::rebuildAddSectorShapeMenu()
{
    if (!addSectorShapeMenu_)
    {
        return;
    }

    if (addSectorShapeGroup_)
    {
        delete addSectorShapeGroup_;
        addSectorShapeGroup_ = nullptr;
    }

    addSectorShapeMenu_->clear();
    addSectorShapeGroup_ = new QActionGroup(addSectorShapeMenu_);
    addSectorShapeGroup_->setExclusive(true);

    const editor::Selection& selection = document_.selection();
    const bool canSaveSelectedSector = selection.sectors.size() == 1
        && selection.surfaces.empty()
        && selection.surfaceVertices.empty()
        && selection.things.empty()
        && selection.frames.empty()
        && selection.lights.empty()
        && selection.cogs.empty();

    QAction* saveShapeAction = addSectorShapeMenu_->addAction(QStringLiteral("Save Selected Sector as Shape"));
    saveShapeAction->setToolTip(canSaveSelectedSector
        ? QStringLiteral("Save the selected sector to data/shapes.tpl")
        : QStringLiteral("Select exactly one sector to save it as a reusable shape."));
    saveShapeAction->setEnabled(canSaveSelectedSector);
    connect(saveShapeAction, &QAction::triggered, this, [this]()
        {
            saveSelectedSectorAsShape();
        });

    addSectorShapeMenu_->addSeparator();

    auto addShapeAction = [&](QMenu* menu, const std::string& shapeName, const bool legacy)
    {
        QAction* action = menu->addAction(ui::sector::shapeLabel(shapeName));
        action->setCheckable(true);
        action->setChecked(sed::text::iequalsAscii(selectedSectorShapeName_, shapeName));
        action->setData(QString::fromStdString(shapeName));
        action->setToolTip(legacy
            ? QStringLiteral("Use a saved sector shape from data/shapes.tpl")
            : QStringLiteral("Use a built-in sector primitive"));
        addSectorShapeGroup_->addAction(action);

        connect(action, &QAction::triggered, this, [this, shapeName]()
            {
                cancelSectorDrawPlacement();
                selectedSectorShapeName_ = shapeName;
                setPlacementMode(PlacementMode::cube);
                syncAddSectorOptionsOverlay();
            });
    };

    for (const ui::sector::ShapeBuiltin& builtin : ui::sector::builtinShapes())
    {
        addShapeAction(addSectorShapeMenu_, builtin.name, false);
    }

    addSectorShapeMenu_->addSeparator();

    QMenu* legacyMenu = ui::shortcuts::addKeySequenceDisplayMenu(
        addSectorShapeMenu_, QStringLiteral("Shapes"));
    const std::vector<sed::level::SectorShape> legacyShapes = ui::sector::loadLegacyShapes();
    if (legacyShapes.empty())
    {
        QAction* empty = legacyMenu->addAction(QStringLiteral("No saved shapes found"));
        empty->setEnabled(false);
    }
    else
    {
        for (const sed::level::SectorShape& shape : legacyShapes)
        {
            addShapeAction(legacyMenu, shape.name, true);
        }
    }
}

} // namespace sed
