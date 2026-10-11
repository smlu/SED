#include "test_common.hpp"

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "render/gizmos/light_category_gizmo_utils.hpp"
#include "render/picking/depth_readback_utils.hpp"
#include "render/gizmos/gizmo_icon_utils.hpp"
#include "sed/assets/3do.hpp"
#include "sed/level/build.hpp"
#include "sed/level/arch_lighting.hpp"

#include "ui/recovery/document_recovery_utils.hpp"

#include <QApplication>
#include <QImage>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QDialog>
#include <QMetaObject>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QTimer>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

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

MotsArchLightObject makeArchObject()
{
    MotsArchLightMesh mesh{};
    mesh.wireIndex = 0;
    mesh.vertices = {
        MotsArchLightVertex{ 0.10f, 0.20f, 0.30f, 0.40f },
        MotsArchLightVertex{ 0.20f, 0.30f, 0.40f, 0.50f },
        MotsArchLightVertex{ 0.30f, 0.40f, 0.50f, 0.60f },
    };

    MotsArchLightObject object{};
    object.wireIndex = 0;
    object.meshes.push_back(std::move(mesh));
    return object;
}

MotsArchLightObject makeClosedDoorArchObject()
{
    MotsArchLightMesh mesh{};
    mesh.wireIndex = 0;
    mesh.vertices = {
        MotsArchLightVertex{ -1.0f, -0.6f, -1.0f, 0.20f },
        MotsArchLightVertex{  2.0f, -0.6f, -1.0f, 0.30f },
        MotsArchLightVertex{  2.0f,  1.2f, -1.0f, 0.40f },
        MotsArchLightVertex{ -1.0f,  1.2f, -1.0f, 0.50f },
        MotsArchLightVertex{ -1.0f, -0.6f,  3.0f, 0.60f },
        MotsArchLightVertex{  2.0f, -0.6f,  3.0f, 0.70f },
        MotsArchLightVertex{  2.0f,  1.2f,  3.0f, 0.80f },
        MotsArchLightVertex{ -1.0f,  1.2f,  3.0f, 0.90f },
    };

    MotsArchLightObject object{};
    object.wireIndex = 0;
    object.meshes.push_back(std::move(mesh));
    return object;
}

std::optional<std::pair<double, double>> surfaceZBounds(
    const Sector& sector,
    const Surface& surface)
{
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    bool haveVertex = false;
    for (const std::int32_t vertexIndex : surface.vertices)
    {
        if (vertexIndex < 0
            || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
        {
            continue;
        }
        const double z = sector.vertices[static_cast<std::size_t>(vertexIndex)].z;
        minimum = std::min(minimum, z);
        maximum = std::max(maximum, z);
        haveVertex = true;
    }
    return haveVertex
        ? std::optional<std::pair<double, double>>{
              std::pair<double, double>{ minimum, maximum }}
        : std::nullopt;
}

bool waitForEditorViewRefitToSettle(MainWindow& window, const int timeoutMs = 2000)
{
    QElapsedTimer timeout{};
    timeout.start();
    while (window.pendingEditorViewRefitForTest()
        && timeout.elapsed() < timeoutMs)
    {
        QEventLoop eventLoop{};
        QTimer::singleShot(5, &eventLoop, &QEventLoop::quit);
        eventLoop.exec(QEventLoop::AllEvents);
    }
    return !window.pendingEditorViewRefitForTest();
}

std::shared_ptr<const assets::Model3do> makePickingModel()
{
    auto model = std::make_shared<assets::Model3do>();
    model->version = 2.1;

    assets::Model3doMesh mesh{};
    mesh.name = "arch_picking_fixture";
    mesh.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    mesh.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    mesh.vertices = {
        assets::Model3doVertex{ 0.0, 0.0, 0.0, ColorWhite },
        assets::Model3doVertex{ 0.25, 0.0, 0.0, ColorWhite },
        assets::Model3doVertex{ 0.0, 0.25, 0.0, ColorWhite },
    };

    assets::Model3doFace face{};
    face.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    face.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    face.faceFlags |= FF_DoubleSided;
    face.vertices = { 0, 1, 2 };
    mesh.faces.push_back(std::move(face));
    model->meshes.push_back(std::move(mesh));
    return model;
}

std::vector<std::byte> pickingModelFixtureBytes()
{
    // MainWindow's interactive popup resolves topology through its
    // ResourceLocator, while the viewport itself uses the primed in-memory
    // closed-door model. Serialize the same eight-vertex mesh identity here
    // so the test exercises that publication boundary without publishing a
    // contradictory two-vertex topology.
    constexpr std::string_view model =
        "SECTION: HEADER\n"
        "3DO 2.3\n"
        "SECTION: GEOMETRYDEF\n"
        "RADIUS 1\n"
        "INSERT OFFSET 0 0 0\n"
        "GEOSETS 1\n"
        "GEOSET 0\n"
        "MESHES 1\n"
        "MESH 0\n"
        "NAME arch_picking_fixture\n"
        "VERTICES 8\n"
        "0: -1 -0.6 -1\n"
        "1: 2 -0.6 -1\n"
        "2: 2 1.2 -1\n"
        "3: -1 1.2 -1\n"
        "4: -1 -0.6 3\n"
        "5: 2 -0.6 3\n"
        "6: 2 1.2 3\n"
        "7: -1 1.2 3\n"
        "FACES 0\n"
        "SECTION: HIERARCHYDEF\n"
        "HIERARCHY NODES 1\n"
        "0: 0x0 0x1 0 -1 -1 -1 0 0 0 0 0 0 0 0 0 0 arch_picking_fixture\n";
    std::vector<std::byte> bytes{};
    bytes.reserve(model.size());
    for (const unsigned char character : model)
    {
        bytes.push_back(static_cast<std::byte>(character));
    }
    return bytes;
}

std::shared_ptr<const assets::Model3do> makeClosedDoorModel()
{
    auto model = std::make_shared<assets::Model3do>();
    model->version = 2.1;

    assets::Model3doMesh mesh{};
    mesh.name = "arch_picking_fixture";
    mesh.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    mesh.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    mesh.vertices = {
        assets::Model3doVertex{ -1.0, -0.6, -1.0, ColorWhite },
        assets::Model3doVertex{  2.0, -0.6, -1.0, ColorWhite },
        assets::Model3doVertex{  2.0,  1.2, -1.0, ColorWhite },
        assets::Model3doVertex{ -1.0,  1.2, -1.0, ColorWhite },
        assets::Model3doVertex{ -1.0, -0.6,  3.0, ColorWhite },
        assets::Model3doVertex{  2.0, -0.6,  3.0, ColorWhite },
        assets::Model3doVertex{  2.0,  1.2,  3.0, ColorWhite },
        assets::Model3doVertex{ -1.0,  1.2,  3.0, ColorWhite },
    };

    const std::array<std::array<std::int32_t, 3>, 12> triangles{
        std::array<std::int32_t, 3>{ 0, 2, 1 },
        std::array<std::int32_t, 3>{ 0, 3, 2 },
        std::array<std::int32_t, 3>{ 4, 5, 6 },
        std::array<std::int32_t, 3>{ 4, 6, 7 },
        std::array<std::int32_t, 3>{ 0, 1, 5 },
        std::array<std::int32_t, 3>{ 0, 5, 4 },
        std::array<std::int32_t, 3>{ 1, 2, 6 },
        std::array<std::int32_t, 3>{ 1, 6, 5 },
        std::array<std::int32_t, 3>{ 2, 3, 7 },
        std::array<std::int32_t, 3>{ 2, 7, 6 },
        std::array<std::int32_t, 3>{ 3, 0, 4 },
        std::array<std::int32_t, 3>{ 3, 4, 7 },
    };
    for (const auto& triangle : triangles)
    {
        assets::Model3doFace face{};
        face.geo = static_cast<std::int32_t>(TGeoMode::Solid);
        face.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
        face.faceFlags |= FF_DoubleSided;
        face.vertices.assign(triangle.begin(), triangle.end());
        mesh.faces.push_back(std::move(face));
    }
    model->meshes.push_back(std::move(mesh));
    return model;
}

std::shared_ptr<const assets::Model3do> makeSixBySixDoorModel()
{
    // The original d6x6 door is a narrow model (approximately +/-0.3 in X/Z
    // and +/-0.0125 in Y). Keep that aspect ratio in the synthetic fixture so
    // direct body/icon picking is exercised at the same projected footprint.
    auto model = std::make_shared<assets::Model3do>(*makeClosedDoorModel());
    for (assets::Model3doVertex& vertex : model->meshes.front().vertices)
    {
        vertex.x = (vertex.x - 0.5) * 0.2;
        vertex.y = (vertex.y - 0.3) * (0.0125 / 0.9);
        vertex.z = (vertex.z - 1.0) * 0.3;
    }
    return model;
}

std::shared_ptr<const assets::Model3do> makeSlopedPickingModel()
{
    auto model = std::make_shared<assets::Model3do>(*makePickingModel());
    model->meshes.front().vertices = {
        assets::Model3doVertex{ -0.23, -0.18, -0.45, ColorWhite },
        assets::Model3doVertex{  0.31, -0.16,  0.75, ColorWhite },
        assets::Model3doVertex{ -0.18,  0.33, -0.25, ColorWhite },
    };
    return model;
}

Level makePickingLevel(const QVector3D& thingPosition, const bool withWall)
{
    Level level{};
    level.kind = ProjectType::MOTS;
    level.layers.emplace_back("ARCH picking fixture");

    if (withWall)
    {
        Sector& sector = level::levelCreateCube(
            level,
            0.0,
            0.0,
            0.0,
            TVector::up,
            TVector::right);
        sector.layer = 0;
        const auto top = std::max_element(
            sector.surfaces.begin(),
            sector.surfaces.end(),
            [&sector](const Surface& left, const Surface& right)
            {
                const auto leftBounds = surfaceZBounds(sector, left);
                const auto rightBounds = surfaceZBounds(sector, right);
                return leftBounds.value_or(
                           std::pair<double, double>{
                               -std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity() })
                        .first
                    < rightBounds.value_or(
                          std::pair<double, double>{
                              -std::numeric_limits<double>::infinity(),
                              -std::numeric_limits<double>::infinity() })
                        .first;
            });
        if (top != sector.surfaces.end())
        {
            // The wall is an intentional two-sided occluder. This keeps the
            // depth contract identical in the CPU fallback and the GL pick
            // pass while the X-Ray case bypasses it explicitly.
            top->faceFlags |= FF_DoubleSided;
        }
    }

    Thing thing{};
    thing.name = "arch_picking_fixture";
    thing.templateName = thing.name;
    thing.layer = 0;
    thing.sector = withWall ? 0 : -1;
    thing.x = thingPosition.x();
    thing.y = thingPosition.y();
    thing.z = thingPosition.z();
    thing.archLightingIndex = 0;
    thing.values.push_back(makeStringValue(
        "model3do",
        "arch_picking_fixture.3do"));
    thing.values.push_back(makeIntegerValue("thingflags", 0x00000008));
    level.things.push_back(std::move(thing));
    level.motsArchLighting.push_back(makeArchObject());
    return level;
}

Level makeClosedDoorLevel()
{
    Level level = makePickingLevel(QVector3D{ 0.0f, 0.0f, 0.0f }, false);
    level.motsArchLighting.clear();
    level.motsArchLighting.push_back(makeClosedDoorArchObject());
    return level;
}

Level makeClosedDoorPairLevel()
{
    Level level = makeClosedDoorLevel();
    Thing second = level.things.front();
    second.x = 4.0;
    second.archLightingIndex = 1;
    level.things.push_back(std::move(second));
    level.motsArchLighting.push_back(makeClosedDoorArchObject());
    return level;
}

MotsArchLightObject makeSixBySixDoorArchObject()
{
    // ARCHLIGHTING records contain authored channel values only. The narrow
    // d6x6-like geometry is supplied by makeSixBySixDoorModel(); keeping this
    // fixture's channel record separate avoids pretending that archive light
    // vertices also carry model positions.
    return makeClosedDoorArchObject();
}

Level makeSixBySixDoorLevel()
{
    Level level = makePickingLevel(QVector3D{ 0.0f, 0.0f, 0.0f }, false);
    level.motsArchLighting.clear();
    level.motsArchLighting.push_back(makeSixBySixDoorArchObject());

    // A solid backdrop makes the depth comparison exercise a populated level
    // while leaving the narrow door's body in front of it and the raised icon
    // above it. The real maps commonly have sector geometry immediately behind
    // an Arch Thing door.
    Sector& backdrop = level.sectors.emplace_back();
    backdrop.layer = 0;
    backdrop.vertices = {
        Vertex{ -2.0, 0.25, -1.0 },
        Vertex{  2.0, 0.25, -1.0 },
        Vertex{  2.0, 0.25,  0.25 },
        Vertex{ -2.0, 0.25,  0.25 },
    };
    Surface surface{};
    surface.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    surface.faceFlags = FF_DoubleSided;
    surface.normal = TVector{ 0.0, -1.0, 0.0 };
    surface.vertices = { 0, 1, 2, 3 };
    surface.texVertices.resize(surface.vertices.size());
    backdrop.surfaces.push_back(std::move(surface));
    return level;
}

void configurePickingView(
    GlView& view,
    Level& level,
    assets::ResourceLocator& resources,
    const app::ViewportCamera::ProjectionType projection,
    const bool withWall)
{
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.resize(640, 480);
    view.setShowGrid(false);
    view.setShowWireframe(false);
    view.setShowVertices(false);
    view.setShowSolid(true);
    view.setShowTextures(false);
    view.setShowThings(true);
    view.setShowThingGizmos(false);
    view.setShowAxisWidget(false);
    view.setShowToastOverlays(false);
    view.setThingViewMode(GlView::ThingViewMode::Shaded);
    view.setRenderPipeline(GlView::RenderPipeline::Editor);
    view.setTransformTool(GlView::TransformTool::Select);
    // A standalone GlView has no MainWindow-owned locator. The renderer
    // requires a live locator even when the model is already primed, so keep
    // this object alive for the complete fixture lifetime and install it
    // before publishing the synthetic model.
    view.setResourceLocator(&resources, false);
    view.primeModel3do("arch_picking_fixture.3do", makePickingModel());
    view.setLevel(&level);
    view.setCameraProjectionType(projection);
    if (withWall)
    {
        view.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
        view.frameWorldBounds(
            QVector3D{ 0.0f, 0.0f, 0.0f },
            QVector3D{ 1.0f, 1.0f, 1.0f },
            false);
    }
    else
    {
        view.setOrbitViewImmediate(32.0f, 42.0f, 0.0f);
        view.frameWorldBounds(
            QVector3D{ -0.1f, -0.1f, -0.1f },
            QVector3D{ 0.4f, 0.4f, 0.1f },
            false);
    }
    view.setPickMode(GlView::PickMode::ArchitecturalLighting);
    view.setViewportPickingEnabled(true);
    view.setXraySelectionCyclingEnabled(false);
    view.setArchitecturalLightingEditingActive(true);
    view.setSelectedThing(0);
    view.show();
    QApplication::processEvents(QEventLoop::AllEvents);
}

struct PickedVertex
{
    std::int32_t thingIndex{ -1 };
    std::int32_t lodIndex{ -1 };
    std::int32_t meshIndex{ -1 };
    std::int32_t vertexIndex{ -1 };
};

std::optional<float> projectedWindowDepthForDiagnostic(
    const GlView& view,
    const QVector3D& worldPosition)
{
    const float nearPlane = view.cameraNearPlane();
    const float farPlane = view.cameraFarPlane();
    const float distance = QVector3D::dotProduct(
        worldPosition - view.cameraEye(),
        view.cameraForward());
    if (!std::isfinite(nearPlane) || !std::isfinite(farPlane)
        || !std::isfinite(distance) || farPlane <= nearPlane
        || distance <= 0.0f)
    {
        return std::nullopt;
    }

    if (view.cameraProjectionType()
        == app::ViewportCamera::ProjectionType::Perspective)
    {
        if (nearPlane <= 0.0f)
        {
            return std::nullopt;
        }
        return farPlane / (farPlane - nearPlane)
            - farPlane * nearPlane / ((farPlane - nearPlane) * distance);
    }
    return (distance - nearPlane) / (farPlane - nearPlane);
}

void printArchitecturalDepthDiagnostic(
    const char* label,
    GlView& view,
    const QPoint& clickPoint,
    const QVector3D& candidatePosition,
    const QVector3D& expectedScenePosition,
    const std::optional<QVector3D>& sampledSurfacePosition,
    const std::optional<PickedVertex>& picked)
{
    const std::optional<float> candidateDepth =
        projectedWindowDepthForDiagnostic(view, candidatePosition);
    const std::optional<float> expectedSceneDepth =
        projectedWindowDepthForDiagnostic(view, expectedScenePosition);
    const std::optional<float> sampledSceneDepth = sampledSurfacePosition
        ? projectedWindowDepthForDiagnostic(view, *sampledSurfacePosition)
        : std::nullopt;
    const std::optional<float> pickPassDepth =
        view.architecturalLightingSceneDepthForTest(QPointF(clickPoint));
    const QOpenGLContext* const context = view.context();
    const auto printDepth = [](const std::optional<float>& depth)
    {
        if (depth)
        {
            std::cerr << *depth;
        }
        else
        {
            std::cerr << "n/a";
        }
    };
    std::cerr << std::setprecision(10)
              << label << " click=(" << clickPoint.x() << ',' << clickPoint.y()
              << ") candidateWorld=(" << candidatePosition.x() << ','
              << candidatePosition.y() << ',' << candidatePosition.z() << ')'
              << " candidateDepth=";
    printDepth(candidateDepth);
    std::cerr << " expectedSceneWorld=(" << expectedScenePosition.x() << ','
              << expectedScenePosition.y() << ',' << expectedScenePosition.z() << ')'
              << " expectedSceneDepth=";
    printDepth(expectedSceneDepth);
    std::cerr << " sampledHit=" << sampledSurfacePosition.has_value();
    if (sampledSurfacePosition)
    {
        std::cerr << " sampledSceneWorld=(" << sampledSurfacePosition->x() << ','
                  << sampledSurfacePosition->y() << ','
                  << sampledSurfacePosition->z() << ") sampledSceneDepth=";
        printDepth(sampledSceneDepth);
    }
    std::cerr << " pickPassDepth=";
    printDepth(pickPassDepth);
    std::cerr << " cameraEye=(" << view.cameraEye().x() << ','
              << view.cameraEye().y() << ',' << view.cameraEye().z()
              << ") cameraForward=(" << view.cameraForward().x() << ','
              << view.cameraForward().y() << ',' << view.cameraForward().z()
              << ") viewport=" << view.width() << 'x' << view.height()
              << " dpr=" << view.devicePixelRatioF()
              << " glWidgetValid=" << view.isValid()
              << " glContextValid=" << (context && context->isValid())
              << " contextCurrent="
              << (QOpenGLContext::currentContext() == context)
              << " returnedVertex=";
    if (picked)
    {
        std::cerr << picked->thingIndex << ':' << picked->lodIndex << ':'
                  << picked->meshIndex << ':' << picked->vertexIndex;
    }
    else
    {
        std::cerr << "none";
    }
    std::cerr << '\n';
}

std::optional<std::int32_t> clickArchitecturalOwner(
    GlView& view,
    const QVector3D& worldPosition,
    const bool processEvents = true,
    const bool forceOwnerTarget = true,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier,
    QPoint* const releasePointForTest = nullptr)
{
    if (forceOwnerTarget)
    {
        view.setArchitecturalLightingTargetMode(
            GlView::ArchitecturalLightingTargetMode::Owner);
    }
    const std::optional<QPointF> projected =
        view.projectWorldToViewport(worldPosition);
    if (!projected)
    {
        return std::nullopt;
    }

    const QPoint point(
        qRound(projected->x()),
        qRound(projected->y()));
    if (releasePointForTest)
    {
        *releasePointForTest = point;
    }
    std::optional<std::int32_t> picked{};
    const QMetaObject::Connection connection = QObject::connect(
        &view,
        &GlView::clickPicked,
        [&picked](
            const std::int32_t,
            const std::int32_t,
            const std::int32_t thingIndex,
            const std::int32_t)
        {
            picked = thingIndex;
        });

    QMouseEvent press(
        QEvent::MouseButtonPress,
        QPointF(point),
        QPointF(point),
        Qt::LeftButton,
        Qt::LeftButton,
        modifiers);
    QApplication::sendEvent(&view, &press);
    QMouseEvent release(
        QEvent::MouseButtonRelease,
        QPointF(point),
        QPointF(point),
        Qt::LeftButton,
        Qt::NoButton,
        modifiers);
    QApplication::sendEvent(&view, &release);
    if (processEvents)
    {
        QApplication::processEvents(QEventLoop::AllEvents);
    }
    QObject::disconnect(connection);
    return picked;
}

std::optional<PickedVertex> clickArchitecturalVertex(
    GlView& view,
    const QVector3D& worldPosition,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier,
    const bool processEvents = true)
{
    view.setArchitecturalLightingTargetMode(
        GlView::ArchitecturalLightingTargetMode::Vertices);
    const std::optional<QPointF> projected =
        view.projectWorldToViewport(worldPosition);
    if (!projected)
    {
        return std::nullopt;
    }

    const QPoint point(
        qRound(projected->x()),
        qRound(projected->y()));
    std::optional<PickedVertex> picked{};
    const QMetaObject::Connection connection = QObject::connect(
        &view,
        &GlView::architecturalLightingVertexPicked,
        [&picked](
            const std::int32_t thingIndex,
            const std::int32_t lodIndex,
            const std::int32_t meshIndex,
            const std::int32_t vertexIndex,
            const Qt::KeyboardModifiers)
        {
            picked = PickedVertex{
                thingIndex,
                lodIndex,
                meshIndex,
                vertexIndex };
        });

    QMouseEvent press(
        QEvent::MouseButtonPress,
        QPointF(point),
        QPointF(point),
        Qt::LeftButton,
        Qt::LeftButton,
        modifiers);
    QApplication::sendEvent(&view, &press);
    QMouseEvent release(
        QEvent::MouseButtonRelease,
        QPointF(point),
        QPointF(point),
        Qt::LeftButton,
        Qt::NoButton,
        modifiers);
    QApplication::sendEvent(&view, &release);
    if (processEvents)
    {
        QApplication::processEvents(QEventLoop::AllEvents);
    }
    QObject::disconnect(connection);
    return picked;
}

void clickViewportPoint(
    GlView& view,
    const QPoint& point,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier,
    const bool processEvents = true)
{
    QMouseEvent press(
        QEvent::MouseButtonPress,
        QPointF(point),
        QPointF(point),
        Qt::LeftButton,
        Qt::LeftButton,
        modifiers);
    QApplication::sendEvent(&view, &press);
    QMouseEvent release(
        QEvent::MouseButtonRelease,
        QPointF(point),
        QPointF(point),
        Qt::LeftButton,
        Qt::NoButton,
        modifiers);
    QApplication::sendEvent(&view, &release);
    if (processEvents)
    {
        QApplication::processEvents(QEventLoop::AllEvents);
    }
}

void dragViewportRect(
    GlView& view,
    const QPoint& start,
    const QPoint& finish)
{
    QMouseEvent press(
        QEvent::MouseButtonPress,
        QPointF(start),
        QPointF(start),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(&view, &press);
    QMouseEvent move(
        QEvent::MouseMove,
        QPointF(finish),
        QPointF(finish),
        Qt::NoButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(&view, &move);
    QMouseEvent release(
        QEvent::MouseButtonRelease,
        QPointF(finish),
        QPointF(finish),
        Qt::LeftButton,
        Qt::NoButton,
        Qt::NoModifier);
    QApplication::sendEvent(&view, &release);
    QApplication::processEvents(QEventLoop::AllEvents);
}

void clearCurrentOpenGlContextForTest(GlView& view)
{
    // ARCH mouse release enters pickPeel directly. Keep this test honest about
    // the event-loop contract: a QOpenGLWidget event is allowed to arrive
    // while no context, or another widget's context, is current.
    view.doneCurrent();
    if (QOpenGLContext* current = QOpenGLContext::currentContext())
    {
        current->doneCurrent();
    }
}

void runArchitecturalLightingProjectionPickTests()
{
    for (const app::ViewportCamera::ProjectionType projection : {
             app::ViewportCamera::ProjectionType::Orthographic,
             app::ViewportCamera::ProjectionType::Perspective })
    {
        Level level = makePickingLevel(QVector3D{ 0.0f, 0.0f, 0.0f }, false);
        assets::ResourceLocator resources{};
        GlView view{};
        configurePickingView(view, level, resources, projection, false);

        const auto vertices = view.architecturalLightingVerticesForThing(0);
        const auto target = std::find_if(
            vertices.begin(),
            vertices.end(),
            [](const GlView::ArchitecturalLightingRenderVertex& vertex)
            {
                return vertex.meshIndex == 0 && vertex.vertexIndex == 1;
            });
        const std::optional<PickedVertex> picked = target != vertices.end()
            ? clickArchitecturalVertex(view, target->worldPosition)
            : std::nullopt;

        const bool identityMatches = picked
            && picked->thingIndex == 0
            && picked->lodIndex == 0
            && picked->meshIndex == 0
            && picked->vertexIndex == 1;
        check(
            target != vertices.end() && identityMatches,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic press/release picks the selected source mesh vertex"
                : "ARCHLIGHTING perspective press/release picks the selected source mesh vertex");
        view.hide();
    }
}

void runArchitecturalLightingThinWallPickTests()
{
    Level level = makePickingLevel(QVector3D{ 0.5f, 0.5f, 0.5f }, true);
    assets::ResourceLocator resources{};
    GlView view{};
    for (const app::ViewportCamera::ProjectionType projection : {
             app::ViewportCamera::ProjectionType::Orthographic,
             app::ViewportCamera::ProjectionType::Perspective })
    {
        configurePickingView(view, level, resources, projection, true);

        const auto vertices = view.architecturalLightingVerticesForThing(0);
        const auto target = std::find_if(
            vertices.begin(),
            vertices.end(),
            [](const GlView::ArchitecturalLightingRenderVertex& vertex)
            {
                return vertex.meshIndex == 0 && vertex.vertexIndex == 0;
            });
        check(target != vertices.end(),
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic thin-wall fixture publishes the interior source vertex"
                : "ARCHLIGHTING perspective thin-wall fixture publishes the interior source vertex");
        if (target == vertices.end())
        {
            continue;
        }

        const std::optional<QPointF> projected =
            view.projectWorldToViewport(target->worldPosition);
        const bool projectedOnscreen = projected
            && projected->x() >= 0.0
            && projected->x() < static_cast<qreal>(view.width())
            && projected->y() >= 0.0
            && projected->y() < static_cast<qreal>(view.height());
        check(projectedOnscreen,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic thin-wall vertex projects inside the viewport"
                : "ARCHLIGHTING perspective thin-wall vertex projects inside the viewport");

        const Sector& wallSector = level.sectors[0];
        const auto top = std::max_element(
            wallSector.surfaces.begin(),
            wallSector.surfaces.end(),
            [&wallSector](const Surface& left, const Surface& right)
            {
                const auto leftBounds = surfaceZBounds(wallSector, left);
                const auto rightBounds = surfaceZBounds(wallSector, right);
                return leftBounds.value_or(
                           std::pair<double, double>{
                               -std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity() })
                        .first
                    < rightBounds.value_or(
                          std::pair<double, double>{
                              -std::numeric_limits<double>::infinity(),
                              -std::numeric_limits<double>::infinity() })
                        .first;
            });
        const auto topBounds = top != wallSector.surfaces.end()
            ? surfaceZBounds(wallSector, *top)
            : std::nullopt;
        double cubeMinimumZ = std::numeric_limits<double>::infinity();
        double cubeMaximumZ = -std::numeric_limits<double>::infinity();
        for (const Vertex& vertex : wallSector.vertices)
        {
            cubeMinimumZ = std::min(cubeMinimumZ, vertex.z);
            cubeMaximumZ = std::max(cubeMaximumZ, vertex.z);
        }
        const bool cubeContainsTarget = cubeMinimumZ < target->worldPosition.z()
            && target->worldPosition.z() < cubeMaximumZ;
        check(cubeContainsTarget
                && cubeMinimumZ == 0.0
                && cubeMaximumZ == 1.0,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic cube fixture has unit Z bounds around the interior target"
                : "ARCHLIGHTING perspective cube fixture has unit Z bounds around the interior target");
        const bool topIsPlane = topBounds
            && std::abs(topBounds->second - topBounds->first) <= 1.0e-9;
        check(topIsPlane,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic occluder uses one maximum-Z face plane"
                : "ARCHLIGHTING perspective occluder uses one maximum-Z face plane");
        const bool targetBehindWall = topBounds
            && cubeContainsTarget
            && target->worldPosition.z()
                < static_cast<float>(topBounds->first);
        check(targetBehindWall,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic thin-wall target is explicitly behind the top occluder"
                : "ARCHLIGHTING perspective thin-wall target is explicitly behind the top occluder");

        const std::optional<PickedVertex> ordinary =
            clickArchitecturalVertex(view, target->worldPosition);
        check(!ordinary,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING ordinary orthographic picking rejects a vertex behind a solid thin wall"
                : "ARCHLIGHTING ordinary perspective picking rejects a vertex behind a solid thin wall");

        view.setXraySelectionCyclingEnabled(true);
        const std::optional<PickedVertex> xray =
            clickArchitecturalVertex(view, target->worldPosition);
        check(xray
                && xray->thingIndex == 0
                && xray->lodIndex == 0
                && xray->meshIndex == 0
                && xray->vertexIndex == 0,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING X-Ray orthographic picking accepts the interior source vertex"
                : "ARCHLIGHTING X-Ray perspective picking accepts the interior source vertex");
        view.setXraySelectionCyclingEnabled(false);
    }
    view.hide();
}

void runArchitecturalLightingOutsideBackfacePickTests()
{
    for (const app::ViewportCamera::ProjectionType projection : {
             app::ViewportCamera::ProjectionType::Orthographic,
             app::ViewportCamera::ProjectionType::Perspective })
    {
        const QVector3D target{ 0.05f, 0.05f, 1.5f };
        constexpr double blockerExtent = 20.0;
        Level level = makePickingLevel(QVector3D{ 0.0f, 0.0f, 1.5f }, false);
        Sector& backfaceSector = level.sectors.emplace_back();
        backfaceSector.layer = 0;
        backfaceSector.vertices = {
            Vertex{ -blockerExtent, -blockerExtent, 0.0 },
            Vertex{  blockerExtent, -blockerExtent, 0.0 },
            Vertex{  blockerExtent,  blockerExtent, 0.0 },
            Vertex{ -blockerExtent,  blockerExtent, 0.0 },
        };
        Surface backface{};
        backface.geo = static_cast<std::int32_t>(TGeoMode::Solid);
        backface.normal = TVector{ 0.0, 0.0, 1.0 };
        backface.vertices = { 0, 1, 2, 3 };
        backface.texVertices.resize(backface.vertices.size());
        backfaceSector.surfaces.push_back(std::move(backface));

        assets::ResourceLocator resources{};
        GlView view{};
        configurePickingView(view, level, resources, projection, false);
        view.setOrbitViewImmediate(0.0f, -35.0f, 0.0f);
        view.frameWorldBounds(
            QVector3D{ -1.2f, -1.2f, -0.2f },
            QVector3D{  1.2f,  1.2f,  1.8f },
            false);
        view.update();
        QApplication::processEvents(QEventLoop::AllEvents);

        const auto projected = view.projectWorldToViewport(target);
        const bool projectedOnscreen = projected
            && projected->x() >= 0.0
            && projected->x() < static_cast<qreal>(view.width())
            && projected->y() >= 0.0
            && projected->y() < static_cast<qreal>(view.height());
        check(projectedOnscreen,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic backface target projects inside the viewport"
                : "ARCHLIGHTING perspective backface target projects inside the viewport");

        const QPoint point = projected
            ? QPoint(qRound(projected->x()), qRound(projected->y()))
            : QPoint(-1, -1);
        const QVector3D eye = view.cameraEye();
        const QVector3D forward = view.cameraForward();
        const std::optional<QVector3D> blockerHit = projectedOnscreen
            ? view.viewportPointOnPlane(
                  point, QVector3D{ 0.0f, 0.0f, 0.0f }, QVector3D{ 0.0f, 0.0f, 1.0f })
            : std::nullopt;
        const float blockerDepth = blockerHit
            ? QVector3D::dotProduct(*blockerHit - eye, forward)
            : -1.0f;
        const float targetDepth = QVector3D::dotProduct(target - eye, forward);
        const bool rayCrossesBackfaceBeforeTarget = eye.z() < 0.0f
            && forward.z() > 0.0f
            && blockerHit
            && std::abs(blockerHit->z()) <= 1.0e-4f
            && std::abs(blockerHit->x()) < blockerExtent
            && std::abs(blockerHit->y()) < blockerExtent
            && blockerDepth > 0.0f
            && blockerDepth < targetDepth;
        check(rayCrossesBackfaceBeforeTarget,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic click ray crosses the culled +Z backface before the Thing"
                : "ARCHLIGHTING perspective click ray crosses the culled +Z backface before the Thing");

        // The CCW +Z level face is viewed from its back side, so ordinary
        // rendering culls it. Even though this exact click ray crosses the
        // face before reaching the Thing, ARCH picking must not let that
        // invisible backface act as an occluder.
        const std::optional<std::int32_t> picked = projectedOnscreen
            ? clickArchitecturalOwner(view, target)
            : std::nullopt;
        check(picked && *picked == 0,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic outside-sector click picks a Thing through a culled single-sided backface"
                : "ARCHLIGHTING perspective outside-sector click picks a Thing through a culled single-sided backface");

        const auto pickedVertex = clickArchitecturalVertex(
            view, QVector3D{ 0.0f, 0.0f, 1.5f });
        check(pickedVertex
                && pickedVertex->thingIndex == 0
                && pickedVertex->lodIndex == 0
                && pickedVertex->meshIndex == 0
                && pickedVertex->vertexIndex == 0,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic outside-sector click picks a model vertex through a culled single-sided backface"
                : "ARCHLIGHTING perspective outside-sector click picks a model vertex through a culled single-sided backface");

        view.hide();
    }
}

void runArchitecturalLightingOwnerGizmoRestoreTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(editor::DocumentState{
        makeClosedDoorLevel(),
        {} });
    window.setProjectTypeForTest(ProjectType::MOTS);
    window.resize(640, 480);
    window.show();
    QApplication::processEvents(QEventLoop::AllEvents);
    const bool initialViewRefitSettled = waitForEditorViewRefitToSettle(window);
    check(initialViewRefitSettled,
        "ARCHLIGHTING owner-gizmo fixture waits for the MainWindow startup camera refit");
    if (!initialViewRefitSettled)
    {
        window.hide();
        return;
    }


    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING owner-gizmo fixture exposes the MainWindow viewport");
    if (!view)
    {
        return;
    }

    view->setShowThings(true);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->setResourceLocator(&window.resourceLocatorForTest(), false);
    view->primeModel3do("arch_picking_fixture.3do", makeClosedDoorModel());
    view->setLevel(&window.mutableDocumentStateForTest().level);
    view->setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view->setOrbitViewImmediate(32.0f, 42.0f, 0.0f);
    view->frameWorldBounds(
        QVector3D{ -1.2f, -0.8f, -1.2f },
        QVector3D{  2.2f,  1.4f,  3.2f },
        false);

    view->setShowThingGizmos(false);
    window.activateArchitecturalLightingEditModeForTest();
    check(window.architecturalLightingEditModeActiveForTest()
            && view->architecturalLightingEditingActive()
            && view->architecturalLightingOwnerGizmosVisibleForTest()
            && !view->showThingGizmos(),
        "ARCHLIGHTING active mode forces owner gizmos while ordinary Thing gizmos remain disabled");

    view->setSelectedThing(-1);
    view->update();
    QApplication::processEvents(QEventLoop::AllEvents);
    const std::optional<QVector3D> displayAnchor =
        view->architecturalLightingOwnerGizmoDisplayPositionForTest(0);
    const std::optional<render::ThingBounds> ownerBounds =
        view->architecturalLightingOwnerGizmoBoundsForTest(0);
    const Thing& owner = window.documentStateForTest().level.things.front();
    const float markerHalfSize =
        std::clamp(view->lightGizmoWorldSize(), 0.001f, 1.0f) * 0.61f;
    const float modelTop = ownerBounds && ownerBounds->valid
        ? std::max(static_cast<float>(owner.z), ownerBounds->max.z())
        : std::numeric_limits<float>::quiet_NaN();
    const float expectedMarkerZ = modelTop + 0.002f + markerHalfSize
        + view->archThingGizmoOffsetAboveModel();
    const bool markerAtFixedWorldTop = displayAnchor && ownerBounds
        && ownerBounds->valid
        && std::isfinite(expectedMarkerZ)
        && close(displayAnchor->x(), owner.x, 1.0e-5)
        && close(displayAnchor->y(), owner.y, 1.0e-5)
        && close(displayAnchor->z(), expectedMarkerZ, 1.0e-5);
    check(displayAnchor.has_value()
            && view->architecturalLightingOwnerGizmoEntryCountForTest() >= 1
            && markerAtFixedWorldTop,
        "ARCHLIGHTING owner marker uses a fixed world-space model-top anchor");

    // A fresh ARCH viewport may still carry the legacy Vertices target from a
    // previous session. Unified input must resolve a displayed owner first,
    // publish that owner selection, and expand its cached vertices without
    // requiring a warm selection or a marquee pass.
    view->setArchitecturalLightingTargetMode(
        GlView::ArchitecturalLightingTargetMode::Vertices);
    editor::Selection coldSelection{};
    window.mutableDocumentStateForTest().selection = coldSelection;
    view->setSelectionState(-1, -1, -1, -1, coldSelection);
    view->setSelectedThing(-1);
    const std::optional<std::int32_t> coldOwner = displayAnchor
        ? clickArchitecturalOwner(*view, *displayAnchor, true, false)
        : std::nullopt;
    check(coldOwner && *coldOwner == 0
            && window.documentStateForTest().selection.things
                == std::unordered_set<std::int32_t>{ 0 }
            && view->architecturalLightingSelectedVertices().size() == 8,
        "ARCHLIGHTING unified cold owner click selects the owner and all vertices independent of the remembered target");
    view->setArchitecturalLightingTargetMode(
        GlView::ArchitecturalLightingTargetMode::Owner);

    const std::optional<std::int32_t> bodyOwner =
        clickArchitecturalOwner(*view, QVector3D{ 0.5f, 0.0f, 0.5f });
    check(bodyOwner && *bodyOwner == 0,
        "ARCHLIGHTING body click selects a visible bound owner whose origin is inside the closed model");

    // Check the exact projected marker center first. The nearby offset point
    // remains inside the marker footprint and lies on the same camera-facing
    // corner region, so both pixels should resolve the visible marker.
    // This is the camera-facing upper corner for the +X/-Y/+Z view. Using
    // vertex 0 here would test the hidden back corner, not a visible marker.
    const QVector3D frontCorner{ 2.0f, -0.6f, 3.0f };
    const QVector3D frontEdge{ 2.0f, -0.6f, -1.0f };
    const QVector3D topEdge{ 2.0f, 1.2f, 3.0f };
    const QVector3D nearCorner = frontCorner
        + (frontEdge - frontCorner) * 0.02f
        + (topEdge - frontCorner) * 0.02f;
    const std::optional<QPointF> cornerScreen =
        view->projectWorldToViewport(frontCorner);
    const std::optional<QPointF> nearCornerScreen =
        view->projectWorldToViewport(nearCorner);
    const bool nearCornerInsideMarker = cornerScreen && nearCornerScreen
        && std::hypot(
            nearCornerScreen->x() - cornerScreen->x(),
            nearCornerScreen->y() - cornerScreen->y())
            <= static_cast<qreal>(
                view->architecturalLightingMarkerHitRadiusPx());
    check(nearCornerInsideMarker,
        "ARCHLIGHTING closed-door body point remains within the displayed vertex marker radius");
    int vertexSignalsAtCorner = 0;
    const QMetaObject::Connection cornerConnection = QObject::connect(
        view,
        &GlView::architecturalLightingVertexPicked,
        [&vertexSignalsAtCorner](
            const std::int32_t,
            const std::int32_t,
            const std::int32_t,
            const std::int32_t,
            const Qt::KeyboardModifiers)
        {
            ++vertexSignalsAtCorner;
        });
    const std::optional<std::int32_t> cornerOwner =
        clickArchitecturalOwner(*view, frontCorner);
    QObject::disconnect(cornerConnection);
    const std::optional<float> cornerCandidateDepth =
        projectedWindowDepthForDiagnostic(*view, frontCorner);
    const QPoint cornerPoint = cornerScreen
        ? QPoint(qRound(cornerScreen->x()), qRound(cornerScreen->y()))
        : QPoint{};
    const std::optional<float> cornerSceneDepth =
        view->architecturalLightingSceneDepthForTest(QPointF(cornerPoint));
    if (!cornerOwner || *cornerOwner != 0 || vertexSignalsAtCorner != 1)
    {
        std::cerr << std::setprecision(10)
                  << "ARCHLIGHTING exact near-door marker failure: owner="
                  << (cornerOwner ? std::to_string(*cornerOwner) : "none")
                  << " vertexSignals=" << vertexSignalsAtCorner
                  << " point=" << cornerPoint.x() << ',' << cornerPoint.y()
                  << " candidateDepth=";
        if (cornerCandidateDepth) std::cerr << *cornerCandidateDepth;
        else std::cerr << "n/a";
        std::cerr << " sceneDepth=";
        if (cornerSceneDepth) std::cerr << *cornerSceneDepth;
        else std::cerr << "n/a";
        std::cerr << " selectedVertices="
                  << view->architecturalLightingSelectedVertices().size()
                  << " pickMode=" << static_cast<int>(view->pickMode())
                  << " targetMode="
                  << static_cast<int>(view->architecturalLightingTargetMode())
                  << " pickingEnabled=" << view->viewportPickingEnabled()
                  << " editingActive=" << view->architecturalLightingEditingActive()
                  << " eye=(" << view->cameraEye().x() << ','
                  << view->cameraEye().y() << ',' << view->cameraEye().z()
                  << ") forward=(" << view->cameraForward().x() << ','
                  << view->cameraForward().y() << ',' << view->cameraForward().z()
                  << ")\n";
    }
    check(cornerOwner && *cornerOwner == 0 && vertexSignalsAtCorner == 1,
        "ARCHLIGHTING click on a visible oblique vertex marker wins over its closed-door body");

    int vertexSignalsAtMarkerRim = 0;
    const QMetaObject::Connection offsetConnection = QObject::connect(
        view,
        &GlView::architecturalLightingVertexPicked,
        [&vertexSignalsAtMarkerRim](
            const std::int32_t,
            const std::int32_t,
            const std::int32_t,
            const std::int32_t,
            const Qt::KeyboardModifiers)
        {
            ++vertexSignalsAtMarkerRim;
        });
    const std::optional<std::int32_t> nearCornerOwner =
        clickArchitecturalOwner(*view, nearCorner);
    QObject::disconnect(offsetConnection);
    const QPoint nearCornerPoint = nearCornerScreen
        ? QPoint(qRound(nearCornerScreen->x()), qRound(nearCornerScreen->y()))
        : QPoint{};
    const std::optional<float> offsetCandidateDepth =
        projectedWindowDepthForDiagnostic(*view, frontCorner);
    const std::optional<float> offsetSceneDepth =
        view->architecturalLightingSceneDepthForTest(QPointF(nearCornerPoint));
    const bool offsetMarkerVisible = offsetCandidateDepth && offsetSceneDepth
        && *offsetCandidateDepth < *offsetSceneDepth;
    if (!nearCornerOwner || *nearCornerOwner != 0
        || vertexSignalsAtMarkerRim != 1)
    {
        std::cerr << std::setprecision(10)
                  << "ARCHLIGHTING near-door marker-rim diagnostic: owner="
                  << (nearCornerOwner ? std::to_string(*nearCornerOwner) : "none")
                  << " vertexSignals=" << vertexSignalsAtMarkerRim
                  << " point=" << nearCornerPoint.x() << ',' << nearCornerPoint.y()
                  << " insideRadius=" << nearCornerInsideMarker
                  << " candidateDepth=";
        if (offsetCandidateDepth) std::cerr << *offsetCandidateDepth;
        else std::cerr << "n/a";
        std::cerr << " sceneDepth=";
        if (offsetSceneDepth) std::cerr << *offsetSceneDepth;
        else std::cerr << "n/a";
        std::cerr << " offsetMarkerVisible=" << offsetMarkerVisible << '\n';
    }
    check(nearCornerInsideMarker && nearCornerOwner && *nearCornerOwner == 0
            && vertexSignalsAtMarkerRim == 1,
        "ARCHLIGHTING unified input gives the depth-tested vertex first chance near a closed-door body");

    view->setArchitecturalLightingTargetMode(
        GlView::ArchitecturalLightingTargetMode::Vertices);
    const std::optional<QPointF> markerScreenBeforeOwnerClick = displayAnchor
        ? view->projectWorldToViewport(*displayAnchor)
        : std::nullopt;
    const std::optional<std::int32_t> markerOwner = displayAnchor
        ? clickArchitecturalOwner(*view, *displayAnchor, true, false)
        : std::nullopt;
    check(markerOwner && *markerOwner == 0,
        "ARCHLIGHTING click on the displayed owner marker selects an unselected Arch Thing owner with ordinary Thing gizmos disabled");

    int ownerMarkerEditRequests = 0;
    const QMetaObject::Connection ownerMarkerEditConnection = QObject::connect(
        view,
        &GlView::architecturalLightingVertexEditRequested,
        [&ownerMarkerEditRequests](
            const std::int32_t thingIndex,
            const std::int32_t,
            const std::int32_t,
            const std::int32_t,
            const Qt::KeyboardModifiers)
        {
            if (thingIndex == 0)
            {
                ++ownerMarkerEditRequests;
            }
        });
    const std::optional<std::int32_t> rememberedOwnerBeforeDoubleClick =
        view->architecturalLightingLastOwnerClickForTest();
    if (markerScreenBeforeOwnerClick)
    {
        const QPoint markerPoint(
            qRound(markerScreenBeforeOwnerClick->x()),
            qRound(markerScreenBeforeOwnerClick->y()));
        QMouseEvent markerDoubleClick(
            QEvent::MouseButtonDblClick,
            QPointF(markerPoint),
            QPointF(markerPoint),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(view, &markerDoubleClick);
        QMouseEvent markerDoubleClickRelease(
            QEvent::MouseButtonRelease,
            QPointF(markerPoint),
            QPointF(markerPoint),
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(view, &markerDoubleClickRelease);
    }
    QObject::disconnect(ownerMarkerEditConnection);
    if (ownerMarkerEditRequests != 1)
    {
        const std::optional<std::int32_t> rememberedOwner =
            view->architecturalLightingLastOwnerClickForTest();
        std::cerr << "ARCHLIGHTING owner-marker double-click diagnostics: requests="
                  << ownerMarkerEditRequests
                  << " rememberedBefore="
                  << (rememberedOwnerBeforeDoubleClick
                          ? std::to_string(*rememberedOwnerBeforeDoubleClick)
                          : std::string("none"))
                  << " rememberedAfter="
                  << (rememberedOwner ? std::to_string(*rememberedOwner)
                                      : std::string("none"))
                  << " selectedVertices="
                  << view->architecturalLightingSelectedVertices().size()
                  << " ownerSelected="
                  << window.documentStateForTest().selection.things.contains(0)
                  << " pickMode=" << static_cast<int>(view->pickMode())
                  << " pipeline="
                  << static_cast<int>(view->renderPipeline())
                  << " targetMode="
                  << static_cast<int>(view->architecturalLightingTargetMode())
                  << " active=" << view->architecturalLightingEditingActive()
                  << " gizmoEntries="
                  << view->architecturalLightingOwnerGizmoEntryCountForTest()
                  << "\n";
    }
    check(ownerMarkerEditRequests == 1,
        "ARCHLIGHTING double-click on the displayed owner marker opens its selected vertex batch");
    window.cancelArchitecturalLightingVertexForTest();

    // Exercise the real click route while the ordinary Thing-gizmo preference
    // remains disabled. The ARCH branch owns its owner scan and vertex cache;
    // it must not depend on the generic gizmo pick pass being enabled.
    view->setSelectedThing(0);
    GlView::ArchitecturalLightingRenderVertex target{};
    bool haveTarget = false;
    if (view->architecturalLightingVerticesForThing(0).size() > 1)
    {
        target = view->architecturalLightingVerticesForThing(0)[1];
        haveTarget = true;
    }
    const std::optional<PickedVertex> picked = haveTarget
        ? clickArchitecturalVertex(*view, target.worldPosition)
        : std::nullopt;
    check(haveTarget
            && picked
            && picked->thingIndex == target.thingIndex
            && picked->lodIndex == target.lodIndex
        && picked->meshIndex == target.meshIndex
        && picked->vertexIndex == target.vertexIndex,
        "ARCHLIGHTING selected-owner click picks a real vertex with ordinary Thing gizmos disabled");

    window.activateBakedLightEditModeForTest();
    check(!window.architecturalLightingEditModeActiveForTest()
            && !view->architecturalLightingEditingActive()
            && view->architecturalLightingOwnerGizmosVisibleForTest()
            && !view->showThingGizmos(),
        "ARCHLIGHTING exit restores ordinary Thing gizmos while retaining the independent owner-light overlay");
}

void runArchThingGizmoIntegrationTests()
{
    GlView defaults{};
    check(defaults.renderPipeline() == GlView::RenderPipeline::Editor
            && defaults.showArchThingGizmos()
            && defaults.editorDefaultShowArchThingGizmos(),
        "Arch Thing gizmos default on in the Editor pipeline");
    defaults.setRenderPipeline(GlView::RenderPipeline::GamePreview);
    check(!defaults.showArchThingGizmos()
            && !defaults.previewDefaultShowArchThingGizmos(),
        "Arch Thing gizmos default off in Game Preview");
    defaults.setShowArchThingGizmos(true);
    defaults.setRenderPipeline(GlView::RenderPipeline::Editor);
    check(defaults.showArchThingGizmos()
            && defaults.editorDefaultShowArchThingGizmos(),
        "Arch Thing gizmo visibility is retained independently per pipeline");

    Level level = makeClosedDoorLevel();
    level.things.front().archLightingIndex = -1;
    level.motsArchLighting.clear();
    assets::ResourceLocator resources{};
    GlView view{};
    view.resize(640, 480);
    view.setShowWireframe(false);
    view.setShowVertices(false);
    view.setShowSolid(true);
    view.setShowTextures(false);
    view.setShowThings(true);
    view.setShowThingGizmos(false);
    view.setShowThingCollisionShapes(false);
    view.setShowThingVisualBounds(false);
    view.setShowArchThingGizmos(true);
    view.setShowAxisWidget(false);
    view.setShowToastOverlays(false);
    view.setThingViewMode(GlView::ThingViewMode::Shaded);
    view.setRenderPipeline(GlView::RenderPipeline::Editor);
    view.setTransformTool(GlView::TransformTool::Select);
    view.setResourceLocator(&resources, false);
    constexpr float kThingGizmoHeightAboveModel = 0.35f;
    view.setThingGizmoHeightAboveModel(kThingGizmoHeightAboveModel);
    view.primeModel3do("arch_picking_fixture.3do", makeClosedDoorModel());
    view.setArchitecturalLightingEditingActive(true);
    view.setLevel(&level);
    view.setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view.setOrbitViewImmediate(32.0f, 42.0f, 0.0f);
    view.frameWorldBounds(
        QVector3D{-1.2f, -0.8f, -1.2f},
        QVector3D{2.2f, 1.4f, 3.2f}, false);
    view.setPickMode(GlView::PickMode::ArchitecturalLighting);
    view.setViewportPickingEnabled(true);
    view.setSelectedThing(-1);
    view.show();
    QApplication::processEvents(QEventLoop::AllEvents);
    view.update();
    QApplication::processEvents(QEventLoop::AllEvents);

    const Thing& owner = level.things.front();
    const bool effectiveArchFlag =
        (sed::level::thingFlags(level, owner) & 0x00000008) != 0;
    const auto marker = view.architecturalLightingOwnerGizmoDisplayPositionForTest(0);
    const auto markerBounds = view.architecturalLightingOwnerGizmoBoundsForTest(0);
    const QVector3D ownerPosition(
        static_cast<float>(owner.x),
        static_cast<float>(owner.y),
        static_cast<float>(owner.z));
    const float thingTypeHalfSize =
        std::clamp(view.thingGizmoWorldSize(), 0.001f, 1.0f) * 0.5f;
    const float lightStackHalfSize =
        std::clamp(view.lightGizmoWorldSize(), 0.001f, 1.0f) * 0.732f;
    const float architecturalHalfSize =
        std::clamp(view.lightGizmoWorldSize(), 0.001f, 1.0f) * 0.61f;
    render::ThingGizmoStackLayout ownerMarkerLayout{};
    ownerMarkerLayout.thingTypeVisible = view.showThingGizmos();
    ownerMarkerLayout.archThingVisible = true;
    ownerMarkerLayout.thingTypeOffset = view.thingGizmoHeightAboveModel();
    ownerMarkerLayout.archThingOffset = view.archThingGizmoOffsetAboveModel();
    ownerMarkerLayout.spacing = render::kThingGizmoStackSpacingDefault;
    const std::optional<QVector3D> expectedMarker = markerBounds
        ? std::optional<QVector3D>{
            render::thingGizmoStackDisplayPosition(
                ownerPosition,
                &*markerBounds,
                render::ThingGizmoStackLane::ArchThing,
                thingTypeHalfSize,
                lightStackHalfSize,
                architecturalHalfSize,
                ownerMarkerLayout) }
        : std::nullopt;
    check(effectiveArchFlag && owner.archLightingIndex < 0
            && level.motsArchLighting.empty() && marker.has_value()
            && expectedMarker
            && closeQVec(*marker, *expectedMarker, 1.0e-5)
            && !view.showThingGizmos()
            && view.architecturalLightingOwnerGizmoEntryCountForTest() == 1u
            && !view.archThingGizmoDisplayPositionForTest(0).has_value(),
        "an unbound MotS Arch Thing uses the configured world-space model-top anchor without a separate generic marker");

    if (marker)
    {
        const std::optional<QPointF> projected =
            view.projectWorldToViewport(*marker);
        int pickedThing = -1;
        const QMetaObject::Connection connection = QObject::connect(
            &view, &GlView::clickPicked,
            [&pickedThing](const std::int32_t, const std::int32_t,
                           const std::int32_t thingIndex, const std::int32_t)
            {
                pickedThing = thingIndex;
            });
        if (projected)
        {
            const QPoint point(qRound(projected->x()), qRound(projected->y()));
            QMouseEvent press(
                QEvent::MouseButtonPress, QPointF(point), QPointF(point),
                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&view, &press);
            QMouseEvent release(
                QEvent::MouseButtonRelease, QPointF(point), QPointF(point),
                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(&view, &release);
            QApplication::processEvents(QEventLoop::AllEvents);
        }
        QObject::disconnect(connection);
        check(projected.has_value() && pickedThing == 0,
            "clicking the unified ARCH owner marker selects its Thing while Thing type icons are disabled");
    }
    else
    {
        check(false,
            "clicking the unified ARCH owner marker selects its Thing while Thing type icons are disabled");
    }

    // Force the ordinary Thing-gizmo cache cold, then click the hidden Arch
    // lane while only the ordinary Thing Type marker is visible. MotS does not
    // draw a standalone Arch badge in Thing mode, so that second lane must not
    // produce a Thing pick. The dedicated ARCH owner marker must still work
    // after the ordinary cache has been invalidated and rebuilt.
    const auto ownerBounds = view.architecturalLightingOwnerGizmoBoundsForTest(0);
    if (ownerBounds)
    {
        const QVector3D ownerPosition(
            static_cast<float>(owner.x),
            static_cast<float>(owner.y),
            static_cast<float>(owner.z));
        view.setArchitecturalLightingEditingActive(false);
        view.setPickMode(GlView::PickMode::Thing);
        view.setShowThingGizmos(true);
        view.setShowArchThingGizmos(false);
        view.setThingGizmoWorldSize(
            view.thingGizmoWorldSize() < 0.999f
                ? view.thingGizmoWorldSize() + 0.001f
                : view.thingGizmoWorldSize() - 0.001f);
        // Keep the hypothetical upper lane's screen ray clear of the model.
        // At the regular angled view, that pixel projects back through the
        // Thing body and a valid body pick is mistaken for a phantom badge.
        view.setOrbitViewImmediate(32.0f, 0.0f, 0.0f);
        const float thingTypeHalfSize =
            std::clamp(view.thingGizmoWorldSize(), 0.001f, 1.0f) * 0.5f;
        const float lightStackHalfSize =
            std::clamp(view.lightGizmoWorldSize(), 0.001f, 1.0f) * 0.732f;
        const float architecturalHalfSize =
            std::clamp(view.lightGizmoWorldSize(), 0.001f, 1.0f) * 0.61f;
        render::ThingGizmoStackLayout suppressedFallbackLayout{};
        suppressedFallbackLayout.thingTypeVisible = true;
        suppressedFallbackLayout.archThingVisible = false;
        suppressedFallbackLayout.thingTypeOffset = view.thingGizmoHeightAboveModel();
        suppressedFallbackLayout.archThingOffset = view.archThingGizmoOffsetAboveModel();
        suppressedFallbackLayout.spacing = view.thingGizmoStackSpacing();
        const QVector3D suppressedFallbackCenter =
            render::thingGizmoStackDisplayPosition(
                ownerPosition,
                &*ownerBounds,
                render::ThingGizmoStackLane::ArchThing,
                thingTypeHalfSize,
                lightStackHalfSize,
                architecturalHalfSize,
                suppressedFallbackLayout);
        const std::uint64_t gizmoBuildCountBeforeColdPick =
            view.thingTypeGizmoBuildCountForTest();

        int fallbackPickedThing = -2;
        const QMetaObject::Connection fallbackConnection = QObject::connect(
            &view,
            &GlView::clickPicked,
            [&fallbackPickedThing](
                const std::int32_t,
                const std::int32_t,
                const std::int32_t thingIndex,
                const std::int32_t)
            {
                fallbackPickedThing = thingIndex;
            });
        const std::optional<QPointF> fallbackPoint =
            view.projectWorldToViewport(suppressedFallbackCenter);
        if (fallbackPoint)
        {
            const QPoint point(qRound(fallbackPoint->x()), qRound(fallbackPoint->y()));
            QMouseEvent press(
                QEvent::MouseButtonPress,
                QPointF(point),
                QPointF(point),
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::NoModifier);
            QApplication::sendEvent(&view, &press);
            QMouseEvent release(
                QEvent::MouseButtonRelease,
                QPointF(point),
                QPointF(point),
                Qt::LeftButton,
                Qt::NoButton,
                Qt::NoModifier);
            QApplication::sendEvent(&view, &release);
        }
        QObject::disconnect(fallbackConnection);
        const std::uint64_t gizmoBuildCountAfterColdPick =
            view.thingTypeGizmoBuildCountForTest();
        view.setOrbitViewImmediate(32.0f, 42.0f, 0.0f);
        QApplication::processEvents(QEventLoop::AllEvents);
        check(gizmoBuildCountAfterColdPick == gizmoBuildCountBeforeColdPick,
            "MotS fallback regression reaches the invalidated Thing-gizmo cache path");
        check(fallbackPoint.has_value() && fallbackPickedThing == -1,
            "MotS cold-cache fallback does not pick a phantom standalone Arch marker");

        view.setShowThingGizmos(false);
        view.setShowArchThingGizmos(true);
        view.setArchitecturalLightingEditingActive(true);
        view.setPickMode(GlView::PickMode::ArchitecturalLighting);
        QApplication::processEvents(QEventLoop::AllEvents);
        const auto rebuiltArchMarker =
            view.architecturalLightingOwnerGizmoDisplayPositionForTest(0);
        const auto rebuiltArchPick = rebuiltArchMarker
            ? clickArchitecturalOwner(view, *rebuiltArchMarker)
            : std::nullopt;
        check(rebuiltArchPick && *rebuiltArchPick == 0,
            "ARCH owner marker remains pickable after ordinary Thing-gizmo cache invalidation");
    }
    else
    {
        check(false,
            "MotS cold-cache fallback does not pick a phantom standalone Arch marker");
        check(false,
            "ARCH owner marker remains pickable after ordinary Thing-gizmo cache invalidation");
    }

    const ProjectType originalProjectType = level.kind;
    level.kind = ProjectType::JKDF2;
    view.setLevel(&level);
    view.update();
    QApplication::processEvents(QEventLoop::AllEvents);
    check(view.archThingGizmoDisplayPositionForTest(0).has_value(),
        "JKDF2 Arch marker is available for a Thing carrying the classic 0x8 Arch flag");

    level.kind = ProjectType::IJIM;
    view.setLevel(&level);
    view.update();
    QApplication::processEvents(QEventLoop::AllEvents);
    check(!view.archThingGizmoDisplayPositionForTest(0).has_value(),
        "IJIM Whip climb flag 0x8 does not create an Arch Thing marker");
    level.kind = originalProjectType;
    std::erase_if(
        level.things.front().values,
        [](const TplValue& value) { return value.name == "thingflags"; });
    ThingTemplate inheritedTemplate{};
    inheritedTemplate.name = owner.templateName;
    inheritedTemplate.values.push_back(
        makeIntegerValue("thingflags", 0x00000008));
    level.templates.push_back(std::move(inheritedTemplate));
    view.setLevel(&level);
    view.update();
    QApplication::processEvents(QEventLoop::AllEvents);
    check(sed::level::thingFlags(level, owner) == 0x00000008
            && view.architecturalLightingOwnerGizmoDisplayPositionForTest(0).has_value()
            && !view.archThingGizmoDisplayPositionForTest(0).has_value(),
        "the unified ARCH owner marker follows effective 0x8 inherited from the Thing template");

    view.setLayerVisibilityState(
        std::unordered_set<std::int32_t>{0}, false);
    view.update();
    QApplication::processEvents(QEventLoop::AllEvents);
    check(!view.architecturalLightingOwnerGizmoDisplayPositionForTest(0),
        "the unified ARCH owner marker follows hidden-layer visibility");

    view.setLayerVisibilityState(
        std::unordered_set<std::int32_t>{}, false);
    view.setLockedLayers(std::unordered_set<std::int32_t>{0});
    view.update();
    QApplication::processEvents(QEventLoop::AllEvents);
    const auto lockedMarker =
        view.architecturalLightingOwnerGizmoDisplayPositionForTest(0);
    check(view.isLayerLocked(0) && lockedMarker.has_value(),
        "the unified ARCH owner marker remains visible for a Thing on a visible locked layer");
    view.hide();
}

void runArchThingRecoveryTests()
{
    Level previewSpawnLevel{};
    Thing playerSpawn{};
    playerSpawn.name = "walkplayer";
    playerSpawn.templateName = "walkplayer";
    playerSpawn.sector = -1;
    playerSpawn.x = 2.5;
    playerSpawn.y = 3.0;
    playerSpawn.z = -4.25;
    playerSpawn.yaw = 37.0;
    playerSpawn.values.push_back(makeStringValue("type", "player"));
    previewSpawnLevel.things.push_back(std::move(playerSpawn));

    GlView firstPreviewEntry{};
    firstPreviewEntry.setPreviewDefaultShowThingGizmos(false);
    firstPreviewEntry.setPreviewDefaultShowArchThingGizmos(false);
    firstPreviewEntry.setLevel(&previewSpawnLevel);
    check(!firstPreviewEntry.recoveryState().preview.initialized,
        "viewport display defaults do not mark Preview camera state initialized");
    firstPreviewEntry.setRenderPipeline(GlView::RenderPipeline::GamePreview);
    const GlView::RecoveryState firstPreviewState =
        firstPreviewEntry.recoveryState();
    check(firstPreviewEntry.cameraProjectionType()
                == app::ViewportCamera::ProjectionType::Perspective
            && firstPreviewState.preview.orbit.center.x() == 2.5f
            && firstPreviewState.preview.orbit.center.y() == 3.0f
            && firstPreviewState.preview.orbit.center.z() == -4.25f
            && firstPreviewState.preview.orbit.yawDeg == 37.0f
            && firstPreviewState.preview.orbit.pitchDeg == 10.0f
            && !firstPreviewEntry.showArchThingGizmos()
            && !firstPreviewEntry.showThingGizmos(),
        "first Preview entry keeps Perspective spawn framing with gizmo defaults applied");

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(editor::DocumentState{
        makeClosedDoorLevel(),
        {} });
    auto* const view = window.glViewForTest();
    check(view != nullptr,
        "Arch Thing recovery fixture exposes the viewport");
    if (!view)
    {
        return;
    }

    view->setRenderPipeline(GlView::RenderPipeline::Editor);
    view->setShowArchThingGizmos(false);
    view->setRenderPipeline(GlView::RenderPipeline::GamePreview);
    view->setShowArchThingGizmos(true);
    view->setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view->setRenderPipeline(GlView::RenderPipeline::Editor);

    const QByteArray payload = window.captureRecoveryEditorStateForTest();
    const QCborMap capturedRoot = QCborValue::fromCbor(payload).toMap();
    const QCborArray capturedViewport =
        capturedRoot.value(QStringLiteral("viewport")).toArray();
    const bool hasBothPipelines = capturedViewport.size() >= 2;
    const QCborArray capturedEditor = hasBothPipelines
        ? capturedViewport.at(0).toArray()
        : QCborArray{};
    const QCborArray capturedPreview = hasBothPipelines
        ? capturedViewport.at(1).toArray()
        : QCborArray{};
    check(!payload.isEmpty()
            && hasBothPipelines
            && capturedEditor.size() == 18
            && capturedPreview.size() == 18
            && capturedEditor.at(16).isBool()
            && !capturedEditor.at(16).toBool()
            && capturedPreview.at(16).isBool()
            && capturedPreview.at(16).toBool(),
        "viewport recovery stores distinct Editor and Preview Arch visibility values");
    check(capturedPreview.at(1).toInteger(-1)
                == static_cast<int>(
                    app::ViewportCamera::ProjectionType::Orthographic)
            && capturedPreview.at(15).toBool(),
        "viewport recovery captures an initialized Preview camera independently");

    view->setShowArchThingGizmos(true);
    view->setRenderPipeline(GlView::RenderPipeline::GamePreview);
    view->setShowArchThingGizmos(false);
    QString warning{};
    const bool restored = window.restoreRecoveryEditorStateForTest(
        payload, &warning);
    const bool editorRestored = restored
        && view->renderPipeline() == GlView::RenderPipeline::Editor
        && !view->showArchThingGizmos();
    view->setRenderPipeline(GlView::RenderPipeline::GamePreview);
    check(editorRestored && view->showArchThingGizmos(),
        "viewport recovery round-trip restores separate Arch visibility per pipeline");
    check(view->cameraProjectionType()
                == app::ViewportCamera::ProjectionType::Orthographic,
        "recovered initialized Preview camera remains authoritative on first re-entry");

    QCborMap legacyRoot = capturedRoot;
    QCborArray legacyViewport = capturedViewport;
    bool legacyShapeBuilt = capturedViewport.size() >= 2;
    for (qsizetype pipeline = 0; pipeline < 2 && legacyShapeBuilt; ++pipeline)
    {
        QCborArray legacyState = legacyViewport.at(pipeline).toArray();
        legacyShapeBuilt = legacyState.size() == 18;
        if (legacyShapeBuilt)
        {
            // Index 17 is the additive viewport-state map. Strip it first,
            // then remove the preceding Arch flag to emulate the old 16-slot
            // positional record and exercise Editor/Preview default fallback.
            legacyState.removeLast();
            legacyState.removeAt(16);
            legacyViewport.removeAt(pipeline);
            legacyViewport.insert(pipeline, legacyState);
        }
    }
    legacyRoot.insert(QStringLiteral("viewport"), legacyViewport);
    const QByteArray legacyPayload = legacyShapeBuilt
        ? ui::recovery::serializeCanonicalCbor(QCborValue(legacyRoot))
        : QByteArray{};
    view->setShowArchThingGizmos(true);
    const bool restoredLegacy = !legacyPayload.isEmpty()
        && window.restoreRecoveryEditorStateForTest(legacyPayload, &warning);
    const bool legacyEditorDefault = restoredLegacy
        && view->renderPipeline() == GlView::RenderPipeline::Editor
        && view->showArchThingGizmos();
    view->setRenderPipeline(GlView::RenderPipeline::GamePreview);
    check(legacyEditorDefault && !view->showArchThingGizmos(),
        "legacy recovery without the optional Arch visibility value defaults Editor on and Preview off");
    window.hide();
}

void runArchitecturalLightingThinDoorDirectPickTests()
{
    for (const app::ViewportCamera::ProjectionType projection : {
             app::ViewportCamera::ProjectionType::Orthographic,
             app::ViewportCamera::ProjectionType::Perspective })
    {
        Level level = makeSixBySixDoorLevel();
        assets::ResourceLocator resources{};
        GlView view{};
        configurePickingView(view, level, resources, projection, false);
        view.primeModel3do("arch_picking_fixture.3do", makeSixBySixDoorModel());
        view.setLevel(&level);
        view.setShowThings(true);
        view.setThingViewMode(GlView::ThingViewMode::Shaded);
        view.setOrbitViewImmediate(0.0f, 0.0f, 0.0f);
        view.frameWorldBounds(
            QVector3D{ -0.5f, -0.2f, -0.5f },
            QVector3D{  0.5f,  0.35f,  0.5f },
            false);
        view.setSelectedThing(-1);
        view.update();
        QApplication::processEvents(QEventLoop::AllEvents);

        const auto marker = view.architecturalLightingOwnerGizmoDisplayPositionForTest(0);
        check(marker.has_value(),
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING narrow-door orthographic fixture publishes its visible owner icon"
                : "ARCHLIGHTING narrow-door perspective fixture publishes its visible owner icon");

        const std::optional<std::int32_t> bodyOwner =
            clickArchitecturalOwner(view, QVector3D{ 0.0f, 0.0f, 0.0f });
        check(bodyOwner && *bodyOwner == 0,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING narrow-door orthographic visible body click selects its owner"
                : "ARCHLIGHTING narrow-door perspective visible body click selects its owner");

        const std::optional<std::int32_t> markerOwner = marker
            ? clickArchitecturalOwner(view, *marker)
            : std::nullopt;
        check(markerOwner && *markerOwner == 0,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING narrow-door orthographic icon click selects its owner"
                : "ARCHLIGHTING narrow-door perspective icon click selects its owner");
        view.hide();
    }
}

void runArchitecturalLightingContextPickTests()
{
    for (const app::ViewportCamera::ProjectionType projection : {
             app::ViewportCamera::ProjectionType::Orthographic,
             app::ViewportCamera::ProjectionType::Perspective })
    {
        Level level = makeSixBySixDoorLevel();
        assets::ResourceLocator resources{};
        GlView view{};
        configurePickingView(view, level, resources, projection, false);
        view.primeModel3do("arch_picking_fixture.3do", makeSixBySixDoorModel());
        view.setLevel(&level);
        view.setShowThings(true);
        view.setThingViewMode(GlView::ThingViewMode::Shaded);
        view.setOrbitViewImmediate(0.0f, 0.0f, 0.0f);
        view.frameWorldBounds(
            QVector3D{ -0.5f, -0.2f, -0.5f },
            QVector3D{  0.5f,  0.35f,  0.5f },
            false);
        view.setSelectedThing(0);
        view.update();
        QApplication::processEvents(QEventLoop::AllEvents);

        const auto marker = view.architecturalLightingOwnerGizmoDisplayPositionForTest(0);
        check(marker.has_value(),
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING context fixture publishes its orthographic owner icon"
                : "ARCHLIGHTING context fixture publishes its perspective owner icon");

        // The release handler must acquire the owning widget context itself;
        // ordinary Qt event delivery does not guarantee that one is current.
        clearCurrentOpenGlContextForTest(view);
        check(QOpenGLContext::currentContext() == nullptr,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic direct pick runs with no current GL context"
                : "ARCHLIGHTING perspective direct pick runs with no current GL context");
        const std::optional<std::int32_t> noContextBody =
            clickArchitecturalOwner(
                view,
                QVector3D{ 0.0f, 0.0f, 0.0f },
                false);
        check(QOpenGLContext::currentContext() == nullptr,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic no-context pick leaves no context current before callbacks"
                : "ARCHLIGHTING perspective no-context pick leaves no context current before callbacks");
        check(noContextBody && *noContextBody == 0,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic body pick acquires its context"
                : "ARCHLIGHTING perspective body pick acquires its context");

        GlView foreignView{};
        Level foreignLevel = makePickingLevel(QVector3D{ 0.0f, 0.0f, 0.0f }, false);
        assets::ResourceLocator foreignResources{};
        configurePickingView(
            foreignView,
            foreignLevel,
            foreignResources,
            projection,
            false);
        foreignView.makeCurrent();
        check(QOpenGLContext::currentContext() == foreignView.context(),
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic context fixture installs a foreign GL context"
                : "ARCHLIGHTING perspective context fixture installs a foreign GL context");
        const std::optional<std::int32_t> foreignMarker = marker
            ? clickArchitecturalOwner(view, *marker, false)
            : std::nullopt;
        check(QOpenGLContext::currentContext() == foreignView.context(),
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic pick restores the foreign context after dispatch"
                : "ARCHLIGHTING perspective pick restores the foreign context after dispatch");
        check(foreignMarker && *foreignMarker == 0,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic marker pick switches away from a foreign context"
                : "ARCHLIGHTING perspective marker pick switches away from a foreign context");
        foreignView.doneCurrent();

        // The scene-depth helper temporarily binds its own readback FBO. It
        // must restore a caller-owned target as well as the owning context so
        // an ARCH click cannot redirect a surrounding render pass.
        view.makeCurrent();
        QOpenGLFramebufferObjectFormat callerFboFormat{};
        callerFboFormat.setAttachment(QOpenGLFramebufferObject::Depth);
        callerFboFormat.setSamples(0);
        callerFboFormat.setInternalTextureFormat(GL_RGBA8);
        QOpenGLFramebufferObject callerFbo(QSize(64, 64), callerFboFormat);
        const bool callerFboReady = callerFbo.isValid() && callerFbo.bind();
        GLint callerFboBefore = 0;
        GLint callerFboAfter = 0;
        if (callerFboReady)
        {
            QOpenGLContext::currentContext()->functions()->glGetIntegerv(
                GL_FRAMEBUFFER_BINDING,
                &callerFboBefore);
            if (marker)
            {
                (void)clickArchitecturalOwner(view, *marker, false);
            }
            QOpenGLContext::currentContext()->functions()->glGetIntegerv(
                GL_FRAMEBUFFER_BINDING,
                &callerFboAfter);
        }
        check(callerFboReady && callerFboBefore != 0
                && callerFboAfter == callerFboBefore,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic depth pick restores a caller FBO"
                : "ARCHLIGHTING perspective depth pick restores a caller FBO");
        if (callerFboReady)
        {
            callerFbo.release();
        }
        view.doneCurrent();

        const auto vertices = view.architecturalLightingVerticesForThing(0);
        const auto target = std::find_if(
            vertices.begin(),
            vertices.end(),
            [](const GlView::ArchitecturalLightingRenderVertex& vertex)
            {
                return vertex.meshIndex == 0 && vertex.vertexIndex == 1;
            });
        if (target != vertices.end())
        {
            clearCurrentOpenGlContextForTest(view);
            const std::optional<PickedVertex> noContextVertex =
                clickArchitecturalVertex(
                    view,
                    target->worldPosition,
                    Qt::NoModifier,
                    false);
            check(QOpenGLContext::currentContext() == nullptr,
                projection == app::ViewportCamera::ProjectionType::Orthographic
                    ? "ARCHLIGHTING orthographic vertex pick leaves no context current before callbacks"
                    : "ARCHLIGHTING perspective vertex pick leaves no context current before callbacks");
            check(noContextVertex
                    && noContextVertex->thingIndex == 0
                    && noContextVertex->lodIndex == target->lodIndex
                    && noContextVertex->meshIndex == target->meshIndex
                    && noContextVertex->vertexIndex == target->vertexIndex,
                projection == app::ViewportCamera::ProjectionType::Orthographic
                    ? "ARCHLIGHTING orthographic vertex pick acquires its context"
                    : "ARCHLIGHTING perspective vertex pick acquires its context");
        }
        else
        {
            check(false,
                projection == app::ViewportCamera::ProjectionType::Orthographic
                    ? "ARCHLIGHTING orthographic context fixture publishes a source vertex"
                    : "ARCHLIGHTING perspective context fixture publishes a source vertex");
        }

        foreignView.hide();
        view.hide();
    }
}

void runArchitecturalLightingMultiOwnerVertexPickTests()
{
    Level level = makeClosedDoorPairLevel();
    assets::ResourceLocator resources{};
    GlView view{};
    configurePickingView(
        view,
        level,
        resources,
        app::ViewportCamera::ProjectionType::Orthographic,
        false);
    view.primeModel3do("arch_picking_fixture.3do", makeClosedDoorModel());
    view.setLevel(&level);
    view.setShowThings(true);
    view.setThingViewMode(GlView::ThingViewMode::Shaded);
    view.setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view.setOrbitViewImmediate(32.0f, 42.0f, 0.0f);
    view.frameWorldBounds(
        QVector3D{ -1.2f, -0.8f, -1.2f },
        QVector3D{  6.2f,  1.4f,  3.2f },
        false);

    editor::Selection selectedOwners{};
    selectedOwners.things.insert(0);
    selectedOwners.things.insert(1);
    view.setSelectionState(-1, -1, 0, -1, selectedOwners);
    view.update();
    QApplication::processEvents(QEventLoop::AllEvents);

    const auto firstOwnerVertices = view.architecturalLightingVerticesForThing(0);
    const auto secondOwnerVertices = view.architecturalLightingVerticesForThing(1);
    check(!firstOwnerVertices.empty() && !secondOwnerVertices.empty(),
        "ARCHLIGHTING builds a bounded vertex cache for every selected owner");
    if (secondOwnerVertices.empty())
    {
        view.hide();
        return;
    }

    const auto target = std::find_if(
        secondOwnerVertices.begin(),
        secondOwnerVertices.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return vertex.meshIndex == 0 && vertex.vertexIndex == 1;
        });
    const std::optional<PickedVertex> picked = target != secondOwnerVertices.end()
        ? clickArchitecturalVertex(view, target->worldPosition)
        : std::nullopt;
    check(target != secondOwnerVertices.end()
            && picked
            && picked->thingIndex == 1
            && picked->lodIndex == target->lodIndex
            && picked->meshIndex == target->meshIndex
            && picked->vertexIndex == target->vertexIndex,
        "ARCHLIGHTING picks an exact vertex from the second selected owner");

    const auto firstTargetIt = std::find_if(
        firstOwnerVertices.begin(),
        firstOwnerVertices.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            // Camera yaw/pitch (32, 42) looks from +X/-Y/+Z. Vertex 5 is
            // the visible camera-facing upper corner; vertex 0 is occluded.
            return vertex.meshIndex == 0 && vertex.vertexIndex == 5;
        });
    if (firstTargetIt != firstOwnerVertices.end()
        && target != secondOwnerVertices.end())
    {
        const std::optional<PickedVertex> firstClick =
            clickArchitecturalVertex(view, firstTargetIt->worldPosition);
        const std::optional<PickedVertex> secondClick = clickArchitecturalVertex(
            view,
            target->worldPosition,
            Qt::ShiftModifier);
        const auto selected = view.architecturalLightingSelectedVertices();
        const auto hasIdentity = [&selected](
            const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return std::ranges::find(
                       selected,
                       GlView::ArchitecturalLightingVertexIdentity{
                           vertex.thingIndex,
                           vertex.lodIndex,
                           vertex.meshIndex,
                           vertex.vertexIndex})
                != selected.end();
        };
        const bool exactMultiOwnerSelection = selected.size() == 2
            && hasIdentity(*firstTargetIt)
            && hasIdentity(*target);
        if (!exactMultiOwnerSelection)
        {
            std::cerr << "ARCHLIGHTING multi-owner vertex diagnostic: first=";
            if (firstClick)
            {
                std::cerr << firstClick->thingIndex << ':' << firstClick->meshIndex
                          << ':' << firstClick->vertexIndex;
            }
            else
            {
                std::cerr << "none";
            }
            std::cerr << " second=";
            if (secondClick)
            {
                std::cerr << secondClick->thingIndex << ':' << secondClick->meshIndex
                          << ':' << secondClick->vertexIndex;
            }
            else
            {
                std::cerr << "none";
            }
            std::cerr << " selected=";
            for (const auto& identity : selected)
            {
                std::cerr << identity.thingIndex << ':' << identity.meshIndex
                          << ':' << identity.vertexIndex << ',';
            }
            std::cerr << " targetScreens=";
            for (const auto* vertex : { &*firstTargetIt, &*target })
            {
                const auto screen = view.projectWorldToViewport(vertex->worldPosition);
                if (screen)
                {
                    std::cerr << '(' << screen->x() << ',' << screen->y() << ')';
                }
                else
                {
                    std::cerr << "none";
                }
            }
            std::cerr << '\n';
        }
        check(exactMultiOwnerSelection,
            "ARCHLIGHTING Shift-click keeps exact vertices from multiple owners");
    }
    view.hide();
}

void runArchitecturalLightingMainWindowOwnerPreservationTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setPrimaryResourceOverrideForTest(
        "arch_picking_fixture.3do",
        []()
        {
            return std::optional<std::vector<std::byte>>(
                pickingModelFixtureBytes());
        });
    Level exactSelectionLevel = makeClosedDoorPairLevel();
    exactSelectionLevel.motsArchLighting[0].meshes[0].vertices[0].mono = 0.45f;
    exactSelectionLevel.motsArchLighting[1].meshes[0].vertices[1].mono = 0.45f;
    window.setDocumentStateForTest(editor::DocumentState{
        std::move(exactSelectionLevel),
        {} });
    window.setProjectTypeForTest(ProjectType::MOTS);
    window.resize(640, 480);
    window.show();
    QApplication::processEvents(QEventLoop::AllEvents);

    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING MainWindow fixture exposes the viewport for owner preservation");
    if (!view)
    {
        return;
    }

    view->setShowThings(true);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->setShowThingGizmos(false);
    view->setSurfaceClickSelectionEnabled(true);
    view->setTransformTool(GlView::TransformTool::Select);
    view->setResourceLocator(&window.resourceLocatorForTest(), false);
    view->primeModel3do("arch_picking_fixture.3do", makeClosedDoorModel());
    view->setLevel(&window.mutableDocumentStateForTest().level);
    view->setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view->setOrbitViewImmediate(32.0f, 42.0f, 0.0f);
    view->frameWorldBounds(
        QVector3D{ -1.2f, -0.8f, -1.2f },
        QVector3D{  6.2f,  1.4f,  3.2f },
        false);
    window.activateArchitecturalLightingEditModeForTest();

    editor::Selection selectedOwners{};
    selectedOwners.things.insert(0);
    selectedOwners.things.insert(1);
    window.mutableDocumentStateForTest().selection = selectedOwners;
    view->setSelectionState(-1, -1, 0, -1, selectedOwners);
    view->setSelectedThing(0);
    view->setArchitecturalLightingTargetMode(
        GlView::ArchitecturalLightingTargetMode::Vertices);
    view->update();
    QApplication::processEvents(QEventLoop::AllEvents);

    const auto firstOwnerVertices = view->architecturalLightingVerticesForThing(0);
    const auto secondOwnerVertices = view->architecturalLightingVerticesForThing(1);
    const auto firstTargetIt = std::find_if(
        firstOwnerVertices.begin(),
        firstOwnerVertices.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return vertex.meshIndex == 0 && vertex.vertexIndex == 0;
        });
    const auto secondTargetIt = std::find_if(
        secondOwnerVertices.begin(),
        secondOwnerVertices.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return vertex.meshIndex == 0 && vertex.vertexIndex == 1;
        });
    check(firstTargetIt != firstOwnerVertices.end()
            && secondTargetIt != secondOwnerVertices.end(),
        "ARCHLIGHTING MainWindow fixture publishes exact vertices for both selected owners");
    if (firstTargetIt == firstOwnerVertices.end()
        || secondTargetIt == secondOwnerVertices.end())
    {
        view->hide();
        return;
    }
    const GlView::ArchitecturalLightingRenderVertex firstTarget = *firstTargetIt;
    const GlView::ArchitecturalLightingRenderVertex secondTarget = *secondTargetIt;

    const auto ownersRemainSelected = [&window]()
    {
        return window.documentStateForTest().selection.things
            == std::unordered_set<std::int32_t>{ 0, 1 };
    };
    const auto hasIdentity = [view](
                                  const GlView::ArchitecturalLightingRenderVertex& vertex)
    {
        return std::ranges::find(
                   view->architecturalLightingSelectedVertices(),
                   GlView::ArchitecturalLightingVertexIdentity{
                       vertex.thingIndex,
                       vertex.lodIndex,
                       vertex.meshIndex,
                       vertex.vertexIndex })
            != view->architecturalLightingSelectedVertices().end();
    };

    (void)clickArchitecturalVertex(*view, firstTarget.worldPosition);
    check(ownersRemainSelected()
            && view->architecturalLightingSelectedVertices().size() == 1
            && hasIdentity(firstTarget)
            && window.architecturalLightingVertexDialogForTest() == nullptr,
        "ARCHLIGHTING MainWindow plain vertex click preserves both owners without opening a dialog");

    (void)clickArchitecturalVertex(
        *view,
        secondTarget.worldPosition,
        Qt::ShiftModifier);
    check(ownersRemainSelected()
            && view->architecturalLightingSelectedVertices().size() == 2
            && hasIdentity(firstTarget)
            && hasIdentity(secondTarget),
        "ARCHLIGHTING MainWindow Shift-click adds the second vertex without collapsing owners");

    const std::optional<QPointF> secondScreen =
        view->projectWorldToViewport(secondTarget.worldPosition);
    bool batchPopupOpened = false;
    bool doubleClickEditRequested = false;
    std::optional<std::array<std::int32_t, 4>> requestedIdentity{};
    QPointer<QDialog> doubleClickDialog{};
    std::size_t doubleClickSelectedCount = 0;
    if (secondScreen)
    {
        const QPoint point(
            qRound(secondScreen->x()),
            qRound(secondScreen->y()));
        // Deliver the real unmodified double-click sequence. The first
        // release must keep an already-selected vertex set intact so the
        // subsequent MouseButtonDblClick can open the batch editor.
        QMouseEvent firstPress(
            QEvent::MouseButtonPress,
            QPointF(point),
            QPointF(point),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(view, &firstPress);
        QMouseEvent firstRelease(
            QEvent::MouseButtonRelease,
            QPointF(point),
            QPointF(point),
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(view, &firstRelease);

        bool editRequested = false;
        const QMetaObject::Connection editConnection = QObject::connect(
            view,
            &GlView::architecturalLightingVertexEditRequested,
            [&editRequested, &requestedIdentity](
                const std::int32_t thingIndex,
                const std::int32_t lodIndex,
                const std::int32_t meshIndex,
                const std::int32_t vertexIndex,
                const Qt::KeyboardModifiers)
            {
                editRequested = true;
                requestedIdentity = std::array<std::int32_t, 4>{
                    thingIndex, lodIndex, meshIndex, vertexIndex };
            });
        QMouseEvent doubleClick(
            QEvent::MouseButtonDblClick,
            QPointF(point),
            QPointF(point),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(view, &doubleClick);
        QMouseEvent doubleClickRelease(
            QEvent::MouseButtonRelease,
            QPointF(point),
            QPointF(point),
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(view, &doubleClickRelease);
        QApplication::processEvents(QEventLoop::AllEvents);
        QObject::disconnect(editConnection);
        doubleClickEditRequested = editRequested;
        doubleClickDialog = window.architecturalLightingVertexDialogForTest();
        doubleClickSelectedCount =
            window.architecturalLightingSelectedVertexCountForTest();
        batchPopupOpened = doubleClickDialog
            && window.architecturalLightingVertexDialogForTest()
                == doubleClickDialog.data()
            && editRequested
            && doubleClickSelectedCount == 2;
    }
    if (!batchPopupOpened)
    {
        std::cerr << "ARCHLIGHTING double-click diagnostics: screen="
                  << (secondScreen ? "present" : "missing")
                  << " editSignal=" << doubleClickEditRequested
                  << " dialog=" << (doubleClickDialog ? "present" : "null")
                  << " selected=" << doubleClickSelectedCount
                  << " viewportSelected="
                  << view->architecturalLightingSelectedVertices().size()
                  << " owners=" << ownersRemainSelected()
                  << " pickMode=" << static_cast<int>(view->pickMode())
                  << " targetMode="
                  << static_cast<int>(view->architecturalLightingTargetMode())
                  << " requested=";
        if (requestedIdentity)
        {
            for (const std::int32_t component : *requestedIdentity)
            {
                std::cerr << component << ':';
            }
        }
        else
        {
            std::cerr << "none";
        }
        std::cerr << "\n";
    }
    check(batchPopupOpened,
        "ARCHLIGHTING MainWindow double-click opens the batch vertex popup for both selected owners");
    window.cancelArchitecturalLightingVertexForTest();

    (void)clickArchitecturalVertex(
        *view,
        firstTarget.worldPosition,
        Qt::ControlModifier);
    check(ownersRemainSelected()
            && view->architecturalLightingSelectedVertices().size() == 1
            && hasIdentity(secondTarget)
            && !hasIdentity(firstTarget),
        "ARCHLIGHTING MainWindow Control-click removes one vertex while preserving both owners");
    view->hide();
}

void runArchitecturalLightingMissDeselectionTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setPrimaryResourceOverrideForTest(
        "arch_picking_fixture.3do",
        []()
        {
            return std::optional<std::vector<std::byte>>(
                pickingModelFixtureBytes());
        });
    Level level = makeClosedDoorPairLevel();
    Thing unboundModel = level.things.front();
    unboundModel.x = 8.0;
    unboundModel.archLightingIndex = -1;
    for (TplValue& value : unboundModel.values)
    {
        if (text::iequalsAscii(value.name, "thingflags"))
        {
            value.intValue = 0;
        }
    }
    level.things.push_back(std::move(unboundModel));
    Sector& surfaceSector = level::levelCreateCube(
        level, 12.0, 0.0, 0.0, TVector::up, TVector::right);
    surfaceSector.layer = 0;

    window.setDocumentStateForTest(editor::DocumentState{
        std::move(level),
        {} });
    window.setProjectTypeForTest(ProjectType::MOTS);
    window.resize(640, 480);
    window.show();
    QApplication::processEvents(QEventLoop::AllEvents);
    const bool initialViewRefitSettled = waitForEditorViewRefitToSettle(window);
    check(initialViewRefitSettled,
        "ARCHLIGHTING empty-click fixture waits for the MainWindow startup camera refit");
    if (!initialViewRefitSettled)
    {
        window.hide();
        return;
    }

    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING empty-click fixture exposes the viewport");
    if (!view)
    {
        return;
    }

    view->setShowThings(true);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->setShowThingGizmos(false);
    view->setSurfaceClickSelectionEnabled(true);
    view->setTransformTool(GlView::TransformTool::Select);
    view->setResourceLocator(&window.resourceLocatorForTest(), false);
    view->primeModel3do("arch_picking_fixture.3do", makeClosedDoorModel());
    view->setLevel(&window.mutableDocumentStateForTest().level);
    view->setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view->setOrbitViewImmediate(32.0f, 42.0f, 0.0f);
    view->frameWorldBounds(
        QVector3D{ -1.2f, -0.8f, -1.2f },
        QVector3D{ 14.0f,  1.4f,  3.2f },
        false);
    window.activateArchitecturalLightingEditModeForTest();

    const auto selectOwnerPairWithOneVertex = [&]()
    {
        editor::Selection selection{};
        selection.things.insert(0);
        selection.things.insert(1);
        window.mutableDocumentStateForTest().selection = selection;
        view->setSelectionState(-1, -1, 0, -1, selection);
        view->setSelectedThing(0);
        const auto vertices = view->architecturalLightingVerticesForThing(0);
        if (vertices.empty())
        {
            return false;
        }
        const GlView::ArchitecturalLightingVertexIdentity identity{
            vertices.front().thingIndex,
            vertices.front().lodIndex,
            vertices.front().meshIndex,
            vertices.front().vertexIndex };
        view->setArchitecturalLightingSelectedVertices(
            std::span<const GlView::ArchitecturalLightingVertexIdentity>(
                &identity, 1));
        return view->architecturalLightingSelectedVertices().size() == 1;
    };
    const auto ownersRemainSelected = [&window]()
    {
        return window.documentStateForTest().selection.things
            == std::unordered_set<std::int32_t>{ 0, 1 };
    };
    const bool fixtureReady = selectOwnerPairWithOneVertex();
    check(fixtureReady,
        "ARCHLIGHTING empty-click fixture starts with multiple selected owners and one exact vertex");
    if (!fixtureReady)
    {
        view->hide();
        return;
    }

    const QPoint emptyPoint(3, 3);
    clickViewportPoint(*view, emptyPoint, Qt::ShiftModifier);
    check(ownersRemainSelected()
            && view->architecturalLightingSelectedVertices().size() == 1,
        "ARCHLIGHTING Shift-click on empty viewport space preserves the owner and vertex selection");
    clickViewportPoint(*view, emptyPoint, Qt::ControlModifier);
    check(ownersRemainSelected()
            && view->architecturalLightingSelectedVertices().size() == 1,
        "ARCHLIGHTING Control-click on empty viewport space preserves the owner and vertex selection");
    dragViewportRect(*view, QPoint(3, 3), QPoint(28, 25));
    check(ownersRemainSelected()
            && view->architecturalLightingSelectedVertices().size() == 1,
        "ARCHLIGHTING empty area drag does not clear the existing owner or vertex selection");

    clickViewportPoint(*view, emptyPoint);
    check(window.documentStateForTest().selection.things.empty()
            && view->architecturalLightingSelectedVertices().empty()
            && view->activeThingIndex() < 0,
        "ARCHLIGHTING unmodified click on empty viewport space clears owner and vertex selection");

    check(selectOwnerPairWithOneVertex(),
        "ARCHLIGHTING empty-click regression can restore its selected-owner fixture");
    const Thing& unboundThing = window.documentStateForTest().level.things[2];
    const QVector3D unboundBodyCenter(
        static_cast<float>(unboundThing.x + 0.5),
        static_cast<float>(unboundThing.y + 0.3),
        static_cast<float>(unboundThing.z + 1.0));
    const std::optional<QPointF> unboundScreen =
        view->projectWorldToViewport(unboundBodyCenter);
    if (unboundScreen)
    {
        clickViewportPoint(
            *view,
            QPoint(qRound(unboundScreen->x()), qRound(unboundScreen->y())));
    }
    check(unboundScreen
            && window.documentStateForTest().selection.things.empty()
            && view->architecturalLightingSelectedVertices().empty()
            && view->activeThingIndex() < 0,
        "ARCHLIGHTING treats a model without the Arch flag as a miss instead of an Add-binding target");

    check(selectOwnerPairWithOneVertex(),
        "ARCHLIGHTING empty-click regression restores its selected-owner fixture before a level-surface click");
    const Level& surfaceLevel = window.documentStateForTest().level;
    const Sector& clickedSector = surfaceLevel.sectors.back();
    QVector3D surfaceCenter{};
    bool haveFacingSurface = false;
    const QVector3D cameraForward = view->cameraForward();
    for (const Surface& surface : clickedSector.surfaces)
    {
        if (surface.geo < static_cast<std::int32_t>(TGeoMode::Solid)
            || surface.vertices.empty())
        {
            continue;
        }
        const QVector3D normal(
            static_cast<float>(surface.normal.x),
            static_cast<float>(surface.normal.y),
            static_cast<float>(surface.normal.z));
        if (QVector3D::dotProduct(normal, cameraForward) >= 0.0f)
        {
            continue;
        }
        surfaceCenter = {};
        std::size_t count = 0;
        for (const std::int32_t vertexIndex : surface.vertices)
        {
            if (vertexIndex < 0
                || static_cast<std::size_t>(vertexIndex)
                    >= clickedSector.vertices.size())
            {
                continue;
            }
            const Vertex& vertex = clickedSector.vertices[
                static_cast<std::size_t>(vertexIndex)];
            surfaceCenter += QVector3D(
                static_cast<float>(vertex.x),
                static_cast<float>(vertex.y),
                static_cast<float>(vertex.z));
            ++count;
        }
        if (count > 0)
        {
            surfaceCenter /= static_cast<float>(count);
            haveFacingSurface = true;
            break;
        }
    }
    const std::optional<QPointF> surfaceScreen = haveFacingSurface
        ? view->projectWorldToViewport(surfaceCenter)
        : std::nullopt;
    if (surfaceScreen)
    {
        clickViewportPoint(
            *view,
            QPoint(qRound(surfaceScreen->x()), qRound(surfaceScreen->y())));
    }
    check(surfaceScreen
            && window.documentStateForTest().selection.things.empty()
            && view->architecturalLightingSelectedVertices().empty()
            && view->activeThingIndex() < 0,
        "ARCHLIGHTING unmodified click on a visible level surface clears owner and vertex selection");
    view->hide();
}

void runThingLightGizmoDirectPickTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setPrimaryResourceOverrideForTest(
        "arch_picking_fixture.3do",
        []()
        {
            return std::optional<std::vector<std::byte>>(
                pickingModelFixtureBytes());
        });
    Level level = makeClosedDoorPairLevel();
    level.sectors.emplace_back();
    for (Thing& thing : level.things)
    {
        thing.sector = 0;
        // JKDF2/MotS regular Thing lights are scalar. Slash-separated RGB
        // plus range values are IJIM's format and resolve to zero here.
        thing.values.push_back(makeStringValue("light", "0.25"));
    }
    level.things[1].values.push_back(makeStringValue("type", "actor"));
    level.things[1].values.push_back(makeStringValue("typeflags", "0x4"));
    level.things[1].values.push_back(makeStringValue("lightintensity", "0.8"));
    level.things[1].values.push_back(makeStringValue("lightoffset", "0/0/4"));
    window.setDocumentStateForTest(editor::DocumentState{
        std::move(level),
        {} });
    window.setProjectTypeForTest(ProjectType::MOTS);
    window.resize(640, 480);
    window.show();
    QApplication::processEvents(QEventLoop::AllEvents);
    const bool initialViewRefitSettled = waitForEditorViewRefitToSettle(window);
    check(initialViewRefitSettled,
        "Thing-light icon fixture waits for the MainWindow startup camera refit");
    if (!initialViewRefitSettled)
    {
        window.hide();
        return;
    }

    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "Thing-light icon fixture exposes the MainWindow viewport");
    if (!view)
    {
        window.hide();
        return;
    }
    view->setShowThings(true);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->setShowThingGizmos(false);
    view->setShowThingCollisionShapes(false);
    view->setShowThingVisualBounds(false);
    view->setSurfaceClickSelectionEnabled(true);
    view->setTransformTool(GlView::TransformTool::Select);
    view->setResourceLocator(&window.resourceLocatorForTest(), false);
    view->primeModel3do("arch_picking_fixture.3do", makeClosedDoorModel());
    view->setLevel(&window.mutableDocumentStateForTest().level);
    view->setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view->setOrbitViewImmediate(32.0f, 0.0f, 0.0f);
    view->frameWorldBounds(
        QVector3D{ -2.0f, -1.5f, -1.5f },
        QVector3D{  7.0f,  2.0f,  5.0f },
        false);
    window.activateThingLightEditModeForTest();
    QApplication::processEvents(QEventLoop::AllEvents);

    const std::vector<render::LightGizmoEntry> entries =
        render::thingLightGizmoEntries(
            window.documentStateForTest().level,
            nullptr);
    const auto entryFor = [&entries](
                              const std::int32_t owner,
                              const render::LightGizmoKind kind)
        -> std::optional<render::LightGizmoEntry>
    {
        const auto found = std::find_if(
            entries.begin(),
            entries.end(),
            [owner, kind](const render::LightGizmoEntry& entry)
            {
                return entry.ownerIndex == owner && entry.kind == kind;
            });
        return found == entries.end()
            ? std::nullopt
            : std::optional<render::LightGizmoEntry>{ *found };
    };
    const auto displayPositionFor = [view](
                                       const render::LightGizmoEntry& entry)
        -> QVector3D
    {
        const float baseHalfSize =
            std::clamp(view->lightGizmoWorldSize(), 0.001f, 1.0f) * 0.5f;
        return view->thingLightGizmoDisplayPositionForTest(
            entry,
            view->cameraUp(),
            baseHalfSize);
    };
    const std::optional<render::LightGizmoEntry> firstLight =
        entryFor(0, render::LightGizmoKind::ThingRegular);
    const std::optional<render::LightGizmoEntry> secondLight =
        entryFor(1, render::LightGizmoKind::ThingRegular);
    const std::optional<render::LightGizmoEntry> secondHeadlight =
        entryFor(1, render::LightGizmoKind::ThingHead);
    const auto displayCenterFor = [&displayPositionFor](
                                      const std::optional<render::LightGizmoEntry>& entry)
        -> std::optional<QVector3D>
    {
        return entry
            ? std::optional<QVector3D>{ displayPositionFor(*entry) }
            : std::nullopt;
    };
    const auto projectedCenterFor = [view](
                                        const std::optional<QVector3D>& center)
        -> std::optional<QPointF>
    {
        return center ? view->projectWorldToViewport(*center) : std::nullopt;
    };
    const auto onScreenPointFor = [view](const std::optional<QPointF>& point)
        -> std::optional<QPoint>
    {
        if (!point
            || point->x() < 0.0 || point->x() >= static_cast<qreal>(view->width())
            || point->y() < 0.0 || point->y() >= static_cast<qreal>(view->height()))
        {
            return std::nullopt;
        }
        return QPoint(qRound(point->x()), qRound(point->y()));
    };
    const std::optional<QVector3D> firstCenter = displayCenterFor(firstLight);
    const std::optional<QVector3D> secondCenter = displayCenterFor(secondLight);
    const std::optional<QVector3D> headlightCenter =
        displayCenterFor(secondHeadlight);
    const std::optional<QPointF> firstProjected = projectedCenterFor(firstCenter);
    const std::optional<QPointF> secondProjected = projectedCenterFor(secondCenter);
    const std::optional<QPointF> headlightProjected =
        projectedCenterFor(headlightCenter);
    const std::optional<QPoint> firstIcon = onScreenPointFor(firstProjected);
    const std::optional<QPoint> secondIcon = onScreenPointFor(secondProjected);
    const std::optional<QPoint> headlightIcon =
        onScreenPointFor(headlightProjected);
    check(firstIcon && secondIcon && headlightIcon,
        "Thing-light fixture resolves regular-light and Headlight icon centers through the display-position helper");
    if (!firstIcon || !secondIcon || !headlightIcon)
    {
        const auto printCenter = [](
                                     const char* label,
                                     const std::optional<render::LightGizmoEntry>& entry,
                                     const std::optional<QVector3D>& center,
                                     const std::optional<QPointF>& projected,
                                     const std::optional<QPoint>& icon,
                                     const GlView& targetView)
        {
            std::cerr << " " << label << " entry=" << entry.has_value()
                      << " center=" << center.has_value()
                      << " projected=" << projected.has_value()
                      << " onscreen=" << icon.has_value();
            if (center)
            {
                std::cerr << " center3d=(" << center->x() << "," << center->y()
                          << "," << center->z() << ")";
            }
            if (projected)
            {
                std::cerr << " screen=(" << projected->x() << ","
                          << projected->y() << ")";
            }
            std::cerr << " viewport=" << targetView.width() << "x"
                      << targetView.height() << "\n";
        };
        std::cerr << "Thing-light gizmo center diagnostics: projection="
                  << static_cast<int>(view->cameraProjectionType()) << "\n";
        printCenter("regular owner 0", firstLight, firstCenter,
                    firstProjected, firstIcon, *view);
        printCenter("regular owner 1", secondLight, secondCenter,
                    secondProjected, secondIcon, *view);
        printCenter("Headlight owner 1", secondHeadlight, headlightCenter,
                    headlightProjected, headlightIcon, *view);
        window.hide();
        return;
    }

    clickViewportPoint(*view, *firstIcon);
    check(window.documentStateForTest().selection.things
            == std::unordered_set<std::int32_t>{ 0 }
            && view->property("sed.selectedLightGizmoKind").toInt()
                == static_cast<int>(render::LightGizmoKind::ThingRegular),
        "Thing-light mode selects its owner when the regular-light gizmo itself is clicked");

    clickViewportPoint(*view, *secondIcon, Qt::ShiftModifier);
    check(window.documentStateForTest().selection.things
            == std::unordered_set<std::int32_t>{ 0, 1 },
        "Thing-light gizmo picking preserves multi-selection with Shift-click");

    const auto projectCurrentHeadlightIcon = [view, &headlightCenter]()
        -> std::optional<QPoint>
    {
        if (!headlightCenter)
        {
            return std::nullopt;
        }
        const std::optional<QPointF> projected =
            view->projectWorldToViewport(*headlightCenter);
        if (!projected
            || projected->x() < 0.0
            || projected->x() >= static_cast<qreal>(view->width())
            || projected->y() < 0.0
            || projected->y() >= static_cast<qreal>(view->height()))
        {
            return std::nullopt;
        }
        return QPoint(qRound(projected->x()), qRound(projected->y()));
    };
    const std::optional<QPoint> headlightAddPoint = projectCurrentHeadlightIcon();
    check(headlightAddPoint.has_value(),
        "Headlight remains on-screen after the first Thing-light selection");
    if (headlightAddPoint)
    {
        clickViewportPoint(*view, *headlightAddPoint, Qt::ShiftModifier);
    }
    check(window.documentStateForTest().selection.things
            == std::unordered_set<std::int32_t>{ 0, 1 }
            && view->property("sed.selectedLightGizmoKind").toInt()
                == static_cast<int>(render::LightGizmoKind::ThingHead),
        "Thing-light mode selects the owner when its Headlight gizmo itself is clicked");

    std::optional<std::array<std::int32_t, 4>> controlClickPicked{};
    std::optional<std::array<std::int32_t, 5>> controlSelectionPicked{};
    const QMetaObject::Connection controlClickConnection = QObject::connect(
        view,
        &GlView::clickPicked,
        [&controlClickPicked](
            const std::int32_t sectorIndex,
            const std::int32_t surfaceIndex,
            const std::int32_t thingIndex,
            const std::int32_t lightIndex)
        {
            controlClickPicked = std::array<std::int32_t, 4>{
                sectorIndex, surfaceIndex, thingIndex, lightIndex };
        });
    const QMetaObject::Connection controlSelectionConnection = QObject::connect(
        view,
        &GlView::selectionPicked,
        [&controlSelectionPicked](
            const std::int32_t sectorIndex,
            const std::int32_t surfaceIndex,
            const std::int32_t thingIndex,
            const std::int32_t lightIndex,
            const Qt::KeyboardModifiers modifiers)
        {
            controlSelectionPicked = std::array<std::int32_t, 5>{
                sectorIndex,
                surfaceIndex,
                thingIndex,
                lightIndex,
                static_cast<std::int32_t>(modifiers.toInt()) };
        });
    const std::optional<QPoint> headlightControlPoint = projectCurrentHeadlightIcon();
    check(headlightControlPoint.has_value(),
        "Headlight remains on-screen before Control-deselecting its owner");
    if (headlightControlPoint)
    {
        clickViewportPoint(*view, *headlightControlPoint, Qt::ControlModifier);
    }
    QObject::disconnect(controlClickConnection);
    QObject::disconnect(controlSelectionConnection);
    const auto selectionAfterControlPick =
        window.documentStateForTest().selection.things;
    if (selectionAfterControlPick != std::unordered_set<std::int32_t>{ 0 })
    {
        const QPoint diagnosticPoint = headlightControlPoint.value_or(*headlightIcon);
        const std::optional<float> sampledHeadlightSceneDepth =
            view->architecturalLightingSceneDepthForTest(QPointF(diagnosticPoint));
        const std::optional<QPoint> freshHeadlightPoint = projectCurrentHeadlightIcon();
        std::cerr << "Thing-light Control-pick diagnostics: selected=";
        for (const std::int32_t owner : selectionAfterControlPick)
        {
            std::cerr << owner << ',';
        }
        std::cerr << " clickedHeadlight=(" << diagnosticPoint.x() << ','
                  << diagnosticPoint.y() << ") originalHeadlight=("
                  << headlightIcon->x() << ',' << headlightIcon->y()
                  << ") freshHeadlight=";
        if (freshHeadlightPoint)
        {
            std::cerr << '(' << freshHeadlightPoint->x() << ','
                      << freshHeadlightPoint->y() << ')';
        }
        else
        {
            std::cerr << "offscreen";
        }
        std::cerr << " headCenter=("
                  << headlightCenter->x() << ',' << headlightCenter->y() << ','
                  << headlightCenter->z() << ") activeKind="
                  << view->property("sed.selectedLightGizmoKind").toInt()
                  << " sceneDepth=";
        if (sampledHeadlightSceneDepth)
        {
            std::cerr << *sampledHeadlightSceneDepth;
        }
        else
        {
            std::cerr << "none";
        }
        std::cerr << " addModifiers=" << view->selectionAddModifiers().toInt()
                  << " removeModifiers=" << view->selectionRemoveModifiers().toInt()
                  << " clickPicked=";
        if (controlClickPicked)
        {
            for (const std::int32_t index : *controlClickPicked)
            {
                std::cerr << index << ':';
            }
        }
        else
        {
            std::cerr << "none";
        }
        std::cerr << " selectionPicked=";
        if (controlSelectionPicked)
        {
            for (const std::int32_t index : *controlSelectionPicked)
            {
                std::cerr << index << ':';
            }
        }
        else
        {
            std::cerr << "none";
        }
        std::cerr << "\n";
    }
    check(selectionAfterControlPick
            == std::unordered_set<std::int32_t>{ 0 },
        "Thing-light gizmo picking removes only the clicked owner with Control-click");

    const std::optional<QPointF> bodyScreen =
        view->projectWorldToViewport(QVector3D{ 0.5f, 0.3f, 1.0f });
    if (bodyScreen)
    {
        clickViewportPoint(
            *view,
            QPoint(qRound(bodyScreen->x()), qRound(bodyScreen->y())));
    }
    check(bodyScreen
            && window.documentStateForTest().selection.things
                == std::unordered_set<std::int32_t>{ 0 }
            && view->property("sed.selectedLightGizmoKind").toInt()
                == static_cast<int>(render::LightGizmoKind::ThingRegular),
        "Thing-light mode retains regular-light owner-body picking as a fallback");
    window.hide();
}

void runArchitecturalLightingExactSelectionPopupTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setPrimaryResourceOverrideForTest(
        "arch_picking_fixture.3do",
        []()
        {
            return std::optional<std::vector<std::byte>>(
                pickingModelFixtureBytes());
        });
    window.setDocumentStateForTest(editor::DocumentState{
        makeClosedDoorPairLevel(),
        {} });
    window.setProjectTypeForTest(ProjectType::MOTS);
    window.resize(640, 480);
    window.show();
    QApplication::processEvents(QEventLoop::AllEvents);
    const bool initialViewRefitSettled = waitForEditorViewRefitToSettle(window);
    check(initialViewRefitSettled,
        "ARCHLIGHTING fixture waits for the MainWindow startup camera refit");
    if (!initialViewRefitSettled)
    {
        window.hide();
        return;
    }

    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING exact-selection fixture exposes the viewport");
    if (!view)
    {
        return;
    }

    view->setShowThings(true);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->setShowThingGizmos(false);
    view->setSurfaceClickSelectionEnabled(true);
    view->setTransformTool(GlView::TransformTool::Select);
    view->setResourceLocator(&window.resourceLocatorForTest(), false);
    view->primeModel3do("arch_picking_fixture.3do", makeClosedDoorModel());
    view->setLevel(&window.mutableDocumentStateForTest().level);
    view->setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view->setOrbitViewImmediate(32.0f, 42.0f, 0.0f);
    view->frameWorldBounds(
        QVector3D{ -1.2f, -0.8f, -1.2f },
        QVector3D{  6.2f,  1.4f,  3.2f },
        false);
    const QVector3D fixtureCameraEye = view->cameraEye();
    const QVector3D fixtureCameraTarget = view->cameraTarget();
    window.activateArchitecturalLightingEditModeForTest();

    editor::Selection selectedOwners{};
    selectedOwners.things.insert(0);
    selectedOwners.things.insert(1);
    window.mutableDocumentStateForTest().selection = selectedOwners;
    view->setSelectionState(-1, -1, 0, -1, selectedOwners);
    view->setSelectedThing(0);
    view->update();
    QApplication::processEvents(QEventLoop::AllEvents);
    const bool cameraStableAfterOwnerPublication =
        (view->cameraEye() - fixtureCameraEye).lengthSquared() <= 1.0e-8f
        && (view->cameraTarget() - fixtureCameraTarget).lengthSquared() <= 1.0e-8f
        && !window.pendingEditorViewRefitForTest();
    check(cameraStableAfterOwnerPublication,
        "ARCHLIGHTING fixture camera remains fixed after owner selection publication");
    if (!cameraStableAfterOwnerPublication)
    {
        view->hide();
        return;
    }

    const auto firstOwnerVertices = view->architecturalLightingVerticesForThing(0);
    const auto secondOwnerVertices = view->architecturalLightingVerticesForThing(1);
    const auto firstTargetIt = std::find_if(
        firstOwnerVertices.begin(),
        firstOwnerVertices.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return vertex.meshIndex == 0 && vertex.vertexIndex == 0;
        });
    const auto secondTargetIt = std::find_if(
        secondOwnerVertices.begin(),
        secondOwnerVertices.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return vertex.meshIndex == 0 && vertex.vertexIndex == 1;
        });
    const auto firstAnchor = view->architecturalLightingOwnerGizmoDisplayPositionForTest(0);
    const auto secondAnchor = view->architecturalLightingOwnerGizmoDisplayPositionForTest(1);
    check(firstTargetIt != firstOwnerVertices.end()
            && secondTargetIt != secondOwnerVertices.end()
            && firstAnchor.has_value()
            && secondAnchor.has_value(),
        "ARCHLIGHTING exact-selection fixture publishes two owner markers and target vertices");
    if (firstTargetIt == firstOwnerVertices.end()
        || secondTargetIt == secondOwnerVertices.end()
        || !firstAnchor
        || !secondAnchor)
    {
        view->hide();
        return;
    }

    const GlView::ArchitecturalLightingRenderVertex firstTarget = *firstTargetIt;
    const GlView::ArchitecturalLightingRenderVertex secondTarget = *secondTargetIt;

    // Build the implicit all-vertex selection through the actual owner-gizmo
    // route. Both owners are already selected, so Shift preserves the owner
    // set while each click expands one owner into its complete vertex set.
    // Unified ARCH input resolves owner markers independently of the legacy
    // target enum; keep the real workflow in its vertex-first state.
    view->setArchitecturalLightingTargetMode(
        GlView::ArchitecturalLightingTargetMode::Vertices);
    const std::optional<std::int32_t> firstOwnerPick = clickArchitecturalOwner(
        *view,
        *firstAnchor,
        true,
        false,
        Qt::ShiftModifier);
    const std::optional<std::int32_t> secondOwnerPick = clickArchitecturalOwner(
        *view,
        *secondAnchor,
        true,
        false,
        Qt::ShiftModifier);
    check(firstOwnerPick && *firstOwnerPick == 0
            && secondOwnerPick && *secondOwnerPick == 1
            && view->architecturalLightingSelectedVertices().size() == 16,
        "ARCHLIGHTING owner clicks create the complete multi-owner vertex selection");

    const GlView::ArchitecturalLightingVertexIdentity firstIdentity{
        firstTarget.thingIndex,
        firstTarget.lodIndex,
        firstTarget.meshIndex,
        firstTarget.vertexIndex};
    const GlView::ArchitecturalLightingVertexIdentity secondIdentity{
        secondTarget.thingIndex,
        secondTarget.lodIndex,
        secondTarget.meshIndex,
        secondTarget.vertexIndex};
    const auto hasIdentity = [view](
                                  const GlView::ArchitecturalLightingVertexIdentity identity)
    {
        return std::ranges::find(
                   view->architecturalLightingSelectedVertices(), identity)
            != view->architecturalLightingSelectedVertices().end();
    };
    const auto pickedIdentityMatches = [](
                                           const std::optional<PickedVertex>& picked,
                                           const GlView::ArchitecturalLightingVertexIdentity expected)
    {
        return picked
            && picked->thingIndex == expected.thingIndex
            && picked->lodIndex == expected.lodIndex
            && picked->meshIndex == expected.meshIndex
            && picked->vertexIndex == expected.vertexIndex;
    };

    const std::optional<PickedVertex> removedPick = clickArchitecturalVertex(
        *view,
        secondTarget.worldPosition,
        Qt::ControlModifier);
    const bool controlRemovalPass =
        pickedIdentityMatches(removedPick, secondIdentity)
        && view->architecturalLightingSelectedVertices().size() == 15
        && hasIdentity(firstIdentity)
        && !hasIdentity(secondIdentity);
    check(controlRemovalPass,
        "ARCHLIGHTING Control-click removes one vertex from an implicit owner expansion");

    // Recreate the implicit owner expansion through the real marker route so
    // the following plain-click and explicit-batch assertions exercise both
    // selection origins in one workflow.
    const std::optional<std::int32_t> restoredFirstOwner =
        clickArchitecturalOwner(
            *view,
            *firstAnchor,
            true,
            false,
            Qt::ShiftModifier);
    const std::optional<std::int32_t> restoredSecondOwner =
        clickArchitecturalOwner(
            *view,
            *secondAnchor,
            true,
            false,
            Qt::ShiftModifier);
    const bool restoredOwnerExpansionPass =
        restoredFirstOwner && *restoredFirstOwner == 0
            && restoredSecondOwner && *restoredSecondOwner == 1
            && view->architecturalLightingSelectedVertices().size() == 16;
    check(restoredOwnerExpansionPass,
        "ARCHLIGHTING owner markers restore the complete implicit multi-owner set");

    // A plain vertex click must discard the implicit owner expansion and keep
    // exactly the clicked vertex. The old path sees that vertex already in
    // the set and incorrectly preserves every vertex from both owners.
    const std::optional<PickedVertex> plainPick = clickArchitecturalVertex(
        *view,
        firstTarget.worldPosition);
    const bool plainVertexPass =
        pickedIdentityMatches(plainPick, firstIdentity)
        && view->architecturalLightingSelectedVertices().size() == 1
        && hasIdentity(firstIdentity);
    check(plainVertexPass,
        "ARCHLIGHTING plain vertex click collapses an owner-expanded selection to one exact vertex");

    const std::optional<PickedVertex> shiftPick = clickArchitecturalVertex(
        *view,
        secondTarget.worldPosition,
        Qt::ShiftModifier);
    const bool shiftVertexPass =
        pickedIdentityMatches(shiftPick, secondIdentity)
        && view->architecturalLightingSelectedVertices().size() == 2
        && hasIdentity(firstIdentity)
        && hasIdentity(secondIdentity);
    check(shiftVertexPass,
        "ARCHLIGHTING Shift-click adds one exact vertex across selected owners");

    const auto recordBefore = window.documentStateForTest().level.motsArchLighting;
    const std::optional<QPointF> secondScreen =
        view->projectWorldToViewport(secondTarget.worldPosition);
    bool exactPopupOpened = false;
    if (secondScreen)
    {
        const QPoint point(
            qRound(secondScreen->x()),
            qRound(secondScreen->y()));
        QMouseEvent firstPress(
            QEvent::MouseButtonPress,
            QPointF(point),
            QPointF(point),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(view, &firstPress);
        QMouseEvent firstRelease(
            QEvent::MouseButtonRelease,
            QPointF(point),
            QPointF(point),
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(view, &firstRelease);
        QMouseEvent doubleClick(
            QEvent::MouseButtonDblClick,
            QPointF(point),
            QPointF(point),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(view, &doubleClick);
        QMouseEvent doubleClickRelease(
            QEvent::MouseButtonRelease,
            QPointF(point),
            QPointF(point),
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(view, &doubleClickRelease);
        QApplication::processEvents(QEventLoop::AllEvents);
        exactPopupOpened = window.architecturalLightingVertexDialogForTest()
            && window.architecturalLightingSelectedVertexCountForTest() == 2
            && view->architecturalLightingSelectedVertices().size() == 2;
    }
    check(exactPopupOpened,
        "ARCHLIGHTING vertex double-click opens a popup for the exact selected subset");

    if (exactPopupOpened)
    {
        const MotsArchLightVertex replacement{
            0.45f, 0.82f, 0.73f, 0.64f };
        check(window.stageArchitecturalLightingSelectedVerticesForTest(
                    replacement,
                    level::MotsArchLightingVertexMode::Rgb)
                && window.commitArchitecturalLightingVertexForTest(),
            "ARCHLIGHTING exact popup commits the selected subset");
        const auto& after = window.documentStateForTest().level.motsArchLighting;
        bool untouched = after.size() == recordBefore.size();
        for (std::size_t recordIndex = 0;
             untouched && recordIndex < after.size();
             ++recordIndex)
        {
            const auto& beforeMeshes = recordBefore[recordIndex].meshes;
            const auto& afterMeshes = after[recordIndex].meshes;
            if (beforeMeshes.size() != afterMeshes.size())
            {
                untouched = false;
                break;
            }
            for (std::size_t meshIndex = 0;
                 untouched && meshIndex < afterMeshes.size();
                 ++meshIndex)
            {
                if (beforeMeshes[meshIndex].vertices.size()
                    != afterMeshes[meshIndex].vertices.size())
                {
                    untouched = false;
                    break;
                }
                for (std::size_t vertexIndex = 0;
                     vertexIndex < afterMeshes[meshIndex].vertices.size();
                     ++vertexIndex)
                {
                    const bool selected =
                        (recordIndex == 0 && meshIndex == 0 && vertexIndex == 0)
                        || (recordIndex == 1 && meshIndex == 0 && vertexIndex == 1);
                    const MotsArchLightVertex& actual =
                        afterMeshes[meshIndex].vertices[vertexIndex];
                    const MotsArchLightVertex& expected = selected
                        ? replacement
                        : beforeMeshes[meshIndex].vertices[vertexIndex];
                    const auto sameFloat = [](const float left, const float right)
                    {
                        return std::bit_cast<std::uint32_t>(left)
                            == std::bit_cast<std::uint32_t>(right);
                    };
                    if (!sameFloat(actual.mono, expected.mono)
                        || !sameFloat(actual.red, expected.red)
                        || !sameFloat(actual.green, expected.green)
                        || !sameFloat(actual.blue, expected.blue))
                    {
                        untouched = false;
                        break;
                    }
                }
            }
        }
        check(untouched,
            "ARCHLIGHTING exact popup commit leaves every unselected vertex unchanged");
    }
    window.cancelArchitecturalLightingVertexForTest();
    if (QDialog* const activePopup =
            window.architecturalLightingVertexDialogForTest())
    {
        activePopup->close();
    }
    QApplication::processEvents(QEventLoop::AllEvents);
    const auto postPopupAnchor =
        view->architecturalLightingOwnerGizmoDisplayPositionForTest(0);
    const QVector3D ownerAnchorForBatch = postPopupAnchor.value_or(*firstAnchor);
    const std::optional<QPointF> ownerClickScreenBeforePopupPick =
        view->projectWorldToViewport(ownerAnchorForBatch);
    const QPoint ownerClickPointBeforePopupPick = ownerClickScreenBeforePopupPick
        ? QPoint(qRound(ownerClickScreenBeforePopupPick->x()),
                 qRound(ownerClickScreenBeforePopupPick->y()))
        : QPoint{};
    const std::optional<float> ownerIconDepthBeforePopupPick =
        projectedWindowDepthForDiagnostic(*view, ownerAnchorForBatch);
    const std::optional<float> ownerSceneDepthBeforePopupPick =
        view->architecturalLightingSceneDepthForTest(
            QPointF(ownerClickPointBeforePopupPick));
    const bool pickingEnabledBeforePopupPick = view->viewportPickingEnabled();
    const bool editingActiveBeforePopupPick =
        view->architecturalLightingEditingActive();
    const int pickModeBeforePopupPick = static_cast<int>(view->pickMode());
    const std::size_t ownerEntriesBeforePopupPick =
        view->architecturalLightingOwnerGizmoEntryCountForTest();
    const bool popupPresentBeforeOwnerPick =
        window.architecturalLightingVertexDialogForTest() != nullptr;

    // An owner-gizmo double-click is the explicit whole-owner operation. With
    // both Things selected, it must expand both owners before opening the
    // popup even if the current exact vertex set is only a subset.
    view->setArchitecturalLightingTargetMode(
        GlView::ArchitecturalLightingTargetMode::Vertices);
    QPoint ownerFirstReleasePoint(-1, -1);
    const std::optional<std::int32_t> ownerForBatch = postPopupAnchor
        ? clickArchitecturalOwner(
              *view,
              ownerAnchorForBatch,
              true,
              false,
              Qt::ShiftModifier,
              &ownerFirstReleasePoint)
        : std::nullopt;
    const std::optional<std::int32_t> handoffAfterOwnerClick =
        view->architecturalLightingLastOwnerClickForTest();
    const std::size_t selectedVerticesAfterOwnerClick =
        view->architecturalLightingSelectedVertices().size();
    const editor::Selection selectionAfterOwnerClick =
        window.documentStateForTest().selection;
    int ownerBatchEditRequests = 0;
    const QMetaObject::Connection ownerBatchConnection = QObject::connect(
        view,
        &GlView::architecturalLightingVertexEditRequested,
        [&ownerBatchEditRequests](
            const std::int32_t thingIndex,
            const std::int32_t,
            const std::int32_t,
            const std::int32_t,
            const Qt::KeyboardModifiers)
        {
            if (thingIndex == 0)
            {
                ++ownerBatchEditRequests;
            }
        });
    const std::optional<QPointF> ownerScreen =
        view->projectWorldToViewport(ownerAnchorForBatch);
    const std::optional<std::int32_t> ownerLastClickBeforeDouble =
        view->architecturalLightingLastOwnerClickForTest();
    QPoint ownerDoubleClickPoint(-1, -1);
    if (ownerScreen)
    {
        const QPoint projectedPoint(
            qRound(ownerScreen->x()),
            qRound(ownerScreen->y()));
        const bool cameraStableForOwnerDoubleClick =
            projectedPoint == ownerFirstReleasePoint
            && (view->cameraEye() - fixtureCameraEye).lengthSquared() <= 1.0e-8f
            && (view->cameraTarget() - fixtureCameraTarget).lengthSquared() <= 1.0e-8f
            && !window.pendingEditorViewRefitForTest();
        check(cameraStableForOwnerDoubleClick,
            "ARCHLIGHTING owner marker stays at the first click pixel after selection event processing");
        ownerDoubleClickPoint = ownerFirstReleasePoint;
        const QPoint point = ownerFirstReleasePoint;
        QMouseEvent doubleClick(
            QEvent::MouseButtonDblClick,
            QPointF(point),
            QPointF(point),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(view, &doubleClick);
        QMouseEvent doubleClickRelease(
            QEvent::MouseButtonRelease,
            QPointF(point),
            QPointF(point),
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(view, &doubleClickRelease);
        QApplication::processEvents(QEventLoop::AllEvents);
    }
    QObject::disconnect(ownerBatchConnection);
    const std::size_t viewportBatchSelectionCount =
        view->architecturalLightingSelectedVertices().size();
    const std::size_t popupBatchSelectionCount =
        window.architecturalLightingSelectedVertexCountForTest();
    const bool batchPopupPresent =
        window.architecturalLightingVertexDialogForTest() != nullptr;
    const bool ownerBatchPass = ownerForBatch && *ownerForBatch == 0
        && ownerBatchEditRequests == 1
        && batchPopupPresent
        && viewportBatchSelectionCount == 16
        && popupBatchSelectionCount == 16;
    if (!ownerBatchPass)
    {
        const std::optional<QPointF> firstAnchorScreen =
            view->projectWorldToViewport(*firstAnchor);
        const std::optional<QPointF> secondAnchorScreen =
            view->projectWorldToViewport(*secondAnchor);
        const auto firstCachedBounds =
            view->architecturalLightingOwnerGizmoBoundsForTest(0);
        const auto secondCachedBounds =
            view->architecturalLightingOwnerGizmoBoundsForTest(1);
        std::cerr << std::setprecision(10)
                  << "ARCHLIGHTING owner-batch anchors: first3d=("
                  << firstAnchor->x() << ',' << firstAnchor->y() << ','
                  << firstAnchor->z() << ") firstScreen=";
        if (firstAnchorScreen)
        {
            std::cerr << '(' << firstAnchorScreen->x() << ','
                      << firstAnchorScreen->y() << ')';
        }
        else
        {
            std::cerr << "none";
        }
        std::cerr << " second3d=(" << secondAnchor->x() << ','
                  << secondAnchor->y() << ',' << secondAnchor->z()
                  << ") secondScreen=";
        if (secondAnchorScreen)
        {
            std::cerr << '(' << secondAnchorScreen->x() << ','
                      << secondAnchorScreen->y() << ')';
        }
        else
        {
            std::cerr << "none";
        }
        std::cerr << " cameraUp=(" << view->cameraUp().x() << ','
                  << view->cameraUp().y() << ',' << view->cameraUp().z()
                  << ") firstBounds=";
        if (firstCachedBounds)
        {
            std::cerr << '(' << firstCachedBounds->min.x() << ','
                      << firstCachedBounds->min.y() << ','
                      << firstCachedBounds->min.z() << ")->("
                      << firstCachedBounds->max.x() << ','
                      << firstCachedBounds->max.y() << ','
                      << firstCachedBounds->max.z() << ')';
        }
        else
        {
            std::cerr << "none";
        }
        std::cerr << " secondBounds=";
        if (secondCachedBounds)
        {
            std::cerr << '(' << secondCachedBounds->min.x() << ','
                      << secondCachedBounds->min.y() << ','
                      << secondCachedBounds->min.z() << ")->("
                      << secondCachedBounds->max.x() << ','
                      << secondCachedBounds->max.y() << ','
                      << secondCachedBounds->max.z() << ')';
        }
        else
        {
            std::cerr << "none";
        }
        std::cerr << '\n';
        std::cerr << "ARCHLIGHTING owner-batch failure: ownerForBatch="
                  << (ownerForBatch ? std::to_string(*ownerForBatch) : "none")
                  << " handoffAfterClick="
                  << (handoffAfterOwnerClick
                          ? std::to_string(*handoffAfterOwnerClick) : "none")
                  << " selectedThings="
                  << selectionAfterOwnerClick.things.contains(0) << ','
                  << selectionAfterOwnerClick.things.contains(1)
                  << " selectedVerticesAfterClick="
                  << selectedVerticesAfterOwnerClick
                  << " requests=" << ownerBatchEditRequests << " viewport="
                  << viewportBatchSelectionCount << " popup="
                  << popupBatchSelectionCount << " first="
                  << ownerFirstReleasePoint.x() << ',' << ownerFirstReleasePoint.y()
                  << " second=" << ownerDoubleClickPoint.x() << ','
                  << ownerDoubleClickPoint.y() << " handoff="
                  << (ownerLastClickBeforeDouble
                          ? std::to_string(*ownerLastClickBeforeDouble) : "none")
                  << " preClickPickMode=" << pickModeBeforePopupPick
                  << " preClickPickingEnabled=" << pickingEnabledBeforePopupPick
                  << " preClickEditingActive=" << editingActiveBeforePopupPick
                  << " preClickGizmoEntries=" << ownerEntriesBeforePopupPick
                  << " preClickPopupPresent=" << popupPresentBeforeOwnerPick
                  << " preClickScreen="
                  << (ownerClickScreenBeforePopupPick ? "present" : "missing")
                  << " preClickPoint=" << ownerClickPointBeforePopupPick.x() << ','
                  << ownerClickPointBeforePopupPick.y()
                  << " iconDepth=";
        if (ownerIconDepthBeforePopupPick) std::cerr << *ownerIconDepthBeforePopupPick;
        else std::cerr << "n/a";
        std::cerr << " sceneDepth=";
        if (ownerSceneDepthBeforePopupPick) std::cerr << *ownerSceneDepthBeforePopupPick;
        else std::cerr << "n/a";
        std::cerr << " cameraStable="
                  << ((view->cameraEye() - fixtureCameraEye).lengthSquared() <= 1.0e-8f
                      && (view->cameraTarget() - fixtureCameraTarget).lengthSquared() <= 1.0e-8f)
                  << '\n';
    }
    check(ownerBatchPass,
        "ARCHLIGHTING owner-gizmo double-click expands all selected owners before opening the batch popup");
    window.cancelArchitecturalLightingVertexForTest();
    const bool fixtureCameraStayedStable =
        (view->cameraEye() - fixtureCameraEye).lengthSquared() <= 1.0e-8f
        && (view->cameraTarget() - fixtureCameraTarget).lengthSquared() <= 1.0e-8f
        && !window.pendingEditorViewRefitForTest();
    check(fixtureCameraStayedStable,
        "ARCHLIGHTING selection and popup workflow leaves the stabilized fixture camera unchanged");
    view->hide();
}

void runArchitecturalVirtualDefaultOwnerPickTests()
{
    Level level = makeClosedDoorLevel();
    Thing unbound = level.things.front();
    unbound.x = 4.0;
    unbound.archLightingIndex = -1;
    level.things.push_back(std::move(unbound));

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setPrimaryResourceOverrideForTest(
        "arch_picking_fixture.3do",
        []()
        {
            return std::optional<std::vector<std::byte>>(
                pickingModelFixtureBytes());
        });
    window.setDocumentStateForTest(editor::DocumentState{ std::move(level), {} });
    window.setProjectTypeForTest(ProjectType::MOTS);
    window.resize(640, 480);
    window.show();
    QApplication::processEvents(QEventLoop::AllEvents);

    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING virtual-default fixture exposes the MainWindow viewport");
    if (!view)
    {
        return;
    }

    view->setShowThings(true);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->setShowThingGizmos(false);
    view->setSurfaceClickSelectionEnabled(true);
    view->setTransformTool(GlView::TransformTool::Select);
    view->setResourceLocator(&window.resourceLocatorForTest(), false);
    view->primeModel3do("arch_picking_fixture.3do", makeClosedDoorModel());
    view->setLevel(&window.mutableDocumentStateForTest().level);
    view->setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view->setOrbitViewImmediate(32.0f, 42.0f, 0.0f);
    view->frameWorldBounds(
        QVector3D{ -1.2f, -0.8f, -1.2f },
        QVector3D{  6.2f,  1.4f,  3.2f },
        false);
    window.activateArchitecturalLightingEditModeForTest();
    view->setArchitecturalLightingTargetMode(
        GlView::ArchitecturalLightingTargetMode::Owner);
    editor::Selection noSelection{};
    window.mutableDocumentStateForTest().selection = noSelection;
    view->setSelectionState(-1, -1, -1, -1, noSelection);
    view->setSelectedThing(-1);
    view->update();
    QApplication::processEvents(QEventLoop::AllEvents);

    const Level& before = window.documentStateForTest().level;
    const bool ownerEligible = before.things.size() == 2
        && before.things[0].archLightingIndex == 0
        && before.things[1].archLightingIndex == -1
        && before.motsArchLighting.size() == 1
        && sed::level::isMotsArchThing(before, before.things[1]);
    QAction* const addAction = window.findChild<QAction*>(
        QStringLiteral("viewport.arch_lighting.add"));
    const auto marker = view->architecturalLightingOwnerGizmoDisplayPositionForTest(1);
    check(ownerEligible && addAction == nullptr && marker.has_value()
            && !view->archThingGizmoDisplayPositionForTest(1).has_value(),
        "an unbound effective MotS Arch Thing exposes one ARCH marker with no Add action");

    const std::size_t undoBefore = window.undoDepthForTest();
    const std::optional<std::int32_t> pickedOwner = marker
        ? clickArchitecturalOwner(*view, *marker)
        : std::nullopt;
    check(pickedOwner && *pickedOwner == 1
            && window.documentStateForTest().selection.things
                == std::unordered_set<std::int32_t>{ 1 }
            && window.documentStateForTest().level.things[1].archLightingIndex == -1
            && window.documentStateForTest().level.motsArchLighting.size() == 1
            && window.undoDepthForTest() == undoBefore,
        "clicking the virtual-default ARCH marker selects its owner without materializing data or history");
    const auto selectedVirtualVertices = view->architecturalLightingVerticesForThing(1);
    check(!selectedVirtualVertices.empty()
            && std::all_of(selectedVirtualVertices.begin(), selectedVirtualVertices.end(),
                [](const GlView::ArchitecturalLightingRenderVertex& vertex)
                {
                    return vertex.lighting == MotsArchLightVertex{};
                }),
        "selecting an unbound effective MotS Arch Thing publishes every model vertex at zero");
    view->hide();
}

void runArchitecturalLightingTargetSelectionTests()
{
    Level level = makePickingLevel(QVector3D{ 0.0f, 0.0f, 0.0f }, false);
    assets::ResourceLocator resources{};
    GlView view{};
    configurePickingView(
        view,
        level,
        resources,
        app::ViewportCamera::ProjectionType::Orthographic,
        false);

    const auto vertices = view.architecturalLightingVerticesForThing(0);
    const auto first = std::find_if(
        vertices.begin(),
        vertices.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return vertex.meshIndex == 0 && vertex.vertexIndex == 0;
        });
    const auto second = std::find_if(
        vertices.begin(),
        vertices.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return vertex.meshIndex == 0 && vertex.vertexIndex == 1;
        });
    const auto third = std::find_if(
        vertices.begin(),
        vertices.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return vertex.meshIndex == 0 && vertex.vertexIndex == 2;
        });
    check(first != vertices.end()
            && second != vertices.end()
            && third != vertices.end(),
        "ARCHLIGHTING target fixture publishes three authored vertices");
    if (first == vertices.end()
        || second == vertices.end()
        || third == vertices.end())
    {
        view.hide();
        return;
    }

    int selectionChanged = 0;
    int vertexPicked = 0;
    std::optional<PickedVertex> editRequested{};
    QObject::connect(
        &view,
        &GlView::architecturalLightingVertexSelectionChanged,
        [&selectionChanged]() { ++selectionChanged; });
    QObject::connect(
        &view,
        &GlView::architecturalLightingVertexPicked,
        [&vertexPicked](
            const std::int32_t,
            const std::int32_t,
            const std::int32_t,
            const std::int32_t,
            const Qt::KeyboardModifiers)
        {
            ++vertexPicked;
        });
    QObject::connect(
        &view,
        &GlView::architecturalLightingVertexEditRequested,
        [&editRequested](
            const std::int32_t thingIndex,
            const std::int32_t lodIndex,
            const std::int32_t meshIndex,
            const std::int32_t vertexIndex,
            const Qt::KeyboardModifiers)
        {
            editRequested = PickedVertex{
                thingIndex,
                lodIndex,
                meshIndex,
                vertexIndex};
        });

    view.setArchitecturalLightingTargetMode(
        GlView::ArchitecturalLightingTargetMode::Vertices);
    check(
        view.architecturalLightingTargetMode()
            == GlView::ArchitecturalLightingTargetMode::Vertices,
        "ARCHLIGHTING can enter the explicit Vertices target");

    const std::optional<PickedVertex> firstPick =
        clickArchitecturalVertex(view, first->worldPosition);
    check(
        firstPick && view.architecturalLightingSelectedVertices().size() == 1
            && view.architecturalLightingSelectedVertices().front()
                == GlView::ArchitecturalLightingVertexIdentity{
                    first->thingIndex,
                    first->lodIndex,
                    first->meshIndex,
                    first->vertexIndex},
        "ARCHLIGHTING plain vertex click replaces the exact selection set");

    const std::optional<PickedVertex> secondPick = clickArchitecturalVertex(
        view,
        second->worldPosition,
        Qt::ShiftModifier);
    check(
        secondPick && view.architecturalLightingSelectedVertices().size() == 2,
        "ARCHLIGHTING Shift-click adds an exact vertex without clearing the first");

    (void)clickArchitecturalVertex(
        view,
        first->worldPosition,
        Qt::ControlModifier);
    const auto selectedAfterRemove = view.architecturalLightingSelectedVertices();
    const bool secondOnly = selectedAfterRemove.size() == 1
        && selectedAfterRemove.front()
            == GlView::ArchitecturalLightingVertexIdentity{
                second->thingIndex,
                second->lodIndex,
                second->meshIndex,
                second->vertexIndex};
    check(secondOnly,
        "ARCHLIGHTING Control-click removes only the exact vertex");
    check(selectionChanged >= 3 && vertexPicked >= 3,
        "ARCHLIGHTING vertex clicks publish selection and low-level pick signals");

    view.setArchitecturalLightingTargetMode(
        GlView::ArchitecturalLightingTargetMode::Owner);
    int ownerClicks = 0;
    QObject::connect(
        &view,
        &GlView::clickPicked,
        [&ownerClicks](
            const std::int32_t,
            const std::int32_t,
            const std::int32_t thingIndex,
            const std::int32_t)
        {
            if (thingIndex == 0)
            {
                ++ownerClicks;
            }
        });
    const int vertexPickedBeforeOwner = vertexPicked;
    const std::optional<QVector3D> ownerAnchor =
        view.architecturalLightingOwnerGizmoDisplayPositionForTest(0);
    const QVector3D bodyInterior = first->worldPosition
        + (second->worldPosition - first->worldPosition) * 0.01f
        + (third->worldPosition - first->worldPosition) * 0.01f;
    const std::optional<QPointF> cornerScreen =
        view.projectWorldToViewport(first->worldPosition);
    const std::optional<QPointF> bodyScreen =
        view.projectWorldToViewport(bodyInterior);
    const bool bodyIsInsideVertexHitRadius = cornerScreen && bodyScreen
        && std::hypot(
            bodyScreen->x() - cornerScreen->x(),
            bodyScreen->y() - cornerScreen->y())
            <= static_cast<qreal>(view.architecturalLightingMarkerHitRadiusPx());
    check(bodyIsInsideVertexHitRadius,
        "ARCHLIGHTING Owner body fixture lies just inside the vertex marker hit radius");

    const std::optional<std::int32_t> markerPicked = ownerAnchor
        ? clickArchitecturalOwner(view, *ownerAnchor)
        : std::nullopt;
    check(markerPicked && *markerPicked == 0,
        "ARCHLIGHTING Owner target selects the displayed owner marker");
    const std::optional<std::int32_t> bodyPicked =
        clickArchitecturalOwner(view, bodyInterior);
    check(bodyPicked && *bodyPicked == 0 && ownerClicks == 2,
        "ARCHLIGHTING unified input emits the owner pick for marker and body clicks");
    check(vertexPicked == vertexPickedBeforeOwner + 1,
        "ARCHLIGHTING unified input gives the nearby visible vertex the first chance");

    view.setArchitecturalLightingTargetMode(
        GlView::ArchitecturalLightingTargetMode::Vertices);
    const std::optional<QPointF> secondScreenForDoubleClick =
        view.projectWorldToViewport(second->worldPosition);
    if (secondScreenForDoubleClick)
    {
        const QPoint point(
            qRound(secondScreenForDoubleClick->x()),
            qRound(secondScreenForDoubleClick->y()));
        QMouseEvent doubleClick(
            QEvent::MouseButtonDblClick,
            QPointF(point),
            QPointF(point),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(&view, &doubleClick);
        QMouseEvent doubleClickRelease(
            QEvent::MouseButtonRelease,
            QPointF(point),
            QPointF(point),
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(&view, &doubleClickRelease);
    }
    check(
        editRequested
            && editRequested->thingIndex == second->thingIndex
            && editRequested->lodIndex == second->lodIndex
            && editRequested->meshIndex == second->meshIndex
            && editRequested->vertexIndex == second->vertexIndex,
        "ARCHLIGHTING double-click requests editing after selection");

    const std::optional<QPointF> firstScreen =
        view.projectWorldToViewport(first->worldPosition);
    const std::optional<QPointF> secondScreen =
        view.projectWorldToViewport(second->worldPosition);
    if (firstScreen && secondScreen)
    {
        view.setXraySelectionCyclingEnabled(true);
        const QPoint start(
            qFloor(std::min(firstScreen->x(), secondScreen->x())) - 8,
            qFloor(std::min(firstScreen->y(), secondScreen->y())) - 8);
        const QPoint finish(
            qCeil(std::max(firstScreen->x(), secondScreen->x())) + 8,
            qCeil(std::max(firstScreen->y(), secondScreen->y())) + 8);
        view.clearArchitecturalLightingSelectedVertices();
        QMouseEvent boxPress(
            QEvent::MouseButtonPress,
            QPointF(start),
            QPointF(start),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(&view, &boxPress);
        QMouseEvent boxMove(
            QEvent::MouseMove,
            QPointF(finish),
            QPointF(finish),
            Qt::NoButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(&view, &boxMove);
        QMouseEvent boxRelease(
            QEvent::MouseButtonRelease,
            QPointF(finish),
            QPointF(finish),
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(&view, &boxRelease);
        check(view.architecturalLightingSelectedVertices().size() >= 2,
            "ARCHLIGHTING Vertices marquee selects exact visible identities");

        view.setArchitecturalLightingTargetMode(
            GlView::ArchitecturalLightingTargetMode::Vertices);
        QMouseEvent interruptedPress(
            QEvent::MouseButtonPress,
            QPointF(start),
            QPointF(start),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(&view, &interruptedPress);
        QApplication::sendEvent(&view, &boxMove);
        check(view.boxSelectActiveForTest() && view.boxSelectVisibleForTest(),
            "ARCHLIGHTING Vertices marquee exposes its active drag state");
        view.setArchitecturalLightingTargetMode(
            GlView::ArchitecturalLightingTargetMode::Owner);
        check(!view.boxSelectActiveForTest() && !view.boxSelectVisibleForTest(),
            "ARCHLIGHTING target toggle cancels an interrupted marquee");
    }

    view.setArchitecturalLightingTargetMode(
        GlView::ArchitecturalLightingTargetMode::Vertices);
    (void)clickArchitecturalVertex(view, first->worldPosition);
    editor::Selection noOwners{};
    view.setSelectionState(-1, -1, -1, -1, noOwners);
    check(view.architecturalLightingSelectedVertices().empty(),
        "ARCHLIGHTING owner deselection prunes stale vertex identities");
    view.hide();
}

void runArchitecturalLightingSlopedVertexPickTests()
{
    for (const app::ViewportCamera::ProjectionType projection : {
             app::ViewportCamera::ProjectionType::Orthographic,
             app::ViewportCamera::ProjectionType::Perspective })
    {
        Level level = makePickingLevel(QVector3D{ 0.0f, 0.0f, 0.0f }, false);
        assets::ResourceLocator resources{};
        GlView view{};
        configurePickingView(view, level, resources, projection, false);
        view.primeModel3do("arch_picking_fixture.3do", makeSlopedPickingModel());
        view.setLevel(&level);
        view.setShowThings(true);
        view.setThingViewMode(GlView::ThingViewMode::Shaded);
        view.setOrbitViewImmediate(17.0f, 53.0f, 0.0f);
        view.frameWorldBounds(
            QVector3D{ -0.55f, -0.55f, -0.85f },
            QVector3D{  0.55f,  0.65f,  0.85f },
            false);
        view.setSelectedThing(0);
        view.update();
        QApplication::processEvents(QEventLoop::AllEvents);

        const auto vertices = view.architecturalLightingVerticesForThing(0);
        const auto target = std::find_if(
            vertices.begin(),
            vertices.end(),
            [](const GlView::ArchitecturalLightingRenderVertex& vertex)
            {
                return vertex.meshIndex == 0 && vertex.vertexIndex == 1;
            });
        const std::optional<QPointF> projected = target != vertices.end()
            ? view.projectWorldToViewport(target->worldPosition)
            : std::nullopt;
        const bool projectedOnscreen = projected
            && projected->x() >= 0.0
            && projected->x() < static_cast<qreal>(view.width())
            && projected->y() >= 0.0
            && projected->y() < static_cast<qreal>(view.height());
        check(target != vertices.end() && projectedOnscreen,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic sloped corner vertex is onscreen"
                : "ARCHLIGHTING perspective sloped corner vertex is onscreen");
        if (target == vertices.end() || !projectedOnscreen)
        {
            view.hide();
            continue;
        }

        // The projected point is deliberately clicked through a rounded
        // widget coordinate. The steep depth slope makes a neighbouring
        // raster sample measurably different from the CPU vertex depth; the
        // visible vertex must still agree with the rendered point marker.
        const std::optional<PickedVertex> picked =
            clickArchitecturalVertex(view, target->worldPosition);
        check(picked
                && picked->thingIndex == 0
                && picked->lodIndex == target->lodIndex
                && picked->meshIndex == target->meshIndex
                && picked->vertexIndex == target->vertexIndex,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic sloped corner vertex survives raster-depth picking"
                : "ARCHLIGHTING perspective sloped corner vertex survives raster-depth picking");
        view.hide();
    }
}

void runArchitecturalLightingObliqueMarkerRimPickTests()
{
    for (const app::ViewportCamera::ProjectionType projection : {
             app::ViewportCamera::ProjectionType::Orthographic,
             app::ViewportCamera::ProjectionType::Perspective })
    {
        Level level = makePickingLevel(QVector3D{ 0.0f, 0.0f, 0.0f }, false);
        assets::ResourceLocator resources{};
        GlView view{};
        configurePickingView(view, level, resources, projection, false);
        view.primeModel3do("arch_picking_fixture.3do", makeSlopedPickingModel());
        view.setLevel(&level);
        view.setShowThings(true);
        view.setThingViewMode(GlView::ThingViewMode::Shaded);
        view.setOrbitViewImmediate(17.0f, 53.0f, 0.0f);
        view.frameWorldBounds(
            QVector3D{ -0.55f, -0.55f, -0.85f },
            QVector3D{  0.55f,  0.65f,  0.85f },
            false);
        view.setSelectedThing(0);
        QApplication::processEvents(QEventLoop::AllEvents);

        const auto beforeOccluder = view.architecturalLightingVerticesForThing(0);
        const auto targetBefore = std::find_if(
            beforeOccluder.begin(),
            beforeOccluder.end(),
            [](const GlView::ArchitecturalLightingRenderVertex& vertex)
            {
                return vertex.meshIndex == 0 && vertex.vertexIndex == 1;
            });
        const std::optional<QPointF> targetScreen = targetBefore != beforeOccluder.end()
            ? view.projectWorldToViewport(targetBefore->worldPosition)
            : std::nullopt;
        const std::optional<QPointF> rightNeighborScreen =
            targetBefore != beforeOccluder.end()
            ? view.projectWorldToViewport(
                  targetBefore->worldPosition + view.cameraRight().normalized())
            : std::nullopt;
        const double pixelsPerWorldUnit =
            targetScreen && rightNeighborScreen
            ? std::hypot(
                  rightNeighborScreen->x() - targetScreen->x(),
                  rightNeighborScreen->y() - targetScreen->y())
            : 0.0;
        const bool validProjection = targetBefore != beforeOccluder.end()
            && targetScreen
            && rightNeighborScreen
            && std::isfinite(pixelsPerWorldUnit)
            && pixelsPerWorldUnit > 0.0;
        check(validProjection,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING oblique-rim fixture projects a sloped vertex"
                : "ARCHLIGHTING perspective-rim fixture projects a sloped vertex");
        if (!validProjection)
        {
            view.hide();
            continue;
        }

        const QVector3D forward = view.cameraForward().normalized();
        const QVector3D right = view.cameraRight().normalized();
        const QVector3D up = view.cameraUp().normalized();
        const QVector3D blockerCenter =
            targetBefore->worldPosition - forward * 0.01f;
        const float blockerHalfSize =
            static_cast<float>(1.5 / pixelsPerWorldUnit);
        const std::array<QVector3D, 4> blockerCorners{
            blockerCenter - right * blockerHalfSize - up * blockerHalfSize,
            blockerCenter + right * blockerHalfSize - up * blockerHalfSize,
            blockerCenter + right * blockerHalfSize + up * blockerHalfSize,
            blockerCenter - right * blockerHalfSize + up * blockerHalfSize};
        Sector& blockerSector = level.sectors.emplace_back();
        blockerSector.layer = 0;
        for (const QVector3D& corner : blockerCorners)
        {
            blockerSector.vertices.push_back(
                Vertex{corner.x(), corner.y(), corner.z()});
        }
        Surface blocker{};
        blocker.geo = static_cast<std::int32_t>(TGeoMode::Solid);
        blocker.faceFlags = FF_DoubleSided;
        blocker.normal = TVector{
            QVector3D::crossProduct(right, up).x(),
            QVector3D::crossProduct(right, up).y(),
            QVector3D::crossProduct(right, up).z()};
        blocker.vertices = { 0, 1, 2, 3 };
        blocker.texVertices.resize(blocker.vertices.size());
        blockerSector.surfaces.push_back(std::move(blocker));
        // `level` is already the published pointer. Notify the viewport after
        // mutating its synthetic geometry so the level-only GPU batches and
        // the CPU ray helpers see the same blocker.
        view.notifyLevelGeometryChanged();
        view.setSelectedThing(0);
        view.update();
        QApplication::processEvents(QEventLoop::AllEvents);

        const auto vertices = view.architecturalLightingVerticesForThing(0);
        const auto target = std::find_if(
            vertices.begin(),
            vertices.end(),
            [](const GlView::ArchitecturalLightingRenderVertex& vertex)
            {
                return vertex.meshIndex == 0 && vertex.vertexIndex == 1;
            });
        const std::optional<QPointF> projected = target != vertices.end()
            ? view.projectWorldToViewport(target->worldPosition)
            : std::nullopt;
        check(projected.has_value(),
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING oblique-rim marker remains projected after a central occluder is added"
                : "ARCHLIGHTING perspective-rim marker remains projected after a central occluder is added");
        if (!projected)
        {
            view.hide();
            continue;
        }
        const GlView::ArchitecturalLightingRenderVertex targetVertex = *target;

        const auto clickAt = [&view](const QPoint point)
            -> std::optional<PickedVertex>
        {
            view.setArchitecturalLightingTargetMode(
                GlView::ArchitecturalLightingTargetMode::Vertices);
            std::optional<PickedVertex> picked{};
            const QMetaObject::Connection connection = QObject::connect(
                &view,
                &GlView::architecturalLightingVertexPicked,
                [&picked](
                    const std::int32_t thingIndex,
                    const std::int32_t lodIndex,
                    const std::int32_t meshIndex,
                    const std::int32_t vertexIndex,
                    const Qt::KeyboardModifiers)
                {
                    picked = PickedVertex{
                        thingIndex, lodIndex, meshIndex, vertexIndex};
                });
            QMouseEvent press(
                QEvent::MouseButtonPress,
                QPointF(point),
                QPointF(point),
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::NoModifier);
            QApplication::sendEvent(&view, &press);
            QMouseEvent release(
                QEvent::MouseButtonRelease,
                QPointF(point),
                QPointF(point),
                Qt::LeftButton,
                Qt::NoButton,
                Qt::NoModifier);
            QApplication::sendEvent(&view, &release);
            QApplication::processEvents(QEventLoop::AllEvents);
            QObject::disconnect(connection);
            return picked;
        };

        const QPoint center(
            qRound(projected->x()),
            qRound(projected->y()));
        const std::optional<float> blockerDepth =
            view.architecturalLightingSceneDepthForTest(QPointF(center));
        const std::optional<float> targetDepth =
            projectedWindowDepthForDiagnostic(view, targetVertex.worldPosition);
        const bool blockerPublishedBeforeTarget = blockerDepth && targetDepth
            && *blockerDepth < *targetDepth;
        check(blockerPublishedBeforeTarget,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic fixture publishes its foreground blocker to the pick depth pass"
                : "ARCHLIGHTING perspective fixture publishes its foreground blocker to the pick depth pass");
        const std::optional<PickedVertex> centerPick = clickAt(center);
        check(!centerPick,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING ordinary orthographic click rejects the small real foreground occluder"
                : "ARCHLIGHTING ordinary perspective click rejects the small real foreground occluder");
        if (centerPick)
        {
            bool sampledSurfaceHit = false;
            const std::optional<QVector3D> sampledSurface =
                view.placementPointForViewport(
                    center, &sampledSurfaceHit, nullptr, nullptr, true, false);
            printArchitecturalDepthDiagnostic(
                projection == app::ViewportCamera::ProjectionType::Orthographic
                    ? "ARCHLIGHTING orthographic occluder depth"
                    : "ARCHLIGHTING perspective occluder depth",
                view,
                center,
                targetVertex.worldPosition,
                blockerCenter,
                sampledSurfaceHit ? sampledSurface : std::nullopt,
                centerPick);
        }

        const int rimOffset = std::max(
            1,
            static_cast<int>(std::floor(
                view.architecturalLightingMarkerHitRadiusPx() - 2.0f)));
        const QPoint rim(center.x() + rimOffset, center.y());
        const std::optional<PickedVertex> rimPick = clickAt(rim);
        check(rimPick
                && rimPick->thingIndex == 0
                && rimPick->lodIndex == targetVertex.lodIndex
                && rimPick->meshIndex == targetVertex.meshIndex
                && rimPick->vertexIndex == targetVertex.vertexIndex,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "ARCHLIGHTING orthographic pick accepts the visible near-coplanar marker rim outside the tiny occluder"
                : "ARCHLIGHTING perspective pick accepts the visible near-coplanar marker rim outside the tiny occluder");
        if (!rimPick)
        {
            bool sampledSurfaceHit = false;
            const std::optional<QVector3D> sampledSurface =
                view.placementPointForViewport(
                    rim, &sampledSurfaceHit, nullptr, nullptr, true, false);
            printArchitecturalDepthDiagnostic(
                projection == app::ViewportCamera::ProjectionType::Orthographic
                    ? "ARCHLIGHTING orthographic marker-rim depth"
                    : "ARCHLIGHTING perspective marker-rim depth",
                view,
                rim,
                targetVertex.worldPosition,
                blockerCenter,
                sampledSurfaceHit ? sampledSurface : std::nullopt,
                rimPick);
        }
        view.hide();
    }

    // The fixture cube occupies z=-1..0, so its actual front wall is z=0.
    // Put the candidate just behind that face, rather than in front of it.
    Level smallGapLevel = makePickingLevel(
        QVector3D{ 0.5f, 0.5f, -0.0005f },
        true);
    assets::ResourceLocator smallGapResources{};
    GlView smallGapView{};
    configurePickingView(
        smallGapView,
        smallGapLevel,
        smallGapResources,
        app::ViewportCamera::ProjectionType::Orthographic,
        true);
    const auto smallGapVertices =
        smallGapView.architecturalLightingVerticesForThing(0);
    const auto smallGapTarget = std::find_if(
        smallGapVertices.begin(),
        smallGapVertices.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return vertex.meshIndex == 0 && vertex.vertexIndex == 0;
        });
    const std::optional<QVector3D> smallGapWorldPosition =
        smallGapTarget != smallGapVertices.end()
        ? std::optional<QVector3D>{ smallGapTarget->worldPosition }
        : std::nullopt;
    const std::optional<QPointF> smallGapScreen = smallGapWorldPosition
        ? smallGapView.projectWorldToViewport(*smallGapWorldPosition)
        : std::nullopt;
    const QPoint smallGapClick = smallGapScreen
        ? QPoint(qRound(smallGapScreen->x()), qRound(smallGapScreen->y()))
        : QPoint{};
    const std::optional<PickedVertex> smallGapPick =
        smallGapWorldPosition
        ? clickArchitecturalVertex(
              smallGapView,
              *smallGapWorldPosition)
        : std::nullopt;
    const bool smallGapBehindWall = smallGapWorldPosition
        && smallGapWorldPosition->z() < 0.0f;
    const double smallGapWorldDistance = smallGapWorldPosition
        ? std::abs(static_cast<double>(smallGapWorldPosition->z()))
        : std::numeric_limits<double>::infinity();
    const bool smallGapWithinRequestedDistance = smallGapWorldPosition
        && std::abs(smallGapWorldDistance - 0.0005) <= 1.0e-7;
    bool sampledWallHit = false;
    const std::optional<QVector3D> sampledWallPosition = smallGapScreen
        ? smallGapView.placementPointForViewport(
              smallGapClick,
              &sampledWallHit,
              nullptr,
              nullptr,
              true,
              false)
        : std::nullopt;
    const std::optional<float> smallGapTargetDepth = smallGapWorldPosition
        ? projectedWindowDepthForDiagnostic(smallGapView, *smallGapWorldPosition)
        : std::nullopt;
    const std::optional<float> smallGapSceneDepth = smallGapScreen
        ? smallGapView.architecturalLightingSceneDepthForTest(
              QPointF(smallGapClick))
        : std::nullopt;
    const bool actualWallIsInFront = sampledWallHit && sampledWallPosition
        && std::abs(sampledWallPosition->z()) <= 1.0e-4f
        && smallGapTargetDepth && smallGapSceneDepth
        && *smallGapSceneDepth < *smallGapTargetDepth;
    const bool smallGapRejected = !smallGapPick;
    check(smallGapWorldPosition
            && smallGapBehindWall
            && smallGapWithinRequestedDistance
            && actualWallIsInFront
            && smallGapRejected,
        "ARCHLIGHTING depth picking rejects a vertex 0.0005 world units behind a solid wall without adding a global bias");
    if (!smallGapWorldPosition || !smallGapBehindWall
        || !smallGapWithinRequestedDistance || !actualWallIsInFront
        || !smallGapRejected)
    {
        const QVector3D expectedWallPoint{
            smallGapWorldPosition ? smallGapWorldPosition->x() : 0.5f,
            smallGapWorldPosition ? smallGapWorldPosition->y() : 0.5f,
            0.0f};
        printArchitecturalDepthDiagnostic(
            "ARCHLIGHTING 0.0005-wall-gap depth",
            smallGapView,
            smallGapClick,
            smallGapWorldPosition.value_or(QVector3D{}),
            expectedWallPoint,
            sampledWallHit ? sampledWallPosition : std::nullopt,
            smallGapPick);
        const auto printDepth = [](const std::optional<float>& depth)
        {
            if (depth)
            {
                std::cerr << *depth;
            }
            else
            {
                std::cerr << "n/a";
            }
        };
        std::cerr << std::setprecision(12)
                  << "ARCHLIGHTING 0.0005-wall-gap conditions: behind="
                  << smallGapBehindWall << " worldGap=" << smallGapWorldDistance
                  << " withinTolerance=" << smallGapWithinRequestedDistance
                  << " wallHit=" << sampledWallHit
                  << " wallInFront=" << actualWallIsInFront
                  << " candidateDepth=";
        printDepth(smallGapTargetDepth);
        std::cerr << " sceneDepth=";
        printDepth(smallGapSceneDepth);
        std::cerr << " rejected=" << smallGapRejected
                  << " authoredThingZ=" << smallGapLevel.things[0].z
                  << " targetZ="
                  << (smallGapWorldPosition
                          ? smallGapWorldPosition->z() : 0.0f)
                  << "\n";
    }
    smallGapView.hide();
}

void runArchitecturalLightingMarqueeTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(editor::DocumentState{
        makeClosedDoorPairLevel(),
        {} });
    window.setProjectTypeForTest(ProjectType::MOTS);
    window.resize(640, 480);
    window.show();
    QApplication::processEvents(QEventLoop::AllEvents);

    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING marquee fixture exposes the MainWindow viewport");
    if (!view)
    {
        return;
    }

    view->setShowThings(true);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->setResourceLocator(&window.resourceLocatorForTest(), false);
    view->primeModel3do("arch_picking_fixture.3do", makeClosedDoorModel());
    view->setLevel(&window.mutableDocumentStateForTest().level);
    view->setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view->setOrbitViewImmediate(32.0f, 42.0f, 0.0f);
    view->frameWorldBounds(
        QVector3D{ -1.2f, -0.8f, -1.2f },
        QVector3D{  6.2f,  1.4f,  3.2f },
        false);
    view->setShowThingGizmos(false);
    view->setSurfaceClickSelectionEnabled(true);
    view->setTransformTool(GlView::TransformTool::Select);
    window.activateArchitecturalLightingEditModeForTest();
    view->setSelectedThing(-1);
    view->update();
    QApplication::processEvents(QEventLoop::AllEvents);

    const std::optional<QVector3D> firstAnchor =
        view->architecturalLightingOwnerGizmoDisplayPositionForTest(0);
    const std::optional<QVector3D> secondAnchor =
        view->architecturalLightingOwnerGizmoDisplayPositionForTest(1);
    check(firstAnchor && secondAnchor,
        "ARCHLIGHTING marquee publishes both displayed owner anchors");
    if (!firstAnchor || !secondAnchor)
    {
        return;
    }
    const std::optional<QPointF> firstScreen =
        view->projectWorldToViewport(*firstAnchor);
    const std::optional<QPointF> secondScreen =
        view->projectWorldToViewport(*secondAnchor);
    check(firstScreen && secondScreen,
        "ARCHLIGHTING marquee projects both displayed owner anchors");
    if (!firstScreen || !secondScreen)
    {
        return;
    }

    std::optional<editor::Selection> emittedSelection{};
    std::optional<GlView::SelectionBoxMode> emittedMode{};
    const QMetaObject::Connection connection = QObject::connect(
        view,
        &GlView::selectionBoxPicked,
        [&emittedSelection, &emittedMode](
            editor::Selection selection,
            const GlView::SelectionBoxMode mode)
        {
            emittedSelection = std::move(selection);
            emittedMode = mode;
        });

    const QPoint start(
        qFloor(std::min(firstScreen->x(), secondScreen->x())) - 24,
        qFloor(std::min(firstScreen->y(), secondScreen->y())) - 24);
    const QPoint finish(
        qCeil(std::max(firstScreen->x(), secondScreen->x())) + 24,
        qCeil(std::max(firstScreen->y(), secondScreen->y())) + 24);
    QMouseEvent press(
        QEvent::MouseButtonPress,
        QPointF(start),
        QPointF(start),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(view, &press);
    QMouseEvent move(
        QEvent::MouseMove,
        QPointF(finish),
        QPointF(finish),
        Qt::NoButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(view, &move);
    QMouseEvent release(
        QEvent::MouseButtonRelease,
        QPointF(finish),
        QPointF(finish),
        Qt::LeftButton,
        Qt::NoButton,
        Qt::NoModifier);
    QApplication::sendEvent(view, &release);
    QApplication::processEvents(QEventLoop::AllEvents);
    QObject::disconnect(connection);

    check(emittedSelection
            && emittedSelection->things
                == std::unordered_set<std::int32_t>{ 0, 1 }
            && emittedMode == GlView::SelectionBoxMode::Replace
            && !view->boxSelectActiveForTest()
            && !view->boxSelectVisibleForTest(),
        "ARCHLIGHTING marquee emits all visible owners and clears its rectangle state on release");
    check(view->architecturalLightingSelectedVertices().size() == 16,
        "ARCHLIGHTING marquee publishes every vertex for newly selected owners");

    // A selected closed model's body surrounds its vertex markers. A small
    // box around one marker therefore intersects both the model body and the
    // exact marker. Entity selection must preserve the selected owner set;
    // the vertex operation owns the partial Replace/Add/Remove change.
    const auto owner0Vertices = view->architecturalLightingVerticesForThing(0);
    const auto owner1Vertices = view->architecturalLightingVerticesForThing(1);
    const auto owner0Marker = std::find_if(
        owner0Vertices.begin(),
        owner0Vertices.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return vertex.meshIndex == 0 && vertex.vertexIndex == 1;
        });
    const auto owner0AddMarker = std::find_if(
        owner0Vertices.begin(),
        owner0Vertices.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return vertex.meshIndex == 0 && vertex.vertexIndex == 2;
        });
    const auto owner0RemoveMarker = std::find_if(
        owner0Vertices.begin(),
        owner0Vertices.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return vertex.meshIndex == 0 && vertex.vertexIndex == 3;
        });
    const auto owner1RetainedMarker = std::find_if(
        owner1Vertices.begin(),
        owner1Vertices.end(),
        [](const GlView::ArchitecturalLightingRenderVertex& vertex)
        {
            return vertex.meshIndex == 0 && vertex.vertexIndex == 0;
        });
    using RenderVertex = GlView::ArchitecturalLightingRenderVertex;
    const std::optional<RenderVertex> owner0MarkerValue =
        owner0Marker != owner0Vertices.end()
        ? std::optional<RenderVertex>{ *owner0Marker }
        : std::nullopt;
    const std::optional<RenderVertex> owner0AddMarkerValue =
        owner0AddMarker != owner0Vertices.end()
        ? std::optional<RenderVertex>{ *owner0AddMarker }
        : std::nullopt;
    const std::optional<RenderVertex> owner0RemoveMarkerValue =
        owner0RemoveMarker != owner0Vertices.end()
        ? std::optional<RenderVertex>{ *owner0RemoveMarker }
        : std::nullopt;
    const std::optional<RenderVertex> owner1RetainedMarkerValue =
        owner1RetainedMarker != owner1Vertices.end()
        ? std::optional<RenderVertex>{ *owner1RetainedMarker }
        : std::nullopt;
    const auto identityOf = [](
        const GlView::ArchitecturalLightingRenderVertex& vertex)
    {
        return GlView::ArchitecturalLightingVertexIdentity{
            vertex.thingIndex,
            vertex.lodIndex,
            vertex.meshIndex,
            vertex.vertexIndex};
    };
    const bool havePartialMarqueeFixture = owner0MarkerValue.has_value()
        && owner0AddMarkerValue.has_value()
        && owner0RemoveMarkerValue.has_value()
        && owner1RetainedMarkerValue.has_value();
    check(havePartialMarqueeFixture,
        "ARCHLIGHTING partial marquee fixture publishes distinct owner markers");

    const auto dragPartialMarker = [&view](
        const QVector3D& worldPosition,
        const Qt::KeyboardModifiers modifiers)
    {
        const std::optional<QPointF> screen =
            view->projectWorldToViewport(worldPosition);
        if (!screen)
        {
            return false;
        }
        const QPoint center(qRound(screen->x()), qRound(screen->y()));
        const QPoint start = center - QPoint(7, 7);
        const QPoint finish = center + QPoint(7, 7);
        QMouseEvent press(
            QEvent::MouseButtonPress,
            QPointF(start),
            QPointF(start),
            Qt::LeftButton,
            Qt::LeftButton,
            modifiers);
        QApplication::sendEvent(view, &press);
        QMouseEvent move(
            QEvent::MouseMove,
            QPointF(finish),
            QPointF(finish),
            Qt::NoButton,
            Qt::LeftButton,
            modifiers);
        QApplication::sendEvent(view, &move);
        QMouseEvent release(
            QEvent::MouseButtonRelease,
            QPointF(finish),
            QPointF(finish),
            Qt::LeftButton,
            Qt::NoButton,
            modifiers);
        QApplication::sendEvent(view, &release);
        QApplication::processEvents(QEventLoop::AllEvents);
        return true;
    };

    const auto resetPartialSelection = [&window, &view](
        const std::vector<GlView::ArchitecturalLightingVertexIdentity>& vertices)
    {
        editor::Selection owners{};
        owners.things.insert(0);
        owners.things.insert(1);
        window.mutableDocumentStateForTest().selection = owners;
        view->setSelectionState(-1, -1, 0, -1, owners);
        view->setSelectedThing(0);
        view->setArchitecturalLightingSelectedVertices(vertices);
    };

    if (havePartialMarqueeFixture)
    {
        view->setXraySelectionCyclingEnabled(true);
        const GlView::ArchitecturalLightingVertexIdentity markerIdentity =
            identityOf(*owner0MarkerValue);
        const GlView::ArchitecturalLightingVertexIdentity addIdentity =
            identityOf(*owner0AddMarkerValue);
        const GlView::ArchitecturalLightingVertexIdentity removeIdentity =
            identityOf(*owner0RemoveMarkerValue);
        const GlView::ArchitecturalLightingVertexIdentity retainedIdentity =
            identityOf(*owner1RetainedMarkerValue);

        resetPartialSelection({ markerIdentity, retainedIdentity });
        (void)dragPartialMarker(
            owner0MarkerValue->worldPosition,
            Qt::NoModifier);
        const std::span<const GlView::ArchitecturalLightingVertexIdentity>
            replaceSpan = view->architecturalLightingSelectedVertices();
        const std::vector<GlView::ArchitecturalLightingVertexIdentity>
            replaceSelection(replaceSpan.begin(), replaceSpan.end());
        check(window.documentStateForTest().selection.things
                == std::unordered_set<std::int32_t>{ 0, 1 }
                && replaceSelection.size() == 1
                && replaceSelection.front() == markerIdentity,
            "ARCHLIGHTING partial marquee Replace preserves owners and exact vertex identity");

        resetPartialSelection({ markerIdentity, retainedIdentity });
        (void)dragPartialMarker(
            owner0AddMarkerValue->worldPosition,
            Qt::ShiftModifier);
        const std::span<const GlView::ArchitecturalLightingVertexIdentity>
            addSpan = view->architecturalLightingSelectedVertices();
        const std::vector<GlView::ArchitecturalLightingVertexIdentity>
            addSelection(addSpan.begin(), addSpan.end());
        if (addSelection.size() != 3)
        {
            std::cerr << "ARCHLIGHTING partial Add diagnostics: selected="
                      << addSelection.size() << " identities=";
            for (const auto& identity : addSelection)
            {
                std::cerr << " (" << identity.thingIndex << ','
                          << identity.lodIndex << ',' << identity.meshIndex
                          << ',' << identity.vertexIndex << ')';
            }
            std::cerr << " expected-add=(" << addIdentity.thingIndex << ','
                      << addIdentity.lodIndex << ',' << addIdentity.meshIndex
                      << ',' << addIdentity.vertexIndex << ")\n";
        }
        check(window.documentStateForTest().selection.things
                == std::unordered_set<std::int32_t>{ 0, 1 }
                && addSelection.size() == 3
                && std::ranges::find(addSelection, markerIdentity)
                    != addSelection.end()
                && std::ranges::find(addSelection, addIdentity)
                    != addSelection.end()
                && std::ranges::find(addSelection, retainedIdentity)
                    != addSelection.end(),
            "ARCHLIGHTING partial marquee Add preserves owners and adds one exact vertex");

        resetPartialSelection({ markerIdentity, removeIdentity, retainedIdentity });
        (void)dragPartialMarker(
            owner0RemoveMarkerValue->worldPosition,
            Qt::ControlModifier);
        const std::span<const GlView::ArchitecturalLightingVertexIdentity>
            removeSpan = view->architecturalLightingSelectedVertices();
        const std::vector<GlView::ArchitecturalLightingVertexIdentity>
            removeSelection(removeSpan.begin(), removeSpan.end());
        check(window.documentStateForTest().selection.things
                == std::unordered_set<std::int32_t>{ 0, 1 }
                && removeSelection.size() == 2
                && std::ranges::find(removeSelection, markerIdentity)
                    != removeSelection.end()
                && std::ranges::find(removeSelection, retainedIdentity)
                    != removeSelection.end()
                && std::ranges::find(removeSelection, removeIdentity)
                    == removeSelection.end(),
            "ARCHLIGHTING partial marquee Remove preserves owners and removes one exact vertex");
        view->setXraySelectionCyclingEnabled(false);
    }

    // The following modifier assertions exercise the generic owner marquee.
    // Start them cold so exact cached vertex markers do not intentionally
    // route the same full box through the unified vertex-only path above.
    editor::Selection noOwnersForGenericMarquee{};
    window.mutableDocumentStateForTest().selection = noOwnersForGenericMarquee;
    view->setSelectionState(
        -1,
        -1,
        -1,
        -1,
        noOwnersForGenericMarquee);
    view->setSelectedThing(-1);

    const auto dragWithModifiers = [&](const Qt::KeyboardModifiers modifiers)
        -> std::optional<GlView::SelectionBoxMode>
    {
        window.mutableDocumentStateForTest().selection =
            noOwnersForGenericMarquee;
        view->setSelectionState(
            -1,
            -1,
            -1,
            -1,
            noOwnersForGenericMarquee);
        view->setSelectedThing(-1);
        view->clearArchitecturalLightingSelectedVertices();
        view->setPickMode(GlView::PickMode::ArchitecturalLighting);
        std::optional<GlView::SelectionBoxMode> mode{};
        const QMetaObject::Connection modifierConnection = QObject::connect(
            view,
            &GlView::selectionBoxPicked,
            [&mode](editor::Selection, const GlView::SelectionBoxMode emitted)
            {
                mode = emitted;
            });
        QMouseEvent modifierPress(
            QEvent::MouseButtonPress,
            QPointF(start),
            QPointF(start),
            Qt::LeftButton,
            Qt::LeftButton,
            modifiers);
        QApplication::sendEvent(view, &modifierPress);
        QMouseEvent modifierMove(
            QEvent::MouseMove,
            QPointF(finish),
            QPointF(finish),
            Qt::NoButton,
            Qt::LeftButton,
            modifiers);
        QApplication::sendEvent(view, &modifierMove);
        QMouseEvent modifierRelease(
            QEvent::MouseButtonRelease,
            QPointF(finish),
            QPointF(finish),
            Qt::LeftButton,
            Qt::NoButton,
            modifiers);
        QApplication::sendEvent(view, &modifierRelease);
        QApplication::processEvents(QEventLoop::AllEvents);
        QObject::disconnect(modifierConnection);
        return mode;
    };

    check(dragWithModifiers(Qt::ShiftModifier)
            == GlView::SelectionBoxMode::Add,
        "ARCHLIGHTING marquee publishes Shift as Add");
    check(dragWithModifiers(Qt::ControlModifier)
            == GlView::SelectionBoxMode::Remove,
        "ARCHLIGHTING marquee publishes Control as Remove");

    // A mode transition must clear an interrupted marquee, rather than only
    // clearing state after the ARCH release handler has already run.
    view->setPickMode(GlView::PickMode::ArchitecturalLighting);
    QMouseEvent interruptedPress(
        QEvent::MouseButtonPress,
        QPointF(start),
        QPointF(start),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(view, &interruptedPress);
    QMouseEvent interruptedMove(
        QEvent::MouseMove,
        QPointF(finish),
        QPointF(finish),
        Qt::NoButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(view, &interruptedMove);
    check(view->boxSelectActiveForTest() && view->boxSelectVisibleForTest(),
        "ARCHLIGHTING marquee exposes active rectangle state while dragging");
    view->setPickMode(GlView::PickMode::Thing);
    check(!view->boxSelectActiveForTest() && !view->boxSelectVisibleForTest(),
        "ARCHLIGHTING marquee state is cleared when the pick mode changes mid-drag");
}

} // namespace

void runThingLightPlacementCursorValidityTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    Level level = makePickingLevel(QVector3D{ 0.0f, 0.0f, 0.0f }, false);
    level.sectors.emplace_back().layer = 0;
    Thing& target = level.things.front();
    target.sector = 0;
    target.archLightingIndex = -1;
    for (TplValue& value : target.values)
    {
        if (value.name == "thingflags")
        {
            value.intValue = 0;
        }
    }
    level.motsArchLighting.clear();
    const bool targetHasNoRegularLight = std::none_of(
        target.values.begin(),
        target.values.end(),
        [](const TplValue& value)
        {
            return value.name == "light";
        });
    window.setDocumentStateForTest(editor::DocumentState{ std::move(level), {} });
    window.setProjectTypeForTest(ProjectType::MOTS);
    window.resize(640, 480);
    window.show();
    QApplication::processEvents(QEventLoop::AllEvents);
    const bool startupViewSettled = waitForEditorViewRefitToSettle(window);
    check(startupViewSettled,
        "Thing-light cursor fixture waits for the MainWindow startup camera refit");
    if (!startupViewSettled)
    {
        window.hide();
        return;
    }

    GlView* const view = window.glViewForTest();
    QAction* const lightEditAction = window.findChild<QAction*>(
        QStringLiteral("viewport.edit_mode.light"));
    QAction* const thingSubmodeAction = window.findChild<QAction*>(
        QStringLiteral("viewport.light_mode.thing"));
    QAction* const addThingLight = window.findChild<QAction*>(
        QStringLiteral("viewport.light_mode.add_thing_light"));
    check(view && lightEditAction && thingSubmodeAction && addThingLight,
        "Thing-light cursor fixture exposes Light, Thing, Add, and viewport actions");
    if (!view || !lightEditAction || !thingSubmodeAction || !addThingLight)
    {
        window.hide();
        return;
    }

    view->setShowThings(true);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->setResourceLocator(&window.resourceLocatorForTest(), false);
    view->primeModel3do("arch_picking_fixture.3do", makePickingModel());
    view->setLevel(&window.mutableDocumentStateForTest().level);
    view->setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view->setOrbitViewImmediate(32.0f, 42.0f, 0.0f);
    view->frameWorldBounds(
        QVector3D{ -0.1f, -0.1f, -0.1f },
        QVector3D{  0.4f,  0.4f,  0.1f },
        false);

    // Follow the same action route used by the Light / Thing / Add Thing Light UI.
    lightEditAction->trigger();
    thingSubmodeAction->trigger();
    addThingLight->trigger();
    QApplication::processEvents(QEventLoop::AllEvents);
    check(lightEditAction->isChecked()
            && thingSubmodeAction->isChecked()
            && addThingLight->isChecked(),
        "Thing-light cursor fixture enters Light / Thing / Add through the production actions");

    const auto moveCursorTo = [view](const QPoint& point)
    {
        QMouseEvent move(
            QEvent::MouseMove,
            QPointF(point),
            QPointF(point),
            Qt::NoButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(view, &move);
        QApplication::processEvents(QEventLoop::AllEvents);
        return view->cursor().pixmap().toImage();
    };
    const QImage voidCursor = moveCursorTo(QPoint(4, 4));
    const std::optional<QPointF> projectedTarget =
        view->projectWorldToViewport(QVector3D{ 0.05f, 0.05f, 0.0f });
    const bool targetOnScreen = projectedTarget
        && projectedTarget->x() >= 0.0
        && projectedTarget->x() < static_cast<qreal>(view->width())
        && projectedTarget->y() >= 0.0
        && projectedTarget->y() < static_cast<qreal>(view->height());
    const QImage validTargetCursor = targetOnScreen
        ? moveCursorTo(QPoint(
              qRound(projectedTarget->x()),
              qRound(projectedTarget->y())))
        : QImage{};
    const auto ringHasDominantColor = [](const QImage& image, const bool expectGreen)
    {
        if (image.isNull())
        {
            return false;
        }
        const std::array<QPoint, 4> ringSamples{
            QPoint(17, 3), QPoint(30, 17), QPoint(17, 30), QPoint(3, 17) };
        for (const QPoint& sample : ringSamples)
        {
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const QPoint point = sample + QPoint(dx, dy);
                    if (!image.rect().contains(point))
                    {
                        continue;
                    }
                    const QColor pixel = image.pixelColor(point);
                    if (pixel.alpha() < 160)
                    {
                        continue;
                    }
                    if (expectGreen
                        ? (pixel.green() > 190
                            && pixel.red() < 145
                            && pixel.blue() > 85
                            && pixel.green() > pixel.red() + 65)
                        : (pixel.red() > 200
                            && pixel.red() > pixel.green() + 70
                            && pixel.red() > pixel.blue() + 70))
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    };
    check(targetHasNoRegularLight
            && lightEditAction->isChecked()
            && thingSubmodeAction->isChecked()
            && addThingLight->isChecked()
            && targetOnScreen
            && ringHasDominantColor(voidCursor, false)
            && ringHasDominantColor(validTargetCursor, true)
            && voidCursor != validTargetCursor,
        "Thing-light Add cursor changes from crossed red at empty space to green on a valid unlit Thing");
    window.hide();
}

void runArchitecturalLightingRejectsNonArchThingTests()
{
    const auto makeNonArchLevel = []()
    {
        Level level = makePickingLevel(QVector3D{ 0.0f, 0.0f, 0.0f }, false);
        Thing& thing = level.things.front();
        thing.archLightingIndex = -1;
        for (TplValue& value : thing.values)
        {
            if (value.name == "thingflags")
            {
                value.intValue = 0;
            }
        }
        level.motsArchLighting.clear();
        return level;
    };
    const auto clearOwnerSelection = [](GlView& view)
    {
        editor::Selection empty{};
        view.setSelectedThing(-1);
        view.setSelectionState(-1, -1, -1, -1, empty);
        view.clearArchitecturalLightingSelectedVertices();
    };

    {
        Level level = makeNonArchLevel();
        assets::ResourceLocator resources{};
        GlView view{};
        configurePickingView(
            view,
            level,
            resources,
            app::ViewportCamera::ProjectionType::Orthographic,
            false);
        clearOwnerSelection(view);
        const std::optional<std::int32_t> picked = clickArchitecturalOwner(
            view,
            QVector3D{ 0.06f, 0.06f, 0.0f });
        check(picked && *picked < 0,
            "ARCH mode direct body picking ignores a model-backed Thing without the Arch flag");
        view.hide();
    }

    {
        Level level = makeNonArchLevel();
        assets::ResourceLocator resources{};
        GlView view{};
        configurePickingView(
            view,
            level,
            resources,
            app::ViewportCamera::ProjectionType::Orthographic,
            false);
        clearOwnerSelection(view);
        std::array<QPointF, 3> projectedCorners{};
        const std::array<QVector3D, 3> bodyCorners{
            QVector3D{ 0.0f, 0.0f, 0.0f },
            QVector3D{ 0.25f, 0.0f, 0.0f },
            QVector3D{ 0.0f, 0.25f, 0.0f }};
        bool allCornersProjected = true;
        for (std::size_t index = 0; index < bodyCorners.size(); ++index)
        {
            const std::optional<QPointF> projected =
                view.projectWorldToViewport(bodyCorners[index]);
            if (!projected)
            {
                allCornersProjected = false;
                break;
            }
            projectedCorners[index] = *projected;
        }
        std::optional<editor::Selection> pickedSelection{};
        const QMetaObject::Connection connection = QObject::connect(
            &view,
            &GlView::selectionBoxPicked,
            [&pickedSelection](editor::Selection selection, GlView::SelectionBoxMode)
            {
                pickedSelection = std::move(selection);
            });
        if (allCornersProjected)
        {
            qreal minX = projectedCorners.front().x();
            qreal maxX = minX;
            qreal minY = projectedCorners.front().y();
            qreal maxY = minY;
            for (const QPointF& point : projectedCorners)
            {
                minX = std::min(minX, point.x());
                maxX = std::max(maxX, point.x());
                minY = std::min(minY, point.y());
                maxY = std::max(maxY, point.y());
            }
            const QPoint start(
                qFloor(minX) - 8,
                qFloor(minY) - 8);
            const QPoint finish(
                qCeil(maxX) + 8,
                qCeil(maxY) + 8);
            dragViewportRect(view, start, finish);
        }
        QObject::disconnect(connection);
        check(allCornersProjected
                && (!pickedSelection || pickedSelection->things.empty()),
            "ARCH mode area selection ignores model-backed Things without the Arch flag");
        view.hide();
    }
}

void runArchitecturalIconConsistencyTests()
{
    const QImage motsArchitecturalIcon =
        render::renderArchitecturalLightingIconImage(
            QColor(255, 204, 64),
            64).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QImage jkdf2ArchThingIcon =
        render::renderArchThingBadgeImage(64)
            .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    check(!motsArchitecturalIcon.isNull()
            && motsArchitecturalIcon == jkdf2ArchThingIcon,
        "MotS ARCH gizmo icon matches the JKDF2 Arch Thing gizmo icon");
}

void runDepthReadbackUtilityTests()
{
    using namespace render::picking;
    const auto topLeft = framebufferPixelForWidgetPoint(
        QPointF{ 0.0, 0.0 }, 4, 3, 1.5, 6, 5);
    const auto scaled = framebufferPixelForWidgetPoint(
        QPointF{ 1.2, 1.8 }, 4, 3, 1.5, 6, 5);
    const auto bottomRight = framebufferPixelForWidgetPoint(
        QPointF{ 3.0, 2.0 }, 4, 3, 1.5, 6, 5);
    check(topLeft && *topLeft == QPoint(0, 4),
        "depth readback maps top-left to GL bottom-left");
    check(scaled && *scaled == QPoint(2, 1),
        "depth readback applies rounded DPR scaling then Y flip");
    check(bottomRight && *bottomRight == QPoint(5, 1),
        "depth readback clamps rounded edge pixels");
    check(!framebufferPixelForWidgetPoint(
              QPointF{ 4.0, 0.0 }, 4, 3, 1.5, 6, 5)
            && !framebufferPixelForWidgetPoint(
                QPointF{ std::numeric_limits<double>::infinity(), 0.0 },
                4, 3, 1.5, 6, 5)
            && !framebufferPixelForWidgetPoint(
                QPointF{ 0.0, 0.0 }, 4, 3, 0.0, 6, 5),
        "depth readback rejects out-of-widget, non-finite, and invalid-DPR points");

    const std::vector<QPoint> samples{
        QPoint{ 10, 20 }, QPoint{ 15, 26 }, QPoint{ 11, 22 },
        QPoint{ -1, 22 }, QPoint{ 100, 100 }};
    const auto region = denseDepthReadbackRegion(
        samples, 100, 100, 3u, 100u, 64u);
    check(region && region->left == 10 && region->bottom == 20
            && region->width == 6 && region->height == 7
            && region->pixelCount == 42u && region->validSampleCount == 3u,
        "depth readback plans the exact bounded rectangle and excludes invalid points");
    check(region
            && depthReadbackSampleIndex(*region, QPoint{ 15, 26 }) == 41u
            && depthReadbackSampleIndex(*region, QPoint{ 10, 20 }) == 0u
            && !depthReadbackSampleIndex(*region, QPoint{ 16, 26 }),
        "depth readback uses exact row-major offsets and rejects pixels outside its region");
    check(!denseDepthReadbackRegion(
              std::span<const QPoint>(samples.data(), 2u), 100, 100, 3u, 100u, 64u),
        "depth readback keeps small query sets on sparse reads");
    const std::vector<QPoint> dispersed{ QPoint{ 0, 0 }, QPoint{ 20, 20 } };
    check(!denseDepthReadbackRegion(dispersed, 100, 100, 2u, 1000u, 64u),
        "depth readback rejects a low-density rectangle below its allocation cap");
    check(!denseDepthReadbackRegion(dispersed, 100, 100, 2u, 100u, 64u),
        "depth readback rejects a rectangle above its pixel cap");
    check(!denseDepthReadbackRegion(
              std::vector<QPoint>{ QPoint{ -1, 0 }, QPoint{ 100, 100 } },
              100, 100, 1u, 100u, 64u),
        "depth readback rejects a query set with no in-frame points");
}

bool projectedPointsMatchWithinSubpixel(
    const std::optional<QPointF>& first,
    const std::optional<QPointF>& second) noexcept
{
    return first && second
        && std::hypot(first->x() - second->x(), first->y() - second->y()) <= 0.05;
}

void runThingOverlayInteractionPickTests()
{
    for (const app::ViewportCamera::ProjectionType projection : {
             app::ViewportCamera::ProjectionType::Orthographic,
             app::ViewportCamera::ProjectionType::Perspective })
    {
        Level level = makePickingLevel(QVector3D{ 0.0f, 0.0f, 0.0f }, false);
        level.things.front().values.push_back(makeIntegerValue("collide", 1));
        level.things.front().values.push_back(makeStringValue("size", "2.0"));
        assets::ResourceLocator resources{};
        GlView view{};
        configurePickingView(view, level, resources, projection, false);
        // The Editor now defaults these overlays on. This block verifies the
        // disabled-overlay picking contract, so state it explicitly rather
        // than inheriting a constructor default.
        view.setShowThingCollisionShapes(false);
        view.setShowThingVisualBounds(false);
        view.setPickMode(GlView::PickMode::Thing);
        view.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
        view.frameWorldBounds(
            QVector3D{ -2.5f, -2.5f, -2.5f },
            QVector3D{  2.5f,  2.5f,  2.5f },
            false);
        const auto projectCollisionTarget = [&view]()
        {
            return view.projectWorldToViewport(QVector3D{ 2.0f, 0.0f, 0.0f });
        };
        const auto collisionScreen = projectCollisionTarget();
        const auto bodyScreen =
            view.projectWorldToViewport(QVector3D{ 0.05f, 0.05f, 0.0f });
        check(collisionScreen && bodyScreen,
            "Thing overlay fixture projects line and body targets");
        if (!collisionScreen || !bodyScreen)
        {
            view.hide();
            continue;
        }
        QPoint collisionPoint(
            qRound(collisionScreen->x()), qRound(collisionScreen->y()));
        const QPoint bodyPoint(qRound(bodyScreen->x()), qRound(bodyScreen->y()));
        int clickedThing = -2;
        const QMetaObject::Connection connection = QObject::connect(
            &view, &GlView::clickPicked,
            [&clickedThing](std::int32_t, std::int32_t,
                            const std::int32_t thingIndex, std::int32_t)
            { clickedThing = thingIndex; });
        clickViewportPoint(view, bodyPoint);
        const int normalBodyPick = clickedThing;
        check(normalBodyPick == 0,
            "ordinary model click still picks with both overlays disabled");
        const auto afterBodyClick = projectCollisionTarget();
        check(projectedPointsMatchWithinSubpixel(collisionScreen, afterBodyClick),
            "ordinary Thing body picking preserves the viewport camera");
        if ( afterBodyClick )
        {
            collisionPoint = QPoint(qRound(afterBodyClick->x()), qRound(afterBodyClick->y()));
        }
        clickedThing = -2;
        clickViewportPoint(view, collisionPoint);
        check(clickedThing == -1,
            "disabled collision overlay does not add a line-only click target");
        const auto afterDisabledOverlayClick = projectCollisionTarget();
        check(projectedPointsMatchWithinSubpixel(afterBodyClick, afterDisabledOverlayClick),
            "disabled overlay picking preserves the viewport camera");
        if ( afterDisabledOverlayClick )
        {
            collisionPoint = QPoint(
                qRound(afterDisabledOverlayClick->x()),
                qRound(afterDisabledOverlayClick->y()));
        }
        if (projection == app::ViewportCamera::ProjectionType::Orthographic)
        {
            view.setSelectedThing(-1);
            std::optional<editor::Selection> selection{};
            const auto disabledBoxConnection = QObject::connect(
                &view, &GlView::selectionBoxPicked,
                [&selection](editor::Selection value, GlView::SelectionBoxMode)
                { selection = std::move(value); });
            dragViewportRect(view, collisionPoint - QPoint{ 3, 3 },
                collisionPoint + QPoint{ 3, 3 });
            QObject::disconnect(disabledBoxConnection);
            check(!selection || !selection->things.contains(0),
                "disabled collision overlay does not add a line-only marquee target");
            const auto afterDisabledMarquee = projectCollisionTarget();
            check(projectedPointsMatchWithinSubpixel(
                      afterDisabledOverlayClick, afterDisabledMarquee),
                "disabled overlay marquee preserves the viewport camera");
            if ( afterDisabledMarquee )
            {
                collisionPoint = QPoint(
                    qRound(afterDisabledMarquee->x()),
                    qRound(afterDisabledMarquee->y()));
            }
        }

        const auto beforeCollisionOverlayEnable = projectCollisionTarget();
        view.setShowThingCollisionShapes(true);
        const auto afterCollisionOverlayEnable = projectCollisionTarget();
        check(projectedPointsMatchWithinSubpixel(
                  beforeCollisionOverlayEnable, afterCollisionOverlayEnable),
            "enabling Collision Shapes preserves the viewport camera");
        if ( afterCollisionOverlayEnable )
        {
            collisionPoint = QPoint(
                qRound(afterCollisionOverlayEnable->x()),
                qRound(afterCollisionOverlayEnable->y()));
        }
        clickedThing = -2;
        clickViewportPoint(view, collisionPoint);
        if ( clickedThing != 0 && afterCollisionOverlayEnable )
        {
        }
        check(clickedThing == 0,
            projection == app::ViewportCamera::ProjectionType::Orthographic
                ? "collision overlay line click selects its owner in orthographic view"
                : "collision overlay line click selects its owner in perspective view");
        view.setHiddenLayers({ 0 });
        clickedThing = -2;
        clickViewportPoint(view, collisionPoint);
        check(clickedThing == -1, "collision overlay click respects hidden layers");
        view.setHiddenLayers({});
        const auto thingFlags = std::find_if(
            level.things.front().values.begin(),
            level.things.front().values.end(),
            [](const TplValue& value) { return value.name == "thingflags"; });
        check(thingFlags != level.things.front().values.end(),
            "collision overlay hidden-Thing fixture has effective flags");
        if (thingFlags != level.things.front().values.end())
        {
            const std::int32_t originalFlags = thingFlags->intValue;
            thingFlags->intValue |= TF_INVISIBLE;
            view.setShowHiddenThings(false);
            clickedThing = -2;
            clickViewportPoint(view, collisionPoint);
            check(clickedThing == -1, "collision overlay click respects a runtime-hidden Thing");
            thingFlags->intValue = originalFlags;
            view.setShowHiddenThings(true);
        }

        if (projection == app::ViewportCamera::ProjectionType::Orthographic)
        {
            view.setSelectedThing(-1);
            std::optional<editor::Selection> selection{};
            const QMetaObject::Connection boxConnection = QObject::connect(
                &view, &GlView::selectionBoxPicked,
                [&selection](editor::Selection value, GlView::SelectionBoxMode)
                { selection = std::move(value); });
            const QPoint start = collisionPoint - QPoint{ 3, 3 };
            const QPoint finish = collisionPoint + QPoint{ 3, 3 };
            const QRectF box{ QPointF(start), QPointF(finish) };
            check(!box.normalized().contains(*bodyScreen),
                "overlay marquee covers only the line, outside the model body");
            dragViewportRect(view, start, finish);
            QObject::disconnect(boxConnection);
            const bool lineMarqueeSelected = selection && selection->things.contains(0);
            if (!lineMarqueeSelected && afterCollisionOverlayEnable)
            {
            }
            check(lineMarqueeSelected,
                "line-only marquee selection includes the visible collision-line owner");
        }
        QObject::disconnect(connection);
        view.hide();
    }

    Level boundsLevel = makePickingLevel(QVector3D{ 0.0f, 0.0f, 0.0f }, false);
    assets::ResourceLocator boundsResources{};
    GlView boundsView{};
    configurePickingView(
        boundsView, boundsLevel, boundsResources,
        app::ViewportCamera::ProjectionType::Orthographic, false);
    boundsView.setShowThingCollisionShapes(false);
    boundsView.setShowThingVisualBounds(false);
    auto boundsModel = std::make_shared<assets::Model3do>(*makePickingModel());
    boundsModel->meshes.front().vertices.push_back(
        assets::Model3doVertex{ 2.0, 0.0, 0.0, ColorWhite });
    boundsView.primeModel3do("arch_picking_fixture.3do", boundsModel);
    boundsView.setLevel(&boundsLevel);
    boundsView.setPickMode(GlView::PickMode::Thing);
    boundsView.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
    boundsView.frameWorldBounds(
        QVector3D{ -2.5f, -2.5f, -2.5f },
        QVector3D{  2.5f,  2.5f,  2.5f }, false);
    const auto projectBoundsTarget = [&boundsView]()
    {
        return boundsView.projectWorldToViewport(QVector3D{ 2.0f, 0.0f, 0.0f });
    };
    const auto boundsScreen = projectBoundsTarget();
    const auto boundsBodyScreen = boundsView.projectWorldToViewport(
        QVector3D{ 0.05f, 0.05f, 0.0f });
    int boundsPick = -2;
    const QMetaObject::Connection boundsConnection = QObject::connect(
        &boundsView, &GlView::clickPicked,
        [&boundsPick](std::int32_t, std::int32_t,
                      const std::int32_t thingIndex, std::int32_t)
        { boundsPick = thingIndex; });
    if (boundsScreen)
    {
        if (boundsBodyScreen)
        {
            clickViewportPoint(
                boundsView,
                QPoint(qRound(boundsBodyScreen->x()), qRound(boundsBodyScreen->y())));
        }
        const auto afterBoundsBodyClick = projectBoundsTarget();
        check(projectedPointsMatchWithinSubpixel(boundsScreen, afterBoundsBodyClick),
            "ordinary model picking preserves the Visual Bounds target camera");
        boundsPick = -2;
        QPoint point = afterBoundsBodyClick
            ? QPoint(qRound(afterBoundsBodyClick->x()), qRound(afterBoundsBodyClick->y()))
            : QPoint(qRound(boundsScreen->x()), qRound(boundsScreen->y()));
        clickViewportPoint(boundsView, point);
        check(boundsPick == -1, "disabled Visual Bounds leaves AABB-only corner unpickable");
        const auto afterDisabledBoundsClick = projectBoundsTarget();
        check(projectedPointsMatchWithinSubpixel(afterBoundsBodyClick, afterDisabledBoundsClick),
            "disabled Visual Bounds picking preserves the viewport camera");
        const auto beforeBoundsEnable = projectBoundsTarget();
        boundsView.setShowThingVisualBounds(true);
        const auto afterBoundsEnable = projectBoundsTarget();
        check(projectedPointsMatchWithinSubpixel(beforeBoundsEnable, afterBoundsEnable),
            "enabling Visual Bounds preserves the viewport camera");
        if ( afterBoundsEnable )
        {
            point = QPoint(qRound(afterBoundsEnable->x()), qRound(afterBoundsEnable->y()));
        }
        boundsPick = -2;
        clickViewportPoint(boundsView, point);
        check(boundsPick == 0, "Visual Bounds line click selects outside rendered model triangles");
        boundsView.setSelectedThing(-1);
        std::optional<editor::Selection> selection{};
        const auto boxConnection = QObject::connect(
            &boundsView, &GlView::selectionBoxPicked,
            [&selection](editor::Selection value, GlView::SelectionBoxMode)
            { selection = std::move(value); });
        const QPoint start = point - QPoint{ 3, 3 };
        const QPoint finish = point + QPoint{ 3, 3 };
        const QRectF box{ QPointF(start), QPointF(finish) };
        const auto bodyScreen = boundsView.projectWorldToViewport(
            QVector3D{ 0.05f, 0.05f, 0.0f });
        check(bodyScreen && !box.normalized().contains(*bodyScreen),
            "Visual Bounds marquee fixture excludes rendered model triangles");
        dragViewportRect(boundsView, start, finish);
        QObject::disconnect(boxConnection);
        const bool lineMarqueeSelected = selection && selection->things.contains(0);
        check(lineMarqueeSelected,
            "line-only Visual Bounds marquee selects the Thing outside rendered triangles");
    }
    else
    {
        check(false, "Visual Bounds fixture projects the model-AABB corner");
        check(false, "Visual Bounds line click selects outside rendered model triangles");
    }
    QObject::disconnect(boundsConnection);
    boundsView.hide();
}

void runThingOverlayOcclusionTests()
{
    const auto makeOverlayOcclusionLevel = [](const bool doubleSided,
                                               const bool reverseWinding)
    {
        Level level = makePickingLevel(QVector3D{ 0.0f, 0.0f, 0.0f }, false);
        level.things.front().values.push_back(makeIntegerValue("collide", 1));
        level.things.front().values.push_back(makeStringValue("size", "2.0"));
        Sector& wall = level.sectors.emplace_back();
        wall.layer = 0;
        wall.vertices = {
            Vertex{ -8.0, -8.0, 3.0 }, Vertex{  8.0, -8.0, 3.0 },
            Vertex{  8.0,  8.0, 3.0 }, Vertex{ -8.0,  8.0, 3.0 }};
        Surface surface{};
        surface.geo = static_cast<std::int32_t>(TGeoMode::Solid);
        surface.faceFlags = doubleSided ? FF_DoubleSided : 0u;
        surface.normal = reverseWinding
            ? TVector{ 0.0, 0.0, -1.0 }
            : TVector{ 0.0, 0.0, 1.0 };
        surface.vertices = reverseWinding
            ? std::vector<std::int32_t>{ 0, 3, 2, 1 }
            : std::vector<std::int32_t>{ 0, 1, 2, 3 };
        surface.texVertices.resize(4u);
        wall.surfaces.push_back(std::move(surface));
        return level;
    };

    for (const app::ViewportCamera::ProjectionType projection : {
             app::ViewportCamera::ProjectionType::Orthographic,
             app::ViewportCamera::ProjectionType::Perspective })
    {
        const bool orthographic =
            projection == app::ViewportCamera::ProjectionType::Orthographic;
        const std::string cameraName = orthographic ? "orthographic " : "perspective ";
        const auto describe = [&cameraName](const bool visualBounds,
                                             const char* behavior)
        {
            return cameraName
                + (visualBounds ? "Visual Bounds " : "Collision Shapes ")
                + behavior;
        };
        const auto configureOverlayView = [&](GlView& view,
                                               Level& level,
                                               assets::ResourceLocator& resources)
        {
            configurePickingView(view, level, resources, projection, false);
            // This unreferenced vertex extends only the model AABB, creating
            // a Visual Bounds line target outside all rendered model triangles.
            auto model = std::make_shared<assets::Model3do>(*makePickingModel());
            model->meshes.front().vertices.push_back(
                assets::Model3doVertex{ 2.0, 0.0, 0.0, ColorWhite });
            view.primeModel3do("arch_picking_fixture.3do", model);
            view.setLevel(&level);
            view.setPickMode(GlView::PickMode::Thing);
            view.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
            view.frameWorldBounds(
                QVector3D{ -3.0f, -3.0f, -2.0f },
                QVector3D{  3.0f,  3.0f,  4.0f }, false);
        };

        const auto exerciseOcclusionCase = [&](Level& level,
                                                assets::ResourceLocator& resources,
                                                const bool opaqueOccluder)
        {
            GlView view{};
            configureOverlayView(view, level, resources);
            check(view.cameraEye().z() > 3.0f,
                describe(false, "fixture camera is above the occlusion plane").c_str());
            const auto projected = view.projectWorldToViewport(
                QVector3D{ 2.0f, 0.0f, 0.0f });
            if (!projected)
            {
                check(false, describe(false, "fixture projects its overlay target").c_str());
                check(false, describe(true, "fixture projects its overlay target").c_str());
                view.hide();
                return;
            }

            const QPoint point(qRound(projected->x()), qRound(projected->y()));
            int clickedThing = -2;
            const auto clickConnection = QObject::connect(
                &view, &GlView::clickPicked,
                [&clickedThing](std::int32_t, std::int32_t,
                                const std::int32_t thingIndex, std::int32_t)
                { clickedThing = thingIndex; });
            std::optional<editor::Selection> selection{};
            const auto selectionConnection = QObject::connect(
                &view, &GlView::selectionBoxPicked,
                [&selection](editor::Selection value, GlView::SelectionBoxMode)
                { selection = std::move(value); });

            for (const bool visualBounds : { false, true })
            {
                view.setShowThingCollisionShapes(!visualBounds);
                view.setShowThingVisualBounds(visualBounds);
                view.setSelectedThing(-1);
                clickedThing = -2;
                clickViewportPoint(view, point);
                const bool clickSelected = clickedThing == 0;
                const std::string clickLabel = describe(
                    visualBounds,
                    opaqueOccluder
                        ? "click remains blocked by opaque level geometry"
                        : "click passes through the culled single-sided backface");
                check(clickSelected != opaqueOccluder, clickLabel.c_str());

                view.setSelectedThing(-1);
                selection.reset();
                dragViewportRect(view, point - QPoint{ 3, 3 }, point + QPoint{ 3, 3 });
                const bool marqueeSelected = selection && selection->things.contains(0);
                const std::string marqueeLabel = describe(
                    visualBounds,
                    opaqueOccluder
                        ? "marquee remains blocked by opaque level geometry"
                        : "marquee passes through the culled single-sided backface");
                check(marqueeSelected != opaqueOccluder, marqueeLabel.c_str());

                if (opaqueOccluder)
                {
                    view.setSelectionModifierBindings(
                        view.selectionAddModifiers(),
                        view.selectionRemoveModifiers(),
                        Qt::AltModifier);
                    clickedThing = -2;
                    clickViewportPoint(view, point, Qt::AltModifier);
                    const std::string throughLabel = describe(
                        visualBounds,
                        "through-selection modifier reaches the occluded wire");
                    check(clickedThing == 0, throughLabel.c_str());
                }
            }

            QObject::disconnect(clickConnection);
            QObject::disconnect(selectionConnection);
            view.hide();
        };

        // This single-sided surface sits at z=+3 but has reversed winding.
        // The camera is above it, so ordinary rendering culls the backface
        // and the surface is not visible along the overlay line's screen ray.
        Level backfaceLevel = makeOverlayOcclusionLevel(false, true);
        assets::ResourceLocator backfaceResources{};
        exerciseOcclusionCase(backfaceLevel, backfaceResources, false);

        // A double-sided wall really is visible from this camera and must
        // continue blocking ordinary click and marquee selection.
        Level opaqueLevel = makeOverlayOcclusionLevel(true, false);
        assets::ResourceLocator opaqueResources{};
        exerciseOcclusionCase(opaqueLevel, opaqueResources, true);
    }
}

void runClassicModelLodCpuPickTests()
{
    struct Scenario
    {
        ProjectType project{};
        app::ViewportCamera::ProjectionType projection{};
        const char* name{};
    };
    const std::array<Scenario, 2> scenarios{
        Scenario{ ProjectType::MOTS,
            app::ViewportCamera::ProjectionType::Orthographic,
            "MotS orthographic" },
        Scenario{ ProjectType::JKDF2,
            app::ViewportCamera::ProjectionType::Perspective,
            "JKDF2 perspective" },
    };
    const auto makeLodModel = [](const double centerX)
    {
        auto model = std::make_shared<assets::Model3do>(*makePickingModel());
        model->geosetCount = 4;
        model->radius = 2.0;
        model->meshes.front().vertices = {
            assets::Model3doVertex{ centerX - 0.3, -0.3, 0.0, ColorWhite },
            assets::Model3doVertex{ centerX + 0.3, -0.3, 0.0, ColorWhite },
            assets::Model3doVertex{ centerX, 0.3, 0.0, ColorWhite },
        };
        return model;
    };
    const QVector3D hqCenter{ 1.1f, -0.1f, 0.0f };
    const QVector3D lowCenter{ -1.1f, -0.1f, 0.0f };

    for (const Scenario& scenario : scenarios)
    {
        Level level = makePickingLevel(QVector3D{}, false);
        level.kind = scenario.project;
        level.header.lodDistances = { 0.1, 0.2, 0.3, 0.4 };
        level.things.front().archLightingIndex = -1;
        level.motsArchLighting.clear();
        auto thingFlags = std::find_if(
            level.things.front().values.begin(),
            level.things.front().values.end(),
            [](const TplValue& value) { return value.name == "thingflags"; });
        check(thingFlags != level.things.front().values.end(),
            "classic LOD CPU fixture has an editable Thing flag");
        if (thingFlags == level.things.front().values.end())
        {
            continue;
        }
        thingFlags->intValue = 0;

        assets::ResourceLocator resources{};
        GlView view{};
        view.setAttribute(Qt::WA_DontShowOnScreen);
        view.resize(640, 480);
        view.setShowGrid(false);
        view.setShowWireframe(false);
        view.setShowVertices(false);
        view.setShowSolid(true);
        view.setShowTextures(false);
        view.setShowThings(true);
        view.setShowThingGizmos(false);
        view.setShowArchThingGizmos(false);
        view.setShowThingCollisionShapes(false);
        view.setShowThingVisualBounds(false);
        view.setShowAxisWidget(false);
        view.setShowToastOverlays(false);
        view.setThingViewMode(GlView::ThingViewMode::Shaded);
        view.setRenderPipeline(GlView::RenderPipeline::Editor);
        view.setTransformTool(GlView::TransformTool::Select);
        view.setResourceLocator(&resources, false);
        view.primeModel3do("arch_picking_fixture.3do", makeLodModel(1.1));
        view.primeModel3do("arch_picking_fixture.3do", makeLodModel(-1.1), 3);
        view.setLevel(&level);
        view.setCameraProjectionType(scenario.projection);
        view.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
        view.frameWorldBounds(
            QVector3D{ -2.0f, -1.0f, -0.1f },
            QVector3D{  2.0f,  1.0f,  0.1f },
            false);
        view.setPickMode(GlView::PickMode::Thing);
        view.setViewportPickingEnabled(true);
        view.setXraySelectionCyclingEnabled(false);
        view.setThingModelLodEnabled(true);
        view.show();
        QApplication::processEvents(QEventLoop::AllEvents);
        view.setForceCpuPickForTest(true);

        const auto clickFace = [&view](const QVector3D& center)
            -> std::optional<std::int32_t>
        {
            const std::optional<QPointF> point =
                view.projectWorldToViewport(center);
            if (!point
                || point->x() < 0.0 || point->x() >= view.width()
                || point->y() < 0.0 || point->y() >= view.height())
            {
                return std::nullopt;
            }
            std::int32_t pickedThing = -2;
            const QMetaObject::Connection connection = QObject::connect(
                &view, &GlView::clickPicked,
                [&pickedThing](std::int32_t, std::int32_t,
                    const std::int32_t thingIndex, std::int32_t)
                {
                    pickedThing = thingIndex;
                });
            clickViewportPoint(view,
                QPoint(qRound(point->x()), qRound(point->y())));
            QObject::disconnect(connection);
            return pickedThing;
        };
        const std::string label = scenario.name;
        const auto assertPick = [&label](
            const std::optional<std::int32_t> actual,
            const std::int32_t expected,
            const char* caseName)
        {
            const std::string message = label + ": " + caseName;
            check(actual && *actual == expected, message.c_str());
        };
        const auto assertLod = [&label, &view](
            const std::size_t expected, const char* caseName)
        {
            const std::string message = label + ": " + caseName;
            check(view.thingRenderedLodIndexForTest(0) == expected,
                message.c_str());
        };

        assertLod(3u, "lower geoset is displayed");
        assertPick(clickFace(lowCenter), 0,
            "CPU fallback picks visible LOD 3 face");
        assertPick(clickFace(hqCenter), -1,
            "CPU fallback ignores hidden HQ-only face");

        view.setThingModelLodEnabled(false);
        QApplication::processEvents(QEventLoop::AllEvents);
        assertLod(0u, "disabled LOD displays HQ");
        assertPick(clickFace(hqCenter), 0,
            "disabled LOD CPU fallback picks HQ face");
        assertPick(clickFace(lowCenter), -1,
            "disabled LOD CPU fallback ignores LOD 3 face");

        thingFlags->intValue = 0x00000008;
        view.setLevel(&level, GlView::SetLevelMode::keepView);
        view.setThingModelLodEnabled(true);
        QApplication::processEvents(QEventLoop::AllEvents);
        assertLod(0u, "Arch Thing remains at HQ");
        assertPick(clickFace(hqCenter), 0,
            "Arch Thing CPU fallback picks HQ face");
        assertPick(clickFace(lowCenter), -1,
            "Arch Thing CPU fallback ignores LOD 3 face");
        view.hide();
    }
}

void runMotsArchLightingPickingTests()
{
    runDepthReadbackUtilityTests();
    runClassicModelLodCpuPickTests();
    runThingOverlayInteractionPickTests();
    runThingOverlayOcclusionTests();
    runArchitecturalLightingProjectionPickTests();
    runArchitecturalLightingThinWallPickTests();
    runArchitecturalLightingOwnerGizmoRestoreTests();
    runArchThingGizmoIntegrationTests();
    runArchThingRecoveryTests();
    runArchitecturalLightingThinDoorDirectPickTests();
    runArchitecturalLightingContextPickTests();
    runArchitecturalLightingMultiOwnerVertexPickTests();
    runArchitecturalLightingMainWindowOwnerPreservationTests();
    runArchitecturalLightingMissDeselectionTests();
    runThingLightGizmoDirectPickTests();
    runThingLightPlacementCursorValidityTests();
    runArchitecturalLightingRejectsNonArchThingTests();
    runArchitecturalIconConsistencyTests();
    runArchitecturalLightingExactSelectionPopupTests();
    runArchitecturalVirtualDefaultOwnerPickTests();
    runArchitecturalLightingTargetSelectionTests();
    runArchitecturalLightingSlopedVertexPickTests();
    runArchitecturalLightingObliqueMarkerRimPickTests();
    runArchitecturalLightingMarqueeTests();
    // Run this standalone QOpenGLWidget fixture last so its temporary context
    // and direct mouse events cannot affect later MainWindow interaction tests.
    runArchitecturalLightingOutsideBackfacePickTests();
}

} // namespace sed::app_tests
