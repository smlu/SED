#include "main_window/common.hpp"

namespace sed
{

/// Create the Mirror action and wire it to the document transform pipeline.
void MainWindow::setupMirrorTransformAction(QMenu* transformMenu)
{
    if (!transformMenu)
    {
        return;
    }

    // Step 1: compute the current selection center, ask for an axis, then mirror selected entities.
    mirrorAction_ = new QAction("&Mirror...", this);
    mirrorAction_->setObjectName("transform.mirror");
    mirrorAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    connect(mirrorAction_, &QAction::triggered, this, [this]()
        {
            if (document_.selection().sectors.empty() && document_.selection().things.empty() && document_.selection().lights.empty())
            {
                QMessageBox::information(this, "Mirror", "Select a sector, thing, or light first.");
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
            const QString axisLabel = QInputDialog::getItem(this, "Mirror", "Axis", axes, 0, false, &ok);
            if (!ok)
            {
                return;
            }

            ::sed::level::Axis axisEnum = ::sed::level::Axis::x;
            TVector planeNormal{ 1.0, 0.0, 0.0 };

            if (axisLabel == "Y")
            {
                axisEnum = ::sed::level::Axis::y;
                planeNormal = TVector{ 0.0, 1.0, 0.0 };
            }
            else if (axisLabel == "Z")
            {
                axisEnum = ::sed::level::Axis::z;
                planeNormal = TVector{ 0.0, 0.0, 1.0 };
            }

            ::sed::Level candidate = document_.level();

            if (!document_.selection().sectors.empty())
            {
                std::vector<std::int32_t> sectors{};
                sectors.reserve(document_.selection().sectors.size());
                for (const std::int32_t index : document_.selection().sectors)
                {
                    sectors.push_back(index);
                }
                if ( !::sed::level::levelFlipSectorsChecked(
                        candidate, sectors, cx, cy, cz, axisEnum) )
                {
                    QMessageBox::warning(
                        this,
                        "Mirror",
                        "Mirror was cancelled because the geometry transform could not be prepared.");
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
                ::sed::level::levelFlipThings(candidate, things, cx, cy, cz, axisEnum);
            }

            if (!document_.selection().lights.empty())
            {
                std::vector<std::int32_t> lights{};
                lights.reserve(document_.selection().lights.size());
                for (const std::int32_t index : document_.selection().lights)
                {
                    lights.push_back(index);
                }
                ::sed::level::levelFlipLightsOverPlane(candidate, lights, planeNormal, cx, cy, cz);
            }

            editor::DocumentDirty dirty = editor::DocumentDirty::None;
            if (!document_.selection().sectors.empty())
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

            document_.pushUndo(ui::undo::selectionAction(document_.selection(), "Mirrored"), dirty);
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
    transformMenu->addAction(mirrorAction_);

}

} // namespace sed
