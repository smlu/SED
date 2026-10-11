#pragma once

#include "ui/components/live_color_popup.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/components/ui_decimal_format.hpp"
#include "ui/components/viewport_icons.hpp"

#include <QCloseEvent>
#include <QCursor>
#include <QFormLayout>
#include <QGuiApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QScreen>
#include <QShortcut>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <utility>

namespace sed::ui
{

/**
 * Type-aware, live light editor presented as a transient viewport popup.
 *
 * There is deliberately no modal accept/cancel step: every palette movement,
 * scrub, or valid typed value updates the document immediately.  Qt::Popup
 * supplies the familiar color-picker behavior where Escape or a click outside
 * dismisses the editor without rolling the live edit back.
 *
 * IJIM RGB fields can exceed one.  They are presented as a normal color plus
 * a separate intensity, preserving the authored RGB as
 * `displayColor * intensity` without exposing three low-level channel boxes.
 */
class LightParametersDialog final : public QWidget
{
public:
    enum class Mode : std::uint8_t
    {
        baked,
        jkdf2Baked,
        ijimThing,
        classicThing,
        ijimHeadlight,
        classicHeadlight,
        ijimSector,
    };

    struct Values
    {
        double red{ 1.0 };
        double green{ 1.0 };
        double blue{ 1.0 };
        double intensity{ 1.0 };
        double range{};
        double minRange{};
        double maxRange{};
        double offsetX{};
        double offsetY{};
        double offsetZ{};

        bool operator==(const Values&) const = default;
    };

    using PreviewCallback = std::function<void(const Values&)>;
    using ClosedCallback = std::function<void()>;

    LightParametersDialog(
        QWidget* parent,
        QString title,
        const Mode mode,
        const Values& initial,
        PreviewCallback previewCallback = {},
        ClosedCallback closedCallback = {})
        : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
        , mode_(mode)
        , initial_(initial)
        , previewCallback_(std::move(previewCallback))
        , closedCallback_(std::move(closedCallback))
    {
        setObjectName(QStringLiteral("lightParametersPopup"));
        setWindowTitle(title);
        setAttribute(Qt::WA_DeleteOnClose, true);
        setStyleSheet(QStringLiteral(
            "QWidget#lightParametersPopup { background-color: #252525; border: 1px solid #51565d; border-radius: 6px; }"
            "QLabel#lightParametersTitle { color: #e6e9ed; font-weight: 600; padding: 0 1px 2px 1px; }"));

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(9, 9, 9, 9);
        root->setSpacing(7);

        auto* titleLabel = new QLabel(std::move(title), this);
        titleLabel->setObjectName(QStringLiteral("lightParametersTitle"));
        root->addWidget(titleLabel);

        if (hasColor())
        {
            const double rawRed = std::max(0.0, finiteOr(initial.red, 0.0));
            const double rawGreen = std::max(0.0, finiteOr(initial.green, 0.0));
            const double rawBlue = std::max(0.0, finiteOr(initial.blue, 0.0));
            const double scale = usesColorIntensity()
                ? std::max({ rawRed, rawGreen, rawBlue })
                : 1.0;
            if (usesColorIntensity() && scale > 1.0e-12)
            {
                displayColor_ = { rawRed / scale, rawGreen / scale, rawBlue / scale };
            }
            else
            {
                displayColor_ = {
                    std::clamp(rawRed, 0.0, 1.0),
                    std::clamp(rawGreen, 0.0, 1.0),
                    std::clamp(rawBlue, 0.0, 1.0),
                };
            }

            colorEditor_ = new LiveColorPopup(
                this,
                currentDisplayColor(),
                false,
                [this](const QColor& color)
                {
                    if (!color.isValid())
                    {
                        return;
                    }
                    colorEdited_ = true;
                    // Use QColor's native 16-bit channels so concise picker
                    // values do not gain float promotion noise when authored.
                    const QRgba64 rgba = color.rgba64();
                    constexpr double channelMax = 65535.0;
                    displayColor_ = {
                        static_cast<double>(rgba.red()) / channelMax,
                        static_cast<double>(rgba.green()) / channelMax,
                        static_cast<double>(rgba.blue()) / channelMax,
                    };
                    if (colorIntensity_ && colorIntensity_->value() <= 1.0e-12
                        && (color.red() != 0 || color.green() != 0 || color.blue() != 0))
                    {
                        colorIntensity_->setValue(1.0);
                    }
                    notifyPreview();
                },
                {},
                LiveColorPopup::Presentation::Embedded);
            root->addWidget(colorEditor_);

            if (usesColorIntensity())
            {
                colorIntensity_ = makeScrub(
                    QStringLiteral("lightParameters.colorIntensity"),
                    0.0,
                    64.0,
                    scale,
                    0.05,
                    true);
                colorIntensity_->setToolTip(QStringLiteral(
                    "Brightness multiplier for the selected color; values may exceed 1."));
            }
        }

        auto* form = new QFormLayout();
        form->setContentsMargins(0, 0, 0, 0);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(5);
        root->addLayout(form);

        if (colorIntensity_)
        {
            form->addRow(QStringLiteral("Intensity"), colorIntensity_);
        }

        if (hasScalarIntensity())
        {
            scalarIntensity_ = makeScrub(
                QStringLiteral("lightParameters.intensity"),
                0.0,
                100000.0,
                initial.intensity,
                0.05);
            form->addRow(QStringLiteral("Intensity"), scalarIntensity_);
        }

        if (hasRange())
        {
            range_ = makeScrub(
                QStringLiteral("lightParameters.range"),
                0.0,
                100000.0,
                initial.range,
                0.1);
            form->addRow(QStringLiteral("Range"), range_);
        }

        if (hasSectorRanges())
        {
            sectorRangeRatio_ = validSectorRangeRatio(initial.minRange, initial.maxRange);
            minRange_ = makeScrub(
                QStringLiteral("lightParameters.minRange"),
                0.0,
                100000.0,
                initial.minRange,
                0.1,
                false,
                [this](ScrubFloatEdit* edited, const double value)
                {
                    synchronizeSectorRanges(edited, value);
                });
            maxRange_ = makeScrub(
                QStringLiteral("lightParameters.maxRange"),
                0.0,
                100000.0,
                initial.maxRange,
                0.1,
                false,
                [this](ScrubFloatEdit* edited, const double value)
                {
                    synchronizeSectorRanges(edited, value);
                });
            minRange_->setToolTip(QStringLiteral(
                "Outer falloff range; zero lets the engine derive it."));
            maxRange_->setToolTip(QStringLiteral(
                "Full-strength inner range; zero lets the engine derive it."));

            auto* rangeRow = new QWidget(this);
            rangeRow->setObjectName(QStringLiteral("lightParameters.sectorRanges"));
            auto* rangeLayout = new QHBoxLayout(rangeRow);
            rangeLayout->setContentsMargins(0, 0, 0, 0);
            rangeLayout->setSpacing(4);

            auto* rangeEditors = new QWidget(rangeRow);
            auto* rangeEditorsLayout = new QGridLayout(rangeEditors);
            rangeEditorsLayout->setContentsMargins(0, 0, 0, 0);
            rangeEditorsLayout->setHorizontalSpacing(5);
            rangeEditorsLayout->setVerticalSpacing(3);
            auto* minLabel = new QLabel(QStringLiteral("Min"), rangeEditors);
            auto* maxLabel = new QLabel(QStringLiteral("Max"), rangeEditors);
            minLabel->setMinimumWidth(
                std::max(minLabel->sizeHint().width(), maxLabel->sizeHint().width()));
            rangeEditorsLayout->addWidget(minLabel, 0, 0);
            rangeEditorsLayout->addWidget(minRange_, 0, 1);
            rangeEditorsLayout->addWidget(maxLabel, 1, 0);
            rangeEditorsLayout->addWidget(maxRange_, 1, 1);
            rangeEditorsLayout->setColumnStretch(1, 1);
            rangeLayout->addWidget(rangeEditors, 1);

            sectorRangeLink_ = new QToolButton(rangeRow);
            sectorRangeLink_->setObjectName(QStringLiteral("lightParameters.linkRanges"));
            sectorRangeLink_->setAccessibleName(QStringLiteral("Link point-light ranges"));
            sectorRangeLink_->setCheckable(true);
            sectorRangeLink_->setChecked(true);
            sectorRangeLink_->setAutoRaise(true);
            sectorRangeLink_->setToolButtonStyle(Qt::ToolButtonIconOnly);
            sectorRangeLink_->setIconSize(QSize(18, 18));
            sectorRangeLink_->setFixedSize(24, 48);
            sectorRangeLink_->setStyleSheet(QStringLiteral(
                "QToolButton { border: 0; background: transparent; padding: 0; }"));
            updateSectorRangeLinkAppearance();
            connect(sectorRangeLink_, &QToolButton::toggled, this, [this](const bool checked)
                {
                    sectorRangesLinked_ = checked;
                    if (checked && minRange_ && maxRange_)
                    {
                        sectorRangeRatio_ = validSectorRangeRatio(
                            minRange_->value(),
                            maxRange_->value());
                    }
                    updateSectorRangeLinkAppearance();
                });
            rangeLayout->addWidget(sectorRangeLink_, 0, Qt::AlignVCenter);
            form->addRow(QStringLiteral("Ranges"), rangeRow);
        }

        if (hasOffset())
        {
            offset_[0] = makeScrub(
                QStringLiteral("lightParameters.offsetX"),
                -100000.0,
                100000.0,
                initial.offsetX,
                0.05);
            offset_[1] = makeScrub(
                QStringLiteral("lightParameters.offsetY"),
                -100000.0,
                100000.0,
                initial.offsetY,
                0.05);
            offset_[2] = makeScrub(
                QStringLiteral("lightParameters.offsetZ"),
                -100000.0,
                100000.0,
                initial.offsetZ,
                0.05);
            form->addRow(QStringLiteral("Offset X"), offset_[0]);
            form->addRow(QStringLiteral("Offset Y"), offset_[1]);
            form->addRow(QStringLiteral("Offset Z"), offset_[2]);
        }

        auto* escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
        escape->setContext(Qt::WidgetWithChildrenShortcut);
        connect(escape, &QShortcut::activated, this, &QWidget::close);

        root->activate();
        adjustSize();
        setFixedWidth(std::max(306, sizeHint().width()));
    }

    [[nodiscard]] Mode mode() const noexcept { return mode_; }

    [[nodiscard]] Values values() const
    {
        Values out = initial_;
        if (hasColor() && colorEdited_)
        {
            // Avoid quantizing authored RGB merely because the author adjusted
            // an unrelated range/offset field.
            const double scale = colorIntensity_ ? colorIntensity_->value() : 1.0;
            out.red = displayColor_[0] * scale;
            out.green = displayColor_[1] * scale;
            out.blue = displayColor_[2] * scale;
        }
        if (scalarIntensity_)
        {
            out.intensity = scalarIntensity_->value();
        }
        if (range_)
        {
            out.range = range_->value();
        }
        if (minRange_)
        {
            out.minRange = minRange_->value();
        }
        if (maxRange_)
        {
            out.maxRange = maxRange_->value();
        }
        if (offset_[0])
        {
            out.offsetX = offset_[0]->value();
            out.offsetY = offset_[1]->value();
            out.offsetZ = offset_[2]->value();
        }
        return out;
    }

    /** Places the transient editor near the invoking viewport/property control. */
    void showAt(QPoint globalPosition = QCursor::pos())
    {
        adjustSize();
        globalPosition += QPoint(8, 8);
        const QScreen* screen = QGuiApplication::screenAt(globalPosition);
        if (!screen && parentWidget())
        {
            screen = parentWidget()->screen();
        }
        if (screen)
        {
            const QRect available = screen->availableGeometry().adjusted(6, 6, -6, -6);
            if (globalPosition.x() + width() > available.right())
            {
                globalPosition.setX(available.right() - width());
            }
            if (globalPosition.y() + height() > available.bottom())
            {
                globalPosition.setY(globalPosition.y() - height() - 16);
            }
            globalPosition.setX(std::max(globalPosition.x(), available.left()));
            globalPosition.setY(std::max(globalPosition.y(), available.top()));
        }
        move(globalPosition);
        show();
        raise();
        activateWindow();
    }

protected:
    void closeEvent(QCloseEvent* event) override
    {
        if (!closeReported_)
        {
            closeReported_ = true;
            if (closedCallback_)
            {
                closedCallback_();
            }
        }
        QWidget::closeEvent(event);
    }

private:
    [[nodiscard]] bool hasColor() const noexcept
    {
        return mode_ != Mode::jkdf2Baked
            && mode_ != Mode::classicThing
            && mode_ != Mode::classicHeadlight;
    }

    [[nodiscard]] bool usesColorIntensity() const noexcept
    {
        return mode_ == Mode::ijimThing
            || mode_ == Mode::ijimHeadlight
            || mode_ == Mode::ijimSector;
    }

    [[nodiscard]] bool hasScalarIntensity() const noexcept
    {
        return mode_ == Mode::baked
            || mode_ == Mode::jkdf2Baked
            || mode_ == Mode::classicThing
            || mode_ == Mode::classicHeadlight;
    }

    [[nodiscard]] bool hasRange() const noexcept
    {
        return mode_ == Mode::baked
            || mode_ == Mode::jkdf2Baked
            || mode_ == Mode::ijimThing
            || mode_ == Mode::ijimHeadlight;
    }

    [[nodiscard]] bool hasSectorRanges() const noexcept
    {
        return mode_ == Mode::ijimSector;
    }

    [[nodiscard]] bool hasOffset() const noexcept
    {
        return mode_ == Mode::ijimHeadlight || mode_ == Mode::classicHeadlight;
    }

    static double finiteOr(const double value, const double fallback) noexcept
    {
        return std::isfinite(value) ? value : fallback;
    }

    using BeforePreviewCallback = std::function<void(ScrubFloatEdit*, double)>;

    ScrubFloatEdit* makeScrub(
        const QString& objectName,
        const double minimum,
        const double maximum,
        const double value,
        const double step,
        const bool editsColor = false,
        BeforePreviewCallback beforePreview = {})
    {
        auto* scrub = new ScrubFloatEdit(this);
        scrub->setObjectName(objectName);
        scrub->setDecimals(6);
        scrub->setRange(minimum, maximum);
        scrub->setSingleStep(step);
        scrub->setPixelsPerStep(5.0);
        scrub->setValue(std::clamp(finiteOr(value, 0.0), minimum, maximum));

        const auto changed = [this, scrub, editsColor, beforePreview](const double value)
        {
            if (beforePreview)
            {
                beforePreview(scrub, value);
            }
            colorEdited_ = colorEdited_ || editsColor;
            notifyPreview();
        };
        connect(scrub, &ScrubFloatEdit::scrubValueChanged, this, changed);
        connect(scrub, &ScrubFloatEdit::valueCommitted, this, changed);
        connect(scrub->lineEdit(), &QLineEdit::textEdited, this,
            [this, scrub, editsColor, beforePreview](const QString& text)
            {
                bool valid = false;
                const double typed = parseUiNumber(text, scrub->lineEdit()->locale(), &valid);
                if (!valid || !std::isfinite(typed))
                {
                    return;
                }
                if (beforePreview)
                {
                    beforePreview(scrub, typed);
                }
                colorEdited_ = colorEdited_ || editsColor;
                notifyPreview();
            });
        return scrub;
    }

    static double validSectorRangeRatio(const double minRange, const double maxRange) noexcept
    {
        constexpr double fallbackRatio = 4.0;
        return std::isfinite(minRange) && std::isfinite(maxRange)
                && minRange > 1.0e-12 && maxRange > 1.0e-12
            ? minRange / maxRange
            : fallbackRatio;
    }

    void synchronizeSectorRanges(ScrubFloatEdit* edited, const double value)
    {
        if (!sectorRangesLinked_ || !minRange_ || !maxRange_)
        {
            return;
        }
        const double ratio = std::isfinite(sectorRangeRatio_)
                && sectorRangeRatio_ > 1.0e-12
            ? sectorRangeRatio_
            : 4.0;
        if (edited == minRange_)
        {
            maxRange_->setValue(value / ratio);
        }
        else if (edited == maxRange_)
        {
            minRange_->setValue(value * ratio);
        }
    }

    void updateSectorRangeLinkAppearance()
    {
        if (!sectorRangeLink_)
        {
            return;
        }
        sectorRangeLink_->setIcon(makeViewportIcon(sectorRangesLinked_
            ? ViewportIcon::rangeLinked
            : ViewportIcon::rangeUnlinked));
        sectorRangeLink_->setToolTip(sectorRangesLinked_
            ? QStringLiteral("Ranges linked: editing either preserves the current min:max ratio.")
            : QStringLiteral("Ranges unlinked: edit minimum and maximum independently."));
    }

    [[nodiscard]] QColor currentDisplayColor() const
    {
        return QColor::fromRgbF(
            std::clamp(displayColor_[0], 0.0, 1.0),
            std::clamp(displayColor_[1], 0.0, 1.0),
            std::clamp(displayColor_[2], 0.0, 1.0));
    }

    void notifyPreview()
    {
        if (previewCallback_)
        {
            previewCallback_(values());
        }
    }

    Mode mode_{};
    Values initial_{};
    PreviewCallback previewCallback_{};
    ClosedCallback closedCallback_{};
    std::array<double, 3> displayColor_{ 1.0, 1.0, 1.0 };
    bool colorEdited_{};
    bool closeReported_{};
    LiveColorPopup* colorEditor_{};
    ScrubFloatEdit* colorIntensity_{};
    ScrubFloatEdit* scalarIntensity_{};
    ScrubFloatEdit* range_{};
    ScrubFloatEdit* minRange_{};
    ScrubFloatEdit* maxRange_{};
    QToolButton* sectorRangeLink_{};
    bool sectorRangesLinked_{ true };
    double sectorRangeRatio_{ 4.0 };
    std::array<ScrubFloatEdit*, 3> offset_{};
};

} // namespace sed::ui
