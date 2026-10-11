#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace sed::assets
{

struct CmpColor
{
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
};

using CmpPalette = std::array<CmpColor, 256>;
using CmpTranslation = std::array<std::uint8_t, 256>;
using CmpLightLevels = std::array<CmpTranslation, 64>;
using CmpLightRamp = std::array<CmpColor, 64>;

/** Classic CMP color data used by indexed MAT rendering. */
struct CmpFile
{
    CmpPalette palette{};
    /** Palette-index translation for every classic scalar light level. */
    CmpLightLevels lightLevels{};
    /**
     * The final classic light-level translation table. Original SED applies
     * this table to the level's master palette when a Sector CMP is active.
     */
    CmpTranslation translation{};
};

std::optional<CmpFile> parseCmp(std::span<const std::byte> data) noexcept;
std::optional<CmpPalette> parseCmpPalette(std::span<const std::byte> data) noexcept;
CmpPalette applyCmpTranslation(
    const CmpPalette& masterPalette,
    const CmpTranslation& translation) noexcept;
CmpLightRamp makeCmpLightRamp(
    const CmpFile& cmp,
    std::uint8_t paletteIndex) noexcept;
CmpPalette makeGrayscalePalette() noexcept;

} // namespace sed::assets
