#pragma once

#include "sed/level.hpp"

#include <QLocale>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <array>
#include <cmath>
#include <limits>
#include <optional>

namespace sed::ui::properties::detail
{

/** Parse the stable plain-text clipboard representation used by the sector
 * point-light Copy/Paste actions.
 *
 * Colors and ranges must be finite and non-negative. Position is signed so
 * sources below or outside the sector remain representable. The color
 * channels are stored as floats, so values outside their finite range are
 * rejected before conversion.
 */
[[nodiscard]] inline std::optional<PointLight> parseSectorPointLightClipboard(
    QString raw)
{
    raw = raw.trimmed();
    if (raw.isEmpty())
    {
        return std::nullopt;
    }

    static const QString kPrefix = QStringLiteral("SED sector point light:");
    if (raw.startsWith(kPrefix, Qt::CaseInsensitive))
    {
        raw.remove(0, kPrefix.size());
    }
    raw.replace(QChar('('), QChar(' '));
    raw.replace(QChar(')'), QChar(' '));

    const QStringList parts = raw.split(
        QRegularExpression(QStringLiteral(R"([\s,;/]+)")),
        Qt::SkipEmptyParts);
    std::array<double, 8> values{};
    bool valid = parts.size() == static_cast<qsizetype>(values.size());
    const QLocale c = QLocale::c();
    for (qsizetype i = 0; valid && i < parts.size(); ++i)
    {
        bool ok = false;
        values[static_cast<std::size_t>(i)] = c.toDouble(parts[i], &ok);
        valid = ok && std::isfinite(values[static_cast<std::size_t>(i)]);
    }

    const double maxColor = static_cast<double>(
        std::numeric_limits<float>::max());
    valid = valid
        && values[0] >= 0.0 && values[1] >= 0.0 && values[2] >= 0.0
        && values[6] >= 0.0 && values[7] >= 0.0
        && values[0] <= maxColor && values[1] <= maxColor
        && values[2] <= maxColor;
    if (!valid)
    {
        return std::nullopt;
    }

    PointLight result{};
    result.color = ColorF{
        static_cast<float>(values[0]),
        static_cast<float>(values[1]),
        static_cast<float>(values[2]),
        1.0f};
    result.position = TVector{ values[3], values[4], values[5] };
    result.minRange = values[6];
    result.maxRange = values[7];
    return result;
}

} // namespace sed::ui::properties::detail
