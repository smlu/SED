#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <QString>

#include "sed/level.hpp"
#include "ui/navigation/outliner_types.hpp"

namespace sed::assets
{
class ResourceLocator;
class Templates;
} // namespace sed::assets

class QSettings;

namespace sed::ui::checks
{

enum class CheckRuleGroup
{
    consistency,
    resources,
};

/** Stable identifiers used by collection, the settings dialog, and persisted preferences. */
enum class CheckRule
{
    sectorColormap,
    sectorMinimumSurfaces,
    sectorMinimumVertices,
    sectorConvexity,
    solidSurfaceFaceFlags,
    floorCollision,
    solidSurfaceGeometryMode,
    surfaceVertexLimit,
    surfaceMinimumVertices,
    surfaceTexVertexCount,
    ijimTrackVertexLimit,
    surfaceVertexReferences,
    surfaceNormal,
    surfacePlanarity,
    surfaceConvexity,
    adjoinReference,
    adjoinBackLink,
    adjoinOverlap,
    adjoinVisibility,
    sectorOverlap,
    thingFinitePosition,
    thingSectorAssignment,
    thingSectorContainment,
    thingBaseTemplate,
    cogParameterExtension,
    materialResource,
    masterCmpResource,
    sectorCmpResource,
    cmpCountLimit,
    sectorSoundResource,
    thingTemplateResource,
    thingSpriteResource,
    thingModelResource,
    cogScriptResource,
    count,
};

struct CheckRuleDefinition
{
    CheckRule rule{};
    CheckRuleGroup group{};
    std::string_view key{};
    std::string_view name{};
    std::string_view description{};
    bool enabledByDefault{ true };
};

enum class GeometryCheckPrecision
{
    GameCompatible,
    Precise,
};

struct CheckRuleSelection
{
    CheckRuleSelection() noexcept
    {
        enabled.fill(true);
    }

    bool isEnabled(CheckRule rule) const noexcept
    {
        const std::size_t index = static_cast<std::size_t>(rule);
        return index < enabled.size() && enabled[index];
    }

    void setEnabled(CheckRule rule, bool value) noexcept
    {
        const std::size_t index = static_cast<std::size_t>(rule);
        if (index < enabled.size())
        {
            enabled[index] = value;
        }
    }

    GeometryCheckPrecision geometryPrecision{GeometryCheckPrecision::GameCompatible};
    std::array<bool, static_cast<std::size_t>(CheckRule::count)> enabled{};
};

/** Returns only rules that belong to the requested tab and apply to the project. */
std::vector<CheckRuleDefinition> checkRuleDefinitions(CheckRuleGroup group, ProjectType project);

bool checkRuleAppliesToProject(CheckRule rule, ProjectType project) noexcept;

/** Defaults are enabled except for the intentionally expensive sector-overlap pass. */
CheckRuleSelection defaultCheckRuleSelection(ProjectType project) noexcept;

/** Stable per-project QSettings key for one checker rule. */
QString checkRuleSettingsKey(ProjectType project, CheckRule rule);
QString geometryCheckPrecisionSettingsKey(ProjectType project);

CheckRuleSelection loadCheckRuleSelection(QSettings& settings, ProjectType project);
void saveCheckRuleSelection(
    QSettings& settings,
    ProjectType project,
    CheckRuleGroup group,
    const CheckRuleSelection& selection);

/** Severity level for a check issue before the UI maps it to MainWindow styling. */
enum class CheckSeverity
{
    info,
    warning,
    error,
};

/** One consistency/resource issue plus optional navigation metadata. */
struct CheckIssue
{
    CheckSeverity severity{ CheckSeverity::info };
    QString message{};
    std::optional<OutlinerKind> navKind{};
    std::optional<int> navIndex{};
    std::optional<int> navSector{};
    std::optional<int> navSurface{};
};

/**
 * Runs all structural level consistency checks in deterministic display order.
 *
 * Sector-pair overlap detection is substantially more expensive than the other
 * checks and matches the legacy editor's default-off CheckOverlaps option.
 */
std::vector<CheckIssue> collectConsistencyCheckIssues(
    const Level& level,
    bool checkSectorOverlaps = false);

/** Runs only consistency rules enabled in the supplied selection. */
std::vector<CheckIssue> collectConsistencyCheckIssues(
    const Level& level,
    const CheckRuleSelection& selection);

/** Runs Thing-only consistency rules for the supplied authored indices. */
std::vector<CheckIssue> collectThingConsistencyCheckIssues(
    const Level& level,
    const CheckRuleSelection& selection,
    const std::vector<std::int32_t>& thingIndices,
    bool containmentAlreadyResolved = false);

/** Runs all resource availability checks using the currently mounted locator state. */
std::vector<CheckIssue> collectResourceCheckIssues(
    const Level& level,
    const assets::ResourceLocator& resources,
    const assets::Templates& templates);

/** Runs only resource rules enabled in the supplied selection. */
std::vector<CheckIssue> collectResourceCheckIssues(
    const Level& level,
    const assets::ResourceLocator& resources,
    const assets::Templates& templates,
    const CheckRuleSelection& selection);

/** Runs Thing-only resource rules for the supplied authored indices. */
std::vector<CheckIssue> collectThingResourceCheckIssues(
    const Level& level,
    const assets::ResourceLocator& resources,
    const assets::Templates& templates,
    const CheckRuleSelection& selection,
    const std::vector<std::int32_t>& thingIndices);

} // namespace sed::ui::checks
