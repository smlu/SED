#include "ui/shortcuts/shortcut_context_utils.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"
#include "ui/shortcuts/selection_modifier_utils.hpp"

#include <QAction>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSet>
#include <QTextEdit>
#include <QWidget>

namespace sed::ui::shortcuts
{
namespace
{

bool isEditableTextWidget(const QWidget* widget) noexcept
{
    if (!widget || !widget->isEnabled())
    {
        return false;
    }

    if (const auto* edit = qobject_cast<const QLineEdit*>(widget))
    {
        return !edit->isReadOnly();
    }
    if (const auto* edit = qobject_cast<const QTextEdit*>(widget))
    {
        return !edit->isReadOnly();
    }
    if (const auto* edit = qobject_cast<const QPlainTextEdit*>(widget))
    {
        return !edit->isReadOnly();
    }
    if (const auto* spin = qobject_cast<const QAbstractSpinBox*>(widget))
    {
        return !spin->isReadOnly();
    }
    if (const auto* combo = qobject_cast<const QComboBox*>(widget))
    {
        return combo->isEditable();
    }

    return qobject_cast<const QKeySequenceEdit*>(widget) != nullptr;
}

constexpr unsigned int kSectorShortcutDomain = 1u;
constexpr unsigned int kSurfaceShortcutDomain = 2u;
constexpr unsigned int kThingShortcutDomain = 4u;
constexpr unsigned int kLightShortcutDomain = 8u;
constexpr unsigned int kUvShortcutDomain = 16u;
constexpr unsigned int kCogShortcutDomain = 32u;
constexpr unsigned int kFlyShortcutDomain = 64u;
constexpr unsigned int kOtherShortcutDomain = 128u;
constexpr unsigned int kViewportShortcutDomains = kSectorShortcutDomain
    | kSurfaceShortcutDomain | kThingShortcutDomain | kLightShortcutDomain;
constexpr unsigned int kOrdinaryShortcutDomains = kViewportShortcutDomains
    | kUvShortcutDomain | kCogShortcutDomain | kOtherShortcutDomain;

unsigned int keymapShortcutDomains(const QAction& action)
{
    QString id = action.objectName().trimmed().toLower();
    if ( id.isEmpty() )
    {
        id = action.property("sed_generated_keymap_id").toString().trimmed().toLower();
    }
    if ( id.startsWith(QStringLiteral("cog_editor.")) )
    {
        return kCogShortcutDomain;
    }
    if ( id.startsWith(QStringLiteral("viewport.fly.")) )
    {
        return kFlyShortcutDomain;
    }
    if ( action.shortcutContext() == Qt::WidgetShortcut
        || action.shortcutContext() == Qt::WidgetWithChildrenShortcut )
    {
        for ( const QObject* owner : action.associatedObjects() )
        {
            for ( const QObject* current = owner; current; current = current->parent() )
            {
                if ( current->objectName() == QStringLiteral("uvEditorCanvas")
                    || current->objectName() == QStringLiteral("uvEditorPanel") )
                {
                    return kUvShortcutDomain;
                }
            }
        }
    }
    if ( (action.shortcutContext() == Qt::WindowShortcut
            || action.shortcutContext() == Qt::ApplicationShortcut)
        && !action.associatedObjects().isEmpty() )
    {
        // Registered Qt shortcuts remain active across focused editors. Only
        // the known focus-gated COG and exclusive Fly actions above use their
        // narrower runtime domains instead of this window/application scope.
        return kOrdinaryShortcutDomains;
    }
    if ( id.startsWith(QStringLiteral("viewport.sector."))
        || id.startsWith(QStringLiteral("sector."))
        || id.startsWith(QStringLiteral("sector_move.")) )
    {
        return kSectorShortcutDomain;
    }
    if ( id.startsWith(QStringLiteral("viewport.surface."))
        || id.startsWith(QStringLiteral("surface."))
        || id.startsWith(QStringLiteral("surface_move."))
        || id.startsWith(QStringLiteral("selection.mode.surface_")) )
    {
        return kSurfaceShortcutDomain;
    }
    if ( id.startsWith(QStringLiteral("viewport.frame."))
        || id.startsWith(QStringLiteral("thing."))
        || id.startsWith(QStringLiteral("thing_move."))
        || id.startsWith(QStringLiteral("thing_placement.")) )
    {
        return kThingShortcutDomain;
    }
    if ( id.startsWith(QStringLiteral("viewport.light_mode."))
        || id.startsWith(QStringLiteral("light.")) )
    {
        return kLightShortcutDomain;
    }
    if ( id.startsWith(QStringLiteral("cleave.transform.")) )
    {
        return kSectorShortcutDomain | kSurfaceShortcutDomain;
    }
    if ( id.startsWith(QStringLiteral("viewport."))
        || id.startsWith(QStringLiteral("editor.tool."))
        || id.startsWith(QStringLiteral("editor.transform."))
        || id.startsWith(QStringLiteral("edit_mode."))
        || id.startsWith(QStringLiteral("selection."))
        || id.startsWith(QStringLiteral("transform.")) )
    {
        return kViewportShortcutDomains;
    }
    for ( const QObject* owner : action.associatedObjects() )
    {
        for ( const QObject* current = owner; current; current = current->parent() )
        {
            const QString name = current->objectName();
            if ( name == QStringLiteral("edit.sector") )
            {
                return kSectorShortcutDomain;
            }
            if ( name == QStringLiteral("edit.surface") )
            {
                return kSurfaceShortcutDomain;
            }
            if ( name == QStringLiteral("edit.thing") )
            {
                return kThingShortcutDomain;
            }
            if ( name == QStringLiteral("edit.light") )
            {
                return kLightShortcutDomain;
            }
        }
    }
    return kOrdinaryShortcutDomains;
}

bool defaultIncludesShortcut(const QAction& action, const QKeySequence& sequence)
{
    for ( const QString& value : action.property("sed_default_shortcuts").toStringList() )
    {
        const QKeySequence candidate(value.trimmed(), QKeySequence::PortableText);
        if ( !candidate.isEmpty() && candidate == sequence )
        {
            return true;
        }
    }
    return false;
}

} // namespace

QString keymapShortcutConflict(const QList<KeymapShortcutBinding>& bindings)
{
    for ( qsizetype index = 0; index < bindings.size(); ++index )
    {
        const KeymapShortcutBinding& binding = bindings[index];
        if ( !binding.action )
        {
            continue;
        }
        const unsigned int domains = keymapShortcutDomains(*binding.action);
        for ( qsizetype previous = 0; previous < index; ++previous )
        {
            const KeymapShortcutBinding& other = bindings[previous];
            if ( !other.action || binding.action == other.action
                || (domains & keymapShortcutDomains(*other.action)) == 0u )
            {
                continue;
            }
            for ( const QKeySequence& sequence : binding.shortcuts )
            {
                if ( sequence.isEmpty() )
                {
                    continue;
                }
                for ( const QKeySequence& otherSequence : other.shortcuts )
                {
                    if ( otherSequence.isEmpty()
                        || (sequence.matches(otherSequence) == QKeySequence::NoMatch
                            && otherSequence.matches(sequence) == QKeySequence::NoMatch) )
                    {
                        continue;
                    }
                    if ( sequence == otherSequence
                        && defaultIncludesShortcut(*binding.action, sequence)
                        && defaultIncludesShortcut(*other.action, sequence) )
                    {
                        continue;
                    }
                    return QStringLiteral("%1 and %2 have overlapping shortcuts: %3 and %4.")
                        .arg(other.label, binding.label,
                            keySequenceDisplayText(otherSequence),
                            keySequenceDisplayText(sequence));
                }
            }
        }
    }
    return {};
}

bool focusAcceptsTextInput(const QWidget* focusWidget) noexcept
{
    for (const QWidget* current = focusWidget; current; current = current->parentWidget())
    {
        if (isEditableTextWidget(current))
        {
            return true;
        }
    }
    return false;
}

QWidget* focusedNativeTextEditor(QWidget* focusWidget) noexcept
{
    for ( QWidget* current = focusWidget; current; current = current->parentWidget() )
    {
        if ( QLineEdit* const lineEdit = qobject_cast<QLineEdit*>(current);
             lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly() )
        {
            return lineEdit;
        }
        if ( QAbstractSpinBox* const spinBox = qobject_cast<QAbstractSpinBox*>(current);
             spinBox && spinBox->isEnabled() && !spinBox->isReadOnly() )
        {
            // lineEdit() is protected in Qt 6.10. Use the public child lookup
            // because the focused widget may be the spin box itself.
            QLineEdit* const lineEdit = spinBox->findChild<QLineEdit*>();
            if ( lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly() )
            {
                return lineEdit;
            }
        }
        if ( QComboBox* const comboBox = qobject_cast<QComboBox*>(current);
             comboBox && comboBox->isEnabled() && comboBox->isEditable() )
        {
            QLineEdit* const lineEdit = comboBox->lineEdit();
            if ( lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly() )
            {
                return lineEdit;
            }
        }
        if ( QPlainTextEdit* const plainTextEdit = qobject_cast<QPlainTextEdit*>(current);
             plainTextEdit && plainTextEdit->isEnabled()
                 && !plainTextEdit->isReadOnly() )
        {
            return plainTextEdit;
        }
        if ( QTextEdit* const textEdit = qobject_cast<QTextEdit*>(current);
             textEdit && textEdit->isEnabled() && !textEdit->isReadOnly() )
        {
            return textEdit;
        }
    }
    return nullptr;
}

bool nativeTextEditorCanUndoRedo(QWidget* focusWidget, const bool redo) noexcept
{
    QWidget* const editor = focusedNativeTextEditor(focusWidget);
    if ( QLineEdit* const lineEdit = qobject_cast<QLineEdit*>(editor) )
    {
        return redo ? lineEdit->isRedoAvailable() : lineEdit->isUndoAvailable();
    }
    if ( QPlainTextEdit* const plainTextEdit = qobject_cast<QPlainTextEdit*>(editor) )
    {
        return redo
            ? plainTextEdit->document()->isRedoAvailable()
            : plainTextEdit->document()->isUndoAvailable();
    }
    if ( QTextEdit* const textEdit = qobject_cast<QTextEdit*>(editor) )
    {
        return redo ? textEdit->document()->isRedoAvailable()
                    : textEdit->document()->isUndoAvailable();
    }
    return false;
}

bool triggerNativeTextEditorUndoRedo(QWidget* focusWidget, const bool redo) noexcept
{
    QWidget* const editor = focusedNativeTextEditor(focusWidget);
    if ( QLineEdit* const lineEdit = qobject_cast<QLineEdit*>(editor) )
    {
        redo ? lineEdit->redo() : lineEdit->undo();
        return true;
    }
    if ( QPlainTextEdit* const plainTextEdit = qobject_cast<QPlainTextEdit*>(editor) )
    {
        redo ? plainTextEdit->redo() : plainTextEdit->undo();
        return true;
    }
    if ( QTextEdit* const textEdit = qobject_cast<QTextEdit*>(editor) )
    {
        redo ? textEdit->redo() : textEdit->undo();
        return true;
    }
    return false;
}

bool shouldDispatchViewportShortcut(const bool inViewport, const bool focusAcceptsTextInput) noexcept
{
    return inViewport && !focusAcceptsTextInput;
}

bool isUvEditorForwardedShortcutKey(
    const int key,
    const Qt::KeyboardModifiers modifiers) noexcept
{
    constexpr Qt::KeyboardModifiers kBlockingModifiers =
        Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
    if ((modifiers & kBlockingModifiers) != Qt::NoModifier)
    {
        return false;
    }

    switch (key)
    {
        case Qt::Key_G:
        case Qt::Key_R:
        case Qt::Key_S:
        case Qt::Key_A:
        case Qt::Key_Escape:
        case Qt::Key_Return:
        case Qt::Key_Enter:
            return true;
        default:
            return false;
    }
}

bool shouldForwardUvEditorShortcut(
    const UvEditorShortcutForwardingContext& context,
    const int key,
    const Qt::KeyboardModifiers modifiers) noexcept
{
    // Step 1: Only real, non-repeated key events can be forwarded.
    if (!context.keyEventAvailable || context.autoRepeat || context.forwardingAlreadyActive)
    {
        return false;
    }

    // Step 2: Forward only while the actual UV canvas owns the pointer and
    // keyboard focus is elsewhere.  The panel also contains text/spin edits;
    // hovering the dock must never steal G/S/R from a modal or property edit.
    if (!context.panelVisible
        || context.focusInsidePanel
        || !context.pointerInsideCanvas
        || context.textEntryFocusContext
        || !context.editorWidgetAvailable)
    {
        return false;
    }

    // Step 3: Keep the same bare-key allow-list used by the UV editor commands.
    return isUvEditorForwardedShortcutKey(key, modifiers);
}

bool shouldUpdateSectorDrawModifierPreview(
    const ModifierPreviewContext& context,
    const int key,
    const bool cubePlacementActive,
    const bool sectorDrawActive) noexcept
{
    // Step 1: Sector drawing only changes live preview shape on Shift.
    if (key != Qt::Key_Shift)
    {
        return false;
    }

    // Step 2: Refresh only while cube placement is actively drawing a sector.
    if (!cubePlacementActive || !sectorDrawActive || !context.viewportAvailable)
    {
        return false;
    }

    // Step 3: The preview can update from either viewport focus or hover.
    return context.viewportFocusContext || context.viewportPointerContext;
}

bool shouldUpdateThingPlacementModifierPreview(
    const ModifierPreviewContext& context,
    const int key,
    const bool thingPlacementActive) noexcept
{
    // Step 1: Thing placement mode responds to all editor modifier keys.
    if (!isEditorModifierKey(key))
    {
        return false;
    }

    // Step 2: Fly navigation owns modifier key handling while active.
    if (!thingPlacementActive || !context.viewportAvailable || context.flyNavigationActive)
    {
        return false;
    }

    // Step 3: The preview can update from either viewport focus or hover.
    return context.viewportFocusContext || context.viewportPointerContext;
}

bool isEditorSaveShortcutKey(
    const int key,
    const Qt::KeyboardModifiers modifiers) noexcept
{
    return key == Qt::Key_S
        && modifiers.testFlag(Qt::ControlModifier)
        && !modifiers.testFlag(Qt::AltModifier)
        && !modifiers.testFlag(Qt::MetaModifier);
}

bool shouldTriggerEditorSaveShortcut(
    const EditorSaveShortcutContext& context,
    const int key,
    const Qt::KeyboardModifiers modifiers) noexcept
{
    // Step 1: Only a real, non-repeated key press can trigger editor save.
    if (!context.keyEventAvailable || !context.pressed || context.autoRepeat)
    {
        return false;
    }

    // Step 2: Scope Ctrl+S handling to embedded COG script editors.
    if (!context.cogEditorFocusContext)
    {
        return false;
    }

    // Step 3: Reuse the editor-save shortcut allow-list.
    return isEditorSaveShortcutKey(key, modifiers);
}

bool shouldDispatchFlyNavigationShortcut(const FlyNavigationDispatchContext& context) noexcept
{
    // Step 1: Fly navigation key state only matters when the viewport exists and
    // the fly controller is active.
    if (!context.viewportAvailable || !context.flyNavigationActive)
    {
        return false;
    }

    // Step 2: Ignore auto-repeat and text-entry focus, but allow both key press
    // and key release so held movement state clears correctly.
    return context.keyEventAvailable
        && !context.autoRepeat
        && !context.textEntryFocusContext;
}

bool shouldDispatchThingEditShortcut(const ThingEditDispatchContext& context) noexcept
{
    // Step 1: Editor shortcuts are discrete key-press actions.
    if (!context.viewportAvailable || !context.keyEventAvailable || !context.pressed)
    {
        return false;
    }

    // Step 2: Scope them to non-repeated viewport shortcut events.
    return !context.autoRepeat && context.viewportShortcutContext;
}

bool shouldDispatchEditorShortcut(const EditorShortcutDispatchContext& context) noexcept
{
    return shouldDispatchThingEditShortcut(context);
}

bool shouldDispatchViewportRollShortcut(const ViewportRollDispatchContext& context) noexcept
{
    // Step 1: Roll shortcuts are discrete viewport key-press actions.
    if (!context.viewportAvailable || !context.keyEventAvailable || !context.pressed)
    {
        return false;
    }

    // Step 2: Ignore auto-repeat and keep the shortcut scoped to viewport input.
    return !context.autoRepeat && context.inViewport;
}

bool shortcutListMatchesKey(
    const QList<QKeySequence>& shortcuts,
    const int key,
    const Qt::KeyboardModifiers modifiers,
    const bool ignoreShiftModifier,
    const Qt::KeyboardModifiers ignoredHeldModifiers) noexcept
{
    const Qt::KeyboardModifiers maskedModifiers =
        ignoreShiftModifier ? (modifiers & ~Qt::ShiftModifier) : modifiers;
    auto matchesPressedSequence = [&](const int candidateKey, const Qt::KeyboardModifiers candidateModifiers) noexcept
    {
        for (const QKeySequence& sequence : shortcuts)
        {
            if (sequence.count() != 1)
            {
                continue;
            }
            const Qt::KeyboardModifiers required = Qt::KeyboardModifiers(
                int(sequence[0]) & int(Qt::KeyboardModifierMask));
            const int pressedSequence = candidateKey
                | (candidateModifiers & ~(ignoredHeldModifiers & ~required));
            if (sequence[0] == pressedSequence)
            {
                return true;
            }
        }
        return false;
    };

    // Step 1: Match the key exactly as Qt reported it.
    if (matchesPressedSequence(key, maskedModifiers))
    {
        return true;
    }

    const Qt::KeyboardModifiers shiftAwareModifiers = ignoreShiftModifier
        ? maskedModifiers
        : (maskedModifiers | Qt::ShiftModifier);

    // Step 2: Qt reports Shift+Tab as Backtab on several platforms.
    if (key == Qt::Key_Backtab && matchesPressedSequence(Qt::Key_Tab, shiftAwareModifiers))
    {
        return true;
    }

    // Step 3: Top-row Shift+number shortcuts often arrive as the produced
    // punctuation key. Normalize common US/EU layouts so Shift+1..4 work.
    if (modifiers.testFlag(Qt::ShiftModifier))
    {
        switch (key)
        {
            case Qt::Key_Exclam:
                return matchesPressedSequence(Qt::Key_1, shiftAwareModifiers);
            case Qt::Key_At:
            case Qt::Key_QuoteDbl:
                return matchesPressedSequence(Qt::Key_2, shiftAwareModifiers);
            case Qt::Key_NumberSign:
                return matchesPressedSequence(Qt::Key_3, shiftAwareModifiers);
            case Qt::Key_Dollar:
                return matchesPressedSequence(Qt::Key_4, shiftAwareModifiers);
            default:
                break;
        }
    }

    return false;
}

bool shortcutSettingMatchesFormerDefault(
    const QStringList& configured,
    const QList<QKeySequence>& formerDefault)
{
    const auto canonical = [](const QStringList& strings)
    {
        QSet<QString> sequences{};
        for (const QString& raw : strings)
        {
            const QKeySequence sequence(
                raw.trimmed(), QKeySequence::PortableText);
            if (!sequence.isEmpty())
            {
                sequences.insert(
                    sequence.toString(QKeySequence::PortableText));
            }
        }
        return sequences;
    };

    QStringList formerStrings{};
    formerStrings.reserve(formerDefault.size());
    for (const QKeySequence& sequence : formerDefault)
    {
        formerStrings.push_back(
            sequence.toString(QKeySequence::PortableText));
    }
    return canonical(configured) == canonical(formerStrings);
}

bool actionMatchesShortcut(
    const QAction* action,
    const int key,
    const Qt::KeyboardModifiers modifiers,
    const bool ignoreShiftModifier) noexcept
{
    return action
        && shortcutListMatchesKey(action->shortcuts(), key, modifiers, ignoreShiftModifier);
}

FlyNavigationShortcut flyNavigationShortcutForKey(
    const FlyNavigationShortcutBindings& bindings,
    const int key,
    const Qt::KeyboardModifiers modifiers) noexcept
{
    Qt::KeyboardModifiers speedModifiers = Qt::ShiftModifier;
    for (const auto* shortcuts : { &bindings.fast, &bindings.slow })
    {
        for (const QKeySequence& sequence : *shortcuts)
        {
            if (const auto hold = decodeModifierOnlyShortcut(sequence))
            {
                speedModifiers |= *hold;
            }
        }
    }
    const auto speedBindingMatches = [&](const QList<QKeySequence>& shortcuts)
    {
        const Qt::KeyboardModifier changedModifier = modifierForEditorKey(key);
        if (changedModifier != Qt::NoModifier)
        {
            const Qt::KeyboardModifiers held = modifiersWithKeyState(modifiers, key, true);
            for (const QKeySequence& sequence : shortcuts)
            {
                if (const auto hold = decodeModifierOnlyShortcut(sequence);
                    hold && hold->testFlag(changedModifier) && (held & *hold) == *hold)
                {
                    return true;
                }
            }
        }
        return shortcutListMatchesKey(shortcuts, key, modifiers, false, speedModifiers);
    };
    // Speed holds can coexist. Directional chords ignore their held modifier
    // bits while retaining modifiers explicitly required by a movement binding.
    if (speedBindingMatches(bindings.fast))
    {
        return FlyNavigationShortcut::fast;
    }
    if (speedBindingMatches(bindings.slow))
    {
        return FlyNavigationShortcut::slow;
    }

    // Persistent speed steps coexist with the temporary fast/slow holds.
    if (shortcutListMatchesKey(bindings.speedUp, key, modifiers, false, speedModifiers))
    {
        return FlyNavigationShortcut::speedUp;
    }
    if (shortcutListMatchesKey(bindings.speedDown, key, modifiers, false, speedModifiers))
    {
        return FlyNavigationShortcut::speedDown;
    }

    // Step 2: Resolve directional movement in the previous action-check order.
    if (shortcutListMatchesKey(bindings.forward, key, modifiers, false, speedModifiers))
    {
        return FlyNavigationShortcut::forward;
    }
    if (shortcutListMatchesKey(bindings.backward, key, modifiers, false, speedModifiers))
    {
        return FlyNavigationShortcut::backward;
    }
    if (shortcutListMatchesKey(bindings.left, key, modifiers, false, speedModifiers))
    {
        return FlyNavigationShortcut::left;
    }
    if (shortcutListMatchesKey(bindings.right, key, modifiers, false, speedModifiers))
    {
        return FlyNavigationShortcut::right;
    }
    if (shortcutListMatchesKey(bindings.down, key, modifiers, false, speedModifiers))
    {
        return FlyNavigationShortcut::down;
    }
    if (shortcutListMatchesKey(bindings.up, key, modifiers, false, speedModifiers))
    {
        return FlyNavigationShortcut::up;
    }

    return FlyNavigationShortcut::none;
}

bool shouldClaimFlyNavigationShortcutOverride(
    const FlyNavigationDispatchContext& context,
    const FlyNavigationShortcutBindings& bindings,
    const int key,
    const Qt::KeyboardModifiers modifiers) noexcept
{
    // ShortcutOverride is a routing decision, not a movement tick. Claim
    // auto-repeat overrides too so an overlapping QAction cannot fire midway
    // through a held fly-navigation chord.
    if (!context.viewportAvailable
        || !context.flyNavigationActive
        || !context.keyEventAvailable
        || context.textEntryFocusContext)
    {
        return false;
    }

    if (flyNavigationShortcutForKey(bindings, key, modifiers)
        != FlyNavigationShortcut::none)
    {
        return true;
    }
    const Qt::KeyboardModifiers held = modifiersWithKeyState(modifiers, key, true);
    for (const auto* shortcuts : { &bindings.fast, &bindings.slow })
    {
        for (const QKeySequence& sequence : *shortcuts)
        {
            if (const auto hold = decodeModifierOnlyShortcut(sequence);
                hold && *hold != Qt::NoModifier && (held & *hold) == *hold)
            {
                return true;
            }
        }
    }
    return false;
}

ThingEditShortcut thingEditShortcutForKey(
    const ThingEditShortcutBindings& bindings,
    const int key,
    const Qt::KeyboardModifiers modifiers) noexcept
{
    // Step 1: Tool shortcuts keep the previous MainWindow priority order.
    if (shortcutListMatchesKey(bindings.select, key, modifiers, false))
    {
        return ThingEditShortcut::select;
    }
    if (shortcutListMatchesKey(bindings.move, key, modifiers, false))
    {
        return ThingEditShortcut::move;
    }
    if (shortcutListMatchesKey(bindings.rotate, key, modifiers, false))
    {
        return ThingEditShortcut::rotate;
    }
    if (shortcutListMatchesKey(bindings.scale, key, modifiers, false))
    {
        return ThingEditShortcut::scale;
    }

    // Step 2: Orientation toggle is checked last so tool activation wins if a
    // user accidentally configures overlapping shortcuts.
    if (shortcutListMatchesKey(bindings.toggleOrientation, key, modifiers, false))
    {
        return ThingEditShortcut::toggleOrientation;
    }

    return ThingEditShortcut::none;
}

ViewportRollShortcut viewportRollShortcutForKey(
    const int key,
    const Qt::KeyboardModifiers modifiers,
    const bool inViewport) noexcept
{
    if (!inViewport || !modifiers.testFlag(Qt::ShiftModifier))
    {
        return ViewportRollShortcut::none;
    }

    constexpr Qt::KeyboardModifiers kBlockingModifiers =
        Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
    if ((modifiers & kBlockingModifiers) != Qt::NoModifier)
    {
        return ViewportRollShortcut::none;
    }

    switch (key)
    {
        case Qt::Key_Left:
        case Qt::Key_4:
            return ViewportRollShortcut::left;
        case Qt::Key_Right:
        case Qt::Key_6:
            return ViewportRollShortcut::right;
        case Qt::Key_Clear:
        case Qt::Key_5:
            return ViewportRollShortcut::reset;
        default:
            return ViewportRollShortcut::none;
    }
}

} // namespace sed::ui::shortcuts
