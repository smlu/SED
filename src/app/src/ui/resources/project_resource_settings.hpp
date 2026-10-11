#pragma once

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include <QString>

#include "sed/level.hpp"

namespace sed::assets
{

class ResourceLocator;

} // namespace sed::assets

namespace sed::ui::resources
{

struct ResourceSettings
{
    std::vector<std::filesystem::path> roots{};
    std::vector<std::filesystem::path> gobs{};
};

struct ResourceSettingsMergeResult
{
    ResourceSettings settings{};
    bool changed{};
};

enum class ProjectInstallPathMatch
{
    plausible,
    matching,
    mismatched,
};

/**
 * Result of checking an install/resource directory for strong supported-game
 * signatures. Signature-free custom layouts deliberately remain plausible.
 */
struct ProjectInstallPathValidation
{
    ProjectInstallPathMatch match{ ProjectInstallPathMatch::plausible };
    std::optional<ProjectType> detectedKind{};
    std::filesystem::path signaturePath{};

    bool canUse() const noexcept
    {
        return match != ProjectInstallPathMatch::mismatched;
    }
};

/// Returns the stable lowercase key used for project-specific settings groups.
std::optional<QString> settingsProjectName(ProjectType kind);

/// Parses the persisted project setting value, accepting surrounding whitespace and mixed case.
std::optional<ProjectType> parseProjectTypeSettingValue(const QString& value) noexcept;

/// Checks whether a lowercase archive extension is valid for an optional project filter.
bool isAllowedArchiveExt(std::string_view extLower, std::optional<ProjectType> kindFilter = std::nullopt) noexcept;

/// Guards persisted or cast ProjectType values before they drive save/export behavior.
bool isValidProjectType(ProjectType kind) noexcept;

/**
 * Detects only unambiguous cross-project install mistakes. Missing optional
 * archives, custom resource layouts, and mixed shared roots remain allowed.
 */
ProjectInstallPathValidation validateProjectInstallPath(
    ProjectType expectedKind,
    const std::filesystem::path& installDir) noexcept;

/// Returns path when it exists without surfacing filesystem errors to callers.
std::optional<std::filesystem::path> findFileIfExists(const std::filesystem::path& path) noexcept;

/// Builds the default roots and archives expected from a game install directory.
ResourceSettings findDefaultProjectResources(ProjectType kind, const std::filesystem::path& installDir);

/// Adds detected roots and archives to existing settings, preserving existing order.
ResourceSettingsMergeResult mergeDetectedResourceSettings(
    ResourceSettings existing,
    const ResourceSettings& detected);

/// Case-insensitive path containment check used to reject resources outside an install directory.
bool pathIsUnderInstallDir(const std::filesystem::path& path, const std::filesystem::path& installDir);

/// Compares setting paths using filesystem identity when available and normalized text otherwise.
bool pathsEquivalentForSettings(const std::filesystem::path& a, const std::filesystem::path& b);

/// Ensures IJIM uses Resource/ as its primary editable asset root when that folder exists.
void putIjimResourceRootFirst(std::vector<std::filesystem::path>& roots, const std::filesystem::path& installDir);

/// Lists archive files directly inside dir, filtered by project kind and sorted by path.
std::vector<std::filesystem::path> collectGobsInDir(
    const std::filesystem::path& dir,
    std::optional<ProjectType> kindFilter);

/// Adds the edited level's parent directory as a resource root if it is not already present.
void configureResourceLocator(
    assets::ResourceLocator& locator,
    const std::filesystem::path& levelPath,
    std::optional<ProjectType> archiveKindFilter = std::nullopt);

} // namespace sed::ui::resources
