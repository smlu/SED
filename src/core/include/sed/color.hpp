#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace sed
{

struct ColorF
{
    float r{ 0.0f };
    float g{ 0.0f };
    float b{ 0.0f };
    float a{ 1.0f }; // alpha or intensity

    constexpr ColorF() noexcept = default;
    constexpr ColorF(float r_, float g_, float b_, float a_ = 1.0f) noexcept : r(r_), g(g_), b(b_), a(a_)
    {
    }

    void normalize() noexcept
    {
        r = std::clamp(r, 0.0f, 1.0f);
        g = std::clamp(g, 0.0f, 1.0f);
        b = std::clamp(b, 0.0f, 1.0f);
        a = std::clamp(a, 0.0f, 1.0f);
    }

    void clamp(const ColorF& min, const ColorF& max) noexcept
    {
        r = std::clamp(r, min.r, max.r);
        g = std::clamp(g, min.g, max.g);
        b = std::clamp(b, min.b, max.b);
        a = std::clamp(a, min.a, max.a);
    }

    ColorF clamped(const ColorF& min, const ColorF& max) const noexcept
    {
        ColorF out = *this;
        out.clamp(min, max);
        return out;
    }

    void setToZero() noexcept
    {
        r = g = b = a = 0.0f;
    }

    bool isZero() const noexcept
    {
        return std::abs(r) <= std::numeric_limits<float>::epsilon()
            && std::abs(g) <= std::numeric_limits<float>::epsilon()
            && std::abs(b) <= std::numeric_limits<float>::epsilon()
            && std::abs(a) <= std::numeric_limits<float>::epsilon();
    }

    bool isRGBZero() const noexcept
    {
        return std::abs(r) <= std::numeric_limits<float>::epsilon()
            && std::abs(g) <= std::numeric_limits<float>::epsilon()
            && std::abs(b) <= std::numeric_limits<float>::epsilon();
    }

    float intensity() const noexcept
    {
        return (r + g + b) / 3.0f;
    }

    float rgbaIntensity() const noexcept
    {
        return intensity();
    }

    friend constexpr ColorF operator+(const ColorF& a, const ColorF& b) noexcept
    {
        return { a.r + b.r, a.g + b.g, a.b + b.b, a.a + b.a };
    }
    friend constexpr ColorF operator-(const ColorF& a, const ColorF& b) noexcept
    {
        return { a.r - b.r, a.g - b.g, a.b - b.b, a.a - b.a };
    }
    friend constexpr ColorF operator*(const ColorF& a, const ColorF& b) noexcept
    {
        return { a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a };
    }
    friend constexpr ColorF operator/(const ColorF& a, const ColorF& b) noexcept
    {
        return { a.r / b.r, a.g / b.g, a.b / b.b, a.a / b.a };
    }

    friend constexpr ColorF operator+(const ColorF& a, float b) noexcept
    {
        return { a.r + b, a.g + b, a.b + b, a.a + b };
    }
    friend constexpr ColorF operator-(const ColorF& a, float b) noexcept
    {
        return { a.r - b, a.g - b, a.b - b, a.a - b };
    }
    friend constexpr ColorF operator*(const ColorF& a, float b) noexcept
    {
        return { a.r * b, a.g * b, a.b * b, a.a * b };
    }
    friend constexpr ColorF operator/(const ColorF& a, float b) noexcept
    {
        return { a.r / b, a.g / b, a.b / b, a.a / b };
    }

    friend constexpr bool operator==(const ColorF& a, const ColorF& b) noexcept
    {
        return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
    }
    friend constexpr bool operator!=(const ColorF& a, const ColorF& b) noexcept
    {
        return !(a == b);
    }
};

inline constexpr ColorF ColorBlack{0.0f, 0.0f, 0.0f, 1.0f};
inline constexpr ColorF ColorBlue{0.0f, 0.0f, 1.0f, 1.0f};
inline constexpr ColorF ColorGreen{0.0f, 1.0f, 0.0f, 1.0f};
inline constexpr ColorF ColorCyan{0.0f, 1.0f, 1.0f, 1.0f};
inline constexpr ColorF ColorRed{1.0f, 0.0f, 0.0f, 1.0f};
inline constexpr ColorF ColorRedDark{0.54f, 0.0f, 0.0f, 1.0f};
inline constexpr ColorF ColorRedScarlet{1.0f, 0.14f, 0.0f, 1.0f};
inline constexpr ColorF ColorWhite{1.0f, 1.0f, 1.0f, 1.0f};
inline constexpr ColorF ColorYellow{1.0f, 1.0f, 0.0f, 1.0f};

constexpr float clamp01(const float v) noexcept
{
    return (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v);
}

/** Clamps an additive light adjustment while preserving values that dim. */
constexpr float clampSignedUnit(const float v) noexcept
{
    return (v < -1.0f) ? -1.0f : ((v > 1.0f) ? 1.0f : v);
}

inline constexpr ColorF intensityToColor(const float intensity) noexcept
{
    const float c = clamp01(intensity);
    return { c, c, c, 1.0f };
}

/** Converts a scalar additive light adjustment without discarding negative dimming values. */
inline constexpr ColorF signedIntensityToColor(const float intensity) noexcept
{
    const float c = clampSignedUnit(intensity);
    return { c, c, c, 1.0f };
}

} // namespace sed
