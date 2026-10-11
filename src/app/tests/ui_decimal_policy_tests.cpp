#include "test_common.hpp"

#include "app_settings.hpp"
#include "main_window/main_window.hpp"
#include "ui/components/precise_numeric_input.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/components/ui_decimal_format.hpp"

#include <sed/assets/snd.hpp>
#include <sed/assets/spr.hpp>
#include <sed/assets/jkl.hpp>
#include <sed/assets/sed.hpp>
#include <sed/assets/string_table.hpp>

#include <QAction>
#include <QApplication>
#include <QCborArray>
#include <QCborValue>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QFile>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>

#include <array>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <limits>
#include <sstream>
#include <span>

namespace sed::app_tests
{
namespace
{

class DefaultLocaleScope final
{
public:
    explicit DefaultLocaleScope(const QLocale& value)
        : previous_(QLocale())
    {
        QLocale::setDefault(value);
    }

    ~DefaultLocaleScope()
    {
        QLocale::setDefault(previous_);
    }

private:
    QLocale previous_;
};

bool driveDecimalPreferences(MainWindow& window, const int choice,
    const bool accept, const bool expectDefault = false,
    const bool checkCommaTooltips = false)
{
    QAction* const action = window.findChild<QAction*>(
        QStringLiteral("edit.preferences"));
    check(action != nullptr,
        "decimal policy: production Preferences action exists");
    if ( !action )
    {
        return false;
    }

    bool foundChoice = false;
    QTimer driver;
    driver.setSingleShot(true);
    QObject::connect(&driver, &QTimer::timeout, &window, [&]
    {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if ( !dialog || dialog->windowTitle() != QStringLiteral("Preferences") )
        {
            dialog = nullptr;
            for ( auto* candidate : window.findChildren<QDialog*>() )
            {
                if ( candidate->windowTitle() == QStringLiteral("Preferences") )
                {
                    dialog = candidate;
                    break;
                }
            }
        }
        check(dialog != nullptr,
            "decimal policy: actual Preferences modal becomes available");
        if ( !dialog )
        {
            return;
        }
        if ( checkCommaTooltips )
        {
            auto* cube = dialog->findChild<QDoubleSpinBox*>(QStringLiteral("pref_cube_size"));
            auto* collision = dialog->findChild<QDoubleSpinBox*>(
                QStringLiteral("pref_mots_tree_collision_preview_mesh_radius_factor"));
            check(cube && cube->toolTip()
                    == QStringLiteral("Half-size of the cube drawn for non-visual things (max 0,05)"),
                "decimal policy: Preferences cube tooltip uses the selected comma in its exact UI string");
            check(collision && collision->toolTip() == QStringLiteral(
                    "MotS collide=2 tree preview only. treesize sets the broad-phase radius; each hierarchy node normally uses "
                    "0,75 x its authored MESH RADIUS. Flagged 0x2 hierarchy nodes inherit an ancestor radius after rendering. "
                    "This setting changes only the displayed node spheres, never level data or exports."),
                "decimal policy: Preferences collision tooltip uses the selected comma without changing prose or engine tokens");
        }
        auto* combo = dialog->findChild<QComboBox*>(
            QStringLiteral("pref_decimal_separator"));
        check(combo != nullptr,
            "decimal policy: Preferences exposes its decimal separator choice");
        if ( combo )
        {
            foundChoice = true;
            check(combo->count() == 3
                && combo->itemText(0) == QStringLiteral("Dot (.)")
                && combo->itemText(1) == QStringLiteral("Comma (,)")
                && combo->itemText(2) == QStringLiteral("System locale")
                && combo->itemData(0).toInt() == 0
                && combo->itemData(1).toInt() == 1
                && combo->itemData(2).toInt() == 2,
                "decimal policy: Preferences has the exact Dot, Comma and System choices");
            if ( expectDefault )
            {
                check(combo->currentData().toInt() == 0,
                    "decimal policy: absent preference defaults to Dot on a comma host");
            }
            combo->setCurrentIndex(combo->findData(choice));
        }
        auto* buttons = dialog->findChild<QDialogButtonBox*>();
        auto* button = buttons ? buttons->button(accept && foundChoice
            ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel) : nullptr;
        check(button != nullptr,
            "decimal policy: actual Preferences provides its requested completion button");
        if ( button )
        {
            button->click();
        }
        else
        {
            dialog->reject();
        }
    });

    QTimer watchdog;
    watchdog.setInterval(1000);
    QObject::connect(&watchdog, &QTimer::timeout, &window, [&]
    {
        if ( auto* modal = qobject_cast<QDialog*>(QApplication::activeModalWidget()) )
        {
            check(false,
                "decimal policy: Preferences completes without an unexpected warning or blocked modal");
            modal->reject();
        }
    });
    watchdog.start();
    driver.start(0);
    action->trigger();
    watchdog.stop();
    driver.stop();
    return foundChoice;
}

void checkUnchangedMachineNumberBytes()
{
    assets::SprFile classic{};
    classic.material = "locale.mat";
    classic.width = 3.25f;
    classic.height = 4.5f;
    classic.extraLightRGBA = { 0.125f, 0.125f, 0.125f, 1.0f };
    classic.offsetXYZ = { 8.25f, -9.5f, 10.75f };
    for ( const auto project : { "JKDF2", "MotS" } )
    {
        (void)project;
        check(assets::saveToSPRText(classic, assets::SprProfile::classic)
                == "locale.mat 0 3.25 4.5 0 0 0 0.125 8.25 -9.5 10.75\n",
            "decimal policy: classic SPR exact independent file vector retains dot");
    }

    assets::SprFile ijim{};
    ijim.material = "ijim.mat";
    ijim.type = 1;
    ijim.width = 2.0f;
    ijim.height = 3.0f;
    ijim.geometryMode = 4;
    ijim.lightMode = 5;
    ijim.textureMode = 6;
    ijim.extraLightRGBA = { 0.125f, 0.25f, 0.375f, 0.5f };
    ijim.offsetXYZ = { -0.625f, 0.75f, 0.875f };
    check(assets::saveToSPRText(ijim, assets::SprProfile::ijim)
            == "ijim.mat 1 2 3 4 5 6 0.125 0.25 0.375 0.5 -0.625 0.75 0.875\n",
        "decimal policy: IJIM SPR exact independent file vector is unchanged");

    assets::SoundClassFile sound{};
    sound.entries.push_back({ "localeMode", "locale.wav", 0x40u,
        0.125f, 1.25f, 0.75f });
    for ( const auto profile : { assets::SoundClassProfile::classic,
             assets::SoundClassProfile::ijim } )
    {
        check(assets::saveToSoundClassText(sound, profile)
                == "localeMode locale.wav 0x00000040 0.125 1.25 0.75\n",
            "decimal policy: SND exact independent file vector retains dot in every profile");
    }

    assets::StringTableFile strings{};
    strings.capacity = 7;
    strings.entries.push_back({ "PRICE", -2147483647 - 1, "1.25 and 1,25" });
    for ( const auto profile : { assets::UniProfile::classic, assets::UniProfile::ijim } )
    {
        check(assets::saveToStringTableUniText(strings, profile)
                == "MSGS 1\n\n\"PRICE\" -2147483648 \"1.25 and 1,25\"\n\nEND\n",
            "decimal policy: UNI exact independent vector preserves opaque decimal-looking prose");
    }

    const auto vectorRoot = std::filesystem::path(__FILE__).parent_path()
        .parent_path().parent_path() / "core/tests/assets/fixtures/sed_vectors";
    for ( const auto* stem : { "jkdf2", "mots", "ijim" } )
    {
        const auto nativePath = vectorRoot / (std::string(stem) + "_native.sed");
        QFile nativeFile(QString::fromStdWString(nativePath.wstring()));
        check(nativeFile.open(QIODevice::ReadOnly),
            "decimal policy: checked-in independent native project vector is available");
        const QByteArray expectedNative = nativeFile.readAll();
        if ( expectedNative.isEmpty() )
        {
            check(false, "decimal policy: native byte golden is not empty");
            continue;
        }
        const auto native = assets::loadSedFileBytes(std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(expectedNative.constData()),
            static_cast<std::size_t>(expectedNative.size())));
        const auto saved = assets::saveToSedBytes(native.level, native.optionalSections,
            assets::SedSerializationProfile::ProtectedV1, 0);
        check(QByteArray(reinterpret_cast<const char*>(saved.data()),
                static_cast<qsizetype>(saved.size())) == expectedNative,
            "decimal policy: complete native SED bytes match the independent project golden at timestamp zero");
        const auto extension = std::string(stem) == "ijim" ? ".ndy" : ".jkl";
        const auto exportPath = vectorRoot / (std::string(stem) + "_expected" + extension);
        QFile exportFile(QString::fromStdWString(exportPath.wstring()));
        check(exportFile.open(QIODevice::ReadOnly),
            "decimal policy: checked-in independent engine export vector is available");
        std::ostringstream exported;
        assets::saveToJKL(native.level, exported);
        check(QByteArray::fromStdString(exported.str()) == exportFile.readAll(),
            "decimal policy: complete JKL/NDY bytes match the independent project golden");
    }

    QJsonObject object;
    object.insert(QStringLiteral("value"), 1.25);
    check(QJsonDocument(object).toJson(QJsonDocument::Compact)
            == QByteArrayLiteral("{\"value\":1.25}"),
        "decimal policy: JSON numeric encoding remains the exact dot vector");
    QCborArray array;
    array.append(1.25);
    check(QCborValue(array).toCbor()
            == QByteArray::fromHex("81fb3ff4000000000000"),
        "decimal policy: CBOR preserves the exact IEEE double numeric vector");
}

void commitScrubText(ui::ScrubFloatEdit& edit, const QString& text)
{
    edit.lineEdit()->setText(text);
    QMetaObject::invokeMethod(edit.lineEdit(), "textEdited", Qt::DirectConnection,
        Q_ARG(QString, text));
    QMetaObject::invokeMethod(edit.lineEdit(), "editingFinished", Qt::DirectConnection);
}

void diagnoseDecimalControls(const char* stage, const ui::ScrubFloatEdit& scrub,
    const ui::ScrubFloatEdit& disabledScrub, const QDoubleSpinBox& spin,
    const QDoubleSpinBox& disabledSpin)
{
    const auto print = [stage](const char* name, const QLineEdit* edit,
        const QLocale& locale, const double value, const bool enabled)
    {
        const QByteArray text = edit ? edit->text().left(80).toUtf8() : QByteArrayLiteral("<no edit>");
        std::fprintf(stderr,
            "UI_DECIMAL stage=%s control=%s locale=%s decimal_hex=%s minus_hex=%s enabled=%d modified=%d value=%.17g text=%s text_hex=%s\n",
            stage, name, locale.name().toUtf8().constData(),
            locale.decimalPoint().toUtf8().toHex().constData(),
            locale.negativeSign().toUtf8().toHex().constData(), enabled,
            edit && edit->isModified(), value, text.constData(), text.toHex().constData());
    };
    print("scrub", scrub.lineEdit(), scrub.lineEdit()->locale(), scrub.value(), scrub.isEnabled());
    print("disabled_scrub", disabledScrub.lineEdit(), disabledScrub.lineEdit()->locale(),
        disabledScrub.value(), disabledScrub.isEnabled());
    print("spin", spin.findChild<QLineEdit*>(), spin.locale(), spin.value(), spin.isEnabled());
    print("disabled_spin", disabledSpin.findChild<QLineEdit*>(), disabledSpin.locale(),
        disabledSpin.value(), disabledSpin.isEnabled());
}

void checkDecimalHelperContracts()
{
    QTemporaryDir directory;
    check(directory.isValid(), "decimal policy: pure settings fixture owns a temporary directory");
    if ( !directory.isValid() )
    {
        return;
    }
    QSettings settings(directory.filePath(QStringLiteral("decimal.ini")), QSettings::IniFormat);
    settings.setFallbacksEnabled(false);
    check(loadUiDecimalSeparator(settings) == UiDecimalSeparator::dot,
        "decimal policy: absent isolated setting returns Dot");
    for ( const auto separator : { UiDecimalSeparator::dot,
             UiDecimalSeparator::comma, UiDecimalSeparator::system } )
    {
        saveUiDecimalSeparator(settings, separator);
        check(loadUiDecimalSeparator(settings) == separator,
            "decimal policy: all three supported preferences round-trip independently");
    }
    for ( const auto& malformed : { QVariant(-1), QVariant(3), QVariant(1.5),
             QVariant(QStringLiteral("comma")), QVariant(QStringLiteral("1,0")) } )
    {
        settings.setValue(QStringLiteral("ui/decimal_separator"), malformed);
        check(loadUiDecimalSeparator(settings) == UiDecimalSeparator::dot,
            "decimal policy: malformed or unsupported stored preferences default safely to Dot");
    }
    const QLocale controlledComma(QLocale::Slovenian, QLocale::Slovenia);
    for ( const auto separator : { UiDecimalSeparator::dot,
             UiDecimalSeparator::comma, UiDecimalSeparator::system } )
    {
        ui::installUiDecimalPolicy(*qApp, separator, controlledComma);
        const bool comma = separator != UiDecimalSeparator::dot;
        check(ui::formatUiNumber(1234.5, 'f', 3) == (comma
                ? QStringLiteral("1234,500") : QStringLiteral("1234.500")),
            "decimal policy: Dot, Comma and controlled System-comma use exact independent display vectors");
        check(ui::formatUiNumber(std::nextafter(1.0, 2.0), 'g', 17) == (comma
                ? QStringLiteral("1,0000000000000002") : QStringLiteral("1.0000000000000002")),
            "decimal policy: separator policy retains concise exact double round-trip digits");
        check(ui::uiNumericLocale().numberOptions().testFlag(QLocale::OmitGroupSeparator)
                && ui::uiNumericLocale().numberOptions().testFlag(QLocale::RejectGroupSeparator),
            "decimal policy: every selected UI locale suppresses and rejects digit grouping");
        bool ok = false;
        const double parsed = ui::parseUiNumber(comma
            ? QStringLiteral(" -1,23456789012345e-3 ")
            : QStringLiteral(" -1.23456789012345e-3 "), &ok);
        check(ok && parsed == -1.23456789012345e-3,
            "decimal policy: selected separator parses exact finite signed exponent edits");
        for ( const QString& malformed : { QStringLiteral("1.234,5"), QStringLiteral("1,234.5"),
                 QStringLiteral("1 234"), QStringLiteral("nan"), QStringLiteral("inf"),
                 QStringLiteral("-"), QStringLiteral("1e"), QStringLiteral("") } )
        {
            ok = true;
            check(ui::parseUiNumber(malformed, &ok) == 0.0 && !ok,
                "decimal policy: invalid, intermediate, grouped and nonfinite tokens reject with assigned failure outputs");
            check(ui::canonicalUiNumericText(malformed).isEmpty(),
                "decimal policy: invalid UI tokens cannot become accepted canonical numeric data");
        }
        ok = true;
        (void)ui::parseUiNumber(comma ? QStringLiteral("1.25") : QStringLiteral("1,25"), &ok);
        check(!ok, "decimal policy: opposite decimal punctuation is rejected instead of guessed as grouping");
        check(ui::canonicalUiNumericText(comma
                ? QStringLiteral(" -0,000E+03 ") : QStringLiteral(" -0.000E+03 "))
                == QStringLiteral("-0.000e+03"),
            "decimal policy: canonical UI bridge preserves exact significant spelling and signed zero");
        check(ui::localizeUiNumericText(QStringLiteral("0.01 / -2.5e-3")) == (comma
                ? QStringLiteral("0,01 / -2,5e-3") : QStringLiteral("0.01 / -2.5e-3")),
            "decimal policy: numeric display bridge preserves shortest float spellings and tuple delimiters");
        checkUnchangedMachineNumberBytes();
    }
    const QLocale controlledArabic(QLocale::Arabic, QLocale::Egypt);
    ui::installUiDecimalPolicy(*qApp, UiDecimalSeparator::system, controlledArabic);
    check(ui::uiNumericLocale().decimalPoint() == QStringLiteral("\u066b")
            && ui::formatUiNumber(1234.5, 'f', 3)
                == QStringLiteral("\u0661\u0662\u0663\u0664\u066b\u0665\u0660\u0660"),
        "decimal policy: controlled System-Arabic has the exact non-dot, non-comma decimal and numeric display vector");
    bool arabicOk = false;
    check(ui::parseUiNumber(QStringLiteral("\u0661\u0662\u066b\u0665"), &arabicOk) == 12.5
            && arabicOk,
        "decimal policy: controlled System-Arabic parses its exact localized digit and decimal vector");
    check(ui::canonicalUiNumericText(QStringLiteral("\u061c-\u0661\u0662\u066b\u0665"))
            == QStringLiteral("-12.5"),
        "decimal policy: System-Arabic signed localized digits become canonical ASCII dot text");
    const QString arabicExponentInput = QStringLiteral("\u0661\u066b\u0662\u0665e-\u0663");
    const QString arabicExponentCanonical = ui::canonicalUiNumericText(arabicExponentInput);
    if ( arabicExponentCanonical != QStringLiteral("1.25e-3") )
    {
        const QString nativeExponentInput = QStringLiteral("\u0661\u066b\u0662\u0665")
            + ui::uiNumericLocale().exponential() + ui::uiNumericLocale().negativeSign()
            + QStringLiteral("\u0663");
        const QString nativeExponentCanonical = ui::canonicalUiNumericText(nativeExponentInput);
        const QString nativeExponentDisplay = ui::formatUiNumber(-0.00125, 'e', 2);
        std::fprintf(stderr,
            "UI_DECIMAL stage=arabic_exponent exponential_hex=%s minus_hex=%s input_hex=%s canonical_hex=%s native_input_hex=%s native_canonical_hex=%s native_display_hex=%s\n",
            ui::uiNumericLocale().exponential().toUtf8().toHex().constData(),
            ui::uiNumericLocale().negativeSign().toUtf8().toHex().constData(),
            arabicExponentInput.toUtf8().toHex().constData(),
            arabicExponentCanonical.toUtf8().toHex().constData(),
            nativeExponentInput.toUtf8().toHex().constData(),
            nativeExponentCanonical.toUtf8().toHex().constData(),
            nativeExponentDisplay.toUtf8().toHex().constData());
    }
    check(arabicExponentCanonical == QStringLiteral("1.25e-3"),
        "decimal policy: System-Arabic localized exponent digits become canonical ASCII exponent text");
    check(ui::localizeUiNumericText(QStringLiteral("-0.000E+03"))
                == QStringLiteral("-0\u066b000E+03")
            && ui::canonicalUiNumericText(QStringLiteral("-0\u066b000E+03"))
                == QStringLiteral("-0.000e+03"),
        "decimal policy: System-Arabic token presentation round-trips ASCII exponent spelling and exact signed-zero digits");
    arabicOk = true;
    (void)ui::parseUiNumber(QStringLiteral("\u0661\u066c\u0662\u0663\u0664\u066b\u0665"), &arabicOk);
    check(!arabicOk, "decimal policy: System-Arabic rejects its native U+066C grouping separator");
    checkUnchangedMachineNumberBytes();
    ui::installUiDecimalPolicy(*qApp, UiDecimalSeparator::dot, controlledComma);
}

} // namespace

void runUiDecimalPolicyTests()
{
    check(appSettingsIniPathIsOverriddenForTest(),
        "decimal policy: actual preferences use a process-isolated INI");
    const DefaultLocaleScope commaHost(QLocale(QLocale::Slovenian, QLocale::Slovenia));
    checkDecimalHelperContracts();
    QSettings settings = settingsStore();
    settings.remove(QStringLiteral("ui/decimal_separator"));
    settings.setValue(QStringLiteral("recovery/autosave_enabled"), false);
    settings.sync();

    MainWindow window;
    ui::ScrubFloatEdit scrub(&window);
    scrub.setDecimals(3);
    scrub.setValue(1234.5);
    ui::ScrubFloatEdit disabledScrub(&window);
    disabledScrub.setDecimals(3);
    disabledScrub.setValue(-12.5);
    disabledScrub.setEnabled(false);
    QDoubleSpinBox spin(&window);
    spin.setRange(-10000.0, 10000.0);
    spin.setDecimals(3);
    spin.setValue(1234.5);
    QDoubleSpinBox disabledSpin(&window);
    disabledSpin.setRange(-100.0, 100.0);
    disabledSpin.setDecimals(3);
    disabledSpin.setValue(-12.5);
    disabledSpin.setEnabled(false);
    window.show();
    QApplication::processEvents();

    check(scrub.lineEdit()->text() == QStringLiteral("1234.500")
            && spin.text() == QStringLiteral("1234.500"),
        "decimal policy: default Dot applies consistently to existing scrub and native spin controls");
    checkUnchangedMachineNumberBytes();

    const bool choiceExists = driveDecimalPreferences(window, 1, false, true);
    check(scrub.lineEdit()->text() == QStringLiteral("1234.500")
            && !settingsStore().contains(QStringLiteral("ui/decimal_separator")),
        "decimal policy: Cancel preserves the exact live default and missing INI key");
    if ( !choiceExists )
    {
        return;
    }

    driveDecimalPreferences(window, 1, true);
    check(settingsStore().value(QStringLiteral("ui/decimal_separator")).toInt() == 1,
        "decimal policy: Preferences OK persists Comma in the isolated INI");
    const bool commaControlsMatch = scrub.lineEdit()->text() == QStringLiteral("1234,500")
            && spin.text() == QStringLiteral("1234,500")
            && disabledScrub.lineEdit()->text() == QStringLiteral("-12,500")
            && disabledSpin.text() == QStringLiteral("-12,500");
    if ( !commaControlsMatch )
    {
        diagnoseDecimalControls("comma", scrub, disabledScrub, spin, disabledSpin);
    }
    check(commaControlsMatch,
        "decimal policy: Comma immediately refreshes enabled and disabled existing numeric controls without groups");
    check(QLocale().decimalPoint() == QStringLiteral(","),
        "decimal policy: UI preference does not mutate the process Qt default locale");
    check(scrub.value() == 1234.5 && disabledScrub.value() == -12.5
            && spin.value() == 1234.5 && disabledSpin.value() == -12.5,
        "decimal policy: changing UI separators does not round or author numeric values");
    commitScrubText(scrub, QStringLiteral("2,3456789012345"));
    check(scrub.value() == 2.3456789012345,
        "decimal policy: comma scrub input commits every authored digit");
    commitScrubText(scrub, QStringLiteral("1.234,5"));
    check(scrub.value() == 2.3456789012345,
        "decimal policy: grouping ambiguity is rejected rather than changing the numeric value");
    checkUnchangedMachineNumberBytes();

    ui::ScrubFloatEdit pendingScrub(&window);
    pendingScrub.setDecimals(3);
    pendingScrub.setValue(6.0);
    pendingScrub.lineEdit()->setText(QStringLiteral("6,125"));
    QMetaObject::invokeMethod(pendingScrub.lineEdit(), "textEdited", Qt::DirectConnection,
        Q_ARG(QString, pendingScrub.lineEdit()->text()));
    const int cursor = 2;
    pendingScrub.lineEdit()->setCursorPosition(cursor);
    int commits = 0;
    QObject::connect(&pendingScrub, &ui::ScrubFloatEdit::valueCommitted,
        &window, [&] { ++commits; });
    ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
    check(pendingScrub.lineEdit()->text() == QStringLiteral("6,125")
            && pendingScrub.lineEdit()->cursorPosition() == cursor
            && pendingScrub.lineEdit()->locale().decimalPoint() == QStringLiteral(",")
            && pendingScrub.value() == 6.125 && commits == 0,
        "decimal policy: live change preserves exact raw edit, cursor, prior parsing locale and uncommitted value");
    QMetaObject::invokeMethod(pendingScrub.lineEdit(), "editingFinished", Qt::DirectConnection);
    check(pendingScrub.value() == 6.125 && commits == 1
            && pendingScrub.lineEdit()->text() == QStringLiteral("6.125"),
        "decimal policy: old-locale raw edit commits once then adopts the current separator");
    pendingScrub.lineEdit()->setText(QStringLiteral("-0."));
    QMetaObject::invokeMethod(pendingScrub.lineEdit(), "textEdited", Qt::DirectConnection,
        Q_ARG(QString, pendingScrub.lineEdit()->text()));
    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    check(pendingScrub.lineEdit()->text() == QStringLiteral("-0.") && commits == 1,
        "decimal policy: incomplete raw edit survives a locale change without normalization or a commit");

    QTimer::singleShot(0, &window, [&]
    {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        auto* edit = dialog ? dialog->findChild<QLineEdit*>() : nullptr;
        check(edit && edit->text() == QStringLiteral("1234,5"),
            "decimal policy: actual precise modal renders the Comma default without grouping");
        if ( dialog && edit )
        {
            edit->setText(QStringLiteral("1,23456789012345"));
            dialog->accept();
        }
        else if ( dialog )
        {
            dialog->reject();
        }
    });
    bool ok = false;
    const double value = ui::preciseNumericInput(&window, QStringLiteral("Decimal policy test"),
        QStringLiteral("Value"), 1234.5, -10000.0, 10000.0, &ok);
    check(ok && value == 1.23456789012345,
        "decimal policy: precise modal parses the complete comma edit without rounding");

    driveDecimalPreferences(window, 0, false, false, true);
    const bool cancelPreservesComma = settingsStore().value(QStringLiteral("ui/decimal_separator")).toInt() == 1
        && disabledSpin.text() == QStringLiteral("-12,500");
    if ( !cancelPreservesComma )
    {
        diagnoseDecimalControls("cancel_dot", scrub, disabledScrub, spin, disabledSpin);
    }
    check(cancelPreservesComma,
        "decimal policy: cancelling Dot preserves exact persisted and disabled Comma controls");
    driveDecimalPreferences(window, 0, true);
    check(scrub.lineEdit()->text() == QStringLiteral("2.346")
            && spin.text() == QStringLiteral("1234.500")
            && disabledSpin.text() == QStringLiteral("-12.500"),
        "decimal policy: Dot applies immediately to existing controls after Comma");
    checkUnchangedMachineNumberBytes();
    driveDecimalPreferences(window, 2, true);
    const bool systemControlsMatch = settingsStore().value(QStringLiteral("ui/decimal_separator")).toInt() == 2
            && spin.text() == QStringLiteral("1234,500")
            && disabledSpin.text() == QStringLiteral("\u221212,500");
    if ( !systemControlsMatch )
    {
        diagnoseDecimalControls("controlled_system", scrub, disabledScrub, spin, disabledSpin);
    }
    check(systemControlsMatch,
        "decimal policy: actual System preference uses the deterministically controlled comma locale");
    checkUnchangedMachineNumberBytes();
    driveDecimalPreferences(window, 0, true);
}

} // namespace sed::app_tests
