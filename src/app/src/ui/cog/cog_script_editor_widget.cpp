#include "ui/cog/cog_script_editor_widget.hpp"

#include "ui/cog/cog_script_format_utils.hpp"
#include "ui/cog/cog_script_language_utils.hpp"
#include "ui/cog/cog_script_text_utils.hpp"

#include <algorithm>
#include <optional>
#include <utility>

#include <QAbstractItemView>
#include <QCompleter>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QEvent>
#include <QHelpEvent>
#include <QHash>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStringListModel>
#include <QToolButton>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextOption>
#include <QTimer>
#include <QToolTip>
#include <QVector>

namespace sed::ui::cog
{

namespace
{

QString leadingWhitespace(const QString& text)
{
    int end = 0;
    while (end < text.size()
        && (text.at(end) == QLatin1Char(' ') || text.at(end) == QLatin1Char('\t')))
    {
        ++end;
    }
    return text.left(end);
}

struct SymbolInitializerSpan
{
    int start{};
    int length{};
    bool numeric{};
};

std::optional<SymbolInitializerSpan> symbolInitializerSpan(
    const QString& line,
    const int symbolNameEnd,
    const QSet<QString>& attributeNames)
{
    int cursor = symbolNameEnd;
    while (cursor < line.size() && line.at(cursor).isSpace())
    {
        ++cursor;
    }
    if (cursor >= line.size() || line.at(cursor) != QLatin1Char('='))
    {
        return std::nullopt;
    }

    const int valueStartAfterEquals = cursor + 1;
    ++cursor;
    while (cursor < line.size() && line.at(cursor).isSpace())
    {
        ++cursor;
    }
    if (cursor >= line.size())
    {
        return std::nullopt;
    }
    const bool separatedFromEquals = cursor > valueStartAfterEquals;

    const QChar quote = line.at(cursor);
    if (quote == QLatin1Char('\'') || quote == QLatin1Char('"'))
    {
        bool escaped = false;
        for (int end = cursor + 1; end < line.size(); ++end)
        {
            const QChar ch = line.at(end);
            if (escaped)
            {
                escaped = false;
                continue;
            }
            if (ch == QLatin1Char('\\'))
            {
                escaped = true;
                continue;
            }
            if (ch == quote)
            {
                return SymbolInitializerSpan{
                    .start = cursor,
                    .length = end - cursor + 1,
                    .numeric = false,
                };
            }
        }
        return std::nullopt;
    }

    const int valueStart = cursor;
    while (cursor < line.size()
        && !line.at(cursor).isSpace()
        && line.at(cursor) != QLatin1Char(',')
        && line.at(cursor) != QLatin1Char(';'))
    {
        ++cursor;
    }
    if (cursor == valueStart)
    {
        return std::nullopt;
    }

    const QString value = line.mid(valueStart, cursor - valueStart);
    const QString lowerValue = value.toLower();
    if (value.contains(QLatin1Char('='))
        || (separatedFromEquals && attributeNames.contains(lowerValue)))
    {
        return std::nullopt;
    }

    static const QRegularExpression kNumericInitializerRx(
        QStringLiteral(
            "^[+-]?(?:0[xX][0-9A-Fa-f]+|"
            "(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][+-]?\\d+)?)$"),
        QRegularExpression::UseUnicodePropertiesOption);
    return SymbolInitializerSpan{
        .start = valueStart,
        .length = cursor - valueStart,
        .numeric = kNumericInitializerRx.match(value).hasMatch(),
    };
}

bool isCodeSectionAt(const QTextDocument* document, const QTextBlock& currentBlock)
{
    if (!document || !currentBlock.isValid())
    {
        return false;
    }

    bool inCode = false;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next())
    {
        const QString line = stripComment(block.text()).trimmed();
        if (line.compare(QStringLiteral("code"), Qt::CaseInsensitive) == 0)
        {
            inCode = true;
        }
        else if (line.compare(QStringLiteral("symbols"), Qt::CaseInsensitive) == 0
            || line.compare(QStringLiteral("end"), Qt::CaseInsensitive) == 0)
        {
            inCode = false;
        }

        if (block == currentBlock)
        {
            return inCode;
        }
    }
    return false;
}

int braceDelta(const QString& text);

std::optional<QString> matchingOpenBraceIndentBefore(const QTextCursor& cursor)
{
    const QTextDocument* document = cursor.document();
    const QTextBlock currentBlock = cursor.block();
    if (!document || !currentBlock.isValid())
    {
        return std::nullopt;
    }

    QVector<QString> openBraceIndents{};
    bool inCode = false;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next())
    {
        QString source = block.text();
        if (block == currentBlock)
        {
            const int offset = std::clamp(
                cursor.position() - block.position(),
                0,
                static_cast<int>(source.size()));
            source = source.left(offset);
        }

        const QString structural = stripComment(source).trimmed();
        if (structural.compare(QStringLiteral("code"), Qt::CaseInsensitive) == 0)
        {
            inCode = true;
            openBraceIndents.clear();
        }
        else if (structural.compare(QStringLiteral("symbols"), Qt::CaseInsensitive) == 0
            || structural.compare(QStringLiteral("end"), Qt::CaseInsensitive) == 0)
        {
            inCode = false;
            openBraceIndents.clear();
        }
        else if (inCode)
        {
            const QString analysis = sanitizeLineForAnalysis(source);
            const QString indent = leadingWhitespace(block.text());
            for (const QChar ch : analysis)
            {
                if (ch == QLatin1Char('{'))
                {
                    openBraceIndents.push_back(indent);
                }
                else if (ch == QLatin1Char('}') && !openBraceIndents.isEmpty())
                {
                    openBraceIndents.pop_back();
                }
            }
        }

        if (block == currentBlock)
        {
            break;
        }
    }

    if (openBraceIndents.isEmpty())
    {
        return std::nullopt;
    }
    return openBraceIndents.back();
}

int codeBraceDepthBefore(const QTextDocument* document, const QTextBlock& currentBlock)
{
    if (!document || !currentBlock.isValid())
    {
        return 0;
    }

    bool inCode = false;
    int depth = 0;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next())
    {
        if (block == currentBlock)
        {
            return inCode ? std::max(0, depth) : 0;
        }

        const QString line = stripComment(block.text()).trimmed();
        if (line.compare(QStringLiteral("code"), Qt::CaseInsensitive) == 0)
        {
            inCode = true;
            depth = 0;
        }
        else if (line.compare(QStringLiteral("symbols"), Qt::CaseInsensitive) == 0
            || line.compare(QStringLiteral("end"), Qt::CaseInsensitive) == 0)
        {
            inCode = false;
            depth = 0;
        }
        else if (inCode)
        {
            depth = std::max(0, depth + braceDelta(block.text()));
        }
    }
    return 0;
}

int braceDelta(const QString& text)
{
    const QString analysis = sanitizeLineForAnalysis(text);
    int delta = 0;
    for (const QChar ch : analysis)
    {
        if (ch == QLatin1Char('{'))
        {
            ++delta;
        }
        else if (ch == QLatin1Char('}'))
        {
            --delta;
        }
    }
    return delta;
}

void removeIndentUnit(QString& indent, const bool useTabs, const int indentWidth)
{
    if (indent.isEmpty())
    {
        return;
    }

    if (useTabs && indent.endsWith(QLatin1Char('\t')))
    {
        indent.chop(1);
        return;
    }

    int spaces = 0;
    while (spaces < indent.size() && indent.at(indent.size() - spaces - 1) == QLatin1Char(' '))
    {
        ++spaces;
    }
    if (spaces > 0)
    {
        indent.chop(std::min(spaces, std::max(1, indentWidth)));
    }
    else if (indent.endsWith(QLatin1Char('\t')))
    {
        indent.chop(1);
    }
}

QString automaticNewlineIndent(
    const QTextCursor& cursor,
    const bool useTabs,
    const int indentWidth)
{
    const QTextBlock block = cursor.block();
    if (!block.isValid())
    {
        return {};
    }

    const qsizetype offset = std::clamp<qsizetype>(
        cursor.position() - block.position(),
        0,
        block.text().size());
    QString currentPrefix = block.text().left(offset);
    QString indent = leadingWhitespace(currentPrefix);
    if (!isCodeSectionAt(cursor.document(), block))
    {
        return indent;
    }

    const int depthBefore = codeBraceDepthBefore(cursor.document(), block);
    const QString analysis = sanitizeLineForAnalysis(currentPrefix).trimmed();
    int leadingClosers = 0;
    int afterLeadingClosers = 0;
    while (afterLeadingClosers < analysis.size())
    {
        while (afterLeadingClosers < analysis.size()
            && analysis.at(afterLeadingClosers).isSpace())
        {
            ++afterLeadingClosers;
        }
        if (afterLeadingClosers >= analysis.size()
            || analysis.at(afterLeadingClosers) != QLatin1Char('}'))
        {
            break;
        }
        ++leadingClosers;
        ++afterLeadingClosers;
    }

    const bool seededFromBraceDepth = indent.isEmpty() && depthBefore > 0;
    if (seededFromBraceDepth)
    {
        // A line that starts with a closing brace belongs to the parent
        // scope.  Seed it at that scope before applying any later braces.
        indent = indentString(
            std::max(0, depthBefore - leadingClosers),
            useTabs,
            indentWidth);
    }

    const QString remainder = analysis.mid(afterLeadingClosers).trimmed();
    const int remainderDelta = braceDelta(remainder);
    // For `} else {`, the closing brace aligns the current line with its
    // parent, while the later opening brace increases the next line's
    // indentation.  Do not cancel the current line's parent indentation.
    const int delta = leadingClosers > 0 && remainderDelta > 0
        ? remainderDelta
        : (seededFromBraceDepth && leadingClosers > 0
                ? remainderDelta
                : braceDelta(analysis));
    if (delta > 0)
    {
        indent += indentString(delta, useTabs, indentWidth);
    }
    else
    {
        for (int i = 0; i > delta; --i)
        {
            removeIndentUnit(indent, useTabs, indentWidth);
        }
    }
    return indent;
}

} // namespace

QFont resolveCogScriptEditorFont(QSettings& settings)
{
    return makeCogScriptEditorFont(
        settings.value(
            "cog_editor/font_family",
            defaultCogScriptEditorFontFamilyList()).toString(),
        settings.value("cog_editor/font_size", 10).toInt(),
        settings.value("cog_editor/font_bold", false).toBool(),
        settings.value("cog_editor/font_italic", false).toBool());
}

QString defaultCogScriptEditorFontFamilyList()
{
    return QStringLiteral("Consolas, 'Courier New', monospace");
}

QStringList availableCogScriptEditorFontFamilies()
{
    // Qt 5 exposes families() on an instance while Qt 6 made it static.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QFontDatabase::families();
#else
    const QFontDatabase database{};
    return database.families();
#endif
}

QFont makeCogScriptEditorFont(
    const QString& familyList,
    const int pointSize,
    const bool bold,
    const bool italic)
{
    QStringList families{};
    for (QString family : familyList.split(QLatin1Char(','), Qt::SkipEmptyParts))
    {
        family = family.trimmed();
        if (family.size() >= 2
            && ((family.front() == QLatin1Char('\'') && family.back() == QLatin1Char('\''))
                || (family.front() == QLatin1Char('"') && family.back() == QLatin1Char('"'))))
        {
            family = family.mid(1, family.size() - 2).trimmed();
        }
        if (!family.isEmpty() && !families.contains(family, Qt::CaseInsensitive))
        {
            families.push_back(family);
        }
    }
    if (families.isEmpty())
    {
        families.push_back(QStringLiteral("Consolas"));
    }

    QFont font{};
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    font.setFamilies(families);
#else
    // Qt 5 has no ordered-fallback setter. Preserve the user's primary
    // family and retain the monospace style hint as the portable fallback.
    font.setFamily(families.front());
#endif
    font.setPointSize(std::clamp(pointSize, 7, 32));
    font.setBold(bold);
    font.setItalic(italic);
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    return font;
}

int clampCogScriptEditorZoomPercent(const int percent) noexcept
{
    return std::clamp(percent, 20, 400);
}

std::optional<int> parseCogScriptEditorZoomPercent(const QStringView text)
{
    QString normalized = text.trimmed().toString();
    if (normalized.endsWith(QLatin1Char('%')))
    {
        normalized.chop(1);
        normalized = normalized.trimmed();
    }
    bool ok = false;
    const int percent = normalized.toInt(&ok);
    if (!ok || normalized.isEmpty())
    {
        return std::nullopt;
    }
    return clampCogScriptEditorZoomPercent(percent);
}

QFont cogScriptEditorFontAtZoom(const QFont& baseFont, const int percent)
{
    QFont zoomed = baseFont;
    const qreal baseSize = baseFont.pointSizeF() > 0.0
        ? baseFont.pointSizeF()
        : 10.0;
    zoomed.setPointSizeF(std::clamp(
        baseSize * static_cast<qreal>(clampCogScriptEditorZoomPercent(percent)) / 100.0,
        1.0,
        512.0));
    return zoomed;
}

class CogScriptLineNumberArea final : public QWidget
{
public:
    explicit CogScriptLineNumberArea(CogScriptCodeEditor* editor)
        : QWidget(editor)
        , editor_(editor)
    {
    }

    QSize sizeHint() const override
    {
        return QSize(editor_ ? editor_->lineNumberAreaWidth() : 0, 0);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        if (editor_)
        {
            editor_->lineNumberAreaPaintEvent(event);
        }
    }

private:
    CogScriptCodeEditor* editor_{};
};

CogScriptCodeEditor::CogScriptCodeEditor(QWidget* parent)
    : QPlainTextEdit(parent)
    , lineNumberArea_(new CogScriptLineNumberArea(this))
    , completionModel_(new QStringListModel(this))
    , completer_(new QCompleter(completionModel_, this))
{
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabChangesFocus(false);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    lineNumberArea_->setMouseTracking(true);

    inlineAssignmentButton_ = new QToolButton(viewport());
    inlineAssignmentButton_->setObjectName(
        QStringLiteral("cogInlineAssignmentPickerButton"));
    inlineAssignmentButton_->setText(QStringLiteral("⋯"));
    inlineAssignmentButton_->setAutoRaise(true);
    inlineAssignmentButton_->setFocusPolicy(Qt::NoFocus);
    inlineAssignmentButton_->setCursor(Qt::PointingHandCursor);
    inlineAssignmentButton_->setAccessibleName(
        QStringLiteral("Pick assigned COG resource or edit flags"));
    inlineAssignmentButton_->setToolTip(
        QStringLiteral("Pick assigned resource"));
    inlineAssignmentButton_->setStyleSheet(QStringLiteral(
        "QToolButton { border: 0; border-radius: 4px; padding: 0;"
        " color: rgba(230, 234, 240, 225); background: rgba(70, 91, 124, 220); }"
        "QToolButton:hover { background: rgba(102, 135, 184, 245); color: white; }"));
    inlineAssignmentButton_->hide();
    inlineAssignmentButton_->installEventFilter(this);
    lineNumberArea_->installEventFilter(this);
    viewport()->installEventFilter(this);

    inlineAssignmentHideTimer_ = new QTimer(this);
    inlineAssignmentHideTimer_->setSingleShot(true);
    inlineAssignmentHideTimer_->setInterval(130);
    connect(inlineAssignmentHideTimer_, &QTimer::timeout, this, [this]()
        {
            clearInlineAssignmentButton();
        });
    connect(inlineAssignmentButton_, &QToolButton::clicked, this, [this]()
        {
            activateInlineAssignmentPicker();
        });
    connect(this, &QPlainTextEdit::textChanged, this, [this]()
        {
            clearInlineAssignmentButton();
        });

    // Step 1: bind the completer to this editor and insert the selected suffix.
    completer_->setWidget(this);
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    completer_->setCompletionMode(QCompleter::PopupCompletion);
    connect(completer_, qOverload<const QString&>(&QCompleter::activated), this, [this](const QString& completion)
        {
            insertCompletion(completion);
        });

    // Step 2: keep the gutter geometry synchronized with the document viewport.
    connect(this, &QPlainTextEdit::blockCountChanged, this, [this](int)
        {
            updateLineNumberAreaWidth();
        });
    connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect& rect, const int dy)
        {
            updateLineNumberArea(rect, dy);
            updateInlineAssignmentButtonGeometry();
        });
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [this](int)
        {
            updateInlineAssignmentButtonGeometry();
        });
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int)
        {
            updateInlineAssignmentButtonGeometry();
        });
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this]()
        {
            refreshExtraSelections();
        });
    updateLineNumberAreaWidth();
}

void CogScriptCodeEditor::setIndentationStyle(const bool useTabs, const int indentWidth)
{
    useTabs_ = useTabs;
    indentWidth_ = std::clamp(indentWidth, 1, 8);
    QFontMetrics metrics(font());
    setTabStopDistance(static_cast<qreal>(metrics.horizontalAdvance(QLatin1Char(' ')) * indentWidth_));
}

void CogScriptCodeEditor::setMessageWords(QStringList words)
{
    messageWords_ = std::move(words);
}

void CogScriptCodeEditor::applyTheme(const CogScriptTheme& theme)
{
    theme_ = theme;

    // Step 1: apply palette colors used by Qt's text and selection painting.
    QPalette pal = palette();
    pal.setColor(QPalette::Base, theme.editorBackground);
    pal.setColor(QPalette::Text, theme.editorForeground);
    pal.setColor(QPalette::Highlight, theme.selectionBackground);
    pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    setPalette(pal);

    // Step 2: preserve the editor chrome while injecting the theme selection color.
    setStyleSheet(QStringLiteral(
        "QPlainTextEdit { border: 1px solid rgba(120,150,210,45); border-radius: 6px; padding: 4px;"
        " selection-background-color: %1; }")
            .arg(theme.selectionBackground.name(QColor::HexArgb)));

    // Step 3: refresh dependent layout and selection overlays.
    setIndentationStyle(useTabs_, indentWidth_);
    updateLineNumberAreaWidth();
    refreshExtraSelections();
}

void CogScriptCodeEditor::setWhitespaceVisible(const bool visible)
{
    whitespaceVisible_ = visible;
    QTextOption option = document()->defaultTextOption();
    QTextOption::Flags flags = option.flags();
    if (visible)
    {
        flags |= QTextOption::ShowTabsAndSpaces;
    }
    else
    {
        flags &= ~QTextOption::ShowTabsAndSpaces;
    }
    option.setFlags(flags);
    document()->setDefaultTextOption(option);
    viewport()->update();
}

void CogScriptCodeEditor::setBottomAccessoryHeight(const int height)
{
    bottomAccessoryHeight_ = std::max(0, height);
    updateLineNumberAreaWidth();
    const QRect viewportRect = viewport()->geometry();
    lineNumberArea_->setGeometry(QRect(
        contentsRect().left(),
        viewportRect.top(),
        lineNumberAreaWidth(),
        viewportRect.height()));
}

void CogScriptCodeEditor::setResizeLayoutCompletedCallback(
    std::function<void()> callback)
{
    resizeLayoutCompletedCallback_ = std::move(callback);
}

void CogScriptCodeEditor::setInlineAssignmentCallbacks(
    InlineResourcePickerCallback resourcePicker,
    InlineFlagsEditorCallback flagsEditor,
    InlineMaskEditorCallback maskEditor)
{
    inlineResourcePickerCallback_ = std::move(resourcePicker);
    inlineFlagsEditorCallback_ = std::move(flagsEditor);
    inlineMaskEditorCallback_ = std::move(maskEditor);
    if ( inlineAssignmentSpan_ )
    {
        const bool configured =
            inlineAssignmentSpan_->kind == CogScriptInlineAssignmentKind::flags
                ? static_cast<bool>(inlineFlagsEditorCallback_)
            : inlineAssignmentSpan_->kind == CogScriptInlineAssignmentKind::mask
                ? static_cast<bool>(inlineMaskEditorCallback_)
                : static_cast<bool>(inlineResourcePickerCallback_);
        if ( configured )
        {
            updateInlineAssignmentButtonGeometry();
        }
        else
        {
            clearInlineAssignmentButton();
        }
    }
}

void CogScriptCodeEditor::triggerCompletion()
{
    completionModel_->setStringList(buildCogScriptCompletionWords(toPlainText(), messageWords_));
    const QString prefix = textUnderCursor();
    completer_->setCompletionPrefix(prefix);
    if (QAbstractItemView* popup = completer_->popup())
    {
        popup->setCurrentIndex(completer_->completionModel()->index(0, 0));
    }
    QRect cr = cursorRect();
    cr.setWidth(280);
    completer_->complete(cr);
}

void CogScriptCodeEditor::setDiagnosticSelections(const QList<QTextEdit::ExtraSelection>& selections)
{
    diagnosticSelections_ = selections;
    refreshExtraSelections();
}

void CogScriptCodeEditor::setDiagnosticEntries(const std::vector<CogScriptDiagnostic>& diagnostics)
{
    diagnosticEntries_ = diagnostics;
}

int CogScriptCodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = std::max(1, blockCount());
    while (max >= 10)
    {
        max /= 10;
        ++digits;
    }
    return 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void CogScriptCodeEditor::lineNumberAreaPaintEvent(QPaintEvent* event)
{
    QPainter painter(lineNumberArea_);
    painter.fillRect(event->rect(), theme_.lineNumberBackground);

    // Step 1: find the first visible text block and its viewport bounds.
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    // Step 2: draw only the visible line numbers, matching Qt's scroll window.
    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            const QString number = QString::number(blockNumber + 1);
            painter.setPen(theme_.lineNumberForeground);
            painter.drawText(
                0,
                top,
                lineNumberArea_->width() - 5,
                fontMetrics().height(),
                Qt::AlignRight,
                number);
        }
        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void CogScriptCodeEditor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    const QRect viewportRect = viewport()->geometry();
    lineNumberArea_->setGeometry(QRect(
        contentsRect().left(),
        viewportRect.top(),
        lineNumberAreaWidth(),
        viewportRect.height()));
    if (resizeLayoutCompletedCallback_)
    {
        resizeLayoutCompletedCallback_();
    }
    updateInlineAssignmentButtonGeometry();
}

bool CogScriptCodeEditor::event(QEvent* event)
{
    if (event && event->type() == QEvent::ToolTip)
    {
        auto* helpEvent = static_cast<QHelpEvent*>(event);
        const QTextCursor cursor = cursorForPosition(helpEvent->pos());
        const int line = cursor.blockNumber();
        const int column = cursor.position() - cursor.block().position();
        for (const CogScriptDiagnostic& diagnostic : diagnosticEntries_)
        {
            if (diagnostic.line != line)
            {
                continue;
            }
            const int start = std::max(0, diagnostic.column);
            const int end = start + std::max(1, diagnostic.length);
            if (diagnostic.column < 0 || (column >= start && column <= end))
            {
                QToolTip::showText(helpEvent->globalPos(), diagnostic.message, viewport());
                return true;
            }
        }
    }
    if ( event && event->type() == QEvent::Hide )
    {
        clearInlineAssignmentButton();
    }
    const bool handled = QPlainTextEdit::event(event);
    if ( event && (event->type() == QEvent::FontChange
                     || event->type() == QEvent::Show ) )
    {
        updateInlineAssignmentButtonGeometry();
    }
    return handled;
}

bool CogScriptCodeEditor::eventFilter(QObject* watched, QEvent* event)
{
    if ( !event )
    {
        return QPlainTextEdit::eventFilter(watched, event);
    }

    if ( watched == viewport() )
    {
        if ( event->type() == QEvent::MouseMove )
        {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            updateInlineAssignmentAtPosition(mouse->pos());
        }
        else if ( event->type() == QEvent::Leave )
        {
            scheduleInlineAssignmentHide();
        }
    }

    if ( watched == lineNumberArea_ || watched == inlineAssignmentButton_ )
    {
        if ( watched == inlineAssignmentButton_
            && event->type() == QEvent::MouseButtonRelease )
        {
            const auto* const mouse = static_cast<QMouseEvent*>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            inlineAssignmentEventGlobalPosition_ = mouse->globalPosition().toPoint();
#else
            inlineAssignmentEventGlobalPosition_ = mouse->globalPos();
#endif
            QTimer::singleShot(0, this, [this]()
                {
                    inlineAssignmentEventGlobalPosition_.reset();
                });
        }
        if ( event->type() == QEvent::Enter )
        {
            if ( inlineAssignmentHideTimer_ )
            {
                inlineAssignmentHideTimer_->stop();
            }
        }
        else if ( event->type() == QEvent::Leave )
        {
            scheduleInlineAssignmentHide();
        }
    }

    return QPlainTextEdit::eventFilter(watched, event);
}

void CogScriptCodeEditor::updateInlineAssignmentAtPosition(
    const QPoint& viewportPosition)
{
    if ( !inlineAssignmentButton_ || isReadOnly() )
    {
        clearInlineAssignmentButton();
        return;
    }

    const QTextCursor cursor = cursorForPosition(viewportPosition);
    const QTextBlock block = cursor.block();
    if ( !block.isValid() )
    {
        clearInlineAssignmentButton();
        return;
    }

    const int column = cursor.position() - block.position();
    const CogScriptEditorSection section =
        CogScriptSyntaxHighlighter::sectionForBlock(block);
    std::optional<CogScriptInlineAssignmentSpan> span =
        cogScriptInlineAssignmentAtColumn(block.text(), section, column);
    if ( !span && column > 0 && block.text().at(column - 1) == QLatin1Char('=') )
    {
        span = cogScriptInlineAssignmentAtColumn(
            block.text(), section, column - 1);
    }
    if ( !span && column > 0 )
    {
        span = cogScriptInlineAssignmentAtColumn(
            block.text(), section, column - 1);
    }

    const bool hasCallback = span
        && (span->kind == CogScriptInlineAssignmentKind::flags
                ? static_cast<bool>(inlineFlagsEditorCallback_)
            : span->kind == CogScriptInlineAssignmentKind::mask
                ? static_cast<bool>(inlineMaskEditorCallback_)
                : static_cast<bool>(inlineResourcePickerCallback_));
    if ( !hasCallback )
    {
        const bool sameUnchangedRow = inlineAssignmentSpan_
            && inlineAssignmentBlockNumber_ == block.blockNumber()
            && inlineAssignmentRevision_ == document()->revision()
            && inlineAssignmentLineText_ == block.text();
        if ( sameUnchangedRow )
        {
            // Keep the helper armed while the pointer crosses the declaration
            // text toward its inline button. A different block, document
            // revision, or line contents invalidates this cached target.
            inlineAssignmentHideTimer_->stop();
            updateInlineAssignmentButtonGeometry();
            return;
        }
        clearInlineAssignmentButton();
        return;
    }

    inlineAssignmentSpan_ = std::move(span);
    inlineAssignmentBlockNumber_ = block.blockNumber();
    inlineAssignmentRevision_ = document()->revision();
    inlineAssignmentLineText_ = block.text();
    inlineAssignmentHideTimer_->stop();
    updateInlineAssignmentButtonGeometry();
}

void CogScriptCodeEditor::updateInlineAssignmentButtonGeometry()
{
    if ( !inlineAssignmentButton_ || !viewport()
        || !inlineAssignmentSpan_ || inlineAssignmentBlockNumber_ < 0 )
    {
        return;
    }

    if ( inlineAssignmentRevision_ != document()->revision() )
    {
        clearInlineAssignmentButton();
        return;
    }

    const QTextBlock block = document()->findBlockByNumber(
        inlineAssignmentBlockNumber_);
    if ( !block.isValid() || block.text() != inlineAssignmentLineText_ )
    {
        clearInlineAssignmentButton();
        return;
    }

    const int top = static_cast<int>(
        blockBoundingGeometry(block).translated(contentOffset()).top());
    const int height = std::max(1, static_cast<int>(blockBoundingRect(block).height()));
    if ( top + height <= 0 || top >= viewport()->height() )
    {
        inlineAssignmentButton_->hide();
        return;
    }

    const int anchorOffset = inlineAssignmentSpan_->hasInitializer
        ? inlineAssignmentSpan_->valueStart + inlineAssignmentSpan_->valueLength
        : inlineAssignmentSpan_->valueStart;
    QTextCursor anchor(block);
    anchor.setPosition(block.position() + std::clamp(
        anchorOffset, 0, static_cast<int>(block.text().size())));
    const QRect anchorRect = cursorRect(anchor);
    constexpr int kInlineAssignmentButtonWidth = 20;
    const int buttonWidth = std::min(
        kInlineAssignmentButtonWidth,
        std::max(1, viewport()->width()));
    const int buttonHeight = std::min(
        std::max(1, anchorRect.height()),
        std::max(1, fontMetrics().height()));
    const int buttonX = anchorRect.right() + 2;
    if (anchorRect.left() < 0
        || buttonX < 0
        || buttonX + buttonWidth > viewport()->width())
    {
        inlineAssignmentButton_->hide();
        return;
    }

    inlineAssignmentButton_->setFixedSize(buttonWidth, buttonHeight);
    const int buttonY = std::clamp(
        anchorRect.center().y() - buttonHeight / 2,
        0,
        std::max(0, viewport()->height() - buttonHeight));
    inlineAssignmentButton_->setGeometry(
        buttonX,
        buttonY,
        buttonWidth,
        buttonHeight);
    inlineAssignmentButton_->raise();
    inlineAssignmentButton_->show();
}

void CogScriptCodeEditor::scheduleInlineAssignmentHide()
{
    if ( inlineAssignmentHideTimer_ && inlineAssignmentButton_
        && inlineAssignmentButton_->isVisible() )
    {
        inlineAssignmentHideTimer_->start();
    }
}

void CogScriptCodeEditor::clearInlineAssignmentButton()
{
    if ( inlineAssignmentHideTimer_ )
    {
        inlineAssignmentHideTimer_->stop();
    }
    if ( inlineAssignmentButton_ )
    {
        inlineAssignmentButton_->hide();
    }
    inlineAssignmentSpan_.reset();
    inlineAssignmentBlockNumber_ = -1;
    inlineAssignmentRevision_ = -1;
    inlineAssignmentLineText_.clear();
}

void CogScriptCodeEditor::activateInlineAssignmentPicker()
{
    if ( isReadOnly() )
    {
        clearInlineAssignmentButton();
        return;
    }

    const QPoint fallbackAnchor = inlineAssignmentButton_
        ? inlineAssignmentButton_->mapToGlobal(
              inlineAssignmentButton_->rect().center())
        : viewport()->mapToGlobal(viewport()->rect().center());
    inlineAssignmentActivationAnchor_ =
        inlineAssignmentEventGlobalPosition_.value_or(fallbackAnchor);
    inlineAssignmentEventGlobalPosition_.reset();

    if ( !inlineAssignmentSpan_ || inlineAssignmentBlockNumber_ < 0 )
    {
        inlineAssignmentActivationAnchor_.reset();
        return;
    }

    const CogScriptInlineAssignmentSpan expected = *inlineAssignmentSpan_;
    const int targetBlockNumber = inlineAssignmentBlockNumber_;
    const int sourceRevision = inlineAssignmentRevision_;
    const QString originalLine = inlineAssignmentLineText_;
    QPointer<CogScriptCodeEditor> guardedEditor(this);
    QPointer<QTextDocument> guardedDocument(document());
    if ( !guardedDocument || guardedDocument->revision() != sourceRevision )
    {
        clearInlineAssignmentButton();
        inlineAssignmentActivationAnchor_.reset();
        return;
    }

    const QTextBlock initialBlock = guardedDocument->findBlockByNumber(
        targetBlockNumber);
    const auto currentTarget = initialBlock.isValid()
        ? cogScriptInlineAssignmentAtColumn(
              initialBlock.text(),
              expected.section,
              expected.hasInitializer
                  ? expected.equalsStart
                  : expected.symbolNameStart)
        : std::nullopt;
    if ( !initialBlock.isValid() || initialBlock.text() != originalLine
        || !currentTarget || *currentTarget != expected )
    {
        clearInlineAssignmentButton();
        inlineAssignmentActivationAnchor_.reset();
        return;
    }

    const QTextCursor originalCursor = textCursor();
    const int originalVerticalScroll = verticalScrollBar()->value();
    const int originalHorizontalScroll = horizontalScrollBar()->value();

    if ( expected.kind == CogScriptInlineAssignmentKind::flags )
    {
        const InlineFlagsEditorCallback callback = inlineFlagsEditorCallback_;
        if ( !callback )
        {
            return;
        }
        callback();
        if ( !guardedEditor )
        {
            return;
        }
        guardedEditor->inlineAssignmentActivationAnchor_.reset();
        if ( !guardedDocument
            || guardedEditor->document() != guardedDocument
            || guardedDocument->revision() != sourceRevision )
        {
            return;
        }
        guardedEditor->setTextCursor(originalCursor);
        guardedEditor->verticalScrollBar()->setValue(originalVerticalScroll);
        guardedEditor->horizontalScrollBar()->setValue(originalHorizontalScroll);
        guardedEditor->setFocus(Qt::OtherFocusReason);
        return;
    }

    if ( expected.kind == CogScriptInlineAssignmentKind::mask )
    {
        const InlineMaskEditorCallback callback = inlineMaskEditorCallback_;
        if ( !callback )
        {
            return;
        }
        callback(expected, targetBlockNumber, sourceRevision);
        if ( !guardedEditor )
        {
            return;
        }
        guardedEditor->inlineAssignmentActivationAnchor_.reset();
        if ( !guardedDocument
            || guardedEditor->document() != guardedDocument
            || guardedDocument->revision() != sourceRevision )
        {
            return;
        }
        guardedEditor->setTextCursor(originalCursor);
        guardedEditor->verticalScrollBar()->setValue(originalVerticalScroll);
        guardedEditor->horizontalScrollBar()->setValue(originalHorizontalScroll);
        guardedEditor->setFocus(Qt::OtherFocusReason);
        return;
    }

    const InlineResourcePickerCallback callback = inlineResourcePickerCallback_;
    if ( !callback || !expected.resourceKind )
    {
        return;
    }
    const std::optional<QString> picked = callback(
        *expected.resourceKind,
        expected.currentValue);
    if ( !guardedEditor )
    {
        return;
    }
    guardedEditor->inlineAssignmentActivationAnchor_.reset();
    if ( !guardedDocument
        || guardedEditor->document() != guardedDocument )
    {
        return;
    }

    if ( guardedDocument->revision() != sourceRevision )
    {
        // A resource dialog may outlive an edited or recovered source buffer.
        // Never apply its result to a different revision.
        return;
    }

    if ( guardedEditor->isReadOnly() )
    {
        guardedEditor->setTextCursor(originalCursor);
        guardedEditor->verticalScrollBar()->setValue(originalVerticalScroll);
        guardedEditor->horizontalScrollBar()->setValue(originalHorizontalScroll);
        guardedEditor->clearInlineAssignmentButton();
        return;
    }

    if ( !picked )
    {
        guardedEditor->setTextCursor(originalCursor);
        guardedEditor->verticalScrollBar()->setValue(originalVerticalScroll);
        guardedEditor->horizontalScrollBar()->setValue(originalHorizontalScroll);
        guardedEditor->setFocus(Qt::OtherFocusReason);
        return;
    }

    const QTextBlock targetBlock = guardedDocument->findBlockByNumber(
        targetBlockNumber);
    const auto revalidated = targetBlock.isValid()
        ? cogScriptInlineAssignmentAtColumn(
              targetBlock.text(),
              expected.section,
              expected.hasInitializer
                  ? expected.equalsStart
                  : expected.symbolNameStart)
        : std::nullopt;
    if ( !targetBlock.isValid() || targetBlock.text() != originalLine
        || !revalidated || *revalidated != expected )
    {
        return;
    }

    const std::optional<QString> replacedLine =
        replaceCogScriptInlineResourceValue(
            targetBlock.text(), expected, *picked);
    if ( !replacedLine )
    {
        guardedEditor->setTextCursor(originalCursor);
        guardedEditor->verticalScrollBar()->setValue(originalVerticalScroll);
        guardedEditor->horizontalScrollBar()->setValue(originalHorizontalScroll);
        guardedEditor->setFocus(Qt::OtherFocusReason);
        return;
    }

    if ( *replacedLine == originalLine )
    {
        guardedEditor->setTextCursor(originalCursor);
        guardedEditor->verticalScrollBar()->setValue(originalVerticalScroll);
        guardedEditor->horizontalScrollBar()->setValue(originalHorizontalScroll);
        guardedEditor->setFocus(Qt::OtherFocusReason);
        return;
    }

    const int replacementLength = replacedLine->size()
        - targetBlock.text().size() + expected.valueLength;
    if ( replacementLength < 0 )
    {
        return;
    }
    const QString replacement = replacedLine->mid(
        expected.valueStart, replacementLength);
    QTextCursor edit(guardedDocument);
    edit.beginEditBlock();
    edit.setPosition(targetBlock.position() + expected.valueStart);
    edit.setPosition(
        targetBlock.position() + expected.valueStart + expected.valueLength,
        QTextCursor::KeepAnchor);
    edit.insertText(replacement);
    edit.endEditBlock();

    guardedEditor->setTextCursor(originalCursor);
    guardedEditor->verticalScrollBar()->setValue(originalVerticalScroll);
    guardedEditor->horizontalScrollBar()->setValue(originalHorizontalScroll);
    guardedEditor->setFocus(Qt::OtherFocusReason);
}

QPoint CogScriptCodeEditor::inlineAssignmentActivationAnchor() const
{
    if ( inlineAssignmentActivationAnchor_ )
    {
        return *inlineAssignmentActivationAnchor_;
    }
    if ( inlineAssignmentButton_ )
    {
        return inlineAssignmentButton_->mapToGlobal(
            inlineAssignmentButton_->rect().center());
    }
    return viewport()->mapToGlobal(viewport()->rect().center());
}

void CogScriptCodeEditor::keyPressEvent(QKeyEvent* event)
{
    if (completer_ && completer_->popup() && completer_->popup()->isVisible())
    {
        switch (event->key())
        {
            case Qt::Key_Enter:
            case Qt::Key_Return:
            case Qt::Key_Escape:
            case Qt::Key_Tab:
            case Qt::Key_Backtab:
                event->ignore();
                return;
            default:
                break;
        }
    }

    if (event->key() == Qt::Key_Tab && !event->modifiers().testFlag(Qt::ShiftModifier))
    {
        textCursor().insertText(useTabs_ ? QStringLiteral("\t") : indentString(1, false, indentWidth_));
        return;
    }

    if (event->text() == QStringLiteral("}")
        && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)))
    {
        QTextCursor cursor = textCursor();
        if (!cursor.hasSelection())
        {
            const QTextBlock block = cursor.block();
            const int offset = cursor.position() - block.position();
            const QString prefix = block.text().left(std::max(0, offset));
            if (prefix.trimmed().isEmpty() && isCodeSectionAt(document(), block))
            {
                if (const std::optional<QString> matchingIndent = matchingOpenBraceIndentBefore(cursor))
                {
                    cursor.beginEditBlock();
                    cursor.setPosition(block.position());
                    cursor.setPosition(block.position() + std::max(0, offset), QTextCursor::KeepAnchor);
                    cursor.insertText(*matchingIndent + QStringLiteral("}"));
                    cursor.endEditBlock();
                    setTextCursor(cursor);
                    ensureCursorVisible();
                    event->accept();
                    return;
                }
            }
        }
    }

    if ((event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier | Qt::ShiftModifier)))
    {
        QTextCursor cursor = textCursor();
        cursor.beginEditBlock();
        cursor.removeSelectedText();

        // Enter on a whitespace-prefixed closing-brace line first aligns that
        // brace with its matching scope opener, then inserts the newline. Keep
        // both edits in one undo unit so interactive formatting is atomic.
        {
            const QTextBlock currentBlock = cursor.block();
            const QString currentText = currentBlock.text();
            const int currentOffset = cursor.position() - currentBlock.position();
            int closingOffset = 0;
            while (closingOffset < currentText.size()
                && (currentText.at(closingOffset) == QLatin1Char(' ')
                    || currentText.at(closingOffset) == QLatin1Char('\t')))
            {
                ++closingOffset;
            }
            const bool onlyClosingBrace = closingOffset < currentText.size()
                && currentText.at(closingOffset) == QLatin1Char('}')
                && currentText.mid(closingOffset + 1).trimmed().isEmpty();
            if (onlyClosingBrace && isCodeSectionAt(document(), currentBlock))
            {
                QTextCursor probe(document());
                probe.setPosition(currentBlock.position() + closingOffset);
                if (const std::optional<QString> matchingIndent = matchingOpenBraceIndentBefore(probe))
                {
                    const bool cursorAfterBrace = currentOffset > closingOffset;
                    const int tailOffset = cursorAfterBrace
                        ? currentOffset - closingOffset
                        : 0;
                    cursor.setPosition(currentBlock.position());
                    cursor.setPosition(
                        currentBlock.position() + closingOffset,
                        QTextCursor::KeepAnchor);
                    cursor.insertText(*matchingIndent);
                    cursor.setPosition(
                        currentBlock.position() + matchingIndent->size()
                            + tailOffset);
                }
            }
        }

        const QTextBlock block = cursor.block();
        const int offset = cursor.position() - block.position();
        const QString blockText = block.text();
        const QString prefix = blockText.left(std::max(0, offset));
        const QString suffix = blockText.mid(std::max(0, offset));
        int suffixWhitespace = 0;
        while (suffixWhitespace < suffix.size() && suffix.at(suffixWhitespace).isSpace())
        {
            ++suffixWhitespace;
        }
        const bool splittingBeforeClosingBrace = suffixWhitespace < suffix.size()
            && suffix.at(suffixWhitespace) == QLatin1Char('}')
            && isCodeSectionAt(document(), block);

        QString indent = automaticNewlineIndent(cursor, useTabs_, indentWidth_);
        if (splittingBeforeClosingBrace)
        {
            if (const std::optional<QString> matchingIndent = matchingOpenBraceIndentBefore(cursor))
            {
                indent = *matchingIndent;
                if (suffixWhitespace > 0)
                {
                    cursor.setPosition(cursor.position() + suffixWhitespace, QTextCursor::KeepAnchor);
                    cursor.removeSelectedText();
                }
            }
        }
        cursor.insertText(QStringLiteral("\n") + indent);
        cursor.endEditBlock();
        setTextCursor(cursor);
        ensureCursorVisible();
        event->accept();
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

void CogScriptCodeEditor::updateLineNumberAreaWidth()
{
    setViewportMargins(
        lineNumberAreaWidth(), 0, 0, bottomAccessoryHeight_);
}

void CogScriptCodeEditor::updateLineNumberArea(const QRect& rect, const int dy)
{
    if (dy)
    {
        lineNumberArea_->scroll(0, dy);
    }
    else
    {
        lineNumberArea_->update(0, rect.y(), lineNumberArea_->width(), rect.height());
    }
    if (rect.contains(viewport()->rect()))
    {
        updateLineNumberAreaWidth();
    }
}

QString CogScriptCodeEditor::textUnderCursor() const
{
    QTextCursor cursor = textCursor();
    cursor.select(QTextCursor::WordUnderCursor);
    return cursor.selectedText();
}

void CogScriptCodeEditor::insertCompletion(const QString& completion)
{
    if (!completer_ || completer_->widget() != this)
    {
        return;
    }
    QTextCursor cursor = textCursor();
    const int extra = completion.length() - completer_->completionPrefix().length();
    cursor.movePosition(QTextCursor::Left);
    cursor.movePosition(QTextCursor::EndOfWord);
    cursor.insertText(completion.right(std::max(0, extra)));
    setTextCursor(cursor);
}

void CogScriptCodeEditor::refreshExtraSelections()
{
    QList<QTextEdit::ExtraSelection> selections = diagnosticSelections_;
    if (!isReadOnly())
    {
        QTextEdit::ExtraSelection selection{};
        selection.format.setBackground(theme_.currentLineBackground);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        selections.prepend(selection);
    }
    QPlainTextEdit::setExtraSelections(selections);
}

CogScriptSyntaxHighlighter::CogScriptSyntaxHighlighter(
    QTextDocument* document,
    QStringList messageWords,
    const CogScriptTheme& theme)
    : QSyntaxHighlighter(document)
    , messageWords_(std::move(messageWords))
{
    semanticRefreshTimer_ = new QTimer(this);
    semanticRefreshTimer_->setObjectName(QStringLiteral("cogSemanticRefreshTimer"));
    semanticRefreshTimer_->setSingleShot(true);
    semanticRefreshTimer_->setInterval(90);
    connect(semanticRefreshTimer_, &QTimer::timeout, this, [this]()
        {
            ++semanticRefreshFireCount_;
            if (semanticRefreshRunning_)
            {
                return;
            }
            semanticRefreshRunning_ = true;
            const bool changed = refreshSemanticNames();
            if (changed)
            {
                ++semanticRehighlightCount_;
                rehighlight();
            }
            semanticRefreshRunning_ = false;
        });
    (void)refreshSemanticNames();
    applyTheme(theme);
}

void CogScriptSyntaxHighlighter::applyTheme(const CogScriptTheme& theme)
{
    commentFormat_.setForeground(theme.commentForeground);
    labelFormat_.setForeground(theme.labelForeground);
    labelFormat_.setFontWeight(QFont::Bold);
    messageFormat_.setForeground(theme.messageForeground);
    functionFormat_.setForeground(theme.functionForeground);

    symbolsSectionFormat_.setForeground(theme.sectionForeground);
    symbolsSectionFormat_.setFontWeight(QFont::Bold);
    // Section terminators inherit the section they close. Code headers and
    // their `end` therefore remain distinct from message/handler labels.
    codeSectionFormat_.setForeground(theme.codeSectionForeground);
    codeSectionFormat_.setFontWeight(QFont::Bold);
    flagsFormat_.setForeground(theme.flagsForeground);
    flagsFormat_.setFontWeight(QFont::Bold);
    keywordFormat_.setForeground(theme.keywordForeground);
    typeFormat_.setForeground(theme.typeForeground);
    attributeFormat_.setForeground(theme.attributeForeground);
    globalFormat_.setForeground(theme.globalForeground);
    numberFormat_.setForeground(theme.numberForeground);
    operatorFormat_.setForeground(theme.operatorForeground);
    stringFormat_.setForeground(theme.stringForeground);
    symbolFormat_.setForeground(theme.symbolForeground);
    exposedSymbolFormat_.setForeground(theme.exposedSymbolForeground);
    braceFormats_[0].setForeground(theme.braceLevel1Foreground);
    braceFormats_[1].setForeground(theme.braceLevel2Foreground);
    braceFormats_[2].setForeground(theme.braceLevel3Foreground);
    ++themeRehighlightCount_;
    rehighlight();
}

CogScriptEditorSection CogScriptSyntaxHighlighter::sectionForBlock(
    const QTextBlock& block) noexcept
{
    if (!block.isValid())
    {
        return CogScriptEditorSection::outside;
    }
    const int state = block.userState();
    if (state < 0)
    {
        return CogScriptEditorSection::outside;
    }
    switch (state & 0x3)
    {
        case 1:
            return CogScriptEditorSection::symbols;
        case 2:
            return CogScriptEditorSection::code;
        default:
            return CogScriptEditorSection::outside;
    }
}

void CogScriptSyntaxHighlighter::refreshSemanticNamesNow()
{
    if (semanticRefreshTimer_)
    {
        semanticRefreshTimer_->stop();
    }
    semanticRefreshRunning_ = true;
    (void)refreshSemanticNames();
    ++semanticRehighlightCount_;
    rehighlight();
    semanticRefreshRunning_ = false;
}

bool CogScriptSyntaxHighlighter::refreshSemanticNames()
{
    ++semanticScanCount_;
    QSet<QString> messageNames{};
    QSet<QString> codeLabelNames{};
    QSet<QString> labelVariableNames{};
    QSet<QString> localSymbolNames{};
    QSet<QString> exposedSymbolNames{};
    QSet<int> semanticSourceBlockNumbers{};
    for (const QString& word : messageWords_)
    {
        messageNames.insert(word.toLower());
    }
    if (!document())
    {
        return false;
    }

    static const QRegularExpression kSymbolDeclarationRx(
        QStringLiteral(
            "^\\s*([A-Za-z_][A-Za-z0-9_]*)\\s+"
            "([A-Za-z_][A-Za-z0-9_]*)\\b(.*)$"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kLocalAttributeRx(
        QStringLiteral("(?:^|[\\s,])local(?=[\\s,]|$)"),
        QRegularExpression::CaseInsensitiveOption
            | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kCodeLabelRx(
        QStringLiteral("^\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*:"),
        QRegularExpression::UseUnicodePropertiesOption);
    enum class Section
    {
        none,
        symbols,
        code,
    };
    Section section = Section::none;
    QHash<QString, QString> declaredSymbolTypes{};
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next())
    {
        const QString analysis = sanitizeLineForAnalysis(block.text()).trimmed();
        if (analysis.compare(QStringLiteral("symbols"), Qt::CaseInsensitive) == 0)
        {
            semanticSourceBlockNumbers.insert(block.blockNumber());
            section = Section::symbols;
            continue;
        }
        if (analysis.compare(QStringLiteral("code"), Qt::CaseInsensitive) == 0)
        {
            semanticSourceBlockNumbers.insert(block.blockNumber());
            section = Section::code;
            continue;
        }
        if (analysis.compare(QStringLiteral("end"), Qt::CaseInsensitive) == 0)
        {
            semanticSourceBlockNumbers.insert(block.blockNumber());
            section = Section::none;
            continue;
        }
        if (section == Section::symbols)
        {
            const QRegularExpressionMatch match =
                kSymbolDeclarationRx.match(analysis);
            if (match.hasMatch())
            {
                semanticSourceBlockNumbers.insert(block.blockNumber());
                const QString typeName = match.captured(1).toLower();
                const QString name = match.captured(2).toLower();
                declaredSymbolTypes.insert(name, typeName);
                if (typeName == QStringLiteral("message"))
                {
                    messageNames.insert(name);
                }
                else if (kLocalAttributeRx.match(match.captured(3)).hasMatch())
                {
                    localSymbolNames.insert(name);
                }
                else
                {
                    exposedSymbolNames.insert(name);
                }
            }
        }
        else if (section == Section::code)
        {
            if (const QRegularExpressionMatch match = kCodeLabelRx.match(analysis);
                match.hasMatch())
            {
                semanticSourceBlockNumbers.insert(block.blockNumber());
                codeLabelNames.insert(match.captured(1).toLower());
            }
        }
    }

    // A symbol whose name is also a code label is a label variable. Keep its
    // declaration, references, and destination label visually identical,
    // regardless of whether the declaration is local or non-local.
    for (const QString& labelName : codeLabelNames)
    {
        if (!messageNames.contains(labelName)
            && cogScriptTypeSupportsCodeLabel(
                declaredSymbolTypes.value(labelName)))
        {
            labelVariableNames.insert(labelName);
            localSymbolNames.remove(labelName);
            exposedSymbolNames.remove(labelName);
        }
    }

    const bool changed = messageNames != messageNames_
        || codeLabelNames != codeLabelNames_
        || labelVariableNames != labelVariableNames_
        || localSymbolNames != localSymbolNames_
        || exposedSymbolNames != exposedSymbolNames_;
    messageNames_ = std::move(messageNames);
    codeLabelNames_ = std::move(codeLabelNames);
    labelVariableNames_ = std::move(labelVariableNames);
    localSymbolNames_ = std::move(localSymbolNames);
    exposedSymbolNames_ = std::move(exposedSymbolNames);
    semanticSourceBlockNumbers_ = std::move(semanticSourceBlockNumbers);
    semanticDocumentBlockCount_ = document()->blockCount();
    return changed;
}

void CogScriptSyntaxHighlighter::noteDocumentEdit(
    const int editedBlockNumber,
    const bool fullDocumentReplacement)
{
    if (semanticRefreshRunning_ || !semanticRefreshTimer_ || !document())
    {
        return;
    }

    ++semanticBlockCheckCount_;
    const QTextBlock block = document()->findBlockByNumber(editedBlockNumber);
    lastSemanticClassifiedBlock_ = block.blockNumber();
    lastSemanticBlockCountChanged_ = fullDocumentReplacement
        || document()->blockCount() != semanticDocumentBlockCount_;
    lastSemanticBlockTracked_ = block.isValid()
        && semanticSourceBlockNumbers_.contains(block.blockNumber());
    static const QRegularExpression kPotentialSemanticLineRx(
        QStringLiteral(
            "^(?:symbols|code|end)\\s*$|"
            "^[A-Za-z_][A-Za-z0-9_]*\\s+[A-Za-z_][A-Za-z0-9_]*\\b|"
            "^[A-Za-z_][A-Za-z0-9_]*\\s*:"),
        QRegularExpression::CaseInsensitiveOption
            | QRegularExpression::UseUnicodePropertiesOption);
    const auto looksSemantic = [](const QTextBlock& candidate)
    {
        return candidate.isValid()
            && kPotentialSemanticLineRx.match(
                sanitizeLineForAnalysis(candidate.text()).trimmed()).hasMatch();
    };
    lastSemanticBlockLooksSemantic_ = looksSemantic(block)
        || looksSemantic(block.previous())
        || looksSemantic(block.next());
    const bool semanticChange = !block.isValid()
        || lastSemanticBlockCountChanged_
        || lastSemanticBlockTracked_
        || lastSemanticBlockLooksSemantic_;
    if (semanticChange)
    {
        ++semanticBlockPositiveCount_;
        ++semanticRefreshStartCount_;
        semanticRefreshTimer_->start();
    }
}

void CogScriptSyntaxHighlighter::highlightBlock(const QString& text)
{
    const int commentStart = findCommentStart(text);
    const QString code = (commentStart >= 0) ? text.left(commentStart) : text;

    // Step 1: establish operator and punctuation color first. More specific
    // number/string/comment/brace formats below intentionally win on overlap.
    static const QRegularExpression kOperatorRx(
        QStringLiteral(
            "(?:>>=|<<=|\\+\\+|--|&&|\\|\\||==|!=|<=|>=|"
            "\\+=|-=|\\*=|/=|%=|&=|\\|=|\\^=|<<|>>|"
            "[!~+\\-*/%<>=&|^;,\\.()])"),
        QRegularExpression::UseUnicodePropertiesOption);
    QRegularExpressionMatchIterator operators = kOperatorRx.globalMatch(code);
    while (operators.hasNext())
    {
        const QRegularExpressionMatch match = operators.next();
        setFormat(match.capturedStart(), match.capturedLength(), operatorFormat_);
    }

    // Step 2: classify every identifier once. The earlier implementation
    // built eight regular expressions for every source block, including
    // expressions containing every symbol/label name in the document. On a
    // large cutscene that multiplied regex compilation by thousands of lines.
    // Stable vocabulary and semantic names are sets, so one token scan keeps
    // the same override order with linear per-block work.
    const auto makeWordSet = [](const QStringList& words)
    {
        QSet<QString> result{};
        result.reserve(words.size());
        for (const QString& word : words)
        {
            result.insert(word.toLower());
        }
        return result;
    };
    static const QSet<QString> kTypeSet = makeWordSet(cogScriptTypeWords());
    static const QSet<QString> kAttributeSet = makeWordSet(cogScriptAttributeWords());
    static const QSet<QString> kGlobalSet = makeWordSet(cogScriptGlobalWords());
    static const QSet<QString> kKeywordSet = makeWordSet(cogScriptKeywordWords());
    static const QRegularExpression kIdentifierRx(
        QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]*\\b"),
        QRegularExpression::UseUnicodePropertiesOption);
    QRegularExpressionMatchIterator identifiers = kIdentifierRx.globalMatch(code);
    while (identifiers.hasNext())
    {
        const QRegularExpressionMatch match = identifiers.next();
        const QString word = match.captured().toLower();
        const QTextCharFormat* format = nullptr;
        if (kTypeSet.contains(word))
        {
            format = &typeFormat_;
        }
        if (kAttributeSet.contains(word))
        {
            format = &attributeFormat_;
        }
        if (localSymbolNames_.contains(word))
        {
            format = &symbolFormat_;
        }
        if (exposedSymbolNames_.contains(word))
        {
            format = &exposedSymbolFormat_;
        }
        if (kGlobalSet.contains(word))
        {
            format = &globalFormat_;
        }
        if (codeLabelNames_.contains(word))
        {
            format = &labelFormat_;
        }
        if (messageNames_.contains(word))
        {
            format = &messageFormat_;
        }
        if (kKeywordSet.contains(word))
        {
            format = &keywordFormat_;
        }
        if (format)
        {
            setFormat(match.capturedStart(), match.capturedLength(), *format);
        }
    }

    // Calls use their own color even when a function happens to share a name
    // with a message. Reserved control words are deliberately excluded so
    // `if` remains stable with and without a parenthesized condition.
    static const QRegularExpression kFunctionRx(
        QStringLiteral("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*\\("),
        QRegularExpression::UseUnicodePropertiesOption);
    QRegularExpressionMatchIterator functions = kFunctionRx.globalMatch(code);
    while (functions.hasNext())
    {
        const QRegularExpressionMatch match = functions.next();
        if (!kKeywordSet.contains(match.captured(1).toLower()))
        {
            setFormat(
                match.capturedStart(1),
                match.capturedLength(1),
                functionFormat_);
        }
    }

    // Step 3: highlight labels by syntax shape.
    static const QRegularExpression kLabelRx(
        QStringLiteral("^\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*:"),
        QRegularExpression::UseUnicodePropertiesOption);
    if (const auto match = kLabelRx.match(code); match.hasMatch())
    {
        const QTextCharFormat& format = messageNames_.contains(match.captured(1).toLower())
            ? messageFormat_
            : labelFormat_;
        setFormat(match.capturedStart(1), match.capturedLength(1), format);
    }

    // Step 4: section headers and their closing `end` use stateful colors.
    // The same block state carries code brace depth so matching braces retain
    // their three-color nesting cycle after incremental edits.
    constexpr int kOutsideSection = 0;
    constexpr int kSymbolsSection = 1;
    constexpr int kCodeSection = 2;
    constexpr int kSectionMask = 0x3;
    const int previousState = std::max(0, previousBlockState());
    int activeSection = previousState & kSectionMask;
    int braceDepth = std::max(0, previousState >> 2);
    if (activeSection < kOutsideSection || activeSection > kCodeSection)
    {
        activeSection = kOutsideSection;
    }
    static const QRegularExpression kSectionLineRx(
        QStringLiteral("^\\s*(symbols|code|end)\\s*$"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    if (const QRegularExpressionMatch section = kSectionLineRx.match(code); section.hasMatch())
    {
        const QString word = section.captured(1).toLower();
        const QTextCharFormat* format = &symbolsSectionFormat_;
        if (word == QStringLiteral("code")
            || (word == QStringLiteral("end") && activeSection == kCodeSection))
        {
            format = &codeSectionFormat_;
        }
        setFormat(section.capturedStart(1), section.capturedLength(1), *format);

        if (word == QStringLiteral("symbols"))
        {
            activeSection = kSymbolsSection;
            braceDepth = 0;
        }
        else if (word == QStringLiteral("code"))
        {
            activeSection = kCodeSection;
            braceDepth = 0;
        }
        else if (word == QStringLiteral("end"))
        {
            activeSection = kOutsideSection;
            braceDepth = 0;
        }
    }

    // `flags=<hexadecimal>` is an optional standalone preamble rather than a
    // stateful symbols/code section. Give its leading token a dedicated theme
    // role while retaining the ordinary number color for both prefixed and
    // engine-valid bare A-F values, and deliberately leave section state
    // unchanged.
    static const QRegularExpression kFlagsLineRx(
        QStringLiteral("^\\s*(flags)\\b\\s*=\\s*((?:0[xX])?[0-9A-Fa-f]+)\\s*$"),
        QRegularExpression::CaseInsensitiveOption
            | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kFlagsTokenRx(
        QStringLiteral("^\\s*(flags)\\b"),
        QRegularExpression::CaseInsensitiveOption
            | QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpressionMatch flagsLine = kFlagsLineRx.match(code);
    const QRegularExpressionMatch flags = flagsLine.hasMatch()
        ? flagsLine
        : kFlagsTokenRx.match(code);
    if (flags.hasMatch())
    {
        setFormat(flags.capturedStart(1), flags.capturedLength(1), flagsFormat_);
    }
    if (flagsLine.hasMatch())
    {
        setFormat(
            flagsLine.capturedStart(2),
            flagsLine.capturedLength(2),
            numberFormat_);
    }

    // Symbol declarations distinguish local from non-local parameters.
    // Message declarations and label variables retain their semantic colors.
    std::optional<SymbolInitializerSpan> symbolInitializer{};
    if (activeSection == kSymbolsSection)
    {
        static const QRegularExpression kSymbolDeclarationRx(
            QStringLiteral("^\\s*([A-Za-z_][A-Za-z0-9_]*)\\s+([A-Za-z_][A-Za-z0-9_]*)\\b"),
            QRegularExpression::UseUnicodePropertiesOption);
        const QRegularExpressionMatch symbol = kSymbolDeclarationRx.match(code);
        if (symbol.hasMatch())
        {
            const QString symbolName = symbol.captured(2).toLower();
            const QTextCharFormat& format =
                symbol.captured(1).compare(
                    QStringLiteral("message"), Qt::CaseInsensitive) == 0
                ? messageFormat_
                : (labelVariableNames_.contains(symbolName)
                        ? labelFormat_
                        : (localSymbolNames_.contains(symbolName)
                                ? symbolFormat_
                                : exposedSymbolFormat_));
            setFormat(symbol.capturedStart(2), symbol.capturedLength(2), format);
            symbolInitializer = symbolInitializerSpan(
                code, symbol.capturedEnd(), kAttributeSet);
        }
    }

    static const QRegularExpression kNumberRx(
        QStringLiteral("(?<![A-Za-z0-9_])(?:0[xX][0-9A-Fa-f]+|(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][+-]?\\d+)?)(?![A-Za-z0-9_])"),
        QRegularExpression::UseUnicodePropertiesOption);
    QRegularExpressionMatchIterator numbers = kNumberRx.globalMatch(code);
    while (numbers.hasNext())
    {
        const QRegularExpressionMatch number = numbers.next();
        setFormat(number.capturedStart(), number.capturedLength(), numberFormat_);
    }

    if (symbolInitializer && !symbolInitializer->numeric)
    {
        // Symbol-section defaults are literal configuration values, even
        // when their spelling matches a declared name, an attribute, or a
        // number fragment inside a resource filename. Keep complete numeric
        // defaults on the Number role; the quoted-string pass below also
        // retains String for quoted values.
        setFormat(
            symbolInitializer->start,
            symbolInitializer->length,
            stringFormat_);
    }

    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool escaped = false;
    int stringStart = -1;
    for (int i = 0; i < code.size(); ++i)
    {
        const QChar ch = code.at(i);
        if (escaped)
        {
            escaped = false;
            continue;
        }
        if ((inSingleQuote || inDoubleQuote) && ch == QLatin1Char('\\'))
        {
            escaped = true;
            continue;
        }
        if (!inDoubleQuote && ch == QLatin1Char('\''))
        {
            if (!inSingleQuote)
            {
                stringStart = i;
                inSingleQuote = true;
            }
            else
            {
                setFormat(stringStart, i - stringStart + 1, stringFormat_);
                inSingleQuote = false;
                stringStart = -1;
            }
            continue;
        }
        if (!inSingleQuote && ch == QLatin1Char('"'))
        {
            if (!inDoubleQuote)
            {
                stringStart = i;
                inDoubleQuote = true;
            }
            else
            {
                setFormat(stringStart, i - stringStart + 1, stringFormat_);
                inDoubleQuote = false;
                stringStart = -1;
            }
        }
    }
    if (stringStart >= 0)
    {
        setFormat(stringStart, code.size() - stringStart, stringFormat_);
    }

    if (activeSection == kCodeSection && !kSectionLineRx.match(code).hasMatch())
    {
        inSingleQuote = false;
        inDoubleQuote = false;
        escaped = false;
        for (int i = 0; i < code.size(); ++i)
        {
            const QChar ch = code.at(i);
            if (escaped)
            {
                escaped = false;
                continue;
            }
            if ((inSingleQuote || inDoubleQuote) && ch == QLatin1Char('\\'))
            {
                escaped = true;
                continue;
            }
            if (!inDoubleQuote && ch == QLatin1Char('\''))
            {
                inSingleQuote = !inSingleQuote;
                continue;
            }
            if (!inSingleQuote && ch == QLatin1Char('"'))
            {
                inDoubleQuote = !inDoubleQuote;
                continue;
            }
            if (inSingleQuote || inDoubleQuote)
            {
                continue;
            }
            if (ch == QLatin1Char('{'))
            {
                setFormat(i, 1, braceFormats_[static_cast<std::size_t>(braceDepth % 3)]);
                braceDepth = std::min(braceDepth + 1, 8191);
            }
            else if (ch == QLatin1Char('}'))
            {
                braceDepth = std::max(0, braceDepth - 1);
                setFormat(i, 1, braceFormats_[static_cast<std::size_t>(braceDepth % 3)]);
            }
        }
    }
    setCurrentBlockState(activeSection | (braceDepth << 2));

    // Step 5: comments win over everything to their right.
    if (commentStart >= 0)
    {
        setFormat(commentStart, text.size() - commentStart, commentFormat_);
    }
}

} // namespace sed::ui::cog
