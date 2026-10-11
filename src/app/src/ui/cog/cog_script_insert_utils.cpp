#include "ui/cog/cog_script_insert_utils.hpp"

#include "ui/cog/cog_script_text_utils.hpp"

#include <algorithm>

#include <QRegularExpression>
#include <QStringList>

namespace sed::ui::cog
{
namespace
{

enum class ScanSection
{
    none,
    symbols,
    code,
};

const QRegularExpression& flagsLineRegex()
{
    static const QRegularExpression kFlagsRx(
        // Treat a malformed leading flags declaration as present so Insert
        // and the flags editor repair it in place instead of creating a
        // second top-level flags line.
        QStringLiteral("^\\s*flags\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    return kFlagsRx;
}

const QRegularExpression& topLevelSectionRegex()
{
    static const QRegularExpression kSectionRx(
        QStringLiteral("^\\s*(symbols|code)\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    return kSectionRx;
}

const QRegularExpression& endLineRegex()
{
    static const QRegularExpression kEndRx(
        QStringLiteral("^\\s*end\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    return kEndRx;
}

const QRegularExpression& codeLabelRegex()
{
    static const QRegularExpression kLabelRx(
        QStringLiteral("^\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*:"),
        QRegularExpression::UseUnicodePropertiesOption);
    return kLabelRx;
}

QStringList splitCogLines(const QString& text)
{
    return text.split(QLatin1Char('\n'));
}

QString trimmedCodeLine(const QStringList& lines, const int line)
{
    return stripComment(lines.value(line)).trimmed();
}

} // namespace

CogScriptTextEditSelection mapCogScriptTextEditSelection(
    const QString& oldText,
    const QString& newText,
    const int anchor,
    const int position)
{
    const int oldLength = static_cast<int>(oldText.size());
    const int newLength = static_cast<int>(newText.size());
    int prefixLength = 0;
    while (prefixLength < oldLength
        && prefixLength < newLength
        && oldText.at(prefixLength) == newText.at(prefixLength))
    {
        ++prefixLength;
    }

    int suffixLength = 0;
    while (suffixLength < oldLength - prefixLength
        && suffixLength < newLength - prefixLength
        && oldText.at(oldLength - suffixLength - 1)
            == newText.at(newLength - suffixLength - 1))
    {
        ++suffixLength;
    }

    const int oldChangedEnd = oldLength - suffixLength;
    const int newChangedEnd = newLength - suffixLength;
    const int newChangedLength = newChangedEnd - prefixLength;
    const int lengthDelta = newLength - oldLength;
    const auto mapEndpoint = [&](const int rawPosition)
    {
        const int endpoint = std::clamp(rawPosition, 0, oldLength);
        if (endpoint < prefixLength)
        {
            return endpoint;
        }
        if (endpoint >= oldChangedEnd)
        {
            return std::clamp(endpoint + lengthDelta, 0, newLength);
        }
        return prefixLength + std::min(
            endpoint - prefixLength,
            newChangedLength);
    };

    return {
        mapEndpoint(anchor),
        mapEndpoint(position),
    };
}

CogScriptSectionsInfo scanCogScriptSections(const QString& text)
{
    CogScriptSectionsInfo info{};
    const QStringList lines = splitCogLines(text);
    ScanSection currentSection = ScanSection::none;

    // Step 1: walk top-to-bottom and remember the first occurrence of each
    // structural marker. Later duplicate diagnostics are handled elsewhere.
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
    {
        const QString line = trimmedCodeLine(lines, lineIndex);
        if (line.isEmpty())
        {
            continue;
        }

        if (flagsLineRegex().match(line).hasMatch())
        {
            if (info.flagsLine < 0)
            {
                info.flagsLine = lineIndex;
            }
            continue;
        }

        if (const auto match = topLevelSectionRegex().match(line); match.hasMatch())
        {
            const QString sectionName = match.captured(1).toLower();
            if (sectionName == QStringLiteral("symbols"))
            {
                if (currentSection == ScanSection::code && info.codeEndLine < 0)
                {
                    // A new top-level header closes a malformed section that
                    // omitted `end`; insertion must never leak across it.
                    info.codeEndLine = lineIndex;
                }
                if (info.symbolsLine < 0)
                {
                    info.symbolsLine = lineIndex;
                }
                currentSection = ScanSection::symbols;
            }
            else
            {
                if (currentSection == ScanSection::symbols && info.symbolsEndLine < 0)
                {
                    info.symbolsEndLine = lineIndex;
                }
                if (info.codeLine < 0)
                {
                    info.codeLine = lineIndex;
                }
                currentSection = ScanSection::code;
            }
            continue;
        }

        if (endLineRegex().match(line).hasMatch())
        {
            if (currentSection == ScanSection::symbols && info.symbolsEndLine < 0)
            {
                info.symbolsEndLine = lineIndex;
            }
            else if (currentSection == ScanSection::code && info.codeEndLine < 0)
            {
                info.codeEndLine = lineIndex;
            }
            currentSection = ScanSection::none;
        }
    }

    // Step 2: unfinished sections behave as if they end at EOF.
    const int lineCount = lines.size();
    if (info.symbolsLine >= 0 && info.symbolsEndLine < 0)
    {
        info.symbolsEndLine = lineCount;
    }
    if (info.codeLine >= 0 && info.codeEndLine < 0)
    {
        info.codeEndLine = lineCount;
    }
    return info;
}

CogScriptInsertContext cogScriptInsertContextAtLine(const QString& text, const int targetLine)
{
    const CogScriptSectionsInfo sections = scanCogScriptSections(text);
    if (sections.flagsLine >= 0 && targetLine == sections.flagsLine)
    {
        return CogScriptInsertContext::flags;
    }
    if (sections.symbolsLine >= 0
        && sections.symbolsEndLine >= 0
        && targetLine >= sections.symbolsLine
        && targetLine <= sections.symbolsEndLine)
    {
        return CogScriptInsertContext::symbols;
    }
    if (sections.codeLine >= 0
        && sections.codeEndLine >= 0
        && targetLine >= sections.codeLine
        && targetLine <= sections.codeEndLine)
    {
        return CogScriptInsertContext::code;
    }

    return CogScriptInsertContext::outside;
}

std::vector<CogScriptInsertContext> buildCogScriptShortcutInsertContexts(const QString& text)
{
    static const QRegularExpression kReturnRx(
        QStringLiteral("^return\\s*;\\s*(?:}\\s*)*$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kControlRx(
        QStringLiteral("^(?:(?:else\\s+)?(?:if|while|for|switch)\\b|else\\b|do\\b)"),
        QRegularExpression::CaseInsensitiveOption);

    const QStringList lines = splitCogLines(text);
    QStringList structuralLines{};
    QStringList analysisLines{};
    structuralLines.reserve(lines.size());
    analysisLines.reserve(lines.size());
    for ( const QString& line : lines )
    {
        structuralLines.push_back(stripComment(line).trimmed());
        analysisLines.push_back(sanitizeLineForAnalysis(line).trimmed());
    }
    std::vector<CogScriptInsertContext> contexts(
        static_cast<std::size_t>(lines.size()), CogScriptInsertContext::outside);

    // Cache successors once, so checking whether a return is the final
    // statement never scans a long comment/blank gap from every source line.
    std::vector<int> nextSubstantiveLine(static_cast<std::size_t>(lines.size()),
        static_cast<int>(lines.size()));
    int nextLine = static_cast<int>(lines.size());
    for ( int line = static_cast<int>(lines.size()) - 1; line >= 0; --line )
    {
        nextSubstantiveLine[static_cast<std::size_t>(line)] = nextLine;
        if ( !structuralLines.at(line).isEmpty() )
        {
            nextLine = line;
        }
    }
    const auto finalReturnHasBoundary = [&](const int line)
    {
        int following = nextSubstantiveLine[static_cast<std::size_t>(line)];
        while ( following < lines.size() )
        {
            const QString& analysis = analysisLines.at(following);
            const bool onlyClosingBraces = !analysis.isEmpty()
                && std::all_of(analysis.cbegin(), analysis.cend(), [](const QChar ch)
                    { return ch.isSpace() || ch == QLatin1Char('}'); });
            if ( !onlyClosingBraces )
            {
                return structuralLines.at(following).compare(
                           QStringLiteral("end"), Qt::CaseInsensitive) == 0
                    || codeLabelRegex().match(analysis).hasMatch();
            }
            following = nextSubstantiveLine[static_cast<std::size_t>(following)];
        }
        return true;
    };
    const auto standaloneControlHeader = [](const QString& analysis)
    {
        const int open = static_cast<int>(analysis.indexOf(QLatin1Char('(')));
        if ( open < 0 )
        {
            return true;
        }
        int depth = 0;
        for ( int column = open; column < analysis.size(); ++column )
        {
            if ( analysis.at(column) == QLatin1Char('(') )
            {
                ++depth;
            }
            else if ( analysis.at(column) == QLatin1Char(')') && --depth == 0 )
            {
                return analysis.mid(column + 1).trimmed().isEmpty();
            }
        }
        return true;
    };

    ScanSection section = ScanSection::none;
    bool activeHandler = false;
    bool unexpectedCode = false;
    bool firstHandlerStatement = false;
    bool handlerOuterBrace = false;
    bool finalHandlerReturn = false;
    bool pendingControlBody = false;
    int braceDepth = 0;
    int parenDepth = 0;
    int bracketDepth = 0;
    const auto resetCodeState = [&]()
    {
        activeHandler = false;
        unexpectedCode = false;
        firstHandlerStatement = false;
        handlerOuterBrace = false;
        finalHandlerReturn = false;
        pendingControlBody = false;
        braceDepth = 0;
        parenDepth = 0;
        bracketDepth = 0;
    };
    for ( int line = 0; line < lines.size(); ++line )
    {
        const QString& structural = structuralLines.at(line);
        QString analysis = analysisLines.at(line);
        auto& context = contexts[static_cast<std::size_t>(line)];
        if ( structural.compare(QStringLiteral("symbols"), Qt::CaseInsensitive) == 0 )
        {
            section = ScanSection::symbols;
            resetCodeState();
            context = CogScriptInsertContext::symbols;
            continue;
        }
        if ( structural.compare(QStringLiteral("code"), Qt::CaseInsensitive) == 0 )
        {
            section = ScanSection::code;
            resetCodeState();
            context = CogScriptInsertContext::code;
            continue;
        }
        if ( structural.compare(QStringLiteral("end"), Qt::CaseInsensitive) == 0 )
        {
            if ( section == ScanSection::symbols )
            {
                context = CogScriptInsertContext::symbols;
            }
            else if ( section == ScanSection::code && !activeHandler
                && !unexpectedCode && braceDepth == 0 )
            {
                context = CogScriptInsertContext::code;
            }
            section = ScanSection::none;
            resetCodeState();
            continue;
        }
        if ( section == ScanSection::symbols )
        {
            context = CogScriptInsertContext::symbols;
            continue;
        }
        if ( section != ScanSection::code )
        {
            continue;
        }
        if ( structural.isEmpty() )
        {
            if ( !activeHandler && !unexpectedCode && braceDepth == 0 )
            {
                context = CogScriptInsertContext::code;
            }
            continue;
        }
        if ( const auto label = codeLabelRegex().match(analysis); label.hasMatch() )
        {
            activeHandler = true;
            unexpectedCode = false;
            firstHandlerStatement = true;
            finalHandlerReturn = false;
            pendingControlBody = false;
            handlerOuterBrace = false;
            analysis = analysis.mid(label.capturedEnd()).trimmed();
            if ( analysis.isEmpty() )
            {
                continue;
            }
        }
        if ( !activeHandler )
        {
            unexpectedCode = true;
        }
        if ( activeHandler && firstHandlerStatement )
        {
            handlerOuterBrace = braceDepth == 0 && analysis.startsWith(QLatin1Char('{'));
            firstHandlerStatement = false;
        }
        const int handlerBodyDepth = handlerOuterBrace ? 1 : 0;
        if ( activeHandler && !pendingControlBody
            && braceDepth == handlerBodyDepth && parenDepth == 0 && bracketDepth == 0
            && kReturnRx.match(analysis).hasMatch() && finalReturnHasBoundary(line) )
        {
            finalHandlerReturn = true;
        }
        const bool continuingControlHeader = pendingControlBody && parenDepth > 0;
        const bool controlHeader = kControlRx.match(analysis).hasMatch();
        if ( controlHeader )
        {
            pendingControlBody = standaloneControlHeader(analysis);
        }
        else if ( !continuingControlHeader )
        {
            pendingControlBody = false;
        }
        for ( const QChar ch : analysis )
        {
            if ( ch == QLatin1Char('{') )
            {
                ++braceDepth;
            }
            else if ( ch == QLatin1Char('}') )
            {
                --braceDepth;
            }
            else if ( ch == QLatin1Char('(') )
            {
                ++parenDepth;
            }
            else if ( ch == QLatin1Char(')') )
            {
                --parenDepth;
            }
            else if ( ch == QLatin1Char('[') )
            {
                ++bracketDepth;
            }
            else if ( ch == QLatin1Char(']') )
            {
                --bracketDepth;
            }
        }
        if ( finalHandlerReturn && braceDepth == 0 )
        {
            activeHandler = false;
            finalHandlerReturn = false;
            handlerOuterBrace = false;
        }
    }
    return contexts;
}

int preferredCogScriptSymbolInsertLine(const QString& text, const std::optional<int> lineHint)
{
    const CogScriptSectionsInfo sections = scanCogScriptSections(text);
    if (sections.symbolsEndLine < 0)
    {
        return -1;
    }
    if (!lineHint || *lineHint <= sections.symbolsLine || *lineHint > sections.symbolsEndLine)
    {
        return sections.symbolsEndLine;
    }

    const QStringList lines = splitCogLines(text);
    const int clampedLine = std::clamp(*lineHint, sections.symbolsLine + 1, sections.symbolsEndLine);
    const QString trimmed = trimmedCodeLine(lines, clampedLine);
    if (trimmed.isEmpty() || endLineRegex().match(trimmed).hasMatch())
    {
        return clampedLine;
    }
    return std::min(clampedLine + 1, sections.symbolsEndLine);
}

int preferredCogScriptMessageSymbolInsertLine(const QString& text, const std::optional<int> lineHint)
{
    static const QRegularExpression kMessageRx(
        QStringLiteral("^\\s*message\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);

    const CogScriptSectionsInfo sections = scanCogScriptSections(text);
    if (sections.symbolsEndLine < 0)
    {
        return -1;
    }

    const QStringList lines = splitCogLines(text);
    int lastMessageLine = -1;
    for (int lineIndex = sections.symbolsLine + 1; lineIndex < sections.symbolsEndLine; ++lineIndex)
    {
        const QString trimmed = trimmedCodeLine(lines, lineIndex);
        if (kMessageRx.match(trimmed).hasMatch())
        {
            lastMessageLine = lineIndex;
        }
    }

    if (lastMessageLine >= 0)
    {
        return std::min(lastMessageLine + 1, sections.symbolsEndLine);
    }

    return preferredCogScriptSymbolInsertLine(text, lineHint);
}

int preferredCogScriptCodeInsertLine(const QString& text, const std::optional<int> lineHint)
{
    const CogScriptSectionsInfo sections = scanCogScriptSections(text);
    if (sections.codeEndLine < 0)
    {
        return -1;
    }
    if (!lineHint || *lineHint <= sections.codeLine || *lineHint > sections.codeEndLine)
    {
        return sections.codeEndLine;
    }

    const QStringList lines = splitCogLines(text);
    const int clampedLine = std::clamp(*lineHint, sections.codeLine + 1, sections.codeEndLine);
    const QString trimmed = trimmedCodeLine(lines, clampedLine);

    int currentLabelLine = -1;
    for (int lineIndex = sections.codeLine + 1; lineIndex <= clampedLine && lineIndex < sections.codeEndLine; ++lineIndex)
    {
        const QString currentTrimmed = trimmedCodeLine(lines, lineIndex);
        if (codeLabelRegex().match(currentTrimmed).hasMatch())
        {
            currentLabelLine = lineIndex;
        }
    }

    int nextLabelLine = sections.codeEndLine;
    for (int lineIndex = clampedLine + 1; lineIndex < sections.codeEndLine; ++lineIndex)
    {
        const QString nextTrimmed = trimmedCodeLine(lines, lineIndex);
        if (codeLabelRegex().match(nextTrimmed).hasMatch())
        {
            nextLabelLine = lineIndex;
            break;
        }
    }

    if (currentLabelLine >= 0)
    {
        return nextLabelLine;
    }

    if (trimmed.isEmpty() || codeLabelRegex().match(trimmed).hasMatch())
    {
        return clampedLine;
    }

    return nextLabelLine;
}

int preferredCogScriptSectionInsertLine(
    const QString& text,
    const QString& sectionName,
    const std::optional<int> lineHint)
{
    const CogScriptSectionsInfo sections = scanCogScriptSections(text);

    if (sectionName.compare(QStringLiteral("flags"), Qt::CaseInsensitive) == 0)
    {
        if (sections.flagsLine >= 0)
        {
            return -1;
        }
        const int defaultInsertLine = (sections.symbolsLine >= 0)
            ? sections.symbolsLine
            : (sections.codeLine >= 0 ? sections.codeLine : 0);
        if (!lineHint)
        {
            return defaultInsertLine;
        }
        const int maxLine = std::max(0, defaultInsertLine);
        return std::clamp(*lineHint, 0, maxLine);
    }

    if (sectionName.compare(QStringLiteral("symbols"), Qt::CaseInsensitive) == 0)
    {
        if (sections.symbolsLine >= 0)
        {
            return -1;
        }
        const int defaultInsertLine = (sections.codeLine >= 0)
            ? sections.codeLine
            : (sections.flagsLine >= 0 ? sections.flagsLine + 1 : 0);
        const int minLine = (sections.flagsLine >= 0) ? sections.flagsLine + 1 : 0;
        if (!lineHint)
        {
            return defaultInsertLine;
        }
        return std::clamp(*lineHint, minLine, std::max(minLine, defaultInsertLine));
    }

    if (sectionName.compare(QStringLiteral("code"), Qt::CaseInsensitive) == 0)
    {
        if (sections.codeLine >= 0)
        {
            return -1;
        }
        const int defaultInsertLine = (sections.symbolsEndLine >= 0)
            ? sections.symbolsEndLine + 1
            : (sections.flagsLine >= 0 ? sections.flagsLine + 1 : -1);
        const int minLine = (sections.symbolsEndLine >= 0)
            ? sections.symbolsEndLine + 1
            : ((sections.flagsLine >= 0) ? sections.flagsLine + 1 : -1);
        if (!lineHint)
        {
            return defaultInsertLine;
        }
        return std::max(minLine, *lineHint);
    }

    return -1;
}

} // namespace sed::ui::cog
