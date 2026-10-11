#pragma once

#include "sed/text.hpp"
#include "sed/values.hpp"
#include "ui/components/ui_decimal_format.hpp"

#include <QByteArray>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QVariant>

#include <array>
#include <optional>
#include <string_view>
#include <utility>

namespace sed::ui
{

// Thing/template codecs retain most runtime arguments as strings. Classify
// their numeric editors by the known parser name, never by numberlike content.
inline ValueType typedNumericTextType(const TplValue& value)
{
    if ( value.adjType != AdjType::unk && value.adjType != AdjType::frame )
    {
        return ValueType::str;
    }
    if ( value.type != ValueType::str && value.type != ValueType::unknown )
    {
        return value.type;
    }
    constexpr auto tupleNames = std::to_array<std::string_view>({
        "angvel", "eyeoffset", "fireoffset", "lightoffset", "orient",
        "spriteend", "spritestart", "vel", "voicecolor", "light", "lightintensity",
    });
    for ( const std::string_view name : tupleNames )
    {
        if ( text::iequalsAscii(value.name, name) )
        {
            return ValueType::vect;
        }
    }
    constexpr auto scalarNames = std::to_array<std::string_view>({
        "size", "treesize", "timer", "surfdrag", "airdrag", "staticdrag", "mass",
        "height", "maxrotvel", "maxvel", "health", "maxthrust", "maxrotthrust",
        "maxheadvel", "maxheadyaw", "jumpspeed", "damage", "mindamage", "blasttime",
        "babytime", "force", "maxlight", "range", "expandtime", "fadetime", "respawn",
        "respawnfactor", "rate", "elementsize", "maxhealth", "movesize", "orientspeed",
        "buoyancy", "minheadpitch", "maxheadpitch", "trailcylradius", "trailrandangle",
        "minsize", "pitchrange", "yawrange", "error", "fov", "chance", "userval",
        "collheight", "collwidth",
    });
    for ( const std::string_view name : scalarNames )
    {
        if ( text::iequalsAscii(value.name, name) )
        {
            return ValueType::floating;
        }
    }
    return value.type;
}

inline bool isTypedNumericText(const ValueType type)
{
    return type == ValueType::floating || type == ValueType::vect || type == ValueType::frame;
}

namespace typed_numeric_detail
{

template <typename ConvertToken>
inline std::optional<QString> convertTuple(const QString& text, const ValueType type,
    ConvertToken&& convertToken, const bool canonicalTuple = false)
{
    const QString trimmed = text.trimmed();
    if ( trimmed.isEmpty() )
    {
        return QString{};
    }
    const bool parenthesized = trimmed.startsWith(QLatin1Char('('));
    if ( parenthesized != trimmed.endsWith(QLatin1Char(')')) )
    {
        return std::nullopt;
    }
    const qsizetype begin = parenthesized ? 1 : 0;
    const qsizetype end = trimmed.size() - (parenthesized ? 1 : 0);
    QString result{};
    result.reserve(trimmed.size());
    if ( parenthesized )
    {
        result += QLatin1Char('(');
    }
    qsizetype position = begin;
    std::size_t count = 0;
    bool pendingSeparator = false;
    while ( position < end )
    {
        const QChar character = trimmed[position];
        if ( character.isSpace() )
        {
            result += character;
            ++position;
            continue;
        }
        if ( character == QLatin1Char('/') || character == QLatin1Char(':')
            || character == QLatin1Char(';')
            || (canonicalTuple && character == QLatin1Char(',')) )
        {
            if ( count == 0 || pendingSeparator )
            {
                return std::nullopt;
            }
            pendingSeparator = true;
            result += character == QLatin1Char(',') ? QLatin1Char('/') : character;
            ++position;
            continue;
        }
        const qsizetype tokenStart = position;
        while ( position < end && !trimmed[position].isSpace()
            && trimmed[position] != QLatin1Char('/') && trimmed[position] != QLatin1Char(':')
            && trimmed[position] != QLatin1Char(';')
            && (!canonicalTuple || trimmed[position] != QLatin1Char(',')) )
        {
            ++position;
        }
        const QString token = convertToken(trimmed.mid(tokenStart, position - tokenStart));
        if ( token.isEmpty() )
        {
            return std::nullopt;
        }
        result += token;
        ++count;
        pendingSeparator = false;
    }
    if ( count == 0 || pendingSeparator
        || (type == ValueType::frame && count != 3 && count != 6) )
    {
        return std::nullopt;
    }
    if ( parenthesized )
    {
        result += QLatin1Char(')');
    }
    return result;
}

} // namespace typed_numeric_detail

inline QString localizeTypedNumericText(const QString& canonical, const ValueType type)
{
    if ( !isTypedNumericText(type) )
    {
        return canonical;
    }
    const auto convert = [](const QString& token)
    {
        return canonicalUiNumericText(token, QLocale::c()).isEmpty()
            ? QString{} : localizeUiNumericText(token);
    };
    if ( type == ValueType::floating )
    {
        const QString localized = convert(canonical.trimmed());
        return localized.isEmpty() ? canonical : localized;
    }
    const auto localized = typed_numeric_detail::convertTuple(canonical, type, convert, true);
    return localized.value_or(canonical);
}

inline std::optional<QString> canonicalTypedNumericText(const QString& localized,
    const ValueType type, const QLocale& inputLocale)
{
    if ( !isTypedNumericText(type) )
    {
        return localized;
    }
    const auto convert = [&inputLocale](const QString& token)
    {
        return canonicalUiNumericText(token, inputLocale);
    };
    if ( type == ValueType::floating )
    {
        const QString canonical = convert(localized.trimmed());
        return canonical.isEmpty() ? std::nullopt : std::optional<QString>(canonical);
    }
    return typed_numeric_detail::convertTuple(localized, type, convert);
}

inline void setTypedNumericLineEditText(QLineEdit& edit, const QString& canonical,
    const ValueType type, const QByteArray& baselineProperty = {})
{
    const QSignalBlocker blocker(&edit);
    if ( isTypedNumericText(type) )
    {
        edit.setLocale(uiNumericLocale());
    }
    const QString displayed = localizeTypedNumericText(canonical, type);
    edit.setText(displayed);
    edit.setModified(false);
    edit.setProperty("sed.typedNumericCanonicalText", canonical);
    if ( !baselineProperty.isEmpty() )
    {
        edit.setProperty(baselineProperty.constData(), displayed.trimmed());
    }
}

inline void configureTypedNumericLineEdit(QLineEdit& edit, const QString& canonical,
    const ValueType type, QByteArray baselineProperty = {})
{
    setTypedNumericLineEditText(edit, canonical, type, baselineProperty);
    if ( !isTypedNumericText(type) )
    {
        return;
    }
    onUiNumericFormatChanged(&edit, [&edit, type, baselineProperty = std::move(baselineProperty)]
    {
        if ( !edit.isModified() )
        {
            setTypedNumericLineEditText(edit,
                edit.property("sed.typedNumericCanonicalText").toString(), type, baselineProperty);
        }
    });
}

} // namespace sed::ui
