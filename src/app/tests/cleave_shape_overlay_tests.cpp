#include "test_common.hpp"

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "ui/viewport/cleave_shape_popover.hpp"
#include "ui/viewport/cleave_shape_utils.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/components/viewport_icons.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace sed::app_tests
{

void runCleaveShapeOverlayTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(QSize(1280, 800));
    window.activateSectorEditModeForTest();

    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.layers.emplace_back("Layer0");
    Sector& sector = level::levelCreateCube(
        state.level,
        0.0,
        0.0,
        0.0,
        TVector::up,
        TVector::right);
    sector.layer = 0;
    state.selection.sectors.insert(0);
    window.setDocumentStateForTest(std::move(state));
    window.activateSectorEditModeForTest();
    window.setLayerVisualStateForTest({true}, {false});

    GlView* const view = window.glViewForTest();
    QAction* const cleaveAction = window.findChild<QAction*>(
        QStringLiteral("viewport.sector.cleave"));
    QToolButton* const multiCut = window.findChild<QToolButton*>(
        QStringLiteral("cleaveMultiCutButton"));
    check(view && cleaveAction,
        "shape overlay fixture creates a Sector Cleave command context");
    if ( !view || !cleaveAction )
    {
        return;
    }

    view->setLevel(
        &window.documentStateForTest().level,
        GlView::SetLevelMode::resetView);
    view->setSelectionState(
        0, -1, -1, -1, window.documentStateForTest().selection);
    window.show();
    window.activateWindow();
    QApplication::processEvents();
    window.updateEditActionsForTest();
    check(cleaveAction->isEnabled(),
        "shape overlay fixture enables Sector Cleave from the selected Sector");
    if ( !cleaveAction->isEnabled() )
    {
        return;
    }

    if ( multiCut )
    {
        multiCut->setChecked(true);
    }
    cleaveAction->trigger();
    QApplication::processEvents();

    QWidget* const overlay = window.findChild<QWidget*>(
        QStringLiteral("cleaveOptionsOverlay"));
    QToolButton* const addShape = window.findChild<QToolButton*>(
        QStringLiteral("cleaveAddShapeButton"));
    QToolButton* const move = window.findChild<QToolButton*>(
        QStringLiteral("cleaveMoveButton"));
    QToolButton* const rotate = window.findChild<QToolButton*>(
        QStringLiteral("cleaveRotateButton"));
    QToolButton* const scale = window.findChild<QToolButton*>(
        QStringLiteral("cleaveScaleButton"));
    QToolButton* const gridSnap = window.findChild<QToolButton*>(
        QStringLiteral("cleaveGridSnapButton"));
    QToolButton* const bounded = window.findChild<QToolButton*>(
        QStringLiteral("cleaveBoundedButton"));
    QToolButton* const chain = window.findChild<QToolButton*>(
        QStringLiteral("cleaveChainButton"));
    QAction* const sharedGridSnap = window.findChild<QAction*>(
        QStringLiteral("viewport.transform.grid_snap"));
    QWidget* const shapePopover = window.findChild<QWidget*>(
        QStringLiteral("cleaveShapePopover"));
    QComboBox* const shapeKind = window.findChild<QComboBox*>(
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

    check(overlay && !overlay->isHidden() && addShape
            && addShape->toolButtonStyle() == Qt::ToolButtonIconOnly
            && !addShape->icon().isNull()
            && addShape->isCheckable()
            && addShape->accessibleName().contains(QStringLiteral("shape"),
                Qt::CaseInsensitive),
        "Cleave exposes a compact accessible Add Shape placement button");
    check(move && rotate && scale
            && move->isCheckable() && rotate->isCheckable() && scale->isCheckable()
            && move->isChecked() && !rotate->isChecked() && !scale->isChecked()
            && move->isEnabled() && !rotate->isEnabled() && !scale->isEnabled(),
        "Cleave-specific Move, Rotate, and Scale default to Move and gate unavailable transforms");
    check(move && rotate && scale
            && move->iconSize() == QSize(22, 22)
            && rotate->iconSize() == QSize(22, 22)
            && scale->iconSize() == QSize(22, 22)
            && move->size() == QSize(28, 28)
            && rotate->size() == QSize(28, 28)
            && scale->size() == QSize(28, 28),
        "Cleave transform glyphs match nearby option icons within their existing button footprints");
    check(gridSnap && sharedGridSnap && gridSnap->defaultAction() == sharedGridSnap,
        "Cleave Grid Snap reuses the shared viewport setting");
    check(gridSnap && gridSnap->iconSize() == QSize(22, 22),
        "Cleave Grid Snap draws a legible 22 px glyph within its compact button");
    check(bounded && multiCut && chain && bounded->height() == multiCut->height()
            && bounded->height() == chain->height(),
        "Bounded cut has the same button height as its adjacent text toggles");
    check(bounded && bounded->icon().pixmap(QSize(22, 22)).toImage()
            != ui::makeViewportIcon(ui::ViewportIcon::toolCleave).pixmap(QSize(22, 22)).toImage(),
        "Bounded cut uses its own reach-limiting glyph rather than the generic Cleave tool");
    QToolButton* const recalculate = window.findChild<QToolButton*>(
        QStringLiteral("lightBakeRecalculateButton"));
    check(recalculate && recalculate->iconSize() == QSize(22, 22)
            && !recalculate->icon().isNull()
            && recalculate->icon().pixmap(QSize(22, 22)).toImage()
                != window.style()->standardIcon(QStyle::SP_BrowserReload).pixmap(QSize(22, 22)).toImage()
            && recalculate->toolButtonStyle() == Qt::ToolButtonTextBesideIcon
            && recalculate->popupMode() == QToolButton::MenuButtonPopup
            && recalculate->menu(),
        "Baked Recalculate has a legible dedicated lighting glyph and preserves its scope dropdown");
    check(shapePopover && shapeKind && width && height && linked && units
            && segments && points
            && linked->toolButtonStyle() == Qt::ToolButtonIconOnly
            && !linked->icon().isNull(),
        "Add Shape popover exposes a shape, linked dimensions, segments, and points controls");
    if ( !addShape || !shapePopover || !shapeKind || !width || !height
        || !linked || !segments || !points || !units )
    {
        return;
    }

    addShape->click();
    QApplication::processEvents();
    check(addShape->isChecked() && shapePopover->isVisible()
            && view->cleaveShapeOptions().placementArmed,
        "Add Shape arms cursor placement and keeps its nonmodal settings popover open");
    // Exercise a normal tall, narrow viewport: the responsive options bar
    // must reserve all wrapped rows before anchoring the shape popover.
    const QSize originalWindowSize = window.size();
    const int originalMinimumViewportWidth = view->minimumWidth();
    const int originalMaximumViewportWidth = view->maximumWidth();
    const int unwrappedToolbarWidth = overlay->width();
    // Dock minima may keep the top-level window wider than its requested size.
    // Bound the actual viewport for this fixture, then restore its constraints.
    constexpr int narrowViewportWidth = 600;
    view->setFixedWidth(narrowViewportWidth);
    window.resize(900, 980);
    QApplication::processEvents();
    view->viewportStateChanged();
    QApplication::processEvents();
    bool containedControls = overlay != nullptr;
    int minimumRowY = std::numeric_limits<int>::max();
    int maximumRowY = std::numeric_limits<int>::min();
    if (overlay)
    {
        for (const QWidget* control : overlay->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
        {
            if (control->isVisible())
            {
                containedControls = containedControls && overlay->rect().contains(control->geometry());
                minimumRowY = std::min(minimumRowY, control->geometry().center().y());
                maximumRowY = std::max(maximumRowY, control->geometry().center().y());
            }
        }
    }
    const bool wrappedPopoverGeometry = view->width() == narrowViewportWidth
            && view->width() < unwrappedToolbarWidth
            && containedControls && maximumRowY - minimumRowY >= 20
            && shapePopover->geometry().top() > overlay->geometry().bottom()
            && !shapePopover->geometry().intersects(overlay->geometry())
            && view->rect().contains(shapePopover->geometry());
    if (!wrappedPopoverGeometry)
    {
        const auto reportRect = [](const char* name, const QRect& rect)
        {
            std::cerr << name << "=(" << rect.x() << ',' << rect.y() << ','
                << rect.width() << ',' << rect.height() << ") ";
        };
        std::cerr << "Cleave wrapped geometry: containedControls=" << containedControls
            << " rowCenters=" << minimumRowY << ',' << maximumRowY << ' ';
        reportRect("window", window.geometry());
        reportRect("view", view->rect());
        reportRect("overlay", overlay->geometry());
        reportRect("popover", shapePopover->geometry());
        std::cerr << '\n';
        for (const QWidget* control : overlay->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
        {
            if (control->isVisible())
            {
                reportRect(control->objectName().toStdString().c_str(), control->geometry());
                std::cerr << "hint=" << control->sizeHint().width() << ','
                    << control->sizeHint().height() << " minimum="
                    << control->minimumSize().width() << ',' << control->minimumSize().height()
                    << '\n';
            }
        }
    }
    check(wrappedPopoverGeometry,
        "a wrapped Cleave bar contains its second row and anchors Add Shape below the complete toolbar");
    view->setMinimumWidth(originalMinimumViewportWidth);
    view->setMaximumWidth(originalMaximumViewportWidth);
    window.resize(originalWindowSize);
    QApplication::processEvents();
    view->viewportStateChanged();
    QApplication::processEvents();
    check(shapeKind->count() == 4
            && shapeKind->findData(static_cast<int>(ui::viewport::CleaveShapeKind::Square)) == -1
            && shapeKind->findData(static_cast<int>(ui::viewport::CleaveShapeKind::Rectangle)) >= 0
            && shapeKind->findData(static_cast<int>(ui::viewport::CleaveShapeKind::RegularPolygon)) >= 0
            && shapeKind->findData(static_cast<int>(ui::viewport::CleaveShapeKind::Arch)) >= 0
            && shapeKind->findData(static_cast<int>(ui::viewport::CleaveShapeKind::Star)) >= 0,
        "shape selector offers rectangle, regular N-gon, arch, and star without redundant Square");
    const int polygonIndex = shapeKind->findData(
        static_cast<int>(ui::viewport::CleaveShapeKind::RegularPolygon));
    check(polygonIndex >= 0 && shapeKind->itemText(polygonIndex) == QStringLiteral("N-Gon"),
        "shape selector calls the regular polygon N-Gon");
    check(units->count() == 3
            && units->itemData(0).toInt() == static_cast<int>(GlView::CleaveMeasureUnit::Meters)
            && units->itemData(1).toInt() == static_cast<int>(GlView::CleaveMeasureUnit::IJU)
            && units->itemData(2).toInt() == static_cast<int>(GlView::CleaveMeasureUnit::Bare)
            && units->findData(static_cast<int>(GlView::CleaveMeasureUnit::JKU)) == -1,
        "IJIM offers exactly Meters, IJU, and Bare length units");

    QCheckBox* const cameraPlane = window.findChild<QCheckBox*>(
        QStringLiteral("cleaveShapeCameraPlaneCheck"));
    check(cameraPlane && cameraPlane->isVisible() && !cameraPlane->isChecked(),
        "Sector Add Shape exposes an unchecked Camera plane placement option");
    if ( cameraPlane )
    {
        cameraPlane->setChecked(true);
        QApplication::processEvents();
        check(view->cleaveShapeOptions().cameraPlane,
            "Camera plane checkbox publishes the Sector shape placement preference");
        cameraPlane->setChecked(false);
    }

    units->setCurrentIndex(units->findData(
        static_cast<int>(GlView::CleaveMeasureUnit::Meters)));
    QApplication::processEvents();
    const auto meterOptions = view->cleaveShapeOptions();
    check(width->value() == 1.0 && height->value() == 1.0
            && meterOptions.width == 0.1 && meterOptions.height == 0.1,
        "fresh shape defaults to one displayed meter in each dimension");
    units->setCurrentIndex(units->findData(
        static_cast<int>(GlView::CleaveMeasureUnit::Bare)));
    QApplication::processEvents();
    check(width->minimum() == 0.005 && width->maximum() == 10000.0
            && height->minimum() == 0.005 && height->maximum() == 10000.0
            && segments->minimum() == 3 && segments->maximum() == 64
            && points->minimum() == 3 && points->maximum() == 64,
        "shape dimensions and counts expose the validated geometry limits in Bare units");
    check(width->value() == 0.1 && height->value() == 0.1,
        "fresh shape defaults to point one in Bare units");

    const auto initialOptions = view->cleaveShapeOptions();
    shapeKind->setCurrentIndex(shapeKind->findData(
        static_cast<int>(ui::viewport::CleaveShapeKind::Rectangle)));
    height->setValue(2.75);
    QApplication::processEvents();
    check(linked->isChecked() && width->value() == height->value(),
        "linked rectangle dimensions stay equal when height is edited");
    linked->setChecked(false);
    width->setValue(2.5);
    height->setValue(3.5);
    shapeKind->setCurrentIndex(shapeKind->findData(
        static_cast<int>(ui::viewport::CleaveShapeKind::Star)));
    segments->setValue(12);
    points->setValue(7);
    QApplication::processEvents();
    const auto changedOptions = view->cleaveShapeOptions();
    check(initialOptions.kind == ui::viewport::CleaveShapeKind::Rectangle
            && changedOptions.kind == ui::viewport::CleaveShapeKind::Star
            && !changedOptions.dimensionsLinked
            && changedOptions.width > 0.0 && changedOptions.height > 0.0
            && changedOptions.points == 7
            && changedOptions.placementArmed,
        "shape controls update the persisted shape and keep placement armed while editing");
    check(view->stagedCleaveLineCount() == 0,
        "opening and editing the shape popover creates no draft cut until viewport placement");

    const auto wheel = [view](QWidget* target)
    {
        view->setFocus(Qt::OtherFocusReason);
        target->clearFocus();
        const QPointF local = target->rect().center();
        const QPointF global = target->mapToGlobal(local.toPoint());
        QWheelEvent event(local, global, {}, QPoint(0, 120),
            Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(target, &event);
        QApplication::processEvents();
    };
    shapeKind->setCurrentIndex(shapeKind->findData(
        static_cast<int>(ui::viewport::CleaveShapeKind::Rectangle)));
    linked->setChecked(false);
    width->setValue(2.5);
    height->setValue(3.5);
    wheel(width);
    check(width->value() == 2.6 && view->cleaveShapeOptions().width == 2.6,
        "unfocused shape width accepts one mouse-wheel step and publishes its value");
    wheel(height);
    check(height->value() == 3.6 && view->cleaveShapeOptions().height == 3.6,
        "unfocused shape height accepts one mouse-wheel step and publishes its value");
    shapeKind->setCurrentIndex(shapeKind->findData(
        static_cast<int>(ui::viewport::CleaveShapeKind::RegularPolygon)));
    segments->setValue(12);
    wheel(segments);
    check(segments->value() == 13 && view->cleaveShapeOptions().segments == 13,
        "unfocused shape segments accept one mouse-wheel step");
    shapeKind->setCurrentIndex(shapeKind->findData(
        static_cast<int>(ui::viewport::CleaveShapeKind::Star)));
    points->setValue(7);
    wheel(points);
    check(points->value() == 8 && view->cleaveShapeOptions().points == 8,
        "unfocused Star points accept one mouse-wheel step");

    QLineEdit* const widthText = width->findChild<QLineEdit*>();
    check(widthText != nullptr, "shape width exposes its native numeric editor");
    if ( widthText )
    {
        widthText->setFocus(Qt::OtherFocusReason);
        widthText->setText(QStringLiteral("4.125"));
        view->viewportStateChanged();
        check(widthText->text() == QStringLiteral("4.125"),
            "unrelated viewport state synchronization preserves pending shape numeric text");
        width->setValue(2.6);
    }

    auto authoredOptions = view->cleaveShapeOptions();
    authoredOptions.width = 0.325;
    authoredOptions.height = 0.425;
    authoredOptions.dimensionsLinked = false;
    view->setCleaveShapeOptions(authoredOptions);
    units->setCurrentIndex(units->findData(
        static_cast<int>(GlView::CleaveMeasureUnit::IJU)));
    check(width->value() == 3.25 && height->value() == 4.25,
        "switching authored shape dimensions to IJU converts display without resizing geometry");
    for ( const ProjectType project : {ProjectType::JKDF2, ProjectType::MOTS} )
    {
        window.mutableDocumentStateForTest().level.kind = project;
        view->setLevel(&window.documentStateForTest().level,
            GlView::SetLevelMode::keepView);
        view->setSelectionState(0, -1, -1, -1,
            window.documentStateForTest().selection);
        window.updateEditActionsForTest();
        view->viewportStateChanged();
        QApplication::processEvents();
        check(units->count() == 3
                && units->itemData(0).toInt() == static_cast<int>(GlView::CleaveMeasureUnit::Meters)
                && units->itemData(1).toInt() == static_cast<int>(GlView::CleaveMeasureUnit::JKU)
                && units->itemData(2).toInt() == static_cast<int>(GlView::CleaveMeasureUnit::Bare)
                && units->findData(static_cast<int>(GlView::CleaveMeasureUnit::IJU)) == -1,
            "classic projects offer exactly Meters, JKU, and Bare after a project switch");
        check(view->cleaveMeasureUnit() == GlView::CleaveMeasureUnit::Meters
                && units->currentData().toInt() == static_cast<int>(GlView::CleaveMeasureUnit::Meters)
                && width->value() == 3.25 && height->value() == 4.25
                && view->cleaveShapeOptions().width == authoredOptions.width
                && view->cleaveShapeOptions().height == authoredOptions.height,
            "unsupported project unit falls back to Meters while preserving authored dimensions and control agreement");
    }

    view->addStagedCleaveLineForTest({-0.5f, 0, 0}, {0.5f, 0, 0}, {0, 0, 1});
    view->viewportStateChanged();
    for ( const auto mode : {GlView::StagedCleaveTransformMode::Rotate,
        GlView::StagedCleaveTransformMode::Scale} )
    {
        view->selectStagedCleaveLineForTest(0);
        view->viewportStateChanged();
        view->setStagedCleaveTransformMode(mode);
        check(view->stagedCleaveTransformMode() == mode,
            "valid selected shape cut permits Cleave-local Rotate or Scale");
        view->selectStagedCleaveLineForTest(-1);
        view->viewportStateChanged();
        check(view->stagedCleaveTransformMode() == GlView::StagedCleaveTransformMode::Move
                && move->isChecked() && !rotate->isChecked() && !scale->isChecked()
                && !rotate->isEnabled() && !scale->isEnabled(),
            "losing the eligible line selection returns disabled Rotate and Scale to checked Move");
    }

    view->cancelStagedCleave();
    window.activateSurfaceEditModeForTest();
    window.activateSurfaceFaceSubModeForTest();
    window.mutableDocumentStateForTest().selection.clear();
    window.mutableDocumentStateForTest().selection.surfaces.insert({0, 0});
    view->setSelectionState(0, 0, -1, -1,
        window.documentStateForTest().selection);
    window.updateEditActionsForTest();
    if ( QAction* const surfaceCleave = window.findChild<QAction*>(
        QStringLiteral("viewport.surface.cleave")); surfaceCleave && surfaceCleave->isEnabled() )
    {
        if ( !surfaceCleave->isChecked() )
        {
            surfaceCleave->trigger();
        }
        multiCut->setChecked(true);
        if ( !addShape->isChecked() )
        {
            addShape->click();
        }
        QApplication::processEvents();
        check(cameraPlane && cameraPlane->isHidden(),
            "Surface Add Shape hides the Sector-only Camera plane control");
    }
    else
    {
        check(false, "shape overlay fixture enables Surface Cleave for camera-plane visibility");
    }

    ui::viewport::CleaveShapePopover standalone{};
    ui::viewport::CleaveShapeSpec legacySquare{};
    legacySquare.kind = ui::viewport::CleaveShapeKind::Square;
    legacySquare.width = 2.0;
    legacySquare.height = 3.0;
    legacySquare.dimensionsLinked = false;
    standalone.setShapeSpec(legacySquare, 1.0, {});
    check(standalone.shapeSpec().kind == ui::viewport::CleaveShapeKind::Rectangle
            && standalone.shapeSpec().width == 2.0
            && standalone.shapeSpec().height == 2.0
            && standalone.shapeSpec().dimensionsLinked,
        "legacy Square options display as an equal-dimension Rectangle");
    auto invalidSpec = legacySquare;
    invalidSpec.width = std::numeric_limits<double>::quiet_NaN();
    invalidSpec.height = std::numeric_limits<double>::infinity();
    standalone.setShapeSpec(invalidSpec, std::numeric_limits<double>::max(), {});
    QDoubleSpinBox* const finiteWidth = standalone.findChild<QDoubleSpinBox*>(
        QStringLiteral("cleaveShapeWidthSpin"));
    check(finiteWidth && standalone.shapeSpec().width == 0.1
            && standalone.shapeSpec().height == 0.1
            && finiteWidth->minimum() == 0.005 && finiteWidth->maximum() == 10000.0,
        "malformed dimensions and an overflowing display scale normalize to finite shape defaults and bounds");
}

void runCleaveFamilyControlsTests()
{
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(1280, 800);
    editor::DocumentState document{};
    document.level.kind = ProjectType::IJIM;
    document.level.layers.emplace_back("Layer0");
    level::levelCreateCube(document.level, 0, 0, 0, TVector::up, TVector::right).layer = 0;
    document.selection.sectors.insert(0);
    window.activateSectorEditModeForTest();
    window.setDocumentStateForTest(std::move(document));
    window.activateSectorEditModeForTest();
    window.setLayerVisualStateForTest({true}, {false});
    window.show();
    QApplication::processEvents();
    GlView* const view = window.glViewForTest();
    auto* sectorCleave = window.findChild<QAction*>("viewport.sector.cleave");
    auto* surfaceCleave = window.findChild<QAction*>("viewport.surface.cleave");
    auto* staged = window.findChild<QToolButton*>("cleaveMultiCutButton");
    auto* chain = window.findChild<QToolButton*>("cleaveChainButton");
    auto* bounded = window.findChild<QToolButton*>("cleaveBoundedButton");
    auto* angles = window.findChild<QToolButton*>("cleaveShowAnglesButton");
    auto* lengths = window.findChild<QToolButton*>("cleaveShowLengthsButton");
    auto* numbers = window.findChild<QToolButton*>("cleaveShowNumbersButton");
    auto* step = window.findChild<ui::ScrubFloatEdit*>("cleaveAngleStepSpin");
    auto* units = window.findChild<QComboBox*>("cleaveUnitsCombo");
    check(view && sectorCleave && surfaceCleave && staged && chain && bounded
            && angles && lengths && numbers && step && units,
        "independent Cleave controls fixture has both tool owners and all option controls");
    if (!view || !sectorCleave || !surfaceCleave || !staged || !chain || !bounded
        || !angles || !lengths || !numbers || !step || !units) return;
    const auto activateSector = [&]()
    {
        window.activateSectorEditModeForTest();
        window.mutableDocumentStateForTest().selection.clear();
        window.mutableDocumentStateForTest().selection.sectors.insert(0);
        view->setSelectionState(0, -1, -1, -1, window.documentStateForTest().selection);
        window.updateEditActionsForTest();
        if (sectorCleave->isEnabled() && !sectorCleave->isChecked()) sectorCleave->trigger();
        QApplication::processEvents();
    };
    const auto activateSurface = [&]()
    {
        window.activateSurfaceEditModeForTest();
        window.activateSurfaceFaceSubModeForTest();
        window.mutableDocumentStateForTest().selection.clear();
        window.mutableDocumentStateForTest().selection.surfaces.insert({0, 0});
        view->setSelectionState(0, 0, -1, -1, window.documentStateForTest().selection);
        window.updateEditActionsForTest();
        if (surfaceCleave->isEnabled() && !surfaceCleave->isChecked()) surfaceCleave->trigger();
        QApplication::processEvents();
    };
    activateSector();
    check(sectorCleave->isEnabled() && sectorCleave->isChecked(),
        "independent controls fixture activates Sector Cleave");
    view->setCleaveStagedMode(true);
    view->setCleaveChainEnabled(true);
    view->setCleaveBounded(false);
    view->setCleaveAnglesVisible(false);
    view->setCleaveLengthsVisible(true);
    view->setCleaveLineNumbersVisible(true);
    view->setCleaveAngleStepDegrees(15);
    view->setCleaveMeasureUnit(GlView::CleaveMeasureUnit::IJU);
    activateSurface();
    check(surfaceCleave->isEnabled() && surfaceCleave->isChecked(),
        "independent controls fixture activates Surface Face Cleave");
    view->setCleaveStagedMode(false);
    view->setCleaveChainEnabled(false);
    view->setCleaveBounded(true);
    view->setCleaveAnglesVisible(true);
    view->setCleaveLengthsVisible(false);
    view->setCleaveLineNumbersVisible(false);
    view->setCleaveAngleStepDegrees(45);
    view->setCleaveMeasureUnit(GlView::CleaveMeasureUnit::Bare);
    QApplication::processEvents();
    check(!view->cleaveStagedMode() && !staged->isChecked()
            && !chain->isChecked() && bounded->isChecked()
            && angles->isChecked() && !lengths->isChecked() && !numbers->isChecked()
            && step->value() == 45
            && units->currentData().toInt() == static_cast<int>(GlView::CleaveMeasureUnit::Bare),
        "Surface controls read its options from core without reapplying the prior Sector checkbox");
    activateSector();
    check(view->cleaveStagedMode() && staged->isChecked()
            && view->cleaveChainEnabled() && chain->isChecked()
            && !view->cleaveBounded() && !bounded->isChecked()
            && !angles->isChecked() && lengths->isChecked() && numbers->isChecked()
            && step->value() == 15
            && units->currentData().toInt() == static_cast<int>(GlView::CleaveMeasureUnit::IJU),
        "returning to Sector publishes its independent checked and numeric controls");
    activateSurface();
    check(!view->cleaveStagedMode() && !staged->isChecked()
            && !view->cleaveChainEnabled() && !chain->isChecked()
            && view->cleaveBounded() && bounded->isChecked()
            && angles->isChecked() && !lengths->isChecked() && !numbers->isChecked()
            && step->value() == 45
            && units->currentData().toInt() == static_cast<int>(GlView::CleaveMeasureUnit::Bare),
        "returning to Surface retains its separate immediate-mode and guide options");
}

} // namespace sed::app_tests
