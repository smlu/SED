#include "ui/cog/cog_script_assignment_utils.hpp"

#include "ui/cog/cog_script_text_utils.hpp"
#include "ui/cog/cog_script_language_utils.hpp"

#include <algorithm>

#include <QRegularExpression>

namespace sed::ui::cog
{
namespace
{

std::optional<ResourcePickerKind> resourceKindForSymbolType(const QString& typeName)
{
    const QString normalized = typeName.toLower();
    if ( normalized == QStringLiteral("material") )
    {
        return ResourcePickerKind::material;
    }
    if ( normalized == QStringLiteral("sound") )
    {
        return ResourcePickerKind::wav;
    }
    if ( normalized == QStringLiteral("template") )
    {
        return ResourcePickerKind::template_;
    }
    if ( normalized == QStringLiteral("model") )
    {
        return ResourcePickerKind::model3do;
    }
    if ( normalized == QStringLiteral("ai") )
    {
        return ResourcePickerKind::ai;
    }
    if ( normalized == QStringLiteral("keyframe") )
    {
        return ResourcePickerKind::key;
    }
    return std::nullopt;
}

bool assignmentValueHit(
    const CogScriptInlineAssignmentSpan& span,
    const int column)
{
    if ( !span.hasInitializer )
    {
        return column >= span.symbolNameStart
            && column <= span.symbolNameStart + span.symbolNameLength;
    }
    return column == span.equalsStart
        || (span.valueLength > 0
            && column >= span.valueStart
            && column < span.valueStart + span.valueLength);
}

std::optional<CogScriptInlineAssignmentSpan> assignmentOnLine(
    const QString& rawLine,
    const CogScriptEditorSection section,
    const int column,
    const CogScriptInlineAssignmentKind kind,
    const int equalsStart,
    const QString& typeName,
    const QString& symbolName,
    const std::optional<ResourcePickerKind> resourceKind)
{
    const QString code = stripComment(rawLine);
    if ( equalsStart < 0 || equalsStart >= code.size()
        || code.at(equalsStart) != QLatin1Char('=') )
    {
        return std::nullopt;
    }

    int logicalEnd = code.size();
    while ( logicalEnd > 0 && code.at(logicalEnd - 1).isSpace() )
    {
        --logicalEnd;
    }

    int valueStart = equalsStart + 1;
    while ( valueStart < logicalEnd && code.at(valueStart).isSpace() )
    {
        ++valueStart;
    }

    CogScriptInlineAssignmentSpan span{};
    span.kind = kind;
    span.section = section;
    span.equalsStart = equalsStart;
    span.typeName = typeName;
    span.symbolName = symbolName;
    span.resourceKind = resourceKind;

    if ( valueStart >= logicalEnd )
    {
        // Insert before authored trailing whitespace or a comment. This keeps
        // an empty `=` target useful without normalizing anything nearby.
        span.valueStart = logicalEnd;
        if ( column != equalsStart )
        {
            return std::nullopt;
        }
        return span;
    }

    const QChar first = code.at(valueStart);
    if ( first == QLatin1Char('\'') || first == QLatin1Char('"') )
    {
        bool escaped = false;
        int closingQuote = -1;
        for ( int index = valueStart + 1; index < logicalEnd; ++index )
        {
            const QChar ch = code.at(index);
            if ( escaped )
            {
                escaped = false;
                continue;
            }
            if ( ch == QLatin1Char('\\') )
            {
                escaped = true;
                continue;
            }
            if ( ch == first )
            {
                closingQuote = index;
                break;
            }
        }
        if ( closingQuote < 0 )
        {
            return std::nullopt;
        }
        if ( closingQuote + 1 < logicalEnd
            && !code.at(closingQuote + 1).isSpace()
            && code.at(closingQuote + 1) != QLatin1Char(',')
            && code.at(closingQuote + 1) != QLatin1Char(';') )
        {
            return std::nullopt;
        }
        span.valueStart = valueStart;
        span.valueLength = closingQuote - valueStart + 1;
        span.currentValue = code.mid(valueStart + 1, closingQuote - valueStart - 1);
        span.quote = first;
    }
    else
    {
        int valueEnd = valueStart;
        while ( valueEnd < logicalEnd
            && !code.at(valueEnd).isSpace()
            && code.at(valueEnd) != QLatin1Char(',')
            && code.at(valueEnd) != QLatin1Char(';') )
        {
            if ( code.at(valueEnd) == QLatin1Char('=')
                || code.at(valueEnd) == QLatin1Char('\'')
                || code.at(valueEnd) == QLatin1Char('"') )
            {
                return std::nullopt;
            }
            ++valueEnd;
        }
        if ( valueEnd == valueStart )
        {
            return std::nullopt;
        }
        span.valueStart = valueStart;
        span.valueLength = valueEnd - valueStart;
        span.currentValue = code.mid(valueStart, span.valueLength);
    }

    return assignmentValueHit(span, column)
        ? std::optional<CogScriptInlineAssignmentSpan>{span}
        : std::nullopt;
}

std::optional<CogScriptInlineAssignmentSpan> flagsAssignmentAtColumn(
    const QString& rawLine,
    const CogScriptEditorSection section,
    const int column)
{
    if ( section != CogScriptEditorSection::outside )
    {
        return std::nullopt;
    }

    const QString code = stripComment(rawLine);
    static const QRegularExpression kFlagsPrefixRx(
        QStringLiteral("^\\s*(flags)\\b"),
        QRegularExpression::CaseInsensitiveOption
            | QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpressionMatch prefix = kFlagsPrefixRx.match(code);
    if ( !prefix.hasMatch() )
    {
        return std::nullopt;
    }

    int equalsStart = prefix.capturedEnd();
    while ( equalsStart < code.size() && code.at(equalsStart).isSpace() )
    {
        ++equalsStart;
    }
    if ( equalsStart >= code.size() || code.at(equalsStart) != QLatin1Char('=') )
    {
        return std::nullopt;
    }

    return assignmentOnLine(
        rawLine,
        section,
        column,
        CogScriptInlineAssignmentKind::flags,
        equalsStart,
        prefix.captured(1),
        {},
        std::nullopt);
}

std::optional<CogScriptInlineAssignmentSpan> symbolMaskAtColumn(
    const QString& rawLine,
    const CogScriptEditorSection section,
    const int column)
{
    if ( section != CogScriptEditorSection::symbols )
    {
        return std::nullopt;
    }

    const QString code = stripComment(rawLine);
    static const QRegularExpression kSymbolPrefixRx(
        QStringLiteral("^\\s*([A-Za-z_][A-Za-z0-9_]*)\\s+([A-Za-z_][A-Za-z0-9_]*)\\b"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpressionMatch declaration = kSymbolPrefixRx.match(code);
    if ( !declaration.hasMatch()
        || !cogScriptTypeSupportsTriggerFilterAttributes(declaration.captured(1)) )
    {
        return std::nullopt;
    }

    const auto attributeValueIsPending = [&code, start = static_cast<int>(declaration.capturedEnd())](
        const int stop)
    {
        int index = start;
        bool expectsValue = false;
        while ( index < stop )
        {
            while ( index < stop
                && (code.at(index).isSpace() || code.at(index) == QLatin1Char(',')) )
            {
                ++index;
            }
            if ( index >= stop )
            {
                break;
            }

            const int tokenStart = index;
            QChar activeQuote{};
            bool escaped = false;
            while ( index < stop )
            {
                const QChar character = code.at(index);
                if ( !activeQuote.isNull() )
                {
                    if ( escaped )
                    {
                        escaped = false;
                    }
                    else if ( character == QLatin1Char('\\') )
                    {
                        escaped = true;
                    }
                    else if ( character == activeQuote )
                    {
                        activeQuote = QChar{};
                    }
                    ++index;
                    continue;
                }
                if ( character == QLatin1Char('\'')
                    || character == QLatin1Char('"') )
                {
                    activeQuote = character;
                    ++index;
                    continue;
                }
                if ( character.isSpace() || character == QLatin1Char(',') )
                {
                    break;
                }
                ++index;
            }

            const QString token = code.mid(tokenStart, index - tokenStart);
            if ( token == QStringLiteral("=") )
            {
                expectsValue = true;
            }
            else if ( token.contains(QLatin1Char('=')) )
            {
                const int equals = token.indexOf(QLatin1Char('='));
                expectsValue = equals == token.size() - 1;
            }
            else if ( expectsValue )
            {
                expectsValue = false;
            }
        }
        return expectsValue;
    };

    const QString analysis = sanitizeLineForAnalysis(code);
    static const QRegularExpression kMaskAttributeRx(
        QStringLiteral("(?<![^\\s,])mask\\s*="),
        QRegularExpression::CaseInsensitiveOption
            | QRegularExpression::UseUnicodePropertiesOption);
    QRegularExpressionMatchIterator matches = kMaskAttributeRx.globalMatch(analysis);
    while ( matches.hasNext() )
    {
        const QRegularExpressionMatch match = matches.next();
        const int attributeStart = static_cast<int>(match.capturedStart());
        if ( attributeStart < declaration.capturedEnd()
            || attributeValueIsPending(attributeStart) )
        {
            continue;
        }

        const int equalsStart = static_cast<int>(match.capturedEnd()) - 1;
        const auto span = assignmentOnLine(
            rawLine,
            section,
            column,
            CogScriptInlineAssignmentKind::mask,
            equalsStart,
            declaration.captured(1),
            declaration.captured(2),
            std::nullopt);
        if ( span )
        {
            return span;
        }
    }

    return std::nullopt;
}

std::optional<CogScriptInlineAssignmentSpan> symbolResourceAtColumn(
    const QString& rawLine,
    const CogScriptEditorSection section,
    const int column)
{
    if ( section != CogScriptEditorSection::symbols )
    {
        return std::nullopt;
    }

    const QString code = stripComment(rawLine);
    static const QRegularExpression kSymbolPrefixRx(
        QStringLiteral("^\\s*([A-Za-z_][A-Za-z0-9_]*)\\s+([A-Za-z_][A-Za-z0-9_]*)\\b"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpressionMatch declaration = kSymbolPrefixRx.match(code);
    if ( !declaration.hasMatch() )
    {
        return std::nullopt;
    }

    const QString typeName = declaration.captured(1);
    const std::optional<ResourcePickerKind> resourceKind =
        resourceKindForSymbolType(typeName);
    if ( !resourceKind )
    {
        return std::nullopt;
    }

    const int symbolNameStart = static_cast<int>(declaration.capturedStart(2));
    const int symbolNameLength = static_cast<int>(declaration.capturedLength(2));
    int equalsStart = declaration.capturedEnd();
    while ( equalsStart < code.size() && code.at(equalsStart).isSpace() )
    {
        ++equalsStart;
    }
    if ( equalsStart >= code.size() || code.at(equalsStart) != QLatin1Char('=') )
    {
        const int symbolNameEnd = symbolNameStart + symbolNameLength;
        if ( column < symbolNameStart || column > symbolNameEnd )
        {
            return std::nullopt;
        }

        CogScriptInlineAssignmentSpan span{};
        span.kind = CogScriptInlineAssignmentKind::resource;
        span.section = section;
        span.valueStart = symbolNameEnd;
        span.symbolNameStart = symbolNameStart;
        span.symbolNameLength = symbolNameLength;
        span.typeName = typeName;
        span.symbolName = declaration.captured(2);
        span.resourceKind = resourceKind;
        span.hasInitializer = false;
        return span;
    }

    std::optional<CogScriptInlineAssignmentSpan> span = assignmentOnLine(
        rawLine,
        section,
        column,
        CogScriptInlineAssignmentKind::resource,
        equalsStart,
        typeName,
        declaration.captured(2),
        resourceKind);
    if ( span )
    {
        span->symbolNameStart = symbolNameStart;
        span->symbolNameLength = symbolNameLength;
    }
    return span;
}

} // namespace

std::optional<CogScriptInlineAssignmentSpan> cogScriptInlineAssignmentAtColumn(
    const QString& rawLine,
    const CogScriptEditorSection section,
    const int column)
{
    if ( column < 0 || column >= rawLine.size() )
    {
        return std::nullopt;
    }

    if ( const auto flags = flagsAssignmentAtColumn(rawLine, section, column) )
    {
        return flags;
    }
    if ( const auto mask = symbolMaskAtColumn(rawLine, section, column) )
    {
        return mask;
    }
    return symbolResourceAtColumn(rawLine, section, column);
}

std::optional<CogScriptSymbolDeclarationNameSpan> cogScriptSymbolDeclarationNameSpan(
    const QString& rawLine)
{
    const QString code = stripComment(rawLine);
    static const QRegularExpression kSymbolPrefixRx(
        QStringLiteral("^\\s*([A-Za-z_][A-Za-z0-9_]*)\\s+([A-Za-z_][A-Za-z0-9_]*)\\b"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpressionMatch declaration = kSymbolPrefixRx.match(code);
    if ( !declaration.hasMatch() )
    {
        return std::nullopt;
    }

    return CogScriptSymbolDeclarationNameSpan{
        .start = static_cast<int>(declaration.capturedStart(2)),
        .length = static_cast<int>(declaration.capturedLength(2)),
    };
}

bool cogScriptCursorIsSymbolDeclarationName(
    const QString& rawLine,
    const CogScriptEditorSection section,
    const int column)
{
    if ( section != CogScriptEditorSection::symbols
        || column < 0 || column > rawLine.size() )
    {
        return false;
    }

    const std::optional<CogScriptSymbolDeclarationNameSpan> nameSpan =
        cogScriptSymbolDeclarationNameSpan(rawLine);
    return nameSpan
        && column >= nameSpan->start
        && column <= nameSpan->start + nameSpan->length;
}

bool isSafeCogScriptResourceName(const QStringView name) noexcept
{
    constexpr int kMaximumResourceNameLength = 255;
    if ( name.isEmpty() || name.size() > kMaximumResourceNameLength )
    {
        return false;
    }
    for ( int index = 0; index < name.size(); ++index )
    {
        const QChar ch = name.at(index);
        if ( ch.isSpace()
            || ch.isNull()
            || ch.category() == QChar::Other_Control
            || ch == QLatin1Char('#')
            || ch == QLatin1Char('=')
            || ch == QLatin1Char(',')
            || ch == QLatin1Char(';')
            || ch == QLatin1Char('\'')
            || ch == QLatin1Char('"')
            || (ch == QLatin1Char('/')
                && index + 1 < name.size()
                && name.at(index + 1) == QLatin1Char('/')) )
        {
            return false;
        }
    }
    return true;
}

std::optional<QString> replaceCogScriptInlineResourceValue(
    const QString& currentLine,
    const CogScriptInlineAssignmentSpan& expected,
    const QString& selectedName)
{
    if ( expected.kind != CogScriptInlineAssignmentKind::resource
        || !expected.resourceKind
        || !isSafeCogScriptResourceName(QStringView{selectedName}) )
    {
        return std::nullopt;
    }

    const std::optional<CogScriptInlineAssignmentSpan> current =
        cogScriptInlineAssignmentAtColumn(
            currentLine,
            expected.section,
            expected.hasInitializer
                ? expected.equalsStart
                : expected.symbolNameStart);
    if ( !current || *current != expected
        || expected.valueStart < 0
        || expected.valueLength < 0
        || expected.valueStart + expected.valueLength > currentLine.size() )
    {
        return std::nullopt;
    }

    QString replacement = selectedName;
    if ( !expected.quote.isNull() )
    {
        replacement.prepend(expected.quote);
        replacement.append(expected.quote);
    }
    QString updated = currentLine;
    if ( !expected.hasInitializer )
    {
        updated.insert(
            expected.valueStart,
            QStringLiteral(" = ") + selectedName);
        return updated;
    }
    updated.replace(expected.valueStart, expected.valueLength, replacement);
    return updated;
}

} // namespace sed::ui::cog
