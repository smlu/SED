#include "ui/cog/cog_script_text_utils.hpp"

#include <QChar>

namespace sed::ui::cog
{
namespace
{

bool quoteIsEscaped(const QString& source, const int quoteIndex)
{
    int precedingBackslashes = 0;
    for (int i = quoteIndex - 1;
         i >= 0 && source.at(i) == QLatin1Char('\\');
         --i)
    {
        ++precedingBackslashes;
    }
    return (precedingBackslashes % 2) != 0;
}

} // namespace

int findCommentStart(const QString& line)
{
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    for (int i = 0; i < line.size(); ++i)
    {
        const QChar ch = line.at(i);
        if (!inDoubleQuote && ch == QLatin1Char('\'')
            && !quoteIsEscaped(line, i))
        {
            inSingleQuote = !inSingleQuote;
            continue;
        }
        if (!inSingleQuote && ch == QLatin1Char('"')
            && !quoteIsEscaped(line, i))
        {
            inDoubleQuote = !inDoubleQuote;
            continue;
        }
        if (inSingleQuote || inDoubleQuote)
        {
            continue;
        }

        if (ch == QLatin1Char('#'))
        {
            return i;
        }
        if (ch == QLatin1Char('/') && i + 1 < line.size() && line.at(i + 1) == QLatin1Char('/'))
        {
            return i;
        }
    }
    return -1;
}

QString stripComment(const QString& line)
{
    const int commentStart = findCommentStart(line);
    if (commentStart >= 0)
    {
        return line.left(commentStart);
    }
    return line;
}

QString sanitizeLineForAnalysis(const QString& line)
{
    QString out = line;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    for (int i = 0; i < out.size(); ++i)
    {
        const QChar ch = out.at(i);
        // Quote escaping must be classified from the original source. `out`
        // is progressively blanked to preserve positions, so looking behind
        // in it would lose the backslash run inside a string.
        if (!inDoubleQuote && ch == QLatin1Char('\'')
            && !quoteIsEscaped(line, i))
        {
            inSingleQuote = !inSingleQuote;
            out[i] = QLatin1Char(' ');
            continue;
        }
        if (!inSingleQuote && ch == QLatin1Char('"')
            && !quoteIsEscaped(line, i))
        {
            inDoubleQuote = !inDoubleQuote;
            out[i] = QLatin1Char(' ');
            continue;
        }
        if (inSingleQuote || inDoubleQuote)
        {
            out[i] = QLatin1Char(' ');
            continue;
        }
        if (ch == QLatin1Char('#'))
        {
            for (int j = i; j < out.size(); ++j)
            {
                out[j] = QLatin1Char(' ');
            }
            break;
        }
        if (ch == QLatin1Char('/') && i + 1 < out.size() && out.at(i + 1) == QLatin1Char('/'))
        {
            for (int j = i; j < out.size(); ++j)
            {
                out[j] = QLatin1Char(' ');
            }
            break;
        }
    }
    return out;
}

} // namespace sed::ui::cog
