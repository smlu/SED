#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

#include <QColor>
#include <QString>
#include <QStringList>
#include <Qt>

class QSettings;

namespace sed
{

std::filesystem::path appSettingsIniPath();
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
void setAppSettingsIniPathOverrideForTest(const std::filesystem::path& path);
void setAppSettingsIniPathBaselineOverrideForTest(const std::filesystem::path& path);
bool appSettingsIniPathIsOverriddenForTest();
#endif

enum class AppThemeKind : std::uint8_t
{
    viewport,
    cog,
};

// Stores user-created viewport/app themes in data/themes and COG themes in
// data/themes/cog beside portable settings when atomic writes are available.
// Installations that cannot write there use the matching per-user locations.
// Search paths accept each format in its default and alternate grouping.
std::filesystem::path appThemeDirectoryPath(
    AppThemeKind kind,
    QString* errorMessage = nullptr);
std::filesystem::path appThemePerUserDataDirectory();
#ifdef SED_APP_THEME_STORAGE_TEST_ACCESS
void setAppThemeFallbackDirectoryOverrideForTest(
    const std::filesystem::path& path);
#endif
std::filesystem::path appThemeDirectoryPath(
    AppThemeKind kind,
    const std::filesystem::path& settingsFilePath,
    const std::filesystem::path& perUserApplicationDataDirectory,
    QString* errorMessage = nullptr);
std::vector<std::filesystem::path> appThemeDirectorySearchPaths(
    AppThemeKind kind,
    const std::filesystem::path& settingsFilePath,
    const std::filesystem::path& perUserApplicationDataDirectory);

// Prefer portable recovery storage beside sed.ini, but fall back to the
// per-user application-data directory when the executable directory cannot
// support atomic writes (for example, an install under Program Files).
std::filesystem::path appRecoveryDirectoryPath();
// Includes the current writable destination plus every durable/legacy
// fallback location that an earlier launch may have selected.
std::vector<std::filesystem::path> appRecoverySearchDirectoryPaths();
QSettings settingsStore();

enum class ViewportMouseAction : std::uint8_t
{
    pick,
    flyLook,
    orbit,
    pan,
    dolly,
    fastDolly,
    count,
    none = 255,
};

inline constexpr std::array<const char*, 6> kViewportMouseActionIds{
    "pick", "fly_look", "orbit", "pan", "dolly", "fast_dolly",
};
inline constexpr std::array<const char*, 6> kViewportMouseActionLabels{
    "Pick and edit", "Fly look", "Orbit", "Pan", "Dolly / zoom", "Faster dolly / zoom",
};

struct ViewportMouseBinding
{
    Qt::MouseButton button{ Qt::NoButton };
    Qt::KeyboardModifiers modifiers{};
    bool operator==(const ViewportMouseBinding&) const = default;
};

struct ViewportWheelBinding
{
    bool enabled{ true };
    Qt::KeyboardModifiers modifiers{};
    bool inverted{};
    bool operator==(const ViewportWheelBinding&) const = default;
};

/** Application preferences only; native document camera state is independent. */
struct ViewportNavigationSettings
{
    std::array<ViewportMouseBinding, 6> mouse{
        ViewportMouseBinding{ Qt::LeftButton, Qt::NoModifier },
        ViewportMouseBinding{ Qt::RightButton, Qt::NoModifier },
        ViewportMouseBinding{ Qt::MiddleButton, Qt::NoModifier },
        ViewportMouseBinding{ Qt::MiddleButton, Qt::ShiftModifier },
        ViewportMouseBinding{ Qt::MiddleButton, Qt::ControlModifier },
        ViewportMouseBinding{ Qt::MiddleButton, Qt::ControlModifier | Qt::ShiftModifier },
    };
    ViewportWheelBinding zoom{};
    ViewportWheelBinding flySpeed{};
    bool invertHorizontal{};
    bool invertVertical{};
    bool operator==(const ViewportNavigationSettings&) const = default;
};

bool validateViewportNavigationSettings(
    const ViewportNavigationSettings& settings, QString* error = nullptr);
ViewportNavigationSettings loadViewportNavigationSettings(const QSettings& settings);
bool saveViewportNavigationSettings(
    QSettings& settings, const ViewportNavigationSettings& navigation);
/** Only modifiers reserved on this physical button participate in routing. */
ViewportMouseAction viewportMouseActionForPress(
    const ViewportNavigationSettings& settings,
    Qt::MouseButton button, Qt::KeyboardModifiers modifiers) noexcept;
bool viewportMouseActionIsCameraDrag(ViewportMouseAction action) noexcept;
bool viewportWheelBindingMatches(
    const ViewportWheelBinding& binding, Qt::KeyboardModifiers modifiers) noexcept;

/** Application preferences for transient messages painted in the viewport. */
enum class ViewportBannerKind : std::uint8_t
{
    general,
    projection,
    flySpeed,
    error,
    cleaveRotate,
    cleaveScale,
    placementYaw,
    count,
};

inline constexpr std::size_t kViewportBannerKindCount =
    static_cast<std::size_t>(ViewportBannerKind::count);
inline constexpr std::array<const char*, kViewportBannerKindCount>
    kViewportBannerKindIds{
        "general", "projection", "fly_speed", "error", "cleave_rotate",
        "cleave_scale", "placement_yaw",
    };
inline constexpr std::array<const char*, kViewportBannerKindCount>
    kViewportBannerKindLabels{
        "Messages", "Projection", "Fly speed", "Errors", "Cleave rotation",
        "Cleave scale", "Placement rotation",
    };

struct ViewportBannerStyle
{
    int hideAfterMs{};
    bool useMessageDuration{};
    QColor textColor{};
    QColor backgroundColor{};
    QColor borderColor{};
    bool operator==(const ViewportBannerStyle&) const = default;
};

struct ViewportBannerSettings
{
    std::array<ViewportBannerStyle, kViewportBannerKindCount> styles{};

    ViewportBannerSettings();
    bool operator==(const ViewportBannerSettings&) const = default;
};

bool validateViewportBannerSettings(
    const ViewportBannerSettings& settings, QString* error = nullptr);
ViewportBannerSettings loadViewportBannerSettings(const QSettings& settings);
bool saveViewportBannerSettings(
    QSettings& settings, const ViewportBannerSettings& banners);
int viewportBannerDurationMs(
    const ViewportBannerSettings& settings, ViewportBannerKind kind,
    int messageDurationMs);

/** Decimal punctuation for UI numbers only; serialized data remains canonical. */
enum class UiDecimalSeparator : std::uint8_t
{
    dot,
    comma,
    system,
};

inline constexpr const char* UiDecimalSeparatorSetting = "ui/decimal_separator";

UiDecimalSeparator loadUiDecimalSeparator();
UiDecimalSeparator loadUiDecimalSeparator(const QSettings& settings);
void saveUiDecimalSeparator(UiDecimalSeparator separator);
void saveUiDecimalSeparator(QSettings& settings, UiDecimalSeparator separator);

QStringList loadRecentLevels();
void saveRecentLevels(const QStringList& files);

QStringList loadPinnedRecentLevels();
void savePinnedRecentLevels(const QStringList& files);

QString loadLastOpenLevelDir();
void saveLastOpenLevelDir(const QString& dir);

enum class FileDialogHistoryGroup : std::uint8_t
{
    openSave,
    import_,
    export_,
};

/** Policy for admitting legacy native SED pixel-space UVs (V1 or V4). */
enum class LegacyUvPromotionPolicy : std::uint8_t
{
    normalizeAutomatically,
    askBeforeNormalizing,
    keepPixelSpace,
};

inline constexpr const char* LegacyUvPromotionPolicySetting =
    "save/legacy_uv_promotion_policy";

LegacyUvPromotionPolicy loadLegacyUvPromotionPolicy();
LegacyUvPromotionPolicy loadLegacyUvPromotionPolicy(const QSettings& settings);
void saveLegacyUvPromotionPolicy(LegacyUvPromotionPolicy policy);
void saveLegacyUvPromotionPolicy(
    QSettings& settings, LegacyUvPromotionPolicy policy);

QString fileDialogStartLocation(
    FileDialogHistoryGroup group,
    const QString& suggestedPath = {});
void rememberFileDialogSelection(
    FileDialogHistoryGroup group,
    const QString& selectedPath,
    bool selectedDirectory = false);

} // namespace sed
