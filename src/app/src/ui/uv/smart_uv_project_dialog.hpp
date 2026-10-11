#pragma once

#include "ui/uv/smart_uv_project_utils.hpp"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;

namespace sed::ui::uv
{

/** Compact Blender-compatible option surface for Smart UV Project. */
class SmartUvProjectDialog final : public QDialog
{
public:
    explicit SmartUvProjectDialog(
        const SmartUvProjectOptions& initialOptions,
        QWidget* parent = nullptr);

    SmartUvProjectOptions options() const;

private:
    QDoubleSpinBox* angleLimitSpin_{};
    QComboBox* marginMethodCombo_{};
    QComboBox* rotationMethodCombo_{};
    QDoubleSpinBox* islandMarginSpin_{};
    QDoubleSpinBox* areaWeightSpin_{};
    QCheckBox* correctAspectCheck_{};
    QCheckBox* scaleToBoundsCheck_{};
    QDoubleSpinBox* islandScaleSpin_{};
};

} // namespace sed::ui::uv
