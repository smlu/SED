#pragma once

#include "ui/viewport/cleave_shape_utils.hpp"

#include <QFrame>
#include <QString>

#include <functional>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QToolButton;

namespace sed::ui::viewport
{

class CleaveShapePopover final : public QFrame
{
public:
    using ShapeSpecChangedHandler = std::function<void(const CleaveShapeSpec&)>;
    using CameraPlaneChangedHandler = std::function<void(bool)>;

    explicit CleaveShapePopover(QWidget* parent = nullptr);

    void setShapeSpec(
        const CleaveShapeSpec& spec,
        double displayScale,
        const QString& displayUnitSuffix);
    [[nodiscard]] const CleaveShapeSpec& shapeSpec() const noexcept;
    void setShapeSpecChangedHandler(ShapeSpecChangedHandler handler);
    void setCameraPlanePlacement(bool enabled, bool available);
    void setCameraPlaneChangedHandler(CameraPlaneChangedHandler handler);

private:
    void updateSpecFromControls(bool widthChanged, bool heightChanged);
    void updateControlVisibility();

    QComboBox* shapeKindCombo_{};
    QDoubleSpinBox* widthSpin_{};
    QDoubleSpinBox* heightSpin_{};
    QToolButton* dimensionsLinkedButton_{};
    QLabel* segmentsLabel_{};
    QSpinBox* segmentsSpin_{};
    QLabel* pointsLabel_{};
    QSpinBox* pointsSpin_{};
    QCheckBox* cameraPlaneCheck_{};
    CleaveShapeSpec spec_{};
    ShapeSpecChangedHandler shapeSpecChangedHandler_{};
    CameraPlaneChangedHandler cameraPlaneChangedHandler_{};
    bool shapeControlsInitialized_{};
    double displayScale_{1.0};
    QString displayUnitSuffix_{};
};

} // namespace sed::ui::viewport
