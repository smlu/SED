#include "test_common.hpp"

#include <QAction>

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/level/build.hpp"
#include "sed/level/cleave.hpp"

namespace sed::app_tests
{
namespace
{

constexpr SurfaceRef kSplitOwner{0, 0};

editor::DocumentState cubeState()
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.layers.emplace_back("Layer0");
    Sector& sector = level::levelCreateCube(state.level, 0.0, 0.0, 0.0, TVector::up, TVector::right);
    sector.layer = 0;
    return state;
}

bool splitSurfaceEdgeAtMidpoint(editor::DocumentState& state, const SurfaceRef owner, const std::int32_t localEdge)
{
    if ( owner.sector < 0 || owner.surface < 0 || localEdge < 0 ||
         static_cast<std::size_t>(owner.sector) >= state.level.sectors.size() )
    {
        return false;
    }
    const Sector& sector = state.level.sectors[static_cast<std::size_t>(owner.sector)];
    if ( static_cast<std::size_t>(owner.surface) >= sector.surfaces.size() )
    {
        return false;
    }
    const Surface& surface = sector.surfaces[static_cast<std::size_t>(owner.surface)];
    if ( static_cast<std::size_t>(localEdge) >= surface.vertices.size() )
    {
        return false;
    }

    const std::size_t first = static_cast<std::size_t>(localEdge);
    const std::size_t second = (first + 1u) % surface.vertices.size();
    const Vertex& a = sector.vertices[static_cast<std::size_t>(surface.vertices[first])];
    const Vertex& b = sector.vertices[static_cast<std::size_t>(surface.vertices[second])];
    TVector planeNormal{b.x - a.x, b.y - a.y, b.z - a.z};
    planeNormal.Normalize();
    const TVector midpoint{(a.x + b.x) * 0.5, (a.y + b.y) * 0.5, (a.z + b.z) * 0.5};
    return level::levelCleaveEdge(state.level, owner, localEdge, planeNormal, midpoint.x, midpoint.y, midpoint.z);
}

std::optional<std::int32_t> findLocalVertex(const Level& level, const SurfaceRef owner, const std::int32_t sectorVertex)
{
    const Surface& surface =
        level.sectors[static_cast<std::size_t>(owner.sector)].surfaces[static_cast<std::size_t>(owner.surface)];
    const auto found = std::find(surface.vertices.begin(), surface.vertices.end(), sectorVertex);
    if ( found == surface.vertices.end() )
    {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(std::distance(surface.vertices.begin(), found));
}

std::pair<std::int32_t, std::int32_t> edgeEndpoints(const Level& level, const SurfaceRef owner,
                                                    const std::int32_t localEdge)
{
    const Surface& surface =
        level.sectors[static_cast<std::size_t>(owner.sector)].surfaces[static_cast<std::size_t>(owner.surface)];
    const std::size_t first = static_cast<std::size_t>(localEdge);
    const std::size_t second = (first + 1u) % surface.vertices.size();
    return {surface.vertices[first], surface.vertices[second]};
}

QAction* historyAction(MainWindow& window, const char* name)
{
    window.updateEditActionsForTest();
    return window.findChild<QAction*>(QString::fromLatin1(name));
}

bool isOnlySurface(const editor::Selection& selection, const SurfaceRef expected)
{
    return selection.surfaces.size() == 1u && selection.surfaces.contains(expected);
}

bool isOnlySurfaceVertex(const editor::Selection& selection, const editor::SurfaceVertexRef expected)
{
    return selection.surfaceVertices.size() == 1u && selection.surfaceVertices.contains(expected);
}

} // namespace

void runSurfaceSelectionHistoryTests()
{
    {
        // Split edge zero so every old corner after it changes local index.  Undo
        // must preserve the old sector-vertex identity, not accidentally retain a
        // local index which now names a different corner.  The transient midpoint
        // exists only in the post-edit topology and must be discarded.
        auto window = std::make_unique<MainWindow>();
        window->setAttribute(Qt::WA_DontShowOnScreen);
        editor::DocumentState state = cubeState();
        const Surface& beforeSurface = state.level.sectors[0].surfaces[0];
        const std::int32_t survivingSectorVertex = beforeSurface.vertices.back();
        const std::int32_t beforeSurvivingLocal = static_cast<std::int32_t>(beforeSurface.vertices.size() - 1u);
        const std::size_t beforeSectorVertexCount = state.level.sectors[0].vertices.size();

        window->setDocumentStateForTest(std::move(state));
        window->activateSurfaceEditModeForTest();
        window->activateSurfaceVertexSubModeForTest();
        const bool editApplied = window->applyStructuralHistoryEditForTest(
            [](editor::DocumentState& edited) { return splitSurfaceEdgeAtMidpoint(edited, kSplitOwner, 0); });
        GlView* view = window->glViewForTest();
        if ( view )
        {
            view->notifyLevelGeometryChanged();
        }

        const Level& afterLevel = window->documentStateForTest().level;
        const auto afterSurvivingLocal = findLocalVertex(afterLevel, kSplitOwner, survivingSectorVertex);
        const std::int32_t addedSectorVertex = static_cast<std::int32_t>(beforeSectorVertexCount);
        const auto addedLocal = findLocalVertex(afterLevel, kSplitOwner, addedSectorVertex);

        editor::Selection& selection = window->mutableDocumentStateForTest().selection;
        selection.clear();
        if ( afterSurvivingLocal && addedLocal && view )
        {
            selection.surfaceVertices.insert(editor::SurfaceVertexRef{kSplitOwner, *afterSurvivingLocal});
            selection.surfaceVertices.insert(editor::SurfaceVertexRef{kSplitOwner, *addedLocal});
            selection.surfaces.insert(kSplitOwner);
            view->setSelectionState(0, 0, -1, -1, selection);
            view->setActiveSurfaceVertexHint(0, 0, *addedLocal);
        }

        QAction* undo = historyAction(*window, "edit.undo");
        const bool undoReady = undo && undo->isEnabled();
        if ( undoReady )
        {
            undo->trigger();
        }
        const editor::SurfaceVertexRef expectedBefore{kSplitOwner, beforeSurvivingLocal};
        const editor::Selection& undoSelection = window->documentStateForTest().selection;
        const bool undoRemapped =
            isOnlySurfaceVertex(undoSelection, expectedBefore) && isOnlySurface(undoSelection, kSplitOwner) && view &&
            view->activeSurfaceVertexForTest() == std::optional<editor::SurfaceVertexRef>{expectedBefore};

        QAction* redo = historyAction(*window, "edit.redo");
        const bool redoReady = redo && redo->isEnabled();
        if ( redoReady )
        {
            redo->trigger();
        }
        const editor::SurfaceVertexRef expectedAfter{kSplitOwner, afterSurvivingLocal.value_or(-1)};
        const editor::Selection& redoSelection = window->documentStateForTest().selection;
        check(editApplied && afterSurvivingLocal && addedLocal && *afterSurvivingLocal != beforeSurvivingLocal &&
                  undoReady && undoRemapped && redoReady && isOnlySurfaceVertex(redoSelection, expectedAfter) &&
                  isOnlySurface(redoSelection, kSplitOwner) && view &&
                  view->activeSurfaceVertexForTest() == std::optional<editor::SurfaceVertexRef>{expectedAfter},
              "Topology Undo/Redo preserves a surviving Surface Vertex by identity "
              "and drops a transient local vertex without retargeting it");
    }

    {
        // A closing-edge split keeps local zero stable but changes the old closing
        // local's endpoints.  Keep a second, unaffected face selected so direct
        // Undo/Redo must drop the stale exact edge, promote the valid survivor,
        // and republish that survivor as the sole semantic/active owner.
        auto window = std::make_unique<MainWindow>();
        window->setAttribute(Qt::WA_DontShowOnScreen);
        editor::DocumentState state = cubeState();
        const Surface& beforeSplit = state.level.sectors[0].surfaces[0];
        const std::int32_t closingEdge = static_cast<std::int32_t>(beforeSplit.vertices.size() - 1u);
        const auto beforeClosingEndpoints = edgeEndpoints(state.level, kSplitOwner, closingEdge);

        window->setDocumentStateForTest(std::move(state));
        window->activateSurfaceEditModeForTest();
        window->activateSurfaceEdgeSubModeForTest();
        const bool editApplied = window->applyStructuralHistoryEditForTest(
            [closingEdge](editor::DocumentState& edited)
            { return splitSurfaceEdgeAtMidpoint(edited, kSplitOwner, closingEdge); });
        GlView* view = window->glViewForTest();
        if ( view )
        {
            view->notifyLevelGeometryChanged();
        }

        const Level& afterLevel = window->documentStateForTest().level;
        const Sector& afterSector = afterLevel.sectors[0];
        const std::int32_t addedSectorVertex = static_cast<std::int32_t>(afterSector.vertices.size() - 1u);
        std::optional<SurfaceRef> stableOwner{};
        for ( std::size_t surfaceIndex = 1; surfaceIndex < afterSector.surfaces.size(); ++surfaceIndex )
        {
            const Surface& surface = afterSector.surfaces[surfaceIndex];
            if ( std::find(surface.vertices.begin(), surface.vertices.end(), addedSectorVertex) ==
                 surface.vertices.end() )
            {
                stableOwner = SurfaceRef{0, static_cast<std::int32_t>(surfaceIndex)};
                break;
            }
        }
        const bool staleEdgeChanged = edgeEndpoints(afterLevel, kSplitOwner, closingEdge) != beforeClosingEndpoints;
        const auto installEdgeSelection = [&]()
        {
            editor::Selection& selection = window->mutableDocumentStateForTest().selection;
            selection.clear();
            if ( !stableOwner || !view )
            {
                return;
            }
            selection.surfaces.insert(*stableOwner);
            selection.surfaces.insert(kSplitOwner);
            view->setSelectionState(0, 0, -1, -1, selection);
            view->setSelectedSurfaceEdgesForTest({{*stableOwner, 0}, {kSplitOwner, closingEdge}}, 1u);
        };
        installEdgeSelection();

        QAction* undo = historyAction(*window, "edit.undo");
        const bool undoReady = undo && undo->isEnabled();
        if ( undoReady )
        {
            undo->trigger();
        }
        const bool undoPromoted = stableOwner && view && view->selectedSurfaceEdgeCountForTest() == 1u &&
                                  view->activeSurfaceEdgeForTest() ==
                                      std::optional<std::pair<SurfaceRef, std::int32_t>>{std::pair{*stableOwner, 0}} &&
                                  view->selectedSurfaceRef() == std::optional<SurfaceRef>{*stableOwner} &&
                                  isOnlySurface(window->documentStateForTest().selection, *stableOwner);

        // Reinstall a transient edge in the pre-edit topology so Redo exercises
        // the inverse endpoint mismatch instead of merely carrying Undo's result.
        installEdgeSelection();
        QAction* redo = historyAction(*window, "edit.redo");
        const bool redoReady = redo && redo->isEnabled();
        if ( redoReady )
        {
            redo->trigger();
        }
        const bool redoPromoted = stableOwner && view && view->selectedSurfaceEdgeCountForTest() == 1u &&
                                  view->activeSurfaceEdgeForTest() ==
                                      std::optional<std::pair<SurfaceRef, std::int32_t>>{std::pair{*stableOwner, 0}} &&
                                  view->selectedSurfaceRef() == std::optional<SurfaceRef>{*stableOwner} &&
                                  isOnlySurface(window->documentStateForTest().selection, *stableOwner);

        check(editApplied && stableOwner && staleEdgeChanged && undoReady && undoPromoted && redoReady && redoPromoted,
              "Topology Undo/Redo invalidates a transient exact Surface Edge and "
              "promotes its valid edge/owner without retaining stale sidecars");
    }
}

} // namespace sed::app_tests
