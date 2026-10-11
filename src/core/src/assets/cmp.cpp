#include "sed/assets/cmp.hpp"

#include <algorithm>
#include <cstring>

namespace sed::assets
{

namespace
{

constexpr std::size_t kCmpHeaderSize = 64;
constexpr std::size_t kCmpPaletteSize = 256 * 3;
constexpr std::size_t kCmpTranslationTableCount = 64;
constexpr std::size_t kCmpTranslationSize = 256;
constexpr std::size_t kCmpTranslationOffset = kCmpHeaderSize + kCmpPaletteSize;
constexpr std::size_t kCmpFinalTranslationOffset = kCmpTranslationOffset
    + (kCmpTranslationTableCount - 1) * kCmpTranslationSize;

} // namespace

std::optional<CmpFile> parseCmp(const std::span<const std::byte> data) noexcept
{
    if (data.size() < kCmpFinalTranslationOffset + kCmpTranslationSize)
    {
        return std::nullopt;
    }

    const char* sig = reinterpret_cast<const char*>(data.data());
    if (!(sig[0] == 'C' && sig[1] == 'M' && sig[2] == 'P' && sig[3] == ' '))
    {
        return std::nullopt;
    }

    CmpFile file{};
    const std::byte* p = data.data() + kCmpHeaderSize;

    for (std::size_t i = 0; i < file.palette.size(); ++i)
    {
        file.palette[i].r = std::to_integer<std::uint8_t>(p[i * 3 + 0]);
        file.palette[i].g = std::to_integer<std::uint8_t>(p[i * 3 + 1]);
        file.palette[i].b = std::to_integer<std::uint8_t>(p[i * 3 + 2]);
    }

    for (std::size_t level = 0; level < file.lightLevels.size(); ++level)
    {
        for (std::size_t index = 0;
            index < file.lightLevels[level].size();
            ++index)
        {
            file.lightLevels[level][index] = std::to_integer<std::uint8_t>(
                data[kCmpTranslationOffset + level * kCmpTranslationSize + index]);
        }
    }
    file.translation = file.lightLevels.back();

    return file;
}

std::optional<CmpPalette> parseCmpPalette(const std::span<const std::byte> data) noexcept
{
    if (data.size() < kCmpHeaderSize + kCmpPaletteSize)
    {
        return std::nullopt;
    }
    const char* signature = reinterpret_cast<const char*>(data.data());
    if (!(signature[0] == 'C' && signature[1] == 'M'
            && signature[2] == 'P' && signature[3] == ' '))
    {
        return std::nullopt;
    }

    CmpPalette palette{};
    const std::byte* bytes = data.data() + kCmpHeaderSize;
    for (std::size_t index = 0; index < palette.size(); ++index)
    {
        palette[index].r = std::to_integer<std::uint8_t>(bytes[index * 3 + 0]);
        palette[index].g = std::to_integer<std::uint8_t>(bytes[index * 3 + 1]);
        palette[index].b = std::to_integer<std::uint8_t>(bytes[index * 3 + 2]);
    }
    return palette;
}

CmpPalette applyCmpTranslation(
    const CmpPalette& masterPalette,
    const CmpTranslation& translation) noexcept
{
    CmpPalette result{};
    for (std::size_t index = 0; index < result.size(); ++index)
    {
        result[index] = masterPalette[translation[index]];
    }
    return result;
}

CmpLightRamp makeCmpLightRamp(
    const CmpFile& cmp,
    const std::uint8_t paletteIndex) noexcept
{
    CmpLightRamp ramp{};
    for (std::size_t level = 0; level < ramp.size(); ++level)
    {
        ramp[level] = cmp.palette[cmp.lightLevels[level][paletteIndex]];
    }
    return ramp;
}

CmpPalette makeGrayscalePalette() noexcept
{
    CmpPalette palette{};
    for (std::size_t i = 0; i < palette.size(); ++i)
    {
        const auto v = static_cast<std::uint8_t>(i);
        palette[i] = CmpColor{ v, v, v };
    }
    return palette;
}

} // namespace sed::assets
