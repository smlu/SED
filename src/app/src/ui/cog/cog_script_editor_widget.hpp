#pragma once

#include "ui/cog/cog_script_diagnostics_utils.hpp"
#include "ui/cog/cog_script_assignment_utils.hpp"
#include "ui/cog/cog_script_theme_utils.hpp"

#include <functional>
#include <optional>
#include <vector>

#include <QFont>
#include <QList>
#include <QPlainTextEdit>
#include <QPoint>
#include <QSettings>
#include <QSet>
#include <QStringView>
#include <QStringList>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextEdit>

class QCompleter;
class QEvent;
class QKeyEvent;
class QMouseEvent;
class QObject;
class QPaintEvent;
class QPoint;
class QStringListModel;
class QTimer;
class QToolButton;

namespace sed::ui::cog
{

class CogScriptLineNumberArea;

/** Resolves the monospace font used by COG editor widgets from app settings. */
QFont resolveCogScriptEditorFont(QSettings& settings);

/** Default ordered font-family fallback list shown in Preferences. */
QString defaultCogScriptEditorFontFamilyList();

/** Installed font families, obtained through a Qt 5/6 compatible API. */
QStringList availableCogScriptEditorFontFamilies();

/** Builds a COG code-editor font from user-facing family/style controls. */
QFont makeCogScriptEditorFont(
    const QString& familyList,
    int pointSize,
    bool bold,
    bool italic);

/** Clamps an editor zoom percentage to the supported presentation range. */
int clampCogScriptEditorZoomPercent(int percent) noexcept;

/** Parses an editable zoom field such as `125` or `125%`. */
std::optional<int> parseCogScriptEditorZoomPercent(QStringView text);

/** Applies a non-cumulative presentation zoom to a configured base font. */
QFont cogScriptEditorFontAtZoom(const QFont& baseFont, int percent);

/**
 * Plain-text COG script editor with line numbers, diagnostics, whitespace
 * visibility, indentation handling, and symbol completion.
 */
class CogScriptCodeEditor final : public QPlainTextEdit
{
public:
    using InlineResourcePickerCallback = std::function<std::optional<QString>(
        ResourcePickerKind,
        const QString&)>;
    using InlineFlagsEditorCallback = std::function<void()>;
    using InlineMaskEditorCallback = std::function<void(
        const CogScriptInlineAssignmentSpan&,
        int,
        int)>;

    explicit CogScriptCodeEditor(QWidget* parent = nullptr);

    void setIndentationStyle(bool useTabs, int indentWidth);
    void setMessageWords(QStringList words);
    void applyTheme(const CogScriptTheme& theme);
    void setWhitespaceVisible(bool visible);
    /** Reserve an internal bottom strip for a compact editor accessory. */
    void setBottomAccessoryHeight(int height);
    /** Runs after QPlainTextEdit finalizes viewport/scrollbar resize. */
    void setResizeLayoutCompletedCallback(std::function<void()> callback);
    /** Configures resource and flags actions for the transient inline assignment helper. */
    void setInlineAssignmentCallbacks(
        InlineResourcePickerCallback resourcePicker,
        InlineFlagsEditorCallback flagsEditor,
        InlineMaskEditorCallback maskEditor = {});
    QPoint inlineAssignmentActivationAnchor() const;
    void triggerCompletion();
    void setDiagnosticSelections(const QList<QTextEdit::ExtraSelection>& selections);
    void setDiagnosticEntries(const std::vector<CogScriptDiagnostic>& diagnostics);

    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent* event);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void updateInlineAssignmentAtPosition(const QPoint& viewportPosition);
    void updateInlineAssignmentButtonGeometry();
    void scheduleInlineAssignmentHide();
    void clearInlineAssignmentButton();
    void activateInlineAssignmentPicker();
    void updateLineNumberAreaWidth();
    void updateLineNumberArea(const QRect& rect, int dy);
    QString textUnderCursor() const;
    void insertCompletion(const QString& completion);
    void refreshExtraSelections();

    CogScriptLineNumberArea* lineNumberArea_{};
    QStringListModel* completionModel_{};
    QCompleter* completer_{};
    QStringList messageWords_{};
    QList<QTextEdit::ExtraSelection> diagnosticSelections_{};
    std::vector<CogScriptDiagnostic> diagnosticEntries_{};
    CogScriptTheme theme_{};
    bool whitespaceVisible_{false};
    bool useTabs_{false};
    int indentWidth_{4};
    int bottomAccessoryHeight_{};
    std::function<void()> resizeLayoutCompletedCallback_{};
    InlineResourcePickerCallback inlineResourcePickerCallback_{};
    InlineFlagsEditorCallback inlineFlagsEditorCallback_{};
    InlineMaskEditorCallback inlineMaskEditorCallback_{};
    QToolButton* inlineAssignmentButton_{};
    QTimer* inlineAssignmentHideTimer_{};
    std::optional<CogScriptInlineAssignmentSpan> inlineAssignmentSpan_{};
    std::optional<QPoint> inlineAssignmentEventGlobalPosition_{};
    std::optional<QPoint> inlineAssignmentActivationAnchor_{};
    int inlineAssignmentBlockNumber_{-1};
    int inlineAssignmentRevision_{-1};
    QString inlineAssignmentLineText_{};
};

/** Syntax highlighter for COG section headers, symbols, messages, and comments. */
class CogScriptSyntaxHighlighter final : public QSyntaxHighlighter
{
public:
    CogScriptSyntaxHighlighter(QTextDocument* document, QStringList messageWords, const CogScriptTheme& theme);

    void applyTheme(const CogScriptTheme& theme);
    /** Reads the active COG section cached in one QTextBlock's highlighter state. */
    static CogScriptEditorSection sectionForBlock(const QTextBlock& block) noexcept;
    /** Resynchronizes semantic names after a document replacement with signals blocked. */
    void refreshSemanticNamesNow();
    /** Classifies one stable editor text change without scanning the document. */
    void noteDocumentEdit(int editedBlockNumber, bool fullDocumentReplacement = false);
    int semanticScanCountForTesting() const noexcept { return semanticScanCount_; }
    int semanticRehighlightCountForTesting() const noexcept { return semanticRehighlightCount_; }
    int themeRehighlightCountForTesting() const noexcept { return themeRehighlightCount_; }
    int semanticBlockCheckCountForTesting() const noexcept
    {
        return semanticBlockCheckCount_;
    }
    int semanticBlockPositiveCountForTesting() const noexcept
    {
        return semanticBlockPositiveCount_;
    }
    int semanticRefreshStartCountForTesting() const noexcept
    {
        return semanticRefreshStartCount_;
    }
    int semanticRefreshFireCountForTesting() const noexcept
    {
        return semanticRefreshFireCount_;
    }
    int lastSemanticClassifiedBlockForTesting() const noexcept
    {
        return lastSemanticClassifiedBlock_;
    }
    int semanticSourceBlockCountForTesting() const noexcept
    {
        return semanticSourceBlockNumbers_.size();
    }
    bool semanticSourceTracksBlockForTesting(int blockNumber) const noexcept
    {
        return semanticSourceBlockNumbers_.contains(blockNumber);
    }
    bool lastSemanticBlockTrackedForTesting() const noexcept
    {
        return lastSemanticBlockTracked_;
    }
    bool lastSemanticBlockLooksSemanticForTesting() const noexcept
    {
        return lastSemanticBlockLooksSemantic_;
    }
    bool lastSemanticBlockCountChangedForTesting() const noexcept
    {
        return lastSemanticBlockCountChanged_;
    }

protected:
    void highlightBlock(const QString& text) override;

private:
    bool refreshSemanticNames();

    QStringList messageWords_{};
    QSet<QString> messageNames_{};
    QSet<QString> codeLabelNames_{};
    QSet<QString> labelVariableNames_{};
    QSet<QString> localSymbolNames_{};
    QSet<QString> exposedSymbolNames_{};
    // Cache the declaration/label block numbers produced by the last complete
    // scan. The owning code editor reports its stable cursor block after
    // textChanged, avoiding QTextDocument signal/highlighter timing assumptions.
    QSet<int> semanticSourceBlockNumbers_{};
    QTimer* semanticRefreshTimer_{};
    int semanticDocumentBlockCount_{};
    int semanticScanCount_{};
    int semanticRehighlightCount_{};
    int themeRehighlightCount_{};
    int semanticBlockCheckCount_{};
    int semanticBlockPositiveCount_{};
    int semanticRefreshStartCount_{};
    int semanticRefreshFireCount_{};
    int lastSemanticClassifiedBlock_{-1};
    bool lastSemanticBlockTracked_{};
    bool lastSemanticBlockLooksSemantic_{};
    bool lastSemanticBlockCountChanged_{};
    bool semanticRefreshRunning_{};
    QTextCharFormat commentFormat_{};
    QTextCharFormat labelFormat_{};
    QTextCharFormat messageFormat_{};
    QTextCharFormat functionFormat_{};
    QTextCharFormat symbolsSectionFormat_{};
    QTextCharFormat codeSectionFormat_{};
    QTextCharFormat flagsFormat_{};
    QTextCharFormat keywordFormat_{};
    QTextCharFormat typeFormat_{};
    QTextCharFormat attributeFormat_{};
    QTextCharFormat globalFormat_{};
    QTextCharFormat numberFormat_{};
    QTextCharFormat operatorFormat_{};
    QTextCharFormat stringFormat_{};
    QTextCharFormat symbolFormat_{};
    QTextCharFormat exposedSymbolFormat_{};
    std::array<QTextCharFormat, 3> braceFormats_{};
};

} // namespace sed::ui::cog
