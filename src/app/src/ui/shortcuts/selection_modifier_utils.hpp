#pragma once

#include <optional>

#include <QList>
#include <Qt>
#include <QString>

class QAction;
class QKeySequence;

namespace sed::ui::shortcuts
{

/** Cached modifier-only shortcut alternatives for one editor behavior. */
using ModifierBindings = QList<Qt::KeyboardModifiers>;

enum class SelectionHintMode
{
    none,
    sector,
    surface,
    thing,
};

enum class SelectionModifierHintStatusAction
{
    none,
    show,
    hide,
};

struct SelectionModifierHintConfig
{
    Qt::KeyboardModifiers addModifiers{};
    Qt::KeyboardModifiers removeModifiers{};
    Qt::KeyboardModifiers shortestPathModifiers{};
    Qt::KeyboardModifiers bypassFiltersModifiers{};
};

struct SelectionModifierHintBindingsConfig
{
    ModifierBindings addBindings{};
    ModifierBindings removeBindings{};
    ModifierBindings shortestPathBindings{};
    ModifierBindings bypassFiltersBindings{};
};

struct SelectionModifierHintEligibility
{
    bool viewportAvailable{};
    bool flyNavigationActive{};
    bool selectionToolActive{};
    bool editToolActive{};
    bool viewportFocusContext{};
    bool viewportPointerContext{};
};

struct SelectionModifierHintStatusState
{
    bool visible{};
    QString trackedText{};
    QString currentMessage{};
};

struct SelectionModifierHintStatusUpdate
{
    SelectionModifierHintStatusAction action{SelectionModifierHintStatusAction::none};
    bool clearCurrentMessage{};
    QString trackedText{};
};

/** Returns the keyboard modifier bits that are meaningful for editor shortcut modes. */
Qt::KeyboardModifiers editorModifierMask() noexcept;

/** Parses a one-step shortcut that represents only keyboard modifiers, such as Ctrl or Shift+Alt. */
std::optional<Qt::KeyboardModifiers> decodeModifierOnlyShortcut(const QKeySequence& sequence) noexcept;

/** Returns the first modifier-only shortcut on an action, or the masked fallback when no shortcut matches. */
Qt::KeyboardModifiers modifiersFromActionShortcuts(const QAction* action, Qt::KeyboardModifiers fallback) noexcept;

/** Returns a normalized, deduplicated list of modifier-only action alternatives. */
ModifierBindings modifierBindingsFromActionShortcuts(
    const QAction* action,
    Qt::KeyboardModifiers fallback);

/** Normalizes a cached modifier list to the finite set of editor modifier chords. */
ModifierBindings normalizedModifierBindings(const ModifierBindings& bindings);

/** Returns true when any non-empty alternative is satisfied by the held modifiers. */
bool modifierBindingsActive(Qt::KeyboardModifiers held, const ModifierBindings& required) noexcept;

/** Returns true when the held modifiers exactly match any non-empty alternative. */
bool modifierBindingsExact(Qt::KeyboardModifiers held, const ModifierBindings& required) noexcept;

/** Formats all alternatives in order, separated by " or ". */
QString modifierBindingsText(const ModifierBindings& bindings);

/** Returns true when the held modifiers satisfy the required editor modifier combination. */
bool modifierActive(Qt::KeyboardModifiers held, Qt::KeyboardModifiers required) noexcept;

/** Returns true when the held modifiers exactly match a non-empty required editor modifier combination. */
bool modifierExact(Qt::KeyboardModifiers held, Qt::KeyboardModifiers required) noexcept;

/**
 * Returns true when an add/remove modifier requires a click to reach the
 * document selection handler even if the clicked entity is already the active
 * viewport entity. Empty bindings are treated as unassigned.
 */
bool selectionModifierRequestsClickDispatch(
    Qt::KeyboardModifiers held,
    Qt::KeyboardModifiers addModifiers,
    Qt::KeyboardModifiers removeModifiers) noexcept;

bool selectionModifierRequestsClickDispatch(
    Qt::KeyboardModifiers held,
    const ModifierBindings& addModifiers,
    const ModifierBindings& removeModifiers) noexcept;

/**
 * Returns true when X-ray click cycling may advance with the current selection
 * modifiers. Plain clicks and the exact configured Add chord are eligible;
 * remove/path/filter combinations retain their explicit front target.
 */
bool selectionModifiersAllowXrayCycle(
    Qt::KeyboardModifiers held,
    Qt::KeyboardModifiers addModifiers) noexcept;

bool selectionModifiersAllowXrayCycle(
    Qt::KeyboardModifiers held,
    const ModifierBindings& addModifiers) noexcept;

/**
 * Applies an exact-match hold-to-invert modifier to a boolean option.
 *
 * This is used for viewport options where the checked button sets the default
 * behavior and the bound modifier temporarily flips that behavior while held.
 * Extra held modifiers do not activate a shorter binding.
 */
bool modifierToggledSetting(
    bool defaultEnabled,
    Qt::KeyboardModifiers held,
    Qt::KeyboardModifiers toggleModifiers) noexcept;

bool modifierToggledSetting(
    bool defaultEnabled,
    Qt::KeyboardModifiers held,
    const ModifierBindings& toggleModifiers) noexcept;

struct SurfaceTransformSnapModifierState
{
    bool gridSnap{};
    bool nearbyGeometrySnap{};
    bool compositionChord{};
};

/**
 * Resolves the two Surface transform snap inverters while reserving their
 * exact combined chord for grid-first, nearby-geometry-second composition.
 * The normal exact-match behavior remains unchanged for every other chord.
 */
SurfaceTransformSnapModifierState surfaceTransformSnapModifierState(
    bool gridDefaultEnabled,
    bool nearbyDefaultEnabled,
    Qt::KeyboardModifiers held,
    Qt::KeyboardModifiers gridToggleModifiers,
    Qt::KeyboardModifiers nearbyToggleModifiers) noexcept;

SurfaceTransformSnapModifierState surfaceTransformSnapModifierState(
    bool gridDefaultEnabled,
    bool nearbyDefaultEnabled,
    Qt::KeyboardModifiers held,
    const ModifierBindings& gridToggleModifiers,
    const ModifierBindings& nearbyToggleModifiers) noexcept;

/** Returns true for Qt keys that directly represent editor keyboard modifiers. */
bool isEditorModifierKey(int key) noexcept;

/** Returns the editor modifier bit represented by a Qt key, or NoModifier for other keys. */
Qt::KeyboardModifier modifierForEditorKey(int key) noexcept;

/** Applies the current key press/release to a held modifier mask. */
Qt::KeyboardModifiers modifiersWithKeyState(Qt::KeyboardModifiers held, int key, bool pressed) noexcept;

/** Formats editor modifiers in the stable UI order: Ctrl, Shift, Alt, Meta. */
QString modifierText(Qt::KeyboardModifiers modifiers);

/**
 * Builds the status-bar hint for selection add/remove/path/xray modifiers.
 */
QString selectionModifierHintText(
    SelectionHintMode mode,
    Qt::KeyboardModifiers held,
    const SelectionModifierHintConfig& config);

QString selectionModifierHintText(
    SelectionHintMode mode,
    Qt::KeyboardModifiers held,
    const SelectionModifierHintBindingsConfig& config);

/**
 * Returns true when the viewport/editor state allows selection-modifier hints.
 */
bool shouldShowSelectionModifierHint(const SelectionModifierHintEligibility& eligibility) noexcept;

/**
 * Plans how a status bar should update the tracked selection-modifier hint.
 * Empty next text hides the hint only when the current status message is still
 * the tracked hint, so unrelated status messages are not cleared.
 */
SelectionModifierHintStatusUpdate selectionModifierHintStatusUpdate(
    const SelectionModifierHintStatusState& state,
    const QString& nextHintText);

} // namespace sed::ui::shortcuts
