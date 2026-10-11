#include "test_common.hpp"

#include "render/overlays/frame_path_overlay_utils.hpp"
#include "render/cleave/closed_cleave_loop_utils.hpp"
#include "render/cleave/cleave_measurement_utils.hpp"
#include "render/grid/viewport_grid_utils.hpp"
#include "render/picking/viewport_ray_utils.hpp"
#include "sed/level/geometry.hpp"
#include "ui/surface/surface_extrude_preview_utils.hpp"

#include <QMatrix4x4>

#include <cmath>
#include <limits>

namespace sed::app_tests
{
namespace
{

template <typename Point>
double pointX(const Point& point)
{
    if constexpr ( requires { point.x(); } )
    {
        return point.x();
    }
    else
    {
        return point.x;
    }
}

} // namespace

void runEditingPrecisionTests()
{
    {
        QMatrix4x4 view{};
        view.setToIdentity();
        QMatrix4x4 orthographicProjection{};
        orthographicProjection.ortho(-1.0f, 1.0f, -1.0f, 1.0f, -8.0f, 3.0f);
        const auto clippedOrthoSpan = render::viewportRayMaxDistanceForWidget(
            orthographicProjection, view, QPointF(500.0, 500.0), 1000, 1000, true);
        check(clippedOrthoSpan && std::abs(*clippedOrthoSpan - 11.0) < 1.0e-5,
            "viewport ray extent spans the complete signed orthographic near/far interval");

        QMatrix4x4 unclippedOrthographicProjection{};
        unclippedOrthographicProjection.ortho(
            -1.0f, 1.0f, -1.0f, 1.0f, -100000.0f, 100000.0f);
        const auto unclippedOrthoSpan = render::viewportRayMaxDistanceForWidget(
            unclippedOrthographicProjection, view, QPointF(500.0, 500.0), 1000, 1000, true);
        check(unclippedOrthoSpan && std::abs(*unclippedOrthoSpan - 200000.0) < 0.05,
            "viewport ray extent uses the full clipping-disabled orthographic projection");

        QMatrix4x4 perspectiveProjection{};
        perspectiveProjection.perspective(60.0f, 1.0f, 0.5f, 16.0f);
        const auto centerPerspectiveSpan = render::viewportRayMaxDistanceForWidget(
            perspectiveProjection, view, QPointF(500.0, 500.0), 1000, 1000, false);
        const auto edgePerspectiveSpan = render::viewportRayMaxDistanceForWidget(
            perspectiveProjection, view, QPointF(1000.0, 500.0), 1000, 1000, false);
        check(centerPerspectiveSpan && std::abs(*centerPerspectiveSpan - 16.0) < 1.0e-4,
            "viewport ray extent matches the far plane on the perspective center ray");
        check(edgePerspectiveSpan
                && std::abs(*edgePerspectiveSpan - (32.0 / std::sqrt(3.0))) < 1.0e-4,
            "viewport ray extent follows the longer off-axis perspective segment");

        QMatrix4x4 unclippedPerspectiveProjection{};
        unclippedPerspectiveProjection.perspective(60.0f, 1.0f, 0.1f, 100000.0f);
        const auto unclippedCenterPerspectiveSpan = render::viewportRayMaxDistanceForWidget(
            unclippedPerspectiveProjection, view, QPointF(500.0, 500.0), 1000, 1000, false);
        const auto unclippedEdgePerspectiveSpan = render::viewportRayMaxDistanceForWidget(
            unclippedPerspectiveProjection, view, QPointF(1000.0, 500.0), 1000, 1000, false);
        // The requested 100,000-unit endpoint is near the precision limit of
        // QMatrix4x4's float perspective coefficients. Define the endpoint
        // from the actual stored clip equation and verify both center and
        // off-axis rays reach that represented plane.
        const double representedFarDepth = std::abs(
            static_cast<double>(unclippedPerspectiveProjection(2, 3))
            / (static_cast<double>(unclippedPerspectiveProjection(2, 2)) + 1.0));
        const double representedEdgeSlope =
            1.0 / static_cast<double>(unclippedPerspectiveProjection(0, 0));
        const double representedEdgeDistance = representedFarDepth
            * std::sqrt(1.0 + representedEdgeSlope * representedEdgeSlope);
        check(representedFarDepth > 95000.0 && representedFarDepth < 105000.0
                && unclippedCenterPerspectiveSpan
                && std::abs(*unclippedCenterPerspectiveSpan - representedFarDepth) < 1.0e-6,
            "viewport ray extent reaches the clipping-disabled perspective matrix far endpoint");
        check(unclippedEdgePerspectiveSpan
                && std::abs(*unclippedEdgePerspectiveSpan - representedEdgeDistance) < 1.0e-6,
            "clipping-disabled off-axis perspective ray reaches its full represented far-plane endpoint");
        QMatrix4x4 singularProjection{};
        singularProjection.fill(0.0f);
        check(!render::viewportRayMaxDistanceForWidget(
                  singularProjection, view, QPointF(500.0, 500.0), 1000, 1000, false)
                && !render::viewportRayMaxDistanceForWidget(
                    orthographicProjection, view, QPointF(0.0, 0.0), 0, 1000, true)
                && !render::viewportRayMaxDistanceForWidget(
                    orthographicProjection, view,
                    QPointF(std::numeric_limits<double>::quiet_NaN(), 0.0), 1000, 1000, true),
            "viewport ray extent rejects singular projections, invalid viewport dimensions and pointer coordinates");
    }
    check(render::cleaveLengthText(1.2e-7, render::CleaveMeasureUnit::Bare).toDouble() > 0.0,
        "Cleave measurement labels never present a finite nonzero tiny length as zero");
    check(render::cleaveLineNumberText(0) == QStringLiteral("1")
            && render::cleaveLineNumberText(1) == QStringLiteral("2")
            && render::cleaveLineNumberText(999) == QStringLiteral("1000"),
        "Cleave line-number labels follow 1-based authored order without a fixed digit width");
    for (const double origin : {0.0, 10000.0, 1000000.0})
    {
        const TVector first{origin, origin, 0.0};
        const TVector second{origin + 0.00002, origin, 0.0};
        std::vector<TVector> polygon{first, second,
            {second.x, origin + 0.00002, 0.0}, {origin, origin + 0.00002, 0.0}};
        check(render::cleaveLineMatchesPolygonEdge(first, second, polygon)
                && render::cleaveLineMatchesPolygonEdge(second, first, polygon),
            "Cleave leader suppresses only an attached cap edge in either direction");
        for (TVector& point : polygon) point.x += 0.00004;
        check(!render::cleaveLineMatchesPolygonEdge(first, second, polygon),
            "Cleave leader remains visible for a distinct micron-scale cap polygon");
        if (origin != 0.0)
        {
            polygon = {first, second};
            polygon[0].x = std::nextafter(first.x, std::numeric_limits<double>::infinity());
            check(render::cleaveLineMatchesPolygonEdge(first, second, polygon),
                "Cleave leader tolerates arithmetic roundoff in independently recomputed cap endpoints");
        }
    }
    {
        Sector extreme{};
        extreme.newVertex(1.0e308, 0.0, 0.0);
        extreme.newVertex(-1.0e308, 0.0, 0.0);
        Surface face{}; face.vertices = {0, 1};
        const auto center = render::cleaveSurfaceCenter(extreme, face);
        check(!center || render::cleaveFinitePoint(*center),
            "Cleave center fallback never returns a present nonfinite point after arithmetic overflow");
    }
    for (const double origin : {0.0, 1000000.0})
    for (const double scale : {1.0, 1.0e-6})
    for (const bool subdivided : {false, true})
    {
        Sector sector{};
        sector.newVertex(origin, origin, 0.0);
        sector.newVertex(origin + 4.0 * scale, origin, 0.0);
        sector.newVertex(origin + 2.0 * scale, origin + 2.0 * scale, 0.0);
        sector.newVertex(origin, origin + 2.0 * scale, 0.0);
        sector.newVertex(origin + 3.0 * scale, origin, 0.0);
        Surface face{};
        face.vertices = subdivided ? std::vector<std::int32_t>{0,4,1,2,3}
                                   : std::vector<std::int32_t>{0,1,2,3};
        const auto center = render::cleaveSurfaceCenter(sector, face);
        const TVector expected{origin + 14.0 / 9.0 * scale, origin + 8.0 / 9.0 * scale, 0.0};
        check(center && (*center - expected).length() < 4.0e-10,
            "Cleave center snap preserves analytic area centroid under subdivision, scaling and translation");
    }
    for ( const double origin : {0.0, 10000.0, 1000000.0} )
    {
        for ( const bool orthographic : {true, false} )
        {
            QMatrix4x4 projection;
            if ( orthographic )
            {
                projection.ortho(-0.5f, 0.5f, -0.5f, 0.5f, 0.1f, 100.0f);
            }
            else
            {
                projection.perspective(60.0f, 1.0f, 0.1f, 100.0f);
            }
            const QVector3D eye(static_cast<float>(origin), static_cast<float>(origin),
                static_cast<float>(origin + 10.0));
            QMatrix4x4 view;
            view.translate(-eye);
            const auto firstRay = render::viewportRayForWidget(projection, view, eye,
                QPointF(500.0, 500.0), 1000, 1000, orthographic);
            const auto secondRay = render::viewportRayForWidget(projection, view, eye,
                QPointF(501.0, 500.0), 1000, 1000, orthographic);
            const TVector point{origin, origin, origin};
            const auto first = firstRay ? render::cleaveIntersectRayPlane(*firstRay, point, {0, 0, 1}) : std::nullopt;
            const auto second = secondRay ? render::cleaveIntersectRayPlane(*secondRay, point, {0, 0, 1}) : std::nullopt;
            check(first && second, "cleave precision: camera-local rays intersect translated editing plane");
            if ( first && second )
            {
                const double expected = orthographic ? 0.001 : 20.0 / (1000.0 * projection(0, 0));
                check(std::abs((second->x - first->x) - expected) < 2.0e-8
                        && std::abs(second->z - origin) < 1.0e-8,
                    "cleave precision: one-pixel pointer motion preserves analytic translated plane movement");
            }
        }
        const render::TransformGridFrame frame = render::makeTransformGridFrame(
            {}, {1, 0, 0}, {0, 1, 0}, 0.001);
        const TVector input{origin + 0.0014, origin + 0.0026, origin + 0.0003};
        const TVector snapped = render::snapPointToGridOnPlane(input, input, {0, 0, 1.0e-12}, frame);
        check(std::abs(snapped.x - (origin + 0.001)) < 1.0e-9
                && std::abs(snapped.y - (origin + 0.003)) < 1.0e-9 && snapped.z == input.z,
            "cleave precision: grid snapping retains fine document coordinates and exact interaction plane");
    }
    for ( const double normalScale : {1.0, 1.0e-12} )
    {
        // The screen polygon is comfortably visible at the authored zoom;
        // the world-space cut must not impose an unrelated minimum size.
        const std::vector<QPointF> screen{{0.0, 0.0}, {100.0, 0.0},
            {100.0, 100.0}, {0.0, 100.0}};
        using PlanePoint = decltype(render::ClosedCleaveLoopPlane{}.point);
        const double side = 1.0e-6;
        const std::vector<render::ClosedCleaveLoopPlane> planes{
            {PlanePoint(0.0, normalScale, 0.0), PlanePoint(0.0, 0.0, 0.0)},
            {PlanePoint(-normalScale, 0.0, 0.0), PlanePoint(side, 0.0, 0.0)},
            {PlanePoint(0.0, -normalScale, 0.0), PlanePoint(side, side, 0.0)},
            {PlanePoint(normalScale, 0.0, 0.0), PlanePoint(0.0, side, 0.0)}};
        const PlanePoint interior(side * 0.5, side * 0.5, 0.0);
        check(render::validateClosedCleaveLoop(screen, planes, interior).valid(),
            "cleave precision: visible micron-wide loop is valid independently of normal scale");
        auto invalid = planes;
        invalid[0].normal = {};
        check(render::validateClosedCleaveLoop(screen, invalid, interior).error
                == render::ClosedCleaveLoopError::InvalidPlane,
            "cleave precision: zero plane normal remains invalid");
        invalid = planes;
        invalid[0].point = interior;
        check(render::validateClosedCleaveLoop(screen, invalid, interior).error
                == render::ClosedCleaveLoopError::InteriorOnPlane,
            "cleave precision: exact interior-on-plane remains invalid at micron scale");
    }
    for ( const double origin : {0.0, 10000.0, 1000000.0} )
    {
        Level level{};
        auto& sector = level.newSector();
        sector.newVertex(origin, -0.5, -0.5);
        sector.newVertex(origin, 0.5, -0.5);
        sector.newVertex(origin, 0.5, 0.5);
        sector.newVertex(origin, -0.5, 0.5);
        Surface face{};
        face.vertices = {0, 1, 2, 3};
        face.texVertices.resize(4);
        face.normal = level::surfaceCalcNormalNewell(sector, face);
        sector.addSurface(std::move(face));

        const auto lines = ui::surface::buildSurfaceExtrudePreviewLines(level, {0, 0}, 0.001, 0.0);
        check(lines.size() == 24, "extrude precision: complete preview loops and connectors");
        if ( lines.size() != 24 )
        {
            continue;
        }
        const double distance = std::abs(pointX(lines[17]) - pointX(lines[16]));
        check(std::abs(distance - 0.001) < 1.0e-9,
            "extrude precision: distant preview retains authored one-millith length");

        QMatrix4x4 projection;
        projection(0, 3) = static_cast<float>(-origin);
        const auto projected = render::frame_path::projectWorldSegment(
            projection, lines[16], lines[17], 1000, 1000);
        check(projected.has_value(), "extrude precision: distant connector remains projectable");
        if ( projected )
        {
            check(std::abs(std::abs(projected->second.x() - projected->first.x()) - 0.5) < 1.0e-5,
                "extrude precision: double world coordinates preserve analytic screen separation");
        }
    }
}

} // namespace sed::app_tests
