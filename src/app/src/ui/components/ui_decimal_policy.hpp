#pragma once

#include "ui_decimal_format.hpp"

#include <QObject>

class QDoubleSpinBox;
class QLineEdit;

namespace sed::ui::detail
{

// The QApplication owns the policy and all signal subscriptions. Formatting
// reads the cached locale; settings and widget enumeration occur only during
// initialization or a preference change.
class UiDecimalPolicy final : public QObject
{
    Q_OBJECT

public:
    explicit UiDecimalPolicy(QApplication& application,
        UiDecimalSeparator separator, const QLocale& systemLocale);

    const QLocale& numericLocale() const noexcept { return locale_; }
    const QLocale& systemLocale() const noexcept { return systemLocale_; }
    UiDecimalSeparator separator() const noexcept { return separator_; }
    std::uint64_t revision() const noexcept { return revision_; }
    void apply(UiDecimalSeparator separator, const QLocale& systemLocale);

Q_SIGNALS:
    void numericFormatChanged();

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    void updateSpin(QDoubleSpinBox* spin);
    void updateValidatedEdit(QLineEdit* edit);
    void updateExistingWidgets();

    UiDecimalSeparator separator_{ UiDecimalSeparator::dot };
    QLocale locale_{ QLocale::c() };
    QLocale systemLocale_;
    std::uint64_t revision_{ 1 };
    bool applyingWidgetLocale_{};
};

} // namespace sed::ui::detail
