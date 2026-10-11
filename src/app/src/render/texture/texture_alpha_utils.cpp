#include "render/texture/texture_alpha_utils.hpp"

#include <algorithm>

#include <QtGlobal>

namespace sed::render
{

std::int32_t inferTextureAlphaBpp(const QImage& img) noexcept
{
    if (!img.hasAlphaChannel())
    {
        return 0;
    }

    const int w = img.width();
    const int h = img.height();
    if (w <= 0 || h <= 0)
    {
        return 0;
    }

    constexpr int kSamples = 16;
    const int stepX = std::max(1, w / kSamples);
    const int stepY = std::max(1, h / kSamples);

    bool anyTransparent = false;
    bool anyMidAlpha = false;

    const QImage::Format fmt = img.format();
    if (fmt == QImage::Format_RGBA8888 || fmt == QImage::Format_RGBA8888_Premultiplied)
    {
        for (int y = 0; y < h; y += stepY)
        {
            const uchar* row = img.constScanLine(y);
            if (!row)
            {
                continue;
            }

            for (int x = 0; x < w; x += stepX)
            {
                const int a = row[x * 4 + 3];
                if (a < 255)
                {
                    anyTransparent = true;
                    if (a > 0)
                    {
                        anyMidAlpha = true;
                        break;
                    }
                }
            }
            if (anyMidAlpha)
            {
                break;
            }
        }
    }
    else
    {
        for (int y = 0; y < h; y += stepY)
        {
            for (int x = 0; x < w; x += stepX)
            {
                const int a = qAlpha(img.pixel(x, y));
                if (a < 255)
                {
                    anyTransparent = true;
                    if (a > 0)
                    {
                        anyMidAlpha = true;
                        break;
                    }
                }
            }
            if (anyMidAlpha)
            {
                break;
            }
        }
    }

    if (!anyTransparent)
    {
        return 0;
    }

    // Use a 1bpp hint for binary cutouts, otherwise treat as true translucency.
    return anyMidAlpha ? 8 : 1;
}

} // namespace sed::render
