#include "ui/uv/smart_uv_project_dialog.hpp"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace sed::ui::uv
{

SmartUvProjectDialog::SmartUvProjectDialog(
    const SmartUvProjectOptions& initialOptions,
    QWidget* parent)
    : QDialog(parent)
{
    setObjectName("smartUvProjectDialog");
    setWindowTitle("Smart UV Project");
    setMinimumWidth(430);

    auto* rootLayout = new QVBoxLayout(this);
    auto* summary = new QLabel(
        "Split selected faces into low-distortion projection groups, then pack the resulting UV islands.",
        this);
    summary->setObjectName("smartUvSummaryLabel");
    summary->setWordWrap(true);
    summary->setStyleSheet("color:#aeb9c8;");
    rootLayout->addWidget(summary);

    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    rootLayout->addLayout(form);

    angleLimitSpin_ = new QDoubleSpinBox(this);
    angleLimitSpin_->setObjectName("smartUvAngleLimitSpin");
    angleLimitSpin_->setRange(0.0, 89.0);
    angleLimitSpin_->setDecimals(1);
    angleLimitSpin_->setSingleStep(1.0);
    angleLimitSpin_->setValue(initialOptions.angleLimitDeg);
    angleLimitSpin_->setSuffix("°");
    angleLimitSpin_->setToolTip("Maximum normal-angle span before Blender-style projection groups split.");
    form->addRow("Angle Limit", angleLimitSpin_);

    marginMethodCombo_ = new QComboBox(this);
    marginMethodCombo_->setObjectName("smartUvMarginMethodCombo");
    marginMethodCombo_->addItem("Scaled", static_cast<int>(SmartUvMarginMethod::scaled));
    marginMethodCombo_->addItem("Add", static_cast<int>(SmartUvMarginMethod::add));
    marginMethodCombo_->addItem("Fraction", static_cast<int>(SmartUvMarginMethod::fraction));
    marginMethodCombo_->setCurrentIndex(static_cast<int>(initialOptions.marginMethod));
    marginMethodCombo_->setToolTip(
        "Scaled follows island size, Add uses UV-space distance, and Fraction targets a fraction of the unit square.");
    form->addRow("Margin Method", marginMethodCombo_);

    rotationMethodCombo_ = new QComboBox(this);
    rotationMethodCombo_->setObjectName("smartUvRotationMethodCombo");
    rotationMethodCombo_->addItem("Axis-aligned", static_cast<int>(SmartUvRotationMethod::axisAligned));
    rotationMethodCombo_->addItem(
        "Axis-aligned (Horizontal)",
        static_cast<int>(SmartUvRotationMethod::axisAlignedHorizontal));
    rotationMethodCombo_->addItem(
        "Axis-aligned (Vertical)",
        static_cast<int>(SmartUvRotationMethod::axisAlignedVertical));
    rotationMethodCombo_->addItem(
        "Geometry (World Vertical, Recommended)",
        static_cast<int>(SmartUvRotationMethod::geometryWorldVertical));
    const int initialRotationIndex = rotationMethodCombo_->findData(
        static_cast<int>(initialOptions.rotationMethod));
    rotationMethodCombo_->setCurrentIndex(std::max(0, initialRotationIndex));
    rotationMethodCombo_->setToolTip(
        "Axis-aligned modes match Blender's island-bound orientation. Geometry (World Vertical) follows upward model edges and preserves a reliable existing up/down texture sign.");
    form->addRow("Rotation Method", rotationMethodCombo_);

    islandMarginSpin_ = new QDoubleSpinBox(this);
    islandMarginSpin_->setObjectName("smartUvIslandMarginSpin");
    islandMarginSpin_->setRange(0.0, 1.0);
    islandMarginSpin_->setDecimals(4);
    islandMarginSpin_->setSingleStep(0.001);
    islandMarginSpin_->setValue(initialOptions.islandMargin);
    islandMarginSpin_->setToolTip("Empty space between packed islands, interpreted by Margin Method.");
    form->addRow("Island Margin", islandMarginSpin_);

    areaWeightSpin_ = new QDoubleSpinBox(this);
    areaWeightSpin_->setObjectName("smartUvAreaWeightSpin");
    areaWeightSpin_->setRange(0.0, 1.0);
    areaWeightSpin_->setDecimals(3);
    areaWeightSpin_->setSingleStep(0.05);
    areaWeightSpin_->setValue(initialOptions.areaWeight);
    areaWeightSpin_->setToolTip("Blend projection normals toward larger faces. Zero is an unweighted average.");
    form->addRow("Area Weight", areaWeightSpin_);

    correctAspectCheck_ = new QCheckBox(this);
    correctAspectCheck_->setObjectName("smartUvCorrectAspectCheck");
    correctAspectCheck_->setChecked(initialOptions.correctAspect);
    correctAspectCheck_->setToolTip("Compensate for the active surface texture's non-square dimensions.");
    form->addRow("Correct Aspect", correctAspectCheck_);

    scaleToBoundsCheck_ = new QCheckBox(this);
    scaleToBoundsCheck_->setObjectName("smartUvScaleToBoundsCheck");
    scaleToBoundsCheck_->setChecked(initialOptions.scaleToBounds);
    scaleToBoundsCheck_->setToolTip("Stretch the final packed layout to the full 0–1 UV bounds.");
    form->addRow("Scale to Bounds", scaleToBoundsCheck_);

    islandScaleSpin_ = new QDoubleSpinBox(this);
    islandScaleSpin_->setObjectName("smartUvIslandScaleSpin");
    islandScaleSpin_->setRange(0.01, 100.0);
    islandScaleSpin_->setDecimals(3);
    islandScaleSpin_->setSingleStep(0.25);
    islandScaleSpin_->setValue(initialOptions.islandScale);
    islandScaleSpin_->setSuffix("×");
    islandScaleSpin_->setToolTip(
        "Uniformly scale every final connected UV island after packing. The initial value is estimated from the current selection's UV edge scale; values above 1 make textures repeat more densely.");
    form->addRow("Island Scale", islandScaleSpin_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName("smartUvDialogButtons");
    if (QPushButton* unwrap = buttons->button(QDialogButtonBox::Ok))
    {
        unwrap->setText("Unwrap");
    }
    rootLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

SmartUvProjectOptions SmartUvProjectDialog::options() const
{
    SmartUvProjectOptions value{};
    value.angleLimitDeg = angleLimitSpin_->value();
    value.marginMethod = static_cast<SmartUvMarginMethod>(marginMethodCombo_->currentData().toInt());
    value.rotationMethod = static_cast<SmartUvRotationMethod>(rotationMethodCombo_->currentData().toInt());
    value.islandMargin = islandMarginSpin_->value();
    value.areaWeight = areaWeightSpin_->value();
    value.correctAspect = correctAspectCheck_->isChecked();
    value.scaleToBounds = scaleToBoundsCheck_->isChecked();
    value.islandScale = islandScaleSpin_->value();
    return value;
}

} // namespace sed::ui::uv
