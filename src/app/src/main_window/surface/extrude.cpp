#include "main_window/common.hpp"
#include "ui/components/scrub_float_edit.hpp"

#include "ui/surface/surface_extrude_preview_utils.hpp"

#include <QFontMetrics>
#include <QLineEdit>
#include <QMargins>
#include <QStyle>
#include <QToolButton>

#include <algorithm>

namespace sed
{
namespace
{
constexpr double kSurfaceExtrudeValueLimit = 1'000'000.0;
constexpr double kSurfaceExtrudeSingleStep = 0.125;
constexpr int kSurfaceExtrudeOverlayMargin = 10;

QToolButton* makeSurfaceExtrudeButton(
    QWidget* parent,
    const QString& text,
    const QString& objectName,
    const QIcon& icon = QIcon{})
{
    auto* button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setText(text);
    button->setToolButtonStyle(icon.isNull() ? Qt::ToolButtonTextOnly : Qt::ToolButtonTextBesideIcon);
    button->setIcon(icon);
    button->setIconSize(QSize(16, 16));
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoRaise(true);
    return button;
}

ui::ScrubFloatEdit* makeSurfaceExtrudeSpin(
    QWidget* parent,
    const QString& objectName,
    const QString& tooltip,
    const double value)
{
    auto* spin = new ui::ScrubFloatEdit(parent);
    spin->setObjectName(objectName);
    spin->setRange(-kSurfaceExtrudeValueLimit, kSurfaceExtrudeValueLimit);
    // Display precision never rounds the authored double or the grid step.
    spin->setDecimals(6);
    spin->setSingleStep(kSurfaceExtrudeSingleStep);
    spin->setToolTip(tooltip);
    spin->setValue(value);
    return spin;
}

void sizeSurfaceExtrudeSpin(ui::ScrubFloatEdit* const spin)
{
    if ( !spin || !spin->lineEdit() )
    {
        return;
    }

    QLineEdit* const lineEdit = spin->lineEdit();
    const QFontMetrics metrics(lineEdit->font());
    const int valueWidth = metrics.horizontalAdvance(
        QStringLiteral("-1000000.000000"));
    const QMargins margins = lineEdit->textMargins();
    const int frameWidth = lineEdit->style()->pixelMetric(
        QStyle::PM_DefaultFrameWidth, nullptr, lineEdit);

    int stepperWidth = 0;
    const QList<QToolButton*> steppers = spin->findChildren<QToolButton*>();
    for ( QToolButton* const stepper : steppers )
    {
        if ( stepper )
        {
            stepperWidth = std::max({
                stepperWidth,
                stepper->minimumWidth(),
                stepper->sizeHint().width()});
        }
    }

    // Reserve a complete signed, six-decimal endpoint plus one font-sized
    // cursor inset. The two stepper widths are part of the ScrubFloatEdit
    // layout, so showing them cannot steal digits from the value.
    const int textWidth = valueWidth + metrics.averageCharWidth();
    const int lineEditWidth = textWidth
        + margins.left() + margins.right() + 2 * frameWidth + 2;
    spin->setFixedWidth(lineEditWidth + 2 * stepperWidth);
}

void resetSurfaceExtrudeExpand(ui::ScrubFloatEdit* spin, double& expand)
{
    expand = 0.0;
    if (!spin)
    {
        return;
    }

    const QSignalBlocker blocker(spin);
    spin->setValue(expand);
}

void clearSurfaceExtrudeFailure(QToolButton* applyButton, QStatusBar* status)
{
    if ( !applyButton )
    {
        return;
    }
    const QString previousFailure = applyButton->property("extrudeFailureMessage").toString();
    if ( status && !previousFailure.isEmpty() && status->currentMessage() == previousFailure )
    {
        status->clearMessage();
    }
    applyButton->setProperty("extrudeFailureMessage", QVariant{});
    applyButton->setToolTip(QString{});
}

bool sameSurfaceRef(const SurfaceRef lhs, const SurfaceRef rhs) noexcept
{
    return lhs.sector == rhs.sector && lhs.surface == rhs.surface;
}
} // namespace

void MainWindow::setupSurfaceExtrudeOptionsOverlay()
{
    if (!glView_)
    {
        return;
    }

    surfaceExtrudeOptionsOverlay_ = new QWidget(glView_);
    surfaceExtrudeOptionsOverlay_->setAttribute(Qt::WA_StyledBackground, true);
    surfaceExtrudeOptionsOverlay_->setObjectName("surfaceExtrudeOptionsOverlay");

    auto* layout = new QHBoxLayout(surfaceExtrudeOptionsOverlay_);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    auto* title = new QLabel("Surface Extrude", surfaceExtrudeOptionsOverlay_);
    title->setObjectName("surfaceExtrudeOptionsLabel");
    layout->addWidget(title);

    auto* separatorA = new QFrame(surfaceExtrudeOptionsOverlay_);
    separatorA->setFrameShape(QFrame::VLine);
    separatorA->setObjectName("surfaceExtrudeOptionsSeparator");
    layout->addWidget(separatorA);

    auto* lengthLabel = new QLabel("Length", surfaceExtrudeOptionsOverlay_);
    lengthLabel->setObjectName("surfaceExtrudeOptionsLabel");
    layout->addWidget(lengthLabel);

    surfaceExtrudeLengthSpin_ = makeSurfaceExtrudeSpin(
        surfaceExtrudeOptionsOverlay_,
        "surfaceExtrudeLengthSpin",
        "Positive extrusion distance outward from the surface",
        surfaceExtrudeLength_);
    layout->addWidget(surfaceExtrudeLengthSpin_);

    auto* expandLabel = new QLabel("Expand", surfaceExtrudeOptionsOverlay_);
    expandLabel->setObjectName("surfaceExtrudeOptionsLabel");
    layout->addWidget(expandLabel);

    surfaceExtrudeExpandSpin_ = makeSurfaceExtrudeSpin(
        surfaceExtrudeOptionsOverlay_,
        "surfaceExtrudeExpandSpin",
        "Outward in-plane expansion at the new outer face",
        surfaceExtrudeExpand_);
    layout->addWidget(surfaceExtrudeExpandSpin_);

    surfaceExtrudeGridSnapButton_ = makeSurfaceExtrudeButton(
        surfaceExtrudeOptionsOverlay_,
        QString{},
        "surfaceExtrudeGridSnapButton",
        ui::makeViewportIcon(ui::ViewportIcon::gridSnap));
    surfaceExtrudeGridSnapButton_->setCheckable(true);
    surfaceExtrudeGridSnapButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    surfaceExtrudeGridSnapButton_->setToolTip(
        "Quantize Length and Expand to the effective minor spacing of the active viewport grid.");
    surfaceExtrudeGridSnapButton_->setAccessibleName("Snap Extrude values to grid");
    surfaceExtrudeGridSnapButton_->setChecked(
        viewportTransformGridSnapAction_
        && viewportTransformGridSnapAction_->isChecked());
    layout->addWidget(surfaceExtrudeGridSnapButton_);

    auto* separatorB = new QFrame(surfaceExtrudeOptionsOverlay_);
    separatorB->setFrameShape(QFrame::VLine);
    separatorB->setObjectName("surfaceExtrudeOptionsSeparator");
    layout->addWidget(separatorB);

    surfaceExtrudeApplyButton_ = makeSurfaceExtrudeButton(
        surfaceExtrudeOptionsOverlay_,
        "Apply",
        "surfaceExtrudeApplyButton",
        ui::makeViewportIcon(ui::ViewportIcon::actionExtrude));
    layout->addWidget(surfaceExtrudeApplyButton_);

    surfaceExtrudeCancelButton_ = makeSurfaceExtrudeButton(
        surfaceExtrudeOptionsOverlay_,
        "Cancel",
        "surfaceExtrudeCancelButton");
    layout->addWidget(surfaceExtrudeCancelButton_);

    const auto changeLength = [this](const double value)
    {
        surfaceExtrudeLength_ = value;
        refreshSurfaceExtrudeGridSnapState(true);
    };
    const auto changeExpand = [this](const double value)
    {
        surfaceExtrudeExpand_ = value;
        refreshSurfaceExtrudeGridSnapState(true);
    };
    connect(surfaceExtrudeLengthSpin_, &ui::ScrubFloatEdit::valueCommitted, this, changeLength);
    connect(surfaceExtrudeLengthSpin_, &ui::ScrubFloatEdit::scrubValueChanged, this, changeLength);
    connect(surfaceExtrudeExpandSpin_, &ui::ScrubFloatEdit::valueCommitted, this, changeExpand);
    connect(surfaceExtrudeExpandSpin_, &ui::ScrubFloatEdit::scrubValueChanged, this, changeExpand);
    connect(surfaceExtrudeGridSnapButton_, &QToolButton::toggled, this, [this](const bool checked)
        {
            if (viewportTransformGridSnapAction_
                && viewportTransformGridSnapAction_->isChecked() != checked)
            {
                viewportTransformGridSnapAction_->setChecked(checked);
                return;
            }
            if (surfaceExtrudePreviewActive_)
            {
                refreshSurfaceExtrudeGridSnapState(true);
            }
        });
    if (viewportTransformGridSnapAction_)
    {
        connect(viewportTransformGridSnapAction_, &QAction::toggled, this, [this](const bool checked)
            {
                if (surfaceExtrudeGridSnapButton_)
                {
                    const QSignalBlocker blocker(surfaceExtrudeGridSnapButton_);
                    surfaceExtrudeGridSnapButton_->setChecked(checked);
                }
                if (surfaceExtrudePreviewActive_
                    && !viewportTransformGridSnapAction_->property(
                        "sed.heldModifierPreview").toBool())
                {
                    refreshSurfaceExtrudeGridSnapState(true);
                }
            });
    }
    connect(surfaceExtrudeApplyButton_, &QToolButton::clicked, this, [this]()
        {
            applySurfaceExtrudePreview();
        });
    connect(surfaceExtrudeCancelButton_, &QToolButton::clicked, this, [this]()
        {
            cancelSurfaceExtrudePreview();
        });

    surfaceExtrudeOptionsOverlay_->setStyleSheet(
        "QWidget#surfaceExtrudeOptionsOverlay {"
        "  background: rgba(18, 18, 22, 235);"
        "  border: 1px solid rgba(255, 255, 255, 40);"
        "  border-radius: 8px;"
        "}"
        "QLabel#surfaceExtrudeOptionsLabel {"
        "  color: rgba(230, 232, 238, 220);"
        "  padding-left: 2px;"
        "}"
        "QFrame#surfaceExtrudeOptionsSeparator {"
        "  color: rgba(255, 255, 255, 32);"
        "}"
        "QLineEdit {"
        "  background: rgba(255, 255, 255, 14);"
        "  border: 1px solid rgba(255, 255, 255, 32);"
        "  border-radius: 5px;"
        "  padding: 2px 5px;"
        "  color: rgba(236, 238, 242, 245);"
        "}"
        "QToolButton {"
        "  color: rgba(236, 238, 242, 245);"
        "  background: rgba(255, 255, 255, 12);"
        "  border: 1px solid rgba(255, 255, 255, 28);"
        "  border-radius: 5px;"
        "}"
        "QToolButton#surfaceExtrudeGridSnapButton,"
        "QToolButton#surfaceExtrudeApplyButton,"
        "QToolButton#surfaceExtrudeCancelButton {"
        "  padding: 3px 8px;"
        "}"
        "QToolButton[sedButtonRole=\"stepper\"] {"
        "  min-width: 0px;"
        "  padding: 0px 2px;"
        "}"
        "QToolButton:hover {"
        "  background: rgba(255, 255, 255, 24);"
        "}"
        "QToolButton#surfaceExtrudeGridSnapButton:checked {"
        "  background: rgba(75, 145, 225, 145);"
        "  border-color: rgba(150, 200, 255, 125);"
        "}"
        "QToolButton#surfaceExtrudeApplyButton {"
        "  background: rgba(90, 145, 220, 130);"
        "  border-color: rgba(150, 195, 255, 100);"
        "}");

    sizeSurfaceExtrudeSpin(surfaceExtrudeLengthSpin_);
    sizeSurfaceExtrudeSpin(surfaceExtrudeExpandSpin_);

    surfaceExtrudeOptionsOverlay_->hide();
    syncSurfaceExtrudeOptionsOverlay();
}

void MainWindow::positionSurfaceExtrudeOptionsOverlay()
{
    if (!glView_ || !surfaceExtrudeOptionsOverlay_)
    {
        return;
    }

    surfaceExtrudeOptionsOverlay_->adjustSize();

    int x = std::max(
        kSurfaceExtrudeOverlayMargin,
        (glView_->width() - surfaceExtrudeOptionsOverlay_->width()) / 2);
    const int y = kSurfaceExtrudeOverlayMargin;

    if (viewportOverlay_ && viewportOverlay_->isVisible())
    {
        const QRect optionsRect(
            x,
            y,
            surfaceExtrudeOptionsOverlay_->width(),
            surfaceExtrudeOptionsOverlay_->height());
        const QRect avoidRect = viewportOverlay_->geometry().adjusted(
            -kSurfaceExtrudeOverlayMargin,
            -kSurfaceExtrudeOverlayMargin,
            kSurfaceExtrudeOverlayMargin,
            kSurfaceExtrudeOverlayMargin);
        if (optionsRect.intersects(avoidRect))
        {
            x = std::max(
                kSurfaceExtrudeOverlayMargin,
                avoidRect.left() - surfaceExtrudeOptionsOverlay_->width() - kSurfaceExtrudeOverlayMargin);
        }
    }

    surfaceExtrudeOptionsOverlay_->move(x, y);
    reflowTopCenterOverlays();
}

void MainWindow::syncSurfaceExtrudeOptionsOverlay()
{
    if (!surfaceExtrudeOptionsOverlay_)
    {
        return;
    }

    const std::optional<SurfaceRef> selectedSurface = selectedSingleSurfaceForExtrude();
    const bool selectionMatchesPreview =
        selectedSurface
        && surfaceExtrudePreviewActive_
        && sameSurfaceRef(*selectedSurface, surfaceExtrudePreviewSurface_);
    const bool active =
        selectionMatchesPreview
        && editMode_ == EditMode::surface
        && placementMode_ == PlacementMode::none
        && !selectionTouchesNonEditableLayer(document_.selection());

    if (surfaceExtrudePreviewActive_ && !active)
    {
        cancelSurfaceExtrudePreview();
        return;
    }

    surfaceExtrudeOptionsOverlay_->setVisible(active);
    if (!active)
    {
        return;
    }

    refreshSurfaceExtrudeGridSnapState(true);

    sizeSurfaceExtrudeSpin(surfaceExtrudeLengthSpin_);
    sizeSurfaceExtrudeSpin(surfaceExtrudeExpandSpin_);
    surfaceExtrudeOptionsOverlay_->adjustSize();
    positionSurfaceExtrudeOptionsOverlay();
}

void MainWindow::refreshSurfaceExtrudeGridSnapState(
    const bool quantizeValues)
{
    const std::optional<SurfaceRef> selectedSurface =
        selectedSingleSurfaceForExtrude();
    const ui::surface::SurfaceExtrudeExpandSpinConfig expandConfig =
        selectedSurface
            ? ui::surface::surfaceExtrudeExpandSpinConfig(
                document_.level(),
                *selectedSurface,
                -kSurfaceExtrudeValueLimit,
                kSurfaceExtrudeSingleStep)
            : ui::surface::SurfaceExtrudeExpandSpinConfig{
                .minimum = -kSurfaceExtrudeValueLimit,
                .singleStep = kSurfaceExtrudeSingleStep,
                .surfaceLimited = false,
            };

    surfaceExtrudeExpand_ = std::max(
        surfaceExtrudeExpand_, expandConfig.minimum);

    const bool gridSnapEnabled = viewportTransformGridSnapAction_
        && viewportTransformGridSnapAction_->isChecked()
        && glView_;
    const double gridSpacing = gridSnapEnabled
        ? glView_->activeGridSnapSpacing()
        : 0.0;
    const bool usableGridSpacing = std::isfinite(gridSpacing)
        && gridSpacing > 0.0;

    if (quantizeValues && usableGridSpacing)
    {
        surfaceExtrudeLength_ =
            ui::surface::quantizeSurfaceExtrudeScalarToGrid(
                surfaceExtrudeLength_,
                gridSpacing,
                -kSurfaceExtrudeValueLimit,
                kSurfaceExtrudeValueLimit);
        surfaceExtrudeExpand_ =
            ui::surface::quantizeSurfaceExtrudeScalarToGrid(
                surfaceExtrudeExpand_,
                gridSpacing,
                expandConfig.minimum,
                kSurfaceExtrudeValueLimit);
    }

    if (surfaceExtrudeGridSnapButton_)
    {
        const QSignalBlocker blocker(surfaceExtrudeGridSnapButton_);
        surfaceExtrudeGridSnapButton_->setChecked(gridSnapEnabled);
    }
    if (surfaceExtrudeLengthSpin_)
    {
        const QSignalBlocker blocker(surfaceExtrudeLengthSpin_);
        surfaceExtrudeLengthSpin_->setSingleStep(
            usableGridSpacing ? gridSpacing : kSurfaceExtrudeSingleStep);
        surfaceExtrudeLengthSpin_->setValue(surfaceExtrudeLength_);
    }
    if (surfaceExtrudeExpandSpin_)
    {
        const QSignalBlocker blocker(surfaceExtrudeExpandSpin_);
        surfaceExtrudeExpandSpin_->setRange(expandConfig.minimum, kSurfaceExtrudeValueLimit);
        surfaceExtrudeExpandSpin_->setSingleStep(
            usableGridSpacing ? gridSpacing : expandConfig.singleStep);
        surfaceExtrudeExpandSpin_->setValue(surfaceExtrudeExpand_);
    }

    updateSurfaceExtrudePreview();
}

void MainWindow::beginSurfaceExtrudePreview()
{
    if (!documentEditingAllowed())
    {
        return;
    }
    const auto clearExtrudeChecked = [this]()
    {
        if (!extrudeSurfaceAction_)
        {
            return;
        }
        const QSignalBlocker blocker(extrudeSurfaceAction_);
        extrudeSurfaceAction_->setChecked(false);
    };

    const std::optional<SurfaceRef> surfaceRef = selectedSingleSurfaceForExtrude();
    if (!surfaceRef)
    {
        clearExtrudeChecked();
        QMessageBox::information(this, "Extrude surface", "Select exactly one surface first.");
        return;
    }

    refreshSurfaceExtrudeGridSnapState(true);

    const ui::surface::SurfaceExtrudePreviewSetup previewSetup =
        ui::surface::prepareSurfaceExtrudePreview(
            document_.level(),
            *surfaceRef,
            surfaceExtrudeLength_,
            surfaceExtrudeExpand_);
    if (previewSetup.expand != surfaceExtrudeExpand_)
    {
        surfaceExtrudeExpand_ = previewSetup.expand;
        if (surfaceExtrudeExpandSpin_)
        {
            const QSignalBlocker blocker(surfaceExtrudeExpandSpin_);
            surfaceExtrudeExpandSpin_->setValue(surfaceExtrudeExpand_);
        }
    }

    if (!previewSetup.previewSector)
    {
        clearExtrudeChecked();
        QMessageBox::warning(this, "Extrude failed",
            ui::surface::surfaceExtrudeFailureMessage(previewSetup.report));
        return;
    }

    if (placementMode_ == PlacementMode::thing)
    {
        deselectThingPlacementTemplate();
    }
    else if (placementMode_ != PlacementMode::none)
    {
        setPlacementMode(PlacementMode::none);
    }
    deactivateViewportEditFunctions();
    deactivateViewportTransformForExclusiveTool();

    surfaceExtrudePreviewActive_ = true;
    surfaceExtrudePreviewSurface_ = *surfaceRef;
    if (extrudeSurfaceAction_)
    {
        const QSignalBlocker blocker(extrudeSurfaceAction_);
        extrudeSurfaceAction_->setChecked(true);
    }
    if (glView_)
    {
        glView_->setPlacementPreviewPersistent(true);
    }

    syncTransformMoveOptionsOverlay();
    syncSurfaceExtrudeOptionsOverlay();

    if (statusBar())
    {
        statusBar()->showMessage(
            previewSetup.resetCollapsedNegativeExpand
                ? "Surface Extrude expand was reset because it would collapse this surface."
                : "Surface extrude",
            2500);
    }
}

void MainWindow::updateSurfaceExtrudePreview()
{
    if (!surfaceExtrudePreviewActive_ || !glView_)
    {
        return;
    }

    level::SurfaceExtrudeReport report;
    std::optional<Sector> previewSector = sed::level::levelBuildSurfaceExtrudeSectorPreview(
        document_.level(), surfaceExtrudePreviewSurface_,
        surfaceExtrudeLength_, surfaceExtrudeExpand_, &report);
    const std::vector<TVector> lines = previewSector
        ? ui::surface::buildSurfaceExtrudePreviewLines(document_.level(),
            surfaceExtrudePreviewSurface_, surfaceExtrudeLength_, surfaceExtrudeExpand_)
        : std::vector<TVector>{};
    const bool validPreview = previewSector.has_value() && !lines.empty();
    if ( surfaceExtrudeApplyButton_ )
    {
        surfaceExtrudeApplyButton_->setEnabled(validPreview);
    }
    if ( !validPreview )
    {
        glView_->clearPlacementPreviewSector();
        glView_->clearPlacementPreviewLines();
        if ( report.error == level::SurfaceExtrudeError::None )
        {
            report = {level::SurfaceExtrudeError::InvalidResult, surfaceExtrudePreviewSurface_};
        }
        const QString message = ui::surface::surfaceExtrudeFailureMessage(report);
        if ( surfaceExtrudeApplyButton_ )
        {
            surfaceExtrudeApplyButton_->setProperty("extrudeFailureMessage", message);
            surfaceExtrudeApplyButton_->setToolTip(message);
        }
        if ( statusBar() )
        {
            statusBar()->showMessage(message);
        }
        return;
    }
    clearSurfaceExtrudeFailure(surfaceExtrudeApplyButton_, statusBar());

    ui::surface::SurfaceExtrudePreviewRenderPlan renderPlan =
        ui::surface::buildSurfaceExtrudePreviewRenderPlan(
            document_.level(),
            surfaceExtrudePreviewSurface_,
            surfaceExtrudeExpand_);

    glView_->setPlacementPreviewSector(
        std::move(previewSector),
        false,
        renderPlan.hiddenSourceSurface,
        std::move(renderPlan.sourceCutPreview));
    glView_->setPlacementPreviewLines(lines);
}

void MainWindow::cancelSurfaceExtrudePreview()
{
    clearSurfaceExtrudeFailure(surfaceExtrudeApplyButton_, statusBar());
    surfaceExtrudePreviewActive_ = false;
    if (extrudeSurfaceAction_)
    {
        const QSignalBlocker blocker(extrudeSurfaceAction_);
        extrudeSurfaceAction_->setChecked(false);
    }
    if (surfaceExtrudeOptionsOverlay_)
    {
        surfaceExtrudeOptionsOverlay_->hide();
    }
    syncTransformMoveOptionsOverlay();
    if (glView_)
    {
        glView_->setPlacementPreviewPersistent(false);
        glView_->clearPlacementPreviewSector();
        glView_->clearPlacementPreviewLines();
    }
    restoreViewportTransformAfterExclusiveTool();
}

void MainWindow::applySurfaceExtrudePreview()
{
    if (!surfaceExtrudePreviewActive_)
    {
        beginSurfaceExtrudePreview();
        return;
    }

    // Apply may be clicked while a text editor still owns focus. Commit its
    // exact pending number before reading the preview parameters, including
    // the same bounds and grid quantization used by normal editingFinished.
    if (surfaceExtrudeLengthSpin_)
    {
        surfaceExtrudeLengthSpin_->setValue(surfaceExtrudeLengthSpin_->value());
        surfaceExtrudeLength_ = surfaceExtrudeLengthSpin_->value();
    }
    if (surfaceExtrudeExpandSpin_)
    {
        surfaceExtrudeExpandSpin_->setValue(surfaceExtrudeExpandSpin_->value());
        surfaceExtrudeExpand_ = surfaceExtrudeExpandSpin_->value();
    }
    refreshSurfaceExtrudeGridSnapState(true);
    const SurfaceRef surfaceRef = surfaceExtrudePreviewSurface_;
    const double length = surfaceExtrudeLength_;
    const double expand = surfaceExtrudeExpand_;

    level::SurfaceExtrudeReport report;
    const bool applied = document_.applyUndoable(
        "Surface: Extruded",
        editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure,
        [&](editor::DocumentState& state) -> bool
        {
            if (!::sed::level::levelExtrudeSurface(state.level, surfaceRef, length, expand, &report))
            {
                return false;
            }

            state.selection.clear();
            return true;
        });

    if (!applied)
    {
        QMessageBox::warning(this, "Extrude failed",
            ui::surface::surfaceExtrudeFailureMessage(report));
        updateSurfaceExtrudePreview();
        return;
    }

    invalidateSurfaceSelectionContextForTopologyEdit();

    cancelSurfaceExtrudePreview();
    resetSurfaceExtrudeExpand(surfaceExtrudeExpandSpin_, surfaceExtrudeExpand_);

    if (glView_)
    {
        glView_->notifyLevelGeometryChanged();
        glView_->setSelection(-1, -1, -1, -1);
        glView_->setHighlightedSelection(document_.selection());
    }
    rebuildOutliner();
    rebuildProperties();
    updateEditActions();
}

std::optional<SurfaceRef> MainWindow::selectedSingleSurfaceForExtrude() const
{
    return ui::surface::singleSurfaceOnlySelection(document_.selection());
}

} // namespace sed
