#pragma once

#include <algorithm>
#include <cmath>

namespace sed::render
{

inline constexpr float kLevelGeometryVertexMarkerMinSizePx = 2.0f;
inline constexpr float kLevelGeometryVertexMarkerMaxSizePx = 8.0f;
inline constexpr float kLevelGeometryVertexMarkerDefaultSizePx = 4.0f;

struct LevelGeometryVertexMarkerSizes
{
    float level{};
    float selectedBase{};
    float selected{};
    float active{};
};

/**
 * Resolves level-geometry vertex point sizes while keeping the established
 * selection hierarchy. The size preference scales each state from its current
 * default so selected and active handles remain visibly larger.
 */
[[nodiscard]] inline LevelGeometryVertexMarkerSizes levelGeometryVertexMarkerSizes(
    const float configuredSizePx,
    const bool forceLevelPoints) noexcept
{
    const float sizePx = std::isfinite(configuredSizePx)
        ? std::clamp(
            configuredSizePx,
            kLevelGeometryVertexMarkerMinSizePx,
            kLevelGeometryVertexMarkerMaxSizePx)
        : kLevelGeometryVertexMarkerDefaultSizePx;
    const float scale = sizePx / kLevelGeometryVertexMarkerDefaultSizePx;
    return forceLevelPoints
        ? LevelGeometryVertexMarkerSizes{ 12.0f * scale, 22.0f * scale, 28.0f * scale, 32.0f * scale }
        : LevelGeometryVertexMarkerSizes{ 4.0f * scale, 10.0f * scale, 13.0f * scale, 15.0f * scale };
}

} // namespace sed::render
