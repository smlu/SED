#include "ui/cog/cog_script_diagnostics_utils.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QVector>

#include "ui/cog/cog_script_format_utils.hpp"
#include "ui/cog/cog_script_language_utils.hpp"
#include "ui/cog/cog_script_text_utils.hpp"

namespace sed::ui::cog
{


namespace
{

int cogLeadingWhitespaceCount(const QString& line)
{
    int out = 0;
    while (out < line.size() && line.at(out).isSpace())
    {
        ++out;
    }
    return out;
}

struct PendingCogStatement
{
    int line{-1};
    int column{-1};
    int length{1};
    bool expectsContinuation{false};
};

bool cogAnalysisContainsBlockCommentDelimiter(const QStringView analysis)
{
    for (int column = 0; column + 1 < analysis.size(); ++column)
    {
        const QChar first = analysis.at(column);
        const QChar second = analysis.at(column + 1);
        if ((first == QLatin1Char('/') && second == QLatin1Char('*'))
            || (first == QLatin1Char('*') && second == QLatin1Char('/')))
        {
            return true;
        }
    }
    return false;
}

bool isEngineAllTypesMaskSpelling(const QString& authoredValue)
{
    const QString trimmed = authoredValue.trimmed();
    if ( trimmed == QStringLiteral("-1") )
    {
        return true;
    }

    // The engine reads mask values with sscanf("%x"), so these prefixed
    // hexadecimal spellings reach the dispatch layer as the all-types bits.
    // Keep the authored token in place and leave the editor's legacy bare
    // decimal policy unchanged.
    if ( !trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive) )
    {
        return false;
    }

    const std::optional<std::uint32_t> parsed = parseMaskValue(trimmed);
    return parsed && (*parsed == 0xFFFFu
        || *parsed == std::numeric_limits<std::uint32_t>::max());
}

bool cogLineEndsWithContinuation(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
    {
        return false;
    }

    static const QString kContinuationTails = QStringLiteral("+-*/%&|^=!<>?:,.\\([{");
    return kContinuationTails.contains(trimmed.back());
}

bool cogLineBeginsWithContinuation(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
    {
        return false;
    }

    static const QString kContinuationHeads = QStringLiteral("+-*/%&|^=<>?:,.)]");
    return kContinuationHeads.contains(trimmed.front());
}

int lastTopLevelCogStatementTerminator(const QString& value)
{
    int parenthesisDepth = 0;
    int bracketDepth = 0;
    int last = -1;
    for (int i = 0; i < value.size(); ++i)
    {
        const QChar ch = value.at(i);
        if (ch == QLatin1Char('('))
        {
            ++parenthesisDepth;
        }
        else if (ch == QLatin1Char(')'))
        {
            parenthesisDepth = std::max(0, parenthesisDepth - 1);
        }
        else if (ch == QLatin1Char('['))
        {
            ++bracketDepth;
        }
        else if (ch == QLatin1Char(']'))
        {
            bracketDepth = std::max(0, bracketDepth - 1);
        }
        else if (ch == QLatin1Char(';')
            && parenthesisDepth == 0 && bracketDepth == 0)
        {
            last = i;
        }
    }
    return last;
}

void appendGenericStatementTerminationDiagnostics(
    const QStringList& lines,
    std::vector<CogScriptDiagnostic>& out)
{
    static const QRegularExpression kSectionRx(
        QStringLiteral("^\\s*(symbols|code)\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kEndRx(
        QStringLiteral("^\\s*end\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kLabelRx(
        QStringLiteral("^\\s*[A-Za-z_][A-Za-z0-9_]*\\s*:\\s*$"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kStatementHeadRx(
        QStringLiteral("^\\s*(return|call|goto|stop|sleep|break|continue)\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kControlHeadRx(
        QStringLiteral("^\\s*(if|while|for|switch|else|do|case|default)\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kEmbeddedStatementRx(
        QStringLiteral("^(.+?)(?:\\s+)(return|call|goto|stop|sleep|break|continue)\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);

    std::optional<PendingCogStatement> pending{};
    bool inCode = false;
    int multilineControlParenDepth = 0;

    auto parenthesisDelta = [](const QString& value) -> int
    {
        int delta = 0;
        for (const QChar ch : value)
        {
            if (ch == QLatin1Char('('))
            {
                ++delta;
            }
            else if (ch == QLatin1Char(')'))
            {
                --delta;
            }
        }
        return delta;
    };

    auto emitPending = [&]()
    {
        if (!pending)
        {
            return;
        }
        out.push_back(CogScriptDiagnostic{
            .message = QStringLiteral("Statement appears to be missing a terminating ';'."),
            .line = pending->line,
            .error = true,
            .column = std::max(0, pending->column),
            .length = std::max(1, pending->length),
        });
        pending.reset();
    };

    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
    {
        const QString analysis = sanitizeLineForAnalysis(lines.at(lineIndex));
        // Exact unsupported-delimiter diagnostics are emitted by the main
        // pass. Do not reinterpret intended block-comment prose as COG
        // statements or structural punctuation and cascade unrelated errors.
        if (cogAnalysisContainsBlockCommentDelimiter(analysis))
        {
            continue;
        }
        const QString trimmed = analysis.trimmed();
        const int trimmedColumn = std::max(0, static_cast<int>(analysis.indexOf(trimmed)));
        if (trimmed.isEmpty())
        {
            continue;
        }

        if (const auto section = kSectionRx.match(trimmed); section.hasMatch())
        {
            if (inCode)
            {
                emitPending();
            }
            inCode = section.captured(1).compare(QStringLiteral("code"), Qt::CaseInsensitive) == 0;
            multilineControlParenDepth = 0;
            continue;
        }
        if (kEndRx.match(trimmed).hasMatch())
        {
            if (inCode)
            {
                emitPending();
            }
            inCode = false;
            multilineControlParenDepth = 0;
            continue;
        }
        if (!inCode)
        {
            continue;
        }

        if (multilineControlParenDepth > 0)
        {
            multilineControlParenDepth = std::max(
                0,
                multilineControlParenDepth + parenthesisDelta(trimmed));
            continue;
        }

        const bool labelOnly = kLabelRx.match(trimmed).hasMatch();
        const bool statementHead = kStatementHeadRx.match(trimmed).hasMatch();
        const bool controlHead = kControlHeadRx.match(trimmed).hasMatch();
        const bool startsClosingBrace = trimmed.startsWith(QLatin1Char('}'));
        if (pending)
        {
            const bool continues = !labelOnly
                && !statementHead
                && !controlHead
                && !startsClosingBrace
                && (pending->expectsContinuation || cogLineBeginsWithContinuation(trimmed));
            if (!continues)
            {
                emitPending();
            }
        }

        if (labelOnly)
        {
            continue;
        }

        if (controlHead)
        {
            multilineControlParenDepth = std::max(0, parenthesisDelta(trimmed));
        }

        // A new statement keyword cannot be appended to an unfinished
        // expression.  Control-flow bodies such as `if (x) return;` are a
        // supported exception and are handled by the dedicated validators.
        if (!controlHead)
        {
            if (const auto embedded = kEmbeddedStatementRx.match(trimmed); embedded.hasMatch())
            {
                QString prefix = embedded.captured(1);
                int last = prefix.size() - 1;
                while (last >= 0 && prefix.at(last).isSpace())
                {
                    --last;
                }
                if (last >= 0 && prefix.at(last) != QLatin1Char(';')
                    && prefix.at(last) != QLatin1Char('{')
                    && prefix.at(last) != QLatin1Char('}'))
                {
                    out.push_back(CogScriptDiagnostic{
                        .message = QStringLiteral("Statement before '%1' is missing a terminating ';'.")
                            .arg(embedded.captured(2).toLower()),
                        .line = lineIndex,
                        .error = true,
                        .column = trimmedColumn + std::max(0, last),
                        .length = 1,
                    });
                }
            }
        }

        // Only a top-level semicolon completes a statement.  In particular,
        // separators inside `for (...)` do not terminate its control header,
        // and code after a real terminator must be checked as a fresh tail
        // instead of being hidden by the earlier `;` (for example `a; b`).
        const int lastTerminator = lastTopLevelCogStatementTerminator(analysis);
        if (lastTerminator >= 0)
        {
            pending.reset();
        }

        if (pending)
        {
            pending->expectsContinuation = cogLineEndsWithContinuation(trimmed);
            continue;
        }

        // Examine code after the last block delimiter.  This catches compact
        // malformed forms such as `{a`, and after the last completed
        // statement, while leaving a standalone `{` or `a;` alone.
        int tailStart = lastTerminator >= 0 ? lastTerminator + 1 : 0;
        const int lastOpen = analysis.lastIndexOf(QLatin1Char('{'));
        const int lastClose = analysis.lastIndexOf(QLatin1Char('}'));
        const int lastDelimiter = std::max(lastOpen, lastClose);
        if (lastDelimiter >= 0)
        {
            tailStart = std::max(tailStart, lastDelimiter + 1);
        }
        const QString tail = analysis.mid(tailStart).trimmed();
        if (tail.isEmpty())
        {
            continue;
        }
        if (startsClosingBrace || tail.startsWith(QLatin1Char('}')))
        {
            continue;
        }
        // Dedicated statement/control validators own their whole-line forms.
        // Once a delimiter or terminator exposes an inline body/tail, however,
        // that tail must pass the same generic termination check.
        if (tailStart == 0 && (controlHead || statementHead))
        {
            continue;
        }

        const bool recognizedExpression = tail.contains(QLatin1Char('='))
            || tail.contains(QLatin1Char('('));
        if (recognizedExpression && !cogLineEndsWithContinuation(tail))
        {
            // Complete-looking assignment/call lines are covered by the
            // existing immediate semicolon diagnostic.  Only keep a delayed
            // record when the line explicitly continues, so `a =` and
            // multiline calls cannot disappear at a closing brace/section.
            continue;
        }

        int column = analysis.lastIndexOf(tail);
        int last = column + tail.size() - 1;
        while (last >= column && analysis.at(last).isSpace())
        {
            --last;
        }
        pending = PendingCogStatement{
            .line = lineIndex,
            .column = std::max(column, last),
            .length = 1,
            .expectsContinuation = cogLineEndsWithContinuation(tail),
        };
    }

    if (inCode)
    {
        emitPending();
    }
}

} // namespace

std::vector<CogScriptDiagnostic> buildCogScriptDiagnostics(
    const QString& text,
    const QStringList& knownMessages,
    const ProjectType projectType)
{
    enum class Section
    {
        none,
        symbols,
        code,
    };

    static const QSet<QString> kTypeWords = []()
    {
        QSet<QString> out{};
        for (const QString& word : cogScriptTypeWords())
        {
            out.insert(word.toLower());
        }
        return out;
    }();
    static const QSet<QString> kGlobalWords = []()
    {
        QSet<QString> out{};
        for (const QString& word : cogScriptGlobalWords())
        {
            out.insert(word.toLower());
        }
        return out;
    }();
    static const QSet<QString> kSectionWords = []()
    {
        QSet<QString> out{};
        for (const QString& word : cogScriptSectionWords())
        {
            out.insert(word.toLower());
        }
        return out;
    }();
    static const QSet<QString> kKeywordWords = []()
    {
        QSet<QString> out{};
        for (const QString& word : cogScriptKeywordWords())
        {
            out.insert(word.toLower());
        }
        return out;
    }();
    static const QSet<QString> kAttributeWords = []()
    {
        QSet<QString> out{};
        for (const QString& word : cogScriptAttributeWords())
        {
            out.insert(word.toLower());
        }
        return out;
    }();
    struct DeclaredSymbol
    {
        QString name{};
        QString typeName{};
        int line = -1;
    };
    static const QRegularExpression kFlagsStartRx(
        QStringLiteral("^\\s*flags\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kFlagsRx(
        QStringLiteral("^\\s*flags\\s*=\\s*((?:0x)?[0-9A-Fa-f]+)\\s*$"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kSectionRx(
        QStringLiteral("^\\s*(symbols|code)\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kEndRx(
        QStringLiteral("^\\s*end\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kSymbolRx(
        QStringLiteral("^\\s*([A-Za-z0-9_]+)\\s+([A-Za-z_][A-Za-z0-9_]*)\\b"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kSymbolWithAttrsRx(
        QStringLiteral("^\\s*([A-Za-z0-9_]+)\\s+([A-Za-z_][A-Za-z0-9_]*)(?:\\s*=\\s*(?:'[^']*'|\"[^\"]*\"|[^\\s#]+))?(.*)$"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kRedundantSymbolSemicolonRx(
        QStringLiteral("(;+)\\s*$"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kLabelRx(
        QStringLiteral("^\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*:"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kMalformedLabelRx(
        QStringLiteral("^\\s*(\\S+)\\s*:"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kEmptyLabelRx(
        QStringLiteral("^\\s*:"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kCallRx(
        QStringLiteral("\\bcall\\s+([A-Za-z_][A-Za-z0-9_]*)\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kGotoRx(
        QStringLiteral("\\bgoto\\s+([A-Za-z_][A-Za-z0-9_]*)\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kIdentifierRx(
        QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]*\\b"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kAttributeTokenRx(
        QStringLiteral("(^|[\\s,]+)(local|desc|mask|linkid|nolink)(?:\\s*=\\s*(?:'([^']*)'|\"([^\"]*)\"|([^\\s,#]*)))?(?=[\\s,]|$)"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kFirstTokenRx(
        QStringLiteral("^\\s*([A-Za-z_][A-Za-z0-9_]*|[0-9][A-Za-z0-9_]*|\\S)"),
        QRegularExpression::UseUnicodePropertiesOption);

    std::vector<CogScriptDiagnostic> out{};
    Section currentSection = Section::none;
    int currentSectionLine = -1;
    int currentSectionColumn = 0;
    int currentSectionLength = 0;
    bool sawSymbols = false;
    bool sawCode = false;
    bool sawFlags = false;
    QHash<QString, int> symbolLines{};
    QHash<QString, std::pair<int, int>> symbolSpans{};
    QHash<QString, std::pair<int, int>> symbolTypeSpans{};
    QHash<QString, int> labelLines{};
    QHash<QString, std::pair<int, int>> labelSpans{};
    QSet<QString> usedSymbols{};
    QSet<QString> declaredMessages{};
    QSet<QString> knownMessagesDeclaredWithWrongType{};
    QSet<QString> handledMessages{};
    QSet<QString> callableNames{};
    QSet<QString> arrayBaseSymbols{};
    std::vector<DeclaredSymbol> declaredSymbols{};
    QHash<QString, int> declaredSymbolIndices{};
    struct PendingCallRef
    {
        QString target{};
        int line{-1};
        int column{-1};
        int length{0};
    };
    std::vector<PendingCallRef> pendingCalls{};
    struct PendingGotoRef
    {
        QString target{};
        int line{-1};
        int column{-1};
        int length{0};
    };
    std::vector<PendingGotoRef> pendingGotos{};
    QSet<QString> knownMessageSet{};
    struct PendingParenRef
    {
        int line{-1};
        int column{-1};
    };
    QVector<PendingParenRef> pendingOpenParens{};
    struct PendingBraceRef
    {
        int line{-1};
        int column{-1};
    };
    QVector<PendingBraceRef> pendingOpenBraces{};
    struct PendingBracketRef
    {
        int line{-1};
        int column{-1};
    };
    QVector<PendingBracketRef> pendingOpenBrackets{};
    for (const QString& message : knownMessages)
    {
        knownMessageSet.insert(message.toLower());
    }

    auto lineContinuesStatement = [](const QString& value) -> bool
    {
        const QString trimmed = value.trimmed();
        if (trimmed.isEmpty())
        {
            return false;
        }

        if (trimmed.endsWith(QLatin1Char(','))
            || trimmed.endsWith(QLatin1Char('\\'))
            || trimmed.endsWith(QLatin1Char('('))
            || trimmed.endsWith(QLatin1Char('[')))
        {
            return true;
        }

        static const QString kOperatorTails = QStringLiteral("+-*/%&|^=!<>?:");
        if (!trimmed.isEmpty() && kOperatorTails.contains(trimmed.back()))
        {
            return true;
        }

        int parenDepth = 0;
        int bracketDepth = 0;
        bool inSingleQuote = false;
        bool inDoubleQuote = false;
        for (int i = 0; i < trimmed.size(); ++i)
        {
            const QChar ch = trimmed.at(i);
            const QChar prev = (i > 0) ? trimmed.at(i - 1) : QChar{};
            if (!inDoubleQuote && ch == QLatin1Char('\'') && prev != QLatin1Char('\\'))
            {
                inSingleQuote = !inSingleQuote;
                continue;
            }
            if (!inSingleQuote && ch == QLatin1Char('"') && prev != QLatin1Char('\\'))
            {
                inDoubleQuote = !inDoubleQuote;
                continue;
            }
            if (inSingleQuote || inDoubleQuote)
            {
                continue;
            }
            if (ch == QLatin1Char('('))
            {
                ++parenDepth;
            }
            else if (ch == QLatin1Char(')'))
            {
                parenDepth = std::max(0, parenDepth - 1);
            }
            else if (ch == QLatin1Char('['))
            {
                ++bracketDepth;
            }
            else if (ch == QLatin1Char(']'))
            {
                bracketDepth = std::max(0, bracketDepth - 1);
            }
        }

        return parenDepth > 0 || bracketDepth > 0;
    };

    auto hasInlineTrailingStatement = [](const QString& value, const QString& keyword) -> bool
    {
        QString trimmed = value.trimmed();
        if (!trimmed.startsWith(keyword, Qt::CaseInsensitive))
        {
            return false;
        }

        int pos = keyword.size();
        while (pos < trimmed.size() && trimmed.at(pos).isSpace())
        {
            ++pos;
        }
        if (pos >= trimmed.size() || trimmed.at(pos) != QLatin1Char('('))
        {
            return false;
        }

        int depth = 0;
        for (; pos < trimmed.size(); ++pos)
        {
            const QChar ch = trimmed.at(pos);
            if (ch == QLatin1Char('('))
            {
                ++depth;
            }
            else if (ch == QLatin1Char(')'))
            {
                --depth;
                if (depth == 0)
                {
                    ++pos;
                    break;
                }
            }
        }

        if (depth != 0)
        {
            return false;
        }

        const QString trailing = trimmed.mid(pos).trimmed();
        return !trailing.isEmpty() && !trailing.startsWith(QLatin1Char('{'));
    };

    auto hasInlineElseIfTrailingStatement = [&](const QString& value) -> bool
    {
        QString trimmed = value.trimmed();
        if (!trimmed.startsWith(QStringLiteral("else"), Qt::CaseInsensitive))
        {
            return false;
        }

        int pos = 4;
        while (pos < trimmed.size() && trimmed.at(pos).isSpace())
        {
            ++pos;
        }
        if (pos >= trimmed.size() || !trimmed.mid(pos).startsWith(QStringLiteral("if"), Qt::CaseInsensitive))
        {
            return false;
        }

        return hasInlineTrailingStatement(trimmed.mid(pos), QStringLiteral("if"));
    };

    auto elseStartsStandaloneBody = [](const QString& value) -> bool
    {
        QString trimmed = value.trimmed();
        if (!trimmed.startsWith(QStringLiteral("else"), Qt::CaseInsensitive))
        {
            return false;
        }

        int pos = 4;
        while (pos < trimmed.size() && trimmed.at(pos).isSpace())
        {
            ++pos;
        }
        if (pos >= trimmed.size())
        {
            return true;
        }
        if (trimmed.at(pos) == QLatin1Char('{'))
        {
            return true;
        }
        // Other tokens are inline bodies and are validated by the statement
        // parser below. A spaced `else if` is a supported conditional head.
        return false;
    };

    auto elseHasInlineStatement = [&](const QString& value) -> bool
    {
        QString trimmed = value.trimmed();
        if (!trimmed.startsWith(QStringLiteral("else"), Qt::CaseInsensitive))
        {
            return false;
        }

        int pos = 4;
        while (pos < trimmed.size() && trimmed.at(pos).isSpace())
        {
            ++pos;
        }
        if (pos >= trimmed.size())
        {
            return false;
        }
        if (trimmed.at(pos) == QLatin1Char('{'))
        {
            return false;
        }
        if (trimmed.mid(pos).startsWith(QStringLiteral("if"), Qt::CaseInsensitive))
        {
            return hasInlineElseIfTrailingStatement(trimmed);
        }

        return true;
    };

    auto findUnterminatedQuote = [](const QString& value) -> std::optional<std::pair<int, QChar>>
    {
        bool inSingleQuote = false;
        bool inDoubleQuote = false;
        int singleQuoteStart = -1;
        int doubleQuoteStart = -1;
        for (int i = 0; i < value.size(); ++i)
        {
            const QChar ch = value.at(i);
            const QChar prev = (i > 0) ? value.at(i - 1) : QChar{};
            if (!inDoubleQuote && ch == QLatin1Char('\'') && prev != QLatin1Char('\\'))
            {
                if (inSingleQuote)
                {
                    inSingleQuote = false;
                    singleQuoteStart = -1;
                }
                else
                {
                    inSingleQuote = true;
                    singleQuoteStart = i;
                }
                continue;
            }
            if (!inSingleQuote && ch == QLatin1Char('"') && prev != QLatin1Char('\\'))
            {
                if (inDoubleQuote)
                {
                    inDoubleQuote = false;
                    doubleQuoteStart = -1;
                }
                else
                {
                    inDoubleQuote = true;
                    doubleQuoteStart = i;
                }
            }
        }

        if (inSingleQuote && singleQuoteStart >= 0)
        {
            return std::make_pair(singleQuoteStart, QLatin1Char('\''));
        }
        if (inDoubleQuote && doubleQuoteStart >= 0)
        {
            return std::make_pair(doubleQuoteStart, QLatin1Char('"'));
        }
        return std::nullopt;
    };

    auto looksLikeCodeLine = [&](const QString& value) -> bool
    {
        static const QRegularExpression kCodeLeadRx(
            QStringLiteral("^(call|goto|return|stop|sleep|if|while|for|switch|else|do|break|continue|case|default)\\b"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
        return kLabelRx.match(value).hasMatch()
            || value.endsWith(QLatin1Char(';'))
            || value.endsWith(QLatin1Char('{'))
            || value.endsWith(QLatin1Char('}'))
            || kCodeLeadRx.match(value).hasMatch()
            || value.contains(QLatin1Char('('))
            || value.contains(QLatin1Char('='));
    };

    enum class AttributeNumericParseState
    {
        missing,
        invalid,
        outOfRange,
        ok,
    };

    auto classifyLinkIdValue = [](const QString& text) -> AttributeNumericParseState
    {
        const QString trimmed = text.trimmed();
        if (trimmed.isEmpty())
        {
            return AttributeNumericParseState::missing;
        }

        static const QRegularExpression kIntRx(
            QStringLiteral("^[+-]?\\d+$"),
            QRegularExpression::UseUnicodePropertiesOption);
        if (!kIntRx.match(trimmed).hasMatch())
        {
            return AttributeNumericParseState::invalid;
        }

        bool ok = false;
        const qlonglong value = trimmed.toLongLong(&ok, 10);
        if (!ok)
        {
            return AttributeNumericParseState::invalid;
        }
        if (value < static_cast<qlonglong>(std::numeric_limits<std::int32_t>::min())
            || value > static_cast<qlonglong>(std::numeric_limits<std::int32_t>::max()))
        {
            return AttributeNumericParseState::outOfRange;
        }

        return AttributeNumericParseState::ok;
    };

    auto classifyMaskValue = [](const QString& text) -> AttributeNumericParseState
    {
        const QString trimmed = text.trimmed();
        if (trimmed.isEmpty())
        {
            return AttributeNumericParseState::missing;
        }

        if (!parseMaskValue(trimmed))
        {
            bool ok = false;
            trimmed.toULongLong(&ok, 10);
            if (ok)
            {
                return AttributeNumericParseState::outOfRange;
            }

            QString hexDigits = trimmed;
            if (hexDigits.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
            {
                hexDigits = hexDigits.mid(2);
                hexDigits.toULongLong(&ok, 16);
                return ok ? AttributeNumericParseState::outOfRange : AttributeNumericParseState::invalid;
            }

            return AttributeNumericParseState::invalid;
        }

        return AttributeNumericParseState::ok;
    };

    auto firstDiagnosticSpan = [&](const QString& value) -> std::pair<int, int>
    {
        if (const auto match = kFirstTokenRx.match(value); match.hasMatch())
        {
            return {
                static_cast<int>(match.capturedStart(1)),
                std::max(1, static_cast<int>(match.capturedLength(1))),
            };
        }
        return { 0, 1 };
    };

    auto tokenAfterLeadingKeyword = [](const QString& value, const QString& keyword)
        -> std::optional<std::pair<int, int>>
    {
        static const QString kTokenTerminators = QStringLiteral(" \t;:,(){}[]");
        const QString lowerValue = value.toLower();
        int pos = 0;
        while (pos < lowerValue.size() && lowerValue.at(pos).isSpace())
        {
            ++pos;
        }
        if (!lowerValue.mid(pos).startsWith(keyword))
        {
            return std::nullopt;
        }

        pos += keyword.size();
        while (pos < value.size() && value.at(pos).isSpace())
        {
            ++pos;
        }
        if (pos >= value.size())
        {
            return std::nullopt;
        }

        int end = pos + 1;
        if (!kTokenTerminators.contains(value.at(pos)))
        {
            while (end < value.size() && !kTokenTerminators.contains(value.at(end)))
            {
                ++end;
            }
        }

        return std::make_pair(pos, std::max(1, end - pos));
    };

    auto findMatchingParen = [](const QString& value, const int openPos) -> int
    {
        if (openPos < 0 || openPos >= value.size() || value.at(openPos) != QLatin1Char('('))
        {
            return -1;
        }

        int depth = 0;
        bool inSingleQuote = false;
        bool inDoubleQuote = false;
        for (int i = openPos; i < value.size(); ++i)
        {
            const QChar ch = value.at(i);
            const QChar prev = (i > 0) ? value.at(i - 1) : QChar{};
            if (!inDoubleQuote && ch == QLatin1Char('\'') && prev != QLatin1Char('\\'))
            {
                inSingleQuote = !inSingleQuote;
                continue;
            }
            if (!inSingleQuote && ch == QLatin1Char('"') && prev != QLatin1Char('\\'))
            {
                inDoubleQuote = !inDoubleQuote;
                continue;
            }
            if (inSingleQuote || inDoubleQuote)
            {
                continue;
            }

            if (ch == QLatin1Char('('))
            {
                ++depth;
            }
            else if (ch == QLatin1Char(')'))
            {
                --depth;
                if (depth == 0)
                {
                    return i;
                }
            }
        }

        return -1;
    };

    auto reservedIdentifierCategory = [&](const QString& name) -> QString
    {
        if (kSectionWords.contains(name))
        {
            return QStringLiteral("section name");
        }
        if (kKeywordWords.contains(name))
        {
            return QStringLiteral("keyword");
        }
        if (kTypeWords.contains(name))
        {
            return QStringLiteral("type name");
        }
        if (kAttributeWords.contains(name))
        {
            return QStringLiteral("attribute name");
        }
        return {};
    };

    auto symbolReservedIdentifierCategory = [&](const QString& name) -> QString
    {
        const QString category = reservedIdentifierCategory(name);
        if (category == QStringLiteral("keyword") || category == QStringLiteral("section name"))
        {
            return {};
        }
        return category;
    };

    const std::uint32_t supportedMaskBits = cogScriptSupportedMaskBits(projectType);

    auto flushPendingOpenParens = [&](const int fallbackLine)
    {
        if (pendingOpenParens.isEmpty())
        {
            return;
        }

        const PendingParenRef openRef = pendingOpenParens.front();
        out.push_back(CogScriptDiagnostic{
            .message = QStringLiteral("Function call or expression is missing a closing ')'."),
            .line    = (openRef.line >= 0) ? openRef.line : fallbackLine,
            .error   = true,
            .column  = std::max(0, openRef.column),
            .length  = 1,
        });
        pendingOpenParens.clear();
    };

    auto flushPendingOpenBraces = [&](const int fallbackLine)
    {
        if (pendingOpenBraces.isEmpty())
        {
            return;
        }

        const PendingBraceRef openRef = pendingOpenBraces.front();
        out.push_back(CogScriptDiagnostic{
            .message = QStringLiteral("Code block is missing a closing '}'."),
            .line    = (openRef.line >= 0) ? openRef.line : fallbackLine,
            .error   = true,
            .column  = std::max(0, openRef.column),
            .length  = 1,
        });
        pendingOpenBraces.clear();
    };

    auto flushPendingOpenBrackets = [&](const int fallbackLine)
    {
        if (pendingOpenBrackets.isEmpty())
        {
            return;
        }

        const PendingBracketRef openRef = pendingOpenBrackets.front();
        out.push_back(CogScriptDiagnostic{
            .message = QStringLiteral("Index expression is missing a closing ']'."),
            .line    = (openRef.line >= 0) ? openRef.line : fallbackLine,
            .error   = true,
            .column  = std::max(0, openRef.column),
            .length  = 1,
        });
        pendingOpenBrackets.clear();
    };

    const QStringList lines = text.split(QLatin1Char('\n'));
    appendGenericStatementTerminationDiagnostics(lines, out);
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
    {
        const QString originalLine = lines[lineIndex];
        const QString rawLine = stripComment(originalLine);
        const QString line = rawLine.trimmed();
        const QString analysisLine = sanitizeLineForAnalysis(originalLine);
        const QString labelAnalysisLine = analysisLine.trimmed();
        const int leadingWhitespace = cogLeadingWhitespaceCount(rawLine);
        const int trimmedLength = static_cast<int>(line.size());

        // COG supports only line comments (`#` and `//`). Search the
        // position-preserving analysis text so delimiters inside strings or
        // after a real line-comment marker cannot become false positives.
        // Report every authored delimiter independently; `/* ... */` must
        // never be mistaken for a supported stateful comment construct.
        const bool hasBlockCommentDelimiter =
            cogAnalysisContainsBlockCommentDelimiter(analysisLine);
        for (int column = 0; column + 1 < analysisLine.size(); ++column)
        {
            const QChar first = analysisLine.at(column);
            const QChar second = analysisLine.at(column + 1);
            const bool isOpeningDelimiter = first == QLatin1Char('/')
                && second == QLatin1Char('*');
            const bool isClosingDelimiter = first == QLatin1Char('*')
                && second == QLatin1Char('/');
            if (!isOpeningDelimiter && !isClosingDelimiter)
            {
                continue;
            }
            out.push_back(CogScriptDiagnostic{
                .message = QStringLiteral(
                    "Block comments are not supported; use '#' or '//' line comments."),
                .line = lineIndex,
                .error = true,
                .column = column,
                .length = 2,
            });
        }

        // The delimiter errors above are authoritative. Skip other language
        // analysis for this invalid physical line so braces, brackets, or
        // statement-looking prose intended as a block comment cannot create
        // misleading secondary diagnostics.
        if (hasBlockCommentDelimiter)
        {
            continue;
        }

        if (line.isEmpty())
        {
            continue;
        }

        if (const auto unterminatedQuote = findUnterminatedQuote(rawLine))
        {
            out.push_back(CogScriptDiagnostic{
                .message = QStringLiteral("String literal is missing a closing %1.")
                    .arg(QString(unterminatedQuote->second)),
                .line    = lineIndex,
                .error   = true,
                .column  = std::max(0, unterminatedQuote->first),
                .length  = 1,
            });
        }

        if (kFlagsStartRx.match(line).hasMatch())
        {
            const int flagsColumn = leadingWhitespace;
            const int flagsLength = 5;
            const auto flagsMatch = kFlagsRx.match(line);
            if (!flagsMatch.hasMatch())
            {
                const int equalsPos = rawLine.indexOf(QLatin1Char('='));
                const int column = (equalsPos >= 0) ? equalsPos : leadingWhitespace;
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Flags line must be in the form 'flags=<hexadecimal>'."),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = column,
                    .length  = std::max(1, trimmedLength),
                });
            }
            else
            {
                switch (parseCogScriptFlagsValue(flagsMatch.capturedView(1)).state)
                {
                    case CogScriptFlagsValueParseState::outOfRange:
                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("Flags value must fit in a 32-bit unsigned integer."),
                            .line    = lineIndex,
                            .error   = true,
                            .column  = flagsColumn + static_cast<int>(flagsMatch.capturedStart(1)),
                            .length  = std::max(1, static_cast<int>(flagsMatch.capturedLength(1))),
                        });
                        break;
                    case CogScriptFlagsValueParseState::missing:
                    case CogScriptFlagsValueParseState::invalid:
                    case CogScriptFlagsValueParseState::ok:
                        break;
                }
            }
            if (sawFlags)
            {
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Duplicate flags line."),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = flagsColumn,
                    .length  = flagsLength,
                });
            }
            if (sawSymbols || sawCode)
            {
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Flags line should appear before symbols/code."),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = flagsColumn,
                    .length  = flagsLength,
                });
            }
            sawFlags = true;
            continue;
        }

        if (const auto match = kSectionRx.match(line); match.hasMatch())
        {
            const QString trailingText = line.mid(match.capturedEnd(1));
            int trailingOffset = static_cast<int>(match.capturedEnd(1));
            while (trailingOffset < line.size() && line.at(trailingOffset).isSpace())
            {
                ++trailingOffset;
            }
            if (!trailingText.trimmed().isEmpty())
            {
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Section header '%1' does not take trailing text.")
                        .arg(match.captured(1).toLower()),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = leadingWhitespace + trailingOffset,
                    .length  = std::max(1, static_cast<int>(line.size()) - trailingOffset),
                });
            }

            if (currentSection == Section::code)
            {
                flushPendingOpenParens(lineIndex);
                flushPendingOpenBraces(lineIndex);
                flushPendingOpenBrackets(lineIndex);
            }

            const QString sectionName = match.captured(1).toLower();
            const int sectionColumn = leadingWhitespace + static_cast<int>(match.capturedStart(1));
            const int sectionLength = std::max(1, static_cast<int>(match.capturedLength(1)));
            if (currentSection != Section::none)
            {
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Section '%1' was not closed before '%2'.")
                        .arg(
                            currentSection == Section::symbols ? QStringLiteral("symbols")
                                                               : QStringLiteral("code"),
                            sectionName),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = sectionColumn,
                    .length  = sectionLength,
                });
            }

            if (sectionName == QStringLiteral("symbols"))
            {
                if (sawSymbols)
                {
                    out.push_back(CogScriptDiagnostic{
                        .message = QStringLiteral("Duplicate symbols section."),
                        .line    = lineIndex,
                        .error   = true,
                        .column  = sectionColumn,
                        .length  = sectionLength,
                    });
                }
                if (sawCode)
                {
                    out.push_back(CogScriptDiagnostic{
                        .message = QStringLiteral("Symbols section should appear before code."),
                        .line    = lineIndex,
                        .error   = true,
                        .column  = sectionColumn,
                        .length  = sectionLength,
                    });
                }
                sawSymbols = true;
                currentSection = Section::symbols;
            }
            else
            {
                if (sawCode)
                {
                    out.push_back(CogScriptDiagnostic{
                        .message = QStringLiteral("Duplicate code section."),
                        .line    = lineIndex,
                        .error   = true,
                        .column  = sectionColumn,
                        .length  = sectionLength,
                    });
                }
                sawCode = true;
                currentSection = Section::code;
            }

            currentSectionLine = lineIndex;
            currentSectionColumn = sectionColumn;
            currentSectionLength = sectionLength;
            continue;
        }

        if (kEndRx.match(line).hasMatch())
        {
            const int endTokenLength = 3;
            int trailingOffset = endTokenLength;
            while (trailingOffset < line.size() && line.at(trailingOffset).isSpace())
            {
                ++trailingOffset;
            }
            if (trailingOffset < line.size())
            {
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("'end' does not take trailing text."),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = leadingWhitespace + trailingOffset,
                    .length  = std::max(1, static_cast<int>(line.size()) - trailingOffset),
                });
            }

            if (currentSection == Section::code)
            {
                flushPendingOpenParens(lineIndex);
                flushPendingOpenBraces(lineIndex);
                flushPendingOpenBrackets(lineIndex);
            }

            if (currentSection == Section::none)
            {
                const auto [tokenColumn, tokenLength] = firstDiagnosticSpan(rawLine);
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Unexpected 'end' outside a section."),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = tokenColumn,
                    .length  = tokenLength,
                });
            }
            currentSection = Section::none;
            currentSectionLine = -1;
            currentSectionColumn = 0;
            currentSectionLength = 0;
            continue;
        }

        if (currentSection == Section::symbols)
        {
            const auto match = kSymbolRx.match(line);
            if (!match.hasMatch())
            {
                static const QRegularExpression kSymbolLeadRx(
                    QStringLiteral("^\\s*([A-Za-z0-9_]+)(?:\\s+(\\S+))?"),
                    QRegularExpression::UseUnicodePropertiesOption);
                if (const auto leadMatch = kSymbolLeadRx.match(line);
                    leadMatch.hasMatch() && kTypeWords.contains(leadMatch.captured(1).toLower()))
                {
                    const int nameStart = static_cast<int>(leadMatch.capturedStart(2));
                    const int nameLength = static_cast<int>(leadMatch.capturedLength(2));
                    if (nameStart >= 0)
                    {
                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("Symbol declaration has an invalid symbol name."),
                            .line    = lineIndex,
                            .error   = true,
                            .column  = leadingWhitespace + nameStart,
                            .length  = std::max(1, nameLength),
                        });
                    }
                    else
                    {
                        const int typeStart = leadingWhitespace + static_cast<int>(leadMatch.capturedStart(1));
                        const int typeLength = std::max(1, static_cast<int>(leadMatch.capturedLength(1)));
                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("Symbol declaration requires a symbol name."),
                            .line    = lineIndex,
                            .error   = true,
                            .column  = typeStart,
                            .length  = typeLength,
                        });
                    }
                    continue;
                }

                if (looksLikeCodeLine(line))
                {
                    const auto [tokenColumn, tokenLength] = firstDiagnosticSpan(rawLine);
                    out.push_back(CogScriptDiagnostic{
                        .message = QStringLiteral("Code must be inside the code section."),
                        .line    = lineIndex,
                        .error   = true,
                        .column  = tokenColumn,
                        .length  = tokenLength,
                    });
                    continue;
                }

                const auto [tokenColumn, tokenLength] = firstDiagnosticSpan(rawLine);
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Line is not a valid symbol declaration."),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = tokenColumn,
                    .length  = tokenLength,
                });
                continue;
            }
            const QString typeName = match.captured(1).toLower();
            const int typeColumn = leadingWhitespace + static_cast<int>(match.capturedStart(1));
            const int typeLength = std::max(1, static_cast<int>(match.capturedLength(1)));
            if (!kTypeWords.contains(typeName))
            {
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Unknown symbol type '%1'.").arg(match.captured(1)),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = typeColumn,
                    .length  = typeLength,
                });
                continue;
            }
            const QString symbolName = match.captured(2).toLower();
            const int symbolColumn = leadingWhitespace + static_cast<int>(match.capturedStart(2));
            const int symbolLength = static_cast<int>(match.capturedLength(2));
            if (symbolLines.contains(symbolName))
            {
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Duplicate symbol '%1'.").arg(match.captured(2)),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = symbolColumn,
                    .length  = symbolLength,
                });
            }
            else
            {
                symbolLines.insert(symbolName, lineIndex);
                symbolSpans.insert(symbolName, { symbolColumn, symbolLength });
                symbolTypeSpans.insert(symbolName, { typeColumn, typeLength });
                declaredSymbolIndices.insert(symbolName, static_cast<int>(declaredSymbols.size()));
                declaredSymbols.push_back(DeclaredSymbol{ symbolName, typeName, lineIndex });
            }

            if (kGlobalWords.contains(symbolName))
            {
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Global '%1' is predefined by the engine and should not be declared in symbols.")
                        .arg(match.captured(2)),
                    .line    = lineIndex,
                    .error   = false,
                    .column  = symbolColumn,
                    .length  = symbolLength,
                });
            }
            if (const QString reservedCategory = symbolReservedIdentifierCategory(symbolName);
                !reservedCategory.isEmpty())
            {
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Symbol name '%1' uses reserved %2 '%1'.")
                        .arg(match.captured(2), reservedCategory),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = symbolColumn,
                    .length  = symbolLength,
                });
            }

            if (typeName != QStringLiteral("message") && knownMessageSet.contains(symbolName))
            {
                knownMessagesDeclaredWithWrongType.insert(symbolName);
            }

            if (typeName == QStringLiteral("message"))
            {
                declaredMessages.insert(symbolName);
                if (!knownMessageSet.contains(symbolName))
                {
                    out.push_back(CogScriptDiagnostic{
                        .message = QStringLiteral("Message '%1' is not defined for the current project type.").arg(match.captured(2)),
                        .line    = lineIndex,
                        .error   = false,
                        .column  = symbolColumn,
                        .length  = symbolLength,
                    });
                }
            }

            if (typeName == QStringLiteral("float") || typeName == QStringLiteral("flex"))
            {
                callableNames.insert(symbolName);
            }

            if (const QRegularExpressionMatch semicolons =
                    kRedundantSymbolSemicolonRx.match(analysisLine);
                semicolons.hasMatch())
            {
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral(
                        "Symbol declarations do not require a terminating ';'."),
                    .line    = lineIndex,
                    .error   = false,
                    .column  = static_cast<int>(semicolons.capturedStart(1)),
                    .length  = std::max(
                        1,
                        static_cast<int>(semicolons.capturedLength(1))),
                });
            }

            const bool supportsTriggerFilterAttributes = cogScriptTypeSupportsTriggerFilterAttributes(typeName);

            const auto attrsMatch = kSymbolWithAttrsRx.match(line);
            if (attrsMatch.hasMatch())
            {
                const QString attrsText = attrsMatch.captured(3);
                const int attrsColumnBase = leadingWhitespace + static_cast<int>(attrsMatch.capturedStart(3));
                QHash<QString, std::pair<int, int>> seenAttributes{};
                bool sawLinkId = false;
                bool sawNoLink = false;
                std::pair<int, int> linkIdSpan{ symbolColumn, std::max(1, symbolLength) };
                std::pair<int, int> noLinkSpan{ symbolColumn, std::max(1, symbolLength) };

                QRegularExpressionMatchIterator attrIt = kAttributeTokenRx.globalMatch(attrsText);
                while (attrIt.hasNext())
                {
                    const QRegularExpressionMatch attrMatch = attrIt.next();
                    const QString attributeName = attrMatch.captured(2).toLower();
                    const int attrColumn = attrsColumnBase + static_cast<int>(attrMatch.capturedStart(2));
                    const int attrLength = static_cast<int>(attrMatch.capturedLength(2));

                    QString attributeValue{};
                    int attributeValueColumn = -1;
                    int attributeValueLength = 0;
                    for (int group = 3; group <= 5; ++group)
                    {
                        const int capturedStart = attrMatch.capturedStart(group);
                        if (capturedStart < 0)
                        {
                            continue;
                        }

                        attributeValue = attrMatch.captured(group);
                        attributeValueColumn = attrsColumnBase + capturedStart;
                        attributeValueLength = static_cast<int>(attrMatch.capturedLength(group));
                        break;
                    }
                    const int diagnosticColumn = (attributeValueColumn >= 0) ? attributeValueColumn : attrColumn;
                    const int diagnosticLength = std::max(
                        1,
                        (attributeValueLength > 0)
                            ? attributeValueLength
                            : std::max(attrLength, static_cast<int>(attrMatch.capturedLength(0))));

                    if (const auto seenIt = seenAttributes.find(attributeName); seenIt != seenAttributes.end())
                    {
                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("Attribute '%1' is specified more than once for symbol '%2'.")
                                .arg(attributeName, match.captured(2)),
                            .line    = lineIndex,
                            .error   = true,
                            .column  = attrColumn,
                            .length  = attrLength,
                        });
                        continue;
                    }
                    seenAttributes.insert(attributeName, { attrColumn, attrLength });

                    if (!supportsTriggerFilterAttributes
                        && (attributeName == QStringLiteral("mask")
                            || attributeName == QStringLiteral("linkid")
                            || attributeName == QStringLiteral("nolink")))
                    {
                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("Attribute '%1' only applies to message, sector, surface, or thing symbols.")
                                .arg(attributeName),
                            .line    = lineIndex,
                            .error   = false,
                            .column  = attrColumn,
                            .length  = std::max(1, attrLength),
                        });
                    }

                    if (attributeName == QStringLiteral("linkid"))
                    {
                        sawLinkId = true;
                        linkIdSpan = { attrColumn, std::max(1, attrLength) };
                        switch (classifyLinkIdValue(attributeValue))
                        {
                            case AttributeNumericParseState::missing:
                                out.push_back(CogScriptDiagnostic{
                                    .message = QStringLiteral("Attribute 'linkid' requires an integer value."),
                                    .line    = lineIndex,
                                    .error   = true,
                                    .column  = diagnosticColumn,
                                    .length  = diagnosticLength,
                                });
                                break;
                            case AttributeNumericParseState::invalid:
                                out.push_back(CogScriptDiagnostic{
                                    .message = QStringLiteral("Attribute 'linkid' must be an integer value."),
                                    .line    = lineIndex,
                                    .error   = true,
                                    .column  = diagnosticColumn,
                                    .length  = diagnosticLength,
                                });
                                break;
                            case AttributeNumericParseState::outOfRange:
                                out.push_back(CogScriptDiagnostic{
                                    .message = QStringLiteral("Attribute 'linkid' must fit in a 32-bit signed integer."),
                                    .line    = lineIndex,
                                    .error   = true,
                                    .column  = diagnosticColumn,
                                    .length  = diagnosticLength,
                                });
                                break;
                            case AttributeNumericParseState::ok:
                                break;
                        }
                    }
                    else if (attributeName == QStringLiteral("mask"))
                    {
                        switch (classifyMaskValue(attributeValue))
                        {
                            case AttributeNumericParseState::missing:
                                out.push_back(CogScriptDiagnostic{
                                    .message = QStringLiteral("Attribute 'mask' requires a numeric value."),
                                    .line    = lineIndex,
                                    .error   = true,
                                    .column  = diagnosticColumn,
                                    .length  = diagnosticLength,
                                });
                                break;
                            case AttributeNumericParseState::invalid:
                                out.push_back(CogScriptDiagnostic{
                                    .message = QStringLiteral("Attribute 'mask' must be an unsigned integer value."),
                                    .line    = lineIndex,
                                    .error   = true,
                                    .column  = diagnosticColumn,
                                    .length  = diagnosticLength,
                                });
                                break;
                            case AttributeNumericParseState::outOfRange:
                                out.push_back(CogScriptDiagnostic{
                                    .message = QStringLiteral("Attribute 'mask' must fit in a 32-bit unsigned integer."),
                                    .line    = lineIndex,
                                    .error   = true,
                                    .column  = diagnosticColumn,
                                    .length  = diagnosticLength,
                                });
                                break;
                            case AttributeNumericParseState::ok:
                                if (const auto parsedMask = parseMaskValue(attributeValue))
                                {
                                    const std::uint32_t unsupportedBits = *parsedMask & ~supportedMaskBits;
                                    if (unsupportedBits != 0u
                                        && !isEngineAllTypesMaskSpelling(attributeValue))
                                    {
                                        out.push_back(CogScriptDiagnostic{
                                            .message = QStringLiteral("Attribute 'mask' contains unsupported bits for the current project type: %1.")
                                                .arg(formatMaskHex(unsupportedBits)),
                                            .line    = lineIndex,
                                            .error   = false,
                                            .column  = diagnosticColumn,
                                            .length  = diagnosticLength,
                                        });
                                    }
                                }
                                break;
                        }
                    }
                    else if (attributeName == QStringLiteral("desc"))
                    {
                        if (attributeValue.trimmed().isEmpty())
                        {
                            out.push_back(CogScriptDiagnostic{
                                .message = QStringLiteral("Attribute 'desc' requires a one-word value."),
                                .line    = lineIndex,
                                .error   = true,
                                .column  = diagnosticColumn,
                                .length  = diagnosticLength,
                            });
                        }
                        else if (attributeValue.contains(QRegularExpression(QStringLiteral("\\s"))))
                        {
                            out.push_back(CogScriptDiagnostic{
                                .message = QStringLiteral("Attribute 'desc' must be a single word. Use '_' instead of spaces."),
                                .line    = lineIndex,
                                .error   = false,
                                .column  = diagnosticColumn,
                                .length  = diagnosticLength,
                            });
                        }
                    }
                    else if (attributeName == QStringLiteral("local")
                        || attributeName == QStringLiteral("nolink"))
                    {
                        if (attributeValueColumn >= 0)
                        {
                            out.push_back(CogScriptDiagnostic{
                                .message = QStringLiteral("Attribute '%1' does not take a value.")
                                    .arg(attributeName),
                                .line    = lineIndex,
                                .error   = true,
                                .column  = diagnosticColumn,
                                .length  = diagnosticLength,
                            });
                        }

                        if (attributeName == QStringLiteral("nolink"))
                        {
                            sawNoLink = true;
                            noLinkSpan = { attrColumn, std::max(1, attrLength) };
                        }
                    }
                }

                if (sawLinkId && sawNoLink)
                {
                    out.push_back(CogScriptDiagnostic{
                        .message = QStringLiteral("Attributes 'linkid' and 'nolink' are mutually exclusive."),
                        .line    = lineIndex,
                        .error   = false,
                        .column  = linkIdSpan.first,
                        .length  = linkIdSpan.second,
                    });
                    out.push_back(CogScriptDiagnostic{
                        .message = QStringLiteral("Attributes 'nolink' and 'linkid' are mutually exclusive."),
                        .line    = lineIndex,
                        .error   = false,
                        .column  = noLinkSpan.first,
                        .length  = noLinkSpan.second,
                    });
                }
            }
        }
        else if (currentSection == Section::code)
        {
            if (const auto symbolMatch = kSymbolRx.match(line);
                symbolMatch.hasMatch() && kTypeWords.contains(symbolMatch.captured(1).toLower()))
            {
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Symbol declarations must be inside the symbols section."),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = leadingWhitespace + static_cast<int>(symbolMatch.capturedStart(1)),
                    .length  = std::max(1, static_cast<int>(symbolMatch.capturedLength(1))),
                });
                continue;
            }

            static const QRegularExpression kCallStatementRx(
                QStringLiteral("^\\s*call\\b"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression kStrictCallStatementRx(
                QStringLiteral("^\\s*call\\s+([A-Za-z_][A-Za-z0-9_]*)\\b\\s*(.*)$"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression kGotoStatementRx(
                QStringLiteral("^\\s*goto\\b"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression kStrictGotoStatementRx(
                QStringLiteral("^\\s*goto\\s+([A-Za-z_][A-Za-z0-9_]*)\\b\\s*(.*)$"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression kTerminatorAndClosingBracesRx(
                QStringLiteral("^;\\s*(?:\\}\\s*)*$"),
                QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression kReturnStatementRx(
                QStringLiteral("^\\s*return\\b\\s*(.*)$"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression kStopStatementRx(
                QStringLiteral("^\\s*stop\\b\\s*(.*)$"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression kSleepStatementRx(
                QStringLiteral("^\\s*sleep\\b"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression kEmptySleepRx(
                QStringLiteral("^\\s*sleep\\s*\\(\\s*\\)"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression kForStatementRx(
                QStringLiteral("^\\s*for\\b"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression kConditionalHeadRx(
                QStringLiteral("^\\s*((?:else\\s+)?if|while|for|switch)\\b"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression kEmptyConditionalRx(
                QStringLiteral("^\\s*((?:else\\s+)?if|while|switch)\\s*\\(\\s*\\)"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression kElseIfHeadRx(
                QStringLiteral("\\belse\\s+if\\b"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression kHyphenatedElseIfHeadRx(
                QStringLiteral("\\belse-if\\b"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression kCaseLabelHeadRx(
                QStringLiteral("^\\s*(case)\\b"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression kDefaultLabelHeadRx(
                QStringLiteral("^\\s*(default)\\b"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression kBreakOrContinueStatementRx(
                QStringLiteral("^\\s*(break|continue)\\b"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression kStrictBreakOrContinueRx(
                QStringLiteral("^\\s*(break|continue)\\b\\s*(.*)$"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            const auto leadingDeclaredKeywordActsLikeSymbol = [&](QStringView keyword) -> bool
            {
                const QString keywordLower = keyword.toString().toLower();
                if (!symbolLines.contains(keywordLower))
                {
                    return false;
                }

                int start = 0;
                while (start < line.size() && line.at(start).isSpace())
                {
                    ++start;
                }

                const int keywordLength = static_cast<int>(keyword.size());
                if (line.mid(start, keywordLength).compare(keyword.toString(), Qt::CaseInsensitive) != 0)
                {
                    return false;
                }

                const int afterKeyword = start + keywordLength;
                if (afterKeyword < line.size())
                {
                    const QChar boundary = line.at(afterKeyword);
                    if (boundary.isLetterOrNumber() || boundary == QLatin1Char('_'))
                    {
                        return false;
                    }
                }

                int next = afterKeyword;
                while (next < line.size() && line.at(next).isSpace())
                {
                    ++next;
                }
                if (next >= line.size())
                {
                    return false;
                }

                const QChar ch = line.at(next);
                return ch == QLatin1Char('=')
                       || ch == QLatin1Char('[')
                       || ch == QLatin1Char('.')
                       || ch == QLatin1Char('+')
                       || ch == QLatin1Char('-')
                       || ch == QLatin1Char('*')
                       || ch == QLatin1Char('/')
                       || ch == QLatin1Char('%')
                       || ch == QLatin1Char('|')
                       || ch == QLatin1Char('&')
                       || ch == QLatin1Char('^')
                       || ch == QLatin1Char('<')
                       || ch == QLatin1Char('>')
                       || ch == QLatin1Char('!')
                       || ch == QLatin1Char('?')
                       || ch == QLatin1Char(':');
            };

            bool inSingleQuote = false;
            bool inDoubleQuote = false;
            for (int i = 0; i < analysisLine.size(); ++i)
            {
                const QChar ch = analysisLine.at(i);
                const QChar prev = (i > 0) ? analysisLine.at(i - 1) : QChar{};
                if (!inDoubleQuote && ch == QLatin1Char('\'') && prev != QLatin1Char('\\'))
                {
                    inSingleQuote = !inSingleQuote;
                    continue;
                }
                if (!inSingleQuote && ch == QLatin1Char('"') && prev != QLatin1Char('\\'))
                {
                    inDoubleQuote = !inDoubleQuote;
                    continue;
                }
                if (inSingleQuote || inDoubleQuote)
                {
                    continue;
                }

                const auto isIdentifierCharacter = [](const QChar value) -> bool
                {
                    return value.isLetterOrNumber() || value == QLatin1Char('_');
                };
                if (ch == QLatin1Char('{'))
                {
                    const int previous = i - 1;
                    if (previous >= 0 && isIdentifierCharacter(analysisLine.at(previous)))
                    {
                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("Token immediately before '{' must be separated by whitespace."),
                            .line    = lineIndex,
                            .error   = true,
                            .column  = previous,
                            .length  = 2,
                        });
                    }
                }
                else if (ch == QLatin1Char('}'))
                {
                    int next = i + 1;
                    while (next < analysisLine.size() && analysisLine.at(next).isSpace())
                    {
                        ++next;
                    }
                    const QString trailing = (next < analysisLine.size())
                        ? analysisLine.mid(next).trimmed()
                        : QString{};
                    const auto startsKeyword = [&trailing, &isIdentifierCharacter](
                                                   const QString& keyword) -> bool
                    {
                        return trailing.startsWith(keyword, Qt::CaseInsensitive)
                            && (trailing.size() == keyword.size()
                                || !isIdentifierCharacter(
                                    trailing.at(keyword.size())));
                    };
                    const bool startsValidBraceFollower =
                        startsKeyword(QStringLiteral("else"))
                        || startsKeyword(QStringLiteral("if"));
                    if (next < analysisLine.size()
                        && isIdentifierCharacter(analysisLine.at(next))
                        && !startsValidBraceFollower)
                    {
                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("Code cannot follow a closing brace on the same line."),
                            .line    = lineIndex,
                            .error   = true,
                            .column  = next,
                            .length  = std::max(1, static_cast<int>(trailing.size())),
                        });
                    }

                    int previous = i - 1;
                    while (previous >= 0 && analysisLine.at(previous).isSpace())
                    {
                        --previous;
                    }
                    if (previous >= 0
                        && (isIdentifierCharacter(analysisLine.at(previous))
                            || analysisLine.at(previous) == QLatin1Char(')')
                            || analysisLine.at(previous) == QLatin1Char(']')))
                    {
                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("Statement before '}' is missing a terminating ';'."),
                            .line    = lineIndex,
                            .error   = true,
                            .column  = previous,
                            .length  = 1,
                        });
                    }
                }

                if (ch == QLatin1Char('('))
                {
                    pendingOpenParens.push_back(PendingParenRef{ lineIndex, i });
                }
                else if (ch == QLatin1Char(')'))
                {
                    if (!pendingOpenParens.isEmpty())
                    {
                        pendingOpenParens.removeLast();
                    }
                    else
                    {
                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("Unmatched ')'."),
                            .line    = lineIndex,
                            .error   = true,
                            .column  = i,
                            .length  = 1,
                        });
                    }
                }
                else if (ch == QLatin1Char('{'))
                {
                    pendingOpenBraces.push_back(PendingBraceRef{ lineIndex, i });
                }
                else if (ch == QLatin1Char('}'))
                {
                    if (!pendingOpenBraces.isEmpty())
                    {
                        pendingOpenBraces.removeLast();
                    }
                    else
                    {
                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("Unmatched '}'."),
                            .line    = lineIndex,
                            .error   = true,
                            .column  = i,
                            .length  = 1,
                        });
                    }
                }
                else if (ch == QLatin1Char('['))
                {
                    pendingOpenBrackets.push_back(PendingBracketRef{ lineIndex, i });
                }
                else if (ch == QLatin1Char(']'))
                {
                    if (!pendingOpenBrackets.isEmpty())
                    {
                        pendingOpenBrackets.removeLast();
                    }
                    else
                    {
                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("Unmatched ']'."),
                            .line    = lineIndex,
                            .error   = true,
                            .column  = i,
                            .length  = 1,
                        });
                    }
                }
            }

            const auto labelMatch = kLabelRx.match(labelAnalysisLine);
            if (!labelMatch.hasMatch())
            {
                if (const auto emptyLabelMatch =
                        kEmptyLabelRx.match(labelAnalysisLine);
                    emptyLabelMatch.hasMatch())
                {
                    out.push_back(CogScriptDiagnostic{
                        .message = QStringLiteral("Label declaration requires a label name."),
                        .line    = lineIndex,
                        .error   = true,
                        .column  = leadingWhitespace + static_cast<int>(emptyLabelMatch.capturedStart(0)),
                        .length  = 1,
                    });
                    continue;
                }

                if (const auto malformedLabelMatch =
                        kMalformedLabelRx.match(labelAnalysisLine);
                    malformedLabelMatch.hasMatch())
                {
                    out.push_back(CogScriptDiagnostic{
                        .message = QStringLiteral("Label declaration has an invalid label name."),
                        .line    = lineIndex,
                        .error   = true,
                        .column  = leadingWhitespace + static_cast<int>(malformedLabelMatch.capturedStart(1)),
                        .length  = std::max(1, static_cast<int>(malformedLabelMatch.capturedLength(1))),
                    });
                    continue;
                }
            }

            if (labelMatch.hasMatch())
            {
                const QString labelName = labelMatch.captured(1).toLower();
                const int labelColumn = leadingWhitespace + static_cast<int>(labelMatch.capturedStart(1));
                const int labelLength = static_cast<int>(labelMatch.capturedLength(1));
                if (labelLines.contains(labelName))
                {
                    out.push_back(CogScriptDiagnostic{
                        .message = knownMessages.contains(labelMatch.captured(1), Qt::CaseInsensitive)
                            ? QStringLiteral("Duplicate message handler '%1'.").arg(labelMatch.captured(1))
                            : QStringLiteral("Duplicate label '%1'.").arg(labelMatch.captured(1)),
                        .line    = lineIndex,
                        .error   = true,
                        .column  = labelColumn,
                        .length  = labelLength,
                    });
                }
                else
                {
                    labelLines.insert(labelName, lineIndex);
                    labelSpans.insert(labelName, { labelColumn, labelLength });
                }
                if (const QString reservedCategory = reservedIdentifierCategory(labelName);
                    !reservedCategory.isEmpty())
                {
                    out.push_back(CogScriptDiagnostic{
                        .message = QStringLiteral("Label '%1' uses reserved %2 '%1'.")
                            .arg(labelMatch.captured(1), reservedCategory),
                        .line    = lineIndex,
                        .error   = true,
                        .column  = labelColumn,
                        .length  = labelLength,
                    });
                }
                if (knownMessages.contains(labelMatch.captured(1), Qt::CaseInsensitive))
                {
                    handledMessages.insert(labelName);
                }
                callableNames.insert(labelName);
            }

            QRegularExpressionMatchIterator it = kCallRx.globalMatch(analysisLine);
            while (it.hasNext())
            {
                const QRegularExpressionMatch match = it.next();
                pendingCalls.push_back(PendingCallRef{
                    match.captured(1).toLower(),
                    lineIndex,
                    static_cast<int>(match.capturedStart(1)),
                    static_cast<int>(match.capturedLength(1)),
                });
            }
            if (!leadingDeclaredKeywordActsLikeSymbol(QStringView{ QStringLiteral("call") })
                && kCallStatementRx.match(line).hasMatch()
                && !kCallRx.match(analysisLine).hasMatch())
            {
                const auto span = tokenAfterLeadingKeyword(rawLine, QStringLiteral("call"));
                const auto [tokenColumn, tokenLength] = span ? *span : firstDiagnosticSpan(rawLine);
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Call statement requires a target label or flex/float symbol."),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = tokenColumn,
                    .length  = tokenLength,
                });
            }
            else if (!leadingDeclaredKeywordActsLikeSymbol(QStringView{ QStringLiteral("call") }))
            {
                if (const auto strictCallMatch = kStrictCallStatementRx.match(line); strictCallMatch.hasMatch())
                {
                    const QString trailing = strictCallMatch.captured(2).trimmed();
                    if (!trailing.isEmpty()
                        && !kTerminatorAndClosingBracesRx.match(trailing).hasMatch())
                    {
                        int trailingColumn = static_cast<int>(strictCallMatch.capturedStart(2));
                        while (trailingColumn < line.size() && line.at(trailingColumn).isSpace())
                        {
                            ++trailingColumn;
                        }
                        const int trailingLength =
                            std::max(1, static_cast<int>(line.size()) - trailingColumn);
                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("Call statement does not take text after the target."),
                            .line    = lineIndex,
                            .error   = true,
                            .column  = leadingWhitespace + trailingColumn,
                            .length  = trailingLength,
                        });
                        continue;
                    }
                }
            }
            QRegularExpressionMatchIterator gotoIt = kGotoRx.globalMatch(analysisLine);
            while (gotoIt.hasNext())
            {
                const QRegularExpressionMatch match = gotoIt.next();
                pendingGotos.push_back(PendingGotoRef{
                    match.captured(1).toLower(),
                    lineIndex,
                    static_cast<int>(match.capturedStart(1)),
                    static_cast<int>(match.capturedLength(1)),
                });
            }
            if (!leadingDeclaredKeywordActsLikeSymbol(QStringView{ QStringLiteral("goto") })
                && kGotoStatementRx.match(line).hasMatch()
                && !kGotoRx.match(analysisLine).hasMatch())
            {
                const auto span = tokenAfterLeadingKeyword(rawLine, QStringLiteral("goto"));
                const auto [tokenColumn, tokenLength] = span ? *span : firstDiagnosticSpan(rawLine);
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Goto statement requires a target label."),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = tokenColumn,
                    .length  = tokenLength,
                });
            }
            else if (!leadingDeclaredKeywordActsLikeSymbol(QStringView{ QStringLiteral("goto") }))
            {
                if (const auto strictGotoMatch = kStrictGotoStatementRx.match(line); strictGotoMatch.hasMatch())
                {
                    const QString trailing = strictGotoMatch.captured(2).trimmed();
                    if (!trailing.isEmpty()
                        && !kTerminatorAndClosingBracesRx.match(trailing).hasMatch())
                    {
                        int trailingColumn = static_cast<int>(strictGotoMatch.capturedStart(2));
                        while (trailingColumn < line.size() && line.at(trailingColumn).isSpace())
                        {
                            ++trailingColumn;
                        }
                        const int trailingLength =
                            std::max(1, static_cast<int>(line.size()) - trailingColumn);
                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("Goto statement does not take text after the target."),
                            .line    = lineIndex,
                            .error   = true,
                            .column  = leadingWhitespace + trailingColumn,
                            .length  = trailingLength,
                        });
                        continue;
                    }
                }
            }
            if (!leadingDeclaredKeywordActsLikeSymbol(QStringView{ QStringLiteral("return") }))
            {
                if (const auto returnMatch = kReturnStatementRx.match(line); returnMatch.hasMatch())
                {
                    const QString trailing = returnMatch.captured(1).trimmed();
                    if (!trailing.isEmpty() && trailing != QStringLiteral(";"))
                    {
                        int trailingColumn = static_cast<int>(returnMatch.capturedStart(1));
                        while (trailingColumn < line.size() && line.at(trailingColumn).isSpace())
                        {
                            ++trailingColumn;
                        }
                        int trailingLength = static_cast<int>(line.size()) - trailingColumn;
                        if (const int semicolonPos = line.indexOf(QLatin1Char(';'), trailingColumn); semicolonPos >= 0)
                        {
                            trailingLength = semicolonPos - trailingColumn;
                        }

                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("Return statement does not take a value or expression."),
                            .line    = lineIndex,
                            .error   = true,
                            .column  = leadingWhitespace + trailingColumn,
                            .length  = std::max(1, trailingLength),
                        });
                        continue;
                    }
                }
            }
            if (!leadingDeclaredKeywordActsLikeSymbol(QStringView{ QStringLiteral("stop") }))
            {
                if (const auto stopMatch = kStopStatementRx.match(line); stopMatch.hasMatch())
                {
                    const QString trailing = stopMatch.captured(1).trimmed();
                    if (!trailing.isEmpty() && trailing != QStringLiteral(";"))
                    {
                        int trailingColumn = static_cast<int>(stopMatch.capturedStart(1));
                        while (trailingColumn < line.size() && line.at(trailingColumn).isSpace())
                        {
                            ++trailingColumn;
                        }
                        int trailingLength = static_cast<int>(line.size()) - trailingColumn;
                        if (const int semicolonPos = line.indexOf(QLatin1Char(';'), trailingColumn); semicolonPos >= 0)
                        {
                            trailingLength = semicolonPos - trailingColumn;
                        }

                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("Stop statement does not take a value or expression."),
                            .line    = lineIndex,
                            .error   = true,
                            .column  = leadingWhitespace + trailingColumn,
                            .length  = std::max(1, trailingLength),
                        });
                        continue;
                    }
                }
            }
            if (!leadingDeclaredKeywordActsLikeSymbol(QStringView{ QStringLiteral("sleep") }))
            {
                if (const auto sleepMatch = kSleepStatementRx.match(line); sleepMatch.hasMatch())
                {
                    int afterKeyword = static_cast<int>(sleepMatch.capturedEnd(0));
                    while (afterKeyword < line.size() && line.at(afterKeyword).isSpace())
                    {
                        ++afterKeyword;
                    }
                    if (afterKeyword >= line.size() || line.at(afterKeyword) != QLatin1Char('('))
                    {
                        const int diagnosticColumn =
                            (afterKeyword < line.size())
                                ? (leadingWhitespace + afterKeyword)
                                : (leadingWhitespace + static_cast<int>(sleepMatch.capturedStart(0)));
                        const int diagnosticLength =
                            (afterKeyword < line.size()) ? 1 : 5;
                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("Sleep statement requires a parenthesized duration."),
                            .line    = lineIndex,
                            .error   = true,
                            .column  = diagnosticColumn,
                            .length  = diagnosticLength,
                        });
                        continue;
                    }
                }
            }
            if (!leadingDeclaredKeywordActsLikeSymbol(QStringView{ QStringLiteral("sleep") }))
            {
                if (const auto emptySleepMatch = kEmptySleepRx.match(line); emptySleepMatch.hasMatch())
                {
                    const int openParenPos = line.indexOf(QLatin1Char('('));
                    const int closeParenPos =
                        static_cast<int>(line.indexOf(QLatin1Char(')'), std::max(0, openParenPos)));
                    const int diagnosticLength =
                        (closeParenPos >= openParenPos) ? (closeParenPos - openParenPos + 1) : 1;
                    out.push_back(CogScriptDiagnostic{
                        .message = QStringLiteral("Sleep statement requires a non-empty duration."),
                        .line    = lineIndex,
                        .error   = true,
                        .column  = leadingWhitespace + std::max(0, openParenPos),
                        .length  = diagnosticLength,
                    });
                    continue;
                }
            }
            if (!leadingDeclaredKeywordActsLikeSymbol(QStringView{ QStringLiteral("sleep") }))
            {
                if (const auto sleepMatch = kSleepStatementRx.match(line); sleepMatch.hasMatch())
                {
                    const int openParenPos = line.indexOf(QLatin1Char('('));
                    const int closeParenPos = findMatchingParen(line, openParenPos);
                    if (closeParenPos >= 0)
                    {
                        const QString trailing = line.mid(closeParenPos + 1).trimmed();
                        if (!trailing.isEmpty() && trailing != QStringLiteral(";"))
                        {
                            int trailingColumn = closeParenPos + 1;
                            while (trailingColumn < line.size() && line.at(trailingColumn).isSpace())
                            {
                                ++trailingColumn;
                            }
                            const int trailingLength =
                                std::max(1, static_cast<int>(line.size()) - trailingColumn);
                            out.push_back(CogScriptDiagnostic{
                                .message = QStringLiteral("Sleep statement does not take text after the duration."),
                                .line    = lineIndex,
                                .error   = true,
                                .column  = leadingWhitespace + trailingColumn,
                                .length  = trailingLength,
                            });
                            continue;
                        }
                    }
                }
            }
            if (const auto forMatch = kForStatementRx.match(line); forMatch.hasMatch())
            {
                const int openParenPos = line.indexOf(QLatin1Char('('),
                                                     static_cast<int>(forMatch.capturedEnd(0)));
                const int closeParenPos = findMatchingParen(line, openParenPos);
                if (openParenPos >= 0 && closeParenPos > openParenPos)
                {
                    int nestedParenDepth = 0;
                    int nestedBracketDepth = 0;
                    int nestedBraceDepth = 0;
                    int separatorCount = 0;
                    bool inSingleQuote = false;
                    bool inDoubleQuote = false;
                    for (int i = openParenPos + 1; i < closeParenPos; ++i)
                    {
                        const QChar ch = line.at(i);
                        const QChar prev = (i > 0) ? line.at(i - 1) : QChar{};
                        if (!inDoubleQuote && ch == QLatin1Char('\'') && prev != QLatin1Char('\\'))
                        {
                            inSingleQuote = !inSingleQuote;
                            continue;
                        }
                        if (!inSingleQuote && ch == QLatin1Char('"') && prev != QLatin1Char('\\'))
                        {
                            inDoubleQuote = !inDoubleQuote;
                            continue;
                        }
                        if (inSingleQuote || inDoubleQuote)
                        {
                            continue;
                        }

                        if (ch == QLatin1Char('('))
                        {
                            ++nestedParenDepth;
                        }
                        else if (ch == QLatin1Char(')'))
                        {
                            nestedParenDepth = std::max(0, nestedParenDepth - 1);
                        }
                        else if (ch == QLatin1Char('['))
                        {
                            ++nestedBracketDepth;
                        }
                        else if (ch == QLatin1Char(']'))
                        {
                            nestedBracketDepth = std::max(0, nestedBracketDepth - 1);
                        }
                        else if (ch == QLatin1Char('{'))
                        {
                            ++nestedBraceDepth;
                        }
                        else if (ch == QLatin1Char('}'))
                        {
                            nestedBraceDepth = std::max(0, nestedBraceDepth - 1);
                        }
                        else if (ch == QLatin1Char(';')
                                 && nestedParenDepth == 0
                                 && nestedBracketDepth == 0
                                 && nestedBraceDepth == 0)
                        {
                            ++separatorCount;
                        }
                    }

                    if (separatorCount != 2)
                    {
                        const int diagnosticLength =
                            (closeParenPos >= openParenPos) ? (closeParenPos - openParenPos + 1) : 1;
                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("For statement requires two ';' separators inside its header."),
                            .line    = lineIndex,
                            .error   = true,
                            .column  = leadingWhitespace + openParenPos,
                            .length  = diagnosticLength,
                        });
                        continue;
                    }
                }
            }
            if (const auto conditionalHeadMatch = kConditionalHeadRx.match(line); conditionalHeadMatch.hasMatch())
            {
                const QString conditionalKeyword = conditionalHeadMatch.captured(1).toLower();
                int afterKeyword = static_cast<int>(conditionalHeadMatch.capturedEnd(1));
                while (afterKeyword < line.size() && line.at(afterKeyword).isSpace())
                {
                    ++afterKeyword;
                }
                if (!leadingDeclaredKeywordActsLikeSymbol(QStringView{ conditionalHeadMatch.captured(1) })
                    && (afterKeyword >= line.size() || line.at(afterKeyword) != QLatin1Char('(')))
                {
                    const int diagnosticColumn =
                        (afterKeyword < line.size())
                            ? (leadingWhitespace + afterKeyword)
                            : (leadingWhitespace + static_cast<int>(conditionalHeadMatch.capturedStart(1)));
                    const int diagnosticLength =
                        (afterKeyword < line.size())
                            ? 1
                            : std::max(1, static_cast<int>(conditionalHeadMatch.capturedLength(1)));
                    out.push_back(CogScriptDiagnostic{
                        .message = QStringLiteral("%1 statement requires a parenthesized condition.")
                            .arg(conditionalKeyword),
                        .line    = lineIndex,
                        .error   = true,
                        .column  = diagnosticColumn,
                        .length  = diagnosticLength,
                    });
                }
            }
            if (const auto emptyConditionalMatch = kEmptyConditionalRx.match(line); emptyConditionalMatch.hasMatch())
            {
                const int openParenPos = line.indexOf(QLatin1Char('('),
                                                     static_cast<int>(emptyConditionalMatch.capturedStart(1))
                                                         + static_cast<int>(emptyConditionalMatch.capturedLength(1)));
                const int closeParenPos =
                    static_cast<int>(line.indexOf(QLatin1Char(')'), std::max(0, openParenPos)));
                const int diagnosticLength =
                    (closeParenPos >= openParenPos) ? (closeParenPos - openParenPos + 1) : 1;
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("%1 statement requires a non-empty condition.")
                        .arg(emptyConditionalMatch.captured(1).toLower()),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = leadingWhitespace + std::max(0, openParenPos),
                    .length  = diagnosticLength,
                });
                continue;
            }
            if (const auto elseIfHeadMatch = kHyphenatedElseIfHeadRx.match(line); elseIfHeadMatch.hasMatch())
            {
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Hyphenated 'else-if' is invalid; write 'else if'."),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = leadingWhitespace + static_cast<int>(elseIfHeadMatch.capturedStart(0)),
                    .length  = std::max(1, static_cast<int>(elseIfHeadMatch.capturedLength(0))),
                });
                continue;
            }
            if (const auto caseHeadMatch = kCaseLabelHeadRx.match(line); caseHeadMatch.hasMatch())
            {
                int afterKeyword = static_cast<int>(caseHeadMatch.capturedEnd(1));
                while (afterKeyword < line.size() && line.at(afterKeyword).isSpace())
                {
                    ++afterKeyword;
                }
                const int colonPos = line.indexOf(QLatin1Char(':'), afterKeyword);
                if (colonPos < 0)
                {
                    out.push_back(CogScriptDiagnostic{
                        .message = QStringLiteral("Case label requires a trailing ':'."),
                        .line    = lineIndex,
                        .error   = true,
                        .column  = leadingWhitespace + std::max(0, static_cast<int>(line.size()) - 1),
                        .length  = 1,
                    });
                    continue;
                }

                if (line.mid(afterKeyword, colonPos - afterKeyword).trimmed().isEmpty())
                {
                    out.push_back(CogScriptDiagnostic{
                        .message = QStringLiteral("Case label requires a value before ':'."),
                        .line    = lineIndex,
                        .error   = true,
                        .column  = leadingWhitespace + colonPos,
                        .length  = 1,
                    });
                    continue;
                }
            }
            if (const auto defaultHeadMatch = kDefaultLabelHeadRx.match(line); defaultHeadMatch.hasMatch())
            {
                int afterKeyword = static_cast<int>(defaultHeadMatch.capturedEnd(1));
                while (afterKeyword < line.size() && line.at(afterKeyword).isSpace())
                {
                    ++afterKeyword;
                }
                const int colonPos = line.indexOf(QLatin1Char(':'), afterKeyword);
                if (colonPos < 0)
                {
                    out.push_back(CogScriptDiagnostic{
                        .message = QStringLiteral("Default label requires a trailing ':'."),
                        .line    = lineIndex,
                        .error   = true,
                        .column  = leadingWhitespace + std::max(0, static_cast<int>(line.size()) - 1),
                        .length  = 1,
                    });
                    continue;
                }

                if (!line.mid(afterKeyword, colonPos - afterKeyword).trimmed().isEmpty())
                {
                    out.push_back(CogScriptDiagnostic{
                        .message = QStringLiteral("Default label should not include a value before ':'."),
                        .line    = lineIndex,
                        .error   = true,
                        .column  = leadingWhitespace + afterKeyword,
                        .length  = std::max(1, colonPos - afterKeyword),
                    });
                    continue;
                }
            }
            if (!(leadingDeclaredKeywordActsLikeSymbol(QStringView{ QStringLiteral("break") })
                  || leadingDeclaredKeywordActsLikeSymbol(QStringView{ QStringLiteral("continue") })))
            {
                if (const auto breakOrContinueMatch = kStrictBreakOrContinueRx.match(line);
                    breakOrContinueMatch.hasMatch())
                {
                    const QString trailing = breakOrContinueMatch.captured(2).trimmed();
                    if (!trailing.isEmpty() && trailing != QStringLiteral(";"))
                    {
                        int trailingColumn = static_cast<int>(breakOrContinueMatch.capturedStart(2));
                        while (trailingColumn < line.size() && line.at(trailingColumn).isSpace())
                        {
                            ++trailingColumn;
                        }
                        int trailingLength = static_cast<int>(line.size()) - trailingColumn;
                        if (const int semicolonPos = line.indexOf(QLatin1Char(';'), trailingColumn); semicolonPos >= 0)
                        {
                            trailingLength = semicolonPos - trailingColumn;
                        }

                        out.push_back(CogScriptDiagnostic{
                            .message = QStringLiteral("%1 statement does not take a value or expression.")
                                .arg(breakOrContinueMatch.captured(1).toLower()),
                            .line    = lineIndex,
                            .error   = true,
                            .column  = leadingWhitespace + trailingColumn,
                            .length  = std::max(1, trailingLength),
                        });
                        continue;
                    }
                }
            }

            QRegularExpressionMatchIterator identIt = kIdentifierRx.globalMatch(analysisLine);
            while (identIt.hasNext())
            {
                const QRegularExpressionMatch match = identIt.next();
                const QString ident = match.captured(0);
                const QString identLower = ident.toLower();
                const int start = static_cast<int>(match.capturedStart(0));
                const int end = start + static_cast<int>(match.capturedLength(0));
                const QStringView before = QStringView{ analysisLine }.left(start);
                const QStringView after = QStringView{ analysisLine }.mid(end);

                if (symbolLines.contains(identLower))
                {
                    usedSymbols.insert(identLower);
                    if (after.trimmed().startsWith(QLatin1Char('[')))
                    {
                        arrayBaseSymbols.insert(identLower);
                    }
                    continue;
                }

                if (kKeywordWords.contains(identLower)
                    || kTypeWords.contains(identLower)
                    || kAttributeWords.contains(identLower)
                    || kGlobalWords.contains(identLower)
                    || knownMessageSet.contains(identLower))
                {
                    continue;
                }

                if (after.trimmed().startsWith(QLatin1Char('(')))
                {
                    continue;
                }

                static const QRegularExpression kCallHeadRx(
                    QStringLiteral("\\bcall\\s*$"),
                    QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
                if (kCallHeadRx.match(before.toString()).hasMatch())
                {
                    continue;
                }
                static const QRegularExpression kGotoHeadRx(
                    QStringLiteral("\\bgoto\\s*$"),
                    QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
                if (kGotoHeadRx.match(before.toString()).hasMatch())
                {
                    continue;
                }

                if (labelLines.contains(identLower) || callableNames.contains(identLower))
                {
                    continue;
                }

                const bool startsLabel =
                    start == 0
                    && after.trimmed().startsWith(QLatin1Char(':'));
                if (startsLabel)
                {
                    continue;
                }

                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Identifier '%1' is used in code but is not declared in symbols.").arg(ident),
                    .line    = lineIndex,
                    .error   = false,
                    .column  = start,
                    .length  = static_cast<int>(match.capturedLength(0)),
                });
            }

            QString flowLine = line;
            while (flowLine.startsWith(QLatin1Char('}')))
            {
                flowLine = flowLine.mid(1).trimmed();
            }
            const QString lowerLine = flowLine.toLower();
            const bool isLabelLine = kLabelRx.match(labelAnalysisLine).hasMatch();
            const bool inlineElseIfBody = hasInlineElseIfTrailingStatement(flowLine);
            const bool inlineElseBody = elseHasInlineStatement(flowLine);
            const bool inlineConditionalBody =
                hasInlineTrailingStatement(flowLine, QStringLiteral("if"))
                || hasInlineTrailingStatement(flowLine, QStringLiteral("while"))
                || hasInlineTrailingStatement(flowLine, QStringLiteral("for"))
                || hasInlineTrailingStatement(flowLine, QStringLiteral("switch"))
                || inlineElseIfBody;
            const auto conditionalHeadMatch = kConditionalHeadRx.match(flowLine);
            const bool startsWithDeclaredConditionalSymbol =
                conditionalHeadMatch.hasMatch()
                && leadingDeclaredKeywordActsLikeSymbol(QStringView{ conditionalHeadMatch.captured(1) });
            const bool standaloneConditionalHead =
                !startsWithDeclaredConditionalSymbol
                &&
                !inlineConditionalBody
                && (lowerLine.startsWith(QStringLiteral("if "))
                    || lowerLine.startsWith(QStringLiteral("if("))
                    || lowerLine.startsWith(QStringLiteral("while "))
                    || lowerLine.startsWith(QStringLiteral("while("))
                    || lowerLine.startsWith(QStringLiteral("for "))
                    || lowerLine.startsWith(QStringLiteral("for("))
                    || lowerLine.startsWith(QStringLiteral("switch "))
                    || lowerLine.startsWith(QStringLiteral("switch(")));
            const bool standaloneElseIfHead =
                !inlineElseIfBody
                && kElseIfHeadRx.match(flowLine).hasMatch();
            const bool standaloneElseHead =
                lowerLine.startsWith(QStringLiteral("else"))
                && !inlineElseBody
                && (standaloneElseIfHead || elseStartsStandaloneBody(flowLine));
            const bool needsSemicolonCheck =
                !isLabelLine
                && !line.endsWith(QLatin1Char(';'))
                && !line.endsWith(QLatin1Char('{'))
                && !line.endsWith(QLatin1Char('}'))
                && !standaloneElseHead
                && !lowerLine.startsWith(QStringLiteral("do"))
                && !standaloneConditionalHead
                && !lineContinuesStatement(rawLine)
                && (kCallStatementRx.match(flowLine).hasMatch()
                    || kGotoStatementRx.match(flowLine).hasMatch()
                    || kBreakOrContinueStatementRx.match(flowLine).hasMatch()
                    || inlineConditionalBody
                    || inlineElseBody
                    || lowerLine.contains(QLatin1Char('='))
                    || lowerLine.startsWith(QStringLiteral("return"))
                    || lowerLine.startsWith(QStringLiteral("stop"))
                    || lowerLine.startsWith(QStringLiteral("sleep"))
                    || lowerLine.contains(QLatin1Char('(')));
            if (needsSemicolonCheck)
            {
                int column = rawLine.size() - 1;
                while (column >= 0 && rawLine.at(column).isSpace())
                {
                    --column;
                }
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Statement appears to be missing a terminating ';'."),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = std::max(0, column),
                    .length  = 1,
                });
            }

            // Parentheses and brackets are allowed to span physical lines.
            // Keep their stacks until a matching closer, section boundary,
            // or end of file instead of diagnosing the final argument/index
            // line merely because that one line has no local continuation
            // marker.
        }
        else
        {
            const auto symbolMatch = kSymbolRx.match(line);
            if (symbolMatch.hasMatch() && kTypeWords.contains(symbolMatch.captured(1).toLower()))
            {
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Symbol declarations must be inside the symbols section."),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = leadingWhitespace + static_cast<int>(symbolMatch.capturedStart(1)),
                    .length  = std::max(1, static_cast<int>(symbolMatch.capturedLength(1))),
                });
                continue;
            }

            if (looksLikeCodeLine(line))
            {
                const auto [tokenColumn, tokenLength] = firstDiagnosticSpan(rawLine);
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Code must be inside the code section."),
                    .line    = lineIndex,
                    .error   = true,
                    .column  = tokenColumn,
                    .length  = tokenLength,
                });
            }
            else
            {
                const auto [tokenColumn, tokenLength] = firstDiagnosticSpan(rawLine);
                out.push_back(CogScriptDiagnostic{
                    .message = QStringLiteral("Line is outside any recognized section."),
                    .line    = lineIndex,
                    .error   = false,
                    .column  = tokenColumn,
                    .length  = tokenLength,
                });
            }
        }
    }

    if (currentSection == Section::code)
    {
        flushPendingOpenParens(std::max(0, static_cast<int>(lines.size()) - 1));
        flushPendingOpenBraces(std::max(0, static_cast<int>(lines.size()) - 1));
        flushPendingOpenBrackets(std::max(0, static_cast<int>(lines.size()) - 1));
    }

    if (!sawSymbols)
    {
        out.push_back(CogScriptDiagnostic{
            .message = QStringLiteral("Missing symbols section."),
            .line    = -1,
            .error   = true,
        });
    }
    if (!sawCode)
    {
        out.push_back(CogScriptDiagnostic{
            .message = QStringLiteral("Missing code section."),
            .line    = -1,
            .error   = true,
        });
    }
    if (currentSection != Section::none)
    {
        out.push_back(CogScriptDiagnostic{
            .message = QStringLiteral("Section '%1' is not closed with 'end'.")
                .arg(
                    currentSection == Section::symbols ? QStringLiteral("symbols")
                                                       : QStringLiteral("code")),
            .line    = currentSectionLine >= 0
                ? currentSectionLine
                : std::max(0, static_cast<int>(lines.size()) - 1),
            .error   = true,
            .column  = std::max(0, currentSectionColumn),
            .length  = std::max(1, currentSectionLength),
        });
    }

    for (const PendingCallRef& callRef : pendingCalls)
    {
        if (!callableNames.contains(callRef.target))
        {
            out.push_back(CogScriptDiagnostic{
                .message = QStringLiteral("Call target '%1' is not defined as a label or flex/float symbol.").arg(callRef.target),
                .line    = callRef.line,
                .error   = true,
                .column  = std::max(0, callRef.column),
                .length  = std::max(1, callRef.length),
            });
        }
    }

    for (const PendingGotoRef& gotoRef : pendingGotos)
    {
        if (!labelLines.contains(gotoRef.target))
        {
            out.push_back(CogScriptDiagnostic{
                .message = QStringLiteral("Goto target '%1' is not defined as a label.").arg(gotoRef.target),
                .line    = gotoRef.line,
                .error   = true,
                .column  = std::max(0, gotoRef.column),
                .length  = std::max(1, gotoRef.length),
            });
        }
    }

    // The editor's generic-label workflow backs a non-message code label with
    // a numeric int/flex/float symbol (normally local). Keep this advisory;
    // built-in message handlers are labels in their own right, and call/goto
    // reference validity is diagnosed independently above.
    for (auto it = labelLines.cbegin(); it != labelLines.cend(); ++it)
    {
        if (knownMessageSet.contains(it.key()))
        {
            continue;
        }
        if (!reservedIdentifierCategory(it.key()).isEmpty())
        {
            continue;
        }

        bool hasApplicableDeclaration = false;
        if (const auto symbolIndex = declaredSymbolIndices.find(it.key());
            symbolIndex != declaredSymbolIndices.end())
        {
            const QString& symbolType = declaredSymbols[
                static_cast<std::size_t>(symbolIndex.value())].typeName;
            hasApplicableDeclaration =
                cogScriptTypeSupportsCodeLabel(symbolType)
                || symbolType == QStringLiteral("message");
        }
        if (hasApplicableDeclaration)
        {
            continue;
        }

        const auto span = labelSpans.find(it.key());
        out.push_back(CogScriptDiagnostic{
            .message = QStringLiteral(
                "Code label '%1' has no matching int/flex/float symbol declaration.")
                .arg(it.key()),
            .line    = it.value(),
            .error   = false,
            .column  = (span != labelSpans.end()) ? span->first : -1,
            .length  = (span != labelSpans.end()) ? span->second : 0,
        });
    }

    for (auto it = labelLines.cbegin(); it != labelLines.cend(); ++it)
    {
        if (!symbolLines.contains(it.key()) || declaredMessages.contains(it.key()))
        {
            continue;
        }

        if (const auto symbolIndexIt = declaredSymbolIndices.find(it.key());
            symbolIndexIt != declaredSymbolIndices.end())
        {
            const QString& symbolType = declaredSymbols[static_cast<std::size_t>(symbolIndexIt.value())].typeName;
            if (cogScriptTypeSupportsCodeLabel(symbolType))
            {
                continue;
            }
        }

        const auto symbolSpanIt = symbolSpans.find(it.key());
        const auto labelSpanIt = labelSpans.find(it.key());
        out.push_back(CogScriptDiagnostic{
            .message = QStringLiteral("Name '%1' is used as both a symbol and a label.").arg(it.key()),
            .line    = symbolLines.value(it.key(), -1),
            .error   = false,
            .column  = (symbolSpanIt != symbolSpans.end()) ? symbolSpanIt->first : -1,
            .length  = (symbolSpanIt != symbolSpans.end()) ? symbolSpanIt->second : 0,
        });
        out.push_back(CogScriptDiagnostic{
            .message = QStringLiteral("Name '%1' is used as both a label and a symbol.").arg(it.key()),
            .line    = it.value(),
            .error   = false,
            .column  = (labelSpanIt != labelSpans.end()) ? labelSpanIt->first : -1,
            .length  = (labelSpanIt != labelSpans.end()) ? labelSpanIt->second : 0,
        });
    }

    for (const QString& baseSymbol : arrayBaseSymbols)
    {
        const auto it = declaredSymbolIndices.find(baseSymbol);
        if (it == declaredSymbolIndices.end())
        {
            continue;
        }

        const int baseIndex = it.value();
        const QString baseType = declaredSymbols[static_cast<std::size_t>(baseIndex)].typeName;
        for (int i = baseIndex; i < static_cast<int>(declaredSymbols.size()); ++i)
        {
            const DeclaredSymbol& decl = declaredSymbols[static_cast<std::size_t>(i)];
            if (decl.typeName != baseType)
            {
                break;
            }
            usedSymbols.insert(decl.name);
        }
    }

    for (const QString& symbolName : knownMessagesDeclaredWithWrongType)
    {
        if (!handledMessages.contains(symbolName))
        {
            continue;
        }

        const auto lineIt = symbolLines.find(symbolName);
        const auto typeSpanIt = symbolTypeSpans.find(symbolName);
        out.push_back(CogScriptDiagnostic{
            .message = QStringLiteral("Known message '%1' should be declared as type 'message'.")
                .arg(symbolName),
            .line    = (lineIt != symbolLines.end()) ? lineIt.value() : -1,
            .error   = false,
            .column  = (typeSpanIt != symbolTypeSpans.end()) ? typeSpanIt->first : -1,
            .length  = (typeSpanIt != symbolTypeSpans.end()) ? typeSpanIt->second : 0,
        });
    }

    for (auto it = symbolLines.cbegin(); it != symbolLines.cend(); ++it)
    {
        if (declaredMessages.contains(it.key()))
        {
            continue;
        }

        if (!usedSymbols.contains(it.key()))
        {
            const auto spanIt = symbolSpans.find(it.key());
            out.push_back(CogScriptDiagnostic{
                .message = QStringLiteral("Symbol '%1' is declared but not used in code.").arg(it.key()),
                .line    = it.value(),
                .error   = false,
                .column  = (spanIt != symbolSpans.end()) ? spanIt->first : -1,
                .length  = (spanIt != symbolSpans.end()) ? spanIt->second : 0,
            });
        }
    }

    return out;
}

} // namespace sed::ui::cog
