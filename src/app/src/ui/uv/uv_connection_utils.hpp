#pragma once
#include "sed/level.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace sed::ui::uv
{
// Connected-selection propagation requires coincident edges. A fixed world
// proximity threshold can cross a real gap in finely authored geometry.
inline bool uvVerticesCoincide(const Vertex& a, const Vertex& b) noexcept
{
    const auto same = [](double x, double y)
    {
        return std::isfinite(x) && std::isfinite(y)
            && std::abs(x - y) <= 64.0 * std::numeric_limits<double>::epsilon()
                * std::max(std::abs(x), std::abs(y));
    };
    return same(a.x, b.x) && same(a.y, b.y) && same(a.z, b.z);
}
}
