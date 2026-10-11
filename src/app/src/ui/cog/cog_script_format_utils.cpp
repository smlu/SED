#include "ui/cog/cog_script_format_utils.hpp"

#include <algorithm>
#include <limits>

#include <QRegularExpression>
#include <QStringList>
#include <QVector>

#include "ui/cog/cog_script_text_utils.hpp"

namespace sed::ui::cog
{

std::optional<std::uint32_t> parseMaskValue(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
    {
        return std::nullopt;
    }

    // Special case: -1 means "all types" (0xFFFF) in the Sith engine.
    if (trimmed == QStringLiteral("-1"))
    {
        return static_cast<std::uint32_t>(0xFFFFu);
    }

    int base = 10;
    QString digits = trimmed;
    if (digits.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
    {
        base = 16;
        digits = digits.mid(2);
    }

    bool ok = false;
    const qulonglong value = digits.toULongLong(&ok, base);
    if (!ok || value > static_cast<qulonglong>(std::numeric_limits<std::uint32_t>::max()))
    {
        return std::nullopt;
    }

    return static_cast<std::uint32_t>(value);
}

QString formatMaskHex(const std::uint32_t mask)
{
    return QStringLiteral("0x%1").arg(mask, 0, 16);
}

QString flagsLineSnippet()
{
    return QStringLiteral("flags=0x00000000");
}

QString sectionSnippet(const QString& sectionName)
{
    if (sectionName.compare(QStringLiteral("flags"), Qt::CaseInsensitive) == 0)
    {
        return flagsLineSnippet() + QLatin1Char('\n');
    }
    return QStringLiteral("%1\n\nend\n").arg(sectionName.toLower());
}

QString symbolSnippet(const QString& typeName)
{
    const QString lower = typeName.toLower();
    if (lower == QStringLiteral("message"))
    {
        return QStringLiteral("message startup\n");
    }
    if (lower == QStringLiteral("flex") || lower == QStringLiteral("float"))
    {
        return QStringLiteral("%1 value=0\n").arg(lower);
    }
    if (lower == QStringLiteral("int"))
    {
        return QStringLiteral("int value=0\n");
    }
    if (lower == QStringLiteral("vector"))
    {
        return QStringLiteral("vector value\n");
    }
    if (lower == QStringLiteral("sector")
        || lower == QStringLiteral("surface")
        || lower == QStringLiteral("thing")
        || lower == QStringLiteral("cog")
        || lower == QStringLiteral("sound")
        || lower == QStringLiteral("template")
        || lower == QStringLiteral("model")
        || lower == QStringLiteral("keyframe")
        || lower == QStringLiteral("material")
        || lower == QStringLiteral("ai"))
    {
        return QStringLiteral("%1 ref=-1\n").arg(lower);
    }
    return QStringLiteral("%1 value\n").arg(lower);
}

QString handlerSnippet(const QString& label)
{
    return QStringLiteral("%1:\n    return;\n").arg(label);
}

QString skeletonSnippet()
{
    return QStringLiteral(
        "symbols\n"
        "\n"
        "end\n"
        "\n"
        "code\n"
        "\n"
        "end\n");
}

QString indentString(const int level, const bool useTabs, const int indentWidth)
{
    const int safeLevel = std::max(0, level);
    if (useTabs)
    {
        return QString(safeLevel, QLatin1Char('\t'));
    }
    return QString(safeLevel * std::max(1, indentWidth), QLatin1Char(' '));
}

QString braceStyleId(const BraceStyle style)
{
    switch (style)
    {
        case BraceStyle::nextLine:
            return QStringLiteral("next_line");
        case BraceStyle::sameLine:
        default:
            return QStringLiteral("same_line");
    }
}

BraceStyle braceStyleFromId(const QString& id)
{
    return id.trimmed().compare(QStringLiteral("next_line"), Qt::CaseInsensitive) == 0
        ? BraceStyle::nextLine
        : BraceStyle::sameLine;
}

QString formatCogScriptText(
    const QString& text,
    const bool useTabs,
    const int indentWidth,
    const BraceStyle braceStyle,
    const bool indentSectionBodies,
    const bool indentCodeBodies)
{
    return formatCogScriptText(text, CogScriptFormatOptions{
        useTabs,
        indentWidth,
        braceStyle,
        indentSectionBodies,
        indentCodeBodies,
        false,
        false,
        false,
        false,
        false,
        true,
        false,
        false,
        false,
    });
}

namespace
{

QString normalizeCodeWhitespace(
    const QString& source,
    const CogScriptFormatOptions& options)
{
    if (source.isEmpty())
    {
        return source;
    }
    if (!options.keywordSpacing
        && !options.insideParenthesesSpacing
        && !options.functionParenthesesSpacing
        && !options.forSemicolonSpacing
        && !options.alignFinalReturnLabel
        && !options.canonicalizeFunctionParentheses
        && !options.canonicalizeNonCallParentheses)
    {
        // The legacy formatter API only promised indentation/brace changes;
        // retain authored intra-line whitespace when all new lexical options
        // are disabled.
        return source;
    }

    QString code{};
    code.reserve(source.size() + 8);
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool escaped = false;
    // Consume authored whitespace after delimiters before adding the selected
    // canonical spacing.  This keeps the option deterministic instead of
    // producing doubled spaces when the input was already partly formatted.
    bool suppressWhitespaceAfterOpenParen = false;
    bool suppressWhitespaceAfterForSemicolon = false;
    struct ParenthesisContext
    {
        bool forClause{};
        bool canonicalizeInside{};
        bool spaceInside{};
    };
    QVector<ParenthesisContext> parentheses{};
    parentheses.reserve(8);

    auto trimTrailingWhitespace = [&code]()
    {
        while (!code.isEmpty() && code.back().isSpace())
        {
            code.chop(1);
        }
    };
    auto appendCanonicalSpace = [&code, &trimTrailingWhitespace]()
    {
        trimTrailingWhitespace();
        if (!code.isEmpty())
        {
            code += QLatin1Char(' ');
        }
    };

    for (int i = 0; i < source.size(); ++i)
    {
        const QChar ch = source.at(i);
        if (escaped)
        {
            code += ch;
            escaped = false;
            continue;
        }
        if ((inSingleQuote || inDoubleQuote) && ch == QLatin1Char('\\'))
        {
            code += ch;
            escaped = true;
            continue;
        }
        if (!inDoubleQuote && ch == QLatin1Char('\''))
        {
            inSingleQuote = !inSingleQuote;
            code += ch;
            continue;
        }
        if (!inSingleQuote && ch == QLatin1Char('"'))
        {
            inDoubleQuote = !inDoubleQuote;
            code += ch;
            continue;
        }
        if (inSingleQuote || inDoubleQuote)
        {
            code += ch;
            continue;
        }

        if (ch.isSpace())
        {
            if (suppressWhitespaceAfterOpenParen || suppressWhitespaceAfterForSemicolon)
            {
                continue;
            }
            code += ch;
            continue;
        }
        suppressWhitespaceAfterOpenParen = false;
        suppressWhitespaceAfterForSemicolon = false;

        if (ch == QLatin1Char('('))
        {
            int wordEnd = code.size() - 1;
            while (wordEnd >= 0 && code.at(wordEnd).isSpace())
            {
                --wordEnd;
            }
            int wordStart = wordEnd;
            while (wordStart >= 0
                && (code.at(wordStart).isLetterOrNumber()
                    || code.at(wordStart) == QLatin1Char('_')))
            {
                --wordStart;
            }
            const QString keyword = code.mid(wordStart + 1, wordEnd - wordStart);
            const bool isKeyword = keyword.compare(QStringLiteral("if"), Qt::CaseInsensitive) == 0
                || keyword.compare(QStringLiteral("while"), Qt::CaseInsensitive) == 0
                || keyword.compare(QStringLiteral("for"), Qt::CaseInsensitive) == 0
                || keyword.compare(QStringLiteral("switch"), Qt::CaseInsensitive) == 0;
            const bool isFunctionInvocation = !isKeyword && !keyword.isEmpty();
            const bool spaceInside = isFunctionInvocation
                ? options.functionParenthesesSpacing
                : options.insideParenthesesSpacing;
            const bool canonicalizeInside = isFunctionInvocation
                ? (options.canonicalizeFunctionParentheses
                    || options.functionParenthesesSpacing)
                : (options.canonicalizeNonCallParentheses
                    || options.insideParenthesesSpacing);
            if (isFunctionInvocation && canonicalizeInside)
            {
                // Calls use canonical `Name(...)` spelling regardless of
                // their selected inner-spacing style. Control keywords keep
                // their independent keyword-spacing policy below.
                trimTrailingWhitespace();
            }
            if (isKeyword)
            {
                trimTrailingWhitespace();
                if (options.keywordSpacing)
                {
                    appendCanonicalSpace();
                }
            }
            code += ch;
            parentheses.push_back(ParenthesisContext{
                .forClause = keyword.compare(
                    QStringLiteral("for"), Qt::CaseInsensitive) == 0,
                .canonicalizeInside = canonicalizeInside,
                .spaceInside = spaceInside,
            });
            suppressWhitespaceAfterOpenParen = canonicalizeInside;
            if (spaceInside
                && i + 1 < source.size()
                && source.at(i + 1) != QLatin1Char(')'))
            {
                code += QLatin1Char(' ');
            }
            continue;
        }
        if (ch == QLatin1Char(')'))
        {
            const ParenthesisContext context = !parentheses.isEmpty()
                ? parentheses.back()
                : ParenthesisContext{};
            if (context.canonicalizeInside
                && !code.endsWith(QLatin1Char('(')))
            {
                trimTrailingWhitespace();
                if (context.spaceInside)
                {
                    appendCanonicalSpace();
                }
            }
            code += ch;
            if (!parentheses.isEmpty())
            {
                parentheses.pop_back();
            }
            continue;
        }
        if (ch == QLatin1Char(';') && !parentheses.isEmpty()
            && parentheses.back().forClause)
        {
            if (options.forSemicolonSpacing)
            {
                trimTrailingWhitespace();
            }
            code += ch;
            suppressWhitespaceAfterForSemicolon = options.forSemicolonSpacing;
            if (options.forSemicolonSpacing
                && i + 1 < source.size()
                && source.at(i + 1) != QLatin1Char(')'))
            {
                code += QLatin1Char(' ');
            }
            continue;
        }
        if (ch == QLatin1Char('{') && options.keywordSpacing)
        {
            int previous = code.size() - 1;
            while (previous >= 0 && code.at(previous).isSpace())
            {
                --previous;
            }
            if (previous >= 0 && code.at(previous) == QLatin1Char(')'))
            {
                // Keep a formatted control header visually separated from its
                // same-line body, canonicalizing authored tabs or Unicode
                // whitespace between `)` and `{` to one ASCII space. This is
                // tied to keyword spacing so legacy formatting stays untouched.
                appendCanonicalSpace();
            }
        }
        code += ch;
    }
    return code;
}

} // namespace

QString formatCogScriptText(
    const QString& text,
    const CogScriptFormatOptions& options)
{
    enum class Section
    {
        none,
        symbols,
        code,
    };

    static const QRegularExpression kFlagsRx(
        QStringLiteral("^\\s*flags\\s*="),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kSectionRx(
        QStringLiteral("^(symbols|code)$"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kEndRx(
        QStringLiteral("^end$"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kLabelRx(
        QStringLiteral("^\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*:"),
        QRegularExpression::UseUnicodePropertiesOption);

    const QStringList lines = text.split(QLatin1Char('\n'));
    QStringList out{};
    out.reserve(lines.size());

    Section currentSection = Section::none;
    int braceDepth = 0;
    const int sectionIndent = options.indentSectionBodies ? 1 : 0;
    const int codeBodyIndent = options.indentCodeBodies ? 1 : 0;

    // Aligning a handler's final return used to scan forward from every
    // `return;`. A return-heavy cutscene could therefore make formatting
    // quadratic. Cache whether the next substantive source line is a label
    // or section terminator in one reverse pass.
    QVector<bool> nextSubstantiveIsLabelOrEnd(lines.size(), false);
    if (options.alignFinalReturnLabel)
    {
        bool nextIsBoundary = false;
        for (qsizetype i = lines.size(); i-- > 0;)
        {
            nextSubstantiveIsLabelOrEnd[i] = nextIsBoundary;
            const QString code = stripComment(lines.at(i)).trimmed();
            if (!code.isEmpty())
            {
                nextIsBoundary = code.compare(
                    QStringLiteral("end"), Qt::CaseInsensitive) == 0
                    || kLabelRx.match(code).hasMatch();
            }
        }
    }

    for (qsizetype lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
    {
        const QString& rawLine = lines.at(lineIndex);
        QString line = rawLine;
        while (!line.isEmpty() && line.back().isSpace())
        {
            line.chop(1);
        }

        const int commentStart = findCommentStart(line);
        QString commentSuffix{};
        QString commentPrefix{};
        if (commentStart >= 0)
        {
            commentPrefix = line.left(commentStart);
            commentSuffix = line.mid(commentStart).trimmed();
            line = line.left(commentStart);
        }

        const QString trimmed = line.trimmed();
        auto withComment = [&](const QString& base) -> QString
        {
            if (commentSuffix.isEmpty())
            {
                return base;
            }
            if (base.isEmpty())
            {
                return commentSuffix;
            }
            return base + QStringLiteral(" ") + commentSuffix;
        };

        if (trimmed.isEmpty())
        {
            if (commentSuffix.isEmpty())
            {
                // Blank lines have no visual indentation.  Besides producing
                // cleaner diffs, this guarantees that Format on Save removes
                // whitespace-only lines rather than silently reintroducing it.
                out.push_back(QString{});
            }
            else
            {
                if (!options.indentComments)
                {
                    out.push_back(commentPrefix + commentSuffix);
                    continue;
                }
                int commentIndent = 0;
                if (currentSection == Section::symbols)
                {
                    commentIndent = sectionIndent;
                }
                else if (currentSection == Section::code)
                {
                    commentIndent = sectionIndent + codeBodyIndent + braceDepth;
                }
                out.push_back(indentString(commentIndent, options.useTabs, options.indentWidth) + commentSuffix);
            }
            continue;
        }

        if (kFlagsRx.match(trimmed).hasMatch())
        {
            out.push_back(withComment(trimmed));
            continue;
        }

        if (const auto match = kSectionRx.match(trimmed); match.hasMatch())
        {
            currentSection = (match.captured(1).compare(QStringLiteral("symbols"), Qt::CaseInsensitive) == 0)
                ? Section::symbols
                : Section::code;
            braceDepth = 0;
            out.push_back(withComment(match.captured(1).toLower()));
            continue;
        }

        if (kEndRx.match(trimmed).hasMatch())
        {
            currentSection = Section::none;
            braceDepth = 0;
            out.push_back(withComment(QStringLiteral("end")));
            continue;
        }

        if (currentSection == Section::symbols)
        {
            out.push_back(withComment(indentString(sectionIndent, options.useTabs, options.indentWidth)
                + normalizeCodeWhitespace(trimmed, options)));
            continue;
        }

        if (currentSection == Section::code)
        {
            if (kLabelRx.match(trimmed).hasMatch())
            {
                out.push_back(withComment(indentString(sectionIndent, options.useTabs, options.indentWidth)
                    + normalizeCodeWhitespace(trimmed, options)));
                continue;
            }

            int closeCount = 0;
            int openCount = 0;
            // Strings/comments may contain braces; they must not alter the
            // formatter's structural depth. Sanitize once per line while
            // counting both characters.
            for (const QChar ch : sanitizeLineForAnalysis(trimmed))
            {
                if (ch == QLatin1Char('}'))
                {
                    ++closeCount;
                }
                else if (ch == QLatin1Char('{'))
                {
                    ++openCount;
                }
            }
            const int lineIndent =
                sectionIndent + codeBodyIndent
                + std::max(0, braceDepth - (trimmed.startsWith(QLatin1Char('}')) ? 1 : 0));
            bool finalReturnLabel = false;
            if (options.alignFinalReturnLabel
                && trimmed.compare(
                       QStringLiteral("return;"), Qt::CaseInsensitive) == 0)
            {
                finalReturnLabel = nextSubstantiveIsLabelOrEnd.at(lineIndex);
            }
            const int effectiveLineIndent = finalReturnLabel ? sectionIndent : lineIndent;

            if (options.braceStyle == BraceStyle::sameLine && trimmed == QStringLiteral("{"))
            {
                bool merged = false;
                for (int i = static_cast<int>(out.size()) - 1; i >= 0; --i)
                {
                    const QString prevTrimmed = out[static_cast<qsizetype>(i)].trimmed();
                    if (prevTrimmed.isEmpty())
                    {
                        continue;
                    }
                    if (!prevTrimmed.endsWith(QLatin1Char(':')) && !prevTrimmed.endsWith(QLatin1Char('{')))
                    {
                        out[static_cast<qsizetype>(i)] += commentSuffix.isEmpty()
                            ? QStringLiteral(" {")
                            : QStringLiteral(" { ") + commentSuffix;
                        merged = true;
                    }
                    break;
                }

                if (!merged)
                {
                    out.push_back(withComment(indentString(effectiveLineIndent, options.useTabs, options.indentWidth) + trimmed));
                }

                braceDepth = std::max(0, braceDepth + openCount - closeCount);
                continue;
            }

            if (options.braceStyle == BraceStyle::nextLine
                && trimmed != QStringLiteral("{")
                && trimmed.endsWith(QLatin1Char('{'))
                && openCount == 1
                && closeCount == 0)
            {
                QString header = normalizeCodeWhitespace(trimmed, options);
                header.chop(1);
                header = header.trimmed();
                if (!header.isEmpty())
                {
                    out.push_back(withComment(indentString(effectiveLineIndent, options.useTabs, options.indentWidth) + header));
                }
                out.push_back(indentString(effectiveLineIndent, options.useTabs, options.indentWidth) + QStringLiteral("{"));
                braceDepth = std::max(0, braceDepth + 1);
                continue;
            }

            out.push_back(withComment(indentString(effectiveLineIndent, options.useTabs, options.indentWidth)
                + normalizeCodeWhitespace(trimmed, options)));
            braceDepth = std::max(0, braceDepth + openCount - closeCount);
            continue;
        }

        out.push_back(withComment(normalizeCodeWhitespace(trimmed, options)));
    }

    // A final split element represents the newline we add below, not an
    // authored blank line. Remove that sentinel in either policy mode.
    if (text.endsWith(QLatin1Char('\n')) && !out.isEmpty() && out.back().isEmpty())
    {
        out.pop_back();
    }

    if (options.trimExcessBlankLines)
    {
        const auto structuralText = [](const QString& line)
        {
            return stripComment(line).trimmed();
        };
        const auto opensBlankFreeBoundary = [&structuralText](const QString& line)
        {
            const QString code = structuralText(line);
            return code.endsWith(QLatin1Char('{'))
                || code.compare(QStringLiteral("symbols"), Qt::CaseInsensitive) == 0
                || code.compare(QStringLiteral("code"), Qt::CaseInsensitive) == 0;
        };
        const auto closesBlankFreeBoundary = [&structuralText](const QString& line)
        {
            const QString code = structuralText(line);
            return code.startsWith(QLatin1Char('}'))
                || code.compare(QStringLiteral("end"), Qt::CaseInsensitive) == 0;
        };
        QStringList compact{};
        compact.reserve(out.size());
        bool sawContent = false;
        bool previousBlank = false;
        for (qsizetype i = 0; i < out.size(); ++i)
        {
            const QString& line = out.at(i);
            const bool blank = line.isEmpty();
            if (blank)
            {
                qsizetype next = i + 1;
                while (next < out.size() && out.at(next).isEmpty())
                {
                    ++next;
                }
                const bool afterBoundary = !compact.isEmpty()
                    && opensBlankFreeBoundary(compact.back());
                const bool beforeBoundary = next < out.size()
                    && closesBlankFreeBoundary(out.at(next));
                if (!sawContent || previousBlank || afterBoundary || beforeBoundary)
                {
                    // Consume the complete blank run once. Re-scanning its
                    // remaining suffix from every blank line made a long run
                    // quadratic in otherwise linear large-script formatting.
                    i = next - 1;
                    continue;
                }
                // Preserve exactly one interior blank and skip the remainder
                // of this already-classified run.
                i = next - 1;
            }
            compact.push_back(line);
            sawContent = sawContent || !blank;
            previousBlank = blank;
        }
        while (!compact.isEmpty() && compact.back().isEmpty())
        {
            compact.pop_back();
        }
        out = std::move(compact);
    }

    return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

} // namespace sed::ui::cog
