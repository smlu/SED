#include "test_common.hpp"

#include "ui/cog/cog_script_assignment_utils.hpp"
#include "ui/cog/cog_script_diagnostics_utils.hpp"
#include "ui/cog/cog_script_editor_widget.hpp"
#include "ui/cog/cog_script_language_utils.hpp"
#include "ui/cog/cog_script_theme_utils.hpp"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QFont>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPlainTextDocumentLayout>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>

namespace sed::app_tests
{
namespace
{

void moveMouseToEditorColumn(
    sed::ui::cog::CogScriptCodeEditor& editor,
    const int line,
    const int column)
{
    QTextBlock block = editor.document()->findBlockByNumber(line);
    if ( !block.isValid() )
    {
        return;
    }
    QTextCursor cursor(block);
    cursor.setPosition(block.position() + std::clamp(
        column, 0, static_cast<int>(block.text().size())));
    const QRect cursorRect = editor.cursorRect(cursor);
    const QPoint local(cursorRect.left() + 2, cursorRect.center().y());
    const QPoint global = editor.viewport()->mapToGlobal(local);
    QMouseEvent move(
        QEvent::MouseMove,
        QPointF(local),
        QPointF(global),
        QPointF(global),
        Qt::NoButton,
        Qt::NoButton,
        Qt::NoModifier);
    QApplication::sendEvent(editor.viewport(), &move);
    QApplication::processEvents();
}

QToolButton* inlineAssignmentButton(sed::ui::cog::CogScriptCodeEditor& editor)
{
    return editor.findChild<QToolButton*>(
        QStringLiteral("cogInlineAssignmentPickerButton"));
}

void configureEditor(sed::ui::cog::CogScriptCodeEditor& editor, const QString& text)
{
    editor.resize(760, 260);
    editor.setPlainText(text);
    editor.document()->setModified(false);
    editor.applyTheme(defaultCogScriptTheme());
    new sed::ui::cog::CogScriptSyntaxHighlighter(
        editor.document(),
        sed::ui::cog::cogScriptProjectMessageWords(sed::ProjectType::JKDF2),
        defaultCogScriptTheme());
    editor.show();
    QApplication::processEvents();
}

void positionForInlinePicker(sed::ui::cog::CogScriptCodeEditor& editor,
                             const QString& source)
{
    editor.resize(760, 100);
    QApplication::processEvents();
    const int valueStart = source.indexOf(QStringLiteral("old.wav"));
    QTextCursor selected(editor.document());
    selected.setPosition(valueStart);
    selected.setPosition(valueStart + 2, QTextCursor::KeepAnchor);
    editor.setTextCursor(selected);
    editor.verticalScrollBar()->setValue(1);
    editor.horizontalScrollBar()->setValue(5);
    QApplication::processEvents();
}

} // namespace

void runCogInlineAssignmentTests()
{
    const QString source = QStringLiteral(
        "flags=0x001 # flag comment\n"
        "symbols\n"
        "sound clip=old.wav local # keep this suffix\n"
        "thing target=level\n"
        "end\n"
        "code\n"
        "sound codeValue=old.wav;\n"
        "end\n");

    sed::ui::cog::CogScriptCodeEditor editor;
    configureEditor(editor, source);
    QString requestedCurrentValue{};
    editor.setInlineAssignmentCallbacks(
        [&requestedCurrentValue](const sed::ui::ResourcePickerKind kind,
                                 const QString& currentValue)
        {
            requestedCurrentValue = currentValue;
            return kind == sed::ui::ResourcePickerKind::wav
                ? std::optional<QString>{QStringLiteral("picked.wav")}
                : std::nullopt;
        },
        []() {});

    const QString symbolLine = QStringLiteral("sound clip=old.wav local # keep this suffix");
    QString longSource = source;
    for ( int line = 0; line < 24; ++line )
    {
        longSource += QStringLiteral("outside\n");
    }
    longSource += QString(300, QLatin1Char('x')) + QLatin1Char('\n');
    moveMouseToEditorColumn(
        editor, 2, symbolLine.indexOf(QStringLiteral("old.wav")));
    QToolButton* resourceButton = inlineAssignmentButton(editor);
    const bool resourceHelperShown = resourceButton && resourceButton->isVisible();
    moveMouseToEditorColumn(
        editor, 2, symbolLine.indexOf(QStringLiteral("clip")));
    const bool resourceHelperSurvivesSymbolHover = resourceButton
        && resourceButton->isVisible();
    moveMouseToEditorColumn(editor, 2, 0);
    const bool resourceHelperSurvivesLineStart = resourceButton
        && resourceButton->isVisible();
    QString expectedAccepted = source;
    expectedAccepted.replace(QStringLiteral("clip=old.wav"), QStringLiteral("clip=picked.wav"));
    bool acceptedResourceEdit = false;
    if ( resourceHelperShown && resourceHelperSurvivesSymbolHover
        && resourceHelperSurvivesLineStart )
    {
        QEvent gutterEnter(QEvent::Enter);
        QApplication::sendEvent(resourceButton->parentWidget(), &gutterEnter);
        QEvent buttonEnter(QEvent::Enter);
        QApplication::sendEvent(resourceButton, &buttonEnter);
        resourceButton->click();
        acceptedResourceEdit = editor.toPlainText() == expectedAccepted
            && requestedCurrentValue == QStringLiteral("old.wav");
    }
    check(resourceHelperShown && resourceHelperSurvivesSymbolHover
            && resourceHelperSurvivesLineStart && acceptedResourceEdit,
        "cog_inline_assignment_ui keeps the helper armed while the pointer crosses the symbol row to the gutter and accepts one replacement");
    check(editor.document()->isModified()
            && editor.document()->isUndoAvailable()
            && requestedCurrentValue == QStringLiteral("old.wav"),
        "cog_inline_assignment_ui marks the accepted resource edit dirty and creates undo history");
    if ( acceptedResourceEdit )
    {
        editor.undo();
    }
    check(editor.toPlainText() == source,
        "cog_inline_assignment_ui accepts a resource replacement as one undo step");

    sed::ui::cog::CogScriptCodeEditor cancelEditor;
    configureEditor(cancelEditor, longSource);
    int cancelPickerCalls = 0;
    cancelEditor.setInlineAssignmentCallbacks(
        [&cancelEditor, &cancelPickerCalls](sed::ui::ResourcePickerKind,
                                            const QString&) -> std::optional<QString>
        {
            ++cancelPickerCalls;
            QTextCursor changed(cancelEditor.document());
            changed.setPosition(0);
            cancelEditor.setTextCursor(changed);
            cancelEditor.verticalScrollBar()->setValue(0);
            cancelEditor.horizontalScrollBar()->setValue(0);
            return std::nullopt;
        },
        []() {});
    moveMouseToEditorColumn(
        cancelEditor, 2, symbolLine.indexOf(QStringLiteral("old.wav")));
    positionForInlinePicker(cancelEditor, longSource);
    const QTextCursor cancelExpectedCursor = cancelEditor.textCursor();
    const int cancelExpectedVerticalScroll = cancelEditor.verticalScrollBar()->value();
    const int cancelExpectedHorizontalScroll = cancelEditor.horizontalScrollBar()->value();
    QToolButton* cancelButton = inlineAssignmentButton(cancelEditor);
    const bool cancelHelperShown = cancelButton && cancelButton->isVisible();
    if ( cancelHelperShown )
    {
        cancelButton->click();
    }
    check(cancelHelperShown && cancelPickerCalls == 1
            && cancelExpectedVerticalScroll > 0
            && cancelExpectedHorizontalScroll > 0
            && cancelEditor.toPlainText() == longSource
            && !cancelEditor.document()->isModified()
            && !cancelEditor.document()->isUndoAvailable()
            && cancelEditor.textCursor().position() == cancelExpectedCursor.position()
            && cancelEditor.textCursor().anchor() == cancelExpectedCursor.anchor()
            && cancelEditor.verticalScrollBar()->value() == cancelExpectedVerticalScroll
            && cancelEditor.horizontalScrollBar()->value() == cancelExpectedHorizontalScroll,
        "cog_inline_assignment_ui cancel preserves text, dirty and undo state, cursor selection and scroll position");

    sed::ui::cog::CogScriptCodeEditor sameValueEditor;
    configureEditor(sameValueEditor, longSource);
    int sameValuePickerCalls = 0;
    sameValueEditor.setInlineAssignmentCallbacks(
        [&sameValueEditor, &sameValuePickerCalls](sed::ui::ResourcePickerKind,
                                                  const QString& currentValue)
            -> std::optional<QString>
        {
            ++sameValuePickerCalls;
            QTextCursor changed(sameValueEditor.document());
            changed.setPosition(0);
            sameValueEditor.setTextCursor(changed);
            sameValueEditor.verticalScrollBar()->setValue(0);
            sameValueEditor.horizontalScrollBar()->setValue(0);
            return currentValue;
        },
        []() {});
    moveMouseToEditorColumn(
        sameValueEditor, 2, symbolLine.indexOf(QStringLiteral("old.wav")));
    positionForInlinePicker(sameValueEditor, longSource);
    const QTextCursor sameExpectedCursor = sameValueEditor.textCursor();
    const int sameExpectedVerticalScroll = sameValueEditor.verticalScrollBar()->value();
    const int sameExpectedHorizontalScroll = sameValueEditor.horizontalScrollBar()->value();
    QToolButton* sameValueButton = inlineAssignmentButton(sameValueEditor);
    const bool sameValueHelperShown = sameValueButton && sameValueButton->isVisible();
    if ( sameValueHelperShown )
    {
        sameValueButton->click();
    }
    check(sameValueHelperShown && sameValuePickerCalls == 1
            && sameExpectedVerticalScroll > 0
            && sameExpectedHorizontalScroll > 0
            && sameValueEditor.toPlainText() == longSource
            && !sameValueEditor.document()->isModified()
            && !sameValueEditor.document()->isUndoAvailable()
            && sameValueEditor.textCursor().position() == sameExpectedCursor.position()
            && sameValueEditor.textCursor().anchor() == sameExpectedCursor.anchor()
            && sameValueEditor.verticalScrollBar()->value() == sameExpectedVerticalScroll
            && sameValueEditor.horizontalScrollBar()->value() == sameExpectedHorizontalScroll,
        "cog_inline_assignment_ui selecting the current resource preserves clean state, cursor selection and scroll without a no-op undo");

    int flagsEditorCalls = 0;
    sed::ui::cog::CogScriptCodeEditor flagsEditor;
    configureEditor(flagsEditor, source);
    flagsEditor.setInlineAssignmentCallbacks(
        [](sed::ui::ResourcePickerKind, const QString&) -> std::optional<QString>
        {
            return std::nullopt;
        },
        [&flagsEditorCalls]()
        {
            ++flagsEditorCalls;
        });
    moveMouseToEditorColumn(flagsEditor, 0, 6);
    if ( QToolButton* button = inlineAssignmentButton(flagsEditor);
        button && button->isVisible() )
    {
        button->click();
    }
    check(flagsEditorCalls == 1
            && flagsEditor.toPlainText() == source
            && !flagsEditor.document()->isModified(),
        "cog_inline_assignment_ui routes the flags value to the existing flags editor callback");

    sed::ui::cog::CogScriptCodeEditor conflictEditor;
    configureEditor(conflictEditor, source);
    const QString revisedSource = QStringLiteral(
        "symbols\n"
        "sound clip=changed.wav local\n"
        "end\n");
    conflictEditor.setInlineAssignmentCallbacks(
        [&conflictEditor](sed::ui::ResourcePickerKind,
                          const QString&) -> std::optional<QString>
        {
            conflictEditor.setPlainText(QStringLiteral(
                "symbols\n"
                "sound clip=changed.wav local\n"
                "end\n"));
            return QStringLiteral("stale.wav");
        },
        []() {});
    moveMouseToEditorColumn(
        conflictEditor, 2, symbolLine.indexOf(QStringLiteral("old.wav")));
    if ( QToolButton* button = inlineAssignmentButton(conflictEditor);
        button && button->isVisible() )
    {
        button->click();
    }
    check(conflictEditor.toPlainText() == revisedSource
            && !conflictEditor.toPlainText().contains(QStringLiteral("stale.wav")),
        "cog_inline_assignment_ui discards a picker result when the source revision changes in the modal callback");

    QTextDocument replacementDocument{};
    replacementDocument.setPlainText(QStringLiteral("replacement document"));
    replacementDocument.setDocumentLayout(
        new QPlainTextDocumentLayout(&replacementDocument));
    sed::ui::cog::CogScriptCodeEditor replacedDocumentEditor;
    configureEditor(replacedDocumentEditor, source);
    replacedDocumentEditor.setInlineAssignmentCallbacks(
        [&replacedDocumentEditor, &replacementDocument](
            sed::ui::ResourcePickerKind,
            const QString&) -> std::optional<QString>
        {
            replacedDocumentEditor.setDocument(&replacementDocument);
            return QStringLiteral("stale.wav");
        },
        []() {});
    moveMouseToEditorColumn(
        replacedDocumentEditor, 2,
        symbolLine.indexOf(QStringLiteral("old.wav")));
    if ( QToolButton* button = inlineAssignmentButton(replacedDocumentEditor);
        button && button->isVisible() )
    {
        button->click();
    }
    check(replacedDocumentEditor.document() == &replacementDocument
            && replacedDocumentEditor.toPlainText()
                == QStringLiteral("replacement document"),
        "cog_inline_assignment_ui abandons the picker result when its document is replaced during the callback");

    sed::ui::cog::CogScriptCodeEditor readOnlyEditor;
    configureEditor(readOnlyEditor, source);
    int readOnlyPickerCalls = 0;
    readOnlyEditor.setInlineAssignmentCallbacks(
        [&readOnlyEditor, &readOnlyPickerCalls](
            sed::ui::ResourcePickerKind,
            const QString&) -> std::optional<QString>
        {
            ++readOnlyPickerCalls;
            readOnlyEditor.setReadOnly(true);
            return QStringLiteral("stale.wav");
        },
        []() {});
    moveMouseToEditorColumn(
        readOnlyEditor, 2, symbolLine.indexOf(QStringLiteral("old.wav")));
    QToolButton* readOnlyButton = inlineAssignmentButton(readOnlyEditor);
    const bool readOnlyHelperShown = readOnlyButton && readOnlyButton->isVisible();
    if ( readOnlyHelperShown )
    {
        readOnlyButton->click();
    }
    check(readOnlyHelperShown && readOnlyPickerCalls == 1
            && readOnlyEditor.isReadOnly()
            && readOnlyEditor.toPlainText() == source
            && !readOnlyEditor.document()->isModified()
            && !readOnlyEditor.document()->isUndoAvailable(),
        "cog_inline_assignment_ui drops a picker result if the editor becomes read-only while the picker is open");

    sed::ui::cog::CogScriptCodeEditor unsupportedEditor;
    configureEditor(unsupportedEditor, source);
    unsupportedEditor.setInlineAssignmentCallbacks(
        [](sed::ui::ResourcePickerKind, const QString&) -> std::optional<QString>
        {
            return QStringLiteral("should-not-run.wav");
        },
        []() {});
    moveMouseToEditorColumn(
        unsupportedEditor, 3,
        QStringLiteral("thing target=level").indexOf(QStringLiteral("level")));
    QToolButton* unsupportedButton = inlineAssignmentButton(unsupportedEditor);
    const bool hiddenForUnsupported = !unsupportedButton || !unsupportedButton->isVisible();
    moveMouseToEditorColumn(
        unsupportedEditor, 6,
        QStringLiteral("sound codeValue=old.wav;").indexOf(QStringLiteral("old.wav")));
    const bool hiddenForCode = !unsupportedButton || !unsupportedButton->isVisible();
    check(hiddenForUnsupported && hiddenForCode,
        "cog_inline_assignment_ui hides the helper for unsupported symbol types and code assignments");

    // The button belongs in the editor viewport beside the initializer. Its
    // anchor must follow the text after the font and scroll position change.
    const QString positionedLine = QString(24, QLatin1Char(' '))
        + QStringLiteral(
            "sound ambience=old.wav local desc=keep_this_long_authored_suffix # retain");
    QString positionedSource{};
    for ( int line = 0; line < 16; ++line )
    {
        positionedSource += QStringLiteral("# vertical filler\n");
    }
    positionedSource += QStringLiteral("symbols\n%1\nend\n").arg(positionedLine);
    for ( int line = 0; line < 12; ++line )
    {
        positionedSource += QStringLiteral("# trailing filler\n");
    }
    sed::ui::cog::CogScriptCodeEditor positionedEditor;
    configureEditor(positionedEditor, positionedSource);
    positionedEditor.setInlineAssignmentCallbacks(
        [](const sed::ui::ResourcePickerKind, const QString&)
        {
            return std::optional<QString>{};
        },
        []() {});
    positionedEditor.resize(360, 120);
    QFont zoomedFont = positionedEditor.font();
    zoomedFont.setPointSize(std::max(10, zoomedFont.pointSize() + 5));
    positionedEditor.setFont(zoomedFont);
    QApplication::processEvents();
    constexpr int positionedLineNumber = 17;
    const QTextBlock positionedBlock =
        positionedEditor.document()->findBlockByNumber(positionedLineNumber);
    QTextCursor positionedLineStart(positionedBlock);
    const int valueColumn = positionedLine.indexOf(QStringLiteral("old.wav"));
    QTextCursor positionedValueStart(positionedBlock);
    positionedValueStart.setPosition(
        positionedBlock.position() + valueColumn);
    QTextCursor positionedValueEnd(positionedBlock);
    positionedValueEnd.setPosition(
        positionedBlock.position()
        + valueColumn
        + QStringLiteral("old.wav").size());

    int visibleVerticalOffset = -1;
    for ( int offset = 1;
        offset <= positionedEditor.verticalScrollBar()->maximum();
        ++offset )
    {
        positionedEditor.verticalScrollBar()->setValue(offset);
        QApplication::processEvents();
        const QRect lineRect = positionedEditor.cursorRect(positionedLineStart);
        if ( lineRect.top() >= 0
            && lineRect.bottom() < positionedEditor.viewport()->height() )
        {
            visibleVerticalOffset = offset;
            break;
        }
    }

    int visibleHorizontalOffset = -1;
    for ( int offset = 1;
        offset <= positionedEditor.horizontalScrollBar()->maximum();
        ++offset )
    {
        positionedEditor.horizontalScrollBar()->setValue(offset);
        QApplication::processEvents();
        const QRect valueStartRect =
            positionedEditor.cursorRect(positionedValueStart);
        const QRect valueEndRect =
            positionedEditor.cursorRect(positionedValueEnd);
        if ( valueStartRect.left() >= 0
            && valueEndRect.right() + 2 + 20
                <= positionedEditor.viewport()->width() )
        {
            visibleHorizontalOffset = offset;
            break;
        }
    }
    QApplication::processEvents();
    moveMouseToEditorColumn(
        positionedEditor,
        positionedLineNumber,
        valueColumn);
    QToolButton* positionedButton = inlineAssignmentButton(positionedEditor);
    const bool positionedButtonWasVisible = positionedButton
        && positionedButton->isVisible();
    QRect buttonViewportRect{};
    QRect initializerEndRect{};
    const QRect valueEndRectBeforeHover =
        positionedEditor.cursorRect(positionedValueEnd);
    if ( positionedButtonWasVisible )
    {
        buttonViewportRect = QRect(
            positionedButton->mapTo(positionedEditor.viewport(), QPoint(0, 0)),
            positionedButton->size());
        QTextCursor afterInitializer(positionedEditor.document()->findBlockByNumber(
            positionedLineNumber));
        afterInitializer.setPosition(
            afterInitializer.block().position()
            + positionedLine.indexOf(QStringLiteral("old.wav"))
            + QStringLiteral("old.wav").size());
        initializerEndRect = positionedEditor.cursorRect(afterInitializer);
    }
    check(positionedButtonWasVisible,
        "cog_inline_assignment_ui keeps the initializer affordance visible after scrolling and zooming");
    check(positionedButtonWasVisible
            && positionedButton->parentWidget() == positionedEditor.viewport(),
        "cog_inline_assignment_ui parents the initializer affordance to the editor viewport");
    check(positionedButtonWasVisible
            && buttonViewportRect.left() >= initializerEndRect.left()
            && buttonViewportRect.left() <= initializerEndRect.left() + 20
            && std::abs(buttonViewportRect.center().y()
                - initializerEndRect.center().y()) <= initializerEndRect.height(),
        "cog_inline_assignment_ui anchors the picker button in the scrolled and zoomed viewport beside its initializer");
    check(visibleHorizontalOffset > 0
            && positionedEditor.horizontalScrollBar()->value()
                == visibleHorizontalOffset
            && visibleVerticalOffset > 0
            && positionedEditor.verticalScrollBar()->value()
                == visibleVerticalOffset,
        "cog_inline_assignment_ui geometry fixture uses nonzero horizontal and vertical scroll offsets");
    if ( !positionedButtonWasVisible
        || visibleHorizontalOffset <= 0
        || visibleVerticalOffset < 0 )
    {
        std::cerr << "COG inline geometry diagnostic: visible="
                  << positionedButtonWasVisible
                  << " button=" << buttonViewportRect.x() << ','
                  << buttonViewportRect.y() << ' '
                  << buttonViewportRect.width() << 'x'
                  << buttonViewportRect.height()
                  << " valueEnd=" << valueEndRectBeforeHover.x() << ','
                  << valueEndRectBeforeHover.y() << ' '
                  << valueEndRectBeforeHover.width() << 'x'
                  << valueEndRectBeforeHover.height()
                  << " viewport=" << positionedEditor.viewport()->width()
                  << 'x' << positionedEditor.viewport()->height()
                  << " fontPixels=" << positionedEditor.fontMetrics().height()
                  << " horizontal="
                  << positionedEditor.horizontalScrollBar()->value() << '/'
                  << positionedEditor.horizontalScrollBar()->maximum()
                  << " horizontalFitOffset=" << visibleHorizontalOffset
                  << " vertical="
                  << positionedEditor.verticalScrollBar()->value() << '/'
                  << positionedEditor.verticalScrollBar()->maximum()
                  << " verticalVisibleOffset=" << visibleVerticalOffset
                  << "\n";
    }
    if ( positionedButtonWasVisible )
    {
        positionedEditor.horizontalScrollBar()->setValue(
            positionedEditor.horizontalScrollBar()->maximum());
        QApplication::processEvents();
    }
    const bool hidesWhenInitializerLeavesViewport = positionedButton
        && !positionedButton->isVisible();
    positionedEditor.horizontalScrollBar()->setValue(visibleHorizontalOffset);
    QApplication::processEvents();
    const bool restoresVisibleAnchor = positionedButton
        && positionedButton->isVisible()
        && positionedButton->parentWidget() == positionedEditor.viewport();
    check(hidesWhenInitializerLeavesViewport && restoresVisibleAnchor,
        "cog_inline_assignment_ui hides an offscreen initializer affordance and restores it when horizontal scrolling brings the anchor back");

    const auto quotedButtonFollowsClosingQuote = [](
        const QChar quote) -> bool
    {
        const QString value = QStringLiteral("cue.wav");
        const QString line = QStringLiteral("sound cue=")
            + quote + value + quote + QStringLiteral(" local");
        const QString quotedSource = QStringLiteral("symbols\n%1\nend\n")
            .arg(line);
        sed::ui::cog::CogScriptCodeEditor quotedEditor;
        configureEditor(quotedEditor, quotedSource);
        quotedEditor.setInlineAssignmentCallbacks(
            [](const sed::ui::ResourcePickerKind, const QString&)
            {
                return std::optional<QString>{};
            },
            []() {});
        const int valueColumn = line.indexOf(value);
        moveMouseToEditorColumn(quotedEditor, 1, valueColumn + 2);
        QToolButton* const button = inlineAssignmentButton(quotedEditor);
        if ( !button || !button->isVisible() )
        {
            return false;
        }

        const int closingQuoteAfter = line.indexOf(value) + value.size() + 1;
        QTextCursor afterClosingQuote(
            quotedEditor.document()->findBlockByNumber(1));
        afterClosingQuote.setPosition(
            afterClosingQuote.block().position() + closingQuoteAfter);
        const QRect closingQuoteEndRect =
            quotedEditor.cursorRect(afterClosingQuote);
        const QRect buttonRect(
            button->mapTo(quotedEditor.viewport(), QPoint(0, 0)),
            button->size());
        return button->parentWidget() == quotedEditor.viewport()
            && buttonRect.left() >= closingQuoteEndRect.left()
            && buttonRect.left() <= closingQuoteEndRect.left() + 20;
    };
    const bool doubleQuotedButtonFollowsQuote =
        quotedButtonFollowsClosingQuote(QLatin1Char('"'));
    const bool singleQuotedButtonFollowsQuote =
        quotedButtonFollowsClosingQuote(QLatin1Char('\''));
    check(doubleQuotedButtonFollowsQuote && singleQuotedButtonFollowsQuote,
        "cog_inline_assignment_ui anchors quoted resource affordances after the closing quote for both quote styles");

    const QString declarationWithoutInitializer = QStringLiteral(
        "symbols\n"
        "sound ambience local desc=keep # retain this comment\n"
        "end\n");
    sed::ui::cog::CogScriptCodeEditor uninitializedEditor;
    configureEditor(uninitializedEditor, declarationWithoutInitializer);
    QString uninitializedCurrentValue = QStringLiteral("not-called");
    uninitializedEditor.setInlineAssignmentCallbacks(
        [&uninitializedCurrentValue](const sed::ui::ResourcePickerKind kind,
                                     const QString& currentValue)
        {
            uninitializedCurrentValue = currentValue;
            return kind == sed::ui::ResourcePickerKind::wav
                ? std::optional<QString>{QStringLiteral("picked.wav")}
                : std::nullopt;
        },
        []() {});
    moveMouseToEditorColumn(
        uninitializedEditor,
        1,
        QStringLiteral("sound ambience local desc=keep # retain this comment")
            .indexOf(QStringLiteral("ambience")));
    QToolButton* uninitializedButton = inlineAssignmentButton(uninitializedEditor);
    const bool uninitializedButtonWasVisible =
        uninitializedButton && uninitializedButton->isVisible();
    if ( uninitializedButtonWasVisible )
    {
        uninitializedButton->click();
    }
    check(uninitializedButtonWasVisible
            && uninitializedCurrentValue.isEmpty()
            && uninitializedEditor.toPlainText() == QStringLiteral(
                "symbols\n"
                "sound ambience = picked.wav local desc=keep # retain this comment\n"
                "end\n")
            && uninitializedEditor.document()->isUndoAvailable(),
        "cog_inline_assignment_ui inserts the exact resource initializer after a declaration name and before preserved attributes and comments");
    if ( uninitializedEditor.document()->isUndoAvailable() )
    {
        uninitializedEditor.undo();
    }
    const bool undoRestoredOriginal = uninitializedEditor.toPlainText()
        == declarationWithoutInitializer
        && !uninitializedEditor.document()->isUndoAvailable()
        && uninitializedEditor.document()->isRedoAvailable();
    check(undoRestoredOriginal,
        "cog_inline_assignment_ui undoes a missing-initializer resource pick as one edit");
    if ( uninitializedEditor.document()->isRedoAvailable() )
    {
        uninitializedEditor.redo();
    }
    check(uninitializedEditor.toPlainText() == QStringLiteral(
            "symbols\n"
            "sound ambience = picked.wav local desc=keep # retain this comment\n"
            "end\n"),
        "cog_inline_assignment_ui redoes the exact inserted initializer");
}

} // namespace sed::app_tests
