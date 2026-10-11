#include "ui/shortcuts/selection_modifier_utils.hpp"

#include <QAction>
#include <QKeySequence>
#include <QStringList>

namespace sed::ui::shortcuts
{
namespace
{

constexpr qsizetype kEditorModifierBindingLimit = 15;

std::optional<Qt::KeyboardModifiers> matchingModifierBinding(
    const Qt::KeyboardModifiers held,
    const ModifierBindings& bindings,
    const bool exact)
{
    for (const Qt::KeyboardModifiers rawBinding : bindings)
    {
        const Qt::KeyboardModifiers binding = rawBinding & editorModifierMask();
        if (binding == Qt::NoModifier)
        {
            continue;
        }
        if (exact ? modifierExact(held, binding) : modifierActive(held, binding))
        {
            return binding;
        }
    }
    return std::nullopt;
}

QString combinedModifierText(
    const std::optional<Qt::KeyboardModifiers>& first,
    const std::optional<Qt::KeyboardModifiers>& second)
{
    return modifierText(
        first.value_or(Qt::NoModifier) | second.value_or(Qt::NoModifier));
}

} // namespace

Qt::KeyboardModifiers editorModifierMask() noexcept
{
    return Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
}

std::optional<Qt::KeyboardModifiers> decodeModifierOnlyShortcut(const QKeySequence& sequence) noexcept
{
    if (sequence.count() != 1)
    {
        return std::nullopt;
    }

    const int packed = sequence[0];
    Qt::KeyboardModifiers modifiers = Qt::KeyboardModifiers(packed & int(Qt::KeyboardModifierMask)) & editorModifierMask();
    const int key = packed & ~int(Qt::KeyboardModifierMask);

    switch (key)
    {
        case 0:
        case Qt::Key_unknown:
            break;
        case Qt::Key_Shift:
            modifiers |= Qt::ShiftModifier;
            break;
        case Qt::Key_Control:
            modifiers |= Qt::ControlModifier;
            break;
        case Qt::Key_Alt:
            modifiers |= Qt::AltModifier;
            break;
        case Qt::Key_Meta:
            modifiers |= Qt::MetaModifier;
            break;
        default:
            return std::nullopt;
    }

    return modifiers & editorModifierMask();
}

Qt::KeyboardModifiers modifiersFromActionShortcuts(
    const QAction* action,
    const Qt::KeyboardModifiers fallback) noexcept
{
    if (action)
    {
        for (const QKeySequence& sequence : action->shortcuts())
        {
            if (const std::optional<Qt::KeyboardModifiers> modifiers = decodeModifierOnlyShortcut(sequence))
            {
                return *modifiers;
            }
        }
    }

    return fallback & editorModifierMask();
}

ModifierBindings modifierBindingsFromActionShortcuts(
    const QAction* action,
    const Qt::KeyboardModifiers fallback)
{
    ModifierBindings bindings{};
    bindings.reserve(kEditorModifierBindingLimit);
    if (!action)
    {
        const Qt::KeyboardModifiers binding = fallback & editorModifierMask();
        if (binding != Qt::NoModifier)
        {
            bindings.push_back(binding);
        }
        return bindings;
    }

    const QList<QKeySequence> shortcuts = action->shortcuts();
    for (const QKeySequence& sequence : shortcuts)
    {
        const std::optional<Qt::KeyboardModifiers> decoded =
            decodeModifierOnlyShortcut(sequence);
        if (!decoded || *decoded == Qt::NoModifier || bindings.contains(*decoded))
        {
            continue;
        }
        bindings.push_back(*decoded);
        if (bindings.size() == kEditorModifierBindingLimit)
        {
            break;
        }
    }
    return bindings;
}

ModifierBindings normalizedModifierBindings(const ModifierBindings& bindings)
{
    ModifierBindings normalized{};
    normalized.reserve(kEditorModifierBindingLimit);
    for (const Qt::KeyboardModifiers rawBinding : bindings)
    {
        const Qt::KeyboardModifiers binding = rawBinding & editorModifierMask();
        if (binding == Qt::NoModifier || normalized.contains(binding))
        {
            continue;
        }
        normalized.push_back(binding);
        if (normalized.size() == kEditorModifierBindingLimit)
        {
            break;
        }
    }
    return normalized;
}

bool modifierBindingsActive(
    const Qt::KeyboardModifiers held,
    const ModifierBindings& required) noexcept
{
    return matchingModifierBinding(held, required, false).has_value();
}

bool modifierBindingsExact(
    const Qt::KeyboardModifiers held,
    const ModifierBindings& required) noexcept
{
    return matchingModifierBinding(held, required, true).has_value();
}

QString modifierBindingsText(const ModifierBindings& bindings)
{
    QStringList labels{};
    for (const Qt::KeyboardModifiers rawBinding : bindings)
    {
        const Qt::KeyboardModifiers binding = rawBinding & editorModifierMask();
        if (binding == Qt::NoModifier)
        {
            continue;
        }
        const QString label = modifierText(binding);
        if (!labels.contains(label))
        {
            labels.push_back(label);
        }
    }
    return labels.isEmpty() ? QStringLiteral("None") : labels.join(QStringLiteral(" or "));
}

bool modifierActive(const Qt::KeyboardModifiers held, const Qt::KeyboardModifiers required) noexcept
{
    const Qt::KeyboardModifiers heldMasked = held & editorModifierMask();
    const Qt::KeyboardModifiers requiredMasked = required & editorModifierMask();
    if (requiredMasked == Qt::NoModifier)
    {
        return heldMasked == Qt::NoModifier;
    }
    return (heldMasked & requiredMasked) == requiredMasked;
}

bool modifierExact(const Qt::KeyboardModifiers held, const Qt::KeyboardModifiers required) noexcept
{
    const Qt::KeyboardModifiers requiredMasked = required & editorModifierMask();
    return requiredMasked != Qt::NoModifier && (held & editorModifierMask()) == requiredMasked;
}

bool selectionModifierRequestsClickDispatch(
    const Qt::KeyboardModifiers held,
    const Qt::KeyboardModifiers addModifiers,
    const Qt::KeyboardModifiers removeModifiers) noexcept
{
    const Qt::KeyboardModifiers addMasked = addModifiers & editorModifierMask();
    const Qt::KeyboardModifiers removeMasked = removeModifiers & editorModifierMask();
    return (addMasked != Qt::NoModifier && modifierActive(held, addMasked))
        || (removeMasked != Qt::NoModifier && modifierActive(held, removeMasked));
}

bool selectionModifierRequestsClickDispatch(
    const Qt::KeyboardModifiers held,
    const ModifierBindings& addModifiers,
    const ModifierBindings& removeModifiers) noexcept
{
    const auto activeNonEmptyBinding = [held](const ModifierBindings& bindings)
    {
        return modifierBindingsActive(held, bindings);
    };
    return activeNonEmptyBinding(addModifiers) || activeNonEmptyBinding(removeModifiers);
}

bool selectionModifiersAllowXrayCycle(
    const Qt::KeyboardModifiers held,
    const Qt::KeyboardModifiers addModifiers) noexcept
{
    const Qt::KeyboardModifiers heldMasked = held & editorModifierMask();
    return heldMasked == Qt::NoModifier
        || modifierExact(heldMasked, addModifiers);
}

bool selectionModifiersAllowXrayCycle(
    const Qt::KeyboardModifiers held,
    const ModifierBindings& addModifiers) noexcept
{
    const Qt::KeyboardModifiers heldMasked = held & editorModifierMask();
    return heldMasked == Qt::NoModifier || modifierBindingsExact(heldMasked, addModifiers);
}

bool modifierToggledSetting(
    const bool defaultEnabled,
    const Qt::KeyboardModifiers held,
    const Qt::KeyboardModifiers toggleModifiers) noexcept
{
    const Qt::KeyboardModifiers toggleMasked = toggleModifiers & editorModifierMask();
    if (toggleMasked == Qt::NoModifier)
    {
        return defaultEnabled;
    }

    // Transform options have independent modifier-only commands. Exact
    // matching prevents a Ctrl+Alt command from also toggling the Ctrl-only
    // and Alt-only options during the same drag.
    return defaultEnabled != modifierExact(held, toggleMasked);
}

bool modifierToggledSetting(
    const bool defaultEnabled,
    const Qt::KeyboardModifiers held,
    const ModifierBindings& toggleModifiers) noexcept
{
    const bool assignedBindingHeld = modifierBindingsExact(held, toggleModifiers);
    return defaultEnabled != assignedBindingHeld;
}

SurfaceTransformSnapModifierState surfaceTransformSnapModifierState(
    const bool gridDefaultEnabled,
    const bool nearbyDefaultEnabled,
    const Qt::KeyboardModifiers held,
    const Qt::KeyboardModifiers gridToggleModifiers,
    const Qt::KeyboardModifiers nearbyToggleModifiers) noexcept
{
    SurfaceTransformSnapModifierState result{
        modifierToggledSetting(
            gridDefaultEnabled, held, gridToggleModifiers),
        modifierToggledSetting(
            nearbyDefaultEnabled, held, nearbyToggleModifiers),
        false,
    };

    const Qt::KeyboardModifiers gridBinding =
        gridToggleModifiers & editorModifierMask();
    const Qt::KeyboardModifiers nearbyBinding =
        nearbyToggleModifiers & editorModifierMask();
    const Qt::KeyboardModifiers combinedBinding =
        gridBinding | nearbyBinding;
    result.compositionChord =
        gridBinding != Qt::NoModifier
        && nearbyBinding != Qt::NoModifier
        && gridBinding != nearbyBinding
        && modifierExact(held, combinedBinding);
    if (result.compositionChord)
    {
        result.gridSnap = true;
        result.nearbyGeometrySnap = true;
    }
    return result;
}

SurfaceTransformSnapModifierState surfaceTransformSnapModifierState(
    const bool gridDefaultEnabled,
    const bool nearbyDefaultEnabled,
    const Qt::KeyboardModifiers held,
    const ModifierBindings& gridToggleModifiers,
    const ModifierBindings& nearbyToggleModifiers) noexcept
{
    SurfaceTransformSnapModifierState result{
        modifierToggledSetting(gridDefaultEnabled, held, gridToggleModifiers),
        modifierToggledSetting(nearbyDefaultEnabled, held, nearbyToggleModifiers),
        false,
    };

    for (const Qt::KeyboardModifiers gridBinding : gridToggleModifiers)
    {
        const Qt::KeyboardModifiers grid = gridBinding & editorModifierMask();
        if (grid == Qt::NoModifier)
        {
            continue;
        }
        for (const Qt::KeyboardModifiers nearbyBinding : nearbyToggleModifiers)
        {
            const Qt::KeyboardModifiers nearby = nearbyBinding & editorModifierMask();
            if (nearby == Qt::NoModifier || grid == nearby)
            {
                continue;
            }
            const Qt::KeyboardModifiers combinedBinding = grid | nearby;
            if (modifierExact(held, combinedBinding))
            {
                result.compositionChord = true;
                result.gridSnap = true;
                result.nearbyGeometrySnap = true;
                return result;
            }
        }
    }
    return result;
}

bool isEditorModifierKey(const int key) noexcept
{
    return modifierForEditorKey(key) != Qt::NoModifier;
}

Qt::KeyboardModifier modifierForEditorKey(const int key) noexcept
{
    switch (key)
    {
        case Qt::Key_Shift:   return Qt::ShiftModifier;
        case Qt::Key_Control: return Qt::ControlModifier;
        case Qt::Key_Alt:     return Qt::AltModifier;
        case Qt::Key_Meta:    return Qt::MetaModifier;
        default:              return Qt::NoModifier;
    }
}

Qt::KeyboardModifiers modifiersWithKeyState(
    const Qt::KeyboardModifiers held,
    const int key,
    const bool pressed) noexcept
{
    const Qt::KeyboardModifier changedModifier = modifierForEditorKey(key);
    Qt::KeyboardModifiers updated = held & editorModifierMask();
    if (changedModifier == Qt::NoModifier)
    {
        return updated;
    }

    if (pressed)
    {
        updated |= changedModifier;
    }
    else
    {
        updated &= ~changedModifier;
    }
    return updated;
}

QString modifierText(const Qt::KeyboardModifiers modifiers)
{
    const Qt::KeyboardModifiers masked = modifiers & editorModifierMask();

    QString out{};
    auto append = [&out](const QString& part)
    {
        if (!out.isEmpty())
        {
            out += " ";
        }
        out += part;
    };

    if (masked.testFlag(Qt::ControlModifier))
    {
        append(QStringLiteral("Ctrl"));
    }
    if (masked.testFlag(Qt::ShiftModifier))
    {
        append(QStringLiteral("Shift"));
    }
    if (masked.testFlag(Qt::AltModifier))
    {
        append(QStringLiteral("Alt"));
    }
    if (masked.testFlag(Qt::MetaModifier))
    {
        append(QStringLiteral("Meta"));
    }

    return out.isEmpty() ? QStringLiteral("None") : out;
}

QString selectionModifierHintText(
    const SelectionHintMode mode,
    const Qt::KeyboardModifiers held,
    const SelectionModifierHintConfig& config)
{
    // Step 1: resolve which selection modifier behavior is active. Shortest-path
    // selection intentionally suppresses add/remove hints while it is active.
    const bool surfacePathSelect =
        mode == SelectionHintMode::surface
        && modifierActive(held, config.shortestPathModifiers);
    const bool removeOnly =
        modifierActive(held, config.removeModifiers)
        && !surfacePathSelect;
    const bool additive =
        modifierActive(held, config.addModifiers)
        && !surfacePathSelect;
    const bool bypassSurfaceFilters =
        (mode == SelectionHintMode::surface || mode == SelectionHintMode::sector)
        && modifierActive(held, config.bypassFiltersModifiers);
    const bool bypassThingOcclusion =
        mode == SelectionHintMode::thing
        && modifierActive(held, config.bypassFiltersModifiers);

    // Step 2: format the most specific active hint first.
    const QString surfaceEntityLabel =
        mode == SelectionHintMode::sector ? QStringLiteral("Sectors") : QStringLiteral("Surfaces");
    if (surfacePathSelect && bypassSurfaceFilters)
    {
        return QStringLiteral("%1 Click - Pick Shortest Path (%2 Click/Drag - Xray %3)")
            .arg(modifierText(config.shortestPathModifiers))
            .arg(modifierText(config.bypassFiltersModifiers))
            .arg(surfaceEntityLabel);
    }
    if (surfacePathSelect)
    {
        return QStringLiteral("%1 Click - Pick Shortest Path")
            .arg(modifierText(config.shortestPathModifiers));
    }
    if (removeOnly && bypassSurfaceFilters)
    {
        return QStringLiteral("%1 Click/Drag - Remove From Selection (Xray %2)")
            .arg(modifierText(config.removeModifiers | config.bypassFiltersModifiers))
            .arg(surfaceEntityLabel);
    }
    if (additive && bypassSurfaceFilters)
    {
        return QStringLiteral("%1 Click/Drag - Add To Selection (Xray %2)")
            .arg(modifierText(config.addModifiers | config.bypassFiltersModifiers))
            .arg(surfaceEntityLabel);
    }
    if (removeOnly && bypassThingOcclusion)
    {
        return QStringLiteral("%1 Click/Drag - Remove From Selection (Xray Things)")
            .arg(modifierText(config.removeModifiers | config.bypassFiltersModifiers));
    }
    if (additive && bypassThingOcclusion)
    {
        return QStringLiteral("%1 Click/Drag - Add To Selection (Xray Things)")
            .arg(modifierText(config.addModifiers | config.bypassFiltersModifiers));
    }
    if (removeOnly)
    {
        return QStringLiteral("%1 Click/Drag - Remove From Selection")
            .arg(modifierText(config.removeModifiers));
    }
    if (additive)
    {
        return QStringLiteral("%1 Click/Drag - Add To Selection")
            .arg(modifierText(config.addModifiers));
    }
    if (bypassSurfaceFilters)
    {
        return QStringLiteral("%1 Click/Drag - Xray %2")
            .arg(modifierText(config.bypassFiltersModifiers))
            .arg(surfaceEntityLabel);
    }
    if (bypassThingOcclusion)
    {
        return QStringLiteral("%1 Click/Drag - Xray Things")
            .arg(modifierText(config.bypassFiltersModifiers));
    }

    return {};
}

QString selectionModifierHintText(
    const SelectionHintMode mode,
    const Qt::KeyboardModifiers held,
    const SelectionModifierHintBindingsConfig& config)
{
    const std::optional<Qt::KeyboardModifiers> pathBinding =
        mode == SelectionHintMode::surface
        ? matchingModifierBinding(held, config.shortestPathBindings, false)
        : std::nullopt;
    const std::optional<Qt::KeyboardModifiers> removeBinding =
        matchingModifierBinding(held, config.removeBindings, false);
    const std::optional<Qt::KeyboardModifiers> addBinding =
        matchingModifierBinding(held, config.addBindings, false);
    const bool surfacePathSelect = pathBinding.has_value();
    const bool removeOnly = removeBinding.has_value() && !surfacePathSelect;
    const bool additive = addBinding.has_value() && !surfacePathSelect;
    const bool bypassSurfaceFilters =
        (mode == SelectionHintMode::surface || mode == SelectionHintMode::sector)
        && matchingModifierBinding(held, config.bypassFiltersBindings, false).has_value();
    const bool bypassThingOcclusion =
        mode == SelectionHintMode::thing
        && matchingModifierBinding(held, config.bypassFiltersBindings, false).has_value();
    const std::optional<Qt::KeyboardModifiers> bypassBinding =
        (bypassSurfaceFilters || bypassThingOcclusion)
        ? matchingModifierBinding(held, config.bypassFiltersBindings, false)
        : std::nullopt;

    const QString surfaceEntityLabel =
        mode == SelectionHintMode::sector ? QStringLiteral("Sectors") : QStringLiteral("Surfaces");
    if (surfacePathSelect && bypassSurfaceFilters)
    {
        return QStringLiteral("%1 Click - Pick Shortest Path (%2 Click/Drag - Xray %3)")
            .arg(modifierText(*pathBinding))
            .arg(modifierText(*bypassBinding))
            .arg(surfaceEntityLabel);
    }
    if (surfacePathSelect)
    {
        return QStringLiteral("%1 Click - Pick Shortest Path")
            .arg(modifierText(*pathBinding));
    }
    if (removeOnly && bypassSurfaceFilters)
    {
        return QStringLiteral("%1 Click/Drag - Remove From Selection (Xray %2)")
            .arg(combinedModifierText(removeBinding, bypassBinding))
            .arg(surfaceEntityLabel);
    }
    if (additive && bypassSurfaceFilters)
    {
        return QStringLiteral("%1 Click/Drag - Add To Selection (Xray %2)")
            .arg(combinedModifierText(addBinding, bypassBinding))
            .arg(surfaceEntityLabel);
    }
    if (removeOnly && bypassThingOcclusion)
    {
        return QStringLiteral("%1 Click/Drag - Remove From Selection (Xray Things)")
            .arg(combinedModifierText(removeBinding, bypassBinding));
    }
    if (additive && bypassThingOcclusion)
    {
        return QStringLiteral("%1 Click/Drag - Add To Selection (Xray Things)")
            .arg(combinedModifierText(addBinding, bypassBinding));
    }
    if (removeOnly)
    {
        return QStringLiteral("%1 Click/Drag - Remove From Selection")
            .arg(modifierText(*removeBinding));
    }
    if (additive)
    {
        return QStringLiteral("%1 Click/Drag - Add To Selection")
            .arg(modifierText(*addBinding));
    }
    if (bypassSurfaceFilters)
    {
        return QStringLiteral("%1 Click/Drag - Xray %2")
            .arg(modifierText(*bypassBinding))
            .arg(surfaceEntityLabel);
    }
    if (bypassThingOcclusion)
    {
        return QStringLiteral("%1 Click/Drag - Xray Things")
            .arg(modifierText(*bypassBinding));
    }

    return {};
}

bool shouldShowSelectionModifierHint(const SelectionModifierHintEligibility& eligibility) noexcept
{
    // Step 1: Selection hints belong to editor viewport interactions only.
    if (!eligibility.viewportAvailable || eligibility.flyNavigationActive)
    {
        return false;
    }

    // Step 2: Click-edit tools and placement modes own the mouse, so the
    // selection modifier hint should disappear while they are active.
    if (!eligibility.selectionToolActive || eligibility.editToolActive)
    {
        return false;
    }

    // Step 3: The hint should be visible when either keyboard focus or the
    // pointer is in the viewport interaction context.
    return eligibility.viewportFocusContext || eligibility.viewportPointerContext;
}

SelectionModifierHintStatusUpdate selectionModifierHintStatusUpdate(
    const SelectionModifierHintStatusState& state,
    const QString& nextHintText)
{
    // Step 1: Non-empty hint text should always be shown and tracked as the
    // current selection-modifier hint.
    if (!nextHintText.isEmpty())
    {
        return SelectionModifierHintStatusUpdate{
            .action              = SelectionModifierHintStatusAction::show,
            .clearCurrentMessage = false,
            .trackedText         = nextHintText,
        };
    }

    // Step 2: Empty hint text only needs work when a previous hint is currently
    // tracked as visible.
    if (!state.visible)
    {
        return {};
    }

    // Step 3: Hide the tracked hint, but only clear the visible status-bar
    // message when it still matches the message owned by this feature.
    return SelectionModifierHintStatusUpdate{
        .action              = SelectionModifierHintStatusAction::hide,
        .clearCurrentMessage = state.currentMessage == state.trackedText,
        .trackedText         = {},
    };
}

} // namespace sed::ui::shortcuts
