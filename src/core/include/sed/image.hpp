#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sed
{

enum class PixelFormat : std::uint8_t
{
    Rgb8,
    Rgba8,
};

enum class ColorMode : std::uint32_t
{
    Indexed = 0,
    RGB = 1,
    RGBA = 2,
    IndexedCols = 3,
};

struct ColorInfo
{
    ColorMode mode{};
    std::uint32_t bpp{};

    std::uint32_t redBPP{};
    std::uint32_t greenBPP{};
    std::uint32_t blueBPP{};

    std::uint32_t redShl{};
    std::uint32_t greenShl{};
    std::uint32_t blueShl{};

    std::uint32_t redShr{};
    std::uint32_t greenShr{};
    std::uint32_t blueShr{};

    std::uint32_t alphaBPP{};
    std::uint32_t alphaShl{};
    std::uint32_t alphaShr{};

    constexpr std::uint32_t pixelSize() const noexcept
    {
        return bpp / 8u;
    }
};

static_assert(sizeof(ColorMode) == 4, "ColorMode must match Delphi file layout (4 bytes)");
static_assert(sizeof(ColorInfo) == 56, "ColorInfo must match Delphi file layout (56 bytes)");

struct Rgba8Image
{
    std::int32_t width{};
    std::int32_t height{};
    std::vector<std::uint8_t> rgba{};

    bool empty() const noexcept
    {
        return width <= 0 || height <= 0 || rgba.empty();
    }

    std::size_t byteSize() const noexcept
    {
        return rgba.size();
    }
};

struct Image8
{
    std::int32_t width{};
    std::int32_t height{};
    PixelFormat format{ PixelFormat::Rgba8 };
    std::vector<std::uint8_t> pixels{};

    bool empty() const noexcept
    {
        return width <= 0 || height <= 0 || pixels.empty();
    }

    std::size_t bytesPerPixel() const noexcept
    {
        return (format == PixelFormat::Rgb8) ? 3u : 4u;
    }

    bool hasAlpha() const noexcept
    {
        return format == PixelFormat::Rgba8;
    }

    std::size_t byteSize() const noexcept
    {
        return pixels.size();
    }
};

} // namespace sed
