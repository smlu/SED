#include "test_common.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include <QLineEdit>
#include <QToolButton>
#include <limits>

namespace sed::app_tests
{
void runScrubFloatPrecisionTests()
{
    for (const double value : {1.234567891234e-9, -1.234567891234e-9})
    {
        ui::ScrubFloatEdit edit;
        edit.setDecimals(1);
        edit.setValue(value);
        check(edit.lineEdit()->text().toDouble() != 0.0 && edit.value() == value,
            "ScrubFloatEdit displays small nonzero values without rounding its authored base");
        QMetaObject::invokeMethod(edit.lineEdit(), "editingFinished", Qt::DirectConnection);
        check(edit.value() == value,
            "ScrubFloatEdit scientific fallback preserves exact hidden digits on focus completion");
    }
    for (const bool gesture : {false, true})
    {
        ui::ScrubFloatEdit edit;
        edit.setDecimals(3);
        edit.setSingleStep(1e-6);
        edit.setStepperUsesScrubSignals(gesture);
        const double authored = 1.23456789012345;
        edit.setValue(authored);
        check(edit.value() == authored,
            "ScrubFloatEdit retains exact authored value independently of displayed decimals");
        edit.setDecimals(2);
        check(edit.value() == authored, "ScrubFloatEdit decimal change does not round authored value");
        double started = -1.0, committed = -1.0;
        QObject::connect(&edit, &ui::ScrubFloatEdit::scrubStarted,
            &edit, [&](double value) { started = value; });
        QObject::connect(&edit, &ui::ScrubFloatEdit::valueCommitted,
            &edit, [&](double value) { committed = value; });
        QObject::connect(&edit, &ui::ScrubFloatEdit::scrubFinished,
            &edit, [&](double value) { committed = value; });
        QToolButton* increase = nullptr;
        for (auto* button : edit.findChildren<QToolButton*>())
            if (button->text() == ">") increase = button;
        check(increase != nullptr, "ScrubFloatEdit precision fixture exposes stepper");
        if (increase) increase->click();
        check(edit.value() == authored + 1e-6 && committed == authored + 1e-6,
            "ScrubFloatEdit step starts from exact base and commits sub-display increment");
        if (gesture) check(started == authored, "ScrubFloatEdit gesture signals exact starting value");
        // Focus-only completion must preserve the exact value too.
        QMetaObject::invokeMethod(edit.lineEdit(), "editingFinished", Qt::DirectConnection);
        check(edit.value() == authored + 1e-6,
            "ScrubFloatEdit focus-only completion preserves hidden precision");
        edit.lineEdit()->setText("2.3456789012345");
        QMetaObject::invokeMethod(edit.lineEdit(), "textEdited", Qt::DirectConnection,
            Q_ARG(QString, edit.lineEdit()->text()));
        QMetaObject::invokeMethod(edit.lineEdit(), "editingFinished", Qt::DirectConnection);
        check(edit.value() == 2.3456789012345 && committed == 2.3456789012345,
            "ScrubFloatEdit explicit precise text edit still commits all entered digits");
        edit.lineEdit()->setText("nan");
        QMetaObject::invokeMethod(edit.lineEdit(), "textEdited", Qt::DirectConnection,
            Q_ARG(QString, edit.lineEdit()->text()));
        if (increase) increase->click();
        check(std::isfinite(edit.value()) && edit.value() == 2.3456789012345 + 1e-6,
            "ScrubFloatEdit step after invalid nonfinite text preserves finite exact base");
    }
}
}
