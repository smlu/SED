#include "ui/components/viewport_icons.hpp"

#include "render/gizmos/gizmo_icon_utils.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include <QLinearGradient>
#include <QAction>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QPixmap>
#include <QPolygon>
#include <QRadialGradient>
#include <QStringList>
#include <QToolButton>

namespace sed::ui
{
namespace
{

QPixmap makeDisabledViewportIconPixmap(const QPixmap& source)
{
    QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() == 0)
            {
                continue;
            }

            const int alpha = static_cast<int>(std::lround(static_cast<double>(pixel.alpha()) * 0.34));
            pixel.setRgb(84, 88, 102, std::clamp(alpha, 0, 110));
            image.setPixelColor(x, y, pixel);
        }
    }

    return QPixmap::fromImage(image);
}

void drawIconGrid(QPainter& painter, const QRectF& bounds, const QColor& color,
    const bool perspective = false)
{
    painter.save();
    QPen gridPen(color);
    gridPen.setWidthF(1.7);
    gridPen.setCapStyle(Qt::RoundCap);
    gridPen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(gridPen);
    painter.setBrush(Qt::NoBrush);

    if (!perspective)
    {
        painter.drawRoundedRect(bounds, 1.5, 1.5);
        for (const qreal f : { 1.0 / 3.0, 2.0 / 3.0 })
        {
            painter.drawLine(QPointF(bounds.left() + bounds.width() * f, bounds.top()),
                QPointF(bounds.left() + bounds.width() * f, bounds.bottom()));
            painter.drawLine(QPointF(bounds.left(), bounds.top() + bounds.height() * f),
                QPointF(bounds.right(), bounds.top() + bounds.height() * f));
        }
    }
    else
    {
        const QPolygonF plane{
            QPointF(bounds.left() + bounds.width() * 0.18, bounds.top()),
            QPointF(bounds.right() - bounds.width() * 0.18, bounds.top()),
            QPointF(bounds.right(), bounds.bottom()),
            QPointF(bounds.left(), bounds.bottom()),
        };
        painter.drawPolygon(plane);
        painter.drawLine(QPointF(bounds.center().x(), bounds.top()),
            QPointF(bounds.center().x(), bounds.bottom()));
        painter.drawLine(QPointF(bounds.left() + bounds.width() * 0.09, bounds.center().y()),
            QPointF(bounds.right() - bounds.width() * 0.09, bounds.center().y()));
    }
    painter.restore();
}

void drawIconCube(QPainter& painter, const QRectF& bounds, const QColor& color,
    const bool filled = false)
{
    painter.save();
    const QPointF top(bounds.center().x(), bounds.top());
    const QPointF left(bounds.left(), bounds.top() + bounds.height() * 0.30);
    const QPointF center(bounds.center().x(), bounds.top() + bounds.height() * 0.48);
    const QPointF right(bounds.right(), bounds.top() + bounds.height() * 0.30);
    const QPointF lowerLeft(bounds.left(), bounds.top() + bounds.height() * 0.70);
    const QPointF bottom(bounds.center().x(), bounds.bottom());
    const QPointF lowerRight(bounds.right(), bounds.top() + bounds.height() * 0.70);
    QPen cubePen(color);
    cubePen.setWidthF(1.8);
    cubePen.setCapStyle(Qt::RoundCap);
    cubePen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(cubePen);
    if (filled)
    {
        painter.setBrush(QColor(color.red(), color.green(), color.blue(), 55));
        painter.drawPolygon(QPolygonF{ top, right, center, left });
        painter.drawPolygon(QPolygonF{ left, center, bottom, lowerLeft });
        painter.drawPolygon(QPolygonF{ center, right, lowerRight, bottom });
    }
    painter.setBrush(Qt::NoBrush);
    painter.drawPolygon(QPolygonF{ top, right, lowerRight, bottom, lowerLeft, left });
    painter.drawLine(top, center);
    painter.drawLine(left, center);
    painter.drawLine(right, center);
    painter.drawLine(center, bottom);
    painter.restore();
}

void drawLightCategoryGlyph(QPainter& painter, const int size, const ViewportIcon icon)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QPointF center(size * 0.5, size * 0.5);
    if (icon == ViewportIcon::lightThing)
    {
        painter.drawImage(
            QPointF(0.0, 0.0),
            render::renderThingRegularLightIconImage(QColor(82, 205, 244), size));
        painter.restore();
        return;
    }
    if (icon == ViewportIcon::lightHeadlight)
    {
        painter.drawImage(
            QPointF(0.0, 0.0),
            render::renderThingHeadlightIconImage(QColor(255, 166, 86), size));
        painter.restore();
        return;
    }
    if (icon == ViewportIcon::lightSector)
    {
        painter.drawImage(
            QPointF(0.0, 0.0),
            render::renderSectorPointLightIconImage(QColor(218, 112, 255), size));
        painter.restore();
        return;
    }
    if (icon == ViewportIcon::lightArchitectural)
    {
        painter.drawImage(
            QPointF(0.0, 0.0),
            render::renderArchitecturalLightingIconImage(QColor(255, 211, 82), size));
        painter.restore();
        return;
    }

    const QColor accent(255, 211, 82);
    constexpr int rayCount = 8;
    const qreal innerRadius = size * 0.21;
    const qreal outerRadius = size * 0.39;

    QPen outline(QColor(13, 15, 20, 255), size * 0.105);
    outline.setCapStyle(Qt::RoundCap);
    outline.setJoinStyle(Qt::RoundJoin);
    QPen accentPen(accent, size * 0.060);
    accentPen.setCapStyle(Qt::RoundCap);
    accentPen.setJoinStyle(Qt::RoundJoin);

    for (int pass = 0; pass < 2; ++pass)
    {
        painter.setPen(pass == 0 ? outline : accentPen);
        for (int i = 0; i < rayCount; ++i)
        {
            const double angle = (static_cast<double>(i) / static_cast<double>(rayCount))
                * (2.0 * std::numbers::pi_v<double>);
            const QPointF direction(std::cos(angle), std::sin(angle));
            painter.drawLine(center + direction * innerRadius, center + direction * outerRadius);
        }
    }

    painter.setPen(outline);
    painter.setBrush(QColor(18, 20, 27, 255));
    painter.drawEllipse(center, size * 0.19, size * 0.19);
    painter.setPen(accentPen);
    painter.setBrush(accent);
    painter.drawEllipse(center, size * 0.19, size * 0.19);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 249, 216));
    painter.drawEllipse(center, size * 0.075, size * 0.075);
    painter.restore();
}

} // namespace

QIcon makeViewportIcon(const ViewportIcon icon)
{
    constexpr int size = 32;

    QPixmap pix(size, size);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor fg(235, 236, 240);
    const QColor fill(235, 236, 240, 80);

    QPen pen(fg);
    pen.setWidthF(std::max<qreal>(2.0, static_cast<qreal>(size) / 10.0));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);

    switch (icon)
    {
        case ViewportIcon::vertices:
        {
            p.setPen(Qt::NoPen);
            p.setBrush(fg);

            p.drawEllipse(QPoint(5, 5), 2, 2);
            p.drawEllipse(QPoint(size - 6, 5), 2, 2);
            p.drawEllipse(QPoint(5, size - 6), 2, 2);
            p.drawEllipse(QPoint(size - 6, size - 6), 2, 2);
            p.drawEllipse(QPoint(size / 2, size / 2), 1, 1);
            break;
        }
        case ViewportIcon::visibilityEye:
        {
            QPainterPath eye;
            eye.moveTo(size * 0.10, size * 0.50);
            eye.cubicTo(
                size * 0.28, size * 0.22,
                size * 0.72, size * 0.22,
                size * 0.90, size * 0.50);
            eye.cubicTo(
                size * 0.72, size * 0.78,
                size * 0.28, size * 0.78,
                size * 0.10, size * 0.50);
            QPen eyePen(fg);
            eyePen.setWidthF(2.2);
            eyePen.setCapStyle(Qt::RoundCap);
            eyePen.setJoinStyle(Qt::RoundJoin);
            p.setPen(eyePen);
            p.setBrush(QColor(fg.red(), fg.green(), fg.blue(), 38));
            p.drawPath(eye);
            p.setBrush(QColor(105, 160, 255));
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(size * 0.50, size * 0.50), size * 0.105, size * 0.105);
            break;
        }
        case ViewportIcon::cleaveAngle:
        {
            const QPointF vertex(size * 0.12, size * 0.87);
            const QPointF horizontal(size * 0.90, size * 0.87);
            const QPointF rising(size * 0.12, size * 0.10);
            p.setBrush(Qt::NoBrush);
            p.drawLine(vertex, horizontal);
            p.drawLine(vertex, rising);

            QPen arcPen(QColor(105, 160, 255));
            arcPen.setWidthF(2.6);
            arcPen.setCapStyle(Qt::RoundCap);
            arcPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(arcPen);
            QPainterPath arc;
            arc.moveTo(size * 0.46, size * 0.87);
            arc.cubicTo(
                size * 0.46, size * 0.66,
                size * 0.33, size * 0.53,
                size * 0.12, size * 0.53);
            p.drawPath(arc);
            p.setPen(Qt::NoPen);
            p.setBrush(fg);
            p.drawEllipse(vertex, size * 0.055, size * 0.055);
            break;
        }
        case ViewportIcon::cleaveLength:
        {
            const qreal left = size * 0.10;
            const qreal right = size * 0.90;
            const qreal axisY = size * 0.50;
            const QColor accent(255, 196, 96);
            QPen dimensionPen(fg);
            dimensionPen.setWidthF(2.1);
            dimensionPen.setCapStyle(Qt::RoundCap);
            dimensionPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(dimensionPen);
            p.drawLine(QPointF(left, size * 0.14), QPointF(left, size * 0.86));
            p.drawLine(QPointF(right, size * 0.14), QPointF(right, size * 0.86));

            QPen measurePen(accent);
            measurePen.setWidthF(2.5);
            measurePen.setCapStyle(Qt::RoundCap);
            measurePen.setJoinStyle(Qt::RoundJoin);
            p.setPen(measurePen);
            p.drawLine(QPointF(left + size * 0.06, axisY),
                QPointF(right - size * 0.06, axisY));
            p.drawLine(QPointF(left + size * 0.06, axisY),
                QPointF(left + size * 0.20, axisY - size * 0.12));
            p.drawLine(QPointF(left + size * 0.06, axisY),
                QPointF(left + size * 0.20, axisY + size * 0.12));
            p.drawLine(QPointF(right - size * 0.06, axisY),
                QPointF(right - size * 0.20, axisY - size * 0.12));
            p.drawLine(QPointF(right - size * 0.06, axisY),
                QPointF(right - size * 0.20, axisY + size * 0.12));
            break;
        }
        case ViewportIcon::cleaveLineNumber:
        {
            const QColor accent(102, 211, 191);
            QPen linePen(fg);
            linePen.setWidthF(2.0);
            linePen.setCapStyle(Qt::RoundCap);
            linePen.setJoinStyle(Qt::RoundJoin);
            p.setPen(linePen);
            p.setBrush(Qt::NoBrush);
            p.drawLine(QPointF(size * 0.12, size * 0.78),
                QPointF(size * 0.88, size * 0.22));
            p.setPen(Qt::NoPen);
            p.setBrush(accent);
            p.drawEllipse(QPointF(size * 0.12, size * 0.78), size * 0.07, size * 0.07);
            p.drawEllipse(QPointF(size * 0.88, size * 0.22), size * 0.07, size * 0.07);

            p.setPen(QPen(fg, 1.8));
            p.setBrush(QColor(28, 31, 39));
            p.drawEllipse(QPointF(size * 0.50, size * 0.50), size * 0.30, size * 0.30);
            QFont numberFont = p.font();
            numberFont.setBold(true);
            numberFont.setPixelSize(17);
            p.setFont(numberFont);
            p.setPen(accent);
            p.drawText(QRectF(size * 0.30, size * 0.26, size * 0.40, size * 0.48),
                Qt::AlignCenter, QStringLiteral("1"));
            break;
        }
        case ViewportIcon::cleaveAddShape:
        {
            QPen shapePen(fg, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(shapePen);
            p.setBrush(QColor(fg.red(), fg.green(), fg.blue(), 38));
            p.drawPolygon(QPolygonF{
                QPointF(size * 0.39, size * 0.10),
                QPointF(size * 0.70, size * 0.33),
                QPointF(size * 0.59, size * 0.80),
                QPointF(size * 0.19, size * 0.80),
                QPointF(size * 0.08, size * 0.33),
            });
            const QPointF plusCenter(size * 0.78, size * 0.72);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(18, 20, 27));
            p.drawEllipse(plusCenter, size * 0.21, size * 0.21);
            QPen plusPen(QColor(105, 160, 255), 3.0,
                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(plusPen);
            p.drawLine(plusCenter - QPointF(size * 0.14, 0),
                plusCenter + QPointF(size * 0.14, 0));
            p.drawLine(plusCenter - QPointF(0, size * 0.14),
                plusCenter + QPointF(0, size * 0.14));
            break;
        }
        case ViewportIcon::layerLocked:
        {
            QPen lockPen(fg);
            lockPen.setWidthF(2.4);
            lockPen.setCapStyle(Qt::RoundCap);
            lockPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(lockPen);
            p.setBrush(Qt::NoBrush);
            p.drawArc(
                QRectF(size * 0.28, size * 0.10, size * 0.44, size * 0.48),
                0,
                180 * 16);
            p.setBrush(QColor(235, 196, 96, 105));
            p.drawRoundedRect(
                QRectF(size * 0.18, size * 0.43, size * 0.64, size * 0.44),
                size * 0.07,
                size * 0.07);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(235, 196, 96));
            p.drawEllipse(QPointF(size * 0.50, size * 0.62), size * 0.055, size * 0.055);
            p.drawRoundedRect(
                QRectF(size * 0.475, size * 0.62, size * 0.05, size * 0.13),
                size * 0.02,
                size * 0.02);
            break;
        }
        case ViewportIcon::wireframe:
        {
            const QRect r(3, 3, size - 6, size - 6);
            const QRect inner(6, 6, size - 9, size - 9);
            p.setBrush(Qt::NoBrush);
            p.drawRect(r);
            p.drawRect(inner);
            p.drawLine(r.topLeft(), inner.topLeft());
            p.drawLine(r.topRight(), inner.topRight());
            p.drawLine(r.bottomLeft(), inner.bottomLeft());
            p.drawLine(r.bottomRight(), inner.bottomRight());
            break;
        }
        case ViewportIcon::solid:
        {
            const QRect r(3, 3, size - 6, size - 6);
            p.setBrush(fill);
            p.drawRoundedRect(r, 3, 3);
            break;
        }
        case ViewportIcon::textured:
        {
            const QRect r(3, 3, size - 6, size - 6);
            const int half = (size - 6) / 2;
            const QRect a(3, 3, half, half);
            const QRect b(3 + half, 3, half, half);
            const QRect c(3, 3 + half, half, half);
            const QRect d(3 + half, 3 + half, half, half);

            p.setPen(Qt::NoPen);
            p.setBrush(QColor(235, 236, 240, 140));
            p.drawRect(a);
            p.drawRect(d);
            p.setBrush(QColor(235, 236, 240, 40));
            p.drawRect(b);
            p.drawRect(c);

            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawRect(r);
            break;
        }
        case ViewportIcon::resourceSprite:
        {
            QPen cardPen(fg);
            cardPen.setWidthF(1.8);
            cardPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(cardPen);
            p.setBrush(QColor(fg.red(), fg.green(), fg.blue(), 24));
            p.drawRoundedRect(QRectF(size * 0.18, size * 0.10, size * 0.64, size * 0.72), 2.0, 2.0);

            const QColor spriteColor(87, 210, 229);
            p.setPen(Qt::NoPen);
            p.setBrush(spriteColor);
            p.drawEllipse(QPointF(size * 0.50, size * 0.28), size * 0.075, size * 0.075);
            QPen spritePen(spriteColor);
            spritePen.setWidthF(2.2);
            spritePen.setCapStyle(Qt::RoundCap);
            spritePen.setJoinStyle(Qt::RoundJoin);
            p.setPen(spritePen);
            p.setBrush(Qt::NoBrush);
            p.drawLine(QPointF(size * 0.50, size * 0.39), QPointF(size * 0.50, size * 0.57));
            p.drawLine(QPointF(size * 0.33, size * 0.48), QPointF(size * 0.50, size * 0.42));
            p.drawLine(QPointF(size * 0.50, size * 0.42), QPointF(size * 0.67, size * 0.48));
            p.drawLine(QPointF(size * 0.50, size * 0.57), QPointF(size * 0.39, size * 0.72));
            p.drawLine(QPointF(size * 0.50, size * 0.57), QPointF(size * 0.61, size * 0.72));

            p.setPen(cardPen);
            p.drawLine(QPointF(size * 0.50, size * 0.82), QPointF(size * 0.50, size * 0.91));
            p.drawLine(QPointF(size * 0.32, size * 0.91), QPointF(size * 0.68, size * 0.91));
            break;
        }
        case ViewportIcon::wireframeOverlay:
        {
            const QRect back(2, 2, size - 8, size - 8);
            const QRect front(5, 5, size - 8, size - 8);
            p.setBrush(Qt::NoBrush);
            p.drawRect(back);
            p.drawRect(front);
            break;
        }
        case ViewportIcon::gamePreview:
        {
            const QRect r(3, 3, size - 6, size - 6);
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r, 3, 3);

            p.setPen(Qt::NoPen);
            p.setBrush(fg);
            QPolygon tri;
            tri << QPoint(7, 5) << QPoint(7, size - 5) << QPoint(size - 5, size / 2);
            p.drawPolygon(tri);

            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            break;
        }
        case ViewportIcon::sound:
        case ViewportIcon::soundMuted:
        {
            QPen soundPen(fg);
            soundPen.setWidthF(2.4);
            soundPen.setCapStyle(Qt::RoundCap);
            soundPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(soundPen);
            p.setBrush(fill);

            const QPolygonF speaker{
                QPointF(size * 0.15, size * 0.42),
                QPointF(size * 0.31, size * 0.42),
                QPointF(size * 0.49, size * 0.25),
                QPointF(size * 0.49, size * 0.75),
                QPointF(size * 0.31, size * 0.58),
                QPointF(size * 0.15, size * 0.58),
            };
            p.drawPolygon(speaker);

            p.setBrush(Qt::NoBrush);
            if (icon == ViewportIcon::sound)
            {
                p.drawArc(QRectF(size * 0.42, size * 0.31, size * 0.27, size * 0.38), -58 * 16, 116 * 16);
                p.drawArc(QRectF(size * 0.39, size * 0.19, size * 0.48, size * 0.62), -54 * 16, 108 * 16);
            }
            else
            {
                QPen slashPen(QColor(245, 96, 96));
                slashPen.setWidthF(3.0);
                slashPen.setCapStyle(Qt::RoundCap);
                p.setPen(slashPen);
                p.drawLine(QPointF(size * 0.60, size * 0.34), QPointF(size * 0.84, size * 0.66));
                p.drawLine(QPointF(size * 0.84, size * 0.34), QPointF(size * 0.60, size * 0.66));
            }
            break;
        }
        case ViewportIcon::camera:
        {
            const QRect body(3, 5, size - 6, size - 10);
            const QRect top(6, 3, 6, 4);

            p.setBrush(fill);
            p.drawRoundedRect(body, 3, 3);

            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(body, 3, 3);

            p.setBrush(fill);
            p.drawRoundedRect(top, 2, 2);

            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPoint(size / 2 + 2, size / 2), 3, 3);
            break;
        }
        case ViewportIcon::outlinerSector:
        {
            QPen thinPen(fg);
            thinPen.setWidthF(2.1);
            thinPen.setCapStyle(Qt::RoundCap);
            thinPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(thinPen);

            const QPointF top(size * 0.50, size * 0.10);
            const QPointF upperLeft(size * 0.16, size * 0.29);
            const QPointF upperRight(size * 0.84, size * 0.29);
            const QPointF center(size * 0.50, size * 0.49);
            const QPointF lowerLeft(size * 0.16, size * 0.68);
            const QPointF lowerRight(size * 0.84, size * 0.68);
            const QPointF bottom(size * 0.50, size * 0.89);

            p.setBrush(QColor(fg.red(), fg.green(), fg.blue(), 35));
            p.drawPolygon(QPolygonF{ top, upperLeft, center, upperRight });
            p.drawPolygon(QPolygonF{ upperLeft, center, bottom, lowerLeft });
            p.drawPolygon(QPolygonF{ center, upperRight, lowerRight, bottom });
            p.setBrush(Qt::NoBrush);
            p.drawPolygon(QPolygonF{ top, upperLeft, lowerLeft, bottom, lowerRight, upperRight });
            p.drawLine(top, center);
            p.drawLine(upperLeft, center);
            p.drawLine(upperRight, center);
            p.drawLine(center, bottom);
            break;
        }
        case ViewportIcon::outlinerThing:
        {
            const QPointF top(size * 0.50, size * 0.10);
            const QPointF upperLeft(size * 0.17, size * 0.30);
            const QPointF center(size * 0.50, size * 0.49);
            const QPointF upperRight(size * 0.83, size * 0.30);
            const QPointF lowerLeft(size * 0.17, size * 0.66);
            const QPointF bottom(size * 0.50, size * 0.87);
            const QPointF lowerRight(size * 0.83, size * 0.66);

            // A solid three-face primitive represents any placed game object,
            // while the neighboring Sector tab remains a wireframe volume.
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(fg.red(), fg.green(), fg.blue(), 235));
            p.drawPolygon(QPolygonF{ top, upperRight, center, upperLeft });
            p.setBrush(QColor(fg.red(), fg.green(), fg.blue(), 175));
            p.drawPolygon(QPolygonF{ upperLeft, center, bottom, lowerLeft });
            p.setBrush(QColor(fg.red(), fg.green(), fg.blue(), 105));
            p.drawPolygon(QPolygonF{ center, upperRight, lowerRight, bottom });

            QPen outlinePen(fg);
            outlinePen.setWidthF(1.55);
            outlinePen.setCapStyle(Qt::RoundCap);
            outlinePen.setJoinStyle(Qt::RoundJoin);
            p.setPen(outlinePen);
            p.setBrush(Qt::NoBrush);
            p.drawPolygon(QPolygonF{ top, upperRight, lowerRight, bottom, lowerLeft, upperLeft });
            p.drawLine(top, center);
            p.drawLine(upperLeft, center);
            p.drawLine(upperRight, center);
            p.drawLine(center, bottom);
            break;
        }
        case ViewportIcon::settingsCog:
        {
            // Compact configuration buttons need an unambiguous standalone
            // cog rather than the document-plus-gear Level Settings glyph.
            const QPointF center(size * 0.50, size * 0.50);
            QPen toothPen(fg);
            toothPen.setWidthF(2.6);
            toothPen.setCapStyle(Qt::RoundCap);
            p.setPen(toothPen);
            for (int i = 0; i < 8; ++i)
            {
                const qreal angle = static_cast<qreal>(i)
                    * std::numbers::pi_v<qreal> / 4.0;
                const QPointF inner(
                    center.x() + std::cos(angle) * size * 0.27,
                    center.y() + std::sin(angle) * size * 0.27);
                const QPointF outer(
                    center.x() + std::cos(angle) * size * 0.39,
                    center.y() + std::sin(angle) * size * 0.39);
                p.drawLine(inner, outer);
            }

            QPen bodyPen(fg);
            bodyPen.setWidthF(2.4);
            p.setPen(bodyPen);
            p.setBrush(QColor(fg.red(), fg.green(), fg.blue(), 70));
            p.drawEllipse(center, size * 0.27, size * 0.27);
            p.setBrush(QColor(28, 29, 34));
            p.drawEllipse(center, size * 0.095, size * 0.095);
            break;
        }
        case ViewportIcon::levelSettings:
        {
            // A dog-eared level document plus a settings gear reads as
            // level-wide properties rather than an opaque file-header editor.
            const QPainterPath document = []()
            {
                QPainterPath path;
                path.moveTo(6.0, 3.5);
                path.lineTo(20.0, 3.5);
                path.lineTo(26.0, 9.5);
                path.lineTo(26.0, 27.5);
                path.lineTo(6.0, 27.5);
                path.closeSubpath();
                return path;
            }();

            QPen documentPen(fg);
            documentPen.setWidthF(1.8);
            documentPen.setCapStyle(Qt::RoundCap);
            documentPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(documentPen);
            p.setBrush(fill);
            p.drawPath(document);
            p.setBrush(Qt::NoBrush);
            p.drawLine(QPointF(20.0, 3.5), QPointF(20.0, 9.5));
            p.drawLine(QPointF(20.0, 9.5), QPointF(26.0, 9.5));
            p.drawLine(QPointF(9.5, 10.5), QPointF(16.5, 10.5));
            p.drawLine(QPointF(9.5, 15.0), QPointF(15.0, 15.0));

            const QPointF gearCenter(21.0, 22.0);
            QPen gearPen(QColor(112, 181, 255));
            gearPen.setWidthF(2.2);
            gearPen.setCapStyle(Qt::RoundCap);
            p.setPen(gearPen);
            p.setBrush(QColor(30, 82, 139, 235));
            p.drawEllipse(gearCenter, 5.0, 5.0);
            for (int i = 0; i < 8; ++i)
            {
                const qreal angle = static_cast<qreal>(i) * std::numbers::pi_v<qreal> / 4.0;
                const QPointF inner(
                    gearCenter.x() + std::cos(angle) * 4.7,
                    gearCenter.y() + std::sin(angle) * 4.7);
                const QPointF outer(
                    gearCenter.x() + std::cos(angle) * 6.4,
                    gearCenter.y() + std::sin(angle) * 6.4);
                p.drawLine(inner, outer);
            }
            p.setBrush(QColor(18, 18, 22));
            p.drawEllipse(gearCenter, 1.7, 1.7);
            break;
        }
        case ViewportIcon::mapSettings:
        {
            QPen thinPen(fg);
            thinPen.setWidthF(2.0);
            thinPen.setCapStyle(Qt::RoundCap);
            thinPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(thinPen);

            const auto drawSlider = [&](const qreal y, const qreal knobX)
            {
                const qreal leftX = static_cast<qreal>(size) * 0.22;
                const qreal rightX = static_cast<qreal>(size) * 0.78;
                const qreal r = static_cast<qreal>(size) * 0.085;

                p.setBrush(Qt::NoBrush);
                p.drawLine(QPointF(leftX, y), QPointF(rightX, y));

                p.setBrush(fill);
                p.drawEllipse(QPointF(knobX, y), r, r);
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(QPointF(knobX, y), r, r);
            };

            drawSlider(static_cast<qreal>(size) * 0.30, static_cast<qreal>(size) * 0.36);
            drawSlider(static_cast<qreal>(size) * 0.50, static_cast<qreal>(size) * 0.64);
            drawSlider(static_cast<qreal>(size) * 0.70, static_cast<qreal>(size) * 0.46);
            break;
        }
        case ViewportIcon::mapShowGrid:
        {
            drawIconGrid(p, QRectF(size * 0.15, size * 0.15, size * 0.70, size * 0.70),
                QColor(105, 160, 255));
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 210, 95));
            p.drawEllipse(QPointF(size * 0.50, size * 0.50), size * 0.065, size * 0.065);
            break;
        }
        case ViewportIcon::mapFog:
        {
            QPen hazePen(QColor(175, 196, 215));
            hazePen.setWidthF(2.4);
            hazePen.setCapStyle(Qt::RoundCap);
            p.setPen(hazePen);
            p.setBrush(Qt::NoBrush);
            p.drawPath([]()
                {
                    QPainterPath path;
                    path.moveTo(4.0, 10.0);
                    path.cubicTo(9.0, 6.0, 13.0, 14.0, 18.0, 10.0);
                    path.cubicTo(22.0, 7.0, 25.0, 9.0, 28.0, 10.0);
                    return path;
                }());
            p.drawLine(QPointF(4.0, 17.0), QPointF(25.0, 17.0));
            p.drawLine(QPointF(8.0, 24.0), QPointF(28.0, 24.0));
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(125, 160, 190, 45));
            p.drawRoundedRect(QRectF(3.0, 6.0, 26.0, 21.0), 5.0, 5.0);
            break;
        }
        case ViewportIcon::mapGridFollow:
        {
            drawIconGrid(p, QRectF(size * 0.12, size * 0.58, size * 0.76, size * 0.27),
                QColor(105, 160, 255), true);
            QPen cameraPen(fg);
            cameraPen.setWidthF(2.0);
            cameraPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(cameraPen);
            p.setBrush(fill);
            p.drawRoundedRect(QRectF(size * 0.24, size * 0.14, size * 0.38, size * 0.25), 2.0, 2.0);
            p.drawPolygon(QPolygonF{
                QPointF(size * 0.62, size * 0.20),
                QPointF(size * 0.82, size * 0.13),
                QPointF(size * 0.82, size * 0.40),
                QPointF(size * 0.62, size * 0.33),
            });
            QPen followPen(QColor(255, 205, 95));
            followPen.setWidthF(2.1);
            followPen.setCapStyle(Qt::RoundCap);
            p.setPen(followPen);
            p.drawLine(QPointF(size * 0.42, size * 0.42), QPointF(size * 0.42, size * 0.55));
            break;
        }
        case ViewportIcon::mapGridRotateView:
        {
            drawIconGrid(p, QRectF(size * 0.23, size * 0.28, size * 0.58, size * 0.50),
                QColor(105, 160, 255), true);
            QPen rotatePen(QColor(255, 205, 95));
            rotatePen.setWidthF(2.5);
            rotatePen.setCapStyle(Qt::RoundCap);
            p.setPen(rotatePen);
            p.drawArc(QRectF(size * 0.08, size * 0.08, size * 0.55, size * 0.55), 55 * 16, 190 * 16);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 205, 95));
            p.drawPolygon(QPolygonF{
                QPointF(size * 0.13, size * 0.20),
                QPointF(size * 0.12, size * 0.39),
                QPointF(size * 0.27, size * 0.29),
            });
            break;
        }
        case ViewportIcon::mapGridAdaptive:
        {
            drawIconGrid(p, QRectF(size * 0.23, size * 0.23, size * 0.54, size * 0.54),
                QColor(105, 160, 255));
            QPen scalePen(QColor(255, 205, 95));
            scalePen.setWidthF(2.3);
            scalePen.setCapStyle(Qt::RoundCap);
            scalePen.setJoinStyle(Qt::RoundJoin);
            p.setPen(scalePen);
            p.drawLine(QPointF(size * 0.16, size * 0.84), QPointF(size * 0.39, size * 0.61));
            p.drawLine(QPointF(size * 0.16, size * 0.84), QPointF(size * 0.17, size * 0.70));
            p.drawLine(QPointF(size * 0.16, size * 0.84), QPointF(size * 0.30, size * 0.83));
            p.drawLine(QPointF(size * 0.84, size * 0.16), QPointF(size * 0.61, size * 0.39));
            p.drawLine(QPointF(size * 0.84, size * 0.16), QPointF(size * 0.70, size * 0.17));
            p.drawLine(QPointF(size * 0.84, size * 0.16), QPointF(size * 0.83, size * 0.30));
            break;
        }
        case ViewportIcon::mapGridVisibleThrough:
        {
            drawIconCube(p, QRectF(size * 0.19, size * 0.13, size * 0.62, size * 0.74),
                QColor(205, 210, 220), true);
            // Cyan lines continue across the solid object to communicate the
            // optional x-ray grid rather than ordinary depth testing.
            QPen throughPen(QColor(80, 205, 245));
            throughPen.setWidthF(2.0);
            throughPen.setCapStyle(Qt::RoundCap);
            p.setPen(throughPen);
            p.drawLine(QPointF(size * 0.08, size * 0.39), QPointF(size * 0.92, size * 0.39));
            p.drawLine(QPointF(size * 0.08, size * 0.61), QPointF(size * 0.92, size * 0.61));
            break;
        }
        case ViewportIcon::mapGridBehind:
        {
            drawIconGrid(p, QRectF(size * 0.08, size * 0.12, size * 0.84, size * 0.76),
                QColor(95, 125, 175));
            p.setPen(QPen(fg, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(QColor(40, 44, 57, 245));
            p.drawRoundedRect(QRectF(size * 0.28, size * 0.24, size * 0.44, size * 0.52), 2.0, 2.0);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 205, 95));
            p.drawPolygon(QPolygonF{
                QPointF(size * 0.42, size * 0.86),
                QPointF(size * 0.58, size * 0.86),
                QPointF(size * 0.50, size * 0.96),
            });
            break;
        }
        case ViewportIcon::mapGridReset:
        {
            drawIconGrid(p, QRectF(size * 0.29, size * 0.30, size * 0.55, size * 0.48),
                QColor(105, 160, 255), true);
            QPen resetPen(QColor(255, 205, 95));
            resetPen.setWidthF(2.5);
            resetPen.setCapStyle(Qt::RoundCap);
            p.setPen(resetPen);
            p.drawArc(QRectF(size * 0.09, size * 0.09, size * 0.55, size * 0.55), 35 * 16, 235 * 16);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 205, 95));
            p.drawPolygon(QPolygonF{
                QPointF(size * 0.55, size * 0.08),
                QPointF(size * 0.68, size * 0.19),
                QPointF(size * 0.50, size * 0.24),
            });
            break;
        }
        case ViewportIcon::mapGridAlignView:
        {
            drawIconGrid(p, QRectF(size * 0.16, size * 0.52, size * 0.68, size * 0.31),
                QColor(105, 160, 255));
            QPen viewPen(QColor(255, 205, 95));
            viewPen.setWidthF(2.2);
            viewPen.setCapStyle(Qt::RoundCap);
            p.setPen(viewPen);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(size * 0.50, size * 0.21), size * 0.12, size * 0.09);
            p.drawLine(QPointF(size * 0.50, size * 0.31), QPointF(size * 0.50, size * 0.46));
            p.drawLine(QPointF(size * 0.43, size * 0.39), QPointF(size * 0.50, size * 0.46));
            p.drawLine(QPointF(size * 0.57, size * 0.39), QPointF(size * 0.50, size * 0.46));
            break;
        }
        case ViewportIcon::mapGridOrientObject:
        {
            drawIconGrid(p, QRectF(size * 0.11, size * 0.62, size * 0.78, size * 0.23),
                QColor(105, 160, 255), true);
            drawIconCube(p, QRectF(size * 0.30, size * 0.10, size * 0.40, size * 0.43),
                fg, true);
            QPen snapPen(QColor(255, 205, 95));
            snapPen.setWidthF(2.1);
            snapPen.setCapStyle(Qt::RoundCap);
            p.setPen(snapPen);
            p.drawLine(QPointF(size * 0.50, size * 0.51), QPointF(size * 0.50, size * 0.60));
            break;
        }
        case ViewportIcon::mapGridOrientSelection:
        {
            const QPolygonF face{
                QPointF(size * 0.18, size * 0.25),
                QPointF(size * 0.55, size * 0.13),
                QPointF(size * 0.68, size * 0.46),
                QPointF(size * 0.31, size * 0.57),
            };
            p.setPen(QPen(QColor(255, 205, 95), 2.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(QColor(255, 205, 95, 55));
            p.drawPolygon(face);
            drawIconGrid(p, QRectF(size * 0.25, size * 0.60, size * 0.64, size * 0.24),
                QColor(105, 160, 255), true);
            QPen orientPen(QColor(120, 220, 145));
            orientPen.setWidthF(2.0);
            orientPen.setCapStyle(Qt::RoundCap);
            p.setPen(orientPen);
            p.drawArc(QRectF(size * 0.05, size * 0.42, size * 0.38, size * 0.38), 40 * 16, 145 * 16);
            break;
        }
        case ViewportIcon::mapGridCenterSelection:
        {
            drawIconGrid(p, QRectF(size * 0.12, size * 0.12, size * 0.76, size * 0.76),
                QColor(105, 160, 255));
            QPen targetPen(QColor(255, 205, 95));
            targetPen.setWidthF(2.2);
            targetPen.setCapStyle(Qt::RoundCap);
            p.setPen(targetPen);
            p.setBrush(QColor(255, 205, 95, 50));
            p.drawEllipse(QPointF(size * 0.50, size * 0.50), size * 0.15, size * 0.15);
            p.drawLine(QPointF(size * 0.50, size * 0.25), QPointF(size * 0.50, size * 0.75));
            p.drawLine(QPointF(size * 0.25, size * 0.50), QPointF(size * 0.75, size * 0.50));
            break;
        }
        case ViewportIcon::mapGridClearCenter:
        {
            drawIconGrid(p, QRectF(size * 0.12, size * 0.12, size * 0.76, size * 0.76),
                QColor(105, 160, 255));
            p.setPen(QPen(QColor(245, 95, 100), 3.0, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(size * 0.36, size * 0.36), QPointF(size * 0.64, size * 0.64));
            p.drawLine(QPointF(size * 0.64, size * 0.36), QPointF(size * 0.36, size * 0.64));
            p.setPen(QPen(QColor(255, 205, 95), 1.9, Qt::SolidLine, Qt::RoundCap));
            p.drawArc(QRectF(size * 0.05, size * 0.05, size * 0.34, size * 0.34), 25 * 16, 210 * 16);
            break;
        }
        case ViewportIcon::outlinerCogs:
        {
            QPen thinPen(fg);
            thinPen.setWidthF(2.0);
            thinPen.setCapStyle(Qt::RoundCap);
            thinPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(thinPen);
            p.setBrush(Qt::NoBrush);

            auto drawGear = [&](const QPointF center, const qreal radius, const int teeth, const qreal rotationDeg)
            {
                if (teeth <= 0 || radius <= 0.0)
                {
                    return;
                }

                constexpr qreal kPi = 3.14159265358979323846;
                const qreal innerStart = radius * 0.82;
                const qreal toothEnd = radius * 1.10;
                const qreal stepDeg = 360.0 / static_cast<qreal>(teeth);

                for (int i = 0; i < teeth; ++i)
                {
                    const qreal aDeg = rotationDeg + stepDeg * static_cast<qreal>(i);
                    const qreal aRad = (aDeg * kPi) / 180.0;
                    const qreal ca = std::cos(aRad);
                    const qreal sa = std::sin(aRad);

                    const QPointF a(center.x() + ca * innerStart, center.y() + sa * innerStart);
                    const QPointF b(center.x() + ca * toothEnd, center.y() + sa * toothEnd);
                    p.drawLine(a, b);
                }

                p.setBrush(fill);
                p.drawEllipse(center, radius * 0.72, radius * 0.72);
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(center, radius * 0.72, radius * 0.72);
                p.drawEllipse(center, radius * 0.26, radius * 0.26);
            };

            drawGear(QPointF(size * 0.50, size * 0.50), size * 0.36, 8, 0.0);
            break;
        }
        case ViewportIcon::outlinerLightBulb:
        {
            QPen thinPen(fg);
            thinPen.setWidthF(2.1);
            thinPen.setCapStyle(Qt::RoundCap);
            thinPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(thinPen);
            const QRectF globe(size * 0.25, size * 0.13, size * 0.50, size * 0.50);
            const QRectF base(size * 0.36, size * 0.64, size * 0.28, size * 0.16);

            p.setBrush(fill);
            p.drawEllipse(globe);
            p.drawRoundedRect(base, 2.0, 2.0);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(globe);
            p.drawRoundedRect(base, 2.0, 2.0);
            p.drawLine(QPointF(size * 0.40, size * 0.84), QPointF(size * 0.60, size * 0.84));
            p.drawLine(QPointF(size * 0.50, size * 0.02), QPointF(size * 0.50, size * 0.09));
            p.drawLine(QPointF(size * 0.14, size * 0.23), QPointF(size * 0.22, size * 0.28));
            p.drawLine(QPointF(size * 0.86, size * 0.23), QPointF(size * 0.78, size * 0.28));
            break;
        }
        case ViewportIcon::lightBaked:
        case ViewportIcon::lightThing:
        case ViewportIcon::lightHeadlight:
        case ViewportIcon::lightSector:
        case ViewportIcon::lightArchitectural:
        {
            drawLightCategoryGlyph(p, size, icon);
            break;
        }
        case ViewportIcon::lightingFullbright:
        {
            // An evenly white sphere and short halo rays communicate that
            // every face is displayed at full brightness, independent of
            // scene or vertex lighting.
            const QPointF center(size * 0.50, size * 0.50);
            const qreal radius = size * 0.27;
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(248, 240, 190));
            p.drawEllipse(center, radius, radius);

            QPen haloPen(QColor(255, 220, 100));
            haloPen.setWidthF(2.1);
            haloPen.setCapStyle(Qt::RoundCap);
            p.setPen(haloPen);
            for (int i = 0; i < 8; ++i)
            {
                constexpr qreal kPi = 3.14159265358979323846;
                const qreal angle = kPi * static_cast<qreal>(i) / 4.0;
                const QPointF direction(std::cos(angle), std::sin(angle));
                p.drawLine(center + direction * (size * 0.34), center + direction * (size * 0.43));
            }
            break;
        }
        case ViewportIcon::lightingLit:
        {
            // A light source beside a two-tone sphere is the conventional
            // shorthand for scene lighting without claiming smooth vertex
            // interpolation.
            const QRectF sphere(size * 0.27, size * 0.28, size * 0.58, size * 0.58);
            QPainterPath spherePath;
            spherePath.addEllipse(sphere);
            p.setClipPath(spherePath);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(244, 216, 132));
            p.drawRect(sphere);
            p.setBrush(QColor(67, 73, 91));
            p.drawPolygon(QPolygonF{
                QPointF(size * 0.54, size * 0.25),
                QPointF(size * 0.88, size * 0.25),
                QPointF(size * 0.88, size * 0.89),
                QPointF(size * 0.45, size * 0.89),
            });
            p.setClipping(false);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(sphere);

            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 221, 92));
            p.drawEllipse(QPointF(size * 0.20, size * 0.20), size * 0.095, size * 0.095);
            QPen rayPen(QColor(255, 221, 92));
            rayPen.setWidthF(1.8);
            rayPen.setCapStyle(Qt::RoundCap);
            p.setPen(rayPen);
            p.drawLine(QPointF(size * 0.28, size * 0.28), QPointF(size * 0.37, size * 0.37));
            break;
        }
        case ViewportIcon::lightingDiffuse:
        {
            // Hard-edged tonal facets distinguish per-face diffuse shading
            // from Gouraud's continuous gradient sphere.
            const QRectF sphere(size * 0.16, size * 0.16, size * 0.68, size * 0.68);
            QPainterPath spherePath;
            spherePath.addEllipse(sphere);
            p.setClipPath(spherePath);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(239, 218, 150));
            p.drawPolygon(QPolygonF{
                QPointF(size * 0.16, size * 0.16),
                QPointF(size * 0.59, size * 0.16),
                QPointF(size * 0.48, size * 0.54),
                QPointF(size * 0.16, size * 0.65),
            });
            p.setBrush(QColor(122, 150, 190));
            p.drawPolygon(QPolygonF{
                QPointF(size * 0.59, size * 0.16),
                QPointF(size * 0.84, size * 0.16),
                QPointF(size * 0.84, size * 0.67),
                QPointF(size * 0.48, size * 0.54),
            });
            p.setBrush(QColor(55, 62, 80));
            p.drawPolygon(QPolygonF{
                QPointF(size * 0.16, size * 0.65),
                QPointF(size * 0.48, size * 0.54),
                QPointF(size * 0.84, size * 0.67),
                QPointF(size * 0.84, size * 0.84),
                QPointF(size * 0.16, size * 0.84),
            });
            p.setClipping(false);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(sphere);

            QPen directionPen(QColor(255, 210, 95));
            directionPen.setWidthF(1.9);
            directionPen.setCapStyle(Qt::RoundCap);
            p.setPen(directionPen);
            p.drawLine(QPointF(size * 0.11, size * 0.11), QPointF(size * 0.25, size * 0.25));
            break;
        }
        case ViewportIcon::lightingGouraud:
        {
            // The smooth shaded sphere is the conventional material-preview
            // glyph for interpolated (Gouraud) vertex lighting.
            const QRectF sphere(size * 0.14, size * 0.14, size * 0.72, size * 0.72);
            QRadialGradient gradient(QPointF(size * 0.35, size * 0.31), size * 0.64);
            gradient.setFocalPoint(QPointF(size * 0.32, size * 0.28));
            gradient.setColorAt(0.0, QColor(255, 245, 205));
            gradient.setColorAt(0.28, QColor(190, 212, 235));
            gradient.setColorAt(0.68, QColor(88, 119, 165));
            gradient.setColorAt(1.0, QColor(35, 42, 58));
            p.setBrush(gradient);
            p.setPen(pen);
            p.drawEllipse(sphere);
            break;
        }
        case ViewportIcon::thingDots:
        {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 205, 95));
            p.drawEllipse(QPointF(size * 0.50, size * 0.50), size * 0.10, size * 0.10);
            p.setBrush(fg);
            p.drawEllipse(QPointF(size * 0.25, size * 0.27), size * 0.055, size * 0.055);
            p.drawEllipse(QPointF(size * 0.75, size * 0.31), size * 0.055, size * 0.055);
            p.drawEllipse(QPointF(size * 0.30, size * 0.76), size * 0.055, size * 0.055);
            p.drawEllipse(QPointF(size * 0.73, size * 0.72), size * 0.055, size * 0.055);
            break;
        }
        case ViewportIcon::thingBoxes:
        {
            // One screen-aligned bounding box with handles; this cannot be
            // confused with the perspective cube used by Wireframes.
            const QRectF bounds(size * 0.18, size * 0.22, size * 0.64, size * 0.56);
            p.setBrush(Qt::NoBrush);
            p.drawRect(bounds);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(105, 160, 255));
            for (const QPointF& corner : { bounds.topLeft(), bounds.topRight(), bounds.bottomLeft(), bounds.bottomRight() })
            {
                p.drawRect(QRectF(corner.x() - 2.0, corner.y() - 2.0, 4.0, 4.0));
            }
            break;
        }
        case ViewportIcon::thingWireframes:
        {
            const QRectF back(size * 0.16, size * 0.16, size * 0.48, size * 0.48);
            const QRectF front(size * 0.35, size * 0.35, size * 0.48, size * 0.48);
            p.setBrush(Qt::NoBrush);
            QPen thin = pen;
            thin.setWidthF(2.0);
            p.setPen(thin);
            p.drawRect(back);
            p.drawRect(front);
            p.drawLine(back.topLeft(), front.topLeft());
            p.drawLine(back.topRight(), front.topRight());
            p.drawLine(back.bottomLeft(), front.bottomLeft());
            p.drawLine(back.bottomRight(), front.bottomRight());
            break;
        }
        case ViewportIcon::thingShaded:
        {
            const QPointF top(size * 0.50, size * 0.10);
            const QPointF left(size * 0.17, size * 0.29);
            const QPointF center(size * 0.50, size * 0.48);
            const QPointF right(size * 0.83, size * 0.29);
            const QPointF lowerLeft(size * 0.17, size * 0.67);
            const QPointF bottom(size * 0.50, size * 0.88);
            const QPointF lowerRight(size * 0.83, size * 0.67);

            p.setPen(Qt::NoPen);
            p.setBrush(QColor(235, 236, 240, 220));
            p.drawPolygon(QPolygonF{ top, right, center, left });
            p.setBrush(QColor(105, 160, 255, 155));
            p.drawPolygon(QPolygonF{ left, center, bottom, lowerLeft });
            p.setBrush(QColor(72, 82, 105, 210));
            p.drawPolygon(QPolygonF{ center, right, lowerRight, bottom });
            QPen outline = pen;
            outline.setWidthF(1.6);
            p.setPen(outline);
            p.setBrush(Qt::NoBrush);
            p.drawPolygon(QPolygonF{ top, right, lowerRight, bottom, lowerLeft, left });
            p.drawLine(top, center);
            p.drawLine(left, center);
            p.drawLine(right, center);
            p.drawLine(center, bottom);
            break;
        }
        case ViewportIcon::thingGizmos:
        {
            // Thing-type gizmos are scene markers, so represent the toggle with
            // a centered map pin instead of a transform-axis triad.
            const qreal centerX = size * 0.50;
            QPainterPath pin;
            pin.moveTo(centerX, size * 0.92);
            pin.cubicTo(
                size * 0.43, size * 0.78,
                size * 0.18, size * 0.55,
                size * 0.18, size * 0.34);
            pin.cubicTo(
                size * 0.18, size * 0.16,
                size * 0.32, size * 0.06,
                centerX, size * 0.06);
            pin.cubicTo(
                size * 0.68, size * 0.06,
                size * 0.82, size * 0.16,
                size * 0.82, size * 0.34);
            pin.cubicTo(
                size * 0.82, size * 0.55,
                size * 0.57, size * 0.78,
                centerX, size * 0.92);
            pin.closeSubpath();

            QPen pinOutline(QColor(13, 15, 20), 2.2, Qt::SolidLine,
                Qt::RoundCap, Qt::RoundJoin);
            p.setPen(pinOutline);
            p.setBrush(QColor(105, 160, 255));
            p.drawPath(pin);
            p.setPen(QPen(QColor(13, 15, 20), 1.6));
            p.setBrush(QColor(31, 46, 68));
            p.drawEllipse(QPointF(centerX, size * 0.34),
                size * 0.105, size * 0.105);
            break;
        }
        case ViewportIcon::toolSelect:
        {
            QPen accentPen(QColor(150, 185, 255));
            accentPen.setWidthF(2.4);
            accentPen.setCapStyle(Qt::RoundCap);
            accentPen.setJoinStyle(Qt::RoundJoin);

            const QRectF box(size * 0.15, size * 0.15, size * 0.62, size * 0.62);
            accentPen.setStyle(Qt::DashLine);
            p.setPen(accentPen);
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(box, size * 0.06, size * 0.06);

            p.setPen(Qt::NoPen);
            p.setBrush(fg);
            QPolygonF poly;
            poly << QPointF(size * 0.40, size * 0.35)
                 << QPointF(size * 0.78, size * 0.62)
                 << QPointF(size * 0.61, size * 0.65)
                 << QPointF(size * 0.69, size * 0.84)
                 << QPointF(size * 0.60, size * 0.87)
                 << QPointF(size * 0.52, size * 0.68)
                 << QPointF(size * 0.40, size * 0.78);
            p.drawPolygon(poly);

            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            break;
        }
        case ViewportIcon::toolMove:
        {
            QPen xPen(QColor(235, 85, 85));
            xPen.setWidthF(3.0);
            xPen.setCapStyle(Qt::RoundCap);
            xPen.setJoinStyle(Qt::RoundJoin);
            QPen yPen(QColor(95, 220, 135));
            yPen.setWidthF(3.0);
            yPen.setCapStyle(Qt::RoundCap);
            yPen.setJoinStyle(Qt::RoundJoin);

            const QPointF c(size * 0.50, size * 0.50);
            const qreal min = size * 0.14;
            const qreal max = size * 0.86;
            p.setBrush(Qt::NoBrush);

            p.setPen(yPen);
            p.drawLine(QPointF(c.x(), min), QPointF(c.x(), max));
            p.drawLine(QPointF(c.x(), min), QPointF(c.x() - size * 0.11, min + size * 0.13));
            p.drawLine(QPointF(c.x(), min), QPointF(c.x() + size * 0.11, min + size * 0.13));
            p.drawLine(QPointF(c.x(), max), QPointF(c.x() - size * 0.11, max - size * 0.13));
            p.drawLine(QPointF(c.x(), max), QPointF(c.x() + size * 0.11, max - size * 0.13));

            p.setPen(xPen);
            p.drawLine(QPointF(min, c.y()), QPointF(max, c.y()));
            p.drawLine(QPointF(min, c.y()), QPointF(min + size * 0.13, c.y() - size * 0.11));
            p.drawLine(QPointF(min, c.y()), QPointF(min + size * 0.13, c.y() + size * 0.11));
            p.drawLine(QPointF(max, c.y()), QPointF(max - size * 0.13, c.y() - size * 0.11));
            p.drawLine(QPointF(max, c.y()), QPointF(max - size * 0.13, c.y() + size * 0.11));

            p.setPen(Qt::NoPen);
            p.setBrush(fg);
            p.drawEllipse(c, size * 0.045, size * 0.045);
            break;
        }
        case ViewportIcon::sectorThrust:
        {
            // A deliberately bold IJIM thrust arrow, rising left-to-right so
            // it cannot be confused with the three-axis Move tool.
            const QColor thrustRed(226, 54, 48);
            const QPointF tail(size * 0.16, size * 0.79);
            const QPointF tip(size * 0.82, size * 0.16);
            QPen thrustPen(thrustRed);
            thrustPen.setWidthF(5.8);
            thrustPen.setCapStyle(Qt::RoundCap);
            thrustPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(thrustPen);
            p.setBrush(Qt::NoBrush);
            p.drawLine(tail, tip);
            p.drawLine(tip, QPointF(size * 0.56, size * 0.19));
            p.drawLine(tip, QPointF(size * 0.80, size * 0.43));
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(226, 54, 48, 145));
            p.drawEllipse(tail, size * 0.085, size * 0.085);
            break;
        }
        case ViewportIcon::gridSnap:
        {
            QPen gridPen(QColor(105, 160, 255));
            gridPen.setWidthF(2.0);
            gridPen.setCapStyle(Qt::RoundCap);

            // A front-facing lattice and an amber point pulled onto its
            // nearest intersection. This stays distinct from Grid Stick's
            // side-on plane/contact silhouette at contextual-toolbar sizes.
            p.setBrush(Qt::NoBrush);
            p.setPen(gridPen);
            for (const qreal offset : { 0.24, 0.50, 0.76 })
            {
                p.drawLine(QPointF(size * offset, size * 0.18), QPointF(size * offset, size * 0.82));
                p.drawLine(QPointF(size * 0.18, size * offset), QPointF(size * 0.82, size * offset));
            }

            QPen snapPen(QColor(255, 205, 95));
            snapPen.setWidthF(2.6);
            snapPen.setCapStyle(Qt::RoundCap);
            p.setPen(snapPen);
            p.drawLine(QPointF(size * 0.80, size * 0.18), QPointF(size * 0.56, size * 0.44));
            p.drawLine(QPointF(size * 0.56, size * 0.44), QPointF(size * 0.58, size * 0.31));
            p.drawLine(QPointF(size * 0.56, size * 0.44), QPointF(size * 0.69, size * 0.42));
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 205, 95));
            p.drawEllipse(QPointF(size * 0.50, size * 0.50), size * 0.065, size * 0.065);
            break;
        }
        case ViewportIcon::gridStick:
        {
            QPen gridPen(QColor(105, 160, 255));
            gridPen.setWidthF(2.0);
            gridPen.setCapStyle(Qt::RoundCap);
            gridPen.setJoinStyle(Qt::RoundJoin);

            // A small perspective grid plane with a move arrow held at its
            // contact point. Keep the silhouette legible in compact
            // contextual transform options.
            p.setBrush(Qt::NoBrush);
            p.setPen(gridPen);
            p.drawLine(QPointF(size * 0.13, size * 0.61), QPointF(size * 0.87, size * 0.61));
            p.drawLine(QPointF(size * 0.19, size * 0.78), QPointF(size * 0.81, size * 0.78));
            p.drawLine(QPointF(size * 0.13, size * 0.61), QPointF(size * 0.19, size * 0.78));
            p.drawLine(QPointF(size * 0.87, size * 0.61), QPointF(size * 0.81, size * 0.78));
            p.drawLine(QPointF(size * 0.36, size * 0.61), QPointF(size * 0.39, size * 0.78));
            p.drawLine(QPointF(size * 0.64, size * 0.61), QPointF(size * 0.61, size * 0.78));

            QPen contactPen(QColor(255, 205, 95));
            contactPen.setWidthF(2.8);
            contactPen.setCapStyle(Qt::RoundCap);
            contactPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(contactPen);
            p.drawLine(QPointF(size * 0.50, size * 0.12), QPointF(size * 0.50, size * 0.51));
            p.drawLine(QPointF(size * 0.50, size * 0.51), QPointF(size * 0.40, size * 0.40));
            p.drawLine(QPointF(size * 0.50, size * 0.51), QPointF(size * 0.60, size * 0.40));

            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 205, 95));
            p.drawEllipse(QPointF(size * 0.50, size * 0.61), size * 0.055, size * 0.055);
            break;
        }
        case ViewportIcon::surfaceSnapAlign:
        {
            QPen surfacePen(QColor(105, 160, 255));
            surfacePen.setWidthF(2.4);
            surfacePen.setCapStyle(Qt::RoundCap);
            p.setPen(surfacePen);
            p.setBrush(Qt::NoBrush);
            p.drawLine(QPointF(size * 0.15, size * 0.28), QPointF(size * 0.72, size * 0.20));
            p.drawLine(QPointF(size * 0.28, size * 0.80), QPointF(size * 0.85, size * 0.72));

            QPen alignPen(QColor(255, 205, 95));
            alignPen.setWidthF(2.5);
            alignPen.setCapStyle(Qt::RoundCap);
            alignPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(alignPen);
            p.drawLine(QPointF(size * 0.34, size * 0.40), QPointF(size * 0.47, size * 0.54));
            p.drawLine(QPointF(size * 0.47, size * 0.54), QPointF(size * 0.37, size * 0.52));
            p.drawLine(QPointF(size * 0.47, size * 0.54), QPointF(size * 0.45, size * 0.44));
            p.drawLine(QPointF(size * 0.66, size * 0.60), QPointF(size * 0.53, size * 0.46));
            p.drawLine(QPointF(size * 0.53, size * 0.46), QPointF(size * 0.63, size * 0.48));
            p.drawLine(QPointF(size * 0.53, size * 0.46), QPointF(size * 0.55, size * 0.56));
            break;
        }
        case ViewportIcon::thingSurfaceStick:
        {
            QPen surfacePen(QColor(105, 160, 255));
            surfacePen.setWidthF(2.4);
            surfacePen.setCapStyle(Qt::RoundCap);
            p.setPen(surfacePen);
            p.drawLine(QPointF(size * 0.12, size * 0.75), QPointF(size * 0.88, size * 0.62));

            p.setPen(pen);
            p.setBrush(QColor(235, 236, 240, 85));
            p.drawRect(QRectF(size * 0.32, size * 0.18, size * 0.32, size * 0.28));

            QPen stickPen(QColor(255, 205, 95));
            stickPen.setWidthF(2.6);
            stickPen.setCapStyle(Qt::RoundCap);
            stickPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(stickPen);
            p.drawLine(QPointF(size * 0.50, size * 0.49), QPointF(size * 0.54, size * 0.62));
            p.drawLine(QPointF(size * 0.54, size * 0.62), QPointF(size * 0.45, size * 0.54));
            p.drawLine(QPointF(size * 0.54, size * 0.62), QPointF(size * 0.58, size * 0.51));
            break;
        }
        case ViewportIcon::thingSnap:
        {
            QPen objectPen(fg);
            objectPen.setWidthF(2.1);
            objectPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(objectPen);
            p.setBrush(QColor(235, 236, 240, 65));
            p.drawRect(QRectF(size * 0.10, size * 0.30, size * 0.29, size * 0.40));
            p.drawRect(QRectF(size * 0.61, size * 0.30, size * 0.29, size * 0.40));

            QPen snapPen(QColor(255, 205, 95));
            snapPen.setWidthF(2.5);
            snapPen.setCapStyle(Qt::RoundCap);
            p.setPen(snapPen);
            p.drawLine(QPointF(size * 0.42, size * 0.50), QPointF(size * 0.58, size * 0.50));
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(105, 160, 255));
            p.drawEllipse(QPointF(size * 0.50, size * 0.50), size * 0.07, size * 0.07);
            break;
        }
        case ViewportIcon::thingSnapRotate:
        {
            // The blue object is the nearby snap target. The offset amber
            // object and curved arrow show the moving Thing rotating until
            // its orientation matches that target.
            const QRectF target(size * 0.58, size * 0.38, size * 0.27, size * 0.31);
            p.setPen(QPen(QColor(105, 160, 255), 2.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(QColor(105, 160, 255, 50));
            p.drawRect(target);
            p.drawLine(target.center(), QPointF(target.center().x(), target.top() - size * 0.13));

            p.save();
            const QRectF moving(size * 0.16, size * 0.42, size * 0.25, size * 0.29);
            p.translate(moving.center());
            p.rotate(-28.0);
            p.translate(-moving.center());
            p.setPen(QPen(QColor(255, 205, 95), 2.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(QColor(255, 205, 95, 48));
            p.drawRect(moving);
            p.drawLine(moving.center(), QPointF(moving.center().x(), moving.top() - size * 0.12));
            p.restore();

            QPen rotatePen(QColor(255, 205, 95));
            rotatePen.setWidthF(2.3);
            rotatePen.setCapStyle(Qt::RoundCap);
            rotatePen.setJoinStyle(Qt::RoundJoin);
            p.setPen(rotatePen);
            p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(size * 0.20, size * 0.10, size * 0.50, size * 0.50), 35 * 16, 130 * 16);
            p.setBrush(QColor(255, 205, 95));
            p.setPen(Qt::NoPen);
            p.drawPolygon(QPolygonF{
                QPointF(size * 0.61, size * 0.12),
                QPointF(size * 0.72, size * 0.19),
                QPointF(size * 0.59, size * 0.25),
            });

            QPen snapPen(QColor(120, 220, 145));
            snapPen.setWidthF(2.0);
            snapPen.setCapStyle(Qt::RoundCap);
            p.setPen(snapPen);
            p.drawLine(QPointF(size * 0.43, size * 0.57), QPointF(size * 0.56, size * 0.57));
            break;
        }
        case ViewportIcon::toolRotate:
        {
            QPen xPen(QColor(235, 85, 85));
            xPen.setWidthF(3.0);
            xPen.setCapStyle(Qt::RoundCap);
            xPen.setJoinStyle(Qt::RoundJoin);
            QPen yPen(QColor(95, 220, 135));
            yPen.setWidthF(3.0);
            yPen.setCapStyle(Qt::RoundCap);
            yPen.setJoinStyle(Qt::RoundJoin);

            const QRectF ring(size * 0.16, size * 0.16, size * 0.68, size * 0.68);
            p.setBrush(Qt::NoBrush);
            p.setPen(xPen);
            p.drawArc(ring, -38 * 16, 154 * 16);
            p.setPen(yPen);
            p.drawArc(ring, 142 * 16, 154 * 16);

            // Arrowheads make the circular motion read immediately at 16px.
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(235, 85, 85));
            p.drawPolygon(QPolygonF{
                QPointF(size * 0.80, size * 0.28),
                QPointF(size * 0.88, size * 0.43),
                QPointF(size * 0.71, size * 0.40),
            });
            p.setBrush(QColor(95, 220, 135));
            p.drawPolygon(QPolygonF{
                QPointF(size * 0.20, size * 0.72),
                QPointF(size * 0.12, size * 0.57),
                QPointF(size * 0.29, size * 0.60),
            });
            break;
        }
        case ViewportIcon::toolScale:
        {
            QPen zPen(QColor(105, 160, 255));
            zPen.setWidthF(3.0);
            zPen.setCapStyle(Qt::RoundCap);
            zPen.setJoinStyle(Qt::RoundJoin);

            p.setBrush(Qt::NoBrush);
            const QRectF box(size * 0.16, size * 0.58, size * 0.24, size * 0.24);
            p.setPen(pen);
            p.drawRect(box);

            const QPointF a(box.topRight());
            const QPointF b(size * 0.84, size * 0.16);
            p.setPen(zPen);
            p.drawLine(a, b);
            p.drawLine(b, QPointF(b.x() - size * 0.17, b.y()));
            p.drawLine(b, QPointF(b.x(), b.y() + size * 0.17));
            break;
        }
        case ViewportIcon::toolCleave:
        {
            QPen accentPen(QColor(255, 215, 95));
            accentPen.setWidthF(3.0);
            accentPen.setCapStyle(Qt::RoundCap);
            accentPen.setJoinStyle(Qt::RoundJoin);

            const QRectF r(size * 0.17, size * 0.17, size * 0.66, size * 0.66);
            p.setBrush(Qt::NoBrush);
            p.setPen(pen);
            p.drawRect(r);

            p.setPen(accentPen);
            p.drawLine(QPointF(r.left() + size * 0.12, r.bottom() - size * 0.08), QPointF(r.right() - size * 0.10, r.top() + size * 0.08));

            p.setPen(pen);
            p.drawLine(QPointF(size * 0.39, size * 0.42), QPointF(size * 0.28, size * 0.31));
            p.drawLine(QPointF(size * 0.61, size * 0.58), QPointF(size * 0.72, size * 0.69));
            break;
        }
        case ViewportIcon::cleaveBounded:
        {
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(size * 0.12, size * 0.16, size * 0.76, size * 0.68), 1.5, 1.5);
            QPen boundaryPen(QColor(255, 215, 95));
            boundaryPen.setWidthF(2.8);
            boundaryPen.setCapStyle(Qt::RoundCap);
            p.setPen(boundaryPen);
            p.drawLine(QPointF(size * 0.63, size * 0.20), QPointF(size * 0.63, size * 0.80));
            QPen strokePen(QColor(115, 205, 255));
            strokePen.setWidthF(2.8);
            strokePen.setCapStyle(Qt::RoundCap);
            p.setPen(strokePen);
            p.drawLine(QPointF(size * 0.20, size * 0.69), QPointF(size * 0.63, size * 0.42));
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(115, 205, 255));
            p.drawEllipse(QPointF(size * 0.63, size * 0.42), size * 0.05, size * 0.05);
            break;
        }
        case ViewportIcon::toolConnect:
        {
            QPen accentPen(QColor(120, 220, 145));
            accentPen.setWidthF(3.0);
            accentPen.setCapStyle(Qt::RoundCap);
            accentPen.setJoinStyle(Qt::RoundJoin);

            const QRectF left(size * 0.13, size * 0.34, size * 0.27, size * 0.32);
            const QRectF right(size * 0.60, size * 0.34, size * 0.27, size * 0.32);

            p.setBrush(Qt::NoBrush);
            p.setPen(pen);
            p.drawRoundedRect(left, 2, 2);
            p.drawRoundedRect(right, 2, 2);

            const QPointF a(left.right(), left.center().y());
            const QPointF b(right.left(), right.center().y());
            p.setPen(accentPen);
            p.drawLine(a, b);

            p.drawLine(b, QPointF(b.x() - size * 0.14, b.y() - size * 0.10));
            p.drawLine(b, QPointF(b.x() - size * 0.14, b.y() + size * 0.10));
            break;
        }
        case ViewportIcon::toolMerge:
        {
            QPen accentPen(QColor(100, 205, 230));
            accentPen.setWidthF(2.9);
            accentPen.setCapStyle(Qt::RoundCap);
            accentPen.setJoinStyle(Qt::RoundJoin);

            const QRectF left(size * 0.13, size * 0.35, size * 0.27, size * 0.30);
            const QRectF right(size * 0.60, size * 0.35, size * 0.27, size * 0.30);

            p.setBrush(Qt::NoBrush);
            p.setPen(pen);
            p.drawRoundedRect(left, 2, 2);
            p.drawRoundedRect(right, 2, 2);

            const QPointF c(size * 0.50, size * 0.50);
            p.setPen(accentPen);
            p.drawLine(left.center(), c);
            p.drawLine(right.center(), c);

            p.drawLine(c, QPointF(c.x() - size * 0.13, c.y() - size * 0.08));
            p.drawLine(c, QPointF(c.x() - size * 0.13, c.y() + size * 0.08));
            p.drawLine(c, QPointF(c.x() + size * 0.13, c.y() - size * 0.08));
            p.drawLine(c, QPointF(c.x() + size * 0.13, c.y() + size * 0.08));
            break;
        }
        case ViewportIcon::actionDelete:
        {
            QPen deletePen(QColor(245, 105, 105));
            deletePen.setWidthF(3.3);
            deletePen.setCapStyle(Qt::RoundCap);
            deletePen.setJoinStyle(Qt::RoundJoin);
            p.setPen(deletePen);

            p.drawLine(QPointF(size * 0.22, size * 0.22), QPointF(size * 0.78, size * 0.78));
            p.drawLine(QPointF(size * 0.78, size * 0.22), QPointF(size * 0.22, size * 0.78));
            break;
        }
        case ViewportIcon::actionCopy:
        {
            QPen copyPen(QColor(135, 196, 255));
            copyPen.setWidthF(2.4);
            copyPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(copyPen);
            p.setBrush(QColor(135, 196, 255, 30));
            p.drawRoundedRect(QRectF(size * 0.30, size * 0.18, size * 0.49, size * 0.55), 2.5, 2.5);
            p.setBrush(QColor(20, 24, 31));
            p.drawRoundedRect(QRectF(size * 0.18, size * 0.31, size * 0.49, size * 0.55), 2.5, 2.5);
            break;
        }
        case ViewportIcon::actionPaste:
        {
            QPen pastePen(QColor(240, 193, 103));
            pastePen.setWidthF(2.3);
            pastePen.setCapStyle(Qt::RoundCap);
            pastePen.setJoinStyle(Qt::RoundJoin);
            p.setPen(pastePen);
            p.setBrush(QColor(240, 193, 103, 28));
            p.drawRoundedRect(QRectF(size * 0.20, size * 0.22, size * 0.60, size * 0.64), 2.5, 2.5);
            p.setBrush(QColor(20, 24, 31));
            p.drawRoundedRect(QRectF(size * 0.35, size * 0.13, size * 0.30, size * 0.18), 2.0, 2.0);
            p.drawLine(QPointF(size * 0.32, size * 0.48), QPointF(size * 0.68, size * 0.48));
            p.drawLine(QPointF(size * 0.32, size * 0.64), QPointF(size * 0.61, size * 0.64));
            break;
        }
        case ViewportIcon::rangeLinked:
        case ViewportIcon::rangeUnlinked:
        {
            const bool linked = icon == ViewportIcon::rangeLinked;
            QPen linkPen(linked ? QColor(128, 220, 158) : QColor(242, 158, 100));
            linkPen.setWidthF(2.5);
            linkPen.setCapStyle(Qt::RoundCap);
            linkPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(linkPen);
            p.setBrush(Qt::NoBrush);

            const QRectF top = linked
                ? QRectF(size * 0.33, size * 0.10, size * 0.34, size * 0.46)
                : QRectF(size * 0.33, size * 0.04, size * 0.34, size * 0.40);
            const QRectF bottom = linked
                ? QRectF(size * 0.33, size * 0.44, size * 0.34, size * 0.46)
                : QRectF(size * 0.33, size * 0.56, size * 0.34, size * 0.40);
            p.drawRoundedRect(top, size * 0.16, size * 0.16);
            p.drawRoundedRect(bottom, size * 0.16, size * 0.16);
            if (linked)
            {
                p.drawLine(QPointF(size * 0.50, size * 0.39), QPointF(size * 0.50, size * 0.61));
            }
            else
            {
                p.drawLine(QPointF(size * 0.28, size * 0.42), QPointF(size * 0.20, size * 0.47));
                p.drawLine(QPointF(size * 0.80, size * 0.53), QPointF(size * 0.72, size * 0.58));
            }
            break;
        }
        case ViewportIcon::actionAddCube:
        {
            QPen accentPen(QColor(120, 220, 145));
            accentPen.setWidthF(2.8);
            accentPen.setCapStyle(Qt::RoundCap);
            accentPen.setJoinStyle(Qt::RoundJoin);

            const QRectF front(size * 0.20, size * 0.34, size * 0.40, size * 0.40);
            const QPointF offset(size * 0.14, -size * 0.14);
            const QRectF back = front.translated(offset);
            p.setBrush(Qt::NoBrush);
            p.setPen(pen);
            p.drawRoundedRect(back, 2, 2);
            p.drawRoundedRect(front, 2, 2);
            p.drawLine(front.topLeft(), back.topLeft());
            p.drawLine(front.topRight(), back.topRight());
            p.drawLine(front.bottomRight(), back.bottomRight());

            p.setPen(accentPen);
            const QPointF c(size * 0.70, size * 0.71);
            p.drawLine(QPointF(c.x(), c.y() - size * 0.12), QPointF(c.x(), c.y() + size * 0.12));
            p.drawLine(QPointF(c.x() - size * 0.12, c.y()), QPointF(c.x() + size * 0.12, c.y()));
            break;
        }
        case ViewportIcon::actionAddThing:
        {
            QPen accentPen(QColor(115, 205, 255));
            accentPen.setWidthF(2.8);
            accentPen.setCapStyle(Qt::RoundCap);
            accentPen.setJoinStyle(Qt::RoundJoin);

            const QRectF body(size * 0.25, size * 0.23, size * 0.34, size * 0.42);
            p.setBrush(Qt::NoBrush);
            p.setPen(pen);
            p.drawRoundedRect(body, 4, 4);
            p.drawLine(QPointF(body.center().x(), body.bottom()), QPointF(body.center().x(), size * 0.80));
            p.drawLine(QPointF(body.center().x(), size * 0.80), QPointF(size * 0.33, size * 0.88));
            p.drawLine(QPointF(body.center().x(), size * 0.80), QPointF(size * 0.58, size * 0.88));

            p.setPen(Qt::NoPen);
            p.setBrush(fg);
            p.drawEllipse(QPointF(body.center().x(), size * 0.17), size * 0.08, size * 0.08);

            p.setPen(accentPen);
            const QPointF c(size * 0.72, size * 0.30);
            p.drawLine(QPointF(c.x(), c.y() - size * 0.11), QPointF(c.x(), c.y() + size * 0.11));
            p.drawLine(QPointF(c.x() - size * 0.11, c.y()), QPointF(c.x() + size * 0.11, c.y()));
            break;
        }
        case ViewportIcon::actionAddLight:
        {
            QPen accentPen(QColor(255, 220, 95));
            accentPen.setWidthF(2.8);
            accentPen.setCapStyle(Qt::RoundCap);
            accentPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(accentPen);

            const QRectF glow(size * 0.22, size * 0.18, size * 0.42, size * 0.42);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(glow);
            p.drawLine(QPointF(size * 0.35, size * 0.63), QPointF(size * 0.51, size * 0.63));
            p.drawLine(QPointF(size * 0.37, size * 0.71), QPointF(size * 0.49, size * 0.71));
            p.drawLine(QPointF(size * 0.43, size * 0.60), QPointF(size * 0.43, size * 0.72));

            const QPointF c(size * 0.73, size * 0.33);
            p.drawLine(QPointF(c.x(), c.y() - size * 0.11), QPointF(c.x(), c.y() + size * 0.11));
            p.drawLine(QPointF(c.x() - size * 0.11, c.y()), QPointF(c.x() + size * 0.11, c.y()));
            break;
        }
        case ViewportIcon::actionExtrude:
        {
            QPen accentPen(QColor(115, 205, 255));
            accentPen.setWidthF(3.0);
            accentPen.setCapStyle(Qt::RoundCap);
            accentPen.setJoinStyle(Qt::RoundJoin);

            const QRectF front(size * 0.20, size * 0.48, size * 0.44, size * 0.32);
            const QPointF offset(size * 0.17, -size * 0.19);
            const QRectF back = front.translated(offset);

            p.setBrush(QColor(115, 205, 255, 42));
            p.setPen(pen);
            p.drawRoundedRect(back, 2.0, 2.0);
            p.drawRoundedRect(front, 2.0, 2.0);
            p.drawLine(front.topLeft(), back.topLeft());
            p.drawLine(front.topRight(), back.topRight());
            p.drawLine(front.bottomRight(), back.bottomRight());

            p.setPen(accentPen);
            const QPointF start(size * 0.30, size * 0.70);
            const QPointF end(size * 0.72, size * 0.25);
            p.drawLine(start, end);
            p.drawLine(end, QPointF(end.x() - size * 0.16, end.y() + size * 0.02));
            p.drawLine(end, QPointF(end.x() - size * 0.02, end.y() + size * 0.16));
            break;
        }
        case ViewportIcon::actionFlip:
        {
            QPen accentPen(QColor(250, 190, 92));
            accentPen.setWidthF(2.7);
            accentPen.setCapStyle(Qt::RoundCap);
            accentPen.setJoinStyle(Qt::RoundJoin);

            const qreal cx = size * 0.50;
            p.setPen(accentPen);
            p.drawLine(QPointF(cx, size * 0.16), QPointF(cx, size * 0.84));

            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            QPolygonF left;
            left << QPointF(size * 0.18, size * 0.34)
                 << QPointF(size * 0.39, size * 0.23)
                 << QPointF(size * 0.39, size * 0.45);
            QPolygonF right;
            right << QPointF(size * 0.82, size * 0.66)
                  << QPointF(size * 0.61, size * 0.55)
                  << QPointF(size * 0.61, size * 0.77);
            p.drawPolygon(left);
            p.drawPolygon(right);

            p.drawArc(QRectF(size * 0.15, size * 0.49, size * 0.28, size * 0.28), 35 * 16, 185 * 16);
            p.drawArc(QRectF(size * 0.57, size * 0.23, size * 0.28, size * 0.28), 215 * 16, 185 * 16);
            break;
        }
        case ViewportIcon::actionAdjoin:
        {
            QPen accentPen(QColor(120, 220, 145));
            accentPen.setWidthF(3.0);
            accentPen.setCapStyle(Qt::RoundCap);
            accentPen.setJoinStyle(Qt::RoundJoin);

            const QRectF left(size * 0.14, size * 0.28, size * 0.26, size * 0.44);
            const QRectF right(size * 0.60, size * 0.28, size * 0.26, size * 0.44);

            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(left, 2, 2);
            p.drawRoundedRect(right, 2, 2);

            p.setPen(accentPen);
            p.drawLine(QPointF(left.right(), left.center().y()), QPointF(right.left(), right.center().y()));
            p.drawLine(QPointF(size * 0.50, size * 0.38), QPointF(size * 0.50, size * 0.62));
            break;
        }
        case ViewportIcon::actionUnadjoin:
        {
            QPen accentPen(QColor(240, 105, 105));
            accentPen.setWidthF(3.0);
            accentPen.setCapStyle(Qt::RoundCap);
            accentPen.setJoinStyle(Qt::RoundJoin);

            const QRectF left(size * 0.14, size * 0.28, size * 0.26, size * 0.44);
            const QRectF right(size * 0.60, size * 0.28, size * 0.26, size * 0.44);

            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(left, 2, 2);
            p.drawRoundedRect(right, 2, 2);

            const QPointF a(left.right(), left.center().y());
            const QPointF b(right.left(), right.center().y());
            p.drawLine(a, QPointF(size * 0.43, a.y()));
            p.drawLine(QPointF(size * 0.57, b.y()), b);

            p.setPen(accentPen);
            p.drawLine(QPointF(size * 0.40, size * 0.64), QPointF(size * 0.60, size * 0.36));
            break;
        }
        case ViewportIcon::actionUndo:
        case ViewportIcon::actionRedo:
        {
            if ( icon == ViewportIcon::actionRedo )
            {
                p.translate(size, 0);
                p.scale(-1, 1);
            }
            QPen historyPen(fg);
            historyPen.setWidthF(2.2);
            historyPen.setCapStyle(Qt::RoundCap);
            historyPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(historyPen);
            p.setBrush(Qt::NoBrush);
            QPainterPath arrow;
            arrow.moveTo(size * 0.22, size * 0.35);
            arrow.lineTo(size * 0.57, size * 0.35);
            arrow.cubicTo(size * 0.89, size * 0.35,
                size * 0.89, size * 0.79, size * 0.51, size * 0.79);
            p.drawPath(arrow);
            historyPen.setColor(QColor(115, 205, 255));
            p.setPen(historyPen);
            p.drawLine(QPointF(size * 0.22, size * 0.35), QPointF(size * 0.41, size * 0.17));
            p.drawLine(QPointF(size * 0.22, size * 0.35), QPointF(size * 0.41, size * 0.53));
            break;
        }
        case ViewportIcon::actionSave:
        {
            QPen iconPen(fg);
            iconPen.setWidthF(2.2);
            iconPen.setCapStyle(Qt::RoundCap);
            iconPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(iconPen);
            p.setBrush(QColor(fg.red(), fg.green(), fg.blue(), 18));
            QPainterPath disk;
            disk.moveTo(size * 0.18, size * 0.14);
            disk.lineTo(size * 0.70, size * 0.14);
            disk.lineTo(size * 0.82, size * 0.26);
            disk.lineTo(size * 0.82, size * 0.86);
            disk.lineTo(size * 0.18, size * 0.86);
            disk.closeSubpath();
            p.drawPath(disk);
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(size * 0.29, size * 0.56, size * 0.42, size * 0.30), 1.5, 1.5);
            iconPen.setColor(QColor(115, 205, 255));
            p.setPen(iconPen);
            p.drawRect(QRectF(size * 0.30, size * 0.14, size * 0.30, size * 0.25));
            p.drawLine(QPointF(size * 0.52, size * 0.18), QPointF(size * 0.52, size * 0.31));
            break;
        }
        case ViewportIcon::actionReload:
        {
            QPen reloadPen(fg);
            reloadPen.setWidthF(2.2);
            reloadPen.setCapStyle(Qt::RoundCap);
            reloadPen.setJoinStyle(Qt::RoundJoin);
            p.setBrush(Qt::NoBrush);
            p.setPen(reloadPen);
            p.drawArc(QRectF(size * 0.18, size * 0.18, size * 0.64, size * 0.64), 22 * 16, 288 * 16);
            reloadPen.setColor(QColor(115, 205, 255));
            p.setPen(reloadPen);
            p.drawLine(QPointF(size * 0.77, size * 0.21), QPointF(size * 0.80, size * 0.38));
            p.drawLine(QPointF(size * 0.80, size * 0.38), QPointF(size * 0.65, size * 0.35));
            break;
        }
        case ViewportIcon::bakeRecalculate:
        {
            QPen refreshPen(fg);
            refreshPen.setWidthF(2.2);
            refreshPen.setCapStyle(Qt::RoundCap);
            refreshPen.setJoinStyle(Qt::RoundJoin);
            p.setBrush(Qt::NoBrush);
            p.setPen(refreshPen);
            p.drawArc(QRectF(size * 0.10, size * 0.10, size * 0.80, size * 0.80), 25 * 16, 280 * 16);
            refreshPen.setColor(QColor(115, 205, 255));
            p.setPen(refreshPen);
            p.drawLine(QPointF(size * 0.81, size * 0.17), QPointF(size * 0.87, size * 0.34));
            p.drawLine(QPointF(size * 0.87, size * 0.34), QPointF(size * 0.70, size * 0.31));
            QPen lightPen(QColor(255, 215, 95));
            lightPen.setWidthF(2.1);
            lightPen.setCapStyle(Qt::RoundCap);
            p.setPen(lightPen);
            QPainterPath bulb;
            bulb.moveTo(size * 0.43, size * 0.62);
            bulb.cubicTo(size * 0.43, size * 0.53, size * 0.34, size * 0.52, size * 0.34, size * 0.42);
            bulb.cubicTo(size * 0.34, size * 0.23, size * 0.66, size * 0.23, size * 0.66, size * 0.42);
            bulb.cubicTo(size * 0.66, size * 0.52, size * 0.57, size * 0.53, size * 0.57, size * 0.62);
            p.drawPath(bulb);
            p.drawLine(QPointF(size * 0.43, size * 0.63), QPointF(size * 0.57, size * 0.63));
            p.drawLine(QPointF(size * 0.45, size * 0.71), QPointF(size * 0.55, size * 0.71));
            break;
        }
        case ViewportIcon::actionWhitespace:
        {
            QPen whitespacePen(fg);
            whitespacePen.setWidthF(2.2);
            whitespacePen.setCapStyle(Qt::RoundCap);
            whitespacePen.setJoinStyle(Qt::RoundJoin);
            p.setPen(whitespacePen);
            p.setBrush(fg);
            for ( const qreal y : { size * 0.29, size * 0.50, size * 0.71 } )
            {
                p.drawEllipse(QPointF(size * 0.22, y), size * 0.035, size * 0.035);
            }
            p.drawLine(QPointF(size * 0.34, size * 0.29), QPointF(size * 0.78, size * 0.29));
            p.drawLine(QPointF(size * 0.34, size * 0.71), QPointF(size * 0.78, size * 0.71));
            whitespacePen.setColor(QColor(115, 205, 255));
            p.setPen(whitespacePen);
            p.drawLine(QPointF(size * 0.34, size * 0.50), QPointF(size * 0.79, size * 0.50));
            p.drawLine(QPointF(size * 0.67, size * 0.38), QPointF(size * 0.79, size * 0.50));
            p.drawLine(QPointF(size * 0.79, size * 0.50), QPointF(size * 0.67, size * 0.62));
            break;
        }
        case ViewportIcon::actionFormat:
        {
            QPen formatPen(fg);
            formatPen.setWidthF(2.2);
            formatPen.setCapStyle(Qt::RoundCap);
            formatPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(formatPen);
            p.setBrush(Qt::NoBrush);
            p.drawLine(QPointF(size * 0.28, size * 0.24), QPointF(size * 0.80, size * 0.24));
            p.drawLine(QPointF(size * 0.43, size * 0.50), QPointF(size * 0.80, size * 0.50));
            p.drawLine(QPointF(size * 0.28, size * 0.76), QPointF(size * 0.80, size * 0.76));
            formatPen.setColor(QColor(115, 205, 255));
            p.setPen(formatPen);
            p.drawLine(QPointF(size * 0.12, size * 0.50), QPointF(size * 0.29, size * 0.50));
            p.drawLine(QPointF(size * 0.16, size * 0.37), QPointF(size * 0.29, size * 0.50));
            p.drawLine(QPointF(size * 0.29, size * 0.50), QPointF(size * 0.16, size * 0.63));
            break;
        }
        case ViewportIcon::actionInsert:
        {
            QPen insertPen(fg);
            insertPen.setWidthF(2.2);
            insertPen.setCapStyle(Qt::RoundCap);
            insertPen.setJoinStyle(Qt::RoundJoin);
            p.setPen(insertPen);
            p.setBrush(Qt::NoBrush);
            QPainterPath braces;
            braces.moveTo(size * 0.34, size * 0.15);
            braces.cubicTo(size * 0.19, size * 0.15, size * 0.28, size * 0.36, size * 0.15, size * 0.50);
            braces.cubicTo(size * 0.28, size * 0.64, size * 0.19, size * 0.85, size * 0.34, size * 0.85);
            braces.moveTo(size * 0.66, size * 0.15);
            braces.cubicTo(size * 0.81, size * 0.15, size * 0.72, size * 0.36, size * 0.85, size * 0.50);
            braces.cubicTo(size * 0.72, size * 0.64, size * 0.81, size * 0.85, size * 0.66, size * 0.85);
            p.drawPath(braces);
            insertPen.setColor(QColor(115, 205, 255));
            p.setPen(insertPen);
            p.drawLine(QPointF(size * 0.50, size * 0.36), QPointF(size * 0.50, size * 0.64));
            p.drawLine(QPointF(size * 0.36, size * 0.50), QPointF(size * 0.64, size * 0.50));
            break;
        }
        case ViewportIcon::resourceNew:
        {
            const QRectF sheet(size * 0.19, size * 0.12, size * 0.49, size * 0.72);
            p.setBrush(QColor(fg.red(), fg.green(), fg.blue(), 28));
            p.setPen(pen);
            p.drawRoundedRect(sheet, size * 0.045, size * 0.045);
            p.drawLine(
                QPointF(sheet.left() + size * 0.11, sheet.top() + size * 0.22),
                QPointF(sheet.right() - size * 0.10, sheet.top() + size * 0.22));
            p.drawLine(
                QPointF(sheet.left() + size * 0.11, sheet.top() + size * 0.36),
                QPointF(sheet.right() - size * 0.17, sheet.top() + size * 0.36));

            QPen plusPen(QColor(115, 205, 255));
            plusPen.setWidthF(3.0);
            plusPen.setCapStyle(Qt::RoundCap);
            p.setPen(plusPen);
            const QPointF center(size * 0.70, size * 0.70);
            p.drawLine(QPointF(center.x(), center.y() - size * 0.19),
                QPointF(center.x(), center.y() + size * 0.19));
            p.drawLine(QPointF(center.x() - size * 0.19, center.y()),
                QPointF(center.x() + size * 0.19, center.y()));
            break;
        }
        case ViewportIcon::resourceOpen:
        {
            const QRectF folder(size * 0.10, size * 0.27, size * 0.78, size * 0.53);
            p.setBrush(QColor(115, 205, 255, 38));
            p.setPen(pen);
            QPainterPath folderPath;
            folderPath.moveTo(folder.left(), folder.top() + size * 0.07);
            folderPath.lineTo(folder.left(), folder.top() - size * 0.08);
            folderPath.lineTo(folder.left() + size * 0.34, folder.top() - size * 0.08);
            folderPath.lineTo(folder.left() + size * 0.45, folder.top() + size * 0.07);
            folderPath.lineTo(folder.right(), folder.top() + size * 0.07);
            folderPath.lineTo(folder.right(), folder.bottom());
            folderPath.lineTo(folder.left(), folder.bottom());
            folderPath.closeSubpath();
            p.drawPath(folderPath);

            break;
        }
        case ViewportIcon::transformLocal:
        {
            QPen xPen(QColor(245, 90, 90));
            xPen.setWidthF(3.0);
            xPen.setCapStyle(Qt::RoundCap);
            xPen.setJoinStyle(Qt::RoundJoin);
            QPen yPen(QColor(105, 210, 120));
            yPen.setWidthF(3.0);
            yPen.setCapStyle(Qt::RoundCap);
            yPen.setJoinStyle(Qt::RoundJoin);

            const QPointF o(size * 0.31, size * 0.74);
            p.save();
            p.translate(o);
            p.rotate(-25.0);
            p.translate(-o);

            const QPointF xEnd(o.x() + size * 0.56, o.y());
            const QPointF yEnd(o.x(), o.y() - size * 0.56);
            p.setBrush(Qt::NoBrush);
            p.setPen(xPen);
            p.drawLine(o, xEnd);
            p.drawLine(xEnd, QPointF(xEnd.x() - size * 0.12, xEnd.y() - size * 0.07));
            p.drawLine(xEnd, QPointF(xEnd.x() - size * 0.12, xEnd.y() + size * 0.07));

            p.setPen(yPen);
            p.drawLine(o, yEnd);
            p.drawLine(yEnd, QPointF(yEnd.x() - size * 0.07, yEnd.y() + size * 0.12));
            p.drawLine(yEnd, QPointF(yEnd.x() + size * 0.07, yEnd.y() + size * 0.12));

            p.restore();

            p.setPen(Qt::NoPen);
            p.setBrush(fg);
            p.drawEllipse(o, size * 0.055, size * 0.055);
            break;
        }
        case ViewportIcon::transformWorld:
        {
            QPen xPen(QColor(245, 90, 90));
            xPen.setWidthF(3.0);
            xPen.setCapStyle(Qt::RoundCap);
            xPen.setJoinStyle(Qt::RoundJoin);
            QPen yPen(QColor(105, 210, 120));
            yPen.setWidthF(3.0);
            yPen.setCapStyle(Qt::RoundCap);
            yPen.setJoinStyle(Qt::RoundJoin);

            const QPointF o(size * 0.24, size * 0.78);
            const QPointF xEnd(o.x() + size * 0.64, o.y());
            const QPointF yEnd(o.x(), o.y() - size * 0.64);
            p.setBrush(Qt::NoBrush);
            p.setPen(xPen);
            p.drawLine(o, xEnd);
            p.drawLine(xEnd, QPointF(xEnd.x() - size * 0.12, xEnd.y() - size * 0.07));
            p.drawLine(xEnd, QPointF(xEnd.x() - size * 0.12, xEnd.y() + size * 0.07));

            p.setPen(yPen);
            p.drawLine(o, yEnd);
            p.drawLine(yEnd, QPointF(yEnd.x() - size * 0.07, yEnd.y() + size * 0.12));
            p.drawLine(yEnd, QPointF(yEnd.x() + size * 0.07, yEnd.y() + size * 0.12));

            p.setPen(Qt::NoPen);
            p.setBrush(fg);
            p.drawEllipse(o, size * 0.055, size * 0.055);
            break;
        }
    }

    p.end();

    QIcon result;
    result.addPixmap(pix, QIcon::Normal, QIcon::Off);
    result.addPixmap(pix, QIcon::Normal, QIcon::On);
    result.addPixmap(makeDisabledViewportIconPixmap(pix), QIcon::Disabled, QIcon::Off);
    result.addPixmap(makeDisabledViewportIconPixmap(pix), QIcon::Disabled, QIcon::On);
    return result;
}

void configureDocumentHistoryButton(
    QToolButton* const button,
    QAction* const action,
    const bool redo)
{
    if ( !button )
    {
        return;
    }
    const QString label = redo ? QStringLiteral("Redo") : QStringLiteral("Undo");
    button->setIcon(makeViewportIcon(redo ? ViewportIcon::actionRedo : ViewportIcon::actionUndo));
    button->setIconSize(QSize(20, 20));
    button->setFixedSize(30, 30);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setAccessibleName(label);
    const auto updateTooltip = [button, action, label]()
    {
        QStringList bindings{};
        if ( action )
        {
            for ( const QKeySequence& binding : action->shortcuts() )
            {
                const QString text = shortcuts::keySequenceDisplayText(binding);
                if ( !text.isEmpty() )
                {
                    bindings.push_back(text);
                }
            }
        }
        button->setToolTip(bindings.isEmpty() ? label
            : QStringLiteral("%1 (%2)").arg(label, bindings.join(QStringLiteral(", "))));
    };
    updateTooltip();
    if ( action )
    {
        QObject::connect(action, &QAction::changed, button, updateTooltip);
    }
}

QIcon makeCrossedOutViewportIcon(const QIcon& icon)
{
    constexpr int size = 32;
    QPixmap pix = icon.pixmap(size, size, QIcon::Normal, QIcon::Off);
    if (pix.isNull())
    {
        return icon;
    }

    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Keep the current mode recognizable and add a compact status badge
    // instead of striking through the whole glyph.
    const QPointF center(size * 0.76, size * 0.76);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(28, 29, 34, 235));
    painter.drawEllipse(center, size * 0.20, size * 0.20);

    QPen crossPen(QColor(247, 92, 92));
    crossPen.setWidthF(3.0);
    crossPen.setCapStyle(Qt::RoundCap);
    painter.setPen(crossPen);
    painter.drawLine(QPointF(size * 0.67, size * 0.67), QPointF(size * 0.85, size * 0.85));
    painter.drawLine(QPointF(size * 0.85, size * 0.67), QPointF(size * 0.67, size * 0.85));
    painter.end();

    QIcon result;
    result.addPixmap(pix, QIcon::Normal, QIcon::Off);
    result.addPixmap(pix, QIcon::Normal, QIcon::On);
    result.addPixmap(makeDisabledViewportIconPixmap(pix), QIcon::Disabled, QIcon::Off);
    result.addPixmap(makeDisabledViewportIconPixmap(pix), QIcon::Disabled, QIcon::On);
    return result;
}

} // namespace sed::ui
