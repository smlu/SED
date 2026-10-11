#include "test_common.hpp"
#include "main_window/main_window.hpp"
#include "gl_view.hpp"
#include "sed/assets/sed.hpp"
#include <QAction>
#include <cmath>
#include <algorithm>
#include <limits>

namespace sed::app_tests
{
void runThingTransformCommandTests()
{
    MainWindow window;
    GlView* const view = window.glViewForTest();
    check(view != nullptr, "Thing command fixture exposes the real viewport");
    if (!view)
    {
        return;
    }
    QAction* frameAction = nullptr;
    for (auto* action : window.findChildren<QAction*>())
    {
        QString text = action->text();
        text.remove('&');
        if (action->isCheckable() && text == QStringLiteral("Frame"))
        {
            frameAction = action;
        }
    }
    check(frameAction != nullptr, "Thing command fixture exposes Frame mode");
    if (!frameAction)
    {
        return;
    }
    const auto fixture = []
    {
        editor::DocumentState state;
        state.level.kind = ProjectType::IJIM;
        state.level.header.version = 3;
        Thing owner;
        owner.name = "transform owner";
        owner.x = 1;
        owner.y = 2;
        owner.z = 3;
        TplValue frame;
        frame.name = "frame";
        frame.type = ValueType::frame;
        frame.stringValue = " (1/2/3:0/0/0) ";
        owner.values.push_back(frame);
        state.level.things.push_back(owner);
        state.selection.things.insert(0);
        state.selection.frames.insert({0, 0});
        return state;
    };
    const auto sameSelection = [](const editor::Selection& actual, const editor::Selection& expected)
    {
        const auto equalSet = [](const auto& left, const auto& right)
        {
            return left.size() == right.size() && std::all_of(left.begin(), left.end(),
                                                              [&right](const auto& item)
                                                              {
                                                                  return right.contains(item);
                                                              });
        };
        return equalSet(actual.sectors, expected.sectors) && equalSet(actual.surfaces, expected.surfaces) &&
               equalSet(actual.surfaceVertices, expected.surfaceVertices) &&
               equalSet(actual.things, expected.things) && equalSet(actual.frames, expected.frames) &&
               equalSet(actual.lights, expected.lights) && equalSet(actual.cogs, expected.cogs);
    };
    const auto invoke = [view](int operation, const TVector& delta, double angle)
    {
        const QVector3D axis(0, 0, 1);
        if (operation == 0)
        {
            view->thingsTranslateRequested(delta.x, delta.y, delta.z);
        }
        else if (operation == 1)
        {
            view->frameTranslateRequested(0, 0, delta.x, delta.y, delta.z);
        }
        else if (operation == 2)
        {
            view->thingsSnapTransformRequested(delta, axis, angle, TVector::zero);
        }
        else if (operation == 3)
        {
            view->frameSnapTransformRequested(0, 0, delta, axis, angle, TVector::zero);
        }
        else if (operation == 4)
        {
            view->thingsRotateRequested(axis, angle, delta);
        }
        else
        {
            view->frameRotateRequested(0, 0, axis, angle, delta);
        }
    };
    for (int operation = 0; operation != 6; ++operation)
    {
        window.setDocumentStateForTest(fixture());
        if (operation == 1 || operation == 3 || operation == 5)
        {
            frameAction->trigger();
        }
        else
        {
            window.activateThingEditModeForTest();
        }
        check(window.editModeForTest() == ((operation == 1 || operation == 3 || operation == 5)
                                               ? window.frameEditModeValueForTest()
                                               : window.thingEditModeValueForTest()),
              "Thing command fixture activates the mode required by each real signal");
        for (int invalid = 0; invalid != (operation >= 2 ? 6 : 3); ++invalid)
        {
            auto state = fixture();
            if (invalid == 2)
            {
                state.level.things[0].x = std::numeric_limits<double>::max();
                state.level.things[0].values[0].stringValue = "(1.7976931348623157e+308/2/3:0/0/0)";
            }
            window.setDocumentStateForTest(state);
            // Seed a real redo row: failed commands must retain it as well.
            window.pushUndoForTitleForTest(editor::DocumentDirty::Things);
            window.mutableDocumentStateForTest().level.things[0].z = 9;
            check(window.undoForTest(), "Thing command fixture seeds a retained Redo row");
            const auto before = window.documentStateForTest();
            const auto undoDepth = window.undoDepthForTest();
            const auto redoDepth = window.redoDepthForTest();
            const TVector delta = invalid == 0   ? TVector::zero
                                  : invalid == 1 ? TVector{std::numeric_limits<double>::infinity(), 0, 0}
                                                 : TVector{std::numeric_limits<double>::max(), 0, 0};
            // Rotate around X for overflow cases so X remains near max.
            if (invalid >= 3)
            {
                const TVector pivot =
                    invalid == 3 ? TVector{std::numeric_limits<double>::infinity(), 0, 0} : TVector::zero;
                const QVector3D axis = invalid == 4 ? QVector3D(std::numeric_limits<float>::quiet_NaN(), 0, 1)
                                                    : QVector3D(0, 0, 1);
                const double angle = invalid == 5 ? std::numeric_limits<double>::infinity() : 90;
                if (operation == 2)
                {
                    view->thingsSnapTransformRequested(TVector::zero, axis, angle, pivot);
                }
                else if (operation == 3)
                {
                    view->frameSnapTransformRequested(0, 0, TVector::zero, axis, angle, pivot);
                }
                else if (operation == 4)
                {
                    view->thingsRotateRequested(axis, angle, pivot);
                }
                else
                {
                    view->frameRotateRequested(0, 0, axis, angle, pivot);
                }
            }
            else if (invalid == 2 && operation >= 2)
            {
                if (operation == 2)
                {
                    view->thingsSnapTransformRequested(delta, QVector3D(1, 0, 0), 90, TVector::zero);
                }
                else if (operation == 3)
                {
                    view->frameSnapTransformRequested(0, 0, delta, QVector3D(1, 0, 0), 90, TVector::zero);
                }
                else if (operation == 4)
                {
                    view->thingsRotateRequested(QVector3D(0, 1, 0), 90,
                                                TVector{-std::numeric_limits<double>::max(), 0, 0});
                }
                else
                {
                    view->frameRotateRequested(0, 0, QVector3D(0, 1, 0), 90,
                                               TVector{-std::numeric_limits<double>::max(), 0, 0});
                }
            }
            else
            {
                invoke(operation, delta, invalid == 0 ? 0 : 90);
            }
            check(assets::sedLevelsEqual(window.documentStateForTest().level, before.level) &&
                      sameSelection(window.documentStateForTest().selection, before.selection) &&
                      window.undoDepthForTest() == undoDepth && window.redoDepthForTest() == redoDepth,
                  "Real Thing/Frame translate/snap/rotate callbacks reject zero, nonfinite and overflowing "
                  "input without Level/history changes");
        }
    }
    for (int operation = 0; operation != 6; ++operation)
    {
        for (int overflowingOwner = 0; overflowingOwner != 2; ++overflowingOwner)
        {
            auto state = fixture();
            state.level.things.push_back(state.level.things.front());
            state.selection.things.insert(1);
            state.selection.frames.insert({1, 0});
            state.level.things[overflowingOwner].x = std::numeric_limits<double>::max();
            state.level.things[overflowingOwner].values[0].stringValue =
                "(1.7976931348623157e+308/2/3:0/0/0)";
            window.setDocumentStateForTest(state);
            if (operation == 1 || operation == 3 || operation == 5)
            {
                if (!frameAction->isChecked())
                {
                    frameAction->trigger();
                }
            }
            else
            {
                window.activateThingEditModeForTest();
            }
            const auto before = window.documentStateForTest();
            const TVector translation{std::numeric_limits<double>::max(), 0, 0};
            if (operation < 2)
            {
                invoke(operation, translation, 0);
            }
            else if (operation == 2)
            {
                view->thingsSnapTransformRequested(translation, QVector3D(1, 0, 0), 90, TVector::zero);
            }
            else if (operation == 3)
            {
                view->frameSnapTransformRequested(0, 0, translation, QVector3D(1, 0, 0), 90, TVector::zero);
            }
            else if (operation == 4)
            {
                view->thingsRotateRequested(QVector3D(0, 1, 0), 90,
                                            TVector{-std::numeric_limits<double>::max(), 0, 0});
            }
            else
            {
                view->frameRotateRequested(0, 0, QVector3D(0, 1, 0), 90,
                                           TVector{-std::numeric_limits<double>::max(), 0, 0});
            }
            // Exercise both placements of the failing owner so one case fails
            // after a valid owner regardless of unordered selection iteration.
            check(assets::sedLevelsEqual(window.documentStateForTest().level, before.level) &&
                      sameSelection(window.documentStateForTest().selection, before.selection) &&
                      window.undoDepthForTest() == 0 && window.redoDepthForTest() == 0,
                  "Late overflow in a multi-owner Thing/Frame batch rolls back every earlier selected owner "
                  "and history");
        }
    }
    for (int operation : {0, 1})
    {
        window.setDocumentStateForTest(fixture());
        if (operation == 1)
        {
            frameAction->trigger();
        }
        else
        {
            window.activateThingEditModeForTest();
        }
        invoke(operation, TVector{1, 0, 0}, 0);
        const Thing& moved = window.documentStateForTest().level.things[0];
        check(window.undoDepthForTest() == 1 && moved.x == (operation == 0 ? 2 : 1) && moved.y == 2 &&
                  moved.z == 3 && moved.pch == 0 && moved.yaw == 0 && moved.rol == 0 &&
                  moved.sector == -1 && moved.values[0].asString() == "(2/2/3:0/0/0)",
              "Real Thing/Frame translate commits exact position and full fixed Frame token in one row");
        const auto committed = window.documentStateForTest();
        check(window.undoForTest() && window.documentStateForTest().level.things[0].x == 1 &&
                  window.documentStateForTest().level.things[0].values[0].asString() == " (1/2/3:0/0/0) ",
              "Real translate Undo preserves the complete original authored Frame token");
        check(window.redoForTest() &&
                  assets::sedLevelsEqual(window.documentStateForTest().level, committed.level) &&
                  sameSelection(window.documentStateForTest().selection, committed.selection),
              "Real translate Redo preserves the complete committed state and selection");
    }
    for (int operation : {2, 3, 4, 5})
    {
        auto state = fixture();
        state.level.things[0].x = 2;
        state.level.things[0].y = 1;
        state.level.things[0].values[0].stringValue = " (2/1/3:0/0/0) ";
        window.setDocumentStateForTest(state);
        if (operation == 3 || operation == 5)
        {
            frameAction->trigger();
        }
        else
        {
            window.activateThingEditModeForTest();
        }
        const auto before = window.documentStateForTest();
        const TVector pivot{1, 1, 0};
        const TVector translation{3, -4, 5};
        const QVector3D axis(0, 0, 1);
        if (operation == 2)
        {
            view->thingsSnapTransformRequested(translation, axis, 90, pivot);
        }
        else if (operation == 3)
        {
            view->frameSnapTransformRequested(0, 0, translation, axis, 90, pivot);
        }
        else if (operation == 4)
        {
            view->thingsRotateRequested(axis, 90, pivot);
        }
        else
        {
            view->frameRotateRequested(0, 0, axis, 90, pivot);
        }
        const auto committed = window.documentStateForTest();
        const Thing& moved = committed.level.things[0];
        check(window.undoDepthForTest() == 1 &&
                  moved.x == (operation == 2   ? 4
                              : operation == 4 ? 1
                                               : 2) &&
                  moved.y == (operation == 2   ? -2
                              : operation == 4 ? 2
                                               : 1) &&
                  moved.z == (operation == 2 ? 8 : 3) && moved.pch == 0 &&
                  moved.yaw == (operation == 2 || operation == 4 ? 90 : 0) && moved.rol == 0 &&
                  moved.sector == -1 &&
                  moved.values[0].asString() == (operation == 2 || operation == 4 ? " (2/1/3:0/0/0) "
                                                 : operation == 3                 ? "(4/-2/8:0/90/-0)"
                                                                                  : "(1/2/3:0/90/-0)"),
              "Real Thing/Frame snap/rotate commits fixed analytic off-axis/nonzero-pivot position and full "
              "authored Frame token");
        check(window.undoForTest() &&
                  assets::sedLevelsEqual(window.documentStateForTest().level, before.level) &&
                  sameSelection(window.documentStateForTest().selection, before.selection),
              "Real Thing/Frame snap/rotate Undo restores the full original state and selection");
        check(window.redoForTest() &&
                  assets::sedLevelsEqual(window.documentStateForTest().level, committed.level) &&
                  sameSelection(window.documentStateForTest().selection, committed.selection),
              "Real Thing/Frame snap/rotate Redo restores the full committed state and selection");
    }
}
} // namespace sed::app_tests
