#include "ui/checks/level_check_utils.hpp"
#include "ui/components/ui_decimal_format.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <QMetaType>
#include <QSettings>

#include "sed/assets/3do.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/tpl.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level/thing.hpp"
#include "sed/text.hpp"

namespace sed::ui::checks
{
namespace
{

constexpr int kIjimMaxTrackSurfaceVertices = 4;
constexpr double kLegacyNormalLengthTolerance = 1.0e-4;
constexpr double kLegacySectorOverlapPlaneTolerance = 0.01;

constexpr std::uint8_t kProjectJkdf2 = 1u << 0u;
constexpr std::uint8_t kProjectMots = 1u << 1u;
constexpr std::uint8_t kProjectIjim = 1u << 2u;
constexpr std::uint8_t kProjectClassic = kProjectJkdf2 | kProjectMots;
constexpr std::uint8_t kProjectAll = kProjectClassic | kProjectIjim;

struct CheckRuleRecord
{
    CheckRuleDefinition definition{};
    std::uint8_t projects{};
};

constexpr std::array<CheckRuleRecord, static_cast<std::size_t>(CheckRule::count)> kCheckRules{{
    {{ CheckRule::sectorColormap, CheckRuleGroup::consistency, "sector_colormap", "Sector colormap",
       "Require every sector to specify a colormap.", true }, kProjectClassic},
    {{ CheckRule::sectorMinimumSurfaces, CheckRuleGroup::consistency, "sector_minimum_surfaces", "Sector minimum surfaces",
       "Report sectors with fewer than four surfaces.", true }, kProjectAll},
    {{ CheckRule::sectorMinimumVertices, CheckRuleGroup::consistency, "sector_minimum_vertices", "Sector minimum vertices",
       "Report sectors with fewer than four vertices.", true }, kProjectAll},
    {{ CheckRule::sectorConvexity, CheckRuleGroup::consistency, "sector_convexity", "Sector convexity",
       "Require non-3DO sectors to be convex.", true }, kProjectAll},
    {{ CheckRule::solidSurfaceFaceFlags, CheckRuleGroup::consistency, "solid_surface_face_flags", "Solid-surface face flags",
       "Report translucent face flags on solid surfaces.", true }, kProjectAll},
    {{ CheckRule::floorCollision, CheckRuleGroup::consistency, "floor_collision", "Floor collision flag",
       "Require Floor surfaces to also carry Collision.", true }, kProjectAll},
    {{ CheckRule::solidSurfaceGeometryMode, CheckRuleGroup::consistency, "solid_surface_geometry_mode", "Solid-surface geometry mode",
       "Require solid surfaces to use textured geometry.", true }, kProjectAll},
    {{ CheckRule::surfaceVertexLimit, CheckRuleGroup::consistency, "surface_vertex_limit", "Surface vertex limit",
       "Enforce the project's maximum vertices per surface.", true }, kProjectAll},
    {{ CheckRule::surfaceMinimumVertices, CheckRuleGroup::consistency, "surface_minimum_vertices", "Surface minimum vertices",
       "Report surfaces with fewer than three vertices.", true }, kProjectAll},
    {{ CheckRule::surfaceTexVertexCount, CheckRuleGroup::consistency, "surface_texvertex_count", "Texture-vertex count",
       "Require paired surface vertex and texture-vertex counts.", true }, kProjectAll},
    {{ CheckRule::ijimTrackVertexLimit, CheckRuleGroup::consistency, "ijim_track_vertex_limit", "Track surface vertex limit",
       "Limit IJIM Track surfaces to four vertices.", true }, kProjectIjim},
    {{ CheckRule::surfaceVertexReferences, CheckRuleGroup::consistency, "surface_vertex_references", "Surface vertex references",
       "Report surface vertex indices outside their sector vertex array.", true }, kProjectAll},
    {{ CheckRule::surfaceNormal, CheckRuleGroup::consistency, "surface_normal", "Unit surface normal",
       "Report stored normals whose length differs from one by at least 0.0001.", true }, kProjectAll},
    {{ CheckRule::surfacePlanarity, CheckRuleGroup::consistency, "surface_planarity", "Surface planarity",
       "Report faces whose vertices do not lie in one plane.", true }, kProjectAll},
    {{ CheckRule::surfaceConvexity, CheckRuleGroup::consistency, "surface_convexity", "Surface convexity",
       "Report non-convex surface polygons.", true }, kProjectAll},
    {{ CheckRule::adjoinReference, CheckRuleGroup::consistency, "adjoin_reference", "Adjoin reference",
       "Report adjoins that reference a missing sector or surface.", true }, kProjectAll},
    {{ CheckRule::adjoinBackLink, CheckRuleGroup::consistency, "adjoin_back_link", "Adjoin back-link",
       "Require an adjoin's mirror surface to point back.", true }, kProjectAll},
    {{ CheckRule::adjoinOverlap, CheckRuleGroup::consistency, "adjoin_overlap", "Adjoin overlap",
       "Report paired adjoin surfaces that do not geometrically overlap.", true }, kProjectAll},
    {{ CheckRule::adjoinVisibility, CheckRuleGroup::consistency, "adjoin_visibility", "Adjoin visibility",
       "Require Visible for translucent or geometry-zero adjoins.", true }, kProjectAll},
    {{ CheckRule::sectorOverlap, CheckRuleGroup::consistency, "sector_overlap", "Sector overlap",
       "Run the slower pairwise check for overlapping, non-adjoined sectors.", false }, kProjectAll},
    {{ CheckRule::thingFinitePosition, CheckRuleGroup::consistency, "thing_finite_position", "Thing finite position",
       "Report Things with non-finite position coordinates.", true }, kProjectAll},
    {{ CheckRule::thingSectorAssignment, CheckRuleGroup::consistency, "thing_sector_assignment", "Thing sector assignment",
       "Require each Thing to reference an existing sector.", true }, kProjectAll},
    {{ CheckRule::thingSectorContainment, CheckRuleGroup::consistency, "thing_sector_containment", "Thing sector containment",
       "Require a Thing's position to lie inside its specified sector.", true }, kProjectAll},
    {{ CheckRule::thingBaseTemplate, CheckRuleGroup::consistency, "thing_base_template", "Placed base template",
       "Warn when a placed Thing directly uses an underscore-prefixed base template.", true }, kProjectAll},
    {{ CheckRule::cogParameterExtension, CheckRuleGroup::consistency, "cog_parameter_extension", "COG parameter extension",
       "Validate file extensions used by typed COG resource parameters.", true }, kProjectAll},
    {{ CheckRule::materialResource, CheckRuleGroup::resources, "material_resource", "Material availability",
       "Verify every referenced MAT exists in mounted resources.", true }, kProjectAll},
    {{ CheckRule::masterCmpResource, CheckRuleGroup::resources, "master_cmp_resource", "Master CMP availability",
       "Verify the level master CMP exists when specified.", true }, kProjectClassic},
    {{ CheckRule::sectorCmpResource, CheckRuleGroup::resources, "sector_cmp_resource", "Sector CMP availability",
       "Verify every referenced sector CMP exists.", true }, kProjectClassic},
    {{ CheckRule::cmpCountLimit, CheckRuleGroup::resources, "cmp_count_limit", "CMP count limit",
       "Enforce the project's limit on distinct authored CMP references.", true }, kProjectClassic},
    {{ CheckRule::sectorSoundResource, CheckRuleGroup::resources, "sector_sound_resource", "Sector sound availability",
       "Verify every referenced sector WAV exists.", true }, kProjectAll},
    {{ CheckRule::thingTemplateResource, CheckRuleGroup::resources, "thing_template_resource", "Thing template availability",
       "Verify every placed Thing names an available template.", true }, kProjectAll},
    {{ CheckRule::thingSpriteResource, CheckRuleGroup::resources, "thing_sprite_resource", "Thing sprite availability",
       "Verify inherited Thing sprite resources exist.", true }, kProjectAll},
    {{ CheckRule::thingModelResource, CheckRuleGroup::resources, "thing_model_resource", "Thing 3DO availability",
       "Verify inherited Thing 3DO model resources exist.", true }, kProjectAll},
    {{ CheckRule::cogScriptResource, CheckRuleGroup::resources, "cog_script_resource", "COG script availability",
       "Verify every placed COG names an available script.", true }, kProjectAll},
}};

std::uint8_t projectBit(const ProjectType project) noexcept
{
    switch (project)
    {
        case ProjectType::JKDF2: return kProjectJkdf2;
        case ProjectType::MOTS: return kProjectMots;
        case ProjectType::IJIM: return kProjectIjim;
    }
    return 0u;
}

const CheckRuleRecord* ruleRecord(const CheckRule rule) noexcept
{
    const std::size_t index = static_cast<std::size_t>(rule);
    return index < kCheckRules.size() ? &kCheckRules[index] : nullptr;
}

CheckIssue makeIssue(
    const CheckSeverity severity,
    QString message,
    const std::optional<OutlinerKind> navKind = std::nullopt,
    const std::optional<int> navIndex = std::nullopt,
    const std::optional<int> navSector = std::nullopt,
    const std::optional<int> navSurface = std::nullopt)
{
    CheckIssue issue{};
    issue.severity = severity;
    issue.message = std::move(message);
    issue.navKind = navKind;
    issue.navIndex = navIndex;
    issue.navSector = navSector;
    issue.navSurface = navSurface;
    return issue;
}

void appendSectorTopologyChecks(
    std::vector<CheckIssue>& out,
    const Level& level,
    const Sector& sector,
    const std::size_t sectorIndex,
    const CheckRuleSelection& selection,
    bool& canCheckSurfaces)
{
    const int sectorIndex32 = static_cast<int>(sectorIndex);

    // Step 1: validate sector-level metadata before any surface-level geometry checks.
    if (selection.isEnabled(CheckRule::sectorColormap)
        && level.kind != ProjectType::IJIM
        && sed::text::trimView(sector.colormap).empty())
    {
        out.push_back(makeIssue(
            CheckSeverity::error,
            QString("Sector %1: no colormap set").arg(sectorIndex32),
            OutlinerKind::sector,
            sectorIndex32));
    }

    // Step 2: reject undersized sectors early, matching the legacy checker's short-circuit behavior.
    if (sector.surfaces.size() < 4)
    {
        if (selection.isEnabled(CheckRule::sectorMinimumSurfaces))
        {
            out.push_back(makeIssue(
                CheckSeverity::error,
                QString("Sector %1: sector has less than 4 surfaces").arg(sectorIndex32),
                OutlinerKind::sector,
                sectorIndex32));
        }
        canCheckSurfaces = false;
        return;
    }

    if (sector.vertices.size() < 4)
    {
        if (selection.isEnabled(CheckRule::sectorMinimumVertices))
        {
            out.push_back(makeIssue(
                CheckSeverity::error,
                QString("Sector %1: sector has less than 4 vertices").arg(sectorIndex32),
                OutlinerKind::sector,
                sectorIndex32));
        }
        canCheckSurfaces = false;
        return;
    }

    // Step 3: non-3DO sectors should be convex so portal and containment logic behaves predictably.
    if (selection.isEnabled(CheckRule::sectorConvexity)
        && (sector.flags & SECF_3DO) == 0
        && !(selection.geometryPrecision == GeometryCheckPrecision::Precise
            ? sed::level::sectorIsConvex(sector)
            : sed::level::sectorIsConvexForDiagnostics(sector)))
    {
        out.push_back(makeIssue(
            CheckSeverity::error,
            QString("Sector %1: sector is not convex").arg(sectorIndex32),
            OutlinerKind::sector,
            sectorIndex32));
    }
}

void appendSurfaceFlagAndCountChecks(
    std::vector<CheckIssue>& out,
    const Level& level,
    const Sector& sector,
    const Surface& surface,
    const std::size_t sectorIndex,
    const std::size_t surfaceIndex,
    const int maxSurfaceVertices,
    const CheckRuleSelection& selection)
{
    const int sectorIndex32 = static_cast<int>(sectorIndex);
    const int surfaceIndex32 = static_cast<int>(surfaceIndex);

    // Step 1: catch common flag/mode mismatches before deeper polygon validation.
    if (selection.isEnabled(CheckRule::solidSurfaceFaceFlags)
        && !surface.adjoin
        && (surface.faceFlags & FF_Transluent) != 0u)
    {
        out.push_back(makeIssue(
            CheckSeverity::warning,
            QString("Sector %1 / Surface %2: incorrect face flags on solid surface")
                .arg(sectorIndex32)
                .arg(surfaceIndex32),
            OutlinerKind::surface,
            surfaceIndex32,
            sectorIndex32));
    }

    if (selection.isEnabled(CheckRule::floorCollision)
        && (surface.surfaceFlags & SF_Floor) != 0
        && (surface.surfaceFlags & SF_Collision) == 0)
    {
        out.push_back(makeIssue(
            CheckSeverity::warning,
            QString("Sector %1 / Surface %2: missing collision flag on floor surface")
                .arg(sectorIndex32)
                .arg(surfaceIndex32),
            OutlinerKind::surface,
            surfaceIndex32,
            sectorIndex32));
    }

    if (selection.isEnabled(CheckRule::solidSurfaceGeometryMode)
        && !surface.adjoin
        && surface.geo != static_cast<std::int32_t>(TGeoMode::Texture))
    {
        out.push_back(makeIssue(
            CheckSeverity::warning,
            QString("Sector %1 / Surface %2: incorrect geometry mode on solid surface")
                .arg(sectorIndex32)
                .arg(surfaceIndex32),
            OutlinerKind::surface,
            surfaceIndex32,
            sectorIndex32));
    }

    // Step 2: validate per-project surface size limits and paired vertex/texcoord arrays.
    if (selection.isEnabled(CheckRule::surfaceVertexLimit)
        && static_cast<int>(surface.vertices.size()) > maxSurfaceVertices)
    {
        out.push_back(makeIssue(
            CheckSeverity::warning,
            QString("Sector %1 / Surface %2: more than %3 vertices in a surface")
                .arg(sectorIndex32)
                .arg(surfaceIndex32)
                .arg(maxSurfaceVertices),
            OutlinerKind::surface,
            surfaceIndex32,
            sectorIndex32));
    }

    if (selection.isEnabled(CheckRule::surfaceMinimumVertices)
        && surface.vertices.size() < 3)
    {
        out.push_back(makeIssue(
            CheckSeverity::error,
            QString("Sector %1 / Surface %2: less than 3 vertices in a surface")
                .arg(sectorIndex32)
                .arg(surfaceIndex32),
            OutlinerKind::surface,
            surfaceIndex32,
            sectorIndex32));
    }

    if (selection.isEnabled(CheckRule::surfaceTexVertexCount)
        && !surface.texVertices.empty()
        && surface.texVertices.size() != surface.vertices.size())
    {
        out.push_back(makeIssue(
            CheckSeverity::warning,
            QString("Sector %1 / Surface %2: texVertices count (%3) != vertices count (%4)")
                .arg(sectorIndex32)
                .arg(surfaceIndex32)
                .arg(static_cast<int>(surface.texVertices.size()))
                .arg(static_cast<int>(surface.vertices.size())),
            OutlinerKind::surface,
            surfaceIndex32,
            sectorIndex32));
    }

    if (selection.isEnabled(CheckRule::ijimTrackVertexLimit)
        && level.kind == ProjectType::IJIM
        && (surface.surfaceFlags & SF_IJIM_Track) != 0
        && static_cast<int>(surface.vertices.size()) > kIjimMaxTrackSurfaceVertices)
    {
        out.push_back(makeIssue(
            CheckSeverity::warning,
            QString("Sector %1 / Surface %2: more than %3 vertices in a track surface")
                .arg(sectorIndex32)
                .arg(surfaceIndex32)
                .arg(kIjimMaxTrackSurfaceVertices),
            OutlinerKind::surface,
            surfaceIndex32,
            sectorIndex32));
    }
}

void appendSurfaceGeometryChecks(
    std::vector<CheckIssue>& out,
    const Sector& sector,
    const Surface& surface,
    const std::size_t sectorIndex,
    const std::size_t surfaceIndex,
    const CheckRuleSelection& selection)
{
    const int sectorIndex32 = static_cast<int>(sectorIndex);
    const int surfaceIndex32 = static_cast<int>(surfaceIndex);

    // Step 1: invalid indices make all following polygon math unsafe; report once and stop.
    for (const std::int32_t vi : surface.vertices)
    {
        if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
        {
            if (selection.isEnabled(CheckRule::surfaceVertexReferences))
            {
                out.push_back(makeIssue(
                    CheckSeverity::error,
                    QString("Sector %1 / Surface %2: invalid vertex index %3 (sector has %4 vertices)")
                        .arg(sectorIndex32)
                        .arg(surfaceIndex32)
                        .arg(vi)
                        .arg(static_cast<int>(sector.vertices.size())),
                    OutlinerKind::surface,
                    surfaceIndex32,
                    sectorIndex32));
            }
            return;
        }
    }

    // Step 2: with valid vertex indices, check stored normal and polygon shape quality.
    if (selection.isEnabled(CheckRule::surfaceNormal)
        && std::abs(surface.normal.Length() - 1.0) >= kLegacyNormalLengthTolerance)
    {
        out.push_back(makeIssue(
            CheckSeverity::warning,
            QString("Sector %1 / Surface %2: incorrect normal - surface may be invalid")
                .arg(sectorIndex32)
                .arg(surfaceIndex32),
            OutlinerKind::surface,
            surfaceIndex32,
            sectorIndex32));
    }

    if (selection.isEnabled(CheckRule::surfacePlanarity)
        && !(selection.geometryPrecision == GeometryCheckPrecision::Precise
            ? sed::level::surfaceIsPlanar(sector, surface)
            : sed::level::surfaceIsPlanarForDiagnostics(sector, surface)))
    {
        out.push_back(makeIssue(
            CheckSeverity::info,
            QString("Sector %1 / Surface %2: face is not planar")
                .arg(sectorIndex32)
                .arg(surfaceIndex32),
            OutlinerKind::surface,
            surfaceIndex32,
            sectorIndex32));
    }

    if (selection.isEnabled(CheckRule::surfaceConvexity)
        && !(selection.geometryPrecision == GeometryCheckPrecision::Precise
            ? sed::level::surfaceIsConvex(sector, surface)
            : sed::level::surfaceIsConvexForDiagnostics(sector, surface)))
    {
        out.push_back(makeIssue(
            CheckSeverity::info,
            QString("Sector %1 / Surface %2: polygon is not convex")
                .arg(sectorIndex32)
                .arg(surfaceIndex32),
            OutlinerKind::surface,
            surfaceIndex32,
            sectorIndex32));
    }
}

void appendAdjoinChecks(
    std::vector<CheckIssue>& out,
    const Level& level,
    const Surface& surface,
    const std::size_t sectorIndex,
    const std::size_t surfaceIndex,
    const CheckRuleSelection& selection)
{
    if (!surface.adjoin)
    {
        return;
    }

    const int sectorIndex32 = static_cast<int>(sectorIndex);
    const int surfaceIndex32 = static_cast<int>(surfaceIndex);
    const SurfaceRef ref = *surface.adjoin;
    const bool refValid = ref.sector >= 0 && ref.surface >= 0
        && static_cast<std::size_t>(ref.sector) < level.sectors.size()
        && static_cast<std::size_t>(ref.surface) < level.sectors[static_cast<std::size_t>(ref.sector)].surfaces.size();

    // Step 1: the mirror surface must exist before checking backlink and visibility flags.
    if (!refValid)
    {
        if (selection.isEnabled(CheckRule::adjoinReference))
        {
            out.push_back(makeIssue(
                CheckSeverity::error,
                QString("Sector %1 / Surface %2: invalid adjoin reference (%3:%4)")
                    .arg(sectorIndex32)
                    .arg(surfaceIndex32)
                    .arg(ref.sector)
                    .arg(ref.surface),
                OutlinerKind::surface,
                surfaceIndex32,
                sectorIndex32));
        }
        return;
    }

    // Step 2: valid adjoins should point back, geometrically match, and be visible when they render translucent geometry.
    const Sector& otherSector = level.sectors[static_cast<std::size_t>(ref.sector)];
    const Surface& other = otherSector.surfaces[static_cast<std::size_t>(ref.surface)];
    const bool backLinkOk = other.adjoin
        && other.adjoin->sector == static_cast<std::int32_t>(sectorIndex)
        && other.adjoin->surface == static_cast<std::int32_t>(surfaceIndex);
    if (selection.isEnabled(CheckRule::adjoinBackLink) && !backLinkOk)
    {
        out.push_back(makeIssue(
            CheckSeverity::error,
            QString("Sector %1 / Surface %2: adjoin back-link missing/mismatched on mirror surface (%3:%4)")
                .arg(sectorIndex32)
                .arg(surfaceIndex32)
                .arg(ref.sector)
                .arg(ref.surface),
            OutlinerKind::surface,
            surfaceIndex32,
            sectorIndex32));
    }

    const Sector& sector = level.sectors[sectorIndex];
    if (selection.isEnabled(CheckRule::adjoinOverlap)
        && !(selection.geometryPrecision == GeometryCheckPrecision::Precise
            ? sed::level::surfaceDoOverlap(sector, surface, otherSector, other)
            : sed::level::surfaceBoundariesMatchForDiagnostics(sector, surface, otherSector, other)))
    {
        out.push_back(makeIssue(
            CheckSeverity::warning,
            QString("Sector %1 / Surface %2: adjoined surfaces do not overlap")
                .arg(sectorIndex32)
                .arg(surfaceIndex32),
            OutlinerKind::surface,
            surfaceIndex32,
            sectorIndex32));
    }

    if (selection.isEnabled(CheckRule::adjoinVisibility)
        && (surface.adjoinFlags & SAF_Visible) == 0
        && (surface.geo == 0 || (surface.faceFlags & FF_Transluent) != 0u))
    {
        out.push_back(makeIssue(
            CheckSeverity::warning,
            QString("Sector %1 / Surface %2: translucent adjoin not visible (missing Visible adjoin flag)")
                .arg(sectorIndex32)
                .arg(surfaceIndex32),
            OutlinerKind::surface,
            surfaceIndex32,
            sectorIndex32));
    }
}

void appendSectorChecks(
    std::vector<CheckIssue>& out,
    const Level& level,
    const CheckRuleSelection& selection)
{
    const int maxSurfaceVertices = (level.kind == ProjectType::IJIM) ? 64 : 24;

    // Step 1: walk sectors in file order so diagnostics remain stable in the UI.
    for (std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex)
    {
        const Sector& sector = level.sectors[sectorIndex];

        bool canCheckSurfaces = true;
        appendSectorTopologyChecks(out, level, sector, sectorIndex, selection, canCheckSurfaces);
        if (!canCheckSurfaces)
        {
            continue;
        }

        // Step 2: each surface goes through flag/count, geometry, and adjoin checks in that order.
        for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
        {
            const Surface& surface = sector.surfaces[surfaceIndex];
            appendSurfaceFlagAndCountChecks(
                out,
                level,
                sector,
                surface,
                sectorIndex,
                surfaceIndex,
                maxSurfaceVertices,
                selection);

            appendSurfaceGeometryChecks(out, sector, surface, sectorIndex, surfaceIndex, selection);
            appendAdjoinChecks(out, level, surface, sectorIndex, surfaceIndex, selection);
        }
    }
}

bool sectorCrossesEveryPlane(const Sector& boundary, const Sector& candidate) noexcept
{
    if (boundary.surfaces.empty() || boundary.vertices.empty() || candidate.vertices.empty())
    {
        return false;
    }

    for (const Surface& surface : boundary.surfaces)
    {
        if (surface.vertices.empty())
        {
            return false;
        }

        const std::int32_t firstVertexIndex = surface.vertices.front();
        if (firstVertexIndex < 0 || static_cast<std::size_t>(firstVertexIndex) >= boundary.vertices.size())
        {
            return false;
        }

        const Vertex& firstVertex = boundary.vertices[static_cast<std::size_t>(firstVertexIndex)];
        const bool crossesPlane = std::any_of(
            candidate.vertices.begin(),
            candidate.vertices.end(),
            [&](const Vertex& vertex)
            {
                return surface.normal.dot(
                           vertex.x - firstVertex.x,
                           vertex.y - firstVertex.y,
                           vertex.z - firstVertex.z)
                    > kLegacySectorOverlapPlaneTolerance;
            });
        if (!crossesPlane)
        {
            return false;
        }
    }

    return true;
}

bool sectorsOverlap(const Sector& first, const Sector& second) noexcept
{
    return sectorCrossesEveryPlane(first, second)
        && sectorCrossesEveryPlane(second, first);
}

bool sectorsAreDirectlyAdjoined(const Sector& sector, const std::size_t otherSectorIndex) noexcept
{
    return std::any_of(
        sector.surfaces.begin(),
        sector.surfaces.end(),
        [&](const Surface& surface)
        {
            return surface.adjoin
                && surface.adjoin->sector == static_cast<std::int32_t>(otherSectorIndex);
        });
}

void appendSectorOverlapChecks(std::vector<CheckIssue>& out, const Level& level)
{
    std::vector<TBox> bounds{};
    bounds.reserve(level.sectors.size());
    for (const Sector& sector : level.sectors)
    {
        bounds.push_back(sed::level::sectorCalcBBox(sector));
    }

    // Match the legacy checker: compare in file order, skip directly adjoined
    // sectors, and attach one informational row to each sector in the pair.
    for (std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex)
    {
        const Sector& sector = level.sectors[sectorIndex];
        for (std::size_t otherIndex = sectorIndex; otherIndex-- > 0;)
        {
            if (sectorsAreDirectlyAdjoined(sector, otherIndex)
                || !sed::level::doBoxesIntersect(bounds[sectorIndex], bounds[otherIndex])
                || !sectorsOverlap(sector, level.sectors[otherIndex]))
            {
                continue;
            }

            const QString message = QString("Sectors %1 and %2 overlap")
                                        .arg(static_cast<int>(sectorIndex))
                                        .arg(static_cast<int>(otherIndex));
            out.push_back(makeIssue(
                CheckSeverity::info,
                message,
                OutlinerKind::sector,
                static_cast<int>(sectorIndex)));
            out.push_back(makeIssue(
                CheckSeverity::info,
                message,
                OutlinerKind::sector,
                static_cast<int>(otherIndex)));
        }
    }
}

void appendThingChecks(
    std::vector<CheckIssue>& out,
    const Level& level,
    const CheckRuleSelection& selection,
    const std::vector<std::int32_t>* thingIndices = nullptr,
    const bool containmentAlreadyResolved = false)
{
    // Step 1: validate placement coordinates and sector references before containment checks.
    const std::size_t iterationCount = thingIndices
        ? thingIndices->size()
        : level.things.size();
    for (std::size_t iteration = 0; iteration < iterationCount; ++iteration)
    {
        const std::int32_t requestedIndex = thingIndices
            ? (*thingIndices)[iteration]
            : static_cast<std::int32_t>(iteration);
        if (requestedIndex < 0
            || static_cast<std::size_t>(requestedIndex) >= level.things.size())
        {
            continue;
        }
        const std::size_t thingIndex = static_cast<std::size_t>(requestedIndex);
        const Thing& thing = level.things[thingIndex];
        const int thingIndex32 = static_cast<int>(thingIndex);

        if (!std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z))
        {
            if (selection.isEnabled(CheckRule::thingFinitePosition))
            {
                out.push_back(makeIssue(
                    CheckSeverity::error,
                    QString("Thing %1: non-finite position (%2, %3, %4)")
                        .arg(thingIndex32)
                        .arg(ui::formatUiNumber(thing.x))
                        .arg(ui::formatUiNumber(thing.y))
                        .arg(ui::formatUiNumber(thing.z)),
                    OutlinerKind::thing,
                    thingIndex32));
            }
            continue;
        }

        const std::int32_t sectorIndex = thing.sector;
        if (sectorIndex < 0)
        {
            if (selection.isEnabled(CheckRule::thingSectorAssignment))
            {
                out.push_back(makeIssue(
                    CheckSeverity::error,
                    QString("Thing %1: not in any sector (sector = %2)")
                        .arg(thingIndex32)
                        .arg(sectorIndex),
                    OutlinerKind::thing,
                    thingIndex32));
            }
            continue;
        }

        if (static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
        {
            if (selection.isEnabled(CheckRule::thingSectorAssignment))
            {
                out.push_back(makeIssue(
                    CheckSeverity::error,
                    QString("Thing %1: invalid sector index %2 (level has %3 sectors)")
                        .arg(thingIndex32)
                        .arg(sectorIndex)
                        .arg(static_cast<int>(level.sectors.size())),
                    OutlinerKind::thing,
                    thingIndex32));
            }
            continue;
        }

        // Step 2: valid sector references can be checked for containment and template hygiene.
        const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        if (selection.isEnabled(CheckRule::thingSectorContainment)
            && !containmentAlreadyResolved
            && !sed::level::sectorContainsPoint(sector, thing.x, thing.y, thing.z))
        {
            out.push_back(makeIssue(
                CheckSeverity::error,
                QString("Thing %1: sector %2 does not contain its position")
                    .arg(thingIndex32)
                    .arg(sectorIndex),
                OutlinerKind::thing,
                thingIndex32));
        }

        const std::string_view tplName = !thing.templateName.empty()
            ? std::string_view(thing.templateName)
            : std::string_view(thing.name);
        if (selection.isEnabled(CheckRule::thingBaseTemplate)
            && !tplName.empty()
            && tplName.front() == '_')
        {
            out.push_back(makeIssue(
                CheckSeverity::warning,
                QString("Thing %1: placed thing uses base template %2")
                    .arg(thingIndex32)
                    .arg(QString::fromStdString(std::string(tplName))),
                OutlinerKind::thing,
                thingIndex32));
        }
    }
}

void appendCogChecks(
    std::vector<CheckIssue>& out,
    const Level& level,
    const CheckRuleSelection& selection)
{
    if (!selection.isEnabled(CheckRule::cogParameterExtension))
    {
        return;
    }

    // Step 1: validate that typed COG resource parameters use the expected file extension.
    for (std::size_t cogIndex = 0; cogIndex < level.cogs.size(); ++cogIndex)
    {
        const Cog& cog = level.cogs[cogIndex];
        for (const CogValue& value : cog.values)
        {
            const std::string_view asText = sed::text::trimView(value.stringValue.empty() ? value.asString() : value.stringValue);
            std::string_view expectedExt{};
            switch (value.cogType)
            {
                case CogType::ai:       expectedExt = ".ai"; break;
                case CogType::key:      expectedExt = ".key"; break;
                case CogType::mat:      expectedExt = ".mat"; break;
                case CogType::wav:      expectedExt = ".wav"; break;
                case CogType::model3do: expectedExt = ".3do"; break;
                default: break;
            }

            if (expectedExt.empty() || sed::text::iendsWithAscii(asText, expectedExt))
            {
                continue;
            }

            out.push_back(makeIssue(
                CheckSeverity::warning,
                QString("COG %1: incorrect file name for parameter %2")
                    .arg(static_cast<int>(cogIndex))
                    .arg(QString::fromStdString(value.name)),
                OutlinerKind::cog,
                static_cast<int>(cogIndex)));
        }
    }
}

bool resourceExistsWithExt(
    const assets::ResourceLocator& resources,
    const std::string_view name,
    const std::string_view ext)
{
    const std::string_view trimmed = sed::text::trimView(name);
    if (trimmed.empty())
    {
        return true;
    }

    if (resources.findOnDisk(trimmed) || resources.findInGobs(trimmed))
    {
        return true;
    }

    if (sed::text::iendsWithAscii(trimmed, ext))
    {
        return false;
    }

    std::string withExt(trimmed);
    withExt += ext;
    return resources.findOnDisk(withExt) || resources.findInGobs(withExt);
}

void appendMaterialResourceChecks(
    std::vector<CheckIssue>& out,
    const Level& level,
    const assets::ResourceLocator& resources)
{
    std::unordered_map<std::string, SurfaceRef> firstMaterialUser{};

    // Step 1: remember the first surface that uses each material so each missing MAT is reported once.
    for (std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex)
    {
        const Sector& sector = level.sectors[sectorIndex];
        for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
        {
            const Surface& surface = sector.surfaces[surfaceIndex];
            const std::string_view trimmed = sed::text::trimView(surface.material);
            if (trimmed.empty())
            {
                continue;
            }

            const std::string key(trimmed);
            if (firstMaterialUser.contains(key))
            {
                continue;
            }

            firstMaterialUser.emplace(
                key,
                SurfaceRef{ static_cast<std::int32_t>(sectorIndex), static_cast<std::int32_t>(surfaceIndex) });
        }
    }

    // Step 2: check the mounted project roots/GOBs for each referenced material.
    for (const auto& [mat, ref] : firstMaterialUser)
    {
        if (!resourceExistsWithExt(resources, mat, ".mat"))
        {
            out.push_back(makeIssue(
                CheckSeverity::error,
                QString("Missing material: %1").arg(QString::fromStdString(mat)),
                OutlinerKind::surface,
                ref.surface,
                ref.sector));
        }
    }
}

void appendColormapResourceChecks(
    std::vector<CheckIssue>& out,
    const Level& level,
    const assets::ResourceLocator& resources,
    const CheckRuleSelection& selection)
{
    if (level.kind == ProjectType::IJIM)
    {
        return;
    }

    struct ColormapUse
    {
        std::string name{};
        std::int32_t sectorIndex{};
    };

    std::unordered_map<std::string, ColormapUse> firstColormapUser{};
    std::unordered_set<std::string> usedColormaps{};

    // Step 1: the level-wide master CMP participates in both availability and
    // project palette-count checks, but a blank master is valid legacy data.
    const std::string_view masterCmp = sed::text::trimView(level.masterCMP);
    if (!masterCmp.empty())
    {
        usedColormaps.emplace(sed::text::upperAscii(masterCmp));
        if (selection.isEnabled(CheckRule::masterCmpResource)
            && !resourceExistsWithExt(resources, masterCmp, ".cmp"))
        {
            out.push_back(makeIssue(
                CheckSeverity::error,
                QString("Level master CMP not found: %1")
                    .arg(QString::fromStdString(std::string(masterCmp)))));
        }
    }

    // Step 2: JK/MOTS levels can share colormaps, so report each missing CMP only once.
    for (std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex)
    {
        const Sector& sector = level.sectors[sectorIndex];
        const std::string_view trimmed = sed::text::trimView(sector.colormap);
        if (trimmed.empty())
        {
            continue;
        }

        const std::string key = sed::text::upperAscii(trimmed);
        usedColormaps.emplace(key);
        if (firstColormapUser.contains(key))
        {
            continue;
        }
        firstColormapUser.emplace(
            key,
            ColormapUse{ std::string(trimmed), static_cast<std::int32_t>(sectorIndex) });
    }

    // Step 3: validate every distinct colormap reference against the active resource locator.
    for (const auto& [_, use] : firstColormapUser)
    {
        if (selection.isEnabled(CheckRule::sectorCmpResource)
            && !resourceExistsWithExt(resources, use.name, ".cmp"))
        {
            out.push_back(makeIssue(
                CheckSeverity::error,
                QString("Missing colormap: %1").arg(QString::fromStdString(use.name)),
                OutlinerKind::sector,
                use.sectorIndex));
        }
    }

    // Step 4: retain the original engine limits for distinct authored CMP references.
    if (!selection.isEnabled(CheckRule::cmpCountLimit))
    {
        return;
    }

    if (level.kind == ProjectType::MOTS && usedColormaps.size() > 1)
    {
        out.push_back(makeIssue(
            CheckSeverity::warning,
            "More than 1 CMP used in the level"));
    }
    else if (level.kind == ProjectType::JKDF2 && usedColormaps.size() > 3)
    {
        out.push_back(makeIssue(
            CheckSeverity::warning,
            "More than 3 CMP used in the level"));
    }
}

void appendSectorSoundResourceChecks(
    std::vector<CheckIssue>& out,
    const Level& level,
    const assets::ResourceLocator& resources,
    const CheckRuleSelection& selection)
{
    if (!selection.isEnabled(CheckRule::sectorSoundResource))
    {
        return;
    }

    for (std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex)
    {
        const std::string_view sound = sed::text::trimView(level.sectors[sectorIndex].sound);
        if (sound.empty() || resourceExistsWithExt(resources, sound, ".wav"))
        {
            continue;
        }

        out.push_back(makeIssue(
            CheckSeverity::error,
            QString("Sector %1: sound not found: %2")
                .arg(static_cast<int>(sectorIndex))
                .arg(QString::fromStdString(std::string(sound))),
            OutlinerKind::sector,
            static_cast<int>(sectorIndex)));
    }
}

void appendCogResourceChecks(
    std::vector<CheckIssue>& out,
    const Level& level,
    const assets::ResourceLocator& resources,
    const CheckRuleSelection& selection)
{
    if (!selection.isEnabled(CheckRule::cogScriptResource))
    {
        return;
    }

    for (std::size_t cogIndex = 0; cogIndex < level.cogs.size(); ++cogIndex)
    {
        const std::string_view script = sed::text::trimView(level.cogs[cogIndex].name);
        if (!script.empty() && resourceExistsWithExt(resources, script, ".cog"))
        {
            continue;
        }

        const QString displayName = script.empty()
            ? QStringLiteral("<empty>")
            : QString::fromStdString(std::string(script));
        out.push_back(makeIssue(
            CheckSeverity::error,
            QString("COG %1: script not found: %2")
                .arg(static_cast<int>(cogIndex))
                .arg(displayName),
            OutlinerKind::cog,
            static_cast<int>(cogIndex)));
    }
}

void appendThingResourceChecks(
    std::vector<CheckIssue>& out,
    const Level& level,
    const assets::ResourceLocator& resources,
    const assets::Templates& templates,
    const CheckRuleSelection& selection,
    const std::vector<std::int32_t>* thingIndices = nullptr)
{
    const assets::Templates* templatesDb = templates.size() > 0 ? &templates : nullptr;
    // A level commonly places the same 3DO many times. Keep one parsed model
    // per authored filename for this pass so topology diagnostics stay cheap
    // without retaining a cross-pass cache whose resource revision could go
    // stale.
    std::unordered_map<std::string, std::optional<assets::Model3do>> modelCache{};

    const auto parseModelForCheck =
        [&](const std::string& modelName) -> const assets::Model3do*
    {
        const auto existing = modelCache.find(modelName);
        if (existing != modelCache.end())
        {
            return existing->second ? &*existing->second : nullptr;
        }

        std::optional<assets::Model3do> parsed{};
        if (const auto bytes = resources.tryReadBinaryWithExt(modelName, ".3do"))
        {
            try
            {
                parsed = assets::loadFrom3DO(
                    std::span<const std::byte>(*bytes));
            }
            catch (const std::exception&)
            {
                // The caller reports an uncheckable ARCHLIGHTING/model pair
                // while preserving the ordinary model-availability warning.
            }
        }

        const auto [inserted, unused] = modelCache.emplace(
            modelName, std::move(parsed));
        (void)unused;
        return inserted->second ? &*inserted->second : nullptr;
    };

    // Step 1: each placed thing must resolve to either the mounted template DB or local level templates.
    const std::size_t iterationCount = thingIndices
        ? thingIndices->size()
        : level.things.size();
    for (std::size_t iteration = 0; iteration < iterationCount; ++iteration)
    {
        const std::int32_t requestedIndex = thingIndices
            ? (*thingIndices)[iteration]
            : static_cast<std::int32_t>(iteration);
        if (requestedIndex < 0
            || static_cast<std::size_t>(requestedIndex) >= level.things.size())
        {
            continue;
        }
        const std::size_t i = static_cast<std::size_t>(requestedIndex);
        const Thing& thing = level.things[i];
        const int thingIndex32 = static_cast<int>(i);
        const std::string_view tplName = !thing.templateName.empty() ? std::string_view(thing.templateName)
                                                                     : std::string_view(thing.name);
        if (tplName.empty())
        {
            if (selection.isEnabled(CheckRule::thingTemplateResource))
            {
                out.push_back(makeIssue(
                    CheckSeverity::error,
                    QString("Thing %1: missing template name").arg(thingIndex32),
                    OutlinerKind::thing,
                    thingIndex32));
            }
            continue;
        }

        const bool haveTpl = templates.find(tplName) != nullptr
            || std::any_of(level.templates.begin(), level.templates.end(), [&](const ThingTemplate& t)
                {
                    return t.name == tplName;
                });

        if (selection.isEnabled(CheckRule::thingTemplateResource) && !haveTpl)
        {
            out.push_back(makeIssue(
                CheckSeverity::error,
                QString("Thing %1: template not found: %2")
                    .arg(thingIndex32)
                    .arg(QString::fromStdString(std::string(tplName))),
                OutlinerKind::thing,
                thingIndex32));
        }

        // Step 2: resolved sprite fields should point at an existing SPR resource when present.
        if (selection.isEnabled(CheckRule::thingSpriteResource))
        {
            if (const TplValue* sprValue = sed::level::thingFindValue(level, templatesDb, thing, "sprite"))
            {
                const std::string sprName = sprValue->stringValue.empty() ? sprValue->asString() : sprValue->stringValue;
                if (!sprName.empty() && !resourceExistsWithExt(resources, sprName, ".spr"))
                {
                    out.push_back(makeIssue(
                        CheckSeverity::warning,
                        QString("Thing %1: missing sprite: %2")
                            .arg(thingIndex32)
                            .arg(QString::fromStdString(sprName)),
                        OutlinerKind::thing,
                        thingIndex32));
                }
            }
        }

        // Step 3: models may be named model3d or model3do depending on source data.
        if (selection.isEnabled(CheckRule::thingModelResource))
        {
            const TplValue* modelValue = sed::level::thingFindValue(level, templatesDb, thing, "model3d");
            if (!modelValue)
            {
                modelValue = sed::level::thingFindValue(level, templatesDb, thing, "model3do");
            }
            if (modelValue)
            {
                const std::string modelName = modelValue->stringValue.empty() ? modelValue->asString() : modelValue->stringValue;
                if (!modelName.empty() && !resourceExistsWithExt(resources, modelName, ".3do"))
                {
                    out.push_back(makeIssue(
                        CheckSeverity::warning,
                        QString("Thing %1: missing 3DO model: %2")
                            .arg(thingIndex32)
                            .arg(QString::fromStdString(modelName)),
                        OutlinerKind::thing,
                        thingIndex32));
                }
                else if (!modelName.empty() && level.kind == ProjectType::MOTS
                    && thing.archLightingIndex != -1)
                {
                    if (thing.archLightingIndex < -1
                        || static_cast<std::size_t>(thing.archLightingIndex)
                            >= level.motsArchLighting.size())
                    {
                        out.push_back(makeIssue(
                            CheckSeverity::error,
                            QString("Thing %1: ARCHLIGHTING index %2 is out of range")
                                .arg(thingIndex32)
                                .arg(thing.archLightingIndex),
                            OutlinerKind::thing,
                            thingIndex32));
                    }
                    else if (thing.archLightingIndex >= 0)
                    {
                        const assets::Model3do* model =
                            parseModelForCheck(modelName);
                        if (!model)
                        {
                            out.push_back(makeIssue(
                                CheckSeverity::error,
                                QString("Thing %1: ARCHLIGHTING topology cannot be checked because the 3DO model is malformed: %2")
                                    .arg(thingIndex32)
                                    .arg(QString::fromStdString(modelName)),
                                OutlinerKind::thing,
                                thingIndex32));
                        }
                        else
                        {
                            const MotsArchLightObject& object =
                                level.motsArchLighting[static_cast<std::size_t>(
                                    thing.archLightingIndex)];
                            bool topologyMatches = object.meshes.size()
                                >= model->meshes.size();
                            if (topologyMatches)
                            {
                                for (std::size_t meshIndex = 0;
                                     meshIndex < model->meshes.size();
                                     ++meshIndex)
                                {
                                    if (object.meshes[meshIndex].vertices.size()
                                        < model->meshes[meshIndex].vertices.size())
                                    {
                                        topologyMatches = false;
                                        break;
                                    }
                                }
                            }
                            if (!topologyMatches)
                            {
                                out.push_back(makeIssue(
                                    CheckSeverity::error,
                                    QString("Thing %1: ARCHLIGHTING topology does not match 3DO model: %2")
                                        .arg(thingIndex32)
                                        .arg(QString::fromStdString(modelName)),
                                    OutlinerKind::thing,
                                    thingIndex32));
                            }
                        }
                    }
                }
            }
        }
    }
}

} // namespace

std::vector<CheckRuleDefinition> checkRuleDefinitions(
    const CheckRuleGroup group,
    const ProjectType project)
{
    std::vector<CheckRuleDefinition> definitions{};
    for (const CheckRuleRecord& record : kCheckRules)
    {
        if (record.definition.group == group
            && (record.projects & projectBit(project)) != 0u)
        {
            definitions.push_back(record.definition);
        }
    }
    return definitions;
}

bool checkRuleAppliesToProject(const CheckRule rule, const ProjectType project) noexcept
{
    const CheckRuleRecord* record = ruleRecord(rule);
    return record && (record->projects & projectBit(project)) != 0u;
}

CheckRuleSelection defaultCheckRuleSelection(const ProjectType project) noexcept
{
    CheckRuleSelection selection{};
    selection.enabled.fill(false);
    for (const CheckRuleRecord& record : kCheckRules)
    {
        if ((record.projects & projectBit(project)) != 0u)
        {
            selection.setEnabled(record.definition.rule, record.definition.enabledByDefault);
        }
    }
    return selection;
}

QString checkRuleSettingsKey(const ProjectType project, const CheckRule rule)
{
    QString projectKey{};
    switch (project)
    {
        case ProjectType::JKDF2: projectKey = QStringLiteral("jkdf2"); break;
        case ProjectType::MOTS: projectKey = QStringLiteral("mots"); break;
        case ProjectType::IJIM: projectKey = QStringLiteral("ijim"); break;
    }

    const CheckRuleRecord* record = ruleRecord(rule);
    if (projectKey.isEmpty() || !record)
    {
        return {};
    }
    const QString ruleKey = QString::fromLatin1(
        record->definition.key.data(),
        static_cast<int>(record->definition.key.size()));
    return QStringLiteral("checks/rules/%1/%2").arg(projectKey, ruleKey);
}

QString geometryCheckPrecisionSettingsKey(const ProjectType project)
{
    switch ( project )
    {
        case ProjectType::JKDF2: return QStringLiteral("checks/geometry_precision/jkdf2");
        case ProjectType::MOTS: return QStringLiteral("checks/geometry_precision/mots");
        case ProjectType::IJIM: return QStringLiteral("checks/geometry_precision/ijim");
    }
    return {};
}

CheckRuleSelection loadCheckRuleSelection(QSettings& settings, const ProjectType project)
{
    CheckRuleSelection selection = defaultCheckRuleSelection(project);
    const QString precisionKey = geometryCheckPrecisionSettingsKey(project);
    if ( !precisionKey.isEmpty() )
    {
        const QVariant stored = settings.value(precisionKey);
        // INI backends can return an integer as text. Accept only the two
        // canonical integer spellings/types, never coercing bools or fractions.
        const int type = stored.metaType().id();
        if ( type == QMetaType::Int || type == QMetaType::UInt
            || type == QMetaType::LongLong || type == QMetaType::ULongLong
            || type == QMetaType::QString )
        {
            if ( stored.toString() == QStringLiteral("1") )
            {
                selection.geometryPrecision = GeometryCheckPrecision::Precise;
            }
        }
    }
    for (const CheckRuleRecord& record : kCheckRules)
    {
        const CheckRule rule = record.definition.rule;
        if (!checkRuleAppliesToProject(rule, project))
        {
            continue;
        }

        const QString key = checkRuleSettingsKey(project, rule);
        bool fallback = record.definition.enabledByDefault;
        // Preserve the pre-dialog overlap preference as a one-way fallback
        // until a project-specific value is saved.
        if (rule == CheckRule::sectorOverlap
            && !settings.contains(key)
            && settings.contains(QStringLiteral("checks/check_sector_overlaps")))
        {
            fallback = settings.value(QStringLiteral("checks/check_sector_overlaps")).toBool();
        }
        selection.setEnabled(rule, settings.value(key, fallback).toBool());
    }
    return selection;
}

void saveCheckRuleSelection(
    QSettings& settings,
    const ProjectType project,
    const CheckRuleGroup group,
    const CheckRuleSelection& selection)
{
    const QString precisionKey = geometryCheckPrecisionSettingsKey(project);
    if ( group == CheckRuleGroup::consistency && !precisionKey.isEmpty() )
    {
        settings.setValue(precisionKey,
            selection.geometryPrecision == GeometryCheckPrecision::Precise ? 1 : 0);
    }
    for (const CheckRuleDefinition& definition : checkRuleDefinitions(group, project))
    {
        settings.setValue(
            checkRuleSettingsKey(project, definition.rule),
            selection.isEnabled(definition.rule));
    }
}

std::vector<CheckIssue> collectConsistencyCheckIssues(
    const Level& level,
    const bool checkSectorOverlaps)
{
    CheckRuleSelection selection{};
    selection.setEnabled(CheckRule::sectorOverlap, checkSectorOverlaps);
    return collectConsistencyCheckIssues(level, selection);
}

std::vector<CheckIssue> collectConsistencyCheckIssues(
    const Level& level,
    const CheckRuleSelection& selection)
{
    std::vector<CheckIssue> issues{};

    // Step 1: sector and surface structure issues, including adjoin consistency.
    appendSectorChecks(issues, level, selection);

    // Step 2: retain the legacy default-off expensive sector-pair overlap pass.
    if (selection.isEnabled(CheckRule::sectorOverlap))
    {
        appendSectorOverlapChecks(issues, level);
    }

    // Step 3: thing placement and template hygiene issues.
    appendThingChecks(issues, level, selection);

    // Step 4: COG resource-parameter naming issues.
    appendCogChecks(issues, level, selection);

    return issues;
}

std::vector<CheckIssue> collectThingConsistencyCheckIssues(
    const Level& level,
    const CheckRuleSelection& selection,
    const std::vector<std::int32_t>& thingIndices,
    const bool containmentAlreadyResolved)
{
    std::vector<CheckIssue> issues{};
    appendThingChecks(
        issues,
        level,
        selection,
        &thingIndices,
        containmentAlreadyResolved);
    return issues;
}

std::vector<CheckIssue> collectResourceCheckIssues(
    const Level& level,
    const assets::ResourceLocator& resources,
    const assets::Templates& templates)
{
    return collectResourceCheckIssues(
        level,
        resources,
        templates,
        defaultCheckRuleSelection(level.kind));
}

std::vector<CheckIssue> collectResourceCheckIssues(
    const Level& level,
    const assets::ResourceLocator& resources,
    const assets::Templates& templates,
    const CheckRuleSelection& selection)
{
    std::vector<CheckIssue> issues{};

    // Step 1: surface material resources.
    if (selection.isEnabled(CheckRule::materialResource))
    {
        appendMaterialResourceChecks(issues, level, resources);
    }

    // Step 2: sector colormap resources for projects that use CMP files.
    appendColormapResourceChecks(issues, level, resources, selection);

    // Step 3: sector sound resources.
    appendSectorSoundResourceChecks(issues, level, resources, selection);

    // Step 4: thing template, sprite, and model resources.
    appendThingResourceChecks(issues, level, resources, templates, selection);

    // Step 5: placed COG scripts.
    appendCogResourceChecks(issues, level, resources, selection);

    return issues;
}

std::vector<CheckIssue> collectThingResourceCheckIssues(
    const Level& level,
    const assets::ResourceLocator& resources,
    const assets::Templates& templates,
    const CheckRuleSelection& selection,
    const std::vector<std::int32_t>& thingIndices)
{
    std::vector<CheckIssue> issues{};
    appendThingResourceChecks(
        issues,
        level,
        resources,
        templates,
        selection,
        &thingIndices);
    return issues;
}

} // namespace sed::ui::checks
