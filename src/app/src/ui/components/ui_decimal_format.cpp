#include "ui_decimal_format.hpp"
#include "ui_decimal_policy.hpp"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QEvent>
#include <QLineEdit>
#include <QPointer>
#include <QSignalBlocker>
#include <QTimer>

#include <cmath>
#include <utility>

namespace sed::ui
{
namespace
{

QPointer<detail::UiDecimalPolicy> activePolicy;

UiDecimalSeparator validSeparator(const UiDecimalSeparator separator)
{
    return separator == UiDecimalSeparator::comma || separator == UiDecimalSeparator::system
        ? separator : UiDecimalSeparator::dot;
}

QLocale withoutGroups(QLocale locale)
{
    locale.setNumberOptions(locale.numberOptions()
        | QLocale::OmitGroupSeparator | QLocale::RejectGroupSeparator);
    return locale;
}

detail::UiDecimalPolicy* policy()
{
    if ( !activePolicy && qApp )
    {
        activePolicy = new detail::UiDecimalPolicy(*qApp,
            UiDecimalSeparator::dot, QLocale::system());
    }
    return activePolicy.data();
}

} // namespace

QLocale numericLocaleForPreference(const UiDecimalSeparator separator,
    const QLocale& systemLocale)
{
    switch ( validSeparator(separator) )
    {
        case UiDecimalSeparator::comma:
            return withoutGroups(QLocale(QLocale::German, QLocale::Germany));
        case UiDecimalSeparator::system:
            return withoutGroups(systemLocale);
        case UiDecimalSeparator::dot:
            return withoutGroups(QLocale::c());
    }
    return withoutGroups(QLocale::c());
}

const QLocale& uiNumericLocale()
{
    if ( const auto* current = policy() )
    {
        return current->numericLocale();
    }
    static const QLocale dotLocale = numericLocaleForPreference(
        UiDecimalSeparator::dot, QLocale::c());
    return dotLocale;
}

UiDecimalSeparator uiDecimalSeparator()
{
    return activePolicy ? activePolicy->separator() : UiDecimalSeparator::dot;
}

QLocale uiSystemNumericLocale()
{
    return activePolicy ? activePolicy->systemLocale() : QLocale::system();
}

std::uint64_t uiNumericFormatRevision()
{
    return activePolicy ? activePolicy->revision() : 0;
}

bool uiDecimalPolicyInstalled()
{
    return !activePolicy.isNull();
}

void installUiDecimalPolicy(QApplication& application,
    const UiDecimalSeparator separator, const QLocale& systemLocale)
{
    if ( activePolicy )
    {
        activePolicy->apply(separator, systemLocale);
    }
    else
    {
        activePolicy = new detail::UiDecimalPolicy(application, separator, systemLocale);
    }
}

void applyUiDecimalSeparator(const UiDecimalSeparator separator)
{
    if ( auto* current = policy() )
    {
        current->apply(separator, current->systemLocale());
    }
}

QString formatUiNumber(const double value, const char format, const int precision)
{
    return uiNumericLocale().toString(value, format, precision);
}

double parseUiNumber(const QString& text, bool* const ok)
{
    return parseUiNumber(text, uiNumericLocale(), ok);
}

double parseUiNumber(const QString& text, const QLocale& inputLocale, bool* const ok)
{
    QString input = text.trimmed();
    const QString exponential = inputLocale.exponential();
    if ( !exponential.isEmpty()
        && exponential.compare(QStringLiteral("e"), Qt::CaseInsensitive) != 0
        && !input.contains(exponential, Qt::CaseInsensitive) )
    {
        // The numeric-token presenter keeps canonical e/E spelling. Accept it
        // alongside the locale's exponent without guessing decimal/grouping
        // punctuation or changing the exact draft retained by its editor.
        input.replace(QStringLiteral("e"), exponential, Qt::CaseInsensitive);
    }
    bool accepted = false;
    const double value = withoutGroups(inputLocale).toDouble(input, &accepted);
    accepted = accepted && std::isfinite(value);
    if ( ok )
    {
        *ok = accepted;
    }
    return accepted ? value : 0.0;
}

QString localizeUiNumericText(const QString& canonicalNumericText)
{
    QString localized = canonicalNumericText;
    localized.replace(QStringLiteral("."), uiNumericLocale().decimalPoint());
    return localized;
}

QString canonicalUiNumericText(const QString& localizedNumericToken)
{
    return canonicalUiNumericText(localizedNumericToken, uiNumericLocale());
}

QString canonicalUiNumericText(const QString& localizedNumericToken,
    const QLocale& inputLocale)
{
    bool ok = false;
    (void)parseUiNumber(localizedNumericToken, inputLocale, &ok);
    if ( !ok )
    {
        return {};
    }
    QString canonical = localizedNumericToken.trimmed();
    canonical.replace(inputLocale.decimalPoint(), QStringLiteral("."));
    canonical.replace(inputLocale.negativeSign(), QStringLiteral("-"));
    canonical.replace(inputLocale.positiveSign(), QStringLiteral("+"));
    canonical.replace(inputLocale.exponential(), QStringLiteral("e"), Qt::CaseInsensitive);
    canonical.replace(u'E', u'e');
    for ( qsizetype index = 0; index < canonical.size(); ++index )
    {
        const int digit = canonical.at(index).digitValue();
        if ( digit >= 0 )
        {
            canonical[index] = QChar(static_cast<char16_t>(u'0' + digit));
        }
    }
    if ( canonical.startsWith(u'+') )
    {
        canonical.remove(0, 1);
    }
    return canonical;
}

QMetaObject::Connection onUiNumericFormatChanged(QObject* context,
    std::function<void()> refresh)
{
    auto* const current = policy();
    if ( !current || !context || !refresh )
    {
        return {};
    }
    return QObject::connect(current, &detail::UiDecimalPolicy::numericFormatChanged,
        context, std::move(refresh));
}

namespace detail
{

UiDecimalPolicy::UiDecimalPolicy(QApplication& application,
    const UiDecimalSeparator separator, const QLocale& systemLocale)
    : QObject(&application), separator_(validSeparator(separator)),
      locale_(numericLocaleForPreference(separator_, systemLocale)),
      systemLocale_(systemLocale)
{
    activePolicy = this;
    application.installEventFilter(this);
    updateExistingWidgets();
}

void UiDecimalPolicy::apply(const UiDecimalSeparator separator, const QLocale& systemLocale)
{
    separator_ = validSeparator(separator);
    systemLocale_ = systemLocale;
    const QLocale nextLocale = numericLocaleForPreference(separator_, systemLocale_);
    if ( locale_ == nextLocale )
    {
        return;
    }
    locale_ = nextLocale;
    ++revision_;
    updateExistingWidgets();
    Q_EMIT numericFormatChanged();
}

void UiDecimalPolicy::updateSpin(QDoubleSpinBox* spin)
{
    if ( !spin )
    {
        return;
    }
    if ( !spin->property("sed.uiDecimalPolicyAttached").toBool() )
    {
        spin->setProperty("sed.uiDecimalPolicyAttached", true);
        QObject::connect(spin, &QDoubleSpinBox::editingFinished, this,
            [this, spin]
            {
                const QPointer<QDoubleSpinBox> guard(spin);
                QTimer::singleShot(0, this, [this, guard]
                {
                    if ( guard && guard->hasAcceptableInput() )
                    {
                        if ( auto* edit = guard->findChild<QLineEdit*>() )
                        {
                            edit->setModified(false);
                        }
                        updateSpin(guard);
                    }
                });
            });
    }
    auto* const edit = spin->findChild<QLineEdit*>();
    // Keep the input locale and exact text while the native editor still owns
    // a raw draft. Its next successful commit adopts the new presentation.
    if ( edit && edit->isModified() )
    {
        return;
    }
    if ( spin->locale() == locale_ )
    {
        return;
    }
    const QSignalBlocker spinSignals(spin);
    const QSignalBlocker editSignals(edit);
    applyingWidgetLocale_ = true;
    spin->setLocale(locale_);
    applyingWidgetLocale_ = false;
}

void UiDecimalPolicy::updateValidatedEdit(QLineEdit* edit)
{
    if ( !edit || qobject_cast<QDoubleSpinBox*>(edit->parentWidget()) )
    {
        return;
    }
    auto* const validator = const_cast<QDoubleValidator*>(
        qobject_cast<const QDoubleValidator*>(edit->validator()));
    if ( !validator || edit->isModified() )
    {
        return;
    }
    if ( !edit->property("sed.uiDecimalPolicyAttached").toBool() )
    {
        edit->setProperty("sed.uiDecimalPolicyAttached", true);
        QObject::connect(edit, &QLineEdit::editingFinished, this,
            [this, edit]
            {
                const QPointer<QLineEdit> guard(edit);
                QTimer::singleShot(0, this, [this, guard]
                {
                    if ( guard && guard->hasAcceptableInput() )
                    {
                        guard->setModified(false);
                        updateValidatedEdit(guard);
                    }
                });
            });
    }
    validator->setLocale(locale_);
    applyingWidgetLocale_ = true;
    edit->setLocale(locale_);
    applyingWidgetLocale_ = false;
}

void UiDecimalPolicy::updateExistingWidgets()
{
    const auto widgets = QApplication::allWidgets();
    for ( QWidget* widget : widgets )
    {
        if ( auto* spin = qobject_cast<QDoubleSpinBox*>(widget) )
        {
            updateSpin(spin);
        }
        else if ( auto* edit = qobject_cast<QLineEdit*>(widget) )
        {
            updateValidatedEdit(edit);
        }
    }
}

bool UiDecimalPolicy::eventFilter(QObject* object, QEvent* event)
{
    if ( !applyingWidgetLocale_ && event
        && (event->type() == QEvent::Polish || event->type() == QEvent::Show
            || event->type() == QEvent::ParentChange || event->type() == QEvent::LocaleChange) )
    {
        if ( auto* spin = qobject_cast<QDoubleSpinBox*>(object) )
        {
            updateSpin(spin);
        }
        else if ( auto* edit = qobject_cast<QLineEdit*>(object) )
        {
            updateValidatedEdit(edit);
        }
    }
    return QObject::eventFilter(object, event);
}

} // namespace detail
} // namespace sed::ui
