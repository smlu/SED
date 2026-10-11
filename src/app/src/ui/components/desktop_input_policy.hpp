#pragma once

#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QLineEdit>
#include <QScrollBar>
#include <QWidget>
#include <QWheelEvent>

namespace sed::ui
{

/**
 * Removes Qt's WheelFocus bit from value editors.  Otherwise the first wheel
 * event under the pointer may focus a spin/combo before application-level
 * wheel routing gets a chance to decide whether the editor is active.
 * StrongFocus retains normal keyboard-tab and click-to-edit behavior.
 */
inline void normalizeWheelEditorFocusPolicy(QObject* watched) noexcept
{
    auto* widget = qobject_cast<QWidget*>(watched);
    for (QWidget* current = widget; current; current = current->parentWidget())
    {
        const bool valueSlider = qobject_cast<QAbstractSlider*>(current)
            && !qobject_cast<QScrollBar*>(current);
        if (qobject_cast<QComboBox*>(current)
            || qobject_cast<QAbstractSpinBox*>(current)
            || valueSlider)
        {
            if (current->focusPolicy() == Qt::WheelFocus)
            {
                current->setFocusPolicy(Qt::StrongFocus);
            }
            return;
        }
    }
}

/**
 * Finds the value editor which would consume a wheel event. Qt changes combo,
 * spin, and slider values merely because the pointer happens to be over them;
 * that is particularly surprising inside a scrollable property panel.
 */
inline QWidget* wheelValueEditor(QObject* watched) noexcept
{
    auto* widget = qobject_cast<QWidget*>(watched);
    // Keep the composite-editor walk deterministic.  An editable combo box
    // owns a QLineEdit child, but it is still a combo box for wheel policy:
    // the child must not bypass the "never cycle by wheel" rule below.
    QWidget* editableLineEdit = nullptr;
    for (QWidget* current = widget; current; current = current->parentWidget())
    {
        // A combo popup is a scrollable list, not the closed value editor.
        // Some Qt styles parent the popup container back under the combo;
        // the explicit marker keeps application-wide wheel suppression from
        // swallowing ordinary list navigation in that configuration.
        if (current->property("sed.comboPopupWheel").toBool())
        {
            return nullptr;
        }
        const bool valueSlider = qobject_cast<QAbstractSlider*>(current)
            && !qobject_cast<QScrollBar*>(current);
        if (qobject_cast<QComboBox*>(current))
        {
            return current;
        }
        if (qobject_cast<QAbstractSpinBox*>(current) || valueSlider)
        {
            return current;
        }
        // Composite editors such as ScrubFloatEdit expose a child line edit
        // instead of QAbstractSpinBox.  Treat it as the value editor too so a
        // wheel over an unfocused text/value field scrolls its containing
        // panel rather than changing a value or being swallowed by the child.
        if (const auto* lineEdit = qobject_cast<QLineEdit*>(current))
        {
            if (!lineEdit->isReadOnly())
            {
                editableLineEdit = current;
            }
        }
    }
    return editableLineEdit;
}

inline bool widgetOwnsFocus(const QWidget* widget) noexcept
{
    const QWidget* focus = QApplication::focusWidget();
    return widget && focus && (focus == widget || widget->isAncestorOf(focus));
}

/**
 * Combo boxes are never wheel-selectable unless a focused, purpose-built
 * control explicitly sets `sed.allowFocusedWheel`; a pointer passing over an
 * ordinary dropdown must not silently change the chosen enum. Other value
 * editors (spin boxes, scrub fields, and sliders) are blocked only while
 * unfocused. When blocked and embedded in a scroll area, the same wheel delta
 * is forwarded to that scroll area so scrolling a property/list panel
 * continues naturally.
 */
inline bool routeUnfocusedEditorWheel(QObject* watched, QEvent* event)
{
    if (!event || event->type() != QEvent::Wheel)
    {
        return false;
    }

    normalizeWheelEditorFocusPolicy(watched);
    QWidget* editor = wheelValueEditor(watched);
    if (!editor)
    {
        return false;
    }

    if ( qobject_cast<QAbstractSpinBox*>(editor)
        && editor->property("sedSpinWheelEnabled").toBool() )
    {
        return false;
    }

    const bool comboBox = qobject_cast<QComboBox*>(editor) != nullptr;
    const bool focusedWheelOptIn =
        editor->property("sed.allowFocusedWheel").toBool();
    if ((!comboBox || focusedWheelOptIn) && widgetOwnsFocus(editor))
    {
        return false;
    }

    QAbstractScrollArea* scrollArea = nullptr;
    for (QWidget* current = editor->parentWidget(); current; current = current->parentWidget())
    {
        if ((scrollArea = qobject_cast<QAbstractScrollArea*>(current)))
        {
            break;
        }
    }

    auto* wheel = static_cast<QWheelEvent*>(event);
    if (scrollArea && scrollArea->viewport())
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPointF global = wheel->globalPosition();
#else
        const QPointF global = wheel->globalPosF();
#endif
        const QPointF local = scrollArea->viewport()->mapFromGlobal(global.toPoint());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QWheelEvent forwarded(
            local,
            global,
            wheel->pixelDelta(),
            wheel->angleDelta(),
            wheel->buttons(),
            wheel->modifiers(),
            wheel->phase(),
            wheel->inverted(),
            wheel->source());
#else
        // Qt 5 exposes the same high-resolution constructor contract but uses
        // the pre-QSinglePointEvent position accessors above.
        QWheelEvent forwarded(
            local,
            global,
            wheel->pixelDelta(),
            wheel->angleDelta(),
            wheel->buttons(),
            wheel->modifiers(),
            wheel->phase(),
            wheel->inverted(),
            wheel->source());
#endif
        QApplication::sendEvent(scrollArea->viewport(), &forwarded);
    }

    event->accept();
    return true;
}

} // namespace sed::ui
