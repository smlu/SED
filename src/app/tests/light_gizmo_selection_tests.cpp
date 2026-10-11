#include "test_common.hpp"

#include "gl_view.hpp"
#include "render/gizmos/light_category_gizmo_utils.hpp"
#include "sed/geometry.hpp"
#include "sed/level.hpp"

#include <QApplication>
#include <QByteArray>
#include <QEventLoop>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QObject>
#include <QPointF>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sed::app_tests
{
namespace
{

float perspectiveWindowDepth(const double distance) noexcept
{
    constexpr double nearPlane = 0.1;
    constexpr double farPlane = 1000.0;
    return static_cast<float>(
        farPlane / (farPlane - nearPlane)
        - (farPlane * nearPlane) / ((farPlane - nearPlane) * distance));
}

void sendClick(
    GlView& view,
    const QPointF& point,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QMouseEvent press(
        QEvent::MouseButtonPress,
        point,
        point,
        Qt::LeftButton,
        Qt::LeftButton,
        modifiers);
    QApplication::sendEvent(&view, &press);

    QMouseEvent release(
        QEvent::MouseButtonRelease,
        point,
        point,
        Qt::LeftButton,
        Qt::NoButton,
        modifiers);
    QApplication::sendEvent(&view, &release);
    QApplication::processEvents();
}

void sendMarquee(
    GlView& view,
    const QPoint& start,
    const QPoint& finish,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    const QPointF startPosition(start);
    const QPointF finishPosition(finish);
    QMouseEvent press(
        QEvent::MouseButtonPress,
        startPosition,
        startPosition,
        Qt::LeftButton,
        Qt::LeftButton,
        modifiers);
    QApplication::sendEvent(&view, &press);

    QMouseEvent move(
        QEvent::MouseMove,
        finishPosition,
        finishPosition,
        Qt::NoButton,
        Qt::LeftButton,
        modifiers);
    QApplication::sendEvent(&view, &move);

    QMouseEvent release(
        QEvent::MouseButtonRelease,
        finishPosition,
        finishPosition,
        Qt::LeftButton,
        Qt::NoButton,
        modifiers);
    QApplication::sendEvent(&view, &release);
    QApplication::processEvents();
}

TplValue makeStringValue(const std::string& name, const std::string& value)
{
    TplValue result{};
    result.name = name;
    result.type = ValueType::str;
    result.stringValue = value;
    return result;
}

void appendQuadSurface(
    Sector& sector,
    const double z,
    const bool facesTopDownCamera,
    const bool doubleSided = false)
{
    const std::int32_t firstVertex =
        static_cast<std::int32_t>(sector.vertices.size());
    sector.vertices.push_back(Vertex{ -2.0, -2.0, z, 0 });
    sector.vertices.push_back(Vertex{  2.0, -2.0, z, 0 });
    sector.vertices.push_back(Vertex{  2.0,  2.0, z, 0 });
    sector.vertices.push_back(Vertex{ -2.0,  2.0, z, 0 });

    Surface surface{};
    surface.num = static_cast<std::int32_t>(sector.surfaces.size());
    surface.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    surface.faceFlags = doubleSided ? FF_DoubleSided : 0u;
    surface.normal = facesTopDownCamera
        ? TVector{ 0.0, 0.0, 1.0 }
        : TVector{ 0.0, 0.0, -1.0 };
    surface.vertices = facesTopDownCamera
        ? std::vector<std::int32_t>{
              firstVertex, firstVertex + 1, firstVertex + 2, firstVertex + 3 }
        : std::vector<std::int32_t>{
              firstVertex, firstVertex + 3, firstVertex + 2, firstVertex + 1 };
    surface.texVertices.resize(surface.vertices.size());
    sector.surfaces.push_back(std::move(surface));
}

struct LightFamilyFixture
{
    ProjectType project{};
    render::LightGizmoCategory category{};
    GlView::PickMode pickMode{};
    const char* name{};
    std::int32_t ownerIndex{};
};

struct ClickPickResult
{
    bool received{};
    std::int32_t sector{ -2 };
    std::int32_t thing{ -2 };
    std::int32_t light{ -2 };
};

bool clickSelectsFamilyOwner(
    const LightFamilyFixture& fixture,
    const ClickPickResult& result)
{
    if ( !result.received )
    {
        return false;
    }

    switch ( fixture.category )
    {
        case render::LightGizmoCategory::Baked:
            return result.light == fixture.ownerIndex;
        case render::LightGizmoCategory::Thing:
            return result.thing == fixture.ownerIndex;
        case render::LightGizmoCategory::Sector:
            return result.sector == fixture.ownerIndex;
        case render::LightGizmoCategory::Architectural:
            return false;
    }
    return false;
}

bool clickMissesFamilyOwner(
    const LightFamilyFixture& fixture,
    const ClickPickResult& result)
{
    if ( !result.received )
    {
        return false;
    }

    switch ( fixture.category )
    {
        case render::LightGizmoCategory::Baked:
            return result.light != fixture.ownerIndex;
        case render::LightGizmoCategory::Thing:
            return result.thing != fixture.ownerIndex;
        case render::LightGizmoCategory::Sector:
            return result.sector != fixture.ownerIndex;
        case render::LightGizmoCategory::Architectural:
            return false;
    }
    return false;
}

bool boxSelectsFamilyOwner(
    const LightFamilyFixture& fixture,
    const editor::Selection& selection)
{
    switch ( fixture.category )
    {
        case render::LightGizmoCategory::Baked:
            return selection.lights
                == std::unordered_set<std::int32_t>{ fixture.ownerIndex };
        case render::LightGizmoCategory::Thing:
            return selection.things
                == std::unordered_set<std::int32_t>{ fixture.ownerIndex };
        case render::LightGizmoCategory::Sector:
            return selection.sectors
                == std::unordered_set<std::int32_t>{ fixture.ownerIndex };
        case render::LightGizmoCategory::Architectural:
            return false;
    }
    return false;
}

bool selectionIsEmpty(const editor::Selection& selection)
{
    return selection.sectors.empty()
        && selection.surfaces.empty()
        && selection.things.empty()
        && selection.lights.empty()
        && selection.frames.empty();
}

void setFixtureOwnerLayer(
    Level& level,
    const LightFamilyFixture& fixture,
    const std::int32_t layer)
{
    switch ( fixture.category )
    {
        case render::LightGizmoCategory::Baked:
            level.lights[static_cast<std::size_t>(fixture.ownerIndex)].layer = layer;
            break;
        case render::LightGizmoCategory::Thing:
            level.things[static_cast<std::size_t>(fixture.ownerIndex)].layer = layer;
            break;
        case render::LightGizmoCategory::Sector:
            level.sectors[static_cast<std::size_t>(fixture.ownerIndex)].layer = layer;
            break;
        case render::LightGizmoCategory::Architectural:
            break;
    }
}

void setSelectedFamilyOwner(
    GlView& view,
    const LightFamilyFixture& fixture,
    const std::int32_t owner)
{
    view.setSelection(
        fixture.category == render::LightGizmoCategory::Sector ? owner : -1,
        -1,
        fixture.category == render::LightGizmoCategory::Thing ? owner : -1,
        fixture.category == render::LightGizmoCategory::Baked ? owner : -1);
}

std::vector<render::LightGizmoEntry> familyEntries(
    const Level& level,
    const render::LightGizmoCategory category)
{
    switch ( category )
    {
        case render::LightGizmoCategory::Baked:
            return render::bakedLightGizmoEntries(level);
        case render::LightGizmoCategory::Thing:
            return render::thingLightGizmoEntries(level, nullptr);
        case render::LightGizmoCategory::Sector:
            return render::sectorLightGizmoEntries(level);
        case render::LightGizmoCategory::Architectural:
            break;
    }
    return {};
}

Level makeLightFamilyLevel(const LightFamilyFixture& fixture)
{
    Level level{};
    level.kind = fixture.project;
    level.layers.emplace_back("Visible");
    level.layers.emplace_back("Hidden");

    Sector& wallSector = level.sectors.emplace_back();
    wallSector.layer = 0;
    appendQuadSurface(wallSector, 0.0, false);
    // This visible surface is behind the light source. It keeps the actual
    // emitted Level bounds wide enough for orthographic auto-clipping while
    // leaving the source in front of the sampled depth.
    appendQuadSurface(wallSector, -3.0, true);

    switch ( fixture.category )
    {
        case render::LightGizmoCategory::Baked:
        {
            SedLight& light = level.lights.emplace_back();
            light.layer = 0;
            light.position = TVector{ 0.0, 0.0, -2.0 };
            light.range = 4.0;
            break;
        }
        case render::LightGizmoCategory::Thing:
        {
            Thing& thing = level.things.emplace_back();
            thing.num = 0;
            thing.sector = 0;
            thing.layer = 0;
            thing.x = 0.0;
            thing.y = 0.0;
            thing.z = -2.0;
            thing.values.push_back(makeStringValue(
                "light",
                fixture.project == ProjectType::IJIM ? "1/1/1/4" : "0.5"));
            break;
        }
        case render::LightGizmoCategory::Sector:
        {
            Sector& ownerSector = level.sectors.emplace_back();
            ownerSector.layer = 0;
            ownerSector.pointLight.color = ColorF{ 1.0f, 1.0f, 1.0f, 1.0f };
            ownerSector.pointLight.position = TVector{ 0.0, 0.0, -2.0 };
            ownerSector.pointLight.minRange = 4.0;
            ownerSector.pointLight.maxRange = 8.0;
            break;
        }
        case render::LightGizmoCategory::Architectural:
            break;
    }
    return level;
}

void setFrontWallState(
    Level& level,
    const bool frontFacing,
    const bool doubleSided)
{
    Surface& wall = level.sectors.front().surfaces.front();
    wall.normal = frontFacing
        ? TVector{ 0.0, 0.0, 1.0 }
        : TVector{ 0.0, 0.0, -1.0 };
    wall.vertices = frontFacing
        ? std::vector<std::int32_t>{ 0, 1, 2, 3 }
        : std::vector<std::int32_t>{ 0, 3, 2, 1 };
    wall.faceFlags = doubleSided ? FF_DoubleSided : 0u;
}

void runLightFamilyMatrixTests()
{
    using ProjectionType = app::ViewportCamera::ProjectionType;
    const QByteArray expectedDprText = qgetenv("SED_TEST_EXPECTED_DPR");

    const std::array<LightFamilyFixture, 5> fixtures{
        LightFamilyFixture{
            ProjectType::JKDF2,
            render::LightGizmoCategory::Baked,
            GlView::PickMode::Light,
            "JKDF2 Baked",
            0 },
        LightFamilyFixture{
            ProjectType::MOTS,
            render::LightGizmoCategory::Baked,
            GlView::PickMode::Light,
            "MotS Baked",
            0 },
        LightFamilyFixture{
            ProjectType::JKDF2,
            render::LightGizmoCategory::Thing,
            GlView::PickMode::Thing,
            "JKDF2 Thing",
            0 },
        LightFamilyFixture{
            ProjectType::MOTS,
            render::LightGizmoCategory::Thing,
            GlView::PickMode::Thing,
            "MotS Thing",
            0 },
        LightFamilyFixture{
            ProjectType::IJIM,
            render::LightGizmoCategory::Sector,
            GlView::PickMode::Sector,
            "IJIM Sector",
            1 },
    };
    struct ProjectionFixture
    {
        ProjectionType type{};
        const char* name{};
        bool distant{};
    };
    const std::array<ProjectionFixture, 2> projections{
        ProjectionFixture{ ProjectionType::Orthographic, "orthographic", false },
        ProjectionFixture{ ProjectionType::Perspective, "distant perspective", true },
    };

    const auto runViewPass = [](
                                GlView& view,
                                Level& level,
                                const LightFamilyFixture& fixture,
                                const QPointF& markerPoint,
                                const bool checkPixelBoundary,
                                const std::string& label,
                                const char* path)
    {
        ClickPickResult clickResult{};
        const QMetaObject::Connection clickConnection = QObject::connect(
            &view,
            &GlView::clickPicked,
            [&clickResult](
                const std::int32_t sector,
                const std::int32_t,
                const std::int32_t thing,
                const std::int32_t light)
            {
                clickResult = ClickPickResult{ true, sector, thing, light };
            });
        std::optional<editor::Selection> boxSelection{};
        std::optional<GlView::SelectionBoxMode> boxMode{};
        const QMetaObject::Connection boxConnection = QObject::connect(
            &view,
            &GlView::selectionBoxPicked,
            [&boxSelection, &boxMode](
                editor::Selection selection,
                const GlView::SelectionBoxMode mode)
            {
                boxSelection = std::move(selection);
                boxMode = mode;
            });

        const QPoint markerPixel(
            qRound(markerPoint.x()),
            qRound(markerPoint.y()));
        const auto reportClick = [&](const bool shouldSelect, const char* scenario)
        {
            clickResult = {};
            sendClick(view, markerPoint);
            const bool passed = shouldSelect
                ? clickSelectsFamilyOwner(fixture, clickResult)
                : clickMissesFamilyOwner(fixture, clickResult);
            check(passed,
                (label + " " + path + " " + scenario + " direct light pick").c_str());
        };
        const auto reportBox = [
                                 &view,
                                 &fixture,
                                 &markerPixel,
                                 &boxSelection,
                                 &boxMode,
                                 &label,
                                 path](
                                    const bool shouldSelect,
                                    const GlView::SelectionBoxMode expectedMode,
                                    const Qt::KeyboardModifiers modifiers,
                                    const char* scenario)
        {
            boxSelection.reset();
            boxMode.reset();
            sendMarquee(
                view,
                markerPixel - QPoint(12, 12),
                markerPixel + QPoint(12, 12),
                modifiers);
            const bool selectionPassed = shouldSelect
                ? boxSelection
                    && boxSelectsFamilyOwner(fixture, *boxSelection)
                : boxSelection && selectionIsEmpty(*boxSelection);
            check(selectionPassed
                    && boxMode == expectedMode,
                (label + " " + path + " " + scenario + " light marquee selection").c_str());
        };
        const auto publishWallState = [
                                         &view,
                                         &level](
                                            const bool frontFacing,
                                            const bool doubleSided)
        {
            setFrontWallState(level, frontFacing, doubleSided);
            view.notifyLevelGeometryChanged();
            view.repaint();
            QApplication::processEvents();
        };

        view.setHiddenLayers({});
        setFixtureOwnerLayer(level, fixture, 0);
        view.setXraySelectionCyclingEnabled(false);
        publishWallState(false, false);
        reportClick(true, "single-sided backface allows");
        reportBox(
            true,
            GlView::SelectionBoxMode::Replace,
            Qt::NoModifier,
            "single-sided backface allows");

        if ( checkPixelBoundary )
        {
            view.setLightGizmoWorldSize(0.001f);
            clickResult = {};
            sendClick(view, markerPoint + QPointF(14.0, 0.0));
            check(clickSelectsFamilyOwner(fixture, clickResult),
                (label + " " + path + " accepts a click within the logical 15-pixel light hit margin").c_str());
            clickResult = {};
            sendClick(view, markerPoint + QPointF(16.0, 0.0));
            check(clickMissesFamilyOwner(fixture, clickResult),
                (label + " " + path + " rejects clicks beyond the logical 15-pixel light hit margin").c_str());
        }

        publishWallState(true, false);
        reportClick(false, "opaque front wall blocks");
        reportBox(
            false,
            GlView::SelectionBoxMode::Replace,
            Qt::NoModifier,
            "opaque front wall blocks");

        publishWallState(false, true);
        reportClick(false, "double-sided backface blocks");
        reportBox(
            false,
            GlView::SelectionBoxMode::Replace,
            Qt::NoModifier,
            "double-sided backface blocks");

        publishWallState(true, false);
        view.setXraySelectionCyclingEnabled(true);
        setSelectedFamilyOwner(view, fixture, fixture.ownerIndex);
        reportClick(true, "X-Ray bypasses opaque wall");
        view.setTransformTool(GlView::TransformTool::Move);
        reportBox(
            true,
            GlView::SelectionBoxMode::Replace,
            Qt::AltModifier,
            "Alt-transform X-Ray bypasses opaque wall");
        view.setXraySelectionCyclingEnabled(false);
        view.setTransformTool(GlView::TransformTool::Select);

        publishWallState(false, false);
        setFixtureOwnerLayer(level, fixture, 1);
        view.setHiddenLayers({ 1 });
        reportClick(false, "hidden owner layer rejects");
        reportBox(
            false,
            GlView::SelectionBoxMode::Replace,
            Qt::NoModifier,
            "hidden owner layer rejects");
        setFixtureOwnerLayer(level, fixture, 0);
        view.setHiddenLayers({});

        view.setTransformTool(GlView::TransformTool::Move);
        reportBox(
            true,
            GlView::SelectionBoxMode::Replace,
            Qt::AltModifier,
            "Alt-transform Replace");
        check(view.transformTool() == GlView::TransformTool::Move,
            (label + " " + path + " keeps Move active after Alt-transform Replace").c_str());
        reportBox(
            true,
            GlView::SelectionBoxMode::Add,
            Qt::AltModifier | Qt::ShiftModifier,
            "Alt-transform Shift Add");
        reportBox(
            true,
            GlView::SelectionBoxMode::Remove,
            Qt::AltModifier | Qt::ControlModifier,
            "Alt-transform Control Remove");
        view.setTransformTool(GlView::TransformTool::Select);
        QObject::disconnect(clickConnection);
        QObject::disconnect(boxConnection);
    };

    for ( const LightFamilyFixture& fixture : fixtures )
    {
        for ( const ProjectionFixture& projection : projections )
        {
            Level level = makeLightFamilyLevel(fixture);
            GlView view{};
            view.setAttribute(Qt::WA_DontShowOnScreen);
            view.resize(480, 360);
            view.setShowGrid(false);
            view.setShowWireframe(false);
            view.setShowVertices(false);
            view.setShowSolid(true);
            view.setShowTextures(false);
            view.setShowThings(fixture.category == render::LightGizmoCategory::Thing);
            view.setShowThingGizmos(false);
            view.setShowAxisWidget(false);
            view.setShowToastOverlays(false);
            view.setThingViewMode(GlView::ThingViewMode::Boxes);
            view.setThingCubeHalfSize(0.05f);
            view.setLightGizmoWorldSize(0.001f);
            view.setTransformTool(GlView::TransformTool::Select);
            view.setPickMode(fixture.pickMode);
            view.setLightGizmoCategory(fixture.category);
            view.setSurfaceClickSelectionEnabled(true);
            view.setCameraClipEnabled(true);
            view.setLevel(&level);
            view.resetEditorTopDownView();
            view.setCameraProjectionType(projection.type);
            if ( projection.distant )
            {
                view.setCameraNearFar(0.1f, 1000.0f);
                view.setCameraOrbitDistance(100.0f);
            }

            const std::string label = std::string(fixture.name)
                + " " + projection.name;
            const bool cpuPath = !view.mainShaderReadyForTest();
            check(cpuPath, (label + " starts on CPU depth fallback").c_str());
            const std::vector<render::LightGizmoEntry> entries =
                familyEntries(level, fixture.category);
            const auto entry = std::find_if(
                entries.begin(),
                entries.end(),
                [&fixture](const render::LightGizmoEntry& candidate)
                {
                    return candidate.ownerIndex == fixture.ownerIndex;
                });
            const bool entryAvailable = entry != entries.end();
            check(entryAvailable, (label + " publishes its source in the real family entry list").c_str());
            if ( !entryAvailable )
            {
                continue;
            }

            const QVector3D displayPosition = view.thingLightGizmoDisplayPositionForTest(
                *entry,
                view.cameraUp(),
                0.0005f);
            const std::optional<QPointF> markerPoint =
                view.projectWorldToViewport(displayPosition);
            const bool markerInside = markerPoint
                && markerPoint->x() >= 0.0
                && markerPoint->x() < static_cast<qreal>(view.width())
                && markerPoint->y() >= 0.0
                && markerPoint->y() < static_cast<qreal>(view.height());
            check(markerInside, (label + " projects the family icon inside real geometry bounds").c_str());
            if ( !markerInside )
            {
                continue;
            }

            const std::string cpuLabel = label + " CPU";
            runViewPass(
                view,
                level,
                fixture,
                *markerPoint,
                fixture.project == ProjectType::JKDF2
                    && fixture.category == render::LightGizmoCategory::Baked
                    && projection.type == ProjectionType::Orthographic,
                cpuLabel,
                "uninitialized");

            view.show();
            QApplication::processEvents(QEventLoop::AllEvents);
            view.repaint();
            QApplication::processEvents(QEventLoop::AllEvents);
            if ( !expectedDprText.isEmpty() )
            {
                bool validExpectedDpr = false;
                const double expectedDpr = expectedDprText.toDouble(&validExpectedDpr);
                const double afterShowDpr = view.devicePixelRatioF();
                const bool afterShowDprMatches = validExpectedDpr
                    && std::abs(afterShowDpr - expectedDpr) < 0.01;
                if ( !afterShowDprMatches )
                {
                    std::fprintf(
                        stderr,
                        "light-gizmo DPR mismatch stage=after-show fixture=%s projection=%s expected=%s observed=%.6g\n",
                        fixture.name,
                        projection.name,
                        expectedDprText.constData(),
                        afterShowDpr);
                }
                check(afterShowDprMatches,
                    (label + " GPU viewport uses the requested isolated-test device-pixel ratio").c_str());
            }
            check(view.mainShaderReadyForTest(), (label + " initializes the GPU depth path after show").c_str());
            if ( view.mainShaderReadyForTest() )
            {
                const std::string gpuLabel = label + " GPU";
                runViewPass(
                    view,
                    level,
                    fixture,
                    *markerPoint,
                    fixture.project == ProjectType::JKDF2
                        && fixture.category == render::LightGizmoCategory::Baked
                        && projection.type == ProjectionType::Orthographic,
                    gpuLabel,
                    "initialized");
            }
            view.hide();
        }
    }
}

} // namespace

void runLightGizmoSelectionTests()
{
    using render::LightGizmoCategory;
    using render::LightGizmoEntry;

    QMatrix4x4 perspectiveProjection{};
    perspectiveProjection.perspective(60.0f, 1.0f, 0.1f, 1000.0f);
    const std::vector<LightGizmoEntry> farPerspectiveLight{
        LightGizmoEntry{
            LightGizmoCategory::Baked,
            render::LightGizmoKind::Baked,
            0,
            QVector3D(0.0f, 0.0f, -100.0f) } };
    const float wallAt50 = perspectiveWindowDepth(50.0);
    const std::vector<render::LightGizmoScreenPick> farWallPicks =
        render::pickLightGizmoEntries(
            farPerspectiveLight,
            perspectiveProjection,
            480,
            480,
            QPointF(240.0, 240.0),
            15.0f,
            wallAt50);
    check(perspectiveWindowDepth(100.0) - wallAt50 > 0.001f
            && farWallPicks.empty(),
        "Baked-light direct picking rejects a source 50 world units behind a wall at far perspective depth");

    const std::vector<LightGizmoEntry> boundaryLight{
        LightGizmoEntry{
            LightGizmoCategory::Baked,
            render::LightGizmoKind::Baked,
            0,
            QVector3D{} } };
    const std::vector<render::LightGizmoScreenPick> atFifteenLogicalPixels =
        render::pickLightGizmoEntries(
            boundaryLight,
            QMatrix4x4{},
            480,
            480,
            QPointF(255.0, 240.0),
            15.0f);
    const std::vector<render::LightGizmoScreenPick> pastFifteenLogicalPixels =
        render::pickLightGizmoEntries(
            boundaryLight,
            QMatrix4x4{},
            480,
            480,
            QPointF(255.25, 240.0),
            15.0f);
    check(atFifteenLogicalPixels.size() == 1u
            && pastFifteenLogicalPixels.empty(),
        "Light-gizmo direct picking keeps the exact logical 15-pixel radius boundary");

    Level level{};
    level.kind = ProjectType::IJIM;
    level.layers.emplace_back("Visible");
    Sector& wallSector = level.sectors.emplace_back();
    wallSector.layer = 0;
    for ( const QVector3D& position : {
             QVector3D(-2.0f, -2.0f, 0.0f),
             QVector3D(2.0f, -2.0f, 0.0f),
             QVector3D(2.0f, 2.0f, 0.0f),
             QVector3D(-2.0f, 2.0f, 0.0f) })
    {
        wallSector.vertices.push_back(Vertex{
            static_cast<double>(position.x()),
            static_cast<double>(position.y()),
            static_cast<double>(position.z()),
            0 });
    }
    Surface backFacingWall{};
    backFacingWall.num = 0;
    backFacingWall.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    backFacingWall.normal = TVector{ 0.0, 0.0, -1.0 };
    // From the top-down camera this winding faces away from the view. The
    // light is beyond this lone wall and outside the Sector volume.
    backFacingWall.vertices = { 0, 3, 2, 1 };
    backFacingWall.texVertices.resize(backFacingWall.vertices.size());
    wallSector.surfaces.push_back(std::move(backFacingWall));

    SedLight light{};
    light.layer = 0;
    light.position = TVector{ 0.0, 0.0, -2.0 };
    level.lights.push_back(light);

    GlView view{};
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.resize(480, 360);
    view.setShowGrid(false);
    view.setShowWireframe(false);
    view.setShowVertices(false);
    view.setShowSolid(true);
    view.setShowTextures(false);
    view.setShowThings(false);
    view.setShowThingGizmos(false);
    view.setShowAxisWidget(false);
    view.setShowToastOverlays(false);
    view.setTransformTool(GlView::TransformTool::Select);
    view.setPickMode(GlView::PickMode::Light);
    view.setLightGizmoCategory(LightGizmoCategory::Baked);
    view.setSurfaceClickSelectionEnabled(true);
    view.setCameraClipEnabled(false);
    view.setLevel(&level);
    view.resetEditorTopDownView();
    view.setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    check(!view.mainShaderReadyForTest(),
        "light-gizmo synthetic viewport begins on the CPU depth fallback before OpenGL initialization");

    const QByteArray expectedDprText = qgetenv("SED_TEST_EXPECTED_DPR");
    if ( !expectedDprText.isEmpty() )
    {
        bool validExpectedDpr = false;
        const double expectedDpr = expectedDprText.toDouble(&validExpectedDpr);
        const double beforeShowDpr = view.devicePixelRatioF();
        const bool beforeShowDprMatches = validExpectedDpr
            && std::abs(beforeShowDpr - expectedDpr) < 0.01;
        if ( !beforeShowDprMatches )
        {
            std::fprintf(
                stderr,
                "light-gizmo DPR mismatch stage=before-show expected=%s observed=%.6g\n",
                expectedDprText.constData(),
                beforeShowDpr);
        }
        check(beforeShowDprMatches,
            "light-gizmo viewport uses the requested isolated-test device-pixel ratio");
    }

    const std::vector<LightGizmoEntry> entries =
        render::bakedLightGizmoEntries(level);
    const std::optional<LightGizmoEntry> bakedLight = entries.empty()
        ? std::nullopt
        : std::optional<LightGizmoEntry>{ entries.front() };
    const float baseHalfSize =
        std::clamp(view.lightGizmoWorldSize(), 0.001f, 1.0f) * 0.5f;
    const QVector3D markerWorldPosition = bakedLight
        ? view.thingLightGizmoDisplayPositionForTest(
              *bakedLight,
              view.cameraUp(),
              baseHalfSize)
        : QVector3D{};
    const std::optional<QPointF> screenPosition = bakedLight
        ? view.projectWorldToViewport(markerWorldPosition)
        : std::nullopt;
    const bool screenPositionInside = screenPosition
        && screenPosition->x() >= 0.0
        && screenPosition->x() < static_cast<qreal>(view.width())
        && screenPosition->y() >= 0.0
        && screenPosition->y() < static_cast<qreal>(view.height());
    check(screenPositionInside,
        "light-gizmo backface fixture projects its baked-light icon into the viewport");
    if ( !screenPositionInside )
    {
        const QVector3D eye = view.cameraEye();
        const QVector3D target = view.cameraTarget();
        const QVector3D forward = view.cameraForward();
        const std::optional<QPointF> projectedOrigin =
            view.projectWorldToViewport(QVector3D{});
        std::fprintf(stderr,
            "light-gizmo fixture projection failure marker=(%.9g,%.9g,%.9g) eye=(%.9g,%.9g,%.9g) target=(%.9g,%.9g,%.9g) forward=(%.9g,%.9g,%.9g) clip=(%.9g,%.9g) size=%dx%d projectedOrigin=%d(%.9g,%.9g)\n",
            markerWorldPosition.x(),
            markerWorldPosition.y(),
            markerWorldPosition.z(),
            eye.x(),
            eye.y(),
            eye.z(),
            target.x(),
            target.y(),
            target.z(),
            forward.x(),
            forward.y(),
            forward.z(),
            view.cameraNearPlane(),
            view.cameraFarPlane(),
            view.width(),
            view.height(),
            projectedOrigin.has_value() ? 1 : 0,
            projectedOrigin ? projectedOrigin->x() : -1.0,
            projectedOrigin ? projectedOrigin->y() : -1.0);
        return;
    }

    const QPoint iconPoint(
        qRound(screenPosition->x()),
        qRound(screenPosition->y()));
    std::int32_t pickedLight = -2;
    const QMetaObject::Connection clickConnection = QObject::connect(
        &view,
        &GlView::clickPicked,
        [&pickedLight](
            const std::int32_t,
            const std::int32_t,
            const std::int32_t,
            const std::int32_t lightIndex)
        {
            pickedLight = lightIndex;
        });
    sendClick(view, iconPoint);
    check(pickedLight == 0,
        "Baked-light click selection ignores an unseen single-sided Level backface and reaches an outside-Sector light");

    std::optional<editor::Selection> boxSelection{};
    std::optional<GlView::SelectionBoxMode> boxMode{};
    const QMetaObject::Connection boxConnection = QObject::connect(
        &view,
        &GlView::selectionBoxPicked,
        [&boxSelection, &boxMode](
            editor::Selection selection,
            const GlView::SelectionBoxMode mode)
        {
            boxSelection = std::move(selection);
            boxMode = mode;
        });
    sendMarquee(
        view,
        iconPoint - QPoint(12, 12),
        iconPoint + QPoint(12, 12));
    check(boxSelection
            && boxSelection->lights == std::unordered_set<std::int32_t>{ 0 }
            && boxMode == GlView::SelectionBoxMode::Replace,
        "Baked-light marquee selection ignores the same unseen single-sided Level backface");
    QObject::disconnect(clickConnection);
    QObject::disconnect(boxConnection);
    runLightFamilyMatrixTests();
}

} // namespace sed::app_tests
