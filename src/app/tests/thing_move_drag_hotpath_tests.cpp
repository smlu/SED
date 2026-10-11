#include "test_common.hpp"

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "resource_release_test_fixtures.hpp"
#include "sed/assets/sed.hpp"
#include "sed/level/build.hpp"

#include <QApplication>
#include <QTemporaryDir>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sed::app_tests
{

namespace
{

constexpr std::size_t kMovingThingCount = 32;
constexpr std::size_t kStationaryThingCount = 512;
constexpr std::size_t kTotalThingCount =
    kMovingThingCount + kStationaryThingCount;
constexpr int kPointerSamples = 5;

editor::DocumentState makeThingMoveHotPathState()
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.header.version = 3;
    state.level.layers = { "Layer0" };

    // Keep a populated, indexed surface set in the same fixture. Surface Stick
    // must use the indexed path during a Thing drag rather than issuing one
    // complete level ray query for every support sample.
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            (void)level::levelCreateCube(
                state.level,
                -2.0 + static_cast<double>(column) * 1.25,
                -2.0 + static_cast<double>(row) * 1.25,
                -0.5,
                TVector::up,
                TVector::right);
        }
    }

    ThingTemplate hotPathTemplate{};
    hotPathTemplate.name = "+drag_hotpath_fixture";
    hotPathTemplate.bbox.Set(-0.125, -0.125, -0.125, 0.125, 0.125, 0.125);
    TplValue model3d{};
    model3d.name = "model3d";
    model3d.type = ValueType::str;
    model3d.stringValue = "synthetic_drag_hotpath.3do";
    hotPathTemplate.values.push_back(std::move(model3d));
    state.level.templates.push_back(std::move(hotPathTemplate));

    state.level.things.reserve(kTotalThingCount);
    for (std::size_t index = 0; index < kTotalThingCount; ++index)
    {
        Thing thing{};
        thing.id = static_cast<std::int32_t>(index + 1u);
        thing.num = static_cast<std::int32_t>(index);
        thing.name = "+drag_hotpath_fixture";
        thing.templateName = thing.name;
        thing.layer = 0;
        thing.sector = 0;
        thing.bbox.Set(-0.125, -0.125, -0.125, 0.125, 0.125, 0.125);

        if (index < kMovingThingCount)
        {
            const std::size_t column = index % 8u;
            const std::size_t row = index / 8u;
            thing.x = -0.28 + static_cast<double>(column) * 0.08;
            thing.y = -0.16 + static_cast<double>(row) * 0.08;
            thing.z = 0.0;
            state.selection.things.insert(static_cast<std::int32_t>(index));
        }
        else
        {
            const std::size_t stationary = index - kMovingThingCount;
            thing.x = 32.0 + static_cast<double>(stationary % 32u) * 0.75;
            thing.y = -12.0 + static_cast<double>(stationary / 32u) * 0.75;
            thing.z = 0.0;
        }
        state.level.things.push_back(std::move(thing));
    }
    return state;
}

bool sameThingSelection(
    const editor::Selection& actual,
    const editor::Selection& expected)
{
    return actual.things.size() == expected.things.size()
        && std::ranges::all_of(
            expected.things,
            [&actual](const std::int32_t index)
            {
                return actual.things.contains(index);
            });
}

void checkCached(
    const bool ok,
    const std::string_view label,
    const std::string_view detail)
{
    std::string message{};
    message.reserve(label.size() + detail.size());
    message.append(label);
    message.append(detail);
    check(ok, message.c_str());
}

void logCachedThingPose(const std::string_view label, const Thing& thing)
{
    std::cerr.precision(17);
    std::cerr << "CachedThingPose " << label
              << " x=" << thing.x
              << " y=" << thing.y
              << " z=" << thing.z
              << " yaw=" << thing.yaw
              << " pch=" << thing.pch
              << " rol=" << thing.rol << '\n';
}

enum class CachedVisual : std::uint8_t
{
    editorBox,
    model,
    tinyModel,
    sprite
};

void addTemplateStringValue(
    ThingTemplate& tpl,
    const std::string_view name,
    const std::string_view value)
{
    TplValue entry{};
    entry.name = std::string(name);
    entry.type = ValueType::str;
    entry.stringValue = std::string(value);
    tpl.values.push_back(std::move(entry));
}

void setThingIdentity(Thing& thing, const std::int32_t index, const std::string& templateName)
{
    thing.id = index + 1;
    thing.num = index;
    thing.name = templateName;
    thing.templateName = templateName;
    thing.layer = 0;
    thing.sector = 0;
}

editor::DocumentState makeCachedOutputState(
    const double origin,
    const CachedVisual visual,
    const double anchorX,
    const double anchorY,
    const double anchorYaw,
    const bool oneUlpAnchorBox = false)
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.header.version = 3;
    state.level.layers = { "Layer0" };

    constexpr std::string_view templateName = "+cached_output_fixture";
    ThingTemplate tpl{};
    tpl.name = std::string(templateName);
    if (visual == CachedVisual::model)
    {
        addTemplateStringValue(tpl, "model3d", "cached_drag_output.3do");
    }
    else if (visual == CachedVisual::tinyModel)
    {
        addTemplateStringValue(tpl, "model3d", "cached_tiny_vertical.3do");
    }
    else if (visual == CachedVisual::sprite)
    {
        addTemplateStringValue(tpl, "sprite", "cached_drag_output.spr");
    }
    state.level.templates.push_back(std::move(tpl));

    Thing moving{};
    setThingIdentity(moving, 0, std::string(templateName));
    moving.x = origin;
    moving.y = origin;
    moving.z = 0.0;
    if (visual == CachedVisual::editorBox)
    {
        moving.bbox.Set(-0.0001, -0.0001, -0.0001, 0.0001, 0.0001, 0.0001);
    }
    state.level.things.push_back(moving);

    Thing anchor{};
    setThingIdentity(anchor, 1, std::string(templateName));
    anchor.x = anchorX;
    anchor.y = anchorY;
    anchor.z = 0.0;
    anchor.yaw = anchorYaw;
    if (visual == CachedVisual::editorBox)
    {
        const double nextHalf = std::nextafter(
            0.0001,
            std::numeric_limits<double>::infinity());
        anchor.bbox.Set(
            -0.0001,
            -0.0001,
            -0.0001,
            oneUlpAnchorBox ? nextHalf : 0.0001,
            0.0001,
            0.0001);
    }
    state.level.things.push_back(anchor);
    state.selection.things.insert(0);
    return state;
}

bool writeCachedOutputResource(
    const std::filesystem::path& path,
    const QByteArray& bytes)
{
    std::ofstream output(path, std::ios::binary);
    output.write(bytes.constData(), bytes.size());
    return output.good();
}

QByteArray makeCachedVolumetricModelFixture()
{
    // The preferred model solver needs a real volume so its X face is
    // distinguishable from the Y/Z contacts. Keep the box asymmetric in X
    // while retaining nonzero extents on every other axis.
    return QByteArrayLiteral(
        "SECTION: HEADER\n"
        "3DO 2.3\n"
        "SECTION: GEOMETRYDEF\n"
        "RADIUS 1\n"
        "INSERT OFFSET 0 0 0\n"
        "GEOSETS 1\n"
        "GEOSET 0\n"
        "MESHES 1\n"
        "MESH 0\n"
        "NAME cached_output_box\n"
        "VERTICES 8\n"
        "0: -0.002 -0.1 -0.1\n"
        "1: 0.002 -0.1 -0.1\n"
        "2: 0.002 0.1 -0.1\n"
        "3: -0.002 0.1 -0.1\n"
        "4: -0.002 -0.1 0.1\n"
        "5: 0.002 -0.1 0.1\n"
        "6: 0.002 0.1 0.1\n"
        "7: -0.002 0.1 0.1\n"
        "FACES 0\n"
        "SECTION: HIERARCHYDEF\n"
        "HIERARCHY NODES 1\n"
        "0: 0x0 0x1 0 -1 -1 -1 0 0 0 0 0 0 0 0 0 0 cached_output_box\n");
}

QByteArray makeCachedTinyVerticalModelFixture()
{
    // The preferred X-face candidate is laterally close but vertically .18
    // away. This should survive weighted model scoring despite the legacy
    // three-radius broad cull rejecting its center distance.
    return QByteArrayLiteral(
        "SECTION: HEADER\n"
        "3DO 2.3\n"
        "SECTION: GEOMETRYDEF\n"
        "RADIUS 1\n"
        "INSERT OFFSET 0 0 0\n"
        "GEOSETS 1\n"
        "GEOSET 0\n"
        "MESHES 1\n"
        "MESH 0\n"
        "NAME cached_tiny_vertical\n"
        "VERTICES 8\n"
        "0: -0.001 -0.001 -0.001\n"
        "1: 0.001 -0.001 -0.001\n"
        "2: 0.001 0.001 -0.001\n"
        "3: -0.001 0.001 -0.001\n"
        "4: -0.001 -0.001 0.001\n"
        "5: 0.001 -0.001 0.001\n"
        "6: 0.001 0.001 0.001\n"
        "7: -0.001 0.001 0.001\n"
        "FACES 0\n"
        "SECTION: HIERARCHYDEF\n"
        "HIERARCHY NODES 1\n"
        "0: 0x0 0x1 0 -1 -1 -1 0 0 0 0 0 0 0 0 0 0 0 cached_tiny_vertical\n");
}

void prepareCachedOutputWindow(
    MainWindow& window,
    GlView& view,
    const std::filesystem::path& resourceRoot,
    editor::DocumentState state)
{
    if (resourceRoot.empty())
    {
        window.setResourcePathsForTest({}, {});
    }
    else
    {
        window.setResourcePathsForTest({ resourceRoot }, {});
    }
    window.setDocumentStateForTest(std::move(state));
    window.activateThingEditModeForTest();
    // Mode activation may clear the authored selection while rebuilding edit
    // actions. Restore the fixture selection before beginning the gesture.
    window.mutableDocumentStateForTest().selection.things.insert(0);
    window.updateEditActionsForTest();
    view.resize(800, 600);
    view.setLevel(
        &window.mutableDocumentStateForTest().level,
        GlView::SetLevelMode::resetView);
    view.fitViewToLevel();
    QApplication::processEvents();
}

std::optional<Thing> commitCachedOutputGesture(
    MainWindow& window,
    GlView& view,
    const bool rotateToSnap,
    const int pointerSteps,
    const std::string_view label)
{
    const auto beforeBytes = assets::saveToSedBytes(window.documentStateForTest().level);
    const Thing before = window.documentStateForTest().level.things[0];
    checkCached(
        view.beginThingMoveDragForTest({ 0 }, false, true),
        label,
        " begins a real cached Thing drag");
    if (!view.transformPreviewActiveForTest())
    {
        checkCached(false, label, " publishes an active Thing preview");
        return std::nullopt;
    }
    if (rotateToSnap)
    {
        // The test seam starts with this option disabled to make the basic
        // fixture deterministic. Turning it on after begin exercises the same
        // captured cache and the production Rotate-to-snap branch.
        view.setThingMoveSnapRotateEnabled(true);
    }
    view.updateThingMoveDragSampleForTest(pointerSteps);
    checkCached(
        assets::saveToSedBytes(window.documentStateForTest().level) == beforeBytes,
        label,
        " keeps authored Level bytes unchanged during preview");
    checkCached(
        window.documentStateForTest().level.things[0].x == before.x
            && window.documentStateForTest().level.things[0].y == before.y
            && window.documentStateForTest().level.things[0].z == before.z
            && window.documentStateForTest().level.things[0].yaw == before.yaw
            && window.documentStateForTest().level.things[0].pch == before.pch
            && window.documentStateForTest().level.things[0].rol == before.rol,
        label,
        " keeps the authored Thing pose unchanged during preview");
    view.endTransformDragForTest();
    checkCached(
        !view.transformPreviewActiveForTest(),
        label,
        " clears the preview after commit");
    return window.documentStateForTest().level.things[0];
}

void runThingMoveCachedOutputTests()
{
    QTemporaryDir resourceDirectory{};
    check(resourceDirectory.isValid(),
        "cached Thing output fixture creates a temporary resource root");
    if (!resourceDirectory.isValid())
    {
        return;
    }
    const std::filesystem::path resourceRoot(
        resourceDirectory.path().toStdWString());
    check(writeCachedOutputResource(
              resourceRoot / "cached_drag_output.3do",
              makeCachedVolumetricModelFixture()),
        "cached Thing output fixture writes a valid volumetric model");
    check(writeCachedOutputResource(
              resourceRoot / "cached_tiny_vertical.3do",
              makeCachedTinyVerticalModelFixture()),
        "cached Thing output fixture writes a valid tiny vertical model");
    check(writeCachedOutputResource(
              resourceRoot / "cached_drag_output.spr",
              QByteArrayLiteral("unused.mat 0 2 2 4 3 3 0 0 0 1 0 0 0\n")),
        "cached Thing output fixture writes a valid sprite");

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(800, 600);
    window.show();
    QApplication::processEvents();
    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "cached Thing output fixture exposes the real viewport");
    if (!view)
    {
        return;
    }

    // The model fixture has x extents [-.002,.002]. With the anchor at .05,
    // the deterministic preferred solver contacts its left face and leaves a
    // .046 translation. Repeat it at large document origins to catch accidental
    // float-frame conversions in the cache.
    for (const double origin : { 0.0, 10000.0, 1000000.0 })
    {
        prepareCachedOutputWindow(
            window,
            *view,
            resourceRoot,
            makeCachedOutputState(
                origin,
                CachedVisual::model,
                origin + 0.05,
                origin,
                0.0));
        const std::optional<Thing> result = commitCachedOutputGesture(
            window,
            *view,
            false,
            8,
            "model cached drag");
        if (result)
        {
            logCachedThingPose("model", *result);
            check(std::abs((result->x - origin) - 0.046) <= 2.0e-9,
                "model cached drag commits the fixed translated x contact");
            check(std::abs(result->y - origin) <= 2.0e-9
                    && std::abs(result->z) <= 2.0e-9
                    && result->yaw == 0.0
                    && result->pch == 0.0
                    && result->rol == 0.0,
                "model cached drag preserves the fixed y/z/rotation output");
            check(view->thingMoveBoundsResolveCountForTest() == 1,
                "model cached drag resolves one shared local visual box");
            check(view->thingMoveResourceResolveCountForTest() <= 3,
                "model cached drag resolves visual metadata once per gesture");
        }
    }

    // This fixture keeps the exact preferred X-face score below the solver
    // threshold while placing the anchor outside the current three-radius
    // broad phase. It is intentionally a distinct model identity.
    editor::DocumentState verticalState = makeCachedOutputState(
        0.0,
        CachedVisual::tinyModel,
        0.005,
        0.006,
        0.0);
    verticalState.level.things[1].z = 0.18;
    prepareCachedOutputWindow(
        window,
        *view,
        resourceRoot,
        std::move(verticalState));
    const std::optional<Thing> verticalResult = commitCachedOutputGesture(
        window,
        *view,
        false,
        0,
        "vertical diagonal model cached drag");
    if (verticalResult)
    {
        logCachedThingPose("vertical", *verticalResult);
        check(std::abs(verticalResult->x - 0.003) <= 2.0e-9
                && std::abs(verticalResult->y - 0.006) <= 2.0e-9
                && std::abs(verticalResult->z - 0.18) <= 2.0e-9
                && std::abs(verticalResult->yaw) <= 1.0e-12
                && std::abs(verticalResult->pch) <= 1.0e-12
                && std::abs(verticalResult->rol) <= 1.0e-12,
            "vertical diagonal model cached drag keeps the valid X-face contact");
        check(view->thingMoveSnapAnchorVisitCountForTest() == 1,
            "vertical diagonal model cached drag retains the weighted candidate after culling");
    }

    // Keep a paired score rejection so widening the broad phase alone cannot
    // make every distant vertical candidate snap. At .30 the weighted
    // lateral score exceeds the preferred solver radius and raw movement wins.
    editor::DocumentState rejectedVerticalState = makeCachedOutputState(
        0.0,
        CachedVisual::tinyModel,
        0.005,
        0.006,
        0.0);
    rejectedVerticalState.level.things[1].z = 0.30;
    prepareCachedOutputWindow(
        window,
        *view,
        resourceRoot,
        std::move(rejectedVerticalState));
    const std::optional<Thing> rejectedVerticalResult = commitCachedOutputGesture(
        window,
        *view,
        false,
        0,
        "rejected vertical model cached drag");
    if (rejectedVerticalResult)
    {
        logCachedThingPose("rejected-vertical", *rejectedVerticalResult);
        check(std::abs(rejectedVerticalResult->x) <= 2.0e-12
                && std::abs(rejectedVerticalResult->y) <= 2.0e-12
                && std::abs(rejectedVerticalResult->z) <= 2.0e-12
                && std::abs(rejectedVerticalResult->yaw) <= 1.0e-12,
            "rejected vertical model cached drag preserves raw movement");
    }

    // The sprite fixture is intentionally model-less, so it stays on the
    // generic box solver. The anchor is the fixed distance vector rotated by
    // five degrees. A zero-step sample then has an unambiguous X contact,
    // whose expected world vector is fixed below rather than derived from the
    // production solver.
    constexpr double spriteAnchorX = 2.0322371841071607;
    constexpr double spriteAnchorY = 0.1777977152052226;
    constexpr double spriteExpectedX = 0.0398477879236698;
    constexpr double spriteExpectedY = 0.00348622970990633;
    prepareCachedOutputWindow(
        window,
        *view,
        resourceRoot,
        makeCachedOutputState(
            0.0,
            CachedVisual::sprite,
            spriteAnchorX,
            spriteAnchorY,
            5.0));
    const std::optional<Thing> spriteResult = commitCachedOutputGesture(
        window,
        *view,
        true,
        0,
        "sprite rotating cached drag");
    if (spriteResult)
    {
        logCachedThingPose("sprite", *spriteResult);
        check(std::abs(spriteResult->x - spriteExpectedX) <= 2.0e-9
                && std::abs(spriteResult->y - spriteExpectedY) <= 2.0e-9
                && std::abs(spriteResult->z) <= 2.0e-6,
            "sprite Rotate-to-snap commits the fixed rotated X contact vector");
        check(std::abs(spriteResult->yaw - 5.0) <= 1.0e-6,
            "sprite Rotate-to-snap commits the fixed anchor rotation");
        check(view->thingMoveBoundsResolveCountForTest() == 1,
            "sprite cached drag resolves one shared local visual box");
    }

    // The diagonal point is close enough for the exact AABB score but outside
    // the old one-radius sphere. A one-ULP x extent must still produce a second
    // cache identity, rather than being folded by decimal formatting.
    prepareCachedOutputWindow(
        window,
        *view,
        {},
        makeCachedOutputState(
            0.0,
            CachedVisual::editorBox,
            0.025,
            0.060,
            0.0,
            true));
    const std::optional<Thing> diagonalResult = commitCachedOutputGesture(
        window,
        *view,
        false,
        8,
        "diagonal cached drag");
    if (diagonalResult)
    {
        logCachedThingPose("diagonal", *diagonalResult);
        check(std::abs(diagonalResult->x - 0.0248) <= 2.0e-12
                && std::abs(diagonalResult->y) <= 2.0e-12
                && std::abs(diagonalResult->z) <= 2.0e-12,
            "diagonal cached drag commits the tangent-preserving AABB contact");
        check(view->thingMoveBoundsResolveCountForTest() == 2,
            "diagonal cached drag keeps one-ULP-distinct bbox keys separate");
        check(view->thingMoveSnapAnchorVisitCountForTest() == 1,
            "diagonal cached drag retains the exact candidate after conservative culling");
    }

    prepareCachedOutputWindow(
        window,
        *view,
        {},
        makeCachedOutputState(
            0.0,
            CachedVisual::editorBox,
            0.40,
            0.40,
            0.0));
    const std::optional<Thing> distantResult = commitCachedOutputGesture(
        window,
        *view,
        false,
        8,
        "distant cached drag");
    if (distantResult)
    {
        logCachedThingPose("distant", *distantResult);
        // The orthographic test hook's fixed projection is 1/64 world units
        // wide, so eight horizontal pixels are this exact unsnapped movement.
        check(std::abs(distantResult->x - 0.00015625) <= 2.0e-12
                && std::abs(distantResult->y) <= 2.0e-12,
            "distant cached drag commits the fixed unsnapped movement");
        check(view->thingMoveSnapAnchorVisitCountForTest() == 0,
            "distant cached drag culls the remote anchor before exact scoring");
    }

    // Starting with snapping disabled must stay cheap. Enabling Grid Plane
    // Stick after begin exercises the lazy support transition on the same drag.
    // The YZ plane makes the horizontal test movement cross the plane normal,
    // so the committed value proves the modifier affected the result.
    prepareCachedOutputWindow(
        window,
        *view,
        {},
        [&]()
        {
            editor::DocumentState state = makeCachedOutputState(
                0.0,
                CachedVisual::editorBox,
                0.4,
                0.4,
                0.0);
            state.level.things[0].x = -0.0002;
            return state;
    }());
    view->setGridTransformOrigin(QVector3D{});
    view->setGridTransformPlane(GlView::GridPlane::YZ);
    const auto gridBefore = assets::saveToSedBytes(window.documentStateForTest().level);
    check(view->beginThingMoveDragForTest({ 0 }, false, false),
        "grid modifier cached drag starts without snapping");
    view->setTransformGridPlaneStickEnabled(true);
    view->updateThingMoveDragSampleForTest(4);
    check(view->transformPreviewActiveForTest(),
        "grid modifier cached drag remains active after lazy support capture");
    check(assets::saveToSedBytes(window.documentStateForTest().level) == gridBefore,
        "grid modifier cached preview preserves authored bytes");
    check(view->thingMoveBoundsResolveCountForTest() == 0
            && view->thingMoveResourceResolveCountForTest() == 0,
        "grid modifier activation does not resolve Thing snap resources");
    view->endTransformDragForTest();
    const Thing gridResult = window.documentStateForTest().level.things[0];
    logCachedThingPose("grid", gridResult);
    check(std::abs(gridResult.x - (-0.0001)) <= 2.0e-12
            && gridResult.y == 0.0
            && gridResult.z == 0.0
            && gridResult.yaw == 0.0
            && gridResult.pch == 0.0
            && gridResult.rol == 0.0,
        "grid modifier cached drag commits the fixed YZ-plane support contact");
    check(!view->transformPreviewActiveForTest(),
        "grid modifier cached drag clears the preview after commit");

    // A tool switch cancels a second active gesture. Capture the bytes and
    // authored pose before the gesture so this check cannot compare a value
    // captured after the preview was already discarded.
    prepareCachedOutputWindow(
        window,
        *view,
        {},
        [&]()
        {
            editor::DocumentState state = makeCachedOutputState(
                0.0,
                CachedVisual::editorBox,
                0.4,
                0.4,
                0.0);
            state.level.things[0].x = -0.0002;
            return state;
        }());
    view->setGridTransformPlane(GlView::GridPlane::YZ);
    const auto cancelBefore = assets::saveToSedBytes(window.documentStateForTest().level);
    const Thing cancelPoseBefore = window.documentStateForTest().level.things[0];
    check(view->beginThingMoveDragForTest({ 0 }, false, false),
        "grid modifier cancellation gesture starts");
    view->setTransformGridPlaneStickEnabled(true);
    view->updateThingMoveDragSampleForTest(4);
    view->setTransformTool(GlView::TransformTool::Select);
    check(!view->transformPreviewActiveForTest()
            && assets::saveToSedBytes(window.documentStateForTest().level) == cancelBefore
            && window.documentStateForTest().level.things[0].x == cancelPoseBefore.x
            && window.documentStateForTest().level.things[0].y == cancelPoseBefore.y
            && window.documentStateForTest().level.things[0].z == cancelPoseBefore.z,
        "grid modifier cancellation restores authored bytes and pose");

    // A new gesture after changing the authored bbox must resolve the changed
    // exact key rather than retaining the prior gesture's cached box. Start
    // from a fixed near candidate so the changed x1 produces a different
    // deterministic contact, rather than merely checking that a counter moved.
    prepareCachedOutputWindow(
        window,
        *view,
        {},
        makeCachedOutputState(
            0.0,
            CachedVisual::editorBox,
            0.025,
            0.060,
            0.0,
            true));
    const std::optional<Thing> firstFreshResult = commitCachedOutputGesture(
        window,
        *view,
        false,
        8,
        "initial cache invalidation drag");
    editor::DocumentState changed = window.documentStateForTest();
    changed.level.things[0].bbox.x1 = 0.5;
    window.setDocumentStateForTest(std::move(changed));
    window.activateThingEditModeForTest();
    view->setLevel(
        &window.mutableDocumentStateForTest().level,
        GlView::SetLevelMode::resetView);
    view->fitViewToLevel();
    const auto freshBeforeBytes = assets::saveToSedBytes(
        window.documentStateForTest().level);
    const std::size_t freshUndoBefore = window.undoDepthForTest();
    const std::optional<Thing> freshResult = commitCachedOutputGesture(
        window,
        *view,
        false,
        8,
        "fresh cached drag");
    const auto freshAfterBytes = assets::saveToSedBytes(
        window.documentStateForTest().level);
    if (firstFreshResult && freshResult)
    {
        logCachedThingPose("fresh-initial", *firstFreshResult);
        logCachedThingPose("fresh-changed", *freshResult);
        check(std::abs(firstFreshResult->x - 0.0248) <= 2.0e-12,
            "initial cache invalidation drag commits its fixed contact");
        check(std::abs(freshResult->x - 0.025) <= 2.0e-12,
            "fresh cached drag uses the changed exact bbox in its fixed contact");
        check(freshResult->x != firstFreshResult->x
                && view->thingMoveBoundsResolveCountForTest() >= 1,
            "fresh cached drag rebuilds the changed exact bbox identity");
        check(std::isfinite(freshResult->x)
                && std::isfinite(freshResult->y)
                && std::isfinite(freshResult->z)
                && freshResult->yaw == 0.0
                && freshResult->pch == 0.0
                && freshResult->rol == 0.0,
            "fresh cached drag commits a finite pose with authored rotations preserved");
        const bool undone = freshUndoBefore < window.undoDepthForTest()
            && window.undoForTest();
        check(undone
                && assets::saveToSedBytes(window.documentStateForTest().level)
                    == freshBeforeBytes,
            "fresh cached drag Undo restores the exact pre-gesture bytes");
        const bool redone = window.redoForTest();
        check(redone
                && assets::saveToSedBytes(window.documentStateForTest().level)
                    == freshAfterBytes,
            "fresh cached drag Redo restores the exact committed bytes");
    }
}

void runThingMoveConnectorBypassCase()
{
    QTemporaryDir resourceDirectory{};
    check(resourceDirectory.isValid(), "cached connector temp root exists");
    if (!resourceDirectory.isValid())
    {
        return;
    }
    const std::filesystem::path root(resourceDirectory.path().toStdWString());
    std::filesystem::create_directories(root / "3do");
    const QByteArray model = QByteArrayLiteral(
        "SECTION: HEADER\n"
        "3DO 2.3\n"
        "SECTION: GEOMETRYDEF\n"
        "RADIUS 0.125\n"
        "INSERT OFFSET 0 0 0\n"
        "GEOSETS 1\nGEOSET 0\nMESHES 1\nMESH 0\n"
        "NAME synthetic_rail_track\nVERTICES 8\n"
        "0: 0 -0.006 -0.00125\n1: 0 -0.006 0.00125\n"
        "2: 0 0.006 -0.00125\n3: 0 0.006 0.00125\n"
        "4: 0.125 -0.006 -0.00125\n5: 0.125 -0.006 0.00125\n"
        "6: 0.125 0.006 -0.00125\n7: 0.125 0.006 0.00125\n"
        "FACES 0\nSECTION: HIERARCHYDEF\nHIERARCHY NODES 1\n"
        "0: 0x0 0x1 0 -1 -1 -1 0 0 0 0 0 0 0 0 0 0 synthetic_rail_track\n");
    check(writeCachedOutputResource(root / "3do" / "scaled_connector_track.3do", model),
        "cached connector scaled model writes");
    const auto modelValue = [](const std::string_view value)
    {
        TplValue result{};
        result.name = "model3d";
        result.type = ValueType::str;
        result.stringValue = std::string(value);
        return result;
    };
    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.header.version = 3;
    state.level.layers = { "Layer0" };
    Thing moving{};
    moving.id = 1;
    moving.num = 0;
    moving.name = "+connector_move_fixture";
    moving.templateName = moving.name;
    moving.values.push_back(modelValue("scaled_connector_track.3do"));
    Thing anchor = moving;
    anchor.id = 2;
    anchor.num = 1;
    anchor.name = "+connector_anchor_fixture";
    anchor.templateName = anchor.name;
    // The near rail endpoint gap is within the zoom-dependent connector
    // capture reach, giving a deterministic accepted connector pose.
    anchor.x = 0.25;
    anchor.yaw = 10.0;
    state.level.things = { moving, anchor };
    state.selection.things.insert(0);
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(800, 600);
    window.setResourcePathsForTest({ root }, {});
    window.setDocumentStateForTest(std::move(state));
    window.activateThingEditModeForTest();
    window.mutableDocumentStateForTest().selection.things.insert(0);
    window.updateEditActionsForTest();
    window.show();
    QApplication::processEvents();
    GlView* const view = window.glViewForTest();
    check(view != nullptr, "connector bypass fixture exposes viewport");
    if (!view)
    {
        return;
    }
    view->resize(800, 600);
    view->setLevel(&window.mutableDocumentStateForTest().level,
        GlView::SetLevelMode::resetView);
    view->fitViewToLevel();
    assets::ResourceLocator resources{};
    resources.addRoot(root);
    const auto connectors = sed::thing::resolveRailTrackConnectors(
        resources, "scaled_connector_track.3do", "+connector_move_fixture");
    check(connectors.size() >= 2, "cached connector fixture resolves both rail connectors");
    const auto before = assets::saveToSedBytes(window.documentStateForTest().level);
    check(view->beginThingMoveDragForTest({ 0 }, false, true), "cached connector drag begins");
    view->setThingMoveSnapRotateEnabled(true);
    view->updateThingMoveDragSampleForTest(8);
    check(view->thingMoveSnapAnchorVisitCountForTest() > 0,
        "cached connector candidate is visited for exact scoring");
    view->endTransformDragForTest();
    const Thing result = window.documentStateForTest().level.things[0];
    logCachedThingPose("connector", result);
    check(std::abs(result.x - 0.126899030873474) <= 1.0e-6
            && std::abs(result.y + 0.021706022208366) <= 1.0e-6
            && std::abs(result.yaw - 10.0) <= 1.0e-6,
        "cached connector commits fixed rail pose and rotation");
    const auto committed = assets::saveToSedBytes(window.documentStateForTest().level);
    check(window.undoForTest()
            && assets::saveToSedBytes(window.documentStateForTest().level) == before
            && window.redoForTest()
            && assets::saveToSedBytes(window.documentStateForTest().level) == committed,
        "cached connector preserves complete bytes across Undo and Redo");
}

} // namespace

void runThingMoveDragHotPathTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(800, 600);
    window.setDocumentStateForTest(makeThingMoveHotPathState());
    window.activateThingEditModeForTest();
    window.show();
    QApplication::processEvents();

    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "Thing Move hot-path fixture exposes the real viewport");
    if (!view)
    {
        return;
    }
    view->resize(800, 600);
    view->setLevel(
        &window.mutableDocumentStateForTest().level,
        GlView::SetLevelMode::resetView);
    view->fitViewToLevel();
    QApplication::processEvents();

    editor::Selection selected{};
    for (std::size_t index = 0; index < kMovingThingCount; ++index)
    {
        selected.things.insert(static_cast<std::int32_t>(index));
    }
    view->setSelectionState(-1, -1, 0, -1, selected);
    view->setThingMoveSurfaceStickEnabled(true);
    view->setThingMoveSurfaceAlignEnabled(false);
    view->setThingMoveSnapToThingEnabled(true);
    view->setThingMoveSnapRotateEnabled(false);

    const std::vector<std::byte> before = assets::saveToSedBytes(
        window.documentStateForTest().level);
    const editor::Selection selectionBefore =
        window.documentStateForTest().selection;
    const std::uint64_t meshBefore = view->meshRebuildCountForTest();
    const std::uint64_t publicationBefore =
        view->thingTransformFastPublicationCountForTest();
    const std::uint64_t fallbackBefore =
        view->thingTransformFallbackRebuildCountForTest();
    const std::uint64_t geometryCacheBefore =
        view->geometryMoveSnapCacheBuildCountForTest();

    std::vector<std::int32_t> movingIndices{};
    movingIndices.reserve(kMovingThingCount);
    for (std::size_t index = 0; index < kMovingThingCount; ++index)
    {
        movingIndices.push_back(static_cast<std::int32_t>(index));
    }

    const auto gestureStart = std::chrono::steady_clock::now();
    const bool began = view->beginThingMoveDragForTest(
        movingIndices,
        true,
        true);
    const auto afterBegin = std::chrono::steady_clock::now();
    const auto beginElapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        afterBegin - gestureStart);
    check(began,
        "Thing Move hot-path fixture starts one multi-selection drag");
    if (!began)
    {
        return;
    }

    const auto start = afterBegin;
    for (int sample = 0; sample < kPointerSamples; ++sample)
    {
        // Every sample belongs to the same active drag. The old convenience
        // hook intentionally starts a new drag per call and is not used here.
        view->updateThingMoveDragSampleForTest(2 + sample);
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start);
    const auto totalElapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - gestureStart);

    const std::uint64_t samples = view->thingMoveDragSampleCountForTest();
    const std::uint64_t anchorVisits =
        view->thingMoveSnapAnchorVisitCountForTest();
    const std::uint64_t boundsResolves =
        view->thingMoveBoundsResolveCountForTest();
    const std::uint64_t resourceResolves =
        view->thingMoveResourceResolveCountForTest();
    const std::uint64_t surfaceQueries =
        view->thingMoveSurfacePickQueryCountForTest();
    std::cerr << "ThingMoveHotPath begin_us=" << beginElapsed.count()
              << " samples=" << samples
              << " elapsed_us=" << elapsed.count()
              << " total_us=" << totalElapsed.count()
              << " anchor_visits=" << anchorVisits
              << " bounds_resolves=" << boundsResolves
              << " resource_resolves=" << resourceResolves
              << " exhaustive_surface_queries=" << surfaceQueries << '\n';

    check(samples == static_cast<std::uint64_t>(kPointerSamples),
        "Thing Move hot-path fixture updates every sample without resetting the drag");
    check(view->transformPreviewActiveForTest(),
        "Thing Move hot-path fixture keeps the multi-selection preview active");
    check(assets::saveToSedBytes(window.documentStateForTest().level) == before,
        "Thing Move preview leaves the authored Level unchanged until release");
    check(sameThingSelection(
              window.documentStateForTest().selection,
              selectionBefore),
        "Thing Move preview leaves the authored Thing selection unchanged");
    check(view->meshRebuildCountForTest() == meshBefore,
        "Thing Move preview does not rebuild the complete mesh per pointer sample");
    check(view->thingTransformFastPublicationCountForTest() == publicationBefore,
        "Thing Move preview does not publish committed Thing transforms per pointer sample");
    check(view->thingTransformFallbackRebuildCountForTest() == fallbackBefore,
        "Thing Move preview does not fall back to a committed Thing rebuild");
    check(view->geometryMoveSnapCacheBuildCountForTest() == geometryCacheBefore,
        "Thing Move preview does not build a geometry snap cache for a Thing target");

    // These are gesture-level budgets. Resolution is lazy and deduplicated by
    // visual identity, so the first sample may resolve one entry per distinct
    // model/template while later samples reuse the immutable gesture cache.
    check(boundsResolves <= static_cast<std::uint64_t>(kTotalThingCount + 64u),
        "Thing Move bounds are resolved once per gesture rather than per anchor and sample");
    check(resourceResolves <= static_cast<std::uint64_t>(
              kTotalThingCount * 2u + kMovingThingCount + 64u),
        "Thing Move resource metadata is resolved once per visual identity and gesture");
    check(anchorVisits <= static_cast<std::uint64_t>(kTotalThingCount),
        "Thing Move broad-phase avoids revisiting far anchors for every selected Thing and sample");
    check(surfaceQueries == 0,
        "Thing Move Surface Stick uses the indexed dense-surface path without exhaustive level picking");

    // Switching tools is the test-only cancellation path: it clears the
    // transient preview and avoids committing a synthetic movement.
    view->setTransformTool(GlView::TransformTool::Select);
    check(!view->transformPreviewActiveForTest(),
        "Thing Move hot-path cancellation clears the transient preview");

    // A plain Move gesture must not pay the snapping resource cost merely
    // because the editor can enable snapping through a modifier later.
    const auto plainGestureStart = std::chrono::steady_clock::now();
    const bool plainBegan = view->beginThingMoveDragForTest(
        movingIndices,
        false,
        false);
    const auto plainAfterBegin = std::chrono::steady_clock::now();
    const auto plainBeginElapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        plainAfterBegin - plainGestureStart);
    check(plainBegan,
        "Thing Move hot-path fixture starts a plain non-snapping gesture");
    if (plainBegan)
    {
        view->updateThingMoveDragSampleForTest(3);
        const auto plainTotalElapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - plainGestureStart);
        std::cerr << "ThingMovePlain begin_us=" << plainBeginElapsed.count()
                  << " total_us=" << plainTotalElapsed.count()
                  << " bounds_resolves="
                  << view->thingMoveBoundsResolveCountForTest()
                  << " resource_resolves="
                  << view->thingMoveResourceResolveCountForTest() << '\n';
        check(view->thingMoveBoundsResolveCountForTest() == 0,
            "Plain Thing Move defers all bounds resolution while snapping is disabled");
        check(view->thingMoveResourceResolveCountForTest() == 0,
            "Plain Thing Move defers all resource resolution while snapping is disabled");
        view->setTransformTool(GlView::TransformTool::Select);
    }

    // Repeat the same production gesture with Rotate-to-snap enabled. The
    // ordinary box broad phase is rotation-invariant; this benchmark ensures
    // enabling the default option does not restore per-anchor resource work.
    const auto rotateGestureStart = std::chrono::steady_clock::now();
    const bool rotateBegan = view->beginThingMoveDragForTest(
        movingIndices,
        true,
        true);
    const auto rotateAfterBegin = std::chrono::steady_clock::now();
    const auto rotateBeginElapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        rotateAfterBegin - rotateGestureStart);
    check(rotateBegan,
        "Thing Move hot-path fixture starts the standard Rotate-to-snap gesture");
    if (rotateBegan)
    {
        view->setThingMoveSnapRotateEnabled(true);
        for (int sample = 0; sample < kPointerSamples; ++sample)
        {
            view->updateThingMoveDragSampleForTest(2 + sample);
        }
        const auto rotateTotalElapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - rotateGestureStart);
        std::cerr << "ThingMoveRotate begin_us=" << rotateBeginElapsed.count()
                  << " total_us=" << rotateTotalElapsed.count()
                  << " samples="
                  << view->thingMoveDragSampleCountForTest()
                  << " anchor_visits="
                  << view->thingMoveSnapAnchorVisitCountForTest()
                  << " bounds_resolves="
                  << view->thingMoveBoundsResolveCountForTest()
                  << " resource_resolves="
                  << view->thingMoveResourceResolveCountForTest()
                  << " exhaustive_surface_queries="
                  << view->thingMoveSurfacePickQueryCountForTest() << '\n';
        check(view->thingMoveDragSampleCountForTest()
                == static_cast<std::uint64_t>(kPointerSamples),
            "Rotate-to-snap cached gesture updates every sample");
        check(view->thingMoveBoundsResolveCountForTest()
                <= static_cast<std::uint64_t>(kTotalThingCount),
            "Rotate-to-snap resolves each exact bbox identity once");
        check(view->thingMoveResourceResolveCountForTest() <= 4,
            "Rotate-to-snap resolves each visual resource identity once");
        check(view->thingMoveSnapAnchorVisitCountForTest() == 0,
            "Rotate-to-snap culls all distant anchors before exact scoring");
        check(view->thingMoveSurfacePickQueryCountForTest() == 0,
            "Rotate-to-snap uses the indexed dense-surface path");
        view->setTransformTool(GlView::TransformTool::Select);
    }

    runThingMoveCachedOutputTests();
    runThingMoveConnectorBypassCase();
}

} // namespace sed::app_tests
