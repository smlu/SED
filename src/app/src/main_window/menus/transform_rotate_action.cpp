#include "ui/components/precise_numeric_input.hpp"
#include "main_window/common.hpp"
#include "render/transform/geometry_transform_preview_utils.hpp"

namespace sed
{

/// Create the Rotate action and wire it to the document transform pipeline.
void MainWindow::setupRotateTransformAction(QMenu* transformMenu)
{
    if (!transformMenu)
    {
        return;
    }

    // Step 1: compute the current selection center, ask for an axis/angle, then rotate selected entities.
    rotateAction_ = new QAction("&Rotate...", this);
    rotateAction_->setObjectName("transform.rotate");
    rotateAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(rotateAction_, &QAction::triggered, this, [this]()
        {
            if (document_.selection().sectors.empty() && document_.selection().surfaces.empty()
                && document_.selection().things.empty() && document_.selection().lights.empty())
            {
                QMessageBox::information(this, "Rotate", "Select a sector, surface, thing, or light first.");
                return;
            }

            const Level& level = document_.level();

            double sx = 0.0;
            double sy = 0.0;
            double sz = 0.0;
            std::int32_t count = 0;

            auto addPoint = [&](const double x, const double y, const double z)
            {
                sx += x;
                sy += y;
                sz += z;
                ++count;
            };

            for (const std::int32_t sectorIndex : document_.selection().sectors)
            {
                if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
                {
                    continue;
                }

                const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
                for (const Vertex& v : sector.vertices)
                {
                    addPoint(v.x, v.y, v.z);
                }
            }

            for (const SurfaceRef ref : document_.selection().surfaces)
            {
                if (ref.sector < 0 || ref.surface < 0 || static_cast<std::size_t>(ref.sector) >= level.sectors.size())
                {
                    continue;
                }

                const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
                if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
                {
                    continue;
                }

                const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
                for (const std::int32_t vi : surface.vertices)
                {
                    if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
                    {
                        continue;
                    }

                    const Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
                    addPoint(v.x, v.y, v.z);
                }
            }

            for (const std::int32_t thingIndex : document_.selection().things)
            {
                if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= level.things.size())
                {
                    continue;
                }

                const Thing& thing = level.things[static_cast<std::size_t>(thingIndex)];
                addPoint(thing.x, thing.y, thing.z);
            }

            for (const std::int32_t lightIndex : document_.selection().lights)
            {
                if (lightIndex < 0 || static_cast<std::size_t>(lightIndex) >= level.lights.size())
                {
                    continue;
                }

                const SedLight& light = level.lights[static_cast<std::size_t>(lightIndex)];
                addPoint(light.position.x, light.position.y, light.position.z);
            }

            const double cx = (count > 0) ? (sx / static_cast<double>(count)) : 0.0;
            const double cy = (count > 0) ? (sy / static_cast<double>(count)) : 0.0;
            const double cz = (count > 0) ? (sz / static_cast<double>(count)) : 0.0;

            bool ok = false;
            const QStringList axes{ "X", "Y", "Z" };
            const QString axisLabel = QInputDialog::getItem(this, "Rotate", "Axis", axes, 2, false, &ok);
            if (!ok)
            {
                return;
            }

            const double angleDeg = ui::preciseNumericInput(this, "Rotate", "Angle (deg)", 90.0, -360.0, 360.0, &ok);
            if (!ok)
            {
                return;
            }

            TVector axis{};
            if (axisLabel == "X")
            {
                axis = TVector{ 1.0, 0.0, 0.0 };
            }
            else if (axisLabel == "Y")
            {
                axis = TVector{ 0.0, 1.0, 0.0 };
            }
            else
            {
                axis = TVector{ 0.0, 0.0, 1.0 };
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
            const std::string undoLabel = ui::undo::selectionAction(document_.selection(), "Rotated");
            bool geometryRotationRejected = false;
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
                        if ( !::sed::level::levelRotateSectorsChecked(
                                state.level, sectors, axis, cx, cy, cz, angleDeg) )
                        {
                            geometryRotationRejected = true;
                            return false;
                        }
                    }
                    if (!surfaces.empty())
                    {
                        if ( !::sed::level::levelRotateSurfacesChecked(
                                state.level, surfaces, axis, cx, cy, cz, angleDeg) )
                        {
                            geometryRotationRejected = true;
                            return false;
                        }
                    }
                    if (!things.empty())
                    {
                        ::sed::level::levelRotateThings(state.level, things, axis, cx, cy, cz, angleDeg, true);
                    }
                    if (!lights.empty())
                    {
                        ::sed::level::levelRotateLights(state.level, lights, axis, cx, cy, cz, angleDeg);
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
                    "Rotate",
                    geometryRotationRejected
                        ? "The transform was cancelled because the selected geometry could not be rotated safely."
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
    transformMenu->addAction(rotateAction_);

}

} // namespace sed
