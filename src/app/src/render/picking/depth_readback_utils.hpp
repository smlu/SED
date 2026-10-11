#pragma once

#include <QPoint>
#include <QPointF>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace sed::render::picking
{

/**
 * Convert one logical widget position to a framebuffer pixel using the same
 * rounded DPR scaling and bottom-left Y origin used by glReadPixels.
 */
[[nodiscard]] inline std::optional<QPoint> framebufferPixelForWidgetPoint(
    const QPointF& widgetPoint,
    const int widgetWidth,
    const int widgetHeight,
    const double devicePixelRatio,
    const int framebufferWidth,
    const int framebufferHeight) noexcept
{
    if (widgetWidth <= 0 || widgetHeight <= 0
        || framebufferWidth <= 0 || framebufferHeight <= 0
        || !std::isfinite(devicePixelRatio) || devicePixelRatio <= 0.0
        || !std::isfinite(widgetPoint.x()) || !std::isfinite(widgetPoint.y())
        || widgetPoint.x() < 0.0 || widgetPoint.y() < 0.0
        || widgetPoint.x() >= static_cast<double>(widgetWidth)
        || widgetPoint.y() >= static_cast<double>(widgetHeight))
    {
        return std::nullopt;
    }

    const double scaledX = widgetPoint.x() * devicePixelRatio;
    const double scaledY = widgetPoint.y() * devicePixelRatio;
    if (!std::isfinite(scaledX) || !std::isfinite(scaledY)
        || scaledX < static_cast<double>(std::numeric_limits<long>::min())
        || scaledX > static_cast<double>(std::numeric_limits<long>::max())
        || scaledY < static_cast<double>(std::numeric_limits<long>::min())
        || scaledY > static_cast<double>(std::numeric_limits<long>::max()))
    {
        return std::nullopt;
    }

    const long pixelX = std::clamp(
        std::lround(scaledX),
        0L,
        static_cast<long>(framebufferWidth - 1));
    const long pixelY = std::clamp(
        std::lround(scaledY),
        0L,
        static_cast<long>(framebufferHeight - 1));
    return QPoint(
        static_cast<int>(pixelX),
        (framebufferHeight - 1) - static_cast<int>(pixelY));
}

struct DepthReadbackRegion
{
    int left{};
    int bottom{};
    int width{};
    int height{};
    std::uint64_t pixelCount{};
    std::size_t validSampleCount{};
};

/**
 * Return a safe row-major offset for a queried pixel inside a planned region.
 */
[[nodiscard]] inline std::optional<std::size_t> depthReadbackSampleIndex(
    const DepthReadbackRegion& region,
    const QPoint& framebufferPixel) noexcept
{
    if (region.width <= 0 || region.height <= 0
        || framebufferPixel.x() < region.left
        || framebufferPixel.y() < region.bottom)
    {
        return std::nullopt;
    }

    const std::int64_t localX = static_cast<std::int64_t>(framebufferPixel.x())
        - static_cast<std::int64_t>(region.left);
    const std::int64_t localY = static_cast<std::int64_t>(framebufferPixel.y())
        - static_cast<std::int64_t>(region.bottom);
    if (localX >= region.width || localY >= region.height)
    {
        return std::nullopt;
    }

    return static_cast<std::size_t>(localY)
        * static_cast<std::size_t>(region.width)
        + static_cast<std::size_t>(localX);
}

/**
 * Plan one depth-buffer rectangle read only when the samples are dense enough
 * to bound excess pixels and the allocation remains <= 1,048,576 floats
 * (4 MiB). Otherwise callers should keep the exact sparse 1x1 read path.
 * Coordinates use OpenGL's bottom-left origin.
 */
[[nodiscard]] inline std::optional<DepthReadbackRegion> denseDepthReadbackRegion(
    const std::span<const QPoint> samplePixels,
    const int framebufferWidth,
    const int framebufferHeight,
    const std::size_t minimumSampleCount = 512u,
    const std::uint64_t maximumPixelCount = 1u * 1024u * 1024u,
    const std::uint64_t maximumPixelsPerSample = 64u) noexcept
{
    if (framebufferWidth <= 0 || framebufferHeight <= 0
        || minimumSampleCount == 0u || maximumPixelCount == 0u
        || maximumPixelsPerSample == 0u)
    {
        return std::nullopt;
    }

    int minimumX = framebufferWidth;
    int minimumY = framebufferHeight;
    int maximumX = -1;
    int maximumY = -1;
    std::size_t validSampleCount = 0u;
    for (const QPoint& sample : samplePixels)
    {
        if (sample.x() < 0 || sample.y() < 0
            || sample.x() >= framebufferWidth
            || sample.y() >= framebufferHeight)
        {
            continue;
        }
        minimumX = std::min(minimumX, sample.x());
        minimumY = std::min(minimumY, sample.y());
        maximumX = std::max(maximumX, sample.x());
        maximumY = std::max(maximumY, sample.y());
        ++validSampleCount;
    }

    if (validSampleCount < minimumSampleCount
        || maximumX < minimumX || maximumY < minimumY)
    {
        return std::nullopt;
    }

    const std::uint64_t regionWidth =
        static_cast<std::uint64_t>(maximumX - minimumX) + 1u;
    const std::uint64_t regionHeight =
        static_cast<std::uint64_t>(maximumY - minimumY) + 1u;
    if (regionWidth > maximumPixelCount / regionHeight)
    {
        return std::nullopt;
    }
    const std::uint64_t pixelCount = regionWidth * regionHeight;
    if (pixelCount > maximumPixelCount)
    {
        return std::nullopt;
    }

    const std::uint64_t minimumDensitySampleCount =
        (pixelCount / maximumPixelsPerSample)
        + (pixelCount % maximumPixelsPerSample == 0u ? 0u : 1u);
    if (validSampleCount < minimumDensitySampleCount)
    {
        return std::nullopt;
    }

    return DepthReadbackRegion{
        minimumX,
        minimumY,
        static_cast<int>(regionWidth),
        static_cast<int>(regionHeight),
        pixelCount,
        validSampleCount };
}

} // namespace sed::render::picking
