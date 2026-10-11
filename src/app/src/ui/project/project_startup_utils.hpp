#pragma once

#include "sed/assets/lev.hpp"
#include "sed/level.hpp"
#include "sed/level/uv.hpp"
#include "ui/resources/project_resource_settings.hpp"

#include <filesystem>
#include <optional>

#include <QString>
#include <QStringList>
#include <QRect>

class QSettings;

namespace sed::ui::project
{

/** Live-Pascal-compatible defaults shown before importing a Dark Forces LEV. */
assets::LevLoadOptions defaultLevImportOptions();

/** Returns whether a Dark Forces-to-Sith coordinate divisor is safe to use. */
bool isValidLevImportScale(double scale) noexcept;

/** Applies the format/header defaults used by the real New Project flow. */
void applyNewProjectTypeDefaults(Level& level, ProjectType kind);

/** Editable NDY/JKL spare capacities assigned to a newly created project. */
LevelExportBufferExtras defaultNewProjectExportBufferExtras(ProjectType kind) noexcept;

/** Returns the user-configured new-project extras, falling back field-by-field to the built-in defaults. */
LevelExportBufferExtras configuredNewProjectExportBufferExtras(
    const QSettings& settings,
    ProjectType kind);

/** Stores only fields that differ from the built-in project defaults. */
void writeConfiguredNewProjectExportBufferExtras(
    QSettings& settings,
    ProjectType kind,
    const LevelExportBufferExtras& extras);

/** Canonical fallback dimensions of the built-in starter material. */
inline constexpr level::UvTextureDimensions DefaultStarterMaterialDimensions{
    64.0,
    64.0,
};

/** Seeds the deterministic one-room, one-player, one-light starter level. */
void seedDefaultNewProjectLevel(
    Level& level,
    level::UvTextureDimensions materialDimensions);

/**
 * Human-readable project name used by startup, menus, and preferences.
 */
QString projectTypeLabel(ProjectType kind);

/** Returns whether the project still exposes the legacy JED editor format as a save target. */
bool supportsJedLevelExport(ProjectType kind) noexcept;

/** Returns the normal runtime level suffix for the project, including the leading dot. */
QString nativeRuntimeLevelSuffix(ProjectType kind);

/** Returns the native Sith Editor Save/Save As filename filter. */
QString levelSaveDialogFilter(ProjectType kind);

/** Returns the project-aware engine-format Export filename filter. */
QString levelExportDialogFilter(ProjectType kind);

/** Returns the Open filter whose first entry is every supported level/geometry format. */
QString levelOpenDialogFilter();

/** Returns the level-designer-facing explanation for a mismatched path. */
QString projectInstallPathMismatchMessage(
    ProjectType expectedKind,
    const std::filesystem::path& installDir,
    const ui::resources::ProjectInstallPathValidation& validation);

/** Returns whether Open accepts this level/editor/legacy geometry suffix. */
bool isSupportedLevelOpenPath(const std::filesystem::path& path);

/** Returns whether an existing path is a native Sith Editor document target. */
bool isNativeLevelSavePath(const std::filesystem::path& path, ProjectType kind);

/** Returns whether a path is a valid engine-format export target for the project. */
bool isEngineLevelExportPath(const std::filesystem::path& path, ProjectType kind);

/**
 * Returns true when a recent-level list contains the path, ignoring case.
 */
bool recentLevelListContainsPath(const QStringList& paths, const QString& needle);

/**
 * Removes all case-insensitive matches for one path from a recent-level list.
 */
void removeRecentLevelPath(QStringList& paths, const QString& needle);

/**
 * Drops empty, duplicate, or no-longer-recent pins while preserving pin order.
 */
QStringList sanitizedPinnedRecentLevels(const QStringList& recentLevels, const QStringList& pinnedLevels);

/**
 * Places pinned recents first, then unpinned recents in the original last-used order.
 */
QStringList recentLevelsWithPinnedFirst(const QStringList& recentLevels, const QStringList& pinnedLevels);

/**
 * Computes the persisted pinned list after a pin checkbox change.
 *
 * New pins are appended after existing pins so the user-controlled pinned
 * group keeps its relative order. Unpinned entries disappear from the pinned
 * group and will naturally fall back into the recent last-used order.
 */
QStringList pinnedRecentLevelsAfterToggle(
    const QStringList& recentLevels,
    const QStringList& pinnedLevels,
    const QString& toggledPath,
    bool pinned);

/**
 * Centers the startup window inside one screen's available area and leaves a
 * small frame margin so native caption controls cannot be clipped at high DPI.
 */
QRect startupDialogGeometryForAvailableScreen(const QRect& availableGeometry) noexcept;

/**
 * Returns true when a configured or detected install directory is present on disk.
 */
bool installDirExists(const std::optional<std::filesystem::path>& dir) noexcept;

/**
 * Summarizes configured and auto-detected install paths for one project type.
 *
 * The startup dialog uses this object to keep path validation, status text, and
 * button enablement together instead of scattering those checks through UI code.
 */
class ProjectInstallStatus final
{
public:
    ProjectInstallStatus(
        ProjectType kind,
        std::optional<std::filesystem::path> configured,
        std::optional<std::filesystem::path> detected);

    ProjectType kind() const noexcept { return kind_; }
    const std::optional<std::filesystem::path>& configured() const noexcept { return configured_; }
    const std::optional<std::filesystem::path>& detected() const noexcept { return detected_; }

    bool configuredValid() const noexcept { return configuredValid_; }
    bool detectedValid() const noexcept { return detectedValid_; }
    bool canCreateProject() const noexcept { return configuredValid_ || detectedValid_; }

    QString statusObjectName() const;
    QString statusText() const;

private:
    ProjectType kind_{ ProjectType::JKDF2 };
    std::optional<std::filesystem::path> configured_{};
    std::optional<std::filesystem::path> detected_{};
    ui::resources::ProjectInstallPathValidation configuredPathValidation_{};
    ui::resources::ProjectInstallPathValidation detectedPathValidation_{};
    bool configuredValid_{};
    bool detectedValid_{};
};

} // namespace sed::ui::project
