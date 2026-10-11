#pragma once

#include "sed/geometry.hpp"
#include "sed/level.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace sed::ui::surface
{

/**
 * UI decomposition of signed RGB surface light. The stored level value stays
 * a signed RGB triplet. QColor cannot display negative channels, so the popup
 * shows their absolute magnitude and keeps only the brighten/dim polarity in
 * the signed intensity value.
 */
struct SignedExtraLightColor
{
    ColorF displayColor{ColorBlack};
    float intensity{};
};

inline SignedExtraLightColor splitSignedExtraLightColor(const ColorF& extraLight) noexcept
{
    const float red = clampSignedUnit(extraLight.r);
    const float green = clampSignedUnit(extraLight.g);
    const float blue = clampSignedUnit(extraLight.b);
    const float magnitude = std::max({std::abs(red), std::abs(green), std::abs(blue)});
    if (magnitude <= std::numeric_limits<float>::epsilon())
    {
        return SignedExtraLightColor{
            ColorF{0.0f, 0.0f, 0.0f, clamp01(extraLight.a)},
            0.0f,
        };
    }

    // Valid editor-produced values share one sign. For legacy mixed-sign RGB,
    // use the dominant channel so reopening the popup remains deterministic.
    float dominant = red;
    if (std::abs(green) > std::abs(dominant))
    {
        dominant = green;
    }
    if (std::abs(blue) > std::abs(dominant))
    {
        dominant = blue;
    }
    const float sign = dominant < 0.0f ? -1.0f : 1.0f;

    return SignedExtraLightColor{
        ColorF{
            clamp01(std::abs(red)),
            clamp01(std::abs(green)),
            clamp01(std::abs(blue)),
            clamp01(extraLight.a),
        },
        sign * magnitude,
    };
}

inline ColorF joinSignedExtraLightColor(
    const ColorF& displayColor,
    const float intensity,
    const float alpha) noexcept
{
    const float signedIntensity = clampSignedUnit(intensity);
    const float magnitude = std::abs(signedIntensity);
    const float red = clamp01(displayColor.r);
    const float green = clamp01(displayColor.g);
    const float blue = clamp01(displayColor.b);
    const float sourceMagnitude = std::max({red, green, blue});

    // The popup normally keeps the visible RGB value synchronized with the
    // absolute intensity. Be defensive for callers that provide an
    // inconsistent pair: preserve hue/saturation and scale Value to the
    // requested magnitude. With black there is no hue to preserve, so use a
    // neutral light. This makes "black, then -0.5" a useful half-strength dim
    // rather than multiplying zero by a negative number and doing nothing.
    float scaledRed = magnitude;
    float scaledGreen = magnitude;
    float scaledBlue = magnitude;
    if (sourceMagnitude > std::numeric_limits<float>::epsilon())
    {
        const float scale = magnitude / sourceMagnitude;
        scaledRed = red * scale;
        scaledGreen = green * scale;
        scaledBlue = blue * scale;
    }

    const float sign = signedIntensity < 0.0f ? -1.0f : 1.0f;
    return ColorF{
        scaledRed * sign,
        scaledGreen * sign,
        scaledBlue * sign,
        clamp01(alpha),
    };
}

/**
 * Alpha on IJIM surface extra light controls adjoin transparency. It is only
 * meaningful for a rendered adjoin face, not for hidden, vertex, or wireframe
 * geometry modes.
 */
inline bool surfaceExtraLightSupportsAlpha(const Surface& surface) noexcept
{
    const auto geo = static_cast<TGeoMode>(surface.geo);
    return surface.adjoin.has_value() && (geo == TGeoMode::Solid || geo == TGeoMode::Texture);
}

} // namespace sed::ui::surface
