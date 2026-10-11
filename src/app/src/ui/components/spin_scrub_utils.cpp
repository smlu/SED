#include "spin_scrub_utils.hpp"

#include <QAbstractSpinBox>
#include <QDoubleSpinBox>
#include <QObject>
#include <QSpinBox>
#include <QWidget>

#include <cmath>

namespace sed::ui
{
namespace
{
constexpr int kStartThresholdPx = 2;
constexpr double kPixelsPerStep = 6.0;
constexpr double kFineMultiplier = 0.1;
constexpr double kCoarseMultiplier = 10.0;
} // namespace

bool objectHasSpinScrubMarker(const QObject* object) noexcept
{
    return object && object->property(kSpinScrubMarkerProperty).toBool();
}

SpinScrubEventRoute spinScrubEventRoute(const SpinScrubEventContext& context) noexcept
{
    // Step 1: Ignore anything that is not a marked, enabled spin-box scrub handle.
    if (!context.objectMarked || !context.spinAvailable || !context.spinEnabled)
    {
        return SpinScrubEventRoute::none;
    }

    // Step 2: Left press arms a pending scrub but lets the original click continue.
    if (context.mousePress)
    {
        return context.leftButton
            ? SpinScrubEventRoute::beginPending
            : SpinScrubEventRoute::none;
    }

    // Step 3: Move events either cancel, stay pending, or apply scrub deltas.
    if (context.mouseMove)
    {
        if (!context.matchingSpin || (!context.pending && !context.active))
        {
            return SpinScrubEventRoute::none;
        }
        if (!context.leftButtonHeld)
        {
            return context.active
                ? SpinScrubEventRoute::cancelActive
                : SpinScrubEventRoute::cancelInactive;
        }
        if (!context.active && !context.startThresholdPassed)
        {
            return SpinScrubEventRoute::keepPending;
        }
        return context.active
            ? SpinScrubEventRoute::apply
            : SpinScrubEventRoute::startAndApply;
    }

    // Step 4: Left release finishes the active scrub, or lets an inactive click through.
    if (context.mouseRelease)
    {
        if (!context.matchingSpin || !context.leftButton)
        {
            return SpinScrubEventRoute::none;
        }
        return context.active
            ? SpinScrubEventRoute::finishActive
            : SpinScrubEventRoute::finishInactive;
    }

    return SpinScrubEventRoute::none;
}

QAbstractSpinBox* findSpinScrubBox(QObject* object) noexcept
{
    if (auto* spin = qobject_cast<QAbstractSpinBox*>(object))
    {
        return spin;
    }

    if (auto* widget = qobject_cast<QWidget*>(object))
    {
        for (QWidget* parent = widget->parentWidget(); parent; parent = parent->parentWidget())
        {
            if (auto* spin = qobject_cast<QAbstractSpinBox*>(parent))
            {
                return spin;
            }
        }
    }
    return nullptr;
}

double spinScrubValue(const QAbstractSpinBox* spin) noexcept
{
    if (const auto* doubleSpin = qobject_cast<const QDoubleSpinBox*>(spin))
    {
        return doubleSpin->value();
    }
    if (const auto* intSpin = qobject_cast<const QSpinBox*>(spin))
    {
        return static_cast<double>(intSpin->value());
    }
    return 0.0;
}

double spinScrubStep(const QAbstractSpinBox* spin) noexcept
{
    if (const auto* doubleSpin = qobject_cast<const QDoubleSpinBox*>(spin))
    {
        return doubleSpin->singleStep();
    }
    if (const auto* intSpin = qobject_cast<const QSpinBox*>(spin))
    {
        return static_cast<double>(intSpin->singleStep());
    }
    return 1.0;
}

void setSpinScrubValue(QAbstractSpinBox* spin, const double value)
{
    if (auto* doubleSpin = qobject_cast<QDoubleSpinBox*>(spin))
    {
        doubleSpin->setValue(value);
    }
    else if (auto* intSpin = qobject_cast<QSpinBox*>(spin))
    {
        intSpin->setValue(static_cast<int>(std::llround(value)));
    }
}

bool spinScrubShouldStart(const int dragDeltaPx) noexcept
{
    return std::abs(dragDeltaPx) > kStartThresholdPx;
}

double spinScrubMultiplier(const Qt::KeyboardModifiers modifiers) noexcept
{
    if (modifiers.testFlag(Qt::ShiftModifier))
    {
        return kFineMultiplier;
    }
    if (modifiers.testFlag(Qt::ControlModifier))
    {
        return kCoarseMultiplier;
    }
    return 1.0;
}

double spinScrubNextValue(
    const double startValue,
    const int dragDeltaPx,
    const double singleStep,
    const Qt::KeyboardModifiers modifiers) noexcept
{
    const double steps = static_cast<double>(dragDeltaPx) / kPixelsPerStep;
    return startValue + steps * singleStep * spinScrubMultiplier(modifiers);
}

} // namespace sed::ui
