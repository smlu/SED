#include "ui/cog/cog_script_symbol_utils.hpp"

#include "ui/cog/cog_script_insert_utils.hpp"
#include "ui/cog/cog_script_language_utils.hpp"
#include "ui/cog/cog_script_text_utils.hpp"

#include <algorithm>
#include <vector>

#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace sed::ui::cog
{
namespace
{

const QSet<QString>& cogScriptTypeWordSet()
{
    static const QSet<QString> kTypeWords = []()
    {
        QSet<QString> out{};
        for (const QString& word : cogScriptTypeWords())
        {
            out.insert(word.toLower());
        }
        return out;
    }();
    return kTypeWords;
}

const QRegularExpression& symbolLineRegex()
{
    static const QRegularExpression kSymbolRx(
        QStringLiteral("^\\s*([A-Za-z0-9_]+)\\s+([A-Za-z_][A-Za-z0-9_]*)\\b"),
        QRegularExpression::UseUnicodePropertiesOption);
    return kSymbolRx;
}

const QRegularExpression& endLineRegex()
{
    static const QRegularExpression kEndRx(
        QStringLiteral("^\\s*end\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    return kEndRx;
}

QRegularExpression attributeRegex(const QString& attributeName)
{
    return QRegularExpression(
        QStringLiteral("(^|[\\s,]+)%1(?:\\s*=\\s*(?:'([^']*)'|\"([^\"]*)\"|([^\\s,#]*)))?(?=[\\s,]|$)")
            .arg(QRegularExpression::escape(attributeName)),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
}

QStringList splitCogLines(const QString& text)
{
    return text.split(QLatin1Char('\n'));
}

struct AttributeOccurrence
{
    int removeStart{-1};
    int removeLength{0};
    int tokenStart{-1};
    int tokenLength{0};
};

std::vector<QRegularExpressionMatch> collectAttributeMatches(
    const QString& code,
    const QString& attributeName)
{
    std::vector<QRegularExpressionMatch> matches{};
    const QRegularExpression attrRx = attributeRegex(attributeName);
    const QString analysis = sanitizeLineForAnalysis(code);
    QRegularExpressionMatchIterator it = attrRx.globalMatch(code);
    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        const int tokenStart = static_cast<int>(match.capturedStart(0)
            + match.capturedLength(1));
        // The regular expression deliberately runs against the authored line so
        // quoted attribute values and their exact spans remain available to the
        // editor.  Accept a token only when the same characters survive the
        // quote/comment sanitizer; otherwise text such as
        // `desc="keep mask=7 local"` would be mistaken for real attributes.
        if (tokenStart < 0
            || tokenStart + attributeName.size() > analysis.size()
            || analysis.mid(tokenStart, attributeName.size()).compare(
                    attributeName, Qt::CaseInsensitive) != 0)
        {
            continue;
        }
        matches.push_back(match);
    }
    return matches;
}

std::optional<QString> replaceLineInScript(QStringList lines, const int line, const QString& replacement)
{
    if (line < 0 || line >= lines.size())
    {
        return std::nullopt;
    }

    lines[line] = replacement;
    return lines.join(QLatin1Char('\n'));
}

void splitRawLineAndComment(QString& rawLine, QString& commentSuffix)
{
    const int commentStart = findCommentStart(rawLine);
    int suffixStart = commentStart >= 0 ? commentStart : rawLine.size();
    while (suffixStart > 0
        && (rawLine.at(suffixStart - 1) == QLatin1Char(' ')
            || rawLine.at(suffixStart - 1) == QLatin1Char('\t')))
    {
        --suffixStart;
    }
    commentSuffix = rawLine.mid(suffixStart);
    rawLine = rawLine.left(suffixStart);
}

AttributeOccurrence attributeOccurrence(
    const QString& rawLine,
    const QRegularExpressionMatch& match)
{
    const QString leadingSeparator = match.captured(1);
    const int matchStart = static_cast<int>(match.capturedStart(0));
    const int tokenStart = matchStart + static_cast<int>(leadingSeparator.size());
    const int tokenEnd = matchStart + static_cast<int>(match.capturedLength(0));
    int removeStart = tokenStart;
    int removeEnd = tokenEnd;

    // Keep the separator before a non-final attribute and consume only the
    // delimiter after it. This repairs the local edit boundary without ever
    // normalizing unrelated indentation, quoted desc text, or comment space.
    if (removeEnd < rawLine.size() && rawLine.at(removeEnd) == QLatin1Char(','))
    {
        ++removeEnd;
    }
    while (removeEnd < rawLine.size()
        && (rawLine.at(removeEnd) == QLatin1Char(' ')
            || rawLine.at(removeEnd) == QLatin1Char('\t')))
    {
        ++removeEnd;
    }

    if (removeEnd == tokenEnd)
    {
        // A final attribute owns its leading separator; removing it must not
        // leave a dangling comma or trailing field separator.
        removeStart = matchStart;
    }

    return AttributeOccurrence{
        removeStart,
        removeEnd - removeStart,
        tokenStart,
        tokenEnd - tokenStart,
    };
}

QString appendCommentSuffix(const QString& rawLine, const QString& commentSuffix)
{
    if (commentSuffix.isEmpty())
    {
        return rawLine;
    }
    if (rawLine.isEmpty())
    {
        return commentSuffix;
    }
    return rawLine + commentSuffix;
}

std::vector<AttributeOccurrence> collectAttributeOccurrences(const QString& rawLine, const QString& attributeName)
{
    std::vector<AttributeOccurrence> occurrences{};
    for (const QRegularExpressionMatch& match
        : collectAttributeMatches(rawLine, attributeName))
    {
        occurrences.push_back(attributeOccurrence(rawLine, match));
    }
    return occurrences;
}

} // namespace

std::optional<CogScriptSymbolLineContext> cogScriptSymbolLineContextAtLine(
    const QString& text,
    const int targetLine)
{
    if (targetLine < 0)
    {
        return std::nullopt;
    }

    const CogScriptSectionsInfo sections = scanCogScriptSections(text);
    if (sections.symbolsLine < 0 || sections.symbolsEndLine < 0
        || targetLine <= sections.symbolsLine || targetLine > sections.symbolsEndLine)
    {
        return std::nullopt;
    }

    const QStringList lines = splitCogLines(text);
    const QString trimmed = stripComment(lines.value(targetLine)).trimmed();
    if (trimmed.isEmpty() || endLineRegex().match(trimmed).hasMatch())
    {
        return std::nullopt;
    }

    const auto match = symbolLineRegex().match(trimmed);
    if (!match.hasMatch())
    {
        return std::nullopt;
    }

    const QString typeName = match.captured(1).trimmed();
    if (!cogScriptTypeWordSet().contains(typeName.toLower()))
    {
        return std::nullopt;
    }

    return CogScriptSymbolLineContext{
        .line       = targetLine,
        .typeName   = typeName,
        .identifier = match.captured(2).trimmed(),
    };
}

std::optional<CogScriptSymbolLineAttributeMatch> cogScriptSymbolLineAttributeMatch(
    const QString& text,
    const CogScriptSymbolLineContext& context,
    const QString& attributeName,
    const std::optional<int> column)
{
    if (context.line < 0)
    {
        return std::nullopt;
    }

    const QStringList lines = splitCogLines(text);
    QString code = stripComment(lines.value(context.line));
    while (!code.isEmpty()
        && (code.endsWith(QLatin1Char(' '))
            || code.endsWith(QLatin1Char('\t'))))
    {
        code.chop(1);
    }
    for (const QRegularExpressionMatch& match
        : collectAttributeMatches(code, attributeName))
    {
        const QString leadingSeparator = match.captured(1);
        const int tokenStart = static_cast<int>(match.capturedStart(0)) + static_cast<int>(leadingSeparator.size());
        const int tokenLength = static_cast<int>(match.capturedLength(0) - leadingSeparator.size());
        if (column && (tokenStart < 0 || tokenLength <= 0 || *column < tokenStart || *column >= (tokenStart + tokenLength)))
        {
            continue;
        }

        // Step 1: edit only the clicked attribute and its local delimiter.
        const AttributeOccurrence occurrence = attributeOccurrence(code, match);

        // Step 2: capture unquoted, single-quoted, or double-quoted values.
        QString value{};
        int valueStart = -1;
        int valueLength = 0;
        for (int group = 2; group <= 4; ++group)
        {
            const int capturedStart = match.capturedStart(group);
            if (capturedStart < 0)
            {
                continue;
            }

            value = match.captured(group);
            valueStart = capturedStart;
            valueLength = static_cast<int>(match.capturedLength(group));
            if ((group == 2 || group == 3) && valueStart > 0)
            {
                const int valueEnd = valueStart + valueLength;
                if (valueEnd < code.size()
                    && (code.at(valueStart - 1) == QLatin1Char('\'')
                        || code.at(valueStart - 1) == QLatin1Char('"'))
                    && code.at(valueEnd) == code.at(valueStart - 1))
                {
                    --valueStart;
                    valueLength += 2;
                }
            }
            break;
        }

        return CogScriptSymbolLineAttributeMatch{
            attributeName,
            value,
            occurrence.removeStart,
            occurrence.removeLength,
            tokenStart,
            tokenLength,
            tokenStart,
            tokenLength,
            valueStart,
            valueLength,
        };
    }

    return std::nullopt;
}

std::optional<QString> cogScriptSymbolLineAttributeValue(
    const QString& text,
    const CogScriptSymbolLineContext& context,
    const QString& attributeName,
    const std::optional<int> column)
{
    const auto match = cogScriptSymbolLineAttributeMatch(text, context, attributeName, column);
    if (!match)
    {
        return std::nullopt;
    }
    return match->value;
}

bool cogScriptSymbolLineHasAttribute(
    const QString& text,
    const CogScriptSymbolLineContext& context,
    const QString& attributeName)
{
    return cogScriptSymbolLineAttributeMatch(text, context, attributeName).has_value();
}

int cogScriptSymbolLineAttributeCount(
    const QString& text,
    const CogScriptSymbolLineContext& context,
    const QString& attributeName)
{
    if (context.line < 0)
    {
        return 0;
    }

    const QStringList lines = splitCogLines(text);
    const QString code = stripComment(lines.value(context.line));
    return static_cast<int>(collectAttributeMatches(code, attributeName).size());
}

std::optional<CogScriptSymbolLineAttributeMatch> cogScriptSymbolLineAttributeAtColumn(
    const QString& text,
    const CogScriptSymbolLineContext& context,
    const int column)
{
    if (context.line < 0 || column < 0)
    {
        return std::nullopt;
    }

    static const QStringList kKnownAttributes{
        QStringLiteral("local"),
        QStringLiteral("desc"),
        QStringLiteral("mask"),
        QStringLiteral("linkid"),
        QStringLiteral("nolink"),
    };

    for (const QString& attributeName : kKnownAttributes)
    {
        if (const auto match = cogScriptSymbolLineAttributeMatch(text, context, attributeName, column))
        {
            return match;
        }
    }

    return std::nullopt;
}

bool cogScriptSymbolLineAttributeValueHit(
    const CogScriptSymbolLineAttributeMatch& match,
    const int column)
{
    return match.valueStart >= 0
        && match.valueLength > 0
        && column >= match.valueStart
        && column < (match.valueStart + match.valueLength);
}

std::optional<QString> removeCogScriptSymbolLineAttribute(
    const QString& text,
    const CogScriptSymbolLineContext& context,
    const QString& attributeName,
    const std::optional<int> column)
{
    QStringList lines = splitCogLines(text);
    if (context.line < 0 || context.line >= lines.size())
    {
        return std::nullopt;
    }

    QString rawLine = lines[context.line];
    QString commentSuffix{};
    splitRawLineAndComment(rawLine, commentSuffix);

    const auto match = cogScriptSymbolLineAttributeMatch(text, context, attributeName, column);
    if (!match || match->removeStart < 0 || match->removeLength <= 0)
    {
        return std::nullopt;
    }

    rawLine.remove(match->removeStart, match->removeLength);
    return replaceLineInScript(lines, context.line, appendCommentSuffix(rawLine, commentSuffix));
}

std::optional<QString> dedupeCogScriptSymbolLineAttribute(
    const QString& text,
    const CogScriptSymbolLineContext& context,
    const QString& attributeName,
    const std::optional<int> keepColumn)
{
    QStringList lines = splitCogLines(text);
    if (context.line < 0 || context.line >= lines.size())
    {
        return std::nullopt;
    }

    QString rawLine = lines[context.line];
    QString commentSuffix{};
    splitRawLineAndComment(rawLine, commentSuffix);

    const std::vector<AttributeOccurrence> occurrences = collectAttributeOccurrences(rawLine, attributeName);
    if (occurrences.size() <= 1)
    {
        return std::nullopt;
    }

    int keepIndex = 0;
    if (keepColumn)
    {
        for (int index = 0; index < static_cast<int>(occurrences.size()); ++index)
        {
            const AttributeOccurrence& occurrence = occurrences[static_cast<std::size_t>(index)];
            if (occurrence.tokenStart >= 0
                && occurrence.tokenLength > 0
                && *keepColumn >= occurrence.tokenStart
                && *keepColumn < (occurrence.tokenStart + occurrence.tokenLength))
            {
                keepIndex = index;
                break;
            }
        }
    }

    for (int index = static_cast<int>(occurrences.size()) - 1; index >= 0; --index)
    {
        if (index == keepIndex)
        {
            continue;
        }

        const AttributeOccurrence& occurrence = occurrences[static_cast<std::size_t>(index)];
        rawLine.remove(occurrence.removeStart, occurrence.removeLength);
    }

    return replaceLineInScript(lines, context.line, appendCommentSuffix(rawLine, commentSuffix));
}

std::optional<QString> upsertCogScriptSymbolLineAttribute(
    const QString& text,
    const CogScriptSymbolLineContext& context,
    const QString& attributeName,
    const std::optional<QString>& attributeValue,
    const std::optional<int> column)
{
    QStringList lines = splitCogLines(text);
    if (context.line < 0 || context.line >= lines.size())
    {
        return std::nullopt;
    }

    QString rawLine = lines[context.line];
    QString commentSuffix{};
    splitRawLineAndComment(rawLine, commentSuffix);
    const QString replacement = attributeValue
        ? QStringLiteral("%1=%2").arg(attributeName, *attributeValue)
        : attributeName;

    const auto match = cogScriptSymbolLineAttributeMatch(text, context, attributeName, column);
    if (match && match->replaceStart >= 0 && match->replaceLength > 0)
    {
        rawLine.replace(match->replaceStart, match->replaceLength, replacement);
    }
    else
    {
        rawLine += rawLine.trimmed().isEmpty()
            ? replacement
            : QStringLiteral(" ") + replacement;
    }

    return replaceLineInScript(lines, context.line, appendCommentSuffix(rawLine, commentSuffix));
}

} // namespace sed::ui::cog
