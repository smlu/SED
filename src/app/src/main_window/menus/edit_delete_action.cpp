#include "main_window/common.hpp"
#include "app_logging.hpp"
#include "render/gizmos/light_category_gizmo_utils.hpp"
#include "thing/thing_light_authoring_utils.hpp"

namespace sed
{

bool MainWindow::deleteSelectedSurfaceVertices(const bool force)
{
    if (!documentEditingAllowed()
        || editMode_ != EditMode::surface
        || surfaceSelectionMode_ != SurfaceSelectionMode::vertex
        || document_.selection().surfaceVertices.empty())
    {
        return false;
    }
    if (selectionTouchesNonEditableLayer(document_.selection()))
    {
        QMessageBox::warning(this, "Layer read-only",
            "Selection contains objects on a hidden or locked layer.");
        return false;
    }

    std::vector<VertexRef> vertices{};
    const Level& level = document_.level();
    for (const editor::SurfaceVertexRef selected
        : document_.selection().surfaceVertices)
    {
        if (selected.surface.sector < 0 || selected.surface.surface < 0
            || selected.localVertex < 0
            || static_cast<std::size_t>(selected.surface.sector)
                >= level.sectors.size())
        {
            continue;
        }
        const Sector& sector = level.sectors[
            static_cast<std::size_t>(selected.surface.sector)];
        if (static_cast<std::size_t>(selected.surface.surface)
            >= sector.surfaces.size())
        {
            continue;
        }
        const Surface& surface = sector.surfaces[
            static_cast<std::size_t>(selected.surface.surface)];
        if (static_cast<std::size_t>(selected.localVertex)
            >= surface.vertices.size())
        {
            continue;
        }
        const VertexRef ref{ selected.surface.sector,
            surface.vertices[static_cast<std::size_t>(selected.localVertex)] };
        if (std::find_if(vertices.begin(), vertices.end(), [ref](const VertexRef existing)
            {
                return existing.sector == ref.sector
                    && existing.vertex == ref.vertex;
            }) == vertices.end())
        {
            vertices.push_back(ref);
        }
    }
    if (vertices.empty())
    {
        return false;
    }
    std::sort(vertices.begin(), vertices.end(), [](const VertexRef a, const VertexRef b)
        {
            return a.sector != b.sector ? a.sector > b.sector
                                        : a.vertex > b.vertex;
        });

    if (force)
    {
        const QString countText = vertices.size() == 1
            ? QStringLiteral("the selected vertex")
            : QStringLiteral("%1 selected vertices").arg(vertices.size());
        QMessageBox confirmation(
            QMessageBox::Warning,
            QStringLiteral("Force Delete Vertices"),
            QString("Force-delete %1?\n\n"
                    "This may leave surfaces open or invalid. Validation, "
                    "export, or saving may reject the level until the "
                    "geometry is repaired. You can Undo this operation.")
                .arg(countText),
            QMessageBox::Ok | QMessageBox::Cancel,
            this);
        confirmation.setDefaultButton(QMessageBox::Cancel);
        confirmation.setEscapeButton(QMessageBox::Cancel);
        confirmation.setProperty(
            kMessageBoxLoggingDestructiveProperty, true);
        QStringList vertexReferences{};
        vertexReferences.reserve(static_cast<qsizetype>(vertices.size()));
        for (const VertexRef vertex : vertices)
        {
            vertexReferences.push_back(QStringLiteral("%1:%2")
                .arg(vertex.sector).arg(vertex.vertex));
        }
        confirmation.setProperty(kMessageBoxLoggingContextProperty,
            QStringLiteral("Surface vertices (%1): %2")
                .arg(vertices.size())
                .arg(vertexReferences.join(QStringLiteral(", "))));
        if (confirmation.exec() != QMessageBox::Ok)
        {
            return false;
        }
    }

    std::size_t deletedCount = 0;
    bool applied = false;
    try
    {
        applied = document_.applyUndoable(
            ui::undo::action("Surface Vertex", "Surface Vertices",
                vertices.size(), force ? "Force-deleted" : "Deleted"),
            editor::DocumentDirty::Structure | editor::DocumentDirty::LevelGeometry,
            [&](editor::DocumentState& state) -> bool
            {
                for (const VertexRef ref : vertices)
                {
                    const bool deleted = force
                        ? ::sed::level::levelForceDeleteSectorVertex(
                            state.level, ref.sector, ref.vertex)
                        : ::sed::level::levelDeleteSectorVertex(
                            state.level, ref.sector, ref.vertex);
                    deletedCount += deleted ? 1u : 0u;
                }
                if (deletedCount == 0)
                {
                    return false;
                }
                state.selection.clear();
                return true;
            });
    }
    catch (const std::exception& ex)
    {
        QMessageBox::warning(this, "Delete vertices failed",
            QString::fromUtf8(ex.what()));
        return false;
    }
    if (!applied)
    {
        if (glView_ && !force)
        {
            glView_->showToast(
                "Delete removes only redundant vertices on a straight polygon edge. Use Shift Delete to force removal.",
                4200,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
        }
        return false;
    }

    invalidateSurfaceSelectionContextForTopologyEdit();

    if (glView_)
    {
        glView_->notifyLevelGeometryChanged();
        glView_->setSelectionState(-1, -1, -1, -1, document_.selection());
    }
    rebuildOutliner();
    rebuildProperties();
    updateEditActions();
    scheduleRebuildChecks(
        editor::DocumentDirty::Structure | editor::DocumentDirty::LevelGeometry);
    return true;
}

void MainWindow::setupEditDeleteAction()
{
    // Step 1: create the shared Delete action used by the entity-specific submenus.
    deleteAction_ = new QAction("&Delete", this);
    deleteAction_->setObjectName("edit.delete");
    deleteAction_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::actionDelete));
    deleteAction_->setShortcut(QKeySequence(Qt::Key_Delete));
    deleteAction_->setToolTip(
        "Delete the selection; surface vertices are removed only when they are redundant");
    // Entity submenus use independent proxy actions so their enabled states do
    // not fight each other.  Keep this canonical shortcut-bearing action on
    // the window even though it is no longer inserted into those menus.
    addAction(deleteAction_);

    forceDeleteSurfaceVertexAction_ = new QAction("Force Delete &Vertex", this);
    forceDeleteSurfaceVertexAction_->setObjectName("surface.force_delete_vertex");
    forceDeleteSurfaceVertexAction_->setShortcut(
        QKeySequence(Qt::SHIFT | Qt::Key_Delete));
    forceDeleteSurfaceVertexAction_->setToolTip(
        "Force-delete selected surface vertices after a warning; geometry may become invalid");
    connect(forceDeleteSurfaceVertexAction_, &QAction::triggered, this,
        [this]() { (void)deleteSelectedSurfaceVertices(true); });
    addAction(forceDeleteSurfaceVertexAction_);

    connect(deleteAction_, &QAction::triggered,
        this, &MainWindow::deleteSelection);
}

void MainWindow::deleteSelection()
{
    // All menu proxies and the canonical Delete shortcut terminate here.  The
    // defensive preview check matters even when a stale button or a queued
    // signal reaches this slot after its QAction was disabled.
    if (!documentEditingAllowed())
    {
        return;
    }

            const bool stagedCleaveActive = glView_
                && glView_->isStagedCleaveInteractionActive();
            if (stagedCleaveActive)
            {
                if (!glView_->deleteSelectedStagedCleaveLine())
                {
                    glView_->showToast(
                        "Select a Multi-cut plane to delete.",
                        2400,
                        GlView::ToastPlacement::BottomCenter,
                        GlView::ToastGroup::Error);
                }
                return;
            }

            const editor::Selection& selection = document_.selection();
            if (selectionTouchesNonEditableLayer(selection))
            {
                QMessageBox::warning(this, "Layer read-only", "Selection contains objects on a hidden or locked layer.");
                return;
            }

            if (editMode_ == EditMode::surface
                && surfaceSelectionMode_ == SurfaceSelectionMode::vertex)
            {
                // Vertex mode never owns polygon-face deletion.  Marquee or
                // recovered editor state can retain only the owning Surface
                // context (or only invalid vertex references); falling
                // through in that state would turn ordinary Delete into the
                // Face-mode command and remove complete polygons.  Valid
                // redundant vertices take the topology-safe path, otherwise
                // this is deliberately a no-op. Shift+Delete remains the
                // explicit, warned recovery path.
                if (!selection.surfaceVertices.empty())
                {
                    (void)deleteSelectedSurfaceVertices(false);
                }
                return;
            }

            if (editMode_ == EditMode::surface
                && surfaceSelectionMode_ == SurfaceSelectionMode::edge)
            {
                // There is no topology-safe edge deletion command.  Owning
                // Surface refs are selection context in Edge mode, not a
                // request to delete their polygon faces.
                return;
            }

            // Sector thrust is an authored sector property, but its viewport
            // handle behaves like a selected sub-object. Delete therefore
            // clears the selected vectors instead of deleting their sectors.
            if (editMode_ == EditMode::sector
                && viewportSectorThrustAction_
                && viewportSectorThrustAction_->isChecked())
            {
                const bool applied = document_.applyUndoable(
                    "Sector: Thrust cleared",
                    editor::DocumentDirty::LevelGeometry,
                    [this](editor::DocumentState& state) -> bool
                    {
                        bool changed = false;
                        for (const std::int32_t sectorIndex : state.selection.sectors)
                        {
                            if (sectorIndex < 0
                                || static_cast<std::size_t>(sectorIndex) >= state.level.sectors.size())
                            {
                                continue;
                            }
                            Sector& sector = state.level.sectors[static_cast<std::size_t>(sectorIndex)];
                            if (isLayerLocked(sector.layer))
                            {
                                continue;
                            }
                            const std::int32_t nextFlags =
                                sector.flags & ~SECF_IJIM_UseThrust;
                            if (sector.thrust.IsZero() && sector.flags == nextFlags)
                            {
                                continue;
                            }
                            sector.thrust = TVector::zero;
                            sector.flags = nextFlags;
                            changed = true;
                        }
                        return changed;
                    });
                if (applied)
                {
                    if (glView_)
                    {
                        glView_->update();
                    }
                    rebuildProperties();
                    updateEditActions();
                    scheduleRebuildChecks(editor::DocumentDirty::LevelGeometry);
                }
                return;
            }

            // Runtime Thing lights are template/instance parameters rather
            // than standalone entities. Delete removes only the selected
            // emitter's local override; an inherited template emitter is
            // deliberately left in place and becomes effective immediately.
            if (editMode_ == EditMode::light
                && lightEditSubMode_ == LightEditSubMode::architectural)
            {
                (void)resetArchitecturalLightingForSelection();
                return;
            }

            if (editMode_ == EditMode::light
                && lightEditSubMode_ == LightEditSubMode::thing)
            {
                const QVariant kindValue = glView_
                    ? glView_->property("sed.selectedLightGizmoKind")
                    : QVariant{};
                const auto selectedKind = static_cast<render::LightGizmoKind>(
                    kindValue.isValid() ? kindValue.toInt() : -1);
                if (selectedKind != render::LightGizmoKind::ThingRegular
                    && selectedKind != render::LightGizmoKind::ThingHead)
                {
                    if (glView_)
                    {
                        glView_->showToast(
                            "Select a Thing light or Headlight gizmo first.",
                            3000,
                            GlView::ToastPlacement::BottomCenter,
                            GlView::ToastGroup::Error);
                    }
                    return;
                }

                const thing::light_authoring::Kind authoringKind =
                    selectedKind == render::LightGizmoKind::ThingHead
                    ? thing::light_authoring::Kind::headlight
                    : thing::light_authoring::Kind::regular;
                const assets::Templates* templatesDb =
                    templates_.size() > 0 ? &templates_ : nullptr;
                int inheritedOnly = 0;
                int inheritedRevealed = 0;
                const std::string undoLabel = ui::undo::action(
                    "Thing",
                    document_.selection().things.size(),
                    authoringKind == thing::light_authoring::Kind::headlight
                        ? "Local headlight removed"
                        : "Local light removed");
                const bool applied = document_.applyUndoable(
                    undoLabel,
                    editor::DocumentDirty::Things,
                    [templatesDb, authoringKind, &inheritedOnly, &inheritedRevealed](
                        editor::DocumentState& state) -> bool
                    {
                        bool changed = false;
                        for (const std::int32_t thingIndex : state.selection.things)
                        {
                            const thing::light_authoring::RemovalResult result =
                                thing::light_authoring::removeLocalOverride(
                                    state.level, templatesDb, thingIndex, authoringKind);
                            changed = changed
                                || result == thing::light_authoring::RemovalResult::removed
                                || result == thing::light_authoring::RemovalResult::inheritedRevealed;
                            inheritedOnly += result
                                == thing::light_authoring::RemovalResult::inheritedOnly;
                            inheritedRevealed += result
                                == thing::light_authoring::RemovalResult::inheritedRevealed;
                        }
                        return changed;
                    });

                if (!applied)
                {
                    if (glView_ && inheritedOnly > 0)
                    {
                        glView_->showToast(
                            authoringKind == thing::light_authoring::Kind::headlight
                                ? "Template Headlight cannot be removed from this Thing."
                                : "Template Thing light cannot be removed from this Thing.",
                            3400,
                            GlView::ToastPlacement::BottomCenter,
                            GlView::ToastGroup::Error);
                    }
                    return;
                }

                if (glView_)
                {
                    glView_->notifyThingsChanged();
                    if (inheritedRevealed > 0)
                    {
                        glView_->showToast(
                            authoringKind == thing::light_authoring::Kind::headlight
                                ? "Local Headlight override removed; template Headlight restored."
                                : "Local Thing-light override removed; template light restored.",
                            2600,
                            GlView::ToastPlacement::BottomCenter);
                    }
                }
                rebuildProperties();
                updateEditActions();
                scheduleRebuildChecks(editor::DocumentDirty::Things);
                return;
            }

            // IJIM Sector point lights live on sectors. In the Sector-light
            // submode Delete zeros that PointLight and retains the sector.
            if (editMode_ == EditMode::light
                && lightEditSubMode_ == LightEditSubMode::sector)
            {
                const std::string undoLabel = ui::undo::action(
                    "Sector", "Sectors",
                    document_.selection().sectors.size(),
                    "Point light removed");
                const bool applied = document_.applyUndoable(
                    undoLabel,
                    editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Things,
                    [this](editor::DocumentState& state) -> bool
                    {
                        bool changed = false;
                        for (const std::int32_t sectorIndex : state.selection.sectors)
                        {
                            if (sectorIndex < 0
                                || static_cast<std::size_t>(sectorIndex) >= state.level.sectors.size())
                            {
                                continue;
                            }
                            Sector& sector = state.level.sectors[static_cast<std::size_t>(sectorIndex)];
                            if (isLayerLocked(sector.layer)
                                || !pointLightHasAuthoredParameters(sector.pointLight))
                            {
                                continue;
                            }
                            sector.pointLight = PointLight{};
                            changed = true;
                        }
                        return changed;
                    });
                if (applied)
                {
                    if (glView_)
                    {
                        glView_->notifyLevelGeometryChanged();
                        glView_->notifyThingsChanged();
                    }
                    rebuildProperties();
                    updateEditActions();
                    scheduleRebuildChecks(
                        editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Things);
                }
                return;
            }

            // Frame/path mode edits the `frame` values of the active thing. In this mode the Delete
            // key should remove selected frame points rather than deleting the whole thing.
            if (editMode_ == EditMode::frame)
            {
                if (!selection.frames.empty())
                {
                    std::vector<::sed::level::FrameRef> frames(selection.frames.begin(), selection.frames.end());
                    std::sort(frames.begin(), frames.end(), [](const ::sed::level::FrameRef& a, const ::sed::level::FrameRef& b)
                        {
                            if (a.thing != b.thing)
                            {
                                return a.thing > b.thing;
                            }
                            return a.value > b.value;
                        });

                    const std::string label = ui::undo::action("Thing Frame", frames.size(), "Deleted");
                    const bool applied = document_.applyUndoable(label, editor::DocumentDirty::Things, [&](editor::DocumentState& state) -> bool
                        {
                            std::vector<std::int32_t> touchedThings{};
                            touchedThings.reserve(frames.size());

                            for (const ::sed::level::FrameRef ref : frames)
                            {
                                if (ref.thing < 0 || ref.value < 0)
                                {
                                   continue;
                               }
                               if (static_cast<std::size_t>(ref.thing) >= state.level.things.size())
                               {
                                   continue;
                               }

                               Thing& thing = state.level.things[static_cast<std::size_t>(ref.thing)];
                               if (static_cast<std::size_t>(ref.value) >= thing.values.size())
                               {
                                   continue;
                               }

                               if (thing.values[static_cast<std::size_t>(ref.value)].type != ValueType::frame
                                   || !::sed::text::iequalsAscii(thing.values[static_cast<std::size_t>(ref.value)].name, "frame"))
                                {
                                    continue;
                                }

                                touchedThings.push_back(ref.thing);
                                thing.values.erase(thing.values.begin() + ref.value);
                            }

                            std::sort(touchedThings.begin(), touchedThings.end());
                            touchedThings.erase(std::unique(touchedThings.begin(), touchedThings.end()), touchedThings.end());

                            // Keep `numframes` in sync with the number of `frame` values.
                            for (const std::int32_t thingIndex : touchedThings)
                            {
                                if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= state.level.things.size())
                                {
                                    continue;
                                }

                                Thing& thing = state.level.things[static_cast<std::size_t>(thingIndex)];
                                ::sed::level::thingSyncNumFrames(thing);
                            }

                            state.selection.frames.clear();
                            return true;
                        });

                    if (!applied)
                    {
                        QMessageBox::warning(this, "Delete failed", "Operation not permitted for current selection.");
                        return;
                    }

                    if (glView_)
                    {
                        glView_->notifyThingsChanged();
                        glView_->clearFrameSelection();
                        glView_->setHighlightedSelection(document_.selection());
                    }

                    rebuildOutliner();
                    rebuildProperties();
                    updateEditActions();
                    return;
                }

                // In frame mode, don't delete the whole thing unless the selection contains other object types.
                const bool onlyThings =
                    !selection.things.empty()
                    && selection.sectors.empty()
                    && selection.surfaces.empty()
                    && selection.surfaceVertices.empty()
                    && selection.lights.empty()
                    && selection.cogs.empty();
                if (onlyThings)
                {
                    return;
                }
            }

            const std::size_t selectedCount = selection.sectors.size() + selection.surfaces.size()
                + selection.things.size() + selection.lights.size() + selection.cogs.size();
            if (selectedCount == 0)
            {
                return;
            }

            const std::string label = ui::undo::selectionAction(selection, "Deleted");

            std::vector<std::int32_t> sectors(selection.sectors.begin(), selection.sectors.end());
            std::sort(sectors.begin(), sectors.end(), [](const std::int32_t a, const std::int32_t b) { return a > b; });

            std::vector<SurfaceRef> surfaces(selection.surfaces.begin(), selection.surfaces.end());
            std::sort(surfaces.begin(), surfaces.end(), [](const SurfaceRef& a, const SurfaceRef& b)
                {
                    if (a.sector != b.sector)
                    {
                        return a.sector > b.sector;
                    }
                    return a.surface > b.surface;
                });

            std::vector<std::int32_t> things(selection.things.begin(), selection.things.end());
            std::sort(things.begin(), things.end(), [](const std::int32_t a, const std::int32_t b) { return a > b; });

            std::vector<std::int32_t> lights(selection.lights.begin(), selection.lights.end());
            std::sort(lights.begin(), lights.end(), [](const std::int32_t a, const std::int32_t b) { return a > b; });

            std::vector<std::int32_t> cogs(selection.cogs.begin(), selection.cogs.end());
            std::sort(cogs.begin(), cogs.end(), [](const std::int32_t a, const std::int32_t b) { return a > b; });

            if (!cogs.empty())
            {
                const QString prompt =
                    (cogs.size() == 1)
                    ? QString("Delete selected COG?")
                    : QString("Delete %1 selected COGs?").arg(static_cast<int>(cogs.size()));

                QStringList cogIndices{};
                cogIndices.reserve(static_cast<qsizetype>(cogs.size()));
                for (const std::int32_t cog : cogs)
                {
                    cogIndices.push_back(QString::number(cog));
                }
                QMessageBox confirmation(
                    QMessageBox::Question,
                    QStringLiteral("Delete COG"),
                    prompt,
                    QMessageBox::Yes | QMessageBox::No,
                    this);
                confirmation.setDefaultButton(QMessageBox::No);
                confirmation.setEscapeButton(QMessageBox::No);
                confirmation.setProperty(
                    kMessageBoxLoggingDestructiveProperty, true);
                confirmation.setProperty(kMessageBoxLoggingContextProperty,
                    QStringLiteral("COG indices: %1")
                        .arg(cogIndices.join(QStringLiteral(", "))));
                if (confirmation.exec() != QMessageBox::Yes)
                {
                    return;
                }
            }

            editor::DocumentDirty dirty = editor::DocumentDirty::Structure;
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
            if (!cogs.empty())
            {
                dirty |= editor::DocumentDirty::Cogs;
            }

            const bool applied = document_.applyUndoable(std::move(label), dirty, [&](editor::DocumentState& state) -> bool
                {
                    for (const std::int32_t index : cogs)
                    {
                        if (!::sed::level::levelDeleteCog(state.level, index))
                        {
                            return false;
                        }
                    }

                    for (const std::int32_t index : lights)
                    {
                        if (!::sed::level::levelDeleteLight(state.level, index))
                        {
                            return false;
                        }
                    }

                    for (const std::int32_t index : things)
                    {
                        if (!::sed::level::levelDeleteThing(state.level, index))
                        {
                            return false;
                        }
                    }

                    for (const SurfaceRef ref : surfaces)
                    {
                        if (!::sed::level::levelDeleteSurface(state.level, ref))
                        {
                            return false;
                        }
                    }

                    for (const std::int32_t index : sectors)
                    {
                        if (!::sed::level::levelDeleteSector(state.level, index))
                        {
                            return false;
                        }
                    }

                    state.selection.clear();
                    return true;
                });

            if (!applied)
            {
                QMessageBox::warning(this, "Delete failed", "Operation not permitted for current selection.");
                return;
            }

            if (editor::hasAny(dirty, editor::DocumentDirty::Structure)
                && editor::hasAny(dirty, editor::DocumentDirty::LevelGeometry))
            {
                invalidateSurfaceSelectionContextForTopologyEdit();
            }

            if (glView_)
            {
                const bool levelChanged = editor::hasAny(dirty, editor::DocumentDirty::LevelGeometry);
                const bool thingsChanged = editor::hasAny(dirty, editor::DocumentDirty::Things);
                const bool lightsChanged = editor::hasAny(dirty, editor::DocumentDirty::Lights);

                if (levelChanged)
                {
                    const bool hadPendingCuts =
                        !glView_->stagedCleaveDraftView(GlView::CleaveWorkspaceFamily::Sector).lines.empty()
                        || !glView_->stagedCleaveDraftView(GlView::CleaveWorkspaceFamily::Surface).lines.empty();
                    glView_->cancelAllStagedCleaveWorkspaces();
                    if (hadPendingCuts)
                    {
                        glView_->showToast(
                            "Multi-cut: level topology changed; the pending draft was cleared",
                            3200, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
                    }
                }

                if (levelChanged && thingsChanged)
                {
                    glView_->notifyAllMeshesChanged();
                }
                else if (levelChanged)
                {
                    glView_->notifyLevelGeometryChanged();
                }
                else if (thingsChanged)
                {
                    glView_->notifyThingsChanged();
                }

                if (lightsChanged)
                {
                    glView_->notifyLightsChanged();
                }

                glView_->setSelectionState(-1, -1, -1, -1, document_.selection());
            }
            if (editor::hasAny(dirty, editor::DocumentDirty::Cogs))
            {
                syncCogScriptWatches();
            }
            rebuildOutliner();
            rebuildProperties();
            updateEditActions();
}

} // namespace sed
