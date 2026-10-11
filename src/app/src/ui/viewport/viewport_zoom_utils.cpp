#include "ui/viewport/viewport_zoom_utils.hpp"

#include <algorithm>
#include <cmath>

namespace sed::ui::viewport
{

namespace
{

float safePositiveRatio(const float numerator, const float denominator) noexcept
{
    if (!std::isfinite(numerator) || !std::isfinite(denominator) || numerator <= 0.0f || denominator <= 0.0f)
    {
        return 1.0f;
    }

    const float ratio = numerator / denominator;
    return (std::isfinite(ratio) && ratio > 0.0f) ? ratio : 1.0f;
}

} // namespace

float viewportZoomResizeRatio(const QSize& baselineSize, const QSize& targetSize) noexcept
{
    if (baselineSize.width() <= 0 || baselineSize.height() <= 0 || targetSize.width() <= 0 || targetSize.height() <= 0)
    {
        return 1.0f;
    }

    const float widthRatio = safePositiveRatio(static_cast<float>(targetSize.width()), static_cast<float>(baselineSize.width()));
    const float heightRatio = safePositiveRatio(static_cast<float>(targetSize.height()), static_cast<float>(baselineSize.height()));
    const float ratio = std::min(widthRatio, heightRatio);
    return (std::isfinite(ratio) && ratio > 0.0f) ? ratio : 1.0f;
}

float viewportPanelResizeRatio(const QSize& baselineSize, const QSize& targetSize, const bool vertical) noexcept
{
    if (baselineSize.width() <= 0 || baselineSize.height() <= 0 || targetSize.width() <= 0 || targetSize.height() <= 0)
    {
        return 1.0f;
    }

    return vertical
        ? safePositiveRatio(static_cast<float>(targetSize.height()), static_cast<float>(baselineSize.height()))
        : safePositiveRatio(static_cast<float>(targetSize.width()), static_cast<float>(baselineSize.width()));
}

float compensatedOrthoScaleForViewportResize(const float baselineScale, const QSize& baselineSize, const QSize& targetSize) noexcept
{
    if (!std::isfinite(baselineScale) || baselineScale <= 0.0f)
    {
        return baselineScale;
    }

    const float ratio = viewportZoomResizeRatio(baselineSize, targetSize);
    return std::clamp(baselineScale * ratio, 0.01f, 1.0e6f);
}

float compensatedPerspectiveDistanceForViewportResize(const float baselineDistance, const QSize& baselineSize, const QSize& targetSize) noexcept
{
    if (!std::isfinite(baselineDistance) || baselineDistance <= 0.0f)
    {
        return baselineDistance;
    }

    const float ratio = viewportZoomResizeRatio(baselineSize, targetSize);
    return std::max(baselineDistance / std::max(ratio, 1.0e-6f), 0.01f);
}

float compensatedOrthoScaleForPanelResize(
    const float baselineScale,
    const QSize& baselineSize,
    const QSize& targetSize) noexcept
{
    if (!std::isfinite(baselineScale) || baselineScale <= 0.0f)
    {
        return baselineScale;
    }

    if (baselineSize.width() <= 0 || baselineSize.height() <= 0
        || targetSize.width() <= 0 || targetSize.height() <= 0)
    {
        return std::clamp(baselineScale, 0.01f, 1.0e6f);
    }

    const float baselineAspect = safePositiveRatio(
        static_cast<float>(baselineSize.width()),
        static_cast<float>(baselineSize.height()));
    const float targetAspect = safePositiveRatio(
        static_cast<float>(targetSize.width()),
        static_cast<float>(targetSize.height()));
    return std::clamp(
        baselineScale * safePositiveRatio(baselineAspect, targetAspect),
        0.01f,
        1.0e6f);
}

float compensatedPerspectiveDistanceForPanelResize(
    const float baselineDistance,
    const QSize& baselineSize,
    const QSize& targetSize) noexcept
{
    if (!std::isfinite(baselineDistance) || baselineDistance <= 0.0f)
    {
        return baselineDistance;
    }

    if (baselineSize.width() <= 0 || baselineSize.height() <= 0
        || targetSize.width() <= 0 || targetSize.height() <= 0)
    {
        return std::max(baselineDistance, 0.01f);
    }

    const float widthRatio = safePositiveRatio(
        static_cast<float>(targetSize.width()),
        static_cast<float>(baselineSize.width()));
    return std::max(baselineDistance * widthRatio, 0.01f);
}

QSize viewportSizeAfterPanelSplitterMove(
    const QSize& currentViewportSize,
    const QSize& splitterSize,
    const int handlePosition,
    const int handleWidth,
    const bool vertical,
    const bool panelFirst) noexcept
{
    const int totalExtent = vertical ? splitterSize.height() : splitterSize.width();
    if (totalExtent <= 0)
    {
        return currentViewportSize;
    }

    const int clampedHandlePosition = std::clamp(handlePosition, 0, totalExtent);
    const int safeHandleWidth = std::max(0, handleWidth);
    const int viewportExtent = panelFirst
        ? totalExtent - clampedHandlePosition - safeHandleWidth
        : clampedHandlePosition;

    QSize targetSize = currentViewportSize;
    if (vertical)
    {
        targetSize.setHeight(std::max(1, viewportExtent));
    }
    else
    {
        targetSize.setWidth(std::max(1, viewportExtent));
    }
    return targetSize;
}

} // namespace sed::ui::viewport
