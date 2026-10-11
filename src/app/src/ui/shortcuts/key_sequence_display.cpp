#include "ui/shortcuts/key_sequence_display.hpp"

#include <QAction>
#include <QActionEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QKeyCombination>
#include <QMenuBar>
#include <QResizeEvent>
#include <QStyleOptionMenuItem>
#include <QWidgetAction>

#include <algorithm>

namespace sed::ui::shortcuts
{

namespace
{

QString keyStrokeDisplayText(const QKeyCombination combination)
{
    Qt::KeyboardModifiers modifiers = combination.keyboardModifiers();
    int key = combination.key();

    // Modifier-only QKeySequences encode their final modifier as the stroke's
    // key. Fold it into the same stable modifier order used by ordinary keys.
    switch ( key )
    {
        case Qt::Key_Control:
            modifiers |= Qt::ControlModifier;
            key = 0;
            break;
        case Qt::Key_Shift:
            modifiers |= Qt::ShiftModifier;
            key = 0;
            break;
        case Qt::Key_Alt:
            modifiers |= Qt::AltModifier;
            key = 0;
            break;
        case Qt::Key_Meta:
            modifiers |= Qt::MetaModifier;
            key = 0;
            break;
        default:
            break;
    }

    QStringList parts{};
    if ( modifiers.testFlag(Qt::ControlModifier) )
    {
        parts.push_back(QStringLiteral("Ctrl"));
    }
    if ( modifiers.testFlag(Qt::ShiftModifier) )
    {
        parts.push_back(QStringLiteral("Shift"));
    }
    if ( modifiers.testFlag(Qt::AltModifier) )
    {
        parts.push_back(QStringLiteral("Alt"));
    }
    if ( modifiers.testFlag(Qt::MetaModifier) )
    {
        parts.push_back(QStringLiteral("Meta"));
    }
    if ( modifiers.testFlag(Qt::KeypadModifier) )
    {
        parts.push_back(QStringLiteral("Numpad"));
    }

    if ( key != 0 && key != Qt::Key_unknown )
    {
        // A plus is a real key when it is the stroke's key. Do not confuse it
        // with the separators QKeySequence::toString() uses between keys.
        const QString keyText = key == Qt::Key_Plus ? QStringLiteral("+")
            : key == Qt::Key_Comma ? QStringLiteral("Comma")
            : QKeySequence(key).toString(QKeySequence::NativeText);
        if ( !keyText.isEmpty() )
        {
            parts.push_back(keyText);
        }
    }

    return parts.join(QLatin1Char(' '));
}

} // namespace

QString keySequenceDisplayText(const QKeySequence& sequence)
{
    if ( sequence.isEmpty() )
    {
        return {};
    }

    QStringList strokes{};
    strokes.reserve(sequence.count());
    for ( int index = 0; index < sequence.count(); ++index )
    {
        const QString stroke = keyStrokeDisplayText(sequence[index]);
        if ( !stroke.isEmpty() )
        {
            strokes.push_back(stroke);
        }
    }
    return strokes.join(QStringLiteral(", "));
}

void formatMenuShortcutOption(
    QStyleOptionMenuItem* const option, const QAction* const action)
{
    if ( !option || !action )
    {
        return;
    }

    const int tab = option->text.indexOf(QLatin1Char('\t'));
    if ( tab < 0 )
    {
        return;
    }

    // Some native text-control context actions expose their accelerator only
    // as the visible tab suffix, with no QAction shortcut. Parse that suffix
    // only when present so hidden shortcuts stay hidden.
    const QString nativeSuffix = option->text.mid(tab + 1);
    const QKeySequence shortcut = action->shortcut().isEmpty()
        ? QKeySequence(nativeSuffix, QKeySequence::NativeText)
        : action->shortcut();
    const QString display = keySequenceDisplayText(shortcut);
    if ( display.isEmpty() )
    {
        return;
    }

    option->text = option->text.left(tab) + QLatin1Char('\t') + display;
    option->reservedShortcutWidth = std::max(
        option->reservedShortcutWidth,
        option->fontMetrics.horizontalAdvance(display));
}

KeySequenceDisplayMenu::KeySequenceDisplayMenu(QWidget* const parent)
    : QMenu(parent)
{
}

KeySequenceDisplayMenu::KeySequenceDisplayMenu(
    const QString& title, QWidget* const parent)
    : QMenu(title, parent)
{
}

QSize KeySequenceDisplayMenu::sizeHint() const
{
    auto* const self = const_cast<KeySequenceDisplayMenu*>(this);
    const unsigned int resizeSerialAtEntry = resizeEventSerial_;
    const int currentMinimumWidth = minimumWidth();
    const int currentMaximumWidth = maximumWidth();
    if ( currentMinimumWidth == currentMaximumWidth )
    {
        // A caller-specified fixed width is an explicit layout constraint.
        callerMinimumWidth_ = currentMinimumWidth;
        callerMaximumWidth_ = currentMaximumWidth;
        appliedMinimumWidth_ = -1;
        minimumWidthTrackingInitialized_ = true;
        minimumWidthTemporarilyReset_ = false;
        nativeSizeHintWidthCached_ = false;
        layoutSizingCompleted_ = true;
        QSize hint = QMenu::sizeHint();
        hint.setWidth(std::min(hint.width(), callerMaximumWidth_));
        return hint;
    }

    const bool minimumWidthIsOurs = minimumWidthTrackingInitialized_
        && appliedMinimumWidth_ >= 0
        && currentMinimumWidth == appliedMinimumWidth_;
    const bool callerMaximumChanged = minimumWidthTrackingInitialized_
        && currentMaximumWidth != callerMaximumWidth_;
    const bool callerMinimumChanged = minimumWidthTrackingInitialized_
        && !minimumWidthTemporarilyReset_ && !minimumWidthIsOurs
        && currentMinimumWidth != callerMinimumWidth_;
    const bool callerBoundsChanged = callerMaximumChanged || callerMinimumChanged;
    if ( !minimumWidthTrackingInitialized_ )
    {
        callerMinimumWidth_ = currentMinimumWidth;
        callerMaximumWidth_ = currentMaximumWidth;
        minimumWidthTrackingInitialized_ = true;
    }
    else
    {
        if ( callerMaximumChanged )
        {
            callerMaximumWidth_ = currentMaximumWidth;
        }
        if ( callerMinimumChanged )
        {
            callerMinimumWidth_ = currentMinimumWidth;
        }
    }

    const ShortcutColumnWidths columns = shortcutColumnWidths();
    const int extraShortcutWidth = std::max(0, columns.formatted - columns.native);
    if ( extraShortcutWidth <= 0 )
    {
        if ( minimumWidthTemporarilyReset_ || minimumWidthIsOurs )
        {
            self->setMinimumWidth(callerMinimumWidth_);
        }
        appliedMinimumWidth_ = -1;
        minimumWidthTrackingInitialized_ = true;
        minimumWidthTemporarilyReset_ = false;
        nativeSizeHintWidthCached_ = false;
        layoutSizingCompleted_ = true;
        QSize hint = QMenu::sizeHint();
        hint.setWidth(std::min(hint.width(), callerMaximumWidth_));
        return hint;
    }

    if ( !nativeSizeHintWidthCached_ )
    {
        if ( minimumWidth() != 0 )
        {
            self->setMinimumWidth(0);
            minimumWidthTemporarilyReset_ = true;
        }
        nativeSizeHintWidth_ = QMenu::sizeHint().width();
        nativeSizeHintWidthCached_ = true;
    }

    const int requiredMinimumWidth = std::min(
        callerMaximumWidth_, std::max(
            callerMinimumWidth_, nativeSizeHintWidth_ + extraShortcutWidth));
    if ( minimumWidth() != requiredMinimumWidth )
    {
        // QMenu lays out action rectangles from minimumWidth(), not only from
        // the sizeHint returned to the popup window. Applying this temporary
        // minimum ensures the formatted suffix column and caption cannot
        // overlap inside a naturally sized menu.
        self->setMinimumWidth(requiredMinimumWidth);
    }
    appliedMinimumWidth_ = requiredMinimumWidth;
    minimumWidthTrackingInitialized_ = true;
    minimumWidthTemporarilyReset_ = false;
    layoutSizingCompleted_ = true;

    if ( callerBoundsChanged )
    {
        auto* const mutableSelf = const_cast<KeySequenceDisplayMenu*>(this);
        mutableSelf->refreshActionLayoutIfNoResize(resizeSerialAtEntry);
    }

    QSize hint = QMenu::sizeHint();
    hint.setWidth(requiredMinimumWidth);
    return hint;
}

KeySequenceDisplayMenu::ShortcutColumnWidths
KeySequenceDisplayMenu::shortcutColumnWidths() const
{
    const QFontMetrics metrics(font());
    ShortcutColumnWidths widths{};
    for ( const QAction* const action : actions() )
    {
        if ( !action || !action->isVisible() || action->isSeparator()
            || qobject_cast<const QWidgetAction*>(action) != nullptr)
        {
            continue;
        }

        QStyleOptionMenuItem nativeOption{};
        QMenu::initStyleOption(&nativeOption, action);
        const int tab = nativeOption.text.indexOf(QLatin1Char('\t'));
        if ( tab < 0 )
        {
            continue;
        }

        widths.native = std::max(
            widths.native,
            metrics.horizontalAdvance(nativeOption.text.mid(tab + 1)));

        QStyleOptionMenuItem displayOption = nativeOption;
        formatMenuShortcutOption(&displayOption, action);
        const int displayTab = displayOption.text.indexOf(QLatin1Char('\t'));
        if ( displayTab >= 0 )
        {
            widths.formatted = std::max(
                widths.formatted,
                displayOption.fontMetrics.horizontalAdvance(
                    displayOption.text.mid(displayTab + 1)));
        }
    }
    return widths;
}

void KeySequenceDisplayMenu::prepareForNativeLayoutChange()
{
    const int currentMinimumWidth = minimumWidth();
    if ( currentMinimumWidth == maximumWidth() )
    {
        callerMinimumWidth_ = currentMinimumWidth;
        callerMaximumWidth_ = maximumWidth();
        appliedMinimumWidth_ = -1;
        minimumWidthTrackingInitialized_ = true;
        minimumWidthTemporarilyReset_ = false;
    }
    else
    {
        if ( !minimumWidthTrackingInitialized_ )
        {
            callerMinimumWidth_ = currentMinimumWidth;
            callerMaximumWidth_ = maximumWidth();
            minimumWidthTrackingInitialized_ = true;
        }
        const bool minimumWidthIsOurs = minimumWidthTrackingInitialized_
            && appliedMinimumWidth_ >= 0
            && currentMinimumWidth == appliedMinimumWidth_;
        if ( !minimumWidthTemporarilyReset_ && !minimumWidthIsOurs )
        {
            callerMinimumWidth_ = currentMinimumWidth;
        }
        if ( currentMinimumWidth != 0 )
        {
            setMinimumWidth(0);
        }
        minimumWidthTemporarilyReset_ = true;
        appliedMinimumWidth_ = -1;
    }
    nativeSizeHintWidthCached_ = false;
    layoutSizingCompleted_ = false;
}

void KeySequenceDisplayMenu::initStyleOption(
    QStyleOptionMenuItem* const option, const QAction* const action) const
{
    QMenu::initStyleOption(option, action);
    // QStyleOptionMenuItem carries the suffix in its text. QSS menu rendering
    // paints that tab-delimited field directly, so keep the formatting at the
    // style-option boundary and leave QAction captions and bindings untouched.
    formatMenuShortcutOption(option, action);
}

void KeySequenceDisplayMenu::actionEvent(QActionEvent* const event)
{
    prepareForNativeLayoutChange();
    const unsigned int serialBeforeInvalidation = resizeEventSerial_;
    QMenu::actionEvent(event);
    if ( minimumWidth() != maximumWidth() && !layoutSizingCompleted_ )
    {
        (void)sizeHint();
    }
    refreshActionLayoutIfNoResize(serialBeforeInvalidation);
}

void KeySequenceDisplayMenu::changeEvent(QEvent* const event)
{
    const bool affectsShortcutMetrics = event
        && (event->type() == QEvent::FontChange
            || event->type() == QEvent::ApplicationFontChange
            || event->type() == QEvent::StyleChange);
    if ( affectsShortcutMetrics )
    {
        prepareForNativeLayoutChange();
    }
    const unsigned int serialBeforeInvalidation = resizeEventSerial_;
    QMenu::changeEvent(event);
    if ( affectsShortcutMetrics && minimumWidth() != maximumWidth()
        && !layoutSizingCompleted_ )
    {
        (void)sizeHint();
    }
    if ( affectsShortcutMetrics )
    {
        refreshActionLayoutIfNoResize(serialBeforeInvalidation);
    }
}

void KeySequenceDisplayMenu::resizeEvent(QResizeEvent* const event)
{
    ++resizeEventSerial_;
    QMenu::resizeEvent(event);
}

void KeySequenceDisplayMenu::refreshActionLayoutIfNoResize(
    const unsigned int serialBeforeInvalidation)
{
    if ( resizeEventSerial_ != serialBeforeInvalidation
        || !nativeSizeHintWidthCached_ || appliedMinimumWidth_ < 0 )
    {
        return;
    }

    const QSize originalSize = size();
    if ( width() < minimumWidth() || width() > maximumWidth()
        || height() < minimumHeight() || height() > maximumHeight() )
    {
        return;
    }

    QSize temporarySize = originalSize;
    if ( width() < maximumWidth() )
    {
        temporarySize.rwidth() += 1;
    }
    else if ( width() > minimumWidth() )
    {
        temporarySize.rwidth() -= 1;
    }
    else if ( height() < maximumHeight() )
    {
        temporarySize.rheight() += 1;
    }
    else if ( height() > minimumHeight() )
    {
        temporarySize.rheight() -= 1;
    }

    if ( temporarySize == originalSize )
    {
        return;
    }

    const bool wasUpdatesEnabled = updatesEnabled();
    const unsigned int serialBeforeRefresh = resizeEventSerial_;
    setUpdatesEnabled(false);
    resize(temporarySize);
    resize(originalSize);
    setUpdatesEnabled(wasUpdatesEnabled);
    if ( resizeEventSerial_ == serialBeforeRefresh )
    {
        return;
    }

    // QMenu derives action rectangles from the minimum width. The real resize
    // pair dirties its layout cache; querying the base hint rebuilds it after
    // the original geometry and corrected minimum have both been restored.
    (void)QMenu::sizeHint();
}

KeySequenceDisplayMenu* addKeySequenceDisplayMenu(
    QMenuBar* const menuBar, const QString& title)
{
    if ( !menuBar )
    {
        return nullptr;
    }
    auto* const menu = new KeySequenceDisplayMenu(title, menuBar);
    menuBar->addMenu(menu);
    return menu;
}

KeySequenceDisplayMenu* addKeySequenceDisplayMenu(
    QMenu* const parent, const QString& title)
{
    if ( !parent )
    {
        return nullptr;
    }
    auto* const menu = new KeySequenceDisplayMenu(title, parent);
    parent->addMenu(menu);
    return menu;
}

} // namespace sed::ui::shortcuts
