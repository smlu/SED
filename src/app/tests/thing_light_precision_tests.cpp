#include "test_common.hpp"
#include "ui/components/thing_light_value_dialog.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "sed/format_number.hpp"
#include <QLineEdit>
#include <QToolButton>
#include <bit>

namespace sed::app_tests
{
void runThingLightPrecisionTests()
{
    using Dialog = ui::ThingLightValueDialog;
    const auto components = [](QString text)
    {
        text.replace('(', ' '); text.replace(')', ' '); text.replace('/', ' ');
        std::vector<double> result;
        for (const auto& token : text.split(' ', Qt::SkipEmptyParts))
        {
            bool valid = false;
            const double number = token.toDouble(&valid);
            if (!valid || !std::isfinite(number)) return std::vector<double>{};
            result.push_back(number);
        }
        return result;
    };
    const auto exact = [](double a, double b)
    { return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b); };
    for (auto mode : {Dialog::Mode::scalar, Dialog::Mode::vector,
             Dialog::Mode::ijimColor, Dialog::Mode::ijimLight})
    {
        const std::size_t expectedComponents = mode == Dialog::Mode::scalar ? 1u
            : mode == Dialog::Mode::ijimLight ? 4u : 3u;
        {
            const double outside = mode == Dialog::Mode::ijimColor || mode == Dialog::Mode::ijimLight
                ? 100.12345678901234 : 100001.123456789;
            const auto authoredOutside = mode == Dialog::Mode::scalar
                ? sed::format::authoredDouble(outside)
                : sed::formatTplTuple({outside, outside, 1e-8, 100001.123456789});
            Dialog outsideDialog(nullptr, "Existing extended light", mode,
                QString::fromStdString(authoredOutside));
            auto outsideValues = components(outsideDialog.valueText());
            check(outsideValues.size() == expectedComponents && exact(outsideValues[0], outside)
                    && (mode == Dialog::Mode::scalar || exact(outsideValues[1], outside))
                    && (mode != Dialog::Mode::ijimLight || exact(outsideValues[3], 100001.123456789)),
                "Structured light untouched values above editing ranges preserve exact authored data");
            auto* first = outsideDialog.findChild<ui::ScrubFloatEdit*>(mode == Dialog::Mode::scalar
                ? QStringLiteral("thingLightValue.intensity") : QStringLiteral("thingLightValue.component.0"));
            if (first)
            {
                QMetaObject::invokeMethod(first->lineEdit(), "editingFinished", Qt::DirectConnection);
                const auto focusedValues = components(outsideDialog.valueText());
                check(focusedValues.size() == expectedComponents && exact(focusedValues[0], outside),
                    "Structured light focus-only completion preserves authored value beyond editing range");
                first->lineEdit()->setText(QStringLiteral("2.3456789012345"));
                QMetaObject::invokeMethod(first->lineEdit(), "textEdited", Qt::DirectConnection,
                    Q_ARG(QString, first->lineEdit()->text()));
                QMetaObject::invokeMethod(first->lineEdit(), "editingFinished", Qt::DirectConnection);
                outsideValues = components(outsideDialog.valueText());
                check(outsideValues.size() == expectedComponents && exact(outsideValues[0], 2.3456789012345)
                        && (mode == Dialog::Mode::scalar || exact(outsideValues[1], outside))
                        && (mode != Dialog::Mode::ijimLight || exact(outsideValues[3], 100001.123456789)),
                    "Structured light one-field edit preserves all untouched out-of-range components");
            }
        }
        const double initial = std::nextafter(1.0, 2.0);
        const std::string authored = mode == Dialog::Mode::scalar
            ? sed::format::authoredDouble(initial)
            : sed::formatTplTuple({initial, -0.0, 1e-8, 12345.678901234567});
        QString lastPreview;
        Dialog dialog(nullptr, "Exact light", mode, QString::fromStdString(authored),
            [&](const QString& value) { lastPreview = value; });
        auto values = components(dialog.valueText());
        const QString expectedInitial = mode == Dialog::Mode::scalar
            ? QStringLiteral("1.0000000000000002") : mode == Dialog::Mode::ijimLight
            ? QStringLiteral("(1.0000000000000002/-0/1e-08/12345.678901234567)")
            : QStringLiteral("(1.0000000000000002/-0/1e-08)");
        check(dialog.valueText() == expectedInitial,
            "Structured light output matches complete fixed mode-specific initial token");
        check(!values.empty() && exact(values[0], initial)
                && (mode == Dialog::Mode::scalar || (values.size() >= 3
                    && exact(values[1], -0.0) && exact(values[2], 1e-8)))
                && (mode != Dialog::Mode::ijimLight || (values.size() == 4
                    && exact(values[3], 12345.678901234567))),
            "Structured light dialog untouched values preserve exact authored components in every mode");
        auto* edit = dialog.findChild<ui::ScrubFloatEdit*>(mode == Dialog::Mode::scalar
            ? QStringLiteral("thingLightValue.intensity") : QStringLiteral("thingLightValue.component.0"));
        check(edit != nullptr, "Structured light numeric field keeps exact state");
        if (!edit) continue;
        QToolButton* increase = nullptr;
        for (auto* button : edit->findChildren<QToolButton*>())
            if (button->text() == ">") increase = button;
        if (increase) increase->click();
        values = components(dialog.valueText());
        const double step = mode == Dialog::Mode::vector ? 0.1 : 0.05;
        check(increase && !values.empty() && exact(values[0], initial + step),
            "Structured light step preserves hidden authored digits");
        const double typed = std::nextafter(2.0, 3.0);
        edit->lineEdit()->setText(QString::fromStdString(sed::format::authoredDouble(typed)));
        QMetaObject::invokeMethod(edit->lineEdit(), "textEdited", Qt::DirectConnection,
            Q_ARG(QString, edit->lineEdit()->text()));
        const auto preview = components(lastPreview);
        const QString expectedTyped = mode == Dialog::Mode::scalar
            ? QStringLiteral("2.0000000000000004") : mode == Dialog::Mode::ijimLight
            ? QStringLiteral("(2.0000000000000004/-0/1e-08/12345.678901234567)")
            : QStringLiteral("(2.0000000000000004/-0/1e-08)");
        check(lastPreview == expectedTyped,
            "Structured light preview callback publishes complete fixed authored token");
        check(!preview.empty() && exact(preview[0], typed),
            "Structured light typed preview retains exact entered digits before focus leaves");
        QMetaObject::invokeMethod(edit->lineEdit(), "editingFinished", Qt::DirectConnection);
        values = components(dialog.valueText());
        check(values.size() == expectedComponents && exact(values[0], typed)
                && dialog.valueText() == expectedTyped,
            "Structured light typed commit retains exact complete token and every component");
        const double lastValid = 3.123456789012345;
        edit->lineEdit()->setText(QString::fromStdString(sed::format::authoredDouble(lastValid)));
        QMetaObject::invokeMethod(edit->lineEdit(), "textEdited", Qt::DirectConnection,
            Q_ARG(QString, edit->lineEdit()->text()));
        edit->lineEdit()->setText(QStringLiteral("3.123invalid"));
        QMetaObject::invokeMethod(edit->lineEdit(), "textEdited", Qt::DirectConnection,
            Q_ARG(QString, edit->lineEdit()->text()));
        QMetaObject::invokeMethod(edit->lineEdit(), "editingFinished", Qt::DirectConnection);
        values = components(dialog.valueText());
        check(!values.empty() && exact(values[0], lastValid) && exact(edit->value(), lastValid),
            "Structured light invalid typed suffix retains matching last-valid authored and displayed values");
    }
}
}
