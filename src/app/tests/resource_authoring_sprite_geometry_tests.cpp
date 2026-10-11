#include "test_common.hpp"
#include "resource_release_test_fixtures.hpp"

#include "gl_view.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/spr.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/components/live_color_popup.hpp"
#include "ui/resources/resource_authoring_editor.hpp"
#include "ui/resources/sprite_animation_clock.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QCborArray>
#include <QCborMap>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFormLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPushButton>
#include <QSplitter>
#include <QSlider>
#include <QStringList>
#include <QTemporaryDir>
#include <QToolButton>

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>

namespace sed::app_tests
{
namespace
{

bool waitForSprite(const std::function<bool()>& predicate, const int timeoutMs = 3500)
{
    QElapsedTimer elapsed{};
    elapsed.start();
    while ( elapsed.elapsed() < timeoutMs )
    {
        QApplication::processEvents(QEventLoop::AllEvents);
        if ( predicate() )
        {
            return true;
        }
    }
    return predicate();
}

bool writeSpriteFixture(const std::filesystem::path& path, const QByteArray& bytes)
{
    std::ofstream output(path, std::ios::binary);
    output.write(bytes.constData(), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(output);
}

void appendSpriteU32(QByteArray& bytes, const std::uint32_t value)
{
    for ( unsigned shift = 0u; shift < 32u; shift += 8u )
    {
        bytes.append(static_cast<char>((value >> shift) & 0xffu));
    }
}

QByteArray animatedSpriteMaterial()
{
    QByteArray bytes("MAT ", 4);
    for ( const std::uint32_t value : { 0x32u, 2u, 2u, 2u,
              static_cast<std::uint32_t>(ColorMode::RGBA), 32u,
              8u, 8u, 8u, 16u, 8u, 0u, 0u, 0u, 0u, 8u, 24u, 0u } )
    {
        appendSpriteU32(bytes, value);
    }
    for ( int cel = 0; cel < 2; ++cel )
    {
        for ( int field = 0; field < 10; ++field )
        {
            appendSpriteU32(bytes, field == 1 ? 6u : 0u);
        }
    }
    for ( const QColor& color : { QColor(232, 24, 48), QColor(24, 48, 232) } )
    {
        for ( const std::uint32_t value : { 2u, 2u, 0u, 0u, 0u, 1u } )
        {
            appendSpriteU32(bytes, value);
        }
        for ( int pixel = 0; pixel < 4; ++pixel )
        {
            bytes.append(static_cast<char>(color.blue()));
            bytes.append(static_cast<char>(color.green()));
            bytes.append(static_cast<char>(color.red()));
            bytes.append(static_cast<char>(color.alpha()));
        }
    }
    return bytes;
}

QByteArray spriteRenderModesMaterial()
{
    // One true-color cel: a white left half and blue right half. Its Solid
    // palette index is synthetic CMP red, so Solid and Texture cannot agree
    // accidentally merely because a fixture has one uniform texture color.
    QByteArray bytes("MAT ", 4);
    for ( const std::uint32_t value : { 0x32u, 2u, 1u, 1u,
              static_cast<std::uint32_t>(ColorMode::RGBA), 32u,
              8u, 8u, 8u, 16u, 8u, 0u, 0u, 0u, 0u, 8u, 24u, 0u } )
    {
        appendSpriteU32(bytes, value);
    }
    for ( int field = 0; field < 10; ++field )
    {
        appendSpriteU32(bytes, field == 1 ? 6u : 0u);
    }
    for ( const std::uint32_t value : { 4u, 4u, 0u, 0u, 0u, 1u } )
    {
        appendSpriteU32(bytes, value);
    }
    for ( int row = 0; row < 4; ++row )
    {
        for ( int column = 0; column < 4; ++column )
        {
            const QColor color = column < 2 ? QColor(255, 255, 255) : QColor(24, 48, 232);
            bytes.append(static_cast<char>(color.blue()));
            bytes.append(static_cast<char>(color.green()));
            bytes.append(static_cast<char>(color.red()));
            bytes.append(static_cast<char>(color.alpha()));
        }
    }
    return bytes;
}

QByteArray spriteMaterialArchive()
{
    const std::array<QByteArray, 2> payloads{
        animatedSpriteMaterial(), fixtures::makeClassicCmpFixture(6u, QColor(232, 24, 48)) };
    const std::array<QByteArray, 2> names{ "sprite_geometry.mat", "sprite_geometry.cmp" };
    QByteArray bytes("GOB ", 4);
    appendSpriteU32(bytes, 0u);
    appendSpriteU32(bytes, 0u);
    appendSpriteU32(bytes, 2u);
    std::uint32_t offset = 16u + 2u * 136u;
    for ( std::size_t entry = 0u; entry < payloads.size(); ++entry )
    {
        appendSpriteU32(bytes, offset);
        appendSpriteU32(bytes, static_cast<std::uint32_t>(payloads[entry].size()));
        bytes.append(names[entry]);
        bytes.append(QByteArray(128 - names[entry].size(), '\0'));
        offset += static_cast<std::uint32_t>(payloads[entry].size());
    }
    for ( const QByteArray& payload : payloads )
    {
        bytes.append(payload);
    }
    return bytes;
}

std::unique_ptr<QWidget> makeSpritePane(
    const std::shared_ptr<const assets::ResourceLocator>& resources,
    const ProjectType project,
    const int type,
    const float width,
    const float height,
    const std::array<float, 3>& offset)
{
    ui::resources::ResourceAuthoringContext context{};
    context.resources = resources;
    context.projectType = project;
    context.masterCmp = "sprite_geometry.cmp";
    std::unique_ptr<QWidget> pane(ui::resources::createResourceAuthoringPane(
        nullptr, ui::resources::ResourceAuthoringKind::sprite, std::move(context)));
    if ( !pane )
    {
        return {};
    }
    if ( auto* const view = pane->findChild<GlView*>(QStringLiteral("spr.preview.gl_view")) )
    {
        const auto rememberMessage = [view](const QString& prefix, const QString& message)
        {
            QStringList messages = view->property("spr.test.resource_messages").toStringList();
            if ( messages.size() < 8 )
            {
                messages.push_back(prefix + message);
                view->setProperty("spr.test.resource_messages", messages);
            }
        };
        QObject::connect(view, &GlView::resourceWarningMessage, view,
            [rememberMessage](const QString& message)
            {
                rememberMessage(QStringLiteral("warning: "), message);
            });
        QObject::connect(view, &GlView::resourceErrorMessage, view,
            [rememberMessage](const QString& message)
            {
                rememberMessage(QStringLiteral("error: "), message);
            });
    }
    QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane.get());
    // This is an independent authored baseline, not the previous material's journal.
    state.remove(QStringLiteral("history"));
    state.remove(QStringLiteral("presentation"));
    QCborArray sprite = state.value(QStringLiteral("sprite")).toArray();
    sprite[0] = QStringLiteral("sprite_geometry.mat");
    sprite[1] = type;
    sprite[2] = QString::number(width, 'g', 9);
    sprite[3] = QString::number(height, 'g', 9);
    sprite[4] = 4;
    sprite[5] = 0;
    for ( qsizetype axis = 0; axis < 3; ++axis )
    {
        sprite[12 + axis] = QString::number(offset[static_cast<std::size_t>(axis)], 'g', 9);
    }
    state.insert(QStringLiteral("sprite"), sprite);
    QString warning{};
    check(ui::resources::restoreResourceAuthoringPaneState(pane.get(), state, &warning),
        "Sprite geometry fixture restores through the real native draft boundary");
    return pane;
}

std::optional<assets::SprFile> previewSprite(GlView& view)
{
    const QString serialized = view.property("spr.preview.serialized_resource").toString();
    if ( serialized.isEmpty() )
    {
        return std::nullopt;
    }
    std::istringstream input(serialized.toStdString());
    return assets::loadFromSPR(input);
}

QVector3D spriteCenter(const std::array<QVector3D, 4>& corners)
{
    return (corners[0] + corners[1] + corners[2] + corners[3]) * 0.25f;
}

bool cornersInsideFrame(GlView& view, const std::array<QVector3D, 4>& corners)
{
    for ( const QVector3D& point : corners )
    {
        const std::optional<QPointF> projected = view.projectWorldToViewport(point);
        if ( !projected || projected->x() < 5.0 || projected->y() < 5.0
            || projected->x() > static_cast<double>(view.width()) - 5.0
            || projected->y() > static_cast<double>(view.height()) - 5.0 )
        {
            return false;
        }
    }
    return true;
}

bool hasSpritePixels(const QImage& frame)
{
    for ( int y = 0; y < frame.height(); ++y )
    {
        for ( int x = 0; x < frame.width(); ++x )
        {
            const QColor color = frame.pixelColor(x, y);
            if ( color.red() >= 120 && color.green() < 70 && color.blue() < 100 )
            {
                return true;
            }
        }
    }
    return false;
}

bool hasBlueSpritePixels(const QImage& frame)
{
    for ( int y = 0; y < frame.height(); ++y )
    {
        for ( int x = 0; x < frame.width(); ++x )
        {
            const QColor color = frame.pixelColor(x, y);
            if ( color.blue() >= 120 && color.red() < 70 && color.green() < 100 )
            {
                return true;
            }
        }
    }
    return false;
}

bool boundsInsideFrame(GlView& view)
{
    const auto bounds = view.thingVisualBoundsOverlayAabbForTest(0u, false);
    if ( !bounds )
    {
        return false;
    }
    for ( unsigned corner = 0u; corner < 8u; ++corner )
    {
        const QVector3D point(
            static_cast<float>((*bounds)[(corner & 1u) != 0u ? 3u : 0u]),
            static_cast<float>((*bounds)[(corner & 2u) != 0u ? 4u : 1u]),
            static_cast<float>((*bounds)[(corner & 4u) != 0u ? 5u : 2u]));
        const auto projected = view.projectWorldToViewport(point);
        if ( !projected || projected->x() < 5.0 || projected->y() < 5.0
            || projected->x() > static_cast<double>(view.width()) - 5.0
            || projected->y() > static_cast<double>(view.height()) - 5.0 )
        {
            return false;
        }
    }
    return true;
}

void printSpriteFramingFailure(
    GlView& view, const ProjectType project, const int type, const double dimension, const char* stage)
{
    const auto state = view.recoveryState();
    const auto& pose = state.preview.orbit;
    std::cerr << "SPR framing " << stage << " project=" << static_cast<int>(project)
        << " type=" << type << " dimension=" << dimension << " canvas=" << view.width() << 'x' << view.height()
        << " clip=" << state.preview.clipEnabled << " scale=" << state.preview.orthoScale
        << " nearfar=" << state.nearPlane << ',' << state.farPlane
        << " angles=" << pose.yawDeg << ',' << pose.pitchDeg << ',' << pose.rollDeg
        << " center=" << pose.center.x() << ',' << pose.center.y() << ',' << pose.center.z()
        << " pan=" << pose.pan.x() << ',' << pose.pan.y() << ',' << pose.pan.z()
        << " radius/distance=" << pose.radius << '/' << pose.distance
        << " unavailable=" << view.property("spr.preview.fit_unavailable").toBool();
    if ( const auto bounds = view.thingVisualBoundsOverlayAabbForTest(0u, false) )
    {
        std::cerr << " bounds=";
        for ( const double value : *bounds )
        {
            std::cerr << value << ',';
        }
    }
    if ( const auto corners = view.thingSpriteCornersForTest(0u) )
    {
        std::cerr << " card=";
        for ( const QVector3D& point : *corners )
        {
            std::cerr << '(' << point.x() << ',' << point.y() << ',' << point.z() << ";depth="
                << QVector3D::dotProduct(point - view.cameraEye(), view.cameraForward());
            if ( const auto screen = view.projectWorldToViewport(point) )
            {
                std::cerr << ";screen=" << screen->x() << ',' << screen->y();
            }
            else
            {
                std::cerr << ";clipped";
            }
            std::cerr << ')';
        }
    }
    std::cerr << '\n';
}

void commitSpriteNumber(QWidget& pane, const char* name, const double value)
{
    auto* const edit = pane.findChild<ui::ScrubFloatEdit*>(QString::fromLatin1(name));
    check(edit != nullptr, "Sprite numeric edit is present for geometry regression");
    if ( edit )
    {
        edit->setValue(value);
        check(QMetaObject::invokeMethod(
                edit, "valueCommitted", Qt::DirectConnection, Q_ARG(double, value)),
            "Sprite geometry regression uses the real valueCommitted binding");
    }
}

QColor sampleSpriteWorldPixel(GlView& view, const QImage& frame, const QVector3D& world)
{
    const auto projected = view.projectWorldToViewport(world);
    if ( !projected || frame.isNull() || view.width() <= 0 || view.height() <= 0 )
    {
        return {};
    }
    const int x = static_cast<int>(std::lround(projected->x() * frame.width() / view.width()));
    const int y = static_cast<int>(std::lround(projected->y() * frame.height() / view.height()));
    if ( x < 1 || y < 1 || x >= frame.width() - 1 || y >= frame.height() - 1 )
    {
        return {};
    }
    std::array<int, 3> channels{};
    for ( int dy = -1; dy <= 1; ++dy )
    {
        for ( int dx = -1; dx <= 1; ++dx )
        {
            const QColor pixel = frame.pixelColor(x + dx, y + dy);
            channels[0] += pixel.red();
            channels[1] += pixel.green();
            channels[2] += pixel.blue();
        }
    }
    return QColor(channels[0] / 9, channels[1] / 9, channels[2] / 9);
}

bool spritePixelNear(const QColor& actual, const QColor& expected, const int tolerance = 8)
{
    return actual.isValid() && expected.isValid()
        && std::abs(actual.red() - expected.red()) <= tolerance
        && std::abs(actual.green() - expected.green()) <= tolerance
        && std::abs(actual.blue() - expected.blue()) <= tolerance;
}

void printSpritePixelFailure(
    GlView& view, const ProjectType project, const int type, const char* stage,
    const std::array<QVector3D, 4>& originalCorners,
    const QVector3D& leftSample, const QVector3D& rightSample)
{
    const QImage frame = view.grabFramebuffer();
    const QColor left = sampleSpriteWorldPixel(view, frame, leftSample);
    const QColor right = sampleSpriteWorldPixel(view, frame, rightSample);
    std::error_code error{};
    const std::filesystem::path directory = std::filesystem::current_path(error)
        / "build" / "test-artifacts" / "sprite-pixel-diagnostics";
    if ( !error )
    {
        std::filesystem::create_directories(directory, error);
    }
    const std::filesystem::path path = directory / ("sprite-" + std::to_string(static_cast<int>(project))
        + "-" + std::to_string(type) + "-" + stage + ".png");
    const bool saved = !error && frame.save(QString::fromStdWString(path.wstring()));
    std::cerr << "SPR pixel " << stage << " project=" << static_cast<int>(project) << " type=" << type
        << " framebuffer=" << frame.width() << 'x' << frame.height()
        << " visible=" << view.isVisible() << " valid=" << view.isValid()
        << " left=" << left.red() << ',' << left.green() << ',' << left.blue()
        << " right=" << right.red() << ',' << right.green() << ',' << right.blue()
        << " cmp=" << view.property("resource_authoring.preview_cmp").toString().toStdString()
        << " imageSaved=" << saved << " image=" << path.string() << '\n';
    std::cerr << "  originalCard=";
    for ( const QVector3D& point : originalCorners )
    {
        std::cerr << '(' << point.x() << ',' << point.y() << ',' << point.z() << ')';
    }
    std::cerr << " samples=";
    for ( const QVector3D& point : { leftSample, rightSample } )
    {
        std::cerr << '(' << point.x() << ',' << point.y() << ',' << point.z();
        if ( const auto screen = view.projectWorldToViewport(point) )
        {
            std::cerr << ";screen=" << screen->x() << ',' << screen->y();
        }
        std::cerr << ')';
    }
    std::cerr << "\n  resource="
        << view.property("spr.preview.serialized_resource").toString().left(1024).toStdString() << '\n';
    for ( const QString& message : view.property("spr.test.resource_messages").toStringList() )
    {
        std::cerr << "  " << message.toStdString() << '\n';
    }
    printSpriteFramingFailure(view, project, type, 4.0, stage);
}

int spriteWhitePixelCount(const QImage& frame)
{
    int count = 0;
    for ( int y = 0; y < frame.height(); ++y )
    {
        for ( int x = 0; x < frame.width(); ++x )
        {
            const QColor pixel = frame.pixelColor(x, y);
            count += pixel.red() > 200 && pixel.green() > 200 && pixel.blue() > 200 ? 1 : 0;
        }
    }
    return count;
}

int spriteWhitePixelsNearWorld(GlView& view, const QImage& frame, const QVector3D& point)
{
    const auto projected = view.projectWorldToViewport(point);
    if ( !projected || frame.isNull() )
    {
        return 0;
    }
    const int x = static_cast<int>(std::lround(projected->x() * frame.width() / view.width()));
    const int y = static_cast<int>(std::lround(projected->y() * frame.height() / view.height()));
    int count = 0;
    for ( int dy = -4; dy <= 4; ++dy )
    {
        for ( int dx = -4; dx <= 4; ++dx )
        {
            if ( x + dx >= 0 && y + dy >= 0 && x + dx < frame.width() && y + dy < frame.height() )
            {
                const QColor pixel = frame.pixelColor(x + dx, y + dy);
                count += pixel.red() > 160 && pixel.green() > 160 && pixel.blue() > 160 ? 1 : 0;
            }
        }
    }
    return count;
}

void selectSpriteMode(QWidget& pane, const char* name, const int value)
{
    auto* const combo = pane.findChild<QComboBox*>(QString::fromLatin1(name));
    check(combo && combo->findData(value) >= 0, "Sprite render-mode regression uses the real authored combo");
    if ( combo && combo->findData(value) >= 0 )
    {
        combo->setCurrentIndex(combo->findData(value));
    }
}

QLabel* spriteFieldLabel(QWidget& pane, QWidget* const field)
{
    for ( QFormLayout* const form : pane.findChildren<QFormLayout*>() )
    {
        if ( auto* const label = qobject_cast<QLabel*>(form->labelForField(field)) )
        {
            return label;
        }
    }
    return nullptr;
}

void checkSpriteRenderModes(
    const std::shared_ptr<const assets::ResourceLocator>& resources,
    const ProjectType project,
    const int type)
{
    auto pane = makeSpritePane(resources, project, type, 4.0f, 4.0f, { 0.0f, 0.0f, 0.0f });
    if ( !pane )
    {
        return;
    }
    QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane.get());
    // Replacing the authored material establishes a fresh fixture baseline.
    state.remove(QStringLiteral("history"));
    state.remove(QStringLiteral("presentation"));
    QCborArray sprite = state.value(QStringLiteral("sprite")).toArray();
    sprite[0] = QStringLiteral("sprite_modes.mat");
    for ( qsizetype channel = 8; channel < 11; ++channel )
    {
        sprite[channel] = QStringLiteral("0.25");
    }
    sprite[7] = QStringLiteral("0.25");
    state.insert(QStringLiteral("sprite"), sprite);
    QString warning{};
    check(ui::resources::restoreResourceAuthoringPaneState(pane.get(), state, &warning),
        "Sprite modes fixture restores exact authored mode and extra-light data");
    pane->resize(1120, 720);
    auto* const view = pane->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"));
    auto* const bounds = pane->findChild<QCheckBox*>(QStringLiteral("spr.preview.bounds"));
    check(view && bounds, "Sprite render modes use the real OpenGL authoring preview");
    if ( !view || !bounds )
    {
        return;
    }
    bounds->setChecked(false);
    pane->show();
    check(waitForSprite([view]()
        {
            const auto authored = previewSprite(*view);
            return view->isValid() && view->thingSpriteCornersForTest(0u).has_value()
                && authored && authored->material == "sprite_modes.mat";
        }),
        "Sprite render-mode fixture publishes a ready rendered card");
    const auto corners = view->thingSpriteCornersForTest(0u);
    if ( !corners )
    {
        return;
    }
    const QVector3D leftSample = ((*corners)[0] + (*corners)[3]) * 0.375f
        + ((*corners)[1] + (*corners)[2]) * 0.125f;
    const auto pixel = [view, leftSample]()
    {
        return sampleSpriteWorldPixel(*view, view->grabFramebuffer(), leftSample);
    };
    const bool whiteControl = waitForSprite([&pixel]() { return spritePixelNear(pixel(), QColor(255, 255, 255)); });
    check(whiteControl,
        "Mode zero is a real full-bright white-texture positive control before lighting edits");
    const QVector3D rightSample = ((*corners)[0] + (*corners)[3]) * 0.125f
        + ((*corners)[1] + (*corners)[2]) * 0.375f;
    const bool blueControl = spritePixelNear(
        sampleSpriteWorldPixel(*view, view->grabFramebuffer(), rightSample), QColor(24, 48, 232));
    check(blueControl,
        "Patterned MAT positive control independently renders its blue half rather than a white missing-texture fallback");
    if ( !whiteControl || !blueControl )
    {
        printSpritePixelFailure(*view, project, type, "modes-positive", *corners, leftSample, rightSample);
    }
    const auto initialCamera = view->recoveryState();
    for ( const int mode : { 1, 2, 3 } )
    {
        selectSpriteMode(*pane, "spr.light_mode", mode);
        check(waitForSprite([view, mode]()
            {
                const auto authored = previewSprite(*view);
                return authored && authored->lightMode == mode;
            }), "Changing Sprite light mode publishes its exact engine token");
        check(waitForSprite([&pixel]() { return spritePixelNear(pixel(), QColor(64, 64, 64)); }),
            "Lit Sprite modes evaluate authored extra light against neutral isolated-preview ambient");
    }
    if ( project == ProjectType::IJIM )
    {
        auto* const swatch = pane->findChild<QPushButton*>(QStringLiteral("spr.extra_light.swatch"));
        check(swatch != nullptr, "IJIM Sprite extra-light regression uses the real color picker binding");
        if ( swatch )
        {
            const QCborArray before = ui::resources::captureResourceAuthoringPaneState(pane.get())
                .value(QStringLiteral("sprite")).toArray();
            swatch->click();
            auto* const popup = dynamic_cast<ui::LiveColorPopup*>(
                pane->findChild<QWidget*>(QStringLiteral("spr.extra_light.popup")));
            check(popup != nullptr, "IJIM Sprite RGB preview opens its actual live popup");
            if ( popup )
            {
                popup->setCurrentColor(QColor::fromRgbF(0.25, 0.5, 0.75, 1.0), true);
                check(waitForSprite([&pixel]() { return spritePixelNear(pixel(), QColor(64, 128, 191)); }),
                    "IJIM extra-light color changes visible Sprite pixels before the popup commits");
                check(ui::resources::captureResourceAuthoringPaneState(pane.get())
                        .value(QStringLiteral("sprite")).toArray() == before,
                    "Live IJIM Sprite color preview leaves canonical authored RGBA unchanged until close");
                popup->close();
                check(waitForSprite([view]()
                    {
                        const auto authored = previewSprite(*view);
                        return authored && std::abs(authored->extraLightRGBA[1] - 0.5f) < 0.001f;
                    }), "Closing the IJIM extra-light popup commits the exact chosen channels");
            }
        }
    }
    else
    {
        const std::uint64_t rebuilds = view->meshRebuildCountForTest();
        commitSpriteNumber(*pane, "spr.extra_light.scalar", 0.75);
        check(waitForSprite([&pixel]() { return spritePixelNear(pixel(), QColor(191, 191, 191)); }),
            "Classic Sprite scalar extra-light edits visibly change textured pixels");
        check(view->meshRebuildCountForTest() == rebuilds,
            "Classic extra-light numeric updates retain the existing fixed-card cache path");
    }
    selectSpriteMode(*pane, "spr.light_mode", 0);
    check(waitForSprite([&pixel]() { return spritePixelNear(pixel(), QColor(255, 255, 255)); }),
        "Sprite mode zero bypasses authored extra RGB or scalar light exactly as the engine does");
    const QColor background = view->grabFramebuffer().pixelColor(4, 4);
    for ( const int mode : { 0, 1, 2, 3, 4 } )
    {
        selectSpriteMode(*pane, "spr.geometry_mode", mode);
        check(waitForSprite([view, mode]()
            {
                const auto authored = previewSprite(*view);
                return authored && authored->geometryMode == mode;
            }), "Changing Sprite geometry mode preserves and publishes the exact source integer");
        const QImage frame = view->grabFramebuffer();
        const QColor sample = sampleSpriteWorldPixel(*view, frame, leftSample);
        const QCborArray capturedSprite = ui::resources::captureResourceAuthoringPaneState(pane.get())
            .value(QStringLiteral("sprite")).toArray();
        check(capturedSprite.size() > 4 && capturedSprite.at(4).toInteger() == mode,
            "Each visible Geometry choice is the authored SPR token captured in the raw draft");
        if ( project == ProjectType::IJIM )
        {
            if ( mode < 3 )
            {
                check(spritePixelNear(sample, background),
                    "IJIM isolated preview follows authored None, Vertices and Wireframe geometry without filling the card");
                const int whitePixels = spriteWhitePixelCount(frame);
                check(mode == 0 ? whitePixels == 0 : mode == 1 ? whitePixels >= 4 : whitePixels >= 40,
                    "IJIM authored geometry draws no pixels, corner points, or the closed outer wire");
            }
            else if ( mode == 3 )
            {
                check(spritePixelNear(sample, QColor(255, 255, 255)),
                    "IJIM authored Solid geometry draws its untextured white card");
            }
            else
            {
                check(spritePixelNear(sample, QColor(255, 255, 255)),
                    "IJIM authored Textured geometry restores the actual MAT pixels");
            }
        }
        else if ( mode < 3 )
        {
            check(spritePixelNear(sample, background),
                "Classic Not Drawn, Vertices and Wireframe Sprite modes do not fill the card interior");
            const int whitePixels = spriteWhitePixelCount(frame);
            check(mode == 0 ? whitePixels == 0 : mode == 1 ? whitePixels >= 4 : whitePixels >= 40,
                "Classic Sprite geometry submits no pixels, visible corner vertices or a closed white wire as authored");
            const QVector3D edgeCenter = ((*corners)[0] + (*corners)[1]) * 0.5f;
            check(spriteWhitePixelsNearWorld(*view, frame, edgeCenter) == 0 || mode == 2,
                "Classic Vertices mode draws corners without drawing the Wireframe edge midpoint");
            if ( mode == 2 )
            {
                check(spriteWhitePixelsNearWorld(*view, frame, edgeCenter) >= 2
                        && spritePixelNear(sampleSpriteWorldPixel(*view, frame, spriteCenter(*corners)), background),
                    "Classic Wireframe follows the outer four card edges without a triangulation diagonal");
            }
        }
        else if ( mode == 3 )
        {
            check(sample.red() > 180 && sample.green() < 60 && sample.blue() < 80,
                "Classic Solid Sprite geometry uses the MAT cel Solid palette color rather than its white texture pixels");
        }
        else
        {
            check(spritePixelNear(sample, QColor(255, 255, 255)),
                "Classic Textured Sprite geometry restores its actual patterned MAT pixels");
        }
        if ( const auto authored = previewSprite(*view) )
        {
            const std::string roundTripText = assets::saveToSPRText(*authored,
                project == ProjectType::IJIM ? assets::SprProfile::ijim : assets::SprProfile::classic);
            std::istringstream roundTripInput(roundTripText);
            const assets::SprFile roundTrip = assets::loadFromSPR(roundTripInput);
            check(roundTrip.geometryMode == mode,
                "Every authored Geometry mode survives external SPR serialization and parsing");
        }
    }
    const auto finalCamera = view->recoveryState();
    check(initialCamera.preview.orbit.center == finalCamera.preview.orbit.center
            && initialCamera.preview.orbit.pan == finalCamera.preview.orbit.pan
            && initialCamera.preview.orbit.yawDeg == finalCamera.preview.orbit.yawDeg
            && initialCamera.preview.orbit.pitchDeg == finalCamera.preview.orbit.pitchDeg,
        "Sprite render-mode changes preserve the user's camera pose and pan");
}

void checkSpriteParameterHelpAndTextureModes(
    const std::shared_ptr<const assets::ResourceLocator>& resources,
    const ProjectType project)
{
    auto pane = makeSpritePane(resources, project, project == ProjectType::IJIM ? 0 : 1,
        4.0f, 4.0f, { 0.0f, 0.0f, 0.0f });
    if ( !pane )
    {
        return;
    }
    QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane.get());
    // Imported authored values start a new fixture baseline, not an old journal.
    state.remove(QStringLiteral("history"));
    state.remove(QStringLiteral("presentation"));
    QCborArray sprite = state.value(QStringLiteral("sprite")).toArray();
    sprite[0] = QStringLiteral("sprite_modes.mat");
    sprite[6] = 4;
    state.insert(QStringLiteral("sprite"), sprite);
    QString warning{};
    check(ui::resources::restoreResourceAuthoringPaneState(pane.get(), state, &warning),
        "Sprite parameter fixture preserves an arbitrary imported texture integer through native restore");
    pane->resize(1120, 720);
    pane->show();
    auto* const view = pane->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"));
    auto* const bounds = pane->findChild<QCheckBox*>(QStringLiteral("spr.preview.bounds"));
    auto* const geometry = pane->findChild<QComboBox*>(QStringLiteral("spr.geometry_mode"));
    auto* const scalar = pane->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.extra_light.scalar"));
    auto* const swatch = pane->findChild<QPushButton*>(QStringLiteral("spr.extra_light.swatch"));
    QWidget* const textureField = pane->findChild<QWidget*>(QStringLiteral("spr.texture_mode"));
    check(view && bounds && geometry && scalar && swatch && textureField,
        "Sprite parameter regression exercises the real authored controls");
    if ( !view || !bounds || !geometry || !scalar || !swatch || !textureField )
    {
        return;
    }
    check(bounds->toolTip() == QStringLiteral(
            "Bounds encloses the sprite from every camera angle. XYZ offsets can enlarge it; it is not the current image rectangle. IJIM type 2 follows the oriented image and ignores offsets."),
        "Sprite Bounds help exactly distinguishes an orbit envelope from the current image rectangle");
    const bool ijim = project == ProjectType::IJIM;
    auto* const global = pane->findChild<QComboBox*>(QStringLiteral("spr.preview.geometry_mode"));
    check(global == nullptr,
        "Sprite authoring has no preview-only global geometry selector");
    QWidget* const extraRow = ijim ? swatch->parentWidget() : scalar->parentWidget();
    QLabel* const extraLabel = spriteFieldLabel(*pane, extraRow);
    check(extraLabel && extraLabel->text() == QStringLiteral("Extra light"),
        "Both Sprite profiles use the exact plain Extra light label");
    QLabel* const geometryLabel = spriteFieldLabel(*pane, geometry);
    QLabel* const textureLabel = spriteFieldLabel(*pane, textureField);
    check(geometry->isVisible() && geometryLabel && geometryLabel->isVisible()
            && textureField->isVisible() == !ijim && textureLabel && textureLabel->isVisible() == !ijim,
        "Classic retains authored geometry and texture controls while IJIM exposes authored geometry and hides its ignored texture token");
    bool geometryLabelsAreNames = geometry->count() == 5;
    for ( int index = 0; index < geometry->count(); ++index )
    {
        const QString label = geometry->itemText(index);
        for ( const QChar character : label )
        {
            geometryLabelsAreNames = geometryLabelsAreNames && !character.isDigit();
        }
    }
    check(geometryLabelsAreNames,
        "Sprite Geometry labels use readable names without numeric mode values");
    if ( ijim )
    {
        check(swatch->toolTip() == QStringLiteral(
                "Extra light color (RGBA). RGB lights the sprite except in None (0). Alpha controls opacity in every light mode."),
            "IJIM extra-light color help uses an independent exact RGB and alpha vector");
    }
    else
    {
        check(scalar->toolTip() == QStringLiteral("Extra light intensity")
                && scalar->lineEdit()->toolTip() == QStringLiteral("Extra light intensity"),
            "Classic extra-light field and child editor use the exact intensity help vector");
    }
    bounds->setChecked(false);
    check(waitForSprite([view]()
        {
            const auto authored = previewSprite(*view);
            return view->isValid() && view->thingSpriteCornersForTest(0u).has_value()
                && authored && authored->material == "sprite_modes.mat";
        }),
        "Sprite parameter fixture has a ready rendered card before texture assertions");
    const auto corners = view->thingSpriteCornersForTest(0u);
    if ( !corners )
    {
        return;
    }
    const QVector3D leftSample = ((*corners)[0] + (*corners)[3]) * 0.375f
        + ((*corners)[1] + (*corners)[2]) * 0.125f;
    const QVector3D rightSample = ((*corners)[0] + (*corners)[3]) * 0.125f
        + ((*corners)[1] + (*corners)[2]) * 0.375f;
    const bool textureControl = waitForSprite([view, leftSample, rightSample]()
        {
            const QImage frame = view->grabFramebuffer();
            return spritePixelNear(sampleSpriteWorldPixel(*view, frame, leftSample), QColor(255, 255, 255))
                && spritePixelNear(sampleSpriteWorldPixel(*view, frame, rightSample), QColor(24, 48, 232));
        });
    check(textureControl, "Sprite texture controls start with independent real white and blue interior texels");
    if ( !textureControl )
    {
        printSpritePixelFailure(*view, project, ijim ? 0 : 1, "texture-positive", *corners, leftSample, rightSample);
    }
    if ( !ijim )
    {
        auto* const texture = pane->findChild<QComboBox*>(QStringLiteral("spr.texture_mode"));
        check(texture != nullptr, "Classic Sprite texture mode uses the named algorithm and legacy-value combo");
        if ( texture )
        {
            const std::array<QString, 4> labels{
                QStringLiteral("Affine (0)"), QStringLiteral("Perspective (1)"),
                QStringLiteral("Legacy / unknown (2)"), QStringLiteral("Legacy / unknown (3)") };
            for ( int mode = 0; mode < 4; ++mode )
            {
                const int index = texture->findData(mode);
                check(index >= 0 && texture->itemText(index) == labels[static_cast<std::size_t>(mode)],
                    "Classic texture choices use exact named algorithms and preserve unknown legacy modes");
            }
            for ( const int imported : { 4, -7 } )
            {
                QCborMap importedState = ui::resources::captureResourceAuthoringPaneState(pane.get());
                // This independently imported token is a new authored baseline.
                importedState.remove(QStringLiteral("history"));
                QCborArray importedSprite = importedState.value(QStringLiteral("sprite")).toArray();
                importedSprite[6] = imported;
                importedState.insert(QStringLiteral("sprite"), importedSprite);
                check(ui::resources::restoreResourceAuthoringPaneState(pane.get(), importedState, &warning),
                    "Classic Sprite restores arbitrary texture integers without an invented range clamp");
                const int index = texture->findData(imported);
                check(index >= 0 && texture->currentData().toInt() == imported
                        && texture->itemText(index) == QStringLiteral("Unknown current value (%1)").arg(imported)
                        && ui::resources::captureResourceAuthoringPaneState(pane.get())
                            .value(QStringLiteral("sprite")).toArray().at(6).toInteger() == imported,
                    "Classic unknown texture integers retain exact displayed, native and source identity");
            }
            for ( const int mode : { 0, 1, 2, 3 } )
            {
                texture->setCurrentIndex(texture->findData(mode));
                check(waitForSprite([view, mode]()
                    {
                        const auto authored = previewSprite(*view);
                        return authored && authored->textureMode == mode;
                    }), "Classic texture combo publishes its exact stored token through the actual binding");
                const QImage frame = view->grabFramebuffer();
                check(spritePixelNear(sampleSpriteWorldPixel(*view, frame, leftSample), QColor(255, 255, 255))
                        && spritePixelNear(sampleSpriteWorldPixel(*view, frame, rightSample), QColor(24, 48, 232)),
                    "Affine and perspective mapping retain identical real texels on a constant-depth classic billboard");
            }
        }
        selectSpriteMode(*pane, "spr.light_mode", 1);
        check(waitForSprite([view]()
            {
                const auto authored = previewSprite(*view);
                return authored && authored->lightMode == 1;
            }), "Signed classic extra-light checks use the engine's Not lit mode");
        const std::uint64_t rebuilds = view->meshRebuildCountForTest();
        for ( const float value : { -0.25f, 1.5f } )
        {
            commitSpriteNumber(*pane, "spr.extra_light.scalar", value);
            check(waitForSprite([view, value, leftSample]()
                {
                    const auto authored = previewSprite(*view);
                    return authored && authored->extraLightRGBA[0] == value
                        && spritePixelNear(sampleSpriteWorldPixel(*view, view->grabFramebuffer(), leftSample),
                            value < 0.0f ? QColor(0, 0, 0) : QColor(255, 255, 255));
                }), "Classic signed and HDR extra light stays authored exactly while rendered intensity clamps to zero or one");
            const QCborArray captured = ui::resources::captureResourceAuthoringPaneState(pane.get())
                .value(QStringLiteral("sprite")).toArray();
            bool valid = false;
            const float capturedValue = captured.at(7).toString().toFloat(&valid);
            check(valid && capturedValue == value && scalar->value() == static_cast<double>(value),
                "Classic negative and HDR scalar edits retain their exact native values without authored clamping");
        }
        check(view->meshRebuildCountForTest() == rebuilds,
            "Signed extra-light numeric edits preserve the isolated fixed-card mesh cache");
    }
    else
    {
        QCborMap signedState = ui::resources::captureResourceAuthoringPaneState(pane.get());
        // Seed signed channels independently of the previously captured commands.
        signedState.remove(QStringLiteral("history"));
        QCborArray signedSprite = signedState.value(QStringLiteral("sprite")).toArray();
        signedSprite[4] = 3;
        signedSprite[5] = 1;
        signedSprite[6] = -7;
        // Native slot 7 mirrors RGBA red in slot 8, including for IJIM.
        signedSprite[7] = QStringLiteral("-0.25");
        signedSprite[8] = QStringLiteral("-0.25");
        signedSprite[9] = QStringLiteral("0.5");
        signedSprite[10] = QStringLiteral("1.25");
        signedSprite[11] = QStringLiteral("1");
        signedState.insert(QStringLiteral("sprite"), signedSprite);
        check(ui::resources::restoreResourceAuthoringPaneState(pane.get(), signedState, &warning),
            "IJIM accepts signed and HDR extra-light channels while preserving ignored source tokens");
        check(waitForSprite([view, leftSample]()
            {
                const auto authored = previewSprite(*view);
                return authored && authored->geometryMode == 3 && authored->textureMode == -7
                    && authored->extraLightRGBA[0] == -0.25f && authored->extraLightRGBA[2] == 1.25f
                    && spritePixelNear(sampleSpriteWorldPixel(*view, view->grabFramebuffer(), leftSample), QColor(0, 128, 255));
            }), "IJIM preserves raw signed RGBA and ignored geometry/texture while clamping visible lighting channels");
        const QCborArray capturedSprite = ui::resources::captureResourceAuthoringPaneState(pane.get())
            .value(QStringLiteral("sprite")).toArray();
        if ( capturedSprite != signedSprite )
        {
            std::cerr << "IJIM signed Sprite native preservation expectedSize=" << signedSprite.size()
                << " actualSize=" << capturedSprite.size() << '\n';
            for ( qsizetype index = 0; index < std::max(signedSprite.size(), capturedSprite.size()); ++index )
            {
                const QCborValue expected = index < signedSprite.size() ? signedSprite.at(index) : QCborValue{};
                const QCborValue actual = index < capturedSprite.size() ? capturedSprite.at(index) : QCborValue{};
                if ( expected != actual )
                {
                    std::cerr << "  slot=" << index
                        << " expectedCBOR=" << QCborValue(expected).toCbor().toHex().toStdString()
                        << " actualCBOR=" << QCborValue(actual).toCbor().toHex().toStdString() << '\n';
                }
            }
        }
        check(capturedSprite == signedSprite,
            "Hidden IJIM geometry and texture fields do not rewrite imported tokens or signed channels");
    }
}

void checkIjimSpriteGlobalPreviewGeometry(
    const std::shared_ptr<const assets::ResourceLocator>& resources,
    const int type)
{
    auto pane = makeSpritePane(resources, ProjectType::IJIM, type, 4.0f, 4.0f, { 0.0f, 0.0f, 0.0f });
    if ( !pane )
    {
        return;
    }
    QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane.get());
    // Replace the authored fixture rather than replaying its former material journal.
    state.remove(QStringLiteral("history"));
    state.remove(QStringLiteral("presentation"));
    QCborArray sprite = state.value(QStringLiteral("sprite")).toArray();
    sprite[0] = QStringLiteral("sprite_modes.mat");
    sprite[4] = 4;
    sprite[6] = -7;
    state.insert(QStringLiteral("sprite"), sprite);
    QString warning{};
    check(ui::resources::restoreResourceAuthoringPaneState(pane.get(), state, &warning),
        "IJIM authored-geometry fixture keeps its exact stored geometry and ignored texture tokens");
    pane->resize(1120, 720);
    pane->show();
    auto* const view = pane->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"));
    auto* const bounds = pane->findChild<QCheckBox*>(QStringLiteral("spr.preview.bounds"));
    auto* const global = pane->findChild<QComboBox*>(QStringLiteral("spr.preview.geometry_mode"));
    auto* const geometry = pane->findChild<QComboBox*>(QStringLiteral("spr.geometry_mode"));
    check(view && bounds && geometry, "IJIM authored-geometry regression uses the actual rendered authoring preview and source control");
    check(!global && geometry && geometry->isVisible() && geometry->currentData().toInt() == 4,
        "IJIM exposes authored Geometry and has no preview-only global selector");
    if ( !view || !bounds || !geometry )
    {
        return;
    }
    bounds->setChecked(false);
    check(waitForSprite([view]() { return view->isValid() && view->thingSpriteCornersForTest(0u).has_value(); }),
        "IJIM authored geometry starts from a ready textured card");
    const auto corners = view->thingSpriteCornersForTest(0u);
    if ( !corners )
    {
        return;
    }
    const QVector3D leftSample = ((*corners)[0] + (*corners)[3]) * 0.375f
        + ((*corners)[1] + (*corners)[2]) * 0.125f;
    const QVector3D rightSample = ((*corners)[0] + (*corners)[3]) * 0.125f
        + ((*corners)[1] + (*corners)[2]) * 0.375f;
    check(waitForSprite([view, leftSample, rightSample]()
        {
            const QImage frame = view->grabFramebuffer();
            return spritePixelNear(sampleSpriteWorldPixel(*view, frame, leftSample), QColor(255, 255, 255))
                && spritePixelNear(sampleSpriteWorldPixel(*view, frame, rightSample), QColor(24, 48, 232));
        }), "IJIM authored geometry has real white and blue texture positive controls before edits");
    const auto beforeCamera = view->recoveryState();
    const std::uint64_t rebuilds = view->meshRebuildCountForTest();
    const QCborArray authoredBefore = ui::resources::captureResourceAuthoringPaneState(pane.get())
        .value(QStringLiteral("sprite")).toArray();
    const QColor background = view->grabFramebuffer().pixelColor(4, 4);
    for ( const int mode : { 0, 1, 2, 3, 4 } )
    {
        geometry->setCurrentIndex(geometry->findData(mode));
        check(waitForSprite([view, mode]()
            {
                const auto authored = previewSprite(*view);
                return authored && authored->geometryMode == mode;
            }), "IJIM Geometry edits publish their exact authored SPR integer before raster inspection");
        const QImage frame = view->grabFramebuffer();
        const QColor left = sampleSpriteWorldPixel(*view, frame, leftSample);
        const QColor right = sampleSpriteWorldPixel(*view, frame, rightSample);
        if ( mode < 3 )
        {
            check(spritePixelNear(left, background) && spritePixelNear(right, background),
                "IJIM authored None, Vertices and Wireframe modes do not fill the Sprite interior");
            const int whitePixels = spriteWhitePixelCount(frame);
            check(mode == 0 ? whitePixels == 0 : mode == 1 ? whitePixels >= 4 : whitePixels >= 40,
                "IJIM authored geometry draws no pixels, white corner points or a closed white outer wire");
            const QVector3D edgeCenter = ((*corners)[0] + (*corners)[1]) * 0.5f;
            check(mode == 2 ? spriteWhitePixelsNearWorld(*view, frame, edgeCenter) >= 2
                           : spriteWhitePixelsNearWorld(*view, frame, edgeCenter) == 0,
                "IJIM authored Vertices and Wireframe differ at the outer edge midpoint");
            check(spritePixelNear(sampleSpriteWorldPixel(*view, frame, spriteCenter(*corners)), background),
                "IJIM authored wireframe has no internal triangle diagonal");
        }
        else if ( mode == 3 )
        {
            check(spritePixelNear(left, QColor(255, 255, 255)) && spritePixelNear(right, QColor(255, 255, 255)),
                "IJIM authored Solid omits the MAT texture and classic palette Solid color");
        }
        else
        {
            check(spritePixelNear(left, QColor(255, 255, 255)) && spritePixelNear(right, QColor(24, 48, 232)),
                "IJIM authored Textured restores both independent actual MAT texels");
        }
        const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(pane.get());
        const QCborArray capturedSprite = captured.value(QStringLiteral("sprite")).toArray();
        const bool exactAuthoredAndDirty = capturedSprite.size() > 4
            && capturedSprite.at(4).toInteger() == mode
            && captured.value(QStringLiteral("modified")).toBool();
        if ( !exactAuthoredAndDirty )
        {
            const QCborMap history = captured.value(QStringLiteral("history")).toMap();
            std::fprintf(stderr,
                "IJIM sprite authored-state diagnostic: type=%d mode=%d rawMode=%lld modified=%d cursor=%lld clean=%lld commandCount=%lld\n",
                type, mode, capturedSprite.size() > 4 ? static_cast<long long>(capturedSprite.at(4).toInteger()) : -1LL,
                static_cast<int>(captured.value(QStringLiteral("modified")).toBool()),
                static_cast<long long>(history.value(QStringLiteral("cursor")).toInteger(-1)),
                static_cast<long long>(history.value(QStringLiteral("clean")).toInteger(-1)),
                static_cast<long long>(history.value(QStringLiteral("commands")).toArray().size()));
        }
        check(exactAuthoredAndDirty,
            "IJIM Geometry updates the exact authored token and its dirty state");
        check(!captured.value(QStringLiteral("presentation")).toMap()
                .contains(QStringLiteral("globalGeometryMode")),
            "IJIM geometry is not stored as preview presentation state");
        const auto currentCorners = view->thingSpriteCornersForTest(0u);
        const bool privatePreviewCacheRetained = view->meshRebuildCountForTest() == rebuilds
            && currentCorners == corners;
        if ( !privatePreviewCacheRetained )
        {
            std::fprintf(stderr,
                "IJIM sprite cache diagnostic: type=%d mode=%d rebuildsBefore=%llu rebuildsAfter=%llu cornersRetained=%d\n",
                type, mode, static_cast<unsigned long long>(rebuilds),
                static_cast<unsigned long long>(view->meshRebuildCountForTest()),
                static_cast<int>(currentCorners == corners));
        }
        check(privatePreviewCacheRetained,
            "IJIM authored geometry retains the exact fixed-card vertices and private Level mesh cache");
    }
    const auto afterCamera = view->recoveryState();
    check(beforeCamera.preview.orbit.center == afterCamera.preview.orbit.center
            && beforeCamera.preview.orbit.pan == afterCamera.preview.orbit.pan
            && beforeCamera.preview.orbit.yawDeg == afterCamera.preview.orbit.yawDeg
            && beforeCamera.preview.orbit.pitchDeg == afterCamera.preview.orbit.pitchDeg
            && beforeCamera.preview.orthoScale == afterCamera.preview.orthoScale,
        "IJIM authored geometry inspection preserves exact camera framing and pose");
    const bool undoPublished = ui::resources::resourceAuthoringPaneDocumentUndoRedo(pane.get(), false)
            && waitForSprite([view]()
                {
                    const auto authored = previewSprite(*view);
                    return authored && authored->geometryMode == 3;
                });
    if ( !undoPublished )
    {
        const auto authored = previewSprite(*view);
        const QCborMap history = ui::resources::captureResourceAuthoringPaneState(pane.get())
            .value(QStringLiteral("history")).toMap();
        std::fprintf(stderr,
            "IJIM sprite Undo diagnostic: type=%d undoPublished=%d previewMode=%d cursor=%lld clean=%lld\n",
            type, static_cast<int>(undoPublished), authored ? authored->geometryMode : -999,
            static_cast<long long>(history.value(QStringLiteral("cursor")).toInteger(-1)),
            static_cast<long long>(history.value(QStringLiteral("clean")).toInteger(-1)));
    }
    check(undoPublished,
        "IJIM authored Geometry Undo restores the prior raw SPR mode");
    QCborMap undone = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborArray undoneSprite = undone.value(QStringLiteral("sprite")).toArray();
    const QColor undoPixel = sampleSpriteWorldPixel(*view, view->grabFramebuffer(), leftSample);
    const bool undoExact = undoneSprite.size() > 4 && undoneSprite.at(4).toInteger() == 3
            && undone.value(QStringLiteral("modified")).toBool()
            && spritePixelNear(undoPixel, QColor(255, 255, 255));
    if ( !undoExact )
    {
        std::fprintf(stderr,
            "IJIM sprite Undo-state diagnostic: type=%d rawMode=%lld modified=%d pixel=%d,%d,%d\n",
            type, undoneSprite.size() > 4 ? static_cast<long long>(undoneSprite.at(4).toInteger()) : -1LL,
            static_cast<int>(undone.value(QStringLiteral("modified")).toBool()),
            undoPixel.red(), undoPixel.green(), undoPixel.blue());
    }
    check(undoExact,
        "IJIM Geometry Undo restores the authored raw token, dirty marker, and Solid raster");
    const bool redoPublished = ui::resources::resourceAuthoringPaneDocumentUndoRedo(pane.get(), true)
            && waitForSprite([view]()
                {
                    const auto authored = previewSprite(*view);
                    return authored && authored->geometryMode == 4;
                });
    if ( !redoPublished )
    {
        const auto authored = previewSprite(*view);
        const QCborMap history = ui::resources::captureResourceAuthoringPaneState(pane.get())
            .value(QStringLiteral("history")).toMap();
        std::fprintf(stderr,
            "IJIM sprite Redo diagnostic: type=%d redoPublished=%d previewMode=%d cursor=%lld clean=%lld\n",
            type, static_cast<int>(redoPublished), authored ? authored->geometryMode : -999,
            static_cast<long long>(history.value(QStringLiteral("cursor")).toInteger(-1)),
            static_cast<long long>(history.value(QStringLiteral("clean")).toInteger(-1)));
    }
    check(redoPublished,
        "IJIM authored Geometry Redo restores the exact prior raw SPR mode");
    QCborMap redone = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborArray redoneSprite = redone.value(QStringLiteral("sprite")).toArray();
    const QCborMap redoneHistory = redone.value(QStringLiteral("history")).toMap();
    const bool redoExact = redoneSprite.size() > 4 && redoneSprite.at(4).toInteger() == 4
            && redone.value(QStringLiteral("modified")).toBool()
            && !redone.value(QStringLiteral("presentation")).toMap()
                .contains(QStringLiteral("globalGeometryMode"));
    if ( !redoExact )
    {
        std::fprintf(stderr,
            "IJIM sprite Redo-state diagnostic: type=%d rawMode=%lld modified=%d cursor=%lld clean=%lld\n",
            type, redoneSprite.size() > 4 ? static_cast<long long>(redoneSprite.at(4).toInteger()) : -1LL,
            static_cast<int>(redone.value(QStringLiteral("modified")).toBool()),
            static_cast<long long>(redoneHistory.value(QStringLiteral("cursor")).toInteger(-1)),
            static_cast<long long>(redoneHistory.value(QStringLiteral("clean")).toInteger(-1)));
    }
    check(redoExact,
        "IJIM Geometry Redo restores the raw Textured token and sticky dirty state without presentation state");
    QCborMap restoredState = ui::resources::captureResourceAuthoringPaneState(pane.get());
    check(ui::resources::restoreResourceAuthoringPaneState(pane.get(), restoredState, &warning),
        "IJIM authored geometry restores through the native raw draft boundary");
    check(geometry->currentData().toInt() == 4
            && ui::resources::captureResourceAuthoringPaneState(pane.get())
                .value(QStringLiteral("sprite")).toArray() == authoredBefore,
        "Native IJIM restore returns the authored Geometry control and exact source field");
}

void checkProfileGeometry(
    const std::shared_ptr<const assets::ResourceLocator>& resources,
    const ProjectType project,
    const int type)
{
    constexpr std::array<float, 3> offset{ 1.25f, 3.5f, 2.0f };
    auto pane = makeSpritePane(resources, project, type, 4.0f, 2.0f, offset);
    if ( !pane )
    {
        check(false, "Sprite geometry pane is available");
        return;
    }
    pane->resize(1120, 720);
    pane->show();
    auto* const view = pane->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"));
    check(view != nullptr, "Sprite profile geometry uses the production GlView preview");
    if ( !view )
    {
        return;
    }
    static constexpr std::array<const char*, 3> offsetNames{
        "spr.offset.x", "spr.offset.y", "spr.offset.z" };
    const std::array<QString, 3> expectedOffsetHelp{
        QStringLiteral("Moves the billboard along camera right. This direction turns with the view."),
        QStringLiteral("Moves the billboard along camera depth. In an orthographic view, this does not change its on-screen position or size."),
        QStringLiteral("Moves the billboard along camera up. This direction turns with the view.") };
    for ( std::size_t axis = 0; axis < offsetNames.size(); ++axis )
    {
        auto* const control = pane->findChild<ui::ScrubFloatEdit*>(QString::fromLatin1(offsetNames[axis]));
        check(control && control->toolTip() == expectedOffsetHelp[axis]
                && control->lineEdit()->toolTip() == control->toolTip(),
            "Sprite XYZ hover help explains camera-right, depth and up on the actual numeric edit");
    }
    const auto* const boundsControl = pane->findChild<QCheckBox*>(QStringLiteral("spr.preview.bounds"));
    check(boundsControl && boundsControl->isChecked(),
        "Sprite Bounds are enabled by default without a saved presentation override");
    for ( const char* const name : { "spr.offset.x", "spr.offset.y", "spr.offset.z" } )
    {
        auto* const offsetControl = pane->findChild<ui::ScrubFloatEdit*>(QString::fromLatin1(name));
        const auto* const form = offsetControl && offsetControl->parentWidget()
            ? qobject_cast<QFormLayout*>(offsetControl->parentWidget()->layout()) : nullptr;
        const QWidget* const label = form && offsetControl ? form->labelForField(offsetControl) : nullptr;
        const bool expectedVisible = !(project == ProjectType::IJIM && type == 2);
        check(offsetControl && label && offsetControl->isVisible() == expectedVisible
                && label->isVisible() == expectedVisible,
            "Only IJIM type 2 hides each ignored XYZ offset field and its label");
    }
    if ( project != ProjectType::IJIM )
    {
        const auto* const types = pane->findChild<QComboBox*>(QStringLiteral("spr.type"));
        for ( const int admitted : { 0, 1, 2 } )
        {
            const int index = types ? types->findData(admitted) : -1;
            check(index >= 0 && !types->itemText(index).contains(QStringLiteral("Unsupported")),
                "Classic Sprite type control offers every original-engine admitted billboard type as supported");
        }
    }
    check(waitForSprite([view, type]()
        {
            const auto sprite = previewSprite(*view);
            return view->isValid() && view->attachedLevelForTest() && sprite && sprite->type == type;
        }), "Sprite profile geometry publishes a valid OpenGL draft preview");
    const QImage framebuffer = view->grabFramebuffer();
    check(!framebuffer.isNull(), "Sprite profile geometry has a real rendered framebuffer");
    const auto corners = view->thingSpriteCornersForTest(0u);
    if ( project == ProjectType::IJIM && type == 1 )
    {
        check(!corners,
            "IJIM type 1 preserves its source token and has no original-engine draw geometry");
        return;
    }
    check(corners.has_value(),
        "JKDF2 and MotS ignore stored sprite type; IJIM type 0 and 2 produce a quad");
    if ( !corners )
    {
        return;
    }
    const QVector3D expectedCenter = project == ProjectType::IJIM && type == 2
        ? QVector3D{}
        : QVector3D(offset[0], offset[1], offset[2]);
    check(closeQVec(spriteCenter(*corners), expectedCenter),
        "Straight-on billboard XYZ is camera-space; IJIM type 2 ignores all source offsets");
    check(close(((*corners)[1] - (*corners)[0]).length(), 4.0)
            && close(((*corners)[2] - (*corners)[1]).length(), 2.0),
        "Sprite width and height preserve original-engine card dimensions");

    auto cameraState = view->recoveryState();
    cameraState.preview.orbit.yawDeg = 90.0f;
    cameraState.preview.orbit.pitchDeg = 0.0f;
    cameraState.preview.orbit.rollDeg = 0.0f;
    view->restoreRecoveryState(cameraState, false);
    view->rebuildSpriteVerticesForTest();
    const auto rotated = view->thingSpriteCornersForTest(0u);
    check(rotated.has_value(), "Sprite geometry survives preview camera rotation");
    if ( rotated )
    {
        // OpenJKDF2 rdSprite_Draw and IJIM type 0 add offsets in view space.
        // At yaw 90 the camera right is +Y and camera forward is -X.
        const QVector3D expectedRotated = project == ProjectType::IJIM && type == 2
            ? QVector3D{}
            : QVector3D(-offset[1], offset[0], offset[2]);
        check(closeQVec(spriteCenter(*rotated), expectedRotated),
            "Billboard offsets rotate with the camera while IJIM type-2 offsets stay ignored");
        if ( project == ProjectType::IJIM && type == 2 )
        {
            check(*rotated == *corners,
                "IJIM oriented sprite quad stays fixed during camera orbit");
        }
    }
    const auto authored = ui::resources::captureResourceAuthoringPaneState(pane.get());
    check(authored.value(QStringLiteral("sprite")).toArray().at(1).toInteger() == type,
        "Renderer profile semantics preserve the authored stored sprite type");
    if ( project == ProjectType::IJIM && type == 0 )
    {
        auto* const types = pane->findChild<QComboBox*>(QStringLiteral("spr.type"));
        for ( const int selectedType : { 2, 0 } )
        {
            types->setCurrentIndex(types->findData(selectedType));
            for ( const char* const name : { "spr.offset.x", "spr.offset.y", "spr.offset.z" } )
            {
                auto* const control = pane->findChild<ui::ScrubFloatEdit*>(QString::fromLatin1(name));
                check(control && control->isVisible() == (selectedType == 0),
                    "Changing IJIM Sprite type immediately updates ignored-offset row visibility");
            }
            const QCborArray changed = ui::resources::captureResourceAuthoringPaneState(pane.get())
                .value(QStringLiteral("sprite")).toArray();
            for ( qsizetype axis = 0; axis < 3; ++axis )
            {
                check(changed.at(12 + axis) == authored.value(QStringLiteral("sprite")).toArray().at(12 + axis),
                    "Hiding ignored IJIM type-2 offset controls preserves every raw authored offset");
            }
        }
    }
}

void checkRepeatedGeometryEditsAndRecenter(
    const std::shared_ptr<const assets::ResourceLocator>& resources,
    const ProjectType project,
    const int type,
    const bool wide,
    const bool clipEnabled)
{
    auto pane = makeSpritePane(resources, project, type, 1.0f, 1.0f, { 0.0f, 0.0f, 0.0f });
    if ( !pane )
    {
        return;
    }
    pane->resize(wide ? 1280 : 760, wide ? 640 : 1080);
    if ( auto* const splitter = pane->findChild<QSplitter*>(QStringLiteral("spr.editor_splitter")) )
    {
        splitter->setSizes({ 300, wide ? 940 : 420 });
    }
    pane->show();
    auto* const view = pane->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"));
    auto* const bounds = pane->findChild<QCheckBox*>(QStringLiteral("spr.preview.bounds"));
    auto* const recenter = pane->findChild<QPushButton*>(QStringLiteral("spr.preview.recenter"));
    check(view && bounds && recenter, "Sprite edit/framing regression has preview controls");
    if ( !view || !bounds || !recenter )
    {
        return;
    }
    check(waitForSprite([view]()
        {
            return view->isValid() && view->thingSpriteCornersForTest(0u).has_value();
        }), "Sprite geometry edit fixture publishes its initial card");
    check(waitForSprite([view]()
        {
            return hasSpritePixels(view->grabFramebuffer());
        }), "Synthetic MAT and CMP render visible red texture pixels before any geometry edit");
    bounds->setChecked(true);
    auto cameraState = view->recoveryState();
    cameraState.preview.clipEnabled = clipEnabled;
    cameraState.preview.orbit.yawDeg = 30.0f;
    cameraState.preview.orbit.pitchDeg = 15.0f;
    cameraState.preview.orbit.pan = QVector3D(0.25f, 0.1f, 0.2f);
    view->restoreRecoveryState(cameraState, false);
    const double aspect = static_cast<double>(view->width()) / view->height();
    check(wide ? aspect >= 1.25 : aspect <= 0.8,
        "Sprite edit regression exercises the requested wide or tall preview aspect");

    // At yaw 30/pitch 15 the conservative billboard cube for dimension 32768
    // has a projected depth span greater than the clip-disabled 200000 range.
    // 16384 fits that range; enabled clipping also exercises the larger edit.
    const std::array<double, 4> dimensions{ 2.0, 4.0, 12.0, clipEnabled ? 32768.0 : 16384.0 };
    for ( const double dimension : dimensions )
    {
        const auto beforeEdit = view->recoveryState().preview.orbit;
        commitSpriteNumber(*pane, "spr.width", dimension);
        commitSpriteNumber(*pane, "spr.height", dimension * 0.5);
        commitSpriteNumber(*pane, "spr.offset.x", dimension * 0.75);
        commitSpriteNumber(*pane, "spr.offset.y", dimension * 1.5);
        commitSpriteNumber(*pane, "spr.offset.z", dimension * 0.5);
        check(waitForSprite([view, dimension]()
            {
                const auto sprite = previewSprite(*view);
                return sprite && sprite->width == dimension && sprite->height == dimension * 0.5
                    && sprite->offsetXYZ[1] == dimension * 1.5;
            }), "Repeated width, height and XYZ edits publish their exact authored geometry");
        const QImage editedFrame = view->grabFramebuffer();
        const auto editedCorners = view->thingSpriteCornersForTest(0u);
        if ( !editedCorners || !cornersInsideFrame(*view, *editedCorners)
            || !boundsInsideFrame(*view) || !hasSpritePixels(editedFrame) )
        {
            printSpriteFramingFailure(*view, project, type, dimension, "edit");
        }
        check(editedCorners && cornersInsideFrame(*view, *editedCorners)
                && boundsInsideFrame(*view),
            "Sprite geometry edits expand framing so card and Bounds remain visible before Recenter");
        const auto afterEdit = view->recoveryState().preview.orbit;
        check(afterEdit.yawDeg == beforeEdit.yawDeg && afterEdit.pitchDeg == beforeEdit.pitchDeg
                && afterEdit.rollDeg == beforeEdit.rollDeg && afterEdit.center == beforeEdit.center
                && afterEdit.pan == beforeEdit.pan,
            "Sprite geometry edits preserve the user's orbit, target and pan while expanding framing");
        check(hasSpritePixels(editedFrame),
            "Sprite geometry edits keep synthetic MAT texture pixels visible before Recenter");
        check(view->cameraClipEnabled() == clipEnabled,
            "Sprite geometry framing preserves the user's clipping presentation choice");
        recenter->click();
        const QImage recenteredFrame = view->grabFramebuffer();
        const auto recenteredCorners = view->thingSpriteCornersForTest(0u);
        if ( !recenteredCorners || !cornersInsideFrame(*view, *recenteredCorners)
            || !boundsInsideFrame(*view) || !hasSpritePixels(recenteredFrame) )
        {
            printSpriteFramingFailure(*view, project, type, dimension, "recenter");
        }
        check(hasSpritePixels(recenteredFrame),
            "Recenter keeps the edited synthetic MAT texture visible in the real framebuffer");
        const auto corners = view->thingSpriteCornersForTest(0u);
        check(corners && cornersInsideFrame(*view, *corners),
            "Recenter keeps the edited textured Sprite card inside the viewport");
        check(boundsInsideFrame(*view),
            "Recenter fits the same Sprite bounds that the preview actually draws after XYZ edits");
        if ( corners )
        {
            for ( const QVector3D& corner : *corners )
            {
                const float depth = QVector3D::dotProduct(corner - view->cameraEye(), view->cameraForward());
                const float nearPlane = clipEnabled ? view->cameraNearPlane() : -100000.0f;
                const float farPlane = clipEnabled ? view->cameraFarPlane() : 100000.0f;
                check(depth >= nearPlane && depth <= farPlane,
                    "Edited Sprite card remains between the actual preview near and far planes");
            }
        }
        const auto authored = ui::resources::captureResourceAuthoringPaneState(pane.get());
        const QCborArray sprite = authored.value(QStringLiteral("sprite")).toArray();
        check(sprite.at(12).toString().toDouble() == dimension * 0.75
                && sprite.at(13).toString().toDouble() == dimension * 1.5
                && sprite.at(14).toString().toDouble() == dimension * 0.5,
            "Recenter never changes authored Sprite XYZ offsets");
    }

    if ( !clipEnabled && !(project == ProjectType::IJIM && type == 2) )
    {
        auto state = view->recoveryState();
        state.preview.orbit.yawDeg = 30.0f;
        state.preview.orbit.pitchDeg = 15.0f;
        view->restoreRecoveryState(state, false);
        const auto before = view->recoveryState();
        constexpr double dimension = 32768.0;
        const double radius = std::hypot(dimension * 0.75, dimension * 1.5, dimension * 0.5)
            + std::hypot(dimension * 0.5, dimension * 0.25);
        const auto basis = app::ViewportCamera::BasisFromOrbit(30.0f, 15.0f);
        const double projectedDepthSpan = 2.0 * radius
            * (std::abs(basis.forward.x()) + std::abs(basis.forward.y()) + std::abs(basis.forward.z()));
        check(projectedDepthSpan > 200000.0,
            "Disabled-clipping failure fixture analytically exceeds the fixed camera depth span");
        commitSpriteNumber(*pane, "spr.width", dimension);
        commitSpriteNumber(*pane, "spr.height", dimension * 0.5);
        commitSpriteNumber(*pane, "spr.offset.x", dimension * 0.75);
        commitSpriteNumber(*pane, "spr.offset.y", dimension * 1.5);
        commitSpriteNumber(*pane, "spr.offset.z", dimension * 0.5);
        check(waitForSprite([view]()
            {
                const auto sprite = previewSprite(*view);
                return sprite && sprite->width == dimension && sprite->offsetXYZ[1] == dimension * 1.5;
            }), "Impossible depth fit still publishes the exact authored Sprite resource");
        auto* const status = pane->findChild<QLabel*>(QStringLiteral("resource_authoring.status"));
        check(status && status->text() == QStringLiteral("Sprite preview exceeds the camera's depth range."),
            "Impossible rotated Bounds depth fit reports its finite camera limitation explicitly");
        const auto after = view->recoveryState();
        const auto& previousPose = before.preview.orbit;
        const auto& currentPose = after.preview.orbit;
        check(currentPose.center == previousPose.center && currentPose.pan == previousPose.pan
                && currentPose.yawDeg == previousPose.yawDeg && currentPose.pitchDeg == previousPose.pitchDeg
                && currentPose.rollDeg == previousPose.rollDeg && currentPose.distance == previousPose.distance
                && after.preview.orthoScale == before.preview.orthoScale
                && after.nearPlane == before.nearPlane && after.farPlane == before.farPlane
                && !after.preview.clipEnabled,
            "Failed Sprite fit leaves the prior orbit, framing and depth settings intact");
        const QCborArray source = ui::resources::captureResourceAuthoringPaneState(pane.get())
            .value(QStringLiteral("sprite")).toArray();
        check(source.at(2).toString().toDouble() == dimension
                && source.at(3).toString().toDouble() == dimension * 0.5
                && source.at(12).toString().toDouble() == dimension * 0.75
                && source.at(13).toString().toDouble() == dimension * 1.5
                && source.at(14).toString().toDouble() == dimension * 0.5,
            "Impossible Sprite fit retains every authored dimension and offset without clamping");
    }
}

void checkUnavailableDefaultDepthIsExplicit(
    const std::shared_ptr<const assets::ResourceLocator>& resources,
    const ProjectType project)
{
    auto pane = makeSpritePane(resources, project, 0, 1.0f, 1.0f, { 0.0f, 200000.0f, 0.0f });
    if ( !pane )
    {
        return;
    }
    pane->resize(1120, 720);
    pane->show();
    auto* const view = pane->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"));
    auto* const status = pane->findChild<QLabel*>(QStringLiteral("resource_authoring.status"));
    check(view && status, "Sprite depth-limit regression has the real preview and status label");
    if ( !view || !status )
    {
        return;
    }
    auto state = view->recoveryState();
    state.preview.clipEnabled = false;
    view->restoreRecoveryState(state, false);
    check(waitForSprite([view]()
        {
            const auto sprite = previewSprite(*view);
            return sprite && sprite->offsetXYZ[1] == 200000.0f;
        }), "Sprite depth-limit regression publishes the large finite source offset");
    check(status->text() == QStringLiteral("Sprite preview exceeds the camera's depth range."),
        "A Sprite outside the unchanged default depth range reports its preview limitation explicitly");
    check(!view->cameraClipEnabled(),
        "Depth-limit reporting does not silently change the user's clipping presentation choice");
    const QCborArray authored = ui::resources::captureResourceAuthoringPaneState(pane.get())
        .value(QStringLiteral("sprite")).toArray();
    check(authored.at(13).toString().toDouble() == 200000.0,
        "Depth-limit reporting preserves the exact large authored offset instead of clamping it");
}

void checkUnknownSpriteTypeIsPreservedWithoutGeometry(
    const std::shared_ptr<const assets::ResourceLocator>& resources,
    const ProjectType project,
    const int type)
{
    // OpenJKDF2 sithSprite_Load converts the token to uint32_t and admits
    // type_id <= 2. Both 17 and -1 therefore fail original SPR admission,
    // even though rdSprite_NewEntry itself stores any integer unchecked.
    auto pane = makeSpritePane(resources, project, type, 4.0f, 2.0f, { 1.25f, 3.5f, 2.0f });
    if ( !pane )
    {
        return;
    }
    pane->resize(1120, 720);
    pane->show();
    auto* const view = pane->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"));
    check(view != nullptr, "Unknown Sprite type regression has the production preview");
    if ( !view )
    {
        return;
    }
    check(waitForSprite([view, type]()
        {
            const auto sprite = previewSprite(*view);
            return view->isValid() && view->attachedLevelForTest() && sprite && sprite->type == type;
        }), "Unknown Sprite type remains available in the exact preview resource text");
    check(!view->grabFramebuffer().isNull(),
        "Unknown Sprite type keeps a usable preview framebuffer");
    check(!view->thingSpriteCornersForTest(0u),
        "Unknown Sprite type rejected by original engine admission produces no textured quad");
    const QCborArray authored = ui::resources::captureResourceAuthoringPaneState(pane.get())
        .value(QStringLiteral("sprite")).toArray();
    check(authored.at(1).toInteger() == type,
        "Unknown Sprite type no-preview policy preserves the exact raw authored integer");
}

bool sameSpriteCamera(const GlView::RecoveryState& left, const GlView::RecoveryState& right)
{
    const auto& a = left.preview.orbit;
    const auto& b = right.preview.orbit;
    return a.center == b.center && a.pan == b.pan && a.radius == b.radius && a.distance == b.distance
        && a.yawDeg == b.yawDeg && a.pitchDeg == b.pitchDeg && a.rollDeg == b.rollDeg
        && left.preview.projectionType == right.preview.projectionType
        && left.preview.fovXDeg == right.preview.fovXDeg
        && left.preview.orthoScale == right.preview.orthoScale
        && left.preview.clipEnabled == right.preview.clipEnabled
        && left.nearPlane == right.nearPlane && left.farPlane == right.farPlane;
}

void checkLiveSpriteNumericScrubs(
    const std::shared_ptr<const assets::ResourceLocator>& resources,
    const ProjectType project, const int type)
{
    auto pane = makeSpritePane(resources, project, type, 1.0f, 1.0f, { 0.0f, 0.0f, 0.0f });
    if ( !pane )
    {
        return;
    }
    pane->resize(1120, 720);
    pane->show();
    auto* const view = pane->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"));
    check(view && waitForSprite([view]() { return view->isValid() && hasSpritePixels(view->grabFramebuffer()); }),
        "Continuous Sprite scrub fixture has a rendered positive-control texture");
    if ( !view )
    {
        return;
    }
    auto camera = view->recoveryState();
    camera.preview.orbit.yawDeg = 30.0f;
    camera.preview.orbit.pitchDeg = 15.0f;
    camera.preview.orbit.pan = QVector3D(0.1f, 0.05f, 0.1f);
    view->restoreRecoveryState(camera, false);
    const std::uint64_t rebuilds = view->meshRebuildCountForTest();
    const std::array<const char*, 5> names{
        "spr.width", "spr.height", "spr.offset.x", "spr.offset.y", "spr.offset.z" };
    const auto numericValue = [](const assets::SprFile& sprite, const std::size_t field)
    {
        return field == 0u ? sprite.width : field == 1u ? sprite.height : sprite.offsetXYZ[field - 2u];
    };
    for ( std::size_t field = 0u; field < names.size(); ++field )
    {
        auto* const control = pane->findChild<ui::ScrubFloatEdit*>(QString::fromLatin1(names[field]));
        check(control && control->isVisible(), "Continuous Sprite scrub exercises a visible authored numeric field");
        if ( !control )
        {
            continue;
        }
        QLineEdit* const line = control->lineEdit();
        const QPoint start(line->width() / 2, line->height() / 2);
        const QPoint startGlobal = line->mapToGlobal(start);
        const auto before = view->recoveryState().preview.orbit;
        const auto original = previewSprite(*view);
        float previous = original ? numericValue(*original, field) : 0.0f;
        int visibleUpdates = 0;
        int publishedUpdates = 0;
        int texturedUpdates = 0;
        int dimensionMatches = 0;
        int centerMatches = 0;
        int boundsMatches = 0;
        int framedUpdates = 0;
        int moveStep = 0;
        int startedGestures = 0;
        int finishedGestures = 0;
        int finishedAtStep = -1;
        std::array<double, 40> controlSamples{};
        std::array<double, 40> previewSamples{};
        const QPoint physicalStart = QCursor::pos();
        const auto started = QObject::connect(control, &ui::ScrubFloatEdit::scrubStarted, pane.get(),
            [&startedGestures](double) { ++startedGestures; });
        const auto finished = QObject::connect(control, &ui::ScrubFloatEdit::scrubFinished, pane.get(),
            [&finishedGestures, &finishedAtStep, &moveStep](double)
            {
                ++finishedGestures;
                finishedAtStep = moveStep;
            });
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(start), QPointF(startGlobal),
            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(line, &press);
        QPoint last = start;
        QPoint lastGlobal = startGlobal;
        for ( int step = 1; step <= 40; ++step )
        {
            moveStep = step;
            last = start + QPoint(step * 2, 0);
            lastGlobal = startGlobal + QPoint(step * 2, 0);
            QMouseEvent move(QEvent::MouseMove, QPointF(last), QPointF(lastGlobal),
                Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(line, &move);
            // Keep input arriving faster than the old 90 ms debounce. The
            // preview must publish multiple intermediate values while held.
            QElapsedTimer interval{};
            interval.start();
            while ( interval.elapsed() < 10 )
            {
                QApplication::processEvents(QEventLoop::AllEvents);
                if ( const auto published = previewSprite(*view) )
                {
                    const float value = numericValue(*published, field);
                    if ( value != previous )
                    {
                        previous = value;
                        ++publishedUpdates;
                        const QImage frame = view->grabFramebuffer();
                        const auto corners = view->thingSpriteCornersForTest(0u);
                        const auto pose = view->recoveryState().preview.orbit;
                        const auto basis = app::ViewportCamera::BasisFromOrbit(
                            pose.yawDeg, pose.pitchDeg, pose.rollDeg);
                        const QVector3D center = basis.right * published->offsetXYZ[0]
                            + basis.forward * published->offsetXYZ[1] + basis.up * published->offsetXYZ[2];
                        const double radius = std::hypot(static_cast<double>(published->offsetXYZ[0]),
                            static_cast<double>(published->offsetXYZ[1]), static_cast<double>(published->offsetXYZ[2]))
                            + std::hypot(published->width * 0.5, published->height * 0.5);
                        // The original billboard's camera-space half extents
                        // become float world vertices. Compare that analytical
                        // card, rather than double lengths of rounded vertices.
                        const QVector3D rightHalf = basis.right * (published->width * 0.5f);
                        const QVector3D upHalf = basis.up * (published->height * 0.5f);
                        const std::array<QVector3D, 4> expectedCorners{
                            center - rightHalf - upHalf, center + rightHalf - upHalf,
                            center + rightHalf + upHalf, center - rightHalf + upHalf };
                        const auto bounds = view->thingVisualBoundsOverlayAabbForTest(0u, false);
                        const bool textured = hasSpritePixels(frame);
                        const bool dimensions = corners && *corners == expectedCorners;
                        const bool centered = corners && closeQVec(spriteCenter(*corners), center);
                        const double endpoint = static_cast<double>(static_cast<float>(radius));
                        const bool bounded = bounds
                            && (*bounds)[0] == -endpoint && (*bounds)[1] == -endpoint && (*bounds)[2] == -endpoint
                            && (*bounds)[3] == endpoint && (*bounds)[4] == endpoint && (*bounds)[5] == endpoint;
                        const bool framed = corners && cornersInsideFrame(*view, *corners) && boundsInsideFrame(*view);
                        texturedUpdates += textured ? 1 : 0;
                        dimensionMatches += dimensions ? 1 : 0;
                        centerMatches += centered ? 1 : 0;
                        boundsMatches += bounded ? 1 : 0;
                        framedUpdates += framed ? 1 : 0;
                        if ( textured && dimensions && centered && bounded && framed )
                        {
                            ++visibleUpdates;
                        }
                    }
                }
            }
            controlSamples[static_cast<std::size_t>(step - 1)] = control->value();
            const auto published = previewSprite(*view);
            previewSamples[static_cast<std::size_t>(step - 1)] = published
                ? numericValue(*published, field) : std::numeric_limits<double>::quiet_NaN();
        }
        if ( visibleUpdates < 3 )
        {
            std::cerr << "SPR live project=" << static_cast<int>(project) << " type=" << type
                << " field=" << names[field] << " valid/published=" << visibleUpdates << '/' << publishedUpdates
                << " predicates(texture/dim/center/bounds/frame)=" << texturedUpdates << '/' << dimensionMatches
                << '/' << centerMatches << '/' << boundsMatches << '/' << framedUpdates
                << " gestures(start/finish/finishStep)=" << startedGestures << '/' << finishedGestures << '/' << finishedAtStep
                << " physicalCursor=" << physicalStart.x() << ',' << physicalStart.y()
                << " pressGlobal=" << startGlobal.x() << ',' << startGlobal.y() << " samples(control/preview)=";
            for ( const std::size_t sample : { 0u, 1u, 2u, 9u, 19u, 29u, 39u } )
            {
                std::cerr << controlSamples[sample] << '/' << previewSamples[sample] << ',';
            }
            std::cerr << '\n';
        }
        check(visibleUpdates >= 3,
            "A held Sprite numeric drag publishes several live preview values before release");
        const float committed = static_cast<float>(control->value());
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(last), QPointF(lastGlobal),
            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(line, &release);
        QObject::disconnect(started);
        QObject::disconnect(finished);
        check(waitForSprite([view, field, committed, numericValue]()
            {
                const auto published = previewSprite(*view);
                return published && numericValue(*published, field) == committed;
            }), "Sprite scrub release publishes the exact final authored float");
        const auto after = view->recoveryState().preview.orbit;
        check(after.yawDeg == before.yawDeg && after.pitchDeg == before.pitchDeg
                && after.rollDeg == before.rollDeg && after.center == before.center && after.pan == before.pan,
            "Live Sprite width, height and XYZ scrubs preserve the user's orbit and pan");
        const QImage frame = view->grabFramebuffer();
        const auto corners = view->thingSpriteCornersForTest(0u);
        check(corners && cornersInsideFrame(*view, *corners) && boundsInsideFrame(*view) && hasSpritePixels(frame),
            "Live Sprite numeric scrubs leave the textured card and default Bounds visibly framed");
        check(view->meshRebuildCountForTest() == rebuilds,
            "Numeric-only Sprite scrubs update their isolated geometry without rebuilding the level mesh cache");
    }
}

void checkFiniteExtremeSpriteOutcome(
    QWidget& pane, GlView& view, const GlView::RecoveryState& previous,
    const std::shared_ptr<const assets::ResourceLocator>& resources,
    const ProjectType project, const float dimension, const char* stage)
{
    const QImage frame = view.grabFramebuffer();
    const bool unavailable = view.property("spr.preview.fit_unavailable").toBool();
    auto* const status = pane.findChild<QLabel*>(QStringLiteral("resource_authoring.status"));
    const auto corners = view.thingSpriteCornersForTest(0u);
    const bool visible = corners && cornersInsideFrame(view, *corners)
        && boundsInsideFrame(view) && hasSpritePixels(frame);
    if ( unavailable )
    {
        check(status && status->text() == QStringLiteral("Sprite preview exceeds the camera's depth range.")
                && sameSpriteCamera(previous, view.recoveryState()),
            "Finite extreme Sprite failure reports its limit and retains the exact previous valid camera");
    }
    else
    {
        if ( !visible )
        {
            printSpriteFramingFailure(view, project, 0, dimension, stage);
        }
        check(visible, "Finite extreme Sprite successful fit keeps the textured card and Bounds visible");
    }
    const QCborMap native = ui::resources::captureResourceAuthoringPaneState(&pane);
    const QCborArray source = native.value(QStringLiteral("sprite")).toArray();
    check(static_cast<float>(source.at(2).toString().toDouble()) == dimension
            && static_cast<float>(source.at(3).toString().toDouble()) == dimension * 0.5f
            && static_cast<float>(source.at(12).toString().toDouble()) == dimension * 0.25f
            && static_cast<float>(source.at(13).toString().toDouble()) == dimension * 0.5f
            && static_cast<float>(source.at(14).toString().toDouble()) == dimension * 0.25f,
        "Finite extreme Sprite preview retains every exact authored float without clamping");
    QString warning{};
    const bool validNative = ui::resources::validateResourceAuthoringPaneState(native, &warning);
    if ( !validNative )
    {
        std::cerr << "SPR extreme native " << stage << " dimension=" << dimension
            << " warning=" << warning.toStdString() << '\n';
    }
    check(validNative, "Finite extreme Sprite outcome always captures a valid native camera presentation");
    auto restored = makeSpritePane(resources, project, 0, 1.0f, 1.0f, { 0.0f, 0.0f, 0.0f });
    const bool didRestore = restored
        && ui::resources::restoreResourceAuthoringPaneState(restored.get(), native, &warning);
    check(didRestore, "Finite extreme Sprite source and camera presentation restore through the native boundary");
    if ( !didRestore )
    {
        return;
    }
    restored->resize(pane.size());
    restored->show();
    auto* const restoredView = restored->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"));
    check(restoredView && waitForSprite([restoredView, dimension]()
        {
            const auto sprite = previewSprite(*restoredView);
            return restoredView->isValid() && sprite && sprite->width == dimension;
        }), "Finite extreme native restore publishes the exact resource to a valid real preview");
    const auto* const restoredStatus = restored->findChild<QLabel*>(QStringLiteral("resource_authoring.status"));
    check(restoredStatus && (unavailable
            ? restoredStatus->text() == QStringLiteral("Sprite preview exceeds the camera's depth range.")
            : restoredStatus->text() != QStringLiteral("Sprite preview exceeds the camera's depth range.")),
        "Native Sprite restoration reports only its final camera fit result without a stale depth warning");
    if ( restoredView && !unavailable )
    {
        check(waitForSprite([restoredView]() { return hasSpritePixels(restoredView->grabFramebuffer()); }),
            "An available finite extreme Sprite remains visibly textured after native camera restoration");
    }
}

void checkFiniteExtremeSpritePreview(
    const std::shared_ptr<const assets::ResourceLocator>& resources, const ProjectType project)
{
    auto pane = makeSpritePane(resources, project, 0, 1.0f, 1.0f, { 0.0f, 0.0f, 0.0f });
    if ( !pane )
    {
        return;
    }
    pane->resize(1120, 720);
    pane->show();
    auto* const view = pane->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"));
    auto* const recenter = pane->findChild<QPushButton*>(QStringLiteral("spr.preview.recenter"));
    auto* const bounds = pane->findChild<QCheckBox*>(QStringLiteral("spr.preview.bounds"));
    check(view && recenter && bounds, "Finite extreme Sprite regression has the production preview controls");
    if ( !view || !recenter || !bounds )
    {
        return;
    }
    check(waitForSprite([view]() { return view->isValid() && hasSpritePixels(view->grabFramebuffer()); }),
        "Finite extreme Sprite fixture renders its positive-control texture before editing");
    bounds->setChecked(true);
    auto initial = view->recoveryState();
    initial.preview.clipEnabled = true;
    for ( const float dimension : { 1.0e20f, std::numeric_limits<float>::max() } )
    {
        view->restoreRecoveryState(initial, false);
        const auto beforeEdit = view->recoveryState();
        commitSpriteNumber(*pane, "spr.width", dimension);
        commitSpriteNumber(*pane, "spr.height", dimension * 0.5f);
        commitSpriteNumber(*pane, "spr.offset.x", dimension * 0.25f);
        commitSpriteNumber(*pane, "spr.offset.y", dimension * 0.5f);
        commitSpriteNumber(*pane, "spr.offset.z", dimension * 0.25f);
        check(waitForSprite([view, dimension]()
            {
                const auto sprite = previewSprite(*view);
                return sprite && sprite->width == dimension && sprite->offsetXYZ[1] == dimension * 0.5f;
            }), "Finite extreme dimension and offsets publish without becoming non-finite");
        checkFiniteExtremeSpriteOutcome(*pane, *view, beforeEdit, resources, project, dimension, "extreme edit");
        const auto beforeRecenter = view->recoveryState();
        recenter->click();
        checkFiniteExtremeSpriteOutcome(*pane, *view, beforeRecenter, resources, project, dimension, "extreme recenter");
    }
}

void checkSpriteAnimationClockBoundaries()
{
    struct BoundaryVector
    {
        int fps{};
        double remainder{};
        std::int64_t uploadMs{};
        int delayMs{};
    };
    constexpr std::array<BoundaryVector, 17> boundaries{{
        { 0, 0.0, 0, 0 },
        { 12, 0.0, 0, 84 },
        { 12, 0.996, 0, 1 },
        { 12, 0.008, 0, 83 },
        { 20, 0.96, 0, 2 },
        { 20, 0.98, 0, 1 },
        { 20, 0.9999999999999999, 0, 1 },
        { 20, 0.0, 0, 50 },
        { 20, 0.1, 0, 45 },
        { 20, 0.98, 2, 1 },
        { 20, 0.1, 44, 1 },
        { 20, 0.62, 0, 19 },
        { 99, 0.0, 0, 11 },
        { 99, 0.95, 0, 1 },
        { 99, 0.395, 0, 7 },
        { 99, 0.395, 8, 1 },
        { 99, 0.395, std::numeric_limits<std::int64_t>::max(), 1 },
    }};
    for ( const BoundaryVector& vector : boundaries )
    {
        const int delay = ui::resources::detail::spriteAnimationNextDelayMs(
            vector.fps, vector.remainder, vector.uploadMs);
        if ( delay != vector.delayMs )
        {
            std::cerr << "SPR next cel deadline fps=" << vector.fps << " remainder=" << vector.remainder
                << " uploadMs=" << vector.uploadMs << " expectedMs=" << vector.delayMs
                << " actualMs=" << delay << '\n';
        }
        check(delay == vector.delayMs,
            "Sprite animation timer schedules the actual next cel boundary, including early callbacks, upload time and carried phase");
    }

    struct StepVector
    {
        std::int64_t deltaMs{};
        int fps{};
        double remainder{};
        int celCount{};
        int exactFrameUnits{};
        int wholeFrames{};
        int remainderUnits{};
        int advance{};
    };
    constexpr std::array<StepVector, 11> steps{{
        { 0, 20, 0.0, 2, 0, 0, 0, 0 },
        { 48, 20, 0.0, 2, 960, 0, 960, 0 },
        { 49, 20, 0.0, 2, 980, 0, 980, 0 },
        { 50, 20, 0.0, 2, 1000, 1, 0, 1 },
        { 105, 20, 0.0, 2, 2100, 2, 100, 0 },
        { 20, 20, 0.62, 2, 1020, 1, 20, 1 },
        { 0, 20, 0.62, 2, 620, 0, 620, 0 },
        { 83, 12, 0.0, 2, 996, 0, 996, 0 },
        { 84, 12, 0.0, 2, 1008, 1, 8, 1 },
        { 105, 99, 0.0, 3, 10395, 10, 395, 1 },
        { 1005, 20, 0.0, 3, 20100, 20, 100, 2 },
    }};
    for ( const StepVector& vector : steps )
    {
        const auto step = ui::resources::detail::spriteAnimationClockStep(
            vector.deltaMs, vector.fps, vector.remainder, vector.celCount);
        // Integral milliseconds and FPS give exact thousandth-frame units;
        // compare that quantum without depending on binary decimal rounding.
        check(std::lround(step.exactFrames * 1000.0) == vector.exactFrameUnits
                && step.wholeFrames == vector.wholeFrames
                && std::lround(step.remainder * 1000.0) == vector.remainderUnits
                && step.advance == vector.advance,
            "Sprite animation clock produces fixed exact-frame, whole-frame, remainder and cel-advance vectors");
    }
    const auto emptyStep = [](const ui::resources::detail::SpriteAnimationClockStep& step)
    {
        return step.exactFrames == 0.0 && step.wholeFrames == 0.0 && step.remainder == 0.0 && step.advance == 0;
    };
    for ( const int invalidFps : { -1, 0, 100 } )
    {
        check(emptyStep(ui::resources::detail::spriteAnimationClockStep(50, invalidFps, 0.0, 2))
                && ui::resources::detail::spriteAnimationNextDelayMs(invalidFps, 0.0, 0) == 0,
            "Disabled or invalid Sprite FPS cannot produce an unsafe clock step or timer interval");
    }
    for ( const double invalidRemainder : { -0.1, 1.0,
              std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() } )
    {
        check(emptyStep(ui::resources::detail::spriteAnimationClockStep(50, 20, invalidRemainder, 2))
                && ui::resources::detail::spriteAnimationNextDelayMs(20, invalidRemainder, 0) == 0,
            "Invalid Sprite fractional-frame state is rejected before any floating-to-integer conversion");
    }
    check(emptyStep(ui::resources::detail::spriteAnimationClockStep(-1, 20, 0.0, 2))
            && emptyStep(ui::resources::detail::spriteAnimationClockStep(50, 20, 0.0, 0))
            && emptyStep(ui::resources::detail::spriteAnimationClockStep(50, 20, 0.0, -1))
            && ui::resources::detail::spriteAnimationNextDelayMs(20, 0.0, -1) == 0,
        "Negative Sprite elapsed time and invalid cel counts are rejected without undefined arithmetic");

    const auto early = ui::resources::detail::spriteAnimationClockStep(49, 20, 0.0, 2);
    const auto late = ui::resources::detail::spriteAnimationClockStep(57, 20, early.remainder, 2);
    const auto repeatedEarly = ui::resources::detail::spriteAnimationClockStep(43, 20, late.remainder, 2);
    const auto repeatedLate = ui::resources::detail::spriteAnimationClockStep(57, 20, repeatedEarly.remainder, 2);
    check(early.wholeFrames == 0.0 && late.wholeFrames == 2.0
            && repeatedEarly.wholeFrames == 0.0 && repeatedLate.wholeFrames == 2.0
            && early.advance == 0 && late.advance == 0 && repeatedEarly.advance == 0 && repeatedLate.advance == 0,
        "Fixed-period early and late callbacks can repeatedly skip both cels despite an active Sprite animation clock");

    for ( const int celCount : { 2, 3, 256 } )
    {
        const auto catchup = ui::resources::detail::spriteAnimationClockStep(1005, 20, 0.0, celCount);
        const auto carry = ui::resources::detail::spriteAnimationClockStep(20, 20, 0.62, celCount);
        const auto restarted = ui::resources::detail::spriteAnimationClockStep(0, 20, 0.62, celCount);
        check(catchup.wholeFrames == 20.0 && catchup.advance == 20 % celCount
                && std::lround(catchup.remainder * 1000.0) == 100
                && carry.wholeFrames == 1.0 && carry.advance == 1
                && std::lround(carry.remainder * 1000.0) == 20
                && restarted.wholeFrames == 0.0 && restarted.advance == 0
                && std::lround(restarted.remainder * 1000.0) == 620,
            "Sprite animation retains complete elapsed catch-up and partial-frame phase across a clock restart");
    }
}

void checkArchiveAnimationSurvivesNumericEdits(
    const std::shared_ptr<const assets::ResourceLocator>& resources, const ProjectType project)
{
    auto pane = makeSpritePane(resources, project, project == ProjectType::IJIM ? 0 : 1,
        1.0f, 1.0f, { 0.0f, 0.0f, 0.0f });
    if ( !pane )
    {
        return;
    }
    pane->resize(1120, 720);
    auto* const view = pane->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"));
    auto* const fps = pane->findChild<QSlider*>(QStringLiteral("spr.preview.fps"));
    auto* const pause = pane->findChild<QToolButton*>(QStringLiteral("spr.preview.pause"));
    check(view && fps && pause, "Archived animated Sprite fixture has the production playback controls");
    if ( !view || !fps || !pause )
    {
        return;
    }
    fps->setValue(0);
    pane->show();
    check(waitForSprite([view]() { return view->isValid() && hasSpritePixels(view->grabFramebuffer()); }),
        "Synthetic archived MAT renders its first red cel before playback");
    fps->setValue(20);
    check(waitForSprite([view, pause]()
        {
            if ( !hasBlueSpritePixels(view->grabFramebuffer()) )
            {
                return false;
            }
            pause->setChecked(true);
            return true;
        }), "Synthetic archived MAT advances to and pauses its distinct blue cel");
    const std::uint64_t rebuilds = view->meshRebuildCountForTest();
    for ( const double width : { 2.0, 3.0, 4.0 } )
    {
        commitSpriteNumber(*pane, "spr.width", width);
        check(waitForSprite([view, width]()
            {
                const auto sprite = previewSprite(*view);
                return sprite && sprite->width == width;
            }), "Archived Sprite numeric edit publishes the exact updated resource");
        check(pause->isChecked() && hasBlueSpritePixels(view->grabFramebuffer())
                && !hasSpritePixels(view->grabFramebuffer()),
            "Numeric Sprite edits retain the paused current archive MAT cel without decoding back to cel zero");
        check(view->meshRebuildCountForTest() == rebuilds,
            "Draft SPR revisions do not invalidate unrelated archived MAT/CMP dependencies or rebuild their meshes");
    }
    struct AnimationSample
    {
        qint64 elapsedMs{};
        float publishedWidth{};
        QColor centerPixel{};
        bool red{};
        bool blue{};
        bool framed{};
    };
    std::array<AnimationSample, 40> samples{};
    QElapsedTimer playbackElapsed{};
    playbackElapsed.start();
    pause->setChecked(false);
    bool renderedRed = false;
    bool renderedBlue = false;
    int exactLivePublications = 0;
    for ( int step = 0; step < 40; ++step )
    {
        commitSpriteNumber(*pane, "spr.width", 5.0 + step * 0.025);
        QElapsedTimer interval{};
        interval.start();
        while ( interval.elapsed() < 10 )
        {
            QApplication::processEvents(QEventLoop::AllEvents);
        }
        const QImage frame = view->grabFramebuffer();
        AnimationSample& sample = samples[static_cast<std::size_t>(step)];
        sample.elapsedMs = playbackElapsed.elapsed();
        // Preserve the original short-circuit pixel checks; inspecting every
        // absent color over the full image would change the sampling cadence.
        renderedRed = renderedRed || hasSpritePixels(frame);
        renderedBlue = renderedBlue || hasBlueSpritePixels(frame);
        sample.red = renderedRed;
        sample.blue = renderedBlue;
        if ( const auto sprite = previewSprite(*view) )
        {
            sample.publishedWidth = sprite->width;
            if ( !pause->isChecked() && sprite->width == static_cast<float>(5.0 + step * 0.025) )
            {
                ++exactLivePublications;
            }
        }
        if ( const auto corners = view->thingSpriteCornersForTest(0u) )
        {
            sample.centerPixel = sampleSpriteWorldPixel(*view, frame, spriteCenter(*corners));
            sample.framed = cornersInsideFrame(*view, *corners) && boundsInsideFrame(*view);
        }
    }
    constexpr float finalWidth = 5.975f;
    const bool finalWidthPublished = waitForSprite([view, pause]()
        {
            const auto sprite = previewSprite(*view);
            return sprite && sprite->width == finalWidth && !pause->isChecked();
        });
    check(exactLivePublications > 0 && finalWidthPublished && view->meshRebuildCountForTest() == rebuilds,
        "Archived Sprite playback publishes live exact numeric widths and its final width without pausing or rebuilding meshes");
    if ( !renderedRed || !renderedBlue || pause->isChecked()
        || exactLivePublications == 0 || !finalWidthPublished || view->meshRebuildCountForTest() != rebuilds )
    {
        const auto* const width = pane->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width"));
        const auto* const status = pane->findChild<QLabel*>(QStringLiteral("resource_authoring.status"));
        std::cerr << "SPR archive animation project=" << static_cast<int>(project)
            << " red/blue=" << renderedRed << '/' << renderedBlue
            << " fps=" << fps->value() << " paused=" << pause->isChecked()
            << " visible=" << pane->isVisible() << '/' << view->isVisible()
            << " elapsedMs=" << playbackElapsed.elapsed()
            << " meshRebuilds=" << rebuilds << '/' << view->meshRebuildCountForTest()
            << " exactLivePublications=" << exactLivePublications << " finalPublished=" << finalWidthPublished
            << " committedWidth=" << (width ? width->value() : -1.0)
            << " status=" << (status ? status->text().toStdString() : std::string{})
            << " samples(ms:seenColors:publishedWidth:centerRGB:framed)=";
        for ( const AnimationSample& sample : samples )
        {
            std::cerr << sample.elapsedMs << ':' << (sample.red ? 'R' : '-') << (sample.blue ? 'B' : '-')
                << ':' << sample.publishedWidth << ':' << sample.centerPixel.red() << ','
                << sample.centerPixel.green() << ',' << sample.centerPixel.blue() << ':' << sample.framed << ';';
        }
        std::cerr << '\n';
        printSpriteFramingFailure(*view, project, project == ProjectType::IJIM ? 0 : 1,
            5.0 + 39.0 * 0.025, "archive animation");
    }
    check(renderedRed && renderedBlue && !pause->isChecked(),
        "Archived Sprite playback advances through both cels while numeric preview updates remain continuous");
}

} // namespace

void runResourceAuthoringSpriteGeometryTests()
{
    checkSpriteAnimationClockBoundaries();
    QTemporaryDir directory{};
    check(directory.isValid(), "Sprite geometry has an isolated synthetic fixture directory");
    if ( !directory.isValid() )
    {
        return;
    }
    const std::filesystem::path root(directory.path().toStdWString());
    check(writeSpriteFixture(root / "sprite_geometry.mat", fixtures::makeIndexedMatFixture(6u))
            && writeSpriteFixture(root / "sprite_geometry.cmp",
                fixtures::makeClassicCmpFixture(6u, QColor(232, 24, 48))),
        "Sprite geometry uses synthetic project-owned MAT and CMP bytes");
    check(writeSpriteFixture(root / "sprite_modes.mat", spriteRenderModesMaterial()),
        "Sprite render-mode regressions use a synthetic patterned MAT with an independently colored Solid cel");
    auto resources = std::make_shared<assets::ResourceLocator>();
    resources->addRoot(root);
    const std::filesystem::path archivePath = root / "sprite_geometry.gob";
    check(writeSpriteFixture(archivePath, spriteMaterialArchive()),
        "Archived Sprite cache regression writes only synthetic project-owned GOB/MAT/CMP bytes");
    auto archivedResources = std::make_shared<assets::ResourceLocator>();
    archivedResources->mountGob(archivePath);
    for ( const ProjectType project : { ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM } )
    {
        checkSpriteParameterHelpAndTextureModes(resources, project);
        checkSpriteRenderModes(resources, project, project == ProjectType::IJIM ? 0 : 1);
        if ( project == ProjectType::IJIM )
        {
            checkIjimSpriteGlobalPreviewGeometry(resources, 0);
            checkIjimSpriteGlobalPreviewGeometry(resources, 2);
            checkSpriteRenderModes(resources, project, 2);
        }
        checkArchiveAnimationSurvivesNumericEdits(archivedResources, project);
        checkLiveSpriteNumericScrubs(resources, project, project == ProjectType::IJIM ? 0 : 1);
        checkFiniteExtremeSpritePreview(resources, project);
        checkUnknownSpriteTypeIsPreservedWithoutGeometry(resources, project, 17);
        checkUnknownSpriteTypeIsPreservedWithoutGeometry(resources, project, -1);
        checkUnavailableDefaultDepthIsExplicit(resources, project);
        for ( const int type : { 0, 1, 2 } )
        {
            checkProfileGeometry(resources, project, type);
            if ( project != ProjectType::IJIM || type != 1 )
            {
                for ( const bool clipEnabled : { true, false } )
                {
                    checkRepeatedGeometryEditsAndRecenter(resources, project, type, true, clipEnabled);
                    checkRepeatedGeometryEditsAndRecenter(resources, project, type, false, clipEnabled);
                }
            }
        }
    }
}

} // namespace sed::app_tests
