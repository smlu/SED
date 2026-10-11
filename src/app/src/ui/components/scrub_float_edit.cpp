#include "ui/components/scrub_float_edit.hpp"
#include "ui/components/ui_decimal_format.hpp"

#include <QApplication>
#include <QCursor>
#include <QEnterEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPointer>
#include <QShowEvent>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace sed::ui
{
namespace
{

QString displayNumber(const double value, const int decimals)
{
    const QString fixed = formatUiNumber(value, 'f', std::max(0, decimals));
    // A small nonzero value must not look like zero (or a disabled option).
    // This is presentation only; the exact double remains the editing base.
    return value != 0.0 && parseUiNumber(fixed) == 0.0
        ? formatUiNumber(value, 'g', 6) : fixed;
}

std::optional<double> tryParseDouble(const QString& text, const QLocale& inputLocale)
{
    bool ok = false;
    const double v = parseUiNumber(text, inputLocale, &ok);
    return ok ? std::optional<double>(v) : std::nullopt;
}

} // namespace

ScrubFloatEdit::ScrubFloatEdit(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    decButton_ = new QToolButton(this);
    decButton_->setText("<");
    decButton_->setAutoRaise(true);
    decButton_->setProperty("sedButtonRole", "stepper");
    decButton_->setAutoRepeat(true);
    decButton_->setAutoRepeatDelay(350);
    decButton_->setAutoRepeatInterval(80);
    decButton_->setCursor(Qt::ArrowCursor);
    decButton_->setFocusPolicy(Qt::NoFocus);
    decButton_->setToolTip("Decrease");
    decButton_->setVisible(false);
    layout->addWidget(decButton_);

    edit_ = new QLineEdit(this);
    edit_->setLocale(uiNumericLocale());
    edit_->setCursor(Qt::SizeHorCursor);
    edit_->setAlignment(Qt::AlignCenter);
    edit_->installEventFilter(this);
    layout->addWidget(edit_, 1);

    incButton_ = new QToolButton(this);
    incButton_->setText(">");
    incButton_->setAutoRaise(true);
    incButton_->setProperty("sedButtonRole", "stepper");
    incButton_->setAutoRepeat(true);
    incButton_->setAutoRepeatDelay(350);
    incButton_->setAutoRepeatInterval(80);
    incButton_->setCursor(Qt::ArrowCursor);
    incButton_->setFocusPolicy(Qt::NoFocus);
    incButton_->setToolTip("Increase");
    incButton_->setVisible(false);
    layout->addWidget(incButton_);

    // Keep the buttons symmetric so showing/hiding them doesn't shift the centered value text.
    const int btnW = std::max(decButton_->sizeHint().width(), incButton_->sizeHint().width());
    if (btnW > 0)
    {
        decButton_->setFixedWidth(btnW);
        incButton_->setFixedWidth(btnW);
    }

    connect(decButton_, &QToolButton::pressed, this, [this]()
        {
            if (stepperUsesScrubSignals_)
            {
                stepFromButton(-1.0);
            }
        });
    connect(incButton_, &QToolButton::pressed, this, [this]()
        {
            if (stepperUsesScrubSignals_)
            {
                stepFromButton(1.0);
            }
        });
    const auto finishStepperGesture = [this]()
    {
        if (stepperUsesScrubSignals_
            && !QApplication::mouseButtons().testFlag(Qt::LeftButton))
        {
            endStepperGesture(true);
        }
    };
    connect(decButton_, &QToolButton::released, this, finishStepperGesture);
    connect(incButton_, &QToolButton::released, this, finishStepperGesture);

    connect(decButton_, &QToolButton::clicked, this, [this]()
        {
            if (!stepperUsesScrubSignals_)
            {
                setValue(value() - step_);
                Q_EMIT valueCommitted(value());
            }
        });
    connect(incButton_, &QToolButton::clicked, this, [this]()
        {
            if (!stepperUsesScrubSignals_)
            {
                setValue(value() + step_);
                Q_EMIT valueCommitted(value());
            }
        });

    connect(edit_, &QLineEdit::textEdited, this, [this]()
        {
            textEditedSinceCommit_ = true;
        });

    connect(edit_, &QLineEdit::editingFinished, this, [this]()
        {
            if (scrubbing_ || stepperGestureActive_)
            {
                return;
            }
            // Focus changes after scrubbing/stepping are not text edits. In
            // particular, committing those displayed active-Thing values as
            // absolute multi-selection values would collapse every selected
            // Thing onto the previous axis.
            if (stepperUsesScrubSignals_ && !textEditedSinceCommit_)
            {
                return;
            }
            commitTextIfValid();
        });

    onUiNumericFormatChanged(this, [this]
    {
        // A raw edit keeps both its spelling and captured parsing locale.
        // Locale changes update settled presentation without a commit signal.
        if ( !textEditedSinceCommit_ && edit_->text() == displayText_ )
        {
            setValue(value_);
        }
    });
    updateTextFromValue();
}

ScrubFloatEdit::~ScrubFloatEdit()
{
    // Property panels can rebuild while a live scrub is in progress.  Do not
    // leave the application-wide resize cursor behind when that destroys the
    // editor before it receives the matching mouse release.
    endScrub(false);
    endStepperGesture(false);
}

double ScrubFloatEdit::value() const noexcept
{
    // Display decimals are presentation only. An explicit edit may author the
    // displayed rounded value, but focus/stepping must retain the exact base.
    if (!textEditedSinceCommit_
        && edit_->text() == displayText_)
        return value_;
    const auto parsed = tryParseDouble(edit_->text(), edit_->locale());
    if (parsed && std::isfinite(*parsed))
    {
        return *parsed;
    }
    return clamp(value_);
}

void ScrubFloatEdit::setValue(const double value)
{
    const double clamped = clamp(value);
    if (std::isnan(clamped) || !std::isfinite(clamped))
    {
        return;
    }

    value_ = clamped;
    updatingText_ = true;
    edit_->setLocale(uiNumericLocale());
    displayText_ = displayNumber(clamped, decimals_);
    edit_->setText(displayText_);
    updatingText_ = false;
    textEditedSinceCommit_ = false;
}

void ScrubFloatEdit::cancelActiveGesture()
{
    const QPointer<ScrubFloatEdit> self(this);
    // A held arrow must stop auto-repeat before cancellation restores its value.
    decButton_->setDown(false);
    incButton_->setDown(false);
    endScrub(false);
    // A cancellation callback can rebuild or delete its owning pane.
    if (self)
    {
        self->endStepperGesture(false);
    }
}

int ScrubFloatEdit::decimals() const noexcept
{
    return decimals_;
}

void ScrubFloatEdit::setDecimals(const int decimals)
{
    const double exact = value();
    decimals_ = std::max(0, decimals);
    setValue(exact);
}

double ScrubFloatEdit::singleStep() const noexcept
{
    return step_;
}

void ScrubFloatEdit::setSingleStep(const double step)
{
    step_ = std::isfinite(step) ? step : step_;
}

double ScrubFloatEdit::pixelsPerStep() const noexcept
{
    return pixelsPerStep_;
}

void ScrubFloatEdit::setPixelsPerStep(const double px)
{
    if (std::isfinite(px) && px > 0.0)
    {
        pixelsPerStep_ = px;
    }
}

void ScrubFloatEdit::setStepperUsesScrubSignals(const bool enabled) noexcept
{
    stepperUsesScrubSignals_ = enabled;
}

void ScrubFloatEdit::setStepperButtonWidth(const int width)
{
    stepperButtonWidth_ = std::max(0, width);
    if (stepperButtonWidth_ == 0)
    {
        return;
    }

    const QString compactStyle = QStringLiteral(
        "QToolButton[sedButtonRole=\"stepper\"] { min-width:0px; padding:0px; }");
    decButton_->setStyleSheet(compactStyle);
    incButton_->setStyleSheet(compactStyle);
    decButton_->setFixedWidth(stepperButtonWidth_);
    incButton_->setFixedWidth(stepperButtonWidth_);
}

void ScrubFloatEdit::clearRange()
{
    min_.reset();
    max_.reset();
}

void ScrubFloatEdit::setRange(const double minValue, const double maxValue)
{
    if (!std::isfinite(minValue) || !std::isfinite(maxValue))
    {
        clearRange();
        return;
    }

    min_ = std::min(minValue, maxValue);
    max_ = std::max(minValue, maxValue);
    updateTextFromValue();
}

QLineEdit* ScrubFloatEdit::lineEdit() noexcept
{
    return edit_;
}

const QLineEdit* ScrubFloatEdit::lineEdit() const noexcept
{
    return edit_;
}

bool ScrubFloatEdit::event(QEvent* event)
{
    if (event)
    {
        switch (event->type())
        {
            case QEvent::WindowDeactivate:
            case QEvent::ApplicationDeactivate:
                endScrub(true);
                endStepperGesture(true);
                break;
            case QEvent::Hide:
            case QEvent::Close:
                // A property-panel rebuild already supplies the follow-up UI
                // refresh; only release transient input/cursor state here.
                endScrub(false);
                endStepperGesture(false);
                break;
            case QEvent::EnabledChange:
                if (!isEnabled())
                {
                    // Disabling a property is a cancellation boundary, not a
                    // commit. In particular, changing selection/layer state
                    // must not let the old drag's release mutate a now
                    // read-only entity.
                    endScrub(false);
                    endStepperGesture(false);
                    showButtons(false);
                    if (edit_)
                    {
                        edit_->clearFocus();
                        edit_->setFocusPolicy(Qt::NoFocus);
                        edit_->setCursor(Qt::ArrowCursor);
                    }
                }
                else if (edit_)
                {
                    edit_->setFocusPolicy(Qt::StrongFocus);
                    edit_->setCursor(Qt::SizeHorCursor);
                }
                break;
            default:
                break;
        }
    }

    return QWidget::event(event);
}

bool ScrubFloatEdit::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == edit_)
    {
        if (!isEnabled() || !edit_->isEnabled())
        {
            endScrub(false);
            showButtons(false);
            // QWidget normally filters input before a disabled child sees it,
            // but this object is itself an installed child filter. Consume
            // synthetic, queued, or stale mouse/key events here as a final
            // guarantee that the displayed value cannot move underneath a
            // disabled Item Edit row.
            switch (event->type())
            {
                case QEvent::MouseButtonPress:
                case QEvent::MouseButtonRelease:
                case QEvent::MouseButtonDblClick:
                case QEvent::MouseMove:
                case QEvent::Wheel:
                case QEvent::KeyPress:
                case QEvent::KeyRelease:
                    return true;
                default:
                    break;
            }
        }

        if (event->type() == QEvent::MouseButtonPress)
        {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton)
            {
                endScrub(true);
                endStepperGesture(true);
                if (textEditedSinceCommit_)
                {
                    commitTextIfValid();
                }
                scrubPending_ = true;
                scrubStartX_ = me->globalPosition().toPoint().x();
                scrubStartValue_ = value();
                // Let the QLineEdit process the press so the user can focus/type.
                return false;
            }
        }
        else if (event->type() == QEvent::MouseMove)
        {
            auto* me = static_cast<QMouseEvent*>(event);
            if ((scrubbing_ || scrubPending_)
                && !me->buttons().testFlag(Qt::LeftButton))
            {
                const bool wasScrubbing = scrubbing_;
                endScrub(true);
                // A lost release followed by a buttonless move is the final
                // event for an active scrub.  Consume it so QLineEdit cannot
                // turn the stale drag into a text selection.
                return wasScrubbing;
            }

            if (!scrubbing_ && scrubPending_ && me->buttons().testFlag(Qt::LeftButton))
            {
                const int x = me->globalPosition().toPoint().x();
                const int dx = x - scrubStartX_;

                constexpr int kStartThresholdPx = 2;
                if (std::abs(dx) > kStartThresholdPx)
                {
                    beginScrub();
                }
            }

            if (scrubbing_)
            {
                const int x = me->globalPosition().toPoint().x();
                const int dx = x - scrubStartX_;

                double mult = 1.0;
                const Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
                if (mods.testFlag(Qt::ShiftModifier))
                {
                    mult = 0.1;
                }
                else if (mods.testFlag(Qt::ControlModifier))
                {
                    mult = 10.0;
                }

                const double denom = std::max(0.001, pixelsPerStep_);
                const double steps = static_cast<double>(dx) / denom;
                const double v = scrubStartValue_ + steps * step_ * mult;
                setValue(v);
                Q_EMIT scrubValueChanged(value());
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            auto* me = static_cast<QMouseEvent*>(event);
            if (scrubbing_ && me->button() == Qt::LeftButton)
            {
                endScrub(true);
                return true;
            }

            if (scrubPending_ && me->button() == Qt::LeftButton)
            {
                scrubPending_ = false;
                // Let the QLineEdit handle the release (cursor placement/selection).
                return false;
            }
        }
        else if (event->type() == QEvent::KeyPress)
        {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Escape && (scrubbing_ || scrubPending_))
            {
                const bool wasScrubbing = scrubbing_;
                endScrub(false);
                return wasScrubbing;
            }
        }
        else if (event->type() == QEvent::FocusOut
                 || event->type() == QEvent::WindowDeactivate
                 || event->type() == QEvent::ApplicationDeactivate
                 || event->type() == QEvent::UngrabMouse)
        {
            endScrub(true);
            endStepperGesture(true);
        }
        else if (event->type() == QEvent::Hide || event->type() == QEvent::Close)
        {
            endScrub(false);
            endStepperGesture(false);
        }
        else if (event->type() == QEvent::Leave)
        {
            // Don't hide buttons when leaving the line edit; the cursor might be moving to the < / > buttons.
            // The parent widget's leaveEvent handles hiding when leaving the whole control.
        }
        else if (event->type() == QEvent::Enter)
        {
            showButtons(true);
        }
    }

    return QWidget::eventFilter(obj, event);
}

void ScrubFloatEdit::beginScrub()
{
    if (scrubbing_)
    {
        return;
    }

    scrubbing_ = true;
    scrubPending_ = false;
    edit_->deselect();
    if (!scrubCursorOverrideActive_)
    {
        QApplication::setOverrideCursor(Qt::SizeHorCursor);
        scrubCursorOverrideActive_ = true;
    }
    Q_EMIT scrubStarted(scrubStartValue_);
}

void ScrubFloatEdit::endScrub(const bool notifyFinished)
{
    const bool wasScrubbing = scrubbing_;
    scrubbing_ = false;
    scrubPending_ = false;

    if (scrubCursorOverrideActive_)
    {
        if (QApplication::overrideCursor())
        {
            QApplication::restoreOverrideCursor();
        }
        scrubCursorOverrideActive_ = false;
    }

    if (wasScrubbing)
    {
        if (notifyFinished)
        {
            Q_EMIT scrubFinished(value());
        }
        else
        {
            setValue(scrubStartValue_);
            Q_EMIT scrubCanceled();
        }
    }
}

void ScrubFloatEdit::stepFromButton(const double direction)
{
    if (!isEnabled() || !edit_ || !std::isfinite(direction) || direction == 0.0)
    {
        return;
    }

    if (!stepperGestureActive_)
    {
        endScrub(true);
        if (textEditedSinceCommit_)
        {
            commitTextIfValid();
        }
        stepperGestureActive_ = true;
        scrubStartValue_ = value();
        Q_EMIT scrubStarted(scrubStartValue_);
    }

    setValue(value() + direction * step_);
    Q_EMIT scrubValueChanged(value());
}

void ScrubFloatEdit::endStepperGesture(const bool notifyFinished)
{
    const bool wasActive = stepperGestureActive_;
    stepperGestureActive_ = false;
    if (wasActive)
    {
        if (notifyFinished)
        {
            Q_EMIT scrubFinished(value());
        }
        else
        {
            setValue(scrubStartValue_);
            Q_EMIT scrubCanceled();
        }
    }
}

void ScrubFloatEdit::enterEvent(QEnterEvent* event)
{
    showButtons(isEnabled());
    QWidget::enterEvent(event);
}

void ScrubFloatEdit::leaveEvent(QEvent* event)
{
    if (!scrubbing_ && !scrubPending_ && !edit_->hasFocus())
    {
        showButtons(false);
    }
    QWidget::leaveEvent(event);
}

void ScrubFloatEdit::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    // If the widget is recreated under the cursor (e.g. property panel rebuild), Qt won't send Enter,
    // so the arrow buttons would remain hidden and the next click would go to the line edit.
    //
    // Defer the hit-test until after layout has assigned a final geometry; otherwise `rect()` may
    // still be empty/incorrect here.
    QTimer::singleShot(0, this, [this]()
        {
            const QPoint p = mapFromGlobal(QCursor::pos());
            if (isEnabled() && rect().contains(p))
            {
                showButtons(true);
            }
        });
}

void ScrubFloatEdit::showButtons(const bool show)
{
    const bool visible = show && isEnabled();
    if (decButton_)
    {
        decButton_->setVisible(visible);
    }
    if (incButton_)
    {
        incButton_->setVisible(visible);
    }
}

void ScrubFloatEdit::updateTextFromValue()
{
    setValue(value());
}

void ScrubFloatEdit::commitTextIfValid()
{
    if (updatingText_)
    {
        return;
    }

    const auto parsed = tryParseDouble(edit_->text(), edit_->locale());
    if (!parsed || !std::isfinite(*parsed))
    {
        updateTextFromValue();
        return;
    }

    const double clamped = clamp(textEditedSinceCommit_ ? *parsed : value());
    setValue(clamped);
    Q_EMIT valueCommitted(clamped);
}

double ScrubFloatEdit::clamp(const double v) const noexcept
{
    double out = v;
    if (min_)
    {
        out = std::max(out, *min_);
    }
    if (max_)
    {
        out = std::min(out, *max_);
    }
    return out;
}

} // namespace sed::ui
