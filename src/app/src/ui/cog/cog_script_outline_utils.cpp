#include "ui/cog/cog_script_outline_utils.hpp"

#include <QRegularExpression>
#include <QSet>

#include "ui/cog/cog_script_language_utils.hpp"

namespace sed::ui::cog
{

std::vector<CogScriptOutlineEntry> buildCogScriptOutline(const QString& text)
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

    static const QRegularExpression kFlagsRx(
        QStringLiteral("^\\s*flags\\s*="),
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
    static const QRegularExpression kLabelRx(
        QStringLiteral("^\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*:"),
        QRegularExpression::UseUnicodePropertiesOption);

    std::vector<CogScriptOutlineEntry> entries{};
    Section section = Section::none;
    const QStringList lines = text.split(QLatin1Char('\n'));
    entries.reserve(static_cast<std::size_t>(lines.size() / 3 + 4));

    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
    {
        const QString line = lines[lineIndex].trimmed();
        if (line.isEmpty() || line.startsWith(QStringLiteral("//")) || line.startsWith(QLatin1Char('#')))
        {
            continue;
        }

        if (kFlagsRx.match(line).hasMatch())
        {
            entries.push_back(CogScriptOutlineEntry{
                .label   = QStringLiteral("Flags"),
                .line    = lineIndex,
                .section = true,
                .group   = CogScriptOutlineGroup::symbols,
            });
            continue;
        }

        if (const auto match = kSectionRx.match(line); match.hasMatch())
        {
            const QString sectionName = match.captured(1).toLower();
            if (sectionName == QStringLiteral("symbols"))
            {
                section = Section::symbols;
                entries.push_back(CogScriptOutlineEntry{
                    .label   = QStringLiteral("Symbols"),
                    .line    = lineIndex,
                    .section = true,
                    .group   = CogScriptOutlineGroup::symbols,
                });
            }
            else if (sectionName == QStringLiteral("code"))
            {
                section = Section::code;
                entries.push_back(CogScriptOutlineEntry{
                    .label   = QStringLiteral("Code"),
                    .line    = lineIndex,
                    .section = true,
                    .group   = CogScriptOutlineGroup::code,
                });
            }
            continue;
        }

        if (kEndRx.match(line).hasMatch())
        {
            section = Section::none;
            continue;
        }

        if (section == Section::symbols)
        {
            const auto match = kSymbolRx.match(line);
            if (match.hasMatch())
            {
                const QString typeName = match.captured(1).toLower();
                if (kTypeWords.contains(typeName))
                {
                    entries.push_back(CogScriptOutlineEntry{
                        .label   = QStringLiteral("  %1 : %2").arg(match.captured(2), typeName),
                        .line    = lineIndex,
                        .section = false,
                        .group   = CogScriptOutlineGroup::symbols,
                    });
                }
            }
        }
        else if (section == Section::code)
        {
            const auto match = kLabelRx.match(line);
            if (match.hasMatch())
            {
                entries.push_back(CogScriptOutlineEntry{
                    .label   = QStringLiteral("  %1").arg(match.captured(1)),
                    .line    = lineIndex,
                    .section = false,
                    .group   = CogScriptOutlineGroup::code,
                });
            }
        }
    }

    return entries;
}

} // namespace sed::ui::cog
