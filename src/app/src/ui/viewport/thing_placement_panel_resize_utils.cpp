#include "thing_placement_panel_resize_utils.hpp"

#include <algorithm>
#include <cmath>

#include <QWidget>

namespace sed::ui::viewport
{
namespace
{
constexpr int kPanelResizeGripPx = 8;
constexpr int kSidePanelMinPx = 340;
constexpr int kBandPanelMinPx = 330;
constexpr int kViewportInsetPx = 48;
constexpr int kFilterMinHeightPx = 38;
constexpr int kFilterMaxHeightPx = 260;
constexpr int kDockDropCatchMarginPx = 120;
constexpr int kDockPreviewInsetPx = 6;
constexpr int kDockPreviewMinThicknessPx = 58;
constexpr int kDockPreviewMaxThicknessPx = 180;
constexpr int kUvDockPreviewMinThicknessPx = 52;
constexpr int kUvDockPreviewMaxThicknessPx = 140;
constexpr int kDockDragFallbackThresholdPx = 42;
} // namespace

bool isSideDock(const PanelDockSide dockSide) noexcept
{
    return dockSide == PanelDockSide::left || dockSide == PanelDockSide::right;
}

ThingPlacementPanelEventRoute thingPlacementPanelEventRoute(
    const ThingPlacementPanelEventContext& context) noexcept
{
    // Step 1: no panel or no event means the global filter should ignore this branch.
    if (!context.panelAvailable || !context.eventAvailable)
    {
        return ThingPlacementPanelEventRoute::none;
    }

    // Step 2: active filter-grip drags own move/release until the left button is released.
    if (context.filterResizeActive)
    {
        if (context.mouseMove)
        {
            return ThingPlacementPanelEventRoute::applyFilterResize;
        }
        if (context.mouseRelease && context.leftButton)
        {
            return ThingPlacementPanelEventRoute::finishFilterResize;
        }
        return ThingPlacementPanelEventRoute::none;
    }

    // Step 3: active outer-panel drags resize the currently docked panel dimension.
    if (context.panelResizeActive)
    {
        if (context.mouseMove)
        {
            return ThingPlacementPanelEventRoute::applyPanelResize;
        }
        if (context.mouseRelease && context.leftButton)
        {
            return ThingPlacementPanelEventRoute::finishPanelResize;
        }
        return ThingPlacementPanelEventRoute::none;
    }

    // Step 4: idle left-presses can begin either the inner filter resize or outer panel resize.
    if (context.insidePanel && context.mousePress && context.leftButton)
    {
        if (context.watchedFilterResizeGrip)
        {
            return ThingPlacementPanelEventRoute::beginFilterResize;
        }
        if (context.resizeEdgeHit)
        {
            return ThingPlacementPanelEventRoute::beginPanelResize;
        }
        return ThingPlacementPanelEventRoute::none;
    }

    // Step 5: idle hover only changes the cursor when no button is held.
    if (context.insidePanel && context.mouseMove && context.noButtons)
    {
        if (context.resizeEdgeHit)
        {
            return ThingPlacementPanelEventRoute::showResizeCursor;
        }
        if (!context.headerDragWidget)
        {
            return ThingPlacementPanelEventRoute::clearCursor;
        }
    }

    // Step 6: leaving a non-header-drag child clears any hover cursor.
    if (context.insidePanel && context.leave && !context.headerDragWidget)
    {
        return ThingPlacementPanelEventRoute::clearCursor;
    }

    return ThingPlacementPanelEventRoute::none;
}

PanelHeaderDragEventRoute panelHeaderDragEventRoute(
    const PanelHeaderDragEventContext& context) noexcept
{
    // Step 1: only active drags with real events are handled by this route.
    if (!context.active || !context.eventAvailable)
    {
        return PanelHeaderDragEventRoute::none;
    }

    // Step 2: moving without the left button means Qt lost the drag button state.
    if (context.mouseMove)
    {
        return context.leftButtonHeld
            ? PanelHeaderDragEventRoute::updatePreview
            : PanelHeaderDragEventRoute::cancelDrag;
    }

    // Step 3: releasing the initiating button commits the target or fallback side.
    if (context.mouseRelease && context.leftButton)
    {
        return PanelHeaderDragEventRoute::finishDrag;
    }

    return PanelHeaderDragEventRoute::none;
}

bool panelHeaderDragExceedsThreshold(
    const QPoint& delta,
    const int thresholdPx) noexcept
{
    return delta.manhattanLength() >= std::max(1, thresholdPx);
}

bool widgetInsidePanel(const QWidget* watchedWidget, const QWidget* panel) noexcept
{
    return watchedWidget
        && panel
        && (watchedWidget == panel || panel->isAncestorOf(watchedWidget));
}

bool visibleWidgetContainsGlobalPoint(const QWidget* widget, const QPoint& globalPoint) noexcept
{
    if (!widget || !widget->isVisible())
    {
        return false;
    }

    const QRect globalBounds(widget->mapToGlobal(QPoint(0, 0)), widget->size());
    return globalBounds.contains(globalPoint);
}

Qt::CursorShape panelResizeCursor(const PanelDockSide dockSide) noexcept
{
    return isSideDock(dockSide) ? Qt::SizeHorCursor : Qt::SizeVerCursor;
}

std::optional<PanelDockSide> panelDockSideForLocalPoint(
    const QRect& localBounds,
    const QPoint& localPos,
    const int catchMarginPx) noexcept
{
    // Step 1: accept drags slightly outside the viewport so edge docking feels forgiving.
    const QRect catchRect = localBounds.adjusted(
        -catchMarginPx,
        -catchMarginPx,
        catchMarginPx,
        catchMarginPx);
    if (!catchRect.contains(localPos))
    {
        return std::nullopt;
    }

    // Step 2: choose the nearest viewport edge, preserving the legacy tie order.
    const int leftDist = std::abs(localPos.x() - localBounds.left());
    const int rightDist = std::abs(localPos.x() - localBounds.right());
    const int topDist = std::abs(localPos.y() - localBounds.top());
    const int bottomDist = std::abs(localPos.y() - localBounds.bottom());
    const int minDist = std::min({ leftDist, rightDist, topDist, bottomDist });
    if (minDist == leftDist)
    {
        return PanelDockSide::left;
    }
    if (minDist == rightDist)
    {
        return PanelDockSide::right;
    }
    if (minDist == topDist)
    {
        return PanelDockSide::top;
    }
    return PanelDockSide::bottom;
}

QRect panelDockPreviewRect(
    const QRect& localBounds,
    const PanelDockSide dockSide,
    const PanelDockPreviewMetrics metrics) noexcept
{
    const int thickness = std::clamp(
        std::min(localBounds.width(), localBounds.height()) / 4,
        metrics.minThicknessPx,
        metrics.maxThicknessPx);

    // Step 1: shrink the preview to a small inset so it reads as a target band.
    switch (dockSide)
    {
        case PanelDockSide::left:
            return localBounds.adjusted(
                metrics.insetPx,
                metrics.insetPx,
                -(std::max(metrics.insetPx, localBounds.width() - thickness)),
                -metrics.insetPx);
        case PanelDockSide::right:
            return localBounds.adjusted(
                std::max(metrics.insetPx, localBounds.width() - thickness),
                metrics.insetPx,
                -metrics.insetPx,
                -metrics.insetPx);
        case PanelDockSide::top:
            return localBounds.adjusted(
                metrics.insetPx,
                metrics.insetPx,
                -metrics.insetPx,
                -(std::max(metrics.insetPx, localBounds.height() - thickness)));
        case PanelDockSide::bottom:
        default:
            return localBounds.adjusted(
                metrics.insetPx,
                std::max(metrics.insetPx, localBounds.height() - thickness),
                -metrics.insetPx,
                -metrics.insetPx);
    }
}

std::optional<PanelDockSide> panelDockSideForDragDelta(
    const QPoint& delta,
    const int thresholdPx) noexcept
{
    // Step 1: tiny releases are clicks, not dock-direction drags.
    if (!panelHeaderDragExceedsThreshold(delta, thresholdPx))
    {
        return std::nullopt;
    }

    // Step 2: use the dominant axis to resolve the user's intended dock edge.
    if (std::abs(delta.x()) > std::abs(delta.y()))
    {
        return delta.x() < 0 ? PanelDockSide::left : PanelDockSide::right;
    }
    return delta.y() < 0 ? PanelDockSide::top : PanelDockSide::bottom;
}

std::optional<PanelDockSide> panelDockSideForLocalPoint(
    const QRect& localBounds,
    const QPoint& localPos) noexcept
{
    return panelDockSideForLocalPoint(localBounds, localPos, kDockDropCatchMarginPx);
}

std::optional<PanelDockSide> panelDockSideForDragDelta(const QPoint& delta) noexcept
{
    return panelDockSideForDragDelta(delta, kDockDragFallbackThresholdPx);
}

std::optional<PanelDockSide> thingPlacementDockSideForLocalPoint(
    const QRect& localBounds,
    const QPoint& localPos) noexcept
{
    return panelDockSideForLocalPoint(localBounds, localPos);
}

QRect thingPlacementDockPreviewRect(
    const QRect& localBounds,
    const PanelDockSide dockSide) noexcept
{
    return panelDockPreviewRect(
        localBounds,
        dockSide,
        PanelDockPreviewMetrics{
            kDockPreviewInsetPx,
            kDockPreviewMinThicknessPx,
            kDockPreviewMaxThicknessPx,
        });
}

QRect uvEditorDockPreviewRect(
    const QRect& localBounds,
    const PanelDockSide dockSide) noexcept
{
    return panelDockPreviewRect(
        localBounds,
        dockSide,
        PanelDockPreviewMetrics{
            kDockPreviewInsetPx,
            kUvDockPreviewMinThicknessPx,
            kUvDockPreviewMaxThicknessPx,
        });
}

std::optional<PanelDockSide> thingPlacementDockSideForDragDelta(const QPoint& delta) noexcept
{
    return panelDockSideForDragDelta(delta);
}

bool thingPlacementPanelResizeEdgeHit(
    const QRect& globalPanelRect,
    const QPoint& globalPos,
    const PanelDockSide dockSide) noexcept
{
    switch (dockSide)
    {
        case PanelDockSide::left:
            return std::abs(globalPos.x() - globalPanelRect.right()) <= kPanelResizeGripPx
                && globalPanelRect.adjusted(0, 0, kPanelResizeGripPx, 0).contains(globalPos);
        case PanelDockSide::right:
            return std::abs(globalPos.x() - globalPanelRect.left()) <= kPanelResizeGripPx
                && globalPanelRect.adjusted(-kPanelResizeGripPx, 0, 0, 0).contains(globalPos);
        case PanelDockSide::top:
            return std::abs(globalPos.y() - globalPanelRect.bottom()) <= kPanelResizeGripPx
                && globalPanelRect.adjusted(0, 0, 0, kPanelResizeGripPx).contains(globalPos);
        case PanelDockSide::bottom:
        default:
            return std::abs(globalPos.y() - globalPanelRect.top()) <= kPanelResizeGripPx
                && globalPanelRect.adjusted(0, -kPanelResizeGripPx, 0, 0).contains(globalPos);
    }
}

int resizedThingPlacementPanelSize(
    const PanelDockSide dockSide,
    const int viewportExtent,
    const int startSize,
    const QPoint& startGlobal,
    const QPoint& currentGlobal) noexcept
{
    const bool sideDock = isSideDock(dockSide);
    const int minSize = std::min(sideDock ? kSidePanelMinPx : kBandPanelMinPx, std::max(1, viewportExtent));
    const int maxSize = std::max(minSize, viewportExtent - kViewportInsetPx);

    // Step 1: compute drag delta on the resize axis.
    const int delta = sideDock
        ? currentGlobal.x() - startGlobal.x()
        : currentGlobal.y() - startGlobal.y();

    // Step 2: edges on the far side grow with positive delta; near-side edges grow in reverse.
    const bool growsWithPositiveDelta =
        dockSide == PanelDockSide::left || dockSide == PanelDockSide::top;
    const int candidate = growsWithPositiveDelta
        ? startSize + delta
        : startSize - delta;

    // Step 3: preserve the legacy viewport-relative min/max clamps.
    return std::clamp(candidate, minSize, maxSize);
}

int resizedThingPlacementFilterHeight(
    const int startHeight,
    const QPoint& startGlobal,
    const QPoint& currentGlobal) noexcept
{
    const int delta = currentGlobal.y() - startGlobal.y();
    return std::clamp(
        startHeight + delta,
        kFilterMinHeightPx,
        kFilterMaxHeightPx);
}

} // namespace sed::ui::viewport
