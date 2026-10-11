#pragma once

#include "ui/components/typed_numeric_text.hpp"
#include "ui/query/level_query_utils.hpp"

namespace sed::ui::query
{

inline bool isNumericQueryField(const LevelQueryField field)
{
    const LevelQueryFieldKind kind = levelQueryFieldKind(field);
    return kind == LevelQueryFieldKind::real || kind == LevelQueryFieldKind::color;
}

inline bool isNumericQueryFragment(const QString& fragment, const bool color)
{
    for ( const QChar character : fragment )
    {
        if ( character.isDigit() || character == QLatin1Char('+')
            || character == QLatin1Char('-') || character == QLatin1Char('.')
            || character == QLatin1Char('e') || character == QLatin1Char('E') )
        {
            continue;
        }
        if ( color && (character.isSpace() || character == QLatin1Char('/')
            || character == QLatin1Char(':') || character == QLatin1Char('(')
            || character == QLatin1Char(')')) )
        {
            continue;
        }
        return false;
    }
    return true;
}

inline QString localizeLevelQueryNumericText(const QString& canonical,
    const LevelQueryField field, const LevelQueryMatchMode mode)
{
    if ( !isNumericQueryField(field) )
    {
        return canonical;
    }
    const bool color = levelQueryFieldKind(field) == LevelQueryFieldKind::color;
    if ( levelQueryUsesTextComparison(mode) && isNumericQueryFragment(canonical, color) )
    {
        return localizeUiNumericText(canonical);
    }
    return localizeTypedNumericText(canonical, color ? ValueType::vect : ValueType::floating);
}

inline std::optional<QString> canonicalLevelQueryNumericText(const QString& localized,
    const LevelQueryField field, const LevelQueryMatchMode mode, const QLocale& inputLocale)
{
    if ( !isNumericQueryField(field) || localized.isEmpty() )
    {
        return localized;
    }
    const bool color = levelQueryFieldKind(field) == LevelQueryFieldKind::color;
    if ( levelQueryUsesTextComparison(mode) )
    {
        QString fragment = localized;
        if ( inputLocale.decimalPoint() != QStringLiteral(".")
            && fragment.contains(QLatin1Char('.')) )
        {
            return std::nullopt;
        }
        if ( !inputLocale.groupSeparator().isEmpty()
            && fragment.contains(inputLocale.groupSeparator()) )
        {
            return std::nullopt;
        }
        fragment.replace(inputLocale.decimalPoint(), QStringLiteral("."));
        fragment.replace(inputLocale.negativeSign(), QStringLiteral("-"));
        fragment.replace(inputLocale.positiveSign(), QStringLiteral("+"));
        fragment.replace(inputLocale.exponential(), QStringLiteral("e"), Qt::CaseInsensitive);
        for ( qsizetype position = 0; position < fragment.size(); ++position )
        {
            const int digit = fragment[position].digitValue();
            if ( digit >= 0 )
            {
                fragment[position] = QChar(static_cast<char16_t>(u'0' + digit));
            }
        }
        return isNumericQueryFragment(fragment, color) ? fragment : localized;
    }
    const auto canonical = canonicalTypedNumericText(localized,
        color ? ValueType::vect : ValueType::floating, inputLocale);
    if ( canonical && color )
    {
        LevelQuerySpec spec{};
        spec.value = canonical->toStdString();
        if ( !parseLevelQueryColor(spec) )
        {
            return std::nullopt;
        }
    }
    return canonical;
}

} // namespace sed::ui::query
