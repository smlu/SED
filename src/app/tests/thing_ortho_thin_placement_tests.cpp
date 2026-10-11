#include "test_common.hpp"

#include <QApplication>

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/assets/sed.hpp"
#include "sed/level/build.hpp"
#include "sed/level/geometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace sed::app_tests
{

void runThingOrthoThinPlacementTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(640, 480);

    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.layers = { "Layer0" };
    level::levelCreateCube(state.level, 0.0, 0.0, 0.0, TVector::up, TVector::right);
    const Level thickLevel = state.level;
    for (Vertex& vertex : state.level.sectors[0].vertices)
    {
        vertex.x *= 0.001;
    }
    for (Surface& surface : state.level.sectors[0].surfaces)
    {
        level::surfaceRecalcNewAll(
            level::levelPixelPerUnit(state.level),
            state.level.sectors[0],
            surface,
            0);
    }
    window.setDocumentStateForTest(std::move(state));
    window.show();
    QApplication::processEvents();

    GlView* const view = window.glViewForTest();
    check(view != nullptr && window.documentStateForTest().level.sectors.size() == 1u,
        "thin orthographic placement fixture publishes one closed IJIM sector");
    if (view == nullptr)
    {
        return;
    }

    view->setLevel(
        &window.mutableDocumentStateForTest().level,
        GlView::SetLevelMode::resetView);
    view->fitViewToLevel();
    view->setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view->setOrbitViewImmediate(32.0f, 42.0f, 0.0f);
    const std::optional<QPointF> projectedCenter = view->projectWorldToViewport(
        QVector3D(0.0005f, 0.5f, 0.5f));
    check(projectedCenter.has_value(),
        "thin orthographic placement projects the fixture center into the viewport");
    if (!projectedCenter)
    {
        return;
    }
    const QPoint centerPoint(
        qRound(projectedCenter->x()),
        qRound(projectedCenter->y()));

    const TVector rayOrigin{
        static_cast<double>(view->cameraEye().x()),
        static_cast<double>(view->cameraEye().y()),
        static_cast<double>(view->cameraEye().z())};
    const QVector3D cameraDirection = view->cameraForward();
    const TVector rayDirection{
        static_cast<double>(cameraDirection.x()),
        static_cast<double>(cameraDirection.y()),
        static_cast<double>(cameraDirection.z())};
    const std::vector<level::SurfacePickHit> thinHits = view->placementSurfaceHits(
        rayOrigin,
        rayDirection,
        static_cast<double>(view->cameraFarPlane()));
    const std::size_t thinSideHits = static_cast<std::size_t>(std::count_if(
        thinHits.cbegin(),
        thinHits.cend(),
        [](const level::SurfacePickHit& hit)
        {
            return hit.surface.sector == 0 && hit.surface.surface != 0;
        }));
    const bool thinFloorHit = std::any_of(
        thinHits.cbegin(),
        thinHits.cend(),
        [](const level::SurfacePickHit& hit)
        {
            return hit.surface.sector == 0 && hit.surface.surface == 0;
        });
    check(thinSideHits >= 2u && !thinFloorHit
            && thinHits.size() >= 2u
            && thinHits[1].distance - thinHits[0].distance < 0.02,
        "thin angled orthographic ray crosses both side walls, misses the floor, and has sub-0.02 width");

    const std::vector<std::byte> levelBefore = assets::saveToSedBytes(
        window.documentStateForTest().level);
    const std::size_t undoBefore = window.undoDepthForTest();

    bool haveSurface = false;
    SurfaceRef surface{};
    QVector3D surfaceNormal{};
    TVector documentPoint{};
    const std::optional<QVector3D> point = view->placementPointForViewport(
        centerPoint,
        &haveSurface,
        &surface,
        &surfaceNormal,
        true,
        false,
        &documentPoint);

    check(point.has_value() && haveSurface,
        "thin angled orthographic placement finds a surface after fixed samples overshoot");
    check(surface.sector == 0 && surface.surface == 0,
        "thin angled orthographic placement selects the authored floor surface");
    check(documentPoint.z == 0.0,
        "thin angled orthographic placement returns the exact floor height");
    check(surfaceNormal.x() == 0.0f && surfaceNormal.y() == 0.0f
            && surfaceNormal.z() == 1.0f,
        "thin angled orthographic placement returns the exact upward floor normal");
    check(point && std::abs(point->z()) < 1.0e-6f,
        "thin angled orthographic placement returns a floor-position QVector3D");
    check(point && std::isfinite(point->x()) && std::isfinite(point->y())
            && std::isfinite(point->z()),
        "thin angled orthographic placement remains finite");

    // With clipping disabled the ortho projection spans ±100,000 view units,
    // even though the persisted plane controls retain their authored values.
    // Placement rays begin at the projected near plane and must traverse the
    // entire interval to reach this same visible floor.
    view->setCameraClipEnabled(false);
    bool unclippedHaveSurface = false;
    SurfaceRef unclippedSurface{};
    QVector3D unclippedNormal{};
    TVector unclippedDocumentPoint{};
    const std::optional<QVector3D> unclippedPoint = view->placementPointForViewport(
        centerPoint,
        &unclippedHaveSurface,
        &unclippedSurface,
        &unclippedNormal,
        true,
        false,
        &unclippedDocumentPoint);
    check(unclippedPoint && unclippedHaveSurface
            && unclippedSurface.sector == 0 && unclippedSurface.surface == 0,
        "unclipped orthographic placement traverses the full near-to-far range and selects the floor");
    check(unclippedPoint && unclippedDocumentPoint.z == 0.0
            && unclippedNormal.x() == 0.0f && unclippedNormal.y() == 0.0f
            && unclippedNormal.z() == 1.0f,
        "unclipped orthographic placement preserves the floor point and normal");
    view->setCameraClipEnabled(true);

    const Level& level = window.documentStateForTest().level;
    check(level.sectors.size() == 1u
            && assets::saveToSedBytes(level) == levelBefore
            && window.undoDepthForTest() == undoBefore,
        "thin orthographic placement leaves level data and undo history unchanged");

    Level tallThinLevel = thickLevel;
    for (Vertex& vertex : tallThinLevel.sectors[0].vertices)
    {
        vertex.x *= 0.001;
        vertex.z *= 100.0;
    }
    for (Surface& surface : tallThinLevel.sectors[0].surfaces)
    {
        level::surfaceRecalcNewAll(
            level::levelPixelPerUnit(tallThinLevel),
            tallThinLevel.sectors[0],
            surface,
            0);
    }
    window.mutableDocumentStateForTest().level = std::move(tallThinLevel);
    view->setLevel(
        &window.mutableDocumentStateForTest().level,
        GlView::SetLevelMode::resetView);
    view->fitViewToLevel();
    view->setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view->setOrbitViewImmediate(32.0f, 10.0f, 0.0f);
    view->setCameraOrbitDistance(10.0f);
    view->setShowGrid(false);
    // Keep the target point visible in a short authored range while the
    // floor is substantially farther away along the vertical sector query.
    view->setCameraNearFar(-0.5f, 20.5f);
    const std::optional<QPointF> tallCenter = view->projectWorldToViewport(
        QVector3D(0.0005f, 0.5f, 50.0f));
    bool tallHaveSurface = false;
    SurfaceRef tallSurface{};
    QVector3D tallNormal{};
    TVector tallPoint{};
    const std::optional<QVector3D> tallPlacement = tallCenter
        ? view->placementPointForViewport(
            QPoint(qRound(tallCenter->x()), qRound(tallCenter->y())),
            &tallHaveSurface,
            &tallSurface,
            &tallNormal,
            true,
            false,
            &tallPoint)
        : std::nullopt;
    bool foundShortSameSectorInterval = false;
    double shortIntervalFloorDistance = 0.0;
    double shortIntervalGap = 0.0;
    if (tallCenter)
    {
        const QVector3D eye = view->cameraEye();
        const QVector3D forward = view->cameraForward();
        const TVector rayOrigin{eye.x(), eye.y(), eye.z()};
        const TVector rayDirection{forward.x(), forward.y(), forward.z()};
        const std::vector<level::SurfacePickHit> hits = view->placementSurfaceHits(
            rayOrigin, rayDirection, 1000.0);
        for (std::size_t i = 0; i + 1u < hits.size(); ++i)
        {
            if (hits[i].surface.sector != 0
                || hits[i + 1u].surface.sector != 0)
            {
                continue;
            }
            const double gap = hits[i + 1u].distance - hits[i].distance;
            if (!std::isfinite(gap) || gap <= 0.0 || gap >= 0.04)
            {
                continue;
            }
            const TVector midpoint = hits[i].position + rayDirection.Scaled(gap * 0.5);
            const Sector& sector = window.documentStateForTest().level.sectors[0];
            double floorZ = std::numeric_limits<double>::infinity();
            for (const Vertex& vertex : sector.vertices)
            {
                floorZ = std::min(floorZ, vertex.z);
            }
            shortIntervalGap = gap;
            shortIntervalFloorDistance = midpoint.z - floorZ;
            foundShortSameSectorInterval = std::isfinite(floorZ)
                && std::isfinite(shortIntervalFloorDistance);
            break;
        }
    }
    check(tallCenter && foundShortSameSectorInterval
            && shortIntervalGap > 0.0 && shortIntervalGap < 0.04,
        "tall thin-sector fixture contains a short same-sector side-hit interval");
    check(foundShortSameSectorInterval
            && shortIntervalFloorDistance
                > static_cast<double>(view->cameraFarPlane() - view->cameraNearPlane()),
        "tall thin-sector floor lies beyond the authored orthographic ray span");
    check(tallPlacement && tallHaveSurface && tallSurface.sector == 0
            && tallSurface.surface == 0 && tallPoint.z == 0.0
            && tallNormal.x() == 0.0f && tallNormal.y() == 0.0f
            && tallNormal.z() == 1.0f,
        "low-angle orthographic placement reaches the tall sector floor beyond view-depth span");

    window.mutableDocumentStateForTest().level = thickLevel;
    view->setLevel(
        &window.mutableDocumentStateForTest().level,
        GlView::SetLevelMode::resetView);
    view->fitViewToLevel();
    view->setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view->setOrbitViewImmediate(32.0f, 42.0f, 0.0f);
    const std::optional<QPointF> thickCenter = view->projectWorldToViewport(
        QVector3D(0.5f, 0.5f, 0.5f));
    bool thickHaveSurface = false;
    SurfaceRef thickSurface{};
    QVector3D thickNormal{};
    TVector thickPoint{};
    const std::optional<QVector3D> thickPlacement = thickCenter
        ? view->placementPointForViewport(
            QPoint(qRound(thickCenter->x()), qRound(thickCenter->y())),
            &thickHaveSurface,
            &thickSurface,
            &thickNormal,
            true,
            false,
            &thickPoint)
        : std::nullopt;
    check(thickPlacement && thickHaveSurface && thickSurface.sector == 0
            && thickSurface.surface == 0,
        "unit-thickness angled orthographic control selects the floor surface");
    check(thickPlacement && std::isfinite(thickPoint.z)
            && std::abs(thickPoint.z) <= 1.0e-6,
        "unit-thickness angled orthographic floor hit stays on the authored floor plane");
    check(thickPlacement && std::abs(thickNormal.x()) <= 1.0e-6f
            && std::abs(thickNormal.y()) <= 1.0e-6f
            && std::abs(thickNormal.z() - 1.0f) <= 1.0e-6f,
        "unit-thickness angled orthographic floor hit returns the upward floor normal");
}

} // namespace sed::app_tests
