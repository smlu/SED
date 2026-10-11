#include "ui/components/viewport_camera_menu.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"

#include <QAction>
#include <QMouseEvent>
#include <QPainterPath>
#include <QRegion>
#include <QResizeEvent>
#include <QWidgetAction>

namespace sed::ui
{

ViewportCameraMenu::ViewportCameraMenu(QWidget* parent)
    : shortcuts::KeySequenceDisplayMenu(parent)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    setWindowFlag(Qt::FramelessWindowHint, true);
    setWindowFlag(Qt::NoDropShadowWindowHint, true);
}

ViewportCameraMenu::ViewportCameraMenu(const QString& title, QWidget* parent)
    : shortcuts::KeySequenceDisplayMenu(title, parent)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    setWindowFlag(Qt::FramelessWindowHint, true);
    setWindowFlag(Qt::NoDropShadowWindowHint, true);
}

void ViewportCameraMenu::mouseReleaseEvent(QMouseEvent* event)
{
    // Keep the menu open when interacting with embedded widget actions (spin boxes, edits, sliders).
    if (event)
    {
        if (const QAction* action = actionAt(event->pos()))
        {
            if (qobject_cast<const QWidgetAction*>(action) != nullptr)
            {
                event->ignore();
                return;
            }
        }
    }

    QMenu::mouseReleaseEvent(event);
}

void ViewportCameraMenu::initStyleOption(
    QStyleOptionMenuItem* const option, const QAction* const action) const
{
    shortcuts::KeySequenceDisplayMenu::initStyleOption(option, action);
}

void ViewportCameraMenu::resizeEvent(QResizeEvent* event)
{
    shortcuts::KeySequenceDisplayMenu::resizeEvent(event);

    // A QSS border-radius changes how QMenu paints but does not constrain its
    // native rectangular backing window on every platform/style.  Mask the
    // actual popup window so no opaque corner pixels can survive outside the
    // ten-pixel radius used by the viewport popup stylesheets.
    if (width() <= 0 || height() <= 0)
    {
        return;
    }

    constexpr qreal radius = 10.0;
    QPainterPath shape;
    shape.addRoundedRect(
        QRectF(0.0, 0.0, static_cast<qreal>(width()), static_cast<qreal>(height())),
        radius,
        radius);
    setMask(QRegion(shape.toFillPolygon().toPolygon()));
}

} // namespace sed::ui
