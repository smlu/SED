#include "test_common.hpp"

#include "sed/assets/cmp.hpp"
#include "sed/assets/mat.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sed::tests
{

void runMatCmpTests()
{
    const sed::assets::CmpPalette pal = sed::assets::makeGrayscalePalette();

    auto pushU32LE = [](std::vector<std::byte>& buf, const std::uint32_t v)
    {
        buf.push_back(static_cast<std::byte>(v & 0xFFu));
        buf.push_back(static_cast<std::byte>((v >> 8) & 0xFFu));
        buf.push_back(static_cast<std::byte>((v >> 16) & 0xFFu));
        buf.push_back(static_cast<std::byte>((v >> 24) & 0xFFu));
    };

    auto pushI32LE = [&](std::vector<std::byte>& buf, const std::int32_t v)
    {
        pushU32LE(buf, static_cast<std::uint32_t>(v));
    };

    auto pushColorInfo = [&](std::vector<std::byte>& buf, const sed::ColorInfo& ci)
    {
        pushU32LE(buf, static_cast<std::uint32_t>(ci.mode));
        pushU32LE(buf, ci.bpp);

        pushU32LE(buf, ci.redBPP);
        pushU32LE(buf, ci.greenBPP);
        pushU32LE(buf, ci.blueBPP);

        pushU32LE(buf, ci.redShl);
        pushU32LE(buf, ci.greenShl);
        pushU32LE(buf, ci.blueShl);

        pushU32LE(buf, ci.redShr);
        pushU32LE(buf, ci.greenShr);
        pushU32LE(buf, ci.blueShr);

        pushU32LE(buf, ci.alphaBPP);
        pushU32LE(buf, ci.alphaShl);
        pushU32LE(buf, ci.alphaShr);
    };

    {
        std::vector<std::byte> cmpBytes{};
        cmpBytes.resize(64);
        cmpBytes[0] = std::byte{ 'C' };
        cmpBytes[1] = std::byte{ 'M' };
        cmpBytes[2] = std::byte{ 'P' };
        cmpBytes[3] = std::byte{ ' ' };
        for (std::uint32_t i = 0; i < 256; ++i)
        {
            cmpBytes.push_back(static_cast<std::byte>(i));
            cmpBytes.push_back(static_cast<std::byte>(0));
            cmpBytes.push_back(static_cast<std::byte>(0));
        }

        const auto parsedPal = sed::assets::parseCmpPalette(cmpBytes);
        check(parsedPal.has_value(), "CMP.parseCmpPalette");
        if (parsedPal)
        {
            check((*parsedPal)[10].r == 10, "CMP.palette.r");
            check((*parsedPal)[10].g == 0, "CMP.palette.g");
            check((*parsedPal)[10].b == 0, "CMP.palette.b");
        }
    }

    {
        std::vector<std::byte> cmpBytes(64 + 256 * 3 + 64 * 256);
        cmpBytes[0] = std::byte{ 'C' };
        cmpBytes[1] = std::byte{ 'M' };
        cmpBytes[2] = std::byte{ 'P' };
        cmpBytes[3] = std::byte{ ' ' };
        for (std::uint32_t i = 0; i < 256; ++i)
        {
            cmpBytes[64 + i * 3 + 0] = static_cast<std::byte>(i);
            cmpBytes[64 + i * 3 + 1] = static_cast<std::byte>(255u - i);
            cmpBytes[64 + i * 3 + 2] = static_cast<std::byte>(i / 2u);
            for (std::uint32_t lightLevel = 0; lightLevel < 64; ++lightLevel)
            {
                cmpBytes[64 + 256 * 3 + lightLevel * 256 + i] =
                    static_cast<std::byte>((i + lightLevel + 17u) & 0xffu);
            }
        }

        const auto parsed = sed::assets::parseCmp(cmpBytes);
        check(parsed.has_value(),
            "CMP.parse retains the classic palette translation table");
        if (parsed)
        {
            check(parsed->lightLevels[0][0] == 17
                    && parsed->lightLevels[31][208] == 0
                    && parsed->lightLevels[63][176] == 0,
                "CMP.parse retains every classic 256-entry light-level table");
            check(parsed->translation[0] == 80
                    && parsed->translation[176] == 0,
                "CMP.parse reads the final classic 256-entry translation table");

            const sed::assets::CmpLightRamp solidRamp =
                sed::assets::makeCmpLightRamp(*parsed, 6);
            check(solidRamp[0].r == 23
                    && solidRamp[31].r == 54
                    && solidRamp[63].r == 86,
                "CMP.lightRamp resolves a Solid MAT palette index through all lighting tables");

            const sed::assets::CmpPalette translated =
                sed::assets::applyCmpTranslation(parsed->palette, parsed->translation);
            check(translated[0].r == 80
                    && translated[0].g == 175
                    && translated[176].r == 0
                    && translated[176].g == 255,
                "CMP.applyTranslation maps every output index through the retained table");
        }

        cmpBytes.resize(64 + 256 * 3 + 63 * 256 + 255);
        check(!sed::assets::parseCmp(cmpBytes).has_value(),
            "CMP.parse rejects a truncated classic translation table");
    }

    {
        std::vector<std::byte> mat{};
        mat.push_back(std::byte{ 'M' });
        mat.push_back(std::byte{ 'A' });
        mat.push_back(std::byte{ 'T' });
        mat.push_back(std::byte{ ' ' });
        pushI32LE(mat, 0x32);
        pushI32LE(mat, 2); // texture MAT
        pushI32LE(mat, 1);
        pushI32LE(mat, 1);

        sed::ColorInfo ci{};
        ci.mode = sed::ColorMode::Indexed;
        ci.bpp  = 8;
        pushColorInfo(mat, ci);

        pushI32LE(mat, 8);  // texture_type: sampled texture
        pushI32LE(mat, 37); // solidColor palette index
        for (int i = 0; i < 8; ++i)
        {
            pushI32LE(mat, 0);
        }

        pushI32LE(mat, 2);
        pushI32LE(mat, 2);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 1);

        mat.push_back(std::byte{ 0 });
        mat.push_back(std::byte{ 1 });
        mat.push_back(std::byte{ 2 });
        mat.push_back(std::byte{ 3 });

        const sed::assets::MatFile mf = sed::assets::MatFile::parse(std::move(mat), &pal);
        check(!mf.isColor(), "MAT.texture.isColor");
        check(mf.celCount() == 1, "MAT.texture.celCount");
        check(mf.cel(0).solidColorIndex == 37,
            "MAT.texture retains the per-cel solidColor palette index");
        check(mf.cel(0).solidColor.r == 37
                && mf.cel(0).solidColor.g == 37
                && mf.cel(0).solidColor.b == 37,
            "MAT.texture resolves the per-cel solidColor through its CMP palette");

        check(mf.cel(0).mipmaps.size() == 1, "MAT.texture.mipmaps.count");
        check(mf.cel(0).mipmaps[0].width == 2 && mf.cel(0).mipmaps[0].height == 2, "MAT.texture.mip0.size");
        check(mf.cel(0).mipmaps[0].format == sed::PixelFormat::Rgb8, "MAT.texture.mip0.format");
        check(mf.cel(0).mipmaps[0].pixels.size() == 12, "MAT.texture.mip0.pixels.size");
        check(mf.cel(0).mipmaps[0].pixels[9] == 3, "MAT.texture.mip0.pixel3.r");
        check(mf.cel(0).mipmaps[0].pixels[10] == 3, "MAT.texture.mip0.pixel3.g");
        check(mf.cel(0).mipmaps[0].pixels[11] == 3, "MAT.texture.mip0.pixel3.b");

        const sed::Rgba8Image img = mf.decodeCelRgba8(0);
        check(img.width == 2 && img.height == 2, "MAT.texture.size");
        check(img.rgba.size() == 16, "MAT.texture.rgba.size");
        check(img.rgba.size() >= 4 && img.rgba[0] == 0 && img.rgba[1] == 0 && img.rgba[2] == 0 && img.rgba[3] == 255,
              "MAT.texture.pixel0");
    }

    {
        std::vector<std::byte> mat{};
        mat.push_back(std::byte{ 'M' });
        mat.push_back(std::byte{ 'A' });
        mat.push_back(std::byte{ 'T' });
        mat.push_back(std::byte{ ' ' });
        pushI32LE(mat, 0x32);
        pushI32LE(mat, 2);
        pushI32LE(mat, 1);
        pushI32LE(mat, 1);

        sed::ColorInfo ci{};
        ci.mode = sed::ColorMode::Indexed;
        ci.bpp = 8;
        pushColorInfo(mat, ci);

        pushI32LE(mat, 8);
        pushI32LE(mat, 256);
        for (int i = 0; i < 8; ++i)
        {
            pushI32LE(mat, 0);
        }

        checkThrowsWithMessage(
            [&mat, &pal]()
            {
                (void)sed::assets::MatFile::parse(mat, &pal);
            },
            "MAT solid color index out of range",
            "MAT.texture rejects an out-of-range solidColor palette index before publishing a cel");
    }

    {
        std::vector<std::byte> mat{};
        mat.push_back(std::byte{ 'M' });
        mat.push_back(std::byte{ 'A' });
        mat.push_back(std::byte{ 'T' });
        mat.push_back(std::byte{ ' ' });
        pushI32LE(mat, 0x32);
        pushI32LE(mat, 2);
        pushI32LE(mat, 1);
        pushI32LE(mat, 1);

        sed::ColorInfo ci{};
        ci.mode = sed::ColorMode::Indexed;
        ci.bpp = 8;
        // Shipped JKDF2/MotS indexed MATs use only mode/bpp reliably. These
        // deliberately nonsensical unused channel fields model the observed
        // legacy bytes and must not control indexed transparency semantics.
        ci.redBPP = 0x7f7f7f7fu;
        ci.greenShl = 0xfefefefeu;
        ci.alphaBPP = 0;
        ci.alphaShl = 0xababababu;
        pushColorInfo(mat, ci);

        mat.resize(mat.size() + 40);
        pushI32LE(mat, 2);
        pushI32LE(mat, 1);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 1);
        mat.push_back(std::byte{ 0 });
        mat.push_back(std::byte{ 7 });

        const sed::assets::MatFile mf =
            sed::assets::MatFile::parse(std::move(mat), &pal);
        const sed::Rgba8Image image = mf.decodeCelRgba8(0);
        check(image.rgba.size() == 8
                && image.rgba[3] == 255
                && image.rgba[7] == 255,
            "MAT.indexed zero alpha marker keeps palette-index-zero opaque despite unrelated ColorInfo garbage");
    }

    {
        std::vector<std::byte> mat{};
        mat.push_back(std::byte{ 'M' });
        mat.push_back(std::byte{ 'A' });
        mat.push_back(std::byte{ 'T' });
        mat.push_back(std::byte{ ' ' });
        pushI32LE(mat, 0x32);
        pushI32LE(mat, 2);
        pushI32LE(mat, 1);
        pushI32LE(mat, 1);

        sed::ColorInfo ci{};
        ci.mode = sed::ColorMode::Indexed;
        ci.bpp = 8;
        ci.redBPP = 0xbabababbu;
        ci.alphaBPP = 0xbabababbu;
        ci.alphaShl = 0x804080ebu;
        ci.alphaShr = 0xffffffffu;
        pushColorInfo(mat, ci);

        mat.resize(mat.size() + 40);
        pushI32LE(mat, 2);
        pushI32LE(mat, 1);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 1);
        mat.push_back(std::byte{ 0 });
        mat.push_back(std::byte{ 7 });

        const sed::assets::MatFile mf =
            sed::assets::MatFile::parse(std::move(mat), &pal);
        const sed::Rgba8Image image = mf.decodeCelRgba8(0);
        check(image.rgba.size() == 8
                && image.rgba[3] == 0
                && image.rgba[7] == 255,
            "MAT.indexed nonzero legacy alpha marker treats only palette index zero as transparent");
    }

    {
        std::vector<std::byte> mat{};
        mat.push_back(std::byte{ 'M' });
        mat.push_back(std::byte{ 'A' });
        mat.push_back(std::byte{ 'T' });
        mat.push_back(std::byte{ ' ' });
        pushI32LE(mat, 0x32);
        pushI32LE(mat, 0); // color MAT
        pushI32LE(mat, 1);
        pushI32LE(mat, 0);

        sed::ColorInfo ci{};
        ci.mode = sed::ColorMode::Indexed;
        ci.bpp  = 8;
        pushColorInfo(mat, ci);

        pushI32LE(mat, 0); // textype
        pushI32LE(mat, 5); // colornum
        for (int i = 0; i < 4; ++i)
        {
            pushI32LE(mat, 0);
        }

        const sed::assets::MatFile mf = sed::assets::MatFile::parse(std::move(mat), &pal);
        check(mf.isColor(), "MAT.color.isColor");
        check(mf.celCount() == 1, "MAT.color.celCount");
        check(mf.cel(0).solidColorIndex == 5,
            "MAT.color retains its solid color palette index");
        check(mf.cel(0).solidColor.r == 5
                && mf.cel(0).solidColor.g == 5
                && mf.cel(0).solidColor.b == 5,
            "MAT.color resolves its solid color through its CMP palette");

        check(mf.cel(0).mipmaps.size() == 1, "MAT.color.mipmaps.count");
        check(mf.cel(0).mipmaps[0].width == 64 && mf.cel(0).mipmaps[0].height == 64, "MAT.color.mip0.size");
        check(mf.cel(0).mipmaps[0].format == sed::PixelFormat::Rgb8, "MAT.color.mip0.format");
        check(mf.cel(0).mipmaps[0].pixels.size() == 64 * 64 * 3, "MAT.color.mip0.pixels.size");
        check(mf.cel(0).mipmaps[0].pixels[0] == 5 && mf.cel(0).mipmaps[0].pixels[1] == 5 && mf.cel(0).mipmaps[0].pixels[2] == 5,
              "MAT.color.mip0.pixel0.rgb");
        check(mf.cel(0).mipmaps[0].pixels.back() == 5, "MAT.color.mip0.pixels[last]");

        const sed::Rgba8Image img = mf.decodeCelRgba8(0);
        check(img.width == 64 && img.height == 64, "MAT.color.size");
        check(img.rgba.size() == 64 * 64 * 4, "MAT.color.rgba.size");
        check(img.rgba.size() >= 4 && img.rgba[0] == 5 && img.rgba[1] == 5 && img.rgba[2] == 5 && img.rgba[3] == 255,
              "MAT.color.pixel0");
    }

    {
        std::vector<std::byte> mat{};
        mat.push_back(std::byte{ 'M' });
        mat.push_back(std::byte{ 'A' });
        mat.push_back(std::byte{ 'T' });
        mat.push_back(std::byte{ ' ' });
        pushI32LE(mat, 0x32);
        pushI32LE(mat, 2); // texture MAT
        pushI32LE(mat, 1);
        pushI32LE(mat, 1);

        sed::ColorInfo ci{};
        ci.mode     = sed::ColorMode::RGB;
        ci.bpp      = 16;
        ci.redBPP   = 5;
        ci.greenBPP = 6;
        ci.blueBPP  = 5;
        ci.redShl   = 11;
        ci.greenShl = 5;
        ci.blueShl  = 0;
        ci.redShr   = 3;
        ci.greenShr = 2;
        ci.blueShr  = 3;
        pushColorInfo(mat, ci);

        mat.resize(mat.size() + 40); // texture headers

        pushI32LE(mat, 2);
        pushI32LE(mat, 2);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 1);

        const std::uint16_t pixels[]{
            0xF800, // red
            0x07E0, // green
            0x001F, // blue
            0xFFFF, // white
        };

        for (const std::uint16_t p : pixels)
        {
            mat.push_back(static_cast<std::byte>(p & 0xFFu));
            mat.push_back(static_cast<std::byte>((p >> 8) & 0xFFu));
        }

        const sed::assets::MatFile mf = sed::assets::MatFile::parse(std::move(mat));
        check(mf.cel(0).mipmaps[0].format == sed::PixelFormat::Rgb8, "MAT.16bpp.mip0.format");
        check(mf.cel(0).mipmaps[0].pixels.size() == 12, "MAT.16bpp.mip0.pixels.size");

        const sed::Rgba8Image img = mf.decodeCelRgba8(0);
        check(img.width == 2 && img.height == 2, "MAT.16bpp.size");
        check(img.rgba.size() >= 4 && img.rgba[0] == 255 && img.rgba[1] == 0 && img.rgba[2] == 0 && img.rgba[3] == 255, "MAT.16bpp.pixel0");
    }

    {
        std::vector<std::byte> mat{};
        mat.push_back(std::byte{ 'M' });
        mat.push_back(std::byte{ 'A' });
        mat.push_back(std::byte{ 'T' });
        mat.push_back(std::byte{ ' ' });
        pushI32LE(mat, 0x32);
        pushI32LE(mat, 2); // texture MAT
        pushI32LE(mat, 1);
        pushI32LE(mat, 1);

        sed::ColorInfo ci{};
        ci.mode     = sed::ColorMode::RGB;
        ci.bpp      = 24;
        ci.redBPP   = 8;
        ci.greenBPP = 8;
        ci.blueBPP  = 8;
        ci.redShl   = 16;
        ci.greenShl = 8;
        ci.blueShl  = 0;
        pushColorInfo(mat, ci);

        mat.resize(mat.size() + 40); // texture headers

        pushI32LE(mat, 2);
        pushI32LE(mat, 2);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 1);

        const std::uint8_t rgb[][3]{
            { 0x11, 0x22, 0x33 },
            { 0x44, 0x55, 0x66 },
            { 0x77, 0x88, 0x99 },
            { 0xAA, 0xBB, 0xCC },
        };

        for (const auto& p : rgb)
        {
            mat.push_back(static_cast<std::byte>(p[2])); // B
            mat.push_back(static_cast<std::byte>(p[1])); // G
            mat.push_back(static_cast<std::byte>(p[0])); // R
        }

        const sed::assets::MatFile mf = sed::assets::MatFile::parse(std::move(mat));
        check(mf.cel(0).mipmaps[0].format == sed::PixelFormat::Rgb8, "MAT.24bpp.mip0.format");
        check(mf.cel(0).mipmaps[0].pixels.size() == 12, "MAT.24bpp.mip0.pixels.size");

        const sed::Rgba8Image img = mf.decodeCelRgba8(0);
        check(img.width == 2 && img.height == 2, "MAT.24bpp.size");
        check(img.rgba.size() >= 4 && img.rgba[0] == 0x11 && img.rgba[1] == 0x22 && img.rgba[2] == 0x33 && img.rgba[3] == 255, "MAT.24bpp.pixel0");
    }

    {
        std::vector<std::byte> mat{};
        mat.push_back(std::byte{ 'M' });
        mat.push_back(std::byte{ 'A' });
        mat.push_back(std::byte{ 'T' });
        mat.push_back(std::byte{ ' ' });
        pushI32LE(mat, 0x32);
        pushI32LE(mat, 2); // texture MAT
        pushI32LE(mat, 1);
        pushI32LE(mat, 1);

        sed::ColorInfo ci{};
        ci.mode     = sed::ColorMode::RGBA;
        ci.bpp      = 32;
        ci.redBPP   = 8;
        ci.greenBPP = 8;
        ci.blueBPP  = 8;
        ci.redShl   = 16;
        ci.greenShl = 8;
        ci.blueShl  = 0;
        ci.alphaBPP = 8;
        ci.alphaShl = 24;
        pushColorInfo(mat, ci);

        mat.resize(mat.size() + 40); // texture headers

        pushI32LE(mat, 2);
        pushI32LE(mat, 2);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 1);

        const std::uint8_t rgba[][4]{
            { 1, 2, 3, 4 },
            { 5, 6, 7, 8 },
            { 9, 10, 11, 12 },
            { 13, 14, 15, 16 },
        };

        for (const auto& p : rgba)
        {
            mat.push_back(static_cast<std::byte>(p[2])); // B
            mat.push_back(static_cast<std::byte>(p[1])); // G
            mat.push_back(static_cast<std::byte>(p[0])); // R
            mat.push_back(static_cast<std::byte>(p[3])); // A
        }

        const sed::assets::MatFile mf = sed::assets::MatFile::parse(std::move(mat));
        check(mf.cel(0).mipmaps[0].format == sed::PixelFormat::Rgba8, "MAT.32bpp.mip0.format");
        check(mf.cel(0).mipmaps[0].pixels.size() == 16, "MAT.32bpp.mip0.pixels.size");

        const sed::Rgba8Image img = mf.decodeCelRgba8(0);
        check(img.width == 2 && img.height == 2, "MAT.32bpp.size");
        check(img.rgba.size() >= 4 && img.rgba[0] == 1 && img.rgba[1] == 2 && img.rgba[2] == 3 && img.rgba[3] == 4, "MAT.32bpp.pixel0");
    }

    {
        std::vector<std::byte> mat{};
        mat.push_back(std::byte{ 'M' });
        mat.push_back(std::byte{ 'A' });
        mat.push_back(std::byte{ 'T' });
        mat.push_back(std::byte{ ' ' });
        pushI32LE(mat, 0x32);
        pushI32LE(mat, 2); // texture MAT
        pushI32LE(mat, 1);
        pushI32LE(mat, 1);

        sed::ColorInfo ci{};
        ci.mode = sed::ColorMode::Indexed;
        ci.bpp  = 8;
        pushColorInfo(mat, ci);

        mat.resize(mat.size() + 40); // texture headers

        pushI32LE(mat, 4);
        pushI32LE(mat, 4);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 3);

        for (std::uint8_t i = 0; i < 16; ++i)
        {
            mat.push_back(static_cast<std::byte>(i));
        }
        mat.push_back(static_cast<std::byte>(20));
        mat.push_back(static_cast<std::byte>(21));
        mat.push_back(static_cast<std::byte>(22));
        mat.push_back(static_cast<std::byte>(23));
        mat.push_back(static_cast<std::byte>(42));

        const sed::assets::MatFile mf = sed::assets::MatFile::parse(std::move(mat), &pal);
        check(mf.cel(0).mipmaps.size() == 3, "MAT.mipmaps.count");
        check(mf.cel(0).mipmaps[0].width == 4 && mf.cel(0).mipmaps[0].height == 4, "MAT.mipmaps.m0.size");
        check(mf.cel(0).mipmaps[1].width == 2 && mf.cel(0).mipmaps[1].height == 2, "MAT.mipmaps.m1.size");
        check(mf.cel(0).mipmaps[2].width == 1 && mf.cel(0).mipmaps[2].height == 1, "MAT.mipmaps.m2.size");
        check(mf.cel(0).mipmaps[2].format == sed::PixelFormat::Rgb8, "MAT.mipmaps.m2.format");
        check(mf.cel(0).mipmaps[2].pixels.size() == 3, "MAT.mipmaps.m2.pixels.size");
        check(mf.cel(0).mipmaps[2].pixels[0] == 42 && mf.cel(0).mipmaps[2].pixels[1] == 42 && mf.cel(0).mipmaps[2].pixels[2] == 42,
              "MAT.mipmaps.m2.pixel0.rgb");
    }

    {
        std::vector<std::byte> mat{};
        mat.push_back(std::byte{ 'M' });
        mat.push_back(std::byte{ 'A' });
        mat.push_back(std::byte{ 'T' });
        mat.push_back(std::byte{ ' ' });
        pushI32LE(mat, 0x32);
        pushI32LE(mat, 2); // texture MAT
        pushI32LE(mat, 2);
        pushI32LE(mat, 2);

        sed::ColorInfo ci{};
        ci.mode = sed::ColorMode::Indexed;
        ci.bpp  = 8;
        pushColorInfo(mat, ci);

        pushI32LE(mat, 8);
        pushI32LE(mat, 11);
        for (int i = 0; i < 8; ++i)
        {
            pushI32LE(mat, 0);
        }
        pushI32LE(mat, 8);
        pushI32LE(mat, 29);
        for (int i = 0; i < 8; ++i)
        {
            pushI32LE(mat, 0);
        }

        pushI32LE(mat, 2);
        pushI32LE(mat, 2);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 1);
        mat.push_back(static_cast<std::byte>(1));
        mat.push_back(static_cast<std::byte>(2));
        mat.push_back(static_cast<std::byte>(3));
        mat.push_back(static_cast<std::byte>(4));

        pushI32LE(mat, 2);
        pushI32LE(mat, 2);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 0);
        pushI32LE(mat, 1);
        mat.push_back(static_cast<std::byte>(5));
        mat.push_back(static_cast<std::byte>(6));
        mat.push_back(static_cast<std::byte>(7));
        mat.push_back(static_cast<std::byte>(8));

        const sed::assets::MatFile mf = sed::assets::MatFile::parse(std::move(mat), &pal);
        check(mf.celCount() == 2, "MAT.cels.count");
        check(mf.cel(0).solidColorIndex == 11
                && mf.cel(1).solidColorIndex == 29,
            "MAT.cels retain their independent solidColor palette indices");
        check(mf.cel(0).mipmaps[0].pixels[0] == 1, "MAT.cels.0.pixel0.r");
        check(mf.cel(1).mipmaps[0].pixels[0] == 5, "MAT.cels.1.pixel0.r");
    }
}

} // namespace sed::tests
