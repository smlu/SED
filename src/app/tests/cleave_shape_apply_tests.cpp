#include "test_common.hpp"

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/assets/sed.hpp"
#include "sed/level/geometry.hpp"
#include "ui/layers/layer_color_utils.hpp"
#include "ui/viewport/cleave_shape_utils.hpp"
#include "ui/components/scrub_float_edit.hpp"

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QSpinBox>
#include <QToolButton>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sed::app_tests
{
namespace
{

using Point2 = ui::viewport::CleaveShapePoint2D;
using Polygon2 = std::vector<Point2>;

struct ShapeCase
{
    ui::viewport::CleaveShapeKind kind{};
    double width{};
    double height{};
    bool dimensionsLinked{};
    int segments{};
    int points{};
    const char* name{};
};

bool sameStagedLines(
    const std::vector<GlView::RecoveryState::CleaveLine>& first,
    const std::vector<GlView::RecoveryState::CleaveLine>& second)
{
    if (first.size() != second.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < first.size(); ++index)
    {
        const auto& a = first[index];
        const auto& b = second[index];
        if (!closeVec(a.first, b.first, 1.0e-9)
            || !closeVec(a.second, b.second, 1.0e-9)
            || !closeVec(a.planeNormal, b.planeNormal, 1.0e-9)
            || !closeVec(a.constructionPlaneNormal, b.constructionPlaneNormal, 1.0e-9)
            || !closeVec(a.angleReferenceAxis, b.angleReferenceAxis, 1.0e-9)
            || a.linkedToPrevious != b.linkedToPrevious
            || a.linkedToNext != b.linkedToNext)
        {
            return false;
        }
    }
    return true;
}

void sendMouse(
    QWidget& target,
    const QEvent::Type type,
    const QPointF& position,
    const Qt::MouseButton button,
    const Qt::MouseButtons buttons,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    const QPointF global = target.mapToGlobal(position.toPoint());
    QMouseEvent event(
        type,
        position,
        global,
        global,
        button,
        buttons,
        modifiers);
    QApplication::sendEvent(&target, &event);
}

void click(
    QWidget& target,
    const QPointF& position,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    sendMouse(
        target,
        QEvent::MouseButtonPress,
        position,
        Qt::LeftButton,
        Qt::LeftButton,
        modifiers);
    sendMouse(
        target,
        QEvent::MouseButtonRelease,
        position,
        Qt::LeftButton,
        Qt::NoButton,
        modifiers);
    QApplication::processEvents(QEventLoop::AllEvents);
}

void drag(
    QWidget& target,
    const QPointF& first,
    const QPointF& second,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    sendMouse(
        target,
        QEvent::MouseButtonPress,
        first,
        Qt::LeftButton,
        Qt::LeftButton,
        modifiers);
    sendMouse(
        target,
        QEvent::MouseMove,
        second,
        Qt::NoButton,
        Qt::LeftButton,
        modifiers);
    sendMouse(
        target,
        QEvent::MouseButtonRelease,
        second,
        Qt::LeftButton,
        Qt::NoButton,
        modifiers);
    QApplication::processEvents(QEventLoop::AllEvents);
}

void sendKey(
    QWidget& target,
    const QEvent::Type type,
    const int key,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier,
    const QString& text = {})
{
    QKeyEvent event(type, key, modifiers, text, false, 1);
    QApplication::sendEvent(&target, &event);
}

editor::DocumentState makeWallState(const bool selectSurface)
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.uvCoordinateSpace = UvCoordinateSpace::Normalized;
    state.level.layers = { "Layer0" };
    state.level.lvisstring = ui::layers::buildLayerVisibilityString(
        state.level.layers.size(),
        std::vector<bool>(state.level.layers.size(), true));

    Sector sector{};
    sector.layer = 0;
    const std::array<TVector, 8> points{{
        { -1.0, -1.0, 0.0 },
        { 1.0, -1.0, 0.0 },
        { 1.0, 1.0, 0.0 },
        { -1.0, 1.0, 0.0 },
        { -1.0, -1.0, 1.0 },
        { 1.0, -1.0, 1.0 },
        { 1.0, 1.0, 1.0 },
        { -1.0, 1.0, 1.0 },
    }};
    for (const TVector& point : points)
    {
        sector.newVertex(point.x, point.y, point.z);
    }

    const std::array<std::array<std::int32_t, 4>, 6> faces{{
        { 1, 5, 6, 2 },
        { 0, 3, 7, 4 },
        { 0, 4, 5, 1 },
        { 4, 7, 6, 5 },
        { 0, 1, 2, 3 },
        { 3, 2, 6, 7 },
    }};
    for (const auto& indices : faces)
    {
        Surface surface{};
        surface.vertices.assign(indices.begin(), indices.end());
        surface.texVertices.resize(surface.vertices.size());
        surface.normal = level::surfaceCalcNormalNewell(sector, surface);
        sector.surfaces.push_back(std::move(surface));
    }
    Surface& wall = sector.surfaces[5];
    wall.texVertices = {
        TexVertex{ 0.0f, 0.0f, ColorF{ 0.0f, 0.0f, 0.0f, 1.0f } },
        TexVertex{ 1.0f, 0.0f, ColorF{ 0.0f, 0.0f, 0.0f, 1.0f } },
        TexVertex{ 1.0f, 1.0f, ColorF{ 0.0f, 0.0f, 0.0f, 1.0f } },
        TexVertex{ 0.0f, 1.0f, ColorF{ 0.0f, 0.0f, 0.0f, 1.0f } },
    };
    state.level.sectors.push_back(std::move(sector));
    if (selectSurface)
    {
        state.selection.surfaces.insert(SurfaceRef{ 0, 5 });
    }
    else
    {
        state.selection.sectors.insert(0);
    }
    return state;
}

double signedArea(const Polygon2& polygon)
{
    double area = 0.0;
    for (std::size_t index = 0; index < polygon.size(); ++index)
    {
        const Point2& first = polygon[index];
        const Point2& second = polygon[(index + 1) % polygon.size()];
        area += first.x * second.y - first.y * second.x;
    }
    return area * 0.5;
}

double cross2(const Point2& first, const Point2& second, const Point2& point)
{
    return (second.x - first.x) * (point.y - first.y)
        - (second.y - first.y) * (point.x - first.x);
}

Polygon2 intersectConvexPolygons(Polygon2 subject, const Polygon2& clipper)
{
    const double orientation = signedArea(clipper) >= 0.0 ? 1.0 : -1.0;
    constexpr double kInsideTolerance = 1.0e-10;
    for (std::size_t edge = 0; edge < clipper.size() && !subject.empty(); ++edge)
    {
        const Point2& clipFirst = clipper[edge];
        const Point2& clipSecond = clipper[(edge + 1) % clipper.size()];
        Polygon2 output{};
        output.reserve(subject.size() + 1);
        Point2 previous = subject.back();
        double previousDistance = orientation * cross2(clipFirst, clipSecond, previous);
        bool previousInside = previousDistance >= -kInsideTolerance;
        for (const Point2& current : subject)
        {
            const double currentDistance = orientation * cross2(
                clipFirst, clipSecond, current);
            const bool currentInside = currentDistance >= -kInsideTolerance;
            if (currentInside != previousInside)
            {
                const double denominator = previousDistance - currentDistance;
                if (std::abs(denominator) > 1.0e-15)
                {
                    const double parameter = previousDistance / denominator;
                    output.push_back(Point2{
                        previous.x + (current.x - previous.x) * parameter,
                        previous.y + (current.y - previous.y) * parameter,
                    });
                }
            }
            if (currentInside)
            {
                output.push_back(current);
            }
            previous = current;
            previousDistance = currentDistance;
            previousInside = currentInside;
        }
        subject = std::move(output);
    }
    return subject;
}

bool pointInPolygon(const Point2& point, const Polygon2& polygon)
{
    bool inside = false;
    for (std::size_t current = 0, previous = polygon.size() - 1;
        current < polygon.size();
        previous = current++)
    {
        const Point2& a = polygon[current];
        const Point2& b = polygon[previous];
        if ((a.y > point.y) != (b.y > point.y)
            && point.x < (b.x - a.x) * (point.y - a.y)
                    / (b.y - a.y)
                + a.x)
        {
            inside = !inside;
        }
    }
    return inside;
}

Polygon2 sourceWallPolygon(const Surface& surface, const Sector& sector)
{
    Polygon2 polygon{};
    polygon.reserve(surface.vertices.size());
    for (const std::int32_t index : surface.vertices)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size())
        {
            return {};
        }
        const Vertex& vertex = sector.vertices[static_cast<std::size_t>(index)];
        polygon.push_back(Point2{ vertex.x, vertex.z });
    }
    return polygon;
}

double surfaceAreaXZ(const Sector& sector, const Surface& surface)
{
    return std::abs(signedArea(sourceWallPolygon(surface, sector)));
}

Polygon2 shapeSilhouette(const ui::viewport::CleaveShapePlan& plan)
{
    Polygon2 result{};
    result.reserve(plan.perimeter.size());
    for (const Point2& point : plan.perimeter)
    {
        result.push_back(Point2{ point.x, 0.5 + point.y });
    }
    return result;
}

Polygon2 shapeCellPolygon(const ui::viewport::CleaveShapeCell2D& cell)
{
    Polygon2 result{};
    result.reserve(cell.vertices.size());
    for (const Point2& point : cell.vertices)
    {
        result.push_back(Point2{ point.x, 0.5 + point.y });
    }
    return result;
}

bool faceInsideShapeCells(
    const Polygon2& face,
    const std::vector<ui::viewport::CleaveShapeCell2D>& cells)
{
    if (face.size() < 3)
    {
        return false;
    }
    double coveredArea = 0.0;
    for (const ui::viewport::CleaveShapeCell2D& cell : cells)
    {
        const Polygon2 intersection = intersectConvexPolygons(
            face, shapeCellPolygon(cell));
        if (intersection.size() >= 3)
        {
            coveredArea += std::abs(signedArea(intersection));
        }
    }
    return std::abs(coveredArea - std::abs(signedArea(face))) <= 1.0e-7;
}

bool polygonsHavePositiveAreaOverlap(const Polygon2& first, const Polygon2& second)
{
    const Polygon2 intersection = intersectConvexPolygons(first, second);
    return intersection.size() >= 3
        && std::abs(signedArea(intersection)) > 1.0e-8;
}

bool configureSurfaceWindow(MainWindow& window)
{
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(1280, 900);
    window.activateSurfaceEditModeForTest();
    window.activateSurfaceFaceSubModeForTest();
    window.setDocumentStateForTest(makeWallState(true));
    window.activateSurfaceEditModeForTest();
    window.activateSurfaceFaceSubModeForTest();
    window.setLayerVisualStateForTest({ true }, { false });
    window.show();
    window.activateWindow();
    QApplication::setActiveWindow(&window);
    QApplication::processEvents(QEventLoop::AllEvents);

    GlView* const view = window.glViewForTest();
    QAction* const cleaveAction = window.findChild<QAction*>(
        QStringLiteral("viewport.surface.cleave"));
    QToolButton* const multiCut = window.findChild<QToolButton*>(
        QStringLiteral("cleaveMultiCutButton"));
    QToolButton* const chain = window.findChild<QToolButton*>(
        QStringLiteral("cleaveChainButton"));
    QToolButton* const bounded = window.findChild<QToolButton*>(
        QStringLiteral("cleaveBoundedButton"));
    if (!view || !cleaveAction || !multiCut || !chain || !bounded)
    {
        return false;
    }

    view->setLevel(
        &window.documentStateForTest().level,
        GlView::SetLevelMode::resetView);
    view->setSelectionState(
        0,
        5,
        -1,
        -1,
        window.documentStateForTest().selection);
    view->setSurfaceSubMode(GlView::SurfaceSubMode::Face);
    view->setCleaveStagedMode(true);
    view->setSurfaceCleaveToolEnabled(true);
    view->setCleaveChainEnabled(false);
    view->setCleaveBounded(true);
    view->setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view->setOrbitViewImmediate(180.0f, 0.0f, 0.0f);
    view->setCameraOrbitDistance(4.0f);
    view->setCameraOrthoScale(128.0f);
    view->setGridPlane(GlView::GridPlane::XZ);
    view->setGridTransformOrigin(QVector3D(0.0f, 1.0f, 0.5f));
    view->setGridMinorSpacing(0.1f);
    view->setShowGrid(false);
    view->setShowSolid(false);
    view->setShowTextures(false);
    view->setShowWireframe(false);
    view->setShowVertices(false);
    view->setShowFog(false);
    view->setShowThings(false);

    window.updateEditActionsForTest();
    if (!cleaveAction->isEnabled())
    {
        return false;
    }
    multiCut->setChecked(true);
    chain->setChecked(false);
    bounded->setChecked(true);
    if (!cleaveAction->isChecked())
    {
        cleaveAction->trigger();
    }
    QApplication::processEvents(QEventLoop::AllEvents);
    view->captureStagedCleaveContextForTest(true);
    view->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents(QEventLoop::AllEvents);
    return cleaveAction->isChecked()
        && multiCut->isChecked()
        && !chain->isChecked()
        && bounded->isChecked()
        && view->cleaveStagedMode()
        && view->surfaceCleaveToolEnabled()
        && view->activeSurfaceSectorIndex() == 0
        && view->activeSurfaceIndex() == 5
        && view->stagedCleaveTargetsMatchCurrentForTest(true);
}

bool configureSectorWindow(MainWindow& window)
{
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(1280, 900);
    window.activateSectorEditModeForTest();
    window.setDocumentStateForTest(makeWallState(false));
    window.activateSectorEditModeForTest();
    window.setLayerVisualStateForTest({ true }, { false });
    window.show();
    window.activateWindow();
    QApplication::setActiveWindow(&window);
    QApplication::processEvents(QEventLoop::AllEvents);

    GlView* const view = window.glViewForTest();
    QAction* const cleaveAction = window.findChild<QAction*>(
        QStringLiteral("viewport.sector.cleave"));
    QToolButton* const multiCut = window.findChild<QToolButton*>(
        QStringLiteral("cleaveMultiCutButton"));
    QToolButton* const chain = window.findChild<QToolButton*>(
        QStringLiteral("cleaveChainButton"));
    QToolButton* const bounded = window.findChild<QToolButton*>(
        QStringLiteral("cleaveBoundedButton"));
    if (!view || !cleaveAction || !multiCut || !chain || !bounded)
    {
        return false;
    }

    view->setLevel(
        &window.documentStateForTest().level,
        GlView::SetLevelMode::resetView);
    view->setSelectionState(
        0,
        -1,
        -1,
        -1,
        window.documentStateForTest().selection);
    view->setCleaveStagedMode(true);
    view->setSectorCleaveToolEnabled(true);
    view->setSurfaceCleaveToolEnabled(false);
    view->setCleaveChainEnabled(false);
    view->setCleaveBounded(true);
    view->setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view->setOrbitViewImmediate(180.0f, 0.0f, 0.0f);
    view->setCameraOrbitDistance(4.0f);
    view->setCameraOrthoScale(128.0f);
    view->setGridPlane(GlView::GridPlane::XZ);
    view->setGridTransformOrigin(QVector3D(0.0f, 1.0f, 0.5f));
    view->setGridMinorSpacing(0.1f);
    view->setShowGrid(false);
    view->setShowSolid(false);
    view->setShowTextures(false);
    view->setShowWireframe(false);
    view->setShowVertices(false);
    view->setShowFog(false);
    view->setShowThings(false);

    window.updateEditActionsForTest();
    if (!cleaveAction->isEnabled())
    {
        return false;
    }
    multiCut->setChecked(true);
    chain->setChecked(false);
    bounded->setChecked(true);
    if (!cleaveAction->isChecked())
    {
        cleaveAction->trigger();
    }
    QApplication::processEvents(QEventLoop::AllEvents);
    view->captureStagedCleaveContextForTest(false);
    view->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents(QEventLoop::AllEvents);
    return cleaveAction->isChecked()
        && multiCut->isChecked()
        && !chain->isChecked()
        && bounded->isChecked()
        && view->cleaveStagedMode()
        && view->sectorCleaveToolEnabled()
        && view->activeSectorIndex() == 0
        && window.documentStateForTest().selection.sectors.contains(0)
        && view->stagedCleaveTargetsMatchCurrentForTest(false);
}

bool setShapeControls(
    MainWindow& window,
    const ShapeCase& shape)
{
    QToolButton* const addShape = window.findChild<QToolButton*>(
        QStringLiteral("cleaveAddShapeButton"));
    QComboBox* const kind = window.findChild<QComboBox*>(
        QStringLiteral("cleaveShapeKindCombo"));
    QDoubleSpinBox* const width = window.findChild<QDoubleSpinBox*>(
        QStringLiteral("cleaveShapeWidthSpin"));
    QDoubleSpinBox* const height = window.findChild<QDoubleSpinBox*>(
        QStringLiteral("cleaveShapeHeightSpin"));
    QToolButton* const linked = window.findChild<QToolButton*>(
        QStringLiteral("cleaveShapeDimensionsLinkedButton"));
    QComboBox* const units = window.findChild<QComboBox*>(
        QStringLiteral("cleaveUnitsCombo"));
    QSpinBox* const segments = window.findChild<QSpinBox*>(
        QStringLiteral("cleaveShapeSegmentsSpin"));
    QSpinBox* const points = window.findChild<QSpinBox*>(
        QStringLiteral("cleaveShapePointsSpin"));
    if (!addShape || !kind || !width || !height || !linked || !units
        || !segments || !points)
    {
        return false;
    }
    if (units->findData(static_cast<int>(GlView::CleaveMeasureUnit::Bare)) >= 0)
    {
        units->setCurrentIndex(units->findData(
            static_cast<int>(GlView::CleaveMeasureUnit::Bare)));
    }
    if (!addShape->isChecked())
    {
        addShape->click();
    }
    const int kindIndex = kind->findData(static_cast<int>(shape.kind));
    if (kindIndex < 0)
    {
        return false;
    }
    kind->setCurrentIndex(kindIndex);
    const bool expectedLinked = shape.dimensionsLinked;
    if (linked->isEnabled() && linked->isChecked() != expectedLinked)
    {
        linked->click();
    }
    width->setValue(shape.width);
    height->setValue(shape.height);
    segments->setValue(shape.segments);
    points->setValue(shape.points);
    QApplication::processEvents(QEventLoop::AllEvents);
    const bool ready = addShape->isChecked()
        && kind->currentData().toInt() == static_cast<int>(shape.kind)
        && linked->isChecked() == expectedLinked
        && std::abs(width->value() - shape.width) < 1.0e-9
        && std::abs(height->value() - shape.height) < 1.0e-9;
    if ( !ready )
    {
        std::fprintf(
            stderr,
            "Shape controls not ready: kind=%d expectedKind=%d linked=%d enabled=%d expectedLinked=%d width=%.9g expectedWidth=%.9g height=%.9g expectedHeight=%.9g armed=%d\n",
            kind->currentData().toInt(),
            static_cast<int>(shape.kind),
            linked->isChecked() ? 1 : 0,
            linked->isEnabled() ? 1 : 0,
            expectedLinked ? 1 : 0,
            width->value(),
            shape.width,
            height->value(),
            shape.height,
            addShape->isChecked() ? 1 : 0);
    }
    return ready;
}

bool expectedShapePlacement(
    const GlView::RecoveryState& draft,
    const ui::viewport::CleaveShapePlan& plan)
{
    std::vector<ui::viewport::CleaveShapeStroke2D> expected;
    expected.insert(
        expected.end(),
        plan.decompositionStrokes.begin(),
        plan.decompositionStrokes.end());
    for (std::size_t index = 0; index < plan.perimeter.size(); ++index)
    {
        expected.push_back(ui::viewport::CleaveShapeStroke2D{
            plan.perimeter[index],
            plan.perimeter[(index + 1) % plan.perimeter.size()],
        });
    }
    if ( draft.cleaveLines.size() != expected.size()
        || draft.applyOrder.size() != expected.size())
    {
        std::fprintf(
            stderr,
            "Shape stroke count/order mismatch: actualLines=%zu actualOrder=%zu expected=%zu\n",
            draft.cleaveLines.size(),
            draft.applyOrder.size(),
            expected.size());
        return false;
    }
    for ( std::size_t index = 0; index < expected.size(); ++index )
    {
        const GlView::RecoveryState::CleaveLine& line = draft.cleaveLines[index];
        const auto closePoint = [](const TVector& actual, const Point2& local)
        {
            return std::abs(actual.x - local.x) <= 0.025
                && std::abs(actual.y - 1.0) <= 0.025
                && std::abs(actual.z - (0.5 + local.y)) <= 0.025;
        };
        if ( !closePoint(line.first, expected[index].first)
            || !closePoint(line.second, expected[index].second)
            || line.linkedToPrevious || line.linkedToNext
            || draft.applyOrder[index] != static_cast<std::int32_t>(index))
        {
            std::fprintf(
                stderr,
                "Shape stroke mismatch[%zu]: actual=(%.12g,%.12g,%.12g)->(%.12g,%.12g,%.12g) expectedLocal=(%.12g,%.12g)->(%.12g,%.12g) expectedWorld=(%.12g,1,%.12g)->(%.12g,1,%.12g) links=(%d,%d) order=%d\n",
                index,
                line.first.x, line.first.y, line.first.z,
                line.second.x, line.second.y, line.second.z,
                expected[index].first.x,
                expected[index].first.y,
                expected[index].second.x,
                expected[index].second.y,
                expected[index].first.x,
                0.5 + expected[index].first.y,
                expected[index].second.x,
                0.5 + expected[index].second.y,
                line.linkedToPrevious ? 1 : 0,
                line.linkedToNext ? 1 : 0,
                draft.applyOrder[index]);
            return false;
        }
    }
    return true;
}

std::vector<SurfaceRef> validateSurfacePartition(
    const Level& level,
    const ui::viewport::CleaveShapePlan& plan)
{
    std::vector<SurfaceRef> interior{};
    if (level.sectors.empty())
    {
        return interior;
    }
    const Polygon2 silhouette = shapeSilhouette(plan);
    double wallArea = 0.0;
    double interiorArea = 0.0;
    std::vector<std::pair<SurfaceRef, Polygon2>> wallFaces{};
    std::vector<Polygon2> interiorFaces{};
    bool validFaces = true;
    for ( std::size_t sectorIndex = 0;
        sectorIndex < level.sectors.size();
        ++sectorIndex )
    {
        const Sector& sector = level.sectors[sectorIndex];
        for (std::size_t surfaceIndex = 0;
            surfaceIndex < sector.surfaces.size();
            ++surfaceIndex)
        {
            const Surface& surface = sector.surfaces[surfaceIndex];
            if (surface.vertices.size() < 3)
            {
                continue;
            }
            bool onWall = true;
            for (const std::int32_t vertexIndex : surface.vertices)
            {
                if (vertexIndex < 0
                    || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size()
                    || std::abs(sector.vertices[static_cast<std::size_t>(vertexIndex)].y - 1.0)
                        > 1.0e-7)
                {
                    onWall = false;
                    break;
                }
            }
            if (!onWall)
            {
                continue;
            }
            Polygon2 polygon = sourceWallPolygon(surface, sector);
            const double area = std::abs(signedArea(polygon));
            if (!std::isfinite(area) || area <= 1.0e-10
                || !level::surfaceIsPlanar(sector, surface)
                || !level::surfaceIsConvex(sector, surface))
            {
                validFaces = false;
            }
            wallArea += area;
            wallFaces.emplace_back(
                SurfaceRef{
                    static_cast<std::int32_t>(sectorIndex),
                    static_cast<std::int32_t>(surfaceIndex),
                },
                polygon);
            Point2 centroid{};
            for (const Point2& point : polygon)
            {
                centroid.x += point.x / static_cast<double>(polygon.size());
                centroid.y += point.y / static_cast<double>(polygon.size());
            }
            if (pointInPolygon(centroid, silhouette))
            {
                validFaces = validFaces && faceInsideShapeCells(polygon, plan.convexCells);
                interiorArea += area;
                interior.push_back(SurfaceRef{
                    static_cast<std::int32_t>(sectorIndex),
                    static_cast<std::int32_t>(surfaceIndex),
                });
                interiorFaces.push_back(std::move(polygon));
            }
        }
    }
    for ( std::size_t first = 0; first < wallFaces.size(); ++first )
    {
        for ( std::size_t second = first + 1; second < wallFaces.size(); ++second )
        {
            validFaces = validFaces
                && !polygonsHavePositiveAreaOverlap(
                    wallFaces[first].second,
                    wallFaces[second].second);
        }
    }
    for ( std::size_t first = 0; first < interiorFaces.size(); ++first )
    {
        for ( std::size_t second = first + 1; second < interiorFaces.size(); ++second )
        {
            validFaces = validFaces
                && !polygonsHavePositiveAreaOverlap(
                    interiorFaces[first],
                    interiorFaces[second]);
        }
    }

    const double expectedArea = std::abs(signedArea(silhouette));
    const double expectedCellArea = std::accumulate(
        plan.convexCells.begin(),
        plan.convexCells.end(),
        0.0,
        [](const double total, const ui::viewport::CleaveShapeCell2D& cell)
        {
            return total + std::abs(signedArea(shapeCellPolygon(cell)));
        });
    const bool wallAreaValid = std::abs(wallArea - 2.0) <= 1.0e-6;
    const bool interiorAreaValid =
        std::abs(interiorArea - expectedArea) <= 1.0e-6;
    const bool cellAreaValid =
        std::abs(expectedCellArea - expectedArea) <= 1.0e-6;
    const bool partitionValid = wallAreaValid
        && interiorAreaValid
        && cellAreaValid
        && validFaces
        && !interior.empty();
    if ( !partitionValid )
    {
        std::fprintf(
            stderr,
            "Shape partition mismatch: expectedArea=%.12g expectedCellArea=%.12g wallArea=%.12g interiorArea=%.12g wallOK=%d interiorOK=%d cellsOK=%d facesOK=%d interiorFaces=%zu wallFaces=%zu silhouette=",
            expectedArea,
            expectedCellArea,
            wallArea,
            interiorArea,
            wallAreaValid ? 1 : 0,
            interiorAreaValid ? 1 : 0,
            cellAreaValid ? 1 : 0,
            validFaces ? 1 : 0,
            interior.size(),
            wallFaces.size());
        for ( const Point2& point : silhouette )
        {
            std::fprintf(stderr, " (%.8g,%.8g)", point.x, point.y);
        }
        std::fprintf(stderr, "\n");
        for ( std::size_t index = 0; index < wallFaces.size(); ++index )
        {
            const SurfaceRef ref = wallFaces[index].first;
            const Polygon2& polygon = wallFaces[index].second;
            Point2 centroid{};
            for ( const Point2& point : polygon )
            {
                centroid.x += point.x / static_cast<double>(polygon.size());
                centroid.y += point.y / static_cast<double>(polygon.size());
            }
            std::fprintf(
                stderr,
                "  wallFace[%zu] ref=(%d,%d) area=%.12g centroid=(%.8g,%.8g) silhouette=%d cells=%d vertices=",
                index,
                ref.sector,
                ref.surface,
                std::abs(signedArea(polygon)),
                centroid.x,
                centroid.y,
                pointInPolygon(centroid, silhouette) ? 1 : 0,
                faceInsideShapeCells(polygon, plan.convexCells) ? 1 : 0);
            for (const Point2& point : polygon)
            {
                std::fprintf(stderr, " (%.8g,%.8g)", point.x, point.y);
            }
            std::fprintf(stderr, "\n");
        }
        for ( std::size_t first = 0; first < wallFaces.size(); ++first )
        {
            for ( std::size_t second = first + 1; second < wallFaces.size(); ++second )
            {
                if (polygonsHavePositiveAreaOverlap(
                        wallFaces[first].second,
                        wallFaces[second].second))
                {
                    std::fprintf(
                        stderr,
                        "  overlapping wall faces: (%d,%d) and (%d,%d)\n",
                        wallFaces[first].first.sector,
                        wallFaces[first].first.surface,
                        wallFaces[second].first.sector,
                        wallFaces[second].first.surface);
                }
            }
        }
        interior.clear();
    }
    return interior;
}

bool closedVolume(const Sector& sector, long double& outVolume)
{
    if (sector.vertices.empty())
    {
        return false;
    }
    std::map<std::pair<std::int32_t, std::int32_t>, std::pair<int, int>> edges;
    long double signedVolume = 0.0L;
    const Vertex& anchor = sector.vertices.front();
    const auto point = [&sector, &anchor](const std::int32_t index)
    {
        const Vertex& vertex = sector.vertices[static_cast<std::size_t>(index)];
        return std::array<long double, 3>{
            static_cast<long double>(vertex.x - anchor.x),
            static_cast<long double>(vertex.y - anchor.y),
            static_cast<long double>(vertex.z - anchor.z),
        };
    };
    for (const Surface& surface : sector.surfaces)
    {
        if (surface.vertices.size() < 3)
        {
            return false;
        }
        for (std::size_t edge = 0; edge < surface.vertices.size(); ++edge)
        {
            const std::int32_t first = surface.vertices[edge];
            const std::int32_t second = surface.vertices[(edge + 1) % surface.vertices.size()];
            if (first < 0 || second < 0
                || static_cast<std::size_t>(first) >= sector.vertices.size()
                || static_cast<std::size_t>(second) >= sector.vertices.size())
            {
                return false;
            }
            auto& incidence = edges[{ std::min(first, second), std::max(first, second) }];
            ++incidence.first;
            incidence.second += first < second ? 1 : -1;
        }
        const auto origin = point(surface.vertices.front());
        for (std::size_t corner = 1; corner + 1 < surface.vertices.size(); ++corner)
        {
            const auto first = point(surface.vertices[corner]);
            const auto second = point(surface.vertices[corner + 1]);
            signedVolume += origin[0]
                    * (first[1] * second[2] - first[2] * second[1])
                + origin[1]
                    * (first[2] * second[0] - first[0] * second[2])
                + origin[2]
                    * (first[0] * second[1] - first[1] * second[0]);
        }
    }
    for (const auto& [edge, incidence] : edges)
    {
        (void)edge;
        if (incidence.first != 2 || incidence.second != 0)
        {
            return false;
        }
    }
    outVolume = std::abs(signedVolume) / 6.0L;
    return std::isfinite(static_cast<double>(outVolume)) && outVolume > 0.0L;
}

bool configureAndPlaceShape(MainWindow& window, const ShapeCase& shape)
{
    GlView* const view = window.glViewForTest();
    const bool controlsReady = view && setShapeControls(window, shape);
    if ( !controlsReady )
    {
        std::fprintf(stderr, "Shape placement stopped before pointer input: view=%d\n",
            view ? 1 : 0);
        return false;
    }
    const auto options = view->cleaveShapeOptions();
    const ui::viewport::CleaveShapeSpec spec{
        .kind = shape.kind,
        .width = shape.width,
        .height = shape.height,
        .dimensionsLinked = options.dimensionsLinked,
        .segments = shape.segments,
        .points = shape.points,
    };
    const std::optional<ui::viewport::CleaveShapePlan> plan =
        ui::viewport::makeCleaveShapePlan(spec);
    QToolButton* const gridButton = window.findChild<QToolButton*>(
        QStringLiteral("cleaveGridSnapButton"));
    QAction* const gridAction = window.findChild<QAction*>(
        QStringLiteral("viewport.transform.grid_snap"));
    if (!plan || !gridButton || !gridAction)
    {
        std::fprintf(
            stderr,
            "Shape plan/input controls unavailable: plan=%d gridButton=%d gridAction=%d\n",
            plan ? 1 : 0,
            gridButton ? 1 : 0,
            gridAction ? 1 : 0);
        return false;
    }
    if (!gridAction->isChecked())
    {
        gridAction->setChecked(true);
    }
    view->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents(QEventLoop::AllEvents);
    const std::optional<QPointF> center = view->projectWorldToViewport(
        QVector3D(0.0f, 1.0f, 0.5f));
    if (!center || !view->rect().contains(center->toPoint()))
    {
        std::fprintf(
            stderr,
            "Shape placement center did not project into GlView: center=%d viewRect=(%d,%d %dx%d)\n",
            center ? 1 : 0,
            view->rect().x(),
            view->rect().y(),
            view->rect().width(),
            view->rect().height());
        return false;
    }
    sendMouse(
        *view,
        QEvent::MouseMove,
        *center,
        Qt::NoButton,
        Qt::NoButton,
        Qt::NoModifier);
    QApplication::processEvents(QEventLoop::AllEvents);
    const std::optional<QVector3D> previewCenter =
        view->stagedCleaveShapePreviewCenterForTest();
    const bool previewReady = view->stagedCleaveShapePlacementActiveForTest()
        && previewCenter
        && (*previewCenter - QVector3D(0.0f, 1.0f, 0.5f)).length() < 1.0e-5f;
    const std::optional<QPointF> placementClick = view->projectWorldToViewport(
        QVector3D(0.0f, 1.0f, 0.5f));
    const bool placementClickValid = placementClick
        && view->rect().contains(placementClick->toPoint());
    if ( placementClickValid )
    {
        click(*view, *placementClick);
    }
    QApplication::processEvents(QEventLoop::AllEvents);
    const GlView::RecoveryState placedDraft = view->recoveryState();
    const bool placed = previewReady && placementClickValid
        && view->stagedCleaveShapePlacementActiveForTest()
        && expectedShapePlacement(placedDraft, *plan);
    if (!placed)
    {
        const auto options = view->cleaveShapeOptions();
        const auto perimeterBounds = [](const ui::viewport::CleaveShapePlan* value)
        {
            std::array<double, 4> bounds{
                std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::infinity(),
                -std::numeric_limits<double>::infinity(),
                -std::numeric_limits<double>::infinity() };
            if ( value )
            {
                for ( const Point2& point : value->perimeter )
                {
                    bounds[0] = std::min(bounds[0], point.x);
                    bounds[1] = std::min(bounds[1], point.y);
                    bounds[2] = std::max(bounds[2], point.x);
                    bounds[3] = std::max(bounds[3], point.y);
                }
            }
            return bounds;
        };
        const auto expectedBounds = perimeterBounds(&*plan);
        const auto& cachedPlan = view->stagedCleaveShapePlacementPlanForTest();
        const auto cachedBounds = perimeterBounds(
            cachedPlan ? &*cachedPlan : nullptr);
        const bool hasPreview = previewCenter.has_value();
        const QVector3D centerValue = previewCenter.value_or(QVector3D{});
        std::fprintf(
            stderr,
            "Shape placement failed: kind=%d requested=(%.9g,%.9g) options=(%.9g,%.9g linked=%d segments=%d points=%d) helperBounds=(%.9g,%.9g)-(%.9g,%.9g) helperSize=(%.9g,%.9g) cached=%d cachedBounds=(%.9g,%.9g)-(%.9g,%.9g) cachedSize=(%.9g,%.9g) armedOption=%d addButton=%d previewActive=%d previewReady=%d preview=%d(%.9g,%.9g,%.9g) center=%d(%.9g,%.9g) click=%d(%.9g,%.9g) lines=%zu expectedStrokes=%zu tool=(sector%d,surface%d) activeSector=%d context=%d surface=%d source=(%d,%d) targetSectors=%zu targets=%zu selection=(%d,%d) grid=%d\n",
            static_cast<int>(shape.kind),
            shape.width,
            shape.height,
            options.width,
            options.height,
            options.dimensionsLinked ? 1 : 0,
            options.segments,
            options.points,
            expectedBounds[0],
            expectedBounds[1],
            expectedBounds[2],
            expectedBounds[3],
            expectedBounds[2] - expectedBounds[0],
            expectedBounds[3] - expectedBounds[1],
            cachedPlan ? 1 : 0,
            cachedBounds[0],
            cachedBounds[1],
            cachedBounds[2],
            cachedBounds[3],
            cachedBounds[2] - cachedBounds[0],
            cachedBounds[3] - cachedBounds[1],
            options.placementArmed ? 1 : 0,
            window.findChild<QToolButton*>(QStringLiteral("cleaveAddShapeButton"))
                    && window.findChild<QToolButton*>(QStringLiteral("cleaveAddShapeButton"))->isChecked()
                ? 1 : 0,
            view->stagedCleaveShapePlacementActiveForTest() ? 1 : 0,
            previewReady ? 1 : 0,
            hasPreview ? 1 : 0,
            centerValue.x(),
            centerValue.y(),
            centerValue.z(),
            center ? 1 : 0,
            center ? center->x() : 0.0,
            center ? center->y() : 0.0,
            placementClickValid ? 1 : 0,
            placementClick ? placementClick->x() : 0.0,
            placementClick ? placementClick->y() : 0.0,
            placedDraft.cleaveLines.size(),
            plan->perimeter.size() + plan->decompositionStrokes.size(),
            view->sectorCleaveToolEnabled() ? 1 : 0,
            view->surfaceCleaveToolEnabled() ? 1 : 0,
            view->activeSectorIndex(),
            placedDraft.cleaveContext.valid ? 1 : 0,
            placedDraft.cleaveContext.surfaceMode ? 1 : 0,
            placedDraft.cleaveContext.sourceSurface.sector,
            placedDraft.cleaveContext.sourceSurface.surface,
            placedDraft.cleaveContext.targetSectors.size(),
            placedDraft.cleaveContext.targetSurfaces.size(),
            view->activeSurfaceSectorIndex(),
            view->activeSurfaceIndex(),
            gridAction->isChecked() ? 1 : 0);
    }
    return placed;
}

void runSurfaceShapeCase(const ShapeCase& shape)
{
    MainWindow window{};
    check(configureSurfaceWindow(window),
        "shape Apply fixture selects the source wall and arms the actual Surface Multi-cut tool");
    if (!window.glViewForTest()
        || !window.glViewForTest()->surfaceCleaveToolEnabled())
    {
        return;
    }
    GlView* const view = window.glViewForTest();
    QAction* const applyAction = window.findChild<QAction*>(
        QStringLiteral("viewport.surface.cleave"));
    QToolButton* const applyButton = window.findChild<QToolButton*>(
        QStringLiteral("cleaveApplyButton"));
    check(applyAction && applyButton,
        "shape Apply fixture exposes the real Apply action and button");
    if (!applyAction || !applyButton)
    {
        return;
    }

    const ShapeCase testShape = shape;
    const ui::viewport::CleaveShapeSpec spec{
        .kind = testShape.kind,
        .width = testShape.width,
        .height = testShape.height,
        .dimensionsLinked = testShape.dimensionsLinked,
        .segments = testShape.segments,
        .points = testShape.points,
    };
    const std::optional<ui::viewport::CleaveShapePlan> plan =
        ui::viewport::makeCleaveShapePlan(spec);
    const std::size_t linesBefore = view->stagedCleaveLineCount();
    const std::size_t undoBefore = window.undoDepthForTest();
    const std::vector<std::byte> levelBefore =
        assets::saveToSedBytes(window.documentStateForTest().level);
    const bool placementWorked = configureAndPlaceShape(window, testShape);
    const std::string label = "Surface Add Shape ";
    const std::string name = label + testShape.name;
    check(plan.has_value() && placementWorked,
        (name + " consumes the real popover settings and viewport placement click")
            .c_str());
    if (!plan || !placementWorked)
    {
        return;
    }
    const std::size_t expectedStrokeCount = plan->decompositionStrokes.size()
        + plan->perimeter.size();
    check(view->stagedCleaveLineCount() == linesBefore + expectedStrokeCount
            && window.undoDepthForTest() == undoBefore
            && assets::saveToSedBytes(window.documentStateForTest().level) == levelBefore,
        (name + " appends ordinary ordered cut strokes without a document history edit")
            .c_str());

    sendKey(*view, QEvent::KeyPress, Qt::Key_Escape);
    sendKey(*view, QEvent::KeyRelease, Qt::Key_Escape);
    QApplication::processEvents(QEventLoop::AllEvents);
    const bool escapePreserved = !view->cleaveShapeOptions().placementArmed
        && view->stagedCleaveLineCount() == expectedStrokeCount;
    if ( !escapePreserved )
    {
        QAction* const ownerAction = window.findChild<QAction*>(
            QStringLiteral("viewport.surface.cleave"));
        const GlView::RecoveryState afterEscape = view->recoveryState();
        std::fprintf(
            stderr,
            "Shape Escape state: armed=%d plan=%d lines=%zu expected=%zu gestureUndo=%d localOwns=%d surfaceTool=%d sectorTool=%d ownerAction=%d bounded=%d context=%d surfaceContext=%d targetSurfaces=%zu\n",
            view->cleaveShapeOptions().placementArmed ? 1 : 0,
            view->stagedCleaveShapePlacementActiveForTest() ? 1 : 0,
            view->stagedCleaveLineCount(),
            expectedStrokeCount,
            view->canUndoStagedCleaveHistory() ? 1 : 0,
            view->stagedCleaveHistoryOwnsUndo() ? 1 : 0,
            view->surfaceCleaveToolEnabled() ? 1 : 0,
            view->sectorCleaveToolEnabled() ? 1 : 0,
            ownerAction && ownerAction->isChecked() ? 1 : 0,
            view->cleaveBounded() ? 1 : 0,
            afterEscape.cleaveContext.valid ? 1 : 0,
            afterEscape.cleaveContext.surfaceMode ? 1 : 0,
            afterEscape.cleaveContext.targetSurfaces.size());
    }
    check(!view->cleaveShapeOptions().placementArmed
            && view->stagedCleaveLineCount() == expectedStrokeCount,
        (name + " exits placement with Escape while preserving its authored strokes")
            .c_str());

    applyButton->click();
    QApplication::processEvents(QEventLoop::AllEvents);
    const Level& applied = window.documentStateForTest().level;
    const std::vector<SurfaceRef> interior = validateSurfacePartition(applied, *plan);
    check(window.undoDepthForTest() == undoBefore + 1
            && view->stagedCleaveLineCount() == 0
            && assets::saveToSedBytes(applied) != levelBefore,
        (name + " applies as one real bounded Level transaction")
            .c_str());
    check(!interior.empty(),
        (name + " produces a non-overlapping convex partition covering its analytic footprint")
            .c_str());
    if (interior.empty())
    {
        return;
    }

    QAction* const extrudeAction = window.findChild<QAction*>(
        QStringLiteral("surface.extrude"));
    ui::ScrubFloatEdit* const length = window.findChild<ui::ScrubFloatEdit*>(
        QStringLiteral("surfaceExtrudeLengthSpin"));
    ui::ScrubFloatEdit* const expand = window.findChild<ui::ScrubFloatEdit*>(
        QStringLiteral("surfaceExtrudeExpandSpin"));
    QToolButton* const extrudeApply = window.findChild<QToolButton*>(
        QStringLiteral("surfaceExtrudeApplyButton"));
    QAction* const sharedGridSnap = window.findChild<QAction*>(
        QStringLiteral("viewport.transform.grid_snap"));
    check(extrudeAction && length && expand && extrudeApply && sharedGridSnap,
        (name + " exposes the production one-face Extrude controls")
            .c_str());
    if (!extrudeAction || !length || !expand || !extrudeApply || !sharedGridSnap)
    {
        return;
    }
    sharedGridSnap->setChecked(false);
    std::size_t expectedSectorCount = applied.sectors.size();
    for (const SurfaceRef ref : interior)
    {
        editor::Selection selection{};
        selection.surfaces.insert(ref);
        window.mutableDocumentStateForTest().selection = selection;
        view->setSelectionState(
            ref.sector,
            ref.surface,
            -1,
            -1,
            selection);
        window.updateEditActionsForTest();
        const std::size_t undoBeforeExtrude = window.undoDepthForTest();
        extrudeAction->trigger();
        QApplication::processEvents(QEventLoop::AllEvents);
        check(extrudeApply->isEnabled()
                && window.documentStateForTest().level.sectors.size()
            == expectedSectorCount,
            (name + " prepares an actual one-face extrusion preview without publication")
                .c_str());
        length->lineEdit()->setFocus(Qt::OtherFocusReason);
        length->lineEdit()->selectAll();
        length->lineEdit()->setText(QStringLiteral("0.25"));
        sendKey(*length->lineEdit(), QEvent::KeyPress, Qt::Key_Return);
        sendKey(*length->lineEdit(), QEvent::KeyRelease, Qt::Key_Return);
        expand->setValue(0.0);
        QApplication::processEvents(QEventLoop::AllEvents);
        check(std::abs(length->value() - 0.25) < 1.0e-9,
            (name + " commits the actual extrusion preview length as 0.25").c_str());
        extrudeApply->click();
        QApplication::processEvents(QEventLoop::AllEvents);
        const Level& extrudedLevel = window.documentStateForTest().level;
        const bool appendedOneSector = extrudedLevel.sectors.size()
            == expectedSectorCount + 1;
        bool validSolid = appendedOneSector;
        long double volume = 0.0L;
        if (appendedOneSector)
        {
            const Sector& solid = extrudedLevel.sectors.back();
            validSolid = level::sectorIsConvex(solid)
                && closedVolume(solid, volume)
                && std::abs(volume - 0.25L * std::abs(signedArea(
                    sourceWallPolygon(
                        applied.sectors[static_cast<std::size_t>(ref.sector)]
                            .surfaces[static_cast<std::size_t>(ref.surface)],
                        applied.sectors[static_cast<std::size_t>(ref.sector)]))))
                    < 1.0e-5L;
            const Surface& base = solid.surfaces.front();
            validSolid = validSolid && base.adjoin.has_value()
                && base.adjoin->sector == ref.sector
                && base.adjoin->surface == ref.surface;
            const Surface& source = extrudedLevel.sectors[
                static_cast<std::size_t>(ref.sector)].surfaces[
                    static_cast<std::size_t>(ref.surface)];
            validSolid = validSolid && source.adjoin.has_value()
                && source.adjoin->sector
                    == static_cast<std::int32_t>(expectedSectorCount)
                && source.adjoin->surface == 0;
        }
        check(appendedOneSector && validSolid
                && window.undoDepthForTest() == undoBeforeExtrude + 1,
            (name + " extrudes each convex piece as one closed positive-volume Sector with a reciprocal base portal")
                .c_str());
        if (!appendedOneSector)
        {
            return;
        }
        ++expectedSectorCount;
    }
}

void runRankOrderApplyCase(const bool reorder)
{
    MainWindow window{};
    check(configureSurfaceWindow(window),
        "rank-order Apply fixture selects and arms the real Surface Cleave target");
    GlView* const view = window.glViewForTest();
    if (!view || !view->surfaceCleaveToolEnabled())
    {
        return;
    }
    QToolButton* const numbers = window.findChild<QToolButton*>(
        QStringLiteral("cleaveShowNumbersButton"));
    QToolButton* const bounded = window.findChild<QToolButton*>(
        QStringLiteral("cleaveBoundedButton"));
    QToolButton* const apply = window.findChild<QToolButton*>(
        QStringLiteral("cleaveApplyButton"));
    check(numbers && bounded && apply,
        "rank-order fixture exposes line-number, Bounded, and Apply controls");
    if (!numbers || !bounded || !apply)
    {
        return;
    }
    numbers->setChecked(false);
    bounded->setChecked(true);
    const auto project = [view](const double x, const double z)
    {
        return view->projectWorldToViewport(QVector3D(
            static_cast<float>(x), 1.0f, static_cast<float>(z)));
    };
    const std::optional<QPointF> verticalFirst = project(0.0, 0.1);
    const std::optional<QPointF> verticalSecond = project(0.0, 0.9);
    const std::optional<QPointF> horizontalFirst = project(0.25, 0.5);
    const std::optional<QPointF> horizontalSecond = project(0.9, 0.5);
    if (!verticalFirst || !verticalSecond || !horizontalFirst || !horizontalSecond)
    {
        check(false, "rank-order Apply fixture projects both real cut strokes onto the wall");
        return;
    }
    drag(*view, *verticalFirst, *verticalSecond);
    drag(*view, *horizontalFirst, *horizontalSecond);
    const GlView::RecoveryState beforeOrder = view->recoveryState();
    const bool authoredTwoStrokes = beforeOrder.cleaveLines.size() == 2;
    const bool identityOrder = beforeOrder.applyOrder
        == std::vector<std::int32_t>{ 0, 1 };
    const bool ordinaryFiniteStrokes = authoredTwoStrokes
        && std::all_of(
            beforeOrder.cleaveLines.begin(),
            beforeOrder.cleaveLines.end(),
            [](const GlView::RecoveryState::CleaveLine& line)
            {
                return !line.linkedToPrevious && !line.linkedToNext;
            });
    const bool exactSurfaceTarget = view->activeSurfaceSectorIndex() == 0
        && view->activeSurfaceIndex() == 5
        && view->stagedCleaveTargetsMatchCurrentForTest(true);
    check(exactSurfaceTarget,
        "rank-order Apply fixture retains source Surface (0,5) through both viewport drags");
    check(authoredTwoStrokes,
        "rank-order Apply fixture authors exactly two strokes through actual viewport drags");
    check(identityOrder,
        "rank-order Apply fixture starts in physical execution order [0,1]");
    check(ordinaryFiniteStrokes,
        "rank-order Apply fixture authors two ordinary unlinked bounded strokes");
    if (!authoredTwoStrokes || !identityOrder || !ordinaryFiniteStrokes || !exactSurfaceTarget)
    {
        return;
    }

    if (reorder)
    {
        numbers->setChecked(true);
        QApplication::processEvents(QEventLoop::AllEvents);
        view->setFocus(Qt::OtherFocusReason);
        QApplication::processEvents(QEventLoop::AllEvents);
        const QRect label = view->stagedCleaveOrderLabelRectForTest(1);
        check(!label.isEmpty(),
            "real Apply-order line label exposes its shared paint and hit rectangle");
        if (label.isEmpty())
        {
            return;
        }
        const bool viewportHadFocusBeforeClick =
            QApplication::focusWidget() == view;
        const bool placementWasDisarmed =
            !view->cleaveShapeOptions().placementArmed;
        const QPointF labelPoint(label.center());
        view->setProperty("testCleavePressRouteDiagnostics", true);
        sendMouse(
            *view,
            QEvent::MouseButtonPress,
            labelPoint,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QLineEdit* const editorAfterPress = view->findChild<QLineEdit*>(
            QStringLiteral("cleaveLineOrderEditor"));
        const bool editorVisibleAfterPress = editorAfterPress
            && editorAfterPress->isVisible();
        const bool editorFocusedAfterPress = editorAfterPress
            && QApplication::focusWidget() == editorAfterPress;
        sendMouse(
            *view,
            QEvent::MouseButtonRelease,
            labelPoint,
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        view->setProperty("testCleavePressRouteDiagnostics", false);
        QLineEdit* const editorAfterRelease = view->findChild<QLineEdit*>(
            QStringLiteral("cleaveLineOrderEditor"));
        const bool editorVisibleAfterRelease = editorAfterRelease
            && editorAfterRelease->isVisible();
        const bool editorFocusedAfterRelease = editorAfterRelease
            && QApplication::focusWidget() == editorAfterRelease;
        QApplication::processEvents(QEventLoop::AllEvents);
        QLineEdit* const editor = view->findChild<QLineEdit*>(
            QStringLiteral("cleaveLineOrderEditor"));
        const bool editorReceivedNativeFocus = editor
            && QApplication::focusWidget() == editor;
        if ( !viewportHadFocusBeforeClick || !editorReceivedNativeFocus )
        {
            QWidget* const focusedWidget = QApplication::focusWidget();
            QWidget* const activeWindow = QApplication::activeWindow();
            QWidget* const windowFocusedWidget = window.focusWidget();
            const QByteArray focusClass = focusedWidget
                ? QByteArray(focusedWidget->metaObject()->className())
                : QByteArrayLiteral("<none>");
            const QByteArray focusName = focusedWidget
                ? focusedWidget->objectName().toUtf8()
                : QByteArrayLiteral("<none>");
            const QByteArray activeClass = activeWindow
                ? QByteArray(activeWindow->metaObject()->className())
                : QByteArrayLiteral("<none>");
            const QByteArray activeName = activeWindow
                ? activeWindow->objectName().toUtf8()
                : QByteArrayLiteral("<none>");
            const QByteArray windowFocusName = windowFocusedWidget
                ? windowFocusedWidget->objectName().toUtf8()
                : QByteArrayLiteral("<none>");
            std::fprintf(
                stderr,
                "Rank label focus failed: viewportBefore=%d placementDisarmed=%d pressEditor=%d pressVisible=%d pressFocus=%d releaseEditor=%d releaseVisible=%d releaseFocus=%d editor=%d editorVisible=%d editorFocus=%d appFocusClass=%s appFocusName=%s windowFocusName=%s activeClass=%s activeName=%s label=(%d,%d %dx%d) click=(%d,%d) otherLabel=(%d,%d %dx%d)\n",
                viewportHadFocusBeforeClick ? 1 : 0,
                placementWasDisarmed ? 1 : 0,
                editorAfterPress ? 1 : 0,
                editorVisibleAfterPress ? 1 : 0,
                editorFocusedAfterPress ? 1 : 0,
                editorAfterRelease ? 1 : 0,
                editorVisibleAfterRelease ? 1 : 0,
                editorFocusedAfterRelease ? 1 : 0,
                editor ? 1 : 0,
                editor && editor->isVisible() ? 1 : 0,
                editorReceivedNativeFocus ? 1 : 0,
                focusClass.constData(),
                focusName.constData(),
                windowFocusName.constData(),
                activeClass.constData(),
                activeName.constData(),
                label.x(),
                label.y(),
                label.width(),
                label.height(),
                label.center().x(),
                label.center().y(),
                view->stagedCleaveOrderLabelRectForTest(0).x(),
                view->stagedCleaveOrderLabelRectForTest(0).y(),
                view->stagedCleaveOrderLabelRectForTest(0).width(),
                view->stagedCleaveOrderLabelRectForTest(0).height());
        }
        check(viewportHadFocusBeforeClick && editorReceivedNativeFocus,
            "clicking the displayed execution rank opens its native focused editor");
        if (!editor)
        {
            return;
        }
        sendKey(*editor, QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
        sendKey(*editor, QEvent::KeyRelease, Qt::Key_A, Qt::ControlModifier);
        sendKey(*editor, QEvent::KeyPress, Qt::Key_1, Qt::NoModifier, QStringLiteral("1"));
        sendKey(*editor, QEvent::KeyRelease, Qt::Key_1, Qt::NoModifier);
        sendKey(*editor, QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        sendKey(*editor, QEvent::KeyRelease, Qt::Key_Return, Qt::NoModifier);
        QApplication::processEvents(QEventLoop::AllEvents);
        const GlView::RecoveryState reordered = view->recoveryState();
        check(reordered.applyOrder == std::vector<std::int32_t>{ 1, 0 }
                && sameStagedLines(beforeOrder.cleaveLines, reordered.cleaveLines),
            "editing line 2 to rank 1 changes only the execution permutation");
        const bool rankUndo = view->undoStagedCleaveHistory();
        const bool rankUndoRestored = rankUndo
            && view->recoveryState().applyOrder == std::vector<std::int32_t>{ 0, 1 }
            && sameStagedLines(beforeOrder.cleaveLines, view->recoveryState().cleaveLines);
        const bool rankRedo = rankUndoRestored && view->redoStagedCleaveHistory();
        check(rankUndoRestored && rankRedo
                && view->recoveryState().applyOrder
                    == std::vector<std::int32_t>{ 1, 0 },
            "one local Undo and Redo restore the original and reordered rank without changing physical lines");
    }

    apply->click();
    QApplication::processEvents(QEventLoop::AllEvents);
    std::size_t wallPieces = 0;
    double wallArea = 0.0;
    std::vector<Polygon2> wallPolygons{};
    for (const Sector& sector : window.documentStateForTest().level.sectors)
    {
        for (const Surface& surface : sector.surfaces)
        {
            if (surface.vertices.size() < 3)
            {
                continue;
            }
            const Polygon2 polygon = sourceWallPolygon(surface, sector);
            const bool onWall = std::all_of(
                surface.vertices.begin(),
                surface.vertices.end(),
                [&sector](const std::int32_t vertex)
                {
                    return vertex >= 0
                        && static_cast<std::size_t>(vertex) < sector.vertices.size()
                        && std::abs(sector.vertices[static_cast<std::size_t>(vertex)].y - 1.0)
                            < 1.0e-7;
                });
            if (onWall)
            {
                ++wallPieces;
                wallArea += std::abs(signedArea(polygon));
                wallPolygons.push_back(polygon);
            }
        }
    }
    bool noOverlap = true;
    for (std::size_t first = 0; first < wallPolygons.size(); ++first)
    {
        for (std::size_t second = first + 1; second < wallPolygons.size(); ++second)
        {
            noOverlap = noOverlap
                && !polygonsHavePositiveAreaOverlap(
                    wallPolygons[first],
                    wallPolygons[second]);
        }
    }
    const std::size_t expected = reorder ? 4u : 3u;
    check(wallPieces == expected && std::abs(wallArea - 2.0) < 1.0e-6
            && noOverlap,
        reorder
            ? "real Apply consumes [1,0] and partitions the wall into four area-complete pieces"
            : "real Apply consumes [0,1] and partitions the wall into three area-complete pieces");
}

[[maybe_unused]] void runSectorShapeCase(const ShapeCase& shape)
{
    MainWindow window{};
    check(configureSectorWindow(window),
        "Sector shape Apply fixture selects the source Sector and arms the actual Cleave tool");
    GlView* const view = window.glViewForTest();
    QToolButton* const apply = window.findChild<QToolButton*>(
        QStringLiteral("cleaveApplyButton"));
    if (!view || !view->sectorCleaveToolEnabled() || !apply)
    {
        return;
    }
    const ui::viewport::CleaveShapeSpec spec{
        .kind = shape.kind,
        .width = shape.width,
        .height = shape.height,
        .dimensionsLinked = shape.dimensionsLinked,
        .segments = shape.segments,
        .points = shape.points,
    };
    const std::optional<ui::viewport::CleaveShapePlan> plan =
        ui::viewport::makeCleaveShapePlan(spec);
    const std::size_t undoBefore = window.undoDepthForTest();
    const std::size_t sectorsBefore = window.documentStateForTest().level.sectors.size();
    const std::vector<std::byte> levelBefore =
        assets::saveToSedBytes(window.documentStateForTest().level);
    const bool placed = configureAndPlaceShape(window, shape);
    const std::string name = std::string("Sector Add Shape ") + shape.name;
    check(plan.has_value() && placed,
        (name + " uses the real shape controls and viewport placement click")
            .c_str());
    if (!plan || !placed)
    {
        return;
    }
    sendKey(*view, QEvent::KeyPress, Qt::Key_Escape);
    sendKey(*view, QEvent::KeyRelease, Qt::Key_Escape);
    apply->click();
    QApplication::processEvents(QEventLoop::AllEvents);
    const Level& applied = window.documentStateForTest().level;
    check(window.undoDepthForTest() == undoBefore + 1
            && applied.sectors.size() > sectorsBefore
            && assets::saveToSedBytes(applied) != levelBefore
            && view->stagedCleaveLineCount() == 0,
        (name + " uses the separate real Sector batch Apply route in one transaction")
            .c_str());
    if (applied.sectors.size() <= sectorsBefore)
    {
        return;
    }

    const std::vector<SurfaceRef> interior = validateSurfacePartition(applied, *plan);
    check(!interior.empty(),
        (name + " preserves a complete, non-overlapping convex source-wall footprint")
            .c_str());

    bool validSectors = true;
    long double totalVolume = 0.0L;
    for (const Sector& sector : applied.sectors)
    {
        long double volume = 0.0L;
        validSectors = validSectors
            && level::sectorIsConvex(sector)
            && closedVolume(sector, volume);
        totalVolume += volume;
        for (std::size_t surfaceIndex = 0;
            surfaceIndex < sector.surfaces.size();
            ++surfaceIndex)
        {
            const Surface& surface = sector.surfaces[surfaceIndex];
            if (!surface.adjoin)
            {
                continue;
            }
            const SurfaceRef& adjoin = *surface.adjoin;
            if (adjoin.sector < 0
                || static_cast<std::size_t>(adjoin.sector) >= applied.sectors.size())
            {
                validSectors = false;
                continue;
            }
            const Sector& neighbor = applied.sectors[
                static_cast<std::size_t>(adjoin.sector)];
            if (adjoin.surface < 0
                || static_cast<std::size_t>(adjoin.surface) >= neighbor.surfaces.size())
            {
                validSectors = false;
                continue;
            }
            const Surface& reciprocal = neighbor.surfaces[
                static_cast<std::size_t>(adjoin.surface)];
            validSectors = validSectors
                && reciprocal.adjoin.has_value()
                && reciprocal.adjoin->sector
                    == static_cast<std::int32_t>(&sector - applied.sectors.data())
                && reciprocal.adjoin->surface == static_cast<std::int32_t>(surfaceIndex);
        }
    }
    check(validSectors && std::abs(totalVolume - 4.0L) < 1.0e-6L,
        (name + " emits convex closed sectors with positive volumes, conserved volume, and reciprocal cut portals")
            .c_str());
}

} // namespace

void runCleaveShapeApplyTests()
{
    const std::array<ShapeCase, 7> surfaceCases{{
        { ui::viewport::CleaveShapeKind::Rectangle, 0.6, 0.6, true, 16, 5, "square from linked Rectangle" },
        { ui::viewport::CleaveShapeKind::Rectangle, 0.8, 0.4, false, 16, 5, "rectangle" },
        { ui::viewport::CleaveShapeKind::RegularPolygon, 0.6, 0.6, true, 7, 5, "seven-sided ngon" },
        { ui::viewport::CleaveShapeKind::Arch, 0.8, 0.6, false, 8, 5, "filled arch" },
        { ui::viewport::CleaveShapeKind::Star, 0.8, 0.6, false, 16, 3, "three-point Star" },
        { ui::viewport::CleaveShapeKind::Star, 0.8, 0.6, false, 16, 5, "five-point Star" },
        { ui::viewport::CleaveShapeKind::Star, 0.8, 0.6, false, 16, 6, "six-point Star" },
    }};
    for (const ShapeCase& shape : surfaceCases)
    {
        runSurfaceShapeCase(shape);
    }
    runRankOrderApplyCase(false);
    runRankOrderApplyCase(true);
    runSectorShapeCase(surfaceCases[0]);
    runSectorShapeCase(surfaceCases[3]);
    runSectorShapeCase(surfaceCases[4]);
}

} // namespace sed::app_tests
