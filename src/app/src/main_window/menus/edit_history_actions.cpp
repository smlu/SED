#include "main_window/common.hpp"
#include "main_window/detail/history_dirty_utils.hpp"
#include "ui/resources/project_resource_settings.hpp"
#include "ui/shortcuts/shortcut_context_utils.hpp"

#include <QLineEdit>

namespace sed
{

#include "main_window/detail/helpers.hpp"

void MainWindow::setupEditHistoryActions()
{
    const auto hasMotsArchLighting = [](const Level& level) noexcept
    {
        return level.kind == ProjectType::MOTS
            && !level.motsArchLighting.empty();
    };
    const auto hasMotsArchBakeState = [](const Level& level) noexcept
    {
        if (level.kind != ProjectType::MOTS)
        {
            return false;
        }
        return !level.motsArchLightingBakeBaselines.empty()
            || std::ranges::any_of(level.lights, [](const SedLight& light)
                {
                    return (light.flags & LF_AffectArchThings) != 0;
                });
    };

    // Step 1: prepare shared selection/outliner helpers used by undo and redo refresh paths.
    auto selectionEquals = [](const editor::Selection& a, const editor::Selection& b) -> bool
    {
        return a.sectors == b.sectors
            && a.surfaces == b.surfaces
            && a.surfaceVertices == b.surfaceVertices
            && a.things == b.things
            && a.frames == b.frames
            && a.lights == b.lights
            && a.cogs == b.cogs;
    };

    struct FrameHistoryProbe
    {
        struct ThingState
        {
            std::int32_t thing{-1};
            std::size_t frameCount{};
        };
        struct SelectedFrame
        {
            level::FrameRef ref{};
            std::optional<std::size_t> ordinal{};
        };
        std::vector<ThingState> things{};
        std::vector<SelectedFrame> frames{};
    };
    const auto isFrameValue = [](const TplValue& value)
    {
        return value.type == ValueType::frame
            && text::iequalsAscii(value.name, "frame");
    };
    auto captureFrameHistoryProbe = [this, isFrameValue](
        const editor::Selection& selection,
        const editor::DocumentHistoryTransitionState* transition)
    {
        FrameHistoryProbe probe{};
        std::vector<std::int32_t> owners{};
        const std::size_t transitionFrameCount = transition
            ? transition->beforeSelection.frames.size()
                + transition->afterSelection.frames.size()
            : 0u;
        owners.reserve(selection.frames.size() + transitionFrameCount
            + (selection.things.size() == 1u ? 1u : 0u));
        const auto appendOwner = [&owners](const std::int32_t thingIndex)
        {
            if ( std::find(owners.begin(), owners.end(), thingIndex)
                == owners.end() )
            {
                owners.push_back(thingIndex);
            }
        };
        if ( selection.things.size() == 1u )
        {
            appendOwner(*selection.things.begin());
        }
        for ( const level::FrameRef ref : selection.frames )
        {
            appendOwner(ref.thing);
        }
        if ( transition )
        {
            for ( const level::FrameRef ref : transition->beforeSelection.frames )
            {
                appendOwner(ref.thing);
            }
            for ( const level::FrameRef ref : transition->afterSelection.frames )
            {
                appendOwner(ref.thing);
            }
        }

        const Level& level = document_.level();
        for ( const std::int32_t thingIndex : owners )
        {
            if ( thingIndex < 0
                || static_cast<std::size_t>(thingIndex) >= level.things.size() )
            {
                continue;
            }
            const Thing& thing = level.things[
                static_cast<std::size_t>(thingIndex)];
            const std::size_t frameCount = static_cast<std::size_t>(
                std::count_if(thing.values.begin(), thing.values.end(),
                    isFrameValue));
            probe.things.push_back({ thingIndex, frameCount });
        }

        for ( const level::FrameRef ref : selection.frames )
        {
            std::optional<std::size_t> ordinal{};
            if ( ref.thing >= 0
                && static_cast<std::size_t>(ref.thing) < level.things.size() )
            {
                const Thing& thing = level.things[
                    static_cast<std::size_t>(ref.thing)];
                if ( ref.value >= 0
                    && static_cast<std::size_t>(ref.value) < thing.values.size()
                    && isFrameValue(thing.values[
                        static_cast<std::size_t>(ref.value)]) )
                {
                    ordinal = static_cast<std::size_t>(std::count_if(
                        thing.values.begin(),
                        thing.values.begin() + ref.value, isFrameValue));
                }
            }
            probe.frames.push_back({ ref, ordinal });
        }
        return probe;
    };
    auto frameStorageChanged = [this, isFrameValue](
        const FrameHistoryProbe& probe)
    {
        const Level& level = document_.level();
        for ( const FrameHistoryProbe::ThingState& saved : probe.things )
        {
            if ( saved.thing < 0
                || static_cast<std::size_t>(saved.thing) >= level.things.size() )
            {
                return true;
            }
            const Thing& thing = level.things[
                static_cast<std::size_t>(saved.thing)];
            const std::size_t currentCount = static_cast<std::size_t>(
                std::count_if(thing.values.begin(), thing.values.end(),
                    isFrameValue));
            if ( currentCount != saved.frameCount )
            {
                return true;
            }
        }

        for ( const FrameHistoryProbe::SelectedFrame& saved : probe.frames )
        {
            if ( !saved.ordinal || saved.ref.thing < 0
                || static_cast<std::size_t>(saved.ref.thing) >= level.things.size() )
            {
                return true;
            }
            const Thing& thing = level.things[
                static_cast<std::size_t>(saved.ref.thing)];
            std::size_t ordinal = 0;
            std::optional<std::int32_t> currentValueIndex{};
            for ( std::size_t valueIndex = 0;
                 valueIndex < thing.values.size(); ++valueIndex )
            {
                if ( !isFrameValue(thing.values[valueIndex]) )
                {
                    continue;
                }
                if ( ordinal == *saved.ordinal )
                {
                    if ( valueIndex <= static_cast<std::size_t>(
                            std::numeric_limits<std::int32_t>::max()) )
                    {
                        currentValueIndex = static_cast<std::int32_t>(valueIndex);
                    }
                    break;
                }
                ++ordinal;
            }
            if ( !currentValueIndex || *currentValueIndex != saved.ref.value )
            {
                return true;
            }
        }
        return false;
    };

    struct PreservedSurfaceVertex
    {
        editor::SurfaceVertexRef ref{};
        std::int32_t sectorVertex{-1};
        TVector point{};
    };
    auto captureSurfaceVertices = [this](const editor::Selection& selection)
    {
        const Level& level = document_.level();
        std::vector<PreservedSurfaceVertex> result{};
        result.reserve(selection.surfaceVertices.size());
        for (const editor::SurfaceVertexRef ref : selection.surfaceVertices)
        {
            if (ref.surface.sector < 0 || ref.surface.surface < 0
                || ref.localVertex < 0
                || static_cast<std::size_t>(ref.surface.sector)
                    >= level.sectors.size())
            {
                continue;
            }
            const Sector& sector = level.sectors[
                static_cast<std::size_t>(ref.surface.sector)];
            if (static_cast<std::size_t>(ref.surface.surface)
                >= sector.surfaces.size())
            {
                continue;
            }
            const Surface& surface = sector.surfaces[
                static_cast<std::size_t>(ref.surface.surface)];
            if (static_cast<std::size_t>(ref.localVertex)
                >= surface.vertices.size())
            {
                continue;
            }
            const std::int32_t sectorVertex = surface.vertices[
                static_cast<std::size_t>(ref.localVertex)];
            if (sectorVertex < 0
                || static_cast<std::size_t>(sectorVertex)
                    >= sector.vertices.size())
            {
                continue;
            }
            const Vertex& vertex = sector.vertices[
                static_cast<std::size_t>(sectorVertex)];
            result.push_back(PreservedSurfaceVertex{
                ref,
                sectorVertex,
                TVector{ vertex.x, vertex.y, vertex.z } });
        }
        return result;
    };

    auto sanitizeSelection = [this](editor::Selection& sel)
    {
        const Level& level = document_.level();

        for (auto it = sel.sectors.begin(); it != sel.sectors.end();)
        {
            const std::int32_t idx = *it;
            if (idx < 0 || static_cast<std::size_t>(idx) >= level.sectors.size())
            {
                it = sel.sectors.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = sel.surfaces.begin(); it != sel.surfaces.end();)
        {
            const SurfaceRef ref = *it;
            if (ref.sector < 0 || ref.surface < 0 || static_cast<std::size_t>(ref.sector) >= level.sectors.size())
            {
                it = sel.surfaces.erase(it);
                continue;
            }

            const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
            if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
            {
                it = sel.surfaces.erase(it);
                continue;
            }

            ++it;
        }

        for (auto it = sel.surfaceVertices.begin();
             it != sel.surfaceVertices.end();)
        {
            const editor::SurfaceVertexRef ref = *it;
            if (ref.surface.sector < 0 || ref.surface.surface < 0
                || ref.localVertex < 0
                || static_cast<std::size_t>(ref.surface.sector)
                    >= level.sectors.size())
            {
                it = sel.surfaceVertices.erase(it);
                continue;
            }
            const Sector& sector = level.sectors[
                static_cast<std::size_t>(ref.surface.sector)];
            if (static_cast<std::size_t>(ref.surface.surface)
                >= sector.surfaces.size())
            {
                it = sel.surfaceVertices.erase(it);
                continue;
            }
            const Surface& surface = sector.surfaces[
                static_cast<std::size_t>(ref.surface.surface)];
            if (static_cast<std::size_t>(ref.localVertex)
                    >= surface.vertices.size()
                || surface.vertices[static_cast<std::size_t>(ref.localVertex)] < 0
                || static_cast<std::size_t>(surface.vertices[
                       static_cast<std::size_t>(ref.localVertex)])
                    >= sector.vertices.size())
            {
                it = sel.surfaceVertices.erase(it);
                continue;
            }
            ++it;
        }

        for (auto it = sel.things.begin(); it != sel.things.end();)
        {
            const std::int32_t idx = *it;
            if (idx < 0 || static_cast<std::size_t>(idx) >= level.things.size())
            {
                it = sel.things.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = sel.frames.begin(); it != sel.frames.end();)
        {
            const sed::level::FrameRef ref = *it;
            if (ref.thing < 0 || ref.value < 0
                || static_cast<std::size_t>(ref.thing) >= level.things.size())
            {
                it = sel.frames.erase(it);
                continue;
            }
            const Thing& thing = level.things[
                static_cast<std::size_t>(ref.thing)];
            if (static_cast<std::size_t>(ref.value) >= thing.values.size())
            {
                it = sel.frames.erase(it);
                continue;
            }
            const TplValue& value = thing.values[
                static_cast<std::size_t>(ref.value)];
            if (value.type != ValueType::frame
                || !text::iequalsAscii(value.name, "frame"))
            {
                it = sel.frames.erase(it);
                continue;
            }
            ++it;
        }

        for (auto it = sel.lights.begin(); it != sel.lights.end();)
        {
            const std::int32_t idx = *it;
            if (idx < 0 || static_cast<std::size_t>(idx) >= level.lights.size())
            {
                it = sel.lights.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = sel.cogs.begin(); it != sel.cogs.end();)
        {
            const std::int32_t idx = *it;
            if (idx < 0 || static_cast<std::size_t>(idx) >= level.cogs.size())
            {
                it = sel.cogs.erase(it);
            }
            else
            {
                ++it;
            }
        }
    };

    auto restoreSurfaceVertices = [this](
        editor::Selection& selection,
        const std::vector<PreservedSurfaceVertex>& preserved)
    {
        const Level& level = document_.level();
        selection.surfaceVertices.clear();
        const auto samePoint = [](const TVector& lhs, const Vertex& rhs)
        {
            const double dx = lhs.x - rhs.x;
            const double dy = lhs.y - rhs.y;
            const double dz = lhs.z - rhs.z;
            return dx * dx + dy * dy + dz * dz <= 1.0e-10;
        };
        for (const PreservedSurfaceVertex& saved : preserved)
        {
            const SurfaceRef owner = saved.ref.surface;
            if (owner.sector < 0 || owner.surface < 0
                || static_cast<std::size_t>(owner.sector)
                    >= level.sectors.size())
            {
                continue;
            }
            const Sector& sector = level.sectors[
                static_cast<std::size_t>(owner.sector)];
            if (static_cast<std::size_t>(owner.surface)
                >= sector.surfaces.size())
            {
                continue;
            }
            const Surface& surface = sector.surfaces[
                static_cast<std::size_t>(owner.surface)];
            std::optional<std::int32_t> local{};

            // Stable local+sector identity survives ordinary vertex moves.
            if (saved.ref.localVertex >= 0
                && static_cast<std::size_t>(saved.ref.localVertex)
                    < surface.vertices.size()
                && surface.vertices[
                       static_cast<std::size_t>(saved.ref.localVertex)]
                    == saved.sectorVertex)
            {
                local = saved.ref.localVertex;
            }
            // Topology insertion/reordering can move the same sector vertex
            // to another polygon corner. Require its old position as well so
            // compaction cannot retarget an old numeric id to a neighbour.
            if (!local)
            {
                for (std::size_t index = 0;
                     index < surface.vertices.size(); ++index)
                {
                    const std::int32_t sectorVertex = surface.vertices[index];
                    if (sectorVertex != saved.sectorVertex || sectorVertex < 0
                        || static_cast<std::size_t>(sectorVertex)
                            >= sector.vertices.size()
                        || !samePoint(saved.point, sector.vertices[
                            static_cast<std::size_t>(sectorVertex)]))
                    {
                        continue;
                    }
                    local = static_cast<std::int32_t>(index);
                    break;
                }
            }
            // Sector-vertex compaction changes numeric ids. Coordinate
            // identity provides the deterministic recovery fallback.
            if (!local)
            {
                for (std::size_t index = 0;
                     index < surface.vertices.size(); ++index)
                {
                    const std::int32_t sectorVertex = surface.vertices[index];
                    if (sectorVertex < 0
                        || static_cast<std::size_t>(sectorVertex)
                            >= sector.vertices.size()
                        || !samePoint(saved.point, sector.vertices[
                            static_cast<std::size_t>(sectorVertex)]))
                    {
                        continue;
                    }
                    local = static_cast<std::int32_t>(index);
                    break;
                }
            }
            if (local)
            {
                selection.surfaceVertices.insert(
                    editor::SurfaceVertexRef{ owner, *local });
            }
        }

        if (surfaceSelectionMode_ == SurfaceSelectionMode::vertex)
        {
            selection.surfaces.clear();
            for (const editor::SurfaceVertexRef ref : selection.surfaceVertices)
            {
                selection.surfaces.insert(ref.surface);
            }
        }
    };

    auto applyDocumentSelectionToViewport = [this]()
    {
        if (!glView_)
        {
            return;
        }

        const editor::Selection& selection = document_.selection();

        auto minIndex = [](const auto& set) -> std::int32_t
        {
            if (set.empty())
            {
                return -1;
            }
            return *std::min_element(set.begin(), set.end());
        };

        auto minSurface = [&]() -> SurfaceRef
        {
            if (selection.surfaces.empty())
            {
                return SurfaceRef{ -1, -1 };
            }

            SurfaceRef best = *selection.surfaces.begin();
            for (const SurfaceRef& ref : selection.surfaces)
            {
                if (ref.sector < best.sector || (ref.sector == best.sector && ref.surface < best.surface))
                {
                    best = ref;
                }
            }
            return best;
        };

        std::int32_t selSector = -1;
        std::int32_t selSurface = -1;
        std::int32_t selThing = -1;
        std::int32_t selLight = -1;

        if (editMode_ == EditMode::surface)
        {
            if (!selection.surfaces.empty())
            {
                const SurfaceRef ref = activeSelectedSurfaceRef().value_or(
                    minSurface());
                selSector = ref.sector;
                selSurface = ref.surface;
            }
            else if (!selection.sectors.empty())
            {
                selSector = minIndex(selection.sectors);
            }
        }
        else if (editMode_ == EditMode::sector)
        {
            if (!selection.sectors.empty())
            {
                selSector = minIndex(selection.sectors);
            }
            else if (!selection.surfaces.empty())
            {
                selSector = minSurface().sector;
            }
        }
        else if (editMode_ == EditMode::thing || editMode_ == EditMode::frame)
        {
            if (!selection.things.empty())
            {
                selThing = minIndex(selection.things);
            }
        }
        else if (editMode_ == EditMode::light)
        {
            if (!selection.lights.empty())
            {
                selLight = minIndex(selection.lights);
            }
        }
        else
        {
            if (!selection.surfaces.empty())
            {
                const SurfaceRef ref = minSurface();
                selSector = ref.sector;
                selSurface = ref.surface;
            }
            else if (!selection.sectors.empty())
            {
                selSector = minIndex(selection.sectors);
            }
            else if (!selection.things.empty())
            {
                selThing = minIndex(selection.things);
            }
            else if (!selection.lights.empty())
            {
                selLight = minIndex(selection.lights);
            }
        }

        glView_->setSelectionState(selSector, selSurface, selThing, selLight, selection);
    };

    auto updateOutlinerSelectionOnly = [this]()
    {
        if (!outliner_)
        {
            return;
        }

        const editor::Selection& selection = document_.selection();
        const int tab = outlinerTabs_ ? outlinerTabs_->currentIndex() : 0;

        const QSignalBlocker blocker(outliner_);
        outliner_->clearSelection();

        auto minIndex = [](const auto& set) -> std::int32_t
        {
            if (set.empty())
            {
                return -1;
            }
            return *std::min_element(set.begin(), set.end());
        };

        std::int32_t primaryIndex = -1;
        std::unordered_set<std::int32_t> selectedIndices{};

        if (tab == 0)
        {
            selectedIndices.reserve(selection.sectors.size() + selection.surfaces.size());
            for (const std::int32_t idx : selection.sectors)
            {
                selectedIndices.insert(idx);
            }
            for (const SurfaceRef& ref : selection.surfaces)
            {
                selectedIndices.insert(ref.sector);
            }
            primaryIndex = (!selection.surfaces.empty()) ? selection.surfaces.begin()->sector : minIndex(selection.sectors);
        }
        else if (tab == 1)
        {
            selectedIndices.reserve(selection.things.size());
            for (const std::int32_t idx : selection.things)
            {
                selectedIndices.insert(idx);
            }
            primaryIndex = minIndex(selection.things);
        }
        else if (tab == 2)
        {
            selectedIndices.reserve(selection.cogs.size());
            for (const std::int32_t idx : selection.cogs)
            {
                selectedIndices.insert(idx);
            }
            primaryIndex = minIndex(selection.cogs);
        }
        else if (tab == 3)
        {
            selectedIndices.reserve(selection.lights.size());
            for (const std::int32_t idx : selection.lights)
            {
                selectedIndices.insert(idx);
            }
            primaryIndex = minIndex(selection.lights);
        }

        QTreeWidgetItem* current = nullptr;

        for (int i = 0; i < outliner_->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem* item = outliner_->topLevelItem(i);
            if (!item)
            {
                continue;
            }

            const int kind = item->data(0, kOutlinerKindRole).toInt();
            const int index = item->data(0, kOutlinerIndexRole).toInt();

            bool selectable = false;
            if (tab == 0) selectable = kind == static_cast<int>(OutlinerKind::sector);
            if (tab == 1) selectable = kind == static_cast<int>(OutlinerKind::thing);
            if (tab == 2) selectable = kind == static_cast<int>(OutlinerKind::cog);
            if (tab == 3) selectable = kind == static_cast<int>(OutlinerKind::light);

            if (!selectable)
            {
                item->setSelected(false);
                continue;
            }

            const auto idx32 = static_cast<std::int32_t>(index);
            const bool selected = selectedIndices.contains(idx32);
            item->setSelected(selected);

            if (idx32 == primaryIndex)
            {
                current = item;
            }
        }

        if (current)
        {
            outliner_->setCurrentItem(current);
        }
    };

    // Step 2: wire Undo and preserve selection/outliner state when history rolls back.
    undoAction_ = new QAction("&Undo", this);
    undoAction_->setObjectName("edit.undo");
    undoAction_->setShortcut(QKeySequence::Undo);
    connect(undoAction_, &QAction::triggered, this,
        [this, selectionEquals, sanitizeSelection, captureSurfaceVertices,
         restoreSurfaceVertices, applyDocumentSelectionToViewport,
         updateOutlinerSelectionOnly, captureFrameHistoryProbe,
         frameStorageChanged, hasMotsArchLighting,
         hasMotsArchBakeState]()
        {
            QWidget* const focusWidget = QApplication::focusWidget();
            QWidget* const nativeTextEditor =
                ui::shortcuts::focusedNativeTextEditor(focusWidget);
            if ( nativeTextEditor && nativeTextEditor != focusWidget
                && qobject_cast<QLineEdit*>(nativeTextEditor) )
            {
                (void)ui::shortcuts::triggerNativeTextEditorUndoRedo(
                    nativeTextEditor, false);
                syncCogScriptEditorActionFocus();
                return;
            }
            if ( resourceEditorOwnsFocus() )
            {
                const bool routed = triggerActiveResourceEditorUndoRedo(false);
                if ( !routed && nativeTextEditor )
                {
                    (void)ui::shortcuts::triggerNativeTextEditorUndoRedo(
                        nativeTextEditor, false);
                }
                return;
            }
            if ( cogScriptEditorOwnsFocus() )
            {
                const bool routed = triggerActiveCogScriptEditorUndoRedo(false);
                if ( !routed && nativeTextEditor )
                {
                    (void)ui::shortcuts::triggerNativeTextEditorUndoRedo(
                        nativeTextEditor, false);
                }
                return;
            }
            if ( nativeTextEditor )
            {
                (void)ui::shortcuts::triggerNativeTextEditorUndoRedo(
                    nativeTextEditor, false);
                syncCogScriptEditorActionFocus();
                return;
            }
            if ( ui::shortcuts::focusAcceptsTextInput(focusWidget) )
            {
                return;
            }
            if ( glView_ && glView_->stagedCleaveHistoryOwnsUndo() )
            {
                if ( documentEditingAllowed() )
                {
                    (void)glView_->undoStagedCleaveHistory();
                    syncCogScriptEditorActionFocus();
                }
                return;
            }
            if ( !documentEditingAllowed() )
            {
                return;
            }
            const std::optional<std::uint64_t> cleaveApplyRecordId =
                document_.nextUndoRecordId();
            std::optional<CleaveApplyWorkspaceMemento> preparedCleaveApply{};
            if (cleaveApplyRecordId)
            {
                preparedCleaveApply = prepareCleaveApplyWorkspaceRestore(
                    *cleaveApplyRecordId);
                const bool attachmentExists = std::any_of(
                    cleaveApplyWorkspaceMementos_.begin(),
                    cleaveApplyWorkspaceMementos_.end(),
                    [recordId = *cleaveApplyRecordId](
                        const CleaveApplyWorkspaceMemento& value)
                    {
                        return value.recordId == recordId;
                    });
                if (attachmentExists && !preparedCleaveApply)
                {
                    if (glView_)
                    {
                        glView_->showToast(
                            "Undo could not prepare the saved Cleave workspace; the document was left unchanged",
                            3500,
                            GlView::ToastPlacement::BottomCenter,
                            GlView::ToastGroup::Error);
                    }
                    return;
                }
            }
            const editor::Selection prevSelection = document_.selection();
            const std::optional<editor::DocumentHistoryTransitionState>
                frameTransition = document_.transitionAtDepth(
                    document_.undoDepth());
            const FrameHistoryProbe frameProbe =
                captureFrameHistoryProbe(
                    prevSelection,
                    frameTransition ? &*frameTransition : nullptr);
            const std::vector<PreservedSurfaceVertex> preservedVertices =
                captureSurfaceVertices(prevSelection);
            const std::string opLabel(document_.nextUndoLabel());
            const std::uint64_t historyFromSignature = document_.undoSignature();
            const bool hadMotsArchLighting =
                hasMotsArchLighting(document_.level());
            const bool hadMotsArchBakeState =
                hasMotsArchBakeState(document_.level());
            const std::uint32_t transitionDirtyFlags = frameTransition
                ? frameTransition->dirtyFlags : 0u;
            const bool transitionHasTopologyFastPath = frameTransition
                && ui::history_detail::isKnownTopologyHistoryTransition(
                    transitionDirtyFlags);
            const bool transitionMayChangeSurfaceTopology = !frameTransition
                || ui::history_detail::historyDirtyMayChangeSurfaceTopology(
                    transitionDirtyFlags);
            std::vector<std::int64_t> surfaceTopologyBefore{};
            const bool compareSurfaceTopology =
                !transitionHasTopologyFastPath
                && transitionMayChangeSurfaceTopology
                && hasSurfaceSelectionContextForTopologyInvalidation();
            if (compareSurfaceTopology
                && !captureSurfaceSelectionTopologySnapshot(surfaceTopologyBefore))
            {
                if (glView_)
                {
                    glView_->showToast(
                        "Undo could not prepare the surface topology check; the document was left unchanged",
                        3500,
                        GlView::ToastPlacement::BottomCenter,
                        GlView::ToastGroup::Error);
                }
                return;
            }

            editor::DocumentDirty dirty = editor::DocumentDirty::None;
            std::vector<std::int32_t> compactThingTargets{};
            if (!document_.undo(&dirty, &compactThingTargets))
            {
                return;
            }
            const std::uint32_t rawDirtyFlags =
                static_cast<std::uint32_t>(dirty);
            dirty = static_cast<editor::DocumentDirty>(
                ui::history_detail::normalizeUnsupportedHistoryDirtyFlags(
                    rawDirtyFlags));
            cancelArchitecturalLightingInteractiveEdit();
            const bool compactThingTransform =
                dirty == editor::DocumentDirty::Things
                && !compactThingTargets.empty();
            const bool hasMotsArchLightingNow =
                hasMotsArchLighting(document_.level());
            const bool archLightingMayHaveChanged =
                hadMotsArchLighting || hasMotsArchLightingNow;
            const bool archBakeMayHaveChanged = hadMotsArchBakeState
                || hasMotsArchBakeState(document_.level());
            if (dirty == editor::DocumentDirty::All)
            {
                // Full snapshots can change the layer vector (for example,
                // undoing a cross-document clipboard layer import). Keep the
                // parallel visibility/lock/color state and GlView sets in
                // exactly the same cardinality before publishing refreshes.
                const bool restoredLayerVisuals = restoreLayerVisualHistoryForUndo(
                    historyFromSignature,
                    document_.undoSignature());
                syncLayerViewState();
                if (restoredLayerVisuals)
                {
                    persistLayerColors();
                }
            }
            else
            {
                // A non-layer edit can sit above an Edit-layers transaction.
                // Carry any later non-undoable visual preference changes back
                // to that history position so a subsequent layer Undo/Redo
                // does not revive stale colors, locks, or visibility.
                refreshLayerVisualHistoryForCurrentSignature();
            }

            const bool frameStorageChangedByUndo =
                frameStorageChanged(frameProbe);
            editor::Selection preserved = prevSelection;
            if ( frameStorageChangedByUndo )
            {
                // Frame raw indices belong to the pre-Undo Level. When the
                // frame count or a selected frame's stored value index changes,
                // keep the selection saved in the exact restored history state.
                preserved.frames = document_.selection().frames;
            }
            sanitizeSelection(preserved);
            restoreSurfaceVertices(preserved, preservedVertices);
            document_.selection() = std::move(preserved);
            const bool knownTopologyTransition =
                ui::history_detail::isKnownTopologyHistoryTransition(
                    rawDirtyFlags);
            const bool dirtyMayChangeSurfaceTopology =
                ui::history_detail::historyDirtyMayChangeSurfaceTopology(
                    rawDirtyFlags);
            const bool surfaceTopologyChanged = compareSurfaceTopology
                && dirtyMayChangeSurfaceTopology
                && !knownTopologyTransition
                && !matchesSurfaceSelectionTopologySnapshot(surfaceTopologyBefore);
            if (surfaceTopologyChanged)
            {
                // This history record's dirty mask did not establish that
                // its restored face ordinals are trustworthy. Do not let
                // restoreSurfaceVertices() promote an inherited positional
                // vertex owner into the remembered Face selection.
                document_.selection().surfaces.clear();
            }
            if (knownTopologyTransition || surfaceTopologyChanged)
            {
                invalidateSurfaceSelectionContextForTopologyEdit();
            }
            bool selectionUnchanged = selectionEquals(
                prevSelection, document_.selection());
            const bool topologyMayHaveChanged =
                dirty == editor::DocumentDirty::None
                || editor::hasAny(
                    dirty, editor::DocumentDirty::LevelGeometry);

            if (glView_)
            {
                const bool levelChanged = editor::hasAny(dirty, editor::DocumentDirty::LevelGeometry);
                const bool thingsChanged = editor::hasAny(dirty, editor::DocumentDirty::Things);
                const bool lightsChanged = editor::hasAny(dirty, editor::DocumentDirty::Lights);
                const bool unknown = dirty == editor::DocumentDirty::None;

                // Full structural geometry history cannot prove that positional
                // Sector/Surface refs still name the same owners. An admitted
                // Apply attachment restores its exact workspace after publication.
                if (!preparedCleaveApply
                    && (rawDirtyFlags
                            & static_cast<std::uint32_t>(
                                editor::DocumentDirty::LevelGeometry)) != 0u
                    && (rawDirtyFlags
                            & static_cast<std::uint32_t>(
                                editor::DocumentDirty::Structure)) != 0u)
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

                if (levelChanged || unknown)
                {
                    uvEditorForceDisplayRebuild_ = true;
                }

                if (unknown || (levelChanged && thingsChanged))
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
                        if (compactThingTransform)
                        {
                            glView_->notifyThingTransformsChanged(
                                compactThingTargets);
                        }
                        else if (archLightingMayHaveChanged)
                        {
                            glView_->notifyThingAppearanceChanged();
                        }
                        else
                        {
                            glView_->notifyThingsChanged();
                        }
                    }
                }

                if (lightsChanged)
                {
                    glView_->notifyLightsChanged();
                    std::vector<std::int32_t> bakedTargets{};
                    bakedTargets.reserve(document_.level().sectors.size());
                    for (std::size_t sectorIndex = 0;
                         sectorIndex < document_.level().sectors.size();
                         ++sectorIndex)
                    {
                        bakedTargets.push_back(static_cast<std::int32_t>(sectorIndex));
                    }
                    // Light-history snapshots include persisted baked colors
                    // and Ambient so source edits and their automatic output
                    // undo/redo atomically.
                    const bool thingAppearanceAlreadyInvalidated = unknown
                        || dirty == editor::DocumentDirty::All
                        || (thingsChanged && !compactThingTransform);
                    const bool archAppearanceNeedsRebuild = archBakeMayHaveChanged
                        && !thingAppearanceAlreadyInvalidated;
                    glView_->notifyBakedLightingChanged(
                        bakedTargets,
                        !thingAppearanceAlreadyInvalidated
                            && !archBakeMayHaveChanged);
                    if (archAppearanceNeedsRebuild)
                    {
                        // MotS 0x20 light snapshots also restore or recompute
                        // ARCH vertex colors. The full appearance rebuild
                        // reads the updated sector ambient after the surface
                        // bake, avoiding a redundant fast Thing recolor.
                        glView_->notifyThingAppearanceChanged();
                    }
                }

                if (topologyMayHaveChanged
                    && surfaceSelectionMode_ == SurfaceSelectionMode::edge)
                {
                    editor::Selection& selection = document_.selection();
                    selection.surfaces = glView_->selectedSurfaceEdgeOwners();
                    selection.surfaceVertices.clear();
                    selectionUnchanged = selectionEquals(
                        prevSelection, selection);
                }

                // Geometry publication can invalidate or promote an exact
                // vertex/edge while every owner Surface remains unchanged.
                // Always republish in that case; semantic set equality alone
                // cannot revalidate the GlView sidecars.
                if (!selectionUnchanged || topologyMayHaveChanged)
                {
                    applyDocumentSelectionToViewport();
                }
            }

            bool outlinerFast = false;
            const bool unknownDirty = (dirty == editor::DocumentDirty::None);
            const bool structureChanged = editor::hasAny(dirty, editor::DocumentDirty::Structure);

            if (!unknownDirty && structureChanged && outliner_ && outlinerTabs_)
            {
                const Level& levelNow = document_.level();
                const int tab = outlinerTabs_->currentIndex();
                const QSignalBlocker blocker(outliner_);

                auto removeLastIfInSync = [&](const int expectedCount, const char* countLabel, const int newCount) -> bool
                {
                    if (outliner_->topLevelItemCount() != expectedCount || expectedCount <= 0)
                    {
                        return false;
                    }
                    delete outliner_->takeTopLevelItem(expectedCount - 1);
                    if (outlinerCountLabel_)
                    {
                        outlinerCountLabel_->setText(QString("%1: %2").arg(countLabel).arg(newCount));
                    }
                    refreshOutlinerTabCaptions();
                    return true;
                };

                if (tab == 1 && editor::hasAny(dirty, editor::DocumentDirty::Things)
                    && (opLabel == "Thing: Added" || opLabel == "Add thing"))
                {
                    outlinerFast = removeLastIfInSync(static_cast<int>(levelNow.things.size()) + 1, "Things", static_cast<int>(levelNow.things.size()));
                }
                else if (tab == 3 && editor::hasAny(dirty, editor::DocumentDirty::Lights)
                    && (opLabel == "Baked Light: Added" || opLabel == "Add light"))
                {
                    outlinerFast = removeLastIfInSync(static_cast<int>(levelNow.lights.size()) + 1, "Lights", static_cast<int>(levelNow.lights.size()));
                }
                else if (tab == 0 && editor::hasAny(dirty, editor::DocumentDirty::LevelGeometry)
                    && (opLabel == "Sector: Added" || opLabel == "Add sector" || opLabel == "Add cube"))
                {
                    outlinerFast = removeLastIfInSync(static_cast<int>(levelNow.sectors.size()) + 1, "Sectors", static_cast<int>(levelNow.sectors.size()));
                }
            }

            bool rebuildOutlinerNeeded = false;
            if (unknownDirty)
            {
                rebuildOutlinerNeeded = true;
            }
            else if (structureChanged && !outlinerFast)
            {
                const int tab = outlinerTabs_ ? outlinerTabs_->currentIndex() : 0;
                if (tab == 0)
                {
                    rebuildOutlinerNeeded = editor::hasAny(dirty, editor::DocumentDirty::LevelGeometry);
                }
                else if (tab == 1)
                {
                    rebuildOutlinerNeeded = editor::hasAny(dirty, editor::DocumentDirty::Things);
                }
                else if (tab == 2)
                {
                    rebuildOutlinerNeeded = editor::hasAny(dirty, editor::DocumentDirty::Cogs);
                }
                else
                {
                    rebuildOutlinerNeeded = editor::hasAny(dirty, editor::DocumentDirty::Lights);
                }
            }

            if (rebuildOutlinerNeeded)
            {
                rebuildOutliner();
            }
            else if (!selectionUnchanged || outlinerFast)
            {
                updateOutlinerSelectionOnly();
            }
            if (compactThingTransform)
            {
                refreshThingChecks(compactThingTargets, false);
                updateThingPropertiesPreview(false);
            }
            else
            {
                scheduleRebuildChecks(
                    editor::DocumentDirty::LevelGeometry
                    | editor::DocumentDirty::Things
                    | editor::DocumentDirty::Lights
                    | editor::DocumentDirty::Structure);
                rebuildProperties();
            }
            updateEditActions();
            syncUvEditorPanel();
            if (preparedCleaveApply && cleaveApplyRecordId)
            {
                (void)restoreCleaveApplyWorkspaceAfterUndo(
                    *cleaveApplyRecordId,
                    std::move(*preparedCleaveApply));
            }
            // Undo is a semantic timeline transition even when it returns to
            // the saved Level (the redo branch is still user-visible state).
            // Mark it explicitly so recovery does not rely on the dirty flag
            // or lose the history on a hard termination.
            noteRecoveryStateChanged(true);
        });

    // Step 3: wire Redo and mirror the same targeted refresh behavior as Undo.
    redoAction_ = new QAction("&Redo", this);
    redoAction_->setObjectName("edit.redo");
    redoAction_->setShortcut(QKeySequence(
        int(Qt::ControlModifier) | int(Qt::ShiftModifier) | int(Qt::Key_Z)));
    connect(redoAction_, &QAction::triggered, this,
        [this, selectionEquals, sanitizeSelection, captureSurfaceVertices,
         restoreSurfaceVertices, applyDocumentSelectionToViewport,
         updateOutlinerSelectionOnly, captureFrameHistoryProbe,
         frameStorageChanged, hasMotsArchLighting,
         hasMotsArchBakeState]()
        {
            QWidget* const focusWidget = QApplication::focusWidget();
            QWidget* const nativeTextEditor =
                ui::shortcuts::focusedNativeTextEditor(focusWidget);
            if ( nativeTextEditor && nativeTextEditor != focusWidget
                && qobject_cast<QLineEdit*>(nativeTextEditor) )
            {
                (void)ui::shortcuts::triggerNativeTextEditorUndoRedo(
                    nativeTextEditor, true);
                syncCogScriptEditorActionFocus();
                return;
            }
            if ( resourceEditorOwnsFocus() )
            {
                const bool routed = triggerActiveResourceEditorUndoRedo(true);
                if ( !routed && nativeTextEditor )
                {
                    (void)ui::shortcuts::triggerNativeTextEditorUndoRedo(
                        nativeTextEditor, true);
                }
                return;
            }
            if ( cogScriptEditorOwnsFocus() )
            {
                const bool routed = triggerActiveCogScriptEditorUndoRedo(true);
                if ( !routed && nativeTextEditor )
                {
                    (void)ui::shortcuts::triggerNativeTextEditorUndoRedo(
                        nativeTextEditor, true);
                }
                return;
            }
            if ( nativeTextEditor )
            {
                (void)ui::shortcuts::triggerNativeTextEditorUndoRedo(
                    nativeTextEditor, true);
                syncCogScriptEditorActionFocus();
                return;
            }
            if ( ui::shortcuts::focusAcceptsTextInput(focusWidget) )
            {
                return;
            }
            const bool documentCleaveApplyRedo = cleaveApplyDocumentRedoHasPriority();
            const std::optional<std::uint64_t> nextRedoRecordId =
                document_.nextRedoRecordId();
            if ( glView_ && glView_->stagedCleaveHistoryOwnsUndo()
                && !documentCleaveApplyRedo )
            {
                if ( documentEditingAllowed() )
                {
                    (void)glView_->redoStagedCleaveHistory();
                    syncCogScriptEditorActionFocus();
                }
                return;
            }
            if ( !documentEditingAllowed() )
            {
                return;
            }
            const editor::Selection prevSelection = document_.selection();
            const std::optional<editor::DocumentHistoryTransitionState>
                frameTransition = document_.transitionAtDepth(
                    document_.undoDepth() + 1u);
            const FrameHistoryProbe frameProbe =
                captureFrameHistoryProbe(
                    prevSelection,
                    frameTransition ? &*frameTransition : nullptr);
            const std::vector<PreservedSurfaceVertex> preservedVertices =
                captureSurfaceVertices(prevSelection);
            const std::string opLabel(document_.nextRedoLabel());
            const std::uint64_t historyFromSignature = document_.undoSignature();
            const bool hadMotsArchLighting =
                hasMotsArchLighting(document_.level());
            const bool hadMotsArchBakeState =
                hasMotsArchBakeState(document_.level());
            const std::uint32_t transitionDirtyFlags = frameTransition
                ? frameTransition->dirtyFlags : 0u;
            const bool transitionHasTopologyFastPath = frameTransition
                && ui::history_detail::isKnownTopologyHistoryTransition(
                    transitionDirtyFlags);
            const bool transitionMayChangeSurfaceTopology = !frameTransition
                || ui::history_detail::historyDirtyMayChangeSurfaceTopology(
                    transitionDirtyFlags);
            std::vector<std::int64_t> surfaceTopologyBefore{};
            const bool compareSurfaceTopology =
                !transitionHasTopologyFastPath
                && transitionMayChangeSurfaceTopology
                && hasSurfaceSelectionContextForTopologyInvalidation();
            if (compareSurfaceTopology
                && !captureSurfaceSelectionTopologySnapshot(surfaceTopologyBefore))
            {
                if (glView_)
                {
                    glView_->showToast(
                        "Redo could not prepare the surface topology check; the document was left unchanged",
                        3500,
                        GlView::ToastPlacement::BottomCenter,
                        GlView::ToastGroup::Error);
                }
                return;
            }

            editor::DocumentDirty dirty = editor::DocumentDirty::None;
            std::vector<std::int32_t> compactThingTargets{};
            if (!document_.redo(&dirty, &compactThingTargets))
            {
                return;
            }
            const std::uint32_t rawDirtyFlags =
                static_cast<std::uint32_t>(dirty);
            dirty = static_cast<editor::DocumentDirty>(
                ui::history_detail::normalizeUnsupportedHistoryDirtyFlags(
                    rawDirtyFlags));
            const bool clearedCleaveApply = nextRedoRecordId
                && clearCleaveApplyWorkspaceAfterRedo(*nextRedoRecordId);
            cancelArchitecturalLightingInteractiveEdit();
            const bool compactThingTransform =
                dirty == editor::DocumentDirty::Things
                && !compactThingTargets.empty();
            const bool hasMotsArchLightingNow =
                hasMotsArchLighting(document_.level());
            const bool archLightingMayHaveChanged =
                hadMotsArchLighting || hasMotsArchLightingNow;
            const bool archBakeMayHaveChanged = hadMotsArchBakeState
                || hasMotsArchBakeState(document_.level());
            if (dirty == editor::DocumentDirty::All)
            {
                const bool restoredLayerVisuals = restoreLayerVisualHistoryForRedo(
                    historyFromSignature,
                    document_.undoSignature());
                syncLayerViewState();
                if (restoredLayerVisuals)
                {
                    persistLayerColors();
                }
            }
            else
            {
                refreshLayerVisualHistoryForCurrentSignature();
            }

            const bool frameStorageChangedByRedo =
                frameStorageChanged(frameProbe);
            editor::Selection preserved = prevSelection;
            if ( frameStorageChangedByRedo )
            {
                // Redo may recreate a frame at a raw index currently occupied
                // by another parameter. Restore the snapshot's frame refs.
                preserved.frames = document_.selection().frames;
            }
            sanitizeSelection(preserved);
            restoreSurfaceVertices(preserved, preservedVertices);
            document_.selection() = std::move(preserved);
            const bool knownTopologyTransition =
                ui::history_detail::isKnownTopologyHistoryTransition(
                    rawDirtyFlags);
            const bool dirtyMayChangeSurfaceTopology =
                ui::history_detail::historyDirtyMayChangeSurfaceTopology(
                    rawDirtyFlags);
            const bool surfaceTopologyChanged = compareSurfaceTopology
                && dirtyMayChangeSurfaceTopology
                && !knownTopologyTransition
                && !matchesSurfaceSelectionTopologySnapshot(surfaceTopologyBefore);
            if (surfaceTopologyChanged)
            {
                // This history record's dirty mask did not establish that
                // its restored face ordinals are trustworthy. Do not let
                // restoreSurfaceVertices() promote an inherited positional
                // vertex owner into the remembered Face selection.
                document_.selection().surfaces.clear();
            }
            if (knownTopologyTransition || surfaceTopologyChanged)
            {
                invalidateSurfaceSelectionContextForTopologyEdit();
            }
            bool selectionUnchanged = selectionEquals(
                prevSelection, document_.selection());
            const bool topologyMayHaveChanged =
                dirty == editor::DocumentDirty::None
                || editor::hasAny(
                    dirty, editor::DocumentDirty::LevelGeometry);

            if (glView_)
            {
                const bool levelChanged = editor::hasAny(dirty, editor::DocumentDirty::LevelGeometry);
                const bool thingsChanged = editor::hasAny(dirty, editor::DocumentDirty::Things);
                const bool lightsChanged = editor::hasAny(dirty, editor::DocumentDirty::Lights);
                const bool unknown = dirty == editor::DocumentDirty::None;

                if ((rawDirtyFlags
                            & static_cast<std::uint32_t>(
                                editor::DocumentDirty::LevelGeometry)) != 0u
                    && (rawDirtyFlags
                            & static_cast<std::uint32_t>(
                                editor::DocumentDirty::Structure)) != 0u
                    && !clearedCleaveApply)
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

                if (levelChanged || unknown)
                {
                    uvEditorForceDisplayRebuild_ = true;
                }

                if (unknown || (levelChanged && thingsChanged))
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
                        if (compactThingTransform)
                        {
                            glView_->notifyThingTransformsChanged(
                                compactThingTargets);
                        }
                        else if (archLightingMayHaveChanged)
                        {
                            glView_->notifyThingAppearanceChanged();
                        }
                        else
                        {
                            glView_->notifyThingsChanged();
                        }
                    }
                }

                if (lightsChanged)
                {
                    glView_->notifyLightsChanged();
                    std::vector<std::int32_t> bakedTargets{};
                    bakedTargets.reserve(document_.level().sectors.size());
                    for (std::size_t sectorIndex = 0;
                         sectorIndex < document_.level().sectors.size();
                         ++sectorIndex)
                    {
                        bakedTargets.push_back(static_cast<std::int32_t>(sectorIndex));
                    }
                    const bool thingAppearanceAlreadyInvalidated = unknown
                        || dirty == editor::DocumentDirty::All
                        || (thingsChanged && !compactThingTransform);
                    const bool archAppearanceNeedsRebuild = archBakeMayHaveChanged
                        && !thingAppearanceAlreadyInvalidated;
                    glView_->notifyBakedLightingChanged(
                        bakedTargets,
                        !thingAppearanceAlreadyInvalidated
                            && !archBakeMayHaveChanged);
                    if (archAppearanceNeedsRebuild)
                    {
                        // MotS 0x20 light snapshots also restore or recompute
                        // ARCH vertex colors. The full appearance rebuild
                        // reads the updated sector ambient after the surface
                        // bake, avoiding a redundant fast Thing recolor.
                        glView_->notifyThingAppearanceChanged();
                    }
                }

                if (topologyMayHaveChanged
                    && surfaceSelectionMode_ == SurfaceSelectionMode::edge)
                {
                    editor::Selection& selection = document_.selection();
                    selection.surfaces = glView_->selectedSurfaceEdgeOwners();
                    selection.surfaceVertices.clear();
                    selectionUnchanged = selectionEquals(
                        prevSelection, selection);
                }

                if (!selectionUnchanged || topologyMayHaveChanged)
                {
                    applyDocumentSelectionToViewport();
                }
            }

            bool outlinerFast = false;
            const bool unknownDirty = (dirty == editor::DocumentDirty::None);
            const bool structureChanged = editor::hasAny(dirty, editor::DocumentDirty::Structure);

            if (!unknownDirty && structureChanged && outliner_ && outlinerTabs_)
            {
                const Level& levelNow = document_.level();
                const int tab = outlinerTabs_->currentIndex();
                const QSignalBlocker blocker(outliner_);

                auto appendIfInSync = [&](const int expectedCountBefore, const char* countLabel, const int newCount, auto&& makeText, const OutlinerKind kind) -> bool
                {
                    if (outliner_->topLevelItemCount() != expectedCountBefore)
                    {
                        return false;
                    }

                    auto* item = new QTreeWidgetItem(outliner_);
                    item->setText(0, makeText());
                    item->setData(0, kOutlinerKindRole, static_cast<int>(kind));
                    item->setData(0, kOutlinerIndexRole, expectedCountBefore);

                    if (outlinerCountLabel_)
                    {
                        outlinerCountLabel_->setText(QString("%1: %2").arg(countLabel).arg(newCount));
                    }
                    refreshOutlinerTabCaptions();
                    return true;
                };

                if (tab == 1 && editor::hasAny(dirty, editor::DocumentDirty::Things)
                    && (opLabel == "Thing: Added" || opLabel == "Add thing"))
                {
                    const int idx = static_cast<int>(levelNow.things.size()) - 1;
                    if (idx >= 0 && static_cast<std::size_t>(idx) < levelNow.things.size())
                    {
                        const Thing& thing = levelNow.things[static_cast<std::size_t>(idx)];
                        outlinerFast = appendIfInSync(
                            static_cast<int>(levelNow.things.size()) - 1,
                            "Things",
                            static_cast<int>(levelNow.things.size()),
                            [&]() { return QString("%1: %2").arg(idx).arg(QString::fromStdString(thing.name)); },
                            OutlinerKind::thing);
                    }
                }
                else if (tab == 3 && editor::hasAny(dirty, editor::DocumentDirty::Lights)
                    && (opLabel == "Baked Light: Added" || opLabel == "Add light"))
                {
                    const int idx = static_cast<int>(levelNow.lights.size()) - 1;
                    if (idx >= 0 && static_cast<std::size_t>(idx) < levelNow.lights.size())
                    {
                        const SedLight& light = levelNow.lights[static_cast<std::size_t>(idx)];
                        outlinerFast = appendIfInSync(
                            static_cast<int>(levelNow.lights.size()) - 1,
                            "Lights",
                            static_cast<int>(levelNow.lights.size()),
                            [&]() { return QString("%1: Light (id %2)").arg(idx).arg(light.id); },
                            OutlinerKind::light);
                    }
                }
                else if (tab == 0 && editor::hasAny(dirty, editor::DocumentDirty::LevelGeometry)
                    && (opLabel == "Sector: Added" || opLabel == "Add sector" || opLabel == "Add cube"))
                {
                    const int idx = static_cast<int>(levelNow.sectors.size()) - 1;
                    if (idx >= 0 && static_cast<std::size_t>(idx) < levelNow.sectors.size())
                    {
                        const Sector& sector = levelNow.sectors[static_cast<std::size_t>(idx)];
                        outlinerFast = appendIfInSync(
                            static_cast<int>(levelNow.sectors.size()) - 1,
                            "Sectors",
                            static_cast<int>(levelNow.sectors.size()),
                            [&]() { return QString("%1: Sector (id %2, surfaces %3)").arg(idx).arg(sector.id).arg(sector.surfaces.size()); },
                            OutlinerKind::sector);
                    }
                }
            }

            bool rebuildOutlinerNeeded = false;
            if (unknownDirty)
            {
                rebuildOutlinerNeeded = true;
            }
            else if (structureChanged && !outlinerFast)
            {
                const int tab = outlinerTabs_ ? outlinerTabs_->currentIndex() : 0;
                if (tab == 0)
                {
                    rebuildOutlinerNeeded = editor::hasAny(dirty, editor::DocumentDirty::LevelGeometry);
                }
                else if (tab == 1)
                {
                    rebuildOutlinerNeeded = editor::hasAny(dirty, editor::DocumentDirty::Things);
                }
                else if (tab == 2)
                {
                    rebuildOutlinerNeeded = editor::hasAny(dirty, editor::DocumentDirty::Cogs);
                }
                else
                {
                    rebuildOutlinerNeeded = editor::hasAny(dirty, editor::DocumentDirty::Lights);
                }
            }

            if (rebuildOutlinerNeeded)
            {
                rebuildOutliner();
            }
            else if (!selectionUnchanged || outlinerFast)
            {
                updateOutlinerSelectionOnly();
            }
            if (compactThingTransform)
            {
                refreshThingChecks(compactThingTargets, false);
                updateThingPropertiesPreview(false);
            }
            else
            {
                scheduleRebuildChecks(
                    editor::DocumentDirty::LevelGeometry
                    | editor::DocumentDirty::Things
                    | editor::DocumentDirty::Lights
                    | editor::DocumentDirty::Structure);
                rebuildProperties();
            }
            updateEditActions();
            syncUvEditorPanel();
            // Redo likewise must survive a clean/dirty boundary and be
            // represented in the next recovery snapshot.
            noteRecoveryStateChanged(true);
        });

    if ( glView_ )
    {
        connect(
            glView_,
            &GlView::viewportStateChanged,
            this,
            &MainWindow::syncCogScriptEditorActionFocus,
            Qt::UniqueConnection);
    }
}

} // namespace sed
