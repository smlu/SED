#pragma once

#include <QPoint>
#include <Qt>

namespace sed::ui::viewport
{

enum class ThingPlacementWheelRoute
{
    none,
    consumePanelWheel,
    rotatePlacementYaw,
};

struct ThingPlacementWheelContext
{
    bool viewportAvailable{};
    bool watchedViewport{};
    bool wheelEvent{};
    bool pointerInsidePanel{};
    bool thingPlacementMode{};
    bool templateSelected{};
    bool flyNavigationActive{};
    bool yawShortcut{};
};

/**
 * Returns true when a wheel event's modifiers mean "rotate thing placement yaw".
 */
bool isThingPlacementYawWheelShortcut(Qt::KeyboardModifiers modifiers) noexcept;

/**
 * Resolves how a viewport wheel event should be handled for thing placement.
 */
ThingPlacementWheelRoute thingPlacementWheelRoute(const ThingPlacementWheelContext& context) noexcept;

/**
 * Converts Qt wheel deltas into logical wheel steps.
 *
 * Angle deltas are preferred because they are device-independent. Pixel deltas
 * are used as a touchpad fallback and keep the legacy scaling from MainWindow.
 */
double thingPlacementWheelSteps(const QPoint& angleDelta, const QPoint& pixelDelta) noexcept;

/**
 * Applies wheel steps to the placement yaw and wraps the result into one turn.
 */
double updatedThingPlacementYaw(double currentYawDeg, double wheelSteps) noexcept;

/**
 * Returns whether the computed wheel steps are large enough to update placement yaw.
 */
bool hasThingPlacementWheelSteps(double wheelSteps) noexcept;

/**
 * Removes the yaw-wheel shortcut modifiers before recomputing the placement preview.
 */
Qt::KeyboardModifiers thingPlacementYawPreviewModifiers(Qt::KeyboardModifiers modifiers) noexcept;

} // namespace sed::ui::viewport
