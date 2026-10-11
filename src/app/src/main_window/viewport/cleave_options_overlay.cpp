#include "main_window/common.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/viewport/cleave_shape_popover.hpp"

#include <QButtonGroup>
#include <QFontMetrics>
#include <QLayout>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMargins>
#include <QStyle>
#include <QSignalBlocker>
#include <QToolButton>
#include <QWidget>

#include <algorithm>
#include <utility>
#include <vector>

namespace sed
{
namespace
{

class CleaveResponsiveFlowLayout final : public QLayout
{
public:
    explicit CleaveResponsiveFlowLayout(QWidget* parent)
        : QLayout(parent)
    {
        setContentsMargins(8, 6, 8, 6);
        setSpacing(5);
    }

    ~CleaveResponsiveFlowLayout() override
    {
        while (QLayoutItem* const item = takeAt(0))
        {
            delete item;
        }
    }

    void addItem(QLayoutItem* const item) override
    {
        items_.push_back(item);
    }

    int count() const override
    {
        return static_cast<int>(items_.size());
    }

    QLayoutItem* itemAt(const int index) const override
    {
        return index >= 0 && static_cast<std::size_t>(index) < items_.size()
            ? items_[static_cast<std::size_t>(index)]
            : nullptr;
    }

    QLayoutItem* takeAt(const int index) override
    {
        if (index < 0 || static_cast<std::size_t>(index) >= items_.size())
        {
            return nullptr;
        }

        QLayoutItem* const item = items_[static_cast<std::size_t>(index)];
        items_.erase(items_.begin() + index);
        return item;
    }

    Qt::Orientations expandingDirections() const override
    {
        return {};
    }

    bool hasHeightForWidth() const override
    {
        return true;
    }

    int heightForWidth(const int width) const override
    {
        return layoutItems(QRect(0, 0, width, 0), true);
    }

    QSize minimumSize() const override
    {
        QSize size{};
        for (QLayoutItem* const item : items_)
        {
            if (itemIsVisible(item))
            {
                size = size.expandedTo(item->minimumSize());
            }
        }
        const QMargins margins = contentsMargins();
        return size + QSize(
            margins.left() + margins.right(),
            margins.top() + margins.bottom());
    }

    QSize sizeHint() const override
    {
        const QMargins margins = contentsMargins();
        int naturalWidth = margins.left() + margins.right();
        bool hasItem = false;
        for (QLayoutItem* const item : items_)
        {
            if (!itemIsVisible(item))
            {
                continue;
            }
            if (hasItem)
            {
                naturalWidth += spacing();
            }
            naturalWidth += item->sizeHint().width();
            hasItem = true;
        }

        const QWidget* const overlay = parentWidget();
        const QWidget* const viewport = overlay ? overlay->parentWidget() : nullptr;
        const int availableWidth = viewport && viewport->width() > 0
            ? std::max(0, viewport->width() - 20)
            : naturalWidth;
        const int width = std::max(
            std::min(naturalWidth, availableWidth),
            std::min(minimumSize().width(), naturalWidth));
        return QSize(width, heightForWidth(width));
    }

    void setGeometry(const QRect& rect) override
    {
        QLayout::setGeometry(rect);
        layoutItems(rect, false);
    }

private:
    static bool itemIsVisible(const QLayoutItem* const item)
    {
        const QWidget* const widget = item ? item->widget() : nullptr;
        return item && (!widget || !widget->isHidden());
    }

    int layoutItems(const QRect& rect, const bool testOnly) const
    {
        struct RowItem
        {
            QLayoutItem* item{};
            QSize size{};
        };
        struct Row
        {
            std::vector<RowItem> items{};
            int width{};
            int height{};
        };

        const QMargins margins = contentsMargins();
        const QRect effectiveRect = rect.adjusted(
            margins.left(), margins.top(), -margins.right(), -margins.bottom());
        const int availableWidth = std::max(0, effectiveRect.width());
        std::vector<Row> rows{};
        Row currentRow{};

        for (QLayoutItem* const item : items_)
        {
            if (!itemIsVisible(item))
            {
                continue;
            }

            const QSize itemSize = item->sizeHint();
            const int itemWidth = itemSize.width()
                + (currentRow.items.empty() ? 0 : spacing());
            if (!currentRow.items.empty()
                && currentRow.width + itemWidth > availableWidth)
            {
                rows.push_back(std::move(currentRow));
                currentRow = Row{};
            }

            const int rowItemWidth = itemSize.width()
                + (currentRow.items.empty() ? 0 : spacing());
            currentRow.items.push_back(RowItem{ item, itemSize });
            currentRow.width += rowItemWidth;
            currentRow.height = std::max(currentRow.height, itemSize.height());
        }

        if (!currentRow.items.empty())
        {
            rows.push_back(std::move(currentRow));
        }

        int y = effectiveRect.y();
        for (const Row& row : rows)
        {
            int x = effectiveRect.x();
            for (const RowItem& rowItem : row.items)
            {
                if (!testOnly)
                {
                    const int itemY = y + (row.height - rowItem.size.height()) / 2;
                    rowItem.item->setGeometry(QRect(
                        QPoint(x, itemY), rowItem.size));
                }
                x += rowItem.size.width() + spacing();
            }
            y += row.height + spacing();
        }

        if (!rows.empty())
        {
            y -= spacing();
        }
        return y + margins.bottom() - rect.y();
    }

    std::vector<QLayoutItem*> items_{};
};

void sizeCleaveAngleSpin(ui::ScrubFloatEdit* const spin)
{
    if ( !spin || !spin->lineEdit() )
    {
        return;
    }

    QLineEdit* const lineEdit = spin->lineEdit();
    const QFontMetrics metrics(lineEdit->font());
    const int valueWidth = metrics.horizontalAdvance(QStringLiteral("360.0"));
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

    // Reserve the complete maximum angle plus a font-sized cursor inset. The
    // two stepper widths are part of ScrubFloatEdit's layout, so showing them
    // on hover or press cannot steal digits from the value.
    const int textWidth = std::max(valueWidth, 2 * stepperWidth)
        + metrics.averageCharWidth();
    const int lineEditWidth = textWidth
        + margins.left() + margins.right() + 2 * frameWidth + 2;
    spin->setFixedWidth(lineEditWidth + 2 * stepperWidth);
}

struct CleaveDimensionDisplay
{
    double scale{1.0};
    QString suffix{};
};

CleaveDimensionDisplay cleaveDimensionDisplay(const GlView::CleaveMeasureUnit unit)
{
    switch ( unit )
    {
        case GlView::CleaveMeasureUnit::Bare:
            return { 1.0, {} };
        case GlView::CleaveMeasureUnit::JKU:
            return { 10.0, QStringLiteral(" JKU") };
        case GlView::CleaveMeasureUnit::IJU:
            return { 10.0, QStringLiteral(" IJU") };
        case GlView::CleaveMeasureUnit::None:
        case GlView::CleaveMeasureUnit::Meters:
        default:
            return { 10.0, QStringLiteral(" m") };
    }
}

GlView::CleaveMeasureUnit syncCleaveUnitChoices(
    QComboBox* const combo,
    const ProjectType project,
    const GlView::CleaveMeasureUnit requestedUnit)
{
    using Unit = GlView::CleaveMeasureUnit;
    const Unit projectUnit = project == ProjectType::IJIM ? Unit::IJU : Unit::JKU;
    const QSignalBlocker blocker(combo);
    if ( combo->count() != 3
        || combo->itemData(0).toInt() != static_cast<int>(Unit::Meters)
        || combo->itemData(1).toInt() != static_cast<int>(projectUnit)
        || combo->itemData(2).toInt() != static_cast<int>(Unit::Bare) )
    {
        combo->clear();
        combo->addItem(QStringLiteral("Meters"), static_cast<int>(Unit::Meters));
        combo->addItem(project == ProjectType::IJIM
            ? QStringLiteral("IJU") : QStringLiteral("JKU"),
            static_cast<int>(projectUnit));
        combo->addItem(QStringLiteral("Bare"), static_cast<int>(Unit::Bare));
    }
    const int requestedIndex = combo->findData(static_cast<int>(requestedUnit));
    combo->setCurrentIndex(requestedIndex >= 0 ? requestedIndex : 0);
    return static_cast<Unit>(combo->currentData().toInt());
}

ui::viewport::CleaveShapeSpec cleaveShapeSpecFromOptions(
    const GlView::RecoveryState::CleaveShapeOptions& options)
{
    return {
        options.kind,
        options.width,
        options.height,
        options.dimensionsLinked,
        options.segments,
        options.points,
    };
}

} // namespace

void MainWindow::setupCleaveOptionsOverlay()
{
    if (!glView_ || !viewportSectorCleaveAction_ || !viewportSurfaceCleaveAction_)
    {
        return;
    }

    cleaveOptionsOverlay_ = new QWidget(glView_);
    cleaveOptionsOverlay_->setAttribute(Qt::WA_StyledBackground, true);
    cleaveOptionsOverlay_->setObjectName("cleaveOptionsOverlay");

    auto* layout = new CleaveResponsiveFlowLayout(cleaveOptionsOverlay_);

    auto* title = new QLabel(QStringLiteral("Cleave"), cleaveOptionsOverlay_);
    title->setObjectName("cleaveOptionsLabel");
    layout->addWidget(title);

    auto* separator = new QFrame(cleaveOptionsOverlay_);
    separator->setFrameShape(QFrame::VLine);
    separator->setObjectName("cleaveOptionsSeparator");
    layout->addWidget(separator);

    auto makeToggle = [this, layout](const char* objectName,
                                      const QString& text,
                                      const QString& tooltip) -> QToolButton*
    {
        auto* button = new QToolButton(cleaveOptionsOverlay_);
        button->setObjectName(QString::fromLatin1(objectName));
        button->setText(text);
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(tooltip);
        layout->addWidget(button);
        return button;
    };

    viewportCleaveStagedButton_ = makeToggle(
        "cleaveMultiCutButton",
        QStringLiteral("Multi-cut"),
        QStringLiteral(
            "<b>Multi-cut</b><br>Keep several cut lines editable until you apply or clear them. "
            "Apply commits them together as one edit."));
    viewportCleaveBoundedButton_ = makeToggle(
        "cleaveBoundedButton",
        QStringLiteral(""),
        QStringLiteral(
            "<b>Bounded cut</b><br>When enabled, earlier cuts stop later strokes from cutting pieces they do not reach. "
            "A stroke that crosses an earlier cut reaches pieces on both sides; turn this off to cut every piece crossed by the line's plane."));
    viewportCleaveBoundedButton_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::cleaveBounded));
    viewportCleaveBoundedButton_->setIconSize(QSize(22, 22));
    viewportCleaveBoundedButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    viewportCleaveBoundedButton_->setAccessibleName(QStringLiteral("Bounded cut"));
    viewportCleaveBoundedButton_->setAccessibleDescription(
        QStringLiteral("Limit each later cut to the pieces its line reaches."));
    viewportCleaveChainButton_ = makeToggle(
        "cleaveChainButton",
        QStringLiteral("Chain"),
        QStringLiteral(
            "<b>Chain</b><br>Start the next cut at the highlighted endpoint. Click any staged endpoint to choose it; "
            "your choice is remembered while Chain is off."));

    viewportCleaveAddShapeButton_ = makeToggle(
        "cleaveAddShapeButton",
        QStringLiteral(""),
        QStringLiteral(
            "<b>Add shape</b><br>Choose a centered shape, then click in the viewport to place it. "
            "The outline becomes bounded cut lines; a star is split into convex pieces."));
    viewportCleaveAddShapeButton_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::cleaveAddShape));
    viewportCleaveAddShapeButton_->setIconSize(QSize(22, 22));
    viewportCleaveAddShapeButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    viewportCleaveAddShapeButton_->setFixedSize(QSize(28, 28));
    viewportCleaveAddShapeButton_->setAccessibleName(QStringLiteral("Add shape"));
    viewportCleaveAddShapeButton_->setAccessibleDescription(
        QStringLiteral("Arm a centered shape for placement in the viewport."));

    viewportCleaveGridSnapButton_ = new QToolButton(cleaveOptionsOverlay_);
    viewportCleaveGridSnapButton_->setObjectName("cleaveGridSnapButton");
    viewportCleaveGridSnapButton_->setAutoRaise(true);
    viewportCleaveGridSnapButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    viewportCleaveGridSnapButton_->setCursor(Qt::PointingHandCursor);
    if ( viewportTransformGridSnapAction_ )
    {
        viewportCleaveGridSnapButton_->setDefaultAction(
            viewportTransformGridSnapAction_);
    }
    viewportCleaveGridSnapButton_->setAccessibleName(
        QStringLiteral("Grid snap for shape placement"));
    viewportCleaveGridSnapButton_->setAccessibleDescription(
        QStringLiteral("Toggle the shared viewport Grid Snap setting."));
    viewportCleaveGridSnapButton_->setToolTip(QStringLiteral(
        "<b>Grid snap</b><br>Snap the shape center to grid intersections. "
        "Hold Ctrl to temporarily reverse Grid Snap; hold Shift to snap to nearby geometry."));
    layout->addWidget(viewportCleaveGridSnapButton_);

    auto* cleaveTransformModeGroup = new QButtonGroup(cleaveOptionsOverlay_);
    cleaveTransformModeGroup->setExclusive(true);
    const auto makeCleaveTransformButton = [this, layout, cleaveTransformModeGroup](
        const char* objectName,
        const ui::ViewportIcon icon,
        const QString& accessibleName,
        const QString& tooltip) -> QToolButton*
    {
        auto* button = new QToolButton(cleaveOptionsOverlay_);
        button->setObjectName(QString::fromLatin1(objectName));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setIcon(ui::makeViewportIcon(icon));
        button->setIconSize(QSize(22, 22));
        button->setFixedSize(QSize(28, 28));
        button->setCursor(Qt::PointingHandCursor);
        button->setAccessibleName(accessibleName);
        button->setToolTip(tooltip);
        cleaveTransformModeGroup->addButton(button);
        layout->addWidget(button);
        return button;
    };
    viewportCleaveMoveButton_ = makeCleaveTransformButton(
        "cleaveMoveButton",
        ui::ViewportIcon::toolMove,
        QStringLiteral("Move selected cut lines"),
        QStringLiteral(
            "<b>Move</b><br>Move the selected cut lines together. Choose Move to leave Rotate or Scale."));
    viewportCleaveRotateButton_ = makeCleaveTransformButton(
        "cleaveRotateButton",
        ui::ViewportIcon::toolRotate,
        QStringLiteral("Rotate selected cut lines"),
        QStringLiteral(
            "<b>Rotate</b><br>Rotate selected cut lines around their shared center in the cut plane. "
            "The configured angle step applies by default."));
    viewportCleaveScaleButton_ = makeCleaveTransformButton(
        "cleaveScaleButton",
        ui::ViewportIcon::toolScale,
        QStringLiteral("Scale selected cut lines"),
        QStringLiteral(
            "<b>Scale</b><br>Resize selected cut lines uniformly around their shared center."));
    viewportCleaveMoveButton_->setChecked(true);

    cleaveShapePopover_ = new ui::viewport::CleaveShapePopover(glView_);
    cleaveShapePopover_->setAttribute(Qt::WA_StyledBackground, true);
    cleaveShapePopover_->setStyleSheet(
        "QFrame#cleaveShapePopover {"
        "  color: rgba(236, 238, 242, 245);"
        "  background: rgba(18, 18, 22, 245);"
        "  border: 1px solid rgba(255, 255, 255, 55);"
        "  border-radius: 8px;"
        "}"
        "QLabel { color: rgba(230, 232, 238, 220); }"
        "QComboBox, QDoubleSpinBox, QSpinBox { min-height: 22px; padding: 1px 4px; }"
        "QToolButton { min-width: 24px; min-height: 22px; padding: 1px; }");
    cleaveShapePopover_->hide();

    viewportCleaveUnitsCombo_ = new QComboBox(cleaveOptionsOverlay_);
    viewportCleaveUnitsCombo_->setObjectName("cleaveUnitsCombo");
    viewportCleaveUnitsCombo_->setToolTip(
        QStringLiteral(
            "<b>Length units</b><br>Choose how visible Multi-cut line lengths are formatted."));
    (void)syncCleaveUnitChoices(viewportCleaveUnitsCombo_, document_.level().kind,
        GlView::CleaveMeasureUnit::Meters);
    viewportCleaveUnitsCombo_->setMinimumContentsLength(6);
    layout->addWidget(viewportCleaveUnitsCombo_);

    viewportCleaveShowAnglesButton_ = makeToggle(
        "cleaveShowAnglesButton",
        QStringLiteral(""),
        QStringLiteral(
            "<b>Angle guides</b><br>Show or hide the angle guides and labels for staged cut lines."));
    viewportCleaveShowAnglesButton_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::cleaveAngle));
    viewportCleaveShowAnglesButton_->setIconSize(QSize(20, 20));
    viewportCleaveShowAnglesButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    viewportCleaveShowAnglesButton_->setAccessibleName(
        QStringLiteral("Show Multi-cut angle guides and labels"));
    viewportCleaveShowAnglesButton_->setAccessibleDescription(
        QStringLiteral("Toggle Multi-cut angle guides and labels."));
    viewportCleaveShowLengthsButton_ = makeToggle(
        "cleaveShowLengthsButton",
        QStringLiteral(""),
        QStringLiteral(
            "<b>Line lengths</b><br>Show or hide staged line-length labels. Units changes their format."));
    viewportCleaveShowLengthsButton_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::cleaveLength));
    viewportCleaveShowLengthsButton_->setIconSize(QSize(20, 20));
    viewportCleaveShowLengthsButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    viewportCleaveShowLengthsButton_->setAccessibleName(
        QStringLiteral("Show Multi-cut line-length labels"));
    viewportCleaveShowLengthsButton_->setAccessibleDescription(
        QStringLiteral("Toggle Multi-cut line-length labels."));
    viewportCleaveShowNumbersButton_ = makeToggle(
        "cleaveShowNumbersButton",
        QStringLiteral(""),
        QStringLiteral(
            "<b>Line order</b><br>Show the order cuts will be applied. Click a number to edit that cut's position."));
    viewportCleaveShowNumbersButton_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::cleaveLineNumber));
    viewportCleaveShowNumbersButton_->setIconSize(QSize(20, 20));
    viewportCleaveShowNumbersButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    viewportCleaveShowNumbersButton_->setAccessibleName(
        QStringLiteral("Show 1-based Multi-cut line numbers"));
    viewportCleaveShowNumbersButton_->setAccessibleDescription(
        QStringLiteral("Toggle 1-based Multi-cut line numbers."));

    viewportCleaveAngleStepSpin_ = new ui::ScrubFloatEdit(cleaveOptionsOverlay_);
    viewportCleaveAngleStepSpin_->setObjectName("cleaveAngleStepSpin");
    viewportCleaveAngleStepSpin_->setRange(0.0, 360.0);
    viewportCleaveAngleStepSpin_->setDecimals(1);
    viewportCleaveAngleStepSpin_->setSingleStep(5.0);
    viewportCleaveAngleStepSpin_->setAccessibleName(QStringLiteral("Angle step in degrees (0 = Free)"));
    viewportCleaveAngleStepSpin_->setToolTip(
        QStringLiteral(
            "<b>Angle step</b><br>Set the angle interval for new cut lines. Hold Ctrl Shift while editing an endpoint to snap its angle to this step. "
            "Set to 0 (Free) to turn angle snapping off."));
    viewportCleaveAngleStepSpin_->setFixedWidth(92);
    layout->addWidget(viewportCleaveAngleStepSpin_);
    auto* angleUnitLabel = new QLabel(QString(QChar(0x00b0)), cleaveOptionsOverlay_);
    angleUnitLabel->setObjectName("cleaveAngleUnitLabel");
    layout->addWidget(angleUnitLabel);

    viewportCleaveApplyButton_ = new QToolButton(cleaveOptionsOverlay_);
    viewportCleaveApplyButton_->setObjectName("cleaveApplyButton");
    viewportCleaveApplyButton_->setText(QStringLiteral("Apply"));
    viewportCleaveApplyButton_->setToolTip(QStringLiteral(
        "<b>Apply cuts</b><br>Apply all staged cut lines in the displayed order as one undoable edit."));
    viewportCleaveApplyButton_->setCursor(Qt::PointingHandCursor);
    layout->addWidget(viewportCleaveApplyButton_);

    viewportCleaveCancelButton_ = new QToolButton(cleaveOptionsOverlay_);
    viewportCleaveCancelButton_->setObjectName("cleaveCancelButton");
    viewportCleaveCancelButton_->setText(QStringLiteral("Clear"));
    viewportCleaveCancelButton_->setToolTip(QStringLiteral(
        "<b>Clear cuts</b><br>Discard every staged cut line without changing the level. "
        "Undo can restore lines cleared during this draft session."));
    viewportCleaveCancelButton_->setCursor(Qt::PointingHandCursor);
    layout->addWidget(viewportCleaveCancelButton_);

    const bool staged = settingsStore().value(
        "cleave/multi_cut_mode",
        settingsStore().value("cleave/staged_mode", false)).toBool();
    const bool chained = settingsStore().value("cleave/chain", false).toBool();
    const bool bounded = settingsStore().value("cleave/bounded", true).toBool();
    QSettings cleaveSettings = settingsStore();
    const bool hasLegacyMeasurements = cleaveSettings.contains("cleave/show_measurements");
    const bool legacyMeasurements = cleaveSettings.value(
        "cleave/show_measurements", true).toBool();
    bool showAngles = cleaveSettings.value(
        "cleave/show_angles", legacyMeasurements).toBool();
    bool showLengths = cleaveSettings.value(
        "cleave/show_lengths", legacyMeasurements).toBool();
    const bool showLineNumbers = cleaveSettings.value(
        "cleave/show_line_numbers", false).toBool();
    bool storedUnitOk = false;
    const int storedUnit = cleaveSettings.value(
        "cleave/measure_unit",
        static_cast<int>(GlView::CleaveMeasureUnit::Meters)).toInt(&storedUnitOk);
    const int firstUnit = static_cast<int>(GlView::CleaveMeasureUnit::Bare);
    const int noneUnit = static_cast<int>(GlView::CleaveMeasureUnit::None);
    const bool invalidUnit = !storedUnitOk || storedUnit < firstUnit || storedUnit > noneUnit;
    const bool legacyNoneUnit = storedUnit == noneUnit;
    const int effectiveUnit = (invalidUnit || legacyNoneUnit)
        ? static_cast<int>(GlView::CleaveMeasureUnit::Meters)
        : storedUnit;
    if (!legacyMeasurements || legacyNoneUnit)
    {
        showLengths = false;
    }
    if (hasLegacyMeasurements || legacyNoneUnit || invalidUnit)
    {
        // Migrate the obsolete combined visibility flag once. Explicit angle
        // and number choices survive, and Lengths becomes the only authority
        // for whether length labels are drawn.
        cleaveSettings.setValue("cleave/show_angles", showAngles);
        cleaveSettings.setValue("cleave/show_lengths", showLengths);
        cleaveSettings.setValue("cleave/show_line_numbers", showLineNumbers);
        cleaveSettings.setValue("cleave/measure_unit", effectiveUnit);
        cleaveSettings.remove("cleave/show_measurements");
        cleaveSettings.sync();
    }
    const double angleStep = settingsStore().value("cleave/angle_step_degrees", 0.0).toDouble();
    viewportCleaveStagedButton_->setChecked(staged);
    viewportCleaveBoundedButton_->setChecked(bounded);
    viewportCleaveChainButton_->setChecked(chained);
    viewportCleaveShowAnglesButton_->setChecked(showAngles);
    viewportCleaveShowLengthsButton_->setChecked(showLengths);
    viewportCleaveShowNumbersButton_->setChecked(showLineNumbers);
    (void)syncCleaveUnitChoices(viewportCleaveUnitsCombo_, document_.level().kind,
        static_cast<GlView::CleaveMeasureUnit>(effectiveUnit));
    viewportCleaveAngleStepSpin_->setValue(std::clamp(angleStep, 0.0, 360.0));
    angleUnitLabel->setText(viewportCleaveAngleStepSpin_->value() == 0.0
        ? QStringLiteral("Free") : QString(QChar(0x00b0)));
    glView_->resetStagedCleaveWorkspaces();
    const auto initialUnit = glView_->cleaveMeasureUnit();
    const auto initialDisplay = cleaveDimensionDisplay(initialUnit);
    cleaveShapePopover_->setShapeSpec(
        cleaveShapeSpecFromOptions(glView_->cleaveShapeOptions()),
        initialDisplay.scale,
        initialDisplay.suffix);
    cleaveShapePopover_->setShapeSpecChangedHandler(
        [this](const ui::viewport::CleaveShapeSpec& spec)
        {
            if ( !glView_ )
            {
                return;
            }

            auto options = glView_->cleaveShapeOptions();
            options.kind = spec.kind;
            options.width = spec.width;
            options.height = spec.height;
            options.dimensionsLinked = spec.dimensionsLinked;
            options.segments = spec.segments;
            options.points = spec.points;
            glView_->setCleaveShapeOptions(options);
            if ( options.placementArmed )
            {
                const auto plan = ui::viewport::makeCleaveShapePlan(spec);
                if ( plan )
                {
                    (void)glView_->setStagedCleaveShapePlan(*plan);
                }
            }
        });
    cleaveShapePopover_->setCameraPlaneChangedHandler(
        [this](const bool enabled)
        {
            if ( glView_ )
            {
                auto options = glView_->cleaveShapeOptions();
                options.cameraPlane = enabled;
                glView_->setCleaveShapeOptions(options);
            }
        });

    connect(viewportCleaveAddShapeButton_, &QToolButton::toggled, this,
        [this](const bool checked)
        {
            if ( !glView_ )
            {
                return;
            }

            auto options = glView_->cleaveShapeOptions();
            options.placementArmed = checked;
            glView_->setCleaveShapeOptions(options);
            if ( checked && cleaveShapePopover_ )
            {
                const auto plan = ui::viewport::makeCleaveShapePlan(
                    cleaveShapePopover_->shapeSpec());
                if ( plan )
                {
                    (void)glView_->setStagedCleaveShapePlan(*plan);
                }
            }
            syncCleaveOptionsOverlay();
        });
    connect(viewportCleaveMoveButton_, &QToolButton::toggled, this,
        [this](const bool checked)
        {
            if ( checked && glView_ )
            {
                glView_->setStagedCleaveTransformMode(
                    GlView::StagedCleaveTransformMode::Move);
            }
        });
    connect(viewportCleaveRotateButton_, &QToolButton::toggled, this,
        [this](const bool checked)
        {
            if ( checked && glView_ )
            {
                glView_->setStagedCleaveTransformMode(
                    GlView::StagedCleaveTransformMode::Rotate);
            }
        });
    connect(viewportCleaveScaleButton_, &QToolButton::toggled, this,
        [this](const bool checked)
        {
            if ( checked && glView_ )
            {
                glView_->setStagedCleaveTransformMode(
                    GlView::StagedCleaveTransformMode::Scale);
            }
        });
    connect(glView_, &GlView::viewportStateChanged, this,
        [this]()
        {
            syncCleaveOptionsOverlay();
        });

    connect(viewportCleaveStagedButton_, &QToolButton::toggled, this, [this](const bool checked)
        {
            settingsStore().setValue("cleave/multi_cut_mode", checked);
            if (glView_)
            {
                glView_->setCleaveStagedMode(checked);
            }
            syncCleaveOptionsOverlay();
        });
    connect(viewportCleaveBoundedButton_, &QToolButton::toggled, this,
        [this](const bool checked)
        {
            settingsStore().setValue("cleave/bounded", checked);
            if ( glView_ )
            {
                glView_->setCleaveBounded(checked);
            }
        });
    connect(viewportCleaveChainButton_, &QToolButton::toggled, this, [this](const bool checked)
        {
            settingsStore().setValue("cleave/chain", checked);
            if (glView_)
            {
                glView_->setCleaveChainEnabled(checked);
            }
        });
    connect(viewportCleaveShowAnglesButton_, &QToolButton::toggled, this,
        [this](const bool checked)
        {
            settingsStore().setValue("cleave/show_angles", checked);
            if (glView_)
            {
                glView_->setCleaveAnglesVisible(checked);
            }
        });
    connect(viewportCleaveShowLengthsButton_, &QToolButton::toggled, this,
        [this](const bool checked)
        {
            settingsStore().setValue("cleave/show_lengths", checked);
            if (glView_)
            {
                glView_->setCleaveLengthsVisible(checked);
            }
        });
    connect(viewportCleaveShowNumbersButton_, &QToolButton::toggled, this,
        [this](const bool checked)
        {
            settingsStore().setValue("cleave/show_line_numbers", checked);
            if (glView_)
            {
                glView_->setCleaveLineNumbersVisible(checked);
            }
        });
    connect(viewportCleaveUnitsCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
        [this](const int)
        {
            if (!viewportCleaveUnitsCombo_ || !glView_)
            {
                return;
            }
            const auto unit = static_cast<GlView::CleaveMeasureUnit>(
                viewportCleaveUnitsCombo_->currentData().toInt());
            settingsStore().setValue("cleave/measure_unit", static_cast<int>(unit));
            glView_->setCleaveMeasureUnit(unit);
            if ( cleaveShapePopover_ )
            {
                const auto display = cleaveDimensionDisplay(unit);
                cleaveShapePopover_->setShapeSpec(
                    cleaveShapeSpecFromOptions(glView_->cleaveShapeOptions()),
                    display.scale,
                    display.suffix);
            }
        });
    const auto changeAngleStep = [this, angleUnitLabel](const double value)
    {
        angleUnitLabel->setText(value == 0.0 ? QStringLiteral("Free") : QString(QChar(0x00b0)));
        settingsStore().setValue("cleave/angle_step_degrees", value);
        if (glView_) glView_->setCleaveAngleStepDegrees(value);
    };
    connect(viewportCleaveAngleStepSpin_, &ui::ScrubFloatEdit::valueCommitted, this, changeAngleStep);
    connect(viewportCleaveAngleStepSpin_, &ui::ScrubFloatEdit::scrubValueChanged, this, changeAngleStep);
    connect(viewportCleaveApplyButton_, &QToolButton::clicked, this, [this]()
        {
            if (glView_)
            {
                glView_->requestApplyStagedCleave();
            }
        });
    connect(viewportCleaveCancelButton_, &QToolButton::clicked, this, [this]()
        {
            if (glView_)
            {
                glView_->clearStagedCleaveDraft();
            }
        });
    connect(viewportSectorCleaveAction_, &QAction::toggled, this, [this](const bool)
        {
            syncCleaveOptionsOverlay();
        });
    connect(viewportSurfaceCleaveAction_, &QAction::toggled, this, [this](const bool)
        {
            syncCleaveOptionsOverlay();
        });

    cleaveOptionsOverlay_->setStyleSheet(
        "QWidget#cleaveOptionsOverlay {"
        "  background: rgba(18, 18, 22, 235);"
        "  border: 1px solid rgba(255, 255, 255, 40);"
        "  border-radius: 8px;"
        "}"
        "QLabel#cleaveOptionsLabel { color: rgba(230, 232, 238, 220); padding-left: 2px; }"
        "QFrame#cleaveOptionsSeparator { color: rgba(255, 255, 255, 32); }"
        "QToolButton {"
        "  color: rgba(236, 238, 242, 245); background: transparent;"
        "  border: 1px solid transparent; border-radius: 5px; padding: 3px 8px;"
        "}"
        "QToolButton:hover { background: rgba(255, 255, 255, 24); }"
        "QToolButton:checked { background: rgba(90, 145, 220, 105); border-color: rgba(150, 195, 255, 90); }"
        "QToolButton#cleaveAddShapeButton, QToolButton#cleaveMoveButton, "
        "QToolButton#cleaveRotateButton, QToolButton#cleaveScaleButton { padding: 0px; }"
        "QToolButton#cleaveApplyButton { background: rgba(54, 116, 74, 150); border-color: rgba(100, 200, 130, 100); }"
        "QToolButton#cleaveCancelButton { background: rgba(110, 55, 55, 110); border-color: rgba(220, 110, 100, 80); }"
        "QToolButton[sedButtonRole=\"stepper\"] { min-width: 0px; padding: 0px 2px; }"
        "QComboBox, QLineEdit { min-height: 22px; padding: 1px 6px; }"
    );

    cleaveOptionsOverlay_->ensurePolished();
    const QSize gridFootprint = viewportCleaveGridSnapButton_->sizeHint();
    viewportCleaveGridSnapButton_->setStyleSheet(QStringLiteral("QToolButton { padding: 0px; }"));
    viewportCleaveGridSnapButton_->setIconSize(QSize(22, 22));
    viewportCleaveGridSnapButton_->setFixedSize(gridFootprint);
    viewportCleaveBoundedButton_->setStyleSheet(QStringLiteral("QToolButton { padding: 0px; }"));
    viewportCleaveBoundedButton_->setFixedSize(QSize(28, viewportCleaveStagedButton_->sizeHint().height()));
    sizeCleaveAngleSpin(viewportCleaveAngleStepSpin_);
    cleaveOptionsOverlay_->hide();
    syncCleaveOptionsOverlay();
}

void MainWindow::positionCleaveOptionsOverlay()
{
    if (!glView_ || !cleaveOptionsOverlay_)
    {
        return;
    }

    cleaveOptionsOverlay_->adjustSize();
    constexpr int margin = 10;
    int x = std::max(margin, (glView_->width() - cleaveOptionsOverlay_->width()) / 2);
    int y = margin;
    if (viewportOverlay_ && viewportOverlay_->isVisible())
    {
        const QRect targetRect(x, y, cleaveOptionsOverlay_->width(), cleaveOptionsOverlay_->height());
        const QRect avoidRect = viewportOverlay_->geometry().adjusted(-margin, -margin, margin, margin);
        if (targetRect.intersects(avoidRect))
        {
            y = avoidRect.bottom() + margin;
        }
    }
    cleaveOptionsOverlay_->move(x, y);
    reflowTopCenterOverlays();
    if ( QLayout* const flow = cleaveOptionsOverlay_->layout() )
    {
        // Resolve wrapped rows at their final width before anchoring the popup.
        const int height = flow->heightForWidth(cleaveOptionsOverlay_->width());
        if ( height >= 0 )
        {
            cleaveOptionsOverlay_->resize(cleaveOptionsOverlay_->width(), height);
        }
        flow->setGeometry(cleaveOptionsOverlay_->rect());
    }
    if ( cleaveShapePopover_ && cleaveShapePopover_->isVisible() )
    {
        cleaveShapePopover_->adjustSize();
        const int popupWidth = cleaveShapePopover_->width();
        const int popupHeight = cleaveShapePopover_->height();
        const int maxX = std::max(margin, glView_->width() - popupWidth - margin);
        const int maxY = std::max(margin, glView_->height() - popupHeight - margin);
        int popupX = std::clamp(
            cleaveOptionsOverlay_->geometry().center().x() - popupWidth / 2,
            margin,
            maxX);
        int popupY = cleaveOptionsOverlay_->geometry().bottom() + margin;
        if ( popupY + popupHeight > glView_->height() - margin )
        {
            popupY = cleaveOptionsOverlay_->geometry().top() - popupHeight - margin;
        }
        popupY = std::clamp(popupY, margin, maxY);
        popupX = std::clamp(popupX, margin, maxX);
        cleaveShapePopover_->move(popupX, popupY);
        cleaveShapePopover_->raise();
    }
}

void MainWindow::syncCleaveOptionsOverlay()
{
    // Recovery publishes GlView before its QAction owner. A viewportStateChanged
    // emitted during that guarded interval must not interpret the temporarily
    // unchecked Cleave actions as an instruction to cancel the restored draft.
    // syncViewportContextualOverlays() performs the final sync once ownership,
    // mode, and option widgets have all been restored.
    if (recoveryViewportStateRestoreInProgress_)
    {
        return;
    }
    if (!cleaveOptionsOverlay_)
    {
        return;
    }

    const bool sectorActive = viewportSectorCleaveAction_
        && viewportSectorCleaveAction_->isChecked();
    const bool surfaceActive = viewportSurfaceCleaveAction_
        && viewportSurfaceCleaveAction_->isChecked();
    const bool active = glView_
        && glView_->renderPipeline() == GlView::RenderPipeline::Editor
        && (sectorActive || surfaceActive);
    if ( glView_ && viewportCleaveUnitsCombo_ )
    {
        const auto selectedUnit = syncCleaveUnitChoices(viewportCleaveUnitsCombo_,
            document_.level().kind, glView_->cleaveMeasureUnit());
        if ( active && glView_->cleaveMeasureUnit() != selectedUnit )
        {
            glView_->setCleaveMeasureUnit(selectedUnit);
        }
    }
    if ( glView_ )
    {
        // The active family's workspace owns options. Publishing a widget must
        // never feed its preceding family's value back into the new workspace.
        const auto syncChecked = [](QToolButton* button, const bool checked)
        {
            if ( !button ) return;
            const QSignalBlocker blocker(button);
            button->setChecked(checked);
        };
        syncChecked(viewportCleaveStagedButton_, glView_->cleaveStagedMode());
        syncChecked(viewportCleaveChainButton_, glView_->cleaveChainEnabled());
        syncChecked(viewportCleaveBoundedButton_, glView_->cleaveBounded());
        syncChecked(viewportCleaveShowAnglesButton_, glView_->cleaveAnglesVisible());
        syncChecked(viewportCleaveShowLengthsButton_, glView_->cleaveLengthsVisible());
        syncChecked(viewportCleaveShowNumbersButton_, glView_->cleaveLineNumbersVisible());
        if ( viewportCleaveAngleStepSpin_
            && viewportCleaveAngleStepSpin_->value() != glView_->cleaveAngleStepDegrees() )
        {
            const QSignalBlocker blocker(viewportCleaveAngleStepSpin_);
            viewportCleaveAngleStepSpin_->setValue(glView_->cleaveAngleStepDegrees());
        }
    }
    cleaveOptionsOverlay_->setVisible(active);
    if (!active)
    {
        if ( cleaveShapePopover_ )
        {
            cleaveShapePopover_->hide();
        }
        return;
    }

    const bool edgeMode = surfaceActive && surfaceSelectionMode_ == SurfaceSelectionMode::edge;
    if (viewportCleaveStagedButton_)
    {
        viewportCleaveStagedButton_->setEnabled(!edgeMode);
        viewportCleaveStagedButton_->setToolTip(edgeMode
            ? QStringLiteral(
                  "<b>Multi-cut</b><br>Surface Edge cuts are immediate. Multi-cut is available for sectors and Surface Faces.")
            : QStringLiteral(
                  "<b>Multi-cut</b><br>Keep several cut lines editable until you apply or clear them. "
                  "Apply commits them together as one edit."));
    }
    const bool staged = glView_->cleaveStagedMode() && !edgeMode;
    if (viewportCleaveChainButton_)
    {
        viewportCleaveChainButton_->setVisible(staged);
    }
    if ( viewportCleaveBoundedButton_ )
    {
        viewportCleaveBoundedButton_->setVisible(staged);
    }
    if (viewportCleaveApplyButton_)
    {
        viewportCleaveApplyButton_->setVisible(staged);
    }
    if (viewportCleaveAngleStepSpin_)
    {
        viewportCleaveAngleStepSpin_->setVisible(staged);
        sizeCleaveAngleSpin(viewportCleaveAngleStepSpin_);
    }
    if (auto* label = cleaveOptionsOverlay_->findChild<QLabel*>("cleaveAngleUnitLabel"))
    {
        label->setVisible(staged);
        label->setText(glView_->cleaveAngleStepDegrees() == 0.0
            ? QStringLiteral("Free") : QString(QChar(0x00b0)));
    }
    if (viewportCleaveCancelButton_)
    {
        viewportCleaveCancelButton_->setVisible(staged);
    }
    if ( viewportCleaveAddShapeButton_ )
    {
        const QSignalBlocker blocker(viewportCleaveAddShapeButton_);
        viewportCleaveAddShapeButton_->setVisible(staged);
        viewportCleaveAddShapeButton_->setChecked(
            glView_->cleaveShapeOptions().placementArmed);
    }
    if ( glView_ && cleaveShapePopover_ )
    {
        const auto display = cleaveDimensionDisplay(glView_->cleaveMeasureUnit());
        cleaveShapePopover_->setShapeSpec(
            cleaveShapeSpecFromOptions(glView_->cleaveShapeOptions()),
            display.scale,
            display.suffix);
        cleaveShapePopover_->setCameraPlanePlacement(
            glView_->cleaveShapeOptions().cameraPlane, sectorActive);
        cleaveShapePopover_->setVisible(
            staged && glView_->cleaveShapeOptions().placementArmed);
    }
    if ( viewportCleaveGridSnapButton_ )
    {
        viewportCleaveGridSnapButton_->setVisible(staged);
        viewportCleaveGridSnapButton_->setEnabled(
            staged && viewportTransformGridSnapAction_);
    }
    const auto transformMode = glView_->stagedCleaveTransformMode();
    if ( staged && transformMode != GlView::StagedCleaveTransformMode::Move
        && !glView_->canTransformSelectedStagedCleave(transformMode) )
    {
        glView_->setStagedCleaveTransformMode(GlView::StagedCleaveTransformMode::Move);
    }
    const auto syncTransformButton = [this, staged](
        QToolButton* button,
        const GlView::StagedCleaveTransformMode mode,
        const bool canTransform)
    {
        if ( !button || !glView_ )
        {
            return;
        }
        button->setVisible(staged);
        button->setEnabled(staged && canTransform);
        const QSignalBlocker blocker(button);
        button->setChecked(glView_->stagedCleaveTransformMode() == mode);
    };
    syncTransformButton(
        viewportCleaveMoveButton_,
        GlView::StagedCleaveTransformMode::Move,
        true);
    syncTransformButton(
        viewportCleaveRotateButton_,
        GlView::StagedCleaveTransformMode::Rotate,
        glView_->canTransformSelectedStagedCleave(
            GlView::StagedCleaveTransformMode::Rotate));
    syncTransformButton(
        viewportCleaveScaleButton_,
        GlView::StagedCleaveTransformMode::Scale,
        glView_->canTransformSelectedStagedCleave(
            GlView::StagedCleaveTransformMode::Scale));

    cleaveOptionsOverlay_->adjustSize();
    positionCleaveOptionsOverlay();
    cleaveOptionsOverlay_->raise();
}

} // namespace sed
