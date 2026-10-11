#include "test_common.hpp"

#include "app_settings.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/assets/3do.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/sed.hpp"
#include "sed/level/build.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QImage>
#include <QMouseEvent>
#include <QSettings>
#include <QSurfaceFormat>
#include <QTimer>
#include <QToolButton>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sed::app_tests
{
namespace
{

TplValue makeStringValue(const std::string& name, const std::string& value)
{
    TplValue result{};
    result.name = name;
    result.type = ValueType::str;
    result.stringValue = value;
    return result;
}

TplValue makeIntegerValue(const std::string& name, const std::int32_t value)
{
    TplValue result{};
    result.name = name;
    result.type = ValueType::integer;
    result.intValue = value;
    return result;
}

MotsArchLightObject makeArchObject(const MotsArchLightVertex first)
{
    MotsArchLightMesh mesh{};
    mesh.wireIndex = 0;
    mesh.vertices = {
        first,
        MotsArchLightVertex{ 0.25f, 0.25f, 0.5f, 0.75f },
        MotsArchLightVertex{ 0.50f, 0.50f, 0.75f, 1.0f },
    };

    MotsArchLightObject object{};
    object.wireIndex = 0;
    object.meshes.push_back(std::move(mesh));
    return object;
}

editor::DocumentState makeRenderState()
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::MOTS;
    state.level.layers.emplace_back("ARCH render fixture");

    Thing thing{};
    thing.name = "arch_render_fixture";
    thing.templateName = thing.name;
    thing.layer = 0;
    thing.archLightingIndex = 0;
    thing.values.push_back(makeStringValue(
        "model3do", "arch_render_fixture.3do"));
    thing.values.push_back(makeIntegerValue("thingflags", 0x00000008));
    state.level.things.push_back(std::move(thing));
    state.level.motsArchLighting.push_back(makeArchObject(
        MotsArchLightVertex{ 0.125f, 0.25f, 0.5f, 0.875f }));
    return state;
}

std::shared_ptr<const assets::Model3do> makeRenderModel(
    const TLightMode lightMode = TLightMode::VertexDiffuseOnly)
{
    auto model = std::make_shared<assets::Model3do>();
    model->version = 2.1;

    assets::Model3doMesh mesh{};
    mesh.name = "arch_render_fixture";
    mesh.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    mesh.light = static_cast<std::int32_t>(lightMode);
    mesh.vertices = {
        // d6x6.3do serializes zero vertex intensities. Keep the fixture's
        // unbound model black so the ARCH differential exercises the same
        // ordinary Gouraud path as the original door.
        assets::Model3doVertex{ 0.0, 0.0, 0.0, ColorF{ 0.0f, 0.0f, 0.0f, 0.375f } },
        assets::Model3doVertex{ 1.0, 0.0, 0.0, ColorWhite },
        assets::Model3doVertex{ 0.0, 1.0, 0.0, ColorWhite },
    };

    assets::Model3doFace face{};
    face.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    face.light = static_cast<std::int32_t>(lightMode);
    // Retail JK/MotS 3DO face extra light is zero when the serialized field
    // is zero; Model3doFace's generic default is white for other callers.
    face.extraLight = ColorBlack;
    face.vertices = { 0, 1, 2 };
    mesh.faces.push_back(std::move(face));
    model->meshes.push_back(std::move(mesh));
    return model;
}

bool sameColor(const std::optional<ColorF>& actual, const ColorF& expected);

void runSyntheticPortalWireframeOcclusionTest()
{
    Level level{};
    level.kind = ProjectType::MOTS;
    level.layers = { "solid fill", "portal wire" };

    const auto makeSurface = [](const std::int32_t geo,
                                std::vector<std::int32_t> vertexIndices)
    {
        Surface surface{};
        surface.geo = geo;
        surface.faceFlags = FF_DoubleSided;
        surface.normal = TVector{ 0.0, 0.0, 1.0 };
        surface.vertices = std::move(vertexIndices);
        surface.texVertices.resize(surface.vertices.size());
        for (TexVertex& texVertex : surface.texVertices)
        {
            texVertex.color = ColorWhite;
        }
        return surface;
    };

    // Synthetic geometry: a finite NotDrawn portal edge crosses a broad
    // filled face in top-down projection, exercising support-plane depth
    // matching without loading any level or game asset.
    const std::vector<Vertex> portalFixtureVertices{
        Vertex{  5.0, 0.20,  0.00 },
        Vertex{  5.2, 0.20, -0.15 },
        Vertex{  5.0, 0.05,  0.00 },
        Vertex{ -2.0, 0.20, -0.15 },
        Vertex{ -1.8, 0.20,  0.00 },
        Vertex{ -1.8, 0.05,  0.00 },
        Vertex{ -1.2, 0.05,  0.00 },
        Vertex{ -1.4, 0.05,  0.00 },
    };
    Sector filledSector{};
    filledSector.layer = 0;
    filledSector.ambient = ColorWhite;
    filledSector.vertices = portalFixtureVertices;
    filledSector.surfaces.push_back(makeSurface(
        static_cast<std::int32_t>(TGeoMode::Solid), { 3, 4, 5, 2, 1, 6 }));
    filledSector.surfaces.push_back(makeSurface(
        static_cast<std::int32_t>(TGeoMode::Solid), { 0, 1, 2 }));
    filledSector.surfaces.push_back(makeSurface(
        static_cast<std::int32_t>(TGeoMode::Solid), { 6, 7, 3 }));

    Sector portalSector{};
    portalSector.layer = 1;
    portalSector.vertices = portalFixtureVertices;
    Surface portal = makeSurface(
        static_cast<std::int32_t>(TGeoMode::NotDrawn), { 6, 1, 0, 7 });
    portal.adjoin = SurfaceRef{ 0, 0 };
    portalSector.surfaces.push_back(std::move(portal));

    // This nearer top surface is a deliberate foreground-depth control. It
    // covers the midpoint of portal edge 0->7 and must remain an occluder.
    Sector foregroundSector{};
    foregroundSector.layer = 0;
    foregroundSector.ambient = ColorWhite;
    foregroundSector.vertices = {
        Vertex{ 1.60, 0.070, 0.200 },
        Vertex{ 2.00, 0.070, 0.200 },
        Vertex{ 2.00, 0.160, 0.200 },
        Vertex{ 1.60, 0.160, 0.200 },
    };
    foregroundSector.surfaces.push_back(makeSurface(
        static_cast<std::int32_t>(TGeoMode::Solid), { 0, 1, 2, 3 }));

    level.sectors.push_back(std::move(filledSector));
    level.sectors.push_back(std::move(portalSector));
    level.sectors.push_back(std::move(foregroundSector));

    GlView view{};
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.resize(320, 320);
    view.setBackgroundClearColor(Qt::black);
    view.setShowGrid(false);
    view.setShowWireframe(true);
    view.setShowVertices(false);
    view.setShowSolid(true);
    view.setShowTextures(false);
    view.setShowThings(false);
    view.setShowThingGizmos(false);
    view.setShowAxisWidget(false);
    view.setShowToastOverlays(false);
    view.setShowEditorCubes(false);
    view.setLightingMode(GlView::LightingMode::FullyLit);
    view.setLayerColors({ Qt::white, QColor(255, 0, 255), QColor(0, 255, 255) });
    view.setLayerWireframeColoringEnabled(true);
    view.setWireframeLineWidthPx(6.0f);
    view.setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view.setLevel(&level);
    view.frameWorldBounds(
        QVector3D{ -2.3f, -0.15f, -0.30f },
        QVector3D{  5.5f,  0.35f,  0.35f },
        false);
    view.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
    view.show();
    QApplication::processEvents(QEventLoop::AllEvents);

    const auto captureFrame = [&view]()
    {
        (void)view.grabFramebuffer();
        view.repaint();
        QApplication::processEvents(QEventLoop::AllEvents);
        return view.grabFramebuffer();
    };

    const auto pointFromVertex = [](const Vertex& vertex)
    {
        return QVector3D{
            static_cast<float>(vertex.x),
            static_cast<float>(vertex.y),
            static_cast<float>(vertex.z),
        };
    };
    const QVector3D portalEdgeStart = pointFromVertex(portalFixtureVertices[0]);
    const QVector3D portalEdgeEnd = pointFromVertex(portalFixtureVertices[7]);
    QVector3D filledFaceCenter{};
    for (const std::size_t index : { 3u, 4u, 5u, 2u, 1u, 6u })
    {
        filledFaceCenter += pointFromVertex(portalFixtureVertices[index]);
    }
    filledFaceCenter /= 6.0f;

    const auto portalProbePixel = [&view, &portalEdgeStart, &portalEdgeEnd,
                                   &filledFaceCenter](
        const QImage& frame,
        const float edgeParameter,
        const double inwardPixels) -> std::optional<QPointF>
    {
        if (frame.isNull() || view.width() <= 0 || view.height() <= 0)
        {
            return std::nullopt;
        }
        const std::optional<QPointF> start =
            view.projectWorldToViewport(portalEdgeStart);
        const std::optional<QPointF> end =
            view.projectWorldToViewport(portalEdgeEnd);
        const std::optional<QPointF> interior =
            view.projectWorldToViewport(filledFaceCenter);
        if (!start || !end || !interior)
        {
            return std::nullopt;
        }

        const double scaleX = static_cast<double>(frame.width()) / view.width();
        const double scaleY = static_cast<double>(frame.height()) / view.height();
        const double ax = start->x() * scaleX;
        const double ay = start->y() * scaleY;
        const double bx = end->x() * scaleX;
        const double by = end->y() * scaleY;
        const double dx = bx - ax;
        const double dy = by - ay;
        const double length = std::hypot(dx, dy);
        if (!(length > 1.0e-6))
        {
            return std::nullopt;
        }

        const double midpointX = 0.5 * (ax + bx);
        const double midpointY = 0.5 * (ay + by);
        const double interiorX = interior->x() * scaleX;
        const double interiorY = interior->y() * scaleY;
        const double sideCross =
            dx * (interiorY - midpointY) - dy * (interiorX - midpointX);
        if (std::abs(sideCross) <= 1.0e-8)
        {
            return std::nullopt;
        }
        const double inwardSide = sideCross > 0.0 ? 1.0 : -1.0;
        const double parameter = std::clamp(
            static_cast<double>(edgeParameter), 0.0, 1.0);
        const double normalX = -dy / length;
        const double normalY = dx / length;
        return QPointF{
            (ax + dx * parameter)
                + inwardSide * normalX * inwardPixels,
            (ay + dy * parameter)
                + inwardSide * normalY * inwardPixels,
        };
    };

    const auto containsPortalColorAtPixel = [](const QImage& frame,
                                                const QPointF& pixelPoint)
    {
        const int centerX = std::clamp(
            static_cast<int>(std::lround(pixelPoint.x())),
            0,
            frame.width() - 1);
        const int centerY = std::clamp(
            static_cast<int>(std::lround(pixelPoint.y())),
            0,
            frame.height() - 1);
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int x = centerX + dx;
                const int y = centerY + dy;
                if (x < 0 || x >= frame.width()
                    || y < 0 || y >= frame.height())
                {
                    continue;
                }
                const QColor pixel = frame.pixelColor(x, y);
                if (pixel.red() > 160
                    && pixel.blue() > 160
                    && pixel.green() < 100)
                {
                    return true;
                }
            }
        }
        return false;
    };

    constexpr std::array<float, 6> kPortalSpanProbeParameters{
        0.10f, 0.25f, 0.40f, 0.60f, 0.75f, 0.90f,
    };
    const auto portalSampleVisible = [&portalProbePixel,
                                      &containsPortalColorAtPixel](
        const QImage& frame,
        const float edgeParameter)
    {
        const std::optional<QPointF> probe =
            portalProbePixel(frame, edgeParameter, 2.0);
        return probe && containsPortalColorAtPixel(frame, *probe);
    };
    const auto portalSpanVisible = [&portalSampleVisible,
                                    &kPortalSpanProbeParameters](
        const QImage& frame)
    {
        return std::all_of(
            kPortalSpanProbeParameters.begin(),
            kPortalSpanProbeParameters.end(),
            [&portalSampleVisible, &frame](const float edgeParameter)
            {
                return portalSampleVisible(frame, edgeParameter);
            });
    };

    const QImage exactTopDown = captureFrame();
    const bool finitePortalSpanMatchesVisibleFill =
        portalSpanVisible(exactTopDown);
    const bool nearerForegroundStillOccludesPortal =
        !portalSampleVisible(exactTopDown, 0.50f);

    // One degree off the exact top-down view makes the portal's own supporting
    // plane project with finite area. This is the angle-sensitive control.
    view.setOrbitViewImmediate(0.0f, 89.0f, 0.0f);
    const QImage oneDegreeTilt = captureFrame();
    const bool oneDegreeControlVisible = portalSpanVisible(oneDegreeTilt);

    check(finitePortalSpanMatchesVisibleFill,
        "synthetic portal support matching keeps a finite span visible at exact top-down orthographic view");
    check(nearerForegroundStillOccludesPortal,
        "synthetic portal wire remains occluded by a nearer opaque surface");
    check(oneDegreeControlVisible,
        "synthetic portal support matching also works one degree off top-down");
    view.hide();
}

void runOpaquePolygonBoundaryWireframeContinuityRegressionTest()
{
    constexpr std::array projectCases{
        std::pair{ ProjectType::JKDF2, "JKDF2" },
        std::pair{ ProjectType::MOTS, "MOTS" },
        std::pair{ ProjectType::IJIM, "IJIM" },
    };

    for (const auto& [projectKind, projectName] : projectCases)
    {
        Level level{};
        level.kind = projectKind;
        level.layers = { "opaque foreground", "grazing solid face", "rear wire" };

        const auto makeSurface = [](const std::int32_t geo,
                                    std::vector<std::int32_t> vertices)
        {
            Surface surface{};
            surface.geo = geo;
            surface.faceFlags = FF_DoubleSided;
            surface.normal = TVector{ 0.0, 0.0, 1.0 };
            surface.vertices = std::move(vertices);
            surface.texVertices.resize(surface.vertices.size());
            return surface;
        };

        Sector foreground{};
        foreground.layer = 0;
        foreground.ambient = ColorWhite;
        foreground.vertices = {
            Vertex{ -1.5, -0.55, 0.0 },
            Vertex{  1.5, -0.55, 0.0 },
            Vertex{  1.5,  0.55, 0.0 },
            Vertex{ -1.5,  0.55, 0.0 },
        };
        foreground.surfaces.push_back(makeSurface(
            static_cast<std::int32_t>(TGeoMode::Solid), { 0, 1, 2, 3 }));

        // This real boundary sits just behind the foreground plane, while its
        // opaque face rises sharply in front. The inward stroke therefore
        // depends on matching the exact filled polygon depth, not a broad bias.
        Sector grazingFace{};
        grazingFace.layer = 1;
        grazingFace.ambient = ColorWhite;
        grazingFace.vertices = {
            Vertex{ -1.2, 0.405, -0.002 },
            Vertex{  1.2, 0.405, -0.002 },
            Vertex{  1.2, 0.8, 8.0 },
            Vertex{ -1.2, 0.8, 8.0 },
        };
        Surface grazingSurface = makeSurface(
            static_cast<std::int32_t>(TGeoMode::Solid), { 0, 1, 2, 3 });
        grazingSurface.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
        grazingFace.surfaces.push_back(std::move(grazingSurface));

        // This rear edge projects inside the foreground fill and must stay hidden.
        Sector rearWire{};
        rearWire.layer = 2;
        rearWire.vertices = {
            Vertex{ -0.9, -0.30, -0.001 },
            Vertex{  0.9, -0.30, -0.001 },
            Vertex{  0.0, -0.52, -0.001 },
        };
        rearWire.surfaces.push_back(makeSurface(
            static_cast<std::int32_t>(TGeoMode::NotDrawn), { 0, 1, 2 }));

        level.sectors.push_back(std::move(foreground));
        level.sectors.push_back(std::move(grazingFace));
        level.sectors.push_back(std::move(rearWire));

        assets::ResourceLocator renderResources{};
        GlView view{};
        QSurfaceFormat depthFormat = view.format();
        depthFormat.setDepthBufferSize(24);
        view.setFormat(depthFormat);
        view.setResourceLocator(&renderResources, false);
        view.setAttribute(Qt::WA_DontShowOnScreen);
        view.resize(480, 360);
        view.setBackgroundClearColor(Qt::black);
        view.setShowGrid(false);
        view.setShowWireframe(true);
        view.setShowVertices(false);
        view.setShowSolid(true);
        view.setShowTextures(false);
        view.setShowThings(false);
        view.setShowThingGizmos(false);
        view.setShowAxisWidget(false);
        view.setShowToastOverlays(false);
        view.setShowEditorCubes(false);
        view.setLightingMode(GlView::LightingMode::FullyLit);
        view.setLayerColors({ Qt::white, QColor(255, 0, 255), QColor(0, 128, 255) });
        view.setLayerWireframeColoringEnabled(true);
        view.setWireframeLineWidthPx(6.0f);
        view.setCameraProjectionType(app::ViewportCamera::ProjectionType::Orthographic);
        view.setLevel(&level);
        view.setOrbitViewImmediate(0.0f, 89.0f, 0.0f);
        view.frameWorldBounds(
            QVector3D{ -1.8f, -0.8f, -0.5f },
            QVector3D{  1.8f,  1.0f,  8.5f },
            false);
        view.setCameraClipEnabled(true);
        view.setCameraNearFar(-10000.0f, 10000.0f);
        view.show();
        QApplication::processEvents(QEventLoop::AllEvents);
        (void)view.grabFramebuffer();
        view.repaint();
        QApplication::processEvents(QEventLoop::AllEvents);
        const QImage frame = view.grabFramebuffer();

        const auto projectPixel = [&view, &frame](const QVector3D& point)
            -> std::optional<QPointF>
        {
            if (frame.isNull() || view.width() <= 0 || view.height() <= 0)
            {
                return std::nullopt;
            }
            const auto viewportPoint = view.projectWorldToViewport(point);
            if (!viewportPoint)
            {
                return std::nullopt;
            }
            return QPointF{
                viewportPoint->x() * static_cast<double>(frame.width()) / view.width(),
                viewportPoint->y() * static_cast<double>(frame.height()) / view.height(),
            };
        };
        const auto containsMagenta = [](const QColor& color)
        {
            return color.red() > 160 && color.blue() > 160 && color.green() < 110;
        };
        const auto containsBlue = [](const QColor& color)
        {
            return color.red() < 110 && color.green() < 190 && color.blue() > 160;
        };
        const auto lineColorCoverage = [&frame, &projectPixel](
            const QVector3D& startWorld,
            const QVector3D& endWorld,
            const QVector3D& interiorWorld,
            const double inwardOffsetPixels,
            const auto& matches) -> double
        {
            const auto start = projectPixel(startWorld);
            const auto end = projectPixel(endWorld);
            const auto interior = projectPixel(interiorWorld);
            if (!start || !end || !interior || frame.isNull())
            {
                return 0.0;
            }
            const double dx = end->x() - start->x();
            const double dy = end->y() - start->y();
            const double length = std::hypot(dx, dy);
            if (!(length > 16.0))
            {
                return 0.0;
            }
            const double sideCross =
                dx * (interior->y() - 0.5 * (start->y() + end->y()))
                - dy * (interior->x() - 0.5 * (start->x() + end->x()));
            if (std::abs(sideCross) <= 1.0e-8)
            {
                return 0.0;
            }
            const double inwardSide = sideCross > 0.0 ? 1.0 : -1.0;
            const double inwardX = -dy / length * inwardSide;
            const double inwardY = dx / length * inwardSide;
            const int samples = std::max(32, static_cast<int>(std::ceil(length * 1.5)));
            int visibleSamples = 0;
            int totalSamples = 0;
            for (int sample = 0; sample <= samples; ++sample)
            {
                const double parameter = 0.08 + 0.84
                    * static_cast<double>(sample) / samples;
                const int x = static_cast<int>(std::lround(
                    start->x() + dx * parameter + inwardX * inwardOffsetPixels));
                const int y = static_cast<int>(std::lround(
                    start->y() + dy * parameter + inwardY * inwardOffsetPixels));
                if (x < 0 || x >= frame.width() || y < 0 || y >= frame.height())
                {
                    continue;
                }
                ++totalSamples;
                if (matches(frame.pixelColor(x, y)))
                {
                    ++visibleSamples;
                }
            }
            return totalSamples == 0
                ? 0.0
                : static_cast<double>(visibleSamples) / totalSamples;
        };

        const double grazingBoundaryCoverage = lineColorCoverage(
            QVector3D{ -1.2f, 0.405f, -0.002f },
            QVector3D{  1.2f, 0.405f, -0.002f },
            QVector3D{  0.0f, 0.6025f, 3.999f },
            2.0,
            containsMagenta);
        const double rearEdgeLeakCoverage = lineColorCoverage(
            QVector3D{ -0.9f, -0.30f, -0.001f },
            QVector3D{  0.9f, -0.30f, -0.001f },
            QVector3D{  0.0f, -0.52f, -0.001f },
            0.0,
            containsBlue);

        const std::string boundaryCheck = std::string{ projectName }
            + ": solid opaque polygon boundary remains continuous "
              "across its projected inward stroke";
        const std::string occlusionCheck = std::string{ projectName }
            + ": wireframe does not leak through a nearer opaque "
              "foreground polygon";
        check(grazingBoundaryCoverage >= 0.98, boundaryCheck.c_str());
        check(rearEdgeLeakCoverage == 0.0, occlusionCheck.c_str());
        if (grazingBoundaryCoverage < 0.98 || rearEdgeLeakCoverage != 0.0)
        {
            std::cerr << projectName
                      << " opaque polygon wireframe diagnostic: "
                      << "boundary coverage=" << grazingBoundaryCoverage
                      << ", rear-edge leak coverage=" << rearEdgeLeakCoverage << '\n';
        }
        view.hide();
        QApplication::processEvents(QEventLoop::AllEvents);
    }
}

void runPerspectiveWireframeSupportDepthFramebufferRegressionTest()
{
    Level level{};
    level.kind = ProjectType::MOTS;
    level.layers = { "grazing support", "opaque foreground" };

    constexpr float kCameraYawDeg = 0.0f;
    constexpr float kCameraPitchDeg = 0.0f;

    const auto makeSurface = [](std::vector<std::int32_t> vertexIndices)
    {
        Surface surface{};
        surface.geo = static_cast<std::int32_t>(TGeoMode::Solid);
        surface.faceFlags = FF_DoubleSided;
        surface.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
        surface.normal = TVector{ 0.0, -1.0, 0.0 };
        surface.vertices = std::move(vertexIndices);
        surface.texVertices.resize(surface.vertices.size());
        for (TexVertex& texVertex : surface.texVertices)
        {
            texVertex.color = ColorWhite;
        }
        return surface;
    };

    // The front boundary is viewed from only a few degrees above its narrow
    // supporting face. The projected face height is smaller than the line
    // stroke; a thick-line fragment inside the fill has a nearer depth than
    // the unexpanded edge centerline.
    Sector grazingSupport{};
    grazingSupport.layer = 0;
    grazingSupport.ambient = ColorWhite;
    grazingSupport.vertices = {
        Vertex{ -0.05, 0.10, 0.000 }, // target edge A
        Vertex{  0.05, 0.10, 0.000 }, // target edge B
        Vertex{  0.04, 0.09, 0.002 },
        Vertex{  0.00, 0.09, 0.002 }, // triangle reference for edge A->B
    };
    grazingSupport.surfaces.push_back(makeSurface({ 0, 1, 2, 3 }));

    level.sectors.push_back(std::move(grazingSupport));

    assets::ResourceLocator renderResources{};
    GlView view{};
    QSurfaceFormat depthFormat = view.format();
    depthFormat.setDepthBufferSize(24);
    view.setFormat(depthFormat);
    view.setResourceLocator(&renderResources, false);
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.setFocusPolicy(Qt::NoFocus);
    view.resize(320, 320);
    view.setBackgroundClearColor(Qt::black);
    view.setShowGrid(false);
    view.setShowWireframe(true);
    view.setShowVertices(false);
    view.setShowSolid(true);
    view.setShowTextures(false);
    view.setShowThings(false);
    view.setShowThingGizmos(false);
    view.setShowAxisWidget(false);
    view.setShowToastOverlays(false);
    view.setShowEditorCubes(false);
    view.setLightingMode(GlView::LightingMode::FullyLit);
    view.setLayerColors({ QColor(255, 0, 255), QColor(0, 255, 255) });
    view.setLayerWireframeColoringEnabled(true);
    view.setWireframeLineWidthPx(1.5f);
    view.setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Perspective);
    view.setCameraFovXDeg(45.0f);
    view.setCameraClipEnabled(true);
    view.setCameraNearFar(0.015625f, 5000.0f);
    view.setOrbitViewImmediate(kCameraYawDeg, kCameraPitchDeg, 0.0f);
    view.setLevel(&level);
    view.frameWorldBounds(
        QVector3D{ -0.055f, 0.075f, -0.01f },
        QVector3D{  0.055f, 0.105f,  0.01f },
        false);
    // Keep the scene close to the perspective camera, as in the reported
    // grazing-angle repro, while staying beyond the near clip plane.
    view.setCameraOrbitDistance(0.20f);

    // Trace the exact ray from the framed camera eye through the edge midpoint.
    // A camera-forward offset alone is insufficient when framing moves the eye
    // off that projected ray.
    constexpr float kOccluderTowardEye = 0.04f;
    constexpr float kOccluderHalfWidth = 0.006f;
    constexpr float kOccluderHalfHeight = 0.006f;
    const QVector3D edgeMidpoint{ 0.0f, 0.10f, 0.0f };
    const QVector3D towardEye =
        (view.cameraEye() - edgeMidpoint).normalized();
    const QVector3D cameraForward = view.cameraForward();
    const QVector3D foregroundCenter =
        edgeMidpoint + towardEye * kOccluderTowardEye;
    const QVector3D foregroundRight = view.cameraRight() * kOccluderHalfWidth;
    const QVector3D foregroundUp = view.cameraUp() * kOccluderHalfHeight;
    const auto toVertex = [](const QVector3D& point)
    {
        return Vertex{
            static_cast<double>(point.x()),
            static_cast<double>(point.y()),
            static_cast<double>(point.z()),
        };
    };
    const std::array<QVector3D, 4> foregroundWorldCorners{
        foregroundCenter - foregroundRight - foregroundUp,
        foregroundCenter + foregroundRight - foregroundUp,
        foregroundCenter + foregroundRight + foregroundUp,
        foregroundCenter - foregroundRight + foregroundUp,
    };
    Sector foreground{};
    foreground.layer = 1;
    foreground.ambient = ColorWhite;
    foreground.vertices.reserve(foregroundWorldCorners.size());
    for (const QVector3D& corner : foregroundWorldCorners)
    {
        foreground.vertices.push_back(toVertex(corner));
    }
    Surface foregroundSurface = makeSurface({ 0, 1, 2, 3 });
    foregroundSurface.normal = TVector{
        -static_cast<double>(cameraForward.x()),
        -static_cast<double>(cameraForward.y()),
        -static_cast<double>(cameraForward.z()),
    };
    foreground.surfaces.push_back(std::move(foregroundSurface));
    level.sectors.push_back(std::move(foreground));

    // The same level object now includes the occluder. Keep the already-framed
    // camera while publishing the extra opaque surface.
    view.setLevel(&level, GlView::SetLevelMode::keepView);
    view.show();
    QApplication::processEvents(QEventLoop::AllEvents);

    const auto captureFrame = [&view]()
    {
        (void)view.grabFramebuffer();
        view.repaint();
        QApplication::processEvents(QEventLoop::AllEvents);
        return view.grabFramebuffer();
    };
    const auto projectPixel = [&view](const QImage& frame,
                                      const QVector3D& worldPoint)
        -> std::optional<QPointF>
    {
        if (frame.isNull() || view.width() <= 0 || view.height() <= 0)
        {
            return std::nullopt;
        }
        const std::optional<QPointF> viewportPoint =
            view.projectWorldToViewport(worldPoint);
        if (!viewportPoint)
        {
            return std::nullopt;
        }
        return QPointF{
            viewportPoint->x() * static_cast<double>(frame.width()) / view.width(),
            viewportPoint->y() * static_cast<double>(frame.height()) / view.height(),
        };
    };
    const auto edgeProbe = [&projectPixel](const QImage& frame,
                                           const QVector3D& edgeA,
                                           const QVector3D& edgeB,
                                           const QVector3D& support,
                                           const double edgeParameter,
                                           const double inwardPixels)
        -> std::optional<QPointF>
    {
        const std::optional<QPointF> a = projectPixel(frame, edgeA);
        const std::optional<QPointF> b = projectPixel(frame, edgeB);
        const std::optional<QPointF> supportPoint = projectPixel(frame, support);
        if (!a || !b || !supportPoint)
        {
            return std::nullopt;
        }
        const double dx = b->x() - a->x();
        const double dy = b->y() - a->y();
        const double length = std::hypot(dx, dy);
        if (!(length > 1.0))
        {
            return std::nullopt;
        }
        const double middleX = 0.5 * (a->x() + b->x());
        const double middleY = 0.5 * (a->y() + b->y());
        const double sideCross =
            dx * (supportPoint->y() - middleY)
            - dy * (supportPoint->x() - middleX);
        if (std::abs(sideCross) <= 1.0e-8)
        {
            return std::nullopt;
        }
        const double side = sideCross > 0.0 ? 1.0 : -1.0;
        return QPointF{
            a->x() + dx * edgeParameter - side * dy / length * inwardPixels,
            a->y() + dy * edgeParameter + side * dx / length * inwardPixels,
        };
    };
    const auto hasColorNear = [](const QImage& frame,
                                 const QPointF& point,
                                 const auto& matches)
    {
        const int centerX = static_cast<int>(std::lround(point.x()));
        const int centerY = static_cast<int>(std::lround(point.y()));
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int x = centerX + dx;
                const int y = centerY + dy;
                if (x >= 0 && x < frame.width()
                    && y >= 0 && y < frame.height()
                    && matches(frame.pixelColor(x, y)))
                {
                    return true;
                }
            }
        }
        return false;
    };
    const auto containsMagenta = [](const QColor& pixel)
    {
        return pixel.red() > 160 && pixel.blue() > 160 && pixel.green() < 100;
    };

    const QVector3D edgeA{ -0.05f, 0.10f, 0.0f };
    const QVector3D edgeB{  0.05f, 0.10f, 0.0f };
    const QVector3D support{ 0.0f, 0.09f, 0.002f };
    const QImage wireframeFrame = captureFrame();
    const std::optional<QPointF> visibleFillProbe = edgeProbe(
        wireframeFrame, edgeA, edgeB, support, 0.40, 0.20);
    const std::optional<QPointF> foregroundProbe = edgeProbe(
        wireframeFrame, edgeA, edgeB, support, 0.50, 0.0);
    std::array<QPointF, 4> foregroundPolygon{};
    bool foregroundPolygonProjected = true;
    for (std::size_t cornerIndex = 0;
         cornerIndex < foregroundWorldCorners.size();
         ++cornerIndex)
    {
        const std::optional<QPointF> cornerPixel = projectPixel(
            wireframeFrame, foregroundWorldCorners[cornerIndex]);
        if (!cornerPixel)
        {
            foregroundPolygonProjected = false;
            break;
        }
        foregroundPolygon[cornerIndex] = *cornerPixel;
    }
    const auto pointInsideForeground = [](
        const QPointF& point,
        const std::array<QPointF, 4>& polygon)
    {
        double referenceSign = 0.0;
        bool haveReferenceSign = false;
        for (std::size_t index = 0; index < polygon.size(); ++index)
        {
            const QPointF& a = polygon[index];
            const QPointF& b = polygon[(index + 1) % polygon.size()];
            const double edgeX = b.x() - a.x();
            const double edgeY = b.y() - a.y();
            const double pointCross = edgeX * (point.y() - a.y())
                - edgeY * (point.x() - a.x());
            if (std::abs(pointCross) <= 1.0e-7)
            {
                continue;
            }
            if (!haveReferenceSign)
            {
                referenceSign = pointCross;
                haveReferenceSign = true;
            }
            else if ((pointCross < 0.0) != (referenceSign < 0.0))
            {
                return false;
            }
        }
        return haveReferenceSign;
    };
    const bool foregroundCoversProbe = foregroundProbe
        && foregroundPolygonProjected
        && pointInsideForeground(*foregroundProbe, foregroundPolygon);
    const auto sceneDepthAtFramebufferPoint = [&view, &wireframeFrame](
        const std::optional<QPointF>& pixelPoint) -> std::optional<float>
    {
        if (!pixelPoint || wireframeFrame.isNull()
            || wireframeFrame.width() <= 0 || wireframeFrame.height() <= 0)
        {
            return std::nullopt;
        }
        const QPointF widgetPoint{
            pixelPoint->x() * static_cast<double>(view.width()) / wireframeFrame.width(),
            pixelPoint->y() * static_cast<double>(view.height()) / wireframeFrame.height(),
        };
        return view.architecturalLightingSceneDepthForTest(widgetPoint);
    };
    const auto pixelColorAt = [&wireframeFrame](
        const std::optional<QPointF>& point) -> QColor
    {
        if (!point || wireframeFrame.isNull())
        {
            return {};
        }
        const int x = std::clamp(
            static_cast<int>(std::lround(point->x())),
            0,
            wireframeFrame.width() - 1);
        const int y = std::clamp(
            static_cast<int>(std::lround(point->y())),
            0,
            wireframeFrame.height() - 1);
        return wireframeFrame.pixelColor(x, y);
    };
    const std::optional<float> visibleSceneDepth =
        sceneDepthAtFramebufferPoint(visibleFillProbe);
    const std::optional<float> foregroundSceneDepth =
        sceneDepthAtFramebufferPoint(foregroundProbe);
    const bool hasForegroundOccluder = foregroundCoversProbe
        && visibleSceneDepth
        && foregroundSceneDepth
        && std::isfinite(*visibleSceneDepth)
        && std::isfinite(*foregroundSceneDepth)
        && *foregroundSceneDepth < *visibleSceneDepth
        && *foregroundSceneDepth < 1.0f - 1.0e-6f;
    const bool grazingEdgeVisible = visibleFillProbe
        && hasColorNear(wireframeFrame, *visibleFillProbe, containsMagenta);
    const bool foregroundHasMagenta = foregroundProbe
        && hasColorNear(wireframeFrame, *foregroundProbe, containsMagenta);
    const bool opaqueForegroundRejectsEdge = foregroundProbe
        && hasForegroundOccluder
        && !foregroundHasMagenta;

    if (!opaqueForegroundRejectsEdge)
    {
        const QColor visiblePixel = pixelColorAt(visibleFillProbe);
        const QColor foregroundPixel = pixelColorAt(foregroundProbe);
        std::cerr << "[perspective_wireframe diagnostic] widget="
                  << view.width() << 'x' << view.height()
                  << " framebuffer=" << wireframeFrame.width() << 'x'
                  << wireframeFrame.height()
                  << " visibleProbe=";
        if (visibleFillProbe)
        {
            std::cerr << '(' << visibleFillProbe->x() << ','
                      << visibleFillProbe->y() << ')';
        }
        else
        {
            std::cerr << "none";
        }
        std::cerr << " foregroundProbe=";
        if (foregroundProbe)
        {
            std::cerr << '(' << foregroundProbe->x() << ','
                      << foregroundProbe->y() << ')';
        }
        else
        {
            std::cerr << "none";
        }
        std::cerr << " depths=";
        if (visibleSceneDepth)
        {
            std::cerr << *visibleSceneDepth;
        }
        else
        {
            std::cerr << "none";
        }
        std::cerr << ',';
        if (foregroundSceneDepth)
        {
            std::cerr << *foregroundSceneDepth;
        }
        else
        {
            std::cerr << "none";
        }
        std::cerr << " cameraForward=(" << cameraForward.x() << ','
                  << cameraForward.y() << ',' << cameraForward.z() << ')'
                  << " foregroundCenter=(" << foregroundCenter.x() << ','
                  << foregroundCenter.y() << ',' << foregroundCenter.z() << ')'
                  << " foregroundCoversProbe=" << foregroundCoversProbe
                  << " hasForegroundOccluder=" << hasForegroundOccluder
                  << " visiblePixel=" << visiblePixel.red() << ','
                  << visiblePixel.green() << ',' << visiblePixel.blue()
                  << " foregroundPixel=" << foregroundPixel.red() << ','
                  << foregroundPixel.green() << ',' << foregroundPixel.blue()
                  << " magentaNearVisible="
                  << (visibleFillProbe
                      && hasColorNear(wireframeFrame, *visibleFillProbe, containsMagenta))
                  << " magentaNearForeground=" << foregroundHasMagenta
                  << '\n';
    }

    view.setShowWireframe(false);
    const QImage fillOnlyFrame = captureFrame();
    const bool visibleSampleComesFromWireframe = visibleFillProbe
        && !hasColorNear(fillOnlyFrame, *visibleFillProbe, containsMagenta);

    check(grazingEdgeVisible && visibleSampleComesFromWireframe,
        "perspective_wireframe renders the near-grazing support edge into its own solid fill");
    check(foregroundCoversProbe,
        "perspective_wireframe camera-plane quad covers the matched edge probe");
    check(hasForegroundOccluder,
        "perspective_wireframe foreground sample has a nearer opaque scene-depth surface");
    check(foregroundProbe && !foregroundHasMagenta,
        "perspective_wireframe does not draw the matched edge through the foreground sample");
    view.hide();
    QApplication::processEvents(QEventLoop::AllEvents);
}

void runWireframeSupportDepthOcclusionRegressionTest()
{
    Level level{};
    level.kind = ProjectType::MOTS;
    level.layers = {
        "opaque foreground",
        "rear wire",
        "opaque adjoin",
        "rear filled surface",
        "far scene geometry",
    };

    const auto makeSurface = [](const std::int32_t geo,
                                std::vector<std::int32_t> vertexIndices)
    {
        Surface surface{};
        surface.geo = geo;
        surface.faceFlags = FF_DoubleSided;
        surface.normal = TVector{ 0.0, 0.0, 1.0 };
        surface.vertices = std::move(vertexIndices);
        surface.texVertices.resize(surface.vertices.size());
        for (TexVertex& texVertex : surface.texVertices)
        {
            texVertex.color = ColorWhite;
        }
        return surface;
    };

    Sector foreground{};
    foreground.layer = 0;
    foreground.ambient = ColorWhite;
    foreground.vertices = {
        Vertex{ -1.5, -0.55, 0.0 },
        Vertex{  1.5, -0.55, 0.0 },
        Vertex{  1.5,  0.55, 0.0 },
        Vertex{ -1.5,  0.55, 0.0 },
    };
    Surface foregroundSurface = makeSurface(
        static_cast<std::int32_t>(TGeoMode::Solid), { 0, 1, 2, 3 });
    foregroundSurface.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    for (TexVertex& texVertex : foregroundSurface.texVertices)
    {
        texVertex.color = ColorF{ 0.25f, 0.25f, 0.25f, 1.0f };
    }
    foreground.surfaces.push_back(std::move(foregroundSurface));

    // The rear NotDrawn edge is less than one 24-bit depth-buffer step behind
    // the opaque fill when the saved broad orthographic range (±10,000) is in
    // use. Its third vertex defines a steep support plane, but that plane has
    // no filled pixels here. The other two boundary edges stay away from the
    // tested mid-span. This checks that depth precision does not turn a rear
    // wire into a visible speck under the front surface.
    Sector rearWire{};
    rearWire.layer = 1;
    rearWire.vertices = {
        Vertex{ -1.25, 0.0, -0.0002 },
        Vertex{  1.25, 0.0, -0.0002 },
        Vertex{  0.0, 0.4, 10.0 },
    };
    rearWire.surfaces.push_back(makeSurface(
        static_cast<std::int32_t>(TGeoMode::NotDrawn), { 0, 1, 2 }));

    // With textures off this filled adjoin is opaque (no explicit translucency
    // flags). Its base edge sits just behind the foreground fill while the face
    // rises steeply in front of it; matching the true fill depth is required
    // for the inward half of the thick edge to remain visible.
    Sector opaqueAdjoin{};
    opaqueAdjoin.layer = 2;
    opaqueAdjoin.ambient = ColorWhite;
    opaqueAdjoin.vertices = {
        Vertex{ -1.2, 0.405, -0.002 },
        Vertex{  1.2, 0.405, -0.002 },
        Vertex{  1.2, 0.8, 8.0 },
        Vertex{ -1.2, 0.8, 8.0 },
    };
    Surface opaqueAdjoinSurface = makeSurface(
        static_cast<std::int32_t>(TGeoMode::Texture), { 0, 1, 2, 3 });
    opaqueAdjoinSurface.light =
        static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    opaqueAdjoinSurface.material = "wireframe_alpha_adjoin_test.mat";
    opaqueAdjoinSurface.adjoin = SurfaceRef{ 0, 0 };
    for (TexVertex& texVertex : opaqueAdjoinSurface.texVertices)
    {
        texVertex.color = ColorF{ 0.25f, 0.25f, 0.25f, 1.0f };
    }
    opaqueAdjoin.surfaces.push_back(std::move(opaqueAdjoinSurface));

    // Far but visible geometry sits outside every nearer synthetic face. It
    // verifies that tightening uses the complete scene bounds rather than
    // clipping everything outside a near-camera envelope.
    Sector farGeometry{};
    farGeometry.layer = 4;
    farGeometry.ambient = ColorWhite;
    farGeometry.vertices = {
        Vertex{ -1.0, 1.20, -500.0 },
        Vertex{  1.0, 1.20, -500.0 },
        Vertex{  1.0, 1.28, -500.0 },
        Vertex{ -1.0, 1.28, -500.0 },
    };
    Surface farGeometrySurface = makeSurface(
        static_cast<std::int32_t>(TGeoMode::Solid), { 0, 1, 2, 3 });
    farGeometrySurface.light =
        static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    farGeometry.surfaces.push_back(std::move(farGeometrySurface));

    // A genuine opaque face behind the foreground must remain hidden. This
    // catches dotted wire leakage independently of the portal case above.
    Sector rearFilled{};
    rearFilled.layer = 3;
    rearFilled.ambient = ColorWhite;
    rearFilled.vertices = {
        Vertex{ -1.25, -0.30, -0.01 },
        Vertex{  1.25, -0.30, -0.01 },
        Vertex{  1.25,  0.30, -0.01 },
        Vertex{ -1.25,  0.30, -0.01 },
    };
    Surface rearFilledSurface = makeSurface(
        static_cast<std::int32_t>(TGeoMode::Solid), { 0, 1, 2, 3 });
    rearFilledSurface.light =
        static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    rearFilled.surfaces.push_back(std::move(rearFilledSurface));

    level.sectors.push_back(std::move(foreground));
    level.sectors.push_back(std::move(rearWire));
    level.sectors.push_back(std::move(opaqueAdjoin));
    level.sectors.push_back(std::move(rearFilled));
    level.sectors.push_back(std::move(farGeometry));

    assets::ResourceLocator renderResources{};
    GlView view{};
    QSurfaceFormat depthFormat = view.format();
    depthFormat.setDepthBufferSize(24);
    view.setFormat(depthFormat);
    view.setResourceLocator(&renderResources, false);
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.resize(320, 320);
    view.setBackgroundClearColor(Qt::black);
    view.setShowGrid(true);
    view.setGridAlwaysBehind(true);
    view.setShowWireframe(true);
    view.setShowVertices(false);
    view.setShowSolid(true);
    view.setShowTextures(false);
    view.setShowThings(false);
    view.setShowThingGizmos(false);
    view.setShowAxisWidget(false);
    view.setShowToastOverlays(false);
    view.setShowEditorCubes(false);
    view.setLightingMode(GlView::LightingMode::FullyLit);
    view.setLayerColors({
        Qt::white,
        QColor(255, 0, 255),
        QColor(0, 255, 255),
        QColor(255, 255, 0),
        QColor(0, 128, 255),
    });
    view.setLayerWireframeColoringEnabled(true);
    view.setWireframeLineWidthPx(6.0f);
    view.setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view.setLevel(&level);
    // Frame at the final top-down orientation. If framing uses the previous
    // oblique view, the distant z=-500 face needlessly inflates the screen
    // scale and can sit under the foreground face's thick wire stroke.
    view.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
    view.frameWorldBounds(
        QVector3D{ -1.8f, -0.75f, -0.5f },
        QVector3D{  1.8f,  1.35f, 10.5f },
        false);
    // Match the persisted camera state in the reported SED repros.
    view.setCameraClipEnabled(true);
    view.setCameraNearFar(-10000.0f, 10000.0f);
    view.show();
    QApplication::processEvents(QEventLoop::AllEvents);

    const auto captureFrame = [&view]()
    {
        (void)view.grabFramebuffer();
        view.repaint();
        QApplication::processEvents(QEventLoop::AllEvents);
        return view.grabFramebuffer();
    };
    const auto projectPixel = [&view](const QImage& frame,
                                      const QVector3D& worldPoint)
        -> std::optional<QPointF>
    {
        if (frame.isNull() || view.width() <= 0 || view.height() <= 0)
        {
            return std::nullopt;
        }
        const std::optional<QPointF> viewportPoint =
            view.projectWorldToViewport(worldPoint);
        if (!viewportPoint)
        {
            return std::nullopt;
        }
        return QPointF{
            viewportPoint->x() * static_cast<double>(frame.width()) / view.width(),
            viewportPoint->y() * static_cast<double>(frame.height()) / view.height(),
        };
    };
    const auto hasColorNear = [](const QImage& frame,
                                 const QPointF& point,
                                 const auto& matches)
    {
        const int centerX = static_cast<int>(std::lround(point.x()));
        const int centerY = static_cast<int>(std::lround(point.y()));
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int x = centerX + dx;
                const int y = centerY + dy;
                if (x >= 0 && x < frame.width()
                    && y >= 0 && y < frame.height()
                    && matches(frame.pixelColor(x, y)))
                {
                    return true;
                }
            }
        }
        return false;
    };
    const auto hasColorNearVisibleSceneDepth = [&view](
        const QImage& frame, const QPointF& point, const auto& matches)
    {
        const int centerX = static_cast<int>(std::lround(point.x()));
        const int centerY = static_cast<int>(std::lround(point.y()));
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int x = centerX + dx;
                const int y = centerY + dy;
                if (x < 0 || x >= frame.width() || y < 0 || y >= frame.height())
                {
                    continue;
                }
                if (!matches(frame.pixelColor(x, y)))
                {
                    continue;
                }
                const QPointF widgetPoint{
                    (static_cast<double>(x) + 0.5)
                        * static_cast<double>(view.width()) / frame.width(),
                    (static_cast<double>(y) + 0.5)
                        * static_cast<double>(view.height()) / frame.height(),
                };
                const std::optional<float> depth =
                    view.architecturalLightingSceneDepthForTest(widgetPoint);
                // A colored stroke sample only counts as a leak if an opaque
                // scene surface covers that exact framebuffer pixel. This
                // excludes valid line flanks outside the foreground silhouette.
                if (depth && std::isfinite(*depth) && *depth < 1.0f - 1.0e-6f)
                {
                    return true;
                }
            }
        }
        return false;
    };
    const auto containsMagenta = [](const QColor& pixel)
    {
        return pixel.red() > 160 && pixel.blue() > 160 && pixel.green() < 110;
    };
    const auto containsCyan = [](const QColor& pixel)
    {
        return pixel.red() < 110 && pixel.green() > 160 && pixel.blue() > 160;
    };
    const auto containsWhite = [](const QColor& pixel)
    {
        return pixel.red() > 220 && pixel.green() > 220 && pixel.blue() > 220;
    };
    const auto containsYellow = [](const QColor& pixel)
    {
        return pixel.red() > 160 && pixel.green() > 160 && pixel.blue() < 110;
    };
    const auto containsBlue = [](const QColor& pixel)
    {
        return pixel.red() < 110 && pixel.green() < 190 && pixel.blue() > 160;
    };

    const auto viewSpaceZ = [&view](const QVector3D& worldPoint)
    {
        return QVector3D::dotProduct(
            worldPoint - view.cameraEye(),
            -view.cameraForward());
    };
    const auto farGeometryEdgeSamplesProjectable =
        [&projectPixel](const QImage& frame)
    {
        constexpr std::array<float, 3> kEdgeParameters{ 0.20f, 0.50f, 0.80f };
        return std::all_of(
            kEdgeParameters.begin(), kEdgeParameters.end(),
            [&frame, &projectPixel](const float parameter)
            {
                const QVector3D worldPoint{ -1.0f + 2.0f * parameter, 1.20f, -500.0f };
                return projectPixel(frame, worldPoint).has_value();
            });
    };
    const auto farGeometryEdgeVisible =
        [&projectPixel, &hasColorNear, &containsBlue](const QImage& frame)
    {
        constexpr std::array<float, 3> kEdgeParameters{ 0.20f, 0.50f, 0.80f };
        return std::all_of(
            kEdgeParameters.begin(), kEdgeParameters.end(),
            [&frame, &projectPixel, &hasColorNear, &containsBlue](const float parameter)
            {
                const QVector3D worldPoint{ -1.0f + 2.0f * parameter, 1.20f, -500.0f };
                const std::optional<QPointF> pixel = projectPixel(frame, worldPoint);
                return pixel && hasColorNear(frame, *pixel, containsBlue);
            });
    };

    const QVector3D rearStart{ -1.25f, 0.0f, -0.0002f };
    const QVector3D rearEnd{ 1.25f, 0.0f, -0.0002f };
    const auto rearSpanHasMagenta = [&projectPixel,
                                     &hasColorNearVisibleSceneDepth,
                                     &containsMagenta, &rearStart, &rearEnd](
        const QImage& frame)
        -> std::optional<bool>
    {
        const std::optional<QPointF> start = projectPixel(frame, rearStart);
        const std::optional<QPointF> end = projectPixel(frame, rearEnd);
        if (!start || !end)
        {
            return std::nullopt;
        }
        const double dx = end->x() - start->x();
        const double dy = end->y() - start->y();
        const double length = std::hypot(dx, dy);
        if (!(length > 1.0))
        {
            return std::nullopt;
        }
        const double nx = -dy / length;
        const double ny = dx / length;
        constexpr std::array<double, 5> kInteriorParameters{
            0.25, 0.375, 0.50, 0.625, 0.75,
        };
        constexpr std::array<double, 3> kLineOffsetsPixels{
            -2.0, 0.0, 2.0,
        };
        for (const double parameter : kInteriorParameters)
        {
            for (const double offset : kLineOffsetsPixels)
            {
                const QPointF sample{
                    start->x() + dx * parameter + nx * offset,
                    start->y() + dy * parameter + ny * offset,
                };
                // Ignore stroke flanks outside the foreground silhouette;
                // only a magenta pixel with opaque scene depth is a leak.
                if (hasColorNearVisibleSceneDepth(
                        frame, sample, containsMagenta))
                {
                    return true;
                }
            }
        }
        return false;
    };
    const auto solidEdgeSpanVisible = [&projectPixel, &hasColorNear, &containsWhite](
        const QImage& frame)
    {
        constexpr std::array<float, 3> kEdgeParameters{ 0.20f, 0.50f, 0.80f };
        return std::all_of(
            kEdgeParameters.begin(), kEdgeParameters.end(),
            [&frame, &projectPixel, &hasColorNear, &containsWhite](const float parameter)
            {
                const QVector3D worldPoint{
                    -1.5f,
                    -0.55f + 1.10f * parameter,
                    0.0f,
                };
                const std::optional<QPointF> pixel = projectPixel(frame, worldPoint);
                return pixel && hasColorNear(frame, *pixel, containsWhite);
            });
    };
    const auto adjoinEdgeSpanVisibleWithColor =
        [&projectPixel, &hasColorNear](
            const QImage& frame, const auto& matches, const double inwardOffsetPixels)
    {
        const std::optional<QPointF> start = projectPixel(
            frame, QVector3D{ -1.2f, 0.405f, -0.002f });
        const std::optional<QPointF> end = projectPixel(
            frame, QVector3D{ 1.2f, 0.405f, -0.002f });
        const std::optional<QPointF> interior = projectPixel(
            frame, QVector3D{ 0.0f, 0.6025f, 3.999f });
        if (!start || !end || !interior)
        {
            return false;
        }
        const double dx = end->x() - start->x();
        const double dy = end->y() - start->y();
        const double length = std::hypot(dx, dy);
        if (!(length > 1.0))
        {
            return false;
        }
        const double midX = 0.5 * (start->x() + end->x());
        const double midY = 0.5 * (start->y() + end->y());
        const double sideCross =
            dx * (interior->y() - midY) - dy * (interior->x() - midX);
        if (std::abs(sideCross) <= 1.0e-8)
        {
            return false;
        }
        const double inwardSide = sideCross > 0.0 ? 1.0 : -1.0;
        const double nx = -dy / length;
        const double ny = dx / length;
        constexpr std::array<double, 3> kInteriorParameters{ 0.20, 0.50, 0.80 };
        return std::all_of(
            kInteriorParameters.begin(), kInteriorParameters.end(),
            [&frame, &start, dx, dy, nx, ny, inwardSide,
             inwardOffsetPixels, &hasColorNear, &matches](const double parameter)
            {
                const QPointF sample{
                    start->x() + dx * parameter + inwardSide * nx * inwardOffsetPixels,
                    start->y() + dy * parameter + inwardSide * ny * inwardOffsetPixels,
                };
                return hasColorNear(frame, sample, matches);
            });
    };
    const auto opaqueAdjoinEdgeSpanVisible =
        [&adjoinEdgeSpanVisibleWithColor, &containsCyan](const QImage& frame)
    {
        return adjoinEdgeSpanVisibleWithColor(frame, containsCyan, 2.0);
    };
    const auto selectedAdjoinEdgeSpanVisible =
        [&adjoinEdgeSpanVisibleWithColor, &containsYellow](const QImage& frame)
    {
        return adjoinEdgeSpanVisibleWithColor(frame, containsYellow, 1.0);
    };
    const auto rearFilledEdgeSpanHasYellow =
        [&projectPixel, &hasColorNear, &containsYellow](const QImage& frame)
    {
        const QVector3D startWorld{ -1.25f, -0.30f, -0.01f };
        const QVector3D endWorld{ 1.25f, -0.30f, -0.01f };
        const std::optional<QPointF> start = projectPixel(frame, startWorld);
        const std::optional<QPointF> end = projectPixel(frame, endWorld);
        if (!start || !end)
        {
            return true;
        }
        const double dx = end->x() - start->x();
        const double dy = end->y() - start->y();
        for (const double parameter : { 0.25, 0.50, 0.75 })
        {
            const QPointF sample{
                start->x() + dx * parameter,
                start->y() + dy * parameter,
            };
            if (hasColorNear(frame, sample, containsYellow))
            {
                return true;
            }
        }
        return false;
    };

    view.setLayerVisibilityState({ 1, 2, 3 }, true);
    const QImage foregroundOnly = captureFrame();
    const std::optional<QPointF> foregroundWidgetSample =
        view.projectWorldToViewport(QVector3D{ 0.0f, 0.0f, 0.0f });
    const std::optional<float> foregroundDepth = foregroundWidgetSample
        ? view.architecturalLightingSceneDepthForTest(*foregroundWidgetSample)
        : std::nullopt;
    const auto selectedAdjoinEdgeVisibleOverOpaqueFill =
        [&view, &projectPixel, &containsYellow, foregroundDepth](
            const QImage& frame)
    {
        if (!foregroundDepth || !std::isfinite(*foregroundDepth)
            || frame.isNull() || frame.width() <= 0 || frame.height() <= 0)
        {
            return false;
        }
        const auto start = projectPixel(
            frame, QVector3D{ -1.2f, 0.405f, -0.002f });
        const auto end = projectPixel(
            frame, QVector3D{ 1.2f, 0.405f, -0.002f });
        const auto interior = projectPixel(
            frame, QVector3D{ 0.0f, 0.6025f, 3.999f });
        if (!start || !end || !interior)
        {
            return false;
        }
        const double dx = end->x() - start->x();
        const double dy = end->y() - start->y();
        const double length = std::hypot(dx, dy);
        if (!(length > 1.0))
        {
            return false;
        }
        const double sideCross =
            dx * (interior->y() - 0.5 * (start->y() + end->y()))
            - dy * (interior->x() - 0.5 * (start->x() + end->x()));
        if (std::abs(sideCross) <= 1.0e-8)
        {
            return false;
        }
        const double inwardSide = sideCross > 0.0 ? 1.0 : -1.0;
        const double inwardX = -dy / length * inwardSide;
        const double inwardY = dx / length * inwardSide;
        constexpr std::array<double, 3> kInteriorParameters{ 0.20, 0.50, 0.80 };
        return std::all_of(
            kInteriorParameters.begin(), kInteriorParameters.end(),
            [&view, &frame, &start, dx, dy, inwardX, inwardY, foregroundDepth,
             &containsYellow](const double parameter)
            {
                // Sample a single pixel center rather than accepting a nearby
                // colored fragment that may be outside the selected stroke or
                // still hidden by the foreground.
                const double lineX = start->x() + dx * parameter;
                const double lineY = start->y() + dy * parameter;
                const int x = static_cast<int>(std::lround(
                    lineX + inwardX * 2.0));
                const int y = static_cast<int>(std::lround(
                    lineY + inwardY * 2.0));
                if (x < 0 || x >= frame.width() || y < 0 || y >= frame.height())
                {
                    return false;
                }
                const double signedInwardDistance =
                    (static_cast<double>(x) + 0.5 - lineX) * inwardX
                    + (static_cast<double>(y) + 0.5 - lineY) * inwardY;
                if (signedInwardDistance < 0.5
                    || signedInwardDistance > 1.6
                    || !containsYellow(frame.pixelColor(x, y)))
                {
                    return false;
                }
                const QPointF widgetPoint{
                    (static_cast<double>(x) + 0.5)
                        * static_cast<double>(view.width()) / frame.width(),
                    (static_cast<double>(y) + 0.5)
                        * static_cast<double>(view.height()) / frame.height(),
                };
                const auto sceneDepth =
                    view.architecturalLightingSceneDepthForTest(widgetPoint);
                return sceneDepth
                    && *sceneDepth < *foregroundDepth - 1.0e-6f;
            });
    };
    const bool savedBroadDepthRange = view.cameraClipEnabled()
        && std::abs(view.cameraNearPlane() + 10000.0f) < 1.0e-4f
        && std::abs(view.cameraFarPlane() - 10000.0f) < 1.0e-4f
        && view.gridAlwaysBehind();
    const bool foregroundCoversRearSpan = foregroundDepth
        && std::isfinite(*foregroundDepth)
        && *foregroundDepth < 1.0f - 1.0e-6f;
    const bool drawnSurfaceEdgeVisibleAtTopDown =
        solidEdgeSpanVisible(foregroundOnly);

    view.setLayerVisibilityState({ 1, 3 }, true);
    const QImage opaqueAdjoinOnly = captureFrame();
    const bool opaqueAdjoinEdgeVisibleAtTopDown =
        opaqueAdjoinEdgeSpanVisible(opaqueAdjoinOnly);

    view.setLayerVisibilityState({}, true);
    const QImage exactTopDown = captureFrame();
    const float farViewZAtBroadClip = viewSpaceZ(
        QVector3D{ 0.0f, 1.20f, -500.0f });
    const float frontViewZAtBroadClip = viewSpaceZ(
        QVector3D{ 0.0f, 0.0f, 0.0f });
    const float rearViewZAtBroadClip = viewSpaceZ(rearStart);
    const bool farGeometryVisibleAtBroadAuthoredClip =
        farGeometryEdgeVisible(exactTopDown);
    const bool farGeometrySamplesProjectableAtBroadAuthoredClip =
        farGeometryEdgeSamplesProjectable(exactTopDown);
    const bool farGeometryHasProjectedPointAtBroadAuthoredClip =
        view.projectWorldToViewport(QVector3D{ 0.0f, 1.20f, -500.0f }).has_value();
    const bool worldOriginRemainsInExactTopDownProjection =
        view.projectWorldToViewport(QVector3D{ 0.0f, 0.0f, 0.0f }).has_value();
    const std::optional<bool> rearEdgeLeakAtTopDown =
        rearSpanHasMagenta(exactTopDown);
    const bool rearEdgeProjectableAtTopDown = rearEdgeLeakAtTopDown.has_value();
    const bool rearEdgeHiddenAtTopDown = rearEdgeProjectableAtTopDown
        && !*rearEdgeLeakAtTopDown;
    const bool rearFilledEdgeHiddenAtTopDown =
        !rearFilledEdgeSpanHasYellow(exactTopDown);

    // The level contains z=-500 geometry. Rebuilding the complete scene also
    // increases the orbit radius and minimum camera distance, so an arbitrary
    // ±16 interval clips the z≈0 regression edge itself. Compute its actual
    // view-space depth and retain four world units on either side, while still
    // making the depth interval dramatically narrower than the saved ±10000.
    const float tightClipHalfSpan = std::max(
        24.0f,
        std::abs(rearViewZAtBroadClip) + 4.0f);
    view.setCameraNearFar(-tightClipHalfSpan, tightClipHalfSpan);
    const QImage tightDepthControl = captureFrame();
    const std::optional<bool> rearEdgeLeakWithTightRange =
        rearSpanHasMagenta(tightDepthControl);
    const bool rearEdgeProjectableWithTightRange =
        rearEdgeLeakWithTightRange.has_value();
    const bool rearEdgeHiddenWithTightRange = rearEdgeProjectableWithTightRange
        && !*rearEdgeLeakWithTightRange;
    const float tightFrontViewZ = viewSpaceZ(
        QVector3D{ 0.0f, 0.0f, 0.0f });
    const float tightRearViewZ = viewSpaceZ(rearStart);
    const float tightRearFrontGap = std::abs(tightFrontViewZ - tightRearViewZ);
    const float tightNearPlane = view.cameraNearPlane();
    const float tightFarPlane = view.cameraFarPlane();
    view.setCameraNearFar(-10000.0f, 10000.0f);
    const QImage broadAuthoredClip = captureFrame();
    const bool farGeometryVisibleAfterRangeRestore =
        farGeometryEdgeVisible(broadAuthoredClip);
    const bool farGeometrySamplesProjectableAfterRangeRestore =
        farGeometryEdgeSamplesProjectable(broadAuthoredClip);
    view.setCameraNearFar(-100.0f, 100.0f);
    const QImage narrowAuthoredClip = captureFrame();
    const bool farGeometryHiddenByNarrowAuthoredClip =
        !farGeometryEdgeVisible(narrowAuthoredClip)
        && !view.projectWorldToViewport(
            QVector3D{ 0.0f, 1.20f, -500.0f }).has_value();
    const bool authoredClipPlanesRemainUnchanged =
        std::abs(view.cameraNearPlane() + 100.0f) < 1.0e-4f
        && std::abs(view.cameraFarPlane() - 100.0f) < 1.0e-4f;
    view.setCameraNearFar(-10000.0f, 10000.0f);

    view.setOrbitViewImmediate(0.0f, 89.5f, 0.0f);
    const QImage halfDegreeTilt = captureFrame();
    const float frontViewZAfterTilt = viewSpaceZ(
        QVector3D{ 0.0f, 0.0f, 0.0f });
    const float rearViewZAfterTilt = viewSpaceZ(rearStart);
    const bool drawnSurfaceEdgeVisibleAfterTilt =
        solidEdgeSpanVisible(halfDegreeTilt);
    const bool worldOriginRemainsInTiltedProjection =
        view.projectWorldToViewport(QVector3D{ 0.0f, 0.0f, 0.0f }).has_value();
    const bool opaqueAdjoinEdgeVisibleAfterTilt =
        opaqueAdjoinEdgeSpanVisible(halfDegreeTilt);
    const std::optional<bool> rearEdgeLeakAfterTilt =
        rearSpanHasMagenta(halfDegreeTilt);
    const bool rearEdgeProjectableAfterTilt = rearEdgeLeakAfterTilt.has_value();
    const bool rearEdgeHiddenAfterTilt = rearEdgeProjectableAfterTilt
        && !*rearEdgeLeakAfterTilt;
    const bool rearFilledEdgeHiddenAfterTilt =
        !rearFilledEdgeSpanHasYellow(halfDegreeTilt);

    check(foregroundCoversRearSpan,
        "wireframe precision regression proves the foreground-only frame covers the rear-edge sample");
    check(savedBroadDepthRange,
        "wireframe precision regression uses the persisted ±10000 orthographic clip range and grid-behind setting");
    check(drawnSurfaceEdgeVisibleAtTopDown
            && worldOriginRemainsInExactTopDownProjection,
        "coplanar wireframe and the world origin remain inside the exact top-down projection");
    check(opaqueAdjoinEdgeVisibleAtTopDown,
        "opaque filled-adjoin wireframe remains visible across its inward edge at exact top-down");
    check(farGeometryHasProjectedPointAtBroadAuthoredClip
            && farGeometrySamplesProjectableAtBroadAuthoredClip,
        "far level geometry remains inside the authored orthographic clip interval");
    check(farGeometryVisibleAtBroadAuthoredClip,
        "far level geometry wireframe is visible inside the authored clip interval");
    check(farGeometrySamplesProjectableAfterRangeRestore,
        "restoring broad authored clipping keeps far geometry projectable");
    check(farGeometryVisibleAfterRangeRestore,
        "restoring broad authored clipping restores the far geometry wireframe");
    check(farGeometryHiddenByNarrowAuthoredClip,
        "effective orthographic range respects narrow authored clipping");
    check(authoredClipPlanesRemainUnchanged,
        "effective orthographic projection does not rewrite authored clip settings");
    check(rearEdgeProjectableAtTopDown,
        "rear-edge regression endpoints project at exact top-down");
    check(rearEdgeHiddenAtTopDown,
        "wireframe precision regression keeps a sub-depth-step rear portal edge hidden at exact top-down");
    check(rearFilledEdgeHiddenAtTopDown,
        "wireframe precision regression keeps a fully opaque rear surface edge hidden at exact top-down");
    check(rearEdgeProjectableWithTightRange,
        "tight orthographic control retains the tested rear edge inside its clip interval");
    check(rearEdgeHiddenWithTightRange,
        "wireframe precision control hides the same sub-depth-step rear edge with tight clipping");
    check(drawnSurfaceEdgeVisibleAfterTilt && worldOriginRemainsInTiltedProjection,
        "coplanar wireframe and the world origin remain inside the 0.5-degree projection");
    check(opaqueAdjoinEdgeVisibleAfterTilt,
        "opaque filled-adjoin wireframe remains visible across its inward edge after a 0.5-degree orbit");
    check(rearEdgeProjectableAfterTilt,
        "rear-edge regression endpoints project after a 0.5-degree orbit");
    check(rearEdgeHiddenAfterTilt,
        "wireframe precision regression keeps a sub-depth-step rear portal edge hidden after a 0.5-degree orbit");
    check(rearFilledEdgeHiddenAfterTilt,
        "wireframe precision regression keeps a fully opaque rear surface edge hidden after a 0.5-degree orbit");

    if (!farGeometryHasProjectedPointAtBroadAuthoredClip
        || !farGeometrySamplesProjectableAtBroadAuthoredClip
        || !farGeometryVisibleAtBroadAuthoredClip
        || !farGeometryVisibleAfterRangeRestore
        || !rearEdgeProjectableAtTopDown
        || !rearEdgeProjectableWithTightRange
        || !rearEdgeProjectableAfterTilt
        || !rearEdgeHiddenAtTopDown
        || !rearEdgeHiddenWithTightRange
        || !rearEdgeHiddenAfterTilt)
    {
        std::cerr << "wireframe clip diagnostic: authored=["
                  << view.cameraNearPlane() << ", " << view.cameraFarPlane()
                  << "], tightRequested=[-" << tightClipHalfSpan << ", "
                  << tightClipHalfSpan << "], tightAuthored=["
                  << tightNearPlane << ", " << tightFarPlane
                  << "], viewZ(far/front/rear)=["
                  << farViewZAtBroadClip << ", " << frontViewZAtBroadClip
                  << ", " << rearViewZAtBroadClip << "], tightViewZ(front/rear/gap)=["
                  << tightFrontViewZ << ", " << tightRearViewZ << ", "
                  << tightRearFrontGap << "], tiltedViewZ(front/rear)=["
                  << frontViewZAfterTilt << ", " << rearViewZAfterTilt
                  << "], farProjected/colored(broad/restore)=["
                  << farGeometrySamplesProjectableAtBroadAuthoredClip << ", "
                  << farGeometryVisibleAtBroadAuthoredClip << ", "
                  << farGeometrySamplesProjectableAfterRangeRestore << ", "
                  << farGeometryVisibleAfterRangeRestore
                  << "], rearProjected(top/tight/tilt)=["
                  << rearEdgeProjectableAtTopDown << ", "
                  << rearEdgeProjectableWithTightRange << ", "
                  << rearEdgeProjectableAfterTilt << "], rearLeak(top/tight/tilt)=["
                  << rearEdgeLeakAtTopDown.value_or(false) << ", "
                  << rearEdgeLeakWithTightRange.value_or(false) << ", "
                  << rearEdgeLeakAfterTilt.value_or(false) << "]\n";
    }

    QImage transparentAdjoinTexture(2, 2, QImage::Format_RGBA8888);
    transparentAdjoinTexture.fill(QColor(255, 255, 255, 0));
    const std::string alphaAdjoinColormap = level.getMasterCMP();
    view.primeMaterialTexture(
        "wireframe_alpha_adjoin_test.mat",
        alphaAdjoinColormap,
        transparentAdjoinTexture,
        8);
    view.setShowTextures(true);
    view.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
    const QImage alphaAdjoinTopDown = captureFrame();
    const std::optional<std::int32_t> loadedAlphaBpp =
        view.materialTextureAlphaBppForTest(
            "wireframe_alpha_adjoin_test.mat",
            alphaAdjoinColormap);
    const bool alphaTextureWasLoaded = loadedAlphaBpp && *loadedAlphaBpp == 8;
    check(alphaTextureWasLoaded,
        "alpha-adjoin wire regression loads its prepared transparent MAT with 8-bit alpha");
    const bool alphaAdjoinStaysHiddenAtTopDown =
        !opaqueAdjoinEdgeSpanVisible(alphaAdjoinTopDown);
    view.setOrbitViewImmediate(0.0f, 89.0f, 0.0f);
    const QImage alphaAdjoinTilt = captureFrame();
    const bool alphaAdjoinStaysHiddenAfterTilt =
        !opaqueAdjoinEdgeSpanVisible(alphaAdjoinTilt);
    check(alphaAdjoinStaysHiddenAtTopDown,
        "alpha-textured adjoin wire does not match a non-depth-writing face at exact top-down");
    check(alphaAdjoinStaysHiddenAfterTilt,
        "alpha-textured adjoin wire does not match a non-depth-writing face after a one-degree orbit");

    view.setShowTextures(false);
    view.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
    const QImage texturesDisabledTopDown = captureFrame();
    const bool opaqueAdjoinReturnsWhenTexturesDisabled =
        opaqueAdjoinEdgeSpanVisible(texturesDisabledTopDown);
    check(opaqueAdjoinReturnsWhenTexturesDisabled,
        "disabling textures restores depth matching for the now-opaque adjoin fill");

    view.setShowTextures(true);
    const QImage texturesReenabledTopDown = captureFrame();
    const bool alphaAdjoinHiddenAfterToggle =
        !opaqueAdjoinEdgeSpanVisible(texturesReenabledTopDown);
    check(alphaAdjoinHiddenAfterToggle,
        "enabling textures reclassifies the alpha adjoin before the next wireframe draw");

    view.reloadCachedResources();
    view.primeMaterialTexture(
        "wireframe_alpha_adjoin_test.mat",
        alphaAdjoinColormap,
        transparentAdjoinTexture,
        8);
    const QImage alphaAdjoinAfterResourceReload = captureFrame();
    const bool alphaAdjoinHiddenAfterReload =
        !opaqueAdjoinEdgeSpanVisible(alphaAdjoinAfterResourceReload);
    check(alphaAdjoinHiddenAfterReload,
        "resource reload reapplies alpha-adjoin wire depth classification before rendering");

    editor::Selection selectedAdjoin{};
    selectedAdjoin.surfaces.insert(SurfaceRef{ 2, 0 });
    view.setSelectionPrimaryColor(QColor(255, 255, 0));
    view.setShowTextures(false);
    view.setSelectionState(2, 0, -1, -1, selectedAdjoin);
    const auto selectedAdjoinPlaneSupportValid = [&view]()
    {
        const auto support = view.selectionWirePlaneSupportForTest(
            QVector3D{ -1.2f, 0.405f, -0.002f },
            QVector3D{ 1.2f, 0.405f, -0.002f });
        if (!support || !support->valid)
        {
            return false;
        }
        const QVector3D expectedReference{ -1.2f, 0.8f, 8.0f };
        return (support->reference - expectedReference).lengthSquared() <= 1.0e-8f;
    };
    view.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
    const QImage selectedFaceOpaque = captureFrame();
    const bool selectedFaceHasValidPlaneSupport =
        selectedAdjoinPlaneSupportValid();
    const bool selectedFaceCachedWireVisibleWhenOpaque =
        selectedAdjoinEdgeVisibleOverOpaqueFill(selectedFaceOpaque);
    view.setShowTextures(true);
    view.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
    const QImage selectedFaceAlphaTopDown = captureFrame();
    const bool selectedFaceCachedWireHiddenAtTopDown =
        !selectedAdjoinEdgeSpanVisible(selectedFaceAlphaTopDown);
    view.setOrbitViewImmediate(0.0f, 89.0f, 0.0f);
    const QImage selectedFaceAlphaTilt = captureFrame();
    const bool selectedFaceCachedWireHiddenAfterTilt =
        !selectedAdjoinEdgeSpanVisible(selectedFaceAlphaTilt);
    check(selectedFaceHasValidPlaneSupport,
        "selected-face cached adjoin wire retains matching endpoint plane support");
    check(selectedFaceCachedWireVisibleWhenOpaque,
        "selected-face cached adjoin wire remains visible with an opaque fill");
    check(selectedFaceCachedWireHiddenAtTopDown,
        "selected-face cached adjoin wire stays hidden when alpha fill does not write depth at exact top-down");
    check(selectedFaceCachedWireHiddenAfterTilt,
        "selected-face cached adjoin wire stays hidden when alpha fill does not write depth after a one-degree orbit");

    view.setSurfaceSubMode(GlView::SurfaceSubMode::Edge);
    view.setSelectedSurfaceEdgesForTest(
        { { SurfaceRef{ 2, 0 }, 1 } },
        0);
    const bool selectedEdgeOwnerWasRebuilt =
        view.selectionHighlightLineCountForTest() > 0
        && view.activeSurfaceEdgeIsSelectedForTest();
    view.setShowTextures(false);
    view.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
    const QImage selectedEdgeOpaque = captureFrame();
    const bool selectedEdgeHasValidPlaneSupport =
        selectedAdjoinPlaneSupportValid();
    const bool selectedEdgeRebuiltWireVisibleWhenOpaque =
        selectedAdjoinEdgeVisibleOverOpaqueFill(selectedEdgeOpaque);
    view.setShowTextures(true);
    view.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
    const QImage selectedEdgeAlphaTopDown = captureFrame();
    const bool selectedEdgeRebuiltWireHiddenAtTopDown =
        !selectedAdjoinEdgeSpanVisible(selectedEdgeAlphaTopDown);
    view.setOrbitViewImmediate(0.0f, 89.0f, 0.0f);
    const QImage selectedEdgeAlphaTilt = captureFrame();
    const bool selectedEdgeRebuiltWireHiddenAfterTilt =
        !selectedAdjoinEdgeSpanVisible(selectedEdgeAlphaTilt);
    check(selectedEdgeOwnerWasRebuilt,
        "selected-edge fixture rebuilds the owner wire around a distinct active edge");
    check(selectedEdgeHasValidPlaneSupport,
        "selected-edge rebuilt adjoin wire retains matching endpoint plane support");
    check(selectedEdgeRebuiltWireVisibleWhenOpaque,
        "selected-edge rebuilt adjoin wire remains visible with an opaque fill");
    check(selectedEdgeRebuiltWireHiddenAtTopDown,
        "selected-edge rebuilt adjoin wire stays hidden when alpha fill does not write depth at exact top-down");
    check(selectedEdgeRebuiltWireHiddenAfterTilt,
        "selected-edge rebuilt adjoin wire stays hidden when alpha fill does not write depth after a one-degree orbit");
    view.hide();
}

void runNormalModelLightingModeTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(makeRenderState());
    window.mutableDocumentStateForTest().level.sectors.emplace_back();
    window.mutableDocumentStateForTest().level.sectors[0].ambient =
        ColorF{ 0.33f, 0.33f, 0.33f, 1.0f };
    window.mutableDocumentStateForTest().level.things[0].sector = 0;

    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING normal-lighting fixture exposes the MainWindow GlView");
    if (!view)
    {
        return;
    }

    view->setShowTextures(false);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->setLightingMode(GlView::LightingMode::Gouraud);
    view->primeModel3do(
        "arch_render_fixture.3do",
        makeRenderModel(TLightMode::Gouraud));
    view->setLevel(&window.documentStateForTest().level);

    // Gouraud model colors are deferred to the shader's classic dynamic-light
    // path. The CPU cache therefore keeps the authored ARCH RGB here; the
    // sector ambient floor is asserted by the framebuffer regression below.
    const ColorF expectedRgb{ 0.25f, 0.5f, 0.875f, 1.0f };
    check(sameColor(view->thingModelVertexColorForTest(0), expectedRgb),
        "ARCHLIGHTING ordinary Gouraud model lighting publishes the accelerated RGB channels");

    view->setLightingMode(GlView::LightingMode::FullyLit);
    const ColorF expectedFullyLit{ 1.0f, 1.0f, 1.0f, 1.0f };
    check(sameColor(view->thingModelVertexColorForTest(0), expectedFullyLit),
        "ARCHLIGHTING FullyLit keeps the engine's intentional white result instead of forcing authored ARCH channels");
}

void runNormalModelFramebufferTest()
{
    editor::DocumentState state = makeRenderState();
    state.level.sectors.emplace_back();
    // Match s1l2 Thing 157's sector 347 ambient floor. The sector-48 doors
    // use .33 and legitimately hide their lower ARCH values behind the
    // classic max(ambient, authored) Gouraud rule.
    state.level.sectors[0].ambient = ColorF{ 0.03f, 0.03f, 0.03f, 1.0f };
    state.level.things[0].sector = 0;
    // The synthetic model is one world unit wide.  Give the fixture the same
    // finite bounds so the normal level fit frames the model instead of
    // fitting only the Thing origin (an origin-only fit makes the interior
    // sample project far outside the framebuffer).
    state.level.things[0].bbox.x1 = 0.0;
    state.level.things[0].bbox.y1 = 0.0;
    state.level.things[0].bbox.z1 = 0.0;
    state.level.things[0].bbox.x2 = 1.0;
    state.level.things[0].bbox.y2 = 1.0;
    state.level.things[0].bbox.z2 = 0.0;
    // Use a red-dominant ARCH payload for the readback differential. The
    // ordinary history fixtures intentionally retain their blue-dominant
    // values, while this capture must distinguish ARCH from the model's
    // zero-intensity authored vertex color.
    state.level.motsArchLighting[0].meshes[0].vertices = {
        MotsArchLightVertex{ 0.125f, 0.90f, 0.05f, 0.05f },
        MotsArchLightVertex{ 0.125f, 0.80f, 0.05f, 0.05f },
        MotsArchLightVertex{ 0.125f, 0.70f, 0.05f, 0.05f },
    };
    GlView view{};
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.resize(320, 320);
    view.setBackgroundClearColor(Qt::black);
    view.setShowGrid(false);
    view.setShowWireframe(false);
    view.setShowVertices(false);
    view.setShowSolid(true);
    view.setShowTextures(false);
    view.setShowThings(true);
    view.setShowThingGizmos(false);
    view.setShowAxisWidget(false);
    view.setShowToastOverlays(false);
    view.setShowEditorCubes(false);
    view.setThingViewMode(GlView::ThingViewMode::Shaded);
    view.setLightingMode(GlView::LightingMode::Gouraud);
    view.setRenderPipeline(GlView::RenderPipeline::Editor);
    // Keep a visibly different authored 3DO color underneath the ARCH
    // record. When the explicit record is removed, MotS's effective 0x8
    // default must replace it with zero RGB in the same Gouraud pipeline.
    auto framebufferModel = std::make_shared<assets::Model3do>(
        *makeRenderModel(TLightMode::Gouraud));
    for (assets::Model3doVertex& vertex : framebufferModel->meshes[0].vertices)
    {
        vertex.color = ColorF{ 0.05f, 0.70f, 0.75f, 1.0f };
    }
    // This capture isolates ARCH channel composition from camera-facing
    // single-sided model winding. Picking retains a separate sidedness test;
    // the lighting differential must first prove that the shaded triangle is
    // actually rasterized in this synthetic top-down fixture.
    framebufferModel->meshes[0].faces[0].faceFlags |= FF_DoubleSided;
    // A standalone GlView has no owning MainWindow ResourceLocator.  The
    // renderer's model lookup checks that owner before consulting the primed
    // cache, so install a lifetime-stable empty locator before priming the
    // synthetic model.  The cache hit remains deterministic and performs no
    // filesystem read.
    assets::ResourceLocator framebufferResources{};
    view.setResourceLocator(&framebufferResources, false);
    view.primeModel3do("arch_render_fixture.3do", framebufferModel);
    view.setLevel(&state.level);
    check(view.thingModelVertexColorForTest(0).has_value(),
        "ARCHLIGHTING framebuffer fixture publishes a CPU triangle before capture");
    view.resetEditorTopDownView();
    // Frame the actual model bounds after the editor reset.  The synthetic
    // Level has no surfaces, so its generic bounds contain only the Thing
    // origin; the 3DO triangle itself is the authoritative capture extent.
    view.frameWorldBounds(
        QVector3D{ 0.0f, 0.0f, 0.0f },
        QVector3D{ 1.0f, 1.0f, 0.0f },
        false);
    view.show();
    QApplication::processEvents();

    const auto sampleFrame = [&view](const QImage& frame) -> std::optional<QColor>
    {
        if (frame.isNull() || frame.width() <= 0 || frame.height() <= 0)
        {
            return std::nullopt;
        }

        const std::optional<QPointF> widgetPoint = view.projectWorldToViewport(
            QVector3D{ 0.25f, 0.25f, 0.0f });
        if (!widgetPoint
            || widgetPoint->x() < 0.0
            || widgetPoint->x() >= static_cast<qreal>(view.width())
            || widgetPoint->y() < 0.0
            || widgetPoint->y() >= static_cast<qreal>(view.height()))
        {
            return std::nullopt;
        }

        const double imageScaleX = static_cast<double>(frame.width())
            / std::max(1, view.width());
        const double imageScaleY = static_cast<double>(frame.height())
            / std::max(1, view.height());
        const int centerX = std::clamp(
            static_cast<int>(std::lround(widgetPoint->x() * imageScaleX)),
            0,
            frame.width() - 1);
        const int centerY = std::clamp(
            static_cast<int>(std::lround(widgetPoint->y() * imageScaleY)),
            0,
            frame.height() - 1);

        std::int64_t red = 0;
        std::int64_t green = 0;
        std::int64_t blue = 0;
        int count = 0;
        for (int y = std::max(0, centerY - 2);
             y <= std::min(frame.height() - 1, centerY + 2);
             ++y)
        {
            for (int x = std::max(0, centerX - 2);
                 x <= std::min(frame.width() - 1, centerX + 2);
                 ++x)
            {
                const QColor pixel = frame.pixelColor(x, y);
                red += pixel.red();
                green += pixel.green();
                blue += pixel.blue();
                ++count;
            }
        }

        return count > 0
            ? std::optional<QColor>{ QColor(
                static_cast<int>(red / count),
                static_cast<int>(green / count),
                static_cast<int>(blue / count)) }
            : std::nullopt;
    };

    const auto captureFrame = [&]() -> QImage
    {
        (void)view.grabFramebuffer();
        view.repaint();
        QApplication::processEvents();
        return view.grabFramebuffer();
    };

    const QImage archFrame = captureFrame();
    const std::optional<QColor> archSample = sampleFrame(archFrame);

    // Keep the model, camera, and ambient sector identical. Removing only the
    // ARCH binding proves that the framebuffer difference comes from the
    // archive channels rather than a stale cache or an unrelated overlay.
    state.level.things[0].archLightingIndex = -1;
    view.notifyThingAppearanceChanged();
    const QImage modelFrame = captureFrame();
    const std::optional<QColor> modelSample = sampleFrame(modelFrame);

    // FullyLit is an intentional control: it must remain white and therefore
    // must not change when the ARCH binding is toggled.
    state.level.things[0].archLightingIndex = 0;
    view.setLightingMode(GlView::LightingMode::FullyLit);
    view.notifyThingAppearanceChanged();
    const QImage fullyLitArchFrame = captureFrame();
    const std::optional<QColor> fullyLitArchSample = sampleFrame(fullyLitArchFrame);
    state.level.things[0].archLightingIndex = -1;
    view.notifyThingAppearanceChanged();
    const QImage fullyLitModelFrame = captureFrame();
    const std::optional<QColor> fullyLitModelSample = sampleFrame(fullyLitModelFrame);

    const bool archReadbackIsDistinct = archSample && modelSample
        // Removing the explicit record leaves an effective LevelGeo Thing
        // on MotS's virtual all-zero ARCHLIGHTING default. The ordinary
        // Gouraud path must therefore show the red-dominant explicit record
        // against a dark zero-default sample, not the authored 3DO colors.
        && archSample->red() > archSample->green() + 64
        && archSample->red() > archSample->blue() + 64
        && archSample->red() > modelSample->red() + 96
        && modelSample->green() < 64
        && modelSample->blue() < 64;
    const bool fullyLitIsBindingIndependent = fullyLitArchSample
        && fullyLitModelSample
        && fullyLitArchSample->red() >= 240
        && fullyLitArchSample->green() >= 240
        && fullyLitArchSample->blue() >= 240
        && fullyLitModelSample->red() >= 240
        && fullyLitModelSample->green() >= 240
        && fullyLitModelSample->blue() >= 240
        && std::abs(fullyLitArchSample->red() - fullyLitModelSample->red()) <= 1
        && std::abs(fullyLitArchSample->green() - fullyLitModelSample->green()) <= 1
        && std::abs(fullyLitArchSample->blue() - fullyLitModelSample->blue()) <= 1;

    const bool shaderReady = view.mainShaderReadyForTest();
    const bool framebufferCheckPassed = shaderReady
        && archReadbackIsDistinct
        && fullyLitIsBindingIndependent;
    check(framebufferCheckPassed,
        "ARCHLIGHTING ordinary Gouraud framebuffer distinguishes an explicit RGB record from the virtual zero default");
    view.hide();
}

void runSelectedVertexAccessorTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(makeRenderState());

    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING selected-vertex fixture exposes the MainWindow GlView");
    if (!view)
    {
        return;
    }

    view->setShowTextures(false);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->primeModel3do("arch_render_fixture.3do", makeRenderModel());
    view->setLevel(&window.documentStateForTest().level);
    view->setArchitecturalLightingEditingActive(true);
    view->setSelectedThing(0);

    const std::uint64_t readsBeforeQuery = view->model3doResourceReadCountForTest();
    const auto vertices = view->architecturalLightingVerticesForThing(0);
    const bool published = vertices.size() == 3
        && vertices[0].thingIndex == 0
        && vertices[0].lodIndex == 0
        && vertices[0].meshIndex == 0
        && vertices[0].vertexIndex == 0
        && vertices[0].worldPosition == QVector3D{ 0.0f, 0.0f, 0.0f }
        && vertices[0].lighting
            == MotsArchLightVertex{ 0.125f, 0.25f, 0.5f, 0.875f };
    check(published,
        "ARCHLIGHTING selected-owner accessor publishes model vertex identities and exact world positions");

    const auto repeat = view->architecturalLightingVerticesForThing(0);
    check(repeat.size() == vertices.size()
            && view->model3doResourceReadCountForTest() == readsBeforeQuery,
        "ARCHLIGHTING selected-owner accessor remains read-only and does not load a 3DO on repeated queries");

    Level& live = window.mutableDocumentStateForTest().level;
    live.motsArchLighting[0].meshes[0].vertices[0].red = 0.91f;
    view->notifyThingAppearanceChanged();
    const auto tableRepublished = view->architecturalLightingVerticesForThing(0);
    check(tableRepublished.size() == 3
            && tableRepublished[0].lighting.red == 0.91f,
        "ARCHLIGHTING selected-owner accessor refreshes after a published table edit");

    live.things[0].archLightingIndex = -1;
    view->notifyThingAppearanceChanged();
    const auto virtualDefault = view->architecturalLightingVerticesForThing(0);
    check(virtualDefault.size() == 3
            && std::all_of(virtualDefault.begin(), virtualDefault.end(),
                [](const GlView::ArchitecturalLightingRenderVertex& vertex)
                {
                    return vertex.lighting == MotsArchLightVertex{};
                }),
        "ARCHLIGHTING selected-owner accessor publishes virtual zero vertices after a binding reset");

    live.things[0].archLightingIndex = 0;
    view->notifyThingAppearanceChanged();
    check(view->architecturalLightingVerticesForThing(0).size() == 3,
        "ARCHLIGHTING selected-owner accessor repopulates after binding restoration");

    view->reloadCachedResources();
    check(view->architecturalLightingVerticesForThing(0).empty(),
        "ARCHLIGHTING selected-owner accessor rejects a resource-cache publication");
}

void runArchitecturalLightingSourceMeshMappingTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(makeRenderState());

    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING source-mesh fixture exposes the MainWindow GlView");
    if (!view)
    {
        return;
    }

    view->setShowTextures(false);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    auto model = std::make_shared<assets::Model3do>(*makeRenderModel());
    model->meshes.push_back(model->meshes.front());
    model->hierarchyTransformsApplied = true;
    model->nodes = {
        assets::Model3doNode{ .mesh = 0, .parent = -1 },
        assets::Model3doNode{ .mesh = 1, .parent = -1 },
    };
    model->meshSourceIndices = { 0, 0 };
    view->primeModel3do("arch_render_fixture.3do", model);
    view->setLevel(&window.documentStateForTest().level);
    view->setArchitecturalLightingEditingActive(true);
    view->setSelectedThing(0);

    const auto vertices = view->architecturalLightingVerticesForThing(0);
    const bool sourceIdentityPublished = vertices.size() == 6
        && std::all_of(
            vertices.begin(), vertices.end(),
            [](const GlView::ArchitecturalLightingRenderVertex& vertex)
            {
                return vertex.meshIndex == 0;
            });
    check(sourceIdentityPublished,
        "ARCHLIGHTING expanded hierarchy vertices retain the authored source mesh identity");

    const std::optional<ColorF> sourceColor =
        view->thingModelVertexColorForTest(0, 0, 0);
    const std::optional<ColorF> cloneOrdinalColor =
        view->thingModelVertexColorForTest(0, 1, 0);
    check(sourceColor.has_value() && !cloneOrdinalColor.has_value(),
        "ARCHLIGHTING expanded clone shades the source archive mesh without publishing a clone ordinal");
}

void runArchitecturalLightingPreviewIdentityTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    editor::DocumentState state = makeRenderState();
    state.level.sectors.emplace_back();
    state.level.sectors[0].ambient = ColorF{ 0.2f, 0.3f, 0.4f, 1.0f };
    state.level.things[0].sector = 0;
    window.setDocumentStateForTest(std::move(state));

    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING preview fixture exposes the MainWindow GlView");
    if (!view)
    {
        return;
    }

    view->setShowTextures(false);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    auto model = std::make_shared<assets::Model3do>(
        *makeRenderModel(TLightMode::Gouraud));
    // Keep two source vertices nearly coincident. A positional preview would
    // incorrectly recolor both; the renderer must patch only the selected
    // authored (mesh, vertex) identity.
    model->meshes[0].vertices[1].x = 0.00001;
    model->meshes[0].faces[0].extraLight =
        ColorF{ -0.2f, 0.1f, 0.25f, 1.0f };
    view->primeModel3do("arch_render_fixture.3do", model);
    view->setLevel(&window.documentStateForTest().level);
    view->setArchitecturalLightingEditingActive(true);
    view->setSelectedThing(0);

    const std::optional<ColorF> original0 =
        view->thingModelVertexColorForTest(0, 0, 0);
    const std::optional<ColorF> original1 =
        view->thingModelVertexColorForTest(0, 0, 1);
    check(original0 && original1,
        "ARCHLIGHTING preview fixture publishes both exact source identities");
    if (!original0 || !original1)
    {
        return;
    }

    const auto published = view->architecturalLightingVerticesForThing(0);
    const auto selected = std::find_if(
        published.begin(),
        published.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return vertex.meshIndex == 0 && vertex.vertexIndex == 1;
        });
    if (selected == published.end())
    {
        check(false,
            "ARCHLIGHTING preview fixture resolves the selected source identity");
        return;
    }

    // Negative/HDR channels deliberately remain raw in the deferred Gouraud
    // payload. The shader performs the production ambient/extra-light order
    // and final clamp; the preview must not clamp or replace that path early.
    const GlView::ArchitecturalLightingPreviewVertex rgbPreview{
        *selected,
        MotsArchLightVertex{ -0.4f, -0.2f, 1.4f, 2.2f }};
    view->setArchitecturalLightingPreview(
        std::span<const GlView::ArchitecturalLightingPreviewVertex>(
            &rgbPreview, 1),
        level::MotsArchLightingVertexMode::Rgb);
    const std::optional<ColorF> rgb0 =
        view->thingModelVertexColorForTest(0, 0, 0);
    const std::optional<ColorF> rgb1 =
        view->thingModelVertexColorForTest(0, 0, 1);
    check(rgb0 && *rgb0 == *original0
            && rgb1
            && *rgb1 == ColorF{ -0.2f, 1.4f, 2.2f, 1.0f },
        "ARCHLIGHTING RGB preview patches only the exact source vertex and preserves signed/HDR Gouraud channels");

    const GlView::ArchitecturalLightingPreviewVertex monoPreview{
        *selected,
        MotsArchLightVertex{ -0.35f, 0.8f, 0.1f, 0.2f }};
    view->setArchitecturalLightingPreview(
        std::span<const GlView::ArchitecturalLightingPreviewVertex>(
            &monoPreview, 1),
        level::MotsArchLightingVertexMode::Mono);
    const std::optional<ColorF> mono1 =
        view->thingModelVertexColorForTest(0, 0, 1);
    check(mono1 && *mono1 == ColorF{ -0.35f, -0.35f, -0.35f, 1.0f },
        "ARCHLIGHTING Mono preview selects the archive mono channel without changing identity mapping");

    Level& live = window.mutableDocumentStateForTest().level;
    window.pushUndoForTitleForTest(editor::DocumentDirty::Things);
    live.motsArchLighting[0].meshes[0].vertices[1] =
        MotsArchLightVertex{ 0.6f, 0.31f, 0.32f, 0.33f };
    view->notifyThingAppearanceChanged();
    check(!view->architecturalLightingPreviewActive()
            && view->thingModelVertexColorForTest(0, 0, 1)
                == std::optional<ColorF>{ ColorF{ 0.31f, 0.32f, 0.33f, 1.0f } },
        "ARCHLIGHTING preview invalidates on a published table rebuild before stale identity reuse");

    QAction* const undoAction = window.findChild<QAction*>("edit.undo");
    window.updateEditActionsForTest();
    check(undoAction && undoAction->isEnabled(),
        "ARCHLIGHTING preview fixture exposes Undo for invalidation coverage");
    if (undoAction && undoAction->isEnabled())
    {
        undoAction->trigger();
        check(!view->architecturalLightingPreviewActive()
                && view->thingModelVertexColorForTest(0, 0, 1) == original1,
            "ARCHLIGHTING preview remains cancelled across Undo publication");
    }

    view->clearArchitecturalLightingPreview();
    check(view->thingModelVertexColorForTest(0, 0, 0) == original0
            && view->thingModelVertexColorForTest(0, 0, 1) == original1,
        "ARCHLIGHTING preview clear restores the composed production colors");

    const std::optional<ColorF> replacementBaseline =
        view->thingModelVertexColorForTest(0, 0, 1);
    const auto replacementPublished = view->architecturalLightingVerticesForThing(0);
    const auto replacementSelected = std::find_if(
        replacementPublished.begin(),
        replacementPublished.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return vertex.meshIndex == 0 && vertex.vertexIndex == 1;
        });
    check(replacementBaseline && replacementSelected != replacementPublished.end(),
        "ARCHLIGHTING replacement fixture publishes the post-Undo source identity");
    if (replacementBaseline && replacementSelected != replacementPublished.end())
    {
        const GlView::ArchitecturalLightingPreviewVertex stalePreview{
            *replacementSelected,
            MotsArchLightVertex{ 0.9f, 0.91f, 0.92f, 0.93f }};
        view->setArchitecturalLightingPreview(
            std::span<const GlView::ArchitecturalLightingPreviewVertex>(
                &stalePreview, 1),
            level::MotsArchLightingVertexMode::Rgb);

        auto replacementModel = std::make_shared<assets::Model3do>(
            *makeRenderModel(TLightMode::Gouraud));
        replacementModel->meshes[0].vertices[1].x = 0.00002;
        view->primeModel3do("arch_render_fixture.3do", replacementModel);
        view->notifyAllMeshesChanged();
        check(view->thingModelVertexColorForTest(0, 0, 1)
                == replacementBaseline,
            "ARCHLIGHTING preview invalidates an old source identity after model replacement");
        view->clearArchitecturalLightingPreview();
    }
}

void runArchitecturalLightingLifecycleTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(makeRenderState());

    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING lifecycle fixture exposes the MainWindow GlView");
    if (!view)
    {
        return;
    }

    view->setShowTextures(false);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->primeModel3do("arch_render_fixture.3do", makeRenderModel(TLightMode::Gouraud));
    view->setLevel(&window.documentStateForTest().level);
    view->setArchitecturalLightingEditingActive(true);
    view->setSelectedThing(0);

    // The lifecycle preview targets source vertex 1. Vertex 0 has blue=.875;
    // vertex 1's authored ARCH blue channel is .75.
    const ColorF baseline{ 0.25f, 0.5f, 0.75f, 1.0f };
    auto publishPreview = [&view]() -> bool
    {
        const auto vertices = view->architecturalLightingVerticesForThing(0);
        const auto selected = std::find_if(
            vertices.begin(),
            vertices.end(),
            [](const GlView::ArchitecturalLightingRenderVertex& vertex)
            {
                return vertex.meshIndex == 0 && vertex.vertexIndex == 1;
            });
        if (selected == vertices.end())
        {
            return false;
        }
        const GlView::ArchitecturalLightingPreviewVertex preview{
            *selected,
            MotsArchLightVertex{ 0.61f, 0.62f, 0.63f, 0.64f }};
        view->setArchitecturalLightingPreview(
            std::span<const GlView::ArchitecturalLightingPreviewVertex>(
                &preview, 1),
            level::MotsArchLightingVertexMode::Rgb);
        return true;
    };

    check(publishPreview() && view->architecturalLightingPreviewActive(),
        "ARCHLIGHTING lifecycle fixture stages a preview before publication changes");
    view->setArchitecturalLightingEditingActive(false);
    check(!view->architecturalLightingPreviewActive()
            && view->thingModelVertexColorForTest(0, 0, 1)
                == std::optional<ColorF>{ baseline },
        "ARCHLIGHTING mode exit clears a staged preview and restores the composed color");

    view->setArchitecturalLightingEditingActive(true);
    view->setSelectedThing(0);
    check(publishPreview() && view->architecturalLightingPreviewActive(),
        "ARCHLIGHTING lifecycle fixture restages before same-level publication");
    view->setLevel(
        &window.documentStateForTest().level,
        GlView::SetLevelMode::keepView);
    check(!view->architecturalLightingPreviewActive(),
        "ARCHLIGHTING same-address level publication cancels a staged preview");

    view->setArchitecturalLightingEditingActive(true);
    view->setSelectedThing(0);
    check(publishPreview() && view->architecturalLightingPreviewActive(),
        "ARCHLIGHTING lifecycle fixture restages before resource publication");
    view->reloadCachedResources();
    check(!view->architecturalLightingPreviewActive(),
        "ARCHLIGHTING resource-cache replacement cancels a staged preview");
}

void runArchitecturalLightingMultiOwnerPreviewTests()
{
    editor::DocumentState state = makeRenderState();
    Thing second = state.level.things.front();
    second.name = "arch_render_fixture_second";
    second.templateName = second.name;
    second.x = 2.0;
    second.archLightingIndex = 1;
    second.values[0].stringValue = "arch_render_fixture_second.3do";
    state.level.things.push_back(std::move(second));
    state.level.motsArchLighting.push_back(makeArchObject(
        MotsArchLightVertex{ 0.225f, 0.35f, 0.45f, 0.55f }));

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(std::move(state));
    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING multi-owner preview fixture exposes the MainWindow GlView");
    if (!view)
    {
        return;
    }

    view->setShowTextures(false);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->primeModel3do("arch_render_fixture.3do", makeRenderModel());
    view->primeModel3do(
        "arch_render_fixture_second.3do",
        makeRenderModel(TLightMode::Gouraud));
    view->setLevel(&window.documentStateForTest().level);
    view->setArchitecturalLightingEditingActive(true);

    editor::Selection owners{};
    owners.things.insert(0);
    owners.things.insert(1);
    view->setSelectionState(-1, -1, 0, -1, owners);
    const auto first = view->architecturalLightingVerticesForThing(0);
    const auto secondPublished = view->architecturalLightingVerticesForThing(1);
    const auto firstIt = std::find_if(
        first.begin(),
        first.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return vertex.meshIndex == 0 && vertex.vertexIndex == 1;
        });
    const auto secondIt = std::find_if(
        secondPublished.begin(),
        secondPublished.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return vertex.meshIndex == 0 && vertex.vertexIndex == 1;
        });
    check(firstIt != first.end() && secondIt != secondPublished.end(),
        "ARCHLIGHTING multi-owner preview publishes exact identities for both model resources");
    if (firstIt == first.end() || secondIt == secondPublished.end())
    {
        return;
    }

    const GlView::ArchitecturalLightingPreviewVertex firstPreview{
        *firstIt,
        MotsArchLightVertex{ 0.61f, 0.62f, 0.63f, 0.64f }};
    const GlView::ArchitecturalLightingPreviewVertex secondPreview{
        *secondIt,
        MotsArchLightVertex{ 0.71f, 0.72f, 0.73f, 0.74f }};
    const std::array<GlView::ArchitecturalLightingPreviewVertex, 2> preview{
        firstPreview,
        secondPreview };
    view->setArchitecturalLightingPreview(
        std::span<const GlView::ArchitecturalLightingPreviewVertex>(preview),
        level::MotsArchLightingVertexMode::Rgb);
    check(view->thingModelVertexColorForTest(0, 0, 1)
            == std::optional<ColorF>{ ColorF{ 0.62f, 0.63f, 0.64f, 1.0f } }
            && view->thingModelVertexColorForTest(1, 0, 1)
                == std::optional<ColorF>{ ColorF{ 0.72f, 0.73f, 0.74f, 1.0f } },
        "ARCHLIGHTING preview applies exact channel edits to every selected owner resource");
    view->clearArchitecturalLightingPreview();
}

bool sameColor(const std::optional<ColorF>& actual, const ColorF& expected)
{
    return actual && *actual == expected;
}

void runCachedTablePublicationTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(makeRenderState());

    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING render fixture exposes the MainWindow GlView");
    if (!view)
    {
        return;
    }

    view->setShowTextures(false);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->primeModel3do("arch_render_fixture.3do", makeRenderModel());
    view->setLevel(&window.documentStateForTest().level);

    const ColorF initialColor{ 0.25f, 0.5f, 0.875f, 1.0f };
    const ColorF appliedColor{ 0.75f, 0.5f, 0.125f, 1.0f };
    const std::optional<ColorF> initialCachedColor =
        view->thingModelVertexColorForTest(0);
    check(sameColor(initialCachedColor, initialColor),
        "ARCHLIGHTING cached triangle starts with the authored RGB channels and opaque model alpha");
    if (!sameColor(initialCachedColor, initialColor))
    {
        return;
    }

    // The MainWindow Apply transaction records the complete Things domain so
    // Undo/Redo can restore both the binding and the ARCHLIGHTING table. The
    // table-only publication is deliberately explicit: it must not be routed
    // through the transform-only fast path.
    window.pushUndoForTitleForTest(editor::DocumentDirty::Things);
    window.mutableDocumentStateForTest().level.motsArchLighting[0].meshes[0].vertices[0] =
        MotsArchLightVertex{ 0.5f, 0.75f, 0.5f, 0.125f };
    view->notifyThingAppearanceChanged();
    check(sameColor(view->thingModelVertexColorForTest(0), appliedColor),
        "ARCHLIGHTING table-only Apply republishes the cached triangle RGB payload");

    QAction* const undoAction = window.findChild<QAction*>("edit.undo");
    QAction* const redoAction = window.findChild<QAction*>("edit.redo");
    window.updateEditActionsForTest();
    check(undoAction && undoAction->isEnabled() && redoAction,
        "ARCHLIGHTING render fixture exposes enabled Undo and Redo actions");
    if (!undoAction || !redoAction)
    {
        return;
    }

    undoAction->trigger();
    check(window.documentStateForTest().level.motsArchLighting.size() == 1
            && sameColor(view->thingModelVertexColorForTest(0), initialColor),
        "ARCHLIGHTING Undo restores the table and republishes cached triangle RGB");

    redoAction->trigger();
    check(window.documentStateForTest().level.motsArchLighting.size() == 1
            && sameColor(view->thingModelVertexColorForTest(0), appliedColor),
        "ARCHLIGHTING Redo restores the table and republishes cached triangle RGB");
}

void runAppendHistoryJumpTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(makeRenderState());

    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING append fixture exposes the MainWindow GlView");
    if (!view)
    {
        return;
    }
    view->setShowTextures(false);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->primeModel3do("arch_render_fixture.3do", makeRenderModel());
    view->setLevel(&window.documentStateForTest().level);

    const ColorF initialColor{ 0.25f, 0.5f, 0.875f, 1.0f };
    const ColorF appliedColor{ 0.75f, 0.5f, 0.125f, 1.0f };
    const std::optional<ColorF> initialCachedColor =
        view->thingModelVertexColorForTest(0);
    check(sameColor(initialCachedColor, initialColor),
        "ARCHLIGHTING append fixture establishes its cached triangle baseline");
    if (!sameColor(initialCachedColor, initialColor))
    {
        return;
    }

    window.pushUndoForTitleForTest(editor::DocumentDirty::Things);
    Level& live = window.mutableDocumentStateForTest().level;
    live.motsArchLighting[0].meshes[0].vertices[0] =
        MotsArchLightVertex{ 0.5f, 0.75f, 0.5f, 0.125f };
    live.motsArchLighting.push_back(makeArchObject(
        MotsArchLightVertex{ 0.5f, 0.75f, 0.5f, 0.125f }));
    Thing appended = live.things.front();
    appended.archLightingIndex = 1;
    appended.x = 2.0;
    live.things.push_back(std::move(appended));

    view->notifyThingAppearanceChanged();
    check(live.things.size() == 2
            && sameColor(view->thingModelVertexColorForTest(0), appliedColor)
            && sameColor(view->thingModelVertexColorForTest(1), appliedColor),
        "ARCHLIGHTING table-only append publishes RGB for old and newly appended cached triangles");

    window.jumpToUndoDepthForTest(0);
    check(window.documentStateForTest().level.things.size() == 1
            && sameColor(view->thingModelVertexColorForTest(0), initialColor),
        "ARCHLIGHTING History jump Undo restores the compact table and cached baseline RGB");

    window.jumpToUndoDepthForTest(1);
    check(window.documentStateForTest().level.things.size() == 2
            && sameColor(view->thingModelVertexColorForTest(0), appliedColor)
            && sameColor(view->thingModelVertexColorForTest(1), appliedColor),
        "ARCHLIGHTING History jump Redo bypasses incremental append reuse for cached RGB");
}

void runCompactHistoryJumpTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    editor::DocumentState state = makeRenderState();
    Thing second = state.level.things.front();
    second.name = "arch_render_fixture_second";
    second.templateName = second.name;
    second.x = 2.0;
    state.level.things.push_back(std::move(second));
    window.setDocumentStateForTest(std::move(state));

    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING compact history fixture exposes the MainWindow GlView");
    if (!view)
    {
        return;
    }

    view->setShowTextures(false);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->primeModel3do("arch_render_fixture.3do", makeRenderModel());
    view->setLevel(&window.documentStateForTest().level);

    const ColorF initialColor{ 0.25f, 0.5f, 0.875f, 1.0f };
    const bool baseline =
        sameColor(view->thingModelVertexColorForTest(0), initialColor)
        && sameColor(view->thingModelVertexColorForTest(1), initialColor);
    check(baseline,
        "ARCHLIGHTING compact history fixture establishes both cached RGB baselines");
    if (!baseline)
    {
        return;
    }

    window.pushThingSubsetUndoForTitleForTest({ 0 });
    window.mutableDocumentStateForTest().level.things[0].x = 4.0;
    window.pushThingSubsetUndoForTitleForTest({ 1 });
    window.mutableDocumentStateForTest().level.things[1].x = 8.0;
    view->notifyThingTransformsChanged({ 0, 1 });

    const std::uint64_t rebuildBeforeUndo = view->meshRebuildCountForTest();
    const std::uint64_t resourceReadsBeforeUndo =
        view->model3doResourceReadCountForTest();
    const std::uint64_t fastBeforeUndo =
        view->thingTransformFastPublicationCountForTest();
    window.jumpToUndoDepthForTest(0);
    const Level& undone = window.documentStateForTest().level;
    const bool compactUndoPublished =
        undone.things.size() == 2
        && undone.things[0].x == 0.0
        && undone.things[1].x == 2.0
        && view->meshRebuildCountForTest() == rebuildBeforeUndo
        && view->model3doResourceReadCountForTest() == resourceReadsBeforeUndo
        && view->thingTransformFastPublicationCountForTest() > fastBeforeUndo
        && sameColor(view->thingModelVertexColorForTest(0), initialColor)
        && sameColor(view->thingModelVertexColorForTest(1), initialColor);
    check(compactUndoPublished,
        "ARCHLIGHTING History jump keeps pure compact Thing movement on the targeted fast path");

    const std::uint64_t rebuildBeforeRedo = view->meshRebuildCountForTest();
    const std::uint64_t resourceReadsBeforeRedo =
        view->model3doResourceReadCountForTest();
    const std::uint64_t fastBeforeRedo =
        view->thingTransformFastPublicationCountForTest();
    window.jumpToUndoDepthForTest(2);
    const Level& redone = window.documentStateForTest().level;
    const bool compactRedoPublished =
        redone.things.size() == 2
        && redone.things[0].x == 4.0
        && redone.things[1].x == 8.0
        && view->meshRebuildCountForTest() == rebuildBeforeRedo
        && view->model3doResourceReadCountForTest() == resourceReadsBeforeRedo
        && view->thingTransformFastPublicationCountForTest() > fastBeforeRedo
        && sameColor(view->thingModelVertexColorForTest(0), initialColor)
        && sameColor(view->thingModelVertexColorForTest(1), initialColor);
    check(compactRedoPublished,
        "ARCHLIGHTING History jump keeps pure compact Thing redo on the targeted fast path");
}

void runMixedHistoryJumpTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(makeRenderState());

    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING mixed history fixture exposes the MainWindow GlView");
    if (!view)
    {
        return;
    }

    view->setShowTextures(false);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->primeModel3do("arch_render_fixture.3do", makeRenderModel());
    view->setLevel(&window.documentStateForTest().level);

    const ColorF initialColor{ 0.25f, 0.5f, 0.875f, 1.0f };
    const ColorF appliedColor{ 0.75f, 0.5f, 0.125f, 1.0f };
    check(sameColor(view->thingModelVertexColorForTest(0), initialColor),
        "ARCHLIGHTING mixed history fixture establishes its cached RGB baseline");
    if (!sameColor(view->thingModelVertexColorForTest(0), initialColor))
    {
        return;
    }

    window.pushThingSubsetUndoForTitleForTest({ 0 });
    window.mutableDocumentStateForTest().level.things[0].x = 4.0;
    window.pushUndoForTitleForTest(editor::DocumentDirty::Things);
    window.mutableDocumentStateForTest().level.motsArchLighting[0].meshes[0].vertices[0] =
        MotsArchLightVertex{ 0.5f, 0.75f, 0.5f, 0.125f };
    view->notifyThingAppearanceChanged();
    check(sameColor(view->thingModelVertexColorForTest(0), appliedColor),
        "ARCHLIGHTING mixed history fixture publishes the table edit baseline");
    if (!sameColor(view->thingModelVertexColorForTest(0), appliedColor))
    {
        return;
    }

    const std::uint64_t rebuildBeforeUndo = view->meshRebuildCountForTest();
    window.jumpToUndoDepthForTest(0);
    const Level& undone = window.documentStateForTest().level;
    const bool mixedUndoPublished =
        undone.things.size() == 1
        && undone.things[0].x == 0.0
        && sameColor(view->thingModelVertexColorForTest(0), initialColor)
        && view->meshRebuildCountForTest() > rebuildBeforeUndo;
    check(mixedUndoPublished,
        "ARCHLIGHTING mixed History jump retains appearance rebuild for a table transition");

    const std::uint64_t rebuildBeforeRedo = view->meshRebuildCountForTest();
    window.jumpToUndoDepthForTest(2);
    const Level& redone = window.documentStateForTest().level;
    const bool mixedRedoPublished =
        redone.things.size() == 1
        && redone.things[0].x == 4.0
        && sameColor(view->thingModelVertexColorForTest(0), appliedColor)
        && view->meshRebuildCountForTest() > rebuildBeforeRedo;
    check(mixedRedoPublished,
        "ARCHLIGHTING mixed History redo retains appearance rebuild for a table transition");
}

void runArchitecturalLightingMarkerColorRenderingTests()
{
    editor::DocumentState state = makeRenderState();
    state.level.motsArchLighting[0].meshes[0].vertices = {
        MotsArchLightVertex{
            .mono = 1.0f, .red = 0.80f, .green = 0.10f, .blue = 0.15f },
        MotsArchLightVertex{
            .mono = 1.0f, .red = 0.12f, .green = 0.72f, .blue = 0.20f },
        MotsArchLightVertex{
            .mono = 1.0f, .red = 0.20f, .green = 0.25f, .blue = 0.85f } };

    GlView view{};
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.resize(320, 320);
    view.setBackgroundClearColor(Qt::black);
    view.setShowGrid(false);
    view.setShowWireframe(false);
    view.setShowVertices(false);
    view.setShowSolid(false);
    view.setShowThings(false);
    view.setShowThingGizmos(false);
    view.setShowAxisWidget(false);
    view.setShowToastOverlays(false);
    view.setShowEditorCubes(false);
    view.setRenderPipeline(GlView::RenderPipeline::Editor);

    assets::ResourceLocator resources{};
    view.setResourceLocator(&resources, false);
    view.primeModel3do("arch_render_fixture.3do", makeRenderModel());
    view.setLevel(&state.level);
    view.setArchitecturalLightingEditingActive(true);
    view.setPickMode(GlView::PickMode::ArchitecturalLighting);
    view.setViewportPickingEnabled(true);
    view.setHoverPickEnabled(true);
    view.setSelectedThing(0);
    view.frameWorldBounds(
        QVector3D{ -0.25f, -0.25f, -0.1f },
        QVector3D{ 1.25f, 1.25f, 0.1f },
        false);
    view.show();
    QApplication::processEvents(QEventLoop::AllEvents);

    const auto vertices = view.architecturalLightingVerticesForThing(0);
    check(vertices.size() >= 2,
        "ARCHLIGHTING marker-color fixture publishes two source vertices");
    if (vertices.size() < 2)
    {
        view.hide();
        return;
    }

    const GlView::ArchitecturalLightingVertexIdentity selectedIdentity{
        vertices[0].thingIndex,
        vertices[0].lodIndex,
        vertices[0].meshIndex,
        vertices[0].vertexIndex };
    view.setArchitecturalLightingSelectedVertices(
        std::span<const GlView::ArchitecturalLightingVertexIdentity>(
            &selectedIdentity,
            1));

    const auto projectedPixel = [&view](
        const QImage& frame,
        const QVector3D& world) -> std::optional<QPoint>
    {
        const std::optional<QPointF> projected =
            view.projectWorldToViewport(world);
        if (!projected
            || projected->x() < 0.0
            || projected->x() >= static_cast<qreal>(view.width())
            || projected->y() < 0.0
            || projected->y() >= static_cast<qreal>(view.height())
            || frame.isNull())
        {
            return std::nullopt;
        }

        const double imageScaleX = static_cast<double>(frame.width())
            / std::max(1, view.width());
        const double imageScaleY = static_cast<double>(frame.height())
            / std::max(1, view.height());
        return QPoint(
            std::clamp(
                static_cast<int>(std::lround(projected->x() * imageScaleX)),
                0,
                frame.width() - 1),
            std::clamp(
                static_cast<int>(std::lround(projected->y() * imageScaleY)),
                0,
                frame.height() - 1));
    };

    const auto sampleCenter = [&projectedPixel](
        const QImage& frame,
        const QVector3D& world) -> std::optional<QColor>
    {
        const std::optional<QPoint> pixel = projectedPixel(frame, world);
        return pixel ? std::optional<QColor>{ frame.pixelColor(*pixel) }
                     : std::nullopt;
    };

    const auto printSampledColor = [](
        const char* const label,
        const std::optional<QColor>& color)
    {
        std::cerr << label << '=';
        if (!color)
        {
            std::cerr << "<missing>";
            return;
        }
        std::cerr << '(' << color->red() << ',' << color->green() << ','
            << color->blue() << ',' << color->alpha() << ')';
    };
    const auto reportSampledColorPairFailure =
        [&printSampledColor](
            const char* const checkName,
            const bool passed,
            const std::optional<QColor>& first,
            const std::optional<QColor>& second)
    {
        if (passed)
        {
            return;
        }
        std::cerr << "DETAIL: " << checkName << " ";
        printSampledColor("first", first);
        std::cerr << ' ';
        printSampledColor("second", second);
        std::cerr << '\n';
    };
    const auto reportSampledColorFailure =
        [&printSampledColor](
            const char* const checkName,
            const bool passed,
            const std::optional<QColor>& color)
    {
        if (passed)
        {
            return;
        }
        std::cerr << "DETAIL: " << checkName << ' ';
        printSampledColor("sample", color);
        std::cerr << '\n';
    };

    const auto countBrightOuterBand = [&view, &projectedPixel](
        const QImage& frame,
        const QVector3D& world) -> int
    {
        const std::optional<QPoint> center = projectedPixel(frame, world);
        if (!center)
        {
            return 0;
        }

        const float dpr = std::max(
            1.0f,
            static_cast<float>(view.devicePixelRatioF()));
        const float baseRadius = view.architecturalLightingMarkerSizePx()
            * dpr * 0.5f;
        const float innerRadius = baseRadius + 2.5f * dpr;
        const float outerRadius = baseRadius + 5.0f * dpr;
        const int extent = static_cast<int>(std::ceil(outerRadius));
        int brightPixels = 0;
        for (int y = -extent; y <= extent; ++y)
        {
            for (int x = -extent; x <= extent; ++x)
            {
                const float radius = std::sqrt(
                    static_cast<float>(x * x + y * y));
                if (radius < innerRadius || radius > outerRadius)
                {
                    continue;
                }
                const int px = center->x() + x;
                const int py = center->y() + y;
                if (px < 0 || px >= frame.width()
                    || py < 0 || py >= frame.height())
                {
                    continue;
                }
                const QColor color = frame.pixelColor(px, py);
                if (color.red() > 180 && color.green() > 180
                    && color.blue() > 180)
                {
                    ++brightPixels;
                }
            }
        }
        return brightPixels;
    };

    const auto captureFrame = [&view]() -> QImage
    {
        (void)view.grabFramebuffer();
        view.repaint();
        QApplication::processEvents(QEventLoop::AllEvents);
        return view.grabFramebuffer();
    };

    const auto hoverAt = [&view](const QVector3D& world) -> bool
    {
        const std::optional<QPointF> projected =
            view.projectWorldToViewport(world);
        if (!projected
            || projected->x() < 0.0
            || projected->x() >= static_cast<qreal>(view.width())
            || projected->y() < 0.0
            || projected->y() >= static_cast<qreal>(view.height()))
        {
            return false;
        }

        const QPoint point(qRound(projected->x()), qRound(projected->y()));
        QMouseEvent move(
            QEvent::MouseMove,
            QPointF(point),
            QPointF(point),
            Qt::NoButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(&view, &move);
        QApplication::processEvents(QEventLoop::AllEvents);
        return true;
    };

    const QImage authoredFrame = captureFrame();
    const std::optional<QColor> selectedAuthored = sampleCenter(
        authoredFrame,
        vertices[0].worldPosition);
    const std::optional<QColor> unselectedAuthored = sampleCenter(
        authoredFrame,
        vertices[1].worldPosition);
    const bool authoredColorsRetained = selectedAuthored
        && unselectedAuthored
        && selectedAuthored->red() > selectedAuthored->green() + 60
        && selectedAuthored->red() > selectedAuthored->blue() + 40
        && unselectedAuthored->green() > unselectedAuthored->red() + 60
        && unselectedAuthored->green() > unselectedAuthored->blue() + 40;
    reportSampledColorPairFailure(
        "authored marker centers",
        authoredColorsRetained,
        selectedAuthored,
        unselectedAuthored);
    check(authoredColorsRetained,
        "ARCHLIGHTING selected and unselected marker centers retain authored RGB colors");

    const bool hoveredSelectedProjected = hoverAt(
        vertices[0].worldPosition);
    const QImage hoveredSelectedFrame = captureFrame();
    const std::optional<QColor> hoveredSelectedAuthored = sampleCenter(
        hoveredSelectedFrame,
        vertices[0].worldPosition);
    const bool hoveredSelectedRetainsColor = hoveredSelectedProjected
        && hoveredSelectedAuthored
        && hoveredSelectedAuthored->red()
            > hoveredSelectedAuthored->green() + 60
        && hoveredSelectedAuthored->red()
            > hoveredSelectedAuthored->blue() + 40;
    reportSampledColorFailure(
        "hovered selected authored marker",
        hoveredSelectedRetainsColor,
        hoveredSelectedAuthored);
    check(hoveredSelectedRetainsColor,
        "ARCHLIGHTING hovered selected marker center retains authored RGB color");

    const bool hoveredUnselectedProjected = hoverAt(
        vertices[1].worldPosition);
    const QImage hoveredUnselectedFrame = captureFrame();
    const std::optional<QColor> hoveredUnselectedAuthored = sampleCenter(
        hoveredUnselectedFrame,
        vertices[1].worldPosition);
    const bool hoveredUnselectedRetainsColor = hoveredUnselectedProjected
        && hoveredUnselectedAuthored
        && hoveredUnselectedAuthored->green()
            > hoveredUnselectedAuthored->red() + 60
        && hoveredUnselectedAuthored->green()
            > hoveredUnselectedAuthored->blue() + 40;
    reportSampledColorFailure(
        "hovered unselected authored marker",
        hoveredUnselectedRetainsColor,
        hoveredUnselectedAuthored);
    check(hoveredUnselectedRetainsColor,
        "ARCHLIGHTING hovered unselected marker center retains authored RGB color");

    const int selectedBrightOuter = countBrightOuterBand(
        authoredFrame,
        vertices[0].worldPosition);
    const int unselectedBrightOuter = countBrightOuterBand(
        authoredFrame,
        vertices[1].worldPosition);
    check(selectedBrightOuter > unselectedBrightOuter,
        "ARCHLIGHTING selected marker remains distinguishable through its contrasting outer ring");

    view.setArchitecturalLightingMarkerUseAuthoredColor(false);
    view.setArchitecturalLightingMarkerColor(QColor(36, 156, 220));
    const QImage fallbackFrame = captureFrame();
    const std::optional<QColor> selectedFallback = sampleCenter(
        fallbackFrame,
        vertices[0].worldPosition);
    const std::optional<QColor> unselectedFallback = sampleCenter(
        fallbackFrame,
        vertices[1].worldPosition);
    const auto nearChannel = [](const int actual, const int expected)
    {
        return std::abs(actual - expected) <= 45;
    };
    const bool fallbackColorRetained = selectedFallback
        && unselectedFallback
        && nearChannel(selectedFallback->red(), 36)
        && nearChannel(selectedFallback->green(), 156)
        && nearChannel(selectedFallback->blue(), 220)
        && nearChannel(unselectedFallback->red(), 36)
        && nearChannel(unselectedFallback->green(), 156)
        && nearChannel(unselectedFallback->blue(), 220);
    reportSampledColorPairFailure(
        "fallback marker centers",
        fallbackColorRetained,
        selectedFallback,
        unselectedFallback);
    check(fallbackColorRetained,
        "ARCHLIGHTING selected marker retains the configured fallback color when authored RGB is disabled");

    const bool fallbackHoveredSelectedProjected = hoverAt(
        vertices[0].worldPosition);
    const QImage fallbackHoveredSelectedFrame = captureFrame();
    const std::optional<QColor> fallbackHoveredSelected = sampleCenter(
        fallbackHoveredSelectedFrame,
        vertices[0].worldPosition);
    const bool fallbackHoveredSelectedRetained =
        fallbackHoveredSelectedProjected
        && fallbackHoveredSelected
        && nearChannel(fallbackHoveredSelected->red(), 36)
        && nearChannel(fallbackHoveredSelected->green(), 156)
        && nearChannel(fallbackHoveredSelected->blue(), 220);
    reportSampledColorFailure(
        "hovered selected fallback marker",
        fallbackHoveredSelectedRetained,
        fallbackHoveredSelected);
    check(fallbackHoveredSelectedRetained,
        "ARCHLIGHTING hovered selected marker retains the configured fallback color");

    const bool fallbackHoveredUnselectedProjected = hoverAt(
        vertices[1].worldPosition);
    const QImage fallbackHoveredUnselectedFrame = captureFrame();
    const std::optional<QColor> fallbackHoveredUnselected = sampleCenter(
        fallbackHoveredUnselectedFrame,
        vertices[1].worldPosition);
    const bool fallbackHoveredUnselectedRetained =
        fallbackHoveredUnselectedProjected
        && fallbackHoveredUnselected
        && nearChannel(fallbackHoveredUnselected->red(), 36)
        && nearChannel(fallbackHoveredUnselected->green(), 156)
        && nearChannel(fallbackHoveredUnselected->blue(), 220);
    reportSampledColorFailure(
        "hovered unselected fallback marker",
        fallbackHoveredUnselectedRetained,
        fallbackHoveredUnselected);
    check(fallbackHoveredUnselectedRetained,
        "ARCHLIGHTING hovered unselected marker retains the configured fallback color");
    view.hide();
}

void runArchitecturalLightingMarkerPreferenceTests()
{
    constexpr auto kMarkerSizeSetting =
        "viewport/architectural_lighting_marker_size_px";
    constexpr auto kMarkerAuthoredColorSetting =
        "viewport/architectural_lighting_marker_use_authored_color";
    constexpr auto kMarkerColorSetting =
        "viewport/architectural_lighting_marker_color";
    QSettings settings = settingsStore();
    const std::array<const char*, 3> settingNames{
        kMarkerSizeSetting,
        kMarkerAuthoredColorSetting,
        kMarkerColorSetting };
    std::array<bool, settingNames.size()> hadSetting{};
    std::array<QVariant, settingNames.size()> savedValues{};
    for (std::size_t index = 0; index < settingNames.size(); ++index)
    {
        hadSetting[index] = settings.contains(settingNames[index]);
        savedValues[index] = settings.value(settingNames[index]);
        settings.remove(settingNames[index]);
    }
    settings.sync();

    {
        MainWindow defaultsWindow{};
        defaultsWindow.setAttribute(Qt::WA_DontShowOnScreen);
        GlView* const view = defaultsWindow.glViewForTest();
        const QColor expectedDefaultColor = QColor::fromRgb(255, 204, 51);
        const bool defaultsValid = view
            && std::abs(view->architecturalLightingMarkerSizePx() - 14.0f)
                < 1.0e-5f
            && view->architecturalLightingMarkerUseAuthoredColor()
            && view->architecturalLightingMarkerColor() == expectedDefaultColor;
        check(defaultsValid,
            "ARCHLIGHTING marker preferences use a visible authored-color default and contrasting fallback color");
        if (view)
        {
            view->setArchitecturalLightingMarkerSizePx(
                GlView::kArchitecturalLightingMarkerMinSizePx - 1.0f);
            check(std::abs(view->architecturalLightingMarkerSizePx()
                        - GlView::kArchitecturalLightingMarkerMinSizePx)
                    < 1.0e-5f,
                "ARCHLIGHTING marker size clamps to its documented lower bound");
            view->setArchitecturalLightingMarkerSizePx(
                GlView::kArchitecturalLightingMarkerMaxSizePx + 1.0f);
            check(std::abs(view->architecturalLightingMarkerSizePx()
                        - GlView::kArchitecturalLightingMarkerMaxSizePx)
                    < 1.0e-5f,
                "ARCHLIGHTING marker size clamps to its documented upper bound");
            check(view->architecturalLightingMarkerHitRadiusPx() >= 2.0f
                    && view->architecturalLightingMarkerHitRadiusPx()
                        <= view->architecturalLightingMarkerSizePx(),
                "ARCHLIGHTING marker hit radius stays bounded by the configured marker size");
            view->setArchitecturalLightingMarkerUseAuthoredColor(false);
            view->setArchitecturalLightingMarkerColor(QColor(17, 34, 68));
            check(!view->architecturalLightingMarkerUseAuthoredColor()
                    && view->architecturalLightingMarkerColor()
                        == QColor(17, 34, 68),
                "ARCHLIGHTING marker preferences select a configurable fallback color");
        }
    }

    settings.setValue(kMarkerSizeSetting, 19.0);
    settings.setValue(kMarkerAuthoredColorSetting, false);
    settings.setValue(kMarkerColorSetting, QStringLiteral("#2040c0"));
    settings.sync();
    {
        MainWindow persistedWindow{};
        persistedWindow.setAttribute(Qt::WA_DontShowOnScreen);
        GlView* const view = persistedWindow.glViewForTest();
        check(view
                && std::abs(view->architecturalLightingMarkerSizePx() - 19.0f)
                    < 1.0e-5f
                && !view->architecturalLightingMarkerUseAuthoredColor()
                && view->architecturalLightingMarkerColor()
                    == QColor(32, 64, 192),
            "ARCHLIGHTING marker preferences load persisted size and color mode");
    }

    bool inspectedControls = false;
    {
        MainWindow preferencesWindow{};
        preferencesWindow.setAttribute(Qt::WA_DontShowOnScreen);
        const ProjectType previousProjectKind =
            preferencesWindow.documentStateForTest().level.kind;
        preferencesWindow.setProjectTypeForTest(ProjectType::MOTS);
        QAction* const preferencesAction = preferencesWindow.findChild<QAction*>(
            QStringLiteral("edit.preferences"));
        QTimer::singleShot(0, &preferencesWindow, [&]()
            {
                auto* const dialog = qobject_cast<QDialog*>(
                    QApplication::activeModalWidget());
                auto* const size = dialog
                    ? dialog->findChild<QDoubleSpinBox*>(QStringLiteral(
                        "pref_architectural_lighting_marker_size_px"))
                    : nullptr;
                auto* const authored = dialog
                    ? dialog->findChild<QCheckBox*>(QStringLiteral(
                        "pref_architectural_lighting_marker_use_authored_color"))
                    : nullptr;
                auto* const fallback = dialog
                    ? dialog->findChild<QToolButton*>(QStringLiteral(
                        "pref_architectural_lighting_marker_color"))
                    : nullptr;
                inspectedControls = size && authored && fallback
                    && size->minimum()
                        == static_cast<double>(GlView::kArchitecturalLightingMarkerMinSizePx)
                    && size->maximum()
                        == static_cast<double>(GlView::kArchitecturalLightingMarkerMaxSizePx)
                    && size->suffix() == QStringLiteral(" px")
                    && authored->toolTip().contains(
                        QStringLiteral("authored"), Qt::CaseInsensitive)
                    && QColor(fallback->toolTip()).isValid()
                    && fallback->styleSheet().contains(
                        QStringLiteral("background:"), Qt::CaseInsensitive);
                if (size)
                {
                    size->setValue(21.0);
                }
                if (authored)
                {
                    authored->setChecked(true);
                }
                if (dialog)
                {
                    dialog->accept();
                }
            });
        if (preferencesAction)
        {
            preferencesAction->trigger();
        }
        else
        {
            check(false,
                "ARCHLIGHTING marker preference regression finds the Preferences action");
        }
        preferencesWindow.setProjectTypeForTest(previousProjectKind);
        check(inspectedControls,
            "ARCHLIGHTING marker preferences expose bounded size and authored-color controls");
    }

    settings.sync();
    check(std::abs(settings.value(kMarkerSizeSetting).toDouble() - 21.0) < 1.0e-6
            && settings.value(kMarkerAuthoredColorSetting).toBool(),
        "ARCHLIGHTING marker preference Apply persists size and authored-color mode");

    for (std::size_t index = 0; index < settingNames.size(); ++index)
    {
        if (hadSetting[index])
        {
            settings.setValue(settingNames[index], savedValues[index]);
        }
        else
        {
            settings.remove(settingNames[index]);
        }
    }
    settings.sync();
}

void runSavedSceneWireframeCapture()
{
    const char* const requestedInput = std::getenv("SED_WIRE_REPRO");
    if (!requestedInput || requestedInput[0] == '\0'
        || std::string_view(requestedInput) == "0"
        || std::string_view(requestedInput) == "false"
        || std::string_view(requestedInput) == "off")
    {
        return;
    }

    constexpr auto kDefaultInput =
        "cpp/build/ninja/src/app/Release/s1l3_astcore_wiv.sed";
    const auto resolveInput = [](const std::filesystem::path& requested)
        -> std::optional<std::filesystem::path>
    {
        std::error_code error{};
        if (requested.is_absolute())
        {
            return std::filesystem::is_regular_file(requested, error)
                ? std::optional<std::filesystem::path>{
                    std::filesystem::absolute(requested, error).lexically_normal() }
                : std::nullopt;
        }

        std::filesystem::path directory = std::filesystem::current_path(error);
        if (error)
        {
            return std::nullopt;
        }
        while (!directory.empty())
        {
            const std::filesystem::path candidate = directory / requested;
            error.clear();
            if (std::filesystem::is_regular_file(candidate, error))
            {
                return std::filesystem::absolute(candidate, error).lexically_normal();
            }
            const std::filesystem::path parent = directory.parent_path();
            if (parent == directory)
            {
                break;
            }
            directory = parent;
        }
        return std::nullopt;
    };

    const std::string_view inputValue(requestedInput);
    const std::optional<std::filesystem::path> primaryPath =
        inputValue == "1" || inputValue == "true" || inputValue == "on"
            ? resolveInput(std::filesystem::path(kDefaultInput))
            : resolveInput(std::filesystem::path(requestedInput));
    if (!primaryPath)
    {
        std::cerr << "SED_WIRE_REPRO could not resolve its input file\n";
        check(false, "SED_WIRE_REPRO resolves a saved SED input file");
        return;
    }

    // Run one exact path per invocation. The _wv variant has a project-specific
    // filename, so callers select it explicitly through SED_WIRE_REPRO.
    const std::vector<std::filesystem::path> inputPaths{ *primaryPath };
    std::error_code error{};
    const char* const requestedOutput = std::getenv("SED_WIRE_REPRO_OUTPUT");
    std::filesystem::path outputDirectory =
        requestedOutput && requestedOutput[0] != '\0'
            ? std::filesystem::path(requestedOutput)
            : primaryPath->parent_path() / "wire_repro_capture";
    if (outputDirectory.is_relative())
    {
        outputDirectory = std::filesystem::absolute(outputDirectory, error)
            .lexically_normal();
    }
    error.clear();
    std::filesystem::create_directories(outputDirectory, error);
    if (error)
    {
        std::cerr << "SED_WIRE_REPRO could not create "
                  << outputDirectory.string() << ": " << error.message() << '\n';
        check(false, "SED_WIRE_REPRO output directory is writable");
        return;
    }

    setAppSettingsIniPathOverrideForTest(
        outputDirectory / "wire_repro_sed.ini");
    struct ResetSettingsOverride
    {
        ~ResetSettingsOverride()
        {
            setAppSettingsIniPathOverrideForTest({});
        }
    } resetSettingsOverride{};

    std::ofstream summary(
        outputDirectory / "wire_repro_summary.txt",
        std::ios::out | std::ios::trunc);
    if (!summary)
    {
        check(false, "SED_WIRE_REPRO summary file is writable");
        return;
    }
    summary << "SED_WIRE_REPRO saved-scene framebuffer capture\n"
            << "Output: " << outputDirectory.string() << '\n';

    for (const std::filesystem::path& inputPath : inputPaths)
    {
        try
        {
            (void)assets::loadSedFile(inputPath);
        }
        catch (const std::exception& exception)
        {
            std::cerr << "SED_WIRE_REPRO load failed for "
                      << inputPath.string() << ": " << exception.what() << '\n';
            check(false, "SED_WIRE_REPRO input loads with assets::loadSedFile");
            continue;
        }

        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.setRecoveryDirectoryForTest(
            outputDirectory / "recovery" / inputPath.stem());
        window.resize(1800, 1300);
        window.openLevelFromFilenameForTest(
            QString::fromStdWString(inputPath.wstring()));
        if (window.currentPathForTest() != inputPath)
        {
            std::cerr << "SED_WIRE_REPRO MainWindow failed to open "
                      << inputPath.string() << '\n';
            check(false, "SED_WIRE_REPRO opens the exact SED through MainWindow");
            continue;
        }

        GlView* const view = window.glViewForTest();
        if (!view)
        {
            check(false, "SED_WIRE_REPRO MainWindow provides its viewport");
            continue;
        }
        view->setMinimumSize(1200, 900);
        window.resize(1800, 1300);
        window.show();
        QApplication::processEvents(QEventLoop::AllEvents);

        // MainWindow's native SED open restores the saved EDIT viewport.
        // Keep that camera and change only pitch and the requested draw modes.
        const GlView::RecoveryState savedState = view->recoveryState();
        const auto& orbit = savedState.editor.orbit;
        summary << "\nScene: " << inputPath.string()
                << "\nyaw=" << orbit.yawDeg
                << " pitch=" << orbit.pitchDeg
                << " roll=" << orbit.rollDeg
                << " projection="
                << static_cast<int>(savedState.editor.projectionType)
                << " viewport=" << view->width() << 'x' << view->height()
                << " dpr=" << view->devicePixelRatioF() << '\n';

        struct RenderCase
        {
            const char* name;
            bool showSolid;
            bool showTextures;
        };
        constexpr std::array renderCases{
            RenderCase{ "solid_wire", true, false },
            RenderCase{ "texture_wire", true, true },
        };
        const std::array cameraCases{
            std::pair{ "saved", 0.0f },
            std::pair{ "pitch_plus_1deg", 1.0f },
        };

        for (const RenderCase& renderCase : renderCases)
        {
            view->setShowSolid(renderCase.showSolid);
            view->setShowTextures(renderCase.showTextures);
            view->setShowWireframe(true);
            for (const auto& [cameraTag, pitchDelta] : cameraCases)
            {
                view->setOrbitViewImmediate(
                    orbit.yawDeg,
                    orbit.pitchDeg + pitchDelta,
                    orbit.rollDeg);
                view->update();
                view->repaint();
                QApplication::processEvents(QEventLoop::AllEvents);
                const QImage frame = view->grabFramebuffer();
                const GlView::RecoveryState capturedState = view->recoveryState();
                const auto& capturedOrbit = capturedState.editor.orbit;
                const std::filesystem::path imagePath = outputDirectory
                    / (inputPath.stem().string() + "_"
                        + renderCase.name + "_" + cameraTag + ".png");
                const bool saved = frame.save(
                    QString::fromStdWString(imagePath.wstring()), "PNG");
                check(saved, "SED_WIRE_REPRO saves a wireframe PNG");
                summary << "render=" << renderCase.name
                        << " camera=" << cameraTag
                        << " yaw=" << capturedOrbit.yawDeg
                        << " pitch=" << capturedOrbit.pitchDeg
                        << " wireframe=" << view->showWireframe()
                        << " solid=" << view->showSolid()
                        << " textures=" << view->showTextures()
                        << " image=" << imagePath.string()
                        << " physical_size=" << frame.width() << 'x'
                        << frame.height()
                        << " dpr=" << view->devicePixelRatioF() << '\n';
                std::cerr << "SED_WIRE_REPRO camera=" << cameraTag
                          << " render=" << renderCase.name
                          << " yaw=" << capturedOrbit.yawDeg
                          << " pitch=" << capturedOrbit.pitchDeg
                          << " wireframe=" << view->showWireframe()
                          << " solid=" << view->showSolid()
                          << " textures=" << view->showTextures()
                          << " image=" << imagePath.string()
                          << " size=" << frame.width() << 'x' << frame.height()
                          << " dpr=" << view->devicePixelRatioF() << '\n';
            }
        }
        window.close();
        QApplication::processEvents(QEventLoop::AllEvents);
    }
}

std::shared_ptr<const assets::Model3do> makeLodRenderModel(
    const std::size_t lodIndex)
{
    auto model = std::make_shared<assets::Model3do>();
    model->version = 2.1;
    model->geosetCount = 4u;
    assets::Model3doMesh mesh{};
    mesh.name = "lod_render_fixture";
    mesh.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    mesh.light = static_cast<std::int32_t>(TLightMode::Gouraud);
    for (std::size_t triangle = 0; triangle <= lodIndex; ++triangle)
    {
        const std::int32_t base = static_cast<std::int32_t>(mesh.vertices.size());
        const double scale = lodIndex == 3u ? 0.1 : 1.0;
        const double x = 0.1 * static_cast<double>(triangle) * scale;
        mesh.vertices.push_back({ x - 0.25 * scale, 0.0, -0.25 * scale, ColorWhite });
        mesh.vertices.push_back({ x + 0.25 * scale, 0.0, -0.25 * scale, ColorWhite });
        mesh.vertices.push_back({ x, 0.0, 0.25 * scale, ColorWhite });
        assets::Model3doFace face{};
        face.geo = static_cast<std::int32_t>(TGeoMode::Solid);
        face.light = static_cast<std::int32_t>(TLightMode::Gouraud);
        face.extraLight = ColorBlack;
        face.vertices = { base, base + 1, base + 2 };
        mesh.faces.push_back(std::move(face));
    }
    model->meshes.push_back(std::move(mesh));
    return model;
}

void runModelLodPlacementPreviewRegressionTests()
{
    Level level{};
    level.kind = ProjectType::MOTS;
    level.layers = { "LOD preview fixture" };
    level.header.lodDistances = { 2.0, 4.0, 6.0, 8.0 };

    assets::ResourceLocator resources{};
    GlView view{};
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.resize(320, 320);
    view.setResourceLocator(&resources, false);
    view.setShowGrid(false);
    view.setShowTextures(false);
    view.setShowThingGizmos(false);
    view.setShowAxisWidget(false);
    view.setShowToastOverlays(false);
    view.setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Perspective);
    for (std::size_t lod = 0; lod < 4u; ++lod)
    {
        view.primeModel3do(
            "lod_render_fixture.3do", makeLodRenderModel(lod), lod);
    }
    view.setLevel(&level);
    view.frameWorldBounds(
        QVector3D{ -0.5f, -0.5f, -0.5f },
        QVector3D{  0.5f,  0.5f,  0.5f }, false);
    view.setOrbitViewImmediate(0.0f, 0.0f, 0.0f);
    view.show();
    QApplication::processEvents(QEventLoop::AllEvents);
    const auto paint = [&view]()
    {
        check(!view.grabFramebuffer().isNull(),
            "3DO LOD placement preview renders a frame");
        QApplication::processEvents(QEventLoop::AllEvents);
    };

    Thing preview{};
    preview.name = "lod_preview";
    preview.layer = 0;
    preview.values.push_back(
        makeStringValue("model3do", "lod_render_fixture.3do"));
    view.setCameraOrbitDistance(5.9f);
    view.setPlacementPreviewThing(std::optional<Thing>{ preview });
    paint();
    check(view.thingTriangleCacheVertexCountForTest() == 3u,
        "3DO LOD-off placement preview uses HQ geometry");

    view.setThingModelLodEnabled(true);
    paint();
    check(view.placementPreviewRenderedThingCountForTest() == 1u
            && view.thingTriangleCacheVertexCountForTest() == 9u,
        "3DO LOD placement preview selects its current camera tier");

    level.header.lodDistances[2] = 5.5;
    view.update();
    paint();
    check(view.thingTriangleCacheVertexCountForTest() == 12u,
        "3DO LOD header edit changes a preview without persistent model Things");
    level.header.lodDistances[2] = 6.0;
    view.update();
    paint();
    check(view.thingTriangleCacheVertexCountForTest() == 9u,
        "3DO LOD restoring the header returns the preview to its prior tier");

    const std::uint64_t sameTierBuilds = view.meshRebuildCountForTest();
    const std::uint64_t sameTierFast =
        view.placementPreviewFastUpdateCountForTest();
    preview.y = 0.05;
    view.setPlacementPreviewThing(std::optional<Thing>{ preview });
    check(view.meshRebuildCountForTest() == sameTierBuilds
            && view.placementPreviewFastUpdateCountForTest()
                == sameTierFast + 1u
            && view.thingTriangleCacheVertexCountForTest() == 9u,
        "3DO LOD same-tier placement motion keeps the preview affine patch");

    preview.y = 0.2;
    view.setPlacementPreviewThing(std::optional<Thing>{ preview });
    paint();
    check(view.thingTriangleCacheVertexCountForTest() == 12u,
        "3DO LOD placement motion across a threshold changes preview geometry");

    level.header.lodDistances[2] = 7.0;
    view.update();
    paint();
    check(view.thingTriangleCacheVertexCountForTest() == 9u,
        "3DO LOD header edit refreshes a preview without persistent model Things");

    view.setCameraOrbitDistance(7.1f);
    paint();
    check(view.thingTriangleCacheVertexCountForTest() == 12u,
        "3DO LOD camera motion refreshes a preview without persistent model Things");
    view.hide();
}

void runModelLodViewportRegressionTests()
{
    Level level{};
    level.kind = ProjectType::MOTS;
    level.layers = { "LOD fixture" };
    level.header.lodDistances = { 2.0, 4.0, 6.0, 8.0 };
    const std::int32_t staticCube = sed::level::levelCreateSectorPrimitiveBoxAxes(
        level,
        sed::level::SectorPrimitive::Cube,
        TVector{ -1.0, -1.0, -1.0 },
        TVector{ 1.0, 1.0, 1.0 },
        0, 1, 2, 0);
    const std::int32_t removableStaticCube = sed::level::levelCreateSectorPrimitiveBoxAxes(
        level,
        sed::level::SectorPrimitive::Cube,
        TVector{ 3.0, -1.0, -1.0 },
        TVector{ 5.0, 1.0, 1.0 },
        0, 1, 2, 0);
    const bool hasDistinctBatchSurface = removableStaticCube == 1
        && level.sectors.size() == 2u
        && !level.sectors[1].surfaces.empty();
    if (hasDistinctBatchSurface)
    {
        // One independently removable double-sided face creates a distinct
        // Level TriBuild even though the rest of both cubes share one batch.
        level.sectors[1].surfaces[0].faceFlags |= FF_DoubleSided;
    }
    const bool hasStaticSurfaceUvs = staticCube == 0
        && !level.sectors.empty()
        && !level.sectors[0].surfaces.empty()
        && !level.sectors[0].surfaces[0].texVertices.empty();
    check(hasStaticSurfaceUvs,
        "3DO LOD fixture includes static Level geometry with editable UVs");
    check(hasDistinctBatchSurface,
        "3DO LOD fixture includes an independently removable Level batch");
    Thing thing{};
    thing.name = "lod_render_fixture";
    thing.layer = 0;
    thing.values.push_back(makeStringValue("model3do", "lod_render_fixture.3do"));
    thing.values.push_back(makeStringValue("collide", "3"));
    level.things.push_back(std::move(thing));

    assets::ResourceLocator resources{};
    GlView view{};
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.resize(320, 320);
    view.setResourceLocator(&resources, false);
    view.setShowGrid(false);
    view.setShowTextures(false);
    view.setShowThingCollisionShapes(true);
    view.setShowThingVisualBounds(true);
    view.setShowThingGizmos(false);
    view.setShowAxisWidget(false);
    view.setShowToastOverlays(false);
    view.setCameraProjectionType(app::ViewportCamera::ProjectionType::Perspective);
    for (std::size_t lod = 0; lod < 4u; ++lod)
    {
        view.primeModel3do("lod_render_fixture.3do", makeLodRenderModel(lod), lod);
    }
    view.setLevel(&level);
    view.frameWorldBounds(
        QVector3D{ -0.5f, -0.5f, -0.5f },
        QVector3D{  0.5f,  0.5f,  0.5f }, false);
    view.setOrbitViewImmediate(0.0f, 0.0f, 0.0f);
    view.show();
    QApplication::processEvents(QEventLoop::AllEvents);
    const auto paint = [&view]()
    {
        const QImage frame = view.grabFramebuffer();
        QApplication::processEvents(QEventLoop::AllEvents);
        check(!frame.isNull(), "3DO LOD viewport fixture renders a frame");
    };

    view.setCameraOrbitDistance(9.0f);
    paint();
    check(!view.thingModelLodEnabled()
            && view.thingRenderedLodIndexForTest(0) == 0u
            && view.thingTriangleCacheVertexCountForTest() == 3u
            && view.thingPickTriangleVertexCountForTest(0) == 3u,
        "3DO LOD remains HQ with the viewport option off at a far camera");
    const auto hqVisualBounds = view.thingVisualBoundsOverlayAabbForTest(0u, false);
    const auto hqCollisionExtent = view.thingCollisionOverlayExtentForTest(0u, false);
    check(hqVisualBounds
            && ((*hqVisualBounds)[3] - (*hqVisualBounds)[0]) > 0.4
            && hqCollisionExtent && *hqCollisionExtent > 0.3,
        "3DO LOD fixture exposes HQ visual bounds and face collision guides");
    const std::uint64_t offRebuildCount = view.meshRebuildCountForTest();
    view.setCameraOrbitDistance(10.0f);
    paint();
    check(view.meshRebuildCountForTest() == offRebuildCount,
        "3DO LOD off does not rebuild Thing meshes on camera motion");

    view.setThingModelLodEnabled(true);
    paint();
    check(view.thingRenderedLodIndexForTest(0) == 3u
            && view.thingTriangleCacheVertexCountForTest() == 12u
            && view.thingPickTriangleVertexCountForTest(0) == 12u,
        "3DO LOD on renders and picks the fourth geoset beyond the third threshold");
    check(view.thingVisualBoundsOverlayAabbForTest(0u, false) == hqVisualBounds
            && view.thingCollisionOverlayExtentForTest(0u, false) == hqCollisionExtent,
        "3DO LOD rendering preserves HQ visual bounds and face collision guides");
    const std::uint64_t readsBeforeToggleCycle =
        view.model3doResourceReadCountForTest();
    view.setThingModelLodEnabled(false);
    paint();
    check(view.thingRenderedLodIndexForTest(0) == 0u
            && view.thingPickTriangleVertexCountForTest(0) == 3u,
        "disabling 3DO LOD restores visible HQ geometry and picking");
    view.setThingModelLodEnabled(true);
    paint();
    check(view.thingRenderedLodIndexForTest(0) == 3u
            && view.thingPickTriangleVertexCountForTest(0) == 12u
            && view.model3doResourceReadCountForTest()
                == readsBeforeToggleCycle,
        "reenabling 3DO LOD reuses the same parsed model revision");
    view.setCameraOrbitDistance(5.9f);
    paint();
    check(view.thingRenderedLodIndexForTest(0) == 2u
            && view.thingTriangleCacheVertexCountForTest() == 9u,
        "3DO LOD selects the third geoset below the third threshold");
    view.setCameraOrbitDistance(6.0f);
    paint();
    check(view.thingRenderedLodIndexForTest(0) == 3u
            && view.thingTriangleCacheVertexCountForTest() == 12u,
        "3DO LOD equality selects the farther geoset");

    const std::uint64_t rebuildsBeforeStaticUvPatch = view.meshRebuildCountForTest();
    const std::uint64_t uvPublicationsBeforeStaticUvPatch =
        view.surfaceUvFastPublicationCountForTest();
    if (hasStaticSurfaceUvs)
    {
        level.sectors[0].surfaces[0].texVertices[0].u += 0.125f;
        view.notifySurfaceUvsChanged({ SurfaceRef{ 0, 0 } });
    }
    check(hasStaticSurfaceUvs
            && view.meshRebuildCountForTest() == rebuildsBeforeStaticUvPatch
            && view.surfaceUvFastPublicationCountForTest()
                == uvPublicationsBeforeStaticUvPatch + 1u,
        "LOD tier rebuild retains static Level surface-UV ranges for fast UV editing");

    view.setShowThingCollisionShapes(false);
    view.setShowThingVisualBounds(false);
    bool changedLevelBatchMappingSurvives = false;
    if (hasDistinctBatchSurface && level.sectors.size() == 2u)
    {
        const std::size_t levelBatchCountBefore =
            view.levelTriangleCacheBatchCountForTest();
        const std::uint64_t beforeLevelOnlyRebuild = view.meshRebuildCountForTest();
        level.sectors.pop_back();
        view.notifyLevelGeometryChanged();
        const std::size_t levelBatchCountAfter =
            view.levelTriangleCacheBatchCountForTest();
        const std::uint64_t afterLevelOnlyRebuild = view.meshRebuildCountForTest();
        const std::uint64_t beforeMappingMoveRebuild = view.meshRebuildCountForTest();
        const std::uint64_t beforeMappingMoveFast =
            view.thingTransformFastPublicationCountForTest();
        level.things[0].y = 0.05;
        view.notifyThingTransformsChanged({ 0 });
        changedLevelBatchMappingSurvives =
            levelBatchCountAfter < levelBatchCountBefore
            && afterLevelOnlyRebuild == beforeLevelOnlyRebuild + 1u
            && view.meshRebuildCountForTest() == beforeMappingMoveRebuild
            && view.thingTransformFastPublicationCountForTest()
                == beforeMappingMoveFast + 1u
            && view.thingTriangleCacheVertexCountForTest() == 12u
            && view.thingPickTriangleVertexCountForTest(0) == 12u;
    }
    check(changedLevelBatchMappingSurvives,
        "Level-only batch-count change retains Thing rendering, picking, and the affine fast path");

    view.setCameraOrbitDistance(5.9f);
    paint();
    check(view.thingRenderedLodIndexForTest(0) == 2u
            && view.thingTriangleCacheVertexCountForTest() == 9u
            && view.thingPickTriangleVertexCountForTest(0) == 9u,
        "repeated tier crossing after a Level-only rebuild keeps Thing picking aligned");
    view.setCameraOrbitDistance(6.0f);
    paint();
    check(view.thingRenderedLodIndexForTest(0) == 3u
            && view.thingTriangleCacheVertexCountForTest() == 12u
            && view.thingPickTriangleVertexCountForTest(0) == 12u,
        "returning across the tier boundary preserves Thing picking after Level batch changes");

    const std::uint64_t sameTierRebuildCount = view.meshRebuildCountForTest();
    const std::uint64_t sameTierResourceReads =
        view.model3doResourceReadCountForTest();
    view.setCameraOrbitDistance(6.5f);
    paint();
    check(view.meshRebuildCountForTest() == sameTierRebuildCount
            && view.model3doResourceReadCountForTest()
                == sameTierResourceReads,
        "3DO LOD same-tier camera motion does not rebuild or reread a model");

    level.header.lodDistances[2] = 7.0;
    view.update();
    paint();
    check(view.thingRenderedLodIndexForTest(0) == 2u
            && view.thingTriangleCacheVertexCountForTest() == 9u,
        "3DO LOD fixed-camera Level Settings edit refreshes the rendered geoset");

    const std::uint64_t beforeSameTierMove = view.meshRebuildCountForTest();
    const std::uint64_t beforeFastMove = view.thingTransformFastPublicationCountForTest();
    level.things[0].y = 0.1;
    view.notifyThingTransformsChanged({ 0 });
    check(view.meshRebuildCountForTest() == beforeSameTierMove
            && view.thingTransformFastPublicationCountForTest() == beforeFastMove + 1u
            && view.thingRenderedLodIndexForTest(0) == 2u,
        "3DO LOD same-tier Thing movement keeps the affine transform fast path");
    level.things[0].y = 1.0;
    view.notifyThingTransformsChanged({ 0 });
    check(view.meshRebuildCountForTest() == beforeSameTierMove + 1u
            && view.thingRenderedLodIndexForTest(0) == 3u
            && view.thingTriangleCacheVertexCountForTest() == 12u,
        "3DO LOD Thing movement crossing a threshold switches its rendered mesh");

    // Placement can append a Thing before the pending camera repaint. The
    // existing prefix must not retain geometry from the old camera tier.
    view.setCameraOrbitDistance(2.0f);
    level.things.push_back(level.things.front());
    level.things.back().x = 1.0;
    view.notifyThingsAppended(1u);
    check(view.thingRenderedLodIndexForTest(0) == 1u
            && view.thingRenderedLodIndexForTest(1) == 1u
            && view.thingPickTriangleVertexCountForTest(0) == 6u
            && view.thingPickTriangleVertexCountForTest(1) == 6u,
        "3DO LOD append after camera motion refreshes the existing Thing prefix");
    paint();

    level.kind = ProjectType::IJIM;
    view.setLevel(&level, GlView::SetLevelMode::keepView);
    paint();
    check(view.thingTriangleCacheVertexCountForTest() == 6u
            && view.thingRenderedLodIndexForTest(0) == 0u
            && view.thingRenderedLodIndexForTest(1) == 0u,
        "3DO LOD option never changes IJIM model rendering");
    // The original classic renderer draws 0x8 architecture models in its
    // sector pass and applies model LOD only to ordinary Things.
    level.things[0].values.push_back(makeStringValue("thingflags", "8"));
    for (const ProjectType kind : { ProjectType::MOTS, ProjectType::JKDF2 })
    {
        level.kind = kind;
        view.setLevel(&level, GlView::SetLevelMode::keepView);
        view.setCameraOrbitDistance(10.0f);
        paint();
        check(view.thingRenderedLodIndexForTest(0) == 0u
                && view.thingPickTriangleVertexCountForTest(0) == 3u
                && view.thingRenderedLodIndexForTest(1) == 3u
                && view.thingPickTriangleVertexCountForTest(1) == 12u,
            "classic Arch Thing stays HQ while ordinary Thing uses far LOD");
    }

    // A one-geoset source cannot cross tiers, so camera samples stay on the
    // existing mesh without a resource lookup or rebuild.
    auto singleGeoset = std::make_shared<assets::Model3do>(
        *makeLodRenderModel(0u));
    singleGeoset->geosetCount = 1u;
    view.primeModel3do("lod_render_fixture.3do", singleGeoset);
    view.setLevel(&level, GlView::SetLevelMode::keepView);
    paint();
    const std::uint64_t singleGeosetRebuilds = view.meshRebuildCountForTest();
    const std::uint64_t singleGeosetReads =
        view.model3doResourceReadCountForTest();
    view.setCameraOrbitDistance(3.0f);
    paint();
    view.setCameraOrbitDistance(10.0f);
    paint();
    check(view.meshRebuildCountForTest() == singleGeosetRebuilds
            && view.model3doResourceReadCountForTest() == singleGeosetReads
            && view.thingPickTriangleVertexCountForTest(0) == 3u
            && view.thingPickTriangleVertexCountForTest(1) == 3u,
        "one-geoset Things avoid camera rebuilds and resource reads");

    // Publishing a new HQ source evicts every old lower variant. Until a
    // matching lower geoset is available, visible geometry and picking use
    // HQ; repeated paints must not retry the unavailable variant or rebuild.
    view.primeModel3do("lod_render_fixture.3do", makeLodRenderModel(0u));
    view.setLevel(&level, GlView::SetLevelMode::keepView);
    paint();
    check(view.thingRenderedLodIndexForTest(1) == 0u
            && view.thingPickTriangleVertexCountForTest(1) == 3u,
        "missing lower geoset falls back to the newly published HQ mesh");
    const std::uint64_t fallbackRebuilds = view.meshRebuildCountForTest();
    const std::uint64_t fallbackReads =
        view.model3doResourceReadCountForTest();
    paint();
    paint();
    check(view.meshRebuildCountForTest() == fallbackRebuilds
            && view.model3doResourceReadCountForTest() == fallbackReads,
        "missing lower geoset is negative-cached without a repaint rebuild loop");

    view.primeModel3do(
        "lod_render_fixture.3do", makeLodRenderModel(3u), 3u);
    view.setLevel(&level, GlView::SetLevelMode::keepView);
    paint();
    check(view.thingRenderedLodIndexForTest(1) == 3u
            && view.thingPickTriangleVertexCountForTest(1) == 12u,
        "publishing the matching lower geoset retries after HQ replacement");

    view.hide();
}

} // namespace

void runMotsArchLightingRenderTests()
{
    const char* const wireRepro = std::getenv("SED_WIRE_REPRO");
    if (wireRepro && wireRepro[0] != '\0'
        && std::string_view(wireRepro) != "0"
        && std::string_view(wireRepro) != "false"
        && std::string_view(wireRepro) != "off")
    {
        runSavedSceneWireframeCapture();
        return;
    }

    runModelLodPlacementPreviewRegressionTests();
    runModelLodViewportRegressionTests();
    runSyntheticPortalWireframeOcclusionTest();
    runOpaquePolygonBoundaryWireframeContinuityRegressionTest();
    runPerspectiveWireframeSupportDepthFramebufferRegressionTest();
    runWireframeSupportDepthOcclusionRegressionTest();
    runNormalModelLightingModeTests();
    runNormalModelFramebufferTest();
    runSelectedVertexAccessorTests();
    runArchitecturalLightingSourceMeshMappingTests();
    runArchitecturalLightingPreviewIdentityTests();
    runArchitecturalLightingMultiOwnerPreviewTests();
    runArchitecturalLightingLifecycleTests();
    runCachedTablePublicationTests();
    runAppendHistoryJumpTests();
    runCompactHistoryJumpTests();
    runMixedHistoryJumpTests();
    runArchitecturalLightingMarkerColorRenderingTests();
    runArchitecturalLightingMarkerPreferenceTests();
}

} // namespace sed::app_tests
