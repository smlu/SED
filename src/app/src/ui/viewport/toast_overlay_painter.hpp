#pragma once

#include <chrono>
#include <cstdint>
#include <span>

#include <QColor>
#include <QRectF>
#include <QString>
#include <QWidget>

class QPainter;

namespace sed::ui::viewport
{

enum class ToastOverlayPlacement : std::uint8_t
{
    TopLeft,
    Center,
    BottomCenter,
};

enum class ToastOverlayGroup : std::uint8_t
{
    General,
    Projection,
    FlySpeed,
    Error,
    PlacementYaw,
};

/**
 * Immutable toast snapshot consumed by the viewport overlay painter.
 *
 * GlView owns toast lifetime/state and snapshots its palette here; this helper
 * only decides which active messages are visible and paints them into the
 * current widget.
 */
struct ToastOverlayToast
{
    QString text{};
    int durationMs{};
    std::chrono::steady_clock::time_point shownAt{};
    std::chrono::steady_clock::time_point expires{};
    ToastOverlayPlacement placement{ ToastOverlayPlacement::TopLeft };
    ToastOverlayGroup group{ ToastOverlayGroup::General };
    QColor textColor{};
    QColor backgroundColor{};
    QColor borderColor{};
};

/** Paints active transient viewport messages over the OpenGL scene. */
void drawToastOverlay(QWidget& host, std::span<const ToastOverlayToast> toasts);

/** Paints the Cleave transform HUD with the shared viewport banner palette. */
void drawCleaveHudBadge(
    QPainter& painter,
    const QRectF& rect,
    const QString& text,
    const QColor& textColor,
    const QColor& backgroundColor,
    const QColor& borderColor,
    qreal opacity);

} // namespace sed::ui::viewport
