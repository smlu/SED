#include "ui/viewport/toast_overlay_painter.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QPainter>
#include <QPen>
#include <QRect>

namespace sed::ui::viewport
{
namespace
{

constexpr int kMarginPx = 10;
constexpr int kToastSpacingPx = 6;

struct ToastStyle
{
    double minPointSize{ 9.0 };
    bool bold{};
    QColor textColor{ 235, 236, 240, 255 };
    QColor bgColor{ 18, 18, 22, 200 };
    QColor borderColor{};
    int padX{ 8 };
    int padY{ 4 };
    int radius{ 6 };
    Qt::Alignment textAlign{ Qt::AlignLeft | Qt::AlignVCenter };
};

struct ActiveToast
{
    const ToastOverlayToast* toast{};
    float alpha{};
    ToastStyle style{};
};

struct MeasuredToast
{
    ActiveToast toast{};
    int bgW{};
    int bgH{};
};

ToastStyle styleForToast(const ToastOverlayGroup group, const ToastOverlayPlacement placement)
{
    ToastStyle style{};
    if (placement != ToastOverlayPlacement::TopLeft)
    {
        style.textAlign = Qt::AlignCenter;
    }

    switch (group)
    {
    case ToastOverlayGroup::FlySpeed:
        style.minPointSize = 12.0;
        style.bold = true;
        style.padX = 12;
        style.padY = 7;
        style.radius = 8;
        style.textAlign = Qt::AlignCenter;
        break;
    case ToastOverlayGroup::Error:
        style.minPointSize = 11.0;
        style.bold = true;
        style.textColor = QColor(255, 120, 120, 255);
        style.bgColor = QColor(40, 12, 12, 210);
        style.padX = 12;
        style.padY = 7;
        style.radius = 8;
        style.textAlign = Qt::AlignCenter;
        break;
    case ToastOverlayGroup::General:
    case ToastOverlayGroup::Projection:
    case ToastOverlayGroup::PlacementYaw:
        break;
    }

    return style;
}

float toastAlpha(
    const std::chrono::steady_clock::time_point now,
    const ToastOverlayToast& toast) noexcept
{
    const int durationMs = std::max(1, toast.durationMs);
    const auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(toast.expires - now).count();
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - toast.shownAt).count();
    const int fadeMs = std::min(200, durationMs / 2);

    if (fadeMs <= 0)
    {
        return 1.0f;
    }

    const float inAlpha = std::clamp(static_cast<float>(elapsedMs) / static_cast<float>(fadeMs), 0.0f, 1.0f);
    const float outAlpha = std::clamp(static_cast<float>(remainingMs) / static_cast<float>(fadeMs), 0.0f, 1.0f);
    return std::min(inAlpha, outAlpha);
}

std::optional<QRect> visibleLeftOverlayRect(QWidget& host, const char* objectName)
{
    if (!objectName || !*objectName)
    {
        return std::nullopt;
    }

    const QWidget* overlay = host.findChild<QWidget*>(QString::fromLatin1(objectName));
    if (!overlay || !overlay->isVisible())
    {
        return std::nullopt;
    }

    const QRect geometry = overlay->geometry();
    if (!geometry.isValid())
    {
        return std::nullopt;
    }

    // Only left-anchored viewport controls should move the top-left toast stack.
    if (geometry.left() > (kMarginPx + 24))
    {
        return std::nullopt;
    }

    return geometry;
}

QSize measureToast(QPainter& painter, const ActiveToast& toast)
{
    QFont font = painter.font();
    font.setPointSizeF(std::max(toast.style.minPointSize, font.pointSizeF()));
    font.setBold(toast.style.bold);
    painter.setFont(font);

    const QFontMetrics fm(font);
    return fm.size(Qt::TextSingleLine, toast.toast->text);
}

void drawToast(QPainter& painter, const ActiveToast& toast, const QRect& bgRect)
{
    const ToastStyle& style = toast.style;

    QColor bg = style.bgColor;
    bg.setAlpha(static_cast<int>(std::lround(toast.alpha * static_cast<float>(bg.alpha()))));

    if ( style.borderColor.isValid() && style.borderColor.alpha() > 0 )
    {
        QColor border = style.borderColor;
        border.setAlpha(static_cast<int>(std::lround(
            toast.alpha * static_cast<float>(border.alpha()))));
        painter.setPen(QPen(border, 1.0));
    }
    else
    {
        painter.setPen(Qt::NoPen);
    }
    painter.setBrush(bg);
    painter.drawRoundedRect(bgRect, style.radius, style.radius);

    QColor text = style.textColor;
    text.setAlpha(static_cast<int>(std::lround(
        toast.alpha * static_cast<float>(text.alpha()))));
    painter.setPen(text);
    painter.drawText(
        bgRect.adjusted(style.padX, style.padY, -style.padX, -style.padY),
        style.textAlign,
        toast.toast->text);
}

void drawTopLeftStack(QWidget& host, QPainter& painter, std::span<const ActiveToast> toasts)
{
    int x = kMarginPx;
    int y = kMarginPx;

    // Step 1: keep projection/general messages below the edit-mode strip.
    if (const std::optional<QRect> editRect = visibleLeftOverlayRect(host, "viewportEditModeOverlay"))
    {
        y = std::max(y, editRect->bottom() + 1 + kMarginPx);
    }

    // Step 2: shift right when the transform/tools panel occupies the same lane.
    if (const std::optional<QRect> transformRect = visibleLeftOverlayRect(host, "viewportTransformOverlay"))
    {
        if (transformRect->top() <= y)
        {
            x = std::max(x, transformRect->right() + 1 + kMarginPx);
        }
    }
    else if (const std::optional<QRect> toolsRect = visibleLeftOverlayRect(host, "viewportToolsOverlay"))
    {
        if (toolsRect->top() <= y)
        {
            x = std::max(x, toolsRect->right() + 1 + kMarginPx);
        }
    }

    // Step 3: paint from top to bottom in insertion/group order.
    for (const ActiveToast& toast : toasts)
    {
        const QSize textSize = measureToast(painter, toast);
        const int bgW = textSize.width() + toast.style.padX * 2;
        const int bgH = textSize.height() + toast.style.padY * 2;
        drawToast(painter, toast, QRect(x, y, bgW, bgH));
        y += bgH + kToastSpacingPx;
    }
}

void drawBottomCenterStack(QWidget& host, QPainter& painter, std::span<const ActiveToast> toasts)
{
    int y = host.height() - kMarginPx;

    // Paint upward so the newest bottom toast remains closest to the bottom edge.
    for (const ActiveToast& toast : toasts)
    {
        const QSize textSize = measureToast(painter, toast);
        const int bgW = textSize.width() + toast.style.padX * 2;
        const int bgH = textSize.height() + toast.style.padY * 2;
        y -= bgH;
        drawToast(painter, toast, QRect((host.width() - bgW) / 2, y, bgW, bgH));
        y -= kToastSpacingPx;
    }
}

void drawCenterStack(QWidget& host, QPainter& painter, std::span<const ActiveToast> toasts)
{
    std::vector<MeasuredToast> measured{};
    measured.reserve(toasts.size());

    // Step 1: measure the whole stack before drawing so it can be vertically centered.
    int totalH = 0;
    for (const ActiveToast& toast : toasts)
    {
        const QSize textSize = measureToast(painter, toast);
        const int bgW = textSize.width() + toast.style.padX * 2;
        const int bgH = textSize.height() + toast.style.padY * 2;
        measured.push_back(MeasuredToast{ toast, bgW, bgH });
        totalH += bgH;
    }
    totalH += kToastSpacingPx * std::max(0, static_cast<int>(measured.size()) - 1);

    // Step 2: draw the stack from its centered top edge downward.
    int y = (host.height() - totalH) / 2;
    for (const MeasuredToast& item : measured)
    {
        (void)measureToast(painter, item.toast);
        drawToast(painter, item.toast, QRect((host.width() - item.bgW) / 2, y, item.bgW, item.bgH));
        y += item.bgH + kToastSpacingPx;
    }
}

} // namespace

void drawToastOverlay(QWidget& host, const std::span<const ToastOverlayToast> toasts)
{
    const auto now = std::chrono::steady_clock::now();

    std::vector<ActiveToast> topLeft{};
    std::vector<ActiveToast> center{};
    std::vector<ActiveToast> bottomCenter{};
    topLeft.reserve(toasts.size());
    center.reserve(toasts.size());
    bottomCenter.reserve(toasts.size());

    // Step 1: discard expired/empty messages and bucket the visible ones by placement.
    for (const ToastOverlayToast& toast : toasts)
    {
        if (toast.text.isEmpty() || now >= toast.expires)
        {
            continue;
        }

        ActiveToast active{};
        active.toast = &toast;
        active.alpha = toastAlpha(now, toast);
        active.style = styleForToast(toast.group, toast.placement);
        if ( toast.textColor.isValid() )
        {
            active.style.textColor = toast.textColor;
        }
        if ( toast.backgroundColor.isValid() )
        {
            active.style.bgColor = toast.backgroundColor;
        }
        if ( toast.borderColor.isValid() )
        {
            active.style.borderColor = toast.borderColor;
        }

        switch (toast.placement)
        {
        case ToastOverlayPlacement::Center:
            center.push_back(active);
            break;
        case ToastOverlayPlacement::BottomCenter:
            bottomCenter.push_back(active);
            break;
        case ToastOverlayPlacement::TopLeft:
            topLeft.push_back(active);
            break;
        }
    }

    if (topLeft.empty() && center.empty() && bottomCenter.empty())
    {
        return;
    }

    // Step 2: use QPainter only after we know at least one toast will be visible.
    QPainter painter(&host);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Step 3: paint each placement stack with its own anchoring rules.
    drawTopLeftStack(host, painter, topLeft);
    drawBottomCenterStack(host, painter, bottomCenter);
    drawCenterStack(host, painter, center);
}

void drawCleaveHudBadge(
    QPainter& painter,
    const QRectF& rect,
    const QString& text,
    const QColor& textColor,
    const QColor& backgroundColor,
    const QColor& borderColor,
    const qreal opacity)
{
    const qreal safeOpacity = std::isfinite(opacity)
        ? std::clamp(opacity, 0.0, 1.0) : 0.0;
    const auto fadedColor = [safeOpacity](const QColor& original)
    {
        QColor faded = original;
        faded.setAlpha(static_cast<int>(std::lround(
            safeOpacity * static_cast<qreal>(original.alpha()))));
        return faded;
    };
    const QColor fadedTextColor = fadedColor(textColor);
    const QColor background = fadedColor(backgroundColor);
    const QColor border = fadedColor(borderColor);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSizeF(std::max(9.0, font.pointSizeF()));
    painter.setFont(font);

    painter.setPen(border.alpha() > 0
        ? QPen(border, 1.0) : QPen(Qt::NoPen));
    painter.setBrush(background);
    painter.drawRoundedRect(rect, 6.0, 6.0);

    const QFontMetricsF metrics(font);
    const QString elided = metrics.elidedText(
        text, Qt::ElideRight, std::max(0.0, rect.width() - 24.0));
    painter.setPen(fadedTextColor);
    painter.drawText(rect, Qt::AlignCenter, elided);
    painter.restore();
}

} // namespace sed::ui::viewport
