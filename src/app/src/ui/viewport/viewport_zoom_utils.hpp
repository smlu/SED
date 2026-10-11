#pragma once

#include <QSize>

namespace sed::ui::viewport
{

/** Returns the limiting viewport-size ratio used to keep the same scene area visible after a panel resize. */
float viewportZoomResizeRatio(const QSize& baselineSize, const QSize& targetSize) noexcept;

/** Returns the ratio for the splitter axis being resized: height for horizontal panels, width for side panels. */
float viewportPanelResizeRatio(const QSize& baselineSize, const QSize& targetSize, bool vertical) noexcept;

/** Returns the orthographic scale that preserves the baseline world area after the viewport size changes. */
float compensatedOrthoScaleForViewportResize(float baselineScale, const QSize& baselineSize, const QSize& targetSize) noexcept;

/** Returns the perspective orbit distance that preserves the baseline world area after the viewport size changes. */
float compensatedPerspectiveDistanceForViewportResize(float baselineDistance, const QSize& baselineSize, const QSize& targetSize) noexcept;

/**
 * Returns the orthographic scale that preserves projected world-unit size while
 * a docked panel changes the viewport aspect ratio.
 *
 * ViewportCamera's projected pixels per world unit are proportional to
 * orthoScale * width / height. Preserving that quantity prevents a panel from
 * introducing a second, artificial zoom on top of the area it occupies.
 */
float compensatedOrthoScaleForPanelResize(
    float baselineScale,
    const QSize& baselineSize,
    const QSize& targetSize) noexcept;

/**
 * Returns the perspective orbit distance that preserves projected world-unit
 * size while a docked panel changes the viewport width. The camera has a fixed
 * horizontal FOV, so screen scale is proportional to width / distance.
 */
float compensatedPerspectiveDistanceForPanelResize(
    float baselineDistance,
    const QSize& baselineSize,
    const QSize& targetSize) noexcept;

/** Returns the viewport size implied by a two-widget panel splitter handle move. */
QSize viewportSizeAfterPanelSplitterMove(
    const QSize& currentViewportSize,
    const QSize& splitterSize,
    int handlePosition,
    int handleWidth,
    bool vertical,
    bool panelFirst) noexcept;

} // namespace sed::ui::viewport
