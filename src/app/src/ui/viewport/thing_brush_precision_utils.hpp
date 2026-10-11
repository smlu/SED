#pragma once

#include <cmath>
#include <cstdint>

namespace sed::ui::viewport
{

/** Deterministic legacy millimeter seed, with defined wrapping for every finite float. */
inline std::uint32_t thingBrushCoordinateSeed(const float value) noexcept
{
    if (!std::isfinite(value)) return 0u;
    constexpr double modulus = 4294967296.0;
    double wrapped = std::fmod(std::round(static_cast<double>(value) * 1000.0), modulus);
    if (wrapped < 0.0) wrapped += modulus;
    return static_cast<std::uint32_t>(wrapped);
}

} // namespace sed::ui::viewport
