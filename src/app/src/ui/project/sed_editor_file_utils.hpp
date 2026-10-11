#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include <QByteArray>

#include "sed/assets/sed.hpp"

namespace sed::ui::project
{

inline constexpr std::array<char, 4> SedEditorStateSection{ 'e', 'd', 'i', 't' };
inline constexpr std::array<char, 4> SedHistorySection{ 'h', 'i', 's', 't' };

inline assets::SedOptionalSection makeSedOptionalSection(
    const std::array<char, 4> type,
    const QByteArray& payload)
{
    assets::SedOptionalSection section{};
    section.type = type;
    const auto bytes = std::as_bytes(std::span(
        payload.constData(), static_cast<std::size_t>(payload.size())));
    section.payload.assign(bytes.begin(), bytes.end());
    return section;
}

inline QByteArray findSedOptionalSection(
    const std::span<const assets::SedOptionalSection> sections,
    const std::array<char, 4> type)
{
    const auto found = std::find_if(sections.begin(), sections.end(),
        [type](const assets::SedOptionalSection& section)
        {
            return section.type == type;
        });
    if (found == sections.end())
    {
        return {};
    }
    return QByteArray(
        reinterpret_cast<const char*>(found->payload.data()),
        static_cast<qsizetype>(found->payload.size()));
}

} // namespace sed::ui::project
