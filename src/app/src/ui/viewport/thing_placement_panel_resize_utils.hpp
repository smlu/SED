#pragma once

#include <cstdint>
#include <optional>

#include <QPoint>
#include <QRect>
#include <Qt>

class QWidget;

namespace sed::ui::viewport
{

enum class PanelDockSide : std::uint8_t
{
    left,
    right,
    top,
    bottom,
};

struct PanelDockPreviewMetrics
{
    int insetPx;
    int minThicknessPx;
    int maxThicknessPx;
};

enum class ThingPlacementPanelEventRoute
{
    none,
    applyFilterResize,
    finishFilterResize,
    applyPanelResize,
    finishPanelResize,
    beginFilterResize,
    beginPanelResize,
    showResizeCursor,
    clearCursor,
};

struct ThingPlacementPanelEventContext
{
    bool panelAvailable{};
    bool eventAvailable{};
    bool filterResizeActive{};
    bool panelResizeActive{};
    bool insidePanel{};
    bool mousePress{};
    bool mouseMove{};
    bool mouseRelease{};
    bool leave{};
    bool leftButton{};
    bool noButtons{};
    bool resizeEdgeHit{};
    bool watchedFilterResizeGrip{};
    bool headerDragWidget{};
};

enum class PanelHeaderDragEventRoute
{
    none,
    cancelDrag,
    updatePreview,
    finishDrag,
};

struct PanelHeaderDragEventContext
{
    bool active{};
    bool eventAvailable{};
    bool mouseMove{};
    bool mouseRelease{};
    bool leftButtonHeld{};
    bool leftButton{};
};

/**
 * Returns true for side-docked panels, where resizing is horizontal.
 */
bool isSideDock(PanelDockSide dockSide) noexcept;

/**
 * Resolves how the thing-placement panel should react to a mouse/leave event.
 */
ThingPlacementPanelEventRoute thingPlacementPanelEventRoute(
    const ThingPlacementPanelEventContext& context) noexcept;

/**
 * Resolves how an already-active floating panel header drag should react.
 */
PanelHeaderDragEventRoute panelHeaderDragEventRoute(
    const PanelHeaderDragEventContext& context) noexcept;

/**
 * Returns true after pointer movement exceeds the platform drag threshold.
 */
bool panelHeaderDragExceedsThreshold(
    const QPoint& delta,
    int thresholdPx) noexcept;

/**
 * Returns true when watchedWidget is the panel itself or one of its descendants.
 */
bool widgetInsidePanel(const QWidget* watchedWidget, const QWidget* panel) noexcept;

/**
 * Returns true when a visible widget's global bounds contain the given point.
 */
bool visibleWidgetContainsGlobalPoint(const QWidget* widget, const QPoint& globalPoint) noexcept;

/**
 * Returns the cursor shape used for resizing a panel docked on this side.
 */
Qt::CursorShape panelResizeCursor(PanelDockSide dockSide) noexcept;

/**
 * Resolves which viewport edge a panel-header drag is hovering near.
 */
std::optional<PanelDockSide> panelDockSideForLocalPoint(
    const QRect& localBounds,
    const QPoint& localPos,
    int catchMarginPx) noexcept;

/**
 * Resolves the hovered dock edge using the standard panel-header drag margin.
 */
std::optional<PanelDockSide> panelDockSideForLocalPoint(
    const QRect& localBounds,
    const QPoint& localPos) noexcept;

/**
 * Builds a rubber-band preview rectangle for a dock target inside viewport bounds.
 */
QRect panelDockPreviewRect(
    const QRect& localBounds,
    PanelDockSide dockSide,
    PanelDockPreviewMetrics metrics) noexcept;

/**
 * Resolves a fallback dock side from the final header-drag delta.
 */
std::optional<PanelDockSide> panelDockSideForDragDelta(
    const QPoint& delta,
    int thresholdPx) noexcept;

/**
 * Resolves a fallback dock side using the standard panel-header drag threshold.
 */
std::optional<PanelDockSide> panelDockSideForDragDelta(const QPoint& delta) noexcept;

/**
 * Resolves which viewport edge a panel-header drag is hovering near.
 */
std::optional<PanelDockSide> thingPlacementDockSideForLocalPoint(
    const QRect& localBounds,
    const QPoint& localPos) noexcept;

/**
 * Builds the rubber-band preview rectangle for a dock target inside viewport bounds.
 */
QRect thingPlacementDockPreviewRect(
    const QRect& localBounds,
    PanelDockSide dockSide) noexcept;

/**
 * Builds the UV editor rubber-band preview rectangle for a dock target.
 */
QRect uvEditorDockPreviewRect(
    const QRect& localBounds,
    PanelDockSide dockSide) noexcept;

/**
 * Resolves a fallback dock side from the final header-drag delta.
 */
std::optional<PanelDockSide> thingPlacementDockSideForDragDelta(const QPoint& delta) noexcept;

/**
 * Checks whether a global cursor point is on the resize edge for a docked panel.
 */
bool thingPlacementPanelResizeEdgeHit(
    const QRect& globalPanelRect,
    const QPoint& globalPos,
    PanelDockSide dockSide) noexcept;

/**
 * Computes the next panel size after dragging a docked panel resize edge.
 */
int resizedThingPlacementPanelSize(
    PanelDockSide dockSide,
    int viewportExtent,
    int startSize,
    const QPoint& startGlobal,
    const QPoint& currentGlobal) noexcept;

/**
 * Computes the next template-filter height after dragging its vertical grip.
 */
int resizedThingPlacementFilterHeight(
    int startHeight,
    const QPoint& startGlobal,
    const QPoint& currentGlobal) noexcept;

} // namespace sed::ui::viewport
