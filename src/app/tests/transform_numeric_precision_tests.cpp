#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/assets/sed.hpp"
#include "sed/level/build.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level/transform.hpp"
#include "test_common.hpp"
#include "ui/components/ui_decimal_format.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QTimer>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <iostream>

namespace sed::app_tests
{
namespace
{
bool equalNumericMenuSelection(const editor::Selection& a, const editor::Selection& b)
{
    const auto equalSet = [](const auto& left, const auto& right)
    {
        return left.size() == right.size() && std::all_of(left.begin(), left.end(), [&right](const auto& item)
                                                          { return right.contains(item); });
    };
    return equalSet(a.sectors, b.sectors) && equalSet(a.surfaces, b.surfaces) &&
           equalSet(a.surfaceVertices, b.surfaceVertices) && equalSet(a.things, b.things) &&
           equalSet(a.frames, b.frames) && equalSet(a.lights, b.lights) && equalSet(a.cogs, b.cogs);
}

bool equalTransformRuntimeGeometry(const Level& a, const Level& b)
{
    if (a.curId != b.curId || a.sectors.size() != b.sectors.size())
    {
        return false;
    }

    const auto sameDouble = [](const double lhs, const double rhs)
    {
        return std::bit_cast<std::uint64_t>(lhs) == std::bit_cast<std::uint64_t>(rhs);
    };
    for (std::size_t sectorIndex = 0; sectorIndex < a.sectors.size(); ++sectorIndex)
    {
        const Sector& left = a.sectors[sectorIndex];
        const Sector& right = b.sectors[sectorIndex];
        if (left.id != right.id || left.num != right.num || left.mark != right.mark ||
            left.vertices.size() != right.vertices.size() || left.surfaces.size() != right.surfaces.size())
        {
            return false;
        }
        for (std::size_t vertexIndex = 0; vertexIndex < left.vertices.size(); ++vertexIndex)
        {
            if (left.vertices[vertexIndex].mark != right.vertices[vertexIndex].mark)
            {
                return false;
            }
        }
        for (std::size_t surfaceIndex = 0; surfaceIndex < left.surfaces.size(); ++surfaceIndex)
        {
            const Surface& leftSurface = left.surfaces[surfaceIndex];
            const Surface& rightSurface = right.surfaces[surfaceIndex];
            if (leftSurface.num != rightSurface.num || leftSurface.mark != rightSurface.mark ||
                leftSurface.d3dId != rightSurface.d3dId || leftSurface.nAdj != rightSurface.nAdj ||
                leftSurface.nMat != rightSurface.nMat ||
                !sameDouble(leftSurface.normal.x, rightSurface.normal.x) ||
                !sameDouble(leftSurface.normal.y, rightSurface.normal.y) ||
                !sameDouble(leftSurface.normal.z, rightSurface.normal.z))
            {
                return false;
            }
        }
    }
    return true;
}

bool equalTransformDocumentState(const editor::DocumentState& a, const editor::DocumentState& b)
{
    return assets::sedLevelsEqual(a.level, b.level) &&
           equalTransformRuntimeGeometry(a.level, b.level) &&
           equalNumericMenuSelection(a.selection, b.selection);
}

editor::DocumentState numericMenuFixture()
{
    editor::DocumentState state;
    state.level.layers = {"Layer0"};
    // Two valid outside-sector Things provide a nonzero center and off-axis
    // offsets without depending on geometry creation or connected-UV helpers.
    for (const int direction : {1, -1})
    {
        Thing thing;
        thing.name = "__numeric_menu_missing_template__";
        thing.x = 4 + direction;
        thing.y = 5 + 2 * direction;
        thing.z = 6 + 3 * direction;
        thing.pch = 20;
        thing.yaw = 10;
        thing.rol = 30;
        thing.sector = -1;
        state.level.things.push_back(thing);
    }
    state.selection.things = {0, 1};
    return state;
}

editor::DocumentState numericSectorScaleFixture(const bool collapseAtDoublePrecision, const bool roundedSource = false)
{
    editor::DocumentState state;
    state.level.kind = ProjectType::JKDF2;
    state.level.layers = {"Layer0"};

    const TVector minimum = collapseAtDoublePrecision
        ? TVector{1.0e16, 1.0e16, 1.0e16}
        : TVector{-1.0, -1.0, -1.0};
    const TVector maximum = collapseAtDoublePrecision
        ? TVector{1.0e16 + 8.0, 1.0e16 + 8.0, 1.0e16 + 8.0}
        : TVector{1.0, 1.0, 1.0};
    const std::int32_t sectorIndex = level::levelCreateSectorPrimitiveBox(
        state.level, level::SectorPrimitive::Cube, minimum, maximum);
    if (sectorIndex >= 0)
    {
        if (roundedSource)
        {
            // Authored level geometry can have small text-coordinate rounding
            // residuals. This remains inside the diagnostic allowance but is
            // deliberately outside the strict solid-convexity predicate.
            state.level.sectors[static_cast<std::size_t>(sectorIndex)].vertices[6].z += 0.0002;
            level::levelRecalcSector(state.level, sectorIndex);
        }
        state.selection.sectors.insert(sectorIndex);
    }
    return state;
}

editor::DocumentState numericOctahedronSurfaceScaleFixture(const bool roundedSource = false)
{
    editor::DocumentState state;
    state.level.kind = ProjectType::JKDF2;
    state.level.layers = {"Layer0"};

    Sector sector{};
    const std::array<TVector, 6> points{{
        {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {-1.0, 0.0, 0.0},
        {0.0, -1.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0},
    }};
    for (const TVector& point : points)
    {
        sector.addVertex(point.x, point.y, point.z);
    }

    // Inward-wound triangular faces keep every individual face planar and
    // convex even when moving one selected face makes the closed solid concave.
    const std::array<std::array<std::int32_t, 3>, 8> faces{{
        {{4, 1, 0}}, {{4, 2, 1}}, {{4, 3, 2}}, {{4, 0, 3}},
        {{5, 0, 1}}, {{5, 1, 2}}, {{5, 2, 3}}, {{5, 3, 0}},
    }};
    for (const auto& face : faces)
    {
        Surface surface{};
        surface.vertices.assign(face.begin(), face.end());
        surface.texVertices.resize(face.size());
        surface.normal = level::surfaceCalcNormalNewell(sector, surface);
        sector.addSurface(std::move(surface));
    }
    if (roundedSource)
    {
        // Preserve a small legacy-like surface-plane residual while leaving all
        // triangular faces individually planar by construction.
        sector.vertices[4].x = 0.0002;
    }
    state.level.sectors.push_back(std::move(sector));
    level::levelRecalcSector(state.level, 0);

    if (roundedSource)
    {
        state.selection.sectors.insert(0);
    }
    else
    {
        Thing pivotThing{};
        pivotThing.name = "__numeric_menu_missing_template__";
        pivotThing.x = 100.0;
        pivotThing.sector = -1;
        state.level.things.push_back(pivotThing);
        state.selection.surfaces.insert(SurfaceRef{0, 0});
        state.selection.things.insert(0);
    }
    return state;
}
} // namespace

void runTransformNumericPrecisionTests()
{
    struct RestoreLocale
    {
        QLocale previous = QLocale();
        UiDecimalSeparator previousPolicy = ui::uiDecimalSeparator();
        QLocale previousSystemLocale = ui::uiSystemNumericLocale();
        ~RestoreLocale()
        {
            QLocale::setDefault(previous);
            ui::installUiDecimalPolicy(*qApp, previousPolicy, previousSystemLocale);
        }
    } restoreLocale;
    for (const bool commaLocale : {false, true})
    {
        const QLocale inputLocale = commaLocale
            ? QLocale(QLocale::German, QLocale::Germany) : QLocale::c();
        QLocale::setDefault(ui::numericLocaleForPreference(UiDecimalSeparator::system, inputLocale));
        ui::installUiDecimalPolicy(*qApp, UiDecimalSeparator::system, inputLocale);
        MainWindow window;
        window.setAttribute(Qt::WA_DontShowOnScreen);
        const std::array<const char*, 3> names{"transform.translate", "transform.rotate", "transform.scale"};
        for (int operation = 0; operation < 3; ++operation)
        {
            auto* action = window.findChild<QAction*>(QString::fromLatin1(names[operation]));
            check(action != nullptr, "Precise Transform test exposes each actual menu action");
            if (!action)
            {
                continue;
            }
            const int promptCount = operation == 0 ? 3 : operation == 1 ? 2 : 1;
            // -1 accepts the full command. Every other vector cancels one stage,
            // including Translate after previously accepted X/Y and Rotate after Axis.
            for (int cancelAt = -1; cancelAt < promptCount; ++cancelAt)
            {
                window.setDocumentStateForTest(numericMenuFixture());
                window.activateThingEditModeForTest();
                window.glViewForTest()->setSelectedThing(0);
                window.mutableDocumentStateForTest().selection.things = {0, 1};
                window.updateEditActionsForTest();
                window.pushUndoForTitleForTest(editor::DocumentDirty::Things);
                window.mutableDocumentStateForTest().level.things[0].z = 123;
                check(window.undoForTest(), "Precise Transform fixture seeds nonempty Redo history");
                const auto before = window.documentStateForTest();
                check(before.selection.things.size() == 2 && before.selection.things.contains(0) &&
                          before.selection.things.contains(1),
                      "Precise Transform fixture retains both independent pivot contributors");
                const auto undoDepth = window.undoDepthForTest();
                const auto redoDepth = window.redoDepthForTest();
                int prompts = 0;
                bool unexpectedMessage = false;
                QTimer respond;
                respond.setInterval(5);
                QObject::connect(
                    &respond, &QTimer::timeout, &respond,
                    [&]
                    {
                        if (auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget()))
                        {
                            const int stage = prompts++;
                            if (stage >= promptCount)
                            {
                                check(false, "Precise Transform has no unexpected extra numeric prompt");
                                dialog->reject();
                                return;
                            }
                            check(dialog->locale() == QLocale(),
                                  "Transform dialog follows the explicitly scoped default locale");
                            const bool axisPrompt = operation == 1 && stage == 0;
                            if (axisPrompt)
                            {
                                check(dialog->labelText() == QStringLiteral("Axis") &&
                                          dialog->textValue() == QStringLiteral("Z"),
                                      "Rotate axis prompt has the full fixed label and default");
                            }
                            else
                            {
                                auto* edit = dialog->findChild<QLineEdit*>();
                                check(edit != nullptr,
                                      "Precise Transform exposes its real numeric line edit");
                                if (!edit)
                                {
                                    dialog->reject();
                                    return;
                                }
                                const QString expectedLabel =
                                    operation == 0 ? QString::fromLatin1(
                                                         std::array<const char*, 3>{"dX", "dY", "dZ"}[stage])
                                    : operation == 1 ? QStringLiteral("Angle (deg)")
                                                     : QStringLiteral("Factor");
                                check(dialog->labelText() == expectedLabel &&
                                          edit->text() == (operation == 0   ? QStringLiteral("0")
                                                           : operation == 1 ? QStringLiteral("90")
                                                                            : QStringLiteral("1")),
                                      "Transform numeric label and concise default match complete fixed "
                                      "strings");
                                check(dialog->doubleMinimum() == (operation == 1 ? -360.0 : -1000000.0) &&
                                          dialog->doubleMaximum() == (operation == 1 ? 360.0 : 1000000.0),
                                      "Precise Transform retains the existing command input ranges");
                                const QString typed = operation == 0
                                                          ? QString::fromLatin1(std::array<const char*, 3>{
                                                                "1e-9", "-2e-9", "3e-9"}[stage])
                                                      : operation == 1 ? QStringLiteral("1e-9")
                                                      : commaLocale    ? QStringLiteral("1,000000001")
                                                                       : QStringLiteral("1.000000001");
                                edit->setFocus();
                                edit->selectAll();
                                for (const QChar character : typed)
                                {
                                    QKeyEvent key(QEvent::KeyPress, 0, Qt::NoModifier, QString(character));
                                    QApplication::sendEvent(edit, &key);
                                }
                                check(edit->text() == typed, "Transform accepts the entire fixed precise "
                                                             "user-typed token without decimal truncation");
                            }
                            if (stage == cancelAt)
                            {
                                dialog->reject();
                            }
                            else
                            {
                                dialog->accept();
                            }
                        }
                        else if (auto* message =
                                     qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                        {
                            unexpectedMessage = true;
                            message->reject();
                        }
                    });
                respond.start();
                action->trigger();
                respond.stop();
                check(!unexpectedMessage && prompts == (cancelAt < 0 ? promptCount : cancelAt + 1),
                      "Transform traverses exactly the accepted or canceled modal chain without a warning");
                const auto committed = window.documentStateForTest();
                if (cancelAt >= 0)
                {
                    check(assets::sedLevelsEqual(committed.level, before.level) &&
                              equalNumericMenuSelection(committed.selection, before.selection) &&
                              window.undoDepthForTest() == undoDepth &&
                              window.redoDepthForTest() == redoDepth,
                          "Cancel at every Transform stage preserves complete Level selection and both "
                          "history stacks");
                    continue;
                }
                check(window.undoDepthForTest() == undoDepth + 1 && window.redoDepthForTest() == 0 &&
                          equalNumericMenuSelection(committed.selection, editor::Selection{}),
                      "Precise Transform commits one history row and clears selection and old Redo");
                auto expected = before.level;
                constexpr double angle = 1e-9;
                const double radians = angle * std::numbers::pi / 180.0;
                for (std::size_t i = 0; i < expected.things.size(); ++i)
                {
                    auto& thing = expected.things[i];
                    if (operation == 0)
                    {
                        thing.x += 1e-9;
                        thing.y -= 2e-9;
                        thing.z += 3e-9;
                    }
                    else if (operation == 1)
                    {
                        const double x = thing.x - 4;
                        const double y = thing.y - 5;
                        // Legacy menu rotations use the transpose of the standard
                        // positive-angle matrix (clockwise for the Z axis).
                        thing.x = 4 + x * std::cos(radians) + y * std::sin(radians);
                        thing.y = 5 - x * std::sin(radians) + y * std::cos(radians);
                        thing.yaw = 10 - angle;
                    }
                    else
                    {
                        thing.x = 4 + 1.000000001 * (thing.x - 4);
                        thing.y = 5 + 1.000000001 * (thing.y - 5);
                        thing.z = 6 + 1.000000001 * (thing.z - 6);
                    }
                    if (operation == 1)
                    {
                        const auto& actual = committed.level.things[i];
                        const bool exactPose = std::abs(actual.x - thing.x) <= 2e-14
                            && std::abs(actual.y - thing.y) <= 2e-14 && actual.z == thing.z
                            && actual.pch == thing.pch && actual.yaw == thing.yaw
                            && actual.rol == thing.rol && actual.sector == -1;
                        if (!exactPose)
                        {
                            const auto poseText = [](const Thing& value)
                            {
                                return QStringLiteral("(%1/%2/%3:%4/%5/%6), sector=%7")
                                    .arg(value.x, 0, 'g', 17).arg(value.y, 0, 'g', 17)
                                    .arg(value.z, 0, 'g', 17).arg(value.pch, 0, 'g', 17)
                                    .arg(value.yaw, 0, 'g', 17).arg(value.rol, 0, 'g', 17).arg(value.sector);
                            };
                            std::cerr << "Tiny Rotate pose mismatch, Thing " << i
                                << "\nexpected: " << poseText(thing).toStdString()
                                << "\nactual: " << poseText(actual).toStdString() << '\n';
                        }
                        check(exactPose,
                              "Tiny Rotate matches independent off-axis analytic coordinates and all "
                              "authored pose fields");
                        // Permit only the already independently bounded trigonometric
                        // rounding difference when checking every other Level field.
                        thing.x = actual.x;
                        thing.y = actual.y;
                    }
                }
                check(assets::sedLevelsEqual(committed.level, expected),
                      "Precise Transform preserves every unrelated Level field and matches the independent "
                      "complete result");
                check(
                    window.undoForTest() &&
                        assets::sedLevelsEqual(window.documentStateForTest().level, before.level) &&
                        equalNumericMenuSelection(window.documentStateForTest().selection, before.selection),
                    "Precise Transform Undo restores complete original Level and selection");
                check(window.redoForTest() &&
                          assets::sedLevelsEqual(window.documentStateForTest().level, committed.level) &&
                          equalNumericMenuSelection(window.documentStateForTest().selection,
                                                    committed.selection),
                      "Precise Transform Redo restores complete committed Level and selection");
            }
        }
    }

    MainWindow scaleWindow;
    scaleWindow.setAttribute(Qt::WA_DontShowOnScreen);
    QAction* const scaleAction = scaleWindow.findChild<QAction*>(QStringLiteral("transform.scale"));
    check(scaleAction != nullptr, "Scale admission regression exposes the real numeric Scale action");
    if (!scaleAction)
    {
        return;
    }

    const auto runScaleCase = [&](const double factor, const bool collapseAtDoublePrecision,
                                  const bool expectRejection, const bool roundedSource)
    {
        editor::DocumentState state = numericSectorScaleFixture(collapseAtDoublePrecision, roundedSource);
        check(state.level.sectors.size() == 1 && state.selection.sectors.contains(0),
              "Scale admission fixture creates and selects one complete Sector");
        if (roundedSource && !state.level.sectors.empty())
        {
            check(!level::sectorIsConvex(state.level.sectors[0]) &&
                      level::sectorIsConvexForDiagnostics(state.level.sectors[0]),
                  "Rounded legacy Sector fixture fails only the strict solid-convexity predicate");
        }
        scaleWindow.activateSectorEditModeForTest();
        scaleWindow.setDocumentStateForTest(state);
        GlView* const view = scaleWindow.glViewForTest();
        check(view != nullptr, "Scale admission fixture exposes its viewport");
        if (!view || state.level.sectors.empty())
        {
            return;
        }
        view->setSelectedSector(0);
        scaleWindow.mutableDocumentStateForTest().selection.sectors = {0};
        view->setPreserveConnectedSurfaceUvsEnabled(false);
        scaleWindow.updateEditActionsForTest();
        check(!view->preserveConnectedSurfaceUvsEnabled(),
              "Scale admission regression exercises Preserve UVs off");
        check(scaleAction->isEnabled(), "Scale admission fixture enables the actual numeric Scale action");

        // Leave one Redo row in place so a rejected transform must preserve both
        // history stacks, as well as the selected Sector and complete Level.
        scaleWindow.pushUndoForTitleForTest(editor::DocumentDirty::LevelGeometry);
        scaleWindow.mutableDocumentStateForTest().level.sectors[0].ambient =
            ColorF{0.125f, 0.25f, 0.5f, 1.0f};
        check(scaleWindow.undoForTest(), "Scale admission fixture seeds retained Redo history");
        scaleWindow.updateEditActionsForTest();
        check(scaleAction->isEnabled(), "Scale action remains enabled after the Redo fixture is seeded");
        const auto before = scaleWindow.documentStateForTest();
        const std::size_t undoDepth = scaleWindow.undoDepthForTest();
        const std::size_t redoDepth = scaleWindow.redoDepthForTest();

        int factorPrompts = 0;
        int textureQuestions = 0;
        int warnings = 0;
        QString warningText;
        QTimer respond;
        respond.setInterval(5);
        QObject::connect(&respond, &QTimer::timeout, &respond, [&]
        {
            if (auto* input = qobject_cast<QInputDialog*>(QApplication::activeModalWidget()))
            {
                ++factorPrompts;
                input->setDoubleValue(factor);
                input->accept();
            }
            else if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            {
                if (message->icon() == QMessageBox::Question)
                {
                    ++textureQuestions;
                    if (QAbstractButton* const noButton = message->button(QMessageBox::No))
                    {
                        noButton->click();
                    }
                    else
                    {
                        message->reject();
                    }
                }
                else
                {
                    ++warnings;
                    warningText = message->text();
                    message->accept();
                }
            }
        });
        respond.start();
        scaleAction->trigger();
        respond.stop();

        const auto after = scaleWindow.documentStateForTest();
        check(factorPrompts == 1 && textureQuestions == 1,
              "Sector numeric Scale asks for one factor and the existing texture-scale choice");
        if (expectRejection)
        {
            check(warnings == 1 && warningText == QStringLiteral("Scale was cancelled because it would produce degenerate geometry."),
                  "A geometry-collapsing numeric Scale reports the fixed rejection message");
            check(assets::sedLevelsEqual(after.level, before.level) &&
                      equalNumericMenuSelection(after.selection, before.selection) &&
                      scaleWindow.undoDepthForTest() == undoDepth &&
                      scaleWindow.redoDepthForTest() == redoDepth,
                  "Rejected zero or near-degenerate numeric Scale preserves complete Level, selection, Undo, and Redo");
            return;
        }

        bool transformedVerticesMatch = after.level.sectors.size() == before.level.sectors.size() &&
            after.level.sectors[0].vertices.size() == before.level.sectors[0].vertices.size();
        if (transformedVerticesMatch)
        {
            const auto& sourceVertices = before.level.sectors[0].vertices;
            const auto& transformedVertices = after.level.sectors[0].vertices;
            double cx = 0.0;
            double cy = 0.0;
            double cz = 0.0;
            for (const Vertex& vertex : sourceVertices)
            {
                cx += vertex.x;
                cy += vertex.y;
                cz += vertex.z;
            }
            cx /= static_cast<double>(sourceVertices.size());
            cy /= static_cast<double>(sourceVertices.size());
            cz /= static_cast<double>(sourceVertices.size());
            for (std::size_t i = 0; i < sourceVertices.size(); ++i)
            {
                const Vertex& source = sourceVertices[i];
                const Vertex& actual = transformedVertices[i];
                transformedVerticesMatch = transformedVerticesMatch &&
                    actual.x == cx + factor * (source.x - cx) &&
                    actual.y == cy + factor * (source.y - cy) &&
                    actual.z == cz + factor * (source.z - cz);
            }
        }
        check(warnings == 0 && transformedVerticesMatch &&
                  !assets::sedLevelsEqual(after.level, before.level) &&
                  equalNumericMenuSelection(after.selection, editor::Selection{}) &&
                  scaleWindow.undoDepthForTest() == undoDepth + 1 && scaleWindow.redoDepthForTest() == 0,
              "Valid positive or negative numeric Scale commits expected vertex positions in one history row");
        check(scaleWindow.undoForTest() &&
                  assets::sedLevelsEqual(scaleWindow.documentStateForTest().level, before.level) &&
                  equalNumericMenuSelection(scaleWindow.documentStateForTest().selection, before.selection),
              "Valid numeric Scale Undo restores complete original Sector and selection");
        check(scaleWindow.redoForTest() &&
                  assets::sedLevelsEqual(scaleWindow.documentStateForTest().level, after.level) &&
                  equalNumericMenuSelection(scaleWindow.documentStateForTest().selection, after.selection),
              "Valid numeric Scale Redo restores complete committed Sector and selection");
    };

    runScaleCase(0.0, false, true, false);
    runScaleCase(0.01, true, true, false);
    runScaleCase(2.0, false, false, false);
    runScaleCase(-1.0, false, false, false);

    // Surface Scale can preserve every individual triangle while making the
    // closed sector concave. Keep a far-away selected Thing in the pivot so the
    // real numeric action transforms only one face.
    {
        editor::DocumentState state = numericOctahedronSurfaceScaleFixture();
        check(state.level.sectors.size() == 1 && state.selection.surfaces.contains(SurfaceRef{0, 0}) &&
                  state.selection.things.contains(0),
              "Surface Scale convexity fixture selects one octahedron face and a pivot Thing");
        if (!state.level.sectors.empty())
        {
            const Sector& source = state.level.sectors[0];
            check(level::sectorIsConvex(source), "Surface Scale source octahedron is strictly convex");
            bool allFacesValid = true;
            for (const Surface& surface : source.surfaces)
            {
                allFacesValid = allFacesValid && level::surfaceIsPlanar(source, surface) &&
                    level::surfaceIsConvex(source, surface) &&
                    !level::surfaceCalcNormalNewell(source, surface).isZero();
            }
            check(allFacesValid, "Surface Scale source has individually valid triangular faces");

            constexpr double factor = 1.5;
            constexpr double cx = 25.25;
            constexpr double cy = 0.25;
            constexpr double cz = 0.25;
            ScaleData scale{};
            scale.cx = cx;
            scale.cy = cy;
            scale.cz = cz;
            scale.sfactor = factor;
            const std::array<SurfaceRef, 1> facesToScale{SurfaceRef{0, 0}};
            const std::array<std::int32_t, 1> thingsToScale{0};
            Level candidate = state.level;
            level::levelScaleSurfaces(candidate, facesToScale, scale, false);
            level::levelScaleThings(candidate, thingsToScale, scale);
            check(!level::sectorIsConvex(candidate.sectors[0]),
                  "Surface Scale candidate turns the closed sector concave");
            bool candidateFacesValid = true;
            for (const Surface& surface : candidate.sectors[0].surfaces)
            {
                candidateFacesValid = candidateFacesValid && level::surfaceIsPlanar(candidate.sectors[0], surface) &&
                    level::surfaceIsConvex(candidate.sectors[0], surface) &&
                    !level::surfaceCalcNormalNewell(candidate.sectors[0], surface).isZero();
            }
            check(candidateFacesValid,
                  "Concave Surface Scale candidate still has individually valid triangular faces");
        }

        MainWindow surfaceWindow;
        surfaceWindow.setAttribute(Qt::WA_DontShowOnScreen);
        QAction* const surfaceScaleAction =
            surfaceWindow.findChild<QAction*>(QStringLiteral("transform.scale"));
        check(surfaceScaleAction != nullptr, "Surface Scale regression exposes the real numeric action");
        if (surfaceScaleAction && !state.level.sectors.empty())
        {
            surfaceWindow.setDocumentStateForTest(state);
            surfaceWindow.activateSurfaceEditModeForTest();
            GlView* const view = surfaceWindow.glViewForTest();
            check(view != nullptr, "Surface Scale fixture exposes its viewport");
            if (view)
            {
                view->setSelectedSurface(0, 0);
                view->setSelectedThing(0);
                view->setPreserveConnectedSurfaceUvsEnabled(false);
            }
            surfaceWindow.mutableDocumentStateForTest().selection.surfaces = {SurfaceRef{0, 0}};
            surfaceWindow.mutableDocumentStateForTest().selection.things = {0};
            surfaceWindow.updateEditActionsForTest();
            check(surfaceScaleAction->isEnabled(), "Surface Scale fixture enables the numeric action");

            surfaceWindow.pushUndoForTitleForTest(editor::DocumentDirty::LevelGeometry);
            surfaceWindow.mutableDocumentStateForTest().level.sectors[0].ambient =
                ColorF{0.125f, 0.25f, 0.5f, 1.0f};
            check(surfaceWindow.undoForTest(), "Surface Scale fixture seeds retained Redo history");
            surfaceWindow.updateEditActionsForTest();
            const auto before = surfaceWindow.documentStateForTest();
            const std::size_t undoDepth = surfaceWindow.undoDepthForTest();
            const std::size_t redoDepth = surfaceWindow.redoDepthForTest();
            int factorPrompts = 0;
            int textureQuestions = 0;
            int warnings = 0;
            QString warningText;
            QTimer respond;
            respond.setInterval(5);
            QObject::connect(&respond, &QTimer::timeout, &respond, [&]
            {
                if (auto* input = qobject_cast<QInputDialog*>(QApplication::activeModalWidget()))
                {
                    ++factorPrompts;
                    input->setDoubleValue(1.5);
                    input->accept();
                }
                else if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                {
                    if (message->icon() == QMessageBox::Question)
                    {
                        ++textureQuestions;
                        if (QAbstractButton* const noButton = message->button(QMessageBox::No))
                        {
                            noButton->click();
                        }
                        else
                        {
                            message->reject();
                        }
                    }
                    else
                    {
                        ++warnings;
                        warningText = message->text();
                        message->accept();
                    }
                }
            });
            respond.start();
            surfaceScaleAction->trigger();
            respond.stop();
            const auto after = surfaceWindow.documentStateForTest();
            check(factorPrompts == 1 && textureQuestions == 1,
                  "Surface numeric Scale asks for its factor and texture choice exactly once");
            check(warnings == 1 && warningText ==
                      QStringLiteral("Scale was cancelled because it would produce degenerate geometry."),
                  "Concave Surface Scale is rejected with the fixed geometry warning");
            check(assets::sedLevelsEqual(after.level, before.level) &&
                      equalNumericMenuSelection(after.selection, before.selection) &&
                      surfaceWindow.undoDepthForTest() == undoDepth &&
                      surfaceWindow.redoDepthForTest() == redoDepth,
                  "Rejected concave Surface Scale preserves complete Level, selection, Undo, and Redo");
        }
    }

    // A source already outside strict solid convexity but inside the legacy
    // diagnostic tolerance remains editable by a valid whole-Sector scale.
    runScaleCase(2.0, false, false, true);

    // Viewport Scale commits are separate from pointer previews. Exercise the
    // exact end-of-drag signals with Preserve UVs disabled, where the numeric
    // action's existing source-relative geometry admission used to be bypassed.
    const auto runSectorScaleSignalCase = [&](const bool expectRejection,
                                               const bool collapseAtDoublePrecision,
                                               const bool roundedSource,
                                               const double factor,
                                               const TVector& pivot)
    {
        editor::DocumentState state = numericSectorScaleFixture(collapseAtDoublePrecision, roundedSource);
        check(state.level.sectors.size() == 1 && state.selection.sectors.contains(0),
              "Viewport Sector Scale fixture creates and selects one complete Sector");
        if ( roundedSource && !state.level.sectors.empty() )
        {
            check(!level::sectorIsConvex(state.level.sectors[0]) &&
                      level::sectorIsConvexForDiagnostics(state.level.sectors[0]),
                  "Viewport Sector Scale fixture retains a rounded legacy source inside diagnostic tolerance");
        }

        MainWindow signalWindow;
        signalWindow.setAttribute(Qt::WA_DontShowOnScreen);
        signalWindow.activateSectorEditModeForTest();
        signalWindow.setDocumentStateForTest(state);
        GlView* const view = signalWindow.glViewForTest();
        check(view != nullptr, "Viewport Sector Scale fixture exposes its viewport");
        if ( !view || state.level.sectors.empty() )
        {
            return;
        }
        view->setSelectedSector(0);
        view->setPreserveConnectedSurfaceUvsEnabled(false);

        signalWindow.pushUndoForTitleForTest(editor::DocumentDirty::LevelGeometry);
        signalWindow.mutableDocumentStateForTest().level.sectors[0].ambient =
            ColorF{0.125f, 0.25f, 0.5f, 1.0f};
        check(signalWindow.undoForTest(), "Viewport Sector Scale fixture seeds retained Redo history");
        const editor::DocumentState before = signalWindow.documentStateForTest();
        const std::size_t undoDepth = signalWindow.undoDepthForTest();
        const std::size_t redoDepth = signalWindow.redoDepthForTest();

        view->sectorsScaleRequested(QVector3D{}, factor, pivot);
        const editor::DocumentState after = signalWindow.documentStateForTest();
        if ( expectRejection )
        {
            check(equalTransformDocumentState(after, before) &&
                      signalWindow.undoDepthForTest() == undoDepth &&
                      signalWindow.redoDepthForTest() == redoDepth,
                  "Rejected viewport Sector Scale preserves authored/runtime Level, selection, Undo, and Redo");
            return;
        }

        editor::DocumentState expected = before;
        Sector& expectedSector = expected.level.sectors[0];
        for ( Vertex& vertex : expectedSector.vertices )
        {
            vertex.x = pivot.x + factor * (vertex.x - pivot.x);
            vertex.y = pivot.y + factor * (vertex.y - pivot.y);
            vertex.z = pivot.z + factor * (vertex.z - pivot.z);
            vertex.mark = 1;
        }
        for ( Surface& surface : expectedSector.surfaces )
        {
            surface.normal = level::surfaceCalcNormalNewell(expectedSector, surface);
        }

        const editor::DocumentState committed = signalWindow.documentStateForTest();
        check(assets::sedLevelsEqual(committed.level, expected.level) &&
                  equalNumericMenuSelection(committed.selection, expected.selection),
              "Valid viewport Sector Scale publishes the fixed authored geometry and retains selection");
        check(signalWindow.undoDepthForTest() == undoDepth + 1u &&
                  signalWindow.redoDepthForTest() == 0u,
              "Valid viewport Sector Scale commits exactly one history row and clears Redo");
        check(signalWindow.undoForTest() &&
                  equalTransformDocumentState(signalWindow.documentStateForTest(), before),
              "Viewport Sector Scale Undo restores the complete original Level and selection");
        check(signalWindow.redoForTest() &&
                  equalTransformDocumentState(signalWindow.documentStateForTest(), committed),
              "Viewport Sector Scale Redo restores the exact committed Level and selection");
    };

    runSectorScaleSignalCase(true, true, false, 0.01,
                             TVector{1.0e16 + 4.0, 1.0e16 + 4.0, 1.0e16 + 4.0});
    runSectorScaleSignalCase(false, false, false, 2.0, TVector{});
    runSectorScaleSignalCase(false, false, true, 2.0, TVector{});

    {
        editor::DocumentState state = numericOctahedronSurfaceScaleFixture();
        check(state.level.sectors.size() == 1 && state.selection.surfaces.contains(SurfaceRef{0, 0}),
              "Viewport Surface Scale fixture selects the octahedron's first face");
        if ( !state.level.sectors.empty() )
        {
            check(level::sectorIsConvex(state.level.sectors[0]),
                  "Viewport Surface Scale fixture starts with a strictly convex solid");
        }

        MainWindow signalWindow;
        signalWindow.setAttribute(Qt::WA_DontShowOnScreen);
        signalWindow.setDocumentStateForTest(state);
        signalWindow.activateSurfaceEditModeForTest();
        GlView* const view = signalWindow.glViewForTest();
        check(view != nullptr, "Viewport Surface Scale fixture exposes its viewport");
        if ( !view || state.level.sectors.empty() )
        {
            return;
        }
        view->setSelectedSurface(0, 0);
        view->setSelectedThing(0);
        view->setPreserveConnectedSurfaceUvsEnabled(false);
        signalWindow.mutableDocumentStateForTest().selection.surfaces = {SurfaceRef{0, 0}};
        signalWindow.mutableDocumentStateForTest().selection.things = {0};

        signalWindow.pushUndoForTitleForTest(editor::DocumentDirty::LevelGeometry);
        signalWindow.mutableDocumentStateForTest().level.sectors[0].ambient =
            ColorF{0.125f, 0.25f, 0.5f, 1.0f};
        check(signalWindow.undoForTest(), "Viewport Surface Scale fixture seeds retained Redo history");
        const editor::DocumentState before = signalWindow.documentStateForTest();
        const std::size_t undoDepth = signalWindow.undoDepthForTest();
        const std::size_t redoDepth = signalWindow.redoDepthForTest();

        view->surfacesScaleRequested(QVector3D{}, 1.5, TVector{25.25, 0.25, 0.25});
        check(equalTransformDocumentState(signalWindow.documentStateForTest(), before) &&
                  signalWindow.undoDepthForTest() == undoDepth &&
                  signalWindow.redoDepthForTest() == redoDepth,
              "Rejected viewport Surface Scale preserves full Level, selection, Undo, and Redo");
    }

    {
        MainWindow mirrorWindow;
        mirrorWindow.setAttribute(Qt::WA_DontShowOnScreen);
        QAction* const mirrorAction = mirrorWindow.findChild<QAction*>(QStringLiteral("transform.mirror"));
        GlView* const mirrorView = mirrorWindow.glViewForTest();
        check(mirrorAction && mirrorView, "Thing Mirror fixture exposes the real menu action and viewport");
        if ( !mirrorAction || !mirrorView )
        {
            return;
        }
        editor::DocumentState state;
        state.level.clear();
        state.level.kind = ProjectType::JKDF2;
        state.level.layers = {"Layer0"};
        const std::int32_t left = level::levelCreateSectorPrimitiveBox(
            state.level, level::SectorPrimitive::Cube,
            TVector{-5.0, -1.0, -1.0}, TVector{-3.0, 1.0, 1.0});
        const std::int32_t right = level::levelCreateSectorPrimitiveBox(
            state.level, level::SectorPrimitive::Cube,
            TVector{3.0, -1.0, -1.0}, TVector{5.0, 1.0, 1.0});
        check(left == 0 && right == 1, "Thing Mirror fixture creates two valid independent rooms");
        for ( const int side : {0, 1} )
        {
            Thing thing;
            thing.name = "__mirror_missing_template__";
            thing.x = side == 0 ? -4.0 : 4.0;
            thing.y = side == 0 ? 0.25 : -0.25;
            thing.pch = 12.5;
            thing.yaw = -27.25;
            thing.rol = 3.75;
            thing.sector = side == 0 ? left : right;
            state.level.things.push_back(thing);
        }
        state.selection.things = {0, 1};
        for ( const bool cancel : {true, false} )
        {
            mirrorWindow.setDocumentStateForTest(state);
            mirrorWindow.activateThingEditModeForTest();
            mirrorView->setSelectedThing(0);
            mirrorWindow.mutableDocumentStateForTest().selection.things = {0, 1};
            mirrorWindow.updateEditActionsForTest();
            mirrorWindow.pushUndoForTitleForTest(editor::DocumentDirty::Things);
            mirrorWindow.mutableDocumentStateForTest().level.things[0].z = 123.0;
            check(mirrorWindow.undoForTest(), "Thing Mirror fixture seeds retained Redo history");
            const auto before = mirrorWindow.documentStateForTest();
            const std::size_t undoDepth = mirrorWindow.undoDepthForTest();
            const std::size_t redoDepth = mirrorWindow.redoDepthForTest();
            int prompts = 0;
            bool unexpectedMessage = false;
            QTimer respond;
            respond.setInterval(5);
            QObject::connect(&respond, &QTimer::timeout, &respond, [&]
            {
                if ( auto* input = qobject_cast<QInputDialog*>(QApplication::activeModalWidget()) )
                {
                    ++prompts;
                    check(input->labelText() == QStringLiteral("Axis")
                            && input->textValue() == QStringLiteral("X"),
                        "Thing Mirror opens its actual X-axis choice");
                    if ( cancel )
                    {
                        input->reject();
                    }
                    else
                    {
                        input->accept();
                    }
                }
                else if ( auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) )
                {
                    unexpectedMessage = true;
                    message->reject();
                }
            });
            respond.start();
            mirrorAction->trigger();
            respond.stop();
            const auto committed = mirrorWindow.documentStateForTest();
            check(prompts == 1 && !unexpectedMessage, "Thing Mirror traverses exactly one real axis prompt");
            if ( cancel )
            {
                check(assets::sedLevelsEqual(committed.level, before.level)
                        && equalNumericMenuSelection(committed.selection, before.selection)
                        && mirrorWindow.undoDepthForTest() == undoDepth
                        && mirrorWindow.redoDepthForTest() == redoDepth,
                    "Canceled Thing Mirror preserves the complete Level, selection, Undo, and Redo");
                continue;
            }
            Level expected = before.level;
            expected.things[0].x = 4.0;
            expected.things[0].sector = right;
            expected.things[1].x = -4.0;
            expected.things[1].sector = left;
            check(assets::sedLevelsEqual(committed.level, expected)
                    && equalNumericMenuSelection(committed.selection, editor::Selection{})
                    && mirrorWindow.undoDepthForTest() == undoDepth + 1u
                    && mirrorWindow.redoDepthForTest() == 0u,
                "Real Thing Mirror swaps room ownership, preserves other authored fields, and clears selection in one Undo row");
            check(mirrorWindow.undoForTest()
                    && assets::sedLevelsEqual(mirrorWindow.documentStateForTest().level, before.level)
                    && equalNumericMenuSelection(mirrorWindow.documentStateForTest().selection, before.selection),
                "Thing Mirror Undo restores exact poses, containing Sectors, and original selection");
            check(mirrorWindow.redoForTest()
                    && assets::sedLevelsEqual(mirrorWindow.documentStateForTest().level, committed.level)
                    && equalNumericMenuSelection(mirrorWindow.documentStateForTest().selection, committed.selection),
                "Thing Mirror Redo restores exact mirrored poses, containing Sectors, and committed selection");
        }
    }
}
} // namespace sed::app_tests
