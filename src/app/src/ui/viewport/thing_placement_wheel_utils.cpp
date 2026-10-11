#include "thing_placement_wheel_utils.hpp"

#include <cmath>

namespace sed::ui::viewport
{
namespace
{
constexpr Qt::KeyboardModifiers kYawWheelRequiredModifiers =
    Qt::ControlModifier | Qt::ShiftModifier;
constexpr Qt::KeyboardModifiers kModifierMask =
    Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
constexpr double kWheelAngleUnitsPerStep = 120.0;
constexpr double kWheelPixelUnitsPerStep = 80.0;
constexpr double kYawStepDeg = 5.0;
constexpr double kWheelStepEpsilon = 1.0e-6;
} // namespace

bool isThingPlacementYawWheelShortcut(const Qt::KeyboardModifiers modifiers) noexcept
{
    return (modifiers & kModifierMask) == kYawWheelRequiredModifiers;
}

ThingPlacementWheelRoute thingPlacementWheelRoute(const ThingPlacementWheelContext& context) noexcept
{
    // Step 1: Only wheel events delivered to the viewport participate in this routing.
    if (!context.viewportAvailable || !context.watchedViewport || !context.wheelEvent)
    {
        return ThingPlacementWheelRoute::none;
    }

    // Step 2: The floating placement panel owns wheel input while the pointer is over it.
    if (context.pointerInsidePanel)
    {
        return ThingPlacementWheelRoute::consumePanelWheel;
    }

    // Step 3: Otherwise, route the dedicated Ctrl+Shift wheel shortcut to yaw placement.
    if (context.thingPlacementMode
        && context.templateSelected
        && !context.flyNavigationActive
        && context.yawShortcut)
    {
        return ThingPlacementWheelRoute::rotatePlacementYaw;
    }

    return ThingPlacementWheelRoute::none;
}

double thingPlacementWheelSteps(const QPoint& angleDelta, const QPoint& pixelDelta) noexcept
{
    // Step 1: prefer vertical wheel angle deltas, which are the common mouse-wheel path.
    if (angleDelta.y() != 0)
    {
        return static_cast<double>(angleDelta.y()) / kWheelAngleUnitsPerStep;
    }

    // Step 2: use horizontal angle deltas when the device reports sideways scrolling.
    if (angleDelta.x() != 0)
    {
        return static_cast<double>(angleDelta.x()) / kWheelAngleUnitsPerStep;
    }

    // Step 3: touchpads often provide only pixel deltas, so preserve the old fallback scale.
    if (pixelDelta.y() != 0)
    {
        return static_cast<double>(pixelDelta.y()) / kWheelPixelUnitsPerStep;
    }
    if (pixelDelta.x() != 0)
    {
        return static_cast<double>(pixelDelta.x()) / kWheelPixelUnitsPerStep;
    }

    return 0.0;
}

double updatedThingPlacementYaw(const double currentYawDeg, const double wheelSteps) noexcept
{
    return std::remainder(currentYawDeg + wheelSteps * kYawStepDeg, 360.0);
}

bool hasThingPlacementWheelSteps(const double wheelSteps) noexcept
{
    return std::abs(wheelSteps) > kWheelStepEpsilon;
}

Qt::KeyboardModifiers thingPlacementYawPreviewModifiers(const Qt::KeyboardModifiers modifiers) noexcept
{
    return modifiers & ~kYawWheelRequiredModifiers;
}

} // namespace sed::ui::viewport
