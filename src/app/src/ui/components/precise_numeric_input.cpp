#include "precise_numeric_input.hpp"
#include "ui_decimal_format.hpp"

#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QLocale>
#include <QSignalBlocker>
#include <QTimer>

#include <cmath>
#include <limits>
#include <optional>

namespace sed::ui
{
namespace
{

// Keep QInputDialog's numeric automation and keyboard/stepper behavior, while
// avoiding fixed-decimal quantization of the value a designer enters.
class PreciseNumericDialog final : public QInputDialog
{
public:
    PreciseNumericDialog(QWidget* parent, const QString& title, const QString& label,
        const double value, const double minimum, const double maximum)
        : QInputDialog(parent)
    {
        setLocale(uiNumericLocale());
        setWindowTitle(title);
        setLabelText(label);
        setInputMode(QInputDialog::DoubleInput);
        setDoubleDecimals(323);
        setDoubleRange(minimum, maximum);
        setDoubleValue(value);
        spin_ = findChild<QDoubleSpinBox*>();
        if ( spin_ )
        {
            spin_->setLocale(locale());
            spin_->setKeyboardTracking(false);
        }
        edit_ = findChild<QLineEdit*>();
        if (edit_)
        {
            auto* validator = new QDoubleValidator(minimum, maximum,323, edit_);
            validator->setLocale(locale());
            validator->setNotation(QDoubleValidator::ScientificNotation);
            edit_->setValidator(validator);
            edit_->installEventFilter(this);
            connect(edit_, &QLineEdit::textEdited, this, [this]
                { invalidFocusText_ = false; focusTextCaptured_ = false; });
        }
        connect(this, &QInputDialog::doubleValueChanged, this,
            [this](const double number)
            {
                setConciseText(focusTextCaptured_ && focusValue_ ? *focusValue_ : number);
            });
        if ( spin_ )
        {
            connect(spin_, &QDoubleSpinBox::editingFinished, this, [this]
            {
                setConciseText(focusTextCaptured_ && focusValue_ ? *focusValue_ : doubleValue());
            });
        }
        setConciseText(value);
    }

    void setVisible(const bool visible) override
    {
        QInputDialog::setVisible(visible);
        if ( visible )
        {
            setConciseText(doubleValue());
        }
    }

    void accept() override
    {
        const auto number = parsedText();
        if (invalidFocusText_ || !number)
        {
            reject();
            return;
        }
        acceptedValue_ = *number;
        QInputDialog::accept();
    }

    double acceptedValue() const noexcept { return acceptedValue_; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == edit_ && event->type() == QEvent::Paint && !edit_->isModified())
        {
            // Native same-value spin updates suppress value/text signals. Keep
            // programmatic presentation concise when it is actually drawn;
            // genuine typing, incomplete inputs and signed zero stay untouched.
            if (const auto number = parsedText(); number && !(*number == 0.0 && std::signbit(*number)))
            {
                setConciseText(*number);
            }
        }
        if (watched == edit_ && event->type() == QEvent::FocusOut)
        {
            focusValue_ = parsedText();
            invalidFocusText_ = !focusValue_;
            focusTextCaptured_ = true;
            // Retain the raw parsed value through QAbstractSpinBox's immediate
            // focus-out formatting, including finite subnormal inputs.
            QTimer::singleShot(0, this, [this] { focusTextCaptured_ = false; });
        }
        else if ( watched == edit_ && event->type() == QEvent::FocusIn )
        {
            focusTextCaptured_ = false;
        }
        return QInputDialog::eventFilter(watched, event);
    }

private:
    std::optional<double> parsedText() const
    {
        if ( !edit_ )
        {
            return std::nullopt;
        }
        bool ok = false;
        const double number = parseUiNumber(edit_->text(), edit_->locale(), &ok);
        if (!ok || !std::isfinite(number) || number < doubleMinimum() || number > doubleMaximum())
        {
            return std::nullopt;
        }
        return number;
    }

    void setConciseText(const double number)
    {
        if ( !edit_ )
        {
            return;
        }
        QLocale displayLocale = edit_->locale();
        displayLocale.setNumberOptions(displayLocale.numberOptions() | QLocale::OmitGroupSeparator);
        const QString text = displayLocale.toString(number, 'g', std::numeric_limits<double>::max_digits10);
        if ( edit_->text() == text )
        {
            return;
        }
        const QSignalBlocker blocker(edit_);
        edit_->setText(text);
    }

    QDoubleSpinBox* spin_{};
    QLineEdit* edit_{};
    std::optional<double> focusValue_{};
    bool focusTextCaptured_{};
    bool invalidFocusText_{};
    double acceptedValue_{};
};

} // namespace

double preciseNumericInput(QWidget* parent, const QString& title, const QString& label,
    const double initial, const double minimum, const double maximum, bool* ok)
{
    PreciseNumericDialog dialog(parent,title,label,initial,minimum, maximum);
    const bool accepted = dialog.exec() == QDialog::Accepted;
    if ( ok )
    {
        *ok = accepted;
    }
    return accepted ? dialog.acceptedValue() : initial;
}

} // namespace sed::ui
