#pragma once

#include "sed/level.hpp"
#include "sed/text.hpp"
#include <bit>
#include <cstdint>

namespace sed::ui
{
inline bool templateEditorAuthoredValuesEqual(const ThingTemplate& a, const ThingTemplate& b)
{
    const auto sameNumber = [](const double left, const double right)
    {
        return std::bit_cast<std::uint64_t>(left) == std::bit_cast<std::uint64_t>(right);
    };
    const auto sameValue = [&](const TplValue& left, const TplValue& right)
    {
        return text::iequalsAscii(left.name, right.name)
            && left.type == right.type && left.adjType == right.adjType
            && left.intValue == right.intValue
            && sameNumber(left.floatValue, right.floatValue)
            && left.stringValue == right.stringValue;
    };
    if (!text::iequalsAscii(a.name, b.name) || !text::iequalsAscii(a.parent, b.parent)
        || a.desc != b.desc || a.values.size() != b.values.size()
        || !sameNumber(a.bbox.x1, b.bbox.x1) || !sameNumber(a.bbox.y1, b.bbox.y1)
        || !sameNumber(a.bbox.z1, b.bbox.z1) || !sameNumber(a.bbox.x2, b.bbox.x2)
        || !sameNumber(a.bbox.y2, b.bbox.y2) || !sameNumber(a.bbox.z2, b.bbox.z2))
        return false;
    for (std::size_t i = 0; i < a.values.size(); ++i)
        if (!sameValue(a.values[i], b.values[i])) return false;
    return true;
}
}
