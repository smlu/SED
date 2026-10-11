#pragma once

#include <optional>

#include <QWidget>
#include <QString>

class QLineEdit;
class QToolButton;
class QShowEvent;

namespace sed::ui
{

class ScrubFloatEdit final : public QWidget
{
    Q_OBJECT

public:
    explicit ScrubFloatEdit(QWidget* parent = nullptr);
    ~ScrubFloatEdit() override;

    double value() const noexcept;
    void setValue(double value);

    int decimals() const noexcept;
    void setDecimals(int decimals);

    double singleStep() const noexcept;
    void setSingleStep(double step);

    double pixelsPerStep() const noexcept;
    void setPixelsPerStep(double px);

    void setStepperUsesScrubSignals(bool enabled) noexcept;
    void setStepperButtonWidth(int width);
    void cancelActiveGesture();

    void clearRange();
    void setRange(double minValue, double maxValue);

    QLineEdit* lineEdit() noexcept;
    const QLineEdit* lineEdit() const noexcept;

Q_SIGNALS:
    void scrubStarted(double startValue);
    void scrubValueChanged(double value);
    void scrubFinished(double value);
    void scrubCanceled();
    void valueCommitted(double value);

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void beginScrub();
    void endScrub(bool notifyFinished);
    void stepFromButton(double direction);
    void endStepperGesture(bool notifyFinished);
    void showButtons(bool show);
    void updateTextFromValue();
    void commitTextIfValid();
    double clamp(double v) const noexcept;

    QLineEdit* edit_{};
    QToolButton* decButton_{};
    QToolButton* incButton_{};

    std::optional<double> min_{};
    std::optional<double> max_{};
    int decimals_{ 3 };
    double value_{};
    QString displayText_{};
    double step_{ 0.01 };
    double pixelsPerStep_{ 6.0 };

    bool updatingText_{ false };
    bool scrubbing_{ false };
    bool scrubPending_{ false };
    bool scrubCursorOverrideActive_{ false };
    bool stepperUsesScrubSignals_{ false };
    int stepperButtonWidth_{};
    bool stepperGestureActive_{ false };
    bool textEditedSinceCommit_{ false };
    int scrubStartX_{};
    double scrubStartValue_{};
};

} // namespace sed::ui
