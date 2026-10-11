#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "sed/image.hpp"
#include "sed/assets/cmp.hpp"

namespace sed::assets
{

class MatFile
{
public:
    using MipMap = sed::Image8;

    struct Cel
    {
        std::int32_t width{};
        std::int32_t height{};
        std::uint8_t solidColorIndex{};
        CmpColor solidColor{};
        std::vector<MipMap> mipmaps{};
    };

    static MatFile parse(std::vector<std::byte> data, const CmpPalette* palette = nullptr);

    bool isColor() const noexcept;
    bool isAnimated() const noexcept;
    std::int32_t celCount() const noexcept;
    const sed::ColorInfo& colorInfo() const noexcept;

    const Cel& cel(std::int32_t celIndex) const;
    const std::vector<Cel>& cels() const noexcept;

    sed::Rgba8Image decodeCelRgba8(std::int32_t celIndex) const;

private:
    sed::ColorInfo colorInfo_{};
    bool isColor_{};
    bool isAnimated_{};
    std::uint8_t colorIndex_{};
    std::vector<Cel> cels_{};
};

} // namespace sed::assets
