#include "test_common.hpp"

#include "ui/viewport/toast_overlay_painter.hpp"

#include <algorithm>
#include <chrono>
#include <optional>
#include <vector>

#include <QApplication>
#include <QImage>
#include <QPaintEvent>
#include <QPainter>
#include <QWidget>

namespace sed::app_tests
{
namespace
{

using Toast = ui::viewport::ToastOverlayToast;
using ToastGroup = ui::viewport::ToastOverlayGroup;
using ToastPlacement = ui::viewport::ToastOverlayPlacement;

class ToastOverlayTestHost final : public QWidget
{
public:
    ToastOverlayTestHost()
    {
        setAttribute(Qt::WA_DontShowOnScreen);
        resize(320, 200);
    }

    QImage renderToast(const Toast& toast)
    {
        toasts_ = {toast};
        repaint();
        return grab().toImage().convertToFormat(QImage::Format_ARGB32);
    }

    QImage renderEmpty()
    {
        toasts_.clear();
        repaint();
        return grab().toImage().convertToFormat(QImage::Format_ARGB32);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        {
            QPainter background(this);
            background.fillRect(rect(), Qt::black);
        }
        ui::viewport::drawToastOverlay(*this, toasts_);
    }

private:
    std::vector<Toast> toasts_{};
};

int maximumChannel(const QImage& image, const int channel)
{
    int maximum{};
    for ( int y = 0; y < image.height(); ++y )
    {
        for ( int x = 0; x < image.width(); ++x )
        {
            const QColor color = image.pixelColor(x, y);
            const int value = channel == 0 ? color.red()
                : channel == 1 ? color.green() : color.blue();
            maximum = std::max(maximum, value);
        }
    }
    return maximum;
}

Toast visibleToast()
{
    const auto now = std::chrono::steady_clock::now();
    Toast toast{};
    toast.text = QStringLiteral("Palette check");
    toast.durationMs = 1000;
    toast.shownAt = now - std::chrono::milliseconds(300);
    toast.expires = now + std::chrono::seconds(2);
    toast.placement = ToastPlacement::BottomCenter;
    toast.group = ToastGroup::PlacementYaw;
    toast.textColor = QColor(255, 0, 0, 128);
    toast.backgroundColor = QColor(0, 0, 255, 128);
    toast.borderColor = QColor(0, 255, 0, 0);
    return toast;
}

void testToastPaletteAlphaAndExpiry()
{
    ToastOverlayTestHost host{};
    host.show();
    QApplication::processEvents();

    const Toast visible = visibleToast();
    const QImage fullAlpha = host.renderToast(visible);
    Toast fading = visible;
    const auto now = std::chrono::steady_clock::now();
    fading.durationMs = 1000;
    fading.shownAt = now - std::chrono::milliseconds(100);
    fading.expires = now + std::chrono::seconds(2);
    const QImage faded = host.renderToast(fading);
    const int fullTextAlpha = maximumChannel(fullAlpha, 0);
    const int fadedTextAlpha = maximumChannel(faded, 0);
    const int fullBackgroundAlpha = maximumChannel(fullAlpha, 2);
    const int fadedBackgroundAlpha = maximumChannel(faded, 2);
    check(fullTextAlpha >= 100 && fullTextAlpha <= 145
            && fadedTextAlpha >= 35 && fadedTextAlpha <= 90
            && fadedTextAlpha * 3 < fullTextAlpha * 2
            && fullBackgroundAlpha >= 100 && fullBackgroundAlpha <= 145
            && fadedBackgroundAlpha >= 35 && fadedBackgroundAlpha <= 90
            && fadedBackgroundAlpha * 3 < fullBackgroundAlpha * 2,
        "toast text and background keep configured alpha and multiply it by the fade amount");

    Toast withBorder = visible;
    withBorder.backgroundColor = QColor(0, 0, 0, 255);
    withBorder.borderColor = QColor(0, 255, 0, 255);
    const QImage bordered = host.renderToast(withBorder);
    const QImage transparentBorder = host.renderToast(visible);
    check(maximumChannel(bordered, 1) >= 200
            && maximumChannel(transparentBorder, 1) == 0,
        "a configured toast border is painted while a transparent border keeps the legacy no-pen appearance");

    Toast expired = visible;
    expired.shownAt = std::chrono::steady_clock::now() - std::chrono::seconds(3);
    expired.expires = std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
    const QImage hidden = host.renderToast(expired);
    const QImage empty = host.renderEmpty();
    check(hidden == empty,
        "an expired toast palette snapshot leaves the viewport overlay unchanged");
    host.hide();
}

} // namespace

void runToastOverlayPainterTests()
{
    testToastPaletteAlphaAndExpiry();
}

} // namespace sed::app_tests
