#pragma once

#include "thing/thing_light_authoring_utils.hpp"
#include "ui/components/live_color_popup.hpp"
#include "ui/components/ui_decimal_format.hpp"
#include "ui/components/property_editor_controls.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include "ui/components/scrub_float_edit.hpp"
#include <QLineEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <utility>

namespace sed::ui
{

/** Small structured editor for the three runtime-light Thing parameters.
 *
 * The raw text field remains available for format compatibility; this dialog
 * gives authors named numeric controls and an RGB picker without normalizing
 * values above one, which the IJIM renderer accepts.
 */
class ThingLightValueDialog final : public QDialog
{
public:
    using PreviewCallback = std::function<void(const QString&)>;

    enum class Mode : std::uint8_t
    {
        ijimLight,
        ijimColor,
        scalar,
        vector,
    };

    ThingLightValueDialog(
        QWidget* parent,
        const QString& title,
        const Mode mode,
        const QString& authored,
        PreviewCallback previewCallback = {})
        : QDialog(parent)
        , mode_(mode)
        , previewCallback_(std::move(previewCallback))
    {
        setObjectName(QStringLiteral("thingLightValueDialog"));
        setWindowTitle(title);
        setModal(true);
        setSizeGripEnabled(false);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(10, 10, 10, 10);
        root->setSpacing(8);

        auto* form = new QFormLayout();
        form->setContentsMargins(0, 0, 0, 0);
        form->setHorizontalSpacing(10);
        form->setVerticalSpacing(6);
        root->addLayout(form);

        authoredValues_ = parseComponents(authored);
        if (mode_ != Mode::vector)
        {
            // Negative intensities are invalid; upper control limits are only
            // editing affordances and must not truncate existing HDR values.
            for (double& value : authoredValues_) value = std::max(value, 0.0);
        }
        const std::array<double, 4>& parsed = authoredValues_;
        if (mode_ == Mode::ijimLight || mode_ == Mode::ijimColor || mode_ == Mode::vector)
        {
            static constexpr std::array<const char*, 3> kLabels{ "Red", "Green", "Blue" };
            static constexpr std::array<const char*, 3> kVectorLabels{ "X", "Y", "Z" };
            const bool vectorMode = mode_ == Mode::vector;
            for (int component = 0; component < 3; ++component)
            {
                components_[static_cast<std::size_t>(component)] = makeSpin(
                    vectorMode ? -100000.0 : 0.0,
                    vectorMode ? 100000.0 : 64.0,
                    parsed[static_cast<std::size_t>(component)],
                    vectorMode ? 0.1 : 0.05);
                components_[static_cast<std::size_t>(component)]->setObjectName(
                    QStringLiteral("thingLightValue.component.%1").arg(component));
                form->addRow(
                    vectorMode
                        ? QString::fromLatin1(kVectorLabels[static_cast<std::size_t>(component)])
                        : QString::fromLatin1(kLabels[static_cast<std::size_t>(component)]),
                    components_[static_cast<std::size_t>(component)]);
            }

            if (!vectorMode)
            {
                colorButton_ = new QPushButton(QStringLiteral("Pick color..."), this);
                colorButton_->setObjectName(QStringLiteral("thingLightValue.color"));
                refreshColorButton();
                form->addRow(QStringLiteral("Color"), colorButton_);
                connect(colorButton_, &QPushButton::clicked, this, [this]()
                    {
                        const QPointer<QPushButton> anchor(colorButton_);
                        auto* popup = new LiveColorPopup(
                            this,
                            currentDisplayColor(),
                            false,
                            [this](const QColor& color)
                            {
                                setColorComponents(color);
                            });
                        popup->showFor(anchor);
                    });
                for (std::size_t index = 0; index < components_.size(); ++index)
                {
                    connectNumeric(components_[index], index,
                        mode_ == Mode::vector ? -100000.0 : 0.0,
                        mode_ == Mode::vector ? 100000.0 : 64.0,
                        [this](double)
                        {
                            refreshColorButton();
                            notifyPreview();
                        });
                }
            }
            else
            {
                for (std::size_t index = 0; index < components_.size(); ++index)
                {
                    connectNumeric(components_[index], index,
                        mode_ == Mode::vector ? -100000.0 : 0.0,
                        mode_ == Mode::vector ? 100000.0 : 64.0,
                        [this](double) { notifyPreview(); });
                }
            }

            if (mode_ == Mode::ijimLight)
            {
                range_ = makeSpin(0.0, 100000.0, parsed[3], 0.1);
                range_->setObjectName(QStringLiteral("thingLightValue.range"));
                form->addRow(QStringLiteral("Range"), range_);
                connectNumeric(range_, 3, 0.0, 100000.0,
                    [this](double) { notifyPreview(); });
            }
        }
        else
        {
            scalar_ = makeSpin(0.0, 100000.0, parsed[0], 0.05);
            scalar_->setObjectName(QStringLiteral("thingLightValue.intensity"));
            form->addRow(QStringLiteral("Intensity"), scalar_);
            connectNumeric(scalar_, 0, 0.0, 100000.0,
                [this](double) { notifyPreview(); });
        }

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            Qt::Horizontal,
            this);
        buttons->setObjectName(QStringLiteral("thingLightValue.buttons"));
        root->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        adjustSize();
        setMinimumWidth(std::max(260, sizeHint().width()));
    }

    [[nodiscard]] QString valueText() const
    {
        if (mode_ == Mode::scalar)
        {
            return QString::fromStdString(
                thing::light_authoring::formatScalar(authoredValues_[0]));
        }

        const double x = authoredValues_[0];
        const double y = authoredValues_[1];
        const double z = authoredValues_[2];
        if (mode_ == Mode::ijimLight)
        {
            return QString::fromStdString(thing::light_authoring::formatIjimLight(
                x, y, z, authoredValues_[3]));
        }
        return QString::fromStdString(thing::light_authoring::formatVector(x, y, z));
    }

private:
    void connectNumeric(ScrubFloatEdit* edit, const std::size_t index,
        const double minimum, const double maximum, std::function<void(double)> changed)
    {
        const auto apply = [this, index, minimum, maximum, changed](const double value)
        {
            authoredValues_[index] = std::clamp(value, minimum, maximum);
            changed(authoredValues_[index]);
        };
        connect(edit, &ScrubFloatEdit::valueCommitted, this, apply);
        connect(edit, &ScrubFloatEdit::scrubValueChanged, this, apply);
        // Typed previews update only the edited component, without reformatting
        // text or sampling other controls' clipped presentation values.
        connect(edit->lineEdit(), &QLineEdit::textEdited, this,
            [apply, edit](const QString& text)
            {
                bool ok = false;
                const double parsed = parseUiNumber(text, edit->lineEdit()->locale(), &ok);
                if (ok && std::isfinite(parsed)) apply(parsed);
            });
        // ScrubFloatEdit first finalizes or rejects the text. Retain the last
        // valid live preview when an invalid suffix is rejected, so displayed
        // text and the accepted authored component cannot diverge.
        connect(edit->lineEdit(), &QLineEdit::editingFinished, this,
            [this, edit, index]() { edit->setValue(authoredValues_[index]); });
    }

    static std::array<double, 4> parseComponents(QString authored)
    {
        authored.replace('/', ' ');
        authored.replace(',', ' ');
        authored.replace(':', ' ');
        authored.remove('(');
        authored.remove(')');
        const QStringList parts = authored.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        std::array<double, 4> out{};
        const qsizetype count = std::min<qsizetype>(4, parts.size());
        for (qsizetype i = 0; i < count; ++i)
        {
            bool ok = false;
            const double value = parts[i].toDouble(&ok);
            out[static_cast<std::size_t>(i)] = ok && std::isfinite(value) ? value : 0.0;
        }
        return out;
    }

    ScrubFloatEdit* makeSpin(
        const double minimum,
        const double maximum,
        const double value,
        const double step)
    {
        auto* spin = new ScrubFloatEdit(this);
        spin->setDecimals(5);
        spin->setRange(minimum, maximum);
        spin->setSingleStep(step);
        spin->setStepperUsesScrubSignals(true);
        spin->setValue(std::clamp(value, minimum, maximum));
        return spin;
    }

    void setColorComponents(const QColor& color)
    {
        if (!color.isValid() || !components_[0] || !components_[1] || !components_[2])
        {
            return;
        }

        const QSignalBlocker redBlocker(components_[0]);
        const QSignalBlocker greenBlocker(components_[1]);
        const QSignalBlocker blueBlocker(components_[2]);
        components_[0]->setValue(color.redF());
        components_[1]->setValue(color.greenF());
        components_[2]->setValue(color.blueF());
        authoredValues_[0] = color.redF();
        authoredValues_[1] = color.greenF();
        authoredValues_[2] = color.blueF();
        refreshColorButton();
        notifyPreview();
    }

    void notifyPreview()
    {
        if (previewCallback_)
        {
            previewCallback_(valueText());
        }
    }

    [[nodiscard]] QColor currentDisplayColor() const
    {
        return QColor::fromRgbF(
            std::clamp(components_[0] ? components_[0]->value() : 0.0, 0.0, 1.0),
            std::clamp(components_[1] ? components_[1]->value() : 0.0, 0.0, 1.0),
            std::clamp(components_[2] ? components_[2]->value() : 0.0, 0.0, 1.0));
    }

    void refreshColorButton()
    {
        if (colorButton_)
        {
            configureColorSwatchButton(colorButton_, currentDisplayColor());
            colorButton_->setText(QStringLiteral("Pick color..."));
        }
    }

    Mode mode_{};
    std::array<double, 4> authoredValues_{};
    std::array<ScrubFloatEdit*, 3> components_{};
    ScrubFloatEdit* range_{};
    ScrubFloatEdit* scalar_{};
    QPushButton* colorButton_{};
    PreviewCallback previewCallback_{};
};

} // namespace sed::ui
