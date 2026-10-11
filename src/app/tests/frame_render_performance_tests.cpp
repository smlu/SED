#include "test_common.hpp"

#include "gl_view.hpp"
#include "sed/assets/3do.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/spr.hpp"
#include "sed/level/build.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QMouseEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sed::app_tests
{
namespace
{

constexpr std::string_view kModelName = "frame_render_performance.3do";
constexpr std::size_t kRingVertexCount = 32u;
constexpr std::size_t kDuplicateFaceCount = 8u;

std::shared_ptr<assets::Model3do> denseWireModel(
    const std::size_t duplicateFaceCount = kDuplicateFaceCount,
    const std::size_t ringVertexCount = kRingVertexCount)
{
    auto model = std::make_shared<assets::Model3do>();
    model->version = 2.3;
    model->geosetCount = 1u;

    assets::Model3doMesh mesh{};
    mesh.name = "frame-render-performance-mesh";
    mesh.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    mesh.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    mesh.vertices.reserve(ringVertexCount);
    for ( std::size_t index = 0u; index < ringVertexCount; ++index )
    {
        const double angle = (2.0 * std::numbers::pi * static_cast<double>(index))
            / static_cast<double>(ringVertexCount);
        mesh.vertices.push_back(assets::Model3doVertex{
            0.75 * std::cos(angle),
            0.75 * std::sin(angle),
            0.05 * static_cast<double>(index % 2u),
            ColorWhite,
        });
    }

    assets::Model3doFace face{};
    face.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    face.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    face.extraLight = ColorBlack;
    face.vertices.reserve(ringVertexCount);
    for ( std::size_t index = 0u; index < ringVertexCount; ++index )
    {
        face.vertices.push_back(static_cast<std::int32_t>(index));
    }
    mesh.faces.reserve(duplicateFaceCount);
    for ( std::size_t index = 0u; index < duplicateFaceCount; ++index )
    {
        // Repeating the same face makes shared polygon edges explicit. The
        // frame cache should visit source topology once and submit one copy of
        // each undirected vertex pair per mesh.
        mesh.faces.push_back(face);
    }
    model->meshes.push_back(std::move(mesh));
    return model;
}

std::shared_ptr<assets::Model3do> denseTriangleModel()
{
    constexpr std::size_t kCellsPerSide = 24u;
    constexpr std::size_t kVertexRow = kCellsPerSide + 1u;

    auto model = std::make_shared<assets::Model3do>();
    model->version = 2.3;
    model->geosetCount = 1u;

    assets::Model3doMesh mesh{};
    mesh.name = "frame-render-performance-triangle-mesh";
    mesh.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    mesh.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    mesh.vertices.reserve(kVertexRow * kVertexRow);
    for ( std::size_t y = 0u; y <= kCellsPerSide; ++y )
    {
        for ( std::size_t x = 0u; x <= kCellsPerSide; ++x )
        {
            mesh.vertices.push_back(assets::Model3doVertex{
                -0.75 + static_cast<double>(x) * (1.5 / static_cast<double>(kCellsPerSide)),
                -0.75 + static_cast<double>(y) * (1.5 / static_cast<double>(kCellsPerSide)),
                0.04 * std::sin(static_cast<double>(x) * 0.4)
                    * std::cos(static_cast<double>(y) * 0.4),
                ColorWhite,
            });
        }
    }

    mesh.faces.reserve(kCellsPerSide * kCellsPerSide * 2u);
    for ( std::size_t y = 0u; y < kCellsPerSide; ++y )
    {
        for ( std::size_t x = 0u; x < kCellsPerSide; ++x )
        {
            const std::int32_t a = static_cast<std::int32_t>(y * kVertexRow + x);
            const std::int32_t b = a + 1;
            const std::int32_t c = a + static_cast<std::int32_t>(kVertexRow);
            const std::int32_t d = c + 1;
            for ( const std::array<std::int32_t, 3u>& indices :
                { std::array<std::int32_t, 3u>{ a, b, d },
                  std::array<std::int32_t, 3u>{ a, d, c } } )
            {
                assets::Model3doFace face{};
                face.geo = static_cast<std::int32_t>(TGeoMode::Solid);
                face.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
                face.extraLight = ColorBlack;
                face.vertices.assign(indices.begin(), indices.end());
                mesh.faces.push_back(std::move(face));
            }
        }
    }

    model->meshes.push_back(std::move(mesh));
    return model;
}

TplValue frameValue(const double x, const double y, const double z)
{
    TplValue value{};
    value.name = "frame";
    value.type = ValueType::frame;
    value.adjType = AdjType::frame;
    value.setFrame(x, y, z, 0.0, 0.0, 0.0);
    return value;
}

TplValue modelValue()
{
    TplValue value{};
    value.name = "model3d";
    value.type = ValueType::str;
    value.stringValue = std::string(kModelName);
    return value;
}

TplValue spriteValue(const std::string& spriteName)
{
    TplValue value{};
    value.name = "sprite";
    value.type = ValueType::str;
    value.stringValue = spriteName;
    return value;
}

struct FrameRenderFixture
{
    Level level{};
    assets::ResourceLocator resources{};
    GlView view{};
    std::shared_ptr<assets::Model3do> model{};

    explicit FrameRenderFixture(
        const std::size_t frameCount,
        const std::size_t ownerCount = 2u,
        const bool denseTriangleMesh = false,
        const bool visibleGhostScene = false,
        const bool includeModelVisual = true,
        const ProjectType projectType = ProjectType::JKDF2)
    {
        level.kind = projectType;
        level.layers.emplace_back("Layer0");
        Sector cube = level::makeSectorPrimitiveShape(
            projectType,
            level::SectorPrimitive::Cube);
        cube.layer = 0;
        level.sectors.push_back(std::move(cube));

        ThingTemplate thingTemplate{};
        thingTemplate.name = "frame_render_performance";
        thingTemplate.bbox.Set(-0.75, -0.75, -0.25, 0.75, 0.75, 0.25);
        if ( includeModelVisual )
        {
            thingTemplate.values.push_back(modelValue());
        }
        level.templates.push_back(thingTemplate);

        for ( std::size_t owner = 0u; owner < ownerCount; ++owner )
        {
            Thing thing{};
            thing.name = "frame_render_performance_" + std::to_string(owner);
            thing.templateName = thingTemplate.name;
            thing.layer = 0;
            thing.sector = -1;
            thing.x = visibleGhostScene
                ? 100.0 + static_cast<double>(owner) * 3.0
                : static_cast<double>(owner) * 3.0;
            thing.bbox = thingTemplate.bbox;
            for ( std::size_t index = 0u; index < frameCount; ++index )
            {
                const double x = visibleGhostScene
                    ? (index == 0u ? -10.0 : static_cast<double>(index - 1u) * 1.5)
                    : thing.x - 0.7 + static_cast<double>(index) * 0.03;
                thing.values.push_back(frameValue(x, 0.0, 0.0));
            }
            level.things.push_back(std::move(thing));
        }

        model = denseTriangleMesh ? denseTriangleModel() : denseWireModel();
        view.setResourceLocator(&resources, false);
        view.primeModel3do(kModelName, model);
        view.resize(800, 600);
        view.setBackgroundClearColor(Qt::black);
        view.setShowGrid(false);
        view.setShowWireframe(false);
        view.setShowVertices(false);
        view.setShowAxisWidget(false);
        view.setShowToastOverlays(false);
        view.setShowThingGizmos(false);
        view.setShowThingVisualBounds(false);
        view.setShowThingCollisionShapes(false);
        if ( visibleGhostScene )
        {
            view.setShowSolid(false);
        }
        view.setLightingMode(GlView::LightingMode::FullyLit);
        view.setCameraProjectionType(
            app::ViewportCamera::ProjectionType::Orthographic);
        view.setLevel(&level);
        view.frameWorldBounds(
            QVector3D{-2.0f, -2.0f, -2.0f},
            QVector3D{5.0f, 2.0f, 2.0f},
            false);
        view.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
        view.setSelectedThing(0);
        view.setFrameEditEnabled(true);
        view.setSelectedFrameValueIndex(frameCount == 0u ? -1 : 0);
        view.show();
        QApplication::processEvents(QEventLoop::AllEvents);
    }

    QImage paint()
    {
        const QImage image = view.grabFramebuffer();
        QApplication::processEvents(QEventLoop::AllEvents);
        return image;
    }
};

void reportOrbitTiming(
    const std::size_t frameCount,
    std::vector<double> samples)
{
    std::sort(samples.begin(), samples.end());
    const double median = samples[samples.size() / 2u];
    const std::size_t p95Index = std::min(
        samples.size() - 1u,
        static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(samples.size()))) - 1u);
    std::cerr << std::fixed << std::setprecision(2)
        << "Frame render paint-path diagnostic (grabFramebuffer + processEvents, not FPS): frames=" << frameCount
        << " median_ms=" << median
        << " p95_ms=" << samples[p95Index]
        << " samples=" << samples.size() << '\n';
}

void testWarmPaintDoesNoFrameModelWork(const std::size_t frameCount)
{
    FrameRenderFixture fixture(frameCount, 2u, true);
    check(!fixture.paint().isNull(),
        "Frame render performance fixture creates an OpenGL framebuffer");

    const GlView::FrameOverlayWorkForTest coldWork =
        fixture.view.frameOverlayWorkForTest();
    if ( frameCount > 0u )
    {
        check(coldWork.validFramePosesParsed >= frameCount,
            "initial Frame path paint parses every well-formed stored frame");
    }
    if ( frameCount > 1u )
    {
        check(coldWork.topologyCacheBuilds == 1u
                && coldWork.uniqueGhostEdgesBuilt == 1776u,
            "one resolved model builds one per-mesh deduplicated ghost topology");
        check(coldWork.ghostDrawPasses > 0u
                && coldWork.lastGhostInstanceCount == frameCount - 1u,
            "selected Frame is excluded while every other valid Frame submits one ghost instance");
        check(coldWork.persistentBatchUploads > 0u,
            "the first ghost batch uploads persistent GPU data");
    }

    fixture.view.resetFrameOverlayWorkForTest();
    const std::uint64_t rebuildCount = fixture.view.meshRebuildCountForTest();
    const std::uint64_t uploadCount = fixture.view.meshUploadCountForTest();
    const std::uint64_t resourceReadCount = fixture.view.model3doResourceReadCountForTest();

    fixture.view.update();
    (void)fixture.paint();

    std::vector<double> orbitSamples{};
    orbitSamples.reserve(5u);
    for ( int sample = 0; sample < 5; ++sample )
    {
        QElapsedTimer timer{};
        timer.start();
        fixture.view.setOrbitViewImmediate(
            static_cast<float>(sample) * 3.0f,
            90.0f + static_cast<float>(sample) * 0.5f,
            0.0f);
        (void)fixture.paint();
        orbitSamples.push_back(static_cast<double>(timer.nsecsElapsed()) / 1.0e6);
    }

    const GlView::FrameOverlayWorkForTest orbitWork =
        fixture.view.frameOverlayWorkForTest();
    check(orbitWork.frameValueParseCalls == 0u
            && orbitWork.framePickValueParseCalls == 0u,
        "warmed stationary/orbit paints do not reparse frame values or pick poses");
    check(orbitWork.modelCacheResolveCalls == 0u
            && orbitWork.sourceFaceEdgeVisits == 0u,
        "warmed stationary/orbit paints do not resolve model resources or visit source face edges");
    check(orbitWork.persistentBatchUploads == 0u
            && fixture.view.meshRebuildCountForTest() == rebuildCount
            && fixture.view.meshUploadCountForTest() == uploadCount
            && fixture.view.model3doResourceReadCountForTest() == resourceReadCount,
        "camera motion leaves persistent Frame batches, ordinary Thing meshes, and resources untouched");

    if ( frameCount == 3u || frameCount == 5u || frameCount == 64u )
    {
        reportOrbitTiming(frameCount, std::move(orbitSamples));
    }
}

void testSharedTopologyAndSourceReplacement()
{
    FrameRenderFixture fixture(3u);
    (void)fixture.paint();
    const GlView::FrameOverlayWorkForTest firstOwnerWork =
        fixture.view.frameOverlayWorkForTest();
    check(firstOwnerWork.topologyCacheBuilds == 1u
            && firstOwnerWork.uniqueGhostEdgesBuilt == kRingVertexCount,
        "first owner publishes one deduplicated topology for its resolved model revision");

    fixture.view.resetFrameOverlayWorkForTest();
    fixture.view.setSelectedThing(1);
    fixture.view.setSelectedFrameValueIndex(0);
    (void)fixture.paint();
    const GlView::FrameOverlayWorkForTest secondOwnerWork =
        fixture.view.frameOverlayWorkForTest();
    check(secondOwnerWork.topologyCacheBuilds == 0u
            && secondOwnerWork.uniqueGhostEdgesBuilt == 0u,
        "another owner sharing the model reuses its validated per-mesh topology");

    fixture.view.resetFrameOverlayWorkForTest();
    fixture.model = denseWireModel(kDuplicateFaceCount, 16u);
    fixture.view.primeModel3do(kModelName, fixture.model);
    fixture.view.update();
    (void)fixture.paint();
    const GlView::FrameOverlayWorkForTest replacementWork =
        fixture.view.frameOverlayWorkForTest();
    check(replacementWork.topologyCacheBuilds == 1u
            && replacementWork.uniqueGhostEdgesBuilt == 16u,
        "replacing an immutable model source rebuilds its ghost topology exactly once");

    fixture.view.resetFrameOverlayWorkForTest();
    fixture.model = denseWireModel(kDuplicateFaceCount, 12u);
    fixture.view.primeModel3do(kModelName, fixture.model);
    fixture.view.setLevel(&fixture.level, GlView::SetLevelMode::keepView);
    fixture.view.update();
    (void)fixture.paint();
    const GlView::FrameOverlayWorkForTest sameLevelWork =
        fixture.view.frameOverlayWorkForTest();
    check(sameLevelWork.topologyCacheBuilds == 1u
            && sameLevelWork.uniqueGhostEdgesBuilt == 12u,
        "republishing replacement model content through the same Level pointer invalidates Frame topology");
}

void testOwnerLocalModelIdentity()
{
    FrameRenderFixture fixture(3u);
    const std::string localModelName = "frame_render_owner_local.3do";
    fixture.view.primeModel3do(localModelName, denseWireModel(kDuplicateFaceCount, 16u));

    TplValue localModel{};
    localModel.name = "model3d";
    localModel.type = ValueType::str;
    localModel.stringValue = localModelName;
    fixture.level.things[1].values.insert(
        fixture.level.things[1].values.begin(),
        std::move(localModel));
    fixture.view.notifyThingsChanged();
    fixture.view.setSelectedThing(1);
    fixture.view.setSelectedFrameValueIndex(1);
    fixture.view.resetFrameOverlayWorkForTest();
    (void)fixture.paint();

    const GlView::FrameOverlayWorkForTest localWork =
        fixture.view.frameOverlayWorkForTest();
    check(localWork.topologyCacheBuilds == 1u
            && localWork.uniqueGhostEdgesBuilt == 16u,
        "frame ghost topology follows the selected owner's local model override");
}

void testOwnerOnlyFrameInvalidation()
{
    FrameRenderFixture fixture(3u, 2u);
    fixture.view.setShowThingGizmos(true);
    (void)fixture.paint();
    fixture.view.setSelectedThing(1);
    fixture.view.setSelectedFrameValueIndex(0);
    (void)fixture.paint();
    fixture.view.setSelectedThing(0);
    (void)fixture.paint();

    const std::uint64_t gizmoBuilds =
        fixture.view.thingTypeGizmoBuildCountForTest();
    const std::uint64_t gizmoMetadataBuilds =
        fixture.view.thingTypeGizmoMetadataBuildCountForTest();
    const std::uint64_t meshRebuilds = fixture.view.meshRebuildCountForTest();
    const std::uint64_t meshUploads = fixture.view.meshUploadCountForTest();

    fixture.level.things[0].values[1].setFrame(
        -0.55, 0.0, 0.0, 0.0, 0.0, 0.0);
    fixture.view.notifyThingFramesChanged({ 0 });
    (void)fixture.paint();

    check(fixture.view.thingTypeGizmoBuildCountForTest() == gizmoBuilds
            && fixture.view.thingTypeGizmoMetadataBuildCountForTest()
                == gizmoMetadataBuilds,
        "publishing one owner's Frame values leaves global Thing-gizmo caches warm");
    check(fixture.view.meshRebuildCountForTest() == meshRebuilds
            && fixture.view.meshUploadCountForTest() == meshUploads,
        "publishing Frame values does not rebuild or upload ordinary Thing meshes");

    fixture.view.setSelectedThing(1);
    fixture.view.setSelectedFrameValueIndex(0);
    fixture.view.resetFrameOverlayWorkForTest();
    (void)fixture.paint();
    const GlView::FrameOverlayWorkForTest otherOwnerWork =
        fixture.view.frameOverlayWorkForTest();
    check(otherOwnerWork.frameValueParseCalls == 0u
            && otherOwnerWork.modelCacheResolveCalls == 0u
            && otherOwnerWork.sourceFaceEdgeVisits == 0u
            && otherOwnerWork.persistentBatchUploads == 0u,
        "changing one owner's Frames keeps another warmed owner's path and ghost batch intact");
}

std::optional<std::int32_t> clickThingAt(GlView& view, const QPoint& point)
{
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
}

void testTargetedFramePublicationInvalidatesThingPeel()
{
    FrameRenderFixture fixture(3u, 1u, true);
    fixture.view.setShowSolid(false);
    fixture.view.setPickMode(GlView::PickMode::Thing);
    (void)fixture.paint();

    // The sampled point lies inside the selected Frame's model face, well
    // outside the small frame-path marker tolerance around its pivot.
    const std::optional<QPointF> projected =
        fixture.view.projectWorldToViewport(QVector3D(-0.35f, 0.25f, 0.0f));
    check(projected.has_value(),
        "selected Frame model silhouette projects into the viewport");
    if ( !projected )
    {
        return;
    }
    const QPoint point(qRound(projected->x()), qRound(projected->y()));

    const std::optional<std::int32_t> before = clickThingAt(fixture.view, point);
    check(before && *before == 0,
        "the selected Frame model silhouette is returned by a real Thing peel");
    check(fixture.view.thingPeelCacheWarmForTest(),
        "the silhouette click leaves a populated Thing peel cache at this pixel");

    fixture.level.things[0].values[0].setFrame(
        100.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    fixture.view.notifyThingFramesChanged({ 0 });
    const auto movedPose = fixture.view.cachedFramePoseForTest(0, 0);
    check(movedPose && std::abs(movedPose->x - 100.0) < 1.0e-9,
        "targeted Frame publication exposes the moved selected pose without camera motion");

    const std::optional<std::int32_t> after = clickThingAt(fixture.view, point);
    check(!after || *after != 0,
        "a pixel vacated by the selected Frame model does not reuse its old peel hit");
}

void testMalformedFrameIsOmitted()
{
    FrameRenderFixture fixture(3u, 1u);
    Thing& thing = fixture.level.things[0];
    thing.values[1].stringValue = "(bad frame)";
    fixture.view.notifyThingsChanged();
    fixture.view.setSelectedThing(0);
    fixture.view.setSelectedFrameValueIndex(0);
    fixture.view.resetFrameOverlayWorkForTest();
    (void)fixture.paint();

    const GlView::FrameOverlayWorkForTest work =
        fixture.view.frameOverlayWorkForTest();
    check(work.lastFramePathPoseCount == 2u,
        "malformed Frame text is rejected while valid neighboring poses remain available");
    check(work.lastGhostInstanceCount == 1u,
        "malformed and selected Frames do not create ghost model instances");
}

void testNegativeTopologyDoesNotRepeatSourceWalks()
{
    FrameRenderFixture fixture(3u, 1u);
    fixture.view.setFrameGhostTopologyEdgeLimitForTest(4u);
    fixture.view.resetFrameOverlayWorkForTest();
    (void)fixture.paint();
    const GlView::FrameOverlayWorkForTest coldWork =
        fixture.view.frameOverlayWorkForTest();
    check(coldWork.topologyCacheBuilds == 1u
            && coldWork.sourceFaceEdgeVisits > 4u,
        "a small test edge budget exercises the real cached negative-topology path");
    check(coldWork.lastFramePathPoseCount == 3u
            && fixture.view.activeFrameValueIndex() == 0,
        "negative model topology retains the Frame path and selected frame");
    const auto selectedPose = fixture.view.cachedFramePoseForTest(0, 0);
    check(selectedPose && std::abs(selectedPose->x + 0.7) < 1.0e-9,
        "negative model topology does not discard the selected pose cache");

    fixture.view.resetFrameOverlayWorkForTest();
    fixture.view.setOrbitViewImmediate(2.0f, 91.0f, 0.0f);
    (void)fixture.paint();
    const GlView::FrameOverlayWorkForTest warmWork =
        fixture.view.frameOverlayWorkForTest();
    check(warmWork.topologyCacheBuilds == 0u
            && warmWork.sourceFaceEdgeVisits == 0u,
        "a cached negative topology omits model ghosts instead of revisiting source face edges");
    check(warmWork.lastFramePathPoseCount == 3u,
        "the clipped Frame path remains available after model ghost omission");
}

void testOversizedFramePrefixDoesNotReparseAfterWarm()
{
    constexpr std::size_t kFrameLimit =
        render::frame_ghost::kMaximumCachedFramePosesPerOwner;
    FrameRenderFixture fixture(kFrameLimit + 1u, 1u, false, false, false);
    check(!fixture.paint().isNull(),
        "oversized Frame path fixture creates an OpenGL framebuffer");

    fixture.view.setSelectedFrameValueIndex(static_cast<std::int32_t>(kFrameLimit));
    fixture.view.resetFrameOverlayWorkForTest();
    (void)fixture.paint();
    const GlView::FrameOverlayWorkForTest selectedWork =
        fixture.view.frameOverlayWorkForTest();
    check(selectedWork.lastFramePathPoseCount == kFrameLimit,
        "oversized owner paths draw only the bounded cached pose prefix");
    check(selectedWork.frameValueParseCalls == 1u,
        "one selected pose beyond the prefix is parsed through the indexed overflow path");
    const auto selectedPose = fixture.view.cachedFramePoseForTest(
        0,
        static_cast<std::int32_t>(kFrameLimit));
    check(selectedPose
            && std::abs(selectedPose->x
                - (-0.7 + static_cast<double>(kFrameLimit) * 0.03)) < 1.0e-8,
        "the selected overflow pose remains available without extending the path cache");

    fixture.view.resetFrameOverlayWorkForTest();
    fixture.view.setOrbitViewImmediate(4.0f, 92.0f, 0.0f);
    (void)fixture.paint();
    const GlView::FrameOverlayWorkForTest warmWork =
        fixture.view.frameOverlayWorkForTest();
    check(warmWork.frameValueParseCalls == 0u
            && warmWork.framePickValueParseCalls == 0u,
        "warmed oversized paths do not reparse the full owner on camera motion");
    check(warmWork.lastFramePathPoseCount == kFrameLimit,
        "camera motion keeps the same bounded Frame path prefix");
}

void testClassicTypeTwoLocalSpriteSuppressesInheritedModelGhosts()
{
    constexpr std::string_view kSpriteName = "frame_render_type_two.spr";
    FrameRenderFixture fixture(3u, 1u);

    assets::SprFile sprite{};
    sprite.material = "frame_render_type_two";
    sprite.type = 2;
    sprite.width = 1.0f;
    sprite.height = 1.0f;
    const std::string spriteText = assets::saveToSPRText(
        sprite,
        assets::SprProfile::classic);
    std::vector<std::byte> spriteBytes{};
    spriteBytes.reserve(spriteText.size());
    for ( const char character : spriteText )
    {
        spriteBytes.push_back(static_cast<std::byte>(
            static_cast<unsigned char>(character)));
    }
    fixture.resources.setPrimaryResourceOverride(
        kSpriteName,
        [bytes = std::move(spriteBytes)]() mutable
            -> std::optional<std::vector<std::byte>>
        {
            return std::move(bytes);
        });
    fixture.level.things[0].values.push_back(
        spriteValue(std::string(kSpriteName)));
    fixture.view.notifyThingsChanged();
    fixture.view.resetFrameOverlayWorkForTest();
    (void)fixture.paint();

    const GlView::FrameOverlayWorkForTest work =
        fixture.view.frameOverlayWorkForTest();
    check(work.modelCacheResolveCalls == 0u
            && work.topologyCacheBuilds == 0u,
        "a local classic SPR type 2 follows renderer type mapping and suppresses the inherited 3DO ghost");
}

void testHiddenOwnerLayerSuppressesFrameGhosts()
{
    FrameRenderFixture fixture(3u, 1u);
    fixture.view.setHiddenLayers({ 0 });
    fixture.view.resetFrameOverlayWorkForTest();
    (void)fixture.paint();

    const GlView::FrameOverlayWorkForTest work =
        fixture.view.frameOverlayWorkForTest();
    check(work.topologyCacheBuilds == 0u
            && work.ghostDrawPasses == 0u
            && work.lastGhostInstanceCount == 0u,
        "hidden owner layers suppress Frame model ghosts and topology resolution");
}

void testFrameGpuResourceRecreationRetainsCpuCaches()
{
    FrameRenderFixture fixture(3u, 1u, true, true);
    check(!fixture.paint().isNull(),
        "Frame resource recreation fixture creates an OpenGL framebuffer");

    fixture.view.resetFrameOverlayWorkForTest();
    fixture.view.makeCurrent();
    fixture.view.recreateFrameGhostGpuResourcesForTest();
    fixture.view.doneCurrent();
    fixture.view.update();
    check(!fixture.paint().isNull(),
        "Frame ghost resources render again after GL resource recreation");

    const GlView::FrameOverlayWorkForTest work =
        fixture.view.frameOverlayWorkForTest();
    check(work.frameValueParseCalls == 0u
            && work.topologyCacheBuilds == 0u
            && work.sourceFaceEdgeVisits == 0u,
        "Frame GL resource recreation retains parsed poses and model topology");
    check(work.persistentBatchUploads > 0u
            && work.ghostDrawPasses > 0u,
        "Frame GL resource recreation reuploads persistent buffers and resumes ghost draws");
}

std::size_t translucentGhostPixelCount(const QImage& image)
{
    std::size_t count = 0u;
    for ( int y = 0; y < image.height(); ++y )
    {
        for ( int x = 0; x < image.width(); ++x )
        {
            const QColor color = image.pixelColor(x, y);
            if ( color.red() >= 30 && color.red() <= 145
                && color.green() >= color.red()
                && color.green() <= 150
                && color.blue() >= color.green()
                && color.blue() <= 160 )
            {
                ++count;
            }
        }
    }
    return count;
}

void testGhostModelsRemainVisibleInBothProjections()
{
    for ( const ProjectType project : {
            ProjectType::JKDF2,
            ProjectType::MOTS,
            ProjectType::IJIM } )
    {
        FrameRenderFixture fixture(3u, 1u, true, true, true, project);
        check(!fixture.paint().isNull(),
            "visible Frame ghost fixture creates an OpenGL framebuffer");
        const GlView::FrameOverlayWorkForTest orthographicWork =
            fixture.view.frameOverlayWorkForTest();
        check(orthographicWork.ghostDrawPasses > 0u
                && orthographicWork.lastGhostInstanceCount == 2u,
            "three-frame GPU batch excludes only the selected pose in each project type");

        const QImage orthographic = fixture.paint();
        const std::size_t orthographicGhostPixels =
            translucentGhostPixelCount(orthographic);
        check(orthographicGhostPixels > 20u,
            "translucent ghost model edges remain visible in orthographic projection");

        fixture.view.setCameraProjectionType(
            app::ViewportCamera::ProjectionType::Perspective);
        fixture.view.frameWorldBounds(
            QVector3D{-2.0f, -2.0f, -2.0f},
            QVector3D{5.0f, 2.0f, 2.0f},
            false);
        fixture.view.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
        fixture.view.resetFrameOverlayWorkForTest();
        const QImage perspective = fixture.paint();
        const std::size_t perspectiveGhostPixels =
            translucentGhostPixelCount(perspective);
        check(perspectiveGhostPixels > 20u,
            "translucent ghost model edges remain visible in perspective projection");
        const GlView::FrameOverlayWorkForTest perspectiveWork =
            fixture.view.frameOverlayWorkForTest();
        check(perspectiveWork.frameValueParseCalls == 0u
                && perspectiveWork.sourceFaceEdgeVisits == 0u
                && perspectiveWork.persistentBatchUploads == 0u,
            "project-specific camera changes reuse Frame poses, topology, and GPU buffers");
    }
}

} // namespace

void runFrameRenderPerformanceTests()
{
    for ( const std::size_t frameCount : std::array<std::size_t, 5u>{ 0u, 1u, 3u, 5u, 64u } )
    {
        testWarmPaintDoesNoFrameModelWork(frameCount);
    }
    testSharedTopologyAndSourceReplacement();
    testOwnerLocalModelIdentity();
    testOwnerOnlyFrameInvalidation();
    testTargetedFramePublicationInvalidatesThingPeel();
    testMalformedFrameIsOmitted();
    testNegativeTopologyDoesNotRepeatSourceWalks();
    testOversizedFramePrefixDoesNotReparseAfterWarm();
    testClassicTypeTwoLocalSpriteSuppressesInheritedModelGhosts();
    testHiddenOwnerLayerSuppressesFrameGhosts();
    testFrameGpuResourceRecreationRetainsCpuCaches();
    testGhostModelsRemainVisibleInBothProjections();
}

} // namespace sed::app_tests
