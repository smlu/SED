#include "test_common.hpp"

#include "render/cleave/cleave_hud_presentation.hpp"
#include "ui/viewport/toast_overlay_painter.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>

#include <QImage>
#include <QPainter>
#include <QRectF>
#include <QString>

namespace sed::app_tests
{
namespace
{

using Presentation = render::cleave::CleaveHudPresentation;
using TimePoint = Presentation::TimePoint;

TimePoint atMilliseconds(const int milliseconds)
{
    return TimePoint{} + std::chrono::milliseconds(milliseconds);
}

bool near(const float actual, const float expected)
{
    return std::abs(actual - expected) <= 0.01f;
}

void testDefaultFadeEdgesAndWakeSchedule()
{
    Presentation presentation{};
    presentation.sample(atMilliseconds(0), 400);

    check(presentation.durationMs() == 400
            && presentation.startedAt() == atMilliseconds(0)
            && presentation.deadline() == atMilliseconds(400)
            && presentation.fadeEdgeDurationMs() == 40,
        "Cleave HUD timeline keeps the exact 400 ms lifetime and 40 ms fade edges");
    check(near(presentation.opacityAt(atMilliseconds(0)), 0.0f)
            && near(presentation.opacityAt(atMilliseconds(20)), 0.5f)
            && near(presentation.opacityAt(atMilliseconds(40)), 1.0f)
            && near(presentation.opacityAt(atMilliseconds(360)), 1.0f)
            && near(presentation.opacityAt(atMilliseconds(380)), 0.5f)
            && near(presentation.opacityAt(atMilliseconds(400)), 0.0f),
        "Cleave HUD opacity fades at both edges without extending its absolute deadline");

    check(presentation.nextWakeDelayMs(atMilliseconds(0)) == 12
            && presentation.nextWakeDelayMs(atMilliseconds(20)) == 12
            && presentation.nextWakeDelayMs(atMilliseconds(40)) == 320
            && presentation.nextWakeDelayMs(atMilliseconds(360)) == 12
            && presentation.nextWakeDelayMs(atMilliseconds(380)) == 12
            && presentation.nextWakeDelayMs(atMilliseconds(400)) == 0,
        "Cleave HUD wakes during fade edges and sleeps through the fully opaque plateau");
}

void testShortDurationAndRenewal()
{
    Presentation shortPresentation{};
    shortPresentation.sample(atMilliseconds(0), 100);
    check(shortPresentation.fadeEdgeDurationMs() == 10
            && near(shortPresentation.opacityAt(atMilliseconds(0)), 0.0f)
            && near(shortPresentation.opacityAt(atMilliseconds(5)), 0.5f)
            && near(shortPresentation.opacityAt(atMilliseconds(10)), 1.0f)
            && near(shortPresentation.opacityAt(atMilliseconds(90)), 1.0f)
            && near(shortPresentation.opacityAt(atMilliseconds(95)), 0.5f)
            && near(shortPresentation.opacityAt(atMilliseconds(100)), 0.0f),
        "custom 100 ms Cleave HUD lifetimes use 10 ms edge fades inside their deadline");
    const int firstShortWake = shortPresentation.nextWakeDelayMs(atMilliseconds(0));
    const int secondShortWake = shortPresentation.nextWakeDelayMs(
        atMilliseconds(firstShortWake));
    check(firstShortWake == 5 && secondShortWake == 5
            && near(shortPresentation.opacityAt(atMilliseconds(firstShortWake)), 0.5f)
            && near(shortPresentation.opacityAt(
                atMilliseconds(firstShortWake + secondShortWake)), 1.0f)
            && shortPresentation.nextWakeDelayMs(atMilliseconds(10)) == 80
            && shortPresentation.nextWakeDelayMs(atMilliseconds(90)) == 5
            && shortPresentation.nextWakeDelayMs(atMilliseconds(95)) == 5
            && shortPresentation.nextWakeDelayMs(atMilliseconds(100)) == 0,
        "a 100 ms lifetime schedules intermediate 5 ms fade frames and sleeps only across its plateau");

    Presentation continued{};
    continued.sample(atMilliseconds(0), 400);
    continued.sample(atMilliseconds(20), 400);
    check(continued.startedAt() == atMilliseconds(0)
            && continued.deadline() == atMilliseconds(420)
            && near(continued.opacityAt(atMilliseconds(20)), 0.5f)
            && near(continued.opacityAt(atMilliseconds(40)), 1.0f),
        "a continuing sample during fade-in preserves its original ramp instead of restarting it");

    Presentation renewed{};
    renewed.sample(atMilliseconds(0), 400);
    const std::uint64_t firstGeneration = renewed.generation();
    renewed.sample(atMilliseconds(380), 400);
    check(renewed.generation() > firstGeneration
            && renewed.startedAt() == atMilliseconds(0)
            && renewed.deadline() == atMilliseconds(780)
            && renewed.activeAt(atMilliseconds(400))
            && !renewed.expiredAt(atMilliseconds(400))
            && near(renewed.opacityAt(atMilliseconds(380)), 0.5f)
            && near(renewed.opacityAt(atMilliseconds(400)), 0.75f),
        "a renewed fade-out sample preserves current opacity, restores toward one, and invalidates the old deadline");
    renewed.sample(atMilliseconds(400), 400);
    check(renewed.deadline() == atMilliseconds(800)
            && near(renewed.opacityAt(atMilliseconds(420)), 1.0f)
            && renewed.activeAt(atMilliseconds(780))
            && near(renewed.opacityAt(atMilliseconds(780)), 0.5f)
            && renewed.expiredAt(atMilliseconds(800)),
        "repeated renewal during recovery does not restart the fade-in and retains the latest absolute deadline");
}

void testResetStartsANewFadeIn()
{
    Presentation presentation{};
    presentation.sample(atMilliseconds(0), 400);
    presentation.sample(atMilliseconds(380), 400);
    const std::uint64_t beforeClear = presentation.generation();
    presentation.clear();
    presentation.sample(atMilliseconds(800), 400);

    check(presentation.generation() > beforeClear
            && presentation.startedAt() == atMilliseconds(800)
            && presentation.deadline() == atMilliseconds(1200)
            && near(presentation.opacityAt(atMilliseconds(800)), 0.0f)
            && near(presentation.opacityAt(atMilliseconds(820)), 0.5f)
            && presentation.expiredAt(atMilliseconds(1200)),
        "clearing and resetting a Cleave HUD starts a fresh fade without retaining an old timeout");
}

int maximumAlphaForChannel(const QImage& image, const int channel)
{
    int maximum{};
    for ( int y = 0; y < image.height(); ++y )
    {
        for ( int x = 0; x < image.width(); ++x )
        {
            const QColor color = image.pixelColor(x, y);
            const int primary = channel == 0 ? color.red()
                : channel == 1 ? color.green() : color.blue();
            const int otherA = channel == 0 ? color.green() : color.red();
            const int otherB = channel == 2 ? color.green() : color.blue();
            if ( primary > otherA + 20 && primary > otherB + 20 )
            {
                maximum = std::max(maximum, color.alpha());
            }
        }
    }
    return maximum;
}

QImage renderBadge(
    const qreal opacity,
    const QColor& textColor,
    const QColor& backgroundColor,
    const QColor& borderColor)
{
    QImage image(240, 90, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    ui::viewport::drawCleaveHudBadge(
        painter,
        QRectF(20.0, 15.0, 200.0, 60.0),
        QStringLiteral("Cleave HUD"),
        textColor,
        backgroundColor,
        borderColor,
        opacity);
    painter.end();
    return image;
}

void testProductionPainterMultipliesColorAlpha()
{
    const QColor transparent(0, 0, 0, 0);
    const QColor textColor(255, 0, 0, 200);
    const QColor backgroundColor(0, 0, 255, 200);
    const QColor borderColor(0, 255, 0, 180);

    const QImage fullText = renderBadge(1.0, textColor, transparent, transparent);
    const QImage halfText = renderBadge(0.5, textColor, transparent, transparent);
    const QImage fullBackground = renderBadge(1.0, transparent, backgroundColor, transparent);
    const QImage halfBackground = renderBadge(0.5, transparent, backgroundColor, transparent);
    const QImage fullBorder = renderBadge(1.0, transparent, transparent, borderColor);
    const QImage halfBorder = renderBadge(0.5, transparent, transparent, borderColor);
    const QImage invisible = renderBadge(
        0.0, textColor, backgroundColor, borderColor);

    const int fullTextAlpha = maximumAlphaForChannel(fullText, 0);
    const int halfTextAlpha = maximumAlphaForChannel(halfText, 0);
    const int fullBackgroundAlpha = fullBackground.pixelColor(40, 30).alpha();
    const int halfBackgroundAlpha = halfBackground.pixelColor(40, 30).alpha();
    std::array<int, 3> fullTopBorderAlpha{};
    std::array<int, 3> halfTopBorderAlpha{};
    bool borderHalved{};
    for ( int y = 14; y <= 16; ++y )
    {
        const std::size_t index = static_cast<std::size_t>(y - 14);
        fullTopBorderAlpha[index] = fullBorder.pixelColor(100, y).alpha();
        halfTopBorderAlpha[index] = halfBorder.pixelColor(100, y).alpha();
        const int fullAlpha = fullTopBorderAlpha[index];
        const int halfAlpha = halfTopBorderAlpha[index];
        borderHalved = borderHalved
            || (fullAlpha > 0 && halfAlpha > 0
                && std::abs((2 * halfAlpha) - fullAlpha) <= 2);
    }
    bool invisibleRaster = true;
    for ( int y = 0; y < invisible.height(); ++y )
    {
        for ( int x = 0; x < invisible.width(); ++x )
        {
            invisibleRaster = invisibleRaster && invisible.pixelColor(x, y).alpha() == 0;
        }
    }
    const bool textHalved = fullTextAlpha > 0 && halfTextAlpha > 0
        && std::abs((2 * halfTextAlpha) - fullTextAlpha) <= 2;
    const bool backgroundHalved = fullBackgroundAlpha == 200
        && halfBackgroundAlpha == 100;
    if ( !textHalved || !borderHalved || !backgroundHalved || !invisibleRaster )
    {
        std::cerr << "Cleave badge alpha diagnostics: text=" << fullTextAlpha
                  << '/' << halfTextAlpha << " background=" << fullBackgroundAlpha
                  << '/' << halfBackgroundAlpha << " border top-midpoint full=["
                  << fullTopBorderAlpha[0] << ',' << fullTopBorderAlpha[1] << ','
                  << fullTopBorderAlpha[2] << "] half=[" << halfTopBorderAlpha[0]
                  << ',' << halfTopBorderAlpha[1] << ',' << halfTopBorderAlpha[2]
                  << "] transparent=" << invisibleRaster << '\n';
    }
    check(textHalved && borderHalved && backgroundHalved && invisibleRaster,
        "the production Cleave HUD painter multiplies original text, fill and border alpha by presentation opacity");
}

} // namespace

void runCleaveHudPresentationTests()
{
    testDefaultFadeEdgesAndWakeSchedule();
    testShortDurationAndRenewal();
    testResetStartsANewFadeIn();
    testProductionPainterMultipliesColorAlpha();
}

} // namespace sed::app_tests
