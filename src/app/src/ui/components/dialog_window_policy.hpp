#pragma once

#include <algorithm>

#include <QDialog>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QScreen>
#include <QStyle>
#include <QStyleOptionTitleBar>
#include <QWidget>
#include <QWindow>

namespace sed::ui
{

/** Place a dialog beside its invoker while keeping it within one screen. */
inline QPoint dialogPositionNearAnchor(
    const QRect& anchorGlobalRect,
    const QSize& dialogSize,
    const QRect& availableGeometry,
    const int gap = 8)
{
    if (!anchorGlobalRect.isValid() || !dialogSize.isValid() || !availableGeometry.isValid())
    {
        return availableGeometry.topLeft();
    }

    const int width = std::min(dialogSize.width(), availableGeometry.width());
    const int height = std::min(dialogSize.height(), availableGeometry.height());
    const int minimumX = availableGeometry.left();
    const int maximumX = std::max(minimumX, availableGeometry.right() + 1 - width);
    const int minimumY = availableGeometry.top();
    const int maximumY = std::max(minimumY, availableGeometry.bottom() + 1 - height);

    const int rightX = anchorGlobalRect.right() + 1 + gap;
    const int leftX = anchorGlobalRect.left() - width - gap;
    const bool fitsRight = rightX <= maximumX;
    const bool fitsLeft = leftX >= minimumX;

    int x = rightX;
    if (!fitsRight && fitsLeft)
    {
        x = leftX;
    }
    else if (!fitsRight && !fitsLeft)
    {
        const int spaceRight = availableGeometry.right() + 1
            - (anchorGlobalRect.right() + 1 + gap);
        const int spaceLeft = anchorGlobalRect.left() - gap - minimumX;
        x = spaceRight >= spaceLeft ? rightX : leftX;
    }
    x = std::clamp(x, minimumX, maximumX);
    const int y = std::clamp(anchorGlobalRect.top(), minimumY, maximumY);
    return { x, y };
}

inline void positionDialogNearWidget(QDialog& dialog, const QWidget* anchor, const int gap = 8)
{
    if (!anchor)
    {
        return;
    }

    QScreen* screen = anchor->screen();
    const QRect anchorGlobalRect(anchor->mapToGlobal(QPoint(0, 0)), anchor->size());
    if (!screen)
    {
        screen = QGuiApplication::screenAt(anchorGlobalRect.center());
    }
    if (!screen)
    {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen)
    {
        return;
    }

    // Create the native window before reading frame margins. QWidget::size()
    // excludes the caption and borders, but the position has to keep the full
    // frame on screen as well.
    dialog.ensurePolished();
    (void)dialog.winId();
    const QSize clientSize = dialog.sizeHint().expandedTo(dialog.size());
    const QMargins frameMargins = dialog.windowHandle()
        ? dialog.windowHandle()->frameMargins()
        : QMargins{};
    QSize frameSize = clientSize + QSize(
        frameMargins.left() + frameMargins.right(),
        frameMargins.top() + frameMargins.bottom());
    const QSize measuredFrameSize = dialog.frameGeometry().size();
    if (measuredFrameSize.isValid())
    {
        frameSize = frameSize.expandedTo(measuredFrameSize);
    }

    const QPoint target = dialogPositionNearAnchor(
        anchorGlobalRect,
        frameSize,
        screen->availableGeometry(),
        gap);
    dialog.move(target);

    // On native window systems QWidget::move() may address the client origin
    // while the screen clamp above is based on the outer frame. Correct any
    // difference after the hidden native window reports its actual frame.
    const QPoint measuredTopLeft = dialog.frameGeometry().topLeft();
    if (measuredTopLeft != target)
    {
        dialog.move(dialog.pos() + (target - measuredTopLeft));
    }
}

inline int minimumDialogWidthForCaption(const QDialog& dialog)
{
    if (dialog.windowTitle().isEmpty() || !dialog.style())
    {
        return 0;
    }

    const QFont titleFont = QFontDatabase::systemFont(QFontDatabase::TitleFont);
    const QFontMetrics metrics(titleFont);
    const int textWidth = metrics.horizontalAdvance(dialog.windowTitle()) + 12;
    const int titleHeight = std::max(
        dialog.style()->pixelMetric(QStyle::PM_TitleBarHeight, nullptr, &dialog),
        metrics.height() + 8);
    const int buttonWidth = std::max(
        titleHeight,
        dialog.style()->pixelMetric(QStyle::PM_TitleBarButtonSize, nullptr, &dialog));
    const int iconWidth = std::max(
        titleHeight,
        dialog.style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, &dialog));

    int candidate = textWidth + iconWidth + buttonWidth + 32;
    QStyleOptionTitleBar option{};
    option.initFrom(&dialog);
    option.rect = QRect(0, 0, candidate, titleHeight);
    option.text = dialog.windowTitle();
    option.icon = dialog.windowIcon();
    option.titleBarFlags = dialog.windowFlags();
    option.titleBarState = static_cast<int>(dialog.windowState());
    const QRect label = dialog.style()->subControlRect(
        QStyle::CC_TitleBar,
        &option,
        QStyle::SC_TitleBarLabel,
        &dialog);
    if (label.isValid() && label.width() < textWidth)
    {
        candidate += textWidth - label.width();
    }
    return candidate;
}

/** Applies the standard SED dialog caption contract without fixing client size. */
inline void applyDialogWindowPolicy(QDialog& dialog)
{
    // QDialog's platform default may carry the min/max hints even when the
    // dialog was constructed with only a title and close button.  Building
    // the caption flags from the dialog type makes the policy deterministic
    // on Windows (where the native caption otherwise keeps stale buttons).
    constexpr Qt::WindowFlags kCaptionMask =
        Qt::CustomizeWindowHint
        | Qt::WindowTitleHint
        | Qt::WindowCloseButtonHint
        | Qt::WindowMinimizeButtonHint
        | Qt::WindowMaximizeButtonHint
        | Qt::WindowContextHelpButtonHint;
    Qt::WindowFlags flags = dialog.windowFlags();
    flags &= ~kCaptionMask;
    flags |= Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint;
    if (flags != dialog.windowFlags())
    {
        dialog.setWindowFlags(flags);
    }
    // Keep explicit per-hint clears after setWindowFlags as well.  This is
    // harmless on Qt styles that already applied the exact mask and protects
    // against a native platform style reintroducing caption hints while the
    // dialog is polished.
    dialog.setWindowFlag(Qt::WindowMinimizeButtonHint, false);
    dialog.setWindowFlag(Qt::WindowMaximizeButtonHint, false);
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog.setMinimumWidth(std::max(
        dialog.minimumWidth(),
        minimumDialogWidthForCaption(dialog)));
}

/** Applies an exact non-resizable dialog caption before the native window is created. */
inline void applyFixedDialogWindowPolicy(QDialog& dialog)
{
    constexpr Qt::WindowFlags kFixedDialogFlags =
        Qt::Dialog
        | Qt::CustomizeWindowHint
        | Qt::WindowTitleHint
        | Qt::WindowSystemMenuHint
        | Qt::WindowCloseButtonHint
        | Qt::MSWindowsFixedSizeDialogHint;
    dialog.setWindowFlags(kFixedDialogFlags);
    dialog.setWindowFlag(Qt::WindowMinimizeButtonHint, false);
    dialog.setWindowFlag(Qt::WindowMaximizeButtonHint, false);
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog.setSizeGripEnabled(false);
    dialog.setMinimumWidth(std::max(
        dialog.minimumWidth(),
        minimumDialogWidthForCaption(dialog)));
}

} // namespace sed::ui
