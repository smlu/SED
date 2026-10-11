#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/assets/sed.hpp"
#include "sed/level/geometry.hpp"
#include "test_common.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/surface/surface_extrude_preview_utils.hpp"
#include "ui/viewport/cleave_message_utils.hpp"
#include <QAction>
#include <QApplication>
#include <QLineEdit>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <algorithm>
#include <limits>

namespace sed::app_tests
{
namespace
{
editor::DocumentState extrusionFixture(const double warp)
{
    editor::DocumentState state;
    state.level.layers = {"Layer0"};
    Sector sector;
    sector.num = 0;
    sector.vertices = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1},       {-1, 1, -1},
                       {-1, -1, 1},  {1, -1, 1},  {1, 1, 1 + warp}, {-1, 1, 1}};
    for ( const auto& indices : std::vector<std::vector<std::int32_t>>{
              {1, 5, 6, 2}, {0, 3, 7, 4}, {3, 2, 6, 7}, {0, 4, 5, 1}, {4, 7, 6, 5}, {0, 1, 2, 3}} )
    {
        Surface surface;
        surface.vertices = indices;
        surface.texVertices.resize(4);
        surface.normal = level::surfaceCalcNormalNewell(sector, surface);
        sector.addSurface(std::move(surface));
    }
    state.level.sectors.push_back(std::move(sector));
    state.selection.surfaces.insert({0, 4});
    return state;
}
bool sameRejectionSelection(const editor::Selection& a, const editor::Selection& b)
{
    const auto same = [](const auto& x, const auto& y)
    {
        return x.size() == y.size() &&
               std::all_of(x.begin(), x.end(), [&](const auto& item) { return y.contains(item); });
    };
    return same(a.sectors, b.sectors) && same(a.surfaces, b.surfaces) &&
           same(a.surfaceVertices, b.surfaceVertices) && same(a.things, b.things) &&
           same(a.frames, b.frames) && same(a.lights, b.lights) && same(a.cogs, b.cogs);
}
} // namespace
void runEditingRejectionUiTests()
{
    // Every enum is paired with independently fixed complete strings. The helpers
    // must not mutate the report/result supplied as input.
    {
        using Error = level::SurfaceExtrudeError;
        struct Vector
        {
            Error error;
            const char* targeted;
            const char* absent;
        };
        const Vector vectors[]{
            {Error::None, "", ""},
            {Error::InvalidSurface,
             "Cannot extrude Sector 7 / Surface 2: the selected surface does not exist. Select an existing "
             "face.",
             "Cannot extrude: the selected surface does not exist. Select an existing face."},
            {Error::InvalidLength,
             "Cannot extrude Sector 7 / Surface 2: length must be a finite number greater than zero.",
             "Cannot extrude: length must be a finite number greater than zero."},
            {Error::InvalidExpand, "Cannot extrude Sector 7 / Surface 2: expand must be a finite number.",
             "Cannot extrude: expand must be a finite number."},
            {Error::AlreadyAdjoined,
             "Cannot extrude Sector 7 / Surface 2: the source surface is already adjoined. Choose an "
             "unadjoined face.",
             "Cannot extrude: the source surface is already adjoined. Choose an unadjoined face."},
            {Error::InvalidSourceTopology,
             "Cannot extrude Sector 7 / Surface 2: the source surface has invalid topology or texture "
             "coordinates. Repair it before extruding.",
             "Cannot extrude: the source surface has invalid topology or texture coordinates. Repair it "
             "before extruding."},
            {Error::NonPlanarSource,
             "Cannot extrude Sector 7 / Surface 2: the source surface is not planar. Flatten the surface or "
             "choose another face.",
             "Cannot extrude: the source surface is not planar. Flatten the surface or choose another face."},
            {Error::NonConvexSource,
             "Cannot extrude Sector 7 / Surface 2: the source surface is not convex. Split it into convex "
             "faces first.",
             "Cannot extrude: the source surface is not convex. Split it into convex faces first."},
            {Error::CollapsedExpand,
             "Cannot extrude Sector 7 / Surface 2: expand would collapse the opening. Increase Expand toward "
             "zero.",
             "Cannot extrude: expand would collapse the opening. Increase Expand toward zero."},
            {Error::InvalidResult,
             "Cannot extrude Sector 7 / Surface 2: the requested extrusion would create invalid geometry. "
             "Change Length or Expand.",
             "Cannot extrude: the requested extrusion would create invalid geometry. Change Length or "
             "Expand."},
            {Error::InvalidAdjoin,
             "Cannot extrude Sector 7 / Surface 2: the generated opening cannot form a valid adjoin. Change "
             "Expand or choose another face.",
             "Cannot extrude: the generated opening cannot form a valid adjoin. Change Expand or choose "
             "another face."},
            {Error::AllocationFailure,
             "Cannot extrude Sector 7 / Surface 2: there is not enough memory to prepare the extrusion. Free "
             "memory and retry.",
             "Cannot extrude: there is not enough memory to prepare the extrusion. Free memory and retry."},
        };
        for ( const auto& vector : vectors )
        {
            for ( const bool target : {false, true} )
            {
                const level::SurfaceExtrudeReport report{vector.error,
                                                         target ? SurfaceRef{7, 2} : SurfaceRef{-1, -1}};
                check(ui::surface::surfaceExtrudeFailureMessage(report) ==
                          QString::fromLatin1(target ? vector.targeted : vector.absent),
                      "Extrude error formatter returns the complete fixed reason and optional target for "
                      "every enum");
                check(report.error == vector.error &&
                          report.surface == (target ? SurfaceRef{7, 2} : SurfaceRef{-1, -1}),
                      "Extrude message formatting leaves all input report fields unchanged");
            }
        }
        check(ui::surface::surfaceExtrudeFailureMessage({Error::InvalidLength, {7, -1}}) ==
                      QStringLiteral(
                          "Cannot extrude Sector 7: length must be a finite number greater than zero.") &&
                  ui::surface::surfaceExtrudeFailureMessage({Error::InvalidLength, {-1, 2}}) ==
                      QStringLiteral("Cannot extrude: length must be a finite number greater than zero."),
              "Extrude formatter handles partial target fields without inventing a sector");
    }
    {
        using Error = level::SectorCleaveBatchError;
        struct Vector
        {
            Error error;
            const char* targeted;
            const char* absent;
        };
        const Vector vectors[]{
            {Error::None, "", ""},
            {Error::InvalidSector,
             "Cleave rejected: Sector 7 / Surface 2: the selected sector does not exist. Select an existing "
             "sector.",
             "Cleave rejected: the selected sector does not exist. Select an existing sector."},
            {Error::EmptyPlaneSet,
             "Cleave rejected: Sector 7 / Surface 2: no cutting planes were provided. Draw a cleave line.",
             "Cleave rejected: no cutting planes were provided. Draw a cleave line."},
            {Error::InvalidPlane,
             "Cleave rejected: Sector 7 / Surface 2: line 4 is invalid. Redraw the line.",
             "Cleave rejected: the cutting plane is invalid. Redraw the line."},
            {Error::InvalidInputGeometry,
             "Cleave rejected: Sector 7 / Surface 2 has invalid input geometry. Repair it before cleaving.",
             "Cleave rejected: the selection has invalid input geometry. Repair it before cleaving."},
            {Error::PlaneDoesNotSplit,
             "Cleave rejected: Sector 7 / Surface 2: line 4 does not split the selected geometry. Move the "
             "line through the sector.",
             "Cleave rejected: the line does not split the selected geometry. Move the line through the "
             "sector."},
            {Error::CleaveFailed,
             "Cleave rejected: Sector 7 / Surface 2: the cut could not be completed. Move the line or choose "
             "another sector.",
             "Cleave rejected: the cut could not be completed. Move the line or choose another sector."},
            {Error::InvalidResultTopology,
             "Cleave rejected: Sector 7 / Surface 2: the cut would create invalid topology. Move the line or "
             "repair the source.",
             "Cleave rejected: the cut would create invalid topology. Move the line or repair the source."},
            {Error::NonPlanarResultSurface,
             "Cleave rejected: Sector 7 / Surface 2: the cut would create a non-planar face. Move the line "
             "or flatten the source face.",
             "Cleave rejected: the cut would create a non-planar face. Move the line or flatten the source "
             "face."},
            {Error::NonConvexResultSurface,
             "Cleave rejected: Sector 7 / Surface 2: the cut would create a non-convex face. Move the line "
             "or split the source face first.",
             "Cleave rejected: the cut would create a non-convex face. Move the line or split the source "
             "face first."},
            {Error::NonConvexResultSector,
             "Cleave rejected: Sector 7 / Surface 2: the cut would create a non-convex sector. Move the line "
             "or repair the source sector.",
             "Cleave rejected: the cut would create a non-convex sector. Move the line or repair the source "
             "sector."},
            {Error::InvalidResultAdjoin,
             "Cleave rejected: Sector 7 / Surface 2: the cut would create an invalid adjoin. Repair the "
             "shared boundary or move the line.",
             "Cleave rejected: the cut would create an invalid adjoin. Repair the shared boundary or move "
             "the line."},
        };
        for ( const auto& vector : vectors )
        {
            for ( const bool target : {false, true} )
            {
                level::SectorCleaveBatchResult result;
                result.error = vector.error;
                result.failedPlane = target ? 3 : -1;
                result.failedSector = target ? 7 : -1;
                result.failedSurface = target ? 2 : -1;
                result.outputSectors = {10, 11};
                check(ui::viewport::sectorCleaveFailureMessage(result) ==
                          QString::fromLatin1(target ? vector.targeted : vector.absent),
                      "Cleave error formatter returns the complete fixed reason line and optional target for "
                      "every enum");
                check(result.error == vector.error && result.failedPlane == (target ? 3 : -1) &&
                          result.failedSector == (target ? 7 : -1) &&
                          result.failedSurface == (target ? 2 : -1) &&
                          result.outputSectors == std::vector<std::int32_t>{10, 11},
                      "Cleave message formatting preserves every result and output field");
            }
        }
        level::SectorCleaveBatchResult partial;
        partial.error = Error::InvalidInputGeometry;
        partial.failedSector = 7;
        check(ui::viewport::sectorCleaveFailureMessage(partial) ==
                  QStringLiteral(
                      "Cleave rejected: Sector 7 has invalid input geometry. Repair it before cleaving."),
              "Cleave formatter handles a sector-only target without inventing a surface");
        partial.failedSector = -1;
        partial.failedSurface = 2;
        check(
            ui::viewport::sectorCleaveFailureMessage(partial) ==
                QStringLiteral(
                    "Cleave rejected: the selection has invalid input geometry. Repair it before cleaving."),
            "Cleave formatter ignores an orphan surface index without a valid target sector");
    }

    {
        const auto valid = extrusionFixture(0);
        const auto invalidSource = extrusionFixture(0.25);
        const auto badLength = ui::surface::prepareSurfaceExtrudePreview(valid.level, {0, 4}, 0, -0.1);
        check(!badLength.previewSector && badLength.expand == -0.1 &&
                  !badLength.resetCollapsedNegativeExpand &&
                  badLength.report.error == level::SurfaceExtrudeError::InvalidLength &&
                  badLength.report.surface == SurfaceRef{0, 4},
              "Extrude setup returns all invalid Length report fields without resetting unrelated Expand");
        const auto badSource =
            ui::surface::prepareSurfaceExtrudePreview(invalidSource.level, {0, 4}, 0.5, -0.1);
        check(!badSource.previewSector && badSource.expand == -0.1 &&
                  !badSource.resetCollapsedNegativeExpand &&
                  badSource.report.error == level::SurfaceExtrudeError::NonPlanarSource &&
                  badSource.report.surface == SurfaceRef{0, 4},
              "Extrude setup returns the exact source rejection report and preserves requested Expand");
        const auto collapsed = ui::surface::prepareSurfaceExtrudePreview(valid.level, {0, 4}, 0.5, -2.0);
        check(collapsed.previewSector && collapsed.expand == 0 && collapsed.resetCollapsedNegativeExpand &&
                  collapsed.report.error == level::SurfaceExtrudeError::None &&
                  collapsed.report.surface == SurfaceRef{0, 4},
              "Only proven collapse retries as plain Extrude and publishes a cleared successful report");
    }

    check(
        ui::surface::surfaceExtrudeFailureMessage({static_cast<level::SurfaceExtrudeError>(999), {-1, -1}}) ==
            QStringLiteral("Cannot extrude: the operation was rejected."),
        "Unknown Extrude error still produces a complete nonempty rejection");
    level::SectorCleaveBatchResult unknownCleave;
    unknownCleave.error = static_cast<level::SectorCleaveBatchError>(999);
    check(ui::viewport::sectorCleaveFailureMessage(unknownCleave) ==
              QStringLiteral("Cleave rejected: the operation was rejected."),
          "Unknown Cleave error still produces a complete nonempty rejection");
    unknownCleave.error = level::SectorCleaveBatchError::InvalidPlane;
    unknownCleave.failedPlane = std::numeric_limits<std::int32_t>::max();
    check(ui::viewport::sectorCleaveFailureMessage(unknownCleave) ==
              QStringLiteral("Cleave rejected: line 2147483648 is invalid. Redraw the line."),
          "Cleave display numbering widens before incrementing the maximum plane index");

    // A negative expansion retry is permitted only for an actual collapsed
    // expansion, not for source topology/planarity or an invalid Length.
    const auto invalid = extrusionFixture(0.25);
    for ( const bool invalidLength : {false, true} )
    {
        const auto setup = ui::surface::prepareSurfaceExtrudePreview(
            invalidLength ? extrusionFixture(0).level : invalid.level, {0, 4}, invalidLength ? 0 : 0.5, -0.1);
        check(!setup.previewSector && setup.expand == -0.1 && !setup.resetCollapsedNegativeExpand,
              "Extrude preview preserves negative Expand on non-collapse rejection");
    }
    MainWindow window;
    window.setAttribute(Qt::WA_DontShowOnScreen);
    auto* view = window.glViewForTest();
    auto* extrude = window.findChild<QAction*>(QStringLiteral("surface.extrude"));
    auto* apply = window.findChild<QToolButton*>(QStringLiteral("surfaceExtrudeApplyButton"));
    auto* length = window.findChild<ui::ScrubFloatEdit*>(QStringLiteral("surfaceExtrudeLengthSpin"));
    check(view && extrude && apply && length, "Editing rejection fixture exposes actual Extrude controls");
    if ( !view || !extrude || !apply || !length )
    {
        return;
    }
    auto* snap = window.findChild<QToolButton*>(QStringLiteral("surfaceExtrudeGridSnapButton"));
    if ( snap )
    {
        snap->setChecked(false);
    }
    const auto selectSource = [&](const double warp)
    {
        window.setDocumentStateForTest(extrusionFixture(warp));
        window.activateSurfaceEditModeForTest();
        window.activateSurfaceFaceSubModeForTest();
        view->setSelectedSurface(0, 4);
        window.mutableDocumentStateForTest().selection.clear();
        window.mutableDocumentStateForTest().selection.surfaces.insert({0, 4});
        window.updateEditActionsForTest();
    };
    const auto setLength = [&](const QString& text)
    {
        length->lineEdit()->setText(text);
        QMetaObject::invokeMethod(length->lineEdit(), "textEdited", Qt::DirectConnection,
                                  Q_ARG(QString, text));
        QMetaObject::invokeMethod(length->lineEdit(), "editingFinished", Qt::DirectConnection);
    };
    selectSource(0.25);
    setLength(QStringLiteral("0.5"));
    const auto before = window.documentStateForTest();
    const auto undoBefore = window.undoDepthForTest();
    const auto redoBefore = window.redoDepthForTest();
    const bool dirtyBefore = window.hasUnsavedChangesForTest();
    QString warning;
    QTimer dismiss;
    dismiss.setInterval(5);
    QObject::connect(&dismiss, &QTimer::timeout, &dismiss,
                     [&]
                     {
                         if ( auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) )
                         {
                             warning = message->text();
                             message->accept();
                         }
                     });
    check(extrude->isEnabled(), "Nonplanar source remains selectable so Extrude can explain its rejection");
    dismiss.start();
    extrude->trigger();
    dismiss.stop();
    check(warning == QStringLiteral("Cannot extrude Sector 0 / Surface 4: the source surface is not planar. "
                                    "Flatten the surface or choose another face."),
          "Rejected Extrude reports the complete actionable reason and exact source target");
    check(!extrude->isChecked() &&
              assets::sedLevelsEqual(window.documentStateForTest().level, before.level) &&
              sameRejectionSelection(window.documentStateForTest().selection, before.selection) &&
              window.undoDepthForTest() == undoBefore && window.redoDepthForTest() == redoBefore &&
              window.hasUnsavedChangesForTest() == dirtyBefore,
          "Rejected Extrude preview preserves complete document selection history and dirty state");

    selectSource(0);
    setLength(QStringLiteral("0.5"));
    dismiss.start();
    extrude->trigger();
    dismiss.stop();
    check(extrude->isChecked(), "Valid Extrude starts its real live preview");
    if ( extrude->isChecked() )
    {
        const auto validBefore = window.documentStateForTest();
        const auto validUndo = window.undoDepthForTest();
        const auto validRedo = window.redoDepthForTest();
        const bool validDirty = window.hasUnsavedChangesForTest();
        setLength(QStringLiteral("0"));
        check(!apply->isEnabled(), "Invalid live Extrude preview disables Apply");
        check(
            window.statusBar()->currentMessage() ==
                QStringLiteral(
                    "Cannot extrude Sector 0 / Surface 4: length must be a finite number greater than zero."),
            "Invalid live Extrude preview explains the precise input to correct");
        warning.clear();
        dismiss.start();
        apply->click();
        // Invoke the command connection directly as keyboard/automation routes
        // can reach Apply independently of the button enabled state.
        QMetaObject::invokeMethod(apply, "clicked", Qt::DirectConnection, Q_ARG(bool, false));
        dismiss.stop();
        check(
            warning ==
                QStringLiteral(
                    "Cannot extrude Sector 0 / Surface 4: length must be a finite number greater than zero."),
            "Extrude commit independently rejects invalid input with the same complete reason");
        check(assets::sedLevelsEqual(window.documentStateForTest().level, validBefore.level) &&
                  sameRejectionSelection(window.documentStateForTest().selection, validBefore.selection) &&
                  window.undoDepthForTest() == validUndo && window.redoDepthForTest() == validRedo &&
                  window.hasUnsavedChangesForTest() == validDirty,
              "Disabled invalid Extrude Apply leaves history dirty state and complete document untouched");
        extrude->trigger();
    }
    selectSource(0.00005);
    setLength(QStringLiteral("0.5"));
    const auto roundedBefore = window.documentStateForTest();
    const auto roundedUndo = window.undoDepthForTest();
    const auto roundedRedo = window.redoDepthForTest();
    const bool roundedDirty = window.hasUnsavedChangesForTest();
    warning.clear();
    dismiss.start();
    extrude->trigger();
    dismiss.stop();
    check(warning.isEmpty() && extrude->isChecked() && apply->isEnabled(),
          "Rounded original-style source opens an applicable actual Extrude preview");
    check(assets::sedLevelsEqual(window.documentStateForTest().level, roundedBefore.level) &&
              sameRejectionSelection(window.documentStateForTest().selection, roundedBefore.selection) &&
              window.undoDepthForTest() == roundedUndo && window.redoDepthForTest() == roundedRedo &&
              window.hasUnsavedChangesForTest() == roundedDirty,
          "Rounded Extrude live preview preserves exact document geometry history and dirty state");
    if ( extrude->isChecked() && apply->isEnabled() )
    {
        dismiss.start();
        apply->click();
        dismiss.stop();
        const auto roundedAfter = window.documentStateForTest();
        check(warning.isEmpty() && !extrude->isChecked() &&
                  roundedAfter.level.sectors.size() > roundedBefore.level.sectors.size() &&
                  window.undoDepthForTest() == roundedUndo + 1 && window.redoDepthForTest() == 0 &&
                  sameRejectionSelection(roundedAfter.selection, editor::Selection{}),
              "Rounded Extrude Apply publishes generated geometry with one history row and clears selection");
        double highestZ = -1e30;
        for ( const auto& sector : roundedAfter.level.sectors )
        {
            for ( const auto& vertex : sector.vertices )
            {
                highestZ = std::max(highestZ, vertex.z);
            }
        }
        check(std::abs(highestZ - 1.5) < 0.0001,
              "Rounded Extrude independently reaches the requested outward half-unit extent");
        check(window.undoForTest() &&
                  assets::sedLevelsEqual(window.documentStateForTest().level, roundedBefore.level) &&
                  sameRejectionSelection(window.documentStateForTest().selection, roundedBefore.selection),
              "Rounded Extrude Undo restores every original rounded coordinate and selection");
        check(window.redoForTest() &&
                  assets::sedLevelsEqual(window.documentStateForTest().level, roundedAfter.level) &&
                  sameRejectionSelection(window.documentStateForTest().selection, roundedAfter.selection),
              "Rounded Extrude Redo restores the complete committed result");
    }
    auto cleaveState = extrusionFixture(0.25);
    cleaveState.selection.clear();
    cleaveState.selection.sectors.insert(0);
    window.setDocumentStateForTest(cleaveState);
    window.activateSectorEditModeForTest();
    window.mutableDocumentStateForTest().selection.sectors.insert(0);
    const auto cleaveBefore = window.documentStateForTest();
    const auto cleaveUndo = window.undoDepthForTest();
    for ( const bool batch : {false, true} )
    {
        if ( batch )
        {
            view->sectorsCleaveBatchRequested({TVector{1, 0, 0}}, {TVector{0, 0, 0}}, false, TVector{});
        }
        else
        {
            view->sectorsCleaveRequested(TVector{1, 0, 0}, TVector{0, 0, 0});
        }
        check(
            view->toastTextForTest(GlView::ToastGroup::Error) ==
                QStringLiteral("Cleave rejected: Sector 0 / Surface 4 has invalid input geometry. Repair it "
                               "before cleaving."),
            "Cleave reports invalid source geometry and target rather than blaming an uncreated result");
        check(assets::sedLevelsEqual(window.documentStateForTest().level, cleaveBefore.level) &&
                  sameRejectionSelection(window.documentStateForTest().selection, cleaveBefore.selection) &&
                  window.undoDepthForTest() == cleaveUndo,
              "Cleave diagnostic rejection preserves the complete source and history");
    }
}
} // namespace sed::app_tests
