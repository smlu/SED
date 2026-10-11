#include "sed/assets/mat.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>

#include "sed/binary_reader.hpp"

namespace sed::assets
{

namespace
{

static_assert(std::endian::native == std::endian::little, "MAT parsing currently requires a little-endian host");

#pragma pack(push, 1)
struct MatHeaderRaw
{
    char tag[4]{};
    std::int32_t version{};
    std::int32_t matType{};
    std::int32_t celCount{};
    std::int32_t textureCount{};
    sed::ColorInfo colorInfo{};
};

struct MatTextureHeaderRaw
{
    std::int32_t words[10]{};
};

struct MatTextureDataRaw
{
    std::int32_t sizeX{};
    std::int32_t sizeY{};
    std::int32_t pad0{};
    std::int32_t pad1{};
    std::int32_t pad2{};
    std::int32_t numMipMaps{};
};

struct MatColorHeaderRaw
{
    std::int32_t textype{};
    std::int32_t colornum{};
    std::int32_t pads[4]{};
};
#pragma pack(pop)

static_assert(sizeof(MatHeaderRaw) == 76, "MatHeaderRaw must match MAT file layout (76 bytes)");
static_assert(sizeof(MatTextureHeaderRaw) == 40, "MatTextureHeaderRaw must match MAT file layout (40 bytes)");
static_assert(sizeof(MatTextureDataRaw) == 24, "MatTextureDataRaw must match MAT file layout (24 bytes)");
static_assert(sizeof(MatColorHeaderRaw) == 24, "MatColorHeaderRaw must match MAT file layout (24 bytes)");

template <typename T>
T readStruct(sed::ByteReader& r)
{
    const auto bytes = r.readBytes(sizeof(T));

    T out{};
    std::memcpy(&out, bytes.data(), sizeof(T));
    return out;
}

std::size_t checkedMul(const std::size_t a, const std::size_t b, const char* message)
{
    if (a != 0 && b > (std::numeric_limits<std::size_t>::max)() / a)
    {
        throw std::runtime_error(message);
    }

    return a * b;
}

std::size_t checkedImageBytes(const std::int32_t width, const std::int32_t height, const std::size_t bytesPerPixel, const char* message)
{
    if (width <= 0 || height <= 0)
    {
        throw std::runtime_error(message);
    }

    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    return checkedMul(checkedMul(w, h, message), bytesPerPixel, message);
}

std::uint8_t scaleTo8(const std::uint32_t value, const std::uint32_t bits) noexcept
{
    if (bits == 0)
    {
        return 0;
    }
    if (bits >= 8)
    {
        const std::uint32_t shift = bits - 8;
        return static_cast<std::uint8_t>((value >> shift) & 0xFFu);
    }

    const std::uint32_t max = (1u << bits) - 1u;
    return static_cast<std::uint8_t>((value * 255u + (max / 2u)) / max);
}

std::uint32_t readPixelLE(const std::byte* p, const std::uint32_t pixelSize) noexcept
{
    switch (pixelSize)
    {
    case 1:
        return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[0]));
    case 2:
        return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[0]))
            | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[1])) << 8);
    case 3:
        return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[0]))
            | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[1])) << 8)
            | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[2])) << 16);
    default:
        return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[0]))
            | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[1])) << 8)
            | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[2])) << 16)
            | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[3])) << 24);
    }
}

sed::Image8 decodeIndexedImage8(
    const std::span<const std::byte> indices,
    const std::int32_t width,
    const std::int32_t height,
    const CmpPalette& palette,
    const bool alphaRef)
{
    const std::size_t pixelCount = checkedMul(static_cast<std::size_t>(width), static_cast<std::size_t>(height), "MAT indexed pixel count overflow");
    if (indices.size() < pixelCount)
    {
        throw std::runtime_error("MAT indexed pixel data truncated");
    }

    sed::Image8 out{};
    out.width = width;
    out.height = height;
    out.format = alphaRef ? sed::PixelFormat::Rgba8 : sed::PixelFormat::Rgb8;
    const std::size_t outBpp = alphaRef ? 4u : 3u;
    out.pixels.resize(checkedMul(pixelCount, outBpp, "MAT indexed buffer overflow"));

    for (std::size_t i = 0; i < pixelCount; ++i)
    {
        const std::uint8_t idx = std::to_integer<std::uint8_t>(indices[i]);
        const CmpColor c = palette[idx];

        if (alphaRef)
        {
            out.pixels[i * 4 + 0] = c.r;
            out.pixels[i * 4 + 1] = c.g;
            out.pixels[i * 4 + 2] = c.b;
            out.pixels[i * 4 + 3] = (idx == 0) ? 0u : 255u;
        }
        else
        {
            out.pixels[i * 3 + 0] = c.r;
            out.pixels[i * 3 + 1] = c.g;
            out.pixels[i * 3 + 2] = c.b;
        }
    }

    return out;
}

sed::Image8 decodePackedImage8(const std::span<const std::byte> raw, const sed::ColorInfo& ci, const std::int32_t width, const std::int32_t height)
{
    const std::uint32_t pixelSize = ci.pixelSize();
    const std::size_t expectedRawBytes = checkedImageBytes(width, height, pixelSize, "MAT pixel data size overflow");
    if (raw.size() < expectedRawBytes)
    {
        throw std::runtime_error("MAT pixel data truncated");
    }

    const bool hasAlpha = ci.alphaBPP > 0;
    const std::size_t outBpp = hasAlpha ? 4u : 3u;

    sed::Image8 out{};
    out.width = width;
    out.height = height;
    out.format = hasAlpha ? sed::PixelFormat::Rgba8 : sed::PixelFormat::Rgb8;
    out.pixels.resize(checkedImageBytes(width, height, outBpp, "MAT decoded pixel data size overflow"));

    const std::uint32_t rm = (ci.redBPP == 32) ? 0xFFFFFFFFu : ((ci.redBPP == 0) ? 0u : ((1u << ci.redBPP) - 1u));
    const std::uint32_t gm = (ci.greenBPP == 32) ? 0xFFFFFFFFu : ((ci.greenBPP == 0) ? 0u : ((1u << ci.greenBPP) - 1u));
    const std::uint32_t bm = (ci.blueBPP == 32) ? 0xFFFFFFFFu : ((ci.blueBPP == 0) ? 0u : ((1u << ci.blueBPP) - 1u));
    const std::uint32_t am = (ci.alphaBPP == 0) ? 0u : ((ci.alphaBPP == 32) ? 0xFFFFFFFFu : ((1u << ci.alphaBPP) - 1u));

    const std::size_t pixelCount = checkedMul(static_cast<std::size_t>(width), static_cast<std::size_t>(height), "MAT pixel count overflow");

    for (std::size_t i = 0; i < pixelCount; ++i)
    {
        const std::byte* p = raw.data() + i * pixelSize;
        const std::uint32_t pixel = readPixelLE(p, pixelSize);

        const std::uint32_t r = (rm == 0u) ? 0u : ((pixel >> ci.redShl) & rm);
        const std::uint32_t g = (gm == 0u) ? 0u : ((pixel >> ci.greenShl) & gm);
        const std::uint32_t b = (bm == 0u) ? 0u : ((pixel >> ci.blueShl) & bm);

        const std::uint8_t r8 = scaleTo8(r, ci.redBPP);
        const std::uint8_t g8 = scaleTo8(g, ci.greenBPP);
        const std::uint8_t b8 = scaleTo8(b, ci.blueBPP);

        if (hasAlpha)
        {
            const std::uint32_t a = (pixel >> ci.alphaShl) & am;
            const std::uint8_t a8 = scaleTo8(a, ci.alphaBPP);

            out.pixels[i * 4 + 0] = r8;
            out.pixels[i * 4 + 1] = g8;
            out.pixels[i * 4 + 2] = b8;
            out.pixels[i * 4 + 3] = a8;
            continue;
        }

        out.pixels[i * 3 + 0] = r8;
        out.pixels[i * 3 + 1] = g8;
        out.pixels[i * 3 + 2] = b8;
    }

    return out;
}

} // namespace

MatFile MatFile::parse(std::vector<std::byte> data, const CmpPalette* palette)
{
    MatFile out{};
    sed::ByteReader r(std::span<const std::byte>(data.data(), data.size()));

    const MatHeaderRaw header = readStruct<MatHeaderRaw>(r);

    const std::string_view tag(header.tag, sizeof(header.tag));
    if (tag != std::string_view("MAT ", 4))
    {
        throw std::runtime_error("Not a MAT file");
    }

    if (header.version != 0x32)
    {
        throw std::runtime_error("Unsupported MAT version");
    }

    if (header.celCount <= 0)
    {
        throw std::runtime_error("MAT celCount must be positive");
    }

    out.colorInfo_ = header.colorInfo;

    const CmpPalette fallback = makeGrayscalePalette();
    const CmpPalette* pal = palette ? palette : &fallback;

    if (header.matType == 0)
    {
        out.isColor_ = true;
        out.isAnimated_ = false;

        const MatColorHeaderRaw colorHeader = readStruct<MatColorHeaderRaw>(r);

        if (colorHeader.colornum < 0 || colorHeader.colornum > 255)
        {
            throw std::runtime_error("MAT color index out of range");
        }
        out.colorIndex_ = static_cast<std::uint8_t>(colorHeader.colornum);
        if (out.colorInfo_.mode != sed::ColorMode::Indexed && out.colorInfo_.mode != sed::ColorMode::IndexedCols)
        {
            throw std::runtime_error("MAT color MAT must use indexed color mode");
        }

        const std::uint32_t pixelSize = out.colorInfo_.pixelSize();
        if (pixelSize != 1)
        {
            throw std::runtime_error("MAT color MAT must be 8bpp");
        }

        const CmpColor color = (*pal)[out.colorIndex_];

        Cel cel{};
        cel.width = 64;
        cel.height = 64;
        cel.solidColorIndex = out.colorIndex_;
        cel.solidColor = color;

        MipMap mip{};
        mip.width = 64;
        mip.height = 64;
        mip.format = sed::PixelFormat::Rgb8;
        mip.pixels.resize(checkedImageBytes(mip.width, mip.height, 3u, "MAT color mipmap size overflow"));
        for (std::size_t i = 0; i < mip.pixels.size(); i += 3)
        {
            mip.pixels[i + 0] = color.r;
            mip.pixels[i + 1] = color.g;
            mip.pixels[i + 2] = color.b;
        }

        cel.mipmaps.push_back(std::move(mip));
        out.cels_.push_back(std::move(cel));
        return out;
    }

    if ((out.colorInfo_.bpp % 8u) != 0u)
    {
        throw std::runtime_error("MAT bpp must be a multiple of 8");
    }
    if (out.colorInfo_.bpp > 32u)
    {
        throw std::runtime_error("Unsupported MAT bpp");
    }

    const std::uint32_t pixelSize = out.colorInfo_.pixelSize();
    if (pixelSize == 0 || pixelSize > 4)
    {
        throw std::runtime_error("Unsupported MAT pixel size");
    }

    const bool indexed = out.colorInfo_.mode == sed::ColorMode::Indexed || out.colorInfo_.mode == sed::ColorMode::IndexedCols;
    if (indexed && pixelSize != 1)
    {
        throw std::runtime_error("MAT indexed textures must be 8bpp");
    }

    if ( static_cast<std::size_t>(header.celCount)
        > r.remaining() / sizeof(MatTextureHeaderRaw) )
    {
        throw std::runtime_error("MAT cel count exceeds remaining data");
    }

    std::vector<MatTextureHeaderRaw> textureHeaders{};
    textureHeaders.reserve(static_cast<std::size_t>(header.celCount));
    for (std::int32_t i = 0; i < header.celCount; ++i)
    {
        const MatTextureHeaderRaw textureHeader = readStruct<MatTextureHeaderRaw>(r);
        if (textureHeader.words[1] < 0 || textureHeader.words[1] > 255)
        {
            throw std::runtime_error("MAT solid color index out of range");
        }
        textureHeaders.push_back(textureHeader);
    }

    const std::size_t minimumCelDataBytes = sizeof(MatTextureDataRaw) + pixelSize;
    if ( static_cast<std::size_t>(header.celCount) > r.remaining() / minimumCelDataBytes )
    {
        throw std::runtime_error("MAT cel count exceeds remaining data");
    }

    out.isColor_ = false;
    out.cels_.reserve(static_cast<std::size_t>(header.celCount));

    for (std::int32_t i = 0; i < header.celCount; ++i)
    {
        const MatTextureDataRaw texData = readStruct<MatTextureDataRaw>(r);

        if (texData.sizeX <= 0 || texData.sizeY <= 0)
        {
            throw std::runtime_error("Invalid MAT texture size");
        }
        if (texData.numMipMaps <= 0)
        {
            throw std::runtime_error("MAT NumMipMaps must be positive");
        }
        const auto maximumMipMaps = std::bit_width(static_cast<std::uint32_t>(
            std::min(texData.sizeX, texData.sizeY)));
        if ( static_cast<std::uint32_t>(texData.numMipMaps) > maximumMipMaps
            || static_cast<std::size_t>(texData.numMipMaps) > r.remaining() / pixelSize )
        {
            throw std::runtime_error("MAT mipmap count exceeds dimensions or remaining data");
        }

        Cel cel{};
        cel.width = texData.sizeX;
        cel.height = texData.sizeY;
        cel.solidColorIndex = static_cast<std::uint8_t>(
            textureHeaders[static_cast<std::size_t>(i)].words[1]);
        cel.solidColor = (*pal)[cel.solidColorIndex];
        cel.mipmaps.reserve(static_cast<std::size_t>(texData.numMipMaps));

        std::int32_t w = texData.sizeX;
        std::int32_t h = texData.sizeY;
        for (std::int32_t m = 0; m < texData.numMipMaps; ++m)
        {
            if (w <= 0 || h <= 0)
            {
                throw std::runtime_error("Invalid MAT mipmap size");
            }

            const std::size_t rawBytes = checkedImageBytes(w, h, pixelSize, "MAT mipmap raw byte size overflow");
            const auto raw = r.readBytes(rawBytes);

            if (indexed)
            {
                // Classic JKDF2/MotS indexed MAT headers only define mode and
                // bpp reliably; the remaining ColorInfo words occur as a few
                // repeated legacy byte patterns in the shipped JKDF2/MotS
                // assets. Treat alphaBPP only as the original zero/nonzero
                // alpha-reference marker. Its nonzero numeric value is not a
                // usable channel width for an 8-bit palette index.
                cel.mipmaps.push_back(decodeIndexedImage8(
                    raw, w, h, *pal, out.colorInfo_.alphaBPP != 0));
            }
            else
            {
                cel.mipmaps.push_back(decodePackedImage8(raw, out.colorInfo_, w, h));
            }

            w /= 2;
            h /= 2;
        }

        out.cels_.push_back(std::move(cel));
    }

    out.isAnimated_ = out.cels_.size() > 1;
    return out;
}

bool MatFile::isColor() const noexcept
{
    return isColor_;
}

bool MatFile::isAnimated() const noexcept
{
    return isAnimated_;
}

std::int32_t MatFile::celCount() const noexcept
{
    return static_cast<std::int32_t>(cels_.size());
}

const sed::ColorInfo& MatFile::colorInfo() const noexcept
{
    return colorInfo_;
}

const MatFile::Cel& MatFile::cel(const std::int32_t celIndex) const
{
    if (celIndex < 0 || static_cast<std::size_t>(celIndex) >= cels_.size())
    {
        throw std::runtime_error("MAT cel index out of range");
    }
    return cels_[static_cast<std::size_t>(celIndex)];
}

const std::vector<MatFile::Cel>& MatFile::cels() const noexcept
{
    return cels_;
}

sed::Rgba8Image MatFile::decodeCelRgba8(const std::int32_t celIndex) const
{
    const Cel& cel = this->cel(celIndex);
    if (cel.mipmaps.empty())
    {
        throw std::runtime_error("MAT cel has no mipmaps");
    }

    const MipMap& mip0 = cel.mipmaps[0];
    if (mip0.empty())
    {
        return {};
    }

    sed::Rgba8Image image{};
    image.width = mip0.width;
    image.height = mip0.height;

    const std::size_t pixelCount = checkedMul(static_cast<std::size_t>(mip0.width), static_cast<std::size_t>(mip0.height), "MAT image pixel count overflow");
    image.rgba.resize(checkedMul(pixelCount, 4u, "MAT image byte size overflow"));

    if (mip0.format == sed::PixelFormat::Rgba8)
    {
        const std::size_t expectedBytes = checkedMul(pixelCount, 4u, "MAT image byte size overflow");
        if (mip0.pixels.size() < expectedBytes)
        {
            throw std::runtime_error("MAT decoded pixel data truncated");
        }

        std::memcpy(image.rgba.data(), mip0.pixels.data(), expectedBytes);
        return image;
    }

    const std::size_t expectedBytes = checkedMul(pixelCount, 3u, "MAT image byte size overflow");
    if (mip0.pixels.size() < expectedBytes)
    {
        throw std::runtime_error("MAT decoded pixel data truncated");
    }

    for (std::size_t i = 0; i < pixelCount; ++i)
    {
        image.rgba[i * 4 + 0] = mip0.pixels[i * 3 + 0];
        image.rgba[i * 4 + 1] = mip0.pixels[i * 3 + 1];
        image.rgba[i * 4 + 2] = mip0.pixels[i * 3 + 2];
        image.rgba[i * 4 + 3] = 255;
    }

    return image;
}

} // namespace sed::assets
