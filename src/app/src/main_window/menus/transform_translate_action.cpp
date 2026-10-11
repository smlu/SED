#include "ui/components/precise_numeric_input.hpp"
#include "main_window/common.hpp"
#include "render/transform/geometry_transform_preview_utils.hpp"

namespace sed
{

/// Create the Translate action and wire it to the document transform pipeline.
void MainWindow::setupTranslateTransformAction(QMenu* transformMenu)
{
    if (!transformMenu)
    {
        return;
    }

    // Step 1: ask for a movement vector, copy the current selection, then translate every selected entity.
    translateAction_ = new QAction("&Translate...", this);
    translateAction_->setObjectName("transform.translate");
    translateAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(translateAction_, &QAction::triggered, this, [this]()
        {
            if (document_.selection().sectors.empty() && document_.selection().surfaces.empty()
                && document_.selection().things.empty() && document_.selection().lights.empty())
            {
                QMessageBox::information(this, "Translate", "Select a sector, surface, thing, or light first.");
                return;
            }

            bool ok = false;

            const double dx = ui::preciseNumericInput(this, "Translate", "dX", 0.0, -1'000'000.0, 1'000'000.0, &ok);
            if (!ok)
            {
                return;
            }
            const double dy = ui::preciseNumericInput(this, "Translate", "dY", 0.0, -1'000'000.0, 1'000'000.0, &ok);
            if (!ok)
            {
                return;
            }
            const double dz = ui::preciseNumericInput(this, "Translate", "dZ", 0.0, -1'000'000.0, 1'000'000.0, &ok);
            if (!ok)
            {
                return;
            }

            if ( dx == 0.0 && dy == 0.0 && dz == 0.0 )
            {
                return;
            }

            std::vector<std::int32_t> sectors{};
            sectors.reserve(document_.selection().sectors.size());
            for (const std::int32_t index : document_.selection().sectors)
            {
                sectors.push_back(index);
            }

            std::vector<SurfaceRef> surfaces{};
            surfaces.reserve(document_.selection().surfaces.size());
            for (const SurfaceRef ref : document_.selection().surfaces)
            {
                surfaces.push_back(ref);
            }

            std::vector<std::int32_t> things{};
            things.reserve(document_.selection().things.size());
            for (const std::int32_t index : document_.selection().things)
            {
                things.push_back(index);
            }

            std::vector<std::int32_t> lights{};
            lights.reserve(document_.selection().lights.size());
            for (const std::int32_t index : document_.selection().lights)
            {
                lights.push_back(index);
            }

            editor::DocumentDirty dirty = editor::DocumentDirty::None;
            if (!sectors.empty() || !surfaces.empty())
            {
                dirty |= editor::DocumentDirty::LevelGeometry;
            }
            if (!things.empty())
            {
                dirty |= editor::DocumentDirty::Things;
            }
            if (!lights.empty())
            {
                dirty |= editor::DocumentDirty::Lights;
            }

            const bool preserveUvs = glView_
                ? glView_->preserveConnectedSurfaceUvsEnabled()
                : settingsStore()
                    .value("viewport/preserve_connected_surface_uvs", true)
                    .toBool();
            const std::string undoLabel = ui::undo::selectionAction(document_.selection(), "Moved");
            bool thingTranslationRejected = false;
            bool geometryTranslationRejected = false;
            const bool applied = document_.applyUndoable(undoLabel, dirty, [&](editor::DocumentState& state) -> bool
                {
                    const level::ConnectedSurfaceUvPlan uvPlan =
                        preserveUvs && (!sectors.empty() || !surfaces.empty())
                        ? render::buildConnectedSurfaceUvTransformPlan(
                            state.level, sectors, surfaces, false)
                        : level::ConnectedSurfaceUvPlan{};
                    if (preserveUvs && (!sectors.empty() || !surfaces.empty())
                        && !uvPlan.valid)
                    {
                        return false;
                    }
                    if (!sectors.empty())
                    {
                        if ( !::sed::level::levelTranslateSectorsChecked(
                                state.level, sectors, dx, dy, dz) )
                        {
                            geometryTranslationRejected = true;
                            return false;
                        }
                    }
                    if (!surfaces.empty())
                    {
                        if ( !::sed::level::levelTranslateSurfacesChecked(
                                state.level, surfaces, dx, dy, dz) )
                        {
                            geometryTranslationRejected = true;
                            return false;
                        }
                    }
                    if (!things.empty())
                    {
                        if ( !::sed::level::levelTranslateThings(state.level, things, dx, dy, dz, true) )
                        {
                            thingTranslationRejected = true;
                            return false;
                        }
                    }
                    if (!lights.empty())
                    {
                        ::sed::level::levelTranslateLights(state.level, lights, dx, dy, dz);
                    }
                    if (preserveUvs && (!sectors.empty() || !surfaces.empty()))
                    {
                        const level::ConnectedSurfaceUvEvaluation evaluation =
                            level::evaluateConnectedSurfaceUvPlan(
                                uvPlan, state.level);
                        if (!level::applyConnectedSurfaceUvEvaluation(
                                state.level, evaluation))
                        {
                            return false;
                        }
                    }

                    state.selection.clear();
                    return true;
                });
            if (!applied)
            {
                QMessageBox::warning(
                    this,
                    "Translate",
                    geometryTranslationRejected
                        ? "The transform was cancelled because the selected geometry could not be translated safely."
                        : thingTranslationRejected
                        ? "The selected Things have no finite representable translation."
                        : "The transform was cancelled because connected surface UVs could not be preserved.");
                return;
            }

            if (glView_)
            {
                const bool levelChanged = editor::hasAny(dirty, editor::DocumentDirty::LevelGeometry);
                const bool thingsChanged = editor::hasAny(dirty, editor::DocumentDirty::Things);
                const bool lightsChanged = editor::hasAny(dirty, editor::DocumentDirty::Lights);

                if ((dirty == editor::DocumentDirty::None) || (levelChanged && thingsChanged))
                {
                    glView_->notifyAllMeshesChanged();
                }
                else
                {
                    if (levelChanged)
                    {
                        glView_->notifyLevelGeometryChanged();
                    }
                    if (thingsChanged)
                    {
                        glView_->notifyThingsChanged();
                    }
                }

                if (lightsChanged)
                {
                    glView_->notifyLightsChanged();
                }

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
    transformMenu->addAction(translateAction_);

}

} // namespace sed
