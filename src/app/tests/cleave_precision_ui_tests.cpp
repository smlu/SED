#include "test_common.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level/build.hpp"
#include "sed/level/transform.hpp"
#include "sed/assets/sed.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/sector/sector_wire_utils.hpp"

#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QAction>
#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QLineEdit>
#include <QToolButton>

#include <array>
#include <cmath>
#include <limits>

namespace sed::app_tests
{
namespace
{

template <typename Point>
double xCoordinate(const Point& point)
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

void runCleavePrecisionUiTests()
{
    // Failed preview admission must report through assertions rather than
    // leave the automated regression waiting for a warning dialog.
    QTimer dismissWarnings;
    dismissWarnings.setInterval(10);
    QObject::connect(&dismissWarnings, &QTimer::timeout, []
    {
        if (auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            warning->accept();
    });
    dismissWarnings.start();
    Level recoveryLevel{};
    {
        GlView view{};
        view.setAttribute(Qt::WA_DontShowOnScreen);
        const QVector3D nearbyEndpoint(0.000001f, 0.0f, 0.0f);
        view.addStagedCleaveLineForTest(QVector3D{}, QVector3D(1.0f, 0.0f, 0.0f), QVector3D(0.0f, 0.0f, 1.0f));
        view.addStagedCleaveLineForTest(nearbyEndpoint, QVector3D(1.0f, 1.0f, 0.0f), QVector3D(0.0f, 0.0f, 1.0f));
        check(view.reshapeStagedCleaveEndpointForTest(0, 0, QVector3D(0.0f, 0.5f, 0.0f)),
            "cleave precision: independent endpoint drag succeeds");
        check(view.stagedCleavePlanePointForTest(1) == nearbyEndpoint,
            "cleave precision: distinct nearby endpoint must not move as a shared handle");
    }
    for ( const double offset : {0.0, 10000.0, 1000000.0} )
    {
        Level level{};
        auto& sector = level.newSector();
        for ( const auto point : std::array<TVector, 8>{{
                {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
                {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}}} )
        {
            sector.newVertex(offset + point.x * 0.01, offset + point.y * 0.01, offset + point.z * 0.01);
        }
        for ( const auto indices : std::array<std::array<std::int32_t, 4>, 6>{{
                {1, 5, 6, 2}, {0, 3, 7, 4}, {3, 2, 6, 7},
                {0, 4, 5, 1}, {4, 7, 6, 5}, {0, 1, 2, 3}}} )
        {
            Surface face{};
            face.vertices.assign(indices.begin(), indices.end());
            face.texVertices.resize(4);
            face.normal = level::surfaceCalcNormalNewell(sector, face);
            sector.addSurface(std::move(face));
        }
        const double expectedWidth = sector.vertices[1].x - sector.vertices[0].x;
        GlView view{};
        view.setAttribute(Qt::WA_DontShowOnScreen);
        view.resize(640, 480);
        view.setLevel(&level);
        view.setSelectedSector(0);
        view.captureStagedCleaveContextForTest(false);
        view.addStagedCleaveLineForTest(
            QVector3D(static_cast<float>(offset - 1.0), static_cast<float>(offset), static_cast<float>(offset)),
            QVector3D(static_cast<float>(offset + 1.0), static_cast<float>(offset), static_cast<float>(offset)),
            QVector3D(0.0f, 0.0f, 1.0f));
        const auto polygons = view.stagedCleavePlanePolygonsForTest(0);
        check(polygons.size() == 1 && polygons[0].size() == 4,
            "cleave precision: distant small cube produces complete visual plane polygon");
        if ( polygons.size() != 1 || polygons[0].empty() )
        {
            continue;
        }
        double minimum = std::numeric_limits<double>::infinity();
        double maximum = -minimum;
        for ( const auto& point : polygons[0] )
        {
            minimum = std::min(minimum, xCoordinate(point));
            maximum = std::max(maximum, xCoordinate(point));
        }
        check(std::abs((maximum - minimum) - expectedWidth) < 1.0e-10,
            "cleave precision: visual cut preserves exact document intersection extent");

        view.cancelStagedCleave();
        view.setSectorCleaveToolEnabled(true);
        view.setCleaveStagedMode(true);
        view.captureStagedCleaveContextForTest(false);
        const TVector exactFirst{offset + 0.0013, offset + 0.0027, offset + 0.0009};
        const TVector exactSecond{offset + 0.0049, exactFirst.y, exactFirst.z};
        view.addStagedCleaveDocumentLineForTest(exactFirst, exactSecond, {0, 0, 1});
        int requests = 0;
        QObject::connect(&view, &GlView::sectorsCleaveBatchRequested, &view,
            [&](const QList<TVector>& normals, const QList<TVector>& points, bool closed, const TVector& interior)
            {
                ++requests;
                check(!closed && normals.size() == 1 && points.size() == 1,
                    "cleave precision: single authored plane produces one batch payload");
                if ( points.size() == 1 )
                {
                    check(points[0].x == exactFirst.x && points[0].y == exactFirst.y && points[0].z == exactFirst.z
                            && interior.x == exactFirst.x && interior.y == exactFirst.y && interior.z == exactFirst.z,
                        "cleave precision: authoritative commit payload retains exact document plane point");
                }
            });
        view.requestApplyStagedCleave();
        check(requests == 1, "cleave precision: Apply emits the double plane payload");
        view.cancelStagedCleave();
        view.requestApplyStagedCleave();
        check(requests == 1 && view.stagedCleaveLineCount() == 0,
            "cleave precision: Cancel removes the authoritative plane and prevents a later commit");
        if ( offset == 1000000.0 )
        {
            recoveryLevel = level;
        }
    }

    MainWindow window{};
    window.setSelectedSectorShapeForTest("Cube");
    window.setAttribute(Qt::WA_DontShowOnScreen);
    const auto typeNumber = [](ui::ScrubFloatEdit* edit, const double value, const bool finish = true)
    {
        const QString text = QString::number(value, 'g', 17);
        edit->lineEdit()->setText(text);
        QMetaObject::invokeMethod(edit->lineEdit(), "textEdited", Qt::DirectConnection, Q_ARG(QString, text));
        if (finish) QMetaObject::invokeMethod(edit->lineEdit(), "editingFinished", Qt::DirectConnection);
    };
    if (auto* angle = window.findChild<ui::ScrubFloatEdit*>("cleaveAngleStepSpin"))
    {
        const auto depth = window.undoDepthForTest();
        for (const double step : {22.5, std::nextafter(22.5, 23.0), 1.2e-7, 1.0e-10, 0.0})
        {
            typeNumber(angle, step);
            check(angle->value() == step && window.glViewForTest()->cleaveAngleStepDegrees() == step,
                "Cleave angle field authors exact typed steps including nextafter, tiny and Free values");
        }
        check(window.undoDepthForTest() == depth, "Cleave angle preferences do not publish document history");
    }
    else check(false, "Cleave precision fixture exposes the exact angle field");

    for (const double expand : {0.0, -0.0001})
    {
        editor::DocumentState extrusionState{};
        extrusionState.level.layers = {"Layer0"};
        Sector& cube = level::levelCreateCube(extrusionState.level, 0, 0, 0, TVector::up, TVector::right);
        cube.layer = 0;
        for (Vertex& vertex : cube.vertices)
        {
            vertex.x *= 0.001; vertex.y *= 0.001; vertex.z *= 0.001;
        }
        level::levelRecalcSector(extrusionState.level, 0);
        const TVector normal = cube.surfaces[0].normal;
        const Vertex planeVertex = cube.vertices[static_cast<std::size_t>(cube.surfaces[0].vertices[0])];
        const TVector planePoint{planeVertex.x, planeVertex.y, planeVertex.z};
        extrusionState.selection.surfaces.insert({0,0});
        window.setDocumentStateForTest(std::move(extrusionState));
        window.activateSurfaceEditModeForTest();
        window.mutableDocumentStateForTest().selection.surfaces = {{0,0}};
        window.updateEditActionsForTest();
        auto* lengthEdit = window.findChild<ui::ScrubFloatEdit*>("surfaceExtrudeLengthSpin");
        auto* expandEdit = window.findChild<ui::ScrubFloatEdit*>("surfaceExtrudeExpandSpin");
        auto* snap = window.findChild<QToolButton*>("surfaceExtrudeGridSnapButton");
        auto* action = window.findChild<QAction*>("surface.extrude");
        auto* apply = window.findChild<QToolButton*>("surfaceExtrudeApplyButton");
        check(lengthEdit && expandEdit && snap && action && apply && action->isEnabled(),
            "Extrude precision fixture exposes enabled exact editing controls");
        if (!lengthEdit || !expandEdit || !snap || !action || !apply || !action->isEnabled()) continue;
        snap->setChecked(false);
        action->trigger();
        const double length = expand == 0.0 ? 1.2e-7 : 0.002;
        typeNumber(lengthEdit, length * 2.0);
        typeNumber(expandEdit, expand);
        typeNumber(lengthEdit, length, false);
        check(lengthEdit->value() == length && expandEdit->value() == expand,
            "Extrude typed tiny length and valid sub-millimeter inset retain exact authored values");
        const auto before = assets::saveToSedBytes(window.documentStateForTest().level);
        const auto depth = window.undoDepthForTest();
        apply->click();
        const auto& output = window.documentStateForTest().level;
        check(output.sectors.size() == 2 && window.undoDepthForTest() == depth + 1,
            "Extrude exact controls publish one geometry command");
        if (output.sectors.size() == 2)
        {
            double maximumDistance = 0.0;
            for (const Vertex& vertex : output.sectors[1].vertices)
                maximumDistance = std::max(maximumDistance,
                    std::abs((TVector{vertex.x,vertex.y,vertex.z} - planePoint).dot(normal)));
            check(std::abs(maximumDistance - length) < 1.0e-12,
                "Extrude exact field commits the analytic physical normal distance");
            const auto after = assets::saveToSedBytes(output);
            check(window.undoDocumentForTest() && assets::saveToSedBytes(window.documentStateForTest().level) == before,
                "Extrude precise controls Undo restores exact native bytes");
            check(window.redoDocumentForTest() && assets::saveToSedBytes(window.documentStateForTest().level) == after,
                "Extrude precise controls Redo restores exact native bytes");
        }
    }
    for (const double offset : {0.0, 10000.0, 1000000.0})
    {
        for (const bool freePlane : {false, true})
        for (const double dimension : {0.005, 0.02, 10000.0})
        {
            const TVector origin{offset + 0.0013, offset + 0.0027, offset + 0.0009};
            const TVector a = freePlane ? TVector{0.6, 0.8, 0.0} : TVector{1, 0, 0};
            const TVector b = freePlane ? TVector{-0.48, 0.36, 0.8} : TVector{0, 1, 0};
            const TVector h = freePlane ? TVector{0.64, -0.48, 0.6} : TVector{0, 0, 1};
            const auto sector = window.sectorDrawFrameForTest(origin, a, b, h, {dimension, dimension, dimension}, freePlane);
            check(sector.has_value() && sector->vertices.size() == 8,
                "Draw Sector precision: translated small frame produces eight document corners");
            if (!sector || sector->vertices.size() != 8) continue;
            check(level::sectorIsConvex(*sector),
                "Draw Sector precision: translated tilted cube remains planar and convex");
            for (int mask = 0; mask < 8; ++mask)
            {
                const TVector expected = origin + a * ((mask & 1) ? dimension : 0.0)
                    + b * ((mask & 2) ? dimension : 0.0) + h * ((mask & 4) ? dimension : 0.0);
                double nearest = std::numeric_limits<double>::infinity();
                for (const auto& vertex : sector->vertices)
                {
                    nearest = std::min(nearest, (TVector{vertex.x, vertex.y, vertex.z} - expected).length());
                }
                check(nearest < 1.0e-10,
                    "Draw Sector precision: each output corner equals its analytic affine frame coordinate");
            }
            const auto wire = ui::sector::documentWireLinesFromSector(*sector);
            check(wire.size() == 24, "Draw Sector precision: preview includes twelve complete edges");
            for (std::size_t edge = 0; edge + 1 < wire.size(); edge += 2)
            {
                check(std::abs((wire[edge + 1] - wire[edge]).length() - dimension) < 3.0e-10,
                    "Draw Sector precision: wire preview preserves physical edge length at distant coordinates");
            }
            for (const double invalidDimension : {0.004999, 10000.000001})
                check(!window.sectorDrawFrameForTest(origin, a, b, h,
                        {invalidDimension, dimension, dimension}, freePlane),
                    "Draw Sector precision: dimension allowance does not accept a real below-minimum or above-maximum extent");
        }
    }
    editor::DocumentState state{};
    state.level = std::move(recoveryLevel);
    state.selection.sectors.insert(0);
    window.setDocumentStateForTest(std::move(state));
    window.activateSectorEditModeForTest();
    window.mutableDocumentStateForTest().selection.sectors = {0};
    window.updateEditActionsForTest();
    GlView* view = window.glViewForTest();
    check(view != nullptr, "cleave precision: native workspace recovery has a viewport");
    if ( !view )
    {
        return;
    }
    QAction* const cleaveAction = window.findChild<QAction*>(
        QStringLiteral("viewport.sector.cleave"));
    QToolButton* const multiCutButton = window.findChild<QToolButton*>(
        QStringLiteral("cleaveMultiCutButton"));
    check(cleaveAction && cleaveAction->isEnabled()
            && multiCutButton && multiCutButton->isEnabled(),
        "cleave precision: recovery fixture exposes enabled Sector Cleave and Multi-cut controls");
    if ( !cleaveAction || !cleaveAction->isEnabled()
        || !multiCutButton || !multiCutButton->isEnabled() )
    {
        return;
    }
    if ( !cleaveAction->isChecked() )
    {
        cleaveAction->trigger();
    }
    multiCutButton->setChecked(true);
    check(cleaveAction->isChecked() && multiCutButton->isChecked()
            && view->sectorCleaveToolEnabled() && view->cleaveStagedMode(),
        "cleave precision: recovery fixture owns the active Sector Cleave and Multi-cut controls");
    view->captureStagedCleaveContextForTest(false);
    view->addStagedCleaveLineForTest({0.1f, 0.2f, 0.3f}, {1.1f, 0.2f, 0.3f}, {0, 0, 1});
    const QByteArray legacyPayload = window.captureRecoveryEditorStateForTest();
    view->cancelStagedCleave();
    check(window.restoreRecoveryEditorStateForTest(legacyPayload) && view->stagedCleaveLineCount() == 1
            && view->recoveryState().cleaveLines[0].first.x == static_cast<double>(0.1f),
        "cleave precision: V1 records containing legacy float-authored numeric values remain readable");
    QCborMap root = QCborValue::fromCbor(legacyPayload).toMap();
    QCborArray viewport = root.value(QStringLiteral("viewport")).toArray();
    QCborMap cleave = viewport.at(38).toMap();
    QCborArray lines = cleave.value(QStringLiteral("lines")).toArray();
    check(lines.size() == 1, "cleave precision: captured V1 workspace contains its authored line");
    if ( lines.size() != 1 )
    {
        return;
    }
    QCborArray line = lines.at(0).toArray();
    const std::array<QCborArray, 5> exactVectors{{
        {1000000.0013, 1000000.0027, 1000000.0009},
        {1000000.0005, 1000000.0033, 1000000.0009},
        {0.6, 0.8, 0.0}, {0.0, 0.0, -1.0}, {-0.8, 0.6, 0.0}}};
    for ( std::size_t vector = 0; vector < exactVectors.size(); ++vector )
    {
        line[static_cast<qsizetype>(vector)] = exactVectors[vector];
    }
    lines[0] = line;
    cleave.insert(QStringLiteral("lines"), lines);
    viewport[38] = cleave;
    root.insert(QStringLiteral("viewport"), viewport);
    check(window.restoreRecoveryEditorStateForTest(QCborValue(root).toCbor()),
        "cleave precision: legacy V1 numeric arrays accept exact double authored points and normals");
    const auto recoveredRoot = QCborValue::fromCbor(window.captureRecoveryEditorStateForTest()).toMap();
    const auto recoveredViewport = recoveredRoot.value(QStringLiteral("viewport")).toArray();
    const auto recoveredLines = recoveredViewport.at(38).toMap().value(QStringLiteral("lines")).toArray();
    check(recoveredLines.size() == 1, "cleave precision: workspace recovery retains the pending line");
    if ( recoveredLines.size() == 1 )
    {
        const auto recovered = recoveredLines.at(0).toArray();
        for ( std::size_t vector = 0; vector < exactVectors.size(); ++vector )
        {
            const auto actual = recovered.at(static_cast<qsizetype>(vector)).toArray();
            check(actual == exactVectors[vector],
                "cleave precision: native recovery roundtrip preserves every double plane/frame component exactly");
        }
    }
    view->cancelStagedCleave();
    window.activateSectorEditModeForTest();
    const double cutX = 1000000.0013;
    view->sectorsCleaveRequested({1, 0, 0}, {cutX, 1000000.0, 1000000.0});
    const auto& committedLevel = window.documentStateForTest().level;
    std::size_t cutCorners = 0;
    for ( const auto& sector : committedLevel.sectors )
    {
        for ( const auto& vertex : sector.vertices )
        {
            cutCorners += std::abs(vertex.x - cutX) < 1.0e-9;
        }
    }
    check(committedLevel.sectors.size() == 2 && cutCorners == 8,
        "cleave precision: viewport-to-document command commits the exact fine plane at distant origin");
    for (const double normalScale : {1.0e-12, 1.0e200})
    {
        editor::DocumentState surfaceState{};
        surfaceState.level.layers = {"Layer0"};
        auto& sector = surfaceState.level.sectors.emplace_back();
        sector.layer = 0;
        sector.vertices = {{0,0,0,0},{2,0,0,0},{2,2,0,0},{0,2,0,0}};
        Surface face{};
        face.vertices = {0,1,2,3}; face.normal = {0,0,1}; face.texVertices.resize(4);
        sector.surfaces.push_back(face);
        surfaceState.selection.surfaces.insert({0,0});
        window.setDocumentStateForTest(std::move(surfaceState));
        window.activateSurfaceEditModeForTest();
        window.mutableDocumentStateForTest().selection.surfaces = {{0,0}};
        window.updateEditActionsForTest();
        const auto depth = window.undoDepthForTest();
        view->surfacesCleaveBatchRequested({TVector{normalScale,0,0}}, {TVector{1,0,0}}, false, {});
        const auto& result = window.documentStateForTest().level.sectors[0];
        check(result.surfaces.size() == 2 && window.undoDepthForTest() == depth + 1,
            "Surface Multi-cut uses scale-independent normalization for finite nonzero plane normals");
    }
}

} // namespace sed::app_tests
