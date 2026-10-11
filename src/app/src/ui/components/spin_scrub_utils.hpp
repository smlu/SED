#pragma once

#include <Qt>

class QAbstractSpinBox;
class QObject;

namespace sed::ui
{

inline constexpr const char* kSpinScrubMarkerProperty = "sed.thingPlacementSpinScrub";

enum class SpinScrubEventRoute
{
    none,
    beginPending,
    keepPending,
    startAndApply,
    apply,
    cancelInactive,
    cancelActive,
    finishInactive,
    finishActive,
};

struct SpinScrubEventContext
{
    bool objectMarked{};
    bool spinAvailable{};
    bool spinEnabled{};
    bool mousePress{};
    bool mouseMove{};
    bool mouseRelease{};
    bool leftButton{};
    bool leftButtonHeld{};
    bool matchingSpin{};
    bool pending{};
    bool active{};
    bool startThresholdPassed{};
};

/**
 * Returns true when an object is marked as a spin-box scrub handle.
 */
bool objectHasSpinScrubMarker(const QObject* object) noexcept;

/**
 * Resolves the spin-scrub event phase from input/state flags.
 */
SpinScrubEventRoute spinScrubEventRoute(const SpinScrubEventContext& context) noexcept;

/**
 * Finds the spin box owned by the scrub-marked object or one of its widget parents.
 */
QAbstractSpinBox* findSpinScrubBox(QObject* object) noexcept;

/**
 * Reads a supported spin-box value as a double.
 */
double spinScrubValue(const QAbstractSpinBox* spin) noexcept;

/**
 * Reads a supported spin-box step as a double.
 */
double spinScrubStep(const QAbstractSpinBox* spin) noexcept;

/**
 * Writes a value back to a supported spin box, rounding for integer boxes.
 */
void setSpinScrubValue(QAbstractSpinBox* spin, double value);

/**
 * Returns whether a drag delta is large enough to turn a pending click into a scrub.
 */
bool spinScrubShouldStart(int dragDeltaPx) noexcept;

/**
 * Converts held modifiers into the scrub speed multiplier.
 */
double spinScrubMultiplier(Qt::KeyboardModifiers modifiers) noexcept;

/**
 * Computes the next value for a horizontal spin-box scrub.
 */
double spinScrubNextValue(
    double startValue,
    int dragDeltaPx,
    double singleStep,
    Qt::KeyboardModifiers modifiers) noexcept;

} // namespace sed::ui
