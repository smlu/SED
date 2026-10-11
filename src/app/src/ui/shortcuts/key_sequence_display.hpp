#pragma once

#include <QMenu>
#include <QKeySequence>
#include <QSize>
#include <QString>

class QMenuBar;
class QAction;
class QActionEvent;
class QEvent;
class QResizeEvent;
class QStyleOptionMenuItem;

namespace sed::ui::shortcuts
{

/** Formats a key sequence for readable UI display without changing its stored value. */
QString keySequenceDisplayText(const QKeySequence& sequence);

/** Rewrites only a visible menu item's shortcut suffix, keeping the action unchanged. */
void formatMenuShortcutOption(QStyleOptionMenuItem* option, const QAction* action);

/**
 * A menu that presents each action's shortcut using the application's
 * space-separated key-combination style while leaving QAction text and
 * bindings untouched.
 */
class KeySequenceDisplayMenu : public QMenu
{
public:
    explicit KeySequenceDisplayMenu(QWidget* parent = nullptr);
    KeySequenceDisplayMenu(const QString& title, QWidget* parent = nullptr);

    QSize sizeHint() const override;

protected:
    void initStyleOption(QStyleOptionMenuItem* option, const QAction* action) const override;
    void actionEvent(QActionEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct ShortcutColumnWidths
    {
        int native = 0;
        int formatted = 0;
    };

    [[nodiscard]] ShortcutColumnWidths shortcutColumnWidths() const;
    void prepareForNativeLayoutChange();
    void refreshActionLayoutIfNoResize(unsigned int serialBeforeInvalidation);

    mutable int callerMinimumWidth_ = 0;
    mutable int callerMaximumWidth_ = 0;
    mutable int appliedMinimumWidth_ = -1;
    mutable bool minimumWidthTrackingInitialized_ = false;
    mutable bool minimumWidthTemporarilyReset_ = false;
    mutable int nativeSizeHintWidth_ = 0;
    mutable bool nativeSizeHintWidthCached_ = false;
    mutable bool layoutSizingCompleted_ = false;
    unsigned int resizeEventSerial_ = 0;
};

/** Creates and adds a shortcut-formatted top-level menu. */
KeySequenceDisplayMenu* addKeySequenceDisplayMenu(
    QMenuBar* menuBar, const QString& title);

/** Creates and adds a shortcut-formatted submenu. */
KeySequenceDisplayMenu* addKeySequenceDisplayMenu(
    QMenu* parent, const QString& title);

} // namespace sed::ui::shortcuts
