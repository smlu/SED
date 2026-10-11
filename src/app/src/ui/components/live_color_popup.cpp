#include "ui/components/live_color_popup.hpp"

#include "ui/components/property_editor_controls.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include <QApplication>
#include <QCloseEvent>
#include <QDoubleSpinBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QVBoxLayout>

namespace sed::ui
{
namespace
{

class SaturationValuePlane final : public QWidget
{
public:
    using ChangedCallback = std::function<void(qreal, qreal)>;

    explicit SaturationValuePlane(ChangedCallback callback, QWidget* parent)
        : QWidget(parent), callback_(std::move(callback))
    {
        setObjectName(QStringLiteral("liveColorPlane"));
        setMinimumSize(220, 148);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setCursor(Qt::CrossCursor);
    }

    void setColorState(const qreal hue, const qreal saturation, const qreal value)
    {
        const qreal clampedHue = std::clamp(hue, 0.0, 1.0);
        if (!qFuzzyCompare(hue_ + 1.0, clampedHue + 1.0))
        {
            hue_ = clampedHue;
            gradient_ = QImage{};
        }
        saturation_ = std::clamp(saturation, 0.0, 1.0);
        value_ = std::clamp(value, 0.0, 1.0);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (gradient_.size() != size())
        {
            rebuildGradient();
        }

        QPainter painter(this);
        painter.drawImage(rect(), gradient_);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QPointF marker(
            saturation_ * static_cast<qreal>(std::max(1, width() - 1)),
            (1.0 - value_) * static_cast<qreal>(std::max(1, height() - 1)));
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(0, 0, 0, 190), 3.0));
        painter.drawEllipse(marker, 5.0, 5.0);
        painter.setPen(QPen(QColor(255, 255, 255, 235), 1.4));
        painter.drawEllipse(marker, 4.0, 4.0);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        gradient_ = QImage{};
        QWidget::resizeEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            updateFromPosition(event->position());
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (event->buttons().testFlag(Qt::LeftButton))
        {
            updateFromPosition(event->position());
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

private:
    void rebuildGradient()
    {
        const int imageWidth = std::max(1, width());
        const int imageHeight = std::max(1, height());
        gradient_ = QImage(imageWidth, imageHeight, QImage::Format_RGB32);
        for (int y = 0; y < imageHeight; ++y)
        {
            const qreal value = 1.0 - (static_cast<qreal>(y) / static_cast<qreal>(std::max(1, imageHeight - 1)));
            for (int x = 0; x < imageWidth; ++x)
            {
                const qreal saturation = static_cast<qreal>(x) / static_cast<qreal>(std::max(1, imageWidth - 1));
                gradient_.setPixelColor(x, y, QColor::fromHsvF(hue_, saturation, value));
            }
        }
    }

    void updateFromPosition(const QPointF& position)
    {
        saturation_ = std::clamp(position.x() / static_cast<qreal>(std::max(1, width() - 1)), 0.0, 1.0);
        value_ = std::clamp(1.0 - position.y() / static_cast<qreal>(std::max(1, height() - 1)), 0.0, 1.0);
        update();
        if (callback_)
        {
            callback_(saturation_, value_);
        }
    }

    ChangedCallback callback_{};
    QImage gradient_{};
    qreal hue_{};
    qreal saturation_{1.0};
    qreal value_{1.0};
};

class HueStrip final : public QWidget
{
public:
    using ChangedCallback = std::function<void(qreal)>;

    explicit HueStrip(ChangedCallback callback, QWidget* parent)
        : QWidget(parent), callback_(std::move(callback))
    {
        setObjectName(QStringLiteral("liveColorHueStrip"));
        setFixedWidth(20);
        setMinimumHeight(148);
        setCursor(Qt::PointingHandCursor);
    }

    void setHue(const qreal hue)
    {
        hue_ = std::clamp(hue, 0.0, 1.0);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QLinearGradient gradient(0.0, 0.0, 0.0, static_cast<qreal>(height()));
        for (int i = 0; i <= 6; ++i)
        {
            const qreal stop = static_cast<qreal>(i) / 6.0;
            gradient.setColorAt(stop, QColor::fromHsvF(stop == 1.0 ? 0.0 : stop, 1.0, 1.0));
        }

        QPainter painter(this);
        painter.fillRect(rect(), gradient);
        painter.setPen(QPen(QColor(238, 241, 246), 1.5));
        const qreal y = hue_ * static_cast<qreal>(std::max(1, height() - 1));
        painter.drawLine(QPointF(-1.0, y), QPointF(static_cast<qreal>(width()), y));
        painter.setPen(QPen(QColor(10, 10, 10), 1.0));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            updateFromY(event->position().y());
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (event->buttons().testFlag(Qt::LeftButton))
        {
            updateFromY(event->position().y());
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

private:
    void updateFromY(const qreal y)
    {
        hue_ = std::clamp(y / static_cast<qreal>(std::max(1, height() - 1)), 0.0, 1.0);
        update();
        if (callback_)
        {
            callback_(hue_);
        }
    }

    ChangedCallback callback_{};
    qreal hue_{};
};

class SignedIntensityStrip final : public QWidget
{
public:
    using ChangedCallback = std::function<void(qreal)>;

    explicit SignedIntensityStrip(ChangedCallback callback, QWidget* parent)
        : QWidget(parent), callback_(std::move(callback))
    {
        setObjectName(QStringLiteral("liveColorSignedIntensityStrip"));
        setProperty("sed.signedNegativeRegionUsesColor", true);
        setProperty("sed.signedZeroMarkerCompact", true);
        setMinimumSize(104, 22);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
        setAccessibleName(QStringLiteral("Signed surface light intensity"));
        setToolTip(QStringLiteral(
            "Surface-light intensity (-1 to 1). Left dims and right brightens. "
            "The absolute value is the visible color brightness and Hex value."));
    }

    void setState(
        const QColor& visibleColor,
        const qreal intensity,
        const qreal hue,
        const qreal saturation)
    {
        rampColor_ = QColor::fromHsvF(
            std::clamp(hue, 0.0, 1.0),
            std::clamp(saturation, 0.0, 1.0),
            1.0);
        if (!visibleColor.isValid())
        {
            rampColor_ = QColor(Qt::white);
        }
        rampColor_.setAlpha(255);
        intensity_ = std::clamp(intensity, -1.0, 1.0);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF track = QRectF(rect()).adjusted(1.0, 2.0, -1.0, -2.0);
        const qreal zeroX = track.left() + (track.width() * 0.5);
        const QRectF negativeRegion(track.left(), track.top(), track.width() * 0.5, track.height());
        const QRectF positiveRegion(zeroX, track.top(), track.width() * 0.5, track.height());

        // QColor cannot show the stored negative sign, but it can show its
        // absolute RGB magnitude. Mirror the selected hue toward black at zero
        // and retain the warm border to distinguish the dimming half.
        QLinearGradient negativeGradient(negativeRegion.left(), 0.0, negativeRegion.right(), 0.0);
        negativeGradient.setColorAt(0.0, rampColor_);
        negativeGradient.setColorAt(1.0, QColor(0, 0, 0));
        painter.fillRect(negativeRegion, negativeGradient);
        painter.setPen(QPen(QColor(174, 92, 98), 1.4));
        painter.drawRect(negativeRegion.adjusted(0.7, 0.7, -0.7, -0.7));

        QLinearGradient positiveGradient(positiveRegion.left(), 0.0, positiveRegion.right(), 0.0);
        positiveGradient.setColorAt(0.0, QColor(0, 0, 0));
        positiveGradient.setColorAt(1.0, rampColor_);
        painter.fillRect(positiveRegion, positiveGradient);

        painter.setPen(QPen(QColor(98, 104, 113), 1.0));
        painter.drawRect(track.adjusted(0.5, 0.5, -0.5, -0.5));
        // The gradients already meet at black to identify zero. Keep only two
        // quiet edge ticks here: a full-height bright line can be mistaken for
        // the movable intensity marker when the actual value is elsewhere.
        painter.setPen(QPen(QColor(128, 134, 144), 1.0));
        constexpr qreal zeroTickLength = 3.0;
        painter.drawLine(
            QPointF(zeroX, track.top()),
            QPointF(zeroX, track.top() + zeroTickLength));
        painter.drawLine(
            QPointF(zeroX, track.bottom() - zeroTickLength),
            QPointF(zeroX, track.bottom()));

        const qreal markerX = track.left() + ((intensity_ + 1.0) * 0.5 * track.width());
        painter.setPen(QPen(QColor(10, 10, 10, 220), 3.4));
        painter.drawLine(QPointF(markerX, track.top() - 1.0), QPointF(markerX, track.bottom() + 1.0));
        painter.setPen(QPen(QColor(248, 249, 252), 1.5));
        painter.drawLine(QPointF(markerX, track.top() - 1.0), QPointF(markerX, track.bottom() + 1.0));
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            updateFromX(event->position().x());
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (event->buttons().testFlag(Qt::LeftButton))
        {
            updateFromX(event->position().x());
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        qreal delta = 0.0;
        if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Down)
        {
            delta = -0.01;
        }
        else if (event->key() == Qt::Key_Right || event->key() == Qt::Key_Up)
        {
            delta = 0.01;
        }
        if (delta != 0.0)
        {
            if (event->modifiers().testFlag(Qt::ShiftModifier))
            {
                delta *= 10.0;
            }
            intensity_ = std::clamp(intensity_ + delta, -1.0, 1.0);
            update();
            if (callback_)
            {
                callback_(intensity_);
            }
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

private:
    void updateFromX(const qreal x)
    {
        const qreal normalized = std::clamp(
            (x - 1.0) / static_cast<qreal>(std::max(1, width() - 2)),
            0.0,
            1.0);
        intensity_ = (normalized * 2.0) - 1.0;
        update();
        if (callback_)
        {
            callback_(intensity_);
        }
    }

    ChangedCallback callback_{};
    QColor rampColor_{Qt::white};
    qreal intensity_{};
};

constexpr std::array<QColor, 10> kPresetColors{
    QColor(230, 66, 60),
    QColor(237, 139, 53),
    QColor(235, 203, 72),
    QColor(84, 184, 99),
    QColor(59, 177, 182),
    QColor(70, 132, 220),
    QColor(132, 91, 211),
    QColor(210, 85, 163),
    QColor(235, 235, 235),
    QColor(35, 35, 35),
};

} // namespace

LiveColorPopup::LiveColorPopup(
    QWidget* parent,
    const QColor& initialColor,
    const bool showAlpha,
    PreviewCallback previewCallback,
    ClosedCallback closedCallback,
    const Presentation presentation)
    : QWidget(
        parent,
        presentation == Presentation::Popup
            ? Qt::WindowFlags(Qt::Popup | Qt::FramelessWindowHint)
            : Qt::WindowFlags(Qt::Widget))
    , currentColor_(initialColor.isValid() ? initialColor : QColor(Qt::white))
    , presentation_(presentation)
    , showAlpha_(showAlpha)
    , previewCallback_(std::move(previewCallback))
    , closedCallback_(std::move(closedCallback))
{
    buildUi();
}

LiveColorPopup::LiveColorPopup(
    QWidget* parent,
    const QColor& initialColor,
    const bool showAlpha,
    const qreal initialSignedIntensity,
    SignedPreviewCallback previewCallback,
    ClosedCallback closedCallback,
    const Presentation presentation)
    : QWidget(
        parent,
        presentation == Presentation::Popup
            ? Qt::WindowFlags(Qt::Popup | Qt::FramelessWindowHint)
            : Qt::WindowFlags(Qt::Widget))
    , currentColor_(initialColor.isValid() ? initialColor : QColor(Qt::white))
    , presentation_(presentation)
    , showAlpha_(showAlpha)
    , showSignedIntensity_(true)
    , signedIntensity_(std::clamp(initialSignedIntensity, -1.0, 1.0))
    , signedPolarity_(initialSignedIntensity < 0.0 ? -1.0 : 1.0)
    , signedPreviewCallback_(std::move(previewCallback))
    , closedCallback_(std::move(closedCallback))
{
    buildUi();
}

void LiveColorPopup::buildUi()
{
    setObjectName(presentation_ == Presentation::Popup
            ? QStringLiteral("liveColorPopup")
            : QStringLiteral("liveColorEditor"));
    setAttribute(Qt::WA_DeleteOnClose, presentation_ == Presentation::Popup);
    const int signedHeight = showSignedIntensity_ ? 36 : 0;
    const QSize basePopupSize(286, (showAlpha_ ? 252 : 224) + signedHeight);
    setStyleSheet(QStringLiteral(
        "QWidget#liveColorPopup, QWidget#liveColorEditor { background-color: #2b2b2b; border: 1px solid #51565d; border-radius: 5px; }"
        "QLabel { color: #cfd3d9; }"
        "QLineEdit#liveColorHex { background-color: #202020; border: 1px solid #484848; border-radius: 3px; padding: 2px 6px; }"
        "QSlider::groove:horizontal { background: #1d1d1d; height: 5px; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #d8dde5; border: 1px solid #626a74; width: 12px; margin: -4px 0; border-radius: 6px; }"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(9, 9, 9, 9);
    root->setSpacing(7);

    auto* planeRow = new QHBoxLayout();
    planeRow->setContentsMargins(0, 0, 0, 0);
    planeRow->setSpacing(7);
    colorPlane_ = new SaturationValuePlane(
        [this](const qreal saturation, const qreal value)
        {
            setSaturationValue(saturation, value);
        },
        this);
    hueStrip_ = new HueStrip(
        [this](const qreal hue)
        {
            setHue(hue);
        },
        this);
    planeRow->addWidget(colorPlane_, 1);
    planeRow->addWidget(hueStrip_);
    root->addLayout(planeRow, 1);

    auto* presets = new QHBoxLayout();
    presets->setContentsMargins(0, 0, 0, 0);
    presets->setSpacing(4);
    for (const QColor& preset : kPresetColors)
    {
        auto* button = new QPushButton(this);
        button->setFixedSize(20, 18);
        configureColorSwatchButton(button, preset);
        button->setMinimumHeight(18);
        button->setMaximumHeight(18);
        button->setToolTip(preset.name(QColor::HexRgb).toUpper());
        connect(button, &QPushButton::clicked, this, [this, preset]()
            {
                QColor next = preset;
                next.setAlpha(currentColor_.alpha());
                setCurrentColor(next, true);
            });
        presets->addWidget(button);
    }
    root->addLayout(presets);

    if (showSignedIntensity_)
    {
        auto* intensityRow = new QHBoxLayout();
        intensityRow->setContentsMargins(0, 0, 0, 0);
        intensityRow->setSpacing(6);
        auto* intensityLabel = new QLabel(QStringLiteral("Intensity"), this);
        const QString intensityHelp = QStringLiteral(
            "Signed surface-light amount (-1 to 1). Negative values dim and "
            "positive values brighten. Its magnitude changes the visible "
            "color brightness, Hex value, and vertical palette position.");
        intensityLabel->setToolTip(intensityHelp);
        intensityRow->addWidget(intensityLabel);
        signedIntensityStrip_ = new SignedIntensityStrip(
            [this](const qreal intensity)
            {
                setSignedIntensity(intensity, true);
            },
            this);
        intensityRow->addWidget(signedIntensityStrip_, 1);

        auto* intensityEdit = new QDoubleSpinBox(this);
        signedIntensityEdit_ = intensityEdit;
        intensityEdit->setObjectName(QStringLiteral("liveColorSignedIntensity"));
        intensityEdit->setRange(-1.0, 1.0);
        intensityEdit->setDecimals(3);
        intensityEdit->setSingleStep(0.01);
        // The shared theme reserves a wide, DPI-scaled strip for the two arrow
        // buttons.  A fixed 67 px control left almost no edit field at larger
        // fonts/scales, so values such as -1.000 were reduced to a clipped
        // fragment.  Size from both the current style and the widest value.
        const int signedValueWidth = intensityEdit->fontMetrics().horizontalAdvance(
            QStringLiteral("-1.000"));
        intensityEdit->setMinimumWidth(std::max(
            intensityEdit->sizeHint().width(),
            signedValueWidth + 54));
        intensityEdit->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        intensityEdit->setToolTip(intensityHelp);
        intensityRow->addWidget(intensityEdit);
        root->addLayout(intensityRow);

        connect(intensityEdit, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](const double intensity)
            {
                setSignedIntensity(intensity, true);
            });
    }

    auto* valueRow = new QHBoxLayout();
    valueRow->setContentsMargins(0, 0, 0, 0);
    valueRow->setSpacing(6);
    auto* hexLabel = new QLabel(QStringLiteral("Hex"), this);
    if (showSignedIntensity_)
    {
        const QString signedHexHelp = QStringLiteral(
            "Absolute RGB magnitude. Its brightness follows the absolute signed "
            "intensity; a negative intensity stores this color as dimming light.");
        hexLabel->setToolTip(signedHexHelp);
        hexEdit_ = new QLineEdit(this);
        hexEdit_->setToolTip(signedHexHelp);
    }
    valueRow->addWidget(hexLabel);
    if (!hexEdit_)
    {
        hexEdit_ = new QLineEdit(this);
    }
    hexEdit_->setObjectName(QStringLiteral("liveColorHex"));
    hexEdit_->setMaxLength(showAlpha_ ? 9 : 7);
    valueRow->addWidget(hexEdit_, 1);
    root->addLayout(valueRow);

    connect(hexEdit_, &QLineEdit::editingFinished, this, [this]()
        {
            const QColor parsed(hexEdit_->text().trimmed());
            if (!parsed.isValid())
            {
                syncControls();
                return;
            }
            QColor next = parsed;
            if (!showAlpha_)
            {
                next.setAlpha(255);
            }
            setCurrentColor(next, true);
        });

    if (showAlpha_)
    {
        auto* alphaRow = new QHBoxLayout();
        alphaRow->setContentsMargins(0, 0, 0, 0);
        alphaRow->setSpacing(6);
        alphaRow->addWidget(new QLabel(QStringLiteral("Opacity"), this));
        alphaSlider_ = new QSlider(Qt::Horizontal, this);
        alphaSlider_->setObjectName(QStringLiteral("liveColorAlpha"));
        alphaSlider_->setRange(0, 255);
        alphaRow->addWidget(alphaSlider_, 1);
        root->addLayout(alphaRow);
        connect(alphaSlider_, &QSlider::valueChanged, this, [this](const int alpha)
            {
                QColor next = currentColor_;
                next.setAlpha(alpha);
                setCurrentColor(next, true);
            });
    }

    qreal initialHue = currentColor_.hsvHueF();
    if (initialHue < 0.0)
    {
        initialHue = 0.0;
    }
    hue_ = initialHue;
    if (showSignedIntensity_)
    {
        signedSaturation_ = std::clamp(
            static_cast<qreal>(currentColor_.hsvSaturationF()),
            0.0,
            1.0);
        currentColor_ = QColor::fromHsvF(
            hue_,
            signedSaturation_,
            std::abs(signedIntensity_),
            currentColor_.alphaF());
    }
    if (!showAlpha_)
    {
        currentColor_.setAlpha(255);
    }
    syncControls();

    // Respect larger application fonts and platform spin-box metrics instead
    // of clipping a row to the historical fixed dimensions.  The palette
    // remains compact at the default theme because the old size is the floor.
    root->activate();
    const QSize layoutMinimum = root->minimumSize();
    setFixedSize(
        std::max(basePopupSize.width(), layoutMinimum.width()),
        std::max(basePopupSize.height(), layoutMinimum.height()));
}

QColor LiveColorPopup::currentColor() const noexcept
{
    return currentColor_;
}

qreal LiveColorPopup::signedIntensity() const noexcept
{
    return signedIntensity_;
}

bool LiveColorPopup::hasSignedIntensity() const noexcept
{
    return showSignedIntensity_;
}

void LiveColorPopup::setCurrentColor(const QColor& color, const bool notify)
{
    if (!color.isValid())
    {
        return;
    }

    QColor next = color;
    if (!showAlpha_)
    {
        next.setAlpha(255);
    }
    const qreal nextHue = next.hsvHueF();
    if (nextHue >= 0.0)
    {
        hue_ = nextHue;
    }
    if (showSignedIntensity_)
    {
        const qreal nextSaturation = next.hsvSaturationF();
        // Black has no intrinsic hue or saturation. Retain the previous
        // chroma so moving away from zero intensity stays on the same vertical
        // palette line instead of unexpectedly becoming neutral.
        if (next.valueF() > 0.0 || nextSaturation > 0.0)
        {
            signedSaturation_ = std::clamp(nextSaturation, 0.0, 1.0);
        }
        signedIntensity_ = signedPolarity_ * std::clamp(
            static_cast<qreal>(next.valueF()),
            0.0,
            1.0);
    }
    if (next == currentColor_)
    {
        return;
    }

    currentColor_ = next;
    syncControls();
    if (notify)
    {
        notifyPreview();
    }
}

void LiveColorPopup::setSignedIntensity(const qreal intensity, const bool notify)
{
    if (!showSignedIntensity_)
    {
        return;
    }
    const qreal next = std::clamp(intensity, -1.0, 1.0);
    if (qFuzzyCompare(next + 2.0, signedIntensity_ + 2.0))
    {
        return;
    }
    if (next < 0.0)
    {
        signedPolarity_ = -1.0;
    }
    else if (next > 0.0)
    {
        signedPolarity_ = 1.0;
    }
    signedIntensity_ = next;
    currentColor_ = QColor::fromHsvF(
        hue_,
        signedSaturation_,
        std::abs(signedIntensity_),
        currentColor_.alphaF());
    syncControls();
    if (notify)
    {
        notifyPreview();
    }
}

void LiveColorPopup::notifyPreview()
{
    if (showSignedIntensity_)
    {
        if (signedPreviewCallback_)
        {
            signedPreviewCallback_(currentColor_, signedIntensity_);
        }
        return;
    }
    if (previewCallback_)
    {
        previewCallback_(currentColor_);
    }
}

void LiveColorPopup::showFor(const QWidget* anchor)
{
    if (presentation_ == Presentation::Embedded)
    {
        show();
        return;
    }
    if (!anchor)
    {
        show();
        return;
    }

    QPoint position = anchor->mapToGlobal(QPoint(0, anchor->height() + 4));
    const QScreen* screen = QGuiApplication::screenAt(position);
    if (!screen)
    {
        screen = anchor->screen();
    }
    if (screen)
    {
        const QRect available = screen->availableGeometry().adjusted(6, 6, -6, -6);
        if (position.x() + width() > available.right())
        {
            position.setX(available.right() - width());
        }
        if (position.y() + height() > available.bottom())
        {
            position.setY(anchor->mapToGlobal(QPoint(0, -height() - 4)).y());
        }
        position.setX(std::max(position.x(), available.left()));
        position.setY(std::max(position.y(), available.top()));
    }

    move(position);
    show();
    raise();
}

void LiveColorPopup::closeEvent(QCloseEvent* event)
{
    if ( !closeReported_ )
    {
        closeReported_ = true;
        const ClosedCallback callback = closedCallback_;
        const QPointer<LiveColorPopup> guardedPopup(this);
        if ( callback )
        {
            // Committing a resource can synchronously delete its owner and
            // this popup. Keep the executing callback target alive locally.
            callback();
        }
        if ( !guardedPopup )
        {
            return;
        }
    }
    QWidget::closeEvent(event);
}

void LiveColorPopup::setSaturationValue(const qreal saturation, const qreal value)
{
    if (showSignedIntensity_)
    {
        signedSaturation_ = std::clamp(saturation, 0.0, 1.0);
    }
    QColor next = QColor::fromHsvF(
        hue_,
        std::clamp(saturation, 0.0, 1.0),
        std::clamp(value, 0.0, 1.0),
        currentColor_.alphaF());
    setCurrentColor(next, true);
}

void LiveColorPopup::setHue(const qreal hue)
{
    hue_ = std::clamp(hue, 0.0, 1.0);
    QColor next = QColor::fromHsvF(
        hue_,
        showSignedIntensity_ ? signedSaturation_ : currentColor_.hsvSaturationF(),
        currentColor_.valueF(),
        currentColor_.alphaF());
    setCurrentColor(next, true);
}

void LiveColorPopup::syncControls()
{
    if (auto* plane = static_cast<SaturationValuePlane*>(colorPlane_))
    {
        plane->setColorState(
            hue_,
            showSignedIntensity_ ? signedSaturation_ : currentColor_.hsvSaturationF(),
            currentColor_.valueF());
    }
    if (auto* strip = static_cast<HueStrip*>(hueStrip_))
    {
        strip->setHue(hue_);
    }
    if (hexEdit_)
    {
        const QSignalBlocker blocker(hexEdit_);
        hexEdit_->setText(currentColor_.name(showAlpha_ ? QColor::HexArgb : QColor::HexRgb).toUpper());
    }
    if (alphaSlider_)
    {
        const QSignalBlocker blocker(alphaSlider_);
        alphaSlider_->setValue(currentColor_.alpha());
    }
    if (auto* strip = static_cast<SignedIntensityStrip*>(signedIntensityStrip_))
    {
        strip->setState(currentColor_, signedIntensity_, hue_, signedSaturation_);
    }
    if (auto* intensityEdit = qobject_cast<QDoubleSpinBox*>(signedIntensityEdit_))
    {
        const QSignalBlocker blocker(intensityEdit);
        intensityEdit->setValue(signedIntensity_);
    }
}

} // namespace sed::ui
