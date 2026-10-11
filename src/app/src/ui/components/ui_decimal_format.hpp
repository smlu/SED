#pragma once

#include "app_settings.hpp"

#include <QLocale>
#include <QMetaObject>
#include <QString>

#include <cstdint>
#include <functional>

class QApplication;
class QObject;

namespace sed::ui
{

// UI-only locale: never install this through QLocale::setDefault or use these
// helpers for engine/native/resource/clipboard/JSON/CBOR protocol text.
QLocale numericLocaleForPreference(UiDecimalSeparator separator,
    const QLocale& systemLocale);
const QLocale& uiNumericLocale();
QLocale uiSystemNumericLocale();
UiDecimalSeparator uiDecimalSeparator();
std::uint64_t uiNumericFormatRevision();

bool uiDecimalPolicyInstalled();
void installUiDecimalPolicy(QApplication& application,
    UiDecimalSeparator separator, const QLocale& systemLocale = QLocale::system());
void applyUiDecimalSeparator(UiDecimalSeparator separator);

QString formatUiNumber(double value, char format = 'g', int precision = 6);
double parseUiNumber(const QString& text, bool* ok = nullptr);
double parseUiNumber(const QString& text, const QLocale& inputLocale, bool* ok = nullptr);

// Preserve canonical shortest-float spellings while presenting explicitly
// numeric UI tokens/readouts. Do not pass resource names, scripts or prose.
QString localizeUiNumericText(const QString& canonicalNumericText);
// An invalid, grouped, or nonfinite UI token has no canonical numeric spelling
// and returns an empty string. Raw drafts must be stored separately verbatim.
QString canonicalUiNumericText(const QString& localizedNumericToken);
QString canonicalUiNumericText(const QString& localizedNumericToken,
    const QLocale& inputLocale);

// The context owns this subscription. Refresh presentation from numeric model
// values without rebuilding editors or authoring/committing their raw drafts.
QMetaObject::Connection onUiNumericFormatChanged(QObject* context,
    std::function<void()> refresh);

} // namespace sed::ui
