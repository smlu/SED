#include "test_common.hpp"

#include "app_settings.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>

#include <array>
#include <filesystem>
#include <functional>
#include <memory>

namespace sed::app_tests
{
namespace
{

bool waitFor(const std::function<bool()>& predicate)
{
    QElapsedTimer deadline{};
    deadline.start();
    while ( !predicate() && deadline.elapsed() < 1000 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return predicate();
}

bool focusWidget(QWidget* target)
{
    if ( !target || !target->window() )
    {
        return false;
    }
    QWidget* const window = target->window();
    window->activateWindow();
    window->raise();
    QApplication::setActiveWindow(window);
    target->setFocus(Qt::OtherFocusReason);
    return waitFor([window, target]()
        {
            return QApplication::activeWindow() == window
                && QApplication::focusWidget() == target;
        });
}

void sendKey(QWidget* target, const int key,
             const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    if ( !target )
    {
        return;
    }
    QKeyEvent overrideEvent(QEvent::ShortcutOverride, key, modifiers);
    overrideEvent.setAccepted(false);
    QApplication::sendEvent(target, &overrideEvent);
    QKeyEvent press(QEvent::KeyPress, key, modifiers);
    press.setAccepted(false);
    QApplication::sendEvent(target, &press);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers);
    QApplication::sendEvent(target, &release);
    QApplication::processEvents();
}

class PopupInvocation final
{
public:
    PopupInvocation()
        : watchdog_(&owner_)
    {
        watchdog_.setSingleShot(true);
        QObject::connect(&watchdog_, &QTimer::timeout, &owner_, []()
            {
                if ( auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()) )
                {
                    dialog->reject();
                }
                if ( QWidget* popup = QApplication::activePopupWidget() )
                {
                    popup->close();
                }
            });
        watchdog_.start(3000);
    }

    void poll(const std::function<bool()>& callback)
    {
        auto* timer = new QTimer(&owner_);
        timer->setInterval(5);
        QObject::connect(timer, &QTimer::timeout, timer, [timer, callback]()
            {
                if ( callback() )
                {
                    timer->stop();
                }
            });
        timer->start();
    }

    QObject* owner() noexcept
    {
        return &owner_;
    }

private:
    QObject owner_{};
    QTimer watchdog_{};
};

struct ScriptFixture
{
    std::unique_ptr<MainWindow> window{std::make_unique<MainWindow>()};
    QWidget* page{};
    QPlainTextEdit* editor{};
    QAction* insert{};

    explicit ScriptFixture(const QString& name)
    {
        window->setAnimated(false);
        window->createNewCogScriptForTest(name);
        auto* tabs = window->findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
        page = tabs ? tabs->currentWidget() : nullptr;
        editor = page ? page->findChild<QPlainTextEdit*>() : nullptr;
        insert = window->findChild<QAction*>(QStringLiteral("cog_editor.insert_context"));
        window->show();
        QApplication::processEvents();
    }

    bool setSource(const QString& source, const int line)
    {
        if ( !editor )
        {
            return false;
        }
        editor->setPlainText(source);
        editor->document()->clearUndoRedoStacks();
        editor->document()->setModified(false);
        QTextBlock block = editor->document()->findBlockByNumber(line);
        if ( !block.isValid() )
        {
            return false;
        }
        editor->setTextCursor(QTextCursor(block));
        const bool focused = focusWidget(editor);
        window->updateEditActionsForTest();
        QApplication::processEvents();
        return focused;
    }
};

QAction* menuAction(QMenu* menu, const QString& text)
{
    if ( menu )
    {
        for ( QAction* action : menu->actions() )
        {
            if ( action && action->text() == text )
            {
                return action;
            }
        }
    }
    return nullptr;
}

void testActionLabelsAndUnboundCommands()
{
    ScriptFixture fixture(QStringLiteral("shortcut_action_probe.cog"));
    const std::array<std::pair<const char*, const char*>, 6> labels{{
        { "cog_editor.reload", "Reload" },
        { "cog_editor.complete", "Completion" },
        { "cog_editor.find_previous", "Find Previous" },
        { "cog_editor.format", "Format" },
        { "cog_editor.toggle_whitespace", "Toggle Whitespace" },
        { "cog_editor.goto_definition", "Go To Definition" },
    }};
    for ( const auto& [id, label] : labels )
    {
        QAction* const action = fixture.window->findChild<QAction*>(QString::fromLatin1(id));
        check(action && action->text() == QString::fromLatin1(label),
            "COG Script Editor action display text omits the repeated editor prefix while retaining its stable binding ID");
    }
    const std::array<const char*, 4> unbound{{
        "cog_editor.add_symbol", "cog_editor.add_message",
        "cog_editor.add_code_label", "cog_editor.edit_flags",
    }};
    for ( const char* id : unbound )
    {
        QAction* const action = fixture.window->findChild<QAction*>(QString::fromLatin1(id));
        check(action && action->shortcuts().isEmpty()
                && action->property("sed_default_shortcuts").toStringList().isEmpty()
                && !action->toolTip().isEmpty(),
            "existing useful COG Script Editor insertion and flags flows expose configurable commands without invented default bindings");
    }
    check(fixture.insert && fixture.insert->shortcut() == QKeySequence(Qt::Key_Insert)
            && fixture.insert->property("sed_default_shortcuts").toStringList()
                == QStringList{ QStringLiteral("Ins") },
        "COG Script Editor context insertion exposes its stable keymap action with the default Insert key");
}

void testCaretContextAndNativeInsertFallback()
{
    ScriptFixture fixture(QStringLiteral("shortcut_context_probe.cog"));
    check(fixture.editor && fixture.insert,
        "COG context Insert has a real source editor and configurable action");
    if ( !fixture.editor || !fixture.insert )
    {
        return;
    }
    struct ContextCase
    {
        QString source{};
        int line{};
        bool enabled{};
        const char* description{};
    };
    const std::array<ContextCase, 15> cases{{
        { QStringLiteral("symbols\nsound clip local # no assignment\nend\ncode\nend\n"), 1, true,
            "Insert adds a new symbol from a symbols declaration without an initializer" },
        { QStringLiteral("symbols\n# code fake: return; end\nend\ncode\nend\n"), 1, true,
            "Insert recognizes comments within symbols without interpreting their structural-looking text" },
        { QStringLiteral("# preamble\nflags=0x001\nsymbols\nend\ncode\nend\n"), 0, false,
            "Insert retains native text behavior outside a script section" },
        { QStringLiteral("flags=0x001\nsymbols\nend\ncode\nend\n"), 0, false,
            "Insert retains native text behavior on a flags declaration" },
        { QStringLiteral("symbols\nend\ncode\n\nend\n"), 3, true,
            "Insert opens message or label insertion in code before any handler" },
        { QStringLiteral("symbols\nend\ncode\nstartup:\n\nreturn;\nend\n"), 3, false,
            "Insert does not create a handler on an existing handler label" },
        { QStringLiteral("symbols\nend\ncode\nstartup:\nvalue=1;\nreturn;\nend\n"), 4, false,
            "Insert retains native text behavior inside a handler statement" },
        { QStringLiteral("symbols\nend\ncode\nstartup:\nif (value) {\nreturn;\n\n}\nreturn;\nend\n"), 6, false,
            "a nested return does not create an insertion boundary within an existing handler" },
        { QStringLiteral("symbols\nend\ncode\nstartup:\nif (value) return;\n\nvalue=2;\nreturn;\nend\n"), 5, false,
            "an inline conditional return does not end the active handler for context Insert" },
        { QStringLiteral("symbols\nend\ncode\nstartup:\nif (value)\nreturn;\n\nvalue=2;\nreturn;\nend\n"), 6, false,
            "a multiline conditional return does not end the active handler for context Insert" },
        { QStringLiteral("symbols\nend\ncode\nstartup:\nreturn; # actual final return\n\nend\n"), 5, true,
            "Insert opens handler insertion after an unconditional top-level return" },
        { QStringLiteral("symbols\nend\ncode\nstartup:\n# return;\n\nend\n"), 5, false,
            "a comment containing return does not end a handler" },
        { QStringLiteral("symbols\nend\ncode\nstartup:\nPrint(\"return; fake: {}\");\n\nend\n"), 5, false,
            "quoted return, label and brace text cannot create a handler insertion boundary" },
        { QStringLiteral("symbols\nend\ncode\nstartup:\nvalue=1;\n"), 5, false,
            "an unfinished handler stays active through its EOF blank line" },
        { QStringLiteral("symbols\nend\ncode invalid\n\nend\n"), 3, false,
            "malformed section headers retain native Insert rather than creating structure" },
    }};
    QAction* const grid = fixture.window->findChild<QAction*>(
        QStringLiteral("viewport.grid_always_behind"));
    for ( const ContextCase& item : cases )
    {
        const bool sourceReady = fixture.setSource(item.source, item.line);
        check(sourceReady && fixture.insert->isEnabled() == item.enabled, item.description);
        if ( !sourceReady || item.enabled )
        {
            continue;
        }
        const int revision = fixture.editor->document()->revision();
        const std::size_t levelUndo = fixture.window->undoDepthForTest();
        const bool gridChecked = grid && grid->isChecked();
        sendKey(fixture.editor, Qt::Key_Insert);
        check(fixture.editor->toPlainText() == item.source
                && fixture.editor->document()->revision() == revision
                && !fixture.editor->document()->isModified()
                && !fixture.editor->document()->isUndoAvailable()
                && fixture.window->undoDepthForTest() == levelUndo
                && (!grid || grid->isChecked() == gridChecked)
                && !QApplication::activePopupWidget(),
            "unsupported COG Insert context preserves source and native history and never propagates to viewport Insert");
    }
    const QString symbols = QStringLiteral("symbols\nsound clip local\nend\ncode\nend\n");
    check(fixture.setSource(symbols, 1), "context Insert focus probe restores the source editor");
    const std::array<QWidget*, 2> otherWidgets{{
        fixture.window->glViewForTest(),
        fixture.page->findChild<QListWidget*>(QStringLiteral("cogEditorSymbolsOutlineList")),
    }};
    for ( QWidget* target : otherWidgets )
    {
        check(focusWidget(target) && !fixture.insert->isEnabled(),
            "context Insert requires focused script text instead of viewport or outline focus");
    }
    QWidget uvCanvas(fixture.window.get());
    uvCanvas.setObjectName(QStringLiteral("uvEditorCanvas"));
    uvCanvas.setFocusPolicy(Qt::StrongFocus);
    uvCanvas.show();
    check(focusWidget(&uvCanvas) && !fixture.insert->isEnabled(),
        "UV canvas focus cannot enable an insertion action for the open COG source");
    sendKey(&uvCanvas, Qt::Key_Insert);
    check(fixture.editor->toPlainText() == symbols && !QApplication::activePopupWidget(),
        "an Insert delivered to the UV canvas never opens a COG insertion flow");
    QPlainTextEdit otherEditor(fixture.window.get());
    otherEditor.setPlainText(QStringLiteral("another document"));
    otherEditor.show();
    check(focusWidget(&otherEditor) && !fixture.insert->isEnabled(),
        "context Insert cannot act on an unfocused COG buffer while another text editor owns focus");
    sendKey(&otherEditor, Qt::Key_Insert);
    check(otherEditor.toPlainText() == QStringLiteral("another document")
            && fixture.editor->toPlainText() == symbols,
        "another text editor retains native Insert without changing the open COG source");
}

void testSymbolMenuAcceptCancelAndLocalUndo()
{
    ScriptFixture fixture(QStringLiteral("shortcut_symbol_probe.cog"));
    check(fixture.editor && fixture.insert, "keyboard symbol insertion has its real configurable action");
    if ( !fixture.editor || !fixture.insert )
    {
        return;
    }
    const QString source = QStringLiteral(
        "symbols\nsound clip local # no assignment\n# declaration comment\nend\ncode\nend\n");
    const std::array<QString, 2> types{{ QStringLiteral("int"), QStringLiteral("sound") }};
    const std::array<QString, 2> expected{{
        QStringLiteral("symbols\nsound clip local # no assignment\nint value=0\n# declaration comment\nend\ncode\nend\n"),
        QStringLiteral("symbols\nsound clip local # no assignment\nsound ref=-1\n# declaration comment\nend\ncode\nend\n"),
    }};
    for ( std::size_t index = 0; index < types.size(); ++index )
    {
        const bool sourceReady = fixture.setSource(source, 1);
        const std::size_t levelUndo = fixture.window->undoDepthForTest();
        bool chosen = false;
        PopupInvocation invocation{};
        invocation.poll([&]()
            {
                auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                QAction* const action = menuAction(menu, types[index]);
                if ( !menu || !action )
                {
                    return false;
                }
                chosen = true;
                action->trigger();
                menu->close();
                return true;
            });
        if ( sourceReady )
        {
            sendKey(fixture.editor, Qt::Key_Insert);
        }
        check(chosen && fixture.editor->toPlainText() == expected[index]
                && fixture.editor->document()->isModified()
                && fixture.window->undoDepthForTest() == levelUndo,
            "keyboard Insert uses the existing primitive or resource type menu and preserves the surrounding authored declaration and comment");
        sendKey(fixture.editor, Qt::Key_Z, Qt::ControlModifier);
        check(fixture.editor->toPlainText() == source
                && !fixture.editor->document()->isUndoAvailable()
                && fixture.window->undoDepthForTest() == levelUndo,
            "one configured local Undo reverses symbol insertion without consuming Level history");
    }
    check(fixture.setSource(source, 2), "symbol menu cancellation uses the comment caret context");
    const QTextCursor cursor = fixture.editor->textCursor();
    const int revision = fixture.editor->document()->revision();
    bool cancelled = false;
    PopupInvocation invocation{};
    invocation.poll([&]()
        {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if ( !menu || !menuAction(menu, QStringLiteral("int")) )
            {
                return false;
            }
            cancelled = true;
            menu->close();
            return true;
        });
    sendKey(fixture.editor, Qt::Key_Insert);
    check(cancelled && fixture.editor->toPlainText() == source
            && fixture.editor->document()->revision() == revision
            && !fixture.editor->document()->isModified()
            && !fixture.editor->document()->isUndoAvailable()
            && fixture.editor->textCursor().position() == cursor.position()
            && fixture.editor->textCursor().anchor() == cursor.anchor(),
        "cancelling keyboard Add Symbol preserves source, caret, dirty state and local history");

    check(fixture.setSource(source, 1), "stale symbol menu starts with the authored source caret");
    const QString replacement = QStringLiteral("symbols\nint external=7\nend\ncode\nend\n");
    bool staleChoiceAttempted = false;
    PopupInvocation staleInvocation{};
    staleInvocation.poll([&]()
        {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            QAction* const choice = menuAction(menu, QStringLiteral("int"));
            if ( !menu || !choice )
            {
                return false;
            }
            fixture.editor->setPlainText(replacement);
            staleChoiceAttempted = true;
            choice->trigger();
            menu->close();
            return true;
        });
    sendKey(fixture.editor, Qt::Key_Insert);
    check(staleChoiceAttempted && fixture.editor->toPlainText() == replacement
            && !fixture.editor->document()->isUndoAvailable(),
        "an insertion menu discards a stale choice after the source changes instead of editing or restoring the previous buffer");
}

void testMessageAndLabelMenus()
{
    ScriptFixture fixture(QStringLiteral("shortcut_handler_probe.cog"));
    check(fixture.editor && fixture.insert, "keyboard handler insertion has its real configurable action");
    if ( !fixture.editor || !fixture.insert )
    {
        return;
    }
    const QString source = QStringLiteral("symbols\nint prior=0\nend\ncode\n\nend\n");
    const bool sourceReady = fixture.setSource(source, 4);
    const std::size_t levelUndo = fixture.window->undoDepthForTest();
    bool chosen = false;
    PopupInvocation invocation{};
    invocation.poll([&]()
        {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            QAction* const messages = menuAction(menu, QStringLiteral("Message"));
            QAction* const startup = messages && messages->menu()
                ? menuAction(messages->menu(), QStringLiteral("startup")) : nullptr;
            if ( !menu || !startup )
            {
                return false;
            }
            chosen = true;
            startup->trigger();
            menu->close();
            return true;
        });
    if ( sourceReady )
    {
        sendKey(fixture.editor, Qt::Key_Insert);
    }
    const QString expected = QStringLiteral(
        "symbols\nint prior=0\nmessage startup\nend\ncode\nstartup:\n    return;\n\n\nend\n");
    check(chosen && fixture.editor->toPlainText() == expected
            && fixture.window->undoDepthForTest() == levelUndo,
        "code-context Insert opens the existing message flow and inserts its matching symbols declaration and handler");
    sendKey(fixture.editor, Qt::Key_Z, Qt::ControlModifier);
    check(fixture.editor->toPlainText() == source
            && !fixture.editor->document()->isUndoAvailable()
            && fixture.window->undoDepthForTest() == levelUndo,
        "message declaration and handler are one local Undo unit");

    for ( const bool accept : { false, true } )
    {
        check(fixture.setSource(source, 4), "custom label flow starts at the blank code caret");
        const int revision = fixture.editor->document()->revision();
        bool menuSeen = false;
        bool dialogSeen = false;
        PopupInvocation labelInvocation{};
        labelInvocation.poll([&]()
            {
                auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                QAction* const label = menuAction(menu, QStringLiteral("Code section..."));
                if ( !menu || !label )
                {
                    return false;
                }
                menuSeen = true;
                QTimer::singleShot(0, labelInvocation.owner(), [&]()
                    {
                        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                        QLineEdit* const name = dialog ? dialog->findChild<QLineEdit*>(
                            QStringLiteral("cogEditorCodeSectionNameEdit")) : nullptr;
                        QPushButton* const ok = dialog ? dialog->findChild<QPushButton*>(
                            QStringLiteral("cogEditorCodeSectionOkButton")) : nullptr;
                        dialogSeen = dialog && name && ok;
                        if ( !dialogSeen )
                        {
                            return;
                        }
                        if ( accept )
                        {
                            name->setText(QStringLiteral("review_label"));
                            ok->click();
                        }
                        else
                        {
                            dialog->reject();
                        }
                    });
                label->trigger();
                menu->close();
                return true;
            });
        sendKey(fixture.editor, Qt::Key_Insert);
        const QString labelExpected = QStringLiteral(
            "symbols\nint prior=0\nflex review_label=0 local\nend\ncode\nreview_label:\n    return;\n\n\nend\n");
        check(menuSeen && dialogSeen
                && fixture.editor->toPlainText() == (accept ? labelExpected : source)
                && fixture.window->undoDepthForTest() == levelUndo
                && (accept || (fixture.editor->document()->revision() == revision
                    && !fixture.editor->document()->isModified()
                    && !fixture.editor->document()->isUndoAvailable())),
            accept
                ? "keyboard Insert accepts the existing custom label dialog and adds its declaration and code handler"
                : "cancelling the existing keyboard custom label dialog creates no source, dirty-state or history change");
        if ( accept )
        {
            sendKey(fixture.editor, Qt::Key_Z, Qt::ControlModifier);
            check(fixture.editor->toPlainText() == source
                    && !fixture.editor->document()->isUndoAvailable()
                    && fixture.window->undoDepthForTest() == levelUndo,
                "custom label declaration and handler are one local Undo unit");
        }
    }
}

void testRemappedInsertUsesTheCaret()
{
    ScriptFixture fixture(QStringLiteral("shortcut_remap_probe.cog"));
    check(fixture.editor && fixture.insert, "remapped context Insert has its real keymap action");
    if ( !fixture.editor || !fixture.insert )
    {
        return;
    }
    const QString source = QStringLiteral("symbols\nint prior=0\nend\ncode\n\nend\n");
    check(fixture.setSource(source, 1), "remapped context Insert owns the script caret");
    const QPoint previousPointer = QCursor::pos();
    struct RestorePointer
    {
        QPoint position{};
        ~RestorePointer() { QCursor::setPos(position); }
    } restorePointer{ previousPointer };
    const QTextCursor codeCursor(fixture.editor->document()->findBlockByNumber(4));
    const QPoint codePosition = fixture.editor->cursorRect(codeCursor).center();
    const QPoint codeGlobal = fixture.editor->viewport()->mapToGlobal(codePosition);
    QCursor::setPos(codeGlobal);
    QMouseEvent hover(QEvent::MouseMove, QPointF(codePosition), QPointF(codeGlobal),
        QPointF(codeGlobal), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(fixture.editor->viewport(), &hover);
    fixture.insert->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+I")));
    bool popupSeen = false;
    PopupInvocation invocation{};
    invocation.poll([&]()
        {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if ( !menu || !menuAction(menu, QStringLiteral("int")) )
            {
                return false;
            }
            popupSeen = true;
            menu->close();
            return true;
        });
    sendKey(fixture.editor, Qt::Key_I, Qt::ControlModifier | Qt::AltModifier);
    check(popupSeen && fixture.editor->toPlainText() == source
            && !fixture.editor->document()->isUndoAvailable(),
        "context insertion resolves the live configured QAction binding at the Symbols caret while the pointer hovers over Code");
    sendKey(fixture.editor, Qt::Key_Insert);
    check(!QApplication::activePopupWidget() && fixture.editor->toPlainText() == source,
        "the former Insert binding retains native script behavior after context insertion is remapped");

    const QString handler = QStringLiteral("symbols\nend\ncode\nstartup:\nvalue=1;\n\nend\n");
    check(fixture.setSource(handler, 5), "disabled remapped Insert remains inside the active handler");
    fixture.insert->setShortcut(QKeySequence(Qt::Key_F14));
    QAction* const grid = fixture.window->findChild<QAction*>(
        QStringLiteral("viewport.grid_always_behind"));
    const bool gridChecked = grid && grid->isChecked();
    QKeyEvent disabledOverride(QEvent::ShortcutOverride, Qt::Key_F14, Qt::NoModifier);
    disabledOverride.setAccepted(false);
    const bool claimed = fixture.window->eventFilterForTest(fixture.editor, &disabledOverride);
    sendKey(fixture.editor, Qt::Key_F14);
    check(!fixture.insert->isEnabled() && claimed && disabledOverride.isAccepted()
            && fixture.editor->toPlainText() == handler
            && (!grid || grid->isChecked() == gridChecked)
            && !QApplication::activePopupWidget(),
        "a disabled caret-context Insert still protects its configured function-key binding while leaving the source's native behavior intact");
}

void testEofInsertionSeparatesTheConstructAndUndo()
{
    ScriptFixture fixture(QStringLiteral("shortcut_eof_probe.cog"));
    check(fixture.editor && fixture.insert, "EOF insertion exercises the real focused script action");
    if ( !fixture.editor || !fixture.insert )
    {
        return;
    }
    struct EofCase
    {
        QString source{};
        QString expected{};
        int line{};
        bool symbol{};
    };
    const std::array<EofCase, 2> cases{{
        { QStringLiteral("symbols\nint existing=1"),
          QStringLiteral("symbols\nint existing=1\nint value=0\n"), 1, true },
        { QStringLiteral("symbols\nint existing=1\nend\ncode"),
          QStringLiteral("symbols\nint existing=1\nmessage startup\nend\ncode\nstartup:\n    return;\n\n"),
          3, false },
    }};
    for ( const EofCase& item : cases )
    {
        const bool sourceReady = fixture.setSource(item.source, item.line);
        const std::size_t levelUndo = fixture.window->undoDepthForTest();
        bool chosen = false;
        PopupInvocation invocation{};
        invocation.poll([&]()
            {
                auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                QAction* choice = menuAction(menu, QStringLiteral("int"));
                if ( !item.symbol )
                {
                    QAction* const messages = menuAction(menu, QStringLiteral("Message"));
                    choice = messages && messages->menu()
                        ? menuAction(messages->menu(), QStringLiteral("startup")) : nullptr;
                }
                if ( !menu || !choice )
                {
                    return false;
                }
                chosen = true;
                choice->trigger();
                menu->close();
                return true;
            });
        if ( sourceReady )
        {
            sendKey(fixture.editor, Qt::Key_Insert);
        }
        check(sourceReady && chosen && fixture.editor->toPlainText() == item.expected
                && fixture.window->undoDepthForTest() == levelUndo,
            item.symbol
                ? "Insert at an unclosed Symbols EOF separates the new declaration from the existing token without rewriting it"
                : "Insert at an empty Code EOF separates the new handler from the section marker and creates its message declaration");
        sendKey(fixture.editor, Qt::Key_Z, Qt::ControlModifier);
        check(fixture.editor->toPlainText() == item.source
                && !fixture.editor->document()->isUndoAvailable()
                && fixture.window->undoDepthForTest() == levelUndo,
            "the required EOF newline and complete inserted construct share one local Undo step with exact source restoration");
    }
}

} // namespace

void runCogShortcutContextTests()
{
    QTemporaryDir temporary{};
    check(temporary.isValid(), "COG shortcut context tests own an isolated settings directory");
    if ( !temporary.isValid() )
    {
        return;
    }
    const std::filesystem::path previous = appSettingsIniPath();
    struct RestoreSettings
    {
        std::filesystem::path path{};
        ~RestoreSettings() { setAppSettingsIniPathOverrideForTest(path); }
    } restore{ previous };
    const std::filesystem::path settingsPath = std::filesystem::path(temporary.path().toStdWString())
        / "cog-shortcut-context.ini";
    setAppSettingsIniPathOverrideForTest(settingsPath);
    QSettings settings = settingsStore();
    settings.clear();
    settings.setValue(QStringLiteral("app/last_project_kind"), QStringLiteral("jkdf2"));
    settings.setValue(QStringLiteral("resources/roots"), QStringList{});
    settings.setValue(QStringLiteral("resources/gobs"), QStringList{});
    settings.setValue(QStringLiteral("cog_editor/format_indent_sections"), false);
    settings.setValue(QStringLiteral("cog_editor/format_indent_code_bodies"), true);
    settings.setValue(QStringLiteral("cog_editor/format_use_tabs"), false);
    settings.setValue(QStringLiteral("cog_editor/format_indent_width"), 4);
    settings.sync();
    testActionLabelsAndUnboundCommands();
    testCaretContextAndNativeInsertFallback();
    testSymbolMenuAcceptCancelAndLocalUndo();
    testMessageAndLabelMenus();
    testRemappedInsertUsesTheCaret();
    testEofInsertionSeparatesTheConstructAndUndo();
}

} // namespace sed::app_tests
