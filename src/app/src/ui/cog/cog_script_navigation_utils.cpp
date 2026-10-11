#include "ui/cog/cog_script_navigation_utils.hpp"

#include <QRegularExpression>
#include <QSet>
#include <QTextCursor>
#include <QTextDocument>

#include "ui/cog/cog_script_language_utils.hpp"
#include "ui/cog/cog_script_text_utils.hpp"

namespace sed::ui::cog
{

namespace
{

enum class CogScriptSection
{
    none,
    symbols,
    code,
};

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

const QRegularExpression& cogScriptSectionRx()
{
    static const QRegularExpression kSectionRx(
        QStringLiteral("^\\s*(symbols|code)\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    return kSectionRx;
}

const QRegularExpression& cogScriptEndRx()
{
    static const QRegularExpression kEndRx(
        QStringLiteral("^\\s*end\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    return kEndRx;
}

const QRegularExpression& cogScriptSymbolRx()
{
    static const QRegularExpression kSymbolRx(
        QStringLiteral("^\\s*([A-Za-z0-9_]+)\\s+([A-Za-z_][A-Za-z0-9_]*)\\b"),
        QRegularExpression::UseUnicodePropertiesOption);
    return kSymbolRx;
}

const QRegularExpression& cogScriptLabelRx()
{
    static const QRegularExpression kLabelRx(
        QStringLiteral("^\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*:"),
        QRegularExpression::UseUnicodePropertiesOption);
    return kLabelRx;
}

CogScriptSection sectionFromLine(const QString& line, CogScriptSection current)
{
    if (const auto match = cogScriptSectionRx().match(line); match.hasMatch())
    {
        const QString sectionName = match.captured(1).toLower();
        return (sectionName == QStringLiteral("symbols")) ? CogScriptSection::symbols : CogScriptSection::code;
    }

    if (cogScriptEndRx().match(line).hasMatch())
    {
        return CogScriptSection::none;
    }

    return current;
}

} // namespace

std::vector<CogScriptTextMatch> findCogScriptTextMatches(
    const QTextDocument& document,
    const QString& needle,
    const bool caseSensitive,
    const bool wholeWords)
{
    if (needle.isEmpty())
    {
        return {};
    }

    QTextDocument::FindFlags flags{};
    if (caseSensitive)
    {
        flags |= QTextDocument::FindCaseSensitively;
    }
    if (wholeWords)
    {
        flags |= QTextDocument::FindWholeWords;
    }

    std::vector<CogScriptTextMatch> matches{};
    int nextPosition = 0;
    while (true)
    {
        const QTextCursor match = document.find(needle, nextPosition, flags);
        if (match.isNull())
        {
            break;
        }

        const int start = match.selectionStart();
        const int end = match.selectionEnd();
        if (end <= start)
        {
            break;
        }
        matches.push_back(CogScriptTextMatch{ start, end - start });
        nextPosition = end;
    }
    return matches;
}

std::optional<int> findCogScriptDefinitionLine(const QString& text, const QString& identifier)
{
    const QString target = identifier.trimmed().toLower();
    if (target.isEmpty())
    {
        return std::nullopt;
    }

    CogScriptSection currentSection = CogScriptSection::none;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
    {
        const QString line = stripComment(lines[lineIndex]).trimmed();
        if (line.isEmpty())
        {
            continue;
        }

        const CogScriptSection nextSection = sectionFromLine(line, currentSection);
        if (nextSection != currentSection || cogScriptEndRx().match(line).hasMatch())
        {
            currentSection = nextSection;
            continue;
        }

        // Step 1: symbol definitions are valid only in the symbols section and only for known COG types.
        if (currentSection == CogScriptSection::symbols)
        {
            const auto match = cogScriptSymbolRx().match(line);
            if (match.hasMatch()
                && cogScriptTypeWordSet().contains(match.captured(1).toLower())
                && match.captured(2).toLower() == target)
            {
                return lineIndex;
            }
        }
        // Step 2: labels are valid only in code sections.
        else if (currentSection == CogScriptSection::code)
        {
            const auto match = cogScriptLabelRx().match(line);
            if (match.hasMatch() && match.captured(1).toLower() == target)
            {
                return lineIndex;
            }
        }
    }

    return std::nullopt;
}

std::optional<int> findCogScriptSymbolLine(const QString& text, const QString& identifier)
{
    const QString target = identifier.trimmed().toLower();
    if (target.isEmpty())
    {
        return std::nullopt;
    }

    bool inSymbols = false;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
    {
        const QString line = stripComment(lines[lineIndex]).trimmed();
        if (line.isEmpty())
        {
            continue;
        }

        if (const auto match = cogScriptSectionRx().match(line); match.hasMatch())
        {
            inSymbols = (match.captured(1).compare(QStringLiteral("symbols"), Qt::CaseInsensitive) == 0);
            continue;
        }

        if (cogScriptEndRx().match(line).hasMatch())
        {
            inSymbols = false;
            continue;
        }

        if (!inSymbols)
        {
            continue;
        }

        const auto match = cogScriptSymbolRx().match(line);
        if (match.hasMatch()
            && cogScriptTypeWordSet().contains(match.captured(1).toLower())
            && match.captured(2).toLower() == target)
        {
            return lineIndex;
        }
    }

    return std::nullopt;
}

std::optional<int> findCogScriptCodeLabelLine(const QString& text, const QString& identifier)
{
    const QString target = identifier.trimmed().toLower();
    if (target.isEmpty())
    {
        return std::nullopt;
    }

    bool inCode = false;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
    {
        const QString line = stripComment(lines[lineIndex]).trimmed();
        if (line.isEmpty())
        {
            continue;
        }

        if (const auto match = cogScriptSectionRx().match(line); match.hasMatch())
        {
            inCode = (match.captured(1).compare(QStringLiteral("code"), Qt::CaseInsensitive) == 0);
            continue;
        }

        if (cogScriptEndRx().match(line).hasMatch())
        {
            inCode = false;
            continue;
        }

        if (!inCode)
        {
            continue;
        }

        const auto match = cogScriptLabelRx().match(line);
        if (match.hasMatch() && match.captured(1).toLower() == target)
        {
            return lineIndex;
        }
    }

    return std::nullopt;
}

} // namespace sed::ui::cog
