#include "test_common.hpp"

#include <QApplication>
#include <QAction>
#include <QSettings>
#include <QStatusBar>
#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "app_settings.hpp"
#include "sed/assets/sed.hpp"
#include "sed/level/build.hpp"

#include <QVector3D>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QMetaObject>
#include <QToolButton>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>

namespace sed::app_tests
{

namespace
{

class ScopedThingPlacementSettings final
{
public:
    ScopedThingPlacementSettings()
    {
        QSettings settings = settingsStore();
        const QString prefix = QStringLiteral("thing_placement/");
        for (const QString& key : settings.allKeys())
        {
            if (key.startsWith(prefix))
            {
                values_.insert(key, settings.value(key));
            }
        }
        settings.remove(QStringLiteral("thing_placement"));
        settings.sync();
    }

    ~ScopedThingPlacementSettings()
    {
        QSettings settings = settingsStore();
        settings.remove(QStringLiteral("thing_placement"));
        for (auto it = values_.cbegin(); it != values_.cend(); ++it)
        {
            settings.setValue(it.key(), it.value());
        }
        settings.sync();
    }

    ScopedThingPlacementSettings(const ScopedThingPlacementSettings&) = delete;
    ScopedThingPlacementSettings& operator=(const ScopedThingPlacementSettings&) = delete;

private:
    QMap<QString, QVariant> values_{};
};

struct PlacementSample
{
    float sourceZ{};
    std::int32_t expectedSector{-1};
};

Level makeStackedPlacementLevel()
{
    Level level{};
    level.kind = ProjectType::IJIM;
    level.layers = { "Layer0" };
    for (const double z : { 0.0, 1.0, 2.0 })
    {
        sed::level::levelCreateCube(level, 0.0, 0.0, z, TVector::up, TVector::right);
    }
    return level;
}

std::int32_t linearContainingSector(
    const Level& level,
    const TVector& point)
{
    for (std::size_t index = 0; index < level.sectors.size(); ++index)
    {
        const Sector& sector = level.sectors[index];
        if (sed::level::sectorContainsPoint(sector, point.x, point.y, point.z))
        {
            return static_cast<std::int32_t>(index);
        }
    }
    return -1;
}

} // namespace

void runThingPlacementBehaviorTests()
{
    const ScopedThingPlacementSettings placementSettings{};
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);

    editor::DocumentState state{};
    state.level = makeStackedPlacementLevel();
    window.setDocumentStateForTest(std::move(state));

    const Level& level = window.documentStateForTest().level;
    GlView* const view = window.glViewForTest();
    check(view != nullptr && level.sectors.size() == 3u,
        "thing placement sector fixture publishes three adjacent indexed cubes");

    constexpr double kSupportOffset = 0.2;
    const std::array samples{
        PlacementSample{ 0.1f, 0 },
        PlacementSample{ 0.9f, 1 },
        PlacementSample{ 1.9f, 2 },
        PlacementSample{ 2.9f, -1 },
    };

    for (const PlacementSample& sample : samples)
    {
        const Thing placed = window.thingPlacementPoseForTest(
            QVector3D(0.5f, 0.5f, sample.sourceZ), kSupportOffset);
        const TVector expectedPoint{
            0.5,
            0.5,
            static_cast<double>(sample.sourceZ) + kSupportOffset};
        const std::int32_t linearSector = linearContainingSector(
            level, expectedPoint);
        const std::int32_t indexedSector = view
            ? view->findContainingSector(expectedPoint, 0)
            : -1;
        check(linearSector == sample.expectedSector,
            "thing placement fixture has an unambiguous fixed final-point Sector oracle");
        check(indexedSector == sample.expectedSector,
            "thing placement indexed containment resolves each final brush sample");
        check(placed.x == expectedPoint.x
                && placed.y == expectedPoint.y
                && placed.z == expectedPoint.z,
            "thing placement preserves the exact final point after applying the support offset");
        check(placed.sector == sample.expectedSector,
            "thing placement rechecks the preferred Sector after support offset and assigns the final containing Sector");
        if (sample.expectedSector >= 0)
        {
            check(sed::level::sectorContainsPoint(
                      level.sectors[static_cast<std::size_t>(sample.expectedSector)],
                      placed.x,
                      placed.y,
                      placed.z),
                "thing placement stores a point inside the assigned final Sector");
        }
        else
        {
            bool insideAny = false;
            for (const Sector& sector : level.sectors)
            {
                insideAny = insideAny || sed::level::sectorContainsPoint(
                    sector, placed.x, placed.y, placed.z);
            }
            check(!insideAny && placed.sector < 0,
                "thing placement rejects an offset point outside every Sector");
        }
    }

    MainWindow viewportWindow{};
    viewportWindow.setAttribute(Qt::WA_DontShowOnScreen);
    viewportWindow.resize(640, 480);
    editor::DocumentState viewportState{};
    viewportState.level.kind = ProjectType::IJIM;
    viewportState.level.layers = { "Layer0" };
    ThingTemplate placementTemplate{};
    placementTemplate.name = "+placement_probe";
    viewportState.level.templates.push_back(placementTemplate);
    level::levelCreateCube(viewportState.level, 0.0, 0.0, 0.0, TVector::up, TVector::right);
    viewportWindow.setDocumentStateForTest(std::move(viewportState));
    viewportWindow.activateThingEditModeForTest();
    viewportWindow.setThingPlacementTemplateForTest("+placement_probe", 0.0);
    viewportWindow.setThingPlacementActiveForTest(true);
    viewportWindow.show();
    QApplication::processEvents();
    GlView* const viewport = viewportWindow.glViewForTest();
    check(viewport != nullptr,
        "Thing placement viewport fixture creates the real viewport");
    if (viewport)
    {
        viewport->resize(640, 480);
        viewport->setLevel(
            &viewportWindow.mutableDocumentStateForTest().level,
            GlView::SetLevelMode::resetView);
        viewport->fitViewToLevel();
        viewport->setCameraOrbitDistance(8.0f);
        QToolButton* placementSurfaceButton = nullptr;
        QToolButton* placementAlignButton = nullptr;
        for (QToolButton* button : viewportWindow.findChildren<QToolButton*>(
                 QStringLiteral("thingPlacementSegmentButton")))
        {
            if (button->text() == QStringLiteral("Surface"))
            {
                placementSurfaceButton = button;
            }
            else if (button->text() == QStringLiteral("Align"))
            {
                placementAlignButton = button;
            }
        }
        QCheckBox* snapToThing = nullptr;
        QCheckBox* rotateToAlign = nullptr;
        for (QCheckBox* checkBox : viewportWindow.findChildren<QCheckBox*>(
                 QStringLiteral("thingPlacementOptionCheck")))
        {
            if (checkBox->text() == QStringLiteral("Snap to Thing"))
            {
                snapToThing = checkBox;
            }
            else if (checkBox->text() == QStringLiteral("Rotate to Align"))
            {
                rotateToAlign = checkBox;
            }
        }
        const bool priorSnapToThing = snapToThing && snapToThing->isChecked();
        const bool priorRotateToAlign = rotateToAlign && rotateToAlign->isChecked();
        check(placementSurfaceButton && placementAlignButton
                && snapToThing && rotateToAlign,
            "Thing placement fixture exposes Surface/Align modes and Snap to Thing/Rotate to Align controls");
        if (placementAlignButton)
        {
            placementAlignButton->click();
        }
        if (snapToThing)
        {
            snapToThing->setChecked(true);
        }
        if (rotateToAlign)
        {
            rotateToAlign->setChecked(true);
        }
        check(placementAlignButton && placementAlignButton->isChecked()
                && snapToThing && snapToThing->isChecked()
                && rotateToAlign && rotateToAlign->isChecked(),
            "Thing placement fixture enables aligned surface placement and both placement snap options");
        const std::vector<std::byte> levelBefore = assets::saveToSedBytes(
            viewportWindow.documentStateForTest().level);
        const std::size_t undoBefore = viewportWindow.undoDepthForTest();

        const std::array<std::tuple<const char*, float, float, bool>, 4> views{
            std::tuple{ "top orthographic", 0.0f, 90.0f, false },
            std::tuple{ "side orthographic", 90.0f, 0.0f, false },
            std::tuple{ "diagonal-down orthographic", 32.0f, 42.0f, false },
            std::tuple{ "perspective", 32.0f, 42.0f, true },
        };
        for (const auto& [label, yaw, pitch, perspective] : views)
        {
            viewport->setCameraProjectionType(
                perspective
                    ? app::ViewportCamera::ProjectionType::Perspective
                    : app::ViewportCamera::ProjectionType::Orthographic);
            viewport->setOrbitViewImmediate(yaw, pitch, 0.0f);
            bool haveSurface = false;
            SurfaceRef surface{};
            TVector documentPoint{};
            const std::optional<QVector3D> point =
                viewport->placementPointForViewport(
                    QPoint(320, 240), &haveSurface, &surface, nullptr, true, false,
                    &documentPoint);
            const std::optional<Thing> pose =
                viewportWindow.thingPlacementTargetPoseForTest(QPoint(320, 240));
            if (!point || !haveSurface || !pose)
            {
                std::cerr << "ThingPlacement view=" << label
                    << " point=" << (point ? "yes" : "no")
                    << " doc=" << documentPoint.x << ',' << documentPoint.y
                    << ',' << documentPoint.z
                    << " pose=" << (pose ? "yes" : "no")
                    << " surface=" << surface.sector << ':' << surface.surface
                    << '\n';
            }
            check(point.has_value() && haveSurface && pose.has_value()
                    && pose->sector == 0 && pose->layer == 0
                    && pose->name == "+placement_probe"
                    && surface.sector == 0
                    && surface.surface >= 0
                    && static_cast<std::size_t>(surface.surface)
                        < viewportWindow.documentStateForTest().level.sectors[0].surfaces.size()
                    && pose->x == documentPoint.x
                    && pose->y == documentPoint.y
                    && pose->z == documentPoint.z,
                label);
        }
        check(assets::saveToSedBytes(viewportWindow.documentStateForTest().level)
                == levelBefore
                && viewportWindow.undoDepthForTest() == undoBefore,
            "Thing placement target preview leaves Level and history unchanged");
        if (snapToThing)
        {
            snapToThing->setChecked(priorSnapToThing);
        }
        if (rotateToAlign)
        {
            rotateToAlign->setChecked(priorRotateToAlign);
        }

    }

    MainWindow brushWindow{};
    brushWindow.setAttribute(Qt::WA_DontShowOnScreen);
    brushWindow.resize(800, 600);
    editor::DocumentState brushState{};
    brushState.level.kind = ProjectType::IJIM;
    brushState.level.layers = { "Lower", "Upper" };
    ThingTemplate brushTemplate{};
    brushTemplate.name = "+placement_probe";
    brushState.level.templates.push_back(brushTemplate);
    level::levelCreateCube(brushState.level, 0.0, 0.0, 0.0, TVector::up, TVector::right).layer = 0;
    level::levelCreateCube(brushState.level, 1.0, 0.0, 0.0, TVector::up, TVector::right).layer = 1;
    brushWindow.setDocumentStateForTest(std::move(brushState));
    brushWindow.activateThingEditModeForTest();
    brushWindow.setThingPlacementTemplateForTest("+placement_probe", 0.0);
    brushWindow.setThingPlacementActiveForTest(true);
    brushWindow.show();
    QApplication::processEvents();
    GlView* const brushView = brushWindow.glViewForTest();
    check(brushView != nullptr,
        "Thing placement brush fixture creates the real viewport");
    if (brushView)
    {
        brushView->resize(800, 600);
        brushView->setLevel(
            &brushWindow.mutableDocumentStateForTest().level,
            GlView::SetLevelMode::resetView);
        brushView->fitViewToLevel();
        brushView->setCameraProjectionType(
            app::ViewportCamera::ProjectionType::Orthographic);
        brushView->setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
        brushView->setCameraOrbitDistance(8.0f);

        brushWindow.configureThingPlacementBrushForTest(16, 0.6, 0.1);
        const auto screenPoint = [brushView](const QVector3D& world)
        {
            const std::optional<QPointF> projected =
                brushView->projectWorldToViewport(world);
            return projected
                ? std::optional<QPoint>{QPoint(qRound(projected->x()), qRound(projected->y()))}
                : std::nullopt;
        };
        const std::optional<QPoint> boundaryPoint =
            screenPoint(QVector3D(1.0f, 0.5f, 0.5f));
        const std::optional<QPoint> outsidePoint =
            screenPoint(QVector3D(8.0f, 8.0f, 0.5f));
        if (boundaryPoint)
        {
            const std::vector<std::byte> previewBefore = assets::saveToSedBytes(
                brushWindow.documentStateForTest().level);
            brushWindow.resetThingPlacementTemplateRevisionChecksForTest();
            brushWindow.updateThingPlacementPreviewForTest(*boundaryPoint);
            const std::uint64_t firstEventChecks =
                brushWindow.thingPlacementTemplateRevisionChecksForTest();
            const std::size_t firstEventPreviewCount =
                brushView->placementPreviewRenderedThingCountForTest();
            std::cerr << "ThingPlacementHotPath event=1 revisionChecks="
                << firstEventChecks << " previewCount=" << firstEventPreviewCount << '\n';
            check(firstEventChecks == 1u && firstEventPreviewCount > 1u,
                "Thing placement brush preview checks template resource revisions once per event, independent of brush sample count");
            brushWindow.resetThingPlacementTemplateRevisionChecksForTest();
            brushWindow.updateThingPlacementPreviewForTest(*boundaryPoint);
            const std::uint64_t secondEventChecks =
                brushWindow.thingPlacementTemplateRevisionChecksForTest();
            const std::size_t secondEventPreviewCount =
                brushView->placementPreviewRenderedThingCountForTest();
            std::cerr << "ThingPlacementHotPath event=2 revisionChecks="
                << secondEventChecks << " previewCount=" << secondEventPreviewCount << '\n';
            check(secondEventChecks == 1u
                    && secondEventPreviewCount == firstEventPreviewCount
                    && assets::saveToSedBytes(brushWindow.documentStateForTest().level)
                        == previewBefore,
                "Thing placement preview refreshes its revision snapshot on the next event while preserving authored Level bytes");
        }
        const std::vector<std::byte> brushBefore = assets::saveToSedBytes(
            brushWindow.documentStateForTest().level);
        check(boundaryPoint.has_value() && outsidePoint.has_value(),
            "Thing brush fixture projects boundary and outside points into the viewport");
        if (boundaryPoint)
        {
            const std::optional<Thing> initialPose =
                brushWindow.thingPlacementTargetPoseForTest(*boundaryPoint);
            if (!initialPose)
            {
                std::cerr << "ThingBrush initial target unavailable at widget="
                    << boundaryPoint->x() << ',' << boundaryPoint->y()
                    << " template=+placement_probe mode=brush\n";
            }
        }
        const bool boundaryPlaced = boundaryPoint
            && brushWindow.stampThingPlacementBrushForTest(*boundaryPoint);
        const bool outsideRejected = outsidePoint
            && !brushWindow.stampThingPlacementBrushForTest(*outsidePoint);
        const bool finalized = brushWindow.finalizeThingPlacementBrushForTest();
        const Level& brushLevel = brushWindow.documentStateForTest().level;
        const std::vector<std::byte> brushPlaced = assets::saveToSedBytes(brushLevel);
        bool sawLower = false;
        bool sawUpper = false;
        bool everyPlacementValid = true;
        for (const Thing& placed : brushLevel.things)
        {
            sawLower = sawLower || (placed.sector == 0 && placed.layer == 0);
            sawUpper = sawUpper || (placed.sector == 1 && placed.layer == 1);
            const std::int32_t expectedSector = placed.x < 1.0
                ? 0 : (placed.x > 1.0 ? 1 : placed.sector);
            const bool insideExpectedSector = placed.sector >= 0
                && static_cast<std::size_t>(placed.sector)
                    < brushLevel.sectors.size()
                && sed::level::sectorContainsPoint(
                    brushLevel.sectors[static_cast<std::size_t>(placed.sector)],
                    placed.x, placed.y, placed.z);
            everyPlacementValid = everyPlacementValid
                && std::isfinite(placed.x) && std::isfinite(placed.y)
                && std::isfinite(placed.z)
                && placed.x >= 0.0 && placed.x <= 2.0
                && placed.y >= 0.0 && placed.y <= 1.0
                && (placed.x == 1.0 || placed.sector == expectedSector)
                && placed.layer == placed.sector
                && insideExpectedSector;
            if (!insideExpectedSector || !std::isfinite(placed.x)
                || !std::isfinite(placed.y) || !std::isfinite(placed.z)
                || placed.x < 0.0 || placed.x > 2.0
                || placed.y < 0.0 || placed.y > 1.0
                || (placed.x != 1.0 && placed.sector != expectedSector)
                || placed.layer != placed.sector)
            {
                std::cerr.precision(17);
                std::cerr << "ThingBrush invalid pose=" << placed.x << ','
                    << placed.y << ',' << placed.z << " sector="
                    << placed.sector << " layer=" << placed.layer
                    << " expectedSector=" << expectedSector << '\n';
            }
        }
        if (!(boundaryPlaced && outsideRejected && finalized
                && brushLevel.things.size() >= 8u
                && sawLower && sawUpper && everyPlacementValid
                && brushWindow.undoDepthForTest() == 1u))
        {
            std::cerr << "ThingBrush result boundary=" << boundaryPlaced
                << " outsideRejected=" << outsideRejected
                << " finalized=" << finalized
                << " count=" << brushLevel.things.size()
                << " undo=" << brushWindow.undoDepthForTest()
                << " sawLower=" << sawLower << " sawUpper=" << sawUpper
                << " everyValid=" << everyPlacementValid << '\n';
        }
        check(boundaryPlaced && outsideRejected && finalized
                && brushLevel.things.size() >= 8u
                && sawLower && sawUpper && everyPlacementValid
                && brushWindow.undoDepthForTest() == 1u,
            "Thing brush radius crossing adjacent sectors assigns accepted samples to final Sector and layer while rejecting outside samples");
        check(brushPlaced != brushBefore
                && brushWindow.undoForTest()
                && brushWindow.documentStateForTest().level.things.empty()
                && assets::saveToSedBytes(brushWindow.documentStateForTest().level)
                    == brushBefore
                && brushWindow.undoDepthForTest() == 0u,
            "Thing brush crossing-sector stroke uses one Undo transaction to remove every accepted stamp");
        check(brushWindow.redoForTest()
                && brushWindow.documentStateForTest().level.things.size() >= 8u
                && assets::saveToSedBytes(brushWindow.documentStateForTest().level)
                    == brushPlaced,
            "Thing brush crossing-sector stroke Redo restores the exact full placed state");

    }

    for (const int brushCount : std::array<int, 4>{ 1, 64, 65, 256 })
    {
        const std::string countLabel = std::to_string(brushCount);
        // Keep this fixture deliberately lightweight: the brush samples an
        // ordinary template with no model or sprite resource, so a maximum-
        // count cursor update measures placement and preview bookkeeping only.
        MainWindow countWindow{};
        countWindow.setAttribute(Qt::WA_DontShowOnScreen);
        countWindow.resize(800, 600);
        editor::DocumentState countState{};
        countState.level.kind = ProjectType::IJIM;
        countState.level.layers = { "Layer0" };
        ThingTemplate countTemplate{};
        countTemplate.name = "+placement_probe";
        countState.level.templates.push_back(countTemplate);
        sed::level::levelCreateCube(
            countState.level, 0.0, 0.0, 0.0, TVector::up, TVector::right);
        countWindow.setDocumentStateForTest(std::move(countState));
        countWindow.activateThingEditModeForTest();
        countWindow.setThingPlacementTemplateForTest("+placement_probe", 0.0);
        countWindow.setThingPlacementActiveForTest(true);
        countWindow.show();
        QApplication::processEvents();

        GlView* const countView = countWindow.glViewForTest();
        check(countView != nullptr,
            "Thing brush count fixture creates a lightweight viewport");
        if (!countView)
        {
            continue;
        }
        countView->resize(800, 600);
        countView->setLevel(
            &countWindow.mutableDocumentStateForTest().level,
            GlView::SetLevelMode::resetView);
        countView->fitViewToLevel();
        countView->setCameraProjectionType(
            app::ViewportCamera::ProjectionType::Orthographic);
        countView->setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
        countView->setCameraOrbitDistance(8.0f);
        countWindow.configureThingPlacementBrushForTest(brushCount, 0.25, 0.1);
        constexpr double kFixedBrushAngle = 37.5;
        if (brushCount == 1)
        {
            QWidget* const brushOptions = countWindow.findChild<QWidget*>(
                QStringLiteral("thingPlacementBrushOptions"));
            QCheckBox* randomRotate = nullptr;
            QCheckBox* rotatePitch = nullptr;
            QCheckBox* rotateYaw = nullptr;
            QCheckBox* rotateRoll = nullptr;
            QDoubleSpinBox* fixedAngle = nullptr;
            if (brushOptions)
            {
                for (QCheckBox* checkBox : brushOptions->findChildren<QCheckBox*>())
                {
                    if (checkBox->text() == QStringLiteral("Random rotate"))
                    {
                        randomRotate = checkBox;
                    }
                    else if (checkBox->text() == QStringLiteral("Pitch"))
                    {
                        rotatePitch = checkBox;
                    }
                    else if (checkBox->text() == QStringLiteral("Yaw"))
                    {
                        rotateYaw = checkBox;
                    }
                    else if (checkBox->text() == QStringLiteral("Roll"))
                    {
                        rotateRoll = checkBox;
                    }
                }
                for (QDoubleSpinBox* spin : brushOptions->findChildren<QDoubleSpinBox*>())
                {
                    if (spin->minimum() == -360.0 && spin->maximum() == 360.0)
                    {
                        fixedAngle = spin;
                        break;
                    }
                }
            }
            const bool haveFixedRotationControls = randomRotate && rotatePitch
                && rotateYaw && rotateRoll && fixedAngle;
            check(haveFixedRotationControls,
                "Count 1 brush fixture exposes fixed rotation controls");
            if (haveFixedRotationControls)
            {
                randomRotate->setChecked(false);
                rotatePitch->setChecked(false);
                rotateYaw->setChecked(false);
                rotateRoll->setChecked(false);
                rotatePitch->setChecked(true);
                rotateYaw->setChecked(true);
                rotateRoll->setChecked(true);
                fixedAngle->setValue(kFixedBrushAngle);
            }
        }

        const std::optional<QPointF> projectedCenter =
            countView->projectWorldToViewport(QVector3D(0.5f, 0.5f, 1.0f));
        check(projectedCenter.has_value(),
            "Thing brush count fixture projects its fixed surface sample");
        if (!projectedCenter)
        {
            continue;
        }
        const QPoint centerPoint{
            qRound(projectedCenter->x()), qRound(projectedCenter->y()) };

        const std::uint64_t modelReadsBefore =
            countView->model3doResourceReadCountForTest();
        const std::uint64_t materialReadsBefore =
            countView->materialResourceReadCountForTest();
        countWindow.resetThingPlacementTemplateRevisionChecksForTest();
        countWindow.updateThingPlacementPreviewForTest(centerPoint);
        const std::size_t previewCount =
            countView->placementPreviewRenderedThingCountForTest();
        std::vector<TVector> previewPositions{};
        previewPositions.reserve(previewCount);
        for (std::size_t index = 0; index < previewCount; ++index)
        {
            const std::optional<TVector> position =
                countView->placementPreviewRenderedThingPositionForTest(index);
            if (position)
            {
                previewPositions.push_back(*position);
            }
        }
        const std::string previewCountLabel =
            "Thing brush preview shows every requested sample at Count " + countLabel;
        check(previewCount == static_cast<std::size_t>(brushCount)
                && previewPositions.size() == previewCount,
            previewCountLabel.c_str());
        const std::string revisionCheckLabel =
            "Thing brush preview validates template resources once per event at Count "
            + countLabel;
        check(countWindow.thingPlacementTemplateRevisionChecksForTest() == 1u,
            revisionCheckLabel.c_str());

        // A one-pixel cursor move must update all 256 lightweight preview
        // Things through the cached ranges without rebuilding the persistent
        // Thing mesh or rereading template assets.
        const std::uint64_t meshRebuildsBeforeMove =
            countView->meshRebuildCountForTest();
        const std::uint64_t fastUpdatesBeforeMove =
            countView->placementPreviewFastUpdateCountForTest();
        countWindow.resetThingPlacementTemplateRevisionChecksForTest();
        countWindow.updateThingPlacementPreviewForTest(
            QPoint(centerPoint.x() + 1, centerPoint.y()));
        const std::string cachedUpdateLabel =
            "Thing brush cursor update at Count " + countLabel
            + " uses cached preview ranges without repeated resource reads";
        check(countWindow.thingPlacementTemplateRevisionChecksForTest() == 1u
                && countView->meshRebuildCountForTest() == meshRebuildsBeforeMove
                && countView->placementPreviewFastUpdateCountForTest()
                    == fastUpdatesBeforeMove + 1u
                && countView->model3doResourceReadCountForTest() == modelReadsBefore
                && countView->materialResourceReadCountForTest() == materialReadsBefore,
            cachedUpdateLabel.c_str());

        const bool committed =
            countWindow.stampThingPlacementBrushForTest(centerPoint);
        const bool finalized = countWindow.finalizeThingPlacementBrushForTest();
        const Level& placedLevel = countWindow.documentStateForTest().level;
        const std::string commitCountLabel =
            "Thing brush commits every requested sample at Count " + countLabel;
        check(committed && finalized
                && placedLevel.things.size() == static_cast<std::size_t>(brushCount),
            commitCountLabel.c_str());
        const bool fixedRotationMatches = brushCount != 1
            || (placedLevel.things.size() == 1u
                && placedLevel.things.front().pch == kFixedBrushAngle
                && placedLevel.things.front().yaw == kFixedBrushAngle
                && placedLevel.things.front().rol == kFixedBrushAngle);
        if (brushCount == 1)
        {
            check(fixedRotationMatches,
                "Count 1 fixed Brush angle commits exact pitch, yaw, and roll");
        }

        bool everySampleMatches = previewPositions.size() == placedLevel.things.size();
        const std::size_t comparedCount = std::min(
            previewPositions.size(), placedLevel.things.size());
        for (std::size_t index = 0; index < comparedCount; ++index)
        {
            const Thing& placed = placedLevel.things[index];
            const TVector committedPosition{ placed.x, placed.y, placed.z };
            everySampleMatches = everySampleMatches
                && previewPositions[index].x == committedPosition.x
                && previewPositions[index].y == committedPosition.y
                && previewPositions[index].z == committedPosition.z;
        }
        const std::string sampleVectorLabel =
            "Thing brush preview and commit use the same complete fixed sample vector at Count "
            + countLabel;
        check(everySampleMatches, sampleVectorLabel.c_str());
    }

    for (const auto& cameraCase : std::array<std::tuple<float, float, bool, double, double, double>, 4>{
             std::tuple{0.0f, 90.0f, false, 0.625, 0.5, 0.5},
             std::tuple{90.0f, 0.0f, false, 0.5, 0.625, 0.5},
             std::tuple{35.0f, 42.0f, false, 0.625, 0.5625, 0.5},
             std::tuple{35.0f, 42.0f, true, 0.625, 0.5625, 0.5}})
    {
        const auto [yaw, pitch, perspective, expectedX, expectedY, expectedZ] = cameraCase;
        MainWindow moveWindow{};
        moveWindow.setAttribute(Qt::WA_DontShowOnScreen);
        editor::DocumentState state{};
        state.level.kind = ProjectType::IJIM;
        state.level.layers = {"Layer0"};
        sed::level::levelCreateCube(state.level, 0.0, 0.0, 0.0, TVector::up, TVector::right);
        ThingTemplate moveTemplate{};
        moveTemplate.name = "+placement_probe";
        state.level.templates.push_back(moveTemplate);
        Thing moving{};
        moving.templateName = moveTemplate.name;
        moving.x = 0.5;
        moving.y = 0.5;
        moving.z = 0.5;
        moving.sector = 0;
        state.level.things.push_back(moving);
        state.selection.things.insert(0);
        moveWindow.setDocumentStateForTest(std::move(state));
        moveWindow.activateThingEditModeForTest();
        moveWindow.mutableDocumentStateForTest().selection.things.insert(0);
        moveWindow.updateEditActionsForTest();
        GlView* const view = moveWindow.glViewForTest();
        const std::vector<std::byte> before = assets::saveToSedBytes(
            moveWindow.documentStateForTest().level);
        const std::size_t undoBefore = moveWindow.undoDepthForTest();
        check(view != nullptr, "Thing Move camera fixture creates a viewport");
        if (!view)
        {
            continue;
        }
        view->resize(800, 600);
        view->setLevel(&moveWindow.mutableDocumentStateForTest().level,
            GlView::SetLevelMode::resetView);
        view->fitViewToLevel();
        view->setSelectedThing(0);
        view->setTransformGridSnapEnabled(true);
        view->setThingMoveSurfaceStickEnabled(true);
        view->setThingMoveSurfaceAlignEnabled(true);
        view->updateThingMovePointerCameraForTest(
            0.5f, true, true, true, true, yaw, pitch, perspective, 0.125f);
        view->endTransformDragForTest();

        const Thing& result = moveWindow.documentStateForTest().level.things[0];
        const std::vector<std::byte> firstPlaced = assets::saveToSedBytes(
            moveWindow.documentStateForTest().level);
        const bool finite = std::isfinite(result.x) && std::isfinite(result.y)
            && std::isfinite(result.z);
        const bool contained = result.sector >= 0
            && static_cast<std::size_t>(result.sector)
                < moveWindow.documentStateForTest().level.sectors.size()
            && sed::level::sectorContainsPoint(
                moveWindow.documentStateForTest().level.sectors[
                    static_cast<std::size_t>(result.sector)],
                result.x, result.y, result.z);
        // The angled cameras derive the move plane from normalized float camera
        // vectors. Their unsnapped normal component may retain a few 1e-7 world
        // units of round-off; X/Y remain exact grid results and axis-aligned cases stay exact.
        const bool angledCamera = yaw == 35.0f && pitch == 42.0f;
        const bool zMatches = angledCamera
            ? std::abs(result.z - expectedZ) < 1.0e-7
            : result.z == expectedZ;
        if (!(result.x == expectedX && result.y == expectedY && zMatches))
        {
            std::cerr.precision(17);
            std::cerr << "ThingMove camera result yaw=" << yaw << " pitch=" << pitch
                << " perspective=" << perspective << " pose=" << result.x << ','
                << result.y << ',' << result.z << " sector=" << result.sector
                << " expected=" << expectedX << ',' << expectedY << ','
                << expectedZ << '\n';
        }
        check(finite && contained && result.sector == 0
                && result.x == expectedX
                && result.y == expectedY
                && zMatches
                && firstPlaced != before
                && moveWindow.undoDepthForTest() == undoBefore + 1u,
            "Thing Move combined grid/geometry/surface snapping commits a finite in-sector pose for top, side, diagonal, and perspective cameras");
        check(moveWindow.undoForTest()
                && assets::saveToSedBytes(moveWindow.documentStateForTest().level) == before,
            "Thing Move camera case Undo restores the exact pre-drag Level");
        view->updateThingMovePointerCameraForTest(
            0.5f, true, true, true, true, yaw, pitch, perspective, 0.125f);
        view->endTransformDragForTest();
        check(assets::saveToSedBytes(moveWindow.documentStateForTest().level)
                == firstPlaced,
            "Thing Move camera case produces the same snapped document pose on repeat");
    }

}

void runThingPlacementIdExhaustionTests()
{
    const ScopedThingPlacementSettings placementSettings{};
    constexpr std::int32_t maximumId =
        std::numeric_limits<std::int32_t>::max();
    const auto makeState = []
    {
        editor::DocumentState state{};
        state.level = makeStackedPlacementLevel();
        ThingTemplate placementTemplate{};
        placementTemplate.name = "+placement_probe";
        state.level.templates.push_back(std::move(placementTemplate));
        return state;
    };
    const auto clickSinglePlacement = [](
        GlView* const view,
        const QVector3D& hit,
        const SurfaceRef& surface,
        const QPoint& clickPosition)
    {
        const Qt::KeyboardModifiers noModifiers = Qt::NoModifier;
        return view && QMetaObject::invokeMethod(
            view,
            "surfaceClicked",
            Qt::DirectConnection,
            Q_ARG(QVector3D, hit),
            Q_ARG(std::int32_t, surface.sector),
            Q_ARG(std::int32_t, surface.surface),
            Q_ARG(QPoint, clickPosition),
            Q_ARG(Qt::KeyboardModifiers, noModifiers));
    };

    MainWindow ordinarySingleWindow{};
    ordinarySingleWindow.setAttribute(Qt::WA_DontShowOnScreen);
    ordinarySingleWindow.resize(800, 600);
    ordinarySingleWindow.setDocumentStateForTest(makeState());
    ordinarySingleWindow.activateThingEditModeForTest();
    ordinarySingleWindow.setThingPlacementTemplateForTest("+placement_probe", 0.0);
    ordinarySingleWindow.setThingPlacementActiveForTest(true);
    QCheckBox* keepTemplateActive = nullptr;
    for ( QCheckBox* const checkBox : ordinarySingleWindow.findChildren<QCheckBox*>(
             QStringLiteral("thingPlacementOptionCheck")) )
    {
        if (checkBox->text() == QStringLiteral("Active"))
        {
            keepTemplateActive = checkBox;
            break;
        }
    }
    check(keepTemplateActive != nullptr,
        "Thing successive-placement fixture exposes the real Active option");
    if ( keepTemplateActive )
    {
        keepTemplateActive->setChecked(true);
    }
    GlView* const ordinarySingleView = ordinarySingleWindow.glViewForTest();
    check(ordinarySingleView != nullptr,
        "Thing successive-placement fixture owns a hidden viewport");
    if ( ordinarySingleView )
    {
        ordinarySingleView->resize(800, 600);
        ordinarySingleView->setLevel(
            &ordinarySingleWindow.mutableDocumentStateForTest().level,
            GlView::SetLevelMode::resetView);
        ordinarySingleView->fitViewToLevel();
        ordinarySingleView->setCameraProjectionType(
            app::ViewportCamera::ProjectionType::Orthographic);
        ordinarySingleView->setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
        ordinarySingleView->setCameraOrbitDistance(8.0f);

        bool haveSurface = false;
        SurfaceRef surface{};
        TVector documentPosition{};
        const QPoint clickPosition(400, 300);
        const std::optional<QVector3D> hit =
            ordinarySingleView->placementPointForViewport(
                clickPosition,
                &haveSurface,
                &surface,
                nullptr,
                true,
                false,
                &documentPosition);
        check(hit.has_value() && haveSurface,
            "Thing successive-placement fixture resolves a real surface-click target");
        if ( hit && haveSurface )
        {
            ordinarySingleWindow.markCurrentDocumentAsSavedForTest();
            Level& level = ordinarySingleWindow.mutableDocumentStateForTest().level;
            const std::int32_t firstId = level.curId;
            const std::vector<std::byte> before = assets::saveToSedBytes(level);
            const bool firstClick = clickSinglePlacement(
                ordinarySingleView, *hit, surface, clickPosition);
            const std::vector<std::byte> afterFirst = assets::saveToSedBytes(level);
            const bool firstOwnerCorrect = level.things.size() == 1
                && level.things[0].id == firstId
                && level.things[0].num == 0
                && level.curId == firstId + 1
                && ordinarySingleWindow.documentStateForTest().selection.things
                    == std::unordered_set<std::int32_t>{0};
            check(firstClick && firstOwnerCorrect,
                "first ordinary Thing click preserves its allocated ID and destination ordinal");

            const bool secondClick = clickSinglePlacement(
                ordinarySingleView, *hit, surface, clickPosition);
            const std::vector<std::byte> afterSecond = assets::saveToSedBytes(level);
            const bool bothOwnersCorrect = level.things.size() == 2
                && level.things[0].id == firstId && level.things[0].num == 0
                && level.things[1].id == firstId + 1 && level.things[1].num == 1
                && level.curId == firstId + 2
                && ordinarySingleWindow.documentStateForTest().selection.things
                    == std::unordered_set<std::int32_t>{1};
            check(secondClick && bothOwnersCorrect,
                "second ordinary Thing click receives the next ID and destination ordinal while selecting its appended owner");
            check(ordinarySingleWindow.undoForTest()
                    && assets::saveToSedBytes(level) == afterFirst
                    && ordinarySingleWindow.documentStateForTest().selection.things
                        == std::unordered_set<std::int32_t>{0}
                    && ordinarySingleWindow.undoForTest()
                    && assets::saveToSedBytes(level) == before
                    && ordinarySingleWindow.documentStateForTest().selection.things.empty()
                    && ordinarySingleWindow.redoForTest()
                    && assets::saveToSedBytes(level) == afterFirst
                    && ordinarySingleWindow.documentStateForTest().selection.things
                        == std::unordered_set<std::int32_t>{0}
                    && ordinarySingleWindow.redoForTest()
                    && assets::saveToSedBytes(level) == afterSecond
                    && ordinarySingleWindow.documentStateForTest().selection.things
                        == std::unordered_set<std::int32_t>{1},
                "successive Thing clicks Undo and Redo exact owner bytes and selected ordinals");
        }
    }

    MainWindow existingThingBrushWindow{};
    existingThingBrushWindow.setAttribute(Qt::WA_DontShowOnScreen);
    existingThingBrushWindow.resize(800, 600);
    editor::DocumentState existingThingBrushState = makeState();
    Thing& existingThing = existingThingBrushState.level.newThing();
    existingThing.name = "existing_thing";
    existingThing.templateName = "+placement_probe";
    existingThingBrushWindow.setDocumentStateForTest(std::move(existingThingBrushState));
    existingThingBrushWindow.activateThingEditModeForTest();
    existingThingBrushWindow.setThingPlacementTemplateForTest("+placement_probe", 0.0);
    existingThingBrushWindow.setThingPlacementActiveForTest(true);
    GlView* const existingThingBrushView = existingThingBrushWindow.glViewForTest();
    check(existingThingBrushView != nullptr,
        "Thing brush append fixture owns a hidden viewport with an existing Thing");
    if ( existingThingBrushView )
    {
        existingThingBrushView->resize(800, 600);
        existingThingBrushView->setLevel(
            &existingThingBrushWindow.mutableDocumentStateForTest().level,
            GlView::SetLevelMode::resetView);
        existingThingBrushView->fitViewToLevel();
        existingThingBrushView->setCameraProjectionType(
            app::ViewportCamera::ProjectionType::Orthographic);
        existingThingBrushView->setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
        existingThingBrushView->setCameraOrbitDistance(8.0f);
        const std::optional<QPointF> projected =
            existingThingBrushView->projectWorldToViewport(QVector3D(0.5f, 0.5f, 0.5f));
        const std::optional<QPoint> brushPoint = projected
            ? std::optional<QPoint>{QPoint(qRound(projected->x()), qRound(projected->y()))}
            : std::nullopt;
        check(brushPoint.has_value(),
            "Thing multi-brush fixture projects a surface sample beside its existing owner");
        if ( brushPoint )
        {
            existingThingBrushWindow.configureThingPlacementBrushForTest(3, 0.25, 0.1);
            editor::Selection& selection =
                existingThingBrushWindow.mutableDocumentStateForTest().selection;
            selection.things = { 0 };
            existingThingBrushWindow.updateEditActionsForTest();
            existingThingBrushView->setSelectionState(-1, -1, 0, -1, selection);
            check(selection.things == std::unordered_set<std::int32_t>{0},
                "multi-brush baseline selects its existing Thing after edit-mode and brush setup");
            existingThingBrushWindow.markCurrentDocumentAsSavedForTest();
            Level& level = existingThingBrushWindow.mutableDocumentStateForTest().level;
            const std::int32_t firstBrushId = level.curId;
            const Thing originalThing = level.things[0];
            const std::vector<std::byte> before = assets::saveToSedBytes(level);
            const std::size_t undoBefore = existingThingBrushWindow.undoDepthForTest();
            const bool stamped = existingThingBrushWindow.stampThingPlacementBrushForTest(*brushPoint);
            const bool finalized = existingThingBrushWindow.finalizeThingPlacementBrushForTest();
            const std::vector<std::byte> after = assets::saveToSedBytes(level);
            bool appendedOwnersCorrect = level.things.size() == 4
                && level.things[0].id == originalThing.id
                && level.things[0].num == originalThing.num
                && level.things[0].name == originalThing.name
                && level.curId == firstBrushId + 3;
            for ( std::size_t index = 1; index < level.things.size(); ++index )
            {
                appendedOwnersCorrect = appendedOwnersCorrect
                    && level.things[index].id == firstBrushId + static_cast<std::int32_t>(index - 1)
                    && level.things[index].num == static_cast<std::int32_t>(index);
            }
            check(stamped && finalized && appendedOwnersCorrect
                    && existingThingBrushWindow.documentStateForTest().selection.things
                        == std::unordered_set<std::int32_t>{1, 2, 3}
                    && existingThingBrushWindow.undoDepthForTest() == undoBefore + 1,
                "one multi-sample Thing brush appends sequential allocated IDs and destination ordinals after an existing owner");
            check(assets::saveToSedBytes(level) != before
                    && existingThingBrushWindow.undoForTest()
                    && assets::saveToSedBytes(level) == before
                    && existingThingBrushWindow.documentStateForTest().selection.things
                        == std::unordered_set<std::int32_t>{0}
                    && existingThingBrushWindow.redoForTest()
                    && assets::saveToSedBytes(level) == after
                    && existingThingBrushWindow.documentStateForTest().selection.things
                        == std::unordered_set<std::int32_t>{1, 2, 3},
                "multi-sample Thing brush Undo and Redo restore exact bytes and source/appended selection ordinals");
        }
    }

    MainWindow singleWindow{};
    singleWindow.setAttribute(Qt::WA_DontShowOnScreen);
    singleWindow.resize(800, 600);
    singleWindow.setDocumentStateForTest(makeState());
    singleWindow.activateThingEditModeForTest();
    singleWindow.setThingPlacementTemplateForTest("+placement_probe", 0.0);
    singleWindow.setThingPlacementActiveForTest(true);
    GlView* const singleView = singleWindow.glViewForTest();
    check(singleView != nullptr,
        "Thing placement ID exhaustion single fixture owns a hidden viewport");
    if ( singleView )
    {
        singleView->resize(800, 600);
        singleView->setLevel(
            &singleWindow.mutableDocumentStateForTest().level,
            GlView::SetLevelMode::resetView);
        singleView->fitViewToLevel();
        singleView->setCameraProjectionType(
            app::ViewportCamera::ProjectionType::Orthographic);
        singleView->setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
        singleView->setCameraOrbitDistance(8.0f);

        bool haveSurface = false;
        SurfaceRef surface{};
        TVector documentPosition{};
        const QPoint clickPosition(400, 300);
        const std::optional<QVector3D> hit =
            singleView->placementPointForViewport(
                clickPosition,
                &haveSurface,
                &surface,
                nullptr,
                true,
                false,
                &documentPosition);
        check(hit.has_value() && haveSurface,
            "Thing placement ID exhaustion hidden viewport resolves a real single-placement target without showing GL");

        Level& level = singleWindow.mutableDocumentStateForTest().level;
        level.curId = maximumId;
        const std::vector<std::byte> before = assets::saveToSedBytes(level);
        const std::size_t undoBefore = singleWindow.undoDepthForTest();
        const std::size_t redoBefore = singleWindow.redoDepthForTest();
        const bool clicked = hit && haveSurface
            && clickSinglePlacement(singleView, *hit, surface, clickPosition);
        check(clicked
                && level.things.empty()
                && level.curId == maximumId
                && assets::saveToSedBytes(level) == before
                && singleWindow.undoDepthForTest() == undoBefore
                && singleWindow.redoDepthForTest() == redoBefore
                && !singleView->toastTextForTest(
                    GlView::ToastGroup::Error).isEmpty(),
            "Thing single placement reports exhausted IDs before appending an owner or publishing history");

        level.curId = maximumId - 1;
        singleWindow.markCurrentDocumentAsSavedForTest();
        const std::vector<std::byte> beforeFinalSingle = assets::saveToSedBytes(level);
        const std::size_t finalSingleUndoBefore = singleWindow.undoDepthForTest();
        const bool finalSingleClicked = hit && haveSurface
            && clickSinglePlacement(singleView, *hit, surface, clickPosition);
        const std::vector<std::byte> afterFinalSingle = assets::saveToSedBytes(level);
        check(finalSingleClicked
                && level.things.size() == 1
                && level.things[0].id == maximumId - 1
                && level.things[0].num == 0
                && level.curId == maximumId
                && singleWindow.documentStateForTest().selection.things
                    == std::unordered_set<std::int32_t>{0}
                && singleWindow.hasUnsavedAuthoredLevelChangesForTest()
                && singleWindow.undoDepthForTest() == finalSingleUndoBefore + 1
                && singleWindow.redoDepthForTest() == 0,
            "single Thing placement accepts the final available ID with its destination ordinal and selected owner");
        check(singleWindow.undoForTest()
                && assets::saveToSedBytes(level) == beforeFinalSingle
                && singleWindow.documentStateForTest().selection.things.empty()
                && !singleWindow.hasUnsavedAuthoredLevelChangesForTest()
                && singleWindow.redoForTest()
                && assets::saveToSedBytes(level) == afterFinalSingle
                && singleWindow.documentStateForTest().selection.things
                    == std::unordered_set<std::int32_t>{0},
            "final-ID single placement Undo and Redo restore exact bytes and selection");
    }

    MainWindow brushWindow{};
    brushWindow.setAttribute(Qt::WA_DontShowOnScreen);
    brushWindow.resize(800, 600);
    brushWindow.setDocumentStateForTest(makeState());
    brushWindow.activateThingEditModeForTest();
    brushWindow.setThingPlacementTemplateForTest("+placement_probe", 0.0);
    brushWindow.setThingPlacementActiveForTest(true);
    GlView* const brushView = brushWindow.glViewForTest();
    check(brushView != nullptr,
        "Thing placement ID exhaustion brush fixture owns a hidden viewport");
    if ( brushView )
    {
        brushView->resize(800, 600);
        brushView->setLevel(
            &brushWindow.mutableDocumentStateForTest().level,
            GlView::SetLevelMode::resetView);
        brushView->fitViewToLevel();
        brushView->setCameraProjectionType(
            app::ViewportCamera::ProjectionType::Orthographic);
        brushView->setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
        brushView->setCameraOrbitDistance(8.0f);
        const std::optional<QPointF> projected =
            brushView->projectWorldToViewport(QVector3D(0.5f, 0.5f, 0.5f));
        const std::optional<QPoint> placementPoint = projected
            ? std::optional<QPoint>{QPoint(
                  qRound(projected->x()), qRound(projected->y()))}
            : std::nullopt;
        check(placementPoint.has_value(),
            "Thing placement ID exhaustion hidden viewport projects a brush target without showing GL");
        if ( !placementPoint )
        {
            return;
        }

        Level& level = brushWindow.mutableDocumentStateForTest().level;
        level.curId = maximumId - 1;
        const std::vector<std::byte> before = assets::saveToSedBytes(level);
        const std::size_t undoBefore = brushWindow.undoDepthForTest();
        const std::size_t redoBefore = brushWindow.redoDepthForTest();
        brushWindow.configureThingPlacementBrushForTest(2, 0.0, 0.1);
        const bool rejectedWholeBrush =
            !brushWindow.stampThingPlacementBrushForTest(*placementPoint);
        const bool noEmptyStroke =
            !brushWindow.finalizeThingPlacementBrushForTest();
        check(rejectedWholeBrush
                && noEmptyStroke
                && level.curId == maximumId - 1
                && level.things.empty()
                && assets::saveToSedBytes(level) == before
                && brushWindow.undoDepthForTest() == undoBefore
                && brushWindow.redoDepthForTest() == redoBefore,
            "Thing brush rejects an oversized batch before its first Thing, ID consumption, or history entry");

        brushWindow.configureThingPlacementBrushForTest(1, 0.0, 0.1);
        const bool finalIdStamp =
            brushWindow.stampThingPlacementBrushForTest(*placementPoint);
        const std::vector<std::byte> afterFinalStamp =
            assets::saveToSedBytes(level);
        const std::size_t undoAfterFinalStamp =
            brushWindow.undoDepthForTest();
        const bool rejectedLaterStamp =
            !brushWindow.stampThingPlacementBrushForTest(*placementPoint);
        const bool finalized =
            brushWindow.finalizeThingPlacementBrushForTest();
        check(finalIdStamp
                && rejectedLaterStamp
                && finalized
                && level.things.size() == 1
                && level.things[0].id == maximumId - 1
                && level.things[0].num == 0
                && level.curId == maximumId
                && brushWindow.documentStateForTest().selection.things
                    == std::unordered_set<std::int32_t>{0}
                && assets::saveToSedBytes(level) == afterFinalStamp
                && brushWindow.undoDepthForTest() == undoAfterFinalStamp,
            "Thing brush keeps its final allocated ID and destination ordinal when a later same-stroke stamp is rejected");
        check(brushWindow.undoForTest()
                && assets::saveToSedBytes(level) == before
                && brushWindow.documentStateForTest().selection.things.empty()
                && brushWindow.redoForTest()
                && assets::saveToSedBytes(level) == afterFinalStamp
                && brushWindow.documentStateForTest().selection.things
                    == std::unordered_set<std::int32_t>{0},
            "Thing brush ID exhaustion preserves exact final-stamp bytes and selection across Undo and Redo");
    }

    const std::string cogScriptText = "symbols\nint value\nend\n";
    std::vector<std::byte> cogScriptBytes{};
    cogScriptBytes.reserve(cogScriptText.size());
    for ( const char ch : cogScriptText )
    {
        cogScriptBytes.push_back(
            static_cast<std::byte>(static_cast<unsigned char>(ch)));
    }
    const auto installCogScript = [&cogScriptBytes](MainWindow& window)
    {
        window.setPrimaryResourceOverrideForTest(
            "id_exhaustion.cog",
            [cogScriptBytes]()
            {
                return std::optional<std::vector<std::byte>>{ cogScriptBytes };
            });
    };

    const auto makeCogState = [&makeState](const std::size_t count,
                                           const std::int32_t curId)
    {
        editor::DocumentState state = makeState();
        for ( std::size_t i = 0; i < count; ++i )
        {
            Cog& cog = state.level.newCog();
            cog.name = "id_exhaustion.cog";
            state.selection.cogs.insert(cog.num);
        }
        state.level.curId = curId;
        return state;
    };

    MainWindow addCogRejectedWindow{};
    addCogRejectedWindow.setAttribute(Qt::WA_DontShowOnScreen);
    addCogRejectedWindow.setDocumentStateForTest(makeCogState(1, maximumId));
    installCogScript(addCogRejectedWindow);
    addCogRejectedWindow.markCurrentDocumentAsSavedForTest();
    const Level& rejectedCogLevel =
        addCogRejectedWindow.documentStateForTest().level;
    const std::vector<std::byte> beforeRejectedCogAdd =
        assets::saveToSedBytes(rejectedCogLevel);
    const std::unordered_set<std::int32_t> rejectedCogSelection =
        addCogRejectedWindow.documentStateForTest().selection.cogs;
    const std::size_t rejectedCogUndoBefore =
        addCogRejectedWindow.undoDepthForTest();
    const std::size_t rejectedCogRedoBefore =
        addCogRejectedWindow.redoDepthForTest();
    const auto rejectedCog =
        addCogRejectedWindow.createPlacedCogFromScriptForTest(
            "id_exhaustion.cog", true);
    GlView* const rejectedCogView = addCogRejectedWindow.glViewForTest();
    check(!rejectedCog.has_value()
            && rejectedCogLevel.cogs.size() == 1
            && rejectedCogLevel.curId == maximumId
            && assets::saveToSedBytes(rejectedCogLevel)
                == beforeRejectedCogAdd
            && addCogRejectedWindow.documentStateForTest().selection.cogs
                == rejectedCogSelection
            && addCogRejectedWindow.undoDepthForTest()
                == rejectedCogUndoBefore
            && addCogRejectedWindow.redoDepthForTest()
                == rejectedCogRedoBefore
            && !addCogRejectedWindow.hasUnsavedAuthoredLevelChangesForTest()
            && addCogRejectedWindow.statusBar()->currentMessage().contains(
                QStringLiteral("entity ID space exhausted"))
            && rejectedCogView
            && !rejectedCogView->toastTextForTest(
                    GlView::ToastGroup::Error).isEmpty(),
        "placed COG creation resolves a synthetic script then rejects an unavailable ID before changing owners selection dirty state or history");

    MainWindow addCogFinalIdWindow{};
    addCogFinalIdWindow.setAttribute(Qt::WA_DontShowOnScreen);
    addCogFinalIdWindow.setDocumentStateForTest(
        makeCogState(1, maximumId - 1));
    installCogScript(addCogFinalIdWindow);
    addCogFinalIdWindow.markCurrentDocumentAsSavedForTest();
    const std::vector<std::byte> beforeFinalCogAdd = assets::saveToSedBytes(
        addCogFinalIdWindow.documentStateForTest().level);
    const auto finalIdCog = addCogFinalIdWindow.createPlacedCogFromScriptForTest(
        "id_exhaustion.cog", true);
    const std::vector<std::byte> afterFinalCogAdd = assets::saveToSedBytes(
        addCogFinalIdWindow.documentStateForTest().level);
    check(finalIdCog.has_value()
            && *finalIdCog == 1
            && addCogFinalIdWindow.documentStateForTest().level.cogs.size() == 2
            && addCogFinalIdWindow.documentStateForTest().level.cogs[1].id
                == maximumId - 1
            && addCogFinalIdWindow.documentStateForTest().level.curId
                == maximumId
            && addCogFinalIdWindow.documentStateForTest().selection.cogs
                == std::unordered_set<std::int32_t>{1}
            && addCogFinalIdWindow.hasUnsavedAuthoredLevelChangesForTest()
            && addCogFinalIdWindow.undoDepthForTest() == 1
            && addCogFinalIdWindow.redoDepthForTest() == 0,
        "placed COG creation accepts the final allocatable ID and publishes one dirty undoable command");
    check(addCogFinalIdWindow.undoForTest()
            && assets::saveToSedBytes(
                   addCogFinalIdWindow.documentStateForTest().level)
                == beforeFinalCogAdd
            && addCogFinalIdWindow.documentStateForTest().selection.cogs
                == std::unordered_set<std::int32_t>{0}
            && !addCogFinalIdWindow.hasUnsavedAuthoredLevelChangesForTest()
            && addCogFinalIdWindow.redoForTest()
            && assets::saveToSedBytes(
                   addCogFinalIdWindow.documentStateForTest().level)
                == afterFinalCogAdd
            && addCogFinalIdWindow.documentStateForTest().selection.cogs
                == std::unordered_set<std::int32_t>{1},
        "placed COG final-ID Undo and Redo restore the exact owner payload and selection");

    MainWindow duplicateCogWindow{};
    duplicateCogWindow.setAttribute(Qt::WA_DontShowOnScreen);
    duplicateCogWindow.setDocumentStateForTest(
        makeCogState(2, maximumId - 1));
    duplicateCogWindow.markCurrentDocumentAsSavedForTest();
    duplicateCogWindow.updateEditActionsForTest();
    QAction* const duplicateCogAction =
        duplicateCogWindow.findChild<QAction*>(QStringLiteral("edit.duplicate_cog"));
    GlView* const duplicateCogView = duplicateCogWindow.glViewForTest();
    const std::vector<std::byte> beforeRejectedCogDuplicate =
        assets::saveToSedBytes(duplicateCogWindow.documentStateForTest().level);
    const std::unordered_set<std::int32_t> duplicateCogSourceSelection =
        duplicateCogWindow.documentStateForTest().selection.cogs;
    const std::size_t duplicateCogUndoBefore =
        duplicateCogWindow.undoDepthForTest();
    const std::size_t duplicateCogRedoBefore =
        duplicateCogWindow.redoDepthForTest();
    if ( duplicateCogAction )
    {
        duplicateCogAction->trigger();
    }
    check(duplicateCogAction
            && duplicateCogView
            && duplicateCogWindow.documentStateForTest().level.cogs.size() == 2
            && duplicateCogWindow.documentStateForTest().level.curId
                == maximumId - 1
            && assets::saveToSedBytes(
                   duplicateCogWindow.documentStateForTest().level)
                == beforeRejectedCogDuplicate
            && duplicateCogWindow.documentStateForTest().selection.cogs
                == duplicateCogSourceSelection
            && duplicateCogWindow.undoDepthForTest() == duplicateCogUndoBefore
            && duplicateCogWindow.redoDepthForTest() == duplicateCogRedoBefore
            && !duplicateCogWindow.hasUnsavedAuthoredLevelChangesForTest()
            && duplicateCogWindow.statusBar()->currentMessage().contains(
                QStringLiteral("entity ID space exhausted"))
            && !duplicateCogView->toastTextForTest(
                    GlView::ToastGroup::Error).isEmpty(),
        "two-source COG duplication rejects at one-ID-short capacity before changing selection owners dirty state or history");

    duplicateCogWindow.mutableDocumentStateForTest().level.curId = maximumId - 2;
    duplicateCogWindow.markCurrentDocumentAsSavedForTest();
    duplicateCogWindow.updateEditActionsForTest();
    const std::vector<std::byte> beforeExactCogDuplicate =
        assets::saveToSedBytes(duplicateCogWindow.documentStateForTest().level);
    if ( duplicateCogAction )
    {
        duplicateCogAction->trigger();
    }
    const std::vector<std::byte> afterExactCogDuplicate =
        assets::saveToSedBytes(duplicateCogWindow.documentStateForTest().level);
    check(duplicateCogWindow.documentStateForTest().level.cogs.size() == 4
            && duplicateCogWindow.documentStateForTest().level.curId
                == maximumId
            && duplicateCogWindow.documentStateForTest().selection.cogs
                == std::unordered_set<std::int32_t>{2, 3}
            && duplicateCogWindow.hasUnsavedAuthoredLevelChangesForTest()
            && duplicateCogWindow.undoDepthForTest()
                == duplicateCogUndoBefore + 1
            && duplicateCogWindow.redoDepthForTest() == 0,
        "two-source COG duplication accepts exact two-ID capacity and consumes the final two IDs in one dirty command");
    check(duplicateCogWindow.undoForTest()
            && assets::saveToSedBytes(
                   duplicateCogWindow.documentStateForTest().level)
                == beforeExactCogDuplicate
            && duplicateCogWindow.documentStateForTest().selection.cogs
                == duplicateCogSourceSelection
            && !duplicateCogWindow.hasUnsavedAuthoredLevelChangesForTest()
            && duplicateCogWindow.redoForTest()
            && assets::saveToSedBytes(
                   duplicateCogWindow.documentStateForTest().level)
                == afterExactCogDuplicate
            && duplicateCogWindow.documentStateForTest().selection.cogs
                == std::unordered_set<std::int32_t>{2, 3},
        "exact-capacity COG duplication Undo and Redo restore both source selection and duplicate owners");

    MainWindow bakedLightWindow{};
    bakedLightWindow.setAttribute(Qt::WA_DontShowOnScreen);
    bakedLightWindow.resize(800, 600);
    editor::DocumentState bakedLightState = makeState();
    bakedLightState.level.curId = maximumId;
    bakedLightWindow.setDocumentStateForTest(std::move(bakedLightState));
    bakedLightWindow.markCurrentDocumentAsSavedForTest();
    bakedLightWindow.activateBakedLightEditModeForTest();
    QAction* const addLightAction =
        bakedLightWindow.findChild<QAction*>(QStringLiteral("edit.add_light"));
    GlView* const lightView = bakedLightWindow.glViewForTest();
    if ( addLightAction )
    {
        addLightAction->trigger();
    }
    const std::vector<std::byte> beforeRejectedLight = assets::saveToSedBytes(
        bakedLightWindow.documentStateForTest().level);
    const editor::Selection lightSelectionBefore =
        bakedLightWindow.documentStateForTest().selection;
    const std::size_t lightUndoBefore = bakedLightWindow.undoDepthForTest();
    const std::size_t lightRedoBefore = bakedLightWindow.redoDepthForTest();
    const Qt::KeyboardModifiers noModifiers = Qt::NoModifier;
    const bool rejectedLightClick = lightView && QMetaObject::invokeMethod(
        lightView,
        "surfaceClicked",
        Qt::DirectConnection,
        Q_ARG(QVector3D, QVector3D(0.5f, 0.5f, 0.5f)),
        Q_ARG(std::int32_t, 0),
        Q_ARG(std::int32_t, 0),
        Q_ARG(QPoint, QPoint(10, 10)),
        Q_ARG(Qt::KeyboardModifiers, noModifiers));
    check(addLightAction
            && rejectedLightClick
            && bakedLightWindow.documentStateForTest().level.lights.empty()
            && bakedLightWindow.documentStateForTest().level.curId == maximumId
            && assets::saveToSedBytes(
                   bakedLightWindow.documentStateForTest().level)
                == beforeRejectedLight
            && bakedLightWindow.documentStateForTest().selection.empty()
            && lightSelectionBefore.empty()
            && bakedLightWindow.undoDepthForTest() == lightUndoBefore
            && bakedLightWindow.redoDepthForTest() == lightRedoBefore
            && !bakedLightWindow.hasUnsavedAuthoredLevelChangesForTest()
            && bakedLightWindow.statusBar()->currentMessage().contains(
                QStringLiteral("entity ID space exhausted"))
            && lightView
            && !lightView->toastTextForTest(
                    GlView::ToastGroup::Error).isEmpty(),
        "baked-light Add rejects INT_MAX through the hidden viewport signal before changing owner selection dirty state or history");

    bakedLightWindow.mutableDocumentStateForTest().level.curId = maximumId - 1;
    bakedLightWindow.markCurrentDocumentAsSavedForTest();
    const std::vector<std::byte> beforeFinalLight = assets::saveToSedBytes(
        bakedLightWindow.documentStateForTest().level);
    const bool finalLightClick = lightView && QMetaObject::invokeMethod(
        lightView,
        "surfaceClicked",
        Qt::DirectConnection,
        Q_ARG(QVector3D, QVector3D(0.5f, 0.5f, 0.5f)),
        Q_ARG(std::int32_t, 0),
        Q_ARG(std::int32_t, 0),
        Q_ARG(QPoint, QPoint(10, 10)),
        Q_ARG(Qt::KeyboardModifiers, noModifiers));
    const std::vector<std::byte> afterFinalLight = assets::saveToSedBytes(
        bakedLightWindow.documentStateForTest().level);
    check(finalLightClick
            && bakedLightWindow.documentStateForTest().level.lights.size() == 1
            && bakedLightWindow.documentStateForTest().level.lights[0].id
                == maximumId - 1
            && bakedLightWindow.documentStateForTest().level.curId
                == maximumId
            && bakedLightWindow.documentStateForTest().selection.lights
                == std::unordered_set<std::int32_t>{0}
            && bakedLightWindow.hasUnsavedAuthoredLevelChangesForTest()
            && bakedLightWindow.undoDepthForTest() == lightUndoBefore + 1
            && bakedLightWindow.redoDepthForTest() == 0,
        "baked-light Add accepts the final allocatable ID and publishes one dirty undoable owner");
    check(bakedLightWindow.undoForTest()
            && assets::saveToSedBytes(
                   bakedLightWindow.documentStateForTest().level)
                == beforeFinalLight
            && bakedLightWindow.documentStateForTest().selection.empty()
            && !bakedLightWindow.hasUnsavedAuthoredLevelChangesForTest()
            && bakedLightWindow.redoForTest()
            && assets::saveToSedBytes(
                   bakedLightWindow.documentStateForTest().level)
                == afterFinalLight
            && bakedLightWindow.documentStateForTest().selection.lights
                == std::unordered_set<std::int32_t>{0},
        "baked-light final-ID Undo and Redo restore the exact owner and selection");
}

} // namespace sed::app_tests
