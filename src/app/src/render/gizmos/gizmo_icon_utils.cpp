#include "render/gizmos/gizmo_icon_utils.hpp"

#include "sed/text.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <numbers>

#include <QPainter>
#include <QPainterPath>
#include <QPen>

namespace sed::render
{
namespace
{

struct GizmoColorRule
{
    std::string_view type;
    QColor color;
};

const std::array<GizmoColorRule, 15> kBuiltInGizmoTypeColors{
    GizmoColorRule{ "camera", QColor(77, 153, 255) },
    GizmoColorRule{ "actor", QColor(230, 89, 51) },
    GizmoColorRule{ "weapon", QColor(242, 153, 38) },
    GizmoColorRule{ "item", QColor(51, 217, 77) },
    GizmoColorRule{ "player", QColor(26, 230, 230) },
    GizmoColorRule{ "cog", QColor(204, 128, 230) },
    GizmoColorRule{ "ghost", QColor(153, 153, 166) },
    GizmoColorRule{ "corpse", QColor(140, 89, 51) },
    GizmoColorRule{ "explosion", QColor(255, 102, 26) },
    GizmoColorRule{ "particle", QColor(242, 242, 77) },
    GizmoColorRule{ "debris", QColor(166, 128, 89) },
    GizmoColorRule{ "hint", QColor(102, 230, 204) },
    GizmoColorRule{ "sprite", QColor(217, 179, 217) },
    GizmoColorRule{ "polyline", QColor(191, 191, 102) },
    GizmoColorRule{ "free", QColor(128, 128, 128) },
};

const QColor kFallbackGizmoTypeColor{ 179, 179, 179 };

} // namespace

QColor gizmoIconColorForType(const std::string_view type, const GizmoTypeColorMap& userTypeColors)
{
    const std::string_view trimmed = sed::text::trimView(type);

    if (!userTypeColors.empty())
    {
        for (const auto& entry : userTypeColors)
        {
            if (sed::text::iequalsAscii(entry.first, trimmed))
            {
                return entry.second;
            }
        }

        for (const auto& entry : userTypeColors)
        {
            if (sed::text::iequalsAscii(entry.first, "default"))
            {
                return entry.second;
            }
        }
    }

    for (const GizmoColorRule& rule : kBuiltInGizmoTypeColors)
    {
        if (sed::text::iequalsAscii(trimmed, rule.type))
        {
            return rule.color;
        }
    }

    return kFallbackGizmoTypeColor;
}

QString gizmoIconLabelForType(const std::string_view type)
{
    QString trimmed = QString::fromUtf8(type.data(), static_cast<qsizetype>(type.size())).trimmed();
    if (trimmed.isEmpty())
    {
        return QStringLiteral("?");
    }

    QString cleaned{};
    cleaned.reserve(trimmed.size());
    for (const QChar ch : trimmed)
    {
        if (ch.isLetterOrNumber())
        {
            cleaned.append(ch);
        }
    }

    if (cleaned.isEmpty())
    {
        cleaned = trimmed;
    }

    cleaned = cleaned.left(2).toUpper();
    return cleaned.isEmpty() ? QStringLiteral("?") : cleaned;
}

QImage renderThingRegularLightIconImage(QColor tint, const int requestedSize)
{
    const int size = std::max(1, requestedSize);
    if (!tint.isValid())
    {
        tint = Qt::white;
    }
    tint.setAlpha(255);

    QImage image(size, size, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor outline(4, 6, 10, 255);
    const auto drawOutlinedLine = [&](const QPointF& from, const QPointF& to, const qreal width)
    {
        painter.setPen(QPen(
            outline,
            width + size * 0.039,
            Qt::SolidLine,
            Qt::RoundCap,
            Qt::RoundJoin));
        painter.drawLine(from, to);
        painter.setPen(QPen(
            tint,
            width,
            Qt::SolidLine,
            Qt::RoundCap,
            Qt::RoundJoin));
        painter.drawLine(from, to);
    };

    const std::array<std::pair<QPointF, QPointF>, 5> rays{
        std::pair{ QPointF(size * 0.50, size * 0.07), QPointF(size * 0.50, size * 0.17) },
        std::pair{ QPointF(size * 0.23, size * 0.15), QPointF(size * 0.31, size * 0.23) },
        std::pair{ QPointF(size * 0.77, size * 0.15), QPointF(size * 0.69, size * 0.23) },
        std::pair{ QPointF(size * 0.14, size * 0.39), QPointF(size * 0.25, size * 0.39) },
        std::pair{ QPointF(size * 0.86, size * 0.39), QPointF(size * 0.75, size * 0.39) },
    };
    for (const auto& [from, to] : rays)
    {
        drawOutlinedLine(from, to, size * 0.031);
    }

    QPainterPath bulb;
    bulb.moveTo(size * 0.39, size * 0.63);
    bulb.cubicTo(
        size * 0.38, size * 0.57,
        size * 0.29, size * 0.54,
        size * 0.29, size * 0.40);
    bulb.cubicTo(
        size * 0.29, size * 0.24,
        size * 0.38, size * 0.18,
        size * 0.50, size * 0.18);
    bulb.cubicTo(
        size * 0.62, size * 0.18,
        size * 0.71, size * 0.24,
        size * 0.71, size * 0.40);
    bulb.cubicTo(
        size * 0.71, size * 0.54,
        size * 0.62, size * 0.57,
        size * 0.61, size * 0.63);
    bulb.closeSubpath();
    painter.setPen(QPen(
        outline,
        size * 0.063,
        Qt::SolidLine,
        Qt::RoundCap,
        Qt::RoundJoin));
    painter.setBrush(tint);
    painter.drawPath(bulb);

    drawOutlinedLine(
        QPointF(size * 0.43, size * 0.48),
        QPointF(size * 0.47, size * 0.57),
        size * 0.020);
    drawOutlinedLine(
        QPointF(size * 0.57, size * 0.48),
        QPointF(size * 0.53, size * 0.57),
        size * 0.020);
    drawOutlinedLine(
        QPointF(size * 0.39, size * 0.69),
        QPointF(size * 0.61, size * 0.69),
        size * 0.039);
    drawOutlinedLine(
        QPointF(size * 0.40, size * 0.77),
        QPointF(size * 0.60, size * 0.77),
        size * 0.039);
    drawOutlinedLine(
        QPointF(size * 0.44, size * 0.85),
        QPointF(size * 0.56, size * 0.85),
        size * 0.039);

    painter.end();
    return image;
}

QImage renderThingHeadlightIconImage(QColor tint, const int requestedSize)
{
    const int size = std::max(1, requestedSize);
    if (!tint.isValid())
    {
        tint = Qt::white;
    }
    tint.setAlpha(255);

    QImage image(size, size, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor outline(4, 6, 10, 255);
    const QPointF lightCenter(size * 0.69, size * 0.31);
    const qreal innerRayRadius = size * 0.145;
    const qreal outerRayRadius = size * 0.260;

    // A Headlight is an omnidirectional point source attached to an actor,
    // not a spotlight. Draw the rays behind both bodies so the small toolbar
    // and viewport variants still read clearly without implying a beam cone.
    for (int pass = 0; pass < 2; ++pass)
    {
        painter.setPen(QPen(
            pass == 0 ? outline : tint,
            size * (pass == 0 ? 0.075 : 0.038),
            Qt::SolidLine,
            Qt::RoundCap,
            Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        for (int i = 0; i < 8; ++i)
        {
            const double angle = (static_cast<double>(i) / 8.0)
                * (2.0 * std::numbers::pi_v<double>);
            const QPointF direction(std::cos(angle), std::sin(angle));
            painter.drawLine(
                lightCenter + direction * innerRayRadius,
                lightCenter + direction * outerRayRadius);
        }
    }

    QColor lens = tint.lighter(155);
    lens.setAlpha(255);
    painter.setPen(QPen(
        outline,
        size * 0.055,
        Qt::SolidLine,
        Qt::RoundCap,
        Qt::RoundJoin));
    painter.setBrush(lens);
    painter.drawEllipse(lightCenter, size * 0.105, size * 0.105);
    painter.setPen(QPen(tint, size * 0.025, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(lightCenter, size * 0.068, size * 0.068);

    // Keep a bright actor bust in the foreground to make ownership explicit.
    QPainterPath actor;
    actor.addEllipse(QRectF(size * 0.12, size * 0.22, size * 0.34, size * 0.34));
    actor.moveTo(size * 0.07, size * 0.90);
    actor.cubicTo(
        size * 0.10, size * 0.66,
        size * 0.22, size * 0.56,
        size * 0.30, size * 0.56);
    actor.cubicTo(
        size * 0.44, size * 0.56,
        size * 0.55, size * 0.69,
        size * 0.59, size * 0.90);
    actor.closeSubpath();
    painter.setPen(QPen(
        outline,
        size * 0.063,
        Qt::SolidLine,
        Qt::RoundCap,
        Qt::RoundJoin));
    painter.setBrush(tint);
    painter.drawPath(actor);

    painter.end();
    return image;
}

QImage renderSectorPointLightIconImage(QColor tint, const int requestedSize)
{
    const int size = std::max(1, requestedSize);
    if (!tint.isValid())
    {
        tint = Qt::white;
    }
    tint.setAlpha(255);

    QImage image(size, size, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QPointF center(size * 0.5, size * 0.5);
    const qreal innerRadius = size * 0.305;
    const qreal outerRadius = size * 0.445;
    const QColor outline(7, 9, 14, 255);

    // Draw the rays behind the source body. Two passes preserve the authored
    // tint while retaining a dark edge against bright scene geometry.
    for (int pass = 0; pass < 2; ++pass)
    {
        QPen rayPen(
            pass == 0 ? outline : tint,
            size * (pass == 0 ? 0.095 : 0.050),
            Qt::SolidLine,
            Qt::RoundCap,
            Qt::RoundJoin);
        painter.setPen(rayPen);
        painter.setBrush(Qt::NoBrush);
        for (int i = 0; i < 8; ++i)
        {
            const double angle = (static_cast<double>(i) / 8.0)
                * (2.0 * std::numbers::pi_v<double>);
            const QPointF direction(std::cos(angle), std::sin(angle));
            painter.drawLine(
                center + direction * innerRadius,
                center + direction * outerRadius);
        }
    }

    const QRectF sourceRect(
        size * 0.305,
        size * 0.305,
        size * 0.390,
        size * 0.390);
    const qreal cornerRadius = size * 0.070;
    painter.setPen(QPen(
        outline,
        size * 0.095,
        Qt::SolidLine,
        Qt::RoundCap,
        Qt::RoundJoin));
    painter.setBrush(QColor(18, 20, 27, 255));
    painter.drawRoundedRect(sourceRect, cornerRadius, cornerRadius);

    // The reference editor glyph uses a solid colored emitter body.  Keeping
    // this opaque also makes the small toolbar version read as a light source
    // instead of a dark generic cube with a faint color wash.
    QColor sourceFill = tint;
    sourceFill.setAlpha(255);
    painter.setPen(QPen(
        tint,
        size * 0.052,
        Qt::SolidLine,
        Qt::RoundCap,
        Qt::RoundJoin));
    painter.setBrush(sourceFill);
    painter.drawRoundedRect(sourceRect, cornerRadius, cornerRadius);
    painter.end();
    return image;
}

QImage renderArchitecturalLightingIconImage(
    QColor,
    const int requestedSize)
{
    // MotS ARCH owners are Arch Things, so keep this overlay and the
    // JKDF2 Arch Thing marker visually identical.
    return renderArchThingBadgeImage(requestedSize);
}

QImage renderArchThingBadgeImage(const int requestedSize)
{
    const int size = std::max(1, requestedSize);
    QImage image(size, size, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF plate(
        size * 0.08, size * 0.08, size * 0.84, size * 0.84);
    painter.setPen(QPen(
        QColor(19, 25, 34, 255),
        std::max<qreal>(1.0, size * 0.075),
        Qt::SolidLine,
        Qt::RoundCap,
        Qt::RoundJoin));
    painter.setBrush(QColor(255, 197, 45, 255));
    painter.drawRoundedRect(plate, size * 0.16, size * 0.16);

    const qreal cx = size * 0.5;
    const qreal topY = size * 0.25;
    const qreal midY = size * 0.46;
    const qreal lowerY = size * 0.68;
    const qreal leftX = size * 0.28;
    const qreal rightX = size * 0.72;
    const qreal stroke = std::max<qreal>(1.0, size * 0.075);
    painter.setPen(QPen(
        QColor(30, 39, 51, 255), stroke, Qt::SolidLine,
        Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(cx, topY), QPointF(leftX, midY));
    painter.drawLine(QPointF(cx, topY), QPointF(rightX, midY));
    painter.drawLine(QPointF(leftX, midY), QPointF(cx, size * 0.59));
    painter.drawLine(QPointF(rightX, midY), QPointF(cx, size * 0.59));
    painter.drawLine(QPointF(leftX, midY), QPointF(leftX, lowerY));
    painter.drawLine(QPointF(rightX, midY), QPointF(rightX, lowerY));
    painter.drawLine(QPointF(cx, size * 0.59), QPointF(cx, size * 0.80));
    painter.drawLine(QPointF(leftX, lowerY), QPointF(cx, size * 0.80));
    painter.drawLine(QPointF(rightX, lowerY), QPointF(cx, size * 0.80));
    painter.end();
    return image;
}

QImage renderGizmoIconImage(
    const std::string_view type,
    const GizmoTypeColorMap& userTypeColors,
    QColor baseTint,
    const int requestedSize)
{
    const int size = std::max(1, requestedSize);
    if (!baseTint.isValid())
    {
        baseTint = Qt::white;
    }
    baseTint.setAlpha(255);

    QColor frame = baseTint.darker(112);
    frame.setAlpha(255);

    QImage image(size, size, QImage::Format_RGBA8888_Premultiplied);
    image.fill(frame);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QColor ink = gizmoIconColorForType(type, userTypeColors);
    if (!ink.isValid())
    {
        ink = QColor(30, 30, 30);
    }
    ink.setAlpha(235);

    const std::string typeLower = sed::text::lowerAscii(sed::text::trimView(type));
    const qreal pad = size * 0.10;
    const QRectF rect(pad, pad, size - pad * 2.0, size - pad * 2.0);
    const qreal radius = size * 0.20;

    painter.setPen(QPen(QColor(0, 0, 0, 200), std::max<qreal>(2.0, size * 0.06), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(baseTint);
    painter.drawRoundedRect(rect, radius, radius);

    const qreal strokeW = std::max<qreal>(2.0, size * 0.040);

    auto drawIcon = [&]() -> bool
    {
        const QRectF iconRect = rect.adjusted(rect.width() * 0.10, rect.height() * 0.10, -rect.width() * 0.10, -rect.height() * 0.10);
        const QPointF center = iconRect.center();

        auto setStroke = [&]()
        {
            painter.setPen(QPen(ink, strokeW, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
        };

        auto drawGear = [&](const QPointF& gearCenter, const qreal rad) -> void
        {
            constexpr int kTeeth = 8;
            const qreal toothW = rad * 0.38;
            const qreal toothH = rad * 0.34;
            const qreal bodyR = rad * 0.62;
            const qreal holeR = rad * 0.24;

            painter.save();
            painter.translate(gearCenter);

            setStroke();
            for (int i = 0; i < kTeeth; ++i)
            {
                painter.save();
                painter.rotate(static_cast<qreal>(i) * (360.0 / static_cast<qreal>(kTeeth)));
                const QRectF toothRect(-toothW * 0.5, -rad, toothW, toothH);
                painter.drawRoundedRect(toothRect, toothW * 0.18, toothW * 0.18);
                painter.restore();
            }

            painter.drawEllipse(QPointF(0.0, 0.0), bodyR, bodyR);

            painter.setPen(Qt::NoPen);
            painter.setBrush(baseTint);
            painter.drawEllipse(QPointF(0.0, 0.0), holeR, holeR);

            painter.restore();
        };

        if (typeLower == "camera")
        {
            setStroke();
            const QRectF body(
                iconRect.left() + iconRect.width() * 0.10,
                iconRect.top() + iconRect.height() * 0.28,
                iconRect.width() * 0.80,
                iconRect.height() * 0.52);
            painter.drawRoundedRect(body, body.height() * 0.18, body.height() * 0.18);

            const QRectF top(
                body.left() + body.width() * 0.12,
                body.top() - body.height() * 0.20,
                body.width() * 0.28,
                body.height() * 0.22);
            painter.drawRoundedRect(top, top.height() * 0.25, top.height() * 0.25);

            const qreal lensR = std::min(body.width(), body.height()) * 0.22;
            painter.drawEllipse(body.center(), lensR, lensR);
            painter.drawEllipse(body.center(), lensR * 0.45, lensR * 0.45);
            return true;
        }

        if (typeLower == "ghost")
        {
            setStroke();
            QColor fill = ink;
            fill.setAlpha(32);
            painter.setBrush(fill);

            const qreal w = iconRect.width() * 0.68;
            const qreal h = iconRect.height() * 0.76;
            const qreal topY = center.y() - h * 0.42;
            const qreal bottomY = center.y() + h * 0.46;
            const qreal leftX = center.x() - w * 0.50;
            const qreal rightX = center.x() + w * 0.50;

            QPainterPath path;
            path.moveTo(leftX, center.y());
            path.quadTo(leftX, topY, center.x(), topY);
            path.quadTo(rightX, topY, rightX, center.y());

            const qreal wave = h * 0.16;
            path.lineTo(rightX, bottomY - wave);
            path.quadTo(center.x() + w * 0.34, bottomY, center.x() + w * 0.10, bottomY - wave);
            path.quadTo(center.x(), bottomY, center.x() - w * 0.10, bottomY - wave);
            path.quadTo(center.x() - w * 0.34, bottomY, leftX, bottomY - wave);
            path.closeSubpath();

            painter.drawPath(path);
            return true;
        }

        if (typeLower == "cog" || typeLower == "cogs")
        {
            const qreal rad0 = std::min(iconRect.width(), iconRect.height()) * 0.30;
            const qreal rad1 = rad0 * 0.82;
            drawGear(QPointF(center.x() - rad0 * 0.28, center.y() + rad0 * 0.10), rad0);
            drawGear(QPointF(center.x() + rad0 * 0.42, center.y() - rad0 * 0.25), rad1);
            return true;
        }

        if (typeLower == "hint")
        {
            setStroke();
            const qreal bulbR = std::min(iconRect.width(), iconRect.height()) * 0.26;
            const QPointF bulbCenter(center.x(), center.y() - bulbR * 0.10);
            painter.drawEllipse(bulbCenter, bulbR, bulbR);

            const QRectF base(
                bulbCenter.x() - bulbR * 0.45,
                bulbCenter.y() + bulbR * 0.62,
                bulbR * 0.90,
                bulbR * 0.58);
            painter.drawRoundedRect(base, base.height() * 0.25, base.height() * 0.25);
            painter.drawLine(
                QPointF(bulbCenter.x() - bulbR * 0.28, bulbCenter.y() + bulbR * 0.05),
                QPointF(bulbCenter.x() + bulbR * 0.28, bulbCenter.y() + bulbR * 0.05));
            return true;
        }

        if (typeLower == "weapon" || typeLower == "projectile")
        {
            setStroke();
            const qreal w = iconRect.width() * 0.62;
            const qreal h = iconRect.height() * 0.22;
            const QRectF shaft(center.x() - w * 0.35, center.y() - h * 0.30, w * 0.55, h);
            painter.drawRoundedRect(shaft, h * 0.35, h * 0.35);

            QPolygonF head;
            head << QPointF(center.x() + w * 0.35, center.y())
                 << QPointF(center.x() + w * 0.05, center.y() - h * 0.80)
                 << QPointF(center.x() + w * 0.05, center.y() + h * 0.80);
            painter.drawPolygon(head);
            return true;
        }

        if (typeLower == "polyline")
        {
            setStroke();
            const QPointF p0(iconRect.left() + iconRect.width() * 0.20, iconRect.top() + iconRect.height() * 0.70);
            const QPointF p1(iconRect.left() + iconRect.width() * 0.42, iconRect.top() + iconRect.height() * 0.40);
            const QPointF p2(iconRect.left() + iconRect.width() * 0.62, iconRect.top() + iconRect.height() * 0.58);
            const QPointF p3(iconRect.left() + iconRect.width() * 0.80, iconRect.top() + iconRect.height() * 0.32);
            QPolygonF poly;
            poly << p0 << p1 << p2 << p3;
            painter.drawPolyline(poly);

            painter.setPen(Qt::NoPen);
            painter.setBrush(ink);
            const qreal nodeRadius = strokeW * 0.40;
            painter.drawEllipse(p0, nodeRadius, nodeRadius);
            painter.drawEllipse(p1, nodeRadius, nodeRadius);
            painter.drawEllipse(p2, nodeRadius, nodeRadius);
            painter.drawEllipse(p3, nodeRadius, nodeRadius);
            return true;
        }

        if (typeLower == "debris")
        {
            setStroke();
            QPolygonF a;
            a << QPointF(iconRect.left() + iconRect.width() * 0.24, iconRect.top() + iconRect.height() * 0.62)
              << QPointF(iconRect.left() + iconRect.width() * 0.42, iconRect.top() + iconRect.height() * 0.30)
              << QPointF(iconRect.left() + iconRect.width() * 0.52, iconRect.top() + iconRect.height() * 0.64);
            painter.drawPolygon(a);

            QPolygonF b;
            b << QPointF(iconRect.left() + iconRect.width() * 0.56, iconRect.top() + iconRect.height() * 0.70)
              << QPointF(iconRect.left() + iconRect.width() * 0.76, iconRect.top() + iconRect.height() * 0.44)
              << QPointF(iconRect.left() + iconRect.width() * 0.82, iconRect.top() + iconRect.height() * 0.76);
            painter.drawPolygon(b);
            return true;
        }

        if (typeLower == "particle")
        {
            setStroke();
            const qreal rad = std::min(iconRect.width(), iconRect.height()) * 0.30;
            painter.drawLine(QPointF(center.x() - rad, center.y()), QPointF(center.x() + rad, center.y()));
            painter.drawLine(QPointF(center.x(), center.y() - rad), QPointF(center.x(), center.y() + rad));
            painter.drawLine(QPointF(center.x() - rad * 0.70, center.y() - rad * 0.70), QPointF(center.x() + rad * 0.70, center.y() + rad * 0.70));
            painter.drawLine(QPointF(center.x() + rad * 0.70, center.y() - rad * 0.70), QPointF(center.x() - rad * 0.70, center.y() + rad * 0.70));
            return true;
        }

        return false;
    };

    if (!drawIcon())
    {
        QFont font = painter.font();
        font.setBold(true);
        font.setPointSizeF(size * 0.28);
        painter.setFont(font);

        painter.setPen(ink);
        painter.drawText(rect, Qt::AlignCenter, gizmoIconLabelForType(type));
    }

    painter.end();
    return image;
}

} // namespace sed::render
