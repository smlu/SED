#include "app_settings.hpp"

#include <algorithm>
#include <limits>
#include <optional>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>

namespace sed
{
namespace
{

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
std::filesystem::path appSettingsIniPathOverrideForTest{};
std::filesystem::path appSettingsIniPathBaselineOverrideForTest{};
#endif
#ifdef SED_APP_THEME_STORAGE_TEST_ACCESS
std::filesystem::path appThemeFallbackDirectoryOverrideForTest{};
#endif

bool containsThemeFiles(const std::filesystem::path& directory)
{
    const QString path = QString::fromStdWString(directory.wstring());
    if (path.isEmpty())
    {
        return false;
    }
    const QDir themeDirectory(path);
    return !themeDirectory.entryList(
        { QStringLiteral("*.json") }, QDir::Files | QDir::NoDotAndDotDot).isEmpty();
}

bool containsThemeData(const std::filesystem::path& root)
{
    return containsThemeFiles(root)
        || containsThemeFiles(root / "cog")
        || containsThemeFiles(root / "app");
}

std::filesystem::path defaultThemeDirectory(
    const std::filesystem::path& themeRoot,
    const AppThemeKind kind)
{
    return kind == AppThemeKind::cog ? themeRoot / "cog" : themeRoot;
}

bool supportsAtomicFileWrites(const std::filesystem::path& directory)
{
    const QString path = QString::fromStdWString(directory.wstring());
    if (path.isEmpty() || !QDir().mkpath(path))
    {
        return false;
    }

    const QString probePath = QDir(path).filePath(
        QStringLiteral(".sed-atomic-write-probe-%1.tmp")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    QSaveFile probe(probePath);
    probe.setDirectWriteFallback(false);
    const bool ok = probe.open(QIODevice::WriteOnly)
        && probe.write("SED", 3) == 3
        && probe.commit();
    if (probe.isOpen())
    {
        probe.cancelWriting();
    }
    (void)QFile::remove(probePath);
    return ok;
}

void maybeMigrateLegacySettingsToIni()
{
    static bool migrated = false;
    if (migrated)
    {
        return;
    }
    migrated = true;

    const std::filesystem::path iniPath = appSettingsIniPath();
    if (iniPath.empty())
    {
        return;
    }

    // If INI already exists, nothing to do.
    if (std::filesystem::exists(iniPath))
    {
        return;
    }

    QSettings legacy;
    const QStringList keys = legacy.allKeys();
    if (keys.isEmpty())
    {
        return;
    }

    QSettings ini(QString::fromStdWString(iniPath.wstring()), QSettings::IniFormat);
    for (const QString& key : keys)
    {
        ini.setValue(key, legacy.value(key));
    }
    ini.sync();
}

} // namespace

std::filesystem::path appSettingsIniPath()
{
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    if ( !appSettingsIniPathOverrideForTest.empty() )
    {
        return appSettingsIniPathOverrideForTest;
    }
    if ( !appSettingsIniPathBaselineOverrideForTest.empty() )
    {
        return appSettingsIniPathBaselineOverrideForTest;
    }
#endif
    return std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString()) / "sed.ini";
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
void setAppSettingsIniPathOverrideForTest(const std::filesystem::path& path)
{
    appSettingsIniPathOverrideForTest = path;
}

void setAppSettingsIniPathBaselineOverrideForTest(const std::filesystem::path& path)
{
    appSettingsIniPathBaselineOverrideForTest = path;
}

bool appSettingsIniPathIsOverriddenForTest()
{
    return !appSettingsIniPathOverrideForTest.empty()
        || !appSettingsIniPathBaselineOverrideForTest.empty();
}
#endif

std::filesystem::path appThemePerUserDataDirectory()
{
#ifdef SED_APP_THEME_STORAGE_TEST_ACCESS
    if (!appThemeFallbackDirectoryOverrideForTest.empty())
    {
        return appThemeFallbackDirectoryOverrideForTest;
    }
#endif
    const QString appLocalData = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    return std::filesystem::path(appLocalData.toStdWString());
}

#ifdef SED_APP_THEME_STORAGE_TEST_ACCESS
void setAppThemeFallbackDirectoryOverrideForTest(
    const std::filesystem::path& path)
{
    appThemeFallbackDirectoryOverrideForTest = path;
}
#endif

std::filesystem::path appThemeDirectoryPath(
    const AppThemeKind kind,
    QString* errorMessage)
{
    return appThemeDirectoryPath(
        kind, appSettingsIniPath(), appThemePerUserDataDirectory(), errorMessage);
}

std::filesystem::path appThemeDirectoryPath(
    const AppThemeKind kind,
    const std::filesystem::path& settingsFilePath,
    const std::filesystem::path& perUserApplicationDataDirectory,
    QString* errorMessage)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }
    if (kind != AppThemeKind::viewport && kind != AppThemeKind::cog)
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Unknown theme storage type.");
        }
        return {};
    }

    const std::filesystem::path effectiveSettingsPath = settingsFilePath.empty()
        ? appSettingsIniPath()
        : settingsFilePath;
    const std::filesystem::path portableRoot =
        effectiveSettingsPath.parent_path() / "data" / "themes";
    const std::filesystem::path perUserRoot = perUserApplicationDataDirectory.empty()
        ? std::filesystem::path{}
        : perUserApplicationDataDirectory / "data" / "themes";
    const std::filesystem::path portable =
        defaultThemeDirectory(portableRoot, kind);
    const std::filesystem::path perUser = perUserRoot.empty()
        ? std::filesystem::path{}
        : defaultThemeDirectory(perUserRoot, kind);

    // Keep the per-user fallback active when data exists in either the
    // default location or the accepted alternate grouping.
    if (!perUserRoot.empty() && containsThemeData(perUserRoot)
        && supportsAtomicFileWrites(perUser))
    {
        return perUser;
    }
    if (supportsAtomicFileWrites(portable))
    {
        return portable;
    }
    if (!perUser.empty() && supportsAtomicFileWrites(perUser))
    {
        return perUser;
    }

    if (errorMessage != nullptr)
    {
        *errorMessage = QStringLiteral(
            "Neither the portable theme directory nor the per-user application-data directory supports atomic writes.");
    }
    return {};
}

std::vector<std::filesystem::path> appThemeDirectorySearchPaths(
    const AppThemeKind kind,
    const std::filesystem::path& settingsFilePath,
    const std::filesystem::path& perUserApplicationDataDirectory)
{
    if (kind != AppThemeKind::viewport && kind != AppThemeKind::cog)
    {
        return {};
    }

    const std::filesystem::path effectiveSettingsPath = settingsFilePath.empty()
        ? appSettingsIniPath()
        : settingsFilePath;
    const std::filesystem::path portableRoot =
        effectiveSettingsPath.parent_path() / "data" / "themes";
    const std::filesystem::path perUserRoot = perUserApplicationDataDirectory.empty()
        ? std::filesystem::path{}
        : perUserApplicationDataDirectory / "data" / "themes";
    const std::filesystem::path portableDefault =
        defaultThemeDirectory(portableRoot, kind);
    const std::filesystem::path perUserDefault = perUserRoot.empty()
        ? std::filesystem::path{}
        : defaultThemeDirectory(perUserRoot, kind);
    std::vector<std::filesystem::path> paths{};
    const std::filesystem::path active = appThemeDirectoryPath(
        kind, effectiveSettingsPath, perUserApplicationDataDirectory, nullptr);
    const auto appendUnique = [&paths](const std::filesystem::path& path)
    {
        if (!path.empty()
            && std::find(paths.begin(), paths.end(), path) == paths.end())
        {
            paths.push_back(path);
        }
    };
    appendUnique(active);
    appendUnique(perUserDefault);
    appendUnique(portableDefault);
    if (kind == AppThemeKind::cog)
    {
        // COG files may also be grouped at the shared theme root.
        appendUnique(perUserRoot);
        appendUnique(portableRoot);
    }
    else
    {
        // App/viewport files may also be grouped in an app subfolder.
        if (!perUserRoot.empty())
        {
            appendUnique(perUserRoot / "app");
        }
        appendUnique(portableRoot / "app");
    }
    return paths;
}

std::filesystem::path appRecoveryDirectoryPath()
{
    static const std::filesystem::path directory = []
    {
        const std::filesystem::path portable = appSettingsIniPath().parent_path() / "recovery";
        if (supportsAtomicFileWrites(portable))
        {
            return portable;
        }

        const QString appData = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);
        const std::filesystem::path fallback =
            std::filesystem::path(appData.toStdWString()) / "recovery";
        if (supportsAtomicFileWrites(fallback))
        {
            return fallback;
        }

        // AppLocalData should normally be writable, but profile redirection,
        // ransomware protection, or a damaged ACL can make it unavailable.
        // Keep trying durable per-user locations before the last-resort temp
        // directory so recovery does not silently disappear with the first
        // path failure.
        const std::filesystem::path homeFallback =
            std::filesystem::path(QStandardPaths::writableLocation(
                QStandardPaths::HomeLocation).toStdWString())
            / ".sed" / "recovery";
        if (supportsAtomicFileWrites(homeFallback))
        {
            return homeFallback;
        }

        const std::filesystem::path temporaryFallback =
            std::filesystem::path(QStandardPaths::writableLocation(
                QStandardPaths::TempLocation).toStdWString())
            / "SED" / "recovery";
        (void)supportsAtomicFileWrites(temporaryFallback);
        return temporaryFallback;
    }();
    return directory;
}

std::vector<std::filesystem::path> appRecoverySearchDirectoryPaths()
{
    std::vector<std::filesystem::path> directories{};
    const auto appendUnique = [&directories](const std::filesystem::path& path)
    {
        if (!path.empty()
            && std::find(directories.begin(), directories.end(), path) == directories.end())
        {
            directories.push_back(path);
        }
    };
    appendUnique(appRecoveryDirectoryPath());
    appendUnique(appSettingsIniPath().parent_path() / "recovery");
    appendUnique(std::filesystem::path(QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation).toStdWString()) / "recovery");
    appendUnique(std::filesystem::path(QStandardPaths::writableLocation(
        QStandardPaths::HomeLocation).toStdWString()) / ".sed" / "recovery");
    appendUnique(std::filesystem::path(QStandardPaths::writableLocation(
        QStandardPaths::TempLocation).toStdWString()) / "SED" / "recovery");
    return directories;
}

QSettings settingsStore()
{
    maybeMigrateLegacySettingsToIni();
    return QSettings(QString::fromStdWString(appSettingsIniPath().wstring()), QSettings::IniFormat);
}

namespace
{

constexpr Qt::KeyboardModifiers kViewportNavigationModifierMask =
    Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;

bool validViewportMouseButton(const Qt::MouseButton button) noexcept
{
    return button == Qt::NoButton || button == Qt::LeftButton
        || button == Qt::RightButton || button == Qt::MiddleButton
        || button == Qt::BackButton || button == Qt::ForwardButton;
}

std::optional<int> navigationInteger(const QVariant& value)
{
    const int type = value.metaType().id();
    if ( type != QMetaType::Int && type != QMetaType::UInt
        && type != QMetaType::LongLong && type != QMetaType::ULongLong
        && type != QMetaType::QString )
    {
        return std::nullopt;
    }
    bool valid = false;
    const qlonglong integer = value.toString().toLongLong(&valid);
    if ( !valid || integer < std::numeric_limits<int>::min()
        || integer > std::numeric_limits<int>::max() )
    {
        return std::nullopt;
    }
    return static_cast<int>(integer);
}

Qt::KeyboardModifiers navigationModifiers(
    const QVariant& value, const Qt::KeyboardModifiers fallback)
{
    const auto integer = navigationInteger(value);
    if ( !integer || (*integer & ~int(kViewportNavigationModifierMask)) != 0 )
    {
        return fallback;
    }
    return Qt::KeyboardModifiers(*integer);
}

bool navigationBoolean(const QVariant& value, const bool fallback)
{
    if ( value.metaType().id() == QMetaType::Bool )
    {
        return value.toBool();
    }
    const QString text = value.toString().trimmed();
    if ( text == QStringLiteral("true") || text == QStringLiteral("1") )
    {
        return true;
    }
    if ( text == QStringLiteral("false") || text == QStringLiteral("0") )
    {
        return false;
    }
    return fallback;
}

std::optional<bool> bannerBoolean(const QVariant& value)
{
    if ( value.metaType().id() == QMetaType::Bool )
    {
        return value.toBool();
    }
    if ( value.metaType().id() == QMetaType::QString )
    {
        const QString text = value.toString();
        if ( text == QStringLiteral("true") )
        {
            return true;
        }
        if ( text == QStringLiteral("false") )
        {
            return false;
        }
    }
    return std::nullopt;
}

std::optional<QColor> bannerColor(const QVariant& value)
{
    if ( value.metaType().id() != QMetaType::QString )
    {
        return std::nullopt;
    }
    const QString text = value.toString();
    if ( text.size() != 9 || text.front() != QLatin1Char('#') )
    {
        return std::nullopt;
    }
    for ( int index = 1; index < text.size(); ++index )
    {
        const QChar character = text.at(index);
        const bool hexadecimal = (character >= QLatin1Char('0')
                && character <= QLatin1Char('9'))
            || (character >= QLatin1Char('a') && character <= QLatin1Char('f'))
            || (character >= QLatin1Char('A') && character <= QLatin1Char('F'));
        if ( !hexadecimal )
        {
            return std::nullopt;
        }
    }
    const QColor color(text);
    if ( !color.isValid() )
    {
        return std::nullopt;
    }
    return color;
}

bool bannerDurationIsInRange(const int durationMs) noexcept
{
    return durationMs >= 100 && durationMs <= 30000;
}

QString viewportBannerSettingKey(
    const std::size_t index, const QString& field)
{
    return QStringLiteral("viewport/banners/%1/%2")
        .arg(QString::fromLatin1(kViewportBannerKindIds[index]), field);
}

} // namespace

ViewportBannerSettings::ViewportBannerSettings()
{
    const QColor ordinaryText(235, 236, 240, 255);
    const QColor ordinaryBackground(18, 18, 22, 200);
    const QColor transparentBorder(0, 0, 0, 0);
    const QColor errorText(255, 120, 120, 255);
    const QColor errorBackground(40, 12, 12, 210);
    const QColor cleaveText(248, 249, 252, 250);
    const QColor cleaveBackground(18, 21, 27, 232);

    styles[static_cast<std::size_t>(ViewportBannerKind::general)] = {
        2000, true, ordinaryText, ordinaryBackground, transparentBorder };
    styles[static_cast<std::size_t>(ViewportBannerKind::projection)] = {
        2500, false, ordinaryText, ordinaryBackground, transparentBorder };
    styles[static_cast<std::size_t>(ViewportBannerKind::flySpeed)] = {
        900, false, ordinaryText, ordinaryBackground, transparentBorder };
    styles[static_cast<std::size_t>(ViewportBannerKind::error)] = {
        3000, true, errorText, errorBackground, transparentBorder };
    styles[static_cast<std::size_t>(ViewportBannerKind::cleaveRotate)] = {
        400, false, cleaveText, cleaveBackground, QColor(126, 190, 255, 235) };
    styles[static_cast<std::size_t>(ViewportBannerKind::cleaveScale)] = {
        400, false, cleaveText, cleaveBackground, QColor(143, 220, 170, 235) };
    styles[static_cast<std::size_t>(ViewportBannerKind::placementYaw)] = {
        900, false, ordinaryText, ordinaryBackground, transparentBorder };
}

bool validateViewportBannerSettings(
    const ViewportBannerSettings& settings, QString* const error)
{
    if ( error != nullptr )
    {
        error->clear();
    }

    for ( std::size_t index = 0; index < settings.styles.size(); ++index )
    {
        const ViewportBannerStyle& style = settings.styles[index];
        const QString label = QString::fromLatin1(kViewportBannerKindLabels[index]);
        if ( !bannerDurationIsInRange(style.hideAfterMs) )
        {
            if ( error != nullptr )
            {
                *error = QStringLiteral("%1 hide duration must be between 100 and 30000 ms.")
                    .arg(label);
            }
            return false;
        }
        const bool messageDurationAllowed =
            index == static_cast<std::size_t>(ViewportBannerKind::general)
            || index == static_cast<std::size_t>(ViewportBannerKind::error);
        if ( style.useMessageDuration && !messageDurationAllowed )
        {
            if ( error != nullptr )
            {
                *error = QStringLiteral("%1 cannot use the message duration.").arg(label);
            }
            return false;
        }
        if ( !style.textColor.isValid() || !style.backgroundColor.isValid()
            || !style.borderColor.isValid() )
        {
            if ( error != nullptr )
            {
                *error = QStringLiteral("%1 colors must be valid.").arg(label);
            }
            return false;
        }
    }
    return true;
}

ViewportBannerSettings loadViewportBannerSettings(const QSettings& settings)
{
    ViewportBannerSettings result{};
    for ( std::size_t index = 0; index < result.styles.size(); ++index )
    {
        ViewportBannerStyle& style = result.styles[index];
        const auto duration = navigationInteger(settings.value(
            viewportBannerSettingKey(index, QStringLiteral("hide_after_ms"))));
        if ( duration && bannerDurationIsInRange(*duration) )
        {
            style.hideAfterMs = *duration;
        }

        const auto messageDuration = bannerBoolean(settings.value(
            viewportBannerSettingKey(index, QStringLiteral("use_message_duration"))));
        const bool messageDurationAllowed =
            index == static_cast<std::size_t>(ViewportBannerKind::general)
            || index == static_cast<std::size_t>(ViewportBannerKind::error);
        if ( messageDuration && (!*messageDuration || messageDurationAllowed) )
        {
            style.useMessageDuration = *messageDuration;
        }

        const auto textColor = bannerColor(settings.value(
            viewportBannerSettingKey(index, QStringLiteral("text_color"))));
        if ( textColor )
        {
            style.textColor = *textColor;
        }
        const auto backgroundColor = bannerColor(settings.value(
            viewportBannerSettingKey(index, QStringLiteral("background_color"))));
        if ( backgroundColor )
        {
            style.backgroundColor = *backgroundColor;
        }
        const auto borderColor = bannerColor(settings.value(
            viewportBannerSettingKey(index, QStringLiteral("border_color"))));
        if ( borderColor )
        {
            style.borderColor = *borderColor;
        }
    }
    return result;
}

bool saveViewportBannerSettings(
    QSettings& settings, const ViewportBannerSettings& banners)
{
    if ( settings.status() != QSettings::NoError
        || !validateViewportBannerSettings(banners) )
    {
        return false;
    }

    const bool fallbacksEnabled = settings.fallbacksEnabled();
    settings.setFallbacksEnabled(false);
    const QStringList keysBefore = settings.allKeys();
    QSettings::SettingsMap valuesBefore{};
    for ( const QString& key : keysBefore )
    {
        valuesBefore.insert(key, settings.value(key));
    }
    settings.setFallbacksEnabled(fallbacksEnabled);
    if ( settings.status() != QSettings::NoError )
    {
        return false;
    }

    for ( std::size_t index = 0; index < banners.styles.size(); ++index )
    {
        const ViewportBannerStyle& style = banners.styles[index];
        settings.setValue(
            viewportBannerSettingKey(index, QStringLiteral("hide_after_ms")),
            style.hideAfterMs);
        settings.setValue(
            viewportBannerSettingKey(index, QStringLiteral("use_message_duration")),
            style.useMessageDuration);
        settings.setValue(
            viewportBannerSettingKey(index, QStringLiteral("text_color")),
            style.textColor.name(QColor::HexArgb));
        settings.setValue(
            viewportBannerSettingKey(index, QStringLiteral("background_color")),
            style.backgroundColor.name(QColor::HexArgb));
        settings.setValue(
            viewportBannerSettingKey(index, QStringLiteral("border_color")),
            style.borderColor.name(QColor::HexArgb));
    }
    settings.sync();
    if ( settings.status() == QSettings::NoError )
    {
        return true;
    }

    const bool failedSyncFallbacksEnabled = settings.fallbacksEnabled();
    settings.setFallbacksEnabled(false);
    const QStringList keysAfter = settings.allKeys();
    for ( const QString& key : keysAfter )
    {
        if ( !valuesBefore.contains(key) )
        {
            settings.remove(key);
        }
    }
    for ( auto value = valuesBefore.constBegin(); value != valuesBefore.constEnd(); ++value )
    {
        settings.setValue(value.key(), value.value());
    }
    settings.setFallbacksEnabled(failedSyncFallbacksEnabled);
    return false;
}

int viewportBannerDurationMs(
    const ViewportBannerSettings& settings, const ViewportBannerKind kind,
    const int messageDurationMs)
{
    const std::size_t index = static_cast<std::size_t>(kind);
    if ( index >= kViewportBannerKindCount )
    {
        return 2000;
    }

    const ViewportBannerStyle& style = settings.styles[index];
    int fallbackDuration = style.hideAfterMs;
    if ( !bannerDurationIsInRange(fallbackDuration) )
    {
        static const ViewportBannerSettings defaults{};
        fallbackDuration = defaults.styles[index].hideAfterMs;
    }
    const bool messageDurationAllowed =
        kind == ViewportBannerKind::general || kind == ViewportBannerKind::error;
    if ( messageDurationAllowed && style.useMessageDuration && messageDurationMs > 0 )
    {
        return std::max(1, messageDurationMs);
    }
    return fallbackDuration;
}

bool validateViewportNavigationSettings(
    const ViewportNavigationSettings& settings, QString* const error)
{
    if ( error )
    {
        error->clear();
    }
    for ( std::size_t index = 0; index < settings.mouse.size(); ++index )
    {
        const ViewportMouseBinding& binding = settings.mouse[index];
        if ( !validViewportMouseButton(binding.button)
            || (int(binding.modifiers) & ~int(kViewportNavigationModifierMask)) != 0 )
        {
            if ( error )
            {
                *error = QStringLiteral("%1 has an unsupported mouse button or modifier.")
                    .arg(QString::fromLatin1(kViewportMouseActionLabels[index]));
            }
            return false;
        }
        if ( binding.button == Qt::NoButton )
        {
            continue;
        }
        for ( std::size_t previous = 0; previous < index; ++previous )
        {
            if ( binding == settings.mouse[previous] )
            {
                if ( error )
                {
                    *error = QStringLiteral("%1 and %2 use the same mouse button and modifier combination.")
                        .arg(QString::fromLatin1(kViewportMouseActionLabels[previous]),
                            QString::fromLatin1(kViewportMouseActionLabels[index]));
                }
                return false;
            }
        }
    }
    if ( (int(settings.zoom.modifiers | settings.flySpeed.modifiers)
        & ~int(kViewportNavigationModifierMask)) != 0 )
    {
        if ( error )
        {
            *error = QStringLiteral("Wheel controls have an unsupported modifier.");
        }
        return false;
    }
    return true;
}

ViewportNavigationSettings loadViewportNavigationSettings(const QSettings& settings)
{
    ViewportNavigationSettings result{};
    for ( std::size_t index = 0; index < result.mouse.size(); ++index )
    {
        ViewportMouseBinding& binding = result.mouse[index];
        const QString prefix = QStringLiteral("viewport/mouse/%1")
            .arg(QString::fromLatin1(kViewportMouseActionIds[index]));
        const auto button = navigationInteger(settings.value(prefix + QStringLiteral("_button"), int(binding.button)));
        if ( button && validViewportMouseButton(static_cast<Qt::MouseButton>(*button)) )
        {
            binding.button = static_cast<Qt::MouseButton>(*button);
        }
        binding.modifiers = navigationModifiers(
            settings.value(prefix + QStringLiteral("_modifiers"), int(binding.modifiers)),
            binding.modifiers);
    }
    const auto loadWheel = [&settings](const char* const id, ViewportWheelBinding& binding)
    {
        const QString prefix = QStringLiteral("viewport/mouse/%1").arg(QString::fromLatin1(id));
        const auto modifiers = navigationInteger(settings.value(prefix + QStringLiteral("_modifiers"), 0));
        if ( modifiers && *modifiers == -1 )
        {
            binding.enabled = false;
        }
        else
        {
            binding.modifiers = navigationModifiers(
                settings.value(prefix + QStringLiteral("_modifiers"), 0), Qt::NoModifier);
        }
        binding.inverted = navigationBoolean(
            settings.value(prefix + QStringLiteral("_inverted"), false), false);
    };
    loadWheel("zoom", result.zoom);
    loadWheel("fly_speed", result.flySpeed);
    result.invertHorizontal = navigationBoolean(
        settings.value("viewport/mouse/invert_horizontal", false), false);
    result.invertVertical = navigationBoolean(
        settings.value("viewport/mouse/invert_vertical", false), false);
    // A partially damaged map must never leave two actions sharing one chord.
    return validateViewportNavigationSettings(result) ? result : ViewportNavigationSettings{};
}

bool saveViewportNavigationSettings(
    QSettings& settings, const ViewportNavigationSettings& navigation)
{
    if ( !validateViewportNavigationSettings(navigation) )
    {
        return false;
    }
    for ( std::size_t index = 0; index < navigation.mouse.size(); ++index )
    {
        const QString prefix = QStringLiteral("viewport/mouse/%1")
            .arg(QString::fromLatin1(kViewportMouseActionIds[index]));
        settings.setValue(prefix + QStringLiteral("_button"), int(navigation.mouse[index].button));
        settings.setValue(prefix + QStringLiteral("_modifiers"), int(navigation.mouse[index].modifiers));
    }
    const auto saveWheel = [&settings](const char* const id, const ViewportWheelBinding& binding)
    {
        const QString prefix = QStringLiteral("viewport/mouse/%1").arg(QString::fromLatin1(id));
        settings.setValue(prefix + QStringLiteral("_modifiers"), binding.enabled ? int(binding.modifiers) : -1);
        settings.setValue(prefix + QStringLiteral("_inverted"), binding.inverted);
    };
    saveWheel("zoom", navigation.zoom);
    saveWheel("fly_speed", navigation.flySpeed);
    settings.setValue("viewport/mouse/invert_horizontal", navigation.invertHorizontal);
    settings.setValue("viewport/mouse/invert_vertical", navigation.invertVertical);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

ViewportMouseAction viewportMouseActionForPress(
    const ViewportNavigationSettings& settings, const Qt::MouseButton button,
    const Qt::KeyboardModifiers modifiers) noexcept
{
    if ( button == Qt::NoButton )
    {
        return ViewportMouseAction::none;
    }
    Qt::KeyboardModifiers reserved{};
    for ( const ViewportMouseBinding& binding : settings.mouse )
    {
        if ( binding.button == button )
        {
            reserved |= binding.modifiers;
        }
    }
    const Qt::KeyboardModifiers held = modifiers & reserved;
    for ( std::size_t index = 0; index < settings.mouse.size(); ++index )
    {
        if ( settings.mouse[index].button == button && settings.mouse[index].modifiers == held )
        {
            return static_cast<ViewportMouseAction>(index);
        }
    }
    return ViewportMouseAction::none;
}

bool viewportMouseActionIsCameraDrag(const ViewportMouseAction action) noexcept
{
    return action == ViewportMouseAction::orbit || action == ViewportMouseAction::pan
        || action == ViewportMouseAction::dolly || action == ViewportMouseAction::fastDolly;
}

bool viewportWheelBindingMatches(
    const ViewportWheelBinding& binding, const Qt::KeyboardModifiers modifiers) noexcept
{
    return binding.enabled && (modifiers & binding.modifiers) == binding.modifiers;
}

UiDecimalSeparator loadUiDecimalSeparator()
{
    const QSettings settings = settingsStore();
    return loadUiDecimalSeparator(settings);
}

UiDecimalSeparator loadUiDecimalSeparator(const QSettings& settings)
{
    bool ok = false;
    const int value = settings.value(QString::fromLatin1(UiDecimalSeparatorSetting), 0)
        .toString().toInt(&ok);
    if ( ok && value >= static_cast<int>(UiDecimalSeparator::dot)
        && value <= static_cast<int>(UiDecimalSeparator::system) )
    {
        return static_cast<UiDecimalSeparator>(value);
    }
    return UiDecimalSeparator::dot;
}

void saveUiDecimalSeparator(const UiDecimalSeparator separator)
{
    QSettings settings = settingsStore();
    saveUiDecimalSeparator(settings, separator);
    settings.sync();
}

void saveUiDecimalSeparator(QSettings& settings, const UiDecimalSeparator separator)
{
    const UiDecimalSeparator valid = separator == UiDecimalSeparator::comma
            || separator == UiDecimalSeparator::system
        ? separator : UiDecimalSeparator::dot;
    settings.setValue(QString::fromLatin1(UiDecimalSeparatorSetting), static_cast<int>(valid));
}

LegacyUvPromotionPolicy loadLegacyUvPromotionPolicy()
{
    const QSettings settings = settingsStore();
    return loadLegacyUvPromotionPolicy(settings);
}

LegacyUvPromotionPolicy loadLegacyUvPromotionPolicy(
    const QSettings& settings)
{
    const int value = settings.value(
        QString::fromLatin1(LegacyUvPromotionPolicySetting),
        static_cast<int>(
            LegacyUvPromotionPolicy::normalizeAutomatically)).toInt();
    switch ( static_cast<LegacyUvPromotionPolicy>(value) )
    {
        case LegacyUvPromotionPolicy::normalizeAutomatically:
        case LegacyUvPromotionPolicy::askBeforeNormalizing:
        case LegacyUvPromotionPolicy::keepPixelSpace:
            return static_cast<LegacyUvPromotionPolicy>(value);
    }
    return LegacyUvPromotionPolicy::normalizeAutomatically;
}

void saveLegacyUvPromotionPolicy(const LegacyUvPromotionPolicy policy)
{
    QSettings settings = settingsStore();
    saveLegacyUvPromotionPolicy(settings, policy);
    settings.sync();
}

void saveLegacyUvPromotionPolicy(
    QSettings& settings,
    const LegacyUvPromotionPolicy policy)
{
    settings.setValue(
        QString::fromLatin1(LegacyUvPromotionPolicySetting),
        static_cast<int>(policy));
}

QStringList loadRecentLevels()
{
    QSettings settings = settingsStore();
    return settings.value("app/recent_levels").toStringList();
}

void saveRecentLevels(const QStringList& files)
{
    QSettings settings = settingsStore();
    settings.setValue("app/recent_levels", files);
}

QStringList loadPinnedRecentLevels()
{
    QSettings settings = settingsStore();
    return settings.value("app/pinned_recent_levels").toStringList();
}

void savePinnedRecentLevels(const QStringList& files)
{
    QSettings settings = settingsStore();
    settings.setValue("app/pinned_recent_levels", files);
}

QString loadLastOpenLevelDir()
{
    QSettings settings = settingsStore();
    return settings.value("app/last_open_level_dir").toString().trimmed();
}

void saveLastOpenLevelDir(const QString& dir)
{
    QSettings settings = settingsStore();
    settings.setValue("app/last_open_level_dir", dir.trimmed());
}

namespace
{

QString fileDialogHistoryKey(const FileDialogHistoryGroup group)
{
    switch (group)
    {
        case FileDialogHistoryGroup::openSave:
            return QStringLiteral("dialogs/last_open_save_directory");
        case FileDialogHistoryGroup::import_:
            return QStringLiteral("dialogs/last_import_directory");
        case FileDialogHistoryGroup::export_:
            return QStringLiteral("dialogs/last_export_directory");
    }
    return QStringLiteral("dialogs/last_open_save_directory");
}

QString cleanAbsoluteDirectory(const QString& path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty())
    {
        return {};
    }
    return QDir::cleanPath(QFileInfo(trimmed).absoluteFilePath());
}

} // namespace

QString fileDialogStartLocation(
    const FileDialogHistoryGroup group,
    const QString& suggestedPath)
{
    QSettings settings = settingsStore();
    QString remembered = settings.value(fileDialogHistoryKey(group))
                             .toString().trimmed();
    if (remembered.isEmpty())
    {
        remembered = settings.value(QStringLiteral("dialogs/last_any_directory"))
                         .toString().trimmed();
    }
    if (remembered.isEmpty())
    {
        remembered = loadLastOpenLevelDir();
    }

    const QString suggested = suggestedPath.trimmed();
    if (remembered.isEmpty())
    {
        return suggested;
    }
    remembered = cleanAbsoluteDirectory(remembered);
    if (suggested.isEmpty())
    {
        return remembered;
    }

    const QFileInfo suggestedInfo(suggested);
    if (suggestedInfo.isDir()
        || suggested.endsWith(QLatin1Char('/'))
        || suggested.endsWith(QLatin1Char('\\'))
        || suggestedInfo.fileName().isEmpty())
    {
        return remembered;
    }
    return QDir(remembered).filePath(suggestedInfo.fileName());
}

void rememberFileDialogSelection(
    const FileDialogHistoryGroup group,
    const QString& selectedPath,
    const bool selectedDirectory)
{
    const QString trimmed = selectedPath.trimmed();
    if (trimmed.isEmpty())
    {
        return;
    }

    const QFileInfo selectedInfo(trimmed);
    const QString directory = cleanAbsoluteDirectory(
        selectedDirectory ? trimmed : selectedInfo.absolutePath());
    if (directory.isEmpty())
    {
        return;
    }

    QSettings settings = settingsStore();
    settings.setValue(fileDialogHistoryKey(group), directory);
    settings.setValue(QStringLiteral("dialogs/last_any_directory"), directory);
    if (group == FileDialogHistoryGroup::openSave)
    {
        settings.setValue(QStringLiteral("app/last_open_level_dir"), directory);
    }
}

} // namespace sed
