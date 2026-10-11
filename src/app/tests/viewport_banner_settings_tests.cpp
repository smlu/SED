#include "test_common.hpp"

#include "app_settings.hpp"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>

#include <array>
#include <limits>
#include <string_view>

namespace sed::app_tests
{
namespace
{

constexpr std::size_t indexOf(const ViewportBannerKind kind)
{
    return static_cast<std::size_t>(kind);
}

QString settingKey(const ViewportBannerKind kind, const char* const field)
{
    return QStringLiteral("viewport/banners/%1/%2")
        .arg(QString::fromLatin1(kViewportBannerKindIds[indexOf(kind)]),
            QString::fromLatin1(field));
}

void testDefaults()
{
    const ViewportBannerSettings defaults{};
    constexpr std::array<std::string_view, kViewportBannerKindCount> expectedIds{
        "general", "projection", "fly_speed", "error", "cleave_rotate",
        "cleave_scale", "placement_yaw",
    };
    constexpr std::array<std::string_view, kViewportBannerKindCount> expectedLabels{
        "Messages", "Projection", "Fly speed", "Errors", "Cleave rotation",
        "Cleave scale", "Placement rotation",
    };
    bool namesMatch = true;
    for ( std::size_t index = 0; index < kViewportBannerKindCount; ++index )
    {
        namesMatch = namesMatch
            && std::string_view(kViewportBannerKindIds[index]) == expectedIds[index]
            && std::string_view(kViewportBannerKindLabels[index]) == expectedLabels[index];
    }
    check(namesMatch,
        "viewport banner settings: stable IDs and friendly labels match the UI contract");

    const QColor ordinaryText(235, 236, 240, 255);
    const QColor ordinaryBackground(18, 18, 22, 200);
    const QColor transparent(0, 0, 0, 0);
    const QColor errorText(255, 120, 120, 255);
    const QColor errorBackground(40, 12, 12, 210);
    const QColor cleaveText(248, 249, 252, 250);
    const QColor cleaveBackground(18, 21, 27, 232);
    const std::array<ViewportBannerStyle, kViewportBannerKindCount> expected{
        ViewportBannerStyle{ 2000, true, ordinaryText, ordinaryBackground, transparent },
        ViewportBannerStyle{ 2500, false, ordinaryText, ordinaryBackground, transparent },
        ViewportBannerStyle{ 900, false, ordinaryText, ordinaryBackground, transparent },
        ViewportBannerStyle{ 3000, true, errorText, errorBackground, transparent },
        ViewportBannerStyle{ 400, false, cleaveText, cleaveBackground,
            QColor(126, 190, 255, 235) },
        ViewportBannerStyle{ 400, false, cleaveText, cleaveBackground,
            QColor(143, 220, 170, 235) },
        ViewportBannerStyle{ 900, false, ordinaryText, ordinaryBackground, transparent },
    };
    check(defaults.styles == expected,
        "viewport banner settings: every kind has its specified duration and RGBA defaults");
    QString error{};
    check(validateViewportBannerSettings(defaults, &error) && error.isEmpty(),
        "viewport banner settings: defaults satisfy the typed settings contract");
}

void testRoundTripAndLegacyAbsence()
{
    QTemporaryDir directory{};
    check(directory.isValid(),
        "viewport banner settings: round-trip fixture creates isolated settings storage");
    if ( !directory.isValid() )
    {
        return;
    }
    const QString path = directory.filePath(QStringLiteral("banners.ini"));
    QSettings settings(path, QSettings::IniFormat);
    settings.setFallbacksEnabled(false);
    settings.clear();

    const ViewportBannerSettings defaults{};
    check(loadViewportBannerSettings(settings) == defaults,
        "viewport banner settings: absent legacy keys preserve all defaults");

    ViewportBannerSettings edited = defaults;
    for ( std::size_t index = 0; index < edited.styles.size(); ++index )
    {
        ViewportBannerStyle& style = edited.styles[index];
        style.hideAfterMs = 100 + static_cast<int>(index) * 137;
        style.useMessageDuration = index == indexOf(ViewportBannerKind::error);
        style.textColor = QColor(12 + static_cast<int>(index),
            34 + static_cast<int>(index), 56 + static_cast<int>(index),
            78 + static_cast<int>(index));
        style.backgroundColor = QColor(90 + static_cast<int>(index),
            110 + static_cast<int>(index), 130 + static_cast<int>(index),
            150 + static_cast<int>(index));
        style.borderColor = QColor(170 + static_cast<int>(index),
            190 + static_cast<int>(index), 210 + static_cast<int>(index),
            230 + static_cast<int>(index));
    }
    edited.styles[indexOf(ViewportBannerKind::general)].useMessageDuration = false;

    check(validateViewportBannerSettings(edited),
        "viewport banner settings: independently edited values validate");
    check(saveViewportBannerSettings(settings, edited),
        "viewport banner settings: valid typed values save successfully");

    bool exactArgbStrings = true;
    for ( std::size_t index = 0; index < edited.styles.size(); ++index )
    {
        const ViewportBannerKind kind = static_cast<ViewportBannerKind>(index);
        const ViewportBannerStyle& style = edited.styles[index];
        exactArgbStrings = exactArgbStrings
            && settings.value(settingKey(kind, "text_color")).toString()
                == style.textColor.name(QColor::HexArgb)
            && settings.value(settingKey(kind, "background_color")).toString()
                == style.backgroundColor.name(QColor::HexArgb)
            && settings.value(settingKey(kind, "border_color")).toString()
                == style.borderColor.name(QColor::HexArgb);
    }
    check(exactArgbStrings,
        "viewport banner settings: colors persist as exact alpha-preserving HexArgb strings");

    constexpr std::array<const char*, 5> fields{
        "hide_after_ms", "use_message_duration", "text_color",
        "background_color", "border_color",
    };
    bool everyKeyUsesTheStableIds = static_cast<std::size_t>(settings.allKeys().size())
        == kViewportBannerKindCount * fields.size();
    for ( std::size_t index = 0; index < kViewportBannerKindCount; ++index )
    {
        const ViewportBannerKind kind = static_cast<ViewportBannerKind>(index);
        for ( const char* const field : fields )
        {
            everyKeyUsesTheStableIds = everyKeyUsesTheStableIds
                && settings.contains(settingKey(kind, field));
        }
    }
    check(everyKeyUsesTheStableIds,
        "viewport banner settings: save uses all five required fields under every stable ID");

    QSettings reopened(path, QSettings::IniFormat);
    reopened.setFallbacksEnabled(false);
    check(loadViewportBannerSettings(reopened) == edited,
        "viewport banner settings: all per-kind durations, booleans and RGBA colors round-trip from disk");

    const QString uncachedPath = directory.filePath(QStringLiteral("uncached-copy.ini"));
    check(QFile::copy(path, uncachedPath),
        "viewport banner settings: serialized INI copies to a never-used settings path");
    QSettings uncached(uncachedPath, QSettings::IniFormat);
    uncached.setFallbacksEnabled(false);
    check(loadViewportBannerSettings(uncached) == edited,
        "viewport banner settings: canonical persisted true/false values round-trip through an uncached INI path");
}

void testMalformedFieldsFallBackIndependently()
{
    QTemporaryDir directory{};
    check(directory.isValid(),
        "viewport banner settings: malformed-value fixture creates isolated settings storage");
    if ( !directory.isValid() )
    {
        return;
    }
    QSettings settings(directory.filePath(QStringLiteral("malformed.ini")),
        QSettings::IniFormat);
    settings.setFallbacksEnabled(false);
    const ViewportBannerSettings defaults{};

    const ViewportBannerKind projection = ViewportBannerKind::projection;
    settings.setValue(settingKey(projection, "hide_after_ms"), 2500.5);
    settings.setValue(settingKey(projection, "use_message_duration"),
        QStringLiteral("true-ish"));
    settings.setValue(settingKey(ViewportBannerKind::general,
        "use_message_duration"), QStringLiteral("false"));
    settings.setValue(settingKey(projection, "text_color"),
        QStringLiteral("red"));
    settings.setValue(settingKey(projection, "background_color"),
        QStringLiteral("#AA123456"));
    settings.setValue(settingKey(projection, "border_color"),
        QColor(1, 2, 3, 4));
    const ViewportBannerSettings partiallyDamaged =
        loadViewportBannerSettings(settings);
    const ViewportBannerStyle& damagedProjection =
        partiallyDamaged.styles[indexOf(projection)];
    check(damagedProjection.hideAfterMs == defaults.styles[indexOf(projection)].hideAfterMs
            && !damagedProjection.useMessageDuration
            && damagedProjection.textColor == defaults.styles[indexOf(projection)].textColor
            && damagedProjection.backgroundColor == QColor(0x12, 0x34, 0x56, 0xAA)
            && damagedProjection.borderColor == defaults.styles[indexOf(projection)].borderColor,
        "viewport banner settings: each malformed field defaults alone while valid RGBA fields load");
    check(!partiallyDamaged.styles[indexOf(ViewportBannerKind::general)].useMessageDuration,
        "viewport banner settings: canonical false strings are accepted as persisted booleans");

    struct MalformedBoolean
    {
        ViewportBannerKind kind{};
        QVariant value{};
    };
    const std::array malformedBooleans{
        MalformedBoolean{ ViewportBannerKind::general, QVariant(0) },
        MalformedBoolean{ ViewportBannerKind::general, QVariant(0.0) },
        MalformedBoolean{ ViewportBannerKind::general, QVariant(QStringLiteral("FALSE")) },
        MalformedBoolean{ ViewportBannerKind::projection, QVariant(1) },
        MalformedBoolean{ ViewportBannerKind::projection, QVariant(1.0) },
        MalformedBoolean{ ViewportBannerKind::projection, QVariant(QStringLiteral("1")) },
        MalformedBoolean{ ViewportBannerKind::projection, QVariant(QStringLiteral("true-ish")) },
    };
    for ( const MalformedBoolean& malformed : malformedBooleans )
    {
        settings.setValue(settingKey(malformed.kind, "use_message_duration"),
            malformed.value);
        const ViewportBannerSettings loaded = loadViewportBannerSettings(settings);
        check(loaded.styles[indexOf(malformed.kind)].useMessageDuration
                == defaults.styles[indexOf(malformed.kind)].useMessageDuration,
            "viewport banner settings: numeric, noncanonical and malformed booleans default independently");
    }

    settings.setValue(settingKey(ViewportBannerKind::error,
        "use_message_duration"), QStringLiteral("false"));
    check(!loadViewportBannerSettings(settings)
                .styles[indexOf(ViewportBannerKind::error)].useMessageDuration,
        "viewport banner settings: exact false strings are accepted for Errors");
    settings.setValue(settingKey(ViewportBannerKind::general,
        "use_message_duration"), QStringLiteral("true"));
    check(loadViewportBannerSettings(settings)
                .styles[indexOf(ViewportBannerKind::general)].useMessageDuration,
        "viewport banner settings: exact true strings are accepted for General messages");

    const ViewportBannerKind rotate = ViewportBannerKind::cleaveRotate;
    settings.setValue(settingKey(rotate, "use_message_duration"), true);
    const ViewportBannerSettings invalidKindOverride = loadViewportBannerSettings(settings);
    check(!invalidKindOverride.styles[indexOf(rotate)].useMessageDuration
            && invalidKindOverride.styles[indexOf(rotate)].hideAfterMs == 400,
        "viewport banner settings: message-duration true is ignored for fixed-duration kinds");

    const std::array malformedDurations{
        QVariant(true),
        QVariant(1.5),
        QVariant(-1),
        QVariant(0),
        QVariant(99),
        QVariant(QStringLiteral("30001")),
        QVariant(QStringLiteral("-1")),
        QVariant(QStringLiteral("2500ms")),
        QVariant(QStringLiteral("999999999999999999999999999999")),
    };
    for ( const QVariant& malformed : malformedDurations )
    {
        settings.setValue(settingKey(projection, "hide_after_ms"), malformed);
        check(loadViewportBannerSettings(settings).styles[indexOf(projection)].hideAfterMs
                == defaults.styles[indexOf(projection)].hideAfterMs,
            "viewport banner settings: bool, fractional, out-of-range, malformed and overflow durations default safely");
    }

    settings.setValue(settingKey(projection, "hide_after_ms"), QStringLiteral("100"));
    check(loadViewportBannerSettings(settings).styles[indexOf(projection)].hideAfterMs == 100,
        "viewport banner settings: the inclusive 100 ms minimum loads");
    settings.setValue(settingKey(projection, "hide_after_ms"), 30000);
    check(loadViewportBannerSettings(settings).styles[indexOf(projection)].hideAfterMs == 30000,
        "viewport banner settings: the inclusive 30000 ms maximum loads");
}

void testInvalidSaveIsFailureAtomic()
{
    QTemporaryDir directory{};
    check(directory.isValid(),
        "viewport banner settings: atomic-save fixture creates isolated settings storage");
    if ( !directory.isValid() )
    {
        return;
    }
    QSettings settings(directory.filePath(QStringLiteral("atomic.ini")),
        QSettings::IniFormat);
    settings.setFallbacksEnabled(false);
    settings.setValue(settingKey(ViewportBannerKind::general, "hide_after_ms"), 1234);
    settings.setValue(QStringLiteral("unrelated/keep"), QStringLiteral("original"));
    const QStringList keysBefore = settings.allKeys();

    ViewportBannerSettings invalid{};
    invalid.styles[indexOf(ViewportBannerKind::projection)].hideAfterMs = 99;
    QString error{};
    check(!validateViewportBannerSettings(invalid, &error) && !error.isEmpty(),
        "viewport banner settings: typed validation rejects values below the duration bound");
    check(!saveViewportBannerSettings(settings, invalid),
        "viewport banner settings: an invalid candidate is rejected before save");
    check(settings.allKeys() == keysBefore
            && settings.value(settingKey(ViewportBannerKind::general, "hide_after_ms")).toInt() == 1234
            && settings.value(QStringLiteral("unrelated/keep")).toString()
                == QStringLiteral("original"),
        "viewport banner settings: invalid save does not mutate existing or unrelated settings");

    invalid = ViewportBannerSettings{};
    invalid.styles[indexOf(ViewportBannerKind::flySpeed)].useMessageDuration = true;
    check(!validateViewportBannerSettings(invalid),
        "viewport banner settings: only General and Errors may use caller message duration");

    invalid = ViewportBannerSettings{};
    invalid.styles[indexOf(ViewportBannerKind::placementYaw)].textColor = QColor{};
    check(!validateViewportBannerSettings(invalid),
        "viewport banner settings: invalid QColor values fail typed validation");

    invalid = ViewportBannerSettings{};
    invalid.styles[indexOf(ViewportBannerKind::error)].hideAfterMs = 30001;
    check(!validateViewportBannerSettings(invalid),
        "viewport banner settings: typed validation rejects values above the duration bound");
}

void testFailedSettingsWriteDoesNotPublishDraft()
{
    QTemporaryDir directory{};
    check(directory.isValid(),
        "viewport banner settings: failed-write fixture creates isolated temporary storage");
    if ( !directory.isValid() )
    {
        return;
    }

    const QString path = directory.filePath(QStringLiteral("save-failure.ini"));
    const QString baselineCopyPath = directory.filePath(
        QStringLiteral("save-failure-baseline.ini"));
    const QString existingBannerKey = settingKey(
        ViewportBannerKind::general, "hide_after_ms");
    QStringList keysBefore{};
    QSettings::SettingsMap valuesBefore{};
    bool baselineRestored = false;
    {
        QSettings settings(path, QSettings::IniFormat);
        settings.setFallbacksEnabled(false);
        settings.setValue(QStringLiteral("unrelated/keep"), QStringLiteral("baseline"));
        settings.setValue(existingBannerKey, QStringLiteral("legacy-duration"));
        settings.sync();
        check(settings.status() == QSettings::NoError,
            "viewport banner settings: legacy baseline writes cleanly before failure injection");

        keysBefore = settings.allKeys();
        for ( const QString& key : keysBefore )
        {
            valuesBefore.insert(key, settings.value(key));
        }
        const bool copiedBaseline = QFile::copy(path, baselineCopyPath);
        check(copiedBaseline
                && !keysBefore.contains(settingKey(
                    ViewportBannerKind::general, "background_color"))
                && !keysBefore.contains(settingKey(
                    ViewportBannerKind::error, "text_color")),
            "viewport banner settings: baseline keeps many banner keys absent and is copied for later verification");
        if ( !copiedBaseline )
        {
            return;
        }

        const bool removedFile = QFile::remove(path);
        const bool obstructedPath = removedFile && QDir().mkdir(path);
        check(obstructedPath && settings.status() == QSettings::NoError,
            "viewport banner settings: temporary directory obstruction follows a clean legacy baseline");
        if ( !obstructedPath )
        {
            return;
        }

        ViewportBannerSettings draft{};
        draft.styles[indexOf(ViewportBannerKind::general)].hideAfterMs = 4100;
        draft.styles[indexOf(ViewportBannerKind::error)].textColor = QColor(2, 4, 6, 8);
        const bool saved = saveViewportBannerSettings(settings, draft);

        bool valuesUnchanged = settings.allKeys() == keysBefore
            && settings.value(existingBannerKey).metaType().id() == QMetaType::QString
            && settings.value(existingBannerKey).toString() == QStringLiteral("legacy-duration");
        for ( const QString& key : keysBefore )
        {
            valuesUnchanged = valuesUnchanged
                && settings.value(key) == valuesBefore.value(key);
        }
        const bool newlyCreatedKeysAbsent = !settings.contains(settingKey(
                ViewportBannerKind::general, "background_color"))
            && !settings.contains(settingKey(ViewportBannerKind::error, "text_color"));
        check(!saved && settings.status() == QSettings::AccessError,
            "viewport banner settings: attempted commit fails after the clean backend becomes obstructed");
        check(valuesUnchanged && newlyCreatedKeysAbsent,
            "viewport banner settings: failed QSettings sync restores old values and keeps previously absent keys absent");

        ViewportBannerSettings secondDraft{};
        secondDraft.styles[indexOf(ViewportBannerKind::general)].hideAfterMs = 5100;
        check(!saveViewportBannerSettings(settings, secondDraft)
                && settings.allKeys() == keysBefore
                && settings.value(existingBannerKey) == valuesBefore.value(existingBannerKey),
            "viewport banner settings: a pre-errored QSettings rejects another save before mutating its cache");

        const bool obstructionRemoved = QDir().rmdir(path);
        baselineRestored = obstructionRemoved
            && QFile::copy(baselineCopyPath, path);
        check(baselineRestored,
            "viewport banner settings: removing the obstruction restores the original backend file");
        if ( baselineRestored )
        {
            // A later flush/destructor must not publish a draft that the failed
            // save already rolled back from QSettings' in-memory map.
            settings.sync();
        }
    }

    const QString uncachedPath = directory.filePath(
        QStringLiteral("save-failure-uncached.ini"));
    const bool copiedAfterFailure = baselineRestored && QFile::copy(path, uncachedPath);
    bool freshValuesUnchanged = copiedAfterFailure;
    if ( copiedAfterFailure )
    {
        QSettings fresh(uncachedPath, QSettings::IniFormat);
        fresh.setFallbacksEnabled(false);
        freshValuesUnchanged = fresh.allKeys() == keysBefore;
        for ( const QString& key : keysBefore )
        {
            freshValuesUnchanged = freshValuesUnchanged
                && fresh.value(key) == valuesBefore.value(key);
        }
        freshValuesUnchanged = freshValuesUnchanged
            && !fresh.contains(settingKey(
                ViewportBannerKind::general, "background_color"))
            && !fresh.contains(settingKey(
                ViewportBannerKind::error, "text_color"));
    }
    check(freshValuesUnchanged,
        "viewport banner settings: fresh settings read after failure cannot observe a rejected draft or added keys");
}

void testDurationPolicy()
{
    ViewportBannerSettings settings{};
    check(viewportBannerDurationMs(settings, ViewportBannerKind::general, 4321) == 4321
            && viewportBannerDurationMs(settings, ViewportBannerKind::error, 3210) == 3210,
        "viewport banner settings: General and Errors preserve caller durations by default");
    check(viewportBannerDurationMs(settings, ViewportBannerKind::projection, 90) == 2500
            && viewportBannerDurationMs(settings, ViewportBannerKind::flySpeed, 90) == 900
            && viewportBannerDurationMs(settings, ViewportBannerKind::cleaveRotate, 90) == 400
            && viewportBannerDurationMs(settings, ViewportBannerKind::cleaveScale, 90) == 400
            && viewportBannerDurationMs(settings, ViewportBannerKind::placementYaw, 90) == 900,
        "viewport banner settings: fixed-duration kinds use their per-kind defaults");

    settings.styles[indexOf(ViewportBannerKind::general)].useMessageDuration = false;
    settings.styles[indexOf(ViewportBannerKind::general)].hideAfterMs = 1475;
    check(viewportBannerDurationMs(settings, ViewportBannerKind::general, 4321) == 1475,
        "viewport banner settings: disabling message duration selects the explicit General override");
    check(viewportBannerDurationMs(ViewportBannerSettings{}, ViewportBannerKind::general, 1) == 1
            && viewportBannerDurationMs(ViewportBannerSettings{}, ViewportBannerKind::error, 50000) == 50000,
        "viewport banner settings: positive General/Error caller durations retain their original values");
    check(viewportBannerDurationMs(ViewportBannerSettings{}, ViewportBannerKind::general, 0) == 2000,
        "viewport banner settings: a missing caller duration uses the configured General fallback");

    settings.styles[indexOf(ViewportBannerKind::projection)].hideAfterMs = -1;
    check(viewportBannerDurationMs(settings, ViewportBannerKind::projection, 4000) == 2500,
        "viewport banner settings: invalid in-memory duration falls back safely");
    check(viewportBannerDurationMs(settings,
              static_cast<ViewportBannerKind>(std::numeric_limits<std::uint8_t>::max()),
              4000) == 2000,
        "viewport banner settings: invalid enum values cannot index outside the style array");
}

} // namespace

void runViewportBannerSettingsTests()
{
    testDefaults();
    testRoundTripAndLegacyAbsence();
    testMalformedFieldsFallBackIndependently();
    testInvalidSaveIsFailureAtomic();
    testFailedSettingsWriteDoesNotPublishDraft();
    testDurationPolicy();
}

} // namespace sed::app_tests
