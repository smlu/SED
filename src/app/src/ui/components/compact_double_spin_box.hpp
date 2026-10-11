#pragma once

#include <algorithm>

#include <QDoubleSpinBox>
#include <QEvent>
#include <QFontMetrics>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleOptionSpinBox>

namespace sed::ui
{

// A fixed-policy numeric editor that stays visually compact without assuming
// that every platform, DPI, or application font fits inside one fixed height.
// The control grows when its style metrics require more room, and its width
// reserves the real spin-box edit subcontrol for the widest formatted bound.
class CompactDoubleSpinBox final : public QDoubleSpinBox
{
public:
    explicit CompactDoubleSpinBox(QWidget* parent = nullptr)
        : QDoubleSpinBox(parent)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    void setPreferredCompactSize(const int height, const int minimumWidth)
    {
        preferredHeight_ = std::max(0, height);
        preferredMinimumWidth_ = std::max(0, minimumWidth);
        setFixedSize(compactSizeHint());
        updateGeometry();
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        return compactSizeHint();
    }

    [[nodiscard]] QSize minimumSizeHint() const override
    {
        return compactSizeHint();
    }

protected:
    void changeEvent(QEvent* event) override
    {
        QDoubleSpinBox::changeEvent(event);
        if (event && (event->type() == QEvent::FontChange
                || event->type() == QEvent::ApplicationFontChange
                || event->type() == QEvent::StyleChange))
        {
            setFixedSize(compactSizeHint());
            updateGeometry();
        }
    }

private:
    [[nodiscard]] QString formattedValue(const double value) const
    {
        return prefix() + textFromValue(value) + suffix();
    }

    [[nodiscard]] QSize compactSizeHint() const
    {
        const QFontMetrics metrics(font());
        const QString minimumText = formattedValue(minimum());
        const QString maximumText = formattedValue(maximum());
        const QString currentText = formattedValue(value());
        const int textWidth = std::max({
            metrics.horizontalAdvance(minimumText),
            metrics.horizontalAdvance(maximumText),
            metrics.horizontalAdvance(currentText),
            specialValueText().isEmpty() ? 0 : metrics.horizontalAdvance(specialValueText())});
        // One average character leaves room for the cursor and avoids clipping
        // the final fractional digit against the edit-field edge.
        const int requiredEditWidth = textWidth + std::max(2, metrics.averageCharWidth());

        QStyleOptionSpinBox option{};
        option.initFrom(this);
        option.frame = hasFrame();
        option.buttonSymbols = buttonSymbols();

        const QSize styled = style()->sizeFromContents(
            QStyle::CT_SpinBox,
            &option,
            QSize(requiredEditWidth, metrics.height()),
            this);
        // Do not ask the base class for its minimum size here. Some Qt styles
        // implement that path in terms of the virtual sizeHint(), which would
        // re-enter this function. The style content metric plus the measured
        // edit subcontrol below are the authoritative inputs.
        int width = std::max(preferredMinimumWidth_, styled.width());
        int height = std::max(preferredHeight_, metrics.height() + 2);

        // Stylesheets and native styles can reserve different button/padding
        // geometry. Measure the actual edit subcontrol and grow only if it
        // would crop one of the formatted values above. Deliberately do not
        // inherit the base spin box's height: that is often taller than the
        // text and is precisely what this compact control is intended to trim.
        option.rect = QRect(0, 0, width, height);
        QRect editRect = style()->subControlRect(
            QStyle::CC_SpinBox,
            &option,
            QStyle::SC_SpinBoxEditField,
            this);
        const int requiredEditHeight = metrics.height() + 2;
        if (editRect.height() < requiredEditHeight)
        {
            height += requiredEditHeight - editRect.height();
            option.rect.setHeight(height);
            editRect = style()->subControlRect(
                QStyle::CC_SpinBox,
                &option,
                QStyle::SC_SpinBoxEditField,
                this);
        }
        if (editRect.width() < requiredEditWidth)
        {
            width += requiredEditWidth - editRect.width();
        }
        return QSize(width, height);
    }

    int preferredHeight_{26};
    int preferredMinimumWidth_{};
};

} // namespace sed::ui
