#include "ui/components/precise_numeric_input.hpp"
#include "main_window/common.hpp"
#include "render/transform/geometry_transform_preview_utils.hpp"
#include "sed/level/geometry.hpp"

#include <algorithm>

namespace sed
{
namespace
{

bool isValidSurfaceRef(const Level& level, const SurfaceRef ref) noexcept
{
    if (ref.sector < 0 || ref.surface < 0
        || static_cast<std::size_t>(ref.sector) >= level.sectors.size())
    {
        return false;
    }

    return static_cast<std::size_t>(ref.surface)
        < level.sectors[static_cast<std::size_t>(ref.sector)].surfaces.size();
}

} // namespace

/// Create the Scale action and wire it to the document transform pipeline.
void MainWindow::setupScaleTransformAction(QMenu* transformMenu)
{
    if (!transformMenu)
    {
        return;
    }

    // Step 1: compute the current selection center, ask for a factor, then scale selected entities.
    scaleAction_ = new QAction("&Scale...", this);
    scaleAction_->setObjectName("transform.scale");
    scaleAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
    connect(scaleAction_, &QAction::triggered, this, [this]()
        {
            if (document_.selection().sectors.empty() && document_.selection().surfaces.empty()
                && document_.selection().things.empty() && document_.selection().lights.empty())
            {
                QMessageBox::information(this, "Scale", "Select a sector, surface, thing, or light first.");
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
            const double factor = ui::preciseNumericInput(this, "Scale", "Factor", 1.0, -1'000'000.0, 1'000'000.0, &ok);
            if (!ok)
            {
                return;
            }

            bool scaleTextures = false;
            if (!document_.selection().sectors.empty() || !document_.selection().surfaces.empty())
            {
                const auto result = QMessageBox::question(this, "Scale", "Scale textures too?", QMessageBox::Yes | QMessageBox::No);
                scaleTextures = result == QMessageBox::Yes;
            }

            ScaleData sd{};
            sd.cx = cx;
            sd.cy = cy;
            sd.cz = cz;
            sd.sfactor = factor;
            sd.how = ScaleHow::xyz;

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
            const bool preserveUvs = glView_
                ? glView_->preserveConnectedSurfaceUvsEnabled()
                : settingsStore()
                    .value("viewport/preserve_connected_surface_uvs", true)
                    .toBool();
            const level::ConnectedSurfaceUvPlan uvPlan =
                preserveUvs && (!sectors.empty() || !surfaces.empty())
                ? render::buildConnectedSurfaceUvTransformPlan(
                    document_.level(), sectors, surfaces, true)
                : level::ConnectedSurfaceUvPlan{};
            if (preserveUvs && (!sectors.empty() || !surfaces.empty())
                && !uvPlan.valid)
            {
                QMessageBox::warning(
                    this,
                    "Scale",
                    "The transform was cancelled because connected surface UVs could not be preserved.");
                return;
            }

            ::sed::Level candidate = document_.level();

            if (!sectors.empty())
            {
                if ( !::sed::level::levelScaleSectorsChecked(
                        candidate, sectors, sd, scaleTextures) )
                {
                    QMessageBox::warning(
                        this,
                        "Scale",
                        "Scale was cancelled because the geometry transform could not be prepared.");
                    return;
                }
            }

            if (!surfaces.empty())
            {
                if ( !::sed::level::levelScaleSurfacesChecked(
                        candidate, surfaces, sd, scaleTextures) )
                {
                    QMessageBox::warning(
                        this,
                        "Scale",
                        "Scale was cancelled because the geometry transform could not be prepared.");
                    return;
                }
            }

            if (!document_.selection().things.empty())
            {
                std::vector<std::int32_t> things{};
                things.reserve(document_.selection().things.size());
                for (const std::int32_t index : document_.selection().things)
                {
                    things.push_back(index);
                }
                ::sed::level::levelScaleThings(candidate, things, sd);
            }

            if (!document_.selection().lights.empty())
            {
                std::vector<std::int32_t> lights{};
                lights.reserve(document_.selection().lights.size());
                for (const std::int32_t index : document_.selection().lights)
                {
                    lights.push_back(index);
                }
                ::sed::level::levelScaleLights(candidate, lights, sd);
            }

            if (preserveUvs && (!sectors.empty() || !surfaces.empty()))
            {
                const level::ConnectedSurfaceUvEvaluation evaluation =
                    level::evaluateConnectedSurfaceUvPlan(uvPlan, candidate);
                if (!level::applyConnectedSurfaceUvEvaluation(
                        candidate, evaluation))
                {
                    QMessageBox::warning(
                        this,
                        "Scale",
                        "The transform was cancelled because connected surface UVs could not be preserved.");
                    return;
                }
            }

            if (!sectors.empty() || !surfaces.empty())
            {
                std::vector<std::int32_t> affectedSectors{};
                affectedSectors.reserve(sectors.size() + surfaces.size() * 2u);
                const auto includeSector = [&affectedSectors, &level](const std::int32_t sectorIndex)
                {
                    if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
                    {
                        return;
                    }
                    if (std::find(affectedSectors.begin(), affectedSectors.end(), sectorIndex)
                        == affectedSectors.end())
                    {
                        affectedSectors.push_back(sectorIndex);
                    }
                };
                const auto includeAdjoinSector = [&includeSector, &level](const Surface& surface)
                {
                    if (surface.adjoin && isValidSurfaceRef(level, *surface.adjoin))
                    {
                        includeSector(surface.adjoin->sector);
                    }
                };

                for (const std::int32_t sectorIndex : sectors)
                {
                    if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
                    {
                        continue;
                    }
                    includeSector(sectorIndex);
                    for (const Surface& surface : level.sectors[static_cast<std::size_t>(sectorIndex)].surfaces)
                    {
                        includeAdjoinSector(surface);
                    }
                }
                for (const SurfaceRef ref : surfaces)
                {
                    if (!isValidSurfaceRef(level, ref))
                    {
                        continue;
                    }
                    includeSector(ref.sector);
                    includeAdjoinSector(
                        level.sectors[static_cast<std::size_t>(ref.sector)]
                            .surfaces[static_cast<std::size_t>(ref.surface)]);
                }

                if ( !render::preservesAffectedScaleGeometry(
                        level, candidate, affectedSectors, factor < 0.0) )
                {
                    QMessageBox::warning(
                        this,
                        "Scale",
                        "Scale was cancelled because it would produce degenerate geometry.");
                    return;
                }
            }

            editor::DocumentDirty dirty = editor::DocumentDirty::None;
            if (!document_.selection().sectors.empty() || !document_.selection().surfaces.empty())
            {
                dirty |= editor::DocumentDirty::LevelGeometry;
            }
            if (!document_.selection().things.empty())
            {
                dirty |= editor::DocumentDirty::Things;
            }
            if (!document_.selection().lights.empty())
            {
                dirty |= editor::DocumentDirty::Lights;
            }

            document_.pushUndo(ui::undo::selectionAction(document_.selection(), "Scaled"), dirty);
            document_.level() = std::move(candidate);
            document_.selection().clear();

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
    transformMenu->addAction(scaleAction_);

}

} // namespace sed
