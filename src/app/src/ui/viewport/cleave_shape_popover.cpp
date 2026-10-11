#include "ui/viewport/cleave_shape_popover.hpp"

#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>

#include "ui/components/viewport_icons.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace sed::ui::viewport
{

CleaveShapePopover::CleaveShapePopover(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("cleaveShapePopover"));
    setFrameShape(QFrame::StyledPanel);

    auto* layout = new QGridLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setHorizontalSpacing(6);
    layout->setVerticalSpacing(6);

    auto* shapeLabel = new QLabel(QStringLiteral("Shape"), this);
    shapeLabel->setObjectName(QStringLiteral("cleaveShapeKindLabel"));
    shapeKindCombo_ = new QComboBox(this);
    shapeKindCombo_->setObjectName(QStringLiteral("cleaveShapeKindCombo"));
    shapeKindCombo_->addItem(QStringLiteral("Rectangle"),
        static_cast<int>(CleaveShapeKind::Rectangle));
    shapeKindCombo_->addItem(QStringLiteral("N-Gon"),
        static_cast<int>(CleaveShapeKind::RegularPolygon));
    shapeKindCombo_->addItem(QStringLiteral("Filled arch"),
        static_cast<int>(CleaveShapeKind::Arch));
    shapeKindCombo_->addItem(QStringLiteral("Star"),
        static_cast<int>(CleaveShapeKind::Star));
    shapeKindCombo_->setAccessibleName(QStringLiteral("Cleave shape"));
    shapeKindCombo_->setToolTip(QStringLiteral(
        "<b>Shape</b><br>Choose a centered outline to place as bounded cut lines. "
        "A regular N-gon's width is its full diameter."));
    layout->addWidget(shapeLabel, 0, 0);
    layout->addWidget(shapeKindCombo_, 0, 1, 1, 5);

    auto* widthLabel = new QLabel(QStringLiteral("Width"), this);
    widthLabel->setObjectName(QStringLiteral("cleaveShapeWidthLabel"));
    widthSpin_ = new QDoubleSpinBox(this);
    widthSpin_->setObjectName(QStringLiteral("cleaveShapeWidthSpin"));
    widthSpin_->setDecimals(3);
    widthSpin_->setSingleStep(0.1);
    widthSpin_->setKeyboardTracking(false);
    widthSpin_->setProperty("sedSpinWheelEnabled", true);
    widthSpin_->setAccessibleName(QStringLiteral("Shape width"));
    widthSpin_->setToolTip(QStringLiteral(
        "<b>Width</b><br>Set the shape's width in the selected Cleave length units."));

    auto* heightLabel = new QLabel(QStringLiteral("Height"), this);
    heightLabel->setObjectName(QStringLiteral("cleaveShapeHeightLabel"));
    heightSpin_ = new QDoubleSpinBox(this);
    heightSpin_->setObjectName(QStringLiteral("cleaveShapeHeightSpin"));
    heightSpin_->setDecimals(3);
    heightSpin_->setSingleStep(0.1);
    heightSpin_->setKeyboardTracking(false);
    heightSpin_->setProperty("sedSpinWheelEnabled", true);
    heightSpin_->setAccessibleName(QStringLiteral("Shape height"));
    heightSpin_->setToolTip(QStringLiteral(
        "<b>Height</b><br>Set the shape's height in the selected Cleave length units."));

    dimensionsLinkedButton_ = new QToolButton(this);
    dimensionsLinkedButton_->setObjectName(
        QStringLiteral("cleaveShapeDimensionsLinkedButton"));
    dimensionsLinkedButton_->setCheckable(true);
    dimensionsLinkedButton_->setAutoRaise(true);
    dimensionsLinkedButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    dimensionsLinkedButton_->setIconSize(QSize(18, 18));
    layout->addWidget(widthLabel, 1, 0);
    layout->addWidget(widthSpin_, 1, 1);
    layout->addWidget(heightLabel, 1, 2);
    layout->addWidget(heightSpin_, 1, 3);
    layout->addWidget(dimensionsLinkedButton_, 1, 4, 1, 2);

    segmentsLabel_ = new QLabel(QStringLiteral("Segments"), this);
    segmentsLabel_->setObjectName(QStringLiteral("cleaveShapeSegmentsLabel"));
    segmentsSpin_ = new QSpinBox(this);
    segmentsSpin_->setObjectName(QStringLiteral("cleaveShapeSegmentsSpin"));
    segmentsSpin_->setRange(3, 64);
    segmentsSpin_->setProperty("sedSpinWheelEnabled", true);
    segmentsSpin_->setAccessibleName(QStringLiteral("Polygon sides or arch segments"));
    segmentsSpin_->setToolTip(QStringLiteral(
        "<b>Segments</b><br>Set the number of polygon sides or arch-cap sections."));
    pointsLabel_ = new QLabel(QStringLiteral("Points"), this);
    pointsLabel_->setObjectName(QStringLiteral("cleaveShapePointsLabel"));
    pointsSpin_ = new QSpinBox(this);
    pointsSpin_->setObjectName(QStringLiteral("cleaveShapePointsSpin"));
    pointsSpin_->setRange(3, 64);
    pointsSpin_->setProperty("sedSpinWheelEnabled", true);
    pointsSpin_->setAccessibleName(QStringLiteral("Star points"));
    pointsSpin_->setToolTip(QStringLiteral(
        "<b>Points</b><br>Set the number of outer tips on the star."));
    layout->addWidget(segmentsLabel_, 2, 0);
    layout->addWidget(segmentsSpin_, 2, 1);
    layout->addWidget(pointsLabel_, 2, 2);
    layout->addWidget(pointsSpin_, 2, 3);
    layout->setColumnStretch(5, 1);

    cameraPlaneCheck_ = new QCheckBox(QStringLiteral("Camera plane"), this);
    cameraPlaneCheck_->setObjectName(QStringLiteral("cleaveShapeCameraPlaneCheck"));
    cameraPlaneCheck_->setAccessibleName(QStringLiteral("Place shape on camera plane"));
    cameraPlaneCheck_->setToolTip(QStringLiteral(
        "<b>Camera plane</b><br>Place the sector shape freely on a plane facing the camera. "
        "Turn this off to place and orient the shape on a sector surface."));
    layout->addWidget(cameraPlaneCheck_, 3, 0, 1, 6);
    cameraPlaneCheck_->hide();
    connect(cameraPlaneCheck_, &QCheckBox::toggled, this,
        [this](const bool enabled)
        {
            if ( cameraPlaneChangedHandler_ )
            {
                cameraPlaneChangedHandler_(enabled);
            }
        });

    const auto changed = [this]()
    {
        updateSpecFromControls(false, false);
    };
    connect(shapeKindCombo_, qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this](const int)
        {
            updateSpecFromControls(false, false);
        });
    connect(widthSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
        this, [this](const double)
        {
            updateSpecFromControls(true, false);
        });
    connect(heightSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
        this, [this](const double)
        {
            updateSpecFromControls(false, true);
        });
    connect(dimensionsLinkedButton_, &QToolButton::toggled, this,
        [this](const bool)
        {
            updateSpecFromControls(false, false);
        });
    connect(segmentsSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
        [changed](const int)
        {
            changed();
        });
    connect(pointsSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
        [changed](const int)
        {
            changed();
        });

    CleaveShapeSpec initialSpec{};
    initialSpec.kind = CleaveShapeKind::Rectangle;
    initialSpec.width = 0.1;
    initialSpec.height = 0.1;
    setShapeSpec(initialSpec, 1.0, {});
}

void CleaveShapePopover::setShapeSpec(
    const CleaveShapeSpec& spec,
    double displayScale,
    const QString& displayUnitSuffix)
{
    if ( !std::isfinite(displayScale) || displayScale <= 0.0
        || !std::isfinite(10000.0 * displayScale)
        || 0.005 * displayScale < 0.001 )
    {
        displayScale = 1.0;
    }

    const bool supportedKind = spec.kind == CleaveShapeKind::Square
        || spec.kind == CleaveShapeKind::Rectangle
        || spec.kind == CleaveShapeKind::RegularPolygon
        || spec.kind == CleaveShapeKind::Arch
        || spec.kind == CleaveShapeKind::Star;
    CleaveShapeSpec nextSpec{};
    nextSpec.kind = supportedKind && spec.kind != CleaveShapeKind::Square
        ? spec.kind : CleaveShapeKind::Rectangle;
    nextSpec.width = std::isfinite(spec.width)
        ? std::clamp(spec.width, 0.005, 10000.0) : 0.1;
    nextSpec.height = std::isfinite(spec.height)
        ? std::clamp(spec.height, 0.005, 10000.0) : 0.1;
    nextSpec.dimensionsLinked = spec.dimensionsLinked;
    nextSpec.segments = std::clamp(spec.segments, 3, 64);
    nextSpec.points = std::clamp(spec.points, 3, 64);
    if ( spec.kind == CleaveShapeKind::Square
        || nextSpec.kind == CleaveShapeKind::RegularPolygon )
    {
        nextSpec.dimensionsLinked = true;
        nextSpec.height = nextSpec.width;
    }
    else if ( nextSpec.dimensionsLinked )
    {
        nextSpec.height = nextSpec.width;
    }

    if ( shapeControlsInitialized_ && spec_.kind == nextSpec.kind
        && spec_.width == nextSpec.width && spec_.height == nextSpec.height
        && spec_.dimensionsLinked == nextSpec.dimensionsLinked
        && spec_.segments == nextSpec.segments && spec_.points == nextSpec.points
        && displayScale_ == displayScale && displayUnitSuffix_ == displayUnitSuffix )
    {
        return;
    }
    spec_ = nextSpec;
    displayScale_ = displayScale;
    displayUnitSuffix_ = displayUnitSuffix;

    const int kindIndex = shapeKindCombo_->findData(
        static_cast<int>(spec_.kind));
    const QSignalBlocker kindBlocker(shapeKindCombo_);
    const QSignalBlocker widthBlocker(widthSpin_);
    const QSignalBlocker heightBlocker(heightSpin_);
    const QSignalBlocker linkedBlocker(dimensionsLinkedButton_);
    const QSignalBlocker segmentsBlocker(segmentsSpin_);
    const QSignalBlocker pointsBlocker(pointsSpin_);
    shapeKindCombo_->setCurrentIndex(std::max(0, kindIndex));
    const double minimumDisplay = 0.005 * displayScale_;
    const double maximumDisplay = 10000.0 * displayScale_;
    widthSpin_->setRange(minimumDisplay, maximumDisplay);
    heightSpin_->setRange(minimumDisplay, maximumDisplay);
    widthSpin_->setSingleStep(0.1 * displayScale_);
    heightSpin_->setSingleStep(0.1 * displayScale_);
    widthSpin_->setSuffix(displayUnitSuffix_);
    heightSpin_->setSuffix(displayUnitSuffix_);
    widthSpin_->setValue(spec_.width * displayScale_);
    heightSpin_->setValue(spec_.height * displayScale_);
    dimensionsLinkedButton_->setChecked(spec_.dimensionsLinked);
    segmentsSpin_->setValue(spec_.segments);
    pointsSpin_->setValue(spec_.points);
    updateControlVisibility();
    shapeControlsInitialized_ = true;
    adjustSize();
}

const CleaveShapeSpec& CleaveShapePopover::shapeSpec() const noexcept
{
    return spec_;
}

void CleaveShapePopover::setShapeSpecChangedHandler(
    ShapeSpecChangedHandler handler)
{
    shapeSpecChangedHandler_ = std::move(handler);
}

void CleaveShapePopover::setCameraPlanePlacement(
    const bool enabled,
    const bool available)
{
    const QSignalBlocker blocker(cameraPlaneCheck_);
    cameraPlaneCheck_->setChecked(enabled);
    cameraPlaneCheck_->setVisible(available);
}

void CleaveShapePopover::setCameraPlaneChangedHandler(
    CameraPlaneChangedHandler handler)
{
    cameraPlaneChangedHandler_ = std::move(handler);
}

void CleaveShapePopover::updateSpecFromControls(
    const bool widthChanged,
    const bool heightChanged)
{
    if ( !shapeKindCombo_ || !widthSpin_ || !heightSpin_
        || !dimensionsLinkedButton_ || !segmentsSpin_ || !pointsSpin_ )
    {
        return;
    }

    const auto requestedKind = static_cast<CleaveShapeKind>(
        shapeKindCombo_->currentData().toInt());
    const bool fixedLinkedKind = requestedKind == CleaveShapeKind::RegularPolygon;
    bool dimensionsLinked = fixedLinkedKind || dimensionsLinkedButton_->isChecked();
    if ( fixedLinkedKind && !dimensionsLinkedButton_->isChecked() )
    {
        const QSignalBlocker linkedBlocker(dimensionsLinkedButton_);
        dimensionsLinkedButton_->setChecked(true);
    }

    if ( dimensionsLinked )
    {
        if ( heightChanged && !widthChanged )
        {
            const QSignalBlocker widthBlocker(widthSpin_);
            widthSpin_->setValue(heightSpin_->value());
        }
        else
        {
            const QSignalBlocker heightBlocker(heightSpin_);
            heightSpin_->setValue(widthSpin_->value());
        }
    }

    spec_.kind = requestedKind;
    spec_.width = widthSpin_->value() / displayScale_;
    spec_.height = heightSpin_->value() / displayScale_;
    spec_.dimensionsLinked = dimensionsLinked;
    spec_.segments = segmentsSpin_->value();
    spec_.points = pointsSpin_->value();
    updateControlVisibility();
    adjustSize();
    if ( shapeSpecChangedHandler_ )
    {
        shapeSpecChangedHandler_(spec_);
    }
}

void CleaveShapePopover::updateControlVisibility()
{
    const bool isStar = spec_.kind == CleaveShapeKind::Star;
    const bool hasSegments = spec_.kind == CleaveShapeKind::RegularPolygon
        || spec_.kind == CleaveShapeKind::Arch;
    const bool fixedLinkedKind = spec_.kind == CleaveShapeKind::RegularPolygon;
    segmentsLabel_->setVisible(hasSegments);
    segmentsSpin_->setVisible(hasSegments);
    segmentsSpin_->setToolTip(spec_.kind == CleaveShapeKind::Arch
        ? QStringLiteral(
              "<b>Segments</b><br>Set how many straight sections form the arch's curved cap.")
        : QStringLiteral(
              "<b>Segments</b><br>Set the number of sides on the regular N-gon."));
    pointsLabel_->setVisible(isStar);
    pointsSpin_->setVisible(isStar);
    dimensionsLinkedButton_->setEnabled(!fixedLinkedKind);
    heightSpin_->setEnabled(!fixedLinkedKind);
    dimensionsLinkedButton_->setIcon(ui::makeViewportIcon(
        spec_.dimensionsLinked
            ? ui::ViewportIcon::rangeLinked
            : ui::ViewportIcon::rangeUnlinked));
    if ( fixedLinkedKind )
    {
        dimensionsLinkedButton_->setAccessibleName(
            QStringLiteral("Shape requires equal width and height"));
        dimensionsLinkedButton_->setToolTip(QStringLiteral(
            "<b>Linked dimensions</b><br>This shape always uses equal width and height."));
    }
    else if ( spec_.dimensionsLinked )
    {
        dimensionsLinkedButton_->setAccessibleName(
            QStringLiteral("Unlink shape width and height"));
        dimensionsLinkedButton_->setToolTip(QStringLiteral(
            "<b>Unlink dimensions</b><br>Allow width and height to use different values."));
    }
    else
    {
        dimensionsLinkedButton_->setAccessibleName(
            QStringLiteral("Link shape width and height"));
        dimensionsLinkedButton_->setToolTip(QStringLiteral(
            "<b>Link dimensions</b><br>Keep width and height equal while you change either value."));
    }
    heightSpin_->setVisible(true);
}

} // namespace sed::ui::viewport
