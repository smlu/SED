#include "main_window/common.hpp"

namespace sed
{

/// Create the surface winding flip action and wire it to the document transform pipeline.
void MainWindow::setupFlipSurfaceTransformAction(QMenu* transformMenu)
{
    if (!transformMenu)
    {
        return;
    }

    // Step 1: copy the selected surfaces, flip their winding, then refresh geometry-dependent UI.
    flipSurfaceAction_ = new QAction("Flip &Surface Winding", this);
    flipSurfaceAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::actionFlip));
    flipSurfaceAction_->setToolTip("Flip surface winding");
    connect(flipSurfaceAction_, &QAction::triggered, this, [this]()
        {
            if (document_.selection().surfaces.empty())
            {
                QMessageBox::information(this, "Flip surface", "Select a surface first.");
                return;
            }

            ::sed::Level candidate = document_.level();

            std::vector<SurfaceRef> surfaces{};
            surfaces.reserve(document_.selection().surfaces.size());
            for (const SurfaceRef ref : document_.selection().surfaces)
            {
                surfaces.push_back(ref);
            }

            ::sed::level::levelFlipSurfaces(candidate, surfaces);

            document_.pushUndo(
                ui::undo::action("Surface", surfaces.size(), "Winding flipped"),
                editor::DocumentDirty::LevelGeometry);
            document_.level() = std::move(candidate);
            document_.selection().clear();

            if (glView_)
            {
                glView_->notifyLevelGeometryChanged();
                glView_->setSelection(-1, -1, -1, -1);
                glView_->setHighlightedSelection(document_.selection());
            }

            if (outliner_)
            {
                const QSignalBlocker blocker(outliner_);
                outliner_->clearSelection();
                outliner_->setCurrentItem(nullptr);
            }
            rebuildProperties();
            updateEditActions();
        });
    transformMenu->addAction(flipSurfaceAction_);
}

} // namespace sed
