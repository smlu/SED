#pragma once

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include "sed/text.hpp"
#include "sed/format_number.hpp"

namespace sed
{

struct FrameValue
{
    double x{};
    double y{};
    double z{};
    double pch{};
    double yaw{};
    double rol{};
};

} // namespace sed

namespace sed::parse
{

// Parses a JKL/JED-style frame value. Accepts formats like:
// - (x/y/z:pch/yaw/rol)
// - (x/y/z)
// - x y z pch yaw rol
// Separators '/', ':', ',', ';', and parentheses are treated as whitespace.
// Exactly three position components or six position/orientation components are required.
inline std::optional<sed::FrameValue> frameValue(const std::string_view text)
{
    const std::string_view trimmed = sed::text::trimView(text);
    if (trimmed.empty())
    {
        return std::nullopt;
    }

    std::string normalized;
    normalized.reserve(trimmed.size());

    for (const char ch : trimmed)
    {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isspace(uch) || ch == '/' || ch == ':' || ch == ',' || ch == ';' || ch == '(' || ch == ')')
        {
            normalized.push_back(' ');
        }
        else
        {
            normalized.push_back(ch);
        }
    }

    const auto parts = sed::text::splitWords(normalized);
    if (parts.size() != 3 && parts.size() != 6)
    {
        return std::nullopt;
    }

    auto parseAt = [&](const std::size_t i) -> std::optional<double>
    {
        return (i < parts.size()) ? sed::text::parseDouble(parts[i]) : std::nullopt;
    };

    const auto x = parseAt(0);
    const auto y = parseAt(1);
    const auto z = parseAt(2);
    if (!x || !y || !z)
    {
        return std::nullopt;
    }

    sed::FrameValue out{};
    out.x = *x;
    out.y = *y;
    out.z = *z;
    out.pch = 0.0;
    out.yaw = 0.0;
    out.rol = 0.0;

    if (parts.size() == 6)
    {
        const auto pch = parseAt(3);
        const auto yaw = parseAt(4);
        const auto rol = parseAt(5);
        if (!pch || !yaw || !rol)
        {
            return std::nullopt;
        }

        out.pch = *pch;
        out.yaw = *yaw;
        out.rol = *rol;
    }

    return out;
}

} // namespace sed::parse

namespace sed::format
{

inline std::string frameValue(const sed::FrameValue& v)
{
    return '(' + authoredDouble(v.x) + '/' + authoredDouble(v.y) + '/'
        + authoredDouble(v.z) + ':' + authoredDouble(v.pch) + '/'
        + authoredDouble(v.yaw) + '/' + authoredDouble(v.rol) + ')';
}

// Explicit presentation precision remains available to callers that need it.
inline std::string frameValue(const sed::FrameValue& v, const int decimals)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::fixed << std::setprecision(std::clamp(decimals, 0, 12));
    oss << '(' << v.x << '/' << v.y << '/' << v.z << ':' << v.pch << '/' << v.yaw << '/' << v.rol << ')';
    return oss.str();
}

inline std::string frameValue(
    const double x,
    const double y,
    const double z,
    const double pch,
    const double yaw,
    const double rol,
    const int decimals)
{
    return sed::format::frameValue(sed::FrameValue{ x, y, z, pch, yaw, rol }, decimals);
}

inline std::string frameValue(
    const double x, const double y, const double z,
    const double pch, const double yaw, const double rol)
{
    return sed::format::frameValue(sed::FrameValue{x, y, z, pch, yaw, rol});
}

} // namespace sed::format
