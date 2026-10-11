#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sed::ui::resources::detail
{

struct SpriteAnimationClockStep
{
    double exactFrames{};
    double wholeFrames{};
    double remainder{};
    int advance{};
};

inline SpriteAnimationClockStep spriteAnimationClockStep(
    const std::int64_t deltaMs, const int fps, const double previousRemainder, const int celCount)
{
    if ( deltaMs < 0 || fps <= 0 || fps > 99 || celCount <= 0
        || !std::isfinite(previousRemainder) || previousRemainder < 0.0 || previousRemainder >= 1.0 )
    {
        return {};
    }
    const double exactFrames = static_cast<double>(deltaMs) * static_cast<double>(fps) / 1000.0
        + previousRemainder;
    const double wholeFrames = std::floor(exactFrames);
    return SpriteAnimationClockStep{
        .exactFrames = exactFrames,
        .wholeFrames = wholeFrames,
        .remainder = exactFrames - wholeFrames,
        .advance = static_cast<int>(std::fmod(wholeFrames, static_cast<double>(celCount))),
    };
}

inline int spriteAnimationNextDelayMs(
    const int fps, const double remainder, const std::int64_t millisecondsSinceAdvance)
{
    if ( fps <= 0 || fps > 99 || !std::isfinite(remainder) || remainder < 0.0 || remainder >= 1.0
        || millisecondsSinceAdvance < 0 )
    {
        return 0;
    }
    // Integral clock milliseconds and FPS produce thousandth-frame phase.
    // Quantize that phase before ceil-division so .96 at 20 FPS means exactly
    // 2 ms, rather than a binary-rounding artifact of 3 ms. A phase rounding
    // to the boundary remains overdue; it must not wrap to a full period.
    const int phaseUnits = static_cast<int>(std::lround(remainder * 1000.0));
    const int remainingUnits = 1000 - phaseUnits;
    if ( remainingUnits <= 0 )
    {
        return 1;
    }
    const int boundaryDelayMs = (remainingUnits + fps - 1) / fps;
    if ( millisecondsSinceAdvance >= boundaryDelayMs )
    {
        return 1;
    }
    // Compare before multiplication: even a very large upload delay cannot
    // overflow the remaining-phase arithmetic.
    const int consumedUnits = static_cast<int>(millisecondsSinceAdvance) * fps;
    return std::max(1, (remainingUnits - consumedUnits + fps - 1) / fps);
}

} // namespace sed::ui::resources::detail
