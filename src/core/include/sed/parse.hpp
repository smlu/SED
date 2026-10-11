#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "sed/level.hpp"
#include "sed/text.hpp"

namespace sed::parse
{

inline std::optional<sed::SurfaceRef> surfaceRef(const std::string_view text)
{
    std::string_view trimmed = sed::text::trimView(text);
    if (trimmed.empty())
    {
        return std::nullopt;
    }

    if (sed::text::iequalsAscii(trimmed, "none") || trimmed == "-1")
    {
        return std::nullopt;
    }

    std::string normalized(trimmed);
    for (char& ch : normalized)
    {
        if (ch == ':' || ch == ',' || ch == ';' || ch == '(' || ch == ')')
        {
            ch = ' ';
        }
    }

    const auto parts = sed::text::splitWords(normalized);
    if (parts.size() < 2)
    {
        return std::nullopt;
    }

    const auto sec = sed::text::parseInt<std::int32_t>(parts[0]);
    const auto surf = sed::text::parseInt<std::int32_t>(parts[1]);
    if (!sec || !surf)
    {
        return std::nullopt;
    }

    return sed::SurfaceRef{ *sec, *surf };
}

} // namespace sed::parse

namespace sed::format
{

inline std::string surfaceRef(const sed::SurfaceRef ref)
{
    return std::to_string(ref.sector) + ":" + std::to_string(ref.surface);
}

} // namespace sed::format
