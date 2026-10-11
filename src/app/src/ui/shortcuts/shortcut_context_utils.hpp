#pragma once

#include <QList>
#include <QKeySequence>
#include <QStringList>
#include <Qt>

class QAction;
class QWidget;

namespace sed::ui::shortcuts
{

enum class ViewportRollShortcut
{
    none,
    left,
    right,
    reset,
};

enum class FlyNavigationShortcut
{
    none,
    fast,
    slow,
    forward,
    backward,
    left,
    right,
    down,
    up,
    speedUp,
    speedDown,
};

enum class ThingEditShortcut
{
    none,
    select,
    move,
    rotate,
    scale,
    toggleOrientation,
};

/**
 * Shortcut lists for the fly-navigation actions. Keeping the lists separate
 * lets tests verify matching without constructing QActions.
 */
struct FlyNavigationShortcutBindings
{
    QList<QKeySequence> fast;
    QList<QKeySequence> forward;
    QList<QKeySequence> backward;
    QList<QKeySequence> left;
    QList<QKeySequence> right;
    QList<QKeySequence> down;
    QList<QKeySequence> up;
    QList<QKeySequence> slow;
    QList<QKeySequence> speedUp{};
    QList<QKeySequence> speedDown{};
};

/**
 * Shortcut lists for editor transform tools. Separate lists keep the
 * resolver testable without QActions.
 */
struct ThingEditShortcutBindings
{
    QList<QKeySequence> select;
    QList<QKeySequence> move;
    QList<QKeySequence> rotate;
    QList<QKeySequence> scale;
    QList<QKeySequence> toggleOrientation;
};

struct UvEditorShortcutForwardingContext
{
    bool keyEventAvailable{};
    bool autoRepeat{};
    bool panelVisible{};
    bool focusInsidePanel{};
    bool pointerInsideCanvas{};
    bool textEntryFocusContext{};
    bool editorWidgetAvailable{};
    bool forwardingAlreadyActive{};
};

struct ModifierPreviewContext
{
    bool viewportAvailable{};
    bool viewportFocusContext{};
    bool viewportPointerContext{};
    bool flyNavigationActive{};
};

struct EditorSaveShortcutContext
{
    bool keyEventAvailable{};
    bool pressed{};
    bool autoRepeat{};
    bool cogEditorFocusContext{};
};

struct FlyNavigationDispatchContext
{
    bool viewportAvailable{};
    bool flyNavigationActive{};
    bool keyEventAvailable{};
    bool autoRepeat{};
    bool textEntryFocusContext{};
};

struct ThingEditDispatchContext
{
    bool viewportAvailable{};
    bool keyEventAvailable{};
    bool pressed{};
    bool autoRepeat{};
    bool viewportShortcutContext{};
};

/**
 * Shared gate for discrete editor commands that are dispatched from viewport
 * key presses. Kept as an alias so older thing-mode tests continue to validate
 * the same behavior.
 */
using EditorShortcutDispatchContext = ThingEditDispatchContext;

struct ViewportRollDispatchContext
{
    bool viewportAvailable{};
    bool keyEventAvailable{};
    bool pressed{};
    bool autoRepeat{};
    bool inViewport{};
};

struct KeymapShortcutBinding
{
    const QAction* action{};
    QString label{};
    QList<QKeySequence> shortcuts{};
};

/**
 * Reports the first overlapping shortcut in simultaneously active contexts.
 * Focused COG/UV and exclusive Fly bindings are separate from viewport input.
 * Only an exact sequence shared by both recorded default lists is retained as
 * an intentional default alias; other alternatives still undergo validation.
 */
QString keymapShortcutConflict(const QList<KeymapShortcutBinding>& bindings);

bool focusAcceptsTextInput(const QWidget* focusWidget) noexcept;
bool shouldDispatchViewportShortcut(bool inViewport, bool focusAcceptsTextInput) noexcept;

/**
 * Finds the editable native text control that owns focus, including the line
 * edit inside an editable combo box or spin box.
 */
QWidget* focusedNativeTextEditor(QWidget* focusWidget) noexcept;

/** Returns whether the focused native text control can undo or redo. */
bool nativeTextEditorCanUndoRedo(QWidget* focusWidget, bool redo) noexcept;

/** Runs native text undo/redo and reports whether a focused editor was found. */
bool triggerNativeTextEditorUndoRedo(QWidget* focusWidget, bool redo) noexcept;

/**
 * Returns true for bare keys that should be forwarded to the UV editor when the
 * pointer is over that panel but keyboard focus is still elsewhere.
 */
bool isUvEditorForwardedShortcutKey(int key, Qt::KeyboardModifiers modifiers) noexcept;

/**
 * Returns true when a key event should be forwarded to the UV editor panel even
 * though keyboard focus is currently outside that panel.
 */
bool shouldForwardUvEditorShortcut(
    const UvEditorShortcutForwardingContext& context,
    int key,
    Qt::KeyboardModifiers modifiers) noexcept;

/**
 * Returns true when a modifier key should refresh an active sector-draw preview.
 */
bool shouldUpdateSectorDrawModifierPreview(
    const ModifierPreviewContext& context,
    int key,
    bool cubePlacementActive,
    bool sectorDrawActive) noexcept;

/**
 * Returns true when a modifier key should refresh the thing-placement preview.
 */
bool shouldUpdateThingPlacementModifierPreview(
    const ModifierPreviewContext& context,
    int key,
    bool thingPlacementActive) noexcept;

/**
 * Returns true for the editor-save shortcut used by embedded script editors.
 */
bool isEditorSaveShortcutKey(int key, Qt::KeyboardModifiers modifiers) noexcept;

/**
 * Returns true when a key event should trigger the active embedded COG editor save.
 */
bool shouldTriggerEditorSaveShortcut(
    const EditorSaveShortcutContext& context,
    int key,
    Qt::KeyboardModifiers modifiers) noexcept;

/**
 * Returns true when a key event may be sent to the fly-navigation controller.
 */
bool shouldDispatchFlyNavigationShortcut(const FlyNavigationDispatchContext& context) noexcept;

/**
 * Returns true when a key event may be sent to editor transform shortcuts.
 */
bool shouldDispatchThingEditShortcut(const ThingEditDispatchContext& context) noexcept;

/**
 * Returns true when a key event may be sent to editor mode/tool shortcuts.
 */
bool shouldDispatchEditorShortcut(const EditorShortcutDispatchContext& context) noexcept;

/**
 * Returns true when a key event may be sent to viewport roll shortcuts.
 */
bool shouldDispatchViewportRollShortcut(const ViewportRollDispatchContext& context) noexcept;

/**
 * Returns true when a shortcut list has a one-step sequence matching the
 * current key event.
 */
bool shortcutListMatchesKey(
    const QList<QKeySequence>& shortcuts,
    int key,
    Qt::KeyboardModifiers modifiers,
    bool ignoreShiftModifier,
    Qt::KeyboardModifiers ignoredHeldModifiers = Qt::NoModifier) noexcept;

/**
 * Returns true when a persisted portable-text shortcut list is exactly one
 * former default, ignoring duplicate spellings of the same sequence. This is
 * used by one-time keymap migrations so customized bindings are never taken.
 */
bool shortcutSettingMatchesFormerDefault(
    const QStringList& configured,
    const QList<QKeySequence>& formerDefault);

/**
 * Returns true when an action has a one-step shortcut matching the current key
 * event. Shift can be ignored for fly-navigation style movement bindings.
 */
bool actionMatchesShortcut(
    const QAction* action,
    int key,
    Qt::KeyboardModifiers modifiers,
    bool ignoreShiftModifier) noexcept;

/**
 * Resolves the fly-navigation shortcut for a key event using the same priority
 * order as MainWindow action handling: fast, forward, backward, left, right,
 * down, then up. Shift is ignored so movement keys still work while holding the
 * fast modifier.
 */
FlyNavigationShortcut flyNavigationShortcutForKey(
    const FlyNavigationShortcutBindings& bindings,
    int key,
    Qt::KeyboardModifiers modifiers) noexcept;

/**
 * Returns true when fly navigation must claim Qt's ShortcutOverride event.
 *
 * Claiming the override makes Shift+movement reach the held-key controller
 * instead of activating an overlapping discrete QAction such as legacy
 * Shift+S "Snap Grid to Item".
 */
bool shouldClaimFlyNavigationShortcutOverride(
    const FlyNavigationDispatchContext& context,
    const FlyNavigationShortcutBindings& bindings,
    int key,
    Qt::KeyboardModifiers modifiers) noexcept;

/**
 * Resolves editor transform shortcuts using the same priority order as
 * MainWindow: select, move, rotate, then orientation toggle.
 */
ThingEditShortcut thingEditShortcutForKey(
    const ThingEditShortcutBindings& bindings,
    int key,
    Qt::KeyboardModifiers modifiers) noexcept;

/**
 * Resolves the viewport roll shortcut for Shift + Left/Right/Clear or the
 * equivalent keypad 4/6/5 keys reported by some Windows Qt setups.
 */
ViewportRollShortcut viewportRollShortcutForKey(
    int key,
    Qt::KeyboardModifiers modifiers,
    bool inViewport) noexcept;

} // namespace sed::ui::shortcuts
