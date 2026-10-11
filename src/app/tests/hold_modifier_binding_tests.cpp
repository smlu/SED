#include "test_common.hpp"

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "ui/shortcuts/selection_modifier_utils.hpp"

#include <QAction>
#include <QKeySequence>

#include <algorithm>

namespace sed::app_tests
{

void runHoldModifierBindingTests()
{
    MainWindow window(nullptr, true);
    QAction* const selectionAddAction =
        window.findChild<QAction*>(QStringLiteral("selection.mode.add"));
    QAction* const selectionRemoveAction =
        window.findChild<QAction*>(QStringLiteral("selection.mode.remove"));
    QAction* const surfaceBypassAction = window.findChild<QAction*>(
        QStringLiteral("selection.mode.surface_bypass_filters"));
    QAction* const surfacePathAction = window.findChild<QAction*>(
        QStringLiteral("selection.mode.surface_shortest_path"));
    QAction* const placementTiltAction = window.findChild<QAction*>(
        QStringLiteral("thing_placement.modifier.surface_tilt"));
    QAction* const placementFreeAction = window.findChild<QAction*>(
        QStringLiteral("thing_placement.modifier.free_insert"));
    QAction* const gridSnapAction = window.findChild<QAction*>(
        QStringLiteral("viewport.transform.modifier.invert_snap"));
    QAction* const gridStickAction = window.findChild<QAction*>(
        QStringLiteral("viewport.transform.modifier.invert_grid_stick"));
    QAction* const sectorAlignAction = window.findChild<QAction*>(
        QStringLiteral("sector_move.modifier.invert_snap_align"));
    QAction* const surfaceGeometryAction = window.findChild<QAction*>(
        QStringLiteral("surface_move.modifier.invert_nearby_snap"));
    QAction* const surfaceStickAction = window.findChild<QAction*>(
        QStringLiteral("thing_move.modifier.invert_surface_stick"));
    QAction* const surfaceAlignAction = window.findChild<QAction*>(
        QStringLiteral("thing_move.modifier.invert_surface_align"));
    QAction* const thingSnapAction = window.findChild<QAction*>(
        QStringLiteral("thing_move.modifier.invert_snap_to_thing"));
    QAction* const thingRotateAction = window.findChild<QAction*>(
        QStringLiteral("thing_move.modifier.invert_snap_rotate"));

    const QList<QKeySequence> addAlternatives{
        QKeySequence(Qt::Key_Shift),
        QKeySequence(Qt::Key_Alt),
        QKeySequence(int(Qt::ControlModifier) | int(Qt::AltModifier)),
    };
    const QList<QKeySequence> removeAlternatives{
        QKeySequence(Qt::Key_Control),
        QKeySequence(Qt::Key_Meta),
        QKeySequence(int(Qt::ControlModifier) | int(Qt::ShiftModifier)),
    };
    const QList<QKeySequence> bypassAlternatives{
        QKeySequence(Qt::Key_Alt),
        QKeySequence(Qt::Key_Meta),
        QKeySequence(int(Qt::ControlModifier) | int(Qt::MetaModifier)),
    };
    const QList<QKeySequence> gridSnapAlternatives{
        QKeySequence(Qt::Key_Control),
        QKeySequence(Qt::Key_Alt),
        QKeySequence(int(Qt::ControlModifier) | int(Qt::ShiftModifier)),
    };
    const QList<QKeySequence> gridStickAlternatives{
        QKeySequence(int(Qt::ShiftModifier) | int(Qt::AltModifier)),
        QKeySequence(Qt::Key_Meta),
    };
    const QList<QKeySequence> sectorAlignAlternatives{
        QKeySequence(Qt::Key_Control),
        QKeySequence(Qt::Key_Shift),
    };
    const QList<QKeySequence> surfaceGeometryAlternatives{
        QKeySequence(Qt::Key_Shift),
        QKeySequence(Qt::Key_Alt),
    };
    const QList<QKeySequence> surfaceStickAlternatives{
        QKeySequence(Qt::Key_Shift),
        QKeySequence(Qt::Key_Meta),
    };
    const QList<QKeySequence> surfaceAlignAlternatives{
        QKeySequence(int(Qt::ControlModifier) | int(Qt::ShiftModifier)),
        QKeySequence(int(Qt::AltModifier) | int(Qt::MetaModifier)),
    };
    const QList<QKeySequence> thingSnapAlternatives{
        QKeySequence(Qt::Key_Control),
        QKeySequence(Qt::Key_Alt),
    };
    const QList<QKeySequence> thingRotateAlternatives{
        QKeySequence(int(Qt::ControlModifier) | int(Qt::AltModifier)),
        QKeySequence(int(Qt::ShiftModifier) | int(Qt::MetaModifier)),
    };

    const QList<QAction*> actions{
        selectionAddAction, selectionRemoveAction, surfaceBypassAction,
        surfacePathAction, placementTiltAction, placementFreeAction,
        gridSnapAction, gridStickAction, sectorAlignAction,
        surfaceGeometryAction, surfaceStickAction, surfaceAlignAction,
        thingSnapAction, thingRotateAction,
    };
    const bool allActionsAvailable = std::all_of(
        actions.begin(), actions.end(), [](const QAction* action) { return action != nullptr; });
    check(allActionsAvailable,
        "hold modifier publication: MainWindow owns all selection, placement, and transform actions");
    if (!allActionsAvailable)
    {
        return;
    }

    selectionAddAction->setShortcuts(addAlternatives);
    selectionRemoveAction->setShortcuts(removeAlternatives);
    surfaceBypassAction->setShortcuts(bypassAlternatives);
    surfacePathAction->setShortcuts({
        QKeySequence(int(Qt::ControlModifier) | int(Qt::ShiftModifier)),
        QKeySequence(Qt::Key_Meta),
        QKeySequence(int(Qt::ControlModifier) | int(Qt::MetaModifier)),
    });
    placementTiltAction->setShortcuts({
        QKeySequence(Qt::Key_Control),
        QKeySequence(Qt::Key_Shift),
        QKeySequence(Qt::Key_Meta),
    });
    placementFreeAction->setShortcuts({
        QKeySequence(Qt::Key_Shift),
        QKeySequence(Qt::Key_Alt),
    });
    gridSnapAction->setShortcuts(gridSnapAlternatives);
    gridStickAction->setShortcuts(gridStickAlternatives);
    sectorAlignAction->setShortcuts(sectorAlignAlternatives);
    surfaceGeometryAction->setShortcuts(surfaceGeometryAlternatives);
    surfaceStickAction->setShortcuts(surfaceStickAlternatives);
    surfaceAlignAction->setShortcuts(surfaceAlignAlternatives);
    thingSnapAction->setShortcuts(thingSnapAlternatives);
    thingRotateAction->setShortcuts(thingRotateAlternatives);
    window.syncSelectionModifierBindingsForTest();

    const GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "hold modifier publication: MainWindow has its cached viewport");
    if (!view)
    {
        return;
    }

    check(window.selectionAddModifierBindingsForTest()
              == ui::shortcuts::ModifierBindings{
                  Qt::ShiftModifier,
                  Qt::AltModifier,
                  Qt::ControlModifier | Qt::AltModifier},
        "hold modifier publication: MainWindow caches each selection Add alternative");
    check(window.selectionRemoveModifierBindingsForTest()
              == ui::shortcuts::ModifierBindings{
                  Qt::ControlModifier,
                  Qt::MetaModifier,
                  Qt::ControlModifier | Qt::ShiftModifier},
        "hold modifier publication: MainWindow caches each selection Remove alternative");
    check(window.selectionShortestPathModifierBindingsForTest()
              == ui::shortcuts::ModifierBindings{
                  Qt::ControlModifier | Qt::ShiftModifier,
                  Qt::MetaModifier,
                  Qt::ControlModifier | Qt::MetaModifier},
        "hold modifier publication: MainWindow caches each Surface path alternative");
    check(window.selectionBypassSurfaceFiltersModifierBindingsForTest()
              == ui::shortcuts::ModifierBindings{
                  Qt::AltModifier,
                  Qt::MetaModifier,
                  Qt::ControlModifier | Qt::MetaModifier},
        "hold modifier publication: MainWindow caches each filter-bypass alternative");
    check(window.thingPlacementTiltModifierBindingsForTest()
              == ui::shortcuts::ModifierBindings{
                  Qt::ControlModifier,
                  Qt::ShiftModifier,
                  Qt::MetaModifier}
            && window.thingPlacementFreeModifierBindingsForTest()
                == ui::shortcuts::ModifierBindings{
                    Qt::ShiftModifier,
                    Qt::AltModifier},
        "hold modifier publication: MainWindow caches all placement alternatives");
    check(window.transformGridSnapInvertModifierBindingsForTest()
              == ui::shortcuts::ModifierBindings{
                  Qt::ControlModifier,
                  Qt::AltModifier,
                  Qt::ControlModifier | Qt::ShiftModifier}
            && window.transformGridStickInvertModifierBindingsForTest()
                == ui::shortcuts::ModifierBindings{
                    Qt::ShiftModifier | Qt::AltModifier,
                    Qt::MetaModifier}
            && window.sectorMoveSnapAlignInvertModifierBindingsForTest()
                == ui::shortcuts::ModifierBindings{
                    Qt::ControlModifier,
                    Qt::ShiftModifier}
            && window.surfaceGeometrySnapInvertModifierBindingsForTest()
                == ui::shortcuts::ModifierBindings{
                    Qt::ShiftModifier,
                    Qt::AltModifier}
            && window.thingMoveSurfaceStickInvertModifierBindingsForTest()
                == ui::shortcuts::ModifierBindings{
                    Qt::ShiftModifier,
                    Qt::MetaModifier}
            && window.thingMoveSurfaceAlignInvertModifierBindingsForTest()
                == ui::shortcuts::ModifierBindings{
                    Qt::ControlModifier | Qt::ShiftModifier,
                    Qt::AltModifier | Qt::MetaModifier}
            && window.thingMoveSnapInvertModifierBindingsForTest()
                == ui::shortcuts::ModifierBindings{
                    Qt::ControlModifier,
                    Qt::AltModifier}
            && window.thingMoveSnapRotateInvertModifierBindingsForTest()
                == ui::shortcuts::ModifierBindings{
                    Qt::ControlModifier | Qt::AltModifier,
                    Qt::ShiftModifier | Qt::MetaModifier},
        "hold modifier publication: MainWindow caches alternatives for all eight transform holds");

    check(view->selectionAddModifierBindingsForTest()
              == ui::shortcuts::ModifierBindings{
                  Qt::ShiftModifier,
                  Qt::AltModifier,
                  Qt::ControlModifier | Qt::AltModifier},
        "hold modifier publication: selection Add alternatives reach the GlView cache");
    check(view->selectionRemoveModifierBindingsForTest()
              == ui::shortcuts::ModifierBindings{
                  Qt::ControlModifier,
                  Qt::MetaModifier,
                  Qt::ControlModifier | Qt::ShiftModifier},
        "hold modifier publication: selection Remove alternatives reach the GlView cache");
    check(view->selectionSurfaceBypassModifierBindingsForTest()
              == ui::shortcuts::ModifierBindings{
                  Qt::AltModifier,
                  Qt::MetaModifier,
                  Qt::ControlModifier | Qt::MetaModifier},
        "hold modifier publication: Surface filter-bypass alternatives reach the GlView cache");
    check(view->transformGridSnapModifierBindingsForTest()
              == ui::shortcuts::ModifierBindings{
                  Qt::ControlModifier,
                  Qt::AltModifier,
                  Qt::ControlModifier | Qt::ShiftModifier},
        "hold modifier publication: transform hold alternatives reach the GlView cache");

    check(window.thingPlacementModeForModifiersForTest(Qt::AltModifier) == 2,
        "hold modifier placement: the secondary Free binding selects Free mode");
    check(window.thingPlacementModeForModifiersForTest(Qt::MetaModifier) == 1,
        "hold modifier placement: the third Tilt binding selects Tilt mode");
    placementFreeAction->setShortcuts({
        QKeySequence(Qt::Key_Shift),
        QKeySequence(Qt::Key_Alt),
        QKeySequence(Qt::Key_Meta),
    });
    window.syncSelectionModifierBindingsForTest();
    check(window.thingPlacementModeForModifiersForTest(Qt::MetaModifier) == 2,
        "hold modifier placement: Free keeps priority when a held chord also matches Tilt");
    placementFreeAction->setShortcuts({});
    window.syncSelectionModifierBindingsForTest();
    check(window.thingPlacementFreeModifierBindingsForTest().isEmpty()
              && window.thingPlacementModeForModifiersForTest(Qt::AltModifier) == 0,
        "hold modifier placement: clearing Free disables it and removes its stale cached alternatives");

    const QString addHint = window.selectionModifierHintForTest(
        ui::shortcuts::SelectionHintMode::surface, Qt::AltModifier);
    check(addHint.contains(QStringLiteral("Add To Selection"))
              && addHint.contains(QStringLiteral("Xray Surfaces")),
        "hold modifier hints: a secondary Add and bypass alternative resolves from the cached lists");
    const QString pathHint = window.selectionModifierHintForTest(
        ui::shortcuts::SelectionHintMode::surface,
        Qt::ControlModifier | Qt::MetaModifier);
    check(pathHint.contains(QStringLiteral("Pick Shortest Path"))
              && !pathHint.contains(QStringLiteral("Add To Selection"))
              && !pathHint.contains(QStringLiteral("Remove From Selection")),
        "hold modifier hints: a later Surface path alternative suppresses Add and Remove");

    selectionAddAction->setShortcuts({});
    selectionRemoveAction->setShortcuts({QKeySequence(Qt::Key_Alt)});
    window.syncSelectionModifierBindingsForTest();
    check(window.selectionAddModifierBindingsForTest().isEmpty()
              && window.selectionRemoveModifierBindingsForTest()
                  == ui::shortcuts::ModifierBindings{Qt::AltModifier}
              && view->selectionAddModifierBindingsForTest().isEmpty()
              && view->selectionRemoveModifierBindingsForTest()
                  == ui::shortcuts::ModifierBindings{Qt::AltModifier},
        "hold modifier publication: a live re-sync disables cleared bindings and replaces changed ones");
}

} // namespace sed::app_tests
