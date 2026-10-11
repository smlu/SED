#include "test_common.hpp"
#include "main_window/main_window.hpp"
#include "gl_view.hpp"
#include "sed/assets/sed.hpp"

#include <QAction>
#include <QApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#include <limits>

namespace sed::app_tests
{
void runTranslateMenuContractTests()
{
    MainWindow window;
    window.setAttribute(Qt::WA_DontShowOnScreen);
    auto* action = window.findChild<QAction*>(QStringLiteral("transform.translate"));
    check(action != nullptr, "Translate menu contract exposes the actual action");
    if ( !action )
    {
        return;
    }
    const auto sameSelection = [](const editor::Selection& a, const editor::Selection& b)
    {
        return a.sectors == b.sectors && a.surfaces == b.surfaces && a.surfaceVertices == b.surfaceVertices
            && a.things == b.things && a.frames == b.frames && a.lights == b.lights && a.cogs == b.cogs;
    };
    for ( int mode = 0; mode < 4; ++mode )
    {
        editor::DocumentState state;
        state.level.layers = {"Layer0"};
        Thing owner;
        owner.name = "__translate_contract_missing_template__";
        owner.x = mode == 1 ? std::numeric_limits<double>::max() : 1;
        owner.y = 2;
        owner.z = 3;
        owner.sector = -1;
        TplValue frame;
        frame.name = "frame";
        frame.type = ValueType::frame;
        frame.stringValue = mode == 1 ? " (1.7976931348623157e+308/2/3:0/0/0) " : " (1/2/3:0/0/0) ";
        owner.values.push_back(frame);
        state.level.things.push_back(owner);
        state.selection.things.insert(0);
        window.setDocumentStateForTest(state);
        window.activateThingEditModeForTest();
        window.glViewForTest()->setSelectedThing(0);
        // Entering Thing mode can clear selection. Establish the authored
        // selection after the viewport's active-item callbacks have completed.
        window.mutableDocumentStateForTest().selection.things = {0};
        window.updateEditActionsForTest();
        window.pushUndoForTitleForTest(editor::DocumentDirty::Things);
        window.mutableDocumentStateForTest().level.things[0].z = 9;
        check(window.undoForTest(), "Translate menu fixture seeds a retained Redo row");
        window.updateEditActionsForTest();
        check(sameSelection(window.documentStateForTest().selection, state.selection)
                && action->isEnabled(),
            "Translate menu fixture restores its exact selected Thing and enabled action after Undo seed");
        const auto before = window.documentStateForTest();
        const auto undoDepth = window.undoDepthForTest();
        const auto redoDepth = window.redoDepthForTest();
        int prompt = 0;
        int messageCount = 0;
        QString warning;
        QTimer respond;
        respond.setInterval(5);
        QObject::connect(&respond, &QTimer::timeout,[&]
        {
            if ( auto* input = qobject_cast<QInputDialog*>(QApplication::activeModalWidget()) )
            {
                const int component = prompt++;
                if ( mode == 3 || component > 2 )
                {
                    input->reject();
                    return;
                }
                input->setDoubleValue(component == 0 ? (mode == 0 ? 0 : mode == 1 ? 1 : 0.125) : 0);
                input->accept();
            }
            else if ( auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) )
            {
                ++messageCount;
                warning = message->text();
                message->accept();
            }
        });
        check(sameSelection(window.documentStateForTest().selection, state.selection)
                && action->isEnabled(),
            "Translate menu fixture retains exact selection and enabled action immediately before trigger");
        respond.start();
        action->trigger();
        respond.stop();
        const int expectedPrompts = mode == 3 ? 1 : 3;
        const int expectedMessages = mode == 1 ? 1 : 0;
        const QString expectedWarning = mode == 1
            ? QStringLiteral("The selected Things have no finite representable translation.") : QString{};
        if ( prompt != expectedPrompts || messageCount != expectedMessages || warning != expectedWarning )
        {
            std::cerr << "TranslateMenu mode=" << mode << " prompts=" << prompt
                << " expectedPrompts=" << expectedPrompts << " messages=" << messageCount
                << " expectedMessages=" << expectedMessages << " warning=" << warning.toStdString()
                << " actionEnabled=" << action->isEnabled()
                << " selectedThings=" << before.selection.things.size() << '\n';
        }
        check(prompt == expectedPrompts, "Translate menu exercises each real numeric prompt or cancellation");
        check(messageCount == expectedMessages && warning == expectedWarning,
            "Translate menu returns exactly the fixed warning vector or no message for success zero and cancel");
        if ( mode != 2 )
        {
            check(assets::sedLevelsEqual(window.documentStateForTest().level, before.level)
                    && sameSelection(window.documentStateForTest().selection, before.selection)
                    && window.undoDepthForTest() == undoDepth && window.redoDepthForTest() == redoDepth,
                "Translate menu zero rounded-away and canceled edits preserve complete Level selection and both history stacks");
        }
        else
        {
            const auto committed = window.documentStateForTest();
            const Thing& moved = committed.level.things[0];
            check(moved.x == 1.125 && moved.y == 2 && moved.z == 3 && moved.sector == -1
                    && moved.pch == 0 && moved.yaw == 0 && moved.rol == 0
                    && moved.values[0].asString() == "(1.125/2/3:0/0/0)"
                    && sameSelection(committed.selection, editor::Selection{}) && window.undoDepthForTest() == undoDepth + 1
                    && window.redoDepthForTest() == 0,
                "Translate menu commits exact owner and fixed complete Frame token in one history row");
            check(window.undoForTest() && assets::sedLevelsEqual(window.documentStateForTest().level, before.level)
                    && sameSelection(window.documentStateForTest().selection, before.selection),
                "Translate menu Undo restores full original Level token and selection");
            check(window.redoForTest() && assets::sedLevelsEqual(window.documentStateForTest().level, committed.level)
                    && sameSelection(window.documentStateForTest().selection, committed.selection),
                "Translate menu Redo restores full committed Level and selection");
        }
    }
}
}
