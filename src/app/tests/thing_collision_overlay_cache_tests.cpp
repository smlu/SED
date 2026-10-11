#include "test_common.hpp"

#include "gl_view.hpp"
#include "sed/assets/3do.hpp"
#include "sed/assets/resource_locator.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QLineF>
#include <QMatrix4x4>
#include <QRect>
#include <QTemporaryDir>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>

namespace sed::app_tests
{

namespace
{

struct OverlayPixelCenter
{
    QPointF point{};
    std::size_t count{};
};

bool matchesOverlayPixel(const QColor& color, const bool collision)
{
    return collision
        ? (color.green() > 150
            && color.green() > color.red() * 1.25
            && color.green() > color.blue() * 1.20)
        : (color.red() > 160
            && color.green() > 70
            && color.green() < color.red() * 0.85
            && color.blue() < 100);
}

OverlayPixelCenter overlayPixelCenter(const QImage& frame, const bool collision)
{
    double sumX = 0.0;
    double sumY = 0.0;
    std::size_t count = 0u;
    for (int y = 0; y < frame.height(); ++y)
    {
        for (int x = 0; x < frame.width(); ++x)
        {
            if (matchesOverlayPixel(frame.pixelColor(x, y), collision))
            {
                sumX += x;
                sumY += y;
                ++count;
            }
        }
    }
    return count > 0u
        ? OverlayPixelCenter{ QPointF{ sumX / count, sumY / count }, count }
        : OverlayPixelCenter{};
}

// A centroid can drift when other wire colors cover part of an overlay or
// selected Things receive a wider stroke. Match the actual colored pixels at
// their projected transformed positions instead. This also tests each Thing
// separately when the caller passes a small owner region.
double shiftedOverlayPixelCoverage(
    const QImage& before,
    const QImage& after,
    const bool collision,
    const QPointF& projectedMove,
    const QRect& ownerRegion)
{
    if (before.isNull() || after.size() != before.size())
    {
        return 0.0;
    }
    const QRect region = ownerRegion.intersected(before.rect());
    const int moveX = static_cast<int>(std::lround(projectedMove.x()));
    const int moveY = static_cast<int>(std::lround(projectedMove.y()));
    std::size_t eligible = 0u;
    std::size_t matched = 0u;
    for (int y = region.top(); y <= region.bottom(); ++y)
    {
        for (int x = region.left(); x <= region.right(); ++x)
        {
            if (!matchesOverlayPixel(before.pixelColor(x, y), collision))
            {
                continue;
            }
            const int movedX = x + moveX;
            const int movedY = y + moveY;
            if (!after.rect().contains(movedX, movedY))
            {
                continue;
            }
            ++eligible;
            bool found = false;
            for (int dy = -2; dy <= 2 && !found; ++dy)
            {
                for (int dx = -2; dx <= 2; ++dx)
                {
                    const int px = movedX + dx;
                    const int py = movedY + dy;
                    if (after.rect().contains(px, py)
                        && matchesOverlayPixel(after.pixelColor(px, py), collision))
                    {
                        found = true;
                        break;
                    }
                }
            }
            matched += found ? 1u : 0u;
        }
    }
    return eligible > 10u
        ? static_cast<double>(matched) / static_cast<double>(eligible)
        : 0.0;
}

} // namespace

void runThingCollisionOverlayCacheTests()
{
    // Exercise the production GlView publication path: collision size is not
    // Thing triangle geometry, so an inherited/local scalar edit should patch
    // the fixed-topology collision cache and its composed VBO without rebuilding
    // the model triangles.
    Level liveLevel{};
    liveLevel.kind = ProjectType::JKDF2;
    liveLevel.layers.emplace_back("Layer0");

    ThingTemplate liveTemplate{};
    liveTemplate.name = "collision_overlay_live_test";
    liveTemplate.bbox.Set(-0.5, -0.5, -0.5, 0.5, 0.5, 0.5);
    TplValue liveModelValue{};
    liveModelValue.name = "model3d";
    liveModelValue.type = ValueType::str;
    liveModelValue.stringValue = "collision_overlay_live_test.3do";
    TplValue liveCollideValue{};
    liveCollideValue.name = "collide";
    liveCollideValue.type = ValueType::integer;
    liveCollideValue.intValue = 1;
    TplValue liveSizeValue{};
    liveSizeValue.name = "size";
    liveSizeValue.type = ValueType::floating;
    liveSizeValue.floatValue = 2.5;
    liveTemplate.values = { liveModelValue, liveCollideValue, liveSizeValue };
    liveLevel.templates.push_back(liveTemplate);

    Thing liveThing{};
    liveThing.name = "collision_overlay_live_test";
    liveThing.templateName = liveTemplate.name;
    liveThing.layer = 0;
    liveThing.sector = -1;
    liveThing.bbox = liveTemplate.bbox;
    liveLevel.things.push_back(liveThing);

    auto liveModel = std::make_shared<assets::Model3do>();
    liveModel->version = 2.1;
    assets::Model3doMesh liveMesh{};
    liveMesh.name = "collision_overlay_live_test";
    liveMesh.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    liveMesh.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    liveMesh.vertices = {
        assets::Model3doVertex{ 0.0, 0.0, 0.0, ColorWhite },
        assets::Model3doVertex{ 1.0, 0.0, 0.0, ColorWhite },
        assets::Model3doVertex{ 0.0, 1.0, 0.0, ColorWhite },
    };
    assets::Model3doFace liveFace{};
    liveFace.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    liveFace.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    liveFace.extraLight = ColorBlack;
    liveFace.vertices = { 0, 1, 2 };
    liveMesh.faces.push_back(liveFace);
    liveModel->meshes.push_back(liveMesh);

    assets::ResourceLocator liveResources{};
    GlView liveView{};
    liveView.setAttribute(Qt::WA_DontShowOnScreen);
    liveView.setResourceLocator(&liveResources, false);
    liveView.primeModel3do("collision_overlay_live_test.3do", liveModel);
    liveView.setThingViewMode(GlView::ThingViewMode::Shaded);
    liveView.setShowTextures(false);
    liveView.setLevel(&liveLevel);
    liveView.setShowThingCollisionShapes(true);

    const auto collisionRadiusMatches = [&liveView](const double expected)
    {
        const std::optional<double> cacheRadius =
            liveView.thingCollisionOverlayExtentForTest(0u, false);
        const std::optional<double> composedRadius =
            liveView.thingCollisionOverlayExtentForTest(0u, true);
        return cacheRadius && composedRadius
            && std::abs(*cacheRadius - expected) < 1.0e-5
            && std::abs(*composedRadius - expected) < 1.0e-5;
    };
    const std::size_t initialTriangleVertices =
        liveView.thingTriangleCacheVertexCountForTest();
    const std::uint64_t initialTriangleFingerprint =
        liveView.thingTriangleCacheFingerprintForTest();
    const std::uint64_t initialRebuildCount = liveView.meshRebuildCountForTest();
    check(liveView.thingMeshCacheValidForTest()
            && initialTriangleVertices > 0u
            && collisionRadiusMatches(2.5),
        "GlView seeds rendered Thing triangles and the inherited collision sphere cache");

    liveLevel.templates[0].values[2].floatValue = 3.75;
    liveView.notifyThingsChanged();
    check(collisionRadiusMatches(3.75)
            && liveView.meshRebuildCountForTest() == initialRebuildCount
            && liveView.thingTriangleCacheVertexCountForTest() == initialTriangleVertices
            && liveView.thingTriangleCacheFingerprintForTest() == initialTriangleFingerprint,
        "inherited collision size edits patch cached and composed sphere lines without rebuilding Thing triangles");

    TplValue localLiveSize{};
    localLiveSize.name = "size";
    localLiveSize.type = ValueType::floating;
    localLiveSize.floatValue = 4.5;
    liveLevel.things[0].values.push_back(localLiveSize);
    liveView.notifyThingsChanged();
    check(collisionRadiusMatches(4.5)
            && liveView.meshRebuildCountForTest() == initialRebuildCount
            && liveView.thingTriangleCacheVertexCountForTest() == initialTriangleVertices
            && liveView.thingTriangleCacheFingerprintForTest() == initialTriangleFingerprint,
        "local collision size override patches the live overlay while preserving the model triangle cache");

    liveLevel.templates[0].values[2].floatValue = 5.75;
    liveView.notifyThingsChanged();
    check(collisionRadiusMatches(4.5)
            && liveView.meshRebuildCountForTest() == initialRebuildCount
            && liveView.thingTriangleCacheVertexCountForTest() == initialTriangleVertices
            && liveView.thingTriangleCacheFingerprintForTest() == initialTriangleFingerprint,
        "inherited collision size edits remain shadowed by the effective local override without mesh rebuild");

    // The model and its lines are in separate buffers. A live viewport drag or
    // item-editor scrub leaves authored Thing poses untouched and transforms
    // the displayed vertices in the shader. Compare actual framebuffer pixels
    // so stale overlay lines cannot pass by merely updating a CPU cache.
    liveLevel.things[0].values[0].floatValue = 0.8;
    liveView.notifyThingsChanged();
    liveView.resize(640, 480);
    liveView.setBackgroundClearColor(Qt::black);
    liveView.setShowGrid(false);
    liveView.setShowWireframe(false);
    liveView.setShowVertices(false);
    liveView.setShowSolid(true);
    liveView.setShowThingVisualBounds(true);
    liveView.setShowThingGizmos(false);
    liveView.setShowAxisWidget(false);
    liveView.setShowToastOverlays(false);
    liveView.setShowEditorCubes(false);
    liveView.setLightingMode(GlView::LightingMode::FullyLit);
    liveView.setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    liveView.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
    liveView.frameWorldBounds(
        QVector3D{ -4.0f, -4.0f, -2.0f },
        QVector3D{ 4.0f, 4.0f, 2.0f },
        false);
    liveView.show();
    QApplication::processEvents(QEventLoop::AllEvents);
    const QImage baseFrame = liveView.grabFramebuffer();
    const OverlayPixelCenter baseCollision = overlayPixelCenter(baseFrame, true);
    const OverlayPixelCenter baseBounds = overlayPixelCenter(baseFrame, false);
    const auto originScreen = liveView.projectWorldToViewport(QVector3D{});
    const auto movedScreen = liveView.projectWorldToViewport(QVector3D{ 2.0f, 0.0f, 0.0f });
    check(!baseFrame.isNull()
            && baseCollision.count > 20u
            && baseBounds.count > 10u
            && originScreen && movedScreen,
        "live Thing overlay fixture visibly renders collision and visual bounds");
    if (!baseFrame.isNull() && baseCollision.count > 20u
        && baseBounds.count > 10u && originScreen && movedScreen)
    {
        const double pixelsPerLogicalX = static_cast<double>(baseFrame.width())
            / static_cast<double>(liveView.width());
        const double pixelsPerLogicalY = static_cast<double>(baseFrame.height())
            / static_cast<double>(liveView.height());
        const QPointF logicalMove = *movedScreen - *originScreen;
        const QPointF expectedMove{
            logicalMove.x() * pixelsPerLogicalX,
            logicalMove.y() * pixelsPerLogicalY };
        const auto followsMove = [&expectedMove](
            const OverlayPixelCenter& before,
            const OverlayPixelCenter& after)
        {
            return after.count > 10u
                && QLineF(after.point - before.point, expectedMove).length() < 8.0;
        };
        const std::uint64_t previewRebuildCount = liveView.meshRebuildCountForTest();

        liveView.setSelectedThing(0);
        liveView.beginThingMoveTransformForTest(QVector3D{ 2.0f, 0.0f, 0.0f });
        const OverlayPixelCenter dragCollision = overlayPixelCenter(
            liveView.grabFramebuffer(), true);
        check(liveView.transformPreviewActiveForTest()
                && followsMove(baseCollision, dragCollision)
                && liveLevel.things[0].x == 0.0
                && liveView.meshRebuildCountForTest() == previewRebuildCount,
            "collision wire follows viewport Move preview without rebuilding or authoring the Thing");
        liveView.endTransformDragForTest();
        liveView.setSelectedThing(-1);
        const QImage itemBaseFrame = liveView.grabFramebuffer();
        const OverlayPixelCenter itemBaseCollision = overlayPixelCenter(
            itemBaseFrame, true);
        const OverlayPixelCenter itemBaseBounds = overlayPixelCenter(
            itemBaseFrame, false);

        QMatrix4x4 translation{};
        translation.setToIdentity();
        translation.translate(2.0f, 0.0f, 0.0f);
        const bool scrubStarted = liveView.setThingPropertyDeltaPreview(
            { 0 }, 0, translation, false);
        const QImage scrubFrame = liveView.grabFramebuffer();
        const OverlayPixelCenter scrubCollision = overlayPixelCenter(scrubFrame, true);
        const OverlayPixelCenter scrubBounds = overlayPixelCenter(scrubFrame, false);
        const double scrubCollisionCoverage = shiftedOverlayPixelCoverage(
            itemBaseFrame, scrubFrame, true, expectedMove, itemBaseFrame.rect());
        const double scrubBoundsCoverage = shiftedOverlayPixelCoverage(
            itemBaseFrame, scrubFrame, false, expectedMove, itemBaseFrame.rect());
        if (!scrubStarted || scrubCollisionCoverage < 0.65
            || scrubBoundsCoverage < 0.65
            || liveView.meshRebuildCountForTest() != previewRebuildCount)
        {
            std::cerr << "LIVE OVERLAY item Move: started=" << scrubStarted
                << " active=" << liveView.transformPreviewActiveForTest()
                << " property=" << liveView.thingPropertyTransformPreviewActiveForTest()
                << " collision before=" << itemBaseCollision.point.x() << ',' << itemBaseCollision.point.y()
                << " after=" << scrubCollision.point.x() << ',' << scrubCollision.point.y()
                << " count=" << scrubCollision.count
                << " bounds before=" << itemBaseBounds.point.x() << ',' << itemBaseBounds.point.y()
                << " after=" << scrubBounds.point.x() << ',' << scrubBounds.point.y()
                << " count=" << scrubBounds.count
                << " expected=" << expectedMove.x() << ',' << expectedMove.y()
                << " coverage=" << scrubCollisionCoverage << ',' << scrubBoundsCoverage
                << " rebuilds=" << liveView.meshRebuildCountForTest()
                << '/' << previewRebuildCount << '\n';
        }
        check(scrubStarted
                && scrubCollisionCoverage >= 0.65
                && scrubBoundsCoverage >= 0.65
                && liveView.meshRebuildCountForTest() == previewRebuildCount,
            "collision and bounds wire pixels follow Thing position scrub previews without rebuilding");
        liveView.clearThingPropertyTransformPreview();
        // Isolate the amber bounds for rotation. The green collision sphere
        // intersects its unit-square edges and changes which pixels retain
        // their amber color after the turn, even when both wires move.
        liveView.setShowThingCollisionShapes(false);
        const std::uint64_t rotationRebuildCount =
            liveView.meshRebuildCountForTest();
        const QImage rotationBaseFrame = liveView.grabFramebuffer();
        const OverlayPixelCenter rotationBaseBounds = overlayPixelCenter(
            rotationBaseFrame, false);

        QMatrix4x4 rotation{};
        rotation.setToIdentity();
        rotation.rotate(90.0f, QVector3D{ 0.0f, 0.0f, 1.0f });
        const bool rotationStarted = liveView.setThingPropertyDeltaPreview(
            { 0 }, 0, rotation, true);
        const QImage rotatedFrame = liveView.grabFramebuffer();
        const OverlayPixelCenter rotatedBounds = overlayPixelCenter(
            rotatedFrame, false);
        const auto boundsBefore = liveView.projectWorldToViewport(
            QVector3D{ 0.5f, 0.5f, 0.0f });
        const auto boundsAfter = liveView.projectWorldToViewport(
            QVector3D{ -0.5f, 0.5f, 0.0f });
        const QPointF logicalBoundsRotation = boundsBefore && boundsAfter
            ? *boundsAfter - *boundsBefore
            : QPointF{};
        const QPointF expectedBoundsRotation{
            logicalBoundsRotation.x() * pixelsPerLogicalX,
            logicalBoundsRotation.y() * pixelsPerLogicalY };
        // This fixture has a flat unit-square 3DO bounds. A quarter turn
        // around its origin moves the projected bounds exactly one unit left.
        const double boundsRotationCoverage = shiftedOverlayPixelCoverage(
            rotationBaseFrame, rotatedFrame, false, expectedBoundsRotation,
            rotationBaseFrame.rect());
        if (!rotationStarted || rotatedBounds.count <= 10u
            || !boundsBefore || !boundsAfter
            || boundsRotationCoverage < 0.65
            || liveView.meshRebuildCountForTest() != rotationRebuildCount)
        {
            std::cerr << "LIVE OVERLAY item Rotate: started=" << rotationStarted
                << " active=" << liveView.transformPreviewActiveForTest()
                << " property=" << liveView.thingPropertyTransformPreviewActiveForTest()
                << " bounds before=" << rotationBaseBounds.point.x() << ',' << rotationBaseBounds.point.y()
                << " after=" << rotatedBounds.point.x() << ',' << rotatedBounds.point.y()
                << " count=" << rotatedBounds.count
                << " expected=" << expectedBoundsRotation.x() << ',' << expectedBoundsRotation.y()
                << " coverage=" << boundsRotationCoverage
                << " rebuilds=" << liveView.meshRebuildCountForTest()
                << '/' << rotationRebuildCount << '\n';
        }
        check(rotationStarted && rotatedBounds.count > 10u
                && boundsBefore && boundsAfter
                && boundsRotationCoverage >= 0.65
                && liveView.meshRebuildCountForTest() == rotationRebuildCount,
            "visual-bounds wire pixels follow Thing rotation scrub before commit without rebuilding");
        liveView.clearThingPropertyTransformPreview();
        liveView.setShowThingCollisionShapes(true);

        // A face-collision wire is asymmetric, so rotating the Thing makes
        // its green outline visibly move even though a sphere would not.
        TplValue faceCollision{};
        faceCollision.name = "collide";
        faceCollision.type = ValueType::integer;
        faceCollision.intValue = 3;
        liveLevel.things[0].values.push_back(faceCollision);
        liveView.notifyThingsChanged();
        liveView.setSelectedThing(0);
        const OverlayPixelCenter faceBefore = overlayPixelCenter(
            liveView.grabFramebuffer(), true);
        const std::uint64_t rotateRebuildCount = liveView.meshRebuildCountForTest();
        const auto faceCenterBefore = liveView.projectWorldToViewport(
            QVector3D{ 1.0f / 3.0f, 1.0f / 3.0f, 0.0f });
        const auto faceCenterAfter = liveView.projectWorldToViewport(
            QVector3D{ -1.0f / 3.0f, 1.0f / 3.0f, 0.0f });
        liveView.beginThingRotateTransformForTest(
            QVector3D{ 0.0f, 0.0f, 1.0f }, 90.0f, QVector3D{});
        const OverlayPixelCenter faceAfter = overlayPixelCenter(
            liveView.grabFramebuffer(), true);
        const QPointF logicalFaceRotation = faceCenterBefore && faceCenterAfter
            ? *faceCenterAfter - *faceCenterBefore
            : QPointF{};
        const QPointF expectedFaceRotation{
            logicalFaceRotation.x() * pixelsPerLogicalX,
            logicalFaceRotation.y() * pixelsPerLogicalY };
        check(faceBefore.count > 10u && faceAfter.count > 10u
                && faceCenterBefore && faceCenterAfter
                && QLineF(
                    faceAfter.point - faceBefore.point,
                    expectedFaceRotation).length() < 8.0
                && liveLevel.things[0].yaw == 0.0
                && liveView.meshRebuildCountForTest() == rotateRebuildCount,
            "face-collision wire follows viewport Rotate preview without rebuilding or authoring the Thing");
        liveView.endTransformDragForTest();
        liveView.setSelectedThing(-1);
        liveLevel.things[0].values.pop_back();
        liveView.notifyThingsChanged();

        Thing secondThing = liveLevel.things[0];
        secondThing.x = -2.0;
        secondThing.y = -2.0;
        liveLevel.things.push_back(secondThing);
        liveView.notifyThingsChanged();
        const QImage twoThingFrame = liveView.grabFramebuffer();
        const OverlayPixelCenter twoThingCollision = overlayPixelCenter(
            twoThingFrame, true);
        const OverlayPixelCenter twoThingBounds = overlayPixelCenter(
            twoThingFrame, false);
        const std::uint64_t multiRebuildCount = liveView.meshRebuildCountForTest();
        check(liveView.setThingPropertyDeltaPreview(
                { 0, 1 }, 0, translation, false),
            "multi-Thing item scrub starts a shared transform preview");
        const QImage twoThingMovedFrame = liveView.grabFramebuffer();
        const OverlayPixelCenter multiCollision = overlayPixelCenter(
            twoThingMovedFrame, true);
        const OverlayPixelCenter multiBounds = overlayPixelCenter(
            twoThingMovedFrame, false);
        const auto firstScreen = liveView.projectWorldToViewport(QVector3D{});
        const auto secondScreen = liveView.projectWorldToViewport(
            QVector3D{ -2.0f, -2.0f, 0.0f });
        const auto ownerRegion = [&](const std::optional<QPointF>& screen)
        {
            if (!screen)
            {
                return QRect{};
            }
            const int x = static_cast<int>(std::lround(
                screen->x() * pixelsPerLogicalX));
            const int y = static_cast<int>(std::lround(
                screen->y() * pixelsPerLogicalY));
            // The test sphere has radius 0.8 and the 3DO bounds are one
            // unit wide, so each owner's wire fits inside this region.
            return QRect{ x - 90, y - 90, 181, 181 };
        };
        const QRect firstRegion = ownerRegion(firstScreen);
        const QRect secondRegion = ownerRegion(secondScreen);
        const double firstCollisionCoverage = shiftedOverlayPixelCoverage(
            twoThingFrame, twoThingMovedFrame, true, expectedMove, firstRegion);
        const double secondCollisionCoverage = shiftedOverlayPixelCoverage(
            twoThingFrame, twoThingMovedFrame, true, expectedMove, secondRegion);
        const double firstBoundsCoverage = shiftedOverlayPixelCoverage(
            twoThingFrame, twoThingMovedFrame, false, expectedMove, firstRegion);
        const double secondBoundsCoverage = shiftedOverlayPixelCoverage(
            twoThingFrame, twoThingMovedFrame, false, expectedMove, secondRegion);
        if (!firstScreen || !secondScreen
            || firstCollisionCoverage < 0.65 || secondCollisionCoverage < 0.65
            || firstBoundsCoverage < 0.65 || secondBoundsCoverage < 0.65
            || liveView.meshRebuildCountForTest() != multiRebuildCount)
        {
            std::cerr << "LIVE OVERLAY multi Move: active="
                << liveView.transformPreviewActiveForTest()
                << " property=" << liveView.thingPropertyTransformPreviewActiveForTest()
                << " collision before=" << twoThingCollision.point.x() << ',' << twoThingCollision.point.y()
                << " after=" << multiCollision.point.x() << ',' << multiCollision.point.y()
                << " count=" << multiCollision.count
                << " bounds before=" << twoThingBounds.point.x() << ',' << twoThingBounds.point.y()
                << " after=" << multiBounds.point.x() << ',' << multiBounds.point.y()
                << " count=" << multiBounds.count
                << " expected=" << expectedMove.x() << ',' << expectedMove.y()
                << " coverage=" << firstCollisionCoverage << ',' << secondCollisionCoverage
                << ',' << firstBoundsCoverage << ',' << secondBoundsCoverage
                << " rebuilds=" << liveView.meshRebuildCountForTest()
                << '/' << multiRebuildCount << '\n';
        }
        check(firstScreen && secondScreen
                && firstCollisionCoverage >= 0.65
                && secondCollisionCoverage >= 0.65
                && firstBoundsCoverage >= 0.65
                && secondBoundsCoverage >= 0.65
                && liveLevel.things[0].x == 0.0
                && liveLevel.things[1].x == -2.0
                && liveView.meshRebuildCountForTest() == multiRebuildCount,
            "each selected Thing collision and bounds wire pixels follow a multi-Thing scrub sample");
        liveView.clearThingPropertyTransformPreview();
    }
    liveView.hide();

    // Read Visual Bounds from the actual published overlay cache. Fixed Editor
    // type-0 and type-2 cards should match their rendered quad AABBs, while a
    // type-0 Game Preview billboard retains its orbit-safe conservative cube.
    QTemporaryDir spriteRoot{};
    check(spriteRoot.isValid(),
        "temporary SPR root is available for Visual Bounds fixtures");
    if (spriteRoot.isValid())
    {
        const std::filesystem::path spriteDirectory{ spriteRoot.path().toStdString() };
        {
            std::ofstream editorSprite(spriteDirectory / "fixed_editor.spr");
            editorSprite << "fixture.mat 0 4 2 1 1 0 0 1 2 3\n";
            std::ofstream type2Sprite(spriteDirectory / "fixed_type2.spr");
            // Type-2 renderer intentionally ignores the large offsets.
            type2Sprite << "fixture.mat 2 2 6 1 1 0 0 7 11 13\n";
            std::ofstream billboardSprite(spriteDirectory / "billboard.spr");
            billboardSprite << "fixture.mat 0 4 6 1 1 0 0 1 2 2\n";
        }

        Level spriteLevel{};
        spriteLevel.kind = ProjectType::IJIM;
        spriteLevel.layers.emplace_back("Layer0");
        Thing editorSpriteThing{};
        editorSpriteThing.name = "fixed_editor";
        editorSpriteThing.layer = 0;
        editorSpriteThing.sector = -1;
        editorSpriteThing.x = 10.0;
        editorSpriteThing.y = 20.0;
        editorSpriteThing.z = 30.0;
        TplValue editorSpriteValue{};
        editorSpriteValue.name = "sprite";
        editorSpriteValue.type = ValueType::str;
        editorSpriteValue.stringValue = "fixed_editor";
        editorSpriteThing.values.push_back(editorSpriteValue);
        spriteLevel.things.push_back(editorSpriteThing);

        Thing type2SpriteThing{};
        type2SpriteThing.name = "fixed_type2";
        type2SpriteThing.layer = 0;
        type2SpriteThing.sector = -1;
        type2SpriteThing.x = -10.0;
        type2SpriteThing.y = 5.0;
        type2SpriteThing.z = 8.0;
        TplValue type2SpriteValue{};
        type2SpriteValue.name = "sprite";
        type2SpriteValue.type = ValueType::str;
        type2SpriteValue.stringValue = "fixed_type2";
        type2SpriteThing.values.push_back(type2SpriteValue);
        TplValue type2ModelValue{};
        type2ModelValue.name = "model3d";
        type2ModelValue.type = ValueType::str;
        type2ModelValue.stringValue = "collision_overlay_live_test.3do";
        type2SpriteThing.values.push_back(type2ModelValue);
        spriteLevel.things.push_back(type2SpriteThing);

        assets::ResourceLocator spriteResources{};
        spriteResources.addRoot(spriteDirectory);
        GlView spriteView{};
        spriteView.setResourceLocator(&spriteResources, false);
        spriteView.setRenderPipeline(GlView::RenderPipeline::Editor);
        spriteView.setShowThings(true);
        spriteView.setShowThingVisualBounds(true);
        spriteView.setLevel(&spriteLevel);

        const auto editorSpriteBounds =
            spriteView.thingVisualBoundsOverlayAabbForTest(0u, false);
        const auto type2SpriteBounds =
            spriteView.thingVisualBoundsOverlayAabbForTest(1u, false);
        check(editorSpriteBounds
                && std::abs((*editorSpriteBounds)[0] - 9.0) < 1.0e-5
                && std::abs((*editorSpriteBounds)[1] - 22.0) < 1.0e-5
                && std::abs((*editorSpriteBounds)[2] - 32.0) < 1.0e-5
                && std::abs((*editorSpriteBounds)[3] - 13.0) < 1.0e-5
                && std::abs((*editorSpriteBounds)[4] - 22.0) < 1.0e-5
                && std::abs((*editorSpriteBounds)[5] - 34.0) < 1.0e-5,
            "Editor type-0 Visual Bounds match the current sprite card, including authored offset");
        check(type2SpriteBounds
                && std::abs((*type2SpriteBounds)[0] + 11.0) < 1.0e-5
                && std::abs((*type2SpriteBounds)[1] - 5.0) < 1.0e-5
                && std::abs((*type2SpriteBounds)[2] - 5.0) < 1.0e-5
                && std::abs((*type2SpriteBounds)[3] + 9.0) < 1.0e-5
                && std::abs((*type2SpriteBounds)[4] - 5.0) < 1.0e-5
                && std::abs((*type2SpriteBounds)[5] - 11.0) < 1.0e-5,
            "IJIM type-2 Visual Bounds match the fixed rendered card and ignore renderer-ignored SPR offsets");

        // Classic rdSprite_Draw ignores the stored type token. Its billboard
        // retains sprite priority over a 3DO on the same Thing and uses offsets.
        for (const ProjectType project : { ProjectType::JKDF2, ProjectType::MOTS })
        {
            Level classicLevel{};
            classicLevel.kind = project;
            classicLevel.layers.emplace_back("Layer0");
            classicLevel.things.push_back(type2SpriteThing);

            GlView classicView{};
            classicView.setResourceLocator(&spriteResources, false);
            classicView.primeModel3do(
                "collision_overlay_live_test.3do", liveModel);
            classicView.setThingViewMode(GlView::ThingViewMode::Shaded);
            classicView.setShowThingVisualBounds(true);
            classicView.setLevel(&classicLevel);
            classicView.resize(640, 480);
            classicView.show();
            QElapsedTimer renderWait{};
            renderWait.start();
            while ( !classicView.isValid() && renderWait.elapsed() < 3500 )
            {
                QApplication::processEvents(QEventLoop::AllEvents);
            }
            check(classicView.isValid() && !classicView.grabFramebuffer().isNull(),
                "classic stored type-2 bounds fixture renders before reading its textured quad cache");
            const auto classicBounds =
                classicView.thingVisualBoundsOverlayAabbForTest(0u, false);
            check(classicView.thingSpriteCornersForTest(0u).has_value()
                    && classicView.thingPickTriangleVertexCountForTest(0u) == 0u
                    && classicBounds
                    && std::abs((*classicBounds)[0] + 4.0) < 1.0e-5
                    && std::abs((*classicBounds)[1] - 16.0) < 1.0e-5
                    && std::abs((*classicBounds)[2] - 18.0) < 1.0e-5
                    && std::abs((*classicBounds)[3] + 2.0) < 1.0e-5
                    && std::abs((*classicBounds)[4] - 16.0) < 1.0e-5
                    && std::abs((*classicBounds)[5] - 24.0) < 1.0e-5,
                "classic stored type-2 SPR renders its original-engine billboard with matching Visual Bounds");
        }

        Level billboardLevel{};
        billboardLevel.kind = ProjectType::JKDF2;
        billboardLevel.layers.emplace_back("Layer0");
        Thing billboardThing{};
        billboardThing.name = "billboard";
        billboardThing.layer = 0;
        billboardThing.sector = -1;
        billboardThing.x = 4.0;
        billboardThing.y = 5.0;
        billboardThing.z = 6.0;
        TplValue billboardValue{};
        billboardValue.name = "sprite";
        billboardValue.type = ValueType::str;
        billboardValue.stringValue = "billboard";
        billboardThing.values.push_back(billboardValue);
        billboardLevel.things.push_back(billboardThing);

        GlView billboardView{};
        billboardView.setResourceLocator(&spriteResources, false);
        billboardView.setRenderPipeline(GlView::RenderPipeline::GamePreview);
        billboardView.setShowThings(true);
        billboardView.setShowThingVisualBounds(true);
        billboardView.setLevel(&billboardLevel);
        const auto billboardBounds =
            billboardView.thingVisualBoundsOverlayAabbForTest(0u, false);
        const double billboardRadius = 3.0 + std::sqrt(13.0);
        check(billboardBounds
                && std::abs((*billboardBounds)[0] - (4.0 - billboardRadius)) < 1.0e-5
                && std::abs((*billboardBounds)[1] - (5.0 - billboardRadius)) < 1.0e-5
                && std::abs((*billboardBounds)[2] - (6.0 - billboardRadius)) < 1.0e-5
                && std::abs((*billboardBounds)[3] - (4.0 + billboardRadius)) < 1.0e-5
                && std::abs((*billboardBounds)[4] - (5.0 + billboardRadius)) < 1.0e-5
                && std::abs((*billboardBounds)[5] - (6.0 + billboardRadius)) < 1.0e-5,
            "Game Preview type-0 Visual Bounds keep the orbit-safe conservative cube");

        Level visibilityLevel{};
        visibilityLevel.kind = ProjectType::JKDF2;
        visibilityLevel.layers.emplace_back("Thing");
        visibilityLevel.layers.emplace_back("Occluder");
        Thing visibleThing{};
        visibleThing.name = "face_collision_overlay_visibility";
        visibleThing.layer = 0;
        visibleThing.sector = -1;
        TplValue visibleModelName{};
        visibleModelName.name = "model3d";
        visibleModelName.type = ValueType::str;
        visibleModelName.stringValue = "face_collision_overlay_visibility.3do";
        TplValue visibleCollide{};
        visibleCollide.name = "collide";
        visibleCollide.type = ValueType::integer;
        visibleCollide.intValue = 3;
        visibleThing.values = { visibleModelName, visibleCollide };
        visibilityLevel.things.push_back(visibleThing);

        auto visibilityModel = std::make_shared<assets::Model3do>();
        visibilityModel->version = 2.1;
        assets::Model3doMesh visibilityMesh{};
        visibilityMesh.name = "face_collision_overlay_visibility";
        visibilityMesh.geo = static_cast<std::int32_t>(TGeoMode::Solid);
        visibilityMesh.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
        visibilityMesh.vertices = {
            assets::Model3doVertex{ -1.0, -1.0, 0.0, ColorWhite },
            assets::Model3doVertex{  1.0, -1.0, 0.0, ColorWhite },
            assets::Model3doVertex{  1.0,  1.0, 0.0, ColorWhite },
            assets::Model3doVertex{ -1.0,  1.0, 0.0, ColorWhite },
        };
        assets::Model3doFace visibilityFace{};
        visibilityFace.geo = static_cast<std::int32_t>(TGeoMode::Solid);
        visibilityFace.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
        visibilityFace.extraLight = ColorBlack;
        visibilityFace.faceFlags = FF_DoubleSided;
        visibilityFace.vertices = { 0, 1, 2, 3 };
        visibilityMesh.faces.push_back(visibilityFace);
        visibilityModel->meshes.push_back(visibilityMesh);

        Sector occluder{};
        occluder.layer = 1;
        occluder.ambient = ColorWhite;
        occluder.vertices = {
            Vertex{ -0.3, -1.2, 0.25 },
            Vertex{  0.3, -1.2, 0.25 },
            Vertex{  0.3, -0.7, 0.25 },
            Vertex{ -0.3, -0.7, 0.25 },
        };
        Surface occluderSurface{};
        occluderSurface.geo = static_cast<std::int32_t>(TGeoMode::Solid);
        occluderSurface.faceFlags = FF_DoubleSided;
        occluderSurface.normal = TVector{ 0.0, 0.0, 1.0 };
        occluderSurface.vertices = { 0, 1, 2, 3 };
        occluderSurface.texVertices.resize(4u);
        for (TexVertex& texVertex : occluderSurface.texVertices)
        {
            texVertex.color = ColorWhite;
        }
        occluder.surfaces.push_back(occluderSurface);
        visibilityLevel.sectors.push_back(occluder);

        assets::ResourceLocator visibilityResources{};
        GlView visibilityView{};
        visibilityView.setAttribute(Qt::WA_DontShowOnScreen);
        visibilityView.resize(320, 320);
        visibilityView.setResourceLocator(&visibilityResources, false);
        visibilityView.primeModel3do(
            "face_collision_overlay_visibility.3do", visibilityModel);
        visibilityView.setBackgroundClearColor(Qt::black);
        visibilityView.setShowGrid(false);
        visibilityView.setShowWireframe(false);
        visibilityView.setShowVertices(false);
        visibilityView.setShowSolid(true);
        visibilityView.setShowTextures(false);
        visibilityView.setShowThings(true);
        visibilityView.setShowThingGizmos(false);
        visibilityView.setShowAxisWidget(false);
        visibilityView.setShowToastOverlays(false);
        visibilityView.setShowEditorCubes(false);
        visibilityView.setShowThingCollisionShapes(true);
        visibilityView.setShowThingVisualBounds(true);
        visibilityView.setThingViewMode(GlView::ThingViewMode::Shaded);
        visibilityView.setLightingMode(GlView::LightingMode::FullyLit);
        visibilityView.setCameraProjectionType(
            app::ViewportCamera::ProjectionType::Orthographic);
        visibilityView.setLevel(&visibilityLevel);
        visibilityView.setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
        visibilityView.frameWorldBounds(
            QVector3D{ -1.5f, -1.5f, -0.5f },
            QVector3D{  1.5f,  1.5f, 0.5f },
            false);
        visibilityView.setCameraClipEnabled(true);
        visibilityView.setCameraNearFar(-10000.0f, 10000.0f);
        visibilityView.show();
        QApplication::processEvents(QEventLoop::AllEvents);
        (void)visibilityView.grabFramebuffer();
        visibilityView.repaint();
        QApplication::processEvents(QEventLoop::AllEvents);
        const QImage visibilityFrame = visibilityView.grabFramebuffer();
        const auto projectedPixel = [&visibilityView, &visibilityFrame](
            const QVector3D& world) -> std::optional<QPointF>
        {
            if (visibilityFrame.isNull())
            {
                return std::nullopt;
            }
            const auto point = visibilityView.projectWorldToViewport(world);
            if (!point)
            {
                return std::nullopt;
            }
            return QPointF{
                point->x() * static_cast<double>(visibilityFrame.width())
                    / std::max(1, visibilityView.width()),
                point->y() * static_cast<double>(visibilityFrame.height())
                    / std::max(1, visibilityView.height()),
            };
        };
        const auto hasGreenCollisionPixel = [&visibilityFrame](const QPointF& point)
        {
            const int centerX = static_cast<int>(std::lround(point.x()));
            const int centerY = static_cast<int>(std::lround(point.y()));
            for (int dy = -3; dy <= 3; ++dy)
            {
                for (int dx = -3; dx <= 3; ++dx)
                {
                    const int x = centerX + dx;
                    const int y = centerY + dy;
                    if (x < 0 || y < 0
                        || x >= visibilityFrame.width() || y >= visibilityFrame.height())
                    {
                        continue;
                    }
                    const QColor color = visibilityFrame.pixelColor(x, y);
                    if (color.green() > 150
                        && color.green() > color.red() * 1.35
                        && color.green() > color.blue() * 1.35)
                    {
                        return true;
                    }
                }
            }
            return false;
        };
        const auto visibleEdge = projectedPixel(QVector3D{ -0.75f, -1.0f, 0.0f });
        const auto occludedEdge = projectedPixel(QVector3D{ 0.0f, -1.0f, 0.0f });
        check(visibleEdge && hasGreenCollisionPixel(*visibleEdge),
            "face collision wire remains visibly green over coincident amber Visual Bounds");
        check(occludedEdge && !hasGreenCollisionPixel(*occludedEdge),
            "face collision wire remains hidden behind completed opaque scene depth");
        visibilityView.hide();
    }
}

} // namespace sed::app_tests
